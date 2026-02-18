//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#define	USED

#include <windows.h>
#include "cmdlib.h"
#define NO_THREAD_NAMES
#include "threads.h"
#include "pacifier.h"

// Linux optimizations
#ifdef __linux__
	#include <unistd.h>
	#include <sys/types.h>
	#include <stdlib.h>
	#include <string.h>
#endif

// OPTIMIZATION: Dynamic thread pool for Linux
#ifdef _WIN32
	#define	MAX_THREADS	16
#else
	// Linux: Auto-detect CPU cores or use environment variable
	#define MAX_THREADS 64  // Safe upper bound for modern systems
	static int g_nActualThreads = 0;
#endif


class CRunThreadsData
{
public:
	int m_iThread;
	void *m_pUserData;
	RunThreadsFn m_Fn;
};

CRunThreadsData g_RunThreadsData[MAX_THREADS];


int		dispatch;
int		workcount;
qboolean		pacifier;

qboolean	threaded;
bool g_bLowPriorityThreads = false;

HANDLE g_ThreadHandles[MAX_THREADS];



/*
=============
GetThreadWork

=============
*/
int	GetThreadWork (void)
{
	int	r;

	// OPTIMIZATION: Try lock-free path first on Linux
#ifdef __linux__
	// Fast path: check without lock (may see stale value but safe)
	if ( dispatch >= workcount )
		return -1;
#endif

	ThreadLock ();

	if (dispatch == workcount)
	{
		ThreadUnlock ();
		return -1;
	}

	UpdatePacifier( (float)dispatch / workcount );

	r = dispatch;
	dispatch++;
	ThreadUnlock ();

	return r;
}


ThreadWorkerFn workfunction;

void ThreadWorkerFunction( int iThread, void *pUserData )
{
	int		work;

	while (1)
	{
		work = GetThreadWork ();
		if (work == -1)
			break;
		 
		workfunction( iThread, work );
	}
}

void RunThreadsOnIndividual (int workcnt, qboolean showpacifier, ThreadWorkerFn func)
{
	if (numthreads == -1)
		ThreadSetDefault ();
	
	workfunction = func;
	RunThreadsOn (workcnt, showpacifier, ThreadWorkerFunction);
}


/*
===================================================================

WIN32

===================================================================
*/

int		numthreads = -1;
CRITICAL_SECTION		crit;
static int enter;


class CCritInit
{
public:
	CCritInit()
	{
		InitializeCriticalSection (&crit);
	}
} g_CritInit;



void SetLowPriority()
{
#ifdef _WIN32
	SetPriorityClass( GetCurrentProcess(), IDLE_PRIORITY_CLASS );
#else
	// Linux: use nice() for process-wide priority
	nice( 10 );
#endif
}


void ThreadSetDefault (void)
{
#ifdef _WIN32
	SYSTEM_INFO info;

	if (numthreads == -1)	// not set manually
	{
		GetSystemInfo (&info);
		numthreads = info.dwNumberOfProcessors;
		if (numthreads < 1 || numthreads > 32)
			numthreads = 1;
	}
#else
	// OPTIMIZATION: Linux - detect CPU cores from /proc/cpuinfo or sysconf
	if (numthreads == -1)	// not set manually
	{
		// Method 1: Try sysconf (POSIX standard)
		long nProcs = sysconf( _SC_NPROCESSORS_ONLN );
		if ( nProcs > 0 )
		{
			numthreads = (int)nProcs;
		}
		else
		{
			// Method 2: Fallback to /proc/cpuinfo parsing
			FILE *fp = fopen( "/proc/cpuinfo", "r" );
			if ( fp )
			{
				int nCores = 0;
				char line[256];
				while ( fgets( line, sizeof(line), fp ) )
				{
					if ( strncmp( line, "processor", 9 ) == 0 )
						nCores++;
				}
				fclose( fp );
				if ( nCores > 0 )
					numthreads = nCores;
				else
					numthreads = 1;
			}
			else
			{
				numthreads = 1;  // Fallback
			}
		}
		
		// Clamp to reasonable range
		if ( numthreads < 1 ) numthreads = 1;
		if ( numthreads > MAX_THREADS ) numthreads = MAX_THREADS;
		
		g_nActualThreads = numthreads;
	}
#endif

	Msg ("%i threads\n", numthreads);
}


void ThreadLock (void)
{
	if (!threaded)
		return;
	EnterCriticalSection (&crit);
	if (enter)
		Error ("Recursive ThreadLock\n");
	enter = 1;
}

void ThreadUnlock (void)
{
	if (!threaded)
		return;
	if (!enter)
		Error ("ThreadUnlock without lock\n");
	enter = 0;
	LeaveCriticalSection (&crit);
}


// This runs in the thread and dispatches a RunThreadsFn call.
DWORD WINAPI InternalRunThreadsFn( LPVOID pParameter )
{
	CRunThreadsData *pData = (CRunThreadsData*)pParameter;
	pData->m_Fn( pData->m_iThread, pData->m_pUserData );
	return 0;
}


void RunThreads_Start( RunThreadsFn fn, void *pUserData, ERunThreadsPriority ePriority )
{
	Assert( numthreads > 0 );
	threaded = true;

	if ( numthreads > MAX_TOOL_THREADS )
		numthreads = MAX_TOOL_THREADS;

	for ( int i=0; i < numthreads ;i++ )
	{
		g_RunThreadsData[i].m_iThread = i;
		g_RunThreadsData[i].m_pUserData = pUserData;
		g_RunThreadsData[i].m_Fn = fn;

		DWORD dwDummy;
		g_ThreadHandles[i] = CreateThread(
		   NULL,	// LPSECURITY_ATTRIBUTES lpsa,
		   0,		// DWORD cbStack,
		   InternalRunThreadsFn,	// LPTHREAD_START_ROUTINE lpStartAddr,
		   &g_RunThreadsData[i],	// LPVOID lpvThreadParm,
		   0,			// DWORD fdwCreate,
		   &dwDummy );

		if ( ePriority == k_eRunThreadsPriority_UseGlobalState )
		{
			if( g_bLowPriorityThreads )
				SetThreadPriority( g_ThreadHandles[i], THREAD_PRIORITY_LOWEST );
		}
		else if ( ePriority == k_eRunThreadsPriority_Idle )
		{
			SetThreadPriority( g_ThreadHandles[i], THREAD_PRIORITY_IDLE );
		}
	}
}


void RunThreads_End()
{
	WaitForMultipleObjects( numthreads, g_ThreadHandles, TRUE, INFINITE );
	for ( int i=0; i < numthreads; i++ )
		CloseHandle( g_ThreadHandles[i] );

	threaded = false;
}
	

/*
=============
RunThreadsOn
=============
*/
void RunThreadsOn( int workcnt, qboolean showpacifier, RunThreadsFn fn, void *pUserData )
{
	int		start, end;

	start = Plat_FloatTime();
	dispatch = 0;
	workcount = workcnt;
	StartPacifier("");
	pacifier = showpacifier;

#ifdef _PROFILE
	threaded = false;
	(*func)( 0 );
	return;
#endif

	
	RunThreads_Start( fn, pUserData );
	RunThreads_End();


	end = Plat_FloatTime();
	if (pacifier)
	{
		EndPacifier(false);
		printf (" (%i)\n", end-start);
	}
}



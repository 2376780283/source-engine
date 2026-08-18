//========= Copyright Valve Corporation, All rights reserved. ============//
//                       TOGL CODE LICENSE
//
//  Copyright 2011-2014 Valve Corporation
//  All Rights Reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//
// cglmbuffer.cpp
//
//===============================================================================

#include "togles/rendermechanism.h"
#include "tier0/icommandline.h"

// memdbgon -must- be the last include file in a .cpp file.
#include "tier0/memdbgon.h"

// 7LS TODO : took out cmdline here
bool g_bUsePseudoBufs = false; //( Plat_GetCommandLineA() ) ? ( strstr( Plat_GetCommandLineA(), "-gl_enable_pseudobufs" ) != NULL ) : false;

// -gl_multi_buffer_vbos: cycle a ring of plain map/unmap GL buffers on every
// DISCARD lock instead of re-mapping one VBO.  Each slot's data is 2+ frames
// old by the time it is rewritten, so the map never serializes against GPU
// work - the persistent ring's stall-free property on the proven plain path.
// Set once in the CGLMBuffer constructor (before any Lock can occur).
static bool g_bMultiBufferVBOs = false;
static bool g_bMultiBufferVBOsChecked = false;
#ifdef OSX
// Significant perf degradation on some OSX parts if static buffers not disabled
bool g_bDisableStaticBuffer = true;
#else
bool g_bDisableStaticBuffer = true; //( Plat_GetCommandLineA() ) ? ( strstr( Plat_GetCommandLineA(), "-gl_disable_static_buffer" ) != NULL ) : false;
#endif

// http://www.opengl.org/registry/specs/ARB/vertex_buffer_object.txt
// http://www.opengl.org/registry/specs/ARB/pixel_buffer_object.txt

// gl_bufmode: zero means we mark all vertex/index buffers static

// non zero means buffers are initially marked static..
// ->but can shift to dynamic upon first 'discard' (orphaning)

// #define REPORT_LOCK_TIME	0

ConVar gl_bufmode( "gl_bufmode", "1" );

#define GLM_BUFFER_PERF_ANALYSIS 0

#if GLM_BUFFER_PERF_ANALYSIS
struct CGLMBufferPerfStats
{
	CCycleCount m_LockTime;
	CCycleCount m_UnlockTime;
	CCycleCount m_PseudoCopyTime;
	uint64 m_nLocks;
	uint64 m_nUnlocks;
	uint64 m_nPseudoLocks;
	uint64 m_nPseudoUnlocks;
	uint64 m_nDiscards;
	uint64 m_nNoOverwrites;
	uint64 m_nVertexBytes;
	uint64 m_nIndexBytes;
	uint64 m_nOtherBytes;
	uint64 m_nPseudoCopyCalls;
	uint64 m_nPseudoCopyBytes;

	void Reset()
	{
		m_LockTime.Init();
		m_UnlockTime.Init();
		m_PseudoCopyTime.Init();
		m_nLocks = 0;
		m_nUnlocks = 0;
		m_nPseudoLocks = 0;
		m_nPseudoUnlocks = 0;
		m_nDiscards = 0;
		m_nNoOverwrites = 0;
		m_nVertexBytes = 0;
		m_nIndexBytes = 0;
		m_nOtherBytes = 0;
		m_nPseudoCopyCalls = 0;
		m_nPseudoCopyBytes = 0;
	}
};

static CGLMBufferPerfStats s_BufferPerfStats;

class CGLMBufferPerfTimer
{
public:
	explicit CGLMBufferPerfTimer( CCycleCount &total )
		: m_Total( total )
	{
		m_Timer.Start();
	}

	~CGLMBufferPerfTimer()
	{
		m_Timer.End();
		m_Total += m_Timer.GetDuration();
	}

private:
	CFastTimer m_Timer;
	CCycleCount &m_Total;
};

CON_COMMAND( gl_dump_buffer_stats, "Print GL buffer lock/upload time; pass 1 to reset after printing." )
{
	const double flLockMS = s_BufferPerfStats.m_LockTime.GetMillisecondsF();
	const double flUnlockMS = s_BufferPerfStats.m_UnlockTime.GetMillisecondsF();
	const double flCopyMS = s_BufferPerfStats.m_PseudoCopyTime.GetMillisecondsF();
	ConMsg( "GL buffers: Locks: %llu (%llu pseudo), %4.3fms (%4.6fms/call); Unlocks: %llu (%llu pseudo), %4.3fms (%4.6fms/call)\n",
		(unsigned long long)s_BufferPerfStats.m_nLocks,
		(unsigned long long)s_BufferPerfStats.m_nPseudoLocks,
		flLockMS,
		s_BufferPerfStats.m_nLocks ? flLockMS / s_BufferPerfStats.m_nLocks : 0.0,
		(unsigned long long)s_BufferPerfStats.m_nUnlocks,
		(unsigned long long)s_BufferPerfStats.m_nPseudoUnlocks,
		flUnlockMS,
		s_BufferPerfStats.m_nUnlocks ? flUnlockMS / s_BufferPerfStats.m_nUnlocks : 0.0 );
	ConMsg( "GL buffer traffic: Vertex: %llu bytes, Index: %llu bytes, Other: %llu bytes; Discards: %llu NoOverwrite: %llu; pseudo memcpy: %llu calls, %llu bytes, %4.3fms\n",
		(unsigned long long)s_BufferPerfStats.m_nVertexBytes,
		(unsigned long long)s_BufferPerfStats.m_nIndexBytes,
		(unsigned long long)s_BufferPerfStats.m_nOtherBytes,
		(unsigned long long)s_BufferPerfStats.m_nDiscards,
		(unsigned long long)s_BufferPerfStats.m_nNoOverwrites,
		(unsigned long long)s_BufferPerfStats.m_nPseudoCopyCalls,
		(unsigned long long)s_BufferPerfStats.m_nPseudoCopyBytes,
		flCopyMS );

	if ( args.ArgC() == 2 && args.Arg(1)[0] != '0' )
	{
		s_BufferPerfStats.Reset();
	}
}
#endif

char ALIGN16 CGLMBuffer::m_StaticBuffers[ GL_MAX_STATIC_BUFFERS ][ GL_STATIC_BUFFER_SIZE ] ALIGN16_POST;
bool CGLMBuffer::m_bStaticBufferUsed[ GL_MAX_STATIC_BUFFERS ];

extern bool g_bNullD3DDevice; 

//===========================================================================//

static uint gMaxPersistentOffset[kGLMNumBufferTypes] =
{
	0,
	0,
	0,
	0
};
CON_COMMAND( gl_persistent_buffer_max_offset, "" )
{
	ConMsg( "OpenGL Persistent buffer max offset :\n" );
	ConMsg( "  Vertex buffer : %d bytes (%f MB) \n", gMaxPersistentOffset[kGLMVertexBuffer], gMaxPersistentOffset[kGLMVertexBuffer] / (1024.0f*1024.0f) );
	ConMsg( "  Index buffer : %d bytes (%f MB) \n", gMaxPersistentOffset[kGLMIndexBuffer], gMaxPersistentOffset[kGLMIndexBuffer] / (1024.0f*1024.0f) );
	ConMsg( "  Uniform buffer : %d bytes (%f MB) \n", gMaxPersistentOffset[kGLMUniformBuffer], gMaxPersistentOffset[kGLMUniformBuffer] / (1024.0f*1024.0f) );
	ConMsg( "  Pixel buffer : %d bytes (%f MB) \n", gMaxPersistentOffset[kGLMPixelBuffer], gMaxPersistentOffset[kGLMPixelBuffer] / (1024.0f*1024.0f) );
}

CPersistentBuffer::CPersistentBuffer()
:
	m_nSize( 0 )
	, m_nHandle( 0 )
	, m_pImmutablePersistentBuf( NULL )
	, m_nOffset( 0 )
	, m_bReferencedSinceFence( false )
#ifdef HAVE_GL_ARB_SYNC
	, m_nSyncObj( 0 )
#endif
{}

CPersistentBuffer::~CPersistentBuffer()
{
	Deinit();
}

void CPersistentBuffer::Init( EGLMBufferType type,uint nSize )
{
//	Assert( gGL->m_bHave_GL_EXT_buffer_storage );
//	Assert( gGL->m_bHave_GL_ARB_map_buffer_range );

	m_nSize		= nSize;
	m_nOffset	= 0;
	m_type		= type;

	switch ( type )
	{
	case kGLMVertexBuffer:	m_buffGLTarget = GL_ARRAY_BUFFER; break;
	case kGLMIndexBuffer:	m_buffGLTarget = GL_ELEMENT_ARRAY_BUFFER; break;

	default: Assert( nSize == 0 );
	}
	
	if ( m_nSize > 0 )
	{
		gGL->glGenBuffers( 1, &m_nHandle );
		gGL->glBindBuffer( m_buffGLTarget, m_nHandle );

		// Create persistent immutable buffer that we will permanently map.  This buffer can be written from any thread (not just
		// the renderthread)
		//
		// GL_MAP_COHERENT_BIT is the default, but some mobile GLES drivers
		// (observed on Mali-G31 r13p0) expose a persistent mapping whose
		// "coherent" behavior is not actually write-through - writes stay
		// stale on the GPU side no matter how the ring is managed, producing
		// random-triangle flicker.  -gl_persistent_no_coherent drops the
		// coherent bit and instead maps with GL_MAP_FLUSH_EXPLICIT_BIT so the
		// explicit glFlushMappedBufferRange in CGLMBuffer::Unlock/GetHandle is
		// well-defined - exactly the map semantics of the proven plain
		// map/unmap path (the earlier diagnostic omitted FLUSH_EXPLICIT,
		// which made the flush undefined per spec and masked the result).
		GLbitfield nMapFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;
		if ( !CommandLine()->FindParm( "-gl_persistent_no_coherent" ) )
		{
			nMapFlags |= GL_MAP_COHERENT_BIT;
		}
		else
		{
			nMapFlags |= GL_MAP_FLUSH_EXPLICIT_BIT;
		}
		m_nMapFlags = nMapFlags;

		// GL_MAP_FLUSH_EXPLICIT_BIT is a map-only flag; glBufferStorageEXT
		// rejects it as a storage flag.
		gGL->glBufferStorageEXT( m_buffGLTarget, m_nSize, (const GLvoid *)NULL, nMapFlags & ~GL_MAP_FLUSH_EXPLICIT_BIT ); // V_GL_REQ: GL_EXT_buffer_storage, GL_ARB_map_buffer_range, GL_VERSION_4_4

		// Map the buffer for all of eternity.  Pointer can be used from multiple threads.
		m_pImmutablePersistentBuf = gGL->glMapBufferRange( m_buffGLTarget, 0, m_nSize, nMapFlags ); // V_GL_REQ: GL_ARB_map_buffer_range, GL_EXT_buffer_storage, GL_VERSION_4_4
		Assert( m_pImmutablePersistentBuf != NULL );
	}
}

void CPersistentBuffer::Remap()
{
	if ( !m_pImmutablePersistentBuf )
		return;

	// Cycle the mapping so the driver runs its normal unmap flush path.
	// Legal for persistent mappings; the returned CPU pointer may change,
	// which is fine - every Lock re-derives its slice pointer from GetPtr().
	gGL->glUnmapBuffer( m_buffGLTarget );
	m_pImmutablePersistentBuf = gGL->glMapBufferRange( m_buffGLTarget, 0, m_nSize, m_nMapFlags );
	Assert( m_pImmutablePersistentBuf != NULL );
}

void CPersistentBuffer::Deinit()
{
	if ( !m_pImmutablePersistentBuf )
	{
		return;
	}

	BlockUntilNotBusy();

	gGL->glBindBuffer( m_buffGLTarget, m_nHandle );
	gGL->glUnmapBuffer( m_buffGLTarget );
	gGL->glBindBuffer( m_buffGLTarget, 0 );

	gGL->glDeleteBuffers( 1, &m_nHandle );
	
	m_nSize		= 0;
	m_nHandle	= 0;
	m_nOffset	= 0;
	m_pImmutablePersistentBuf = NULL;
}

void CPersistentBuffer::InsertFence()
{
#ifdef HAVE_GL_ARB_SYNC
	if (m_nSyncObj)
	{
		gGL->glDeleteSync( m_nSyncObj );
	}

	m_nSyncObj = gGL->glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );
#endif
}

void CPersistentBuffer::BlockUntilNotBusy()
{
#ifdef HAVE_GL_ARB_SYNC
	if (m_nSyncObj)
	{
		if ( CommandLine()->FindParm( "-gl_persistent_finish" ) )
		{
			// Diagnostic: some mobile GLES drivers (Mali-G31 r13p0) appear to
			// signal GL sync objects before the GPU has actually retired the
			// referenced work, letting the ring overwrite data the GPU still
			// reads (random-triangle flicker that no slice/bookkeeping model
			// removes).  glFinish blocks until ALL prior work completes, so a
			// clean result with this switch implicates the fence machinery;
			// a dirty result implicates the persistent mapping itself.
			gGL->glFinish();
		}
		else
		{
			// 1s is generous for a 2-3 frame ring on any part we target; if this
			// ever fires the GPU is wedged or the ring was sized too small.  The
			// old 3s timeout just hid the stall.
			GLenum result = gGL->glClientWaitSync( m_nSyncObj, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ULL );
			if ( result != GL_ALREADY_SIGNALED && result != GL_CONDITION_SATISFIED )
			{
				Warning( "CPersistentBuffer: glClientWaitSync timed out (0x%X)\n", (int)result );
			}
		}

		gGL->glDeleteSync( m_nSyncObj );

		m_nSyncObj = 0;
	}
#endif
	m_nOffset = 0;
	m_bReferencedSinceFence = false;
}

void CPersistentBuffer::Append( uint nSize )
{
	m_nOffset += nSize;
	Assert( m_nOffset <= m_nSize );

	gMaxPersistentOffset[m_type] = Max( m_nOffset, gMaxPersistentOffset[m_type] );
}

//===========================================================================//

#if GL_ENABLE_INDEX_VERIFICATION

CGLMBufferSpanManager::CGLMBufferSpanManager() : 
	m_pCtx( NULL ),
	m_nBufType( kGLMVertexBuffer ),
	m_nBufSize( 0 ),
	m_bDynamic( false ),
	m_nSpanEndMax( -1 ),
	m_nNumAllocatedBufs( 0 ),
	m_nTotalBytesAllocated( 0 )
{
}

CGLMBufferSpanManager::~CGLMBufferSpanManager()
{
	Deinit();
}

void CGLMBufferSpanManager::Init( GLMContext *pContext, EGLMBufferType nBufType, uint nInitialCapacity, uint nBufSize, bool bDynamic )
{
	Assert( ( nBufType == kGLMIndexBuffer ) || ( nBufType == kGLMVertexBuffer ) );

	m_pCtx = pContext;
	m_nBufType = nBufType;
	
	m_nBufSize = nBufSize;
	m_bDynamic = bDynamic;

	m_ActiveSpans.EnsureCapacity( nInitialCapacity );
	m_DeletedSpans.EnsureCapacity( nInitialCapacity );
	m_nSpanEndMax = -1;

	m_nNumAllocatedBufs = 0;
	m_nTotalBytesAllocated = 0;
}

bool CGLMBufferSpanManager::AllocDynamicBuf( uint nSize, GLDynamicBuf_t &buf )
{
	buf.m_nGLType = GetGLBufType();
	buf.m_nActualBufSize = nSize;
	buf.m_nHandle = 0;
	buf.m_nSize = nSize;

	m_nNumAllocatedBufs++;
	m_nTotalBytesAllocated += buf.m_nActualBufSize;

	return true;
}

void CGLMBufferSpanManager::ReleaseDynamicBuf( GLDynamicBuf_t &buf )
{
	Assert( m_nNumAllocatedBufs > 0 );
	m_nNumAllocatedBufs--;

	Assert( m_nTotalBytesAllocated >= (int)buf.m_nActualBufSize );
	m_nTotalBytesAllocated -= buf.m_nActualBufSize;
}

void CGLMBufferSpanManager::Deinit()
{
	if ( !m_pCtx )
		return;

	for ( int i = 0; i < m_ActiveSpans.Count(); i++ )
	{
		if ( m_ActiveSpans[i].m_bOriginalAlloc )
			ReleaseDynamicBuf( m_ActiveSpans[i].m_buf );
	}
	m_ActiveSpans.SetCountNonDestructively( 0 );

	for ( int i = 0; i < m_DeletedSpans.Count(); i++ )
		ReleaseDynamicBuf( m_DeletedSpans[i].m_buf );

	m_DeletedSpans.SetCountNonDestructively( 0 );

	m_pCtx->BindGLBufferToCtx( GetGLBufType(), NULL, true );

	m_nSpanEndMax = -1;
	m_pCtx = NULL;

	Assert( !m_nNumAllocatedBufs );
	Assert( !m_nTotalBytesAllocated );
}

void CGLMBufferSpanManager::DiscardAllSpans()
{
	for ( int i = 0; i < m_ActiveSpans.Count(); i++ )
	{
		if ( m_ActiveSpans[i].m_bOriginalAlloc )
			ReleaseDynamicBuf( m_ActiveSpans[i].m_buf );
	}
	m_ActiveSpans.SetCountNonDestructively( 0 );

	for ( int i = 0; i < m_DeletedSpans.Count(); i++ )
		ReleaseDynamicBuf( m_DeletedSpans[i].m_buf );

	m_DeletedSpans.SetCountNonDestructively( 0 );

	m_nSpanEndMax = -1;

	Assert( !m_nNumAllocatedBufs );
	Assert( !m_nTotalBytesAllocated );
}

// TODO: Add logic to detect incorrect usage of bNoOverwrite.
CGLMBufferSpanManager::ActiveSpan_t *CGLMBufferSpanManager::AddSpan( uint nOffset, uint nMaxSize, uint nActualSize, bool bDiscard, bool bNoOverwrite  )
{
	(void)bDiscard;
	(void)bNoOverwrite;

	const uint nStart = nOffset;
	const uint nSize = nActualSize;
	const uint nEnd = nStart + nSize;

	GLDynamicBuf_t newDynamicBuf;
	if ( !AllocDynamicBuf( nSize, newDynamicBuf ) )
	{
		DXABSTRACT_BREAK_ON_ERROR();
		return NULL;
	}

	if ( (int)nStart < m_nSpanEndMax )
	{
		// Lock region potentially overlaps another previously locked region (since the last discard) - this is a very rarely (if ever) taken path in Source1 games.
		int i = 0;
		while ( i < m_ActiveSpans.Count() )
		{
			ActiveSpan_t &existingSpan = m_ActiveSpans[i];
			if ( ( nEnd <= existingSpan.m_nStart ) || ( nStart >= existingSpan.m_nEnd ) )
			{
				i++;
				continue;
			}

			Warning( "GL performance warning: AddSpan() at offset %u max size %u actual size %u, on a %s %s buffer of total size %u, overwrites an existing active lock span at offset %u size %u!\n", 
				nOffset, nMaxSize, nActualSize, 
				m_bDynamic ? "dynamic" : "static", ( m_nBufType == kGLMVertexBuffer ) ? "vertex" : "index", m_nBufSize, 
				existingSpan.m_nStart, existingSpan.m_nEnd - existingSpan.m_nStart );
			
			if ( ( nStart <= existingSpan.m_nStart ) && ( nEnd >= existingSpan.m_nEnd ) )
			{
				if ( existingSpan.m_bOriginalAlloc )
				{
					// New span totally covers existing span
					// Can't immediately delete the span's buffer because it could be referred to by another (child) span.
					m_DeletedSpans.AddToTail( existingSpan );
				}

				// Delete span
				m_ActiveSpans[i] = m_ActiveSpans[ m_ActiveSpans.Count() - 1 ];
				m_ActiveSpans.SetCountNonDestructively( m_ActiveSpans.Count() - 1 );
				continue;
			}

			// New span does NOT fully cover the existing span (partial overlap)
			if ( nStart < existingSpan.m_nStart )
			{
				// New span starts before existing span, but ends somewhere inside, so shrink it (start moves "right")
				existingSpan.m_nStart = nEnd;
			}
			else if ( nEnd > existingSpan.m_nEnd )
			{
				// New span ends after existing span, but starts somewhere inside (end moves "left")
				existingSpan.m_nEnd = nStart;
			}
			else //if ( ( nStart >= existingSpan.m_nStart ) && ( nEnd <= existingSpan.m_nEnd ) )
			{
				// New span lies inside of existing span
				if ( nStart == existingSpan.m_nStart )
				{
					// New span begins inside the existing span (start moves "right")
					existingSpan.m_nStart = nEnd;
				}
				else
				{
					if ( nEnd < existingSpan.m_nEnd )
					{
						// New span is completely inside existing span
						m_ActiveSpans.AddToTail( ActiveSpan_t( nEnd, existingSpan.m_nEnd, existingSpan.m_buf, false ) );
					}

					existingSpan.m_nEnd = nStart;
				}
			}

			Assert( existingSpan.m_nStart < existingSpan.m_nEnd );
			i++;
		}
	}

	newDynamicBuf.m_nLockOffset = nStart;
	newDynamicBuf.m_nLockSize = nSize;

	m_ActiveSpans.AddToTail( ActiveSpan_t( nStart, nEnd, newDynamicBuf, true ) );
	m_nSpanEndMax = MAX( m_nSpanEndMax, (int)nEnd );

	return &m_ActiveSpans.Tail();
}

bool CGLMBufferSpanManager::IsValid( uint nOffset, uint nSize ) const
{
	const uint nEnd = nOffset + nSize;
	
	int nTotalBytesRemaining = nSize;

	for ( int i = m_ActiveSpans.Count() - 1; i >= 0; --i )
	{
		const ActiveSpan_t &span = m_ActiveSpans[i];
		
		if ( span.m_nEnd <= nOffset )
			continue;
		if ( span.m_nStart >= nEnd )
			continue;

		uint nIntersectStart = MAX( span.m_nStart, nOffset );
		uint nIntersectEnd = MIN( span.m_nEnd, nEnd );
		Assert( nIntersectStart <= nIntersectEnd );

		nTotalBytesRemaining -= ( nIntersectEnd - nIntersectStart );
		Assert( nTotalBytesRemaining >= 0 );
		if ( nTotalBytesRemaining <= 0 )
			break;
	}

	return nTotalBytesRemaining == 0;
}
#endif // GL_ENABLE_INDEX_VERIFICATION

// glBufferSubData() with a max size limit, to work around NVidia's threaded driver limits (anything > than roughly 256KB triggers a sync with the server thread).
void glBufferSubDataMaxSize( GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data, uint nMaxSizePerCall )
{
#if TOGL_SUPPORT_NULL_DEVICE
	if ( g_bNullD3DDevice ) return;
#endif

	uint nBytesLeft = size;
	uint nOfs = 0;
	while ( nBytesLeft )
	{
		uint nBytesToCopy = MIN( nMaxSizePerCall, nBytesLeft );

		gGL->glBufferSubData( target, offset + nOfs, nBytesToCopy, static_cast<const unsigned char *>( data ) + nOfs );

		nBytesLeft -= nBytesToCopy;
		nOfs += nBytesToCopy;
	}
}

CGLMBuffer::CGLMBuffer( GLMContext *pCtx, EGLMBufferType type, uint size, uint options )
{
	m_pCtx = pCtx;
	m_type = type;
	
	m_bDynamic = ( options & GLMBufferOptionDynamic ) != 0;
				
	switch ( m_type )
	{
		case kGLMVertexBuffer:	m_buffGLTarget = GL_ARRAY_BUFFER; break;
		case kGLMIndexBuffer:	m_buffGLTarget = GL_ELEMENT_ARRAY_BUFFER; break;
		case kGLMUniformBuffer:	m_buffGLTarget = GL_UNIFORM_BUFFER; break;
		case kGLMPixelBuffer:	m_buffGLTarget = GL_PIXEL_UNPACK_BUFFER; break;
		
		default: Assert(!"Unknown buffer type" ); DXABSTRACT_BREAK_ON_ERROR();
	}
				
	m_nSize = size;
	m_nActualSize = size;
	m_bMapped = false;
	m_pLastMappedAddress = NULL;

	m_pStaticBuffer = NULL;
	m_nPinnedMemoryOfs = -1;
	m_nPersistentBufferStartOffset = 0;
	m_bUsingPersistentBuffer = false;

	m_bEnableAsyncMap = false;
	m_bEnableExplicitFlush = false;
	m_dirtyMinOffset = m_dirtyMaxOffset = 0;								// adjust/grow on lock, clear on unlock

	m_pCtx->CheckCurrent();
	m_nRevision = rand();
		
	m_pPseudoBuf = NULL;
	m_pActualPseudoBuf = NULL;

	m_bPseudo = false;
	m_nPseudoLockOffset = 0xFFFFFFFF;
	m_nPersistentBufferSlot = 0;
	m_bPendingPersistentFlush = false;
	m_nPendingPersistentFlushStart = 0;
	m_nPendingPersistentFlushEnd = 0;
	m_nRingSlot = 0;
	m_nRingSlotCount = 1;
	m_nRingSlotFrame = 0xFFFFFFFF;
	m_ringHandles[0] = m_ringHandles[1] = m_ringHandles[2] = 0;
		
#if GL_ENABLE_UNLOCK_BUFFER_OVERWRITE_DETECTION
	m_bPseudo = true;
#endif

	// Client-memory pseudo buffers have no GPU-side storage: on GLES the driver
	// must re-copy the referenced vertex/index ranges on every draw (client-side
	// vertex arrays are undefined in ES 3.x and only tolerated via a slow legacy
	// path on Mali).  Real VBOs with map/orphan keep dynamic data resident on the
	// GPU and are strictly faster on Mali-G31-class parts.  Pseudo buffers remain
	// available for debugging via -gl_enable_pseudobufs.
	if( V_stristr(gGL->m_pGLDriverStrings[cGLVendorString], "arm") != NULL )
	{
		g_bUsePseudoBufs = CommandLine()->CheckParm( "-gl_enable_pseudobufs" ) != NULL;

		// The static-buffer path glBufferSubData's into the same VBO every
		// frame without orphaning, forcing a CPU/GPU serialization per lock -
		// measured at a 5x FPS drop on Mali-G31.  Keep it hard-disabled on
		// ARM even if -gl_enable_static_buffer is passed.
		g_bDisableStaticBuffer = true;
	}

	if ( !g_bMultiBufferVBOsChecked )
	{
		// On Mali (r13p0 tested) glFlushMappedBufferRange on persistently
		// mapped buffers is ignored - only glUnmapBuffer publishes writes -
		// so the persistent ring is not viable there without a per-lock
		// unmap/remap cycle.  Default the stall-free multi-buffer ring ON
		// for ARM instead; -gl_no_multi_buffer_vbos restores the
		// single-VBO path.  Other drivers opt in via -gl_multi_buffer_vbos.
		if ( V_stristr( gGL->m_pGLDriverStrings[cGLVendorString], "arm" ) != NULL )
		{
			g_bMultiBufferVBOs = !CommandLine()->CheckParm( "-gl_no_multi_buffer_vbos" );
		}
		else
		{
			g_bMultiBufferVBOs = CommandLine()->CheckParm( "-gl_multi_buffer_vbos" ) != NULL;
		}
		g_bMultiBufferVBOsChecked = true;
	}

	if ( m_bDynamic )
	{
		static bool s_bReportedDynamicBufferMode = false;
		if ( !s_bReportedDynamicBufferMode )
		{
			const char *pMode = g_bUsePseudoBufs ? "pseudo/client memory" :
				( gGL->m_bHave_GL_EXT_buffer_storage ? "persistent mapped VBO ring" :
				  ( g_bMultiBufferVBOs ? "multi-buffered mapped VBO ring" : "ordinary mapped VBO" ) );
			Msg( "GL dynamic buffer mode: %s\n", pMode );
			s_bReportedDynamicBufferMode = true;
		}
	}

#if GL_ENABLE_INDEX_VERIFICATION
	m_BufferSpanManager.Init( m_pCtx, m_type, 512, m_nSize, m_bDynamic );
		
	if ( m_type == kGLMIndexBuffer )
		m_bPseudo = true;
#endif
	
	if ( g_bUsePseudoBufs && m_bDynamic )
	{
		m_bPseudo = true;
	}
			
	if ( m_bPseudo )
	{
		m_nHandle = 0;		

#if GL_ENABLE_UNLOCK_BUFFER_OVERWRITE_DETECTION
		m_nDirtyRangeStart = 0xFFFFFFFF;
		m_nDirtyRangeEnd = 0;

		m_nActualSize = ALIGN_VALUE( ( m_nSize + sizeof( uint32 ) ), 4096 );
		m_pPseudoBuf = m_pActualPseudoBuf = (char *)VirtualAlloc( NULL, m_nActualSize, MEM_COMMIT, PAGE_READWRITE );
		if ( !m_pPseudoBuf )
		{
			Error( "VirtualAlloc() failed!\n" );
		}

		for ( uint i = 0; i < m_nActualSize / sizeof( uint32 ); i++ )
		{
			reinterpret_cast< uint32 * >( m_pPseudoBuf )[i] = 0xDEADBEEF;
		}

		DWORD nOldProtect;
		BOOL bResult = VirtualProtect( m_pActualPseudoBuf, m_nActualSize, PAGE_READONLY, &nOldProtect );
		if ( !bResult )
		{
			Error( "VirtualProtect() failed!\n" );
		}
#else
		m_nActualSize = size + 15;
		m_pActualPseudoBuf = (char*)malloc( m_nActualSize );
		m_pPseudoBuf = (char*)(((intp)m_pActualPseudoBuf + 15) & ~15);
#endif
		
		m_pCtx->BindBufferToCtx( m_type, NULL );		// exit with no buffer bound
	}
	else
	{
		gGL->glGenBuffers( 1, &m_nHandle );

		m_pCtx->BindBufferToCtx( m_type, this );	// causes glBindBufferARB

		// buffers start out static, but if they get orphaned and gl_bufmode is non zero,
		// then they will get flipped to dynamic.
		
		GLenum hint = GL_STREAM_DRAW;
		switch (m_type)
		{
			case kGLMVertexBuffer:	hint = m_bDynamic ? GL_DYNAMIC_DRAW : GL_STREAM_DRAW; break;
			case kGLMIndexBuffer:	hint = m_bDynamic ? GL_DYNAMIC_DRAW : GL_STREAM_DRAW; break;
			case kGLMUniformBuffer:	hint = GL_DYNAMIC_DRAW; break;
			case kGLMPixelBuffer:	hint = m_bDynamic ? GL_DYNAMIC_DRAW : GL_STREAM_DRAW; break;
			
			default: Assert(!"Unknown buffer type" ); DXABSTRACT_BREAK_ON_ERROR();
		}

		gGL->glBufferData( m_buffGLTarget, m_nSize, (const GLvoid*)NULL, hint );	// may ultimately need more hints to set the usage correctly (esp for streaming)

		SetModes( false, true, true );

		// -gl_multi_buffer_vbos: give dynamic VB/IB a ring of plain GL
		// buffers.  Lock advances the slot on every DISCARD, so a map never
		// waits on GPU work from earlier frames (each slot is 2 frames old
		// by the time it is rewritten).  Skipped when the persistent ring
		// is active (buffer storage takes precedence).
		m_ringHandles[0] = m_nHandle;
		if ( g_bMultiBufferVBOs && m_bDynamic &&
			 ( ( m_type == kGLMVertexBuffer ) || ( m_type == kGLMIndexBuffer ) ) &&
			 !gGL->m_bHave_GL_EXT_buffer_storage )
		{
			for ( uint i = 1; i < 3; ++i )
			{
				gGL->glGenBuffers( 1, &m_ringHandles[i] );
				gGL->glBindBuffer( m_buffGLTarget, m_ringHandles[i] );
				gGL->glBufferData( m_buffGLTarget, m_nSize, (const GLvoid*)NULL, hint );
			}
			m_nRingSlotCount = 3;
		}

		m_pCtx->BindBufferToCtx( m_type, NULL );	// unbind me
	}
}

CGLMBuffer::~CGLMBuffer( )
{
	m_pCtx->CheckCurrent();
	
	if ( m_bPseudo )
	{
#if GL_ENABLE_UNLOCK_BUFFER_OVERWRITE_DETECTION
		BOOL bResult = VirtualFree( m_pActualPseudoBuf, 0, MEM_RELEASE );
		if ( !bResult )
		{
			Error( "VirtualFree() failed!\n" );
		}
#else
		free( m_pActualPseudoBuf );
#endif
		m_pActualPseudoBuf = NULL;
		m_pPseudoBuf = NULL;
	}
	else
	{
		// Delete any multi-buffer ring handles beyond the primary one
		// (m_nHandle is always one of the ring slots when the ring is on).
		for ( uint i = 1; i < m_nRingSlotCount; ++i )
		{
			if ( m_ringHandles[i] && ( m_ringHandles[i] != m_nHandle ) )
			{
				gGL->glDeleteBuffers( 1, &m_ringHandles[i] );
				m_ringHandles[i] = 0;
			}
		}
		gGL->glDeleteBuffers( 1, &m_nHandle );
	}
	
	m_pCtx = NULL;
	m_nHandle = 0;
		
	m_pLastMappedAddress = NULL;

#if GL_ENABLE_INDEX_VERIFICATION
	m_BufferSpanManager.Deinit();
#endif
}

void CGLMBuffer::SetModes( bool bAsyncMap, bool bExplicitFlush, bool bForce )
{
	// assumes buffer is bound. called by constructor and by Lock.

	if ( m_bPseudo )
	{
		// ignore it...
	}
	else
	{
		if ( bForce || ( m_bEnableAsyncMap != bAsyncMap ) )
		{
			m_bEnableAsyncMap = bAsyncMap;
		}

		if ( bForce || ( m_bEnableExplicitFlush != bExplicitFlush ) )
		{
			// Note that the GL_ARB_map_buffer_range path handles this in the glMapBufferRange() call in Lock().
			// note the sense of the parameter, it's TRUE if you *want* auto-flush-on-unmap, so for explicit-flush, you turn it to false.
			m_bEnableExplicitFlush = bExplicitFlush;
		}
	}
}

#if GL_ENABLE_INDEX_VERIFICATION
bool CGLMBuffer::IsSpanValid( uint nOffset, uint nSize ) const
{
	return m_BufferSpanManager.IsValid( nOffset, nSize );
}
#endif

void CGLMBuffer::FlushRange( uint offset, uint size )
{
	if ( m_pStaticBuffer )
	{
	}
	else if ( m_bPseudo )
	{
		// nothing to do
	}
	else
	{
#ifdef REPORT_LOCK_TIME
		double flStart = Plat_FloatTime();
#endif

		gGL->glFlushMappedBufferRange( m_buffGLTarget, (GLintptr)( offset - m_dirtyMinOffset ), (GLsizeiptr)size );
#ifdef REPORT_LOCK_TIME
		double flEnd = Plat_FloatTime();
		if ( flEnd - flStart > 5.0 / 1000.0 )
		{
			int nDelta = ( int )( ( flEnd - flStart ) * 1000 );
			if ( nDelta > 2 )
			{
				Msg( "**** " );
			}
			Msg( "glFlushMappedBufferRange Time %d: ( Name=%d BufSize=%d ) Target=%p Offset=%d FlushSize=%d\n", nDelta, m_nHandle, m_nSize, m_buffGLTarget, offset - m_dirtyMinOffset, size );
		}
#endif

		// If you don't have any extension support here, you'll flush the whole buffer on unmap. Performance loss, but it's still safe and correct.
	}
}

void CGLMBuffer::Lock( GLMBuffLockParams *pParams, char **pAddressOut )
{
#if GL_TELEMETRY_GPU_ZONES
	CScopedGLMPIXEvent glmPIXEvent( "CGLMBuffer::Lock" );
	g_TelemetryGPUStats.m_nTotalBufferLocksAndUnlocks++;
#endif
#if GLM_BUFFER_PERF_ANALYSIS
	CGLMBufferPerfTimer bufferLockTimer( s_BufferPerfStats.m_LockTime );
	++s_BufferPerfStats.m_nLocks;
	if ( m_bPseudo )
	{
		++s_BufferPerfStats.m_nPseudoLocks;
	}
	if ( pParams->m_bDiscard )
	{
		++s_BufferPerfStats.m_nDiscards;
	}
	if ( pParams->m_bNoOverwrite )
	{
		++s_BufferPerfStats.m_nNoOverwrites;
	}
#endif

	char *resultPtr = NULL;
	
	if ( m_bMapped )
	{
		DXABSTRACT_BREAK_ON_ERROR();
		return;
	}
	
	m_pCtx->CheckCurrent();

	Assert( pParams->m_nSize );
	
	m_LockParams = *pParams;
	
	if ( pParams->m_nOffset >= m_nSize )
	{
		DXABSTRACT_BREAK_ON_ERROR();
		return;
	}
	
	if ( ( pParams->m_nOffset + pParams->m_nSize ) > m_nSize)
	{
		DXABSTRACT_BREAK_ON_ERROR();
		return;
	}

#if GL_ENABLE_INDEX_VERIFICATION
	if ( pParams->m_bDiscard )
	{
		m_BufferSpanManager.DiscardAllSpans();
	}
#endif

	m_pStaticBuffer = NULL;
	bool bUsingPersistentBuffer = false;

	if ( m_bPseudo )
	{
		// The attrib-pointer cache (SetBufAndVertexAttribPointer) keys on the
		// client pointer, which moves with the lock offset.  A NOOVERWRITE lock
		// at a different offset would otherwise leave the previous draw's
		// cached pointer stale (the flush's total-revision check never sees a
		// change), so bump the revision whenever the lock address moves.
		if ( pParams->m_bDiscard || ( pParams->m_nOffset != m_nPseudoLockOffset ) )
		{
			m_nRevision++;
			m_nPseudoLockOffset = pParams->m_nOffset;
		}

		// async map modes are a no-op
				
		// calc lock address
		resultPtr = m_pPseudoBuf + pParams->m_nOffset;

#if GL_ENABLE_UNLOCK_BUFFER_OVERWRITE_DETECTION
		BOOL bResult;
		DWORD nOldProtect;
		if ( pParams->m_bDiscard )
		{
			bResult = VirtualProtect( m_pActualPseudoBuf, m_nSize, PAGE_READWRITE, &nOldProtect );
			if ( !bResult )
			{
				Error( "VirtualProtect() failed!\n" );
			}

			m_nDirtyRangeStart = 0xFFFFFFFF;
			m_nDirtyRangeEnd = 0;

			for ( uint i = 0; i < m_nSize / sizeof( uint32 ); i++ )
			{
				reinterpret_cast< uint32 * >( m_pPseudoBuf )[i] = 0xDEADBEEF;
			}

			bResult = VirtualProtect( m_pActualPseudoBuf, m_nSize, PAGE_READONLY, &nOldProtect );
			if ( !bResult )
			{
				Error( "VirtualProtect() failed!\n" );
			}
		}
		uint nProtectOfs = m_LockParams.m_nOffset & 4095;
		uint nProtectEnd = ( m_LockParams.m_nOffset + m_LockParams.m_nSize + 4095 ) & ~4095;
		uint nProtectSize = nProtectEnd - nProtectOfs;
		bResult = VirtualProtect( m_pActualPseudoBuf + nProtectOfs, nProtectSize, PAGE_READWRITE, &nOldProtect );
		if ( !bResult )
		{
			Error( "VirtualProtect() failed!\n" );
		}
#endif
	}
	else if ( m_bDynamic && gGL->m_bHave_GL_EXT_buffer_storage &&
			  ( ( !pParams->m_bDiscard && m_bUsingPersistentBuffer && ( m_nPersistentBufferSlot == m_pCtx->GetCurPersistentBufferIndex() ) ) ||
				( m_pCtx->GetCurPersistentBuffer( m_type )->GetBytesRemaining() >= m_nSize ) ) )
	{
		// Persistent ring path.  Every DISCARD - the frame-start reset or a
		// mid-frame wrap when the engine's shared buffer fills up - starts a
		// FRESH slice in the ring and latches a stable base for it.  The
		// engine writes at offset 0 after a discard and keeps the previous
		// slice's draws queued, so slices must never be reused within a frame.
		// NOOVERWRITE appends resolve to base + lockOffset inside the live
		// slice.  (The original append-per-lock model moved the base on every
		// lock, breaking chunk draws; the first stable-base attempt reused the
		// slice on mid-frame wraps, overwriting the queued geometry - the
		// flicker.)
		CPersistentBuffer *pTempBuffer = m_pCtx->GetCurPersistentBuffer( m_type );
		const bool bHasLiveSlice = m_bUsingPersistentBuffer && ( m_nPersistentBufferSlot == m_pCtx->GetCurPersistentBufferIndex() );

		if ( !pParams->m_bDiscard && bHasLiveSlice )
		{
			// NOOVERWRITE append into the live slice - the base is stable for
			// the whole slice, so every draw resolves its own lock offset
			// against the same ring region.
			resultPtr = static_cast<char*>(pTempBuffer->GetPtr()) + m_nPersistentBufferStartOffset + pParams->m_nOffset;
			bUsingPersistentBuffer = true;
		}
		else
		{
			// Fresh slice: DISCARD (frame-start reset or mid-frame wrap) or
			// the first lock of the frame (capacity was verified above).
			// Reserve the buffer's full size so every later append this frame
			// lands inside the slice.  The revision bump re-issues the attrib
			// pointers so the new base is used by the draws that follow.
			pTempBuffer->Append( m_nSize );
			m_nPersistentBufferStartOffset = pTempBuffer->GetOffset() - m_nSize;
			m_nPersistentBufferSlot = m_pCtx->GetCurPersistentBufferIndex();
			m_nRevision++;

			// A DISCARD invalidates the previous slice's data, so any
			// unflushed range from the old slice can never be drawn again.
			m_bPendingPersistentFlush = false;

			resultPtr = static_cast<char*>(pTempBuffer->GetPtr()) + m_nPersistentBufferStartOffset + pParams->m_nOffset;
			bUsingPersistentBuffer = true;
		}
	}
	else if ( !g_bDisableStaticBuffer && ( pParams->m_bDiscard || pParams->m_bNoOverwrite ) && ( pParams->m_nSize <= GL_STATIC_BUFFER_SIZE ) )
	{
#if TOGL_SUPPORT_NULL_DEVICE
		if ( !g_bNullD3DDevice )
#endif
		{
			if ( pParams->m_bDiscard )
			{
				m_pCtx->BindBufferToCtx( m_type, this );

				// observe gl_bufmode on any orphan event.
				// if orphaned and bufmode is nonzero, flip it to dynamic.
				GLenum hint = gl_bufmode.GetInt() ? GL_DYNAMIC_DRAW : GL_STREAM_DRAW;
				gGL->glBufferData( m_buffGLTarget, m_nSize, (const GLvoid*)NULL, hint );
			
				m_nRevision++; // revision grows on orphan event
			}
		}

		m_dirtyMinOffset = pParams->m_nOffset;
		m_dirtyMaxOffset = pParams->m_nOffset + pParams->m_nSize;

		switch ( m_type )
		{
			case kGLMVertexBuffer:
			{
				m_pStaticBuffer = m_StaticBuffers[ 0 ];
				break;
			}
			case kGLMIndexBuffer:
			{
				m_pStaticBuffer = m_StaticBuffers[ 1 ];
				break;
			}
			default:
			{
				DXABSTRACT_BREAK_ON_ERROR();
				return;
			}
		}

		resultPtr = m_pStaticBuffer;
	}
	else
	{
		// Multi-buffer ring: the slot advances ONCE PER FRAME (the first
		// DISCARD of each frame), not per discard.  A single frame can
		// discard + rewrap the shared dynamic VB many times (flexed and
		// software-skinned meshes lock it repeatedly); advancing per
		// discard recycled slot buffers while the GPU was still reading
		// them - TBDR submits are deferred, so the GPU consumes a frame's
		// geometry after the CPU has moved on.  One slot per frame gives
		// each slot's data a 3-frame lifetime, matching the persistent
		// ring's guarantee.  NOOVERWRITE appends stay on the current slot.
		if ( g_bMultiBufferVBOs && m_bDynamic && pParams->m_bDiscard && ( m_nRingSlotCount > 1 ) )
		{
			const uint nFrame = m_pCtx->m_nCurFrame;
			if ( nFrame != m_nRingSlotFrame )
			{
				m_nRingSlot = ( m_nRingSlot + 1 ) % m_nRingSlotCount;
				m_nHandle = m_ringHandles[ m_nRingSlot ];
				m_nRingSlotFrame = nFrame;
				m_nRevision++;	// new GL buffer name - attrib pointers must re-issue
			}
		}

		// bind (yes, even for pseudo - this binds name 0)
		m_pCtx->BindBufferToCtx( m_type, this );

		// perform discard if requested
		if ( pParams->m_bDiscard )
		{
			// Multi-buffer: the slot advances once per frame (see above), so
			// no per-discard orphan realloc is needed and the revision was
			// already bumped at the slot swap.  Mid-frame discards that stay
			// on the current slot rely on GL_MAP_INVALIDATE_BUFFER_BIT in
			// the map below to rename storage while earlier draws of this
			// frame's data are still in flight - the same contract the
			// single-buffer path has used all along.
			if ( !g_bMultiBufferVBOs || ( m_nRingSlotCount <= 1 ) )
			{
				// observe gl_bufmode on any orphan event.
				// if orphaned and bufmode is nonzero, flip it to dynamic.
				
				// We always want to call glBufferData( ..., NULL ) on discards, even though we're using the GL_MAP_INVALIDATE_BUFFER_BIT flag, because this flag is actually only a hint according to AMD.
				// On ARM/Mali (UMA) the orphan reallocates the shared dynamic VB
				// (1.5MB) on every frame; the discard is usually satisfied cheaper
				// by GL_MAP_INVALIDATE_BUFFER_BIT on the same storage (the flag is
				// already set in the map below).  -gl_force_orphan restores the
				// classic realloc behavior.
				static bool s_bMali = ( gGL->m_nDriverProvider == cGLDriverProviderARM );
				if ( !s_bMali || CommandLine()->CheckParm( "-gl_force_orphan" ) )
				{
					GLenum hint = gl_bufmode.GetInt() ? GL_DYNAMIC_DRAW : GL_STREAM_DRAW;
					gGL->glBufferData( m_buffGLTarget, m_nSize, (const GLvoid*)NULL, hint );
				}
									
				m_nRevision++;	// revision grows on orphan event
			}
		}

		// adjust async map option appropriately, leave explicit flush unchanged
		SetModes( pParams->m_bNoOverwrite, m_bEnableExplicitFlush );

		// map
		char *mapPtr;

		// m_bEnableAsyncMap is actually pParams->m_bNoOverwrite
		GLbitfield parms = GL_MAP_WRITE_BIT | ( m_bEnableAsyncMap ? GL_MAP_UNSYNCHRONIZED_BIT : 0 ) | ( pParams->m_bDiscard ? GL_MAP_INVALIDATE_BUFFER_BIT : 0 ) | ( m_bEnableExplicitFlush ? GL_MAP_FLUSH_EXPLICIT_BIT : 0 );

#ifdef REPORT_LOCK_TIME
		double flStart = Plat_FloatTime();
#endif

		mapPtr = (char*)gGL->glMapBufferRange( m_buffGLTarget, pParams->m_nOffset, pParams->m_nSize, parms);

#ifdef REPORT_LOCK_TIME
		double flEnd = Plat_FloatTime();
		if ( flEnd - flStart > 5.0 / 1000.0 )
		{
			int nDelta = ( int )( ( flEnd - flStart ) * 1000 );
			if ( nDelta > 2 )
			{
				Msg( "**** " );
			}
			Msg( "glMapBufferRange Time=%d: ( Name=%d BufSize=%d ) Target=%p Offset=%d LockSize=%d ", nDelta, m_nHandle, m_nSize, m_buffGLTarget, pParams->m_nOffset, pParams->m_nSize );
			if ( parms & GL_MAP_WRITE_BIT )
			{
				Msg( "GL_MAP_WRITE_BIT ");
			}
			if ( parms & GL_MAP_UNSYNCHRONIZED_BIT )
			{
				Msg( "GL_MAP_UNSYNCHRONIZED_BIT ");
			}
			if ( parms & GL_MAP_INVALIDATE_BUFFER_BIT )
			{
				Msg( "GL_MAP_INVALIDATE_BUFFER_BIT ");
			}
			if ( parms & GL_MAP_INVALIDATE_RANGE_BIT )
			{
				Msg( "GL_MAP_INVALIDATE_RANGE_BIT ");
			}
			if ( parms & GL_MAP_FLUSH_EXPLICIT_BIT )
			{
				Msg( "GL_MAP_FLUSH_EXPLICIT_BIT ");
			}
			Msg( "\n" );
		}
#endif
		// calculate offset location
		resultPtr = mapPtr;

		// set range
		m_dirtyMinOffset = pParams->m_nOffset;
		m_dirtyMaxOffset = pParams->m_nOffset + pParams->m_nSize;
	}

	if ( m_bUsingPersistentBuffer != bUsingPersistentBuffer )
	{
		// Up the revision number when switching from a persistent to a non persistent buffer (or vice versa)
		// Ensure the right GL buffer is bound before drawing (and vertex attribs properly set)
		m_nRevision++;
		m_bUsingPersistentBuffer = bUsingPersistentBuffer;
	}

	m_bMapped = true;

	m_pLastMappedAddress = (float*)resultPtr;
	
	*pAddressOut = resultPtr;
}

void CGLMBuffer::Unlock( int nActualSize, const void *pActualData )
{
#if GL_TELEMETRY_GPU_ZONES
	CScopedGLMPIXEvent glmPIXEvent( "CGLMBuffer::Unlock" );
	g_TelemetryGPUStats.m_nTotalBufferLocksAndUnlocks++;
#endif
#if GLM_BUFFER_PERF_ANALYSIS
	CGLMBufferPerfTimer bufferUnlockTimer( s_BufferPerfStats.m_UnlockTime );
	++s_BufferPerfStats.m_nUnlocks;
	if ( m_bPseudo )
	{
		++s_BufferPerfStats.m_nPseudoUnlocks;
	}
#endif

	m_pCtx->CheckCurrent();
	
	if ( !m_bMapped )
	{
		DXABSTRACT_BREAK_ON_ERROR();
		return;
	}

	if ( nActualSize < 0 )
	{
		nActualSize = m_LockParams.m_nSize;
	}

	if ( nActualSize > (int)m_LockParams.m_nSize )
	{
		DXABSTRACT_BREAK_ON_ERROR();
		return;
	}

#if GLM_BUFFER_PERF_ANALYSIS
	switch ( m_type )
	{
		case kGLMVertexBuffer:
			s_BufferPerfStats.m_nVertexBytes += nActualSize;
			break;
		case kGLMIndexBuffer:
			s_BufferPerfStats.m_nIndexBytes += nActualSize;
			break;
		default:
			s_BufferPerfStats.m_nOtherBytes += nActualSize;
			break;
	}
#endif

#if GL_ENABLE_UNLOCK_BUFFER_OVERWRITE_DETECTION
	if ( m_bPseudo )
	{
		// Check guard DWORD to detect buffer overruns (but are still within the last 4KB page so they don't get caught via pagefaults)
		if ( *reinterpret_cast< const uint32 * >( m_pPseudoBuf + m_nSize ) != 0xDEADBEEF )
		{
			// If this fires the client app has overwritten the guard DWORD beyond the end of the buffer.
			DXABSTRACT_BREAK_ON_ERROR();
		}

		static const uint s_nInitialValues[4] = { 0xEF, 0xBE, 0xAD, 0xDE };

		int nActualModifiedStart, nActualModifiedEnd;
		for ( nActualModifiedStart = 0; nActualModifiedStart < (int)m_LockParams.m_nSize; ++nActualModifiedStart )
			if ( reinterpret_cast< const uint8 * >( m_pLastMappedAddress )[nActualModifiedStart] != s_nInitialValues[ ( m_LockParams.m_nOffset + nActualModifiedStart ) & 3 ] )
				break;

		for ( nActualModifiedEnd = m_LockParams.m_nSize - 1; nActualModifiedEnd > nActualModifiedStart; --nActualModifiedEnd )
			if ( reinterpret_cast< const uint8 * >( m_pLastMappedAddress )[nActualModifiedEnd] != s_nInitialValues[ ( m_LockParams.m_nOffset + nActualModifiedEnd ) & 3 ] )
				break;

		int nNumActualBytesModified = 0;

		if ( nActualModifiedEnd >= nActualModifiedStart )
		{
			// The modified check is conservative (i.e. it should always err on the side of detecting <= actual bytes than where actually modified, never more).
			// We primarily care about the case where the user lies about the actual # of modified bytes, which can lead to difficult to debug/inconsistent problems with some drivers.
			// Round up/down the modified range, because the user's data may alias with the initial buffer values (0xDEADBEEF) so we may miss some bytes that where written.
			if ( m_type == kGLMIndexBuffer )
			{
				nActualModifiedStart &= ~1;
				nActualModifiedEnd = MIN( (int)m_LockParams.m_nSize, ( ( nActualModifiedEnd + 1 ) + 1 ) & ~1 ) - 1;
			}
			else
			{
				nActualModifiedStart &= ~3;
				nActualModifiedEnd = MIN( (int)m_LockParams.m_nSize, ( ( nActualModifiedEnd + 1 ) + 3 ) & ~3 ) - 1;
			}
		
			nNumActualBytesModified = nActualModifiedEnd + 1;

			if ( nActualSize < nNumActualBytesModified )
			{
				// The caller may be lying about the # of actually modified bytes in this lock.
				// Has this lock region been previously locked? If so, it may have been previously overwritten before. Otherwise, the region had to be the 0xDEADBEEF fill DWORD at lock time.
				if ( ( m_nDirtyRangeStart > m_nDirtyRangeEnd ) ||
				     ( m_LockParams.m_nOffset > m_nDirtyRangeEnd ) || ( ( m_LockParams.m_nOffset + m_LockParams.m_nSize ) <= m_nDirtyRangeStart )  )
				{
					// If this fires the client has lied about the actual # of bytes they've modified in the buffer - this will cause unreliable rendering on AMD drivers (because AMD actually pays attention to the actual # of flushed bytes).
					DXABSTRACT_BREAK_ON_ERROR();
				}
			}
		
			m_nDirtyRangeStart = MIN( m_nDirtyRangeStart, m_LockParams.m_nOffset + nActualModifiedStart );
			m_nDirtyRangeEnd = MAX( m_nDirtyRangeEnd, m_LockParams.m_nOffset + nActualModifiedEnd );
		}

#if GL_ENABLE_INDEX_VERIFICATION
		if ( nActualModifiedEnd >= nActualModifiedStart )
		{
			int n = nActualModifiedEnd + 1;
			if ( n != nActualSize )
			{
				// The actual detected modified size is < than the reported size, which is common because the last few DWORD's of the vertex format may not actually be used/written (or read by the vertex shader). So just fudge it so the batch consumption checks work.
				if ( ( (int)nActualSize - n ) <= 32 )
				{
					n = nActualSize;
				}
			}

			m_BufferSpanManager.AddSpan( m_LockParams.m_nOffset + nActualModifiedStart, m_LockParams.m_nSize, n - nActualModifiedStart, m_LockParams.m_bDiscard, m_LockParams.m_bNoOverwrite );
		}
#endif		
	}
#elif GL_ENABLE_INDEX_VERIFICATION
	if ( nActualSize > 0 )
	{
		m_BufferSpanManager.AddSpan( m_LockParams.m_nOffset, m_LockParams.m_nSize, nActualSize, m_LockParams.m_bDiscard, m_LockParams.m_bNoOverwrite );
	}
#endif

#if GL_BATCH_PERF_ANALYSIS
	if ( m_type == kGLMIndexBuffer )
		g_nTotalIBLockBytes += nActualSize;
	else if ( m_type == kGLMVertexBuffer )
		g_nTotalVBLockBytes += nActualSize;
#endif
	if ( m_bUsingPersistentBuffer )
	{
		// Make the written range visible to the GPU.  The mapping is
		// persistent+coherent, but an explicit flush costs one driver call and
		// covers drivers whose "coherent" mapping is not actually write-
		// through - without it, stale GPU reads produced random-triangle
		// flicker on Mali r13p0 even though the equivalent map path (which
		// flushes explicitly) rendered cleanly.  The region itself was
		// reserved at lock time, so no ring accounting happens here.
		//
		// Instead of one bind+glFlushMappedBufferRange per unlock, accumulate
		// the dirty span and let GetHandle() flush it once, right before the
		// buffer is first handed to a draw.  The engine performs hundreds of
		// dynamic locks per frame; flushing per unlock was thousands of
		// redundant driver calls.
		if ( nActualSize > 0 )
		{
			const uint nRangeStart = m_nPersistentBufferStartOffset + m_LockParams.m_nOffset;
			const uint nRangeEnd = nRangeStart + (uint)nActualSize;
			if ( !m_bPendingPersistentFlush )
			{
				m_bPendingPersistentFlush = true;
				m_nPendingPersistentFlushStart = nRangeStart;
				m_nPendingPersistentFlushEnd = nRangeEnd;
			}
			else
			{
				m_nPendingPersistentFlushStart = MIN( m_nPendingPersistentFlushStart, nRangeStart );
				m_nPendingPersistentFlushEnd = MAX( m_nPendingPersistentFlushEnd, nRangeEnd );
			}

			if ( CommandLine()->FindParm( "-gl_persistent_unmap_publish" ) )
			{
				// Diagnostic: some drivers only publish mapped writes at
				// glUnmapBuffer and ignore glFlushMappedBufferRange entirely
				// on persistent mappings.  Cycle the mapping so the driver
				// runs its normal unmap flush path.
				CPersistentBuffer *pSlot = m_pCtx->GetPersistentBuffer( m_nPersistentBufferSlot, m_type );
				gGL->glBindBuffer( m_buffGLTarget, pSlot->GetHandle() );
				pSlot->Remap();
			}
		}
	}
	else if ( m_pStaticBuffer )
	{
#if TOGL_SUPPORT_NULL_DEVICE
		if ( !g_bNullD3DDevice )
#endif
		{
			if ( nActualSize )
			{
				tmZone( TELEMETRY_LEVEL2, TMZF_NONE, "UnlockSubData" );

	#ifdef REPORT_LOCK_TIME
				double flStart = Plat_FloatTime();
	#endif
				m_pCtx->BindBufferToCtx( m_type, this );
				
				Assert( nActualSize <= (int)( m_dirtyMaxOffset - m_dirtyMinOffset ) );

				glBufferSubDataMaxSize( m_buffGLTarget, m_dirtyMinOffset, nActualSize, pActualData ? pActualData : m_pStaticBuffer );

		#ifdef REPORT_LOCK_TIME
				double flEnd = Plat_FloatTime();
				if ( flEnd - flStart > 5.0 / 1000.0 )
				{
					int nDelta = ( int )( ( flEnd - flStart ) * 1000 );
					if ( nDelta > 2 )
					{
						Msg( "**** " );
					}
					// Msg( "glBufferSubData Time=%d: ( Name=%d BufSize=%d ) Target=%p Offset=%d Size=%d\n", nDelta, m_nHandle, m_nSize, m_buffGLTarget, m_dirtyMinOffset, m_dirtyMaxOffset - m_dirtyMinOffset );
				}
	#endif		
			}
		}

		m_pStaticBuffer = NULL;
	}
	else if ( m_bPseudo )
	{
		if ( pActualData )
		{
#if GLM_BUFFER_PERF_ANALYSIS
			CGLMBufferPerfTimer pseudoCopyTimer( s_BufferPerfStats.m_PseudoCopyTime );
			++s_BufferPerfStats.m_nPseudoCopyCalls;
			s_BufferPerfStats.m_nPseudoCopyBytes += nActualSize;
#endif
			memcpy( m_pLastMappedAddress, pActualData, nActualSize );
		}

#if GL_ENABLE_UNLOCK_BUFFER_OVERWRITE_DETECTION
		uint nProtectOfs = m_LockParams.m_nOffset & 4095;
		uint nProtectEnd = ( m_LockParams.m_nOffset + m_LockParams.m_nSize + 4095 ) & ~4095;
		uint nProtectSize = nProtectEnd - nProtectOfs;

		DWORD nOldProtect;
		BOOL bResult = VirtualProtect( m_pActualPseudoBuf + nProtectOfs, nProtectSize, PAGE_READONLY, &nOldProtect );
		if ( !bResult )
		{
			Error( "VirtualProtect() failed!\n" );
		}
#endif
	}
	else
	{
		tmZone( TELEMETRY_LEVEL2, TMZF_NONE, "UnlockUnmap" );

		if ( pActualData )
		{
			memcpy( m_pLastMappedAddress, pActualData, nActualSize );
		}

		m_pCtx->BindBufferToCtx( m_type, this );

		Assert( nActualSize <= (int)( m_dirtyMaxOffset - m_dirtyMinOffset ) );

		// time to do explicit flush (currently m_bEnableExplicitFlush is always true)
		if ( m_bEnableExplicitFlush )
		{
			FlushRange( m_dirtyMinOffset, nActualSize );
		}
		
		// clear dirty range no matter what
		m_dirtyMinOffset = m_dirtyMaxOffset = 0;								// adjust/grow on lock, clear on unlock

#ifdef REPORT_LOCK_TIME
		double flStart = Plat_FloatTime();
#endif

		gGL->glUnmapBuffer( m_buffGLTarget );

#ifdef REPORT_LOCK_TIME
		double flEnd = Plat_FloatTime();
		if ( flEnd - flStart > 5.0 / 1000.0 )
		{
			int nDelta = ( int )( ( flEnd - flStart ) * 1000 );
			if ( nDelta > 2 )
			{
				Msg( "**** " );
			}
			Msg( "glUnmapBuffer Time=%d: ( Name=%d BufSize=%d ) Target=%p\n", nDelta, m_nHandle, m_nSize, m_buffGLTarget );
		}
#endif		
	}

	m_bMapped = false;
}

void CGLMBuffer::FlushPendingPersistentRange()
{
	if ( !m_bUsingPersistentBuffer || !m_bPendingPersistentFlush )
		return;

	// Bind the slot directly and update the context mirror (cannot route
	// through BindBufferToCtx - it calls GetHandle, which calls back here).
	CPersistentBuffer *pSlot = m_pCtx->GetPersistentBuffer( m_nPersistentBufferSlot, m_type );
	const GLuint nHandle = pSlot->GetHandle();
	gGL->glBindBuffer( m_buffGLTarget, nHandle );
	m_pCtx->m_nBoundGLBuffer[ m_type ] = nHandle;
	gGL->glFlushMappedBufferRange( m_buffGLTarget,
		m_nPendingPersistentFlushStart,
		m_nPendingPersistentFlushEnd - m_nPendingPersistentFlushStart );
	m_bPendingPersistentFlush = false;
}

GLuint CGLMBuffer::GetHandle()
{ 
	// The data was appended to the ring slot that was current at lock time;
	// bind that slot's buffer so later-frame draws of the locked data (which
	// D3D9 permits until the buffer is re-locked) read the right memory.  The
	// slot stays resident until the ring wraps back to it two frames later.
	if ( m_bUsingPersistentBuffer )
	{
		CPersistentBuffer *pSlot = m_pCtx->GetPersistentBuffer( m_nPersistentBufferSlot, m_type );
		pSlot->MarkReferenced();

		// Flush the write range accumulated across this frame's unlocks
		// right before the buffer is handed to a draw.  This is the single
		// choke point every draw path (attrib setup, index bind) passes
		// through, so the pending range is always visible before the GPU
		// reads the data - one flush per drawn buffer per frame instead of
		// one per unlock.  (FlushDrawStates also calls
		// FlushPendingPersistentRange for the NOOVERWRITE-append case where
		// the attrib enumeration is skipped and GetHandle never runs.)
		FlushPendingPersistentRange();
		return pSlot->GetHandle();
	}
	return m_nHandle; 
}

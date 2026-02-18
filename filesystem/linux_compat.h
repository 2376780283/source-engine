//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Cross-platform Linux/Android compatibility layer
//
// $NoKeywords: $
//=============================================================================//
#ifndef LINUX_COMPAT_H
#define LINUX_COMPAT_H

#if defined(LINUX) || defined(ANDROID)

// Android compatibility: some systems may not have posix_fadvise
#include <sys/types.h>
#include <unistd.h>

#ifdef ANDROID
	// Android NDK might not have posix_fadvise in all versions
	// Define fallback if needed
	#ifndef POSIX_FADV_SEQUENTIAL
	#define POSIX_FADV_SEQUENTIAL 2
	#endif
	#ifndef POSIX_FADV_NORMAL  
	#define POSIX_FADV_NORMAL 0
	#endif
	
	// Safe wrapper for posix_fadvise (no-op if not available)
	inline int safe_posix_fadvise(int fd, off_t offset, off_t len, int advice)
	{
		#if defined(__ANDROID_API__) && __ANDROID_API__ >= 18
			return posix_fadvise(fd, offset, len, advice);
		#else
			// Gracefully ignore on older Android
			return 0;
		#endif
	}
	
	// Safe wrapper for fileno_unlocked (Android compatibility)
	inline int safe_fileno(FILE *stream)
	{
		#ifdef fileno_unlocked
			return fileno_unlocked(stream);
		#else
			return fileno(stream);
		#endif
	}
#else
	// Linux: use standard functions directly
	#define safe_posix_fadvise(fd, offset, len, advice) posix_fadvise(fd, offset, len, advice)
	#define safe_fileno(stream) fileno_unlocked(stream)
#endif

// fstat wrapper for consistency
inline int safe_fstat(int fd, struct stat *buf)
{
	return fstat(fd, buf);
}

// lstat wrapper 
inline int safe_lstat(const char *path, struct stat *buf)
{
	return lstat(path, buf);
}

#endif // LINUX || ANDROID

#endif // LINUX_COMPAT_H

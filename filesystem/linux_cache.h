//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Fast filename caching for Linux to reduce case-insensitive lookups
//
// $NoKeywords: $
//=============================================================================//
#ifndef LINUX_CACHE_H
#define LINUX_CACHE_H

// NOTE: Filename caching disabled due to initialization timing issues
// The core optimizations (fstat + posix_fadvise) provide sufficient improvement
// This file kept for reference if re-enabling in future

#endif // LINUX_CACHE_H

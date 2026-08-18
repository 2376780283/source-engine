//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#if defined( _WIN32 ) && !defined( _X360 ) && !defined( DX_TO_GL_ABSTRACTION )
#include <windows.h>
#include "../dx9sdk/include/d3d9types.h"
#endif
#include "bitmap/imageformat.h"
#include "basetypes.h"
#include "tier0/dbg.h"
#include <memory.h>
#include "nvtc.h"
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "tier1/utlmemory.h"
#include "tier1/strtools.h"
#include "mathlib/compressed_vector.h"

// Should be last include
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Various important function types for each color format
//-----------------------------------------------------------------------------
static const ImageFormatInfo_t g_ImageFormatInfo[] =
{
	{ "UNKNOWN",					0, 0, 0, 0, 0, false },			// IMAGE_FORMAT_UNKNOWN,
	{ "RGBA8888",					4, 8, 8, 8, 8, false },			// IMAGE_FORMAT_RGBA8888,
	{ "ABGR8888",					4, 8, 8, 8, 8, false },			// IMAGE_FORMAT_ABGR8888, 
	{ "RGB888",						3, 8, 8, 8, 0, false },			// IMAGE_FORMAT_RGB888,
	{ "BGR888",						3, 8, 8, 8, 0, false },			// IMAGE_FORMAT_BGR888,
	{ "RGB565",						2, 5, 6, 5, 0, false },			// IMAGE_FORMAT_RGB565, 
	{ "I8",							1, 0, 0, 0, 0, false },			// IMAGE_FORMAT_I8,
	{ "IA88",						2, 0, 0, 0, 8, false },			// IMAGE_FORMAT_IA88
	{ "P8",							1, 0, 0, 0, 0, false },			// IMAGE_FORMAT_P8
	{ "A8",							1, 0, 0, 0, 8, false },			// IMAGE_FORMAT_A8
	{ "RGB888_BLUESCREEN",			3, 8, 8, 8, 0, false },			// IMAGE_FORMAT_RGB888_BLUESCREEN
	{ "BGR888_BLUESCREEN",			3, 8, 8, 8, 0, false },			// IMAGE_FORMAT_BGR888_BLUESCREEN
	{ "ARGB8888",					4, 8, 8, 8, 8, false },			// IMAGE_FORMAT_ARGB8888
	{ "BGRA8888",					4, 8, 8, 8, 8, false },			// IMAGE_FORMAT_BGRA8888
	{ "DXT1",						0, 0, 0, 0, 0, true },			// IMAGE_FORMAT_DXT1
	{ "DXT3",						0, 0, 0, 0, 8, true },			// IMAGE_FORMAT_DXT3
	{ "DXT5",						0, 0, 0, 0, 8, true },			// IMAGE_FORMAT_DXT5
	{ "BGRX8888",					4, 8, 8, 8, 0, false },			// IMAGE_FORMAT_BGRX8888
	{ "BGR565",						2, 5, 6, 5, 0, false },			// IMAGE_FORMAT_BGR565
	{ "BGRX5551",					2, 5, 5, 5, 0, false },			// IMAGE_FORMAT_BGRX5551
	{ "BGRA4444",					2, 4, 4, 4, 4, false },			// IMAGE_FORMAT_BGRA4444
	{ "DXT1_ONEBITALPHA",			0, 0, 0, 0, 0, true },			// IMAGE_FORMAT_DXT1_ONEBITALPHA
	{ "BGRA5551",					2, 5, 5, 5, 1, false },			// IMAGE_FORMAT_BGRA5551
	{ "UV88",						2, 8, 8, 0, 0, false },			// IMAGE_FORMAT_UV88
	{ "UVWQ8888",					4, 8, 8, 8, 8, false },			// IMAGE_FORMAT_UVWQ8899
	{ "RGBA16161616F",				8, 16, 16, 16, 16, false },		// IMAGE_FORMAT_RGBA16161616F
	{ "RGBA16161616",				8, 16, 16, 16, 16, false },		// IMAGE_FORMAT_RGBA16161616
	{ "IMAGE_FORMAT_UVLX8888",	    4, 8, 8, 8, 8, false },			// IMAGE_FORMAT_UVLX8899
	{ "IMAGE_FORMAT_R32F",			4, 32, 0, 0, 0, false },		// IMAGE_FORMAT_R32F
	{ "IMAGE_FORMAT_RGB323232F",	12, 32, 32, 32, 0, false },		// IMAGE_FORMAT_RGB323232F
	{ "IMAGE_FORMAT_RGBA32323232F",	16, 32, 32, 32, 32, false },	// IMAGE_FORMAT_RGBA32323232F

	// Vendor-dependent depth formats used for shadow depth mapping
	{ "NV_DST16",					2, 16, 0, 0, 0, false },		// IMAGE_FORMAT_NV_DST16
	{ "NV_DST24",					4, 24, 0, 0, 0, false },		// IMAGE_FORMAT_NV_DST24
	{ "NV_INTZ",					4,  8, 8, 8, 8, false },		// IMAGE_FORMAT_NV_INTZ
	{ "NV_RAWZ",					4, 24, 0, 0, 0, false },		// IMAGE_FORMAT_NV_RAWZ
	{ "ATI_DST16",					2, 16, 0, 0, 0, false },		// IMAGE_FORMAT_ATI_DST16
	{ "ATI_DST24",					4, 24, 0, 0, 0, false },		// IMAGE_FORMAT_ATI_DST24
	{ "NV_NULL",					4,  8, 8, 8, 8, false },		// IMAGE_FORMAT_NV_NULL

	// Vendor-dependent compressed formats typically used for normal map compression
	{ "ATI1N",						0, 0, 0, 0, 0, true },			// IMAGE_FORMAT_ATI1N
	{ "ATI2N",						0, 0, 0, 0, 0, true },			// IMAGE_FORMAT_ATI2N

#ifdef _X360
	{ "X360_DST16",					2, 16, 0, 0, 0, false },		// IMAGE_FORMAT_X360_DST16
	{ "X360_DST24",					4, 24, 0, 0, 0, false },		// IMAGE_FORMAT_X360_DST24
	{ "X360_DST24F",				4, 24, 0, 0, 0, false },		// IMAGE_FORMAT_X360_DST24F
	{ "LINEAR_BGRX8888",			4, 8, 8, 8, 8, false },			// IMAGE_FORMAT_LINEAR_BGRX8888
	{ "LINEAR_RGBA8888",			4, 8, 8, 8, 8, false },			// IMAGE_FORMAT_LINEAR_RGBA8888
	{ "LINEAR_ABGR8888",			4, 8, 8, 8, 8, false },			// IMAGE_FORMAT_LINEAR_ABGR8888
	{ "LINEAR_ARGB8888",			4, 8, 8, 8, 8, false },			// IMAGE_FORMAT_LINEAR_ARGB8888
	{ "LINEAR_BGRA8888",			4, 8, 8, 8, 8, false },			// IMAGE_FORMAT_LINEAR_BGRA8888
	{ "LINEAR_RGB888",				3, 8, 8, 8, 0, false },			// IMAGE_FORMAT_LINEAR_RGB888
	{ "LINEAR_BGR888",				3, 8, 8, 8, 0, false },			// IMAGE_FORMAT_LINEAR_BGR888
	{ "LINEAR_BGRX5551",			2, 5, 5, 5, 0, false },			// IMAGE_FORMAT_LINEAR_BGRX5551
	{ "LINEAR_I8",					1, 0, 0, 0, 0, false },			// IMAGE_FORMAT_LINEAR_I8
	{ "LINEAR_RGBA16161616",		8, 16, 16, 16, 16, false },		// IMAGE_FORMAT_LINEAR_RGBA16161616

	{ "LE_BGRX8888",				4, 8, 8, 8, 8, false },			// IMAGE_FORMAT_LE_BGRX8888
	{ "LE_BGRA8888",				4, 8, 8, 8, 8, false },			// IMAGE_FORMAT_LE_BGRA8888
#endif

	{ "DXT1_RUNTIME",				0, 0, 0, 0, 0, true, },			// IMAGE_FORMAT_DXT1_RUNTIME
	{ "DXT5_RUNTIME",				0, 0, 0, 0, 8, true, },			// IMAGE_FORMAT_DXT5_RUNTIME

	{ "ASTC4x4",					0, 0, 0, 0, 0, true },			// IMAGE_FORMAT_ASTC4x4
	{ "ASTC4x4_HDR",				0, 0, 0, 0, 0, true },			// IMAGE_FORMAT_ASTC4x4_HDR

	// 2D ASTC LDR block sizes
	{ "ASTC5x4",					0, 0, 0, 0, 0, true },
	{ "ASTC5x5",					0, 0, 0, 0, 0, true },
	{ "ASTC6x5",					0, 0, 0, 0, 0, true },
	{ "ASTC6x6",					0, 0, 0, 0, 0, true },
	{ "ASTC8x5",					0, 0, 0, 0, 0, true },
	{ "ASTC8x6",					0, 0, 0, 0, 0, true },
	{ "ASTC8x8",					0, 0, 0, 0, 0, true },
	{ "ASTC10x5",					0, 0, 0, 0, 0, true },
	{ "ASTC10x6",					0, 0, 0, 0, 0, true },
	{ "ASTC10x8",					0, 0, 0, 0, 0, true },
	{ "ASTC10x10",					0, 0, 0, 0, 0, true },
	{ "ASTC12x10",					0, 0, 0, 0, 0, true },
	{ "ASTC12x12",					0, 0, 0, 0, 0, true },

	// 3D ASTC LDR block sizes
	{ "ASTC3x3x3",					0, 0, 0, 0, 0, true },
	{ "ASTC4x3x3",					0, 0, 0, 0, 0, true },
	{ "ASTC4x4x3",					0, 0, 0, 0, 0, true },
	{ "ASTC4x4x4",					0, 0, 0, 0, 0, true },
	{ "ASTC5x4x4",					0, 0, 0, 0, 0, true },
	{ "ASTC5x5x4",					0, 0, 0, 0, 0, true },
	{ "ASTC5x5x5",					0, 0, 0, 0, 0, true },
	{ "ASTC6x5x5",					0, 0, 0, 0, 0, true },
	{ "ASTC6x6x5",					0, 0, 0, 0, 0, true },
	{ "ASTC6x6x6",					0, 0, 0, 0, 0, true },

	// 2D ASTC HDR block sizes
	{ "ASTC5x4_HDR",				0, 0, 0, 0, 0, true },
	{ "ASTC5x5_HDR",				0, 0, 0, 0, 0, true },
	{ "ASTC6x5_HDR",				0, 0, 0, 0, 0, true },
	{ "ASTC6x6_HDR",				0, 0, 0, 0, 0, true },
	{ "ASTC8x5_HDR",				0, 0, 0, 0, 0, true },
	{ "ASTC8x6_HDR",				0, 0, 0, 0, 0, true },
	{ "ASTC8x8_HDR",				0, 0, 0, 0, 0, true },
	{ "ASTC10x5_HDR",				0, 0, 0, 0, 0, true },
	{ "ASTC10x6_HDR",				0, 0, 0, 0, 0, true },
	{ "ASTC10x8_HDR",				0, 0, 0, 0, 0, true },
	{ "ASTC10x10_HDR",				0, 0, 0, 0, 0, true },
	{ "ASTC12x10_HDR",				0, 0, 0, 0, 0, true },
	{ "ASTC12x12_HDR",				0, 0, 0, 0, 0, true },

	// 3D ASTC HDR block sizes
	{ "ASTC3x3x3_HDR",				0, 0, 0, 0, 0, true },
	{ "ASTC4x3x3_HDR",				0, 0, 0, 0, 0, true },
	{ "ASTC4x4x3_HDR",				0, 0, 0, 0, 0, true },
	{ "ASTC4x4x4_HDR",				0, 0, 0, 0, 0, true },
	{ "ASTC5x4x4_HDR",				0, 0, 0, 0, 0, true },
	{ "ASTC5x5x4_HDR",				0, 0, 0, 0, 0, true },
	{ "ASTC5x5x5_HDR",				0, 0, 0, 0, 0, true },
	{ "ASTC6x5x5_HDR",				0, 0, 0, 0, 0, true },
	{ "ASTC6x6x5_HDR",				0, 0, 0, 0, 0, true },
	{ "ASTC6x6x6_HDR",				0, 0, 0, 0, 0, true },
};


namespace ImageLoader
{

//-----------------------------------------------------------------------------
// Returns info about each image format
//-----------------------------------------------------------------------------
const ImageFormatInfo_t& ImageFormatInfo( ImageFormat fmt )
{
	Assert( ( NUM_IMAGE_FORMATS + 1 ) == sizeof( g_ImageFormatInfo ) / sizeof( g_ImageFormatInfo[0] ) );
	Assert( unsigned( fmt + 1 ) <= ( NUM_IMAGE_FORMATS ) );
	return g_ImageFormatInfo[ fmt + 1 ];
}

int GetMemRequired( int width, int height, int depth, ImageFormat imageFormat, bool mipmap )
{
	if ( depth <= 0 )
	{
		depth = 1;
	}

	if ( !mipmap )
	{
		// Block compressed formats
		
		if ( IsCompressed( imageFormat ) )
		{
			// All ASTC blocks are 16 bytes regardless of block dimensions.
			// For ASTC, use per-format block dimensions. For DXT/ATI, use 4x4 blocks.
			int blockW = 4, blockH = 4, blockDepth = 1;
			int blockBytes = 16;

			if ( IsASTC( imageFormat ) )
			{
				// ASTC block size lookup (w, h, d=1 for 2D, d=3/4/5/6 for 3D)
				static const struct { ImageFormat fmt; int w; int h; int d; } astcBlocks[] = {
					{ IMAGE_FORMAT_ASTC4x4, 4, 4, 1 },
					{ IMAGE_FORMAT_ASTC5x4, 5, 4, 1 }, { IMAGE_FORMAT_ASTC5x5, 5, 5, 1 },
					{ IMAGE_FORMAT_ASTC6x5, 6, 5, 1 }, { IMAGE_FORMAT_ASTC6x6, 6, 6, 1 },
					{ IMAGE_FORMAT_ASTC8x5, 8, 5, 1 }, { IMAGE_FORMAT_ASTC8x6, 8, 6, 1 }, { IMAGE_FORMAT_ASTC8x8, 8, 8, 1 },
					{ IMAGE_FORMAT_ASTC10x5, 10, 5, 1 }, { IMAGE_FORMAT_ASTC10x6, 10, 6, 1 },
					{ IMAGE_FORMAT_ASTC10x8, 10, 8, 1 }, { IMAGE_FORMAT_ASTC10x10, 10, 10, 1 },
					{ IMAGE_FORMAT_ASTC12x10, 12, 10, 1 }, { IMAGE_FORMAT_ASTC12x12, 12, 12, 1 },
					{ IMAGE_FORMAT_ASTC3x3x3, 3, 3, 3 }, { IMAGE_FORMAT_ASTC4x3x3, 4, 3, 3 },
					{ IMAGE_FORMAT_ASTC4x4x3, 4, 4, 3 }, { IMAGE_FORMAT_ASTC4x4x4, 4, 4, 4 },
					{ IMAGE_FORMAT_ASTC5x4x4, 5, 4, 4 }, { IMAGE_FORMAT_ASTC5x5x4, 5, 5, 4 },
					{ IMAGE_FORMAT_ASTC5x5x5, 5, 5, 5 }, { IMAGE_FORMAT_ASTC6x5x5, 6, 5, 5 },
					{ IMAGE_FORMAT_ASTC6x6x5, 6, 6, 5 }, { IMAGE_FORMAT_ASTC6x6x6, 6, 6, 6 },
				};
				// HDR variants share the same block dimensions as their LDR counterparts
				for ( auto &e : astcBlocks )
				{
					if ( e.fmt == imageFormat )
					{
						blockW = e.w;
						blockH = e.h;
						blockDepth = e.d;
						break;
					}
				}
				// Check HDR variants by subtracting the HDR offset
				if ( blockW == 4 && blockH == 4 && imageFormat != IMAGE_FORMAT_ASTC4x4 )
				{
					// HDR formats are 23 enum slots after their LDR equivalents
					// (except ASTC4x4_HDR at +1, which already mapped via default blockW=4 above)
					int ldrFmt = (int)imageFormat - 23;
					if ( ldrFmt >= (int)IMAGE_FORMAT_ASTC5x4 && ldrFmt <= (int)IMAGE_FORMAT_ASTC6x6x6 )
					{
						for ( auto &e : astcBlocks )
						{
							if ( (int)e.fmt == ldrFmt )
							{
								blockW = e.w;
								blockH = e.h;
								blockDepth = e.d;
								break;
							}
						}
					}
				}
				if ( depth < blockDepth && depth > 0 )
					depth = blockDepth;
				blockBytes = 16;
			}
			else
			{
				// DXT/ATI formats use 4x4 blocks
				switch ( imageFormat )
				{
				case IMAGE_FORMAT_DXT1:
				case IMAGE_FORMAT_DXT1_RUNTIME:
				case IMAGE_FORMAT_ATI1N:
					blockBytes = 8;
					break;
				default:
					blockBytes = 16;
					break;
				}
			}

			if ( width < blockW && width > 0 )
				width = blockW;
			if ( height < blockH && height > 0 )
				height = blockH;

			int blocksW = (width + blockW - 1) / blockW;
			int blocksH = (height + blockH - 1) / blockH;
			int blocksD = (depth + blockDepth - 1) / blockDepth;
			return blocksW * blocksH * blocksD * blockBytes;
		}

		return width * height * depth * SizeInBytes( imageFormat );
	}

	// Mipmap version
	int memSize = 0;
	while ( 1 )
	{
		memSize += GetMemRequired( width, height, depth, imageFormat, false );
		if ( width == 1 && height == 1 && depth == 1 )
		{
			break;
		}
		width >>= 1;
		height >>= 1;
		depth >>= 1;
		if ( width < 1 )
		{
			width = 1;
		}
		if ( height < 1 )
		{
			height = 1;
		}
		if ( depth < 1 )
		{
			depth = 1;
		}
	}

	return memSize;
}

int GetMipMapLevelByteOffset( int width, int height, ImageFormat imageFormat, int skipMipLevels )
{
	int offset = 0;

	while( skipMipLevels > 0 )
	{
		offset += width * height * SizeInBytes(imageFormat);
		if( width == 1 && height == 1 )
		{
			break;
		}
		width >>= 1;
		height >>= 1;
		if( width < 1 )
		{
			width = 1;
		}
		if( height < 1 )
		{
			height = 1;
		}
		skipMipLevels--;
	}
	return offset;
}

void GetMipMapLevelDimensions( int *width, int *height, int skipMipLevels )
{
	while( skipMipLevels > 0 )
	{
		if( *width == 1 && *height == 1 )
		{
			break;
		}
		*width >>= 1;
		*height >>= 1;
		if( *width < 1 )
		{
			*width = 1;
		}
		if( *height < 1 )
		{
			*height = 1;
		}
		skipMipLevels--;
	}
}

int GetNumMipMapLevels( int width, int height, int depth )
{
	if ( depth <= 0 )
	{
		depth = 1;
	}

	if( width < 1 || height < 1 || depth < 1 )
		return 0;

	int numMipLevels = 1;
	while( 1 )
	{
		if( width == 1 && height == 1 && depth == 1 )
			break;

		width >>= 1;
		height >>= 1;
		depth >>= 1;
		if( width < 1 )
		{
			width = 1;
		}
		if( height < 1 )
		{
			height = 1;
		}
		if( depth < 1 )
		{
			depth = 1;
		}
		numMipLevels++;
	}
	return numMipLevels;
}

// Turn off warning about FOURCC formats below...
#pragma warning (disable:4063)

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
	((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |   \
	((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))
#endif //defined(MAKEFOURCC)
//-----------------------------------------------------------------------------
// convert back and forth from D3D format to ImageFormat, regardless of
// whether it's supported or not
//-----------------------------------------------------------------------------
ImageFormat D3DFormatToImageFormat( D3DFORMAT format )
{
#if defined( _X360 )
	if ( IS_D3DFORMAT_SRGB( format ) )
	{
		// sanitize the format from possible sRGB state for comparison purposes
		format = MAKE_NON_SRGB_FMT( format );
	}
#endif

	switch ( format )
	{
#ifdef TOGLES
	case D3DFMT_R8G8B8:
		return IMAGE_FORMAT_RGB888;
	case D3DFMT_A8R8G8B8:
		return IMAGE_FORMAT_RGBA8888;
#else
	case D3DFMT_R8G8B8:
		return IMAGE_FORMAT_BGR888;
	case D3DFMT_A8R8G8B8:
		return IMAGE_FORMAT_BGRA8888;
#endif
	case D3DFMT_X8R8G8B8:
		return IMAGE_FORMAT_BGRX8888;
	case D3DFMT_R5G6B5:
		return IMAGE_FORMAT_BGR565;
	case D3DFMT_X1R5G5B5:
		return IMAGE_FORMAT_BGRX5551;
	case D3DFMT_A1R5G5B5:
		return IMAGE_FORMAT_BGRA5551;
	case D3DFMT_A4R4G4B4:
		return IMAGE_FORMAT_BGRA4444;
	case D3DFMT_L8:
		return IMAGE_FORMAT_I8;
	case D3DFMT_A8L8:
		return IMAGE_FORMAT_IA88;
	case D3DFMT_A8:
		return IMAGE_FORMAT_A8;
	case D3DFMT_DXT1:
		return IMAGE_FORMAT_DXT1;
	case D3DFMT_DXT3:
		return IMAGE_FORMAT_DXT3;
	case D3DFMT_DXT5:
		return IMAGE_FORMAT_DXT5;
	case D3DFMT_V8U8:
		return IMAGE_FORMAT_UV88;
	case D3DFMT_Q8W8V8U8:
		return IMAGE_FORMAT_UVWQ8888;
	case D3DFMT_X8L8V8U8:
		return IMAGE_FORMAT_UVLX8888;
	case D3DFMT_A16B16G16R16F:
		return IMAGE_FORMAT_RGBA16161616F;
	case D3DFMT_A16B16G16R16:
		return IMAGE_FORMAT_RGBA16161616;
	case D3DFMT_R32F:
		return IMAGE_FORMAT_R32F;
	case D3DFMT_A32B32G32R32F:
		return IMAGE_FORMAT_RGBA32323232F;

	// DST and FOURCC formats mapped back to ImageFormat (for vendor-dependent shadow depth textures)
	case (D3DFORMAT)(MAKEFOURCC('R','A','W','Z')):
		return IMAGE_FORMAT_NV_RAWZ;
	case (D3DFORMAT)(MAKEFOURCC('I','N','T','Z')):
		return IMAGE_FORMAT_NV_INTZ;
	case (D3DFORMAT)(MAKEFOURCC('N','U','L','L')):
		return IMAGE_FORMAT_NV_NULL;
	case D3DFMT_D16:
#if !defined( _X360 )
		return IMAGE_FORMAT_NV_DST16;
#else
		return IMAGE_FORMAT_X360_DST16;
#endif
	case D3DFMT_D24S8:
#if !defined( _X360 )
		return IMAGE_FORMAT_NV_DST24;
#else
		return IMAGE_FORMAT_X360_DST24;
#endif
	case (D3DFORMAT)(MAKEFOURCC('D','F','1','6')):
		return IMAGE_FORMAT_ATI_DST16;
	case (D3DFORMAT)(MAKEFOURCC('D','F','2','4')):
		return IMAGE_FORMAT_ATI_DST24;

	// ATIxN FOURCC formats mapped back to ImageFormat
	case (D3DFORMAT)(MAKEFOURCC('A','T','I','1')):
		return IMAGE_FORMAT_ATI1N;
	case (D3DFORMAT)(MAKEFOURCC('A','T','I','2')):
		return IMAGE_FORMAT_ATI2N;

#if defined( _X360 )
	case D3DFMT_LIN_A8R8G8B8:
		return IMAGE_FORMAT_LINEAR_BGRA8888;
	case D3DFMT_LIN_X8R8G8B8:
		return IMAGE_FORMAT_LINEAR_BGRX8888;
	case D3DFMT_LIN_X1R5G5B5:
		return IMAGE_FORMAT_LINEAR_BGRX5551;
	case D3DFMT_LIN_L8:
		return IMAGE_FORMAT_LINEAR_I8;
	case D3DFMT_LIN_A16B16G16R16:
		return IMAGE_FORMAT_LINEAR_RGBA16161616;

	case D3DFMT_LE_X8R8G8B8:
		return IMAGE_FORMAT_LE_BGRX8888;

	case D3DFMT_LE_A8R8G8B8:
		return IMAGE_FORMAT_LE_BGRA8888;

	case D3DFMT_D24FS8:
		return IMAGE_FORMAT_X360_DST24F;
#endif

	// ASTC format (for ToGL(ES) on ARM Mali)
	case (D3DFORMAT)(MAKEFOURCC('A','S','T','4')):
		return IMAGE_FORMAT_ASTC4x4;
	case (D3DFORMAT)(MAKEFOURCC('A','S','H','4')):
		return IMAGE_FORMAT_ASTC4x4_HDR;

	// 2D ASTC LDR
	case D3DFMT_ASTC5x4:   return IMAGE_FORMAT_ASTC5x4;
	case D3DFMT_ASTC5x5:   return IMAGE_FORMAT_ASTC5x5;
	case D3DFMT_ASTC6x5:   return IMAGE_FORMAT_ASTC6x5;
	case D3DFMT_ASTC6x6:   return IMAGE_FORMAT_ASTC6x6;
	case D3DFMT_ASTC8x5:   return IMAGE_FORMAT_ASTC8x5;
	case D3DFMT_ASTC8x6:   return IMAGE_FORMAT_ASTC8x6;
	case D3DFMT_ASTC8x8:   return IMAGE_FORMAT_ASTC8x8;
	case D3DFMT_ASTC10x5:  return IMAGE_FORMAT_ASTC10x5;
	case D3DFMT_ASTC10x6:  return IMAGE_FORMAT_ASTC10x6;
	case D3DFMT_ASTC10x8:  return IMAGE_FORMAT_ASTC10x8;
	case D3DFMT_ASTC10x10: return IMAGE_FORMAT_ASTC10x10;
	case D3DFMT_ASTC12x10: return IMAGE_FORMAT_ASTC12x10;
	case D3DFMT_ASTC12x12: return IMAGE_FORMAT_ASTC12x12;

	// 3D ASTC LDR
	case D3DFMT_ASTC3x3x3: return IMAGE_FORMAT_ASTC3x3x3;
	case D3DFMT_ASTC4x3x3: return IMAGE_FORMAT_ASTC4x3x3;
	case D3DFMT_ASTC4x4x3: return IMAGE_FORMAT_ASTC4x4x3;
	case D3DFMT_ASTC4x4x4: return IMAGE_FORMAT_ASTC4x4x4;
	case D3DFMT_ASTC5x4x4: return IMAGE_FORMAT_ASTC5x4x4;
	case D3DFMT_ASTC5x5x4: return IMAGE_FORMAT_ASTC5x5x4;
	case D3DFMT_ASTC5x5x5: return IMAGE_FORMAT_ASTC5x5x5;
	case D3DFMT_ASTC6x5x5: return IMAGE_FORMAT_ASTC6x5x5;
	case D3DFMT_ASTC6x6x5: return IMAGE_FORMAT_ASTC6x6x5;
	case D3DFMT_ASTC6x6x6: return IMAGE_FORMAT_ASTC6x6x6;

	// 2D ASTC HDR
	case D3DFMT_ASTC5x4_HDR:   return IMAGE_FORMAT_ASTC5x4_HDR;
	case D3DFMT_ASTC5x5_HDR:   return IMAGE_FORMAT_ASTC5x5_HDR;
	case D3DFMT_ASTC6x5_HDR:   return IMAGE_FORMAT_ASTC6x5_HDR;
	case D3DFMT_ASTC6x6_HDR:   return IMAGE_FORMAT_ASTC6x6_HDR;
	case D3DFMT_ASTC8x5_HDR:   return IMAGE_FORMAT_ASTC8x5_HDR;
	case D3DFMT_ASTC8x6_HDR:   return IMAGE_FORMAT_ASTC8x6_HDR;
	case D3DFMT_ASTC8x8_HDR:   return IMAGE_FORMAT_ASTC8x8_HDR;
	case D3DFMT_ASTC10x5_HDR:  return IMAGE_FORMAT_ASTC10x5_HDR;
	case D3DFMT_ASTC10x6_HDR:  return IMAGE_FORMAT_ASTC10x6_HDR;
	case D3DFMT_ASTC10x8_HDR:  return IMAGE_FORMAT_ASTC10x8_HDR;
	case D3DFMT_ASTC10x10_HDR: return IMAGE_FORMAT_ASTC10x10_HDR;
	case D3DFMT_ASTC12x10_HDR: return IMAGE_FORMAT_ASTC12x10_HDR;
	case D3DFMT_ASTC12x12_HDR: return IMAGE_FORMAT_ASTC12x12_HDR;

	// 3D ASTC HDR
	case D3DFMT_ASTC3x3x3_HDR: return IMAGE_FORMAT_ASTC3x3x3_HDR;
	case D3DFMT_ASTC4x3x3_HDR: return IMAGE_FORMAT_ASTC4x3x3_HDR;
	case D3DFMT_ASTC4x4x3_HDR: return IMAGE_FORMAT_ASTC4x4x3_HDR;
	case D3DFMT_ASTC4x4x4_HDR: return IMAGE_FORMAT_ASTC4x4x4_HDR;
	case D3DFMT_ASTC5x4x4_HDR: return IMAGE_FORMAT_ASTC5x4x4_HDR;
	case D3DFMT_ASTC5x5x4_HDR: return IMAGE_FORMAT_ASTC5x5x4_HDR;
	case D3DFMT_ASTC5x5x5_HDR: return IMAGE_FORMAT_ASTC5x5x5_HDR;
	case D3DFMT_ASTC6x5x5_HDR: return IMAGE_FORMAT_ASTC6x5x5_HDR;
	case D3DFMT_ASTC6x6x5_HDR: return IMAGE_FORMAT_ASTC6x6x5_HDR;
	case D3DFMT_ASTC6x6x6_HDR: return IMAGE_FORMAT_ASTC6x6x6_HDR;

	}

	Assert( 0 );

	return IMAGE_FORMAT_UNKNOWN;
}

D3DFORMAT ImageFormatToD3DFormat( ImageFormat format )
{
	// This doesn't care whether it's supported or not
	switch ( format )
	{
	case IMAGE_FORMAT_BGR888:
#if !defined( _X360 )
		return D3DFMT_R8G8B8;
#else
		return D3DFMT_UNKNOWN;
#endif
	case IMAGE_FORMAT_BGRA8888:
		return D3DFMT_A8R8G8B8;
	case IMAGE_FORMAT_RGB888:
		return D3DFMT_R8G8B8;
	case IMAGE_FORMAT_RGBA8888:
		return D3DFMT_A8R8G8B8;
	case IMAGE_FORMAT_BGRX8888:
		return D3DFMT_X8R8G8B8;
	case IMAGE_FORMAT_BGR565:
		return D3DFMT_R5G6B5;
	case IMAGE_FORMAT_BGRX5551:
		return D3DFMT_X1R5G5B5;
	case IMAGE_FORMAT_BGRA5551:
		return D3DFMT_A1R5G5B5;
	case IMAGE_FORMAT_BGRA4444:
		return D3DFMT_A4R4G4B4;
	case IMAGE_FORMAT_I8:
		return D3DFMT_L8;
	case IMAGE_FORMAT_IA88:
		return D3DFMT_A8L8;
	case IMAGE_FORMAT_A8:
		return D3DFMT_A8;
	case IMAGE_FORMAT_DXT1:
	case IMAGE_FORMAT_DXT1_ONEBITALPHA:
		return D3DFMT_DXT1;
	case IMAGE_FORMAT_DXT3:
		return D3DFMT_DXT3;
	case IMAGE_FORMAT_DXT5:
		return D3DFMT_DXT5;
	case IMAGE_FORMAT_UV88:
		return D3DFMT_V8U8;
	case IMAGE_FORMAT_UVWQ8888:
		return D3DFMT_Q8W8V8U8;
	case IMAGE_FORMAT_UVLX8888:
		return D3DFMT_X8L8V8U8;
	case IMAGE_FORMAT_RGBA16161616F:
		return D3DFMT_A16B16G16R16F;
	case IMAGE_FORMAT_RGBA16161616:
		return D3DFMT_A16B16G16R16;
	case IMAGE_FORMAT_R32F:
		return D3DFMT_R32F;
	case IMAGE_FORMAT_RGBA32323232F:
		return D3DFMT_A32B32G32R32F;

	// ImageFormat mapped to vendor-dependent FOURCC formats (for shadow depth textures)
	case IMAGE_FORMAT_NV_RAWZ:
		return (D3DFORMAT)(MAKEFOURCC('R','A','W','Z'));
	case IMAGE_FORMAT_NV_INTZ:
		return (D3DFORMAT)(MAKEFOURCC('I','N','T','Z'));
	case IMAGE_FORMAT_NV_NULL:
		return (D3DFORMAT)(MAKEFOURCC('N','U','L','L'));
	case IMAGE_FORMAT_NV_DST16:
		return D3DFMT_D16;
	case IMAGE_FORMAT_NV_DST24:
		return D3DFMT_D24S8;
	case IMAGE_FORMAT_ATI_DST16:
		return (D3DFORMAT)(MAKEFOURCC('D','F','1','6'));
	case IMAGE_FORMAT_ATI_DST24:
		return (D3DFORMAT)(MAKEFOURCC('D','F','2','4'));

	// ImageFormats mapped to ATIxN FOURCC
	case IMAGE_FORMAT_ATI1N:
		return (D3DFORMAT)(MAKEFOURCC('A','T','I','1'));
	case IMAGE_FORMAT_ATI2N:
		return (D3DFORMAT)(MAKEFOURCC('A','T','I','2'));

#if defined( _X360 )
	case IMAGE_FORMAT_LINEAR_BGRA8888:
		return D3DFMT_LIN_A8R8G8B8;
	case IMAGE_FORMAT_LINEAR_BGRX8888:
		return D3DFMT_LIN_X8R8G8B8;
	case IMAGE_FORMAT_LINEAR_BGRX5551:
		return D3DFMT_LIN_X1R5G5B5;
	case IMAGE_FORMAT_LINEAR_I8:
		return D3DFMT_LIN_L8;
	case IMAGE_FORMAT_LINEAR_RGBA16161616:
		return D3DFMT_LIN_A16B16G16R16;
	case IMAGE_FORMAT_LE_BGRX8888:
		return D3DFMT_LE_X8R8G8B8;
	case IMAGE_FORMAT_LE_BGRA8888:
		return D3DFMT_LE_A8R8G8B8;
	case IMAGE_FORMAT_X360_DST16:
		return D3DFMT_D16;
	case IMAGE_FORMAT_X360_DST24:
		return D3DFMT_D24S8;
	case IMAGE_FORMAT_X360_DST24F:
		return D3DFMT_D24FS8;
#endif

	case IMAGE_FORMAT_DXT1_RUNTIME:
		return D3DFMT_DXT1;
	case IMAGE_FORMAT_DXT5_RUNTIME:
		return D3DFMT_DXT5;

	// ASTC format (for ToGL(ES) on ARM Mali)
	case IMAGE_FORMAT_ASTC4x4:
		return (D3DFORMAT)(MAKEFOURCC('A','S','T','4'));
	case IMAGE_FORMAT_ASTC4x4_HDR:
		return (D3DFORMAT)(MAKEFOURCC('A','S','H','4'));

	// 2D ASTC LDR
	case IMAGE_FORMAT_ASTC5x4:   return D3DFMT_ASTC5x4;
	case IMAGE_FORMAT_ASTC5x5:   return D3DFMT_ASTC5x5;
	case IMAGE_FORMAT_ASTC6x5:   return D3DFMT_ASTC6x5;
	case IMAGE_FORMAT_ASTC6x6:   return D3DFMT_ASTC6x6;
	case IMAGE_FORMAT_ASTC8x5:   return D3DFMT_ASTC8x5;
	case IMAGE_FORMAT_ASTC8x6:   return D3DFMT_ASTC8x6;
	case IMAGE_FORMAT_ASTC8x8:   return D3DFMT_ASTC8x8;
	case IMAGE_FORMAT_ASTC10x5:  return D3DFMT_ASTC10x5;
	case IMAGE_FORMAT_ASTC10x6:  return D3DFMT_ASTC10x6;
	case IMAGE_FORMAT_ASTC10x8:  return D3DFMT_ASTC10x8;
	case IMAGE_FORMAT_ASTC10x10: return D3DFMT_ASTC10x10;
	case IMAGE_FORMAT_ASTC12x10: return D3DFMT_ASTC12x10;
	case IMAGE_FORMAT_ASTC12x12: return D3DFMT_ASTC12x12;

	// 3D ASTC LDR
	case IMAGE_FORMAT_ASTC3x3x3: return D3DFMT_ASTC3x3x3;
	case IMAGE_FORMAT_ASTC4x3x3: return D3DFMT_ASTC4x3x3;
	case IMAGE_FORMAT_ASTC4x4x3: return D3DFMT_ASTC4x4x3;
	case IMAGE_FORMAT_ASTC4x4x4: return D3DFMT_ASTC4x4x4;
	case IMAGE_FORMAT_ASTC5x4x4: return D3DFMT_ASTC5x4x4;
	case IMAGE_FORMAT_ASTC5x5x4: return D3DFMT_ASTC5x5x4;
	case IMAGE_FORMAT_ASTC5x5x5: return D3DFMT_ASTC5x5x5;
	case IMAGE_FORMAT_ASTC6x5x5: return D3DFMT_ASTC6x5x5;
	case IMAGE_FORMAT_ASTC6x6x5: return D3DFMT_ASTC6x6x5;
	case IMAGE_FORMAT_ASTC6x6x6: return D3DFMT_ASTC6x6x6;

	// 2D ASTC HDR
	case IMAGE_FORMAT_ASTC5x4_HDR:   return D3DFMT_ASTC5x4_HDR;
	case IMAGE_FORMAT_ASTC5x5_HDR:   return D3DFMT_ASTC5x5_HDR;
	case IMAGE_FORMAT_ASTC6x5_HDR:   return D3DFMT_ASTC6x5_HDR;
	case IMAGE_FORMAT_ASTC6x6_HDR:   return D3DFMT_ASTC6x6_HDR;
	case IMAGE_FORMAT_ASTC8x5_HDR:   return D3DFMT_ASTC8x5_HDR;
	case IMAGE_FORMAT_ASTC8x6_HDR:   return D3DFMT_ASTC8x6_HDR;
	case IMAGE_FORMAT_ASTC8x8_HDR:   return D3DFMT_ASTC8x8_HDR;
	case IMAGE_FORMAT_ASTC10x5_HDR:  return D3DFMT_ASTC10x5_HDR;
	case IMAGE_FORMAT_ASTC10x6_HDR:  return D3DFMT_ASTC10x6_HDR;
	case IMAGE_FORMAT_ASTC10x8_HDR:  return D3DFMT_ASTC10x8_HDR;
	case IMAGE_FORMAT_ASTC10x10_HDR: return D3DFMT_ASTC10x10_HDR;
	case IMAGE_FORMAT_ASTC12x10_HDR: return D3DFMT_ASTC12x10_HDR;
	case IMAGE_FORMAT_ASTC12x12_HDR: return D3DFMT_ASTC12x12_HDR;

	// 3D ASTC HDR
	case IMAGE_FORMAT_ASTC3x3x3_HDR: return D3DFMT_ASTC3x3x3_HDR;
	case IMAGE_FORMAT_ASTC4x3x3_HDR: return D3DFMT_ASTC4x3x3_HDR;
	case IMAGE_FORMAT_ASTC4x4x3_HDR: return D3DFMT_ASTC4x4x3_HDR;
	case IMAGE_FORMAT_ASTC4x4x4_HDR: return D3DFMT_ASTC4x4x4_HDR;
	case IMAGE_FORMAT_ASTC5x4x4_HDR: return D3DFMT_ASTC5x4x4_HDR;
	case IMAGE_FORMAT_ASTC5x5x4_HDR: return D3DFMT_ASTC5x5x4_HDR;
	case IMAGE_FORMAT_ASTC5x5x5_HDR: return D3DFMT_ASTC5x5x5_HDR;
	case IMAGE_FORMAT_ASTC6x5x5_HDR: return D3DFMT_ASTC6x5x5_HDR;
	case IMAGE_FORMAT_ASTC6x6x5_HDR: return D3DFMT_ASTC6x6x5_HDR;
	case IMAGE_FORMAT_ASTC6x6x6_HDR: return D3DFMT_ASTC6x6x6_HDR;

	}

	Assert( 0 );

	return D3DFMT_UNKNOWN;
}

#pragma warning (default:4063)

} // ImageLoader namespace ends


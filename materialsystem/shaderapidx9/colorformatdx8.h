//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef COLORFORMATDX8_H
#define COLORFORMATDX8_H

#include <pixelwriter.h>
#include "togl/rendermechanism.h"

// FOURCC formats for ATI shadow depth textures
#define ATIFMT_D16		((D3DFORMAT)(MAKEFOURCC('D','F','1','6')))
#define ATIFMT_D24S8	((D3DFORMAT)(MAKEFOURCC('D','F','2','4')))

// FOURCC formats for ATI2N and ATI1N compressed textures (360 and DX10 parts also do these)
#define ATIFMT_ATI2N ((D3DFORMAT) MAKEFOURCC('A', 'T', 'I', '2'))
#define ATIFMT_ATI1N ((D3DFORMAT) MAKEFOURCC('A', 'T', 'I', '1'))

// FOURCC formats for ASTC compressed textures
#define D3DFMT_ASTC4x4 ((D3DFORMAT)(MAKEFOURCC('A', 'S', 'T', '4')))
#define D3DFMT_ASTC4x4_HDR ((D3DFORMAT)(MAKEFOURCC('A', 'S', 'H', '4')))

// 2D ASTC LDR block sizes
#define D3DFMT_ASTC5x4 ((D3DFORMAT)0x34545342)
#define D3DFMT_ASTC5x5 ((D3DFORMAT)0x34545343)
#define D3DFMT_ASTC6x5 ((D3DFORMAT)0x34545344)
#define D3DFMT_ASTC6x6 ((D3DFORMAT)0x34545345)
#define D3DFMT_ASTC8x5 ((D3DFORMAT)0x34545346)
#define D3DFMT_ASTC8x6 ((D3DFORMAT)0x34545347)
#define D3DFMT_ASTC8x8 ((D3DFORMAT)0x34545348)
#define D3DFMT_ASTC10x5 ((D3DFORMAT)0x34545349)
#define D3DFMT_ASTC10x6 ((D3DFORMAT)0x3454534A)
#define D3DFMT_ASTC10x8 ((D3DFORMAT)0x3454534B)
#define D3DFMT_ASTC10x10 ((D3DFORMAT)0x3454534C)
#define D3DFMT_ASTC12x10 ((D3DFORMAT)0x3454534D)
#define D3DFMT_ASTC12x12 ((D3DFORMAT)0x3454534E)

// 3D ASTC LDR block sizes
#define D3DFMT_ASTC3x3x3 ((D3DFORMAT)0x34545350)
#define D3DFMT_ASTC4x3x3 ((D3DFORMAT)0x34545351)
#define D3DFMT_ASTC4x4x3 ((D3DFORMAT)0x34545352)
#define D3DFMT_ASTC4x4x4 ((D3DFORMAT)0x34545353)
#define D3DFMT_ASTC5x4x4 ((D3DFORMAT)0x34545354)
#define D3DFMT_ASTC5x5x4 ((D3DFORMAT)0x34545355)
#define D3DFMT_ASTC5x5x5 ((D3DFORMAT)0x34545356)
#define D3DFMT_ASTC6x5x5 ((D3DFORMAT)0x34545357)
#define D3DFMT_ASTC6x6x5 ((D3DFORMAT)0x34545358)
#define D3DFMT_ASTC6x6x6 ((D3DFORMAT)0x34545359)

// 2D ASTC HDR block sizes
#define D3DFMT_ASTC5x4_HDR ((D3DFORMAT)0x34485342)
#define D3DFMT_ASTC5x5_HDR ((D3DFORMAT)0x34485343)
#define D3DFMT_ASTC6x5_HDR ((D3DFORMAT)0x34485344)
#define D3DFMT_ASTC6x6_HDR ((D3DFORMAT)0x34485345)
#define D3DFMT_ASTC8x5_HDR ((D3DFORMAT)0x34485346)
#define D3DFMT_ASTC8x6_HDR ((D3DFORMAT)0x34485347)
#define D3DFMT_ASTC8x8_HDR ((D3DFORMAT)0x34485348)
#define D3DFMT_ASTC10x5_HDR ((D3DFORMAT)0x34485349)
#define D3DFMT_ASTC10x6_HDR ((D3DFORMAT)0x3448534A)
#define D3DFMT_ASTC10x8_HDR ((D3DFORMAT)0x3448534B)
#define D3DFMT_ASTC10x10_HDR ((D3DFORMAT)0x3448534C)
#define D3DFMT_ASTC12x10_HDR ((D3DFORMAT)0x3448534D)
#define D3DFMT_ASTC12x12_HDR ((D3DFORMAT)0x3448534E)

// 3D ASTC HDR block sizes
#define D3DFMT_ASTC3x3x3_HDR ((D3DFORMAT)0x34485350)
#define D3DFMT_ASTC4x3x3_HDR ((D3DFORMAT)0x34485351)
#define D3DFMT_ASTC4x4x3_HDR ((D3DFORMAT)0x34485352)
#define D3DFMT_ASTC4x4x4_HDR ((D3DFORMAT)0x34485353)
#define D3DFMT_ASTC5x4x4_HDR ((D3DFORMAT)0x34485354)
#define D3DFMT_ASTC5x5x4_HDR ((D3DFORMAT)0x34485355)
#define D3DFMT_ASTC5x5x5_HDR ((D3DFORMAT)0x34485356)
#define D3DFMT_ASTC6x5x5_HDR ((D3DFORMAT)0x34485357)
#define D3DFMT_ASTC6x6x5_HDR ((D3DFORMAT)0x34485358)
#define D3DFMT_ASTC6x6x6_HDR ((D3DFORMAT)0x34485359)

// FOURCC formats for nVidia shadow depth textures
#define NVFMT_RAWZ		((D3DFORMAT)(MAKEFOURCC('R','A','W','Z')))
#define NVFMT_INTZ		((D3DFORMAT)(MAKEFOURCC('I','N','T','Z')))

// FOURCC format for nVidia null texture format
#define NVFMT_NULL		((D3DFORMAT)(MAKEFOURCC('N','U','L','L')))


//-----------------------------------------------------------------------------
// Finds the nearest supported frame buffer format
//-----------------------------------------------------------------------------
ImageFormat FindNearestSupportedBackBufferFormat( unsigned int displayAdapter, D3DDEVTYPE deviceType,
	ImageFormat displayFormat, ImageFormat backBufferFormat, bool bIsWindowed );

//-----------------------------------------------------------------------------
// Initializes the color format informat; call it every time display mode changes
//-----------------------------------------------------------------------------
void InitializeColorInformation( unsigned int displayAdapter, D3DDEVTYPE deviceType, 
								 ImageFormat displayFormat );

//-----------------------------------------------------------------------------
// Returns true if compressed textures are supported
//-----------------------------------------------------------------------------
bool D3DSupportsCompressedTextures();

//-----------------------------------------------------------------------------
// Returns closest supported format
//-----------------------------------------------------------------------------
ImageFormat FindNearestSupportedFormat( ImageFormat format, bool bIsVertexTexture, bool bIsRenderTarget, bool bFilterableRequired );

//-----------------------------------------------------------------------------
// Finds the nearest supported depth buffer format
//-----------------------------------------------------------------------------
D3DFORMAT FindNearestSupportedDepthFormat( int nAdapter, ImageFormat displayFormat, ImageFormat renderTargetFormat, D3DFORMAT depthFormat );

const char *D3DFormatName( D3DFORMAT d3dFormat );

#endif // COLORFORMATDX8_H

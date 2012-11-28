//-----------------------------------------------------------------------------
// Copyright (c) 2012 GarageGames, LLC
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//-----------------------------------------------------------------------------

#include <d3d11.h>
#include "platform/platform.h"
#include "gfx/D3D11/gfxD3D11EnumTranslate.h"
#include "console/console.h"

//------------------------------------------------------------------------------


DXGI_FORMAT GFXD3D11TextureFormat[GFXFormat_COUNT];
D3D11_PRIMITIVE_TOPOLOGY GFXD3D11PrimType[GFXPT_COUNT];

//------------------------------------------------------------------------------

#define INIT_LOOKUPTABLE( tablearray, enumprefix, type ) \
   for( int i = enumprefix##_FIRST; i < enumprefix##_COUNT; i++ ) \
      tablearray##[i] = (##type##)GFX_UNINIT_VAL;

#define VALIDATE_LOOKUPTABLE( tablearray, enumprefix ) \
   for( int i = enumprefix##_FIRST; i < enumprefix##_COUNT; i++ ) \
      if( (int)tablearray##[i] == GFX_UNINIT_VAL ) \
         Con::warnf( "GFXD3D9EnumTranslate: Unassigned value in " #tablearray ": %i", i ); \
      else if( (int)tablearray##[i] == GFX_UNSUPPORTED_VAL ) \
         Con::warnf( "GFXD3D9EnumTranslate: Unsupported value in " #tablearray ": %i", i );

//------------------------------------------------------------------------------

void GFXD3D11EnumTranslate::init()
{
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
   INIT_LOOKUPTABLE( GFXD3D11TextureFormat, GFXFormat, DXGI_FORMAT );
   GFXD3D11TextureFormat[GFXFormatR8G8B8] = DXGI_FORMAT_UNKNOWN;//Not available
   GFXD3D11TextureFormat[GFXFormatR8G8B8A8] = DXGI_FORMAT_B8G8R8A8_UNORM;
   GFXD3D11TextureFormat[GFXFormatR8G8B8X8] = DXGI_FORMAT_B8G8R8X8_UNORM;
   GFXD3D11TextureFormat[GFXFormatR5G6B5] = DXGI_FORMAT_B5G6R5_UNORM;
   GFXD3D11TextureFormat[GFXFormatR5G5B5A1] = DXGI_FORMAT_B5G5R5A1_UNORM;
   GFXD3D11TextureFormat[GFXFormatR5G5B5X1] = DXGI_FORMAT_UNKNOWN;//Not available
   GFXD3D11TextureFormat[GFXFormatR32F] = DXGI_FORMAT_R32_FLOAT;
   GFXD3D11TextureFormat[GFXFormatA4L4] = DXGI_FORMAT_UNKNOWN;//Not available;
   GFXD3D11TextureFormat[GFXFormatA8L8] = DXGI_FORMAT_R8G8_UNORM;//Note: Use swizzle .rrrg in shader to duplicate red and move green to the alpha components to get D3D9 behavior.
   GFXD3D11TextureFormat[GFXFormatA8] = DXGI_FORMAT_A8_UNORM;
   GFXD3D11TextureFormat[GFXFormatL8] = DXGI_FORMAT_R8_UNORM;//Note: Use .r swizzle in shader to duplicate red to other components to get D3D9 behavior.
   GFXD3D11TextureFormat[GFXFormatDXT1] = DXGI_FORMAT_BC1_UNORM;
   GFXD3D11TextureFormat[GFXFormatDXT2] = DXGI_FORMAT_BC1_UNORM;
   GFXD3D11TextureFormat[GFXFormatDXT3] = DXGI_FORMAT_BC2_UNORM;
   GFXD3D11TextureFormat[GFXFormatDXT4] = DXGI_FORMAT_BC2_UNORM;
   GFXD3D11TextureFormat[GFXFormatDXT5] = DXGI_FORMAT_BC3_UNORM;
   GFXD3D11TextureFormat[GFXFormatR32G32B32A32F] = DXGI_FORMAT_R32G32B32A32_FLOAT;
   GFXD3D11TextureFormat[GFXFormatR16G16B16A16F] = DXGI_FORMAT_R16G16B16A16_FLOAT;
   GFXD3D11TextureFormat[GFXFormatL16] = DXGI_FORMAT_R16_UNORM;//Note: Use .r swizzle in shader to duplicate red to other components to get D3D9 behavior.
   GFXD3D11TextureFormat[GFXFormatR16G16B16A16] = DXGI_FORMAT_R16G16B16A16_FLOAT;
   GFXD3D11TextureFormat[GFXFormatR16G16] = DXGI_FORMAT_R16G16_UNORM;
   GFXD3D11TextureFormat[GFXFormatR16F] = DXGI_FORMAT_R16_FLOAT;
   GFXD3D11TextureFormat[GFXFormatR16G16F] = DXGI_FORMAT_R16G16_FLOAT;
   GFXD3D11TextureFormat[GFXFormatR10G10B10A2] = DXGI_FORMAT_R10G10B10A2_UNORM;//Review
   GFXD3D11TextureFormat[GFXFormatD32] = DXGI_FORMAT_D32_FLOAT;//Review
   GFXD3D11TextureFormat[GFXFormatD24X8] = DXGI_FORMAT_D24_UNORM_S8_UINT;//Review
   GFXD3D11TextureFormat[GFXFormatD24S8] = DXGI_FORMAT_D24_UNORM_S8_UINT;//Review
   GFXD3D11TextureFormat[GFXFormatD24FS8] = DXGI_FORMAT_UNKNOWN;//Not available
   GFXD3D11TextureFormat[GFXFormatD16] = DXGI_FORMAT_D16_UNORM;
   VALIDATE_LOOKUPTABLE( GFXD3D11TextureFormat, GFXFormat);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

   INIT_LOOKUPTABLE( GFXD3D11PrimType, GFXPT, D3D11_PRIMITIVE_TOPOLOGY );
   GFXD3D11PrimType[GFXPointList] = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
   GFXD3D11PrimType[GFXLineList] = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
   GFXD3D11PrimType[GFXLineStrip] = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
   GFXD3D11PrimType[GFXTriangleList] = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
   GFXD3D11PrimType[GFXTriangleStrip] = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
   GFXD3D11PrimType[GFXTriangleFan] = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
   VALIDATE_LOOKUPTABLE( GFXD3D11PrimType, GFXPT );

}


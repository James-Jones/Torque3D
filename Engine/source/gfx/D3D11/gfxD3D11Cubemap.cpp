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

#include "gfx/D3D11/gfxD3D11Cubemap.h"
#include "gfx/gfxTextureManager.h"
#include "gfx/D3D11/gfxD3D11EnumTranslate.h"

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
GFXD3D11Cubemap::GFXD3D11Cubemap()
{
   mCubeTex = NULL;
   mDynamic = false;
   mFaceFormat = GFXFormatR8G8B8A8;
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
GFXD3D11Cubemap::~GFXD3D11Cubemap()
{
   releaseSurfaces();

   if ( mDynamic )
      GFXTextureManager::removeEventDelegate( this, &GFXD3D11Cubemap::_onTextureEvent );
}

//-----------------------------------------------------------------------------
// Release D3D surfaces
//-----------------------------------------------------------------------------
void GFXD3D11Cubemap::releaseSurfaces()
{
   if ( !mCubeTex )
      return;

   mCubeTex->Release();
   mCubeTex = NULL;
}

void GFXD3D11Cubemap::_onTextureEvent( GFXTexCallbackCode code )
{
   // Can this happen?
   if ( !mDynamic ) 
      return;
   
   if ( code == GFXZombify )
      releaseSurfaces();
   else if ( code == GFXResurrect )
      initDynamic( mTexSize );
}

//-----------------------------------------------------------------------------
// Init Static
//-----------------------------------------------------------------------------
void GFXD3D11Cubemap::initStatic( GFXTexHandle *faces )
{
   //if( mCubeTex )
   //   return;

   if( faces )
   {
      AssertFatal( faces[0], "empty texture passed to CubeMap::create" );
   
      GFXD3D11Device *dev = static_cast<GFXD3D11Device *>(GFX);

      ID3D11Device* D3D11Device = dev->getDevice();     
      
      mTexSize = faces[0].getWidth();
      mFaceFormat = faces[0].getFormat();

	D3D11_TEXTURE2D_DESC sTexDesc2D;
	sTexDesc2D.Width				= mTexSize;
	sTexDesc2D.Height				= mTexSize;
	sTexDesc2D.MipLevels			= 1;
	sTexDesc2D.ArraySize			= 6;
	sTexDesc2D.Format				= GFXD3D11TextureFormat[mFaceFormat];
	sTexDesc2D.SampleDesc.Count		= 1;
	sTexDesc2D.SampleDesc.Quality	= 0;
	sTexDesc2D.Usage				= D3D11_USAGE_DEFAULT;
	sTexDesc2D.BindFlags			= D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	sTexDesc2D.CPUAccessFlags		= 0;
	sTexDesc2D.MiscFlags			= D3D11_RESOURCE_MISC_TEXTURECUBE;

	D3D11Device->CreateTexture2D(&sTexDesc2D, NULL, &mCubeTex);

      fillCubeTextures( faces, dev->getDeviceContext() );
//      mCubeTex->GenerateMipSubLevels();
   }
}

void GFXD3D11Cubemap::initStatic( DDSFile *dds )
{
   AssertFatal( dds, "GFXD3D11Cubemap::initStatic - Got null DDS file!" );
   AssertFatal( dds->isCubemap(), "GFXD3D11Cubemap::initStatic - Got non-cubemap DDS file!" );
   AssertFatal( dds->mSurfaces.size() == 6, "GFXD3D11Cubemap::initStatic - DDS has less than 6 surfaces!" );

   GFXD3D11Device *dev = static_cast<GFXD3D11Device *>(GFX);


   ID3D11Device* D3D11Device = dev->getDevice();
   
   mTexSize = dds->getWidth();
   mFaceFormat = dds->getFormat();
   U32 levels = dds->getMipLevels();

	D3D11_SUBRESOURCE_DATA *pSubresource = new D3D11_SUBRESOURCE_DATA [levels * 6];


   for( U32 i=0; i < 6; i++ )
   {
      if ( !dds->mSurfaces[i] )
      {
         // TODO: The DDS can skip surfaces, but i'm unsure what i should
         // do here when creating the cubemap.  Ignore it for now.
         continue;
      }

      // Now loop thru the mip levels!
      for( U32 l = 0; l < levels; l++ )
      {
			U32 uiSubResourceOffset=l+(i*levels); 

			pSubresource[uiSubResourceOffset].pSysMem = dds->mSurfaces[i]->mMips[l];
			pSubresource[uiSubResourceOffset].SysMemPitch = dds->getSurfacePitch(l);
      }
   }

	D3D11_TEXTURE2D_DESC sTexDesc2D;
	sTexDesc2D.Width				= mTexSize;
    sTexDesc2D.Height				= mTexSize;
    sTexDesc2D.MipLevels			= levels;
    sTexDesc2D.ArraySize			= 6;
    sTexDesc2D.Format				= GFXD3D11TextureFormat[mFaceFormat];
    sTexDesc2D.SampleDesc.Count		= 1;
	sTexDesc2D.SampleDesc.Quality	= 0;
    sTexDesc2D.Usage				= D3D11_USAGE_DEFAULT;
    sTexDesc2D.BindFlags			= D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    sTexDesc2D.CPUAccessFlags		= 0;
    sTexDesc2D.MiscFlags			= D3D11_RESOURCE_MISC_TEXTURECUBE;

	D3D11Device->CreateTexture2D(&sTexDesc2D, pSubresource, &mCubeTex);
}

//-----------------------------------------------------------------------------
// Init Dynamic
//-----------------------------------------------------------------------------
void GFXD3D11Cubemap::initDynamic( U32 texSize, GFXFormat faceFormat )
{
   if ( mCubeTex )
      return;

   if ( !mDynamic )
      GFXTextureManager::addEventDelegate( this, &GFXD3D11Cubemap::_onTextureEvent );

   mDynamic = true;
   mTexSize = texSize;
   mFaceFormat = faceFormat;
   
   ID3D11Device* D3D11Device = reinterpret_cast<GFXD3D11Device *>(GFX)->getDevice();

	D3D11_TEXTURE2D_DESC sTexDesc2D;
	sTexDesc2D.Width				= texSize;
    sTexDesc2D.Height				= texSize;
    sTexDesc2D.MipLevels			= 1;
    sTexDesc2D.ArraySize			= 6;
    sTexDesc2D.Format				= GFXD3D11TextureFormat[faceFormat];
    sTexDesc2D.SampleDesc.Count		= 1;
	sTexDesc2D.SampleDesc.Quality	= 0;
    sTexDesc2D.Usage				= D3D11_USAGE_DEFAULT;
    sTexDesc2D.BindFlags			= D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    sTexDesc2D.CPUAccessFlags		= 0;
    sTexDesc2D.MiscFlags			= D3D11_RESOURCE_MISC_TEXTURECUBE;

	D3D11Device->CreateTexture2D(&sTexDesc2D, NULL, &mCubeTex);

#if 0
    // Create the 6-face render target view
    D3D11_RENDER_TARGET_VIEW_DESC DescRT;
    DescRT.Format = sTexDesc2D.Format;
    DescRT.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
    DescRT.Texture2DArray.FirstArraySlice = 0;
    DescRT.Texture2DArray.ArraySize = 6;
    DescRT.Texture2DArray.MipSlice = 0;
    V_RETURN( D3D11Device->CreateRenderTargetView( mCubeTex, &DescRT, &mRTV ) );

#if 0
    // Create the one-face render target views
    DescRT.Texture2DArray.ArraySize = 1;
    for( int i = 0; i < 6; ++i )
    {
        DescRT.Texture2DArray.FirstArraySlice = i;
        V_RETURN( pd3dDevice->CreateRenderTargetView( g_pEnvMap, &DescRT, &g_apEnvMapOneRTV[i] ) );
    }
#endif

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = dstex.Format;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    SRVDesc.TextureCube.MipLevels = MIPLEVELS;
    SRVDesc.TextureCube.MostDetailedMip = 0;
    V_RETURN( pd3dDevice->CreateShaderResourceView( mCubeTex, &SRVDesc, &mSRV ) );
#endif
}

//-----------------------------------------------------------------------------
// Fills in face textures of cube map from existing textures
//-----------------------------------------------------------------------------
void GFXD3D11Cubemap::fillCubeTextures( GFXTexHandle *faces, ID3D11DeviceContext* D3DDeviceContext )
{
   for( U32 i=0; i<6; i++ )
   {
	//GFXD3D11TextureObject *texObj = dynamic_cast<GFXD3D11TextureObject*>( (GFXTextureObject*)faces[i] );

	//texObj->get2DTex()->GetSurfaceLevel( 0, &inSurf )

	//GPU-GPU D3DDeviceContext->CopyResource
	//CPU-GPU D3DDeviceContext->UpdateSubresource
   }
}

//-----------------------------------------------------------------------------
// Set the cubemap to the specified texture unit num
//-----------------------------------------------------------------------------
void GFXD3D11Cubemap::setToTexUnit( U32 tuNum )
{
   //static_cast<GFXD3D11Device *>(GFX)->getDevice()->SetTexture( tuNum, mCubeTex );
}

void GFXD3D11Cubemap::zombify()
{
   // Static cubemaps are handled by D3D
   if( mDynamic )
      releaseSurfaces();
}

void GFXD3D11Cubemap::resurrect()
{
   // Static cubemaps are handled by D3D
   if( mDynamic )
      initDynamic( mTexSize, mFaceFormat );
}

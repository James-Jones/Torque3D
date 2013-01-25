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

#include "platform/platform.h"
#include "gfx/D3D11/GFXD3D11Device.h"

#include "console/console.h"
#include "core/strings/stringFunctions.h"
#include "gfx/gfxCubemap.h"
#include "gfx/screenshot.h"
#include "gfx/gfxPrimitiveBuffer.h"
#include "gfx/gfxCardProfile.h"
#include "gfx/gfxTextureManager.h"
#include "gfx/bitmap/gBitmap.h"
#include "core/util/safeDelete.h"
#include "windowManager/win32/win32Window.h"

#include "gfx/D3D11/gfxD3D11QueryFence.h"
#include "gfx/D3D11/gfxD3D11OcclusionQuery.h"
#include "gfx/D3D11/gfxD3D11EnumTranslate.h"
#include "gfx/D3D11/gfxD3D11Cubemap.h"
#include "gfx/D3D11/gfxD3D11TextureObject.h"
#include "gfx/D3D11/gfxD3D11TextureManager.h"
#include "gfx/D3D11/gfxD3D11Shader.h"

#include <vector>


GFXAdapter::CreateDeviceInstanceDelegate GFXD3D11Device::mCreateDeviceInstance(GFXD3D11Device::createInstance); 

class GFXD3D11CardProfiler: public GFXCardProfiler
{
private:
   typedef GFXCardProfiler Parent;
public:

   GFXD3D11CardProfiler(D3D_FEATURE_LEVEL featureLevel) : mFeatureLevel(featureLevel) {
   }

   ///
   virtual const String &getRendererString() const { static String sRS("Direct3D 11 Device Renderer"); return sRS; }

protected:
   D3D_FEATURE_LEVEL mFeatureLevel;

   virtual void setupCardCapabilities() {

      switch(mFeatureLevel)
      {
         case D3D_FEATURE_LEVEL_9_1:
         case D3D_FEATURE_LEVEL_9_2:
         {
            setCapability( "maxTextureWidth", 2048 );
            setCapability( "maxTextureHeight", 2048 );
            setCapability( "maxTextureSize", 2048 );
            break;
         }
         case D3D_FEATURE_LEVEL_9_3:
         {
            setCapability( "maxTextureWidth", 4096 );
            setCapability( "maxTextureHeight", 4096 );
            setCapability( "maxTextureSize", 4096 );
            break;
         }
         case D3D_FEATURE_LEVEL_10_0:
         case D3D_FEATURE_LEVEL_10_1:
         {
            setCapability( "maxTextureWidth", 8192 );
            setCapability( "maxTextureHeight", 8192 );
            setCapability( "maxTextureSize", 8192 );
            break;
         }
         case D3D_FEATURE_LEVEL_11_0:
         {
            setCapability( "maxTextureWidth", 16384 );
            setCapability( "maxTextureHeight", 16384 );
            setCapability( "maxTextureSize", 16384 );
            break;
         }
         default:
         {
               break;
         }
      }
   };

   virtual bool _queryCardCap(const String &query, U32 &foundResult){ return false; }
   virtual bool _queryFormat(const GFXFormat fmt, const GFXTextureProfile *profile, bool &inOutAutogenMips) {
      inOutAutogenMips = false;

      if(GFXD3D11TextureFormat[fmt] == DXGI_FORMAT_UNKNOWN)
      {
         return false;
      }
      return true;
   }

   void GetAdapterDesc(DXGI_ADAPTER_DESC& desc)
   {
      //Find the adapter. Since we passsed NULL to D3D11CreateDeviceAndSwapChain
      //we should be using the first one returned by EnumAdapters.
      IDXGIAdapter * pAdapter; 
      IDXGIFactory* pFactory = NULL; 

      // Create a DXGIFactory object.
      if(FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory) ,(void**)&pFactory)))
      {
         Con::errorf( "CreateDXGIFactory Failed!" );
         Platform::messageBox(   Con::getVariable( "$appName" ),
                                 "CreateDXGIFactory failed!\r\n"
                                 "Please be sure you have the latest version of DirectX installed.",
                                 MBOk, MIStop );
         Platform::forceShutdown( -1 );
      }

      pFactory->EnumAdapters(0, &pAdapter);

      if(pFactory)
      {
         pFactory->Release();
      }

      pAdapter->GetDesc(&desc);

      pAdapter->Release();
   }
public:
   virtual void init()
   {
      DXGI_ADAPTER_DESC adapterDesc;
      
      GetAdapterDesc(adapterDesc);

      mCardDescription = adapterDesc.Description;
      //http://www.pcidatabase.com/vendors.php?sort=id
      switch(adapterDesc.VendorId)
      {
      case 0x163C:
      case 0x8086:
      case 0x8087:
         mChipSet = "Intel";
         break;
      case 0x10DE:
         mChipSet = "NVIDIA";
         break;
      case 0x1022:
      case 0x1002:
         mChipSet = "AMD";
         break;
      default:
         mChipSet = "Unknown";
         break;
      }
      mVersionString = "0";
      mVideoMemory = adapterDesc.DedicatedVideoMemory / 1048576;//Bytes->Mega bytes.

      Parent::init(); // other code notes that not calling this is "BAD".
   };
};

//
// GFXD3D11Device
//

GFXDevice *GFXD3D11Device::createInstance( U32 adapterIndex )
{
   return new GFXD3D11Device();
}

GFXD3D11Device::GFXD3D11Device() : mD3DDevice(NULL), mLastPixShader(NULL), mLastVertShader(NULL)
{
   clip.set(0, 0, 800, 800);

   gScreenShot = new ScreenShot();

   // Set up the Enum translation tables
   GFXD3D11EnumTranslate::init();

   mDeviceSwizzle32 = &Swizzles::bgra;
   GFXVertexColor::setSwizzle( mDeviceSwizzle32 );

   //There is no DXGI_FORMAT for RGB8 in d3d11.
   mDeviceSwizzle24 = NULL;

   mPixVersion = 0.0f;
   mNumRenderTargets = 0;
}

GFXD3D11Device::~GFXD3D11Device()
{
}

//-----------------------------------------------------------------------------
// allocVertexBuffer
//-----------------------------------------------------------------------------
GFXVertexBuffer * GFXD3D11Device::allocVertexBuffer(   U32 numVerts, 
                                                      const GFXVertexFormat *vertexFormat, 
                                                      U32 vertSize, 
                                                      GFXBufferType bufferType,
                                                      void* data)
{
   PROFILE_SCOPE( GFXD3D9Device_allocVertexBuffer );

   GFXD3D11VertexBuffer *res = new GFXD3D11VertexBuffer(   this, 
                                                         numVerts, 
                                                         vertexFormat, 
                                                         vertSize, 
                                                         bufferType );

   res->mNumVerts = 0;

   D3D11_BUFFER_DESC bufferDesc;
   bufferDesc.ByteWidth       = vertSize * numVerts;
   bufferDesc.BindFlags       = D3D11_BIND_VERTEX_BUFFER;
   bufferDesc.CPUAccessFlags  = 0;
   bufferDesc.MiscFlags       = 0;
   bufferDesc.StructureByteStride = vertSize;

   // Assumptions:
   //    - immutable buffers are write once, use many. Data must be available at create time.
   //    - static buffers are write rarely, use many
   //    - dynamic buffers are write many, use many
   //    - volatile buffers are write once, use once
   // You may never read from a buffer.
   //Currently using d3d-dynamic usage for all bufers because
   //torque only uses map/unmap. This should be changed.
   switch(bufferType)
   {
   case GFXBufferTypeImmutable:
      bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
      break;
   case GFXBufferTypeStatic:
      bufferDesc.Usage = D3D11_USAGE_DEFAULT;

      //Create the stagine buffer used to update this resource
      D3D11_BUFFER_DESC stagingDesc;
      stagingDesc.Usage           = D3D11_USAGE_STAGING;
      stagingDesc.ByteWidth       = vertSize * numVerts;
      stagingDesc.BindFlags       = 0;
      stagingDesc.CPUAccessFlags  = D3D11_CPU_ACCESS_WRITE;
      stagingDesc.MiscFlags       = 0;
      stagingDesc.StructureByteStride = vertSize;
      D3D11Assert(mD3DDevice->CreateBuffer( &stagingDesc, NULL, &res->stagingBuffer ), "Failed to allocate an index buffer.");
      break;

   case GFXBufferTypeDynamic:
   case GFXBufferTypeVolatile:
      bufferDesc.Usage = D3D11_USAGE_DYNAMIC;//Should be D3D11_USAGE_DEFAULT?
      bufferDesc.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;
      break;
   }

   res->registerResourceWithDevice(this);

   // Create vertex buffer
   if( bufferType == GFXBufferTypeVolatile )
   {
      // NOTE: Volatile VBs are pooled and will be allocated at lock time.

      AssertFatal( numVerts <= MAX_DYNAMIC_VERTS, 
         "GFXD3D11Device::allocVertexBuffer - Volatile vertex buffer is too big... see MAX_DYNAMIC_VERTS!" );
   }
   else
   {
      // Requesting it will allocate it.
      vertexFormat->getDecl();

      // Get a new buffer...
      if(data)
      {
         D3D11_SUBRESOURCE_DATA subresData;
         subresData.pSysMem = data;
         subresData.SysMemPitch = bufferDesc.ByteWidth;
         subresData.SysMemSlicePitch = 0;
         D3D11Assert(mD3DDevice->CreateBuffer( &bufferDesc, &subresData, &res->vb ), "Failed to allocate VB.");
      }
      else
      {
         D3D11Assert(mD3DDevice->CreateBuffer( &bufferDesc, NULL, &res->vb ), "Failed to allocate VB");
      }
   }

   res->mNumVerts = numVerts;
   return res;
}

//-----------------------------------------------------------------------------
// deallocate vertex buffer
//-----------------------------------------------------------------------------
void GFXD3D11Device::deallocVertexBuffer( GFXD3D11VertexBuffer *vertBuff )
{
   SAFE_RELEASE(vertBuff->vb);
}


//-----------------------------------------------------------------------------
// allocPrimitiveBuffer
//-----------------------------------------------------------------------------
GFXPrimitiveBuffer * GFXD3D11Device::allocPrimitiveBuffer(   U32 numIndices, 
                                                            U32 numPrimitives, 
                                                            GFXBufferType bufferType,
                                                            void* data)
{
   // Allocate a buffer to return
   GFXD3D11PrimitiveBuffer * res = new GFXD3D11PrimitiveBuffer(this, numIndices, numPrimitives, bufferType);

   U32 bytesPerIndex = 4;
   if(bufferType == GFXIndexFormat16)
      bytesPerIndex = 2;

   D3D11_BUFFER_DESC bufferDesc;
   bufferDesc.ByteWidth       = bytesPerIndex * numIndices;
   bufferDesc.BindFlags       = D3D11_BIND_INDEX_BUFFER;
   bufferDesc.CPUAccessFlags  = 0;
   bufferDesc.MiscFlags       = 0;
   bufferDesc.StructureByteStride = bytesPerIndex;


   // Assumptions:
   //    - immutable buffers are write once, use many. Data must be available at create time.
   //    - static buffers are write rarely, use many
   //    - dynamic buffers are write many, use many
   //    - volatile buffers are write once, use once
   // You may never read from a buffer.
   //Currently using d3d-dynamic usage for all bufers because
   //torque only uses map/unmap. This should be changed.
   switch(bufferType)
   {
   case GFXBufferTypeImmutable:
      bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
      break;
   case GFXBufferTypeStatic:
      bufferDesc.Usage = D3D11_USAGE_DEFAULT;

      //Create the stagine buffer used to update this resource
      D3D11_BUFFER_DESC stagingDesc;
      stagingDesc.Usage           = D3D11_USAGE_STAGING;
      stagingDesc.ByteWidth       = bytesPerIndex * numIndices;
      stagingDesc.BindFlags       = 0;
      stagingDesc.CPUAccessFlags  = D3D11_CPU_ACCESS_WRITE;
      stagingDesc.MiscFlags       = 0;
      stagingDesc.StructureByteStride = bytesPerIndex;
      D3D11Assert(mD3DDevice->CreateBuffer( &stagingDesc, NULL, &res->stagingBuffer ), "Failed to allocate an index buffer.");
      break;
   case GFXBufferTypeDynamic:
      bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
      bufferDesc.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;
      break;
   case GFXBufferTypeVolatile:
      bufferDesc.Usage = D3D11_USAGE_DYNAMIC;//Should be either DEFAULT?
      bufferDesc.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;
      break;
   }

   // Register resource
   res->registerResourceWithDevice(this);

   // Create d3d index buffer
   if(bufferType == GFXBufferTypeVolatile)
   {
      // Get it from the pool if it's a volatile...
      AssertFatal( numIndices < MAX_DYNAMIC_INDICES, "Cannot allocate that many indices in a volatile buffer, increase MAX_DYNAMIC_INDICES." );

      res->ib              = mDynamicPB->ib;
      // mDynamicPB->ib->AddRef();
      res->mVolatileBuffer = mDynamicPB;
   }
   else
   {
      // Otherwise, get it as a seperate buffer...
      if(data)
      {
         D3D11_SUBRESOURCE_DATA subresData;
         subresData.pSysMem = data;
         subresData.SysMemPitch = bufferDesc.ByteWidth;
         subresData.SysMemSlicePitch = 0;
         D3D11Assert(mD3DDevice->CreateBuffer( &bufferDesc, &subresData, &res->ib ), "Failed to allocate an index buffer.");
      }
      else
      {
         D3D11Assert(mD3DDevice->CreateBuffer( &bufferDesc, NULL, &res->ib ), "Failed to allocate an index buffer.");
      }

   }

   return res;
}

GFXCubemap* GFXD3D11Device::createCubemap()
{ 
   GFXD3D11Cubemap* cube = new GFXD3D11Cubemap();
   cube->registerResourceWithDevice(this);
   return cube;
};

void GFXD3D11Device::enumerateAdapters( Vector<GFXAdapter*> &adapterList )
{
   IDXGIAdapter * pAdapter; 
   std::vector <IDXGIAdapter*> vAdapters; 
   IDXGIFactory* pFactory = NULL; 

   // Create a DXGIFactory object.
   if(FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory) ,(void**)&pFactory)))
   {
      Con::errorf( "CreateDXGIFactory Failed!" );
      Platform::messageBox(   Con::getVariable( "$appName" ),
                              "CreateDXGIFactory failed!\r\n"
                              "Please be sure you have the latest version of DirectX installed.",
                              MBOk, MIStop );
      Platform::forceShutdown( -1 );
   }

   for ( UINT i = 0;
         pFactory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND;
         ++i )
   {
      vAdapters.push_back(pAdapter); 
   }

   if(pFactory)
   {
      pFactory->Release();
   }

   for( U32 adapterIndex = 0; adapterIndex < vAdapters.size(); adapterIndex++ ) 
   {
      GFXAdapter *toAdd = new GFXAdapter;
      DXGI_ADAPTER_DESC sAdapterDesc;

      pAdapter = vAdapters[adapterIndex];

      toAdd->mType  = Direct3D11;
      toAdd->mIndex = adapterIndex;
      toAdd->mCreateDeviceInstanceDelegate = mCreateDeviceInstance;

      // Grab the shader model / feature level
      HRESULT hr = E_FAIL;
      D3D_FEATURE_LEVEL FeatureLevel;

      //Must be D3D_DRIVER_TYPE_UNKNOWN when supplying a specific adapter.
      hr = D3D11CreateDevice(pAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0,
                              D3D11_SDK_VERSION, NULL, &FeatureLevel, NULL );

      if(FAILED(hr))
      {
         return;
      }

      switch(FeatureLevel)
      {
         case D3D_FEATURE_LEVEL_9_1:
         case D3D_FEATURE_LEVEL_9_2:
         case D3D_FEATURE_LEVEL_9_3:
         {
               toAdd->mShaderModel = 2.0;
               break;
         }
         case D3D_FEATURE_LEVEL_10_0:
         case D3D_FEATURE_LEVEL_10_1:
         {
               toAdd->mShaderModel = 4.0;
               break;
         }
         case D3D_FEATURE_LEVEL_11_0:
         {
               toAdd->mShaderModel = 5.0;
               break;
         }
         default:
         {
               toAdd->mShaderModel = 2.0;
               break;
         }
      }

      pAdapter->GetDesc(&sAdapterDesc);

      // Get the device description string.

      wcstombs(toAdd->mName, sAdapterDesc.Description, GFXAdapter::MaxAdapterNameLen );
        dStrncat(toAdd->mName, " (D3D11)", GFXAdapter::MaxAdapterNameLen);

      // Video mode enumeration.

      UINT i = 0;
      IDXGIOutput * pOutput;
      std::vector<IDXGIOutput*> vOutputs;
      while(pAdapter->EnumOutputs(i, &pOutput) != DXGI_ERROR_NOT_FOUND)
      {
         vOutputs.push_back(pOutput);
         ++i;
      }

      //pOutput->GetDisplayModeList();

      Vector<DXGI_FORMAT> formats( __FILE__, __LINE__ );
      formats.push_back( DXGI_FORMAT_B5G6R5_UNORM );    // D3DFMT_R5G6B5 - 16bit format
      formats.push_back( DXGI_FORMAT_B8G8R8X8_UNORM );  // D3DFMT_X8R8G8B8 - 32bit format

      //Only check one output at the moment
      pOutput = vOutputs[0];

      for( S32 i = 0; i < formats.size(); i++ ) 
      {
         DWORD MaxSampleQualities = 1;
         //d3d9->CheckDeviceMultiSampleType(adapterIndex, D3DDEVTYPE_HAL, formats[i], FALSE, D3DMULTISAMPLE_NONMASKABLE, &MaxSampleQualities);
         //d3d11device->CheckMultisampleQualityLevels

         //Get the number of display modes
         U32 numModesSupported = 0;
         pOutput->GetDisplayModeList( formats[i], DXGI_ENUM_MODES_INTERLACED, &numModesSupported, 0);

         //Get all the display modes
         DXGI_MODE_DESC * pDescs = new DXGI_MODE_DESC[numModesSupported];
         pOutput->GetDisplayModeList( formats[i], DXGI_ENUM_MODES_INTERLACED, &numModesSupported, pDescs);

         //Convert display modes to our custom internal structure
         for( U32 j = 0; j < numModesSupported; j++ ) 
         {
            DXGI_MODE_DESC mode = pDescs[j];

            GFXVideoMode vmAdd;

            vmAdd.bitDepth    = ( i == 0 ? 16 : 32 ); // This will need to be changed later
            vmAdd.fullScreen  = true;
            vmAdd.refreshRate = mode.RefreshRate.Numerator;
            vmAdd.resolution  = Point2I( mode.Width, mode.Height );
            vmAdd.antialiasLevel = MaxSampleQualities;

            toAdd->mAvailableModes.push_back( vmAdd );
         }
      }

        adapterList.push_back( toAdd );
    }
}

void GFXD3D11Device::setLightInternal(U32 lightStage, const GFXLightInfo light, bool lightEnable)
{

}

//-----------------------------------------------------------------------------
// Setup D3D present parameters - init helper function
//-----------------------------------------------------------------------------
DXGI_SWAP_CHAIN_DESC GFXD3D11Device::setupSwapChainParams( const GFXVideoMode &mode, const HWND &hWnd ) const
{
   // Create D3D Presentation params
   DXGI_SWAP_CHAIN_DESC sd; 
   dMemset( &sd, 0, sizeof( sd ) );

   DXGI_FORMAT fmt = DXGI_FORMAT_B8G8R8A8_UNORM; // 32 bit

   if( mode.bitDepth == 16 )
      fmt = DXGI_FORMAT_B5G6R5_UNORM;

   //DWORD aalevel;

   // Setup the AA flags...  If we've been ask to 
   // disable  hardware AA then do that now.
   /*if ( mode.antialiasLevel == 0 || Con::getBoolVariable( "$pref::Video::disableHardwareAA", false ) )
   {
      aalevel = 0;
   } 
   else 
   {
      aalevel = mode.antialiasLevel-1;*/
   //}
  
   //_validateMultisampleParams(fmt, aatype, aalevel);
   
   sd.BufferDesc.Width  = mode.resolution.x;
   sd.BufferDesc.Height = mode.resolution.y;
   sd.BufferDesc.Format = fmt;
   sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
   sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
   sd.BufferDesc.RefreshRate.Numerator = 60;
   sd.BufferDesc.RefreshRate.Denominator = 1;
   sd.SampleDesc.Count = 1;
   sd.SampleDesc.Quality = 0;
   sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
   sd.BufferCount  = 1;
   sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
   sd.OutputWindow = hWnd;
   sd.Windowed = !mode.fullScreen;
   sd.Flags = 0;

   //d3dpp.EnableAutoDepthStencil = TRUE;
   //d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;

   //if ( smDisableVSync )
   //   d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;	// This does NOT wait for vsync

   return sd;
}

void GFXD3D11Device::init( const GFXVideoMode &mode, PlatformWindow *window )
{
   AssertFatal(window, "GFXD3D11Device::init - must specify a window!");

   Win32Window *win = dynamic_cast<Win32Window*>( window );
   AssertISV( win, "GFXD3D11Device::init - got a non Win32Window window passed in! Did DX go crossplatform?" );

   HWND winHwnd = win->getHWND();

   DXGI_SWAP_CHAIN_DESC sd = setupSwapChainParams( mode, winHwnd );


   HRESULT hr = E_FAIL;
   U32 deviceFlags = 0;

   mMultisampleInfo.Count = sd.SampleDesc.Count;
   mMultisampleInfo.Quality = sd.SampleDesc.Quality;

#ifdef TORQUE_DEBUG_RENDER
   deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

   hr = D3D11CreateDeviceAndSwapChain(
      NULL, // might fail with two adapters in machine
      D3D_DRIVER_TYPE_HARDWARE,
      NULL, 
      deviceFlags,
      NULL,
      0,
      D3D11_SDK_VERSION,
      &sd,
      &mSwapChain,
      &mD3DDevice,
      &mFeatureLevel,
      &mImmediateContext);

   if(FAILED(hr))
   {
      Con::errorf("Failed to create hardware device.");
      Platform::AlertOK("DirectX Error!", "Failed to initialize Direct3D! Make sure you have DirectX 11 installed, and "
            "are running a graphics card that supports it.");
      Platform::forceShutdown(1);
   }

   mSwapChain->GetBuffer(0, __uuidof( ID3D11Texture2D ), (LPVOID*)&(mBackBuffer)) ;
   mD3DDevice->CreateRenderTargetView( mBackBuffer, NULL, &mRenderTargetView );

   // Create depth stencil texture
   D3D11_TEXTURE2D_DESC descDepth;
   ZeroMemory( &descDepth, sizeof(descDepth) );
   descDepth.Width = mode.resolution.x;
   descDepth.Height = mode.resolution.y;
   descDepth.MipLevels = 1;
   descDepth.ArraySize = 1;
   descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
   descDepth.SampleDesc.Count = 1;
   descDepth.SampleDesc.Quality = 0;
   descDepth.Usage = D3D11_USAGE_DEFAULT;
   descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
   descDepth.CPUAccessFlags = 0;
   descDepth.MiscFlags = 0;

   D3D11Assert(mD3DDevice->CreateTexture2D( &descDepth, NULL, &mDepthStencilTex ), "Failed to create depth/stencil texture");

    // Create the depth stencil view
   D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
   ZeroMemory( &descDSV, sizeof(descDSV) );
   descDSV.Format = descDepth.Format;
   descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
   descDSV.Texture2D.MipSlice = 0;
   D3D11Assert(mD3DDevice->CreateDepthStencilView( mDepthStencilTex, &descDSV, &mDepthStencilView ), "Failed to create depth/stencil view");

   mImmediateContext->OMSetRenderTargets(1, &mRenderTargetView, mDepthStencilView );

   switch(mFeatureLevel)
   {
      case D3D_FEATURE_LEVEL_9_1:
      case D3D_FEATURE_LEVEL_9_2:
      {
         mPixVersion = 2.0;
         mNumRenderTargets = 1;
      }
      case D3D_FEATURE_LEVEL_9_3:
      {
            mPixVersion = 2.0;
            mNumRenderTargets = 4;
            break;
      }
      case D3D_FEATURE_LEVEL_10_0:
      case D3D_FEATURE_LEVEL_10_1:
      {
            mPixVersion = 4.0;
            mNumRenderTargets = 8;
            break;
      }
      case D3D_FEATURE_LEVEL_11_0:
      {
            mPixVersion = 5.0;
            mNumRenderTargets = 8;
            break;
      }
      default:
      {
            mPixVersion = 2.0;
            mNumRenderTargets = 1;
            break;
      }
   }

   mCardProfiler = new GFXD3D11CardProfiler(mFeatureLevel);
   mCardProfiler->init();

   mTextureManager = new GFXD3D11TextureManager(mD3DDevice);

   mInitialized = true;

   deviceInited();
}

GFXWindowTarget *GFXD3D11Device::allocWindowTarget(PlatformWindow *window)
{
   AssertFatal(window,"GFXD3D11Device::allocWindowTarget - no window provided!");
   if(mD3DDevice == NULL)
   {
      init(window->getVideoMode(), window);
   }
   return new GFXD3D11WindowTarget(mSwapChain);
};

GFXStateBlockRef GFXD3D11Device::createStateBlockInternal(const GFXStateBlockDesc& desc)
{
   return GFXStateBlockRef(new GFXD3D11StateBlock(desc, mD3DDevice, mImmediateContext));
}

/// Activates a stateblock
void GFXD3D11Device::setStateBlockInternal(GFXStateBlock* block, bool force)
{
   AssertFatal(dynamic_cast<GFXD3D11StateBlock*>(block), "Incorrect stateblock type for this device!");
   GFXD3D11StateBlock* d3dBlock = static_cast<GFXD3D11StateBlock*>(block);
   GFXD3D11StateBlock* d3dCurrent = static_cast<GFXD3D11StateBlock*>(mCurrentStateBlock.getPointer());
   if (force)
      d3dCurrent = NULL;
   d3dBlock->activate(d3dCurrent);   
}

GFXFence *GFXD3D11Device::createFence()
{
    GFXFence* fence = new GFXD3D11QueryFence( this );
    fence->registerResourceWithDevice(this);
    return fence;
}

GFXOcclusionQuery* GFXD3D11Device::createOcclusionQuery()
{  
   GFXOcclusionQuery *query;

   query = new GFXD3D11OcclusionQuery( this );    

   query->registerResourceWithDevice(this);
   return query;
}

void GFXD3D11Device::_setPrimitiveBuffer( GFXPrimitiveBuffer *buffer ) 
{
   mCurrentPB = static_cast<GFXD3D11PrimitiveBuffer *>( buffer );

   mImmediateContext->IASetIndexBuffer( mCurrentPB->ib, DXGI_FORMAT_R16_UINT, 0 );
}

void GFXD3D11Device::drawPrimitive( GFXPrimitiveType primType, U32 vertexStart, U32 primitiveCount ) 
{
   // This is done to avoid the function call overhead if possible
   if( mStateDirty )
      updateStates();
   if (mCurrentShaderConstBuffer)
      setShaderConstBufferInternal(mCurrentShaderConstBuffer);

   //if ( mVolatileVB )
   //   vertexStart += mVolatileVB->mVolatileStart;

   D3D11Assert(primType != GFXTriangleFan, "Triangle fans not supported by D3D11");

   mImmediateContext->IASetPrimitiveTopology(GFXD3D11PrimType[primType]);
   mImmediateContext->Draw(primitiveCount, vertexStart);

   mDeviceStatistics.mDrawCalls++;
   if ( mVertexBufferFrequency[0] > 1 )
      mDeviceStatistics.mPolyCount += primitiveCount * mVertexBufferFrequency[0];
   else
      mDeviceStatistics.mPolyCount += primitiveCount;
}

//-----------------------------------------------------------------------------

void GFXD3D11Device::drawIndexedPrimitive( GFXPrimitiveType primType, 
                                          U32 startVertex, 
                                          U32 minIndex, 
                                          U32 numVerts, 
                                          U32 startIndex, 
                                          U32 primitiveCount ) 
{
   // This is done to avoid the function call overhead if possible
   if( mStateDirty )
      updateStates();
   if (mCurrentShaderConstBuffer)
      setShaderConstBufferInternal(mCurrentShaderConstBuffer);

   AssertFatal( mCurrentPB != NULL, "Trying to call drawIndexedPrimitive with no current index buffer, call setIndexBuffer()" );

   //if ( mVolatileVB )
   //   startVertex += mVolatileVB->mVolatileStart;

   mImmediateContext->IASetPrimitiveTopology(GFXD3D11PrimType[primType]);
   mImmediateContext->DrawIndexed(primitiveCount, mCurrentPB->mVolatileStart + startIndex, startVertex);

   mDeviceStatistics.mDrawCalls++;
   if ( mVertexBufferFrequency[0] > 1 )
      mDeviceStatistics.mPolyCount += primitiveCount * mVertexBufferFrequency[0];
   else
      mDeviceStatistics.mPolyCount += primitiveCount;
}

GFXD3D11VertexBuffer* GFXD3D11Device::findVBPool( const GFXVertexFormat *vertexFormat, U32 vertsNeeded )
{
   PROFILE_SCOPE( GFXD3D9Device_findVBPool );

   // Verts needed is ignored on the base device, 360 is different
   for( U32 i=0; i<mVolatileVBList.size(); i++ )
      if( mVolatileVBList[i]->mVertexFormat.isEqual( *vertexFormat ) )
         return mVolatileVBList[i];

   return NULL;
}

GFXD3D11VertexBuffer * GFXD3D11Device::createVBPool( const GFXVertexFormat *vertexFormat, U32 vertSize )
{
   PROFILE_SCOPE( GFXD3D9Device_createVBPool );

   // this is a bit funky, but it will avoid problems with (lack of) copy constructors
   //    with a push_back() situation
   mVolatileVBList.increment();
   StrongRefPtr<GFXD3D11VertexBuffer> newBuff;
   mVolatileVBList.last() = new GFXD3D11VertexBuffer();
   newBuff = mVolatileVBList.last();

   newBuff->mNumVerts   = 0;
   newBuff->mBufferType = GFXBufferTypeVolatile;
   newBuff->mVertexFormat.copy( *vertexFormat );
   newBuff->mVertexSize = vertSize;
   newBuff->mDevice = this;

   // Requesting it will allocate it.
   vertexFormat->getDecl();

   //   Con::printf("Created buff with type %x", vertFlags);

   D3D11_BUFFER_DESC bufferDesc;
   bufferDesc.ByteWidth       = vertSize * MAX_DYNAMIC_VERTS;
   bufferDesc.BindFlags       = D3D11_BIND_VERTEX_BUFFER;
   bufferDesc.CPUAccessFlags  = D3D11_CPU_ACCESS_WRITE;
   bufferDesc.MiscFlags       = 0;
   bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
   bufferDesc.StructureByteStride = vertSize;
   D3D11Assert(mD3DDevice->CreateBuffer( &bufferDesc, NULL, &newBuff->vb ), "Failed to allocate dynamic VB");

   return newBuff;
}

GFXFormat GFXD3D11Device::selectSupportedFormat(  GFXTextureProfile *profile, 
                                          const Vector<GFXFormat> &formats, 
                                          bool texture, 
                                          bool mustblend, 
                                          bool mustfilter ) { 
   
   for(U32 i=0; i<formats.size(); i++)
   {
      const GFXFormat fmt = formats[i];
      if(GFXD3D11TextureFormat[fmt] != DXGI_FORMAT_UNKNOWN)
      {
         return fmt;
      }
   }
   return GFXFormatR8G8B8A8;
}

GFXShader* GFXD3D11Device::createShader()
{
   GFXD3D11Shader* shader = new GFXD3D11Shader();
   shader->registerResourceWithDevice( this );
   return shader;
}

void GFXD3D11Device::setShader( GFXShader *shader )
{
   GFXD3D11Shader *d3dShader = static_cast<GFXD3D11Shader*>( shader );

   ID3D11PixelShader *pixShader = ( d3dShader != NULL ? d3dShader->mPixShader : NULL );
   ID3D11VertexShader *vertShader = ( d3dShader ? d3dShader->mVertShader : NULL );

   if( pixShader != mLastPixShader )
   {
      mImmediateContext->PSSetShader(pixShader, NULL, 0);
      mLastPixShader = pixShader;
   }

   if( vertShader != mLastVertShader )
   {
      mImmediateContext->VSSetShader(vertShader, NULL, 0);
      mLastVertShader = vertShader;
   }
}

GFXVertexDecl* GFXD3D11Device::allocVertexDecl( const GFXVertexFormat *vertexFormat )
{
   // First check the map... you shouldn't allocate VBs very often
   // if you want performance.  The map lookup should never become
   // a performance bottleneck.
   D3D11VertexDecl *decl = mVertexDecls[vertexFormat->getDescription()];
   if ( decl )
      return decl;

   // Setup the declaration struct.
   U32 elemCount = vertexFormat->getElementCount();
   U32 offset = 0;
   D3D11_INPUT_ELEMENT_DESC *ie = new D3D11_INPUT_ELEMENT_DESC[ elemCount + 1 ];

   for ( U32 i=0; i < elemCount; i++ )
   {
      const GFXVertexElement &element = vertexFormat->getElement( i );
      
      AssertWarn(element.getStreamIndex()==0, "Non-zero stream with d3d11 not implemented" );

      ie[i].AlignedByteOffset = offset;
      offset += element.getSizeInBytes();

      ie[i].InputSlot = 0;
      ie[i].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
      ie[i].InstanceDataStepRate = 0;

      switch(element.getType())
      {
         case GFXDeclType_Float:
         {
            ie[i].Format = DXGI_FORMAT_R32_FLOAT;
            break;
         }
         case GFXDeclType_Float2:
         {
            ie[i].Format = DXGI_FORMAT_R32G32_FLOAT;
            break;
         }
         case GFXDeclType_Float3:
         {
            ie[i].Format = DXGI_FORMAT_R32G32B32_FLOAT;
            break;
         }
         case GFXDeclType_Color:
         {
             ie[i].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
             break;
         }
         case GFXDeclType_Float4:
         default:
         {
            ie[i].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            break;
         }
      }

      ie[i].SemanticIndex = 0;
      if ( element.isSemantic( GFXSemantic::POSITION ) )
         ie[i].SemanticName = "POSITION";
      else if ( element.isSemantic( GFXSemantic::NORMAL ) )
         ie[i].SemanticName = "NORMAL";
      else if ( element.isSemantic( GFXSemantic::COLOR ) )
         ie[i].SemanticName = "COLOR";
      else if ( element.isSemantic( GFXSemantic::TANGENT ) )
         ie[i].SemanticName = "TANGENT";
      else if ( element.isSemantic( GFXSemantic::BINORMAL ) )
         ie[i].SemanticName = "BINORMAL";
      else
      {
         // Anything that falls thru to here will be a texture coord.
         ie[i].SemanticName = "TEXCOORD";
         ie[i].SemanticIndex = element.getSemanticIndex();
      }
   }

   decl = new D3D11VertexDecl();

   //D3D11Assert(mD3DDevice->CreateInputLayout(ie, elemCount, 0, 0, &decl->decl), "Failed to create input layout!");

   delete [] ie;

   mVertexDecls[vertexFormat->getDescription()] = decl;

   return decl;
}
void GFXD3D11Device::setVertexDecl( const GFXVertexDecl *decl )
{
   ID3D11InputLayout *dx11Decl = NULL;
   if ( decl )
      dx11Decl = static_cast<const D3D11VertexDecl*>( decl )->decl;
   //mImmediateContext->IASetInputLayout(dx11Decl);
}
void GFXD3D11Device::setVertexStream( U32 stream, GFXVertexBuffer *buffer )
{

}
void GFXD3D11Device::setVertexStreamFrequency( U32 stream, U32 frequency )
{

}

void GFXD3D11Device::setupGenericShaders( GenericShaderType type /* = GSColor */ )
{
   if( mGenericShader[GSColor] == NULL )
   {
       Vector<GFXShaderMacro> shaderMacros;
       mGenericShader[GSColor] = createShader();

       mGenericShader[GSColor]->init("shaders/common/fixedFunction/colorV.hlsl", 
         "shaders/common/fixedFunction/colorP.hlsl", 
         2.f, shaderMacros);

       mGenericShader[GSModColorTexture] = createShader();
       mGenericShader[GSModColorTexture]->init("shaders/common/fixedFunction/modColorTextureV.hlsl", 
         "shaders/common/fixedFunction/modColorTextureP.hlsl", 
         2.f, shaderMacros);

       mGenericShader[GSAddColorTexture] = createShader();
       mGenericShader[GSAddColorTexture]->init("shaders/common/fixedFunction/addColorTextureV.hlsl", 
         "shaders/common/fixedFunction/addColorTextureP.hlsl", 
         2.f, shaderMacros);
   }

   //mGenericShader[type]->process();

   MatrixF world, view, proj;
   mWorldMatrix[mWorldStackSize].transposeTo( world );
   mViewMatrix.transposeTo( view );
   mProjectionMatrix.transposeTo( proj );

   mTempMatrix = world * view * proj;

   //setVertexShaderConstF( VC_WORLD_PROJ, (F32 *)&mTempMatrix, 4 );
}

void GFXD3D11Device::setClipRect( const RectI &inRect ) 
{
	// We transform the incoming rect by the view 
   // matrix first, so that it can be used to pan
   // and scale the clip rect.
   //
   // This is currently used to take tiled screenshots.
   Point3F pos( inRect.point.x, inRect.point.y, 0.0f );
   Point3F extent( inRect.extent.x, inRect.extent.y, 0.0f );
   getViewMatrix().mulP( pos );
   getViewMatrix().mulV( extent );  
   RectI rect( pos.x, pos.y, extent.x, extent.y );

   // Clip the rect against the renderable size.
   Point2I size = mCurrentRT->getSize();

   RectI maxRect(Point2I(0,0), size);
   rect.intersect(maxRect);

   mClipRect = rect;

   F32 l = F32( mClipRect.point.x );
   F32 r = F32( mClipRect.point.x + mClipRect.extent.x );
   F32 b = F32( mClipRect.point.y + mClipRect.extent.y );
   F32 t = F32( mClipRect.point.y );

   // Set up projection matrix, 
   static Point4F pt;   
   pt.set(2.0f / (r - l), 0.0f, 0.0f, 0.0f);
   mTempMatrix.setColumn(0, pt);

   pt.set(0.0f, 2.0f/(t - b), 0.0f, 0.0f);
   mTempMatrix.setColumn(1, pt);

   pt.set(0.0f, 0.0f, 1.0f, 0.0f);
   mTempMatrix.setColumn(2, pt);

   pt.set((l+r)/(l-r), (t+b)/(b-t), 1.0f, 1.0f);
   mTempMatrix.setColumn(3, pt);

   setProjectionMatrix( mTempMatrix );

   // Set up world/view matrix
   mTempMatrix.identity();   
   setWorldMatrix( mTempMatrix );

   setViewport( mClipRect );
}

void GFXD3D11Device::_updateRenderTargets()
{
   if ( mRTDirty || ( mCurrentRT && mCurrentRT->isPendingState() ) )
   {
      if ( mRTDeactivate )
      {
         mRTDeactivate->deactivate();
         mRTDeactivate = NULL;   
      }

      // NOTE: The render target changes are not really accurate
      // as the GFXTextureTarget supports MRT internally.  So when
      // we activate a GFXTarget it could result in multiple calls
      // to SetRenderTarget on the actual device.
      mDeviceStatistics.mRenderTargetChanges++;

      mCurrentRT->activate();

      mRTDirty = false;
   }  

   if ( mViewportDirty )
   {
      D3D11_VIEWPORT vp;
      vp.Width = mViewport.extent.x;
      vp.Height = mViewport.extent.y;
      vp.MinDepth = 0.0f;
      vp.MaxDepth = 1.0f;
      vp.TopLeftX = mViewport.point.x;
      vp.TopLeftY = mViewport.point.y;

      mImmediateContext->RSSetViewports(1, &vp);

      mViewportDirty = false;
   }
}

void GFXD3D11Device::clear( U32 flags, ColorI color, F32 z, U32 stencil )
{
   // Make sure we have flushed our render target state.
   _updateRenderTargets();

   float ClearColor[4] = { 1.0f/256.0f * color.red,
       1.0f/256.0f * color.green,
       1.0f/256.0f * color.blue,
       1.0f/256.0f * color.alpha };

   if( flags & GFXClearTarget )
      mImmediateContext->ClearRenderTargetView(mRenderTargetView, ClearColor);

   U32 zsClear = 0;

   if( flags & GFXClearZBuffer)
       zsClear |= D3D11_CLEAR_DEPTH;
   if( flags & GFXClearStencil)
       zsClear |= D3D11_CLEAR_STENCIL;

    mImmediateContext->ClearDepthStencilView(mDepthStencilView, zsClear, z, stencil);
};

//
// Register this device with GFXInit
//
class GFXD3D11RegisterDevice
{
public:
   GFXD3D11RegisterDevice()
   {
      GFXInit::getRegisterDeviceSignal().notify(&GFXD3D11Device::enumerateAdapters);
   }
};

static GFXD3D11RegisterDevice pNullRegisterDevice;
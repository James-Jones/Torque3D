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

#include <vector>


GFXAdapter::CreateDeviceInstanceDelegate GFXD3D11Device::mCreateDeviceInstance(GFXD3D11Device::createInstance); 

class GFXD3D11CardProfiler: public GFXCardProfiler
{
private:
   typedef GFXCardProfiler Parent;
public:

   ///
   virtual const String &getRendererString() const { static String sRS("Direct3D 11 Device Renderer"); return sRS; }

protected:

   virtual void setupCardCapabilities() { };

   virtual bool _queryCardCap(const String &query, U32 &foundResult){ return false; }
   virtual bool _queryFormat(const GFXFormat fmt, const GFXTextureProfile *profile, bool &inOutAutogenMips) { inOutAutogenMips = false; return false; }
   
public:
   virtual void init()
   {
      mCardDescription = "Direct3D 11 Device Card";
      mChipSet = "Direct3D 11 Device";
      mVersionString = "0";

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

GFXD3D11Device::GFXD3D11Device() : mD3DDevice(NULL)
{
   clip.set(0, 0, 800, 800);

   gScreenShot = new ScreenShot();
   mCardProfiler = new GFXD3D11CardProfiler();
   mCardProfiler->init();

   // Set up the Enum translation tables
   GFXD3D11EnumTranslate::init();
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
                                                      GFXBufferType bufferType )
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

   // Assumptions:
   //    - static buffers are write once, use many
   //    - dynamic buffers are write many, use many
   //    - volatile buffers are write once, use once
   // You may never read from a buffer.
   //Currently using d3d-dynamic usage for all bufers because
   //torque only uses map/unmap. This should be changed.
   switch(bufferType)
   {
   case GFXBufferTypeStatic:
      bufferDesc.Usage = D3D11_USAGE_DYNAMIC;//Should be D3D11_USAGE_IMMUTABLE
      break;

   case GFXBufferTypeDynamic:
   case GFXBufferTypeVolatile:
      bufferDesc.Usage = D3D11_USAGE_DYNAMIC;//Should be D3D11_USAGE_DEFAULT
      break;
   }

   res->registerResourceWithDevice(this);

   // Create vertex buffer
   if( bufferType == GFXBufferTypeVolatile )
   {
      // NOTE: Volatile VBs are pooled and will be allocated at lock time.

      AssertFatal( numVerts <= MAX_DYNAMIC_VERTS, 
         "GFXD3D9Device::allocVertexBuffer - Volatile vertex buffer is too big... see MAX_DYNAMIC_VERTS!" );
   }
   else
   {
      // Requesting it will allocate it.
      vertexFormat->getDecl();

      // Get a new buffer...
      D3D11Assert(mD3DDevice->CreateBuffer( &bufferDesc, NULL, &res->vb ), "Failed to allocate VB");
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
                                                            GFXBufferType bufferType )
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


   // Assumptions:
   //    - static buffers are write once, use many
   //    - dynamic buffers are write many, use many
   //    - volatile buffers are write once, use once
   // You may never read from a buffer.
   //Currently using d3d-dynamic usage for all bufers because
   //torque only uses map/unmap. This should be changed.
   switch(bufferType)
   {
   case GFXBufferTypeStatic:
      bufferDesc.Usage = D3D11_USAGE_DYNAMIC;//Should be D3D11_USAGE_IMMUTABLE;
      break;
   case GFXBufferTypeDynamic:
      bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
      break;
   case GFXBufferTypeVolatile:
      bufferDesc.Usage = D3D11_USAGE_DYNAMIC;//Should be either DEFAULT or IMMUTABLE.
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
      D3D11Assert(mD3DDevice->CreateBuffer( &bufferDesc, NULL, &res->ib ), "Failed to allocate an index buffer.");
      //D3D11Assert(mD3DDevice->CreateIndexBuffer( sizeof(U16) * numIndices , usage, GFXD3D9IndexFormat[GFXIndexFormat16], pool, &res->ib, 0),
       //  "Failed to allocate an index buffer.");
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

#ifdef TORQUE_DEBUG_RENDER
   deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

   hr = D3D11CreateDeviceAndSwapChain(
			NULL,					// might fail with two adapters in machine
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
   mImmediateContext->OMSetRenderTargets(1, &mRenderTargetView, NULL );

   mCardProfiler = new GFXD3D11CardProfiler();
   mCardProfiler->init();

   mTextureManager = new GFXD3D11TextureManager(mD3DDevice);
}

GFXWindowTarget *GFXD3D11Device::allocWindowTarget(PlatformWindow *window)
{
	AssertFatal(window,"GFXD3D11Device::allocWindowTarget - no window provided!");
	if(mD3DDevice == NULL)
	{
		init(window->getVideoMode(), window);
	}
    return new GFXD3D11WindowTarget();
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
   bufferDesc.CPUAccessFlags  = 0;
   bufferDesc.MiscFlags       = 0;
   bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
   D3D11Assert(mD3DDevice->CreateBuffer( &bufferDesc, NULL, &newBuff->vb ), "Failed to allocate dynamic VB");

   return newBuff;
}

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
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

#include <d3d11.h>
#include <vector>


GFXAdapter::CreateDeviceInstanceDelegate GFXD3D11Device::mCreateDeviceInstance(GFXD3D11Device::createInstance); 

class GFXD3D11CardProfiler: public GFXCardProfiler
{
private:
   typedef GFXCardProfiler Parent;
public:

   ///
   virtual const String &getRendererString() const { static String sRS("GFX Null Device Renderer"); return sRS; }

protected:

   virtual void setupCardCapabilities() { };

   virtual bool _queryCardCap(const String &query, U32 &foundResult){ return false; }
   virtual bool _queryFormat(const GFXFormat fmt, const GFXTextureProfile *profile, bool &inOutAutogenMips) { inOutAutogenMips = false; return false; }
   
public:
   virtual void init()
   {
      mCardDescription = "GFX Null Device Card";
      mChipSet = "NULL Device";
      mVersionString = "0";

      Parent::init(); // other code notes that not calling this is "BAD".
   };
};

class GFXD3D11TextureObject : public GFXTextureObject 
{
public:
   GFXD3D11TextureObject(GFXDevice * aDevice, GFXTextureProfile *profile); 
   ~GFXD3D11TextureObject() { kill(); };

   virtual void pureVirtualCrash() { };

   virtual GFXLockedRect * lock( U32 mipLevel = 0, RectI *inRect = NULL ) { return NULL; };
   virtual void unlock( U32 mipLevel = 0) {};
   virtual bool copyToBmp(GBitmap *) { return false; };

   virtual void zombify() {}
   virtual void resurrect() {}
};

GFXD3D11TextureObject::GFXD3D11TextureObject(GFXDevice * aDevice, GFXTextureProfile *profile) :
   GFXTextureObject(aDevice, profile) 
{
   mProfile = profile;
   mTextureSize.set( 0, 0, 0 );
}

class GFXD3D11TextureManager : public GFXTextureManager
{
protected:
      virtual GFXTextureObject *_createTextureObject( U32 height, 
                                                      U32 width, 
                                                      U32 depth, 
                                                      GFXFormat format, 
                                                      GFXTextureProfile *profile, 
                                                      U32 numMipLevels, 
                                                      bool forceMips = false, 
                                                      S32 antialiasLevel = 0, 
                                                      GFXTextureObject *inTex = NULL )
      { 
         GFXD3D11TextureObject *retTex;
         if ( inTex )
         {
            AssertFatal( dynamic_cast<GFXD3D11TextureObject*>( inTex ), "GFXD3D11TextureManager::_createTexture() - Bad inTex type!" );
            retTex = static_cast<GFXD3D11TextureObject*>( inTex );
         }      
         else
         {
            retTex = new GFXD3D11TextureObject( GFX, profile );
            retTex->registerResourceWithDevice( GFX );
         }

         SAFE_DELETE( retTex->mBitmap );
         retTex->mBitmap = new GBitmap(width, height);
         return retTex;
      };

      /// Load a texture from a proper DDSFile instance.
      virtual bool _loadTexture(GFXTextureObject *texture, DDSFile *dds){ return true; };

      /// Load data into a texture from a GBitmap using the internal API.
      virtual bool _loadTexture(GFXTextureObject *texture, GBitmap *bmp){ return true; };

      /// Load data into a texture from a raw buffer using the internal API.
      ///
      /// Note that the size of the buffer is assumed from the parameters used
      /// for this GFXTextureObject's _createTexture call.
      virtual bool _loadTexture(GFXTextureObject *texture, void *raw){ return true; };

      /// Refresh a texture using the internal API.
      virtual bool _refreshTexture(GFXTextureObject *texture){ return true; };

      /// Free a texture (but do not delete the GFXTextureObject) using the internal
      /// API.
      ///
      /// This is only called during zombification for textures which need it, so you
      /// don't need to do any internal safety checks.
      virtual bool _freeTexture(GFXTextureObject *texture, bool zombify=false) { return true; };

      virtual U32 _getTotalVideoMemory() { return 0; };
      virtual U32 _getFreeVideoMemory() { return 0; };
};

class GFXD3D11Cubemap : public GFXCubemap
{
   friend class GFXDevice;
private:
   // should only be called by GFXDevice
   virtual void setToTexUnit( U32 tuNum ) { };

public:
   virtual void initStatic( GFXTexHandle *faces ) { };
   virtual void initStatic( DDSFile *dds ) { };
   virtual void initDynamic( U32 texSize, GFXFormat faceFormat = GFXFormatR8G8B8A8 ) { };
   virtual U32 getSize() const { return 0; }
   virtual GFXFormat getFormat() const { return GFXFormatR8G8B8A8; }

   virtual ~GFXD3D11Cubemap(){};

   virtual void zombify() {}
   virtual void resurrect() {}
};

class GFXD3D11VertexBuffer : public GFXVertexBuffer 
{
   unsigned char* tempBuf;
public:
   GFXD3D11VertexBuffer( GFXDevice *device, 
                        U32 numVerts, 
                        const GFXVertexFormat *vertexFormat, 
                        U32 vertexSize, 
                        GFXBufferType bufferType ) :
      GFXVertexBuffer(device, numVerts, vertexFormat, vertexSize, bufferType) { };
   virtual void lock(U32 vertexStart, U32 vertexEnd, void **vertexPtr);
   virtual void unlock();
   virtual void prepare();

   virtual void zombify() {}
   virtual void resurrect() {}
};

void GFXD3D11VertexBuffer::lock(U32 vertexStart, U32 vertexEnd, void **vertexPtr) 
{
   tempBuf = new unsigned char[(vertexEnd - vertexStart) * mVertexSize];
   *vertexPtr = (void*) tempBuf;
   lockedVertexStart = vertexStart;
   lockedVertexEnd   = vertexEnd;
}

void GFXD3D11VertexBuffer::unlock() 
{
   delete[] tempBuf;
   tempBuf = NULL;
}

void GFXD3D11VertexBuffer::prepare() 
{
}

class GFXD3D11PrimitiveBuffer : public GFXPrimitiveBuffer
{
private:
   U16* temp;
public:
   GFXD3D11PrimitiveBuffer( GFXDevice *device, 
                           U32 indexCount, 
                           U32 primitiveCount, 
                           GFXBufferType bufferType ) :
      GFXPrimitiveBuffer(device, indexCount, primitiveCount, bufferType), temp( NULL ) {};

   virtual void lock(U32 indexStart, U32 indexEnd, void **indexPtr); ///< locks this primitive buffer for writing into
   virtual void unlock(); ///< unlocks this primitive buffer.
   virtual void prepare() { };  ///< prepares this primitive buffer for use on the device it was allocated on

   virtual void zombify() {}
   virtual void resurrect() {}
};

void GFXD3D11PrimitiveBuffer::lock(U32 indexStart, U32 indexEnd, void **indexPtr)
{
   temp = new U16[indexEnd - indexStart];
   *indexPtr = temp;
}

void GFXD3D11PrimitiveBuffer::unlock() 
{
   delete[] temp;
   temp = NULL;
}

//
// GFXD3D11StateBlock
//
class GFXD3D11StateBlock : public GFXStateBlock
{
public:
   /// Returns the hash value of the desc that created this block
   virtual U32 getHashValue() const { return 0; };

   /// Returns a GFXStateBlockDesc that this block represents
   virtual const GFXStateBlockDesc& getDesc() const { return mDefaultDesc; }

   //
   // GFXResource
   //
   virtual void zombify() { }
   /// When called the resource should restore all device sensitive information destroyed by zombify()
   virtual void resurrect() { }
private:
   GFXStateBlockDesc mDefaultDesc;
};

//
// GFXD3D11Device
//

GFXDevice *GFXD3D11Device::createInstance( U32 adapterIndex )
{
   return new GFXD3D11Device();
}

GFXD3D11Device::GFXD3D11Device()
{
   clip.set(0, 0, 800, 800);

   mTextureManager = new GFXD3D11TextureManager();
   gScreenShot = new ScreenShot();
   mCardProfiler = new GFXD3D11CardProfiler();
   mCardProfiler->init();
}

GFXD3D11Device::~GFXD3D11Device()
{
}

GFXVertexBuffer *GFXD3D11Device::allocVertexBuffer( U32 numVerts, 
                                                   const GFXVertexFormat *vertexFormat,
                                                   U32 vertSize, 
                                                   GFXBufferType bufferType ) 
{
   return new GFXD3D11VertexBuffer(GFX, numVerts, vertexFormat, vertSize, bufferType);
}

GFXPrimitiveBuffer *GFXD3D11Device::allocPrimitiveBuffer( U32 numIndices, 
                                                         U32 numPrimitives, 
                                                         GFXBufferType bufferType) 
{
   return new GFXD3D11PrimitiveBuffer(GFX, numIndices, numPrimitives, bufferType);
}

GFXCubemap* GFXD3D11Device::createCubemap()
{ 
   return new GFXD3D11Cubemap(); 
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
                vmAdd.refreshRate = mode.RefreshRate.Denominator;
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

void GFXD3D11Device::init( const GFXVideoMode &mode, PlatformWindow *window )
{
   mCardProfiler = new GFXD3D11CardProfiler();
   mCardProfiler->init();
}

GFXStateBlockRef GFXD3D11Device::createStateBlockInternal(const GFXStateBlockDesc& desc)
{
   return new GFXD3D11StateBlock();
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
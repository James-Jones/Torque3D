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

#ifndef _GFXD3D11DEVICE_H_
#define _GFXD3D11DEVICE_H_

#include "platform/platform.h"

//-----------------------------------------------------------------------------

#include "gfx/gfxDevice.h"
#include "gfx/gfxInit.h"
#include "gfx/gfxFence.h"

#ifndef _GFXD3D11PrimitiveBuffer_H_
#include "gfx/D3D11/gfxD3D11PrimitiveBuffer.h"
#endif

#ifndef _GFXD3D11_VERTEXBUFFER_H_
#include "gfx/D3D11/gfxD3D11VertexBuffer.h"
#endif

#ifndef _GFXD3D11STATEBLOCK_H_
#include "gfx/D3D11/gfxD3D11StateBlock.h"
#endif

#include <d3d11.h>
#include <DxErr.h>

#define SAFE_RELEASE(x) if(x) { x->Release(); x = NULL; } 

inline void D3D11Assert( HRESULT hr, const char *info ) 
{
#if defined( TORQUE_DEBUG )
   if( FAILED( hr ) ) 
   {
      char buf[256];
      dSprintf( buf, 256, "%s\n%s\n%s", DXGetErrorStringA( hr ), DXGetErrorDescriptionA( hr ), info );
      AssertFatal( false, buf ); 
      //      DXTrace( __FILE__, __LINE__, hr, info, true );
   }
#endif
}

class GFXD3D11WindowTarget : public GFXWindowTarget
{
public:
   virtual bool present()
   {
      return true;
   }

   virtual const Point2I getSize()
   {
      // Return something stupid.
      return Point2I(1,1);
   }

   virtual GFXFormat getFormat() { return GFXFormatR8G8B8A8; }

   virtual void resetMode()
   {

   }

   virtual void zombify() {};
   virtual void resurrect() {};

};

class GFXD3D11Device : public GFXDevice
{
public:
   GFXD3D11Device();
   virtual ~GFXD3D11Device();

   static GFXDevice *createInstance( U32 adapterIndex );

   static void enumerateAdapters( Vector<GFXAdapter*> &adapterList );

   virtual void init( const GFXVideoMode &mode, PlatformWindow *window = NULL );

   virtual void activate() { };
   virtual void deactivate() { };
   virtual GFXAdapterType getAdapterType() { return NullDevice; };

   /// @name Debug Methods
   /// @{
   virtual void enterDebugEvent(ColorI color, const char *name) { };
   virtual void leaveDebugEvent() { };
   virtual void setDebugMarker(ColorI color, const char *name) { };
   /// @}

   /// Enumerates the supported video modes of the device
   virtual void enumerateVideoModes() { };

   /// Sets the video mode for the device
   virtual void setVideoMode( const GFXVideoMode &mode ) { };
protected:

   ID3D11Device *mD3DDevice;
   IDXGISwapChain *mSwapChain;
   ID3D11RenderTargetView *mRenderTargetView;
   ID3D11DeviceContext *mImmediateContext;
   D3D_FEATURE_LEVEL mFeatureLevel;
   ID3D11Texture2D *mBackBuffer;
   DXGI_SAMPLE_DESC mMultisampleInfo;
   F32 mPixVersion;
   U32 mNumRenderTargets;

   StrongRefPtr<GFXD3D11PrimitiveBuffer> mDynamicPB;   ///< Dynamic index buffer
   GFXD3D11PrimitiveBuffer* mCurrentPB;

   typedef StrongRefPtr<GFXD3D11VertexBuffer> RPGDVB;
   Vector<RPGDVB> mVolatileVBList;

   static GFXAdapter::CreateDeviceInstanceDelegate mCreateDeviceInstance; 

   /// Called by GFXDevice to create a device specific stateblock
   virtual GFXStateBlockRef createStateBlockInternal(const GFXStateBlockDesc& desc);
   /// Called by GFXDevice to actually set a stateblock.
   virtual void setStateBlockInternal(GFXStateBlock* block, bool force);
   /// @}

   /// Called by base GFXDevice to actually set a const buffer
   virtual void setShaderConstBufferInternal(GFXShaderConstBuffer* buffer) { };

   virtual void setTextureInternal(U32 textureUnit, const GFXTextureObject*texture) { };

   virtual void setLightInternal(U32 lightStage, const GFXLightInfo light, bool lightEnable);
   virtual void setLightMaterialInternal(const GFXLightMaterial mat) { };
   virtual void setGlobalAmbientInternal(ColorF color) { };

   /// @name State Initalization.
   /// @{

   /// State initalization. This MUST BE CALLED in setVideoMode after the device
   /// is created.
   virtual void initStates() { };

   virtual void setMatrix( GFXMatrixType mtype, const MatrixF &mat ) { };


   virtual GFXVertexDecl* allocVertexDecl( const GFXVertexFormat *vertexFormat ) { return NULL; }
   virtual void setVertexDecl( const GFXVertexDecl *decl ) {  }
   virtual void setVertexStream( U32 stream, GFXVertexBuffer *buffer ) { }
   virtual void setVertexStreamFrequency( U32 stream, U32 frequency ) { }

   virtual void _setPrimitiveBuffer( GFXPrimitiveBuffer *buffer );

   virtual GFXD3D11VertexBuffer* findVBPool( const GFXVertexFormat *vertexFormat, U32 numVertsNeeded );
   virtual GFXD3D11VertexBuffer* createVBPool( const GFXVertexFormat *vertexFormat, U32 vertSize );

   /// Device helper function
   DXGI_SWAP_CHAIN_DESC setupSwapChainParams( const GFXVideoMode &mode, const HWND &hWnd ) const;

public:
   virtual GFXCubemap * createCubemap();

   virtual ID3D11Device* getDevice(){ return mD3DDevice; }
   virtual ID3D11DeviceContext* getDeviceContext(){ return mImmediateContext; }

   virtual F32 getFillConventionOffset() const { return 0.0f; };

   ///@}

   virtual GFXTextureTarget *allocRenderToTextureTarget(){return NULL;};
   virtual GFXWindowTarget *allocWindowTarget(PlatformWindow *window);

   virtual void _updateRenderTargets(){};

   virtual F32 getPixelShaderVersion() const { return mPixVersion; };
   virtual void setPixelShaderVersion( F32 version ) { mPixVersion = version;};
   virtual U32 getNumSamplers() const { return 16; };
   virtual U32 getNumRenderTargets() const { return mNumRenderTargets; };

   virtual GFXShader* createShader() { return NULL; };


   virtual void clear( U32 flags, ColorI color, F32 z, U32 stencil ) { };
   virtual bool beginSceneInternal() { return true; };
   virtual void endSceneInternal() { };

   virtual void drawPrimitive( GFXPrimitiveType primType, U32 vertexStart, U32 primitiveCount );
   virtual void drawIndexedPrimitive(  GFXPrimitiveType primType, 
                                       U32 startVertex, 
                                       U32 minIndex, 
                                       U32 numVerts, 
                                       U32 startIndex, 
                                       U32 primitiveCount );

   virtual void setClipRect( const RectI &rect ) { };
   virtual const RectI &getClipRect() const { return clip; };

   virtual void preDestroy() { Parent::preDestroy(); };

   virtual U32 getMaxDynamicVerts() { return 16384; };
   virtual U32 getMaxDynamicIndices() { return 16384; };

   virtual GFXFormat selectSupportedFormat(  GFXTextureProfile *profile, 
                                             const Vector<GFXFormat> &formats, 
                                             bool texture, 
                                             bool mustblend, 
                                             bool mustfilter );

   virtual GFXPrimitiveBuffer *allocPrimitiveBuffer(  U32 numIndices, 
                                                      U32 numPrimitives, 
                                                      GFXBufferType bufferType );
   virtual GFXVertexBuffer *allocVertexBuffer(  U32 numVerts, 
                                                const GFXVertexFormat *vertexFormat, 
                                                U32 vertSize, 
                                                GFXBufferType bufferType );
   virtual void deallocVertexBuffer( GFXD3D11VertexBuffer *vertBuff );

   virtual void destroyD3DResource( ID3D11Resource *d3dResource ) { SAFE_RELEASE( d3dResource ); }; 

   GFXFence *createFence();
   GFXOcclusionQuery* createOcclusionQuery();

   DXGI_SAMPLE_DESC getMultisampleInfo() const { return mMultisampleInfo; }
   
private:
   friend class GFXD3D11PrimitiveBuffer;
   friend class GFXD3D11VertexBuffer;
   typedef GFXDevice Parent;
   RectI clip;
};

#endif

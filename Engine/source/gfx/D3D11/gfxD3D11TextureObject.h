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

#ifndef _GFXD3D11TEXTUREOBJECT_H_
#define _GFXD3D11TEXTUREOBJECT_H_

#ifndef _GFXTEXTUREHANDLE_H_
#include "gfx/gfxTextureHandle.h"
#endif

#ifndef _GFXTEXTUREMANAGER_H_
#include "gfx/gfxTextureManager.h"
#endif


class GFXD3D11TextureObject : public GFXTextureObject
{
protected:
   static U32 mTexCount;
   GFXTexHandle   mLockTex;
	GFXLockedRect mLockRect;
	U32 mLockedSubresource;
   bool           mLocked;

   ID3D11Resource *mD3DTexture;

	GFXD3D11Device* mD3DDevice;

   // used for z buffers...
   //IDirect3DSurface9 *mD3DSurface;

public:

   GFXD3D11TextureObject( GFXDevice * d, GFXTextureProfile *profile);
   ~GFXD3D11TextureObject();

   ID3D11Resource *    getTex(){ return mD3DTexture; }
   ID3D11Texture2D *        get2DTex(){ return (ID3D11Texture2D*) mD3DTexture; }
   ID3D11Texture2D **       get2DTexPtr(){ return (ID3D11Texture2D**) &mD3DTexture; }
   ID3D11Texture3D *  get3DTex(){ return (ID3D11Texture3D*) mD3DTexture; }
   ID3D11Texture3D ** get3DTexPtr(){ return (ID3D11Texture3D**) &mD3DTexture; }

   void release();

   bool isManaged;

   virtual GFXLockedRect * lock(U32 mipLevel = 0, RectI *inRect = NULL);
   virtual void unlock(U32 mipLevel = 0 );

   virtual bool copyToBmp(GBitmap* bmp);
   
   //IDirect3DSurface9 *getSurface() {return mD3DSurface;}
   //IDirect3DSurface9 **getSurfacePtr() {return &mD3DSurface;}

   // GFXResource
   void zombify();
   void resurrect();

#ifdef TORQUE_DEBUG
   virtual void pureVirtualCrash() {};
#endif
};


#endif

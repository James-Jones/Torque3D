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
#ifndef _GFXD3D11STATEBLOCK_H_
#define _GFXD3D11STATEBLOCK_H_

#ifndef _GFXSTATEBLOCK_H_
#include "gfx/gfxStateBlock.h"
#endif

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11BlendState;
struct ID3D11RasterizerState;
struct ID3D11SamplerState;
struct ID3D11DepthStencilState;

class GFXD3D11StateBlock : public GFXStateBlock
{   
public:
   // 
   // GFXD3D11StateBlock interface
   //

   GFXD3D11StateBlock(const GFXStateBlockDesc& desc, ID3D11Device* device, ID3D11DeviceContext* context);
   virtual ~GFXD3D11StateBlock();

   /// Called by D3D11 device to active this state block.
   /// @param oldState  The current state, used to make sure we don't set redundant states on the device.  Pass NULL to reset all states.
   void activate(GFXD3D11StateBlock* oldState);


   // 
   // GFXStateBlock interface
   //

   /// Returns the hash value of the desc that created this block
   virtual U32 getHashValue() const;

   /// Returns a GFXStateBlockDesc that this block represents
   virtual const GFXStateBlockDesc& getDesc() const;

   //
   // GFXResource
   //
   virtual void zombify() { }
   /// When called the resource should restore all device sensitive information destroyed by zombify()
   virtual void resurrect() { }
private:
   GFXStateBlockDesc mDesc;
   U32 mCachedHashValue;
   ID3D11Device *mD3DDevice;  ///< Handle for D3DDevice
   ID3D11DeviceContext* mD3DDeviceContext;
   // Cached D3D specific things, these are "calculated" from GFXStateBlock
   U32 mColorMask;

   ID3D11BlendState* mBlendState;
   void CreateD3DBlendState();

   ID3D11RasterizerState* mRastState;
   void CreateD3DRasterizerState();

   ID3D11SamplerState* mSamplerState[TEXTURE_STAGE_COUNT];
   void CreateD3DSamplerState();

   ID3D11DepthStencilState* mDepthStencilState;
   void CreateD3DDepthStencilState();
};

typedef StrongRefPtr<GFXD3D11StateBlock> GFXD3D11StateBlockRef;

#endif
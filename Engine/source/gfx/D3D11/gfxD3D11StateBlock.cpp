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

#include "gfx/gfxDevice.h"
#include <D3D11.h>
#include "gfx/D3D11/gfxD3D11StateBlock.h"
#include "console/console.h"

#define SAFE_RELEASE(x) if(x) { x->Release(); x = NULL; } 

GFXD3D11StateBlock::GFXD3D11StateBlock(const GFXStateBlockDesc& desc, ID3D11Device* device, ID3D11DeviceContext* context)
{
   AssertFatal(device, "Invalid mD3DDevice!");

   mDesc = desc;
   mCachedHashValue = desc.getHashValue();
   mD3DDevice = device;
   mD3DDeviceContext = context;

   // Color writes
   mColorMask = 0; 
   mColorMask |= ( mDesc.colorWriteRed   ? D3D11_COLOR_WRITE_ENABLE_RED   : 0 );
   mColorMask |= ( mDesc.colorWriteGreen ? D3D11_COLOR_WRITE_ENABLE_GREEN : 0 );
   mColorMask |= ( mDesc.colorWriteBlue  ? D3D11_COLOR_WRITE_ENABLE_BLUE  : 0 );
   mColorMask |= ( mDesc.colorWriteAlpha ? D3D11_COLOR_WRITE_ENABLE_ALPHA : 0 );

   if(mDesc.alphaTestEnable == true)
   {
      Con::warnf("Alpha test not supported in D3D11.");
   }

   CreateD3DBlendState();
   CreateD3DRasterizerState();
   CreateD3DSamplerState();
   CreateD3DDepthStencilState();
}

GFXD3D11StateBlock::~GFXD3D11StateBlock()
{
   SAFE_RELEASE(mBlendState);
   SAFE_RELEASE(mRastState);
   SAFE_RELEASE(mDepthStencilState);

   for(int i = 0; i < TEXTURE_STAGE_COUNT; ++i)
   {
      SAFE_RELEASE(mSamplerState[i]);
   }
}

static D3D11_BLEND GetBlend(GFXBlend gfxBlend)
{
   switch(gfxBlend)
   {
      case GFXBlendZero:
      {
         return D3D11_BLEND_ZERO;
      }
      case GFXBlendOne:
      {
         return D3D11_BLEND_ONE;
      }
      case GFXBlendSrcColor:
      {
         return D3D11_BLEND_SRC_COLOR;
      }
      case GFXBlendInvSrcColor:
      {
         return D3D11_BLEND_INV_SRC_COLOR;
      }
      case GFXBlendSrcAlpha:
      {
         return D3D11_BLEND_SRC_ALPHA;
      }
      case GFXBlendInvSrcAlpha:
      {
         return D3D11_BLEND_INV_SRC_ALPHA;
      }
      case GFXBlendDestAlpha:
      {
         return D3D11_BLEND_DEST_ALPHA;
      }
      case GFXBlendInvDestAlpha:
      {
         return D3D11_BLEND_INV_DEST_ALPHA;
      }
      case GFXBlendDestColor:
      {
         return D3D11_BLEND_DEST_COLOR;
      }
      case GFXBlendInvDestColor:
      {
         return D3D11_BLEND_INV_DEST_COLOR;
      }
   }

   return D3D11_BLEND_ZERO;
}
static D3D11_BLEND_OP GetBlendOp(GFXBlendOp gfxBlendOp)
{
   switch(gfxBlendOp)
   {
      case GFXBlendOpAdd:
         return D3D11_BLEND_OP_ADD;
      case GFXBlendOpSubtract:
         return D3D11_BLEND_OP_SUBTRACT;
      case GFXBlendOpRevSubtract:
         return D3D11_BLEND_OP_REV_SUBTRACT;
      case GFXBlendOpMin:
         return D3D11_BLEND_OP_MIN;
      case GFXBlendOpMax:
         return D3D11_BLEND_OP_MAX;
   }
	return D3D11_BLEND_OP_ADD;
}

static D3D11_COMPARISON_FUNC GetCompFunc(GFXCmpFunc gfxCmp)
{
   switch(gfxCmp)
   {
      case GFXCmpNever:
         return D3D11_COMPARISON_NEVER;
      case GFXCmpLess:
         return D3D11_COMPARISON_LESS;
      case GFXCmpEqual:
         return D3D11_COMPARISON_EQUAL;
      case GFXCmpLessEqual:
         return D3D11_COMPARISON_LESS_EQUAL;
      case GFXCmpGreater:
         return D3D11_COMPARISON_GREATER;
      case GFXCmpNotEqual:
         return D3D11_COMPARISON_NOT_EQUAL;
      case GFXCmpGreaterEqual:
         return D3D11_COMPARISON_GREATER_EQUAL;
      case GFXCmpAlways:
         return D3D11_COMPARISON_ALWAYS;
   }
	return D3D11_COMPARISON_NEVER;
}

static D3D11_STENCIL_OP GetStencilOp(GFXStencilOp gfxStencilOp)
{
   switch(gfxStencilOp)
   {
      case GFXStencilOpKeep:
         return D3D11_STENCIL_OP_KEEP;
      case GFXStencilOpZero:
         return D3D11_STENCIL_OP_ZERO;
      case GFXStencilOpReplace:
         return D3D11_STENCIL_OP_REPLACE;
      case GFXStencilOpIncrSat:
         return D3D11_STENCIL_OP_INCR_SAT;
      case GFXStencilOpDecrSat:
         return D3D11_STENCIL_OP_DECR_SAT;
      case GFXStencilOpInvert:
         return D3D11_STENCIL_OP_INVERT;
      case GFXStencilOpIncr:
         return D3D11_STENCIL_OP_INCR;
      case GFXStencilOpDecr:
         return D3D11_STENCIL_OP_DECR;
   }
	return D3D11_STENCIL_OP_KEEP;
}

void GFXD3D11StateBlock::CreateD3DBlendState()
{
   D3D11_BLEND_DESC sBlendDesc;
   sBlendDesc.AlphaToCoverageEnable = FALSE;
   sBlendDesc.IndependentBlendEnable = FALSE;

   D3D11_RENDER_TARGET_BLEND_DESC sRTBlendDesc;
   sRTBlendDesc.BlendEnable =  mDesc.blendEnable;

   sRTBlendDesc.SrcBlend = GetBlend(mDesc.blendSrc);
   sRTBlendDesc.DestBlend = GetBlend(mDesc.blendDest);

   sRTBlendDesc.BlendOp = GetBlendOp(mDesc.blendOp);

   sRTBlendDesc.RenderTargetWriteMask = mColorMask;

   sRTBlendDesc.SrcBlendAlpha = D3D11_BLEND_ONE;
   sRTBlendDesc.DestBlendAlpha = D3D11_BLEND_ZERO;
   sRTBlendDesc.BlendOpAlpha = D3D11_BLEND_OP_ADD;

   if(mDesc.separateAlphaBlendEnable)
   {
      sRTBlendDesc.SrcBlendAlpha = GetBlend(mDesc.separateAlphaBlendSrc);
      sRTBlendDesc.DestBlendAlpha = GetBlend(mDesc.separateAlphaBlendDest);
      sRTBlendDesc.BlendOpAlpha = GetBlendOp(mDesc.separateAlphaBlendOp);
   }

	sBlendDesc.RenderTarget[0] = sRTBlendDesc;

	mD3DDevice->CreateBlendState(&sBlendDesc, &mBlendState);
}

void GFXD3D11StateBlock::CreateD3DRasterizerState()
{
   D3D11_RASTERIZER_DESC sRastDesc;
   sRastDesc.FillMode = D3D11_FILL_SOLID;
   sRastDesc.CullMode = D3D11_CULL_BACK;
   sRastDesc.FrontCounterClockwise = FALSE;
   sRastDesc.DepthBias = mDesc.zBias;
   sRastDesc.SlopeScaledDepthBias = mDesc.zSlopeBias;
   sRastDesc.DepthBiasClamp = 0.0f;
   sRastDesc.DepthClipEnable = TRUE;
   sRastDesc.ScissorEnable = FALSE;
   sRastDesc.MultisampleEnable = FALSE;
   sRastDesc.AntialiasedLineEnable = FALSE;

   if(mDesc.fillMode != GFXFillSolid)
   {
      sRastDesc.FillMode = D3D11_FILL_WIREFRAME;
   }

   if(mDesc.cullMode == GFXCullCW)
   {
      sRastDesc.CullMode = D3D11_CULL_FRONT;
   }

   mD3DDevice->CreateRasterizerState(&sRastDesc, &mRastState);
}

void GFXD3D11StateBlock::CreateD3DSamplerState()
{
   D3D11_SAMPLER_DESC sSamplerDesc;
   int i = 0;

   for(; i < TEXTURE_STAGE_COUNT; ++i)
   {
      mSamplerState[i] = NULL;
   }

   mD3DDevice->CreateSamplerState(&sSamplerDesc, &mSamplerState[0]);
}

void GFXD3D11StateBlock::CreateD3DDepthStencilState()
{
   D3D11_DEPTH_STENCIL_DESC sDepthStencilDesc;
   sDepthStencilDesc.DepthEnable = mDesc.zEnable;
   sDepthStencilDesc.DepthWriteMask = mDesc.zWriteEnable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
   sDepthStencilDesc.DepthFunc = GetCompFunc(mDesc.zFunc);
   sDepthStencilDesc.StencilEnable = mDesc.stencilEnable;
   sDepthStencilDesc.StencilReadMask = mDesc.stencilMask;
   sDepthStencilDesc.StencilWriteMask = mDesc.stencilWriteMask;
   sDepthStencilDesc.FrontFace.StencilFunc = GetCompFunc(mDesc.stencilFunc);
   sDepthStencilDesc.FrontFace.StencilDepthFailOp = GetStencilOp(mDesc.stencilZFailOp);
   sDepthStencilDesc.FrontFace.StencilPassOp = GetStencilOp(mDesc.stencilPassOp);
   sDepthStencilDesc.FrontFace.StencilFailOp = GetStencilOp(mDesc.stencilFailOp);


   sDepthStencilDesc.BackFace.StencilFunc = sDepthStencilDesc.FrontFace.StencilFunc;
   sDepthStencilDesc.BackFace.StencilDepthFailOp = sDepthStencilDesc.FrontFace.StencilDepthFailOp;
   sDepthStencilDesc.BackFace.StencilPassOp = sDepthStencilDesc.FrontFace.StencilPassOp;
   sDepthStencilDesc.BackFace.StencilFailOp = sDepthStencilDesc.FrontFace.StencilFailOp;

   mD3DDevice->CreateDepthStencilState(&sDepthStencilDesc, &mDepthStencilState);
}

/// Returns the hash value of the desc that created this block
U32 GFXD3D11StateBlock::getHashValue() const
{
   return mCachedHashValue;
}

/// Returns a GFXStateBlockDesc that this block represents
const GFXStateBlockDesc& GFXD3D11StateBlock::getDesc() const
{
   return mDesc;
}

/// Called by D3D11 device to active this state block.
/// @param oldState  The current state, used to make sure we don't set redundant states on the device.  Pass NULL to reset all states.
void GFXD3D11StateBlock::activate(GFXD3D11StateBlock* oldState)
{
   PROFILE_SCOPE( GFXD3D11StateBlock_Activate );

   // Blending
   if (!oldState ||
      oldState->mDesc.blendEnable != mDesc.blendEnable ||
      oldState->mDesc.blendSrc != mDesc.blendSrc ||
      oldState->mDesc.blendDest != mDesc.blendDest ||
      oldState->mDesc.blendOp != mDesc.blendOp ||
      mColorMask != oldState->mColorMask ||
      oldState->mDesc.separateAlphaBlendEnable != mDesc.separateAlphaBlendEnable ||
      oldState->mDesc.separateAlphaBlendSrc != mDesc.separateAlphaBlendSrc ||
      oldState->mDesc.separateAlphaBlendDest != mDesc.separateAlphaBlendDest ||
      oldState->mDesc.separateAlphaBlendOp != mDesc.separateAlphaBlendOp)
   {
      mD3DDeviceContext->OMSetBlendState(mBlendState, NULL, 0xFFFFFFFF);
   }

   mD3DDeviceContext->OMSetDepthStencilState(mDepthStencilState, mDesc.stencilRef);


#if 0
   static DWORD swzTemp;
   getOwningDevice()->getDeviceSwizzle32()->ToBuffer( &swzTemp, &mDesc.textureFactor, sizeof(ColorI) );
   SDD(textureFactor, D3DRS_TEXTUREFACTOR, swzTemp);

#undef SD
#undef SDD


   // NOTE: Samplers and Stages are different things.
   //
   // The Stages were for fixed function blending.  When using shaders
   // calling SetTextureStageState() is a complete waste of time.  In
   // fact if this function rises to the top of profiles we should
   // refactor stateblocks to seperate the two.
   //
   // Samplers are used by both fixed function and shaders, but the
   // number of samplers is limited by shader model.

   #define TSS(x, y, z) if (!oldState || oldState->mDesc.samplers[i].x != mDesc.samplers[i].x) \
                        mD3DDevice->SetTextureStageState(i, y, z)
   for ( U32 i = 0; i < 8; i++ )
   {   
      TSS(textureColorOp, D3DTSS_COLOROP, GFXD3D11TextureOp[mDesc.samplers[i].textureColorOp]);
      TSS(colorArg1, D3DTSS_COLORARG1, mDesc.samplers[i].colorArg1);
      TSS(colorArg2, D3DTSS_COLORARG2, mDesc.samplers[i].colorArg2);
      TSS(colorArg3, D3DTSS_COLORARG0, mDesc.samplers[i].colorArg3);
      TSS(alphaOp, D3DTSS_ALPHAOP, GFXD3D11TextureOp[mDesc.samplers[i].alphaOp]);
      TSS(alphaArg1, D3DTSS_ALPHAARG1, mDesc.samplers[i].alphaArg1);
      TSS(alphaArg2, D3DTSS_ALPHAARG2, mDesc.samplers[i].alphaArg2);
      TSS(alphaArg3, D3DTSS_ALPHAARG0, mDesc.samplers[i].alphaArg3);
      TSS(textureTransform, D3DTSS_TEXTURETRANSFORMFLAGS, mDesc.samplers[i].textureTransform);
      TSS(resultArg, D3DTSS_RESULTARG, mDesc.samplers[i].resultArg);
   }
   #undef TSS

   #define SS(x, y, z)  if (!oldState || oldState->mDesc.samplers[i].x != mDesc.samplers[i].x) \
                        mD3DDevice->SetSamplerState(i, y, z)

   for ( U32 i = 0; i < getOwningDevice()->getNumSamplers(); i++ )
   {      
      SS(minFilter, D3DSAMP_MINFILTER, GFXD3D11TextureFilter[mDesc.samplers[i].minFilter]);
      SS(magFilter, D3DSAMP_MAGFILTER, GFXD3D11TextureFilter[mDesc.samplers[i].magFilter]);
      SS(mipFilter, D3DSAMP_MIPFILTER, GFXD3D11TextureFilter[mDesc.samplers[i].mipFilter]);

      F32 bias = mDesc.samplers[i].mipLODBias;
      DWORD dwBias = *( (LPDWORD)(&bias) );
      SS(mipLODBias, D3DSAMP_MIPMAPLODBIAS, dwBias);

      SS(maxAnisotropy, D3DSAMP_MAXANISOTROPY, mDesc.samplers[i].maxAnisotropy);

      SS(addressModeU, D3DSAMP_ADDRESSU, GFXD3D11TextureAddress[mDesc.samplers[i].addressModeU]);
      SS(addressModeV, D3DSAMP_ADDRESSV, GFXD3D11TextureAddress[mDesc.samplers[i].addressModeV]);
      SS(addressModeW, D3DSAMP_ADDRESSW, GFXD3D11TextureAddress[mDesc.samplers[i].addressModeW]);
   }
   #undef SS
#endif
}

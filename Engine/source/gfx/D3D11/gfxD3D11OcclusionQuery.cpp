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

#include "gfx/D3D11/gfxD3D11Device.h"
#include "gfx/D3D11/gfxD3D11OcclusionQuery.h"

#include "gui/3d/guiTSControl.h"

#ifdef TORQUE_GATHER_METRICS
// For TickMs define
#include "T3D/gameBase/processList.h"
#endif

GFXD3D11OcclusionQuery::GFXD3D11OcclusionQuery( GFXDevice *device )
 : GFXOcclusionQuery( device ), 
   mQuery( NULL )   
{
#ifdef TORQUE_GATHER_METRICS
   mTimer = PlatformTimer::create();
   mTimer->getElapsedMs();

   mTimeSinceEnd = 0;
   mBeginFrame = 0;
#endif
}

GFXD3D11OcclusionQuery::~GFXD3D11OcclusionQuery()
{
   SAFE_RELEASE( mQuery );

#ifdef TORQUE_GATHER_METRICS
   SAFE_DELETE( mTimer );
#endif
}

bool GFXD3D11OcclusionQuery::begin()
{
   if ( GFXDevice::getDisableOcclusionQuery() )
      return true;

   if ( mQuery == NULL )
   {
       D3D11_QUERY_DESC sQueryDesc;
       sQueryDesc.Query = D3D11_QUERY_OCCLUSION;
       sQueryDesc.MiscFlags = 0;

#ifdef TORQUE_OS_XENON
        HRESULT hRes = D3DERR_NOTAVAILABLE;
#else
      HRESULT hRes = static_cast<GFXD3D11Device*>( mDevice )->getDevice()->CreateQuery( &sQueryDesc, &mQuery );
#endif

      //AssertFatal( hRes != D3DERR_NOTAVAILABLE, "GFXD3D11OcclusionQuery::begin - Hardware does not support Occlusion-Queries, this should be caught before this type is created" );
      AssertISV( hRes != E_OUTOFMEMORY, "GFXD3D11OcclusionQuery::begin - Out of memory" );
   }

   // Add a begin marker to the command buffer queue.
   static_cast<GFXD3D11Device*>( mDevice )->getDeviceContext()->Begin(mQuery);

#ifdef TORQUE_GATHER_METRICS
   mBeginFrame = GuiTSCtrl::getFrameCount();
#endif

   return true;
}

void GFXD3D11OcclusionQuery::end()
{
   if ( GFXDevice::getDisableOcclusionQuery() )
      return;

   // Add an end marker to the command buffer queue.
   static_cast<GFXD3D11Device*>( mDevice )->getDeviceContext()->End(mQuery);

#ifdef TORQUE_GATHER_METRICS
   AssertFatal( mBeginFrame == GuiTSCtrl::getFrameCount(), "GFXD3D9OcclusionQuery::end - ended query on different frame than begin!" );   
   mTimer->getElapsedMs();
   mTimer->reset();
#endif
}

GFXD3D11OcclusionQuery::OcclusionQueryStatus GFXD3D11OcclusionQuery::getStatus( bool block, U32 *data )
{
   // If this ever shows up near the top of a profile then your system is 
   // GPU bound or you are calling getStatus too soon after submitting it.
   //
   // To test if you are GPU bound resize your window very small and see if
   // this profile no longer appears at the top.
   //
   // To test if you are calling getStatus to soon after submitting it,
   // check the value of mTimeSinceEnd in a debug build. If it is < half the length
   // of time to render an individual frame you could have problems.
   PROFILE_SCOPE(GFXD3D11OcclusionQuery_getStatus);

   if ( GFXDevice::getDisableOcclusionQuery() )
      return NotOccluded;

   if ( mQuery == NULL )
      return Unset;

#ifdef TORQUE_GATHER_METRICS
   AssertFatal( mBeginFrame < GuiTSCtrl::getFrameCount(), "GFXD3D11OcclusionQuery::getStatus - called on the same frame as begin!" );

   //U32 mTimeSinceEnd = mTimer->getElapsedMs();
   //AssertFatal( mTimeSinceEnd >= 5, "GFXD3DOcculsionQuery::getStatus - less than TickMs since called ::end!" );
#endif

   HRESULT hRes;
   DWORD dwOccluded = 0;

   ID3D11DeviceContext* ctx = static_cast<GFXD3D11Device*>( mDevice )->getDeviceContext();

   if ( block )
   {      
      while( ( hRes = ctx->GetData(mQuery, &dwOccluded, sizeof(DWORD), 0 ) ) == S_FALSE )
         ;
   }
   else
   {
      hRes = ctx->GetData(mQuery, &dwOccluded, sizeof(DWORD), D3D11_ASYNC_GETDATA_DONOTFLUSH );
   }

   if ( hRes == S_OK )
   {
      if ( data != NULL )
         *data = dwOccluded;

      return dwOccluded > 0 ? NotOccluded : Occluded;
   }

   if ( hRes == S_FALSE )
      return Waiting;

   return Error;
}

void GFXD3D11OcclusionQuery::zombify()
{
   SAFE_RELEASE( mQuery );
}

void GFXD3D11OcclusionQuery::resurrect()
{
}

const String GFXD3D11OcclusionQuery::describeSelf() const
{
   // We've got nothing
   return String();
}
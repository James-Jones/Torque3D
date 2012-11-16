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
#include "gfx/D3D11/gfxD3D11Device.h"
#include "gfx/D3D11/gfxD3D11VertexBuffer.h"

GFXD3D11VertexBuffer::~GFXD3D11VertexBuffer() 
{
#ifdef TORQUE_DEBUG
   SAFE_DELETE( name );
#endif

   if (getOwningDevice() != NULL)
   {
      if (mBufferType == GFXBufferTypeDynamic)
         static_cast<GFXD3D11Device *>(getOwningDevice())->deallocVertexBuffer( this );
      else if (mBufferType != GFXBufferTypeVolatile)
      {
         static_cast<GFXD3D11Device *>(getOwningDevice())->destroyD3DResource( vb );
      }
   }
}

//-----------------------------------------------------------------------------
// Lock
//-----------------------------------------------------------------------------
void GFXD3D11VertexBuffer::lock(U32 vertexStart, U32 vertexEnd, void **vertexPtr)
{
   PROFILE_SCOPE(GFXD3D11VertexBuffer_lock);

   AssertFatal(lockedVertexStart == 0 && lockedVertexEnd == 0, "Cannot lock a buffer more than once!");

   D3D11_MAP mapType = D3D11_MAP_WRITE;


   GFXD3D11Device *d = static_cast<GFXD3D11Device *>( mDevice );

   switch( mBufferType )
   {
   case GFXBufferTypeStatic:
      mapType = D3D11_MAP_WRITE;
      break;

   case GFXBufferTypeDynamic:
      // Always discard the content within a locked region.
      mapType = D3D11_MAP_WRITE_DISCARD;
      break;

   case GFXBufferTypeVolatile:

      // Get or create the volatile buffer...
      mVolatileBuffer = d->findVBPool( &mVertexFormat, vertexEnd );

      if( !mVolatileBuffer )
         mVolatileBuffer = d->createVBPool( &mVertexFormat, mVertexSize );

      vb = mVolatileBuffer->vb;

      // Get our range now...
      AssertFatal(vertexStart == 0,              "Cannot get a subrange on a volatile buffer.");
      AssertFatal(vertexEnd <= MAX_DYNAMIC_VERTS, "Cannot get more than MAX_DYNAMIC_VERTS in a volatile buffer. Up the constant!");
      AssertFatal(mVolatileBuffer->lockedVertexStart == 0 && mVolatileBuffer->lockedVertexEnd == 0, "Got more than one lock on the volatile pool.");

      // We created the pool when we requested this volatile buffer, so assume it exists...
      if( mVolatileBuffer->mNumVerts + vertexEnd > MAX_DYNAMIC_VERTS ) 
      {
         mapType = D3D11_MAP_WRITE_DISCARD;

         mVolatileStart = vertexStart  = 0;
         vertexEnd      = vertexEnd;
      }
      else 
      {
         mapType = D3D11_MAP_WRITE_NO_OVERWRITE;

         mVolatileStart = vertexStart  = mVolatileBuffer->mNumVerts;
         vertexEnd                    += mVolatileBuffer->mNumVerts;
      }

      mVolatileBuffer->mNumVerts = vertexEnd+1;

      mVolatileBuffer->lockedVertexStart = vertexStart;
      mVolatileBuffer->lockedVertexEnd   = vertexEnd;
      break;
   }

   lockedVertexStart = vertexStart;
   lockedVertexEnd   = vertexEnd;

   U32 sizeToLock = (vertexEnd - vertexStart) * mVertexSize;

   D3D11_MAPPED_SUBRESOURCE MappedResource;

   D3D11Assert( d->getDeviceContext()->Map( vb, 0, mapType, 0, &MappedResource ),
      "Unable to lock vertex buffer.");

   //Map from vertexStart * mVertexSize), length sizeToLock
   *vertexPtr = (char*)MappedResource.pData + vertexStart * mVertexSize;

   #ifdef TORQUE_DEBUG
   
      // Allocate a debug buffer large enough for the lock
      // plus space for over and under run guard strings.
      const U32 guardSize = sizeof( _VBGuardString );
      mDebugGuardBuffer = new U8[sizeToLock+(guardSize*2)];

      // Setup the guard strings.
      dMemcpy( mDebugGuardBuffer, _VBGuardString, guardSize ); 
      dMemcpy( mDebugGuardBuffer + sizeToLock + guardSize, _VBGuardString, guardSize ); 

      // Store the real lock pointer and return our debug pointer.
      mLockedBuffer = *vertexPtr;
      *vertexPtr = mDebugGuardBuffer + guardSize;

   #endif // TORQUE_DEBUG
}

void GFXD3D11VertexBuffer::unlock()
{
   PROFILE_SCOPE(GFXD3D11VertexBuffer_unlock);

   #ifdef TORQUE_DEBUG
   
      if ( mDebugGuardBuffer )
      {
         const U32 guardSize = sizeof( _VBGuardString );
         const U32 sizeLocked = (lockedVertexEnd - lockedVertexStart) * mVertexSize;

         // First check the guard areas for overwrites.
         AssertFatal( dMemcmp( mDebugGuardBuffer, _VBGuardString, guardSize ) == 0,
            "GFXD3D11VertexBuffer::unlock - Caught lock memory underrun!" );
         AssertFatal( dMemcmp( mDebugGuardBuffer + sizeLocked + guardSize, _VBGuardString, guardSize ) == 0,
            "GFXD3D11VertexBuffer::unlock - Caught lock memory overrun!" );
                        
         // Copy the debug content down to the real VB.
         dMemcpy( mLockedBuffer, mDebugGuardBuffer + guardSize, sizeLocked );

         // Cleanup.
         delete [] mDebugGuardBuffer;
         mDebugGuardBuffer = NULL;
         mLockedBuffer = NULL;
      }

   #endif // TORQUE_DEBUG

   ((GFXD3D11Device*)mDevice)->getDeviceContext()->Unmap( vb, 0);

   mIsFirstLock = false;

   lockedVertexEnd = lockedVertexStart = 0;

   if(mVolatileBuffer.isValid())
   {
      mVolatileBuffer->lockedVertexStart = 0;
      mVolatileBuffer->lockedVertexEnd   = 0;
      mVolatileBuffer = NULL;
   }
}

void GFXD3D11VertexBuffer::zombify()
{
   AssertFatal(lockedVertexStart == 0 && lockedVertexEnd == 0, "GFXD3D11VertexBuffer::zombify - Cannot zombify a locked buffer!");
   // Static buffers are managed by D3D11 so we don't deal with them.
   if(mBufferType == GFXBufferTypeDynamic)
   {
      SAFE_RELEASE(vb);
   }
}

void GFXD3D11VertexBuffer::resurrect()
{
   // Static buffers are managed by D3D so we don't deal with them.
   if(mBufferType == GFXBufferTypeDynamic)
   {
#if 0
      D3D11Assert(static_cast<GFXD3D11Device*>(mDevice)->mD3DDevice->CreateVertexBuffer( mVertexSize * mNumVerts,
#ifdef TORQUE_OS_XENON
         D3DUSAGE_WRITEONLY,
#else
         D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 
#endif
         0, 
         D3DPOOL_DEFAULT,
         &vb,
         NULL ),
         "GFXD3D11VertexBuffer::resurrect - Failed to allocate VB" );
#endif
	AssertFatal(false, "GFXD3D11VertexBuffer::resurrect - Not implemented!");
   }
}


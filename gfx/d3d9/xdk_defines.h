/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _XDK_DEFINES_H
#define _XDK_DEFINES_H


#if defined(_XBOX1)
/* XBox 1*/

#define SetSamplerState_function(device, sampler, type, value) \
 D3D__DirtyFlags |= (D3DDIRTYFLAG_TEXTURE_STATE_0 << sampler); \
 D3D__TextureState[sampler][type] = value

#define RD3DDevice_SetTransform(device, State, pMatrix) D3DDevice_SetTransform(State, pMatrix)

#define RD3DDevice_SetVertexShader(device, handle) D3DDevice_SetVertexShader(handle)
#define RD3DVertexBuffer_Lock(device, OffsetToLock, SizeToLock, ppbData, Flags) *ppbData = D3DVertexBuffer_Lock2(device, Flags) + OffsetToLock
#define RD3DVertexBuffer_Unlock(device)
#define RD3DDevice_SetTexture(device, Stage, pTexture) D3DDevice_SetTexture(Stage, pTexture)
#define RD3DDevice_Clear(device, Count, pRects, Flags, Color, Z, Stencil) D3DDevice_Clear(Count, pRects, Flags, Color, Z, Stencil)
#define D3DDevice_CreateVertexBuffers(device, Length, Usage, UnusedFVF, UnusedPool, ppVertexBuffer, pUnusedSharedHandle) IDirect3DDevice8_CreateVertexBuffer(device, Length, Usage, UnusedFVF, UnusedPool, ppVertexBuffer)

#elif defined(_XBOX360)
/* XBox 360*/

#define RD3DVertexBuffer_Lock(device, OffsetToLock, SizeToLock, ppbData, Flags) *ppbData = D3DVertexBuffer_Lock(device, OffsetToLock, SizeToLock, Flags)
#define RD3DVertexBuffer_Unlock(device) D3DVertexBuffer_Unlock(device)
#define RD3DDevice_SetTexture(device, Stage, pTexture) \
 fetchConstant = GPU_CONVERT_D3D_TO_HARDWARE_TEXTUREFETCHCONSTANT(Stage); \
 pendingMask3 = D3DTAG_MASKENCODE(D3DTAG_START(D3DTAG_FETCHCONSTANTS) + fetchConstant, D3DTAG_START(D3DTAG_FETCHCONSTANTS) + fetchConstant); \
 D3DDevice_SetTexture(device, Stage, pTexture, pendingMask3)

#define D3DDevice_CreateVertexBuffers(device, Length, Usage, UnusedFVF, UnusedPool, ppVertexBuffer, pUnusedSharedHandle) IDirect3DDevice9_CreateVertexBuffer(device, Length, Usage, UnusedFVF, UnusedPool, ppVertexBuffer, NULL)
#define RD3DDevice_Clear(device, Count, pRects, Flags, Color, Z, Stencil) D3DDevice_Clear(device, Count, pRects, Flags, Color, Z, Stencil, false)
#endif

#define D3DTexture_LockRectClear(pass, tex, level, lockedrect, rect, flags) \
   D3DTexture_LockRect(tex, level, &lockedrect, rect, flags); \
   memset(lockedrect.pBits, 0, pass->tex_h * lockedrect.Pitch)

#endif

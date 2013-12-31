/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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
#define LPDIRECT3DRESOURCE LPDIRECT3DRESOURCE8
#define LPDIRECT3DTEXTURE LPDIRECT3DTEXTURE8
#define LPDIRECT3DCUBETEXTURE LPDIRECT3DCUBETEXTURE8
#define LPDIRECT3DVOLUMETEXTURE LPDIRECT3DVOLUMETEXTURE8
#define LPDIRECT3DVERTEXBUFFER LPDIRECT3DVERTEXBUFFER8
#define LPDIRECT3DRESOURCE LPDIRECT3DRESOURCE8
#define LPDIRECT3D LPDIRECT3D8
#define LPDIRECT3DDEVICE LPDIRECT3DDEVICE8
#define LPDIRECT3DSURFACE LPDIRECT3DSURFACE8

#define D3DVIEWPORT D3DVIEWPORT8
#define D3DVERTEXELEMENT D3DVERTEXELEMENT8

#define direct3d_create_ctx Direct3DCreate8

#define SetSamplerState_function(device, sampler, type, value) \
 D3D__DirtyFlags |= (D3DDIRTYFLAG_TEXTURE_STATE_0 << sampler); \
 D3D__TextureState[sampler][type] = value

#define RD3DDevice_SetTransform(device, State, pMatrix) D3DDevice_SetTransform(State, pMatrix)

#define RD3DDevice_SetVertexShader(device, handle) D3DDevice_SetVertexShader(handle)
#define RD3DVertexBuffer_Lock(device, OffsetToLock, SizeToLock, ppbData, Flags) *ppbData = D3DVertexBuffer_Lock2(device, Flags) + OffsetToLock
#define RD3DVertexBuffer_Unlock(device)
#define RD3DDevice_SetTexture(device, Stage, pTexture) D3DDevice_SetTexture(Stage, pTexture)
#define RD3DDevice_DrawPrimitive(device, PrimitiveType, StartVertex, PrimitiveCount) D3DDevice_DrawVertices(PrimitiveType, StartVertex, D3DVERTEXCOUNT(PrimitiveType, PrimitiveCount))
#define RD3DDevice_Clear(device, Count, pRects, Flags, Color, Z, Stencil) D3DDevice_Clear(Count, pRects, Flags, Color, Z, Stencil)
#define RD3DDevice_SetViewport(device, viewport) D3DDevice_SetViewport(viewport)
#define RD3DDevice_Present(device) D3DDevice_Swap(0)
#define RD3DDevice_SetSamplerState_MinFilter(device, sampler, value) SetSamplerState_function(device, sampler, D3DTSS_MINFILTER, value)
#define RD3DDevice_SetSamplerState_MagFilter(device, sampler, value) SetSamplerState_function(device, sampler, D3DTSS_MAGFILTER, value)
#define RD3DDevice_SetSamplerState_AddressU(device, sampler, value) SetSamplerState_function(device, sampler, D3DTSS_ADDRESSU, value)
#define RD3DDevice_SetSamplerState_AddressV(device, sampler, value) SetSamplerState_function(device, sampler, D3DTSS_ADDRESSV, value)

#define D3DLOCK_NOSYSLOCK (0)

#define D3DSAMP_ADDRESSU D3DTSS_ADDRESSU
#define D3DSAMP_ADDRESSV D3DTSS_ADDRESSV
#define D3DSAMP_MAGFILTER D3DTSS_MAGFILTER
#define D3DSAMP_MINFILTER D3DTSS_MINFILTER

#elif defined(_XBOX360)
/* XBox 360*/
#define LPDIRECT3D LPDIRECT3D9
#define LPDIRECT3DDEVICE LPDIRECT3DDEVICE9
#define LPDIRECT3DTEXTURE LPDIRECT3DTEXTURE9
#define LPDIRECT3DCUBETEXTURE LPDIRECT3DCUBETEXTURE9
#define LPDIRECT3DSURFACE LPDIRECT3DSURFACE9
#define LPDIRECT3DVOLUMETEXTURE LPDIRECT3DVOLUMETEXTURE9
#define LPDIRECT3DVERTEXBUFFER LPDIRECT3DVERTEXBUFFER9
#define LPDIRECT3DRESOURCE LPDIRECT3DRESOURCE9

#define D3DVIEWPORT D3DVIEWPORT9
#define D3DVERTEXELEMENT D3DVERTEXELEMENT9

#define direct3d_create_ctx Direct3DCreate9

#define RD3DVertexBuffer_Lock(device, OffsetToLock, SizeToLock, ppbData, Flags) *ppbData = D3DVertexBuffer_Lock(device, OffsetToLock, SizeToLock, Flags)
#define RD3DVertexBuffer_Unlock(device) D3DVertexBuffer_Unlock(device)
#define RD3DDevice_SetTexture(device, Stage, pTexture) \
 fetchConstant = GPU_CONVERT_D3D_TO_HARDWARE_TEXTUREFETCHCONSTANT(Stage); \
 pendingMask3 = D3DTAG_MASKENCODE(D3DTAG_START(D3DTAG_FETCHCONSTANTS) + fetchConstant, D3DTAG_START(D3DTAG_FETCHCONSTANTS) + fetchConstant); \
 D3DDevice_SetTexture(device, Stage, pTexture, pendingMask3)

#define RD3DDevice_DrawPrimitive(device, PrimitiveType, StartVertex, PrimitiveCount) D3DDevice_DrawVertices(device, PrimitiveType, StartVertex, D3DVERTEXCOUNT(PrimitiveType, PrimitiveCount))
#define RD3DDevice_Clear(device, Count, pRects, Flags, Color, Z, Stencil) D3DDevice_Clear(device, Count, pRects, Flags, Color, Z, Stencil, false)
#define RD3DDevice_SetViewport(device, viewport) D3DDevice_SetViewport(device, viewport)
#define RD3DDevice_Present(device) D3DDevice_Present(device)
#define RD3DDevice_SetSamplerState_MinFilter(device, sampler, value) D3DDevice_SetSamplerState_MinFilter(device, sampler, value)
#define RD3DDevice_SetSamplerState_MagFilter(device, sampler, value) D3DDevice_SetSamplerState_MagFilter(device, sampler, value)
#define RD3DDevice_SetSamplerState_AddressU(device, sampler, value) D3DDevice_SetSamplerState_AddressU_Inline(device, sampler, value)
#define RD3DDevice_SetSamplerState_AddressV(device, sampler, value) D3DDevice_SetSamplerState_AddressV_Inline(device, sampler, value)

#endif

#endif

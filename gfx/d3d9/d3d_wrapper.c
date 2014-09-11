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

#include "../context/win32_common.h"

void D3DDevice_Presents(d3d_video_t *d3d, LPDIRECT3DDEVICE dev)
{
#if defined(_XBOX1)
   D3DDevice_Swap(0);
#elif defined(_XBOX360)
   D3DDevice_Present(dev);
#else
   if (dev->Present(NULL, NULL, NULL, NULL) != D3D_OK)
   {
      RARCH_ERR("[D3D]: Present() failed.\n");
      d3d->needs_restore = true;
   }
#endif
}

HRESULT D3DDevice_CreateVertexBuffers(LPDIRECT3DDEVICE dev, unsigned length,
      unsigned usage, unsigned fvf, d3DPOOL pool,
      LPDIRECT3DVERTEXBUFFER** vert_buf, void *handle)
{
#if defined(_XBOX1)
#elif defined(_XBOX360)
#else
   dev->CreateVertexBuffer(length, usage, fvf, pool, vert_buf, NULL)
#endif
}

void D3DDevice_SetStreamSources(LPDIRECT3DDEVICE dev, unsigned stream_no,
      LPDIRECT3DVERTEXBUFFER stream_vertbuf, unsigned offset_bytes,
      unsigned stride)
{
#if defined(_XBOX1)
#elif defined(_XBOX360)
#else
   dev->SetStreamSource(steam_no, stream_vertbuf, offset_bytes, stride);
#endif
}

void D3DDevice_SetSamplerState_AddressU(LPDIRECT3DDEVICE dev,
      unsigned sampler, unsigned type)
{
#if defined(_XBOX1)
#elif defined(_XBOX360)
#else
   dev->SetSamplerState(sampler, D3DSAMP_ADDRESSU, type);
#endif
}

void D3DDevice_SetSamplerState_AddressV(LPDIRECT3DDEVICE dev,
      unsigned sampler, unsigned type)
{
#if defined(_XBOX1)
#elif defined(_XBOX360)
#else
   dev->SetSamplerState(sampler, D3DSAMP_ADDRESSV, type);
#endif
}

void D3DDevice_SetSamplerState_MinFilter(LPDIRECT3DDEVICE dev,
      unsigned sampler, unsigned type)
{
#if defined(_XBOX1)
#elif defined(_XBOX360)
#else
   dev->SetSamplerState(sampler, D3DSAMP_MINFILTER, type)
#endif
}

void D3DDevice_SetSamplerState_MagFilter(LPDIRECT3DDEVICE dev,
      unsigned sampler, unsigned type)
{
#if defined(_XBOX1)
#elif defined(_XBOX360)
#else
   dev->SetSamplerState(sampler, D3DSAMP_MAGFILTER, type)
#endif
}

void D3DDevice_DrawPrimitive(LPDIRECT3DDEVICE dev,
      unsigned type, unsigned start, unsigned count)
{
   if (SUCCEEDED(dev->BeginScene()))
   {
      dev->DrawPrimitive(type, start, count);
      dev->EndScene();
   }
}

void D3DDevice_LockRectClear(unsigned pass, LPDIRECT3DTEXTURE tex,
      unsigned level, D3DLOCKED_RECT lock_rect, RECT rect,
      unsigned flags)
{
#if defined(_XBOX1)
#elif defined(_XBOX360)
#else
   if (SUCCEEDED(tex->LockRect(level, &lockedrect, rect, flags)))
   {
      memset(lockedrect.pBits, level, pass.info.tex_h * lockedrect.Pitch);
      tex->UnlockRect(0);
   }
#endif
}

void D3DDevice_TextureBlit(d3d_video_t *d3d,
      LPDIRECT3DTEXTURE tex, D3DSURFACE_DESC desc,
      D3DLOCKED_RECT rect, const void *frame,
      unsigned width, unsigned height, unsigned pitch)
{
#if defined(_XBOX1)
#elif defined(_XBOX360)
#else
   if (SUCCEEDED(tex->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK)))
   {
      for (unsigned y = 0; y < height; y++)
      { 
         const uint8_t *in = (const uint8_t*)frame + y * pitch;
         uint8_t *out = (uint8_t*)d3dlr.pBits + y * d3dlr.Pitch;
         memcpy(out, in, width * d3d->pixel_size);
      }
      tex->UnlockRect(0);
   }
#endif
}

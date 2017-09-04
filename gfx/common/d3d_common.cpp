/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include "../../configuration.h"
#include "../../verbosity.h"

#include "d3d_common.h"

#if defined(HAVE_D3D9)
#include "../include/d3d9/d3dx9tex.h"
#elif defined(HAVE_D3D8)
#include "../include/d3d8/d3dx8tex.h"
#endif

bool d3d_swap(void *data, LPDIRECT3DDEVICE dev)
{
#if defined(_XBOX1)
   D3DDevice_Swap(0);
#elif defined(_XBOX360)
   D3DDevice_Present(dev);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   if (IDirect3DDevice9_Present(dev, NULL, NULL, NULL, NULL) == D3DERR_DEVICE_LOST)
      return false;
#else
   if (dev->Present(NULL, NULL, NULL, NULL) != D3D_OK)
      return false;
#endif
   return true;
}

void d3d_set_transform(LPDIRECT3DDEVICE dev,
      D3DTRANSFORMSTATETYPE state, CONST D3DMATRIX *matrix)
{
#ifdef _XBOX1
   D3DDevice_SetTransform(state, matrix);
#elif !defined(_XBOX360)
   /* XBox 360 D3D9 does not support fixed-function pipeline. */

#if defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_SetTransform(dev, state, matrix);
#else
   dev->SetTransform(state, matrix);
#endif

#endif
}

LPDIRECT3DTEXTURE d3d_texture_new(LPDIRECT3DDEVICE dev,
      const char *path, unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, D3DFORMAT format,
      D3DPOOL pool, unsigned filter, unsigned mipfilter,
      D3DCOLOR color_key, void *src_info_data, 
      PALETTEENTRY *palette)
{
   HRESULT hr;
   LPDIRECT3DTEXTURE buf;
   D3DXIMAGE_INFO *src_info = (D3DXIMAGE_INFO*)src_info_data;

   if (path)
      hr = D3DXCreateTextureFromFileExA(dev,
            path, width, height, miplevels, usage, format,
            pool, filter, mipfilter, color_key, src_info,
            palette, &buf);
   else
   {
      hr = dev->CreateTexture(width, height, miplevels, usage,
            format, pool, &buf
#ifndef HAVE_D3D8
            , NULL
#endif
            );
   }

   if (FAILED(hr))
	   return NULL;

   return buf;
}

void d3d_texture_free(LPDIRECT3DTEXTURE tex)
{
   if (tex)
   {
#if defined(HAVE_D3D9) && !defined(__cplusplus)
      IDirect3DTexture9_Release(tex);
#else
      tex->Release();
#endif
   }
}

bool d3d_vertex_declaration_new(LPDIRECT3DDEVICE dev,
      const void *vertex_data, void **decl_data)
{
#ifdef HAVE_D3D9
   const D3DVERTEXELEMENT   *vertex_elements = (const D3DVERTEXELEMENT*)vertex_data;
   LPDIRECT3DVERTEXDECLARATION **vertex_decl = (LPDIRECT3DVERTEXDECLARATION**)decl_data;

   if (SUCCEEDED(dev->CreateVertexDeclaration(vertex_elements, (IDirect3DVertexDeclaration9**)vertex_decl)))
      return true;
#endif
   return false;
}

LPDIRECT3DVERTEXBUFFER d3d_vertex_buffer_new(LPDIRECT3DDEVICE dev,
      unsigned length, unsigned usage,
      unsigned fvf, D3DPOOL pool, void *handle)
{
   HRESULT hr;
   LPDIRECT3DVERTEXBUFFER buf;

#ifndef _XBOX
#ifndef HAVE_D3D8
   if (usage == 0)
   {
	  if (dev->GetSoftwareVertexProcessing())
         usage = D3DUSAGE_SOFTWAREPROCESSING;
   }
#endif
#endif

#if defined(HAVE_D3D8)
   hr = IDirect3DDevice8_CreateVertexBuffer(dev, length, usage, fvf, pool,
         &buf);
#elif defined(HAVE_D3D9)
   hr = IDirect3DDevice9_CreateVertexBuffer(dev, length, usage, fvf, pool,
         &buf, NULL);
#else
   hr = dev->CreateVertexBuffer(length, usage, fvf, pool, &buf, NULL);
#endif

   if (FAILED(hr))
	   return NULL;

   return buf;
}

void d3d_vertex_buffer_unlock(void *vertbuf_ptr)
{
   LPDIRECT3DVERTEXBUFFER vertbuf = (LPDIRECT3DVERTEXBUFFER)vertbuf_ptr;

#ifdef _XBOX360
   D3DVertexBuffer_Unlock(vertbuf);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DVertexBuffer9_Unlock(vertbuf);
#elif defined(HAVE_D3D9)
   vertbuf->Unlock();
#endif
}

void *d3d_vertex_buffer_lock(void *vertbuf_ptr)
{
   void                      *buf = NULL;
   LPDIRECT3DVERTEXBUFFER vertbuf = (LPDIRECT3DVERTEXBUFFER)vertbuf_ptr;

#if defined(_XBOX1)
   buf = (void*)D3DVertexBuffer_Lock2(vertbuf, 0);
#elif defined(_XBOX360)
   buf = D3DVertexBuffer_Lock(vertbuf, 0, 0, 0);
#elif defined(HAVE_D3D9)
   vertbuf->Lock(0, sizeof(buf), &buf, 0);
#endif

   if (!buf)
      return NULL;

   return buf;
}

void d3d_vertex_buffer_free(void *vertex_data, void *vertex_declaration)
{
   if (vertex_data)
   {
      LPDIRECT3DVERTEXBUFFER buf = (LPDIRECT3DVERTEXBUFFER)vertex_data;
      buf->Release();
      buf = NULL;
   }

#ifdef HAVE_D3D9
   if (vertex_declaration)
   {
      LPDIRECT3DVERTEXDECLARATION vertex_decl = (LPDIRECT3DVERTEXDECLARATION)vertex_declaration;
      vertex_decl->Release();
      vertex_decl = NULL;
   }
#endif
}

void d3d_set_stream_source(LPDIRECT3DDEVICE dev, unsigned stream_no,
      void *stream_vertbuf_ptr, unsigned offset_bytes,
      unsigned stride)
{
	LPDIRECT3DVERTEXBUFFER stream_vertbuf = (LPDIRECT3DVERTEXBUFFER)stream_vertbuf_ptr;
#if defined(HAVE_D3D8)
   IDirect3DDevice8_SetStreamSource(dev, stream_no, stream_vertbuf, stride);
#elif defined(_XBOX360)
   D3DDevice_SetStreamSource_Inline(dev, stream_no, stream_vertbuf,
         offset_bytes, stride);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_SetStreamSource(dev, stream_no, stream_vertbuf, offset_bytes,
         stride);
#else
   dev->SetStreamSource(stream_no, stream_vertbuf, offset_bytes, stride);
#endif
}

void d3d_set_sampler_address_u(LPDIRECT3DDEVICE dev,
      unsigned sampler, unsigned value)
{
#if defined(_XBOX1)
   D3D__DirtyFlags |= (D3DDIRTYFLAG_TEXTURE_STATE_0 << sampler);
   D3D__TextureState[sampler][D3DTSS_ADDRESSU] = value;
#elif defined(_XBOX360)
   D3DDevice_SetSamplerState_AddressU_Inline(dev, sampler, value);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_SetSamplerState(dev, sampler, D3DSAMP_ADDRESSU, value);
#elif defined(HAVE_D3D8)
   dev->SetTextureStageState(sampler, D3DTSS_ADDRESSU, value);
#else
   dev->SetSamplerState(sampler, D3DSAMP_ADDRESSU, value);
#endif
}

void d3d_set_sampler_address_v(LPDIRECT3DDEVICE dev,
      unsigned sampler, unsigned value)
{
#if defined(_XBOX1)
   D3D__DirtyFlags |= (D3DDIRTYFLAG_TEXTURE_STATE_0 << sampler);
   D3D__TextureState[sampler][D3DTSS_ADDRESSV] = value;
#elif defined(_XBOX360)
   D3DDevice_SetSamplerState_AddressV_Inline(dev, sampler, value);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_SetSamplerState(dev, sampler, D3DSAMP_ADDRESSV, value);
#elif defined(HAVE_D3D8)
   dev->SetTextureStageState(sampler, D3DTSS_ADDRESSV, value);
#else
   dev->SetSamplerState(sampler, D3DSAMP_ADDRESSV, value);
#endif
}

void d3d_set_sampler_minfilter(LPDIRECT3DDEVICE dev,
      unsigned sampler, unsigned value)
{
#if defined(_XBOX1)
   D3D__DirtyFlags |= (D3DDIRTYFLAG_TEXTURE_STATE_0 << sampler);
   D3D__TextureState[sampler][D3DTSS_MINFILTER] = value;
#elif defined(_XBOX360)
   D3DDevice_SetSamplerState_MinFilter(dev, sampler, value);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_SetSamplerState(dev, sampler, D3DSAMP_MINFILTER, value);
#elif defined(HAVE_D3D8)
   dev->SetTextureStageState(sampler, D3DTSS_MINFILTER, value);
#else
   dev->SetSamplerState(sampler, D3DSAMP_MINFILTER, value);
#endif
}

void d3d_set_sampler_magfilter(LPDIRECT3DDEVICE dev,
      unsigned sampler, unsigned value)
{
#if defined(_XBOX1)
   D3D__DirtyFlags |= (D3DDIRTYFLAG_TEXTURE_STATE_0 << sampler);
   D3D__TextureState[sampler][D3DTSS_MAGFILTER] = value;
#elif defined(_XBOX360)
   D3DDevice_SetSamplerState_MagFilter(dev, sampler, value);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_SetSamplerState(dev, sampler, D3DSAMP_MAGFILTER, value);
#elif defined(HAVE_D3D8)
   dev->SetTextureStageState(sampler, D3DTSS_MAGFILTER, value);
#else
   dev->SetSamplerState(sampler, D3DSAMP_MAGFILTER, value);
#endif
}

void d3d_draw_primitive(LPDIRECT3DDEVICE dev,
      D3DPRIMITIVETYPE type, unsigned start, unsigned count)
{
#if defined(_XBOX1)
   D3DDevice_DrawVertices(type, start, D3DVERTEXCOUNT(type, count));
#elif defined(_XBOX360)
   D3DDevice_DrawVertices(dev, type, start, D3DVERTEXCOUNT(type, count));
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_BeginScene(dev);
   IDirect3DDevice9_DrawPrimitive(dev, type, start, count);
   IDirect3DDevice9_EndScene(dev);
#else
   if (SUCCEEDED(dev->BeginScene()))
   {
      dev->DrawPrimitive(type, start, count);
      dev->EndScene();
   }
#endif
}

void d3d_clear(LPDIRECT3DDEVICE dev,
      unsigned count, const D3DRECT *rects, unsigned flags,
      D3DCOLOR color, float z, unsigned stencil)
{
#if defined(_XBOX1)
   D3DDevice_Clear(count, rects, flags, color, z, stencil);
#elif defined(_XBOX360)
   D3DDevice_Clear(dev, count, rects, flags, color, z, 
         stencil, false);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_Clear(dev, count, rects, flags,
         color, z, stencil);
#else
   dev->Clear(count, rects, flags, color, z, stencil);
#endif
}

bool d3d_lock_rectangle(LPDIRECT3DTEXTURE tex,
      unsigned level, D3DLOCKED_RECT *lock_rect, RECT *rect,
      unsigned rectangle_height, unsigned flags)
{
#if defined(_XBOX)
   D3DTexture_LockRect(tex, level, lock_rect, rect, flags);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   if (IDirect3DSurface9_LockRect(tex, lock_rect, rect, flags) != D3D_OK)
      return false;
#else
   if (FAILED(tex->LockRect(level, lock_rect, rect, flags)))
      return false;
#endif
   return true;
}

void d3d_unlock_rectangle(LPDIRECT3DTEXTURE tex)
{
#ifdef _XBOX
   D3DTexture_UnlockRect(tex, 0);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DSurface9_UnlockRect(tex);
#else
   tex->UnlockRect(0);
#endif
}

void d3d_lock_rectangle_clear(LPDIRECT3DTEXTURE tex,
      unsigned level, D3DLOCKED_RECT *lock_rect, RECT *rect,
      unsigned rectangle_height, unsigned flags)
{
#if defined(_XBOX)
   level = 0;
#endif
   memset(lock_rect->pBits, level, rectangle_height * lock_rect->Pitch);
   d3d_unlock_rectangle(tex);
}

void d3d_set_viewports(LPDIRECT3DDEVICE dev, D3DVIEWPORT *vp)
{
#if defined(_XBOX360)
   D3DDevice_SetViewport(dev, vp);
#elif defined(_XBOX1)
   D3DDevice_SetViewport(vp);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_SetViewport(dev, vp);
#else
   dev->SetViewport(vp);
#endif
}

void d3d_set_texture(LPDIRECT3DDEVICE dev, unsigned sampler,
      void *tex_data)
{
   LPDIRECT3DTEXTURE tex = (LPDIRECT3DTEXTURE)tex_data;
#if defined(_XBOX1)
   D3DDevice_SetTexture(sampler, tex);
#elif defined(_XBOX360)
   unsigned fetchConstant = 
      GPU_CONVERT_D3D_TO_HARDWARE_TEXTUREFETCHCONSTANT(sampler);
   uint64_t pendingMask3 = 
      D3DTAG_MASKENCODE(D3DTAG_START(D3DTAG_FETCHCONSTANTS) 
            + fetchConstant, D3DTAG_START(D3DTAG_FETCHCONSTANTS)
            + fetchConstant);
   D3DDevice_SetTexture(dev, sampler, tex, pendingMask3);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_SetTexture(dev, sampler, tex);
#else
   dev->SetTexture(sampler, tex);
#endif
}

HRESULT d3d_set_vertex_shader(LPDIRECT3DDEVICE dev, unsigned index,
      void *data)
{
#if defined(_XBOX1)
   return dev->SetVertexShader(index);
#elif defined(_XBOX360)
   LPDIRECT3DVERTEXSHADER shader = (LPDIRECT3DVERTEXSHADER)data;
   D3DDevice_SetVertexShader(dev, shader);
   return S_OK;
#elif defined(HAVE_D3D8)
   return E_FAIL;
#else
   LPDIRECT3DVERTEXSHADER shader = (LPDIRECT3DVERTEXSHADER)data;
   return dev->SetVertexShader(shader);
#endif
}


void d3d_texture_blit(unsigned pixel_size,
      LPDIRECT3DTEXTURE tex, D3DLOCKED_RECT *lr, const void *frame,
      unsigned width, unsigned height, unsigned pitch)
{
   if (d3d_lock_rectangle(tex, 0, lr, NULL, 0, 0))
   {
#if defined(_XBOX360)
      D3DSURFACE_DESC desc;
      tex->GetLevelDesc(0, &desc);
      XGCopySurface(lr->pBits, lr->Pitch, width, height, desc.Format, NULL,
            frame, pitch, desc.Format, NULL, 0, 0);
#else
      unsigned y;
      for (y = 0; y < height; y++)
      {
         const uint8_t *in = (const uint8_t*)frame + y * pitch;
         uint8_t *out = (uint8_t*)lr->pBits + y * lr->Pitch;
         memcpy(out, in, width * pixel_size);
      }
#endif
      d3d_unlock_rectangle(tex);
   }
}

void d3d_set_render_state(void *data, D3DRENDERSTATETYPE state, DWORD value)
{
   LPDIRECT3DDEVICE dev = (LPDIRECT3DDEVICE)data;

   if (!dev)
      return;

#if defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_SetRenderState(dev, state, value);
#else
   dev->SetRenderState(state, value);
#endif
}

void d3d_enable_blend_func(void *data)
{
   LPDIRECT3DDEVICE dev = (LPDIRECT3DDEVICE)data;

   if (!dev)
      return;

   d3d_set_render_state(dev, D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
   d3d_set_render_state(dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
   d3d_set_render_state(dev, D3DRS_ALPHABLENDENABLE, true);
}

void d3d_enable_alpha_blend_texture_func(void *data)
{
   LPDIRECT3DDEVICE dev = (LPDIRECT3DDEVICE)data;

   if (!dev)
      return;

#ifndef _XBOX360
   /* Also blend the texture with the set alpha value. */
   dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
   dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
   dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);
#endif
}

void d3d_frame_postprocess(void *data)
{
#if defined(_XBOX1)
   LPDIRECT3DDEVICE    dev = (LPDIRECT3DDEVICE)data;
   global_t        *global = global_get_ptr();

   if (!dev)
      return;
#if 0
   if (!d3d_restore_device(dev))
      return;
#endif

   dev->SetFlickerFilter(global->console.screen.flicker_filter_index);
   dev->SetSoftDisplayFilter(global->console.softfilter_enable);
#endif
}

void d3d_disable_blend_func(void *data)
{
   LPDIRECT3DDEVICE dev = (LPDIRECT3DDEVICE)data;

   if (!dev)
      return;

   d3d_set_render_state(dev, D3DRS_ALPHABLENDENABLE, false);
}

void d3d_set_vertex_declaration(void *data, void *vertex_data)
{
   LPDIRECT3DDEVICE dev = (LPDIRECT3DDEVICE)data;
#if defined(HAVE_D3D9)
   LPDIRECT3DVERTEXDECLARATION decl = (LPDIRECT3DVERTEXDECLARATION)vertex_data;
#endif
   if (!dev)
      return;

#ifdef _XBOX1
   d3d_set_vertex_shader(dev, D3DFVF_XYZ | D3DFVF_TEX1, NULL);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_SetVertexDeclaration(dev, decl);
#elif defined(HAVE_D3D9)
   dev->SetVertexDeclaration(decl);
#endif
}

bool d3d_reset(LPDIRECT3DDEVICE dev, D3DPRESENT_PARAMETERS *d3dpp)
{
#ifndef _XBOX
   HRESULT res;
#endif
   const char *err = NULL;

   if (dev->Reset(d3dpp) == D3D_OK)
      return true;

   /* Try to recreate the device completely. */
#ifndef _XBOX
   res     = dev->TestCooperativeLevel();

   switch (res)
   {
      case D3DERR_DEVICELOST:
         err = "DEVICELOST";
         break;

      case D3DERR_DEVICENOTRESET:
         err = "DEVICENOTRESET";
         break;

      case D3DERR_DRIVERINTERNALERROR:
         err = "DRIVERINTERNALERROR";
         break;

      default:
         err = "Unknown";
   }
   RARCH_WARN("[D3D]: Attempting to recover from dead state (%s).\n",
         err);
#else
   RARCH_WARN("[D3D]: Attempting to recover from dead state.\n");
#endif

   return false;
}

void d3d_device_free(LPDIRECT3DDEVICE dev, LPDIRECT3D pd3d)
{
   if (dev)
   {
#if defined(HAVE_D3D9) && !defined(__cplusplus)
      IDirect3DDevice9_Release(dev);
#else
      dev->Release();
#endif
   }
   if (pd3d)
   {
#if defined(HAVE_D3D9) && !defined(__cplusplus)
      IDirect3D9_Release(pd3d);
#else
      pd3d->Release();
#endif
   }
}

D3DTEXTUREFILTERTYPE d3d_translate_filter(unsigned type)
{
   switch (type)
   {
      case RARCH_FILTER_UNSPEC:
         {
            settings_t *settings = config_get_ptr();
            if (!settings->bools.video_smooth)
               break;
         }
         /* fall-through */
      case RARCH_FILTER_LINEAR:
         return D3DTEXF_LINEAR;
      case RARCH_FILTER_NEAREST:
         break;
   }

   return D3DTEXF_POINT;
}

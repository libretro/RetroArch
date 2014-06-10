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

#ifndef D3DVIDEO_HPP__
#define D3DVIDEO_HPP__

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifndef _XBOX
#define HAVE_WINDOW
#endif

#if defined(_XBOX1)
#ifndef HAVE_D3D8
#define HAVE_D3D8
#endif
#else
#ifndef HAVE_D3D9
#define HAVE_D3D9
#endif
#endif

#include "../../general.h"
#include "../../driver.h"
#include "../shader_parse.h"

#include "../fonts/d3d_font.h"
#include "../gfx_context.h"
#include "../gfx_common.h"

#ifdef HAVE_CG
#include <Cg/cg.h>
#include <Cg/cgD3D9.h>
#endif
#include "d3d_defines.h"
#include <string>
#include <vector>

class RenderChain;

#ifndef _XBOX
#define D3DDevice_SetSamplerState_AddressU(dev, sampler, type) dev->SetSamplerState(sampler, D3DSAMP_ADDRESSU, type)
#define D3DDevice_SetSamplerState_AddressV(dev, sampler, type) dev->SetSamplerState(sampler, D3DSAMP_ADDRESSV, type)
#define D3DDevice_SetSamplerState_MinFilter(dev, sampler, type) dev->SetSamplerState(sampler, D3DSAMP_MINFILTER, type)
#define D3DDevice_SetSamplerState_MagFilter(dev, sampler, type) dev->SetSamplerState(sampler, D3DSAMP_MAGFILTER, type)
#define D3DDevice_DrawPrimitive(dev, type, start, count) \
   if (SUCCEEDED(dev->BeginScene())) \
   { \
      dev->DrawPrimitive(type, start, count); \
      dev->EndScene(); \
   }
#define D3DTexture_LockRectClear(pass, tex, level, lockedrect, rect, flags) \
   if (SUCCEEDED(tex->LockRect(level, &lockedrect, rect, flags))) \
   { \
      memset(lockedrect.pBits, level, pass.info.tex_h * lockedrect.Pitch); \
      tex->UnlockRect(0); \
   }
#define D3DDevice_Presents(d3d, dev) \
      if (dev->Present(NULL, NULL, NULL, NULL) != D3D_OK) \
      { \
         RARCH_ERR("[D3D]: Present() failed.\n"); \
         d3d->needs_restore = true; \
      }
#define D3DDevice_CreateVertexBuffers(device, Length, Usage, UnusedFVF, UnusedPool, ppVertexBuffer, pUnusedSharedHandle) device->CreateVertexBuffer(Length, Usage, UnusedFVF, UnusedPool, ppVertexBuffer, NULL)

#define D3DDevice_SetStreamSources(device, streamNumber, pStreamData, OffsetInBytes, Stride) device->SetStreamSource(streamNumber, pStreamData, OffsetInBytes, Stride)

#define D3DTexture_Blit(d3d, desc, d3dlr, frame, width, height, pitch) \
   if (SUCCEEDED(first.tex->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK))) \
   { \
   for (unsigned y = 0; y < height; y++) \
   { \
      const uint8_t *in = (const uint8_t*)frame + y * pitch; \
      uint8_t *out = (uint8_t*)d3dlr.pBits + y * d3dlr.Pitch; \
      memcpy(out, in, width * d3d->pixel_size); \
   } \
         first.tex->UnlockRect(0); \
   }
#endif


typedef struct
{
   struct Coords
   {
      float x, y, w, h;
   };
   Coords tex_coords;
   Coords vert_coords;
   unsigned tex_w, tex_h;
   bool fullscreen;
   bool enabled;
   float alpha_mod;
   LPDIRECT3DTEXTURE tex;
   LPDIRECT3DVERTEXBUFFER vert_buf;
} overlay_t;

void d3d_set_font_rect(void *data, const struct font_params *params);
bool d3d_process_shader(void *data);
void d3d_update_title(void *data);
void d3d_recompute_pass_sizes(void *data);
RECT d3d_monitor_rect(void *data);
bool d3d_init_shader(void *data);
void d3d_deinit_shader(void *data);
bool d3d_init_imports(void *data);
bool d3d_init_luts(void *data);
bool d3d_init_singlepass(void *data);
bool d3d_init_multipass(void *data);
bool d3d_init_chain(void *data, const video_info_t *video_info);
void d3d_deinit_chain(void *data);
void d3d_show_cursor(void *data, bool state);
void d3d_make_d3dpp(void *data, const video_info_t *info, D3DPRESENT_PARAMETERS *d3dpp);
bool d3d_alive_func(void *data);

typedef struct d3d_video
{
      const d3d_font_renderer_t *font_ctx;
      const gfx_ctx_driver_t *ctx_driver;
      bool should_resize;
      bool quitting;

#ifdef HAVE_WINDOW
      WNDCLASSEX windowClass;
#endif
      HWND hWnd;
      LPDIRECT3D g_pD3D;
      LPDIRECT3DDEVICE dev;
#ifndef _XBOX
      LPD3DXFONT font;
#endif
      HRESULT d3d_err;
      unsigned cur_mon_id;

      unsigned screen_width;
      unsigned screen_height;
      unsigned dev_rotation;
      D3DVIEWPORT final_viewport;

      std::string cg_shader;

      struct gfx_shader shader;

      video_info_t video_info;

      bool needs_restore;

#ifdef HAVE_CG
      CGcontext cgCtx;
#endif
      RECT font_rect;
      RECT font_rect_shifted;
      uint32_t font_color;

#ifdef HAVE_OVERLAY
      bool overlays_enabled;
      std::vector<overlay_t> overlays;
#endif

#ifdef HAVE_MENU
      overlay_t *menu;
#endif
      void *chain;
} d3d_video_t;

#ifndef _XBOX
extern "C" bool dinput_handle_message(void *dinput, UINT message, WPARAM wParam, LPARAM lParam);
#endif

#endif


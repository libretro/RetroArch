/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - OV2
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

/* Direct3D 8 driver.
 *
 * Minimum version : Direct3D 8.0 (2000)
 * Minimum OS      : Windows 95 (8.0a), Windows 98 and XP after 8.0a
 * Recommended OS  : Windows 98/ME and/or Windows XP
 */

#define CINTERFACE

#ifdef _XBOX
#include <xtl.h>
#include <xgraphics.h>
#endif

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_inline.h>

#include <formats/image.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <retro_math.h>
#include <encodings/utf.h>
#include <gfx/math/matrix_4x4.h>

#include <d3d8.h>

#include <defines/d3d_defines.h>
#include "../common/d3d_common.h"

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../dynamic.h"
#include "../../frontend/frontend_driver.h"

#ifdef HAVE_THREADS
#include "../video_thread_wrapper.h"
#endif

#include "../common/win32_common.h"

#ifdef _XBOX
#define D3D8_PRESENTATIONINTERVAL D3DRS_PRESENTATIONINTERVAL
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../font_driver.h"
#include "../gfx_display.h"
#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#endif

#include "../../core.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

#ifdef __WINRT__
#error "UWP does not support D3D8"
#endif

#ifdef _XBOX
#define D3D8_RGB565_FORMAT D3DFMT_LIN_R5G6B5
#define D3D8_XRGB8888_FORMAT D3DFMT_LIN_X8R8G8B8
#define D3D8_ARGB8888_FORMAT D3DFMT_LIN_A8R8G8B8
#define D3D8_ARGB4444_FORMAT D3DFMT_LIN_A4R4G4B4
#else
#define D3D8_RGB565_FORMAT D3DFMT_R5G6B5
#define D3D8_XRGB8888_FORMAT D3DFMT_X8R8G8B8
#define D3D8_ARGB8888_FORMAT D3DFMT_A8R8G8B8
#define D3D8_ARGB4444_FORMAT D3DFMT_A4R4G4B4
#endif

typedef struct d3d8_video
{
   overlay_t *menu;
   void *renderchain_data;

   struct video_viewport vp;
   struct video_shader shader;
   video_info_t video_info;
#ifdef HAVE_WINDOW
   WNDCLASSEX windowClass;
#endif
   LPDIRECT3DDEVICE8 dev;
   LPDIRECT3D8 d3d8;
   D3DVIEWPORT8 out_vp;

   char *shader_path;

   struct
   {
      void *buffer;
      void *decl;
      int size;
      int offset;
      /* Soft scissor for D3D8 (no SetScissorRect available).
       * scissor_begin stores the requested rect here, and
       * gfx_display_d3d8_draw skips any quad whose screen-space
       * bounding box lies entirely outside the rect.  Partial
       * overlaps still draw in full — true geometry clipping
       * would require modifying vertex/UV arrays per draw and is
       * not worth the complexity here.  Skip-only is enough to
       * stop entry lists from spilling on top of header/footer
       * regions in Ozone, which is the only place the visual
       * gap with d3d9+ was noticeable. */
      int  scissor_x;
      int  scissor_y;
      int  scissor_w;
      int  scissor_h;
      bool scissor_active;
      /* Scratch UV array for clipped quads.  Layout matches
       * d3d8_tex_coords: BL, BR, TL, TR (8 floats).  Reused
       * across draws — only valid until the next clipped
       * draw. */
      float scissor_uv[8];
   }menu_display;

   overlay_t *overlays;
   size_t overlays_size;
   unsigned cur_mon_id;
   unsigned dev_rotation;
   math_matrix_4x4 mvp; /* float alignment */
   math_matrix_4x4 mvp_rotate; /* float alignment */
   math_matrix_4x4 mvp_transposed; /* float alignment */

   bool keep_aspect;
   bool should_resize;
   bool quitting;
   bool needs_restore;
   bool overlays_enabled;
   /* TODO - refactor this away properly. */
   bool resolution_hd_enable;

   /* Only used for Xbox */
   bool widescreen_mode;

   /* Bit-depth of the data most recently uploaded to `menu->tex`.
    * The menu texture is created with a fixed pixel format (16bpp
    * ARGB4444 for the RGUI fast path, 32bpp ARGB8888 otherwise),
    * so we must recreate it when set_menu_texture_frame is called
    * with a different `rgb32` value.  Defaults to false; the first
    * call will see a NULL tex and create one regardless. */
   bool menu_tex_rgb32;
} d3d8_video_t;

typedef struct d3d8_renderchain
{
   unsigned pixel_size;
   LPDIRECT3DDEVICE8 dev;
   const video_info_t *video_info;
   LPDIRECT3DTEXTURE8 tex;
   LPDIRECT3DVERTEXBUFFER8 vertex_buf;
   unsigned last_width;
   unsigned last_height;
   void *vertex_decl;
   unsigned tex_w;
   unsigned tex_h;
   uint64_t frame_count;
} d3d8_renderchain_t;

struct d3d8_texture_info
{
   void *userdata;
   void *data;
   enum texture_filter_type type;
};

static const float d3d8_vertexes[8] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const float d3d8_tex_coords[8] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

void *dinput;

/*
 * D3D8 COMMON
 */

/* For Xbox we will just link statically
 * to Direct3D libraries instead. */

#if !defined(_XBOX) && defined(HAVE_DYLIB)
#define HAVE_DYNAMIC_D3D
#endif

#ifdef HAVE_DYNAMIC_D3D
#include <dynamic/dylib.h>
#endif

/* TODO/FIXME - static globals */
#ifdef HAVE_DYNAMIC_D3D
static dylib_t g_d3d8_dll;
static bool dylib_initialized = false;
#endif

typedef IDirect3D8 *(__stdcall *D3DCreate_t)(UINT);

static D3DCreate_t D3DCreate;

#ifdef HAVE_DYNAMIC_D3D
static void d3d8_deinitialize_symbols(void)
{
   if (g_d3d8_dll)
      dylib_close(g_d3d8_dll);
   g_d3d8_dll         = NULL;
   dylib_initialized = false;
}
#endif

static bool d3d8_initialize_symbols(enum gfx_ctx_api api)
{
#ifdef HAVE_DYNAMIC_D3D
   if (dylib_initialized)
      return true;

#if defined(DEBUG) || defined(_DEBUG)
   if (!(g_d3d8_dll = dylib_load("d3d8d.dll")))
#endif
      g_d3d8_dll            = dylib_load("d3d8.dll");

   if (!g_d3d8_dll)
   {
      /* On modern Windows the legacy D3D8 user-mode runtime is not
       * installed by default (only d3d8thk.dll, the kernel thunk
       * layer, ships with the OS). Tell the user explicitly --
       * otherwise the only message they see is the generic
       * "Cannot open video driver" from video_driver_init_internal. */
      RARCH_ERR("[D3D8] Failed to load d3d8.dll: %s\n",
            dylib_error() ? dylib_error() : "(no error reported)");
      RARCH_ERR("[D3D8] The legacy DirectX 8 runtime is not present "
            "on this system. Drop a matching d3d8.dll and d3d9.dll"
            "(e.g. from DXVK) next to retroarch.exe, or pick a different "
            "video driver.\n");
      return false;
   }
   if (!(D3DCreate = (D3DCreate_t)dylib_proc(g_d3d8_dll, "Direct3DCreate8")))
   {
      RARCH_ERR("[D3D8] d3d8.dll does not export Direct3DCreate8: %s\n",
            dylib_error() ? dylib_error() : "(no error reported)");
   }
#else
   D3DCreate                = Direct3DCreate8;
#endif

   if (!D3DCreate)
      goto error;

#ifdef HAVE_DYNAMIC_D3D
   dylib_initialized        = true;
#endif

   return true;

error:
#ifdef HAVE_DYNAMIC_D3D
   d3d8_deinitialize_symbols();
#endif
   return false;
}


static INLINE void *
d3d8_vertex_buffer_lock(LPDIRECT3DVERTEXBUFFER8 vertbuf)
{
   void *buf = NULL;
   IDirect3DVertexBuffer8_Lock(vertbuf, 0, 0, (BYTE**)&buf, 0);
   return buf;
}

bool d3d8_reset(void *data, void *d3dpp)
{
   const char       *err = NULL;
   LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)data;
   if (dev && IDirect3DDevice8_Reset(dev, (D3DPRESENT_PARAMETERS*)d3dpp) ==
         D3D_OK)
      return true;
#ifndef _XBOX
   RARCH_WARN("[D3D8] Attempting to recover from dead state...\n");
   /* Try to recreate the device completely. */
   switch (IDirect3DDevice8_TestCooperativeLevel(dev))
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
   RARCH_WARN("[D3D8] Recovering from dead state: (%s).\n", err);
#endif
   return false;
}

static bool d3d8_create_device_internal(
      LPDIRECT3DDEVICE8 dev,
      D3DPRESENT_PARAMETERS *d3dpp,
      LPDIRECT3D8 d3d,
      HWND focus_window,
      unsigned cur_mon_id,
      DWORD behavior_flags)
{
   if (dev &&
         SUCCEEDED(IDirect3D8_CreateDevice(d3d,
               cur_mon_id,
               D3DDEVTYPE_HAL,
               focus_window,
               behavior_flags,
               d3dpp,
               (IDirect3DDevice8**)dev)))
      return true;

   return false;
}

static bool d3d8_create_device(void *dev,
      void *d3dpp,
      LPDIRECT3D8 d3d,
      HWND focus_window,
      unsigned cur_mon_id)
{
   if (!d3d8_create_device_internal(dev,
            (D3DPRESENT_PARAMETERS*)d3dpp,
            d3d,
            focus_window,
            cur_mon_id,
            D3DCREATE_HARDWARE_VERTEXPROCESSING))
      if (!d3d8_create_device_internal(
               dev,
               (D3DPRESENT_PARAMETERS*)d3dpp, d3d, focus_window,
               cur_mon_id,
               D3DCREATE_SOFTWARE_VERTEXPROCESSING))
         return false;
   return true;
}

static void *d3d8_texture_new(LPDIRECT3DDEVICE8 dev,
      unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, INT32 format,
      INT32 pool, unsigned filter, unsigned mipfilter,
      INT32 color_key, void *src_info_data,
      PALETTEENTRY *palette, bool want_mipmap)
{
   void *buf             = NULL;
   if (SUCCEEDED(IDirect3DDevice8_CreateTexture(dev,
               width, height, miplevels, usage,
               (D3DFORMAT)format, (D3DPOOL)pool,
               (struct IDirect3DTexture8**)&buf)))
      return buf;
   return NULL;
}

static void d3d8_set_mvp(void *data, const void *mat_data)
{
   math_matrix_4x4 matrix;
   LPDIRECT3DDEVICE8 d3dr     = (LPDIRECT3DDEVICE8)data;

   matrix_4x4_identity(matrix);

   IDirect3DDevice8_SetTransform(d3dr,
         D3DTS_PROJECTION, (D3DMATRIX*)&matrix);
   IDirect3DDevice8_SetTransform(d3dr,
         D3DTS_VIEW, (D3DMATRIX*)&matrix);
   matrix_4x4_transpose(matrix, (*(const math_matrix_4x4*)mat_data));
   IDirect3DDevice8_SetTransform(d3dr, D3DTS_WORLD, (D3DMATRIX*)&matrix);
}

static void d3d8_set_vertices(
      d3d8_video_t *d3d,
      d3d8_renderchain_t *chain,
      unsigned pass,
      unsigned vert_width, unsigned vert_height, uint64_t frame_count)
{
   unsigned width  = d3d->vp.full_width;
   unsigned height = d3d->vp.full_height;

   if (chain->last_width != vert_width || chain->last_height != vert_height)
   {
      Vertex vert[4];
      void *verts        = NULL;
      float tex_w        = vert_width;
      float tex_h        = vert_height;

      chain->last_width  = vert_width;
      chain->last_height = vert_height;

      if (chain->vertex_buf)
      {
         LPDIRECT3DVERTEXBUFFER8 vbo;
         vert[0].x        =  0.0f;
         vert[0].y        =  1.0f;
         vert[0].z        =  1.0f;

         vert[1].x        =  1.0f;
         vert[1].y        =  1.0f;
         vert[1].z        =  1.0f;

         vert[2].x        =  0.0f;
         vert[2].y        =  0.0f;
         vert[2].z        =  1.0f;

         vert[3].x        =  1.0f;
         vert[3].y        =  0.0f;
         vert[3].z        =  1.0f;

         vert[0].u        = 0.0f;
         vert[0].v        = 0.0f;
         vert[1].v        = 0.0f;
         vert[2].u        = 0.0f;
         vert[1].u        = tex_w;
         vert[2].v        = tex_h;
         vert[3].u        = tex_w;
         vert[3].v        = tex_h;
#ifndef _XBOX
         vert[1].u       /= chain->tex_w;
         vert[2].v       /= chain->tex_h;
         vert[3].u       /= chain->tex_w;
         vert[3].v       /= chain->tex_h;
#endif

         vert[0].color    = 0xFFFFFFFF;
         vert[1].color    = 0xFFFFFFFF;
         vert[2].color    = 0xFFFFFFFF;
         vert[3].color    = 0xFFFFFFFF;

         vbo              = (LPDIRECT3DVERTEXBUFFER8)chain->vertex_buf;
         verts            = d3d8_vertex_buffer_lock(vbo);
         memcpy(verts, vert, sizeof(vert));
         IDirect3DVertexBuffer8_Unlock(vbo);
      }
   }
}

static void d3d8_blit_to_texture(
      d3d8_renderchain_t *chain,
      const void *frame,
      unsigned width, unsigned height, unsigned pitch)
{
   D3DLOCKED_RECT d3dlr;
   D3DLOCKED_RECT *lr         = &d3dlr;
   LPDIRECT3DDEVICE8 d3dr     = (LPDIRECT3DDEVICE8)chain->dev;
   LPDIRECT3DTEXTURE8 tex     = (LPDIRECT3DTEXTURE8)chain->tex;
#ifdef _XBOX
   global_t        *global    = global_get_ptr();
   D3DDevice_SetFlickerFilter(global->console.screen.flicker_filter_index);
   D3DDevice_SetSoftDisplayFilter(global->console.softfilter_enable);
#endif

   if (chain->last_width != width || chain->last_height != height)
   {
      if (IDirect3DTexture8_LockRect(tex, 0, lr,
               NULL, D3DLOCK_NOSYSLOCK) == D3D_OK)
      {
         memset(lr->pBits, 0, chain->tex_h * lr->Pitch);
         IDirect3DTexture8_UnlockRect((LPDIRECT3DTEXTURE8)tex, 0);
      }
   }

   /* Set the texture to NULL so D3D doesn't complain about it being in use... */
   IDirect3DDevice8_SetTexture(d3dr, 0, NULL);

   if (IDirect3DTexture8_LockRect(tex, 0, &d3dlr, NULL, 0) == D3D_OK)
   {
      unsigned y;
      unsigned pixel_size = chain->pixel_size;

      for (y = 0; y < height; y++)
      {
         const uint8_t *in = (const uint8_t*)frame + y * pitch;
         uint8_t      *out = (uint8_t*)lr->pBits + y * lr->Pitch;
         memcpy(out, in, width * pixel_size);
      }
      IDirect3DTexture8_UnlockRect(tex, 0);
   }
}

static void d3d8_render(
      d3d8_video_t *d3d,
      LPDIRECT3DDEVICE8 d3dr,
      d3d8_renderchain_t *chain,
      const void *frame,
      unsigned frame_width, unsigned frame_height,
      unsigned pitch, unsigned rotation)
{
   settings_t *settings      = config_get_ptr();
   bool video_smooth         = settings->bools.video_smooth;

   d3d8_blit_to_texture(chain, frame, frame_width, frame_height, pitch);
   d3d8_set_vertices(d3d, chain, 1, frame_width, frame_height, chain->frame_count);

   IDirect3DDevice8_SetTexture(d3dr, 0,
         (IDirect3DBaseTexture8*)chain->tex);
   IDirect3DDevice8_SetTextureStageState(d3dr, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_MAGFILTER,
         video_smooth ?
         D3DTEXF_LINEAR : D3DTEXF_POINT);
   IDirect3DDevice8_SetTextureStageState(d3dr, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_MINFILTER,
         video_smooth ?
         D3DTEXF_LINEAR : D3DTEXF_POINT);

   IDirect3DDevice8_SetViewport(chain->dev, (D3DVIEWPORT8*)&d3d->out_vp);
   IDirect3DDevice8_SetVertexShader(d3dr,
         D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE);
   IDirect3DDevice8_SetStreamSource(d3dr,
         0, chain->vertex_buf, sizeof(Vertex));
   d3d8_set_mvp(d3d->dev, &d3d->mvp_rotate);
   IDirect3DDevice8_BeginScene(d3dr);
   IDirect3DDevice8_DrawPrimitive(d3dr, D3DPT_TRIANGLESTRIP, 0, 2);
   IDirect3DDevice8_EndScene(d3dr);

   chain->frame_count++;
}

static INLINE void *d3d8_vertex_buffer_new(
      LPDIRECT3DDEVICE8 dev,
      unsigned length, unsigned usage,
      unsigned fvf, D3DPOOL pool)
{
   void              *buf = NULL;
   if (FAILED(IDirect3DDevice8_CreateVertexBuffer(
               dev, length, usage, fvf,
               pool,
               (struct IDirect3DVertexBuffer8**)&buf)))
      return NULL;
   return buf;
}

static bool d3d8_setup_init(void *data,
      const video_info_t *video_info,
      void *dev_data,
      const struct LinkInfo *link_info,
      bool rgb32
      )
{
   d3d8_video_t *d3d                      = (d3d8_video_t*)data;
   settings_t *settings                   = config_get_ptr();
   LPDIRECT3DDEVICE8 d3dr                 = (LPDIRECT3DDEVICE8)d3d->dev;
   d3d8_renderchain_t *chain              = (d3d8_renderchain_t*)d3d->renderchain_data;
   unsigned fmt                           = (rgb32) ? RETRO_PIXEL_FORMAT_XRGB8888 : RETRO_PIXEL_FORMAT_RGB565;
   video_viewport_t *custom_vp            = &settings->video_vp_custom;
   unsigned width                         = d3d->vp.full_width;
   unsigned height                        = d3d->vp.full_height;

   chain->dev                             = dev_data;
   chain->pixel_size                      = (fmt == RETRO_PIXEL_FORMAT_RGB565)
      ? 2
      : 4;
   chain->tex_w                           = link_info->tex_w;
   chain->tex_h                           = link_info->tex_h;

   chain->vertex_buf                      = (LPDIRECT3DVERTEXBUFFER8)d3d8_vertex_buffer_new(d3dr, 4 * sizeof(Vertex),
         D3DUSAGE_WRITEONLY,
         D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE,
         D3DPOOL_MANAGED);

   if (!chain->vertex_buf)
      return false;

   chain->tex = (LPDIRECT3DTEXTURE8)d3d8_texture_new(d3dr,
         chain->tex_w, chain->tex_h, 1, 0,
         video_info->rgb32
         ?
         D3D8_XRGB8888_FORMAT : D3D8_RGB565_FORMAT,
         D3DPOOL_MANAGED, 0, 0, 0, NULL, NULL,
         false);

   if (!chain->tex)
      return false;

   IDirect3DDevice8_SetTextureStageState(d3dr, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_ADDRESSU, D3DTADDRESS_BORDER);
   IDirect3DDevice8_SetTextureStageState(d3dr, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_ADDRESSV, D3DTADDRESS_BORDER);
   IDirect3DDevice8_SetRenderState(d3dr, D3DRS_LIGHTING, 0);
   IDirect3DDevice8_SetRenderState(d3dr, D3DRS_CULLMODE, D3DCULL_NONE);
   IDirect3DDevice8_SetRenderState(d3dr, D3DRS_ZENABLE,  FALSE);

   /* FIXME */
   if (custom_vp->width == 0)
      custom_vp->width = width;

   if (custom_vp->height == 0)
      custom_vp->height = height;

   return true;
}

static void *d3d8_renderchain_new(void)
{
   d3d8_renderchain_t *renderchain = (d3d8_renderchain_t*)calloc(1, sizeof(*renderchain));
   if (!renderchain)
      return NULL;

   return renderchain;
}

/*
 * DISPLAY DRIVER
 */

static const float *gfx_display_d3d8_get_default_vertices(void)
{
   return &d3d8_vertexes[0];
}

static const float *gfx_display_d3d8_get_default_tex_coords(void)
{
   return &d3d8_tex_coords[0];
}

static void *gfx_display_d3d8_get_default_mvp(void *data)
{
   static float id[16] =       { 1.0f, 0.0f, 0.0f, 0.0f,
                                 0.0f, 1.0f, 0.0f, 0.0f,
                                 0.0f, 0.0f, 1.0f, 0.0f,
                                 0.0f, 0.0f, 0.0f, 1.0f
                               };
   return &id;
}

static void gfx_display_d3d8_blend_begin(void *data)
{
   d3d8_video_t *d3d             = (d3d8_video_t*)data;

   if (!d3d)
      return;

   IDirect3DDevice8_SetRenderState(d3d->dev, D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
   IDirect3DDevice8_SetRenderState(d3d->dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
   IDirect3DDevice8_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE, true);
}

static void gfx_display_d3d8_blend_end(void *data)
{
   d3d8_video_t *d3d             = (d3d8_video_t*)data;

   if (!d3d)
      return;

   IDirect3DDevice8_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE, false);
}

static void gfx_display_d3d8_draw(gfx_display_ctx_draw_t *draw,
      void *data,
      unsigned video_width,
      unsigned video_height)
{
   static float default_mvp[] ={ 1.0f, 0.0f, 0.0f, 0.0f,
                                 0.0f, 1.0f, 0.0f, 0.0f,
                                 0.0f, 0.0f, 1.0f, 0.0f,
                                 0.0f, 0.0f, 0.0f, 1.0f
                               };
   unsigned i;
   math_matrix_4x4 mop, m1, m2;
   LPDIRECT3DVERTEXBUFFER8 vbo;
   LPDIRECT3DDEVICE8 dev;
   unsigned start                = 0;
   unsigned count                = 0;
   d3d8_video_t *d3d             = (d3d8_video_t*)data;
   Vertex * pv                   = NULL;
   const float *vertex           = NULL;
   const float *tex_coord        = NULL;
   const float *color            = NULL;
   /* When the soft-scissor clipping path remaps UVs, it points
    * this at d3d->menu_display.scissor_uv and the per-vertex
    * read below uses it instead of draw->coords->tex_coord. */
   const float *clipped_uv       = NULL;

   if (!d3d || !draw)
      return;
   if (!draw->coords)
      return;

   /* Soft scissor.
    *
    * D3D8 has no SetScissorRect, so we approximate scissoring in
    * software inside the draw function itself.  Two strategies
    * depending on what the caller supplies:
    *
    *   - Default-vertex path (draw->coords->vertex == NULL, i.e.
    *     gfx_display_draw_quad): we have an axis-aligned screen
    *     rect from draw->x/y/width/height, plus a 4-element UV
    *     array (either the caller's tex_coord or the default
    *     [0..1] one).  We clip the rect against the scissor and
    *     remap the UVs proportionally so the visible portion of
    *     the texture still lands on the visible portion of the
    *     screen rect.  This is what Ozone's entry icons,
    *     selection borders and dividers use, and the fully
    *     correct path for the cases that overflow.
    *
    *   - Explicit-vertex path (draw->coords->vertex != NULL, i.e.
    *     gfx_display_draw_texture_slice with its 9 sub-quads):
    *     the geometry is already in normalised [0,1] screen
    *     space and clipping each of the 9 sub-quads with UV
    *     remap is too invasive.  Fall back to skip-only — only
    *     drop sub-quads whose bounding box lies entirely outside
    *     the scissor rect.
    *
    * Note that gfx_display_draw_quad converts the caller's
    * top-down Y into bottom-up via draw->y = height - y - h.  We
    * convert back to top-down here for the comparison and back
    * again on the way out, so callers don't notice. */
   if (d3d->menu_display.scissor_active)
   {
      int sx  = d3d->menu_display.scissor_x;
      int sy  = d3d->menu_display.scissor_y;
      int sx2 = sx + d3d->menu_display.scissor_w;
      int sy2 = sy + d3d->menu_display.scissor_h;

      if (draw->coords->vertex)
      {
         /* Skip-only path for explicit-vertex draws.  Build a
          * bounding box from the vertex array (normalised
          * [0,1] screen space, Y bottom-up) and skip if it's
          * entirely outside the scissor. */
         float vmin_x = draw->coords->vertex[0];
         float vmin_y = draw->coords->vertex[1];
         float vmax_x = vmin_x;
         float vmax_y = vmin_y;
         int qx, qy, qx2, qy2;
         unsigned vi;
         for (vi = 1; vi < draw->coords->vertices; vi++)
         {
            float vx = draw->coords->vertex[vi * 2 + 0];
            float vy = draw->coords->vertex[vi * 2 + 1];
            if (vx < vmin_x) vmin_x = vx;
            if (vx > vmax_x) vmax_x = vx;
            if (vy < vmin_y) vmin_y = vy;
            if (vy > vmax_y) vmax_y = vy;
         }
         qx  = (int)(vmin_x * (float)video_width);
         qx2 = (int)(vmax_x * (float)video_width);
         qy2 = (int)((1.0f - vmin_y) * (float)video_height);
         qy  = (int)((1.0f - vmax_y) * (float)video_height);

         if (qx2 <= sx || qx >= sx2 || qy2 <= sy || qy >= sy2)
            return;
      }
      else
      {
         /* Geometry-clipping path for default-vertex draws.
          * Clip the screen rect against the scissor and remap
          * the UVs proportionally; we mutate draw->x/y/w/h and
          * a local UV copy in place, then fall through to the
          * normal rendering code with the clipped values. */
         int qx_left  = draw->x;
         int qx_right = draw->x + (int)draw->width;
         int qy_bot   = (int)video_height - draw->y;             /* top-down */
         int qy_top   = qy_bot - (int)draw->height;              /* top-down */
         int new_left  = qx_left  > sx  ? qx_left  : sx;
         int new_right = qx_right < sx2 ? qx_right : sx2;
         int new_top   = qy_top   > sy  ? qy_top   : sy;
         int new_bot   = qy_bot   < sy2 ? qy_bot   : sy2;

         if (new_left >= new_right || new_top >= new_bot)
            return;

         /* Only mutate if the rect actually clips, to keep the
          * common (no overlap with scissor edges) path free of
          * UV remapping noise and to avoid the float roundtrip. */
         if (   new_left != qx_left || new_right != qx_right
             || new_top  != qy_top  || new_bot   != qy_bot)
         {
            const float *src_uv = draw->coords->tex_coord
               ? draw->coords->tex_coord
               : &d3d8_tex_coords[0];
            float w_orig = (float)draw->width;
            float h_orig = (float)draw->height;
            float fx_l   = (float)(new_left  - qx_left) / w_orig;
            float fx_r   = (float)(new_right - qx_left) / w_orig;
            float fy_t   = (float)(new_top   - qy_top)  / h_orig;
            float fy_b   = (float)(new_bot   - qy_top)  / h_orig;
            /* Source UVs in BL,BR,TL,TR order match d3d8_tex_coords:
             *   src_uv[0,1] = BL    src_uv[2,3] = BR
             *   src_uv[4,5] = TL    src_uv[6,7] = TR
             * The four corners share U-left/U-right and V-top/V-bot,
             * so derive those from BL/BR (U) and TL/BL (V). */
            float u_l_orig = src_uv[0];                           /* BL.u */
            float u_r_orig = src_uv[2];                           /* BR.u */
            float v_t_orig = src_uv[5];                           /* TL.v */
            float v_b_orig = src_uv[1];                           /* BL.v */
            float u_l_new  = u_l_orig + fx_l * (u_r_orig - u_l_orig);
            float u_r_new  = u_l_orig + fx_r * (u_r_orig - u_l_orig);
            /* V interpolates from v_t_orig (top, fy=0) to v_b_orig
             * (bot, fy=1), i.e. fy_t/fy_b are along the top->bot
             * axis.  The default tex coord array has TL.v=0 and
             * BL.v=1 so V grows downward, matching D3D convention. */
            float v_t_new  = v_t_orig + fy_t * (v_b_orig - v_t_orig);
            float v_b_new  = v_t_orig + fy_b * (v_b_orig - v_t_orig);

            /* Write clipped UVs into a local 4-corner array and
             * point the local tex_coord pointer at it.  Layout
             * matches d3d8_tex_coords: BL, BR, TL, TR. */
            d3d->menu_display.scissor_uv[0] = u_l_new;
            d3d->menu_display.scissor_uv[1] = v_b_new;
            d3d->menu_display.scissor_uv[2] = u_r_new;
            d3d->menu_display.scissor_uv[3] = v_b_new;
            d3d->menu_display.scissor_uv[4] = u_l_new;
            d3d->menu_display.scissor_uv[5] = v_t_new;
            d3d->menu_display.scissor_uv[6] = u_r_new;
            d3d->menu_display.scissor_uv[7] = v_t_new;
            clipped_uv = d3d->menu_display.scissor_uv;

            /* Now mutate the screen rect to the clipped one.
             * Convert new_bot back to bottom-up Y for draw->y. */
            draw->x      = new_left;
            draw->y      = (int)video_height - new_bot;
            draw->width  = (unsigned)(new_right - new_left);
            draw->height = (unsigned)(new_bot - new_top);
         }
      }
   }

   if ((d3d->menu_display.offset + draw->coords->vertices )
         > (unsigned)d3d->menu_display.size)
      return;
   vbo                           = (LPDIRECT3DVERTEXBUFFER8)d3d->menu_display.buffer;
   dev                           = d3d->dev;
   pv                            = (Vertex*)d3d8_vertex_buffer_lock(vbo);

   if (!pv)
      return;

   pv          += d3d->menu_display.offset;
   vertex       = draw->coords->vertex;
   tex_coord    = clipped_uv ? clipped_uv : draw->coords->tex_coord;
   color        = draw->coords->color;

   if (!vertex)
      vertex    = &d3d8_vertexes[0];
   if (!tex_coord)
      tex_coord = &d3d8_tex_coords[0];
   if (!color)
   {
      /* Default to opaque white when caller provides no color
       * array — matches the behaviour of the d3d9/d3d10/d3d11
       * gfx_display drivers and avoids dereferencing NULL on
       * pipeline/dispca-driven draws. */
      static const float default_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
      color     = &default_color[0];
   }

   for (i = 0; i < draw->coords->vertices; i++)
   {
      int colors[4];
      const float *cp = color;

      colors[0]   = *cp++ * 0xFF;
      colors[1]   = *cp++ * 0xFF;
      colors[2]   = *cp++ * 0xFF;
      colors[3]   = *cp++ * 0xFF;

      /* Advance the color pointer only when the caller actually
       * provided a per-vertex color array; if we fell back to the
       * static default above, reuse that single RGBA for every
       * vertex. */
      if (draw->coords->color)
         color = cp;

      pv[i].x     = *vertex++;
      pv[i].y     = *vertex++;
      pv[i].z     = 0.5f;
      pv[i].u     = *tex_coord++;
      pv[i].v     = *tex_coord++;

      pv[i].color =
         D3DCOLOR_ARGB(
               colors[3], /* A */
               colors[0], /* R */
               colors[1], /* G */
               colors[2]  /* B */
               );
   }
   IDirect3DVertexBuffer8_Unlock(vbo);

   if (!draw->matrix_data)
      draw->matrix_data = &default_mvp;

   /* ugh */
   matrix_4x4_scale(m1,       2.0,  2.0, 0);
   matrix_4x4_translate(mop, -1.0, -1.0, 0);
   matrix_4x4_multiply(m2, mop, m1);
   matrix_4x4_multiply(m1,
         *((math_matrix_4x4*)draw->matrix_data), m2);
   matrix_4x4_scale(mop,
         (draw->width  / 2.0) / video_width,
         (draw->height / 2.0) / video_height, 0);
   matrix_4x4_multiply(m2, mop, m1);
   matrix_4x4_translate(mop,
         (draw->x + (draw->width  / 2.0)) / video_width,
         (draw->y + (draw->height / 2.0)) / video_height,
         0);
   matrix_4x4_multiply(m1, mop, m2);
   matrix_4x4_multiply(m2, d3d->mvp_transposed, m1);
   matrix_4x4_transpose(m1, m2);

   d3d8_set_mvp(dev, &m1);

   if (draw->texture)
   {
      IDirect3DDevice8_SetTexture(dev, 0,
            (IDirect3DBaseTexture8*)draw->texture);
      IDirect3DDevice8_SetTextureStageState(dev, 0,
            (D3DTEXTURESTAGESTATETYPE)D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
      IDirect3DDevice8_SetTextureStageState(dev, 0,
            (D3DTEXTURESTAGESTATETYPE)D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
      IDirect3DDevice8_SetTextureStageState(dev, 0,
            (D3DTEXTURESTAGESTATETYPE)D3DTSS_MINFILTER, D3DTEXF_LINEAR);
      IDirect3DDevice8_SetTextureStageState(dev, 0,
            (D3DTEXTURESTAGESTATETYPE)D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
   }
   else
   {
      /* Untextured draw — clear any stale texture binding (the
       * font atlas left bound after font_driver_render_msg, the
       * libretro frame texture from d3d8_render, etc.) so the
       * default texture-stage MODULATE doesn't multiply the
       * per-vertex DIFFUSE colour against an unrelated sample.
       * Without this, divider lines and selection highlights in
       * Ozone/XMB/MaterialUI would pick up whatever texture was
       * last bound and render as garbage or invisibly. */
      IDirect3DDevice8_SetTexture(dev, 0, NULL);
   }

   /* Force the alpha pipeline to MODULATE(TEXTURE, DIFFUSE).
    *
    * The fixed-function default for stage 0 is
    *   COLOROP = MODULATE,   COLORARG1 = TEXTURE, COLORARG2 = CURRENT
    *   ALPHAOP = SELECTARG1, ALPHAARG1 = TEXTURE
    * which means the colour channel correctly multiplies the
    * texture sample by the per-vertex DIFFUSE colour, but the
    * alpha channel ignores DIFFUSE entirely and just selects the
    * texture's alpha.  For an opaque texture (the common case —
    * gfx_white_texture, icon atlases, the Ozone cursor texture)
    * that means a draw whose only opacity comes from per-vertex
    * alpha (e.g. a fading-out "old" cursor at alpha=0, or a
    * semi-transparent footer fill) renders fully opaque instead
    * of fading.
    *
    * Switching ALPHAOP to MODULATE makes the alpha output equal
    * texture.alpha * diffuse.alpha, which is what every menu
    * caller expects (and what the d3d9/d3d10/d3d11 stock shaders
    * compute explicitly).  COLOROP stays at its default. */
   IDirect3DDevice8_SetTextureStageState(dev, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
   IDirect3DDevice8_SetTextureStageState(dev, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
   IDirect3DDevice8_SetTextureStageState(dev, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

   start = d3d->menu_display.offset;

   /* Menu draws issued by gfx_display always pass a triangle-strip layout
    * (4 vertices = 2 triangles for a quad).  D3D8 expects PrimitiveCount,
    * not vertex count, hence (vertices - 2).  Guard against vertices < 3
    * which would underflow the unsigned subtraction and pass a huge
    * primitive count to the GPU. */
   if (draw->coords->vertices < 3)
   {
      d3d->menu_display.offset += draw->coords->vertices;
      return;
   }
   count = draw->coords->vertices - 2;

   IDirect3DDevice8_BeginScene(dev);
   IDirect3DDevice8_DrawPrimitive(dev, D3DPT_TRIANGLESTRIP, start, count);
   IDirect3DDevice8_EndScene(dev);

   d3d->menu_display.offset += draw->coords->vertices;
}

/* Set up render state for one of the menu pipeline draws (XMB
 * ribbon backgrounds, snow/bokeh particle effects, etc.).
 *
 * D3D8 has no programmable shader path here — the actual menu
 * shaders the other backends compile (ribbon_sm3, simple_snow_sm3,
 * snowflake_sm3, bokeh_sm3) need at minimum pixel shader 2.0 to
 * fit; PS 1.x cannot represent the per-fragment noise math. So
 * for d3d8 we deliberately skip programmable shading entirely and
 * only do the work that *can* be done in fixed function:
 *
 *   - Hand the dispca coordinate array to the caller via
 *     draw->coords so the subsequent gfx_display_d3d8_draw call
 *     has geometry to render.
 *   - Set the per-pipeline blend mode so the geometry composites
 *     against the background the way XMB expects (ribbon uses
 *     multiplicative DESTCOLOR+ONE, particle effects use the
 *     usual SRCALPHA / INVSRCALPHA premultiplied path).
 *
 * The result is that the ribbon and particle layers render as
 * static geometry rather than the animated shader effect — the
 * menu still composes correctly, just without the eye-candy.
 */
static void gfx_display_d3d8_draw_pipeline(
      gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp,
      void *data, unsigned video_width, unsigned video_height)
{
   video_coord_array_t *ca;
   d3d8_video_t *d3d = (d3d8_video_t*)data;

   if (!d3d || !draw || !p_disp)
      return;

   ca                = &p_disp->dispca;

   /* Position the geometry at the origin and clear any inherited
    * MVP — gfx_display_d3d8_draw will fall back to identity. */
   draw->x           = 0;
   draw->y           = 0;
   draw->matrix_data = NULL;

   if (ca)
      draw->coords   = (struct video_coords*)&ca->coords;

   switch (draw->pipeline_id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
         /* XMB ribbon: multiplicative blend so the ribbon mesh
          * darkens / tints whatever is behind it. Matches the
          * blend setup d3d10/d3d11 use for the ribbon pass. */
         IDirect3DDevice8_SetRenderState(d3d->dev,
               D3DRS_SRCBLEND,         D3DBLEND_DESTCOLOR);
         IDirect3DDevice8_SetRenderState(d3d->dev,
               D3DRS_DESTBLEND,        D3DBLEND_ONE);
         IDirect3DDevice8_SetRenderState(d3d->dev,
               D3DRS_ALPHABLENDENABLE, TRUE);
         break;

      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         /* Snow / bokeh / snowflake: standard alpha blend. The
          * dispca geometry alone won't produce a particle effect
          * without the pixel shader, but at least the blend mode
          * is consistent so any text/icons drawn afterwards don't
          * inherit a stale state. */
         IDirect3DDevice8_SetRenderState(d3d->dev,
               D3DRS_SRCBLEND,         D3DBLEND_SRCALPHA);
         IDirect3DDevice8_SetRenderState(d3d->dev,
               D3DRS_DESTBLEND,        D3DBLEND_INVSRCALPHA);
         IDirect3DDevice8_SetRenderState(d3d->dev,
               D3DRS_ALPHABLENDENABLE, TRUE);
         break;

      default:
         /* Unknown pipeline ID — leave blend state alone and let
          * the regular draw path render whatever was set up. */
         break;
   }
}

/* Soft scissor for D3D8.
 *
 * D3D8 has no SetScissorRect (added in D3D9) and no
 * D3DRS_SCISSORTESTENABLE.  Two workarounds are possible:
 *
 *   1. Shrink the viewport to the requested rect.  This does not
 *      clip — it transforms full-screen geometry into the smaller
 *      rect — so it produces visibly squashed text/icons when
 *      callers (e.g. Ozone's sidebar pass) draw at full-screen
 *      coordinates expecting clipping.
 *
 *   2. Software clipping in gfx_display_d3d8_draw — skip any draw
 *      whose screen-space bounding box lies entirely outside the
 *      requested rect.  This is partial — partially-overlapping
 *      draws still render in full — but it stops fully-outside
 *      draws (the entry-list overflow that spills onto Ozone's
 *      footer) which is the visible artifact users actually
 *      notice.  True geometry clipping (per-vertex remap with UV
 *      adjustment) would be invasive to do for every quad and is
 *      not worth the complexity for a fallback-quality backend.
 *
 * scissor_begin stores the rect; scissor_end clears it; the draw
 * function consults the rect when active. */
static void gfx_display_d3d8_scissor_begin(
      void *data,
      unsigned video_width, unsigned video_height,
      int x, int y, unsigned width, unsigned height)
{
   d3d8_video_t *d3d = (d3d8_video_t*)data;

   if (!d3d)
      return;

   d3d->menu_display.scissor_x      = x;
   d3d->menu_display.scissor_y      = y;
   d3d->menu_display.scissor_w      = (int)width;
   d3d->menu_display.scissor_h      = (int)height;
   d3d->menu_display.scissor_active = true;
}

static void gfx_display_d3d8_scissor_end(void *data,
      unsigned video_width, unsigned video_height)
{
   d3d8_video_t *d3d = (d3d8_video_t*)data;

   if (!d3d)
      return;

   d3d->menu_display.scissor_active = false;
}

/*
 * FONT DRIVER
 *
 * Fixed-function font renderer for D3D8.  Mirrors the structure of
 * d3d9_font in d3d9hlsl.c but uses the D3D8 texture-stage-state
 * APIs (D3D8 has no programmable shaders or sampler-state objects)
 * and an FVF instead of a vertex declaration.  The atlas is an A8
 * buffer that we expand to A8R8G8B8 with white RGB; the per-glyph
 * tint comes from the per-vertex DIFFUSE colour, modulated by the
 * texture sample in the default fixed-function combiner.
 */

typedef struct
{
   LPDIRECT3DTEXTURE8            texture;
   const font_renderer_driver_t *font_driver;
   void                         *font_data;
   struct font_atlas             *atlas;
   unsigned                      tex_width;
   unsigned                      tex_height;
   /* Scratch buffer to avoid per-line malloc/free in font rendering. */
   Vertex                       *scratch_verts;
   unsigned                      scratch_capacity; /* in Vertex count */
} d3d8_font_t;

static void d3d8_font_upload_atlas(d3d8_font_t *font)
{
   D3DLOCKED_RECT lr;
   unsigned i, j;

   if (!font->texture)
      return;

   if (FAILED(IDirect3DTexture8_LockRect(font->texture, 0, &lr, NULL, 0)))
      return;

   for (j = 0; j < font->atlas->height; j++)
   {
      uint32_t      *dst = (uint32_t*)((uint8_t*)lr.pBits + j * lr.Pitch);
      const uint8_t *src = font->atlas->buffer + j * font->atlas->width;
      for (i = 0; i < font->atlas->width; i++)
         dst[i] = D3DCOLOR_ARGB(src[i], 0xFF, 0xFF, 0xFF);
   }

   IDirect3DTexture8_UnlockRect(font->texture, 0);
}

static void *d3d8_font_init(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   d3d8_video_t *d3d = (d3d8_video_t*)data;
   d3d8_font_t  *font = (d3d8_font_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   if (!font_renderer_create_default(
            &font->font_driver, &font->font_data,
            font_path, font_size))
   {
      free(font);
      return NULL;
   }

   font->atlas      = font->font_driver->get_atlas(font->font_data);
   font->tex_width  = font->atlas->width;
   font->tex_height = font->atlas->height;

   /* D3D8 doesn't universally support D3DFMT_A8 as a texture format,
    * so expand the A8 atlas into A8R8G8B8 (white RGB, alpha = atlas
    * sample).  The colour modulation against the per-vertex diffuse
    * is done by the default fixed-function texture stage state. */
   font->texture = (LPDIRECT3DTEXTURE8)d3d8_texture_new(d3d->dev,
         font->tex_width, font->tex_height, 1,
         0, D3D8_ARGB8888_FORMAT,
         D3DPOOL_MANAGED, 0, 0, 0, NULL, NULL, false);

   if (font->texture)
      d3d8_font_upload_atlas(font);

   font->atlas->dirty = false;
   return font;
}

static void d3d8_font_free(void *data, bool is_threaded)
{
   d3d8_font_t *font = (d3d8_font_t*)data;

   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   if (font->texture)
      IDirect3DTexture8_Release(font->texture);

   free(font->scratch_verts);
   free(font);
}

static int d3d8_font_get_message_width(void *data,
      const char *msg, size_t msg_len, float scale)
{
   size_t i;
   int delta_x                      = 0;
   const struct font_glyph *glyph_q = NULL;
   d3d8_font_t *font                = (d3d8_font_t*)data;

   if (!font)
      return 0;

   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph *glyph;
      const char *msg_tmp = &msg[i];
      unsigned    code    = utf8_walk(&msg_tmp);
      unsigned    skip    = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      if (!(glyph = font->font_driver->get_glyph(font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      delta_x += glyph->advance_x;
   }

   return delta_x * scale;
}

/* Emit a single glyph quad (6 vertices, two triangles) into pv.
 * Returns the number of vertices written (always 6). */
static INLINE unsigned d3d8_font_emit_quad(
      Vertex *pv,
      float x, float y, float w, float h,
      float tex_u, float tex_v, float tex_w, float tex_h,
      D3DCOLOR color)
{
   pv[0].x     = x;
   pv[0].y     = y;
   pv[0].z     = 0.5f;
   pv[0].u     = tex_u;
   pv[0].v     = tex_v;
   pv[0].color = color;

   pv[1].x     = x + w;
   pv[1].y     = y;
   pv[1].z     = 0.5f;
   pv[1].u     = tex_u + tex_w;
   pv[1].v     = tex_v;
   pv[1].color = color;

   pv[2].x     = x;
   pv[2].y     = y + h;
   pv[2].z     = 0.5f;
   pv[2].u     = tex_u;
   pv[2].v     = tex_v + tex_h;
   pv[2].color = color;

   pv[3].x     = x + w;
   pv[3].y     = y;
   pv[3].z     = 0.5f;
   pv[3].u     = tex_u + tex_w;
   pv[3].v     = tex_v;
   pv[3].color = color;

   pv[4].x     = x + w;
   pv[4].y     = y + h;
   pv[4].z     = 0.5f;
   pv[4].u     = tex_u + tex_w;
   pv[4].v     = tex_v + tex_h;
   pv[4].color = color;

   pv[5].x     = x;
   pv[5].y     = y + h;
   pv[5].z     = 0.5f;
   pv[5].u     = tex_u;
   pv[5].v     = tex_v + tex_h;
   pv[5].color = color;

   return 6;
}

static INLINE Vertex *d3d8_font_get_scratch(
      d3d8_font_t *font, unsigned needed)
{
   if (needed > font->scratch_capacity)
   {
      unsigned new_cap = needed > 1536 ? needed : 1536; /* 256 glyphs * 6 verts */
      Vertex *tmp      = (Vertex*)realloc(font->scratch_verts,
            new_cap * sizeof(Vertex));
      if (!tmp)
         return NULL;
      font->scratch_verts    = tmp;
      font->scratch_capacity = new_cap;
   }
   memset(font->scratch_verts, 0, needed * sizeof(Vertex));
   return font->scratch_verts;
}

/* Render a single line of glyphs from `m` of length `msg_len` at
 * (line_x, line_y) in [0..1] coords, with the supplied colour.
 * Used for both the drop-shadow pass and the main text pass. */
static void d3d8_font_render_line(
      d3d8_video_t *d3d,
      d3d8_font_t  *font,
      const char   *m,
      size_t        msg_len,
      float         line_x,
      float         line_y,
      float         scale,
      enum text_alignment text_align,
      unsigned      width,
      unsigned      height,
      D3DCOLOR      color)
{
   unsigned i;
   float inv_viewport_w             = 1.0f / (float)width;
   float inv_viewport_h             = 1.0f / (float)height;
   float inv_tex_w                  = 1.0f / (float)font->tex_width;
   float inv_tex_h                  = 1.0f / (float)font->tex_height;
   const struct font_glyph *glyph_q = font->font_driver->get_glyph(
         font->font_data, '?');
   int lx                           = roundf(line_x * width);
   int ly                           = roundf((1.0f - line_y) * height);
   unsigned vert_count              = 0;
   Vertex *verts                    = d3d8_font_get_scratch(font, msg_len * 6);

   if (!verts)
      return;

   /* Soft scissor for text.  The font path doesn't go through
    * gfx_display_d3d8_draw, so apply a similar skip-only check
    * here.  ly is the baseline in top-down screen pixels.
    * Visible glyphs sit at or above ly (the baseline is the
    * bottom of the line for most glyphs; descenders dip slightly
    * below).  We don't know the line height here, but two
    * conservative whole-line culls catch the cases that actually
    * overflow in practice:
    *
    *   - ly >= sy2: baseline at or below the scissor bottom edge
    *     means the whole line (which is above the baseline) is
    *     mostly below the scissor.  In Ozone that's the entry
    *     that has just scrolled past the footer.
    *   - ly < sy:   baseline above the scissor top edge means
    *     the whole line is above sy — every visible glyph sits
    *     above its own baseline, so above sy too.  In Ozone
    *     that's the entry that has just scrolled past the
    *     header.
    *
    * Edge-aligned lines (baseline ~ sy or ~ sy2) still render in
    * full — partial overlap isn't culled.  Pixel-perfect glyph
    * clipping would need per-glyph bounding-box checks; the
    * whole-line cull is enough to stop the visible overflow into
    * Ozone's header/footer regions. */
   if (d3d->menu_display.scissor_active)
   {
      int sy  = d3d->menu_display.scissor_y;
      int sy2 = sy + d3d->menu_display.scissor_h;
      if (ly >= sy2 || ly < sy)
         return;
   }

   if (text_align == TEXT_ALIGN_RIGHT || text_align == TEXT_ALIGN_CENTER)
   {
      int width_accum      = 0;
      const char *scan     = m;
      const char *scan_end = m + msg_len;
      while (scan < scan_end)
      {
         const struct font_glyph *glyph;
         uint32_t code = utf8_walk(&scan);
         if (!(glyph = font->font_driver->get_glyph(font->font_data, code)))
            if (!(glyph = glyph_q))
               continue;
         width_accum += glyph->advance_x;
      }
      if (text_align == TEXT_ALIGN_RIGHT)
         line_x -= (float)(width_accum * scale) / (float)width;
      else
         line_x -= (float)(width_accum * scale) / (float)width / 2.0f;
      lx = roundf(line_x * width);
   }

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph *glyph;
      const char *msg_tmp = &m[i];
      unsigned    code    = utf8_walk(&msg_tmp);
      unsigned    skip    = msg_tmp - &m[i];

      if (skip > 1)
         i += skip - 1;

      if (!(glyph = font->font_driver->get_glyph(font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      vert_count += d3d8_font_emit_quad(
            &verts[vert_count],
            (lx + glyph->draw_offset_x * scale) * inv_viewport_w,
            (ly + glyph->draw_offset_y * scale) * inv_viewport_h,
            glyph->width  * scale * inv_viewport_w,
            glyph->height * scale * inv_viewport_h,
            glyph->atlas_offset_x * inv_tex_w,
            glyph->atlas_offset_y * inv_tex_h,
            glyph->width  * inv_tex_w,
            glyph->height * inv_tex_h,
            color);

      lx += glyph->advance_x * scale;
      ly += glyph->advance_y * scale;
   }

   if (vert_count == 0)
      return;

   IDirect3DDevice8_SetTexture(d3d->dev, 0,
         (IDirect3DBaseTexture8*)font->texture);
   IDirect3DDevice8_SetTextureStageState(d3d->dev, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
   IDirect3DDevice8_SetTextureStageState(d3d->dev, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
   IDirect3DDevice8_SetTextureStageState(d3d->dev, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_MINFILTER, D3DTEXF_LINEAR);
   IDirect3DDevice8_SetTextureStageState(d3d->dev, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_MAGFILTER, D3DTEXF_LINEAR);

   /* MODULATE the atlas alpha with the per-vertex DIFFUSE alpha
    * so callers can fade glyphs in/out via the colour parameter
    * (drop-shadow alpha, animation fades, etc).  Without this the
    * default ALPHAOP=SELECTARG1+TEXTURE makes glyph alpha equal
    * the atlas coverage only, ignoring the requested fade. */
   IDirect3DDevice8_SetTextureStageState(d3d->dev, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
   IDirect3DDevice8_SetTextureStageState(d3d->dev, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
   IDirect3DDevice8_SetTextureStageState(d3d->dev, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

   IDirect3DDevice8_BeginScene(d3d->dev);
   IDirect3DDevice8_DrawPrimitiveUP(d3d->dev,
         D3DPT_TRIANGLELIST,
         vert_count / 3,
         verts,
         sizeof(Vertex));
   IDirect3DDevice8_EndScene(d3d->dev);
}

static void d3d8_font_render_msg(
      void *userdata, void *data,
      const char *msg,
      const struct font_params *params)
{
   float x, y, scale, drop_mod, drop_alpha;
   enum text_alignment text_align;
   int drop_x, drop_y;
   unsigned r, g, b, alpha;
   D3DCOLOR color, color_dark = 0;
   struct font_line_metrics *line_metrics = NULL;
   float line_height;
   d3d8_font_t  *font  = (d3d8_font_t*)data;
   d3d8_video_t *d3d   = (d3d8_video_t*)userdata;
   unsigned      width  = 0;
   unsigned      height = 0;
   /* Top-down ortho mapping x[0..1]→[-1..1], y[0..1]→[1..-1] so
    * (0,0) is the top-left corner.  D3D uses row-vector convention
    * (v_clip = v · M) and stores D3DMATRIX in row-major order, so
    * we write the matrix below in its natural row layout and pass
    * it directly to SetTransform (bypassing d3d8_set_mvp which is
    * tuned for column-major math_matrix_4x4 input from the menu
    * draw path). */
   static const D3DMATRIX topdown_ortho_d3d = {
      {{
          2.0f,  0.0f, 0.0f, 0.0f,  /* row 0 */
          0.0f, -2.0f, 0.0f, 0.0f,  /* row 1 */
          0.0f,  0.0f, 1.0f, 0.0f,  /* row 2 */
         -1.0f,  1.0f, 0.0f, 1.0f   /* row 3 */
      }}
   };
   static const math_matrix_4x4 identity = {{
      1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f
   }};

   if (!font || !msg || !*msg)
      return;
   if (!d3d)
      return;

   width  = d3d->vp.full_width;
   height = d3d->vp.full_height;
   if (!width || !height)
      return;

   if (params)
   {
      x          = params->x;
      y          = params->y;
      scale      = params->scale;
      text_align = params->text_align;
      drop_x     = params->drop_x;
      drop_y     = params->drop_y;
      drop_mod   = params->drop_mod;
      drop_alpha = params->drop_alpha;

      r          = FONT_COLOR_GET_RED(params->color);
      g          = FONT_COLOR_GET_GREEN(params->color);
      b          = FONT_COLOR_GET_BLUE(params->color);
      alpha      = FONT_COLOR_GET_ALPHA(params->color);

      color      = D3DCOLOR_ARGB(alpha, r, g, b);
   }
   else
   {
      settings_t *settings    = config_get_ptr();
      float video_msg_pos_x   = settings->floats.video_msg_pos_x;
      float video_msg_pos_y   = settings->floats.video_msg_pos_y;
      float video_msg_color_r = settings->floats.video_msg_color_r;
      float video_msg_color_g = settings->floats.video_msg_color_g;
      float video_msg_color_b = settings->floats.video_msg_color_b;

      x          = video_msg_pos_x;
      y          = video_msg_pos_y;
      scale      = 1.0f;
      text_align = TEXT_ALIGN_LEFT;

      r          = (unsigned)(video_msg_color_r * 255);
      g          = (unsigned)(video_msg_color_g * 255);
      b          = (unsigned)(video_msg_color_b * 255);
      alpha      = 255;
      color      = D3DCOLOR_ARGB(alpha, r, g, b);

      drop_x     = -2;
      drop_y     = -2;
      drop_mod   = 0.3f;
      drop_alpha = 1.0f;
   }

   font->font_driver->get_line_metrics(font->font_data, &line_metrics);
   line_height = line_metrics->height * scale / height;

   /* Standard premultiplied-alpha blend for glyph compositing. */
   IDirect3DDevice8_SetRenderState(d3d->dev,
         D3DRS_SRCBLEND,         D3DBLEND_SRCALPHA);
   IDirect3DDevice8_SetRenderState(d3d->dev,
         D3DRS_DESTBLEND,        D3DBLEND_INVSRCALPHA);
   IDirect3DDevice8_SetRenderState(d3d->dev,
         D3DRS_ALPHABLENDENABLE, TRUE);

   /* FVF is shared with the rest of the menu draw path. Set it
    * defensively in case a previous stage left a different format
    * bound (e.g. the renderchain's vertex format). */
   IDirect3DDevice8_SetVertexShader(d3d->dev,
         D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE);

   /* Apply top-down ortho. SetTransform consumes a row-major
    * D3DMATRIX directly. PROJ and VIEW are forced to identity so
    * the WORLD transform alone produces clip space. */
   IDirect3DDevice8_SetTransform(d3d->dev, D3DTS_PROJECTION,
         (D3DMATRIX*)&identity);
   IDirect3DDevice8_SetTransform(d3d->dev, D3DTS_VIEW,
         (D3DMATRIX*)&identity);
   IDirect3DDevice8_SetTransform(d3d->dev, D3DTS_WORLD,
         &topdown_ortho_d3d);

   /* Refresh the atlas if the glyph cache has grown or new glyphs
    * have been emitted since the last frame. */
   if (font->atlas->dirty)
   {
      if (   font->atlas->width  != font->tex_width
          || font->atlas->height != font->tex_height)
      {
         if (font->texture)
            IDirect3DTexture8_Release(font->texture);

         font->tex_width  = font->atlas->width;
         font->tex_height = font->atlas->height;
         font->texture    = (LPDIRECT3DTEXTURE8)d3d8_texture_new(d3d->dev,
               font->tex_width, font->tex_height, 1,
               0, D3D8_ARGB8888_FORMAT,
               D3DPOOL_MANAGED, 0, 0, 0, NULL, NULL, false);
      }

      d3d8_font_upload_atlas(font);
      font->atlas->dirty = false;
   }

   {
      int lines     = 0;
      bool has_drop = drop_x || drop_y;
      const char *m = msg;

      if (has_drop)
      {
         unsigned r_dark     = r * drop_mod;
         unsigned g_dark     = g * drop_mod;
         unsigned b_dark     = b * drop_mod;
         unsigned alpha_dark = alpha * drop_alpha;
         color_dark          = D3DCOLOR_ARGB(alpha_dark, r_dark, g_dark, b_dark);
      }

      for (;;)
      {
         const char *end = m;
         size_t msg_len;

         while (*end && *end != '\n')
            end++;
         msg_len = (size_t)(end - m);

         if (msg_len > 0)
         {
            float line_y = y - (float)lines * line_height;

            /* Drop shadow pass. */
            if (has_drop)
            {
               float drop_pos_x = x + scale * drop_x / (float)width;
               float drop_pos_y = line_y + scale * drop_y / (float)height;
               d3d8_font_render_line(d3d, font, m, msg_len,
                     drop_pos_x, drop_pos_y, scale, text_align,
                     width, height, color_dark);
            }

            /* Main text pass. */
            d3d8_font_render_line(d3d, font, m, msg_len,
                  x, line_y, scale, text_align,
                  width, height, color);
         }

         if (*end != '\n')
            break;
         m = end + 1;
         lines++;
      }
   }

   /* Restore the menu vertex stream so subsequent gfx_display_d3d8_draw
    * calls see the correct buffer.  d3d9hlsl does this between every
    * DrawPrimitiveUP; on d3d8 we only need it once at the end since
    * the sole stream switch is to the UP path. */
   IDirect3DDevice8_SetStreamSource(d3d->dev, 0,
         (LPDIRECT3DVERTEXBUFFER8)d3d->menu_display.buffer,
         sizeof(Vertex));
}

static const struct font_glyph *d3d8_font_get_glyph(
      void *data, uint32_t code)
{
   d3d8_font_t *font = (d3d8_font_t*)data;
   if (font && font->font_driver)
      return font->font_driver->get_glyph(
            (void*)font->font_data, code);
   return NULL;
}

static bool d3d8_font_get_line_metrics(
      void *data, struct font_line_metrics **metrics)
{
   d3d8_font_t *font = (d3d8_font_t*)data;
   if (font && font->font_driver && font->font_data)
   {
      font->font_driver->get_line_metrics(font->font_data, metrics);
      return true;
   }
   return false;
}

font_renderer_t d3d8_font = {
   d3d8_font_init,
   d3d8_font_free,
   d3d8_font_render_msg,
   "d3d8",
   d3d8_font_get_glyph,
   NULL, /* bind_block */
   NULL, /* flush */
   d3d8_font_get_message_width,
   d3d8_font_get_line_metrics
};

gfx_display_ctx_driver_t gfx_display_ctx_d3d8 = {
   gfx_display_d3d8_draw,
   gfx_display_d3d8_draw_pipeline,
   gfx_display_d3d8_blend_begin,
   gfx_display_d3d8_blend_end,
   gfx_display_d3d8_get_default_mvp,
   gfx_display_d3d8_get_default_vertices,
   gfx_display_d3d8_get_default_tex_coords,
   FONT_DRIVER_RENDER_D3D8_API,
   GFX_VIDEO_DRIVER_DIRECT3D8,
   "d3d8",
   false,
   gfx_display_d3d8_scissor_begin,
   gfx_display_d3d8_scissor_end
};

/*
 * VIDEO DRIVER
 */

static void d3d8_viewport_info(void *data, struct video_viewport *vp)
{
   d3d8_video_t *d3d   = (d3d8_video_t*)data;

   if (!d3d || !vp)
      return;

   vp->x            = d3d->out_vp.X;
   vp->y            = d3d->out_vp.Y;
   vp->width        = d3d->out_vp.Width;
   vp->height       = d3d->out_vp.Height;

   vp->full_width   = d3d->vp.full_width;
   vp->full_height  = d3d->vp.full_height;
}

static void d3d8_overlay_render(d3d8_video_t *d3d,
      unsigned width, unsigned height,
      overlay_t *overlay, bool force_linear)
{
   D3DVIEWPORT8 vp_full;
   struct video_viewport vp;
   unsigned i;
   Vertex vert[4];
   D3DTEXTUREFILTERTYPE filter_type        = D3DTEXF_LINEAR;

   if (!d3d || !overlay || !overlay->tex)
      return;

   if (!overlay->vert_buf)
   {
      overlay->vert_buf = d3d8_vertex_buffer_new(
      d3d->dev, sizeof(vert), D3DUSAGE_WRITEONLY,
      D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE,
      D3DPOOL_MANAGED);

	  if (!overlay->vert_buf)
		  return;
   }

   for (i = 0; i < 4; i++)
   {
      vert[i].z    = 0.5f;
      vert[i].color   = (((uint32_t)(overlay->alpha_mod * 0xFF)) << 24) | 0xFFFFFF;
   }

   d3d8_viewport_info(d3d, &vp);

   vert[0].x      = overlay->vert_coords[0];
   vert[1].x      = overlay->vert_coords[0] + overlay->vert_coords[2];
   vert[2].x      = overlay->vert_coords[0];
   vert[3].x      = overlay->vert_coords[0] + overlay->vert_coords[2];
   vert[0].y      = overlay->vert_coords[1];
   vert[1].y      = overlay->vert_coords[1];
   vert[2].y      = overlay->vert_coords[1] + overlay->vert_coords[3];
   vert[3].y      = overlay->vert_coords[1] + overlay->vert_coords[3];

   vert[0].u      = overlay->tex_coords[0];
   vert[1].u      = overlay->tex_coords[0] + overlay->tex_coords[2];
   vert[2].u      = overlay->tex_coords[0];
   vert[3].u      = overlay->tex_coords[0] + overlay->tex_coords[2];
   vert[0].v      = overlay->tex_coords[1];
   vert[1].v      = overlay->tex_coords[1];
   vert[2].v      = overlay->tex_coords[1] + overlay->tex_coords[3];
   vert[3].v      = overlay->tex_coords[1] + overlay->tex_coords[3];

   if (overlay->vert_buf)
   {
      LPDIRECT3DVERTEXBUFFER8 vbo = (LPDIRECT3DVERTEXBUFFER8)overlay->vert_buf;
      void *verts = d3d8_vertex_buffer_lock(vbo);
      memcpy(verts, vert, sizeof(vert));
      IDirect3DVertexBuffer8_Unlock(vbo);
   }
   IDirect3DDevice8_SetRenderState(d3d->dev, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
   IDirect3DDevice8_SetRenderState(d3d->dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
   IDirect3DDevice8_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE, true);
   IDirect3DDevice8_SetVertexShader(d3d->dev,
         D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE);
   IDirect3DDevice8_SetStreamSource(d3d->dev,
         0, overlay->vert_buf, sizeof(*vert));

   if (overlay->fullscreen)
   {

      vp_full.X      = 0;
      vp_full.Y      = 0;
      vp_full.Width  = width;
      vp_full.Height = height;
      vp_full.MinZ   = 0.0f;
      vp_full.MaxZ   = 1.0f;
      IDirect3DDevice8_SetViewport(d3d->dev, &vp_full);
   }

   if (!force_linear)
   {
      settings_t *settings    = config_get_ptr();
      bool menu_linear_filter = settings->bools.menu_linear_filter;

      if (!menu_linear_filter)
         filter_type       = D3DTEXF_POINT;
   }

   /* Render overlay. */
   IDirect3DDevice8_SetTexture(d3d->dev, 0,
         (IDirect3DBaseTexture8*)overlay->tex);
   IDirect3DDevice8_SetTextureStageState(d3d->dev, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_ADDRESSU, D3DTADDRESS_BORDER);
   IDirect3DDevice8_SetTextureStageState(d3d->dev, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_ADDRESSV, D3DTADDRESS_BORDER);
   IDirect3DDevice8_SetTextureStageState(d3d->dev, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_MAGFILTER, filter_type);
   IDirect3DDevice8_SetTextureStageState(d3d->dev, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_MINFILTER, filter_type);
   IDirect3DDevice8_BeginScene(d3d->dev);
   IDirect3DDevice8_DrawPrimitive(d3d->dev, D3DPT_TRIANGLESTRIP, 0, 2);
   IDirect3DDevice8_EndScene(d3d->dev);

   /* Restore previous state. */
   IDirect3DDevice8_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE, false);
   IDirect3DDevice8_SetViewport(d3d->dev, (D3DVIEWPORT8*)&d3d->out_vp);
}

static void d3d8_free_overlay(d3d8_video_t *d3d, overlay_t *overlay)
{
   LPDIRECT3DTEXTURE8 tex      = overlay->tex;
   LPDIRECT3DVERTEXBUFFER8 vbo = (LPDIRECT3DVERTEXBUFFER8)overlay->vert_buf;
   if (tex)
      IDirect3DTexture8_Release(tex);
   if (vbo)
      IDirect3DVertexBuffer8_Release(vbo);
   overlay->vert_buf = NULL;
}

static void d3d8_deinitialize(d3d8_video_t *d3d)
{
   LPDIRECT3DVERTEXBUFFER8 _vbo;
   d3d8_renderchain_t *chain = NULL;
   if (!d3d)
      return;
   chain                     = (d3d8_renderchain_t*)d3d->renderchain_data;
   font_driver_free_osd();

   if (chain)
   {
      LPDIRECT3DTEXTURE8      tex = (LPDIRECT3DTEXTURE8)chain->tex;
      LPDIRECT3DVERTEXBUFFER8 vbo = (LPDIRECT3DVERTEXBUFFER8)chain->vertex_buf;
      if (tex)
         IDirect3DTexture8_Release(tex);
      if (vbo)
         IDirect3DVertexBuffer8_Release(vbo);
      chain->vertex_buf = NULL;
      chain->tex        = NULL;

      free(chain);
   }
   d3d->renderchain_data    = NULL;
   _vbo                     = (LPDIRECT3DVERTEXBUFFER8)d3d->menu_display.buffer;
   IDirect3DVertexBuffer8_Release(_vbo);
   d3d->menu_display.buffer = NULL;
   d3d->menu_display.decl   = NULL;
}

#define FS_PRESENTINTERVAL(pp) ((pp)->FullScreen_PresentationInterval)

static INLINE bool d3d8_get_adapter_display_mode(
      LPDIRECT3D8 d3d,
      unsigned idx,
      void *display_mode)
{
   if (d3d &&
         SUCCEEDED(IDirect3D8_GetAdapterDisplayMode(
               d3d, idx, (D3DDISPLAYMODE*)display_mode)))
      return true;
   return false;
}

static D3DFORMAT d3d8_get_color_format_backbuffer(
      LPDIRECT3D8 d3d8, bool rgb32, bool windowed)
{
   D3DFORMAT fmt = D3DFMT_X8R8G8B8;
#ifdef _XBOX
   if (!rgb32)
      fmt        = D3D8_RGB565_FORMAT;
#else
   if (windowed)
   {
      D3DDISPLAYMODE display_mode;
      if (d3d8_get_adapter_display_mode(d3d8, 0, &display_mode))
         fmt = display_mode.Format;
   }
#endif
   return fmt;
}

static bool d3d8_is_windowed_enable(bool info_fullscreen)
{
#ifndef _XBOX
   settings_t *settings = config_get_ptr();
   if (!info_fullscreen)
      return true;
   if (settings)
      return settings->bools.video_windowed_fullscreen;
#endif
   return false;
}

#ifdef _XBOX
static void d3d8_get_video_size(d3d8_video_t *d3d,
      unsigned *width, unsigned *height)
{
   DWORD video_mode      = XGetVideoFlags();

   *width                = 640;
   *height               = 480;

   d3d->widescreen_mode  = false;

   /* Only valid in PAL mode, not valid for HDTV modes! */

   if (XGetVideoStandard() == XC_VIDEO_STANDARD_PAL_I)
   {
      /* Check for 16:9 mode (PAL REGION) */
      if (video_mode & XC_VIDEO_FLAGS_WIDESCREEN)
      {
         *width = 720;
         /* 60 Hz, 720x480i */
         if (video_mode & XC_VIDEO_FLAGS_PAL_60Hz)
            *height = 480;
         else /* 50 Hz, 720x576i */
            *height = 576;
         d3d->widescreen_mode = true;
      }
   }
   else
   {
      /* Check for 16:9 mode (NTSC REGIONS) */
      if (video_mode & XC_VIDEO_FLAGS_WIDESCREEN)
      {
         *width                    = 720;
         *height                   = 480;
         d3d->widescreen_mode      = true;
      }
   }

   if (XGetAVPack() == XC_AV_PACK_HDTV)
   {
      if (video_mode & XC_VIDEO_FLAGS_HDTV_480p)
      {
         *width                    = 640;
         *height                   = 480;
         d3d->widescreen_mode      = false;
         d3d->resolution_hd_enable = true;
      }
      else if (video_mode & XC_VIDEO_FLAGS_HDTV_720p)
      {
         *width                    = 1280;
         *height                   = 720;
         d3d->widescreen_mode      = true;
         d3d->resolution_hd_enable = true;
      }
      else if (video_mode & XC_VIDEO_FLAGS_HDTV_1080i)
      {
         *width                    = 1920;
         *height                   = 1080;
         d3d->widescreen_mode      = true;
         d3d->resolution_hd_enable = true;
      }
   }
}
#endif

static void d3d8_make_d3dpp(void *data,
      const video_info_t *info, void *_d3dpp)
{
   d3d8_video_t *d3d               = (d3d8_video_t*)data;
   D3DPRESENT_PARAMETERS *d3dpp   = (D3DPRESENT_PARAMETERS*)_d3dpp;
   bool windowed_enable           = d3d8_is_windowed_enable(info->fullscreen);

   memset(d3dpp, 0, sizeof(*d3dpp));

   d3dpp->Windowed                = windowed_enable;
   FS_PRESENTINTERVAL(d3dpp)      = D3DPRESENT_INTERVAL_IMMEDIATE;

   if (info->vsync)
   {
      settings_t *settings         = config_get_ptr();
      unsigned video_swap_interval = runloop_get_video_swap_interval(
            settings->uints.video_swap_interval);

      switch (video_swap_interval)
      {
         default:
         case 1:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_ONE;
            break;
         case 2:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_TWO;
            break;
         case 3:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_THREE;
            break;
         case 4:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_FOUR;
            break;
      }
   }

   /* PresentationInterval must be zero for windowed mode on DX8. */
   if (d3dpp->Windowed)
      FS_PRESENTINTERVAL(d3dpp)   = D3DPRESENT_INTERVAL_DEFAULT;

   d3dpp->SwapEffect              = D3DSWAPEFFECT_DISCARD;
   d3dpp->BackBufferCount         = 2;
   d3dpp->BackBufferFormat        = d3d8_get_color_format_backbuffer(
         d3d->d3d8, info->rgb32, windowed_enable);
#ifndef _XBOX
   d3dpp->hDeviceWindow           = win32_get_window();
#endif

   if (!windowed_enable)
   {
#ifdef _XBOX
      /* Xbox: query the actual display size, publish it to video_st
       * and track it in d3d->vp.full_width/full_height so subsequent
       * read sites (font_render_msg, viewport_info, etc.) can pull
       * from the local field instead of locking video_st. */
      unsigned width              = 0;
      unsigned height             = 0;

      d3d8_get_video_size(d3d, &width, &height);
      video_driver_set_output_size(width, height);
      d3d->vp.full_width          = width;
      d3d->vp.full_height         = height;
      d3dpp->BackBufferWidth      = width;
      d3dpp->BackBufferHeight     = height;
#else
      /* Non-Xbox: by the time make_d3dpp runs, d3d8_init_internal
       * has already published the size and written d3d->vp.
       * full_width/full_height; read from there. */
      d3dpp->BackBufferWidth      = d3d->vp.full_width;
      d3dpp->BackBufferHeight     = d3d->vp.full_height;
#endif
   }

#ifdef _XBOX
   d3dpp->MultiSampleType         = D3DMULTISAMPLE_NONE;
   d3dpp->EnableAutoDepthStencil  = FALSE;
   {
      /* Get the "video mode" */
      DWORD video_mode            = XGetVideoFlags();

      /* Check if we are able to use progressive mode. */
      if (video_mode & XC_VIDEO_FLAGS_HDTV_480p)
         d3dpp->Flags = D3DPRESENTFLAG_PROGRESSIVE;
      else
         d3dpp->Flags = D3DPRESENTFLAG_INTERLACED;

      /* Only valid in PAL mode, not valid for HDTV modes. */
      if (XGetVideoStandard() == XC_VIDEO_STANDARD_PAL_I)
      {
         if (video_mode & XC_VIDEO_FLAGS_PAL_60Hz)
            d3dpp->FullScreen_RefreshRateInHz = 60;
         else
            d3dpp->FullScreen_RefreshRateInHz = 50;
      }

      if (XGetAVPack() == XC_AV_PACK_HDTV)
      {
         if (video_mode & XC_VIDEO_FLAGS_HDTV_480p)
            d3dpp->Flags = D3DPRESENTFLAG_PROGRESSIVE;
         else if (video_mode & XC_VIDEO_FLAGS_HDTV_720p)
            d3dpp->Flags = D3DPRESENTFLAG_PROGRESSIVE;
         else if (video_mode & XC_VIDEO_FLAGS_HDTV_1080i)
            d3dpp->Flags = D3DPRESENTFLAG_INTERLACED;
      }

#if 0
      if (d3d->widescreen_mode)
         d3dpp->Flags |= D3DPRESENTFLAG_WIDESCREEN;
#endif
   }
#endif
}

static bool d3d8_init_base(void *data, const video_info_t *info)
{
   D3DPRESENT_PARAMETERS d3dpp;
#ifdef _XBOX
   HWND focus_window = NULL;
#else
   HWND focus_window = win32_get_window();
#endif
   d3d8_video_t *d3d = (d3d8_video_t*)data;

#ifdef _XBOX
   d3d->d3d8           = (LPDIRECT3D8)D3DCreate(0);
#else
   d3d->d3d8           = (LPDIRECT3D8)D3DCreate(220);
#endif

   /* this needs g_pD3D created first */
   d3d8_make_d3dpp(d3d, info, &d3dpp);

   if (!d3d->d3d8)
      return false;
   if (!d3d8_create_device(&d3d->dev, &d3dpp,
            d3d->d3d8,
            focus_window,
            d3d->cur_mon_id)
      )
      return false;
   return true;
}

static void d3d8_calculate_rect(void *data,
      unsigned *width, unsigned *height,
      int *x, int *y,
      bool force_full,
      bool allow_rotate)
{
   struct video_viewport vp;
   d3d8_video_t *d3d         = (d3d8_video_t*)data;

   *width  = d3d->vp.full_width;
   *height = d3d->vp.full_height;

   vp.full_width  = *width;
   vp.full_height = *height;
   video_driver_update_viewport(&vp, force_full, d3d->keep_aspect, true);

   *x      = vp.x;
   *y      = vp.y;
   *width  = vp.width;
   *height = vp.height;
}

static void d3d8_set_viewport(void *data,
      unsigned width, unsigned height,
      bool force_full,
      bool allow_rotate)
{
   int x               = 0;
   int y               = 0;
   d3d8_video_t *d3d = (d3d8_video_t*)data;

   /* Pre-computed transpose(ortho(0,1,0,1,0,1)) — constant */
   static const math_matrix_4x4 k_ortho_mvp = {{
      2, 0, 0,-1,   0, 2, 0,-1,   0, 0, 1, 0,   0, 0, 0, 1
   }};

   d3d8_calculate_rect(data, &width, &height, &x, &y,
         force_full, allow_rotate);

   /* D3D doesn't support negative X/Y viewports ... */
   if (x < 0)
      x = 0;
   if (y < 0)
      y = 0;

   d3d->out_vp.X      = x;
   d3d->out_vp.Y      = y;
   d3d->out_vp.Width  = width;
   d3d->out_vp.Height = height;
   d3d->out_vp.MinZ   = 0.0f;
   d3d->out_vp.MaxZ   = 0.0f;

   d3d->mvp = k_ortho_mvp;

   /* Compute rotated MVP: transpose(ortho(0,1,0,1,0,1) * rot_z(angle))
    * Folded into a single analytical formula. */
   {
      float angle = d3d->dev_rotation * (M_PI / 2.0);
      float c     = cosf(angle);
      float s     = sinf(angle);
      memset(&d3d->mvp_rotate, 0, sizeof(d3d->mvp_rotate));
      MAT_ELEM_4X4(d3d->mvp_rotate, 0, 0) =  2.0f * c;
      MAT_ELEM_4X4(d3d->mvp_rotate, 1, 0) = -2.0f * s;
      MAT_ELEM_4X4(d3d->mvp_rotate, 3, 0) = -c + s;
      MAT_ELEM_4X4(d3d->mvp_rotate, 0, 1) =  2.0f * s;
      MAT_ELEM_4X4(d3d->mvp_rotate, 1, 1) =  2.0f * c;
      MAT_ELEM_4X4(d3d->mvp_rotate, 3, 1) = -s - c;
      MAT_ELEM_4X4(d3d->mvp_rotate, 2, 2) =  1.0f;
      MAT_ELEM_4X4(d3d->mvp_rotate, 3, 3) =  1.0f;
   }
}

static bool d3d8_initialize(d3d8_video_t *d3d, const video_info_t *info)
{
   struct LinkInfo link_info;
   unsigned i           = 0;
   bool ret             = true;
   settings_t *settings = config_get_ptr();

   if (!d3d)
      return false;

   if (!d3d->d3d8)
      ret = d3d8_init_base(d3d, info);
   else if (d3d->needs_restore)
   {
      D3DPRESENT_PARAMETERS d3dpp;
      d3d8_make_d3dpp(d3d, info, &d3dpp);
      if (!d3d8_reset(d3d->dev, &d3dpp))
      {
         d3d8_deinitialize(d3d);
         IDirect3D8_Release(d3d->d3d8);
         d3d->d3d8 = NULL;

         if ((ret = d3d8_init_base(d3d, info)))
            RARCH_LOG("[D3D8] Recovered from dead state.\n");
      }

#ifdef HAVE_MENU
      menu_driver_init(info->is_threaded);
#endif
   }

   if (!ret)
      return ret;

   /* Setup information. */
   link_info.pass               = NULL;
   link_info.tex_w              = info->input_scale * RARCH_SCALE_BASE;
   link_info.tex_h              = info->input_scale * RARCH_SCALE_BASE;
   link_info.pass               = &d3d->shader.pass[0];

   d3d->renderchain_data        = d3d8_renderchain_new();

   if (
         !d3d8_setup_init(
            d3d,
            &d3d->video_info,
            d3d->dev, &link_info,
            d3d->video_info.rgb32)
      )
      return false;

   /* d3d->vp.full_* was written by the caller (d3d8_init_internal
    * has already called set_size at this point). */
   d3d8_set_viewport(d3d,
	   d3d->vp.full_width, d3d->vp.full_height, false, true);

   font_driver_init_osd(d3d, info,
         false,
         info->is_threaded,
         FONT_DRIVER_RENDER_D3D8_API);

   d3d->menu_display.offset = 0;
   d3d->menu_display.size   = 1024;
   d3d->menu_display.buffer = d3d8_vertex_buffer_new(
         d3d->dev, d3d->menu_display.size * sizeof(Vertex),
         D3DUSAGE_WRITEONLY,
         D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE,
         D3DPOOL_DEFAULT);

   if (!d3d->menu_display.buffer)
      return false;

   /* Pre-computed D3D left-handed orthographic projection (0,1,0,1,0,1) */
   {
      static const math_matrix_4x4 k_ortho_transposed = {{
         2, 0, 0, 0,   0, 2, 0, 0,   0, 0, 1, 0,   -1, -1, 0, 1
      }};
      static const math_matrix_4x4 k_ortho = {{
         2, 0, 0,-1,   0, 2, 0,-1,   0, 0, 1, 0,   0, 0, 0, 1
      }};
      d3d->mvp_transposed = k_ortho_transposed;
      d3d->mvp            = k_ortho;
   }

   IDirect3DDevice8_SetRenderState(d3d->dev, D3DRS_CULLMODE, D3DCULL_NONE);

   return true;
}

static bool d3d8_restore(void *data)
{
   d3d8_video_t            *d3d = (d3d8_video_t*)data;

   if (!d3d)
      return false;

   d3d8_deinitialize(d3d);

   if (!d3d8_initialize(d3d, &d3d->video_info))
   {
      RARCH_ERR("[D3D8] Restore error.\n");
      return false;
   }

   d3d->needs_restore = false;

   return true;
}

static void d3d8_set_nonblock_state(void *data, bool state,
      bool adaptive_vsync_enabled,
      unsigned swap_interval)
{
#ifdef _XBOX
   int interval      = 0;
#endif
   d3d8_video_t *d3d = (d3d8_video_t*)data;

   if (!d3d)
      return;

   d3d->video_info.vsync        = !state;

#ifdef _XBOX
   if (!state)
      interval                  = 1;

   IDirect3DDevice8_SetRenderState(d3d->dev,
         D3D8_PRESENTATIONINTERVAL,
         interval ?
         D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE
         );
#else
   d3d->needs_restore           = true;
   d3d8_restore(d3d);
#endif
}

static void d3d8_set_resize(d3d8_video_t *d3d,
      unsigned new_width, unsigned new_height)
{
   /* No changes? */
   if (     (new_width  == d3d->video_info.width)
         && (new_height == d3d->video_info.height))
      return;

   d3d->video_info.width  = new_width;
   d3d->video_info.height = new_height;
   video_driver_set_output_size(new_width, new_height);
   d3d->vp.full_width     = new_width;
   d3d->vp.full_height    = new_height;
}

static bool d3d8_alive(void *data)
{
   unsigned temp_width  = 0;
   unsigned temp_height = 0;
   bool ret             = false;
   d3d8_video_t *d3d    = (d3d8_video_t*)data;
   bool        quit     = false;
   bool        resize   = false;

   /* Read from local bookkeeping rather than video_st (which
    * would acquire context_lock + display_lock).  d3d->vp.full_*
    * is written at every set_size call site in this driver, so
    * it stays in sync with video_st->width/height as long as no
    * other code path writes them.  In practice nothing does --
    * see video_driver.c audit. */
   temp_width  = d3d->vp.full_width;
   temp_height = d3d->vp.full_height;

   win32_check_window(NULL, &quit, &resize, &temp_width, &temp_height);

   if (quit)
      d3d->quitting = quit;

   if (resize)
   {
      d3d->should_resize = true;
      d3d8_set_resize(d3d, temp_width, temp_height);
      d3d8_restore(d3d);
   }

   ret = !quit;

   if (temp_width != 0 && temp_height != 0)
   {
      video_driver_set_output_size(temp_width, temp_height);
      d3d->vp.full_width  = temp_width;
      d3d->vp.full_height = temp_height;
   }

   return ret;
}

#ifdef _XBOX
static bool d3d8_suspend_screensaver(void *data, bool enable) { return true; }
#endif

static void d3d8_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   d3d8_video_t *d3d = (d3d8_video_t*)data;

   if (!d3d)
      return;

   d3d->keep_aspect   = true;
   d3d->should_resize = true;
}

static void d3d8_apply_state_changes(void *data)
{
   d3d8_video_t *d3d = (d3d8_video_t*)data;
   if (d3d)
      d3d->should_resize = true;
}

static void d3d8_set_osd_msg(void *data, const char *msg,
      const struct font_params *params, void *font)
{
   d3d8_video_t          *d3d = (d3d8_video_t*)data;

   IDirect3DDevice8_BeginScene(d3d->dev);
   font_driver_render_msg(d3d, msg, params, font);
   IDirect3DDevice8_EndScene(d3d->dev);
}

static bool d3d8_init_internal(d3d8_video_t *d3d,
      const video_info_t *info, input_driver_t **input,
      void **input_data)
{
#ifdef HAVE_MONITOR
   bool windowed_full;
   RECT mon_rect;
   MONITORINFOEX current_mon;
   HMONITOR hm_to_use;
#endif
   struct video_shader_pass *pass = NULL;
   unsigned full_x           = 0;
   unsigned full_y           = 0;
   settings_t    *settings   = config_get_ptr();
   overlay_t *menu           = (overlay_t*)calloc(1, sizeof(*menu));

   if (!menu)
      return false;

   d3d->menu                 = menu;
   d3d->cur_mon_id           = 0;
   d3d->menu->tex_coords[0]  = 0;
   d3d->menu->tex_coords[1]  = 0;
   d3d->menu->tex_coords[2]  = 1;
   d3d->menu->tex_coords[3]  = 1;
   d3d->menu->vert_coords[0] = 0;
   d3d->menu->vert_coords[1] = 1;
   d3d->menu->vert_coords[2] = 1;
   d3d->menu->vert_coords[3] = -1;

#ifdef HAVE_WINDOW
   memset(&d3d->windowClass, 0, sizeof(d3d->windowClass));
   d3d->windowClass.lpfnWndProc = wnd_proc_d3d_common;
#ifdef HAVE_DINPUT
   if (string_is_equal(settings->arrays.input_driver, "dinput"))
      d3d->windowClass.lpfnWndProc = wnd_proc_d3d_dinput;
#endif
#ifdef HAVE_WINRAWINPUT
   if (string_is_equal(settings->arrays.input_driver, "raw"))
      d3d->windowClass.lpfnWndProc = wnd_proc_d3d_winraw;
#endif
   win32_window_init(&d3d->windowClass, true, NULL);
#endif

#ifdef HAVE_MONITOR
   win32_monitor_info(&current_mon, &hm_to_use, &d3d->cur_mon_id);

   mon_rect              = current_mon.rcMonitor;
   g_win32_resize_width  = info->width;
   g_win32_resize_height = info->height;

   windowed_full         = settings->bools.video_windowed_fullscreen;

   full_x                = (windowed_full || info->width  == 0) ?
      (mon_rect.right  - mon_rect.left) : info->width;
   full_y                = (windowed_full || info->height == 0) ?
      (mon_rect.bottom - mon_rect.top)  : info->height;
#else
   d3d8_get_video_size(d3d, &full_x, &full_y);
#endif
   {
      unsigned new_width  = info->fullscreen ? full_x : info->width;
      unsigned new_height = info->fullscreen ? full_y : info->height;
      video_driver_set_output_size(new_width, new_height);
      d3d->vp.full_width  = new_width;
      d3d->vp.full_height = new_height;

#ifdef HAVE_WINDOW
      /* Use new_width / new_height directly rather than reading
       * them back via video_driver_get_output_size: nothing in the
       * codebase writes video_st->width / height between the
       * set_size above and this call except us. */
      if (!win32_set_video_mode(d3d, new_width, new_height,
            info->fullscreen))
      {
         RARCH_ERR("[D3D8] win32_set_video_mode failed.\n");
         return false;
      }
#endif
   }

   memset(&d3d->shader, 0, sizeof(d3d->shader));
   d3d->shader.passes                    = 1;

   pass                                  = (struct video_shader_pass*)
      &d3d->shader.pass[0];

   pass->fbo.scale_y                     = 1.0;
   pass->fbo.type_y                      = RARCH_SCALE_VIEWPORT;
   pass->fbo.scale_x                     = pass->fbo.scale_y;
   pass->fbo.type_x                      = pass->fbo.type_y;
   pass->fbo.flags                      |= FBO_SCALE_FLAG_VALID;

   if (d3d->shader_path && *d3d->shader_path)
      strlcpy(pass->source.path, d3d->shader_path,
            sizeof(pass->source.path));

   d3d->video_info                       = *info;
   if (!d3d8_initialize(d3d, &d3d->video_info))
      return false;

   d3d_input_driver(settings->arrays.input_driver, settings->arrays.input_joypad_driver, input, input_data);

   return true;
}

static void d3d8_set_rotation(void *data, unsigned rot)
{
   d3d8_video_t *d3d  = (d3d8_video_t*)data;

   if (!d3d)
      return;

   d3d->dev_rotation  = rot;
   d3d->should_resize = true;
}

static void *d3d8_init(const video_info_t *info,
      input_driver_t **input, void **input_data)
{
   d3d8_video_t *d3d = (d3d8_video_t*)calloc(1, sizeof(*d3d));

   if (!d3d)
      return NULL;

   if (!d3d8_initialize_symbols(GFX_CTX_DIRECT3D8_API))
   {
      free(d3d);
      return NULL;
   }

#ifndef _XBOX
   win32_window_reset();
   win32_monitor_init();
#endif

   /* Default values */
   d3d->dev                  = NULL;
   d3d->dev_rotation         = 0;
   d3d->needs_restore        = false;
#ifdef HAVE_OVERLAY
   d3d->overlays_enabled     = false;
#endif
   d3d->should_resize        = false;
   d3d->menu                 = NULL;

   if (!d3d8_init_internal(d3d, info, input, input_data))
   {
      RARCH_ERR("[D3D8] Failed to init D3D.\n");
      free(d3d);
      return NULL;
   }

   d3d->keep_aspect       = info->force_aspect;

   return d3d;
}

#ifdef HAVE_OVERLAY
static void d3d8_free_overlays(d3d8_video_t *d3d)
{
   unsigned i;

   if (!d3d)
      return;

   for (i = 0; i < d3d->overlays_size; i++)
      d3d8_free_overlay(d3d, &d3d->overlays[i]);
   free(d3d->overlays);
   d3d->overlays      = NULL;
   d3d->overlays_size = 0;
}
#endif

static void d3d8_free(void *data)
{
   d3d8_video_t   *d3d = (d3d8_video_t*)data;

   if (!d3d)
      return;

#ifdef HAVE_OVERLAY
   d3d8_free_overlays(d3d);
   if (d3d->overlays)
      free(d3d->overlays);
   d3d->overlays      = NULL;
   d3d->overlays_size = 0;
#endif

   d3d8_free_overlay(d3d, d3d->menu);
   if (d3d->menu)
      free(d3d->menu);
   d3d->menu          = NULL;

   d3d8_deinitialize(d3d);

   if (d3d->shader_path && *d3d->shader_path)
      free(d3d->shader_path);

   IDirect3DDevice8_Release(d3d->dev);
   IDirect3D8_Release(d3d->d3d8);
   d3d->shader_path = NULL;
   d3d->dev         = NULL;
   d3d->d3d8          = NULL;

#ifdef HAVE_DYNAMIC_D3D
   d3d8_deinitialize_symbols();
#endif

#ifndef _XBOX
   win32_monitor_from_window();
   win32_destroy_window();
#endif
   free(d3d);
}

#ifdef HAVE_OVERLAY
static void d3d8_overlay_tex_geom(
      void *data,
      unsigned index,
      float x, float y,
      float w, float h)
{
   d3d8_video_t *d3d                   = (d3d8_video_t*)data;
   if (!d3d)
      return;

   d3d->overlays[index].tex_coords[0]  = x;
   d3d->overlays[index].tex_coords[1]  = y;
   d3d->overlays[index].tex_coords[2]  = w;
   d3d->overlays[index].tex_coords[3]  = h;
#ifdef _XBOX
   d3d->overlays[index].tex_coords[0] *= d3d->overlays[index].tex_w;
   d3d->overlays[index].tex_coords[1] *= d3d->overlays[index].tex_h;
   d3d->overlays[index].tex_coords[2] *= d3d->overlays[index].tex_w;
   d3d->overlays[index].tex_coords[3] *= d3d->overlays[index].tex_h;
#endif
}

static void d3d8_overlay_vertex_geom(
      void *data,
      unsigned index,
      float x, float y,
      float w, float h)
{
   d3d8_video_t *d3d = (d3d8_video_t*)data;
   if (!d3d)
      return;

   y                                   = 1.0f - y;
   h                                   = -h;
   d3d->overlays[index].vert_coords[0] = x;
   d3d->overlays[index].vert_coords[1] = y;
   d3d->overlays[index].vert_coords[2] = w;
   d3d->overlays[index].vert_coords[3] = h;
}

static bool d3d8_overlay_load(void *data,
      const void *image_data, unsigned num_images)
{
   unsigned i, y;
   overlay_t *new_overlays            = NULL;
   d3d8_video_t *d3d                  = (d3d8_video_t*)data;
   const struct texture_image *images = (const struct texture_image*)image_data;

   if (!d3d)
      return false;

   d3d8_free_overlays(d3d);
   d3d->overlays      = (overlay_t*)calloc(num_images, sizeof(*d3d->overlays));
   d3d->overlays_size = num_images;

   for (i = 0; i < num_images; i++)
   {
      D3DLOCKED_RECT d3dlr;
      unsigned width     = images[i].width;
      unsigned height    = images[i].height;
      overlay_t *overlay = (overlay_t*)&d3d->overlays[i];

      overlay->tex       = d3d8_texture_new(d3d->dev,
                  width, height, 1, 0,
                  D3D8_ARGB8888_FORMAT,
                  D3DPOOL_MANAGED, 0, 0, 0,
                  NULL, NULL, false);

      if (!overlay->tex)
         return false;

      if (IDirect3DTexture8_LockRect(
               (LPDIRECT3DTEXTURE8)overlay->tex, 0,
               &d3dlr, NULL, D3DLOCK_NOSYSLOCK) == D3D_OK)
      {
         uint32_t       *dst    = (uint32_t*)(d3dlr.pBits);
         const uint32_t *src    = images[i].pixels;
         unsigned      pitch    = d3dlr.Pitch >> 2;
         LPDIRECT3DTEXTURE8 tex = overlay->tex;
         for (y = 0; y < height; y++, dst += pitch, src += width)
            memcpy(dst, src, width << 2);
         IDirect3DTexture8_UnlockRect(tex, 0);
      }

      overlay->tex_w         = width;
      overlay->tex_h         = height;

      /* Default. Stretch to whole screen. */
      d3d8_overlay_tex_geom(d3d, i, 0, 0, 1, 1);
      d3d8_overlay_vertex_geom(d3d, i, 0, 0, 1, 1);
   }

   return true;
}

static void d3d8_overlay_enable(void *data, bool state)
{
   unsigned i;
   d3d8_video_t            *d3d = (d3d8_video_t*)data;

   if (!d3d)
      return;

   for (i = 0; i < d3d->overlays_size; i++)
      d3d->overlays_enabled = state;

#ifndef _XBOX
   win32_show_cursor(d3d, state);
#endif
}

static void d3d8_overlay_full_screen(void *data, bool enable)
{
   unsigned i;
   d3d8_video_t *d3d = (d3d8_video_t*)data;

   for (i = 0; i < d3d->overlays_size; i++)
      d3d->overlays[i].fullscreen = enable;
}

static void d3d8_overlay_set_alpha(void *data, unsigned index, float mod)
{
   d3d8_video_t *d3d = (d3d8_video_t*)data;
   if (d3d)
      d3d->overlays[index].alpha_mod = mod;
}

static const video_overlay_interface_t d3d8_overlay_interface = {
   d3d8_overlay_enable,
   d3d8_overlay_load,
   d3d8_overlay_tex_geom,
   d3d8_overlay_vertex_geom,
   d3d8_overlay_full_screen,
   d3d8_overlay_set_alpha,
};

static void d3d8_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &d3d8_overlay_interface;
}
#endif

static bool d3d8_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height,
      uint64_t frame_count, unsigned pitch,
      const char *msg, video_frame_info_t *video_info)
{
   D3DVIEWPORT8 screen_vp;
   unsigned i                          = 0;
   d3d8_video_t *d3d                    = (d3d8_video_t*)data;
   unsigned width                      = video_info->width;
   unsigned height                     = video_info->height;
   struct font_params *osd_params      = (struct font_params*)
      &video_info->osd_stat_params;
   const char *stat_text               = video_info->stat_text;
   bool statistics_show                = video_info->statistics_show;
   unsigned black_frame_insertion      = video_info->black_frame_insertion;
#ifdef HAVE_GFX_WIDGETS
   bool widgets_active                 = video_info->widgets_active;
#endif
#ifdef HAVE_MENU
   bool menu_is_alive                  = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif

   if (!frame)
      return true;

   /* We cannot recover in fullscreen. */
   if (d3d->needs_restore)
   {
#ifndef _XBOX
      HWND window = win32_get_window();
      if (IsIconic(window))
         return true;
#endif

      if (!d3d8_restore(d3d))
      {
         RARCH_ERR("[D3D8] Failed to restore.\n");
         return false;
      }
   }

   if (d3d->should_resize)
   {
      d3d8_set_viewport(d3d, width, height, false, true);
      d3d->should_resize = false;
   }

   /* render_chain() only clears out viewport,
    * clear out everything. */
   screen_vp.X      = 0;
   screen_vp.Y      = 0;
   screen_vp.MinZ   = 0;
   screen_vp.MaxZ   = 1;
   screen_vp.Width  = width;
   screen_vp.Height = height;
   IDirect3DDevice8_SetViewport(d3d->dev, (D3DVIEWPORT8*)&screen_vp);
   IDirect3DDevice8_Clear(d3d->dev, 0, 0, D3DCLEAR_TARGET, 0, 1, 0);

   d3d8_render(d3d, d3d->dev, (d3d8_renderchain_t*)d3d->renderchain_data, frame,
            frame_width, frame_height,
            pitch, d3d->dev_rotation);

   if (black_frame_insertion && !d3d->menu->enabled)
   {
      unsigned n;
      for (n = 0; n < video_info->black_frame_insertion; ++n)
      {
         if (IDirect3DDevice8_Present(d3d->dev, NULL, NULL, NULL, NULL)
               == D3DERR_DEVICELOST)
            return true;
         if (d3d->needs_restore)
            return true;
         IDirect3DDevice8_Clear(d3d->dev, 0, 0, D3DCLEAR_TARGET, 0, 1, 0);
      }
   }

#ifdef HAVE_MENU
   if (d3d->menu && d3d->menu->enabled)
   {
      d3d8_set_mvp(d3d->dev, &d3d->mvp);
      d3d8_overlay_render(d3d, width, height, d3d->menu, false);

      d3d->menu_display.offset = 0;
      IDirect3DDevice8_SetStreamSource(d3d->dev,
            0, d3d->menu_display.buffer, sizeof(Vertex));

      IDirect3DDevice8_SetViewport(d3d->dev, (D3DVIEWPORT8*)&screen_vp);
      menu_driver_frame(menu_is_alive, video_info);
   }
   else if (statistics_show)
   {
      if (osd_params)
         font_driver_render_msg(d3d, stat_text,
               (const struct font_params*)osd_params, NULL);
   }
#endif

#ifdef HAVE_OVERLAY
   if (d3d->overlays_enabled)
   {
      d3d8_set_mvp(d3d->dev, &d3d->mvp);
      for (i = 0; i < d3d->overlays_size; i++)
         d3d8_overlay_render(d3d, width, height, &d3d->overlays[i], true);
   }
#endif

#ifdef HAVE_GFX_WIDGETS
   /* Widget overlay (notifications, FPS counter, fast-forward
    * indicator, achievement popups, load-progress bars, etc.).
    *
    * Widgets are drawn in screen-space using the same gfx_display
    * ctx the menu uses, so all the prep here mirrors the
    * pre-menu_driver_frame setup above:
    *   - reset menu_display.offset (gfx_display_d3d8_draw streams
    *     vertices into a ring at this offset)
    *   - bind the menu_display vertex buffer
    *   - full-screen viewport (widgets are positioned in screen
    *     pixels, not in the game viewport)
    *   - alpha blend on so semi-transparent widget panels
    *     composite over the framebuffer (overlay_render disables
    *     alpha blending on the way out, so we re-enable it here)
    *   - FVF reset defensively in case the overlay or renderchain
    *     left a different format bound
    *
    * gfx_widgets_frame ultimately calls gfx_display_d3d8_draw and
    * d3d8_font_render_line, both of which wrap their own
    * BeginScene/EndScene around each DrawPrimitiveUP, so we
    * deliberately don't add an outer scene-wrap here. */
   if (widgets_active)
   {
      d3d->menu_display.offset = 0;
      IDirect3DDevice8_SetStreamSource(d3d->dev,
            0, d3d->menu_display.buffer, sizeof(Vertex));
      IDirect3DDevice8_SetViewport(d3d->dev, (D3DVIEWPORT8*)&screen_vp);
      IDirect3DDevice8_SetRenderState(d3d->dev,
            D3DRS_SRCBLEND,         D3DBLEND_SRCALPHA);
      IDirect3DDevice8_SetRenderState(d3d->dev,
            D3DRS_DESTBLEND,        D3DBLEND_INVSRCALPHA);
      IDirect3DDevice8_SetRenderState(d3d->dev,
            D3DRS_ALPHABLENDENABLE, TRUE);
      IDirect3DDevice8_SetVertexShader(d3d->dev,
            D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE);
      gfx_widgets_frame(video_info);
   }
#endif

   if (msg && *msg)
   {
      IDirect3DDevice8_SetViewport(d3d->dev, (D3DVIEWPORT8*)&screen_vp);
      /* d3d8_font_render_msg wraps its own BeginScene/EndScene
       * around each DrawPrimitiveUP, matching the per-draw scene
       * convention used by gfx_display_d3d8_draw. */
      font_driver_render_msg(d3d, msg, NULL, NULL);
   }

   video_driver_update_title(NULL);
   IDirect3DDevice8_Present(d3d->dev, NULL, NULL, NULL, NULL);

   return true;
}

static bool d3d8_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   return false;
}

static void d3d8_set_menu_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   D3DLOCKED_RECT d3dlr;
   d3d8_video_t *d3d = (d3d8_video_t*)data;

   if (!d3d || !d3d->menu)
      return;

   if (    !d3d->menu->tex                  ||
            d3d->menu->tex_w   != width     ||
            d3d->menu->tex_h   != height    ||
            d3d->menu_tex_rgb32 != rgb32)
   {
      LPDIRECT3DTEXTURE8 tex = d3d->menu->tex;
      if (tex)
         IDirect3DTexture8_Release(tex);

      /* RGUI sends 16bpp ARGB4444 (the d3d8 case in RGUI's pixel
       * format dispatcher selects argb32_to_argb4444), so we can
       * upload it byte-for-byte into a D3DFMT_A4R4G4B4 texture and
       * skip the per-pixel CPU expansion to ARGB8888 the previous
       * implementation did every frame.  The rgb32 path is preserved
       * for callers that hand us 32bpp data; in current practice no
       * such caller exists, but the API contract supports it. */
      d3d->menu->tex = d3d8_texture_new(d3d->dev,
            width, height, 1,
            0, rgb32 ? D3D8_ARGB8888_FORMAT : D3D8_ARGB4444_FORMAT,
            D3DPOOL_MANAGED, 0, 0, 0, NULL, NULL, false);

      if (!d3d->menu->tex)
         return;

      d3d->menu->tex_w          = width;
      d3d->menu->tex_h          = height;
      d3d->menu_tex_rgb32       = rgb32;
#ifdef _XBOX
      d3d->menu->tex_coords [2] = width;
      d3d->menu->tex_coords[3]  = height;
#endif
   }

   d3d->menu->alpha_mod = alpha;

   {
      LPDIRECT3DTEXTURE8 tex = d3d->menu->tex;
      if (IDirect3DTexture8_LockRect(tex,
               0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK) == D3D_OK)
      {
         unsigned h;

         if (rgb32)
         {
            uint8_t        *dst = (uint8_t*)d3dlr.pBits;
            const uint32_t *src = (const uint32_t*)frame;

            for (h = 0; h < height; h++, dst += d3dlr.Pitch, src += width)
            {
               memcpy(dst, src, width * sizeof(uint32_t));
               memset(dst + width * sizeof(uint32_t), 0,
                     d3dlr.Pitch - width * sizeof(uint32_t));
            }
         }
         else
         {
            /* Direct ARGB4444 upload.  The bit layout produced by
             * argb32_to_argb4444 (host-endian uint16_t with A in bits
             * 15..12, R 11..8, G 7..4, B 3..0) matches D3DFMT_A4R4G4B4
             * exactly: D3D reads the locked memory as host-endian
             * 16-bit units with the same bit assignments, so the same
             * source bytes work on LE PC and LE Original Xbox (NV2A
             * via D3DFMT_LIN_*) without a byte swap. */
            uint8_t        *dst = (uint8_t*)d3dlr.pBits;
            const uint8_t  *src = (const uint8_t*)frame;
            unsigned src_pitch  = width * sizeof(uint16_t);
            unsigned row_bytes  = width * sizeof(uint16_t);

            for (h = 0; h < height; h++, dst += d3dlr.Pitch, src += src_pitch)
            {
               memcpy(dst, src, row_bytes);
               if (d3dlr.Pitch > (int)row_bytes)
                  memset(dst + row_bytes, 0, d3dlr.Pitch - row_bytes);
            }
         }

         IDirect3DTexture8_UnlockRect(tex, 0);
      }
   }
}

static void d3d8_set_menu_texture_enable(void *data,
      bool state, bool full_screen)
{
   d3d8_video_t *d3d = (d3d8_video_t*)data;

   if (!d3d || !d3d->menu)
      return;

   d3d->menu->enabled            = state;
   d3d->menu->fullscreen         = full_screen;
}

static void d3d8_video_texture_load_d3d(
      struct d3d8_texture_info *info,
      uintptr_t *id)
{
   D3DLOCKED_RECT d3dlr;
   unsigned usage            = 0;
   d3d8_video_t *d3d         = (d3d8_video_t*)info->userdata;
   struct texture_image *ti  = (struct texture_image*)info->data;
   LPDIRECT3DTEXTURE8 tex    = (LPDIRECT3DTEXTURE8)d3d8_texture_new(d3d->dev,
               ti->width, ti->height, 0,
               usage, D3D8_ARGB8888_FORMAT,
               D3DPOOL_MANAGED, 0, 0, 0,
               NULL, NULL, false);

   if (!tex)
      return;

   if (IDirect3DTexture8_LockRect(tex, 0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK) ==
         D3D_OK)
   {
      unsigned i;
      uint32_t       *dst = (uint32_t*)(d3dlr.pBits);
      const uint32_t *src = ti->pixels;
      unsigned      pitch = d3dlr.Pitch >> 2;

      for (i = 0; i < ti->height; i++, dst += pitch, src += ti->width)
         memcpy(dst, src, ti->width << 2);
      IDirect3DTexture8_UnlockRect(tex, 0);
   }

   *id = (uintptr_t)tex;
}

static uintptr_t d3d8_video_texture_load_wrap_d3d(void *data)
{
   uintptr_t id = 0;
   struct d3d8_texture_info *info = (struct d3d8_texture_info*)data;
   if (!info)
      return 0;
   d3d8_video_texture_load_d3d(info, &id);
   return id;
}

static uintptr_t d3d8_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   struct d3d8_texture_info info;
   uintptr_t id  = 0;

   info.userdata = video_data;
   info.data     = data;
   info.type     = filter_type;

   if (threaded)
      return video_thread_texture_handle(&info,
            d3d8_video_texture_load_wrap_d3d);

   d3d8_video_texture_load_d3d(&info, &id);
   return id;
}

static uintptr_t d3d8_video_texture_unload_wrap_d3d(void *data)
{
   uintptr_t id = (uintptr_t)data;
   if (id)
   {
      LPDIRECT3DTEXTURE8 texid = (LPDIRECT3DTEXTURE8)id;
      IDirect3DTexture8_Release(texid);
   }
   return 0;
}

static void d3d8_unload_texture(void *data, bool threaded,
      uintptr_t id)
{
   LPDIRECT3DTEXTURE8 texid;
   if (!id)
	   return;

   /* Dispatch Release to the video thread when threaded video is
    * active, so it is serialised with any pending draw calls
    * that may still reference this texture.  Matches the
    * threading pattern already used by d3d8_load_texture
    * above. */
   if (threaded)
   {
      video_thread_texture_handle((void*)id,
            d3d8_video_texture_unload_wrap_d3d);
      return;
   }

   texid = (LPDIRECT3DTEXTURE8)id;
   IDirect3DTexture8_Release(texid);
}

static void d3d8_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
#ifndef _XBOX
   win32_show_cursor(data, !fullscreen);
#endif
}

static uint32_t d3d8_get_flags(void *data)
{
   uint32_t             flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);

   return flags;
}

static const video_poke_interface_t d3d_poke_interface = {
   d3d8_get_flags,
   d3d8_load_texture,
   d3d8_unload_texture,
   d3d8_set_video_mode,
#if defined(_XBOX) || defined(__WINRT__)
   NULL, /* get_refresh_rate */
#else
   /* UWP does not expose this information easily */
   NULL, /* refresh_rate - handled by display server */
#endif
   NULL, /* set_filtering */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   d3d8_set_aspect_ratio,
   d3d8_apply_state_changes,
   d3d8_set_menu_texture_frame,
   d3d8_set_menu_texture_enable,
   d3d8_set_osd_msg,
   win32_show_cursor,
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL  /* set_hdr_subpixel_layout */
};

static void d3d8_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   *iface = &d3d_poke_interface;
}

#ifdef _XBOX
static bool d3d8_has_windowed(void *data) { return false; }
#else
static bool d3d8_has_windowed(void *data) { return true; }
#endif

#ifdef HAVE_GFX_WIDGETS
/* Required hook: gfx_widgets initialises only on backends that
 * advertise support via this callback.  When it returns true RA
 * routes things like the fast-forward indicator, FPS counter,
 * achievement popups and load-progress bars to the widget layer
 * (gfx_widgets_status_text + gfx_widgets_frame), bypassing the
 * runloop-msg → font_driver_render_msg fallback that would
 * otherwise fire for text-only notifications.  d3d8_frame calls
 * gfx_widgets_frame each frame after the overlay block, so all
 * the widget rendering (which goes through gfx_display_d3d8_draw
 * + d3d8_font_render_line) is wired up. */
static bool d3d8_gfx_widgets_enabled(void *data)
{
   (void)data;
   return true;
}
#endif

video_driver_t video_d3d8 = {
   d3d8_init,
   d3d8_frame,
   d3d8_set_nonblock_state,
   d3d8_alive,
   NULL, /* focus */
#ifdef _XBOX
   d3d8_suspend_screensaver,
#else
   win32_suspend_screensaver,
#endif
   d3d8_has_windowed,
   d3d8_set_shader,
   d3d8_free,
   "d3d8",
   d3d8_set_viewport,
   d3d8_set_rotation,
   d3d8_viewport_info,
   NULL, /* read_viewport  */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   d3d8_get_overlay_interface,
#endif
   d3d8_get_poke_interface,
   NULL, /* wrap_type_to_enum */
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   d3d8_gfx_widgets_enabled
#endif
};

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

#ifndef _D3D9_COMMON_H
#define _D3D9_COMMON_H

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_inline.h>
#include <gfx/math/matrix_4x4.h>

#ifndef _XBOX
#define WIN32_LEAN_AND_MEAN
#endif
#include <d3d9.h>

#include <formats/image.h>

#include "d3d_common.h"
#include "../../verbosity.h"

/* Views on the PC: Xbox 360 cores have none */
#ifndef _XBOX
#define D3D9_VIEWS
#include <retro_atomic.h>
#include "../video_views.h"
#endif

#ifdef _XBOX
#define D3D9_RGB565_FORMAT D3DFMT_LIN_R5G6B5
#define D3D9_ARGB8888_FORMAT D3DFMT_LIN_A8R8G8B8
#define D3D9_XRGB8888_FORMAT D3DFMT_LIN_X8R8G8B8
#define D3D9_ARGB4444_FORMAT D3DFMT_LIN_A4R4G4B4
#else
#define D3D9_RGB565_FORMAT D3DFMT_R5G6B5
#define D3D9_ARGB8888_FORMAT D3DFMT_A8R8G8B8
#define D3D9_XRGB8888_FORMAT D3DFMT_X8R8G8B8
#define D3D9_ARGB4444_FORMAT D3DFMT_A4R4G4B4
#endif

RETRO_BEGIN_DECLS

#ifdef D3D9_VIEWS
/* A render target views draw into: a texture and its surface */
typedef struct d3d9_views_target
{
   LPDIRECT3DTEXTURE9 tex;
   LPDIRECT3DSURFACE9 surf;
} d3d9_views_target_t;

enum d3d9_views_ps
{
   D3D9_VIEWS_PS_RECT = 0,
   D3D9_VIEWS_PS_BLEND,
   D3D9_VIEWS_PS_LAYER,
   D3D9_VIEWS_PS_COUNT
};

typedef struct d3d9_views
{
   video_views_chains_t  chains;
   video_views_target_t  canvas_target;
   video_views_target_t  ui_target;
   video_views_ui_dims_t saved;
   d3d9_views_target_t   canvas;
   d3d9_views_target_t   ui;
   /* The render target the UI layer was entered from */
   LPDIRECT3DSURFACE9    back;
   /* The pixel shaders, the driver's handles, and each one's constant
    * register */
   void                  *ps[D3D9_VIEWS_PS_COUNT];
   int                   ps_reg[D3D9_VIEWS_PS_COUNT];
   unsigned              frame_dims;
   /* For get_flags on the main thread: whether views may be drawn, and
    * whether the last frame with a map drew it whole */
   retro_atomic_int_t    allowed;
   retro_atomic_int_t    fallback;
   bool                  tried;
   bool                  failed;
   /* The canvas, and the UI layer with its alpha kept apart */
   bool                  blend;
   bool                  layer;
   bool                  active;
   bool                  direct_seen;
   bool                  preset_seen;
} d3d9_views_t;
#endif

typedef struct d3d9_video
{
   overlay_t *menu;
   void *renderchain_data;

   char *shader_path;
   overlay_t *overlays;

   RECT font_rect;
   RECT font_rect_shifted;
   math_matrix_4x4 mvp;
   math_matrix_4x4 mvp_rotate;
   math_matrix_4x4 mvp_transposed;

   struct video_viewport vp;
   struct video_shader shader;
   video_info_t video_info;
#ifdef HAVE_WINDOW
   WNDCLASSEX windowClass;
#endif
   LPDIRECT3DDEVICE9 dev;
   LPDIRECT3D9 d3d9;
   D3DVIEWPORT9 out_vp;
   float translate_x;
   float translate_y;

   struct
   {
      int size;
      int offset;
      void *buffer;
      void *decl;
   }menu_display;

   size_t overlays_size;

   unsigned cur_mon_id;
   unsigned dev_rotation;

   bool keep_aspect;
   bool should_resize;
   bool quitting;
   bool needs_restore;
   bool overlays_enabled;
   /* overlays[].tex are the overlay pack's (load_textures) */
   bool overlays_borrowed;
   /* Only used for Xbox */
   bool widescreen_mode;

   /* Bit-depth of the data most recently uploaded to `menu->tex`.
    * The menu texture is created with a fixed pixel format (16bpp
    * ARGB4444 for the RGUI fast path, 32bpp ARGB8888 otherwise),
    * so we must recreate it when set_menu_texture_frame is called
    * with a different `rgb32` value.  Defaults to false; the first
    * call will see a NULL tex and create one regardless. */
   bool menu_tex_rgb32;
#ifdef D3D9_VIEWS
   d3d9_views_t views;
#endif
} d3d9_video_t;

/* The menu's vertex ring: dynamic, so a lock that starts it over
 * (DISCARD) gets fresh memory and one that appends (NOOVERWRITE) does
 * not wait for the draws still reading what came before */
#define D3D9_MENU_VBUF_SIZE 8192

static INLINE bool d3d9_menu_vbuf_create(d3d9_video_t *d3d)
{
   d3d->menu_display.offset = 0;
   d3d->menu_display.size   = D3D9_MENU_VBUF_SIZE;
   d3d->menu_display.buffer = NULL;
   if (FAILED(IDirect3DDevice9_CreateVertexBuffer(d3d->dev,
               D3D9_MENU_VBUF_SIZE * sizeof(Vertex),
               D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT,
               (LPDIRECT3DVERTEXBUFFER9*)&d3d->menu_display.buffer,
               NULL)))
      d3d->menu_display.buffer = NULL;
   return d3d->menu_display.buffer != NULL;
}

bool d3d9_create_device(void *dev,
      void *d3dpp,
      void *d3d,
      HWND focus_window,
      unsigned cur_mon_id);

bool d3d9_reset(void *dev, void *d3dpp);

void *d3d9_create(void);

bool d3d9_initialize_symbols(enum gfx_ctx_api api);

void d3d9_deinitialize_symbols(void);

void d3d9_make_d3dpp(d3d9_video_t *d3d,
      const video_info_t *info, void *_d3dpp);

bool d3d9_supports_texture_format(void *data,
      enum texture_gpu_format fmt);

uintptr_t d3d9_load_texture_compressed(void *data,
      const struct texture_compressed *tc, bool threaded,
      enum texture_filter_type filter_type);

RETRO_END_DECLS

#endif

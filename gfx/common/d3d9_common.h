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
#include <features/features_cpu.h>

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

/* Hardware-rendered cores on the PC */
#ifndef _XBOX
#define D3D9_HW_RENDER
#include <libretro.h>
#include <libretro_d3d9.h>
#include <retro_atomic.h>
#ifdef HAVE_THREADS
#include <rthreads/retro_eventcount.h>
#endif
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

#include "d3d9_tex.h"
#include "d3d9_sw_fb.h"

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
   /* With Direct3D 9Ex Presentation on, the device's Ex interface
    * (held by the device, so not released here), and where frames
    * wait for the display beside Present: 1 after it, -1 before it
    * (presenting at once), 0 not. Set by d3d9_present_init. */
   LPDIRECT3DDEVICE9 dev_ex;
   /* and its swap chain's Ex interface, held the same way */
   LPDIRECT3DSWAPCHAIN9 chain_ex;
   /* How long the last d3d9_present waited for the display with
    * V-Sync on, and whether its frame was out as it returned */
   retro_time_t present_wait;
   int wait_for_vblank;
   /* The presentation interval the device was made or reset with */
   UINT present_interval;
   bool present_shown;
   d3d9_sw_fb_t sw_fb;
#ifdef D3D9_VIEWS
   d3d9_views_t views;
#endif
#ifdef D3D9_HW_RENDER
   /* A core rendering with this device: libretro_d3d9.h */
   struct
   {
      struct retro_hw_render_interface_d3d9 iface;
      /* The frame the driver draws, with a reference of the driver's */
      LPDIRECT3DTEXTURE9 tex;
      /* The ring's texture for the next frame, not referenced */
      LPDIRECT3DTEXTURE9 ring_tex;
      /* The states the driver draws with, set again over the core's */
      LPDIRECT3DSTATEBLOCK9 state;
#ifdef HAVE_THREADS
      retro_eventcount_t turn_ec;
#endif
      /* D3D9_HW_TURN_* in d3d9hlsl.c */
      retro_atomic_int_t turn;
      /* The thread holding the core's turn, 0 for none */
      retro_atomic_int_t core_tid;
      unsigned core_depth;
      unsigned fe_depth;
      unsigned tex_dims;
      bool enable;
      /* The threaded wrapper is in front */
      bool shared;
      /* The frontend has had the device since the core last released it */
      bool frontend_used;
      /* context_destroy called, context_reset owed */
      bool lost;
      bool reinit_asked;
      bool interval_kept;
      bool warned;
   } hw;
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

/* Room for @count vertices in the ring, locked at vertex *@start for
 * the caller to write in place; NULL when the ring is smaller.
 * d3d9_menu_vbuf_unmap before drawing from it. */
static INLINE Vertex *d3d9_menu_vbuf_map(d3d9_video_t *d3d,
      unsigned count, unsigned *start)
{
   void    *p   = NULL;
   unsigned off = (unsigned)d3d->menu_display.offset;

   if (!d3d->menu_display.buffer
         || count > (unsigned)d3d->menu_display.size)
      return NULL;
   if (off + count > (unsigned)d3d->menu_display.size)
      off = 0;
   if (FAILED(IDirect3DVertexBuffer9_Lock(
               (LPDIRECT3DVERTEXBUFFER9)d3d->menu_display.buffer,
               off * sizeof(Vertex), count * sizeof(Vertex), &p,
               off ? D3DLOCK_NOOVERWRITE : D3DLOCK_DISCARD)) || !p)
      return NULL;
   d3d->menu_display.offset = (int)(off + count);
   *start                   = off;
   return (Vertex*)p;
}

static INLINE void d3d9_menu_vbuf_unmap(d3d9_video_t *d3d)
{
   IDirect3DVertexBuffer9_Unlock(
         (LPDIRECT3DVERTEXBUFFER9)d3d->menu_display.buffer);
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

/* Takes up Direct3D 9Ex Presentation on a device just made or reset:
 * the frame latency, and whether frames wait for the display */
void d3d9_present_init(d3d9_video_t *d3d, bool vsync);

/* When the most recent present reached the display, on the
 * cpu_features_get_time_usec() clock: from the swap chain's statistics
 * under Direct3D 9Ex Presentation, else the compositor's last vblank;
 * 0 where neither can say. Never blocks. */
retro_time_t d3d9_last_present_time(d3d9_video_t *d3d);

/* A frame's Present, as d3d9_present_init set it up. Under Direct3D
 * 9Ex Presentation it is timed for the threaded presenter
 * (get_last_present_wait): with V-Sync on the time is spent waiting
 * for the display, and the frame is out as it returns only where it
 * waited for the vblank after it. */
static INLINE HRESULT d3d9_present(d3d9_video_t *d3d)
{
#ifndef _XBOX
   if (d3d->dev_ex)
   {
      HRESULT hr;
      const d3d9_dev_ex_vtbl_t *vt =
            (const d3d9_dev_ex_vtbl_t*)d3d->dev_ex->lpVtbl;
      retro_time_t at = cpu_features_get_time_usec();
      if (d3d->wait_for_vblank < 0)
      {
         vt->WaitForVBlank(d3d->dev_ex, 0);
         hr = vt->PresentEx(d3d->dev_ex, NULL, NULL, NULL, NULL,
               D3D9_PRESENT_FORCEIMMEDIATE);
      }
      else
      {
         hr = IDirect3DDevice9_Present(d3d->dev, NULL, NULL, NULL, NULL);
         if (d3d->wait_for_vblank > 0)
            vt->WaitForVBlank(d3d->dev_ex, 0);
      }
      d3d->present_wait  = d3d->video_info.vsync
         ? cpu_features_get_time_usec() - at : 0;
      d3d->present_shown = d3d->video_info.vsync
         && d3d->wait_for_vblank > 0;
      return hr;
   }
#endif
   return IDirect3DDevice9_Present(d3d->dev, NULL, NULL, NULL, NULL);
}

bool d3d9_supports_texture_format(void *data,
      enum texture_gpu_format fmt);

uintptr_t d3d9_load_texture_compressed(void *data,
      const struct texture_compressed *tc, bool threaded,
      enum texture_filter_type filter_type);


RETRO_END_DECLS

#endif

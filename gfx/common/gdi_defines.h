/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  copyright (c) 2011-2017 - Daniel De Matteis
 *  copyright (c) 2016-2019 - Brad Parker
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

#ifndef __GDI_DEFINES_H
#define __GDI_DEFINES_H

#include <stdint.h>

#include <retro_environment.h>
#include <boolean.h>

#include "../video_defines.h"

typedef struct gdi
{
#ifndef __WINRT__
   WNDCLASSEX wndclass;
#endif
   HDC winDC;
   HDC memDC;
   HDC texDC;
   HBITMAP bmp;
   HBITMAP bmp_old;
   uint16_t *temp_buf;
   uint8_t *menu_frame;
   size_t menu_frame_cap;

   /* Backing bitmap for the menu/widget compositing surface.
    * Distinct from gdi->bmp (which is a DDB sized to the core
    * frame): bmp_menu is a top-down 32-bit BGRA DIB section sized
    * to the *window* surface, pre-multiplied alpha-ready, used as
    * the back buffer when XMB/Ozone/MaterialUI or widgets draw via
    * gfx_display_ctx_gdi. The DIB lets us do fast solid-color
    * fills (FillRect + cached brush) and AlphaBlend composites
    * without round-tripping through DDB conversion every frame. */
   HBITMAP bmp_menu;
   HBITMAP bmp_menu_old;
   uint32_t *menu_pixels;          /* DIB-backing pointer; passed straight to SetDIBitsToDevice in the present path. */
   unsigned menu_surface_width;
   unsigned menu_surface_height;

   /* Pre-allocated brushes for solid-fill quads. The current brush is
    * cached and reused when consecutive quads share a colour, which
    * is common (Ozone draws hundreds of background quads in the
    * same theme colour per frame). */
   HBRUSH brush_cached;
   COLORREF brush_color_cached;
   bool brush_color_cached_valid;

   /* Scissor stack for gfx_display_ctx_gdi_scissor_{begin,end}. GDI
    * clip regions don't nest natively, so we save/restore the DC
    * clip region across begin/end. */
   int  scissor_saved;
   bool scissor_active;

   /* Cached scratch DIB sections for hot-path AlphaBlend sources.
    *
    * gfx_display_ctx_gdi_draw and the RGUI alpha helper used to
    * CreateDIBSection / DeleteObject on every call: that's a kernel
    * round-trip per draw and Ozone issues hundreds of draws per
    * frame.  These slots cache the DIB across frames; a draw asks
    * for "at least W x H pixels" via gdi_ensure_scratch_*, which
    * grows the DIB if the request exceeds the current cap and
    * otherwise just hands back the existing pixel pointer.
    *
    *   - scratch_1x1: fixed-size 1x1 BGRA, allocated once at gdi_init
    *     and held for the lifetime of gdi_t.  Used by the translucent
    *     solid-quad path (single premultiplied pixel, AlphaBlend
    *     scaled across the destination).  Never freed except in
    *     gdi_free.
    *   - scratch_quad: variable.  Used by the per-vertex gradient
    *     path (sized to dst_w x dst_h) and the texture-modulated
    *     tint path (sized to the source sub-rect).  Grow-only;
    *     doesn't shrink when smaller draws come along, since
    *     reallocation cost would defeat the purpose.
    *   - scratch_rgui: variable.  Used by the RGUI alpha-composite
    *     path (sized to the menu_frame dimensions).  Separate from
    *     scratch_quad so a frame that uses both doesn't thrash one
    *     slot back and forth.
    *
    * Each slot tracks the HBITMAP, the DIB pixel pointer (we write
    * into it directly), and the current capacity in width/height.
    * Width and height are tracked separately rather than as a
    * pixel count because BITMAPINFOHEADER cares about both. */
   HBITMAP   scratch_1x1_bmp;
   uint32_t *scratch_1x1_pixels;
   HBITMAP   scratch_quad_bmp;
   uint32_t *scratch_quad_pixels;
   unsigned  scratch_quad_w;
   unsigned  scratch_quad_h;
   HBITMAP   scratch_rgui_bmp;
   uint32_t *scratch_rgui_pixels;
   unsigned  scratch_rgui_w;
   unsigned  scratch_rgui_h;

   unsigned frame_width;
   unsigned frame_height;
   unsigned screen_width;
   unsigned screen_height;
   /* Surface (window) size last published via video_driver_set_output_size,
    * tracked here so gdi_alive can read it without locking. */
   unsigned full_width;
   unsigned full_height;
   /* Actual size of gdi->bmp (the DDB).  Separate from frame_width
    * because when RGUI is active we draw the menu (a different size
    * than the core) into bmp; without a dedicated tracker, the
    * comparison against frame_width would trigger a destructive
    * DeleteObject + CreateCompatibleBitmap on every frame, racing
    * with WM_PAINT and producing visible flicker. */
   unsigned bmp_width;
   unsigned bmp_height;

   unsigned menu_width;
   unsigned menu_height;
   unsigned menu_pitch;
   unsigned frame_pitch;
   unsigned frame_bits;
   unsigned menu_bits;
   int win_major;
   int win_minor;

   bool rgb32;
   bool lte_win98;
   bool menu_enable;
   bool menu_full_screen;
   /* True while a textured menu (XMB/Ozone/MaterialUI) is being
    * composited onto bmp_menu in gfx_display_ctx_gdi_draw. RGUI
    * still pushes a 16-bit pixel buffer via set_texture_frame and
    * lives in the gdi->menu_frame path. */
   bool menu_textured_active;

   /* Aspect-ratio-aware viewport.  full_width/full_height hold the
    * window size; x/y/width/height hold the destination rect for
    * the core frame inside the window after applying aspect ratio
    * settings (Settings → Video → Scaling → Aspect Ratio).  Mirrors
    * the d3d8/d3d9 vp pattern: video_driver_update_viewport fills
    * this in, the frame's StretchBlt/StretchDIBits uses x/y/width/
    * height as its destination, and the area outside that rect is
    * cleared to black to produce letterbox/pillarbox bars.
    *
    * keep_aspect is set from video_info->force_aspect at init time
    * and toggled to true when the user changes aspect ratio.
    *
    * should_resize is the dirty flag: window-resize / aspect-ratio
    * changes / state-change pokes set it, gdi_frame consumes it by
    * recomputing the viewport at the start of the next frame. */
   video_viewport_t vp;
   bool keep_aspect;
   bool should_resize;

#ifdef HAVE_OVERLAY
   /* On-screen input overlay state.  Each entry holds an HBITMAP
    * DIB section (32-bit BGRA, premultiplied alpha) that's
    * AlphaBlend'd into the active compositing target every frame.
    * vert_coords and tex_coords match the d3d8/d3d9 layout: each
    * is a 4-float (x, y, w, h) tuple in 0..1 space (window space
    * for vert when fullscreen, viewport space otherwise; texture
    * space for tex).  vertex_geom flips y the same way d3d8 does
    * to keep the same on-screen behaviour. */
   struct gdi_overlay
   {
      HBITMAP   bmp;
      unsigned  tex_w;
      unsigned  tex_h;
      float     tex_coords[4];
      float     vert_coords[4];
      float     alpha_mod;
      bool      fullscreen;
   } *overlays;
   unsigned overlays_size;
   bool overlays_enabled;
#endif
} gdi_t;

#endif

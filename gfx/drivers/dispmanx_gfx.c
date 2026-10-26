/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2017 - Manuel Alfayate
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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <bcm_host.h>

#include <compat/strl.h>
#include <encodings/utf.h>
#include <memalign.h>
#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>
#include <retro_atomic.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../../driver.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../video_views.h"
#include "../video_views_cpu.h"
#include "../video_turn.h"
#include "../font_driver.h"
#include "../../configuration.h"

#ifdef HAVE_OVERLAY
#include "../../input/input_overlay.h"
#endif

/* vc_dispmanx_element_change_attributes()'s change_flags bits, as the
 * userland's vc_vchi_dispmanx.h numbers them; bcm_host.h does not carry
 * them */
#define DISPMANX_CHANGE_OPACITY   (1 << 1)
#define DISPMANX_CHANGE_DEST_RECT (1 << 2)
#define DISPMANX_CHANGE_SRC_RECT  (1 << 3)
#define DISPMANX_CHANGE_TRANSFORM (1 << 5)

/* The firmware's scaler kernels: the smooth one the driver has always
 * loaded, and nearest neighbour. The kernel is global to the HVS and
 * outlives the process, so a nearest kernel is put back on exit. */
#define DISPMANX_KERNEL_SMOOTH  "scaling_kernel 0 -2 -6 -8 -10 -8 -3 2 18 50 82 119 155 187 213 227 227 213 187 155 119 82 50 18 2 -3 -8 -10 -8 -6 -2 0 0"
#define DISPMANX_KERNEL_NEAREST "scaling_kernel 0 0 0 0 0 0 0 0 1 1 1 1 255 255 255 255 255 255 255 255 1 1 1 1 0 0 0 0 0 0 0 0 1"

struct dispmanx_page
{
   /* Each page contains it's own resource handler
    * instead of pointing to in by page number */
   DISPMANX_RESOURCE_HANDLE_T resource;
   /* Whether the flip that put this page on screen is still in flight.
    * The frame thread sets it when it takes the page and the vsync
    * callback clears it for the page that was visible until then, so a
    * release store and an acquire load carry the handoff on their own -
    * the free-page scan below has always read this without a lock. */
   retro_atomic_int_t used;

   /* This field will allow us to access the
    * main _dispvars struct from the vsync CB function */
   struct dispmanx_video *dispvars;

   /* This field will allow us to access the
    * surface the page belongs to. */
   struct dispmanx_surface *surface;
};

struct dispmanx_surface
{
   struct dispmanx_page *pages;
   /* the page that's currently on screen */
   struct dispmanx_page *current_page;
   /* The page where we will dump the render. We need to know this
    * already when we enter the surface update function. No time to wait
    * for free pages before blitting and showing the just rendered frame! */
   struct dispmanx_page *next_page;
   /* The frame laid out as the resource holds it, when the frame is not
    * already: its rows at the resource's pitch, or turned a quarter */
   uint8_t *stage;

   /* The part of the resource shown, in 16.16, and where on the screen */
   VC_RECT_T src_rect;
   VC_RECT_T dst_rect;
   /* The rows a write sends */
   VC_RECT_T bmp_rect;
   VC_DISPMANX_ALPHA_T alpha;

   /* Each surface has it's own element, and the
    * resources are contained one in each page */
   DISPMANX_ELEMENT_HANDLE_T element;
   DISPMANX_TRANSFORM_T transform;
   VC_IMAGE_TYPE_T pixformat;

   unsigned numpages;
   /* The frame as it comes, and as the resource holds it */
   unsigned frame_dims;
   unsigned dims;
   /* Bytes from one row to the next: of the frame as it comes, and of
    * the resource, which a write sends whole */
   unsigned frame_pitch;
   unsigned pitch;
   /* Bytes per pixel */
   unsigned bpp;
   /* Quarter turns counter-clockwise the CPU gives the frame */
   unsigned turn;
   /* Where it shows changed since the element was last told */
   bool dirty;
};

#ifdef HAVE_OVERLAY
/* An input overlay image: an element of its own over its own resource,
 * which the HVS crops, scales, mirrors and blends - nothing is drawn on
 * the CPU after it is loaded */
struct dispmanx_overlay_image
{
   VC_RECT_T dst_rect;
   VC_RECT_T src_rect;
   /* Texture and screen rectangles, 0..1, as the overlay sets them */
   float tex[4];
   float vert[4];
   float alpha;
   DISPMANX_RESOURCE_HANDLE_T resource;
   DISPMANX_ELEMENT_HANDLE_T element;
   DISPMANX_TRANSFORM_T transform;
   unsigned dims;
   unsigned opacity;
};

struct dispmanx_overlay
{
   struct dispmanx_overlay_image *img;
   /* The rectangle the images are laid out on, as last laid out */
   video_viewport_t base;
   unsigned count;
   bool enabled;
   bool full_screen;
   /* Something to tell the HVS with the next flip */
   bool dirty;
};

/* Overlays sit above the menu, one layer an image */
#define DISPMANX_OVERLAY_LAYER 2
#endif

/* A line of text on the screen - the message, or the statistics - on a
 * layer of its own at the screen's resolution: drawn and uploaded only
 * when it changes, into the resource not on the screen, which the next
 * flip shows */
struct dispmanx_text
{
   uint32_t *buf;
   char *text;
   VC_RECT_T dst_rect;
   DISPMANX_RESOURCE_HANDLE_T res[2];
   /* Outgrown, still shown until the flip that took them off lands */
   DISPMANX_RESOURCE_HANDLE_T retired[2];
   DISPMANX_ELEMENT_HANDLE_T element;
   size_t text_len;
   size_t text_cap;
   /* What the resources hold room for, 64 pixels a step */
   unsigned cap_dims;
   unsigned shown;
   unsigned color;
   unsigned px;
   int x;
   int y;
   int32_t layer;
};

/* Text sits above everything else */
#define DISPMANX_TEXT_LAYER 0x7000

struct dispmanx_video
{
   DISPMANX_DISPLAY_HANDLE_T display;
   DISPMANX_UPDATE_HANDLE_T update;

   struct dispmanx_surface *main_surface;
   struct dispmanx_surface *menu_surface;
   struct dispmanx_surface *back_surface;
   /* A core's views composed for the element to scale to the screen,
    * from the buffer the composer draws in */
   struct dispmanx_surface *views_surface;
   video_views_cpu_t views_cpu;
   uint32_t *views_pixels;
   size_t views_pixels_size;
   /* Both eyes side by side, for anaglyph and interlaced */
   uint32_t *views_canvas;
   size_t views_canvas_size;
   /* What a core renders into, rows at the resource's pitch, so its
    * frame goes to the firmware as it is */
   uint8_t *lend;
   size_t lend_size;
   unsigned views_dims;
   /* For get_flags: the last frame with a map was drawn whole */
   retro_atomic_int_t views_fallback;

   /* For threading */
   retro_eventcount_t vsync_ec;
   /* Flips issued and not yet reported by the vsync callback. The
    * callback decrements it and notifies; a waiter re-tests it inside
    * the eventcount's wait window, so a callback landing between the
    * test and the park cannot be missed. */
   retro_atomic_int_t pageflip_pending;

#ifdef HAVE_OVERLAY
   struct dispmanx_overlay ov;
#endif

   /* Where the frame shows on the screen, as the frontend lays it out:
    * aspect, integer scale and custom viewport alike */
   video_viewport_t vp;

   /* The screen, not counting overscan settings */
   unsigned disp_dims;
   /* What the main surface was made for: a core's resolution or
    * rotation changing makes it again. Kept outside the surface, which
    * is freed and set up again. */
   unsigned core_dims;
   unsigned core_rotation;
   /* The menu frame the menu surface was made for */
   unsigned menu_dims;
   /* Quarter turns counter-clockwise, from set_rotation */
   unsigned rotation;
   float menu_alpha;

   /* Without vsync: a frame that comes while a flip is pending is not
    * shown, and nothing waits */
   bool nonblock;
   bool menu_active;
   bool menu_full_screen;
   bool menu_rgb32;
   bool rgb32;
   bool keep_aspect;
   bool smooth;
   /* The driver loaded a scaler kernel of its own */
   bool kernel_loaded;
   bool views_seen;
   bool font_enable;

   /* On-screen messages and statistics */
   const font_renderer_driver_t *font_driver;
   void *font;
   struct dispmanx_text msg_text;
   struct dispmanx_text stat_text;
   /* What the frame being drawn carries, for the flip to show */
   const char *pending_msg;
   const video_frame_info_t *pending_info;
   unsigned font_px;
   char font_path[PATH_MAX_LENGTH];
};

/* Park until every issued flip has been reported by the vsync
 * callback. The re-test sits inside the wait window, so a callback
 * that lands after the first test still releases the waiter. */
static void dispmanx_drain_flips(struct dispmanx_video *_dispvars)
{
   int key;

   while (retro_atomic_load_acquire_int(&_dispvars->pageflip_pending) > 0)
   {
      key = retro_eventcount_prepare_wait(&_dispvars->vsync_ec);

      if (retro_atomic_load_acquire_int(&_dispvars->pageflip_pending) > 0)
         retro_eventcount_commit_wait(&_dispvars->vsync_ec, key);
      else
         retro_eventcount_cancel_wait(&_dispvars->vsync_ec);
   }
}

/* If no free page is available when called, wait for a page flip. */
static struct dispmanx_page *dispmanx_get_free_page(struct dispmanx_video *_dispvars,
      struct dispmanx_surface *surface)
{
   unsigned i;
   int key;
   struct dispmanx_page *page = NULL;

   while (!page)
   {
      /* Try to find a free page */
      for (i = 0; i < surface->numpages; ++i)
      {
         if (!retro_atomic_load_acquire_int(&surface->pages[i].used))
         {
            page = (surface->pages) + i;
            break;
         }
      }

      /* If no page is free at the moment,
       * wait until a free page is freed by vsync CB. */
      if (!page)
      {
         /* One flip's worth of progress, then the scan above runs
          * again - the window opens before the test so a callback in
          * between is counted, not missed. */
         key = retro_eventcount_prepare_wait(&_dispvars->vsync_ec);

         if (retro_atomic_load_acquire_int(&_dispvars->pageflip_pending) > 0)
            retro_eventcount_commit_wait(&_dispvars->vsync_ec, key);
         else
            retro_eventcount_cancel_wait(&_dispvars->vsync_ec);
      }
   }

   /* We mark the chosen page as used */
   retro_atomic_store_release_int(&page->used, 1);

   return page;
}

static void dispmanx_vsync_callback(DISPMANX_UPDATE_HANDLE_T u, void *data)
{
   struct dispmanx_page *page = data;
   struct dispmanx_surface *surface = page->surface;

   /* Marking the page as free must be done before the signaling
    * so when update_main continues (it won't continue until we signal)
    * we can chose this page as free */
   if (surface->current_page)
   {
      /* We mark as free the page that was visible until now */
      retro_atomic_store_release_int(&surface->current_page->used, 0);
   }

   /* The page on which we issued the flip that
    * caused this callback becomes the visible one */
   surface->current_page = page;

   /* Decrement before the notify, so a waiter released by it sees the
    * count this callback settled on. */
   retro_atomic_fetch_sub_int(&page->dispvars->pageflip_pending, 1);
   retro_eventcount_notify(&page->dispvars->vsync_ec);
}

static void dispmanx_surface_free(struct dispmanx_video *_dispvars,
      struct dispmanx_surface **sp)
{
   unsigned i;
   struct dispmanx_surface *surface = *sp;

   /* What if we run into the vsync cb code after freeing the surface?
    * We could be trying to get non-existent lock, signal non-existent condition..
    * So we wait for any pending flips to complete before freeing any surface. */
   dispmanx_drain_flips(_dispvars);

   _dispvars->update = vc_dispmanx_update_start(0);
   vc_dispmanx_element_remove(_dispvars->update, surface->element);
   vc_dispmanx_update_submit_sync(_dispvars->update);

   for (i = 0; i < surface->numpages; i++)
      vc_dispmanx_resource_delete(surface->pages[i].resource);

   free(surface->pages);
   free(surface->stage);
   free(surface);
   *sp = NULL;
}

/* The pitch the resource of a @width pixels wide surface gets: rows
 * aligned to 16 pixels, which the firmware's own alignment divides,
 * so the resource's rows are exactly this far apart */
static unsigned dispmanx_pitch(unsigned width, unsigned bpp)
{
   return (unsigned)ALIGN_UP(width * bpp, 16 * bpp);
}

/* @x,@y,@w x @h of the screen showing the 16.16 source rectangle
 * @sx,@sy,@sw x @sh, mirrored across by @hflip and down by @vflip: what
 * falls off the screen is cut from the source rather than handed to the
 * HVS. False when none of it is on the screen. */
static bool dispmanx_clip(const struct dispmanx_video *_dispvars,
      int x, int y, int w, int h, uint32_t sx, uint32_t sy, uint32_t sw,
      uint32_t sh, bool hflip, bool vflip, VC_RECT_T *dst, VC_RECT_T *src)
{
   int x1  = x + w;
   int y1  = y + h;
   int dw  = (int)VIDEO_SCALE_W(_dispvars->disp_dims);
   int dh  = (int)VIDEO_SCALE_H(_dispvars->disp_dims);
   int cl  = 0, cr = 0, ct = 0, cb = 0, t;
   uint32_t fx, fy, fx1, fy1;

   if (w <= 0 || h <= 0)
      return false;
   if (x < 0)
   {
      cl = -x;
      x  = 0;
   }
   if (y < 0)
   {
      ct = -y;
      y  = 0;
   }
   if (x1 > dw)
   {
      cr = x1 - dw;
      x1 = dw;
   }
   if (y1 > dh)
   {
      cb = y1 - dh;
      y1 = dh;
   }
   if (x1 <= x || y1 <= y)
      return false;

   /* Mirrored, the screen's left edge shows the source's right */
   if (hflip)
   {
      t  = cl;
      cl = cr;
      cr = t;
   }
   if (vflip)
   {
      t  = ct;
      ct = cb;
      cb = t;
   }
   fx  = sx + (uint32_t)(((uint64_t)cl * sw) / (uint32_t)w);
   fx1 = sx + sw - (uint32_t)(((uint64_t)cr * sw) / (uint32_t)w);
   fy  = sy + (uint32_t)(((uint64_t)ct * sh) / (uint32_t)h);
   fy1 = sy + sh - (uint32_t)(((uint64_t)cb * sh) / (uint32_t)h);

   vc_dispmanx_rect_set(dst, (uint32_t)x, (uint32_t)y,
         (uint32_t)(x1 - x), (uint32_t)(y1 - y));
   vc_dispmanx_rect_set(src, fx, fy, fx1 - fx, fy1 - fy);
   return true;
}

/* @s shown over @vp's rectangle of the screen, turned by @transform.
 * Wholly off the screen it is left where it was. Marks @s dirty when
 * where it shows changed. */
static void dispmanx_surface_place(const struct dispmanx_video *_dispvars,
      struct dispmanx_surface *s, const video_viewport_t *vp,
      DISPMANX_TRANSFORM_T transform)
{
   VC_RECT_T dst, src;
   bool half = transform == DISPMANX_ROTATE_180;

   if (!dispmanx_clip(_dispvars, VIDEO_POS_X(vp->pos), VIDEO_POS_Y(vp->pos),
            (int)VIDEO_SCALE_W(vp->dims), (int)VIDEO_SCALE_H(vp->dims),
            0, 0, VIDEO_SCALE_W(s->dims) << 16, VIDEO_SCALE_H(s->dims) << 16,
            half, half, &dst, &src))
      return;

   if (     memcmp(&dst, &s->dst_rect, sizeof(dst))
         || memcmp(&src, &s->src_rect, sizeof(src))
         || transform != s->transform)
   {
      s->dst_rect  = dst;
      s->src_rect  = src;
      s->transform = transform;
      s->dirty     = true;
   }
}

/* Tells @s's element where it shows, in @update */
static void dispmanx_surface_restate(DISPMANX_UPDATE_HANDLE_T update,
      struct dispmanx_surface *s)
{
   vc_dispmanx_element_change_attributes(update, s->element,
         DISPMANX_CHANGE_OPACITY | DISPMANX_CHANGE_DEST_RECT
         | DISPMANX_CHANGE_SRC_RECT | DISPMANX_CHANGE_TRANSFORM,
         0, (uint8_t)s->alpha.opacity, &s->dst_rect, &s->src_rect,
         0, s->transform);
   s->dirty = false;
}

/* A surface for frames of @frame_dims, rows @frame_pitch bytes apart,
 * that the CPU turns @turn quarter turns and the HVS by @transform,
 * shown over @vp. The frame goes to the resource as it is when its
 * rows are the resource's; it is laid out in the stage otherwise. */
static void dispmanx_surface_setup(struct dispmanx_video *_dispvars,
      unsigned frame_dims, unsigned frame_pitch, unsigned bpp,
      VC_IMAGE_TYPE_T pixformat, const VC_DISPMANX_ALPHA_T *alpha,
      unsigned turn, DISPMANX_TRANSFORM_T transform,
      const video_viewport_t *vp, unsigned numpages, int layer,
      struct dispmanx_surface **sp)
{
   unsigned i;
   uint32_t vc_image_ptr = 0;
   struct dispmanx_surface *surface = (struct dispmanx_surface*)
      calloc(1, sizeof(struct dispmanx_surface));

   *sp = NULL;
   if (!surface)
      return;

   surface->numpages    = numpages;
   surface->frame_dims  = frame_dims;
   surface->frame_pitch = frame_pitch;
   surface->bpp         = bpp;
   surface->turn        = turn & 1;
   surface->pixformat   = pixformat;
   surface->alpha       = *alpha;
   surface->transform   = DISPMANX_NO_ROTATE;
   surface->dims        = surface->turn
      ? VIDEO_SCALE_PACK(VIDEO_SCALE_H(frame_dims), VIDEO_SCALE_W(frame_dims))
      : frame_dims;

   /* A frame whose rows are already aligned goes as it is, its padding
    * with it; a turned one, or one whose rows are not, is laid out at
    * the narrowest aligned pitch. */
   if (!surface->turn && frame_pitch == dispmanx_pitch(frame_pitch / bpp, bpp))
      surface->pitch    = frame_pitch;
   else
   {
      surface->pitch    = dispmanx_pitch(VIDEO_SCALE_W(surface->dims), bpp);
      surface->stage    = (uint8_t*)malloc(
            (size_t)surface->pitch * VIDEO_SCALE_H(surface->dims));
   }

   surface->pages = (struct dispmanx_page*)calloc(surface->numpages,
         sizeof(struct dispmanx_page));

   if (!surface->pages || (surface->pitch != frame_pitch && !surface->stage))
   {
      free(surface->pages);
      free(surface->stage);
      free(surface);
      return;
   }

   for (i = 0; i < surface->numpages; i++)
   {
      retro_atomic_store_release_int(&surface->pages[i].used, 0);
      surface->pages[i].surface  = surface;
      surface->pages[i].dispvars = _dispvars;
      /* As wide as the pitch, so the firmware's rows are the pitch the
       * write sends */
      surface->pages[i].resource = vc_dispmanx_resource_create(pixformat,
            surface->pitch / bpp, VIDEO_SCALE_H(surface->dims),
            &vc_image_ptr);
   }

   /* The flipping/callbacks are not still running, so nothing races
    * this first claim */
   surface->next_page = &(surface->pages[0]);
   retro_atomic_store_release_int(&surface->next_page->used, 1);

   vc_dispmanx_rect_set(&surface->bmp_rect, 0, 0,
         VIDEO_SCALE_W(surface->dims), VIDEO_SCALE_H(surface->dims));
   dispmanx_surface_place(_dispvars, surface, vp, transform);

   _dispvars->update = vc_dispmanx_update_start(0);

   surface->element = vc_dispmanx_element_add(
         _dispvars->update, _dispvars->display, layer,
         &surface->dst_rect, surface->pages[0].resource,
         &surface->src_rect, DISPMANX_PROTECTION_NONE,
         &surface->alpha, 0, surface->transform);

   vc_dispmanx_update_submit_sync(_dispvars->update);
   surface->dirty = false;
   *sp            = surface;
}

/* @frame as the resource takes it: the frame itself when its rows are
 * the resource's, or laid out in the stage */
static const void *dispmanx_surface_stage(struct dispmanx_surface *s,
      const void *frame, unsigned pitch)
{
   unsigned y;
   size_t row;

   if (!s->stage || (!s->turn && pitch == s->pitch))
      return frame;

   if (s->turn)
   {
      video_turn_copy(s->stage, s->pitch, (const uint8_t*)frame, pitch,
            VIDEO_SCALE_W(s->frame_dims), VIDEO_SCALE_H(s->frame_dims),
            s->bpp, 1);
      return s->stage;
   }

   row = (size_t)VIDEO_SCALE_W(s->dims) * s->bpp;
   for (y = 0; y < VIDEO_SCALE_H(s->dims); y++)
      memcpy(s->stage + (size_t)y * s->pitch,
            (const uint8_t*)frame + (size_t)y * pitch, row);
   return s->stage;
}

static void dispmanx_surface_update_async(const void *frame,
      unsigned pitch, struct dispmanx_surface *surface)
{
   /* Since it's an async update, there's no need for multiple pages */
   vc_dispmanx_resource_write_data(surface->pages[0].resource,
         surface->pixformat, (int)surface->pitch,
         (void*)dispmanx_surface_stage(surface, frame, pitch),
         &surface->bmp_rect);
}

#ifdef HAVE_OVERLAY
/* Lays the images out on the frame's rectangle, or the whole screen,
 * and tells the HVS what changed in @update: elements added, moved,
 * faded or taken off */
static void dispmanx_overlay_commit(struct dispmanx_video *_dispvars,
      DISPMANX_UPDATE_HANDLE_T update)
{
   unsigned i;
   video_viewport_t base;
   struct dispmanx_overlay *ov = &_dispvars->ov;

   if (ov->full_screen)
   {
      base.pos  = VIDEO_POS_PACK(0, 0);
      base.dims = _dispvars->disp_dims;
   }
   else
   {
      base.pos  = _dispvars->vp.pos;
      base.dims = _dispvars->vp.dims;
   }
   if (base.pos != ov->base.pos || base.dims != ov->base.dims)
   {
      ov->base = base;
      ov->dirty = true;
   }
   if (!ov->dirty)
      return;
   ov->dirty = false;

   for (i = 0; i < ov->count; i++)
   {
      VC_RECT_T dst, src;
      VC_DISPMANX_ALPHA_T alpha;
      struct dispmanx_overlay_image *o = &ov->img[i];
      float bw   = (float)VIDEO_SCALE_W(base.dims);
      float bh   = (float)VIDEO_SCALE_H(base.dims);
      float fx   = VIDEO_POS_X(base.pos) + o->vert[0] * bw;
      float fy   = VIDEO_POS_Y(base.pos) + o->vert[1] * bh;
      float fw   = o->vert[2] * bw;
      float fh   = o->vert[3] * bh;
      float iw   = (float)(VIDEO_SCALE_W(o->dims) << 16);
      float ih   = (float)(VIDEO_SCALE_H(o->dims) << 16);
      bool hflip = fw < 0.0f;
      bool vflip = fh < 0.0f;
      int x0, y0, x1, y1;
      unsigned opacity;
      DISPMANX_TRANSFORM_T transform;

      if (hflip)
      {
         fx += fw;
         fw  = -fw;
      }
      if (vflip)
      {
         fy += fh;
         fh  = -fh;
      }
      /* Both edges rounded, so images that meet still meet */
      x0        = (int)floor(fx + 0.5f);
      y0        = (int)floor(fy + 0.5f);
      x1        = (int)floor(fx + fw + 0.5f);
      y1        = (int)floor(fy + fh + 0.5f);
      opacity   = (unsigned)((o->alpha < 0.0f ? 0.0f
               : o->alpha > 1.0f ? 1.0f : o->alpha) * 255.0f + 0.5f);
      transform = (DISPMANX_TRANSFORM_T)(
              (hflip ? DISPMANX_FLIP_HRIZ : 0)
            | (vflip ? DISPMANX_FLIP_VERT : 0));

      /* Off, empty, transparent or off the screen: no element at all,
       * so the HVS spends nothing on it */
      if (     !ov->enabled || !o->resource || !opacity
            || !dispmanx_clip(_dispvars, x0, y0, x1 - x0, y1 - y0,
               (uint32_t)(o->tex[0] * iw), (uint32_t)(o->tex[1] * ih),
               (uint32_t)(o->tex[2] * iw), (uint32_t)(o->tex[3] * ih),
               hflip, vflip, &dst, &src))
      {
         if (o->element)
            vc_dispmanx_element_remove(update, o->element);
         o->element = 0;
         continue;
      }

      if (!o->element)
      {
         alpha.flags   = (DISPMANX_FLAGS_ALPHA_T)(
               DISPMANX_FLAGS_ALPHA_FROM_SOURCE | DISPMANX_FLAGS_ALPHA_MIX);
         alpha.opacity = opacity;
         alpha.mask    = 0;
         o->element    = vc_dispmanx_element_add(update,
               _dispvars->display, DISPMANX_OVERLAY_LAYER + (int)i, &dst,
               o->resource, &src, DISPMANX_PROTECTION_NONE, &alpha, 0,
               transform);
      }
      else if (   memcmp(&dst, &o->dst_rect, sizeof(dst))
               || memcmp(&src, &o->src_rect, sizeof(src))
               || transform != o->transform || opacity != o->opacity)
         vc_dispmanx_element_change_attributes(update, o->element,
               DISPMANX_CHANGE_OPACITY | DISPMANX_CHANGE_DEST_RECT
               | DISPMANX_CHANGE_SRC_RECT | DISPMANX_CHANGE_TRANSFORM,
               0, (uint8_t)opacity, &dst, &src, 0, transform);
      o->dst_rect  = dst;
      o->src_rect  = src;
      o->transform = transform;
      o->opacity   = opacity;
   }
}

/* Every image's element off the screen and its resource gone */
static void dispmanx_overlay_free(struct dispmanx_video *_dispvars)
{
   unsigned i;
   struct dispmanx_overlay *ov = &_dispvars->ov;
   bool shown                  = false;

   for (i = 0; i < ov->count; i++)
      shown = shown || ov->img[i].element;
   if (shown)
   {
      dispmanx_drain_flips(_dispvars);
      _dispvars->update = vc_dispmanx_update_start(0);
      for (i = 0; i < ov->count; i++)
         if (ov->img[i].element)
            vc_dispmanx_element_remove(_dispvars->update,
                  ov->img[i].element);
      vc_dispmanx_update_submit_sync(_dispvars->update);
   }
   for (i = 0; i < ov->count; i++)
      if (ov->img[i].resource)
         vc_dispmanx_resource_delete(ov->img[i].resource);
   free(ov->img);
   ov->img   = NULL;
   ov->count = 0;
}

static bool dispmanx_overlay_load(void *data, const void *image_data,
      unsigned num_images)
{
   unsigned i;
   uint32_t *row = NULL;
   size_t row_cap = 0;
   struct dispmanx_video *_dispvars   = (struct dispmanx_video*)data;
   const struct texture_image *images = (const struct texture_image*)image_data;
   struct dispmanx_overlay *ov;

   if (!_dispvars)
      return false;
   ov = &_dispvars->ov;
   dispmanx_overlay_free(_dispvars);
   ov->dirty = true;
   if (!num_images || !images)
      return true;
   if (!(ov->img = (struct dispmanx_overlay_image*)calloc(num_images,
               sizeof(*ov->img))))
      return false;
   ov->count = num_images;

   for (i = 0; i < num_images; i++)
   {
      unsigned x, y, pitch;
      uint32_t vc_image_ptr = 0;
      VC_RECT_T rect;
      struct dispmanx_overlay_image *o = &ov->img[i];
      const struct texture_image *img  = &images[i];

      o->tex[2]  = o->tex[3]  = 1.0f;
      o->vert[2] = o->vert[3] = 1.0f;
      o->alpha   = 1.0f;
      if (     !img->pixels || !img->width || !img->height
            || img->pix10 || img->fp16)
         continue;

      /* As wide as its pitch, so the firmware's rows are the ones the
       * upload sends; ARGB8888 is the overlay's own layout */
      pitch = dispmanx_pitch(img->width, 4);
      if ((size_t)pitch * img->height > row_cap)
      {
         uint32_t *grown = (uint32_t*)realloc(row,
               (size_t)pitch * img->height);
         if (!grown)
            continue;
         row     = grown;
         row_cap = (size_t)pitch * img->height;
      }
      for (y = 0; y < img->height; y++)
      {
         const uint32_t *in = img->pixels + (size_t)y * img->width;
         uint32_t *out      = row + (size_t)y * (pitch / 4);
         if (img->supports_rgba)
            for (x = 0; x < img->width; x++)
               out[x] = (in[x] & 0xff00ff00u)
                  | ((in[x] & 0xffu) << 16) | ((in[x] >> 16) & 0xffu);
         else
            memcpy(out, in, (size_t)img->width * 4);
      }
      if (!(o->resource = vc_dispmanx_resource_create(VC_IMAGE_ARGB8888,
                  pitch / 4, img->height, &vc_image_ptr)))
         continue;
      o->dims = VIDEO_SCALE_PACK(img->width, img->height);
      vc_dispmanx_rect_set(&rect, 0, 0, img->width, img->height);
      vc_dispmanx_resource_write_data(o->resource, VC_IMAGE_ARGB8888,
            (int)pitch, row, &rect);
   }
   free(row);
   return true;
}

static void dispmanx_overlay_tex_geom(void *data, unsigned index,
      float x, float y, float w, float h)
{
   struct dispmanx_video *_dispvars = (struct dispmanx_video*)data;
   float *t;
   if (!_dispvars || index >= _dispvars->ov.count)
      return;
   t = _dispvars->ov.img[index].tex;
   if (t[0] != x || t[1] != y || t[2] != w || t[3] != h)
   {
      t[0] = x; t[1] = y; t[2] = w; t[3] = h;
      _dispvars->ov.dirty = true;
   }
}

static void dispmanx_overlay_vertex_geom(void *data, unsigned index,
      float x, float y, float w, float h)
{
   struct dispmanx_video *_dispvars = (struct dispmanx_video*)data;
   float *t;
   if (!_dispvars || index >= _dispvars->ov.count)
      return;
   t = _dispvars->ov.img[index].vert;
   if (t[0] != x || t[1] != y || t[2] != w || t[3] != h)
   {
      t[0] = x; t[1] = y; t[2] = w; t[3] = h;
      _dispvars->ov.dirty = true;
   }
}

static void dispmanx_overlay_enable(void *data, bool state)
{
   struct dispmanx_video *_dispvars = (struct dispmanx_video*)data;
   if (_dispvars && _dispvars->ov.enabled != state)
   {
      _dispvars->ov.enabled = state;
      _dispvars->ov.dirty   = true;
   }
}

static void dispmanx_overlay_full_screen(void *data, bool enable)
{
   struct dispmanx_video *_dispvars = (struct dispmanx_video*)data;
   if (_dispvars && _dispvars->ov.full_screen != enable)
   {
      _dispvars->ov.full_screen = enable;
      _dispvars->ov.dirty       = true;
   }
}

static void dispmanx_overlay_set_alpha(void *data, unsigned index, float mod)
{
   struct dispmanx_video *_dispvars = (struct dispmanx_video*)data;
   if (     _dispvars && index < _dispvars->ov.count
         && _dispvars->ov.img[index].alpha != mod)
   {
      _dispvars->ov.img[index].alpha = mod;
      _dispvars->ov.dirty            = true;
   }
}

static const video_overlay_interface_t dispmanx_overlay_interface = {
   dispmanx_overlay_enable,
   dispmanx_overlay_load,
   NULL, /* load_textures */
   dispmanx_overlay_tex_geom,
   dispmanx_overlay_vertex_geom,
   dispmanx_overlay_full_screen,
   dispmanx_overlay_set_alpha
};

static void dispmanx_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   *iface = &dispmanx_overlay_interface;
}
#endif

/* The font at @px pixels, made again when the size changes */
static bool dispmanx_font(struct dispmanx_video *_dispvars, unsigned px)
{
   if (px < 8)
      px = 8;
   if (_dispvars->font && _dispvars->font_px == px)
      return true;
   if (_dispvars->font)
      _dispvars->font_driver->free(_dispvars->font);
   _dispvars->font = NULL;
   if (!font_renderer_create_default(&_dispvars->font_driver,
            &_dispvars->font,
            *_dispvars->font_path ? _dispvars->font_path : NULL,
            px, FONT_ATLAS_FORMAT_A8))
   {
      _dispvars->font_enable = false;
      return false;
   }
   _dispvars->font_px = px;
   /* Glyphs are read from the atlas in memory: it may grow */
   _dispvars->font_driver->get_atlas(_dispvars->font)->max_dims
      = VIDEO_SCALE_PACK(2048, 2048);
   return true;
}

/* The rectangle @msg covers with its baseline starting at @base_x,
 * @base_y, as x0, y0, x1, y1 */
static void dispmanx_text_measure(struct dispmanx_video *_dispvars,
      const char *msg, size_t msg_len, int base_x, int base_y, int *r)
{
   void *font_data = _dispvars->font;
   const struct font_glyph *(*get_glyph)(void*, uint32_t)
      = _dispvars->font_driver->get_glyph;
   const struct font_glyph *glyph_q = get_glyph(font_data, '?');
   struct font_line_metrics *line_metrics = NULL;
   int line_h = 0, line_x = base_x, line_y = base_y;

   if (_dispvars->font_driver->get_line_metrics)
      _dispvars->font_driver->get_line_metrics(font_data, &line_metrics);
   if (line_metrics)
      line_h = (int)line_metrics->height;
   r[0] = r[1] = 0x7fffffff;
   r[2] = r[3] = -0x7fffffff;

#define FONT_LAYOUT_ALIGNED 0
#define FONT_LAYOUT_LINE(line, line_width, count, bytes) \
   do \
   { \
      (void)(line_width); (void)(count); (void)(bytes); \
      line_x = base_x; \
      line_y = base_y + (line) * line_h; \
   } while (0)
#define FONT_LAYOUT_GLYPH(glyph, pen_x, pen_y) \
   do \
   { \
      int gx = line_x + (pen_x) + (glyph)->draw_offset_x; \
      int gy = line_y + (pen_y) + (glyph)->draw_offset_y; \
      int gw = (int)VIDEO_SCALE_W((glyph)->dims); \
      int gh = (int)VIDEO_SCALE_H((glyph)->dims); \
      if (gw > 0 && gh > 0) \
      { \
         if (gx < r[0]) r[0] = gx; \
         if (gy < r[1]) r[1] = gy; \
         if (gx + gw > r[2]) r[2] = gx + gw; \
         if (gy + gh > r[3]) r[3] = gy + gh; \
      } \
   } while (0)
#include "../font_layout.h"
}

/* @msg drawn into @out, rows @stride pixels apart, whose top left is
 * the screen's @ox, @oy: @color with the glyphs' coverage as alpha */
static void dispmanx_text_draw(struct dispmanx_video *_dispvars,
      const char *msg, size_t msg_len, int base_x, int base_y,
      uint32_t *out, unsigned stride, int ox, int oy, unsigned ow,
      unsigned oh, uint32_t color)
{
   int x, y;
   void *font_data = _dispvars->font;
   const struct font_glyph *(*get_glyph)(void*, uint32_t)
      = _dispvars->font_driver->get_glyph;
   const struct font_glyph *glyph_q = get_glyph(font_data, '?');
   const struct font_atlas *atlas;
   struct font_line_metrics *line_metrics = NULL;
   int line_h = 0, line_x = base_x, line_y = base_y;

   if (_dispvars->font_driver->get_line_metrics)
      _dispvars->font_driver->get_line_metrics(font_data, &line_metrics);
   if (line_metrics)
      line_h = (int)line_metrics->height;

#define FONT_LAYOUT_ALIGNED 0
#define FONT_LAYOUT_LINE(line, line_width, count, bytes) \
   do \
   { \
      (void)(line_width); (void)(count); (void)(bytes); \
      line_x = base_x; \
      line_y = base_y + (line) * line_h; \
   } while (0)
#define FONT_LAYOUT_GLYPH(glyph, pen_x, pen_y) \
   do \
   { \
      int gx = line_x + (pen_x) + (glyph)->draw_offset_x - ox; \
      int gy = line_y + (pen_y) + (glyph)->draw_offset_y - oy; \
      int gw = (int)VIDEO_SCALE_W((glyph)->dims); \
      int gh = (int)VIDEO_SCALE_H((glyph)->dims); \
      const uint8_t *gsrc; \
      atlas = _dispvars->font_driver->get_atlas(font_data); \
      gsrc  = atlas->buffer + VIDEO_SCALE_W((glyph)->atlas_pos) \
         + (size_t)VIDEO_SCALE_H((glyph)->atlas_pos) * atlas->width; \
      for (y = 0; y < gh; y++) \
      { \
         int py = gy + y; \
         if (py < 0 || py >= (int)oh) \
            continue; \
         for (x = 0; x < gw; x++) \
         { \
            int px = gx + x; \
            uint32_t a = gsrc[(size_t)y * atlas->width + x]; \
            uint32_t *d; \
            if (!a || px < 0 || px >= (int)ow) \
               continue; \
            d = out + (size_t)py * stride + px; \
            if (a > (*d >> 24)) \
               *d = (a << 24) | color; \
         } \
      } \
   } while (0)
#include "../font_layout.h"
}

/* Every resource and the element of @t gone */
static void dispmanx_text_free(struct dispmanx_video *_dispvars,
      struct dispmanx_text *t)
{
   unsigned i;

   if (t->element)
   {
      dispmanx_drain_flips(_dispvars);
      _dispvars->update = vc_dispmanx_update_start(0);
      vc_dispmanx_element_remove(_dispvars->update, t->element);
      vc_dispmanx_update_submit_sync(_dispvars->update);
      t->element = 0;
   }
   for (i = 0; i < 2; i++)
   {
      if (t->res[i])
         vc_dispmanx_resource_delete(t->res[i]);
      if (t->retired[i])
         vc_dispmanx_resource_delete(t->retired[i]);
      t->res[i] = t->retired[i] = 0;
   }
   t->cap_dims = 0;
   free(t->buf);
   t->buf      = NULL;
}

/* @msg at @base_x, @base_y in @color, told to the HVS in @update: drawn
 * and uploaded only when it is not what @t shows already */
static void dispmanx_text_commit(struct dispmanx_video *_dispvars,
      struct dispmanx_text *t, DISPMANX_UPDATE_HANDLE_T update,
      const char *msg, size_t msg_len, int base_x, int base_y,
      uint32_t color)
{
   int r[4];
   unsigned w, h, cw, ch, next, y;
   VC_RECT_T src, rect;
   VC_DISPMANX_ALPHA_T alpha;
   int dw = (int)VIDEO_SCALE_W(_dispvars->disp_dims);
   int dh = (int)VIDEO_SCALE_H(_dispvars->disp_dims);

   /* The flips are drained: what the last one took off is free */
   for (y = 0; y < 2; y++)
      if (t->retired[y])
      {
         vc_dispmanx_resource_delete(t->retired[y]);
         t->retired[y] = 0;
      }

   if (!msg || !msg_len)
   {
      if (t->element)
         vc_dispmanx_element_remove(update, t->element);
      t->element  = 0;
      t->text_len = 0;
      return;
   }
   if (     t->element && msg_len == t->text_len && color == t->color
         && base_x == t->x && base_y == t->y && t->px == _dispvars->font_px
         && !memcmp(msg, t->text, msg_len))
      return;

   if (msg_len + 1 > t->text_cap)
   {
      char *grown = (char*)realloc(t->text, msg_len + 1);
      if (!grown)
         return;
      t->text     = grown;
      t->text_cap = msg_len + 1;
   }
   memcpy(t->text, msg, msg_len);
   t->text[msg_len] = '\0';
   t->text_len      = msg_len;
   t->color         = color;
   t->x             = base_x;
   t->y             = base_y;
   t->px            = _dispvars->font_px;

   dispmanx_text_measure(_dispvars, msg, msg_len, base_x, base_y, r);
   if (r[0] < 0)  r[0] = 0;
   if (r[1] < 0)  r[1] = 0;
   if (r[2] > dw) r[2] = dw;
   if (r[3] > dh) r[3] = dh;
   if (r[2] <= r[0] || r[3] <= r[1])
   {
      if (t->element)
         vc_dispmanx_element_remove(update, t->element);
      t->element = 0;
      return;
   }
   w = (unsigned)(r[2] - r[0]);
   h = (unsigned)(r[3] - r[1]);

   /* Room for it, both resources, in steps so a growing line does not
    * make them again every frame */
   if (w > VIDEO_SCALE_W(t->cap_dims) || h > VIDEO_SCALE_H(t->cap_dims))
   {
      uint32_t vc_image_ptr = 0;
      cw = (w + 63) & ~63u;
      ch = (h + 63) & ~63u;
      /* The element comes off with this flip; its resources go once
       * the flip has landed */
      if (t->element)
         vc_dispmanx_element_remove(update, t->element);
      t->element    = 0;
      t->retired[0] = t->res[0];
      t->retired[1] = t->res[1];
      free(t->buf);
      t->res[0]   = vc_dispmanx_resource_create(VC_IMAGE_ARGB8888, cw, ch,
            &vc_image_ptr);
      t->res[1]   = vc_dispmanx_resource_create(VC_IMAGE_ARGB8888, cw, ch,
            &vc_image_ptr);
      t->buf      = (uint32_t*)malloc((size_t)cw * ch * 4);
      t->cap_dims = (t->res[0] && t->res[1] && t->buf)
         ? VIDEO_SCALE_PACK(cw, ch) : 0;
      if (!t->cap_dims)
      {
         t->text_len = 0;
         return;
      }
   }
   cw = VIDEO_SCALE_W(t->cap_dims);

   for (y = 0; y < h; y++)
      memset(t->buf + (size_t)y * cw, 0, (size_t)w * 4);
   dispmanx_text_draw(_dispvars, msg, msg_len, base_x, base_y, t->buf, cw,
         r[0], r[1], w, h, color);

   /* Into the resource not on the screen; the flip shows it */
   next = t->element ? (t->shown ^ 1) : 0;
   vc_dispmanx_rect_set(&rect, 0, 0, w, h);
   vc_dispmanx_resource_write_data(t->res[next], VC_IMAGE_ARGB8888,
         (int)(cw * 4), t->buf, &rect);
   vc_dispmanx_rect_set(&t->dst_rect, (uint32_t)r[0], (uint32_t)r[1], w, h);
   vc_dispmanx_rect_set(&src, 0, 0, w << 16, h << 16);
   t->shown = next;

   if (!t->element)
   {
      alpha.flags   = DISPMANX_FLAGS_ALPHA_FROM_SOURCE;
      alpha.opacity = 255;
      alpha.mask    = 0;
      t->element    = vc_dispmanx_element_add(update, _dispvars->display,
            t->layer, &t->dst_rect, t->res[next], &src,
            DISPMANX_PROTECTION_NONE, &alpha, 0, DISPMANX_NO_ROTATE);
   }
   else
   {
      vc_dispmanx_element_change_source(update, t->element, t->res[next]);
      vc_dispmanx_element_change_attributes(update, t->element,
            DISPMANX_CHANGE_DEST_RECT | DISPMANX_CHANGE_SRC_RECT,
            0, 255, &t->dst_rect, &src, 0, DISPMANX_NO_ROTATE);
   }
}

/* The message and the statistics the frame carries, in @update */
static void dispmanx_texts_commit(struct dispmanx_video *_dispvars,
      DISPMANX_UPDATE_HANDLE_T update)
{
   uint32_t color;
   float cr, cg, cb;
   const video_frame_info_t *info = _dispvars->pending_info;
   const char *msg                = _dispvars->pending_msg;
   const char *stat               = NULL;
   int dw = (int)VIDEO_SCALE_W(_dispvars->disp_dims);
   int dh = (int)VIDEO_SCALE_H(_dispvars->disp_dims);

   if (!_dispvars->font_enable || !info)
      return;
   if (info->statistics_show && info->stat_text_len)
      stat = info->stat_text;
   if (     ((msg && *msg) || stat)
         && !dispmanx_font(_dispvars, (unsigned)(info->font_size + 0.5f)))
      return;

   cr    = info->font_msg_color_r < 0.0f ? 0.0f
      : info->font_msg_color_r > 1.0f ? 1.0f : info->font_msg_color_r;
   cg    = info->font_msg_color_g < 0.0f ? 0.0f
      : info->font_msg_color_g > 1.0f ? 1.0f : info->font_msg_color_g;
   cb    = info->font_msg_color_b < 0.0f ? 0.0f
      : info->font_msg_color_b > 1.0f ? 1.0f : info->font_msg_color_b;
   color = ((uint32_t)(cr * 255.0f) << 16) | ((uint32_t)(cg * 255.0f) << 8)
      | (uint32_t)(cb * 255.0f);

   dispmanx_text_commit(_dispvars, &_dispvars->msg_text, update,
         msg, msg ? strlen(msg) : 0,
         (int)(info->font_msg_pos_x * dw),
         (int)((1.0f - info->font_msg_pos_y) * dh), color);
   dispmanx_text_commit(_dispvars, &_dispvars->stat_text, update,
         stat, stat ? info->stat_text_len : 0,
         (int)(info->osd_stat_params.x * dw),
         (int)((1.0f - info->osd_stat_params.y) * dh), color);
}

static void dispmanx_surface_update(struct dispmanx_video *_dispvars,
      const void *frame, unsigned pitch, struct dispmanx_surface *surface)
{
   if (     _dispvars->nonblock
         && retro_atomic_load_acquire_int(&_dispvars->pageflip_pending) > 0)
      return;

   /* Frame blitting */
   vc_dispmanx_resource_write_data(surface->next_page->resource,
         surface->pixformat, (int)surface->pitch,
         (void*)dispmanx_surface_stage(surface, frame, pitch),
         &surface->bmp_rect);

   /* Dispmanx doesn't support more than one pending pageflip. Doing so would overwrite
    * the page in the callback function, so we would be always freeing the same page. */
   dispmanx_drain_flips(_dispvars);

   /* Issue a page flip that will be done at the next vsync, and a new
    * place on the screen with it */
   _dispvars->update = vc_dispmanx_update_start(0);

   vc_dispmanx_element_change_source(_dispvars->update, surface->element,
         surface->next_page->resource);
   if (surface->dirty)
      dispmanx_surface_restate(_dispvars->update, surface);
#ifdef HAVE_OVERLAY
   dispmanx_overlay_commit(_dispvars, _dispvars->update);
#endif
   dispmanx_texts_commit(_dispvars, _dispvars->update);

   retro_atomic_fetch_add_int(&_dispvars->pageflip_pending, 1);

   vc_dispmanx_update_submit(_dispvars->update,
      dispmanx_vsync_callback, (void*)(surface->next_page));

   /* This may block waiting on a new page when max_swapchain_images <= 2 */
   surface->next_page = dispmanx_get_free_page(_dispvars, surface);
}

/* Enable/disable bilinear filtering. */
static void dispmanx_set_scaling(bool bilinear_filter)
{
   vc_gencmd_send("%s", bilinear_filter
         ? DISPMANX_KERNEL_SMOOTH : DISPMANX_KERNEL_NEAREST);
}

/* The whole screen */
static void dispmanx_full_viewport(const struct dispmanx_video *_dispvars,
      video_viewport_t *vp)
{
   vp->pos       = VIDEO_POS_PACK(0, 0);
   vp->dims      = _dispvars->disp_dims;
   vp->full_dims = _dispvars->disp_dims;
}

/* Where the frame shows, as the frontend lays it out on the screen */
static void dispmanx_update_viewport(struct dispmanx_video *_dispvars)
{
   dispmanx_full_viewport(_dispvars, &_dispvars->vp);
   video_driver_update_viewport(&_dispvars->vp, false,
         _dispvars->keep_aspect, true);
}

static void dispmanx_blank_console(struct dispmanx_video *_dispvars)
{
   /* Since pitch will be aligned to 16 pixels (not bytes) we use a
    * 16 pixels image to save the alignment */
   uint16_t image[16] = {0x0000};
   video_viewport_t vp;
   VC_DISPMANX_ALPHA_T alpha;

   alpha.flags   = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS;
   alpha.opacity = 255;
   alpha.mask    = 0;
   dispmanx_full_viewport(_dispvars, &vp);

   dispmanx_surface_setup(_dispvars, VIDEO_SCALE_PACK(16, 1),
         sizeof(image), 2, VC_IMAGE_RGB565, &alpha, 0, DISPMANX_NO_ROTATE,
         &vp, 1, -1, &_dispvars->back_surface);

   /* Updating 1-page surface synchronously causes problems, since the 1st CB will
    * signal but not free because the only page is on screen, so get_free will wait forever. */
   if (_dispvars->back_surface)
      dispmanx_surface_update_async(image, sizeof(image),
            _dispvars->back_surface);
}

/* video_monitor_index to a firmware display: 0 the firmware's own
 * choice, then the HDMI ports, the LCD and composite */
static const uint32_t dispmanx_displays[] = {
   DISPMANX_ID_MAIN_LCD,
   DISPMANX_ID_HDMI0,
   DISPMANX_ID_HDMI1,
   DISPMANX_ID_FORCE_LCD,
   DISPMANX_ID_SDTV
};

static void *dispmanx_init(const video_info_t *video)
{
   uint32_t disp_w = 0;
   uint32_t disp_h = 0;
   uint32_t id     = DISPMANX_ID_MAIN_LCD;
   settings_t *settings             = config_get_ptr();
   struct dispmanx_video *_dispvars = calloc(1, sizeof(struct dispmanx_video));

   if (!_dispvars)
      return NULL;

   if (     settings && settings->uints.video_monitor_index
         < sizeof(dispmanx_displays) / sizeof(dispmanx_displays[0]))
      id = dispmanx_displays[settings->uints.video_monitor_index];

   bcm_host_init();
   _dispvars->display = vc_dispmanx_display_open(id);
   if (!_dispvars->display && id != DISPMANX_ID_MAIN_LCD)
   {
      RARCH_WARN("[Dispmanx] Display %u not connected, using the default.\n",
            (unsigned)id);
      id                 = DISPMANX_ID_MAIN_LCD;
      _dispvars->display = vc_dispmanx_display_open(id);
   }

   /* If the console framebuffer has active overscan settings,
    * the user must have overscan_scale=1 in config.txt to have
    * the same size for both fb console and dispmanx. */
   graphics_get_display_size((uint16_t)id, &disp_w, &disp_h);
   _dispvars->disp_dims = VIDEO_SCALE_PACK(disp_w, disp_h);

   retro_atomic_store_release_int(&_dispvars->pageflip_pending, 0);
   _dispvars->rgb32       = video->rgb32;
   _dispvars->keep_aspect = video->force_aspect;
   _dispvars->smooth      = video->smooth;
   _dispvars->menu_alpha  = 1.0f;
   _dispvars->nonblock    = !video->vsync;
   _dispvars->font_enable = video->font_enable;
   _dispvars->msg_text.layer  = DISPMANX_TEXT_LAYER;
   _dispvars->stat_text.layer = DISPMANX_TEXT_LAYER + 1;
   if (video->path_font)
      strlcpy(_dispvars->font_path, video->path_font,
            sizeof(_dispvars->font_path));

   if (!retro_eventcount_init(&_dispvars->vsync_ec))
   {
      vc_dispmanx_display_close(_dispvars->display);
      bcm_host_deinit();
      free(_dispvars);
      return NULL;
   }

   /* Smooth leaves the firmware's kernel, and so config.txt's
    * scaling_kernel, in place */
   if (!video->smooth)
   {
      dispmanx_set_scaling(false);
      _dispvars->kernel_loaded = true;
   }

   dispmanx_full_viewport(_dispvars, &_dispvars->vp);
   dispmanx_blank_console(_dispvars);

   video_driver_display_type_set(RARCH_DISPLAY_VIDEOCORE);
   /* What views are laid out on */
   video_driver_set_output_dims(_dispvars->disp_dims);

   return _dispvars;
}

/* Grows *@buf to at least @size bytes */
static bool dispmanx_views_grow(uint32_t **buf, size_t *have, size_t size)
{
   uint32_t *grown;
   if (size <= *have)
      return true;
   if (!(grown = (uint32_t*)realloc(*buf, size)))
      return false;
   *buf  = grown;
   *have = size;
   return true;
}

/* A core's views: laid out on the screen, composed at the scale that
 * draws no view larger than it is, into a surface whose element the
 * HVS enlarges to the whole screen. Each view straight from the frame.
 * False when the frame is to be drawn whole. */
static bool dispmanx_views_frame(struct dispmanx_video *_dispvars,
      const void *frame, unsigned dims, unsigned pitch,
      const video_frame_info_t *info)
{
   float scale;
   unsigned cdims;
   unsigned cpitch;
   const video_views_layout_t *layout;
   bool views = info && frame && info->views.num_views
      && video_views_fit_frame(&info->views, dims);

   retro_atomic_store_release_int(&_dispvars->views_fallback,
         (info && info->views.num_views && !views) ? 1 : 0);
   if (!views)
   {
      if (_dispvars->views_surface)
      {
         dispmanx_surface_free(_dispvars, &_dispvars->views_surface);
         /* The frame's surface is set up again on its way back */
         _dispvars->core_dims = 0;
      }
      return false;
   }

   layout = &info->views_layout;
   scale  = video_views_cpu_scale(&info->views, layout);
   cdims  = video_views_cpu_dims(info->dims, scale);
   cpitch = dispmanx_pitch(VIDEO_SCALE_W(cdims), 4);
   if (!_dispvars->views_surface || _dispvars->views_dims != cdims)
   {
      video_viewport_t vp;
      VC_DISPMANX_ALPHA_T alpha;

      alpha.flags   = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS;
      alpha.opacity = 255;
      alpha.mask    = 0;
      dispmanx_full_viewport(_dispvars, &vp);

      if (_dispvars->views_surface)
         dispmanx_surface_free(_dispvars, &_dispvars->views_surface);
      /* The views take the frame's place on the screen */
      if (_dispvars->main_surface)
         dispmanx_surface_free(_dispvars, &_dispvars->main_surface);
      dispmanx_surface_setup(_dispvars, cdims, cpitch, 4,
            VC_IMAGE_XRGB8888, &alpha, 0, DISPMANX_NO_ROTATE, &vp,
            info->max_swapchain_images, 0, &_dispvars->views_surface);
      if (!_dispvars->views_surface)
         return false;
      _dispvars->views_dims = cdims;
   }
   if (!dispmanx_views_grow(&_dispvars->views_pixels,
            &_dispvars->views_pixels_size,
            (size_t)cpitch * VIDEO_SCALE_H(cdims)))
      return false;
   if (     layout->offscreen
         && !dispmanx_views_grow(&_dispvars->views_canvas,
            &_dispvars->views_canvas_size,
            (size_t)2 * VIDEO_SCALE_AREA(cdims) * sizeof(uint32_t)))
      return false;
   if (!_dispvars->views_seen)
   {
      _dispvars->views_seen = true;
      RARCH_LOG("[Dispmanx] Views drawn straight from the frame.\n");
   }

   /* Each view turned in its own place, as the layout laid them out */
   _dispvars->views_cpu.rot = _dispvars->rotation;
   video_views_cpu_compose(&_dispvars->views_cpu, &info->views, layout,
         frame, pitch,
         _dispvars->rgb32 ? SCALER_FMT_ARGB8888 : SCALER_FMT_RGB565,
         info->video_smooth ? SCALER_TYPE_BILINEAR : SCALER_TYPE_POINT,
         scale, cdims, _dispvars->views_pixels, cpitch / 4,
         layout->offscreen ? _dispvars->views_canvas : NULL);
   dispmanx_surface_update(_dispvars, _dispvars->views_pixels, cpitch,
         _dispvars->views_surface);
   return true;
}

static bool dispmanx_frame(void *data, const void *frame,
      unsigned dims, uint64_t frame_count, unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
   DISPMANX_TRANSFORM_T transform;
   struct dispmanx_video *_dispvars = data;
   unsigned    max_swapchain_images = video_info->max_swapchain_images;
#ifdef HAVE_MENU
   bool menu_is_alive = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif

   if (!frame)
      return true;

   _dispvars->pending_msg  = msg;
   _dispvars->pending_info = video_info;

   if (dispmanx_views_frame(_dispvars, frame, dims, pitch, video_info))
   {
#ifdef HAVE_MENU
      menu_driver_frame(menu_is_alive, video_info);
#endif
      return true;
   }

   /* Sanity check. */
   if (!VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims))
      return true;

   /* The HVS flips but cannot turn a quarter: the CPU turns a quarter
    * and the HVS the half, so one turn of the frame serves both */
   transform = (_dispvars->rotation & 2)
      ? DISPMANX_ROTATE_180 : DISPMANX_NO_ROTATE;
   dispmanx_update_viewport(_dispvars);

   /* A surface with a stage takes rows of any pitch; one without, only
    * its own */
   if (     (dims  != _dispvars->core_dims)
         || (_dispvars->rotation != _dispvars->core_rotation)
         || !_dispvars->main_surface
         || (     pitch != _dispvars->main_surface->pitch
               && !_dispvars->main_surface->stage))
   {
      VC_DISPMANX_ALPHA_T alpha;

      alpha.flags   = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS;
      alpha.opacity = 255;
      alpha.mask    = 0;

      _dispvars->core_dims     = dims;
      _dispvars->core_rotation = _dispvars->rotation;

      if (_dispvars->main_surface)
         dispmanx_surface_free(_dispvars, &_dispvars->main_surface);

      dispmanx_surface_setup(_dispvars, dims, pitch,
            _dispvars->rgb32 ? 4 : 2,
            _dispvars->rgb32 ? VC_IMAGE_XRGB8888 : VC_IMAGE_RGB565,
            &alpha, _dispvars->rotation & 1, transform, &_dispvars->vp,
            max_swapchain_images, 0, &_dispvars->main_surface);
      if (!_dispvars->main_surface)
      {
         _dispvars->core_dims = 0;
         return true;
      }
   }
   else
      /* Aspect, integer scale and viewport changes move the element */
      dispmanx_surface_place(_dispvars, _dispvars->main_surface,
            &_dispvars->vp, transform);

#ifdef HAVE_MENU
   menu_driver_frame(menu_is_alive, video_info);
#endif

   /* Update main surface: locate free page, blit and flip. */
   dispmanx_surface_update(_dispvars, frame, pitch, _dispvars->main_surface);
   return true;
}

static void dispmanx_set_texture_enable(void *data, bool state, bool full_screen)
{
   struct dispmanx_video *_dispvars = data;

   /* If it was active but it's not anymore... */
   if (!state && _dispvars->menu_surface)
      dispmanx_surface_free(_dispvars, &_dispvars->menu_surface);

   _dispvars->menu_active      = state;
   _dispvars->menu_full_screen = full_screen;
}

static void dispmanx_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned dims, float alpha)
{
   video_viewport_t vp;
   struct dispmanx_video *_dispvars = data;
   unsigned bpp                     = rgb32 ? 4 : 2;

   if (!_dispvars->menu_active || !frame
         || !VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims))
      return;

   if (alpha < 0.0f)
      alpha = 0.0f;
   else if (alpha > 1.0f)
      alpha = 1.0f;

   /* The menu shows where the frame does, or over the whole screen */
   if (_dispvars->menu_full_screen)
      dispmanx_full_viewport(_dispvars, &vp);
   else
   {
      if (!_dispvars->main_surface && !_dispvars->views_surface)
         dispmanx_update_viewport(_dispvars);
      vp = _dispvars->vp;
   }

   /* A menu frame of a new size or format makes the surface again */
   if (     _dispvars->menu_surface
         && (     dims  != _dispvars->menu_dims
               || rgb32 != _dispvars->menu_rgb32))
      dispmanx_surface_free(_dispvars, &_dispvars->menu_surface);

   if (!_dispvars->menu_surface)
   {
      VC_DISPMANX_ALPHA_T a;

      /* Blended by its own alpha, as the menu's texture is everywhere */
      a.flags   = (DISPMANX_FLAGS_ALPHA_T)(DISPMANX_FLAGS_ALPHA_FROM_SOURCE
            | DISPMANX_FLAGS_ALPHA_MIX);
      a.opacity = (uint32_t)(alpha * 255.0f + 0.5f);
      a.mask    = 0;

      _dispvars->menu_dims  = dims;
      _dispvars->menu_rgb32 = rgb32;
      _dispvars->menu_alpha = alpha;

      /* Menu surface only needs a page as it will be updated
       * asynchronously. A layer above the frame's, so a frame surface
       * made again under an open menu stays under it. */
      dispmanx_surface_setup(_dispvars, dims, VIDEO_SCALE_W(dims) * bpp,
            bpp, rgb32 ? VC_IMAGE_ARGB8888 : VC_IMAGE_RGBA16, &a, 0,
            DISPMANX_NO_ROTATE, &vp, 1, 1, &_dispvars->menu_surface);
      if (!_dispvars->menu_surface)
         return;
   }
   else
   {
      if (alpha != _dispvars->menu_alpha)
      {
         _dispvars->menu_alpha                   = alpha;
         _dispvars->menu_surface->alpha.opacity  =
            (uint32_t)(alpha * 255.0f + 0.5f);
         _dispvars->menu_surface->dirty          = true;
      }
      dispmanx_surface_place(_dispvars, _dispvars->menu_surface, &vp,
            DISPMANX_NO_ROTATE);
      /* Rare - a viewport or opacity change - so sent on its own */
      if (_dispvars->menu_surface->dirty)
      {
         dispmanx_drain_flips(_dispvars);
         _dispvars->update = vc_dispmanx_update_start(0);
         dispmanx_surface_restate(_dispvars->update,
               _dispvars->menu_surface);
         vc_dispmanx_update_submit_sync(_dispvars->update);
      }
   }

   /* We update the menu surface if menu is active.
    * This update is asynchronous, yet menu screen update
    * will be synced because main surface updating is synchronous */
   dispmanx_surface_update_async(frame, VIDEO_SCALE_W(dims) * bpp,
         _dispvars->menu_surface);
}

static void dispmanx_set_nonblock_state(void *data, bool toggle,
      bool adaptive_vsync_enabled, unsigned swap_interval)
{
   struct dispmanx_video *vid = data;
   if (vid)
      vid->nonblock = toggle;
}

static bool dispmanx_alive(void *data) { return true; }
static bool dispmanx_focus(void *data) { return true; }

static void dispmanx_viewport_info(void *data, struct video_viewport *vp)
{
   struct dispmanx_video *vid = data;

   if (!vid)
      return;

   /* The whole screen while views draw, else where the frame shows */
   if (vid->views_surface)
      dispmanx_full_viewport(vid, vp);
   else
   {
      *vp           = vid->vp;
      vp->full_dims = vid->disp_dims;
   }
}

/* The screen as shown - frame, menu and overlay - cut to the viewport:
 * BGR24, bottom row first. Off the screen is black. */
static bool dispmanx_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   int x, y;
   VC_RECT_T rect;
   video_viewport_t vp;
   uint32_t vc_image_ptr = 0;
   uint8_t *shot;
   DISPMANX_RESOURCE_HANDLE_T res;
   struct dispmanx_video *vid = data;
   unsigned dw, dh, pitch;
   int vx, vy, vw, vh;

   if (!vid || !buffer)
      return false;
   dispmanx_viewport_info(vid, &vp);
   dw    = VIDEO_SCALE_W(vid->disp_dims);
   dh    = VIDEO_SCALE_H(vid->disp_dims);
   vx    = VIDEO_POS_X(vp.pos);
   vy    = VIDEO_POS_Y(vp.pos);
   vw    = (int)VIDEO_SCALE_W(vp.dims);
   vh    = (int)VIDEO_SCALE_H(vp.dims);
   /* The firmware's rows for an RGB888 resource: 32 bytes aligned */
   pitch = (unsigned)ALIGN_UP(dw * 3, 32);
   if (!dw || !dh || vw <= 0 || vh <= 0)
      return false;
   if (!(res = vc_dispmanx_resource_create(VC_IMAGE_RGB888, dw, dh,
               &vc_image_ptr)))
      return false;
   if (!(shot = (uint8_t*)malloc((size_t)pitch * dh)))
   {
      vc_dispmanx_resource_delete(res);
      return false;
   }
   vc_dispmanx_rect_set(&rect, 0, 0, dw, dh);
   vc_dispmanx_snapshot(vid->display, res, DISPMANX_NO_ROTATE);
   vc_dispmanx_resource_read_data(res, &rect, shot, pitch);
   vc_dispmanx_resource_delete(res);

   for (y = 0; y < vh; y++)
   {
      int sy       = vy + (vh - 1 - y);
      uint8_t *out = buffer + (size_t)y * vw * 3;
      for (x = 0; x < vw; x++, out += 3)
      {
         int sx = vx + x;
         if (sx < 0 || sy < 0 || sx >= (int)dw || sy >= (int)dh)
            out[0] = out[1] = out[2] = 0;
         else
         {
            const uint8_t *in = shot + (size_t)sy * pitch + (size_t)sx * 3;
            out[0] = in[2];
            out[1] = in[1];
            out[2] = in[0];
         }
      }
   }
   free(shot);
   return true;
}

/* Taken up when the next frame comes, where the surfaces are made */
static void dispmanx_set_rotation(void *data, unsigned rotation)
{
   struct dispmanx_video *vid = data;
   if (vid)
      vid->rotation = rotation & 3;
}

static void dispmanx_set_filtering(void *data, unsigned index,
      bool smooth, bool ctx_scaling)
{
   struct dispmanx_video *vid = data;
   if (!vid || smooth == vid->smooth)
      return;
   dispmanx_set_scaling(smooth);
   vid->smooth        = smooth;
   vid->kernel_loaded = true;
}

/* The frontend has laid the new aspect out; the next frame takes it */
static void dispmanx_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   struct dispmanx_video *vid = data;
   if (vid)
      vid->keep_aspect = true;
}

/* A frame to render into, at the pitch the firmware takes: not while
 * the CPU turns the frame or views are drawn, which read it elsewhere */
static bool dispmanx_get_current_software_framebuffer(void *data,
      struct retro_framebuffer *fb)
{
   size_t size;
   unsigned pitch;
   struct dispmanx_video *vid = data;
   unsigned bpp               = (vid && vid->rgb32) ? 4 : 2;

   if (     !vid || !fb || !fb->width || !fb->height
         || (vid->rotation & 1) || vid->views_surface)
      return false;

   pitch = dispmanx_pitch(fb->width, bpp);
   size  = (size_t)pitch * fb->height;
   if (size > vid->lend_size)
   {
      uint8_t *grown = (uint8_t*)realloc(vid->lend, size);
      if (!grown)
         return false;
      vid->lend      = grown;
      vid->lend_size = size;
   }

   fb->data         = vid->lend;
   fb->pitch        = pitch;
   fb->format       = vid->rgb32
      ? RETRO_PIXEL_FORMAT_XRGB8888 : RETRO_PIXEL_FORMAT_RGB565;
   fb->memory_flags = RETRO_MEMORY_TYPE_CACHED;
   return true;
}

/* The threaded wrapper's frame buffers, rows at the pitch the firmware
 * takes: the wrapper's copy pads them, and the frame goes from there as
 * it is. A write is read before frame() returns, so memory is always
 * free to be written again. */
static void *dispmanx_frame_mem_alloc(void *data, size_t size,
      uintptr_t *id, bool *cached, unsigned *pitch_align)
{
   struct dispmanx_video *vid = data;
   void *mem;
   if (!vid || !(mem = memalign_alloc(64, size)))
      return NULL;
   *id          = (uintptr_t)mem;
   *cached      = true;
   *pitch_align = 16 * (vid->rgb32 ? 4 : 2);
   return mem;
}

static bool dispmanx_frame_mem_ready(void *data, uintptr_t id)
{
   return true;
}

static void dispmanx_frame_mem_free(void *data, uintptr_t id)
{
   memalign_free((void*)id);
}

static bool dispmanx_suppress_screensaver(void *data, bool enable) { return false; }
static bool dispmanx_set_shader(void *data,
      enum rarch_shader_type type, const char *path) { return false; }

static uint32_t dispmanx_get_flags(void *data)
{
   uint32_t             flags = 0;
   struct dispmanx_video *_dispvars = data;
   if (_dispvars)
   {
      BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS);
      if (retro_atomic_load_acquire_int(&_dispvars->views_fallback))
         BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS_FALLBACK);
   }
   return flags;
}

static const video_poke_interface_t dispmanx_poke_interface = {
   dispmanx_get_flags,
   NULL, /* load_texture */
   NULL, /* unload_texture */
   NULL, /* set_video_mode */
   NULL, /* get_refresh_rate */
   dispmanx_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   dispmanx_set_aspect_ratio,
   NULL, /* dispmanx_apply_state_changes */
   dispmanx_set_texture_frame,
   dispmanx_set_texture_enable,
   NULL, /* set_osd_msg */
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   dispmanx_get_current_software_framebuffer,
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL, /* set_hdr_subpixel_layout */
   NULL, /* supports_texture_format */
   NULL, /* load_texture_compressed */
   NULL, /* present_last */
   NULL, /* get_last_present_time */
   NULL, /* hw_ring_install */
   NULL, /* hw_ring_fence_new */
   NULL, /* hw_ring_fence_free */
   NULL, /* hw_ring_fence_signal */
   NULL, /* hw_ring_fence_wait */
   NULL, /* hw_ring_capture */
   NULL, /* hw_ring_present_slot */
   NULL, /* hw_ring_context_new */
   NULL, /* hw_ring_context_free */
   NULL, /* hw_ring_framebuffer */
   NULL, /* update_texture */
   NULL, /* get_swap_interval_cap */
   NULL, /* texture_lend */
   NULL, /* texture_lend_ready */
   NULL, /* get_last_present_wait */
   NULL, /* set_view_count */
   NULL, /* hw_context_destroying */
   NULL, /* get_headset_refresh */
   NULL, /* set_frame_format */
   dispmanx_frame_mem_alloc,
   dispmanx_frame_mem_ready,
   dispmanx_frame_mem_free
};

static void dispmanx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   *iface = &dispmanx_poke_interface;
}

static void dispmanx_free(void *data)
{
   struct dispmanx_video *_dispvars = data;

   if (!_dispvars)
      return;

   if (_dispvars->main_surface)
      dispmanx_surface_free(_dispvars, &_dispvars->main_surface);
   if (_dispvars->views_surface)
      dispmanx_surface_free(_dispvars, &_dispvars->views_surface);
   if (_dispvars->back_surface)
      dispmanx_surface_free(_dispvars, &_dispvars->back_surface);
   video_views_cpu_free(&_dispvars->views_cpu);
   free(_dispvars->views_pixels);
   free(_dispvars->views_canvas);
   free(_dispvars->lend);

   if (_dispvars->menu_surface)
      dispmanx_surface_free(_dispvars, &_dispvars->menu_surface);
#ifdef HAVE_OVERLAY
   dispmanx_overlay_free(_dispvars);
#endif
   dispmanx_text_free(_dispvars, &_dispvars->msg_text);
   dispmanx_text_free(_dispvars, &_dispvars->stat_text);
   free(_dispvars->msg_text.text);
   free(_dispvars->stat_text.text);
   if (_dispvars->font)
      _dispvars->font_driver->free(_dispvars->font);

   /* The console and whatever runs next scale smooth again */
   if (_dispvars->kernel_loaded && !_dispvars->smooth)
      dispmanx_set_scaling(true);

   /* Close display and deinitialize. */
   vc_dispmanx_display_close(_dispvars->display);
   bcm_host_deinit();

   retro_eventcount_free(&_dispvars->vsync_ec);

   free(_dispvars);
}

video_driver_t video_dispmanx = {
   dispmanx_init,
   dispmanx_frame,
   dispmanx_set_nonblock_state,
   dispmanx_alive,
   dispmanx_focus,
   dispmanx_suppress_screensaver,
   NULL, /* has_windowed */
   dispmanx_set_shader,
   dispmanx_free,
   "dispmanx",
   NULL, /* set_viewport */
   dispmanx_set_rotation,
   dispmanx_viewport_info,
   dispmanx_read_viewport,
#ifdef HAVE_OVERLAY
   dispmanx_get_overlay_interface,
#endif
   dispmanx_get_poke_interface,
   NULL, /* wrap_type_to_enum */
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   NULL  /* gfx_widgets_enabled */
#endif
};

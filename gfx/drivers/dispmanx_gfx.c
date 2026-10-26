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

#include <stdlib.h>
#include <string.h>

#include <bcm_host.h>

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

/* @s shown over @vp's rectangle of the screen, turned by @transform:
 * what falls off the screen is cut from the source rather than handed
 * to the HVS. Marks @s dirty when where it shows changed. */
static void dispmanx_surface_place(const struct dispmanx_video *_dispvars,
      struct dispmanx_surface *s, const video_viewport_t *vp,
      DISPMANX_TRANSFORM_T transform)
{
   VC_RECT_T dst, src;
   int x0      = VIDEO_POS_X(vp->pos);
   int y0      = VIDEO_POS_Y(vp->pos);
   int w       = (int)VIDEO_SCALE_W(vp->dims);
   int h       = (int)VIDEO_SCALE_H(vp->dims);
   int x1      = x0 + w;
   int y1      = y0 + h;
   int sw      = (int)VIDEO_SCALE_W(s->dims);
   int sh      = (int)VIDEO_SCALE_H(s->dims);
   int cx      = 0;
   int cy      = 0;
   uint32_t fx, fy, fw, fh;

   if (w <= 0 || h <= 0)
      return;

   if (x0 < 0)
   {
      cx = -x0;
      x0 = 0;
   }
   if (y0 < 0)
   {
      cy = -y0;
      y0 = 0;
   }
   if (x1 > (int)VIDEO_SCALE_W(_dispvars->disp_dims))
      x1 = (int)VIDEO_SCALE_W(_dispvars->disp_dims);
   if (y1 > (int)VIDEO_SCALE_H(_dispvars->disp_dims))
      y1 = (int)VIDEO_SCALE_H(_dispvars->disp_dims);
   /* Wholly off the screen: left where it was */
   if (x1 <= x0 || y1 <= y0)
      return;

   /* The source in 16.16, scaled from the part of the rectangle shown */
   fx = (uint32_t)(((uint64_t)cx * (uint32_t)sw << 16) / (uint32_t)w);
   fy = (uint32_t)(((uint64_t)cy * (uint32_t)sh << 16) / (uint32_t)h);
   fw = (uint32_t)(((uint64_t)(x1 - x0) * (uint32_t)sw << 16) / (uint32_t)w);
   fh = (uint32_t)(((uint64_t)(y1 - y0) * (uint32_t)sh << 16) / (uint32_t)h);
   /* Turned half round, the screen's left edge shows the source's right */
   if (transform == DISPMANX_ROTATE_180)
   {
      fx = ((uint32_t)sw << 16) - fx - fw;
      fy = ((uint32_t)sh << 16) - fy - fh;
   }

   vc_dispmanx_rect_set(&dst, (uint32_t)x0, (uint32_t)y0,
         (uint32_t)(x1 - x0), (uint32_t)(y1 - y0));
   vc_dispmanx_rect_set(&src, fx, fy, fw, fh);

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

static void *dispmanx_init(const video_info_t *video)
{
   uint32_t disp_w = 0;
   uint32_t disp_h = 0;
   struct dispmanx_video *_dispvars = calloc(1, sizeof(struct dispmanx_video));

   if (!_dispvars)
      return NULL;

   bcm_host_init();
   _dispvars->display = vc_dispmanx_display_open(0 /* LCD */);

   /* If the console framebuffer has active overscan settings,
    * the user must have overscan_scale=1 in config.txt to have
    * the same size for both fb console and dispmanx. */
   graphics_get_display_size(_dispvars->display, &disp_w, &disp_h);
   _dispvars->disp_dims = VIDEO_SCALE_PACK(disp_w, disp_h);

   retro_atomic_store_release_int(&_dispvars->pageflip_pending, 0);
   _dispvars->rgb32       = video->rgb32;
   _dispvars->keep_aspect = video->force_aspect;
   _dispvars->smooth      = video->smooth;
   _dispvars->menu_alpha  = 1.0f;
   _dispvars->nonblock    = !video->vsync;

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
   NULL, /* read_viewport */
#ifdef HAVE_OVERLAY
   NULL, /* overlay_interface */
#endif
   dispmanx_get_poke_interface,
   NULL, /* wrap_type_to_enum */
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   NULL  /* gfx_widgets_enabled */
#endif
};

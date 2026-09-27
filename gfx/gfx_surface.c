/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - Libretro team
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

#include "gfx_surface.h"
#include "gfx_instrument.h"

/* Slots start on a cache line so a producer's row loops and the
 * driver's memcpy into staging run on aligned memory. */
#define GFX_SURFACE_SLOT_ALIGN 64

gfx_surface_t *gfx_surface_new(unsigned dims,
      unsigned num_slots, enum texture_filter_type filter,
      gfx_surface_release_t release, void *user)
{
   gfx_surface_t *s;
   uint8_t *base;
   size_t frame_len, i;

   if (     !VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims)
         || !num_slots || num_slots > GFX_SURFACE_MAX_SLOTS
         || (size_t)VIDEO_SCALE_W(dims) > (SIZE_MAX / sizeof(uint32_t)) / VIDEO_SCALE_H(dims))
      return NULL;

   frame_len = (VIDEO_SCALE_AREA(dims) * sizeof(uint32_t)
         + GFX_SURFACE_SLOT_ALIGN - 1) & ~(size_t)(GFX_SURFACE_SLOT_ALIGN - 1);
   if (frame_len > (SIZE_MAX - sizeof(*s) - GFX_SURFACE_SLOT_ALIGN) / num_slots)
      return NULL;

   if (!(s = (gfx_surface_t*)calloc(1,
         sizeof(*s) + GFX_SURFACE_SLOT_ALIGN + frame_len * num_slots)))
      return NULL;

   base = (uint8_t*)(s + 1);
   base = (uint8_t*)(((uintptr_t)base + GFX_SURFACE_SLOT_ALIGN - 1)
         & ~(uintptr_t)(GFX_SURFACE_SLOT_ALIGN - 1));
   for (i = 0; i < num_slots; i++)
      s->slots[i] = (uint32_t*)(base + i * frame_len);

   s->release    = release;
   s->user       = user;
   s->dims       = dims;
   s->num_slots  = num_slots;
   s->filter     = filter;
   s->rgba       = 0xff;
   s->can_update = video_driver_texture_can_update() ? 1 : 0;
   GFX_INSTR_INC(GFX_INSTR_SURFACE_NEW);
   GFX_INSTR_ADD(GFX_INSTR_SURFACE_BYTES, (int)(frame_len * num_slots));
   return s;
}

bool gfx_surface_query_requirements(unsigned width,
      gfx_surface_requirements_t *req)
{
   if (!req)
      return false;
   if ((size_t)width > ((size_t)-1) / sizeof(uint32_t))
      return false;
   req->rgba       = (video_driver_get_disp_flags() & VIDEO_FLAG_USE_RGBA)
         ? true : false;
   /* 8888 is always sampled; the wider formats are what the driver
    * and its context say they can take. The preference is the widest
    * of them, since a producer with a wider source loses nothing by
    * decoding into it and everything by being narrowed twice. */
   req->formats    = GFX_SURFACE_PIXFMT_8888;
   if (video_driver_test_all_flags(GFX_CTX_FLAGS_SCREEN_10BPC_SOURCE))
      req->formats |= GFX_SURFACE_PIXFMT_2101010;
   if (video_driver_test_all_flags(GFX_CTX_FLAGS_SCRGB_FRAMEBUFFER))
      req->formats |= GFX_SURFACE_PIXFMT_FP16;
   if (req->formats & GFX_SURFACE_PIXFMT_FP16)
      req->preferred = GFX_SURFACE_PIXFMT_FP16;
   else if (req->formats & GFX_SURFACE_PIXFMT_2101010)
      req->preferred = GFX_SURFACE_PIXFMT_2101010;
   else
      req->preferred = GFX_SURFACE_PIXFMT_8888;
   req->can_update = video_driver_texture_can_update();
   /* Every upload path in the tree takes tightly packed 32-bit rows;
    * the alignment is what the GL paths set (glPixelStorei) and what
    * the others are happy with. */
   req->pitch      = (size_t)width * sizeof(uint32_t);
   req->align      = 4;
   return true;
}

bool gfx_surface_wants_rgba(void)
{
   gfx_surface_requirements_t req;
   if (!gfx_surface_query_requirements(0, &req))
      return false;
   return req.rgba;
}

bool gfx_surface_supports_compressed(enum texture_gpu_format fmt)
{
   return video_driver_supports_texture_format(fmt);
}

gfx_surface_t *gfx_surface_new_static(unsigned dims,
      enum texture_filter_type filter)
{
   gfx_surface_t *s;

   if (!VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims))
      return NULL;
   if (!(s = (gfx_surface_t*)calloc(1, sizeof(*s))))
      return NULL;
   s->dims       = dims;
   s->num_slots  = 0;
   s->filter     = filter;
   s->rgba       = 0xff;
   s->can_update = video_driver_texture_can_update() ? 1 : 0;
   GFX_INSTR_INC(GFX_INSTR_SURFACE_NEW);
   return s;
}

/* The synchronous upload of s->img: a replacement texture when there
 * is none yet, the order changed, or the driver has no in-place path,
 * else an update. Direct video runs the driver here; under the wrapper
 * this is the fallback when the post was refused, and the driver
 * marshals each call itself. */
static enum gfx_surface_submit_result gfx_surface_upload_sync(
      gfx_surface_t *s, bool rgba)
{
   uintptr_t new_handle = 0;

   if (s->handle && s->rgba == rgba && s->can_update)
   {
      if (video_driver_texture_update(s->handle, &s->img))
         return GFX_SURFACE_SUBMIT_DONE;
      s->can_update = 0;
   }

   if (!video_driver_texture_load(&s->img, s->filter, &new_handle)
         || !new_handle)
      return GFX_SURFACE_SUBMIT_FAILED;
   if (s->handle)
      video_driver_texture_unload(&s->handle);
   s->handle = new_handle;
   s->rgba   = rgba ? 1 : 0;
   return GFX_SURFACE_SUBMIT_DONE;
}

#ifdef HAVE_THREADS
/* Main thread, from video_thread_async_poll(): the video thread is
 * done with the slot. A load brought a texture (0: nothing, the
 * previous one stays and the order is forgotten so the next submit
 * loads again); an update brought the handle back (0: the driver
 * refused, so from now on this surface loads replacements). */
static void gfx_surface_done(void *user, uintptr_t handle)
{
   gfx_surface_t *s = (gfx_surface_t*)user;
   unsigned slot    = s->inflight_slot;

   s->inflight      = 0;

   if (s->node.kind == VIDEO_THREAD_ASYNC_LOAD)
   {
      if (handle)
      {
         if (s->handle)
            video_driver_texture_unload(&s->handle);
         s->handle = handle;
      }
      else
         s->rgba   = 0xff;
   }
   else if (!handle)
      s->can_update = 0;

   if (s->dying)
   {
      if (s->handle)
         video_driver_texture_unload(&s->handle);
      free(s->adopted);
      free(s);
      return;
   }
   if (s->release)
      s->release(s->user, s, slot);
}
#endif

static enum gfx_surface_submit_result gfx_surface_submit_img(
      gfx_surface_t *s, unsigned slot, bool rgba)
{
#ifdef HAVE_THREADS
   if (video_driver_thread_wrapper_active())
   {
      bool need_load = !s->handle || s->rgba != (rgba ? 1 : 0)
            || !s->can_update;

      s->node.kind    = need_load
            ? VIDEO_THREAD_ASYNC_LOAD : VIDEO_THREAD_ASYNC_UPDATE;
      s->node.img     = &s->img;
      s->node.handle  = s->handle;
      s->node.filter  = s->filter;
      s->node.done    = gfx_surface_done;
      s->node.user    = s;
      s->node.release = NULL;
      if (video_thread_async_post(&s->node))
      {
         s->inflight      = 1;
         s->inflight_slot = slot;
         if (need_load)
            s->rgba       = rgba ? 1 : 0;
         return GFX_SURFACE_SUBMIT_QUEUED;
      }
      /* Refused: the wrapper is going away, or this is the video
       * thread. The driver runs the call in place. */
   }
#endif
   return gfx_surface_upload_sync(s, rgba);
}

enum gfx_surface_submit_result gfx_surface_submit(gfx_surface_t *s,
      unsigned slot, bool rgba)
{
   if (!s || slot >= s->num_slots)
   {
      GFX_INSTR_INC(GFX_INSTR_SUBMIT_FAILED);
      return GFX_SURFACE_SUBMIT_FAILED;
   }
   if (s->inflight)
   {
      GFX_INSTR_INC(GFX_INSTR_SUBMIT_BUSY);
      return GFX_SURFACE_SUBMIT_BUSY;
   }

   s->img.pixels        = s->slots[slot];
   s->img.width         = VIDEO_SCALE_W(s->dims);
   s->img.height        = VIDEO_SCALE_H(s->dims);
   s->img.supports_rgba = rgba;
   s->img.pix10         = false;
   s->img.compressed    = NULL;
   {
      enum gfx_surface_submit_result r = gfx_surface_submit_img(s, slot, rgba);
      GFX_INSTR_INC(r == GFX_SURFACE_SUBMIT_QUEUED
            ? GFX_INSTR_SUBMIT_QUEUED
            : (r == GFX_SURFACE_SUBMIT_DONE
               ? GFX_INSTR_SUBMIT_DONE : GFX_INSTR_SUBMIT_FAILED));
      return r;
   }
}

enum gfx_surface_submit_result gfx_surface_submit_pixels(gfx_surface_t *s,
      const uint32_t *pixels, bool rgba)
{
   if (!s || !pixels)
      return GFX_SURFACE_SUBMIT_FAILED;
   if (s->inflight)
      return GFX_SURFACE_SUBMIT_BUSY;

#ifdef HAVE_THREADS
   if (video_driver_thread_wrapper_active())
   {
      /* The caller's buffer does not outlive this call for the video
       * thread's purposes; a slot does. One copy, the size of a frame,
       * against a wait of up to a present. */
      GFX_INSTR_INC(GFX_INSTR_SUBMIT_COPY);
      memcpy(s->slots[0], pixels,
            VIDEO_SCALE_AREA(s->dims) * sizeof(uint32_t));
      return gfx_surface_submit(s, 0, rgba);
   }
#endif

   s->img.pixels        = (uint32_t*)pixels;
   s->img.width         = VIDEO_SCALE_W(s->dims);
   s->img.height        = VIDEO_SCALE_H(s->dims);
   s->img.supports_rgba = rgba;
   s->img.pix10         = false;
   s->img.compressed    = NULL;
   {
      enum gfx_surface_submit_result r = gfx_surface_upload_sync(s, rgba);
      GFX_INSTR_INC(r == GFX_SURFACE_SUBMIT_DONE
            ? GFX_INSTR_SUBMIT_DONE : GFX_INSTR_SUBMIT_FAILED);
      return r;
   }
}

enum gfx_surface_submit_result gfx_surface_submit_external(gfx_surface_t *s,
      const uint32_t *pixels, bool rgba,
      gfx_surface_release_t release, void *user)
{
   if (!s || !pixels || s->num_slots)
      return GFX_SURFACE_SUBMIT_FAILED;
   if (s->inflight)
      return GFX_SURFACE_SUBMIT_BUSY;

   s->release           = release;
   s->user              = user;
   s->img.pixels        = (uint32_t*)pixels;
   s->img.width         = VIDEO_SCALE_W(s->dims);
   s->img.height        = VIDEO_SCALE_H(s->dims);
   s->img.supports_rgba = rgba;
   s->img.pix10         = false;
   s->img.compressed    = NULL;
   {
      /* inflight_slot is meaningless without slots; release() gets 0
       * and the caller looks at the surface, not the slot. */
      enum gfx_surface_submit_result r = gfx_surface_submit_img(s, 0, rgba);
      GFX_INSTR_INC(r == GFX_SURFACE_SUBMIT_QUEUED
            ? GFX_INSTR_SUBMIT_QUEUED
            : (r == GFX_SURFACE_SUBMIT_DONE
               ? GFX_INSTR_SUBMIT_DONE : GFX_INSTR_SUBMIT_FAILED));
      return r;
   }
}

void gfx_surface_free(gfx_surface_t *s)
{
   if (!s)
      return;
   GFX_INSTR_INC(GFX_INSTR_SURFACE_FREE);
   if (s->inflight)
   {
      /* The video thread still reads the slot and, for a load, will
       * hand back a texture: the completion unloads and frees. */
      s->dying = 1;
      return;
   }
   if (s->handle)
      video_driver_texture_unload(&s->handle);
   free(s);
}

bool gfx_surface_free_adopt(gfx_surface_t *s, void *pixels)
{
   if (s && s->inflight && !s->num_slots && pixels)
   {
      GFX_INSTR_INC(GFX_INSTR_SURFACE_FREE);
      s->adopted = pixels;
      s->dying   = 1;
      return true;
   }
   gfx_surface_free(s);
   return false;
}

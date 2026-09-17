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

gfx_surface_t *gfx_surface_new(unsigned width, unsigned height,
      unsigned num_slots, enum texture_filter_type filter,
      gfx_surface_release_t release, void *user)
{
   gfx_surface_t *s;
   uint8_t *base;
   size_t frame_len, i;

   if (     !width || !height
         || !num_slots || num_slots > GFX_SURFACE_MAX_SLOTS
         || (size_t)width > (SIZE_MAX / sizeof(uint32_t)) / height)
      return NULL;

   frame_len = ((size_t)width * height * sizeof(uint32_t)
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
   s->width      = width;
   s->height     = height;
   s->num_slots  = num_slots;
   s->filter     = filter;
   s->rgba       = 0xff;
   s->can_update = video_driver_texture_can_update() ? 1 : 0;
   GFX_INSTR_INC(GFX_INSTR_SURFACE_NEW);
   GFX_INSTR_ADD(GFX_INSTR_SURFACE_BYTES, (int)(frame_len * num_slots));
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
   s->img.width         = s->width;
   s->img.height        = s->height;
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
            (size_t)s->width * s->height * sizeof(uint32_t));
      return gfx_surface_submit(s, 0, rgba);
   }
#endif

   s->img.pixels        = (uint32_t*)pixels;
   s->img.width         = s->width;
   s->img.height        = s->height;
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

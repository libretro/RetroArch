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

#define GFX_SURFACE_FMT_NONE 0xff

gfx_surface_t *gfx_surface_new(unsigned dims,
      unsigned num_slots, uint32_t pixfmt, enum texture_filter_type filter,
      gfx_surface_release_t release, void *user)
{
   gfx_surface_t *s;
   uint8_t *base;
   size_t frame_len, i, bpp;

   bpp = GFX_SURFACE_PIXFMT_BPP(pixfmt);
   if (     !VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims)
         || !num_slots || num_slots > GFX_SURFACE_MAX_SLOTS
         || !pixfmt || (pixfmt & (pixfmt - 1))
         || pixfmt > GFX_SURFACE_PIXFMT_GX_RGBA8
         || (size_t)VIDEO_SCALE_W(dims) > ((SIZE_MAX - GFX_SURFACE_SLOT_ALIGN)
               / bpp) / VIDEO_SCALE_H(dims))
      return NULL;

   frame_len = (VIDEO_SCALE_AREA(dims) * bpp
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
   {
      s->slots[i]     = (uint32_t*)(base + i * frame_len);
      s->own_slots[i] = s->slots[i];
   }

   s->release    = release;
   s->user       = user;
   s->dims       = dims;
   s->num_slots  = num_slots;
   s->pixfmt     = pixfmt;
   s->filter     = filter;
   s->fmt        = GFX_SURFACE_FMT_NONE;
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
   req->rgba       = (video_driver_get_disp_flags() & VIDEO_FLAG_USE_RGBA)
         ? true : false;
#ifdef GEKKO
   /* The gx driver has no texture upload: an overlay's pixels are
    * sampled where they lie, so they must already be GX tiles. */
   req->formats    = GFX_SURFACE_PIXFMT_GX_RGBA8;
   req->preferred  = GFX_SURFACE_PIXFMT_GX_RGBA8;
#else
   /* 8888 is always sampled; the wider formats are what the driver
    * says its texture interface takes. The preference is the widest
    * of them, since a producer with a wider source loses nothing by
    * decoding into it and everything by being narrowed twice.
    *
    * FP16 is listed where the driver keeps half floats
    * (TEXTURE_GPU_FORMAT_RGBA16F) and shows them as linear scRGB
    * (TEXTURE_GPU_FORMAT_SCRGB), which it does only while the output
    * is HDR: anywhere else the composite treats a texture as SDR, and
    * a linear texel would be encoded a second time. */
   req->formats    = GFX_SURFACE_PIXFMT_8888;
   /* The texture path's own answer, not whether the context presents
    * 10-bit core frames (GFX_CTX_FLAGS_SCREEN_10BPC_SOURCE): the two
    * are set by different code and need not agree. */
   if (video_driver_supports_texture_format(TEXTURE_GPU_FORMAT_RGB10A2))
      req->formats |= GFX_SURFACE_PIXFMT_2101010;
   if (     video_driver_supports_texture_format(TEXTURE_GPU_FORMAT_RGBA16F)
         && video_driver_supports_texture_format(TEXTURE_GPU_FORMAT_SCRGB))
      req->formats |= GFX_SURFACE_PIXFMT_FP16;
   if (req->formats & GFX_SURFACE_PIXFMT_FP16)
      req->preferred = GFX_SURFACE_PIXFMT_FP16;
   else if (req->formats & GFX_SURFACE_PIXFMT_2101010)
      req->preferred = GFX_SURFACE_PIXFMT_2101010;
   else
      req->preferred = GFX_SURFACE_PIXFMT_8888;
#endif
   if ((size_t)width > ((size_t)-1) / GFX_SURFACE_PIXFMT_BPP(req->preferred))
      return false;
   req->can_update = video_driver_texture_can_update();
   /* Every upload path in the tree takes tightly packed rows; the
    * alignment is what the GL paths set (glPixelStorei) and what the
    * others are happy with. */
   req->pitch      = (size_t)width * GFX_SURFACE_PIXFMT_BPP(req->preferred);
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
   s->fmt        = GFX_SURFACE_FMT_NONE;
   s->can_update = video_driver_texture_can_update() ? 1 : 0;
   GFX_INSTR_INC(GFX_INSTR_SURFACE_NEW);
   return s;
}

/* Whether a 2101010 frame has to be narrowed for the driver up. */
static bool gfx_surface_must_narrow(uint32_t pixfmt)
{
   return pixfmt == GFX_SURFACE_PIXFMT_2101010
      && !video_driver_supports_texture_format(TEXTURE_GPU_FORMAT_RGB10A2);
}

/* s->img for an upload of @pixels in @pixfmt, and the texture key it
 * makes: the layout bit above the channel order, which is all an
 * in-place update has to agree with. A 2101010 frame the driver cannot
 * sample is narrowed in place to 8888 in the order @rgba names - only
 * for a slot, which is the surface's to rewrite. False for anything
 * else that cannot reach the GPU as it is: the submit fails rather
 * than upload one layout as another. */
static bool gfx_surface_prepare(gfx_surface_t *s, const void *pixels,
      uint32_t pixfmt, bool rgba, bool is_slot, uint8_t *fmt)
{
   s->img.pixels        = (uint32_t*)pixels;
   s->img.width         = VIDEO_SCALE_W(s->dims);
   s->img.height        = VIDEO_SCALE_H(s->dims);
   s->img.supports_rgba = rgba;
   s->img.compressed    = NULL;
   s->img.fp16          = false;
   switch (pixfmt)
   {
      case GFX_SURFACE_PIXFMT_8888:
         s->img.pix10   = false;
         break;
      case GFX_SURFACE_PIXFMT_FP16:
         /* Half floats have no narrower form here: the driver takes
          * them as they are or the submit fails. */
         if (!video_driver_supports_texture_format(TEXTURE_GPU_FORMAT_RGBA16F))
            return false;
         s->img.pix10   = false;
         s->img.fp16    = true;
         *fmt = 4;
         return true;
      case GFX_SURFACE_PIXFMT_2101010:
         s->img.pix10   = true;
         if (gfx_surface_must_narrow(pixfmt))
         {
            if (!is_slot)
               return false;
            image_texture_narrow_10bit(&s->img);
         }
         break;
      default:
         return false;
   }
   *fmt = (uint8_t)((s->img.pix10 ? 2 : 0) | (rgba ? 1 : 0));
   return true;
}

/* The synchronous upload of s->img: a replacement texture when there
 * is none yet, the format changed, or the driver has no in-place path,
 * else an update. Direct video runs the driver here; under the wrapper
 * this is the fallback when the post was refused, and the driver
 * marshals each call itself. */
/* Every slot back on the surface's own memory, before the texture
 * that owns the lent memory is replaced or unloaded. */
static void gfx_surface_unlend(gfx_surface_t *s)
{
   unsigned i;
   if (!s->lent)
      return;
   for (i = 0; i < s->num_slots; i++)
      s->slots[i] = s->own_slots[i];
   s->lent_spare = NULL;
   s->lent       = 0;
   s->lent_cur   = 0;
}

/* After a direct submit of @slot: the texture streams, so the next
 * frame for the slot can be written where the driver uploads it from,
 * and the copy into that memory goes away. Rows as tightly packed as
 * the slot's own; the driver lends only memory laid out so. A surface
 * of two slots or more borrows the driver slot of the same number: its
 * producer alternates them, so one is written while the other's copy
 * runs. A surface of one slot borrows both driver slots at once, and
 * the writability check alternates them for it; a single buffer would
 * have each frame wait on the last one's copy. */
static void gfx_surface_lend(gfx_surface_t *s, unsigned slot)
{
   size_t pitch;
   void *mem, *spare;
   if (     slot >= s->num_slots || (s->lent & (1u << slot))
         || !s->handle || !s->can_update)
      return;
   pitch = (size_t)VIDEO_SCALE_W(s->dims) * GFX_SURFACE_PIXFMT_BPP(s->pixfmt);
   if (s->num_slots >= 2)
   {
      if ((mem = video_driver_texture_lend(s->handle, slot, pitch)))
      {
         s->slots[slot] = (uint32_t*)mem;
         s->lent       |= 1u << slot;
      }
      return;
   }
   if (     (mem   = video_driver_texture_lend(s->handle, 0, pitch))
         && (spare = video_driver_texture_lend(s->handle, 1, pitch)))
   {
      s->slots[0]   = (uint32_t*)mem;
      s->lent_spare = (uint32_t*)spare;
      s->lent       = 3;
      s->lent_cur   = 0;
   }
}

static enum gfx_surface_submit_result gfx_surface_upload_sync(
      gfx_surface_t *s, uint8_t fmt)
{
   uintptr_t new_handle = 0;

   if (s->handle && s->fmt == fmt && s->can_update)
   {
      if (video_driver_texture_update(s->handle, &s->img))
         return GFX_SURFACE_SUBMIT_DONE;
      s->can_update = 0;
   }

   if (!video_driver_texture_load(&s->img, s->filter, &new_handle)
         || !new_handle)
      return GFX_SURFACE_SUBMIT_FAILED;
   /* The new texture has read the frame, wherever it lay; the memory
    * lent from the old one goes with it. */
   gfx_surface_unlend(s);
   if (s->handle)
      video_driver_texture_unload(&s->handle);
   s->handle = new_handle;
   s->fmt    = fmt;
   return GFX_SURFACE_SUBMIT_DONE;
}

#ifdef HAVE_THREADS
/* Main thread, from video_thread_async_poll(): the video thread is
 * done with the slot. A load brought a texture (0: nothing, the
 * previous one stays and the format is forgotten so the next submit
 * loads again); an update brought the handle back (0: the driver
 * refused, so from now on this surface loads replacements). The
 * payload goes first and unconditionally: it is the surface's
 * whether or not anyone is still there to be told. */
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
         s->fmt    = GFX_SURFACE_FMT_NONE;
   }
   else if (!handle)
      s->can_update = 0;

   if (s->payload_free)
   {
      gfx_surface_payload_free_t payload_free = s->payload_free;
      void *payload                           = s->payload;
      s->payload_free = NULL;
      s->payload      = NULL;
      payload_free(payload);
   }

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
      gfx_surface_t *s, unsigned slot, uint8_t fmt)
{
#ifdef HAVE_THREADS
   if (video_driver_thread_wrapper_active())
   {
      bool need_load = !s->handle || s->fmt != fmt || !s->can_update;

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
            s->fmt        = fmt;
         return GFX_SURFACE_SUBMIT_QUEUED;
      }
      /* Refused: the wrapper is going away, or this is the video
       * thread. The driver runs the call in place. */
   }
#endif
   return gfx_surface_upload_sync(s, fmt);
}

static void gfx_surface_count(enum gfx_surface_submit_result r)
{
   GFX_INSTR_INC(r == GFX_SURFACE_SUBMIT_QUEUED
         ? GFX_INSTR_SUBMIT_QUEUED
         : (r == GFX_SURFACE_SUBMIT_DONE
            ? GFX_INSTR_SUBMIT_DONE : GFX_INSTR_SUBMIT_FAILED));
}

enum gfx_surface_submit_result gfx_surface_submit(gfx_surface_t *s,
      unsigned slot, bool rgba)
{
   enum gfx_surface_submit_result r;
   uint8_t fmt;

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

   r = gfx_surface_prepare(s, s->slots[slot], s->pixfmt, rgba, true, &fmt)
      ? gfx_surface_submit_img(s, slot, fmt)
      : GFX_SURFACE_SUBMIT_FAILED;
   /* Taken in place (direct video): the slot's next frame can go
    * where the driver uploads from. */
   if (r == GFX_SURFACE_SUBMIT_DONE)
      gfx_surface_lend(s, slot);
   gfx_surface_count(r);
   return r;
}

enum gfx_surface_submit_result gfx_surface_submit_pixels(gfx_surface_t *s,
      const void *pixels, bool rgba)
{
   enum gfx_surface_submit_result r;
   uint8_t fmt;

   if (!s || !pixels || !s->num_slots)
   {
      GFX_INSTR_INC(GFX_INSTR_SUBMIT_FAILED);
      return GFX_SURFACE_SUBMIT_FAILED;
   }
   if (s->inflight)
   {
      GFX_INSTR_INC(GFX_INSTR_SUBMIT_BUSY);
      return GFX_SURFACE_SUBMIT_BUSY;
   }

   /* The caller's buffer does not outlive this call for the video
    * thread's purposes, and is not the surface's to narrow; a slot is
    * both. One copy, the size of a frame, against a wait of up to a
    * present. */
   /* With a slot lent, the driver's copy path may have no slot of its
    * own left, so the frame goes through slot 0 like the rest. */
   if (     gfx_surface_must_narrow(s->pixfmt)
         || s->lent
#ifdef HAVE_THREADS
         || video_driver_thread_wrapper_active()
#endif
      )
   {
      if (!gfx_surface_slot_writable(s, 0))
      {
         GFX_INSTR_INC(GFX_INSTR_SUBMIT_BUSY);
         return GFX_SURFACE_SUBMIT_BUSY;
      }
      GFX_INSTR_INC(GFX_INSTR_SUBMIT_COPY);
      memcpy(s->slots[0], pixels,
            VIDEO_SCALE_AREA(s->dims) * GFX_SURFACE_PIXFMT_BPP(s->pixfmt));
      return gfx_surface_submit(s, 0, rgba);
   }

   r = gfx_surface_prepare(s, pixels, s->pixfmt, rgba, false, &fmt)
      ? gfx_surface_upload_sync(s, fmt)
      : GFX_SURFACE_SUBMIT_FAILED;
   gfx_surface_count(r);
   return r;
}

enum gfx_surface_submit_result gfx_surface_submit_external(gfx_surface_t *s,
      const gfx_surface_src_t *src,
      gfx_surface_release_t release, void *user)
{
   enum gfx_surface_submit_result r;
   uint8_t fmt;

   if (!s || !src || !src->pixels || s->num_slots)
   {
      GFX_INSTR_INC(GFX_INSTR_SUBMIT_FAILED);
      return GFX_SURFACE_SUBMIT_FAILED;
   }
   if (s->inflight)
   {
      GFX_INSTR_INC(GFX_INSTR_SUBMIT_BUSY);
      return GFX_SURFACE_SUBMIT_BUSY;
   }
   if (!gfx_surface_prepare(s, src->pixels, src->pixfmt, src->rgba, false,
            &fmt))
   {
      GFX_INSTR_INC(GFX_INSTR_SUBMIT_FAILED);
      return GFX_SURFACE_SUBMIT_FAILED;
   }

   s->release      = release;
   s->user         = user;
   s->payload      = src->payload;
   s->payload_free = src->payload_free;
   /* inflight_slot is meaningless without slots; release() gets 0
    * and the caller looks at the surface, not the slot. */
   r = gfx_surface_submit_img(s, 0, fmt);
   if (r != GFX_SURFACE_SUBMIT_QUEUED)
   {
      s->payload      = NULL;
      s->payload_free = NULL;
   }
   gfx_surface_count(r);
   return r;
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
   gfx_surface_unlend(s);
   if (s->handle)
      video_driver_texture_unload(&s->handle);
   free(s);
}

bool gfx_surface_slot_writable(gfx_surface_t *s, unsigned slot)
{
   uint32_t *other;
   if (!s || slot >= s->num_slots || !(s->lent & (1u << slot)))
      return true;
   if (s->num_slots >= 2)
      return video_driver_texture_lend_ready(s->handle, slot);
   if (video_driver_texture_lend_ready(s->handle, s->lent_cur))
      return true;
   if (!video_driver_texture_lend_ready(s->handle, s->lent_cur ^ 1u))
      return false;
   /* The other borrowed buffer is free: write that one next. */
   other         = s->lent_spare;
   s->lent_spare = s->slots[0];
   s->slots[0]   = other;
   s->lent_cur  ^= 1u;
   return true;
}

bool gfx_surface_free_adopt(gfx_surface_t *s, void *pixels)
{
   if (s && s->inflight && !s->num_slots && pixels && !s->payload_free)
   {
      GFX_INSTR_INC(GFX_INSTR_SURFACE_FREE);
      s->payload      = pixels;
      s->payload_free = free;
      s->dying        = 1;
      return true;
   }
   gfx_surface_free(s);
   return false;
}

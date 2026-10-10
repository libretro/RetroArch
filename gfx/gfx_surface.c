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

#include <retro_miscellaneous.h>
#include <formats/image_yuv_blit.h>

#include "gfx_surface.h"
#include "gfx_instrument.h"
#include "../tasks/tasks_internal.h"

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
   size_t frame_len, rgb_len = 0, i;
   bool planar = (pixfmt & (IMAGE_PIXFMT_I420 | IMAGE_PIXFMT_NV12)) != 0;

   /* Bounded at four bytes a pixel for every format: the widest
    * single-plane one short of FP32, and what a planar frame is
    * converted into. */
   if (     !VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims)
         || !num_slots || num_slots > GFX_SURFACE_MAX_SLOTS
         || !pixfmt || (pixfmt & (pixfmt - 1))
         || (pixfmt > IMAGE_PIXFMT_GX_RGBA8 && !planar)
         || (size_t)VIDEO_SCALE_W(dims) > ((SIZE_MAX / 4
               - GFX_SURFACE_SLOT_ALIGN) / 4) / VIDEO_SCALE_H(dims))
      return NULL;

   frame_len = (IMAGE_PIXFMT_FRAME_SIZE(pixfmt, (size_t)VIDEO_SCALE_W(dims),
            (size_t)VIDEO_SCALE_H(dims))
         + GFX_SURFACE_SLOT_ALIGN - 1) & ~(size_t)(GFX_SURFACE_SLOT_ALIGN - 1);
   if (planar)
      rgb_len = (VIDEO_SCALE_AREA(dims) * 4
            + GFX_SURFACE_SLOT_ALIGN - 1)
            & ~(size_t)(GFX_SURFACE_SLOT_ALIGN - 1);
   if (frame_len > (SIZE_MAX / 2 - sizeof(*s) - GFX_SURFACE_SLOT_ALIGN)
         / num_slots)
      return NULL;

   if (!(s = (gfx_surface_t*)calloc(1, sizeof(*s)
         + GFX_SURFACE_SLOT_ALIGN + frame_len * num_slots + rgb_len)))
      return NULL;

   base = (uint8_t*)(s + 1);
   base = (uint8_t*)(((uintptr_t)base + GFX_SURFACE_SLOT_ALIGN - 1)
         & ~(uintptr_t)(GFX_SURFACE_SLOT_ALIGN - 1));
   for (i = 0; i < num_slots; i++)
   {
      s->slots[i]     = (uint32_t*)(base + i * frame_len);
      s->own_slots[i] = s->slots[i];
   }
   if (planar)
      s->rgb        = (uint32_t*)(base + num_slots * frame_len);

   s->release    = release;
   s->user       = user;
   s->dims       = dims;
   s->num_slots  = num_slots;
   s->pixfmt     = pixfmt;
   s->filter     = filter;
   s->fmt        = GFX_SURFACE_FMT_NONE;
   s->can_update = video_driver_texture_can_update() ? 1 : 0;
   GFX_INSTR_INC(GFX_INSTR_SURFACE_NEW);
   GFX_INSTR_ADD(GFX_INSTR_SURFACE_BYTES,
         (int)(frame_len * num_slots + rgb_len));
   return s;
}

bool gfx_surface_query_requirements(unsigned width,
      gfx_surface_requirements_t *req)
{
   if (!req)
      return false;
   req->rgba       = (video_driver_get_disp_flags() & VIDEO_FLAG_USE_RGBA)
         ? true : false;
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
   req->formats    = IMAGE_PIXFMT_8888;
   /* The texture path's own answer, not whether the context presents
    * 10-bit core frames (GFX_CTX_FLAGS_SCREEN_10BPC_SOURCE): the two
    * are set by different code and need not agree. */
   if (video_driver_supports_texture_format(TEXTURE_GPU_FORMAT_RGB10A2))
      req->formats |= IMAGE_PIXFMT_2101010;
   if (     video_driver_supports_texture_format(TEXTURE_GPU_FORMAT_RGBA16F)
         && video_driver_supports_texture_format(TEXTURE_GPU_FORMAT_SCRGB))
      req->formats |= IMAGE_PIXFMT_FP16;
   if (req->formats & IMAGE_PIXFMT_FP16)
      req->preferred = IMAGE_PIXFMT_FP16;
   else if (req->formats & IMAGE_PIXFMT_2101010)
      req->preferred = IMAGE_PIXFMT_2101010;
   else
      req->preferred = IMAGE_PIXFMT_8888;
   if ((size_t)width > ((size_t)-1) / IMAGE_PIXFMT_BPP(req->preferred))
      return false;
   req->can_update = video_driver_texture_can_update();
   /* Every upload path in the tree takes tightly packed rows; the
    * alignment is what the GL paths set (glPixelStorei) and what the
    * others are happy with. */
   req->pitch      = (size_t)width * IMAGE_PIXFMT_BPP(req->preferred);
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

gfx_surface_t *gfx_surface_new_still(enum texture_filter_type filter)
{
   gfx_surface_t *s;
   if (!(s = (gfx_surface_t*)calloc(1, sizeof(*s))))
      return NULL;
   s->filter     = filter;
   s->fmt        = GFX_SURFACE_FMT_NONE;
   s->can_update = video_driver_texture_can_update() ? 1 : 0;
   GFX_INSTR_INC(GFX_INSTR_SURFACE_NEW);
   return s;
}

/* A decoded image a still was given, pixels and descriptor */
static void gfx_surface_image_free(void *payload)
{
   struct texture_image *img = (struct texture_image*)payload;
   image_texture_free(img);
   free(img);
}

/* Whether a 2101010 frame has to be narrowed for the driver up. */
static bool gfx_surface_must_narrow(uint32_t pixfmt)
{
   return pixfmt == IMAGE_PIXFMT_2101010
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
      case IMAGE_PIXFMT_8888:
         s->img.pix10   = false;
         break;
      case IMAGE_PIXFMT_FP16:
         /* Half floats have no narrower form here: the driver takes
          * them as they are or the submit fails. */
         if (!video_driver_supports_texture_format(TEXTURE_GPU_FORMAT_RGBA16F))
            return false;
         s->img.pix10   = false;
         s->img.fp16    = true;
         *fmt = 4;
         return true;
      case IMAGE_PIXFMT_2101010:
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
   s->lent_spare  = NULL;
   s->lent        = 0;
   s->lent_cur    = 0;
   s->lend_rec[0] = 0;
   s->lend_rec[1] = 0;
}

/* Whether lent driver slot @dslot may be written: the wrapper's record
 * says under threaded video, the driver under direct */
static bool gfx_surface_lend_ready(const gfx_surface_t *s, unsigned dslot)
{
#ifdef HAVE_THREADS
   if (s->lend_rec[dslot])
      return video_thread_lend_ready((int)s->lend_rec[dslot] - 1);
#endif
   return video_driver_texture_lend_ready(s->handle, dslot);
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
         || !s->handle || !s->can_update
         /* the texture holds the converted frame, not the slot's */
         || s->rgb)
      return;
   pitch = (size_t)VIDEO_SCALE_W(s->dims) * IMAGE_PIXFMT_BPP(s->pixfmt);
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
      enum video_texture_update u = video_driver_texture_update(
            s->handle, &s->img);
      if (u == VIDEO_TEXTURE_UPDATE_DONE)
         return GFX_SURFACE_SUBMIT_DONE;
      /* A still has no next frame to make up for a dropped one: it
       * loads instead. */
      if (u == VIDEO_TEXTURE_UPDATE_DROPPED)
      {
         if (s->num_slots)
            return GFX_SURFACE_SUBMIT_DROPPED;
      }
      else
         s->can_update = 0;
   }

   /* A producer still writing lent memory of the texture about to be
    * replaced keeps that texture alive (see gfx_surface_slot_begin); a
    * second replacement before it comes back is refused rather than
    * tracked, and this frame goes. */
   if (s->lent && s->writing && s->retired_handle)
      return GFX_SURFACE_SUBMIT_FAILED;
   if (!video_driver_texture_load(&s->img, s->filter, &new_handle)
         || !new_handle)
      return GFX_SURFACE_SUBMIT_FAILED;
   /* The new texture has read the frame, wherever it lay; the memory
    * lent from the old one goes with it - once nothing writes it. */
   if (s->lent && s->writing && s->handle)
   {
      s->retired_handle = s->handle;
      s->handle         = 0;
   }
   gfx_surface_unlend(s);
   if (s->handle)
      video_driver_texture_unload(&s->handle);
   s->handle = new_handle;
   s->fmt    = fmt;
   return GFX_SURFACE_SUBMIT_DONE;
}

#ifdef HAVE_THREADS
/* Main thread: what the video thread lent after the update of @slot,
 * taken as gfx_surface_lend takes a direct lend - the same-numbered
 * driver slot for a surface of two, both for a surface of one */
static void gfx_surface_lend_adopt(gfx_surface_t *s, unsigned slot)
{
   const video_thread_async_load_t *n = &s->node;
   if (s->num_slots >= 2)
   {
      if (slot < 2 && n->lent_mem[slot] && n->lent_idx[slot] >= 0)
      {
         s->slots[slot]       = (uint32_t*)n->lent_mem[slot];
         s->lent             |= (uint8_t)(1u << slot);
         s->lend_rec[slot]    = (uint8_t)(n->lent_idx[slot] + 1);
      }
      return;
   }
   if (     n->lent_mem[0] && n->lent_mem[1]
         && n->lent_idx[0] >= 0 && n->lent_idx[1] >= 0)
   {
      s->slots[0]    = (uint32_t*)n->lent_mem[0];
      s->lent_spare  = (uint32_t*)n->lent_mem[1];
      s->lent        = 3;
      s->lent_cur    = 0;
      s->lend_rec[0] = (uint8_t)(n->lent_idx[0] + 1);
      s->lend_rec[1] = (uint8_t)(n->lent_idx[1] + 1);
   }
}

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
   s->dropped       = 0;

   if (s->node.kind == VIDEO_THREAD_ASYNC_LOAD)
   {
      if (handle)
      {
         /* The new texture has read the frame, wherever it lay; the
          * memory lent from the old one goes with it - once nothing
          * writes it (see gfx_surface_upload_sync) */
         if (s->lent && s->writing && s->handle)
         {
            s->retired_handle = s->handle;
            s->handle         = 0;
         }
         gfx_surface_unlend(s);
         if (s->handle)
            video_driver_texture_unload(&s->handle);
         s->handle = handle;
      }
      else
         s->fmt    = GFX_SURFACE_FMT_NONE;
   }
   else if (!handle)
      s->can_update = 0;
   else if (s->node.lend && !s->node.dropped)
      gfx_surface_lend_adopt(s, slot);
   else if (s->node.dropped)
   {
      GFX_INSTR_INC(GFX_INSTR_SUBMIT_DROPPED);
      /* A still has no next frame to make up for it: sent again, its
       * pixels and payload kept until it lands. */
      if (!s->num_slots && !s->dying)
      {
         /* The wrapper let go of the descriptor when it ran the node */
         s->node.img     = &s->img;
         s->node.dropped = 0;
         if (video_thread_async_post(&s->node))
         {
            s->inflight = 1;
            return;
         }
      }
      s->dropped    = 1;
   }

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
      if (s->next_img)
         gfx_surface_image_free(s->next_img);
      s->next_img = NULL;
      /* A decode still out answers to this surface: it frees it */
      if (s->decoding)
         return;
      if (s->handle)
         video_driver_texture_unload(&s->handle);
      free(s);
      return;
   }
   /* An image given while this one was on its way goes up now */
   if (s->next_img)
   {
      struct texture_image *img = s->next_img;
      s->next_img               = NULL;
      gfx_surface_submit_image(s, img);
   }
   /* The last touch: a release may free the surface (gfx_display's
    * texture loads do). dropped is cleared on the next completion. */
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

      /* A second replacement while a producer still writes memory
       * lent from the first replaced texture is refused, as it is
       * under direct video */
      if (need_load && s->lent && s->writing && s->retired_handle)
         return GFX_SURFACE_SUBMIT_FAILED;
      s->node.kind    = need_load
            ? VIDEO_THREAD_ASYNC_LOAD : VIDEO_THREAD_ASYNC_UPDATE;
      /* An update of a streaming texture asks for its upload memory:
       * the same-numbered driver slot for a surface of two, both for a
       * surface of one, until they are lent */
      s->node.lend        = 0;
      s->node.lent_idx[0] = s->node.lent_idx[1] = -1;
      s->node.lent_mem[0] = s->node.lent_mem[1] = NULL;
      if (!need_load && s->num_slots && !s->rgb)
      {
         if (s->num_slots >= 2)
            s->node.lend = (slot < 2 && !(s->lent & (1u << slot)))
               ? (uint8_t)(1u << slot) : 0;
         else if (!s->lent)
            s->node.lend = 3;
      }
      s->node.img     = &s->img;
      s->node.handle  = s->handle;
      s->node.filter  = s->filter;
      s->node.done    = gfx_surface_done;
      s->node.user    = s;
      s->node.release = NULL;
      s->node.dropped = 0;
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

/* A planar frame of @s converted into s->rgb in the order @rgba names,
 * and s->img made its upload. The one pass over the planes. */
static bool gfx_surface_planar(gfx_surface_t *s,
      const gfx_surface_planes_t *p, bool rgba, uint8_t *fmt)
{
   unsigned w     = VIDEO_SCALE_W(s->dims);
   unsigned h     = VIDEO_SCALE_H(s->dims);
   unsigned flags = s->yuv | (rgba ? IMAGE_YUV_FLAG_RGBA : 0);
   if (     !p->planes[0] || !p->planes[1] || !p->planes[2]
         || !p->chroma_step)
      return false;
   image_yuv_420_to_rgb32(s->rgb, w,
         p->planes[0], p->strides[0],
         p->planes[1], p->strides[1],
         p->planes[2], p->strides[2],
         p->chroma_step, w, h, flags);
   return gfx_surface_prepare(s, s->rgb, IMAGE_PIXFMT_8888, rgba, false,
         fmt);
}

/* The planes of a frame laid out as a planar slot is (gfx_surface_new) */
static void gfx_surface_planes_of(const gfx_surface_t *s,
      const void *frame, gfx_surface_planes_t *p)
{
   const uint8_t *y = (const uint8_t*)frame;
   unsigned w       = VIDEO_SCALE_W(s->dims);
   unsigned cw      = (w + 1) / 2;
   size_t   csz     = (size_t)cw * ((VIDEO_SCALE_H(s->dims) + 1) / 2);
   p->planes[0]     = y;
   p->strides[0]    = w;
   p->planes[1]     = y + VIDEO_SCALE_AREA(s->dims);
   if (s->pixfmt == IMAGE_PIXFMT_NV12)
   {
      p->planes[2]   = p->planes[1] + 1;
      p->strides[1]  = p->strides[2] = cw * 2;
      p->chroma_step = 2;
   }
   else
   {
      p->planes[2]   = p->planes[1] + csz;
      p->strides[1]  = p->strides[2] = cw;
      p->chroma_step = 1;
   }
}

static void gfx_surface_count(enum gfx_surface_submit_result r)
{
   GFX_INSTR_INC(r == GFX_SURFACE_SUBMIT_QUEUED
         ? GFX_INSTR_SUBMIT_QUEUED
         : r == GFX_SURFACE_SUBMIT_DONE ? GFX_INSTR_SUBMIT_DONE
         : r == GFX_SURFACE_SUBMIT_DROPPED ? GFX_INSTR_SUBMIT_DROPPED
         : GFX_INSTR_SUBMIT_FAILED);
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

   if (s->rgb)
   {
      gfx_surface_planes_t p;
      gfx_surface_planes_of(s, s->slots[slot], &p);
      r = gfx_surface_planar(s, &p, rgba, &fmt)
         ? gfx_surface_submit_img(s, slot, fmt)
         : GFX_SURFACE_SUBMIT_FAILED;
      gfx_surface_count(r);
      return r;
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

   /* A planar frame is converted where it lies: the conversion is the
    * one copy, and its result is the surface's own. */
   if (s->rgb)
   {
      gfx_surface_planes_t p;
      gfx_surface_planes_of(s, pixels, &p);
      return gfx_surface_submit_planes(s, &p, rgba);
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
            VIDEO_SCALE_AREA(s->dims) * IMAGE_PIXFMT_BPP(s->pixfmt));
      return gfx_surface_submit(s, 0, rgba);
   }

   r = gfx_surface_prepare(s, pixels, s->pixfmt, rgba, false, &fmt)
      ? gfx_surface_upload_sync(s, fmt)
      : GFX_SURFACE_SUBMIT_FAILED;
   gfx_surface_count(r);
   return r;
}

void gfx_surface_set_yuv(gfx_surface_t *s, unsigned flags)
{
   if (s && s->rgb)
      s->yuv = (uint8_t)(flags & (IMAGE_YUV_FLAG_VU
            | IMAGE_YUV_FLAG_FULL_RANGE | IMAGE_YUV_FLAG_BT709));
}

enum gfx_surface_submit_result gfx_surface_submit_planes(gfx_surface_t *s,
      const gfx_surface_planes_t *p, bool rgba)
{
   enum gfx_surface_submit_result r;
   uint8_t fmt;

   if (!s || !s->rgb || !p)
   {
      GFX_INSTR_INC(GFX_INSTR_SUBMIT_FAILED);
      return GFX_SURFACE_SUBMIT_FAILED;
   }
   /* The last converted frame is still on its way up */
   if (s->inflight)
   {
      GFX_INSTR_INC(GFX_INSTR_SUBMIT_BUSY);
      return GFX_SURFACE_SUBMIT_BUSY;
   }
   r = gfx_surface_planar(s, p, rgba, &fmt)
      ? gfx_surface_submit_img(s, 0, fmt)
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
   if (s->inflight || s->decoding)
   {
      /* The video thread still reads the slot and, for a load, will
       * hand back a texture, or the task queue still decodes for it:
       * the completion unloads and frees. */
      s->dying = 1;
      return;
   }
   gfx_surface_unlend(s);
   if (s->next_img)
      gfx_surface_image_free(s->next_img);
   if (s->handle)
      video_driver_texture_unload(&s->handle);
   /* Its producers are gone by now: a retired texture goes too */
   if (s->retired_handle)
      video_driver_texture_unload(&s->retired_handle);
   free(s);
}

bool gfx_surface_submit_image(gfx_surface_t *s, struct texture_image *img)
{
   bool ok;
   uintptr_t tex = 0;

   if (!img)
      return false;
   if (     !s || !img->width || !img->height
         || (!img->pixels && !img->compressed))
   {
      gfx_surface_image_free(img);
      return false;
   }
   if (s->inflight)
   {
      if (s->next_img)
         gfx_surface_image_free(s->next_img);
      s->next_img = img;
      return true;
   }
   /* Another size is another texture: no update in place */
   if (s->dims != VIDEO_SCALE_PACK(img->width, img->height))
      s->fmt = GFX_SURFACE_FMT_NONE;
   s->dims = VIDEO_SCALE_PACK(img->width, img->height);

   /* Fitted here, where the image is ours to rewrite */
   if (!video_driver_texture_fit(img))
   {
      gfx_surface_image_free(img);
      return false;
   }

#ifdef HAVE_THREADS
   if (     img->pixels && !img->compressed
         && VIDEO_SCALE_FITS(img->width, img->height)
         && video_driver_thread_wrapper_active()
         && task_is_on_main_thread())
   {
      gfx_surface_src_t src;
      enum gfx_surface_submit_result r;
      src.pixels       = img->pixels;
      src.payload      = img;
      src.payload_free = gfx_surface_image_free;
      src.pixfmt       = img->fp16  ? IMAGE_PIXFMT_FP16
                       : img->pix10 ? IMAGE_PIXFMT_2101010
                                    : IMAGE_PIXFMT_8888;
      src.rgba         = img->supports_rgba;
      r                = gfx_surface_submit_external(s, &src,
            s->release, s->user);
      if (r == GFX_SURFACE_SUBMIT_QUEUED)
         return true;
      if (r == GFX_SURFACE_SUBMIT_DONE)
      {
         gfx_surface_image_free(img);
         return true;
      }
      /* Refused: the plain load takes it */
   }
#endif
   ok = video_driver_texture_load(img, s->filter, &tex);
   gfx_surface_image_free(img);
   if (!ok || !tex)
      return false;
   if (s->handle)
      video_driver_texture_unload(&s->handle);
   s->handle = tex;
   /* A plain load's texture is not one an in-place update knows */
   s->fmt    = GFX_SURFACE_FMT_NONE;
   return true;
}

gfx_surface_t *gfx_surface_still(gfx_surface_t **slot,
      enum texture_filter_type filter)
{
   if (!*slot)
      *slot = gfx_surface_new_still(filter);
   else
      (*slot)->filter = filter;
   return *slot;
}

void gfx_surface_image_request(image_texture_request_t *req,
      bool rgba, unsigned flags)
{
   gfx_surface_requirements_t want;
   uint32_t formats = gfx_surface_query_requirements(0, &want)
      ? want.formats : IMAGE_PIXFMT_8888;
   req->rgba            = rgba;
   req->want_10bit      = (formats & IMAGE_PIXFMT_2101010) ? true : false;
   req->want_fp16       = (flags & GFX_SURFACE_REQ_HDR)
      && (formats & IMAGE_PIXFMT_FP16);
   req->want_compressed = (flags & GFX_SURFACE_REQ_COMPRESSED) ? true : false;
}

bool gfx_surface_submit_buffer(gfx_surface_t *s,
      enum image_type_enum type, const void *buf, size_t len,
      bool supports_rgba)
{
   image_texture_request_t req;
   struct texture_image *img;
   if (!s || !buf || !len)
      return false;
   if (!(img = (struct texture_image*)calloc(1, sizeof(*img))))
      return false;
   gfx_surface_image_request(&req, supports_rgba, GFX_SURFACE_REQ_COMPRESSED);
   if (!image_texture_load_buffer_request(img, type, buf, len, &req,
            NULL, NULL))
   {
      free(img);
      return false;
   }
   return gfx_surface_submit_image(s, img);
}

bool gfx_surface_take_image(gfx_surface_t *s, struct texture_image *img)
{
   struct texture_image *own;
   if (!s || !img)
      return false;
   if (!(own = (struct texture_image*)malloc(sizeof(*own))))
      return false;
   *own            = *img;
   img->pixels     = NULL;
   img->compressed = NULL;
   return gfx_surface_submit_image(s, own);
}

bool gfx_surface_submit_file(gfx_surface_t *s, const char *path,
      bool supports_rgba)
{
   image_texture_request_t req;
   struct texture_image *img;
   if (!s || !path || !*path)
      return false;
   if (!(img = (struct texture_image*)calloc(1, sizeof(*img))))
      return false;
   gfx_surface_image_request(&req, supports_rgba, GFX_SURFACE_REQ_COMPRESSED);
   if (!image_texture_load_request(img, path, &req, NULL, NULL))
   {
      free(img);
      return false;
   }
   return gfx_surface_submit_image(s, img);
}

/* At most this many decodes in hand before they go up, whatever
 * the set's size: a theme's icons are small, but there are many */
#define GFX_SURFACE_DECODE_BATCH 32

unsigned gfx_surface_submit_files(gfx_surface_t *const *slots,
      const char *const *paths, unsigned n, bool supports_rgba)
{
   struct texture_image imgs[GFX_SURFACE_DECODE_BATCH];
   image_texture_request_t req;
   unsigned done = 0, first;

   if (!slots || !paths)
      return 0;

   gfx_surface_image_request(&req, supports_rgba, GFX_SURFACE_REQ_COMPRESSED);

   for (first = 0; first < n; first += GFX_SURFACE_DECODE_BATCH)
   {
      unsigned i;
      unsigned count = n - first;
      if (count > GFX_SURFACE_DECODE_BATCH)
         count = GFX_SURFACE_DECODE_BATCH;
      image_texture_load_set(paths + first, imgs, count, &req);

      /* Up, in order, on this thread */
      for (i = 0; i < count; i++)
      {
         struct texture_image *img;
         if (!imgs[i].pixels && !imgs[i].compressed)
            continue;
         if (     !slots[first + i]
               || !(img = (struct texture_image*)malloc(sizeof(*img))))
         {
            image_texture_free(&imgs[i]);
            continue;
         }
         *img = imgs[i];
         if (gfx_surface_submit_image(slots[first + i], img))
            done++;
      }
   }
   return done;
}

unsigned gfx_surface_submit_named(gfx_surface_t **slots, unsigned n,
      enum texture_filter_type filter, gfx_surface_path_t path, void *ud,
      bool supports_rgba)
{
   char        *buf;
   const char **paths;
   unsigned     i, done;

   if (!slots || !n)
      return 0;
   if (!(buf = (char*)malloc((size_t)n * PATH_MAX_LENGTH)))
      return 0;
   if (!(paths = (const char**)malloc((size_t)n * sizeof(*paths))))
   {
      free(buf);
      return 0;
   }
   for (i = 0; i < n; i++)
   {
      char *at = buf + (size_t)i * PATH_MAX_LENGTH;
      gfx_surface_still(&slots[i], filter);
      *at      = '\0';
      path(i, ud, at, PATH_MAX_LENGTH);
      paths[i] = at;
   }
   done = gfx_surface_submit_files(slots, paths, n, supports_rgba);
   free(paths);
   free(buf);
   return done;
}

/* What the decode is told to answer to: the surface, and which of
 * its submits asked */
typedef struct
{
   gfx_surface_t *s;
   uint32_t gen;
} gfx_surface_decode_t;

/* Main thread, the decode done */
static void gfx_surface_decoded(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   struct texture_image *img = (struct texture_image*)task_data;
   gfx_surface_decode_t *d   = (gfx_surface_decode_t*)user_data;
   gfx_surface_t *s;
   bool stale;
   (void)task;
   (void)error;

   if (!d)
   {
      if (img)
         gfx_surface_image_free(img);
      return;
   }
   s     = d->s;
   stale = d->gen != s->decode_gen;
   free(d);
   if (s->decoding)
      s->decoding--;

   if (s->dying)
   {
      if (img)
         gfx_surface_image_free(img);
      if (!s->decoding && !s->inflight)
      {
         gfx_surface_unlend(s);
         if (s->next_img)
            gfx_surface_image_free(s->next_img);
         if (s->handle)
            video_driver_texture_unload(&s->handle);
         if (s->retired_handle)
            video_driver_texture_unload(&s->retired_handle);
         free(s);
      }
      return;
   }
   if (stale || !img)
   {
      if (img)
         gfx_surface_image_free(img);
      return;
   }
   gfx_surface_submit_image(s, img);
}

bool gfx_surface_submit_path(gfx_surface_t *s, const char *path,
      bool supports_rgba)
{
   gfx_surface_decode_t *d;

   if (!s || !path || !*path || s->decoding == 0xff)
      return false;
   if (!(d = (gfx_surface_decode_t*)malloc(sizeof(*d))))
      return false;
   d->s   = s;
   d->gen = ++s->decode_gen;
   s->decoding++;
   if (!task_push_image_load(path, supports_rgba, 0, 0,
            gfx_surface_decoded, d))
   {
      s->decoding--;
      free(d);
      return false;
   }
   return true;
}

uint32_t *gfx_surface_slot_begin(gfx_surface_t *s, unsigned slot)
{
   if (!s || slot >= s->num_slots)
      return NULL;
   s->writing |= (uint8_t)(1u << slot);
   return s->slots[slot];
}

void gfx_surface_slot_end(gfx_surface_t *s, unsigned slot)
{
   if (!s || slot >= s->num_slots)
      return;
   s->writing &= (uint8_t)~(1u << slot);
   if (!s->writing && s->retired_handle)
      video_driver_texture_unload(&s->retired_handle);
}

bool gfx_surface_slot_writable(gfx_surface_t *s, unsigned slot)
{
   uint32_t *other;
   if (!s || slot >= s->num_slots || !(s->lent & (1u << slot)))
      return true;
   if (s->num_slots >= 2)
      return gfx_surface_lend_ready(s, slot);
   if (gfx_surface_lend_ready(s, s->lent_cur))
      return true;
   if (!gfx_surface_lend_ready(s, s->lent_cur ^ 1u))
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

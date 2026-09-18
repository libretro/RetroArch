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

#ifndef __GFX_SURFACE_H
#define __GFX_SURFACE_H

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <formats/image.h>

#include "video_driver.h"
#ifdef HAVE_THREADS
#include "video_thread_wrapper.h"
#endif

RETRO_BEGIN_DECLS

/* A streaming GPU surface: one persistent texture that a producer
 * refreshes frame after frame, and the pixel slots it produces into.
 *
 * The producer writes a whole frame into a slot and submits it. The
 * first submit creates the texture; every later one of the same size
 * and channel order rewrites it in place through the driver's
 * update_texture, so a playing preview costs one upload a frame and
 * no allocation, texture creation or destruction. Drivers without an
 * in-place path get a replacement texture per frame, which is what
 * every driver used to do.
 *
 * Threading is the video layer's business, not the caller's. Without
 * the threaded wrapper a submit runs the driver directly and returns
 * DONE, the slot free again. With it, the submit hands the slot to
 * the video thread - a descriptor, not a copy of the pixels - and
 * returns QUEUED; the slot is the surface's until release() arrives
 * on the main thread, from a later frame's video_thread_async_poll().
 * One submit is in flight at a time: a second one before the release
 * gets BUSY and the caller keeps its frame for the next poll, which
 * is what a slow present would have shown anyway.
 *
 * The struct is public so producers can address the slots directly;
 * everything else is the surface's. */

#define GFX_SURFACE_MAX_SLOTS 2

enum gfx_surface_submit_result
{
   GFX_SURFACE_SUBMIT_FAILED = 0, /* nothing uploaded, slot is free */
   GFX_SURFACE_SUBMIT_DONE,       /* uploaded, slot is free */
   GFX_SURFACE_SUBMIT_QUEUED,     /* slot held until release() */
   GFX_SURFACE_SUBMIT_BUSY        /* a queued submit is still in
                                     flight; nothing taken */
};

typedef struct gfx_surface gfx_surface_t;

/* Main thread. The slot is the caller's again. s->handle is current:
 * a first submit's texture is installed before this runs. */
typedef void (*gfx_surface_release_t)(void *user, gfx_surface_t *s,
      unsigned slot);

struct gfx_surface
{
#ifdef HAVE_THREADS
   video_thread_async_load_t node; /* the one handoff, embedded */
#endif
   struct texture_image img;       /* the frame in flight */
   gfx_surface_release_t release;
   void *user;
   uint32_t *slots[GFX_SURFACE_MAX_SLOTS];
   /* The texture, 0 until a submit has completed. A replacement load
    * in flight leaves the previous texture here, drawable, until the
    * new one arrives. */
   uintptr_t handle;
   unsigned width;
   unsigned height;
   unsigned num_slots;
   unsigned inflight_slot;
   enum texture_filter_type filter;
   uint8_t inflight;
   uint8_t dying;      /* freed while in flight; the completion frees */
   uint8_t rgba;       /* channel order of the texture; 0xff = none */
   uint8_t can_update; /* driver updates in place */
};

/* A surface with no slots of its own, for an image whose pixels the
 * caller keeps: an overlay asset, a still. Submitted through
 * gfx_surface_submit_external(), which is the only submit it takes.
 * NULL when out of memory. */
gfx_surface_t *gfx_surface_new_static(unsigned width, unsigned height,
      enum texture_filter_type filter);

/* Upload @pixels, which the caller owns and must keep valid until the
 * surface's release() has run (QUEUED) or the call has returned
 * (DONE). For a surface made by gfx_surface_new_static; @rgba is the
 * order the pixels are in. Main thread. */
enum gfx_surface_submit_result gfx_surface_submit_external(gfx_surface_t *s,
      const uint32_t *pixels, bool rgba,
      gfx_surface_release_t release, void *user);

/* A surface of @num_slots frames of @width x @height 32-bit pixels
 * (1..GFX_SURFACE_MAX_SLOTS), one allocation. NULL when out of memory
 * or the arguments are out of range. */
gfx_surface_t *gfx_surface_new(unsigned width, unsigned height,
      unsigned num_slots, enum texture_filter_type filter,
      gfx_surface_release_t release, void *user);

/* Upload the frame in slot @slot. @rgba is the channel order it was
 * written in, which is the order the driver asked for
 * (VIDEO_FLAG_USE_RGBA). Main thread. */
enum gfx_surface_submit_result gfx_surface_submit(gfx_surface_t *s,
      unsigned slot, bool rgba);

/* Upload a frame that lives outside the surface - a decoder's own
 * canvas. Direct video uploads it from where it is; under the
 * wrapper it is copied into a free slot first, since the caller's
 * buffer will not wait for the video thread. Never returns QUEUED
 * with the caller's memory still in use. Main thread. */
enum gfx_surface_submit_result gfx_surface_submit_pixels(gfx_surface_t *s,
      const uint32_t *pixels, bool rgba);

/* Unload the texture and free the surface. A submit in flight keeps
 * the slots alive until it completes, without a release() call. */
void gfx_surface_free(gfx_surface_t *s);

RETRO_END_DECLS

#endif

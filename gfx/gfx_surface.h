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

/* Frees what a submit handed over with its pixels (the decoded image
 * they live in, say). Main thread. */
typedef void (*gfx_surface_payload_free_t)(void *payload);

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
   /* What a QUEUED external submit took ownership of. The completion
    * frees it before release() runs, and frees it just the same when
    * the surface was freed meanwhile and release() never runs - so a
    * cancelled upload cannot strand it. NULL otherwise. */
   gfx_surface_payload_free_t payload_free;
   void *payload;
   /* Each slot is a frame of @pixfmt: VIDEO_SCALE_AREA(dims) times
    * GFX_SURFACE_PIXFMT_BPP(pixfmt) bytes. */
   uint32_t *slots[GFX_SURFACE_MAX_SLOTS];
   /* The surface's own slot memory. Under direct video a slot of a
    * surface of two slots or more, whose texture the driver can
    * stream, may instead be lent upload memory (texture_lend): slots[i]
    * then points there, and own_slots[i] is where it points again once
    * the texture is replaced or freed. */
   uint32_t *own_slots[GFX_SURFACE_MAX_SLOTS];
   /* The texture, 0 until a submit has completed. A replacement load
    * in flight leaves the previous texture here, drawable, until the
    * new one arrives. */
   uintptr_t handle;
   /* Both axes in one word, VIDEO_SCALE_PACK's layout. */
   unsigned dims;
   unsigned num_slots;
   unsigned inflight_slot;
   /* The one gfx_surface_pixfmt bit the slots hold; 0 for a static
    * surface, whose every submit names its own. */
   uint32_t pixfmt;
   enum texture_filter_type filter;
   uint8_t inflight;
   uint8_t dying;      /* freed while in flight; the completion frees */
   /* Format and channel order of the texture, one key: an in-place
    * update only ever writes the layout the texture was made with.
    * 0xff = none. */
   uint8_t fmt;
   uint8_t lent;       /* slots lent driver memory, a bit each */
   uint8_t can_update; /* driver updates in place */
};

/* What the active video driver wants of an image, asked once before
 * it is decoded rather than guessed from a flag at every producer.
 *
 * The answers live in three places today - a display flag for channel
 * order, a context flag for 10-bit sources, a poke for compressed
 * formats - and every producer that cared had to know all three. A
 * decoder asks this instead, and emits what it is told. */
/* Pixel formats a producer may be asked for, as a bit per format so
 * a driver can accept several and a producer can pick the best one it
 * can actually emit. A name here is a promise about the layout, not
 * about any driver supporting it. Ten bits is not the ceiling: the
 * surface sizes slots, pitches and copies from the format
 * (GFX_SURFACE_PIXFMT_BPP), keys its in-place updates on it, and
 * refuses a submit of a format the texture interface has no encoding
 * for yet, so adding one - FP16 scRGB first - is a driver change and
 * a texture_image encoding, not an API change. */
enum gfx_surface_pixfmt
{
   /* 8 bits a channel in a 32-bit word, the order req.rgba names. */
   GFX_SURFACE_PIXFMT_8888     = (1 << 0),
   /* XRGB2101010: 10 bits a channel, bits [29:20]=R [19:10]=G [9:0]=B. */
   GFX_SURFACE_PIXFMT_2101010  = (1 << 1),
   /* Half-float per channel, linear scRGB (1.0 = 80 nits), the format
    * an HDR framebuffer wants without an encode pass. */
   GFX_SURFACE_PIXFMT_FP16     = (1 << 2),
   /* Reserved names for layouts a driver may come to accept; a
    * producer that cannot emit one simply never sets it. */
   GFX_SURFACE_PIXFMT_FP32     = (1 << 3),
   GFX_SURFACE_PIXFMT_565      = (1 << 4),
   GFX_SURFACE_PIXFMT_4444     = (1 << 5),
   /* GX RGBA8, the GameCube/Wii texture layout: 4x4 tiles of 64 bytes,
    * the AR halves of a tile's 16 texels then their GB halves, width
    * and height multiples of 4 (image_texture_tile_gx). A layout as
    * much as a format: the gx driver points the GPU at the pixels
    * where they lie, so it samples nothing linear at all. */
   GFX_SURFACE_PIXFMT_GX_RGBA8 = (1 << 6)
};

/* Bytes a texel of one gfx_surface_pixfmt bit takes. */
#define GFX_SURFACE_PIXFMT_BPP(f) \
   (((f) & GFX_SURFACE_PIXFMT_FP32) ? 16u \
   : ((f) & GFX_SURFACE_PIXFMT_FP16) ? 8u \
   : ((f) & (GFX_SURFACE_PIXFMT_565 | GFX_SURFACE_PIXFMT_4444)) ? 2u : 4u)

/* An upload of pixels the caller keeps (gfx_surface_submit_external). */
typedef struct
{
   /* Tightly packed rows of @pixfmt, read and never written. */
   const void *pixels;
   /* Handed to the surface with a QUEUED submit, freed by
    * @payload_free at its completion whatever became of the surface
    * meanwhile. Any other result leaves it the caller's. May be NULL;
    * @pixels usually live inside it. */
   void *payload;
   gfx_surface_payload_free_t payload_free;
   /* Exactly one gfx_surface_pixfmt bit. */
   uint32_t pixfmt;
   /* Channel order of the 8-bit formats, as gfx_surface_requirements_t
    * names it. */
   bool rgba;
} gfx_surface_src_t;

typedef struct
{
   /* The channel order to emit: true for memory-order R,G,B,A, false
    * for ARGB words. A decoder that can do either should do this one;
    * anything else costs a swizzle pass. Applies to the 8-bit
    * formats; the wider ones have their layout fixed by the format. */
   bool rgba;
   /* Every format the driver samples without the frontend converting
    * first, as gfx_surface_pixfmt bits: GFX_SURFACE_PIXFMT_8888 and
    * whatever the driver adds to it, except on the GameCube/Wii, where
    * it is GFX_SURFACE_PIXFMT_GX_RGBA8 alone. A format is only listed
    * when a submit of it reaches the GPU as that format - a scRGB
    * framebuffer is not by itself a reason to list FP16. */
   uint32_t formats;
   /* The one of @formats worth decoding into when the producer has a
    * choice: the widest the driver takes that its source can fill.
    * A producer that cannot emit it falls back to any other bit in
    * @formats, 8888 at worst. */
   uint32_t preferred;
   /* The driver can replace a texture's contents in place, so a
    * streaming producer keeps one texture rather than loading a
    * replacement per frame. */
   bool can_update;
   /* Row pitch in bytes the upload wants for @width pixels of
    * @preferred, and the alignment the first row should start on.
    * Both are what the current upload paths use; a producer that can
    * honour them saves the repack. */
   size_t pitch;
   unsigned align;
} gfx_surface_requirements_t;

/* Fill @req for an image of @width pixels on the active driver.
 * Safe before any surface exists; with no driver up it answers with
 * the defaults a software path would use. A producer that does not
 * know its width yet may pass 0 and read the capability fields; the
 * layout fields are then 0 as well.
 *
 * Formats: a producer asks whether a bit is in @formats rather than
 * assuming a bit depth, so a driver that starts taking FP16 scRGB, or
 * anything else added to gfx_surface_pixfmt, is picked up by every
 * producer that can emit it without this contract changing again.
 *
 * False when @width has no row that size_t can express - reachable
 * only on a 32-bit size_t, and then only for a width no image has,
 * but this is the contract every producer's layout comes through and
 * it does not get to begin with an unchecked multiply. @req is
 * untouched on false. */
bool gfx_surface_query_requirements(unsigned width,
      gfx_surface_requirements_t *req);

/* The channel order alone, for the many producers that decode 32-bit
 * images and have no other question: true for memory-order R,G,B,A,
 * false for ARGB words. Shorthand for the rgba field of a full query,
 * and the same answer. */
bool gfx_surface_wants_rgba(void);

/* Whether the active driver can sample @fmt as a compressed texture,
 * so a decoder can keep the GPU-native payload instead of expanding
 * it. */
bool gfx_surface_supports_compressed(enum texture_gpu_format fmt);

/* A surface with no slots of its own, for an image whose pixels the
 * caller keeps: an overlay asset, a still. Submitted through
 * gfx_surface_submit_external(), which is the only submit it takes.
 * NULL when out of memory. */
gfx_surface_t *gfx_surface_new_static(unsigned dims,
      enum texture_filter_type filter);

/* Upload @src. For a surface made by gfx_surface_new_static. The
 * pixels must stay valid until the call has returned (DONE, FAILED,
 * BUSY) or, for QUEUED, until the completion: hand them over through
 * @src->payload and the surface frees them then, release() or not.
 * FAILED as well for a format the driver does not sample: the caller,
 * who owns the image and its descriptor, narrows it and submits
 * again (image_texture_narrow_10bit). Main thread. */
enum gfx_surface_submit_result gfx_surface_submit_external(gfx_surface_t *s,
      const gfx_surface_src_t *src,
      gfx_surface_release_t release, void *user);

/* A surface of @num_slots (1..GFX_SURFACE_MAX_SLOTS) frames of @dims
 * (one packed size word) in @pixfmt, one gfx_surface_pixfmt bit, all
 * in one allocation. NULL when out of memory or the arguments are out
 * of range. */
gfx_surface_t *gfx_surface_new(unsigned dims,
      unsigned num_slots, uint32_t pixfmt, enum texture_filter_type filter,
      gfx_surface_release_t release, void *user);

/* Upload the frame in slot @slot. @rgba is the channel order it was
 * written in, which is the order the driver asked for
 * (VIDEO_FLAG_USE_RGBA). Main thread. */
enum gfx_surface_submit_result gfx_surface_submit(gfx_surface_t *s,
      unsigned slot, bool rgba);

/* Upload a frame that lives outside the surface - a decoder's own
 * canvas, in the surface's pixfmt. Direct video uploads it from where
 * it is; under the wrapper, or when the format has to be narrowed,
 * it is copied into slot 0 first, since the caller's buffer will not
 * wait for the video thread and is not the surface's to rewrite.
 * Never returns QUEUED with the caller's memory still in use. FAILED
 * for a static surface, which has no slot to copy into. Main
 * thread. A slot itself is narrowed in place when the driver does not
 * sample the surface's format and the surface knows how. */
enum gfx_surface_submit_result gfx_surface_submit_pixels(gfx_surface_t *s,
      const void *pixels, bool rgba);

/* Whether slot @slot may be written now. A slot lent the driver's upload
 * memory is the GPU's until the copy of its last frame has run; the
 * answer never waits, and a producer that gets false keeps the slot
 * for a later frame. Producers read slots[@slot] afresh each time they
 * hand it to a writer: a successful submit of a slot may lend it. Main
 * thread. */
bool gfx_surface_slot_writable(const gfx_surface_t *s, unsigned slot);

/* Unload the texture and free the surface. A submit in flight keeps
 * the slots alive until it completes, without a release() call; its
 * payload is still freed then. */
void gfx_surface_free(gfx_surface_t *s);

/* gfx_surface_free() for a surface whose submit was given pixels the
 * caller owns (gfx_surface_submit_external, no payload) and is about
 * to free. With that submit still in flight the video thread has yet
 * to read them, so the surface takes @pixels as the payload and frees
 * them at the completion, with itself: true, and the caller must
 * forget the pointer. Otherwise the surface is freed as usual and the
 * pixels stay the caller's: false. @pixels must be a malloc() block. */
bool gfx_surface_free_adopt(gfx_surface_t *s, void *pixels);

RETRO_END_DECLS

#endif

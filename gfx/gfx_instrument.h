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

#ifndef __GFX_INSTRUMENT_H
#define __GFX_INSTRUMENT_H

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_atomic.h>

RETRO_BEGIN_DECLS

/* What the image and surface paths actually cost, counted where it
 * happens rather than argued from the source.
 *
 * Every claim the surface work rests on - a streaming frame allocates
 * nothing, copies nothing and creates no texture; an overlay page
 * switch uploads nothing; the direct path queues nothing - is a
 * statement about these counters. They exist so those statements can
 * be checked, and so a regression in any of them is visible instead
 * of theoretical.
 *
 * Built only under HAVE_GFX_INSTRUMENT: without it every macro below
 * is nothing at all, no counter exists, and no call survives the
 * preprocessor. So the counting itself can never be what a release
 * build pays for.
 *
 * The counters are plain relaxed atomics. Several are incremented on
 * the video thread while the main thread reads them, which relaxed
 * ordering covers: each is a tally, nothing is inferred from the
 * order of two of them, and the reader is the thread that starts and
 * stops a measurement window. Nothing here logs, allocates or waits,
 * so a counted path keeps its timing - the instrumented build is
 * still the build whose numbers mean something. */

enum gfx_instrument_counter
{
   /* Texture lifetime, whichever driver is up */
   GFX_INSTR_TEX_LOAD = 0,      /* video_driver_texture_load        */
   GFX_INSTR_TEX_LOAD_ASYNC,    /* ..._load_async, posted           */
   GFX_INSTR_TEX_UPDATE,        /* ..._texture_update, in place     */
   GFX_INSTR_TEX_UPDATE_REFUSED,/* driver declined an update        */
   GFX_INSTR_TEX_UNLOAD,        /* ..._texture_unload               */

   /* The threaded wrapper's texture edge */
   GFX_INSTR_ASYNC_POST,        /* nodes handed to the video thread */
   GFX_INSTR_ASYNC_POST_ALLOC,  /* ..of which allocated a node      */
   GFX_INSTR_ASYNC_DONE,        /* completions delivered            */
   GFX_INSTR_WRAPPER_CMD,       /* synchronous commands, round trip */

   /* gfx_surface */
   GFX_INSTR_SURFACE_NEW,
   GFX_INSTR_SURFACE_BYTES,     /* slot bytes allocated, cumulative */
   GFX_INSTR_SURFACE_FREE,
   GFX_INSTR_SUBMIT_DONE,       /* direct submit, ran here          */
   GFX_INSTR_SUBMIT_QUEUED,     /* threaded submit, descriptor only */
   GFX_INSTR_SUBMIT_BUSY,       /* dropped: one already in flight   */
   GFX_INSTR_SUBMIT_FAILED,
   GFX_INSTR_SUBMIT_COPY,       /* submit_pixels copied into a slot */

   /* Animated previews, per frame */
   GFX_INSTR_ANIM_FRAME,        /* frames decoded                   */
   GFX_INSTR_ANIM_DIRECT,       /* ..decoded straight into a slot   */
   GFX_INSTR_ANIM_COPY,         /* ..copied out of a canvas         */
   GFX_INSTR_ANIM_SWIZZLE,      /* ..swizzled row by row            */

   /* Overlays */
   GFX_INSTR_OVERLAY_UPLOAD,    /* images uploaded for a pack       */
   /* Decoded overlay pixels still held after upload, in KiB. What a
    * pack costs in system memory once its textures exist, which is
    * the number that decides whether releasing them is worth a
    * re-decode on the next video reinit. KiB, because a full pack set
    * is tens of megabytes and the counters are int. */
   GFX_INSTR_OVERLAY_PIXEL_KIB,
   GFX_INSTR_OVERLAY_PAGE,      /* pages shown                      */
   GFX_INSTR_OVERLAY_PAGE_LOAD, /* ..that went through load()       */
   GFX_INSTR_OVERLAY_DRAW,      /* overlay pages drawn (Vulkan)     */
   GFX_INSTR_OVERLAY_DRAW_ALLOC,/* ..buffer ranges they took       */

   GFX_INSTR_COUNT
};

#ifdef HAVE_GFX_INSTRUMENT

extern retro_atomic_int_t gfx_instrument_counters[GFX_INSTR_COUNT];

#define GFX_INSTR_ADD(c, n) \
   retro_atomic_fetch_add_int(&gfx_instrument_counters[(c)], (int)(n))
#define GFX_INSTR_INC(c) GFX_INSTR_ADD((c), 1)

/* The counter's value now. */
int gfx_instrument_get(enum gfx_instrument_counter c);

/* Start a measurement window: every counter back to zero. */
void gfx_instrument_reset(void);

/* The name a report prints for a counter, for a caller that formats
 * its own table. */
const char *gfx_instrument_name(enum gfx_instrument_counter c);

/* The whole table through @write, one line per counter that is not
 * zero, as "name value". @user is passed through. */
void gfx_instrument_report(void (*write)(void *user, const char *line),
      void *user);

#else

#define GFX_INSTR_ADD(c, n) ((void)0)
#define GFX_INSTR_INC(c)    ((void)0)

#endif

RETRO_END_DECLS

#endif

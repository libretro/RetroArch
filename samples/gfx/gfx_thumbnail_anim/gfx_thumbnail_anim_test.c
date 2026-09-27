/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2026 - Daniel De Matteis
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

/* Regression oracle for the file-browser animated-thumbnail path.
 *
 * The unit under test is the real gfx/gfx_thumbnail.c, compiled from
 * the tree; only the frontend surface (video driver, config, menu,
 * playlist) is stubbed, and the video-driver stub is the observation
 * point: it CRCs every texture upload, so "it animates" is measured as
 * "distinct frames reached the texture", not as any internal flag.
 *
 * Two ways this path has actually broken, each invisible to every
 * other test in the tree, each asserted here:
 *
 *  1. The request must carry the file's path to the upload callback.
 *     gfx_thumbnail_request_file once malloc'd its tag and never set
 *     tag->path, so gfx_thumbnail_anim_open read uninitialised heap -
 *     an empty string or garbage - and returned without touching the
 *     file.  Undefined behaviour, not a stable failure: a recycled
 *     tag often still held the previous entry's valid path, so the
 *     bug appeared and vanished with allocation order and survived a
 *     bisect.  The lane drives anim_open with a garbage path and with
 *     the real one, and requires open-failure and animation
 *     respectively.
 *
 *  2. A windowed animation must never be marked read-pending.
 *     anim_install used !data_transfer_complete() as the pending
 *     test.  A window never completes - done stays clear for its
 *     whole life - and the pump animate() uses declines windows by
 *     design, so a windowed animation deadlocked: pending forced the
 *     pump branch, the pump was a no-op, and animate() returned
 *     before ever advancing a frame, forever.  Every path-based open
 *     (animated WEBP, APNG, WEBM/MP4 with no still stream to adopt -
 *     the whole file-browser preview) froze behind its static
 *     thumbnail.  The lane opens the animation and requires at least
 *     two distinct frame uploads within a bounded number of paced
 *     vsyncs; a pending deadlock times out at zero.
 *
 * The animation is a 188-byte 3-frame lossless WEBP embedded below,
 * written to a temp file at startup, so the test needs no fixtures.
 *
 * Build:  make                (SANITIZER=address,undefined, or thread)
 *         make sweep          (all three passes)
 * The Makefile compiles gfx_thumbnail.c with -O0 -fno-inline and
 * globalizes the static gfx_thumbnail_anim_open via objcopy so the
 * test can call it directly, upstream of the task queue.
 */

#include <features/features_cpu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <boolean.h>
#include "gfx/gfx_thumbnail.h"
#include "gfx/gfx_surface.h"
#include "gfx/gfx_instrument.h"

static const unsigned char anim_webp[] = {
   0x52, 0x49, 0x46, 0x46, 0xb4, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50,
   0x56, 0x50, 0x38, 0x58, 0x0a, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
   0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x41, 0x4e, 0x49, 0x4d, 0x06, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x4e, 0x4d, 0x46,
   0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00,
   0x00, 0x03, 0x00, 0x00, 0x32, 0x00, 0x00, 0x02, 0x56, 0x50, 0x38, 0x4c,
   0x0f, 0x00, 0x00, 0x00, 0x2f, 0x03, 0xc0, 0x00, 0x00, 0x07, 0x10, 0xfd,
   0x8f, 0xfe, 0x07, 0x22, 0xa2, 0xff, 0x01, 0x00, 0x41, 0x4e, 0x4d, 0x46,
   0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00,
   0x00, 0x03, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x56, 0x50, 0x38, 0x4c,
   0x0f, 0x00, 0x00, 0x00, 0x2f, 0x03, 0xc0, 0x00, 0x00, 0x07, 0x10, 0xd1,
   0xff, 0xfe, 0x07, 0x22, 0xa2, 0xff, 0x01, 0x00, 0x41, 0x4e, 0x4d, 0x46,
   0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00,
   0x00, 0x03, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x56, 0x50, 0x38, 0x4c,
   0x0f, 0x00, 0x00, 0x00, 0x2f, 0x03, 0xc0, 0x00, 0x00, 0x07, 0xd0, 0xff,
   0x88, 0xfe, 0x07, 0x22, 0xa2, 0xff, 0x01, 0x00
};

int      gt_uploads;
unsigned gt_last_crc;
extern int gt_async_mode, gt_async_posted, gt_async_pending;
extern int gt_can_update, gt_updates;
void gt_async_flush(void);

/* Whether the thumbnail's animation surface has a frame on its way to
 * the video thread. */
static int anim_inflight(const gfx_thumbnail_t *th)
{
   const gfx_surface_t *s = (const gfx_surface_t*)th->anim_surface;
   return s ? s->inflight : 0;
}

void gfx_thumbnail_anim_open(gfx_thumbnail_t *t, const char *path);

static void reset_thumb(gfx_thumbnail_t *t)
{
   memset(t, 0, sizeof(*t));
   /* what gfx_thumbnail_handle_upload leaves behind just before it
    * reaches the animation block */
   t->status  = GFX_THUMBNAIL_STATUS_AVAILABLE;
   t->dims    = VIDEO_SCALE_PACK(4, 4);
   t->texture = 1;
}

int main(void)
{
   gfx_thumbnail_t th;
   char path[256];
   int i, bad = 0;

   /* materialise the embedded animation */
   snprintf(path, sizeof(path), "/tmp/gfx_thumb_anim_%d.webp",
         (int)getpid());
   {
      FILE *f = fopen(path, "wb");
      if (!f) return 0;             /* cannot test here; do not fail */
      fwrite(anim_webp, 1, sizeof(anim_webp), f);
      fclose(f);
   }

   /* 1. a garbage path must not install an animation - this is what
    *    request_file used to pass, when the heap was unkind */
   reset_thumb(&th);
   {
      char garbage[64];
      for (i = 0; i < 63; i++)
         garbage[i] = (char)(0x41 + (i * 7) % 26);
      garbage[63] = 0;
      gfx_thumbnail_anim_open(&th, garbage);
   }
   if (th.anim || (th.flags & GFX_THUMB_FLAG_ANIM_ACTIVE))
   {
      printf("[FAIL] a nonexistent path installed an animation\n");
      bad = 1;
   }
   else
      printf("[ok]   garbage path: no stream installed\n");
   gfx_thumbnail_reset(&th);

   /* 2. the real path must install AND ADVANCE.  Bounded paced
    *    vsyncs: the read-pending deadlock scores zero uploads here
    *    and fails on the timeout, it does not hang the suite. */
   reset_thumb(&th);
   gt_uploads = 0;
   gt_last_crc = 0;
   gfx_thumbnail_anim_open(&th, path);
   if (!th.anim)
   {
      printf("[FAIL] the animation never installed (anim=NULL)\n");
      bad = 1;
   }
   else
   {
      if (th.anim_read_pending && th.anim_windowed)
      {
         /* the exact deadlock, named before the timeout proves it */
         printf("[FAIL] a windowed animation is marked read-pending: "
                "animate() will never advance it\n");
         bad = 1;
      }
      /* The install must reflect the real mapping: a reservation build
       * is windowed here (WebP is small, but the open reserves). */
      if (!th.anim_windowed)
      {
         printf("[FAIL] a reserved open produced a non-windowed thumbnail\n");
         bad = 1;
      }
      for (i = 0; i < 240 && gt_uploads < 3; i++)
      {
         gfx_thumbnail_animate(&th, cpu_features_get_time_usec());
         usleep(16666);
      }
      if (gt_uploads >= 2)
         printf("[ok]   real path: %d distinct frames uploaded in %d "
                "paced vsyncs\n", gt_uploads, i);
      else
      {
         printf("[FAIL] animation installed but never advanced "
                "(%d uploads in %d vsyncs)\n", gt_uploads, i);
         bad = 1;
      }
   }
   gfx_thumbnail_reset(&th);

   /* 3. threaded video: frames leave through the asynchronous upload
    *    and the decoder's buffer is copied, one frame in flight at a
    *    time. Nothing reaches the driver until delivery; a frame that
    *    is decoded while one is travelling is skipped, not queued; and
    *    a reset while one is in flight discards it on delivery. */
   reset_thumb(&th);
   gt_uploads = 0;
   gt_last_crc = 0;
   gt_async_mode = 1;
   gt_async_posted = gt_async_pending = 0;
   gfx_thumbnail_anim_open(&th, path);
   if (!th.anim)
   {
      printf("[FAIL] async: the animation never installed\n");
      bad = 1;
   }
   else
   {
      int posted_before_flush;
      for (i = 0; i < 120 && gt_async_posted < 1; i++)
      {
         gfx_thumbnail_animate(&th, cpu_features_get_time_usec());
         usleep(16666);
      }
      /* keep animating without delivering: nothing more may be posted */
      for (i = 0; i < 20; i++)
      {
         gfx_thumbnail_animate(&th, cpu_features_get_time_usec());
         usleep(16666);
      }
      posted_before_flush = gt_async_posted;
      if (posted_before_flush != 1 || gt_uploads != 0 || !anim_inflight(&th))
      {
         printf("[FAIL] async: %d posted, %d uploaded, inflight=%d before "
                "delivery (want 1, 0, 1)\n", posted_before_flush,
                gt_uploads, anim_inflight(&th));
         bad = 1;
      }
      gt_async_flush();
      if (gt_uploads != 1 || anim_inflight(&th) || th.texture != 2)
      {
         printf("[FAIL] async: after delivery %d uploaded, inflight=%d, "
                "texture=%lu\n", gt_uploads, anim_inflight(&th),
                (unsigned long)th.texture);
         bad = 1;
      }
      /* the next frame may travel now */
      for (i = 0; i < 120 && gt_async_posted < 2; i++)
      {
         gfx_thumbnail_animate(&th, cpu_features_get_time_usec());
         usleep(16666);
      }
      if (gt_async_posted != 2)
      {
         printf("[FAIL] async: second frame never posted after delivery\n");
         bad = 1;
      }
      /* reset with a frame in flight: delivery must not touch it */
      gfx_thumbnail_reset(&th);
      gt_async_flush();
      if (th.texture != 0 || anim_inflight(&th) || gt_async_pending != 0)
      {
         printf("[FAIL] async: delivery after reset installed texture=%lu "
                "inflight=%d pending=%d\n", (unsigned long)th.texture,
                anim_inflight(&th), gt_async_pending);
         bad = 1;
      }
      if (!bad)
         printf("[ok]   async: one frame in flight, delivered on flush, "
                "discarded after reset\n");
   }
   gt_async_mode = 0;

   /* 4. what a played animation costs, counted where it happens
    *    rather than argued from the source (Phase 0 of the surface
    *    plan): with threaded video the steady state must allocate no
    *    slot per frame, copy no frame out of a canvas, create and
    *    destroy no texture, and post one descriptor a frame. The
    *    numbers are printed either way; the check is on the ones the
    *    plan states as budgets. */
   {
      int frames, direct, copies, loads, unloads, updates, posts, allocs;
      int news, submits;

      reset_thumb(&th);
      gt_async_mode   = 1;
      gt_async_posted = gt_async_pending = 0;
      gfx_instrument_reset();
      gfx_thumbnail_anim_open(&th, path);
      for (i = 0; i < 240 && gfx_instrument_get(GFX_INSTR_ANIM_FRAME) < 12; i++)
      {
         gfx_thumbnail_animate(&th, cpu_features_get_time_usec());
         gt_async_flush();
         usleep(16666);
      }
      frames  = gfx_instrument_get(GFX_INSTR_ANIM_FRAME);
      direct  = gfx_instrument_get(GFX_INSTR_ANIM_DIRECT);
      copies  = gfx_instrument_get(GFX_INSTR_ANIM_COPY);
      loads   = gfx_instrument_get(GFX_INSTR_TEX_LOAD);
      unloads = gfx_instrument_get(GFX_INSTR_TEX_UNLOAD);
      updates = gfx_instrument_get(GFX_INSTR_TEX_UPDATE);
      posts   = gfx_instrument_get(GFX_INSTR_ASYNC_POST);
      allocs  = gfx_instrument_get(GFX_INSTR_ASYNC_POST_ALLOC);
      news    = gfx_instrument_get(GFX_INSTR_SURFACE_NEW);
      submits = gfx_instrument_get(GFX_INSTR_SUBMIT_QUEUED)
              + gfx_instrument_get(GFX_INSTR_SUBMIT_DONE);

      printf("[baseline] %d frames: %d direct, %d canvas copies, "
             "%d loads, %d updates, %d unloads, %d posts (%d allocated), "
             "%d surfaces, %d submits\n",
             frames, direct, copies, loads, updates, unloads,
             posts, allocs, news, submits);

      if (frames < 12)
      {
         printf("[FAIL] baseline: only %d frames decoded\n", frames);
         bad = 1;
      }
      /* One surface for the animation, not one per frame. */
      if (news != 1)
      {
         printf("[FAIL] baseline: %d surfaces for one animation\n", news);
         bad = 1;
      }
      /* Threaded posts must carry a descriptor, never allocate. */
      if (allocs != 0)
      {
         printf("[FAIL] baseline: %d of %d posts allocated a node\n",
                allocs, posts);
         bad = 1;
      }
      /* WEBP composes on a canvas, so a copy a frame is expected here
       * and the direct count is zero; what must not happen is both. */
      if (direct && copies)
      {
         printf("[FAIL] baseline: %d direct and %d copied frames\n",
                direct, copies);
         bad = 1;
      }
      /* The texture is made once and updated after that; with a driver
       * that cannot update, a load and an unload a frame is the
       * fallback and the stub here is that driver. */
      if (updates && loads > 1)
      {
         printf("[FAIL] baseline: %d loads beside %d in-place updates\n",
                loads, updates);
         bad = 1;
      }
      if (!bad)
         printf("[ok]   baseline: one surface, %d posts with 0 allocations\n",
                posts);

      gfx_thumbnail_reset(&th);
      gt_async_flush();
      gt_async_mode = 0;
   }

   /* 5. the surface's two routes to the driver. With in-place updates
    *    every frame after the first is an update of the one texture;
    *    without them (a driver with no update path) each frame is a
    *    replacement load and the previous texture is unloaded once
    *    the new one is in. Either way the frames all arrive. */
   {
      int route;
      for (route = 0; route < 2 && !bad; route++)
      {
         reset_thumb(&th);
         gt_can_update   = route == 0;
         gt_uploads      = 0;
         gt_last_crc     = 0;
         gt_updates      = 0;
         gt_async_mode   = 1;
         gt_async_posted = gt_async_pending = 0;
         gfx_thumbnail_anim_open(&th, path);
         for (i = 0; i < 240 && gt_uploads < 3; i++)
         {
            gfx_thumbnail_animate(&th, cpu_features_get_time_usec());
            gt_async_flush();
            usleep(16666);
         }
         if (gt_uploads < 3 || th.texture != 2
               || (route == 0 && gt_updates < 2)
               || (route == 1 && gt_updates != 0))
         {
            printf("[FAIL] route %s: %d distinct frames, %d updates, "
                   "texture=%lu\n", route == 0 ? "update" : "reload",
                   gt_uploads, gt_updates, (unsigned long)th.texture);
            bad = 1;
         }
         else
            printf("[ok]   route %s: %d distinct frames, %d in-place "
                   "updates\n", route == 0 ? "update" : "reload",
                   gt_uploads, gt_updates);
         gfx_thumbnail_reset(&th);
         gt_async_flush();
         gt_async_mode = 0;
      }
      gt_can_update = 1;
   }

   remove(path);
   printf("%s\n", bad ? "FAILED" : "PASS");
   return bad;
}

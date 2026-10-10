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
#include <retro_timers.h>
#include <boolean.h>
#include <queues/task_queue.h>
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
extern int gt_can_update, gt_updates, gt_drop_updates, gt_drop_retried;
extern int gt_lend_mode, gt_lends, gt_lend_violations, gt_lent_uploads,
       gt_lend_stale;
extern void gt_lend_reset(void);
extern int  gt_lend_owned, gt_update_fail, gt_lend_freed, gt_lend_refuse;
void gt_async_flush(void);
int gt_surface_outcome_test(void);

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
    * reaches the animation block: a still shown, its texture the
    * surface's */
   t->status  = GFX_THUMBNAIL_STATUS_AVAILABLE;
   t->dims    = VIDEO_SCALE_PACK(4, 4);
}

/* Lane 14: the still load's callback, captured by the task stub */
extern void (*gt_still_cb)(void *task, void *data, void *user,
      const char *err);
extern void *gt_still_ud;
extern int   gt_still_capture;

/* A decoded still as the load task would hand it over: 4x4, @seed
 * telling one from another for the upload counter. */
static struct texture_image *gt_still_image(uint32_t seed, int fp16)
{
   struct texture_image *img = (struct texture_image*)
         calloc(1, sizeof(*img));
   unsigned w;
   if (!img)
      return NULL;
   img->width  = 4;
   img->height = 4;
   img->fp16   = fp16 ? true : false;
   if (!(img->pixels = (uint32_t*)malloc(16 * sizeof(uint32_t)
         * (fp16 ? 2 : 1))))
   {
      free(img);
      return NULL;
   }
   for (w = 0; w < 16 * (fp16 ? 2u : 1u); w++)
      img->pixels[w] = seed + w * 2654435761u;
   return img;
}

/* The still path end to end on the real gfx_thumbnail.c: a request
 * whose task is stubbed, its callback run with an image here. */
static int gt_still_request(const char *path, gfx_thumbnail_t *th,
      uint32_t seed, int fp16)
{
   struct texture_image *img;
   gt_still_cb = NULL;
   gfx_thumbnail_request_file(path, th, 0);
   if (     !gt_still_cb
         || (int)th->status != GFX_THUMBNAIL_STATUS_PENDING)
      return 0;
   if (!(img = gt_still_image(seed, fp16)))
      return 0;
   gt_still_cb(NULL, img, gt_still_ud, NULL);
   return 1;
}

/* Lane 13: a release that frees its surface, as gfx_display's texture
 * loads do; after it the completion may not touch the surface. */
static int gt_free_next, gt_freed_in_release;
static void gt_release_frees(void *user, gfx_surface_t *s, unsigned slot)
{
   (void)user;
   (void)slot;
   if (!gt_free_next)
      return;
   gt_free_next = 0;
   gt_freed_in_release++;
   gfx_surface_free(s);
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
         retro_sleep(17);
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
         retro_sleep(17);
      }
      /* keep animating without delivering: nothing more may be posted */
      for (i = 0; i < 20; i++)
      {
         gfx_thumbnail_animate(&th, cpu_features_get_time_usec());
         retro_sleep(17);
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
         retro_sleep(17);
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
      int frames, direct, copies, loads, unloads, updates, posts;
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
         retro_sleep(17);
      }
      frames  = gfx_instrument_get(GFX_INSTR_ANIM_FRAME);
      direct  = gfx_instrument_get(GFX_INSTR_ANIM_DIRECT);
      copies  = gfx_instrument_get(GFX_INSTR_ANIM_COPY);
      loads   = gfx_instrument_get(GFX_INSTR_TEX_LOAD);
      unloads = gfx_instrument_get(GFX_INSTR_TEX_UNLOAD);
      updates = gfx_instrument_get(GFX_INSTR_TEX_UPDATE);
      posts   = gfx_instrument_get(GFX_INSTR_ASYNC_POST);
      news    = gfx_instrument_get(GFX_INSTR_SURFACE_NEW);
      submits = gfx_instrument_get(GFX_INSTR_SUBMIT_QUEUED)
              + gfx_instrument_get(GFX_INSTR_SUBMIT_DONE);

      printf("[baseline] %d frames: %d direct, %d canvas copies, "
             "%d loads, %d updates, %d unloads, %d posts, "
             "%d surfaces, %d submits\n",
             frames, direct, copies, loads, updates, unloads,
             posts, news, submits);

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
         printf("[ok]   baseline: one surface, %d posts\n",
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
            retro_sleep(17);
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

   /* 6. thumbnails closed while their jobs still wait for the worker.
    *    A waiting job cannot be pulled out from under the worker, so
    *    it is cancelled and its block kept until the worker has let it
    *    go: freed any sooner, the worker walks into freed memory. The
    *    worker must pass every one of them over and still serve the
    *    thumbnail that comes after. */
   {
      static gfx_thumbnail_t many[24];
      int r, k, opened = 0;
      for (r = 0; r < 20; r++)
      {
         for (k = 0; k < 24; k++)
         {
            reset_thumb(&many[k]);
            gfx_thumbnail_anim_open(&many[k], path);
            if (many[k].anim)
               opened++;
            gfx_thumbnail_animate(&many[k], cpu_features_get_time_usec());
         }
         for (k = 0; k < 24; k++)
            gfx_thumbnail_reset(&many[k]);
      }
      reset_thumb(&th);
      gt_uploads  = 0;
      gt_last_crc = 0;
      gfx_thumbnail_anim_open(&th, path);
      for (i = 0; i < 240 && gt_uploads < 3; i++)
      {
         gfx_thumbnail_animate(&th, cpu_features_get_time_usec());
         retro_sleep(17);
      }
      if (opened && gt_uploads >= 2)
         printf("[ok]   %d animations closed with a job waiting; the "
                "next one still animates\n", opened);
      else
      {
         printf("[FAIL] after %d animations closed with a job waiting, "
                "the next uploaded %d frames\n", opened, gt_uploads);
         bad = 1;
      }
      gfx_thumbnail_reset(&th);
   }
   /* 7. direct video lends the job pipeline's slots the driver's
    *    upload memory: jobs decode straight into it, are handed a slot
    *    only once the GPU is done with it, and every upload from it
    *    carries a frame a job wrote there. */
   reset_thumb(&th);
   gt_async_mode = 0;
   gt_can_update = 1;
   gt_lend_mode  = 1;
   gt_lends = gt_lend_violations = gt_lent_uploads = gt_lend_stale = 0;
   gt_uploads    = 0;
   gt_last_crc   = 0;
   gfx_thumbnail_anim_open(&th, path);
   for (i = 0; i < 480 && gt_lent_uploads < 6; i++)
   {
      gfx_thumbnail_animate(&th, cpu_features_get_time_usec());
      retro_sleep(17);
   }
   if (     th.anim && gt_lends >= 2 && gt_lent_uploads >= 6
         && !gt_lend_violations && !gt_lend_stale)
      printf("[ok]   lent slots: %d lends, %d uploads from lent memory, "
             "none written early or stale\n", gt_lends, gt_lent_uploads);
   else
   {
      printf("[FAIL] lent slots: %d lends, %d lent uploads, %d written "
             "while on the GPU, %d stale\n", gt_lends, gt_lent_uploads,
             gt_lend_violations, gt_lend_stale);
      bad = 1;
   }
   gfx_thumbnail_reset(&th);
   gt_lend_mode = 0;
   gt_lend_reset();

   /* 8. a texture owns the memory it lends, and a producer holding a
    *    slot keeps it valid: with both slots lent, slot 0 is taken as a
    *    job takes it, an in-place update is refused so the texture is
    *    replaced, and the taken memory is written after - the old
    *    texture must survive until the slot comes back, then go.
    *    Unloading it at the replacement would free memory still being
    *    written, which AddressSanitizer reports. */
   {
      gfx_surface_t *s8;
      uint32_t      *held;
      unsigned       k, n8 = 64u * 48u;
      int            freed_at_replace;
      gt_async_mode  = 0;
      gt_can_update  = 1;
      gt_lend_mode   = 1;
      gt_lend_owned  = 1;
      gt_lend_freed  = 0;
      gt_update_fail = 0;
      s8 = gfx_surface_new(VIDEO_SCALE_PACK(64, 48), 2,
            GFX_SURFACE_PIXFMT_8888, TEXTURE_FILTER_LINEAR, NULL, NULL);
      if (!s8)
      {
         printf("[FAIL] lane 8: no surface\n");
         bad = 1;
      }
      else
      {
         for (k = 0; k < 4; k++) /* load, update, then each slot lent */
         {
            unsigned slot = k & 1, w;
            for (w = 0; w < n8; w++)
               s8->slots[slot][w] = 0xff000000u | (k * 0x10101u + w);
            gfx_surface_submit(s8, slot, false);
         }
         held = gfx_surface_slot_begin(s8, 0);
         gt_update_fail = 1;
         gfx_surface_submit(s8, 1, false);
         freed_at_replace = gt_lend_freed;
         for (k = 0; k < n8; k++)
            held[k] = 0xff123456u; /* the job, still writing */
         gfx_surface_slot_end(s8, 0);
         if (     s8->lent == 0 && gt_update_fail == 0
               && freed_at_replace == 0 && gt_lend_freed >= 1
               && !s8->retired_handle)
            printf("[ok]   a texture replaced while a slot it lent was "
                   "taken kept that memory until the slot came back "
                   "(%d buffers freed then)\n", gt_lend_freed);
         else
         {
            printf("[FAIL] replacement under a taken lent slot: lent %u, "
                   "refusal %s, %d buffers freed at the replacement, %d "
                   "after, retired %s\n", (unsigned)s8->lent,
                   gt_update_fail ? "never reached" : "taken",
                   freed_at_replace, gt_lend_freed,
                   s8->retired_handle ? "still held" : "gone");
            bad = 1;
         }
         gfx_surface_free(s8);
      }
      gt_lend_mode  = 0;
      gt_lend_owned = 0;
      gt_lend_reset();
   }

   /* 9. partial lending: the driver lends one slot and refuses the
    *    other. A two-slot surface lends just the slot it was given and
    *    copies into the other; a one-slot surface, which needs both to
    *    stay double buffered, lends neither. Every frame still lands. */
   {
      unsigned nslots, k, w, n9 = 64u * 48u;
      int      ok9 = 1;
      gt_async_mode  = 0;
      gt_can_update  = 1;
      gt_lend_mode   = 1;
      gt_lend_owned  = 1;
      gt_lend_refuse = 1;
      for (nslots = 2; nslots >= 1; nslots--)
      {
         gfx_surface_t *s9 = gfx_surface_new(VIDEO_SCALE_PACK(64, 48),
               nslots, GFX_SURFACE_PIXFMT_8888, TEXTURE_FILTER_LINEAR,
               NULL, NULL);
         int up0 = gt_uploads;
         if (!s9)
         {
            ok9 = 0;
            break;
         }
         for (k = 0; k < 6; k++)
         {
            unsigned slot = k % nslots;
            for (w = 0; w < n9; w++)
               s9->slots[slot][w] = 0xff000000u
                  | ((nslots * 64u + k) * 0x01030507u + w);
            if (gfx_surface_submit(s9, slot, false)
                  != GFX_SURFACE_SUBMIT_DONE)
               ok9 = 0;
         }
         if (     gt_uploads - up0 != 6
               || s9->lent != (nslots == 2 ? 1u : 0u))
         {
            printf("[FAIL] partial lending, %u slot(s): lent %u, %d of 6 "
                   "frames uploaded\n", nslots, (unsigned)s9->lent,
                   gt_uploads - up0);
            ok9 = 0;
         }
         gfx_surface_free(s9);
      }
      if (ok9)
         printf("[ok]   a driver lending one slot of two: a two-slot "
                "surface lent that one, a one-slot surface neither; every "
                "frame landed\n");
      else
         bad = 1;
      gt_lend_refuse = -1;
      gt_lend_mode   = 0;
      gt_lend_owned  = 0;
      gt_lend_reset();
   }

   /* 10. cancellation and teardown under lending: an animation is
    *     closed over and over with its jobs queued or decoding into
    *     memory its texture lent (the GPU is never behind here, so jobs
    *     start the moment they may). Closing waits a running decode out
    *     and keeps a queued one from starting, before the surface and
    *     texture go: a job writing after them writes freed memory,
    *     which AddressSanitizer reports. */
   {
      int closes = 0, lent_closes = 0, round;
      gt_async_mode  = 0;
      gt_can_update  = 1;
      gt_lend_mode   = 1;
      gt_lend_owned  = 1;
      gt_lend_freed  = 0;
      for (round = 0; round < 60; round++)
      {
         reset_thumb(&th);
         gfx_thumbnail_anim_open(&th, path);
         for (i = 0; i < 6 + (round % 6); i++)
         {
            gfx_thumbnail_animate(&th, cpu_features_get_time_usec());
            retro_sleep(4);
         }
         if (th.anim)
            closes++;
         if (     th.anim_surface
               && ((gfx_surface_t*)th.anim_surface)->lent)
            lent_closes++;
         gfx_thumbnail_reset(&th);
      }
      if (closes >= 50 && lent_closes >= 20 && gt_lend_freed >= 20)
         printf("[ok]   %d animations closed, %d while lending, with jobs "
                "on lent memory; %d lent buffers freed after them\n",
                closes, lent_closes, gt_lend_freed);
      else
      {
         printf("[FAIL] closing under lending: %d closes, %d while "
                "lending, %d lent buffers freed\n", closes, lent_closes,
                gt_lend_freed);
         bad = 1;
      }
      gt_lend_mode  = 0;
      gt_lend_owned = 0;
      gt_lend_reset();
   }

   /* 11. threaded video, an update the video thread's driver drops:
    *     the frame comes back to the slot's release, which sends the
    *     same frame again rather than leaving the texture behind. */
   reset_thumb(&th);
   gt_async_mode   = 1;
   gt_can_update   = 1;
   gt_async_posted = gt_async_pending = 0;
   gt_drop_retried = 0;
   gfx_thumbnail_anim_open(&th, path);
   {
      int posted, updates;
      /* the first frame loads, the second goes in place */
      for (i = 0; i < 240 && gt_async_posted < 2; i++)
      {
         gfx_thumbnail_animate(&th, cpu_features_get_time_usec());
         if (gt_async_pending)
            gt_async_flush();
         retro_sleep(17);
      }
      for (i = 0; i < 240 && !gt_async_pending; i++)
      {
         gfx_thumbnail_animate(&th, cpu_features_get_time_usec());
         retro_sleep(17);
      }
      posted          = gt_async_posted;
      updates         = gt_updates;
      gt_drop_updates = 1;
      gt_async_flush();               /* dropped: sent again from release */
      if (     gt_async_posted != posted + 1 || !anim_inflight(&th)
            || gt_drop_updates != 0)
      {
         printf("[FAIL] threaded drop: %d posted after the drop (want %d), "
                "inflight=%d\n", gt_async_posted, posted + 1,
                anim_inflight(&th));
         bad = 1;
      }
      gt_async_flush();               /* and lands */
      if (gt_drop_retried == 1 && gt_updates == updates + 1)
         printf("[ok]   threaded drop: the dropped frame was sent again "
                "and landed\n");
      else
      {
         printf("[FAIL] threaded drop: retried %d, %d updates landed\n",
               gt_drop_retried, gt_updates - updates);
         bad = 1;
      }
   }
   gfx_thumbnail_reset(&th);
   gt_async_flush();
   gt_async_mode = 0;

   gfx_thumbnail_anim_worker_deinit();

   /* 12. the surface's direct-video outcomes, slot by slot */
   gt_lend_mode = 0;
   if (gt_surface_outcome_test())
      bad = 1;

   /* 13. threaded video, a release that frees its surface - a load's
    *     completion, then a dropped update's, whose release reads
    *     dropped: the completion's last touch is the release, so
    *     AddressSanitizer sees nothing write the surface after it. */
   {
      unsigned w, n13 = 64u * 48u;
      int ok13 = 1, round;
      gt_async_mode = 1;
      gt_can_update = 1;
      gt_lend_mode  = 0;
      gt_freed_in_release = 0;
      for (round = 0; round < 2; round++)
      {
         gfx_surface_t *s13 = gfx_surface_new(VIDEO_SCALE_PACK(64, 48), 1,
               GFX_SURFACE_PIXFMT_8888, TEXTURE_FILTER_LINEAR,
               gt_release_frees, NULL);
         if (!s13)
         {
            ok13 = 0;
            break;
         }
         for (w = 0; w < n13; w++)
            s13->slots[0][w] = 0xff000000u | (w * 2654435761u >> 8);
         if (round == 1)
         {
            /* the load first, kept; then an update that drops */
            gfx_surface_submit(s13, 0, false);
            gt_async_flush();
            for (w = 0; w < n13; w++)
               s13->slots[0][w] ^= 0x00ffffffu;
            gt_drop_updates = 1;
         }
         gt_free_next = 1;
         if (gfx_surface_submit(s13, 0, false) != GFX_SURFACE_SUBMIT_QUEUED)
            ok13 = 0;
         gt_async_flush();             /* done -> release frees */
         gt_drop_updates = 0;
      }
      if (ok13 && gt_freed_in_release == 2)
         printf("[ok]   threaded completions whose release freed the "
                "surface: a load's and a dropped update's\n");
      else
      {
         printf("[FAIL] release-frees lane: %d freed in release (want 2)\n",
               gt_freed_in_release);
         bad = 1;
      }
      gt_async_mode = 0;
   }

   /* 14. the still, through the thumbnail's own surface: under
    *     threaded video it is queued and the thumbnail stays PENDING
    *     until the upload lands; a second request while the first is
    *     on its way shows the second; a reset while one is on its way
    *     installs nothing; direct video shows it at once; a half-float
    *     image the driver cannot sample is refused and freed. */
   {
      char still[256];
      int ok14 = 1;
      snprintf(still, sizeof(still), "/tmp/gfx_thumb_still_%d.bin",
            (int)getpid());
      {
         FILE *f = fopen(still, "wb");
         if (f)
         {
            fwrite("still", 1, 5, f);
            fclose(f);
         }
      }
      /* the still's submit asks whether it runs on the main thread */
      task_queue_init(false, NULL);
      gt_still_capture = 1;
      gt_async_mode    = 1;
      gt_async_posted  = gt_async_pending = 0;
      gt_uploads       = 0;
      gt_last_crc      = 0;
      memset(&th, 0, sizeof(th));

      if (!gt_still_request(still, &th, 0x1000u, 0))
         ok14 = 0;
      if (     (int)th.status != GFX_THUMBNAIL_STATUS_PENDING
            || th.texture || !anim_inflight(&th) || gt_async_posted != 1
            || th.dims != VIDEO_SCALE_PACK(4, 4))
      {
         printf("[FAIL] still: queued upload left status=%d texture=%lu "
                "inflight=%d posted=%d\n", (int)th.status,
                (unsigned long)th.texture, anim_inflight(&th),
                gt_async_posted);
         ok14 = 0;
      }
      /* a second request before the first lands */
      if (!gt_still_request(still, &th, 0x2000u, 0))
         ok14 = 0;
      gt_async_flush();
      if (     (int)th.status != GFX_THUMBNAIL_STATUS_AVAILABLE
            || th.texture != 2 || anim_inflight(&th) || gt_async_pending
            || gt_uploads != 2)
      {
         printf("[FAIL] still: after two requests status=%d texture=%lu "
                "inflight=%d pending=%d uploads=%d\n", (int)th.status,
                (unsigned long)th.texture, anim_inflight(&th),
                gt_async_pending, gt_uploads);
         ok14 = 0;
      }
      /* reset with the still on its way: delivery installs nothing */
      if (!gt_still_request(still, &th, 0x3000u, 0))
         ok14 = 0;
      gfx_thumbnail_reset(&th);
      gt_async_flush();
      if (     th.texture || th.anim_surface || gt_async_pending
            || (int)th.status != GFX_THUMBNAIL_STATUS_UNKNOWN)
      {
         printf("[FAIL] still: delivery after reset left texture=%lu "
                "surface=%p status=%d\n", (unsigned long)th.texture,
                th.anim_surface, (int)th.status);
         ok14 = 0;
      }
      /* direct video: shown from the callback */
      gt_async_mode = 0;
      if (!gt_still_request(still, &th, 0x4000u, 0))
         ok14 = 0;
      if (     (int)th.status != GFX_THUMBNAIL_STATUS_AVAILABLE
            || th.texture != 2 || gt_async_posted != 3)
      {
         printf("[FAIL] still: direct upload left status=%d texture=%lu "
                "posted=%d\n", (int)th.status, (unsigned long)th.texture,
                gt_async_posted);
         ok14 = 0;
      }
      /* half floats the driver cannot sample: no still */
      gt_async_mode = 1;
      if (!gt_still_request(still, &th, 0x5000u, 1))
         ok14 = 0;
      if (     (int)th.status != GFX_THUMBNAIL_STATUS_MISSING
            || th.texture || anim_inflight(&th) || gt_async_posted != 3)
      {
         printf("[FAIL] still: refused half floats left status=%d "
                "texture=%lu posted=%d\n", (int)th.status,
                (unsigned long)th.texture, gt_async_posted);
         ok14 = 0;
      }
      gfx_thumbnail_reset(&th);
      gt_async_flush();
      gt_async_mode    = 0;
      gt_still_capture = 0;
      task_queue_deinit();
      remove(still);
      if (ok14)
         printf("[ok]   still: queued, replaced in flight, dropped by a "
                "reset, direct, and refused half floats\n");
      else
         bad = 1;
   }

   remove(path);
   printf("%s\n", bad ? "FAILED" : "PASS");
   return bad;
}

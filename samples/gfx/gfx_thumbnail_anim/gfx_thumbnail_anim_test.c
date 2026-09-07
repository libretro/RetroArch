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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <boolean.h>
#include "gfx/gfx_thumbnail.h"

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

void gfx_thumbnail_anim_open(gfx_thumbnail_t *t, const char *path);

static void reset_thumb(gfx_thumbnail_t *t)
{
   memset(t, 0, sizeof(*t));
   /* what gfx_thumbnail_handle_upload leaves behind just before it
    * reaches the animation block */
   t->status  = GFX_THUMBNAIL_STATUS_AVAILABLE;
   t->width   = 4;
   t->height  = 4;
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
      for (i = 0; i < 240 && gt_uploads < 3; i++)
      {
         gfx_thumbnail_animate(&th);
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

   remove(path);
   printf("%s\n", bad ? "FAILED" : "PASS");
   return bad;
}

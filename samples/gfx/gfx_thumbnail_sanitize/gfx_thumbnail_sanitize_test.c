/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (gfx_thumbnail_sanitize_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Sanitizer sweep for gfx/gfx_thumbnail.c.
 *
 * Links the real thing rather than mirroring it, and drives the
 * lifecycle the sanitizers care about: init, work, teardown;
 * path-data reset and reuse mid-stream; the
 * request/callback/reset cycle; repeated and idempotent frees;
 * and the producer/consumer pairing the atomic status field
 * exists for, on two real threads so TSan has something to see.
 *
 * The image-load callback is fired by this file rather than by
 * the task queue (see stubs_retroarch.c), which is what lets the
 * upload side run on a thread of our choosing while the video
 * side polls.
 *
 * Run it four ways:
 *
 *     make && ./gfx_thumbnail_sanitize_test
 *     make clean && make SANITIZER=address,undefined
 *     ASAN_OPTIONS=detect_leaks=1:detect_stack_use_after_return=1 \
 *         ./gfx_thumbnail_sanitize_test
 *     make clean && make SANITIZER=thread
 *     ./gfx_thumbnail_sanitize_test
 *
 * All four are clean as of writing, over five consecutive TSan
 * runs.
 *
 * THREE WAYS THIS NEARLY LIED, AND WHAT STOPS IT NOW
 *
 * The first working version reported ALL OK with
 * texture loads=0: it pointed at a path that did not exist,
 * path_is_valid() rejected it, and the whole request path was
 * skipped.  Hence make_probe_file() and the explicit assertions
 * on the load/unload counts below -- a run that reaches nothing
 * now fails instead of passing.
 *
 * The first LSan run reported 81920 bytes leaked, all of it
 * because image_texture_free() was stubbed as a no-op and that
 * is the function that owns the pixel buffer.  The stub now
 * frees it like the real one, so a leak that survives is the
 * code under test's.
 *
 * The first TSan run reported six races, all of them the
 * harness's own go/stop handshake, which was a pair of volatile
 * ints.  volatile is not a synchronisation primitive and TSan
 * was right to say so; they are atomics now, because six
 * harness warnings would bury one real report.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rthreads/rthreads.h>
#include <queues/task_queue.h>
#include <retro_timers.h>
#include <retro_atomic.h>

#define GFX_THUMB_STATUS_LOAD(p)     retro_atomic_load_acquire_int(p)
#define GFX_THUMB_STATUS_STORE(p, v) retro_atomic_store_release_int((p), (v))

#include "gfx/gfx_thumbnail.h"

typedef struct
{
   retro_task_callback_t cb;
   void                 *user_data;
   uint32_t              type_hash;
   char                  path[4096];
   int                   pending;
} stub_image_request_t;

extern stub_image_request_t g_stub_image_req;
extern int g_stub_texture_loads;
extern int g_stub_texture_unloads;
extern int g_stub_quiet;

static int  failures;
static char s_probe_path[512];

/* gfx_thumbnail_request_file() calls path_is_valid() before it does
 * anything else, so without a file on disk the whole request path is
 * skipped and every check below passes having tested nothing.  Write
 * one. */
static int make_probe_file(void)
{
   FILE *f;
   snprintf(s_probe_path, sizeof(s_probe_path),
         "gfx_thumbnail_sanitize_probe.png");
   if (!(f = fopen(s_probe_path, "wb")))
      return 0;
   /* Contents are never decoded -- task_push_image_load is stubbed --
    * so a PNG signature is plenty. */
   fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
   fclose(f);
   return 1;
}

#define CHECK(cond, msg) \
   do { if (!(cond)) { fprintf(stderr, "FAIL: %s\n", (msg)); failures++; } } while (0)

/* Deliver the image-load result the way task_image_load's callback
 * would.  A NULL task_data is the "decode failed" path, which the
 * thumbnail code has to treat as MISSING rather than AVAILABLE. */
static void deliver(int succeed)
{
   struct texture_image *img = NULL;

   if (!g_stub_image_req.pending || !g_stub_image_req.cb)
      return;

   if (succeed)
   {
      img = (struct texture_image*)calloc(1, sizeof(*img));
      if (img)
      {
         img->width  = 64;
         img->height = 64;
         img->pixels = (uint32_t*)calloc(64 * 64, sizeof(uint32_t));
      }
   }

   g_stub_image_req.pending = 0;
   g_stub_image_req.cb(NULL, img, g_stub_image_req.user_data, NULL);
}

/* ------------------------------------------------------------------
 * 1. init -> request -> deliver -> reset -> free, repeatedly
 * ------------------------------------------------------------------ */
static void test_lifecycle(void)
{
   gfx_thumbnail_t thumb;
   int i;

   for (i = 0; i < 8; i++)
   {
      memset(&thumb, 0, sizeof(thumb));
      gfx_thumbnail_init_blank(&thumb);

      CHECK(GFX_THUMB_STATUS_LOAD(&thumb.status)
            == GFX_THUMBNAIL_STATUS_UNKNOWN,
            "a blank thumbnail should start UNKNOWN");

      gfx_thumbnail_request_file(s_probe_path, &thumb, 0);
      deliver(i & 1);

      gfx_thumbnail_reset(&thumb);

      CHECK(thumb.texture == 0,
            "reset should leave no texture behind");
   }
}

/* ------------------------------------------------------------------
 * 2. double free / idempotent free
 * ------------------------------------------------------------------ */
static void test_double_reset(void)
{
   gfx_thumbnail_t thumb;

   memset(&thumb, 0, sizeof(thumb));
   gfx_thumbnail_init_blank(&thumb);
   gfx_thumbnail_request_file(s_probe_path, &thumb, 0);
   deliver(1);

   gfx_thumbnail_reset(&thumb);
   gfx_thumbnail_reset(&thumb);
   gfx_thumbnail_reset(&thumb);

   CHECK(thumb.texture == 0, "repeated reset must stay clean");
}

/* ------------------------------------------------------------------
 * 3. path data: reset and reuse mid-stream
 * ------------------------------------------------------------------ */
static void test_path_data_churn(void)
{
   gfx_thumbnail_path_data_t *pd = gfx_thumbnail_path_init();
   int i;

   CHECK(pd != NULL, "path_init should allocate");
   if (!pd)
      return;

   for (i = 0; i < 32; i++)
   {
      char label[64];
      snprintf(label, sizeof(label), "content-%d", i);

      gfx_thumbnail_set_system(pd, "SystemName", NULL);
      gfx_thumbnail_set_content(pd, label);
      gfx_thumbnail_update_path(pd, GFX_THUMBNAIL_RIGHT);
      gfx_thumbnail_update_path(pd, GFX_THUMBNAIL_LEFT);

      if (i & 1)
         gfx_thumbnail_path_reset(pd);

      gfx_thumbnail_set_content_image(pd, "/tmp", "image.png");
      gfx_thumbnail_update_path(pd, GFX_THUMBNAIL_ICON);
   }

   free(pd);
}

/* ------------------------------------------------------------------
 * 4. the producer/consumer pairing, on two real threads
 *
 * The upload thread publishes; the video thread polls status and,
 * on AVAILABLE, reads the fields the publisher wrote before it.
 * A missing release/acquire pair shows up here as a TSan report or
 * as a torn read of width/height/texture.
 * ------------------------------------------------------------------ */
#define PAIR_ITERS 2000

/* go/stop are atomics, not volatile ints.  volatile is not a
 * synchronisation primitive and TSan is right to say so; leaving them
 * plain buries any real report from gfx_thumbnail.c under six
 * warnings about the harness. */
typedef struct
{
   gfx_thumbnail_t    thumb;
   retro_atomic_int_t go;
   retro_atomic_int_t stop;
   int                torn;
} pair_state_t;

static void upload_thread(void *arg)
{
   pair_state_t *st = (pair_state_t*)arg;
   int i;

   for (i = 0; i < PAIR_ITERS; i++)
   {
      while (      !retro_atomic_load_acquire_int(&st->go)
               && !retro_atomic_load_acquire_int(&st->stop))
         retro_sleep(0);
      if (retro_atomic_load_acquire_int(&st->stop))
         return;

      /* Publish, in the order the real upload callback does. */
      st->thumb.width  = 64;
      st->thumb.height = 64;
      st->thumb.texture = (uintptr_t)(0x2000 + i);
      GFX_THUMB_STATUS_STORE(&st->thumb.status,
            GFX_THUMBNAIL_STATUS_AVAILABLE);

      retro_atomic_store_release_int(&st->go, 0);
   }
}

static void test_publish_observe(void)
{
   pair_state_t st;
   sthread_t *t;
   int i;

   memset(&st, 0, sizeof(st));
   retro_atomic_int_init(&st.go, 0);
   retro_atomic_int_init(&st.stop, 0);
   gfx_thumbnail_init_blank(&st.thumb);

   if (!(t = sthread_create(upload_thread, &st)))
   {
      fprintf(stderr, "FAIL: could not start the upload thread\n");
      failures++;
      return;
   }

   for (i = 0; i < PAIR_ITERS; i++)
   {
      GFX_THUMB_STATUS_STORE(&st.thumb.status,
            GFX_THUMBNAIL_STATUS_PENDING);
      st.thumb.texture = 0;
      retro_atomic_store_release_int(&st.go, 1);

      while (GFX_THUMB_STATUS_LOAD(&st.thumb.status)
            != GFX_THUMBNAIL_STATUS_AVAILABLE)
         retro_sleep(0);

      /* Everything the publisher wrote before the release store must
       * be visible now. */
      if (     st.thumb.width  != 64
            || st.thumb.height != 64
            || st.thumb.texture != (uintptr_t)(0x2000 + i))
         st.torn++;
   }

   retro_atomic_store_release_int(&st.stop, 1);
   retro_atomic_store_release_int(&st.go, 1);
   sthread_join(t);

   CHECK(st.torn == 0, "a published thumbnail was observed half-written");
}

int main(void)
{
   g_stub_quiet = 1;

   if (!make_probe_file())
   {
      fputs("FAIL: could not create the probe file in the"
            " working directory\n", stderr);
      return 1;
   }

   test_lifecycle();
   test_double_reset();
   test_path_data_churn();
   test_publish_observe();

   remove(s_probe_path);

   /* The counts are asserted, not just printed.  Zero here means the
    * request path was never entered and everything above passed
    * without touching the code under test. */
   printf("texture loads=%d unloads=%d\n",
         g_stub_texture_loads, g_stub_texture_unloads);

   CHECK(g_stub_texture_loads > 0,
         "no texture was ever loaded; the request path was not reached");
   CHECK(g_stub_texture_loads == g_stub_texture_unloads,
         "texture loads and unloads are unbalanced");

   if (failures)
   {
      fprintf(stderr, "%d check(s) failed\n", failures);
      return 1;
   }
   puts("ALL OK");
   return 0;
}

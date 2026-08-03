/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (image_decode_slice_test.c).
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

/* Regression test for the shared decode window in tasks/task_image.c.
 *
 * The transfer and process loops there used to spin against
 * frame_duration per task, and retro_task_regular_gather() runs
 * every running task's handler once per frame, so the per-task
 * budget multiplied: eight concurrent large decodes could each
 * legitimately consume a full frame of CPU in a single gather.  The
 * fix is a window shared across image tasks - and its period must
 * exceed its allowance, because with the two equal, a task that
 * consumes its whole allowance has by definition aged the window a
 * full period, so the next task in the same gather finds it expired,
 * resets it, and takes a full allowance too: the multiplication
 * survives the "fix" intact.  That variant passed inspection and
 * failed simulation, which is why this test exists in this exact
 * shape.
 *
 * The unit under test is the shipping tasks/task_image.c (and
 * tasks/task_file_transfer.c under it), compiled from the tree, with
 * the frontend stubbed at link time.  Time is the test's instrument:
 * cpu_features_get_time_usec() is provided here, not linked from
 * features_cpu.c, and every observation of the clock advances it by
 * a fixed quantum.  Loop iterations therefore consume deterministic
 * "time", the decode loops become budget-bound exactly as they are
 * against a wall clock, and the cost of a gather is readable as the
 * fake-clock delta across task_queue_check() - no real-time
 * measurement, no flakiness, and the same arithmetic the shipping
 * slice performs.
 *
 * The workload is a synthesized PNG whose zlib stream is split
 * across thousands of tiny IDAT chunks: rpng_iterate_image() walks
 * one chunk per call, so the TRANSFER loop has far more iterations
 * available than any single allowance covers and every decode gather
 * is time-bound, not work-bound.  The stream itself is stored
 * (uncompressed) deflate, built by hand, so the fixture needs no
 * compressor and the decoded pixels are exactly predictable.
 *
 * Checks:
 *   - eight concurrent decodes: no gather's decode cost may exceed
 *     a small multiple of one frame_duration.  The pre-fix code and
 *     the period==allowance variant both cost ~8 frame_durations per
 *     gather here and fail loudly.
 *   - a single decode still gets the full frame_duration per gather
 *     it always had: at least one mid-decode gather must consume it.
 *   - every load still decodes correctly to the end: dimensions and
 *     a pixel spot-check on the delivered image, so the budget
 *     rework cannot have broken the decode itself.
 *
 * Build:  make            (SANITIZER=address,undefined for a checked run)
 * Run:    ./image_decode_slice_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>
#include <encodings/crc32.h>
#include <features/features_cpu.h>
#include <formats/image.h>
#include <queues/task_queue.h>

#include "../../../configuration.h"
#include "../../../gfx/video_driver.h"
#include "../../../gfx/gfx_display.h"
#include "../../../tasks/tasks_internal.h"

/* ---- deterministic clock ----------------------------------------- */

/* Every observation advances time: loop iterations cost exactly one
 * quantum each, so a budget of FRAME_USEC admits about
 * FRAME_USEC / CLOCK_QUANTUM_USEC of them. */
#define CLOCK_QUANTUM_USEC 250
#define FRAME_USEC         16666   /* frame_duration at the stubbed 60 Hz */

static retro_time_t fake_now = 1000000;

retro_time_t cpu_features_get_time_usec(void)
{
   fake_now += CLOCK_QUANTUM_USEC;
   return fake_now;
}

/* SIMD-dispatch query from the crc32/deflate helpers; scalar paths
 * are fine for a correctness test and keep the fake clock the only
 * features_cpu symbol with behavior. */
uint64_t cpu_features_get(void)
{
   return 0;
}

/* ---- frontend stubs ---------------------------------------------- */

static settings_t stub_settings;

settings_t *config_get_ptr(void)
{
   stub_settings.floats.video_refresh_rate = 60.0f;
   return &stub_settings;
}

bool video_driver_test_all_flags(enum display_flags testflag)
{
   (void)testflag;
   return false;                 /* no 10-bit path */
}

uint32_t video_driver_get_disp_flags(void)
{
   return 0;                     /* no RGBA reorder */
}

bool video_driver_texture_load(void *data,
      enum texture_filter_type filter_type, uintptr_t *id)
{
   (void)data;
   (void)filter_type;
   if (id)
      *id = 1;
   return true;
}

bool video_driver_texture_unload(uintptr_t *id)
{
   if (id)
      *id = 0;
   return true;
}

enum texture_filter_type gfx_display_texture_filter(void)
{
   return TEXTURE_FILTER_LINEAR;
}

/* ---- PNG fixture -------------------------------------------------- */

/* Grayscale 8-bit, every scanline filter 0 and every pixel the row
 * index mod 256, so the decoded ARGB is exactly predictable. */
#define FIX_W          256
#define FIX_H          2400
#define FIX_IDAT_SPLIT 240     /* bytes of zlib stream per IDAT chunk */

static void put_be32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v >> 24);
   p[1] = (uint8_t)(v >> 16);
   p[2] = (uint8_t)(v >>  8);
   p[3] = (uint8_t)(v      );
}

static void png_chunk(FILE *f, const char *type,
      const uint8_t *data, size_t len)
{
   uint8_t hdr[8];
   uint8_t crc[4];
   uint32_t c;
   put_be32(hdr, (uint32_t)len);
   memcpy(hdr + 4, type, 4);
   c = encoding_crc32(0, hdr + 4, 4);
   if (len)
      c = encoding_crc32(c, data, len);
   put_be32(crc, c);
   fwrite(hdr, 1, 8, f);
   if (len)
      fwrite(data, 1, len, f);
   fwrite(crc, 1, 4, f);
}

static int write_fixture_png(const char *path)
{
   static const uint8_t sig[8] =
         { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
   size_t   raw_len  = (size_t)FIX_H * (FIX_W + 1);
   uint8_t *raw      = (uint8_t*)malloc(raw_len);
   size_t   zcap     = raw_len + (raw_len / 65535 + 2) * 5 + 8;
   uint8_t *z        = (uint8_t*)malloc(zcap);
   size_t   zlen     = 0;
   uint32_t s1       = 1, s2 = 0;
   size_t   off, y;
   FILE    *f;

   if (!raw || !z)
   {
      free(raw);
      free(z);
      return 0;
   }

   for (y = 0; y < (size_t)FIX_H; y++)
   {
      uint8_t *row = raw + y * (FIX_W + 1);
      row[0] = 0;                          /* filter: none */
      memset(row + 1, (int)(y & 0xff), FIX_W);
   }

   /* zlib header, then stored deflate blocks, then adler32 */
   z[zlen++] = 0x78;
   z[zlen++] = 0x01;
   for (off = 0; off < raw_len; )
   {
      size_t blk = raw_len - off;
      if (blk > 65535)
         blk = 65535;
      z[zlen++] = (off + blk >= raw_len) ? 0x01 : 0x00; /* BFINAL */
      z[zlen++] = (uint8_t)(blk & 0xff);                /* LEN    */
      z[zlen++] = (uint8_t)(blk >> 8);
      z[zlen++] = (uint8_t)(~blk & 0xff);               /* NLEN   */
      z[zlen++] = (uint8_t)(~(blk >> 8) & 0xff);
      memcpy(z + zlen, raw + off, blk);
      zlen += blk;
      off  += blk;
   }
   for (off = 0; off < raw_len; off++)
   {
      s1 = (s1 + raw[off]) % 65521;
      s2 = (s2 + s1)       % 65521;
   }
   put_be32(z + zlen, (s2 << 16) | s1);
   zlen += 4;

   if (!(f = fopen(path, "wb")))
   {
      free(raw);
      free(z);
      return 0;
   }
   fwrite(sig, 1, 8, f);
   {
      uint8_t ihdr[13];
      put_be32(ihdr,     FIX_W);
      put_be32(ihdr + 4, FIX_H);
      ihdr[8]  = 8;   /* bit depth   */
      ihdr[9]  = 0;   /* grayscale   */
      ihdr[10] = 0;   /* compression */
      ihdr[11] = 0;   /* filter      */
      ihdr[12] = 0;   /* interlace   */
      png_chunk(f, "IHDR", ihdr, 13);
   }
   /* the zlib stream split across many small IDATs is the point:
    * one rpng_iterate_image() call consumes one chunk, so the
    * TRANSFER loop has thousands of iterations to be budgeted */
   for (off = 0; off < zlen; off += FIX_IDAT_SPLIT)
   {
      size_t c = zlen - off;
      if (c > FIX_IDAT_SPLIT)
         c = FIX_IDAT_SPLIT;
      png_chunk(f, "IDAT", z + off, c);
   }
   png_chunk(f, "IEND", NULL, 0);
   fclose(f);
   free(raw);
   free(z);
   return 1;
}

/* ---- driver ------------------------------------------------------- */

static int test_fails;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL: %s\n", msg); \
         test_fails++; \
      } \
      else \
         printf("ok:   %s\n", msg); \
   } while (0)

static int cb_done;
static int cb_bad_image;

static void image_loaded_cb(retro_task_t *task,
      void *task_data, void *user_data, const char *error)
{
   struct texture_image *img = (struct texture_image*)task_data;
   (void)task;
   (void)user_data;
   if (error || !img || img->width != FIX_W || img->height != FIX_H
         || !img->pixels)
      cb_bad_image++;
   else
   {
      /* row 100 decodes to gray 100: ARGB 0xff646464 */
      uint32_t px = img->pixels[(size_t)100 * FIX_W];
      if (px != 0xff646464u)
         cb_bad_image++;
   }
   if (img)
   {
      image_texture_free(img);
      free(img);
   }
   cb_done++;
}

/* Pump the queue to completion, recording the most expensive gather
 * in fake microseconds.  'skip' gathers at the front are excluded
 * from the max (the first mixes file I/O with decode); the last
 * gather before completion is excluded too, since it is legitimately
 * short. */
static retro_time_t pump_and_measure(int expect, int skip,
      retro_time_t *out_last_full)
{
   retro_time_t worst = 0;
   retro_time_t prev  = 0;
   int gather         = 0;
   int guard          = 200000;

   while (cb_done < expect && guard-- > 0)
   {
      retro_time_t t0 = fake_now;
      retro_time_t cost;
      task_queue_check();
      cost = fake_now - t0;
      gather++;
      if (gather > skip && cb_done < expect)
      {
         if (cost > worst)
            worst = cost;
         prev = cost;
      }
   }
   if (out_last_full)
      *out_last_full = prev;
   if (guard <= 0)
      printf("FAIL: pump guard exhausted (%d/%d done)\n",
            cb_done, expect);
   return worst;
}

int main(void)
{
   if (!write_fixture_png("slice_fixture.png"))
   {
      printf("FAIL: could not write fixture\n");
      return 1;
   }

   task_queue_init(false, NULL);   /* the regular gather is the unit */

   /* ---- eight concurrent decodes: bounded, not multiplied ---- */
   {
      retro_time_t worst;
      int i;
      cb_done      = 0;
      cb_bad_image = 0;
      for (i = 0; i < 8; i++)
         CHECK(task_push_image_load("slice_fixture.png",
               false, 0, 0, image_loaded_cb, NULL),
               "image load queued");
      worst = pump_and_measure(8, 2, NULL);
      printf("info: worst decode gather with 8 tasks: %ld fake usec "
             "(frame is %d)\n", (long)worst, FRAME_USEC);
      CHECK(worst > 0, "decode gathers observed");
      /* pre-fix, and the period==allowance variant, both cost about
       * 8 * FRAME_USEC here; the shared window costs about one
       * allowance plus a floor iteration per task */
      CHECK(worst < (retro_time_t)3 * FRAME_USEC,
            "8-task gather bounded by the shared window, not 8 frames");
      CHECK(cb_done == 8, "all eight loads completed");
      CHECK(cb_bad_image == 0,
            "every delivered image has correct dimensions and pixels");
   }

   /* ---- a single decode keeps its full per-gather allowance ---- */
   {
      retro_time_t worst;
      cb_done      = 0;
      cb_bad_image = 0;
      CHECK(task_push_image_load("slice_fixture.png",
            false, 0, 0, image_loaded_cb, NULL),
            "single image load queued");
      worst = pump_and_measure(1, 2, NULL);
      printf("info: worst decode gather with 1 task: %ld fake usec\n",
            (long)worst);
      CHECK(worst >= (retro_time_t)FRAME_USEC,
            "single task still consumes a full frame_duration per gather");
      CHECK(worst < (retro_time_t)2 * FRAME_USEC,
            "single task does not exceed its allowance");
      CHECK(cb_done == 1, "single load completed");
      CHECK(cb_bad_image == 0, "single load decoded correctly");
   }

   task_queue_deinit();
   remove("slice_fixture.png");

   if (test_fails)
   {
      printf("== %d FAILURES ==\n", test_fails);
      return 1;
   }
   printf("== image_decode_slice_test: all tests pass ==\n");
   return 0;
}

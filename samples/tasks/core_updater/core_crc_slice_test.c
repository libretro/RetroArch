/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (core_crc_slice_test.c).
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

/* Regression tests for the resumable CRC in
 * libretro-common/streams/interface_stream.c, and for the sliced
 * hashing loop tasks/task_core_updater.c drives it with.
 *
 * WHY
 *
 * task_core_updater.c used to hash a core file with a single blocking
 * intfstream_get_crc() inside a task handler tick, so the cost of
 * that tick was a function of file size and nothing else.  Measured
 * cold on NVMe: 43ms for a 40MB core, 112ms for a 260MB one -- 3 to 7
 * dropped frames apiece, and an order of magnitude worse on the
 * SD-card and spinning-disk targets RetroArch also ships to.
 *
 * The bulk path is where it bites.  update_installed_cores hashes
 * every installed core to discover which ones changed: a realistic
 * 40-core set is 601MB read (measured) before anything is downloaded,
 * and a repeat run reads all of it again to establish that nothing
 * changed.
 *
 * intfstream_crc_step() makes the hash resumable so the caller can
 * bound a tick, and intfstream_get_crc() is now a wrapper over it.
 * Two properties have to hold, and they pull in opposite directions:
 *
 *   1. Resuming must not change the answer.  A CRC assembled from
 *      arbitrary partial steps must equal the one-shot value, at
 *      every chunk size and every boundary -- otherwise a core is
 *      judged out of date when it isn't, or worse, up to date when it
 *      isn't.  This is the correctness half and most of the test.
 *
 *   2. No single step may carry an unbounded share of the work.
 *      That is the whole point of the change, and it is easy to
 *      satisfy property 1 by quietly hashing everything in the first
 *      step, so it is asserted directly rather than assumed.
 *
 * WHAT IS REAL HERE
 *
 * intfstream_crc_step() and intfstream_get_crc() are the shipping
 * functions, compiled from the tree.  The slicing loop is a
 * behavioural copy of task_core_updater_crc_step(), because
 * task_core_updater.c cannot be linked standalone -- it pulls in
 * configuration, core_info and the menu.  That copy is the same
 * convention http_method_match_test.c and archive_name_safety_test.c
 * use; if task_core_updater.c changes how it slices, the copy here
 * must follow.
 *
 * The clock is the test's instrument, as in save_state_io_test: the
 * sample supplies cpu_features_get_time_usec() itself through
 * --wrap, advancing a fixed step per observation.  That makes the
 * budget assertable exactly instead of measured against a wall clock,
 * so the tick-bound lane cannot go flaky on a loaded CI runner.  A
 * step of 0 is a budget that never expires; a step at or above the
 * budget is one that expires on first observation, which is the
 * slowest-storage case and must still make forward progress rather
 * than spin.
 *
 * Build and run:
 *   make check
 *   make check SANITIZER=address
 *   make check SANITIZER=undefined
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <streams/interface_stream.h>
#include <streams/file_stream.h>
#include <retro_common_api.h>

/* Mirrors task_core_updater.c */
#define CORE_CRC_CHUNK           (256 * 1024)
#define CORE_CRC_TICK_BUDGET_US  4000

static int failures;
static int checks;

#define CHECK(cond, ...) \
   do { \
      checks++; \
      if (!(cond)) \
      { \
         printf("    FAIL: "); printf(__VA_ARGS__); printf("\n"); \
         failures++; \
      } \
   } while (0)

/* ================================================================= */
/* Fake clock                                                        */
/* ================================================================= */

int64_t __real_cpu_features_get_time_usec(void);

static int64_t g_now;
static int64_t g_step;     /* advance per observation */
static long    g_observations;

int64_t __wrap_cpu_features_get_time_usec(void)
{
   g_observations++;
   g_now += g_step;
   return g_now;
}

static void clock_reset(int64_t step)
{
   g_now          = 1000000;
   g_step         = step;
   g_observations = 0;
}

/* ================================================================= */
/* Fixtures                                                          */
/* ================================================================= */

static char g_dir[] = "/tmp/core_crc_test_XXXXXX";

/* Non-uniform content, so a slicing bug that drops or double-counts a
 * span is caught rather than masked by a run of identical bytes. */
static char *make_file(const char *name, size_t len)
{
   static char path[512];
   unsigned int seed = 0x2545f49u ^ (unsigned int)len;
   size_t i;
   RFILE *f;
   uint8_t *buf = NULL;

   snprintf(path, sizeof(path), "%s/%s", g_dir, name);

   if (len && !(buf = (uint8_t*)malloc(len)))
      return NULL;

   for (i = 0; i < len; i++)
   {
      seed = seed * 1103515245u + 12345u;
      buf[i] = (uint8_t)(seed >> 16);
   }

   if (!(f = filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE,
               RETRO_VFS_FILE_ACCESS_HINT_NONE)))
   {
      free(buf);
      return NULL;
   }
   if (len)
      filestream_write(f, buf, (int64_t)len);
   /* filestream_close() frees the RFILE on success, so the caller
    * frees only when it fails -- same idiom as
    * filestream_write_file().  intfstream_close() is the opposite:
    * it closes the inner file but leaves the intfstream to the
    * caller, which is why close_stream() below does free it. */
   if (filestream_close(f) != 0)
      free(f);
   free(buf);
   return path;
}

static intfstream_t *open_ro(const char *path)
{
   return intfstream_open_file(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
}

static void close_stream(intfstream_t *f)
{
   if (!f)
      return;
   intfstream_close(f);
   free(f);
}

/* One-shot reference. */
static int oneshot_crc(const char *path, uint32_t *out)
{
   intfstream_t *f = open_ro(path);
   int ok;
   if (!f)
      return 0;
   ok = intfstream_get_crc(f, out) ? 1 : 0;
   close_stream(f);
   return ok;
}

/* Accumulate via crc_step at a fixed chunk size. */
static int stepped_crc(const char *path, size_t chunk, uint32_t *out,
      long *steps)
{
   intfstream_t *f = open_ro(path);
   uint32_t acc    = 0;
   int64_t  n;

   *steps = 0;
   if (!f)
      return 0;
   intfstream_rewind(f);
   while ((n = intfstream_crc_step(f, &acc, chunk)) > 0)
      (*steps)++;
   close_stream(f);
   if (n < 0)
      return 0;
   *out = acc;
   return 1;
}

/* ================================================================= */
/* Behavioural copy of task_core_updater_crc_step()                  */
/* ================================================================= */

typedef struct
{
   intfstream_t *file;
   uint32_t      accumulator;
   bool          active;
} core_crc_slice_t;

static void slice_reset(core_crc_slice_t *slice)
{
   if (slice->file)
   {
      intfstream_close(slice->file);
      free(slice->file);
      slice->file = NULL;
   }
   slice->accumulator = 0;
   slice->active      = false;
}

static bool slice_step(core_crc_slice_t *slice, const char *core_path,
      uint32_t *crc)
{
   int64_t deadline;

   if (!slice->active)
   {
      slice->accumulator = 0;
      if (!(slice->file = open_ro(core_path)))
      {
         *crc = 0;
         return true;
      }
      intfstream_rewind(slice->file);
      slice->active = true;
   }

   deadline = __wrap_cpu_features_get_time_usec() + CORE_CRC_TICK_BUDGET_US;
   do
   {
      int64_t hashed = intfstream_crc_step(slice->file,
            &slice->accumulator, CORE_CRC_CHUNK);
      if (hashed > 0)
         continue;
      *crc = (hashed == 0) ? slice->accumulator : 0;
      slice_reset(slice);
      return true;
   } while (__wrap_cpu_features_get_time_usec() < deadline);

   return false;
}

/* ================================================================= */
/* Tests                                                             */
/* ================================================================= */

/* Property 1: resuming must not change the answer, at any chunk size
 * or boundary. */
static void test_stepped_matches_oneshot(void)
{
   static const size_t sizes[] = {
      0, 1, 2, 255, 4096,
      CORE_CRC_CHUNK - 1, CORE_CRC_CHUNK, CORE_CRC_CHUNK + 1,
      CORE_CRC_CHUNK * 2, CORE_CRC_CHUNK * 2 + 7,
      1024 * 1024 + 12345
   };
   static const size_t chunks[] = {
      1, 2, 3, 511, 4096, 65536, CORE_CRC_CHUNK,
      CORE_CRC_CHUNK * 4, (size_t)-1
   };
   size_t si, ci;

   printf("  stepped CRC == one-shot CRC, all sizes x all chunk sizes\n");

   for (si = 0; si < sizeof(sizes) / sizeof(sizes[0]); si++)
   {
      char name[64];
      const char *path;
      uint32_t ref = 0;

      snprintf(name, sizeof(name), "f_%u.bin", (unsigned)sizes[si]);
      if (!(path = make_file(name, sizes[si])))
      {
         printf("    SKIP: could not create %s\n", name);
         continue;
      }

      if (!oneshot_crc(path, &ref))
      {
         CHECK(0, "one-shot CRC failed for size %u",
               (unsigned)sizes[si]);
         continue;
      }

      for (ci = 0; ci < sizeof(chunks) / sizeof(chunks[0]); ci++)
      {
         uint32_t got = 0;
         long steps   = 0;
         if (!stepped_crc(path, chunks[ci], &got, &steps))
         {
            CHECK(0, "stepped CRC failed: size=%u chunk=%u",
                  (unsigned)sizes[si], (unsigned)chunks[ci]);
            continue;
         }
         CHECK(got == ref,
               "size=%u chunk=%u: stepped %08x != one-shot %08x",
               (unsigned)sizes[si], (unsigned)chunks[ci], got, ref);
      }
   }
}

/* A chunk smaller than the internal read size must actually bound the
 * read, or "step" is a fiction and property 2 cannot hold. */
static void test_chunk_bounds_the_read(void)
{
   const char *path;
   uint32_t crc = 0;
   long steps   = 0;
   size_t len   = CORE_CRC_CHUNK * 2;

   printf("  max_bytes actually bounds one step\n");

   if (!(path = make_file("bound.bin", len)))
   {
      printf("    SKIP: could not create fixture\n");
      return;
   }

   if (!stepped_crc(path, 4096, &crc, &steps))
   {
      CHECK(0, "stepped CRC failed");
      return;
   }

   /* 512KB at 4KB a step: a step that ignored max_bytes would finish
    * in one or two. */
   CHECK(steps >= (long)(len / 4096),
         "expected >= %ld steps at 4096 bytes each, got %ld -- "
         "max_bytes is not bounding the read",
         (long)(len / 4096), steps);
}

/* Property 2: no single tick carries an unbounded share of the work.
 * With a clock that spends the budget on its first observation, the
 * loop must still complete and must take more than one tick for a
 * file bigger than one chunk. */
static void test_tick_is_bounded(void)
{
   core_crc_slice_t slice;
   const char *path;
   uint32_t ref = 0, got = 0;
   long ticks   = 0;
   size_t len   = CORE_CRC_CHUNK * 8;

   printf("  sliced hashing is bounded per tick and still correct\n");

   memset(&slice, 0, sizeof(slice));

   if (!(path = make_file("sliced.bin", len)))
   {
      printf("    SKIP: could not create fixture\n");
      return;
   }
   if (!oneshot_crc(path, &ref))
   {
      CHECK(0, "one-shot CRC failed");
      return;
   }

   /* Budget expires on first observation: one chunk per tick. */
   clock_reset(CORE_CRC_TICK_BUDGET_US);
   while (!slice_step(&slice, path, &got))
      if (++ticks > 10000)
         break;

   CHECK(got == ref, "sliced CRC %08x != one-shot %08x", got, ref);
   CHECK(ticks >= (long)(len / CORE_CRC_CHUNK) - 1,
         "expected roughly %ld ticks for %u bytes at %d per tick, "
         "got %ld -- a single tick is still swallowing the file",
         (long)(len / CORE_CRC_CHUNK), (unsigned)len, CORE_CRC_CHUNK,
         ticks);
   CHECK(!slice.active && !slice.file,
         "slice state not released after completion");
}

/* The opposite extreme: a budget that never expires must complete in
 * one tick, so the change costs nothing where there is time to
 * spare. */
static void test_generous_budget_completes_in_one_tick(void)
{
   core_crc_slice_t slice;
   const char *path;
   uint32_t ref = 0, got = 0;
   long ticks   = 0;

   printf("  a budget that never expires completes in one tick\n");

   memset(&slice, 0, sizeof(slice));

   if (!(path = make_file("fast.bin", CORE_CRC_CHUNK * 8)))
   {
      printf("    SKIP: could not create fixture\n");
      return;
   }
   if (!oneshot_crc(path, &ref))
   {
      CHECK(0, "one-shot CRC failed");
      return;
   }

   clock_reset(0);
   while (!slice_step(&slice, path, &got))
      if (++ticks > 10000)
         break;

   CHECK(got == ref, "CRC %08x != one-shot %08x", got, ref);
   CHECK(ticks == 0, "expected completion within the first tick, "
         "took %ld extra ticks", ticks);
}

/* An unreadable path must complete immediately with 0 rather than
 * stalling the state machine forever -- the callers treat 0 as "no
 * local core to compare against", which is what the blocking version
 * returned. */
static void test_missing_file_completes(void)
{
   core_crc_slice_t slice;
   char path[512];
   uint32_t crc = 0xdeadbeef;

   printf("  unreadable file completes immediately with CRC 0\n");

   memset(&slice, 0, sizeof(slice));
   snprintf(path, sizeof(path), "%s/does_not_exist.bin", g_dir);

   clock_reset(0);
   CHECK(slice_step(&slice, path, &crc),
         "missing file did not complete on the first tick");
   CHECK(crc == 0, "expected CRC 0 for a missing file, got %08x", crc);
   CHECK(!slice.active && !slice.file, "slice state left active");
}

/* Abandoning a slice part-way must release the stream.  This is the
 * cancelled-task path in task_core_updater.c, and the reason
 * free_*_handle() calls the reset; LeakSan is the assertion. */
static void test_abandoned_slice_releases_stream(void)
{
   core_crc_slice_t slice;
   const char *path;
   uint32_t crc = 0;

   printf("  abandoning a slice part-way releases the stream\n");

   memset(&slice, 0, sizeof(slice));

   if (!(path = make_file("abandon.bin", CORE_CRC_CHUNK * 8)))
   {
      printf("    SKIP: could not create fixture\n");
      return;
   }

   clock_reset(CORE_CRC_TICK_BUDGET_US);
   CHECK(!slice_step(&slice, path, &crc),
         "fixture too small to leave the slice mid-flight");
   CHECK(slice.active && slice.file, "slice should be mid-flight");

   slice_reset(&slice);
   CHECK(!slice.active && !slice.file, "reset did not clear the slice");
}

/* ================================================================= */

int main(void)
{
   char cmd[600];

   if (!mkdtemp(g_dir))
   {
      printf("SKIP: could not create temp dir\n");
      return 0;
   }

   printf("core updater sliced CRC regression tests\n\n");

   test_stepped_matches_oneshot();
   test_chunk_bounds_the_read();
   test_tick_is_bounded();
   test_generous_budget_completes_in_one_tick();
   test_missing_file_completes();
   test_abandoned_slice_releases_stream();

   snprintf(cmd, sizeof(cmd), "rm -rf %s", g_dir);
   if (system(cmd)) { /* best effort */ }

   printf("\n%s (%d check%s, %d failure%s)\n",
         failures ? "FAILED" : "PASSED",
         checks,   checks   == 1 ? "" : "s",
         failures, failures == 1 ? "" : "s");
   return failures ? 1 : 0;
}

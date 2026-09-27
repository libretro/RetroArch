/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (data_transfer_pool_scrub_test.c).
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

/* Regression test for the reservation pool's cross-load guard.
 *
 * A recycled slot's pages still hold the previous load's bytes -
 * memrearm() keeps them resident on purpose, that is the whole
 * saving - so the pool has to guarantee that a consumer over-reading
 * its own buffer past avail() sees zeros, never the previous file.
 * The scrub that enforces this is lazy: it runs at the fill's exits,
 * over [max(avail, scrubbed), committed), rather than eagerly over
 * every newly armed span (which zeroed the whole file length and
 * then read the file over it - a quarter of a thumbnail load's cost
 * spent on bytes the next reads replaced).
 *
 * Lazy is exactly the kind of change where the guarantee can rot at
 * one exit and not another, so this test drives a recycled slot
 * through every terminal the fill has and checks the contract at
 * each:
 *
 *   - completed: the sub-page tail above the file length is
 *     readable and must be zeros, never the previous load.
 *   - paused mid-fill (budget callback): everything armed above
 *     avail() must read zeros at the pause, the prefix below must
 *     be the file's own bytes, and resuming must overwrite the
 *     zeroed span with file bytes, not leave the zeros standing.
 *   - capped: same exposure check at the capped() terminal.
 *
 * The leak discriminator is byte content, not a sanitizer: the
 * previous load is written as 0xAA and the current as 0xBB, so a
 * regression answers 0xAA where the contract says 0x00.  Run it
 * under SANITIZER=address,undefined as well - the scrub arithmetic
 * (frontier vs avail vs committed) is where an off-by-one would
 * live, and ASan sees the slot's guard directly.
 *
 * Build:  make            (SANITIZER=address,undefined for a checked run)
 * Run:    ./data_transfer_pool_scrub_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>
#include <formats/data_transfer.h>

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

static void write_file(const char *path, unsigned char byte, size_t len)
{
   FILE *f = fopen(path, "wb");
   unsigned char buf[4096];
   memset(buf, byte, sizeof(buf));
   while (len)
   {
      size_t c = len > sizeof(buf) ? sizeof(buf) : len;
      fwrite(buf, 1, c, f);
      len -= c;
   }
   fclose(f);
}

/* Fill to a terminal.  Every iterate is unbudgeted, so this ends at
 * complete, failed or capped in one call per state. */
static void fill_to_terminal(data_transfer_t *dt)
{
   while (!data_transfer_complete(dt)
         && !data_transfer_failed(dt)
         && !data_transfer_capped(dt))
      data_transfer_iterate(dt, 0);
}

/* Prime the pool: run a whole load of the 0xAA file through a slot
 * and free it, so the next open recycles pages that hold 0xAA. */
static void prime_slot_with_previous_load(const char *path)
{
   data_transfer_t *dt = data_transfer_open_prefix(path, 0);
   if (!dt)
   {
      printf("FAIL: could not open %s to prime the pool\n", path);
      exit(1);
   }
   fill_to_terminal(dt);
   if (!data_transfer_complete(dt))
   {
      printf("FAIL: priming load did not complete\n");
      exit(1);
   }
   data_transfer_free(dt);
}

/* budget callback: allow exactly one read, then stop the fill */
static bool stop_after_first_read(void *ud, size_t avail, size_t len)
{
   int *calls = (int*)ud;
   (void)avail;
   (void)len;
   return ((*calls)++ == 0) ? true : false;
}

int main(void)
{
   /* Both lengths leave a sub-page tail, and both fit the pool slot
    * so the loads actually recycle. */
   size_t page = 4096;
   size_t len_prev = 700 * 1024 + 555;
   size_t len_cur  = 300 * 1024 + 123;

   if (!data_transfer_reserve_supported())
   {
      /* No reservation means no pool and no guard to test: the
       * fallback path holds a plain exact allocation. */
      printf("skip: platform cannot reserve address space\n");
      return 0;
   }

   write_file("pool_prev.bin", 0xAA, len_prev);
   write_file("pool_cur.bin",  0xBB, len_cur);

   /* ---- completed terminal ---- */
   {
      data_transfer_t *dt;
      const uint8_t *p;
      size_t i, l = 0, tail_end;

      prime_slot_with_previous_load("pool_prev.bin");

      dt = data_transfer_open_prefix("pool_cur.bin", 0);
      CHECK(dt != NULL, "current load opens");
      fill_to_terminal(dt);
      CHECK(data_transfer_complete(dt), "current load completes on a recycled slot");

      p = data_transfer_ptr(dt, &l);
      CHECK(l == len_cur, "length reported honestly");

      for (i = 0; i < len_cur; i++)
         if (p[i] != 0xBB)
            break;
      CHECK(i == len_cur, "file content intact below avail");

      /* the committed sub-page tail above the file length is the
       * readable over-read region for a completed load */
      tail_end = ((len_cur + page - 1) / page) * page;
      {
         int leak = 0, nonzero = 0;
         for (i = len_cur; i < tail_end; i++)
         {
            if (p[i] == 0xAA)
               leak = 1;
            if (p[i] != 0x00)
               nonzero = 1;
         }
         CHECK(!leak,    "completed: no previous-load bytes past avail");
         CHECK(!nonzero, "completed: tail past avail reads zeros");
      }
      data_transfer_free(dt);
   }

   /* ---- paused mid-fill ---- */
   {
      data_transfer_t *dt;
      const uint8_t *p;
      size_t i, avail, armed_end;
      int calls = 0;

      prime_slot_with_previous_load("pool_prev.bin");

      dt = data_transfer_open_prefix("pool_cur.bin", 0);
      CHECK(dt != NULL, "current load opens (pause case)");
      data_transfer_iterate_while(dt, 0, stop_after_first_read, &calls);
      avail = data_transfer_avail(dt);
      CHECK(avail > 0 && avail < len_cur, "fill paused mid-file");

      p = data_transfer_ptr(dt, NULL);

      /* the first commit arms the whole sub-slot file, so everything
       * from avail to the page-ceiled length is readable at the
       * pause and must be zeros */
      armed_end = ((len_cur + page - 1) / page) * page;
      {
         int leak = 0, nonzero = 0;
         for (i = avail; i < armed_end; i++)
         {
            if (p[i] == 0xAA)
               leak = 1;
            if (p[i] != 0x00)
               nonzero = 1;
         }
         CHECK(!leak,    "paused: no previous-load bytes past avail");
         CHECK(!nonzero, "paused: armed-unfilled span reads zeros");
      }

      for (i = 0; i < avail; i++)
         if (p[i] != 0xBB)
            break;
      CHECK(i == avail, "paused: prefix below avail is the file's own bytes");

      /* resume: the file must win over the zeros */
      fill_to_terminal(dt);
      CHECK(data_transfer_complete(dt), "resumed load completes");
      for (i = 0; i < len_cur; i++)
         if (p[i] != 0xBB)
            break;
      CHECK(i == len_cur, "resume overwrote the scrubbed span with file bytes");
      data_transfer_free(dt);
   }

   /* ---- capped terminal ---- */
   {
      data_transfer_t *dt;
      const uint8_t *p;
      size_t i, cap = 128 * 1024, scan_end;

      prime_slot_with_previous_load("pool_prev.bin");

      dt = data_transfer_open_prefix("pool_cur.bin", cap);
      CHECK(dt != NULL, "current load opens (cap case)");
      fill_to_terminal(dt);
      CHECK(data_transfer_capped(dt), "cap reached");

      p = data_transfer_ptr(dt, NULL);
      /* the commit step can round the armed span a little past the
       * cap; scan to the page-ceiled file length, which bounds it */
      scan_end = ((len_cur + page - 1) / page) * page;
      {
         int leak = 0;
         for (i = data_transfer_avail(dt); i < scan_end; i++)
         {
            if (p[i] == 0xAA)
            {
               leak = 1;
               break;
            }
         }
         CHECK(!leak, "capped: no previous-load bytes past avail");
      }
      data_transfer_free(dt);
   }

   data_transfer_pool_flush();
   remove("pool_prev.bin");
   remove("pool_cur.bin");

   if (test_fails)
   {
      printf("== %d FAILURES ==\n", test_fails);
      return 1;
   }
   printf("== data_transfer_pool_scrub_test: all tests pass ==\n");
   return 0;
}

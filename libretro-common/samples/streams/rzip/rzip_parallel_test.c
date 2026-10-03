/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rzip_parallel_test.c).
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

/* Regression test for the parallel chunk writer in rzip_stream.c.
 *
 * The writer pool is the only threaded code in rzip, and the other
 * rzip samples build without HAVE_THREADS, so nothing else exercises
 * it.  This test builds rzip_stream.c with HAVE_THREADS and drives
 * the pool at every width that matters: the serial fallback (one
 * core), the smallest pool (two), a full pool (eight) and a core
 * count past RZIP_MAX_THREADS (the clamp).  The pool sizes itself
 * from cpu_features_get_core_amount(), which this test replaces via
 * the linker's --wrap so the width is chosen here rather than by the
 * runner.
 *
 * What is checked:
 *   - every pool width writes a file byte-identical to the serial
 *     writer, both codecs, so in-order emission holds under any
 *     completion order;
 *   - a stream written in several calls, mixing whole-chunk runs
 *     with sub-chunk tails, matches a single-call write - the pool
 *     persists across calls and parked workers wake for the next run;
 *   - the file reads back to the input;
 *   - many short pool lifetimes, so the start/park/shutdown edges run
 *     often enough for TSan to see a lost wakeup or a racing free.
 *
 * Build:  make            (SANITIZER=thread or address,undefined for a checked run)
 * Run:    ./rzip_parallel_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>
#include <streams/rzip_stream.h>
#include <streams/file_stream.h>
#include <features/features_cpu.h>

/* Core count the pool sees; set per case. */
static unsigned fake_cores = 1;

unsigned __wrap_cpu_features_get_core_amount(void)
{
   return fake_cores;
}

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

/* Savestate-shaped: runs of zeros, structured pages and noise, so
 * chunks compress to different sizes and finish out of order. */
static void fill_body(uint8_t *b, size_t n)
{
   uint32_t x = 0x9E3779B9u;
   size_t i, j;
   for (i = 0; i < n; i += 4096)
   {
      unsigned kind;
      x ^= x << 13; x ^= x >> 17; x ^= x << 5;
      kind = x % 10;
      for (j = 0; j < 4096 && i + j < n; j++)
      {
         if (kind < 4)
            b[i + j] = 0;
         else if (kind < 8)
         {
            x ^= x << 13; x ^= x >> 17; x ^= x << 5;
            b[i + j] = (j & 1) ? (uint8_t)(x & 15) : (uint8_t)(j >> 4);
         }
         else
         {
            x ^= x << 13; x ^= x >> 17; x ^= x << 5;
            b[i + j] = (uint8_t)x;
         }
      }
   }
}

static bool write_one_call(const char *path, const uint8_t *body, size_t len)
{
   bool ok;
   rzipstream_t *s = rzipstream_open(path, RETRO_VFS_FILE_ACCESS_WRITE);
   if (!s)
      return false;
   ok = (rzipstream_write(s, body, (int64_t)len) == (int64_t)len);
   return (rzipstream_close(s) == 0) && ok;
}

/* Pieces sized to hit every branch of rzipstream_write: a sub-chunk
 * head that goes through in_buf, a long whole-chunk run that goes to
 * the pool, a single whole chunk (below the two-chunk pool floor),
 * another pool run, and a tail. */
static bool write_pieces(const char *path, const uint8_t *body, size_t len,
      size_t chunk)
{
   static const size_t plan[] = { 3, 7, 1, 5 };
   size_t pos = 0;
   unsigned i;
   bool ok = true;
   rzipstream_t *s = rzipstream_open(path, RETRO_VFS_FILE_ACCESS_WRITE);
   if (!s)
      return false;

   /* sub-chunk head */
   if (rzipstream_write(s, body, 1000) != 1000)
      ok = false;
   pos = 1000;
   /* finish the cached chunk so the runs below start aligned */
   if (rzipstream_write(s, body + pos, (int64_t)(chunk - 1000)) != (int64_t)(chunk - 1000))
      ok = false;
   pos = chunk;

   for (i = 0; i < sizeof(plan) / sizeof(plan[0]) && ok; i++)
   {
      size_t n = plan[i] * chunk;
      if (pos + n > len)
         n = len - pos;
      if (rzipstream_write(s, body + pos, (int64_t)n) != (int64_t)n)
         ok = false;
      pos += n;
   }
   if (ok && pos < len)
   {
      if (rzipstream_write(s, body + pos, (int64_t)(len - pos)) != (int64_t)(len - pos))
         ok = false;
   }
   return (rzipstream_close(s) == 0) && ok;
}

static uint8_t *slurp(const char *path, int64_t *len)
{
   uint8_t *buf = NULL;
   RFILE *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!f)
      return NULL;
   *len = filestream_get_size(f);
   buf  = (uint8_t*)malloc((size_t)*len);
   if (buf && filestream_read(f, buf, *len) != *len)
   {
      free(buf);
      buf = NULL;
   }
   filestream_close(f);
   return buf;
}

static bool files_identical(const char *a, const char *b)
{
   int64_t la = 0, lb = 0;
   uint8_t *da = slurp(a, &la);
   uint8_t *db = slurp(b, &lb);
   bool same = da && db && (la == lb) && (memcmp(da, db, (size_t)la) == 0);
   free(da);
   free(db);
   return same;
}

static bool reads_back(const char *path, const uint8_t *body, size_t len)
{
   void *buf = NULL;
   int64_t got = 0;
   bool same;
   if (!rzipstream_read_file(path, &buf, &got))
      return false;
   same = ((size_t)got == len) && (memcmp(buf, body, len) == 0);
   free(buf);
   return same;
}

int main(void)
{
   /* Default chunk is 128 KiB; 40 chunks keeps every pool width
    * busy for several rounds of the slot window, plus a tail. */
   size_t   chunk = 128 * 1024;
   size_t   len   = 40 * chunk + 4321;
   uint8_t *body  = (uint8_t*)malloc(len);
   static const unsigned widths[] = { 2, 4, 8, 64 };
   unsigned codec;
   unsigned w;
   unsigned round;

   fill_body(body, len);

   for (codec = 0; codec < 2; codec++)
   {
      enum rzip_codec c = codec ? RZIP_CODEC_ZSTD : RZIP_CODEC_DEFLATE;
      if (!rzipstream_codec_available(c))
         continue;
      rzipstream_set_write_codec(c);
      printf("-- %s --\n", codec ? "zstd" : "deflate");

      fake_cores = 1;
      if (!write_one_call("rzip_par.serial", body, len))
      {
         printf("FAIL: could not write the serial reference\n");
         return 1;
      }
      CHECK(reads_back("rzip_par.serial", body, len),
            "serial reference reads back");

      for (w = 0; w < sizeof(widths) / sizeof(widths[0]); w++)
      {
         char msg[96];
         fake_cores = widths[w];

         sprintf(msg, "%u cores: single write is byte-identical to serial",
               widths[w]);
         CHECK(write_one_call("rzip_par.par", body, len)
               && files_identical("rzip_par.serial", "rzip_par.par"), msg);

         sprintf(msg, "%u cores: pieced write is byte-identical to serial",
               widths[w]);
         CHECK(write_pieces("rzip_par.par", body, len, chunk)
               && files_identical("rzip_par.serial", "rzip_par.par"), msg);

         sprintf(msg, "%u cores: parallel output reads back", widths[w]);
         CHECK(reads_back("rzip_par.par", body, len), msg);
      }
   }

   /* Short lifetimes: pool up, two chunks through, pool down. */
   rzipstream_set_write_codec(RZIP_CODEC_DEFLATE);
   fake_cores = 4;
   {
      bool ok = true;
      for (round = 0; round < 200 && ok; round++)
         ok = write_one_call("rzip_par.par", body, 2 * chunk + 1);
      CHECK(ok, "200 short pool lifetimes complete");
      CHECK(reads_back("rzip_par.par", body, 2 * chunk + 1),
            "last short-lifetime file reads back");
   }

   free(body);
   remove("rzip_par.serial");
   remove("rzip_par.par");

   if (test_fails)
   {
      printf("== %d FAILURES ==\n", test_fails);
      return 1;
   }
   printf("== rzip_parallel_test: all tests pass ==\n");
   return 0;
}

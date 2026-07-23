/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (data_transfer_window_test.c).
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

/* Regression test for data_transfer's cyclic window mode.
 *
 * Window mode backs looping background music: the consumer sees one
 * stable base for the whole file, but only the head - kept forever,
 * because a loop lands there - plus a moving window around the play
 * position is actually resident.  That makes its contract different
 * from the other two modes in a way worth pinning down.
 *
 *   - the head must be readable from the moment the window opens, and
 *     must still be readable after a lap, since the jump back happens
 *     on the audio thread before any feeder tick runs.
 *   - whatever the window covers must match the file, on the first
 *     lap and on a second one, where the bytes are re-read rather
 *     than retained.
 *   - extending past the end must clamp, not fail.
 *   - grow_keep must enlarge the permanently resident head without
 *     disturbing what is already in it.
 *   - and the point of the mode: residency must stay bounded by the
 *     window rather than tracking the file, which the last check
 *     measures directly where the platform can report it.
 *
 * The test reports whether this build can reserve address space. Where
 * it cannot, window mode degrades to holding the whole file and the
 * correctness checks still apply - only the residency bound does not.
 *
 * Build:  make            (SANITIZER=address,undefined for a checked run)
 * Run:    ./data_transfer_window_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <formats/data_transfer.h>
#if defined(__linux__)
#include <unistd.h>
#endif

static int bad = 0;
static void ok(const char *w){ printf("[ok]   %s\n", w); }
static void fail(const char *w){ printf("[FAIL] %s\n", w); bad = 1; }

#define KEEP     (64u * 1024)
#define LOOKAHD  (256u * 1024)
#define MARGIN   (64u * 1024)


#if defined(__linux__)
static size_t rss_kb(void)
{
   FILE *f = fopen("/proc/self/statm", "r");
   long size = 0, resident = 0;
   if (!f)
      return 0;
   if (fscanf(f, "%ld %ld", &size, &resident) != 2)
      resident = 0;
   fclose(f);
   return (size_t)resident * (size_t)(sysconf(_SC_PAGESIZE) / 1024);
}

/* Play a large file through a small window and watch the resident set:
 * it must stay near the window rather than near the file. */
static int residency_check(void)
{
   const char *path = "/tmp/dtwin_resident.bin";
   size_t n = 64u << 20, i, tell, before, peak = 0;
   uint8_t *buf = (uint8_t*)malloc(n);
   data_transfer_t *dt; const uint8_t *base; size_t blen = 0;
   FILE *f;
   if (!buf)
      return 0;
   for (i = 0; i < n; i++)
      buf[i] = (uint8_t)i;
   f = fopen(path, "wb"); fwrite(buf, 1, n, f); fclose(f);
   free(buf);                       /* the reference copy must not count */

   before = rss_kb();
   if (!(dt = data_transfer_open_window(path, KEEP)))
   { remove(path); return 0; }
   base = data_transfer_window_base(dt, &blen);
   for (tell = 0; tell < n; tell += 65536)
   {
      size_t r;
      data_transfer_window_feed(dt, tell, LOOKAHD, MARGIN);
      { volatile uint8_t s = base[tell]; (void)s; }
      if ((r = rss_kb()) > peak)
         peak = r;
   }
   data_transfer_free(dt);
   remove(path);
   {
      size_t grew  = (peak > before) ? peak - before : 0;
      size_t bound = (n >> 10) / 4;   /* a quarter of the file, generous */
      if (grew >= bound)
      { printf("[FAIL] residency grew %zu KiB for a %zu MiB file\n",
               grew, n >> 20); return 1; }
      printf("[ok]   residency bound: grew %zu KiB for a %zu MiB file\n",
             grew, n >> 20);
   }
   return 0;
}
#endif

int main(void)
{
   const char *path = "/tmp/dtwin.bin";
   size_t n = 4u << 20, i, tell;
   uint8_t *ref = malloc(n);
   data_transfer_t *dt;
   const uint8_t *base; size_t blen = 0;
   FILE *f;

   for (i = 0; i < n; i++) ref[i] = (uint8_t)(i * 211 + 13);
   f = fopen(path, "wb"); fwrite(ref, 1, n, f); fclose(f);

   printf("reservations %s on this build\n",
         data_transfer_reserve_supported() ? "supported (true windowing)"
                                           : "unavailable (whole-file fallback)");

   if (!(dt = data_transfer_open_window(path, KEEP)))
   { fail("open_window"); return 1; }

   base = data_transfer_window_base(dt, &blen);
   if (blen != n) fail("window base length");

   /* the head is resident from the start */
   if (memcmp(base, ref, KEEP)) fail("head contents after open");
   else ok("head resident and correct immediately after open");

   /* play forward, feeding as a decoder would */
   for (tell = 0; tell < n; tell += 32768)
   {
      size_t want = (tell + 32768 <= n) ? 32768 : n - tell;
      if (!data_transfer_window_feed(dt, tell, LOOKAHD, MARGIN))
      { fail("feed during forward play"); break; }
      if (memcmp(base + tell, ref + tell, want))
      { fail("window contents during forward play"); break; }
   }
   if (!bad) ok("forward play: every chunk matched the file");

   /* loop back to the start: a backwards tell means a new lap */
   if (!data_transfer_window_feed(dt, 0, LOOKAHD, MARGIN))
      fail("feed after loop");
   else if (memcmp(base, ref, KEEP))
      fail("head contents after loop");
   else ok("loop back to 0: head still correct");

   /* second lap re-reads from the file rather than holding memory */
   for (tell = 0; tell < n / 2; tell += 65536)
   {
      if (!data_transfer_window_feed(dt, tell, LOOKAHD, MARGIN))
      { fail("feed during second lap"); break; }
      if (memcmp(base + tell, ref + tell, 65536))
      { fail("window contents during second lap"); break; }
   }
   if (!bad) ok("second lap: contents re-read correctly");

   /* extend past the end clamps rather than failing */
   if (!data_transfer_window_extend(dt, n + (1u << 20)))
      fail("extend past end refused");
   else ok("extend past the end clamps to the length");

   /* growing the permanently-resident head keeps it readable */
   if (!data_transfer_window_grow_keep(dt, KEEP * 2))
      fail("grow_keep refused");
   else if (memcmp(base, ref, KEEP * 2))
      fail("head contents after grow_keep");
   else ok("grow_keep: enlarged head is correct");

   data_transfer_free(dt);
   free(ref); remove(path);

   /* The point of the mode: a long file must not become resident.
    * Measured only where the platform reports a resident set, and
    * only meaningful when address space can actually be reserved. */
#if defined(__linux__)
   if (data_transfer_reserve_supported())
      bad |= residency_check();
   else
      printf("[skip] residency bound: no reservations on this build\n");
#else
   printf("[skip] residency bound: not measurable on this platform\n");
#endif

   printf("%s\n", bad ? "FAILED" : "PASS");
   return bad;
}

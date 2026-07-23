/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (data_transfer_prefix_test.c).
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

/* Regression test for data_transfer's file-backed (prefix) mode.
 *
 * Prefix mode is what lets a consumer pay for only the part of a file
 * it actually needs - a thumbnail decoder reading a header, an audio
 * decoder playing a long file in a constant residency window - so its
 * contract is more than "the bytes arrive".
 *
 *   - A budgeted fill must still deliver the whole file, unchanged.
 *   - discard() releases the physical backing of bytes below a point
 *     and those bytes are then gone; refill() must bring them back
 *     with the file's real contents, because a consumer that seeks
 *     backwards depends on it.
 *   - commit_cap is a ceiling with its own terminal: a transfer that
 *     reaches it reports capped(), which is not complete(), so a
 *     caller can tell "there is more file" from "that was all of it".
 *     A cap at or above the length must not turn a normal completion
 *     into a capped one.
 *   - a file that is not there must be refused at open rather than
 *     presented as an empty success.
 *
 * Build:  make            (SANITIZER=address,undefined for a checked run)
 * Run:    ./data_transfer_prefix_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <formats/data_transfer.h>

static int bad = 0;
static void ok(const char *what) { printf("[ok]   %s\n", what); }
static void fail(const char *what) { printf("[FAIL] %s\n", what); bad = 1; }

static void mkfile(const char *path, uint8_t *ref, size_t n)
{
   FILE *f = fopen(path, "wb"); size_t i;
   for (i = 0; i < n; i++) ref[i] = (uint8_t)(i * 167 + 29);
   if (n) fwrite(ref, 1, n, f);
   fclose(f);
}

int main(void)
{
   const char *path = "/tmp/dtprefix.bin";
   size_t n = 3u << 20;                 /* 3 MiB */
   uint8_t *ref = malloc(n);
   mkfile(path, ref, n);

   /* 1. budgeted fill delivers the file, contents intact */
   {
      data_transfer_t *dt = data_transfer_open_prefix(path, 0);
      size_t total = 0; const uint8_t *base; int ticks = 0;
      if (!dt) { fail("open_prefix"); return 1; }
      base = data_transfer_ptr(dt, &total);
      if (total != n) fail("declared length");
      while (!data_transfer_complete(dt) && !data_transfer_failed(dt)
             && ticks < 100000)
      { data_transfer_iterate(dt, 65536); ticks++; }
      if (!data_transfer_complete(dt))         fail("never completed");
      else if (data_transfer_avail(dt) != n)   fail("avail != length");
      else if (memcmp(base, ref, n))           fail("contents differ");
      else ok("budgeted fill: whole file, contents intact");
      data_transfer_free(dt);
   }

   /* 2. discard then refill restores the released bytes */
   {
      data_transfer_t *dt = data_transfer_open_prefix(path, 0);
      size_t total = 0; const uint8_t *base = data_transfer_ptr(dt, &total);
      while (!data_transfer_complete(dt) && !data_transfer_failed(dt))
         data_transfer_iterate(dt, 0);
      data_transfer_discard(dt, 1u << 20);
      if (!data_transfer_refill(dt, 0))        fail("refill refused");
      else if (memcmp(base, ref, n))           fail("refill restored wrong bytes");
      else ok("discard then refill: bytes restored correctly");
      data_transfer_free(dt);
   }

   /* 3. commit_cap stops at the ceiling and reports capped, not complete */
   {
      size_t cap = 1u << 20;
      data_transfer_t *dt = data_transfer_open_prefix(path, cap);
      int ticks = 0;
      if (!dt) { fail("open_prefix with cap"); return 1; }
      while (!data_transfer_complete(dt) && !data_transfer_failed(dt)
             && !data_transfer_capped(dt) && ticks < 100000)
      { data_transfer_iterate(dt, 65536); ticks++; }
      if (!data_transfer_capped(dt))        fail("cap not reported");
      else if (data_transfer_complete(dt))  fail("capped and complete at once");
      else if (data_transfer_avail(dt) > cap) fail("filled past the cap");
      else ok("commit_cap: stops at the ceiling, distinct from complete");
      data_transfer_free(dt);
   }

   /* 4. a cap at or above the length still completes normally */
   {
      data_transfer_t *dt = data_transfer_open_prefix(path, n);
      while (!data_transfer_complete(dt) && !data_transfer_failed(dt)
             && !data_transfer_capped(dt))
         data_transfer_iterate(dt, 0);
      if (!data_transfer_complete(dt) || data_transfer_capped(dt))
         fail("cap == length should complete, not cap");
      else ok("cap equal to the length completes normally");
      data_transfer_free(dt);
   }

   /* 5. a vanished file fails rather than reporting a short success */
   {
      const char *gone = "/tmp/dtprefix_missing.bin";
      data_transfer_t *dt;
      remove(gone);
      dt = data_transfer_open_prefix(gone, 0);
      if (dt) { fail("opened a file that does not exist"); data_transfer_free(dt); }
      else ok("missing file refused at open");
   }

   free(ref); remove(path);
   printf("%s\n", bad ? "FAILED" : "PASS");
   return bad;
}

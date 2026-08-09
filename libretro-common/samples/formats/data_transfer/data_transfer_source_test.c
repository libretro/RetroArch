/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (data_transfer_source_test.c).
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

/* Regression test for data_transfer's producer-backed (source) mode.
 *
 * Source mode is what carries content into memory: the plain-file and
 * archive-entry load paths, the content prefetch task, and the audio
 * and image layers all pull through it, and every one of them adopts
 * the detached buffer and frees it themselves.  The producer behind it
 * is a decoder, so it is exactly the component most likely to
 * misbehave under a damaged input - stopping early, reporting an
 * error part way, or claiming to have produced more than it wrote.
 *
 * The invariants that matter to those callers:
 *
 *   - detach() yields a buffer only when the whole declared length
 *     arrived.  A caller that adopts a short buffer would treat
 *     truncated content as complete.
 *   - a transfer that failed never reports complete(), so a caller
 *     polling those two flags cannot mistake one for the other.
 *   - avail() never exceeds the declared length however much the
 *     producer claims to have written.
 *   - nothing is written outside the buffer, and nothing leaks on any
 *     of those paths - build with SANITIZER=address,undefined and the
 *     sanitizers judge that part.
 *
 * Build:  make            (SANITIZER=address,undefined for a checked run)
 * Run:    ./data_transfer_source_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <formats/data_transfer.h>

static unsigned R = 424242;
static unsigned xr(void){ R^=R<<13; R^=R>>17; R^=R<<5; return R; }

enum mode { M_GOOD, M_SHORT, M_ERROR, M_OVER, M_ZERO_EARLY, M_TRICKLE, M_BURST };
struct prod { size_t len, pos; int mode; int calls; };

static int64_t prod(void *ud, uint8_t *dst, size_t n)
{
   struct prod *p = (struct prod*)ud;
   size_t give;
   p->calls++;
   switch (p->mode)
   {
      case M_ERROR:       if (p->calls > 2) return -1; break;
      case M_ZERO_EARLY:  if (p->calls > 2) return 0;  break;
      case M_SHORT:       if (p->pos > p->len / 2) return 0; break;
      default: break;
   }
   if (p->pos >= p->len) return 0;
   switch (p->mode)
   {
      case M_TRICKLE: give = 1; break;
      case M_BURST:   give = n; break;
      case M_OVER:
         /* claim more than written - a broken producer */
         give = (p->len - p->pos < n) ? p->len - p->pos : n;
         memset(dst, 0xAB, give);
         p->pos += give;
         return (int64_t)(give + 4096);      /* lie */
      default:        give = 1 + (xr() % (n ? n : 1)); break;
   }
   if (give > n) give = n;
   if (give > p->len - p->pos) give = p->len - p->pos;
   memset(dst, 0xAB, give);
   p->pos += give;
   return (int64_t)give;
}

static int one(size_t len, int mode, const char *label, int expect_ok)
{
   struct prod p; data_transfer_t *dt; uint8_t *out; size_t got = 0;
   int bad = 0;
   p.len = len; p.pos = 0; p.mode = mode; p.calls = 0;
   if (!(dt = data_transfer_open_source(len, prod, &p))) return 0;
   while (!data_transfer_complete(dt) && !data_transfer_failed(dt))
      data_transfer_iterate(dt, 4096);
   if (data_transfer_avail(dt) > len)
   { printf("[FAIL] %-28s avail %zu > len %zu\n", label,
            data_transfer_avail(dt), len); bad = 1; }
   if (data_transfer_failed(dt) && data_transfer_complete(dt))
   { printf("[FAIL] %-28s failed and complete at once\n", label); bad = 1; }
   out = data_transfer_source_detach(dt, &got);
   if (expect_ok && (!out || got != len))
   { printf("[FAIL] %-28s expected a full buffer, got %p/%zu\n",
            label, (void*)out, got); bad = 1; }
   if (!expect_ok && out)
   { printf("[FAIL] %-28s handed out a buffer after failure\n", label); bad = 1; }
   if (out) free(out); else data_transfer_free(dt);
   if (!bad) printf("[ok]   %-28s %s\n", label,
                    expect_ok ? "full buffer detached" : "refused, nothing handed out");
   return bad;
}

int main(void)
{
   int bad = 0, i;
   size_t sizes[] = {1, 4095, 4096, 4097, 65536, 1u<<20};
   for (i = 0; i < 6; i++)
   {
      char lbl[64];
      snprintf(lbl, sizeof(lbl), "well-behaved %zu", sizes[i]);
      bad |= one(sizes[i], M_GOOD, lbl, 1);
   }
   bad |= one(65536, M_TRICKLE,    "one byte per call",      1);
   bad |= one(65536, M_BURST,      "always fills the hint",  1);
   bad |= one(65536, M_SHORT,      "stops half way",         0);
   bad |= one(65536, M_ERROR,      "errors mid-stream",      0);
   bad |= one(65536, M_ZERO_EARLY, "returns 0 early",        0);
   bad |= one(65536, M_OVER,       "over-reports its write", 0);
   printf("%s\n", bad ? "FAILED" : "PASS");
   return bad;
}

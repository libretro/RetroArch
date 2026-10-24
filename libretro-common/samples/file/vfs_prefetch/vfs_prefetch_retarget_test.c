/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vfs_prefetch_retarget_test.c).
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


/* A seek issued while the prefetch thread has not yet taken an earlier
 * retarget is judged against where the stream is headed, not where the
 * thread last was: open, a seek to the end (the size probe every open
 * does), then back to the start, all while the first window is still
 * being fetched. The windows must end up at the start, where the
 * reader is. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <retro_atomic.h>
#include <retro_timers.h>
#include <vfs/vfs_prefetch.h>

#define WINDOW   1024
#define FILE_LEN 200000

static retro_atomic_int_t gate_open;
static retro_atomic_int_t entered;
static retro_atomic_int_t calls;

static uint8_t pattern(uint64_t i)
{
   return (uint8_t)(i * 31 + 7);
}

/* A file of FILE_LEN octets; the first fetch holds until the gate
 * opens, so the reader's seeks land while it is in flight. */
static int64_t fetch(void *user, uint64_t off, uint8_t *buf, size_t len)
{
   size_t i;
   (void)user;
   if (retro_atomic_fetch_add_int(&calls, 1) == 0)
   {
      retro_atomic_store_release_int(&entered, 1);
      while (!retro_atomic_load_acquire_int(&gate_open))
         retro_sleep(1);
   }
   if (off >= FILE_LEN)
      return 0;
   if (len > FILE_LEN - off)
      len = (size_t)(FILE_LEN - off);
   for (i = 0; i < len; i++)
      buf[i] = pattern(off + i);
   return (int64_t)len;
}

int main(void)
{
   struct vfs_prefetch *p;
   uint8_t  buf[16];
   int64_t  got;
   int      waited, last, now, i;

   retro_atomic_store_release_int(&gate_open, 0);
   retro_atomic_store_release_int(&entered, 0);
   retro_atomic_store_release_int(&calls, 0);

   if (!(p = vfs_prefetch_new(fetch, NULL, WINDOW, 2, 0)))
   {
      printf("[SKIP] no prefetch thread on this build\n");
      return 0;
   }
   for (waited = 0; !retro_atomic_load_acquire_int(&entered) && waited < 5000; waited++)
      retro_sleep(1);
   if (!retro_atomic_load_acquire_int(&entered))
   {
      printf("[FAIL] the prefetch thread never fetched\n");
      vfs_prefetch_free(p);
      return 1;
   }

   vfs_prefetch_seek(p, FILE_LEN - 1);   /* the size probe */
   vfs_prefetch_seek(p, 0);              /* and back */
   retro_atomic_store_release_int(&gate_open, 1);

   /* until the thread goes quiet */
   last = -1;
   for (waited = 0; waited < 60; waited++)
   {
      retro_sleep(50);
      now = retro_atomic_load_acquire_int(&calls);
      if (now == last)
         break;
      last = now;
   }

   got = vfs_prefetch_read(p, 0, buf, sizeof(buf));
   vfs_prefetch_free(p);
   if (got != (int64_t)sizeof(buf))
   {
      printf("[FAIL] after open, end, start: no window at the start (read %lld)\n",
            (long long)got);
      return 1;
   }
   for (i = 0; i < (int)sizeof(buf); i++)
      if (buf[i] != pattern((uint64_t)i))
      {
         printf("[FAIL] window at the start holds the wrong octets\n");
         return 1;
      }
   printf("[PASS] a seek behind a pending retarget brings the windows to the reader\n");
   return 0;
}

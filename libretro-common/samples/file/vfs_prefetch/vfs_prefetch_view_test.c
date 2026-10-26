/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vfs_prefetch_view_test.c).
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


/* vfs_prefetch_view(): the windows read ahead, lent in place. A view
 * points into a ready window and runs to its end; the end of the file
 * views as 0 and a miss as -1, which retargets the thread there. The
 * window a view points into is the reader's: nothing the thread does
 * changes it before the reader's next call, however far ahead the
 * thread runs meanwhile. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <retro_timers.h>
#include <vfs/vfs_prefetch.h>

#define WINDOW   4096
#define SLOTS    4
#define FILE_LEN (WINDOW * 64 + 100)

static int fails;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL: %s\n", msg); \
         fails++; \
      } \
   } while (0)

static uint8_t pattern(uint64_t i)
{
   return (uint8_t)((i * 2654435761u) >> 11);
}

static int64_t fetch(void *user, uint64_t off, uint8_t *buf, size_t len)
{
   size_t i;
   (void)user;
   if (off >= FILE_LEN)
      return 0;
   if (len > FILE_LEN - off)
      len = (size_t)(FILE_LEN - off);
   for (i = 0; i < len; i++)
      buf[i] = pattern(off + i);
   return (int64_t)len;
}

/* Views @off, waiting out misses while the thread gets there. */
static int64_t view_wait(struct vfs_prefetch *p, uint64_t off,
      const uint8_t **ptr)
{
   int64_t got = -1;
   int     i;
   for (i = 0; i < 5000 && got < 0; i++)
      if ((got = vfs_prefetch_view(p, off, ptr)) < 0)
         retro_sleep(1);
   return got;
}

static int same(const uint8_t *ptr, uint64_t off, size_t len)
{
   size_t i;
   for (i = 0; i < len; i++)
      if (ptr[i] != pattern(off + i))
         return 0;
   return 1;
}

int main(void)
{
   struct vfs_prefetch *p = vfs_prefetch_new(fetch, NULL, WINDOW, SLOTS, 0);
   const uint8_t       *ptr = NULL;
   uint64_t             off;
   int64_t              got;
   int                  ok = 1;

   if (!p)
   {
      printf("SKIP: no threads\n");
      return 0;
   }

   got = view_wait(p, 100, &ptr);
   CHECK(got == WINDOW - 100 && same(ptr, 100, (size_t)got),
         "a view runs from the offset to the end of its window");

   /* hold a view while the thread fills the windows behind it, then
    * check it is untouched; walk the file this way in steps */
   for (off = 0; ok && off < FILE_LEN; off += WINDOW / 2)
   {
      got = view_wait(p, off, &ptr);
      if (got <= 0)
      {
         ok = 0;
         break;
      }
      retro_sleep(1);
      ok = same(ptr, off, (size_t)got);
   }
   CHECK(ok, "a held view is not changed by the thread running ahead");

   got = view_wait(p, FILE_LEN, &ptr);
   CHECK(got == 0, "the end of the file views as 0");

   got = vfs_prefetch_view(p, WINDOW * 3 + 7, &ptr);
   CHECK(got < 0, "a view behind the windows misses");
   got = view_wait(p, WINDOW * 3 + 7, &ptr);
   CHECK(got == WINDOW - 7 && same(ptr, WINDOW * 3 + 7, (size_t)got),
         "and the miss brings the thread back there");

   CHECK(vfs_prefetch_view(NULL, 0, &ptr) < 0
         && vfs_prefetch_view(p, 0, NULL) < 0, "NULL arguments miss");

   vfs_prefetch_free(p);
   if (fails)
   {
      printf("%d FAIL\n", fails);
      return 1;
   }
   printf("vfs_prefetch_view_test: PASS\n");
   return 0;
}

/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (chd_stream_threads_test.c).
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

/* Two CHD streams opened and read on two threads at once: each must
 * read back exactly its own image. The images are uncompressed
 * version 5 CD images written here, one data track each, with sector
 * contents that differ between the two files. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <rthreads/rthreads.h>
#include <streams/chd_stream.h>
#include <streams/file_stream.h>

#define UNIT_BYTES   2448
#define SECTOR_BYTES 2352
#define FRAMES       64
#define HUNK_FRAMES  8
#define HUNK_BYTES   (UNIT_BYTES * HUNK_FRAMES)
#define HUNKS        (FRAMES / HUNK_FRAMES)
#define ROUNDS       40

static const char *meta_text =
   "TRACK:1 TYPE:MODE1_RAW SUBTYPE:NONE FRAMES:64 PREGAP:0 "
   "PGTYPE:MODE1 PGSUB:NONE POSTGAP:0";

static uint8_t pattern(unsigned seed, uint32_t frame, uint32_t i)
{
   return (uint8_t)(((frame * 2654435761u) ^ (i * 40503u) ^ seed) >> 3);
}

static void put32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
   p[2] = (uint8_t)(v >>  8); p[3] = (uint8_t)v;
}

static void put64(uint8_t *p, uint64_t v)
{
   put32(p,     (uint32_t)(v >> 32));
   put32(p + 4, (uint32_t)v);
}

/* Hunk i lives at (i + 1) * HUNK_BYTES: map value 0 is a hole, so no
 * real hunk can sit where the header does. Metadata follows the data. */
static bool write_image(const char *path, unsigned seed)
{
   size_t   meta_len = strlen(meta_text) + 1;
   size_t   meta_off = (size_t)(HUNKS + 1) * HUNK_BYTES;
   size_t   len      = meta_off + 16 + meta_len;
   uint8_t *img      = (uint8_t*)calloc(1, len);
   uint32_t f, i;
   bool     ok;

   if (!img)
      return false;
   memcpy(img, "MComprHD", 8);
   put32(img + 8,  124);
   put32(img + 12, 5);
   put64(img + 32, (uint64_t)FRAMES * UNIT_BYTES);
   put64(img + 40, 124);
   put64(img + 48, meta_off);
   put32(img + 56, HUNK_BYTES);
   put32(img + 60, UNIT_BYTES);
   for (i = 0; i < HUNKS; i++)
      put32(img + 124 + i * 4, i + 1);
   for (f = 0; f < FRAMES; f++)
   {
      uint8_t *fr = img + (size_t)(f / HUNK_FRAMES + 1) * HUNK_BYTES
                  + (size_t)(f % HUNK_FRAMES) * UNIT_BYTES;
      for (i = 0; i < SECTOR_BYTES; i++)
         fr[i] = pattern(seed, f, i);
   }
   memcpy(img + meta_off, "CHT2", 4);
   img[meta_off + 4] = 0x01;
   img[meta_off + 5] = (uint8_t)(meta_len >> 16);
   img[meta_off + 6] = (uint8_t)(meta_len >> 8);
   img[meta_off + 7] = (uint8_t)meta_len;
   memcpy(img + meta_off + 16, meta_text, meta_len);

   ok = filestream_write_file(path, img, (int64_t)len);
   free(img);
   return ok;
}

struct worker
{
   const char *path;
   unsigned    seed;
   int         fails;
};

static void read_image(void *data)
{
   struct worker *w = (struct worker*)data;
   uint8_t *sec     = (uint8_t*)malloc(SECTOR_BYTES);
   int      r;

   if (!sec)
   {
      w->fails++;
      return;
   }
   for (r = 0; r < ROUNDS; r++)
   {
      uint32_t     f, i;
      chdstream_t *s = chdstream_open(w->path, 1);

      if (!s)
      {
         w->fails++;
         continue;
      }
      for (f = 0; f < FRAMES; f++)
      {
         if (chdstream_read(s, sec, SECTOR_BYTES) != SECTOR_BYTES)
         {
            w->fails++;
            break;
         }
         for (i = 0; i < SECTOR_BYTES; i++)
            if (sec[i] != pattern(w->seed, f, i))
            {
               w->fails++;
               break;
            }
      }
      if (chdstream_getc(s) != EOF)
         w->fails++;
      chdstream_close(s);
   }
   free(sec);
}

int main(void)
{
   struct worker a, b;
   sthread_t    *ta, *tb;
   int           fails = 0;

   a.path = "chd_stream_threads_a.chd"; a.seed = 0x11; a.fails = 0;
   b.path = "chd_stream_threads_b.chd"; b.seed = 0xC7; b.fails = 0;

   if (!write_image(a.path, a.seed) || !write_image(b.path, b.seed))
   {
      printf("FAIL: could not write the images\n");
      return 1;
   }

   {
      chdstream_t *s = chdstream_open(a.path, 1);
      if (!s || chdstream_get_size(s) != (ssize_t)FRAMES * SECTOR_BYTES)
      {
         printf("FAIL: image does not open as one %u-frame track\n",
               FRAMES);
         return 1;
      }
      if (chdstream_getc(s) != (int)pattern(a.seed, 0, 0))
      {
         printf("FAIL: getc does not return the first octet\n");
         fails++;
      }
      chdstream_close(s);
   }

   ta = sthread_create(read_image, &a);
   tb = sthread_create(read_image, &b);
   if (!ta || !tb)
   {
      printf("FAIL: no threads\n");
      return 1;
   }
   sthread_join(ta);
   sthread_join(tb);

   fails += a.fails + b.fails;
   filestream_delete(a.path);
   filestream_delete(b.path);

   if (fails)
   {
      printf("%d FAIL\n", fails);
      return 1;
   }
   printf("chd_stream_threads_test: PASS\n");
   return 0;
}

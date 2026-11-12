/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vfs_seek_read_mix_test.c).
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

/* A read-only stream read the way parsers read it: seeks to random
 * places, reads of every size from 1 byte to past 64 KiB - some
 * across where a refill ends - runs of sequential reads after a seek,
 * seeks back into what was just read, seeks past the end, and the
 * position after each. Every byte is checked against the fixture, whose
 * bytes depend on where they lie. On Windows this is the stream's own
 * buffer sizing its refills by how it is being read. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <streams/file_stream.h>

#define FILE_BYTES ((3u << 20) + 1234)
#define ROUNDS     20000

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: check failed: %s\n", \
      __FILE__, __LINE__, #c); failures++; } } while (0)

static uint8_t at(uint64_t i)
{
   return (uint8_t)((i * 2654435761u) >> 11);
}

static int same(const uint8_t *p, uint64_t off, uint64_t n)
{
   uint64_t i;
   for (i = 0; i < n; i++)
      if (p[i] != at(off + i))
         return 0;
   return 1;
}

int main(void)
{
   static const char path[] = "vfs_seek_read_mix.bin";
   static const unsigned sizes[] = { 1, 3, 16, 100, 512, 4095, 4096, 4097,
      30000, 65535, 65536, 65537, 200000 };
   uint8_t *data = (uint8_t*)malloc(FILE_BYTES);
   uint8_t *buf  = (uint8_t*)malloc(200000);
   RFILE   *f;
   unsigned x = 4242, k;
   uint64_t i;

   if (!data || !buf)
      return 1;
   for (i = 0; i < FILE_BYTES; i++)
      data[i] = at(i);
   CHECK(filestream_write_file(path, data, FILE_BYTES));
   free(data);

   f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   CHECK(f != NULL);
   if (!f)
      return 1;
   CHECK(filestream_get_size(f) == FILE_BYTES);

   for (k = 0; k < ROUNDS; k++)
   {
      int64_t  pos, got, want, left;
      unsigned run, r;
      x   = x * 1103515245u + 12345u;
      switch ((x >> 28) & 3)
      {
         case 0:  /* anywhere */
            pos = (int64_t)((x >> 3) % FILE_BYTES);
            break;
         case 1:  /* back a little, into what was read */
            pos = filestream_tell(f) - (int64_t)((x >> 8) % 70000);
            if (pos < 0)
               pos = 0;
            break;
         case 2:  /* near the end, or past it */
            pos = FILE_BYTES - 70000 + (int64_t)((x >> 8) % 80000);
            break;
         default: /* on from where it is */
            pos = filestream_tell(f);
            break;
      }
      CHECK(filestream_seek(f, pos, RETRO_VFS_SEEK_POSITION_START) >= 0);
      CHECK(filestream_tell(f) == pos);
      run = 1 + ((x >> 12) & 7);
      for (r = 0; r < run; r++)
      {
         x    = x * 1103515245u + 12345u;
         want = (int64_t)sizes[(x >> 20) % (sizeof(sizes) / sizeof(sizes[0]))];
         left = pos < (int64_t)FILE_BYTES ? (int64_t)FILE_BYTES - pos : 0;
         got  = filestream_read(f, buf, want);
         CHECK(got == (want < left ? want : left));
         if (got > 0)
         {
            CHECK(same(buf, (uint64_t)pos, (uint64_t)got));
            pos += got;
         }
         CHECK(filestream_tell(f) == pos);
         if (failures > 20)
            break;
      }
      if (failures > 20)
         break;
   }

   /* and straight through, after all that */
   CHECK(filestream_seek(f, 0, RETRO_VFS_SEEK_POSITION_START) >= 0);
   for (i = 0; i < FILE_BYTES; )
   {
      int64_t got = filestream_read(f, buf, 7777);
      if (got <= 0 || !same(buf, i, (uint64_t)got))
      {
         failures++;
         break;
      }
      i += (uint64_t)got;
   }
   CHECK(i == FILE_BYTES);
   CHECK(filestream_read(f, buf, 10) == 0);

   filestream_close(f);
   remove(path);
   free(buf);
   printf("vfs seek/read mix: %s\n", failures ? "FAIL" : "ok");
   return failures ? 1 : 0;
}

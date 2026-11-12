/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (intfstream_read_at_test.c).
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

/* intfstream_read_at on file, buffered and memory streams:
 *
 * - the bytes at the offset, a short count at the end, 0 past it, -1
 *   for a negative offset;
 * - the stream's position where it was, so a sequential reader
 *   carries on undisturbed, the buffered stream's window included;
 * - the buffered stream's wide reads and window refills are right
 *   after reads at offsets elsewhere;
 * - file and memory streams read this way from four threads at once
 *   give every thread its own bytes (run under TSan in CI).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <streams/file_stream.h>
#include <streams/interface_stream.h>
#include <rthreads/rthreads.h>

#define FILE_BYTES  (256 * 1024)
#define WINDOW      4096
#define THREADS     4
#define ROUNDS      4000

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: check failed: %s\n", \
      __FILE__, __LINE__, #c); failures++; } } while (0)

static const char path[] = "intfstream_read_at.bin";
static uint8_t    data[FILE_BYTES];

/* Byte i of the fixture: position-dependent, so a read from the wrong
 * offset never passes */
static uint8_t at(uint64_t i)
{
   return (uint8_t)((i * 2654435761u) >> 13);
}

static int same(const uint8_t *p, uint64_t off, uint64_t n)
{
   uint64_t i;
   for (i = 0; i < n; i++)
      if (p[i] != at(off + i))
         return 0;
   return 1;
}

static void contract(intfstream_t *s, const char *what)
{
   uint8_t buf[9000];
   int64_t before, r;

   (void)what;
   CHECK(intfstream_seek(s, 100, SEEK_SET) >= 0);
   CHECK(intfstream_read(s, buf, 16) == 16 && same(buf, 100, 16));
   before = intfstream_tell(s);

   r = intfstream_read_at(s, buf, 777, 12345);
   CHECK(r == 777 && same(buf, 12345, 777));
   r = intfstream_read_at(s, buf, 8500, 100000);
   CHECK(r == 8500 && same(buf, 100000, 8500));
   r = intfstream_read_at(s, buf, 64, FILE_BYTES - 10);
   CHECK(r == 10 && same(buf, FILE_BYTES - 10, 10));
   CHECK(intfstream_read_at(s, buf, 64, FILE_BYTES + 5) == 0);
   CHECK(intfstream_read_at(s, buf, 64, -1) == -1);

   /* where it was, and the next sequential read follows on */
   CHECK(intfstream_tell(s) == before);
   CHECK(intfstream_read(s, buf, 32) == 32 && same(buf, 116, 32));
}

/* The buffered stream's window survives reads at offsets, and its
 * wide-read and refill paths stay right around them */
static void buffered_window(intfstream_t *s)
{
   uint8_t *big = (uint8_t*)malloc(3 * WINDOW);
   uint8_t  buf[64];
   if (!big)
   {
      failures++;
      return;
   }
   CHECK(intfstream_seek(s, 1000, SEEK_SET) >= 0);
   CHECK(intfstream_read(s, buf, 8) == 8 && same(buf, 1000, 8));
   CHECK(intfstream_read_at(s, buf, 64, 200000) == 64
         && same(buf, 200000, 64));
   CHECK(intfstream_read(s, buf, 8) == 8 && same(buf, 1008, 8));
   /* wider than the window: straight from the file */
   CHECK(intfstream_read(s, big, 3 * WINDOW) == 3 * WINDOW
         && same(big, 1016, 3 * WINDOW));
   CHECK(intfstream_read_at(s, buf, 64, 50) == 64 && same(buf, 50, 64));
   /* and on past the window's end */
   CHECK(intfstream_read(s, buf, 64) == 64
         && same(buf, 1016 + 3 * WINDOW, 64));
   CHECK(intfstream_seek(s, 40000, SEEK_SET) >= 0);
   CHECK(intfstream_read(s, buf, 64) == 64 && same(buf, 40000, 64));
   free(big);
}

#ifdef HAVE_THREADS
struct worker
{
   intfstream_t *s;
   unsigned      seed;
   int           wrong;
};

static void work(void *ud)
{
   struct worker *w = (struct worker*)ud;
   uint8_t buf[512];
   unsigned x = w->seed, k;
   for (k = 0; k < ROUNDS; k++)
   {
      uint64_t off, n;
      x   = x * 1103515245u + 12345u;
      off = (x >> 4) % (FILE_BYTES - sizeof(buf));
      n   = 1 + ((x >> 20) % sizeof(buf));
      if (     intfstream_read_at(w->s, buf, n, (int64_t)off) != (int64_t)n
            || !same(buf, off, n))
         w->wrong++;
   }
}

static void threads(intfstream_t *s, const char *what)
{
   sthread_t    *t[THREADS];
   struct worker w[THREADS];
   int i, wrong = 0;
   for (i = 0; i < THREADS; i++)
   {
      w[i].s     = s;
      w[i].seed  = 7u + (unsigned)i * 977u;
      w[i].wrong = 0;
      t[i]       = sthread_create(work, &w[i]);
   }
   for (i = 0; i < THREADS; i++)
   {
      if (t[i])
         sthread_join(t[i]);
      else
         failures++;
      wrong += w[i].wrong;
   }
   printf("intfstream read_at: %s: %d reads on %d threads, %d wrong\n",
         what, THREADS * ROUNDS, THREADS, wrong);
   CHECK(wrong == 0);
}
#endif

int main(void)
{
   intfstream_t *s;
   RFILE        *f;
   uint64_t      i;
   int           positional;
   uint8_t       one;

   for (i = 0; i < FILE_BYTES; i++)
      data[i] = at(i);
   f = filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   CHECK(f && filestream_write(f, data, FILE_BYTES) == FILE_BYTES);
   filestream_close(f);

   /* threads only where the file reads at offsets */
   f          = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   positional = f && filestream_read_at(f, &one, 1, 0) == 1;
   filestream_close(f);

   if ((s = intfstream_open_file(path, RETRO_VFS_FILE_ACCESS_READ,
               RETRO_VFS_FILE_ACCESS_HINT_NONE)))
   {
      contract(s, "file");
#ifdef HAVE_THREADS
      if (positional)
         threads(s, "file");
#endif
      intfstream_close(s);
      free(s);
   }
   else
      failures++;

   if ((s = intfstream_open_buffered(path, WINDOW)))
   {
      contract(s, "buffered");
      buffered_window(s);
#ifdef HAVE_THREADS
      if (positional)
         threads(s, "buffered");
#endif
      intfstream_close(s);
      free(s);
   }
   else
      failures++;

   if ((s = intfstream_open_memory(data, RETRO_VFS_FILE_ACCESS_READ,
               RETRO_VFS_FILE_ACCESS_HINT_NONE, FILE_BYTES)))
   {
      contract(s, "memory");
#ifdef HAVE_THREADS
      threads(s, "memory");
#endif
      intfstream_close(s);
      free(s);
   }
   else
      failures++;

   remove(path);
   printf("intfstream read_at: %s\n", failures ? "FAIL" : "ok");
   return failures ? 1 : 0;
}

/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rdeflate_flush_test.c).
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

/* rdeflate_flush: a gzip stream compressed a piece at a time with a sync
 * flush after each piece, as a log written while it grows. After every
 * flush the output so far must end with the empty stored block
 * (00 00 ff ff) and decode to exactly the input so far; at the end the
 * whole stream decodes, trailer checked. Every level, and an output a
 * few bytes at a time so the flush itself suspends. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <encodings/deflate.h>

#define TOTAL  (300 * 1024)
#define PIECES 37

static int failures = 0;

#define CHECK(c) do { if (!(c)) { \
   fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void gen(uint8_t *p, size_t n)
{
   static const char text[] = "Log line: the emulator says something. ";
   uint32_t x = 7;
   size_t i;
   for (i = 0; i < n; i++)
   {
      if ((i / 3000) % 3)
         p[i] = (uint8_t)text[i % (sizeof(text) - 1)];
      else
      {
         x = x * 1103515245u + 12345u;
         p[i] = (uint8_t)(x >> 16);
      }
   }
}

/* Decodes @len bytes of a gzip stream; returns the bytes it gave, or
 * (size_t)-1 on an error. @complete: whether the stream must end. */
static size_t decode(const uint8_t *in, size_t len, uint8_t *out,
      size_t out_len, int complete)
{
   void  *z = rinflate_new(31);
   size_t rd = 0, wr = 0;
   int    r;
   if (!z)
      return (size_t)-1;
   rinflate_set_in(z, in, len);
   rinflate_set_out(z, out, out_len);
   r = rinflate_process(z, &rd, &wr);
   rinflate_free(z);
   if (r == RDEFLATE_PROCESS_ERROR || (complete && r != RDEFLATE_PROCESS_END)
         || (!complete && r == RDEFLATE_PROCESS_END))
      return (size_t)-1;
   return wr;
}

/* Runs the encoder until it stops with room left, @step bytes of output
 * room at a time */
static int pump(void *z, uint8_t *out, size_t cap, size_t *len, size_t step,
      int *ended)
{
   for (;;)
   {
      size_t room = cap - *len < step ? cap - *len : step;
      size_t rd = 0, wr = 0;
      int    r;
      if (!room)
         return 0;
      rdeflate_set_out(z, out + *len, room);
      r = rdeflate_process(z, &rd, &wr);
      *len += wr;
      if (r == RDEFLATE_PROCESS_ERROR)
         return 0;
      if (r == RDEFLATE_PROCESS_END)
      {
         *ended = 1;
         return 1;
      }
      if (wr < room)
         return 1;
   }
}

int main(void)
{
   static const int    levels[] = { 0, 1, 3, 6, 9 };
   static const size_t steps[]  = { 1 << 20, 5 };
   uint8_t *src  = (uint8_t*)malloc(TOTAL);
   uint8_t *out  = (uint8_t*)malloc(TOTAL * 2);
   uint8_t *back = (uint8_t*)malloc(TOTAL);
   unsigned li, si;

   if (!src || !out || !back)
      return 1;
   gen(src, TOTAL);

   for (li = 0; li < sizeof(levels) / sizeof(levels[0]); li++)
   {
      for (si = 0; si < sizeof(steps) / sizeof(steps[0]); si++)
      {
         void  *z    = rdeflate_new(levels[li], 31);
         size_t len  = 0, given = 0;
         int    ended = 0, p;

         CHECK(z != NULL);
         if (!z)
            continue;
         for (p = 0; p < PIECES; p++)
         {
            size_t n = TOTAL / PIECES + (p == PIECES - 1 ? TOTAL % PIECES : 0);
            size_t got;

            rdeflate_set_in(z, src + given, n);
            given += n;
            rdeflate_flush(z);
            CHECK(pump(z, out, TOTAL * 2, &len, steps[si], &ended));
            CHECK(!ended);
            CHECK(len >= 4 && !memcmp(out + len - 4, "\x00\x00\xff\xff", 4));

            got = decode(out, len, back, TOTAL, 0);
            CHECK(got == given);
            CHECK(got == given && !memcmp(back, src, given));
         }

         rdeflate_set_in(z, NULL, 0);
         rdeflate_finish(z);
         CHECK(pump(z, out, TOTAL * 2, &len, steps[si], &ended));
         CHECK(ended);
         CHECK(decode(out, len, back, TOTAL, 1) == TOTAL);
         CHECK(!memcmp(back, src, TOTAL));
         rdeflate_free(z);
      }
   }

   free(src);
   free(out);
   free(back);
   if (failures)
   {
      fprintf(stderr, "rdeflate_flush_test: %d failure(s)\n", failures);
      return 1;
   }
   printf("rdeflate_flush_test: ok\n");
   return 0;
}

/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <formats/rflv_mux.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned read_be(const unsigned char *p, unsigned n)
{
   unsigned v = 0, i;
   for (i = 0; i < n; i++)
      v = (v << 8) | p[i];
   return v;
}

static void output(rflv_mux_t *m, FILE *f)
{
   const rflv_span_t *s;
   size_t n, i;
   while (rflv_mux_get(m, &s, &n))
   {
      for (i = 0; i < n; i++)
         assert(fwrite(s[i].data, 1, s[i].size, f) == s[i].size);
      assert(rflv_mux_release(m));
   }
}

int main(int argc, char **argv)
{
   rflv_config_t c;
   rflv_mux_t *m;
   FILE *f;
   unsigned char *data, *p;
   size_t size, pos, length;
   long bytes;
   unsigned pass, type, timestamp, prefix;
   int result, composition;
   int64_t pts;
   assert(argc == 3);
   f = fopen(argv[1], "rb");
   assert(f && !fseek(f, 0, SEEK_END));
   bytes = ftell(f);
   assert(bytes >= 13 && bytes < 64 * 1024 * 1024 && !fseek(f, 0, SEEK_SET));
   size = (size_t)bytes;
   data = (unsigned char*)malloc(size);
   assert(data && fread(data, 1, size, f) == size && !fclose(f));
   assert(!memcmp(data, "FLV\1", 4) && read_be(data + 5, 4) == 9);
   memset(&c, 0, sizeof(c));
   c.max_packet_size = 0xfffffa;
   m = NULL;
   f = NULL;
   for (pass = 0; pass < 2; pass++)
   {
      for (pos = 13; pos < size; pos += length + 15)
      {
         assert(size - pos >= 15);
         p = data + pos;
         type = p[0];
         length = read_be(p + 1, 3);
         assert(length <= size - pos - 15);
         assert(read_be(p + 11 + length, 4) == length + 11);
         if (type != 8 && type != 9)
            continue;
         prefix = type == 9 ? 5 : 2;
         assert(length >= prefix);
         assert(type == 9 ? (p[11] & 15) == 7 : (p[11] >> 4) == 10);
         if (type == 9 && p[12] == 2)
            continue;
         assert(length > prefix);
         if (!pass)
         {
            if (p[12] != 0)
               continue;
            if (type == 9)
            {
               c.avcc = p + 16;
               c.avcc_size = length - 5;
               c.video_timescale = 1000;
            }
            else
            {
               c.asc = p + 13;
               c.asc_size = length - 2;
               c.audio_timescale = 1000;
            }
            continue;
         }
         if (p[12] != 1)
            continue;
         timestamp = read_be(p + 4, 3) | ((unsigned)p[7] << 24);
         composition = type == 9 ? (int)read_be(p + 13, 3) : 0;
         if (composition & 0x800000)
            composition -= 0x1000000;
         pts = (int64_t)timestamp + composition;
         assert(pts >= 0);
         result = rflv_mux_push(m, type == 9 ? 1 : 2,
               p + 11 + prefix, length - prefix, timestamp, (uint64_t)pts,
               type == 9 && (p[11] >> 4) == 1);
         assert(result == RFLV_PROGRESS);
         do
            result = rflv_mux_step(m);
         while (result == RFLV_PROGRESS);
         assert(result == RFLV_PACKET);
         output(m, f);
      }
      assert(pos == size);
      if (!pass)
      {
         m = rflv_mux_new(&c);
         f = fopen(argv[2], "wb");
         assert(m && f);
         output(m, f);
      }
   }
   assert(rflv_mux_finish(m));
   assert(!fclose(f));
   rflv_mux_free(m);
   free(data);
   return 0;
}

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
#include <net/net_rtmp_chunk.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned char payload[700];

static void fixtures(FILE *file, unsigned chunk_size)
{
   static const unsigned ids[] = { 2, 63, 64, 319, 320, 65599 };
   static const uint32_t times[] = { 0, 0xfffffe, 0xffffff, 0x1000000, 0xffffffff, 42 };
   static const size_t sizes[] = { 0, 1, 127, 128, 129, 700 };
   net_rtmp_chunk_t *c = net_rtmp_chunk_new(chunk_size);
   net_rtmp_span_t in[3], out, again;
   size_t i, n, step = 0, sent;
   bool borrowed;
   assert(c);
   for (i = 0; i < 6; i++)
   {
      in[0].data = payload;
      in[0].size = sizes[i];
      n = sizes[i] ? 1 : 0;
      if (sizes[i] == 700)
      {
         in[0].size = 17;
         in[1].data = payload + 17;
         in[1].size = 111;
         in[2].data = payload + 128;
         in[2].size = 572;
         n = 3;
      }
      assert(net_rtmp_chunk_push(c, ids[i], 0x12345678, 9, times[i], in, n));
      assert(!net_rtmp_chunk_size(c, chunk_size));
      assert(!net_rtmp_chunk_push(c, 2, 1, 8, 0, NULL, 0));
      sent = 0;
      while (net_rtmp_chunk_get(c, &out))
      {
         assert(out.size);
         assert(!net_rtmp_chunk_consume(c, out.size + 1));
         assert(net_rtmp_chunk_consume(c, 0));
         assert(net_rtmp_chunk_get(c, &again));
         assert(out.data == again.data && out.size == again.size);
         borrowed = false;
         for (n = 0; n < sizes[i]; n++)
            if (out.data == payload + n)
            {
               assert(n == sent);
               borrowed = true;
               break;
            }
         n = 1 + step++ % 19;
         if (n > out.size)
            n = out.size;
         if (borrowed)
            sent += n;
         if (file)
            assert(fwrite(out.data, 1, n, file) == n);
         assert(net_rtmp_chunk_consume(c, n));
      }
      assert(sent == sizes[i]);
      assert(net_rtmp_chunk_size(c, chunk_size));
   }
   assert(!net_rtmp_chunk_consume(c, 0));
   net_rtmp_chunk_free(c);
}

static void invalid(void)
{
   net_rtmp_chunk_t *c = net_rtmp_chunk_new(128);
   net_rtmp_span_t s;
   assert(c && !net_rtmp_chunk_new(0));
   assert(!net_rtmp_chunk_size(c, 0));
   s.data = payload;
   s.size = 0x1000000;
   assert(!net_rtmp_chunk_push(c, 2, 1, 9, 0, &s, 1));
   s.size = 1;
   assert(!net_rtmp_chunk_push(c, 1, 1, 9, 0, &s, 1));
   assert(!net_rtmp_chunk_push(c, 65600, 1, 9, 0, &s, 1));
   assert(!net_rtmp_chunk_push(c, 2, 1, 256, 0, &s, 1));
   assert(!net_rtmp_chunk_push(c, 2, 1, 9, 0, &s, 4));
   assert(!net_rtmp_chunk_push(c, 2, 1, 9, 0, NULL, 1));
   assert(net_rtmp_chunk_push(c, 2, 1, 9, 0, &s, 1));
   net_rtmp_chunk_free(c);
}

int main(int argc, char **argv)
{
   unsigned i;
   FILE *file = NULL;
   assert(argc == 1 || argc == 2);
   for (i = 0; i < sizeof(payload); i++)
      payload[i] = (unsigned char)(i * 37 + 11);
   if (argc == 2)
   {
      file = fopen(argv[1], "wb");
      assert(file);
   }
   fixtures(file, 128);
   if (file)
      assert(!fclose(file));
   fixtures(NULL, 1);
   fixtures(NULL, 4096);
   invalid();
   puts("PASS: RTMP chunk ownership, short writes, extended timestamps and IDs");
   return 0;
}

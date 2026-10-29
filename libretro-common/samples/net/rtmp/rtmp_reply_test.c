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
#include <net/net_rtmp_reply.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t stream_reply[] =
{ 2, 0, 7, '_', 'r', 'e', 's', 'u', 'l', 't',
   0, 64, 0, 0, 0, 0, 0, 0, 0, 5, 0, 65, 239, 255, 255, 255, 224, 0, 0 };

static int parse(net_rtmp_reply_t *r, size_t size)
{
   size_t i;
   int result = NET_RTMP_REPLY_PROGRESS;
   for (i = 0; i < size * 3 + 16 && result == NET_RTMP_REPLY_PROGRESS; i++)
      result = net_rtmp_reply_step(r);
   assert(result != NET_RTMP_REPLY_PROGRESS);
   return result;
}

static void contract(void)
{
   net_rtmp_reply_t *r = net_rtmp_reply_new();
   net_rtmp_reply_info_t info;
   uint8_t wrapped[sizeof(stream_reply) + 1];
   size_t i;
   assert(r && !net_rtmp_reply_get(r, &info));
   assert(net_rtmp_reply_begin(r, 20, stream_reply, sizeof(stream_reply)));
   assert(!net_rtmp_reply_begin(r, 20, stream_reply, sizeof(stream_reply)));
   assert(parse(r, sizeof(stream_reply)) == NET_RTMP_REPLY_DONE);
   assert(net_rtmp_reply_get(r, &info));
   assert(info.command.data == stream_reply + 3 && info.command.size == 7);
   assert(info.transaction == 2 && info.stream_valid && info.stream == 0xffffffff);
   assert(net_rtmp_reply_step(r) == NET_RTMP_REPLY_DONE);
   net_rtmp_reply_release(r);
   wrapped[0] = 0;
   memcpy(wrapped + 1, stream_reply, sizeof(stream_reply));
   assert(net_rtmp_reply_begin(r, 17, wrapped, sizeof(wrapped)));
   assert(parse(r, sizeof(wrapped)) == NET_RTMP_REPLY_DONE);
   net_rtmp_reply_release(r);
   wrapped[0] = 1;
   assert(!net_rtmp_reply_begin(r, 17, wrapped, sizeof(wrapped)));
   for (i = 1; i < sizeof(stream_reply); i++)
   {
      assert(net_rtmp_reply_begin(r, 20, stream_reply, i));
      if (i == 20)
         assert(parse(r, i) == NET_RTMP_REPLY_DONE);
      else
      {
         assert(parse(r, i) == NET_RTMP_REPLY_ERROR);
         assert(net_rtmp_reply_step(r) == NET_RTMP_REPLY_ERROR);
         assert(!net_rtmp_reply_get(r, &info));
      }
      net_rtmp_reply_release(r);
   }
   assert(net_rtmp_reply_begin(r, 20, stream_reply, sizeof(stream_reply)));
   net_rtmp_reply_release(r);
   net_rtmp_reply_free(r);
}

static void write_uint(FILE *f, uint32_t value)
{
   uint8_t p[4];
   p[0] = (uint8_t)(value >> 24);
   p[1] = (uint8_t)(value >> 16);
   p[2] = (uint8_t)(value >> 8);
   p[3] = (uint8_t)value;
   assert(fwrite(p, 1, 4, f) == 4);
}

static void write_span(FILE *f, net_rtmp_span_t span)
{
   write_uint(f, (uint32_t)span.size);
   if (span.size)
      assert(fwrite(span.data, 1, span.size, f) == span.size);
}

static void decode(const char *input, const char *output)
{
   net_rtmp_reply_t *r = net_rtmp_reply_new();
   net_rtmp_reply_info_t info;
   FILE *f = fopen(input, "rb");
   uint8_t *data;
   long size;
   bool valid;
   assert(r && f && !fseek(f, 0, SEEK_END));
   size = ftell(f);
   assert(size > 1 && size <= 65537 && !fseek(f, 0, SEEK_SET));
   data = (uint8_t*)malloc((size_t)size);
   assert(data && fread(data, 1, (size_t)size, f) == (size_t)size && !fclose(f));
   valid = net_rtmp_reply_begin(r, data[0], data + 1, (size_t)size - 1)
      && parse(r, (size_t)size) == NET_RTMP_REPLY_DONE;
   f = fopen(output, "wb");
   assert(f && fputc(valid ? 1 : 0, f) != EOF);
   if (valid)
   {
      assert(net_rtmp_reply_get(r, &info));
      write_uint(f, info.transaction);
      write_uint(f, info.stream);
      assert(fputc(info.stream_valid ? 1 : 0, f) != EOF);
      write_span(f, info.command);
      write_span(f, info.code);
      write_span(f, info.level);
      write_span(f, info.description);
   }
   assert(!fclose(f));
   net_rtmp_reply_release(r);
   free(data);
   net_rtmp_reply_free(r);
}

int main(int argc, char **argv)
{
   if (argc == 3)
   {
      decode(argv[1], argv[2]);
      return 0;
   }
   assert(argc == 1);
   contract();
   puts("PASS: bounded AMF reply lifecycle, exact IDs, wrapper and truncation");
   return 0;
}

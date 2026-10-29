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
#include <net/net_rtmp_receive.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8_t wire[] = { 3, 0, 0, 10, 0, 0, 2, 20, 1, 0, 0, 0, 97, 98,
   67, 0, 0, 5, 0, 0, 1, 20, 99, 131, 0, 0, 7, 100, 195, 101 };
static uint8_t large[5012];

static void basic(void)
{
   net_rtmp_receive_t *r = net_rtmp_receive_new(32, 2);
   net_rtmp_message_t m, again;
   const uint8_t expected[] = { 97, 98, 99, 100, 101 };
   const uint32_t times[] = { 10, 15, 22, 29 };
   size_t pos = 0, used, offset = 0, n, ignored;
   unsigned count = 0;
   int result;
   assert(r);
   while (pos < sizeof(wire))
   {
      result = net_rtmp_receive_feed(r, wire + pos, 1, &used);
      assert(result >= 0 && used == 1);
      pos += used;
      if (result != NET_RTMP_RECEIVE_MESSAGE)
         continue;
      assert(count < 4 && net_rtmp_receive_get(r, &m));
      n = count ? 1 : 2;
      assert(m.timestamp == times[count] && m.type == 20 && m.stream_id == 1 && m.chunk_id == 3);
      assert(m.payload.size == n && !memcmp(m.payload.data, expected + offset, n));
      assert(!net_rtmp_receive_finish(r));
      assert(net_rtmp_receive_feed(r, wire, sizeof(wire), &ignored) == NET_RTMP_RECEIVE_MESSAGE);
      assert(!ignored && net_rtmp_receive_get(r, &again));
      assert(again.payload.data == m.payload.data && !memcmp(again.payload.data, expected + offset, n));
      assert(net_rtmp_receive_release(r));
      offset += n;
      count++;
   }
   assert(count == 4 && net_rtmp_receive_finish(r));
   net_rtmp_receive_free(r);
}

static void invalid(void)
{
   net_rtmp_receive_t *r;
   size_t used, i, pos;
   int result = 0;
   assert(!net_rtmp_receive_new(0, 1));
   assert(!net_rtmp_receive_new(1, 65));
   r = net_rtmp_receive_new(1, 1);
   assert(r);
   for (pos = 0; pos < sizeof(wire) && result >= 0; pos += used)
      result = net_rtmp_receive_feed(r, wire + pos, sizeof(wire) - pos, &used);
   assert(result == NET_RTMP_RECEIVE_ERROR && !net_rtmp_receive_finish(r));
   assert(net_rtmp_receive_feed(r, NULL, 0, &used) == NET_RTMP_RECEIVE_ERROR && !used);
   net_rtmp_receive_free(r);
   for (i = 1; i < 14; i++)
   {
      r = net_rtmp_receive_new(32, 1);
      assert(r);
      for (pos = 0; pos < i; pos += used)
         assert(net_rtmp_receive_feed(r, wire + pos, i - pos, &used) >= 0 && used);
      assert(!net_rtmp_receive_finish(r));
      net_rtmp_receive_free(r);
   }
   r = net_rtmp_receive_new(32, 1);
   assert(r && !net_rtmp_receive_size(r, 0));
   assert(net_rtmp_receive_feed(r, "\303", 1, &used) == NET_RTMP_RECEIVE_ERROR);
   net_rtmp_receive_free(r);
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

static void limits_and_abort(void)
{
   net_rtmp_receive_t *r = net_rtmp_receive_new(5000, 1);
   net_rtmp_message_t m;
   size_t used, pos;
   uint8_t changed[sizeof(wire)];
   static const uint8_t partial[] = { 3, 0, 0, 0, 0, 0, 8, 20, 1, 0, 0, 0, 1, 2, 3, 4 };
   assert(r && net_rtmp_receive_size(r, 8192));
   large[0] = 3;
   large[5] = 19;
   large[6] = 136;
   large[7] = 20;
   assert(net_rtmp_receive_feed(r, large, sizeof(large), &used) == NET_RTMP_RECEIVE_MORE && used == 1);
   assert(net_rtmp_receive_feed(r, large + 1, sizeof(large) - 1, &used) == NET_RTMP_RECEIVE_MORE && used == 11);
   assert(net_rtmp_receive_feed(r, large + 12, 5000, &used) == NET_RTMP_RECEIVE_MORE && used == 4096);
   assert(net_rtmp_receive_feed(r, large + 4108, 904, &used) == NET_RTMP_RECEIVE_MESSAGE && used == 904);
   assert(net_rtmp_receive_get(r, &m) && m.payload.size == 5000);
   assert(net_rtmp_receive_release(r));
   memcpy(changed, wire, sizeof(wire));
   changed[0] = 4;
   assert(net_rtmp_receive_feed(r, changed, sizeof(changed), &used) == NET_RTMP_RECEIVE_ERROR);
   net_rtmp_receive_free(r);
   r = net_rtmp_receive_new(32, 1);
   assert(r && net_rtmp_receive_size(r, 4));
   for (pos = 0; pos < sizeof(partial); pos += used)
      assert(net_rtmp_receive_feed(r, partial + pos, sizeof(partial) - pos, &used) == NET_RTMP_RECEIVE_MORE && used);
   assert(!net_rtmp_receive_finish(r));
   assert(net_rtmp_receive_abort(r, 3));
   assert(net_rtmp_receive_finish(r));
   net_rtmp_receive_free(r);
}

static void decode(const char *input, const char *output)
{
   net_rtmp_receive_t *r = net_rtmp_receive_new(8192, 16);
   net_rtmp_message_t m;
   uint8_t buffer[257];
   size_t n, pos, used;
   FILE *in = fopen(input, "rb");
   FILE *out = fopen(output, "wb");
   int result;
   assert(r && in && out);
   while ((n = fread(buffer, 1, sizeof(buffer), in)) != 0)
      for (pos = 0; pos < n; pos += used)
      {
         result = net_rtmp_receive_feed(r, buffer + pos, n - pos, &used);
         assert(result >= 0 && used);
         if (result != NET_RTMP_RECEIVE_MESSAGE)
            continue;
         assert(net_rtmp_receive_get(r, &m));
         write_uint(out, m.chunk_id);
         write_uint(out, m.stream_id);
         write_uint(out, m.type);
         write_uint(out, m.timestamp);
         write_uint(out, (uint32_t)m.payload.size);
         assert(fwrite(m.payload.data, 1, m.payload.size, out) == m.payload.size);
         if (m.type == 1)
         {
            const uint8_t *p = (const uint8_t*)m.payload.data;
            assert(m.payload.size == 4);
            assert(net_rtmp_receive_size(r, ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16)
                     | ((unsigned)p[2] << 8) | p[3]));
         }
         assert(net_rtmp_receive_release(r));
      }
   assert(!ferror(in) && !fclose(in) && !fclose(out));
   assert(net_rtmp_receive_finish(r));
   net_rtmp_receive_free(r);
}

int main(int argc, char **argv)
{
   if (argc == 3)
   {
      decode(argv[1], argv[2]);
      return 0;
   }
   assert(argc == 1);
   basic();
   invalid();
   limits_and_abort();
   puts("PASS: RTMP receive compression, borrowed output, bounds and truncation");
   return 0;
}

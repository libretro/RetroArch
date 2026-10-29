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
#include <net/net_rtmp_control.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int send_control(net_rtmp_control_t *c, net_rtmp_receive_t *r,
      unsigned type, const uint8_t *bytes, size_t size)
{
   net_rtmp_message_t m;
   m.payload.data = bytes;
   m.payload.size = size;
   m.timestamp = 0;
   m.stream_id = 0;
   m.chunk_id = 2;
   m.type = type;
   return net_rtmp_control_handle(c, r, &m);
}

static void response(net_rtmp_control_t *c, unsigned type,
      const uint8_t *bytes, size_t size)
{
   net_rtmp_message_t m, again;
   assert(net_rtmp_control_get(c, &m));
   assert(m.chunk_id == 2 && !m.stream_id && !m.timestamp);
   assert(m.type == type && m.payload.size == size);
   assert(!memcmp(m.payload.data, bytes, size));
   assert(net_rtmp_control_get(c, &again));
   assert(m.payload.data == again.payload.data);
   assert(!net_rtmp_control_free(c));
}

static void accounting(void)
{
   net_rtmp_control_t *c = net_rtmp_control_new();
   net_rtmp_message_t m;
   net_rtmp_control_status_t s;
   const uint8_t window[] = {0, 0, 0, 4};
   const uint8_t four[] = {0, 0, 0, 4};
   const uint8_t eight[] = {0, 0, 0, 8};
   const uint8_t wrapped[] = {0, 0, 0, 7};
   assert(c);
   assert(send_control(c, NULL, 5, window, 4) == NET_RTMP_CONTROL_HANDLED);
   assert(net_rtmp_control_received(c, 3));
   assert(!net_rtmp_control_get(c, &m));
   assert(net_rtmp_control_received(c, 1));
   response(c, 3, four, 4);
   assert(net_rtmp_control_received(c, 4));
   response(c, 3, four, 4);
   assert(net_rtmp_control_ack(c));
   response(c, 3, eight, 4);
   assert(net_rtmp_control_ack(c));
   assert(net_rtmp_control_received(c, (size_t)0xffffffffu));
   response(c, 3, wrapped, 4);
   assert(net_rtmp_control_ack(c));
   assert(!net_rtmp_control_get(c, &m));
   assert(send_control(c, NULL, 3, eight, 4) == NET_RTMP_CONTROL_HANDLED);
   assert(net_rtmp_control_status(c, &s));
   assert(s.peer_ack == 8 && s.window == 4);
   assert(s.received == (uint64_t)0xffffffffu + 8);
   assert(net_rtmp_control_free(c));
}

static void ping_and_bandwidth(void)
{
   net_rtmp_control_t *c = net_rtmp_control_new();
   net_rtmp_control_status_t s;
   net_rtmp_message_t m;
   uint8_t ping[] = {0, 6, 0xde, 0xad, 0xbe, 0xef};
   const uint8_t pong[] = {0, 7, 0xde, 0xad, 0xbe, 0xef};
   uint8_t bandwidth[] = {0, 0, 4, 0, 2};
   const uint8_t smaller[] = {0, 0, 2, 0};
   assert(c);
   assert(send_control(c, NULL, 6, bandwidth, 5) == NET_RTMP_CONTROL_HANDLED);
   assert(!net_rtmp_control_get(c, &m));
   bandwidth[4] = 0;
   assert(send_control(c, NULL, 6, bandwidth, 5) == NET_RTMP_CONTROL_HANDLED);
   response(c, 5, bandwidth, 4);
   bandwidth[2] = 2;
   bandwidth[4] = 2;
   assert(send_control(c, NULL, 6, bandwidth, 5) == NET_RTMP_CONTROL_BUSY);
   assert(net_rtmp_control_ack(c));
   assert(send_control(c, NULL, 6, bandwidth, 5) == NET_RTMP_CONTROL_HANDLED);
   response(c, 5, smaller, 4);
   assert(send_control(c, NULL, 4, ping, 6) == NET_RTMP_CONTROL_HANDLED);
   memset(ping, 0, sizeof(ping));
   response(c, 5, smaller, 4);
   assert(net_rtmp_control_ack(c));
   response(c, 4, pong, 6);
   ping[1] = 6;
   assert(send_control(c, NULL, 4, ping, 6) == NET_RTMP_CONTROL_BUSY);
   assert(net_rtmp_control_ack(c));
   bandwidth[2] = 4;
   bandwidth[4] = 1;
   assert(send_control(c, NULL, 6, bandwidth, 5) == NET_RTMP_CONTROL_HANDLED);
   assert(net_rtmp_control_status(c, &s) && s.bandwidth == 512);
   assert(!net_rtmp_control_get(c, &m));
   bandwidth[2] = 1;
   bandwidth[4] = 2;
   assert(send_control(c, NULL, 6, bandwidth, 5) == NET_RTMP_CONTROL_HANDLED);
   assert(net_rtmp_control_status(c, &s) && s.bandwidth == 512);
   assert(net_rtmp_control_free(c));
}

static void receive_controls(void)
{
   static const uint8_t wire[] =
   {
      2, 0, 0, 0, 0, 0, 4, 1, 0, 0, 0, 0, 0, 0, 0, 1,
      3, 0, 0, 0, 0, 0, 3, 20, 1, 0, 0, 0, 'A', 0xc3, 'B', 0xc3, 'C',
      3, 0, 0, 0, 0, 0, 3, 20, 1, 0, 0, 0, 'D',
      2, 0, 0, 0, 0, 0, 4, 2, 0, 0, 0, 0, 0, 0xc2, 0, 0xc2, 0, 0xc2, 3,
      3, 0, 0, 0, 0, 0, 3, 20, 1, 0, 0, 0, 'E', 0xc3, 'F', 0xc3, 'G'
   };
   net_rtmp_control_t *c = net_rtmp_control_new();
   net_rtmp_receive_t *r = net_rtmp_receive_new(64, 4);
   net_rtmp_message_t m;
   size_t at = 0, used;
   unsigned messages = 0;
   int result;
   const uint8_t abort[] = {0, 0, 0, 64};
   assert(c && r);
   while (at < sizeof(wire))
   {
      result = net_rtmp_receive_feed(r, wire + at, 1, &used);
      assert(result != NET_RTMP_RECEIVE_ERROR && used == 1);
      at += used;
      assert(net_rtmp_control_received(c, used));
      if (result != NET_RTMP_RECEIVE_MESSAGE)
         continue;
      assert(net_rtmp_receive_get(r, &m));
      result = net_rtmp_control_handle(c, r, &m);
      if (m.type != 20)
         assert(result == NET_RTMP_CONTROL_HANDLED);
      else
      {
         assert(result == NET_RTMP_CONTROL_OTHER);
         assert(m.payload.size == 3);
         assert(!memcmp(m.payload.data, messages == 1 ? "ABC" : "EFG", 3));
      }
      messages++;
      assert(net_rtmp_receive_release(r));
   }
   assert(messages == 4);
   assert(send_control(c, r, 2, abort, 4) == NET_RTMP_CONTROL_HANDLED);
   assert(net_rtmp_receive_finish(r));
   net_rtmp_receive_free(r);
   assert(net_rtmp_control_free(c));
}

static void malformed(void)
{
   unsigned type, size;
   const uint8_t bytes[] = {0, 0, 0, 1, 0};
   for (type = 1; type <= 6; type++)
      for (size = 0; size < 6; size++)
      {
         net_rtmp_control_t *c;
         net_rtmp_receive_t *r;
         net_rtmp_message_t m;
         if (type == 4 || size == (type == 6 ? 5 : 4))
            continue;
         c = net_rtmp_control_new();
         r = net_rtmp_receive_new(64, 4);
         assert(c && r);
         assert(send_control(c, r, type, bytes, size) == NET_RTMP_CONTROL_ERROR);
         assert(!net_rtmp_control_received(c, 1));
         assert(!net_rtmp_control_get(c, &m));
         assert(net_rtmp_control_free(c));
         net_rtmp_receive_free(r);
      }
}

static void invalid_values_and_retirement(void)
{
   static const unsigned types[] = {1, 1, 2, 5, 6, 4, 4};
   static const size_t sizes[] = {4, 4, 4, 4, 5, 2, 5};
   static const uint8_t values[][6] =
   {
      {0, 0, 0, 0}, {128, 0, 0, 1}, {0, 0, 0, 0},
      {0, 0, 0, 0}, {0, 0, 0, 1, 3}, {0, 6}, {0, 7, 0, 0, 0}
   };
   unsigned i;
   for (i = 0; i < sizeof(types) / sizeof(types[0]); i++)
   {
      net_rtmp_control_t *c = net_rtmp_control_new();
      net_rtmp_receive_t *r = net_rtmp_receive_new(64, 4);
      net_rtmp_message_t m;
      const uint8_t ping[] = {0, 6, 1, 2, 3, 4};
      assert(c && r);
      assert(send_control(c, r, 4, ping, 6) == NET_RTMP_CONTROL_HANDLED);
      assert(net_rtmp_control_get(c, &m));
      assert(send_control(c, r, types[i], values[i], sizes[i]) == NET_RTMP_CONTROL_ERROR);
      assert(!net_rtmp_control_free(c));
      assert(!memcmp((const uint8_t*)m.payload.data + 2, ping + 2, 4));
      assert(net_rtmp_control_ack(c));
      assert(net_rtmp_control_free(c));
      net_rtmp_receive_free(r);
   }
   if (sizeof(size_t) >= sizeof(uint64_t))
   {
      net_rtmp_control_t *c = net_rtmp_control_new();
      assert(c && net_rtmp_control_received(c, (size_t)-1));
      assert(!net_rtmp_control_received(c, 1));
      assert(net_rtmp_control_free(c));
   }
}

int main(void)
{
   accounting();
   ping_and_bandwidth();
   receive_controls();
   malformed();
   invalid_values_and_retirement();
   puts("RTMP controls: acknowledgements, ping, bandwidth, chunk resize and malformed input pass");
   return 0;
}

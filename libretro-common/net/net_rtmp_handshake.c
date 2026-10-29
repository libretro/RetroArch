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
#include <net/net_rtmp_handshake.h>
#include <stdlib.h>
#include <string.h>

struct net_rtmp_handshake
{
   size_t sent, received;
   unsigned receive_stage;
   uint8_t c1[1537], c2[1536];
   bool failed;
};

static void handshake_time(uint8_t *p, uint32_t time)
{
   p[0] = (uint8_t)(time >> 24);
   p[1] = (uint8_t)(time >> 16);
   p[2] = (uint8_t)(time >> 8);
   p[3] = (uint8_t)time;
}

net_rtmp_handshake_t *net_rtmp_handshake_new(
      const uint8_t random[1528], uint32_t time)
{
   net_rtmp_handshake_t *h;
   if (!random)
      return NULL;
   h = (net_rtmp_handshake_t*)calloc(1, sizeof(*h));
   if (!h)
      return NULL;
   h->c1[0] = 3;
   handshake_time(h->c1 + 1, time);
   memcpy(h->c1 + 9, random, 1528);
   return h;
}

bool net_rtmp_handshake_get(net_rtmp_handshake_t *h, net_rtmp_span_t *span)
{
   if (!h || !span || h->failed)
      return false;
   if (h->sent < 1537)
   {
      span->data = h->c1 + h->sent;
      span->size = 1537 - h->sent;
   }
   else if (h->receive_stage >= 2 && h->sent < 3073)
   {
      span->data = h->c2 + h->sent - 1537;
      span->size = 3073 - h->sent;
   }
   else
      return false;
   return true;
}

bool net_rtmp_handshake_consume(net_rtmp_handshake_t *h, size_t size)
{
   net_rtmp_span_t span;
   if (!net_rtmp_handshake_get(h, &span) || size > span.size)
      return false;
   h->sent += size;
   return true;
}

int net_rtmp_handshake_status(const net_rtmp_handshake_t *h)
{
   if (!h || h->failed)
      return NET_RTMP_HANDSHAKE_ERROR;
   return h->sent == 3073 && h->receive_stage == 3
      ? NET_RTMP_HANDSHAKE_DONE : NET_RTMP_HANDSHAKE_PENDING;
}

int net_rtmp_handshake_feed(net_rtmp_handshake_t *h,
      const void *data, size_t size, size_t *consumed, uint32_t time)
{
   const uint8_t *p = (const uint8_t*)data;
   size_t n, i, start;
   if (consumed)
      *consumed = 0;
   if (!h || h->failed || !consumed || (!data && size))
      return NET_RTMP_HANDSHAKE_ERROR;
   if (!size || h->receive_stage == 3)
      return net_rtmp_handshake_status(h);
   if (!h->receive_stage)
   {
      *consumed = 1;
      if (p[0] != 3)
         goto failed;
      h->receive_stage = 1;
      return NET_RTMP_HANDSHAKE_PENDING;
   }
   n = size < 1536 - h->received ? size : 1536 - h->received;
   if (h->receive_stage == 1)
   {
      memcpy(h->c2 + h->received, p, n);
      h->received += n;
      *consumed = n;
      if (h->received == 1536)
      {
         if (h->c2[4] || h->c2[5] || h->c2[6] || h->c2[7])
            goto failed;
         handshake_time(h->c2 + 4, time);
         h->received = 0;
         h->receive_stage = 2;
      }
   }
   else
   {
      start = h->received;
      for (i = 0; i < n; i++)
      {
         h->received++;
         *consumed = i + 1;
         if ((start + i < 4 || start + i >= 8) && p[i] != h->c1[start + i + 1])
            goto failed;
      }
      if (h->received == 1536)
         h->receive_stage = 3;
   }
   return net_rtmp_handshake_status(h);
failed:
   h->failed = true;
   return NET_RTMP_HANDSHAKE_ERROR;
}

void net_rtmp_handshake_free(net_rtmp_handshake_t *h)
{
   free(h);
}

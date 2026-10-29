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
#include <stdlib.h>
#include <string.h>

struct receive_stream
{
   size_t used;
   uint32_t timestamp, delta, stamp, length, stream;
   unsigned id, type;
   bool valid, active, extended;
};

enum receive_stage { RECEIVE_BASIC, RECEIVE_ID, RECEIVE_HEADER, RECEIVE_EXTENDED, RECEIVE_PAYLOAD };
struct net_rtmp_receive
{
   struct receive_stream *streams;
   uint8_t *storage;
   size_t limit, have, need, left;
   unsigned count, chunk_size, id, fmt, current, ready;
   enum receive_stage stage;
   uint8_t header[11], extension[4];
   bool failed, finished;
};

static uint32_t receive_be(const uint8_t *p, unsigned n)
{
   uint32_t value = 0;
   unsigned i;
   for (i = 0; i < n; i++)
      value = (value << 8) | p[i];
   return value;
}

net_rtmp_receive_t *net_rtmp_receive_new(size_t limit, unsigned streams)
{
   net_rtmp_receive_t *r;
   if (!limit || limit > 0xffffff || !streams || streams > 64
         || limit > (size_t)-1 / streams)
      return NULL;
   r = (net_rtmp_receive_t*)calloc(1, sizeof(*r));
   if (!r)
      return NULL;
   r->streams = (struct receive_stream*)calloc(streams, sizeof(*r->streams));
   r->storage = (uint8_t*)malloc(limit * streams);
   if (!r->streams || !r->storage)
   {
      net_rtmp_receive_free(r);
      return NULL;
   }
   r->limit = limit;
   r->count = streams;
   r->chunk_size = 128;
   return r;
}

static bool receive_prepare(net_rtmp_receive_t *r, uint32_t stamp);

static bool receive_select(net_rtmp_receive_t *r)
{
   unsigned i, empty = r->count;
   for (i = 0; i < r->count; i++)
   {
      if (r->streams[i].id == r->id)
         break;
      if (!r->streams[i].id)
         empty = i;
   }
   if (i == r->count)
   {
      if (empty == r->count || r->fmt)
         return false;
      i = empty;
      r->streams[i].id = r->id;
   }
   if (r->fmt && !r->streams[i].valid)
      return false;
   r->current = i;
   r->have = 0;
   r->need = r->fmt == 0 ? 11 : r->fmt == 1 ? 7 : r->fmt == 2 ? 3 : 0;
   r->stage = RECEIVE_HEADER;
   if (!r->need)
   {
      if (!r->streams[i].extended)
         return receive_prepare(r, r->streams[i].stamp);
      r->stage = RECEIVE_EXTENDED;
      r->need = 4;
   }
   return true;
}

static bool receive_prepare(net_rtmp_receive_t *r, uint32_t stamp)
{
   struct receive_stream *s = r->streams + r->current;
   uint32_t length = s->length, stream = s->stream, timestamp, delta;
   unsigned type = s->type;
   if (r->fmt <= 1)
   {
      length = receive_be(r->header + 3, 3);
      type = r->header[6];
   }
   if (!r->fmt)
      stream = (uint32_t)r->header[7] | ((uint32_t)r->header[8] << 8)
         | ((uint32_t)r->header[9] << 16) | ((uint32_t)r->header[10] << 24);
   delta = r->fmt == 3 ? s->delta : stamp;
   timestamp = !r->fmt ? stamp : s->active ? s->timestamp : s->timestamp + delta;
   if (length > r->limit || !type || (s->active &&
            (length != s->length || type != s->type || stream != s->stream
             || timestamp != s->timestamp || (r->fmt && r->fmt != 3 && stamp != s->delta))))
      return false;
   if (!s->active)
      s->used = 0;
   s->length = length;
   s->stream = stream;
   s->timestamp = timestamp;
   s->delta = delta;
   s->type = type;
   s->valid = s->active = true;
   if (r->fmt != 3)
      s->stamp = stamp;
   r->left = length - s->used;
   if (r->left > r->chunk_size)
      r->left = r->chunk_size;
   r->stage = RECEIVE_PAYLOAD;
   if (!length)
   {
      s->active = false;
      r->ready = r->current + 1;
      r->stage = RECEIVE_BASIC;
   }
   return true;
}

int net_rtmp_receive_feed(net_rtmp_receive_t *r,
      const void *data, size_t size, size_t *consumed)
{
   const uint8_t *p = (const uint8_t*)data;
   struct receive_stream *s;
   size_t n;
   uint32_t stamp;
   if (consumed)
      *consumed = 0;
   if (!r || r->failed || r->finished || !consumed || (!data && size))
      return NET_RTMP_RECEIVE_ERROR;
   if (r->ready)
      return NET_RTMP_RECEIVE_MESSAGE;
   if (!size)
      return NET_RTMP_RECEIVE_MORE;
   if (r->stage == RECEIVE_BASIC)
   {
      r->fmt = p[0] >> 6;
      r->id = p[0] & 63;
      *consumed = 1;
      if (r->id >= 2)
      {
         if (!receive_select(r))
            goto failed;
      }
      else
      {
         r->stage = RECEIVE_ID;
         r->have = 0;
         r->need = r->id ? 2 : 1;
      }
      return r->ready ? NET_RTMP_RECEIVE_MESSAGE : NET_RTMP_RECEIVE_MORE;
   }
   if (r->stage == RECEIVE_PAYLOAD)
   {
      s = r->streams + r->current;
      n = size < r->left ? size : r->left;
      if (n > 4096)
         n = 4096;
      memcpy(r->storage + r->limit * r->current + s->used, p, n);
      s->used += n;
      r->left -= n;
      *consumed = n;
      if (!r->left)
         r->stage = RECEIVE_BASIC;
      if (s->used == s->length)
      {
         s->active = false;
         r->ready = r->current + 1;
         return NET_RTMP_RECEIVE_MESSAGE;
      }
      return NET_RTMP_RECEIVE_MORE;
   }
   n = size < r->need - r->have ? size : r->need - r->have;
   memcpy((r->stage == RECEIVE_EXTENDED ? r->extension : r->header) + r->have, p, n);
   r->have += n;
   *consumed = n;
   if (r->have != r->need)
      return NET_RTMP_RECEIVE_MORE;
   if (r->stage == RECEIVE_ID)
   {
      r->id = 64 + r->header[0] + (r->need == 2 ? (unsigned)r->header[1] * 256 : 0);
      if (!receive_select(r))
         goto failed;
      return r->ready ? NET_RTMP_RECEIVE_MESSAGE : NET_RTMP_RECEIVE_MORE;
   }
   s = r->streams + r->current;
   if (r->stage == RECEIVE_HEADER)
   {
      stamp = r->fmt == 3 ? s->stamp : receive_be(r->header, 3);
      if (r->fmt != 3)
         s->extended = stamp == 0xffffff;
      if (s->extended)
      {
         r->stage = RECEIVE_EXTENDED;
         r->have = 0;
         r->need = 4;
         return NET_RTMP_RECEIVE_MORE;
      }
   }
   else
      stamp = receive_be(r->extension, 4);
   if (r->fmt == 3 && stamp != s->stamp)
      goto failed;
   if (!receive_prepare(r, stamp))
      goto failed;
   return r->ready ? NET_RTMP_RECEIVE_MESSAGE : NET_RTMP_RECEIVE_MORE;
failed:
   r->failed = true;
   return NET_RTMP_RECEIVE_ERROR;
}

bool net_rtmp_receive_get(net_rtmp_receive_t *r, net_rtmp_message_t *message)
{
   struct receive_stream *s;
   if (!r || r->failed || !r->ready || !message)
      return false;
   s = r->streams + r->ready - 1;
   message->payload.data = r->storage + r->limit * (r->ready - 1);
   message->payload.size = s->length;
   message->timestamp = s->timestamp;
   message->stream_id = s->stream;
   message->chunk_id = s->id;
   message->type = s->type;
   return true;
}

bool net_rtmp_receive_release(net_rtmp_receive_t *r)
{
   if (!r || r->failed || !r->ready)
      return false;
   r->ready = 0;
   return true;
}

bool net_rtmp_receive_size(net_rtmp_receive_t *r, unsigned size)
{
   if (!r || r->failed || r->finished || r->stage != RECEIVE_BASIC || !size || size > 0x7fffffff)
      return false;
   r->chunk_size = size;
   return true;
}

bool net_rtmp_receive_abort(net_rtmp_receive_t *r, unsigned id)
{
   unsigned i;
   if (!r || r->failed || r->finished || r->stage != RECEIVE_BASIC || id < 2 || id > 65599)
      return false;
   for (i = 0; i < r->count; i++)
      if (r->streams[i].id == id)
      {
         if (r->ready == i + 1)
            return false;
         r->streams[i].active = false;
         r->streams[i].used = 0;
         break;
      }
   return true;
}

bool net_rtmp_receive_finish(net_rtmp_receive_t *r)
{
   unsigned i;
   if (!r || r->failed || r->ready || r->stage != RECEIVE_BASIC)
      return false;
   for (i = 0; i < r->count; i++)
      if (r->streams[i].active)
         return false;
   r->finished = true;
   return true;
}

void net_rtmp_receive_free(net_rtmp_receive_t *r)
{
   if (!r)
      return;
   free(r->streams);
   free(r->storage);
   free(r);
}

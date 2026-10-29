/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_rtmp_chunk.h"
#include <stdlib.h>
#include <string.h>

struct record_rtmp_chunk
{
   record_rtmp_span_t spans[3];
   size_t count, index, offset, remaining, in_chunk, header_size, header_offset;
   uint32_t timestamp;
   unsigned chunk_size, chunk_id;
   uint8_t header[18];
   bool active;
};

static void rtmp_be(uint8_t *p, uint32_t v, unsigned n)
{
   unsigned i;
   for (i = 0; i < n; i++)
      p[n - i - 1] = (uint8_t)(v >> (i * 8));
}

static unsigned rtmp_basic(uint8_t *p, unsigned fmt, unsigned id)
{
   if (id < 64)
   {
      p[0] = (uint8_t)((fmt << 6) | id);
      return 1;
   }
   id -= 64;
   p[0] = (uint8_t)((fmt << 6) | (id >= 256 ? 1 : 0));
   p[1] = (uint8_t)id;
   if (id < 256)
      return 2;
   p[2] = (uint8_t)(id >> 8);
   return 3;
}

static void rtmp_header(record_rtmp_chunk_t *c, bool first,
      uint32_t stream, unsigned type)
{
   unsigned n = rtmp_basic(c->header, first ? 0 : 3, c->chunk_id);
   if (first)
   {
      rtmp_be(c->header + n, c->timestamp < 0xffffff ? c->timestamp : 0xffffff, 3);
      rtmp_be(c->header + n + 3, (uint32_t)c->remaining, 3);
      c->header[n + 6] = (uint8_t)type;
      c->header[n + 7] = (uint8_t)stream;
      c->header[n + 8] = (uint8_t)(stream >> 8);
      c->header[n + 9] = (uint8_t)(stream >> 16);
      c->header[n + 10] = (uint8_t)(stream >> 24);
      n += 11;
   }
   if (c->timestamp >= 0xffffff)
   {
      rtmp_be(c->header + n, c->timestamp, 4);
      n += 4;
   }
   c->header_size = n;
   c->header_offset = 0;
   c->in_chunk = c->remaining < c->chunk_size ? c->remaining : c->chunk_size;
}

record_rtmp_chunk_t *record_rtmp_chunk_new(unsigned size)
{
   record_rtmp_chunk_t *c;
   if (!size || size > 0x7fffffff)
      return NULL;
   c = (record_rtmp_chunk_t*)calloc(1, sizeof(*c));
   if (c)
      c->chunk_size = size;
   return c;
}

bool record_rtmp_chunk_size(record_rtmp_chunk_t *c, unsigned size)
{
   if (!c || c->active || !size || size > 0x7fffffff)
      return false;
   c->chunk_size = size;
   return true;
}

bool record_rtmp_chunk_push(record_rtmp_chunk_t *c, unsigned id,
      uint32_t stream, unsigned type, uint32_t timestamp,
      const record_rtmp_span_t *spans, size_t count)
{
   size_t i, total = 0;
   if (!c || c->active || id < 2 || id > 65599 || !type || type > 255
         || count > 3 || (count && !spans))
      return false;
   for (i = 0; i < count; i++)
   {
      if (!spans[i].data || !spans[i].size || spans[i].size > 0xffffff - total)
         return false;
      total += spans[i].size;
   }
   for (i = 0; i < count; i++)
      c->spans[i] = spans[i];
   c->count = count;
   c->index = c->offset = 0;
   c->remaining = total;
   c->timestamp = timestamp;
   c->chunk_id = id;
   c->active = true;
   rtmp_header(c, true, stream, type);
   return true;
}

bool record_rtmp_chunk_get(record_rtmp_chunk_t *c, record_rtmp_span_t *span)
{
   size_t left;
   if (!c || !span || !c->active)
      return false;
   if (c->header_offset < c->header_size)
   {
      span->data = c->header + c->header_offset;
      span->size = c->header_size - c->header_offset;
      return true;
   }
   left = c->spans[c->index].size - c->offset;
   span->data = (const uint8_t*)c->spans[c->index].data + c->offset;
   span->size = left < c->in_chunk ? left : c->in_chunk;
   return true;
}

bool record_rtmp_chunk_consume(record_rtmp_chunk_t *c, size_t size)
{
   record_rtmp_span_t span;
   if (!record_rtmp_chunk_get(c, &span) || size > span.size)
      return false;
   if (!size)
      return true;
   if (c->header_offset < c->header_size)
      c->header_offset += size;
   else
   {
      c->offset += size;
      c->remaining -= size;
      c->in_chunk -= size;
      if (c->offset == c->spans[c->index].size)
      {
         c->index++;
         c->offset = 0;
      }
      if (!c->in_chunk && c->remaining)
         rtmp_header(c, false, 0, 0);
   }
   if (!c->remaining && c->header_offset == c->header_size)
   {
      c->active = false;
      memset(c->spans, 0, sizeof(c->spans));
   }
   return true;
}

void record_rtmp_chunk_free(record_rtmp_chunk_t *c)
{
   free(c);
}

/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_rtmp_handshake.h"
#include <stdlib.h>
#include <string.h>

struct record_rtmp_handshake
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

record_rtmp_handshake_t *record_rtmp_handshake_new(
      const uint8_t random[1528], uint32_t time)
{
   record_rtmp_handshake_t *h;
   if (!random)
      return NULL;
   h = (record_rtmp_handshake_t*)calloc(1, sizeof(*h));
   if (!h)
      return NULL;
   h->c1[0] = 3;
   handshake_time(h->c1 + 1, time);
   memcpy(h->c1 + 9, random, 1528);
   return h;
}

bool record_rtmp_handshake_get(record_rtmp_handshake_t *h, record_rtmp_span_t *span)
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

bool record_rtmp_handshake_consume(record_rtmp_handshake_t *h, size_t size)
{
   record_rtmp_span_t span;
   if (!record_rtmp_handshake_get(h, &span) || size > span.size)
      return false;
   h->sent += size;
   return true;
}

int record_rtmp_handshake_status(const record_rtmp_handshake_t *h)
{
   if (!h || h->failed)
      return RECORD_RTMP_HANDSHAKE_ERROR;
   return h->sent == 3073 && h->receive_stage == 3
      ? RECORD_RTMP_HANDSHAKE_DONE : RECORD_RTMP_HANDSHAKE_PENDING;
}

int record_rtmp_handshake_feed(record_rtmp_handshake_t *h,
      const void *data, size_t size, size_t *consumed, uint32_t time)
{
   const uint8_t *p = (const uint8_t*)data;
   size_t n, i, start;
   if (consumed)
      *consumed = 0;
   if (!h || h->failed || !consumed || (!data && size))
      return RECORD_RTMP_HANDSHAKE_ERROR;
   if (!size || h->receive_stage == 3)
      return record_rtmp_handshake_status(h);
   if (!h->receive_stage)
   {
      *consumed = 1;
      if (p[0] != 3)
         goto failed;
      h->receive_stage = 1;
      return RECORD_RTMP_HANDSHAKE_PENDING;
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
   return record_rtmp_handshake_status(h);
failed:
   h->failed = true;
   return RECORD_RTMP_HANDSHAKE_ERROR;
}

void record_rtmp_handshake_free(record_rtmp_handshake_t *h)
{
   free(h);
}

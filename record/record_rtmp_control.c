/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_rtmp_control.h"
#include <stdlib.h>

struct record_rtmp_control
{
   uint64_t received, acknowledged, output_ack;
   uint32_t window, bandwidth, peer_ack, ping, response_window;
   unsigned limit_type, output_type;
   uint8_t output[6];
   bool failed, ping_pending, window_pending, borrowed;
};

static uint32_t control_read(const uint8_t *p)
{
   return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
      | ((uint32_t)p[2] << 8) | p[3];
}

static void control_write(uint8_t *p, uint32_t value)
{
   p[0] = (uint8_t)(value >> 24);
   p[1] = (uint8_t)(value >> 16);
   p[2] = (uint8_t)(value >> 8);
   p[3] = (uint8_t)value;
}

record_rtmp_control_t *record_rtmp_control_new(void)
{
   record_rtmp_control_t *c = (record_rtmp_control_t*)calloc(1, sizeof(*c));
   if (c)
      c->limit_type = 2;
   return c;
}

bool record_rtmp_control_received(record_rtmp_control_t *c, size_t size)
{
   if (!c || c->failed)
      return false;
   if ((uint64_t)size > (uint64_t)-1 - c->received)
   {
      c->failed = true;
      return false;
   }
   c->received += size;
   return true;
}

int record_rtmp_control_handle(record_rtmp_control_t *c,
      record_rtmp_receive_t *receive, const record_rtmp_message_t *m)
{
   const uint8_t *p;
   uint32_t value, limit;
   unsigned event, kind;
   if (!c || c->failed || !m)
      return RECORD_RTMP_CONTROL_ERROR;
   if (m->type < 1 || m->type > 6)
      return RECORD_RTMP_CONTROL_OTHER;
   p = (const uint8_t*)m->payload.data;
   if (!p || m->stream_id || (m->type != 4 && m->chunk_id != 2))
      goto invalid;
   if (m->type == 4)
   {
      if (m->payload.size < 2)
         goto invalid;
      event = ((unsigned)p[0] << 8) | p[1];
      if (event == 3)
      {
         if (m->payload.size != 10)
            goto invalid;
      }
      else if (event <= 4 || event == 6 || event == 7)
      {
         if (m->payload.size != 6)
            goto invalid;
         if (event == 6)
         {
            if (c->ping_pending)
               return RECORD_RTMP_CONTROL_BUSY;
            c->ping = control_read(p + 2);
            c->ping_pending = true;
         }
      }
      return RECORD_RTMP_CONTROL_HANDLED;
   }
   if (m->payload.size != (m->type == 6 ? 5 : 4))
      goto invalid;
   value = control_read(p);
   switch (m->type)
   {
      case 1:
         if (!record_rtmp_receive_size(receive, value))
            goto invalid;
         break;
      case 2:
         if (!record_rtmp_receive_abort(receive, value))
            goto invalid;
         break;
      case 3:
         c->peer_ack = value;
         break;
      case 5:
         if (!value)
            goto invalid;
         c->window = value;
         break;
      case 6:
         kind = p[4];
         if (!value || kind > 2)
            goto invalid;
         if (kind == 2 && c->limit_type != 0)
            break;
         limit = kind == 1 && c->bandwidth && c->bandwidth < value
            ? c->bandwidth : value;
         if (limit != c->bandwidth && c->window_pending)
            return RECORD_RTMP_CONTROL_BUSY;
         if (limit != c->bandwidth)
         {
            c->response_window = limit;
            c->window_pending = true;
         }
         c->bandwidth = limit;
         c->limit_type = kind == 2 ? 0 : kind;
         break;
   }
   return RECORD_RTMP_CONTROL_HANDLED;
invalid:
   c->failed = true;
   return RECORD_RTMP_CONTROL_ERROR;
}

bool record_rtmp_control_get(record_rtmp_control_t *c, record_rtmp_message_t *m)
{
   if (!c || !m || c->failed)
      return false;
   if (!c->borrowed)
   {
      if (c->ping_pending)
      {
         c->output_type = 4;
         c->output[0] = 0;
         c->output[1] = 7;
         control_write(c->output + 2, c->ping);
      }
      else if (c->window_pending)
      {
         c->output_type = 5;
         control_write(c->output, c->response_window);
      }
      else if (c->window && c->received - c->acknowledged >= c->window)
      {
         c->output_type = 3;
         c->output_ack = c->received;
         control_write(c->output, (uint32_t)c->received);
      }
      else
         return false;
      c->borrowed = true;
   }
   m->payload.data = c->output;
   m->payload.size = c->output_type == 4 ? 6 : 4;
   m->type = c->output_type;
   m->chunk_id = 2;
   m->stream_id = 0;
   m->timestamp = 0;
   return true;
}

bool record_rtmp_control_ack(record_rtmp_control_t *c)
{
   if (!c || !c->borrowed)
      return false;
   if (c->output_type == 4)
      c->ping_pending = false;
   else if (c->output_type == 5)
      c->window_pending = false;
   else
      c->acknowledged = c->output_ack;
   c->borrowed = false;
   return true;
}

bool record_rtmp_control_status(const record_rtmp_control_t *c,
      record_rtmp_control_status_t *s)
{
   if (!c || !s || c->failed)
      return false;
   s->received = c->received;
   s->peer_ack = c->peer_ack;
   s->bandwidth = c->bandwidth;
   s->window = c->window;
   return true;
}

bool record_rtmp_control_free(record_rtmp_control_t *c)
{
   if (c && c->borrowed)
      return false;
   free(c);
   return true;
}

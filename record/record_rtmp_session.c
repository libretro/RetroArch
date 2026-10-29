/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_rtmp_session.h"
#include <stdlib.h>

enum session_output { SESSION_OUTPUT_NONE, SESSION_OUTPUT_CONTROL, SESSION_OUTPUT_PUBLISH };
struct record_rtmp_session
{
   record_rtmp_handshake_t *handshake;
   record_rtmp_publish_t *publish;
   record_rtmp_control_t *control;
   record_rtmp_receive_t *receive;
   record_rtmp_chunk_t *chunk;
   record_rtmp_span_t view;
   uint64_t sent;
   enum record_rtmp_session_state state;
   enum session_output owner;
   bool borrowed, handshake_view, reply;
};

record_rtmp_session_t *record_rtmp_session_new(const record_rtmp_publish_config_t *config,
      const uint8_t random[1528], uint32_t time, size_t max_reply, unsigned streams)
{
   record_rtmp_session_t *s = (record_rtmp_session_t*)calloc(1, sizeof(*s));
   if (!s)
      return NULL;
   s->handshake = record_rtmp_handshake_new(random, time);
   s->publish = record_rtmp_publish_new(config);
   s->control = record_rtmp_control_new();
   s->receive = record_rtmp_receive_new(max_reply, streams);
   s->chunk = record_rtmp_chunk_new(128);
   if (!s->handshake || !s->publish || !s->control || !s->receive || !s->chunk)
   {
      record_rtmp_session_free(s);
      return NULL;
   }
   return s;
}

bool record_rtmp_session_feed(record_rtmp_session_t *s,
      const void *data, size_t size, size_t *used, uint32_t time)
{
   int result;
   if (!used)
      return false;
   *used = 0;
   if (!s || s->state == RECORD_RTMP_SESSION_FAILED || (size && !data))
      return false;
   if (s->state == RECORD_RTMP_SESSION_HANDSHAKE)
      result = record_rtmp_handshake_feed(s->handshake, data, size, used, time);
   else
      result = record_rtmp_receive_feed(s->receive, data, size, used);
   if (result < 0 || !record_rtmp_control_received(s->control, *used))
   {
      s->state = RECORD_RTMP_SESSION_FAILED;
      return false;
   }
   return true;
}

bool record_rtmp_session_step(record_rtmp_session_t *s)
{
   record_rtmp_span_t span;
   record_rtmp_message_t m;
   int result;
   if (!s || s->state == RECORD_RTMP_SESSION_FAILED)
      return false;
   if (s->state == RECORD_RTMP_SESSION_HANDSHAKE)
   {
      result = record_rtmp_handshake_status(s->handshake);
      if (result == RECORD_RTMP_HANDSHAKE_ERROR)
         goto invalid;
      if (result != RECORD_RTMP_HANDSHAKE_DONE)
         return false;
      s->state = RECORD_RTMP_SESSION_NEGOTIATING;
      return true;
   }
   if (s->owner != SESSION_OUTPUT_NONE && !record_rtmp_chunk_get(s->chunk, &span))
   {
      result = s->owner == SESSION_OUTPUT_CONTROL
         ? record_rtmp_control_ack(s->control) : record_rtmp_publish_ack(s->publish);
      s->owner = SESSION_OUTPUT_NONE;
      if (!result)
         goto invalid;
      return true;
   }
   if (s->reply)
   {
      result = record_rtmp_publish_step(s->publish);
      if (result == RECORD_RTMP_PUBLISH_RELEASE_REPLY)
      {
         s->reply = false;
         if (!record_rtmp_receive_release(s->receive))
            goto invalid;
         if (record_rtmp_publish_status(s->publish, NULL, NULL) == RECORD_RTMP_PUBLISH_FAILED)
            goto invalid;
         if (record_rtmp_publish_status(s->publish, NULL, NULL) == RECORD_RTMP_PUBLISH_READY)
            s->state = RECORD_RTMP_SESSION_READY;
      }
      else if (result != RECORD_RTMP_PUBLISH_PROGRESS)
         goto invalid;
      return true;
   }
   if (record_rtmp_receive_get(s->receive, &m))
   {
      result = record_rtmp_control_handle(s->control, s->receive, &m);
      if (result == RECORD_RTMP_CONTROL_ERROR)
         goto invalid;
      if (result == RECORD_RTMP_CONTROL_HANDLED)
      {
         if (!record_rtmp_receive_release(s->receive))
            goto invalid;
         return true;
      }
      if (result == RECORD_RTMP_CONTROL_OTHER)
      {
         if (m.type != 17 && m.type != 20)
         {
            if (!record_rtmp_receive_release(s->receive))
               goto invalid;
            return true;
         }
         if (s->owner == SESSION_OUTPUT_NONE)
         {
            /* Pending commands must leave before their replies are admitted. */
            if (!record_rtmp_publish_get(s->publish, &m))
            {
               if (!record_rtmp_receive_get(s->receive, &m)
                     || !record_rtmp_publish_reply(s->publish, &m))
                  goto invalid;
               s->reply = true;
               return true;
            }
            /* get borrowed a pending command; stage it below. */
            s->owner = SESSION_OUTPUT_PUBLISH;
            if (!record_rtmp_chunk_push(s->chunk, m.chunk_id, m.stream_id,
                     m.type, m.timestamp, &m.payload, 1))
               goto invalid;
            return true;
         }
      }
   }
   if (s->owner != SESSION_OUTPUT_NONE)
      return false;
   if (record_rtmp_control_get(s->control, &m))
      s->owner = SESSION_OUTPUT_CONTROL;
   else if (record_rtmp_publish_get(s->publish, &m))
      s->owner = SESSION_OUTPUT_PUBLISH;
   else if (record_rtmp_publish_status(s->publish, NULL, NULL) == RECORD_RTMP_PUBLISH_FAILED)
      goto invalid;
   else
      return false;
   if (!record_rtmp_chunk_push(s->chunk, m.chunk_id, m.stream_id,
            m.type, m.timestamp, &m.payload, 1))
      goto invalid;
   return true;
invalid:
   s->state = RECORD_RTMP_SESSION_FAILED;
   return false;
}

bool record_rtmp_session_get(record_rtmp_session_t *s, record_rtmp_span_t *span)
{
   if (!s || !span)
      return false;
   if (!s->borrowed)
   {
      if (s->state == RECORD_RTMP_SESSION_FAILED)
         return false;
      s->handshake_view = s->state == RECORD_RTMP_SESSION_HANDSHAKE;
      if (!(s->handshake_view ? record_rtmp_handshake_get(s->handshake, &s->view)
               : record_rtmp_chunk_get(s->chunk, &s->view)))
         return false;
      s->borrowed = true;
   }
   *span = s->view;
   return true;
}

bool record_rtmp_session_consume(record_rtmp_session_t *s, size_t size)
{
   if (!s || !s->borrowed || size > s->view.size
         || (uint64_t)size > (uint64_t)-1 - s->sent)
      return false;
   if (!(s->handshake_view ? record_rtmp_handshake_consume(s->handshake, size)
            : record_rtmp_chunk_consume(s->chunk, size)))
      return false;
   s->sent += size;
   s->view.data = (const uint8_t*)s->view.data + size;
   s->view.size -= size;
   if (!s->view.size)
      s->borrowed = false;
   return true;
}

enum record_rtmp_session_state record_rtmp_session_status(
      const record_rtmp_session_t *s, uint32_t *stream, uint64_t *sent)
{
   if (stream)
      record_rtmp_publish_status(s ? s->publish : NULL, stream, NULL);
   if (sent)
      *sent = s ? s->sent : 0;
   return s ? s->state : RECORD_RTMP_SESSION_FAILED;
}

void record_rtmp_session_cancel(record_rtmp_session_t *s)
{
   if (s)
      s->state = RECORD_RTMP_SESSION_FAILED;
}

bool record_rtmp_session_discard(record_rtmp_session_t *s)
{
   if (!s || s->state != RECORD_RTMP_SESSION_FAILED)
      return false;
   s->borrowed = false;
   return true;
}

bool record_rtmp_session_free(record_rtmp_session_t *s)
{
   if (!s)
      return true;
   if (s->borrowed)
      return false;
   record_rtmp_chunk_free(s->chunk);
   record_rtmp_publish_cancel(s->publish);
   if (s->reply)
      record_rtmp_publish_step(s->publish);
   if (s->owner == SESSION_OUTPUT_PUBLISH)
      record_rtmp_publish_ack(s->publish);
   if (s->owner == SESSION_OUTPUT_CONTROL)
      record_rtmp_control_ack(s->control);
   record_rtmp_publish_free(s->publish);
   record_rtmp_control_free(s->control);
   record_rtmp_receive_free(s->receive);
   record_rtmp_handshake_free(s->handshake);
   free(s);
   return true;
}

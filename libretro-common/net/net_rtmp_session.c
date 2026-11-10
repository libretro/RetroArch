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
#include <net/net_rtmp_session.h>
#include <stdlib.h>

enum session_output
{
   SESSION_OUTPUT_NONE, SESSION_OUTPUT_CONTROL,
   SESSION_OUTPUT_PUBLISH, SESSION_OUTPUT_SIZE
};
struct net_rtmp_session
{
   net_rtmp_handshake_t *handshake;
   net_rtmp_publish_t *publish;
   net_rtmp_control_t *control;
   net_rtmp_receive_t *receive;
   net_rtmp_chunk_t *chunk, *media;
   net_rtmp_span_t view;
   uint64_t sent, acknowledged;
   uint32_t peer_ack;
   unsigned chunk_size;
   enum net_rtmp_session_state state;
   enum session_output owner;
   uint8_t size_bytes[4];
   bool borrowed, handshake_view, reply, media_active, media_complete;
   bool media_wire;
};

net_rtmp_session_t *net_rtmp_session_new(const net_rtmp_publish_config_t *config,
      const uint8_t random[1528], uint32_t time, size_t max_reply, unsigned streams)
{
   net_rtmp_session_t *s = (net_rtmp_session_t*)calloc(1, sizeof(*s));
   if (!s)
      return NULL;
   s->handshake = net_rtmp_handshake_new(random, time);
   s->publish = net_rtmp_publish_new(config);
   s->control = net_rtmp_control_new();
   s->receive = net_rtmp_receive_new(max_reply, streams);
   s->chunk = net_rtmp_chunk_new(128);
   s->media = net_rtmp_chunk_new(128);
   if (!s->handshake || !s->publish || !s->control || !s->receive
         || !s->chunk || !s->media)
   {
      net_rtmp_session_free(s);
      return NULL;
   }
   return s;
}

bool net_rtmp_session_feed(net_rtmp_session_t *s,
      const void *data, size_t size, size_t *used, uint32_t time)
{
   int result;
   if (!used)
      return false;
   *used = 0;
   if (!s || s->state == NET_RTMP_SESSION_FAILED || (size && !data))
      return false;
   if (s->state == NET_RTMP_SESSION_HANDSHAKE)
      result = net_rtmp_handshake_feed(s->handshake, data, size, used, time);
   else
      result = net_rtmp_receive_feed(s->receive, data, size, used);
   if (result < 0 || !net_rtmp_control_received(s->control, *used))
   {
      s->state = NET_RTMP_SESSION_FAILED;
      return false;
   }
   return true;
}

bool net_rtmp_session_step(net_rtmp_session_t *s)
{
   net_rtmp_span_t span;
   net_rtmp_message_t m;
   int result;
   bool progress = false;
   if (!s || s->state == NET_RTMP_SESSION_FAILED)
      return false;
   if (s->state == NET_RTMP_SESSION_HANDSHAKE)
   {
      result = net_rtmp_handshake_status(s->handshake);
      if (result == NET_RTMP_HANDSHAKE_ERROR)
         goto invalid;
      if (result != NET_RTMP_HANDSHAKE_DONE)
         return false;
      s->state = NET_RTMP_SESSION_NEGOTIATING;
      return true;
   }
   if (s->owner != SESSION_OUTPUT_NONE && !net_rtmp_chunk_get(s->chunk, &span))
   {
      if (s->owner == SESSION_OUTPUT_SIZE)
      {
         result = net_rtmp_chunk_size(s->chunk, s->chunk_size)
            && net_rtmp_chunk_size(s->media, s->chunk_size);
         s->chunk_size = 0;
      }
      else
         result = s->owner == SESSION_OUTPUT_CONTROL
            ? net_rtmp_control_ack(s->control) : net_rtmp_publish_ack(s->publish);
      s->owner = SESSION_OUTPUT_NONE;
      if (!result)
         goto invalid;
      return true;
   }
   if (s->media_active && !s->media_complete
         && !net_rtmp_chunk_get(s->media, &span))
   {
      s->media_complete = true;
      return true;
   }
   if (s->reply)
   {
      result = net_rtmp_publish_step(s->publish);
      if (result == NET_RTMP_PUBLISH_RELEASE_REPLY)
      {
         s->reply = false;
         if (!net_rtmp_receive_release(s->receive))
            goto invalid;
         if (net_rtmp_publish_status(s->publish, NULL, NULL) == NET_RTMP_PUBLISH_FAILED)
            goto invalid;
         if (net_rtmp_publish_status(s->publish, NULL, NULL) == NET_RTMP_PUBLISH_READY)
            s->state = NET_RTMP_SESSION_READY;
      }
      else if (result != NET_RTMP_PUBLISH_PROGRESS)
         goto invalid;
      return true;
   }
   if (net_rtmp_receive_get(s->receive, &m))
   {
      result = net_rtmp_control_handle(s->control, s->receive, &m);
      if (result == NET_RTMP_CONTROL_ERROR)
         goto invalid;
      if (result == NET_RTMP_CONTROL_HANDLED)
      {
         if (m.type == 3)
         {
            net_rtmp_control_status_t status;
            uint32_t delta;
            if (!net_rtmp_control_status(s->control, &status))
               goto invalid;
            delta = status.peer_ack - s->peer_ack;
            if (delta <= s->sent - s->acknowledged)
            {
               s->acknowledged += delta;
               s->peer_ack = status.peer_ack;
            }
         }
         if (!net_rtmp_receive_release(s->receive))
            goto invalid;
         progress = true;
         goto dispatch;
      }
      if (result == NET_RTMP_CONTROL_OTHER)
      {
         if (m.type != 17 && m.type != 20)
         {
            if (!net_rtmp_receive_release(s->receive))
               goto invalid;
            return true;
         }
         if (s->owner == SESSION_OUTPUT_NONE)
         {
            /* Pending commands must leave before their replies are admitted. */
            if (!net_rtmp_publish_get(s->publish, &m))
            {
               if (!net_rtmp_receive_get(s->receive, &m)
                     || !net_rtmp_publish_reply(s->publish, &m))
                  goto invalid;
               s->reply = true;
               return true;
            }
            /* get borrowed a pending command; stage it below. */
            s->owner = SESSION_OUTPUT_PUBLISH;
            if (!net_rtmp_chunk_push(s->chunk, m.chunk_id, m.stream_id,
                     m.type, m.timestamp, &m.payload, 1))
               goto invalid;
            return true;
         }
      }
   }
dispatch:
   if (s->owner != SESSION_OUTPUT_NONE)
      return progress;
   if (net_rtmp_control_get(s->control, &m))
      s->owner = SESSION_OUTPUT_CONTROL;
   else if (s->chunk_size)
   {
      s->owner = SESSION_OUTPUT_SIZE;
      m.chunk_id = 2;
      m.stream_id = m.timestamp = 0;
      m.type = 1;
      m.payload.data = s->size_bytes;
      m.payload.size = sizeof(s->size_bytes);
   }
   else if (net_rtmp_publish_get(s->publish, &m))
      s->owner = SESSION_OUTPUT_PUBLISH;
   else if (net_rtmp_publish_status(s->publish, NULL, NULL) == NET_RTMP_PUBLISH_FAILED)
      goto invalid;
   else
      return progress;
   if (!net_rtmp_chunk_push(s->chunk, m.chunk_id, m.stream_id,
            m.type, m.timestamp, &m.payload, 1))
      goto invalid;
   return true;
invalid:
   s->state = NET_RTMP_SESSION_FAILED;
   return false;
}

bool net_rtmp_session_get(net_rtmp_session_t *s, net_rtmp_span_t *span)
{
   if (!s || !span)
      return false;
   if (!s->borrowed)
   {
      if (s->state == NET_RTMP_SESSION_FAILED)
         return false;
      s->handshake_view = s->state == NET_RTMP_SESSION_HANDSHAKE;
      if (s->handshake_view)
      {
         if (!net_rtmp_handshake_get(s->handshake, &s->view))
            return false;
      }
      else
      {
         net_rtmp_control_status_t status;
         uint64_t outstanding = s->sent - s->acknowledged;
         if (!net_rtmp_control_status(s->control, &status)
               || (status.bandwidth && outstanding >= status.bandwidth))
            return false;
         if (!s->media_wire && !net_rtmp_chunk_get(s->chunk, &s->view))
         {
            if (!s->media_active || s->media_complete
                  || !net_rtmp_chunk_get(s->media, &s->view))
               return false;
            s->media_wire = true;
         }
         else if (s->media_wire && !net_rtmp_chunk_get(s->media, &s->view))
            return false;
         if (status.bandwidth && s->view.size > status.bandwidth - outstanding)
            s->view.size = (size_t)(status.bandwidth - outstanding);
      }
      s->borrowed = true;
   }
   *span = s->view;
   return true;
}

bool net_rtmp_session_consume(net_rtmp_session_t *s, size_t size)
{
   if (!s || !s->borrowed || size > s->view.size
         || (uint64_t)size > (uint64_t)-1 - s->sent)
      return false;
   if (!(s->handshake_view ? net_rtmp_handshake_consume(s->handshake, size)
            : net_rtmp_chunk_consume(s->media_wire ? s->media : s->chunk, size)))
      return false;
   s->sent += size;
   s->view.data = (const uint8_t*)s->view.data + size;
   s->view.size -= size;
   if (!s->view.size)
   {
      s->borrowed = false;
      if (s->media_wire && net_rtmp_chunk_boundary(s->media))
         s->media_wire = false;
   }
   return true;
}

bool net_rtmp_session_chunk_size(net_rtmp_session_t *s, unsigned size)
{
   if (!s || s->state != NET_RTMP_SESSION_READY || s->media_active
         || s->chunk_size || !size || size > 0x7fffffff)
      return false;
   s->chunk_size = size;
   s->size_bytes[0] = (uint8_t)(size >> 24);
   s->size_bytes[1] = (uint8_t)(size >> 16);
   s->size_bytes[2] = (uint8_t)(size >> 8);
   s->size_bytes[3] = (uint8_t)size;
   return true;
}

bool net_rtmp_session_media(net_rtmp_session_t *s, unsigned type,
      uint32_t timestamp, const net_rtmp_span_t *spans, size_t count)
{
   uint32_t stream;
   unsigned id = type == 8 ? 4 : type == 9 ? 6 : 5;
   if (!s || s->state != NET_RTMP_SESSION_READY || s->media_active || s->chunk_size
         || (type != 8 && type != 9 && type != 18) || !count
         || net_rtmp_publish_status(s->publish, &stream, NULL)
            != NET_RTMP_PUBLISH_READY
         || !net_rtmp_chunk_push(s->media, id, stream, type, timestamp, spans, count))
      return false;
   s->media_active = true;
   s->media_complete = false;
   return true;
}

bool net_rtmp_session_media_release(net_rtmp_session_t *s)
{
   if (!s || !s->media_active || s->borrowed
         || (!s->media_complete && s->state != NET_RTMP_SESSION_FAILED))
      return false;
   s->media_active = false;
   return true;
}

enum net_rtmp_session_state net_rtmp_session_status(
      const net_rtmp_session_t *s, uint32_t *stream, uint64_t *sent)
{
   if (stream)
      net_rtmp_publish_status(s ? s->publish : NULL, stream, NULL);
   if (sent)
      *sent = s ? s->sent : 0;
   return s ? s->state : NET_RTMP_SESSION_FAILED;
}

void net_rtmp_session_cancel(net_rtmp_session_t *s)
{
   if (s)
      s->state = NET_RTMP_SESSION_FAILED;
}

bool net_rtmp_session_discard(net_rtmp_session_t *s)
{
   if (!s || s->state != NET_RTMP_SESSION_FAILED)
      return false;
   s->borrowed = false;
   return true;
}

bool net_rtmp_session_free(net_rtmp_session_t *s)
{
   if (!s)
      return true;
   if (s->borrowed || s->media_active)
      return false;
   net_rtmp_chunk_free(s->chunk);
   net_rtmp_chunk_free(s->media);
   net_rtmp_publish_cancel(s->publish);
   if (s->reply)
      net_rtmp_publish_step(s->publish);
   if (s->owner == SESSION_OUTPUT_PUBLISH)
      net_rtmp_publish_ack(s->publish);
   if (s->owner == SESSION_OUTPUT_CONTROL)
      net_rtmp_control_ack(s->control);
   net_rtmp_publish_free(s->publish);
   net_rtmp_control_free(s->control);
   net_rtmp_receive_free(s->receive);
   net_rtmp_handshake_free(s->handshake);
   free(s);
   return true;
}

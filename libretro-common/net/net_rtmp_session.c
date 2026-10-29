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

enum session_output { SESSION_OUTPUT_NONE, SESSION_OUTPUT_CONTROL, SESSION_OUTPUT_PUBLISH };
struct net_rtmp_session
{
   net_rtmp_handshake_t *handshake;
   net_rtmp_publish_t *publish;
   net_rtmp_control_t *control;
   net_rtmp_receive_t *receive;
   net_rtmp_chunk_t *chunk;
   net_rtmp_span_t view;
   uint64_t sent;
   enum net_rtmp_session_state state;
   enum session_output owner;
   bool borrowed, handshake_view, reply;
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
   if (!s->handshake || !s->publish || !s->control || !s->receive || !s->chunk)
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
      result = s->owner == SESSION_OUTPUT_CONTROL
         ? net_rtmp_control_ack(s->control) : net_rtmp_publish_ack(s->publish);
      s->owner = SESSION_OUTPUT_NONE;
      if (!result)
         goto invalid;
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
         if (!net_rtmp_receive_release(s->receive))
            goto invalid;
         return true;
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
   if (s->owner != SESSION_OUTPUT_NONE)
      return false;
   if (net_rtmp_control_get(s->control, &m))
      s->owner = SESSION_OUTPUT_CONTROL;
   else if (net_rtmp_publish_get(s->publish, &m))
      s->owner = SESSION_OUTPUT_PUBLISH;
   else if (net_rtmp_publish_status(s->publish, NULL, NULL) == NET_RTMP_PUBLISH_FAILED)
      goto invalid;
   else
      return false;
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
      if (!(s->handshake_view ? net_rtmp_handshake_get(s->handshake, &s->view)
               : net_rtmp_chunk_get(s->chunk, &s->view)))
         return false;
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
            : net_rtmp_chunk_consume(s->chunk, size)))
      return false;
   s->sent += size;
   s->view.data = (const uint8_t*)s->view.data + size;
   s->view.size -= size;
   if (!s->view.size)
      s->borrowed = false;
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
   if (s->borrowed)
      return false;
   net_rtmp_chunk_free(s->chunk);
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

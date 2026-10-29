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
#include <net/net_rtmp_socket.h>
#include <net/net_socket.h>
#include <stdlib.h>

struct net_rtmp_socket
{
   net_rtmp_session_t *session;
   size_t offset, size;
   int fd;
   uint8_t input[4096];
   bool failed;
};

net_rtmp_socket_t *net_rtmp_socket_new(net_rtmp_session_t *session, int fd)
{
   net_rtmp_socket_t *s;
   if (!session || fd < 0
         || net_rtmp_session_status(session, NULL, NULL) == NET_RTMP_SESSION_FAILED)
      return NULL;
   s = (net_rtmp_socket_t*)calloc(1, sizeof(*s));
   if (!s)
      return NULL;
   if (!socket_nonblock(fd))
   {
      free(s);
      return NULL;
   }
   s->session = session;
   s->fd = fd;
   return s;
}

int net_rtmp_socket_step(net_rtmp_socket_t *s, uint32_t time)
{
   net_rtmp_span_t span;
   ssize_t count;
   size_t used, limit;
   bool error = false, progress;
   if (!s || s->failed)
      return NET_RTMP_SOCKET_ERROR;
   progress = net_rtmp_session_step(s->session);
   if (s->offset == s->size)
   {
      count = socket_receive_all_nonblocking(s->fd, &error, s->input, sizeof(s->input));
      if (count < 0 || error || (size_t)count > sizeof(s->input))
         goto failed;
      s->offset = 0;
      s->size = (size_t)count;
      if (count)
         progress = true;
   }
   if (s->offset < s->size)
   {
      if (!net_rtmp_session_feed(s->session, s->input + s->offset,
               s->size - s->offset, &used, time))
         goto failed;
      s->offset += used;
      if (used)
         progress = true;
   }
   if (net_rtmp_session_step(s->session))
      progress = true;
   if (net_rtmp_session_status(s->session, NULL, NULL) == NET_RTMP_SESSION_FAILED)
      goto failed;
   if (net_rtmp_session_get(s->session, &span))
   {
      limit = span.size < 4096 ? span.size : 4096;
      count = socket_send_all_nonblocking(s->fd, span.data, limit, true);
      if (count < 0 || (size_t)count > limit
            || !net_rtmp_session_consume(s->session, (size_t)count))
         goto failed;
      if (count)
         progress = true;
   }
   return progress ? NET_RTMP_SOCKET_PROGRESS : NET_RTMP_SOCKET_IDLE;
failed:
   s->failed = true;
   net_rtmp_session_cancel(s->session);
   net_rtmp_session_discard(s->session);
   return NET_RTMP_SOCKET_ERROR;
}

void net_rtmp_socket_free(net_rtmp_socket_t *s)
{
   if (!s)
      return;
   net_rtmp_session_cancel(s->session);
   net_rtmp_session_discard(s->session);
   free(s);
}

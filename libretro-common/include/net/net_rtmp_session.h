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
#ifndef NET_RTMP_SESSION_H
#define NET_RTMP_SESSION_H
#include <net/net_rtmp_publish.h>
#include <net/net_rtmp_control.h>
#include <net/net_rtmp_handshake.h>

typedef struct net_rtmp_session net_rtmp_session_t;
enum net_rtmp_session_state
{
   NET_RTMP_SESSION_FAILED = -1,
   NET_RTMP_SESSION_HANDSHAKE,
   NET_RTMP_SESSION_NEGOTIATING,
   NET_RTMP_SESSION_READY
};
RETRO_BEGIN_DECLS
/* One worker; cold allocation. Copies configuration and handshake random.
 * No socket ownership, internal clock or I/O. */
net_rtmp_session_t *net_rtmp_session_new(const net_rtmp_publish_config_t *config,
      const uint8_t random[1528], uint32_t time, size_t max_reply, unsigned streams);
/* One handshake/receive segment. Zero consumption requires step/output work. */
bool net_rtmp_session_feed(net_rtmp_session_t *session,
      const void *data, size_t size, size_t *consumed, uint32_t receive_time);
/* One output retirement, command dispatch or reply-parser step. True means
 * local progress; false requires transport activity or reports FAILED. */
bool net_rtmp_session_step(net_rtmp_session_t *session);
/* Borrow one output span. Partial consumption keeps its remaining bytes live. */
bool net_rtmp_session_get(net_rtmp_session_t *session, net_rtmp_span_t *span);
bool net_rtmp_session_consume(net_rtmp_session_t *session, size_t size);
enum net_rtmp_session_state net_rtmp_session_status(
      const net_rtmp_session_t *session, uint32_t *stream, uint64_t *sent);
void net_rtmp_session_cancel(net_rtmp_session_t *session);
/* After cancellation, retire a transport borrow without reporting it sent. */
bool net_rtmp_session_discard(net_rtmp_session_t *session);
/* Refuses a live transport borrow. Releases internal borrows before freeing. */
bool net_rtmp_session_free(net_rtmp_session_t *session);
RETRO_END_DECLS
#endif

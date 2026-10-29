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
#ifndef NET_RTMP_SOCKET_H
#define NET_RTMP_SOCKET_H
#include <net/net_rtmp_session.h>

typedef struct net_rtmp_socket net_rtmp_socket_t;
enum net_rtmp_socket_result
{
   NET_RTMP_SOCKET_ERROR = -1,
   NET_RTMP_SOCKET_IDLE,
   NET_RTMP_SOCKET_PROGRESS
};
RETRO_BEGIN_DECLS
/* Borrows an already connected descriptor and its session. Makes the socket
 * nonblocking; neither name resolution nor connection setup occurs here.
 * This worker is the session's sole I/O owner until free. */
net_rtmp_socket_t *net_rtmp_socket_new(net_rtmp_session_t *session, int fd);
/* One bounded receive/feed/send pass. No allocation, poll, wait or clock. */
int net_rtmp_socket_step(net_rtmp_socket_t *socket, uint32_t receive_time);
/* Retires this pump's session borrows and cancels it. Caller then closes the
 * descriptor and frees the session. No asynchronous socket operation survives. */
void net_rtmp_socket_free(net_rtmp_socket_t *socket);
RETRO_END_DECLS
#endif

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
#ifndef NET_RTMP_PUBLISH_H
#define NET_RTMP_PUBLISH_H
#include <net/net_rtmp_command.h>
#include <net/net_rtmp_receive.h>
#include <net/net_rtmp_reply.h>

typedef struct net_rtmp_publish net_rtmp_publish_t;
typedef struct net_rtmp_publish_config
{
   const char *application, *url, *name;
   bool legacy_commands;
} net_rtmp_publish_config_t;
enum net_rtmp_publish_state
{
   NET_RTMP_PUBLISH_NEGOTIATING,
   NET_RTMP_PUBLISH_READY,
   NET_RTMP_PUBLISH_FAILED
};
enum net_rtmp_publish_error
{
   NET_RTMP_PUBLISH_ERROR_NONE,
   NET_RTMP_PUBLISH_ERROR_PROTOCOL,
   NET_RTMP_PUBLISH_ERROR_REJECTED,
   NET_RTMP_PUBLISH_ERROR_CANCELLED
};
enum net_rtmp_publish_result
{
   NET_RTMP_PUBLISH_ERROR = -1,
   NET_RTMP_PUBLISH_BUSY,
   NET_RTMP_PUBLISH_PROGRESS,
   NET_RTMP_PUBLISH_RELEASE_REPLY,
   NET_RTMP_PUBLISH_ESTABLISHED
};
RETRO_BEGIN_DECLS
/* Single worker, after handshake. Copies nonempty strings (up to4096 bytes
 * each) at initialization. Does not own the socket, chunk sender or receiver. */
net_rtmp_publish_t *net_rtmp_publish_new(const net_rtmp_publish_config_t *config);
/* Borrows one outgoing command until ack, including across cancellation.
 * Ack only after transmission completes or its transport borrow is retired. */
bool net_rtmp_publish_get(net_rtmp_publish_t *publish, net_rtmp_message_t *message);
bool net_rtmp_publish_ack(net_rtmp_publish_t *publish);
/* Complete command replies only. True borrows the receiver message until
 * step returns RELEASE_REPLY, including errors/cancellation. False takes none. */
bool net_rtmp_publish_reply(net_rtmp_publish_t *publish, const net_rtmp_message_t *message);
/* One AMF parsing step or one reply transition. BUSY requires external I/O.
 * No I/O, allocation, wait or clock here. Control messages are handled outside. */
int net_rtmp_publish_step(net_rtmp_publish_t *publish);
enum net_rtmp_publish_state net_rtmp_publish_status(const net_rtmp_publish_t *publish,
      uint32_t *stream, enum net_rtmp_publish_error *error);
void net_rtmp_publish_cancel(net_rtmp_publish_t *publish);
/* Refuses outstanding input/output borrows. Unborrowed pending output is safe. */
bool net_rtmp_publish_free(net_rtmp_publish_t *publish);
RETRO_END_DECLS
#endif

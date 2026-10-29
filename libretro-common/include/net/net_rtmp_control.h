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
#ifndef NET_RTMP_CONTROL_H
#define NET_RTMP_CONTROL_H
#include <net/net_rtmp_receive.h>

typedef struct net_rtmp_control net_rtmp_control_t;
typedef struct net_rtmp_control_status
{
   uint64_t received;
   uint32_t peer_ack, bandwidth, window;
} net_rtmp_control_status_t;

enum net_rtmp_control_result
{
   NET_RTMP_CONTROL_ERROR = -1,
   NET_RTMP_CONTROL_BUSY,
   NET_RTMP_CONTROL_HANDLED,
   NET_RTMP_CONTROL_OTHER
};

RETRO_BEGIN_DECLS
/* Single worker. Counts transport bytes once, including handshake bytes. */
net_rtmp_control_t *net_rtmp_control_new(void);
bool net_rtmp_control_received(net_rtmp_control_t *control, size_t size);
/* Complete messages only. BUSY takes nothing; caller retains the input.
 * Applies receive chunk controls before the next input chunk is parsed. */
int net_rtmp_control_handle(net_rtmp_control_t *control,
      net_rtmp_receive_t *receive, const net_rtmp_message_t *message);
/* Borrow response until ack, after all output bytes leave the transmitter.
 * Short writes keep the same response; newly received bytes remain counted. */
bool net_rtmp_control_get(net_rtmp_control_t *control, net_rtmp_message_t *message);
bool net_rtmp_control_ack(net_rtmp_control_t *control);
bool net_rtmp_control_status(const net_rtmp_control_t *control,
      net_rtmp_control_status_t *status);
bool net_rtmp_control_free(net_rtmp_control_t *control);
RETRO_END_DECLS
#endif

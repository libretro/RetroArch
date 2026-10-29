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
#ifndef NET_RTMP_HANDSHAKE_H
#define NET_RTMP_HANDSHAKE_H
#include <net/net_rtmp_chunk.h>

typedef struct net_rtmp_handshake net_rtmp_handshake_t;
enum net_rtmp_handshake_result
{
   NET_RTMP_HANDSHAKE_ERROR = -1,
   NET_RTMP_HANDSHAKE_PENDING,
   NET_RTMP_HANDSHAKE_DONE
};
RETRO_BEGIN_DECLS
/* Worker-owned version3 simple handshake. Copies 1528 caller-supplied random
 * bytes at startup. Times are caller-supplied connection-relative milliseconds. */
net_rtmp_handshake_t *net_rtmp_handshake_new(
      const uint8_t random[1528], uint32_t time);
bool net_rtmp_handshake_get(net_rtmp_handshake_t *handshake,
      net_rtmp_span_t *span);
bool net_rtmp_handshake_consume(net_rtmp_handshake_t *handshake, size_t size);
/* Consumes at most one S0/S1/S2 segment per call. Leaves following chunk data
 * untouched. No I/O, allocation, wait or internal clock. Errors are terminal. */
int net_rtmp_handshake_feed(net_rtmp_handshake_t *handshake,
      const void *data, size_t size, size_t *consumed, uint32_t receive_time);
int net_rtmp_handshake_status(const net_rtmp_handshake_t *handshake);
void net_rtmp_handshake_free(net_rtmp_handshake_t *handshake);
RETRO_END_DECLS
#endif

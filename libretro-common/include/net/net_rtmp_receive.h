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
#ifndef NET_RTMP_RECEIVE_H
#define NET_RTMP_RECEIVE_H
#include <net/net_rtmp_chunk.h>

typedef struct net_rtmp_receive net_rtmp_receive_t;
typedef struct net_rtmp_message
{
   net_rtmp_span_t payload;
   uint32_t timestamp, stream_id;
   unsigned chunk_id, type;
} net_rtmp_message_t;
enum net_rtmp_receive_result
{
   NET_RTMP_RECEIVE_ERROR = -1,
   NET_RTMP_RECEIVE_MORE,
   NET_RTMP_RECEIVE_MESSAGE
};
RETRO_BEGIN_DECLS
/* Worker-owned. Preallocates per-stream message storage; 1..64 cached streams.
 * Oversized messages or too many distinct streams fail rather than allocate.
 * Initial incoming chunk size is 128. */
net_rtmp_receive_t *net_rtmp_receive_new(size_t max_message, unsigned streams);
/* One header segment or at most 4096 payload bytes per call. Pending output
 * consumes nothing until released. Protocol errors are sticky. No I/O/wait. */
int net_rtmp_receive_feed(net_rtmp_receive_t *receive,
      const void *data, size_t size, size_t *consumed);
bool net_rtmp_receive_get(net_rtmp_receive_t *receive, net_rtmp_message_t *message);
bool net_rtmp_receive_release(net_rtmp_receive_t *receive);
/* Apply validated Set Chunk Size / Abort controls at a chunk boundary. */
bool net_rtmp_receive_size(net_rtmp_receive_t *receive, unsigned size);
bool net_rtmp_receive_abort(net_rtmp_receive_t *receive, unsigned chunk_id);
/* End of input succeeds only after all complete output is acknowledged. */
bool net_rtmp_receive_finish(net_rtmp_receive_t *receive);
void net_rtmp_receive_free(net_rtmp_receive_t *receive);
RETRO_END_DECLS
#endif

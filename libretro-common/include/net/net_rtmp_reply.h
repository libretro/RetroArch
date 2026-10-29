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
#ifndef NET_RTMP_REPLY_H
#define NET_RTMP_REPLY_H
#include <net/net_rtmp_chunk.h>

typedef struct net_rtmp_reply net_rtmp_reply_t;
typedef struct net_rtmp_reply_info
{
   net_rtmp_span_t command, code, level, description;
   uint32_t transaction, stream;
   bool stream_valid;
} net_rtmp_reply_info_t;
enum net_rtmp_reply_result
{
   NET_RTMP_REPLY_ERROR = -1,
   NET_RTMP_REPLY_PROGRESS,
   NET_RTMP_REPLY_DONE
};
RETRO_BEGIN_DECLS
net_rtmp_reply_t *net_rtmp_reply_new(void);
/* Borrows a complete command message until release/free. Type20 is AMF0;
 * type17 accepts format selector0 with AMF0 values. AMF3 values are rejected. */
bool net_rtmp_reply_begin(net_rtmp_reply_t *reply, unsigned type,
      const void *data, size_t size);
/* One property, value or container close per step; nesting is limited to8.
 * No allocation, I/O or wait. Strings remain byte spans, not C strings. */
int net_rtmp_reply_step(net_rtmp_reply_t *reply);
/* Available only after DONE. Status fields come only from the top-level
 * fourth argument object; nested lookalikes cannot replace them. */
bool net_rtmp_reply_get(net_rtmp_reply_t *reply, net_rtmp_reply_info_t *info);
void net_rtmp_reply_release(net_rtmp_reply_t *reply);
void net_rtmp_reply_free(net_rtmp_reply_t *reply);
RETRO_END_DECLS
#endif

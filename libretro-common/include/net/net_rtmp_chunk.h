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
#ifndef NET_RTMP_CHUNK_H
#define NET_RTMP_CHUNK_H
#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

typedef struct net_rtmp_chunk net_rtmp_chunk_t;
typedef struct net_rtmp_span
{
   const void *data;
   size_t size;
} net_rtmp_span_t;

RETRO_BEGIN_DECLS
/* Single worker owner. The peer must be told before changing chunk size. */
net_rtmp_chunk_t *net_rtmp_chunk_new(unsigned chunk_size);
bool net_rtmp_chunk_size(net_rtmp_chunk_t *chunk, unsigned chunk_size);
/* Up to three spans, borrowed until all output is consumed or free.
 * False takes nothing. Stream ID is the RTMP message stream, not chunk ID. */
bool net_rtmp_chunk_push(net_rtmp_chunk_t *chunk, unsigned chunk_id,
      uint32_t stream_id, unsigned type, uint32_t timestamp,
      const net_rtmp_span_t *spans, size_t count);
/* One header or borrowed payload span, limited to the current chunk.
 * Partial consumption supports short writes; zero consumption is a no-op.
 * No allocation, copy of payload, I/O, loop over chunks or wait. */
bool net_rtmp_chunk_get(net_rtmp_chunk_t *chunk, net_rtmp_span_t *span);
bool net_rtmp_chunk_consume(net_rtmp_chunk_t *chunk, size_t size);
void net_rtmp_chunk_free(net_rtmp_chunk_t *chunk);
RETRO_END_DECLS
#endif

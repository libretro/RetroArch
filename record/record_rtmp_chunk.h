/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_RTMP_CHUNK_H
#define RECORD_RTMP_CHUNK_H
#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

typedef struct record_rtmp_chunk record_rtmp_chunk_t;
typedef struct record_rtmp_span
{
   const void *data;
   size_t size;
} record_rtmp_span_t;

RETRO_BEGIN_DECLS
/* Single worker owner. The peer must be told before changing chunk size. */
record_rtmp_chunk_t *record_rtmp_chunk_new(unsigned chunk_size);
bool record_rtmp_chunk_size(record_rtmp_chunk_t *chunk, unsigned chunk_size);
/* Up to three spans, borrowed until all output is consumed or free.
 * False takes nothing. Stream ID is the RTMP message stream, not chunk ID. */
bool record_rtmp_chunk_push(record_rtmp_chunk_t *chunk, unsigned chunk_id,
      uint32_t stream_id, unsigned type, uint32_t timestamp,
      const record_rtmp_span_t *spans, size_t count);
/* One header or borrowed payload span, limited to the current chunk.
 * Partial consumption supports short writes; zero consumption is a no-op.
 * No allocation, copy of payload, I/O, loop over chunks or wait. */
bool record_rtmp_chunk_get(record_rtmp_chunk_t *chunk, record_rtmp_span_t *span);
bool record_rtmp_chunk_consume(record_rtmp_chunk_t *chunk, size_t size);
void record_rtmp_chunk_free(record_rtmp_chunk_t *chunk);
RETRO_END_DECLS
#endif

/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_RTMP_RECEIVE_H
#define RECORD_RTMP_RECEIVE_H
#include "record_rtmp_chunk.h"

typedef struct record_rtmp_receive record_rtmp_receive_t;
typedef struct record_rtmp_message
{
   record_rtmp_span_t payload;
   uint32_t timestamp, stream_id;
   unsigned chunk_id, type;
} record_rtmp_message_t;
enum record_rtmp_receive_result
{
   RECORD_RTMP_RECEIVE_ERROR = -1,
   RECORD_RTMP_RECEIVE_MORE,
   RECORD_RTMP_RECEIVE_MESSAGE
};
RETRO_BEGIN_DECLS
/* Worker-owned. Preallocates per-stream message storage; 1..64 cached streams.
 * Oversized messages or too many distinct streams fail rather than allocate.
 * Initial incoming chunk size is 128. */
record_rtmp_receive_t *record_rtmp_receive_new(size_t max_message, unsigned streams);
/* One header segment or at most 4096 payload bytes per call. Pending output
 * consumes nothing until released. Protocol errors are sticky. No I/O/wait. */
int record_rtmp_receive_feed(record_rtmp_receive_t *receive,
      const void *data, size_t size, size_t *consumed);
bool record_rtmp_receive_get(record_rtmp_receive_t *receive, record_rtmp_message_t *message);
bool record_rtmp_receive_release(record_rtmp_receive_t *receive);
/* Apply validated Set Chunk Size / Abort controls at a chunk boundary. */
bool record_rtmp_receive_size(record_rtmp_receive_t *receive, unsigned size);
bool record_rtmp_receive_abort(record_rtmp_receive_t *receive, unsigned chunk_id);
/* End of input succeeds only after all complete output is acknowledged. */
bool record_rtmp_receive_finish(record_rtmp_receive_t *receive);
void record_rtmp_receive_free(record_rtmp_receive_t *receive);
RETRO_END_DECLS
#endif

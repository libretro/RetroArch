/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_RTMP_REPLY_H
#define RECORD_RTMP_REPLY_H
#include "record_rtmp_chunk.h"

typedef struct record_rtmp_reply record_rtmp_reply_t;
typedef struct record_rtmp_reply_info
{
   record_rtmp_span_t command, code, level, description;
   uint32_t transaction, stream;
   bool stream_valid;
} record_rtmp_reply_info_t;
enum record_rtmp_reply_result
{
   RECORD_RTMP_REPLY_ERROR = -1,
   RECORD_RTMP_REPLY_PROGRESS,
   RECORD_RTMP_REPLY_DONE
};
RETRO_BEGIN_DECLS
record_rtmp_reply_t *record_rtmp_reply_new(void);
/* Borrows a complete command message until release/free. Type20 is AMF0;
 * type17 accepts format selector0 with AMF0 values. AMF3 values are rejected. */
bool record_rtmp_reply_begin(record_rtmp_reply_t *reply, unsigned type,
      const void *data, size_t size);
/* One property, value or container close per step; nesting is limited to8.
 * No allocation, I/O or wait. Strings remain byte spans, not C strings. */
int record_rtmp_reply_step(record_rtmp_reply_t *reply);
/* Available only after DONE. Status fields come only from the top-level
 * fourth argument object; nested lookalikes cannot replace them. */
bool record_rtmp_reply_get(record_rtmp_reply_t *reply, record_rtmp_reply_info_t *info);
void record_rtmp_reply_release(record_rtmp_reply_t *reply);
void record_rtmp_reply_free(record_rtmp_reply_t *reply);
RETRO_END_DECLS
#endif

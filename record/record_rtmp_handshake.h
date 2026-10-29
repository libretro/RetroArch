/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_RTMP_HANDSHAKE_H
#define RECORD_RTMP_HANDSHAKE_H
#include "record_rtmp_chunk.h"

typedef struct record_rtmp_handshake record_rtmp_handshake_t;
enum record_rtmp_handshake_result
{
   RECORD_RTMP_HANDSHAKE_ERROR = -1,
   RECORD_RTMP_HANDSHAKE_PENDING,
   RECORD_RTMP_HANDSHAKE_DONE
};
RETRO_BEGIN_DECLS
/* Worker-owned version3 simple handshake. Copies 1528 caller-supplied random
 * bytes at startup. Times are caller-supplied connection-relative milliseconds. */
record_rtmp_handshake_t *record_rtmp_handshake_new(
      const uint8_t random[1528], uint32_t time);
bool record_rtmp_handshake_get(record_rtmp_handshake_t *handshake,
      record_rtmp_span_t *span);
bool record_rtmp_handshake_consume(record_rtmp_handshake_t *handshake, size_t size);
/* Consumes at most one S0/S1/S2 segment per call. Leaves following chunk data
 * untouched. No I/O, allocation, wait or internal clock. Errors are terminal. */
int record_rtmp_handshake_feed(record_rtmp_handshake_t *handshake,
      const void *data, size_t size, size_t *consumed, uint32_t receive_time);
int record_rtmp_handshake_status(const record_rtmp_handshake_t *handshake);
void record_rtmp_handshake_free(record_rtmp_handshake_t *handshake);
RETRO_END_DECLS
#endif

/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_RTMP_SESSION_H
#define RECORD_RTMP_SESSION_H
#include "record_rtmp_publish.h"
#include "record_rtmp_control.h"
#include "record_rtmp_handshake.h"

typedef struct record_rtmp_session record_rtmp_session_t;
enum record_rtmp_session_state
{
   RECORD_RTMP_SESSION_FAILED = -1,
   RECORD_RTMP_SESSION_HANDSHAKE,
   RECORD_RTMP_SESSION_NEGOTIATING,
   RECORD_RTMP_SESSION_READY
};
RETRO_BEGIN_DECLS
/* One worker; cold allocation. Copies configuration and handshake random.
 * No socket ownership, internal clock or I/O. */
record_rtmp_session_t *record_rtmp_session_new(const record_rtmp_publish_config_t *config,
      const uint8_t random[1528], uint32_t time, size_t max_reply, unsigned streams);
/* One handshake/receive segment. Zero consumption requires step/output work. */
bool record_rtmp_session_feed(record_rtmp_session_t *session,
      const void *data, size_t size, size_t *consumed, uint32_t receive_time);
/* One output retirement, command dispatch or reply-parser step. True means
 * local progress; false requires transport activity or reports FAILED. */
bool record_rtmp_session_step(record_rtmp_session_t *session);
/* Borrow one output span. Partial consumption keeps its remaining bytes live. */
bool record_rtmp_session_get(record_rtmp_session_t *session, record_rtmp_span_t *span);
bool record_rtmp_session_consume(record_rtmp_session_t *session, size_t size);
enum record_rtmp_session_state record_rtmp_session_status(
      const record_rtmp_session_t *session, uint32_t *stream, uint64_t *sent);
void record_rtmp_session_cancel(record_rtmp_session_t *session);
/* After cancellation, retire a transport borrow without reporting it sent. */
bool record_rtmp_session_discard(record_rtmp_session_t *session);
/* Refuses a live transport borrow. Releases internal borrows before freeing. */
bool record_rtmp_session_free(record_rtmp_session_t *session);
RETRO_END_DECLS
#endif

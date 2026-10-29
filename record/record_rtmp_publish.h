/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_RTMP_PUBLISH_H
#define RECORD_RTMP_PUBLISH_H
#include "record_rtmp_command.h"
#include "record_rtmp_receive.h"
#include "record_rtmp_reply.h"

typedef struct record_rtmp_publish record_rtmp_publish_t;
typedef struct record_rtmp_publish_config
{
   const char *application, *url, *name;
   bool legacy_commands;
} record_rtmp_publish_config_t;
enum record_rtmp_publish_state
{
   RECORD_RTMP_PUBLISH_NEGOTIATING,
   RECORD_RTMP_PUBLISH_READY,
   RECORD_RTMP_PUBLISH_FAILED
};
enum record_rtmp_publish_error
{
   RECORD_RTMP_PUBLISH_ERROR_NONE,
   RECORD_RTMP_PUBLISH_ERROR_PROTOCOL,
   RECORD_RTMP_PUBLISH_ERROR_REJECTED,
   RECORD_RTMP_PUBLISH_ERROR_CANCELLED
};
enum record_rtmp_publish_result
{
   RECORD_RTMP_PUBLISH_ERROR = -1,
   RECORD_RTMP_PUBLISH_BUSY,
   RECORD_RTMP_PUBLISH_PROGRESS,
   RECORD_RTMP_PUBLISH_RELEASE_REPLY,
   RECORD_RTMP_PUBLISH_ESTABLISHED
};
RETRO_BEGIN_DECLS
/* Single worker, after handshake. Copies nonempty strings (up to4096 bytes
 * each) at initialization. Does not own the socket, chunk sender or receiver. */
record_rtmp_publish_t *record_rtmp_publish_new(const record_rtmp_publish_config_t *config);
/* Borrows one outgoing command until ack, including across cancellation.
 * Ack only after transmission completes or its transport borrow is retired. */
bool record_rtmp_publish_get(record_rtmp_publish_t *publish, record_rtmp_message_t *message);
bool record_rtmp_publish_ack(record_rtmp_publish_t *publish);
/* Complete command replies only. True borrows the receiver message until
 * step returns RELEASE_REPLY, including errors/cancellation. False takes none. */
bool record_rtmp_publish_reply(record_rtmp_publish_t *publish, const record_rtmp_message_t *message);
/* One AMF parsing step or one reply transition. BUSY requires external I/O.
 * No I/O, allocation, wait or clock here. Control messages are handled outside. */
int record_rtmp_publish_step(record_rtmp_publish_t *publish);
enum record_rtmp_publish_state record_rtmp_publish_status(const record_rtmp_publish_t *publish,
      uint32_t *stream, enum record_rtmp_publish_error *error);
void record_rtmp_publish_cancel(record_rtmp_publish_t *publish);
/* Refuses outstanding input/output borrows. Unborrowed pending output is safe. */
bool record_rtmp_publish_free(record_rtmp_publish_t *publish);
RETRO_END_DECLS
#endif

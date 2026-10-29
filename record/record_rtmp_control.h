/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_RTMP_CONTROL_H
#define RECORD_RTMP_CONTROL_H
#include "record_rtmp_receive.h"

typedef struct record_rtmp_control record_rtmp_control_t;
typedef struct record_rtmp_control_status
{
   uint64_t received;
   uint32_t peer_ack, bandwidth, window;
} record_rtmp_control_status_t;

enum record_rtmp_control_result
{
   RECORD_RTMP_CONTROL_ERROR = -1,
   RECORD_RTMP_CONTROL_BUSY,
   RECORD_RTMP_CONTROL_HANDLED,
   RECORD_RTMP_CONTROL_OTHER
};

RETRO_BEGIN_DECLS
/* Single worker. Counts transport bytes once, including handshake bytes. */
record_rtmp_control_t *record_rtmp_control_new(void);
bool record_rtmp_control_received(record_rtmp_control_t *control, size_t size);
/* Complete messages only. BUSY takes nothing; caller retains the input.
 * Applies receive chunk controls before the next input chunk is parsed. */
int record_rtmp_control_handle(record_rtmp_control_t *control,
      record_rtmp_receive_t *receive, const record_rtmp_message_t *message);
/* Borrow response until ack, after all output bytes leave the transmitter.
 * Short writes keep the same response; newly received bytes remain counted. */
bool record_rtmp_control_get(record_rtmp_control_t *control, record_rtmp_message_t *message);
bool record_rtmp_control_ack(record_rtmp_control_t *control);
bool record_rtmp_control_status(const record_rtmp_control_t *control,
      record_rtmp_control_status_t *status);
bool record_rtmp_control_free(record_rtmp_control_t *control);
RETRO_END_DECLS
#endif

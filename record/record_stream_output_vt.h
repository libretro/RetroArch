/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_STREAM_OUTPUT_VT_H
#define RECORD_STREAM_OUTPUT_VT_H
#include "record_stream_vt.h"
#include "record_avfoundation_session.h"
RETRO_BEGIN_DECLS
typedef struct record_stream_output_vt record_stream_output_vt_t;
/* Dedicated I/O worker. Success adopts the READY client, optional fresh AAC
 * and SRC; failure adopts nothing. Borrows video, source queue and native
 * format through output retirement. The session owner destroys those later. */
record_stream_output_vt_t *record_stream_output_vt_new(record_gpu_worker_vt_t *video,
      record_at_audio_t *audio, record_at_resample_t *converter,
      net_rtmp_client_t *client, const record_stream_vt_config_t *config,
      record_audio_queue_t *source);
/* Installs only step/cancel/free/retry/network. Native open and context/
 * encoder destruction callbacks remain the session owner's responsibility. */
bool record_stream_output_vt_set_ops(record_avfoundation_session_output_ops_t *ops);
/* I/O owner only, before free. Sticky transport/encoder error. */
enum record_stream_vt_error record_stream_output_vt_error(const record_stream_output_vt_t *output);
RETRO_END_DECLS
#endif

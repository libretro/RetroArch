/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_STREAM_D3D12_H
#define RECORD_STREAM_D3D12_H
#include "record_gpu_output_d3d12.h"
#include <net/net_rtmp_client.h>

typedef struct record_stream_d3d12 record_stream_d3d12_t;
typedef struct record_stream_wait
{
   net_rtmp_client_view_t network;
   bool encoder_retry;
} record_stream_wait_t;

RETRO_BEGIN_DECLS
/* One recording I/O owner. Borrows an H.264/HEVC worker and a READY client until
 * free; this consumer alone drives the client meanwhile. AAC-LC is optional,
 * 44.1/48 kHz mono/stereo in center/stereo speaker-mask order. PCM starts at
 * zero; video uses config.video.timescale. No file is created. */
record_stream_d3d12_t *record_stream_d3d12_new(record_gpu_worker_d3d12_t *worker,
      const record_gpu_output_config_t *config, net_rtmp_client_t *client);
/* One PCM producer: whole batch or BUSY, no allocation/wait. The native AAC
 * consumer reads the ring directly into its writable input loan. */
int record_stream_d3d12_audio(record_stream_d3d12_t *stream,
      const int16_t *samples, size_t frames);
bool record_stream_d3d12_audio_finish(record_stream_d3d12_t *stream);
/* A bounded network pass plus one mux/producer service pass.
 * On BUSY, schedule network interests, the borrowed GPU worker's completion,
 * producer notification and cancellation. encoder_retry needs a scheduled SDK
 * retry if no completion exists. Never spin on BUSY. END includes client finish.
 * After ERROR/abort, keep stepping and close/quiesce the PCM producer before
 * free; the caller still owns GPU worker/encoder error drain. */
int record_stream_d3d12_step(record_stream_d3d12_t *stream,
      uint32_t receive_time, record_stream_wait_t *wait);
/* Cancellation request only; the I/O owner retires resources in step. */
void record_stream_d3d12_abort(record_stream_d3d12_t *stream);
enum record_gpu_output_error record_stream_d3d12_error(const record_stream_d3d12_t *stream);
bool record_stream_d3d12_free(record_stream_d3d12_t *stream);
RETRO_END_DECLS
#endif

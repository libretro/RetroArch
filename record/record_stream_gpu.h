/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_STREAM_GPU_H
#define RECORD_STREAM_GPU_H
#include "record_gpu_output_config.h"
#include <net/net_rtmp_client.h>

typedef struct record_stream_gpu record_stream_gpu_t;
typedef struct record_stream_wait
{
   net_rtmp_client_view_t network;
   bool encoder_retry;
} record_stream_wait_t;

enum record_stream_worker_result
{
   RECORD_STREAM_WORKER_ERROR = -1, RECORD_STREAM_WORKER_BUSY,
   RECORD_STREAM_WORKER_PROGRESS, RECORD_STREAM_WORKER_PACKET, RECORD_STREAM_WORKER_END
};
typedef struct record_stream_packet
{
   const void *data;
   size_t size;
   uint64_t timestamp, duration;
   bool keyframe;
} record_stream_packet_t;
/* Worker-only dispatch. PACKET bytes remain borrowed until release succeeds.
 * step must return a record_stream_worker_result. No GPU API or pacing here. */
typedef struct record_stream_worker_ops
{
   int (*step)(void *worker, record_stream_packet_t *packet);
   bool (*release)(void *worker);
   bool (*retry_needed)(const void *worker);
} record_stream_worker_ops_t;

RETRO_BEGIN_DECLS
/* One recording I/O owner. Borrows an H.264/HEVC/AV1 worker and a READY client until
 * free; this consumer alone drives the client meanwhile.
 * Copies the immutable worker dispatch; never retains the caller's ops pointer.
 * AAC-LC is optional,
 * 44.1/48 kHz mono/stereo in center/stereo speaker-mask order. PCM starts at
 * zero; video uses config.video.timescale. No file is created. */
record_stream_gpu_t *record_stream_gpu_new(void *worker,
      const record_stream_worker_ops_t *ops,
      const record_gpu_output_config_t *config, net_rtmp_client_t *client);
/* One PCM producer: whole batch or BUSY, no allocation/wait. The native AAC
 * consumer reads the ring directly into its writable input loan. */
int record_stream_gpu_audio(record_stream_gpu_t *stream,
      const int16_t *samples, size_t frames);
bool record_stream_gpu_audio_finish(record_stream_gpu_t *stream);
/* A bounded network pass plus one mux/producer service pass.
 * On BUSY, schedule network interests, the borrowed GPU worker's completion,
 * producer notification and cancellation. encoder_retry needs a scheduled SDK
 * retry if no completion exists. Never spin on BUSY. END includes client finish.
 * After ERROR/abort, keep stepping and close/quiesce the PCM producer before
 * free; the caller still owns GPU worker/encoder error drain. */
int record_stream_gpu_step(record_stream_gpu_t *stream,
      uint32_t receive_time, record_stream_wait_t *wait);
/* Cancellation request only; the I/O owner retires resources in step. */
void record_stream_gpu_abort(record_stream_gpu_t *stream);
enum record_gpu_output_error record_stream_gpu_error(const record_stream_gpu_t *stream);
bool record_stream_gpu_free(record_stream_gpu_t *stream);
/* I/O owner only, before free: submitted source PCM frames, excluding AAC padding. */
uint64_t record_stream_gpu_audio_frames(const record_stream_gpu_t *stream);
RETRO_END_DECLS
#endif

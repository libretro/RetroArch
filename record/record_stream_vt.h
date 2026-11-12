/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_STREAM_VT_H
#define RECORD_STREAM_VT_H
#include "record_gpu_worker_vt.h"
#include "record_at_audio.h"
#include <net/net_rtmp_client.h>
#include <formats/video_color.h>
RETRO_BEGIN_DECLS
typedef struct record_stream_vt record_stream_vt_t;
typedef struct record_stream_vt_config
{
   uint64_t video_epoch_ticks;
   void *format;
   size_t max_packet_size, audio_fifo_frames;
   video_color_metadata_t color;
   unsigned video_timescale, bit_depth, audio_rate, channels, audio_epoch_frames;
} record_stream_vt_config_t;
typedef struct record_stream_vt_wait
{
   net_rtmp_client_view_t network;
   bool encoder_retry;
} record_stream_vt_wait_t;
enum record_stream_vt_result
{
   RECORD_STREAM_VT_ERROR=-1, RECORD_STREAM_VT_BUSY,
   RECORD_STREAM_VT_PROGRESS, RECORD_STREAM_VT_END
};
enum record_stream_vt_error
{
   RECORD_STREAM_VT_ERROR_NONE, RECORD_STREAM_VT_ERROR_VIDEO,
   RECORD_STREAM_VT_ERROR_AUDIO, RECORD_STREAM_VT_ERROR_MUX,
   RECORD_STREAM_VT_ERROR_ABORTED, RECORD_STREAM_VT_ERROR_NETWORK
};
/* I/O worker owns every service call. Borrows the native format, video worker,
 * optional fresh AAC encoder and READY connection until free. The caller owns
 * encoder teardown after network/output borrows retire, including cancellation.
 * Explicit output epochs must match within one video tick; audio epoch covers
 * native leading priming. PCM starts at source index zero. No hidden rebasing. */
record_stream_vt_t *record_stream_vt_new(record_gpu_worker_vt_t *video,
      record_at_audio_t *audio, net_rtmp_client_t *client,
      const record_stream_vt_config_t *config);
/* One PCM producer; whole batch or BUSY, no allocation or waits. */
int record_stream_vt_audio(record_stream_vt_t *stream,
      const int16_t *samples, size_t frames);
bool record_stream_vt_audio_finish(record_stream_vt_t *stream);
/* Bounded network plus mux/producer step. Schedule socket readiness, native
 * completion, producer notification, cancellation and requested encoder retry.
 * After ERROR/abort, keep stepping until borrows retire, close the producer,
 * then free. A native release failure hands remaining encoder/worker leases
 * back to their caller after network retirement; the error stays visible.
 * Caller cancels/drains borrowed encoders and abandons failed GPU leases. */
int record_stream_vt_step(record_stream_vt_t *stream, uint32_t receive_time,
      record_stream_vt_wait_t *wait);
void record_stream_vt_abort(record_stream_vt_t *stream);
enum record_stream_vt_error record_stream_vt_error(const record_stream_vt_t *stream);
bool record_stream_vt_free(record_stream_vt_t *stream);
RETRO_END_DECLS
#endif

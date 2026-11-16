/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_GPU_SESSION_VULKAN_H
#define RECORD_GPU_SESSION_VULKAN_H
#include "record_gpu_output_vulkan.h"
RETRO_BEGIN_DECLS

typedef struct record_gpu_session_vulkan record_gpu_session_vulkan_t;
enum record_gpu_session_vulkan_error
{
   RECORD_VULKAN_SESSION_NONE, RECORD_VULKAN_SESSION_STARTUP,
   RECORD_VULKAN_SESSION_CANCELLED, RECORD_VULKAN_SESSION_OUTPUT
};

/* One I/O owner constructs/services/retires the session. Borrows device and
 * encoder dispatch until retirement. Owns encoder, queue, pump and output.
 * Startup/file I/O may block that owner; never call from a task/frame path.
 * Output headers, colour and timescale must be empty: actual encoder output
 * supplies them. An optional READY output->stream_client stays borrowed
 * through session retirement; never drive it concurrently with session step.
 * The caller frees it afterwards, canceling first on failed startup/abort.
 * A non-NULL session may report startup failure; close both
 * producers and service it to terminal ERROR before free. No scheduler here. */
record_gpu_session_vulkan_t *record_gpu_session_vulkan_new(
      const vulkan_encode_h264_open_t *encoder,
      const vulkan_encode_h264_ops_t *ops,
      const record_gpu_output_vulkan_config_t *output, const char *path);
/* Immutable producer borrows, valid only after startup publication and before
 * video_finish. Submit descriptors in fps_num units, duration fps_den. Only
 * use the queue while error is NONE and cancellation has not been requested.
 * Cancel any outstanding reservation before closing the video producer. */
const vulkan_encode_pool_t *record_gpu_session_vulkan_pool(
      const record_gpu_session_vulkan_t *session);
record_gpu_queue_t *record_gpu_session_vulkan_queue(
      const record_gpu_session_vulkan_t *session);
/* One PCM producer; whole-batch backpressure, no allocation/waits. */
int record_gpu_session_vulkan_audio(record_gpu_session_vulkan_t *session,
      const int16_t *samples, size_t frames);
/* Each producer closes itself after its last call returns. video_finish also
 * promises ALL producer GPU commands using the pool have retired, including
 * unsubmitted/cancelled captures. Poll their native completion; never wait on
 * the render thread. Disabled audio starts closed. */
bool record_gpu_session_vulkan_video_finish(record_gpu_session_vulkan_t *session);
bool record_gpu_session_vulkan_audio_finish(record_gpu_session_vulkan_t *session);
/* Controller request, does not close either producer. */
void record_gpu_session_vulkan_cancel(record_gpu_session_vulkan_t *session);
/* Bounded I/O-owner service, RECORD_VULKAN_OUTPUT_* results. END/ERROR require
 * retired resources; sticky error alone is not terminal. Failed native GPU
 * retirement can require later retries. Wake on input/close/cancel; after BUSY
 * retry_needed distinguishes native completion work from awaiting a producer. */
int record_gpu_session_vulkan_step(record_gpu_session_vulkan_t *session);
bool record_gpu_session_vulkan_retry_needed(const record_gpu_session_vulkan_t *session);
/* Atomic snapshot for controller/producers; details optional. */
enum record_gpu_session_vulkan_error record_gpu_session_vulkan_error(
      const record_gpu_session_vulkan_t *session, int *file_error);
/* All callers quiescent. Refuses until session retirement. */
/* Worker-only snapshot after terminal step, before free. */
uint64_t record_gpu_session_vulkan_audio_frames(const record_gpu_session_vulkan_t *session);
bool record_gpu_session_vulkan_free(record_gpu_session_vulkan_t *session);
/* I/O owner only, after step; readiness never services the transport. */
bool record_gpu_session_vulkan_network(const record_gpu_session_vulkan_t *session,
      int *fd, bool *read, bool *write);
RETRO_END_DECLS
#endif

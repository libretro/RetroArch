/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_GPU_DISPATCH_VULKAN_H
#define RECORD_GPU_DISPATCH_VULKAN_H
#include "record_gpu_session_vulkan.h"
RETRO_BEGIN_DECLS
typedef struct record_gpu_dispatch_vulkan record_gpu_dispatch_vulkan_t;
enum record_gpu_dispatch_vulkan_state
{
   RECORD_VKD_STARTING, RECORD_VKD_READY, RECORD_VKD_DRAINING,
   RECORD_VKD_DONE, RECORD_VKD_FAILED
};
/* Cold controller. Copies configuration, dispatch and path; opens the session
 * on a dedicated I/O worker. Borrows device/queues through successful free.
 * The encode queue must have no other host submitter; producer queue/image
 * ownership remains with the renderer. Requires a lock-free wake backend;
 * unsupported/threadless builds refuse. */
record_gpu_dispatch_vulkan_t *record_gpu_dispatch_vulkan_new(
      const vulkan_encode_h264_open_t *encoder,
      const vulkan_encode_h264_ops_t *ops,
      const record_gpu_output_vulkan_config_t *output, const char *path);
/* Render producer. READY only; immutable pool borrow until video_finish.
 * Capture into an acquired slot, submit its producer commands, then publish
 * the descriptor. BUSY takes nothing; failed submit retains the reservation.
 * No allocation, pixel copies, waits or logging. */
const vulkan_encode_pool_t *record_gpu_dispatch_vulkan_pool(
      const record_gpu_dispatch_vulkan_t *job);
int record_gpu_dispatch_vulkan_acquire(record_gpu_dispatch_vulkan_t *job, unsigned *slot);
bool record_gpu_dispatch_vulkan_submit(record_gpu_dispatch_vulkan_t *job,
      const record_gpu_frame_t *frame);
bool record_gpu_dispatch_vulkan_cancel_slot(record_gpu_dispatch_vulkan_t *job);
/* One PCM producer, existing whole-batch output contract. */
int record_gpu_dispatch_vulkan_audio(record_gpu_dispatch_vulkan_t *job,
      const int16_t *samples, size_t frames);
/* Each producer closes after its last call returns. video_finish promises
 * all producer GPU commands referencing the pool have retired, even captures
 * not published to the worker. Cancel a reservation before closing. */
bool record_gpu_dispatch_vulkan_video_finish(record_gpu_dispatch_vulkan_t *job);
bool record_gpu_dispatch_vulkan_audio_finish(record_gpu_dispatch_vulkan_t *job);
/* Controller request only, never closes another producer. */
void record_gpu_dispatch_vulkan_cancel(record_gpu_dispatch_vulkan_t *job);
enum record_gpu_dispatch_vulkan_state record_gpu_dispatch_vulkan_status(
      const record_gpu_dispatch_vulkan_t *job,
      enum record_gpu_session_vulkan_error *error, int *file_error);
/* Cold controller, all producer/status calls quiescent. Nonblocking; refuses
 * active resources or worker. Retry later; no join or wait-for-idle. */
bool record_gpu_dispatch_vulkan_free(record_gpu_dispatch_vulkan_t *job);
RETRO_END_DECLS
#endif

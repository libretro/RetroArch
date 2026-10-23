/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_ANDROID_CAPTURE_PIPE_H
#define RECORD_ANDROID_CAPTURE_PIPE_H
#include "record_android_capture_submit.h"
#include "record_gpu_dispatch_mediacodec.h"
RETRO_BEGIN_DECLS
typedef struct record_android_capture_pipe record_android_capture_pipe_t;
/* Cold renderer setup. Borrows distinct entries and the GPU executor until
 * free succeeds. Entry order must match the executor's fixed buffer slots. */
record_android_capture_pipe_t *record_android_capture_pipe_new(
      record_gpu_dispatch_mediacodec_t *gpu,
      record_android_capture_submit_t *const *entries, unsigned count,
      void (*close_fd)(void *, int), void *context);
/* Renderer, outside render passes. 1 accepts source lease, 0 BUSY takes
 * nothing, -1 takes nothing, -2 failed submission retains the source lease.
 * One pending frame is serviced by step; never resubmit an accepted source. */
int record_android_capture_pipe_push(record_android_capture_pipe_t *pipe,
      const record_android_capture_source_t *source, uint64_t pts,
      uint64_t duration);
/* Same source/time contract, recording into a borrowed renderer command.
 * Before queue acceptance only abort with never-execute proof is allowed.
 * After acceptance call commit once, including when cancellation was requested.
 * step never publishes an uncommitted frame. Aborted times remain consumed. */
int record_android_capture_pipe_record(record_android_capture_pipe_t *pipe,
      const record_android_capture_source_t *source, uint64_t pts,
      uint64_t duration, VkCommandBuffer command);
/* 0 retains busy completion; step retries completion without copying again. */
int record_android_capture_pipe_commit(record_android_capture_pipe_t *pipe);
bool record_android_capture_pipe_abort_recorded(
      record_android_capture_pipe_t *pipe);
/* Renderer service. At most one copy-fence query; no CPU waits. 1 terminal
 * GPU/copy ownership retired, 0 pending, -1 unproven retirement. */
int record_android_capture_pipe_step(record_android_capture_pipe_t *pipe);
/* Producer closure after final push. BUSY while a pending source is handed
 * off; retry with the same end after servicing step. Cancellation drops
 * unpublished descriptors but still requires explicit producer closure. */
bool record_android_capture_pipe_finish(
      record_android_capture_pipe_t *pipe, int64_t end_us);
void record_android_capture_pipe_cancel(record_android_capture_pipe_t *pipe);
/* Renderer, after source production stops. Cached proof for original source
 * leases only; capture buffers and the device still require joint retirement.
 */
bool record_android_capture_pipe_sources_retired(
      const record_android_capture_pipe_t *pipe);
/* Renderer quiescent. Does not free borrowed executor, entries or pool. */
bool record_android_capture_pipe_free(record_android_capture_pipe_t *pipe);
RETRO_END_DECLS
#endif

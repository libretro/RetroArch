/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_MEDIACODEC_GPU_WORKER_H
#define RECORD_MEDIACODEC_GPU_WORKER_H
#include "record_gpu_queue.h"
#include "record_mediacodec_surface.h"
#include "record_android_capture_gl.h"
#include "record_android_capture_draw.h"
RETRO_BEGIN_DECLS
typedef struct record_mediacodec_gpu_worker record_mediacodec_gpu_worker_t;
/* One GLES worker borrows matching fixed resources. Codec output is serviced
 * independently by the I/O worker. Queue has count slots
 * (1..16). Surface, imports and draw owner use the same dedicated context.
 * Frame timestamps are source microseconds; ready_value encodes owned SYNC_FD
 * as fd+1 (zero means already-ready -1). Keyframe requests are unsupported.
 * Queue submission transfers fd ownership; caller closes it on refusal. */
record_mediacodec_gpu_worker_t *record_mediacodec_gpu_worker_new(
      record_gpu_queue_t *queue, record_mediacodec_surface_t *surface,
      record_android_capture_gl_t *capture, record_android_capture_draw_t *draw,
      unsigned count, void (*close_fd)(void *context, int fd), void *context);
/* One zero-timeout retirement query and at most one queued frame/post.
 * -1 error, 0 busy, 1 progress, 2 input closed AND all source reads retired.
 * Only after 2 may the controller signal encoder input EOS. Each successful
 * draw is fenced before posting. Never retry a failed post. No application
 * wait, allocation or CPU pixel readback occurs. */
int record_mediacodec_gpu_worker_step(record_mediacodec_gpu_worker_t *worker);
/* Terminal cancellation. Producer closes the queue; one queued fd is closed
 * and one GPU read queried per call. Same results; -1 retains unproven reads.
 * Can be called after any error; no further draws/posts occur. */
int record_mediacodec_gpu_worker_retire_step(
      record_mediacodec_gpu_worker_t *worker);
/* ONLY after matching context/owned GL namespace destruction and independent
 * GPU-read completion proof or device loss. Clears GL bookkeeping and returns
 * pending slots without GL calls. Queued descriptors must still drain through
 * retire_step. */
void record_mediacodec_gpu_worker_after_context_retired(
      record_mediacodec_gpu_worker_t *worker);
/* Borrowed components remain caller-owned. Refuses active/undrained service. */
bool record_mediacodec_gpu_worker_free(record_mediacodec_gpu_worker_t *worker);
RETRO_END_DECLS
#endif

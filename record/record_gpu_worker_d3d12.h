/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_GPU_WORKER_D3D12_H
#define RECORD_GPU_WORKER_D3D12_H
#include "record_gpu_queue.h"
#include "../gfx/common/nvenc_d3d12.h"
RETRO_BEGIN_DECLS

enum record_gpu_worker_result
{
   RECORD_GPU_WORKER_ERROR = -1,
   RECORD_GPU_WORKER_BUSY,
   RECORD_GPU_WORKER_PROGRESS,
   RECORD_GPU_WORKER_PACKET,
   RECORD_GPU_WORKER_END
};
typedef struct record_gpu_worker_d3d12 record_gpu_worker_d3d12_t;

/* One worker owns this pump and the fresh encoder. Queue, encoder and fence
 * are borrowed until free. The encoder pool must match the queue's slots.
 * Open/close the encoder on that worker, outside the display frame path. */
record_gpu_worker_d3d12_t *record_gpu_worker_d3d12_new(
      record_gpu_queue_t *queue, nvenc_d3d12_t *encoder, void *ready_fence);
/* One bounded service step, no retries/waits. PACKET repeats the same borrowed
 * bytes until release succeeds; BUSY consumes no pending input. Errors retain
 * ownership for retry or teardown. Producer close submits EOS after the last
 * descriptor, END requires all packets released and EOS completed. */
int record_gpu_worker_d3d12_step(record_gpu_worker_d3d12_t *worker,
      nvenc_d3d12_packet_t *packet);
/* Worker-only borrowed Win32 completion handle for the oldest accepted frame
 * or EOS, or NULL. Never close/reset it; re-query after each service step.
 * This is a scheduling hint, not a promise of progress: also wake on producer
 * input/close and schedule retries for SDK BUSY without pending completion. */
void *record_gpu_worker_d3d12_wait_handle(const record_gpu_worker_d3d12_t *w);
/* Worker-only: outstanding work needs completion or another service attempt.
 * After BUSY, use the completion handle if available; otherwise schedule an
 * SDK retry when true, or await producer input/close when false. A signaled
 * handle followed by BUSY still needs a delayed retry, not a tight loop. */
bool record_gpu_worker_d3d12_retry_needed(const record_gpu_worker_d3d12_t *w);
bool record_gpu_worker_d3d12_release(record_gpu_worker_d3d12_t *worker);
/* Normal teardown after END, or before the first input was taken. */
bool record_gpu_worker_d3d12_free(record_gpu_worker_d3d12_t *worker);
/* Error teardown only AFTER nvenc_d3d12_free succeeded and the producer stopped.
 * Returns queue leases, including unsubmitted input; refuses an open queue.
 * Capture/source GPU work must still retire through the capture owner's fence. */
bool record_gpu_worker_d3d12_abandon(record_gpu_worker_d3d12_t *worker);
RETRO_END_DECLS
#endif

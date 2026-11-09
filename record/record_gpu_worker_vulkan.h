/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_GPU_WORKER_VULKAN_H
#define RECORD_GPU_WORKER_VULKAN_H
#include "record_gpu_queue.h"
#include "../gfx/common/vulkan_encode_h264.h"
RETRO_BEGIN_DECLS

enum record_gpu_worker_vulkan_result
{
   RECORD_GPU_VULKAN_ERROR = -1,
   RECORD_GPU_VULKAN_BUSY,
   RECORD_GPU_VULKAN_PROGRESS,
   RECORD_GPU_VULKAN_PACKET,
   RECORD_GPU_VULKAN_END
};
typedef struct record_gpu_worker_vulkan record_gpu_worker_vulkan_t;

/* One worker owns this pump and the fresh encoder. Queue and encoder
 * are borrowed until free. The encoder pool must match the queue's slots.
 * Open/close the encoder on that worker, outside the display frame path. */
record_gpu_worker_vulkan_t *record_gpu_worker_vulkan_new(
      record_gpu_queue_t *queue, vulkan_encode_h264_t *encoder);
/* One bounded service step, no retries/waits. PACKET repeats the same borrowed
 * bytes until release succeeds; BUSY consumes no pending input. Errors stop admission and retain
 * ownership for teardown. Producer close submits EOS after the last
 * descriptor, END requires all packets released and EOS completed. */
int record_gpu_worker_vulkan_step(record_gpu_worker_vulkan_t *worker,
      vulkan_encode_packet_t *packet);
/* Worker-only: outstanding work needs a completion retry. After BUSY,
 * schedule a delayed retry when true; otherwise await producer input/close. */
bool record_gpu_worker_vulkan_retry_needed(const record_gpu_worker_vulkan_t *w);
bool record_gpu_worker_vulkan_release(record_gpu_worker_vulkan_t *worker);
/* Normal teardown after END, or before the first input was taken. */
bool record_gpu_worker_vulkan_free(record_gpu_worker_vulkan_t *worker);
/* Error teardown after producer closure/GPU retirement and successful encoder
 * free. Returns queue leases, including unsubmitted input; refuses an open
 * queue. Packet readers must retire before destroying the encoder. */
bool record_gpu_worker_vulkan_abandon(record_gpu_worker_vulkan_t *worker);
RETRO_END_DECLS
#endif

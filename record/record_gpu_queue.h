/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_GPU_QUEUE_H
#define RECORD_GPU_QUEUE_H
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS

#define RECORD_GPU_QUEUE_SLOTS 16

enum record_gpu_queue_result
{
   RECORD_GPU_QUEUE_ERROR = -1,
   RECORD_GPU_QUEUE_BUSY,
   RECORD_GPU_QUEUE_OK,
   RECORD_GPU_QUEUE_END
};

typedef struct record_gpu_queue record_gpu_queue_t;
typedef struct record_gpu_frame
{
   uint64_t timestamp, duration, ready_value;
   unsigned slot;
   bool keyframe;
} record_gpu_frame_t;

/* One producer and one worker; no waits or pixel copies. The caller retains
 * the fixed texture pool and ready fence until both sides stop and free
 * succeeds. Construct before handing the queue to either thread. */
record_gpu_queue_t *record_gpu_queue_new(unsigned slots);
/* Producer: one reservation at a time. BUSY takes nothing. Submit transfers
 * the slot to the worker; failure retains the reservation. Cancellation
 * keeps it available to this producer. */
int record_gpu_queue_acquire(record_gpu_queue_t *queue, unsigned *slot);
bool record_gpu_queue_submit(record_gpu_queue_t *queue,
      const record_gpu_frame_t *frame);
bool record_gpu_queue_cancel(record_gpu_queue_t *queue);
/* Producer: refuses an outstanding reservation; otherwise idempotent. */
bool record_gpu_queue_finish(record_gpu_queue_t *queue);
/* Worker: END means no more input, including while GPU work is outstanding.
 * A taken descriptor remains the worker's across encoder BUSY. Release only
 * after the GPU and packet no longer reference the slot. */
int record_gpu_queue_take(record_gpu_queue_t *queue, record_gpu_frame_t *frame);
bool record_gpu_queue_release(record_gpu_queue_t *queue, unsigned slot);
/* Both sides quiesced. Refuses reserved, queued or worker-owned slots. */
bool record_gpu_queue_free(record_gpu_queue_t *queue);
RETRO_END_DECLS
#endif

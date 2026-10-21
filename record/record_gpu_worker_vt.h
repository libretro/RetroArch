/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_GPU_WORKER_VT_H
#define RECORD_GPU_WORKER_VT_H
#include "record_gpu_queue.h"
RETRO_BEGIN_DECLS
enum record_gpu_worker_vt_result
{
   RECORD_GPU_VT_ERROR=-1, RECORD_GPU_VT_BUSY, RECORD_GPU_VT_PROGRESS,
   RECORD_GPU_VT_PACKET, RECORD_GPU_VT_END
};
typedef struct record_gpu_worker_vt record_gpu_worker_vt_t;
typedef struct record_gpu_worker_vt_ops
{
   /* Capture ready: 1 completed, 0 pending, -1 GPU error. */
   int (*ready)(void *capture,unsigned slot);
   void *(*buffer)(void *capture,unsigned slot);
   bool (*release)(void *capture,unsigned slot);
   /* Encoder operations are worker-only. Submit/peek/finish: 1 success,
    * 0 busy, -1 failed. Peek borrows one non-null CMSampleBuffer until ack. */
   int (*submit)(void *encoder,unsigned slot,void *buffer,uint64_t pts,uint64_t duration);
   int (*peek)(void *encoder,unsigned *slot,void **sample);
   bool (*ack)(void *encoder);
   int (*finish)(void *encoder);
} record_gpu_worker_vt_ops_t;
/* One worker; queue/capture/encoder borrowed and their fixed pools match.
 * Driver supplies native adapter operations; tests use fault-injected APIs.
 * No frame-path calls. The pump itself does not allocate, wait or copy pixels;
 * native adapters may create bounded worker-side metadata objects. */
record_gpu_worker_vt_t *record_gpu_worker_vt_new(record_gpu_queue_t *queue,
      void *capture,void *encoder,const record_gpu_worker_vt_ops_t *ops);
/* Adopts owned_context only on success. destroy runs exactly once on normal
 * free or successful abandon, after encoder teardown/GPU retirement. It must
 * not access an encoder that the caller has already destroyed. */
record_gpu_worker_vt_t *record_gpu_worker_vt_new_owned(record_gpu_queue_t *queue,
      void *capture,void *encoder,const record_gpu_worker_vt_ops_t *ops,
      void *owned_context,void (*destroy)(void*));
/* One bounded step. PACKET repeats the borrowed sample until release. */
int record_gpu_worker_vt_step(record_gpu_worker_vt_t *worker,void **sample);
/* A partial release can be retried, but invalidates the old sample borrow. */
bool record_gpu_worker_vt_release(record_gpu_worker_vt_t *worker);
bool record_gpu_worker_vt_free(record_gpu_worker_vt_t *worker);
/* Worker-only after BUSY: native/GPU progress needs a scheduled retry. */
bool record_gpu_worker_vt_retry_needed(const record_gpu_worker_vt_t *worker);
/* Error teardown only after producer closure and native encoder teardown.
 * Polls outstanding GPU work without waiting; false retains the worker.
 * No encoder operation is invoked once abandon begins. */
bool record_gpu_worker_vt_abandon(record_gpu_worker_vt_t *worker);
RETRO_END_DECLS
#endif

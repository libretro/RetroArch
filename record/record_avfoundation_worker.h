/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_AVFOUNDATION_WORKER_H
#define RECORD_AVFOUNDATION_WORKER_H
#include "record_gpu_worker_vt.h"
#include "record_audio_queue.h"
RETRO_BEGIN_DECLS
typedef struct record_avfoundation_worker record_avfoundation_worker_t;
typedef struct record_avfoundation_worker_ops
{
   /* RECORD_AVF_ERROR/BUSY/OK. BUSY consumes nothing. Accepted samples must
    * retain/copy the required data before returning; no asynchronous borrow. */
   int (*video)(void *mux,void *sample);
   int (*audio)(void *mux,const int16_t *samples,size_t frames,uint64_t timestamp);
   int (*finish)(void *mux);
} record_avfoundation_worker_ops_t;
enum record_avfoundation_worker_result
{ RECORD_AVF_WORKER_ERROR=-1, RECORD_AVF_WORKER_BUSY, RECORD_AVF_WORKER_PROGRESS, RECORD_AVF_WORKER_END };
/* Cold allocation. Borrows all components and copies ops. One dedicated
 * worker owns step. NULL audio means video-only. Producer closure remains
 * with each producer; this pump never closes another thread's input. */
record_avfoundation_worker_t *record_avfoundation_worker_new(
      record_gpu_worker_vt_t *video,record_audio_queue_t *audio,void *mux,
      const record_avfoundation_worker_ops_t *ops);
/* Bounded service: one video pump call, one PCM packet, one finalization poll.
 * Retains borrows across BUSY, services audio while video is stalled, and
 * finalizes only after video and audio report drained END. Errors are sticky.
 * No allocation, waits, logging, or hidden frame dropping occurs here. */
int record_avfoundation_worker_step(record_avfoundation_worker_t *worker);
/* Worker quiescent. Refuses an active pump. On failure the controller still
 * owns mux cancellation, GPU/encoder retirement and PCM drain; freeing this
 * coordinator does not release any borrowed component or in-flight callback. */
bool record_avfoundation_worker_free(record_avfoundation_worker_t *worker);
/* Worker-only after stopping service. Marks the coordinator failed so its
 * bookkeeping can be freed; borrowed components still require retirement. */
void record_avfoundation_worker_abort(record_avfoundation_worker_t *worker);
/* Worker-only after BUSY; input/closure notifications are still required. */
bool record_avfoundation_worker_retry_needed(const record_avfoundation_worker_t *worker);
RETRO_END_DECLS
#endif

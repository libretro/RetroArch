/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_GPU_DISPATCH_METAL_H
#define RECORD_GPU_DISPATCH_METAL_H
#include "record_gpu_session_metal.h"
RETRO_BEGIN_DECLS
typedef struct record_gpu_dispatch_metal record_gpu_dispatch_metal_t;
enum record_gpu_dispatch_metal_state
{
   RECORD_METAL_DISPATCH_STARTING, RECORD_METAL_DISPATCH_READY,
   RECORD_METAL_DISPATCH_DRAINING, RECORD_METAL_DISPATCH_DONE, RECORD_METAL_DISPATCH_FAILED
};
/* Cold controller only. Copies path/config, retains device and starts a
 * dedicated I/O worker. Startup creates all native resources on that worker.
 * macOS 10.14+ with threads; unsupported builds return NULL. A returned job
 * may subsequently report startup failure; both producers must still close. */
record_gpu_dispatch_metal_t *record_gpu_dispatch_metal_new(void *device,
      const record_gpu_session_metal_config_t *config,
      const record_mux_avfoundation_config_t *output,size_t audio_capacity_bytes,const char *path);
/* One render producer. Uses RECORD_GPU_QUEUE_* results. STARTING is BUSY and
 * consumes nothing. Accepted work retains source references via the supplied
 * uncommitted command buffer; caller always commits it as part of rendering.
 * No active encoder may remain on that buffer. No waits, input CPU readback,
 * pixel allocation or recording I/O. ERROR stops recording, not rendering. */
int record_gpu_dispatch_metal_video(record_gpu_dispatch_metal_t *job,
      void *commands,void *source,unsigned x,unsigned y,uint64_t timestamp,uint64_t duration);
/* One PCM producer. Whole-packet RECORD_AUDIO_* contract, explicit source
 * sample clock. BUSY retains caller input and advances no clock. */
int record_gpu_dispatch_metal_audio(record_gpu_dispatch_metal_t *job,
      const int16_t *samples,size_t frames,uint64_t timestamp);
/* Each producer closes after its last call returns, including startup failure
 * and cancellation. Cold controller may close only after proving quiescence.
 * A failed publication may require later nonblocking video_finish retries. */
bool record_gpu_dispatch_metal_video_finish(record_gpu_dispatch_metal_t *job);
bool record_gpu_dispatch_metal_audio_finish(record_gpu_dispatch_metal_t *job);
void record_gpu_dispatch_metal_cancel(record_gpu_dispatch_metal_t *job);
/* Atomic observations. DONE/FAILED follow complete native retirement. */
enum record_gpu_dispatch_metal_state record_gpu_dispatch_metal_status(
      const record_gpu_dispatch_metal_t *job,enum record_gpu_session_metal_startup *startup,
      int32_t *native_status,enum record_avfoundation_session_error *error,int *system_error);
/* Cold controller, all producers quiesced. Refuses while worker/resources are
 * active. Normally detached; a rare detach failure joins only after the
 * worker publishes its final retirement. Never call from render/audio paths. */
bool record_gpu_dispatch_metal_free(record_gpu_dispatch_metal_t *job);
RETRO_END_DECLS
#endif

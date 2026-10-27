/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_GPU_DISPATCH_MEDIACODEC_H
#define RECORD_GPU_DISPATCH_MEDIACODEC_H
#include "record_dispatch_mediacodec.h"
#include "record_mediacodec_gpu_worker.h"
RETRO_BEGIN_DECLS
typedef struct record_gpu_dispatch_mediacodec record_gpu_dispatch_mediacodec_t;
typedef struct record_gpu_mediacodec_config
{
   record_mediacodec_surface_ops_t surface;
   record_android_capture_gl_ops_t capture;
   record_android_capture_draw_ops_t draw;
   struct AHardwareBuffer *buffers[RECORD_GPU_QUEUE_SLOTS];
   EGLDisplay display;
   EGLConfig config;
   EGLContext share;
   unsigned width, height, count, mode;
   bool flip_y;
} record_gpu_mediacodec_config_t;
/* Cold controller: creates a separate GLES thread and fixed queue. Borrows
 * I/O job, display, share group and buffers until collection. Request
 * dimensions and SDR/PQ metadata must match I/O configuration. Buffers come
 * from a validated fixed pool: exact dimensions and RGB8/PQ RGB10/scRGB FP16
 * for modes 0/1/2. No ownership on NULL. */
record_gpu_dispatch_mediacodec_t *record_gpu_dispatch_mediacodec_new(
      record_dispatch_mediacodec_t *io,
      const record_gpu_mediacodec_config_t *config);
/* One render producer. READY only; BUSY takes nothing. Submit transfers fd+1
 * encoded in ready_value only on success. Positive duration, increasing source
 * microseconds and no keyframe request. Failed submit retains reservation/fd.
 */
int record_gpu_dispatch_mediacodec_acquire(
      record_gpu_dispatch_mediacodec_t *job, unsigned *slot);
bool record_gpu_dispatch_mediacodec_submit(
      record_gpu_dispatch_mediacodec_t *job, const record_gpu_frame_t *frame);
bool record_gpu_dispatch_mediacodec_cancel_slot(
      record_gpu_dispatch_mediacodec_t *job);
/* Same producer, after last call returns, even on startup failure/cancel.
 * Refuses a reservation or an end before the last submitted picture end. */
bool record_gpu_dispatch_mediacodec_finish(
      record_gpu_dispatch_mediacodec_t *job, int64_t end_us);
void record_gpu_dispatch_mediacodec_cancel(
      record_gpu_dispatch_mediacodec_t *job);
/* Same state values as I/O dispatch. DONE/FAILED retire only GLES users; the
 * renderer must independently retire Vulkan copy/source leases. Errors with
 * unproven reads/native destruction failures retain DRAINING ownership. */
enum record_dispatch_mediacodec_state record_gpu_dispatch_mediacodec_status(
      const record_gpu_dispatch_mediacodec_t *job, int *error,
      int *system_error);
/* Cold controller, producer quiescent; no active-thread join. I/O job and
 * shared native resources must remain alive through successful collection. */
bool record_gpu_dispatch_mediacodec_free(record_gpu_dispatch_mediacodec_t *job);
RETRO_END_DECLS
#endif

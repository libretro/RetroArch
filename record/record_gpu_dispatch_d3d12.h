/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_GPU_DISPATCH_D3D12_H
#define RECORD_GPU_DISPATCH_D3D12_H
#include "record_gpu_session_d3d12.h"
RETRO_BEGIN_DECLS
typedef struct record_gpu_dispatch_d3d12 record_gpu_dispatch_d3d12_t;
enum record_gpu_dispatch_state
{
   RECORD_GPU_DISPATCH_STARTING, RECORD_GPU_DISPATCH_READY,
   RECORD_GPU_DISPATCH_DRAINING, RECORD_GPU_DISPATCH_DONE, RECORD_GPU_DISPATCH_FAILED
};
/* Cold controller only. Copies configuration/path and starts a dedicated I/O
 * worker; encoder/file startup happens there. Borrows a fresh capture until
 * free succeeds. Capture/source GPU retirement remains with its owner.
 * Unsupported platforms and threadless builds return NULL. */
record_gpu_dispatch_d3d12_t *record_gpu_dispatch_d3d12_new(
      record_gpu_capture_d3d12_t *capture, const nvenc_d3d12_config_t *encoder,
      const record_gpu_output_config_t *output, const char *path);
/* One render producer and one PCM producer. STARTING returns BUSY, taking
 * nothing. READY preserves the capture/output whole-input contracts. No
 * allocation, waits or logging. Retain source GPU leases as for capture. */
int record_gpu_dispatch_d3d12_video(record_gpu_dispatch_d3d12_t *job,
      void *source, unsigned source_state, unsigned x, unsigned y,
      void *source_fence, uint64_t source_value, uint64_t timestamp,
      uint64_t duration, bool keyframe, uint64_t *ready_value);
int record_gpu_dispatch_d3d12_audio(record_gpu_dispatch_d3d12_t *job,
      const int16_t *samples, size_t frames);
/* Each producer closes after its last push returns, even on startup failure
 * or cancellation. Disabled audio starts closed. No pushes after closure. */
bool record_gpu_dispatch_d3d12_video_finish(record_gpu_dispatch_d3d12_t *job);
bool record_gpu_dispatch_d3d12_audio_finish(record_gpu_dispatch_d3d12_t *job);
/* Controller request; never closes either producer on another thread. */
void record_gpu_dispatch_d3d12_cancel(record_gpu_dispatch_d3d12_t *job);
/* Atomic observations, which may advance during the call. Optional details;
 * system_error reports a Win32 notification/wait failure. DONE/FAILED require
 * retired resources. Startup failure still awaits producer closure. */
enum record_gpu_dispatch_state record_gpu_dispatch_d3d12_status(
      const record_gpu_dispatch_d3d12_t *job,
      enum record_gpu_session_error *error, uint32_t *file_error, uint32_t *system_error);
/* Cold controller, all producer calls quiescent. Nonblocking collection:
 * refuses active worker/resources; no join or wait-for-idle. Retry later. */
bool record_gpu_dispatch_d3d12_free(record_gpu_dispatch_d3d12_t *job);
RETRO_END_DECLS
#endif

/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_GPU_SESSION_D3D12_H
#define RECORD_GPU_SESSION_D3D12_H
#include "record_gpu_capture_d3d12.h"
#include "record_gpu_output_d3d12.h"
RETRO_BEGIN_DECLS

typedef struct record_gpu_session_d3d12 record_gpu_session_d3d12_t;
enum record_gpu_session_error
{
   RECORD_GPU_SESSION_ERROR_NONE, RECORD_GPU_SESSION_ERROR_STARTUP,
   RECORD_GPU_SESSION_ERROR_CANCELLED, RECORD_GPU_SESSION_ERROR_OUTPUT,
   RECORD_GPU_SESSION_ERROR_VIDEO
};
/* Construct/service/free on one I/O worker. Startup and final file flush may
 * block that worker; never run this from a task handler or frame/audio path.
 * Borrows a fresh capture until retirement. Capture/source GPU retirement
 * remains the caller's responsibility. Session owns encoder, pump and output.
 * Supplies sequence headers, dimensions, codec and bit depth to the mux;
 * output.video headers/ac3_header must be empty. Non-NULL may report startup
 * failure: close both producers and service to ERROR before freeing it. */
record_gpu_session_d3d12_t *record_gpu_session_d3d12_new(
      record_gpu_capture_d3d12_t *capture, const nvenc_d3d12_config_t *encoder,
      const record_gpu_output_config_t *output, const char *path);
/* One PCM producer. Same whole-batch contract as output_audio. */
int record_gpu_session_d3d12_audio(record_gpu_session_d3d12_t *session,
      const int16_t *samples, size_t frames);
/* Each producer closes from its own thread, after its last push returns.
 * Video pushes use the borrowed capture. No more pushes after close, even
 * when cancelled. A worker may reconfirm video closure after that producer
 * has stopped and closed capture. Disabled audio starts closed.
 * No waits or allocation. */
bool record_gpu_session_d3d12_video_finish(record_gpu_session_d3d12_t *session);
bool record_gpu_session_d3d12_audio_finish(record_gpu_session_d3d12_t *session);
/* Controller request only; does not close either producer on its behalf. */
void record_gpu_session_d3d12_cancel(record_gpu_session_d3d12_t *session);
/* Bounded service/drain, using RECORD_GPU_OUTPUT_* results. ERROR and END are
 * terminal only after owned resources retire. A sticky error is observable
 * while draining; a persistent SDK failure may keep retirement pending.
 * After BUSY, query handle/retry as for the output pump. Wake independently
 * on producer input/close and cancellation. No dispatcher is installed here. */
int record_gpu_session_d3d12_step(record_gpu_session_d3d12_t *session);
void *record_gpu_session_d3d12_wait_handle(const record_gpu_session_d3d12_t *session);
bool record_gpu_session_d3d12_retry_needed(const record_gpu_session_d3d12_t *session);
/* Atomic snapshot, valid for controller/producers. Details optional. */
enum record_gpu_session_error record_gpu_session_d3d12_error(
      const record_gpu_session_d3d12_t *session, uint32_t *file_error);
/* Refuses until retired. All callers must be quiescent before free. */
bool record_gpu_session_d3d12_free(record_gpu_session_d3d12_t *session);
RETRO_END_DECLS
#endif

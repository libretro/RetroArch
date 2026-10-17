/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_GPU_CAPTURE_D3D12_H
#define RECORD_GPU_CAPTURE_D3D12_H
#include "record_gpu_queue.h"
#include "../gfx/common/nvenc_d3d12.h"
RETRO_BEGIN_DECLS
typedef struct record_gpu_capture_d3d12 record_gpu_capture_d3d12_t;

/* Create off the frame path. Retains the device/queue and owns a fixed GPU
 * texture/command pool. Bind fills only the encoder's device, pool, size and
 * format; other encoder settings remain the caller's. PQ conversion requires
 * the encoder to keep the creation color/range settings. */
record_gpu_capture_d3d12_t *record_gpu_capture_d3d12_new(
      const nvenc_d3d12_config_t *config, void *command_queue);
bool record_gpu_capture_d3d12_bind(record_gpu_capture_d3d12_t *capture,
      nvenc_d3d12_config_t *config);
record_gpu_queue_t *record_gpu_capture_d3d12_queue(record_gpu_capture_d3d12_t *capture);
void *record_gpu_capture_d3d12_fence(record_gpu_capture_d3d12_t *capture);
/* Producer, on the render queue's owner thread, after submitting the draw.
 * Source state is restored. BGRA capture accepts RGBA via a GPU channel pass.
 * PQ P010 capture also converts PQ/Rec.2020 RGB10A2 or linear scRGB FP16
 * (1.0 = 80 nits) on the GPU, with left/vertical-centre 4:2:0 chroma. RGB conversion supports a viewport crop;
 * direct NV12/P010 copies require the full texture.
 * Submitted GPU work retains source/source_fence through copy completion.
 * Later pushes, reap or free release completed references. Keep source contents
 * unchanged through completion, including on submitted ERROR.
 * Optional source readiness is a GPU wait, issued before the capture copy.
 * Optional ready_value is zero without GPU work, otherwise the completion
 * fence value, including a submission error that must be drained.
 * Returns RECORD_GPU_QUEUE_*; BUSY consumes nothing. No pixel readback/wait. */
int record_gpu_capture_d3d12_push(record_gpu_capture_d3d12_t *capture,
      void *source, unsigned source_state, unsigned x, unsigned y,
      void *source_fence, uint64_t source_value,
      uint64_t timestamp, uint64_t duration, bool keyframe, uint64_t *ready_value);
/* Render producer, or cold controller after producer quiescence. Releases
 * completed source/fence references independently of encoder slot ownership.
 * Call before a swapchain ResizeBuffers, which requires no source references.
 * Returns true when all sources retired; false consumes no unfinished lease.
 * No wait; retry on a later frame. NULL is already retired. */
bool record_gpu_capture_d3d12_reap(record_gpu_capture_d3d12_t *capture);
/* Producer closes; worker uses the borrowed queue/fence until EOS drains.
 * Release slots only after encoder packet release. Free on quiesced sides
 * refuses queued/owned slots and unfinished GPU copies; retry later. */
bool record_gpu_capture_d3d12_finish(record_gpu_capture_d3d12_t *capture);
bool record_gpu_capture_d3d12_free(record_gpu_capture_d3d12_t *capture);
RETRO_END_DECLS
#endif

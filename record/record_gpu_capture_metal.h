/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_GPU_CAPTURE_METAL_H
#define RECORD_GPU_CAPTURE_METAL_H
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
typedef struct record_gpu_capture_metal record_gpu_capture_metal_t;
/* Cold setup on the fixed encoder pixel-buffer pool. Mode 0 is SDR RGB,
 * 1 PQ/BT.2020, 2 linear scRGB (80 nits/unit). HDR requires depth 10.
 * Width/height specify the active image; encoder buffers may be larger.
 * Cold setup attaches the active clean aperture and GPU-replicates borders.
 * Device and 1..16 distinct NV12/P010 buffers are borrowed and retained. */
record_gpu_capture_metal_t *record_gpu_capture_metal_new(void *device,
      void *const *buffers, unsigned count, unsigned width, unsigned height,
      unsigned depth, unsigned mode);
/* Renderer: no active encoder, command buffer retains references, caller
 * commits it. Returns 1 encoded, 0 slot still owned, -1 refused. Input is
 * same-device, single-sample RGB8 / PQ RGB10A2 / scRGB RGBA16F, respectively.
 * No allocation of pixel buffers, waits, readbacks or CPU pixel copies. */
int record_gpu_capture_metal_encode(record_gpu_capture_metal_t *capture,
      unsigned slot, void *command_buffer, void *source, unsigned x, unsigned y);
/* Worker: 0 pending/free, 1 GPU complete, -1 GPU failure. Input stays owned
 * and immutable until encoder/output completion and explicit release. */
int record_gpu_capture_metal_ready(record_gpu_capture_metal_t *capture, unsigned slot);
void *record_gpu_capture_metal_buffer(record_gpu_capture_metal_t *capture, unsigned slot);
bool record_gpu_capture_metal_release(record_gpu_capture_metal_t *capture, unsigned slot);
/* Both producer and worker quiesced; refuses all outstanding GPU/encoder
 * ownership. A command-buffer callback retains plane-view storage via it. */
bool record_gpu_capture_metal_free(record_gpu_capture_metal_t *capture);
RETRO_END_DECLS
#endif

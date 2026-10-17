/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GFX_NVENC_D3D12_H
#define GFX_NVENC_D3D12_H
#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>
#include <formats/video_color.h>

RETRO_BEGIN_DECLS

#define NVENC_D3D12_SLOTS 16
enum nvenc_d3d12_codec
{
   NVENC_D3D12_H264, NVENC_D3D12_HEVC, NVENC_D3D12_AV1
};
enum nvenc_d3d12_format
{
   NVENC_D3D12_RGBA8, NVENC_D3D12_BGRA8, NVENC_D3D12_NV12, NVENC_D3D12_P010
};
enum nvenc_d3d12_result
{
   NVENC_D3D12_ERROR = -1, NVENC_D3D12_OK, NVENC_D3D12_BUSY, NVENC_D3D12_END
};
enum nvenc_d3d12_color
{
   NVENC_D3D12_COLOR_DEFAULT, NVENC_D3D12_COLOR_BT709,
   NVENC_D3D12_COLOR_BT2020_PQ, NVENC_D3D12_COLOR_BT2020_HLG
};
typedef struct nvenc_d3d12 nvenc_d3d12_t;
typedef struct nvenc_d3d12_config
{
   void *device;
   void *textures[NVENC_D3D12_SLOTS];
   unsigned slots, width, height, fps_num, fps_den, bitrate;
   enum nvenc_d3d12_codec codec;
   enum nvenc_d3d12_format format;
   /* Describes input pixels; no transfer or gamut conversion is performed.
    * DEFAULT preserves SDK defaults. HDR requires P010. */
   enum nvenc_d3d12_color color;
   bool full_range;
   /* Optional HDR static metadata, copied at open. The mastering display
    * uses video_color.h units and must be complete when present. Requires
    * PQ/HLG P010 input; emitted on requested and periodic keyframes. */
   video_mastering_display_t mastering;
   unsigned max_cll, max_fall;
} nvenc_d3d12_config_t;
typedef struct nvenc_d3d12_packet
{
   const void *data;
   size_t size;
   uint64_t timestamp, duration;
   unsigned slot;
   bool keyframe;
} nvenc_d3d12_packet_t;

/* Encoder-worker ownership throughout. Textures/device are retained at open.
 * Each accepted slot stays immutable until its packet is released. The GPU
 * waits for ready_fence/value; input textures must be in COMMON state.
 * The ready fence must belong to the encoder device; no pixel readback or CPU fence wait is used.
 * Packet memory belongs to the encoder until release. No B frames/lookahead.
 * Startup/SDK teardown belong on the worker, never the display frame path. */
nvenc_d3d12_t *nvenc_d3d12_open(const nvenc_d3d12_config_t *config);
/* Worker startup, before the first accepted frame. Copies sequence headers
 * (Annex B or AV1 OBUs) for a container's sample description. */
bool nvenc_d3d12_headers(nvenc_d3d12_t *encoder, void *data,
      size_t capacity, size_t *size);
int nvenc_d3d12_submit(nvenc_d3d12_t *encoder, unsigned slot,
      uint64_t timestamp, uint64_t duration, void *ready_fence, uint64_t ready_value,
      bool keyframe);
int nvenc_d3d12_poll(nvenc_d3d12_t *encoder, nvenc_d3d12_packet_t *packet);
/* Worker-only borrowed Win32 completion handle for the oldest accepted frame
 * or EOS, or NULL. Never close/reset it; re-query after each service step.
 * This is a scheduling hint, not a promise of progress: also wake on producer
 * input/close and schedule retries for SDK BUSY without pending completion. */
void *nvenc_d3d12_wait_handle(const nvenc_d3d12_t *e);
int nvenc_d3d12_release(nvenc_d3d12_t *encoder);
int nvenc_d3d12_finish(nvenc_d3d12_t *encoder);
/* Refuses accepted frames/unfinished EOS unless the device was removed. */
bool nvenc_d3d12_free(nvenc_d3d12_t *encoder);
RETRO_END_DECLS
#endif

/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef HEVC_RENDER_ORACLE_H
#define HEVC_RENDER_ORACLE_H
#include <stddef.h>
#include <stdint.h>
#include <formats/video_hw_frame.h>
#include "../../../gfx/common/vulkan_common.h"
void *hevc_render_open(vulkan_context_t *ctx, unsigned width, unsigned height);
/* A successful import clears abandon, including on later oracle failure. */
int hevc_render_submit(void *opaque, video_hw_frame_t *hw);
int hevc_render_frame(void *opaque, const video_hw_frame_t *hw,
      const uint8_t *planes, size_t uv_offset, unsigned sample_bytes);
void hevc_render_close(void *opaque);
#endif

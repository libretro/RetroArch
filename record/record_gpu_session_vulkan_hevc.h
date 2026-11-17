/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_GPU_SESSION_VULKAN_HEVC_H
#define RECORD_GPU_SESSION_VULKAN_HEVC_H
#include "record_gpu_session_vulkan.h"
#include "../gfx/common/vulkan_encode_hevc.h"
RETRO_BEGIN_DECLS
/* Same owner, producer and retirement contract as the H.264 session. */
record_gpu_session_vulkan_t *record_gpu_session_vulkan_new_hevc(
      const vulkan_encode_hevc_open_t *encoder,
      const vulkan_encode_hevc_ops_t *ops,
      const record_gpu_output_vulkan_config_t *output, const char *path);
RETRO_END_DECLS
#endif

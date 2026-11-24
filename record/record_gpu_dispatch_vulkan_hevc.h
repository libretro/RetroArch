/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_GPU_DISPATCH_VULKAN_HEVC_H
#define RECORD_GPU_DISPATCH_VULKAN_HEVC_H
#include "record_gpu_dispatch_vulkan.h"
#include "record_gpu_session_vulkan_hevc.h"
RETRO_BEGIN_DECLS
/* Same controller, producer and retirement contract as the H.264 job.
 * CTB size zero selects a supported size on the I/O worker. */
record_gpu_dispatch_vulkan_t *record_gpu_dispatch_vulkan_new_hevc(
      const vulkan_encode_hevc_open_t *encoder,
      const vulkan_encode_hevc_ops_t *ops,
      const record_gpu_output_vulkan_config_t *output, const char *path,
      const vulkan_encode_capture_ops_t *capture_ops);
/* Streaming counterpart; codec must be HEVC, headers remain empty. */
record_gpu_dispatch_vulkan_t *record_gpu_dispatch_vulkan_new_hevc_stream(
      const vulkan_encode_hevc_open_t *encoder,
      const vulkan_encode_hevc_ops_t *ops,
      const record_gpu_output_vulkan_config_t *output, const char *url,
      const vulkan_encode_capture_ops_t *capture_ops, bool legacy);
RETRO_END_DECLS
#endif

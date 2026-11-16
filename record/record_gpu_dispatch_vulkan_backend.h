/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_GPU_DISPATCH_VULKAN_BACKEND_H
#define RECORD_GPU_DISPATCH_VULKAN_BACKEND_H
#include "record_gpu_dispatch_vulkan.h"
#include "record_gpu_session_vulkan_hevc.h"
RETRO_BEGIN_DECLS
typedef union record_gpu_dispatch_vulkan_config
{
   struct
   {
      vulkan_encode_h264_open_t encoder;
      vulkan_encode_h264_ops_t ops;
   } h264;
   struct
   {
      vulkan_encode_hevc_open_t encoder;
      vulkan_encode_hevc_ops_t ops;
   } hevc;
} record_gpu_dispatch_vulkan_config_t;
typedef record_gpu_session_vulkan_t *(*record_gpu_dispatch_vulkan_start_t)(
      const record_gpu_dispatch_vulkan_config_t *config,
      const record_gpu_output_vulkan_config_t *output, const char *path);
/* Copies cold configuration; the codec factory survives job retirement. */
record_gpu_dispatch_vulkan_t *record_gpu_dispatch_vulkan_new_backend(
      const record_gpu_dispatch_vulkan_config_t *config,
      record_gpu_dispatch_vulkan_start_t start,
      const record_gpu_output_vulkan_config_t *output, const char *path,
      const vulkan_encode_capture_ops_t *capture_ops);
RETRO_END_DECLS
#endif

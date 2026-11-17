/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_STREAM_VULKAN_H
#define RECORD_STREAM_VULKAN_H
#include "record_stream_gpu.h"
#include "record_gpu_worker_vulkan.h"
RETRO_BEGIN_DECLS
/* I/O owner. Borrows the Vulkan worker and READY client through shared stream
 * retirement. The caller retains encoder/GPU drain and scheduling ownership.
 * Uses the shared native AAC consumer; audio requires its platform backend. */
record_stream_gpu_t *record_stream_vulkan_new(record_gpu_worker_vulkan_t *worker,
      const record_gpu_output_config_t *config, net_rtmp_client_t *client);
RETRO_END_DECLS
#endif

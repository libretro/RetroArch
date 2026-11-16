/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_GPU_WORKER_VULKAN_HEVC_H
#define RECORD_GPU_WORKER_VULKAN_HEVC_H
#include "record_gpu_worker_vulkan.h"
#include "../gfx/common/vulkan_encode_hevc.h"
RETRO_BEGIN_DECLS
/* Queue, encoder and output borrows obey the shared worker contract. */
record_gpu_worker_vulkan_t *record_gpu_worker_vulkan_new_hevc(
      record_gpu_queue_t *queue, vulkan_encode_hevc_t *encoder);
RETRO_END_DECLS
#endif

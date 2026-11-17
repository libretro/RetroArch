/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_GPU_SESSION_VULKAN_BACKEND_H
#define RECORD_GPU_SESSION_VULKAN_BACKEND_H
#include "record_gpu_session_vulkan.h"
RETRO_BEGIN_DECLS
typedef struct record_gpu_session_vulkan_backend
{
   bool (*drain)(void *encoder);
   bool (*free)(void *encoder);
} record_gpu_session_vulkan_backend_t;
/* Codec factories allocate the queue before opening their encoder. */
record_gpu_session_vulkan_t *record_gpu_session_vulkan_allocate(
      unsigned slots, const record_gpu_output_vulkan_config_t *output,
      const char *path);
/* Takes ownership even on startup failure. Backend survives session free;
 * mux headers are borrowed only during this call. NULL worker means failure. */
void record_gpu_session_vulkan_start(record_gpu_session_vulkan_t *session,
      void *encoder, const record_gpu_session_vulkan_backend_t *backend,
      const vulkan_encode_pool_t *pool, record_gpu_worker_vulkan_t *worker,
      const record_gpu_output_vulkan_config_t *output, const char *path);
RETRO_END_DECLS
#endif

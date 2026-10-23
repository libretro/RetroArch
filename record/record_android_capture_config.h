/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_ANDROID_CAPTURE_CONFIG_H
#define RECORD_ANDROID_CAPTURE_CONFIG_H
#include "record_android_capture_pool.h"
RETRO_BEGIN_DECLS
struct video_device_info;
typedef struct record_android_capture_config record_android_capture_config_t;
/* Cold controller. Device info must publish enabled AHB/FOREIGN/SYNC_FD
 * prerequisites, not merely extension names or procedure addresses. Owns stable
 * operation context and memory properties; allocates no Vulkan/GLES resources.
 * Keep original driver alive through request creation acknowledgment. */
record_android_capture_config_t *record_android_capture_config_new(
      const struct video_device_info *device, unsigned width, unsigned height,
      unsigned count, unsigned mode, bool flip_y);
/* Borrowed through request collection. The request copies the pool description
 * but its submit/close callbacks still refer to this stable owner. Submission
 * must already hold the current driver's existing queue serialization. */
const record_android_capture_pool_config_t *record_android_capture_config_pool(
      const record_android_capture_config_t *config);
/* Only after the matching request and every source lease are collected.
 * No native calls, even when the device/library has already been destroyed. */
void record_android_capture_config_free(
      record_android_capture_config_t *config);
RETRO_END_DECLS
#endif

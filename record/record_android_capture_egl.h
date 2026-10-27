/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_ANDROID_CAPTURE_EGL_H
#define RECORD_ANDROID_CAPTURE_EGL_H
#include "record_android_capture_ops.h"
RETRO_BEGIN_DECLS
typedef struct record_android_capture_egl record_android_capture_egl_t;
/* Cold Android controller. Uses the system EGL display's Android reference
 * counting. False may retain an output if native teardown failed. */
bool record_android_capture_egl_new(
      unsigned mode, record_android_capture_egl_t **output);
/* Copies borrowed display/config/operations and mode into the job config.
 * Dimensions, buffers, count and orientation are preserved; no share group. */
bool record_android_capture_egl_configure(
      const record_android_capture_egl_t *egl,
      record_gpu_mediacodec_config_t *config);
/* All recording contexts, surfaces, imports and workers must be collected.
 * Failed termination retains libraries and is not retried: Android may have
 * already consumed this owner's display reference. */
bool record_android_capture_egl_free(record_android_capture_egl_t *egl);
RETRO_END_DECLS
#endif

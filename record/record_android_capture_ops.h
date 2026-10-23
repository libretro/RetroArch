/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_ANDROID_CAPTURE_OPS_H
#define RECORD_ANDROID_CAPTURE_OPS_H
#include <dynamic/dylib.h>
#include "record_gpu_dispatch_mediacodec.h"
RETRO_BEGIN_DECLS
typedef function_t (*record_android_capture_resolve_t)(
      void *context, const char *name);
/* Cold setup. The resolver's libraries must outlive the recording job.
 * Failure clears all operation tables, preserving the other configuration.
 * Resolved addresses do not replace context/extension capability checks. */
bool record_android_capture_load_ops(record_gpu_mediacodec_config_t *config,
      record_android_capture_resolve_t resolve, void *context);
RETRO_END_DECLS
#endif

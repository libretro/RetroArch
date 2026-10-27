/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_VP9_RUNTIME_H
#define RARCH_VAAPI_VP9_RUNTIME_H
#include "vaapi_hevc_runtime.h"
#include "vaapi_vp9_session.h"
typedef vaapi_hevc_runtime_t vaapi_vp9_runtime_t;
typedef vaapi_hevc_runtime_io_t vaapi_vp9_runtime_io_t;
RETRO_BEGIN_DECLS
/* Cold owner; renderer DRM identity must match the opened render node.
 * Close even after failed startup, retaining storage when close refuses. */
bool vaapi_vp9_runtime_open(vaapi_vp9_runtime_t *runtime, const char *path,
      unsigned major_id, unsigned minor_id, unsigned width, unsigned height,
      unsigned depth, unsigned slots);
bool vaapi_vp9_runtime_open_with_io(vaapi_vp9_runtime_t *runtime,
      const char *path, unsigned major_id, unsigned minor_id, unsigned width,
      unsigned height, unsigned depth, unsigned slots,
      const vaapi_vp9_runtime_io_t *io);
static INLINE bool vaapi_vp9_runtime_close(vaapi_vp9_runtime_t *runtime)
{
   return vaapi_hevc_runtime_close(runtime);
}
RETRO_END_DECLS
#endif

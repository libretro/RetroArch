/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RARCH_VAAPI_VP9_EXPORT_H
#define RARCH_VAAPI_VP9_EXPORT_H
#include "vaapi_vp9_pool.h"
#include "vaapi_hevc_export.h"
typedef vaapi_hevc_export_t vaapi_vp9_export_t;
typedef vaapi_hevc_export_ops_t vaapi_vp9_export_ops_t;
RETRO_BEGIN_DECLS
/* Owner only. Completed picture, generation checked; source survives close.
 * Descriptor validation is not renderer import admission. False needs close. */
bool vaapi_vp9_export_open(vaapi_vp9_export_t *source, vaapi_vp9_pool_t *pool,
      unsigned slot, const vaapi_vp9_export_ops_t *ops);
static INLINE int vaapi_vp9_export_take(vaapi_vp9_export_t *source, unsigned object)
{
   return vaapi_hevc_export_take(source, object);
}
static INLINE bool vaapi_vp9_export_close(vaapi_vp9_export_t *source)
{
   return vaapi_hevc_export_close(source);
}
RETRO_END_DECLS
#endif

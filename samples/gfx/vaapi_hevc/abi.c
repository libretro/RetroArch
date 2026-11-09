/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "../../../gfx/common/vaapi_hevc_runtime.h"
void vaapi_hevc_sdk_signatures(vaapi_hevc_session_ops_t *ops)
{
   ops->max_profiles       = vaMaxNumProfiles;
   ops->profiles           = vaQueryConfigProfiles;
   ops->max_entrypoints    = vaMaxNumEntrypoints;
   ops->entrypoints        = vaQueryConfigEntrypoints;
   ops->attributes         = vaGetConfigAttributes;
   ops->create_config      = vaCreateConfig;
   ops->destroy_config     = vaDestroyConfig;
   ops->surface_attributes = vaQuerySurfaceAttributes;
   ops->create_surfaces    = vaCreateSurfaces;
   ops->destroy_surfaces   = vaDestroySurfaces;
   ops->create_context     = vaCreateContext;
   ops->destroy_context    = vaDestroyContext;
}
#ifdef __linux__
#include <va/va_drm.h>
#endif
#include "../../../gfx/common/vaapi_hevc_export.h"
void vaapi_hevc_export_sdk_signature(vaapi_hevc_export_ops_t *ops)
{
   ops->surface = vaExportSurfaceHandle;
}
void vaapi_hevc_runtime_sdk_signatures(vaapi_hevc_runtime_t *runtime)
{
   runtime->submit.create  = vaCreateBuffer;
   runtime->submit.destroy = vaDestroyBuffer;
   runtime->submit.begin   = vaBeginPicture;
   runtime->submit.render  = vaRenderPicture;
   runtime->submit.end     = vaEndPicture;
   runtime->submit.sync    = vaSyncSurface2;
#ifdef __linux__
   runtime->get_display = vaGetDisplayDRM;
#endif
   runtime->initialize = vaInitialize;
   runtime->terminate  = vaTerminate;
}
#ifdef __linux__
#include <drm_fourcc.h>
typedef char vaapi_r8_matches[(VA_FOURCC('R', '8', ' ', ' ') == DRM_FORMAT_R8)
            ? 1
            : -1];
typedef char vaapi_gr88_matches
      [(VA_FOURCC('G', 'R', '8', '8') == DRM_FORMAT_GR88) ? 1 : -1];
typedef char vaapi_r16_matches[(VA_FOURCC('R', '1', '6', ' ') == DRM_FORMAT_R16)
            ? 1
            : -1];
typedef char vaapi_gr1616_matches
      [(VA_FOURCC('G', 'R', '3', '2') == DRM_FORMAT_GR1616) ? 1 : -1];
#endif

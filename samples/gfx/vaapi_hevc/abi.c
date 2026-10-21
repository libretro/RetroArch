/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../gfx/common/vaapi_hevc_runtime.h"
void vaapi_hevc_sdk_signatures(vaapi_hevc_session_ops_t *ops)
{
 ops->max_profiles=vaMaxNumProfiles;ops->profiles=vaQueryConfigProfiles;
 ops->max_entrypoints=vaMaxNumEntrypoints;ops->entrypoints=vaQueryConfigEntrypoints;
 ops->attributes=vaGetConfigAttributes;ops->create_config=vaCreateConfig;
 ops->destroy_config=vaDestroyConfig;ops->surface_attributes=vaQuerySurfaceAttributes;
 ops->create_surfaces=vaCreateSurfaces;ops->destroy_surfaces=vaDestroySurfaces;
 ops->create_context=vaCreateContext;ops->destroy_context=vaDestroyContext;
}
#include <va/va_drm.h>
#ifdef __linux__
#include <xf86drm.h>
typedef char vaapi_render_node_matches_sdk[(VAAPI_HEVC_RENDER_NODE==DRM_NODE_RENDER)?1:-1];
#endif
void vaapi_hevc_runtime_sdk_signatures(vaapi_hevc_runtime_t *runtime)
{
 runtime->get_display=vaGetDisplayDRM;
 runtime->initialize=vaInitialize;runtime->terminate=vaTerminate;
#ifdef __linux__
 runtime->node_type=drmGetNodeTypeFromFd;
#endif
}

/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_HEVC_EXPORT_H
#define RARCH_VAAPI_HEVC_EXPORT_H
#include "vaapi_hevc_pool.h"
#include "vaapi_hevc_runtime.h"
#include <va/va_drmcommon.h>
RETRO_BEGIN_DECLS
typedef struct vaapi_hevc_export_ops {
 VAStatus (*surface)(VADisplay,VASurfaceID,uint32_t,uint32_t,void*);
 bool (*cloexec)(int);
 void (*close)(int);
 bool (*identity)(int,uint64_t*,uint64_t*);
} vaapi_hevc_export_ops_t;
typedef struct vaapi_hevc_export {
 vaapi_hevc_pool_t *pool;
 vaapi_hevc_export_ops_t ops;
 VADRMPRIMESurfaceDescriptor descriptor;
 uint64_t object_device[4],object_inode[4];
 uint64_t ticket;
 unsigned slot;
 bool active,ready;
} vaapi_hevc_export_t;
/* Cold binding, Linux only; output unchanged on refusal. Runtime libraries
 * and the pool must outlive every export. */
bool vaapi_hevc_export_bind(vaapi_hevc_export_ops_t *ops,const vaapi_hevc_runtime_t *runtime);
/* Owner thread, initially zeroed export storage. Only completed pool pictures.
 * Capture unique DMA-BUF object identities here, never on the renderer.
 * Separate Y/UV layers retain all modifiers and auxiliary planes. Descriptor
 * validation is not renderer import admission. False still requires close. */
bool vaapi_hevc_export_open(vaapi_hevc_export_t *e,vaapi_hevc_pool_t *pool,
 unsigned slot,const vaapi_hevc_export_ops_t *ops);
/* Transfer one object FD to the importer. It owns the FD even if import fails.
 * Other descriptor metadata remains valid. No duplicate transfer. */
int vaapi_hevc_export_take(vaapi_hevc_export_t *e,unsigned object);
/* After all imported GPU resources/use retire, or before any import begins.
 * Closes remaining FDs and returns the pool ticket exactly once. */
bool vaapi_hevc_export_close(vaapi_hevc_export_t *e);
RETRO_END_DECLS
#endif

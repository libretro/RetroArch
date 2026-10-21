/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_HEVC_RUNTIME_H
#define RARCH_VAAPI_HEVC_RUNTIME_H
#include "vaapi_hevc_session.h"
#include <dynamic/dylib.h>
RETRO_BEGIN_DECLS
enum { VAAPI_HEVC_RENDER_NODE=2 };
typedef struct vaapi_hevc_runtime_io {
 /* Returns an owned CLOEXEC character-device FD matching the renderer IDs. */
 int (*open_node)(const char*,unsigned,unsigned);
 void (*close_node)(int);
 dylib_t (*load)(const char*);
 function_t (*symbol)(dylib_t,const char*);
 void (*unload)(dylib_t);
} vaapi_hevc_runtime_io_t;
typedef struct vaapi_hevc_runtime {
 vaapi_hevc_session_t session;
 vaapi_hevc_runtime_io_t io;
 dylib_t va,va_drm,drm;
 VADisplay display;
 VADisplay (*get_display)(int);
 VAStatus (*initialize)(VADisplay,int*,int*);
 VAStatus (*terminate)(VADisplay);
 int (*node_type)(int);
 VAStatus error;
 int fd,major_version,minor_version;
 bool active,ready,closing,terminating,terminated;
} vaapi_hevc_runtime_t;
/* Cold owner only, initially zeroed storage. The renderer supplies its actual
 * DRM render-device major/minor identity. The path is borrowed during open.
 * A false result still requires close. No backend is registered or exported
 * frame compatibility advertised. Linux default; other platforms refuse. */
bool vaapi_hevc_runtime_open(vaapi_hevc_runtime_t *r,const char *path,
      unsigned major_id,unsigned minor_id,unsigned width,unsigned height,
      unsigned depth,unsigned slots,unsigned slice_mode);
/* Same contract with platform I/O supplied explicitly; copied before use. */
bool vaapi_hevc_runtime_open_with_io(vaapi_hevc_runtime_t *r,const char *path,
      unsigned major_id,unsigned minor_id,unsigned width,unsigned height,
      unsigned depth,unsigned slots,unsigned slice_mode,const vaapi_hevc_runtime_io_t *io);
/* Cold executor, after native/GPU work and exports retire. False retains the
 * libraries, display and FD. Once termination begins, only vaTerminate may be
 * retried: an unsuccessful termination can already invalidate the driver. */
bool vaapi_hevc_runtime_close(vaapi_hevc_runtime_t *r);
RETRO_END_DECLS
#endif

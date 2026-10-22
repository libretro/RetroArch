/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_HEVC_SESSION_H
#define RARCH_VAAPI_HEVC_SESSION_H
#include <va/va.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
#define VAAPI_HEVC_SLICE_AUTO 0
typedef struct vaapi_hevc_session_ops {
 int (*max_profiles)(VADisplay);
 VAStatus (*profiles)(VADisplay,VAProfile*,int*);
 int (*max_entrypoints)(VADisplay);
 VAStatus (*entrypoints)(VADisplay,VAProfile,VAEntrypoint*,int*);
 VAStatus (*attributes)(VADisplay,VAProfile,VAEntrypoint,VAConfigAttrib*,int);
 VAStatus (*create_config)(VADisplay,VAProfile,VAEntrypoint,VAConfigAttrib*,int,VAConfigID*);
 VAStatus (*destroy_config)(VADisplay,VAConfigID);
 VAStatus (*surface_attributes)(VADisplay,VAConfigID,VASurfaceAttrib*,unsigned*);
 VAStatus (*create_surfaces)(VADisplay,unsigned,unsigned,unsigned,VASurfaceID*,unsigned,VASurfaceAttrib*,unsigned);
 VAStatus (*destroy_surfaces)(VADisplay,VASurfaceID*,int);
 VAStatus (*create_context)(VADisplay,VAConfigID,int,int,int,VASurfaceID*,int,VAContextID*);
 VAStatus (*destroy_context)(VADisplay,VAContextID);
} vaapi_hevc_session_ops_t;
typedef struct vaapi_hevc_session {
 vaapi_hevc_session_ops_t ops;
 VADisplay display;
 VASurfaceID surfaces[32];
 VAConfigID config;
 VAContextID context;
 VAStatus error;
 unsigned slots,width,height,depth,slice_mode;
 VAProfile profile;
 bool initialized,ready,closing;
} vaapi_hevc_session_t;
/* Cold decoder worker, initially zeroed storage. Display is initialized and
 * borrowed; native functions/library outlive retirement. Explicit NORMAL/BASE mode
 * is a caller implementation choice, not inferred from picture parameters.
 * AUTO prefers NORMAL, including the API default when the attribute is absent,
 * and selects BASE only if NORMAL is not advertised. The caller implements both.
 * Failure can retain partial native ownership: close even after false.
 * This does not prove surface export/import support or register a backend. */
bool vaapi_hevc_session_open(vaapi_hevc_session_t *s,VADisplay display,
      const vaapi_hevc_session_ops_t *ops,unsigned width,unsigned height,
      unsigned depth,unsigned slots,unsigned slice_mode);
/* Cold retirement executor only, after all native/GPU work and exports retire.
 * False retains failed handles for retry; true releases the display/library
 * borrow and permits storage reuse. Never closes or terminates the display. */
bool vaapi_hevc_session_close(vaapi_hevc_session_t *s);
/* Owner-only recovery after successful vaTerminate(display), with native/GPU
 * work and exports retired. For a matching closing session; no native calls.
 * A failed vaTerminate does not permit this release. */
bool vaapi_hevc_session_terminated(vaapi_hevc_session_t *s,VADisplay display);
RETRO_END_DECLS
#endif

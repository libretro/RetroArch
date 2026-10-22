/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RARCH_VAAPI_VP9_SESSION_H
#define RARCH_VAAPI_VP9_SESSION_H
#include "vaapi_hevc_session.h"
#include <retro_inline.h>
/* Share native VA operations and surface retirement, not codec parameters. */
typedef vaapi_hevc_session_t vaapi_vp9_session_t;
typedef vaapi_hevc_session_ops_t vaapi_vp9_session_ops_t;
RETRO_BEGIN_DECLS
/* Cold decoder owner, initialized borrowed display. Profile 0/2, 8/10-bit 420.
 * Failed startup can retain native handles; close even after false. */
bool vaapi_vp9_session_open(vaapi_vp9_session_t *session, VADisplay display,
      const vaapi_vp9_session_ops_t *ops, unsigned width, unsigned height,
      unsigned depth, unsigned slots);
static INLINE bool vaapi_vp9_session_close(vaapi_vp9_session_t *session)
{
   return vaapi_hevc_session_close(session);
}
static INLINE bool vaapi_vp9_session_terminated(
      vaapi_vp9_session_t *session, VADisplay display)
{
   return vaapi_hevc_session_terminated(session, display);
}
RETRO_END_DECLS
#endif

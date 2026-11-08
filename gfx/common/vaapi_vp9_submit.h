/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RARCH_VAAPI_VP9_SUBMIT_H
#define RARCH_VAAPI_VP9_SUBMIT_H
#include "vaapi_vp9.h"
#include "vaapi_vp9_session.h"
#include "vaapi_hevc_submit.h"
RETRO_BEGIN_DECLS

/* VP9 parameters share native transaction retirement, not HEVC syntax. */
typedef vaapi_hevc_submit_t vaapi_vp9_submit_t;
typedef vaapi_hevc_submit_ops_t vaapi_vp9_submit_ops_t;

/* Decoder owner only; create copies the prepared parameters and raw packet.
 * False can retain buffers or poison ownership. Retire before reuse. */
bool vaapi_vp9_submit_start(vaapi_vp9_submit_t *transaction,
      const vaapi_vp9_session_t *session, const vaapi_vp9_submit_ops_t *ops,
      const vaapi_vp9_params_t *params, const uint8_t *data, unsigned size);
static INLINE int vaapi_vp9_submit_poll(vaapi_vp9_submit_t *transaction)
{
   return vaapi_hevc_submit_poll(transaction);
}
static INLINE bool vaapi_vp9_submit_retire(vaapi_vp9_submit_t *transaction)
{
   return vaapi_hevc_submit_retire(transaction);
}
/* Only after successful vaTerminate of the matching display. */
static INLINE bool vaapi_vp9_submit_terminated(
      vaapi_vp9_submit_t *transaction, VADisplay display)
{
   return vaapi_hevc_submit_terminated(transaction, display);
}
RETRO_END_DECLS
#endif

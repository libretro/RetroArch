/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RARCH_VAAPI_VP9_SUBMIT_H
#define RARCH_VAAPI_VP9_SUBMIT_H
#include "vaapi_hevc_submit.h"
#include "vaapi_vp9_session.h"
#include "vaapi_vp9_picture.h"
typedef vaapi_hevc_submit_t vaapi_vp9_submit_t;
typedef vaapi_hevc_submit_ops_t vaapi_vp9_submit_ops_t;
RETRO_BEGIN_DECLS
/* Decoder owner. Native create copies the borrowed packet and parameters.
 * False may retain handles or uncertain native work; retire before reuse. */
bool vaapi_vp9_submit_start(vaapi_vp9_submit_t *transaction,
      const vaapi_vp9_session_t *session, const vaapi_vp9_submit_ops_t *ops,
      unsigned slot, const vaapi_vp9_picture_t *picture, const uint8_t *packet,
      unsigned size);
static INLINE int vaapi_vp9_submit_poll(vaapi_vp9_submit_t *transaction)
{
   return vaapi_hevc_submit_poll(transaction);
}
static INLINE bool vaapi_vp9_submit_retire(vaapi_vp9_submit_t *transaction)
{
   return vaapi_hevc_submit_retire(transaction);
}
/* Only after matching successful vaTerminate, as for shared VA transactions. */
static INLINE bool vaapi_vp9_submit_terminated(
      vaapi_vp9_submit_t *transaction, VADisplay display)
{
   return vaapi_hevc_submit_terminated(transaction, display);
}
RETRO_END_DECLS
#endif

/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RARCH_VAAPI_AV1_SUBMIT_H
#define RARCH_VAAPI_AV1_SUBMIT_H
#include "vaapi_av1.h"
#include "vaapi_av1_session.h"
#include "vaapi_hevc_submit.h"
RETRO_BEGIN_DECLS

typedef vaapi_hevc_submit_t vaapi_av1_submit_t;
typedef vaapi_hevc_submit_ops_t vaapi_av1_submit_ops_t;

#if RARCH_VAAPI_AV1_PARAMS_AVAILABLE
/* Decoder owner only. Create copies the prepared parameters, tiles and packet.
 * False can retain partial buffers or poison the transaction; retire before
 * reuse. Completion tracks the final display surface when grain is applied. */
bool vaapi_av1_submit_start(vaapi_av1_submit_t *transaction,
                            const vaapi_av1_session_t *session,
                            const vaapi_av1_submit_ops_t *ops,
                            const vaapi_av1_params_t *params,
                            const VASliceParameterBufferAV1 *slices,
                            const uint8_t *data, unsigned size);
#endif
static INLINE int vaapi_av1_submit_poll(vaapi_av1_submit_t *transaction)
{
   return vaapi_hevc_submit_poll(transaction);
}
static INLINE bool vaapi_av1_submit_retire(vaapi_av1_submit_t *transaction)
{
   return vaapi_hevc_submit_retire(transaction);
}
/* Only after actual successful vaTerminate of the matching display. */
static INLINE bool vaapi_av1_submit_terminated(vaapi_av1_submit_t *transaction,
                                               VADisplay display)
{
   return vaapi_hevc_submit_terminated(transaction, display);
}
RETRO_END_DECLS
#endif

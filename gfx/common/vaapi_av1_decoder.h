/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RARCH_VAAPI_AV1_DECODER_H
#define RARCH_VAAPI_AV1_DECODER_H
#include "vaapi_av1_pool.h"
#include "vaapi_frame.h"
#include "vaapi_hevc_export.h"
RETRO_BEGIN_DECLS
#if RARCH_VAAPI_AV1_PARAMS_AVAILABLE
typedef struct vaapi_av1_decoder vaapi_av1_decoder_t;
/* Cold owner; borrowed pool/session/runtime outlive detach and retirement.
 * Copies container metadata. Failure leaves initially empty ops unchanged. */
bool vaapi_av1_decoder_open(vaapi_av1_pool_t *pool,
      const rav1_sequence_t *sequence, const video_color_metadata_t *color,
      const vaapi_hevc_export_ops_t *exports, rav1_hw_ops_t *ops);
/* Owner thread, including paused and closing ticks. One bounded pass over
 * completion checks and renderer returns: 1 idle, 0 pending, negative failure. */
int vaapi_av1_decoder_poll(vaapi_av1_decoder_t *decoder);
/* Stop admission and transfer to the existing owner retirement executor. */
vaapi_av1_decoder_t *vaapi_av1_decoder_detach(rav1_hw_ops_t *ops);
/* Detached owner only. True consumes the adapter after all leases return. */
bool vaapi_av1_decoder_retire(vaapi_av1_decoder_t *decoder);
#endif
RETRO_END_DECLS
#endif

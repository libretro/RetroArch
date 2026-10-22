/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_HEVC_DECODER_H
#define RARCH_VAAPI_HEVC_DECODER_H
#include "vaapi_frame.h"
#include "vaapi_hevc_export.h"
RETRO_BEGIN_DECLS

typedef struct vaapi_hevc_decoder vaapi_hevc_decoder_t;
/* Cold decoder owner; borrowed pool/session/runtime outlive detach/retirement.
 * Initially empty ops; BASE-mode adapter. Copies metadata and creates its opaque carriers.
 * Failure leaves ops unchanged; no native context ownership is transferred. */
bool vaapi_hevc_decoder_open(vaapi_hevc_pool_t *pool,
      const rh265_video_params_t *params, const video_color_metadata_t *color,
      unsigned yuv, const vaapi_hevc_export_ops_t *exports,
      rh265_hw_ops_t *ops);
/* Decoder owner, including paused/closing ticks. One bounded pass over native
 * completions and renderer returns. 1 decode transactions idle, 0 pending, -1
 * error. A failed transaction does not prevent collection of other returned
 * leases. */
int vaapi_hevc_decoder_poll(vaapi_hevc_decoder_t *decoder);
/* Decoder owner: stop admission, clear only this adapter's ops, transfer the
 * adapter to the owner's existing deferred-retirement executor. No wait. */
vaapi_hevc_decoder_t *vaapi_hevc_decoder_detach(rh265_hw_ops_t *ops);
/* Detached decoder owner. True consumes adapter after all frame leases return;
 * false retains it for retry. Pool/native transactions remain caller-owned. */
bool vaapi_hevc_decoder_retire(vaapi_hevc_decoder_t *decoder);
RETRO_END_DECLS
#endif

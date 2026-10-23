/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_AV1_DECODER_H
#define RARCH_VAAPI_AV1_DECODER_H
#include "vaapi_frame.h"
#include "vaapi_av1_export.h"
RETRO_BEGIN_DECLS

typedef struct vaapi_av1_decoder vaapi_av1_decoder_t;
/* Source CICP intersection; mastering/light data stays intact. */
bool vaapi_av1_color_resolve(const rav1_sequence_t *sequence,
      const video_color_metadata_t *container, video_color_metadata_t *out);
/* Cold decoder owner; borrowed pool/session/runtime outlive detach/retirement.
 * Initially empty ops. Copies metadata and creates its opaque carriers.
 * Failure leaves ops unchanged; no native context ownership is transferred. */
bool vaapi_av1_decoder_open(vaapi_av1_pool_t *pool,
      const rav1_sequence_t *params, const video_color_metadata_t *color,
      const vaapi_av1_export_ops_t *exports, rav1_hw_ops_t *ops);
/* Decoder owner, including paused/closing ticks. One bounded pass over native
 * completions and renderer returns. 1 decode transactions idle, 0 pending, -1
 * error. A failed transaction does not prevent collection of other returned
 * leases. */
int vaapi_av1_decoder_poll(vaapi_av1_decoder_t *decoder);
/* Decoder owner: stop admission, clear only this adapter's ops, transfer the
 * adapter to the owner's existing deferred-retirement executor. No wait. */
vaapi_av1_decoder_t *vaapi_av1_decoder_detach(rav1_hw_ops_t *ops);
/* Detached decoder owner. True consumes adapter after all frame leases return;
 * false retains it for retry. Pool/native transactions remain caller-owned. */
bool vaapi_av1_decoder_retire(vaapi_av1_decoder_t *decoder);
RETRO_END_DECLS
#endif

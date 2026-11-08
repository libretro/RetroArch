/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RARCH_VAAPI_VP9_DECODER_H
#define RARCH_VAAPI_VP9_DECODER_H
#include "vaapi_vp9_pool.h"
#include "vaapi_frame.h"
#include "vaapi_hevc_export.h"
RETRO_BEGIN_DECLS
typedef struct vaapi_vp9_decoder vaapi_vp9_decoder_t;
/* Cold owner; borrowed pool/session/runtime outlive detach and retirement.
 * Copies container metadata. Failure leaves initially empty ops unchanged. */
bool vaapi_vp9_decoder_open(vaapi_vp9_pool_t *pool,
      const rvp9_hdr *header, const video_color_metadata_t *color,
      const vaapi_hevc_export_ops_t *exports, rvp9_hw_ops_t *ops);
/* Explicit sample position: 1 left/vertical-center, 2 colocated with luma.
 * The ordinary VP9 constructor uses the colocated default. */
bool vaapi_vp9_decoder_open_position(vaapi_vp9_pool_t *pool,
      const rvp9_hdr *header, const video_color_metadata_t *color,
      unsigned chroma_position, const vaapi_hevc_export_ops_t *exports,
      rvp9_hw_ops_t *ops);
/* Owner thread, including paused and closing ticks. One bounded pass over
 * completion checks and renderer returns: 1 idle, 0 pending, negative failure. */
int vaapi_vp9_decoder_poll(vaapi_vp9_decoder_t *decoder);
/* Stop admission and transfer to the existing owner retirement executor. */
vaapi_vp9_decoder_t *vaapi_vp9_decoder_detach(rvp9_hw_ops_t *ops);
/* Detached owner only. True consumes the adapter after all leases return. */
bool vaapi_vp9_decoder_retire(vaapi_vp9_decoder_t *decoder);
RETRO_END_DECLS
#endif

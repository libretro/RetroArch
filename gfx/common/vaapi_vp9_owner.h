/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_VP9_OWNER_H
#define RARCH_VAAPI_VP9_OWNER_H
#include "vaapi_vp9_decoder.h"
#include "vaapi_vp9_runtime.h"
RETRO_BEGIN_DECLS
typedef struct vaapi_vp9_owner vaapi_vp9_owner_t;
/* Cold owner; empty out/ops. False can publish a partial owned context.
 * Transfer that context to retirement; borrowed metadata expires on return. */
bool vaapi_vp9_owner_open(vaapi_vp9_owner_t **out, const char *path,
      unsigned major_id, unsigned minor_id, const rvp9_hdr *header,
      const video_color_metadata_t *color, unsigned slots, rvp9_hw_ops_t *ops);
bool vaapi_vp9_owner_open_with_io(vaapi_vp9_owner_t **out, const char *path,
      unsigned major_id, unsigned minor_id, const rvp9_hdr *header,
      const video_color_metadata_t *color, unsigned slots,
      const vaapi_vp9_runtime_io_t *io, const vaapi_vp9_export_ops_t *exports,
      rvp9_hw_ops_t *ops);
/* Decoder owner: stop admission and clear only this owner's callbacks. */
vaapi_vp9_owner_t *vaapi_vp9_owner_detach(rvp9_hw_ops_t *ops);
/* Cold executor only. Bounded completion/return pass; never wait for GPU.
 * False retains all resources. No termination before exports return. */
bool vaapi_vp9_owner_retire(vaapi_vp9_owner_t *owner);
RETRO_END_DECLS
#endif

/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_AV1_OWNER_H
#define RARCH_VAAPI_AV1_OWNER_H
#include "vaapi_av1_decoder.h"
#include "vaapi_av1_runtime.h"
RETRO_BEGIN_DECLS
typedef struct vaapi_av1_owner vaapi_av1_owner_t;
/* Cold owner; empty out/ops. False can publish a partial owned context.
 * Transfer that context to retirement; borrowed metadata expires on return. */
bool vaapi_av1_owner_open(vaapi_av1_owner_t **out, const char *path,
      unsigned major_id, unsigned minor_id, const rav1_sequence_t *header,
      const video_color_metadata_t *color, unsigned slots, rav1_hw_ops_t *ops);
bool vaapi_av1_owner_open_with_io(vaapi_av1_owner_t **out, const char *path,
      unsigned major_id, unsigned minor_id, const rav1_sequence_t *header,
      const video_color_metadata_t *color, unsigned slots,
      const vaapi_av1_runtime_io_t *io, const vaapi_av1_export_ops_t *exports,
      rav1_hw_ops_t *ops);
/* Decoder owner: stop admission and clear only this owner's callbacks. */
vaapi_av1_owner_t *vaapi_av1_owner_detach(rav1_hw_ops_t *ops);
/* Cold executor only. Bounded completion/return pass; never wait for GPU.
 * False retains all resources. No termination before exports return. */
bool vaapi_av1_owner_retire(vaapi_av1_owner_t *owner);
RETRO_END_DECLS
#endif

/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_HEVC_OWNER_H
#define RARCH_VAAPI_HEVC_OWNER_H
#include "vaapi_hevc_decoder.h"
RETRO_BEGIN_DECLS

typedef struct vaapi_hevc_owner vaapi_hevc_owner_t;
/* Cold decoder owner, empty out/ops. Path and metadata are borrowed only during
 * open. A false result can still publish an owned partial context in out:
 * transfer it to the existing retirement executor, never discard it. */
bool vaapi_hevc_owner_open(vaapi_hevc_owner_t **out, const char *path,
      unsigned major_id, unsigned minor_id, const rh265_video_params_t *params,
      const video_color_metadata_t *color, unsigned yuv, unsigned slots,
      size_t max_input, rh265_hw_ops_t *ops);
/* Same lifetime, explicit typed platform operations for capability/ABI tests.
 * NULL io uses native bindings; NULL exports resolves the native exporter. */
bool vaapi_hevc_owner_open_with_io(vaapi_hevc_owner_t **out, const char *path,
      unsigned major_id, unsigned minor_id, const rh265_video_params_t *params,
      const video_color_metadata_t *color, unsigned yuv, unsigned slots,
      size_t max_input, const vaapi_hevc_runtime_io_t *io,
      const vaapi_hevc_export_ops_t *exports, rh265_hw_ops_t *ops);
/* Live decoder owner-thread borrow, released at detach or partial retirement.
 * AUTO implements NORMAL and BASE. Source and params describe the same PPS;
 * only parser query scratch changes during decode, never shared DPB state. */
bool vaapi_hevc_owner_open_source(vaapi_hevc_owner_t **out,
      const rh265_video *source, const char *path, unsigned major_id,
      unsigned minor_id, const rh265_video_params_t *params,
      const video_color_metadata_t *color, unsigned yuv, unsigned slots,
      size_t max_input, rh265_hw_ops_t *ops);
bool vaapi_hevc_owner_open_source_with_io(vaapi_hevc_owner_t **out,
      const rh265_video *source, const char *path, unsigned major_id,
      unsigned minor_id, const rh265_video_params_t *params,
      const video_color_metadata_t *color, unsigned yuv, unsigned slots,
      size_t max_input, const vaapi_hevc_runtime_io_t *io,
      const vaapi_hevc_export_ops_t *exports, rh265_hw_ops_t *ops);
/* Decoder owner: clear only this owner's offer, stop admission and transfer
 * the returned context to the existing cold retirement executor. */
vaapi_hevc_owner_t *vaapi_hevc_owner_detach(rh265_hw_ops_t *ops);
/* Cold retirement executor only. One completion/return pass, never a GPU wait.
 * VA destruction/termination can call the native driver. True consumes owner;
 * false retains all still-owned resources. No termination before exports
 * retire. A poisoned pool recovers only through successful display termination.
 */
bool vaapi_hevc_owner_retire(vaapi_hevc_owner_t *owner);
RETRO_END_DECLS
#endif

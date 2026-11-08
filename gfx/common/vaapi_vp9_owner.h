/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RARCH_VAAPI_VP9_OWNER_H
#define RARCH_VAAPI_VP9_OWNER_H
#include "vaapi_vp9_decoder.h"
#include "vaapi_vp9_runtime.h"
RETRO_BEGIN_DECLS
typedef struct vaapi_vp9_owner vaapi_vp9_owner_t;
/* Cold owner, initially empty out/ops. False may publish partial ownership in
 * out; transfer it to the existing retirement executor even on startup failure.
 * Input metadata/path are borrowed only during open. */
bool vaapi_vp9_owner_open(vaapi_vp9_owner_t **out, const char *path,
      unsigned major_id, unsigned minor_id, const rvp9_hdr *header,
      const video_color_metadata_t *color, unsigned chroma_position,
      unsigned slots, size_t max_input,
      rvp9_hw_ops_t *ops);
bool vaapi_vp9_owner_open_with_io(vaapi_vp9_owner_t **out, const char *path,
      unsigned major_id, unsigned minor_id, const rvp9_hdr *header,
      const video_color_metadata_t *color, unsigned chroma_position,
      unsigned slots, size_t max_input,
      const vaapi_vp9_runtime_io_t *io, const vaapi_hevc_export_ops_t *exports,
      rvp9_hw_ops_t *ops);
/* Stop admission; the returned owner goes to the cold retirement executor. */
vaapi_vp9_owner_t *vaapi_vp9_owner_detach(rvp9_hw_ops_t *ops);
/* One bounded completion/return pass, never a GPU wait. True consumes owner;
 * false retains its native handles and storage. Exports prevent termination. */
bool vaapi_vp9_owner_retire(vaapi_vp9_owner_t *owner);
RETRO_END_DECLS
#endif

/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RARCH_VAAPI_AV1_OWNER_H
#define RARCH_VAAPI_AV1_OWNER_H
#include "vaapi_av1_decoder.h"
#include "vaapi_av1_runtime.h"
RETRO_BEGIN_DECLS
#if RARCH_VAAPI_AV1_PARAMS_AVAILABLE
typedef struct vaapi_av1_owner vaapi_av1_owner_t;
/* Cold owner, initially empty out/ops. False may publish partial ownership in
 * out; transfer it to the existing retirement executor even on startup failure.
 * Input metadata/path are borrowed only during open. */
bool vaapi_av1_owner_open(vaapi_av1_owner_t **out, const char *path,
      unsigned major_id, unsigned minor_id, const rav1_sequence_t *sequence,
      const video_color_metadata_t *color,
      unsigned slots, size_t max_input,
      rav1_hw_ops_t *ops);
bool vaapi_av1_owner_open_with_io(vaapi_av1_owner_t **out, const char *path,
      unsigned major_id, unsigned minor_id, const rav1_sequence_t *sequence,
      const video_color_metadata_t *color,
      unsigned slots, size_t max_input,
      const vaapi_av1_runtime_io_t *io, const vaapi_hevc_export_ops_t *exports,
      rav1_hw_ops_t *ops);
/* Stop admission; the returned owner goes to the cold retirement executor. */
vaapi_av1_owner_t *vaapi_av1_owner_detach(rav1_hw_ops_t *ops);
/* One bounded completion/return pass, never a GPU wait. True consumes owner;
 * false retains its native handles and storage. Exports prevent termination. */
bool vaapi_av1_owner_retire(vaapi_av1_owner_t *owner);
#endif
RETRO_END_DECLS
#endif

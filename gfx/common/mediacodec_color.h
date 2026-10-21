/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MEDIACODEC_COLOR_H
#define MEDIACODEC_COLOR_H
#include "mediacodec_session.h"
#include <formats/video_color.h>
RETRO_BEGIN_DECLS
/* Resolve Android MediaFormat constants against coded CICP metadata. Absent
 * values stay absent; unknown native values and explicit conflicts refuse.
 * No resolution-based defaults or HDR-to-SDR reinterpretation. Failure clears
 * out. coded and out may alias; mastering/light metadata is preserved. */
bool mediacodec_color_resolve(const mediacodec_output_format_t *native,
      const video_color_metadata_t *coded,video_color_metadata_t *out);
RETRO_END_DECLS
#endif

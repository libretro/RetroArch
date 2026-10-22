/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef MEDIACODEC_COLOR_H
#define MEDIACODEC_COLOR_H
#include "mediacodec_session.h"
#include <formats/rvp9.h>
#include <formats/rav1_sequence.h>
#include <formats/video_color.h>
RETRO_BEGIN_DECLS
/* Resolve Android MediaFormat constants against coded CICP metadata. Absent
 * values stay absent; unknown native values and explicit conflicts refuse.
 * No resolution-based defaults or HDR-to-SDR reinterpretation. Failure clears
 * out. coded and out may alias; mastering/light metadata is preserved. */
bool mediacodec_color_resolve(const mediacodec_output_format_t *native,
      const video_color_metadata_t *coded, video_color_metadata_t *out);
/* Intersect VP9 color-space/range syntax with optional container CICP.
 * Transfer/mastering/light metadata is not inferred from the VP9 color space.
 * Failure clears out; out may alias container, but not header. */
bool mediacodec_vp9_color(const rvp9_hdr *header,
      const video_color_metadata_t *container, video_color_metadata_t *out);
bool mediacodec_av1_color(const rav1_sequence_t *sequence,
      const video_color_metadata_t *container, video_color_metadata_t *out);
/* Cold input-format hints. Unknown values stay unset; unsupported explicit
 * combinations refuse without changing config. */
bool mediacodec_color_configure(
      const video_color_metadata_t *color, mediacodec_config_t *config);
RETRO_END_DECLS
#endif

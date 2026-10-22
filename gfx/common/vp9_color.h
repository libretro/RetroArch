/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VP9_COLOR_H
#define RETROARCH_VP9_COLOR_H
#include <formats/rvp9_color.h>

static INLINE bool vp9_color_resolve(const rvp9_hdr *header,
      const video_color_metadata_t *container, video_color_metadata_t *out)
{
   return rvp9_color_resolve(header, container, out);
}
#endif

/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_VP9_CONFIG_H
#define RETROARCH_VIDEOTOOLBOX_VP9_CONFIG_H
#include <formats/rvp9_color.h>
#include <formats/image.h>

typedef struct videotoolbox_vp9_config
{
   video_color_metadata_t color;
   unsigned width, height, pixfmt;
   uint8_t vpcc[12];
} videotoolbox_vp9_config_t;

/* Cold admission for the CoreMedia vpcC atom payload, including FullBox
 * version/flags. Level zero means unspecified, not an invented device level.
 * chroma_position: 0 vertical, 1 colocated with luma; caller must know the
 * source convention. No other subsampling is admitted by this Metal path.
 * This describes the stream; it does not prove native hardware support. */
static INLINE bool videotoolbox_vp9_configure(const rvp9_hdr *header,
      const video_color_metadata_t *container, unsigned chroma_position,
      videotoolbox_vp9_config_t *out)
{
   video_color_metadata_t color;
   if (!out)
      return false;
   /* Resolve first, since container may be the previous out->color. */
   if (!rvp9_color_resolve(header, container, &color))
   {
      memset(out, 0, sizeof(*out));
      return false;
   }
   memset(out, 0, sizeof(*out));
   if (header->w <= 0 || header->h <= 0 || header->w > 16384
         || header->h > 16384 || chroma_position > 1
         || (color.primaries == 3
               || (color.primaries > 12 && color.primaries != 22))
         || color.transfer == 3 || color.transfer > 18 || color.matrix == 3
         || color.matrix > 14
         || ((color.transfer == 16 || color.transfer == 18)
               && header->bit_depth != 10))
      return false;
   out->width  = (unsigned)header->w;
   out->height = (unsigned)header->h;
   out->pixfmt =
         header->bit_depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12;
   out->color   = color;
   out->vpcc[0] = 1;
   out->vpcc[4] = (uint8_t)header->profile;
   out->vpcc[6] = (uint8_t)((header->bit_depth << 4) | (chroma_position << 1)
                            | header->color_range);
   out->vpcc[7] = (uint8_t)(color.primaries ? color.primaries : 2);
   out->vpcc[8] = (uint8_t)(color.transfer ? color.transfer : 2);
   out->vpcc[9] = (uint8_t)(color.matrix ? color.matrix : 2);
   return true;
}
#endif

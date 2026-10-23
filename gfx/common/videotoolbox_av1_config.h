/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_AV1_CONFIG_H
#define RETROARCH_VIDEOTOOLBOX_AV1_CONFIG_H
#include <formats/rav1_sequence.h>
#include <formats/image.h>

typedef struct videotoolbox_av1_config
{
   const uint8_t *sequence_obu;
   size_t sequence_size;
   video_color_metadata_t color;
   unsigned width, height, pixfmt, chroma_position;
   uint8_t av1c[4];
} videotoolbox_av1_config_t;

static INLINE bool videotoolbox_av1_color(const rav1_sequence_t *s,
      const video_color_metadata_t *container, video_color_metadata_t *out)
{
   video_color_metadata_t color;
   unsigned coded[3], *fields[3], i, range;
   if (!s || !out || s->color_range > 1 || !s->color_primaries
         || !s->transfer_characteristics || !s->matrix_coefficients
         || s->color_primaries > 255 || s->transfer_characteristics > 255
         || s->matrix_coefficients > 255)
      return false;
   memset(&color, 0, sizeof(color));
   if (container)
      color = *container;
   range = s->color_range ? 2 : 1;
   if (color.range > 2 || (color.range && color.range != range))
      return false;
   coded[0] = s->color_primaries;
   coded[1] = s->transfer_characteristics;
   coded[2] = s->matrix_coefficients;
   fields[0] = &color.primaries;
   fields[1] = &color.transfer;
   fields[2] = &color.matrix;
   for (i = 0; i < 3; i++)
      if (coded[i] != 2)
      {
         if (*fields[i] && *fields[i] != 2 && *fields[i] != coded[i])
            return false;
         *fields[i] = coded[i];
      }
   if (color.primaries == 3 || (color.primaries > 12 && color.primaries != 22)
         || color.transfer == 3 || color.transfer > 18 || color.matrix == 3
         || color.matrix > 14)
      return false;
   color.range = range;
   *out = color;
   return true;
}

/* Cold av1C prefix plus a borrowed, sized sequence OBU. No native capability
 * is implied. The format owner copies the bytes before returning admission. */
static INLINE bool videotoolbox_av1_configure(const rav1_sequence_t *header,
      const rav1_obu_t *sequence, const video_color_metadata_t *container,
      videotoolbox_av1_config_t *out)
{
   rav1_sequence_t parsed;
   rav1_obu_t obu;
   video_color_metadata_t color;
   size_t offset = 0;
   if (!out)
      return false;
   if (!header || !sequence || !sequence->data || !sequence->size
         || sequence->size > 1024u * 1024u
         || rav1_obu_next(&obu, sequence->data, sequence->size, &offset, false)
               != 1
         || offset != sequence->size || obu.type != 1 || obu.temporal_id
         || obu.spatial_id
         || !rav1_sequence_parse(&parsed, obu.payload, obu.payload_size)
         || memcmp(&parsed, header, sizeof(parsed)) || header->profile
         || header->monochrome
         || (header->bit_depth != 8 && header->bit_depth != 10)
         || header->subsampling_x != 1 || header->subsampling_y != 1
         || header->chroma_sample_position > 2 || !header->max_width
         || !header->max_height || header->max_width > 16384
         || header->max_height > 16384 || header->operating_point_count != 1
         || header->operating_points[0].idc
         || !videotoolbox_av1_color(header, container, &color))
   {
      memset(out, 0, sizeof(*out));
      return false;
   }
   memset(out, 0, sizeof(*out));
   out->sequence_obu = obu.data;
   out->sequence_size = obu.size;
   out->width = header->max_width;
   out->height = header->max_height;
   out->pixfmt
         = header->bit_depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12;
   out->chroma_position = header->chroma_sample_position;
   out->color = color;
   out->av1c[0] = 0x81;
   out->av1c[1] = (uint8_t)((header->profile << 5)
         | header->operating_points[0].level);
   out->av1c[2] = (uint8_t)((header->operating_points[0].tier << 7)
         | ((header->bit_depth == 10) << 6) | (header->monochrome << 4)
         | (header->subsampling_x << 3) | (header->subsampling_y << 2)
         | header->chroma_sample_position);
   /* Sample presentation delay cannot be inferred from frame display delay. */
   return true;
}
#endif

/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "mediacodec_frame_export.h"
#include "mediacodec_color.h"
#include <formats/image.h>
#include <stdlib.h>
#include <string.h>
struct mediacodec_frame_exporter
{
   android_frame_pool_t *pool;
   unsigned pixfmt;
};
mediacodec_frame_exporter_t *mediacodec_frame_exporter_new(
      unsigned slots, unsigned pixfmt)
{
   mediacodec_frame_exporter_t *e;
   if (pixfmt != IMAGE_PIXFMT_NV12 && pixfmt != IMAGE_PIXFMT_P010)
      return NULL;
   e = (mediacodec_frame_exporter_t *)calloc(1, sizeof(*e));
   if (!e)
      return NULL;
   e->pool = android_frame_pool_new(slots);
   e->pixfmt = pixfmt;
   if (!e->pool)
   {
      free(e);
      return NULL;
   }
   return e;
}
bool mediacodec_frame_exporter_set_retire_notifier(
      mediacodec_frame_exporter_t *e, void *user,
      void (*retain)(void *), void (*notify_unref)(void *))
{
   return e && android_frame_pool_set_retire_notifier(
         e->pool, user, retain, notify_unref);
}
bool mediacodec_frame_exporter_free(mediacodec_frame_exporter_t *e)
{
   if (!e)
      return true;
   android_frame_pool_close(e->pool);
   if (!android_frame_pool_free(e->pool))
      return false;
   free(e);
   return true;
}

int mediacodec_frame_export_color(void *opaque,
      const android_image_view_t *image,
      const mediacodec_output_format_t *format,
      const video_color_metadata_t *coded, video_hw_frame_t *frame)
{
   mediacodec_frame_exporter_t *e = (mediacodec_frame_exporter_t *)opaque;
   android_frame_view_t view;
   if (!e || !image || !format || !coded || !frame)
      return -1;
   memset(&view, 0, sizeof(view));
   if (!mediacodec_color_resolve(format, coded, &view.color)
         || !view.color.range || !view.color.matrix || view.color.matrix == 2
         || ((!view.color.transfer || view.color.transfer == 2)
               && (e->pixfmt != IMAGE_PIXFMT_NV12 || view.color.primaries != 1
                     || view.color.max_cll || view.color.max_fall
                     || view.color.mastering.present))
         || !view.color.primaries || view.color.primaries == 2)
      return -1;
   view.image = *image;
   view.format = *format;
   view.pixfmt = e->pixfmt;
   return android_frame_export(e->pool, &view, frame);
}

int mediacodec_hevc_frame_export(void *opaque,
      const android_image_view_t *image,
      const mediacodec_output_format_t *format,
      const rh265_video_color_t *coded, video_hw_frame_t *frame)
{
   video_color_metadata_t color;
   if (!coded)
      return -1;
   memset(&color, 0, sizeof(color));
   if (coded->description_present)
   {
      color.primaries = coded->primaries;
      color.transfer = coded->transfer;
      color.matrix = coded->matrix;
   }
   if (coded->video_signal_present)
      color.range = coded->full_range ? 2 : 1;
   return mediacodec_frame_export_color(opaque, image, format, &color, frame);
}

int mediacodec_vp9_frame_export(void *opaque, const android_image_view_t *image,
      const mediacodec_output_format_t *format, const rvp9_hdr *header,
      const video_color_metadata_t *container, video_hw_frame_t *frame)
{
   mediacodec_frame_exporter_t *e = (mediacodec_frame_exporter_t *)opaque;
   video_color_metadata_t color;
   if (!mediacodec_vp9_color(header, container, &color))
      return -1;
   if (!e
         || e->pixfmt
               != (header->bit_depth == 10 ? IMAGE_PIXFMT_P010
                                           : IMAGE_PIXFMT_NV12))
      return -1;
   return mediacodec_frame_export_color(opaque, image, format, &color, frame);
}

int mediacodec_av1_frame_export(void *opaque, const android_image_view_t *image,
      const mediacodec_output_format_t *format, const rav1_sequence_t *sequence,
      const video_color_metadata_t *container, video_hw_frame_t *frame)
{
   mediacodec_frame_exporter_t *e = (mediacodec_frame_exporter_t *)opaque;
   video_color_metadata_t color;
   if (!mediacodec_av1_color(sequence, container, &color) || !e
         || e->pixfmt
               != (sequence->bit_depth == 10 ? IMAGE_PIXFMT_P010
                                             : IMAGE_PIXFMT_NV12))
      return -1;
   return mediacodec_frame_export_color(opaque, image, format, &color, frame);
}

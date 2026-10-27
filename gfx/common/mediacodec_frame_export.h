/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef MEDIACODEC_FRAME_EXPORT_H
#define MEDIACODEC_FRAME_EXPORT_H
#include "android_frame.h"
#include <formats/rh265.h>
#include <formats/rvp9.h>
#include <formats/rav1_sequence.h>
RETRO_BEGIN_DECLS
typedef struct mediacodec_frame_exporter mediacodec_frame_exporter_t;
mediacodec_frame_exporter_t *mediacodec_frame_exporter_new(
      unsigned slots, unsigned pixfmt);
bool mediacodec_frame_exporter_set_retire_notifier(
      mediacodec_frame_exporter_t *exporter, void *user,
      void (*retain)(void *), void (*notify_unref)(void *));
/* Cold owner after producer closure; false retains live renderer leases. */
bool mediacodec_frame_exporter_free(mediacodec_frame_exporter_t *exporter);
/* Resolved coded/container metadata; explicit native conflicts refuse before
 * a ticket is moved. 0 takes image, BUSY/error leave image and frame intact. */
int mediacodec_frame_export_color(void *exporter,
      const android_image_view_t *image,
      const mediacodec_output_format_t *format,
      const video_color_metadata_t *coded, video_hw_frame_t *frame);
int mediacodec_vp9_frame_export(void *exporter,
      const android_image_view_t *image,
      const mediacodec_output_format_t *format, const rvp9_hdr *header,
      const video_color_metadata_t *container, video_hw_frame_t *frame);
/* Native AV1 output already includes grain; export adds no synthesis pass. */
int mediacodec_av1_frame_export(void *exporter,
      const android_image_view_t *image,
      const mediacodec_output_format_t *format, const rav1_sequence_t *sequence,
      const video_color_metadata_t *container, video_hw_frame_t *frame);
/* Signature matches mediacodec_hevc_import.export_image. 0 takes image ticket,
 * 1 BUSY/-1 refusal take nothing and leave frame untouched. Fixed staging only.
 * Rejects native/coded color conflicts and unresolved matrix/range/transfer. */
int mediacodec_hevc_frame_export(void *exporter,
      const android_image_view_t *image,
      const mediacodec_output_format_t *format,
      const rh265_video_color_t *coded, video_hw_frame_t *frame);
RETRO_END_DECLS
#endif

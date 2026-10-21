/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MEDIACODEC_FRAME_EXPORT_H
#define MEDIACODEC_FRAME_EXPORT_H
#include "android_frame.h"
#include <formats/rh265.h>
RETRO_BEGIN_DECLS
typedef struct mediacodec_frame_exporter mediacodec_frame_exporter_t;
mediacodec_frame_exporter_t *mediacodec_frame_exporter_new(unsigned slots,unsigned pixfmt);
/* Cold owner after producer closure; false retains live renderer leases. */
bool mediacodec_frame_exporter_free(mediacodec_frame_exporter_t *exporter);
/* Signature matches mediacodec_hevc_import.export_image. 0 takes image ticket,
 * 1 BUSY/-1 refusal take nothing and leave frame untouched. Fixed staging only.
 * Rejects native/coded color conflicts and unresolved matrix/range/transfer. */
int mediacodec_hevc_frame_export(void *exporter,const android_image_view_t *image,
      const mediacodec_output_format_t *format,const rh265_video_color_t *coded,
      video_hw_frame_t *frame);
RETRO_END_DECLS
#endif

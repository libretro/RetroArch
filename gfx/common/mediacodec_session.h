/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef MEDIACODEC_SESSION_H
#define MEDIACODEC_SESSION_H
#include "mediacodec_inventory.h"
#include <stdint.h>
#include <boolean.h>
RETRO_BEGIN_DECLS
typedef struct mediacodec_session mediacodec_session_t;
typedef struct mediacodec_config
{
   const char *mime;
   unsigned width, height, max_input;
   int profile;
   /* Android input-format constants; zero leaves a key unset. */
   int color_standard, color_transfer, color_range;
   const void *csd[3];
   size_t csd_size[3];
} mediacodec_config_t;
typedef struct mediacodec_output_format
{
   unsigned width, height, left, top, right, bottom;
   int color_standard, color_transfer,
         color_range; /* Android values; zero unknown */
   unsigned serial;
} mediacodec_output_format_t;
/* All operations belong to a decoder worker. Factory requires authoritative
 * hardware admission and exact created component identity. window is borrowed
 * from the image owner, which must survive session destruction. csd entries are
 * already codec-specific Android configuration, not arbitrary MP4 extradata. */
mediacodec_session_t *mediacodec_session_new(
      JNIEnv *env, const mediacodec_config_t *config, void *window);
/* Whole compressed access unit, zero dequeue timeout: 1 accepted, 0 BUSY,
 * -1 sticky failure. BUSY consumes nothing. EOS may accompany a final packet.
 * pts_us uses a nonnegative local epoch; coded input order may reorder PTS. */
int mediacodec_session_submit(mediacodec_session_t *s, const void *data,
      size_t bytes, int64_t pts_us, bool eos);
/* One output action per call: 0 BUSY, 1 released to Surface, 2 format update,
 * 3 EOS (final frame may already have been posted), -1 sticky failure.
 * Surface delivery is asynchronous; the image owner's ready fence is required.
 * Copy output format before the next pump. No CPU pixel readback. */
int mediacodec_session_pump(mediacodec_session_t *s, int64_t *pts_us);
const mediacodec_output_format_t *mediacodec_session_format(
      const mediacodec_session_t *s);
/* Cold worker only; first retire all GPU-held images. Stops/deletes codec,
 * then releases formats/library. Does not close the borrowed image owner. */
void mediacodec_session_free(mediacodec_session_t *s);
RETRO_END_DECLS
#endif

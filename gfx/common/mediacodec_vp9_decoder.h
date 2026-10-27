/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MEDIACODEC_VP9_DECODER_H
#define MEDIACODEC_VP9_DECODER_H
#include <formats/rvp9_hw.h>
#include "mediacodec_session.h"
#include "android_image_queue.h"
RETRO_BEGIN_DECLS
typedef struct mediacodec_vp9_decoder mediacodec_vp9_decoder_t;

typedef struct mediacodec_vp9_import
{
   void *context;
   /* 0 consumes image; BUSY/error leave its ticket untouched. Color retains
    * coded/container metadata; the exporter must reconcile native format. */
   int (*export_image)(void *, const android_image_view_t *,
         const mediacodec_output_format_t *, const rvp9_hdr *,
         const video_color_metadata_t *, video_hw_frame_t *);
   bool (*release)(void *);
   /* Guaranteed deferred execution; never retire inline on close. */
   void (*enqueue)(void *, mediacodec_vp9_decoder_t *);
} mediacodec_vp9_import_t;

/* Cold worker factory. Caller must select VP9 packet-decoder mode. The codec
 * owns references; input is copied by session_submit before acceptance. */
bool mediacodec_vp9_decoder_open(JNIEnv *, const rvp9_hdr *, unsigned slots,
      unsigned max_input, const mediacodec_vp9_import_t *, rvp9_hw_ops_t *);
/* Copies container CICP/mastering/light metadata before native startup. */
bool mediacodec_vp9_decoder_open_color(JNIEnv *, const rvp9_hdr *,
      const video_color_metadata_t *, unsigned slots, unsigned max_input,
      const mediacodec_vp9_import_t *, rvp9_hw_ops_t *);
int mediacodec_vp9_decoder_drain(void *);
/* Nonblocking terminal service; 0 complete, 1 pending, negative failure. */
int mediacodec_vp9_decoder_finish(void *, bool discard);
void mediacodec_vp9_decoder_close(rvp9_hw_ops_t *);
bool mediacodec_vp9_decoder_retire_poll(mediacodec_vp9_decoder_t *);
int mediacodec_vp9_decoder_retire_fd(const mediacodec_vp9_decoder_t *);
RETRO_END_DECLS
#endif

/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef MEDIACODEC_HEVC_DECODER_H
#define MEDIACODEC_HEVC_DECODER_H
#include <formats/rh265_hw.h>
#include "mediacodec_session.h"
#include "android_image_queue.h"
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
typedef struct mediacodec_hevc_decoder mediacodec_hevc_decoder_t;
typedef struct mediacodec_hevc_import
{
   void *context;
   /* Worker callback: wrap an image lease without using a graphics context.
    * 0 consumes the ticket and produces a frame; 1 BUSY/-1 failure take
    * nothing. The frame and its abandonment callback must retain their own
    * context. */
   int (*export_image)(void *context, const android_image_view_t *image,
         const mediacodec_output_format_t *format,
         const rh265_video_color_t *color, video_hw_frame_t *frame);
   /* Cold nonblocking retirement: false keeps context for a later poll. */
   bool (*release)(void *context);
   /* Guaranteed non-inline enqueue of a cold retirement job. The executor calls
    * retire_poll later until true, without spinning or waiting on a frame path.
    */
   void (*enqueue)(void *context, mediacodec_hevc_decoder_t *decoder);
} mediacodec_hevc_import_t;
/* Cold decoder-worker factory. Takes import context only on success. No
 * renderer/backend registration occurs here. max_input provisions fixed
 * staging. */
bool mediacodec_hevc_decoder_open(JNIEnv *env, const rh265_video *source,
      const rh265_video_params_t *params, unsigned slots, unsigned max_input,
      const mediacodec_hevc_import_t *importer, rh265_hw_ops_t *ops);
void mediacodec_hevc_decoder_close(rh265_hw_ops_t *ops);
/* Cold retirement executor only. True consumes decoder. */
bool mediacodec_hevc_decoder_retire_poll(mediacodec_hevc_decoder_t *decoder);
int mediacodec_hevc_decoder_retire_fd(const mediacodec_hevc_decoder_t *decoder);
RETRO_END_DECLS
#endif

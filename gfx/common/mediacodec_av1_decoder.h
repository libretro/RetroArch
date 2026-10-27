/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MEDIACODEC_AV1_DECODER_H
#define MEDIACODEC_AV1_DECODER_H
#include <formats/rav1_hw.h>
#include "mediacodec_session.h"
#include "android_image_queue.h"
RETRO_BEGIN_DECLS
typedef struct mediacodec_av1_decoder mediacodec_av1_decoder_t;

typedef struct mediacodec_av1_import
{
   void *context;
   /* Native output includes grain. Never apply it again during export.
    * 0 consumes image; BUSY/error leave its ticket untouched. */
   int (*export_image)(void *, const android_image_view_t *,
         const mediacodec_output_format_t *, const rav1_sequence_t *,
         const video_color_metadata_t *, video_hw_frame_t *);
   bool (*release)(void *);
   /* Guaranteed deferred execution; never retire inline on close. */
   void (*enqueue)(void *, mediacodec_av1_decoder_t *);
} mediacodec_av1_import_t;

/* Cold factory; av1C includes a validated sequence OBU and is borrowed only
 * during open. Caller selects AV1 packet-decoder mode. Codec owns references.
 */
bool mediacodec_av1_decoder_open(JNIEnv *, const uint8_t *av1c, size_t,
      const video_color_metadata_t *, unsigned slots, unsigned max_input,
      const mediacodec_av1_import_t *, rav1_hw_ops_t *);
int mediacodec_av1_decoder_finish(void *, bool discard);
void mediacodec_av1_decoder_close(rav1_hw_ops_t *);
bool mediacodec_av1_decoder_retire_poll(mediacodec_av1_decoder_t *);
int mediacodec_av1_decoder_retire_fd(const mediacodec_av1_decoder_t *);
RETRO_END_DECLS
#endif

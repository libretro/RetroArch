/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_MEDIACODEC_ENCODER_H
#define RECORD_MEDIACODEC_ENCODER_H
#include "../gfx/common/mediacodec_inventory.h"
#include <stdint.h>
#include <boolean.h>
RETRO_BEGIN_DECLS
typedef struct record_mediacodec_encoder record_mediacodec_encoder_t;
typedef struct record_mediacodec_packet
{
   int64_t pts_us;
   const uint8_t *data;
   size_t size;
   uint32_t flags;
} record_mediacodec_packet_t;
enum record_mediacodec_result
{
   RECORD_MC_ERROR = -1,
   RECORD_MC_BUSY,
   RECORD_MC_PACKET,
   RECORD_MC_FORMAT,
   RECORD_MC_PROGRESS,
   RECORD_MC_END
};
/* Cold recording worker only. Owns a hardware encoder and input Surface.
 * interval is the requested keyframe interval in seconds. */
record_mediacodec_encoder_t *record_mediacodec_encoder_new(JNIEnv *env,
      const mediacodec_encoder_request_t *request, unsigned interval);
/* Borrowed until free; stop/retire the GPU producer before freeing. */
void *record_mediacodec_encoder_surface(record_mediacodec_encoder_t *encoder);
/* Worker only, zero native dequeue timeout, one output action per call.
 * PACKET repeats its borrow until release. Flags use Android key/config/EOS
 * bits 1/2/4. Format is borrowed until the next pump or free. */
int record_mediacodec_encoder_pump(
      record_mediacodec_encoder_t *encoder, record_mediacodec_packet_t *packet);
void *record_mediacodec_encoder_format(record_mediacodec_encoder_t *encoder);
bool record_mediacodec_encoder_release(record_mediacodec_encoder_t *encoder);
/* After the last Surface submission; successful EOS is idempotent. */
bool record_mediacodec_encoder_finish(record_mediacodec_encoder_t *encoder);
void record_mediacodec_encoder_free(record_mediacodec_encoder_t *encoder);
RETRO_END_DECLS
#endif

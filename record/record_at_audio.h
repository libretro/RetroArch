/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_AT_AUDIO_H
#define RECORD_AT_AUDIO_H
#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
typedef struct record_at_audio record_at_audio_t;
typedef struct record_at_audio_packet
{
   int64_t timestamp;
   const uint8_t *data;
   size_t size;
   unsigned frames;
} record_at_audio_packet_t;
enum record_at_audio_result
{
   RECORD_AT_AUDIO_ERROR = -1, RECORD_AT_AUDIO_BUSY,
   RECORD_AT_AUDIO_PROGRESS, RECORD_AT_AUDIO_PACKET, RECORD_AT_AUDIO_END
};
/* One recording worker owns every call. Native AAC-LC, S16 mono/stereo,
 * 44100/48000 Hz. No audio device, frame-thread call or sample-rate change. */
record_at_audio_t *record_at_audio_new(unsigned rate, unsigned channels, unsigned bitrate);
/* Repeats a writable loan until submit. Two fixed buffers protect PCM still
 * held by the native converter until its next input callback. */
int record_at_audio_input(record_at_audio_t *encoder, int16_t **samples, size_t *frames);
/* Contiguous source-frame indices; invalid arguments preserve the loan. */
int record_at_audio_submit(record_at_audio_t *encoder, size_t frames, uint64_t index);
/* Raw AAC loan repeats until release. Timestamp is in source frames and
 * includes native leading priming; it can be negative. Never rebase it. */
int record_at_audio_pump(record_at_audio_t *encoder, record_at_audio_packet_t *packet);
bool record_at_audio_release(record_at_audio_t *encoder);
/* ASC stays borrowed until free. Leading priming is native. Trailing padding
 * is valid after END, calculated from encoded and accepted source frames;
 * it is not the converter's trailing-input requirement. */
bool record_at_audio_config(record_at_audio_t *encoder, const void **data, size_t *size);
bool record_at_audio_timing(record_at_audio_t *encoder, unsigned *leading,
      unsigned *trailing, uint64_t *input_frames);
int record_at_audio_finish(record_at_audio_t *encoder);
/* Return output loans and stop using writable input before cancellation.
 * Reset failure retains converter ownership for a later retry. */
bool record_at_audio_cancel(record_at_audio_t *encoder);
bool record_at_audio_free(record_at_audio_t *encoder);
int32_t record_at_audio_error(const record_at_audio_t *encoder);
RETRO_END_DECLS
#endif

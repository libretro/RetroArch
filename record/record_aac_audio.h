/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_AAC_AUDIO_H
#define RECORD_AAC_AUDIO_H
#include "record_mf_audio.h"

typedef struct record_aac_audio record_aac_audio_t;
typedef record_mf_audio_packet_t record_aac_audio_packet_t;

enum record_aac_audio_result
{
   RECORD_AAC_AUDIO_ERROR = -1, RECORD_AAC_AUDIO_BUSY,
   RECORD_AAC_AUDIO_PROGRESS, RECORD_AAC_AUDIO_PACKET, RECORD_AAC_AUDIO_END
};

RETRO_BEGIN_DECLS
/* One recording I/O owner, including construction and destruction.
 * AAC-LC: 44100/48000 Hz, mono/stereo, 96/128/160/192 kbit/s.
 * Uses Media Foundation when enabled, otherwise the builtin AAC encoder.
 * No audio endpoint is opened. */
record_aac_audio_t *record_aac_audio_new(unsigned rate, unsigned channels, unsigned bitrate);
/* Writable S16 buffer, up to 1024 frames. Pump accepted input before
 * lending another block. Repeated calls repeat the loan. Preparing a
 * submission ends writable access, including BUSY.
 * Encoder work belongs to this worker, never the frame callback. */
int record_aac_audio_input(record_aac_audio_t *encoder, int16_t **samples, size_t *frames);
/* Contiguous source sample clock, with an arbitrary nonnegative first index.
 * Invalid arguments retain the input state. BUSY retains the prepared
 * encoder input; pump output, then retry the same
 * frames/index. PROGRESS transfers the buffer to the encoder. */
int record_aac_audio_submit(record_aac_audio_t *encoder, size_t frames, uint64_t index);
/* Raw 1024-sample AAC frames and 100 ns timestamps; the final frame
 * may include padding. PACKET repeats until release;
 * no further output operation can invalidate the consumer's borrow. */
int record_aac_audio_pump(record_aac_audio_t *encoder, record_aac_audio_packet_t *packet);
/* Ends a live borrow even if encoder cleanup reports a sticky error. */
bool record_aac_audio_release(record_aac_audio_t *encoder);
/* Configuration stays borrowed until free. */
bool record_aac_audio_config(record_aac_audio_t *encoder, const void **data, size_t *size);
/* Return all input/output loans first; BUSY takes nothing. Drain is
 * idempotent. Input indices remain in source coordinates. The builtin
 * encoder discards its leading transform delay and pads only the final block. */
int record_aac_audio_finish(record_aac_audio_t *encoder);
/* Return packet borrows and stop using input loans before cancellation.
 * Flush/shutdown belongs only to this I/O owner. */
bool record_aac_audio_cancel(record_aac_audio_t *encoder);
/* Refuses live loans and unfinished encoding. NULL succeeds. */
bool record_aac_audio_free(record_aac_audio_t *encoder);
uint32_t record_aac_audio_error(const record_aac_audio_t *encoder);
RETRO_END_DECLS
#endif

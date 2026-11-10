/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_MF_AUDIO_H
#define RECORD_MF_AUDIO_H
#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

typedef struct record_mf_audio record_mf_audio_t;
typedef struct record_mf_audio_packet
{
   int64_t timestamp, duration;
   const uint8_t *data;
   size_t size;
} record_mf_audio_packet_t;

enum record_mf_audio_result
{
   RECORD_MF_AUDIO_ERROR = -1, RECORD_MF_AUDIO_BUSY,
   RECORD_MF_AUDIO_PROGRESS, RECORD_MF_AUDIO_PACKET, RECORD_MF_AUDIO_END
};

RETRO_BEGIN_DECLS
/* One recording I/O owner, including construction and destruction. Optional
 * Windows 7+ AAC-LC: 44100/48000 Hz, mono/stereo, 96/128/160/192 kbit/s.
 * Resolves native entry points at runtime. No audio endpoint is opened. */
record_mf_audio_t *record_mf_audio_new(unsigned rate, unsigned channels, unsigned bitrate);
/* Native writable S16 buffer, up to 1024 frames. Pump accepted input before
 * lending another block. Repeated calls repeat the loan. Preparing a
 * submission ends writable access, including BUSY. Native
 * metadata/buffer creation belongs to this worker, never the frame callback. */
int record_mf_audio_input(record_mf_audio_t *encoder, int16_t **samples, size_t *frames);
/* Contiguous source sample clock, with an arbitrary nonnegative first index.
 * Invalid arguments retain the input state. BUSY retains the prepared
 * native input; pump output, then retry the same
 * frames/index. PROGRESS transfers the buffer to the native encoder. */
int record_mf_audio_submit(record_mf_audio_t *encoder, size_t frames, uint64_t index);
/* Raw AAC and native 100 ns timestamps. PACKET repeats until release;
 * no further output operation can invalidate the consumer's borrow. */
int record_mf_audio_pump(record_mf_audio_t *encoder, record_mf_audio_packet_t *packet);
/* Ends a live borrow even if native cleanup reports a sticky error. */
bool record_mf_audio_release(record_mf_audio_t *encoder);
/* Configuration stays borrowed until free. */
bool record_mf_audio_config(record_mf_audio_t *encoder, const void **data, size_t *size);
/* Return all input/output loans first; BUSY takes nothing. Drain is
 * asynchronous and idempotent. No implicit timestamp rebasing or silence. */
int record_mf_audio_finish(record_mf_audio_t *encoder);
/* Return packet borrows and stop using input loans before cancellation.
 * Native flush/shutdown belongs only to this I/O owner. */
bool record_mf_audio_cancel(record_mf_audio_t *encoder);
/* Refuses live loans and unfinished encoding. NULL succeeds. */
bool record_mf_audio_free(record_mf_audio_t *encoder);
uint32_t record_mf_audio_error(const record_mf_audio_t *encoder);
RETRO_END_DECLS
#endif

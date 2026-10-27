/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_AUDIO_TAP_H
#define RECORD_AUDIO_TAP_H
#include "record_audio_queue.h"
RETRO_BEGIN_DECLS
typedef struct record_audio_tap record_audio_tap_t;
/* Cold allocation; one serialized post-mixer producer. Submission borrows S16
 * stereo for the call only and returns RECORD_AUDIO_OK/BUSY/ERROR. */
record_audio_tap_t *record_audio_tap_new(unsigned rate,
      int (*submit)(void *, const int16_t *, size_t, uint64_t), void *context);
unsigned record_audio_tap_rate(const record_audio_tap_t *tap);
/* Audio driver cold owner under its existing state serialization. begin is
 * once-only. retire requires producer quiescence; disconnected marks failure.
 */
bool record_audio_tap_begin(record_audio_tap_t *tap);
void record_audio_tap_retire(record_audio_tap_t *tap, bool disconnected);
/* Existing audio pass serialization; no allocation, wait or lock acquisition.
 * Output-rate stereo after mixing/gain. Failed submission advances no clock. */
bool record_audio_tap_push(record_audio_tap_t *tap, const void *data,
      size_t frames, bool floating, unsigned rate);
bool record_audio_tap_failed(const record_audio_tap_t *tap);
/* Main owner: false while installed. The immutable end is safe to read only
 * after removal or actual audio-driver producer retirement. */
bool record_audio_tap_end(const record_audio_tap_t *tap, uint64_t *frames);
bool record_audio_tap_free(record_audio_tap_t *tap);
RETRO_END_DECLS
#endif

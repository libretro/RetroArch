/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_PYROWAVE_AUDIO_CLOCK_H
#define RECORD_PYROWAVE_AUDIO_CLOCK_H
#include "record_pyrowave_clock.h"
#include "../audio/audio_presentation.h"
RETRO_BEGIN_DECLS
typedef struct record_pyrowave_audio_clock record_pyrowave_audio_clock_t;
/* Cold construction, then one audio owner. No payload storage or copying. */
record_pyrowave_audio_clock_t *record_pyrowave_audio_clock_new(uint64_t generation);
/* Establish/check the native epoch and map the actual presentation cursor.
 * Unknown device silence remains invalid. A queue loss fails this binding;
 * a replacement session must construct a new binding cold. */
bool record_pyrowave_audio_clock_observe(record_pyrowave_audio_clock_t *clock,
      const audio_presentation_position_t *device, record_pyrowave_clock_position_t *source);
/* Before a resampling flush, reserve room for its bounded output fragments.
 * The previous flush's generated output must have been accepted first. */
bool record_pyrowave_audio_clock_begin(record_pyrowave_audio_clock_t *clock,
      uint64_t source_first, uint32_t source_frames);
/* Record the actual source slice and resulting output count, in order.
 * A zero-output slice still consumes its input. Offsets refer to begin(). */
bool record_pyrowave_audio_clock_produced(record_pyrowave_audio_clock_t *clock,
      uint32_t input_offset, uint32_t input_frames, uint32_t output_frames);
/* Preflight before a native write. Accept only its actual positive return,
 * using the driver's accepted-frame ordinal. Partial writes preserve the
 * original resampling ratio across every accepted fragment. */
bool record_pyrowave_audio_clock_writable(const record_pyrowave_audio_clock_t *clock);
bool record_pyrowave_audio_clock_accepted(record_pyrowave_audio_clock_t *clock,
      uint64_t device_first, uint32_t frames);
bool record_pyrowave_audio_clock_pending(const record_pyrowave_audio_clock_t *clock);
bool record_pyrowave_audio_clock_has_output(const record_pyrowave_audio_clock_t *clock);
uint64_t record_pyrowave_audio_clock_end_sample(const record_pyrowave_audio_clock_t *clock);
/* Every generated output frame was accepted and its final accepted range
 * played. Does not invent a source position for silence after that range. */
bool record_pyrowave_audio_clock_drained(const record_pyrowave_audio_clock_t *clock);
bool record_pyrowave_audio_clock_failed(const record_pyrowave_audio_clock_t *clock);
void record_pyrowave_audio_clock_fail(record_pyrowave_audio_clock_t *clock);
void record_pyrowave_audio_clock_free(record_pyrowave_audio_clock_t *clock);
RETRO_END_DECLS
#endif

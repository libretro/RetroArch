/* Copyright (C) 2026 - The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef AUDIO_SPEED_LPF_H
#define AUDIO_SPEED_LPF_H

#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define AUDIO_SPEED_LPF_CHANNELS 11

/* Caller-owned state; initialize before use, then keep on one audio owner.
 * Do not modify fields directly or overlap state with sample storage. */
typedef struct audio_speed_lpf
{
   union {
      float f[2][AUDIO_SPEED_LPF_CHANNELS];
      int64_t i[2][AUDIO_SPEED_LPF_CHANNELS];
   } history;
   unsigned rate, channels, ramp_frames, wet_progress, alpha_progress, block_left;
   uint32_t wet, wet_start, wet_target, alpha, alpha_start, alpha_target;
   float alpha_f, alpha_start_f, alpha_target_f;
   bool is_float, primed;
} audio_speed_lpf_t;

/* Native in-place two-pole low-pass. No allocation, locks or conversions.
 * rate: 8000..192000 Hz; channels: 1..11. Initially fully dry. */
bool audio_speed_lpf_init(audio_speed_lpf_t *state, unsigned rate,
      unsigned channels, bool is_float);

/* Optional fast-forward coloration: floor(0.45 * rate / speed), at least
 * 20 Hz. Q16 speeds at or below unity are dry (zero). Invalid rates are dry.
 * This is a target policy, not an anti-aliasing replacement for the SRC. */
uint32_t audio_speed_lpf_cutoff(unsigned rate, uint32_t speed_q16);

/* Finite positive cutoff in core-rate Hz, clamped to 20..0.45*rate.
 * The cascaded poles have their combined -3 dB point at this frequency.
 * Coefficient and wet/dry changes take 50 ms of processed source frames.
 * Coefficients advance every 64 frames; repeated targets do not restart ramps.
 * Call on the processing owner at the desired source-frame boundary. */
bool audio_speed_lpf_set(audio_speed_lpf_t *state, bool enabled, double cutoff);

/* Matching aligned native frames; float audio must be finite and normalized.
 * Float magnitudes below 1e-20 are flushed to zero while wet to avoid denormals.
 * Fully dry calls leave every sample bit untouched and do not warm history.
 * Invalid arguments and zero-frame calls change nothing. */
bool audio_speed_lpf_process(audio_speed_lpf_t *state, void *samples, size_t frames);
/* Same operation into a separate aligned buffer, without an intermediate
 * copy of wet samples. Source/destination must be identical or disjoint. */
bool audio_speed_lpf_process_into(audio_speed_lpf_t *state,
      const void *source, void *destination, size_t frames);
bool audio_speed_lpf_quiescent(const audio_speed_lpf_t *state);
/* Clear history, keep targets, and restart the engagement fade if enabled. */
void audio_speed_lpf_reset(audio_speed_lpf_t *state);

RETRO_END_DECLS
#endif

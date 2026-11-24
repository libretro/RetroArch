/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef AUDIO_PYROWAVE_H
#define AUDIO_PYROWAVE_H
#include <sys/types.h>
#include "../record/record_pyrowave_audio_clock.h"
RETRO_BEGIN_DECLS
/* Existing audio worker only. A positive return consumes that many source
 * frames; zero applies backpressure, -1 fails the binding. Input is borrowed
 * only for this call. Generated short-write tails remain in output scratch. */
ssize_t audio_driver_pyrowave_submit(record_pyrowave_audio_clock_t *clock,
      const int16_t *samples, size_t frames, unsigned channels, uint64_t first);
/* Poll also retries a retained native tail before allowing another flush. */
bool audio_driver_pyrowave_poll(record_pyrowave_audio_clock_t *clock,
      record_pyrowave_clock_position_t *source);
/* Unmapped output for source starvation/EOF; keeps ALSA from entering XRUN
 * before its final accepted source range can be observed playing. */
ssize_t audio_driver_pyrowave_silence(record_pyrowave_audio_clock_t *clock);
/* Audio owner, or cold after that owner is quiescent. Before freeing clock. */
void audio_driver_pyrowave_detach(record_pyrowave_audio_clock_t *clock);
RETRO_END_DECLS
#endif

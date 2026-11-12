/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_AAC_STREAM_TIMING_H
#define RECORD_AAC_STREAM_TIMING_H
#include "record_aac_probe.h"
RETRO_BEGIN_DECLS
#define RECORD_AAC_STREAM_GUARD 4096u
#define RECORD_AAC_STREAM_PREROLL 2048u
typedef struct record_aac_stream_timing
{
   unsigned input_epoch, coded_origin, audio_epoch, video_epoch_us;
} record_aac_stream_timing_t;
/* Cold worker policy after runtime calibration, not an encoder-delay guess.
 * Feed the probe then GUARD silent input frames before awaiting calibration.
 * Continue silence to input_epoch before accepting original source PCM.
 * Discard complete coded AAC packets before coded_origin. Transmit the
 * remaining silent preroll and offset video by video_epoch_us. Keep native
 * AAC dependencies through this preroll; no MP4 edit or source truncation.
 * Caller must independently validate native encoded/decoded continuity. */
bool record_aac_stream_timing(unsigned rate, unsigned measured_delay,
      record_aac_stream_timing_t *timing);
RETRO_END_DECLS
#endif

/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef AUDIO_PRESENTATION_H
#define AUDIO_PRESENTATION_H
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

typedef struct audio_presentation_position
{
   uint64_t epoch, written, played;
   bool valid, drained, failed;
   /* A valid write_end accepts its entire lend; no borrowed suffix remains. */
   bool lend_complete;
   bool silence_continuous;
} audio_presentation_position_t;

/* Cold main-thread request, before initializing a device. A driver may
 * decline; its presentation_position callback must then return false. */
RETRO_BEGIN_DECLS
void audio_driver_request_presentation(bool enabled);
bool audio_driver_presentation_requested(void);
RETRO_END_DECLS
#endif

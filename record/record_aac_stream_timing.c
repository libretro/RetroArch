/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_aac_stream_timing.h"
#include <string.h>
bool record_aac_stream_timing(unsigned rate, unsigned measured_delay,
      record_aac_stream_timing_t *timing)
{
   unsigned decoded_epoch;
   if (!timing)
      return false;
   memset(timing, 0, sizeof(*timing));
   if (!rate || rate > 192000 || measured_delay > 4096)
      return false;
   decoded_epoch = RECORD_AAC_PROBE_FRAMES + RECORD_AAC_STREAM_GUARD
         + measured_delay + RECORD_AAC_STREAM_PREROLL;
   decoded_epoch = (decoded_epoch + 1023) / 1024 * 1024;
   timing->input_epoch = decoded_epoch - measured_delay;
   timing->coded_origin = decoded_epoch - RECORD_AAC_STREAM_PREROLL;
   timing->audio_epoch = RECORD_AAC_STREAM_PREROLL;
   timing->video_epoch_us = (unsigned)(((uint64_t)timing->audio_epoch
            * 1000000 + rate - 1) / rate);
   return true;
}

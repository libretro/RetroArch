/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef NDEBUG
#error Timing checks require assertions
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../record/record_aac_stream_timing.h"
int main(int argc, char **argv)
{
   static const unsigned rates[] = {8000,32000,44100,48000,96000,192000};
   record_aac_stream_timing_t t, zero;
   unsigned rate, delay;
   (void)argv;
   memset(&zero, 0, sizeof(zero));
   assert(!record_aac_stream_timing(48000, 1024, NULL));
   memset(&t, 255, sizeof(t));
   assert(!record_aac_stream_timing(0, 1024, &t));
   assert(!memcmp(&t, &zero, sizeof(t)));
   assert(!record_aac_stream_timing(192001, 1024, &t));
   assert(!record_aac_stream_timing(48000, 4097, &t));
   for (rate = 0; rate < sizeof(rates) / sizeof(rates[0]); rate++)
      for (delay = 0; delay <= 4096; delay++)
      {
         uint64_t video, audio;
         assert(record_aac_stream_timing(rates[rate], delay, &t));
         assert(t.input_epoch >= RECORD_AAC_PROBE_FRAMES + RECORD_AAC_STREAM_GUARD
               + RECORD_AAC_STREAM_PREROLL);
         assert(t.input_epoch < RECORD_AAC_PROBE_FRAMES + RECORD_AAC_STREAM_GUARD
               + RECORD_AAC_STREAM_PREROLL + 1024);
         assert((t.input_epoch + delay) % 1024 == 0);
         assert(t.coded_origin % 1024 == 0);
         assert(t.input_epoch + delay - t.coded_origin == t.audio_epoch);
         assert(t.audio_epoch == 2048);
         video = (uint64_t)t.video_epoch_us * rates[rate];
         audio = (uint64_t)t.audio_epoch * 1000000;
         assert(video >= audio && video - audio < rates[rate]);
         if (argc > 1 && delay == 1024 && (rates[rate] == 44100 || rates[rate] == 48000))
            printf("%u %u %u %u %u\n", rates[rate], t.input_epoch,
                  t.coded_origin, t.audio_epoch, t.video_epoch_us);
      }
   if (argc == 1)
      puts("PASS: all 4097 calibrated AAC delays at six rates, source boundary alignment and sub-microsecond video epoch rounding");
   return 0;
}

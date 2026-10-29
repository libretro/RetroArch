/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#ifdef __APPLE__
#include <assert.h>
#include <stdlib.h>
#include <AudioToolbox/AudioToolbox.h>

static unsigned info_calls, value_calls;
static OSStatus checked_info(AudioFormatPropertyID property, UInt32 bytes,
      const void *input, UInt32 *size)
{
   assert(property == kAudioFormatProperty_AvailableEncodeSampleRates);
   assert(bytes == sizeof(UInt32) && input
         && *(const UInt32*)input == kAudioFormatMPEG4AAC);
   info_calls++;
   return AudioFormatGetPropertyInfo(property, bytes, input, size);
}
static OSStatus checked_value(AudioFormatPropertyID property, UInt32 bytes,
      const void *input, UInt32 *size, void *output)
{
   assert(property == kAudioFormatProperty_AvailableEncodeSampleRates);
   assert(bytes == sizeof(UInt32) && input
         && *(const UInt32*)input == kAudioFormatMPEG4AAC);
   value_calls++;
   return AudioFormatGetProperty(property, bytes, input, size, output);
}
#define AudioFormatGetPropertyInfo checked_info
#define AudioFormatGetProperty checked_value
#include "../../../record/drivers/record_avfoundation_audio.h"
#undef AudioFormatGetPropertyInfo
#undef AudioFormatGetProperty

int main(void)
{
   static const double inputs[] = {8000, 22050, 44100, 48000};
   UInt32 format = kAudioFormatMPEG4AAC;
   UInt32 size = 0, capacity;
   AudioValueRange *ranges;
   unsigned i, j;
   assert(!AudioFormatGetPropertyInfo(
            kAudioFormatProperty_AvailableEncodeSampleRates,
            sizeof(format), &format, &size));
   assert(size && !(size % sizeof(*ranges)));
   capacity = size;
   ranges = (AudioValueRange*)malloc(size);
   assert(ranges);
   assert(!AudioFormatGetProperty(kAudioFormatProperty_AvailableEncodeSampleRates,
            sizeof(format), &format, &size, ranges));
   assert(size && size <= capacity && !(size % sizeof(*ranges)));
   for (i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++)
   {
      double rate = avfoundation_nearest_aac_sample_rate(inputs[i]);
      int found = 0;
      for (j = 0; j < size / sizeof(*ranges); j++)
         if (rate >= ranges[j].mMinimum && rate <= ranges[j].mMaximum)
            found = 1;
      assert(found && info_calls == i + 1 && value_calls == i + 1);
      printf("AAC source %.0f Hz selects advertised output %.0f Hz\n", inputs[i], rate);
   }
   free(ranges);
   puts("PASS: native AudioToolbox property ABI and advertised AAC rates; no encode claim");
   return 0;
}
#else
int main(void)
{
   puts("UNAVAILABLE: native AudioToolbox requires Apple");
   return 77;
}
#endif

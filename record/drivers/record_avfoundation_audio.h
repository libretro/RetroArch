/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_AVFOUNDATION_AUDIO_H
#define RECORD_AVFOUNDATION_AUDIO_H
#include <AudioToolbox/AudioToolbox.h>
#include <stdlib.h>
/* Query AudioToolbox for the actual valid AAC encode sample rates
 * on this system, then pick the nearest one to the requested rate. */
static double avfoundation_nearest_aac_sample_rate(double rate)
{
   AudioFormatPropertyID prop = kAudioFormatProperty_AvailableEncodeSampleRates;
   UInt32 format = kAudioFormatMPEG4AAC;
   UInt32 size = 0;
   OSStatus status;

   status = AudioFormatGetPropertyInfo(prop, sizeof(format), &format, &size);
   if (status == noErr && size > 0)
   {
      UInt32 capacity = size;
      AudioValueRange *ranges = (AudioValueRange*)malloc(size);
      if (ranges)
      {
         status = AudioFormatGetProperty(prop, sizeof(format), &format,
               &size, ranges);
         if (status == noErr && size <= capacity && !(size % sizeof(AudioValueRange)))
         {
            double best      = 44100.0;
            double best_dist = 1e9;
            unsigned i;
            unsigned count = size / sizeof(AudioValueRange);
            int found = 0;
            for (i = 0; i < count; i++)
            {
               /* Clamp rate into this range's bounds, then measure distance */
               double lo      = ranges[i].mMinimum;
               double hi      = ranges[i].mMaximum;
               double clamped, dist;
               if (!(lo > 0.0) || !(hi >= lo)) continue;
               clamped = rate < lo ? lo : (rate > hi ? hi : rate);
               dist    = rate > clamped
                              ? rate - clamped : clamped - rate;
               if (dist < best_dist)
               {
                  best_dist = dist;
                  best      = clamped;
                  found     = 1;
               }
            }
            if (found) { free(ranges); return best; }
         }
         free(ranges);
      }
   }

   /* Fallback: common safe rates if query fails */
   {
      static const double fallback[] = {
         8000.0, 11025.0, 12000.0, 16000.0, 22050.0, 24000.0,
         32000.0, 44100.0, 48000.0
      };
      double best      = 44100.0;
      double best_dist = 1e9;
      unsigned i;
      for (i = 0; i < sizeof(fallback) / sizeof(fallback[0]); i++)
      {
         double dist = rate > fallback[i]
                     ? rate - fallback[i] : fallback[i] - rate;
         if (dist < best_dist)
         {
            best_dist = dist;
            best      = fallback[i];
         }
      }
      return best;
   }
}

#endif

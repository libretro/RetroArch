/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <AudioToolbox/AudioToolbox.h>
static unsigned mode,allocation_fail;
static AudioValueRange rates[2]={{32000,32000},{48000,48000}};
OSStatus AudioFormatGetPropertyInfo(AudioFormatPropertyID property,UInt32 bytes,const void *input,UInt32 *size)
{
   assert(property==kAudioFormatProperty_AvailableEncodeSampleRates
         && bytes==sizeof(UInt32) && input
         && *(const UInt32*)input==kAudioFormatMPEG4AAC);
   *size=sizeof(rates); return mode==1?-1:0;
}
OSStatus AudioFormatGetProperty(AudioFormatPropertyID property,UInt32 bytes,const void *input,UInt32 *size,void *output)
{
   assert(property==kAudioFormatProperty_AvailableEncodeSampleRates
         && bytes==sizeof(UInt32) && input
         && *(const UInt32*)input==kAudioFormatMPEG4AAC);
   assert(*size==sizeof(rates));
   if (mode==3) return -1;
   memcpy(output,rates,mode==2?sizeof(rates[0]):sizeof(rates));
   *size=mode==2?sizeof(rates[0]):mode==4?0:mode==5?sizeof(rates)+1:sizeof(rates);
   return 0;
}
static void *test_alloc(size_t bytes) { return allocation_fail?NULL:calloc(1,bytes); }
#define malloc test_alloc
#include "../../../record/drivers/record_avfoundation_audio.h"
#undef malloc
int main(void)
{
   assert(avfoundation_nearest_aac_sample_rate(44100)==48000);
   rates[0].mMaximum=48000; assert(avfoundation_nearest_aac_sample_rate(44100)==44100);
   rates[0].mMinimum=rates[0].mMaximum=48000;
   mode=2; assert(avfoundation_nearest_aac_sample_rate(8000)==48000);
   mode=1; assert(avfoundation_nearest_aac_sample_rate(12000)==12000);
   mode=3; assert(avfoundation_nearest_aac_sample_rate(22050)==22050);
   mode=4; assert(avfoundation_nearest_aac_sample_rate(32000)==32000);
   mode=5; assert(avfoundation_nearest_aac_sample_rate(8000)==8000);
   mode=0; allocation_fail=1; assert(avfoundation_nearest_aac_sample_rate(44100)==44100);
   allocation_fail=0; rates[0].mMinimum=rates[0].mMaximum=0;
   rates[1].mMinimum=48000; rates[1].mMaximum=32000;
   assert(avfoundation_nearest_aac_sample_rate(22050)==22050);
   mode=0;
   rates[0].mMinimum=rates[0].mMaximum=8000;
   rates[1].mMinimum=rates[1].mMaximum=22050;
   assert(avfoundation_nearest_aac_sample_rate(8000)==8000);
   assert(avfoundation_nearest_aac_sample_rate(22050)==22050);
   puts("PASS: AAC rate ranges, actual response size, malformed ranges and fallback failures");
   return 0;
}

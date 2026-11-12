/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <AudioToolbox/AudioToolbox.h>
#include <stdio.h>
static OSStatus at_test_new(const AudioStreamBasicDescription *input,
      const AudioStreamBasicDescription *output, AudioConverterRef *converter)
{
   OSStatus status=AudioConverterNew(input,output,converter);
   fprintf(stderr,"AAC new status=%ld input=%.0f/%u flags=%lu bytes=%lu output=%.0f/%u flags=%lu frames=%lu\n",
         (long)status,input->mSampleRate,(unsigned)input->mChannelsPerFrame,
         (unsigned long)input->mFormatFlags,(unsigned long)input->mBytesPerFrame,
         output->mSampleRate,(unsigned)output->mChannelsPerFrame,
         (unsigned long)output->mFormatFlags,(unsigned long)output->mFramesPerPacket);
   return status;
}
static OSStatus at_test_get(AudioConverterRef converter, UInt32 property, UInt32 *size, void *data)
{
   OSStatus status=AudioConverterGetProperty(converter,property,size,data);
   fprintf(stderr,"AAC get property=%lu status=%ld size=%lu\n",(unsigned long)property,(long)status,(unsigned long)*size);
   if(!status && property==kAudioConverterCurrentOutputStreamDescription
         && *size==sizeof(AudioStreamBasicDescription))
   {
      AudioStreamBasicDescription *format=(AudioStreamBasicDescription*)data;
      fprintf(stderr,"AAC actual rate=%.0f channels=%u id=%lu flags=%lu frames=%lu\n",
            format->mSampleRate,(unsigned)format->mChannelsPerFrame,
            (unsigned long)format->mFormatID,(unsigned long)format->mFormatFlags,
            (unsigned long)format->mFramesPerPacket);
   }
   return status;
}
static OSStatus at_test_set(AudioConverterRef converter, UInt32 property, UInt32 size, const void *data)
{
   OSStatus status=AudioConverterSetProperty(converter,property,size,data);
   fprintf(stderr,"AAC set property=%lu status=%ld size=%lu\n",(unsigned long)property,(long)status,(unsigned long)size);
   return status;
}
#define AudioConverterNew at_test_new
#define AudioConverterGetProperty at_test_get
#define AudioConverterSetProperty at_test_set
#include "../../../record/record_at_audio.c"
#undef AudioConverterNew
#undef AudioConverterGetProperty
#undef AudioConverterSetProperty
#include "encode_test.c"

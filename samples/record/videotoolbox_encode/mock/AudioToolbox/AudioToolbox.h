/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_TEST_AUDIO_TOOLBOX_H
#define RECORD_TEST_AUDIO_TOOLBOX_H
#include <stdint.h>
typedef uint32_t UInt32;
typedef UInt32 AudioFormatPropertyID;
typedef int OSStatus;
typedef struct { double mMinimum,mMaximum; } AudioValueRange;
typedef struct
{
   double mSampleRate;
   UInt32 mFormatID,mFormatFlags,mBytesPerPacket,mFramesPerPacket,mBytesPerFrame;
   UInt32 mChannelsPerFrame,mBitsPerChannel,mReserved;
} AudioStreamBasicDescription;
#define kAudioFormatProperty_AvailableEncodeSampleRates 1
#define kAudioFormatMPEG4AAC 2
#define noErr 0
OSStatus AudioFormatGetPropertyInfo(AudioFormatPropertyID,UInt32,const void*,UInt32*);
OSStatus AudioFormatGetProperty(AudioFormatPropertyID,UInt32,const void*,UInt32*,void*);
#endif

/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_AT_MOCK_H
#define RECORD_AT_MOCK_H
#include <stdint.h>
typedef int32_t OSStatus;
typedef uint32_t UInt32;
typedef struct mock_converter *AudioConverterRef;
typedef struct AudioStreamBasicDescription
{
   double mSampleRate;
   UInt32 mFormatID, mFormatFlags, mBytesPerPacket, mFramesPerPacket;
   UInt32 mBytesPerFrame, mChannelsPerFrame, mBitsPerChannel, mReserved;
} AudioStreamBasicDescription;
typedef struct AudioBuffer
{
   UInt32 mNumberChannels, mDataByteSize;
   void *mData;
} AudioBuffer;
typedef struct AudioBufferList
{
   UInt32 mNumberBuffers;
   AudioBuffer mBuffers[1];
} AudioBufferList;
typedef struct AudioStreamPacketDescription
{
   int64_t mStartOffset;
   UInt32 mVariableFramesInPacket, mDataByteSize;
} AudioStreamPacketDescription;
typedef struct AudioConverterPrimeInfo
{
   UInt32 leadingFrames, trailingFrames;
} AudioConverterPrimeInfo;
typedef OSStatus (*AudioConverterComplexInputDataProc)(AudioConverterRef,
      UInt32*, AudioBufferList*, AudioStreamPacketDescription**, void*);
enum
{
   noErr = 0, kAudioFormatLinearPCM = 1, kAudioFormatMPEG4AAC = 2,
   kAudioFormatFlagIsSignedInteger = 4, kAudioFormatFlagIsPacked = 8,
   kAudioFormatFlagsNativeEndian = 0,
   kMPEG4Object_AAC_LC = 2, kAudioConverterPrimeInfo = 10,
   kAudioConverterCurrentOutputStreamDescription,
   kAudioConverterEncodeBitRate, kAudioConverterPropertyMaximumOutputPacketSize
};
OSStatus AudioConverterNew(const AudioStreamBasicDescription*,
      const AudioStreamBasicDescription*, AudioConverterRef*);
OSStatus AudioConverterGetProperty(AudioConverterRef, UInt32, UInt32*, void*);
OSStatus AudioConverterSetProperty(AudioConverterRef, UInt32, UInt32, const void*);
OSStatus AudioConverterFillComplexBuffer(AudioConverterRef,
      AudioConverterComplexInputDataProc, void*, UInt32*, AudioBufferList*, AudioStreamPacketDescription*);
OSStatus AudioConverterReset(AudioConverterRef);
OSStatus AudioConverterDispose(AudioConverterRef);
#endif

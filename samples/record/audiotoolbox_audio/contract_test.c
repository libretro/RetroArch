/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <AudioToolbox/AudioToolbox.h>
#include "../../../record/record_at_audio.h"
struct mock_converter
{
   AudioStreamBasicDescription format;
   const int16_t *retained;
   size_t samples;
   unsigned fills;
   int16_t value;
   bool tail;
};
static unsigned live, calls;
static bool bad_size, reset_fail, short_stream;
OSStatus AudioConverterNew(const AudioStreamBasicDescription *in,
      const AudioStreamBasicDescription *out, AudioConverterRef *converter)
{
   assert(in->mSampleRate == out->mSampleRate && in->mFramesPerPacket == 1);
   assert(in->mBytesPerFrame == in->mChannelsPerFrame * 2);
   *converter = (AudioConverterRef)calloc(1, sizeof(**converter));
   assert(*converter);
   (*converter)->format = *out;
   live++;
   return 0;
}
OSStatus AudioConverterGetProperty(AudioConverterRef c, UInt32 key, UInt32 *size, void *data)
{
   if (key == kAudioConverterCurrentOutputStreamDescription)
   {
      assert(*size == sizeof(c->format));
      memcpy(data, &c->format, *size);
   }
   else if (key == kAudioConverterPropertyMaximumOutputPacketSize)
   {
      assert(*size == sizeof(UInt32));
      *(UInt32*)data = 64;
   }
   else
   {
      AudioConverterPrimeInfo *prime = (AudioConverterPrimeInfo*)data;
      assert(key == kAudioConverterPrimeInfo && *size == sizeof(*prime));
      prime->leadingFrames = 2112;
      prime->trailingFrames = 960;
   }
   return 0;
}
OSStatus AudioConverterSetProperty(AudioConverterRef c, UInt32 key, UInt32 size, const void *data)
{
   (void)c;
   assert(key == kAudioConverterEncodeBitRate && size == sizeof(UInt32) && *(const UInt32*)data);
   return 0;
}
OSStatus AudioConverterFillComplexBuffer(AudioConverterRef c,
      AudioConverterComplexInputDataProc input, void *userdata, UInt32 *packets,
      AudioBufferList *output, AudioStreamPacketDescription *description)
{
   AudioBufferList source;
   UInt32 frames = 512;
   OSStatus status;
   size_t i;
   calls++;
   for (i = 0; i < c->samples; i++)
      assert(c->retained[i] == c->value);
   assert(*packets == 1 && output->mBuffers[0].mDataByteSize == 64);
   memset(&source, 0, sizeof(source));
   status = input(c, &frames, &source, NULL, userdata);
   c->retained = NULL;
   c->samples = 0;
   if (!frames)
   {
      if (!status && !c->tail && !short_stream)
      {
         c->tail = true;
         memset(output->mBuffers[0].mData, 0, 8);
         output->mBuffers[0].mDataByteSize = description->mDataByteSize = 8;
         return 0;
      }
      *packets = 0;
      output->mBuffers[0].mDataByteSize = 0;
      return status;
   }
   assert(!status && source.mNumberBuffers == 1);
   c->retained = (const int16_t*)source.mBuffers[0].mData;
   c->samples = source.mBuffers[0].mDataByteSize / 2;
   c->value = c->retained[0];
   memset(output->mBuffers[0].mData, ++c->fills, 8);
   output->mBuffers[0].mDataByteSize = 8;
   description->mStartOffset = 0;
   description->mDataByteSize = bad_size ? 9 : 8;
   description->mVariableFramesInPacket = 1024;
   return 0;
}
OSStatus AudioConverterReset(AudioConverterRef c)
{
   if (reset_fail)
      return -77;
   c->retained = NULL;
   c->samples = 0;
   return 0;
}
OSStatus AudioConverterDispose(AudioConverterRef c)
{
   assert(live);
   live--;
   free(c);
   return 0;
}
static void fill(int16_t *p, size_t frames, unsigned channels, int16_t value)
{
   size_t i;
   for (i = 0; i < frames * channels; i++)
      p[i] = value;
}
int main(void)
{
   record_at_audio_t *e;
   record_at_audio_packet_t packet, again;
   int16_t *first, *second;
   size_t frames, size;
   const void *asc;
   unsigned leading, trailing, before;
   uint64_t total;
   assert(!record_at_audio_new(32000, 2, 128000));
   assert(!record_at_audio_new(48000, 6, 128000));
   assert(!record_at_audio_new(48000, 2, 64000));
   e = record_at_audio_new(44100, 1, 96000);
   assert(e && record_at_audio_finish(e) == RECORD_AT_AUDIO_PROGRESS);
   assert(record_at_audio_pump(e, &packet) == RECORD_AT_AUDIO_END);
   assert(record_at_audio_free(e) && !live);
   e = record_at_audio_new(48000, 2, 128000);
   assert(e && record_at_audio_config(e, &asc, &size));
   assert(size == 2 && ((const uint8_t*)asc)[0] == 0x11 && ((const uint8_t*)asc)[1] == 0x90);
   assert(record_at_audio_input(e, &first, &frames) == RECORD_AT_AUDIO_PROGRESS && frames == 1024);
   assert(record_at_audio_input(e, &second, &frames) == RECORD_AT_AUDIO_PROGRESS && first == second);
   assert(record_at_audio_submit(e, 1025, 0) == RECORD_AT_AUDIO_ERROR);
   fill(first, frames, 2, 1234);
   assert(record_at_audio_submit(e, 1024, 0) == RECORD_AT_AUDIO_PROGRESS);
   assert(record_at_audio_pump(e, &packet) == RECORD_AT_AUDIO_PACKET && packet.timestamp == -2112);
   before = calls;
   assert(record_at_audio_pump(e, &again) == RECORD_AT_AUDIO_PACKET && calls == before);
   assert(packet.data == again.data && packet.size == 8 && !record_at_audio_free(e));
   assert(record_at_audio_finish(e) == RECORD_AT_AUDIO_BUSY && !record_at_audio_cancel(e));
   assert(record_at_audio_release(e) && !record_at_audio_release(e));
   assert(record_at_audio_input(e, &second, &frames) == RECORD_AT_AUDIO_BUSY);
   assert(record_at_audio_pump(e, &packet) == RECORD_AT_AUDIO_PACKET);
   assert(record_at_audio_release(e));
   assert(record_at_audio_input(e, &second, &frames) == RECORD_AT_AUDIO_PROGRESS && first != second);
   fill(second, frames, 2, -4321);
   assert(record_at_audio_submit(e, 17, 1023) == RECORD_AT_AUDIO_ERROR);
   assert(record_at_audio_submit(e, 17, 1024) == RECORD_AT_AUDIO_PROGRESS);
   assert(record_at_audio_finish(e) == RECORD_AT_AUDIO_PROGRESS);
   assert(record_at_audio_pump(e, &packet) == RECORD_AT_AUDIO_PACKET);
   assert(record_at_audio_release(e));
   assert(record_at_audio_pump(e, &packet) == RECORD_AT_AUDIO_PACKET);
   assert(record_at_audio_release(e));
   assert(record_at_audio_pump(e, &packet) == RECORD_AT_AUDIO_END);
   assert(record_at_audio_timing(e, &leading, &trailing, &total));
   assert(leading == 2112 && trailing == 943 && total == 1041);
   assert(record_at_audio_free(e) && !live);
   e = record_at_audio_new(44100, 1, 96000);
   assert(e && record_at_audio_input(e, &first, &frames) == RECORD_AT_AUDIO_PROGRESS);
   assert(record_at_audio_cancel(e) && record_at_audio_free(e));
   e = record_at_audio_new(48000, 1, 96000);
   assert(e && record_at_audio_input(e, &first, &frames) == RECORD_AT_AUDIO_PROGRESS);
   fill(first, frames, 1, 77);
   assert(record_at_audio_submit(e, 1, 0) == RECORD_AT_AUDIO_PROGRESS);
   bad_size = true;
   assert(record_at_audio_pump(e, &packet) == RECORD_AT_AUDIO_ERROR);
   assert(record_at_audio_error(e));
   reset_fail = true;
   assert(!record_at_audio_cancel(e) && !record_at_audio_free(e));
   reset_fail = false;
   assert(record_at_audio_cancel(e) && record_at_audio_free(e) && !live);
   bad_size = false;
   short_stream = true;
   e = record_at_audio_new(48000, 1, 96000);
   assert(e && record_at_audio_input(e, &first, &frames) == RECORD_AT_AUDIO_PROGRESS);
   fill(first, frames, 1, 55);
   assert(record_at_audio_submit(e, 1, 0) == RECORD_AT_AUDIO_PROGRESS);
   assert(record_at_audio_finish(e) == RECORD_AT_AUDIO_PROGRESS);
   assert(record_at_audio_pump(e, &packet) == RECORD_AT_AUDIO_PACKET);
   assert(record_at_audio_release(e));
   assert(record_at_audio_pump(e, &packet) == RECORD_AT_AUDIO_ERROR);
   assert(record_at_audio_cancel(e) && record_at_audio_free(e) && !live);
   assert(record_at_audio_free(NULL));
   puts("PASS AudioToolbox PCM callback lifetime, packet loans, source clock, bounds and cancellation");
   return 0;
}

/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <AudioToolbox/AudioToolbox.h>
#include "../../../record/record_at_resample.h"
struct mock_converter
{
   AudioStreamBasicDescription output;
   const int16_t *held;
   int16_t first;
};
static unsigned quality, priming, resets, disposed;
static int reset_failure, property_failure, wrong_output;
static const int16_t *expected;
static int16_t *expected_output;

OSStatus AudioConverterNew(const AudioStreamBasicDescription *input,
      const AudioStreamBasicDescription *output, AudioConverterRef *converter)
{
   assert(input->mSampleRate == 32040.5);
   assert(input->mFormatID == kAudioFormatLinearPCM);
   assert(input->mBitsPerChannel == 16 && input->mFramesPerPacket == 1);
   assert(input->mBytesPerFrame == 2 * input->mChannelsPerFrame);
   assert(output->mSampleRate == 48000);
   *converter = (AudioConverterRef)calloc(1, sizeof(**converter));
   assert(*converter);
   (*converter)->output = *output;
   return noErr;
}
OSStatus AudioConverterSetProperty(AudioConverterRef c, UInt32 property,
      UInt32 size, const void *data)
{
   UInt32 value;
   (void)c;
   assert(size == sizeof(value));
   memcpy(&value, data, size);
   if (property_failure)
      return -3;
   if (property == kAudioConverterSampleRateConverterQuality)
   {
      assert(value == kAudioConverterQuality_Max);
      quality++;
   }
   else
   {
      assert(property == kAudioConverterPrimeMethod);
      assert(value == kConverterPrimeMethod_Normal);
      priming++;
   }
   return noErr;
}
OSStatus AudioConverterGetProperty(AudioConverterRef c, UInt32 property,
      UInt32 *size, void *data)
{
   AudioStreamBasicDescription output = c->output;
   assert(property == kAudioConverterCurrentOutputStreamDescription);
   assert(*size == sizeof(output));
   if (wrong_output)
      output.mChannelsPerFrame++;
   memcpy(data, &output, sizeof(output));
   return noErr;
}
OSStatus AudioConverterFillComplexBuffer(AudioConverterRef c,
      AudioConverterComplexInputDataProc callback, void *userdata,
      UInt32 *packets, AudioBufferList *output, AudioStreamPacketDescription *descs)
{
   AudioBufferList input;
   AudioStreamPacketDescription *description = (AudioStreamPacketDescription*)c;
   UInt32 count = *packets;
   OSStatus status;
   assert(!descs && output->mBuffers[0].mData == expected_output);
   if (c->held)
      assert(*c->held == c->first);
   status = callback(c, &count, &input, &description, userdata);
   assert(!description);
   c->held = NULL;
   if (count)
   {
      assert(status == noErr);
      if (expected)
         assert(input.mBuffers[0].mData == expected);
      c->held = (const int16_t*)input.mBuffers[0].mData;
      c->first = *c->held;
      memcpy(output->mBuffers[0].mData, c->held, input.mBuffers[0].mDataByteSize);
   }
   output->mBuffers[0].mDataByteSize = count * c->output.mBytesPerFrame;
   *packets = count;
   return status;
}
OSStatus AudioConverterReset(AudioConverterRef c)
{
   resets++;
   if (reset_failure)
      return -7;
   c->held = NULL;
   return noErr;
}
OSStatus AudioConverterDispose(AudioConverterRef c)
{
   assert(!c->held);
   disposed++;
   free(c);
   return noErr;
}

static void lane(unsigned channels, bool cancel, bool gap)
{
   record_audio_queue_t *q = record_audio_queue_new(channels, 8, 48);
   record_at_resample_t *e;
   record_audio_queue_t *configured;
   const int16_t *borrow;
   int16_t input[16], output[16];
   size_t frames, count;
   uint64_t timestamp, accepted, produced;
   unsigned i;
   unsigned rate, configured_channels;
   int result;
   assert(q);
   for (i = 0; i < 16; i++)
      input[i] = (int16_t)(i + 100);
   assert(record_audio_queue_push(q, input, 8, gap ? 1 : 0) == RECORD_AUDIO_OK);
   assert(record_audio_queue_peek_lend(q, &borrow, &count, &timestamp) == RECORD_AUDIO_OK);
   e = record_at_resample_new(q, 32040.5, 48000);
   assert(e && !record_at_resample_free(e));
   assert(record_at_resample_config(e, &configured, &rate, &configured_channels));
   assert(configured == q && rate == 48000 && configured_channels == channels);
   expected = borrow;
   expected_output = output;
   assert(record_at_resample_read(e, output, 1025, &frames) == RECORD_AT_RESAMPLE_ERROR);
   result = record_at_resample_read(e, output, 3, &frames);
   if (gap)
   {
      assert(result == RECORD_AT_RESAMPLE_ERROR && !frames);
      assert(record_at_resample_cancel(e));
   }
   else
   {
      assert(result == RECORD_AT_RESAMPLE_PROGRESS && frames == 3);
      assert(!memcmp(input, output, 3 * channels * sizeof(int16_t)));
      assert(record_at_resample_timing(e, &accepted, &produced));
      assert(accepted == 3 && produced == 3);
      if (cancel)
      {
         reset_failure = 1;
         assert(!record_at_resample_cancel(e));
         assert(record_at_resample_error(e) == -7);
         assert(!record_audio_queue_ack(NULL));
         assert(record_audio_queue_peek_lend(q, &borrow, &count, &timestamp) == RECORD_AUDIO_OK);
         assert(borrow == expected && count == 8);
         assert(!record_at_resample_free(e));
         reset_failure = 0;
         assert(record_at_resample_cancel(e));
      }
      else
      {
         expected = borrow + 3 * channels;
         assert(record_at_resample_read(e, output, 8, &frames) == RECORD_AT_RESAMPLE_PROGRESS);
         assert(frames == 5 && output[0] == input[3 * channels]);
         assert(record_audio_queue_peek_lend(q, &borrow, &count, &timestamp) == RECORD_AUDIO_OK);
         assert(count == 8);
         expected = NULL;
         assert(record_at_resample_read(e, output, 8, &frames) == RECORD_AT_RESAMPLE_BUSY);
         assert(!frames);
         assert(record_audio_queue_finish(q));
         assert(record_at_resample_read(e, output, 8, &frames) == RECORD_AT_RESAMPLE_END);
         assert(record_at_resample_read(e, output, 8, &frames) == RECORD_AT_RESAMPLE_END);
         assert(record_at_resample_timing(e, &accepted, &produced));
         assert(accepted == 8 && produced == 8);
      }
   }
   assert(!record_at_resample_config(e, &configured, &rate, &configured_channels));
   assert(record_at_resample_free(e));
   assert(record_audio_queue_finish(q));
   assert(record_audio_queue_free(q));
}

int main(void)
{
   record_audio_queue_t *q = record_audio_queue_new(1, 8, 48);
   record_at_resample_t *e;
   int16_t output[8];
   size_t frames;
   assert(q);
   assert(!record_at_resample_new(NULL, 32040.5, 48000));
   assert(!record_at_resample_new(q, 0, 48000));
   assert(!record_at_resample_new(q, 48000, 48000));
   assert(!record_at_resample_new(q, 32040.5, 32000));
   property_failure = 1;
   assert(!record_at_resample_new(q, 32040.5, 48000));
   property_failure = 0;
   wrong_output = 1;
   assert(!record_at_resample_new(q, 32040.5, 48000));
   wrong_output = 0;
   e = record_at_resample_new(q, 32040.5, 48000);
   assert(e && record_audio_queue_finish(q));
   expected_output = output;
   assert(record_at_resample_read(e, output, 8, &frames) == RECORD_AT_RESAMPLE_END);
   assert(record_at_resample_free(e) && record_audio_queue_free(q));
   lane(1, false, false);
   lane(2, false, false);
   lane(1, true, false);
   lane(2, false, true);
   assert(quality == 6 && priming == 6 && resets == 3 && disposed == 7);
   puts("PASS: direct SRC output, native input borrow retirement, clocks, EOF and cancellation");
   return 0;
}

/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_at_resample.h"
#include <stdlib.h>
#include <string.h>
#if (defined(__APPLE__) && defined(HAVE_AVF)) || defined(RECORD_AT_RESAMPLE_TEST)
#include <AudioToolbox/AudioToolbox.h>
#define SRC_NEED_INPUT ((OSStatus)0x72736e69)
struct record_at_resample
{
   uint64_t input_frames, output_frames;
   AudioConverterRef converter;
   record_audio_queue_t *source;
   const int16_t *samples;
   size_t frames, offset;
   unsigned channels;
   OSStatus error;
   bool source_end, ended, cancelled;
};

static OSStatus record_at_resample_input(AudioConverterRef converter,
      UInt32 *packets, AudioBufferList *data,
      AudioStreamPacketDescription **descriptions, void *userdata)
{
   record_at_resample_t *e = (record_at_resample_t*)userdata;
   uint64_t timestamp;
   size_t frames;
   int result;
   (void)converter;
   if (descriptions)
      *descriptions = NULL;
   data->mNumberBuffers = 1;
   data->mBuffers[0].mNumberChannels = e->channels;
   data->mBuffers[0].mData = NULL;
   data->mBuffers[0].mDataByteSize = 0;
   /* The previous callback's pointer is now retired by AudioConverter. */
   if (e->samples && e->offset == e->frames)
   {
      if (!record_audio_queue_ack(e->source))
         goto invalid;
      e->samples = NULL;
   }
   if (!e->samples)
   {
      result = record_audio_queue_peek_lend(e->source, &e->samples,
            &e->frames, &timestamp);
      if (result == RECORD_AUDIO_END || result == RECORD_AUDIO_BUSY)
      {
         e->source_end = result == RECORD_AUDIO_END;
         *packets = 0;
         return e->source_end ? noErr : SRC_NEED_INPUT;
      }
      e->offset = 0;
      if (result != RECORD_AUDIO_OK || timestamp != e->input_frames)
         goto invalid;
   }
   frames = e->frames - e->offset;
   if (frames > *packets)
      frames = *packets;
   if (frames > (~(UInt32)0) / (e->channels * sizeof(int16_t)))
      frames = (~(UInt32)0) / (e->channels * sizeof(int16_t));
   if (!frames || frames > ((uint64_t)-1 - e->input_frames))
      goto invalid;
   data->mBuffers[0].mData = (void*)(e->samples + e->offset * e->channels);
   data->mBuffers[0].mDataByteSize = (UInt32)(frames * e->channels * sizeof(int16_t));
   *packets = (UInt32)frames;
   e->offset += frames;
   e->input_frames += frames;
   return noErr;
invalid:
   e->error = (OSStatus)-1;
   *packets = 0;
   return e->error;
}

record_at_resample_t *record_at_resample_new(record_audio_queue_t *source,
      double input_rate, unsigned output_rate)
{
   AudioStreamBasicDescription input, output, actual;
   record_at_resample_t *e;
   UInt32 value, size = sizeof(actual);
   unsigned channels = record_audio_queue_channels(source);
   if (!(input_rate >= 8000.0 && input_rate <= 192000.0)
         || (output_rate != 44100 && output_rate != 48000)
         || input_rate == output_rate || !channels || channels > 2)
      return NULL;
   e = (record_at_resample_t*)calloc(1, sizeof(*e));
   if (!e)
      return NULL;
   memset(&input, 0, sizeof(input));
   input.mSampleRate = input_rate;
   input.mFormatID = kAudioFormatLinearPCM;
   input.mFormatFlags = kAudioFormatFlagIsSignedInteger
      | kAudioFormatFlagIsPacked | kAudioFormatFlagsNativeEndian;
   input.mChannelsPerFrame = channels;
   input.mFramesPerPacket = 1;
   input.mBitsPerChannel = 16;
   input.mBytesPerFrame = input.mBytesPerPacket = channels * sizeof(int16_t);
   output = input;
   output.mSampleRate = output_rate;
   if (AudioConverterNew(&input, &output, &e->converter) != noErr)
      goto invalid;
   value = kAudioConverterQuality_Max;
   if (AudioConverterSetProperty(e->converter,
            kAudioConverterSampleRateConverterQuality, sizeof(value), &value) != noErr)
      goto invalid;
   value = kConverterPrimeMethod_Normal;
   if (AudioConverterSetProperty(e->converter,
            kAudioConverterPrimeMethod, sizeof(value), &value) != noErr)
      goto invalid;
   memset(&actual, 0, sizeof(actual));
   if (AudioConverterGetProperty(e->converter,
            kAudioConverterCurrentOutputStreamDescription, &size, &actual) != noErr
         || size != sizeof(actual) || actual.mSampleRate != output.mSampleRate
         || actual.mFormatID != output.mFormatID
         || actual.mFormatFlags != output.mFormatFlags
         || actual.mChannelsPerFrame != channels || actual.mFramesPerPacket != 1
         || actual.mBitsPerChannel != 16
         || actual.mBytesPerPacket != output.mBytesPerPacket
         || actual.mBytesPerFrame != output.mBytesPerFrame)
      goto invalid;
   e->source = source;
   e->channels = channels;
   return e;
invalid:
   if (e->converter)
      AudioConverterDispose(e->converter);
   free(e);
   return NULL;
}

int record_at_resample_read(record_at_resample_t *e, int16_t *output,
      size_t capacity, size_t *frames)
{
   AudioBufferList data;
   UInt32 count;
   OSStatus status;
   uint64_t before;
   if (frames)
      *frames = 0;
   if (!e || !output || !frames || !capacity || capacity > 1024
         || e->error || e->cancelled)
      return RECORD_AT_RESAMPLE_ERROR;
   if (e->ended)
      return RECORD_AT_RESAMPLE_END;
   data.mNumberBuffers = 1;
   data.mBuffers[0].mNumberChannels = e->channels;
   data.mBuffers[0].mData = output;
   data.mBuffers[0].mDataByteSize = (UInt32)(capacity * e->channels * sizeof(int16_t));
   count = (UInt32)capacity;
   before = e->input_frames;
   status = AudioConverterFillComplexBuffer(e->converter,
         record_at_resample_input, e, &count, &data, NULL);
   if ((status != noErr && status != SRC_NEED_INPUT) || e->error
         || count > capacity || data.mNumberBuffers != 1
         || data.mBuffers[0].mData != output
         || data.mBuffers[0].mNumberChannels != e->channels
         || data.mBuffers[0].mDataByteSize != count * e->channels * sizeof(int16_t)
         || count > (uint64_t)-1 - e->output_frames)
   {
      e->error = status ? status : (OSStatus)-1;
      return RECORD_AT_RESAMPLE_ERROR;
   }
   *frames = count;
   e->output_frames += count;
   if (count || e->input_frames != before)
      return RECORD_AT_RESAMPLE_PROGRESS;
   if (status == noErr && e->source_end && !e->samples)
   {
      e->ended = true;
      return RECORD_AT_RESAMPLE_END;
   }
   return RECORD_AT_RESAMPLE_BUSY;
}

bool record_at_resample_timing(const record_at_resample_t *e,
      uint64_t *input_frames, uint64_t *output_frames)
{
   if (!e || !input_frames || !output_frames)
      return false;
   *input_frames = e->input_frames;
   *output_frames = e->output_frames;
   return true;
}

bool record_at_resample_cancel(record_at_resample_t *e)
{
   OSStatus status;
   if (!e)
      return false;
   if (e->cancelled)
      return true;
   status = AudioConverterReset(e->converter);
   if (status != noErr)
   {
      e->error = status;
      return false;
   }
   if (e->samples && !record_audio_queue_ack(e->source))
      return false;
   e->samples = NULL;
   e->cancelled = true;
   return true;
}

bool record_at_resample_free(record_at_resample_t *e)
{
   if (!e)
      return true;
   if ((!e->ended && !e->cancelled) || e->samples
         || AudioConverterDispose(e->converter) != noErr)
      return false;
   free(e);
   return true;
}

int32_t record_at_resample_error(const record_at_resample_t *e)
{
   return e ? e->error : -1;
}
#else
record_at_resample_t *record_at_resample_new(record_audio_queue_t *q, double i, unsigned o)
{ (void)q; (void)i; (void)o; return NULL; }
int record_at_resample_read(record_at_resample_t *e, int16_t *p, size_t n, size_t *f)
{ (void)e; (void)p; (void)n; if (f) *f = 0; return RECORD_AT_RESAMPLE_ERROR; }
bool record_at_resample_timing(const record_at_resample_t *e, uint64_t *i, uint64_t *o)
{ (void)e; (void)i; (void)o; return false; }
bool record_at_resample_cancel(record_at_resample_t *e)
{ (void)e; return false; }
bool record_at_resample_free(record_at_resample_t *e)
{ return !e; }
int32_t record_at_resample_error(const record_at_resample_t *e)
{ (void)e; return -1; }
#endif

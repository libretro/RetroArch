/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_at_audio.h"
#include <stdlib.h>
#include <string.h>

#if (defined(__APPLE__) && defined(HAVE_AVF)) || defined(RECORD_AT_AUDIO_TEST)
#include <AudioToolbox/AudioToolbox.h>
#define AT_FRAMES 1024u
#define AT_NEED_INPUT ((OSStatus)0x72616e69)

struct record_at_audio
{
   uint64_t first, next, total, produced;
   AudioConverterRef converter;
   record_at_audio_packet_t packet;
   uint8_t *output;
   size_t pending_frames, pending_offset;
   UInt32 capacity;
   unsigned rate, channels, leading, trailing;
   OSStatus error;
   int loan, pending, held;
   int16_t input[2][AT_FRAMES * 2];
   uint8_t asc[2];
   bool started, finishing, ended, packet_live, cancelled;
};

static int record_at_audio_fail(record_at_audio_t *e, OSStatus error)
{
   if (!e->error)
      e->error = error ? error : (OSStatus)-1;
   return RECORD_AT_AUDIO_ERROR;
}

static OSStatus record_at_audio_read(AudioConverterRef converter, UInt32 *packets,
      AudioBufferList *data, AudioStreamPacketDescription **descriptions, void *userdata)
{
   record_at_audio_t *e = (record_at_audio_t*)userdata;
   size_t frames;
   (void)converter;
   e->held = -1;
   if (descriptions)
      *descriptions = NULL;
   data->mNumberBuffers = 1;
   data->mBuffers[0].mNumberChannels = e->channels;
   data->mBuffers[0].mData = NULL;
   data->mBuffers[0].mDataByteSize = 0;
   if (e->pending < 0)
   {
      *packets = 0;
      return e->finishing ? noErr : AT_NEED_INPUT;
   }
   if (!*packets)
      return (OSStatus)-1;
   frames = e->pending_frames - e->pending_offset;
   if (frames > *packets)
      frames = *packets;
   e->held = e->pending;
   data->mBuffers[0].mData = e->input[e->held] + e->pending_offset * e->channels;
   data->mBuffers[0].mDataByteSize = (UInt32)(frames * e->channels * sizeof(int16_t));
   *packets = (UInt32)frames;
   e->pending_offset += frames;
   if (e->pending_offset == e->pending_frames)
      e->pending = -1;
   return noErr;
}

static bool record_at_audio_priming(record_at_audio_t *e)
{
   AudioConverterPrimeInfo prime;
   UInt32 size = sizeof(prime);
   OSStatus status = AudioConverterGetProperty(e->converter,
         kAudioConverterPrimeInfo, &size, &prime);
   if (status || size != sizeof(prime))
   {
      record_at_audio_fail(e, status);
      return false;
   }
   e->leading = prime.leadingFrames;
   return true;
}

record_at_audio_t *record_at_audio_new(unsigned rate, unsigned channels, unsigned bitrate)
{
   record_at_audio_t *e;
   AudioStreamBasicDescription input, output, actual;
   UInt32 size;
   unsigned frequency;
   OSStatus status;
   if ((rate != 44100 && rate != 48000) || !channels || channels > 2
         || (bitrate != 96000 && bitrate != 128000 && bitrate != 160000 && bitrate != 192000))
      return NULL;
   e = (record_at_audio_t*)calloc(1, sizeof(*e));
   if (!e)
      return NULL;
   e->loan = e->pending = e->held = -1;
   e->rate = rate;
   e->channels = channels;
   memset(&input, 0, sizeof(input));
   memset(&output, 0, sizeof(output));
   input.mSampleRate = output.mSampleRate = rate;
   input.mFormatID = kAudioFormatLinearPCM;
   input.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked
      | kAudioFormatFlagsNativeEndian;
   input.mBytesPerPacket = input.mBytesPerFrame = channels * sizeof(int16_t);
   input.mFramesPerPacket = 1;
   input.mChannelsPerFrame = output.mChannelsPerFrame = channels;
   input.mBitsPerChannel = 16;
   output.mFormatID = kAudioFormatMPEG4AAC;
   output.mFormatFlags = kMPEG4Object_AAC_LC;
   output.mFramesPerPacket = AT_FRAMES;
   status = AudioConverterNew(&input, &output, &e->converter);
   if (status || !e->converter)
      goto failed;
   size = sizeof(actual);
   status = AudioConverterGetProperty(e->converter,
         kAudioConverterCurrentOutputStreamDescription, &size, &actual);
   if (status || size != sizeof(actual) || actual.mSampleRate != rate
         || actual.mChannelsPerFrame != channels || actual.mFormatID != kAudioFormatMPEG4AAC
         || actual.mFormatFlags != kMPEG4Object_AAC_LC || actual.mFramesPerPacket != AT_FRAMES)
      goto failed;
   size = bitrate;
   if (AudioConverterSetProperty(e->converter, kAudioConverterEncodeBitRate, sizeof(size), &size))
      goto failed;
   size = sizeof(e->capacity);
   if (AudioConverterGetProperty(e->converter, kAudioConverterPropertyMaximumOutputPacketSize,
            &size, &e->capacity) || size != sizeof(e->capacity)
         || !e->capacity || e->capacity > 65536 || !record_at_audio_priming(e))
      goto failed;
   e->output = (uint8_t*)malloc(e->capacity);
   if (!e->output)
      goto failed;
   frequency = rate == 44100 ? 4 : 3;
   e->asc[0] = (uint8_t)((2 << 3) | (frequency >> 1));
   e->asc[1] = (uint8_t)((frequency << 7) | (channels << 3));
   return e;
failed:
   if (e->converter)
      AudioConverterDispose(e->converter);
   free(e->output);
   free(e);
   return NULL;
}

int record_at_audio_input(record_at_audio_t *e, int16_t **samples, size_t *frames)
{
   if (!e || !samples || !frames || e->error || e->finishing || e->cancelled)
      return RECORD_AT_AUDIO_ERROR;
   if (e->pending >= 0 || e->packet_live)
      return RECORD_AT_AUDIO_BUSY;
   if (e->loan < 0)
      e->loan = e->held == 0 ? 1 : 0;
   *samples = e->input[e->loan];
   *frames = AT_FRAMES;
   return RECORD_AT_AUDIO_PROGRESS;
}

int record_at_audio_submit(record_at_audio_t *e, size_t frames, uint64_t index)
{
   uint64_t maximum = (~(uint64_t)0) >> 1;
   if (!e || e->error || e->finishing || e->cancelled || e->loan < 0
         || !frames || frames > AT_FRAMES || index > maximum - frames
         || (e->started && index != e->next))
      return RECORD_AT_AUDIO_ERROR;
   if (!e->started)
   {
      e->first = index;
      e->started = true;
   }
   e->next = index + frames;
   e->total += frames;
   e->pending = e->loan;
   e->loan = -1;
   e->pending_frames = frames;
   e->pending_offset = 0;
   return RECORD_AT_AUDIO_PROGRESS;
}

int record_at_audio_pump(record_at_audio_t *e, record_at_audio_packet_t *packet)
{
   AudioBufferList output;
   AudioStreamPacketDescription description;
   UInt32 packets = 1;
   OSStatus status;
   size_t before;
   uint64_t maximum = (~(uint64_t)0) >> 1;
   if (!e || !packet || e->error || e->cancelled)
      return RECORD_AT_AUDIO_ERROR;
   if (e->packet_live)
   {
      *packet = e->packet;
      return RECORD_AT_AUDIO_PACKET;
   }
   if (e->ended)
      return RECORD_AT_AUDIO_END;
   if (!e->started && e->finishing)
   {
      e->ended = true;
      return RECORD_AT_AUDIO_END;
   }
   if (e->loan >= 0 || (!e->started && !e->finishing))
      return RECORD_AT_AUDIO_BUSY;
   before = e->pending_offset;
   memset(&output, 0, sizeof(output));
   memset(&description, 0, sizeof(description));
   output.mNumberBuffers = 1;
   output.mBuffers[0].mNumberChannels = e->channels;
   output.mBuffers[0].mDataByteSize = e->capacity;
   output.mBuffers[0].mData = e->output;
   status = AudioConverterFillComplexBuffer(e->converter, record_at_audio_read, e,
         &packets, &output, &description);
   if ((status && status != AT_NEED_INPUT) || packets > 1 || output.mNumberBuffers != 1
         || output.mBuffers[0].mData != e->output)
      return record_at_audio_fail(e, status);
   if (packets)
   {
      if (!e->started || description.mStartOffset < 0
            || (uint64_t)description.mStartOffset > e->capacity
            || !description.mDataByteSize
            || description.mDataByteSize > e->capacity - (size_t)description.mStartOffset
            || output.mBuffers[0].mDataByteSize > e->capacity
            || (size_t)description.mStartOffset > output.mBuffers[0].mDataByteSize
            || description.mDataByteSize > output.mBuffers[0].mDataByteSize - (size_t)description.mStartOffset
            || (description.mVariableFramesInPacket && description.mVariableFramesInPacket != AT_FRAMES)
            || e->produced > maximum - AT_FRAMES || e->first > maximum - e->produced)
         return record_at_audio_fail(e, (OSStatus)-1);
      e->packet.data = e->output + (size_t)description.mStartOffset;
      e->packet.size = description.mDataByteSize;
      e->packet.timestamp = (int64_t)(e->first + e->produced) - e->leading;
      e->packet.frames = AT_FRAMES;
      e->produced += AT_FRAMES;
      e->packet_live = true;
      *packet = e->packet;
      return RECORD_AT_AUDIO_PACKET;
   }
   if (!status && e->finishing && e->pending < 0 && e->held < 0)
   {
      unsigned leading = e->leading;
      uint64_t padding;
      if (!record_at_audio_priming(e))
         return RECORD_AT_AUDIO_ERROR;
      if (leading != e->leading)
         return record_at_audio_fail(e, (OSStatus)-1);
      if (e->produced < e->leading || e->produced - e->leading < e->total)
         return record_at_audio_fail(e, (OSStatus)-1);
      padding = e->produced - e->leading - e->total;
      if (padding > UINT32_MAX)
         return record_at_audio_fail(e, (OSStatus)-1);
      e->trailing = (unsigned)padding;
      e->ended = true;
      return RECORD_AT_AUDIO_END;
   }
   return e->pending_offset != before ? RECORD_AT_AUDIO_PROGRESS : RECORD_AT_AUDIO_BUSY;
}

bool record_at_audio_release(record_at_audio_t *e)
{
   if (!e || !e->packet_live)
      return false;
   e->packet_live = false;
   memset(&e->packet, 0, sizeof(e->packet));
   return true;
}

bool record_at_audio_config(record_at_audio_t *e, const void **data, size_t *size)
{
   if (!e || !data || !size || e->error || e->cancelled)
      return false;
   *data = e->asc;
   *size = sizeof(e->asc);
   return true;
}

bool record_at_audio_timing(record_at_audio_t *e, unsigned *leading,
      unsigned *trailing, uint64_t *frames)
{
   if (!e || !leading || !trailing || !frames || e->error || e->cancelled)
      return false;
   *leading = e->leading;
   *trailing = e->trailing;
   *frames = e->total;
   return true;
}

int record_at_audio_finish(record_at_audio_t *e)
{
   if (!e || e->error || e->cancelled)
      return RECORD_AT_AUDIO_ERROR;
   if (e->loan >= 0 || e->packet_live)
      return RECORD_AT_AUDIO_BUSY;
   e->finishing = true;
   return RECORD_AT_AUDIO_PROGRESS;
}

bool record_at_audio_cancel(record_at_audio_t *e)
{
   OSStatus status;
   if (!e || e->packet_live)
      return false;
   if (e->cancelled)
      return true;
   status = AudioConverterReset(e->converter);
   if (status)
   {
      record_at_audio_fail(e, status);
      return false;
   }
   e->loan = e->pending = e->held = -1;
   e->cancelled = true;
   return true;
}

bool record_at_audio_free(record_at_audio_t *e)
{
   if (!e)
      return true;
   if (e->loan >= 0 || e->packet_live || (!e->ended && !e->cancelled))
      return false;
   if (AudioConverterDispose(e->converter))
      return false;
   free(e->output);
   free(e);
   return true;
}

int32_t record_at_audio_error(const record_at_audio_t *e)
{
   return e ? (int32_t)e->error : (int32_t)-1;
}
#else
record_at_audio_t *record_at_audio_new(unsigned r, unsigned c, unsigned b)
{ (void)r; (void)c; (void)b; return NULL; }
int record_at_audio_input(record_at_audio_t *e, int16_t **p, size_t *n)
{ (void)e; (void)p; (void)n; return RECORD_AT_AUDIO_ERROR; }
int record_at_audio_submit(record_at_audio_t *e, size_t n, uint64_t p)
{ (void)e; (void)n; (void)p; return RECORD_AT_AUDIO_ERROR; }
int record_at_audio_pump(record_at_audio_t *e, record_at_audio_packet_t *p)
{ (void)e; (void)p; return RECORD_AT_AUDIO_ERROR; }
bool record_at_audio_release(record_at_audio_t *e) { (void)e; return false; }
bool record_at_audio_config(record_at_audio_t *e, const void **p, size_t *n)
{ (void)e; (void)p; (void)n; return false; }
bool record_at_audio_timing(record_at_audio_t *e, unsigned *l, unsigned *t, uint64_t *n)
{ (void)e; (void)l; (void)t; (void)n; return false; }
int record_at_audio_finish(record_at_audio_t *e) { (void)e; return RECORD_AT_AUDIO_ERROR; }
bool record_at_audio_cancel(record_at_audio_t *e) { (void)e; return false; }
bool record_at_audio_free(record_at_audio_t *e) { return e == NULL; }
int32_t record_at_audio_error(const record_at_audio_t *e) { (void)e; return -1; }
#endif

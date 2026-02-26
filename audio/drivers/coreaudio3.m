/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2019      - Stuart Carnie
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#import <Foundation/Foundation.h>
#import <AudioToolbox/AudioToolbox.h>
#import <AVFoundation/AVFoundation.h>
#import <Accelerate/Accelerate.h>

#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>

#include "../audio_driver.h"
#include "../../verbosity.h"

#pragma mark - ringbuffer

#define UNLIKELY(x) __builtin_expect((x), 0)
#define LIKELY(x)   __builtin_expect((x), 1)

typedef struct ringbuffer
{
   float *buffer;
   size_t cap;
   size_t write_ptr;
   size_t read_ptr;
   atomic_int len;
} ringbuffer_t;

typedef ringbuffer_t * ringbuffer_h;

static inline size_t rb_len(ringbuffer_h r)
{
   return atomic_load_explicit(&r->len, memory_order_relaxed);
}

static inline size_t rb_cap(ringbuffer_h r)
{
   return (r->read_ptr + r->cap - r->write_ptr) % r->cap;
}

static inline size_t rb_avail(ringbuffer_h r)
{
   return r->cap - rb_len(r);
}

static inline void rb_advance_write(ringbuffer_h r)
{
   r->write_ptr = (r->write_ptr + 1) % r->cap;
}

static inline void rb_advance_write_n(ringbuffer_h r, size_t n)
{
   r->write_ptr = (r->write_ptr + n) % r->cap;
}

static inline void rb_advance_read(ringbuffer_h r)
{
   r->read_ptr = (r->read_ptr + 1) % r->cap;
}

static inline void rb_len_add(ringbuffer_h r, int n)
{
   atomic_fetch_add(&r->len, n);
}

static inline void rb_len_sub(ringbuffer_h r, int n)
{
   atomic_fetch_sub(&r->len, n);
}

static void rb_init(ringbuffer_h r, size_t cap)
{
   r->buffer     = malloc(cap * sizeof(float));
   r->cap        = cap;
   atomic_init(&r->len, 0);
   r->write_ptr  = 0;
   r->read_ptr   = 0;
}

static void rb_free(ringbuffer_h r)
{
   free(r->buffer);
   memset(r, 0, sizeof(*r));
}

static void rb_write_data(ringbuffer_h r, const float *data, size_t len)
{
   size_t avail       = rb_avail(r);
   size_t n           = MIN(len, avail);
   size_t first_write = n;
   size_t rest_write  = 0;

   if (r->write_ptr + n > r->cap)
   {
      first_write     = r->cap - r->write_ptr;
      rest_write      = n - first_write;
   }

   memcpy(r->buffer + r->write_ptr, data, first_write * sizeof(float));
   memcpy(r->buffer, data + first_write, rest_write * sizeof(float));

   rb_advance_write_n(r, n);
   rb_len_add(r, (int)n);
}

/* Read non-interleaved: separate L and R buffers */
static void rb_read_data_noninterleaved(ringbuffer_h r,
      float *d0, float *d1, size_t len)
{
   size_t need = len * 2;
   size_t have = rb_len(r);
   size_t n    = MIN(have, need);
   size_t i    = 0;

   for (; i < n / 2; i++)
   {
      d0[i] = r->buffer[r->read_ptr];
      rb_advance_read(r);
      d1[i] = r->buffer[r->read_ptr];
      rb_advance_read(r);
   }

   rb_len_sub(r, (int)n);

   /* Fill remainder with silence on underflow */
   for (; i < len; i++)
   {
      d0[i] = 0.0f;
      d1[i] = 0.0f;
   }
}

/* Read interleaved: single buffer with LRLRLR pattern */
static void rb_read_data_interleaved(ringbuffer_h r,
      float *out, size_t frames)
{
   size_t need    = frames * 2; /* samples needed */
   size_t have    = rb_len(r);
   size_t n       = MIN(have, need);
   size_t samples = 0;

   for (; samples < n; samples++)
   {
      out[samples] = r->buffer[r->read_ptr];
      rb_advance_read(r);
   }

   rb_len_sub(r, (int)n);

   /* Fill remainder with silence on underflow */
   for (; samples < need; samples++)
      out[samples] = 0.0f;
}

#pragma mark - CoreAudio3

@interface CoreAudio3 : NSObject {
   ringbuffer_t _rb;
   dispatch_semaphore_t _sema;
   AUAudioUnit *_au;
   size_t _bufferSize;
   BOOL _nonBlock;
   BOOL _interleaved;
   AudioConverterRef _converter;
   unsigned _hwRate;
   unsigned _lastInputRate;
   double _lastRateAdjust;
   float *_convBuffer;
   size_t _convBufferFrames;
   BOOL _converterNeedsReset;
}

@property (nonatomic, readwrite) BOOL nonBlock;
@property (nonatomic, readonly) BOOL paused;
@property (nonatomic, readonly) size_t writeAvailableInBytes;
@property (nonatomic, readonly) size_t bufferSizeInBytes;
@property (nonatomic, readonly) unsigned hardwareRate;

- (instancetype)initWithRate:(NSUInteger)rate
                     latency:(NSUInteger)latency;
- (ssize_t)writeFloat:(const float *)data samples:(size_t)samples;
- (ssize_t)writeRawInt16:(const int16_t *)samples
                  frames:(size_t)frames
               inputRate:(unsigned)inputRate
              rateAdjust:(double)rateAdjust
                  volume:(float)volume;
- (void)start;
- (void)stop;

@end

@implementation CoreAudio3

- (instancetype)initWithRate:(NSUInteger)rate
                     latency:(NSUInteger)latency {
   if (self = [super init])
   {
      NSError *err;
      AUAudioUnit *au;
      AudioComponentDescription desc;
      AVAudioFormat *format, *renderFormat;

      _sema        = dispatch_semaphore_create(0);
      _converter   = NULL;
      _lastInputRate = 0;
      _lastRateAdjust = 1.0;
      _interleaved = NO;
      _converterNeedsReset = NO;

      desc.componentType          = kAudioUnitType_Output;
#if TARGET_OS_IPHONE
      desc.componentSubType       = kAudioUnitSubType_RemoteIO;
#else
      desc.componentSubType       = kAudioUnitSubType_DefaultOutput;
#endif
      desc.componentManufacturer  = kAudioUnitManufacturer_Apple;

      au = [[AUAudioUnit alloc] initWithComponentDescription:desc error:&err];
      if (err != nil)
         return nil;

      format = au.outputBusses[0].format;
      if (format.channelCount < 2)
         return nil;

      /* Get the actual hardware sample rate */
      _hwRate = (unsigned)format.sampleRate;
      if (_hwRate == 0)
         _hwRate = (unsigned)rate;

      /* Use hardware rate for buffer size calculation and output format */
      _bufferSize  = (latency * _hwRate) / 1000;
      _bufferSize *= 2; /* stereo */
      rb_init(&_rb, _bufferSize);

      /* Set up input bus at hardware rate to avoid system resampling.
       * Try non-interleaved first (macOS default), fall back to interleaved (iOS). */
      renderFormat = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:_hwRate channels:2];
      [au.inputBusses[0] setFormat:renderFormat error:&err];
      if (err != nil)
      {
         /* Try interleaved format instead */
         AudioStreamBasicDescription asbd = {0};
         asbd.mSampleRate       = _hwRate;
         asbd.mFormatID         = kAudioFormatLinearPCM;
         asbd.mFormatFlags      = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
         asbd.mBytesPerPacket   = 8;
         asbd.mFramesPerPacket  = 1;
         asbd.mBytesPerFrame    = 8;
         asbd.mChannelsPerFrame = 2;
         asbd.mBitsPerChannel   = 32;

         renderFormat = [[AVAudioFormat alloc] initWithStreamDescription:&asbd];
         [au.inputBusses[0] setFormat:renderFormat error:&err];
         if (err != nil)
            return nil;
         _interleaved = YES;
      }

      ringbuffer_h rb = &_rb;
      __block dispatch_semaphore_t sema = _sema;
      __block BOOL interleaved = _interleaved;

      au.outputProvider = ^AUAudioUnitStatus(AudioUnitRenderActionFlags * actionFlags, const AudioTimeStamp * timestamp, AUAudioFrameCount frameCount, NSInteger inputBusNumber, AudioBufferList * inputData)
      {
         if (interleaved)
            rb_read_data_interleaved(rb, inputData->mBuffers[0].mData, frameCount);
         else
            rb_read_data_noninterleaved(rb, inputData->mBuffers[0].mData, inputData->mBuffers[1].mData, frameCount);
         dispatch_semaphore_signal(sema);
         return 0;
      };

      [au allocateRenderResourcesAndReturnError:&err];
      if (err != nil)
         return nil;

      _au = au;

      /* Allocate converter output buffer (enough for 2048 output frames) */
      _convBufferFrames = 2048;
      _convBuffer = (float *)calloc(_convBufferFrames * 2, sizeof(float));
      if (!_convBuffer)
         return nil;

      RARCH_LOG("[CoreAudio3] Using buffer size of %u bytes: (latency = %u ms, hw rate = %u Hz, %s).\n",
            (unsigned)self.bufferSizeInBytes, (unsigned)latency, _hwRate,
            _interleaved ? "interleaved" : "non-interleaved");

      [self start];
   }
   return self;
}

- (void)dealloc {
   rb_free(&_rb);
   if (_converter)
   {
      AudioConverterDispose(_converter);
      _converter = NULL;
   }
   if (_convBuffer)
   {
      free(_convBuffer);
      _convBuffer = NULL;
   }
}

- (unsigned)hardwareRate {
   return _hwRate;
}

- (BOOL)paused {
   return !_au.running;
}

- (size_t)bufferSizeInBytes {
   return _bufferSize * sizeof(float);
}

- (size_t)writeAvailableInBytes {
   return rb_avail(&_rb) * sizeof(float);
}

- (void)start {
   NSError *err;
   [_au startHardwareAndReturnError:&err];
}

- (void)stop {
   [_au stopHardware];
}

- (ssize_t)writeFloat:(const float *)data samples:(size_t)samples {
   size_t _len = 0;
   while (samples > 0)
   {
      size_t write_avail = rb_avail(&_rb);
      if (write_avail > samples)
         write_avail = samples;

      rb_write_data(&_rb, data, write_avail);
      data    += write_avail;
      _len    += write_avail;
      samples -= write_avail;

      if (_nonBlock)
         break;

      if (write_avail == 0)
      {
         /* If the audio unit has stopped (e.g. audio session interrupted
          * by a phone call), bail out immediately - the callback that
          * drains the buffer will never fire. */
         if (!_au.running)
            break;
         /* Brief timeout as a safety net: if the audio unit stops
          * during the wait, we'll re-check _au.running promptly. */
         dispatch_semaphore_wait(_sema,
               dispatch_time(DISPATCH_TIME_NOW, 100 * NSEC_PER_MSEC));
      }
   }

   return _len;
}

/* Context for AudioConverter callback */
typedef struct {
   const int16_t *data;
   size_t frames_left;
} coreaudio3_converter_ctx_t;

/* AudioConverter input callback */
static OSStatus coreaudio3_converter_cb(
      AudioConverterRef converter,
      UInt32 *ioNumberDataPackets,
      AudioBufferList *ioData,
      AudioStreamPacketDescription **outDataPacketDescription,
      void *inUserData)
{
   coreaudio3_converter_ctx_t *ctx = (coreaudio3_converter_ctx_t *)inUserData;
   UInt32 frames_to_provide;

   if (ctx->frames_left == 0)
   {
      *ioNumberDataPackets = 0;
      return noErr;
   }

   frames_to_provide = *ioNumberDataPackets;
   if (frames_to_provide > ctx->frames_left)
      frames_to_provide = (UInt32)ctx->frames_left;

   ioData->mBuffers[0].mData           = (void *)ctx->data;
   ioData->mBuffers[0].mDataByteSize   = frames_to_provide * 4; /* stereo int16 */
   ioData->mBuffers[0].mNumberChannels = 2;

   ctx->data        += frames_to_provide * 2; /* advance by samples */
   ctx->frames_left -= frames_to_provide;
   *ioNumberDataPackets = frames_to_provide;

   return noErr;
}

- (ssize_t)writeRawInt16:(const int16_t *)samples
                  frames:(size_t)frames
               inputRate:(unsigned)inputRate
              rateAdjust:(double)rateAdjust
                  volume:(float)volume {
   OSStatus err;
   double effectiveRate;
   size_t framesWritten = 0;
   coreaudio3_converter_ctx_t ctx;

   if (frames == 0)
      return 0;

   /* Check if we need to recreate the converter */
   effectiveRate = inputRate / rateAdjust;
   if (_converter == NULL
         || _lastInputRate != inputRate
         || fabs(_lastRateAdjust - rateAdjust) > 0.005)
   {
      AudioStreamBasicDescription inputDesc, outputDesc;

      if (_converter)
      {
         AudioConverterDispose(_converter);
         _converter = NULL;
      }

      /* Input: int16 stereo interleaved at input rate */
      memset(&inputDesc, 0, sizeof(inputDesc));
      inputDesc.mSampleRate       = effectiveRate;
      inputDesc.mFormatID         = kAudioFormatLinearPCM;
      inputDesc.mFormatFlags      = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
      inputDesc.mBytesPerPacket   = 4;  /* 2 channels * 2 bytes */
      inputDesc.mFramesPerPacket  = 1;
      inputDesc.mBytesPerFrame    = 4;
      inputDesc.mChannelsPerFrame = 2;
      inputDesc.mBitsPerChannel   = 16;

      /* Output: float32 stereo interleaved at hardware rate */
      memset(&outputDesc, 0, sizeof(outputDesc));
      outputDesc.mSampleRate       = _hwRate;
      outputDesc.mFormatID         = kAudioFormatLinearPCM;
      outputDesc.mFormatFlags      = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
      outputDesc.mBytesPerPacket   = 8;  /* 2 channels * 4 bytes */
      outputDesc.mFramesPerPacket  = 1;
      outputDesc.mBytesPerFrame    = 8;
      outputDesc.mChannelsPerFrame = 2;
      outputDesc.mBitsPerChannel   = 32;

      err = AudioConverterNew(&inputDesc, &outputDesc, &_converter);
      if (err != noErr)
      {
         RARCH_ERR("[CoreAudio3]: AudioConverterNew failed: %d\n", (int)err);
         return -1;
      }

      /* Set high quality resampling */
      UInt32 quality = kAudioConverterQuality_High;
      AudioConverterSetProperty(_converter,
            kAudioConverterSampleRateConverterQuality,
            sizeof(quality), &quality);

      _lastInputRate = inputRate;
      _lastRateAdjust = rateAdjust;
      _converterNeedsReset = NO;
   }

   /* Set up callback context */
   ctx.data        = samples;
   ctx.frames_left = frames;

   /* Process in chunks that fit our pre-allocated buffer */
   while (ctx.frames_left > 0)
   {
      UInt32 outputFrames = (UInt32)_convBufferFrames;
      AudioBufferList outputBufferList;
      size_t outputSamples;
      ssize_t written;

      outputBufferList.mNumberBuffers = 1;
      outputBufferList.mBuffers[0].mNumberChannels = 2;
      outputBufferList.mBuffers[0].mDataByteSize = outputFrames * 8; /* stereo float */
      outputBufferList.mBuffers[0].mData = _convBuffer;

      err = AudioConverterFillComplexBuffer(_converter,
            coreaudio3_converter_cb, &ctx,
            &outputFrames, &outputBufferList, NULL);

      if (err != noErr && err != 1) /* 1 means end of input */
      {
         RARCH_ERR("[CoreAudio3]: AudioConverterFillComplexBuffer failed: %d\n", (int)err);
         break;
      }

      /* If converter returned 0 output while we have input, it may be stuck
       * in "end of stream" state (tvOS 13/14 issue). Reset and retry once. */
      if (outputFrames == 0)
      {
         if (ctx.frames_left > 0 && !_converterNeedsReset)
         {
            AudioConverterReset(_converter);
            _converterNeedsReset = YES; /* Mark that we've already reset */
            continue; /* Retry with reset converter */
         }
         break;
      }

      _converterNeedsReset = NO; /* Converter is working, clear retry flag */

      /* Apply volume to converted samples */
      outputSamples = outputFrames * 2;
      if (volume != 1.0f)
         vDSP_vsmul(_convBuffer, 1, &volume, _convBuffer, 1, (vDSP_Length)outputSamples);

      /* Write resampled float data to ring buffer */
      written = [self writeFloat:_convBuffer samples:outputSamples];

      if (written > 0)
         framesWritten += written / 2; /* count frames, not samples */

      /* In nonblock mode, stop if we couldn't write everything */
      if (_nonBlock && ctx.frames_left > 0)
         break;
   }

   return (ssize_t)framesWritten;
}

@end

static void coreaudio3_free(void *data)
{
   CoreAudio3 *dev = (__bridge_transfer CoreAudio3 *)data;
   if (dev == nil)
      return;

   [dev stop];
   dev = nil;
}

static void *coreaudio3_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   CoreAudio3 *dev = [[CoreAudio3 alloc] initWithRate:rate
                                              latency:latency];
   if (dev == nil)
      return NULL;

   /* Return hardware rate so RetroArch knows what we're outputting */
   *new_rate = dev.hardwareRate;

   return (__bridge_retained void *)dev;
}

static ssize_t coreaudio3_write(void *data, const void *buf_, size_t len)
{
   CoreAudio3 *dev = (__bridge CoreAudio3 *)data;
   return [dev writeFloat:(const float *)
             buf_ samples:len / sizeof(float)] * sizeof(float);
}

static void coreaudio3_set_nonblock_state(void *data, bool state)
{
   CoreAudio3 *dev = (__bridge CoreAudio3 *)data;
   if (dev == nil)
      return;

   dev.nonBlock = state;
}

static bool coreaudio3_alive(void *data)
{
   CoreAudio3 *dev = (__bridge CoreAudio3 *)data;
   if (dev == nil)
      return false;
   return !dev.paused;
}

static bool coreaudio3_stop(void *data)
{
   CoreAudio3 *dev = (__bridge CoreAudio3 *)data;
   if (dev == nil)
      return false;
   [dev stop];
   return dev.paused;
}

static bool coreaudio3_start(void *data, bool is_shutdown)
{
   CoreAudio3 *dev = (__bridge CoreAudio3 *)data;
   if (dev == nil)
      return false;

   [dev start];
   return !dev.paused;
}

static bool coreaudio3_use_float(void *data) { return true; }

static size_t coreaudio3_write_avail(void *data)
{
   CoreAudio3 *dev = (__bridge CoreAudio3 *)data;
   if (dev == nil)
      return 0;

   return dev.writeAvailableInBytes;
}

static size_t coreaudio3_buffer_size(void *data)
{
   CoreAudio3 *dev = (__bridge CoreAudio3 *)data;
   if (dev == nil)
      return 0;

   return dev.bufferSizeInBytes;
}

static ssize_t coreaudio3_write_raw(void *data, const int16_t *samples,
      size_t frames, unsigned input_rate, double rate_adjust, float volume)
{
   CoreAudio3 *dev = (__bridge CoreAudio3 *)data;
   if (dev == nil)
      return -1;

   return [dev writeRawInt16:samples
                      frames:frames
                   inputRate:input_rate
                  rateAdjust:rate_adjust
                      volume:volume];
}

audio_driver_t audio_coreaudio3 = {
   coreaudio3_init,
   coreaudio3_write,
   coreaudio3_stop,
   coreaudio3_start,
   coreaudio3_alive,
   coreaudio3_set_nonblock_state,
   coreaudio3_free,
   coreaudio3_use_float,
   "coreaudio3",
   NULL, /* device_list_new */
   NULL, /* device_list_free */
   coreaudio3_write_avail,
   coreaudio3_buffer_size,
   coreaudio3_write_raw,
};

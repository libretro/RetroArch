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

#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <memory.h>

#include "../../retroarch.h"

#pragma mark - ringbuffer

typedef struct ringbuffer
{
   float *buffer;
   size_t cap;
   atomic_int len;
   size_t writePtr;
   size_t readPtr;
} ringbuffer_t;

typedef ringbuffer_t * ringbuffer_h;

static inline size_t rb_len(ringbuffer_h r)
{
   return atomic_load_explicit(&r->len, memory_order_relaxed);
}

static inline size_t rb_cap(ringbuffer_h r)
{
   return (r->readPtr + r->cap - r->writePtr) % r->cap;
}

static inline size_t rb_avail(ringbuffer_h r)
{
   return r->cap - rb_len(r);
}

static inline void rb_advance_write(ringbuffer_h r)
{
   r->writePtr = (r->writePtr + 1) % r->cap;
}

static inline void rb_advance_write_n(ringbuffer_h r, size_t n)
{
   r->writePtr = (r->writePtr + n) % r->cap;
}

static inline void rb_advance_read(ringbuffer_h r)
{
   r->readPtr = (r->readPtr + 1) % r->cap;
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
   r->buffer   = malloc(cap * sizeof(float));
   r->cap      = cap;
   atomic_init(&r->len, 0);
   r->writePtr = 0;
   r->readPtr  = 0;
}

static void rb_free(ringbuffer_h r)
{
   free(r->buffer);
   memset(r, 0, sizeof(*r));
}

#define UNLIKELY(x) __builtin_expect((x), 0)
#define LIKELY(x)   __builtin_expect((x), 1)

static void rb_write_data(ringbuffer_h r, const float *data, size_t len)
{
   size_t avail       = rb_avail(r);
   size_t n           = MIN(len, avail);
   size_t first_write = n;
   size_t rest_write  = 0;

   if (r->writePtr + n > r->cap)
   {
      first_write = r->cap - r->writePtr;
      rest_write  = n - first_write;
   }

   memcpy(r->buffer + r->writePtr, data, first_write*sizeof(float));
   memcpy(r->buffer, data + first_write, rest_write*sizeof(float));

   rb_advance_write_n(r, n);
   rb_len_add(r, (int)n);
}

static void rb_read_data(ringbuffer_h r,
      float *d0, float *d1, size_t len)
{
   size_t need    = len * 2;

   do {
      size_t have = rb_len(r);
      size_t n    = MIN(have, need);
      int i       = 0;
      for (; i < n/2; i++)
      {
         d0[i] = r->buffer[r->readPtr];
         rb_advance_read(r);
         d1[i] = r->buffer[r->readPtr];
         rb_advance_read(r);
      }

      need -= n;
      rb_len_sub(r, (int)n);

      if (UNLIKELY(need > 0))
      {
         const float quiet = 0.0f;
         size_t fill;

         /* we got more data */
         if (rb_len(r) > 0)
            continue;

         /* underflow */
         fill = (need/2)*sizeof(float);
         memset_pattern4(&d0[i], &quiet, fill);
         memset_pattern4(&d1[i], &quiet, fill);
      }
   } while (0);
}

#pragma mark - CoreAudio3

static bool g_interrupted;

@interface CoreAudio3 : NSObject {
   ringbuffer_t _rb;
   dispatch_semaphore_t _sema;
   AUAudioUnit *_au;
   size_t _bufferSize;
   BOOL _nonBlock;
}

@property (nonatomic, readwrite) BOOL nonBlock;
@property (nonatomic, readonly) BOOL paused;
@property (nonatomic, readonly) size_t writeAvailableInBytes;
@property (nonatomic, readonly) size_t bufferSizeInBytes;

- (instancetype)initWithRate:(NSUInteger)rate
                     latency:(NSUInteger)latency;
- (ssize_t)writeFloat:(const float *)data samples:(size_t)samples;
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

      _bufferSize  = (latency * rate) / 1000;
      _bufferSize *= 2; /* stereo */
      rb_init(&_rb, _bufferSize);

      desc.componentType          = kAudioUnitType_Output;
      desc.componentSubType       = kAudioUnitSubType_DefaultOutput;
      desc.componentManufacturer  = kAudioUnitManufacturer_Apple;

      au = [[AUAudioUnit alloc] initWithComponentDescription:desc error:&err];
      if (err != nil)
         return nil;

      format = au.outputBusses[0].format;
      if (format.channelCount != 2)
         return nil;

      renderFormat = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:rate channels:2];
      [au.inputBusses[0] setFormat:renderFormat error:&err];
      if (err != nil)
         return nil;

      ringbuffer_h rb = &_rb;
      __block dispatch_semaphore_t sema = _sema;
      au.outputProvider = ^AUAudioUnitStatus(AudioUnitRenderActionFlags * actionFlags, const AudioTimeStamp * timestamp, AUAudioFrameCount frameCount, NSInteger inputBusNumber, AudioBufferList * inputData)
      {
         rb_read_data(rb, inputData->mBuffers[0].mData, inputData->mBuffers[1].mData, frameCount);
         dispatch_semaphore_signal(sema);
         return 0;
      };

      [au allocateRenderResourcesAndReturnError:&err];
      if (err != nil)
         return nil;

      _au = au;

      RARCH_LOG("[CoreAudio3]: Using buffer size of %u bytes: (latency = %u ms)\n", (unsigned)self.bufferSizeInBytes, latency);

      [self start];
   }
   return self;
}

- (void)dealloc {
   rb_free(&_rb);
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
   size_t written = 0;
   while (!g_interrupted && samples > 0)
   {
      size_t write_avail = rb_avail(&_rb);
      if (write_avail > samples)
         write_avail = samples;

      rb_write_data(&_rb, data, write_avail);
      data    += write_avail;
      written += write_avail;
      samples -= write_avail;

      if (_nonBlock)
         break;

      if (write_avail == 0)
         dispatch_semaphore_wait(_sema, DISPATCH_TIME_FOREVER);
   }

   return written;
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

   *new_rate = rate;

   return (__bridge_retained void *)dev;
}

static ssize_t coreaudio3_write(void *data,
      const void *buf_, size_t size)
{
   CoreAudio3 *dev = (__bridge CoreAudio3 *)data;
   return [dev writeFloat:(const float *)
             buf_ samples:size/sizeof(float)] * sizeof(float);
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
      return NO;

   return !dev.paused;
}

static bool coreaudio3_stop(void *data)
{
   CoreAudio3 *dev = (__bridge CoreAudio3 *)data;
   if (dev == nil)
      return NO;

   [dev stop];
   return dev.paused;
}

static bool coreaudio3_start(void *data, bool is_shutdown)
{
   CoreAudio3 *dev = (__bridge CoreAudio3 *)data;
   if (dev == nil)
      return NO;

   [dev start];
   return !dev.paused;
}

static bool coreaudio3_use_float(void *data)
{
   return YES;
}

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
};

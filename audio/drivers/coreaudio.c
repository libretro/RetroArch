/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Chris Moeller
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
#include <stdlib.h>
#include <stdatomic.h>
#include <math.h>

#include <dispatch/dispatch.h>

#if TARGET_OS_IPHONE
#include <AudioToolbox/AudioToolbox.h>
#else
#include <CoreAudio/CoreAudio.h>
#endif

#include <CoreAudio/CoreAudioTypes.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioUnit/AUComponent.h>

/* Nb is defined by AES code included earlier in griffin.c amalgamation.
 * It conflicts with Sparse BLAS headers in Accelerate, so undefine it. */
#undef Nb
#include <Accelerate/Accelerate.h>

#include <boolean.h>
#include <retro_endianness.h>
#include <string/stdstring.h>

#include <defines/cocoa_defines.h>

#include "../audio_driver.h"
#include "../../verbosity.h"

/* Threshold for recreating AudioConverter (0.5% change) */
#define RATE_CHANGE_THRESHOLD 0.005

typedef struct coreaudio
{
   dispatch_semaphore_t sema;

   /* Lock-free ring buffer */
   float *buffer;
   size_t capacity;           /* Power of 2 for fast masking */
   size_t write_ptr;          /* Only touched by main thread */
   size_t read_ptr;           /* Only touched by audio callback */
   atomic_size_t filled;      /* Samples currently in buffer */

#if !HAS_MACOSX_10_12
   ComponentInstance dev;
#else
   AudioComponentInstance dev;
#endif

   /* AudioConverter for hardware-accelerated resampling */
   AudioConverterRef converter;
   unsigned output_rate;       /* Hardware output rate */
   double current_ratio;       /* Current resampling ratio (adjusted input rate) */

   /* Temporary buffer for converter output */
   float *conv_buffer;
   size_t conv_buffer_frames;
   bool converter_needs_reset;

   bool dev_alive;
   bool is_paused;
   bool nonblock;
} coreaudio_t;

/* Context for AudioConverter input callback */
typedef struct
{
   const int16_t *data;
   size_t frames_left;
} converter_callback_ctx_t;

/* Lock-free ring buffer operations */

static inline size_t rb_write_avail(coreaudio_t *dev)
{
   return dev->capacity - atomic_load_explicit(&dev->filled, memory_order_acquire);
}

static inline size_t rb_read_avail(coreaudio_t *dev)
{
   return atomic_load_explicit(&dev->filled, memory_order_acquire);
}

static void rb_write(coreaudio_t *dev, const float *data, size_t count)
{
   size_t first = dev->capacity - dev->write_ptr;
   if (first > count)
      first = count;

   memcpy(dev->buffer + dev->write_ptr, data, first * sizeof(float));
   memcpy(dev->buffer, data + first, (count - first) * sizeof(float));

   dev->write_ptr = (dev->write_ptr + count) & (dev->capacity - 1);
   atomic_fetch_add_explicit(&dev->filled, count, memory_order_release);
}

static void rb_read(coreaudio_t *dev, float *data, size_t count)
{
   size_t first = dev->capacity - dev->read_ptr;
   if (first > count)
      first = count;

   memcpy(data, dev->buffer + dev->read_ptr, first * sizeof(float));
   memcpy(data + first, dev->buffer, (count - first) * sizeof(float));

   dev->read_ptr = (dev->read_ptr + count) & (dev->capacity - 1);
   atomic_fetch_sub_explicit(&dev->filled, count, memory_order_release);
}

/* AudioConverter input callback - provides int16 samples */
static OSStatus converter_input_cb(
      AudioConverterRef converter,
      UInt32 *ioNumberDataPackets,
      AudioBufferList *ioData,
      AudioStreamPacketDescription **outDataPacketDescription,
      void *inUserData)
{
   converter_callback_ctx_t *ctx = (converter_callback_ctx_t *)inUserData;

   if (ctx->frames_left == 0)
   {
      *ioNumberDataPackets = 0;
      return noErr;
   }

   UInt32 frames_to_provide = *ioNumberDataPackets;
   if (frames_to_provide > ctx->frames_left)
      frames_to_provide = (UInt32)ctx->frames_left;

   ioData->mBuffers[0].mData        = (void *)ctx->data;
   ioData->mBuffers[0].mDataByteSize = frames_to_provide * 4; /* stereo int16 */
   ioData->mBuffers[0].mNumberChannels = 2;

   ctx->data        += frames_to_provide * 2; /* advance by samples */
   ctx->frames_left -= frames_to_provide;
   *ioNumberDataPackets = frames_to_provide;

   return noErr;
}

/* Create or update AudioConverter for the given effective input rate */
static bool coreaudio_update_converter(coreaudio_t *dev, double effective_input_rate)
{
   AudioStreamBasicDescription input_desc  = {0};
   AudioStreamBasicDescription output_desc = {0};
   OSStatus err;

   /* Check if we need to recreate the converter */
   if (dev->converter)
   {
      double ratio_change = fabs(effective_input_rate - dev->current_ratio) / dev->current_ratio;
      if (ratio_change < RATE_CHANGE_THRESHOLD)
         return true; /* No significant change, keep existing converter */

      AudioConverterDispose(dev->converter);
      dev->converter = NULL;
   }

   /* Input format: int16 stereo at effective input rate */
   input_desc.mSampleRate       = effective_input_rate;
   input_desc.mFormatID         = kAudioFormatLinearPCM;
   input_desc.mFormatFlags      = kLinearPCMFormatFlagIsSignedInteger
                                | kAudioFormatFlagIsPacked;
   input_desc.mBytesPerPacket   = 4;
   input_desc.mFramesPerPacket  = 1;
   input_desc.mBytesPerFrame    = 4;
   input_desc.mChannelsPerFrame = 2;
   input_desc.mBitsPerChannel   = 16;

   /* Output format: float32 stereo at hardware output rate */
   output_desc.mSampleRate       = dev->output_rate;
   output_desc.mFormatID         = kAudioFormatLinearPCM;
   output_desc.mFormatFlags      = kAudioFormatFlagIsFloat
                                 | kAudioFormatFlagIsPacked;
   output_desc.mBytesPerPacket   = 8;
   output_desc.mFramesPerPacket  = 1;
   output_desc.mBytesPerFrame    = 8;
   output_desc.mChannelsPerFrame = 2;
   output_desc.mBitsPerChannel   = 32;

   err = AudioConverterNew(&input_desc, &output_desc, &dev->converter);
   if (err != noErr)
   {
      RARCH_ERR("[CoreAudio] Failed to create AudioConverter: %d\n", (int)err);
      return false;
   }

   dev->current_ratio = effective_input_rate;
   dev->converter_needs_reset = false;

   /* Set high quality resampling */
   UInt32 quality = kAudioConverterQuality_High;
   AudioConverterSetProperty(dev->converter,
         kAudioConverterSampleRateConverterQuality,
         sizeof(quality), &quality);

   return true;
}

static void coreaudio_free(void *data)
{
   coreaudio_t *dev = (coreaudio_t*)data;

   if (!dev)
      return;

   if (dev->dev_alive)
   {
      AudioOutputUnitStop(dev->dev);
#if !HAS_MACOSX_10_12
      CloseComponent(dev->dev);
#else
      AudioComponentInstanceDispose(dev->dev);
#endif
   }

   if (dev->converter)
      AudioConverterDispose(dev->converter);

   if (dev->conv_buffer)
      free(dev->conv_buffer);

   if (dev->buffer)
      free(dev->buffer);

   if (dev->sema)
      dispatch_release(dev->sema);

   free(dev);
}

static OSStatus coreaudio_audio_write_cb(void *userdata,
      AudioUnitRenderActionFlags *action_flags,
      const AudioTimeStamp *time_stamp, UInt32 bus_number,
      UInt32 number_frames, AudioBufferList *io_data)
{
   coreaudio_t *dev = (coreaudio_t*)userdata;
   float *outbuf;
   size_t frames_needed;
   size_t avail;

   (void)time_stamp;
   (void)bus_number;
   (void)number_frames;

   if (!io_data || io_data->mNumberBuffers != 1)
      return noErr;

   outbuf        = (float *)io_data->mBuffers[0].mData;
   frames_needed = io_data->mBuffers[0].mDataByteSize / sizeof(float);
   avail         = rb_read_avail(dev);

   if (avail < frames_needed)
   {
      /* Underrun: read what we have, fill rest with silence */
      *action_flags = kAudioUnitRenderAction_OutputIsSilence;
      if (avail > 0)
         rb_read(dev, outbuf, avail);
      memset(outbuf + avail, 0, (frames_needed - avail) * sizeof(float));
   }
   else
      rb_read(dev, outbuf, frames_needed);

   /* Wake writer if it might be waiting */
   dispatch_semaphore_signal(dev->sema);

   return noErr;
}

#if !TARGET_OS_IPHONE
static void coreaudio_choose_output_device(coreaudio_t *dev, const char* device)
{
   int i;
   UInt32 device_count;
   AudioObjectPropertyAddress propaddr;
   AudioDeviceID *devices = NULL;
   UInt32 size = 0;

   propaddr.mSelector = kAudioHardwarePropertyDevices;
#if HAS_MACOSX_10_12
   propaddr.mScope    = kAudioObjectPropertyScopeOutput;
#else
   propaddr.mScope    = kAudioObjectPropertyScopeGlobal;
#endif
   propaddr.mElement  = kAudioObjectPropertyElementMaster;

   if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
            &propaddr, 0, 0, &size) != noErr)
      return;

   device_count = size / sizeof(AudioDeviceID);
   devices      = (AudioDeviceID*)malloc(size);

   if (devices && AudioObjectGetPropertyData(kAudioObjectSystemObject,
            &propaddr, 0, 0, &size, devices) == noErr)
   {
#if HAS_MACOSX_10_12
#else
      propaddr.mScope    = kAudioDevicePropertyScopeOutput;
#endif
      propaddr.mSelector = kAudioDevicePropertyDeviceName;

      for (i = 0; i < (int)device_count; i ++)
      {
         char device_name[1024];
         device_name[0] = 0;
         size           = 1024;

         if (AudioObjectGetPropertyData(devices[i],
                  &propaddr, 0, 0, &size, device_name) == noErr
               && string_is_equal(device_name, device))
         {
            AudioUnitSetProperty(dev->dev, kAudioOutputUnitProperty_CurrentDevice,
                  kAudioUnitScope_Global, 0, &devices[i], sizeof(AudioDeviceID));
            break;
         }
      }
   }

   free(devices);
}
#endif

/* Query the actual hardware sample rate */
static unsigned coreaudio_get_hardware_sample_rate(
#if !HAS_MACOSX_10_12
      ComponentInstance dev
#else
      AudioComponentInstance dev
#endif
      )
{
   AudioStreamBasicDescription hw_desc;
   UInt32 size = sizeof(hw_desc);

#if TARGET_OS_IPHONE
   /* On iOS, query the output scope of RemoteIO to get hardware rate */
   if (AudioUnitGetProperty(dev, kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Output, 0, &hw_desc, &size) == noErr)
   {
      if (hw_desc.mSampleRate > 0)
         return (unsigned)hw_desc.mSampleRate;
   }
#else
   /* On macOS, query the current output device's nominal sample rate */
   {
      AudioDeviceID device_id = 0;
      UInt32 device_size = sizeof(device_id);
      AudioObjectPropertyAddress prop;
      Float64 nominal_rate = 0;

      /* Get the current device from the AudioUnit */
      if (AudioUnitGetProperty(dev, kAudioOutputUnitProperty_CurrentDevice,
               kAudioUnitScope_Global, 0, &device_id, &device_size) == noErr
            && device_id != 0)
      {
         prop.mSelector = kAudioDevicePropertyNominalSampleRate;
         prop.mScope    = kAudioObjectPropertyScopeGlobal;
         prop.mElement  = kAudioObjectPropertyElementMaster;
         size = sizeof(nominal_rate);

         if (AudioObjectGetPropertyData(device_id, &prop, 0, NULL,
                  &size, &nominal_rate) == noErr && nominal_rate > 0)
            return (unsigned)nominal_rate;
      }
   }
#endif

   return 0; /* Failed to determine, caller should use fallback */
}

static void *coreaudio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   size_t buffer_samples;
   UInt32 i_size;
   AudioStreamBasicDescription real_desc;
#if !HAS_MACOSX_10_12
   Component comp;
#else
   AudioComponent comp;
#endif
#ifndef TARGET_OS_IPHONE
   AudioChannelLayout layout               = {0};
#endif
   AURenderCallbackStruct cb               = {0};
   AudioStreamBasicDescription stream_desc = {0};
#if !HAS_MACOSX_10_12
   ComponentDescription desc               = {0};
#else
   AudioComponentDescription desc          = {0};
#endif
   coreaudio_t *dev                        = (coreaudio_t*)
      calloc(1, sizeof(*dev));
   if (!dev)
      return NULL;

   dev->sema = dispatch_semaphore_create(0);

   /* Create AudioComponent */
   desc.componentType         = kAudioUnitType_Output;
#if TARGET_OS_IPHONE
   desc.componentSubType      = kAudioUnitSubType_RemoteIO;
#else
   desc.componentSubType      = kAudioUnitSubType_HALOutput;
#endif
   desc.componentManufacturer = kAudioUnitManufacturer_Apple;

#if !HAS_MACOSX_10_12
   if (!(comp = FindNextComponent(NULL, &desc)))
      goto error;
#else
   if (!(comp = AudioComponentFindNext(NULL, &desc)))
      goto error;
#endif

#if !HAS_MACOSX_10_12
   if ((OpenAComponent(comp, &dev->dev) != noErr))
      goto error;
#else
   if ((AudioComponentInstanceNew(comp, &dev->dev) != noErr))
      goto error;
#endif

#if !TARGET_OS_IPHONE
   if (device)
      coreaudio_choose_output_device(dev, device);
#endif

   dev->dev_alive                = true;

   /* Query actual hardware sample rate to avoid double resampling */
   {
      unsigned hw_rate = coreaudio_get_hardware_sample_rate(dev->dev);
      if (hw_rate > 0 && hw_rate != rate)
      {
         RARCH_LOG("[CoreAudio] Hardware sample rate is %u Hz (requested %u Hz), using hardware rate.\n",
               hw_rate, rate);
         rate = hw_rate;
      }
   }

   /* Set audio format */
   stream_desc.mSampleRate       = rate;
   stream_desc.mBitsPerChannel   = sizeof(float) * CHAR_BIT;
   stream_desc.mChannelsPerFrame = 2;
   stream_desc.mBytesPerPacket   = 2 * sizeof(float);
   stream_desc.mBytesPerFrame    = 2 * sizeof(float);
   stream_desc.mFramesPerPacket  = 1;
   stream_desc.mFormatID         = kAudioFormatLinearPCM;
   stream_desc.mFormatFlags      = kAudioFormatFlagIsFloat
                                 | kAudioFormatFlagIsPacked;

   if (!is_little_endian())
      stream_desc.mFormatFlags  |= kAudioFormatFlagIsBigEndian;

   if (AudioUnitSetProperty(dev->dev, kAudioUnitProperty_StreamFormat,
         kAudioUnitScope_Input, 0, &stream_desc, sizeof(stream_desc)) != noErr)
      goto error;

   /* Check returned audio format. */
   i_size = sizeof(real_desc);
   if (AudioUnitGetProperty(dev->dev, kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Input, 0, &real_desc, &i_size) != noErr)
      goto error;

   if (real_desc.mChannelsPerFrame != stream_desc.mChannelsPerFrame)
      goto error;
   if (real_desc.mBitsPerChannel != stream_desc.mBitsPerChannel)
      goto error;
   if (real_desc.mFormatFlags != stream_desc.mFormatFlags)
      goto error;
   if (real_desc.mFormatID != stream_desc.mFormatID)
      goto error;

   RARCH_LOG("[CoreAudio] Using output sample rate of %.1f Hz.\n",
         (float)real_desc.mSampleRate);
   *new_rate = real_desc.mSampleRate;
   dev->output_rate = *new_rate;

   /* Allocate converter output buffer (enough for 2048 output frames) */
   dev->conv_buffer_frames = 2048;
   dev->conv_buffer = (float *)calloc(dev->conv_buffer_frames * 2, sizeof(float));
   if (!dev->conv_buffer)
      goto error;

   /* Set channel layout (fails on iOS). */
#ifndef TARGET_OS_IPHONE
   layout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
   if (AudioUnitSetProperty(dev->dev, kAudioUnitProperty_AudioChannelLayout,
         kAudioUnitScope_Input, 0, &layout, sizeof(layout)) != noErr)
      goto error;
#endif

   /* Set callbacks and finish up. */
   cb.inputProc       = coreaudio_audio_write_cb;
   cb.inputProcRefCon = dev;

   if (AudioUnitSetProperty(dev->dev, kAudioUnitProperty_SetRenderCallback,
         kAudioUnitScope_Input, 0, &cb, sizeof(cb)) != noErr)
      goto error;

   if (AudioUnitInitialize(dev->dev) != noErr)
      goto error;

   /* Enforce minimum latency to prevent buffer issues */
   if (latency < 8)
      latency = 8;

   /* Calculate buffer size in samples (stereo) */
   buffer_samples   = (latency * (*new_rate)) / 1000;
   buffer_samples  *= 2;  /* stereo */

   /* Round up to next power of 2 for fast modulo via masking */
   dev->capacity = 1;
   while (dev->capacity < buffer_samples)
      dev->capacity <<= 1;

   dev->buffer = (float *)calloc(dev->capacity, sizeof(float));
   if (!dev->buffer)
      goto error;

   atomic_init(&dev->filled, 0);
   dev->write_ptr = 0;
   dev->read_ptr  = 0;

   RARCH_LOG("[CoreAudio] Buffer: %u samples (%u bytes, %.1f ms).\n",
         (unsigned)dev->capacity,
         (unsigned)(dev->capacity * sizeof(float)),
         (float)dev->capacity * 1000.0f / (*new_rate) / 2.0f);

   if (AudioOutputUnitStart(dev->dev) != noErr)
      goto error;

   return dev;

error:
   RARCH_ERR("[CoreAudio] Failed to initialize driver.\n");
   coreaudio_free(dev);
   return NULL;
}

static ssize_t coreaudio_write(void *data, const void *buf_, size_t len)
{
   coreaudio_t *dev   = (coreaudio_t*)data;
   const float *buf   = (const float *)buf_;
   size_t samples     = len / sizeof(float);
   size_t written     = 0;

   while (!dev->is_paused && samples > 0)
   {
      size_t avail    = rb_write_avail(dev);
      size_t to_write = (avail < samples) ? avail : samples;

      if (to_write > 0)
      {
         rb_write(dev, buf, to_write);
         buf     += to_write;
         written += to_write;
         samples -= to_write;
      }

      if (dev->nonblock)
         break;

      if (samples > 0)
      {
         /* If the audio unit has stopped (e.g. audio session interrupted
          * by a phone call), bail out - the callback will never drain. */
         UInt32 running = 0;
         UInt32 size    = sizeof(running);
         if (AudioUnitGetProperty(dev->dev,
                  kAudioOutputUnitProperty_IsRunning,
                  kAudioUnitScope_Global, 0,
                  &running, &size) == noErr && !running)
            break;
         /* Brief timeout as safety net for the race where the unit
          * stops during the wait; we'll re-check on the next iteration. */
         dispatch_semaphore_wait(dev->sema,
               dispatch_time(DISPATCH_TIME_NOW, 100 * NSEC_PER_MSEC));
      }
   }

   return written * sizeof(float);
}

/* Write raw int16 samples with hardware-accelerated resampling */
static ssize_t coreaudio_write_raw(void *data, const int16_t *samples,
      size_t frames, unsigned input_rate, double rate_adjust, float volume)
{
   coreaudio_t *dev = (coreaudio_t*)data;
   double effective_rate;
   size_t frames_written = 0;
   converter_callback_ctx_t ctx;
   AudioBufferList output_buffer;
   OSStatus err;

   if (!dev || dev->is_paused || frames == 0)
      return 0;

   /* Calculate effective input rate with rate adjustment.
    * rate_adjust > 1.0 means we need to speed up (more output for same input),
    * so we lower the effective input rate to produce more output frames. */
   effective_rate = (double)input_rate / rate_adjust;

   /* Update converter if needed */
   if (!coreaudio_update_converter(dev, effective_rate))
      return -1;

   /* Set up callback context */
   ctx.data        = samples;
   ctx.frames_left = frames;

   /* Process in chunks that fit our conv_buffer */
   while (ctx.frames_left > 0)
   {
      UInt32 output_frames = (UInt32)dev->conv_buffer_frames;

      output_buffer.mNumberBuffers = 1;
      output_buffer.mBuffers[0].mNumberChannels = 2;
      output_buffer.mBuffers[0].mDataByteSize   = output_frames * 8; /* stereo float */
      output_buffer.mBuffers[0].mData           = dev->conv_buffer;

      err = AudioConverterFillComplexBuffer(dev->converter,
            converter_input_cb, &ctx,
            &output_frames, &output_buffer, NULL);

      if (err != noErr && err != 1)  /* 1 means end of input, which is ok */
      {
         RARCH_ERR("[CoreAudio] AudioConverterFillComplexBuffer failed: %d\n", (int)err);
         break;
      }

      /* If converter returned 0 output while we have input, it may be stuck
       * in "end of stream" state (tvOS 13/14 issue). Reset and retry once. */
      if (output_frames == 0)
      {
         if (ctx.frames_left > 0 && !dev->converter_needs_reset)
         {
            AudioConverterReset(dev->converter);
            dev->converter_needs_reset = true;
            continue;
         }
         break;
      }

      dev->converter_needs_reset = false;

      /* Apply volume to converted samples */
      if (volume != 1.0f)
         vDSP_vsmul(dev->conv_buffer, 1, &volume,
               dev->conv_buffer, 1, (vDSP_Length)(output_frames * 2));

      /* Write converted samples to ring buffer */
      {
         float *out_ptr     = dev->conv_buffer;
         size_t out_samples = output_frames * 2; /* stereo */

         while (!dev->is_paused && out_samples > 0)
         {
            size_t avail    = rb_write_avail(dev);
            size_t to_write = (avail < out_samples) ? avail : out_samples;

            if (to_write > 0)
            {
               rb_write(dev, out_ptr, to_write);
               out_ptr       += to_write;
               out_samples   -= to_write;
               frames_written += to_write / 2; /* count frames, not samples */
            }

            if (dev->nonblock)
               break;

            if (out_samples > 0)
            {
               UInt32 running = 0;
               UInt32 sz      = sizeof(running);
               if (AudioUnitGetProperty(dev->dev,
                        kAudioOutputUnitProperty_IsRunning,
                        kAudioUnitScope_Global, 0,
                        &running, &sz) == noErr && !running)
                  break;
               dispatch_semaphore_wait(dev->sema,
                     dispatch_time(DISPATCH_TIME_NOW, 100 * NSEC_PER_MSEC));
            }
         }
      }

      /* If we couldn't write all samples in nonblock mode, stop */
      if (dev->nonblock && ctx.frames_left > 0)
         break;
   }

   return (ssize_t)frames_written;
}

static void coreaudio_set_nonblock_state(void *data, bool state)
{
   coreaudio_t *dev = (coreaudio_t*)data;
   if (dev)
      dev->nonblock = state;
}

static bool coreaudio_alive(void *data)
{
   coreaudio_t *dev = (coreaudio_t*)data;
   if (!dev)
      return false;
   return !dev->is_paused;
}

static bool coreaudio_stop(void *data)
{
   coreaudio_t *dev = (coreaudio_t*)data;
   if (dev)
   {
      dev->is_paused = (AudioOutputUnitStop(dev->dev) == noErr) ? true : false;
      if (dev->is_paused)
         return true;
   }
   return false;
}

static bool coreaudio_start(void *data, bool is_shutdown)
{
   coreaudio_t *dev = (coreaudio_t*)data;
   if (dev)
   {
      dev->is_paused = (AudioOutputUnitStart(dev->dev) == noErr) ? false : true;
      if (!dev->is_paused)
         return true;
   }
   return false;
}

static bool coreaudio_use_float(void *data) { return true; }

static size_t coreaudio_write_avail(void *data)
{
   coreaudio_t *dev = (coreaudio_t*)data;
   return rb_write_avail(dev) * sizeof(float);
}

static size_t coreaudio_buffer_size(void *data)
{
   coreaudio_t *dev = (coreaudio_t*)data;
   return dev->capacity * sizeof(float);
}

/* TODO/FIXME - implement */
static void *coreaudio_device_list_new(void *data) { return NULL; }
static void coreaudio_device_list_free(void *data, void *array_list_data) { }

audio_driver_t audio_coreaudio = {
   coreaudio_init,
   coreaudio_write,
   coreaudio_stop,
   coreaudio_start,
   coreaudio_alive,
   coreaudio_set_nonblock_state,
   coreaudio_free,
   coreaudio_use_float,
   "coreaudio",
   coreaudio_device_list_new,
   coreaudio_device_list_free,
   coreaudio_write_avail,
   coreaudio_buffer_size,
   coreaudio_write_raw
};

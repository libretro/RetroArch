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

#include <dispatch/dispatch.h>

#if TARGET_OS_IPHONE
#include <AudioToolbox/AudioToolbox.h>
#else
#include <CoreAudio/CoreAudio.h>
#endif

#include <CoreAudio/CoreAudioTypes.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioUnit/AUComponent.h>

#include <boolean.h>
#include <retro_endianness.h>
#include <string/stdstring.h>

#include <defines/cocoa_defines.h>

#include "../audio_driver.h"
#include "../../verbosity.h"

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
   bool dev_alive;
   bool is_paused;
   bool nonblock;
} coreaudio_t;

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
         /* Buffer full, wait for audio callback to drain some.
          * Use a timeout as a safety net in case audio stalls. */
         dispatch_time_t timeout = dispatch_time(
               DISPATCH_TIME_NOW, 500 * NSEC_PER_MSEC);
         if (dispatch_semaphore_wait(dev->sema, timeout) != 0)
            break; /* Timeout - audio might be stalled */
      }
   }

   return written * sizeof(float);
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
};

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

#if TARGET_OS_IPHONE
#include <AudioToolbox/AudioToolbox.h>
#else
#include <CoreAudio/CoreAudio.h>
#endif

#include <CoreAudio/CoreAudioTypes.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioUnit/AUComponent.h>

#include <boolean.h>
#include <queues/fifo_queue.h>
#include <rthreads/rthreads.h>
#include <retro_endianness.h>
#include <string/stdstring.h>

#include "../../retroarch.h"
#include "../../verbosity.h"

typedef struct coreaudio
{
   slock_t *lock;
   scond_t *cond;

#if (defined(__MACH__) && (defined(__ppc__) || defined(__ppc64__)))
   ComponentInstance dev;
#else
   AudioComponentInstance dev;
#endif
   bool dev_alive;
   bool is_paused;

   fifo_buffer_t *buffer;
   bool nonblock;
   size_t buffer_size;
} coreaudio_t;

#if TARGET_OS_IOS
static bool g_interrupted;
#endif

static void coreaudio_free(void *data)
{
   coreaudio_t *dev = (coreaudio_t*)data;

   if (!dev)
      return;

   if (dev->dev_alive)
   {
      AudioOutputUnitStop(dev->dev);
#if (defined(__MACH__) && (defined(__ppc__) || defined(__ppc64__)))
      CloseComponent(dev->dev);
#else
      AudioComponentInstanceDispose(dev->dev);
#endif
   }

   if (dev->buffer)
      fifo_free(dev->buffer);

   slock_free(dev->lock);
   scond_free(dev->cond);

   free(dev);
}

static OSStatus audio_write_cb(void *userdata,
      AudioUnitRenderActionFlags *action_flags,
      const AudioTimeStamp *time_stamp, UInt32 bus_number,
      UInt32 number_frames, AudioBufferList *io_data)
{
   unsigned write_avail;
   void     *outbuf = NULL;
   coreaudio_t *dev = (coreaudio_t*)userdata;

   (void)time_stamp;
   (void)bus_number;
   (void)number_frames;

   if (!io_data || io_data->mNumberBuffers != 1)
      return noErr;

   write_avail = io_data->mBuffers[0].mDataByteSize;
   outbuf      = io_data->mBuffers[0].mData;

   slock_lock(dev->lock);

   if (fifo_read_avail(dev->buffer) < write_avail)
   {
      *action_flags = kAudioUnitRenderAction_OutputIsSilence;

      /* Seems to be needed. */
      memset(outbuf, 0, write_avail);

      slock_unlock(dev->lock);

      /* Technically possible to deadlock without. */
      scond_signal(dev->cond);
      return noErr;
   }

   fifo_read(dev->buffer, outbuf, write_avail);
   slock_unlock(dev->lock);
   scond_signal(dev->cond);
   return noErr;
}

#if TARGET_OS_IPHONE
static void coreaudio_interrupt_listener(void *data, UInt32 interrupt_state)
{
    (void)data;
#if TARGET_OS_IOS
    g_interrupted = (interrupt_state == kAudioSessionBeginInterruption);
#endif
}
#else
static void choose_output_device(coreaudio_t *dev, const char* device)
{
   unsigned i;
   UInt32 deviceCount;
   AudioObjectPropertyAddress propaddr;
   AudioDeviceID *devices              = NULL;
   UInt32 size                         = 0;

   propaddr.mSelector = kAudioHardwarePropertyDevices;
#if MAC_OS_X_VERSION_10_12
   propaddr.mScope    = kAudioObjectPropertyScopeOutput;
#else
   propaddr.mScope    = kAudioObjectPropertyScopeGlobal;
#endif
   propaddr.mElement  = kAudioObjectPropertyElementMaster;

   if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
            &propaddr, 0, 0, &size) != noErr)
      return;

   deviceCount = size / sizeof(AudioDeviceID);
   devices     = (AudioDeviceID*)malloc(size);

   if (!devices || AudioObjectGetPropertyData(kAudioObjectSystemObject,
            &propaddr, 0, 0, &size, devices) != noErr)
      goto done;

#if MAC_OS_X_VERSION_10_12
#else
   propaddr.mScope    = kAudioDevicePropertyScopeOutput;
#endif
   propaddr.mSelector = kAudioDevicePropertyDeviceName;

   for (i = 0; i < deviceCount; i ++)
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
         goto done;
      }
   }

done:
   free(devices);
}
#endif

static void *coreaudio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   size_t fifo_size;
   UInt32 i_size;
   AudioStreamBasicDescription real_desc;
#if (defined(__MACH__) && (defined(__ppc__) || defined(__ppc64__)))
   Component comp;
#else
   AudioComponent comp;
#endif
#ifndef TARGET_OS_IPHONE
   AudioChannelLayout layout               = {0};
#endif
   AURenderCallbackStruct cb               = {0};
   AudioStreamBasicDescription stream_desc = {0};
   bool component_unavailable              = false;
   static bool session_initialized         = false;
#if (defined(__MACH__) && (defined(__ppc__) || defined(__ppc64__)))
   ComponentDescription desc               = {0};
#else
   AudioComponentDescription desc          = {0};
#endif
   coreaudio_t *dev                        = (coreaudio_t*)
      calloc(1, sizeof(*dev));
   if (!dev)
      return NULL;

   (void)session_initialized;
   (void)device;

   dev->lock = slock_new();
   dev->cond = scond_new();

#if TARGET_OS_IOS
   if (!session_initialized)
   {
      session_initialized = true;
      AudioSessionInitialize(0, 0, coreaudio_interrupt_listener, 0);
      AudioSessionSetActive(true);
   }
#endif

   /* Create AudioComponent */
   desc.componentType = kAudioUnitType_Output;
#if TARGET_OS_IPHONE
   desc.componentSubType = kAudioUnitSubType_RemoteIO;
#else
   desc.componentSubType = kAudioUnitSubType_HALOutput;
#endif
   desc.componentManufacturer = kAudioUnitManufacturer_Apple;

#if (defined(__MACH__) && (defined(__ppc__) || defined(__ppc64__)))
   comp = FindNextComponent(NULL, &desc);
#else
   comp = AudioComponentFindNext(NULL, &desc);
#endif
   if (comp == NULL)
      goto error;

#if (defined(__MACH__) && (defined(__ppc__) || defined(__ppc64__)))
   component_unavailable = (OpenAComponent(comp, &dev->dev) != noErr);
#else
   component_unavailable = (AudioComponentInstanceNew(comp, &dev->dev) != noErr);
#endif

   if (component_unavailable)
      goto error;

#if !TARGET_OS_IPHONE
   if (device)
      choose_output_device(dev, device);
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
   stream_desc.mFormatFlags      = kAudioFormatFlagIsFloat |
      kAudioFormatFlagIsPacked | (is_little_endian() ?
            0 : kAudioFormatFlagIsBigEndian);

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

   RARCH_LOG("[CoreAudio]: Using output sample rate of %.1f Hz\n",
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
   cb.inputProc = audio_write_cb;
   cb.inputProcRefCon = dev;

   if (AudioUnitSetProperty(dev->dev, kAudioUnitProperty_SetRenderCallback,
         kAudioUnitScope_Input, 0, &cb, sizeof(cb)) != noErr)
      goto error;

   if (AudioUnitInitialize(dev->dev) != noErr)
      goto error;

   fifo_size         = (latency * (*new_rate)) / 1000;
   fifo_size        *= 2 * sizeof(float);
   dev->buffer_size  = fifo_size;

   dev->buffer       = fifo_new(fifo_size);
   if (!dev->buffer)
      goto error;

   RARCH_LOG("[CoreAudio]: Using buffer size of %u bytes: (latency = %u ms)\n",
         (unsigned)fifo_size, latency);

   if (AudioOutputUnitStart(dev->dev) != noErr)
      goto error;

   return dev;

error:
   RARCH_ERR("[CoreAudio]: Failed to initialize driver ...\n");
   coreaudio_free(dev);
   return NULL;
}

static ssize_t coreaudio_write(void *data, const void *buf_, size_t size)
{
   coreaudio_t *dev   = (coreaudio_t*)data;
   const uint8_t *buf = (const uint8_t*)buf_;
   size_t written     = 0;

#if TARGET_OS_IOS
   while (!g_interrupted && size > 0)
#else
   while (size > 0)
#endif
   {
      size_t write_avail;

      slock_lock(dev->lock);

      write_avail = fifo_write_avail(dev->buffer);
      if (write_avail > size)
         write_avail = size;

      fifo_write(dev->buffer, buf, write_avail);
      buf += write_avail;
      written += write_avail;
      size -= write_avail;

      if (dev->nonblock)
      {
         slock_unlock(dev->lock);
         break;
      }

#if TARGET_OS_IPHONE
      if (write_avail == 0 && !scond_wait_timeout(
               dev->cond, dev->lock, 3000000))
         g_interrupted = true;
#else
      if (write_avail == 0)
         scond_wait(dev->cond, dev->lock);
#endif
      slock_unlock(dev->lock);
   }

   return written;
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
   if (!dev)
      return false;
   dev->is_paused = (AudioOutputUnitStop(dev->dev) == noErr) ? true : false;
   return dev->is_paused ? true : false;
}

static bool coreaudio_start(void *data, bool is_shutdown)
{
   coreaudio_t *dev = (coreaudio_t*)data;
   if (!dev)
      return false;
   dev->is_paused = (AudioOutputUnitStart(dev->dev) == noErr) ? false : true;
   return dev->is_paused ? false : true;
}

static bool coreaudio_use_float(void *data)
{
   (void)data;
   return true;
}

static size_t coreaudio_write_avail(void *data)
{
   size_t avail;
   coreaudio_t *dev = (coreaudio_t*)data;

   slock_lock(dev->lock);
   avail = fifo_write_avail(dev->buffer);
   slock_unlock(dev->lock);

   return avail;
}

static size_t coreaudio_buffer_size(void *data)
{
   coreaudio_t *dev = (coreaudio_t*)data;
   return dev->buffer_size;
}

static void *coreaudio_device_list_new(void *data)
{
   /* TODO/FIXME */
   return NULL;
}

static void coreaudio_device_list_free(void *data, void *array_list_data)
{
   /* TODO/FIXME */
}

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

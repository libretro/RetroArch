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

/* One implementation for every Apple toolchain back to Xcode 3.1 on
 * Tiger and Leopard, PowerPC included, with nothing gated on the SDK.
 * The ring between the writer and the render callback is lock-free
 * on retro_atomic, which has a backend for each of those (C11,
 * clang/GCC builtins, or OSAtomic on the oldest); the resampler is an
 * AudioConverter, in AudioToolbox, which every one of those SDKs ships
 * and the Makefile links. The writer waits
 * between callbacks on a Mach semaphore, in the kernel since 10.0:
 * signal is lock-free and safe from the real-time render thread, and
 * the wait is timed. dispatch_semaphore, which needed a 10.7 SDK and
 * a second copy of the driver for the toolchains without it, is a
 * userspace counter over exactly this primitive; at one signal per
 * callback the counter saves nothing worth a gate. */
#include <AvailabilityMacros.h>
/* TargetConditionals defines every TARGET_OS_* macro to 0 or 1, so the
 * platform split is always #if TARGET_OS_IPHONE / #if !TARGET_OS_IPHONE
 * and never #ifdef, which on a macOS SDK sees the macro defined as 0
 * and takes the iOS branch. */
#include <TargetConditionals.h>
#include <lists/string_list.h>

#include <stdlib.h>
#include <math.h>

#include <boolean.h>
#include <retro_atomic.h>

#if TARGET_OS_IPHONE
#include <AudioToolbox/AudioToolbox.h>
#else
#include <CoreAudio/CoreAudio.h>
#endif
#include <CoreAudio/CoreAudioTypes.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioUnit/AUComponent.h>

#include <retro_endianness.h>
#include <string/stdstring.h>

#include "../audio_driver.h"
#include "../../verbosity.h"

#include <mach/mach.h>
#include <mach/semaphore.h>
#include <mach/task.h>
#include <dlfcn.h>

/* --- Runtime resolution of the component API ------------------------
 *
 * The output unit is opened through AudioComponentFindNext /
 * AudioComponentInstanceNew / AudioComponentInstanceDispose on 10.6
 * and later, and through the Component Manager's FindNextComponent /
 * OpenAComponent / CloseComponent before that. Which one a binary
 * uses was a compile-time choice on the build SDK, which baked the
 * build machine into what the binary could do. Both triples are
 * resolved once at runtime instead: the modern one if the process
 * has it, the old one otherwise, and no header from either side is
 * needed since the description structs share a layout of five 32-bit
 * words and every handle is a pointer. dlsym is in 10.4. */
typedef struct
{
   UInt32 type, subtype, manufacturer, flags, mask;
} ca_component_desc_t;

typedef void *(*ca_find_next_t)(void *after, const ca_component_desc_t *desc);
typedef OSStatus (*ca_open_t)(void *component, AudioUnit *unit);
typedef OSStatus (*ca_close_t)(AudioUnit unit);

static struct
{
   ca_find_next_t find_next;
   ca_open_t      open;
   ca_close_t     close;
   bool           resolved;
   bool           modern;
} ca_cm;

static bool ca_cm_resolve(void)
{
   if (ca_cm.resolved)
      return ca_cm.find_next != NULL;
   ca_cm.resolved  = true;
   ca_cm.find_next = (ca_find_next_t)dlsym(RTLD_DEFAULT, "AudioComponentFindNext");
   ca_cm.open      = (ca_open_t)dlsym(RTLD_DEFAULT, "AudioComponentInstanceNew");
   ca_cm.close     = (ca_close_t)dlsym(RTLD_DEFAULT, "AudioComponentInstanceDispose");
   ca_cm.modern    = ca_cm.find_next && ca_cm.open && ca_cm.close;
   if (!ca_cm.modern)
   {
      ca_cm.find_next = (ca_find_next_t)dlsym(RTLD_DEFAULT, "FindNextComponent");
      ca_cm.open      = (ca_open_t)dlsym(RTLD_DEFAULT, "OpenAComponent");
      ca_cm.close     = (ca_close_t)dlsym(RTLD_DEFAULT, "CloseComponent");
   }
   if (!(ca_cm.find_next && ca_cm.open && ca_cm.close))
   {
      ca_cm.find_next = NULL;
      return false;
   }
   return true;
}

/* kAudioObjectPropertyElementMaster was renamed ElementMain in 12.0;
 * both are 0, and the number is what the HAL sees. */
#define CA_ELEMENT_MAIN 0

/* AudioConverter lives in AudioToolbox, which the common includes above
 * pull in only on iOS; on macOS they pull in CoreAudio, which does not
 * declare it. */
#include <AudioToolbox/AudioToolbox.h>


/* Threshold for recreating AudioConverter (0.5% change) */
#define RATE_CHANGE_THRESHOLD 0.005

typedef struct coreaudio
{
   /* What the writer waits on between render callbacks; see
    * coreaudio_signal() and coreaudio_wait(). */
   semaphore_t sema;
   bool        sema_alive;
   /* Writers currently inside coreaudio_wait(); the callback signals
    * only while this is non-zero. */
   retro_atomic_int_t waiters;

   /* Lock-free ring buffer */
   float *buffer;
   size_t capacity;           /* Power of 2 for fast masking */
   size_t write_ptr;          /* Only touched by main thread */
   size_t read_ptr;           /* Only touched by audio callback */
   retro_atomic_size_t filled; /* Samples currently in buffer */

   /* The output unit: ComponentInstance or AudioComponentInstance,
    * both of which are this type on every SDK. */
   AudioUnit dev;

   /* AudioConverter for hardware-accelerated resampling */
   AudioConverterRef converter;
   unsigned output_rate;  /* Hardware output rate */
   double current_ratio;  /* Effective input rate the converter was built for */
   unsigned last_input_rate; /* The two keys write_raw() last built it from */
   double last_rate_adjust;

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

static bool coreaudio_wait_init(coreaudio_t *dev)
{
   if (semaphore_create(mach_task_self(), &dev->sema,
            SYNC_POLICY_FIFO, 0) != KERN_SUCCESS)
      return false;
   dev->sema_alive = true;
   retro_atomic_int_init(&dev->waiters, 0);
   return true;
}

static void coreaudio_wait_free(coreaudio_t *dev)
{
   if (dev->sema_alive)
      semaphore_destroy(mach_task_self(), dev->sema);
}

/* Lock-free ring buffer operations */

static inline size_t rb_write_avail(coreaudio_t *dev)
{
   return dev->capacity - retro_atomic_load_acquire_size(&dev->filled);
}

static void rb_write(coreaudio_t *dev, const float *data, size_t count)
{
   size_t first = dev->capacity - dev->write_ptr;
   if (first > count)
      first = count;

   memcpy(dev->buffer + dev->write_ptr, data, first * sizeof(float));
   memcpy(dev->buffer, data + first, (count - first) * sizeof(float));

   dev->write_ptr = (dev->write_ptr + count) & (dev->capacity - 1);
   retro_atomic_fetch_add_size(&dev->filled, count);
}

static void rb_read(coreaudio_t *dev, float *data, size_t count)
{
   size_t first = dev->capacity - dev->read_ptr;
   if (first > count)
      first = count;

   memcpy(data, dev->buffer + dev->read_ptr, first * sizeof(float));
   memcpy(data + first, dev->buffer, (count - first) * sizeof(float));

   dev->read_ptr = (dev->read_ptr + count) & (dev->capacity - 1);
   retro_atomic_fetch_sub_size(&dev->filled, count);
}

/* The wait between callbacks, on a Mach semaphore: semaphore_signal
 * takes no lock and is safe from the real-time render thread, and
 * semaphore_timedwait is the wait. The timeout is a ceiling for a
 * unit that has stopped rendering and will never signal; in play the
 * writer wakes when the callback frees space, after the kernel's wake
 * latency, which is the floor on Darwin - pthread_cond, dispatch and
 * os_unfair_lock's waiters all bottom out on this same wait.
 *
 * The waiter count is what dispatch_semaphore keeps in userspace and
 * what a bare Mach semaphore lacks: without it every callback signals
 * whether anyone waits or not, and the signals accumulate while the
 * writer is non-blocking - by one per callback, without bound - so
 * that the next real wait returns at once, again and again, until the
 * lap cap trips and the frontend drops audio it could have delivered.
 * The writer raises the count, then rechecks the ring: a callback that
 * ran before the raise saw no waiter and did not signal, and the
 * recheck sees the space it freed instead. A callback that ran after
 * the raise signals, and if the writer had already left, at most one
 * stale count remains, which the loop around this absorbs. Both sides
 * pair an acq_rel read-modify-write with an acquire load, which no
 * backend reorders. */
static void coreaudio_signal(coreaudio_t *dev)
{
   if (retro_atomic_load_acquire_int(&dev->waiters))
      semaphore_signal(dev->sema);
}

static void coreaudio_wait(coreaudio_t *dev, size_t want_samples, unsigned ms)
{
   retro_atomic_fetch_add_int(&dev->waiters, 1);
   if (rb_write_avail(dev) < want_samples)
   {
      mach_timespec_t ts;
      ts.tv_sec  = ms / 1000;
      ts.tv_nsec = (ms % 1000) * 1000000;
      semaphore_timedwait(dev->sema, ts);
   }
   retro_atomic_fetch_sub_int(&dev->waiters, 1);
}

/* AudioConverter input callback - provides int16 samples */
static OSStatus converter_input_cb(
      AudioConverterRef converter,
      UInt32 *ioNumberDataPackets,
      AudioBufferList *ioData,
      AudioStreamPacketDescription **outDataPacketDescription,
      void *inUserData)
{
   UInt32 frames_to_provide;
   converter_callback_ctx_t *ctx = (converter_callback_ctx_t *)inUserData;

   if (ctx->frames_left == 0)
   {
      *ioNumberDataPackets = 0;
      return noErr;
   }

   frames_to_provide = *ioNumberDataPackets;
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
static bool coreaudio_update_converter(coreaudio_t *dev,
      unsigned input_rate, double rate_adjust, double effective_input_rate)
{
   AudioStreamBasicDescription input_desc  = {0};
   AudioStreamBasicDescription output_desc = {0};
   UInt32 quality                          = kAudioConverterQuality_High;
   OSStatus err;

   /* Rebuild when the source rate changes at all - a core switching
    * its output rate is a new stream - or when rate control has moved
    * the adjustment by more than half a percent, or when the effective
    * rate has moved that far from what the converter was built for.
    * Under that, keep it: rate control's ordinary jitter must not
    * recreate a converter every frame. */
   if (dev->converter)
   {
      double ratio_change = fabs(effective_input_rate - dev->current_ratio) / dev->current_ratio;
      if (     input_rate == dev->last_input_rate
            && fabs(rate_adjust - dev->last_rate_adjust) <= RATE_CHANGE_THRESHOLD
            && ratio_change < RATE_CHANGE_THRESHOLD)
         return true;

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

   dev->current_ratio         = effective_input_rate;
   dev->last_input_rate       = input_rate;
   dev->last_rate_adjust      = rate_adjust;
   dev->converter_needs_reset = false;

   /* Set high quality resampling */
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
      ca_cm.close(dev->dev);
   }

   if (dev->converter)
      AudioConverterDispose(dev->converter);

   if (dev->conv_buffer)
      free(dev->conv_buffer);

   if (dev->buffer)
      free(dev->buffer);

   coreaudio_wait_free(dev);

   free(dev);
}

static OSStatus coreaudio_audio_write_cb(void *userdata,
      AudioUnitRenderActionFlags *action_flags,
      const AudioTimeStamp *time_stamp, UInt32 bus_number,
      UInt32 number_frames, AudioBufferList *io_data)
{
   size_t avail;
   float *outbuf;
   size_t frames_needed;
   coreaudio_t *dev = (coreaudio_t*)userdata;

   (void)time_stamp;
   (void)bus_number;
   (void)number_frames;

   if (!io_data || io_data->mNumberBuffers != 1)
      return noErr;

   outbuf        = (float *)io_data->mBuffers[0].mData;
   frames_needed = io_data->mBuffers[0].mDataByteSize / sizeof(float);
   avail         = retro_atomic_load_acquire_size(&dev->filled);

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
   coreaudio_signal(dev);

   return noErr;
}

#if !TARGET_OS_IPHONE
/* The HAL's output devices. kAudioHardwarePropertyDevices answers on
 * the output scope from 10.6 and only on the global scope before;
 * asked at runtime rather than decided by the build SDK. Returns a
 * malloc'd array the caller frees, or NULL. */
static AudioDeviceID *coreaudio_hal_devices(UInt32 *count)
{
   AudioObjectPropertyAddress propaddr;
   AudioDeviceID *devices = NULL;
   UInt32 size            = 0;

   propaddr.mSelector = kAudioHardwarePropertyDevices;
   propaddr.mScope    = kAudioDevicePropertyScopeOutput;
   propaddr.mElement  = CA_ELEMENT_MAIN;
   if (!AudioObjectHasProperty(kAudioObjectSystemObject, &propaddr))
      propaddr.mScope = kAudioObjectPropertyScopeGlobal;

   if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
            &propaddr, 0, 0, &size) != noErr || !size)
      return NULL;
   if (!(devices = (AudioDeviceID*)malloc(size)))
      return NULL;
   if (AudioObjectGetPropertyData(kAudioObjectSystemObject,
            &propaddr, 0, 0, &size, devices) != noErr)
   {
      free(devices);
      return NULL;
   }
   *count = size / sizeof(AudioDeviceID);
   return devices;
}

static bool coreaudio_hal_device_name(AudioDeviceID id, char *s, size_t len)
{
   AudioObjectPropertyAddress propaddr;
   UInt32 size        = (UInt32)len;
   propaddr.mSelector = kAudioDevicePropertyDeviceName;
   propaddr.mScope    = kAudioDevicePropertyScopeOutput;
   propaddr.mElement  = CA_ELEMENT_MAIN;
   s[0]               = 0;
   return AudioObjectGetPropertyData(id, &propaddr, 0, 0, &size, s) == noErr
         && s[0];
}

static void coreaudio_choose_output_device(coreaudio_t *dev, const char* device)
{
   UInt32 i, device_count = 0;
   AudioDeviceID *devices = coreaudio_hal_devices(&device_count);
   if (!devices)
      return;

   for (i = 0; i < device_count; i++)
   {
      char device_name[1024];
      if (     coreaudio_hal_device_name(devices[i], device_name, sizeof(device_name))
            && string_is_equal(device_name, device))
      {
         AudioUnitSetProperty(dev->dev, kAudioOutputUnitProperty_CurrentDevice,
               kAudioUnitScope_Global, 0, &devices[i], sizeof(AudioDeviceID));
         break;
      }
   }

   free(devices);
}
#endif

/* Query the actual hardware sample rate */
static unsigned coreaudio_get_hardware_sample_rate(AudioUnit dev)
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
         prop.mElement  = CA_ELEMENT_MAIN;
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
#if !TARGET_OS_IPHONE
   AudioChannelLayout layout               = {0};
#endif
   AURenderCallbackStruct cb               = {0};
   AudioStreamBasicDescription stream_desc = {0};
   ca_component_desc_t desc                = {0};
   void *comp;
   coreaudio_t *dev;

   if (!ca_cm_resolve())
   {
      RARCH_ERR("[CoreAudio] Neither the AudioComponent API nor the Component Manager is available.\n");
      return NULL;
   }
   RARCH_DBG("[CoreAudio] Output unit through %s.\n",
         ca_cm.modern ? "AudioComponent" : "the Component Manager");

   if (!(dev = (coreaudio_t*)calloc(1, sizeof(*dev))))
      return NULL;

   if (!coreaudio_wait_init(dev))
      goto error;

   /* Open the output unit */
   desc.type         = kAudioUnitType_Output;
#if TARGET_OS_IPHONE
   desc.subtype      = kAudioUnitSubType_RemoteIO;
#else
   desc.subtype      = kAudioUnitSubType_HALOutput;
#endif
   desc.manufacturer = kAudioUnitManufacturer_Apple;

   if (!(comp = ca_cm.find_next(NULL, &desc)))
      goto error;
   if (ca_cm.open(comp, &dev->dev) != noErr)
      goto error;

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

   /* Interleaved float stereo on the input bus; the unit mixes or
    * downmixes to whatever the hardware has. RemoteIO has been seen to
    * refuse the first set and take the second, so one retry. */
   if (     AudioUnitSetProperty(dev->dev, kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Input, 0, &stream_desc, sizeof(stream_desc)) != noErr
         && AudioUnitSetProperty(dev->dev, kAudioUnitProperty_StreamFormat,
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

   /* Tell the HAL unit the two channels are a stereo pair. RemoteIO
    * refuses the property, hence macOS only; and it is advisory - the
    * stream format above already fixed two channels - so a HAL that
    * refuses it too is logged and not treated as a failed open. This
    * had been under #ifndef, which a macOS SDK defining the macro as
    * 0 turned into "never", so the layout was not being set at all. */
#if !TARGET_OS_IPHONE
   layout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
   if (AudioUnitSetProperty(dev->dev, kAudioUnitProperty_AudioChannelLayout,
         kAudioUnitScope_Input, 0, &layout, sizeof(layout)) != noErr)
      RARCH_WARN("[CoreAudio] The output unit declined a stereo channel layout; continuing with the two-channel stream format.\n");
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

   retro_atomic_size_init(&dev->filled, 0);
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
   /* Each wait below is bounded; this bounds the loop, for a unit that
    * reports running but never renders. */
   int laps           = 8;

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
         if (--laps < 0)
            break;
         /* Brief timeout as safety net for the race where the unit
          * stops during the wait; we'll re-check on the next iteration. */
         coreaudio_wait(dev, 1, 100);
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
   if (!coreaudio_update_converter(dev, input_rate, rate_adjust, effective_rate))
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

      /* Apply volume to converted samples. A plain loop: a few thousand
       * multiplies per frame, which the compiler vectorises, and not
       * worth the Accelerate umbrella that used to be pulled in for it. */
      if (volume != 1.0f)
      {
         float *v = dev->conv_buffer;
         size_t n = output_frames * 2;
         size_t k;
         for (k = 0; k < n; k++)
            v[k] *= volume;
      }

      /* Write converted samples to ring buffer */
      {
         float *out_ptr     = dev->conv_buffer;
         size_t out_samples = output_frames * 2; /* stereo */
         int    laps        = 8;

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
               if (--laps < 0)
                  break;
               coreaudio_wait(dev, 1, 100);
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

/* Also the far end of an audio session interruption on iOS and tvOS:
 * the Cocoa side observes AVAudioSessionInterruptionNotification and
 * calls audio_driver_stop() at Began and audio_driver_start() at Ended,
 * which arrive here. A converter that was mid-stream when the system
 * stopped the unit can come back stuck at end-of-stream - the tvOS
 * 13/14 symptom - so it is reset on every start. */
static bool coreaudio_start(void *data, bool is_shutdown)
{
   coreaudio_t *dev = (coreaudio_t*)data;
   if (dev)
   {
      if (dev->converter)
      {
         AudioConverterReset(dev->converter);
         dev->converter_needs_reset = false;
      }
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

/* Wait on what the render callback signals after every pull
 * until at least len bytes fit in the ring, capped at half of it so the
 * wait always ends. Returns the free space then, or 0 once the unit has
 * stopped (paused, or interrupted, which the running check catches as
 * coreaudio_write() does). */
static size_t coreaudio_wait_writable(void *data, size_t len)
{
   coreaudio_t *dev = (coreaudio_t*)data;
   size_t want      = len / sizeof(float);
   size_t half      = dev->capacity / 2;
   int    laps      = 8;

   if (want > half)
      want = half;

   for (;;)
   {
      size_t avail;
      UInt32 running = 0;
      UInt32 size    = sizeof(running);

      if (dev->is_paused)
         break;
      avail = rb_write_avail(dev);
      if (avail >= want)
         return avail * sizeof(float);
      if (AudioUnitGetProperty(dev->dev,
               kAudioOutputUnitProperty_IsRunning,
               kAudioUnitScope_Global, 0,
               &running, &size) == noErr && !running)
         break;
      /* Each wait is bounded; this bounds the loop, for a unit that
       * reports running but never renders. */
      if (--laps < 0)
         break;
      coreaudio_wait(dev, want, 100);
   }
   return 0;
}

/* Enumerates output devices from the HAL. Needs no driver instance:
 * kAudioObjectSystemObject is always there. Same query as
 * coreaudio_choose_output_device() above, collected instead of
 * matched. iOS has no HAL device enumeration for output; NULL there. */
static void *coreaudio_device_list_new(void *data)
{
#if TARGET_OS_IPHONE
   (void)data;
   return NULL;
#else
   UInt32 i, device_count = 0;
   AudioDeviceID *devices;
   union string_list_elem_attr attr;
   struct string_list *sl = string_list_new();

   (void)data;
   attr.i = 0;
   if (!sl)
      return NULL;

   if (!(devices = coreaudio_hal_devices(&device_count)))
   {
      string_list_free(sl);
      return NULL;
   }

   for (i = 0; i < device_count; i++)
   {
      char device_name[1024];
      if (coreaudio_hal_device_name(devices[i], device_name, sizeof(device_name)))
         string_list_append(sl, device_name, attr);
   }

   free(devices);
   return sl;
#endif
}

static void coreaudio_device_list_free(void *data, void *array_list_data)
{
   struct string_list *sl = (struct string_list*)array_list_data;
   (void)data;
   if (sl)
      string_list_free(sl);
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
   coreaudio_write_raw,
   coreaudio_wait_writable
};


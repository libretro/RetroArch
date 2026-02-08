/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2024 The RetroArch team.
 * Copyright (C) 2025 - Joseph Mattiello
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreFoundation/CoreFoundation.h>

#include <retro_miscellaneous.h> // General utilities
#include <retro_common.h>      // For RARCH_LOG and other common utilities
#include <lists/string_list.h>   // For string_list functions
#include <queues/fifo_queue.h>   // For fifo_buffer_t and fifo_read/write etc.
#include <audio/microphone_driver.h>
#include <verbosity.h>
#include <compat/strl.h>
#include <memory.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <rthreads/rthreads.h>

typedef struct coreaudio_macos_microphone
{
   AudioUnit audio_unit;
   fifo_buffer_t *fifo;
   AudioStreamBasicDescription format;
   atomic_bool is_running;
   atomic_bool is_initialized;
   bool nonblock;
   int sample_rate;
   bool use_float;
   char *device_name;
   AudioDeviceID selected_device_id;
   slock_t *fifo_lock;
   scond_t *fifo_cond;
   void *callback_buffer;
   size_t callback_buffer_size;
   unsigned drop_count;
   bool using_default_device;
   atomic_bool device_changed;
} coreaudio_macos_microphone_t;

/* Forward declarations */
void *coreaudio_macos_microphone_init(void);
void coreaudio_macos_microphone_free(void *data);
static int coreaudio_macos_microphone_read(void *driver_data, void *mic_data, void *buf, size_t samples);
static void coreaudio_macos_microphone_set_nonblock_state(void *data, bool state);

/* set_nonblock_state receives the driver context from init(), which is a
 * placeholder (void*)1 on macOS.  The nonblock flag lives in each mic
 * instance instead, so we cannot dereference the pointer here.
 * In practice this callback is never invoked for the microphone driver. */
static void coreaudio_macos_microphone_set_nonblock_state(void *data, bool state)
{
   (void)data;
   (void)state;
}

static struct string_list *coreaudio_macos_microphone_device_list_new(const void *data);
static void coreaudio_macos_microphone_device_list_free(const void *data, struct string_list *list);
static void *coreaudio_macos_microphone_open_mic(void *data, const char *device, unsigned rate, unsigned latency, unsigned *new_rate);
static void coreaudio_macos_microphone_close_mic(void *data, void *mic_data);
static bool coreaudio_macos_microphone_mic_alive(const void *data, const void *mic_data);
static bool coreaudio_macos_microphone_stop_mic(void *data, void *mic_data);
static bool coreaudio_macos_microphone_mic_use_float(const void *data, const void *mic_data);
static void coreaudio_macos_microphone_set_format(coreaudio_macos_microphone_t *mic, bool use_float);
static AudioDeviceID get_macos_device_id_for_uid_or_name(const char *uid_or_name);
static void coreaudio_macos_handle_device_change(coreaudio_macos_microphone_t *mic);

/* Listener for default input device changes (called on an arbitrary thread) */
static OSStatus coreaudio_macos_default_device_listener(
      AudioObjectID inObjectID,
      UInt32 inNumberAddresses,
      const AudioObjectPropertyAddress inAddresses[],
      void *inClientData)
{
   coreaudio_macos_microphone_t *mic =
      (coreaudio_macos_microphone_t *)inClientData;
   (void)inObjectID;
   (void)inNumberAddresses;
   (void)inAddresses;
   if (mic)
      atomic_store_explicit(&mic->device_changed, true, memory_order_release);
   return noErr;
}

/* Reconnect to the new default input device without changing the reported
 * sample rate.  The AUHAL's internal converter handles any rate difference
 * between the new hardware and our output-scope format. */
static void coreaudio_macos_handle_device_change(
      coreaudio_macos_microphone_t *mic)
{
   AudioObjectPropertyAddress prop = {
      kAudioHardwarePropertyDefaultInputDevice,
      kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMaster
   };
   AudioDeviceID new_device = kAudioObjectUnknown;
   UInt32 prop_size         = sizeof(AudioDeviceID);
   OSStatus status;
   bool was_running;

   status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
         &prop, 0, NULL, &prop_size, &new_device);
   if (status != noErr || new_device == kAudioObjectUnknown
         || new_device == mic->selected_device_id)
      return;

   RARCH_LOG("[CoreAudio macOS Mic] Default input device changed to %u, reconnecting\n",
             (unsigned)new_device);

   was_running = atomic_load(&mic->is_running);

   if (was_running)
      AudioOutputUnitStop(mic->audio_unit);
   atomic_store(&mic->is_running, false);

   if (atomic_load(&mic->is_initialized))
   {
      AudioUnitUninitialize(mic->audio_unit);
      atomic_store(&mic->is_initialized, false);
   }

   mic->selected_device_id = new_device;
   status = AudioUnitSetProperty(mic->audio_unit,
         kAudioOutputUnitProperty_CurrentDevice,
         kAudioUnitScope_Global, 0,
         &new_device, sizeof(AudioDeviceID));
   if (status != noErr)
   {
      RARCH_ERR("[CoreAudio macOS Mic] Failed to set new device: %d\n",
                (int)status);
      return;
   }

   /* Re-apply our format — keep the same rate we originally reported */
   AudioUnitSetProperty(mic->audio_unit,
         kAudioUnitProperty_StreamFormat,
         kAudioUnitScope_Output, 1,
         &mic->format, sizeof(AudioStreamBasicDescription));

   status = AudioUnitInitialize(mic->audio_unit);
   if (status != noErr)
   {
      RARCH_ERR("[CoreAudio macOS Mic] Failed to reinitialize: %d\n",
                (int)status);
      return;
   }
   atomic_store(&mic->is_initialized, true);

   slock_lock(mic->fifo_lock);
   fifo_clear(mic->fifo);
   slock_unlock(mic->fifo_lock);

   if (was_running)
   {
      status = AudioOutputUnitStart(mic->audio_unit);
      if (status == noErr)
         atomic_store(&mic->is_running, true);
      else
         RARCH_ERR("[CoreAudio macOS Mic] Failed to restart: %d\n",
                   (int)status);
   }
}

/* AudioUnit render callback — runs on the real-time audio thread. */
static OSStatus coreaudio_macos_input_callback(void *inRefCon,
                                             AudioUnitRenderActionFlags *ioActionFlags,
                                             const AudioTimeStamp *inTimeStamp,
                                             UInt32 inBusNumber,
                                             UInt32 inNumberFrames,
                                             AudioBufferList *ioData)
{
    coreaudio_macos_microphone_t *mic = (coreaudio_macos_microphone_t*)inRefCon;
    AudioBufferList buffer_list;
    OSStatus status;
    size_t bytes_needed;
    size_t actual_bytes;

    if (!mic || !atomic_load_explicit(&mic->is_running, memory_order_relaxed))
        return noErr;

    bytes_needed = inNumberFrames * mic->format.mBytesPerFrame;
    if (bytes_needed == 0)
        return noErr;

    /* Use pre-allocated buffer; drop this chunk if it doesn't fit rather
     * than allocating on the real-time thread. */
    if (bytes_needed > mic->callback_buffer_size)
        return noErr;

    memset(mic->callback_buffer, 0, bytes_needed);

    buffer_list.mNumberBuffers              = 1;
    buffer_list.mBuffers[0].mDataByteSize   = (UInt32)bytes_needed;
    buffer_list.mBuffers[0].mData           = mic->callback_buffer;
    buffer_list.mBuffers[0].mNumberChannels = mic->format.mChannelsPerFrame;

    status = AudioUnitRender(mic->audio_unit, ioActionFlags,
          inTimeStamp, 1, inNumberFrames, &buffer_list);

    if (status == noErr
          && buffer_list.mBuffers[0].mData
          && buffer_list.mBuffers[0].mDataByteSize > 0)
    {
        actual_bytes = MIN(bytes_needed,
              buffer_list.mBuffers[0].mDataByteSize);

        /* Use trylock so we never block the real-time thread. */
        if (slock_try_lock(mic->fifo_lock))
        {
            if (FIFO_WRITE_AVAIL(mic->fifo) >= actual_bytes)
                fifo_write(mic->fifo, buffer_list.mBuffers[0].mData,
                      actual_bytes);
            else if (mic->drop_count++ % 1000 == 0)
                RARCH_WARN("[CoreAudio macOS Mic] FIFO full, dropping %u bytes\n",
                           (unsigned)actual_bytes);
            scond_signal(mic->fifo_cond);
            slock_unlock(mic->fifo_lock);
        }
    }
    else if (status != noErr)
        RARCH_ERR("[CoreAudio macOS Mic] Failed to render audio: %d\n",
                  (int)status);

    /* Always return noErr — returning errors may cause CoreAudio to
     * stop invoking the callback entirely. */
    return noErr;
}

static int coreaudio_macos_microphone_read(void *driver_data, void *mic_data, void *buf, size_t samples)
{
   coreaudio_macos_microphone_t *microphone = (coreaudio_macos_microphone_t *)mic_data;
   size_t avail, read_amt;

   if (!microphone || !buf)
   {
      RARCH_ERR("[CoreAudio] Invalid parameters in read\n");
      return -1;
   }

   /* Handle pending device change before taking the FIFO lock. */
   if (atomic_load_explicit(&microphone->device_changed, memory_order_acquire))
   {
      atomic_store_explicit(&microphone->device_changed, false,
            memory_order_relaxed);
      coreaudio_macos_handle_device_change(microphone);
   }

   slock_lock(microphone->fifo_lock);

   avail    = FIFO_READ_AVAIL(microphone->fifo);
   read_amt = MIN(avail, samples);

   /* In blocking mode, wait briefly for the callback to provide data. */
   if (read_amt == 0 && !microphone->nonblock)
   {
      scond_wait_timeout(microphone->fifo_cond,
            microphone->fifo_lock, 10000); /* 10 ms */
      avail    = FIFO_READ_AVAIL(microphone->fifo);
      read_amt = MIN(avail, samples);
   }

   if (read_amt > 0)
      fifo_read(microphone->fifo, buf, read_amt);

   slock_unlock(microphone->fifo_lock);

   return (int)read_amt;
}

static void coreaudio_macos_microphone_set_format(coreaudio_macos_microphone_t *mic, bool use_float)
{
   if (!mic)
      return;

   /* Store the format choice */
   mic->use_float = use_float;

   /* Setup the format for the AudioUnit based on the parameters */
   AudioStreamBasicDescription *format = &mic->format;

   /* Clear the format struct */
   memset(format, 0, sizeof(AudioStreamBasicDescription));

   /* Set basic properties */
   format->mSampleRate = mic->sample_rate;
   format->mFormatID = kAudioFormatLinearPCM;
   format->mFormatFlags = use_float ?
      (kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked) :
      (kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked);
   format->mFramesPerPacket = 1;
   format->mChannelsPerFrame = 1; /* Always use mono for microphone input */
   format->mBitsPerChannel = use_float ? 32 : 16;
   format->mBytesPerFrame = format->mChannelsPerFrame * format->mBitsPerChannel / 8;
   format->mBytesPerPacket = format->mBytesPerFrame * format->mFramesPerPacket;

   RARCH_LOG("[CoreAudio macOS Mic] Format setup: sample_rate=%.0f Hz, bits=%u, bytes_per_frame=%u, %s\n",
             format->mSampleRate,
             (unsigned)format->mBitsPerChannel,
             (unsigned)format->mBytesPerFrame,
             use_float ? "Float" : "Int16");
}

/* Implementation of init function */
void *coreaudio_macos_microphone_init(void)
{
   return (void*)1; /* Return a non-NULL placeholder */
}

/* Implementation of free function */
void coreaudio_macos_microphone_free(void *data)
{
   /* No global state to clean up */
   (void)data;
}

static struct string_list *coreaudio_macos_microphone_device_list_new(const void *data)
{
   RARCH_LOG("[CoreAudio macOS Mic] device_list_new called.\n");
   struct string_list *list = string_list_new();
   if (!list)
   {
      RARCH_ERR("[CoreAudio macOS Mic] Failed to create string_list.\n");
      return NULL;
   }

   AudioObjectPropertyAddress prop_addr_devices = {
      kAudioHardwarePropertyDevices,
      kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMaster
   };

   UInt32 propsize = 0;
   OSStatus status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &prop_addr_devices, 0, NULL, &propsize);
   if (status != noErr || propsize == 0)
   {
      RARCH_ERR("[CoreAudio macOS Mic] Error getting size of device list: %d\n", (int)status);
      string_list_free(list);
      return NULL;
   }

   UInt32 num_devices = propsize / sizeof(AudioDeviceID);
   AudioDeviceID *all_devices = (AudioDeviceID *)malloc(propsize);
   if (!all_devices)
   {
      RARCH_ERR("[CoreAudio macOS Mic] Failed to allocate memory for device list.\n");
      string_list_free(list);
      return NULL;
   }

   status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop_addr_devices, 0, NULL, &propsize, all_devices);
   if (status != noErr)
   {
      RARCH_ERR("[CoreAudio macOS Mic] Error getting device list: %d\n", (int)status);
      free(all_devices);
      string_list_free(list);
      return NULL;
   }

   RARCH_LOG("[CoreAudio macOS Mic] Found %u total audio devices.\n", num_devices);

   for (UInt32 i = 0; i < num_devices; i++)
   {
      AudioDeviceID current_device_id = all_devices[i];
      UInt32 input_stream_count = 0;
      AudioObjectPropertyAddress prop_addr_streams_input = {
         kAudioDevicePropertyStreams,
         kAudioDevicePropertyScopeInput,
         kAudioObjectPropertyElementMaster
      };

      propsize = 0;
      status = AudioObjectGetPropertyDataSize(current_device_id, &prop_addr_streams_input, 0, NULL, &propsize);
      if (status == noErr && propsize > 0)
         input_stream_count = propsize / sizeof(AudioStreamID); /* Can be more than 1, but we just need > 0 */

      if (input_stream_count > 0)
      {
         CFStringRef device_uid_cf = NULL;
         /* This device has input streams, get its name and UID */
         CFStringRef device_name_cf = NULL;
         propsize = sizeof(CFStringRef);
         AudioObjectPropertyAddress prop_addr_name = {
            kAudioDevicePropertyDeviceNameCFString,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
         };
         status   = AudioObjectGetPropertyData(current_device_id, &prop_addr_name, 0, NULL, &propsize, &device_name_cf);
         propsize = sizeof(CFStringRef);
         AudioObjectPropertyAddress prop_addr_uid = {
            kAudioDevicePropertyDeviceUID,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
         };
         OSStatus uid_status = AudioObjectGetPropertyData(current_device_id, &prop_addr_uid, 0, NULL, &propsize, &device_uid_cf);

         if (status == noErr && device_name_cf && uid_status == noErr && device_uid_cf)
         {
            char device_name_c[256] = {0};
            char device_uid_c[256]  = {0};

            CFStringGetCString(device_name_cf, device_name_c, sizeof(device_name_c), kCFStringEncodingUTF8);
            CFStringGetCString(device_uid_cf, device_uid_c, sizeof(device_uid_c), kCFStringEncodingUTF8);

            union string_list_elem_attr attr_sl;
            attr_sl.p = strdup(device_uid_c); /* strdup to give ownership to the list item */
            if (!attr_sl.p)
            {
               RARCH_ERR("[CoreAudio macOS Mic] Failed to strdup device UID '%s'.\n", device_uid_c);
               /* Potentially continue without this device or handle error more globally */
            }
            else if (!string_list_append(list, device_name_c, attr_sl))
            {
               RARCH_ERR("[CoreAudio macOS Mic] Failed to append device '%s' to list.\n", device_name_c);
            }
            else
            {
               RARCH_LOG("[CoreAudio macOS Mic] Added input device: '%s' (UID: '%s')\n", device_name_c, device_uid_c);
            }

         }
         if (device_name_cf)
            CFRelease(device_name_cf);
         if (device_uid_cf)
            CFRelease(device_uid_cf);
      }
   }

   free(all_devices);

   if (list->size == 0)
   {
      RARCH_WARN("[CoreAudio macOS Mic] No input devices found after filtering. Will use system default if available.\n");
      /* string_list_free will be called by the caller if list is non-NULL but size is 0 */
      /* However, if we allocated attr.p with strdup, we need to free them before freeing the list itself, */
      /* or modify device_list_free to handle it. For now, let's assume caller handles list freeing. */
      /* If list is returned (even if empty), caller should free it. */
      /* If we return NULL here, the list allocated above is leaked unless freed here. */
      /* Let's ensure it's freed if we return NULL. */
      coreaudio_macos_microphone_device_list_free(NULL, list); /* Pass NULL for data as it's not used by this free func */
      return NULL;
   }
   return list;
}

static void coreaudio_macos_microphone_device_list_free(const void *data, struct string_list *list)
{
   (void)data; /* Not used in this implementation */
   RARCH_LOG("[CoreAudio macOS Mic] device_list_free called.\n");
   if (list)
   {
      for (size_t i = 0; i < list->size; i++)
      {
         if (list->elems[i].attr.p)
         {
            free(list->elems[i].attr.p); /* Free the strdup'd UIDs */
            list->elems[i].attr.p = NULL;
         }
      }
      string_list_free(list); /* This frees elems[i].data and the list itself */
   }
}

static void *coreaudio_macos_microphone_open_mic(void *data, const char *device, unsigned rate, unsigned latency, unsigned *new_rate)
{
   RARCH_LOG("[CoreAudio macOS Mic] *** OPEN_MIC CALLED *** Device: %s, Rate: %u, Latency: %u. Driver context: %p\n", device ? device : "default", rate, latency, data);
   RARCH_LOG("[CoreAudio macOS Mic] IMPORTANT - Requested sample rate from core: %u Hz\n", rate);
   (void)data; /* Driver context from init(), not directly used to create the mic instance */
   /* The 'data' parameter is the driver_context returned by init(). */
   /* For this driver, init() returns a placeholder (void*)1, so we don't use 'data' to allocate the mic instance. */
   /* The actual mic instance (coreaudio_macos_microphone_t) is allocated below. */


   coreaudio_macos_microphone_t *mic = (coreaudio_macos_microphone_t *)calloc(1, sizeof(coreaudio_macos_microphone_t));
   if (!mic)
   {
      RARCH_ERR("[CoreAudio macOS Mic] Failed to allocate memory for microphone context.\n");
      return NULL;
   }

   atomic_init(&mic->is_running, false);
   atomic_init(&mic->is_initialized, false);
   atomic_init(&mic->device_changed, false);
   mic->fifo_lock = slock_new();
   mic->fifo_cond = scond_new();
   mic->sample_rate = rate;
   RARCH_LOG("[CoreAudio macOS Mic] Setting mic->sample_rate to %u Hz\n", mic->sample_rate);
   if (device && strlen(device) > 0 && strcmp(device, "default") != 0)
   {
      mic->device_name = strdup(device);
      mic->selected_device_id = get_macos_device_id_for_uid_or_name(mic->device_name);
      RARCH_LOG("[CoreAudio macOS Mic] Requested device '%s', selected AudioDeviceID: %u\n", mic->device_name, (unsigned int)mic->selected_device_id);
   }
   else
   {
      mic->device_name = strdup("default");
      mic->using_default_device = true;

      /* Get the actual system default input device instead of using kAudioObjectUnknown */
      AudioObjectPropertyAddress prop_addr = {
         kAudioHardwarePropertyDefaultInputDevice,
         kAudioObjectPropertyScopeGlobal,
         kAudioObjectPropertyElementMaster
      };
      UInt32 prop_size = sizeof(AudioDeviceID);
      OSStatus default_status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop_addr, 0, NULL, &prop_size, &mic->selected_device_id);

      if (default_status == noErr && mic->selected_device_id != kAudioObjectUnknown)
      {
         RARCH_LOG("[CoreAudio macOS Mic] Requested default device, found actual default input device ID: %u\n", (unsigned int)mic->selected_device_id);
      }
      else
      {
         RARCH_ERR("[CoreAudio macOS Mic] Failed to get default input device (status: %d), falling back to kAudioObjectUnknown\n", (int)default_status);
         mic->selected_device_id = kAudioObjectUnknown;
      }
   }

   OSStatus status = noErr;

   /* 1. Find and open the AudioUnit */
   AudioComponentDescription desc;
   desc.componentType          = kAudioUnitType_Output;
   desc.componentSubType       = kAudioUnitSubType_HALOutput;
   desc.componentManufacturer  = kAudioUnitManufacturer_Apple;
   desc.componentFlags         = 0;
   desc.componentFlagsMask     = 0;

   AudioComponent comp = AudioComponentFindNext(NULL, &desc);
   if (!comp)
   {
      RARCH_ERR("[CoreAudio macOS Mic] Failed to find HALOutput AudioComponent.\n");
      goto error;
   }

   status = AudioComponentInstanceNew(comp, &mic->audio_unit);
   if (status != noErr || !mic->audio_unit)
   {
      RARCH_ERR("[CoreAudio macOS Mic] Failed to create AudioUnit instance: %d\n", (int)status);
      goto error;
   }
   RARCH_LOG("[CoreAudio macOS Mic] AudioUnit instance created: %p\n", mic->audio_unit);

   /* 2. Enable input on the AudioUnit */
   {
      UInt32 enable_io  = 1;
      UInt32 disable_io = 0;

      status = AudioUnitSetProperty(mic->audio_unit,
            kAudioOutputUnitProperty_EnableIO,
            kAudioUnitScope_Input, 1,
            &enable_io, sizeof(enable_io));
      if (status != noErr)
      {
         RARCH_ERR("[CoreAudio macOS Mic] Failed to enable input on AudioUnit: %d\n", (int)status);
         goto error;
      }

      /* Disable output — we only need input */
      AudioUnitSetProperty(mic->audio_unit,
            kAudioOutputUnitProperty_EnableIO,
            kAudioUnitScope_Output, 0,
            &disable_io, sizeof(disable_io));
   }

   /* 3. Set the current device */
   if (mic->selected_device_id != kAudioObjectUnknown)
   {
      status = AudioUnitSetProperty(mic->audio_unit,
            kAudioOutputUnitProperty_CurrentDevice,
            kAudioUnitScope_Global, 0,
            &mic->selected_device_id,
            sizeof(mic->selected_device_id));
      if (status != noErr)
      {
         RARCH_WARN("[CoreAudio macOS Mic] Failed to set device %u: %d, using system default.\n",
                    (unsigned)mic->selected_device_id, (int)status);
         mic->selected_device_id = kAudioObjectUnknown;
      }
   }

   /* Query the effective device */
   {
      AudioDeviceID actual_device = kAudioObjectUnknown;
      UInt32 actual_size          = sizeof(AudioDeviceID);
      if (AudioUnitGetProperty(mic->audio_unit,
               kAudioOutputUnitProperty_CurrentDevice,
               kAudioUnitScope_Global, 0,
               &actual_device, &actual_size) == noErr)
         mic->selected_device_id = actual_device;
   }

   /* 4. Set stream format */
   coreaudio_macos_microphone_set_format(mic, false);

   /* Get the device's native format and adopt its sample rate */
   {
      AudioStreamBasicDescription device_format = {0};
      UInt32 fmt_size = sizeof(device_format);
      OSStatus fmt_status = AudioUnitGetProperty(mic->audio_unit,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Input, 1,
            &device_format, &fmt_size);
      if (fmt_status == noErr)
      {
         RARCH_LOG("[CoreAudio macOS Mic] Device native format - SR: %.0f, CH: %u\n",
                   device_format.mSampleRate,
                   (unsigned)device_format.mChannelsPerFrame);
         mic->format.mSampleRate     = device_format.mSampleRate;
         mic->format.mBytesPerFrame  = mic->format.mChannelsPerFrame
                                     * (mic->format.mBitsPerChannel / 8);
         mic->format.mBytesPerPacket = mic->format.mBytesPerFrame;
      }
   }

   {
      OSStatus fmt_set = AudioUnitSetProperty(mic->audio_unit,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Output, 1,
            &mic->format, sizeof(AudioStreamBasicDescription));
      if (fmt_set != noErr)
      {
         RARCH_ERR("[CoreAudio macOS Mic] Failed to set client stream format: %d\n", (int)fmt_set);
         goto error;
      }
   }

   /* Set up input callback */
   {
      AURenderCallbackStruct cb;
      cb.inputProc       = coreaudio_macos_input_callback;
      cb.inputProcRefCon = mic;
      status = AudioUnitSetProperty(mic->audio_unit,
            kAudioOutputUnitProperty_SetInputCallback,
            kAudioUnitScope_Global, 0,
            &cb, sizeof(cb));
      if (status != noErr)
      {
         RARCH_ERR("[CoreAudio macOS Mic] Failed to set input callback: %d\n", (int)status);
         goto error;
      }
   }

   /* Set buffer frame size for lower latency */
   {
      UInt32 buffer_frame_size = 256;
      AudioUnitSetProperty(mic->audio_unit,
            kAudioDevicePropertyBufferFrameSize,
            kAudioUnitScope_Global, 0,
            &buffer_frame_size, sizeof(buffer_frame_size));
   }

   /* Initialize AudioUnit */
   status = AudioUnitInitialize(mic->audio_unit);
   if (status != noErr)
   {
      RARCH_ERR("[CoreAudio macOS Mic] Failed to initialize AudioUnit: %d\n", (int)status);
      goto error;
   }
   atomic_store(&mic->is_initialized, true);

   /* Allocate callback buffer based on max frames per slice */
   {
      UInt32 max_frames     = 4096; /* sensible default */
      UInt32 max_frames_sz  = sizeof(max_frames);
      AudioUnitGetProperty(mic->audio_unit,
            kAudioUnitProperty_MaximumFramesPerSlice,
            kAudioUnitScope_Global, 0,
            &max_frames, &max_frames_sz);
      mic->callback_buffer_size = max_frames * mic->format.mBytesPerFrame;
      mic->callback_buffer      = calloc(1, mic->callback_buffer_size);
      if (!mic->callback_buffer)
      {
         RARCH_ERR("[CoreAudio macOS Mic] Failed to allocate callback buffer\n");
         goto error;
      }
   }

   /* Initialize FIFO — size based on latency parameter */
   {
      size_t fifo_size = (size_t)latency * mic->format.mSampleRate
                       * mic->format.mBytesPerFrame / 1000;
      if (fifo_size == 0)
         fifo_size = (size_t)(mic->format.mSampleRate
                   * mic->format.mBytesPerFrame / 10); /* 100ms fallback */
      mic->fifo = fifo_new(fifo_size);
      if (!mic->fifo)
      {
         RARCH_ERR("[CoreAudio macOS Mic] Failed to allocate FIFO buffer\n");
         goto error;
      }
      fifo_clear(mic->fifo);
      RARCH_LOG("[CoreAudio macOS Mic] FIFO buffer: %u bytes (%.0f ms)\n",
                (unsigned)fifo_size,
                (float)fifo_size * 1000.0f
                / (mic->format.mSampleRate * mic->format.mBytesPerFrame));
   }

   /* Register for default-device-change notifications */
   if (mic->using_default_device)
   {
      AudioObjectPropertyAddress prop = {
         kAudioHardwarePropertyDefaultInputDevice,
         kAudioObjectPropertyScopeGlobal,
         kAudioObjectPropertyElementMaster
      };
      AudioObjectAddPropertyListener(kAudioObjectSystemObject,
            &prop, coreaudio_macos_default_device_listener, mic);
   }

   RARCH_LOG("[CoreAudio macOS Mic] Ready — SR: %.0f Hz, %s, device '%s'\n",
             mic->format.mSampleRate,
             mic->use_float ? "Float" : "Int16",
             mic->device_name ? mic->device_name : "default");

   if (new_rate)
      *new_rate = (unsigned)mic->format.mSampleRate;

   return mic;

error:
   if (mic->audio_unit)
   {
      if (atomic_load(&mic->is_initialized))
         AudioUnitUninitialize(mic->audio_unit);
      AudioComponentInstanceDispose(mic->audio_unit);
   }
   if (mic->callback_buffer)
      free(mic->callback_buffer);
   if (mic->fifo)
      fifo_free(mic->fifo);
   if (mic->device_name)
      free(mic->device_name);
   slock_free(mic->fifo_lock);
   scond_free(mic->fifo_cond);
   free(mic);
   return NULL;
}

static void coreaudio_macos_microphone_close_mic(void *data, void *mic_data)
{
   coreaudio_macos_microphone_t *mic = (coreaudio_macos_microphone_t *)mic_data;
   if (!mic)
      return;

   /* Unregister device-change listener before teardown */
   if (mic->using_default_device)
   {
      AudioObjectPropertyAddress prop = {
         kAudioHardwarePropertyDefaultInputDevice,
         kAudioObjectPropertyScopeGlobal,
         kAudioObjectPropertyElementMaster
      };
      AudioObjectRemovePropertyListener(kAudioObjectSystemObject,
            &prop, coreaudio_macos_default_device_listener, mic);
   }

   coreaudio_macos_microphone_stop_mic(data, mic_data);

   if (mic->audio_unit)
   {
      if (atomic_load(&mic->is_initialized))
      {
         AudioUnitUninitialize(mic->audio_unit);
         atomic_store(&mic->is_initialized, false);
      }
      AudioComponentInstanceDispose(mic->audio_unit);
      mic->audio_unit = NULL;
   }

   if (mic->callback_buffer)
   {
      free(mic->callback_buffer);
      mic->callback_buffer = NULL;
   }

   if (mic->fifo)
   {
      fifo_free(mic->fifo);
      mic->fifo = NULL;
   }

   if (mic->device_name)
   {
      free(mic->device_name);
      mic->device_name = NULL;
   }

   slock_free(mic->fifo_lock);
   scond_free(mic->fifo_cond);

   free(mic);
   RARCH_LOG("[CoreAudio macOS Mic] Microphone instance %p freed.\n", mic_data);
}

static bool coreaudio_macos_microphone_mic_alive(const void *data, const void *mic_data)
{
   const coreaudio_macos_microphone_t *mic = (const coreaudio_macos_microphone_t *)mic_data;
   return mic && atomic_load(&mic->is_running);
}

static bool coreaudio_macos_microphone_start_mic(void *data, void *mic_data)
{
   RARCH_LOG("[CoreAudio macOS Mic] start_mic called. Mic context: %p\n", mic_data);
   coreaudio_macos_microphone_t *mic = (coreaudio_macos_microphone_t *)mic_data;
   if (!mic || !mic->audio_unit || !atomic_load(&mic->is_initialized))
   {
      RARCH_ERR("[CoreAudio macOS Mic] Cannot start - mic not opened or AudioUnit not initialized.\n");
      return false;
   }

   if (atomic_load(&mic->is_running))
   {
      RARCH_LOG("[CoreAudio macOS Mic] Already running.\n");
      return true;
   }

   /* Check microphone permission on macOS */
   RARCH_LOG("[CoreAudio macOS Mic] Checking microphone permission...\n");

   /* Check if we have input devices available */
   AudioObjectPropertyAddress prop_addr = {
      kAudioHardwarePropertyDefaultInputDevice,
      kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMaster
   };

   AudioDeviceID default_input_device = kAudioObjectUnknown;
   UInt32 prop_size = sizeof(AudioDeviceID);
   OSStatus perm_status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop_addr, 0, NULL, &prop_size, &default_input_device);

   if (perm_status != noErr || default_input_device == kAudioObjectUnknown)
   {
      RARCH_ERR("[CoreAudio macOS Mic] No default input device available or permission denied. Status: %d, Device ID: %u\n",
               (int)perm_status, (unsigned)default_input_device);
   }
   else
   {
      RARCH_LOG("[CoreAudio macOS Mic] Default input device ID: %u\n", (unsigned)default_input_device);
   }

   if (!mic->fifo)
   {
       RARCH_ERR("[CoreAudio macOS Mic] No FIFO buffer available\n");
       return false;
   }
   slock_lock(mic->fifo_lock);
   fifo_clear(mic->fifo);
   slock_unlock(mic->fifo_lock);

   OSStatus status = AudioOutputUnitStart(mic->audio_unit);
   if (status == noErr)
   {
      atomic_store(&mic->is_running, true);
      RARCH_LOG("[CoreAudio macOS Mic] Microphone started successfully\n");
      return true;
   }
   else
   {
      RARCH_ERR("[CoreAudio macOS Mic] Failed to start AudioUnit: %d (0x%x)\n", (int)status, (unsigned)status);
      atomic_store(&mic->is_running, false);
      return false;
   }
}

static bool coreaudio_macos_microphone_stop_mic(void *data, void *mic_data)
{
   coreaudio_macos_microphone_t *mic = (coreaudio_macos_microphone_t *)mic_data;
   if (!mic || !mic->audio_unit)
   {
      return true; /* Considered stopped if not valid */
   }

   if (!atomic_load(&mic->is_running))
   {
      return true;
   }

   OSStatus status = AudioOutputUnitStop(mic->audio_unit);
   if (status == noErr)
   {
      atomic_store(&mic->is_running, false);
      if (mic->fifo)
      {
         slock_lock(mic->fifo_lock);
         fifo_clear(mic->fifo);
         slock_unlock(mic->fifo_lock);
      }
      return true;
   }
   else
   {
      RARCH_ERR("[CoreAudio macOS Mic] Failed to stop AudioUnit: %d\n", (int)status);
      /* Even if stop fails, we mark as not running from our perspective */
      atomic_store(&mic->is_running, false);
      return false;
   }
}

static bool coreaudio_macos_microphone_mic_use_float(const void *data, const void *mic_data)
{
   (void)data;
   coreaudio_macos_microphone_t *mic = (coreaudio_macos_microphone_t *)mic_data;
   bool result = mic && mic->use_float;
   return result;
}


static AudioDeviceID get_macos_device_id_for_uid_or_name(const char *uid_or_name)
{
   RARCH_LOG("[CoreAudio macOS Mic] get_macos_device_id_for_uid_or_name looking for '%s'\n", uid_or_name ? uid_or_name : "(null - will use default)");
   if (!uid_or_name || strlen(uid_or_name) == 0 || strcmp(uid_or_name, "default") == 0)
   {
      RARCH_LOG("[CoreAudio macOS Mic] Requested default device or empty name. Returning kAudioObjectUnknown to signify system default.\n");
      return kAudioObjectUnknown; /* Indicates to use system default input device */
   }

   AudioObjectPropertyAddress prop_addr_devices = {
      kAudioHardwarePropertyDevices,
      kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMaster
   };

   UInt32 propsize = 0;
   OSStatus status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &prop_addr_devices, 0, NULL, &propsize);
   if (status != noErr || propsize == 0)
   {
      RARCH_ERR("[CoreAudio macOS Mic] Error getting size of device list for UID lookup: %d\n", (int)status);
      return kAudioObjectUnknown;
   }

   UInt32 num_devices = propsize / sizeof(AudioDeviceID);
   AudioDeviceID *all_devices = (AudioDeviceID *)malloc(propsize);
   if (!all_devices)
   {
      RARCH_ERR("[CoreAudio macOS Mic] Failed to allocate memory for device list for UID lookup.\n");
      return kAudioObjectUnknown;
   }

   status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop_addr_devices, 0, NULL, &propsize, all_devices);
   if (status != noErr)
   {
      RARCH_ERR("[CoreAudio macOS Mic] Error getting device list for UID lookup: %d\n", (int)status);
      free(all_devices);
      return kAudioObjectUnknown;
   }

   AudioDeviceID found_device_id = kAudioObjectUnknown;

   for (UInt32 i = 0; i < num_devices; i++)
   {
      AudioDeviceID current_device_id = all_devices[i];
      AudioObjectPropertyAddress prop_addr_streams_input = {
         kAudioDevicePropertyStreams,
         kAudioDevicePropertyScopeInput,
         kAudioObjectPropertyElementMaster
      };
      propsize = 0;
      status = AudioObjectGetPropertyDataSize(current_device_id, &prop_addr_streams_input, 0, NULL, &propsize);
      if (status != noErr || propsize == 0) continue; /* Not an input device or error */

      /* Check UID first */
      CFStringRef device_uid_cf = NULL;
      propsize = sizeof(CFStringRef);
      AudioObjectPropertyAddress prop_addr_uid = {
         kAudioDevicePropertyDeviceUID,
         kAudioObjectPropertyScopeGlobal,
         kAudioObjectPropertyElementMaster
      };
      status = AudioObjectGetPropertyData(current_device_id, &prop_addr_uid, 0, NULL, &propsize, &device_uid_cf);
      if (status == noErr && device_uid_cf)
      {
         char device_uid_c[256] = {0};
         if (CFStringGetCString(device_uid_cf, device_uid_c, sizeof(device_uid_c), kCFStringEncodingUTF8))
         {
            if (strcmp(uid_or_name, device_uid_c) == 0)
            {
               found_device_id = current_device_id;
               CFRelease(device_uid_cf);
               RARCH_LOG("[CoreAudio macOS Mic] Found device by UID: '%s' (ID: %u)\n", uid_or_name, (unsigned int)found_device_id);
               goto end_loop; /* Found by UID, no need to check name */
            }
         }
         CFRelease(device_uid_cf);
      }
   }

   /* If not found by UID, try by name (less reliable) */
   RARCH_LOG("[CoreAudio macOS Mic] Device not found by UID '%s'. Trying by name as fallback.\n", uid_or_name);
   for (UInt32 i = 0; i < num_devices; i++)
   {
      AudioDeviceID current_device_id = all_devices[i];
      AudioObjectPropertyAddress prop_addr_streams_input = {
         kAudioDevicePropertyStreams,
         kAudioDevicePropertyScopeInput,
         kAudioObjectPropertyElementMaster
      };
      propsize = 0;
      status = AudioObjectGetPropertyDataSize(current_device_id, &prop_addr_streams_input, 0, NULL, &propsize);
      if (status != noErr || propsize == 0) continue; /* Not an input device or error */

      CFStringRef device_name_cf = NULL;
      propsize = sizeof(CFStringRef);
      AudioObjectPropertyAddress prop_addr_name = {
         kAudioDevicePropertyDeviceNameCFString,
         kAudioObjectPropertyScopeGlobal,
         kAudioObjectPropertyElementMaster
      };
      status = AudioObjectGetPropertyData(current_device_id, &prop_addr_name, 0, NULL, &propsize, &device_name_cf);
      if (status == noErr && device_name_cf)
      {
         char device_name_c[256] = {0};
         if (CFStringGetCString(device_name_cf, device_name_c, sizeof(device_name_c), kCFStringEncodingUTF8))
         {
            if (strcmp(uid_or_name, device_name_c) == 0)
            {
               found_device_id = current_device_id;
               CFRelease(device_name_cf);
               RARCH_LOG("[CoreAudio macOS Mic] Found device by NAME: '%s' (ID: %u)\n", uid_or_name, (unsigned int)found_device_id);
               goto end_loop; /* Found by name */
            }
         }
         CFRelease(device_name_cf);
      }
   }

end_loop:
   free(all_devices);
   if (found_device_id == kAudioObjectUnknown)
   {
       RARCH_WARN("[CoreAudio macOS Mic] Device UID/Name '%s' not found. Will use system default.\n", uid_or_name);
   }
   return found_device_id;
}

microphone_driver_t microphone_coreaudio = {
   coreaudio_macos_microphone_init,
   coreaudio_macos_microphone_free,
   coreaudio_macos_microphone_read,
   coreaudio_macos_microphone_set_nonblock_state,
   "coreaudio", /* Name */
   coreaudio_macos_microphone_device_list_new,
   coreaudio_macos_microphone_device_list_free,
   coreaudio_macos_microphone_open_mic,
   coreaudio_macos_microphone_close_mic,
   coreaudio_macos_microphone_mic_alive,
   coreaudio_macos_microphone_start_mic,
   coreaudio_macos_microphone_stop_mic,
   coreaudio_macos_microphone_mic_use_float
};

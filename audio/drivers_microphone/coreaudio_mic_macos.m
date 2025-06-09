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
#include <pthread.h> /* For mutexes */

typedef struct coreaudio_macos_microphone
{
   AudioUnit audio_unit;
   fifo_buffer_t *fifo;
   AudioStreamBasicDescription format;
   atomic_bool is_running;
   atomic_bool is_initialized; /* For AudioUnitInitialize state */
   bool nonblock;
   int sample_rate;
   int channels;                /* Number of audio channels */
   bool use_float;
   char *device_name;            /* Store selected device name (from string_list) */
   AudioDeviceID selected_device_id; /* Store selected device ID */
} coreaudio_macos_microphone_t;

/* Forward declarations */
void *coreaudio_macos_microphone_init(void);
void coreaudio_macos_microphone_free(void *data);
static int coreaudio_macos_microphone_read(void *driver_data, void *mic_data, void *buf, size_t samples);
static void coreaudio_macos_microphone_set_nonblock_state(void *data, bool state);

/* Implementation of nonblock state setting */
static void coreaudio_macos_microphone_set_nonblock_state(void *data, bool state)
{
   coreaudio_macos_microphone_t *mic = (coreaudio_macos_microphone_t*)data;
   if (!mic)
      return;

   mic->nonblock = state;
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

/* AudioUnit render callback - optimized for low CPU usage */
static OSStatus coreaudio_macos_input_callback(void *inRefCon,
                                             AudioUnitRenderActionFlags *ioActionFlags,
                                             const AudioTimeStamp *inTimeStamp,
                                             UInt32 inBusNumber,
                                             UInt32 inNumberFrames,
                                             AudioBufferList *ioData)
{
    coreaudio_macos_microphone_t *mic = (coreaudio_macos_microphone_t*)inRefCon;
    if (!mic || !atomic_load_explicit(&mic->is_running, memory_order_relaxed))
        return noErr;
    
    /* Calculate buffer size needed for this callback */
    size_t bytes_needed = inNumberFrames * mic->format.mBytesPerFrame;
    if (bytes_needed == 0)
        return noErr;
    
    /* Use a temporary buffer for AudioUnitRender - zero-initialized to avoid random data */
    void *temp_buffer = calloc(1, bytes_needed);
    if (!temp_buffer)
    {
        RARCH_ERR("[CoreAudio macOS Mic]: Failed to allocate temporary buffer\n");
        return kAudio_MemFullError;
    }
    
    /* Set up buffer list for rendering */
    AudioBufferList buffer_list;
    buffer_list.mNumberBuffers = 1;
    buffer_list.mBuffers[0].mDataByteSize = (UInt32)bytes_needed;
    buffer_list.mBuffers[0].mData = temp_buffer;
    buffer_list.mBuffers[0].mNumberChannels = mic->format.mChannelsPerFrame;
    
    /* Render audio from INPUT BUS (bus 1) */
    OSStatus status = AudioUnitRender(mic->audio_unit,
                                     ioActionFlags,
                                     inTimeStamp,
                                     1, /* Input bus is always 1 for HAL AudioUnits */
                                     inNumberFrames,
                                     &buffer_list);
    
    /* Handle both complete success and partial success cases */
    if (status == noErr || status == kAudioUnitErr_NoConnection)
    {
        /* Only write to FIFO if we got valid data */
        if (buffer_list.mBuffers[0].mData && buffer_list.mBuffers[0].mDataByteSize > 0)
        {
            /* Ensure we don't write more than what was actually rendered */
            size_t actual_bytes = MIN(bytes_needed, buffer_list.mBuffers[0].mDataByteSize);
            
            /* Write all audio data to FIFO - no silence detection to reduce CPU overhead */
            {
                /* Check if there's enough space in the FIFO */
                size_t avail = FIFO_WRITE_AVAIL(mic->fifo);
                if (avail >= actual_bytes)
                {
                    fifo_write(mic->fifo, buffer_list.mBuffers[0].mData, actual_bytes);
                }
                else
                {
                    /* FIFO is full, drop this data to prevent overflow */
                    static unsigned drop_count = 0;
                    if (drop_count++ % 1000 == 0)
                        RARCH_WARN("[CoreAudio macOS Mic]: FIFO full, dropping %u bytes\n", (unsigned)actual_bytes);
                }
            }
        }
    }
    else
    {
        RARCH_ERR("[CoreAudio macOS Mic]: Failed to render audio: %d\n", (int)status);
    }
    
    /* Clean up temporary buffer */
    free(temp_buffer);
    return status;
}

static int coreaudio_macos_microphone_read(void *driver_data, void *mic_data, void *buf, size_t samples)
{
   coreaudio_macos_microphone_t *microphone = (coreaudio_macos_microphone_t *)mic_data;

    size_t avail, read_amt;

    if (!microphone || !buf) {
        RARCH_ERR("[CoreAudio]: Invalid parameters in read\n");
        return -1;
    }

    avail = FIFO_READ_AVAIL(microphone->fifo);
    read_amt = MIN(avail, samples);

    if (microphone->nonblock && read_amt == 0) {
        return 0; /// Return immediately in non-blocking mode
    }

    if (read_amt > 0) {
        fifo_read(microphone->fifo, buf, read_amt);
#if DEBUG
        RARCH_LOG("[CoreAudio]: Read %zu bytes from microphone\n", read_amt);
#endif
    }

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
   
   RARCH_LOG("[CoreAudio macOS Mic]: Format setup: sample_rate=%.0f Hz, bits=%u, bytes_per_frame=%u, %s\n",
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
   RARCH_LOG("[CoreAudio macOS Mic]: device_list_new called.\n");
   struct string_list *list = string_list_new();
   if (!list)
   {
      RARCH_ERR("[CoreAudio macOS Mic]: Failed to create string_list.\n");
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
      RARCH_ERR("[CoreAudio macOS Mic]: Error getting size of device list: %d\n", (int)status);
      string_list_free(list);
      return NULL;
   }

   UInt32 num_devices = propsize / sizeof(AudioDeviceID);
   AudioDeviceID *all_devices = (AudioDeviceID *)malloc(propsize);
   if (!all_devices)
   {
      RARCH_ERR("[CoreAudio macOS Mic]: Failed to allocate memory for device list.\n");
      string_list_free(list);
      return NULL;
   }

   status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop_addr_devices, 0, NULL, &propsize, all_devices);
   if (status != noErr)
   {
      RARCH_ERR("[CoreAudio macOS Mic]: Error getting device list: %d\n", (int)status);
      free(all_devices);
      string_list_free(list);
      return NULL;
   }

   RARCH_LOG("[CoreAudio macOS Mic]: Found %u total audio devices.\n", num_devices);

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
      {
         input_stream_count = propsize / sizeof(AudioStreamID); /* Can be more than 1, but we just need > 0 */
      }

      if (input_stream_count > 0)
      {
         /* This device has input streams, get its name and UID */
         CFStringRef device_name_cf = NULL;
         propsize = sizeof(CFStringRef);
         AudioObjectPropertyAddress prop_addr_name = {
            kAudioDevicePropertyDeviceNameCFString,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
         };
         status = AudioObjectGetPropertyData(current_device_id, &prop_addr_name, 0, NULL, &propsize, &device_name_cf);

         CFStringRef device_uid_cf = NULL;
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
            char device_uid_c[256] = {0};

            CFStringGetCString(device_name_cf, device_name_c, sizeof(device_name_c), kCFStringEncodingUTF8);
            CFStringGetCString(device_uid_cf, device_uid_c, sizeof(device_uid_c), kCFStringEncodingUTF8);

            union string_list_elem_attr attr_sl;
            attr_sl.p = strdup(device_uid_c); /* strdup to give ownership to the list item */
            if (!attr_sl.p)
            {
               RARCH_ERR("[CoreAudio macOS Mic]: Failed to strdup device UID '%s'.\n", device_uid_c);
               /* Potentially continue without this device or handle error more globally */
            }
            else if (!string_list_append(list, device_name_c, attr_sl))
            {
               RARCH_ERR("[CoreAudio macOS Mic]: Failed to append device '%s' to list.\n", device_name_c);
            }
            else
            {
               RARCH_LOG("[CoreAudio macOS Mic]: Added input device: '%s' (UID: '%s')\n", device_name_c, device_uid_c);
            }

         }
         if (device_name_cf) CFRelease(device_name_cf);
         if (device_uid_cf) CFRelease(device_uid_cf);
      }
   }

   free(all_devices);

   if (list->size == 0)
   {
      RARCH_WARN("[CoreAudio macOS Mic]: No input devices found after filtering. Will use system default if available.\n");
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
   RARCH_LOG("[CoreAudio macOS Mic]: device_list_free called.\n");
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
   RARCH_LOG("[CoreAudio macOS Mic]: *** OPEN_MIC CALLED *** Device: %s, Rate: %u, Latency: %u. Driver context: %p\n", device ? device : "default", rate, latency, data);
   RARCH_LOG("[CoreAudio macOS Mic]: IMPORTANT - Requested sample rate from core: %u Hz\n", rate);
   (void)data; /* Driver context from init(), not directly used to create the mic instance */
   /* The 'data' parameter is the driver_context returned by init(). */
   /* For this driver, init() returns a placeholder (void*)1, so we don't use 'data' to allocate the mic instance. */
   /* The actual mic instance (coreaudio_macos_microphone_t) is allocated below. */


   coreaudio_macos_microphone_t *mic = (coreaudio_macos_microphone_t *)calloc(1, sizeof(coreaudio_macos_microphone_t));
   if (!mic)
   {
      RARCH_ERR("[CoreAudio macOS Mic]: Failed to allocate memory for microphone context.\n");
      return NULL;
   }

   atomic_init(&mic->is_running, false);
   atomic_init(&mic->is_initialized, false);
   mic->sample_rate = rate;
   RARCH_LOG("[CoreAudio macOS Mic]: Setting mic->sample_rate to %u Hz\n", mic->sample_rate);
   if (device && strlen(device) > 0 && strcmp(device, "default") != 0)
   {
      mic->device_name = strdup(device);
      mic->selected_device_id = get_macos_device_id_for_uid_or_name(mic->device_name);
      RARCH_LOG("[CoreAudio macOS Mic]: Requested device '%s', selected AudioDeviceID: %u\n", mic->device_name, (unsigned int)mic->selected_device_id);
   }
   else
   {
      mic->device_name = strdup("default");
      mic->selected_device_id = kAudioObjectUnknown; /* System default */
      RARCH_LOG("[CoreAudio macOS Mic]: Requested default device, selected AudioDeviceID: %u (system default)\n", (unsigned int)mic->selected_device_id);
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
      RARCH_ERR("[CoreAudio macOS Mic]: Failed to find HALOutput AudioComponent.\n");
      if (mic->device_name) free(mic->device_name);
      
      free(mic);
      return NULL;
   }

   // Log information about the found AudioComponent
   CFStringRef compNameRef = NULL;
   OSStatus nameStatus = AudioComponentCopyName(comp, &compNameRef);
   if (nameStatus == noErr && compNameRef)
   {
       char compNameCStr[256] = {0};
       Boolean cstrResult = CFStringGetCString(compNameRef, compNameCStr, sizeof(compNameCStr), kCFStringEncodingUTF8);
       if (cstrResult)
          RARCH_LOG("[CoreAudio macOS Mic]: Found AudioComponent with name: %s (pointer: %p)\n", compNameCStr, (void*)comp);
       else
          RARCH_WARN("[CoreAudio macOS Mic]: Found AudioComponent (pointer: %p), but failed to convert its name to CString.\n", (void*)comp);
       CFRelease(compNameRef);
   }
   else
   {
       RARCH_WARN("[CoreAudio macOS Mic]: Found AudioComponent (pointer: %p), but could not get its name. Status: %d\n", (void*)comp, (int)nameStatus);
   }

   RARCH_LOG("[CoreAudio macOS Mic]: Attempting to create AudioUnit instance from found component %p...\n", (void*)comp);
   status = AudioComponentInstanceNew(comp, &mic->audio_unit);
   if (status != noErr || !mic->audio_unit)
   {
      RARCH_ERR("[CoreAudio macOS Mic]: Failed to create AudioUnit instance: %d\n", (int)status);
      if (mic->device_name) free(mic->device_name);
      free(mic);
      return NULL;
   }
   RARCH_LOG("[CoreAudio macOS Mic]: AudioUnit instance created: %p\n", mic->audio_unit);
   
   /* Set the specific audio device if one was requested (not default) */
   if (mic->selected_device_id != kAudioObjectUnknown)
   {
      RARCH_LOG("[CoreAudio macOS Mic]: Setting AudioUnit to use selected device ID: %u\n", (unsigned int)mic->selected_device_id);
      status = AudioUnitSetProperty(mic->audio_unit,
                              kAudioOutputUnitProperty_CurrentDevice,
                              kAudioUnitScope_Global,
                              0, /* Global scope uses element 0 */
                              &mic->selected_device_id,
                              sizeof(AudioDeviceID));
      if (status != noErr)
      {
         RARCH_WARN("[CoreAudio macOS Mic]: Failed to set device ID on AudioUnit: %d. Will fall back to system default.\n", (int)status);
      }
      else
      {
         RARCH_LOG("[CoreAudio macOS Mic]: Successfully set device ID %u on AudioUnit\n", (unsigned int)mic->selected_device_id);
      }
   }
   else
   {
      RARCH_LOG("[CoreAudio macOS Mic]: Using system default audio input device\n");
   }

   /* 2. Enable input on the AudioUnit - CRITICAL STEP */
   UInt32 enable_io = 1;
   
   /* Enable input on input bus */
   status = AudioUnitSetProperty(mic->audio_unit,
                            kAudioOutputUnitProperty_EnableIO,
                            kAudioUnitScope_Input,
                            1, /* Input element/bus */
                            &enable_io,
                            sizeof(enable_io));
   if (status != noErr)
   {
      RARCH_ERR("[CoreAudio macOS Mic]: Failed to enable input on AudioUnit: %d\n", (int)status);
      AudioComponentInstanceDispose(mic->audio_unit);
      if (mic->device_name) free(mic->device_name);
      
      free(mic);
      return NULL;
   }
   RARCH_LOG("[CoreAudio macOS Mic]: Successfully enabled input on AudioUnit\n");

   /* Disable output on output bus since we only need input */
   UInt32 disable_io = 0;
   status = AudioUnitSetProperty(mic->audio_unit,
                            kAudioOutputUnitProperty_EnableIO,
                            kAudioUnitScope_Output,
                            0, /* Output element/bus */
                            &disable_io,
                            sizeof(disable_io));
   if (status != noErr)
   {
      RARCH_WARN("[CoreAudio macOS Mic]: Failed to disable output on AudioUnit: %d. This may be OK.\n", (int)status);
      /* Not fatal, some audio devices might not even support output */
   }
   else
   {
      RARCH_LOG("[CoreAudio macOS Mic]: Successfully disabled output on AudioUnit\n");
   }

   /* 3. Set the current device if a specific one was selected */
   if (mic->selected_device_id != kAudioObjectUnknown)
   {
      RARCH_LOG("[CoreAudio macOS Mic]: Attempting to set current device to AudioDeviceID: %u\n", (unsigned int)mic->selected_device_id);
      status = AudioUnitSetProperty(mic->audio_unit,
                                  kAudioOutputUnitProperty_CurrentDevice,
                                  kAudioUnitScope_Global, /* Some docs say Input scope, some Global. Global is often for HALOutput device selection. */
                                  0, /* Element 0 for global properties */
                                  &mic->selected_device_id,
                                  sizeof(mic->selected_device_id));
      if (status != noErr)
      {
         RARCH_ERR("[CoreAudio macOS Mic]: Failed to set current device on AudioUnit (ID: %u). Error: %d. Will use system default.\n", (unsigned int)mic->selected_device_id, (int)status);
         /* Proceed with system default if setting specific device fails */
         mic->selected_device_id = kAudioObjectUnknown; /* Fallback to default indication */
      }
      else
      {
         RARCH_LOG("[CoreAudio macOS Mic]: Successfully set current device to AudioDeviceID: %u\n", (unsigned int)mic->selected_device_id);
      }
   }
   else
   {
      RARCH_LOG("[CoreAudio macOS Mic]: Using system default input device.\n");
   }

   /* Query and log the actual current device the AudioUnit is using */
   AudioDeviceID actual_device_id_check = kAudioObjectUnknown;
   UInt32 actual_device_prop_size = sizeof(AudioDeviceID);
   OSStatus query_status = AudioUnitGetProperty(mic->audio_unit,
                                             kAudioOutputUnitProperty_CurrentDevice,
                                             kAudioUnitScope_Global,
                                             0, /* Element 0 for global properties */
                                             &actual_device_id_check,
                                             &actual_device_prop_size);

   if (query_status == noErr)
   {
      RARCH_LOG("[CoreAudio macOS Mic]: Queried AudioUnit's effective current device ID: %u\n", (unsigned int)actual_device_id_check);
      if (mic->selected_device_id != kAudioObjectUnknown && mic->selected_device_id != actual_device_id_check)
      {
         RARCH_WARN("[CoreAudio macOS Mic]: AudioUnit's effective device ID (%u) differs from initially set/intended ID (%u)!\n",
                    (unsigned int)actual_device_id_check, (unsigned int)mic->selected_device_id);
      }
      /* Update mic->selected_device_id to reflect the actual one, especially if it was default or fallback */
      mic->selected_device_id = actual_device_id_check; 
   }
   else
   {
      RARCH_ERR("[CoreAudio macOS Mic]: Failed to query AudioUnit's current device ID. Error: %d. This could be problematic.\n", (int)query_status);
      /* Not necessarily fatal here, as subsequent operations might still use a default, but it's a bad sign. */
   }

    /* 4. Set stream format */
    coreaudio_macos_microphone_set_format(mic, false /* use int16 for better compatibility */);
    
    RARCH_LOG("[CoreAudio macOS Mic]: After format setup - mic->format.mSampleRate = %.0f Hz\n", mic->format.mSampleRate);
    
    /* First get the device's native format from the INPUT scope, INPUT bus (bus 1) */
    AudioStreamBasicDescription device_format = {0};
    UInt32 prop_size = sizeof(device_format);
    OSStatus format_status = AudioUnitGetProperty(mic->audio_unit,
                            kAudioUnitProperty_StreamFormat,
                            kAudioUnitScope_Input,
                            1, /* Bus 1 (Input bus) - this represents data FROM the input device */
                            &device_format,
                            &prop_size);
    if (format_status != noErr) {
        RARCH_WARN("[CoreAudio macOS Mic]: Could not get device native format from input bus: %d. Using default format.\n", (int)format_status);
    } else {
       RARCH_LOG("[CoreAudio macOS Mic]: Device native format (input bus) - Sample rate: %.0f, Channels: %u, Bits: %u\n", 
                device_format.mSampleRate, 
                (unsigned)device_format.mChannelsPerFrame,
                (unsigned)device_format.mBitsPerChannel);
                
       /* Use the device's native sample rate but ALWAYS use mono for better compatibility */
       mic->format.mSampleRate = device_format.mSampleRate;
       mic->format.mChannelsPerFrame = 1; /* Always force mono regardless of device capabilities */
       
       /* Update mic->channels to match what we're actually using */
       mic->channels = device_format.mChannelsPerFrame;
       
       /* Re-calculate format bytes per frame to match the channel count */
       mic->format.mBytesPerFrame = mic->format.mChannelsPerFrame * (mic->format.mBitsPerChannel / 8);
       mic->format.mFramesPerPacket = 1;
       mic->format.mBytesPerPacket = mic->format.mBytesPerFrame * mic->format.mFramesPerPacket;
       
       RARCH_LOG("[CoreAudio macOS Mic]: Updated format - SR: %.0f, CH: %u, BitsPerCh: %u, BytesPerFrame: %u\n",
                mic->format.mSampleRate, 
                (unsigned)mic->format.mChannelsPerFrame,
                (unsigned)mic->format.mBitsPerChannel,
                (unsigned)mic->format.mBytesPerFrame);
    }
    
    RARCH_LOG("[CoreAudio macOS Mic]: Setting client format on OUTPUT scope of INPUT bus\n");
    OSStatus set_format_status = AudioUnitSetProperty(mic->audio_unit,
                               kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Output, /* Output scope - what we receive */
                               1, /* Bus 1 (Input bus) - where we receive input data */
                               &mic->format,
                               sizeof(AudioStreamBasicDescription));
    if (set_format_status != noErr)
    {
       RARCH_ERR("[CoreAudio macOS Mic]: Failed to set client stream format: %d\n", (int)set_format_status);
       AudioComponentInstanceDispose(mic->audio_unit);
       if (mic->device_name) free(mic->device_name);
       
       free(mic);
       return NULL;
    }
   
   /* Set up input callback */
   AURenderCallbackStruct callback_struct;
   callback_struct.inputProc       = coreaudio_macos_input_callback;
   callback_struct.inputProcRefCon = mic;
   
   status = AudioUnitSetProperty(mic->audio_unit,
                              kAudioOutputUnitProperty_SetInputCallback, 
                              kAudioUnitScope_Global, 
                              0, 
                              &callback_struct,
                              sizeof(callback_struct));
   if (status != noErr)
   {
      RARCH_ERR("[CoreAudio macOS Mic]: Failed to set INPUT callback: %d\n", (int)status);
      AudioComponentInstanceDispose(mic->audio_unit);
      if (mic->device_name) free(mic->device_name);
      
      free(mic);
      return NULL;
   }
   
   RARCH_LOG("[CoreAudio macOS Mic]: Initializing AudioUnit %p\n", mic->audio_unit);
   
   /* Set a smaller buffer frame size for lower latency */
    UInt32 buffer_frame_size = 256; /* Small buffer for lower latency */
    status = AudioUnitSetProperty(mic->audio_unit,
                               kAudioDevicePropertyBufferFrameSize,
                               kAudioUnitScope_Global,
                               0,
                               &buffer_frame_size,
                               sizeof(buffer_frame_size));
    if (status != noErr)
    {
        RARCH_WARN("[CoreAudio macOS Mic]: Failed to set buffer frame size to %u: %d\n", 
                   (unsigned)buffer_frame_size, (int)status);
        /* Non-fatal, continue with default buffer size */
    }
    else
    {
        RARCH_LOG("[CoreAudio macOS Mic]: Set buffer frame size to %u frames for lower latency\n", 
                 (unsigned)buffer_frame_size);
    }
    
    status = AudioUnitInitialize(mic->audio_unit);
    if (status != noErr)
    {
        RARCH_ERR("[CoreAudio macOS Mic]: Failed to initialize AudioUnit: %d\n", (int)status);
        AudioComponentInstanceDispose(mic->audio_unit);
        if (mic->device_name) free(mic->device_name);
        free(mic);
        return NULL;
    }
   atomic_store(&mic->is_initialized, true);
   RARCH_LOG("[CoreAudio macOS Mic]: AudioUnit successfully initialized.\n");

    /* Initialize FIFO buffer - 50ms buffer size */
    size_t fifo_size = mic->format.mSampleRate * mic->format.mBytesPerFrame * 0.05f;
    RARCH_LOG("[CoreAudio macOS Mic]: Creating FIFO buffer of size %u bytes (%.1f ms at %.0f Hz)\n", 
             (unsigned)fifo_size, 
             (float)fifo_size * 1000.0f / (mic->format.mSampleRate * mic->format.mBytesPerFrame),
             mic->format.mSampleRate);
             
    /* Create and initialize FIFO buffer */
    mic->fifo = fifo_new(fifo_size);
    if (!mic->fifo)
    {
       RARCH_ERR("[CoreAudio macOS Mic]: Failed to allocate FIFO buffer\n");
       AudioUnitUninitialize(mic->audio_unit);
       AudioComponentInstanceDispose(mic->audio_unit);
       if (mic->device_name) free(mic->device_name);
       
       free(mic);
       return NULL;
    }
    
    /* Explicitly clear the FIFO buffer to ensure no random data */
    fifo_clear(mic->fifo);
    RARCH_LOG("[CoreAudio macOS Mic]: FIFO buffer initialized and cleared\n");

    /* Allocate AudioBufferList for AudioUnitRender in the callback */
    
    /* We don't need to pre-allocate buffer list or calculate max frames per slice
     * since we're using temporary buffers in the callback like the iOS version */

    RARCH_LOG("[CoreAudio macOS Mic]: COMPLETE CONFIG - Sample rate: %.0f Hz, Format: %s, Channels: %u\n", 
             mic->format.mSampleRate, 
             mic->use_float ? "Float" : "Int16", 
             (unsigned)mic->format.mChannelsPerFrame);

    if (new_rate)
       *new_rate = (unsigned)mic->format.mSampleRate; /* Reflect actual rate used */

    RARCH_LOG("[CoreAudio macOS Mic]: Microphone instance %p fully configured and initialized for device '%s'. Ready to start.\n", mic, mic->device_name ? mic->device_name : "default");
    return mic;
}

static void coreaudio_macos_microphone_close_mic(void *data, void *mic_data)
{
   RARCH_LOG("[CoreAudio macOS Mic]: close_mic called. Mic context: %p\n", mic_data);
   coreaudio_macos_microphone_t *mic = (coreaudio_macos_microphone_t *)mic_data;
   if (!mic)
      return;

   coreaudio_macos_microphone_stop_mic(data, mic_data); /* Ensure it's stopped */

   if (mic->audio_unit)
   {
      if (atomic_load(&mic->is_initialized))
      {
         RARCH_LOG("[CoreAudio macOS Mic]: Uninitializing AudioUnit %p\n", mic->audio_unit);
         AudioUnitUninitialize(mic->audio_unit);
         atomic_store(&mic->is_initialized, false);
      }
      AudioComponentInstanceDispose(mic->audio_unit);
      mic->audio_unit = NULL;
   }

   /* No buffer list cleanup needed since we're using temporary buffers */

   

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
   free(mic);
   RARCH_LOG("[CoreAudio macOS Mic]: Microphone instance %p freed.\n", mic_data);
}

static bool coreaudio_macos_microphone_mic_alive(const void *data, const void *mic_data)
{
   const coreaudio_macos_microphone_t *mic = (const coreaudio_macos_microphone_t *)mic_data;
   return mic && atomic_load(&mic->is_running);
}

static bool coreaudio_macos_microphone_start_mic(void *data, void *mic_data)
{
   RARCH_LOG("[CoreAudio macOS Mic]: start_mic called. Mic context: %p\n", mic_data);
   coreaudio_macos_microphone_t *mic = (coreaudio_macos_microphone_t *)mic_data;
   if (!mic || !mic->audio_unit || !atomic_load(&mic->is_initialized))
   {
      RARCH_ERR("[CoreAudio macOS Mic]: Cannot start - mic not opened or AudioUnit not initialized.\n");
      return false;
   }

   if (atomic_load(&mic->is_running))
   {
      RARCH_LOG("[CoreAudio macOS Mic]: Already running.\n");
      return true;
   }

   /* Check microphone permission on macOS */
   RARCH_LOG("[CoreAudio macOS Mic]: Checking microphone permission...\n");
   
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
      RARCH_ERR("[CoreAudio macOS Mic]: No default input device available or permission denied. Status: %d, Device ID: %u\n", 
               (int)perm_status, (unsigned)default_input_device);
   }
   else
   {
      RARCH_LOG("[CoreAudio macOS Mic]: Default input device ID: %u\n", (unsigned)default_input_device);
   }

   /* Make sure FIFO is completely cleared before starting */
   if (mic->fifo)
   {
       fifo_clear(mic->fifo);
       RARCH_LOG("[CoreAudio macOS Mic]: FIFO buffer cleared before starting\n");
   }
   else
   {
       RARCH_ERR("[CoreAudio macOS Mic]: No FIFO buffer available\n");
       return false;
   }
   
   OSStatus status = AudioOutputUnitStart(mic->audio_unit);
   if (status == noErr)
   {
      atomic_store(&mic->is_running, true);
      RARCH_LOG("[CoreAudio macOS Mic]: Microphone started successfully\n");
      return true;
   }
   else
   {
      RARCH_ERR("[CoreAudio macOS Mic]: Failed to start AudioUnit: %d (0x%x)\n", (int)status, (unsigned)status);
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
         fifo_clear(mic->fifo);
      }
      return true;
   }
   else
   {
      RARCH_ERR("[CoreAudio macOS Mic]: Failed to stop AudioUnit: %d\n", (int)status);
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
   RARCH_LOG("[CoreAudio macOS Mic]: get_macos_device_id_for_uid_or_name looking for '%s'\n", uid_or_name ? uid_or_name : "(null - will use default)");
   if (!uid_or_name || strlen(uid_or_name) == 0 || strcmp(uid_or_name, "default") == 0)
   {
      RARCH_LOG("[CoreAudio macOS Mic]: Requested default device or empty name. Returning kAudioObjectUnknown to signify system default.\n");
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
      RARCH_ERR("[CoreAudio macOS Mic]: Error getting size of device list for UID lookup: %d\n", (int)status);
      return kAudioObjectUnknown;
   }

   UInt32 num_devices = propsize / sizeof(AudioDeviceID);
   AudioDeviceID *all_devices = (AudioDeviceID *)malloc(propsize);
   if (!all_devices)
   {
      RARCH_ERR("[CoreAudio macOS Mic]: Failed to allocate memory for device list for UID lookup.\n");
      return kAudioObjectUnknown;
   }

   status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop_addr_devices, 0, NULL, &propsize, all_devices);
   if (status != noErr)
   {
      RARCH_ERR("[CoreAudio macOS Mic]: Error getting device list for UID lookup: %d\n", (int)status);
      free(all_devices);
      return kAudioObjectUnknown;
   }

   AudioDeviceID found_device_id = kAudioObjectUnknown;

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
               RARCH_LOG("[CoreAudio macOS Mic]: Found device by UID: '%s' (ID: %u)\n", uid_or_name, (unsigned int)found_device_id);
               goto end_loop; /* Found by UID, no need to check name */
            }
         }
         CFRelease(device_uid_cf);
      }
   }

   /* If not found by UID, try by name (less reliable) */
   RARCH_LOG("[CoreAudio macOS Mic]: Device not found by UID '%s'. Trying by name as fallback.\n", uid_or_name);
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
               RARCH_LOG("[CoreAudio macOS Mic]: Found device by NAME: '%s' (ID: %u)\n", uid_or_name, (unsigned int)found_device_id);
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
       RARCH_WARN("[CoreAudio macOS Mic]: Device UID/Name '%s' not found. Will use system default.\n", uid_or_name);
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

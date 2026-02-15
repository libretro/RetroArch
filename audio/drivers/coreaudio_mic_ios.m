/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2025 - Joseph Mattiello
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 */

#import <AudioToolbox/AudioToolbox.h>
#import <AVFoundation/AVFoundation.h>
#include "audio/microphone_driver.h"
#include "queues/fifo_queue.h"
#include "verbosity.h"
#include <memory.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <rthreads/rthreads.h>

#include "audio/audio_driver.h"
#include "../../verbosity.h"

typedef struct coreaudio_microphone
{
    AudioUnit audio_unit;
    AudioStreamBasicDescription format;
    fifo_buffer_t *sample_buffer;
    bool is_running;
    bool nonblock;
    int sample_rate;
    bool use_float;
    slock_t *fifo_lock;
    scond_t *fifo_cond;
    void *callback_buffer;
    size_t callback_buffer_size;
    unsigned drop_count;
} coreaudio_microphone_t;

/* Callback for receiving audio samples — runs on the real-time audio thread. */
static OSStatus coreaudio_input_callback(
    void *inRefCon,
    AudioUnitRenderActionFlags *ioActionFlags,
    const AudioTimeStamp *inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList *ioData)
{
    coreaudio_microphone_t *microphone = (coreaudio_microphone_t*)inRefCon;
    AudioBufferList bufferList;
    OSStatus status;
    size_t bufferSize;
    size_t actual_bytes;

    if (!microphone || !microphone->is_running)
        return noErr;

    bufferSize = inNumberFrames * microphone->format.mBytesPerFrame;
    if (bufferSize == 0)
        return noErr;

    /* Use pre-allocated buffer; drop this chunk if it doesn't fit rather
     * than allocating on the real-time thread. */
    if (bufferSize > microphone->callback_buffer_size)
        return noErr;

    memset(microphone->callback_buffer, 0, bufferSize);

    bufferList.mNumberBuffers              = 1;
    bufferList.mBuffers[0].mDataByteSize   = (UInt32)bufferSize;
    bufferList.mBuffers[0].mData           = microphone->callback_buffer;
    bufferList.mBuffers[0].mNumberChannels = microphone->format.mChannelsPerFrame;

    status = AudioUnitRender(microphone->audio_unit, ioActionFlags,
          inTimeStamp, inBusNumber, inNumberFrames, &bufferList);

    if (status == noErr
          && bufferList.mBuffers[0].mData
          && bufferList.mBuffers[0].mDataByteSize > 0)
    {
        actual_bytes = MIN(bufferSize,
              bufferList.mBuffers[0].mDataByteSize);

        /* Use trylock so we never block the real-time thread. */
        if (slock_try_lock(microphone->fifo_lock))
        {
            if (FIFO_WRITE_AVAIL(microphone->sample_buffer) >= actual_bytes)
                fifo_write(microphone->sample_buffer,
                      bufferList.mBuffers[0].mData, actual_bytes);
            else if (microphone->drop_count++ % 1000 == 0)
                RARCH_WARN("[CoreAudio] FIFO full, dropping %u bytes.\n",
                           (unsigned)actual_bytes);
            scond_signal(microphone->fifo_cond);
            slock_unlock(microphone->fifo_lock);
        }
    }
    else if (status != noErr)
        RARCH_ERR("[CoreAudio] Failed to render audio: %d.\n", (int)status);

    return noErr;
}

/* Initialize CoreAudio microphone driver */
static void *coreaudio_microphone_init(void)
{
   coreaudio_microphone_t *microphone = (coreaudio_microphone_t*)calloc(1, sizeof(*microphone));
   if (!microphone)
   {
      RARCH_ERR("[CoreAudio] Failed to allocate microphone driver.\n");
      return NULL;
   }

   microphone->sample_rate = 0;
   microphone->nonblock    = false;
   microphone->use_float   = false;
   microphone->fifo_lock = slock_new();
   microphone->fifo_cond = scond_new();

   return microphone;
}

/* Free CoreAudio microphone driver */
static void coreaudio_microphone_free(void *driver_context)
{
   coreaudio_microphone_t *microphone = (coreaudio_microphone_t*)driver_context;
   if (microphone)
   {
      if (microphone->audio_unit)
      {
         if (microphone->is_running)
         {
            AudioOutputUnitStop(microphone->audio_unit);
            microphone->is_running = false;
         }
         AudioUnitUninitialize(microphone->audio_unit);
         AudioComponentInstanceDispose(microphone->audio_unit);
         microphone->audio_unit = nil;
      }
      if (microphone->callback_buffer)
         free(microphone->callback_buffer);
      if (microphone->sample_buffer)
         fifo_free(microphone->sample_buffer);
      slock_free(microphone->fifo_lock);
      scond_free(microphone->fifo_cond);
      free(microphone);
   }
}

/* Read samples from microphone */
static int coreaudio_microphone_read(void *driver_context,
      void *microphone_context, void *buf, size_t size)
{
   coreaudio_microphone_t *microphone = (coreaudio_microphone_t*)driver_context;
   size_t avail, read_amt;

   if (!microphone || !buf)
   {
      RARCH_ERR("[CoreAudio] Invalid parameters in read.\n");
      return -1;
   }

   slock_lock(microphone->fifo_lock);

   avail    = FIFO_READ_AVAIL(microphone->sample_buffer);
   read_amt = MIN(avail, size);

   /* In blocking mode, wait briefly for the callback to provide data. */
   if (read_amt == 0 && !microphone->nonblock)
   {
      scond_wait_timeout(microphone->fifo_cond,
            microphone->fifo_lock, 10000); /* 10 ms */
      avail    = FIFO_READ_AVAIL(microphone->sample_buffer);
      read_amt = MIN(avail, size);
   }

   if (read_amt > 0)
      fifo_read(microphone->sample_buffer, buf, read_amt);

   slock_unlock(microphone->fifo_lock);

   return (int)read_amt;
}

/* Set non-blocking state */
static void coreaudio_microphone_set_nonblock_state(void *driver_context, bool state)
{
   coreaudio_microphone_t *microphone = (coreaudio_microphone_t*)driver_context;
   if (microphone)
      microphone->nonblock = state;
}

/* Helper method to set audio format */
static void coreaudio_microphone_set_format(coreaudio_microphone_t *microphone, bool use_float)
{
   microphone->use_float           = use_float;
   microphone->format.mSampleRate  = microphone->sample_rate;
   microphone->format.mFormatID    = kAudioFormatLinearPCM;
   microphone->format.mFormatFlags = use_float
      ? (kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked)
      : (kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked);
   microphone->format.mFramesPerPacket  = 1;
   microphone->format.mChannelsPerFrame = 1;
   microphone->format.mBitsPerChannel = use_float ? 32 : 16;
   microphone->format.mBytesPerFrame  = microphone->format.mChannelsPerFrame * microphone->format.mBitsPerChannel / 8;
   microphone->format.mBytesPerPacket = microphone->format.mBytesPerFrame * microphone->format.mFramesPerPacket;

   RARCH_LOG("[CoreAudio] Format setup: sample_rate=%d, bits=%d, bytes_per_frame=%d.\n",
         (int)microphone->format.mSampleRate,
         microphone->format.mBitsPerChannel,
         microphone->format.mBytesPerFrame);
}

/* Open microphone device */
static void *coreaudio_microphone_open_mic(void *driver_context,
      const char *device,
      unsigned rate,
      unsigned latency,
      unsigned *new_rate)
{
   coreaudio_microphone_t *microphone = (coreaudio_microphone_t*)driver_context;
   if (!microphone)
   {
      RARCH_ERR("[CoreAudio] Invalid driver context.\n");
      return NULL;
   }

   /* Guard against calling open_mic twice without close_mic */
   if (microphone->audio_unit)
   {
      RARCH_WARN("[CoreAudio] Microphone already open, closing first.\n");
      if (microphone->is_running)
      {
         AudioOutputUnitStop(microphone->audio_unit);
         microphone->is_running = false;
      }
      AudioUnitUninitialize(microphone->audio_unit);
      AudioComponentInstanceDispose(microphone->audio_unit);
      microphone->audio_unit = nil;
      if (microphone->callback_buffer)
      {
         free(microphone->callback_buffer);
         microphone->callback_buffer = NULL;
      }
      if (microphone->sample_buffer)
      {
         fifo_free(microphone->sample_buffer);
         microphone->sample_buffer = NULL;
      }
   }

   microphone->sample_rate = rate;
   microphone->use_float   = false;

#if TARGET_OS_IPHONE
   /* Configure audio session */
   {
      AVAudioSession *audioSession = [AVAudioSession sharedInstance];
      NSError *error = nil;
      Float64 actualRate;

      [audioSession setCategory:AVAudioSessionCategoryPlayAndRecord
                    withOptions:AVAudioSessionCategoryOptionAllowBluetooth
                              | AVAudioSessionCategoryOptionAllowBluetoothA2DP
                          error:&error];
      if (error)
      {
         RARCH_ERR("[CoreAudio] Failed to set audio session category: %s.\n",
               [[error localizedDescription] UTF8String]);
         return NULL;
      }

      /* Let iOS negotiate the rate — don't restrict to 44100/48000. */
      [audioSession setPreferredSampleRate:rate error:&error];
      if (error)
      {
         RARCH_ERR("[CoreAudio] Failed to set preferred sample rate: %s.\n",
               [[error localizedDescription] UTF8String]);
         return NULL;
      }

      actualRate = [audioSession sampleRate];
      if (new_rate)
         *new_rate = (unsigned)actualRate;
      microphone->sample_rate = (int)actualRate;

      RARCH_LOG("[CoreAudio] Using sample rate: %d Hz.\n", microphone->sample_rate);
   }
#endif

   coreaudio_microphone_set_format(microphone, false);

   /* Calculate FIFO buffer size from latency */
   {
      size_t fifoBufferSize = (size_t)latency * microphone->sample_rate
                            * microphone->format.mBytesPerFrame / 1000;
      if (fifoBufferSize == 0)
      {
         RARCH_WARN("[CoreAudio] FIFO size is 0, using 1024 byte fallback.\n");
         fifoBufferSize = 1024;
      }
      microphone->sample_buffer = fifo_new(fifoBufferSize);
      if (!microphone->sample_buffer)
      {
         RARCH_ERR("[CoreAudio] Failed to create sample buffer.\n");
         return NULL;
      }
   }

   /* Initialize audio unit */
   {
      AudioComponentDescription desc = {
         .componentType = kAudioUnitType_Output,
#if TARGET_OS_IPHONE
         .componentSubType = kAudioUnitSubType_RemoteIO,
#else
         .componentSubType = kAudioUnitSubType_HALOutput,
#endif
         .componentManufacturer = kAudioUnitManufacturer_Apple,
         .componentFlags = 0,
         .componentFlagsMask = 0
      };
      AudioComponent comp = AudioComponentFindNext(NULL, &desc);
      OSStatus status     = AudioComponentInstanceNew(comp, &microphone->audio_unit);
      if (status != noErr)
      {
         RARCH_ERR("[CoreAudio] Failed to create audio unit.\n");
         goto error;
      }
   }

   /* Enable input */
   {
      UInt32 flag    = 1;
      OSStatus status = AudioUnitSetProperty(microphone->audio_unit,
            kAudioOutputUnitProperty_EnableIO,
            kAudioUnitScope_Input, 1,
            &flag, sizeof(flag));
      if (status != noErr)
      {
         RARCH_ERR("[CoreAudio] Failed to enable input.\n");
         goto error;
      }
   }

   /* Set format */
   coreaudio_microphone_set_format(microphone, false);
   {
      OSStatus status = AudioUnitSetProperty(microphone->audio_unit,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Output, 1,
            &microphone->format, sizeof(microphone->format));
      if (status != noErr)
      {
         RARCH_ERR("[CoreAudio] Failed to set format: %d.\n", (int)status);
         goto error;
      }
   }

   /* Set callback */
   {
      AURenderCallbackStruct callback = { coreaudio_input_callback, microphone };
      OSStatus status = AudioUnitSetProperty(microphone->audio_unit,
            kAudioOutputUnitProperty_SetInputCallback,
            kAudioUnitScope_Global, 1,
            &callback, sizeof(callback));
      if (status != noErr)
      {
         RARCH_ERR("[CoreAudio] Failed to set callback.\n");
         goto error;
      }
   }

   /* Initialize audio unit */
   {
      OSStatus status = AudioUnitInitialize(microphone->audio_unit);
      if (status != noErr)
      {
         RARCH_ERR("[CoreAudio] Failed to initialize audio unit: %d.\n", (int)status);
         goto error;
      }
   }

   /* Allocate callback buffer based on max frames per slice */
   {
      UInt32 max_frames    = 4096;
      UInt32 max_frames_sz = sizeof(max_frames);
      AudioUnitGetProperty(microphone->audio_unit,
            kAudioUnitProperty_MaximumFramesPerSlice,
            kAudioUnitScope_Global, 0,
            &max_frames, &max_frames_sz);
      microphone->callback_buffer_size = max_frames
                                       * microphone->format.mBytesPerFrame;
      microphone->callback_buffer = calloc(1, microphone->callback_buffer_size);
      if (!microphone->callback_buffer)
      {
         RARCH_ERR("[CoreAudio] Failed to allocate callback buffer.\n");
         goto error;
      }
   }

   /* Start audio unit */
   {
      OSStatus status = AudioOutputUnitStart(microphone->audio_unit);
      if (status != noErr)
      {
         RARCH_ERR("[CoreAudio] Failed to start audio unit: %d.\n", (int)status);
         goto error;
      }
   }

   microphone->is_running = true;
   return microphone;

error:
   if (microphone->audio_unit)
   {
      AudioUnitUninitialize(microphone->audio_unit);
      AudioComponentInstanceDispose(microphone->audio_unit);
      microphone->audio_unit = nil;
   }
   if (microphone->callback_buffer)
   {
      free(microphone->callback_buffer);
      microphone->callback_buffer = NULL;
   }
   if (microphone->sample_buffer)
   {
      fifo_free(microphone->sample_buffer);
      microphone->sample_buffer = NULL;
   }
   return NULL;
}

/* Close microphone */
static void coreaudio_microphone_close_mic(void *driver_context, void *microphone_context)
{
   coreaudio_microphone_t *microphone = (coreaudio_microphone_t*)microphone_context;
   if (!microphone)
   {
      RARCH_ERR("[CoreAudio] Failed to close microphone.\n");
      return;
   }

   if (microphone->is_running)
   {
      AudioOutputUnitStop(microphone->audio_unit);
      microphone->is_running = false;
   }

   if (microphone->audio_unit)
   {
      AudioUnitUninitialize(microphone->audio_unit);
      AudioComponentInstanceDispose(microphone->audio_unit);
      microphone->audio_unit = nil;
   }

   if (microphone->callback_buffer)
   {
      free(microphone->callback_buffer);
      microphone->callback_buffer = NULL;
   }

   if (microphone->sample_buffer)
   {
      fifo_free(microphone->sample_buffer);
      microphone->sample_buffer = NULL;
   }
}

/* Start microphone */
static bool coreaudio_microphone_start_mic(void *driver_context, void *microphone_context)
{
   coreaudio_microphone_t *microphone = (coreaudio_microphone_t*)microphone_context;
   if (!microphone)
   {
      RARCH_ERR("[CoreAudio] Failed to start microphone.\n");
      return false;
   }

   if (microphone->sample_buffer)
   {
      slock_lock(microphone->fifo_lock);
      fifo_clear(microphone->sample_buffer);
      slock_unlock(microphone->fifo_lock);
   }

   {
      OSStatus status = AudioOutputUnitStart(microphone->audio_unit);
      if (status == noErr)
      {
         microphone->is_running = true;
         return true;
      }
      RARCH_ERR("[CoreAudio] Failed to start microphone: %d.\n", (int)status);
   }
   return false;
}

/* Stop microphone */
static bool coreaudio_microphone_stop_mic(void *driver_context, void *microphone_context)
{
   coreaudio_microphone_t *microphone = (coreaudio_microphone_t*)microphone_context;
   if (!microphone)
   {
      RARCH_ERR("[CoreAudio] Failed to stop microphone.\n");
      return false;
   }

   if (microphone->is_running)
   {
      OSStatus status = AudioOutputUnitStop(microphone->audio_unit);
      if (status == noErr)
      {
         microphone->is_running = false;
         if (microphone->sample_buffer)
         {
            slock_lock(microphone->fifo_lock);
            fifo_clear(microphone->sample_buffer);
            slock_unlock(microphone->fifo_lock);
         }
         return true;
      }
      RARCH_ERR("[CoreAudio] Failed to stop microphone: %d.\n", (int)status);
   }
   return true; /* Already stopped */
}

/* Check if microphone is alive */
static bool coreaudio_microphone_mic_alive(const void *driver_context, const void *microphone_context)
{
    coreaudio_microphone_t *microphone = (coreaudio_microphone_t*)microphone_context;
    return microphone && microphone->is_running;
}

/* Check if microphone uses float samples */
static bool coreaudio_microphone_mic_use_float(const void *driver_context, const void *microphone_context)
{
   coreaudio_microphone_t *microphone = (coreaudio_microphone_t*)microphone_context;
   return microphone && microphone->use_float;
}

/* Get device list (not implemented for iOS) */
static struct string_list *coreaudio_microphone_device_list_new(const void *driver_context)
{
    return NULL;
}

/* Free device list (not implemented for iOS) */
static void coreaudio_microphone_device_list_free(const void *driver_context, struct string_list *devices)
{
}

/* CoreAudio microphone driver structure */
microphone_driver_t microphone_coreaudio = {
    coreaudio_microphone_init,
    coreaudio_microphone_free,
    coreaudio_microphone_read,
    coreaudio_microphone_set_nonblock_state,
    "coreaudio",
    coreaudio_microphone_device_list_new,
    coreaudio_microphone_device_list_free,
    coreaudio_microphone_open_mic,
    coreaudio_microphone_close_mic,
    coreaudio_microphone_mic_alive,
    coreaudio_microphone_start_mic,
    coreaudio_microphone_stop_mic,
    coreaudio_microphone_mic_use_float
};

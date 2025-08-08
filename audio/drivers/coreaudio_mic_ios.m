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
#include <stdlib.h>

#include "audio/audio_driver.h"
#include "../../verbosity.h"

typedef struct coreaudio_microphone
{
    AudioUnit audio_unit; /* CoreAudio audio unit */
    AudioStreamBasicDescription format; /* Audio format */
    fifo_buffer_t *sample_buffer; /* Sample buffer */
    bool is_running; /* Whether the microphone is running */
    bool nonblock; /* Non-blocking mode flag */
    int sample_rate; /* Current sample rate */
    bool use_float; /* Whether to use float format */
} coreaudio_microphone_t;

/* Callback for receiving audio samples */
static OSStatus coreaudio_input_callback(
    void *inRefCon,
    AudioUnitRenderActionFlags *ioActionFlags,
    const AudioTimeStamp *inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList *ioData)
{
    OSStatus status;
    AudioBufferList bufferList;
    void *tempBuffer                   = NULL;
    coreaudio_microphone_t *microphone = (coreaudio_microphone_t*)inRefCon;
    /* Calculate required buffer size */
    size_t bufferSize = inNumberFrames * microphone->format.mBytesPerFrame;
    if (bufferSize == 0)
    {
       RARCH_ERR("[CoreAudio] Invalid buffer size calculation.\n");
       return kAudio_ParamError;
    }

    /* Allocate temporary buffer */
    tempBuffer = malloc(bufferSize);
    if (!tempBuffer)
    {
       RARCH_ERR("[CoreAudio] Failed to allocate temporary buffer.\n");
       return kAudio_MemFullError;
    }

    /* Set up buffer list */
    bufferList.mNumberBuffers = 1;
    bufferList.mBuffers[0].mDataByteSize = (UInt32)bufferSize;
    bufferList.mBuffers[0].mData = tempBuffer;

    /* Render audio data */
    status = AudioUnitRender(microphone->audio_unit,
                           ioActionFlags,
                           inTimeStamp,
                           inBusNumber,
                           inNumberFrames,
                           &bufferList);

    /* Write to FIFO buffer */
    if (status == noErr)
        fifo_write(microphone->sample_buffer,
                  bufferList.mBuffers[0].mData,
                  bufferList.mBuffers[0].mDataByteSize);
    else
    {
        RARCH_ERR("[CoreAudio] Failed to render audio: %d.\n", status);
    }

    /* Clean up temporary buffer */
    free(tempBuffer);
    return status;
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

   /* Default sample rate will be set during open_mic */
   microphone->sample_rate = 0;
   microphone->nonblock    = false;
   microphone->use_float   = false;

   return microphone;
}

/* Free CoreAudio microphone driver */
static void coreaudio_microphone_free(void *driver_context)
{
   coreaudio_microphone_t *microphone = (coreaudio_microphone_t*)driver_context;
   if (microphone)
   {
      if (microphone->audio_unit && microphone->is_running)
      {
         AudioOutputUnitStop(microphone->audio_unit);
         microphone->is_running = false;
      }

      /* TODO: This crashes, though we protect calls around `audio_unit` nil! */
#if 0
      if (microphone->audio_unit)
      {
         AudioComponentInstanceDispose(microphone->audio_unit);
         microphone->audio_unit = nil;
      }
#endif
      if (microphone->sample_buffer)
         fifo_free(microphone->sample_buffer);
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

   avail    = FIFO_READ_AVAIL(microphone->sample_buffer);
   read_amt = MIN(avail, size);

   if (microphone->nonblock && read_amt == 0)
      return 0; /* Return immediately in non-blocking mode */

   if (read_amt > 0)
   {
      fifo_read(microphone->sample_buffer, buf, read_amt);
#if DEBUG
      RARCH_LOG("[CoreAudio] Read %zu bytes from microphone.\n", read_amt);
#endif
   }

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
   microphone->use_float           = use_float; /* Store the format choice */
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

   /* Initialize handle fields */
   microphone->sample_rate = rate;
   microphone->use_float   = false; /* Default to integer format */

   /* Validate requested sample rate */
   if (rate != 44100 && rate != 48000)
   {
      RARCH_WARN("[CoreAudio] Requested sample rate %u not supported, defaulting to 48000.\n", rate);
      rate = 48000;
   }

#if TARGET_OS_IPHONE
   /* Configure audio session */
   AVAudioSession *audioSession = [AVAudioSession sharedInstance];
   NSError *error = nil;
   [audioSession setCategory:AVAudioSessionCategoryPlayAndRecord error:&error];
   if (error)
   {
      RARCH_ERR("[CoreAudio] Failed to set audio session category: %s.\n", [[error localizedDescription] UTF8String]);
      return NULL;
   }

   /*/ Set preferred sample rate */
   [audioSession setPreferredSampleRate:rate error:&error];
   if (error)
   {
      RARCH_ERR("[CoreAudio] Failed to set preferred sample rate: %s.\n", [[error localizedDescription] UTF8String]);
      return NULL;
   }

   /* Get actual sample rate */
   Float64 actualRate = [audioSession sampleRate];
   if (new_rate)
      *new_rate = (unsigned)actualRate;
   microphone->sample_rate = (int)actualRate;

   RARCH_LOG("[CoreAudio] Using sample rate: %d Hz.\n", microphone->sample_rate);
#else

#endif

   /* Set format using helper method */
   coreaudio_microphone_set_format(microphone, false); /* Default to 16-bit integer */

   /* Calculate FIFO buffer size */
   size_t fifoBufferSize = (latency * microphone->sample_rate * microphone->format.mBytesPerFrame) / 1000;
   if (fifoBufferSize == 0)
   {
      RARCH_WARN("[CoreAudio] Calculated FIFO buffer size is 0 for latency: %u, sample_rate: %d, bytes_per_frame: %d.\n",
            latency, microphone->sample_rate, microphone->format.mBytesPerFrame);
      fifoBufferSize = 1024; /* Default to a reasonable buffer size */
   }

   RARCH_LOG("[CoreAudio] FIFO buffer size: %zu bytes.\n", fifoBufferSize);

   /* Create sample buffer */
   microphone->sample_buffer = fifo_new(fifoBufferSize);
   if (!microphone->sample_buffer)
   {
      RARCH_ERR("[CoreAudio] Failed to create sample buffer.\n");
      return NULL;
   }

   /* Initialize audio unit */
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

   /* Enable input */
   UInt32 flag = 1;
   status = AudioUnitSetProperty(microphone->audio_unit,
         kAudioOutputUnitProperty_EnableIO,
         kAudioUnitScope_Input,
         1, /* Input bus */
         &flag,
         sizeof(flag));
   if (status != noErr)
   {
      RARCH_ERR("[CoreAudio] Failed to enable input.\n");
      goto error;
   }

   /* Set format using helper method */
   coreaudio_microphone_set_format(microphone, false); /* Default to 16-bit integer */
   status = AudioUnitSetProperty(microphone->audio_unit,
         kAudioUnitProperty_StreamFormat,
         kAudioUnitScope_Output,
         1, /* Input bus */
         &microphone->format,
         sizeof(microphone->format));
   if (status != noErr)
   {
      RARCH_ERR("[CoreAudio] Failed to set format: %d.\n", status);
      goto error;
   }

   /* Set callback */
   AURenderCallbackStruct callback = { coreaudio_input_callback, microphone };
   status = AudioUnitSetProperty(microphone->audio_unit,
         kAudioOutputUnitProperty_SetInputCallback,
         kAudioUnitScope_Global,
         1, /* Input bus */
         &callback,
         sizeof(callback));
   if (status != noErr)
   {
      RARCH_ERR("[CoreAudio] Failed to set callback.\n");
      goto error;
   }

   /* Initialize audio unit */
   status = AudioUnitInitialize(microphone->audio_unit);
   if (status != noErr)
   {
      RARCH_ERR("[CoreAudio] Failed to initialize audio unit: %d.\n", status);
      goto error;
   }

   /* Start audio unit */
   status = AudioOutputUnitStart(microphone->audio_unit);
   if (status != noErr)
   {
      RARCH_ERR("[CoreAudio] Failed to start audio unit: %d.\n", status);
      goto error;
   }

   return microphone;

error:
   if (microphone)
   {
      if (microphone->audio_unit)
      {
         AudioComponentInstanceDispose(microphone->audio_unit);
         microphone->audio_unit = nil;
      }
      free(microphone);
   }
   return NULL;
}

/* Close microphone */
static void coreaudio_microphone_close_mic(void *driver_context, void *microphone_context)
{
   coreaudio_microphone_t *microphone = (coreaudio_microphone_t*)microphone_context;
   if (microphone)
   {
      if (microphone->is_running)
         AudioOutputUnitStop(microphone->audio_unit);

      if (microphone->audio_unit)
      {
         AudioComponentInstanceDispose(microphone->audio_unit);
         microphone->audio_unit = nil;
      }
      if (microphone->sample_buffer)
         fifo_free(microphone->sample_buffer);
      free(microphone);
   }
   else
   {
      RARCH_ERR("[CoreAudio] Failed to close microphone.\n");
   }
}

/* Start microphone */
static bool coreaudio_microphone_start_mic(void *driver_context, void *microphone_context)
{
   RARCH_LOG("[CoreAudio] Starting microphone.\n");
   coreaudio_microphone_t *microphone = (coreaudio_microphone_t*)microphone_context;
   if (!microphone)
   {
      RARCH_ERR("[CoreAudio] Failed to start microphone.\n");
      return false;
   }
   RARCH_LOG("[CoreAudio] Starting audio unit...\n");

   OSStatus status = AudioOutputUnitStart(microphone->audio_unit);
   if (status == noErr)
   {
      RARCH_LOG("[CoreAudio] Audio unit started successfully.\n");
      microphone->is_running = true;
      return true;
   }
   RARCH_ERR("[CoreAudio] Failed to start microphone: %d.\n", status);
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
         return true;
      }
      RARCH_ERR("[CoreAudio] Failed to stop microphone: %d.\n", status);
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

/* Get device list (not implemented for CoreAudio) */
static struct string_list *coreaudio_microphone_device_list_new(const void *driver_context)
{
    return NULL;
}

/* Free device list (not implemented for CoreAudio) */
static void coreaudio_microphone_device_list_free(const void *driver_context, struct string_list *devices)
{
}

/* Check if microphone is using float format */
static bool coreaudio_microphone_use_float(const void *driver_context, const void *microphone_context)
{
   coreaudio_microphone_t *microphone = (coreaudio_microphone_t *)microphone_context;
   if (!microphone)
      return false;

   return microphone->use_float;
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

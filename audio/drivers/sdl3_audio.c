/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C)      2026 - Rob Loach
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

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_inline.h>
#include <retro_math.h>
#include <queues/fifo_queue.h>
#include <lists/string_list.h>
#include <string/stdstring.h>

#include <SDL3/SDL.h>

#include "../audio_driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"

/* The drivers below use a pull architecture: RetroArch pushes into a
 * FIFO owned by the driver, and SDL's stream get-callback pulls from
 * that FIFO each device period (the microphone mirrors this with the
 * put-callback).  The FIFO and its condition variable share one
 * mutex, so blocking reads/writes need no timeouts or sleeps and
 * can't miss a wakeup.  SDL's own synchronization primitives are
 * used instead of rthreads, so the driver behaves identically with
 * or without HAVE_THREADS; SDL3 provides them on every platform it
 * supports.  The lock order is always SDL's stream lock -> ours
 * (callbacks run under the stream lock), never the reverse: the
 * RetroArch side makes no SDL stream calls while holding our mutex. */

static INLINE int sdl3_audio_find_num_frames(int rate, int latency)
{
   /* Keep the buffer size to 2^n. */
   return next_pow2((rate * latency) / 1000);
}

/**
 * Looks up an audio device by name, returning the default
 * device if the name is empty or not found.
 */
static SDL_AudioDeviceID sdl3_audio_find_device(const char *device, bool recording)
{
   int i;
   int count                  = 0;
   SDL_AudioDeviceID *devices = NULL;
   SDL_AudioDeviceID devid    = recording
      ? SDL_AUDIO_DEVICE_DEFAULT_RECORDING
      : SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;

   if (string_is_empty(device))
      return devid;

   devices = recording
      ? SDL_GetAudioRecordingDevices(&count)
      : SDL_GetAudioPlaybackDevices(&count);

   if (devices)
   {
      for (i = 0; i < count; i++)
      {
         if (string_is_equal(SDL_GetAudioDeviceName(devices[i]), device))
         {
            devid = devices[i];
            break;
         }
      }
      SDL_free(devices);
   }

   if (      devid == SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK
         ||  devid == SDL_AUDIO_DEVICE_DEFAULT_RECORDING)
      RARCH_WARN("[SDL3 audio] Requested %s device \"%s\" not found, using the default device.\n",
            recording ? "recording" : "playback", device);

   return devid;
}

/**
 * Enumerates the available audio devices of either direction
 * into a newly allocated string list.
 */
static struct string_list *sdl3_audio_device_list(bool recording)
{
   int i;
   union string_list_elem_attr attr;
   int count                  = 0;
   SDL_AudioDeviceID *devices = NULL;
   struct string_list *sl     = string_list_new();

   if (!sl)
      return NULL;

   attr.i  = 0;
   devices = recording
      ? SDL_GetAudioRecordingDevices(&count)
      : SDL_GetAudioPlaybackDevices(&count);

   if (devices)
   {
      for (i = 0; i < count; i++)
      {
         const char *name = SDL_GetAudioDeviceName(devices[i]);
         if (name)
            string_list_append(sl, name, attr);
      }
      SDL_free(devices);
   }

   return sl;
}

typedef struct sdl3_audio
{
   /* Guards speaker_buffer and pairs with cond; the stream
    * get-callback takes it too, so FIFO fill checks and waits
    * are race-free. */
   SDL_Mutex *lock;
   SDL_Condition *cond;
   /**
    * The queue used to store outgoing samples to be played by the driver.
    * Audio from the core ultimately makes its way here,
    * the last stop before the get-callback pulls it into the device.
    */
   fifo_buffer_t *speaker_buffer;
   size_t buffer_size;
   /* Staging area for FIFO -> stream copies inside the callback. */
   uint8_t *scratch;
   size_t scratch_size;
   /* The device stream; samples are put into it only by the callback. */
   SDL_AudioStream *stream;
   /* The format samples are put in (SDL converts to the device format). */
   SDL_AudioSpec spec;
   /* Bytes per frame on the device side, for converting the
    * callback's byte counts into our put-side units. */
   size_t out_frame_size;
   size_t in_frame_size;
   bool nonblock;
   bool is_paused;
} sdl3_audio_t;

/**
 * Runs on SDL's device thread each period, before the device reads
 * from the stream: pulls however much the device needs out of the
 * FIFO.  If the FIFO can't cover it, the stream runs short and SDL
 * plays silence for the remainder (the underrun case).
 */
static void SDLCALL sdl3_audio_stream_cb(void *userdata,
      SDL_AudioStream *stream, int additional_amount, int total_amount)
{
   sdl3_audio_t *sdl = (sdl3_audio_t*)userdata;
   /* additional_amount is in device-format bytes; convert to ours. */
   size_t needed     = (size_t)((additional_amount + (int)sdl->out_frame_size - 1)
         / (int)sdl->out_frame_size) * sdl->in_frame_size;

   SDL_LockMutex(sdl->lock);

   while (needed > 0)
   {
      size_t avail = FIFO_READ_AVAIL(sdl->speaker_buffer);
      size_t chunk = (needed > sdl->scratch_size) ? sdl->scratch_size : needed;

      if (chunk > avail)
         chunk = avail;
      if (chunk == 0)
         break;

      fifo_read(sdl->speaker_buffer, sdl->scratch, chunk);
      /* This callback already holds the stream's recursive lock,
       * so putting from here can't deadlock. */
      SDL_PutAudioStreamData(stream, sdl->scratch, (int)chunk);
      needed -= chunk;
   }

   /* Signal while holding the mutex, so the wakeup can't slip
    * between a writer's fill check and its wait. */
   SDL_SignalCondition(sdl->cond);

   SDL_UnlockMutex(sdl->lock);
}

static void sdl3_audio_free(void *data)
{
   sdl3_audio_t *sdl = (sdl3_audio_t*)data;

   if (sdl)
   {
      /* Destroying the stream also closes the device.  Do this
       * before freeing anything else, since the stream callback
       * uses the FIFO, the scratch buffer and the locks. */
      if (sdl->stream)
         SDL_DestroyAudioStream(sdl->stream);

      if (sdl->speaker_buffer)
         fifo_free(sdl->speaker_buffer);
      free(sdl->scratch);

      if (sdl->cond)
         SDL_DestroyCondition(sdl->cond);
      if (sdl->lock)
         SDL_DestroyMutex(sdl->lock);

      SDL_QuitSubSystem(SDL_INIT_AUDIO);
   }
   free(sdl);
}

static void *sdl3_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned block_frames, unsigned *new_rate)
{
   int frames;
   char frames_str[16];
   size_t min_size;
   const char *device_name  = NULL;
   void *tmp                = NULL;
   SDL_AudioDeviceID devid  = 0;
   SDL_AudioSpec device_spec = {0};
   int device_sample_frames = 0;
   sdl3_audio_t *sdl        = NULL;
   settings_t *settings     = config_get_ptr();

   /* Subsystem initialization is reference-counted in SDL3, so
    * initialize unconditionally; every init is balanced by the
    * SDL_QuitSubSystem call in sdl3_audio_free. */
   if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
   {
      RARCH_ERR("[SDL3 audio] Failed to initialize audio subsystem: %s.\n",
            SDL_GetError());
      return NULL;
   }

   if (!(sdl = (sdl3_audio_t*)calloc(1, sizeof(*sdl))))
   {
      SDL_QuitSubSystem(SDL_INIT_AUDIO);
      return NULL;
   }

   devid = sdl3_audio_find_device(device, false);

   /* Ask SDL for the device's preferred format so the whole pipeline
    * can run at the device rate; RetroArch resamples to it exactly
    * once and SDL does no rate conversion of its own. */
   if (   !SDL_GetAudioDeviceFormat(devid, &device_spec, &device_sample_frames)
       || device_spec.freq <= 0)
      device_spec.freq = rate;

   /* Request a device period of roughly a quarter of the total
    * latency; the rest of the budget stays queued in the FIFO. */
   frames = sdl3_audio_find_num_frames(device_spec.freq, latency / 4);
   snprintf(frames_str, sizeof(frames_str), "%d", frames);
   SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, frames_str);

   sdl->spec.freq     = device_spec.freq;
   /* SDL3 converts transparently if the device's native format
    * differs; honour the negotiation hint for what we produce. */
   sdl->spec.format   = (settings->uints.audio_format_negotiation
         == AUDIO_FORMAT_NEGOTIATION_INT16)
         ? SDL_AUDIO_S16 : SDL_AUDIO_F32;
   sdl->spec.channels = 2;

   if (!(sdl->stream = SDL_OpenAudioDeviceStream(devid, &sdl->spec, NULL, NULL)))
   {
      RARCH_ERR("[SDL3 audio] Failed to open SDL audio output device: %s.\n",
            SDL_GetError());
      goto error;
   }

   /* See what SDL actually opened (sample_frames honours the hint). */
   sdl->in_frame_size  = SDL_AUDIO_FRAMESIZE(sdl->spec);
   sdl->out_frame_size = sdl->in_frame_size;
   if (SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(sdl->stream),
         &device_spec, &device_sample_frames))
   {
      if (SDL_AUDIO_FRAMESIZE(device_spec) > 0)
         sdl->out_frame_size = SDL_AUDIO_FRAMESIZE(device_spec);
   }
   else
      device_sample_frames = frames;

   /* Buffer the requested latency's worth of audio, with at least
    * two device periods to avoid underruns. */
   sdl->buffer_size = (size_t)((uint64_t)sdl->spec.freq * latency / 1000)
         * sdl->in_frame_size;
   min_size         = (size_t)(2 * device_sample_frames) * sdl->in_frame_size;
   if (sdl->buffer_size < min_size)
      sdl->buffer_size = min_size;

   /* The staging buffer covers one device period per copy;
    * the callback loops if it ever needs more. */
   sdl->scratch_size = (size_t)device_sample_frames * sdl->in_frame_size;
   if (sdl->scratch_size < 1024)
      sdl->scratch_size = 1024;

   sdl->speaker_buffer = fifo_new(sdl->buffer_size);
   sdl->scratch        = (uint8_t*)malloc(sdl->scratch_size);
   sdl->lock           = SDL_CreateMutex();
   sdl->cond           = SDL_CreateCondition();
   if (!sdl->speaker_buffer || !sdl->scratch || !sdl->lock || !sdl->cond)
      goto error;

   *new_rate = sdl->spec.freq;

   device_name = SDL_GetAudioDeviceName(devid);
   RARCH_DBG("[SDL3 audio] Opened SDL audio out device \"%s\".\n",
         device_name ? device_name : "(default)");
   RARCH_DBG("[SDL3 audio] Requested a speaker frequency of %u Hz, received %d Hz.\n",
         rate, sdl->spec.freq);
   RARCH_DBG("[SDL3 audio] Requested a device period of %d frames, received %d frames.\n",
         frames, device_sample_frames);
   RARCH_DBG("[SDL3 audio] Speaker audio format: %u-bit %s.\n",
         SDL_AUDIO_BITSIZE(sdl->spec.format),
         SDL_AUDIO_ISFLOAT(sdl->spec.format) ? "floating-point" : "integer");
   RARCH_LOG("[SDL3 audio] Requested %u ms latency for output device, using %d ms.\n",
         latency,
         (int)((sdl->buffer_size / sdl->in_frame_size + device_sample_frames)
            * 1000 / sdl->spec.freq));

   /* Prefill the FIFO with silence so playback starts with a
    * full latency budget instead of an immediate underrun. */
   if ((tmp = calloc(1, sdl->buffer_size)))
   {
      fifo_write(sdl->speaker_buffer, tmp, sdl->buffer_size);
      free(tmp);
   }

   SDL_SetAudioStreamGetCallback(sdl->stream, sdl3_audio_stream_cb, sdl);

   /* Device streams open in a paused state. */
   SDL_ResumeAudioStreamDevice(sdl->stream);

   return sdl;

error:
   sdl3_audio_free(sdl);
   return NULL;
}

static ssize_t sdl3_audio_write(void *data, const void *s, size_t len)
{
   size_t _len      = 0;
   sdl3_audio_t *sdl = (sdl3_audio_t*)data;

   SDL_LockMutex(sdl->lock);

   while (_len < len)
   {
      size_t avail = FIFO_WRITE_AVAIL(sdl->speaker_buffer);

      /* If the outgoing sample queue is full... */
      if (avail == 0)
      {
         if (sdl->nonblock)
            break; /* If the queue was full... well, too bad. */
         /* Block until the stream callback pulls samples out of the
          * FIFO.  It signals under this same mutex, so the wakeup
          * can't be lost between the check above and this wait. */
         SDL_WaitCondition(sdl->cond, sdl->lock);
      }
      else
      {
         /* Enqueue as many samples as we have available without
          * overflowing the queue. */
         size_t write_amt = (len - _len > avail) ? avail : len - _len;
         fifo_write(sdl->speaker_buffer, (const char*)s + _len, write_amt);
         _len += write_amt;
      }
   }

   SDL_UnlockMutex(sdl->lock);

   return _len;
}

static bool sdl3_audio_stop(void *data)
{
   sdl3_audio_t *sdl = (sdl3_audio_t*)data;
   sdl->is_paused    = true;
   return SDL_PauseAudioStreamDevice(sdl->stream);
}

static bool sdl3_audio_alive(void *data)
{
   sdl3_audio_t *sdl = (sdl3_audio_t*)data;
   if (!sdl)
      return false;
   return !sdl->is_paused;
}

static bool sdl3_audio_start(void *data, bool is_shutdown)
{
   sdl3_audio_t *sdl = (sdl3_audio_t*)data;
   sdl->is_paused    = false;
   return SDL_ResumeAudioStreamDevice(sdl->stream);
}

static void sdl3_audio_set_nonblock_state(void *data, bool state)
{
   sdl3_audio_t *sdl = (sdl3_audio_t*)data;
   if (sdl)
      sdl->nonblock = state;
}

static bool sdl3_audio_use_float(void *data)
{
   sdl3_audio_t *sdl = (sdl3_audio_t*)data;
   return SDL_AUDIO_ISFLOAT(sdl->spec.format) ? true : false;
}

static size_t sdl3_audio_write_avail(void *data)
{
   size_t avail;
   sdl3_audio_t *sdl = (sdl3_audio_t*)data;

   SDL_LockMutex(sdl->lock);
   avail = FIFO_WRITE_AVAIL(sdl->speaker_buffer);
   SDL_UnlockMutex(sdl->lock);

   return avail;
}

static size_t sdl3_audio_buffer_size(void *data)
{
   sdl3_audio_t *sdl = (sdl3_audio_t*)data;
   return sdl->buffer_size;
}

static void *sdl3_audio_list_new(void *u)
{
   return sdl3_audio_device_list(false);
}

static void sdl3_audio_list_free(void *u, void *slp)
{
   struct string_list *sl = (struct string_list*)slp;

   if (sl)
      string_list_free(sl);
}

audio_driver_t audio_sdl3 = {
   sdl3_audio_init,
   sdl3_audio_write,
   sdl3_audio_stop,
   sdl3_audio_start,
   sdl3_audio_alive,
   sdl3_audio_set_nonblock_state,
   sdl3_audio_free,
   sdl3_audio_use_float,
   "sdl3",
   sdl3_audio_list_new,
   sdl3_audio_list_free,
   sdl3_audio_write_avail,
   sdl3_audio_buffer_size,
   NULL  /* write_raw */
};

#ifdef HAVE_MICROPHONE
#include "../microphone_driver.h"

typedef struct sdl3_microphone
{
   bool nonblock;
} sdl3_microphone_t;

typedef struct sdl3_microphone_handle
{
   /* Guards sample_buffer and pairs with cond (see sdl3_audio). */
   SDL_Mutex *lock;
   SDL_Condition *cond;
   /**
    * The queue used to store incoming samples from the driver.
    * The stream put-callback drains the device stream into it.
    */
   fifo_buffer_t *sample_buffer;
   size_t buffer_size;
   /* Staging area for stream -> FIFO copies inside the callback. */
   uint8_t *scratch;
   size_t scratch_size;
   /* The device stream; samples are read from it only by the callback. */
   SDL_AudioStream *stream;
   /* The format samples are read in (SDL converts from the device format). */
   SDL_AudioSpec spec;
} sdl3_microphone_handle_t;

/**
 * Runs on SDL's device thread each period, after the device puts
 * recorded samples into the stream: drains the stream into the FIFO.
 * If the FIFO is almost full, whatever doesn't fit is dropped, which
 * keeps capture latency bounded when nothing is reading.
 */
static void SDLCALL sdl3_microphone_stream_cb(void *userdata,
      SDL_AudioStream *stream, int additional_amount, int total_amount)
{
   int got;
   sdl3_microphone_handle_t *mic = (sdl3_microphone_handle_t*)userdata;

   /* Always drain the stream completely, even when the FIFO can't
    * take it all, so unread samples never pile up inside SDL. */
   while ((got = SDL_GetAudioStreamData(stream,
         mic->scratch, (int)mic->scratch_size)) > 0)
   {
      size_t avail, write_amt;

      SDL_LockMutex(mic->lock);
      avail     = FIFO_WRITE_AVAIL(mic->sample_buffer);
      write_amt = ((size_t)got > avail) ? avail : (size_t)got;
      fifo_write(mic->sample_buffer, mic->scratch, write_amt);
      /* Signal under the mutex; the wakeup can't be lost. */
      SDL_SignalCondition(mic->cond);
      SDL_UnlockMutex(mic->lock);

      if (got < (int)mic->scratch_size)
         break;
   }
}

static void *sdl3_microphone_init(void)
{
   sdl3_microphone_t *sdl = NULL;

   /* Reference-counted; balanced by SDL_QuitSubSystem in free. */
   if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
      return NULL;

   if (!(sdl = (sdl3_microphone_t*)calloc(1, sizeof(*sdl))))
   {
      SDL_QuitSubSystem(SDL_INIT_AUDIO);
      return NULL;
   }
   return sdl;
}

static void sdl3_microphone_free(void *driver_context)
{
   sdl3_microphone_t *sdl = (sdl3_microphone_t*)driver_context;

   if (sdl)
      SDL_QuitSubSystem(SDL_INIT_AUDIO);
   free(sdl);
   /* NOTE: The microphone frontend should've closed the mics by now */
}

static void sdl3_microphone_close_mic(void *driver_context, void *mic_context)
{
   sdl3_microphone_handle_t *mic = (sdl3_microphone_handle_t*)mic_context;

   if (mic)
   {
      /* Destroying the stream also closes the device.  Do this
       * before freeing anything else, since the stream callback
       * uses the FIFO, the scratch buffer and the locks. */
      if (mic->stream)
         SDL_DestroyAudioStream(mic->stream);

      if (mic->sample_buffer)
         fifo_free(mic->sample_buffer);
      free(mic->scratch);

      if (mic->cond)
         SDL_DestroyCondition(mic->cond);
      if (mic->lock)
         SDL_DestroyMutex(mic->lock);

      RARCH_LOG("[SDL3 mic] Freed microphone.\n");
      free(mic);
   }
}

static void *sdl3_microphone_open_mic(void *driver_context, const char *device,
      unsigned rate, unsigned latency, unsigned *new_rate)
{
   int frames;
   char frames_str[16];
   size_t frame_size, min_size;
   const char *device_name       = NULL;
   void *tmp                     = NULL;
   SDL_AudioDeviceID devid       = 0;
   SDL_AudioSpec device_spec     = {0};
   int device_sample_frames      = 0;
   sdl3_microphone_handle_t *mic = NULL;
   settings_t *settings          = config_get_ptr();

   /* If the audio subsystem wasn't initialized yet... */
   if (!SDL_WasInit(SDL_INIT_AUDIO))
   {
      RARCH_ERR("[SDL3 mic] Attempted to initialize input device before initializing the audio subsystem.\n");
      return NULL;
   }

   if (!(mic = (sdl3_microphone_handle_t*)calloc(1, sizeof(*mic))))
      return NULL;

   /* Only print SDL audio devices if verbose logging is enabled */
   if (verbosity_is_enabled())
   {
      int i;
      int count                  = 0;
      SDL_AudioDeviceID *devices = SDL_GetAudioRecordingDevices(&count);

      RARCH_DBG("[SDL3 mic] %d audio capture devices found:\n", count);
      if (devices)
      {
         for (i = 0; i < count; i++)
         {
            const char *name = SDL_GetAudioDeviceName(devices[i]);
            RARCH_DBG("[SDL3 mic]    - %s\n", name ? name : "(unknown)");
         }
         SDL_free(devices);
      }
   }

   devid = sdl3_audio_find_device(device, true);

   /* Run the capture path at the device's preferred rate so SDL
    * does no rate conversion; the mic frontend resamples once. */
   if (   !SDL_GetAudioDeviceFormat(devid, &device_spec, &device_sample_frames)
       || device_spec.freq <= 0)
      device_spec.freq = rate;

   frames = sdl3_audio_find_num_frames(device_spec.freq, latency / 4);
   snprintf(frames_str, sizeof(frames_str), "%d", frames);
   SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, frames_str);

   mic->spec.freq     = device_spec.freq;
   /* The int16 negotiation hint keeps the whole capture path integer
    * instead of converting a float stream back down; SDL converts
    * transparently if the device's native format differs. */
   mic->spec.format   = (settings->uints.audio_format_negotiation
         == AUDIO_FORMAT_NEGOTIATION_INT16)
         ? SDL_AUDIO_S16 : SDL_AUDIO_F32;
   mic->spec.channels = 1; /* Microphones only usually provide input in mono */

   /* Device streams open in a paused state;
    * the frontend starts them with start_mic. */
   if (!(mic->stream = SDL_OpenAudioDeviceStream(devid, &mic->spec, NULL, NULL)))
   {
      RARCH_ERR("[SDL3 mic] Failed to open SDL audio input device: %s.\n",
            SDL_GetError());
      goto error;
   }

   if (!SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(mic->stream),
         &device_spec, &device_sample_frames))
      device_sample_frames = frames;

   /* Buffer up to double the latency budget (with a floor of four
    * device periods) before new samples start getting dropped. */
   frame_size        = SDL_AUDIO_FRAMESIZE(mic->spec);
   mic->buffer_size  = (size_t)((uint64_t)mic->spec.freq * latency / 1000)
         * frame_size * 2;
   min_size          = (size_t)(4 * device_sample_frames) * frame_size;
   if (mic->buffer_size < min_size)
      mic->buffer_size = min_size;

   mic->scratch_size = (size_t)device_sample_frames * frame_size;
   if (mic->scratch_size < 1024)
      mic->scratch_size = 1024;

   mic->sample_buffer = fifo_new(mic->buffer_size);
   mic->scratch       = (uint8_t*)malloc(mic->scratch_size);
   mic->lock          = SDL_CreateMutex();
   mic->cond          = SDL_CreateCondition();
   if (!mic->sample_buffer || !mic->scratch || !mic->lock || !mic->cond)
      goto error;

   /* Prefill half the FIFO with silence, so the first blocking
    * reads have a latency cushion to draw from. */
   if ((tmp = calloc(1, mic->buffer_size / 2)))
   {
      fifo_write(mic->sample_buffer, tmp, mic->buffer_size / 2);
      free(tmp);
   }

   SDL_SetAudioStreamPutCallback(mic->stream, sdl3_microphone_stream_cb, mic);

   if (new_rate)
      *new_rate = mic->spec.freq;

   device_name = SDL_GetAudioDeviceName(devid);
   RARCH_DBG("[SDL3 mic] Opened SDL audio input device \"%s\".\n",
         device_name ? device_name : "(default)");
   RARCH_DBG("[SDL3 mic] Requested a microphone frequency of %u Hz, received %d Hz.\n",
         rate, mic->spec.freq);
   RARCH_DBG("[SDL3 mic] Requested a device period of %d frames, received %d frames.\n",
         frames, device_sample_frames);
   RARCH_DBG("[SDL3 mic] Microphone audio format: %u-bit %s.\n",
         SDL_AUDIO_BITSIZE(mic->spec.format),
         SDL_AUDIO_ISFLOAT(mic->spec.format) ? "floating-point" : "integer");
   RARCH_LOG("[SDL3 mic] Requested %u ms latency for input device, using %d ms.\n",
         latency,
         (int)((mic->buffer_size / 2 / frame_size + device_sample_frames)
            * 1000 / mic->spec.freq));

   return mic;

error:
   sdl3_microphone_close_mic(driver_context, mic);
   return NULL;
}

static bool sdl3_microphone_mic_alive(const void *driver_context, const void *mic_context)
{
   const sdl3_microphone_handle_t *mic = (const sdl3_microphone_handle_t*)mic_context;
   if (!mic)
      return false;
   return !SDL_AudioStreamDevicePaused(mic->stream);
}

static bool sdl3_microphone_start_mic(void *driver_context, void *mic_context)
{
   sdl3_microphone_handle_t *mic = (sdl3_microphone_handle_t*)mic_context;
   if (!mic)
      return false;
   if (!SDL_ResumeAudioStreamDevice(mic->stream))
   {
      RARCH_ERR("[SDL3 mic] Failed to start microphone: %s.\n", SDL_GetError());
      return false;
   }
   RARCH_DBG("[SDL3 mic] Started microphone.\n");
   return true;
}

static bool sdl3_microphone_stop_mic(void *driver_context, void *mic_context)
{
   sdl3_microphone_handle_t *mic = (sdl3_microphone_handle_t*)mic_context;
   if (!mic)
      return false;
   if (!SDL_PauseAudioStreamDevice(mic->stream))
   {
      RARCH_ERR("[SDL3 mic] Failed to pause microphone: %s.\n", SDL_GetError());
      return false;
   }
   return true;
}

static void sdl3_microphone_set_nonblock_state(void *driver_context, bool nonblock)
{
   sdl3_microphone_t *sdl = (sdl3_microphone_t*)driver_context;
   if (sdl)
      sdl->nonblock = nonblock;
}

static int sdl3_microphone_read(void *driver_context, void *mic_context,
      void *s, size_t len)
{
   size_t _len                   = 0;
   sdl3_microphone_t *sdl        = (sdl3_microphone_t*)driver_context;
   sdl3_microphone_handle_t *mic = (sdl3_microphone_handle_t*)mic_context;

   if (!sdl || !mic || !s)
      return -1;

   SDL_LockMutex(mic->lock);

   /* Until we've given the caller as much data as they've asked for... */
   while (_len < len)
   {
      size_t avail = FIFO_READ_AVAIL(mic->sample_buffer);

      /* If the incoming sample queue is empty... */
      if (avail == 0)
      {
         if (sdl->nonblock)
            break;
         /* Block until the stream callback puts samples into the
          * FIFO.  It signals under this same mutex, so the wakeup
          * can't be lost between the check above and this wait. */
         SDL_WaitCondition(mic->cond, mic->lock);
      }
      else
      {
         /* Read as many samples as we have available without
          * underflowing the queue. */
         size_t read_amt = (len - _len > avail) ? avail : len - _len;
         fifo_read(mic->sample_buffer, (char*)s + _len, read_amt);
         _len += read_amt;
      }
   }

   SDL_UnlockMutex(mic->lock);

   return (int)_len;
}

static bool sdl3_microphone_mic_use_float(const void *driver_context, const void *mic_context)
{
   const sdl3_microphone_handle_t *mic = (const sdl3_microphone_handle_t*)mic_context;
   return SDL_AUDIO_ISFLOAT(mic->spec.format) ? true : false;
}

static struct string_list *sdl3_microphone_device_list_new(const void *driver_context)
{
   return sdl3_audio_device_list(true);
}

static void sdl3_microphone_device_list_free(const void *driver_context,
      struct string_list *devices)
{
   if (devices)
      string_list_free(devices);
}

microphone_driver_t microphone_sdl3 = {
   sdl3_microphone_init,
   sdl3_microphone_free,
   sdl3_microphone_read,
   sdl3_microphone_set_nonblock_state,
   "sdl3",
   sdl3_microphone_device_list_new,
   sdl3_microphone_device_list_free,
   sdl3_microphone_open_mic,
   sdl3_microphone_close_mic,
   sdl3_microphone_mic_alive,
   sdl3_microphone_start_mic,
   sdl3_microphone_stop_mic,
   sdl3_microphone_mic_use_float,
};
#endif /* HAVE_MICROPHONE */

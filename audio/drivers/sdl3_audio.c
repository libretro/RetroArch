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

#include <boolean.h>
#include <retro_miscellaneous.h>
#include <lists/string_list.h>
#include <string/stdstring.h>

#include <SDL3/SDL.h>

#include "../audio_driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"

/* SDL3 Audio Driver */

/* The duration of time to continue polling in order to give up
 * on a device. */
#define SDL3_AUDIO_STALL_TIMEOUT_NS SDL_MS_TO_NS(256)

/* The number of nanoseconds to wait between device polls. */
#define SDL3_AUDIO_POLL_INTERVAL_NS SDL_NS_PER_MS

/* Context for an audio device. Covered by three different states:
 * 1. Output Driver, used for playback
 * 2. Input Driver, used for recording, like from a microphone
 * 3. Microphone Device, holds the state for each individual microphone */
typedef struct sdl3_audio
{
   SDL_AudioStream *stream; /**< The device stream. */
   SDL_AudioSpec spec; /**< The format for the given audio sample. */
   size_t buffer_size; /**< Cap in bytes on queued audio: writes block past it, capture backlog is dropped past it. */
   bool nonblock; /**< When true, drop samples instead of waiting for the device to clear. */
} sdl3_audio_t;

/**
 * Looks up an audio device by name, returning the default
 * device ID if the name is empty or not found.
 */
static SDL_AudioDeviceID sdl3_audio_find_device(const char *device, bool recording)
{
   int i;
   bool found = false;
   int count = 0;
   SDL_AudioDeviceID *devices = NULL;
   SDL_AudioDeviceID devid = recording
      ? SDL_AUDIO_DEVICE_DEFAULT_RECORDING
      : SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;

   if (string_is_empty(device))
      return devid;

   devices = recording
      ? SDL_GetAudioRecordingDevices(&count)
      : SDL_GetAudioPlaybackDevices(&count);

   if (!devices)
   {
      RARCH_WARN("[SDL3 audio] Failed to enumerate %s devices: %s.\n",
            recording ? "recording" : "playback", SDL_GetError());
      return devid;
   }

   for (i = 0; i < count; i++)
   {
      if (string_is_equal(SDL_GetAudioDeviceName(devices[i]), device))
      {
         devid = devices[i];
         found = true;
         break;
      }
   }
   SDL_free(devices);

   if (!found)
      RARCH_WARN("[SDL3 audio] Requested %s device \"%s\" not found, using the default device.\n",
            recording ? "recording" : "playback", device);

   return devid;
}

/**
 * Builds a list of strings from the available audio devices.
 *
 * @param recording Whether or not to grab recording devices, or
 *   just playback.
 * @return The list of audio device names, which must be freed with
 *   \c string_list_free() when complete.
 */
static struct string_list *sdl3_audio_device_list(bool recording)
{
   int i;
   union string_list_elem_attr attr;
   int count = 0;
   SDL_AudioDeviceID *devices = NULL;
   struct string_list *sl = string_list_new();

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

/**
 * Opens the given audio device.
 *
 * It is configured to match native hardware rates to avoid
 * resampling, and sets low-latency buffer periods.
 */
static SDL_AudioStream *sdl3_audio_open_stream(const char *device,
      bool recording, unsigned rate, unsigned latency, int channels,
      SDL_AudioSpec *spec, int *period_frames)
{
   int frames;
   char frames_str[16];
   const char *device_name = NULL;
   SDL_AudioStream *stream = NULL;
   SDL_AudioSpec device_spec = {0};
   int device_sample_frames = 0;
   SDL_AudioDeviceID devid = sdl3_audio_find_device(device, recording);
   settings_t *settings = config_get_ptr();

   if (!SDL_GetAudioDeviceFormat(devid, &device_spec, &device_sample_frames) || device_spec.freq <= 0)
      device_spec.freq = rate;

   /* Aim for a device period of a quarter of the requested latency. */
   frames = (int)((uint64_t)device_spec.freq * latency / 4000);
   if (frames < 1)
      frames = 1;
   snprintf(frames_str, sizeof(frames_str), "%d", frames);
   SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, frames_str);

   spec->freq = device_spec.freq;
   /* SDL3 converts the format transparently if needed. */
   spec->format = (settings->uints.audio_format_negotiation
         == AUDIO_FORMAT_NEGOTIATION_INT16)
         ? SDL_AUDIO_S16 : SDL_AUDIO_F32;
   spec->channels = channels;

   stream = SDL_OpenAudioDeviceStream(devid, spec, NULL, NULL);

   /* Audio hints are global, so clear it now that we've used it. */
   SDL_ResetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES);
   if (!stream)
   {
      RARCH_ERR("[SDL3 audio] failed to open SDL audio %s device: %s.\n",
            recording ? "input" : "output", SDL_GetError());
      return NULL;
   }

   /* Retrieve the device information to set the period_frames. */
   if (!SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(stream),
         &device_spec, &device_sample_frames))
      device_sample_frames = frames;
   *period_frames = device_sample_frames;

   device_name = SDL_GetAudioDeviceName(devid);
   RARCH_DBG("[SDL3 audio] Opened %s device \"%s\": "
         "%u-bit %s, requested %u Hz / %d frame periods, "
         "received %d Hz / %d frame periods.\n",
         recording ? "input" : "output",
         device_name ? device_name : "(default)",
         SDL_AUDIO_BITSIZE(spec->format),
         SDL_AUDIO_ISFLOAT(spec->format) ? "floating-point" : "integer",
         rate, frames, spec->freq, device_sample_frames);

   return stream;
}

static void sdl3_audio_free(void *data)
{
   sdl3_audio_t *sdl = (sdl3_audio_t*)data;

   if (sdl)
   {
      /* Destroying the stream also closes the device. */
      if (sdl->stream)
         SDL_DestroyAudioStream(sdl->stream);

      SDL_QuitSubSystem(SDL_INIT_AUDIO);
   }
   free(sdl);
}

static void *sdl3_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned block_frames, unsigned *new_rate)
{
   size_t frame_size, min_size;
   void *tmp = NULL;
   int device_sample_frames = 0;
   sdl3_audio_t *sdl = NULL;

   if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
   {
      RARCH_ERR("[SDL3 audio] Failed to initialize audio subsystem: %s.\n", SDL_GetError());
      return NULL;
   }

   if (!(sdl = (sdl3_audio_t*)calloc(1, sizeof(*sdl))))
   {
      SDL_QuitSubSystem(SDL_INIT_AUDIO);
      return NULL;
   }

   if (!(sdl->stream = sdl3_audio_open_stream(device, false, rate, latency,
         2, &sdl->spec, &device_sample_frames)))
      goto error;

   /* Buffer the requested latency's worth of audio. */
   frame_size = SDL_AUDIO_FRAMESIZE(sdl->spec);
   sdl->buffer_size = (size_t)((uint64_t)sdl->spec.freq * latency / 1000) * frame_size;
   min_size = (size_t)(2 * device_sample_frames) * frame_size;
   if (sdl->buffer_size < min_size)
      sdl->buffer_size = min_size;

   *new_rate = sdl->spec.freq;

   RARCH_LOG("[SDL3 audio] Requested %u ms latency for output device, using %d ms.\n",
         latency,
         (int)((sdl->buffer_size / frame_size + device_sample_frames)
            * 1000 / sdl->spec.freq));

   /* Prefill the stream with silence. */
   if ((tmp = calloc(1, sdl->buffer_size)))
   {
      SDL_PutAudioStreamData(sdl->stream, tmp, (int)sdl->buffer_size);
      free(tmp);
   }

   /* Device streams open in a paused state. */
   SDL_ResumeAudioStreamDevice(sdl->stream);

   return sdl;

error:
   sdl3_audio_free(sdl);
   return NULL;
}

static size_t sdl3_audio_write_avail(void *data)
{
   int queued;
   sdl3_audio_t *sdl = (sdl3_audio_t*)data;

   if (!sdl)
      return 0;

   queued = SDL_GetAudioStreamQueued(sdl->stream);
   if (queued < 0 || (size_t)queued >= sdl->buffer_size)
      return 0;
   return sdl->buffer_size - (size_t)queued;
}

static ssize_t sdl3_audio_write(void *data, const void *s, size_t len)
{
   size_t size = 0;
   Uint64 deadline = 0;
   sdl3_audio_t *sdl = (sdl3_audio_t*)data;

   if (!sdl)
      return -1;

   while (size < len)
   {
      size_t avail = sdl3_audio_write_avail(sdl);

      if (avail == 0)
      {
         Uint64 now;

         /* When the queue is full, and we can't wait on the
          * process for the queue to clear a bit, we're unable
          * to do anything, so skip the rest. */
         if (sdl->nonblock)
            break;
         now = SDL_GetTicksNS();
         if (deadline == 0)
            deadline = now + SDL3_AUDIO_STALL_TIMEOUT_NS;
         else if (now >= deadline)
            break;
         SDL_DelayNS(SDL3_AUDIO_POLL_INTERVAL_NS);
      }
      else
      {
         /* Add as many samples as we are able to without hitting
          * the maximum limit. */
         size_t write_amt = MIN(len - size, avail);
         if (!SDL_PutAudioStreamData(sdl->stream, (const char*)s + size, (int)write_amt))
         {
            RARCH_ERR("[SDL3 audio] Failed to write to audio stream: %s.\n", SDL_GetError());
            if (size == 0)
               return -1;
            break;
         }
         size += write_amt;
         deadline = 0; /* Reset the stall clock. */
      }
   }

   return size;
}

static bool sdl3_audio_stop(void *data)
{
   sdl3_audio_t *sdl = (sdl3_audio_t*)data;
   if (!sdl)
      return false;
   return SDL_PauseAudioStreamDevice(sdl->stream);
}

static bool sdl3_audio_alive(void *data)
{
   sdl3_audio_t *sdl = (sdl3_audio_t*)data;
   if (!sdl)
      return false;
   return !SDL_AudioStreamDevicePaused(sdl->stream);
}

static bool sdl3_audio_start(void *data, bool is_shutdown)
{
   sdl3_audio_t *sdl = (sdl3_audio_t*)data;
   if (!sdl)
      return false;
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
   if (!sdl)
      return false;
   return SDL_AUDIO_ISFLOAT(sdl->spec.format) != 0;
}

static size_t sdl3_audio_buffer_size(void *data)
{
   sdl3_audio_t *sdl = (sdl3_audio_t*)data;
   if (!sdl)
      return 0;
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

/**
 * Stream callback for the microphone to drop stale audio to allow
 * space within the buffer.
 */
static void SDLCALL sdl3_microphone_stream_cb(void *userdata,
      SDL_AudioStream *stream, int additional_amount, int total_amount)
{
   sdl3_audio_t *mic = (sdl3_audio_t*)userdata;

   if (SDL_GetAudioStreamAvailable(stream) > (int)mic->buffer_size)
      SDL_ClearAudioStream(stream);
}

static void *sdl3_microphone_init(void)
{
   sdl3_audio_t *sdl = NULL;

   if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
   {
      RARCH_ERR("[SDL3 audio] Failed to initialize microphone subsystem: %s.\n",
            SDL_GetError());
      return NULL;
   }

   if (!(sdl = (sdl3_audio_t*)calloc(1, sizeof(*sdl))))
   {
      SDL_QuitSubSystem(SDL_INIT_AUDIO);
      return NULL;
   }
   return sdl;
}

static void sdl3_microphone_free(void *driver_context)
{
   sdl3_audio_t *sdl = (sdl3_audio_t*)driver_context;

   if (sdl)
   {
      SDL_QuitSubSystem(SDL_INIT_AUDIO);
      free(sdl);
   }
}

static void sdl3_microphone_close_mic(void *driver_context, void *mic_context)
{
   sdl3_audio_t *mic = (sdl3_audio_t*)mic_context;

   if (!mic)
      return;

   /* Destroying the stream also closes the device. */
   if (mic->stream)
      SDL_DestroyAudioStream(mic->stream);

   RARCH_LOG("[SDL3 audio] Freed microphone.\n");
   free(mic);
}

static void *sdl3_microphone_open_mic(void *driver_context, const char *device,
      unsigned rate, unsigned latency, unsigned *new_rate)
{
   size_t frame_size, min_size;
   int device_sample_frames      = 0;
   sdl3_audio_t *mic = NULL;

   if (!SDL_WasInit(SDL_INIT_AUDIO))
   {
      RARCH_ERR("[SDL3 audio] Attempted to initialize input device before the audio subsystem.\n");
      return NULL;
   }

   if (!(mic = (sdl3_audio_t*)calloc(1, sizeof(*mic))))
      return NULL;

   /* Only print SDL audio devices if verbose logging is enabled */
   if (verbosity_is_enabled())
   {
      struct string_list *sl = sdl3_audio_device_list(true);
      if (sl)
      {
         size_t i;
         RARCH_DBG("[SDL3 audio] %d input devices found:\n", (int)sl->size);
         for (i = 0; i < sl->size; i++)
            RARCH_DBG("[SDL3 audio]    - %s\n", sl->elems[i].data);
         string_list_free(sl);
      }
   }

   /* Device streams open in a paused state. The frontend starts
    * them with start_mic. Microphones usually provide mono input. */
   if (!(mic->stream = sdl3_audio_open_stream(device, true, rate, latency,
         1, &mic->spec, &device_sample_frames)))
      goto error;

   /* Buffer up to double the latency budget. */
   frame_size = SDL_AUDIO_FRAMESIZE(mic->spec);
   mic->buffer_size = (size_t)((uint64_t)mic->spec.freq * latency / 1000) * frame_size * 2;
   min_size = (size_t)(4 * device_sample_frames) * frame_size;
   if (mic->buffer_size < min_size)
      mic->buffer_size = min_size;

   SDL_SetAudioStreamPutCallback(mic->stream, sdl3_microphone_stream_cb, mic);

   if (new_rate)
      *new_rate = mic->spec.freq;

   RARCH_LOG("[SDL3 audio] Requested %u ms latency for input device, capping the backlog at %d ms.\n",
         latency,
         (int)((mic->buffer_size / frame_size) * 1000 / mic->spec.freq));

   return mic;

error:
   sdl3_microphone_close_mic(driver_context, mic);
   return NULL;
}

static bool sdl3_microphone_mic_alive(const void *driver_context, const void *mic_context)
{
   const sdl3_audio_t *mic = (const sdl3_audio_t*)mic_context;
   if (!mic)
      return false;
   return !SDL_AudioStreamDevicePaused(mic->stream);
}

static bool sdl3_microphone_start_mic(void *driver_context, void *mic_context)
{
   sdl3_audio_t *mic = (sdl3_audio_t*)mic_context;
   if (!mic)
      return false;
   if (!SDL_ResumeAudioStreamDevice(mic->stream))
   {
      RARCH_ERR("[SDL3 audio] Failed to start microphone: %s.\n", SDL_GetError());
      return false;
   }
   RARCH_DBG("[SDL3 audio] Started microphone.\n");
   return true;
}

static bool sdl3_microphone_stop_mic(void *driver_context, void *mic_context)
{
   sdl3_audio_t *mic = (sdl3_audio_t*)mic_context;
   if (!mic)
      return false;
   if (!SDL_PauseAudioStreamDevice(mic->stream))
   {
      RARCH_ERR("[SDL3 audio] Failed to pause microphone: %s.\n", SDL_GetError());
      return false;
   }
   return true;
}

static void sdl3_microphone_set_nonblock_state(void *driver_context, bool nonblock)
{
   sdl3_audio_t *sdl = (sdl3_audio_t*)driver_context;
   if (sdl)
      sdl->nonblock = nonblock;
}

static int sdl3_microphone_read(void *driver_context, void *mic_context,
      void *s, size_t len)
{
   size_t size = 0;
   Uint64 deadline = 0;
   sdl3_audio_t *sdl = (sdl3_audio_t*)driver_context;
   sdl3_audio_t *mic = (sdl3_audio_t*)mic_context;

   if (!sdl || !mic || !s)
      return -1;

   while (size < len)
   {
      int got = SDL_GetAudioStreamData(mic->stream, (char*)s + size, (int)(len - size));
      if (got < 0)
      {
         RARCH_ERR("[SDL3 audio] Failed to read from microphone stream: %s.\n", SDL_GetError());
         if (size == 0)
            return -1;
         break;
      }
      if (got > 0)
      {
         size += (size_t)got;
         deadline = 0; /* Reset the stall clock. */
      }
      else
      {
         Uint64 now;
         if (sdl->nonblock)
            break;
         /* Poll until the device puts more samples into the stream.
          * Bounded: if the device stops making progress, give up
          * after the stall timeout instead of blocking forever. */
         now = SDL_GetTicksNS();
         if (deadline == 0)
            deadline = now + SDL3_AUDIO_STALL_TIMEOUT_NS;
         else if (now >= deadline)
            break;
         SDL_DelayNS(SDL3_AUDIO_POLL_INTERVAL_NS);
      }
   }

   return (int)size;
}

static bool sdl3_microphone_mic_use_float(const void *driver_context, const void *mic_context)
{
   const sdl3_audio_t *mic = (const sdl3_audio_t*)mic_context;
   if (!mic)
      return false;
   return SDL_AUDIO_ISFLOAT(mic->spec.format) != 0;
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

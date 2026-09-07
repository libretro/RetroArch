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

/* Timeout in milliseconds for a blocked read/write to detect a stalled
 * audio device.  Applied per wait; each wake from the stream callback
 * re-arms it, so it bounds continuous silence from the device side. */
#define SDL3_AUDIO_STALL_TIMEOUT_MS 256

/* Context for an audio device. Covered by three different states:
 * 1. Output Driver, used for playback
 * 2. Input Driver, used for recording, like from a microphone
 * 3. Microphone Device, holds the state for each individual microphone */
typedef struct sdl3_audio
{
   SDL_AudioStream *stream; /**< The device stream. */
   SDL_Mutex *lock; /**< Guards condition, the stream callbacks are called through it. */
   SDL_Condition *cond; /**< Signalled each time the device moves data. */
   SDL_AudioSpec spec; /**< The format for the given audio sample. */
   SDL_AtomicU32 devid; /**< The device the stream is bound to. */
   size_t buffer_size; /**< Cap in bytes on queued audio: writes block past it, capture backlog is dropped past it. */
   size_t in_cap; /**< buffer_size converted into input-format bytes; equal to buffer_size outside the write_raw fast path. */
   int raw_rate; /**< Core rate the write_raw fast path set as the stream's input side (int16 stereo); 0 while the input side matches the device spec. */
   unsigned latency; /**< The amount of requested latency in milliseconds. */
   float ratio; /**< Frequency ratio currently set on the stream, to skip redundant sets. */
   float gain; /**< Gain currently set on the stream, to skip redundant sets. */
   SDL_AtomicInt device_removed; /**< Becomes true when the stream's device is unplugged. Set under lock so a blocked wait wakes. */
   bool nonblock; /**< When true, drop samples instead of waiting for the device to clear. */
   bool data_moved; /**< Wake token set by the stream callback, consumed by waiters. Guarded by lock; makes the queue-full test race-free. */
   SDL_AtomicInt defunct; /**< True when the device has completely failed. Saves from retrying each frame. */
} sdl3_audio_t;

/**
 * Event callback for SDL_EVENT_AUDIO_DEVICE_REMOVED and _ADDED.
 *
 * Matches on the stream's logical device id, so it only fires for
 * streams bound to an explicitly selected device: SDL never sends
 * REMOVED for a logical device opened as the system default - it
 * parks that on a zombie device and migrates it to new hardware
 * itself.  The reopen path therefore cannot fight SDL's migration.
 */
static bool SDLCALL sdl3_audio_device_removed_watch(void *userdata, SDL_Event *event)
{
   sdl3_audio_t *sdl = (sdl3_audio_t*)userdata;

   if (   event->type == SDL_EVENT_AUDIO_DEVICE_REMOVED
       && event->adevice.which == SDL_GetAtomicU32(&sdl->devid))
   {
      RARCH_WARN("[SDL3 audio] Audio %s device was removed.\n",
            event->adevice.recording ? "input" : "output");
      SDL_LockMutex(sdl->lock);
      SDL_SetAtomicInt(&sdl->device_removed, 1);
      SDL_SignalCondition(sdl->cond);
      SDL_UnlockMutex(sdl->lock);
   }
   else if (event->type == SDL_EVENT_AUDIO_DEVICE_ADDED
       && !event->adevice.recording
       && SDL_GetAtomicInt(&sdl->defunct))
   {
      RARCH_LOG("[SDL3 audio] A playback device appeared, retrying audio output.\n");
      SDL_SetAtomicInt(&sdl->defunct, 0);
   }
   return true;
}

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

/**
 * Device thread callback which wakes blocked reads/writes when
 * queue space becomes available.
 */
static void SDLCALL sdl3_audio_stream_cb(void *userdata,
      SDL_AudioStream *stream, int additional_amount, int total_amount)
{
   sdl3_audio_t *sdl = (sdl3_audio_t*)userdata;

   SDL_LockMutex(sdl->lock);
   sdl->data_moved = true;
   SDL_SignalCondition(sdl->cond);
   SDL_UnlockMutex(sdl->lock);
}

/**
 * Blocks until the stream callback signals device data movement.
 *
 * Callers test the stream's queue level without holding the lock, so
 * a callback can fire between that test and this wait.  The data_moved
 * token covers it: a signal with no waiter leaves the token set and
 * the wait returns immediately.
 *
 * @return False if the device stalls/stops moving data (to report short count).
 */
static bool sdl3_audio_wait_for_device(sdl3_audio_t *ctx)
{
   bool signalled = true;

   SDL_LockMutex(ctx->lock);
   if (!ctx->data_moved && !SDL_GetAtomicInt(&ctx->device_removed))
      signalled = SDL_WaitConditionTimeout(ctx->cond, ctx->lock,
            SDL3_AUDIO_STALL_TIMEOUT_MS);
   ctx->data_moved = false;
   SDL_UnlockMutex(ctx->lock);

   return signalled;
}

/**
 * Releases a context's removal watch, stream, and wake signalling.
 */
static void sdl3_audio_destroy_context(sdl3_audio_t *ctx)
{
   /* Stop the watch from touching the lock before tearing it down. */
   SDL_RemoveEventWatch(sdl3_audio_device_removed_watch, ctx);

   if (ctx->stream)
      SDL_DestroyAudioStream(ctx->stream);
   if (ctx->cond)
      SDL_DestroyCondition(ctx->cond);
   if (ctx->lock)
      SDL_DestroyMutex(ctx->lock);
}

static void sdl3_audio_free(void *data)
{
   sdl3_audio_t *sdl = (sdl3_audio_t*)data;

   if (sdl)
   {
      sdl3_audio_destroy_context(sdl);
      SDL_QuitSubSystem(SDL_INIT_AUDIO);
   }
   free(sdl);
}

/**
 * Prepares a new output stream.
 *
 * Publishes the device id for the removal watch, binds the wake
 * callback, resets write_raw state, prefills silence for latency
 * cushioning, and starts playback.  Removal/wake flags are owned
 * by the callers: init starts from the zeroed allocation and the
 * reopen path re-arms after this returns.
 */
static void sdl3_audio_prime_stream(sdl3_audio_t *sdl)
{
   void *tmp;

   SDL_SetAtomicU32(&sdl->devid, SDL_GetAudioStreamDevice(sdl->stream));
   SDL_SetAudioStreamGetCallback(sdl->stream, sdl3_audio_stream_cb, sdl);

   sdl->raw_rate = 0;
   sdl->in_cap = sdl->buffer_size;
   sdl->ratio = 1.0f;
   sdl->gain = 1.0f;

   if ((tmp = calloc(1, sdl->buffer_size)))
   {
      SDL_PutAudioStreamData(sdl->stream, tmp, (int)sdl->buffer_size);
      free(tmp);
   }

   SDL_ResumeAudioStreamDevice(sdl->stream);
}

static void *sdl3_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned block_frames, unsigned *new_rate)
{
   size_t frame_size, min_size;
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

   if (!(sdl->lock = SDL_CreateMutex()))
      goto error;
   if (!(sdl->cond = SDL_CreateCondition()))
      goto error;

   if (!(sdl->stream = sdl3_audio_open_stream(device, false, rate, latency,
         2, &sdl->spec, &device_sample_frames)))
      goto error;

   sdl->latency = latency;

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

   /* Publish the device id and register the watch before priming
    * so an unplug during setup is caught. */
   SDL_SetAtomicU32(&sdl->devid, SDL_GetAudioStreamDevice(sdl->stream));
   SDL_AddEventWatch(sdl3_audio_device_removed_watch, sdl);
   sdl3_audio_prime_stream(sdl);

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
   if (queued < 0)
      return 0;

   /* Convert SDL's stream-format byte count to device-rate bytes,
    * matching the expected units during write_raw. */
   if (sdl->raw_rate)
      queued = (int)((uint64_t)((size_t)queued / (2 * sizeof(int16_t)))
            * (unsigned)sdl->spec.freq / (unsigned)sdl->raw_rate
            * SDL_AUDIO_FRAMESIZE(sdl->spec));

   if ((size_t)queued >= sdl->buffer_size)
      return 0;
   return sdl->buffer_size - (size_t)queued;
}

/* Sleep until the stream callback reports the device moved data, until
 * at least len bytes fit below the queue cap, len capped at half the
 * cap so the wait always ends. Returns the free space then, or 0 when
 * the device has been removed or has stopped moving data. */
static size_t sdl3_audio_wait_writable(void *data, size_t len)
{
   sdl3_audio_t *sdl = (sdl3_audio_t*)data;
   size_t avail;
   /* Each wait ends on a timeout; this ends the loop when the device
    * keeps moving data but never frees enough. */
   int laps = 8;

   if (len > sdl->buffer_size / 2)
      len = sdl->buffer_size / 2;

   for (;;)
   {
      if (SDL_GetAtomicInt(&sdl->device_removed))
         return 0;
      if (laps-- < 0)
         return 0;
      avail = sdl3_audio_write_avail(sdl);
      if (avail >= len)
         return avail;
      if (!sdl3_audio_wait_for_device(sdl))
         return 0;
   }
}

/**
 * Attempts to reopen a lost audio device.
 *
 * @return True when writes may proceed on the new stream.
 */
static bool sdl3_audio_reopen_default(sdl3_audio_t *sdl)
{
   SDL_AudioSpec spec = {0};
   int device_sample_frames = 0;
   SDL_AudioStream *stream = NULL;

   RARCH_WARN("[SDL3 audio] Reopening output on the default device.\n");

   stream = sdl3_audio_open_stream(NULL, false, (unsigned)sdl->spec.freq,
         sdl->latency, 2, &spec, &device_sample_frames);
   if (stream && !SDL_SetAudioStreamFormat(stream, &sdl->spec, NULL))
   {
      SDL_DestroyAudioStream(stream);
      stream = NULL;
   }

   if (!stream)
   {
      RARCH_ERR("[SDL3 audio] Failed to reopen on the default device, waiting for one to be added.\n");
      SDL_SetAtomicInt(&sdl->defunct, 1);
      return false;
   }

   SDL_DestroyAudioStream(sdl->stream);
   sdl->stream = stream;
   sdl3_audio_prime_stream(sdl);

   /* Re-arm removal tracking now that prime published the new
    * device id.  The new stream is opened on the default device,
    * which never receives REMOVED (SDL migrates it instead), so
    * nothing can race this clear. */
   SDL_LockMutex(sdl->lock);
   SDL_SetAtomicInt(&sdl->device_removed, 0);
   sdl->data_moved = false;
   SDL_UnlockMutex(sdl->lock);
   return true;
}

/**
 * Checks whether or not the given audio stream is okay, and attempts
 * to reopen it if it was removed.
 *
 * @return False when writing is impossible and the device is gone.
 */
static bool sdl3_audio_stream_ok(sdl3_audio_t *sdl)
{
   if (SDL_GetAtomicInt(&sdl->defunct))
      return false;
   if (SDL_GetAtomicInt(&sdl->device_removed))
      return sdl3_audio_reopen_default(sdl);
   return true;
}

/**
 * Queues frame-aligned data up to the given cap.
 *
 * This will drops excess if nonblocking, or blocks until drained/timed out.
 *
 * @return The number of bytes queued, or -1 when the first write failed.
 */
static ssize_t sdl3_audio_queue(sdl3_audio_t *sdl, const void *s,
      size_t len, size_t cap, size_t frame_size)
{
   size_t size = 0;

   while (size < len)
   {
      int queued;
      size_t avail;

      /* The device is gone; drop the rest and let the next write
       * reopen on the default device. */
      if (SDL_GetAtomicInt(&sdl->device_removed))
         break;

      queued = SDL_GetAudioStreamQueued(sdl->stream);
      avail  = (queued < 0 || (size_t)queued >= cap)
            ? 0 : cap - (size_t)queued;

      /* Only queue whole frames. */
      avail -= avail % frame_size;

      if (avail == 0)
      {
         /* When the queue is full, and we can't wait on the
          * process for the queue to clear a bit, we're unable
          * to do anything, so skip the rest. */
         if (sdl->nonblock)
            break;

         /* Wait until the get callback is hit and there is space
          * available in the buffer to write. */
         if (!sdl3_audio_wait_for_device(sdl))
            break;
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
      }
   }

   return (ssize_t)size;
}

static ssize_t sdl3_audio_write(void *data, const void *s, size_t len)
{
   sdl3_audio_t *sdl = (sdl3_audio_t*)data;

   if (!sdl || !sdl3_audio_stream_ok(sdl))
      return -1;

   /* Reset stream format and SDL rate/gain to defaults when exiting
    * write_raw. */
   if (sdl->raw_rate)
   {
      if (!SDL_SetAudioStreamFormat(sdl->stream, &sdl->spec, NULL))
      {
         RARCH_ERR("[SDL3 audio] Failed to restore stream format: %s.\n", SDL_GetError());
         return -1;
      }
      SDL_SetAudioStreamFrequencyRatio(sdl->stream, 1.0f);
      SDL_SetAudioStreamGain(sdl->stream, 1.0f);
      sdl->raw_rate = 0;
      sdl->in_cap = sdl->buffer_size;
      sdl->ratio = 1.0f;
      sdl->gain = 1.0f;
   }

   return sdl3_audio_queue(sdl, s, len, sdl->buffer_size, 1);
}

/**
 * Bypass RetroArch resampling, send int16 stereo directly to SDL.
 */
static ssize_t sdl3_audio_write_raw(void *data, const int16_t *samples,
      size_t frames, unsigned input_rate, double rate_adjust, float volume)
{
   ssize_t size;
   const size_t frame_size = 2 * sizeof(int16_t);
   sdl3_audio_t *sdl = (sdl3_audio_t*)data;

   if (!sdl || !samples || input_rate == 0 || !sdl3_audio_stream_ok(sdl))
      return -1;

   /* Set stream input to core-rate int16 stereo and reconfigure
    * only if sample rate changes */
   if (sdl->raw_rate != (int)input_rate)
   {
      SDL_AudioSpec in_spec;
      in_spec.format = SDL_AUDIO_S16;
      in_spec.channels = 2;
      in_spec.freq = (int)input_rate;
      if (!SDL_SetAudioStreamFormat(sdl->stream, &in_spec, NULL))
      {
         RARCH_ERR("[SDL3 audio] Failed to set raw input format: %s.\n", SDL_GetError());
         return -1;
      }
      sdl->raw_rate = (int)input_rate;

      /* Convert buffer_size to the input rate, matching SDL's queue metrics. */
      sdl->in_cap = (size_t)((uint64_t)(sdl->buffer_size / SDL_AUDIO_FRAMESIZE(sdl->spec))
            * input_rate / (unsigned)sdl->spec.freq) * frame_size;
   }

   /* Invert rate_adjust for SDL's resampler, if needed. */
   if (rate_adjust > 0.0)
   {
      float ratio = (float)(1.0 / rate_adjust);
      if (ratio != sdl->ratio)
      {
         SDL_SetAudioStreamFrequencyRatio(sdl->stream, ratio);
         sdl->ratio = ratio;
      }
   }

   /* Apply write_raw gain directly to stream output. */
   if (volume != sdl->gain)
   {
      SDL_SetAudioStreamGain(sdl->stream, volume);
      sdl->gain = volume;
   }

   size = sdl3_audio_queue(sdl, samples, frames * frame_size, sdl->in_cap, frame_size);
   if (size < 0)
      return -1;
   return size / (ssize_t)frame_size;
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
   sdl3_audio_write_raw,
   sdl3_audio_wait_writable
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

   sdl3_audio_stream_cb(userdata, stream, additional_amount, total_amount);
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

   sdl3_audio_destroy_context(mic);
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

   if (!(mic->lock = SDL_CreateMutex()))
      goto error;
   if (!(mic->cond = SDL_CreateCondition()))
      goto error;

   /* Device streams open in a paused state. The frontend starts
    * them with start_mic. Microphones usually provide mono input. */
   if (!(mic->stream = sdl3_audio_open_stream(device, true, rate, latency,
         1, &mic->spec, &device_sample_frames)))
      goto error;

   SDL_SetAtomicU32(&mic->devid, SDL_GetAudioStreamDevice(mic->stream));
   SDL_AddEventWatch(sdl3_audio_device_removed_watch, mic);

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

/* Sleeps until the capture stream holds len bytes, then says how many it
 * holds. SDL3 gives no event to wait on, so this polls its stream with
 * the same short sleep the read loop uses, bounded so a removed device
 * returns nothing rather than holding the caller. */
static size_t sdl3_microphone_wait_readable(void *driver_context,
      void *mic_context, size_t len)
{
   sdl3_audio_t *mic = (sdl3_audio_t*)mic_context;
   int laps          = 8;
   int avail;

   if (!mic || !mic->stream)
      return 0;

   for (;;)
   {
      if (SDL_GetAtomicInt(&mic->device_removed))
         return 0;
      if ((avail = SDL_GetAudioStreamAvailable(mic->stream)) < 0)
         return 0;
      if (avail >= (int)len)
         return (size_t)avail;
      if (--laps < 0)
         return avail > 0 ? (size_t)avail : 0;
      SDL_Delay(1);
   }
}

static int sdl3_microphone_read(void *driver_context, void *mic_context,
      void *s, size_t len)
{
   size_t size = 0;
   sdl3_audio_t *sdl = (sdl3_audio_t*)driver_context;
   sdl3_audio_t *mic = (sdl3_audio_t*)mic_context;

   if (!sdl || !mic || !s)
      return -1;

   /* Avoid using a recording device that doesn't exist. */
   if (SDL_GetAtomicInt(&mic->device_removed))
      return -1;

   while (size < len)
   {
      int got;

      /* Safety check. */
      if (SDL_GetAtomicInt(&mic->device_removed))
         break;

      got = SDL_GetAudioStreamData(mic->stream, (char*)s + size, (int)(len - size));
      if (got < 0)
      {
         RARCH_ERR("[SDL3 audio] Failed to read from microphone stream: %s.\n", SDL_GetError());
         if (size == 0)
            return -1;
         break;
      }
      if (got > 0)
         size += (size_t)got;
      else
      {
         if (sdl->nonblock)
            break;

         /* Wait until the put callback signals that the device
          * can capture more samples. */
         if (!sdl3_audio_wait_for_device(mic))
            break;
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
   sdl3_microphone_wait_readable
};
#endif /* HAVE_MICROPHONE */

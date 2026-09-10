/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2023-2025 - Jesse Talavera-Greenberg
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

/* Two drivers live here, and a build may have either or both.
 *
 * "alsa" talks to alsa-lib, which is what a desktop Linux has. It is
 * everything below, up to the embedded section.
 *
 * "tinyalsa" talks to the kernel's PCM character devices directly -
 * the ioctls of <sound/asound.h> - and needs no library at all, which
 * is what an embedded build wants. It is the section at the end of
 * this file, under HAVE_TINYALSA, and its includes are its own: a
 * build with only HAVE_TINYALSA never sees an alsa-lib header. */

#include <stdlib.h>

#include <lists/string_list.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_ALSA
#include <alsa/asoundlib.h>
#include <alsa/pcm.h>
#endif
#include <errno.h>

#include "../audio_driver.h"
#ifdef HAVE_ALSA
#include "../common/alsa.h"
#endif
#include "../../verbosity.h"

#ifdef HAVE_ALSA

#ifdef HAVE_MICROPHONE
#include "../microphone_driver.h"

#define BYTES_TO_FRAMES(bytes, frame_bits)  ((bytes) * 8 / frame_bits)
#define FRAMES_TO_BYTES(frames, frame_bits) ((frames) * frame_bits / 8)

typedef struct alsa_microphone_handle
{
   snd_pcm_t *pcm;
   alsa_stream_info_t stream_info;
} alsa_microphone_handle_t;

typedef struct alsa_microphone
{
   bool nonblock;
} alsa_microphone_t;

static void *alsa_microphone_init(void)
{
   alsa_microphone_t *alsa = (alsa_microphone_t*)calloc(1, sizeof(alsa_microphone_t));

   if (!alsa)
   {
      RARCH_ERR("[ALSA] Failed to allocate driver context.\n");
      return NULL;
   }

   RARCH_LOG("[ALSA] Using ALSA version %s.\n", snd_asoundlib_version());

   return alsa;
}

static void alsa_microphone_close_mic(void *driver_context, void *mic_context);
static void alsa_microphone_free(void *driver_context)
{
   alsa_microphone_t *alsa = (alsa_microphone_t*)driver_context;
   /* The mic frontend should've closed all mics before calling free(). */

   if (alsa)
   {
      snd_config_update_free_global();
      free(alsa);
   }
}

static bool alsa_microphone_start_mic(void *driver_context, void *mic_context);

static int alsa_microphone_read(void *driver_context, void *mic_context, void *s, size_t len)
{
   snd_pcm_sframes_t size;
   snd_pcm_state_t state;
   alsa_microphone_t       *alsa = (alsa_microphone_t*)driver_context;
   alsa_microphone_handle_t *mic = (alsa_microphone_handle_t*)mic_context;
   uint8_t *buf                  = (uint8_t*)s;
   snd_pcm_sframes_t read        = 0;
   int errnum                    = 0;

   if (!alsa || !mic || !buf)
      return -1;

   size        = BYTES_TO_FRAMES(len, mic->stream_info.frame_bits);

   state = snd_pcm_state(mic->pcm);
   if (state != SND_PCM_STATE_RUNNING)
   {
      RARCH_WARN("[ALSA] Expected microphone \"%s\" to be in state RUNNING, was in state %s.\n",
                 snd_pcm_name(mic->pcm),
                 snd_pcm_state_name(state));

      errnum = snd_pcm_start(mic->pcm);
      if (errnum < 0)
      {
         RARCH_ERR("[ALSA] Failed to start microphone \"%s\": %s.\n",
                   snd_pcm_name(mic->pcm),
                   snd_strerror(errnum));

         return -1;
      }
   }

   if (alsa->nonblock)
   {
      while (size)
      {
         snd_pcm_sframes_t frames = snd_pcm_readi(mic->pcm, buf, size);

         if (frames == -EPIPE || frames == -EINTR || frames == -ESTRPIPE)
         {
            errnum = snd_pcm_recover(mic->pcm, frames, 0);
            if (errnum < 0)
            {
               RARCH_ERR("[ALSA] Failed to read from microphone: %s.\n", snd_strerror(frames));
               RARCH_ERR("[ALSA] Additionally, recovery failed with: %s.\n", snd_strerror(errnum));
               return -1;
            }

            break;
         }
         else if (frames == -EAGAIN)
            break;
         else if (frames < 0)
            return -1;

         read += frames;
         buf  += FRAMES_TO_BYTES(frames, mic->stream_info.frame_bits);
         size -= frames;
      }
   }
   else
   {
      bool eagain_retry         = true;

      while (size)
      {
         snd_pcm_sframes_t frames;
         int rc = snd_pcm_wait(mic->pcm, -1);

         if (rc == -EPIPE || rc == -ESTRPIPE || rc == -EINTR)
         {
            if (snd_pcm_recover(mic->pcm, rc, 1) < 0)
               return -1;
            continue;
         }

         frames = snd_pcm_readi(mic->pcm, buf, size);

         if (frames == -EPIPE || frames == -EINTR || frames == -ESTRPIPE)
         {
            if (snd_pcm_recover(mic->pcm, frames, 1) < 0)
               return -1;

            break;
         }
         else if (frames == -EAGAIN)
         {
            /* Definitely not supposed to happen. */
            if (eagain_retry)
            {
               eagain_retry = false;
               continue;
            }
            break;
         }
         else if (frames < 0)
            return -1;

         read += frames;
         buf  += FRAMES_TO_BYTES(frames, mic->stream_info.frame_bits);
         size -= frames;
      }
   }

   return FRAMES_TO_BYTES(read, mic->stream_info.frame_bits);
}

static bool alsa_microphone_mic_alive(const void *driver_context, const void *mic_context)
{
   alsa_microphone_handle_t *mic = (alsa_microphone_handle_t*)mic_context;
   (void)driver_context;

   if (!mic)
      return false;

   return snd_pcm_state(mic->pcm) == SND_PCM_STATE_RUNNING;
}

static void alsa_microphone_set_nonblock_state(void *driver_context, bool nonblock)
{
   alsa_microphone_t *alsa = (alsa_microphone_t*)driver_context;
   alsa->nonblock = nonblock;
}

static struct string_list *alsa_microphone_device_list_new(const void *data)
{
   return alsa_device_list_type_new("Input");
}

static void alsa_microphone_device_list_free(const void *driver_context, struct string_list *devices)
{
   string_list_free(devices);
   /* Does nothing if devices is NULL */
}

static void *alsa_microphone_open_mic(void *driver_context,
   const char *device,
   unsigned rate,
   unsigned latency,
   unsigned *new_rate)
{
   alsa_microphone_t       *alsa = (alsa_microphone_t*)driver_context;
   alsa_microphone_handle_t *mic = NULL;

   if (!alsa) /* If we weren't given a valid ALSA context... */
      return NULL;

   /* If the microphone context couldn't be allocated... */
   if (!(mic = calloc(1, sizeof(alsa_microphone_handle_t))))
      return NULL;

   /* channels hardcoded to 1, because we only support mono mic input */
   if (alsa_init_pcm(&mic->pcm, device, SND_PCM_STREAM_CAPTURE, rate, latency, 1,
            &mic->stream_info, new_rate, SND_PCM_NONBLOCK) < 0)
      goto error;

   return mic;

error:
   RARCH_ERR("[ALSA] Failed to initialize microphone.\n");

   alsa_microphone_close_mic(alsa, mic);

   return NULL;

}
static void alsa_microphone_close_mic(void *driver_context, void *mic_context)
{
   alsa_microphone_handle_t *mic = (alsa_microphone_handle_t*)mic_context;
   (void)driver_context;

   if (mic)
   {
      alsa_free_pcm(mic->pcm);
      free(mic);
   }
}

static bool alsa_microphone_start_mic(void *driver_context, void *mic_context)
{
   alsa_microphone_handle_t *mic = (alsa_microphone_handle_t*)mic_context;
   if (!mic)
      return false;
   return alsa_start_pcm(mic->pcm);
}

static bool alsa_microphone_stop_mic(void *driver_context, void *mic_context)
{
   alsa_microphone_handle_t *mic = (alsa_microphone_handle_t*)mic_context;
   if (!mic)
      return false;
   return alsa_stop_pcm(mic->pcm);
}

/* Bounded like the playback side's, and for the same reason: a stalled
 * capture must cost a dropped slice rather than a parked worker. */
#define ALSA_WAIT_READABLE_LAPS 8

/* Sleeps until the microphone has samples, then says how many. The
 * counterpart of alsa_wait_writable(): snd_pcm_avail() on a capture
 * stream reports frames ready to read rather than room to write, and
 * snd_pcm_start() begins capture where it began playback. Bounded by
 * two periods per wait and ALSA_WAIT_READABLE_LAPS waits, so a device
 * that has stopped delivering returns 0 and the caller retries later
 * rather than parking the capture thread. */
static size_t alsa_microphone_wait_readable(void *driver_context,
      void *mic_context, size_t len)
{
   alsa_microphone_handle_t *mic = (alsa_microphone_handle_t*)mic_context;
   snd_pcm_sframes_t want;
   int laps       = ALSA_WAIT_READABLE_LAPS;
   int timeout_ms;

   if (!mic || !mic->pcm)
      return 0;

   want       = BYTES_TO_FRAMES(len, mic->stream_info.frame_bits);
   timeout_ms = (int)(((unsigned long)mic->stream_info.period_frames * 2000ul)
         / (mic->stream_info.rate ? mic->stream_info.rate : 48000u));
   if (timeout_ms < 20)
      timeout_ms = 20;
   if (timeout_ms > 200)
      timeout_ms = 200;

   if (want > (snd_pcm_sframes_t)mic->stream_info.period_frames)
      want = (snd_pcm_sframes_t)mic->stream_info.period_frames;

   for (;;)
   {
      int rc;
      snd_pcm_sframes_t avail = snd_pcm_avail(mic->pcm);

      if (avail == -EPIPE || avail == -ESTRPIPE || avail == -EINTR)
      {
         if (snd_pcm_recover(mic->pcm, (int)avail, 1) < 0)
            return 0;
         if (--laps < 0)
            return 0;
         continue;
      }
      if (avail < 0)
         return 0;
      if (avail >= want)
         return FRAMES_TO_BYTES(avail, mic->stream_info.frame_bits);

      if (snd_pcm_state(mic->pcm) == SND_PCM_STATE_PREPARED)
      {
         rc = snd_pcm_start(mic->pcm);
         if (rc == -EPIPE || rc == -ESTRPIPE || rc == -EINTR)
         {
            if (snd_pcm_recover(mic->pcm, rc, 1) < 0)
               return 0;
         }
         else if (rc < 0)
            return 0;
      }

      rc = snd_pcm_wait(mic->pcm, timeout_ms);
      if (rc == 0)
         return 0;
      if (rc == -EPIPE || rc == -ESTRPIPE || rc == -EINTR)
      {
         if (snd_pcm_recover(mic->pcm, rc, 1) < 0)
            return 0;
      }
      else if (rc < 0)
         return 0;

      if (--laps < 0)
         return 0;
   }
}

static bool alsa_microphone_mic_use_float(const void *driver_context, const void *mic_context)
{
   alsa_microphone_handle_t *mic = (alsa_microphone_handle_t*)mic_context;
   return mic->stream_info.has_float;
}

microphone_driver_t microphone_alsa = {
        alsa_microphone_init,
        alsa_microphone_free,
        alsa_microphone_read,
        alsa_microphone_set_nonblock_state,
        "alsa",
        alsa_microphone_device_list_new,
        alsa_microphone_device_list_free,
        alsa_microphone_open_mic,
        alsa_microphone_close_mic,
        alsa_microphone_mic_alive,
        alsa_microphone_start_mic,
        alsa_microphone_stop_mic,
        alsa_microphone_mic_use_float,
        alsa_microphone_wait_readable
};
#endif

typedef struct alsa
{
   snd_pcm_t *pcm;
   alsa_stream_info_t stream_info;
   uint32_t requested_layout;
   /* Frames snd_pcm_writei() accepted since open; less what the device
    * still holds, it is what the device has consumed. */
   uint64_t frames_written;
   bool nonblock;
   /* Stopped, as the frontend sees it: alive() is its inverse. Held
    * says how: the stream paused with its buffer kept, or dropped,
    * its buffer discarded, to be prepared again on start. */
   bool is_paused;
   bool held;
} alsa_t;

/* The layout the device actually has, from its channel map; the
 * requested one where the map could not be read; stereo where the
 * device would not open wider. */
static uint32_t alsa_layout(void *data)
{
   alsa_t *alsa = (alsa_t*)data;
   if (!alsa || alsa->stream_info.channels <= 2)
      return AUDIO_LAYOUT_STEREO;
   if (alsa->stream_info.layout)
      return alsa->stream_info.layout;
   if (alsa->stream_info.channels == audio_layout_channels(alsa->requested_layout))
      return alsa->requested_layout;
   return AUDIO_LAYOUT_STEREO;
}

static bool alsa_use_float(void *data)
{
   alsa_t *alsa = (alsa_t*)data;
   return alsa->stream_info.has_float;
}

static void alsa_free(void *data);
static void *alsa_init(const char *device, unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   alsa_t *alsa = (alsa_t*)calloc(1, sizeof(alsa_t));

   if (!alsa)
   {
      RARCH_ERR("[ALSA] Failed to allocate driver context.\n");
      return NULL;
   }

   RARCH_LOG("[ALSA] Using ALSA version %s.\n", snd_asoundlib_version());

   alsa->requested_layout = audio_driver_requested_layout();
   if (alsa_init_pcm(&alsa->pcm, device, SND_PCM_STREAM_PLAYBACK, rate,
            latency, audio_layout_channels(alsa->requested_layout),
            &alsa->stream_info, new_rate, SND_PCM_NONBLOCK) < 0)
      goto error;

   return alsa;

error:
   RARCH_ERR("[ALSA] Failed to initialize.\n");

   alsa_free(alsa);

   return NULL;
}

#define BYTES_TO_FRAMES(bytes, frame_bits)  ((bytes) * 8 / frame_bits)
#define FRAMES_TO_BYTES(frames, frame_bits) ((frames) * frame_bits / 8)

/* Stopped is a state of this driver, not of the hardware: alive() is
 * false after stop() and true after start() on every device. A device
 * that can pause holds its buffer across the stop; one that cannot,
 * or whose pause fails when asked - a USB gadget, the Pulse and
 * PipeWire plugins - has its buffer dropped, and the stream is
 * prepared again on start and started by the first write. The
 * frontend's ring is the source of truth for what was queued. */
static bool alsa_start(void *data, bool is_shutdown)
{
   alsa_t *alsa = (alsa_t*)data;
   int ret;
   if (!alsa->is_paused)
      return true;

   if (alsa->held)
      ret = snd_pcm_pause(alsa->pcm, 0);
   else
      ret = snd_pcm_prepare(alsa->pcm);
   if (ret < 0)
   {
      RARCH_ERR("[ALSA] Failed to %s: %s.\n",
            alsa->held ? "unpause" : "prepare", snd_strerror(ret));
      return false;
   }
   alsa->is_paused = false;
   alsa->held      = false;
   return true;
}

static ssize_t alsa_write(void *data, const void *buf_, size_t len)
{
   ssize_t _len = 0;
   alsa_t *alsa           = (alsa_t*)data;
   const uint8_t *buf     = (const uint8_t*)buf_;
   snd_pcm_sframes_t size = BYTES_TO_FRAMES(len, alsa->stream_info.frame_bits);
   size_t frames_size     = alsa->stream_info.has_float ? sizeof(float) : sizeof(int16_t);

   /* Workaround buggy menu code.
    * If a write happens while we're paused, we might never progress. */
   if (alsa->is_paused && !alsa_start(alsa, false))
      return -1;

   if (alsa->nonblock)
   {
      while (size)
      {
         snd_pcm_sframes_t frames = snd_pcm_writei(alsa->pcm, buf, size);

         if (frames == -EPIPE || frames == -EINTR || frames == -ESTRPIPE)
         {
            if (snd_pcm_recover(alsa->pcm, frames, 1) < 0)
               return -1;

            break;
         }
         else if (frames == -EAGAIN)
            break;
         else if (frames < 0)
            return -1;

         _len  += FRAMES_TO_BYTES(frames, alsa->stream_info.frame_bits);
         alsa->frames_written += (uint64_t)frames;
         buf   += (frames << 1) * frames_size;
         size  -= frames;
      }
   }
   else
   {
      /* Write first; the device usually has the room, and a wait
       * before every write was a syscall and a scheduler round trip
       * for nothing. Wait only when it says EAGAIN, and then for a
       * bounded time - one buffer's worth - so a device that stops
       * draining costs a bounded pause and a short write, never the
       * audio thread. A wait that returns without space, twice, is
       * that device. */
      unsigned waits    = 0;
      int      wait_ms  = 100;
      if (alsa->stream_info.buffer_size && alsa->stream_info.rate)
         wait_ms = (int)((uint64_t)BYTES_TO_FRAMES(alsa->stream_info.buffer_size,
                     alsa->stream_info.frame_bits) * 1000 / alsa->stream_info.rate) + 1;

      while (size)
      {
         snd_pcm_sframes_t frames = snd_pcm_writei(alsa->pcm, buf, size);

         if (frames == -EPIPE || frames == -EINTR || frames == -ESTRPIPE)
         {
            if (snd_pcm_recover(alsa->pcm, frames, 1) < 0)
               return -1;
            break;
         }
         else if (frames == -EAGAIN)
         {
            int rc;
            if (waits++ >= 2)
               break;
            rc = snd_pcm_wait(alsa->pcm, wait_ms);
            if (rc == -EPIPE || rc == -ESTRPIPE || rc == -EINTR)
            {
               if (snd_pcm_recover(alsa->pcm, rc, 1) < 0)
                  return -1;
            }
            continue;
         }
         else if (frames < 0)
            return -1;

         waits = 0;
         _len += FRAMES_TO_BYTES(frames, alsa->stream_info.frame_bits);
         alsa->frames_written += (uint64_t)frames;
         buf  += (frames << 1) * frames_size;
         size -= frames;
      }
   }

   return _len;
}

static bool alsa_alive(void *data)
{
   alsa_t *alsa = (alsa_t*)data;
   if (!alsa)
      return false;
   return !alsa->is_paused;
}

static bool alsa_stop(void *data)
{
   alsa_t *alsa = (alsa_t*)data;
   int ret;
   if (alsa->is_paused)
      return true;

   if (alsa->stream_info.can_pause && snd_pcm_pause(alsa->pcm, 1) == 0)
   {
      alsa->is_paused = true;
      alsa->held      = true;
      return true;
   }
   ret = snd_pcm_drop(alsa->pcm);
   if (ret < 0)
   {
      RARCH_ERR("[ALSA] Failed to stop: %s.\n", snd_strerror(ret));
      return false;
   }
   alsa->is_paused = true;
   alsa->held      = false;
   return true;
}

static void alsa_set_nonblock_state(void *data, bool state)
{
   alsa_t *alsa = (alsa_t*)data;
   alsa->nonblock = state;
}

static void alsa_free(void *data)
{
   alsa_t *alsa = (alsa_t*)data;

   if (alsa)
   {
      alsa_free_pcm(alsa->pcm);

      snd_config_update_free_global();
      free(alsa);
   }
}

static size_t alsa_write_avail(void *data)
{
   alsa_t *alsa            = (alsa_t*)data;
   snd_pcm_sframes_t avail = snd_pcm_avail(alsa->pcm);

   if (avail < 0)
      return alsa->stream_info.buffer_size;

   return FRAMES_TO_BYTES(avail, alsa->stream_info.frame_bits);
}

static size_t alsa_buffer_size(void *data)
{
   alsa_t *alsa = (alsa_t*)data;
   return alsa->stream_info.buffer_size;
}

/* Iteration cap for alsa_wait_writable(): bounds recovery storms and
 * wakes that deliver no space, so one call costs at most this many
 * bounded waits before it hands the pass back to the caller. */
#define ALSA_WAIT_WRITABLE_LAPS 8

/* Sleep in snd_pcm_wait() until at least len bytes fit, capped at one
 * period: the wait wakes at period granularity (avail_min), and the
 * write's own loop takes care of the rest as it frees. A prepared but
 * not yet running stream reports its whole buffer free and returns at
 * once, which is what lets the first writes reach the start
 * threshold. Every wait is bounded to two periods' worth of time and
 * the loop to ALSA_WAIT_WRITABLE_LAPS, so on a device that stalls,
 * vanishes, or wakes without delivering space this returns 0 - skip
 * the pass, retry on a later wake - and the audio thread keeps
 * servicing its messages. A prepared stream that is short of space is
 * started here: it drains nothing and arms no poll on its own, and an
 * empty start's immediate underrun recovers to a prepared stream
 * reporting a full buffer. */
static size_t alsa_wait_writable(void *data, size_t len)
{
   alsa_t *alsa            = (alsa_t*)data;
   snd_pcm_sframes_t want  = BYTES_TO_FRAMES(len, alsa->stream_info.frame_bits);
   int laps                = ALSA_WAIT_WRITABLE_LAPS;
   int timeout_ms          = (int)(((unsigned long)
         alsa->stream_info.period_frames * 2000ul)
         / (alsa->stream_info.rate ? alsa->stream_info.rate : 48000u));

   if (timeout_ms < 20)
      timeout_ms = 20;
   if (timeout_ms > 200)
      timeout_ms = 200;

   if (want > (snd_pcm_sframes_t)alsa->stream_info.period_frames)
      want = (snd_pcm_sframes_t)alsa->stream_info.period_frames;

   for (;;)
   {
      int rc;
      snd_pcm_sframes_t avail = snd_pcm_avail(alsa->pcm);

      if (avail == -EPIPE || avail == -ESTRPIPE || avail == -EINTR)
      {
         if (snd_pcm_recover(alsa->pcm, (int)avail, 1) < 0)
            return 0;
         if (--laps < 0)
            return 0;
         continue;
      }
      if (avail < 0)
         return 0;
      if (avail >= want)
         return FRAMES_TO_BYTES(avail, alsa->stream_info.frame_bits);

      if (snd_pcm_state(alsa->pcm) == SND_PCM_STATE_PREPARED)
      {
         rc = snd_pcm_start(alsa->pcm);
         if (rc == -EPIPE || rc == -ESTRPIPE || rc == -EINTR)
         {
            if (snd_pcm_recover(alsa->pcm, rc, 1) < 0)
               return 0;
         }
         else if (rc < 0)
            return 0;
      }

      rc = snd_pcm_wait(alsa->pcm, timeout_ms);
      if (rc == 0)
         return 0;
      if (rc == -EPIPE || rc == -ESTRPIPE || rc == -EINTR)
      {
         if (snd_pcm_recover(alsa->pcm, rc, 1) < 0)
            return 0;
      }
      else if (rc < 0)
         return 0;
      if (--laps < 0)
         return 0;
   }
}

void *alsa_device_list_new(void *data)
{
   return alsa_device_list_type_new("Output");
}

void alsa_device_list_free(void *data, void *array_list_data)
{
   struct string_list *s = (struct string_list*)array_list_data;

   if (s)
      string_list_free(s);
}

/* What the device has consumed: frames accepted, less the frames still
 * queued in front of it. snd_pcm_delay() is that queue when the stream
 * runs; while it does not - paused, or recovering from an underrun -
 * the count holds. */
static size_t alsa_frames_consumed(void *data)
{
   alsa_t *alsa            = (alsa_t*)data;
   snd_pcm_sframes_t delay = 0;
   if (!alsa || !alsa->pcm)
      return 0;
   if (snd_pcm_delay(alsa->pcm, &delay) < 0 || delay < 0)
      delay = 0;
   if ((uint64_t)delay > alsa->frames_written)
      return 0;
   return (size_t)(alsa->frames_written - (uint64_t)delay);
}

audio_driver_t audio_alsa = {
   alsa_init,
   alsa_write,
   alsa_stop,
   alsa_start,
   alsa_alive,
   alsa_set_nonblock_state,
   alsa_free,
   alsa_use_float,
   "alsa",
   alsa_device_list_new,
   alsa_device_list_free,
   alsa_write_avail,
   alsa_buffer_size,
   NULL, /* write_raw */
   alsa_wait_writable,
   alsa_frames_consumed,
   NULL, /* underruns */
   alsa_layout
};

#endif /* HAVE_ALSA */



/* ================= the embedded driver: "tinyalsa" =================
 *
 * The kernel's PCM devices, spoken to directly: /dev/snd/pcmC<card>D<dev>p
 * and the ioctls of <sound/asound.h>. No library, which is the point -
 * an embedded build has the kernel and nothing else.
 *
 * Written against that header, which carries the Linux-syscall-note
 * exception so userspace may use it. It replaces a vendored copy of
 * Android's tinyalsa; the driver keeps its "tinyalsa" ident so no
 * configuration changes, but the implementation is RetroArch's and
 * can therefore take what the rest of the audio stack now expects:
 * float output, a channel layout, the frames the device has consumed,
 * and a bounded wait for writable space.
 *
 * The parameter negotiation is the one part of this that is not
 * obvious from the structs. A hw_params carries a mask per masked
 * parameter (access, format, subformat) and an interval per numeric
 * one (channels, rate, period size, buffer size, ...). The caller
 * fills every mask with every bit and every interval with its widest
 * range, narrows the ones it cares about, sets rmask to say which it
 * touched, and asks the kernel to refine: the kernel intersects each
 * with what the hardware can do and hands the result back. Refining
 * with a single value in an interval is how a specific rate is asked
 * for; refining with a range is how the hardware is asked what it
 * supports. HW_PARAMS then commits a fully determined set. */

#ifdef HAVE_TINYALSA

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <retro_endianness.h>
#include <retro_miscellaneous.h>

#include "../audio_upmix.h"

/* The kernel's PCM ABI, spelled out rather than included.
 *
 * <sound/asound.h> is where these come from, and it is not included
 * for two reasons: an embedded toolchain need not carry it, which is
 * the whole point of this driver; and where alsa-lib is also present
 * the two headers define the same names differently and cannot both
 * be in one translation unit. The layouts are an ABI - the kernel
 * will not move them - so a copy is safe in a way a copy of an
 * interface never is. Everything below is prefixed so nothing here
 * can collide with alsa-lib's names above. */

typedef unsigned long ealsa_uframes_t;
typedef signed long   ealsa_sframes_t;

#define EALSA_MASK_MAX          256
#define EALSA_P_ACCESS          0
#define EALSA_P_FORMAT          1
#define EALSA_P_SUBFORMAT       2
#define EALSA_P_FIRST_MASK      EALSA_P_ACCESS
#define EALSA_P_LAST_MASK       EALSA_P_SUBFORMAT
#define EALSA_P_SAMPLE_BITS     8
#define EALSA_P_CHANNELS        10
#define EALSA_P_RATE            11
#define EALSA_P_PERIOD_SIZE     13
#define EALSA_P_PERIODS         15
#define EALSA_P_BUFFER_SIZE     17
#define EALSA_P_TICK_TIME       19
#define EALSA_P_FIRST_INTERVAL  EALSA_P_SAMPLE_BITS
#define EALSA_P_LAST_INTERVAL   EALSA_P_TICK_TIME

#define EALSA_ACCESS_RW_INTERLEAVED 3
#define EALSA_FMT_S16_LE            2
#define EALSA_FMT_S16_BE            3
#define EALSA_FMT_FLOAT_LE          14
#define EALSA_FMT_FLOAT_BE          15

#define EALSA_INFO_PAUSE            0x00080000
#define EALSA_TSTAMP_NONE           0

struct ealsa_interval
{
   unsigned int min, max;
   unsigned int openmin:1, openmax:1, integer:1, empty:1;
};

struct ealsa_mask
{
   uint32_t bits[(EALSA_MASK_MAX + 31) / 32];
};

struct ealsa_hw_params
{
   unsigned int flags;
   struct ealsa_mask     masks[EALSA_P_LAST_MASK - EALSA_P_FIRST_MASK + 1];
   struct ealsa_mask     mres[5];
   struct ealsa_interval intervals[EALSA_P_LAST_INTERVAL - EALSA_P_FIRST_INTERVAL + 1];
   struct ealsa_interval ires[9];
   unsigned int rmask, cmask, info, msbits, rate_num, rate_den;
   ealsa_uframes_t fifo_size;
   unsigned char reserved[64];
};

struct ealsa_sw_params
{
   int tstamp_mode;
   unsigned int period_step;
   unsigned int sleep_min;
   ealsa_uframes_t avail_min;
   ealsa_uframes_t xfer_align;
   ealsa_uframes_t start_threshold;
   ealsa_uframes_t stop_threshold;
   ealsa_uframes_t silence_threshold;
   ealsa_uframes_t silence_size;
   ealsa_uframes_t boundary;
   unsigned int proto;
   unsigned int tstamp_type;
   unsigned char reserved[56];
};

struct ealsa_xferi
{
   ealsa_sframes_t result;
   void           *buf;
   ealsa_uframes_t frames;
};

#define EALSA_IOCTL_HW_REFINE     _IOWR('A', 0x10, struct ealsa_hw_params)
#define EALSA_IOCTL_HW_PARAMS     _IOWR('A', 0x11, struct ealsa_hw_params)
#define EALSA_IOCTL_SW_PARAMS     _IOWR('A', 0x13, struct ealsa_sw_params)
#define EALSA_IOCTL_DELAY         _IOR('A', 0x21, ealsa_sframes_t)
#define EALSA_IOCTL_PREPARE       _IO('A', 0x40)
#define EALSA_IOCTL_START         _IO('A', 0x42)
#define EALSA_IOCTL_DROP          _IO('A', 0x43)
#define EALSA_IOCTL_PAUSE         _IOW('A', 0x45, int)
#define EALSA_IOCTL_WRITEI_FRAMES _IOW('A', 0x50, struct ealsa_xferi)

/* The seam the harness replaces: everything this driver does to a
 * device goes through these. */
#ifndef EALSA_SYSCALLS
#define ealsa_open_dev(path, flags)  open((path), (flags))
#define ealsa_ioctl(fd, req, arg)    ioctl((fd), (req), (arg))
#define ealsa_close(fd)              close((fd))
#define ealsa_poll(fds, n, timeout)  poll((fds), (n), (timeout))
#endif

#define EALSA_FORMAT_S16   0
#define EALSA_FORMAT_FLOAT 1

typedef struct ealsa
{
   int      fd;
   unsigned rate;
   unsigned channels;
   uint32_t layout;
   unsigned frame_bits;
   size_t   buffer_size;      /* bytes */
   size_t   period_frames;
   size_t   buffer_frames;
   uint64_t frames_written;   /* handed to the device since it opened */
   bool     nonblock;
   bool     has_float;
   bool     can_pause;
   bool     is_paused;
   bool     running;
} ealsa_t;

/* ---- the parameter set ---------------------------------------- */

static INLINE struct ealsa_mask *ealsa_mask(struct ealsa_hw_params *p,
      unsigned param)
{
   return &p->masks[param - EALSA_P_FIRST_MASK];
}

static INLINE struct ealsa_interval *ealsa_interval(struct ealsa_hw_params *p,
      unsigned param)
{
   return &p->intervals[param - EALSA_P_FIRST_INTERVAL];
}

/* Every mask full and every interval as wide as it goes: the set the
 * kernel narrows. rmask names every parameter, so a refine returns
 * what the hardware can do with all of them. */
static void ealsa_params_any(struct ealsa_hw_params *p)
{
   unsigned i;
   memset(p, 0, sizeof(*p));
   for (i = EALSA_P_FIRST_MASK; i <= EALSA_P_LAST_MASK; i++)
      memset(ealsa_mask(p, i)->bits, 0xff, sizeof(ealsa_mask(p, i)->bits));
   for (i = EALSA_P_FIRST_INTERVAL; i <= EALSA_P_LAST_INTERVAL; i++)
   {
      struct ealsa_interval *iv = ealsa_interval(p, i);
      iv->min = 0;
      iv->max = UINT_MAX;
   }
   p->rmask = ~0u;
   p->cmask = 0;
   p->info  = ~0u;
}

static void ealsa_mask_only(struct ealsa_hw_params *p, unsigned param,
      unsigned bit)
{
   struct ealsa_mask *m = ealsa_mask(p, param);
   memset(m->bits, 0, sizeof(m->bits));
   m->bits[bit >> 5] = 1u << (bit & 31);
}

static void ealsa_interval_exact(struct ealsa_hw_params *p, unsigned param,
      unsigned value)
{
   struct ealsa_interval *iv = ealsa_interval(p, param);
   iv->min     = value;
   iv->max     = value;
   iv->integer = 1;
   iv->openmin = 0;
   iv->openmax = 0;
}

/* A parameter the device may settle anywhere within. HW_PARAMS
 * refines and then chooses, so a range is a request the card can meet
 * its own way - which an exact value is not: a card whose periods
 * come in twos, or whose period size is a multiple of 512, refuses an
 * exact 4 x 768 outright and the open fails. */
static void ealsa_interval_range(struct ealsa_hw_params *p, unsigned param,
      unsigned lo, unsigned hi)
{
   struct ealsa_interval *iv = ealsa_interval(p, param);
   iv->min     = lo;
   iv->max     = hi;
   iv->integer = 1;
}

static unsigned ealsa_interval_min(const struct ealsa_hw_params *p, unsigned param)
{
   return p->intervals[param - EALSA_P_FIRST_INTERVAL].min;
}

static unsigned ealsa_interval_max(const struct ealsa_hw_params *p, unsigned param)
{
   return p->intervals[param - EALSA_P_FIRST_INTERVAL].max;
}

/* The kernel's format numbers for what this driver offers. */
static unsigned ealsa_format_bit(int format)
{
   if (format == EALSA_FORMAT_FLOAT)
      return is_little_endian() ? EALSA_FMT_FLOAT_LE
                                : EALSA_FMT_FLOAT_BE;
   return is_little_endian() ? EALSA_FMT_S16_LE
                             : EALSA_FMT_S16_BE;
}

static unsigned ealsa_format_bits(int format)
{
   return (format == EALSA_FORMAT_FLOAT) ? 32 : 16;
}

/* Does the hardware take this format at this channel count and rate?
 * A refine that comes back without an error says yes, and says what
 * it would settle on. */
static bool ealsa_probe(int fd, int format, unsigned channels, unsigned rate)
{
   struct ealsa_hw_params p;
   ealsa_params_any(&p);
   ealsa_mask_only(&p, EALSA_P_ACCESS, EALSA_ACCESS_RW_INTERLEAVED);
   ealsa_mask_only(&p, EALSA_P_FORMAT, ealsa_format_bit(format));
   ealsa_interval_exact(&p, EALSA_P_CHANNELS, channels);
   ealsa_interval_exact(&p, EALSA_P_RATE, rate);
   return ealsa_ioctl(fd, EALSA_IOCTL_HW_REFINE, &p) == 0;
}

/* ---- open and setup -------------------------------------------- */

static bool ealsa_set_params(ealsa_t *ea, int format, unsigned channels,
      unsigned rate, unsigned latency_ms)
{
   struct ealsa_hw_params hw;
   struct ealsa_sw_params sw;
   unsigned period_frames, periods = 4;

   /* A period of a quarter of the asked-for latency, so the device
    * wakes four times across a buffer that holds the latency. */
   period_frames = (rate * latency_ms) / (1000 * periods);
   if (period_frames < 64)
      period_frames = 64;

   ealsa_params_any(&hw);
   ealsa_mask_only(&hw, EALSA_P_ACCESS, EALSA_ACCESS_RW_INTERLEAVED);
   ealsa_mask_only(&hw, EALSA_P_FORMAT, ealsa_format_bit(format));
   ealsa_interval_exact(&hw, EALSA_P_CHANNELS, channels);
   ealsa_interval_exact(&hw, EALSA_P_RATE, rate);
   /* The period and the buffer, asked for as "no smaller than this".
    *
    * An exact value is refused outright by a card whose periods come
    * in twos or whose period size is a multiple of 512 - the refine
    * narrows to nothing and the open fails. A range is met, but the
    * kernel settles every interval at its minimum, so a range alone
    * opens the smallest buffer the card has: eight milliseconds where
    * sixty-four were asked for, which underruns on every frame. So
    * the range is refined first to see what the card can do, the
    * smallest value at or above what was wanted is taken from the
    * result, and that is committed. */
   ealsa_interval_range(&hw, EALSA_P_PERIOD_SIZE, period_frames, UINT_MAX);
   ealsa_interval_range(&hw, EALSA_P_PERIODS, 2, 16);
   if (ealsa_ioctl(ea->fd, EALSA_IOCTL_HW_REFINE, &hw) < 0)
      return false;
   period_frames = ealsa_interval_min(&hw, EALSA_P_PERIOD_SIZE);
   if (!period_frames)
      return false;
   ealsa_interval_exact(&hw, EALSA_P_PERIOD_SIZE, period_frames);

   /* The buffer the latency asks for, rounded up to a whole number of
    * the period the card settled on, then asked for exactly so the
    * card picks the count that makes it. A card that cannot make that
    * buffer keeps the range it refined and the kernel chooses. */
   {
      struct ealsa_hw_params want = hw;
      unsigned buffer_frames = (rate * latency_ms) / 1000;
      unsigned n             = (buffer_frames + period_frames - 1) / period_frames;
      if (n < 2)
         n = 2;
      ealsa_interval_exact(&want, EALSA_P_BUFFER_SIZE, n * period_frames);
      if (ealsa_ioctl(ea->fd, EALSA_IOCTL_HW_REFINE, &want) == 0)
         hw = want;
   }

   if (ealsa_ioctl(ea->fd, EALSA_IOCTL_HW_PARAMS, &hw) < 0)
      return false;

   ea->period_frames = ealsa_interval_min(&hw, EALSA_P_PERIOD_SIZE);
   ea->buffer_frames = ealsa_interval_min(&hw, EALSA_P_BUFFER_SIZE);
   if (ea->buffer_frames < ea->period_frames)
      ea->buffer_frames = ea->period_frames
            * ealsa_interval_min(&hw, EALSA_P_PERIODS);
   if (!ea->period_frames)
      ea->period_frames = period_frames;
   if (!ea->buffer_frames)
      ea->buffer_frames = ea->period_frames * periods;
   ea->can_pause     = (hw.info & EALSA_INFO_PAUSE) != 0;
   ea->rate          = rate;
   ea->channels      = channels;
   ea->has_float     = (format == EALSA_FORMAT_FLOAT);
   ea->frame_bits    = ealsa_format_bits(format) * channels;
   ea->buffer_size   = ea->buffer_frames * ea->frame_bits / 8;

   memset(&sw, 0, sizeof(sw));
   sw.tstamp_mode       = EALSA_TSTAMP_NONE;
   sw.avail_min         = ea->period_frames;
   /* Started explicitly, so the first write does not begin playing a
    * buffer that is not yet full. */
   sw.start_threshold   = ea->buffer_frames + 1;
   /* Never stopped on an underrun: an xrun is recovered by the write
    * path, and a stopped stream would need a prepare the caller did
    * not ask for. */
   sw.stop_threshold    = ULONG_MAX;
   sw.silence_threshold = 0;
   sw.silence_size      = 0;
   /* The pointer wrap: the largest multiple of the buffer that fits,
    * which is what the kernel compares the application pointer
    * against. The test is on the value before doubling, so the
    * doubling cannot overflow - testing the doubled value wraps it to
    * zero at the top and the loop never ends. */
   sw.boundary          = ea->buffer_frames;
   while (sw.boundary <= (ULONG_MAX >> 2))
      sw.boundary *= 2;
   sw.stop_threshold    = sw.boundary;
   if (ealsa_ioctl(ea->fd, EALSA_IOCTL_SW_PARAMS, &sw) < 0)
      return false;
   return ealsa_ioctl(ea->fd, EALSA_IOCTL_PREPARE, NULL) == 0;
}

/* Frames the device still holds. DELAY rather than STATUS: the status
 * struct carries timespecs whose layout has moved between kernel
 * versions, and the delay is the only figure this driver wants. */
static ealsa_sframes_t ealsa_delay(ealsa_t *ea)
{
   ealsa_sframes_t d = 0;
   if (ealsa_ioctl(ea->fd, EALSA_IOCTL_DELAY, &d) < 0)
      return 0;
   return d;
}

/* Frames the device could take now: its buffer less what it holds. */
static size_t ealsa_avail(ealsa_t *ea)
{
   ealsa_sframes_t d = ealsa_delay(ea);
   if (d <= 0)
      return ea->buffer_frames;
   if ((size_t)d >= ea->buffer_frames)
      return 0;
   return ea->buffer_frames - (size_t)d;
}

static bool ealsa_recover(ealsa_t *ea)
{
   if (ealsa_ioctl(ea->fd, EALSA_IOCTL_PREPARE, NULL) < 0)
      return false;
   ea->running = false;
   return true;
}

/* ---- the driver ------------------------------------------------ */

typedef struct ealsa tinyalsa_t;

static void tinyalsa_free(void *data)
{
   ealsa_t *ea = (ealsa_t*)data;
   if (!ea)
      return;
   if (ea->fd >= 0)
   {
      ealsa_ioctl(ea->fd, EALSA_IOCTL_DROP, NULL);
      ealsa_close(ea->fd);
   }
   free(ea);
}

static void *tinyalsa_init(const char *devicestr, unsigned rate,
      unsigned latency,  unsigned *new_rate)
{
   char     path[64];
   unsigned card = 0, device = 0, want_channels, ch;
   uint32_t want_layout = audio_driver_requested_layout();
   int      format      = EALSA_FORMAT_S16;
   ealsa_t *ea          = (ealsa_t*)calloc(1, sizeof(*ea));

   if (!ea)
      return NULL;
   ea->fd = -1;

   /* "<card>,<device>", either part optional. */
   if (devicestr)
   {
      char *endp;
      unsigned long v = strtoul(devicestr, &endp, 10);
      if (endp != devicestr)
      {
         card = (unsigned)v;
         if (*endp == ',')
         {
            const char *p = endp + 1;
            v = strtoul(p, &endp, 10);
            if (endp != p)
               device = (unsigned)v;
         }
      }
   }

   snprintf(path, sizeof(path), "/dev/snd/pcmC%uD%up", card, device);
   if ((ea->fd = ealsa_open_dev(path, O_RDWR)) < 0)
   {
      RARCH_ERR("[TINYALSA] Cannot open %s.\n", path);
      goto error;
   }
   RARCH_LOG("[TINYALSA] Using card %u, device %u.\n", card, device);

   /* The rate the device will take: asked for first, and if it is
    * refused, the nearest end of what the hardware reports. The
    * frontend is told through new_rate either way. */
   {
      struct ealsa_hw_params p;
      ealsa_params_any(&p);
      ealsa_mask_only(&p, EALSA_P_ACCESS, EALSA_ACCESS_RW_INTERLEAVED);
      if (ealsa_ioctl(ea->fd, EALSA_IOCTL_HW_REFINE, &p) < 0)
      {
         RARCH_ERR("[TINYALSA] The device reports no usable parameters.\n");
         goto error;
      }
      {
         unsigned min = ealsa_interval_min(&p, EALSA_P_RATE);
         unsigned max = ealsa_interval_max(&p, EALSA_P_RATE);
         if (rate < min || rate > max)
         {
            RARCH_WARN("[TINYALSA] %u Hz is outside the device's %u-%u Hz.\n",
                  rate, min, max);
            rate = (rate < min) ? min : max;
         }
      }
   }

   /* Float where the device takes it: the pipeline is float, and an
    * s16 device is the only reason to narrow. */
   if (ealsa_probe(ea->fd, EALSA_FORMAT_FLOAT, 2, rate))
      format = EALSA_FORMAT_FLOAT;

   /* The layout the frontend asked for, if the device has that many
    * channels; stereo otherwise. */
   want_channels = audio_layout_channels(want_layout);
   if (want_channels < 2 || !audio_layout_supported(want_layout))
   {
      want_channels = 2;
      want_layout   = AUDIO_LAYOUT_STEREO;
   }
   for (ch = want_channels; ch >= 2; ch -= 2)
   {
      if (ealsa_probe(ea->fd, format, ch, rate))
         break;
      if (ch == 2)
      {
         RARCH_ERR("[TINYALSA] The device takes neither the layout nor stereo.\n");
         goto error;
      }
   }
   if (ch != want_channels)
   {
      RARCH_WARN("[TINYALSA] %u channels refused; opening %u.\n", want_channels, ch);
      want_layout = AUDIO_LAYOUT_STEREO;
   }
   ea->layout = (ch == want_channels) ? want_layout : AUDIO_LAYOUT_STEREO;

   if (!latency)
      latency = 64;
   if (!ealsa_set_params(ea, format, ch, rate, latency))
   {
      RARCH_ERR("[TINYALSA] The device refused the parameters.\n");
      goto error;
   }

   if (new_rate)
      *new_rate = ea->rate;

   RARCH_LOG("[TINYALSA] %u Hz, %u channels, %s, layout 0x%x.\n",
         ea->rate, ea->channels, ea->has_float ? "float" : "s16",
         (unsigned)ea->layout);
   RARCH_LOG("[TINYALSA] Period %u frames, buffer %u frames (%u bytes).\n",
         (unsigned)ea->period_frames, (unsigned)ea->buffer_frames,
         (unsigned)ea->buffer_size);
   RARCH_LOG("[TINYALSA] Can pause: %s.\n", ea->can_pause ? "yes" : "no");
   return ea;

error:
   tinyalsa_free(ea);
   return NULL;
}

static ssize_t tinyalsa_write(void *data, const void *buf, size_t len)
{
   ealsa_t *ea      = (ealsa_t*)data;
   const uint8_t *p = (const uint8_t*)buf;
   size_t   frames  = len * 8 / ea->frame_bits;
   size_t   written = 0;

   while (frames)
   {
      struct ealsa_xferi x;
      x.buf    = (void*)p;
      x.frames = frames;
      x.result = 0;

      if (ealsa_ioctl(ea->fd, EALSA_IOCTL_WRITEI_FRAMES, &x) < 0)
      {
         if (errno == EAGAIN)
         {
            if (ea->nonblock)
               break;
            /* Blocking: wait for the device to free a period, and
             * come back short rather than spinning if it does not. */
            {
               struct pollfd pfd;
               int           pr;
               pfd.fd      = ea->fd;
               pfd.events  = POLLOUT;
               pfd.revents = 0;
               pr          = ealsa_poll(&pfd, 1, 100);
               /* A signal is not the device saying no: RetroArch takes
                * them for its timers, and treating one as a refusal
                * returned a short write for no reason. */
               if (pr < 0 && errno == EINTR)
                  continue;
               if (pr <= 0)
                  break;
            }
            continue;
         }
         if (errno == EPIPE || errno == ESTRPIPE)
         {
            /* An underrun, or a resume after a suspend: prepared
             * again and the frames go out on the next turn. */
            if (!ealsa_recover(ea))
               return -1;
            continue;
         }
         return written ? (ssize_t)(written * ea->frame_bits / 8) : -1;
      }
      if (x.result <= 0)
         break;
      p                   += (size_t)x.result * ea->frame_bits / 8;
      frames              -= x.result;
      written             += x.result;
      ea->frames_written  += x.result;
      /* Started once the buffer holds something: the threshold is
       * past the buffer's end, so the kernel never starts it. */
      if (!ea->running && !ea->is_paused)
      {
         if (ealsa_ioctl(ea->fd, EALSA_IOCTL_START, NULL) == 0)
            ea->running = true;
      }
   }
   return (ssize_t)(written * ea->frame_bits / 8);
}

/* Frames the device has played: what was handed to it, less what it
 * still holds. The sink rate estimate reads this. */
static size_t tinyalsa_frames_consumed(void *data)
{
   ealsa_t        *ea = (ealsa_t*)data;
   ealsa_sframes_t d  = ealsa_delay(ea);
   uint64_t queued    = (d > 0) ? (uint64_t)d : 0;
   if (queued > ea->frames_written)
      return (size_t)ea->frames_written;
   return (size_t)(ea->frames_written - queued);
}

static size_t tinyalsa_write_avail(void *data)
{
   ealsa_t *ea = (ealsa_t*)data;
   return ealsa_avail(ea) * ea->frame_bits / 8;
}

static size_t tinyalsa_buffer_size(void *data)
{
   ealsa_t *ea = (ealsa_t*)data;
   return ea->buffer_size;
}

/* Waits until the device can take @len bytes, bounded so a device
 * that has stopped draining hands the pass back. */
static size_t tinyalsa_wait_writable(void *data, size_t len)
{
   ealsa_t *ea    = (ealsa_t*)data;
   size_t   want  = len * 8 / ea->frame_bits;
   unsigned laps  = 8;
   int      ms    = (int)(ea->period_frames * 2000 / (ea->rate ? ea->rate : 48000));
   if (ms < 2)
      ms = 2;
   else if (ms > 100)
      ms = 100;
   if (want > ea->buffer_frames)
      want = ea->buffer_frames;
   while (laps--)
   {
      struct pollfd pfd;
      size_t avail = ealsa_avail(ea);
      if (avail >= want)
         return avail * ea->frame_bits / 8;
      if (ea->nonblock || ea->is_paused)
         break;
      pfd.fd      = ea->fd;
      pfd.events  = POLLOUT;
      pfd.revents = 0;
      {
         int pr = ealsa_poll(&pfd, 1, ms);
         if (pr < 0 && errno == EINTR)
            continue;      /* a signal, not a device short of room */
         if (pr <= 0)
            break;
      }
   }
   return ealsa_avail(ea) * ea->frame_bits / 8;
}

static bool tinyalsa_stop(void *data)
{
   ealsa_t *ea = (ealsa_t*)data;
   if (ea->is_paused)
      return true;
   if (ea->can_pause && ea->running)
   {
      int on = 1;
      if (ealsa_ioctl(ea->fd, EALSA_IOCTL_PAUSE, &on) == 0)
      {
         ea->is_paused = true;
         return true;
      }
   }
   /* No pause, or it was refused: dropped, and prepared again so the
    * next start has somewhere to begin. */
   ealsa_ioctl(ea->fd, EALSA_IOCTL_DROP, NULL);
   ea->running   = false;
   ea->is_paused = ealsa_ioctl(ea->fd, EALSA_IOCTL_PREPARE, NULL) == 0;
   return ea->is_paused;
}

static bool tinyalsa_start(void *data, bool is_shutdown)
{
   ealsa_t *ea = (ealsa_t*)data;
   (void)is_shutdown;
   if (!ea->is_paused)
      return true;
   if (ea->can_pause && ea->running)
   {
      int off = 0;
      if (ealsa_ioctl(ea->fd, EALSA_IOCTL_PAUSE, &off) < 0)
         return false;
   }
   ea->is_paused = false;
   return true;
}

static bool tinyalsa_alive(void *data)
{
   ealsa_t *ea = (ealsa_t*)data;
   return ea && !ea->is_paused;
}

static void tinyalsa_set_nonblock_state(void *data, bool state)
{
   ealsa_t *ea = (ealsa_t*)data;
   ea->nonblock = state;
}

static bool tinyalsa_use_float(void *data)
{
   ealsa_t *ea = (ealsa_t*)data;
   return ea->has_float;
}

static uint32_t tinyalsa_layout(void *data)
{
   ealsa_t *ea = (ealsa_t*)data;
   return ea ? ea->layout : AUDIO_LAYOUT_STEREO;
}

/* The playback devices the kernel exposes, as "<card>,<device>" with
 * the name the driver reports. */
static void *tinyalsa_device_list_new(void *data)
{
   struct string_list *list = string_list_new();
   unsigned card, device;
   (void)data;
   if (!list)
      return NULL;
   for (card = 0; card < 8; card++)
   {
      for (device = 0; device < 8; device++)
      {
         char path[64], label[32];
         int fd;
         union string_list_elem_attr attr;
         snprintf(path, sizeof(path), "/dev/snd/pcmC%uD%up", card, device);
         /* Opened only to see whether it is there; the name the
          * frontend shows is the "<card>,<device>" it passes back. */
         if ((fd = ealsa_open_dev(path, O_RDWR | O_NONBLOCK)) < 0)
            continue;
         ealsa_close(fd);
         snprintf(label, sizeof(label), "%u,%u", card, device);
         attr.i = 0;
         string_list_append(list, label, attr);
      }
   }
   return list;
}

static void tinyalsa_device_list_free(void *data, void *array_list_data)
{
   struct string_list *s = (struct string_list*)array_list_data;
   (void)data;
   if (s)
      string_list_free(s);
}

audio_driver_t audio_tinyalsa = {
   tinyalsa_init,
   tinyalsa_write,
   tinyalsa_stop,
   tinyalsa_start,
   tinyalsa_alive,
   tinyalsa_set_nonblock_state,
   tinyalsa_free,
   tinyalsa_use_float,
   "tinyalsa",
   tinyalsa_device_list_new,
   tinyalsa_device_list_free,
   tinyalsa_write_avail,
   tinyalsa_buffer_size,
   NULL, /* write_raw */
   tinyalsa_wait_writable,
   tinyalsa_frames_consumed,
   NULL, /* underruns */
   tinyalsa_layout
};

#endif /* HAVE_TINYALSA */

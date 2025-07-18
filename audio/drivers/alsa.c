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

#include <stdlib.h>

#include <lists/string_list.h>
#include <string/stdstring.h>

#include <alsa/asoundlib.h>
#include <alsa/pcm.h>
#include <asm-generic/errno.h>

#include "../audio_driver.h"
#include "../common/alsa.h"
#include "../../verbosity.h"

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
   size_t frames_size;
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
   frames_size = mic->stream_info.has_float ? sizeof(float) : sizeof(int16_t);

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
         buf  += frames_size;
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
         buf  += frames_size;
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
   (void)driver_context;
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
        alsa_microphone_mic_use_float
};
#endif

typedef struct alsa
{
   snd_pcm_t *pcm;
   alsa_stream_info_t stream_info;
   bool nonblock;
   bool is_paused;
} alsa_t;

static bool alsa_use_float(void *data)
{
   alsa_t *alsa = (alsa_t*)data;
   return alsa->stream_info.has_float;
}

static void alsa_free(void *data);
static void *alsa_init(const char *device, unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   alsa_t *alsa = (alsa_t*)calloc(1, sizeof(alsa_t));

   if (!alsa)
   {
      RARCH_ERR("[ALSA] Failed to allocate driver context.\n");
      return NULL;
   }

   RARCH_LOG("[ALSA] Using ALSA version %s.\n", snd_asoundlib_version());

   if (alsa_init_pcm(&alsa->pcm, device, SND_PCM_STREAM_PLAYBACK, rate, latency, 2, &alsa->stream_info, new_rate, SND_PCM_NONBLOCK) < 0)
   {
      goto error;
   }

   return alsa;

error:
   RARCH_ERR("[ALSA] Failed to initialize.\n");

   alsa_free(alsa);

   return NULL;
}

#define BYTES_TO_FRAMES(bytes, frame_bits)  ((bytes) * 8 / frame_bits)
#define FRAMES_TO_BYTES(frames, frame_bits) ((frames) * frame_bits / 8)

static bool alsa_start(void *data, bool is_shutdown)
{
   alsa_t *alsa = (alsa_t*)data;
   if (!alsa->is_paused)
      return true;

   if (     alsa->stream_info.can_pause
         && alsa->is_paused)
   {
      int ret = snd_pcm_pause(alsa->pcm, 0);

      if (ret < 0)
      {
         RARCH_ERR("[ALSA] Failed to unpause: %s.\n",
               snd_strerror(ret));
         return false;
      }

      alsa->is_paused = false;
   }
   return true;
}

static ssize_t alsa_write(void *data, const void *buf_, size_t len)
{
   ssize_t written        = 0;
   alsa_t *alsa           = (alsa_t*)data;
   const uint8_t *buf     = (const uint8_t*)buf_;
   snd_pcm_sframes_t size = BYTES_TO_FRAMES(len, alsa->stream_info.frame_bits);
   size_t frames_size     = alsa->stream_info.has_float ? sizeof(float) : sizeof(int16_t);

   /* Workaround buggy menu code.
    * If a write happens while we're paused, we might never progress. */
   if (alsa->is_paused)
      if (!alsa_start(alsa, false))
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

         written += frames;
         buf     += (frames << 1) * frames_size;
         size    -= frames;
      }
   }
   else
   {
      bool eagain_retry         = true;

      while (size)
      {
         snd_pcm_sframes_t frames;
         int rc = snd_pcm_wait(alsa->pcm, -1);

         if (rc == -EPIPE || rc == -ESTRPIPE || rc == -EINTR)
         {
            if (snd_pcm_recover(alsa->pcm, rc, 1) < 0)
               return -1;
            continue;
         }

         frames = snd_pcm_writei(alsa->pcm, buf, size);

         if (frames == -EPIPE || frames == -EINTR || frames == -ESTRPIPE)
         {
            if (snd_pcm_recover(alsa->pcm, frames, 1) < 0)
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

         written += frames;
         buf     += (frames << 1) * frames_size;
         size    -= frames;
      }
   }

   return written;
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
   if (alsa->is_paused)
	  return true;

   if (alsa->stream_info.can_pause
         && !alsa->is_paused)
   {
      int ret = snd_pcm_pause(alsa->pcm, 1);

      if (ret < 0)
         return false;

      alsa->is_paused = true;
   }

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
};

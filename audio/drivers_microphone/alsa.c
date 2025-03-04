/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2023 Jesse Talavera-Greenberg
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

#include <alsa/asoundlib.h>
#include <alsa/pcm.h>
#include <asm-generic/errno.h>

#include "audio/common/alsa.h" /* For some common functions/types */
#include "audio/microphone_driver.h"
#include "verbosity.h"

#define BYTES_TO_FRAMES(bytes, frame_bits)  ((bytes) * 8 / frame_bits)
#define FRAMES_TO_BYTES(frames, frame_bits) ((frames) * frame_bits / 8)

typedef struct alsa_microphone_handle
{
   snd_pcm_t *pcm;
   alsa_stream_info_t stream_info;
} alsa_microphone_handle_t;

typedef struct alsa
{
   bool nonblock;
} alsa_microphone_t;

static void *alsa_microphone_init(void)
{
   alsa_microphone_t *alsa = (alsa_microphone_t*)calloc(1, sizeof(alsa_microphone_t));

   if (!alsa)
   {
      RARCH_ERR("[ALSA]: Failed to allocate driver context\n");
      return NULL;
   }

   RARCH_LOG("[ALSA]: Using ALSA version %s\n", snd_asoundlib_version());

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
      RARCH_WARN("[ALSA]: Expected microphone \"%s\" to be in state RUNNING, was in state %s\n",
                 snd_pcm_name(mic->pcm),
                 snd_pcm_state_name(state));

      errnum = snd_pcm_start(mic->pcm);
      if (errnum < 0)
      {
         RARCH_ERR("[ALSA]: Failed to start microphone \"%s\": %s\n",
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
               RARCH_ERR("[ALSA]: Failed to read from microphone: %s\n", snd_strerror(frames));
               RARCH_ERR("[ALSA]: Additionally, recovery failed with: %s\n", snd_strerror(errnum));
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
   RARCH_ERR("[ALSA]: Failed to initialize microphone...\n");

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

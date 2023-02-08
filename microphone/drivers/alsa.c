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

#include "audio/drivers/alsa.h" /* For some common functions/types */
#include "microphone/microphone_driver.h"
#include "verbosity.h"


#define BYTES_TO_FRAMES(bytes, frame_bits)  ((bytes) * 8 / frame_bits)
#define FRAMES_TO_BYTES(frames, frame_bits) ((frames) * frame_bits / 8)

typedef struct alsa_microphone_handle
{
   snd_pcm_t *pcm;
   alsa_stream_info_t stream_info;
   bool is_paused;
} alsa_microphone_handle_t;

typedef struct alsa
{
   /* Only one microphone is supported right now;
    * the driver state should track multiple microphone handles,
    * but the driver *context* should track multiple microphone contexts */
   alsa_microphone_handle_t *microphone;

   bool nonblock;
   bool is_paused;
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

static void alsa_microphone_close_mic(void *driver_context, void *microphone_context);
static void alsa_microphone_free(void *driver_context)
{
   alsa_microphone_t *alsa = (alsa_microphone_t*)driver_context;

   if (alsa)
   {
      alsa_microphone_close_mic(alsa, alsa->microphone);

      snd_config_update_free_global();
      free(alsa);
   }
}

static bool alsa_microphone_set_mic_active(void *driver_context, void *microphone_context, bool enabled);
static ssize_t alsa_microphone_read(void *driver_context, void *microphone_context, void *buf_, size_t size_)
{
   alsa_microphone_t *alsa              = (alsa_microphone_t*)driver_context;
   alsa_microphone_handle_t *microphone = (alsa_microphone_handle_t*)microphone_context;
   uint8_t *buf                  = (uint8_t*)buf_;
   snd_pcm_sframes_t read        = 0;
   int errnum                    = 0;
   snd_pcm_sframes_t size;
   size_t frames_size;
   snd_pcm_state_t state;

   if (!alsa || !microphone || !buf)
      return -1;

   size        = BYTES_TO_FRAMES(size_, microphone->stream_info.frame_bits);
   frames_size = microphone->stream_info.has_float ? sizeof(float) : sizeof(int16_t);

   /* Workaround buggy menu code.
    * If a read happens while we're paused, we might never progress. */
   if (microphone->is_paused)
      if (!alsa_microphone_set_mic_active(alsa, microphone, true))
         return -1;

   state = snd_pcm_state(microphone->pcm);
   if (state != SND_PCM_STATE_RUNNING)
   {
      RARCH_WARN("[ALSA]: Expected microphone \"%s\" to be in state RUNNING, was in state %s\n",
                 snd_pcm_name(microphone->pcm),
                 snd_pcm_state_name(state));

      errnum = snd_pcm_start(microphone->pcm);
      if (errnum < 0)
      {
         RARCH_ERR("[ALSA]: Failed to start microphone \"%s\": %s\n",
                   snd_pcm_name(microphone->pcm),
                   snd_strerror(errnum));

         return -1;
      }
   }

   if (alsa->nonblock)
   {
      while (size)
      {
         snd_pcm_sframes_t frames = snd_pcm_readi(microphone->pcm, buf, size);

         if (frames == -EPIPE || frames == -EINTR || frames == -ESTRPIPE)
         {
            errnum = snd_pcm_recover(microphone->pcm, frames, 0);
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
         int rc = snd_pcm_wait(microphone->pcm, -1);

         if (rc == -EPIPE || rc == -ESTRPIPE || rc == -EINTR)
         {
            if (snd_pcm_recover(microphone->pcm, rc, 1) < 0)
               return -1;
            continue;
         }

         frames = snd_pcm_readi(microphone->pcm, buf, size);

         if (frames == -EPIPE || frames == -EINTR || frames == -ESTRPIPE)
         {
            if (snd_pcm_recover(microphone->pcm, frames, 1) < 0)
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

   return FRAMES_TO_BYTES(read, microphone->stream_info.frame_bits);
}

static bool alsa_microphone_stop(void *driver_context)
{
   alsa_microphone_t *alsa = (alsa_microphone_t*)driver_context;
   if (alsa->is_paused)
      return true;

   if (alsa->microphone)
   {
      /* Stop the microphone independently of whether it's paused;
       * note that upon alsa_microphone_start, the microphone won't resume
       * if it was previously paused */
      int errnum = snd_pcm_pause(alsa->microphone->pcm, true);
      if (errnum < 0)
      {
         RARCH_ERR("[ALSA]: Failed to pause microphone \"%s\": %s\n",
                   snd_pcm_name(alsa->microphone->pcm),
                   snd_strerror(errnum));
         return false;
      }
   }

   return true;
}

static bool alsa_microphone_start(void *driver_context, bool is_shutdown)
{
   alsa_microphone_t *alsa = (alsa_microphone_t*)driver_context;
   if (!alsa->is_paused)
      return true;

   alsa->is_paused = false;

   if (alsa->microphone && !alsa->microphone->is_paused)
   { /* If the mic wasn't paused at the time the overall driver was paused... */
      snd_pcm_state_t mic_state = snd_pcm_state(alsa->microphone->pcm);

      /* If we're calling this function with a pending microphone,
       * (as happens when a core requests a microphone at the start),
       * the mic will be in the PREPARED state rather than the PAUSED state.
       **/
      switch (mic_state)
      {
         case SND_PCM_STATE_PREPARED:
         {
            int errnum = snd_pcm_start(alsa->microphone->pcm);
            if (errnum < 0)
            {
               RARCH_ERR("[ALSA]: Failed to start microphone \"%s\": %s\n",
                         snd_pcm_name(alsa->microphone->pcm),
                         snd_strerror(errnum));

               return false;
            }
            break;
         }
         case SND_PCM_STATE_PAUSED:
         {
            int errnum = snd_pcm_pause(alsa->microphone->pcm, false);
            if (errnum < 0)
            {
               RARCH_ERR("[ALSA]: Failed to unpause microphone \"%s\" in state %s: %s\n",
                         snd_pcm_name(alsa->microphone->pcm),
                         snd_pcm_state_name(snd_pcm_state(alsa->microphone->pcm)),
                         snd_strerror(errnum));

               return false;
            }
            break;
         }
         default:
         {
            RARCH_ERR("[ALSA]: Expected microphone \"%s\" to be in state PREPARED or PAUSED, it was in %s\n",
                      snd_pcm_name(alsa->microphone->pcm),
                      snd_pcm_state_name(mic_state));
            return false;
         }
      }
   }

   return true;
}

static bool alsa_microphone_alive(void *driver_context)
{
   alsa_microphone_t *alsa = (alsa_microphone_t*)driver_context;
   if (!alsa)
      return false;
   return !alsa->is_paused;
}

bool alsa_set_mic_enabled_internal(snd_pcm_t *microphone, bool enabled)
{
   snd_pcm_state_t microphone_state = snd_pcm_state(microphone);
   int errnum                       = 0;

   if (enabled)
   { /* If we're trying to unpause a mic (or maybe activate it for the first time)... */
      switch (microphone_state)
      {
         case SND_PCM_STATE_PAUSED: /* If we're unpausing a valid (but paused) mic... */
            if ((errnum = snd_pcm_pause(microphone, false)) < 0)
            { /* ...but we failed... */
               goto error;
            }
            break;
         case SND_PCM_STATE_PREPARED:
            /* If we're activating this mic for the first time... */
            if ((errnum = snd_pcm_start(microphone)) < 0)
            { /* ..but we failed... */
               goto error;
            }
            break;
         default:
            goto unexpected_state;
      }
   }
   else
   { /* We're pausing this mic */
      switch (microphone_state)
      {
         case SND_PCM_STATE_PREPARED:
         case SND_PCM_STATE_RUNNING:
            /* If we're pausing an active mic... */
            if ((errnum = snd_pcm_pause(microphone, true)) < 0)
            { /* ...but we failed... */
               goto error;
            }
            break;
         default:
            goto unexpected_state;
      }
   }

   RARCH_DBG("[ALSA]: %s microphone \"%s\", transitioning from %s to %s\n",
             enabled ? "Unpaused" : "Paused",
             snd_pcm_name(microphone),
             snd_pcm_state_name(microphone_state),
             snd_pcm_state_name(snd_pcm_state(microphone)));

   return true;
   error:
   RARCH_ERR("[ALSA]: Failed to %s microphone \"%s\" in state %s: %s\n",
             enabled ? "unpause" : "pause",
             snd_pcm_name(microphone),
             snd_pcm_state_name(microphone_state),
             snd_strerror(errnum));

   return false;

   unexpected_state:
   RARCH_ERR("[ALSA]: Failed to %s microphone \"%s\" in unexpected state %s\n",
             enabled ? "unpause" : "pause",
             snd_pcm_name(microphone),
             snd_pcm_state_name(microphone_state));

   return false;
}

static bool alsa_microphone_set_mic_active(void *driver_context, void *microphone_context, bool enabled)
{
   alsa_microphone_t *alsa               = (alsa_microphone_t*)driver_context;
   alsa_microphone_handle_t  *microphone = (alsa_microphone_handle_t*)microphone_context;

   if (!alsa || !microphone)
      return false;
   /* Both params must be non-null */

   if (!microphone->stream_info.can_pause)
   {
      RARCH_WARN("[ALSA]: Microphone \"%s\" cannot be paused\n", snd_pcm_name(microphone->pcm));
      return true;
   }

   if (!alsa->is_paused)
   { /* If the entire audio driver isn't paused... */
      if (alsa_set_mic_enabled_internal(microphone->pcm, enabled))
      {
         microphone->is_paused = !enabled;
         return true;
      }
      return false;
   }

   return true;
}

static bool alsa_microphone_mic_alive(const void *driver_context, const void *microphone_context)
{
   alsa_microphone_t *alsa              = (alsa_microphone_t*)driver_context;
   alsa_microphone_handle_t *microphone = (alsa_microphone_handle_t*)microphone_context;

   if (!alsa || !microphone)
      return false;
   /* Both params must be non-null */

   return !microphone->is_paused;
   /* The mic might be paused due to app requirements,
    * or it might be stopped because the entire audio driver is stopped. */
}

static void alsa_microphone_set_nonblock_state(void *driver_context, bool nonblock)
{
   alsa_microphone_t *alsa = (alsa_microphone_t*)driver_context;
   alsa->nonblock = nonblock;
}

static struct string_list *alsa_microphone_device_list_new(const void *data)
{
   (void)data;
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
   unsigned block_frames,
   unsigned *new_rate)
{
   alsa_microphone_t *alsa              = (alsa_microphone_t*)driver_context;
   alsa_microphone_handle_t *microphone = NULL;

   if (!alsa) /* If we weren't given a valid ALSA context... */
      return NULL;

   microphone = calloc(1, sizeof(alsa_microphone_handle_t));

   if (!microphone) /* If the microphone context couldn't be allocated... */
      return NULL;

   /* channels hardcoded to 1, because we only support mono mic input */
   if (alsa_init_pcm(&microphone->pcm, device, SND_PCM_STREAM_CAPTURE, rate, latency, 1, &microphone->stream_info, new_rate, SND_PCM_NONBLOCK) < 0)
   {
      goto error;
   }

   alsa->microphone = microphone;
   return microphone;

error:
   RARCH_ERR("[ALSA]: Failed to initialize microphone...\n");

   alsa_microphone_close_mic(alsa, microphone);

   return NULL;

}
static void alsa_microphone_close_mic(void *driver_context, void *microphone_context)
{
   alsa_microphone_t *alsa              = (alsa_microphone_t *)driver_context;
   alsa_microphone_handle_t *microphone = (alsa_microphone_handle_t*)microphone_context;

   if (alsa && microphone)
   {
      alsa_free_pcm(microphone->pcm);

      alsa->microphone = NULL;
      free(microphone);
   }
}

static bool alsa_microphone_start_mic(void *driver_context, void *microphone_context)
{
   return alsa_microphone_set_mic_active(driver_context, microphone_context, true);
}

static bool alsa_microphone_stop_mic(void *driver_context, void *microphone_context)
{
   return alsa_microphone_set_mic_active(driver_context, microphone_context, true);
}

static bool alsa_microphone_mic_use_float(const void *driver_context, const void *microphone_context)
{
   alsa_microphone_handle_t *microphone = (alsa_microphone_handle_t*)microphone_context;

   return microphone->stream_info.has_float;
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

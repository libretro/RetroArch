/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <alsa/asoundlib.h>

#include "../audio_driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"

typedef struct alsa
{
   snd_pcm_t *pcm;
   size_t buffer_size;
   bool nonblock;
   bool has_float;
   bool can_pause;
   bool is_paused;
} alsa_t;

static bool alsa_use_float(void *data)
{
   alsa_t *alsa = (alsa_t*)data;
   return alsa->has_float;
}

static bool find_float_format(snd_pcm_t *pcm, void *data)
{
   snd_pcm_hw_params_t *params = (snd_pcm_hw_params_t*)data;

   if (snd_pcm_hw_params_test_format(pcm, params, SND_PCM_FORMAT_FLOAT) == 0)
   {
      RARCH_LOG("ALSA: Using floating point format.\n");
      return true;
   }

   RARCH_LOG("ALSA: Using signed 16-bit format.\n");
   return false;
}

static void *alsa_init(const char *device, unsigned rate, unsigned latency)
{
   snd_pcm_format_t format;
   snd_pcm_uframes_t buffer_size;
   snd_pcm_hw_params_t *params = NULL;
   snd_pcm_sw_params_t *sw_params = NULL;

   unsigned latency_usec = latency * 1000;
   unsigned channels = 2;
   unsigned periods = 4;

   const char *alsa_dev = "default";
   alsa_t *alsa = (alsa_t*)calloc(1, sizeof(alsa_t));

   if (!alsa)
      return NULL;

   if (device)
      alsa_dev = device;

   if (snd_pcm_open(
            &alsa->pcm, alsa_dev, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK) < 0)
      goto error;

   if (snd_pcm_hw_params_malloc(&params) < 0)
      goto error;

   alsa->has_float = find_float_format(alsa->pcm, params);
   format = alsa->has_float ? SND_PCM_FORMAT_FLOAT : SND_PCM_FORMAT_S16;

   if (snd_pcm_hw_params_any(alsa->pcm, params) < 0)
      goto error;

   if (snd_pcm_hw_params_set_access(
            alsa->pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
      goto error;

   if (snd_pcm_hw_params_set_format(alsa->pcm, params, format) < 0)
      goto error;

   if (snd_pcm_hw_params_set_channels(alsa->pcm, params, channels) < 0)
      goto error;

   if (snd_pcm_hw_params_set_rate(alsa->pcm, params, rate, 0) < 0)
      goto error;

   if (snd_pcm_hw_params_set_buffer_time_near(
            alsa->pcm, params, &latency_usec, NULL) < 0)
      goto error;

   if (snd_pcm_hw_params_set_periods_near(
            alsa->pcm, params, &periods, NULL) < 0)
      goto error;

   if (snd_pcm_hw_params(alsa->pcm, params) < 0)
      goto error;

   /* Shouldn't have to bother with this, 
    * but some drivers are apparently broken. */
   if (snd_pcm_hw_params_get_period_size(params, &buffer_size, NULL))
      snd_pcm_hw_params_get_period_size_min(params, &buffer_size, NULL);

   RARCH_LOG("ALSA: Period size: %d frames\n", (int)buffer_size);

   if (snd_pcm_hw_params_get_buffer_size(params, &buffer_size))
      snd_pcm_hw_params_get_buffer_size_max(params, &buffer_size);

   RARCH_LOG("ALSA: Buffer size: %d frames\n", (int)buffer_size);

   alsa->buffer_size = snd_pcm_frames_to_bytes(alsa->pcm, buffer_size);
   alsa->can_pause = snd_pcm_hw_params_can_pause(params);

   RARCH_LOG("ALSA: Can pause: %s.\n", alsa->can_pause ? "yes" : "no");

   if (snd_pcm_sw_params_malloc(&sw_params) < 0)
      goto error;

   if (snd_pcm_sw_params_current(alsa->pcm, sw_params) < 0)
      goto error;

   if (snd_pcm_sw_params_set_start_threshold(
            alsa->pcm, sw_params, buffer_size / 2) < 0)
      goto error;

   if (snd_pcm_sw_params(alsa->pcm, sw_params) < 0)
      goto error;

   snd_pcm_hw_params_free(params);
   snd_pcm_sw_params_free(sw_params);

   return alsa;

error:
   RARCH_ERR("ALSA: Failed to initialize...\n");
   if (params)
      snd_pcm_hw_params_free(params);

   if (sw_params)
      snd_pcm_sw_params_free(sw_params);

   if (alsa)
   {
      if (alsa->pcm)
         snd_pcm_close(alsa->pcm);

      free(alsa);
   }
   return NULL;
}

static ssize_t alsa_write(void *data, const void *buf_, size_t size_)
{
   alsa_t *alsa              = (alsa_t*)data;
   const uint8_t *buf        = (const uint8_t*)buf_;
   bool eagain_retry         = true;
   snd_pcm_sframes_t written = 0;
   snd_pcm_sframes_t size    = snd_pcm_bytes_to_frames(alsa->pcm, size_);

   while (size)
   {
      snd_pcm_sframes_t frames;

      if (!alsa->nonblock)
      {
         int rc = snd_pcm_wait(alsa->pcm, -1);

         if (rc == -EPIPE || rc == -ESTRPIPE || rc == -EINTR)
         {
            if (snd_pcm_recover(alsa->pcm, rc, 1) < 0)
            {
               RARCH_ERR("[ALSA]: (#1) Failed to recover from error (%s)\n",
                     snd_strerror(rc));
               return -1;
            }
            continue;
         }
      }

      frames = snd_pcm_writei(alsa->pcm, buf, size);

      if (frames == -EPIPE || frames == -EINTR || frames == -ESTRPIPE)
      {
         if (snd_pcm_recover(alsa->pcm, frames, 1) < 0)
         {
            RARCH_ERR("[ALSA]: (#2) Failed to recover from error (%s)\n",
                  snd_strerror(frames));
            return -1;
         }

         break;
      }
      else if (frames == -EAGAIN && !alsa->nonblock)
      {
         /* Definitely not supposed to happen. */
         RARCH_WARN("[ALSA]: poll() was signaled, but EAGAIN returned from write.\n"
               "Your ALSA driver might be subtly broken.\n");

         if (eagain_retry)
         {
            eagain_retry = false;
            continue;
         }
         return written;
      }
      else if (frames == -EAGAIN) /* Expected if we're running nonblock. */
         return written;
      else if (frames < 0)
      {
         RARCH_ERR("[ALSA]: Unknown error occured (%s).\n",
               snd_strerror(frames));
         return -1;
      }

      written += frames;
      buf     += (frames << 1) *
         (alsa->has_float ? sizeof(float) : sizeof(int16_t));
      size    -= frames;
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

   if (alsa->can_pause
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

static bool alsa_start(void *data)
{
   alsa_t *alsa = (alsa_t*)data;

   if (alsa->can_pause
         && alsa->is_paused)
   {
      int ret = snd_pcm_pause(alsa->pcm, 0);

      if (ret < 0)
      {
         RARCH_ERR("[ALSA]: Failed to unpause: %s.\n",
               snd_strerror(ret));
         return false;
      }

      alsa->is_paused = false;
   }
   return true;
}

static void alsa_free(void *data)
{
   alsa_t *alsa = (alsa_t*)data;

   if (alsa)
   {
      if (alsa->pcm)
      {
         snd_pcm_drop(alsa->pcm);
         snd_pcm_close(alsa->pcm);
      }
      free(alsa);
   }
}

static size_t alsa_write_avail(void *data)
{
   alsa_t *alsa = (alsa_t*)data;
   snd_pcm_sframes_t avail = snd_pcm_avail(alsa->pcm);

   if (avail < 0)
   {
#if 0
      RARCH_WARN("[ALSA]: snd_pcm_avail() failed: %s\n",
            snd_strerror(avail));
#endif
      return alsa->buffer_size;
   }

   return snd_pcm_frames_to_bytes(alsa->pcm, avail);
}

static size_t alsa_buffer_size(void *data)
{
   alsa_t *alsa = (alsa_t*)data;
   return alsa->buffer_size;
}

static void *alsa_device_list_new(void *data)
{
   void **hints, **n;
   union string_list_elem_attr attr;
   struct string_list *s = string_list_new();

   if (!s)
      return NULL;

   attr.i = 0;

   if (snd_device_name_hint(-1, "pcm", &hints) != 0)
      goto error;

   n      = hints;

   while (*n != NULL)
   {
      char *name = snd_device_name_get_hint(*n, "NAME");
      char *io   = snd_device_name_get_hint(*n, "IOID");
      char *desc = snd_device_name_get_hint(*n, "DESC");

      /* description of device IOID - input / output identifcation
       * ("Input" or "Output"), NULL means both) */

      if (!io || !strcmp(io,"Output"))
         string_list_append(s, name, attr);

      if (name)
         free(name);
      if (io)
         free(io);
      if (desc)
         free(desc);

      n++;
   }

   /* free hint buffer too */
   snd_device_name_free_hint(hints);

   return s;

error:
   string_list_free(s);
   return NULL;
}

static void alsa_device_list_free(void *data, void *array_list_data)
{
   struct string_list *s = (struct string_list*)array_list_data;

   if (!s)
      return;

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

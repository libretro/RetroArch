/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 Daniel De Matteis
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
#include <lists/string_list.h>
#include <string/stdstring.h>

#include <alsa/asoundlib.h>
#include <asm-generic/errno.h>

#include "alsa.h"

#include "../audio_driver.h"
#include "../../verbosity.h"

int alsa_init_pcm(snd_pcm_t **pcm,
   const char* device,
   snd_pcm_stream_t stream,
   unsigned rate,
   unsigned latency,
   unsigned channels,
   alsa_stream_info_t *stream_info,
   unsigned *new_rate,
   int mode)
{
   snd_pcm_format_t format;
   snd_pcm_uframes_t buffer_size;
   snd_pcm_hw_params_t *params    = NULL;
   snd_pcm_sw_params_t *sw_params = NULL;
   unsigned latency_usec          = latency * 1000;
   unsigned periods               = 4;
   unsigned orig_rate             = rate;
   const char *alsa_dev           = device ? device : "default";
   int errnum                     = 0;

   RARCH_DBG("[ALSA]: Requesting device \"%s\" for %s stream\n", alsa_dev, snd_pcm_stream_name(stream));

   if ((errnum = snd_pcm_open(pcm, alsa_dev, stream, mode)) < 0)
   {
      RARCH_ERR("[ALSA]: Failed to open %s stream on device \"%s\": %s\n",
            snd_pcm_stream_name(stream),
            alsa_dev,
            snd_strerror(errnum));

      goto error;
   }

   if ((errnum = snd_pcm_hw_params_malloc(&params)) < 0)
   {
      RARCH_ERR("[ALSA]: Failed to allocate hardware parameters: %s\n",
            snd_strerror(errnum));

      goto error;
   }

   if ((errnum = snd_pcm_hw_params_any(*pcm, params)) < 0)
   {
      RARCH_ERR("[ALSA]: Failed to query hardware parameters from %s device \"%s\": %s\n",
            snd_pcm_stream_name(stream),
            snd_pcm_name(*pcm),
            snd_strerror(errnum));

      goto error;
   }

   format = (snd_pcm_hw_params_test_format(*pcm, params, SND_PCM_FORMAT_FLOAT) == 0)
         ? SND_PCM_FORMAT_FLOAT : SND_PCM_FORMAT_S16;
   stream_info->has_float = (format == SND_PCM_FORMAT_FLOAT);

   RARCH_LOG("[ALSA]: Using %s sample format for %s device \"%s\"\n",
         snd_pcm_format_name(format),
         snd_pcm_stream_name(stream),
         snd_pcm_name(*pcm)
   );

   if ((errnum = snd_pcm_hw_params_set_access(*pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
   {
      RARCH_ERR("[ALSA]: Failed to set %s access for %s device \"%s\": %s\n",
            snd_pcm_access_name(SND_PCM_ACCESS_RW_INTERLEAVED),
            snd_pcm_stream_name(stream),
            snd_pcm_name(*pcm),
            snd_strerror(errnum));

      goto error;
   }
   stream_info->frame_bits = snd_pcm_format_physical_width(format) * channels;

   if ((errnum = snd_pcm_hw_params_set_format(*pcm, params, format)) < 0)
   {
      RARCH_ERR("[ALSA]: Failed to set %s format for %s device \"%s\": %s\n",
            snd_pcm_format_name(format),
            snd_pcm_stream_name(stream),
            snd_pcm_name(*pcm),
            snd_strerror(errnum));

      goto error;
   }

   if ((errnum = snd_pcm_hw_params_set_channels(*pcm, params, channels)) < 0)
   {
      RARCH_ERR("[ALSA]: Failed to set %u-channel audio for %s device \"%s\": %s\n",
            channels,
            snd_pcm_stream_name(stream),
            snd_pcm_name(*pcm),
            snd_strerror(errnum));

      goto error;
   }

   /* Don't allow rate resampling when probing for the default rate (but ignore if this call fails) */
   if ((errnum = snd_pcm_hw_params_set_rate_resample(*pcm, params, false)) < 0)
   {
      RARCH_WARN("[ALSA]: Failed to request a default unsampled rate for %s device \"%s\": %s\n",
            snd_pcm_stream_name(stream),
            snd_pcm_name(*pcm),
            snd_strerror(errnum));
   }

   if ((errnum = snd_pcm_hw_params_set_rate_near(*pcm, params, &rate, 0)) < 0)
   {
      RARCH_ERR("[ALSA]: Failed to request a rate near %uHz for %s device \"%s\": %s\n",
            rate,
            snd_pcm_stream_name(stream),
            snd_pcm_name(*pcm),
            snd_strerror(errnum));

      goto error;
   }

   if (new_rate)
      *new_rate = rate;

   if ((snd_pcm_hw_params_set_buffer_time_near(*pcm, params, &latency_usec, NULL)) < 0)
   {
      RARCH_ERR("[ALSA]: Failed to request a buffer time near %uus for %s device \"%s\": %s\n",
            latency_usec,
            snd_pcm_stream_name(stream),
            snd_pcm_name(*pcm),
            snd_strerror(errnum));

      goto error;

   }

   if ((errnum = snd_pcm_hw_params_set_periods_near(*pcm, params, &periods, NULL)) < 0)
   {
      RARCH_ERR("[ALSA]: Failed to request %u periods per buffer for %s device \"%s\": %s\n",
            periods,
            snd_pcm_stream_name(stream),
            snd_pcm_name(*pcm),
            snd_strerror(errnum));

      goto error;
   }

   if ((errnum = snd_pcm_hw_params(*pcm, params)) < 0)
   { /* This calls snd_pcm_prepare() under the hood */
      RARCH_ERR("[ALSA]: Failed to install hardware parameters for %s device \"%s\": %s\n",
            snd_pcm_stream_name(stream),
            snd_pcm_name(*pcm),
            snd_strerror(errnum));

      goto error;
   }

   /* Shouldn't have to bother with this,
    * but some drivers are apparently broken. */
   if ((errnum = snd_pcm_hw_params_get_period_size(params, &stream_info->period_frames, NULL)) < 0)
   {
      RARCH_WARN("[ALSA]: Failed to get an exact period size from %s device \"%s\": %s\n",
            snd_pcm_stream_name(stream),
            snd_pcm_name(*pcm),
            snd_strerror(errnum));
      RARCH_WARN("[ALSA]: Trying the minimum period size instead\n");

      if ((errnum = snd_pcm_hw_params_get_period_size_min(params, &stream_info->period_frames, NULL)) < 0)
      {
         RARCH_ERR("[ALSA]: Failed to get min period size from %s device \"%s\": %s\n",
               snd_pcm_stream_name(stream),
               snd_pcm_name(*pcm),
               snd_strerror(errnum));
         goto error;
      }
   }

   stream_info->period_size = snd_pcm_frames_to_bytes(*pcm, stream_info->period_frames);
   if (stream_info->period_size < 0)
   {
      RARCH_ERR("[ALSA]: Failed to convert a period size of %lu frames to bytes: %s\n",
            stream_info->period_frames,
            snd_strerror(stream_info->period_frames));
      goto error;
   }

   RARCH_LOG("[ALSA]: Period: %u periods per buffer (%lu frames, %lu bytes)\n",
         periods,
         stream_info->period_frames,
         stream_info->period_size);

   if ((errnum = snd_pcm_hw_params_get_buffer_size(params, &buffer_size)) < 0)
   {
      RARCH_WARN("[ALSA]: Failed to get exact buffer size from %s device \"%s\": %s\n",
            snd_pcm_stream_name(stream),
            snd_pcm_name(*pcm),
            snd_strerror(errnum));
      RARCH_WARN("[ALSA]: Trying the maximum buffer size instead\n");

      if ((errnum = snd_pcm_hw_params_get_buffer_size_max(params, &buffer_size)) < 0)
      {
         RARCH_ERR("[ALSA]: Failed to get max buffer size from %s device \"%s\": %s\n",
               snd_pcm_stream_name(stream),
               snd_pcm_name(*pcm),
               snd_strerror(errnum));
         goto error;
      }
   }


   stream_info->buffer_size = snd_pcm_frames_to_bytes(*pcm, buffer_size);
   if (stream_info->buffer_size < 0)
   {
      RARCH_ERR("[ALSA]: Failed to convert a buffer size of %lu frames to bytes: %s\n",
            buffer_size,
            snd_strerror(buffer_size));
      goto error;
   }
   RARCH_LOG("[ALSA]: Buffer size: %lu frames (%lu bytes)\n", buffer_size, stream_info->buffer_size);

   stream_info->can_pause = snd_pcm_hw_params_can_pause(params);

   RARCH_LOG("[ALSA]: Can pause: %s.\n", stream_info->can_pause ? "yes" : "no");

   if ((errnum = snd_pcm_sw_params_malloc(&sw_params)) < 0)
   {
      RARCH_ERR("[ALSA]: Failed to allocate software parameters: %s\n",
            snd_strerror(errnum));

      goto error;
   }

   if ((errnum = snd_pcm_sw_params_current(*pcm, sw_params)) < 0)
   {
      RARCH_ERR("[ALSA]: Failed to query current software parameters for %s device \"%s\": %s\n",
            snd_pcm_stream_name(stream),
            snd_pcm_name(*pcm),
            snd_strerror(errnum));

      goto error;
   }

   if ((errnum = snd_pcm_sw_params_set_start_threshold(*pcm, sw_params, buffer_size / 2)) < 0)
   {
      RARCH_ERR("[ALSA]: Failed to set start %lu-frame threshold for %s device \"%s\": %s\n",
            buffer_size / 2,
            snd_pcm_stream_name(stream),
            snd_pcm_name(*pcm),
            snd_strerror(errnum));

      goto error;
   }

   if ((errnum = snd_pcm_sw_params(*pcm, sw_params)) < 0)
   {
      RARCH_ERR("[ALSA]: Failed to install software parameters for %s device \"%s\": %s\n",
            snd_pcm_stream_name(stream),
            snd_pcm_name(*pcm),
            snd_strerror(errnum));

      goto error;
   }

   snd_pcm_hw_params_free(params);
   snd_pcm_sw_params_free(sw_params);

   RARCH_LOG("[ALSA]: Initialized %s device \"%s\"\n",
         snd_pcm_stream_name(stream),
         snd_pcm_name(*pcm));

   return 0;
error:
   if (params)
      snd_pcm_hw_params_free(params);

   if (sw_params)
      snd_pcm_sw_params_free(sw_params);

   if (*pcm)
   {
      alsa_free_pcm(*pcm);
      *pcm = NULL;
   }

   return errnum;
}

void alsa_free_pcm(snd_pcm_t *pcm)
{
   if (pcm)
   {
      int errnum = 0;

      if ((errnum = snd_pcm_drop(pcm)) < 0)
      {
         RARCH_WARN("[ALSA]: Failed to drop remaining samples in %s stream \"%s\": %s\n",
               snd_pcm_stream_name(snd_pcm_stream(pcm)),
               snd_pcm_name(pcm),
               snd_strerror(errnum));
      }

      if ((errnum = snd_pcm_close(pcm)) < 0)
      {
         RARCH_WARN("[ALSA]: Failed to close %s stream \"%s\": %s\n",
               snd_pcm_stream_name(snd_pcm_stream(pcm)),
               snd_pcm_name(pcm),
               snd_strerror(errnum));
      }
   }
}

bool alsa_start_pcm(snd_pcm_t *pcm)
{
   int errnum = 0;
   snd_pcm_state_t pcm_state;

   if (!pcm)
      return false;

   pcm_state = snd_pcm_state(pcm);
   switch (pcm_state)
   {
      case SND_PCM_STATE_PAUSED: /* If we're unpausing a valid (but paused) stream... */
         if ((errnum = snd_pcm_pause(pcm, false)) < 0) /* ...but we failed... */
            goto error;

         break;
      case SND_PCM_STATE_PREPARED:
         /* If we're starting this stream for the first time... */
         if ((errnum = snd_pcm_start(pcm)) < 0) /* ..but we failed... */
            goto error;

         break;
      case SND_PCM_STATE_RUNNING:
         RARCH_DBG("[ALSA]: %s stream \"%s\" is already running, no action needed.\n",
            snd_pcm_stream_name(snd_pcm_stream(pcm)),
            snd_pcm_name(pcm));
         return true;
      default:
         RARCH_ERR("[ALSA]: Failed to start %s stream \"%s\" in unexpected state %s\n",
                   snd_pcm_stream_name(snd_pcm_stream(pcm)),
                   snd_pcm_name(pcm),
                   snd_pcm_state_name(pcm_state));
         return false;
   }

   RARCH_DBG("[ALSA]: Started %s stream \"%s\", transitioning from %s to %s\n",
             snd_pcm_stream_name(snd_pcm_stream(pcm)),
             snd_pcm_name(pcm),
             snd_pcm_state_name(pcm_state),
             snd_pcm_state_name(snd_pcm_state(pcm)));

   return true;

error:
   RARCH_ERR("[ALSA]: Failed to start %s stream \"%s\" in state %s: %s\n",
             snd_pcm_stream_name(snd_pcm_stream(pcm)),
             snd_pcm_name(pcm),
             snd_pcm_state_name(pcm_state),
             snd_strerror(errnum));

   return false;
}

bool alsa_stop_pcm(snd_pcm_t *pcm)
{
   int errnum = 0;
   snd_pcm_state_t pcm_state;

   if (!pcm)
      return false;

   pcm_state = snd_pcm_state(pcm);
   switch (pcm_state)
   {
      case SND_PCM_STATE_PAUSED:
         RARCH_DBG("[ALSA]: %s stream \"%s\" is already paused, no action needed.\n",
            snd_pcm_stream_name(snd_pcm_stream(pcm)),
            snd_pcm_name(pcm));
         return true;
      case SND_PCM_STATE_PREPARED:
         RARCH_DBG("[ALSA]: %s stream \"%s\" is prepared but not running, no action needed.\n",
            snd_pcm_stream_name(snd_pcm_stream(pcm)),
            snd_pcm_name(pcm));
         return true;
      case SND_PCM_STATE_RUNNING:
         /* If we're pausing an active stream... */
         if ((errnum = snd_pcm_pause(pcm, true)) < 0) /* ...but we failed... */
            goto error;

         break;
      default:
         RARCH_ERR("[ALSA]: Failed to stop %s stream \"%s\" in unexpected state %s\n",
                   snd_pcm_stream_name(snd_pcm_stream(pcm)),
                   snd_pcm_name(pcm),
                   snd_pcm_state_name(pcm_state));

         return false;
   }

   RARCH_DBG("[ALSA]: Stopped %s stream \"%s\", transitioning from %s to %s\n",
             snd_pcm_stream_name(snd_pcm_stream(pcm)),
             snd_pcm_name(pcm),
             snd_pcm_state_name(pcm_state),
             snd_pcm_state_name(snd_pcm_state(pcm)));

   return true;

error:
   RARCH_ERR("[ALSA]: Failed to stop %s stream \"%s\" in state %s: %s\n",
             snd_pcm_stream_name(snd_pcm_stream(pcm)),
             snd_pcm_name(pcm),
             snd_pcm_state_name(pcm_state),
             snd_strerror(errnum));

   return false;
}

struct string_list *alsa_device_list_type_new(const char* type)
{
   void **hints, **n;
   union string_list_elem_attr attr;
   struct string_list *s = string_list_new();

   if (!s)
      return NULL;

   attr.i = 0;

   if (snd_device_name_hint(-1, "pcm", &hints) != 0)
   {
      string_list_free(s);
      return NULL;
   }

   n      = hints;

   while (*n)
   {
      char *name = snd_device_name_get_hint(*n, "NAME");
      char *io   = snd_device_name_get_hint(*n, "IOID");
      char *desc = snd_device_name_get_hint(*n, "DESC");

      /* description of device IOID - input / output identification
       * ("Input" or "Output"), NULL means both) */

      if (!io || (string_is_equal(io, type)))
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
}

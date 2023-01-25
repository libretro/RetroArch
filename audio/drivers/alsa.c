/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#include <asm-generic/errno.h>

#include "../audio_driver.h"
#include "alsa.h"
#include "../../verbosity.h"

typedef struct alsa
{
   snd_pcm_t *pcm;

   /* The error handler that was set before this driver was initialized.
    * Likely to be equal to the default, but kept and restored just in case. */
   snd_lib_error_handler_t prev_error_handler;
   alsa_stream_info_t stream_info;
   bool nonblock;
   bool is_paused;
} alsa_t;

static bool alsa_use_float(void *data)
{
   alsa_t *alsa = (alsa_t*)data;
   return alsa->stream_info.has_float;
}

static void alsa_log_error(const char *file, int line, const char *function, int err, const char *fmt,...)
{
   va_list args;
   char temp[256];
   char errno_temp[256];

   memset(temp, 0, sizeof(temp));
   memset(errno_temp, 0, sizeof(temp));

   va_start(args, fmt);
   vsnprintf(temp, sizeof(temp), fmt, args);
   if (err)
      snprintf(errno_temp, sizeof(errno_temp), " (%s)", snd_strerror(err));

   /* Write up to 255 characters. (The 256th will be \0.) */
   va_end(args);
   RARCH_ERR("[ALSA]: [%s:%s:%d]: %s%s\n", file, function, line, temp, errno_temp); /* To ensure that there's a newline at the end */
}

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

   if (new_rate && (rate != orig_rate))
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
         RARCH_WARN("[ALSA]: Failed to drop remaining samples in %s device \"%s\": %s\n",
               snd_pcm_stream_name(snd_pcm_stream(pcm)),
               snd_pcm_name(pcm),
               snd_strerror(errnum));
      }

      if ((errnum = snd_pcm_close(pcm)) < 0)
      {
         RARCH_WARN("[ALSA]: Failed to close %s device \"%s\": %s\n",
               snd_pcm_stream_name(snd_pcm_stream(pcm)),
               snd_pcm_name(pcm),
               snd_strerror(errnum));
      }
   }
}

static void alsa_free(void *data);
static void *alsa_init(const char *device, unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   alsa_t *alsa = (alsa_t*)calloc(1, sizeof(alsa_t));

   if (!alsa)
   {
      RARCH_ERR("[ALSA]: Failed to allocate driver context\n");
      return NULL;
   }

   //alsa->prev_error_handler = snd_lib_error;
   //snd_lib_error_set_handler(alsa_log_error);

   RARCH_LOG("[ALSA]: Using ALSA version %s\n", snd_asoundlib_version());

   if (alsa_init_pcm(&alsa->pcm, device, SND_PCM_STREAM_PLAYBACK, rate, latency, 2, &alsa->stream_info, new_rate, SND_PCM_NONBLOCK) < 0)
   {
      goto error;
   }

   return alsa;

error:
   RARCH_ERR("[ALSA]: Failed to initialize...\n");

   alsa_free(alsa);

   return NULL;
}

#define BYTES_TO_FRAMES(bytes, frame_bits)  ((bytes) * 8 / frame_bits)
#define FRAMES_TO_BYTES(frames, frame_bits) ((frames) * frame_bits / 8)

static bool alsa_start(void *data, bool is_shutdown);
static ssize_t alsa_write(void *data, const void *buf_, size_t size_)
{
   alsa_t *alsa              = (alsa_t*)data;
   const uint8_t *buf        = (const uint8_t*)buf_;
   snd_pcm_sframes_t written = 0;
   snd_pcm_sframes_t size    = BYTES_TO_FRAMES(size_, alsa->stream_info.frame_bits);
   size_t frames_size        = alsa->stream_info.has_float ? sizeof(float) : sizeof(int16_t);

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

static bool alsa_start(void *data, bool is_shutdown)
{
   alsa_t *alsa = (alsa_t*)data;
   if (!alsa->is_paused)
      return true;

   if (alsa->stream_info.can_pause
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
      alsa_free_pcm(alsa->pcm);

      if (alsa->prev_error_handler)
      { /* If we ever changed the error handler, put it back. */
         snd_lib_error_set_handler(alsa->prev_error_handler);
      }

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

struct string_list *alsa_device_list_type_new(const char* type)
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

   while (*n)
   {
      char *name = snd_device_name_get_hint(*n, "NAME");
      char *io   = snd_device_name_get_hint(*n, "IOID");
      char *desc = snd_device_name_get_hint(*n, "DESC");

      /* description of device IOID - input / output identifcation
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

   error:
   string_list_free(s);
   return NULL;
}

void *alsa_device_list_new(void *data)
{
   return alsa_device_list_type_new("Output");
}

void alsa_device_list_free(void *data, void *array_list_data)
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

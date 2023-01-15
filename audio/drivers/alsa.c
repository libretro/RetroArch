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

#include "../audio_driver.h"
#include "../../verbosity.h"

typedef struct alsa_microphone
{
    snd_pcm_t *pcm;
    size_t buffer_size;
    unsigned int frame_bits;
    bool has_float;
    bool can_pause;
    bool is_paused;
} alsa_microphone_t;

typedef struct alsa
{
   snd_pcm_t *pcm;
    /* Only one microphone is supported right now;
     * the driver state should track multiple microphone handles,
     * but the driver *context* should track multiple microphone contexts */
   alsa_microphone_t *microphone;

   /* The error handler that was set before this driver was initialized.
    * Likely to be equal to the default, but kept and restored just in case. */
   snd_lib_error_handler_t prev_error_handler;
   size_t buffer_size;
   unsigned int frame_bits;
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

static void alsa_log_error(const char *file, int line, const char *function, int err, const char *fmt,...)
{
   va_list args;
   char temp[256];

   memset(temp, 0, sizeof(temp));

   if (err)
   {
      RARCH_ERR("[ALSA] [System] Error in %s:%s:%d: %s\n", file, function, line, snd_strerror(err));
   }

   va_start(args, fmt);
   vsnprintf(temp, sizeof(temp), fmt, args);
   /* Write up to 255 characters. (The 256th will be \0.) */
   va_end(args);
   RARCH_ERR("[ALSA] [%s:%s:%d]: %s\n", file, function, line, temp); /* To ensure that there's a newline at the end */
}

static bool find_float_format(snd_pcm_t *pcm, void *data)
{
   snd_pcm_hw_params_t *params = (snd_pcm_hw_params_t*)data;

   if (snd_pcm_hw_params_test_format(pcm, params, SND_PCM_FORMAT_FLOAT) == 0)
   {
      RARCH_LOG("[ALSA]: Using floating point format.\n");
      return true;
   }

   RARCH_LOG("[ALSA]: Using signed 16-bit format.\n");
   return false;
}

static void *alsa_init(const char *device, unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   snd_pcm_format_t format;
   snd_pcm_uframes_t buffer_size;
   snd_pcm_hw_params_t *params    = NULL;
   snd_pcm_sw_params_t *sw_params = NULL;
   unsigned latency_usec          = latency * 1000;
   unsigned channels              = 2;
   unsigned periods               = 4;
   unsigned orig_rate             = rate;
   const char *alsa_dev           = "default";
   alsa_t *alsa                   = (alsa_t*)calloc(1, sizeof(alsa_t));

   if (!alsa)
      return NULL;

   alsa->prev_error_handler = snd_lib_error;
   snd_lib_error_set_handler(alsa_log_error);

   if (device)
      alsa_dev = device;

   RARCH_DBG("[ALSA] Requesting device \"%s\" for output\n", alsa_dev);

   if (snd_pcm_open(
            &alsa->pcm, alsa_dev, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK) < 0)
      goto error;

   if (snd_pcm_hw_params_malloc(&params) < 0)
      goto error;

   if (snd_pcm_hw_params_any(alsa->pcm, params) < 0)
      goto error;

   alsa->has_float = find_float_format(alsa->pcm, params);
   format = alsa->has_float ? SND_PCM_FORMAT_FLOAT : SND_PCM_FORMAT_S16;

   if (snd_pcm_hw_params_set_access(
            alsa->pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
      goto error;

   /* channels hardcoded to 2 for now */
   alsa->frame_bits = snd_pcm_format_physical_width(format) * 2;

   if (snd_pcm_hw_params_set_format(alsa->pcm, params, format) < 0)
      goto error;

   if (snd_pcm_hw_params_set_channels(alsa->pcm, params, channels) < 0)
      goto error;

   /* Don't allow rate resampling when probing for the default rate (but ignore if this call fails) */
   snd_pcm_hw_params_set_rate_resample(alsa->pcm, params, 0 );
   if (snd_pcm_hw_params_set_rate_near(alsa->pcm, params, &rate, 0) < 0)
      goto error;

   if (new_rate && (rate != orig_rate))
      *new_rate = rate;

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

   RARCH_LOG("[ALSA]: Period size: %d frames\n", (int)buffer_size);

   if (snd_pcm_hw_params_get_buffer_size(params, &buffer_size))
      snd_pcm_hw_params_get_buffer_size_max(params, &buffer_size);

   RARCH_LOG("[ALSA]: Buffer size: %d frames\n", (int)buffer_size);

   alsa->buffer_size = snd_pcm_frames_to_bytes(alsa->pcm, buffer_size);
   alsa->can_pause = snd_pcm_hw_params_can_pause(params);

   RARCH_LOG("[ALSA]: Can pause: %s.\n", alsa->can_pause ? "yes" : "no");

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
   RARCH_ERR("[ALSA]: Failed to initialize...\n");
   if (params)
      snd_pcm_hw_params_free(params);

   if (sw_params)
      snd_pcm_sw_params_free(sw_params);

   if (alsa)
   {
      if (alsa->pcm)
      {
         snd_pcm_close(alsa->pcm);
         snd_config_update_free_global();
      }

      if (alsa->prev_error_handler)
      { /* If we ever changed the error handler, put it back. */
         snd_lib_error_set_handler(alsa->prev_error_handler);
      }

      free(alsa);
   }
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
   snd_pcm_sframes_t size    = BYTES_TO_FRAMES(size_, alsa->frame_bits);
   size_t frames_size        = alsa->has_float ? sizeof(float) : sizeof(int16_t);

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

   if (alsa->can_pause
         && !alsa->is_paused)
   {
      int ret = snd_pcm_pause(alsa->pcm, 1);

      if (ret < 0)
         return false;

      alsa->is_paused = true;
   }

   if (alsa->microphone)
   {
      /* Stop the microphone independently of whether it's paused;
       * note that upon alsa_start, the microphone won't resume
       * if it was previously paused */
      int errnum = snd_pcm_pause(alsa->microphone->pcm, true);
      if (errnum < 0)
      {
         RARCH_ERR("[ALSA]: Failed to pause microphone %x: %s\n",
            alsa->microphone->pcm,
            snd_strerror(errnum));
         return false;
      }
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

      if (alsa->microphone && !alsa->microphone->is_paused)
      { /* If the mic wasn't paused at the time the overall driver was paused... */
         int errnum = snd_pcm_pause(alsa->microphone->pcm, false);
         if (errnum < 0)
         {
            RARCH_ERR("[ALSA]: Failed to unpause microphone %x: %s\n",
               alsa->microphone->pcm,
               snd_strerror(errnum));
            return false;
         }
      }
   }
   return true;
}

static void alsa_free_microphone(void *data, void *microphone_context);
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

      if (alsa->microphone)
      {
         alsa_free_microphone(alsa, alsa->microphone);
      }


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
      return alsa->buffer_size;

   return FRAMES_TO_BYTES(avail, alsa->frame_bits);
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

   while (*n)
   {
      char *name = snd_device_name_get_hint(*n, "NAME");
      char *io   = snd_device_name_get_hint(*n, "IOID");
      char *desc = snd_device_name_get_hint(*n, "DESC");

      /* description of device IOID - input / output identifcation
       * ("Input" or "Output"), NULL means both) */

      if (!io || (string_is_equal(io, "Output")))
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

static void *alsa_init_microphone(void *data,
   const char *device,
   unsigned rate,
   unsigned latency,
   unsigned block_frames,
   unsigned *new_rate)
{
   snd_pcm_format_t format;
   snd_pcm_uframes_t buffer_size;
   alsa_t *alsa                   = (alsa_t*)data;
   snd_pcm_hw_params_t *params    = NULL;
   snd_pcm_sw_params_t *sw_params = NULL;
   unsigned latency_usec          = latency * 1000;
   unsigned periods               = 4;
   unsigned orig_rate             = rate;
   const char *alsa_mic_dev       = "default";
   alsa_microphone_t  *microphone = NULL;
   int errnum                     = 0;

   if (!alsa) /* If we weren't given a valid ALSA context... */
      return NULL;

   microphone = calloc(1, sizeof(alsa_microphone_t));

   if (!microphone) /* If the microphone context couldn't be allocated... */
      return NULL;

   if (device)
      alsa_mic_dev = device;

   RARCH_DBG("[ALSA] Requesting device \"%s\" for input\n", alsa_mic_dev);

   errnum = snd_pcm_open(&microphone->pcm, alsa_mic_dev, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
   if (errnum < 0)
   {
      RARCH_ERR("[ALSA]: Failed to open microphone device: %s\n", snd_strerror(errnum));
      goto error;
   }

   if (snd_pcm_hw_params_malloc(&params) < 0)
      goto error;

   if (snd_pcm_hw_params_any(microphone->pcm, params) < 0)
      goto error;

   microphone->has_float = find_float_format(microphone->pcm, params);
   format = microphone->has_float ? SND_PCM_FORMAT_FLOAT : SND_PCM_FORMAT_S16;

   if (microphone->has_float != alsa->has_float)
   { /* If the mic and speaker don't both use floating point or integer samples... */
      RARCH_WARN("[ALSA]: Microphone and speaker do not use the same PCM format\n");
      RARCH_WARN("[ALSA]: (%s and %s, respectively)\n",
                microphone->has_float ? "SND_PCM_FORMAT_FLOAT" : "SND_PCM_FORMAT_S16",
                alsa->has_float ? "SND_PCM_FORMAT_FLOAT" : "SND_PCM_FORMAT_S16");
    }

   if (snd_pcm_hw_params_set_access(
          microphone->pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
      goto error;

   /* channels hardcoded to 1, because we only support mono mic input */
   microphone->frame_bits = snd_pcm_format_physical_width(format);

   if (snd_pcm_hw_params_set_format(microphone->pcm, params, format) < 0)
      goto error;

   if (snd_pcm_hw_params_set_channels(microphone->pcm, params, 1) < 0)
      goto error;

   /* Don't allow rate resampling when probing for the default rate (but ignore if this call fails) */
   snd_pcm_hw_params_set_rate_resample(microphone->pcm, params, 0 );
   if (snd_pcm_hw_params_set_rate_near(microphone->pcm, params, &rate, 0) < 0)
      goto error;

   if (new_rate && (rate != orig_rate))
      *new_rate = rate;

   if (snd_pcm_hw_params_set_buffer_time_near(
          microphone->pcm, params, &latency_usec, NULL) < 0)
      goto error;

   if (snd_pcm_hw_params_set_periods_near(
         microphone->pcm, params, &periods, NULL) < 0)
      goto error;

   if (snd_pcm_hw_params(microphone->pcm, params) < 0)
      goto error;

   /* Shouldn't have to bother with this,
    * but some drivers are apparently broken. */
   if (snd_pcm_hw_params_get_period_size(params, &buffer_size, NULL))
      snd_pcm_hw_params_get_period_size_min(params, &buffer_size, NULL);

   RARCH_LOG("[ALSA]: Microphone period size: %d frames\n", (int)buffer_size);

   if (snd_pcm_hw_params_get_buffer_size(params, &buffer_size))
      snd_pcm_hw_params_get_buffer_size_max(params, &buffer_size);

   RARCH_LOG("[ALSA]: Microphone buffer size: %d frames\n", (int)buffer_size);

   microphone->buffer_size = snd_pcm_frames_to_bytes(microphone->pcm, buffer_size);
   microphone->can_pause = snd_pcm_hw_params_can_pause(params);

   RARCH_LOG("[ALSA]: Can pause microphone: %s.\n", microphone->can_pause ? "yes" : "no");

   if (snd_pcm_sw_params_malloc(&sw_params) < 0)
      goto error;

   if (snd_pcm_sw_params_current(microphone->pcm, sw_params) < 0)
      goto error;

   snd_pcm_hw_params_free(params);
   snd_pcm_sw_params_free(sw_params);

   alsa->microphone = microphone;
   return microphone;

error:
   RARCH_ERR("[ALSA]: Failed to initialize microphone...\n");
   if (params)
      snd_pcm_hw_params_free(params);

   if (sw_params)
      snd_pcm_sw_params_free(sw_params);

   if (microphone)
   {
      if (microphone->pcm)
      {
         snd_pcm_close(microphone->pcm);
      }

      free(microphone);
   }
   return NULL;
}

static void alsa_free_microphone(void *data, void *microphone_context)
{
   alsa_t *alsa                   = (alsa_t*)data;
   alsa_microphone_t *microphone  = (alsa_microphone_t*)microphone_context;

   if (alsa && microphone)
   {
      if (microphone->pcm)
      {
         snd_pcm_drop(microphone->pcm);
         snd_pcm_close(microphone->pcm);
      }

      alsa->microphone = NULL;
      free(microphone);
   }
}

static bool alsa_get_microphone_state(const void *data, const void *microphone_context)
{
   alsa_t *alsa                   = (alsa_t*)data;
   alsa_microphone_t  *microphone = (alsa_microphone_t*)microphone_context;

   if (!alsa || !microphone)
      return false;
   /* Both params must be non-null */

   return !microphone->is_paused;
   /* The mic might be paused due to app requirements,
    * or it might be stopped because the entire audio driver is stopped. */
}

static bool alsa_set_microphone_state(void *data, void *microphone_context, bool enabled)
{
   alsa_t *alsa                   = (alsa_t*)data;
   alsa_microphone_t  *microphone = (alsa_microphone_t*)microphone_context;

   if (!alsa || !microphone)
      return false;
   /* Both params must be non-null */

   if (!microphone->can_pause)
   {
      RARCH_WARN("[ALSA]: Microphone %x cannot be paused\n", microphone->pcm);
      return true;
   }

   if (!alsa->is_paused)
   { /* If the entire audio driver isn't paused... */
      int errnum = snd_pcm_pause(microphone->pcm, !enabled);
      if (errnum < 0)
      {
         RARCH_ERR("[ALSA]: Failed to %s microphone %x: %s\n",
                   enabled ? "unpause" : "pause",
                   microphone->pcm,
                   snd_strerror(errnum));
         return false;
      }

      microphone->is_paused = !enabled;
      RARCH_DBG("[ALSA]: Set state of microphone %x to %s\n",
              microphone->pcm, enabled ? "enabled" : "disabled");
   }

   return true;
}

static ssize_t alsa_read_microphone(void *driver_context, void *microphone_context, void *buf_, size_t size_)
{
   alsa_t *alsa                  = (alsa_t*)driver_context;
   alsa_microphone_t *microphone = (alsa_microphone_t*)microphone_context;
   uint8_t *buf                  = (uint8_t*)buf_;
   snd_pcm_sframes_t read        = 0;
   int errnum                    = 0;
   snd_pcm_sframes_t size;
   size_t frames_size;

   if (!alsa || !microphone || !buf)
      return -1;

   size        = BYTES_TO_FRAMES(size_, microphone->frame_bits);
   frames_size = microphone->has_float ? sizeof(float) : sizeof(int16_t);

   /* Workaround buggy menu code.
    * If a read happens while we're paused, we might never progress. */
   if (microphone->is_paused)
      if (!alsa_set_microphone_state(alsa, microphone, true))
         return -1;

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
               RARCH_ERR("[ALSA] Failed to read from microphone: %s\n", snd_strerror(frames));
               RARCH_ERR("[ALSA] Additionally, recovery failed with: %s\n", snd_strerror(errnum));
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

   return read * frames_size;
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
   alsa_init_microphone,
   alsa_free_microphone,
   alsa_get_microphone_state,
   alsa_set_microphone_state,
   alsa_read_microphone,
};

/*  SSNES - A Super Ninteno Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#include "driver.h"
#include <stdlib.h>
#include <alsa/asoundlib.h>

#define TRY_ALSA(x) if ( x < 0 ) { \
                  goto error; \
               }

typedef struct alsa
{
   snd_pcm_t *pcm;
   bool nonblock;
} alsa_t;

static void* __alsa_init(const char* device, int rate, int latency)
{
   alsa_t *alsa = calloc(1, sizeof(alsa_t));
   if ( alsa == NULL )
      return NULL;

   snd_pcm_hw_params_t *params = NULL;
   snd_pcm_sw_params_t *sw_params = NULL;

   const char *alsa_dev = "default";
   if ( device != NULL )
      alsa_dev = device;

   //fprintf(stderr, "Opening device: %s\n", alsa_dev);

   TRY_ALSA(snd_pcm_open(&alsa->pcm, alsa_dev, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK));

   unsigned int latency_usec = latency * 1000;

   snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
   unsigned int channels = 2;

   TRY_ALSA(snd_pcm_hw_params_malloc(&params));

   TRY_ALSA(snd_pcm_hw_params_any(alsa->pcm, params));
   TRY_ALSA(snd_pcm_hw_params_set_access(alsa->pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED));
   TRY_ALSA(snd_pcm_hw_params_set_format(alsa->pcm, params, format));
   TRY_ALSA(snd_pcm_hw_params_set_channels(alsa->pcm, params, channels));
   TRY_ALSA(snd_pcm_hw_params_set_rate(alsa->pcm, params, rate, 0));

   // We test if we can run the latencies we are allowed, if not, fallback to *_near.

   if ( snd_pcm_hw_params_set_buffer_time_max(alsa->pcm, params, &latency_usec, NULL) < 0)
   {
      latency_usec = latency * 1000;
      TRY_ALSA(snd_pcm_hw_params_set_buffer_time_near(alsa->pcm, params, &latency_usec, NULL))
   }

   latency_usec = (latency < 32) ? 10000 : latency * 250;
   if ( snd_pcm_hw_params_set_period_time_max(alsa->pcm, params, &latency_usec, NULL) )
   {
      latency_usec = (latency < 32) ? 10000 : latency * 250;
      TRY_ALSA(snd_pcm_hw_params_set_period_time_near(alsa->pcm, params, &latency_usec, NULL));
   }

   TRY_ALSA(snd_pcm_hw_params(alsa->pcm, params));

   snd_pcm_uframes_t buffer_size;
   //snd_pcm_hw_params_get_period_size(params, &buffer_size, NULL);
   //fprintf(stderr, "ALSA Period size: %d frames\n", (int)alsa_sizes);
   snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
   //fprintf(stderr, "Buffer size: %d frames\n", (int)alsa_sizes);

   if (snd_pcm_sw_params_malloc(&sw_params) < 0)
      goto error;

   TRY_ALSA(snd_pcm_sw_params_current(alsa->pcm, sw_params));
   TRY_ALSA(snd_pcm_sw_params_set_start_threshold(alsa->pcm, sw_params, buffer_size/2));
   TRY_ALSA(snd_pcm_sw_params(alsa->pcm, sw_params));

   snd_pcm_sw_params_free(sw_params);
   snd_pcm_hw_params_free(params);

   return alsa;

error:
   if ( params != NULL )
      snd_pcm_hw_params_free(params);

   if ( sw_params != NULL )
      snd_pcm_sw_params_free(sw_params);

   if ( alsa != NULL )
   {
      if ( alsa->pcm != NULL )
         snd_pcm_close(alsa->pcm);

      free(alsa);
   }
   return NULL;
}

static ssize_t __alsa_write(void* data, const void* buf, size_t size)
{
   alsa_t *alsa = data;

   snd_pcm_sframes_t frames;
   snd_pcm_sframes_t written = 0;
   int rc;
   size /= 4; // Frames to write

   while (written < size)
   {
      if (!alsa->nonblock)
      {
         rc = snd_pcm_wait(alsa->pcm, -1);
         if (rc == -EPIPE || rc == -ESTRPIPE)
         {
            if (snd_pcm_recover(alsa->pcm, rc, 1) < 0)
               return -1;
            continue;
         }
      }

      frames = snd_pcm_writei(alsa->pcm, (const uint32_t*)buf + written, size - written);

      if ( frames == -EPIPE || frames == -EINTR || frames == -ESTRPIPE )
      {
         if ( snd_pcm_recover(alsa->pcm, frames, 1) < 0 )
            return -1;

         return 0;
      }
      else if ( frames == -EAGAIN && alsa->nonblock )
         return 0;
      else if ( frames < 0 )
         return -1;

      written += frames;
   }

   return snd_pcm_frames_to_bytes(alsa->pcm, size);
}

static bool __alsa_stop(void *data)
{
   return true;
}

static void __alsa_set_nonblock_state(void *data, bool state)
{
   alsa_t *alsa = data;
   alsa->nonblock = state;
}

static bool __alsa_start(void *data)
{
   return true;
}

static void __alsa_free(void *data)
{
   alsa_t *alsa = data;
   if ( alsa )
   {
      if ( alsa->pcm )
      {
         snd_pcm_drop(alsa->pcm);
         snd_pcm_close(alsa->pcm);
      }
      free(alsa);
   }
}

const audio_driver_t audio_alsa = {
   .init = __alsa_init,
   .write = __alsa_write,
   .stop = __alsa_stop,
   .start = __alsa_start,
   .set_nonblock_state = __alsa_set_nonblock_state,
   .free = __alsa_free,
   .ident = "alsa"
};

   


   
   

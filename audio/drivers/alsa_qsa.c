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

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <sys/asoundlib.h>
#include <retro_math.h>

#include "../../retroarch.h"

#define MAX_FRAG_SIZE 3072
#define DEFAULT_RATE 48000

#define CHANNELS 2

typedef struct alsa
{
   uint8_t **buffer;
   uint8_t *buffer_chunk;
   unsigned buffer_index;
   unsigned buffer_ptr;
   volatile unsigned buffered_blocks;

   snd_pcm_t *pcm;
   bool nonblock;
   bool has_float;
   bool can_pause;
   bool is_paused;
   unsigned buf_size;
   unsigned buf_count;
} alsa_qsa_t;

typedef long snd_pcm_sframes_t;

static void *alsa_qsa_init(const char *device,
      unsigned rate, unsigned latency, unsigned block_frames,
      unsigned *new_rate)
{
   int err, card, dev, i;
   snd_pcm_channel_info_t pi;
   snd_pcm_channel_params_t params = {0};
   snd_pcm_channel_setup_t setup   = {0};
   alsa_qsa_t *alsa                = (alsa_qsa_t*)calloc(1, sizeof(alsa_qsa_t));
   if (!alsa)
      return NULL;

   (void)device;
   (void)rate;
   (void)latency;

   if ((err = snd_pcm_open_preferred(&alsa->pcm, &card, &dev,
               SND_PCM_OPEN_PLAYBACK)) < 0)
   {
      RARCH_ERR("[ALSA QSA]: Audio open error: %s\n",
            snd_strerror(err));
      goto error;
   }

   if((err = snd_pcm_nonblock_mode(alsa->pcm, 1)) < 0)
   {
      RARCH_ERR("[ALSA QSA]: Can't set blocking mode: %s\n",
            snd_strerror(err));
      goto error;
   }

   memset(&pi, 0, sizeof(pi));
   pi.channel = SND_PCM_CHANNEL_PLAYBACK;
   if ((err = snd_pcm_channel_info(alsa->pcm, &pi)) < 0)
   {
      RARCH_ERR("[ALSA QSA]: snd_pcm_channel_info failed: %s\n",
            snd_strerror(err));
      goto error;
   }

   memset(&params, 0, sizeof(params));

   params.channel = SND_PCM_CHANNEL_PLAYBACK;
   params.mode = SND_PCM_MODE_BLOCK;

   params.format.interleave = 1;
   params.format.format = SND_PCM_SFMT_S16_LE;
   params.format.rate = DEFAULT_RATE;
   params.format.voices = 2;

   params.start_mode = SND_PCM_START_FULL;
   params.stop_mode = SND_PCM_STOP_STOP;

   params.buf.block.frag_size = pi.max_fragment_size;
   params.buf.block.frags_min = 2;
   params.buf.block.frags_max = 8;

   RARCH_LOG("Fragment size: %d\n", params.buf.block.frag_size);
   RARCH_LOG("Min Fragment size: %d\n", params.buf.block.frags_min);
   RARCH_LOG("Max Fragment size: %d\n", params.buf.block.frags_max);

   if ((err = snd_pcm_channel_params(alsa->pcm, &params)) < 0)
   {
      RARCH_ERR("[ALSA QSA]: Channel Parameter Error: %s\n",
            snd_strerror(err));
      goto error;
   }

   setup.channel = SND_PCM_CHANNEL_PLAYBACK;

   if ((err = snd_pcm_channel_setup(alsa->pcm, &setup)) < 0)
   {
      RARCH_ERR("[ALSA QSA]: Channel Parameter Read Back Error: %s\n",
            snd_strerror(err));
      goto error;
   }

   if (block_frames)
      alsa->buf_size = block_frames * 4;
   else
      alsa->buf_size = next_pow2(32 * latency);

   RARCH_LOG("[ALSA QSA]: buffer size: %u bytes\n", alsa->buf_size);

   alsa->buf_count = (latency * 4 * rate + 500) / 1000;
   alsa->buf_count = (alsa->buf_count + alsa->buf_size / 2) / alsa->buf_size;

   if ((err = snd_pcm_channel_prepare(alsa->pcm,
               SND_PCM_CHANNEL_PLAYBACK)) < 0)
   {
      RARCH_ERR("[ALSA QSA]: Channel Prepare Error: %s\n",
            snd_strerror(err));
      goto error;
   }

   alsa->buffer = (uint8_t**)calloc(sizeof(uint8_t*), alsa->buf_count);
   if (!alsa->buffer)
      goto error;

   alsa->buffer_chunk = (uint8_t*)calloc(alsa->buf_count, alsa->buf_size);
   if (!alsa->buffer_chunk)
      goto error;

   for (i = 0; i < alsa->buf_count; i++)
      alsa->buffer[i] = alsa->buffer_chunk + i * alsa->buf_size;

   alsa->has_float = false;
   alsa->can_pause = true;
   RARCH_LOG("[ALSA QSA]: Can pause: %s.\n",
         alsa->can_pause ? "yes" : "no");

   return alsa;

error:
   return (void*)-1;
}

static int check_pcm_status(void *data, int channel_type)
{
   snd_pcm_channel_status_t status;
   alsa_qsa_t *alsa = (alsa_qsa_t*)data;
   int ret          = EOK;

   memset(&status, 0, sizeof (status));
   status.channel = channel_type;

   if ((ret = snd_pcm_channel_status(alsa->pcm, &status)) == 0)
   {
      if (status.status == SND_PCM_STATUS_UNSECURE)
      {
         RARCH_ERR("check_pcm_status got SND_PCM_STATUS_UNSECURE, aborting playback\n");
         ret = -EPROTO;
      }
      else if (status.status == SND_PCM_STATUS_UNDERRUN)
      {
         RARCH_LOG("check_pcm_status: SNDP_CM_STATUS_UNDERRUN.\n");
         if ((ret = snd_pcm_channel_prepare(alsa->pcm, channel_type)) < 0)
         {
            RARCH_ERR("Invalid state detected for underrun on snd_pcm_channel_prepare: %s\n",
                  snd_strerror(ret));
            ret = -EPROTO;
         }
      }
      else if (status.status == SND_PCM_STATUS_OVERRUN)
      {
         RARCH_LOG("check_pcm_status: SNDP_CM_STATUS_OVERRUN.\n");
         if ((ret = snd_pcm_channel_prepare(alsa->pcm, channel_type)) < 0)
         {
            RARCH_ERR("Invalid state detected for overrun on snd_pcm_channel_prepare: %s\n",
                  snd_strerror(ret));
            ret = -EPROTO;
         }
      }
      else if (status.status == SND_PCM_STATUS_CHANGE)
      {
         RARCH_LOG("check_pcm_status: SNDP_CM_STATUS_CHANGE.\n");
         if ((ret = snd_pcm_channel_prepare(alsa->pcm, channel_type)) < 0)
         {
            RARCH_ERR("Invalid state detected for change on snd_pcm_channel_prepare: %s\n",
                  snd_strerror(ret));
            ret = -EPROTO;
         }
      }
   }
   else
   {
      RARCH_ERR("check_pcm_status failed: %s\n", snd_strerror(ret));
      if (ret == -ESRCH)
         ret = -EBADF;
   }

   return ret;
}

static ssize_t alsa_qsa_write(void *data, const void *buf, size_t size)
{
   alsa_qsa_t          *alsa = (alsa_qsa_t*)data;
   snd_pcm_sframes_t written = 0;

   while (size)
   {
      size_t avail_write = MIN(alsa->buf_size - alsa->buffer_ptr, size);

      if (avail_write)
      {
         memcpy(alsa->buffer[alsa->buffer_index] +
               alsa->buffer_ptr, buf, avail_write);

         alsa->buffer_ptr      += avail_write;
         buf                    = (void*)((uint8_t*)buf + avail_write);
         size                  -= avail_write;
         written               += avail_write;
      }

      if (alsa->buffer_ptr >= alsa->buf_size)
      {
         snd_pcm_sframes_t frames = snd_pcm_write(alsa->pcm,
               alsa->buffer[alsa->buffer_index], alsa->buf_size);

         alsa->buffer_index = (alsa->buffer_index + 1) % alsa->buf_count;
         alsa->buffer_ptr   = 0;

         if (frames <= 0)
         {
            int ret;

            if (frames == -EAGAIN)
               continue;

            ret = check_pcm_status(alsa, SND_PCM_CHANNEL_PLAYBACK);

            if (ret == -EPROTO || ret == -EBADF)
               return -1;
         }
      }

   }

   return written;
}

static bool alsa_qsa_stop(void *data)
{
   alsa_qsa_t *alsa = (alsa_qsa_t*)data;

   if (alsa->can_pause && !alsa->is_paused)
   {
      int ret = snd_pcm_playback_pause(alsa->pcm);
      if (ret < 0)
         return false;

      alsa->is_paused = true;
   }

   return true;
}

static bool alsa_qsa_alive(void *data)
{
   alsa_qsa_t *alsa = (alsa_qsa_t*)data;
   if (alsa)
      return !alsa->is_paused;
   return false;
}

static bool alsa_qsa_start(void *data, bool is_shutdown)
{
   alsa_qsa_t *alsa = (alsa_qsa_t*)data;

   if (alsa->can_pause && alsa->is_paused)
   {
      int ret = snd_pcm_playback_resume(alsa->pcm);

      if (ret < 0)
      {
         RARCH_ERR("[ALSA QSA]: Failed to unpause: %s.\n",
               snd_strerror(ret));
         return false;
      }

      alsa->is_paused = false;
   }

   return true;
}

static void alsa_qsa_set_nonblock_state(void *data, bool state)
{
   alsa_qsa_t *alsa = (alsa_qsa_t*)data;

   int err;

   if((err = snd_pcm_nonblock_mode(alsa->pcm, state)) < 0)
   {
      RARCH_ERR("Can't set blocking mode to %d: %s\n", state,
            snd_strerror(err));
      return;
   }

   alsa->nonblock = state;
}

static bool alsa_qsa_use_float(void *data)
{
   alsa_qsa_t *alsa = (alsa_qsa_t*)data;
   return alsa->has_float;
}

static void alsa_qsa_free(void *data)
{
   alsa_qsa_t *alsa = (alsa_qsa_t*)data;

   if (alsa)
   {
      if (alsa->pcm)
      {
         snd_pcm_close(alsa->pcm);
         alsa->pcm = NULL;
      }
      free(alsa->buffer);
      free(alsa->buffer_chunk);
      free(alsa);
   }
}

static size_t alsa_qsa_write_avail(void *data)
{
   alsa_qsa_t *alsa = (alsa_qsa_t*)data;
   size_t avail = (alsa->buf_count -
         (int)alsa->buffered_blocks - 1) * alsa->buf_size +
      (alsa->buf_size - (int)alsa->buffer_ptr);
   return avail;
}

static size_t alsa_qsa_buffer_size(void *data)
{
   alsa_qsa_t *alsa = (alsa_qsa_t*)data;
   return alsa->buf_size * alsa->buf_count;
}

audio_driver_t audio_alsa = {
   alsa_qsa_init,
   alsa_qsa_write,
   alsa_qsa_stop,
   alsa_qsa_start,
   alsa_qsa_alive,
   alsa_qsa_set_nonblock_state,
   alsa_qsa_free,
   alsa_qsa_use_float,
   "alsa",
   NULL,
   NULL,
   alsa_qsa_write_avail,
   alsa_qsa_buffer_size,
};

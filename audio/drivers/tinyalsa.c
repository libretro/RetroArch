/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2017      - Charlton Head
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

/* See https://github.com/tinyalsa/tinyalsa */

#include <errno.h>
#include <string.h>

#include "../../deps/tinyalsa/pcm.h"

#include "../audio_driver.h"
#include "../../verbosity.h"

typedef struct tinyalsa {
	struct pcm     *pcm;
	size_t         buffer_size;
	bool           nonblock;
	bool           has_float;
	bool           can_pause;
	bool           is_paused;
	unsigned int   frame_bits;
} tinyalsa_t;

typedef long pcm_sframes_t;

#define BYTES_TO_FRAMES(bytes, frame_bits)  ((bytes) * 8 / frame_bits)
#define FRAMES_TO_BYTES(frames, frame_bits) ((frames) * frame_bits / 8)

static void *
tinyalsa_init(const char *device, unsigned rate,
               unsigned latency, unsigned block_frames,
               unsigned *new_rate)
{
	pcm_sframes_t buffer_size;
	struct pcm_config config;

	tinyalsa_t *tinyalsa = (tinyalsa_t*)calloc(1, sizeof(tinyalsa_t));
	if (!tinyalsa)
		return NULL;

	config.rate              = rate;
	config.format            = PCM_FORMAT_S16_LE;
	config.channels          = 2;
	config.period_size       = 1024;
	config.period_count      = 2;
	config.start_threshold   = 1024;
	config.silence_threshold = 1024 * 2;
	config.stop_threshold    = 1024 * 2;

	tinyalsa->pcm = pcm_open(0, 0, PCM_OUT, &config);

	if (tinyalsa->pcm == NULL) {
		RARCH_ERR("[TINYALSA]: Failed to allocate memory for pcm.\n");
		goto error;
	} else if (!pcm_is_ready(tinyalsa->pcm)) {
		RARCH_ERR("[TINYALSA]: Cannot open audio device.\n");
		goto error;
	}

	buffer_size           = pcm_get_buffer_size(tinyalsa->pcm);
	tinyalsa->buffer_size = pcm_frames_to_bytes(tinyalsa->pcm, buffer_size);
	tinyalsa->frame_bits  = pcm_format_to_bits(config.format) * 2;
	
	tinyalsa->can_pause   = true;
	tinyalsa->has_float   = false;

	RARCH_LOG("[TINYALSA] %u \n", (unsigned int)tinyalsa->buffer_size);

	return tinyalsa;

error:
	RARCH_ERR("[TINYALSA]: Failed to initialize tinyalsa driver.\n");
	return NULL;
}

static ssize_t
tinyalsa_write(void *data, const void *buf_, size_t size_)
{
   tinyalsa_t *tinyalsa  = (tinyalsa_t*)data;
   const uint8_t *buf    = (const uint8_t*)buf_;
   pcm_sframes_t written = 0;
   pcm_sframes_t size    = BYTES_TO_FRAMES(size_, tinyalsa->frame_bits);
   size_t frames_size    = tinyalsa->has_float ? sizeof(float) : sizeof(int16_t);

   if (tinyalsa->nonblock)
   {
      while (size)
      {
         pcm_sframes_t frames   = pcm_writei(tinyalsa->pcm, buf, size);

         if (frames == -EPIPE || frames == -EINTR || frames == -ESTRPIPE)
         {
            break;
         }
         else if (frames == -EAGAIN)
            break;
         if (frames < 0)
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
         pcm_sframes_t frames;
         int rc = pcm_wait(tinyalsa->pcm, -1);

         if (rc == -EPIPE || rc == -ESTRPIPE || rc == -EINTR)
            continue;

         frames   = pcm_writei(tinyalsa->pcm, buf, size);

         if (frames == -EPIPE || frames == -EINTR || frames == -ESTRPIPE)
         {
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

static bool
tinyalsa_stop(void *data)
{
	tinyalsa_t *tinyalsa = (tinyalsa_t*)data;

	if (tinyalsa->can_pause && !tinyalsa->is_paused) {
		int ret = pcm_start(tinyalsa->pcm);
		if (ret < 0)
			return false;

		tinyalsa->is_paused = true;
	}

	return true;
}

static bool
tinyalsa_alive(void *data)
{
	tinyalsa_t *tinyalsa = (tinyalsa_t*)data;

	if (tinyalsa)
		return !tinyalsa->is_paused;

	return false;
}

static bool
tinyalsa_start(void *data, bool is_shutdown)
{
	tinyalsa_t *tinyalsa = (tinyalsa_t*)data;

	if (tinyalsa->can_pause && tinyalsa->is_paused) {
		int ret = pcm_stop(tinyalsa->pcm);

		if (ret < 0) {
			RARCH_ERR("[TINYALSA]: Failed to unpause.\n");
			return false;
		}
		
		tinyalsa->is_paused = false;
	}

	return true;
}

static void
tinyalsa_set_nonblock_state(void *data, bool state)
{
	tinyalsa_t *tinyalsa = (tinyalsa_t*)data;
	tinyalsa->nonblock = state;
}

static bool
tinyalsa_use_float(void *data)
{
	tinyalsa_t *tinyalsa = (tinyalsa_t*)data;

	return tinyalsa->has_float;
}

static void
tinyalsa_free(void *data)
{
	tinyalsa_t *tinyalsa = (tinyalsa_t*)data;

	if (tinyalsa) {
		if (tinyalsa->pcm) {
			pcm_close(tinyalsa->pcm);
			tinyalsa->pcm = NULL;
		}
		free(tinyalsa);
	}
}

static size_t tinyalsa_write_avail(void *data)
{
   tinyalsa_t *alsa        = (tinyalsa_t*)data;
   pcm_sframes_t avail     = pcm_avail_update(alsa->pcm);

   if (avail < 0)
      return alsa->buffer_size;

   return FRAMES_TO_BYTES(avail, alsa->frame_bits);
}

static size_t tinyalsa_buffer_size(void *data)
{
	tinyalsa_t *tinyalsa = (tinyalsa_t*)data;
	
	return tinyalsa->buffer_size;
}

audio_driver_t audio_tinyalsa = {
	tinyalsa_init,               /* AUDIO_init              */
	tinyalsa_write,              /* AUDIO_write             */
	tinyalsa_stop,               /* AUDIO_stop              */
	tinyalsa_start,              /* AUDIO_start             */
	tinyalsa_alive,              /* AUDIO_alive             */
	tinyalsa_set_nonblock_state, /* AUDIO_set_nonblock_sate */
	tinyalsa_free,               /* AUDIO_free              */
	tinyalsa_use_float,          /* AUDIO_use_float         */
	"tinyalsa",                  /* "AUDIO"                 */
	NULL,                        /* AUDIO_device_list_new   */ /*TODO*/
	NULL,                        /* AUDIO_device_list_free  */ /*TODO*/
   tinyalsa_write_avail,        /* AUDIO_write_avail       */ /*TODO*/
	tinyalsa_buffer_size,        /* AUDIO_buffer_size       */ /*TODO*/
};

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
#include <string.h>
#include <time.h>

#ifdef __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include <retro_miscellaneous.h>
#include <retro_timers.h>

#include "../../retroarch.h"
#include "../../verbosity.h"

#define BUFSIZE 1024

typedef struct al
{
   ALuint source;
   ALuint *buffers;
   ALuint *res_buf;
   size_t res_ptr;
   ALenum format;
   size_t num_buffers;
   int rate;

   uint8_t tmpbuf[BUFSIZE];
   size_t tmpbuf_ptr;

   ALCdevice *handle;
   ALCcontext *ctx;

   bool nonblock;
   bool is_paused;
} al_t;

static void al_free(void *data)
{
   al_t *al = (al_t*)data;

   if (!al)
      return;

   alSourceStop(al->source);
   alDeleteSources(1, &al->source);

   if (al->buffers)
      alDeleteBuffers(al->num_buffers, al->buffers);

   free(al->buffers);
   free(al->res_buf);
   alcMakeContextCurrent(NULL);

   if (al->ctx)
      alcDestroyContext(al->ctx);
   if (al->handle)
      alcCloseDevice(al->handle);
   free(al);
}

static void *al_init(const char *device, unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   al_t *al;

   (void)device;

   al = (al_t*)calloc(1, sizeof(al_t));
   if (!al)
      return NULL;

   al->handle = alcOpenDevice(NULL);
   if (!al->handle)
      goto error;

   al->ctx = alcCreateContext(al->handle, NULL);
   if (!al->ctx)
      goto error;

   alcMakeContextCurrent(al->ctx);

   al->rate = rate;

   /* We already use one buffer for tmpbuf. */
   al->num_buffers = (latency * rate * 2 * sizeof(int16_t)) / (1000 * BUFSIZE) - 1;
   if (al->num_buffers < 2)
      al->num_buffers = 2;

   RARCH_LOG("[OpenAL]: Using %u buffers of %u bytes.\n", (unsigned)al->num_buffers, BUFSIZE);

   al->buffers = (ALuint*)calloc(al->num_buffers, sizeof(ALuint));
   al->res_buf = (ALuint*)calloc(al->num_buffers, sizeof(ALuint));
   if (!al->buffers || !al->res_buf)
      goto error;

   alGenSources(1, &al->source);
   alGenBuffers(al->num_buffers, al->buffers);

   memcpy(al->res_buf, al->buffers, al->num_buffers * sizeof(ALuint));
   al->res_ptr = al->num_buffers;

   return al;

error:
   al_free(al);
   return NULL;
}

static bool al_unqueue_buffers(al_t *al)
{
   ALint val;

   alGetSourcei(al->source, AL_BUFFERS_PROCESSED, &val);

   if (val <= 0)
      return false;

   alSourceUnqueueBuffers(al->source, val, &al->res_buf[al->res_ptr]);
   al->res_ptr += val;
   return true;
}

static bool al_get_buffer(al_t *al, ALuint *buffer)
{
   if (!al->res_ptr)
   {
      for (;;)
      {
         if (al_unqueue_buffers(al))
            break;

         if (al->nonblock)
            return false;

         /* Must sleep as there is no proper blocking method. */
         retro_sleep(1);
      }
   }

   *buffer = al->res_buf[--al->res_ptr];
   return true;
}

static size_t al_fill_internal_buf(al_t *al, const void *buf, size_t size)
{
   size_t read_size = MIN(BUFSIZE - al->tmpbuf_ptr, size);
   memcpy(al->tmpbuf + al->tmpbuf_ptr, buf, read_size);
   al->tmpbuf_ptr += read_size;
   return read_size;
}

static ssize_t al_write(void *data, const void *buf_, size_t size)
{
   al_t           *al = (al_t*)data;
   const uint8_t *buf = (const uint8_t*)buf_;
   size_t     written = 0;

   while (size)
   {
      ALint val;
      ALuint buffer;
      size_t rc = al_fill_internal_buf(al, buf, size);

      written += rc;
      buf     += rc;
      size    -= rc;

      if (al->tmpbuf_ptr != BUFSIZE)
         break;

      if (!al_get_buffer(al, &buffer))
         break;

      alBufferData(buffer, AL_FORMAT_STEREO16, al->tmpbuf, BUFSIZE, al->rate);
      al->tmpbuf_ptr = 0;
      alSourceQueueBuffers(al->source, 1, &buffer);
      if (alGetError() != AL_NO_ERROR)
         return -1;

      alGetSourcei(al->source, AL_SOURCE_STATE, &val);
      if (val != AL_PLAYING)
         alSourcePlay(al->source);

      if (alGetError() != AL_NO_ERROR)
         return -1;
   }

   return written;
}

static bool al_stop(void *data)
{
   al_t *al = (al_t*)data;
   if (al)
      al->is_paused = true;
   return true;
}

static bool al_alive(void *data)
{
   al_t *al = (al_t*)data;
   if (!al)
      return false;
   return !al->is_paused;
}

static void al_set_nonblock_state(void *data, bool state)
{
   al_t *al = (al_t*)data;
   if (al)
      al->nonblock = state;
}

static bool al_start(void *data, bool is_shutdown)
{
   al_t *al = (al_t*)data;
   if (al)
      al->is_paused = false;
   return true;
}

static size_t al_write_avail(void *data)
{
   al_t *al = (al_t*)data;
   al_unqueue_buffers(al);
   return al->res_ptr * BUFSIZE + (BUFSIZE - al->tmpbuf_ptr);
}

static size_t al_buffer_size(void *data)
{
   al_t *al = (al_t*)data;
   return (al->num_buffers + 1) * BUFSIZE; /* Also got tmpbuf. */
}

static bool al_use_float(void *data)
{
   (void)data;
   return false;
}

audio_driver_t audio_openal = {
   al_init,
   al_write,
   al_stop,
   al_start,
   al_alive,
   al_set_nonblock_state,
   al_free,
   al_use_float,
   "openal",
   NULL,
   NULL,
   al_write_avail,
   al_buffer_size,
};

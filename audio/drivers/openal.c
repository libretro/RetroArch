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
#include <lists/string_list.h>
#include <string/stdstring.h>

#include "../audio_driver.h"
#include "../../verbosity.h"

#define OPENAL_BUFSIZE 1024

typedef struct al
{
   ALuint source;
   ALuint *buffers;
   ALuint *res_buf;
   ALCdevice *handle;
   ALCcontext *ctx;
   size_t res_ptr;
   ALsizei num_buffers;
   int rate;
   ALenum format;
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

static void *al_list_new(void *u)
{
   union string_list_elem_attr attr;
   const char *audio_out_device_list;
   struct string_list *sl = string_list_new();

   if (!sl)
      return NULL;

   attr.i = 0;

   if (alcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT"))
      audio_out_device_list = alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);
   else
      audio_out_device_list = alcGetString(NULL, ALC_DEVICE_SPECIFIER);

   if (audio_out_device_list)
   {
      while (*audio_out_device_list)
      {
         string_list_append(sl, audio_out_device_list, attr);
         audio_out_device_list += strlen(audio_out_device_list) + 1;
      }
   }

   return sl;
}


static void *al_init(const char *device, unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   size_t _latency;
   char *dev_id = NULL;
   al_t *al     = (al_t*)calloc(1, sizeof(al_t));
   if (!al)
      return NULL;

   if (device)
   {
      struct string_list *list = (struct string_list*)al_list_new(NULL);

       /* Search for device name first */
      if (list && list->elems)
      {
         int32_t idx_found = -1;
         if (list->elems)
         {
            size_t i;
            for (i = 0; i < list->size; i++)
            {
               if (string_is_equal(device, list->elems[i].data))
               {
                  RARCH_DBG("[OpenAL] Found device #%d: \"%s\".\n", i, list->elems[i].data);
                  idx_found = i;
                  dev_id    = strdup(list->elems[i].data);
                  break;
               }
            }
            /* Index was not found yet based on name string,
             * just assume id is a one-character number index. */

            if (idx_found == -1 && isdigit(device[0]))
            {
               idx_found = strtoul(device, NULL, 0);
               RARCH_LOG("[OpenAL] Fallback, device index is a single number index instead: %d.\n", idx_found);

               if (idx_found != -1)
               {
                  if (idx_found < (int32_t)list->size)
                  {
                     RARCH_LOG("[OpenAL] Corresponding name: %s.\n", list->elems[idx_found].data);
                     dev_id    = strdup(list->elems[idx_found].data);
                  }
               }
            }
         }
      }

      string_list_free(list);
   }

   al->handle = alcOpenDevice(dev_id);
   if (dev_id)
      free(dev_id);
   dev_id = NULL;
   if (!al->handle)
      goto error;

   al->ctx = alcCreateContext(al->handle, NULL);
   if (!al->ctx)
      goto error;

   alcMakeContextCurrent(al->ctx);

   al->rate  = rate;
   *new_rate = rate;

   if (alIsExtensionPresent("AL_EXT_FLOAT32"))
   {
      al->format      = alGetEnumValue("AL_FORMAT_STEREO_FLOAT32");
      _latency        = latency * rate * 2 * sizeof(float);
      RARCH_LOG("[OpenAL] Device supports float sample format\n");
   }
   else
   {
      al->format      = AL_FORMAT_STEREO16;
      _latency        = latency * rate * 2 * sizeof(int16_t);
   }

   al->num_buffers = _latency / (1000 * OPENAL_BUFSIZE);
   if (al->num_buffers < 2)
      al->num_buffers = 2;

   RARCH_LOG("[OpenAL] Using %u buffers of %u bytes (%s format).\n", (unsigned)al->num_buffers, OPENAL_BUFSIZE, (al->format == AL_FORMAT_STEREO16) ? "integer" : "float");

   al->buffers = (ALuint*)calloc(al->num_buffers, sizeof(ALuint));
   al->res_buf = (ALuint*)calloc(al->num_buffers, sizeof(ALuint));
   if (!al->buffers || !al->res_buf)
      goto error;

   alGenSources(1, &al->source);
   alSourcei(al->source, AL_LOOPING, AL_FALSE);
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
   while (al->res_ptr == 0)
   {
      if (al_unqueue_buffers(al))
         break;

#ifndef EMSCRIPTEN
      if (al->nonblock)
#endif
         return false;

#ifndef _WIN32
      /* Must sleep as there is no proper blocking method. */
      retro_sleep(1);
#endif
   }

   *buffer = al->res_buf[--al->res_ptr];
   return true;
}

static ssize_t al_write(void *data, const void *s, size_t len)
{
   al_t           *al = (al_t*)data;
   const uint8_t *buf = (const uint8_t*)s;
   size_t        _len = 0;

   while (len)
   {
      ALint val;
      ALuint buffer;
      size_t rc    = MIN(OPENAL_BUFSIZE, len);

      if (!al_get_buffer(al, &buffer))
         break;

      alBufferData(buffer, al->format, buf, rc, al->rate);
      alSourceQueueBuffers(al->source, 1, &buffer);

      _len           += rc;
      buf            += rc;
      len            -= rc;

      alGetSourcei(al->source, AL_SOURCE_STATE, &val);
      if (val != AL_PLAYING)
         alSourcePlay(al->source);
   }
   return _len;
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
   return al && !al->is_paused;
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
   return al->res_ptr * OPENAL_BUFSIZE;
}

static size_t al_buffer_size(void *data)
{
   al_t *al = (al_t*)data;
   return (al->num_buffers) * OPENAL_BUFSIZE;
}

static bool al_use_float(void *data)
{
   al_t *al = (al_t*)data;
   if (al->format == AL_FORMAT_STEREO16)
      return false;
   return true;
}

static void al_device_list_free(void *u, void *slp)
{
   struct string_list *sl = (struct string_list*)slp;

   if (sl)
      string_list_free(sl);
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
   al_list_new,
   al_device_list_free,
   al_write_avail,
   al_buffer_size,
};

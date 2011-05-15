/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
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
#include <rsound.h>
#include "buffer.h"
#include <stdbool.h>
#include "SDL.h"

typedef struct rsd
{
   rsound_t *rd;
   bool nonblock;
   volatile bool has_error;

   rsound_fifo_buffer_t *buffer;

   SDL_mutex *cond_lock;
   SDL_cond *cond;
} rsd_t;

static ssize_t audio_cb(void *data, size_t bytes, void *userdata)
{
   rsd_t *rsd = userdata;

   size_t avail = rsnd_fifo_read_avail(rsd->buffer);
   size_t write_size = bytes > avail ? avail : bytes;
   rsnd_fifo_read(rsd->buffer, data, write_size);
   SDL_CondSignal(rsd->cond);

   return write_size;
}

static void err_cb(void *userdata)
{
   rsd_t *rsd = userdata;
   rsd->has_error = true;
   SDL_CondSignal(rsd->cond);
}

static void* __rsd_init(const char* device, unsigned rate, unsigned latency)
{
   rsd_t *rsd = calloc(1, sizeof(rsd_t));
   if ( rsd == NULL )
      return NULL;

   rsound_t *rd;

   if ( rsd_init(&rd) < 0 )
   {
      free(rsd);
      return NULL;
   }

   rsd->cond_lock = SDL_CreateMutex();
   rsd->cond = SDL_CreateCond();

   rsd->buffer = rsnd_fifo_new(1024 * 4);

   int channels = 2;
   int format = RSD_S16_NE;

   rsd_set_param(rd, RSD_CHANNELS, &channels);
   rsd_set_param(rd, RSD_SAMPLERATE, &rate);
   rsd_set_param(rd, RSD_LATENCY, &latency);

   if ( device != NULL )
      rsd_set_param(rd, RSD_HOST, (void*)device);

   rsd_set_param(rd, RSD_FORMAT, &format);

   rsd_set_callback(rd, audio_cb, err_cb, 256, rsd);

   if ( rsd_start(rd) < 0 )
   {
      free(rsd);
      rsd_free(rd);
      return NULL;
   }

   rsd->rd = rd;
   return rsd;
}

static ssize_t __rsd_write(void* data, const void* buf, size_t size)
{
   rsd_t *rsd = data;

   if (rsd->has_error)
      return -1;

   if (rsd->nonblock)
   {
      rsd_callback_lock(rsd->rd);
      size_t avail = rsnd_fifo_write_avail(rsd->buffer);
      size_t write_amt = avail > size ? size : avail;
      rsnd_fifo_write(rsd->buffer, buf, write_amt);
      rsd_callback_unlock(rsd->rd);
      return write_amt;
   }
   else
   {
      size_t written = 0;
      while (written < size && !rsd->has_error)
      {
         rsd_callback_lock(rsd->rd);
         size_t avail = rsnd_fifo_write_avail(rsd->buffer);

         if (avail == 0)
         {
            rsd_callback_unlock(rsd->rd);
            if (!rsd->has_error)
            {
               SDL_mutexP(rsd->cond_lock);
               SDL_CondWait(rsd->cond, rsd->cond_lock);
               SDL_mutexV(rsd->cond_lock);
            }
         }
         else
         {
            size_t write_amt = size - written > avail ? avail : size - written;
            rsnd_fifo_write(rsd->buffer, (const char*)buf + written, write_amt);
            rsd_callback_unlock(rsd->rd);
            written += write_amt;
         }
      }
      return written;
   }
}

static bool __rsd_stop(void *data)
{
   rsd_t *rsd = data;
   rsd_stop(rsd->rd);

   return true;
}

static void __rsd_set_nonblock_state(void *data, bool state)
{
   rsd_t *rsd = data;
   rsd->nonblock = state;
}

static bool __rsd_start(void *data)
{
   rsd_t *rsd = data;
   if ( rsd_start(rsd->rd) < 0)
      return false;

   return true;
}

static void __rsd_free(void *data)
{
   rsd_t *rsd = data;

   rsd_stop(rsd->rd);
   rsd_free(rsd->rd);

   rsnd_fifo_free(rsd->buffer);
   SDL_DestroyMutex(rsd->cond_lock);
   SDL_DestroyCond(rsd->cond);

   free(rsd);
}

const audio_driver_t audio_rsound = {
   .init = __rsd_init,
   .write = __rsd_write,
   .stop = __rsd_stop,
   .start = __rsd_start,
   .set_nonblock_state = __rsd_set_nonblock_state,
   .free = __rsd_free,
   .ident = "rsound"
};
   

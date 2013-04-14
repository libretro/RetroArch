/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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


#include "../driver.h"
#include <stdlib.h>
#ifdef RARCH_CONSOLE
#include "../deps/librsound/rsound.h"
#else
#include <rsound.h>
#endif
#include "fifo_buffer.h"
#include "../boolean.h"
#include "../thread.h"

typedef struct rsd
{
   rsound_t *rd;
   bool nonblock;
   volatile bool has_error;

   fifo_buffer_t *buffer;

   slock_t *cond_lock;
   scond_t *cond;
} rsd_t;

static ssize_t rsound_audio_cb(void *data, size_t bytes, void *userdata)
{
   rsd_t *rsd = (rsd_t*)userdata;

   size_t avail = fifo_read_avail(rsd->buffer);
   size_t write_size = bytes > avail ? avail : bytes;
   fifo_read(rsd->buffer, data, write_size);
   scond_signal(rsd->cond);

   return write_size;
}

static void err_cb(void *userdata)
{
   rsd_t *rsd = (rsd_t*)userdata;
   rsd->has_error = true;
   scond_signal(rsd->cond);
}

static void *rs_init(const char *device, unsigned rate, unsigned latency)
{
   rsd_t *rsd = (rsd_t*)calloc(1, sizeof(rsd_t));
   if (!rsd)
      return NULL;

   rsound_t *rd;

   if (rsd_init(&rd) < 0)
   {
      free(rsd);
      return NULL;
   }

   rsd->cond_lock = slock_new();
   rsd->cond = scond_new();

   rsd->buffer = fifo_new(1024 * 4);

   int channels = 2;
   int format = RSD_S16_NE;

   rsd_set_param(rd, RSD_CHANNELS, &channels);
   rsd_set_param(rd, RSD_SAMPLERATE, &rate);
   rsd_set_param(rd, RSD_LATENCY, &latency);

   if (device)
      rsd_set_param(rd, RSD_HOST, (void*)device);

   rsd_set_param(rd, RSD_FORMAT, &format);

   rsd_set_callback(rd, rsound_audio_cb, err_cb, 256, rsd);

   if (rsd_start(rd) < 0)
   {
      free(rsd);
      rsd_free(rd);
      return NULL;
   }

   rsd->rd = rd;
   return rsd;
}

static ssize_t rs_write(void *data, const void *buf, size_t size)
{
   rsd_t *rsd = (rsd_t*)data;

   if (rsd->has_error)
      return -1;

   if (rsd->nonblock)
   {
      rsd_callback_lock(rsd->rd);
      size_t avail = fifo_write_avail(rsd->buffer);
      size_t write_amt = avail > size ? size : avail;
      fifo_write(rsd->buffer, buf, write_amt);
      rsd_callback_unlock(rsd->rd);
      return write_amt;
   }
   else
   {
      size_t written = 0;
      while (written < size && !rsd->has_error)
      {
         rsd_callback_lock(rsd->rd);
         size_t avail = fifo_write_avail(rsd->buffer);

         if (avail == 0)
         {
            rsd_callback_unlock(rsd->rd);
            if (!rsd->has_error)
            {
               slock_lock(rsd->cond_lock);
               scond_wait(rsd->cond, rsd->cond_lock);
               slock_unlock(rsd->cond_lock);
            }
         }
         else
         {
            size_t write_amt = size - written > avail ? avail : size - written;
            fifo_write(rsd->buffer, (const char*)buf + written, write_amt);
            rsd_callback_unlock(rsd->rd);
            written += write_amt;
         }
      }
      return written;
   }
}

static bool rs_stop(void *data)
{
   rsd_t *rsd = (rsd_t*)data;
   rsd_stop(rsd->rd);

   return true;
}

static void rs_set_nonblock_state(void *data, bool state)
{
   rsd_t *rsd = (rsd_t*)data;
   rsd->nonblock = state;
}

static bool rs_start(void *data)
{
   rsd_t *rsd = (rsd_t*)data;
   if (rsd_start(rsd->rd) < 0)
      return false;

   return true;
}

static void rs_free(void *data)
{
   rsd_t *rsd = (rsd_t*)data;

   rsd_stop(rsd->rd);
   rsd_free(rsd->rd);

   fifo_free(rsd->buffer);
   slock_free(rsd->cond_lock);
   scond_free(rsd->cond);

   free(rsd);
}

static size_t rs_write_avail(void *data)
{
   rsd_t *rsd = (rsd_t*)data;

   if (rsd->has_error)
      return 0;
   rsd_callback_lock(rsd->rd);
   size_t val = fifo_write_avail(rsd->buffer);
   rsd_callback_unlock(rsd->rd);
   return val;
}

static size_t rs_buffer_size(void *data)
{
   (void)data;
   return 1024 * 4;
}

const audio_driver_t audio_rsound = {
   rs_init,
   rs_write,
   rs_stop,
   rs_start,
   rs_set_nonblock_state,
   rs_free,
   NULL,
   "rsound",
   rs_write_avail,
   rs_buffer_size,
};
   

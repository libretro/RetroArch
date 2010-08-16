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
#include <rsound.h>

typedef struct rsd
{
   rsound_t *rd;
   int latency;
   int rate;
   int nonblock;
} rsd_t;

static void* __rsd_init(const char* device, int rate, int latency)
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

   int channels = 2;
   int format = RSD_S16_NE;

   rsd_set_param(rd, RSD_CHANNELS, &channels);
   rsd_set_param(rd, RSD_SAMPLERATE, &rate);

   if ( device != NULL )
      rsd_set_param(rd, RSD_HOST, (void*)device);

   rsd_set_param(rd, RSD_FORMAT, &format);

   if ( rsd_start(rd) < 0 )
   {
      free(rsd);
      rsd_free(rd);
      return NULL;
   }

   int min_latency = (rsd_delay_ms(rd) > latency) ? (rsd_delay_ms(rd) * 3 / 2) : latency;

   rsd_set_param(rd, RSD_LATENCY, &min_latency);

   rsd->rd = rd;
   rsd->latency = min_latency;
   rsd->rate = rate;

   return rsd;
}

static ssize_t __rsd_write(void* data, const void* buf, size_t size)
{
   rsd_t *rsd = data;

   if ( rsd_delay_ms(rsd->rd) > rsd->latency && rsd->nonblock )
      return 0;

   if ( size == 0 )
      return 0;

   rsd_delay_wait(rsd->rd);
   if ( rsd_write(rsd->rd, buf, size) == 0 )
      return -1;

   if ( rsd_delay_ms(rsd->rd) < rsd->latency/2 )
   {
      int ms = rsd->latency/2;
      size_t size = (ms * rsd->rate * 4) / 1000;
      void *temp = calloc(1, size);
      rsd_write(rsd->rd, temp, size);
      free(temp);
   }

   return size;
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
   free(rsd);
}

const audio_driver_t audio_rsound = {
   .init = __rsd_init,
   .write = __rsd_write,
   .stop = __rsd_stop,
   .start = __rsd_start,
   .set_nonblock_state = __rsd_set_nonblock_state,
   .free = __rsd_free
};

   


   
   

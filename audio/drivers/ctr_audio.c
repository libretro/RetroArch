
/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "../../general.h"
#include "../../driver.h"

static void *ctr_audio_init(const char *device, unsigned rate, unsigned latency)
{
   (void)device;
   (void)rate;
   (void)latency;
   return (void*)-1;
}

static void ctr_audio_free(void *data)
{
   (void)data;
}

static ssize_t ctr_audio_write(void *data, const void *buf, size_t size)
{
   (void)data;
   (void)buf;

   return size;
}

static bool ctr_audio_stop(void *data)
{
   (void)data;
   return true;
}

static bool ctr_audio_alive(void *data)
{
   (void)data;
   return true;
}

static bool ctr_audio_start(void *data)
{
   (void)data;
   return true;
}

static void ctr_audio_set_nonblock_state(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool ctr_audio_use_float(void *data)
{
   (void)data;
   return true;
}

static size_t ctr_audio_write_avail(void *data)
{
   (void)data;
   return 0;
}

audio_driver_t audio_ctr = {
   ctr_audio_init,
   ctr_audio_write,
   ctr_audio_stop,
   ctr_audio_start,
   ctr_audio_alive,
   ctr_audio_set_nonblock_state,
   ctr_audio_free,
   ctr_audio_use_float,
   "ctr",
   ctr_audio_write_avail,
   NULL
};

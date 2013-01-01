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

#include "../general.h"
#include "../driver.h"

static void *null_audio_init(const char *device, unsigned rate, unsigned latency)
{
   (void)device;
   (void)rate;
   (void)latency;
   return (void*)-1;
}

static void null_audio_free(void *data)
{
   (void)data;
}

static ssize_t null_audio_write(void *data, const void *buf, size_t size)
{
   (void)data;
   (void)buf;

   return size;
}

static bool null_audio_stop(void *data)
{
   (void)data;
   return true;
}

static bool null_audio_start(void *data)
{
   (void)data;
   return true;
}

static void null_audio_set_nonblock_state(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool null_audio_use_float(void *data)
{
   (void)data;
   return true;
}

const audio_driver_t audio_null = {
   null_audio_init,
   null_audio_write,
   null_audio_stop,
   null_audio_start,
   null_audio_set_nonblock_state,
   null_audio_free,
   null_audio_use_float,
   "null",
};


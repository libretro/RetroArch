/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Michael Lelli
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
#include "../general.h"

#include "RWebAudio.h"

static void ra_free(void *data)
{
   RWebAudioFree();
}

static void *ra_init(const char *device, unsigned rate, unsigned latency)
{
   (void)device;
   (void)rate;
   void *data = RWebAudioInit(latency);
   if (data)
      g_settings.audio.out_rate = RWebAudioSampleRate();
   return data;
}

static ssize_t ra_write(void *data, const void *buf, size_t size)
{
   (void)data;
   return RWebAudioWrite(buf, size);
}

static bool ra_stop(void *data)
{
   (void)data;
   return RWebAudioStop();
}

static void ra_set_nonblock_state(void *data, bool state)
{
   (void)data;
   RWebAudioSetNonblockState(state);
}

static bool ra_start(void *data)
{
   (void)data;
   return RWebAudioStart();
}

static bool ra_use_float(void *data)
{
   (void)data;
   return true;
}

static size_t ra_write_avail(void *data)
{
   (void)data;
   return RWebAudioWriteAvail();
}

static size_t ra_buffer_size(void *data)
{
   (void)data;
   return RWebAudioBufferSize();
}

const audio_driver_t audio_rwebaudio = {
   ra_init,
   ra_write,
   ra_stop,
   ra_start,
   ra_set_nonblock_state,
   ra_free,
   ra_use_float,
   "rwebaudio",
   ra_write_avail,
   ra_buffer_size,
};


/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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
#include "xaudio-c/xaudio-c.h"
#include "../general.h"

typedef struct
{
   xaudio2_t *xa;
   bool nonblock;
   size_t bufsize;
} xa_t;

static void *xa_init(const char *device, unsigned rate, unsigned latency)
{
   if (latency < 8)
      latency = 8; // Do not allow shenanigans.

   xa_t *xa = (xa_t*)calloc(1, sizeof(*xa));
   if (!xa)
      return NULL;

   size_t bufsize = latency * rate / 1000;

   RARCH_LOG("XAudio2: Requesting %d ms latency, using %d ms latency.\n",
         latency, (int)bufsize * 1000 / rate);

   xa->bufsize = bufsize * 2 * sizeof(float);

   unsigned device_index = 0;
   if (device)
      device_index = strtoul(device, NULL, 0);

   xa->xa = xaudio2_new(rate, 2, xa->bufsize, device_index);
   if (!xa->xa)
   {
      RARCH_ERR("Failed to init XAudio2.\n");
      free(xa);
      return NULL;
   }

   if (g_extern.verbose)
      xaudio2_enumerate_devices(xa->xa);

   return xa;
}

static ssize_t xa_write(void *data, const void *buf, size_t size)
{
   xa_t *xa = (xa_t*)data;
   if (xa->nonblock)
   {
      size_t avail = xaudio2_write_avail(xa->xa);
      if (avail == 0)
         return 0;
      if (avail < size)
         size = avail;
   }

   size_t ret = xaudio2_write(xa->xa, buf, size);
   if (ret == 0 && size > 0)
      return -1;
   return ret;
}

static bool xa_stop(void *data)
{
   (void)data;
   return true;
}

static void xa_set_nonblock_state(void *data, bool state)
{
   xa_t *xa = (xa_t*)data;
   xa->nonblock = state;
}

static bool xa_start(void *data)
{
   (void)data;
   return true;
}

static bool xa_use_float(void *data)
{
   (void)data;
   return true;
}

static void xa_free(void *data)
{
   xa_t *xa = (xa_t*)data;
   if (xa)
   {
      if (xa->xa)
         xaudio2_free(xa->xa);
      free(xa);
   }
}

static size_t xa_write_avail(void *data)
{
   xa_t *xa = (xa_t*)data;
   return xaudio2_write_avail(xa->xa);
}

static size_t xa_buffer_size(void *data)
{
   xa_t *xa = (xa_t*)data;
   return xa->bufsize;
}

const audio_driver_t audio_xa = {
   xa_init,
   xa_write,
   xa_stop,
   xa_start,
   xa_set_nonblock_state,
   xa_free,
   xa_use_float,
   "xaudio",
   xa_write_avail,
   xa_buffer_size,
};

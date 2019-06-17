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

#include <stdint.h>
#include <stdlib.h>

#include <errno.h>

#include <roaraudio.h>

#include <boolean.h>

#include "../../retroarch.h"
#include "../../verbosity.h"

typedef struct
{
   roar_vs_t *vss;
   bool nonblocking;
   bool is_paused;
} roar_t;

static void *ra_init(const char *device, unsigned rate, unsigned latency,
      unsigned block_frames, unsigned *new_rate)
{
   int err;
   roar_vs_t *vss = NULL;
   roar_t   *roar = (roar_t*)calloc(1, sizeof(roar_t));

   if (!roar)
      return NULL;

   (void)latency;

   if ((vss = roar_vs_new_simple(device, "RetroArch", rate, 2, ROAR_CODEC_PCM_S, 16, ROAR_DIR_PLAY, &err)) == NULL)
   {
      RARCH_ERR("RoarAudio: \"%s\"\n", roar_vs_strerr(err));
      free(roar);
      return NULL;
   }

   roar_vs_role(vss, ROAR_ROLE_GAME, NULL);
   roar->vss = vss;

   return roar;
}

static ssize_t ra_write(void *data, const void *buf, size_t size)
{
   int err;
   size_t written = 0;
   roar_t   *roar = (roar_t*)data;

   if (size == 0)
      return 0;

   while (written < size)
   {
      ssize_t rc;
      size_t write_amt = size - written;

      if ((rc = roar_vs_write(roar->vss,
                  (const char*)buf + written, write_amt, &err)) < (ssize_t)write_amt)
      {
         if (roar->nonblocking)
            return rc;
         else if (rc < 0)
            return -1;
      }
      written += rc;
   }

   return size;
}

static bool ra_stop(void *data)
{
   roar_t *roar = (roar_t*)data;
   if (roar)
      roar->is_paused = true;
   return true;
}

static bool ra_alive(void *data)
{
   roar_t *roar = (roar_t*)data;
   if (!roar)
      return false;
   return !roar->is_paused;
}

static void ra_set_nonblock_state(void *data, bool state)
{
   roar_t *roar = (roar_t*)data;
   if (roar_vs_blocking(roar->vss, (state) ? ROAR_VS_FALSE : ROAR_VS_TRUE, NULL) < 0)
      fprintf(stderr, "RetroArch [ERROR]: Can't set nonblocking. Will not be able to fast-forward.\n");
   roar->nonblocking = state;
}

static bool ra_start(void *data, bool is_shutdown)
{
   roar_t *roar = (roar_t*)data;
   if (roar)
      roar->is_paused = false;
   return true;
}

static void ra_free(void *data)
{
   roar_t *roar = (roar_t*)data;
   roar_vs_close(roar->vss, ROAR_VS_TRUE, NULL);
   free(data);
}

static bool ra_use_float(void *data)
{
   return false;
}

static size_t ra_write_avail(void *data)
{
   (void)data;
   return 0;
}

audio_driver_t audio_roar = {
   ra_init,
   ra_write,
   ra_stop,
   ra_start,
   ra_alive,
   ra_set_nonblock_state,
   ra_free,
   ra_use_float,
   "roar",
   NULL,
   NULL,
   ra_write_avail,
   NULL
};

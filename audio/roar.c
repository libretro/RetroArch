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


#include "driver.h"
#include <stdlib.h>
#include <roaraudio.h>
#include <errno.h>
#include <stdio.h>
#include "../boolean.h"
#include "general.h"

typedef struct
{
   roar_vs_t *vss;
   bool nonblocking;
} roar_t;

static void *ra_init(const char *device, unsigned rate, unsigned latency)
{
   (void)latency;
   int err;
   roar_t *roar = (roar_t*)calloc(1, sizeof(roar_t));
   if (roar == NULL)
      return NULL;

   roar_vs_t *vss;
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
   roar_t *roar = (roar_t*)data;
   ssize_t rc;

   if (size == 0)
      return 0;

   int err;
   size_t written = 0;
   while (written < size)
   {
      size_t write_amt = size - written;
      if ((rc = roar_vs_write(roar->vss, (const char*)buf + written, write_amt, &err)) < (ssize_t)write_amt)
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
   (void)data;
   return true;
}

static void ra_set_nonblock_state(void *data, bool state)
{
   roar_t *roar = (roar_t*)data;
   if (roar_vs_blocking(roar->vss, (state) ? ROAR_VS_FALSE : ROAR_VS_TRUE, NULL) < 0)
      fprintf(stderr, "RetroArch [ERROR]: Can't set nonblocking. Will not be able to fast-forward.\n");
   roar->nonblocking = state;
}

static bool ra_start(void *data)
{
   (void)data;
   return true;
}

static void ra_free(void *data)
{
   roar_t *roar = (roar_t*)data;
   roar_vs_close(roar->vss, ROAR_VS_TRUE, NULL);
   free(data);
}

const audio_driver_t audio_roar = {
   ra_init,
   ra_write,
   ra_stop,
   ra_start,
   ra_set_nonblock_state,
   ra_free,
   NULL,
   "roar"
};


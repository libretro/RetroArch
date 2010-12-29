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
#include <roaraudio.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct
{
   roar_vs_t *vss;
   bool nonblocking;
} roar_t;

static void* __roar_init(const char* device, int rate, int latency)
{
   int err;
   roar_t *roar = calloc(1, sizeof(roar_t));
   if (roar == NULL)
      return NULL;

   roar_vs_t *vss;
   if ( (vss = roar_vs_new_simple(NULL, NULL, rate, 2, ROAR_CODEC_PCM_S_LE, 16, ROAR_DIR_PLAY, &err)) == NULL )
   {
      fprintf(stderr, "roar_vs: \"%s\"\n", roar_vs_strerr(err));
      free(roar);
      return NULL;
   }

   roar->vss = vss;

   return roar;
}

static ssize_t __roar_write(void* data, const void* buf, size_t size)
{
   roar_t *roar = data;
   ssize_t rc;

   if ( size == 0 )
      return 0;

   int err;
   size_t written = 0;
   while (written < size)
   {
      if ((rc = roar_vs_write(roar->vss, (const char*)buf + written, size - written, &err)) < size)
      {
         if (rc < 0)
            return -1;
         else if (roar->nonblocking)
            return 0;
      }
      written += rc;
   }

   return size;
}

static bool __roar_stop(void *data)
{
   (void)data;
   return true;
}

static void __roar_set_nonblock_state(void *data, bool state)
{
   roar_t *roar = data;
   if (roar_vs_blocking(roar->vss, (state) ? ROAR_VS_FALSE : ROAR_VS_TRUE, NULL) < 0)
      fprintf(stderr, "SSNES [ERROR]: Can't set nonblocking. Will not be able to fast-forward.\n");
   roar->nonblocking = state;
}

static bool __roar_start(void *data)
{
   (void)data;
   return true;
}

static void __roar_free(void *data)
{
   roar_t *roar = data;
   roar_vs_close(roar->vss, ROAR_VS_TRUE, NULL);
   free(data);
}

const audio_driver_t audio_roar = {
   .init = __roar_init,
   .write = __roar_write,
   .stop = __roar_stop,
   .start = __roar_start,
   .set_nonblock_state = __roar_set_nonblock_state,
   .free = __roar_free,
   .ident = "roar"
};

   


   
   

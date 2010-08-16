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

static void* __roar_init(const char* device, int rate, int latency)
{
   int err;
   roar_vs_t *vss;
   if ( (vss = roar_vs_new_simple(NULL, NULL, rate, 2, ROAR_CODEC_PCM_S_LE, 16, ROAR_DIR_PLAY, &err)) == NULL )
   {
      fprintf(stderr, "roar_vs: \"%s\"\n", roar_vs_strerr(err));
      return NULL;
   }

   return vss;
}

static ssize_t __roar_write(void* data, const void* buf, size_t size)
{
   roar_vs_t *vss = data;

   if ( size == 0 )
      return 0;

   int err;
   if (roar_vs_write(vss, buf, size, &err) < 0)
   {
      if (err == ROAR_ERROR_NONE)
         return 0;
      return -1;
   }

   return size;
}

static bool __roar_stop(void *data)
{
   return true;
}

static void __roar_set_nonblock_state(void *data, bool state)
{
   roar_vs_t *vss = data;
   if (roar_vs_blocking(vss, (state) ? ROAR_VS_FALSE : ROAR_VS_TRUE, NULL) < 0)
      fprintf(stderr, "SSNES [ERROR]: Can't set nonblocking. Will not be able to fast-forward.\n");
}

static bool __roar_start(void *data)
{
   return true;
}

static void __roar_free(void *data)
{
   roar_vs_t *vss = data;
   roar_vs_close(vss, ROAR_VS_TRUE, NULL);
}

const audio_driver_t audio_roar = {
   .init = __roar_init,
   .write = __roar_write,
   .stop = __roar_stop,
   .start = __roar_start,
   .set_nonblock_state = __roar_set_nonblock_state,
   .free = __roar_free
};

   


   
   

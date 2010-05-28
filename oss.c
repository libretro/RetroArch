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
#include <sys/soundcard.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

static void* __oss_init(const char* device, int rate, int latency)
{
   int *fd = calloc(1, sizeof(int));
   if ( fd == NULL )
      return NULL;

   const char *oss_device = "/dev/dsp";

   if ( device != NULL )
      oss_device = device;

   if ( (*fd = open(oss_device, O_WRONLY)) < 0 )
   {
      free(fd);
      return NULL;
   }

   int frags = (latency * rate * 4)/(1000 * (1 << 9));
   int frag = (frags << 16) | 9;

   if ( ioctl(*fd, SNDCTL_DSP_SETFRAGMENT, &frag) < 0 )
   {
      close(*fd);
      free(fd);
      return NULL;
   }

   int channels = 2;
   int format = AFMT_S16_LE;

   if ( ioctl(*fd, SNDCTL_DSP_CHANNELS, &channels) < 0 )
   {
      close(*fd);
      free(fd);
      return NULL;
   }

   if ( ioctl(*fd, SNDCTL_DSP_SETFMT, &format) < 0 )
   {
      close(*fd);
      free(fd);
      return NULL;
   }

   if ( ioctl(*fd, SNDCTL_DSP_SPEED, &rate) < 0 )
   {
      close(*fd);
      free(fd);
      return NULL;
   }

   return fd;
}

static ssize_t __oss_write(void* data, const void* buf, size_t size)
{
   int *fd = data;

   if ( size == 0 )
      return 0;

   ssize_t ret;
   if ( (ret = write(*fd, buf, size)) <= 0 )
      return -1;

   return ret;
}

static bool __oss_stop(void *data)
{
   int *fd = data;
   ioctl(*fd, SNDCTL_DSP_RESET, 0);
   return true;
}

static bool __oss_start(void *data)
{
   return true;
}

static void __oss_free(void *data)
{
   int *fd = data;

   ioctl(*fd, SNDCTL_DSP_RESET, 0);
   close(*fd);
   free(fd);
}

const audio_driver_t audio_oss = {
   .init = __oss_init,
   .write = __oss_write,
   .stop = __oss_stop,
   .start = __oss_start,
   .free = __oss_free
};

   


   
   

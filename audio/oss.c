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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "driver.h"
#include "general.h"
#include <stdlib.h>

#ifdef HAVE_OSS_BSD
#include <soundcard.h>
#else
#include <sys/soundcard.h>
#endif

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#ifdef HAVE_OSS_BSD
#define DEFAULT_OSS_DEV "/dev/audio"
#else
#define DEFAULT_OSS_DEV "/dev/dsp"
#endif

static void *oss_init(const char *device, unsigned rate, unsigned latency)
{
   int *fd = (int*)calloc(1, sizeof(int));
   if (fd == NULL)
      return NULL;

   const char *oss_device = DEFAULT_OSS_DEV;

   if (device != NULL)
      oss_device = device;

   if ((*fd = open(oss_device, O_WRONLY)) < 0)
   {
      free(fd);
      perror("open");
      return NULL;
   }

   int frags = (latency * rate * 4) / (1000 * (1 << 10));
   int frag = (frags << 16) | 10;

   if (ioctl(*fd, SNDCTL_DSP_SETFRAGMENT, &frag) < 0)
      RARCH_WARN("Cannot set fragment sizes. Latency might not be as expected ...\n");

   int channels = 2;
   int format = is_little_endian() ?
      AFMT_S16_LE : AFMT_S16_BE;

   if (ioctl(*fd, SNDCTL_DSP_CHANNELS, &channels) < 0)
   {
      close(*fd);
      free(fd);
      perror("ioctl");
      return NULL;
   }

   if (ioctl(*fd, SNDCTL_DSP_SETFMT, &format) < 0)
   {
      close(*fd);
      free(fd);
      perror("ioctl");
      return NULL;
   }

   int new_rate = rate;
   if (ioctl(*fd, SNDCTL_DSP_SPEED, &new_rate) < 0)
   {
      close(*fd);
      free(fd);
      perror("ioctl");
      return NULL;
   }

   if (new_rate != (int)rate)
   {
      RARCH_WARN("Requested sample rate not supported. Adjusting output rate to %d Hz.\n", new_rate);
      g_settings.audio.out_rate = new_rate;
   }

   return fd;
}

static ssize_t oss_write(void *data, const void *buf, size_t size)
{
   int *fd = (int*)data;

   if (size == 0)
      return 0;

   ssize_t ret;
   if ((ret = write(*fd, buf, size)) < 0)
   {
      if (errno == EAGAIN && (fcntl(*fd, F_GETFL) & O_NONBLOCK))
         return 0;

      return -1;
   }

   return ret;
}

static bool oss_stop(void *data)
{
   int *fd = (int*)data;
   ioctl(*fd, SNDCTL_DSP_RESET, 0);
   return true;
}

static bool oss_start(void *data)
{
   return true;
}

static void oss_set_nonblock_state(void *data, bool state)
{
   int *fd = (int*)data;
   int rc;
   if (state)
      rc = fcntl(*fd, F_SETFL, fcntl(*fd, F_GETFL) | O_NONBLOCK);
   else
      rc = fcntl(*fd, F_SETFL, fcntl(*fd, F_GETFL) & (~O_NONBLOCK));
   if (rc != 0)
      RARCH_WARN("Could not set nonblocking on OSS file descriptor. Will not be able to fast-forward.\n");
}

static void oss_free(void *data)
{
   int *fd = (int*)data;

   ioctl(*fd, SNDCTL_DSP_RESET, 0);
   close(*fd);
   free(fd);
}

static size_t oss_write_avail(void *data)
{
   int *fd = (int*)data;

   audio_buf_info info;
   if (ioctl(*fd, SNDCTL_DSP_GETOSPACE, &info) < 0)
   {
      RARCH_ERR("SNDCTL_DSP_GETOSPACE failed ...\n");
      return 0;
   }

   return info.bytes;
}

static size_t oss_buffer_size(void *data)
{
   int *fd = (int*)data;

   audio_buf_info info;
   if (ioctl(*fd, SNDCTL_DSP_GETOSPACE, &info) < 0)
   {
      RARCH_ERR("SNDCTL_DSP_GETOSPACE failed ...\n");
      return 1; // Return something non-zero to avoid SIGFPE.
   }

   return info.fragsize * info.fragstotal;
}

const audio_driver_t audio_oss = {
   oss_init,
   oss_write,
   oss_stop,
   oss_start,
   oss_set_nonblock_state,
   oss_free,
   NULL,
   "oss",
   oss_write_avail,
   oss_buffer_size,
};


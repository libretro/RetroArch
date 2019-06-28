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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>

#ifdef HAVE_OSS_BSD
#include <soundcard.h>
#else
#include <sys/soundcard.h>
#endif

#include <retro_endianness.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../../retroarch.h"
#include "../../verbosity.h"

#ifdef HAVE_OSS_BSD
#define DEFAULT_OSS_DEV "/dev/audio"
#else
#define DEFAULT_OSS_DEV "/dev/dsp"
#endif

static bool oss_is_paused = false;

static void *oss_init(const char *device, unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_out_rate)
{
   int frags, frag, channels, format, new_rate;
   int              *fd   = (int*)calloc(1, sizeof(int));
   const char *oss_device = device ? device : DEFAULT_OSS_DEV;

   if (!fd)
      return NULL;

   if ((*fd = open(oss_device, O_WRONLY)) < 0)
   {
      free(fd);
      perror("open");
      return NULL;
   }

   frags = (latency * rate * 4) / (1000 * (1 << 10));
   frag  = (frags << 16) | 10;

   if (ioctl(*fd, SNDCTL_DSP_SETFRAGMENT, &frag) < 0)
      RARCH_WARN("Cannot set fragment sizes. Latency might not be as expected ...\n");

   channels = 2;
   format   = is_little_endian() ? AFMT_S16_LE : AFMT_S16_BE;

   if (ioctl(*fd, SNDCTL_DSP_CHANNELS, &channels) < 0)
      goto error;

   if (ioctl(*fd, SNDCTL_DSP_SETFMT, &format) < 0)
      goto error;

   new_rate = rate;

   if (ioctl(*fd, SNDCTL_DSP_SPEED, &new_rate) < 0)
      goto error;

   if (new_rate != (int)rate)
   {
      RARCH_WARN("Requested sample rate not supported. Adjusting output rate to %d Hz.\n", new_rate);
      *new_out_rate = new_rate;
   }

   return fd;

error:
   close(*fd);
   free(fd);
   perror("ioctl");
   return NULL;
}

static ssize_t oss_write(void *data, const void *buf, size_t size)
{
   ssize_t ret;
   int *fd = (int*)data;

   if (size == 0)
      return 0;

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

   if (ioctl(*fd, SNDCTL_DSP_RESET, 0) < 0)
      return false;

   oss_is_paused = true;
   return true;
}

static bool oss_start(void *data, bool is_shutdown)
{
   (void)data;
   oss_is_paused = false;
   return true;
}

static bool oss_alive(void *data)
{
   (void)data;
   return !oss_is_paused;
}

static void oss_set_nonblock_state(void *data, bool state)
{
   int rc;
   int *fd = (int*)data;

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

   if (ioctl(*fd, SNDCTL_DSP_RESET, 0) < 0)
      return;

   close(*fd);
   free(fd);
}

static size_t oss_write_avail(void *data)
{
   audio_buf_info info;
   int *fd = (int*)data;

   if (ioctl(*fd, SNDCTL_DSP_GETOSPACE, &info) < 0)
   {
      RARCH_ERR("[OSS]: SNDCTL_DSP_GETOSPACE failed ...\n");
      return 0;
   }

   return info.bytes;
}

static size_t oss_buffer_size(void *data)
{
   audio_buf_info info;
   int *fd = (int*)data;

   if (ioctl(*fd, SNDCTL_DSP_GETOSPACE, &info) < 0)
   {
      RARCH_ERR("[OSS]: SNDCTL_DSP_GETOSPACE failed ...\n");
      return 1; /* Return something non-zero to avoid SIGFPE. */
   }

   return info.fragsize * info.fragstotal;
}

static bool oss_use_float(void *data)
{
   (void)data;
   return false;
}

audio_driver_t audio_oss = {
   oss_init,
   oss_write,
   oss_stop,
   oss_start,
   oss_alive,
   oss_set_nonblock_state,
   oss_free,
   oss_use_float,
   "oss",
   NULL,
   NULL,
   oss_write_avail,
   oss_buffer_size,
};

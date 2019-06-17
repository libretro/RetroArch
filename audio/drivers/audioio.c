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

#include <sys/audioio.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../../retroarch.h"
#include "../../verbosity.h"

#define DEFAULT_DEV "/dev/audio"

static void *audioio_init(const char *device, unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_out_rate)
{
   int *fd = (int*)calloc(1, sizeof(int));
   const char *audiodev = device ? device : DEFAULT_DEV;
   struct audio_info info;

   if (!fd)
      return NULL;

   AUDIO_INITINFO(&info);

#ifdef AUMODE_PLAY_ALL
   info.mode = AUMODE_PLAY_ALL;
#elif defined(AUMODE_PLAY)
   info.mode = AUMODE_PLAY;
#endif
   info.play.sample_rate = rate;
   info.play.channels = 2;
   info.play.precision = 16;
#ifdef AUDIO_ENCODING_SLINEAR
   info.play.encoding = AUDIO_ENCODING_SLINEAR;
#else
   info.play.encoding = AUDIO_ENCODING_LINEAR;
#endif

   if ((*fd = open(audiodev, O_WRONLY)) < 0)
   {
      free(fd);
      perror("open");
      return NULL;
   }

   if (ioctl(*fd, AUDIO_SETINFO, &info) < 0)
      goto error;

   if (ioctl(*fd, AUDIO_GETINFO, &info) < 0)
      goto error;

   *new_out_rate = info.play.sample_rate;

   return fd;
error:
   close(*fd);
   free(fd);
   perror("ioctl");
   return NULL;
}

static ssize_t audioio_write(void *data, const void *buf, size_t size)
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

static bool audioio_stop(void *data)
{
   struct audio_info info;
   int *fd = (int*)data;

#ifdef AUDIO_FLUSH
   if (ioctl(*fd, AUDIO_FLUSH, NULL) < 0)
      return false;
#endif

   if (ioctl(*fd, AUDIO_GETINFO, &info) < 0)
      return false;

   info.play.pause = true;

   return ioctl(*fd, AUDIO_SETINFO, &info) == 0;
}

static bool audioio_start(void *data, bool is_shutdown)
{
   struct audio_info info;
   int *fd = (int*)data;

#ifdef AUDIO_FLUSH
   if (ioctl(*fd, AUDIO_FLUSH, NULL) < 0)
      return false;
#endif

   if (ioctl(*fd, AUDIO_GETINFO, &info) < 0)
      return false;

   info.play.pause = false;

   return ioctl(*fd, AUDIO_SETINFO, &info) == 0;
}

static bool audioio_alive(void *data)
{
   struct audio_info info;
   int *fd = (int*)data;

   if (ioctl(*fd, AUDIO_GETINFO, &info) < 0)
      return false;

   return !info.play.pause;
}

static void audioio_set_nonblock_state(void *data, bool state)
{
   int rc;
   int *fd = (int*)data;

   if (state)
      rc = fcntl(*fd, F_SETFL, fcntl(*fd, F_GETFL) | O_NONBLOCK);
   else
      rc = fcntl(*fd, F_SETFL, fcntl(*fd, F_GETFL) & (~O_NONBLOCK));
   if (rc != 0)
      RARCH_WARN("Could not set nonblocking on audio file descriptor. Will not be able to fast-forward.\n");
}

static void audioio_free(void *data)
{
   int *fd = (int*)data;

#ifdef AUDIO_FLUSH
   (void)ioctl(*fd, AUDIO_FLUSH, NULL);
#endif

   close(*fd);
   free(fd);
}

static size_t audioio_buffer_size(void *data)
{
   struct audio_info info;
   int *fd = (int*)data;

   if (ioctl(*fd, AUDIO_GETINFO, &info) < 0)
      return false;

   return info.play.buffer_size;
}

static size_t audioio_write_avail(void *data)
{
   return audioio_buffer_size(data);
}

static bool audioio_use_float(void *data)
{
   (void)data;
   return false;
}

audio_driver_t audio_audioio = {
   audioio_init,
   audioio_write,
   audioio_stop,
   audioio_start,
   audioio_alive,
   audioio_set_nonblock_state,
   audioio_free,
   audioio_use_float,
   "audioio",
   NULL,
   NULL,
   audioio_write_avail,
   audioio_buffer_size,
};

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
#include <compat/strl.h>
#include <lists/string_list.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>

#ifdef HAVE_OSS_BSD
#include <soundcard.h>
#else
#include <sys/soundcard.h>
#endif

#include <retro_endianness.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../audio_driver.h"
#include "../../verbosity.h"

#ifdef HAVE_OSS_BSD
#define DEFAULT_OSS_DEV "/dev/audio"
#else
#define DEFAULT_OSS_DEV "/dev/dsp"
#endif

typedef struct oss_audio
{
   int fd;
   /* The rate the device settled on; sizes the wait bound below. */
   int rate;
   /* Frames handed to the device since it opened, for the sink rate
    * estimate; the device's own count is this less what is still
    * queued. The format is fixed at S16 stereo below, so a frame is
    * four bytes. */
   uint64_t frames_written;
   bool is_paused;
} oss_audio_t;

#define OSS_FRAME_BYTES 4

/* Iteration cap for oss_wait_writable(): bounds wakes that deliver no
 * space, so one call costs at most this many bounded waits. */
#define OSS_WAIT_WRITABLE_LAPS 8

static void *oss_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_out_rate)
{
   int frags, frag, channels, format, new_rate;
   oss_audio_t *ossaudio  = (oss_audio_t*)calloc(1, sizeof(oss_audio_t));
   const char *oss_device = device ? device : DEFAULT_OSS_DEV;

   if (!ossaudio)
      return NULL;

   if ((ossaudio->fd = open(oss_device, O_WRONLY)) < 0)
   {
      free(ossaudio);
      perror("open");
      return NULL;
   }

   frags = (latency * rate * 4) / (1000 * (1 << 10));
   frag  = (frags << 16) | 10;

   if (ioctl(ossaudio->fd, SNDCTL_DSP_SETFRAGMENT, &frag) < 0)
      RARCH_WARN("[OSS] Could not set fragment sizes. Latency might not be as expected.\n");

   channels = 2;
   format   = is_little_endian() ? AFMT_S16_LE : AFMT_S16_BE;

   if (ioctl(ossaudio->fd, SNDCTL_DSP_CHANNELS, &channels) < 0)
      goto error;

   if (ioctl(ossaudio->fd, SNDCTL_DSP_SETFMT, &format) < 0)
      goto error;

   new_rate = rate;

   if (ioctl(ossaudio->fd, SNDCTL_DSP_SPEED, &new_rate) < 0)
      goto error;

   if (new_rate != (int)rate)
   {
      RARCH_WARN("[OSS] Requested sample rate not supported. Adjusting output rate to %d Hz.\n", new_rate);
      *new_out_rate = new_rate;
   }
   ossaudio->rate = new_rate;

   return ossaudio;

error:
   close(ossaudio->fd);
   if (ossaudio)
      free(ossaudio);
   perror("ioctl");
   return NULL;
}

static ssize_t oss_write(void *data, const void *s, size_t len)
{
   ssize_t _len;
   oss_audio_t *ossaudio  = (oss_audio_t*)data;
   if (len == 0)
      return 0;
   if ((_len = write(ossaudio->fd, s, len)) < 0)
   {
      if (errno == EAGAIN && (fcntl(ossaudio->fd, F_GETFL) & O_NONBLOCK))
         return 0;
      return -1;
   }
   ossaudio->frames_written += (uint64_t)_len / OSS_FRAME_BYTES;
   return _len;
}

/* Frames the device has played since it opened.
 *
 * ALSA's shape: everything written, less what has not been played yet.
 * SNDCTL_DSP_GETODELAY reports that remainder in bytes - the whole of
 * the device's queue, which is what should come off - and a driver that
 * does not implement it fails the ioctl, which reports nothing rather
 * than a count that would read as a stall. */
static size_t oss_frames_consumed(void *data)
{
   oss_audio_t *ossaudio = (oss_audio_t*)data;
   int          delay    = 0;
   uint64_t     queued;

   if (!ossaudio || ossaudio->fd < 0)
      return 0;
   if (ioctl(ossaudio->fd, SNDCTL_DSP_GETODELAY, &delay) < 0 || delay < 0)
      return 0;

   queued = (uint64_t)delay / OSS_FRAME_BYTES;
   if (queued > ossaudio->frames_written)
      return 0;
   return (size_t)(ossaudio->frames_written - queued);
}

/* Sleep in select() on the device until it is writable, which OSS
 * reports at fragment granularity, then ask SNDCTL_DSP_GETOSPACE how
 * much fits; repeat until at least len bytes do, len capped at one
 * fragment. Every select() is bounded to two fragments' worth of time
 * and the loop to OSS_WAIT_WRITABLE_LAPS, so a device that stalls or
 * wakes without space returns 0 - a pass to skip - rather than holding
 * the audio thread. Returns the free space otherwise. */
static size_t oss_wait_writable(void *data, size_t len)
{
   oss_audio_t *ossaudio  = (oss_audio_t*)data;
   audio_buf_info info;
   int laps               = OSS_WAIT_WRITABLE_LAPS;

   for (;;)
   {
      fd_set wfds;
      struct timeval tv;
      int rc;
      size_t want   = len;
      long   wait_us = 100000;

      if (ioctl(ossaudio->fd, SNDCTL_DSP_GETOSPACE, &info) < 0)
         return 0;
      if (info.fragsize > 0 && want > (size_t)info.fragsize)
         want = (size_t)info.fragsize;
      if (info.bytes >= (int)want)
         return (size_t)info.bytes;

      /* Two fragments, in microseconds: a fragment is fragsize bytes
       * of 16-bit stereo at the device rate. Clamped so a strange
       * fragment size cannot make the bound useless either way. */
      if (ossaudio->rate > 0 && info.fragsize > 0)
         wait_us = (long)((2000000LL * info.fragsize / 4) / ossaudio->rate);
      if (wait_us < 20000)
         wait_us = 20000;
      if (wait_us > 200000)
         wait_us = 200000;
      tv.tv_sec  = 0;
      tv.tv_usec = wait_us;

      FD_ZERO(&wfds);
      FD_SET(ossaudio->fd, &wfds);
      rc = select(ossaudio->fd + 1, NULL, &wfds, NULL, &tv);
      if (rc < 0)
      {
         if (errno == EINTR)
            continue;
         return 0;
      }
      if (rc == 0)
         return 0;
      if (--laps < 0)
         return 0;
   }
}

static bool oss_stop(void *data)
{
   oss_audio_t *ossaudio  = (oss_audio_t*)data;

#if !defined(RETROFW)
   if (ioctl(ossaudio->fd, SNDCTL_DSP_RESET, 0) < 0)
      return false;
#endif

   ossaudio->is_paused = true;
   return true;
}

static bool oss_start(void *data, bool is_shutdown)
{
   oss_audio_t *ossaudio  = (oss_audio_t*)data;
   if (!ossaudio)
      return false;
   ossaudio->is_paused = false;
   return true;
}

static bool oss_alive(void *data)
{
   oss_audio_t *ossaudio  = (oss_audio_t*)data;
   return !ossaudio->is_paused;
}

static void oss_set_nonblock_state(void *data, bool state)
{
   int rc;
   oss_audio_t *ossaudio  = (oss_audio_t*)data;

   if (state)
      rc =  fcntl(ossaudio->fd, F_SETFL,
            fcntl(ossaudio->fd, F_GETFL) | O_NONBLOCK);
   else
      rc =  fcntl(ossaudio->fd, F_SETFL,
            fcntl(ossaudio->fd, F_GETFL) & (~O_NONBLOCK));
   if (rc != 0)
      RARCH_WARN("[OSS] Could not set nonblocking on OSS file descriptor. Will not be able to fast-forward.\n");
}

static void oss_free(void *data)
{
   oss_audio_t *ossaudio  = (oss_audio_t*)data;

/*RETROFW IOCTL always returns EINVAL*/
#if !defined(RETROFW)
   if (ioctl(ossaudio->fd, SNDCTL_DSP_RESET, 0) < 0)
      return;
#endif

   close(ossaudio->fd);
   free(data);
}

static size_t oss_write_avail(void *data)
{
   audio_buf_info info;
   oss_audio_t *ossaudio  = (oss_audio_t*)data;

   if (ioctl(ossaudio->fd, SNDCTL_DSP_GETOSPACE, &info) < 0)
   {
      RARCH_ERR("[OSS] SNDCTL_DSP_GETOSPACE failed.\n");
      return 0;
   }

   return info.bytes;
}

static size_t oss_buffer_size(void *data)
{
   audio_buf_info info;
   oss_audio_t *ossaudio  = (oss_audio_t*)data;

   if (ioctl(ossaudio->fd, SNDCTL_DSP_GETOSPACE, &info) < 0)
   {
      RARCH_ERR("[OSS] SNDCTL_DSP_GETOSPACE failed.\n");
      return 1; /* Return something non-zero to avoid SIGFPE. */
   }

   return info.fragsize * info.fragstotal;
}

static bool oss_use_float(void *data)
{
   return false;
}

/* The device string is a path. List the /dev/dsp nodes that exist:
 * /dev/dsp itself and /dev/dsp0..15, which covers Linux OSS, OSS4 and
 * the BSDs. Where SNDCTL_AUDIOINFO exists the card name is appended so
 * the entry is recognisable; the path stays the value. */
static void *oss_device_list_new(void *data)
{
   int i;
   union string_list_elem_attr attr;
   struct string_list *sl = string_list_new();

   (void)data;
   attr.i = 0;
   if (!sl)
      return NULL;

   for (i = -1; i < 16; i++)
   {
      char path[32];
      char label[160];
      int fd;

      if (i < 0)
         strlcpy(path, DEFAULT_OSS_DEV, sizeof(path));
      else
         snprintf(path, sizeof(path), "/dev/dsp%d", i);

      /* A node that cannot be opened for output is not offered; a busy
       * one is, since it exists and may be free later. */
      fd = open(path, O_WRONLY | O_NONBLOCK);
      if (fd < 0 && errno != EBUSY)
         continue;

      strlcpy(label, path, sizeof(label));
#ifdef SNDCTL_AUDIOINFO
      if (fd >= 0)
      {
         oss_audioinfo ai;
         memset(&ai, 0, sizeof(ai));
         ai.dev = -1;
         if (ioctl(fd, SNDCTL_AUDIOINFO, &ai) == 0 && ai.name[0])
            snprintf(label, sizeof(label), "%s (%s)", path, ai.name);
      }
#endif
      if (fd >= 0)
         close(fd);

      attr.i = i;
      string_list_append(sl, label, attr);
   }

   return sl;
}

static void oss_device_list_free(void *data, void *array_list_data)
{
   struct string_list *sl = (struct string_list*)array_list_data;
   (void)data;
   if (sl)
      string_list_free(sl);
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
   oss_device_list_new,
   oss_device_list_free,
   oss_write_avail,
   oss_buffer_size,
   NULL, /* write_raw */
   oss_wait_writable,
   oss_frames_consumed
};

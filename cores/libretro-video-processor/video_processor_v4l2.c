/*-
 * Copyright (c) 2016 Jared McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LIBRARY_NAME		"V4L2"
#define LIBRARY_VERSION		"0.0.2"
#define VIDEO_BUFFERS		2
#define AUDIO_SAMPLE_RATE	48000
#define AUDIO_BUFSIZE		64
#define ENVVAR_BUFLEN		1024

#include "libretro.h"

#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <linux/videodev2.h>
#include <libv4l2.h>

#ifdef RARCH_INTERNAL
#include "internal_cores.h"
#include "../../config.h"
#define VIDEOPROC_CORE_PREFIX(s) libretro_videoprocessor_##s
#else
#define VIDEOPROC_CORE_PREFIX(s) s
#endif

#ifdef HAVE_ALSA
#include <alsa/asoundlib.h>
#endif

#ifdef HAVE_UDEV
#include <libudev.h>
#endif

struct video_buffer {
   void   *start;
   size_t   len;
};

/*
 * Video capture state
 */
static int video_fd = -1;
static struct v4l2_format video_format;
static struct v4l2_standard video_standard;
static struct video_buffer video_buffer[VIDEO_BUFFERS];
static size_t video_nbuffers;
static uint16_t *conv_data;

/*
 * Audio capture state
 */
#ifdef HAVE_ALSA
static snd_pcm_t *audio_handle;
#endif

/*
 * Libretro API callbacks
 */
static retro_environment_t VIDEOPROC_CORE_PREFIX(environment_cb);
static retro_video_refresh_t VIDEOPROC_CORE_PREFIX(video_refresh_cb);
static retro_audio_sample_t VIDEOPROC_CORE_PREFIX(audio_sample_cb);
static retro_audio_sample_batch_t VIDEOPROC_CORE_PREFIX(audio_sample_batch_cb);
static retro_input_poll_t VIDEOPROC_CORE_PREFIX(input_poll_cb);
static retro_input_state_t VIDEOPROC_CORE_PREFIX(input_state_cb);

#ifdef HAVE_ALSA
static void audio_callback(void)
{
   int16_t audio_data[128];

   if (audio_handle)
   {
      const int frames = snd_pcm_readi(audio_handle,
            audio_data, sizeof(audio_data) / 4);

      if (frames < 0)
         snd_pcm_recover(audio_handle, frames, true);
      else
         VIDEOPROC_CORE_PREFIX(audio_sample_batch_cb)(audio_data, frames);
   }
}

static void
audio_set_state(bool enable)
{
}
#endif

static void
appendstr(char *dst, const char *src, size_t dstsize)
{
   size_t resid = dstsize - (strlen(dst) + 1);
   if (resid == 0)
      return;
   strncat(dst, src, resid);
}

static void
enumerate_video_devices(char *buf, size_t buflen)
{
   memset(buf, 0, buflen);

   appendstr(buf, "Video capture device; ", buflen);

#ifdef HAVE_UDEV
   /* Get a list of devices matching the "video4linux" subsystem from udev */
   int ndevs;
   struct udev_device *dev;
   struct udev_enumerate *enumerate;
   struct udev_list_entry *devices, *dev_list_entry;
   const char *path, *name;
   struct udev *udev = udev_new();

   if (!udev)
   {
      printf("Cannot create udev context\n");
      return;
   }

   enumerate = udev_enumerate_new(udev);
   if (!enumerate)
   {
      printf("Cannot create enumerate context\n");
      udev_unref(udev);
      return;
   }

   udev_enumerate_add_match_subsystem(enumerate, "video4linux");
   udev_enumerate_scan_devices(enumerate);

   devices = udev_enumerate_get_list_entry(enumerate);
   if (!devices)
   {
      printf("Cannot get video device list\n");
      udev_enumerate_unref(enumerate);
      udev_unref(udev);
      return;
   }

   ndevs = 0;
   udev_list_entry_foreach(dev_list_entry, devices)
   {
      path = udev_list_entry_get_name(dev_list_entry);
      dev = udev_device_new_from_syspath(udev, path);
      name = udev_device_get_devnode(dev);

      if (strncmp(name, "/dev/video", strlen("/dev/video")) == 0)
      {
         if (ndevs > 0)
            appendstr(buf, "|", buflen);
         appendstr(buf, name, buflen);
         ndevs++;
      }

      udev_device_unref(dev);
   }

   udev_enumerate_unref(enumerate);
   udev_unref(udev);
#else
   /* Just return a few options. We'll fail later if the device is not found. */
   appendstr(buf, "/dev/video0|/dev/video1|/dev/video2|/dev/video3", buflen);
#endif
}

static void
enumerate_audio_devices(char *buf, size_t buflen)
{
   memset(buf, 0, buflen);

   appendstr(buf, "Audio capture device; ", buflen);

#ifdef HAVE_ALSA
   void **hints, **n;
   char *ioid, *name;
   int ndevs;

   if (snd_device_name_hint(-1, "pcm", &hints) < 0)
      return;

   ndevs = 0;
   for (n = hints; *n; n++)
   {
      name = snd_device_name_get_hint(*n, "NAME");
      ioid = snd_device_name_get_hint(*n, "IOID");
      if ((ioid == NULL || !strcmp(ioid, "Input")) &&
          (!strncmp(name, "hw:", strlen("hw:")) || 
           !strncmp(name, "default:", strlen("default:"))))
      {
         if (ndevs > 0)
            appendstr(buf, "|", buflen);
         appendstr(buf, name, buflen);
         ++ndevs;
      }
      free(name);
      free(ioid);
   }

   snd_device_name_free_hint(hints);
#endif
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_set_environment)(retro_environment_t cb)
{
   char video_devices[ENVVAR_BUFLEN];
   char audio_devices[ENVVAR_BUFLEN];

   VIDEOPROC_CORE_PREFIX(environment_cb) = cb;

#ifdef HAVE_ALSA
   struct retro_audio_callback audio_cb;
   audio_cb.callback = audio_callback;
   audio_cb.set_state = audio_set_state;
   VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK, &audio_cb);
#endif

   enumerate_video_devices(video_devices, sizeof(video_devices));
   enumerate_audio_devices(audio_devices, sizeof(audio_devices));

   struct retro_variable envvars[] = {
      { "videoproc_videodev", video_devices },
      { "videoproc_audiodev", audio_devices },
      { NULL, NULL }
   };

   VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_SET_VARIABLES, envvars);
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_set_video_refresh)(retro_video_refresh_t cb)
{
   VIDEOPROC_CORE_PREFIX(video_refresh_cb) = cb;
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_set_audio_sample)(retro_audio_sample_t cb)
{
   VIDEOPROC_CORE_PREFIX(audio_sample_cb) = cb;
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_set_audio_sample_batch)(retro_audio_sample_batch_t cb)
{
   VIDEOPROC_CORE_PREFIX(audio_sample_batch_cb) = cb;
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_set_input_poll)(retro_input_poll_t cb)
{
   VIDEOPROC_CORE_PREFIX(input_poll_cb) = cb;
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_set_input_state)(retro_input_state_t cb)
{
   VIDEOPROC_CORE_PREFIX(input_state_cb) = cb;
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_init)(void)
{
}

static bool open_devices(void)
{
   struct retro_variable videodev = { "videoproc_videodev", NULL };
   struct retro_variable audiodev = { "videoproc_audiodev", NULL };
   struct v4l2_capability caps;
   int error;

   /* Get the video and audio capture device names from the environment */
   VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &videodev);
   VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &audiodev);

   /* Video device is required */
   if (videodev.value == NULL)
   {
      printf("v4l2_videodev not defined\n");
      return false;
   }

   /* Open the V4L2 device */
   video_fd = v4l2_open(videodev.value, O_RDWR, 0);
   if (video_fd == -1)
   {
      printf("Couldn't open %s: %s\n", videodev.value, strerror(errno));
      return false;
   }

   /* Query V4L2 device capabilities */
   error = v4l2_ioctl(video_fd, VIDIOC_QUERYCAP, &caps);
   if (error != 0)
   {
      printf("VIDIOC_QUERYCAP failed: %s\n", strerror(errno));
      v4l2_close(video_fd);
      return false;
   }

   printf("%s:\n", videodev.value);
   printf("  Driver: %s\n", caps.driver);
   printf("  Card: %s\n", caps.card);
   printf("  Bus Info: %s\n", caps.bus_info);
   printf("  Version: %u.%u.%u\n", (caps.version >> 16) & 0xff,
         (caps.version >> 8) & 0xff, caps.version & 0xff);

#ifdef HAVE_ALSA
   if (audiodev.value)
   {
      snd_pcm_hw_params_t *hw_params;
      unsigned int rate;

      /*
       * Open the audio capture device and configure it for 48kHz, 16-bit stereo
       */
      error = snd_pcm_open(&audio_handle, audiodev.value, SND_PCM_STREAM_CAPTURE, 0);
      if (error < 0)
      {
         printf("Couldn't open %s: %s\n", audiodev.value, snd_strerror(error));
         return false;
      }

      error = snd_pcm_hw_params_malloc(&hw_params);
      if (error)
      {
         printf("Couldn't allocate hw param structure: %s\n", snd_strerror(error));
         return false;
      }
      error = snd_pcm_hw_params_any(audio_handle, hw_params);
      if (error)
      {
         printf("Couldn't initialize hw param structure: %s\n", snd_strerror(error));
         return false;
      }
      error = snd_pcm_hw_params_set_access(audio_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
      if (error)
      {
         printf("Couldn't set hw param access type: %s\n", snd_strerror(error));
         return false;
      }
      error = snd_pcm_hw_params_set_format(audio_handle, hw_params, SND_PCM_FORMAT_S16_LE);
      if (error)
      {   
         printf("Couldn't set hw param format to SND_PCM_FORMAT_S16_LE: %s\n", snd_strerror(error));
         return false;
      }
      rate = AUDIO_SAMPLE_RATE;
      error = snd_pcm_hw_params_set_rate_near(audio_handle, hw_params, &rate, 0);
      if (error)
      {
         printf("Couldn't set hw param sample rate to %u: %s\n", rate, snd_strerror(error));
         return false;
      }
      if (rate != AUDIO_SAMPLE_RATE)
      {
         printf("Hardware doesn't support sample rate %u (returned %u)\n", AUDIO_SAMPLE_RATE, rate);
         return false;
      }
      error = snd_pcm_hw_params_set_channels(audio_handle, hw_params, 2);
      if (error)
      {
         printf("Couldn't set hw param channels to 2: %s\n", snd_strerror(error));
         return false;
      }
      error = snd_pcm_hw_params(audio_handle, hw_params);
      if (error)
      {
         printf("Couldn't set hw params: %s\n", snd_strerror(error));
         return false;
      }
      snd_pcm_hw_params_free(hw_params);

      error = snd_pcm_prepare(audio_handle);
      if (error)
      {
         printf("Couldn't prepare audio interface for use: %s\n", snd_strerror(error));
         return false;
      }

      printf("Using ALSA device %s for audio input\n", audiodev.value);
   }
#endif

   return true;
}

static void close_devices(void)
{
#ifdef HAVE_ALSA
   if (audio_handle)
   {
      snd_pcm_close(audio_handle);
      audio_handle = NULL;
   }
#endif

   if (video_fd != -1)
   {
      v4l2_close(video_fd);
      video_fd = -1;
   }
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_deinit)(void)
{
   close_devices();
}

RETRO_API unsigned VIDEOPROC_CORE_PREFIX(retro_api_version)(void)
{
   return RETRO_API_VERSION;
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_get_system_info)(struct retro_system_info *info)
{
   info->library_name = LIBRARY_NAME;
   info->library_version = LIBRARY_VERSION;
   info->valid_extensions = NULL; /* Anything is fine, we don't care. */
   info->need_fullpath    = false;
   info->block_extract    = true;
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_get_system_av_info)(struct retro_system_av_info *info)
{
   struct v4l2_cropcap cc;
   int error;

   /*
    * Query the device cropping limits. If available, we can use this to find the capture pixel aspect.
    */
   memset(&cc, 0, sizeof(cc));
   cc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   error = v4l2_ioctl(video_fd, VIDIOC_CROPCAP, &cc);
   if (error == 0)
      info->geometry.aspect_ratio = (double)cc.pixelaspect.denominator 
         / (double)cc.pixelaspect.numerator;

   info->geometry.base_width  = info->geometry.max_width = video_format.fmt.pix.width;
   info->geometry.base_height = info->geometry.max_height = video_format.fmt.pix.height;
   info->timing.fps           = (double)video_standard.frameperiod.denominator / 
      (double)video_standard.frameperiod.numerator;
   info->timing.sample_rate   = AUDIO_SAMPLE_RATE;

   printf("Resolution %ux%u %f fps\n", info->geometry.base_width,
         info->geometry.base_height, info->timing.fps);
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_set_controller_port_device)(unsigned port, unsigned device)
{
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_reset)(void)
{
   close_devices();
   open_devices();
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_run)(void)
{
   struct v4l2_buffer buf;
   uint8_t *src;
   uint16_t *dst;
   int i, error;

   VIDEOPROC_CORE_PREFIX(input_poll_cb)();

   if (video_fd == -1)
      return;

   memset(&buf, 0, sizeof(buf));
   buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   buf.memory = V4L2_MEMORY_MMAP;

   error = v4l2_ioctl(video_fd, VIDIOC_DQBUF, &buf);
   if (error != 0)
   {
      printf("VIDIOC_DQBUF failed: %s\n", strerror(errno));
      VIDEOPROC_CORE_PREFIX(video_refresh_cb)(NULL, 0, 0, 0);
      return;
   }

   src = (uint8_t*)video_buffer[buf.index].start;
   dst = (uint16_t*)conv_data;

   /* RGB24 to RGB565 */
   for (i = 0; i < video_format.fmt.pix.width * video_format.fmt.pix.height; i++, src += 3, dst += 1)
      *dst = ((src[0] >> 3) << 11) | ((src[1] >> 2) << 5) | ((src[2] >> 3) << 0);

   error = v4l2_ioctl(video_fd, VIDIOC_QBUF, &buf);
   if (error != 0)
      printf("VIDIOC_QBUF failed: %s\n", strerror(errno));

   VIDEOPROC_CORE_PREFIX(video_refresh_cb)(conv_data, video_format.fmt.pix.width,
         video_format.fmt.pix.height, video_format.fmt.pix.width * 2);
}

RETRO_API size_t VIDEOPROC_CORE_PREFIX(retro_serialize_size)(void)
{
   return 0;
}

RETRO_API bool VIDEOPROC_CORE_PREFIX(retro_serialize)(void *data, size_t size)
{
   return false;
}

RETRO_API bool VIDEOPROC_CORE_PREFIX(retro_unserialize)(const void *data, size_t size)
{
   return false;
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_cheat_reset)(void)
{
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_cheat_set)(unsigned index, bool enabled, const char *code)
{
}

RETRO_API bool VIDEOPROC_CORE_PREFIX(retro_load_game)(const struct retro_game_info *game)
{
   enum retro_pixel_format pixel_format;
   struct v4l2_standard std;
   struct v4l2_requestbuffers reqbufs;
   struct v4l2_buffer buf;
   struct v4l2_format fmt;
   enum v4l2_buf_type type;
   v4l2_std_id std_id;
   uint32_t index;
   bool std_found;
   int error;

   if (open_devices() == false)
   {
      printf("Couldn't open capture device\n");
      close_devices();
      return false;
   }

   memset(&fmt, 0, sizeof(fmt));
   fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

   error = v4l2_ioctl(video_fd, VIDIOC_G_FMT, &fmt);

   if (error != 0)
   {
      printf("VIDIOC_G_FMT failed: %s\n", strerror(errno));
      return false;
   }

   fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
   error = v4l2_ioctl(video_fd, VIDIOC_S_FMT, &fmt);
   if (error != 0)
   {
      printf("VIDIOC_S_FMT failed: %s\n", strerror(errno));
      return false;
   }

   error = v4l2_ioctl(video_fd, VIDIOC_G_STD, &std_id);
   if (error != 0)
   {
      printf("VIDIOC_G_STD failed: %s\n", strerror(errno));
      return false;
   }
   for (index = 0, std_found = false; ; index++)
   {
      memset(&std, 0, sizeof(std));
      std.index = index;
      error = v4l2_ioctl(video_fd, VIDIOC_ENUMSTD, &std);
      if (error)
         break;
      if (std.id == std_id)
      {
         video_standard = std;
         std_found = true;
      }
      printf("VIDIOC_ENUMSTD[%u]: %s%s\n", index, std.name, std.id == std_id ? " [*]" : "");
   }
   if (!std_found)
   {
      printf("VIDIOC_ENUMSTD did not contain std ID %08x\n", (unsigned)std_id);
      return false;
   }

   video_format = fmt;

   memset(&reqbufs, 0, sizeof(reqbufs));
   reqbufs.count = VIDEO_BUFFERS;
   reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   reqbufs.memory = V4L2_MEMORY_MMAP;

   error = v4l2_ioctl(video_fd, VIDIOC_REQBUFS, &reqbufs);
   if (error != 0)
   {
      printf("VIDIOC_REQBUFS failed: %s\n", strerror(errno));
      return false;
   }
   video_nbuffers = reqbufs.count;

   for (index = 0; index < video_nbuffers; index++)
   {
      memset(&buf, 0, sizeof(buf));
      buf.index = index;
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;

      error = v4l2_ioctl(video_fd, VIDIOC_QUERYBUF, &buf);
      if (error != 0)
      {
         printf("VIDIOC_QUERYBUF failed for %u: %s\n", index, strerror(errno));
         return false;
      }

      video_buffer[index].len = buf.length;
      video_buffer[index].start = v4l2_mmap(NULL, buf.length,
            PROT_READ|PROT_WRITE, MAP_SHARED, video_fd, buf.m.offset);
      if (video_buffer[index].start == MAP_FAILED)
      {
         printf("v4l2_mmap failed: %s\n", strerror(errno));
         return false;
      }
   }

   for (index = 0; index < video_nbuffers; index++)
   {
      memset(&buf, 0, sizeof(buf));
      buf.index = index;
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;
      
      error = v4l2_ioctl(video_fd, VIDIOC_QBUF, &buf);
      if (error != 0)
      {
         printf("VIDIOC_QBUF failed for %u: %s\n", index, strerror(errno));
         return false;
      }
   }

   conv_data = (uint16_t*)calloc(1, 
         video_format.fmt.pix.width * video_format.fmt.pix.height * 2);   
   if (!conv_data)
   {
      printf("Cannot allocate conversion buffer\n");
      return false;
   }

   printf("Allocated %u byte conversion buffer\n",
         video_format.fmt.pix.width * video_format.fmt.pix.height * 2);

   pixel_format = RETRO_PIXEL_FORMAT_RGB565;
   if (!VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &pixel_format))
   {
      printf("Cannot set pixel format\n");
      return false;
   }

   type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   error = v4l2_ioctl(video_fd, VIDIOC_STREAMON, &type);
   if (error != 0)
   {
      printf("VIDIOC_STREAMON failed: %s\n", strerror(errno));
      return false;
   }

   return true;
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_unload_game)(void)
{

   if (video_fd != -1)
   {
      uint32_t index;
      enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      int               error = v4l2_ioctl(video_fd, VIDIOC_STREAMOFF, &type);
      if (error != 0)
         printf("VIDIOC_STREAMOFF failed: %s\n", strerror(errno));

      for (index = 0; index < video_nbuffers; index++)
         v4l2_munmap(video_buffer[index].start, video_buffer[index].len);
   }
   
   if (conv_data)
      free(conv_data);
   conv_data = NULL;

   close_devices();
}

RETRO_API bool VIDEOPROC_CORE_PREFIX(retro_load_game_special)(unsigned game_type,
      const struct retro_game_info *info, size_t num_info)
{
   return false;
}

RETRO_API unsigned VIDEOPROC_CORE_PREFIX(retro_get_region)(void)
{
   return (video_standard.id & V4L2_STD_NTSC) != 0 ? RETRO_REGION_NTSC : RETRO_REGION_PAL;
}

RETRO_API void *VIDEOPROC_CORE_PREFIX(retro_get_memory_data)(unsigned id)
{
   return NULL;
}

RETRO_API size_t VIDEOPROC_CORE_PREFIX(retro_get_memory_size)(unsigned id)
{
   return 0;
}

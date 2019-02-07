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
#define VIDEO_BUFFERS_MAX   2
#define AUDIO_SAMPLE_RATE	48000
#define AUDIO_BUFSIZE		64
#define ENVVAR_BUFLEN		1024

#include <libretro.h>
#include <clamping.h>
#include <retro_miscellaneous.h>

#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

#include <string/stdstring.h>

struct v4l2_capbuf
{
   void   *start;
   size_t   len;
};

/*
 * Video capture state
 */
static int  video_device_fd = -1;
static char video_device[ENVVAR_BUFLEN];
static char audio_device[ENVVAR_BUFLEN];

static struct  v4l2_format video_format;
static struct  v4l2_standard video_standard;
static struct  v4l2_buffer video_buf;

static uint8_t v4l2_ncapbuf_target;
static size_t  v4l2_ncapbuf;
static struct  v4l2_capbuf v4l2_capbuf[VIDEO_BUFFERS_MAX];

static float   dummy_pos=0;

static int      video_half_feed_rate=0; /* for interlaced captures */
static uint32_t video_cap_width;
static uint32_t video_cap_height;
static uint32_t video_out_height;
static char     video_capture_mode[ENVVAR_BUFLEN];
static char     video_output_mode[ENVVAR_BUFLEN];
static char     video_frame_times[ENVVAR_BUFLEN];

static uint8_t  *frame_cap;
static uint32_t *frame_out;
static uint32_t *frames[4];
static uint32_t *frame_prev1;
static uint32_t *frame_prev2;
static uint32_t *frame_prev3;
static uint32_t *frame_curr;

/* Frametime debug messages */
struct timeval ft_prevtime = { 0 }, ft_prevtime2 = { 0 };
char *ft_info = NULL, *ft_info2 = NULL;
double ft_favg, ft_ftime;
int ft_fcount;

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
#ifdef HAVE_UDEV
   int ndevs;
   struct udev_device *dev;
   struct udev_enumerate *enumerate;
   struct udev_list_entry *devices, *dev_list_entry;
   const char *path, *name;
   struct udev *udev = NULL;
#endif

   memset(buf, 0, buflen);

   appendstr(buf, "Video capture device; ", buflen);

#ifdef HAVE_UDEV
   /* Get a list of devices matching the "video4linux" subsystem from udev */
   udev = udev_new();

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
      if ((ioid == NULL || string_is_equal(ioid, "Input")) &&
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
   struct retro_variable envvars[] = {
      { "videoproc_videodev", NULL },
      { "videoproc_audiodev", NULL },
      { "videoproc_capture_mode", "Capture mode; alternate|interlaced|top|bottom|alternate_hack" },
      { "videoproc_output_mode","Output mode; progressive|deinterlaced|interlaced" },
      { "videoproc_frame_times","Print frame times to terminal (v4l2 only); Off|On" },
      { NULL, NULL }
   };

   VIDEOPROC_CORE_PREFIX(environment_cb) = cb;

   /* Allows retroarch to seed the previous values */
   VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_SET_VARIABLES, envvars);

   /* Enumerate all real devices */
   enumerate_video_devices(video_devices, sizeof(video_devices));
   enumerate_audio_devices(audio_devices, sizeof(audio_devices));

   /* Add the dummy device */
   appendstr(video_devices, "|dummy", ENVVAR_BUFLEN);

   /* Registers available devices list (still respects saved device if it exists) */
   envvars[0].key   = "videoproc_videodev";
   envvars[0].value = video_devices;
   envvars[1].key   = "videoproc_audiodev";
   envvars[1].value = audio_devices;

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

   if (strcmp(videodev.value, "dummy") == 0) {
      return true;
   }

   /* Video device is required */
   if (videodev.value == NULL)
   {
      printf("v4l2_videodev not defined\n");
      return false;
   }

   /* Open the V4L2 device */
   video_device_fd = v4l2_open(videodev.value, O_RDWR, 0);
   if (video_device_fd == -1)
   {
      printf("Couldn't open %s: %s\n", videodev.value, strerror(errno));
      return false;
   }

   /* Query V4L2 device capabilities */
   error = v4l2_ioctl(video_device_fd, VIDIOC_QUERYCAP, &caps);
   if (error != 0)
   {
      printf("VIDIOC_QUERYCAP failed: %s\n", strerror(errno));
      v4l2_close(video_device_fd);
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
       * Open the audio capture device and configure it for 44kHz, 16-bit stereo
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

   if (video_device_fd != -1)
   {
      v4l2_close(video_device_fd);
      video_device_fd = -1;
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
   struct retro_variable videodev = { "videoproc_videodev", NULL };
   struct v4l2_cropcap cc;
   int error;

   VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &videodev);

   if (strcmp(videodev.value, "dummy") == 0) {
      info->geometry.aspect_ratio = 4.0/3.0;
      info->geometry.base_width  = info->geometry.max_width = video_cap_width;
      info->geometry.base_height = video_cap_height; /* out? */
      info->geometry.max_height  = video_out_height;
      info->timing.fps           = 60;
      info->timing.sample_rate   = AUDIO_SAMPLE_RATE;
   } else {
       /*
        * Query the device cropping limits. If available, we can use this to find the capture pixel aspect.
        */
       memset(&cc, 0, sizeof(cc));
       cc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
       error = v4l2_ioctl(video_device_fd, VIDIOC_CROPCAP, &cc);

       info->geometry.base_width  = info->geometry.max_width = video_format.fmt.pix.width;
       info->geometry.base_height = video_format.fmt.pix.height;
       /* TODO Only double if frames are NOT fields (interlaced, full resolution) */
       info->geometry.max_height = video_format.fmt.pix.height * 2;
       /* TODO Only double if frames ARE fields (progressive or deinterlaced, full framerate)
        * *2 for fields
        */
       info->timing.fps           = ((double)(video_standard.frameperiod.denominator*2)) /
                                    (double)video_standard.frameperiod.numerator;
       info->timing.sample_rate   = AUDIO_SAMPLE_RATE;

       if (error == 0) {
          /* TODO Allow for fixed 4:3 and 16:9 modes */
          info->geometry.aspect_ratio = (double)info->geometry.base_width / (double)info->geometry.max_height /\
                                         ((double)cc.pixelaspect.numerator / (double)cc.pixelaspect.denominator);
       }
   }

   printf("Aspect ratio: %f\n",info->geometry.aspect_ratio);
   printf("Buffer Resolution %ux%u %f fps\n", info->geometry.base_width,
             info->geometry.base_height, info->timing.fps);
   printf("Buffer Max Resolution %ux%u\n", info->geometry.max_width,
             info->geometry.max_height);
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_set_controller_port_device)(unsigned port, unsigned device)
{
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_reset)(void)
{
   close_devices();
   open_devices();
}

/* TODO improve this mess and make it generic enough for use with dummy mode */
void v4l2_frame_times(struct v4l2_buffer buf) {
   if (strcmp("Off", video_frame_times) == 0)
       return;

   if (ft_info == NULL)
       ft_info = (char*)calloc(5000, sizeof(char));

   if ( (buf.timestamp.tv_sec - ft_prevtime.tv_sec >= 1) && \
        (buf.timestamp.tv_usec + 1000000 - ft_prevtime2.tv_usec) >= 1000000) {
       double csec = ((double) buf.timestamp.tv_sec) + (buf.timestamp.tv_usec/1000000);
       double psec = ((double) ft_prevtime.tv_sec) + (ft_prevtime.tv_usec/1000000);
       printf("Fields last %.2f seconds: %d\n", csec - psec, ft_fcount);
       printf("Average frame times: %.3fms\n", ft_favg/(1000*ft_fcount));
       printf("Fields timestampdiffs last second:\n%s\n", ft_info);
       free(ft_info);
       ft_info = (char*)calloc(5000, sizeof(char));
       ft_fcount = 0;
       ft_favg = 0;
       ft_prevtime = buf.timestamp;
   }
   ft_fcount++;
   ft_info2 = strdup(ft_info);
   ft_ftime = (double) (buf.timestamp.tv_usec + ((buf.timestamp.tv_sec - ft_prevtime2.tv_sec >= 1) ? 1000000 : 0)  - ft_prevtime2.tv_usec);
   ft_favg += ft_ftime;
   snprintf(ft_info, 5000 * sizeof(char), "%s %6.d %d %d %.2fms%s", ft_info2, buf.sequence, buf.index, buf.field, ft_ftime/1000, (!(ft_fcount % 7)) ? "\n" : "");
   free(ft_info2);

   ft_prevtime2 = buf.timestamp;
}

void source_dummy(int width, int height) {
   int i, triangpos, triangpos_t=0, triangpos_b=0, offset=0;
   bool field_ahead = false;
   uint8_t *src = frame_cap;
   float step = M_PI/64;

   if (video_buf.field == V4L2_FIELD_TOP) {
      offset=0;
   } else if (video_buf.field == V4L2_FIELD_BOTTOM) {
      offset=1;
   }

   dummy_pos += step;
   /* no animation */
   /* dummy_pos = M_PI/4; step = 0; */
   triangpos = (sinf(dummy_pos)+1)/2*width;
   if (video_buf.field == V4L2_FIELD_INTERLACED) {
      if (video_half_feed_rate == 0)
          video_half_feed_rate = 1;
      triangpos_t = (sinf(dummy_pos)+1)/2*width;
      dummy_pos += step;
      triangpos_b = (sinf(dummy_pos)+1)/2*width;
   }

   for (i = 0; i < width * height; i+=1, src+=3) {
      float color = (clamp_float((float)(triangpos - (i%width) + offset)/10, -1, 1)+1)/2;
      src[0] = 0x10 + color*0xE0;
      src[1] = 0x10 + color*0xE0;
      src[2] = 0x10 + color*0xE0;

      /* End of a line */
      if ( ((i+1) % width) == 0 ) {
         triangpos -= 2; /* offset should be half of this? */
         triangpos_t -= 1;
         triangpos_b -= 1;
         if (video_buf.field == V4L2_FIELD_INTERLACED) {
            if (field_ahead) {
               triangpos = triangpos_b;
               field_ahead = false;
            } else {
               triangpos = triangpos_t;
               field_ahead = true;
            }
         }
      }
   }

   if(video_buf.field == V4L2_FIELD_TOP)
      video_buf.field = V4L2_FIELD_BOTTOM;
   else if (video_buf.field == V4L2_FIELD_BOTTOM)
      video_buf.field = V4L2_FIELD_TOP;
}

void source_v4l2_normal(int width, int height) {
   struct v4l2_buffer bufcp;

   int error;

   /* Wait until v4l2 dequees a buffer */
   memset(&video_buf, 0, sizeof(struct v4l2_buffer));
   video_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   video_buf.memory = V4L2_MEMORY_MMAP;
   error = v4l2_ioctl(video_device_fd, VIDIOC_DQBUF, &video_buf);
   if (error != 0)
   {
      printf("VIDIOC_DQBUF failed: %s\n", strerror(errno));
      VIDEOPROC_CORE_PREFIX(video_refresh_cb)(NULL, 0, 0, 0);
      return;
   }

   bufcp = video_buf;
   memcpy( (uint32_t*) frame_cap, (uint8_t*) v4l2_capbuf[video_buf.index].start, video_format.fmt.pix.width * video_format.fmt.pix.height * 3);

   error = v4l2_ioctl(video_device_fd, VIDIOC_QBUF, &video_buf);
   if (error != 0)
      printf("VIDIOC_QBUF failed: %s\n", strerror(errno));

   v4l2_frame_times(bufcp);
}

void source_v4l2_alternate_hack(int width, int height) {
   struct v4l2_buffer bufcp;
   struct v4l2_format fmt;
   struct v4l2_requestbuffers reqbufs;
   enum v4l2_buf_type type;

   int error;
   uint32_t index;

   /* For later, saving time */
   memset(&fmt, 0, sizeof(fmt));
   fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   error = v4l2_ioctl(video_device_fd, VIDIOC_G_FMT, &fmt);
   if (error != 0)
   {
      printf("VIDIOC_G_FMT failed: %s\n", strerror(errno));
   }

   /* Wait until v4l2 dequees a buffer */
   memset(&video_buf, 0, sizeof(struct v4l2_buffer));
   video_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   video_buf.memory = V4L2_MEMORY_MMAP;
   error = v4l2_ioctl(video_device_fd, VIDIOC_DQBUF, &video_buf);
   if (error != 0)
   {
      printf("VIDIOC_DQBUF failed: %s\n", strerror(errno));
      VIDEOPROC_CORE_PREFIX(video_refresh_cb)(NULL, 0, 0, 0);
      return;
   }

   type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   v4l2_ioctl(video_device_fd, VIDIOC_STREAMOFF, &type);

   /* Let's get the data as fast as possible! */
   bufcp = video_buf;
   memcpy( (uint32_t*) frame_cap, (uint8_t*) v4l2_capbuf[video_buf.index].start, video_format.fmt.pix.width * video_format.fmt.pix.height * 3);

   v4l2_munmap(v4l2_capbuf[0].start, v4l2_capbuf[0].len);

   if (video_buf.field == V4L2_FIELD_TOP)
       fmt.fmt.pix.field = V4L2_FIELD_BOTTOM;
   else
       fmt.fmt.pix.field = V4L2_FIELD_TOP;

   error = v4l2_ioctl(video_device_fd, VIDIOC_S_FMT, &fmt);
   if (error != 0)
   {
      printf("VIDIOC_S_FMT failed: %s\n", strerror(errno));
   }

   video_format = fmt;

   memset(&reqbufs, 0, sizeof(reqbufs));
   reqbufs.count = v4l2_ncapbuf_target;
   reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   reqbufs.memory = V4L2_MEMORY_MMAP;

   error = v4l2_ioctl(video_device_fd, VIDIOC_REQBUFS, &reqbufs);
   if (error != 0)
   {
      printf("VIDIOC_REQBUFS failed: %s\n", strerror(errno));
   }
   v4l2_ncapbuf = reqbufs.count;
   /* printf("GOT v4l2_ncapbuf=%ld\n", v4l2_ncapbuf); */

   index = 0;
   memset(&video_buf, 0, sizeof(struct v4l2_buffer));
   video_buf.index = index;
   video_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   video_buf.memory = V4L2_MEMORY_MMAP;

   v4l2_ioctl(video_device_fd, VIDIOC_QUERYBUF, &video_buf);
   v4l2_capbuf[index].len = video_buf.length;
   v4l2_capbuf[index].start = v4l2_mmap(NULL, video_buf.length,
         PROT_READ|PROT_WRITE, MAP_SHARED, video_device_fd, video_buf.m.offset);
   memset(&video_buf, 0, sizeof(struct v4l2_buffer));

   video_buf.index = index;
   video_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   video_buf.memory = V4L2_MEMORY_MMAP;

   v4l2_ioctl(video_device_fd, VIDIOC_QBUF,& video_buf);

#if 0
   error = v4l2_ioctl(video_device_fd, VIDIOC_QBUF, &video_buf);
   if (error != 0)
      printf("VIDIOC_QBUF failed: %s\n", strerror(errno));
#endif

   type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   error = v4l2_ioctl(video_device_fd, VIDIOC_STREAMON, &type);
   if (error != 0)
   {
      printf("VIDIOC_STREAMON failed: %s\n", strerror(errno));
   }

   v4l2_frame_times(bufcp);
}

void processing_heal(uint8_t *src, int width, int height) {
   uint32_t *fp1 = frame_prev1;
   int i;
   for (i = 0; i < width * height; i+=1, src += 3, ++fp1) {
      /* Tries to filter a bunch of blanked out scanline sections my capture cards spits out with this crazy hack
       * Since the blanked out scanlines are set to a black value bellow anything that can be captued, it's quite
       * easy to select the scanlines...
       */
      if (src[0] <= 0 && src[1] <= 0 && src[2] <= 0 && i >= width && i <= width * height - width) {
         if (*(src + 0 - width*3) >= ((*fp1>> 0&0xFF)-6) && \
             *(src + 1 - width*3) >= ((*fp1>> 8&0xFF)-6) && \
             *(src + 2 - width*3) >= ((*fp1>>16&0xFF)-6) && \
             *(src + 0 - width*3) <= ((*fp1>> 0&0xFF)+6) && \
             *(src + 1 - width*3) <= ((*fp1>> 8&0xFF)+6) && \
             *(src + 2 - width*3) <= ((*fp1>>16&0xFF)+6)) {
            src[0] = (*fp1>> 0&0xFF);
            src[1] = (*fp1>> 8&0xFF);
            src[2] = (*fp1>>16&0xFF);
         } else {
            src[0] = (*(fp1+i+width)>> 0&0xFF);
            src[1] = (*(fp1+i+width)>> 8&0xFF);
            src[2] = (*(fp1+i+width)>>16&0xFF);
         }
      }
   }
}

void processing_deinterlacing_crap(uint32_t *src, uint32_t *dst, int width, int height, enum v4l2_field field, int skip_lines_src) {
   int i, targetrow=0;
   uint32_t pixacul=0;
   uint32_t *fp1 = frame_prev1, *fp2 = frame_prev2, *fp3 = frame_prev3;

   /* Helps pointing to the pixel in the adjacent row */
   if (field == V4L2_FIELD_TOP)
      targetrow = width;
   else
      targetrow = width*-1;

   /* Starts from the even scanline if frame contains bottom fields
    * On progressive sources, should only skip the destination lines, since all lines the source are the same fields
    * On interlaced sources, should skip both the source and the destination lines, since only half the lines in the source are the same fields (must also skip fields later)
    */
   if (field == V4L2_FIELD_BOTTOM) {
      dst += width;
      if (skip_lines_src == 1) {
         src += width;
         fp1 += width;
         fp2 += width;
         fp3 += width;
      }
   }

   for (i = 0; i < width * height; i+=1, src += 1, dst += 1, ++fp1, ++fp2, ++fp3) {
      /* Will fill the current destination line with current field
       * The masking is used to prserve some information set by the
       * deinterlacing process, uses the alpha channel to tell if a
       * pixel needs further processing...
       */
      *(dst) =  (*(src) & 0x00FFFFFF) | (*dst & 0xFF000000);

      /* Crappy deinterlacing */
      if (i >= width && i <= (width*height-width)) {
         pixacul=((((*(dst)>> 0&0xFF) + (pixacul>> 0&0xFF))>>1<<0 |\
                   ((*(dst)>> 8&0xFF) + (pixacul>> 8&0xFF))>>1<<8 |\
                   ((*(dst)>>16&0xFF) + (pixacul>>16&0xFF))>>1<<16) \
                   & 0x00FFFFFF) | 0xFE000000;

         if ( ((*(dst          ) & 0xFF000000) == 0xFE000000) ||\
              ((*(dst+targetrow) & 0xFF000000) == 0xFE000000) )  {
            *dst = pixacul | 0xFF000000;
            *(dst+targetrow) = pixacul | 0xFF000000;
         } else {
            if (!(((*(src+0)>> 0&0xFF) >= ((*(fp2+0)>> 0&0xFF)-9) ) &&\
                  ((*(src+0)>> 8&0xFF) >= ((*(fp2+0)>> 8&0xFF)-9) ) &&\
                  ((*(src+0)>>16&0xFF) >= ((*(fp2+0)>>16&0xFF)-9) ) &&\
                  ((*(src+0)>> 0&0xFF) <= ((*(fp2+0)>> 0&0xFF)+9) ) &&\
                  ((*(src+0)>> 8&0xFF) <= ((*(fp2+0)>> 8&0xFF)+9) ) &&\
                  ((*(src+0)>>16&0xFF) <= ((*(fp2+0)>>16&0xFF)+9) )) ) {
               *(dst+targetrow) = pixacul;
            } else if (!(((*fp3>> 0&0xFF) >= ((*fp1>> 0&0xFF)-9) ) &&\
                         ((*fp3>> 8&0xFF) >= ((*fp1>> 8&0xFF)-9) ) &&\
                         ((*fp3>>16&0xFF) >= ((*fp1>>16&0xFF)-9) ) &&\
                         ((*fp3>> 0&0xFF) <= ((*fp1>> 0&0xFF)+9) ) &&\
                         ((*fp3>> 8&0xFF) <= ((*fp1>> 8&0xFF)+9) ) &&\
                         ((*fp3>>16&0xFF) <= ((*fp1>>16&0xFF)+9) ))) {
               *(dst+targetrow) = pixacul;
            }
         }
      }

      /* Skips a scanline if we reach the end of the current one
       * On progressive sources, should only skip the destination lines,
       * On interlaced sources, should skip both the source and the destination lines
       */
      if ( ((i+1) % width) == 0 ) {
         dst += width;
         if (skip_lines_src == 1) {
            src += width;
            fp1 += width;
            fp2 += width;
            fp3 += width;
         }
      }
   }
}

void processing_bgr_xrgb(uint8_t *src, uint32_t *dst, int width, int height) {
   /* BGR24 to XRGB8888 conversion */
   int i;
   for (i = 0; i < width * height; i+=1, src += 3, dst += 1) {
      *dst = 0xFF << 24 | src[2] << 16 | src[1] << 8 | src[0] << 0;
   }
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_run)(void)
{
   uint32_t *aux;
   struct retro_variable videodev    = { "videoproc_videodev", NULL };
   struct retro_variable audiodev    = { "videoproc_audiodev", NULL };
   struct retro_variable capturemode = { "videoproc_capture_mode", NULL };
   struct retro_variable outputmode  = { "videoproc_output_mode", NULL };
   struct retro_variable frametimes  = { "videoproc_frame_times", NULL };
   bool updated = false;

   if (VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) {
      VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &videodev);
      VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &audiodev);
      VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &capturemode);
      VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &outputmode);
      VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &frametimes);
      /* Video or Audio device(s) has(ve) been changed
       * TODO We may get away without reseting devices when changing output mode...
       */
      if ((videodev.value    && (strcmp(video_device, videodev.value) != 0)) ||\
          (audiodev.value    && (strcmp(audio_device, audiodev.value) != 0)) ||\
          (capturemode.value && (strcmp(video_capture_mode, capturemode.value) != 0)) ||\
          (outputmode.value  && (strcmp(video_output_mode,  outputmode.value)  != 0))) {
          VIDEOPROC_CORE_PREFIX(retro_unload_game)();
          /* This core does not cares for the retro_game_info * argument? */
          VIDEOPROC_CORE_PREFIX(retro_load_game)(NULL);
      }

      if (frametimes.value != NULL) {
         strncpy(video_frame_times, frametimes.value, ENVVAR_BUFLEN-1);
      }
   }

   VIDEOPROC_CORE_PREFIX(input_poll_cb)();

   /* printf("%d %d %d %s\n", video_cap_width, video_cap_height, video_buf.field, video_output_mode);
    * TODO pass frame_curr to source_* functions
    * half_feed_rate allows interlaced intput to be fed at half the calls to this function
    * where the same frame is then read by the deinterlacer twice, for each field
    */
   if (video_half_feed_rate == 0) {
      /* Capture */
      if (strcmp(video_device, "dummy") == 0) {
         source_dummy(video_cap_width, video_cap_height);
      } else {
         if (strcmp(video_capture_mode, "alternate_hack") == 0) {
            source_v4l2_alternate_hack(video_cap_width, video_cap_height);
            processing_heal(frame_cap, video_cap_width, video_cap_height);
         } else {
            source_v4l2_normal(video_cap_width, video_cap_height);
         }
      }

      if (video_buf.field == V4L2_FIELD_INTERLACED)
         video_half_feed_rate = 1;
   } else {
      video_half_feed_rate = 0;
   }

   /* Converts from bgr to xrgb, deinterlacing, final copy to the outpuit buffer (frame_out)
    * Every frame except frame_cap shall be encoded in xrgb
    * Every frame except frame_out shall have the same height
    */
   if (strcmp(video_output_mode, "deinterlaced") == 0) {
      processing_bgr_xrgb(frame_cap, frame_curr, video_cap_width, video_cap_height);
      /* When deinterlacing a interlaced intput, we need to process both fields of a frame,
       * one at a time (retro_run needs to be called twice, vide_half_feed_rate prevents the
       * source from being read twice...
       */
      if (strcmp(video_capture_mode, "interlaced") == 0) {
         enum v4l2_field field_read;
         if (video_half_feed_rate == 0)
            field_read = V4L2_FIELD_TOP;
         else
            field_read = V4L2_FIELD_BOTTOM;
         /* video_half_feed_rate will allow us to capture the interlaced frame once and run the
          * deinterlacing algo twice, extracting a given field for each run.
          */
         processing_deinterlacing_crap(frame_curr, frame_out, video_cap_width, video_cap_height/2, field_read, 1);
      } else {
         processing_deinterlacing_crap(frame_curr, frame_out, video_cap_width, video_cap_height, (enum v4l2_field)video_buf.field, 0);
      }
      aux = frame_prev3;
      frame_prev3 = frame_prev2;
      frame_prev2 = frame_prev1;
      frame_prev1 = frame_curr;
      frame_curr = aux;

      VIDEOPROC_CORE_PREFIX(video_refresh_cb)(frame_out, video_cap_width,
         video_out_height, video_cap_width * sizeof(uint32_t));
   } else if (strcmp(video_capture_mode, "alternate_hack") == 0) {
      /* Case where alternate_hack without deinterlacing would not generate previous frame for processing_heal */
      processing_bgr_xrgb(frame_cap, frame_curr, video_cap_width, video_cap_height);
      aux = frame_prev3;
      frame_prev3 = frame_prev2;
      frame_prev2 = frame_prev1;
      frame_prev1 = frame_curr;
      frame_curr = aux;

      aux = frame_out;
      frame_out = frame_curr;

      VIDEOPROC_CORE_PREFIX(video_refresh_cb)(frame_out, video_cap_width,
         video_out_height, video_cap_width * sizeof(uint32_t));

      frame_out = aux;
   } else {
      processing_bgr_xrgb(frame_cap, frame_out, video_cap_width, video_out_height);

      VIDEOPROC_CORE_PREFIX(video_refresh_cb)(frame_out, video_cap_width,
         video_out_height, video_cap_width * sizeof(uint32_t));
   }
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

#if 0
static void videoinput_set_control_v4l2( uint32_t id, double val )
{
    struct v4l2_queryctrl query;

    query.id = id;
    if( ioctl( video_device_fd, VIDIOC_QUERYCTRL, &query ) >= 0 && !(query.flags & V4L2_CTRL_FLAG_DISABLED) ) {
        struct v4l2_control control;
        control.id = id;
        control.value = query.minimum + ((int) ((val * ((double) (query.maximum - query.minimum))) + 0.5));
        ioctl( video_device_fd, VIDIOC_S_CTRL, &control );
    }
}
#endif

RETRO_API bool VIDEOPROC_CORE_PREFIX(retro_load_game)(const struct retro_game_info *game)
{
   struct retro_variable videodev = { "videoproc_videodev", NULL };
   struct retro_variable audiodev = { "videoproc_audiodev", NULL };
   struct retro_variable capture_mode = { "videoproc_capture_mode", NULL };
   struct retro_variable output_mode  = { "videoproc_output_mode", NULL };
   struct retro_variable frame_times  = { "videoproc_frame_times", NULL };
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

#ifdef HAVE_ALSA
   if (audio_handle != NULL) {
       struct retro_audio_callback audio_cb;
       audio_cb.callback = audio_callback;
       audio_cb.set_state = audio_set_state;
       VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK, &audio_cb);
   }
#endif

   VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &videodev);
   if (videodev.value == NULL) {
       close_devices();
       return false;
   }
   strncpy(video_device, videodev.value, ENVVAR_BUFLEN-1);

   /* Audio device is optional... */
   VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &audiodev);
   if (audiodev.value != NULL) {
      strncpy(audio_device, audiodev.value, ENVVAR_BUFLEN-1);
   }

   VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &capture_mode);
   VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &output_mode);
   if (capture_mode.value == NULL || output_mode.value == NULL) {
       close_devices();
       return false;
   }
   strncpy(video_capture_mode, capture_mode.value, ENVVAR_BUFLEN-1);
   strncpy(video_output_mode, output_mode.value, ENVVAR_BUFLEN-1);

   VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &frame_times);
   if (frame_times.value != NULL) {
      strncpy(video_frame_times, frame_times.value, ENVVAR_BUFLEN-1);
   }

   if (strcmp(video_device, "dummy") == 0) {
      if (strcmp(video_capture_mode, "interlaced") == 0) {
          video_format.fmt.pix.height = 480;
          video_cap_height = 480;
          video_buf.field = V4L2_FIELD_INTERLACED;
      } else if (strcmp(video_capture_mode, "alternate") == 0 ||\
                 strcmp(video_capture_mode, "top") == 0 ||\
                 strcmp(video_capture_mode, "bottom") == 0 ||\
                 strcmp(video_capture_mode, "alternate_hack") == 0) {
          video_format.fmt.pix.height = 240;
          video_cap_height = 240;
          video_buf.field = V4L2_FIELD_TOP;
      }

      dummy_pos=0;
      video_format.fmt.pix.width = 640;
      video_cap_width = 640;
   } else {
      memset(&fmt, 0, sizeof(fmt));
      fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

      error = v4l2_ioctl(video_device_fd, VIDIOC_G_FMT, &fmt);

      if (error != 0)
      {
         printf("VIDIOC_G_FMT failed: %s\n", strerror(errno));
         return false;
      }

      fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24;
      fmt.fmt.pix.colorspace = V4L2_COLORSPACE_REC709;
      fmt.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
      fmt.fmt.pix.quantization = V4L2_QUANTIZATION_LIM_RANGE;

      fmt.fmt.pix.height = 240;
      fmt.fmt.pix.field = V4L2_FIELD_TOP;

      /* TODO Query the size and FPS */
      if (strcmp(video_capture_mode, "interlaced") == 0) {
         v4l2_ncapbuf_target = 2;
         fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
         video_format.fmt.pix.height = 480;
         video_cap_height = 480;
      } else {
         v4l2_ncapbuf_target = 2;
         video_format.fmt.pix.height = 240;
         video_cap_height = 240;
         if (strcmp(video_capture_mode, "alternate") == 0)
            fmt.fmt.pix.field = V4L2_FIELD_ALTERNATE;
         else if (strcmp(video_capture_mode, "top") == 0)
            fmt.fmt.pix.field = V4L2_FIELD_TOP;
         else if (strcmp(video_capture_mode, "bottom") == 0)
            fmt.fmt.pix.field = V4L2_FIELD_BOTTOM;
         else if (strcmp(video_capture_mode, "alternate_hack") == 0) {
            fmt.fmt.pix.field = V4L2_FIELD_TOP;
            v4l2_ncapbuf_target = 1;
         }
      }

      video_format.fmt.pix.width = 720;
      video_cap_width = 720;

      error = v4l2_ioctl(video_device_fd, VIDIOC_S_FMT, &fmt);
      if (error != 0)
      {
         printf("VIDIOC_S_FMT failed: %s\n", strerror(errno));
         return false;
      }

      error = v4l2_ioctl(video_device_fd, VIDIOC_G_STD, &std_id);
      if (error != 0)
      {
         printf("VIDIOC_G_STD failed: %s\n", strerror(errno));
         return false;
      }
      for (index = 0, std_found = false; ; index++)
      {
         memset(&std, 0, sizeof(std));
         std.index = index;
         error = v4l2_ioctl(video_device_fd, VIDIOC_ENUMSTD, &std);
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
      /* TODO Check if what we got is indeed what we asked for */

      memset(&reqbufs, 0, sizeof(reqbufs));
      reqbufs.count = v4l2_ncapbuf_target;
      reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      reqbufs.memory = V4L2_MEMORY_MMAP;

      error = v4l2_ioctl(video_device_fd, VIDIOC_REQBUFS, &reqbufs);
      if (error != 0)
      {
         printf("VIDIOC_REQBUFS failed: %s\n", strerror(errno));
         return false;
      }
      v4l2_ncapbuf = reqbufs.count;
      printf("GOT v4l2_ncapbuf=%" PRI_SIZET "\n", v4l2_ncapbuf);

      for (index = 0; index < v4l2_ncapbuf; index++)
      {
         memset(&buf, 0, sizeof(buf));
         buf.index = index;
         buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
         buf.memory = V4L2_MEMORY_MMAP;

         error = v4l2_ioctl(video_device_fd, VIDIOC_QUERYBUF, &buf);
         if (error != 0)
         {
            printf("VIDIOC_QUERYBUF failed for %u: %s\n", index, strerror(errno));
            return false;
         }

         v4l2_capbuf[index].len = buf.length;
         v4l2_capbuf[index].start = v4l2_mmap(NULL, buf.length,
               PROT_READ|PROT_WRITE, MAP_SHARED, video_device_fd, buf.m.offset);
         if (v4l2_capbuf[index].start == MAP_FAILED)
         {
            printf("v4l2_mmap failed: %s\n", strerror(errno));
            return false;
         }
      }

      for (index = 0; index < v4l2_ncapbuf; index++)
      {
         memset(&buf, 0, sizeof(buf));
         buf.index = index;
         buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
         buf.memory = V4L2_MEMORY_MMAP;

         error = v4l2_ioctl(video_device_fd, VIDIOC_QBUF, &buf);
         if (error != 0)
         {
            printf("VIDIOC_QBUF failed for %u: %s\n", index, strerror(errno));
            return false;
         }
      }

      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      error = v4l2_ioctl(video_device_fd, VIDIOC_STREAMON, &type);
      if (error != 0)
      {
         printf("VIDIOC_STREAMON failed: %s\n", strerror(errno));
         return false;
      }

      /* videoinput_set_control_v4l2(V4L2_CID_HUE, (double) 0.4f); */
   }

   /* TODO Framerates?
    * Each frame should combine both fields into a full frame (if not already captured interlaced), half frame-rate
    */
   if (strcmp(video_output_mode, "interlaced") == 0) {
      if (strcmp(video_capture_mode, "interlaced") == 0) {
         video_out_height = video_cap_height;
      } else {
         printf("WARNING: Capture mode %s with output mode %s is not properly supported yet... (Is this even usefull?)\n", \
                 video_capture_mode, video_output_mode);
         video_out_height = video_cap_height*2;
      }
   /* Each frame has one field, full frame-rate */
   } else if (strcmp(video_output_mode, "progressive") == 0) {
      video_out_height = video_cap_height;
   /* Each frame has one or both field to be deinterlaced into a full frame (double the lines if one field), full frame-rate */
   } else if (strcmp(video_output_mode, "deinterlaced") == 0) {
      if (strcmp(video_capture_mode, "interlaced") == 0)
         video_out_height = video_cap_height;
      else
         video_out_height = video_cap_height*2;
   } else
      video_out_height = video_cap_height;

   printf("Capture Resolution %ux%u\n", video_cap_width, video_cap_height);
   printf("Output Resolution %ux%u\n", video_cap_width, video_out_height);

   frame_cap = (uint8_t*)calloc(1, video_cap_width * video_cap_height * sizeof(uint8_t) * 3);
   frame_out = (uint32_t*)calloc(1, video_cap_width * video_out_height * sizeof(uint32_t));
   /* TODO: Only allocate frames if we are going to use it (for deinterlacing or other filters?) */
   frames[0] = (uint32_t*)calloc(1, video_cap_width * video_out_height * sizeof(uint32_t));
   frames[1] = (uint32_t*)calloc(1, video_cap_width * video_out_height * sizeof(uint32_t));
   frames[2] = (uint32_t*)calloc(1, video_cap_width * video_out_height * sizeof(uint32_t));
   frames[3] = (uint32_t*)calloc(1, video_cap_width * video_out_height * sizeof(uint32_t));

   frame_curr = frames[0];
   frame_prev1 = frames[1];
   frame_prev2 = frames[2];
   frame_prev3 = frames[3];

   /* TODO: Check frames[] allocation */
   if (!frame_out || !frame_cap)
   {
      printf("Cannot allocate buffers\n");
      return false;
   }

   printf("Allocated %" PRI_SIZET " byte conversion buffer\n",
         (size_t)(video_cap_width * video_cap_height) * sizeof(uint32_t));

   pixel_format = RETRO_PIXEL_FORMAT_XRGB8888;
   if (!VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &pixel_format))
   {
      printf("Cannot set pixel format\n");
      return false;
   }

   return true;
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_unload_game)(void)
{
   struct v4l2_requestbuffers reqbufs;
   int i;

#ifdef HAVE_ALSA
   if (audio_handle != NULL) {
       VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK, NULL);
   }
#endif

   if ((strcmp(video_device, "dummy") != 0) && (video_device_fd != -1))
   {
      uint32_t index;
      enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      int               error = v4l2_ioctl(video_device_fd, VIDIOC_STREAMOFF, &type);
      if (error != 0)
         printf("VIDIOC_STREAMOFF failed: %s\n", strerror(errno));

      for (index = 0; index < v4l2_ncapbuf; index++)
         v4l2_munmap(v4l2_capbuf[index].start, v4l2_capbuf[index].len);

      reqbufs.count = 0;
      reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      reqbufs.memory = V4L2_MEMORY_MMAP;
      error = v4l2_ioctl(video_device_fd, VIDIOC_REQBUFS, &reqbufs);
      if (error != 0)
         printf("VIDIOC_REQBUFS failed: %s\n", strerror(errno));

   }

   if (frame_out)
      free(frame_out);
   frame_out = NULL;

   if (frame_cap)
      free(frame_cap);
   frame_cap = NULL;

   for (i = 0; i<4; ++i) {
      if (frames[i])
         free(frames[i]);
      frames[i] = NULL;
   }
   frame_curr = NULL;
   frame_prev1 = NULL;
   frame_prev2 = NULL;
   frame_prev3 = NULL;

   if (ft_info) {
      free(ft_info);
      ft_info = NULL;
   }

   if (ft_info2) {
      free(ft_info2);
      ft_info2 = NULL;
   }

   close_devices();
   video_device[0] = '\0';
   audio_device[0] = '\0';
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

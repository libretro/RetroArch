/*-
 * Copyright (c) 2016 Jared McNeill <jmcneill@invisible.ca>
 * Copyright (c) 2026 Xitee <59659167+Xitee1@users.noreply.github.com>
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
#include <compat/strl.h>
#include <retro_miscellaneous.h>

#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

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
#include <pthread.h>
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

struct v4l2_resolution
{
   int width;
   int height;
};

struct v4l2_resolution v4l2_resolutions[] =
{
   /* 4:3 */
   {160,120},
   {320,240},
   {480,320},
   {640,480},
   {720,480},
   {720,576},
   {800,600},
   {960,720},
   {1024,768},
   {1280,960},
   {1440,1050},
   {1440,1080},
   {1600,1200},
   {1920,1440},
	/* 16:9 */
   {640,360},
   {960,540},
   {1280,720},
   {1600,900},
   {1920,1080},
   {1920,1200},
   {2560,1440},
   {3840,2160}
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

/* Capture frame interval reported by VIDIOC_G_PARM (timeperframe). fps is
 * video_cap_rate_d / video_cap_rate_n; both 0 when the device doesn't report
 * a frame rate (then we fall back to the analog standard or 60). */
static uint32_t video_cap_rate_n;
static uint32_t video_cap_rate_d;

static uint8_t v4l2_ncapbuf_target;
static size_t  v4l2_ncapbuf;
static struct  v4l2_capbuf v4l2_capbuf[VIDEO_BUFFERS_MAX];
struct v4l2_capability caps;

static float   dummy_pos=0;

static int      video_half_feed_rate=0; /* for interlaced captures */
static uint32_t video_cap_width;
static uint32_t video_cap_height;
static uint32_t video_out_height;
static char     video_capture_mode[ENVVAR_BUFLEN];
static char     video_capture_resolution[ENVVAR_BUFLEN];
static char     video_capture_rate[ENVVAR_BUFLEN];
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
/* Serializes audio_handle access between the frontend's async audio
 * callback (which runs on the frontend's audio thread) and the main-thread
 * (re)configuration paths; see close_audio_device(). */
static pthread_mutex_t audio_lock = PTHREAD_MUTEX_INITIALIZER;
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
   int frames  = -1;
   bool opened = false;

   pthread_mutex_lock(&audio_lock);
   if (audio_handle)
   {
      opened = true;
      frames = snd_pcm_readi(audio_handle,
            audio_data, sizeof(audio_data) / 4);

      if (frames < 0)
         snd_pcm_recover(audio_handle, frames, true);
   }
   pthread_mutex_unlock(&audio_lock);

   if (frames >= 0)
      VIDEOPROC_CORE_PREFIX(audio_sample_batch_cb)(audio_data, frames);
   else if (!opened)
   {
      /* No capture device: the frontend's audio thread paces itself on this
       * callback blocking in snd_pcm_readi(), and it cannot be unregistered
       * (SET_AUDIO_CALLBACK(NULL) is ignored). Sleep briefly so the thread
       * doesn't spin at 100% CPU while audio capture is off. */
      usleep(10000);
   }
}

static void audio_set_state(bool enable) { }
#endif

static void appendstr(char *s, const char *in, size_t len)
{
   size_t resid = len - (strlen(s) + 1);
   if (resid != 0)
      strncat(s, in, resid);
}

static void enumerate_video_devices(char *s, size_t len)
{
#ifdef HAVE_UDEV
   int ndevs;
   struct udev_device *dev;
   struct udev_enumerate *enumerate;
   struct udev_list_entry *devices, *dev_list_entry;
   const char *path, *name;
   struct udev *udev = NULL;
#endif

   memset(s, 0, len);
   appendstr(s, "Video capture device; ", len);

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
            appendstr(s, "|", len);
         appendstr(s, name, len);
         ndevs++;
      }

      udev_device_unref(dev);
   }

   udev_enumerate_unref(enumerate);
   udev_unref(udev);
#else
   /* Just return a few options. We'll fail later if the device is not found. */
   appendstr(s, "/dev/video0|/dev/video1|/dev/video2|/dev/video3", len);
#endif
}

static void enumerate_audio_devices(char *s, size_t len)
{
#ifdef HAVE_ALSA
   void **hints, **n;
   char *ioid, *name, *descr;
#endif

   memset(s, 0, len);
   appendstr(s, "Audio capture device; ", len);

   /* Always offer "none" as the first (default) option so the core can be
    * started without any audio capture device present. */
   appendstr(s, "none", len);

#ifdef HAVE_ALSA
   if (snd_device_name_hint(-1, "pcm", &hints) < 0)
      return;

   for (n = hints; *n; n++)
   {
      ioid = snd_device_name_get_hint(*n, "IOID");
      if (ioid != NULL && strcmp(ioid, "Input") != 0)
      {
         free(ioid);
         ioid = NULL;
         continue;
      }

      name = snd_device_name_get_hint(*n, "NAME");
      if (name == NULL || strstr(name, "front:") == NULL)
      {
         free(ioid);
         free(name);
         name = NULL;
         continue;
      }

      /* TODO/FIXME: Add more info to make picking
       * audio device more user friendly */
      descr = snd_device_name_get_hint(*n, "DESC");
      if (!descr)
      {
         free(ioid);
         free(name);
         continue;
      }

      /* "none" already occupies the first slot, so every enumerated
       * device needs a leading separator. */
      appendstr(s, "|", len);
      appendstr(s, name, len);

      /* Not sure if this is necessary
       * but ensuring things are free/NULL */
      if (name)
      {
         free(name);
         name = NULL;
      }
      if(ioid != NULL)
      {
         free(ioid);
         ioid = NULL;
      }
      if(descr != NULL)
      {
         free(descr);
         descr = NULL;
      }
   }

   snd_device_name_free_hint(hints);
#endif
}

/* Append (width, height) to out[] unless it is already present. Returns the
 * updated entry count. */
static size_t add_resolution(struct v4l2_resolution *out, size_t count,
      size_t max, int width, int height)
{
   size_t i;
   for (i = 0; i < count; i++)
      if (out[i].width == width && out[i].height == height)
         return count;
   if (count < max)
   {
      out[count].width  = width;
      out[count].height = height;
      count++;
   }
   return count;
}

/* Append fps to rates[] unless an equal rate (within rounding noise) is
 * already present. Returns the updated entry count. */
static size_t add_rate(double *rates, size_t count, size_t max, double fps)
{
   size_t i;
   for (i = 0; i < count; i++)
      if (fabs(rates[i] - fps) < 0.005)
         return count;
   if (count < max && fps > 0.0)
   {
      rates[count] = fps;
      count++;
   }
   return count;
}

/* Rates offered when a driver reports a continuous frame-interval range,
 * or none at all (no device to probe). */
static const double v4l2_standard_rates[] =
      { 60, 59.94, 50, 30, 29.97, 25, 24, 20, 15, 10 };

/* Collect the frame rates a device supports for one pixel format and frame
 * size (VIDIOC_ENUM_FRAMEINTERVALS) into rates[]. Returns the updated entry
 * count. */
static size_t probe_rates(int fd, uint32_t pixel_format,
      uint32_t width, uint32_t height, double *rates, size_t count, size_t max)
{
   uint32_t index;

   for (index = 0; ; index++)
   {
      struct v4l2_frmivalenum ival;

      memset(&ival, 0, sizeof(ival));
      ival.index        = index;
      ival.pixel_format = pixel_format;
      ival.width        = width;
      ival.height       = height;
      if (v4l2_ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &ival) != 0)
         break;

      if (ival.type == V4L2_FRMIVAL_TYPE_DISCRETE)
      {
         if (ival.discrete.numerator != 0)
            count = add_rate(rates, count, max,
                  (double)ival.discrete.denominator
                  / (double)ival.discrete.numerator);
      }
      else
      {
         /* Stepwise/continuous: index 0 describes the whole interval range
          * (min = shortest interval = fastest rate); offer the standard
          * rates that fall inside it. */
         size_t i;
         double fast = (ival.stepwise.min.numerator != 0)
               ? (double)ival.stepwise.min.denominator
                 / (double)ival.stepwise.min.numerator : 0.0;
         double slow = (ival.stepwise.max.numerator != 0)
               ? (double)ival.stepwise.max.denominator
                 / (double)ival.stepwise.max.numerator : 0.0;
         for (i = 0; i < ARRAY_SIZE(v4l2_standard_rates); i++)
            if (   v4l2_standard_rates[i] >= slow - 0.005
                && v4l2_standard_rates[i] <= fast + 0.005)
               count = add_rate(rates, count, max, v4l2_standard_rates[i]);
         break;
      }
   }
   return count;
}

/* Probe a single V4L2 device for the capture resolutions and frame rates it
 * actually supports and merge them (deduplicated) into out[]/rates[].
 * Returns the updated resolution count and updates *nrates. Drivers report
 * framesizes either as a discrete list or as a stepwise/continuous range;
 * for ranges, offer the entries of the built-in table that fall inside the
 * range. */
static size_t probe_resolutions(const char *device,
      struct v4l2_resolution *out, size_t count, size_t max,
      double *rates, size_t *nrates, size_t rates_max)
{
   int fd;
   uint32_t fmt_index;

   fd = v4l2_open(device, O_RDWR, 0);
   if (fd == -1)
      return count;

   for (fmt_index = 0; ; fmt_index++)
   {
      struct v4l2_fmtdesc fmtdesc;
      uint32_t size_index;

      memset(&fmtdesc, 0, sizeof(fmtdesc));
      fmtdesc.index = fmt_index;
      fmtdesc.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      if (v4l2_ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) != 0)
         break;

      for (size_index = 0; ; size_index++)
      {
         struct v4l2_frmsizeenum frmsize;

         memset(&frmsize, 0, sizeof(frmsize));
         frmsize.index        = size_index;
         frmsize.pixel_format = fmtdesc.pixelformat;
         if (v4l2_ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) != 0)
            break;

         if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
         {
            count   = add_resolution(out, count, max,
                  (int)frmsize.discrete.width, (int)frmsize.discrete.height);
            *nrates = probe_rates(fd, fmtdesc.pixelformat,
                  frmsize.discrete.width, frmsize.discrete.height,
                  rates, *nrates, rates_max);
         }
         else
         {
            /* Stepwise/continuous: index 0 describes the whole range. */
            size_t i;
            for (i = 0; i < ARRAY_SIZE(v4l2_resolutions); i++)
            {
               if (   v4l2_resolutions[i].width  >= (int)frmsize.stepwise.min_width
                   && v4l2_resolutions[i].width  <= (int)frmsize.stepwise.max_width
                   && v4l2_resolutions[i].height >= (int)frmsize.stepwise.min_height
                   && v4l2_resolutions[i].height <= (int)frmsize.stepwise.max_height)
                  count = add_resolution(out, count, max,
                        v4l2_resolutions[i].width, v4l2_resolutions[i].height);
            }
            *nrates = probe_rates(fd, fmtdesc.pixelformat,
                  frmsize.stepwise.max_width, frmsize.stepwise.max_height,
                  rates, *nrates, rates_max);
            break;
         }
      }
   }

   v4l2_close(fd);
   return count;
}

/* Order ascending (width, then height) so the smallest supported mode is the
 * default option and the menu reads naturally. */
static int resolution_cmp(const void *a, const void *b)
{
   const struct v4l2_resolution *ra = (const struct v4l2_resolution *)a;
   const struct v4l2_resolution *rb = (const struct v4l2_resolution *)b;
   if (ra->width != rb->width)
      return ra->width - rb->width;
   return ra->height - rb->height;
}

/* Order descending so the fastest supported rate leads the list ("auto"
 * still precedes it as the default). */
static int rate_cmp(const void *a, const void *b)
{
   double ra = *(const double *)a;
   double rb = *(const double *)b;
   if (ra < rb)
      return 1;
   if (ra > rb)
      return -1;
   return 0;
}

/* Format a probed rate the way users know it (the NTSC fractional rates by
 * their conventional names). Must round-trip through rate_to_timeperframe. */
static void rate_to_string(double fps, char *out, size_t len)
{
   if (fabs(fps - 60000.0 / 1001.0) < 0.01)
      strlcpy(out, "59.94", len);
   else if (fabs(fps - 30000.0 / 1001.0) < 0.01)
      strlcpy(out, "29.97", len);
   else if (fabs(fps - 24000.0 / 1001.0) < 0.01)
      strlcpy(out, "23.976", len);
   else
      snprintf(out, len, "%g", fps);
}

/* Build the capture-resolution and capture-rate option lists by probing
 * capture device(s) for the sizes and frame intervals they really report.
 * video_devices is either a single device path ("/dev/videoN") or the option
 * string built by enumerate_video_devices ("Video capture device;
 * /dev/videoN|..."). Falls back to the built-in v4l2_resolutions[] /
 * v4l2_standard_rates[] tables when nothing could be probed (no device
 * present). */
static void list_capture_options(char *capture_resolutions, size_t res_len,
      char *capture_rates, size_t rates_len, const char *video_devices)
{
   struct v4l2_resolution probed[128];
   double rates[32];
   const struct v4l2_resolution *res;
   size_t count  = 0;
   size_t nrates = 0;
   size_t i, n, written;
   char devices[ENVVAR_BUFLEN];
   char *token;
   const char *list;

   /* Skip the "Video capture device; " menu label before tokenizing. */
   list = strchr(video_devices, ';');
   strlcpy(devices, list ? list + 1 : video_devices, sizeof(devices));

   for (token = strtok(devices, "|"); token != NULL; token = strtok(NULL, "|"))
   {
      /* The menu label ends in "; ", so the first token carries a leading
       * space — trim it or the first device is never matched. */
      while (*token == ' ')
         token++;
      if (!string_starts_with(token, "/dev/video"))
         continue;
      count = probe_resolutions(token, probed, count, ARRAY_SIZE(probed),
            rates, &nrates, ARRAY_SIZE(rates));
   }

   if (count > 0)
      qsort(probed, count, sizeof(probed[0]), resolution_cmp);

   /* Fall back to the built-in table when nothing could be probed. */
   res = (count > 0) ? probed : v4l2_resolutions;
   n   = (count > 0) ? count  : ARRAY_SIZE(v4l2_resolutions);

   written = snprintf(capture_resolutions, res_len, "Capture resolution; ");
   for (i = 0; i < n; i++)
   {
      char entry[32];
      size_t elen = (size_t)snprintf(entry, sizeof(entry), "%s%dx%d",
            i > 0 ? "|" : "", res[i].width, res[i].height);
      /* Stop before truncating an entry mid-token; the caller still
       * appends "|auto" and it must always fit. */
      if (written + elen + STRLEN_CONST("|auto") + 1 > res_len)
         break;
      strlcpy(capture_resolutions + written, entry, res_len - written);
      written += elen;
   }

   /* Rate list: "auto" (keep the device's current rate) is always first and
    * stays the default. */
   if (nrates == 0)
      for (i = 0; i < ARRAY_SIZE(v4l2_standard_rates); i++)
         nrates = add_rate(rates, nrates, ARRAY_SIZE(rates),
               v4l2_standard_rates[i]);
   qsort(rates, nrates, sizeof(rates[0]), rate_cmp);

   written = snprintf(capture_rates, rates_len, "Capture frame rate; auto");
   for (i = 0; i < nrates; i++)
   {
      char entry[32];
      char rstr[16];
      size_t elen;
      rate_to_string(rates[i], rstr, sizeof(rstr));
      elen = (size_t)snprintf(entry, sizeof(entry), "|%s", rstr);
      if (written + elen + 1 > rates_len)
         break;
      strlcpy(capture_rates + written, entry, rates_len - written);
      written += elen;
   }
}
/* Probed capture option value lists and the device selection they were
 * probed from. File-scope statics: they survive core restarts when the core
 * is statically linked into the frontend (an empty-buffer check alone would
 * never refresh them), and the runtime device-switch path re-registers
 * through them. */
static char capture_resolutions[ENVVAR_BUFLEN];
static char capture_rates[ENVVAR_BUFLEN];
static char probed_for[ENVVAR_BUFLEN];

/* Enumerate the capture devices, resolve the selected one and register the
 * complete option set with resolution/rate lists probed from that device.
 * Called from retro_set_environment and again from the reconfigure path in
 * retro_run when the user switches capture devices, so the lists always
 * describe the selected device without requiring a core restart. */
static void register_options(void)
{
   char video_devices[ENVVAR_BUFLEN];
   char audio_devices[ENVVAR_BUFLEN];
   struct retro_variable videodev = { "videoproc_videodev", NULL };
   struct retro_variable envvars[] = {
      { "videoproc_videodev", NULL },
      { "videoproc_audiodev", NULL },
      { "videoproc_capture_mode", "Capture mode; alternate|deinterlaced|interlaced|top|bottom|alternate_hack" },
      { "videoproc_capture_resolution", NULL },
      { "videoproc_capture_rate", NULL },
      { "videoproc_output_mode","Output mode; progressive|deinterlaced|interlaced" },
      { "videoproc_frame_times","Print frame times to terminal (v4l2 only); Off|On" },
      { NULL, NULL }
   };

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
   envvars[3].key   = "videoproc_capture_resolution";
   envvars[3].value = capture_resolutions;
   envvars[4].key   = "videoproc_capture_rate";
   envvars[4].value = capture_rates;

   /* Register the options once before the probed resolution/rate lists are
    * known: this seeds the frontend with the saved values, and a variable
    * can only be resolved after it has been registered with its value list
    * — so the saved device selection only becomes queryable now. Values
    * must not be NULL (only the {NULL, NULL} entry terminates the array),
    * so seed the lists with the built-in tables; they are rebuilt from the
    * actual device below. */
   if (capture_resolutions[0] == '\0')
   {
      list_capture_options(capture_resolutions, sizeof(capture_resolutions),
            capture_rates, sizeof(capture_rates), "");
      appendstr(capture_resolutions, "|auto", ENVVAR_BUFLEN);
   }
   VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_SET_VARIABLES, envvars);
   VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &videodev);

   /* Probe the capture device for its real, supported resolutions and frame
    * rates rather than offering fixed tables the hardware may not honour.
    * Probe only the selected device when it is known, so the option lists
    * reflect what that device actually supports; on a fresh configuration
    * (no device selected yet) probe every enumerated device. Probing opens
    * the device(s), so cache the lists and rebuild them only when the probed
    * selection changes (i.e. on the first core start after switching
    * devices). */
   {
      const char *probe_target =
            (videodev.value && string_starts_with(videodev.value, "/dev/video"))
            ? videodev.value : video_devices;
      if (strcmp(probed_for, probe_target) != 0)
      {
         list_capture_options(capture_resolutions, sizeof(capture_resolutions),
               capture_rates, sizeof(capture_rates), probe_target);
         appendstr(capture_resolutions, "|auto", ENVVAR_BUFLEN);
         strlcpy(probed_for, probe_target, sizeof(probed_for));
      }
   }

   VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_SET_VARIABLES, envvars);
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_set_environment)(retro_environment_t cb)
{
   bool no_content = true;

   VIDEOPROC_CORE_PREFIX(environment_cb) = cb;

   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);

   register_options();
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

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_init)(void) { }

#ifdef HAVE_ALSA
static void close_audio_device(void);

/* Open the ALSA capture device and configure it for 48kHz, 16-bit stereo.
 * Returns 0 on success or the negative ALSA error code with the handle
 * closed again, so a failure leaves no half-configured device behind. */
static int open_audio_device(const char *device)
{
   snd_pcm_t *handle = NULL;
   snd_pcm_hw_params_t *hw_params;
   unsigned int rate;
   int error;

   /* Configure a local handle and publish it in audio_handle only when it is
    * fully set up: RetroArch ignores SET_AUDIO_CALLBACK(NULL), so the async
    * audio callback can still be registered and must never see the device
    * mid-setup (its snd_pcm_recover() would start the stream and make
    * snd_pcm_prepare() below fail with EBUSY). */
   error = snd_pcm_open(&handle, device, SND_PCM_STREAM_CAPTURE, 0);
   if (error < 0)
   {
      printf("Couldn't open %s: %s\n", device, snd_strerror(error));
      return error;
   }

   error = snd_pcm_hw_params_malloc(&hw_params);
   if (error)
   {
      printf("Couldn't allocate hw param structure: %s\n", snd_strerror(error));
      snd_pcm_close(handle);
      return error;
   }
   error = snd_pcm_hw_params_any(handle, hw_params);
   if (error)
   {
      printf("Couldn't initialize hw param structure: %s\n", snd_strerror(error));
      goto fail;
   }
   error = snd_pcm_hw_params_set_access(handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
   if (error)
   {
      printf("Couldn't set hw param access type: %s\n", snd_strerror(error));
      goto fail;
   }
   error = snd_pcm_hw_params_set_format(handle, hw_params, SND_PCM_FORMAT_S16_LE);
   if (error)
   {
      printf("Couldn't set hw param format to SND_PCM_FORMAT_S16_LE: %s\n", snd_strerror(error));
      goto fail;
   }
   rate = AUDIO_SAMPLE_RATE;
   error = snd_pcm_hw_params_set_rate_near(handle, hw_params, &rate, 0);
   if (error)
   {
      printf("Couldn't set hw param sample rate to %u: %s\n", rate, snd_strerror(error));
      goto fail;
   }
   if (rate != AUDIO_SAMPLE_RATE)
   {
      printf("Hardware doesn't support sample rate %u (returned %u)\n", AUDIO_SAMPLE_RATE, rate);
      error = -EINVAL;
      goto fail;
   }
   error = snd_pcm_hw_params_set_channels(handle, hw_params, 2);
   if (error)
   {
      printf("Couldn't set hw param channels to 2: %s\n", snd_strerror(error));
      goto fail;
   }
   error = snd_pcm_hw_params(handle, hw_params);
   if (error)
   {
      printf("Couldn't set hw params: %s\n", snd_strerror(error));
      goto fail;
   }
   snd_pcm_hw_params_free(hw_params);

   error = snd_pcm_prepare(handle);
   if (error)
   {
      printf("Couldn't prepare audio interface for use: %s\n", snd_strerror(error));
      snd_pcm_close(handle);
      return error;
   }

   pthread_mutex_lock(&audio_lock);
   audio_handle = handle;
   pthread_mutex_unlock(&audio_lock);
   printf("Using ALSA device %s for audio input\n", device);
   return 0;

fail:
   snd_pcm_hw_params_free(hw_params);
   snd_pcm_close(handle);
   return error;
}

/* Open the ALSA device with a brief retry (reopening a device that was
 * closed a moment ago can fail transiently with EBUSY while the frontend's
 * audio thread winds down). Audio is optional: on failure, tell the user on
 * screen and continue without audio. Returns true when capturing. */
static bool start_audio_capture(const char *device)
{
   int attempt;
   int error = -EBUSY;

   for (attempt = 0; attempt < 5 && error == -EBUSY; attempt++)
   {
      if (attempt > 0)
         usleep(20000);
      error = open_audio_device(device);
   }
   if (error != 0)
   {
      /* Surface the problem in the frontend, too: a silent core with no
       * on-screen hint is hard to diagnose from the menu. The specific
       * reason is printed to the terminal by open_audio_device(). */
      struct retro_message msg =
            { "Audio capture could not be started - continuing without audio", 240 };
      printf("Audio device %s could not be started (non-fatal), continuing without audio\n",
            device);
      VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
      return false;
   }
   return true;
}
#endif

static bool open_devices(void)
{
   struct retro_variable videodev = { "videoproc_videodev", NULL };
   struct retro_variable audiodev = { "videoproc_audiodev", NULL };
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

   if (strcmp(videodev.value, "dummy") == 0)
      return true;

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
      video_device_fd = -1;
      return false;
   }

   printf("%s:\n", videodev.value);
   printf("  Driver: %s\n", caps.driver);
   printf("  Card: %s\n", caps.card);
   printf("  Bus Info: %s\n", caps.bus_info);
   printf("  Version: %u.%u.%u\n", (caps.version >> 16) & 0xff,
         (caps.version >> 8) & 0xff, caps.version & 0xff);

#ifdef HAVE_ALSA
   /* Skip all audio setup when no capture device is selected. Treat both a
    * missing variable and the explicit "none" option as "no audio".
    * Also skip when audio_handle is already open: a video-only reconfigure
    * (e.g. a resolution change) keeps the existing audio device alive so we
    * don't close and immediately reopen the same ALSA device while its async
    * capture callback is still running (which races and returns EBUSY).
    *
    * Audio is optional: failing to bring it up must not prevent video capture
    * from starting. */
   if (audio_handle == NULL && audiodev.value && strcmp(audiodev.value, "none") != 0)
      start_audio_capture(audiodev.value);
#endif

   return true;
}

static void unload_game_internal(bool keep_audio);
static bool load_game_internal(bool allow_fallback);

/* Stop the frontend from invoking audio_callback before the ALSA handle is
 * closed; otherwise the async callback races snd_pcm_close() and can read
 * from a freed handle. */
static void unregister_audio_callback(void)
{
#ifdef HAVE_ALSA
   if (audio_handle != NULL)
      VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK, NULL);
#endif
}

static void close_audio_device(void)
{
#ifdef HAVE_ALSA
   snd_pcm_t *handle;
   /* Unpublish the handle under the lock: RetroArch ignores
    * SET_AUDIO_CALLBACK(NULL), so the async audio callback may still be
    * registered. Taking the lock waits for an in-flight snd_pcm_readi() to
    * drain, and later invocations see NULL — the handle is never closed
    * underneath a reader. */
   pthread_mutex_lock(&audio_lock);
   handle       = audio_handle;
   audio_handle = NULL;
   pthread_mutex_unlock(&audio_lock);
   if (handle)
      snd_pcm_close(handle);
#endif
}

static void close_video_device(void)
{
   if (video_device_fd != -1)
   {
      v4l2_close(video_device_fd);
      video_device_fd = -1;
   }
}

static void close_devices(void)
{
   close_audio_device();
   close_video_device();
}

/* Unmap all capture buffers mapped so far and forget them. Leaked mappings
 * keep the V4L2 device busy even after its fd is closed, which makes every
 * later VIDIOC_S_FMT on it fail with EBUSY. */
static void release_video_buffers(void)
{
   size_t i;
   for (i = 0; i < v4l2_ncapbuf; i++)
      if (v4l2_capbuf[i].start && v4l2_capbuf[i].start != MAP_FAILED)
         v4l2_munmap(v4l2_capbuf[i].start, v4l2_capbuf[i].len);
   memset(v4l2_capbuf, 0, sizeof(v4l2_capbuf));
   v4l2_ncapbuf = 0;
}

/* Free the conversion/output frame buffers and forget the deinterlacer's
 * rotation pointers. retro_run treats NULL buffers as "stay idle". */
static void release_frame_buffers(void)
{
   int i;

   if (frame_out)
      free(frame_out);
   frame_out = NULL;

   if (frame_cap)
      free(frame_cap);
   frame_cap = NULL;

   for (i = 0; i < 4; ++i)
   {
      if (frames[i])
         free(frames[i]);
      frames[i] = NULL;
   }
   frame_curr  = NULL;
   frame_prev1 = NULL;
   frame_prev2 = NULL;
   frame_prev3 = NULL;
}

/* Configure the built-in test pattern source. Also used as the fallback when
 * the configured capture device cannot be started, so the core still runs
 * and the device option remains editable from the menu. */
static void setup_dummy_source(void)
{
   uint32_t height;

   if (strcmp(video_capture_mode, "interlaced") == 0)
   {
      height          = 480;
      video_buf.field = V4L2_FIELD_INTERLACED;
   }
   else if (     strcmp(video_capture_mode, "alternate") == 0
              || strcmp(video_capture_mode, "top") == 0
              || strcmp(video_capture_mode, "bottom") == 0
              || strcmp(video_capture_mode, "alternate_hack") == 0)
   {
      height          = 240;
      video_buf.field = V4L2_FIELD_TOP;
   }
   else
   {
      /* "deinterlaced": full frames. Without this branch the height stayed
       * 0 (or stale from a previous device) and the zero-height geometry
       * reported to the frontend crashed its video driver. */
      height          = 480;
      video_buf.field = V4L2_FIELD_NONE;
   }

   video_format.fmt.pix.height = height;
   video_cap_height            = height;
   dummy_pos                   = 0;
   video_format.fmt.pix.width  = 640;
   video_cap_width             = 640;
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

/* Convert a "Capture frame rate" option value to a V4L2 timeperframe
 * (seconds per frame = num/den). The NTSC fractional rates map to their
 * exact 1001-based intervals; any other value (integer or decimal, e.g. a
 * probed "7.5") is parsed generically. Returns false for
 * "auto"/unrecognised values, meaning "let the device keep its current
 * rate". */
static bool rate_to_timeperframe(const char *s, uint32_t *num, uint32_t *den)
{
   double fps;
   char *end;
   if (!s || strcmp(s, "auto") == 0)
      return false;
   if (strcmp(s, "59.94") == 0)
   {
      *num = 1001;
      *den = 60000;
      return true;
   }
   if (strcmp(s, "29.97") == 0)
   {
      *num = 1001;
      *den = 30000;
      return true;
   }
   if (strcmp(s, "23.976") == 0)
   {
      *num = 1001;
      *den = 24000;
      return true;
   }
   fps = strtod(s, &end);
   if (end == s || fps <= 0.0)
      return false;
   *num = 1000;
   *den = (uint32_t)(fps * 1000.0 + 0.5);
   return true;
}

/* The av_info most recently reported to the frontend, used to skip the
 * expensive SET_SYSTEM_AV_INFO (a full frontend AV reinit) after an option
 * change that granted identical geometry and timing. */
static struct retro_system_av_info video_last_av_info;

static bool av_info_equal(const struct retro_system_av_info *a,
      const struct retro_system_av_info *b)
{
   return a->geometry.base_width   == b->geometry.base_width
       && a->geometry.base_height  == b->geometry.base_height
       && a->geometry.max_width    == b->geometry.max_width
       && a->geometry.max_height   == b->geometry.max_height
       && a->geometry.aspect_ratio == b->geometry.aspect_ratio
       && a->timing.fps            == b->timing.fps
       && a->timing.sample_rate    == b->timing.sample_rate;
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_get_system_av_info)(struct retro_system_av_info *info)
{
   struct v4l2_cropcap cc;
   int error;

   /* Judge by the device that is actually running, not the option value:
    * retro_load_game may have fallen back to the dummy source. */
   if (video_device[0] == '\0' || strcmp(video_device, "dummy") == 0)
   {
      info->geometry.aspect_ratio = 4.0/3.0;
      info->geometry.base_width   = info->geometry.max_width = video_cap_width;
      info->geometry.base_height  = video_cap_height; /* out? */
      info->geometry.max_height   = video_out_height;
      info->timing.fps            = 60;
      info->timing.sample_rate    = AUDIO_SAMPLE_RATE;
   }
   else
   {
      bool nodouble;
      /*
       * Query the device cropping limits. If available, we can use this to find the capture pixel aspect.
       */
      memset(&cc, 0, sizeof(cc));
      cc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      error = v4l2_ioctl(video_device_fd, VIDIOC_CROPCAP, &cc);

      /* video_format already holds the resolution the driver granted in
       * retro_load_game (clamped to a supported mode). Report that directly;
       * re-parsing the requested option string here would overwrite it with a
       * size the hardware may have rejected and desync the buffers. */
      info->geometry.base_width  = info->geometry.max_width = video_format.fmt.pix.width;
      info->geometry.base_height = video_format.fmt.pix.height;

      /* no doubling for interlaced or deinterlaced capture */
      nodouble = strcmp(video_capture_mode, "deinterlaced") == 0 || strcmp(video_capture_mode, "interlaced") == 0;
      info->geometry.max_height = nodouble ? video_format.fmt.pix.height : video_format.fmt.pix.height * 2;

      /* TODO Only double if frames ARE fields (progressive or deinterlaced, full framerate)
       * *2 for fields
       */
      if(video_standard.frameperiod.denominator != 0 && video_standard.frameperiod.numerator != 0)
      {
         /* Analog TV standard available (PAL/NTSC): frameperiod is the frame
          * rate; *2 yields the field rate. Unchanged legacy behaviour. */
         info->timing.fps = ((double)(video_standard.frameperiod.denominator*2)) /
                             (double)video_standard.frameperiod.numerator;
      }
      else if (video_cap_rate_n != 0 && video_cap_rate_d != 0)
      {
         /* No analog standard (e.g. UVC/HDMI capture): use the actual frame
          * rate from VIDIOC_G_PARM. It is the buffer delivery rate, so it maps
          * directly to the frontend rate, except in "interlaced" mode where the
          * deinterlacer emits two outputs (one per field) per captured frame.
          * That only happens when the driver really granted an interlaced
          * format; if it clamped the request to progressive, buffers arrive
          * (and are output) once per frame at cap_fps. */
         double cap_fps = (double)video_cap_rate_d / (double)video_cap_rate_n;
         info->timing.fps = (   strcmp(video_capture_mode, "interlaced") == 0
                             && video_format.fmt.pix.field == V4L2_FIELD_INTERLACED)
                            ? cap_fps * 2.0 : cap_fps;
      }
      else
         info->timing.fps = 60; /* nothing usable reported */
      info->timing.sample_rate   = AUDIO_SAMPLE_RATE;

      /* TODO/FIXME Allow for fixed 4:3 and 16:9 modes */
      /* Default to square pixels when the driver reports no cropping/pixel
       * aspect information (common for UVC/HDMI grabbers and v4l2loopback);
       * leaving the field unset would push uninitialized stack data to the
       * frontend through SET_SYSTEM_AV_INFO in retro_run(). */
      if (error == 0 && cc.pixelaspect.numerator != 0 && cc.pixelaspect.denominator != 0)
         info->geometry.aspect_ratio = (double)info->geometry.base_width / (double)info->geometry.max_height /
            ((double)cc.pixelaspect.numerator / (double)cc.pixelaspect.denominator);
      else
         info->geometry.aspect_ratio = (double)info->geometry.base_width /
               (double)info->geometry.max_height;
   }

   video_last_av_info = *info;

   printf("Aspect ratio: %f\n",info->geometry.aspect_ratio);
   printf("Buffer Resolution %ux%u %f fps\n", info->geometry.base_width,
             info->geometry.base_height, info->timing.fps);
   printf("Buffer Max Resolution %ux%u\n", info->geometry.max_width,
             info->geometry.max_height);
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_set_controller_port_device)(unsigned a, unsigned b)
{
}

/* Set when the pipeline was rebuilt outside retro_run (retro_reset): the
 * device can come back with a different granted geometry or frame rate, and
 * without pushing it the frontend keeps pacing on the old timing. The libretro
 * spec allows SET_SYSTEM_AV_INFO only from within retro_run, so the push is
 * deferred to the next iteration. */
static bool av_info_dirty;

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_reset)(void)
{
   /* Tear the whole pipeline down and bring it back up. Merely closing and
    * reopening the device fds (the old behaviour) left the V4L2 stream dead
    * — no S_FMT/REQBUFS/STREAMON — until the next option change. */
   unload_game_internal(false);
   if (load_game_internal(true))
      av_info_dirty = true;
}

/* TODO improve this mess and make it generic enough for use with dummy mode */
void v4l2_frame_times(struct v4l2_buffer buf)
{
   /* Opt-in only: video_frame_times can still be empty (not fetched yet)
    * when running under a frontend that doesn't resolve the variable. */
   if (strcmp("On", video_frame_times) != 0)
       return;

   if (ft_info == NULL)
       ft_info = (char*)calloc(5000, sizeof(char));

   if ( (buf.timestamp.tv_sec - ft_prevtime.tv_sec >= 1) && \
        (buf.timestamp.tv_usec + 1000000 - ft_prevtime2.tv_usec) >= 1000000)
   {
       double csec = ((double) buf.timestamp.tv_sec) + (buf.timestamp.tv_usec/1000000);
       double psec = ((double) ft_prevtime.tv_sec) + (ft_prevtime.tv_usec/1000000);
       printf("Fields last %.2f seconds: %d\n", csec - psec, ft_fcount);
       printf("Average frame times: %.3fms\n", ft_favg/(1000*ft_fcount));
       printf("Fields timestampdiffs last second:\n%s\n", ft_info);
       free(ft_info);
       ft_info     = (char*)calloc(5000, sizeof(char));
       ft_fcount   = 0;
       ft_favg     = 0;
       ft_prevtime = buf.timestamp;
   }
   ft_fcount++;
   ft_info2 = strdup(ft_info);
   ft_ftime = (double) (buf.timestamp.tv_usec + ((buf.timestamp.tv_sec - ft_prevtime2.tv_sec >= 1) ? 1000000 : 0)  - ft_prevtime2.tv_usec);
   ft_favg += ft_ftime;
   snprintf(ft_info, 5000 * sizeof(char), "%s %6.d %d %d %.2fms%s",
         ft_info2, buf.sequence, buf.index, buf.field, ft_ftime/1000,
         (!(ft_fcount % 7)) ? "\n" : "");
   free(ft_info2);
   /* Leaving this dangling made unload_game_internal() free it a second
    * time on the next reconfigure (double free → abort). */
   ft_info2 = NULL;

   ft_prevtime2 = buf.timestamp;
}

void source_dummy(int width, int height)
{
   int i, triangpos, triangpos_t=0, triangpos_b=0, offset=0;
   bool field_ahead = false;
   uint8_t *src = frame_cap;
   float step = M_PI/64;

   if (video_buf.field == V4L2_FIELD_TOP)
      offset=0;
   else if (video_buf.field == V4L2_FIELD_BOTTOM)
      offset=1;

   dummy_pos += step;
   /* no animation */
   /* dummy_pos = M_PI/4; step = 0; */
   triangpos = (sinf(dummy_pos)+1)/2*width;
   if (video_buf.field == V4L2_FIELD_INTERLACED)
   {
      if (video_half_feed_rate == 0)
          video_half_feed_rate = 1;
      triangpos_t = (sinf(dummy_pos)+1)/2*width;
      dummy_pos += step;
      triangpos_b = (sinf(dummy_pos)+1)/2*width;
   }

   for (i = 0; i < width * height; i+=1, src+=3)
   {
      float color = (clamp_float((float)(triangpos - (i%width) + offset)/10, -1, 1)+1)/2;
      src[0] = 0x10 + color*0xE0;
      src[1] = 0x10 + color*0xE0;
      src[2] = 0x10 + color*0xE0;

      /* End of a line */
      if ( ((i+1) % width) == 0 )
      {
         triangpos -= 2; /* offset should be half of this? */
         triangpos_t -= 1;
         triangpos_b -= 1;
         if (video_buf.field == V4L2_FIELD_INTERLACED)
         {
            if (field_ahead)
            {
               triangpos = triangpos_b;
               field_ahead = false;
            }
            else
            {
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

/* frame_cap is allocated as video_cap_width * video_cap_height * 3 bytes.
 * Copy no more than that, and never read past the captured buffer. */
static void copy_frame_cap(const struct v4l2_capbuf *capbuf)
{
   size_t copy_size;
   /* alternate_hack re-maps its buffer every frame; a transient mmap failure
    * leaves the pointer invalid. Keep the previous frame instead of reading
    * from a bad mapping. */
   if (capbuf->start == NULL || capbuf->start == MAP_FAILED)
      return;
   copy_size = MIN((size_t)video_cap_width * video_cap_height * 3,
         capbuf->len);
   memcpy(frame_cap, capbuf->start, copy_size);
}

void source_v4l2_normal(int width, int height)
{
   struct v4l2_buffer bufcp;

   int error;

   /* Wait until v4l2 dequees a buffer */
   memset(&video_buf, 0, sizeof(struct v4l2_buffer));
   video_buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   video_buf.memory = V4L2_MEMORY_MMAP;
   error            = v4l2_ioctl(video_device_fd, VIDIOC_DQBUF, &video_buf);
   if (error != 0)
   {
      printf("VIDIOC_DQBUF failed: %s\n", strerror(errno));
      VIDEOPROC_CORE_PREFIX(video_refresh_cb)(NULL, 0, 0, 0);
      return;
   }

   bufcp = video_buf;
   copy_frame_cap(&v4l2_capbuf[video_buf.index]);

   error = v4l2_ioctl(video_device_fd, VIDIOC_QBUF, &video_buf);
   if (error != 0)
      printf("VIDIOC_QBUF failed: %s\n", strerror(errno));

   v4l2_frame_times(bufcp);
}

void source_v4l2_alternate_hack(int width, int height)
{
   struct v4l2_buffer bufcp;
   struct v4l2_format fmt;
   struct v4l2_requestbuffers reqbufs;
   enum v4l2_buf_type type;

   int error;
   uint32_t index;

   /* For later, saving time */
   memset(&fmt, 0, sizeof(fmt));
   fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   error    = v4l2_ioctl(video_device_fd, VIDIOC_G_FMT, &fmt);
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
   copy_frame_cap(&v4l2_capbuf[video_buf.index]);

   if (v4l2_capbuf[0].start && v4l2_capbuf[0].start != MAP_FAILED)
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
   /* The driver can grant more buffers than requested; v4l2_capbuf[] only
    * holds VIDEO_BUFFERS_MAX entries and only that many are mapped/queued. */
   v4l2_ncapbuf = MIN(reqbufs.count, VIDEO_BUFFERS_MAX);
   /* printf("GOT v4l2_ncapbuf=%ld\n", v4l2_ncapbuf); */

   index            = 0;
   memset(&video_buf, 0, sizeof(struct v4l2_buffer));
   video_buf.index  = index;
   video_buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   video_buf.memory = V4L2_MEMORY_MMAP;

   if (v4l2_ioctl(video_device_fd, VIDIOC_QUERYBUF, &video_buf) != 0)
      printf("VIDIOC_QUERYBUF failed: %s\n", strerror(errno));
   v4l2_capbuf[index].len   = video_buf.length;
   v4l2_capbuf[index].start = v4l2_mmap(NULL, video_buf.length,
         PROT_READ|PROT_WRITE, MAP_SHARED, video_device_fd, video_buf.m.offset);
   if (v4l2_capbuf[index].start == MAP_FAILED)
   {
      printf("v4l2_mmap failed: %s\n", strerror(errno));
      v4l2_capbuf[index].start = NULL;
      v4l2_capbuf[index].len   = 0;
   }
   memset(&video_buf, 0, sizeof(struct v4l2_buffer));

   video_buf.index  = index;
   video_buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
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

void processing_heal(uint8_t *src, int width, int height)
{
   int i;
   uint32_t *fp1 = frame_prev1;
   for (i = 0; i < width * height; i+=1, src += 3, ++fp1)
   {
      /* Tries to filter a bunch of blanked out scanline sections my capture cards spits out with this crazy hack
       * Since the blanked out scanlines are set to a black value bellow anything that can be captured, it's quite
       * easy to select the scanlines...
       */
      if (src[0] <= 0 && src[1] <= 0 && src[2] <= 0 && i >= width && i <= width * height - width)
      {
         if (*(src + 0 - width*3) >= ((*fp1>> 0&0xFF)-6) && \
             *(src + 1 - width*3) >= ((*fp1>> 8&0xFF)-6) && \
             *(src + 2 - width*3) >= ((*fp1>>16&0xFF)-6) && \
             *(src + 0 - width*3) <= ((*fp1>> 0&0xFF)+6) && \
             *(src + 1 - width*3) <= ((*fp1>> 8&0xFF)+6) && \
             *(src + 2 - width*3) <= ((*fp1>>16&0xFF)+6))
         {
            src[0] = (*fp1>> 0&0xFF);
            src[1] = (*fp1>> 8&0xFF);
            src[2] = (*fp1>>16&0xFF);
         }
         else
         {
            src[0] = (*(fp1+i+width)>> 0&0xFF);
            src[1] = (*(fp1+i+width)>> 8&0xFF);
            src[2] = (*(fp1+i+width)>>16&0xFF);
         }
      }
   }
}

void processing_deinterlacing_crap(uint32_t *src, uint32_t *dst, int width,
      int height, enum v4l2_field field, int skip_lines_src)
{
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
   if (field == V4L2_FIELD_BOTTOM)
   {
      dst += width;
      if (skip_lines_src == 1)
      {
         src += width;
         fp1 += width;
         fp2 += width;
         fp3 += width;
      }
   }

   for (i = 0; i < width * height; i+=1, src += 1, dst += 1, ++fp1, ++fp2, ++fp3)
   {
      /* Will fill the current destination line with current field
       * The masking is used to prserve some information set by the
       * deinterlacing process, uses the alpha channel to tell if a
       * pixel needs further processing...
       */
      *(dst) =  (*(src) & 0x00FFFFFF) | (*dst & 0xFF000000);

      /* Crappy deinterlacing */
      if (i >= width && i <= (width*height-width))
      {
         pixacul=((((*(dst)>> 0&0xFF) + (pixacul>> 0&0xFF))>>1<<0 |\
                   ((*(dst)>> 8&0xFF) + (pixacul>> 8&0xFF))>>1<<8 |\
                   ((*(dst)>>16&0xFF) + (pixacul>>16&0xFF))>>1<<16) \
                   & 0x00FFFFFF) | 0xFE000000;

         if ( ((*(dst          ) & 0xFF000000) == 0xFE000000) ||\
              ((*(dst+targetrow) & 0xFF000000) == 0xFE000000) )
         {
            *dst = pixacul | 0xFF000000;
            *(dst+targetrow) = pixacul | 0xFF000000;
         }
         else
         {
            if (!(((*(src+0)>> 0&0xFF) >= ((*(fp2+0)>> 0&0xFF)-9) ) &&\
                  ((*(src+0)>> 8&0xFF) >= ((*(fp2+0)>> 8&0xFF)-9) ) &&\
                  ((*(src+0)>>16&0xFF) >= ((*(fp2+0)>>16&0xFF)-9) ) &&\
                  ((*(src+0)>> 0&0xFF) <= ((*(fp2+0)>> 0&0xFF)+9) ) &&\
                  ((*(src+0)>> 8&0xFF) <= ((*(fp2+0)>> 8&0xFF)+9) ) &&\
                  ((*(src+0)>>16&0xFF) <= ((*(fp2+0)>>16&0xFF)+9) )) )
               *(dst+targetrow) = pixacul;
            else if (!(((*fp3>> 0&0xFF) >= ((*fp1>> 0&0xFF)-9) ) &&\
                         ((*fp3>> 8&0xFF) >= ((*fp1>> 8&0xFF)-9) ) &&\
                         ((*fp3>>16&0xFF) >= ((*fp1>>16&0xFF)-9) ) &&\
                         ((*fp3>> 0&0xFF) <= ((*fp1>> 0&0xFF)+9) ) &&\
                         ((*fp3>> 8&0xFF) <= ((*fp1>> 8&0xFF)+9) ) &&\
                         ((*fp3>>16&0xFF) <= ((*fp1>>16&0xFF)+9) )))
               *(dst+targetrow) = pixacul;
         }
      }

      /* Skips a scanline if we reach the end of the current one
       * On progressive sources, should only skip the destination lines,
       * On interlaced sources, should skip both the source and the destination lines
       */
      if ( ((i+1) % width) == 0 )
      {
         dst += width;
         if (skip_lines_src == 1)
         {
            src += width;
            fp1 += width;
            fp2 += width;
            fp3 += width;
         }
      }
   }
}

void processing_bgr_xrgb(uint8_t *src, uint32_t *dst, int width, int height)
{
   /* BGR24 to XRGB8888 conversion */
   int i;
   for (i = 0; i < width * height; i+=1, src += 3, dst += 1)
      *dst = 0xFF << 24 | src[2] << 16 | src[1] << 8 | src[0] << 0;
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_run)(void)
{
   uint32_t *aux;
   struct retro_variable videodev    = { "videoproc_videodev", NULL };
   struct retro_variable audiodev    = { "videoproc_audiodev", NULL };
   struct retro_variable capturemode = { "videoproc_capture_mode", NULL };
   struct retro_variable captureresolution = { "videoproc_capture_resolution", NULL };
   struct retro_variable capturerate = { "videoproc_capture_rate", NULL };
   struct retro_variable outputmode  = { "videoproc_output_mode", NULL };
   struct retro_variable frametimes  = { "videoproc_frame_times", NULL };
   bool updated = false;

   /* A reset rebuilt the pipeline outside retro_run; sync the frontend with
    * whatever geometry/timing the device granted afterwards (deferred here
    * because SET_SYSTEM_AV_INFO may only be called from within retro_run). */
   if (av_info_dirty)
   {
      struct retro_system_av_info av_info;
      struct retro_system_av_info prev_av_info = video_last_av_info;
      av_info_dirty = false;
      VIDEOPROC_CORE_PREFIX(retro_get_system_av_info)(&av_info);
      if (!av_info_equal(&av_info, &prev_av_info))
         VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av_info);
   }

   if (VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
   {
      bool video_changed, audio_changed, device_changed;

      VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &videodev);
      VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &audiodev);
      VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &capturemode);
      VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &captureresolution);
      VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &capturerate);
      VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &outputmode);
      VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &frametimes);
      /* Video or Audio device(s) has(ve) been changed
       * TODO We may get away without resetting devices when changing output mode...
       */
      device_changed = videodev.value && (strcmp(video_device, videodev.value) != 0);
      video_changed = device_changed ||
          (capturemode.value && (strcmp(video_capture_mode, capturemode.value) != 0)) ||
          (captureresolution.value && (strcmp(video_capture_resolution, captureresolution.value) != 0)) ||
          (capturerate.value && (strcmp(video_capture_rate, capturerate.value) != 0)) ||
          (outputmode.value  && (strcmp(video_output_mode,  outputmode.value)  != 0));
      audio_changed = (audiodev.value && (strcmp(audio_device, audiodev.value) != 0));

      if (video_changed || audio_changed)
      {
         bool reload = true;
#ifdef HAVE_ALSA
         /* When only the audio device changed and audio capture is already
          * running, swap the ALSA device in place instead of rebuilding the
          * whole video pipeline. Enabling audio from "none" still takes the
          * reload path: SET_AUDIO_CALLBACK is only honoured during
          * retro_load_game. */
         if (!video_changed && audio_handle != NULL && audiodev.value)
         {
            close_audio_device();
            strlcpy(audio_device, audiodev.value, sizeof(audio_device));
            if (strcmp(audiodev.value, "none") != 0)
               start_audio_capture(audiodev.value);
            reload = false;
         }
#endif
         if (reload)
         {
            /* Keep the audio device alive across a video-only reconfigure so
             * the running ALSA capture isn't torn down and reopened (which
             * races its async callback and fails with EBUSY). Switching to
             * the dummy device stops audio capture, so tear it down in that
             * case. */
            bool keep_audio = !audio_changed && (audio_device[0] != '\0')
                  && videodev.value && (strcmp(videodev.value, "dummy") != 0);
            struct retro_system_av_info av_info;
            struct retro_system_av_info prev_av_info = video_last_av_info;
            unload_game_internal(keep_audio);
            /* A different capture device supports different resolutions and
             * frame rates: re-probe and re-register the options so the menu
             * lists describe the newly selected device — before the load
             * reads the values back, so a stale selection the new device's
             * list doesn't contain resets to that list's default first. */
            if (device_changed)
               register_options();
            if (!load_game_internal(false))
            {
               /* Reload failed: the buffers we just freed are gone. Output a
                * blank frame and stay idle rather than dereferencing them;
                * the user can pick a working configuration to retry. */
               VIDEOPROC_CORE_PREFIX(video_refresh_cb)(NULL, 0, 0, 0);
               return;
            }
            /* The device may have granted a different geometry or frame rate
             * than the previous configuration; push the new av_info so the
             * frontend adjusts its buffers and pacing — but only when it
             * really changed, since SET_SYSTEM_AV_INFO makes the frontend
             * reinitialize its video/audio drivers. */
            VIDEOPROC_CORE_PREFIX(retro_get_system_av_info)(&av_info);
            if (!av_info_equal(&av_info, &prev_av_info))
               VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av_info);
         }
      }

      if (frametimes.value != NULL)
         strlcpy(video_frame_times, frametimes.value, sizeof(video_frame_times));
   }

   VIDEOPROC_CORE_PREFIX(input_poll_cb)();

   /* No frame buffers means a previous (re)load left the core without a usable
    * device. Stay idle instead of dereferencing NULL buffers. */
   if (frame_cap == NULL || frame_out == NULL)
   {
      VIDEOPROC_CORE_PREFIX(video_refresh_cb)(NULL, 0, 0, 0);
      return;
   }

   /* printf("%d %d %d %s\n", video_cap_width, video_cap_height, video_buf.field, video_output_mode);
    * TODO pass frame_curr to source_* functions
    * half_feed_rate allows interlaced input to be fed at half the calls to this function
    * where the same frame is then read by the deinterlacer twice, for each field
    */
   if (video_half_feed_rate == 0)
   {
      /* Capture */
      if (strcmp(video_device, "dummy") == 0)
      {
         source_dummy(video_cap_width, video_cap_height);
      }
      else
      {
         if (strcmp(video_capture_mode, "alternate_hack") == 0)
         {
            source_v4l2_alternate_hack(video_cap_width, video_cap_height);
            processing_heal(frame_cap, video_cap_width, video_cap_height);
         }
         else
         {
            source_v4l2_normal(video_cap_width, video_cap_height);
         }
      }

      if (video_buf.field == V4L2_FIELD_INTERLACED)
         video_half_feed_rate = 1;
   }
   else
      video_half_feed_rate = 0;

   /* Converts from bgr to xrgb, deinterlacing, final copy to the outpuit buffer (frame_out)
    * Every frame except frame_cap shall be encoded in xrgb
    * Every frame except frame_out shall have the same height
    *
    * "deinterlaced" capture already delivers full frames, so there is nothing
    * to deinterlace: route it through the plain conversion below. Running the
    * deinterlacer on it would write twice the capture height into frame_out,
    * which is only video_out_height (== video_cap_height) rows tall. */
   if (   strcmp(video_output_mode, "deinterlaced") == 0
       && strcmp(video_capture_mode, "deinterlaced") != 0)
   {
      processing_bgr_xrgb(frame_cap, frame_curr, video_cap_width, video_cap_height);
      /* When deinterlacing a interlaced input, we need to process both fields of a frame,
       * one at a time (retro_run needs to be called twice, vide_half_feed_rate prevents the
       * source from being read twice...
       */
      if (strcmp(video_capture_mode, "interlaced") == 0)
      {
         enum v4l2_field field_read;
         if (video_half_feed_rate == 0)
            field_read = V4L2_FIELD_TOP;
         else
            field_read = V4L2_FIELD_BOTTOM;
         /* video_half_feed_rate will allow us to capture the interlaced frame once and run the
          * deinterlacing algo twice, extracting a given field for each run.
          */
         processing_deinterlacing_crap(frame_curr, frame_out, video_cap_width, video_cap_height/2, field_read, 1);
      }
      else
      {
         processing_deinterlacing_crap(frame_curr, frame_out, video_cap_width, video_cap_height, (enum v4l2_field)video_buf.field, 0);
      }
      aux         = frame_prev3;
      frame_prev3 = frame_prev2;
      frame_prev2 = frame_prev1;
      frame_prev1 = frame_curr;
      frame_curr  = aux;

      VIDEOPROC_CORE_PREFIX(video_refresh_cb)(frame_out, video_cap_width,
         video_out_height, video_cap_width * sizeof(uint32_t));
   }
   else if (strcmp(video_capture_mode, "alternate_hack") == 0)
   {
      /* Case where alternate_hack without deinterlacing would
       * not generate previous frame for processing_heal */
      processing_bgr_xrgb(frame_cap, frame_curr, video_cap_width, video_cap_height);
      aux         = frame_prev3;
      frame_prev3 = frame_prev2;
      frame_prev2 = frame_prev1;
      frame_prev1 = frame_curr;
      frame_curr  = aux;

      aux         = frame_out;
      frame_out   = frame_curr;

      VIDEOPROC_CORE_PREFIX(video_refresh_cb)(frame_out, video_cap_width,
         video_out_height, video_cap_width * sizeof(uint32_t));

      frame_out   = aux;
   }
   else
   {
      processing_bgr_xrgb(frame_cap, frame_out, video_cap_width, video_out_height);

      VIDEOPROC_CORE_PREFIX(video_refresh_cb)(frame_out, video_cap_width,
         video_out_height, video_cap_width * sizeof(uint32_t));
   }
}

RETRO_API size_t VIDEOPROC_CORE_PREFIX(retro_serialize_size)(void)
{ return 0; }
RETRO_API bool VIDEOPROC_CORE_PREFIX(retro_serialize)(
      void *data, size_t len) { return false; }
RETRO_API bool VIDEOPROC_CORE_PREFIX(retro_unserialize)(
      const void *data, size_t len) { return false; }
RETRO_API void VIDEOPROC_CORE_PREFIX(retro_cheat_reset)(void) { }
RETRO_API void VIDEOPROC_CORE_PREFIX(retro_cheat_set)(unsigned a,
      bool b, const char *c) { }

#if 0
static void videoinput_set_control_v4l2( uint32_t id, double val)
{
    struct v4l2_queryctrl query;

    query.id = id;
    if(ioctl(video_device_fd, VIDIOC_QUERYCTRL, &query) >= 0 && !(query.flags & V4L2_CTRL_FLAG_DISABLED))
    {
        struct v4l2_control control;
        control.id = id;
        control.value = query.minimum + ((int) ((val * ((double) (query.maximum - query.minimum))) + 0.5));
        ioctl(video_device_fd, VIDIOC_S_CTRL, &control);
    }
}
#endif

/* Device bring-up shared by retro_load_game and the runtime-reconfigure path
 * in retro_run. allow_fallback selects the failure policy: on the initial
 * load a failing device falls back to the built-in test pattern (the device
 * choice is persisted, and a broken one would otherwise prevent the core
 * from ever starting again), while a failed reconfigure of a running core is
 * survivable as a plain error (blank frames, options stay editable). */
static bool load_game_internal(bool allow_fallback)
{
   struct retro_variable videodev     = { "videoproc_videodev", NULL };
   struct retro_variable audiodev     = { "videoproc_audiodev", NULL };
   struct retro_variable capture_mode = { "videoproc_capture_mode", NULL };
   struct retro_variable capture_resolution = { "videoproc_capture_resolution", NULL };
   struct retro_variable capture_rate = { "videoproc_capture_rate", NULL };
   struct retro_variable output_mode  = { "videoproc_output_mode", NULL };
   struct retro_variable frame_times  = { "videoproc_frame_times", NULL };
   enum retro_pixel_format pixel_format;
   struct v4l2_standard std;
   struct v4l2_streamparm parm;
   struct v4l2_requestbuffers reqbufs;
   struct v4l2_buffer buf;
   struct v4l2_format fmt;
   enum v4l2_buf_type type;
   v4l2_std_id std_id;
   uint32_t index;
   uint32_t rate_n, rate_d;
   bool std_found;
   int error;
#ifdef HAVE_ALSA
   bool had_audio = (audio_handle != NULL);
#endif

   if (open_devices() == false)
   {
      /* Nothing to clean up here: open_devices() leaves no half-open video
       * fd behind, and the fallback/error paths below release whatever else
       * is held. */
      printf("Couldn't open capture device\n");
      goto device_fallback;
   }

#ifdef HAVE_ALSA
   /* Only (re)register the async audio callback when the audio device was
    * freshly opened. On a video-only reconfigure the device (and its already
    * registered callback) is kept alive, so re-registering would needlessly
    * restart it. */
   if (!had_audio && audio_handle != NULL)
   {
       struct retro_audio_callback audio_cb;
       audio_cb.callback = audio_callback;
       audio_cb.set_state = audio_set_state;
       VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK, &audio_cb);
   }
#endif

   VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &videodev);
   if (videodev.value == NULL)
      goto error;
   strlcpy(video_device, videodev.value, sizeof(video_device));

   /* Audio device is optional... */
   VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &audiodev);
   if (audiodev.value != NULL)
      strlcpy(audio_device, audiodev.value, sizeof(audio_device));

   VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &capture_resolution);
   if(capture_resolution.value != NULL)
      strlcpy(video_capture_resolution, capture_resolution.value, sizeof(video_capture_resolution));
   else
      strlcpy(video_capture_resolution, "auto", sizeof(video_capture_resolution));

   VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &capture_rate);
   if(capture_rate.value != NULL)
      strlcpy(video_capture_rate, capture_rate.value, sizeof(video_capture_rate));
   else
      strlcpy(video_capture_rate, "auto", sizeof(video_capture_rate));

   VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &capture_mode);
   VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &output_mode);
   if (capture_mode.value == NULL || output_mode.value == NULL)
      goto error;
   strlcpy(video_capture_mode, capture_mode.value, sizeof(video_capture_mode));
   strlcpy(video_output_mode,  output_mode.value,  sizeof(video_output_mode));

   VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &frame_times);
   if (frame_times.value != NULL)
      strlcpy(video_frame_times, frame_times.value, sizeof(video_frame_times));

   /* Reset per-device timing and pacing state so nothing leaks from a
    * previously loaded device into av_info/retro_get_region, and a stale
    * half-feed flag from an interlaced session doesn't skip the first
    * capture of the new one. */
   memset(&video_standard, 0, sizeof(video_standard));
   video_cap_rate_n     = 0;
   video_cap_rate_d     = 0;
   video_half_feed_rate = 0;

   if (strcmp(video_device, "dummy") == 0)
      setup_dummy_source();
   else
   {
      memset(&fmt, 0, sizeof(fmt));
      fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

      error    = v4l2_ioctl(video_device_fd, VIDIOC_G_FMT, &fmt);

      if (error != 0)
      {
         printf("VIDIOC_G_FMT failed: %s\n", strerror(errno));
         goto device_fallback;
      }

      fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24;
      fmt.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
      fmt.fmt.pix.quantization = V4L2_QUANTIZATION_LIM_RANGE;

      /* Applied set resolution if not set to auto */
      if (strcmp(video_capture_resolution, "auto") != 0)
      {
         unsigned width, height;
         if (   sscanf(video_capture_resolution, "%ux%u", &width, &height) == 2
             && width != 0 && height != 0)
         {
            fmt.fmt.pix.width  = width;
            fmt.fmt.pix.height = height;
         }
         else
            printf("Invalid capture resolution \"%s\", using device default\n",
                  video_capture_resolution);
      }
      fmt.fmt.pix.field   = V4L2_FIELD_TOP;
      v4l2_ncapbuf_target = 2;
      /* TODO Query the size and FPS */
      if (strcmp(video_capture_mode, "interlaced") == 0)
         fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
      else if (strcmp(video_capture_mode, "alternate") == 0)
         fmt.fmt.pix.field = V4L2_FIELD_ALTERNATE;
      else if (strcmp(video_capture_mode, "top") == 0)
         fmt.fmt.pix.field = V4L2_FIELD_TOP;
      else if (strcmp(video_capture_mode, "bottom") == 0)
         fmt.fmt.pix.field = V4L2_FIELD_BOTTOM;
      else if (strcmp(video_capture_mode, "alternate_hack") == 0)
      {
         fmt.fmt.pix.field   = V4L2_FIELD_TOP;
         v4l2_ncapbuf_target = 1;
      }

      error = v4l2_ioctl(video_device_fd, VIDIOC_S_FMT, &fmt);
      if (error != 0)
      {
         printf("VIDIOC_S_FMT failed: %s\n", strerror(errno));
         goto device_fallback;
      }

      /* S_FMT does not fail on an unsupported resolution: the driver silently
       * clamps width/height (and may even change the pixelformat) to the
       * nearest mode it supports and returns success. Derive every buffer
       * dimension from the *granted* fmt, never from what we requested, so the
       * conversion buffer (video_cap_width * video_cap_height * 3) and the
       * source_v4l2_* memcpy always match the V4L2 buffer the driver actually
       * allocated (otherwise: heap overflow / crash, see issue #16457). */
      video_cap_width = fmt.fmt.pix.width;
      switch (fmt.fmt.pix.field)
      {
         case V4L2_FIELD_TOP:
         case V4L2_FIELD_BOTTOM:
         case V4L2_FIELD_ALTERNATE:
            /* The driver granted a per-field format: pix.height is already
             * the height of a single field. */
            video_cap_height = fmt.fmt.pix.height;
            break;
         default:
            /* The driver delivers whole frames; in the field-based capture
             * modes only one half-height field of each frame is consumed. */
            if (   strcmp(video_capture_mode, "interlaced")   == 0
                || strcmp(video_capture_mode, "deinterlaced") == 0)
               video_cap_height = fmt.fmt.pix.height;
            else
               video_cap_height = fmt.fmt.pix.height / 2;
            break;
      }

      /* Prefer a constant frame rate over auto-exposure: cameras (webcams)
       * with "exposure auto priority" enabled (exposure_dynamic_framerate in
       * v4l2-ctl; browsers/meeting software often switch it on and it sticks
       * until the camera is replugged) may extend the exposure time beyond
       * the frame interval in dim light. The camera then silently delivers
       * only ~10-15 fps while G_PARM still reports the nominal rate, and the
       * whole frontend paces on the slow delivery — everything (menu,
       * shaders) is slow/laggy/choppy except the capture picture itself.
       * Disabling the priority makes the camera clamp exposure to the frame
       * interval instead. HDMI/SDI grabbers have no exposure and don't
       * implement the control; the failed ioctl is simply ignored. */
      {
         struct v4l2_control ctrl;
         memset(&ctrl, 0, sizeof(ctrl));
         ctrl.id    = V4L2_CID_EXPOSURE_AUTO_PRIORITY;
         ctrl.value = 0;
         if (v4l2_ioctl(video_device_fd, VIDIOC_S_CTRL, &ctrl) == 0)
            printf("Disabled exposure auto priority (constant frame rate)\n");
      }

      /* Frame rate is controlled/reported via VIDIOC_G/S_PARM (timeperframe),
       * independent of analog TV standards. Apply the chosen rate (if any),
       * then read back what the driver actually granted so the timing reported
       * to the frontend matches the real capture rate. For modern UVC/HDMI
       * devices (no analog standard) this is the only source of the rate. */
      memset(&parm, 0, sizeof(parm));
      parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      error = v4l2_ioctl(video_device_fd, VIDIOC_G_PARM, &parm);
      if (   error == 0
          && (parm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)
          && rate_to_timeperframe(video_capture_rate, &rate_n, &rate_d))
      {
         parm.parm.capture.timeperframe.numerator   = rate_n;
         parm.parm.capture.timeperframe.denominator = rate_d;
         if (v4l2_ioctl(video_device_fd, VIDIOC_S_PARM, &parm) != 0)
            printf("VIDIOC_S_PARM failed (non-fatal): %s\n", strerror(errno));

         /* Re-read the granted interval (S_PARM clamps to a supported rate). */
         memset(&parm, 0, sizeof(parm));
         parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
         error = v4l2_ioctl(video_device_fd, VIDIOC_G_PARM, &parm);
      }
      if (error == 0)
      {
         video_cap_rate_n = parm.parm.capture.timeperframe.numerator;
         video_cap_rate_d = parm.parm.capture.timeperframe.denominator;
         if (video_cap_rate_n != 0 && video_cap_rate_d != 0)
            printf("Capture frame rate %.3f fps\n",
                  (double)video_cap_rate_d / (double)video_cap_rate_n);
      }
      else
         printf("VIDIOC_G_PARM failed (non-fatal): %s\n", strerror(errno));

      /* Query the analog TV standard (PAL/NTSC) for the frame rate. Modern
       * HDMI/USB capture devices generally don't implement analog standards,
       * so any failure here is non-fatal: fall back to the default timing in
       * retro_get_system_av_info. */
      error = v4l2_ioctl(video_device_fd, VIDIOC_G_STD, &std_id);
      if (error != 0)
      {
         if (errno == ENOTTY)
            printf("Analog TV standards not supported by this device"
                  " (normal for HDMI/USB capture)\n");
         else
            printf("VIDIOC_G_STD failed (non-fatal): %s\n", strerror(errno));
      }
      else
      {
         for (index = 0, std_found = false; ; index++)
         {
            memset(&std, 0, sizeof(std));
            std.index = index;
            error     = v4l2_ioctl(video_device_fd, VIDIOC_ENUMSTD, &std);
            if (error)
               break;
            if (std.id == std_id)
            {
               video_standard = std;
               std_found      = true;
            }
            printf("VIDIOC_ENUMSTD[%u]: %s%s\n", index, std.name, std.id == std_id ? " [*]" : "");
         }
         if (!std_found)
            printf("VIDIOC_ENUMSTD did not contain std ID %08x (non-fatal)\n", (unsigned)std_id);
      }
      video_format = fmt;
      /* fmt still holds the device's full frame height, but for the
       * non-"interlaced"/non-"deinterlaced" capture modes we only copy a
       * half-height field into frame_cap (video_cap_height). Keep
       * video_format in sync with the buffer that is actually allocated
       * (video_cap_width * video_cap_height * 3) to avoid a heap overflow
       * in the source_v4l2_* memcpy (see issue #16457). */
      video_format.fmt.pix.width  = video_cap_width;
      video_format.fmt.pix.height = video_cap_height;
      /* TODO Check if what we got is indeed what we asked for */

      memset(&reqbufs, 0, sizeof(reqbufs));
      reqbufs.count  = v4l2_ncapbuf_target;
      reqbufs.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      reqbufs.memory = V4L2_MEMORY_MMAP;

      error          = v4l2_ioctl(video_device_fd, VIDIOC_REQBUFS, &reqbufs);
      if (error != 0)
      {
         printf("VIDIOC_REQBUFS failed: %s\n", strerror(errno));
         goto device_fallback;
      }
      /* The driver can grant more buffers than requested (some need 3+ to
       * operate); v4l2_capbuf[] only holds VIDEO_BUFFERS_MAX entries, and
       * only the buffers actually mapped and queued here are ever returned
       * by VIDIOC_DQBUF. Going past the array corrupts adjacent state. */
      v4l2_ncapbuf = MIN(reqbufs.count, VIDEO_BUFFERS_MAX);
      printf("GOT v4l2_ncapbuf=%" PRI_SIZET "\n", v4l2_ncapbuf);

      /* Forget any previous session's pointers before the mmap loop so a
       * mid-loop failure never releases stale mappings. */
      memset(v4l2_capbuf, 0, sizeof(v4l2_capbuf));

      for (index = 0; index < v4l2_ncapbuf; index++)
      {
         memset(&buf, 0, sizeof(buf));
         buf.index  = index;
         buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
         buf.memory = V4L2_MEMORY_MMAP;

         error      = v4l2_ioctl(video_device_fd, VIDIOC_QUERYBUF, &buf);
         if (error != 0)
         {
            printf("VIDIOC_QUERYBUF failed for %u: %s\n", index, strerror(errno));
            goto device_fallback;
         }

         v4l2_capbuf[index].len   = buf.length;
         v4l2_capbuf[index].start = v4l2_mmap(NULL, buf.length,
               PROT_READ|PROT_WRITE, MAP_SHARED, video_device_fd, buf.m.offset);
         if (v4l2_capbuf[index].start == MAP_FAILED)
         {
            printf("v4l2_mmap failed: %s\n", strerror(errno));
            goto device_fallback;
         }
      }

      for (index = 0; index < v4l2_ncapbuf; index++)
      {
         memset(&buf, 0, sizeof(buf));
         buf.index  = index;
         buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
         buf.memory = V4L2_MEMORY_MMAP;

         error      = v4l2_ioctl(video_device_fd, VIDIOC_QBUF, &buf);
         if (error != 0)
         {
            printf("VIDIOC_QBUF failed for %u: %s\n", index,
                  strerror(errno));
            goto device_fallback;
         }
      }

      type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      error = v4l2_ioctl(video_device_fd, VIDIOC_STREAMON, &type);
      if (error != 0)
      {
         printf("VIDIOC_STREAMON failed: %s\n", strerror(errno));
         goto device_fallback;
      }

      /* videoinput_set_control_v4l2(V4L2_CID_HUE, (double) 0.4f); */
   }

device_ready:
   /* Never report or allocate zero-sized frames: a zero-height geometry
    * passed on to the frontend crashes its video driver (GPU fault). */
   if (video_cap_width == 0 || video_cap_height == 0)
   {
      printf("Invalid capture dimensions %ux%u\n",
            video_cap_width, video_cap_height);
      goto error;
   }

   /* TODO/FIXME Framerates?
    * Each frame should combine both fields into a full frame
    * (if not already captured interlaced), half frame-rate
    */
   if (strcmp(video_output_mode, "interlaced") == 0)
   {
      if (strcmp(video_capture_mode, "interlaced") == 0)
         video_out_height = video_cap_height;
      else
      {
         printf("WARNING: Capture mode %s with output mode %s is not"
               " properly supported yet... (Is this even useful?)\n", \
                 video_capture_mode, video_output_mode);
         /* Pass the frames through at capture height: the conversion reads
          * from frame_cap, which only holds video_cap_height rows (doubling
          * here made it read past the end of the conversion buffer). */
         video_out_height = video_cap_height;
      }
      /* Each frame has one field, full frame-rate */
   }
   /* Each frame has one or both field to be deinterlaced
    * into a full frame (double the lines if one field), full frame-rate */
   else if (strcmp(video_output_mode, "progressive") == 0)
      video_out_height = video_cap_height;
   else if (strcmp(video_output_mode, "deinterlaced") == 0)
   {
      if (strcmp(video_capture_mode, "interlaced") == 0 || strcmp(video_capture_mode, "deinterlaced") == 0)
         video_out_height = video_cap_height;
      else
         video_out_height = video_cap_height*2;
   }
   else
      video_out_height = video_cap_height;

   printf("Capture Resolution %ux%u\n", video_cap_width, video_cap_height);
   printf("Output Resolution %ux%u\n", video_cap_width, video_out_height);

   frame_cap = (uint8_t*)calloc (1, video_cap_width * video_cap_height * sizeof(uint8_t) * 3);
   frame_out = (uint32_t*)calloc(1, video_cap_width * video_out_height * sizeof(uint32_t));
   /* TODO: Only allocate frames if we are going to use it (for deinterlacing or other filters?) */
   frames[0] = (uint32_t*)calloc(1, video_cap_width * video_out_height * sizeof(uint32_t));
   frames[1] = (uint32_t*)calloc(1, video_cap_width * video_out_height * sizeof(uint32_t));
   frames[2] = (uint32_t*)calloc(1, video_cap_width * video_out_height * sizeof(uint32_t));
   frames[3] = (uint32_t*)calloc(1, video_cap_width * video_out_height * sizeof(uint32_t));

   frame_curr  = frames[0];
   frame_prev1 = frames[1];
   frame_prev2 = frames[2];
   frame_prev3 = frames[3];

   if (   !frame_out  || !frame_cap
       || !frames[0] || !frames[1] || !frames[2] || !frames[3])
      goto error;

   printf("Allocated %" PRI_SIZET " byte conversion buffer\n",
         (size_t)(video_cap_width * video_cap_height) * sizeof(uint32_t));

   pixel_format = RETRO_PIXEL_FORMAT_XRGB8888;
   if (!VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &pixel_format))
   {
      printf("Cannot set pixel format\n");
      goto error;
   }

   return true;

device_fallback:
   if (!allow_fallback)
      goto error;
   printf("Capture device could not be started, falling back to the dummy video source\n");
   {
      struct retro_message msg =
            { "Capture device could not be started - showing the test pattern", 240 };
      VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
   }
   release_video_buffers();
   close_video_device();
   /* The reconfigure path in retro_run tears audio down when the user
    * switches to the dummy source; falling back to it behaves the same
    * instead of leaving an audio capture running under the test pattern.
    * audio_device[] is kept so selecting a working video device later
    * brings audio back up. */
   unregister_audio_callback();
   close_audio_device();
   strlcpy(video_device, "dummy", sizeof(video_device));
   setup_dummy_source();
   {
      /* Reflect the fallback in the core option so the menu shows what is
       * actually running. */
      struct retro_variable var;
      var.key   = "videoproc_videodev";
      var.value = "dummy";
      VIDEOPROC_CORE_PREFIX(environment_cb)(RETRO_ENVIRONMENT_SET_VARIABLE, &var);
   }
   goto device_ready;

error:
   /* Leave no stale state behind: a later option change runs
    * unload_game_internal() again, which must not munmap old buffer
    * pointers or touch a dead ALSA handle, and retro_run must see the
    * NULL frame buffers and stay idle (a partial allocation failure
    * would otherwise leave a half-usable buffer set around). */
   unregister_audio_callback();
   release_video_buffers();
   release_frame_buffers();
   close_devices();
   return false;
}

RETRO_API bool VIDEOPROC_CORE_PREFIX(retro_load_game)(const struct retro_game_info *game)
{
   /* This core ignores the content argument (SET_SUPPORT_NO_GAME). */
   return load_game_internal(true);
}

/* keep_audio leaves the audio device, its async callback and audio_device[]
 * untouched. Used for a video-only reconfigure (resolution/capture mode/output
 * mode change) so the running ALSA capture isn't bounced; see open_devices(). */
static void unload_game_internal(bool keep_audio)
{
   struct v4l2_requestbuffers reqbufs;

   if (!keep_audio)
      unregister_audio_callback();

   if ((strcmp(video_device, "dummy") != 0) && (video_device_fd != -1))
   {
      enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      int               error = v4l2_ioctl(video_device_fd,
            VIDIOC_STREAMOFF, &type);
      if (error != 0)
         printf("VIDIOC_STREAMOFF failed: %s\n", strerror(errno));

      release_video_buffers();

      reqbufs.count = 0;
      reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      reqbufs.memory = V4L2_MEMORY_MMAP;
      error = v4l2_ioctl(video_device_fd, VIDIOC_REQBUFS, &reqbufs);
      if (error != 0)
         printf("VIDIOC_REQBUFS failed: %s\n", strerror(errno));

   }

   release_frame_buffers();

   if (ft_info)
   {
      free(ft_info);
      ft_info = NULL;
   }

   if (ft_info2)
   {
      free(ft_info2);
      ft_info2 = NULL;
   }

   close_video_device();
   video_device[0] = '\0';
   if (!keep_audio)
   {
      close_audio_device();
      audio_device[0] = '\0';
   }
}

RETRO_API void VIDEOPROC_CORE_PREFIX(retro_unload_game)(void)
{
   unload_game_internal(false);
}

RETRO_API bool VIDEOPROC_CORE_PREFIX(retro_load_game_special)(unsigned game_type,
      const struct retro_game_info *info, size_t num_info) { return false; }

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

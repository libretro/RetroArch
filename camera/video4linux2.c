/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
 *  Copyright (C) 2012-2013 - Michael Lelli
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
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include "../general.h"
#include "../driver.h"
#include "../performance.h"
#include "../miscellaneous.h"
#include "../gfx/scaler/scaler.h"
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "../compat/strl.h"

#include <asm/types.h>
#include <linux/videodev2.h>

struct buffer
{
   void *start;
   size_t length;
};

typedef struct video4linux
{
   int fd;
   struct buffer *buffers;
   unsigned n_buffers;
   unsigned width;
   unsigned height;
   size_t pitch;

   struct scaler_ctx scaler;
   uint32_t *buffer_output;
   bool ready;

   char dev_name[PATH_MAX];
} video4linux_t;

static void process_image(video4linux_t *v4l, const uint8_t *buffer_yuv)
{
   RETRO_PERFORMANCE_INIT(yuv_convert_direct);
   RETRO_PERFORMANCE_START(yuv_convert_direct);
   scaler_ctx_scale(&v4l->scaler, v4l->buffer_output, buffer_yuv);
   RETRO_PERFORMANCE_STOP(yuv_convert_direct);
}

static int xioctl(int fd, int request, void *args)
{
   int r;

   do
   {
      r = ioctl(fd, request, args);
   } while (r == -1 && errno == EINTR);

   return r;
}

static bool init_mmap(void *data)
{
   struct v4l2_requestbuffers req;
   video4linux_t *v4l = (video4linux_t*)data;

   memset(&req, 0, sizeof(req));

   req.count = 4;
   req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   req.memory = V4L2_MEMORY_MMAP;

   if (xioctl(v4l->fd, VIDIOC_REQBUFS, &req) == -1)
   {
      if (errno == EINVAL)
      {
         RARCH_ERR("%s does not support memory mapping.\n", v4l->dev_name);
         return false;
      }
      else
      {
         RARCH_ERR("xioctl of VIDIOC_REQBUFS failed.\n");
         return false;
      }
   }

   if (req.count < 2)
   {
      RARCH_ERR("Insufficient buffer memory on %s.\n", v4l->dev_name);
      return false;
   }

   v4l->buffers = (struct buffer*)calloc(req.count, sizeof(*v4l->buffers));

   if (!v4l->buffers)
   {
      RARCH_ERR("Out of memory allocating V4L2 buffers.\n");
      return false;
   }

   for (v4l->n_buffers = 0; v4l->n_buffers < req.count; v4l->n_buffers++)
   {
      struct v4l2_buffer buf;

      memset(&buf, 0, sizeof(buf));

      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;
      buf.index = v4l->n_buffers;

      if (xioctl(v4l->fd, VIDIOC_QUERYBUF, &buf) == -1)
      {
         RARCH_ERR("Error - xioctl VIDIOC_QUERYBUF.\n");
         return false;
      }

      v4l->buffers[v4l->n_buffers].length = buf.length;
      v4l->buffers[v4l->n_buffers].start = mmap(NULL,
            buf.length, PROT_READ | PROT_WRITE,
            MAP_SHARED,
            v4l->fd, buf.m.offset);

      if (v4l->buffers[v4l->n_buffers].start == MAP_FAILED)
      {
         RARCH_ERR("Error - mmap.\n");
         return false;
      }
   }

   return true;
}

static bool init_device(void *data)
{
   struct v4l2_capability cap;
   struct v4l2_cropcap cropcap;
   struct v4l2_crop crop;
   struct v4l2_format fmt;
   unsigned min;
   video4linux_t *v4l = (video4linux_t*)data;

   if (xioctl(v4l->fd, VIDIOC_QUERYCAP, &cap) < 0)
   {
      if (errno == EINVAL)
      {
         RARCH_ERR("%s is no V4L2 device.\n", v4l->dev_name);
         return false;
      }
      else
      {
         RARCH_ERR("Error - VIDIOC_QUERYCAP.\n");
         return false;
      }
   }

   if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
   {
      RARCH_ERR("%s is no video capture device.\n", v4l->dev_name);
      return false;
   }

   if (!(cap.capabilities & V4L2_CAP_STREAMING))
   {
      RARCH_ERR("%s does not support streaming I/O (V4L2_CAP_STREAMING).\n", v4l->dev_name);
      return false;
   }

   memset(&cropcap, 0, sizeof(cropcap));
   cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

   if (xioctl(v4l->fd, VIDIOC_CROPCAP, &cropcap) == 0)
   {
      crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      crop.c = cropcap.defrect;
      // Ignore errors here.
      xioctl(v4l->fd, VIDIOC_S_CROP, &crop);
   }

   memset(&fmt, 0, sizeof(fmt));

   fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   fmt.fmt.pix.width  = v4l->width;
   fmt.fmt.pix.height = v4l->height;
   fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
   fmt.fmt.pix.field = V4L2_FIELD_NONE;

   if (xioctl(v4l->fd, VIDIOC_S_FMT, &fmt) < 0)
   {
      RARCH_ERR("Error - VIDIOC_S_FMT\n");
      return false;
   }

   // VIDIOC_S_FMT may change width, height and pitch.
   v4l->width = fmt.fmt.pix.width;
   v4l->height = fmt.fmt.pix.height;
   v4l->pitch = max(fmt.fmt.pix.bytesperline, v4l->width * 2);

   // Sanity check to see if our assumptions are met.
   // It is possible to support whatever the device gives us,
   // but this dramatically increases complexity.
   if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV)
   {
      RARCH_ERR("The V4L2 device doesn't support YUYV.\n");
      return false;
   }

   if (fmt.fmt.pix.field != V4L2_FIELD_NONE && fmt.fmt.pix.field != V4L2_FIELD_INTERLACED)
   {
      RARCH_ERR("The V4L2 device doesn't support progressive nor interlaced video.\n");
      return false;
   }

   RARCH_LOG("V4L2 device: %u x %u.\n", v4l->width, v4l->height);

   return init_mmap(v4l);
}

static void v4l_stop(void *data)
{
   video4linux_t *v4l = (video4linux_t*)data;
   enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

   if (xioctl(v4l->fd, VIDIOC_STREAMOFF, &type) == -1)
      RARCH_ERR("Error - VIDIOC_STREAMOFF.\n");

   v4l->ready = false;
}

static bool v4l_start(void *data)
{
   video4linux_t *v4l = (video4linux_t*)data;
   unsigned i;
   enum v4l2_buf_type type;

   for (i = 0; i < v4l->n_buffers; i++)
   {
      struct v4l2_buffer buf;

      memset(&buf, 0, sizeof(buf));

      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;
      buf.index = i;

      if (xioctl(v4l->fd, VIDIOC_QBUF, &buf) == -1)
      {
         RARCH_ERR("Error - VIDIOC_QBUF.\n");
         return false;
      }
   }

   type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

   if (xioctl(v4l->fd, VIDIOC_STREAMON, &type) == -1)
   {
      RARCH_ERR("Error - VIDIOC_STREAMON.\n");
      return false;
   }

   v4l->ready = true;

   return true;
}

static void v4l_free(void *data)
{
   video4linux_t *v4l = (video4linux_t*)data;

   unsigned i;
   for (i = 0; i < v4l->n_buffers; i++)
      if (munmap(v4l->buffers[i].start, v4l->buffers[i].length) == -1)
         RARCH_ERR("munmap failed.\n");

   if (v4l->fd >= 0)
      close(v4l->fd);

   free(v4l->buffer_output);
   scaler_ctx_gen_reset(&v4l->scaler);
   free(v4l);
}

static void *v4l_init(const char *device, uint64_t caps, unsigned width, unsigned height)
{
   struct stat st;

   if ((caps & (1ULL << RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER)) == 0)
   {
      RARCH_ERR("video4linux2 returns raw framebuffers.\n");
      return NULL;
   }

   video4linux_t *v4l = (video4linux_t*)calloc(1, sizeof(video4linux_t));
   if (!v4l)
      return NULL;

   strlcpy(v4l->dev_name, device ? device : "/dev/video0", sizeof(v4l->dev_name));

   v4l->width  = width;
   v4l->height = height;
   v4l->ready  = false;

   if (stat(v4l->dev_name, &st) == -1)
   {
      RARCH_ERR("Cannot identify '%s' : %d, %s\n", v4l->dev_name, errno, strerror(errno));
      goto error;
   }

   if (!S_ISCHR(st.st_mode))
   {
      RARCH_ERR("%s is no device.\n", v4l->dev_name);
      goto error;
   }

   v4l->fd = open(v4l->dev_name, O_RDWR | O_NONBLOCK, 0);

   if (v4l->fd == -1)
   {
      RARCH_ERR("Cannot open '%s': %d, %s\n", v4l->dev_name, errno, strerror(errno));
      goto error;
   }

   if (!init_device(v4l))
      goto error;

   v4l->buffer_output = (uint32_t*)malloc(v4l->width * v4l->height * sizeof(uint32_t));
   if (!v4l->buffer_output)
   {
      RARCH_ERR("Failed to allocate output buffer.\n");
      goto error;
   }

   v4l->scaler.in_width = v4l->scaler.out_width = v4l->width;
   v4l->scaler.in_height = v4l->scaler.out_height = v4l->height;
   v4l->scaler.in_fmt = SCALER_FMT_YUYV;
   v4l->scaler.out_fmt = SCALER_FMT_ARGB8888;
   v4l->scaler.in_stride = v4l->pitch;
   v4l->scaler.out_stride = v4l->width * 4;

   if (!scaler_ctx_gen_filter(&v4l->scaler))
   {
      RARCH_ERR("Failed to create scaler.\n");
      goto error;
   }

   return v4l;

error:
   RARCH_ERR("V4L2: Failed to initialize camera.\n");
   v4l_free(v4l);
   return NULL;
}

static bool preprocess_image(void *data)
{
   video4linux_t *v4l = (video4linux_t*)data;
   struct v4l2_buffer buf;
   unsigned i;

   memset(&buf, 0, sizeof(buf));

   buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   buf.memory = V4L2_MEMORY_MMAP;

   if (xioctl(v4l->fd, VIDIOC_DQBUF, &buf) == -1)
   {
      switch (errno)
      {
         case EAGAIN:
            return false;
         default:
            RARCH_ERR("VIDIOC_DQBUF.\n");
            return false;
      }
   }

   rarch_assert(buf.index < v4l->n_buffers);

   process_image(v4l, (const uint8_t*)v4l->buffers[buf.index].start);

   if (xioctl(v4l->fd, VIDIOC_QBUF, &buf) == -1)
      RARCH_ERR("VIDIOC_QBUF\n");

   return true;
}

static bool v4l_poll(void *data, retro_camera_frame_raw_framebuffer_t frame_raw_cb,
      retro_camera_frame_opengl_texture_t frame_gl_cb)
{
   video4linux_t *v4l = (video4linux_t*)data;
   if (!v4l->ready)
      return false;

   (void)frame_gl_cb;

   if (preprocess_image(data))
   {
      if (frame_raw_cb != NULL)
         frame_raw_cb(v4l->buffer_output, v4l->width, v4l->height, v4l->width * 4);
      return true;
   }
   else
      return false;
}

const camera_driver_t camera_v4l2 = {
   v4l_init,
   v4l_free,
   v4l_start,
   v4l_stop,
   v4l_poll,
   "video4linux2",
};


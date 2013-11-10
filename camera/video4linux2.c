/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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
#include "../driver.h"
#include "../miscellaneous.h"
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/types.h>

#include <linux/videodev2.h>

typedef enum
{
   IO_METHOD_READ,
   IO_METHOD_MMAP,
   IO_METHOD_USERPTR,
} io_method;

struct buffer
{
   void *start;
   size_t length;
};

typedef struct video4linux
{
   char *dev_name;
   int fd;
   bool ready;
   io_method io;
   struct buffer *buffers;
   unsigned n_buffers;
   size_t width;
   size_t height;
} video4linux_t;

// FIXME: Shouldn't use LUTs for this.
// The LUT is simply too big, and the conversion can be done efficiently with fixed-point SIMD anyways.
/* 
 * YCbCr to RGB lookup table
 * Y, Cb, Cr range is 0-255
 *
 * Stored value bits:
 *   24-16 Red
 *   15-8  Green
 *   7-0   Blue
 */
#define YUV_SHIFT(y, cb, cr) ((y << 16) | (cb << 8) | (cr << 0))
#define RGB_SHIFT(r, g, b) ((r << 16) | (g << 8) | (b << 0))
static uint32_t *YCbCr_to_RGB;

static void generate_YCbCr_to_RGB_lookup(void)
{
   int y;
   int cb;
   int cr;

   YCbCr_to_RGB = (uint32_t*)realloc(YCbCr_to_RGB, 256 * 256 * 256 * sizeof(uint32_t));
   if (!YCbCr_to_RGB)
      return;

   for (y = 0; y < 256; y++)
   {
      for (cb = 0; cb < 256; cb++)
      {
         for (cr = 0; cr < 256; cr++)
         {
            double Y = (double)y;
            double Cb = (double)cb;
            double Cr = (double)cr;

            int R = (int)(Y + 1.40200 * (Cr - 0x80));
            int G = (int)(Y - 0.34414 * (Cb - 0x80) - 0.71414 * (Cr - 0x80));
            int B = (int)(Y + 1.77200 * (Cb - 0x80));

            R = max(0, min(255, R));
            G = max(0, min(255, G));
            B = max(0, min(255, B));

            YCbCr_to_RGB[YUV_SHIFT(y, cb, cr)] = RGB_SHIFT(R, G, B);
         }
      }
   }
}

/**
 *  Converts YUV422 to RGB
 *  Before first use call generate_YCbCr_to_RGB_lookup();
 *
 *  input is pointer to YUV422 encoded data in following order: Y0, Cb, Y1, Cr.
 *  output is pointer to 24 bit RGB buffer.
 *  Output data is written in following order: R1, G1, B1, R2, G2, B2.
 */

// FIXME: Software CPU color conersion from YUV to RGB - we'll make two codepaths
// eventually - GL binding to texture and color conversion through shaders,
// and this approach

static inline void YUV422_to_RGB(uint32_t *output, const uint8_t *input)
{
   uint8_t y0 = input[0];
   uint8_t cb = input[1];
   uint8_t y1 = input[2];
   uint8_t cr = input[3];

   output[0] = YCbCr_to_RGB[YUV_SHIFT(y0, cb, cr)];
   output[1] = YCbCr_to_RGB[YUV_SHIFT(y1, cb, cr)];
}

static void process_image(const void *p)
{
   (void)p;
   //FIXME - fill in here how we are going to render
   //this - could have two codepaths - one for GL
   //and one non-GL
#if 0
   const uint8_t *buffer_yuv = p;

   size_t x;
   size_t y;

   for (y = 0; y < height; y++)
      for (x = 0; x < width; x += 2)
         YUV422_to_RGB(buffer_sdl + (y * width + x) * 3,
               buffer_yuv + (y * width + x) * 2);

   render(data_sf);
#endif
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

static int init_read(void *data, unsigned int buffer_size)
{
   video4linux_t *v4l = (video4linux_t*)data;
   v4l->buffers = (struct buffer*)calloc(1, sizeof(*v4l->buffers));

   if (!v4l->buffers)
   {
      RARCH_ERR("Out of memory allocating V4L2 buffers.\n");
      return -1;
   }

   v4l->buffers[0].length = buffer_size;
   v4l->buffers[0].start = malloc(buffer_size);

   if (!v4l->buffers[0].start)
   {
      RARCH_ERR("Out of memory allocating V4L2 buffers.\n");
      return -1;
   }

   return 0;
}

static int init_mmap(void *data)
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
         return -1;
      }
      else
      {
         RARCH_ERR("xioctl of VIDIOC_REQBUFS failed.\n");
         return -1;
      }
   }

   if (req.count < 2)
   {
      RARCH_ERR("Insufficient buffer memory on %s.\n", v4l->dev_name);
      return -1;
   }

   v4l->buffers = (struct buffer*)calloc(req.count, sizeof(*v4l->buffers));

   if (!v4l->buffers)
   {
      RARCH_ERR("Out of memory allocating V4L2 buffers.\n");
      return -1;
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
         return -1;
      }

      v4l->buffers[v4l->n_buffers].length = buf.length;
      v4l->buffers[v4l->n_buffers].start = mmap(NULL,
            buf.length, PROT_READ | PROT_WRITE,
            MAP_SHARED,
            v4l->fd, buf.m.offset);

      if (v4l->buffers[v4l->n_buffers].start == MAP_FAILED)
      {
         RARCH_ERR("Error - mmap.\n");
         return -1;
      }
   }

   return 0;
}

static int init_userp(void *data, unsigned int buffer_size)
{
   struct v4l2_requestbuffers req;
   unsigned int page_size;
   video4linux_t *v4l = (video4linux_t*)data;

   page_size = getpagesize();
   buffer_size = (buffer_size + page_size - 1) & ~(page_size - 1);

   memset(&req, 0, sizeof(req));

   req.count = 4;
   req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   req.memory = V4L2_MEMORY_USERPTR;

   if (xioctl(v4l->fd, VIDIOC_REQBUFS, &req) == -1)
   {
      if (errno == EINVAL)
      {
         RARCH_ERR("%s does not support user pointer I/O\n", v4l->dev_name);
         return -1;
      }
      else
      {
         RARCH_ERR("VIDIOC_REQBUFS.\n");
         return -1;
      }
   }

   v4l->buffers = calloc(4, sizeof(*v4l->buffers));

   if (!v4l->buffers)
   {
      RARCH_ERR("Out of memory\n");
      return -1;
   }

   for (v4l->n_buffers = 0; v4l->n_buffers < 4; v4l->n_buffers++)
   {
      v4l->buffers[v4l->n_buffers].length = buffer_size;
      v4l->buffers[v4l->n_buffers].start = memalign( /* boundary */ page_size,
            buffer_size);

      if (!v4l->buffers[v4l->n_buffers].start)
      {
         RARCH_ERR("Out of memory\n");
         return -1;
      }
   }

   return 0;
}

static int init_device(void *data)
{
   struct v4l2_capability cap;
   struct v4l2_cropcap cropcap;
   struct v4l2_crop crop;
   struct v4l2_format fmt;
   unsigned int min;
   video4linux_t *v4l = (video4linux_t*)data;

   if (xioctl(v4l->fd, VIDIOC_QUERYCAP, &cap) == -1)
   {
      if (errno == EINVAL)
      {
         RARCH_ERR("%s is no V4L2 device.\n", v4l->dev_name);
         return -1;
      }
      else
      {
         RARCH_ERR("Error - VIDIOC_QUERYCAP.\n");
         return -1;
      }
   }

   if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
   {
      RARCH_ERR("%s is no video capture device.\n", v4l->dev_name);
      return -1;
   }

   switch (v4l->io)
   {
      case IO_METHOD_READ:
         if (!(cap.capabilities & V4L2_CAP_READWRITE))
         {
            RARCH_ERR("%s does not support read I/O (V4L2_CAP_READWRITE).\n", v4l->dev_name);
            return -1;
         }

         break;

      case IO_METHOD_MMAP:
      case IO_METHOD_USERPTR:
         if (!(cap.capabilities & V4L2_CAP_STREAMING))
         {
            RARCH_ERR("%s does not support streaming I/O (V4L2_CAP_STREAMING).\n", v4l->dev_name);
            return -1;
         }

         break;
   }

   /* Select video input, video standard and tune here. */

   memset (&(cropcap), 0, sizeof (cropcap));

   cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

   if (xioctl(v4l->fd, VIDIOC_CROPCAP, &cropcap) == 0)
   {
      crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      crop.c = cropcap.defrect;   /* reset to default */

      if (xioctl(v4l->fd, VIDIOC_S_CROP, &crop) == -1)
      {
         switch (errno)
         {
            case EINVAL:
               /* Cropping not supported. */
               break;
            default:
               /* Errors ignored. */
               break;
         }
      }
   }

   memset (&fmt, 0, sizeof(fmt));

   fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   fmt.fmt.pix.width  = v4l->width;
   fmt.fmt.pix.height = v4l->height;
   fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
   fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

   if (xioctl(v4l->fd, VIDIOC_S_FMT, &fmt) == -1)
   {
      RARCH_ERR("Error - VIDIOC_S_FMT\n");
      return -1;
   }

   /* Note VIDIOC_S_FMT may change width and height. */

   /* Buggy driver paranoia. */
   min = fmt.fmt.pix.width * 2;
   if (fmt.fmt.pix.bytesperline < min)
      fmt.fmt.pix.bytesperline = min;
   min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
   if (fmt.fmt.pix.sizeimage < min)
      fmt.fmt.pix.sizeimage = min;

   if (fmt.fmt.pix.width != v4l->width)
      v4l->width = fmt.fmt.pix.width;

   if (fmt.fmt.pix.height != v4l->height)
      v4l->height = fmt.fmt.pix.height;

   switch (v4l->io)
   {
      case IO_METHOD_READ:
         return init_read(v4l, fmt.fmt.pix.sizeimage);
      case IO_METHOD_MMAP:
         return init_mmap(v4l);
      case IO_METHOD_USERPTR:
         init_userp(v4l, fmt.fmt.pix.sizeimage);
         break;
   }

   return 0;
}

static int v4l_stop(void *data)
{
   enum v4l2_buf_type type;
   video4linux_t *v4l = (video4linux_t*)data;

   switch (v4l->io)
   {
      case IO_METHOD_READ:
         /* Nothing to do. */
         break;

      case IO_METHOD_MMAP:
      case IO_METHOD_USERPTR:
         type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

         if (xioctl(v4l->fd, VIDIOC_STREAMOFF, &type) == -1)
         {
            RARCH_ERR("Error - VIDIOC_STREAMOFF.\n");
            return -1;
         }

         break;
   }

   v4l->ready = false;

   return 0;
}

static int v4l_start(void *data)
{
   video4linux_t *v4l = (video4linux_t*)data;
   unsigned int i;
   enum v4l2_buf_type type;

   switch (v4l->io)
   {
      case IO_METHOD_READ:
         /* Nothing to do. */
         break;

      case IO_METHOD_MMAP:
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
               return -1;
            }
         }

         type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

         if (xioctl(v4l->fd, VIDIOC_STREAMON, &type) == -1)
         {
            RARCH_ERR("Error - VIDIOC_STREAMON.\n");
            return -1;
         }

         break;

      case IO_METHOD_USERPTR:
         for (i = 0; i < v4l->n_buffers; i++)
         {
            struct v4l2_buffer buf;

            memset(&buf, 0, sizeof(buf));

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_USERPTR;
            buf.index = i;
            buf.m.userptr = (unsigned long)v4l->buffers[i].start;
            buf.length = v4l->buffers[i].length;

            if (xioctl(v4l->fd, VIDIOC_QBUF, &buf) == -1)
            {
               RARCH_ERR("Error - VIDIOC_QBUF.\n");
               return -1;
            }
         }

         type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

         if (xioctl(v4l->fd, VIDIOC_STREAMON, &type) == -1)
         {
            RARCH_ERR("Error - VIDIOC_STREAMON.\n");
            return -1;
         }

         break;
   }

   v4l->ready = true;

   return 0;
}

static void *v4l_init(void)
{
   struct stat st;
   video4linux_t *v4l = (video4linux_t*)calloc(1, sizeof(video4linux_t));
   if (!v4l)
      return NULL;

   // FIXME - /dev/video0 assumed for now - should allow for selecting which device
   v4l->dev_name = "/dev/video0";
   v4l->width    = 640;
   v4l->height   = 480;
   v4l->ready    = false;

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

   v4l->io = IO_METHOD_MMAP;

   if (init_device(v4l) == -1)
      goto error;

   generate_YCbCr_to_RGB_lookup();

   return v4l;

error:
   RARCH_ERR("V4L2: Failed to initialize camera.\n");
   return NULL;
}

static void v4l_free(void *data)
{
   video4linux_t *v4l = (video4linux_t*)data;

   unsigned int i;

   switch (v4l->io)
   {
      case IO_METHOD_READ:
         free(v4l->buffers[0].start);
         break;
      case IO_METHOD_MMAP:
         for (i = 0; i < v4l->n_buffers; i++)
            if (munmap(v4l->buffers[i].start, v4l->buffers[i].length) == -1)
            {
               RARCH_ERR("munmap failed.\n");
               return;
            }
         break;
      case IO_METHOD_USERPTR:
         for (i = 0; i < v4l->n_buffers; i++)
            free(v4l->buffers[i].start);
         break;
   }

   if (close(v4l->fd) == -1)
      RARCH_ERR("close of file descriptor failed.\n");

   v4l->fd = -1;
   free(v4l);

   // Assumes one instance. LUT will be gone at some point anyways.
   free(YCbCr_to_RGB);
   YCbCr_to_RGB = NULL;
}

static void preprocess_image(void *data)
{
   video4linux_t *v4l = (video4linux_t*)data;
   struct v4l2_buffer buf;
   unsigned i;

   switch (v4l->io)
   {
      case IO_METHOD_READ:
         if (read(v4l->fd, v4l->buffers[0].start, v4l->buffers[0].length) == -1)
         {
            switch (errno)
            {
               case EAGAIN:
                  return;
               case EIO:
                  /* Could ignore EIO, see spec. */

                  /* fall through */

               default:
                  return;
            }
         }

         process_image(v4l->buffers[0].start);

         break;

      case IO_METHOD_MMAP:
         memset(&buf, 0, sizeof(buf));

         buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
         buf.memory = V4L2_MEMORY_MMAP;

         if (xioctl(v4l->fd, VIDIOC_DQBUF, &buf) == -1)
         {
            switch (errno)
            {
               case EAGAIN:
                  return;
               case EIO:
                  /* Could ignore EIO, see spec. */

                  /* fall through */

               default:
                  RARCH_ERR("VIDIOC_DQBUF.\n");
                  return;
            }
         }

         assert(buf.index < v4l->n_buffers);

         process_image(v4l->buffers[buf.index].start);

         if (xioctl(v4l->fd, VIDIOC_QBUF, &buf) == -1)
         {
            RARCH_ERR("VIDIOC_QBUF\n");
            return;
         }

         break;

      case IO_METHOD_USERPTR:
         memset(&buf, 0, sizeof(buf));

         buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
         buf.memory = V4L2_MEMORY_USERPTR;

         if (xioctl(v4l->fd, VIDIOC_DQBUF, &buf) == -1)
         {
            switch (errno)
            {
               case EAGAIN:
                  return;

               case EIO:
                  /* Could ignore EIO, see spec. */

                  /* fall through */

               default:
                  RARCH_ERR("VIDIOC_DQBUF.\n");
                  return;
            }
         }

         for (i = 0; i < v4l->n_buffers; i++)
            if (buf.m.userptr == (unsigned long)v4l->buffers[i].start
                  && buf.length == v4l->buffers[i].length)
               break;

         assert(i < v4l->n_buffers);

         process_image((void *)buf.m.userptr);

         if (xioctl(v4l->fd, VIDIOC_QBUF, &buf) == -1)
            RARCH_ERR("VIDIOC_QBUF.\n");

         break;
   }
}

static void v4l_texture_image_2d(void *data)
{
   preprocess_image(data);
}

static void v4l_texture_subimage_2d(void *data)
{
   preprocess_image(data);
}

static bool v4l_ready(void *data, unsigned *width, unsigned *height)
{
   video4linux_t *v4l = (video4linux_t*)data;
   return v4l->ready;
}

const camera_driver_t camera_v4l2 = {
   v4l_init,
   v4l_free,
   v4l_start,
   v4l_stop,
   v4l_ready,
   v4l_texture_image_2d,
   v4l_texture_subimage_2d,
   "video4linux2",
};

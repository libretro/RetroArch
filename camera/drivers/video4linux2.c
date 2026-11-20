/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
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
#if !defined(__FreeBSD__) || __FreeBSD__ < 5
#include <malloc.h>
#endif
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#ifndef __FreeBSD__
#include <asm/types.h>
#endif
#include <linux/videodev2.h>

#include <memmap.h>

#include <retro_miscellaneous.h>
#include <gfx/scaler/scaler.h>
#include <gfx/video_frame.h>
#include <file/file_path.h>
#include <formats/image_yuv_blit.h>
#include <libretro.h>

#include <compat/strl.h>

#include "../camera_driver.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

struct buffer
{
   void *start;
   size_t length;
};

typedef struct video4linux
{
   struct scaler_ctx scaler;
   struct buffer *buffers;
   uint32_t *buffer_output;
   uint8_t *planar;             /* NV12 repacked from a YUYV device */
   size_t pitch;
   size_t chroma_pitch;
   size_t chroma_offset[2];     /* Cb, Cr from the buffer start */
   size_t frame_size;           /* bytes one frame spans */
   uint32_t fourcc;
   int fd;
   unsigned n_buffers;
   unsigned dims;               /* VIDEO_SCALE_PACK */
   unsigned chroma_step;
   unsigned planar_flags;       /* RETRO_CAMERA_PLANAR_* */
   bool want_planar;
   bool ready;

   char dev_name[NAME_MAX_LENGTH];
} video4linux_t;

static int xioctl(int fd, unsigned long request, void *args)
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
   struct v4l2_requestbuffers req = {0};
   video4linux_t *v4l             = (video4linux_t*)data;

   req.count                      = 4;
   req.type                       = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   req.memory                     = V4L2_MEMORY_MMAP;

   if (xioctl(v4l->fd, VIDIOC_REQBUFS, &req) == -1)
   {
      if (errno == EINVAL)
         RARCH_ERR("[V4L2] %s does not support memory mapping.\n", v4l->dev_name);
      else
         RARCH_ERR("[V4L2] xioctl of VIDIOC_REQBUFS failed.\n");
      return false;
   }

   if (req.count < 2)
   {
      RARCH_ERR("[V4L2] Insufficient buffer memory on %s.\n", v4l->dev_name);
      return false;
   }

   v4l->buffers = (struct buffer*)calloc(req.count, sizeof(*v4l->buffers));

   if (!v4l->buffers)
   {
      RARCH_ERR("[V4L2] Out of memory allocating V4L2 buffers.\n");
      return false;
   }

   for (v4l->n_buffers = 0; v4l->n_buffers < req.count; v4l->n_buffers++)
   {
      struct v4l2_buffer buf = {0};

      buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;
      buf.index  = v4l->n_buffers;

      if (xioctl(v4l->fd, VIDIOC_QUERYBUF, &buf) == -1)
      {
         RARCH_ERR("[V4L2] Error - xioctl VIDIOC_QUERYBUF.\n");
         return false;
      }
      if (buf.length < v4l->frame_size)
      {
         RARCH_ERR("[V4L2] Buffer smaller than a frame.\n");
         return false;
      }

      v4l->buffers[v4l->n_buffers].length = buf.length;
      v4l->buffers[v4l->n_buffers].start  = mmap(NULL,
            buf.length, PROT_READ | PROT_WRITE,
            MAP_SHARED,
            v4l->fd, buf.m.offset);

      if (v4l->buffers[v4l->n_buffers].start == MAP_FAILED)
      {
         RARCH_ERR("[V4L2] Error - mmap.\n");
         return false;
      }
   }

   return true;
}

/* Planar consumers' preference order; raw consumers take the last. */
static const uint32_t v4l_formats[3] = {
   V4L2_PIX_FMT_NV12, V4L2_PIX_FMT_YUV420, V4L2_PIX_FMT_YUYV
};

/* The negotiated frame's plane layout and colour, checked against the
 * size the device says one frame takes. */
static bool v4l_layout(video4linux_t *v4l, const struct v4l2_format *fmt)
{
   unsigned w     = fmt->fmt.pix.width;
   unsigned h     = fmt->fmt.pix.height;
   size_t   bpl   = fmt->fmt.pix.bytesperline;
   size_t   rows  = (h + 1) >> 1;

   v4l->fourcc       = fmt->fmt.pix.pixelformat;
   v4l->dims         = VIDEO_SCALE_PACK(w, h);
   v4l->planar_flags = 0;
   if (!w || !h)
      return false;

   switch (v4l->fourcc)
   {
      case V4L2_PIX_FMT_YUYV:
         /* Macropixels: a planar repack needs whole ones. */
         if (v4l->want_planar && (w & 1))
            return false;
         v4l->pitch        = MAX(bpl, (size_t)w * 2);
         v4l->frame_size   = v4l->pitch * h;
         break;
      case V4L2_PIX_FMT_NV12:
         v4l->pitch            = MAX(bpl, (size_t)w);
         v4l->chroma_pitch     = v4l->pitch;
         v4l->chroma_offset[0] = v4l->pitch * h;
         v4l->chroma_offset[1] = v4l->chroma_offset[0] + 1;
         v4l->chroma_step      = 2;
         v4l->frame_size       = v4l->chroma_offset[0]
                               + v4l->chroma_pitch * rows;
         break;
      case V4L2_PIX_FMT_YUV420:
         /* The chroma planes' rows are half the luma's. */
         v4l->pitch            = MAX(bpl, (size_t)w);
         v4l->chroma_pitch     = v4l->pitch >> 1;
         if (v4l->chroma_pitch < (size_t)((w + 1) >> 1))
            return false;
         v4l->chroma_offset[0] = v4l->pitch * h;
         v4l->chroma_offset[1] = v4l->chroma_offset[0]
                               + v4l->chroma_pitch * rows;
         v4l->chroma_step      = 1;
         v4l->frame_size       = v4l->chroma_offset[1]
                               + v4l->chroma_pitch * rows;
         break;
      default:
         return false;
   }

   if (fmt->fmt.pix.sizeimage && fmt->fmt.pix.sizeimage < v4l->frame_size)
      return false;

#ifdef V4L2_MAP_QUANTIZATION_DEFAULT
   {
      unsigned cs  = fmt->fmt.pix.colorspace;
      unsigned enc = fmt->fmt.pix.ycbcr_enc;
      unsigned q   = fmt->fmt.pix.quantization;
      if (enc == V4L2_YCBCR_ENC_DEFAULT)
         enc = V4L2_MAP_YCBCR_ENC_DEFAULT(cs);
      if (q == V4L2_QUANTIZATION_DEFAULT)
         q = V4L2_MAP_QUANTIZATION_DEFAULT(0, cs, enc);
      if (enc == V4L2_YCBCR_ENC_709)
         v4l->planar_flags |= RETRO_CAMERA_PLANAR_BT709;
      if (q == V4L2_QUANTIZATION_FULL_RANGE)
         v4l->planar_flags |= RETRO_CAMERA_PLANAR_FULL_RANGE;
   }
#endif
   return true;
}

static bool init_device(void *data)
{
   struct v4l2_crop crop;
   struct v4l2_capability cap;
   struct v4l2_format      fmt = {0};
   struct v4l2_cropcap cropcap = {0};
   unsigned i;
   video4linux_t          *v4l = (video4linux_t*)data;

   if (xioctl(v4l->fd, VIDIOC_QUERYCAP, &cap) < 0)
   {
      if (errno == EINVAL)
         RARCH_ERR("[V4L2] %s is no V4L2 device.\n", v4l->dev_name);
      else
         RARCH_ERR("[V4L2] Error - VIDIOC_QUERYCAP.\n");
      return false;
   }

   if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
   {
      RARCH_ERR("[V4L2] %s is no video capture device.\n", v4l->dev_name);
      return false;
   }

   if (!(cap.capabilities & V4L2_CAP_STREAMING))
   {
      RARCH_ERR("[V4L2] %s does not support streaming I/O (V4L2_CAP_STREAMING).\n",
            v4l->dev_name);
      return false;
   }

   cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

   if (xioctl(v4l->fd, VIDIOC_CROPCAP, &cropcap) == 0)
   {
      crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      crop.c    = cropcap.defrect;
      /* Ignore errors here. */
      xioctl(v4l->fd, VIDIOC_S_CROP, &crop);
   }

   /* A planar consumer takes the device's own 4:2:0 in place when it
    * has one; YUYV is repacked. */
   for (i = v4l->want_planar ? 0 : 2; i < 3; i++)
   {
      memset(&fmt, 0, sizeof(fmt));
      fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      fmt.fmt.pix.width       = VIDEO_SCALE_W(v4l->dims);
      fmt.fmt.pix.height      = VIDEO_SCALE_H(v4l->dims);
      fmt.fmt.pix.pixelformat = v4l_formats[i];
      fmt.fmt.pix.field       = V4L2_FIELD_NONE;

      if (xioctl(v4l->fd, VIDIOC_S_FMT, &fmt) < 0)
      {
         RARCH_ERR("[V4L2] Error - VIDIOC_S_FMT.\n");
         return false;
      }
      /* VIDIOC_S_FMT may change the format, width, height and pitch. */
      if (fmt.fmt.pix.pixelformat == v4l_formats[i])
         break;
   }

   if (i == 3)
   {
      RARCH_ERR(v4l->want_planar
            ? "[V4L2] The V4L2 device supports none of NV12, YU12 and YUYV.\n"
            : "[V4L2] The V4L2 device doesn't support YUYV.\n");
      return false;
   }

   if (!VIDEO_SCALE_FITS(fmt.fmt.pix.width, fmt.fmt.pix.height))
   {
      RARCH_ERR("[V4L2] The device frame is too large.\n");
      return false;
   }
   if (!v4l_layout(v4l, &fmt))
   {
      RARCH_ERR("[V4L2] The device frame layout is not usable.\n");
      return false;
   }

   if (fmt.fmt.pix.field != V4L2_FIELD_NONE
         && fmt.fmt.pix.field != V4L2_FIELD_INTERLACED)
   {
      RARCH_ERR("[V4L2] The V4L2 device doesn't support progressive nor interlaced video.\n");
      return false;
   }

   RARCH_LOG("[V4L2] Device: %ux%u %.4s.\n",
         VIDEO_SCALE_W(v4l->dims), VIDEO_SCALE_H(v4l->dims),
         (const char*)&v4l->fourcc);

   return init_mmap(v4l);
}

static void v4l_stop(void *data)
{
   video4linux_t *v4l = (video4linux_t*)data;
   enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

   if (xioctl(v4l->fd, VIDIOC_STREAMOFF, &type) == -1)
      RARCH_ERR("[V4L2] Error - VIDIOC_STREAMOFF.\n");

   v4l->ready = false;
}

static bool v4l_start(void *data)
{
   unsigned i;
   enum v4l2_buf_type type;
   video4linux_t *v4l = (video4linux_t*)data;

   for (i = 0; i < v4l->n_buffers; i++)
   {
      struct v4l2_buffer buf = {0};

      buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;
      buf.index  = i;

      if (xioctl(v4l->fd, VIDIOC_QBUF, &buf) == -1)
      {
         RARCH_ERR("[V4L2] Error - VIDIOC_QBUF.\n");
         return false;
      }
   }

   type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

   if (xioctl(v4l->fd, VIDIOC_STREAMON, &type) == -1)
   {
      RARCH_ERR("[V4L2] Error - VIDIOC_STREAMON.\n");
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
         RARCH_ERR("[V4L2] munmap failed.\n");

   if (v4l->fd >= 0)
      close(v4l->fd);

   free(v4l->buffers);
   free(v4l->buffer_output);
   free(v4l->planar);
   scaler_ctx_gen_reset(&v4l->scaler);
   free(v4l);
}

static void *v4l_init(const char *device, uint64_t caps,
      unsigned dims)
{
   video4linux_t *v4l = NULL;

   bool want_planar = (caps
         & (UINT64_C(1) << RETRO_CAMERA_BUFFER_PLANAR)) != 0;

   if (!want_planar && (caps
         & (UINT64_C(1) << RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER)) == 0)
   {
      RARCH_ERR("[V4L2] Returns raw framebuffers or planes.\n");
      return NULL;
   }

   v4l = (video4linux_t*)calloc(1, sizeof(video4linux_t));
   if (!v4l)
      return NULL;

   v4l->want_planar = want_planar;
   strlcpy(v4l->dev_name, device ? device : "/dev/video0",
         sizeof(v4l->dev_name));

   v4l->dims   = dims;
   v4l->ready  = false;

   if (!path_is_character_special(v4l->dev_name))
   {
      RARCH_ERR("[V4L2] %s is no device.\n", v4l->dev_name);
      free(v4l);
      return NULL;
   }

   v4l->fd = open(v4l->dev_name, O_RDWR | O_NONBLOCK, 0);

   if (v4l->fd == -1)
   {
      RARCH_ERR("[V4L2] Cannot open '%s': %d, %s.\n", v4l->dev_name,
            errno, strerror(errno));
      goto error;
   }

   if (!init_device(v4l))
      goto error;

   if (v4l->want_planar)
   {
      if (v4l->fourcc == V4L2_PIX_FMT_YUYV)
      {
         size_t w = VIDEO_SCALE_W(v4l->dims);
         size_t h = VIDEO_SCALE_H(v4l->dims);
         if (!(v4l->planar = (uint8_t*)malloc(w * h + w * ((h + 1) >> 1))))
         {
            RARCH_ERR("[V4L2] Failed to allocate planar buffer.\n");
            goto error;
         }
      }
      return v4l;
   }

   v4l->buffer_output = (uint32_t*)
      malloc((size_t)VIDEO_SCALE_W(v4l->dims) * VIDEO_SCALE_H(v4l->dims)
            * sizeof(uint32_t));

   if (!v4l->buffer_output)
   {
      RARCH_ERR("[V4L2] Failed to allocate output buffer.\n");
      goto error;
   }

   v4l->scaler.in_width   = v4l->scaler.out_width  = VIDEO_SCALE_W(v4l->dims);
   v4l->scaler.in_height  = v4l->scaler.out_height = VIDEO_SCALE_H(v4l->dims);
   v4l->scaler.in_fmt     = SCALER_FMT_YUYV;
   v4l->scaler.out_fmt    = SCALER_FMT_ARGB8888;
   v4l->scaler.in_stride  = v4l->pitch;
   v4l->scaler.out_stride = VIDEO_SCALE_W(v4l->dims) * 4;

   if (!scaler_ctx_gen_filter(&v4l->scaler))
   {
      RARCH_ERR("[V4L2] Failed to create scaler.\n");
      goto error;
   }

   return v4l;

error:
   RARCH_ERR("[V4L2] Failed to initialize camera.\n");
   v4l_free(v4l);
   return NULL;
}

/* The next filled buffer, or false with nothing to hand over. A
 * buffer the device flagged or under-filled goes straight back. */
static bool v4l_dequeue(video4linux_t *v4l, struct v4l2_buffer *buf)
{
   memset(buf, 0, sizeof(*buf));
   buf->type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   buf->memory = V4L2_MEMORY_MMAP;

   if (xioctl(v4l->fd, VIDIOC_DQBUF, buf) == -1)
   {
      if (errno != EAGAIN)
         RARCH_ERR("[V4L2] VIDIOC_DQBUF.\n");
      return false;
   }

   if (     buf->index < v4l->n_buffers
         && !(buf->flags & V4L2_BUF_FLAG_ERROR)
         && (!buf->bytesused || buf->bytesused >= v4l->frame_size))
      return true;

   if (xioctl(v4l->fd, VIDIOC_QBUF, buf) == -1)
      RARCH_ERR("[V4L2] VIDIOC_QBUF.\n");
   return false;
}

static bool v4l_poll(void *data,
      retro_camera_frame_raw_framebuffer_t frame_raw_cb,
      retro_camera_frame_opengl_texture_t frame_gl_cb,
      retro_camera_frame_planar_t frame_planar_cb)
{
   struct v4l2_buffer buf;
   const uint8_t *src;
   video4linux_t *v4l = (video4linux_t*)data;
   unsigned w         = VIDEO_SCALE_W(v4l->dims);
   unsigned h         = VIDEO_SCALE_H(v4l->dims);

   (void)frame_gl_cb;

   if (!v4l->ready || (v4l->want_planar && !frame_planar_cb))
      return false;
   if (!v4l_dequeue(v4l, &buf))
      return false;

   src = (const uint8_t*)v4l->buffers[buf.index].start;

   if (v4l->want_planar)
   {
      struct retro_camera_planar_frame f;
      f.width    = w;
      f.height   = h;
      f.rotation = 0;
      f.flags    = v4l->planar_flags;
      if (v4l->planar)
      {
         /* NV12 out of the YUYV buffer, which then goes straight back. */
         image_yuv_422_to_nv12(v4l->planar, v4l->planar + (size_t)w * h,
               src, v4l->pitch, w, h);
         f.planes[0]  = v4l->planar;
         f.planes[1]  = v4l->planar + (size_t)w * h;
         f.planes[2]  = f.planes[1] + 1;
         f.strides[0] = f.strides[1] = f.strides[2] = w;
         f.chroma_step = 2;
      }
      else
      {
         /* The capture buffer itself, until the callback returns. */
         f.planes[0]   = src;
         f.planes[1]   = src + v4l->chroma_offset[0];
         f.planes[2]   = src + v4l->chroma_offset[1];
         f.strides[0]  = v4l->pitch;
         f.strides[1]  = f.strides[2] = v4l->chroma_pitch;
         f.chroma_step = v4l->chroma_step;
      }
      frame_planar_cb(&f);
   }
   else
   {
      struct scaler_ctx *ctx = &v4l->scaler;
      scaler_ctx_scale_direct(ctx, v4l->buffer_output, src);
      if (frame_raw_cb)
         frame_raw_cb(v4l->buffer_output, w, h, (size_t)w * 4);
   }

   if (xioctl(v4l->fd, VIDIOC_QBUF, &buf) == -1)
      RARCH_ERR("[V4L2] VIDIOC_QBUF.\n");

   return true;
}

camera_driver_t camera_v4l2 = {
   v4l_init,
   v4l_free,
   v4l_start,
   v4l_stop,
   v4l_poll,
   "video4linux2",
};

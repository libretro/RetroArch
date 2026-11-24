/* camera/drivers/video4linux2.c against a scripted device: open, close,
 * ioctl, mmap and munmap are link-wrapped, so each lane picks the
 * formats the device offers and the buffers it hands back, and checks
 * what reaches the core's callbacks. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "../../../camera/drivers/video4linux2.c"

bool path_is_character_special(const char *path)
{
   return path && strcmp(path, "/dev/none") != 0;
}

/* ---- the scripted device ---------------------------------------- */

#define DEV_FD      42
#define DEV_BUFS    4
#define DEV_BUF_MAX (64 * 1024)

static uint8_t  dev_mem[DEV_BUFS][DEV_BUF_MAX];
static uint32_t dev_formats[4];   /* offered, 0-terminated */
static unsigned dev_w, dev_h, dev_bpl_pad;
static unsigned dev_colorspace, dev_ycbcr_enc, dev_quant;
static unsigned dev_buf_len;      /* 0: the frame size */
static int      dev_filled = -1;  /* index DQBUF returns, -1 EAGAIN */
static unsigned dev_fill_flags, dev_fill_bytes;
static unsigned dev_qbufs, dev_dqbufs, dev_closes, dev_unmaps;
static uint32_t dev_last_fmt;

static unsigned dev_frame_size(uint32_t fourcc, unsigned bpl)
{
   switch (fourcc)
   {
      case V4L2_PIX_FMT_YUYV:   return bpl * dev_h;
      case V4L2_PIX_FMT_NV12:   return bpl * dev_h + bpl * ((dev_h + 1) / 2);
      case V4L2_PIX_FMT_YUV420: return bpl * dev_h + bpl * ((dev_h + 1) / 2);
      default:                  return bpl * dev_h;
   }
}

static unsigned dev_bpl(uint32_t fourcc)
{
   return (fourcc == V4L2_PIX_FMT_YUYV ? dev_w * 2 : dev_w) + dev_bpl_pad;
}

int __wrap_open(const char *path, int flags, ...)
{
   (void)path; (void)flags;
   return DEV_FD;
}

int __wrap_close(int fd)
{
   if (fd == DEV_FD)
      dev_closes++;
   return 0;
}

void *__wrap_mmap(void *addr, size_t len, int prot, int flags, int fd,
      off_t off)
{
   (void)addr; (void)prot; (void)flags;
   if (fd != DEV_FD || len > DEV_BUF_MAX || off % DEV_BUF_MAX)
      return MAP_FAILED;
   return dev_mem[off / DEV_BUF_MAX];
}

int __wrap_munmap(void *addr, size_t len)
{
   (void)addr; (void)len;
   dev_unmaps++;
   return 0;
}

int __wrap_ioctl(int fd, unsigned long req, ...)
{
   va_list ap;
   void *arg;
   va_start(ap, req);
   arg = va_arg(ap, void*);
   va_end(ap);
   if (fd != DEV_FD)
   {
      errno = EBADF;
      return -1;
   }
   switch (req)
   {
      case VIDIOC_QUERYCAP:
      {
         struct v4l2_capability *c = (struct v4l2_capability*)arg;
         memset(c, 0, sizeof(*c));
         c->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
         return 0;
      }
      case VIDIOC_CROPCAP:
         errno = EINVAL;
         return -1;
      case VIDIOC_S_FMT:
      {
         struct v4l2_format *f = (struct v4l2_format*)arg;
         uint32_t want = f->fmt.pix.pixelformat;
         unsigned i;
         /* An unoffered format comes back as the first offered one. */
         f->fmt.pix.pixelformat = dev_formats[0];
         for (i = 0; dev_formats[i]; i++)
            if (dev_formats[i] == want)
               f->fmt.pix.pixelformat = want;
         f->fmt.pix.width        = dev_w;
         f->fmt.pix.height       = dev_h;
         f->fmt.pix.field        = V4L2_FIELD_NONE;
         f->fmt.pix.bytesperline = dev_bpl(f->fmt.pix.pixelformat);
         f->fmt.pix.sizeimage    = dev_frame_size(f->fmt.pix.pixelformat,
               f->fmt.pix.bytesperline);
         f->fmt.pix.colorspace   = dev_colorspace;
         f->fmt.pix.ycbcr_enc    = dev_ycbcr_enc;
         f->fmt.pix.quantization = dev_quant;
         dev_last_fmt            = f->fmt.pix.pixelformat;
         return 0;
      }
      case VIDIOC_REQBUFS:
         ((struct v4l2_requestbuffers*)arg)->count = DEV_BUFS;
         return 0;
      case VIDIOC_QUERYBUF:
      {
         struct v4l2_buffer *b = (struct v4l2_buffer*)arg;
         b->length   = dev_buf_len ? dev_buf_len : dev_frame_size(
               dev_last_fmt, dev_bpl(dev_last_fmt));
         b->m.offset = b->index * DEV_BUF_MAX;
         return 0;
      }
      case VIDIOC_QBUF:
         dev_qbufs++;
         return 0;
      case VIDIOC_DQBUF:
      {
         struct v4l2_buffer *b = (struct v4l2_buffer*)arg;
         if (dev_filled < 0)
         {
            errno = EAGAIN;
            return -1;
         }
         dev_dqbufs++;
         b->index     = (unsigned)dev_filled;
         b->flags     = dev_fill_flags;
         b->bytesused = dev_fill_bytes;
         dev_filled   = -1;
         return 0;
      }
      case VIDIOC_STREAMON:
      case VIDIOC_STREAMOFF:
         return 0;
      default:
         errno = ENOTTY;
         return -1;
   }
}

/* ---- what reached the core -------------------------------------- */

static struct retro_camera_planar_frame got_planar;
static unsigned got_planar_count, got_raw_count, got_qbufs_in_cb;
static uint8_t  got_y[64 * 64], got_uv[64 * 64];
static uint32_t got_raw[64 * 64];

static void RETRO_CALLCONV planar_cb(const struct retro_camera_planar_frame *f)
{
   unsigned row, x;
   got_planar       = *f;
   got_planar_count++;
   got_qbufs_in_cb  = dev_qbufs;
   for (row = 0; row < f->height; row++)
      memcpy(got_y + row * f->width, f->planes[0] + row * f->strides[0],
            f->width);
   /* Gathered as NV12 whatever the step, so lanes compare alike. */
   for (row = 0; row < (f->height + 1) / 2; row++)
      for (x = 0; x < (f->width + 1) / 2; x++)
      {
         got_uv[row * f->width + 2 * x]     =
            f->planes[1][row * f->strides[1] + x * f->chroma_step];
         got_uv[row * f->width + 2 * x + 1] =
            f->planes[2][row * f->strides[2] + x * f->chroma_step];
      }
}

static void RETRO_CALLCONV raw_cb(const uint32_t *buf, unsigned w,
      unsigned h, size_t pitch)
{
   unsigned row;
   got_raw_count++;
   for (row = 0; row < h; row++)
      memcpy(got_raw + row * w, (const uint8_t*)buf + row * pitch, w * 4);
}

static int fails;

#define CHECK(cond, what) do { \
   if (cond) printf("  [ok]   %s\n", what); \
   else { printf("  [FAIL] %s\n", what); fails++; } \
} while (0)

#define CAP_RAW    (UINT64_C(1) << RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER)
#define CAP_PLANAR (UINT64_C(1) << RETRO_CAMERA_BUFFER_PLANAR)

static void dev_reset(uint32_t f0, uint32_t f1, uint32_t f2,
      unsigned w, unsigned h)
{
   memset(dev_mem, 0, sizeof(dev_mem));
   dev_formats[0] = f0;
   dev_formats[1] = f1;
   dev_formats[2] = f2;
   dev_formats[3] = 0;
   dev_w = w;
   dev_h = h;
   dev_bpl_pad    = 0;
   dev_colorspace = V4L2_COLORSPACE_SRGB;
   dev_ycbcr_enc  = V4L2_YCBCR_ENC_DEFAULT;
   dev_quant      = V4L2_QUANTIZATION_DEFAULT;
   dev_buf_len    = 0;
   dev_filled     = -1;
   dev_fill_flags = 0;
   dev_fill_bytes = 0;
   dev_qbufs = dev_dqbufs = dev_closes = dev_unmaps = 0;
   got_planar_count = got_raw_count = 0;
}

/* Fills buffer @i with a frame of @fourcc: luma and chroma ramps
 * in the layout the device reported, and returns it next. */
static void dev_fill(unsigned i, uint32_t fourcc)
{
   unsigned bpl = dev_bpl(fourcc);
   unsigned row, x;
   uint8_t *m   = dev_mem[i];
   memset(m, 0xee, DEV_BUF_MAX);
   if (fourcc == V4L2_PIX_FMT_YUYV)
   {
      for (row = 0; row < dev_h; row++)
         for (x = 0; x < dev_w / 2; x++)
         {
            uint8_t *p = m + row * bpl + 4 * x;
            p[0] = (uint8_t)(row * 16 + 2 * x);
            p[1] = (uint8_t)(100 + row * 8 + x);
            p[2] = (uint8_t)(row * 16 + 2 * x + 1);
            p[3] = (uint8_t)(200 - row * 8 - x);
         }
   }
   else
   {
      unsigned cpl = fourcc == V4L2_PIX_FMT_NV12 ? bpl : bpl / 2;
      uint8_t *cb  = m + bpl * dev_h;
      uint8_t *cr  = fourcc == V4L2_PIX_FMT_NV12 ? cb + 1
                   : cb + cpl * ((dev_h + 1) / 2);
      unsigned step = fourcc == V4L2_PIX_FMT_NV12 ? 2 : 1;
      for (row = 0; row < dev_h; row++)
         for (x = 0; x < dev_w; x++)
            m[row * bpl + x] = (uint8_t)(row * 16 + x);
      for (row = 0; row < (dev_h + 1) / 2; row++)
         for (x = 0; x < (dev_w + 1) / 2; x++)
         {
            cb[row * cpl + x * step] = (uint8_t)(100 + row * 8 + x);
            cr[row * cpl + x * step] = (uint8_t)(200 - row * 8 - x);
         }
   }
   dev_filled = (int)i;
}

/* The NV12 a 4:2:0 ramp of dev_fill is, or, for YUYV, its repack. */
static void expect_nv12(uint8_t *y, uint8_t *uv, bool yuyv)
{
   unsigned row, x;
   for (row = 0; row < dev_h; row++)
      for (x = 0; x < dev_w; x++)
         y[row * dev_w + x] = (uint8_t)(row * 16 + x);
   for (row = 0; row < (dev_h + 1) / 2; row++)
      for (x = 0; x < (dev_w + 1) / 2; x++)
      {
         unsigned r0 = 2 * row, r1 = 2 * row + 1 < dev_h ? 2 * row + 1 : r0;
         if (yuyv)
         {
            uv[row * dev_w + 2 * x]     = (uint8_t)(((100 + r0 * 8 + x)
                  + (100 + r1 * 8 + x) + 1) >> 1);
            uv[row * dev_w + 2 * x + 1] = (uint8_t)(((200 - r0 * 8 - x)
                  + (200 - r1 * 8 - x) + 1) >> 1);
         }
         else
         {
            uv[row * dev_w + 2 * x]     = (uint8_t)(100 + row * 8 + x);
            uv[row * dev_w + 2 * x + 1] = (uint8_t)(200 - row * 8 - x);
         }
      }
}

static video4linux_t *open_started(uint64_t caps)
{
   video4linux_t *v4l = (video4linux_t*)v4l_init("/dev/video9", caps, 0);
   if (v4l && !v4l_start(v4l))
   {
      v4l_free(v4l);
      return NULL;
   }
   return v4l;
}

static void lane_zero_copy(uint32_t fourcc, const char *name)
{
   static uint8_t ey[64 * 64], euv[64 * 64];
   video4linux_t *v4l;
   unsigned i = 2;
   char what[128];

   printf("planar consumer, %s device:\n", name);
   dev_reset(fourcc, V4L2_PIX_FMT_YUYV, 0, 8, 6);
   dev_bpl_pad = 4;
   v4l = open_started(CAP_PLANAR);
   CHECK(v4l != NULL, "camera opens");
   if (!v4l)
      return;
   CHECK(v4l->fourcc == fourcc, "device's own 4:2:0 negotiated");
   CHECK(v4l->planar == NULL && v4l->buffer_output == NULL,
         "no conversion buffers");
   dev_fill(i, fourcc);
   dev_qbufs = 0;
   CHECK(v4l_poll(v4l, raw_cb, NULL, planar_cb), "poll reports a frame");
   CHECK(got_planar_count == 1 && got_raw_count == 0,
         "planes delivered once, nothing raw");
   CHECK(got_planar.planes[0] == dev_mem[i], "luma is the capture buffer");
   CHECK(got_planar.strides[0] == 8 + 4, "luma stride is the device's");
   snprintf(what, sizeof(what), "chroma step %u",
         fourcc == V4L2_PIX_FMT_NV12 ? 2 : 1);
   CHECK(got_planar.chroma_step == (fourcc == V4L2_PIX_FMT_NV12 ? 2u : 1u),
         what);
   CHECK(got_planar.strides[1] == (fourcc == V4L2_PIX_FMT_NV12 ? 12u : 6u)
         && got_planar.strides[2] == got_planar.strides[1],
         "chroma stride follows the format");
   CHECK(got_planar.width == 8 && got_planar.height == 6
         && got_planar.flags == 0, "size and default colour");
   expect_nv12(ey, euv, false);
   CHECK(memcmp(got_y, ey, 8 * 6) == 0, "luma samples");
   CHECK(memcmp(got_uv, euv, 8 * 3) == 0, "chroma samples");
   CHECK(got_qbufs_in_cb == 0 && dev_qbufs == 1,
         "buffer goes back after the callback, once");
   v4l_stop(v4l);
   v4l_free(v4l);
   CHECK(dev_unmaps == DEV_BUFS && dev_closes == 1, "free unmaps and closes");
}

static void lane_repack(void)
{
   static uint8_t ey[64 * 64], euv[64 * 64];
   video4linux_t *v4l;

   printf("planar consumer, YUYV-only device:\n");
   dev_reset(V4L2_PIX_FMT_YUYV, 0, 0, 8, 5);
   v4l = open_started(CAP_PLANAR | CAP_RAW);
   CHECK(v4l != NULL, "camera opens");
   if (!v4l)
      return;
   CHECK(v4l->fourcc == V4L2_PIX_FMT_YUYV && v4l->planar != NULL,
         "YUYV negotiated, repack buffer allocated");
   dev_fill(1, V4L2_PIX_FMT_YUYV);
   dev_qbufs = 0;
   CHECK(v4l_poll(v4l, raw_cb, NULL, planar_cb), "poll reports a frame");
   CHECK(got_planar_count == 1 && got_raw_count == 0, "planes, not raw");
   CHECK(got_planar.planes[0] == v4l->planar
         && got_planar.planes[2] == got_planar.planes[1] + 1
         && got_planar.chroma_step == 2 && got_planar.strides[0] == 8,
         "NV12 in the repack buffer");
   expect_nv12(ey, euv, true);
   CHECK(memcmp(got_y, ey, 8 * 5) == 0, "luma deinterleaved");
   CHECK(memcmp(got_uv, euv, 8 * 3) == 0,
         "row pairs' chroma averaged, lone last row kept");
   CHECK(dev_qbufs == 1, "buffer back once");
   v4l_free(v4l);
}

static void lane_raw(void)
{
   video4linux_t *v4l;

   printf("raw consumer, NV12 and YUYV device:\n");
   dev_reset(V4L2_PIX_FMT_NV12, V4L2_PIX_FMT_YUYV, 0, 8, 4);
   v4l = open_started(CAP_RAW);
   CHECK(v4l != NULL, "camera opens");
   if (!v4l)
      return;
   CHECK(v4l->fourcc == V4L2_PIX_FMT_YUYV && v4l->buffer_output != NULL,
         "YUYV negotiated for conversion");
   dev_fill(0, V4L2_PIX_FMT_YUYV);
   CHECK(v4l_poll(v4l, raw_cb, NULL, planar_cb), "poll reports a frame");
   CHECK(got_raw_count == 1 && got_planar_count == 0, "raw, not planes");
   CHECK((got_raw[0] & 0xff000000u) == 0xff000000u, "pixels converted");
   v4l_free(v4l);
}

static void lane_refusals(void)
{
   video4linux_t *v4l;

   printf("refusals:\n");
   dev_reset(V4L2_PIX_FMT_MJPEG, 0, 0, 8, 4);
   CHECK(v4l_init("/dev/video9", CAP_PLANAR, 0) == NULL,
         "MJPEG-only device refused for planes");
   CHECK(dev_closes == 1, "refused device closed");
   CHECK(v4l_init("/dev/video9", CAP_RAW, 0) == NULL,
         "MJPEG-only device refused for raw");
   dev_reset(V4L2_PIX_FMT_YUYV, 0, 0, 8, 4);
   CHECK(v4l_init("/dev/video9", (UINT64_C(1)
               << RETRO_CAMERA_BUFFER_OPENGL_TEXTURE), 0) == NULL,
         "neither raw nor planes refused");
   CHECK(v4l_init("/dev/none", CAP_PLANAR, 0) == NULL,
         "missing device refused");
   dev_reset(V4L2_PIX_FMT_YUYV, 0, 0, 7, 4);
   CHECK(v4l_init("/dev/video9", CAP_PLANAR, 0) == NULL,
         "odd-width YUYV refused for planes");
   dev_reset(V4L2_PIX_FMT_NV12, 0, 0, 8, 4);
   dev_buf_len = 8 * 4;
   CHECK(v4l_init("/dev/video9", CAP_PLANAR, 0) == NULL,
         "buffer shorter than a frame refused");
}

static void lane_buffers(void)
{
   video4linux_t *v4l;

   printf("buffers:\n");
   dev_reset(V4L2_PIX_FMT_NV12, 0, 0, 8, 4);
   v4l = open_started(CAP_PLANAR);
   CHECK(v4l != NULL, "camera opens");
   if (!v4l)
      return;
   dev_qbufs = 0;
   CHECK(!v4l_poll(v4l, NULL, NULL, planar_cb) && got_planar_count == 0,
         "empty queue: no frame");
   dev_fill(3, V4L2_PIX_FMT_NV12);
   CHECK(!v4l_poll(v4l, raw_cb, NULL, NULL) && dev_dqbufs == 0,
         "no planar callback: nothing dequeued");
   dev_fill_flags = V4L2_BUF_FLAG_ERROR;
   CHECK(!v4l_poll(v4l, NULL, NULL, planar_cb) && got_planar_count == 0
         && dev_qbufs == 1, "flagged buffer: requeued, no frame");
   dev_fill(3, V4L2_PIX_FMT_NV12);
   dev_fill_flags = 0;
   dev_fill_bytes = 8 * 4;
   CHECK(!v4l_poll(v4l, NULL, NULL, planar_cb) && got_planar_count == 0
         && dev_qbufs == 2, "short buffer: requeued, no frame");
   dev_fill(3, V4L2_PIX_FMT_NV12);
   dev_fill_bytes = 8 * 4 + 8 * 2;
   CHECK(v4l_poll(v4l, NULL, NULL, planar_cb) && got_planar_count == 1,
         "full buffer: frame");
   v4l_stop(v4l);
   dev_fill(3, V4L2_PIX_FMT_NV12);
   CHECK(!v4l_poll(v4l, NULL, NULL, planar_cb) && got_planar_count == 1,
         "stopped camera: no frame");
   v4l_free(v4l);
}

static void lane_colour(void)
{
   video4linux_t *v4l;

   printf("colour:\n");
   dev_reset(V4L2_PIX_FMT_NV12, 0, 0, 8, 4);
   dev_colorspace = V4L2_COLORSPACE_REC709;
   v4l = open_started(CAP_PLANAR);
   CHECK(v4l && v4l->planar_flags == RETRO_CAMERA_PLANAR_BT709,
         "Rec.709 device: BT.709, limited range");
   if (v4l)
      v4l_free(v4l);
   dev_reset(V4L2_PIX_FMT_NV12, 0, 0, 8, 4);
   dev_colorspace = V4L2_COLORSPACE_JPEG;
   v4l = open_started(CAP_PLANAR);
   CHECK(v4l && v4l->planar_flags == RETRO_CAMERA_PLANAR_FULL_RANGE,
         "JPEG colorspace: BT.601, full range");
   if (v4l)
   {
      dev_fill(0, V4L2_PIX_FMT_NV12);
      v4l_poll(v4l, NULL, NULL, planar_cb);
      CHECK(got_planar.flags == RETRO_CAMERA_PLANAR_FULL_RANGE,
            "flags reach the frame");
      v4l_free(v4l);
   }
   dev_reset(V4L2_PIX_FMT_NV12, 0, 0, 8, 4);
   dev_ycbcr_enc = V4L2_YCBCR_ENC_709;
   dev_quant     = V4L2_QUANTIZATION_FULL_RANGE;
   v4l = open_started(CAP_PLANAR);
   CHECK(v4l && v4l->planar_flags == (RETRO_CAMERA_PLANAR_BT709
            | RETRO_CAMERA_PLANAR_FULL_RANGE),
         "explicit encoding and quantization win");
   if (v4l)
      v4l_free(v4l);
}

int main(void)
{
   lane_zero_copy(V4L2_PIX_FMT_NV12,   "NV12");
   lane_zero_copy(V4L2_PIX_FMT_YUV420, "YU12");
   lane_repack();
   lane_raw();
   lane_refusals();
   lane_buffers();
   lane_colour();
   printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
   return fails ? 1 : 0;
}

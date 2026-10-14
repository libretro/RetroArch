/* An XVideo adaptor for X servers that have none (Xvfb), preloaded in
 * front of libXv: the calls gfx/drivers/xvideo.c makes, answered by one
 * adaptor with one port that takes the format XV_STANDIN_FORMAT names
 * (YUY2, the default, UYVY or I420) and puts an image to a drawable the
 * way a textured adaptor does, scaled, through XPutImage, with the
 * completion event a put asks for queued as the server would send it -
 * at once, the image having been read. A put whose
 * source lies outside the image aborts: the driver handed the adaptor
 * something a real one would read past the end of.
 *
 *   make
 *   LD_PRELOAD=$PWD/libxv_standin.so retroarch --appendconfig xv.cfg
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>

#define STANDIN_PORT 0x5856
#define STANDIN_MAX  4096

enum standin_fmt
{
   FMT_YUY2 = 0,
   FMT_UYVY,
   FMT_I420
};

static enum standin_fmt standin_format(void)
{
   const char *f = getenv("XV_STANDIN_FORMAT");
   if (f && !strcmp(f, "UYVY"))
      return FMT_UYVY;
   if (f && !strcmp(f, "I420"))
      return FMT_I420;
   return FMT_YUY2;
}

static int standin_fourcc(enum standin_fmt f)
{
   switch (f)
   {
      case FMT_UYVY: return 0x59565955; /* UYVY */
      case FMT_I420: return 0x30323449; /* I420 */
      default:       break;
   }
   return 0x32595559; /* YUY2 */
}

int XvQueryExtension(Display *dpy, unsigned *version, unsigned *release,
      unsigned *request_base, unsigned *event_base, unsigned *error_base)
{
   (void)dpy;
   *version      = 2;
   *release      = 2;
   *request_base = 0;
   *event_base   = 0;
   *error_base   = 0;
   return Success;
}

int XvQueryAdaptors(Display *dpy, Window window, unsigned *num,
      XvAdaptorInfo **info)
{
   XvAdaptorInfo *a = (XvAdaptorInfo*)calloc(1, sizeof(*a));
   XvFormat      *f = (XvFormat*)calloc(1, sizeof(*f));
   int screen       = DefaultScreen(dpy);
   (void)window;
   if (!a || !f)
   {
      free(a);
      free(f);
      return XvBadAlloc;
   }
   f->depth       = (char)DefaultDepth(dpy, screen);
   f->visual_id   = XVisualIDFromVisual(DefaultVisual(dpy, screen));
   a->base_id     = STANDIN_PORT;
   a->num_ports   = 1;
   a->type        = XvInputMask | XvImageMask;
   if ((a->name = (char*)malloc(sizeof("Xv stand-in"))))
      memcpy(a->name, "Xv stand-in", sizeof("Xv stand-in"));
   a->num_formats = 1;
   a->formats     = f;
   *num           = 1;
   *info          = a;
   return Success;
}

void XvFreeAdaptorInfo(XvAdaptorInfo *info)
{
   if (!info)
      return;
   free(info->name);
   free(info->formats);
   free(info);
}

XvImageFormatValues *XvListImageFormats(Display *dpy, XvPortID port,
      int *count)
{
   enum standin_fmt fmt   = standin_format();
   XvImageFormatValues *v = (XvImageFormatValues*)calloc(1, sizeof(*v));
   (void)dpy;
   *count = 0;
   if (!v || port != STANDIN_PORT)
   {
      free(v);
      return NULL;
   }
   v->id         = standin_fourcc(fmt);
   v->type       = XvYUV;
   v->byte_order = LSBFirst;
   v->num_planes = fmt == FMT_I420 ? 3 : 1;
   switch (fmt)
   {
      case FMT_UYVY:
         v->bits_per_pixel = 16;
         v->format         = XvPacked;
         memcpy(v->component_order, "UYVY", 4);
         break;
      case FMT_I420:
         v->bits_per_pixel = 12;
         v->format         = XvPlanar;
         memcpy(v->component_order, "YUV", 3);
         break;
      default:
         v->bits_per_pixel = 16;
         v->format         = XvPacked;
         memcpy(v->component_order, "YUYV", 4);
         break;
   }
   *count = 1;
   return v;
}

int XvSetPortAttribute(Display *dpy, XvPortID port, Atom attribute,
      int value)
{
   (void)dpy;
   (void)attribute;
   (void)value;
   return port == STANDIN_PORT ? Success : XvBadPort;
}

/* One block, freed with XFree() as libXv's is: the image, then its
 * pitches and offsets */
XvImage *XvShmCreateImage(Display *dpy, XvPortID port, int id,
      char *data, int width, int height, XShmSegmentInfo *shminfo)
{
   XvImage *img;
   int planes = id == standin_fourcc(FMT_I420) ? 3 : 1;
   (void)dpy;
   if (port != STANDIN_PORT || width <= 0 || height <= 0)
      return NULL;
   if (width > STANDIN_MAX)
      width = STANDIN_MAX;
   if (height > STANDIN_MAX)
      height = STANDIN_MAX;
   width  = (width + 1) & ~1;
   if (planes == 3)
      height = (height + 1) & ~1;
   if (!(img = (XvImage*)calloc(1, sizeof(*img) + 2 * 3 * sizeof(int))))
      return NULL;
   img->pitches    = (int*)(img + 1);
   img->offsets    = img->pitches + 3;
   img->id         = id;
   img->width      = width;
   img->height     = height;
   img->num_planes = planes;
   img->data       = data;
   /* As libXv keeps it, for the put's completion event */
   img->obdata     = (XPointer)shminfo;
   if (planes == 3)
   {
      img->pitches[0] = width;
      img->pitches[1] = img->pitches[2] = width / 2;
      img->offsets[0] = 0;
      img->offsets[1] = width * height;
      img->offsets[2] = width * height + (width / 2) * (height / 2);
      img->data_size  = width * height * 3 / 2;
   }
   else
   {
      img->pitches[0] = width * 2;
      img->offsets[0] = 0;
      img->data_size  = width * height * 2;
   }
   return img;
}

static void standin_rgb(int y, int u, int v, uint8_t *rgb)
{
   int c = y - 16, d = u - 128, e = v - 128;
   int r = (298 * c + 409 * e + 128) >> 8;
   int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
   int b = (298 * c + 516 * d + 128) >> 8;
   rgb[0] = (uint8_t)(r < 0 ? 0 : r > 255 ? 255 : r);
   rgb[1] = (uint8_t)(g < 0 ? 0 : g > 255 ? 255 : g);
   rgb[2] = (uint8_t)(b < 0 ? 0 : b > 255 ? 255 : b);
}

static void standin_sample(const XvImage *img, int x, int y, uint8_t *rgb)
{
   const uint8_t *d = (const uint8_t*)img->data;
   if (img->num_planes == 3)
   {
      /* I420: Y, then U, then V */
      size_t ci = (size_t)(y / 2) * img->pitches[1] + x / 2;
      standin_rgb(d[(size_t)y * img->pitches[0] + x],
            d[img->offsets[1] + ci], d[img->offsets[2] + ci], rgb);
   }
   else
   {
      const uint8_t *m = d + (size_t)y * img->pitches[0] + (x / 2) * 4;
      if (img->id == standin_fourcc(FMT_UYVY))
         standin_rgb(m[(x & 1) ? 3 : 1], m[0], m[2], rgb);
      else
         standin_rgb(m[(x & 1) ? 2 : 0], m[1], m[3], rgb);
   }
}

int XvShmPutImage(Display *dpy, XvPortID port, Drawable d, GC gc,
      XvImage *img, int src_x, int src_y, unsigned src_w, unsigned src_h,
      int dst_x, int dst_y, unsigned dst_w, unsigned dst_h, Bool send_event)
{
   XImage *out;
   char *pixels;
   unsigned x, y;
   if (port != STANDIN_PORT || !img || !img->data)
      return XvBadPort;
   if (     src_x < 0 || src_y < 0 || !src_w || !src_h
         || src_x + (int)src_w > img->width
         || src_y + (int)src_h > img->height)
   {
      fprintf(stderr, "[xv_standin] put of %ux%u+%d+%d from a %dx%d image\n",
            src_w, src_h, src_x, src_y, img->width, img->height);
      abort();
   }
   if (!dst_w || !dst_h)
      return Success;
   if (!(pixels = (char*)malloc((size_t)dst_w * dst_h * 4)))
      return XvBadAlloc;
   for (y = 0; y < dst_h; y++)
   {
      int sy = src_y + (int)(((uint64_t)y * src_h) / dst_h);
      for (x = 0; x < dst_w; x++)
      {
         uint8_t rgb[3];
         uint8_t *p = (uint8_t*)pixels + ((size_t)y * dst_w + x) * 4;
         int sx     = src_x + (int)(((uint64_t)x * src_w) / dst_w);
         standin_sample(img, sx, sy, rgb);
         p[0] = rgb[2];
         p[1] = rgb[1];
         p[2] = rgb[0];
         p[3] = 0;
      }
   }
   out = XCreateImage(dpy, DefaultVisual(dpy, DefaultScreen(dpy)),
         DefaultDepth(dpy, DefaultScreen(dpy)), ZPixmap, 0, pixels,
         dst_w, dst_h, 32, 0);
   if (!out)
   {
      free(pixels);
      return XvBadAlloc;
   }
   XPutImage(dpy, d, gc, out, 0, 0, dst_x, dst_y, dst_w, dst_h);
   XDestroyImage(out);
   if (send_event && img->obdata)
   {
      XShmCompletionEvent done;
      memset(&done, 0, sizeof(done));
      done.type       = XShmGetEventBase(dpy) + ShmCompletion;
      done.display    = dpy;
      done.drawable   = d;
      done.major_code = 0;
      done.shmseg     = ((XShmSegmentInfo*)img->obdata)->shmseg;
      XPutBackEvent(dpy, (XEvent*)&done);
   }
   return Success;
}

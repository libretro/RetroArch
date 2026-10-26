/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016-2017 - Alfred Agrell
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

/* The X11 driver: everything is drawn on the CPU into an image the size
 * of the window, shared with the X server through MIT-SHM where it can
 * be and sent with XPutImage where it cannot (a remote display). The
 * frame, or each of a core's views, is scaled into it, the menu's
 * texture blended over it and messages drawn with the font driver. */

#define _XOPEN_SOURCE 700 /* TODO: this doesn't really belong here. */

#include <stdlib.h>
#include <string.h>

#include <sys/shm.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

#include <retro_atomic.h>
#include <encodings/utf.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../common/x11_common.h"
#include "../font_driver.h"
#include "../video_views.h"
#include "../video_views_cpu.h"
#include "../../configuration.h"
#include "../../frontend/frontend_driver.h"
#include "../../verbosity.h"

/* Images drawn in turn: the server may still be reading the last one
 * shown while the next frame is drawn */
#define XSHM_IMAGES 2

struct xshm_image
{
   XShmSegmentInfo shm;
   XImage *image;
   /* Sent, and the server not yet done reading it */
   bool pending;
};

typedef struct xshm
{
   struct xshm_image images[XSHM_IMAGES];
   /* The frame, each of a core's views and the menu, scaled */
   video_views_cpu_t cpu;
   video_viewport_t vp;
   /* The images' own connection, whose completion events nothing else
    * reads */
   Display *dpy;
   /* The image drawn last, XRGB8888 rows width apart */
   uint32_t *fbptr;
   /* Both eyes side by side, for anaglyph and interlaced */
   uint32_t *canvas;
   GC gc;

   struct
   {
      /* A copy of the menu's texture, and that scaled to where it is
       * drawn */
      void *frame;
      uint32_t *scaled;
      size_t frame_size;
      size_t scaled_size;
      unsigned dims;
      enum scaler_pix_fmt fmt;
      float alpha;
      bool enable;
   } menu;

   void *font;
   const font_renderer_driver_t *font_driver;

   int width;
   int height;
   int completion;
   unsigned shown;
   unsigned canvas_dims;
   /* For get_flags: the last frame with a map was drawn whole */
   retro_atomic_int_t fallback;
   enum scaler_pix_fmt fmt;
   uint8_t font_r;
   uint8_t font_g;
   uint8_t font_b;
   bool use_shm;
   bool smooth;
   bool fullscreen;
   bool direct_seen;
} xshm_t;

/* The image's rows, in bytes */
#define XSHM_STRIDE(xshm) ((unsigned)(xshm)->width * sizeof(uint32_t))

/*
 * IMAGE
 */

static void xshm_images_free(xshm_t *xshm)
{
   unsigned i;
   bool synced = false;
   for (i = 0; i < XSHM_IMAGES; i++)
   {
      struct xshm_image *im = &xshm->images[i];
      if (!im->image)
         continue;
      if (xshm->use_shm)
      {
         XShmDetach(xshm->dpy, &im->shm);
         /* The server lets the segment go before it is unmapped */
         if (!synced)
            XSync(xshm->dpy, False);
         synced         = true;
         im->image->data = NULL;
         XDestroyImage(im->image);
         shmdt(im->shm.shmaddr);
      }
      else
         XDestroyImage(im->image);
      im->image   = NULL;
      im->pending = false;
   }
   xshm->fbptr = NULL;
}

/* The X server's answer to attaching a segment: a remote server cannot
 * map it */
static bool xshm_attach_failed;

static int xshm_attach_error(Display *dpy, XErrorEvent *event)
{
   xshm_attach_failed = true;
   return 0;
}

static bool xshm_image_shm(xshm_t *xshm, struct xshm_image *im,
      int width, int height)
{
   int (*old)(Display*, XErrorEvent*);
   im->image = XShmCreateImage(xshm->dpy,
         DefaultVisual(xshm->dpy, DefaultScreen(xshm->dpy)), 24, ZPixmap,
         NULL, &im->shm, width, height);
   if (!im->image)
      return false;
   im->shm.shmid = shmget(IPC_PRIVATE,
         (size_t)im->image->bytes_per_line * height, IPC_CREAT | 0600);
   if (im->shm.shmid < 0)
   {
      XDestroyImage(im->image);
      im->image = NULL;
      return false;
   }
   im->shm.shmaddr = (char*)shmat(im->shm.shmid, 0, 0);
   /* Gone with the last detach, whatever happens to this process */
   shmctl(im->shm.shmid, IPC_RMID, NULL);
   if (im->shm.shmaddr == (char*)-1)
   {
      XDestroyImage(im->image);
      im->image = NULL;
      return false;
   }
   im->image->data    = im->shm.shmaddr;
   im->shm.readOnly   = False;
   xshm_attach_failed = false;
   old                = XSetErrorHandler(xshm_attach_error);
   XShmAttach(xshm->dpy, &im->shm);
   XSync(xshm->dpy, False);
   XSetErrorHandler(old);
   if (xshm_attach_failed)
   {
      im->image->data = NULL;
      XDestroyImage(im->image);
      shmdt(im->shm.shmaddr);
      im->image = NULL;
      return false;
   }
   return true;
}

static bool xshm_image_plain(xshm_t *xshm, struct xshm_image *im,
      int width, int height)
{
   size_t pitch = (size_t)width * sizeof(uint32_t);
   char *data   = (char*)calloc((size_t)height, pitch);
   if (!data)
      return false;
   im->image    = XCreateImage(xshm->dpy,
         DefaultVisual(xshm->dpy, DefaultScreen(xshm->dpy)), 24, ZPixmap,
         0, data, width, height, 32, (int)pitch);
   if (!im->image)
   {
      free(data);
      return false;
   }
   return true;
}

/* The images at the window's size: shared when the server takes them,
 * else one, which XPutImage copies as it is sent */
static bool xshm_image_make(xshm_t *xshm, unsigned dims)
{
   unsigned i;
   int width  = (int)VIDEO_SCALE_W(dims);
   int height = (int)VIDEO_SCALE_H(dims);
   xshm_images_free(xshm);
   if (!width || !height)
      return false;
   for (i = 0; i < XSHM_IMAGES && xshm->use_shm; i++)
   {
      if (xshm_image_shm(xshm, &xshm->images[i], width, height))
         continue;
      RARCH_LOG("[XSHM] Shared memory refused; images go by XPutImage.\n");
      xshm_images_free(xshm);
      xshm->use_shm = false;
   }
   if (     !xshm->use_shm
         && !xshm_image_plain(xshm, &xshm->images[0], width, height))
      return false;
   for (i = 0; i < XSHM_IMAGES; i++)
   {
      XImage *image = xshm->images[i].image;
      /* 32 bits a pixel, rows packed */
      if (image && (image->bits_per_pixel != 32
               || image->bytes_per_line != width * 4))
      {
         RARCH_ERR("[XSHM] The display takes no 32-bit images.\n");
         xshm_images_free(xshm);
         return false;
      }
      if (image)
         memset(image->data, 0, (size_t)height * width * 4);
   }
   xshm->shown        = 0;
   xshm->fbptr        = (uint32_t*)xshm->images[0].image->data;
   xshm->width        = width;
   xshm->height       = height;
   xshm->vp.full_dims = dims;
   video_driver_set_output_dims(dims);
   return true;
}

/* The image to draw this frame into, its pixels in fbptr: one the
 * server is done reading, the other than the one shown when it can be.
 * NULL while the server reads every one: the frame is not shown. */
static struct xshm_image *xshm_image_next(xshm_t *xshm)
{
   unsigned i;
   XEvent event;
   if (!xshm->use_shm)
      return &xshm->images[0];
   /* What the server has finished, without waiting on it */
   while (XCheckTypedEvent(xshm->dpy, xshm->completion, &event))
   {
      const XShmCompletionEvent *done = (const XShmCompletionEvent*)&event;
      for (i = 0; i < XSHM_IMAGES; i++)
         if (xshm->images[i].shm.shmseg == done->shmseg)
            xshm->images[i].pending = false;
   }
   for (i = 1; i <= XSHM_IMAGES; i++)
   {
      struct xshm_image *im = &xshm->images[(xshm->shown + i) % XSHM_IMAGES];
      if (!im->pending)
         return im;
   }
   return NULL;
}

/*
 * TEXT
 */

static void xshm_init_font(xshm_t *xshm, settings_t *settings)
{
   int r, g, b;
   const char *path_font = settings->paths.path_font;
   if (!settings->bools.video_font_enable)
      return;
   if (!font_renderer_create_default(&xshm->font_driver, &xshm->font,
            *path_font ? path_font : NULL,
            settings->floats.video_font_size, FONT_ATLAS_FORMAT_A8))
   {
      RARCH_LOG("[XSHM] Could not initialize fonts.\n");
      return;
   }
   /* Glyphs are blended from the atlas in memory: it may grow freely */
   xshm->font_driver->get_atlas(xshm->font)->max_dims =
      VIDEO_SCALE_PACK(2048, 2048);
   r = (int)(settings->floats.video_msg_color_r * 255);
   g = (int)(settings->floats.video_msg_color_g * 255);
   b = (int)(settings->floats.video_msg_color_b * 255);
   xshm->font_r = (r < 0) ? 0 : (r > 255) ? 255 : r;
   xshm->font_g = (g < 0) ? 0 : (g > 255) ? 255 : g;
   xshm->font_b = (b < 0) ? 0 : (b > 255) ? 255 : b;
}

/* @msg into the @width x @height rectangle of the image at @x0, @y0,
 * placed at @pos_x, @pos_y of it as a fraction, from its bottom */
static void xshm_render_msg(xshm_t *xshm, const char *msg,
      int x0, int y0, int width, int height, float pos_x, float pos_y)
{
   int x, y;
   const struct font_atlas *atlas = NULL;
   int msg_base_x, msg_base_y;

   if (!xshm->font || !msg || !*msg)
      return;

   atlas      = xshm->font_driver->get_atlas(xshm->font);
   msg_base_x = (int)(pos_x * width);
   msg_base_y = (int)((1.0f - pos_y) * height);

   {
      const struct font_glyph* (*get_glyph)(void*, uint32_t)
                                             = xshm->font_driver->get_glyph;
      void *font_data                        = xshm->font;
      const struct font_glyph *glyph_q       = get_glyph(font_data, '?');
      struct font_line_metrics *line_metrics = NULL;
      size_t msg_len                         = strlen(msg);
      int line_h                             = 0;
      int line_x                             = msg_base_x;
      int line_y                             = msg_base_y;

      xshm->font_driver->get_line_metrics(font_data, &line_metrics);
      if (line_metrics)
         line_h = (int)line_metrics->height;

#define FONT_LAYOUT_ALIGNED 0
#define FONT_LAYOUT_LINE(line, line_width, count, bytes) \
      do \
      { \
         (void)(line_width); \
         (void)(count); \
         (void)(bytes); \
         line_x = msg_base_x; \
         line_y = msg_base_y + (line) * line_h; \
      } while (0)
#define FONT_LAYOUT_GLYPH(glyph, pen_x, pen_y) \
      do \
      { \
         int glyph_width, glyph_height; \
         int base_x, base_y, max_width, max_height; \
         uint32_t       *out = NULL; \
         const uint8_t  *src = NULL; \
         glyph_width  = VIDEO_SCALE_W(glyph->dims); \
         glyph_height = VIDEO_SCALE_H(glyph->dims); \
         base_x       = (line_x + (pen_x)) + glyph->draw_offset_x; \
         base_y       = (line_y + (pen_y)) + glyph->draw_offset_y; \
         src          = atlas->buffer + VIDEO_SCALE_W(glyph->atlas_pos) \
            + VIDEO_SCALE_H(glyph->atlas_pos) * atlas->width; \
         if (base_x < 0) \
         { \
            src         -= base_x; \
            glyph_width += base_x; \
            base_x       = 0; \
         } \
         if (base_y < 0) \
         { \
            src          -= base_y * (int)atlas->width; \
            glyph_height += base_y; \
            base_y        = 0; \
         } \
         max_width  = width - base_x; \
         max_height = height - base_y; \
         if (max_width <= 0 || max_height <= 0 || glyph_width <= 0 \
               || glyph_height <= 0) \
            break; \
         if (glyph_width > max_width) \
            glyph_width = max_width; \
         if (glyph_height > max_height) \
            glyph_height = max_height; \
         out = xshm->fbptr + (size_t)(y0 + base_y) * xshm->width \
            + x0 + base_x; \
         for (y = 0; y < glyph_height; \
               y++, src += atlas->width, out += xshm->width) \
         { \
            for (x = 0; x < glyph_width; x++) \
            { \
               unsigned blend = src[x]; \
               unsigned r, g, b; \
               if (!blend) \
                  continue; \
               r      = (out[x] >> 16) & 0xff; \
               g      = (out[x] >>  8) & 0xff; \
               b      =  out[x]        & 0xff; \
               r      = (r * (256 - blend) + xshm->font_r * blend) >> 8; \
               g      = (g * (256 - blend) + xshm->font_g * blend) >> 8; \
               b      = (b * (256 - blend) + xshm->font_b * blend) >> 8; \
               out[x] = (r << 16) | (g << 8) | b; \
            } \
         } \
      } while (0)
#include "../font_layout.h"
   }
}

/*
 * MENU
 */

/* The menu's texture scaled over the @width x @height rectangle of the
 * image at @x0, @y0 and blended by its alpha */
static void xshm_draw_menu(xshm_t *xshm, int x0, int y0, int width,
      int height)
{
   int x, y;
   struct scaler_ctx *ctx;
   size_t size        = (size_t)width * height * sizeof(uint32_t);
   unsigned mw        = VIDEO_SCALE_W(xshm->menu.dims);
   unsigned mh        = VIDEO_SCALE_H(xshm->menu.dims);
   unsigned bpp       = (xshm->menu.fmt == SCALER_FMT_ARGB8888) ? 4 : 2;
   unsigned alpha     = (unsigned)(xshm->menu.alpha * 256.0f);
   int cx0            = (x0 < 0) ? 0 : x0;
   int cy0            = (y0 < 0) ? 0 : y0;
   int cx1            = (x0 + width  > xshm->width)  ? xshm->width  : x0 + width;
   int cy1            = (y0 + height > xshm->height) ? xshm->height : y0 + height;

   if (!xshm->menu.frame || !mw || !mh || width <= 0 || height <= 0
         || cx1 <= cx0 || cy1 <= cy0)
      return;
   if (size > xshm->menu.scaled_size)
   {
      uint32_t *grown = (uint32_t*)realloc(xshm->menu.scaled, size);
      if (!grown)
         return;
      xshm->menu.scaled      = grown;
      xshm->menu.scaled_size = size;
   }
   if (!(ctx = video_views_cpu_scaler(&xshm->cpu, xshm->menu.fmt,
            SCALER_TYPE_BILINEAR, (int)mw, (int)mh, (int)(mw * bpp),
            width, height, width * (int)sizeof(uint32_t))))
      return;
   scaler_ctx_scale_direct(ctx, xshm->menu.scaled, xshm->menu.frame);
   if (alpha > 256)
      alpha = 256;
   for (y = cy0; y < cy1; y++)
   {
      const uint32_t *src = xshm->menu.scaled
         + (size_t)(y - y0) * width + (cx0 - x0);
      uint32_t *out       = xshm->fbptr + (size_t)y * xshm->width + cx0;
      for (x = cx0; x < cx1; x++, src++, out++)
      {
         unsigned a = ((*src >> 24) * alpha) >> 8;
         unsigned r, g, b;
         if (!a)
            continue;
         r    = (((*src >> 16) & 0xff) * a
              + ((*out >> 16) & 0xff) * (255 - a)) / 255;
         g    = (((*src >>  8) & 0xff) * a
              + ((*out >>  8) & 0xff) * (255 - a)) / 255;
         b    = (( *src        & 0xff) * a
              + ( *out        & 0xff) * (255 - a)) / 255;
         *out = (r << 16) | (g << 8) | b;
      }
   }
}

/*
 * VIEWS
 */

/* Whether this frame draws its views. Anaglyph and interlaced need the
 * canvas, both eyes side by side. */
static bool xshm_views_setup(xshm_t *xshm,
      const video_frame_info_t *video_info, unsigned frame_dims)
{
   const video_views_layout_t *layout = &video_info->views_layout;
   bool views = video_info->views.num_views
      && video_views_fit_frame(&video_info->views, frame_dims);

   if (views && layout->offscreen && xshm->canvas_dims != layout->canvas_dims)
   {
      free(xshm->canvas);
      xshm->canvas_dims = 0;
      if ((xshm->canvas = (uint32_t*)malloc(VIDEO_SCALE_AREA(
                  layout->canvas_dims) * sizeof(uint32_t))))
         xshm->canvas_dims = layout->canvas_dims;
      views = xshm->canvas != NULL;
   }
   else if (!views || !layout->offscreen)
   {
      free(xshm->canvas);
      xshm->canvas      = NULL;
      xshm->canvas_dims = 0;
   }
   retro_atomic_store_release_int(&xshm->fallback,
         (video_info->views.num_views && !views) ? 1 : 0);
   if (views && !xshm->direct_seen)
   {
      xshm->direct_seen = true;
      RARCH_LOG("[XSHM] Views drawn straight from the frame.\n");
   }
   return views;
}

/* The frame's views scaled into the image, or into the canvas whose
 * halves are blended into it */
static void xshm_views_render(xshm_t *xshm,
      const video_frame_info_t *video_info, const void *frame,
      unsigned pitch)
{
   /* Each view turned in its own place, as the layout laid them out */
   xshm->cpu.rot = video_driver_get_rotation_snapshot();
   video_views_cpu_compose(&xshm->cpu, &video_info->views,
         &video_info->views_layout, frame, pitch, xshm->fmt,
         xshm->smooth ? SCALER_TYPE_BILINEAR : SCALER_TYPE_POINT, 1.0f,
         VIDEO_SCALE_PACK(xshm->width, xshm->height), xshm->fbptr,
         (unsigned)xshm->width, xshm->canvas);
}

/*
 * VIDEO DRIVER
 */

static void xshm_free(void *data);

static void *xshm_init(const video_info_t *video)
{
   unsigned dims;
   XEvent event;
   char title[128];
   XSetWindowAttributes attributes;
   settings_t *settings = config_get_ptr();
   xshm_t *xshm         = (xshm_t*)calloc(1, sizeof(xshm_t));

   if (!xshm)
      return NULL;

   XInitThreads();

   if (!(g_x11_dpy = XOpenDisplay(NULL)))
   {
      RARCH_ERR("[XSHM] Cannot connect to the X server.\n");
      free(xshm);
      return NULL;
   }

   if (DefaultDepth(g_x11_dpy, DefaultScreen(g_x11_dpy)) != 24)
   {
      RARCH_ERR("[XSHM] The display is not 24-bit.\n");
      XCloseDisplay(g_x11_dpy);
      g_x11_dpy = NULL;
      free(xshm);
      return NULL;
   }

   if (!(xshm->dpy = XOpenDisplay(NULL)))
   {
      RARCH_ERR("[XSHM] Cannot connect to the X server.\n");
      XCloseDisplay(g_x11_dpy);
      g_x11_dpy = NULL;
      free(xshm);
      return NULL;
   }
   xshm->use_shm    = XShmQueryExtension(xshm->dpy) ? true : false;
   if (!xshm->use_shm)
      RARCH_LOG("[XSHM] XShm extension not found.\n");
   else
      xshm->completion = XShmGetEventBase(xshm->dpy) + ShmCompletion;
   xshm->fmt        = video->rgb32 ? SCALER_FMT_ARGB8888 : SCALER_FMT_RGB565;
   xshm->smooth     = video->smooth;
   xshm->fullscreen = video->fullscreen;

   /* Fullscreen at the desktop's size is asked for as 0x0 */
   dims             = video->dims;
   if (!VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims))
      dims          = VIDEO_SCALE_PACK(
            DisplayWidth(g_x11_dpy,  DefaultScreen(g_x11_dpy)),
            DisplayHeight(g_x11_dpy, DefaultScreen(g_x11_dpy)));

   memset(&attributes, 0, sizeof(attributes));
   attributes.border_pixel = 0;
   attributes.event_mask   = StructureNotifyMask | KeyPressMask
      | KeyReleaseMask | ButtonReleaseMask | ButtonPressMask
      | ExposureMask | FocusChangeMask | EnterWindowMask | LeaveWindowMask;
   g_x11_win = XCreateWindow(g_x11_dpy, DefaultRootWindow(g_x11_dpy),
         0, 0, VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims), 0, 24,
         InputOutput, CopyFromParent, CWBorderPixel | CWEventMask,
         &attributes);
   XSetWindowBackground(g_x11_dpy, g_x11_win, 0);

   if (video->fullscreen)
      x11_set_net_wm_fullscreen_hint(g_x11_dpy, g_x11_win);

   XMapWindow(g_x11_dpy, g_x11_win);

   title[0] = '\0';
   video_driver_get_window_title(title, sizeof(title));
   if (title[0])
      XStoreName(g_x11_dpy, g_x11_win, title);
   x11_set_window_attr(g_x11_dpy, g_x11_win);

   if (video->fullscreen)
   {
      /* Asked again once shown: some window managers miss the hint */
      x11_event_queue_check(&event);
      x11_set_net_wm_fullscreen(g_x11_dpy, g_x11_win);
      XFlush(g_x11_dpy);
      x11_show_mouse(xshm, false);
   }

   /* The window is shown before the images go to it */
   XSync(g_x11_dpy, False);
   xshm->gc = XCreateGC(xshm->dpy, g_x11_win, 0, NULL);

   if (!xshm_image_make(xshm, dims))
      goto error;

   x11_install_quit_atom();
   frontend_driver_install_signal_handler();

   xshm_init_font(xshm, settings);

   if (!x11_input_ctx_new(true))
      goto error;

   /* no input driver of this driver's own: the frontend starts the
    * one that goes with an X11 window */
   input_driver_video_window(INPUT_WINDOW_X11, NULL);

   return xshm;

error:
   xshm_free(xshm);
   return NULL;
}

static bool xshm_frame(void *data, const void *frame,
      unsigned dims, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   unsigned width     = VIDEO_SCALE_W(dims);
   unsigned height    = VIDEO_SCALE_H(dims);
   unsigned window    = 0;
   bool quit          = false;
   bool resize        = false;
   bool views         = false;
   bool per_eye       = false;
   struct xshm_image *im = NULL;
   xshm_t      *xshm  = (xshm_t*)data;
   const video_views_layout_t *layout = &video_info->views_layout;
#ifdef HAVE_MENU
   bool menu_is_alive = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif

   /* The images follow the window's size */
   window = VIDEO_SCALE_PACK(xshm->width, xshm->height);
   x11_check_window(xshm, &quit, &resize, &window);
   if (resize && !xshm_image_make(xshm, window))
      return false;
   /* Drawn only into an image the server is done reading. While it is
    * behind on all of them the frame goes unshown, never waited for. */
   if (!(im = xshm_image_next(xshm)))
      return true;
   xshm->fbptr = (uint32_t*)im->image->data;

   if (frame && width && height)
   {
      views   = xshm_views_setup(xshm, video_info, dims);
      per_eye = views && video_views_ui_per_eye(&video_info->views,
            layout, true);
      /* The viewport is the whole window while views draw. */
      video_driver_update_viewport(&xshm->vp, views, true, true);
      memset(xshm->fbptr, 0, (size_t)xshm->height * XSHM_STRIDE(xshm));
      if (views)
         xshm_views_render(xshm, video_info, frame, pitch);
      else
      {
         video_views_rect_t dst, cut;
         dst.pos  = xshm->vp.pos;
         dst.dims = xshm->vp.dims;
         if (video_views_clip(&dst,
                  VIDEO_SCALE_PACK(xshm->width, xshm->height), &cut))
            video_views_cpu_draw(&xshm->cpu, xshm->fmt,
                  xshm->smooth ? SCALER_TYPE_BILINEAR : SCALER_TYPE_POINT,
                  frame, width, height, pitch, (uint8_t*)xshm->fbptr,
                  XSHM_STRIDE(xshm), &dst, &cut);
      }
   }
   else if (!frame)
      memset(xshm->fbptr, 0, (size_t)xshm->height * XSHM_STRIDE(xshm));

#ifdef HAVE_MENU
   menu_driver_frame(menu_is_alive, video_info);

   if (xshm->menu.enable)
   {
      unsigned a;
      struct video_viewport fit;
      /* In the game's viewport; while views draw, where that would be
       * in the window or, laid out at one eye's UI size, in each eye */
      fit = xshm->vp;
      if (views)
      {
         memset(&fit, 0, sizeof(fit));
         fit.full_dims = per_eye ? layout->ui_dims
            : VIDEO_SCALE_PACK(xshm->width, xshm->height);
         video_driver_update_viewport(&fit, false, true, true);
      }
      if (!per_eye)
         xshm_draw_menu(xshm, VIDEO_POS_X(fit.pos), VIDEO_POS_Y(fit.pos),
               (int)VIDEO_SCALE_W(fit.dims), (int)VIDEO_SCALE_H(fit.dims));
      else
         for (a = 0; a < layout->num_areas; a++)
         {
            const video_views_rect_t *r = &layout->areas[a];
            double sx = (double)VIDEO_SCALE_W(r->dims)
               / VIDEO_SCALE_W(layout->ui_dims);
            double sy = (double)VIDEO_SCALE_H(r->dims)
               / VIDEO_SCALE_H(layout->ui_dims);
            xshm_draw_menu(xshm,
                  VIDEO_POS_X(r->pos) + (int)(VIDEO_POS_X(fit.pos) * sx),
                  VIDEO_POS_Y(r->pos) + (int)(VIDEO_POS_Y(fit.pos) * sy),
                  (int)(VIDEO_SCALE_W(fit.dims) * sx + 0.5),
                  (int)(VIDEO_SCALE_H(fit.dims) * sy + 0.5));
         }
   }
   else
#endif
   if (video_info->statistics_show)
   {
      xshm_render_msg(xshm, video_info->stat_text, 0, 0,
            xshm->width, xshm->height, video_info->osd_stat_params.x,
            video_info->osd_stat_params.y);
   }

   if (msg && *msg)
   {
      unsigned a;
      if (!per_eye)
         xshm_render_msg(xshm, msg, 0, 0, xshm->width, xshm->height,
               video_info->font_msg_pos_x, video_info->font_msg_pos_y);
      else
         for (a = 0; a < layout->num_areas; a++)
         {
            video_views_rect_t cut;
            /* Drawn in what of each area the window shows */
            if (video_views_clip(&layout->areas[a],
                     VIDEO_SCALE_PACK(xshm->width, xshm->height), &cut))
               xshm_render_msg(xshm, msg,
                     VIDEO_POS_X(cut.pos), VIDEO_POS_Y(cut.pos),
                     (int)VIDEO_SCALE_W(cut.dims),
                     (int)VIDEO_SCALE_H(cut.dims),
                     video_info->font_msg_pos_x,
                     video_info->font_msg_pos_y);
         }
   }

   if (xshm->use_shm)
   {
      /* Its completion says when it may be drawn into again */
      XShmPutImage(xshm->dpy, g_x11_win, xshm->gc, im->image,
            0, 0, 0, 0, xshm->width, xshm->height, True);
      im->pending = true;
   }
   else
      XPutImage(xshm->dpy, g_x11_win, xshm->gc, im->image,
            0, 0, 0, 0, xshm->width, xshm->height);
   XFlush(xshm->dpy);
   xshm->shown = (unsigned)(im - xshm->images);

   x11_update_title(NULL);

   return true;
}

static void xshm_set_nonblock_state(void *a, bool b, bool c, unsigned d) { }

static bool xshm_has_windowed(void *data) { return true; }

static void xshm_free(void *data)
{
   xshm_t *xshm = (xshm_t*)data;
   if (!xshm)
      return;
   if (xshm->dpy)
   {
      xshm_images_free(xshm);
      if (xshm->gc)
         XFreeGC(xshm->dpy, xshm->gc);
      XCloseDisplay(xshm->dpy);
   }
   if (g_x11_dpy)
   {
      x11_input_ctx_destroy();
      x11_window_destroy(xshm->fullscreen);
      XCloseDisplay(g_x11_dpy);
      g_x11_dpy = NULL;
   }
   if (xshm->font)
      xshm->font_driver->free(xshm->font);
   video_views_cpu_free(&xshm->cpu);
   free(xshm->canvas);
   free(xshm->menu.frame);
   free(xshm->menu.scaled);
   free(xshm);
}

static void xshm_poke_set_filtering(void *data, unsigned index, bool smooth, bool ctx_scaling)
{
   xshm_t *xshm = (xshm_t*)data;
   if (xshm)
      xshm->smooth = smooth;
}

/* The viewport is laid out every frame */
static void xshm_poke_set_aspect_ratio(void *data, unsigned aspect_ratio_idx) { }
static void xshm_poke_apply_state_changes(void *data) { }

/* The menu's texture, copied: the menu may write it again before the
 * frame that draws it */
static void xshm_poke_set_texture_frame(void *data,
      const void *frame, bool rgb32,
      unsigned dims, float alpha)
{
   xshm_t *xshm = (xshm_t*)data;
   size_t size  = VIDEO_SCALE_AREA(dims) * (rgb32 ? 4 : 2);
   if (!xshm || !frame || !size)
      return;
   if (size > xshm->menu.frame_size)
   {
      void *grown = realloc(xshm->menu.frame, size);
      if (!grown)
         return;
      xshm->menu.frame      = grown;
      xshm->menu.frame_size = size;
   }
   memcpy(xshm->menu.frame, frame, size);
   xshm->menu.dims  = dims;
   xshm->menu.fmt   = rgb32 ? SCALER_FMT_ARGB8888 : SCALER_FMT_RGBA4444;
   xshm->menu.alpha = alpha;
}

static void xshm_poke_texture_enable(void *data,
      bool enable, bool full_screen)
{
   xshm_t *xshm = (xshm_t*)data;
   if (xshm)
      xshm->menu.enable = enable;
}

static void xshm_poke_set_osd_msg(void *data,
      const char *msg, size_t msg_len,
      const struct font_params *params, void *font)
{
   xshm_t *xshm = (xshm_t*)data;
   if (!xshm || !msg)
      return;
   xshm_render_msg(xshm, msg, 0, 0, xshm->width, xshm->height,
         params ? params->x : 0.0f, params ? params->y : 0.0f);
}

static uint32_t xshm_get_flags(void *data)
{
   uint32_t flags = 0;
   xshm_t  *xshm  = (xshm_t*)data;
   BIT32_SET(flags, GFX_CTX_FLAGS_SCREENSHOTS_SUPPORTED);
   if (xshm)
   {
      BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS);
      if (retro_atomic_load_acquire_int(&xshm->fallback))
         BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS_FALLBACK);
   }
   return flags;
}

static void xshm_viewport_info(void *data, struct video_viewport *vp)
{
   xshm_t *xshm = (xshm_t*)data;
   if (xshm)
      *vp = xshm->vp;
}

/* The viewport's pixels in the image the last frame was composed in,
 * bottom row first as BGR24 */
static bool xshm_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   int x, y, vx, vy, vw, vh;
   xshm_t *xshm = (xshm_t*)data;
   if (!xshm || !xshm->fbptr)
      return false;
   vx = VIDEO_POS_X(xshm->vp.pos);
   vy = VIDEO_POS_Y(xshm->vp.pos);
   vw = (int)VIDEO_SCALE_W(xshm->vp.dims);
   vh = (int)VIDEO_SCALE_H(xshm->vp.dims);
   for (y = vh - 1; y >= 0; y--)
   {
      for (x = 0; x < vw; x++)
      {
         uint32_t px = 0;
         int sx      = vx + x;
         int sy      = vy + y;
         if (sx >= 0 && sy >= 0 && sx < xshm->width && sy < xshm->height)
            px = xshm->fbptr[(size_t)sy * xshm->width + sx];
         *buffer++ = (uint8_t)(px);
         *buffer++ = (uint8_t)(px >> 8);
         *buffer++ = (uint8_t)(px >> 16);
      }
   }
   return true;
}

static void xshm_grab_mouse_toggle(void *data) { }

static video_poke_interface_t xshm_video_poke_interface = {
   xshm_get_flags,
   NULL, /* load_texture */
   NULL, /* unload_texture */
   NULL, /* set_video_mode */
#ifdef HAVE_XF86VM
   x11_get_refresh_rate,
#else
   NULL, /* get_refresh_rate */
#endif
   xshm_poke_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   xshm_poke_set_aspect_ratio,
   xshm_poke_apply_state_changes,
   xshm_poke_set_texture_frame,
   xshm_poke_texture_enable,
   xshm_poke_set_osd_msg,
   x11_show_mouse,
   xshm_grab_mouse_toggle,
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL  /* set_hdr_subpixel_layout */
};

static void xshm_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   *iface = &xshm_video_poke_interface;
}

static bool xshm_set_shader(void *data,
      enum rarch_shader_type type, const char *path) { return false; }

video_driver_t video_xshm = {
   xshm_init,
   xshm_frame,
   xshm_set_nonblock_state,
   x11_alive,
   x11_has_focus_internal,
   x11_suspend_screensaver,
   xshm_has_windowed,
   xshm_set_shader,
   xshm_free,
   "x11",
   NULL, /* set_viewport */
   NULL, /* set_rotation */
   xshm_viewport_info,
   xshm_read_viewport,
#ifdef HAVE_OVERLAY
   NULL, /* get_overlay_interface */
#endif
   xshm_get_poke_interface,
   NULL, /* wrap_type_to_enum */
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   NULL  /* gfx_widgets_enabled */
#endif
};

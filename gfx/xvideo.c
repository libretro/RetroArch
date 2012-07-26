/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

#include "driver.h"
#include "general.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#ifdef HAVE_FREETYPE
#include "fonts/fonts.h"
#endif
#include "gfx_common.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>

// Adapted from bSNES and MPlayer source.

typedef struct xv
{
   Display *display;
   GC gc;
   Window window;
   Colormap colormap;
   XShmSegmentInfo shminfo;

   Atom quit_atom;
   bool focus;

   XvPortID port;
   int depth;
   int visualid;

   XvImage *image;
   uint32_t fourcc;

   unsigned width;
   unsigned height;
   bool keep_aspect;

   uint8_t *ytable;
   uint8_t *utable;
   uint8_t *vtable;

#ifdef HAVE_FREETYPE
   font_renderer_t *font;

   unsigned luma_index[2];
   unsigned chroma_u_index;
   unsigned chroma_v_index;

   uint8_t font_y;
   uint8_t font_u;
   uint8_t font_v;
#endif

   void (*render_func)(struct xv*, const void *frame, unsigned width, unsigned height, unsigned pitch);
} xv_t;

static void xv_set_nonblock_state(void *data, bool state)
{
   xv_t *xv = (xv_t*)data;
   Atom atom = XInternAtom(xv->display, "XV_SYNC_TO_VBLANK", true);
   if (atom != None && xv->port)
      XvSetPortAttribute(xv->display, xv->port, atom, !state);
   else
      RARCH_WARN("Failed to set SYNC_TO_VBLANK attribute.\n");
}

static volatile sig_atomic_t g_quit = 0;
static void sighandler(int sig)
{
   g_quit = 1;
}

static inline void calculate_yuv(uint8_t *y, uint8_t *u, uint8_t *v, unsigned r, unsigned g, unsigned b)
{
   int y_ = (int)(+((double)r * 0.257) + ((double)g * 0.504) + ((double)b * 0.098) +  16.0);
   int u_ = (int)(-((double)r * 0.148) - ((double)g * 0.291) + ((double)b * 0.439) + 128.0);
   int v_ = (int)(+((double)r * 0.439) - ((double)g * 0.368) - ((double)b * 0.071) + 128.0);

   *y = y_ < 0 ? 0 : (y_ > 255 ? 255 : y_);
   *u = y_ < 0 ? 0 : (u_ > 255 ? 255 : u_);
   *v = v_ < 0 ? 0 : (v_ > 255 ? 255 : v_);
}

static void init_yuv_tables(xv_t *xv)
{
   xv->ytable = (uint8_t*)malloc(0x8000);
   xv->utable = (uint8_t*)malloc(0x8000);
   xv->vtable = (uint8_t*)malloc(0x8000);

   for (unsigned i = 0; i < 0x8000; i++)
   {
      // Extract RGB555 color data from i
      unsigned r = (i >> 10) & 0x1F, g = (i >> 5) & 0x1F, b = (i) & 0x1F;
      r = (r << 3) | (r >> 2);  // R5->R8
      g = (g << 3) | (g >> 2);  // G5->G8
      b = (b << 3) | (b >> 2);  // B5->B8

      calculate_yuv(&xv->ytable[i], &xv->utable[i], &xv->vtable[i], r, g, b);
   }
}

// Source: MPlayer
static void hide_mouse(xv_t *xv)
{
   Cursor no_ptr;
   Pixmap bm_no;
   XColor black, dummy;
   Colormap colormap;
   static char bm_no_data[] = {0, 0, 0, 0, 0, 0, 0, 0};
   colormap = DefaultColormap(xv->display, DefaultScreen(xv->display));
   if (!XAllocNamedColor(xv->display, colormap, "black", &black, &dummy))
      return;

   bm_no = XCreateBitmapFromData(xv->display, xv->window, bm_no_data, 8, 8);
   no_ptr = XCreatePixmapCursor(xv->display, bm_no, bm_no, &black, &black, 0, 0);
   XDefineCursor(xv->display, xv->window, no_ptr);
   XFreeCursor(xv->display, no_ptr);
   if (bm_no != None)
      XFreePixmap(xv->display, bm_no);
   XFreeColors(xv->display, colormap, &black.pixel, 1, 0);
}

static Atom XA_NET_WM_STATE;
static Atom XA_NET_WM_STATE_FULLSCREEN;
#define XA_INIT(x) XA##x = XInternAtom(xv->display, #x, False)
#define _NET_WM_STATE_ADD 1

// Source: MPlayer
static void set_fullscreen(xv_t *xv)
{
   XA_INIT(_NET_WM_STATE);
   XA_INIT(_NET_WM_STATE_FULLSCREEN);

   if (!XA_NET_WM_STATE || !XA_NET_WM_STATE_FULLSCREEN)
   {
      RARCH_WARN("X11: Cannot set fullscreen.\n");
      return;
   }

   XEvent xev;

   xev.xclient.type = ClientMessage;
   xev.xclient.serial = 0;
   xev.xclient.send_event = True;
   xev.xclient.message_type = XA_NET_WM_STATE;
   xev.xclient.window = xv->window;
   xev.xclient.format = 32;
   xev.xclient.data.l[0] = _NET_WM_STATE_ADD;
   xev.xclient.data.l[1] = XA_NET_WM_STATE_FULLSCREEN;
   xev.xclient.data.l[2] = 0;
   xev.xclient.data.l[3] = 0;
   xev.xclient.data.l[4] = 0;

   XSendEvent(xv->display, DefaultRootWindow(xv->display), False,
            SubstructureRedirectMask | SubstructureNotifyMask,
            &xev);
}

static void xv_init_font(xv_t *xv, const char *font_path, unsigned font_size)
{
#ifdef HAVE_FREETYPE
   if (!g_settings.video.font_enable)
      return;

   const char *path = font_path;
   if (!*path)
      path = font_renderer_get_default_font();

   if (path)
   {
      xv->font = font_renderer_new(path, font_size);
      if (xv->font)
      {
         int r = g_settings.video.msg_color_r * 255;
         r = (r < 0 ? 0 : (r > 255 ? 255 : r));
         int g = g_settings.video.msg_color_g * 255;
         g = (g < 0 ? 0 : (g > 255 ? 255 : g));
         int b = g_settings.video.msg_color_b * 255;
         b = (b < 0 ? 0 : (b > 255 ? 255 : b));

         calculate_yuv(&xv->font_y, &xv->font_u, &xv->font_v,
               r, g, b);
      }
      else
         RARCH_WARN("Failed to init font.\n");
   }
   else
      RARCH_LOG("Did not find default font.\n");
#endif
}

// We render @ 2x scale to combat chroma downsampling. Also makes fonts more bearable :)
static void render16_yuy2(xv_t *xv, const void *input_, unsigned width, unsigned height, unsigned pitch)
{
   const uint16_t *input = (const uint16_t*)input_;
   uint8_t *output = (uint8_t*)xv->image->data;

   for (unsigned y = 0; y < height; y++)
   {
      for (unsigned x = 0; x < width; x++)
      {
         uint16_t p = *input++;

         uint8_t y0 = xv->ytable[p];
         uint8_t u = xv->utable[p];
         uint8_t v = xv->vtable[p];

         unsigned img_width = xv->width << 1;
         output[0] = output[img_width] = y0;
         output[1] = output[img_width + 1] = u;
         output[2] = output[img_width + 2] = y0;
         output[3] = output[img_width + 3] = v;
         output += 4;
      }

      input  += (pitch >> 1) - width;
      output += (xv->width - width) << 2;
   }
}

static void render16_uyvy(xv_t *xv, const void *input_, unsigned width, unsigned height, unsigned pitch)
{
   const uint16_t *input = (const uint16_t*)input_;
   uint8_t *output = (uint8_t*)xv->image->data;

   for (unsigned y = 0; y < height; y++)
   {
      for (unsigned x = 0; x < width; x++)
      {
         uint16_t p = *input++;

         uint8_t y0 = xv->ytable[p];
         uint8_t u = xv->utable[p];
         uint8_t v = xv->vtable[p];

         unsigned img_width = xv->width << 1;
         output[0] = output[img_width] = u;
         output[1] = output[img_width + 1] = y0;
         output[2] = output[img_width + 2] = v;
         output[3] = output[img_width + 3] = y0;
         output += 4;
      }

      input  += (pitch >> 1) - width;
      output += (xv->width - width) << 2;
   }
}

static void render32_yuy2(xv_t *xv, const void *input_, unsigned width, unsigned height, unsigned pitch)
{
   const uint32_t *input = (const uint32_t*)input_;
   uint8_t *output = (uint8_t*)xv->image->data;

   for (unsigned y = 0; y < height; y++)
   {
      for (unsigned x = 0; x < width; x++)
      {
         uint32_t p = *input++;
         p = ((p >> 9) & 0x7c00) | ((p >> 6) & 0x03e0) | ((p >> 3) & 0x1f); // ARGB -> RGB15

         uint8_t y0 = xv->ytable[p];
         uint8_t u = xv->utable[p];
         uint8_t v = xv->vtable[p];

         unsigned img_width = xv->width << 1;
         output[0] = output[img_width] = y0;
         output[1] = output[img_width + 1] = u;
         output[2] = output[img_width + 2] = y0;
         output[3] = output[img_width + 3] = v;
         output += 4;
      }

      input  += (pitch >> 2) - width;
      output += (xv->width - width) << 2;
   }
}

static void render32_uyvy(xv_t *xv, const void *input_, unsigned width, unsigned height, unsigned pitch)
{
   const uint32_t *input = (const uint32_t*)input_;
   uint16_t *output = (uint16_t*)xv->image->data;

   for (unsigned y = 0; y < height; y++)
   {
      for (unsigned x = 0; x < width; x++)
      {
         uint32_t p = *input++;
         p = ((p >> 9) & 0x7c00) | ((p >> 6) & 0x03e0) | ((p >> 3) & 0x1f); // ARGB -> RGB15

         uint8_t y0 = xv->ytable[p];
         uint8_t u = xv->utable[p];
         uint8_t v = xv->vtable[p];

         unsigned img_width = xv->width << 1;
         output[0] = output[img_width] = u;
         output[1] = output[img_width + 1] = y0;
         output[2] = output[img_width + 2] = v;
         output[3] = output[img_width + 3] = y0;
         output += 4;
      }

      input  += (pitch >> 2) - width;
      output += (xv->width - width) << 2;
   }
}

struct format_desc
{
   void (*render_16)(xv_t *xv, const void *input,
         unsigned width, unsigned height, unsigned pitch);
   void (*render_32)(xv_t *xv, const void *input,
         unsigned width, unsigned height, unsigned pitch);
   char components[4];
   unsigned luma_index[2];
   unsigned u_index;
   unsigned v_index;
};

static const struct format_desc formats[] = {
   {
      render16_yuy2,
      render32_yuy2,
      { 'Y', 'U', 'Y', 'V' },
      { 0, 2 },
      1,
      3,
   },
   {
      render16_uyvy,
      render32_uyvy,
      { 'U', 'Y', 'V', 'Y' },
      { 1, 3 },
      0,
      2,
   },
};

static bool adaptor_set_format(xv_t *xv, Display *dpy, XvPortID port, const video_info_t *video)
{
   int format_count;
   XvImageFormatValues *format = XvListImageFormats(xv->display, port, &format_count);
   if (!format)
      return false;

   for (int i = 0; i < format_count; i++)
   {
      for (unsigned j = 0; j < sizeof(formats) / sizeof(formats[0]); j++)
      {
         if (format[i].type == XvYUV && format[i].bits_per_pixel == 16 && format[i].format == XvPacked)
         {
            if (format[i].component_order[0] == formats[j].components[0] &&
                  format[i].component_order[1] == formats[j].components[1] &&
                  format[i].component_order[2] == formats[j].components[2] &&
                  format[i].component_order[3] == formats[j].components[3])
            {
               xv->fourcc = format[i].id;
               xv->render_func = video->rgb32 ? formats[j].render_32 : formats[j].render_16;

#ifdef HAVE_FREETYPE
               xv->luma_index[0] = formats[j].luma_index[0];
               xv->luma_index[1] = formats[j].luma_index[1];
               xv->chroma_u_index = formats[j].u_index;
               xv->chroma_v_index = formats[j].v_index;
#endif
               XFree(format);
               return true;
            }
         }
      }
   }

   XFree(format);
   return false;
}

static void *xv_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
   xv_t *xv = (xv_t*)calloc(1, sizeof(*xv));
   if (!xv)
      return NULL;

   xv->display = XOpenDisplay(NULL);
   struct sigaction sa;
   unsigned adaptor_count = 0;
   int visualmatches = 0;
   XSetWindowAttributes attributes = {0};
   unsigned width = 0, height = 0;
   char buf[128];
   Atom atom = 0;
   void *xinput = NULL;
   XVisualInfo *visualinfo = NULL;
   XVisualInfo visualtemplate = {0};
   const struct retro_game_geometry *geom = &g_extern.system.av_info.geometry;

   if (!XShmQueryExtension(xv->display))
   {
      RARCH_ERR("XVideo: XShm extension not found.\n");
      goto error;
   }

   xv->keep_aspect = video->force_aspect;

   // Find an appropriate Xv port.
   xv->port = 0;
   XvAdaptorInfo *adaptor_info;
   XvQueryAdaptors(xv->display, DefaultRootWindow(xv->display), &adaptor_count, &adaptor_info);
   for (unsigned i = 0; i < adaptor_count; i++)
   {
      // Find adaptor that supports both input (memory->drawable) and image (drawable->screen) masks.
      if (adaptor_info[i].num_formats < 1) continue;
      if (!(adaptor_info[i].type & XvInputMask)) continue;
      if (!(adaptor_info[i].type & XvImageMask)) continue;
      if (!adaptor_set_format(xv, xv->display, adaptor_info[i].base_id, video)) continue;

      xv->port     = adaptor_info[i].base_id;
      xv->depth    = adaptor_info[i].formats->depth;
      xv->visualid = adaptor_info[i].formats->visual_id;

      RARCH_LOG("XVideo: Found suitable XvPort #%u\n", (unsigned)xv->port);
      break;
   }
   XvFreeAdaptorInfo(adaptor_info);

   if (xv->port == 0)
   {
      RARCH_ERR("XVideo: Failed to find valid XvPort or format.\n");
      goto error;
   }

   visualtemplate.visualid = xv->visualid;
   visualtemplate.screen   = DefaultScreen(xv->display);
   visualtemplate.depth    = xv->depth;
   visualtemplate.visual   = 0;
   visualinfo = XGetVisualInfo(xv->display, VisualIDMask | VisualScreenMask | VisualDepthMask, &visualtemplate, &visualmatches);
   if (visualmatches < 1 || !visualinfo->visual)
   {
      if (visualinfo) XFree(visualinfo);
      RARCH_ERR("XVideo: Unable to find Xv-compatible visual.\n");
      goto error;
   }

   xv->colormap = XCreateColormap(xv->display, DefaultRootWindow(xv->display), visualinfo->visual, AllocNone);
   attributes.colormap = xv->colormap;
   attributes.border_pixel = 0;
   attributes.event_mask = StructureNotifyMask | DestroyNotify | ClientMessage;

   width = video->fullscreen ? ((video->width == 0) ? geom->base_width : video->width) : video->width;
   height = video->fullscreen ? ((video->height == 0) ? geom->base_height : video->height) : video->height;
   xv->window = XCreateWindow(xv->display, DefaultRootWindow(xv->display),
         0, 0, width, height,
         0, xv->depth, InputOutput, visualinfo->visual,
         CWColormap | CWBorderPixel | CWEventMask, &attributes);

   XFree(visualinfo);
   XSetWindowBackground(xv->display, xv->window, 0);

   XMapWindow(xv->display, xv->window);

   if (gfx_window_title(buf, sizeof(buf)))
      XStoreName(xv->display, xv->window, buf);

   if (video->fullscreen)
      set_fullscreen(xv);
   hide_mouse(xv);

   xv->gc = XCreateGC(xv->display, xv->window, 0, 0);

   // Set colorkey to auto paint, so that Xv video output is always visible
   atom = XInternAtom(xv->display, "XV_AUTOPAINT_COLORKEY", true);
   if (atom != None) XvSetPortAttribute(xv->display, xv->port, atom, 1);

   xv->width = geom->max_width;
   xv->height = geom->max_height;

   xv->image = XvShmCreateImage(xv->display, xv->port, xv->fourcc, NULL, xv->width, xv->height, &xv->shminfo);
   if (!xv->image)
   {
      RARCH_ERR("XVideo: XShmCreateImage failed.\n");
      goto error;
   }
   xv->width = xv->image->width;
   xv->height = xv->image->height;

   xv->shminfo.shmid = shmget(IPC_PRIVATE, xv->image->data_size, IPC_CREAT | 0777);
   xv->shminfo.shmaddr = xv->image->data = (char*)shmat(xv->shminfo.shmid, NULL, 0);
   xv->shminfo.readOnly = false;
   if (!XShmAttach(xv->display, &xv->shminfo))
   {
      RARCH_ERR("XVideo: XShmAttach failed.\n");
      goto error;
   }
   XSync(xv->display, False);
   memset(xv->image->data, 128, xv->image->data_size);

   xv->quit_atom = XInternAtom(xv->display, "WM_DELETE_WINDOW", False);
   if (xv->quit_atom)
      XSetWMProtocols(xv->display, xv->window, &xv->quit_atom, 1);

   sa.sa_handler = sighandler;
   sa.sa_flags = SA_RESTART;
   sigemptyset(&sa.sa_mask);
   sigaction(SIGINT, &sa, NULL);
   sigaction(SIGTERM, &sa, NULL);

   xv_set_nonblock_state(xv, !video->vsync);
   xv->focus = true;

   suspend_screensaver(xv->window);

   xinput = input_x.init();
   if (xinput)
   {
      *input = &input_x;
      *input_data = xinput;
   }
   else
      *input = NULL;

   init_yuv_tables(xv);
   xv_init_font(xv, g_settings.video.font_path, g_settings.video.font_size);

   return xv;

error:
   free(xv);
   return NULL;
}

static bool check_resize(xv_t *xv, unsigned width, unsigned height)
{
   // We render @ 2x scale to combat chroma downsampling.
   if (xv->width != (width << 1) || xv->height != (height << 1))
   {
      xv->width = width << 1;
      xv->height = height << 1;

      XShmDetach(xv->display, &xv->shminfo);
      shmdt(xv->shminfo.shmaddr);
      shmctl(xv->shminfo.shmid, IPC_RMID, NULL);
      XFree(xv->image);

      memset(&xv->shminfo, 0, sizeof(xv->shminfo));
      xv->image = XvShmCreateImage(xv->display, xv->port, xv->fourcc, NULL, xv->width, xv->height, &xv->shminfo);
      if (xv->image == None)
      {
         RARCH_ERR("Failed to create image.\n");
         return false;
      }

      xv->width = xv->image->width;
      xv->height = xv->image->height;

      xv->shminfo.shmid = shmget(IPC_PRIVATE, xv->image->data_size, IPC_CREAT | 0777);
      if (xv->shminfo.shmid < 0)
      {
         RARCH_ERR("Failed to init SHM.\n");
         return false;
      }

      xv->shminfo.shmaddr = xv->image->data = (char*)shmat(xv->shminfo.shmid, NULL, 0);
      xv->shminfo.readOnly = false;

      if (!XShmAttach(xv->display, &xv->shminfo))
      {
         RARCH_ERR("Failed to reattch XvShm image.\n");
         return false;
      }
      XSync(xv->display, False);
      memset(xv->image->data, 128, xv->image->data_size);
   }
   return true;
}

static void calc_out_rect(bool keep_aspect, unsigned *x, unsigned *y, unsigned *width, unsigned *height, unsigned vp_width, unsigned vp_height)
{
   if (!keep_aspect)
   {
      *x = 0; *y = 0; *width = vp_width; *height = vp_height;
   }
   else
   {
      float desired_aspect = g_settings.video.aspect_ratio;
      float device_aspect = (float)vp_width / vp_height;

      // If the aspect ratios of screen and desired aspect ratio are sufficiently equal (floating point stuff),
      // assume they are actually equal.
      if (fabs(device_aspect - desired_aspect) < 0.0001)
      {
         *x = 0; *y = 0; *width = vp_width; *height = vp_height;
      }
      else if (device_aspect > desired_aspect)
      {
         float delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
         *x = vp_width * (0.5 - delta); *y = 0; *width = 2.0 * vp_width * delta; *height = vp_height;
      }
      else
      {
         float delta = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
         *x = 0; *y = vp_height * (0.5 - delta); *width = vp_width; *height = 2.0 * vp_height * delta;
      }
   }
}

// TODO: Is there some way to render directly like GL? :(
// Hacky C code is hacky :D Yay.
static void xv_render_msg(xv_t *xv, const char *msg, unsigned width, unsigned height)
{
#ifdef HAVE_FREETYPE
   if (!xv->font)
      return;

   struct font_output_list out;
   font_renderer_msg(xv->font, msg, &out);
   struct font_output *head = out.head;

   int _base_x = g_settings.video.msg_pos_x * width;
   int _base_y = height - g_settings.video.msg_pos_y * height;

   unsigned luma_index[2] = { xv->luma_index[0], xv->luma_index[1] };
   unsigned chroma_u_index = xv->chroma_u_index;
   unsigned chroma_v_index = xv->chroma_v_index;

   unsigned pitch = width << 1; // YUV formats used are 16 bpp.

   while (head)
   {
      int base_x = (_base_x + head->off_x) << 1;
      base_x &= ~3; // Make sure we always start on the correct boundary so the indices are correct.

      int base_y = _base_y - head->off_y;
      if (base_y >= 0)
      {
         for (int y = 0; y < (int)head->height && (base_y + y) < (int)height; y++)
         {
            if (base_x < 0)
               continue;

            const uint8_t *a = head->output + head->pitch * y;
            uint8_t *out = (uint8_t*)xv->image->data + (base_y - head->height + y) * pitch + base_x;

            for (int x = 0; x < (int)(head->width << 1) && (base_x + x) < (int)pitch; x += 4)
            {
               unsigned alpha[2];
               alpha[0] = a[(x >> 1) + 0];

               if (((x >> 1) + 1) == (int)head->width) // We reached the end, uhoh. Branching like a BOSS. :D
                  alpha[1] = 0;
               else
                  alpha[1] = a[(x >> 1) + 1];

               unsigned alpha_sub = (alpha[0] + alpha[1]) >> 1; // Blended alpha for the sub-samples U/V channels.

               for (unsigned i = 0; i < 2; i++)
               {
                  unsigned blended = (xv->font_y * alpha[i] + ((256 - alpha[i]) * out[x + luma_index[i]])) >> 8;
                  out[x + luma_index[i]] = blended;
               }

               // Blend chroma channels
               unsigned blended = (xv->font_u * alpha_sub + ((256 - alpha_sub) * out[x + chroma_u_index])) >> 8;
               out[x + chroma_u_index] = blended;

               blended = (xv->font_v * alpha_sub + ((256 - alpha_sub) * out[x + chroma_v_index])) >> 8;
               out[x + chroma_v_index] = blended;
            }
         }
      }

      head = head->next;
   }

   font_renderer_free_output(&out);
#else
   (void)xv;
   (void)msg;
   (void)width;
   (void)height;
#endif
}

static bool xv_frame(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   if (!frame)
      return true;

   xv_t *xv = (xv_t*)data;

   if (!check_resize(xv, width, height))
      return false;

   XWindowAttributes target;
   XGetWindowAttributes(xv->display, xv->window, &target);
   xv->render_func(xv, frame, width, height, pitch);

   unsigned x, y, owidth, oheight;
   calc_out_rect(xv->keep_aspect, &x, &y, &owidth, &oheight, target.width, target.height);

   if (msg)
      xv_render_msg(xv, msg, width << 1, height << 1);

   XvShmPutImage(xv->display, xv->port, xv->window, xv->gc, xv->image,
         0, 0, width << 1, height << 1,
         x, y, owidth, oheight,
         true);
   XSync(xv->display, False);

   char buf[128];
   if (gfx_window_title(buf, sizeof(buf)))
      XStoreName(xv->display, xv->window, buf);

   return true;
}

static bool xv_alive(void *data)
{
   xv_t *xv = (xv_t*)data;

   XEvent event;
   while (XPending(xv->display))
   {
      XNextEvent(xv->display, &event);
      switch (event.type)
      {
         case ClientMessage:
            if ((Atom)event.xclient.data.l[0] == xv->quit_atom)
               return false;
            break;
         case DestroyNotify:
            return false;
         case MapNotify: // Find something that works better.
            xv->focus = true;
            break;
         case UnmapNotify:
            xv->focus = false;
            break;
         default:
            break;
      }
   }

   return !g_quit;
}

static bool xv_focus(void *data)
{
   xv_t *xv = (xv_t*)data;
   return xv->focus;
}


static void xv_free(void *data)
{
   xv_t *xv = (xv_t*)data;
   XShmDetach(xv->display, &xv->shminfo);
   shmdt(xv->shminfo.shmaddr);
   shmctl(xv->shminfo.shmid, IPC_RMID, NULL);
   XFree(xv->image);

   if (xv->window)
      XUnmapWindow(xv->display, xv->window);
   if (xv->colormap)
      XFreeColormap(xv->display, xv->colormap);

   XCloseDisplay(xv->display);

   free(xv->ytable);
   free(xv->utable);
   free(xv->vtable);

#ifdef HAVE_FREETYPE
   if (xv->font)
      font_renderer_free(xv->font);
#endif

   free(xv);
}

const video_driver_t video_xvideo = {
   xv_init,
   xv_frame,
   xv_set_nonblock_state,
   xv_alive,
   xv_focus,
   NULL,
   xv_free,
   "xvideo"
};


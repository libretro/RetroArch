/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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
#include "gfx_common.h"
#include "fonts/fonts.h"

#include "context/x11_common.h"

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
   XIM xim;
   XIC xic;

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
   struct rarch_viewport vp;

   uint8_t *ytable;
   uint8_t *utable;
   uint8_t *vtable;

   void *font;
   const font_renderer_driver_t *font_driver;

   unsigned luma_index[2];
   unsigned chroma_u_index;
   unsigned chroma_v_index;

   uint8_t font_y;
   uint8_t font_u;
   uint8_t font_v;

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
   unsigned i;
   xv->ytable = (uint8_t*)malloc(0x10000);
   xv->utable = (uint8_t*)malloc(0x10000);
   xv->vtable = (uint8_t*)malloc(0x10000);

   for (i = 0; i < 0x10000; i++)
   {
      // Extract RGB565 color data from i
      unsigned r = (i >> 11) & 0x1f, g = (i >> 5) & 0x3f, b = (i >> 0) & 0x1f;
      r = (r << 3) | (r >> 2);  // R5->R8
      g = (g << 2) | (g >> 4);  // G6->G8
      b = (b << 3) | (b >> 2);  // B5->B8

      calculate_yuv(&xv->ytable[i], &xv->utable[i], &xv->vtable[i], r, g, b);
   }
}

static void xv_init_font(xv_t *xv, const char *font_path, unsigned font_size)
{
   if (!g_settings.video.font_enable)
      return;

   if (font_renderer_create_default(&xv->font_driver, &xv->font))
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
      RARCH_LOG("Could not initialize fonts.\n");
}

// We render @ 2x scale to combat chroma downsampling. Also makes fonts more bearable :)
static void render16_yuy2(xv_t *xv, const void *input_, unsigned width, unsigned height, unsigned pitch)
{
   unsigned x, y;
   const uint16_t *input = (const uint16_t*)input_;
   uint8_t *output = (uint8_t*)xv->image->data;

   for (y = 0; y < height; y++)
   {
      for (x = 0; x < width; x++)
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
   unsigned x, y;
   const uint16_t *input = (const uint16_t*)input_;
   uint8_t *output = (uint8_t*)xv->image->data;

   for (y = 0; y < height; y++)
   {
      for (x = 0; x < width; x++)
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
   unsigned x, y;
   const uint32_t *input = (const uint32_t*)input_;
   uint8_t *output = (uint8_t*)xv->image->data;

   for (y = 0; y < height; y++)
   {
      for (x = 0; x < width; x++)
      {
         uint32_t p = *input++;
         p = ((p >> 8) & 0xf800) | ((p >> 5) & 0x07e0) | ((p >> 3) & 0x1f); // ARGB -> RGB16

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
   unsigned x, y;
   const uint32_t *input = (const uint32_t*)input_;
   uint16_t *output = (uint16_t*)xv->image->data;

   for (y = 0; y < height; y++)
   {
      for (x = 0; x < width; x++)
      {
         uint32_t p = *input++;
         p = ((p >> 8) & 0xf800) | ((p >> 5) & 0x07e0) | ((p >> 3) & 0x1f); // ARGB -> RGB16

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
   int i;
   unsigned j;
   int format_count;
   XvImageFormatValues *format = XvListImageFormats(xv->display, port, &format_count);
   if (!format)
      return false;

   for (i = 0; i < format_count; i++)
   {
      for (j = 0; j < ARRAY_SIZE(formats); j++)
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

               xv->luma_index[0] = formats[j].luma_index[0];
               xv->luma_index[1] = formats[j].luma_index[1];
               xv->chroma_u_index = formats[j].u_index;
               xv->chroma_v_index = formats[j].v_index;
               XFree(format);
               return true;
            }
         }
      }
   }

   XFree(format);
   return false;
}

static void calc_out_rect(bool keep_aspect, struct rarch_viewport *vp, unsigned vp_width, unsigned vp_height)
{
   vp->full_width  = vp_width;
   vp->full_height = vp_height;

   if (g_settings.video.scale_integer)
   {
      gfx_scale_integer(vp, vp_width, vp_height, g_extern.system.aspect_ratio, keep_aspect);
   }
   else if (!keep_aspect)
   {
      vp->x = 0; vp->y = 0;
      vp->width = vp_width;
      vp->height = vp_height;
   }
   else
   {
      float desired_aspect = g_extern.system.aspect_ratio;
      float device_aspect = (float)vp_width / vp_height;

      // If the aspect ratios of screen and desired aspect ratio are sufficiently equal (floating point stuff),
      // assume they are actually equal.
      if (fabs(device_aspect - desired_aspect) < 0.0001)
      {
         vp->x = 0; vp->y = 0;
         vp->width = vp_width;
         vp->height = vp_height;
      }
      else if (device_aspect > desired_aspect)
      {
         float delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
         vp->x = vp_width * (0.5 - delta);
         vp->y = 0;
         vp->width = 2.0 * vp_width * delta;
         vp->height = vp_height;
      }
      else
      {
         float delta = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
         vp->x = 0;
         vp->y = vp_height * (0.5 - delta);
         vp->width = vp_width;
         vp->height = 2.0 * vp_height * delta;
      }
   }
}

static void *xv_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
   xv_t *xv = (xv_t*)calloc(1, sizeof(*xv));
   if (!xv)
      return NULL;

   XInitThreads();

   unsigned i;
   xv->display = XOpenDisplay(NULL);
   struct sigaction sa;
   unsigned adaptor_count = 0;
   int visualmatches = 0;
   XSetWindowAttributes attributes = {0};
   unsigned width = 0, height = 0;
   char buf[128], buf_fps[128];
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
   for (i = 0; i < adaptor_count; i++)
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
   attributes.event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask | DestroyNotify | ClientMessage;

   width = video->fullscreen ? ((video->width == 0) ? geom->base_width : video->width) : video->width;
   height = video->fullscreen ? ((video->height == 0) ? geom->base_height : video->height) : video->height;
   xv->window = XCreateWindow(xv->display, DefaultRootWindow(xv->display),
         0, 0, width, height,
         0, xv->depth, InputOutput, visualinfo->visual,
         CWColormap | CWBorderPixel | CWEventMask, &attributes);

   XFree(visualinfo);
   XSetWindowBackground(xv->display, xv->window, 0);

   XMapWindow(xv->display, xv->window);

   if (gfx_get_fps(buf, sizeof(buf), NULL, 0))
      XStoreName(xv->display, xv->window, buf);

   x11_set_window_attr(xv->display, xv->window);

   if (video->fullscreen)
   {
      x11_windowed_fullscreen(xv->display, xv->window);
      x11_show_mouse(xv->display, xv->window, false);
   }

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

   driver.display_type  = RARCH_DISPLAY_X11;
   driver.video_display = (uintptr_t)xv->display;
   driver.video_window  = (Window)xv->window;

   if (input && input_data)
   {
      xinput = input_x.init();
      if (xinput)
      {
         *input = &input_x;
         *input_data = xinput;
      }
      else
         *input = NULL;
   }

   init_yuv_tables(xv);
   xv_init_font(xv, g_settings.video.font_path, g_settings.video.font_size);

   if (!x11_create_input_context(xv->display, xv->window, &xv->xim, &xv->xic))
      goto error;

   XWindowAttributes target;
   XGetWindowAttributes(xv->display, xv->window, &target);
   calc_out_rect(xv->keep_aspect, &xv->vp, target.width, target.height);
   xv->vp.full_width = target.width;
   xv->vp.full_height = target.height;

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

// TODO: Is there some way to render directly like GL? :(
// Hacky C code is hacky :D Yay.
static void xv_render_msg(xv_t *xv, const char *msg, unsigned width, unsigned height)
{
   if (!xv->font)
      return;

   int x, y;
   unsigned i;
   struct font_output_list out;
   xv->font_driver->render_msg(xv->font, msg, &out);
   struct font_output *head = out.head;

   int msg_base_x = g_settings.video.msg_pos_x * width;
   int msg_base_y = height * (1.0 - g_settings.video.msg_pos_y);

   unsigned luma_index[2] = { xv->luma_index[0], xv->luma_index[1] };
   unsigned chroma_u_index = xv->chroma_u_index;
   unsigned chroma_v_index = xv->chroma_v_index;

   unsigned pitch = width << 1; // YUV formats used are 16 bpp.

   for (; head; head = head->next)
   {
      int base_x = (msg_base_x + head->off_x) & ~1; // Make sure we always start on the correct boundary so the indices are correct.
      int base_y = msg_base_y - head->off_y - head->height;

      int glyph_width  = head->width;
      int glyph_height = head->height;

      const uint8_t *src = head->output;

      if (base_x < 0)
      {
         src -= base_x;
         glyph_width += base_x;
         base_x = 0;
      }

      if (base_y < 0)
      {
         src -= base_y * (int)head->pitch;
         glyph_height += base_y;
         base_y = 0;
      }

      int max_width  = width - base_x;
      int max_height = height - base_y;

      if (max_width <= 0 || max_height <= 0)
         continue;

      if (glyph_width > max_width)
         glyph_width = max_width;
      if (glyph_height > max_height)
         glyph_height = max_height;

      uint8_t *out = (uint8_t*)xv->image->data + base_y * pitch + (base_x << 1);

      for (y = 0; y < glyph_height; y++, src += head->pitch, out += pitch)
      {
         // 2 input pixels => 4 bytes (2Y, 1U, 1V).
         for (x = 0; x < glyph_width; x += 2)
         {
            int out_x = x << 1;

            unsigned alpha[2];
            alpha[0] = src[x + 0];

            if (x + 1 < glyph_width)
               alpha[1] = src[x + 1];
            else
               alpha[1] = 0;

            unsigned alpha_sub = (alpha[0] + alpha[1]) >> 1; // Blended alpha for the sub-sampled U/V channels.

            for (i = 0; i < 2; i++)
            {
               unsigned blended = (xv->font_y * alpha[i] + ((256 - alpha[i]) * out[out_x + luma_index[i]])) >> 8;
               out[out_x + luma_index[i]] = blended;
            }

            // Blend chroma channels
            unsigned blended = (xv->font_u * alpha_sub + ((256 - alpha_sub) * out[out_x + chroma_u_index])) >> 8;
            out[out_x + chroma_u_index] = blended;

            blended = (xv->font_v * alpha_sub + ((256 - alpha_sub) * out[out_x + chroma_v_index])) >> 8;
            out[out_x + chroma_v_index] = blended;
         }
      }
   }

   xv->font_driver->free_output(xv->font, &out);
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

   calc_out_rect(xv->keep_aspect, &xv->vp, target.width, target.height);
   xv->vp.full_width = target.width;
   xv->vp.full_height = target.height;

   if (msg)
      xv_render_msg(xv, msg, width << 1, height << 1);

   XvShmPutImage(xv->display, xv->port, xv->window, xv->gc, xv->image,
         0, 0, width << 1, height << 1,
         xv->vp.x, xv->vp.y, xv->vp.width, xv->vp.height,
         true);
   XSync(xv->display, False);

   char buf[128];
   if (gfx_get_fps(buf, sizeof(buf), NULL, 0))
      XStoreName(xv->display, xv->window, buf);

   g_extern.frame_count++;
   return true;
}

static bool xv_alive(void *data)
{
   xv_t *xv = (xv_t*)data;

   XEvent event;
   while (XPending(xv->display))
   {
      XNextEvent(xv->display, &event);
      bool filter = XFilterEvent(&event, xv->window);

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

         case KeyPress:
         case KeyRelease:
            x11_handle_key_event(&event, xv->xic, filter);
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
   x11_destroy_input_context(&xv->xim, &xv->xic);
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

   if (xv->font)
      xv->font_driver->free(xv->font);

   free(xv);
}

static void xv_viewport_info(void *data, struct rarch_viewport *vp)
{
   xv_t *xv = (xv_t*)data;
   *vp = xv->vp;
}

const video_driver_t video_xvideo = {
   xv_init,
   xv_frame,
   xv_set_nonblock_state,
   xv_alive,
   xv_focus,
   NULL,
   xv_free,
   "xvideo",
#ifdef HAVE_MENU
   NULL,
#endif

   NULL,
   xv_viewport_info,
};


/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <math.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>

#include <retro_atomic.h>
#include <retro_inline.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include <encodings/utf.h>

#include "../font_driver.h"
#include "../video_views.h"
#include "../video_views_cpu.h"

#include "../../configuration.h"
#include "../../frontend/frontend_driver.h"
#include "../../input/input_driver.h"
#include "../../verbosity.h"

#include "../common/x11_common.h"

/* Adapted from bSNES and MPlayer source. */

/* Images drawn in turn: the adaptor may still be reading the last one
 * put while the next frame is drawn */
#define XV_IMAGES 2

struct xv_image
{
   XShmSegmentInfo shm;
   XvImage *image;
   /* The placements a core's views were last drawn at in this image, to
    * clear it only when they move: (unsigned)-1 when it is to be
    * cleared */
   video_views_placement_t views_last[VIDEO_VIEWS_MAX_PLACEMENTS];
   unsigned views_last_count;
   /* Put, and the server not yet done reading it */
   bool pending;
};

typedef struct xv
{
   struct xv_image images[XV_IMAGES];
   /* The image drawn this frame */
   struct xv_image *cur;
   XvImage *image;
   /* The images' own connection, whose completion events nothing else
    * reads, and its GC for the window */
   Display *dpy;
   GC gc;
   int completion;
   unsigned shown;
   /* The server took no images: nothing is shown, nothing tried again */
   bool images_failed;

   XvPortID port;

   struct video_viewport vp;

   /* One entry per RGB565 colour. For packed (YUY2/UYVY) formats each
    * entry already holds the complete 4-byte macropixel in destination
    * byte order, so the render loops need a single load and two 32-bit
    * stores instead of three loads and eight byte stores. For planar
    * (YV12) formats the entry holds { y, u, v, 0 }. Built byte-wise, so
    * no endianness fixups are required. */
   uint32_t *yuv_table;

   void *font;
   const font_renderer_driver_t *font_driver;

   void (*render_func16)(struct xv*, const void *frame,
         unsigned width, unsigned height, unsigned pitch);
   void (*render_func32)(struct xv*, const void *frame,
         unsigned width, unsigned height, unsigned pitch);

   void (*render_glyph)(struct xv*, int base_x, int base_y,
			const uint8_t *glyph, int atlas_width,
			int glyph_width, int glyph_height);
   int depth;
   int visualid;
   int fmt_format; /* XvPacked or XvPlanar */
   unsigned luma_index[2];
   unsigned chroma_u_index;
   unsigned chroma_v_index;
   unsigned width;
   unsigned height;
   uint32_t fourcc;
   uint8_t font_y;
   uint8_t font_u;
   uint8_t font_v;
   bool keep_aspect;
   /* The core's frames are XRGB8888, not RGB565 */
   bool rgb32;

   void *tex_frame;
   unsigned tex_width;
   unsigned tex_height;
   unsigned tex_pitch;
   unsigned tex_rgb32;
   float tex_alpha;

   /* Anaglyph's eyes side by side and the row they blend to, and the
    * menu scaled over the views */
   video_views_cpu_t views_cpu;
   uint32_t *views_row;
   size_t views_row_size;
   uint32_t *views_canvas;
   size_t views_canvas_size;
   uint32_t *menu_scaled;
   size_t menu_scaled_size;

   /* The window's size, which the views are laid out on */
   unsigned out_dims;
   /* The part of the image last put to the window */
   unsigned put_width;
   unsigned put_height;
   /* For get_flags: the last frame with a map was drawn whole */
   retro_atomic_int_t views_fallback;
   bool views_on;
   bool views_seen;
} xv_t;

static void xv_set_nonblock_state(void *data, bool state, bool c, unsigned d)
{
   xv_t *xv  = (xv_t*)data;
   Atom atom = XInternAtom(g_x11_dpy, "XV_SYNC_TO_VBLANK", true);

   if (atom != None && xv->port)
      XvSetPortAttribute(g_x11_dpy, xv->port, atom, !state);
   else
      RARCH_WARN("[XVideo] Failed to set SYNC_TO_VBLANK attribute.\n");
}

static INLINE void xv_calculate_yuv(uint8_t *y, uint8_t *u, uint8_t *v,
      unsigned r, unsigned g, unsigned b)
{
   int y_ = (int)(+((double)r * 0.257) + ((double)g * 0.504)
         + ((double)b * 0.098) +  16.0);
   int u_ = (int)(-((double)r * 0.148) - ((double)g * 0.291)
         + ((double)b * 0.439) + 128.0);
   int v_ = (int)(+((double)r * 0.439) - ((double)g * 0.368)
         - ((double)b * 0.071) + 128.0);

   *y     = y_ < 0 ? 0 : (y_ > 255 ? 255 : y_);
   *u     = u_ < 0 ? 0 : (u_ > 255 ? 255 : u_);
   *v     = v_ < 0 ? 0 : (v_ > 255 ? 255 : v_);
}

static void xv_init_yuv_tables(xv_t *xv)
{
   unsigned i;
   uint8_t *entry;

   if (!(xv->yuv_table = (uint32_t*)calloc(0x10000, sizeof(uint32_t))))
      return;

   entry = (uint8_t*)xv->yuv_table;

   for (i = 0; i < 0x10000; i++, entry += 4)
   {
      uint8_t  y, u, v;
      /* Extract RGB565 color data from i */
      unsigned r = (i >> 11) & 0x1f;
      unsigned g = (i >> 5)  & 0x3f;
      unsigned b = (i >> 0)  & 0x1f;
      r          = (r << 3) | (r >> 2);  /* R5->R8 */
      g          = (g << 2) | (g >> 4);  /* G6->G8 */
      b          = (b << 3) | (b >> 2);  /* B5->B8 */

      xv_calculate_yuv(&y, &u, &v, r, g, b);

      if (xv->fmt_format == XvPlanar)
      {
         entry[0] = y;
         entry[1] = u;
         entry[2] = v;
      }
      else
      {
         /* Pre-assemble the destination macropixel: both luma slots
          * carry the same sample, chroma goes where the adaptor wants
          * it. Covers YUY2 ({0,2}/1/3) and UYVY ({1,3}/0/2) alike. */
         entry[xv->luma_index[0]] = y;
         entry[xv->luma_index[1]] = y;
         entry[xv->chroma_u_index] = u;
         entry[xv->chroma_v_index] = v;
      }
   }
}

static void xv_init_font(xv_t *xv, const char *font_path, unsigned font_size)
{
   settings_t *settings   = config_get_ptr();
   bool video_font_enable = settings->bools.video_font_enable;
   const char *path_font  = settings->paths.path_font;
   float video_font_size  = settings->floats.video_font_size;
   float msg_color_r      = settings->floats.video_msg_color_r;
   float msg_color_g      = settings->floats.video_msg_color_g;
   float msg_color_b      = settings->floats.video_msg_color_b;

   if (!video_font_enable)
      return;

   if (font_renderer_create_default(
            &xv->font_driver,
            &xv->font, *path_font
            ? path_font : NULL,
            video_font_size, FONT_ATLAS_FORMAT_A8))
   {
      int r = msg_color_r * 255;
      int g = msg_color_g * 255;
      int b = msg_color_b * 255;
      r = (r < 0 ? 0 : (r > 255 ? 255 : r));
      g = (g < 0 ? 0 : (g > 255 ? 255 : g));
      b = (b < 0 ? 0 : (b > 255 ? 255 : b));

      xv_calculate_yuv(&xv->font_y, &xv->font_u, &xv->font_v,
            r, g, b);
      /* The atlas may grow when a message needs more glyphs than it holds;
       * the glyphs are blitted from it in memory, so there is no texture
       * to make again */
      {
         struct font_atlas *grow = xv->font_driver->get_atlas(xv->font);
         grow->max_dims = VIDEO_SCALE_PACK(2048, 2048);
      }
   }
   else
      RARCH_LOG("[XVideo] Could not initialize fonts.\n");
}

/* We render @ 2x scale to combat chroma downsampling.
 * Also makes fonts more bearable. */
static void render16_packed(xv_t *xv, const void *input_,
      unsigned width, unsigned height, unsigned pitch)
{
   unsigned x, y;
   const uint16_t *input    = (const uint16_t*)input_;
   uint8_t *output          = (uint8_t*)xv->image->data;
   const uint32_t *tbl      = xv->yuv_table;
   const unsigned img_width = xv->width << 1;

   for (y = 0; y < height; y++)
   {
      for (x = 0; x < width; x++)
      {
         uint32_t t = tbl[*input++];
         memcpy(output, &t, sizeof(t));
         memcpy(output + img_width, &t, sizeof(t));
         output += 4;
      }

      input  += (pitch >> 1) - width;
      output += (xv->width - width) << 2;
   }
}

static void render32_packed(xv_t *xv, const void *input_,
      unsigned width, unsigned height, unsigned pitch)
{
   unsigned x, y;
   const uint32_t *input    = (const uint32_t*)input_;
   uint8_t *output          = (uint8_t*)xv->image->data;
   const uint32_t *tbl      = xv->yuv_table;
   const unsigned img_width = xv->width << 1;

   for (y = 0; y < height; y++)
   {
      for (x = 0; x < width; x++)
      {
         uint32_t p = *input++;
         uint32_t t;
         p = ((p >> 8) & 0xf800) | ((p >> 5) & 0x07e0)
            | ((p >> 3) & 0x1f); /* ARGB -> RGB16 */
         t = tbl[p];
         memcpy(output, &t, sizeof(t));
         memcpy(output + img_width, &t, sizeof(t));
         output += 4;
      }

      input  += (pitch >> 2) - width;
      output += (xv->width - width) << 2;
   }
}

static void render32_yuv12(xv_t *xv, const void *input_,
      unsigned width, unsigned height, unsigned pitch)
{
   unsigned x, y;
   const uint32_t *input = (const uint32_t*)input_;
   const uint32_t *tbl   = xv->yuv_table;
   const uint8_t *e;
   unsigned w0 = xv->width >> 1;
   unsigned w1 = w0 << 1;
   unsigned h0 = xv->height >> 1;
   uint8_t *output       = (uint8_t*)xv->image->data;
   uint8_t *outputu       = (uint8_t*)xv->image->data + 4 * w0 * h0;
   uint8_t *outputv       = (uint8_t*)xv->image->data + 5 * w0 * h0;

   for (y = 0; y < height; y++)
   {
      for (x = 0; x < width; x++)
      {
         uint8_t y0, u, v;
         uint32_t p = *input++;
         p = ((p >> 8) & 0xf800) | ((p >> 5) & 0x07e0)
            | ((p >> 3) & 0x1f); /* ARGB -> RGB16 */

         e         = (const uint8_t*)&tbl[p];
         y0        = e[0];
         u         = e[1];
         v         = e[2];

         output[0] = output[w1] = y0;
	 output[1] = output[w1+1] = y0;
         output+=2;
	 *outputu++ = u;
	 *outputv++ = v;
      }

      input  += (pitch >> 2) - width;
      output += 4 * w0 - 2 * width;
      outputu += (w0 - width);
      outputv += (w0 - width);
   }
}

static void render16_yuv12(xv_t *xv, const void *input_,
      unsigned width, unsigned height, unsigned pitch)
{
   unsigned x, y;
   const uint16_t *input = (const uint16_t*)input_;
   const uint32_t *tbl   = xv->yuv_table;
   unsigned w0 = xv->width >> 1;
   unsigned w1 = w0 << 1;
   unsigned h0 = xv->height >> 1;
   uint8_t *output       = (uint8_t*)xv->image->data;
   uint8_t *outputu       = (uint8_t*)xv->image->data + 4 * w0 * h0;
   uint8_t *outputv       = (uint8_t*)xv->image->data + 5 * w0 * h0;

   for (y = 0; y < height; y++)
   {
      for (x = 0; x < width; x++)
      {
         uint16_t p         = *input++;
         const uint8_t *e   = (const uint8_t*)&tbl[p];
         uint8_t y0         = e[0];
         uint8_t u          = e[1];
         uint8_t v          = e[2];

         output[0] = output[w1] = y0;
	 output[1] = output[w1+1] = y0;
         output+=2;
	 *outputu++ = u;
	 *outputv++ = v;
      }

      input  += (pitch >> 1) - width;
      output += 4 * w0 - 2 * width;
      outputu += (w0 - width);
      outputv += (w0 - width);
   }
}

/* Two glyph texels at a time, one chroma sample for the pair; a pair
 * with no coverage changes nothing and is passed over. Past an odd
 * width the second texel is clear. */
static INLINE void render_glyph_yuv12(xv_t *xv, int base_x, int base_y,
      const uint8_t *glyph, int atlas_width,
      int glyph_width, int glyph_height)
{
   uint8_t *out_luma, *out_u, *out_v;
   int x, y;
   unsigned font_y = xv->font_y;
   unsigned font_u = xv->font_u;
   unsigned font_v = xv->font_v;

   out_luma = (uint8_t*)xv->image->data + base_y * xv->width + (base_x);
   out_u = (uint8_t*)xv->image->data + xv->width * xv->height + (base_y / 2) * xv->width / 2 + (base_x / 2);
   out_v= (uint8_t*)xv->image->data + xv->width * xv->height * 5 / 4 + (base_y / 2) * xv->width / 2 + (base_x / 2);

   for (y = 0; y < glyph_height; y++, glyph += atlas_width, out_luma += xv->width)
   {
      int chroma = y & 1;
      for (x = 0; x < glyph_width; x += 2)
      {
         unsigned a0 = glyph[x];
         unsigned a1 = (x + 1 < glyph_width) ? glyph[x + 1] : 0;
         unsigned as;

         if (!(a0 | a1))
            continue;

         out_luma[x] = (font_y * a0 + (256 - a0) * out_luma[x]) >> 8;
         if (a1)
            out_luma[x + 1] = (font_y * a1
                  + (256 - a1) * out_luma[x + 1]) >> 8;

         if (chroma)
         {
            as         = (a0 + a1) >> 1;
            out_u[x/2] = (font_u * as + (256 - as) * out_u[x/2]) >> 8;
            out_v[x/2] = (font_v * as + (256 - as) * out_v[x/2]) >> 8;
         }
      }

      if (chroma)
      {
         out_u += xv->width / 2;
         out_v += xv->width / 2;
      }
   }
}

static INLINE void render_glyph_yuv_packed(xv_t *xv, int base_x, int base_y,
      const uint8_t *glyph, int atlas_width,
      int glyph_width, int glyph_height)
{
   uint8_t *out;
   int x, y;
   unsigned pitch;
   unsigned luma0    = xv->luma_index[0];
   unsigned luma1    = xv->luma_index[1];
   unsigned chroma_u = xv->chroma_u_index;
   unsigned chroma_v = xv->chroma_v_index;
   unsigned font_y   = xv->font_y;
   unsigned font_u   = xv->font_u;
   unsigned font_v   = xv->font_v;

   pitch          = xv->width << 1; /* YUV formats used are 16 bpp. */
   out = (uint8_t*)xv->image->data + base_y * pitch + (base_x << 1);

   for (y = 0; y < glyph_height; y++, glyph += atlas_width, out += pitch)
   {
      /* 2 input pixels => 4 bytes (2Y, 1U, 1V). */
      for (x = 0; x < glyph_width; x += 2)
      {
         uint8_t *o  = out + (x << 1);
         unsigned a0 = glyph[x];
         unsigned a1 = (x + 1 < glyph_width) ? glyph[x + 1] : 0;
         unsigned as;

         if (!(a0 | a1))
            continue;

         as          = (a0 + a1) >> 1;
         o[luma0]    = (font_y * a0 + (256 - a0) * o[luma0]) >> 8;
         o[luma1]    = (font_y * a1 + (256 - a1) * o[luma1]) >> 8;
         o[chroma_u] = (font_u * as + (256 - as) * o[chroma_u]) >> 8;
         o[chroma_v] = (font_v * as + (256 - as) * o[chroma_v]) >> 8;
      }
   }
}

struct format_desc
{
   void (*render_16)(xv_t *xv, const void *input,
         unsigned width, unsigned height, unsigned pitch);
   void (*render_32)(xv_t *xv, const void *input,
         unsigned width, unsigned height, unsigned pitch);
   void (*render_glyph)(xv_t *xv, int base_x, int base_y,
			const uint8_t *glyph, int atlas_width,
			int glyph_width, int glyph_height);
   char components[4];
   unsigned luma_index[2];
   unsigned u_index;
   unsigned v_index;
   unsigned bits;
   int format;
};

static const struct format_desc formats[] = {
   {
      render16_packed,
      render32_packed,
      render_glyph_yuv_packed,
      { 'Y', 'U', 'Y', 'V' },
      { 0, 2 },
      1,
      3,
      16,
      XvPacked
   },
   {
      render16_packed,
      render32_packed,
      render_glyph_yuv_packed,
      { 'U', 'Y', 'V', 'Y' },
      { 1, 3 },
      0,
      2,
      16,
      XvPacked
   },
   {
      render16_yuv12,
      render32_yuv12,
      render_glyph_yuv12,
      { 'Y', 'U', 'V', 0 },
      { 1, 3 },
      0,
      2,
      12,
      XvPlanar
   },
};

static bool xv_adaptor_set_format(xv_t *xv, Display *dpy,
      XvPortID port, const video_info_t *video)
{
   int i;
   unsigned j;
   int format_count;
   XvImageFormatValues *format = XvListImageFormats(
         g_x11_dpy, port, &format_count);

   if (!format)
      return false;

   for (i = 0; i < format_count; i++)
   {
      for (j = 0; j < ARRAY_SIZE(formats); j++)
      {
         if (format[i].type == XvYUV
               && (unsigned)format[i].bits_per_pixel == formats[j].bits
               && format[i].format == formats[j].format)
         {
            if (format[i].component_order[0] == formats[j].components[0] &&
                  format[i].component_order[1] == formats[j].components[1] &&
                  format[i].component_order[2] == formats[j].components[2] &&
                  format[i].component_order[3] == formats[j].components[3])
            {
               xv->fourcc         = format[i].id;
               xv->fmt_format     = formats[j].format;
               xv->render_func16  = formats[j].render_16;
               xv->render_func32  = formats[j].render_32;
               xv->render_glyph   = formats[j].render_glyph;

               xv->luma_index[0]  = formats[j].luma_index[0];
               xv->luma_index[1]  = formats[j].luma_index[1];
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

static void xv_calc_out_rect(bool keep_aspect,
      struct video_viewport *vp,
      unsigned vp_width, unsigned vp_height)
{
   vp->full_dims   = VIDEO_SCALE_PACK(vp_width, vp_height);
   video_driver_update_viewport(vp, false, keep_aspect, true);
}

static void *xv_init(const video_info_t *video)
{
   unsigned i;
   int ret;
   XEvent event;
   XWindowAttributes target;
   char title[128]                        = {0};
   XSetWindowAttributes attributes        = {0};
   XVisualInfo visualtemplate             = {0};
   unsigned width                         = 0;
   unsigned height                        = 0;
   unsigned adaptor_count                 = 0;
   int visualmatches                      = 0;
   Atom atom                              = 0;
   XVisualInfo *visualinfo                = NULL;
   XvAdaptorInfo *adaptor_info            = NULL;
   settings_t *settings                   = config_get_ptr();
   bool video_disable_composition         = settings->bools.video_disable_composition;
   xv_t                               *xv = (xv_t*)calloc(1, sizeof(*xv));
   if (!xv)
      return NULL;

   XInitThreads();

   g_x11_dpy = XOpenDisplay(NULL);

   if (!g_x11_dpy)
   {
      RARCH_ERR("[XVideo] Cannot connect to the X server.\n");
      RARCH_ERR("[XVideo] Check DISPLAY variable and if X is running.\n");
      goto error;
   }


   if (!XShmQueryExtension(g_x11_dpy))
   {
      RARCH_ERR("[XVideo] XShm extension not found.\n");
      goto error;
   }
   /* The images go out on a connection of their own: their completion
    * events say when one may be drawn into again, read as they come */
   if (!(xv->dpy = XOpenDisplay(NULL)))
   {
      RARCH_ERR("[XVideo] Cannot open the images' connection.\n");
      goto error;
   }
   xv->completion = XShmGetEventBase(xv->dpy) + ShmCompletion;

   xv->keep_aspect = video->force_aspect;
   xv->rgb32       = video->rgb32;

   /* Find an appropriate Xv port. */
   xv->port = 0;
   ret = XvQueryAdaptors(g_x11_dpy,
         DefaultRootWindow(g_x11_dpy), &adaptor_count, &adaptor_info);

   if (ret != Success)
   {
      if (ret == XvBadExtension)
         RARCH_ERR("[XVideo] Xv extension not found.\n");
      else if (ret == XvBadAlloc)
         RARCH_ERR("[XVideo] XvQueryAdaptors() failed to allocate memory.\n");
      else
         RARCH_ERR("[XVideo] Unknown error in XvQueryAdaptors().\n");

      goto error;
   }

   if (adaptor_count == 0)
   {
      RARCH_ERR("[XVideo] XvQueryAdaptors() found 0 adaptors.\n");
      goto error;
   }

   for (i = 0; i < adaptor_count; i++)
   {
      /* Find adaptor that supports both input (memory->drawable)
       * and image (drawable->screen) masks. */

      if (adaptor_info[i].num_formats < 1)
         continue;
      if (!(adaptor_info[i].type & XvInputMask))
         continue;
      if (!(adaptor_info[i].type & XvImageMask))
         continue;
      if (!xv_adaptor_set_format(xv, g_x11_dpy,
               adaptor_info[i].base_id, video))
         continue;

      xv->port     = adaptor_info[i].base_id;
      xv->depth    = adaptor_info[i].formats->depth;
      xv->visualid = adaptor_info[i].formats->visual_id;

      RARCH_LOG("[XVideo] Found suitable XvPort #%u.\n", (unsigned)xv->port);
      break;
   }
   XvFreeAdaptorInfo(adaptor_info);

   if (xv->port == 0)
   {
      RARCH_ERR("[XVideo] Failed to find valid XvPort or format.\n");
      goto error;
   }

   visualtemplate.visualid = xv->visualid;
   visualtemplate.screen   = DefaultScreen(g_x11_dpy);
   visualtemplate.depth    = xv->depth;
   visualtemplate.visual   = 0;
   visualinfo              = XGetVisualInfo(g_x11_dpy, VisualIDMask |
         VisualScreenMask | VisualDepthMask, &visualtemplate, &visualmatches);

   if (!visualinfo)
      goto error;

   if (visualmatches < 1 || !visualinfo->visual)
   {
      RARCH_ERR("[XVideo] Unable to find Xv-compatible visual.\n");
      goto error;
   }

   g_x11_cmap = XCreateColormap(g_x11_dpy,
         DefaultRootWindow(g_x11_dpy), visualinfo->visual, AllocNone);

   attributes.colormap     = g_x11_cmap;
   attributes.border_pixel = 0;
   attributes.event_mask   = StructureNotifyMask | KeyPressMask |
      KeyReleaseMask | ButtonReleaseMask | ButtonPressMask | DestroyNotify | ClientMessage;

   if (video->fullscreen)
   {
      /* Fullscreen at the desktop's size is asked for as 0x0 */
      width      = VIDEO_SCALE_W(video->dims);
      height     = VIDEO_SCALE_H(video->dims);
      if (!width || !height)
      {
         width   = DisplayWidth(g_x11_dpy,  DefaultScreen(g_x11_dpy));
         height  = DisplayHeight(g_x11_dpy, DefaultScreen(g_x11_dpy));
      }
   }
   else
   {
      width      = VIDEO_SCALE_W(video->dims);
      height     = VIDEO_SCALE_H(video->dims);
   }
   g_x11_win  = XCreateWindow(g_x11_dpy, DefaultRootWindow(g_x11_dpy),
         0, 0, width, height,
         0, xv->depth, InputOutput, visualinfo->visual,
         CWColormap | CWBorderPixel | CWEventMask, &attributes);

   XFree(visualinfo);
   XSetWindowBackground(g_x11_dpy, g_x11_win, 0);

   if (video->fullscreen)
   {
      /* Give the window a fullscreen hint before it is shown.
       * This helps GNOME + X11 enter fullscreen properly */
      x11_set_net_wm_fullscreen_hint(g_x11_dpy, g_x11_win);
   }

   if (video->fullscreen && video_disable_composition)
   {
      uint32_t value = 1;
      Atom cardinal = XInternAtom(g_x11_dpy, "CARDINAL", False);
      Atom net_wm_bypass_compositor = XInternAtom(g_x11_dpy,
            "_NET_WM_BYPASS_COMPOSITOR", False);

      RARCH_LOG("[XVideo] Requesting compositor bypass.\n");
      XChangeProperty(g_x11_dpy, g_x11_win,
            net_wm_bypass_compositor, cardinal, 32,
            PropModeReplace, (const unsigned char*)&value, 1);
   }

   XMapWindow(g_x11_dpy, g_x11_win);

   video_driver_get_window_title(title, sizeof(title));

   if (title[0])
      XStoreName(g_x11_dpy, g_x11_win, title);

   x11_set_window_attr(g_x11_dpy, g_x11_win);

   if (video->fullscreen)
   {
      /* Ask for fullscreen again after the window is visible. Some
       * GNOME + X11 setups ignore the first request if it happens too
       * early, which causes RetroArch to only maximise the window */
      x11_event_queue_check(&event);
      x11_set_net_wm_fullscreen(g_x11_dpy, g_x11_win);
      XFlush(g_x11_dpy);
      x11_show_mouse(xv, false);
   }

   xv->gc = XCreateGC(xv->dpy, g_x11_win, 0, 0);

   /* Set colorkey to auto paint, so that Xv video output is always visible. */
   atom = XInternAtom(g_x11_dpy, "XV_AUTOPAINT_COLORKEY", true);
   if (atom != None)
      XvSetPortAttribute(g_x11_dpy, xv->port, atom, 1);

   /* The images are made at the first frame's size */
   x11_install_quit_atom();

   frontend_driver_install_signal_handler();

   xv_init_yuv_tables(xv);
   xv_init_font(xv, settings->paths.path_font, settings->floats.video_font_size);

   if (!x11_input_ctx_new(true))
      goto error;

   /* no input driver of this driver's own: the frontend starts the
    * one that goes with an X11 window */
   input_driver_video_window(INPUT_WINDOW_X11, NULL);

   XGetWindowAttributes(g_x11_dpy, g_x11_win, &target);
   xv_calc_out_rect(xv->keep_aspect, &xv->vp, target.width, target.height);
   xv->vp.full_dims  = VIDEO_SCALE_PACK(target.width, target.height);

   return xv;

error:
   if (visualinfo)
      XFree(visualinfo);
   if (xv->dpy)
      XCloseDisplay(xv->dpy);
   free(xv);
   return NULL;
}

static void xv_views_clear(xv_t *xv);

static void xv_images_free(xv_t *xv)
{
   unsigned i;
   bool attached = false;
   for (i = 0; i < XV_IMAGES; i++)
   {
      if (xv->images[i].image && xv->images[i].shm.shmaddr)
      {
         XShmDetach(xv->dpy, &xv->images[i].shm);
         attached = true;
      }
   }
   /* The server lets the segments go before they are unmapped */
   if (attached)
      XSync(xv->dpy, False);
   for (i = 0; i < XV_IMAGES; i++)
   {
      struct xv_image *im = &xv->images[i];
      if (im->shm.shmaddr)
         shmdt(im->shm.shmaddr);
      if (im->image)
         XFree(im->image);
      memset(im, 0, sizeof(*im));
   }
   xv->cur    = NULL;
   xv->image  = NULL;
   xv->width  = 0;
   xv->height = 0;
}

/* The X server's answer to attaching a segment: a remote server cannot
 * map it */
static bool xv_attach_failed;

static int xv_attach_error(Display *dpy, XErrorEvent *event)
{
   xv_attach_failed = true;
   return 0;
}

/* The images at @width x @height, black, or as near as the adaptor
 * makes them: xv->width and xv->height say */
static bool xv_images_make(xv_t *xv, unsigned width, unsigned height)
{
   unsigned i;
   int (*old)(Display*, XErrorEvent*);
   xv_images_free(xv);
   xv_attach_failed = false;
   old              = XSetErrorHandler(xv_attach_error);
   for (i = 0; i < XV_IMAGES; i++)
   {
      struct xv_image *im = &xv->images[i];
      im->image = XvShmCreateImage(xv->dpy, xv->port, xv->fourcc, NULL,
            (int)width, (int)height, &im->shm);
      if (!im->image)
         break;
      im->shm.shmid = shmget(IPC_PRIVATE, im->image->data_size,
            IPC_CREAT | 0600);
      if (im->shm.shmid < 0)
         break;
      im->shm.shmaddr = im->image->data =
         (char*)shmat(im->shm.shmid, NULL, 0);
      /* Gone with the last detach, whatever happens to this process */
      shmctl(im->shm.shmid, IPC_RMID, NULL);
      if (im->shm.shmaddr == (char*)-1)
      {
         im->shm.shmaddr = NULL;
         break;
      }
      im->shm.readOnly = False;
      if (!XShmAttach(xv->dpy, &im->shm))
         break;
      im->views_last_count = (unsigned)-1;
   }
   /* Made once a size: the attaches answered before an image is used */
   XSync(xv->dpy, False);
   if (i < XV_IMAGES || xv_attach_failed)
   {
      RARCH_ERR("[XVideo] Failed to make the images.\n");
      /* Detaching what never attached is answered here too */
      xv_images_free(xv);
      xv->images_failed = true;
      XSetErrorHandler(old);
      return false;
   }
   XSetErrorHandler(old);
   xv->width  = (unsigned)xv->images[0].image->width;
   xv->height = (unsigned)xv->images[0].image->height;
   for (i = 0; i < XV_IMAGES; i++)
   {
      xv->cur   = &xv->images[i];
      xv->image = xv->cur->image;
      xv_views_clear(xv);
   }
   xv->shown = 0;
   return true;
}

/* The image to draw this frame into, at twice @width x @height: one the
 * server is done reading, the other than the one shown when it can be.
 * False while the server reads every one: the frame is not shown, never
 * waited for. */
static bool xv_check_resize(xv_t *xv, unsigned width, unsigned height)
{
   unsigned i;
   XEvent event;
   if (xv->images_failed)
      return false;
   if (     (xv->width != (width << 1) || xv->height != (height << 1))
         && !xv_images_make(xv, width << 1, height << 1))
      return false;
   /* What the server has finished, as it comes */
   while (XCheckTypedEvent(xv->dpy, xv->completion, &event))
   {
      const XShmCompletionEvent *done = (const XShmCompletionEvent*)&event;
      for (i = 0; i < XV_IMAGES; i++)
         if (xv->images[i].shm.shmseg == done->shmseg)
            xv->images[i].pending = false;
   }
   for (i = 1; i <= XV_IMAGES; i++)
   {
      struct xv_image *im = &xv->images[(xv->shown + i) % XV_IMAGES];
      if (!im->pending)
      {
         xv->cur   = im;
         xv->image = im->image;
         return true;
      }
   }
   return false;
}

/* @msg blended into the @width x @height rectangle of the image at @x0,
 * @y0 (@x0 even), at @pos_x, @pos_y of it from its bottom left */
static void xv_render_msg(xv_t *xv, const char *msg,
      int x0, int y0, int width, int height, float pos_x, float pos_y)
{
   int msg_base_x, msg_base_y;
   const struct font_atlas *atlas = NULL;

   if (!xv->font || !msg || !*msg || width <= 0 || height <= 0)
      return;

   atlas          = xv->font_driver->get_atlas(xv->font);

   msg_base_x     = x0 + (int)(pos_x * width);
   msg_base_y     = y0 + (int)(height * (1.0f - pos_y));

   {
      const struct font_glyph* (*get_glyph)(void*, uint32_t)
                                             = xv->font_driver->get_glyph;
      void *font_data                        = xv->font;
      const struct font_glyph *glyph_q       = get_glyph(font_data, '?');
      struct font_line_metrics *line_metrics = NULL;
      size_t msg_len                         = strlen(msg);
      int line_h                             = 0;
      int line_x                             = msg_base_x;
      int line_y                             = msg_base_y;

      xv->font_driver->get_line_metrics(font_data, &line_metrics);
      if (line_metrics)
         line_h = (int)line_metrics->height;

      /* UTF-8, each line one line height below the last */
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
         int base_x, base_y, glyph_width, glyph_height, max_width, max_height; \
         const uint8_t *src             = NULL; \
         /* Make sure we always start on the correct boundary \
          * so the indices are correct. */ \
         base_x          = ((line_x + (pen_x)) + glyph->draw_offset_x + 1) & ~1; \
         base_y          = (line_y + (pen_y)) + glyph->draw_offset_y; \
         glyph_width     = VIDEO_SCALE_W(glyph->dims); \
         glyph_height    = VIDEO_SCALE_H(glyph->dims); \
         src             = atlas->buffer + VIDEO_SCALE_W(glyph->atlas_pos) + \
                           VIDEO_SCALE_H(glyph->atlas_pos) * atlas->width; \
         if (base_x < x0) \
         { \
            src          += x0 - base_x; \
            glyph_width  -= x0 - base_x; \
            base_x        = x0; \
         } \
         if (base_y < y0) \
         { \
            src          += (y0 - base_y) * (int)atlas->width; \
            glyph_height -= y0 - base_y; \
            base_y        = y0; \
         } \
         max_width        = x0 + width  - base_x; \
         max_height       = y0 + height - base_y; \
         if (     max_width <= 0 || max_height <= 0 \
               || glyph_width <= 0 || glyph_height <= 0) \
            break; \
         if (glyph_width > max_width) \
            glyph_width   = max_width; \
         if (glyph_height > max_height) \
            glyph_height  = max_height; \
         xv->render_glyph(xv, base_x, base_y, src, atlas->width, glyph_width, glyph_height); \
      } while (0)
#include "../font_layout.h"
   }
}

/* BT.601 limited range, from eight bits a channel */
#define XV_RGB_TO_YUV(r, g, b, y, u, v) \
   do \
   { \
      (y) = (( 66 * (r) + 129 * (g) +  25 * (b) + 128) >> 8) +  16; \
      (u) = ((-38 * (r) -  74 * (g) + 112 * (b) + 128) >> 8) + 128; \
      (v) = ((112 * (r) -  94 * (g) -  18 * (b) + 128) >> 8) + 128; \
   } while (0)

/* Grows *@buf to at least @size bytes */
static bool xv_grow(uint32_t **buf, size_t *have, size_t size)
{
   uint32_t *grown;
   if (size <= *have)
      return true;
   if (!(grown = (uint32_t*)realloc(*buf, size)))
      return false;
   *buf  = grown;
   *have = size;
   return true;
}

/* One pixel of the frame in the image's format: for packed formats the
 * whole macropixel, both luma slots the same; for planar Y, U, V. At
 * 32 bits a pixel from all eight bits a channel, not through the 16-bit
 * table. */
static INLINE uint32_t xv_views_pixel(const xv_t *xv,
      const uint8_t *row, unsigned x, bool rgb32)
{
   uint32_t e;
   uint8_t *b = (uint8_t*)&e;
   int r, g, bl, y, u, v;
   if (!rgb32)
      return xv->yuv_table[((const uint16_t*)row)[x]];
   {
      uint32_t p = ((const uint32_t*)row)[x];
      r  = (int)((p >> 16) & 0xff);
      g  = (int)((p >>  8) & 0xff);
      bl = (int)( p        & 0xff);
   }
   XV_RGB_TO_YUV(r, g, bl, y, u, v);
   if (xv->fmt_format == XvPlanar)
   {
      b[0] = (uint8_t)y;
      b[1] = (uint8_t)u;
      b[2] = (uint8_t)v;
      b[3] = 0;
   }
   else
   {
      b[xv->luma_index[0]] = (uint8_t)y;
      b[xv->luma_index[1]] = (uint8_t)y;
      b[xv->chroma_u_index] = (uint8_t)u;
      b[xv->chroma_v_index] = (uint8_t)v;
   }
   return e;
}

/* The image black */
static void xv_views_clear(xv_t *xv)
{
   size_t i;
   uint8_t *img = (uint8_t*)xv->image->data;
   size_t luma  = (size_t)xv->width * xv->height;
   if (xv->fmt_format == XvPlanar)
   {
      memset(img, 16, luma);
      memset(img + luma, 128, luma / 2);
   }
   else
   {
      uint32_t black = xv->yuv_table[0];
      for (i = 0; i < luma / 2; i++)
         memcpy(img + i * 4, &black, sizeof(black));
   }
}

/* View @v into the image where @cut of @dst, both in the canvas's
 * pixels less @shift_x, lies: each canvas pixel a 2x2 block of the
 * image, nearest sampled straight from the frame. With @parity 0 or 1
 * only the canvas rows of that parity. */
static void xv_views_place(xv_t *xv, const void *frame, unsigned pitch,
      bool rgb32, const struct retro_video_view *v,
      const video_views_rect_t *dst, const video_views_rect_t *cut,
      int shift_x, int parity)
{
   int cx, cy;
   int dx        = VIDEO_POS_X(dst->pos);
   int dy        = VIDEO_POS_Y(dst->pos);
   unsigned dw   = VIDEO_SCALE_W(dst->dims);
   unsigned dh   = VIDEO_SCALE_H(dst->dims);
   int x0        = VIDEO_POS_X(cut->pos);
   int y0        = VIDEO_POS_Y(cut->pos);
   int x1        = x0 + (int)VIDEO_SCALE_W(cut->dims);
   int y1        = y0 + (int)VIDEO_SCALE_H(cut->dims);
   uint8_t *img  = (uint8_t*)xv->image->data;
   size_t iw     = xv->width;
   size_t luma   = iw * xv->height;
   uint32_t step = (uint32_t)(((uint64_t)v->width << 16) / dw);
   uint32_t fx0  = (uint32_t)(((uint64_t)(x0 - dx) * v->width << 16) / dw);

   for (cy = y0; cy < y1; cy++)
   {
      uint32_t fx      = fx0;
      unsigned sy      = v->y + (unsigned)(((uint64_t)(cy - dy)
               * v->height) / dh);
      const uint8_t *row = (const uint8_t*)frame + (size_t)sy * pitch;
      if (parity >= 0 && (cy & 1) != parity)
         continue;
      if (xv->fmt_format == XvPlanar)
      {
         uint8_t *l0 = img + (size_t)(2 * cy) * iw + 2 * (x0 - shift_x);
         uint8_t *l1 = l0 + iw;
         uint8_t *ou = img + luma + (size_t)cy * (iw / 2) + (x0 - shift_x);
         uint8_t *ov = img + luma + luma / 4 + (size_t)cy * (iw / 2)
            + (x0 - shift_x);
         for (cx = x0; cx < x1; cx++, fx += step)
         {
            unsigned sx    = fx >> 16;
            uint32_t e;
            const uint8_t *b = (const uint8_t*)&e;
            if (sx >= v->width)
               sx = v->width - 1;
            e     = xv_views_pixel(xv, row, v->x + sx, rgb32);
            l0[0] = l0[1] = l1[0] = l1[1] = b[0];
            *ou++ = b[1];
            *ov++ = b[2];
            l0   += 2;
            l1   += 2;
         }
      }
      else
      {
         uint8_t *o0 = img + (size_t)(2 * cy) * iw * 2
            + (size_t)(x0 - shift_x) * 4;
         uint8_t *o1 = o0 + iw * 2;
         for (cx = x0; cx < x1; cx++, fx += step)
         {
            unsigned sx = fx >> 16;
            uint32_t e;
            if (sx >= v->width)
               sx = v->width - 1;
            e = xv_views_pixel(xv, row, v->x + sx, rgb32);
            memcpy(o0, &e, sizeof(e));
            memcpy(o1, &e, sizeof(e));
            o0 += 4;
            o1 += 4;
         }
      }
   }
}

/* The menu's texture scaled over the @w x @h rectangle of the canvas
 * at @x0, @y0 and blended by its alpha into the image, each canvas
 * pixel a 2x2 block of it */
static void xv_views_blend_menu(xv_t *xv, int x0, int y0, int w, int h,
      unsigned cdims)
{
   int x, y;
   struct scaler_ctx *ctx;
   unsigned alpha  = (unsigned)(xv->tex_alpha * 256.0f);
   uint8_t *img    = (uint8_t*)xv->image->data;
   size_t iw       = xv->width;
   size_t luma     = iw * xv->height;
   int cx0         = x0 < 0 ? 0 : x0;
   int cy0         = y0 < 0 ? 0 : y0;
   int cx1         = x0 + w > (int)VIDEO_SCALE_W(cdims)
      ? (int)VIDEO_SCALE_W(cdims) : x0 + w;
   int cy1         = y0 + h > (int)VIDEO_SCALE_H(cdims)
      ? (int)VIDEO_SCALE_H(cdims) : y0 + h;

   if (     !xv->tex_frame || w <= 0 || h <= 0 || cx1 <= cx0 || cy1 <= cy0
         || !xv_grow(&xv->menu_scaled, &xv->menu_scaled_size,
            (size_t)w * h * sizeof(uint32_t))
         || !(ctx = video_views_cpu_scaler(&xv->views_cpu,
               xv->tex_rgb32 ? SCALER_FMT_ARGB8888 : SCALER_FMT_RGBA4444,
               SCALER_TYPE_BILINEAR, (int)xv->tex_width,
               (int)xv->tex_height, (int)xv->tex_pitch, w, h,
               w * (int)sizeof(uint32_t))))
      return;
   scaler_ctx_scale_direct(ctx, xv->menu_scaled, xv->tex_frame);
   if (alpha > 256)
      alpha = 256;
   for (y = cy0; y < cy1; y++)
   {
      const uint32_t *src = xv->menu_scaled + (size_t)(y - y0) * w
         + (cx0 - x0);
      for (x = cx0; x < cx1; x++, src++)
      {
         int sy, su, sv;
         int a = (int)(((*src >> 24) * alpha) >> 8);
         if (!a)
            continue;
         XV_RGB_TO_YUV((int)((*src >> 16) & 0xff),
               (int)((*src >> 8) & 0xff), (int)(*src & 0xff), sy, su, sv);
#define XV_BLEND(p, val) (p) = (uint8_t)(((val) * a + (p) * (255 - a)) / 255)
         if (xv->fmt_format == XvPlanar)
         {
            uint8_t *l = img + (size_t)(2 * y) * iw + 2 * x;
            size_t   c = (size_t)y * (iw / 2) + x;
            XV_BLEND(l[0], sy);
            XV_BLEND(l[1], sy);
            XV_BLEND(l[iw], sy);
            XV_BLEND(l[iw + 1], sy);
            XV_BLEND(img[luma + c], su);
            XV_BLEND(img[luma + luma / 4 + c], sv);
         }
         else
         {
            unsigned r;
            uint8_t *m = img + (size_t)(2 * y) * iw * 2 + (size_t)x * 4;
            for (r = 0; r < 2; r++, m += iw * 2)
            {
               XV_BLEND(m[xv->luma_index[0]], sy);
               XV_BLEND(m[xv->luma_index[1]], sy);
               XV_BLEND(m[xv->chroma_u_index], su);
               XV_BLEND(m[xv->chroma_v_index], sv);
            }
         }
#undef XV_BLEND
      }
   }
}

/* The menu over the views: where the game's viewport would be in the
 * window or, laid out at one eye's UI size, in each eye */
static void xv_views_menu(xv_t *xv, const video_frame_info_t *info,
      unsigned cdims)
{
   unsigned a;
   struct video_viewport fit;
   const video_views_layout_t *layout = &info->views_layout;
   bool per_eye = video_views_ui_per_eye(&info->views, layout, true);

   memset(&fit, 0, sizeof(fit));
   fit.full_dims = per_eye ? layout->ui_dims : cdims;
   video_driver_update_viewport(&fit, false, true, true);
   if (!per_eye)
   {
      xv_views_blend_menu(xv, VIDEO_POS_X(fit.pos), VIDEO_POS_Y(fit.pos),
            (int)VIDEO_SCALE_W(fit.dims), (int)VIDEO_SCALE_H(fit.dims),
            cdims);
      return;
   }
   for (a = 0; a < layout->num_areas; a++)
   {
      const video_views_rect_t *r = &layout->areas[a];
      double sx = (double)VIDEO_SCALE_W(r->dims)
         / VIDEO_SCALE_W(layout->ui_dims);
      double sy = (double)VIDEO_SCALE_H(r->dims)
         / VIDEO_SCALE_H(layout->ui_dims);
      xv_views_blend_menu(xv,
            VIDEO_POS_X(r->pos) + (int)(VIDEO_POS_X(fit.pos) * sx),
            VIDEO_POS_Y(r->pos) + (int)(VIDEO_POS_Y(fit.pos) * sy),
            (int)(VIDEO_SCALE_W(fit.dims) * sx + 0.5),
            (int)(VIDEO_SCALE_H(fit.dims) * sy + 0.5), cdims);
   }
}

/* A core's views: laid out on the window, at the scale that draws no
 * view larger than it is - the window's own with @menu over them -
 * converted into the image at twice that, which the adaptor scales to
 * the window. Each view straight from the frame, interlaced's into its
 * eye's rows; anaglyph's eyes drawn side by side and blended a row at a
 * time into the image.
 * Returns the canvas's size, 0 when the frame is to be drawn whole. */
static unsigned xv_views_frame(xv_t *xv, const void *frame,
      unsigned dims, unsigned pitch, bool rgb32,
      const video_frame_info_t *info, bool menu, bool *skip)
{
   unsigned i, cdims;
   float scale;
   const video_views_layout_t *layout = &info->views_layout;
   bool views = frame && info->views.num_views && xv->yuv_table
      && video_views_fit_frame(&info->views, dims);

   retro_atomic_store_release_int(&xv->views_fallback,
         (info->views.num_views && !views) ? 1 : 0);
   xv->views_on = views;
   if (!views)
      return 0;

   scale = menu ? 1.0f : video_views_cpu_scale(&info->views, layout);
   cdims = video_views_cpu_dims(info->dims, scale);
   if (!xv_check_resize(xv, VIDEO_SCALE_W(cdims), VIDEO_SCALE_H(cdims)))
   {
      /* Every image still read: this frame is not shown */
      *skip = xv->width != 0;
      return 0;
   }
   /* An adaptor that gave a smaller image draws the frame whole */
   if (     xv->width  != VIDEO_SCALE_W(cdims) << 1
         || xv->height != VIDEO_SCALE_H(cdims) << 1)
   {
      retro_atomic_store_release_int(&xv->views_fallback, 1);
      xv->views_on = false;
      return 0;
   }
   if (!xv->views_seen)
   {
      xv->views_seen = true;
      RARCH_LOG("[XVideo] Views drawn straight from the frame.\n");
   }

   /* Interlaced: each eye's placements straight into its own rows */
   if (layout->stereo_mode == VIDEO_STEREO_MODE_INTERLACED)
   {
      if (     menu
            || xv->cur->views_last_count != layout->num_placements + 1
            || memcmp(xv->cur->views_last, layout->placements,
               layout->num_placements * sizeof(*layout->placements)))
      {
         xv_views_clear(xv);
         xv->cur->views_last_count = menu
            ? (unsigned)-1 : layout->num_placements + 1;
         memcpy(xv->cur->views_last, layout->placements,
               layout->num_placements * sizeof(*layout->placements));
      }
      for (i = 0; i < layout->num_placements; i++)
      {
         video_views_rect_t dst, cut;
         const video_views_placement_t *p = &layout->placements[i];
         if (video_views_cpu_cut(layout, p, scale, cdims, true, &dst, &cut))
            xv_views_place(xv, frame, pitch, rgb32,
                  &info->views.views[p->view], &dst, &cut,
                  p->area ? (int)VIDEO_SCALE_W(cdims) : 0, (int)p->area);
      }
      return cdims;
   }

   /* Anaglyph mixes both eyes' channels: the eyes drawn side by side,
    * then blended a row at a time straight into the image */
   if (layout->offscreen)
   {
      struct retro_video_view row;
      video_views_rect_t at;
      unsigned y, w = VIDEO_SCALE_W(cdims);
      size_t size   = (size_t)VIDEO_SCALE_AREA(cdims) * sizeof(uint32_t);
      if (     !xv_grow(&xv->views_canvas, &xv->views_canvas_size,
               2 * size)
            || !xv_grow(&xv->views_row, &xv->views_row_size,
               w * sizeof(uint32_t)))
         return 0;
      video_views_cpu_draw_placements(&xv->views_cpu, &info->views, layout,
            frame, pitch, rgb32 ? SCALER_FMT_ARGB8888 : SCALER_FMT_RGB565,
            info->video_smooth ? SCALER_TYPE_BILINEAR : SCALER_TYPE_POINT,
            scale, cdims, true, xv->views_canvas, 2 * w);
      memset(&row, 0, sizeof(row));
      row.width  = w;
      row.height = 1;
      for (y = 0; y < VIDEO_SCALE_H(cdims); y++)
      {
         video_views_blend_xrgb8888(xv->views_canvas + (size_t)y * 2 * w,
               2 * w, xv->views_row, w, VIDEO_SCALE_PACK(w, 1),
               layout->stereo_mode);
         at.pos  = VIDEO_POS_PACK(0, y);
         at.dims = VIDEO_SCALE_PACK(w, 1);
         xv_views_place(xv, xv->views_row, w * 4, true, &row, &at, &at,
               0, -1);
      }
      /* Laid out anew when the eyes are no longer blended */
      xv->cur->views_last_count = (unsigned)-1;
      return cdims;
   }

   /* Cleared where the placements move, and under the menu, which is
    * blended into it */
   if (     menu
         || xv->cur->views_last_count != layout->num_placements
         || memcmp(xv->cur->views_last, layout->placements,
            layout->num_placements * sizeof(*layout->placements)))
   {
      xv_views_clear(xv);
      xv->cur->views_last_count = menu ? (unsigned)-1 : layout->num_placements;
      memcpy(xv->cur->views_last, layout->placements,
            layout->num_placements * sizeof(*layout->placements));
   }
   for (i = 0; i < layout->num_placements; i++)
   {
      video_views_rect_t dst, cut;
      const video_views_placement_t *p = &layout->placements[i];
      if (video_views_cpu_cut(layout, p, scale, cdims, false, &dst, &cut))
         xv_views_place(xv, frame, pitch, rgb32,
               &info->views.views[p->view], &dst, &cut, 0, -1);
   }
   return cdims;
}

/* The messages, the statistics and, per eye where the UI is, in each
 * eye's area. @views_dims is the canvas's size while views draw, the
 * image twice it. */
static void xv_frame_text(xv_t *xv, const char *msg,
      const video_frame_info_t *info, unsigned views_dims,
      unsigned width, unsigned height)
{
   unsigned a;
   const video_views_layout_t *layout = &info->views_layout;
   bool per_eye = views_dims && video_views_ui_per_eye(&info->views,
         layout, true);

   if (info->statistics_show && info->stat_text && *info->stat_text)
   {
      xv_render_msg(xv, info->stat_text, 0, 0, (int)width << 1,
            (int)height << 1, info->osd_stat_params.x,
            info->osd_stat_params.y);
      xv->cur->views_last_count = (unsigned)-1;
   }
   if (!msg || !*msg)
      return;
   xv->cur->views_last_count = (unsigned)-1;
   if (!per_eye)
   {
      xv_render_msg(xv, msg, 0, 0, (int)width << 1, (int)height << 1,
            info->font_msg_pos_x, info->font_msg_pos_y);
      return;
   }
   for (a = 0; a < layout->num_areas; a++)
   {
      /* The area in the window, then in the image */
      video_views_rect_t r, cut;
      double s = (double)VIDEO_SCALE_W(views_dims)
         / VIDEO_SCALE_W(xv->out_dims);
      video_views_cpu_rect(&layout->areas[a], (float)s, &r);
      if (!video_views_clip(&r, views_dims, &cut))
         continue;
      xv_render_msg(xv, msg, VIDEO_POS_X(cut.pos) * 2,
            VIDEO_POS_Y(cut.pos) * 2, (int)VIDEO_SCALE_W(cut.dims) * 2,
            (int)VIDEO_SCALE_H(cut.dims) * 2, info->font_msg_pos_x,
            info->font_msg_pos_y);
   }
}

static bool xv_frame(void *data, const void *frame,
      unsigned dims, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   unsigned width            = VIDEO_SCALE_W(dims);
   unsigned height           = VIDEO_SCALE_H(dims);
   unsigned window           = 0;
   unsigned views_dims       = 0;
   bool menu                 = false;
   bool skip                 = false;
   xv_t *xv                  = (xv_t*)data;
   bool rgb32                = xv->rgb32;
#ifdef HAVE_MENU
   bool menu_is_alive        = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;

   menu_driver_frame(menu_is_alive, video_info);
   menu                      = menu_is_alive && xv->tex_frame;
#endif

   if (!frame && !menu)
      return true;

   /* The window's size as its events last said: no round trip */
   x11_get_video_size(xv, &window);
   if (!VIDEO_SCALE_W(window) || !VIDEO_SCALE_H(window))
      return true;
   /* What views are laid out on */
   if (xv->out_dims != window)
   {
      xv->out_dims = window;
      video_driver_set_output_dims(window);
   }

   views_dims = xv_views_frame(xv, frame, dims, pitch, rgb32, video_info,
         menu, &skip);
   if (skip)
      return true;

   if (views_dims)
   {
      if (menu)
         xv_views_menu(xv, video_info, views_dims);
      /* The whole window */
      width              = VIDEO_SCALE_W(views_dims);
      height             = VIDEO_SCALE_H(views_dims);
      xv->vp.pos         = VIDEO_POS_PACK(0, 0);
      xv->vp.dims        = window;
      xv->vp.full_dims   = window;
   }
   else
   {
      /* The menu blended over the game's frame, or over black */
      bool alone = menu && !(frame && width && height);
      if (alone)
      {
         width  = xv->tex_width;
         height = xv->tex_height;
      }
      if (!width || !height)
         return true;
      if (!xv_check_resize(xv, width, height))
         return true;
      /* The views' image is laid out anew when they come back */
      xv->cur->views_last_count = (unsigned)-1;

      if (alone)
         xv_views_clear(xv);
      else if (rgb32)
         xv->render_func32(xv, frame, width, height, pitch);
      else
         xv->render_func16(xv, frame, width, height, pitch);
      if (menu)
         xv_views_blend_menu(xv, 0, 0, (int)width, (int)height,
               VIDEO_SCALE_PACK(width, height));

      xv_calc_out_rect(xv->keep_aspect, &xv->vp,
            VIDEO_SCALE_W(window), VIDEO_SCALE_H(window));
      xv->vp.full_dims   = window;
   }

   xv_frame_text(xv, msg, video_info, views_dims, width, height);

   xv->put_width  = width << 1;
   xv->put_height = height << 1;
   /* Its completion says when it may be drawn into again */
   XvShmPutImage(xv->dpy, xv->port, g_x11_win, xv->gc, xv->image,
         0, 0, width << 1, height << 1,
         VIDEO_POS_X(xv->vp.pos), VIDEO_POS_Y(xv->vp.pos),
         VIDEO_SCALE_W(xv->vp.dims), VIDEO_SCALE_H(xv->vp.dims), True);
   XFlush(xv->dpy);
   xv->cur->pending = true;
   xv->shown        = (unsigned)(xv->cur - xv->images);

   x11_update_title(NULL);

   return true;
}

static bool xv_has_windowed(void *data) { return true; }

static void xv_free(void *data)
{
   xv_t *xv = (xv_t*)data;

   if (!xv)
      return;

   x11_input_ctx_destroy();

   if (xv->dpy)
   {
      xv_images_free(xv);
      if (xv->gc)
         XFreeGC(xv->dpy, xv->gc);
      XCloseDisplay(xv->dpy);
   }

   x11_window_destroy(true);
   x11_colormap_destroy();

   XCloseDisplay(g_x11_dpy);

   free(xv->yuv_table);
   video_views_cpu_free(&xv->views_cpu);
   free(xv->views_row);
   free(xv->views_canvas);
   free(xv->menu_scaled);

   if (xv->font)
      xv->font_driver->free(xv->font);

   free(xv);
}

/* The viewport as the adaptor shows it: the part of the image last put,
 * scaled to it nearest-sampled and taken back to RGB, BGR24 bottom-up */
static bool xv_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   int x, y;
   unsigned vw, vh;
   size_t iw, luma;
   const uint8_t *img;
   xv_t *xv = (xv_t*)data;

   if (     !xv || !xv->images[xv->shown].image
         || !xv->put_width || !xv->put_height)
      return false;
   vw   = VIDEO_SCALE_W(xv->vp.dims);
   vh   = VIDEO_SCALE_H(xv->vp.dims);
   img  = (const uint8_t*)xv->images[xv->shown].image->data;
   iw   = xv->width;
   luma = iw * xv->height;
   for (y = (int)vh - 1; y >= 0; y--)
   {
      unsigned sy = (unsigned)(((uint64_t)y * xv->put_height) / vh);
      for (x = 0; x < (int)vw; x++)
      {
         int c, d, e, r, g, b;
         unsigned sx = (unsigned)(((uint64_t)x * xv->put_width) / vw);
         if (xv->fmt_format == XvPlanar)
         {
            size_t ci = (size_t)(sy >> 1) * (iw >> 1) + (sx >> 1);
            c = img[(size_t)sy * iw + sx] - 16;
            d = img[luma + ci] - 128;
            e = img[luma + luma / 4 + ci] - 128;
         }
         else
         {
            const uint8_t *m = img + (size_t)sy * iw * 2 + (sx >> 1) * 4;
            c = m[xv->luma_index[sx & 1]] - 16;
            d = m[xv->chroma_u_index] - 128;
            e = m[xv->chroma_v_index] - 128;
         }
         r = (298 * c + 409 * e + 128) >> 8;
         g = (298 * c - 100 * d - 208 * e + 128) >> 8;
         b = (298 * c + 516 * d + 128) >> 8;
         *buffer++ = (uint8_t)(b < 0 ? 0 : (b > 255 ? 255 : b));
         *buffer++ = (uint8_t)(g < 0 ? 0 : (g > 255 ? 255 : g));
         *buffer++ = (uint8_t)(r < 0 ? 0 : (r > 255 ? 255 : r));
      }
   }
   (void)is_idle;
   return true;
}

static void xv_viewport_info(void *data, struct video_viewport *vp)
{
   xv_t *xv = (xv_t*)data;
   *vp = xv->vp;
}

static uint32_t xv_poke_get_flags(void *data)
{
   uint32_t flags = 0;
   xv_t *xv       = (xv_t*)data;
   if (xv)
   {
      BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS);
      if (retro_atomic_load_acquire_int(&xv->views_fallback))
         BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS_FALLBACK);
   }
   return flags;
}

static void xv_poke_set_texture_frame(void *data,
      const void *frame, bool rgb32,
      unsigned dims, float alpha)
{
   xv_t *xv  = (xv_t*)data;
   xv->tex_frame = (void*)frame;
   xv->tex_rgb32 = rgb32;
   xv->tex_width = VIDEO_SCALE_W(dims);
   xv->tex_height = VIDEO_SCALE_H(dims);
   xv->tex_pitch = VIDEO_SCALE_W(dims) * (rgb32 ? 4 : 2);
   xv->tex_alpha = alpha;
}

static video_poke_interface_t xv_video_poke_interface = {
   xv_poke_get_flags,
   NULL, /* load_texture */
   NULL, /* unload_texture */
   NULL, /* set_video_mode */
#ifdef HAVE_XF86VM
   x11_get_refresh_rate,
#else
   NULL, /* get_refresh_rate */
#endif
   NULL, /* set_filtering */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   NULL, /* set_aspect_ratio */
   NULL, /* apply_state_changes */
   xv_poke_set_texture_frame,
   NULL, /* set_texture_enable */
   NULL, /* set_osd_msg */
   x11_show_mouse,
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL  /* set_hdr_subpixel_layout */
};

static void xv_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &xv_video_poke_interface;
}

static bool xv_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

video_driver_t video_xvideo = {
   xv_init,
   xv_frame,
   xv_set_nonblock_state,
   x11_alive,
   x11_has_focus_internal,
   x11_suspend_screensaver,
   xv_has_windowed,
   xv_set_shader,
   xv_free,
   "xvideo",
   NULL, /* set_viewport */
   NULL, /* set_rotation */
   xv_viewport_info,
   xv_read_viewport,
#ifdef HAVE_OVERLAY
   NULL, /* get_overlay_interface */
#endif
   xv_get_poke_interface,
   NULL, /* wrap_type_to_enum */
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   NULL  /* gfx_widgets_enabled */
#endif
};

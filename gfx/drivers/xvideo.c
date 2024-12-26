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

#include <retro_inline.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../font_driver.h"

#include "../../configuration.h"
#include "../../frontend/frontend_driver.h"
#include "../../input/input_driver.h"
#include "../../verbosity.h"

#include "../common/x11_common.h"

/* Adapted from bSNES and MPlayer source. */

typedef struct xv
{
   GC gc;
   XShmSegmentInfo shminfo;

   XvPortID port;

   XvImage *image;

   struct video_viewport vp;

   uint8_t *ytable;
   uint8_t *utable;
   uint8_t *vtable;

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

   void *tex_frame;
   unsigned tex_width;
   unsigned tex_height;
   unsigned tex_pitch;
   unsigned tex_rgb32;
} xv_t;

static void xv_set_nonblock_state(void *data, bool state, bool c, unsigned d)
{
   xv_t *xv  = (xv_t*)data;
   Atom atom = XInternAtom(g_x11_dpy, "XV_SYNC_TO_VBLANK", true);

   if (atom != None && xv->port)
      XvSetPortAttribute(g_x11_dpy, xv->port, atom, !state);
   else
      RARCH_WARN("Failed to set SYNC_TO_VBLANK attribute.\n");
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
   *u     = y_ < 0 ? 0 : (u_ > 255 ? 255 : u_);
   *v     = v_ < 0 ? 0 : (v_ > 255 ? 255 : v_);
}

static void xv_init_yuv_tables(xv_t *xv)
{
   unsigned i;
   xv->ytable = (uint8_t*)malloc(0x10000);
   xv->utable = (uint8_t*)malloc(0x10000);
   xv->vtable = (uint8_t*)malloc(0x10000);

   for (i = 0; i < 0x10000; i++)
   {
      /* Extract RGB565 color data from i */
      unsigned r = (i >> 11) & 0x1f;
      unsigned g = (i >> 5)  & 0x3f;
      unsigned b = (i >> 0)  & 0x1f;
      r          = (r << 3) | (r >> 2);  /* R5->R8 */
      g          = (g << 2) | (g >> 4);  /* G6->G8 */
      b          = (b << 3) | (b >> 2);  /* B5->B8 */

      xv_calculate_yuv(&xv->ytable[i],
            &xv->utable[i], &xv->vtable[i], r, g, b);
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
            video_font_size))
   {
      int r = msg_color_r * 255;
      int g = msg_color_g * 255;
      int b = msg_color_b * 255;
      r = (r < 0 ? 0 : (r > 255 ? 255 : r));
      g = (g < 0 ? 0 : (g > 255 ? 255 : g));
      b = (b < 0 ? 0 : (b > 255 ? 255 : b));

      xv_calculate_yuv(&xv->font_y, &xv->font_u, &xv->font_v,
            r, g, b);
   }
   else
      RARCH_LOG("[XVideo]: Could not initialize fonts.\n");
}

/* We render @ 2x scale to combat chroma downsampling.
 * Also makes fonts more bearable. */
static void render16_yuy2(xv_t *xv, const void *input_,
      unsigned width, unsigned height, unsigned pitch)
{
   unsigned x, y;
   const uint16_t *input = (const uint16_t*)input_;
   uint8_t *output       = (uint8_t*)xv->image->data;

   for (y = 0; y < height; y++)
   {
      for (x = 0; x < width; x++)
      {
         uint16_t p         = *input++;
         uint8_t y0         = xv->ytable[p];
         uint8_t u          = xv->utable[p];
         uint8_t v          = xv->vtable[p];

         unsigned img_width = xv->width << 1;

         output[0] = output[img_width]     = y0;
         output[1] = output[img_width + 1] = u;
         output[2] = output[img_width + 2] = y0;
         output[3] = output[img_width + 3] = v;
         output += 4;
      }

      input  += (pitch >> 1) - width;
      output += (xv->width - width) << 2;
   }
}

static void render16_uyvy(xv_t *xv, const void *input_,
      unsigned width, unsigned height, unsigned pitch)
{
   unsigned x, y;
   const uint16_t *input = (const uint16_t*)input_;
   uint8_t       *output = (uint8_t*)xv->image->data;

   for (y = 0; y < height; y++)
   {
      for (x = 0; x < width; x++)
      {
         uint16_t p         = *input++;
         uint8_t y0         = xv->ytable[p];
         uint8_t u          = xv->utable[p];
         uint8_t v          = xv->vtable[p];
         unsigned img_width = xv->width << 1;

         output[0] = output[img_width]     = u;
         output[1] = output[img_width + 1] = y0;
         output[2] = output[img_width + 2] = v;
         output[3] = output[img_width + 3] = y0;
         output += 4;
      }

      input  += (pitch >> 1) - width;
      output += (xv->width - width) << 2;
   }
}

static void render32_yuy2(xv_t *xv, const void *input_,
      unsigned width, unsigned height, unsigned pitch)
{
   unsigned x, y;
   const uint32_t *input = (const uint32_t*)input_;
   uint8_t *output       = (uint8_t*)xv->image->data;

   for (y = 0; y < height; y++)
   {
      for (x = 0; x < width; x++)
      {
         uint8_t y0, u, v;
         unsigned img_width;
         uint32_t p = *input++;
         p = ((p >> 8) & 0xf800) | ((p >> 5) & 0x07e0)
            | ((p >> 3) & 0x1f); /* ARGB -> RGB16 */

         y0        = xv->ytable[p];
         u         = xv->utable[p];
         v         = xv->vtable[p];

         img_width = xv->width << 1;
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

static void render32_uyvy(xv_t *xv, const void *input_,
      unsigned width, unsigned height, unsigned pitch)
{
   unsigned x, y;
   const uint32_t *input = (const uint32_t*)input_;
   uint16_t *output      = (uint16_t*)xv->image->data;

   for (y = 0; y < height; y++)
   {
      for (x = 0; x < width; x++)
      {
         uint8_t y0, u, v;
         unsigned img_width;
         uint32_t p = *input++;
         p = ((p >> 8) & 0xf800)
            | ((p >> 5) & 0x07e0) | ((p >> 3) & 0x1f); /* ARGB -> RGB16 */

         y0        = xv->ytable[p];
         u         = xv->utable[p];
         v         = xv->vtable[p];

         img_width = xv->width << 1;
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

static void render32_yuv12(xv_t *xv, const void *input_,
      unsigned width, unsigned height, unsigned pitch)
{
   unsigned x, y;
   const uint32_t *input = (const uint32_t*)input_;
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
         unsigned img_width;
         uint32_t p = *input++;
         p = ((p >> 8) & 0xf800) | ((p >> 5) & 0x07e0)
            | ((p >> 3) & 0x1f); /* ARGB -> RGB16 */

         y0        = xv->ytable[p];
         u         = xv->utable[p];
         v         = xv->vtable[p];

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
         uint8_t y0         = xv->ytable[p];
         uint8_t u          = xv->utable[p];
         uint8_t v          = xv->vtable[p];

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

static INLINE void render_glyph_yuv12(xv_t *xv, int base_x, int base_y,
				      const uint8_t *glyph, int atlas_width,
				      int glyph_width, int glyph_height)
{
   uint8_t *out_luma, *out_u, *out_v;
   int x, y, i;

   out_luma = (uint8_t*)xv->image->data + base_y * xv->width + (base_x);
   out_u = (uint8_t*)xv->image->data + xv->width * xv->height + (base_y / 2) * xv->width / 2 + (base_x / 2);
   out_v= (uint8_t*)xv->image->data + xv->width * xv->height * 5 / 4 + (base_y / 2) * xv->width / 2 + (base_x / 2);

   for (y = 0; y < glyph_height; y++, glyph += atlas_width, out_luma += xv->width)
   {
      /* 2 input pixels => 4 bytes (2Y, 1U, 1V). */

      for (x = 0; x < glyph_width; x += 2)
      {
	 unsigned alpha[2], alpha_sub, blended;

	 alpha[0] = glyph[x + 0];
	 alpha[1] = 0;

	 if (x + 1 < glyph_width)
	    alpha[1] = glyph[x + 1];

	 /* Blended alpha for the sub-sampled U/V channels. */
	 alpha_sub = (alpha[0] + alpha[1]) >> 1;

	 for (i = 0; i < 2; i++)
	 {
	    unsigned blended = (xv->font_y * alpha[i]
				+ ((256 - alpha[i]) * out_luma[x+i])) >> 8;
	    out_luma[x+i] = blended;
	 }

	 /* Blend chroma channels */
	 if (y & 1)
	 {
	    blended = (xv->font_u * alpha_sub
		       + ((256 - alpha_sub) * out_u[x/2])) >> 8;
	    out_u[x / 2] = blended;

	    blended = (xv->font_v * alpha_sub
		       + ((256 - alpha_sub) * out_v[x/2])) >> 8;
	    out_v[x/2] = blended;
	 }
      }

      if (y & 1)
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
   uint8_t *out                   = NULL;
   int x, y, i;
   unsigned luma_index[2], pitch;
   unsigned chroma_u_index, chroma_v_index;

   luma_index[0]  = xv->luma_index[0];
   luma_index[1]  = xv->luma_index[1];

   chroma_u_index = xv->chroma_u_index;
   chroma_v_index = xv->chroma_v_index;

   pitch          = xv->width << 1; /* YUV formats used are 16 bpp. */
   out = (uint8_t*)xv->image->data + base_y * pitch + (base_x << 1);

   for (y = 0; y < glyph_height; y++, glyph += atlas_width, out += pitch)
   {
      /* 2 input pixels => 4 bytes (2Y, 1U, 1V). */

      for (x = 0; x < glyph_width; x += 2)
      {
	 unsigned alpha[2], alpha_sub, blended;
	 int out_x = x << 1;

	 alpha[0] = glyph[x + 0];
	 alpha[1] = 0;

	 if (x + 1 < glyph_width)
	    alpha[1] = glyph[x + 1];

	 /* Blended alpha for the sub-sampled U/V channels. */
	 alpha_sub = (alpha[0] + alpha[1]) >> 1;

	 for (i = 0; i < 2; i++)
	 {
	    unsigned blended = (xv->font_y * alpha[i]
				+ ((256 - alpha[i]) * out[out_x + luma_index[i]])) >> 8;
	    out[out_x + luma_index[i]] = blended;
	 }

	 /* Blend chroma channels */
	 blended = (xv->font_u * alpha_sub
		    + ((256 - alpha_sub) * out[out_x + chroma_u_index])) >> 8;
	 out[out_x + chroma_u_index] = blended;

	 blended = (xv->font_v * alpha_sub
		    + ((256 - alpha_sub) * out[out_x + chroma_v_index])) >> 8;
	 out[out_x + chroma_v_index] = blended;
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
      render16_yuy2,
      render32_yuy2,
      render_glyph_yuv_packed,
      { 'Y', 'U', 'Y', 'V' },
      { 0, 2 },
      1,
      3,
      16,
      XvPacked
   },
   {
      render16_uyvy,
      render32_uyvy,
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
               && format[i].bits_per_pixel == formats[j].bits
               && format[i].format == formats[j].format)
         {
            if (format[i].component_order[0] == formats[j].components[0] &&
                  format[i].component_order[1] == formats[j].components[1] &&
                  format[i].component_order[2] == formats[j].components[2] &&
                  format[i].component_order[3] == formats[j].components[3])
            {
               xv->fourcc         = format[i].id;
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
   settings_t *settings = config_get_ptr();
   bool scale_integer   = settings->bools.video_scale_integer;

   vp->full_width       = vp_width;
   vp->full_height      = vp_height;

   /* TODO: Does xvideo have its origin in top left or bottom-left? Assuming top left. */
   if (scale_integer)
      video_viewport_get_scaled_integer(vp, vp_width, vp_height,
           video_driver_get_aspect_ratio(), keep_aspect, true);
   else if (!keep_aspect)
   {
      vp->x      = 0;
      vp->y      = 0;
      vp->width  = vp_width;
      vp->height = vp_height;
   }
   else
   {
      video_viewport_get_scaled_aspect(vp, vp_width, vp_height, true);
   }
}

static void *xv_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   unsigned i;
   int ret;
   XWindowAttributes target;
   char title[128]                        = {0};
   XSetWindowAttributes attributes        = {0};
   XVisualInfo visualtemplate             = {0};
   unsigned width                         = 0;
   unsigned height                        = 0;
   unsigned adaptor_count                 = 0;
   int visualmatches                      = 0;
   Atom atom                              = 0;
   void *xinput                           = NULL;
   XVisualInfo *visualinfo                = NULL;
   XvAdaptorInfo *adaptor_info            = NULL;
   const struct retro_game_geometry *geom = NULL;
   video_driver_state_t *video_st         = video_state_get_ptr();
   struct retro_system_av_info *av_info   = &video_st->av_info;
   settings_t *settings                   = config_get_ptr();
   bool video_disable_composition         = settings->bools.video_disable_composition;
   xv_t                               *xv = (xv_t*)calloc(1, sizeof(*xv));
   if (!xv)
      return NULL;

   XInitThreads();

   g_x11_dpy = XOpenDisplay(NULL);

   if (!g_x11_dpy)
   {
      RARCH_ERR("[XVideo]: Cannot connect to the X server.\n");
      RARCH_ERR("[XVideo]: Check DISPLAY variable and if X is running.\n");
      goto error;
   }

   if (av_info)
      geom        = &av_info->geometry;

   if (!XShmQueryExtension(g_x11_dpy))
   {
      RARCH_ERR("[XVideo]: XShm extension not found.\n");
      goto error;
   }

   xv->keep_aspect = video->force_aspect;

   /* Find an appropriate Xv port. */
   xv->port = 0;
   ret = XvQueryAdaptors(g_x11_dpy,
         DefaultRootWindow(g_x11_dpy), &adaptor_count, &adaptor_info);

   if (ret != Success)
   {
      if (ret == XvBadExtension)
         RARCH_ERR("[XVideo]: Xv extension not found.\n");
      else if (ret == XvBadAlloc)
         RARCH_ERR("[XVideo]: XvQueryAdaptors() failed to allocate memory.\n");
      else
         RARCH_ERR("[XVideo]: Unknown error in XvQueryAdaptors().\n");

      goto error;
   }

   if (adaptor_count == 0)
   {
      RARCH_ERR("[XVideo]: XvQueryAdaptors() found 0 adaptors.\n");
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

      RARCH_LOG("[XVideo]: Found suitable XvPort #%u\n", (unsigned)xv->port);
      break;
   }
   XvFreeAdaptorInfo(adaptor_info);

   if (xv->port == 0)
   {
      RARCH_ERR("[XVideo]: Failed to find valid XvPort or format.\n");
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
      RARCH_ERR("[XVideo]: Unable to find Xv-compatible visual.\n");
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
      width      = (((video->width  == 0) && geom) ? geom->base_width : video->width);
      height     = (((video->height == 0) && geom) ? geom->base_height : video->height);
   }
   else
   {
      width      = video->width;
      height     = video->height;
   }
   g_x11_win  = XCreateWindow(g_x11_dpy, DefaultRootWindow(g_x11_dpy),
         0, 0, width, height,
         0, xv->depth, InputOutput, visualinfo->visual,
         CWColormap | CWBorderPixel | CWEventMask, &attributes);

   XFree(visualinfo);
   XSetWindowBackground(g_x11_dpy, g_x11_win, 0);

   if (video->fullscreen && video_disable_composition)
   {
      uint32_t value = 1;
      Atom cardinal = XInternAtom(g_x11_dpy, "CARDINAL", False);
      Atom net_wm_bypass_compositor = XInternAtom(g_x11_dpy,
            "_NET_WM_BYPASS_COMPOSITOR", False);

      RARCH_LOG("[XVideo]: Requesting compositor bypass.\n");
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
      x11_set_net_wm_fullscreen(g_x11_dpy, g_x11_win);
      x11_show_mouse(xv, false);
   }

   xv->gc = XCreateGC(g_x11_dpy, g_x11_win, 0, 0);

   /* Set colorkey to auto paint, so that Xv video output is always visible. */
   atom = XInternAtom(g_x11_dpy, "XV_AUTOPAINT_COLORKEY", true);
   if (atom != None)
      XvSetPortAttribute(g_x11_dpy, xv->port, atom, 1);

   if (geom)
   {
      xv->width  = geom->max_width;
      xv->height = geom->max_height;
   }

   xv->image = XvShmCreateImage(g_x11_dpy, xv->port, xv->fourcc,
         NULL, xv->width, xv->height, &xv->shminfo);

   if (!xv->image)
   {
      RARCH_ERR("[XVideo]: XShmCreateImage failed.\n");
      goto error;
   }

   xv->width            = xv->image->width;
   xv->height           = xv->image->height;
   xv->shminfo.shmid    = shmget(IPC_PRIVATE, xv->image->data_size, IPC_CREAT | 0777);
   xv->shminfo.shmaddr  = xv->image->data = (char*)shmat(xv->shminfo.shmid, NULL, 0);
   xv->shminfo.readOnly = false;

   if (!XShmAttach(g_x11_dpy, &xv->shminfo))
   {
      RARCH_ERR("[XVideo]: XShmAttach failed.\n");
      goto error;
   }
   XSync(g_x11_dpy, False);
   memset(xv->image->data, 128, xv->image->data_size);

   x11_install_quit_atom();

   frontend_driver_install_signal_handler();

   xv_init_yuv_tables(xv);
   xv_init_font(xv, settings->paths.path_font, settings->floats.video_font_size);

   if (!x11_input_ctx_new(true))
      goto error;

   if (input && input_data)
   {
      xinput = input_driver_init_wrap(&input_x,
            settings->arrays.input_joypad_driver);
      if (xinput)
      {
         *input = &input_x;
         *input_data = xinput;
      }
      else
         *input = NULL;
   }

   XGetWindowAttributes(g_x11_dpy, g_x11_win, &target);
   xv_calc_out_rect(xv->keep_aspect, &xv->vp, target.width, target.height);
   xv->vp.full_width = target.width;
   xv->vp.full_height = target.height;

   return xv;

error:
   if (visualinfo)
      XFree(visualinfo);
   free(xv);
   return NULL;
}

static bool xv_check_resize(xv_t *xv, unsigned width, unsigned height)
{
   /* We render @ 2x scale to combat chroma downsampling. */
   if (xv->width != (width << 1) || xv->height != (height << 1))
   {
      xv->width  = width << 1;
      xv->height = height << 1;

      XShmDetach(g_x11_dpy, &xv->shminfo);
      shmdt(xv->shminfo.shmaddr);
      shmctl(xv->shminfo.shmid, IPC_RMID, NULL);
      XFree(xv->image);

      memset(&xv->shminfo, 0, sizeof(xv->shminfo));
      xv->image = XvShmCreateImage(g_x11_dpy, xv->port, xv->fourcc,
            NULL, xv->width, xv->height, &xv->shminfo);

      if (xv->image == None)
      {
         RARCH_ERR("[XVideo]: Failed to create image.\n");
         return false;
      }

      xv->width  = xv->image->width;
      xv->height = xv->image->height;

      xv->shminfo.shmid =
         shmget(IPC_PRIVATE, xv->image->data_size, IPC_CREAT | 0777);

      if (xv->shminfo.shmid < 0)
      {
         RARCH_ERR("[XVideo]: Failed to init SHM.\n");
         return false;
      }

      xv->shminfo.shmaddr  = xv->image->data =
         (char*)shmat(xv->shminfo.shmid, NULL, 0);
      xv->shminfo.readOnly = false;

      if (!XShmAttach(g_x11_dpy, &xv->shminfo))
      {
         RARCH_ERR("[XVideo]: Failed to reattch XvShm image.\n");
         return false;
      }
      XSync(g_x11_dpy, False);
      memset(xv->image->data, 128, xv->image->data_size);
   }
   return true;
}

/* TODO: Is there some way to render directly like GL?
 * Hacky C code is hacky. */
static void xv_render_msg(xv_t *xv, const char *msg,
      unsigned width, unsigned height)
{
   int msg_base_x, msg_base_y;
   const struct font_atlas *atlas = NULL;
   settings_t           *settings = config_get_ptr();
   float video_msg_pos_x          = settings->floats.video_msg_pos_x;
   float video_msg_pos_y          = settings->floats.video_msg_pos_y;

   if (!xv->font)
      return;

   atlas          = xv->font_driver->get_atlas(xv->font);

   msg_base_x     = video_msg_pos_x * width;
   msg_base_y     = height * (1.0f - video_msg_pos_y);

   for (; *msg; msg++)
   {
      int base_x, base_y, glyph_width, glyph_height, max_width, max_height;
      const uint8_t *src             = NULL;
      const struct font_glyph *glyph =
         xv->font_driver->get_glyph(xv->font, (uint8_t)*msg);

      if (!glyph)
         continue;

      /* Make sure we always start on the correct boundary
       * so the indices are correct. */
      base_x          = (msg_base_x + glyph->draw_offset_x + 1) & ~1;
      base_y          = msg_base_y + glyph->draw_offset_y;

      glyph_width     = glyph->width;
      glyph_height    = glyph->height;

      src             = atlas->buffer + glyph->atlas_offset_x +
                        glyph->atlas_offset_y * atlas->width;

      if (base_x < 0)
      {
         src          -= base_x;
         glyph_width  += base_x;
         base_x = 0;
      }

      if (base_y < 0)
      {
         src          -= base_y * (int)atlas->width;
         glyph_height += base_y;
         base_y = 0;
      }

      max_width        = width - base_x;
      max_height       = height - base_y;

      if (max_width <= 0 || max_height <= 0)
         continue;

      if (glyph_width > max_width)
         glyph_width   = max_width;
      if (glyph_height > max_height)
         glyph_height  = max_height;

      xv->render_glyph(xv, base_x, base_y, src, atlas->width, glyph_width, glyph_height);

      msg_base_x += glyph->advance_x;
      msg_base_y += glyph->advance_y;
   }
}

static bool xv_frame(void *data, const void *frame, unsigned width,
      unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   XWindowAttributes target;
   xv_t *xv                  = (xv_t*)data;
   bool rgb32                = (video_info->video_st_flags & VIDEO_FLAG_USE_RGBA) ? true : false;
#ifdef HAVE_MENU
   bool menu_is_alive        = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;

   menu_driver_frame(menu_is_alive, video_info);

   if (menu_is_alive && xv->tex_frame)
   {
      frame  = xv->tex_frame;
      width  = xv->tex_width;
      height = xv->tex_height;
      pitch  = xv->tex_pitch;
      rgb32  = xv->tex_rgb32;
   }
#endif

   if (!frame)
      return true;

   if (!xv_check_resize(xv, width, height))
      return false;

   XGetWindowAttributes(g_x11_dpy, g_x11_win, &target);

   if (rgb32)
      xv->render_func32(xv, frame, width, height, pitch);
   else
      xv->render_func16(xv, frame, width, height, pitch);

   xv_calc_out_rect(xv->keep_aspect, &xv->vp, target.width, target.height);
   xv->vp.full_width  = target.width;
   xv->vp.full_height = target.height;

   if (msg)
      xv_render_msg(xv, msg, width << 1, height << 1);

   XvShmPutImage(g_x11_dpy, xv->port, g_x11_win, xv->gc, xv->image,
         0, 0, width << 1, height << 1,
         xv->vp.x, xv->vp.y, xv->vp.width, xv->vp.height,
         true);
   XSync(g_x11_dpy, False);

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

   XShmDetach(g_x11_dpy, &xv->shminfo);
   shmdt(xv->shminfo.shmaddr);
   shmctl(xv->shminfo.shmid, IPC_RMID, NULL);
   XFree(xv->image);

   x11_window_destroy(true);
   x11_colormap_destroy();

   XCloseDisplay(g_x11_dpy);

   free(xv->ytable);
   free(xv->utable);
   free(xv->vtable);

   if (xv->font)
      xv->font_driver->free(xv->font);

   free(xv);
}

static void xv_viewport_info(void *data, struct video_viewport *vp)
{
   xv_t *xv = (xv_t*)data;
   *vp = xv->vp;
}

static uint32_t xv_poke_get_flags(void *data)
{
   return 0;
}

static void xv_poke_set_texture_frame(void *data,
      const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   xv_t *xv  = (xv_t*)data;
   xv->tex_frame = (void*)frame;
   xv->tex_rgb32 = rgb32;
   xv->tex_width = width;
   xv->tex_height = height;
   xv->tex_pitch = width * (rgb32 ? 4 : 2);
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
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_max_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_contrast */
   NULL  /* set_hdr_expand_gamut */
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
   NULL, /* read_viewport */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL, /* get_overlay_interface */
#endif
   xv_get_poke_interface,
   NULL, /* wrap_type_to_enum */
#ifdef HAVE_GFX_WIDGETS
   NULL  /* gfx_widgets_enabled */
#endif
};

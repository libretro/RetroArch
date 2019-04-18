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

#define CINTERFACE

#include <tchar.h>

#include <compat/strl.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../common/d3d_common.h"
#include "../common/d3d9_common.h"
#include "../font_driver.h"

#include "../../configuration.h"

typedef struct d3dx_font_desc
{
    INT Height;
    UINT Width;
    UINT Weight;
    UINT MipLevels;
    BOOL Italic;
    BYTE CharSet;
    BYTE OutputPrecision;
    BYTE Quality;
    BYTE PitchAndFamily;
    CHAR FaceName[32];
} d3dx_font_desc_t;

typedef struct
{
   d3d9_video_t *d3d;
   void *font;
   uint32_t font_size;
   uint32_t ascent;
} d3dfonts_t;

static void *d3dfonts_w32_init_font(void *video_data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   TEXTMETRICA metrics;
   d3dx_font_desc_t desc;
   d3dfonts_t *d3dfonts = (d3dfonts_t*)calloc(1, sizeof(*d3dfonts));
   if (!d3dfonts)
	   return NULL;

   desc.Height          = (int)font_size;
   desc.Width           = 0;
   desc.Weight          = 400;
   desc.MipLevels       = 0;
   desc.Italic          = FALSE;
   desc.CharSet         = DEFAULT_CHARSET;
   desc.OutputPrecision = OUT_TT_PRECIS;
   desc.Quality         = CLIP_DEFAULT_PRECIS;
   desc.PitchAndFamily  = DEFAULT_PITCH;
#ifdef UNICODE
   strlcpy(desc.FaceName, T(L"Verdana"), sizeof(desc.FaceName));
#else
   strlcpy(desc.FaceName, (const char*)_T("Verdana"), sizeof(desc.FaceName));
#endif

   d3dfonts->font_size  = font_size * 1.2; /* to match the other font drivers */
   d3dfonts->d3d        = (d3d9_video_t*)video_data;

   desc.Height          = d3dfonts->font_size;

   if (!d3d9x_create_font_indirect(d3dfonts->d3d->dev,
            &desc, (void**)&d3dfonts->font))
      goto error;

   d3d9x_font_get_text_metrics(d3dfonts->font, &metrics);

   d3dfonts->ascent     = metrics.tmAscent;

   return d3dfonts;

error:
   free(d3dfonts);
   return NULL;
}

static void d3dfonts_w32_free_font(void *data, bool is_threaded)
{
   d3dfonts_t *d3dfonts = (d3dfonts_t*)data;

   if (!d3dfonts)
      return;

   if (d3dfonts->font)
      d3d9x_font_release(d3dfonts->font);

   free(d3dfonts);
}

static int d3dfonts_w32_get_message_width(void* data, const char* msg,
      unsigned msg_len, float scale)
{
   RECT box             = {0,0,0,0};
   d3dfonts_t *d3dfonts = (d3dfonts_t*)data;

   if (!d3dfonts || !msg)
      return 0;

   d3d9x_font_draw_text(d3dfonts->font, NULL, (void*)msg,
         msg_len? msg_len : -1, &box, DT_CALCRECT, 0);

   return box.right - box.left;
}

static void d3dfonts_w32_render_msg(video_frame_info_t *video_info,
      void *data, const char *msg, const struct font_params *params)
{
   unsigned format;
   unsigned a, r, g, b;
   RECT rect, rect_shifted;
   RECT *p_rect_shifted             = NULL;
   RECT *p_rect                     = NULL;
   d3dfonts_t *d3dfonts             = (d3dfonts_t*)data;
   unsigned width                   = video_info->width;
   unsigned height                  = video_info->height;
   float drop_mod                   = 0.3f;
   float drop_alpha                 = 1.0f;
   int drop_x                       = -2;
   int drop_y                       = -2;

   if (!d3dfonts || !d3dfonts->d3d || !msg)
      return;

   format         = DT_LEFT;
   p_rect         = &d3dfonts->d3d->font_rect;
   p_rect_shifted = &d3dfonts->d3d->font_rect_shifted;

   if(params)
   {
      a = FONT_COLOR_GET_ALPHA(params->color);
      r = FONT_COLOR_GET_RED(params->color);
      g = FONT_COLOR_GET_GREEN(params->color);
      b = FONT_COLOR_GET_BLUE(params->color);

      switch (params->text_align)
      {
         case TEXT_ALIGN_RIGHT:
            format     = DT_RIGHT;
            rect.left  = 0;
            rect.right = params->x * width;
            break;
         case TEXT_ALIGN_CENTER:
            format     = DT_CENTER;
            rect.left  = (params->x - 1.0) * width;
            rect.right = (params->x + 1.0) * width;
            break;
         case TEXT_ALIGN_LEFT:
         default:
            format     = DT_LEFT;
            rect.left  = params->x * width;
            rect.right = width;
            break;
      }

      rect.top    = (1.0 - params->y) * height - d3dfonts->ascent;
      rect.bottom = height;
      p_rect      = &rect;

      drop_x      = params->drop_x;
      drop_y      = params->drop_y;

      if(drop_x || drop_y)
      {
         drop_mod             = params->drop_mod;
         drop_alpha           = params->drop_alpha;
         rect_shifted         = rect;
         rect_shifted.left   += params->drop_x;
         rect_shifted.right  += params->drop_x;
         rect_shifted.top    -= params->drop_y;
         rect_shifted.bottom -= params->drop_y;
         p_rect_shifted       = &rect_shifted;
      }
   }
   else
   {
      a = 255;
      r = video_info->font_msg_color_r * 255;
      g = video_info->font_msg_color_g * 255;
      b = video_info->font_msg_color_b * 255;
   }

   if(drop_x || drop_y)
   {
      unsigned drop_a = a * drop_alpha;
      unsigned drop_r = r * drop_mod;
      unsigned drop_g = g * drop_mod;
      unsigned drop_b = b * drop_mod;

      d3d9x_font_draw_text(d3dfonts->font, NULL,
            (void*)msg, -1, p_rect_shifted, format,
            D3DCOLOR_ARGB(drop_a , drop_r, drop_g, drop_b));
   }

   d3d9x_font_draw_text(d3dfonts->font, NULL, (void*)msg, -1,
      p_rect, format, D3DCOLOR_ARGB(a, r, g, b));
}

font_renderer_t d3d_win32_font = {
   d3dfonts_w32_init_font,
   d3dfonts_w32_free_font,
   d3dfonts_w32_render_msg,
   "d3dxfont",
   NULL,                      /* get_glyph */
   NULL,                      /* bind_block */
   NULL,                      /* flush */
   d3dfonts_w32_get_message_width
};

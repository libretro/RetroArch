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

#ifdef HAVE_D3DX

#include <d3dx9core.h>
#include <d3dx9tex.h>

#include "../common/d3d_common.h"
#include "../common/d3d9_common.h"
#include "../font_driver.h"

#include "../../configuration.h"

typedef struct d3d9x_font_desc
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
} d3d9x_font_desc_t;

typedef struct
{
   d3d9_video_t *d3d;
   void *font;
   uint32_t font_size;
   uint32_t ascent;
} d3dfonts_t;

static void *d3d9x_win32_font_init(void *video_data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   TEXTMETRICA metrics;
   d3d9x_font_desc_t desc;
   uint32_t new_font_size = 0;
   ID3DXFont *font        = NULL;
   d3dfonts_t *d3dfonts   = (d3dfonts_t*)calloc(1, sizeof(*d3dfonts));
   if (!d3dfonts)
	   return NULL;

   desc.Height            = (int)font_size;
   desc.Width             = 0;
   desc.Weight            = 400;
   desc.MipLevels         = 0;
   desc.Italic            = FALSE;
   desc.CharSet           = DEFAULT_CHARSET;
   desc.OutputPrecision   = OUT_TT_PRECIS;
   desc.Quality           = CLIP_DEFAULT_PRECIS;
   desc.PitchAndFamily    = DEFAULT_PITCH;

   /* TODO/FIXME - don't hardcode this font */
#ifdef UNICODE
   strlcpy(desc.FaceName, T(L"Verdana"), sizeof(desc.FaceName));
#else
   strlcpy(desc.FaceName, (const char*)_T("Verdana"), sizeof(desc.FaceName));
#endif

   new_font_size          = font_size * 1.2; /* To match the other font drivers */
   d3dfonts->font_size    = new_font_size;
   d3dfonts->d3d          = (d3d9_video_t*)video_data;

   desc.Height            = new_font_size;

   if (!d3d9x_create_font_indirect(d3dfonts->d3d->dev,
            &desc, (void**)&d3dfonts->font))
      goto error;

   font                   = (ID3DXFont*)d3dfonts->font;

   font->lpVtbl->GetTextMetrics(font, &metrics);

   d3dfonts->ascent       = metrics.tmAscent;

   return d3dfonts;

error:
   free(d3dfonts);
   return NULL;
}

static void d3d9x_win32_font_free(void *data, bool is_threaded)
{
   ID3DXFont *font      = NULL;
   d3dfonts_t *d3dfonts = (d3dfonts_t*)data;

   if (!d3dfonts)
      return;

   font                 = (ID3DXFont*)d3dfonts->font;

   if (font)
      font->lpVtbl->Release(font);

   free(d3dfonts);
}

static int d3d9x_win32_font_get_message_width(void* data, const char* msg,
      size_t msg_len, float scale)
{
   RECT box             = {0,0,0,0};
   d3dfonts_t *d3dfonts = (d3dfonts_t*)data;
   ID3DXFont *font      = NULL;

   if (!d3dfonts || !msg)
      return 0;

   font                 = (ID3DXFont*)d3dfonts->font;

   font->lpVtbl->DrawText(font, NULL,
         (void*)msg, msg_len ? (INT)msg_len : -1, &box, DT_CALCRECT, 0);

   return box.right - box.left;
}

static void d3d9x_win32_font_render_msg(
      void *userdata,
      void *data, const char *msg,
      const struct font_params *params)
{
   DWORD format;
   unsigned a, r, g, b;
   unsigned width, height;
   RECT rect, rect_shifted;
   RECT *p_rect_shifted             = NULL;
   RECT *p_rect                     = NULL;
   ID3DXFont *font                  = NULL;
   d3dfonts_t *d3dfonts             = (d3dfonts_t*)data;
   float drop_mod                   = 0.3f;
   float drop_alpha                 = 1.0f;
   int drop_x                       = -2;
   int drop_y                       = -2;

   if (!d3dfonts || !msg)
      return;

   font                             = (ID3DXFont*)d3dfonts->font;

   width                            = d3dfonts->d3d->video_info.width;
   height                           = d3dfonts->d3d->video_info.height;

   p_rect                           = &d3dfonts->d3d->font_rect;
   p_rect_shifted                   = &d3dfonts->d3d->font_rect_shifted;
   format                           = DT_LEFT;

   if (params)
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

      if (drop_x || drop_y)
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
      settings_t *settings     = config_get_ptr();
      float video_msg_pos_x    = settings->floats.video_msg_pos_x;
      float video_msg_pos_y    = settings->floats.video_msg_pos_y;
      float video_msg_color_r  = settings->floats.video_msg_color_r;
      float video_msg_color_g  = settings->floats.video_msg_color_g;
      float video_msg_color_b  = settings->floats.video_msg_color_b;
      a                        = 255;
      r                        = video_msg_color_r * 255;
      g                        = video_msg_color_g * 255;
      b                        = video_msg_color_b * 255;
   }

   if (drop_x || drop_y)
   {
      unsigned drop_a = a * drop_alpha;
      unsigned drop_r = r * drop_mod;
      unsigned drop_g = g * drop_mod;
      unsigned drop_b = b * drop_mod;

      font->lpVtbl->DrawText(font, NULL,
            (void*)msg, -1, p_rect_shifted, format,
            D3DCOLOR_ARGB(drop_a , drop_r, drop_g, drop_b));
   }

   font->lpVtbl->DrawText(font, NULL,
         (void*)msg, -1, p_rect, format,
         D3DCOLOR_ARGB(a, r, g, b));
}

font_renderer_t d3d9x_win32_font = {
   d3d9x_win32_font_init,
   d3d9x_win32_font_free,
   d3d9x_win32_font_render_msg,
   "d3d9x",
   NULL,                      /* get_glyph */
   NULL,                      /* bind_block */
   NULL,                      /* flush */
   d3d9x_win32_font_get_message_width,
   NULL                       /* get_line_metrics */
};

#endif

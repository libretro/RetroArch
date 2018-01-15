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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../drivers/d3d.h"
#include "../font_driver.h"

#include "../../configuration.h"

#if defined(HAVE_D3D9)
#include "../include/d3d9/d3dx9core.h"
#elif defined(HAVE_D3D8)
#include "../include/d3d8/d3dx8core.h"
#endif

#include <tchar.h>

typedef struct
{
   d3d_video_t *d3d;
#ifdef __cplusplus
   LPD3DXFONT font;
#else
   ID3DXFont *font;
#endif
   uint32_t font_size;
   uint32_t ascent;
} d3dfonts_t;

#ifdef __cplusplus
#else
#endif

#if !defined(__cplusplus) || defined(CINTERFACE)
#define IDirect3DXFont_DrawTextA(p, a, b, c, d, e, f) (p)->lpVtbl->DrawTextA(p, a, b, c, d, e, f)
#define IDirect3DXFont_GetTextMetricsA(p, a) (p)->lpVtbl->GetTextMetricsA(p, a)
#define IDirect3DXFont_Release(p) (p)->lpVtbl->Release(p)
#else
#define IDirect3DXFont_DrawTextA(p, a, b, c, d, e, f) (p)->DrawTextA(a, b, c, d, e, f)
#define IDirect3DXFont_GetTextMetricsA(p, a) (p)->GetTextMetricsA(a)
#define IDirect3DXFont_Release(p) (p)->Release()
#endif

static void *d3dfonts_w32_init_font(void *video_data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   TEXTMETRICA metrics;
   settings_t *settings = config_get_ptr();
   D3DXFONT_DESC desc = {
      (int)(font_size), 0, 400, 0,
      false, DEFAULT_CHARSET,
      OUT_TT_PRECIS,
      CLIP_DEFAULT_PRECIS,
      DEFAULT_PITCH,
#ifdef UNICODE
	  _T(L"Verdana") /* Hardcode FTL */
#else
      _T("Verdana") /* Hardcode FTL */
#endif
   };
   d3dfonts_t *d3dfonts = (d3dfonts_t*)calloc(1, sizeof(*d3dfonts));
   if (!d3dfonts)
      return NULL;

   d3dfonts->font_size = font_size * 1.2; /* to match the other font drivers */
   d3dfonts->d3d       = (d3d_video_t*)video_data;

   desc.Height = d3dfonts->font_size;
   if (!d3dx_create_font_indirect(d3dfonts->d3d->dev,
            &desc, (void**)&d3dfonts->font))
      goto error;

   IDirect3DXFont_GetTextMetricsA(d3dfonts->font, &metrics);
   d3dfonts->ascent = metrics.tmAscent;

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
      IDirect3DXFont_Release(d3dfonts->font);

   free(d3dfonts);
}


static int d3dfonts_w32_get_message_width(void* data, const char* msg,
      unsigned msg_len, float scale)
{
   RECT box             = {0,0,0,0};
   d3dfonts_t *d3dfonts = (d3dfonts_t*)data;

   if (!d3dfonts || !d3dfonts->d3d | !msg)
      return 0;

   IDirect3DXFont_DrawTextA(d3dfonts->font, NULL, msg, msg_len? msg_len : -1, &box, DT_CALCRECT, 0);

   return box.right - box.left;
}


static void d3dfonts_w32_render_msg(video_frame_info_t *video_info, void *data, const char *msg,
      const void *userdata)
{
   unsigned format;
   unsigned a, r, g, b;
   RECT rect, *p_rect;
   RECT rect_shifted, *p_rect_shifted;
   settings_t *settings             = config_get_ptr();
   const struct font_params *params = (const struct font_params*)userdata;
   d3dfonts_t *d3dfonts             = (d3dfonts_t*)data;
   unsigned width                   = video_info->width;
   unsigned height                  = video_info->height;
   float drop_mod                   = 0.3f;
   float drop_alpha                 = 1.0f;
   int drop_x                       = -2;
   int drop_y                       = -2;

   if (!d3dfonts || !d3dfonts->d3d)
      return;
   if (!msg)
      return;

   if (!d3d_begin_scene(d3dfonts->d3d->dev))
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
      unsigned drop_a, drop_r, drop_g, drop_b;

      drop_a = a * drop_alpha;
      drop_r = r * drop_mod;
      drop_g = g * drop_mod;
      drop_b = b * drop_mod;

      IDirect3DXFont_DrawTextA(d3dfonts->font, NULL, msg, -1, p_rect_shifted, format,
         D3DCOLOR_ARGB(drop_a , drop_r, drop_g, drop_b));
   }


   IDirect3DXFont_DrawTextA(d3dfonts->font, NULL, msg, -1,
      p_rect, format, D3DCOLOR_ARGB(a, r, g, b));

   d3d_end_scene(d3dfonts->d3d->dev);
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

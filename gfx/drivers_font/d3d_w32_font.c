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
   uint32_t color;
} d3dfonts_t;

#ifdef __cplusplus
#else
#endif

static void *d3dfonts_w32_init_font(void *video_data,
      const char *font_path, float font_size,
      bool is_threaded)
{
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
   uint32_t r           = (settings->floats.video_msg_color_r * 255);
   uint32_t g           = (settings->floats.video_msg_color_g * 255);
   uint32_t b           = (settings->floats.video_msg_color_b * 255);
   r &= 0xff;
   g &= 0xff;
   b &= 0xff;

   if (!d3dfonts)
      return NULL;

   d3dfonts->d3d   = (d3d_video_t*)video_data;
   d3dfonts->color = D3DCOLOR_XRGB(r, g, b);

   if (!d3dx_create_font_indirect(d3dfonts->d3d->dev,
            &desc, (void**)&d3dfonts->font))
      goto error;

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

#ifdef __cplusplus
   if (d3dfonts->font)
      d3dfonts->font->Release();
#else
   if (d3dfonts->font)
      d3dfonts->font->lpVtbl->Release(d3dfonts->font);
#endif
   d3dfonts->font = NULL;

   free(d3dfonts);
   d3dfonts = NULL;
}

static void d3dfonts_w32_render_msg(video_frame_info_t *video_info, void *data, const char *msg,
      const void *userdata)
{
   const struct font_params *params = (const struct font_params*)userdata;
   d3dfonts_t *d3dfonts             = (d3dfonts_t*)data;

   if (!d3dfonts || !d3dfonts->d3d)
      return;
   if (!msg)
      return;
   d3d_set_viewports(d3dfonts->d3d->dev, &d3dfonts->d3d->final_viewport);
   if (!d3d_begin_scene(d3dfonts->d3d->dev))
      return;

#ifdef __cplusplus
   d3dfonts->font->DrawTextA(NULL,
         msg,
         -1,
         &d3dfonts->d3d->font_rect_shifted,
         DT_LEFT,
         ((d3dfonts->color >> 2) & 0x3f3f3f) | 0xff000000);

   d3dfonts->font->DrawTextA(NULL,
         msg,
         -1,
         &d3dfonts->d3d->font_rect,
         DT_LEFT,
         d3dfonts->color | 0xff000000);
#else
   d3dfonts->font->lpVtbl->DrawTextA(
         d3dfonts->font,
         NULL,
         msg,
         -1,
         &d3dfonts->d3d->font_rect_shifted,
         DT_LEFT,
         ((d3dfonts->color >> 2) & 0x3f3f3f) | 0xff000000);

   d3dfonts->font->lpVtbl->DrawTextA(
         d3dfonts->font,
         NULL,
         msg,
         -1,
         &d3dfonts->d3d->font_rect,
         DT_LEFT,
         d3dfonts->color | 0xff000000);
#endif

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
   NULL                       /* get_message_width */
};

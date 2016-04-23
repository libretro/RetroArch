/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include "../d3d/d3d.h"
#include "../font_driver.h"
#include "../../general.h"

#include "../include/d3d9/d3dx9core.h"

typedef struct
{
   d3d_video_t *d3d;
   LPD3DXFONT font;
   uint32_t color;
} d3dfonts_t;

static void *d3dfonts_w32_init_font(void *video_data,
      const char *font_path, float font_size)
{
   uint32_t r, g, b;
   d3dfonts_t *d3dfonts = NULL;
   settings_t *settings = config_get_ptr();
   D3DXFONT_DESC desc = {
      (int)(font_size), 0, 400, 0,
      false, DEFAULT_CHARSET,
      OUT_TT_PRECIS,
      CLIP_DEFAULT_PRECIS,
      DEFAULT_PITCH,
      "Verdana" /* Hardcode FTL */
   };

   d3dfonts = (d3dfonts_t*)calloc(1, sizeof(*d3dfonts));

   if (!d3dfonts)
      return NULL;

   (void)font_path;

   r               = (settings->video.msg_color_r * 255);
   g               = (settings->video.msg_color_g * 255);
   b               = (settings->video.msg_color_b * 255);
   r &= 0xff;
   g &= 0xff;
   b &= 0xff;

   d3dfonts->d3d   = (d3d_video_t*)video_data;
   d3dfonts->color = D3DCOLOR_XRGB(r, g, b);

   if (SUCCEEDED(D3DXCreateFontIndirect(
               d3dfonts->d3d->dev, &desc, &d3dfonts->font)))
      return d3dfonts;

   free(d3dfonts);
   return NULL;
}

static void d3dfonts_w32_free_font(void *data)
{
   d3dfonts_t *d3dfonts = (d3dfonts_t*)data;

   if (!d3dfonts)
      return;

   if (d3dfonts->font)
      d3dfonts->font->Release();
   d3dfonts->font = NULL;

   free(d3dfonts);
   d3dfonts = NULL;
}

static void d3dfonts_w32_render_msg(void *data, const char *msg,
      const void *userdata)
{
   const struct font_params *params = (const struct font_params*)userdata;
   d3dfonts_t *d3dfonts             = (d3dfonts_t*)data;

   if (!d3dfonts || !d3dfonts->d3d)
      return;
   if (!msg)
      return;
   if (!(SUCCEEDED(d3dfonts->d3d->dev->BeginScene())))
      return;

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

   d3dfonts->d3d->dev->EndScene();
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

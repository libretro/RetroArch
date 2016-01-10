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

#include <xtl.h>

#include "../font_driver.h"
#include "../../general.h"

typedef struct
{
   d3d_video_t *d3d;
   XFONT *debug_font;
   D3DSurface *surf;
} xfonts_t;

static void *xfonts_init_font(void *video_data,
      const char *font_path, float font_size)
{
   xfonts_t *xfont = (xfonts_t*)calloc(1, sizeof(*xfont));

   if (!xfont)
      return NULL;

   (void)font_path;
   (void)font_size;

   xfont->d3d = (d3d_video_t*)video_data;

   XFONT_OpenDefaultFont(&xfont->debug_font);
   xfont->debug_font->SetBkMode(XFONT_TRANSPARENT);
   xfont->debug_font->SetBkColor(D3DCOLOR_ARGB(100,0,0,0));
   xfont->debug_font->SetTextHeight(14);
   xfont->debug_font->SetTextAntialiasLevel(xfont->debug_font->GetTextAntialiasLevel());

   return xfont;
}

static void xfonts_free_font(void *data)
{
   xfonts_t *font = (xfonts_t*)data;

   if (font)
      free(font);
   
   font = NULL;
}

static void xfonts_render_msg(void *data, const char *msg,
      const void *userdata)
{
   wchar_t str[PATH_MAX_LENGTH];
   float x, y;
   settings_t *settings = config_get_ptr();
   const struct font_params *params = (const struct font_params*)userdata;
   xfonts_t *xfonts = (xfonts_t*)data;

   if (params)
   {
      x = params->x;
      y = params->y;
   }
   else
   {
      x = settings->video.msg_pos_x;
      y = settings->video.msg_pos_y;
   }

   xfonts->d3d->dev->GetBackBuffer(-1, D3DBACKBUFFER_TYPE_MONO, &xfonts->surf);

   mbstowcs(str, msg, sizeof(str) / sizeof(wchar_t));
   xfonts->debug_font->TextOut(xfonts->surf, str, (unsigned)-1, x, y);
   xfonts->surf->Release();
}

font_renderer_t d3d_xdk1_font = {
   xfonts_init_font,
   xfonts_free_font,
   xfonts_render_msg,
   "xfonts",
   NULL,                      /* get_glyph */
   NULL,                      /* bind_block */
   NULL,                      /* flush */
   NULL                       /* get_message_width */
};

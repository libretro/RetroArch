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

#include <xtl.h>
#include <xfont.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../common/d3d_common.h"
#include "../common/d3d8_common.h"

#include "../font_driver.h"

typedef struct
{
   d3d8_video_t *d3d;
   XFONT *debug_font;
   D3DSurface *surf;
} xfonts_t;

static void *xfonts_init_font(void *video_data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   xfonts_t *xfont = (xfonts_t*)calloc(1, sizeof(*xfont));

   if (!xfont)
      return NULL;

   (void)font_path;
   (void)font_size;

   xfont->d3d = (d3d8_video_t*)video_data;

   XFONT_OpenDefaultFont(&xfont->debug_font);

#ifdef __cplusplus
   xfont->debug_font->SetBkMode(XFONT_TRANSPARENT);
   xfont->debug_font->SetBkColor(D3DCOLOR_ARGB(100,0,0,0));
   xfont->debug_font->SetTextHeight(14);
   xfont->debug_font->SetTextAntialiasLevel(
         xfont->debug_font->GetTextAntialiasLevel());
#else
   XFONT_SetBkMode(xfont->debug_font, XFONT_TRANSPARENT);
   XFONT_SetBkColor(xfont->debug_font, D3DCOLOR_ARGB(100,0,0,0));
   XFONT_SetTextHeight(xfont->debug_font, 14);
   XFONT_SetTextAntialiasLevel(xfont->debug_font,
         XFONT_GetTextAntialiasLevel(xfont->debug_font));
#endif

   return xfont;
}

static void xfonts_free_font(void *data, bool is_threaded)
{
   xfonts_t *font = (xfonts_t*)data;

   if (font)
      free(font);

   font = NULL;
}

static void xfonts_render_msg(
      video_frame_info_t *video_info,
      void *data, const char *msg,
      const struct font_params *params)
{
   wchar_t str[PATH_MAX_LENGTH];
   float x, y;
   xfonts_t *xfonts                 = (xfonts_t*)data;

   if (params)
   {
      x = params->x;
      y = params->y;
   }
   else
   {
      x = video_info->font_msg_pos_x;
      y = video_info->font_msg_pos_y;
   }

   d3d8_device_get_backbuffer(xfonts->d3d->dev,
         -1, 0, D3DBACKBUFFER_TYPE_MONO, &xfonts->surf);

   mbstowcs(str, msg, sizeof(str) / sizeof(wchar_t));

#ifdef __cplusplus
   xfonts->debug_font->TextOut(xfonts->surf, str, (unsigned)-1, x, y);
#else
   XFONT_TextOut(xfonts->debug_font, xfonts->surf, str, (unsigned)-1, x, y);
#endif
   d3d8_surface_free(xfonts->surf);
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

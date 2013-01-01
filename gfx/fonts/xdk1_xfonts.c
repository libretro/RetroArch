/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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
#include "d3d_font.h"
#include "../../general.h"

static XFONT *debug_font;
static D3DSurface *pFrontBuffer;

static bool xfonts_init_font(void *data, const char *font_path, unsigned font_size)
{
   (void)font_path;
   (void)font_size;

   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;

   XFONT_OpenDefaultFont(&debug_font);
   debug_font->SetBkMode(XFONT_TRANSPARENT);
   debug_font->SetBkColor(D3DCOLOR_ARGB(100,0,0,0));
   debug_font->SetTextHeight(14);
   debug_font->SetTextAntialiasLevel(debug_font->GetTextAntialiasLevel());

   return true;
}

static void xfonts_deinit_font(void *data)
{
   (void)data;
}

static void xfonts_render_msg_place(void *data, float x, float y, float scale, uint32_t color, const char *msg)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;

   d3d->d3d_render_device->GetBackBuffer(-1, D3DBACKBUFFER_TYPE_MONO, &pFrontBuffer);

   wchar_t str[256];
   convert_char_to_wchar(str, msg, sizeof(str));
   debug_font->TextOut(pFrontBuffer, str, (unsigned)-1, x, y);

   pFrontBuffer->Release();
}

static void xfonts_render_msg(void *data, const char *msg)
{
   xfonts_render_msg_place(data, g_settings.video.msg_pos_x, g_settings.video.msg_pos_y, 0, 0, msg);
}

const d3d_font_renderer_t d3d_xdk1_font = {
   xfonts_init_font,
   xfonts_deinit_font,
   xfonts_render_msg,
   xfonts_render_msg_place,
   "XDK1 Xfonts",
};

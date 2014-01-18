/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include "d3d_font.h"
#include "../gfx_common.h"
#include "../../general.h"

static bool d3dfonts_w32_init_font(void *data, const char *font_path, unsigned font_size)
{
   (void)font_path;

   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   D3DXFONT_DESC desc = {
      static_cast<int>(font_size), 0, 400, 0,
      false, DEFAULT_CHARSET,
      OUT_TT_PRECIS,
      CLIP_DEFAULT_PRECIS,
      DEFAULT_PITCH,
      "Verdana" // Hardcode ftl :(
   };

   uint32_t r = static_cast<uint32_t>(g_settings.video.msg_color_r * 255) & 0xff;
   uint32_t g = static_cast<uint32_t>(g_settings.video.msg_color_g * 255) & 0xff;
   uint32_t b = static_cast<uint32_t>(g_settings.video.msg_color_b * 255) & 0xff;
   d3d->font_color = D3DCOLOR_XRGB(r, g, b);

   return SUCCEEDED(D3DXCreateFontIndirect(d3d->dev, &desc, &d3d->font));
}

static void d3dfonts_w32_deinit_font(void *data)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   if (d3d->font)
      d3d->font->Release();
   d3d->font = NULL;
}

static void d3dfonts_w32_render_msg(void *data, const char *msg, void *parms)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   font_params_t *params = (font_params_t*)userdata;

   if (msg && SUCCEEDED(d3d->dev->BeginScene()))
   {
      d3d->font->DrawTextA(NULL,
            msg,
            -1,
            &d3d->font_rect_shifted,
            DT_LEFT,
            ((d3d->font_color >> 2) & 0x3f3f3f) | 0xff000000);

      d3d->font->DrawTextA(NULL,
            msg,
            -1,
            &d3d->font_rect,
            DT_LEFT,
            d3d->font_color | 0xff000000);

      d3d->dev->EndScene();
   }
}

const d3d_font_renderer_t d3d_xdk1_font = {
   d3dfonts_w32_init_font,
   d3dfonts_w32_deinit_font,
   d3dfonts_w32_render_msg,
   "d3d-fonts-w32",
};

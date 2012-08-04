/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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
#include "xdk1_xfonts.h"
#include "../../general.h"

void xfonts_deinit_font(void)
{
}

void xfonts_render_msg_pre(xdk_d3d_video_t *d3d)
{
   d3d->d3d_render_device->GetBackBuffer(-1, D3DBACKBUFFER_TYPE_MONO, &d3d->pFrontBuffer);
   d3d->d3d_render_device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &d3d->pBackBuffer);
}

void xfonts_render_msg_place(void *data, float x, float y, float scale, const char *msg)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;

   xfonts_render_msg_pre(d3d);

   wchar_t str[256];
   convert_char_to_wchar(str, msg, sizeof(str));
   d3d->debug_font->TextOut(d3d->pFrontBuffer, str, (unsigned)-1, x, y);
   d3d->debug_font->TextOut(d3d->pBackBuffer, str, (unsigned)-1, x, y);

   xfonts_render_msg_post(d3d);
}

void xfonts_render_msg_post(xdk_d3d_video_t *d3d)
{
   d3d->pFrontBuffer->Release();
   d3d->pBackBuffer->Release();
}

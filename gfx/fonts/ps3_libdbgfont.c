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

#include <cell/dbgfont.h>
#include "../gl_common.h"

void gl_init_font(gl_t *gl, const char *font_path, unsigned font_size)
{
   (void)font_path;
   (void)font_size;

   CellDbgFontConfig cfg = {
      .bufSize      = 512,
      .screenWidth  = gl->win_width,
      .screenHeight = gl->win_height,
   };

   cellDbgFontInit(&cfg);
}

void gl_deinit_font(gl_t *gl)
{
   cellDbgFontExit();
}

void gl_render_msg(gl_t *gl, const char *msg)
{
   cellDbgFontPrintf(g_settings.video.msg_pos_x, 0.75f, 1.06f, SILVER, msg);
   cellDbgFontPrintf(g_settings.video.msg_pos_x, 0.75f, 1.05f, WHITE, msg);
}

void gl_render_msg_place(float x, float y, float scale, uint32_t color, const char *msg, ...)
{
   cellDbgFontPrintf(x, y, scale, color, msg);
}

void gl_render_msg_post(gl_t *gl)
{
   cellDbgFontDraw();
}

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

#include "fonts.h"
#include "../gfx_common.h"

#if defined(SN_TARGET_PSP2)
#include <libdbgfont.h>
#define DbgFontPrint(x, y, scale, color, msg) sceDbgFontPrint(x, y, color, msg)
#define DbgFontConfig SceDbgFontConfig
#define DbgFontInit sceDbgFontInit
#define DbgFontExit sceDbgFontExit
#elif defined(__CELLOS_LV2__)
#include <cell/dbgfont.h>
#define SCE_DBGFONT_BUFSIZE_LARGE 1024
#define DbgFontPrint(x, y, scale, color, msg) cellDbgFontPrintf(x, y, scale, color, msg)
#define DbgFontConfig CellDbgFontConfig
#define DbgFontInit cellDbgFontInit
#define DbgFontExit cellDbgFontExit
#endif

static bool gl_init_font(void *data, const char *font_path, float font_size)
{
   (void)font_path;
   (void)font_size;

   font_renderer_t *handle = (font_renderer_t*)calloc(1, sizeof(*handle));
   if (!handle)
      return NULL;

   DbgFontConfig cfg;
#if defined(SN_TARGET_PSP2)
   cfg.fontSize     = SCE_DBGFONT_FONTSIZE_LARGE;
#elif defined(__CELLOS_LV2__)
   // FIXME - We need to do init_font_first in gl_start because of this
   gl_t *gl = (gl_t*)driver.video_data;

   cfg.bufSize      = SCE_DBGFONT_BUFSIZE_LARGE;
   cfg.screenWidth  = gl->win_width;
   cfg.screenHeight = gl->win_height;
#endif

   DbgFontInit(&cfg);

   return true;
}

static void gl_deinit_font(void *data)
{
   (void)data;

   DbgFontExit();
}

static void gl_render_msg(void *data, const char *msg, void *parms)
{
   (void)data;
   float x, y, scale;
   unsigned color;
   font_params_t *params = (font_params_t*)parms;

   if (params)
   {
      x = params->x;
      y = params->y;
      scale = params->scale;
      color = params->color;
   }
   else
   {
      x = g_settings.video.msg_pos_x;
      y = 0.90f;
      scale = 1.04f;
      color = SILVER;
   }

   DbgFontPrint(x, y, scale, color, msg);

   if (!params)
      DbgFontPrint(x, y, scale - 0.01f, WHITE, msg);

#ifdef SN_TARGET_PSP2
   /* FIXME - if we ever get around to this port, move this out to some better place */
   sceDbgFontFlush();
#endif
}

const gl_font_renderer_t libdbg_font = {
   gl_init_font,
   gl_deinit_font,
   gl_render_msg,
   "GL raster",
};

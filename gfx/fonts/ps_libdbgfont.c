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

#include "fonts.h"

#if defined(SN_TARGET_PSP2)
#include <libdbgfont.h>
#define DbgFontPrint(x, y, scale, color, msg) sceDbgFontPrint(x, y, color, msg)
#define DbgFontConfig SceDbgFontConfig
#define DbgFontInit sceDbgFontInit
#define DbgFontExit sceDbgFontExit
#define DbgFontDraw sceDbgFontFlush
#elif defined(__CELLOS_LV2__)
#include <cell/dbgfont.h>
#define SCE_DBGFONT_BUFSIZE_LARGE 512
#define DbgFontPrint(x, y, scale, color, msg) cellDbgFontPrintf(x, y, scale, color, msg)
#define DbgFontConfig CellDbgFontConfig
#define DbgFontInit cellDbgFontInit
#define DbgFontExit cellDbgFontExit
#define DbgFontDraw cellDbgFontDraw
#endif

static void *libdbgpsfont_renderer_init(const char *font_path, unsigned font_size)
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
   gl_t *gl = (gl_t*)driver.video_data;

   cfg.bufSize      = SCE_DBGFONT_BUFSIZE_LARGE;
   cfg.screenWidth  = gl->win_width;
   cfg.screenHeight = gl->win_height;
#endif

   DbgFontInit(&cfg);

   return handle;
}

static void libdbgpsfont_renderer_free(void *data)
{
   (void)data;

   DbgFontExit();
}

static void libdbgpsfont_renderer_free_output(void *data, struct font_output_list *output)
{
}

static void libdbgpsfont_renderer_msg(void *data, const char *msg, struct font_output_list *output) 
{
   (void)data;
   float x, y, scale;
   unsigned color;

   if(!output)
   {
      x = g_settings.video.msg_pos_x;
      y = 0.76f;
      scale = 1.04f;
      color = SILVER;
   }
   else
   {
      x = output->head->off_x;
      y = output->head->off_y;
      scale = output->head->scaling_factor;
      color = WHITE;
   }

   DbgFontPrint(x, y, scale, color, msg);
   DbgFontPrint(x, y, scale - 0.01f, WHITE, msg);
   DbgFontDraw();
}

static const char *libdbgpsfont_renderer_get_default_font(void)
{
   return "";
}

const font_renderer_driver_t libdbgps_font_renderer = {
   libdbgpsfont_renderer_init,
   libdbgpsfont_renderer_msg,
   libdbgpsfont_renderer_free_output,
   libdbgpsfont_renderer_free,
   libdbgpsfont_renderer_get_default_font,
   "libdbgpsfont",
};

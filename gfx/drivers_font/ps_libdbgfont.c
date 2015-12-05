/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#if defined(SN_TARGET_PSP2)
#include <libdbgfont.h>
#define DbgFontPrint(x, y, scale, color, msg) sceDbgFontPrint(x, y, color, msg)
#define DbgFontConfig SceDbgFontConfig
#define DbgFontInit sceDbgFontInit
#define DbgFontExit sceDbgFontExit
#elif defined(__CELLOS_LV2__)
#include <cell/dbgfont.h>
#define SCE_DBGFONT_BUFSIZE_LARGE 2048
#define DbgFontPrint(x, y, scale, color, msg) cellDbgFontPrintf(x, y, scale, color, msg)
#define DbgFontConfig CellDbgFontConfig
#define DbgFontInit cellDbgFontInit
#define DbgFontExit cellDbgFontExit
#endif

#include "../font_driver.h"

static void *libdbg_font_init_font(void *gl_data, const char *font_path, float font_size)
{
   unsigned width, height;

   video_driver_get_size(&width, &height);

   (void)font_path;
   (void)font_size;
   (void)width;
   (void)height;

   DbgFontConfig cfg;
#if defined(SN_TARGET_PSP2)
   cfg.fontSize     = SCE_DBGFONT_FONTSIZE_LARGE;
#elif defined(__CELLOS_LV2__)
   cfg.bufSize      = SCE_DBGFONT_BUFSIZE_LARGE;
   cfg.screenWidth  = width;
   cfg.screenHeight = height;
#endif

   DbgFontInit(&cfg);

   /* Doesn't need any state. */
   return (void*)-1;
}

static void libdbg_font_free_font(void *data)
{
   (void)data;
   DbgFontExit();
}

static void libdbg_font_render_msg(void *data, const char *msg,
      const void *userdata)
{
   float x, y, scale;
   unsigned color;
   settings_t *settings = config_get_ptr();
   const struct font_params *params = (const struct font_params*)userdata;

   (void)data;

   if (params)
   {
      x     = params->x;
      y     = params->y;
      scale = params->scale;
      color = params->color;
   }
   else
   {
      x     = settings->video.msg_pos_x;
      y     = 0.90f;
      scale = 1.04f;
      color = SILVER;
   }

   DbgFontPrint(x, y, scale, color, msg);

   if (!params)
      DbgFontPrint(x, y, scale - 0.01f, WHITE, msg);

#ifdef SN_TARGET_PSP2
   /* FIXME - if we ever get around to this port, 
    * move this out to some better place */
   sceDbgFontFlush();
#endif
}

font_renderer_t libdbg_font = {
   libdbg_font_init_font,
   libdbg_font_free_font,
   libdbg_font_render_msg,
   "libdbgfont",
   NULL,                      /* get_glyph */
   NULL,                      /* bind_block */
   NULL,                      /* flush */
   NULL,                      /* get_message_width */
};

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2016 - Brad Parker
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

#include <stdlib.h>
#include <string/stdstring.h>
#include <encodings/utf.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../font_driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"
#include "../common/gdi_common.h"
#include "../common/win32_common.h"

#include <windows.h>
#include <wingdi.h>

typedef struct
{
   const font_renderer_driver_t *gdi_font_driver;
   void *gdi_font_data;
   gdi_t *gdi;
} gdi_raster_t;

static void *gdi_init_font(void *data,
      const char *font_path, float font_size)
{
   gdi_raster_t *font  = (gdi_raster_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   font->gdi = (gdi_t*)data;

   font_size = 1;

   if (!font_renderer_create_default((const void**)&font->gdi_font_driver,
            &font->gdi_font_data, font_path, font_size))
   {
      RARCH_WARN("Couldn't initialize font renderer.\n");
      return NULL;
   }

   return font;
}

static void gdi_render_free_font(void *data)
{

}

static int gdi_get_message_width(void *data, const char *msg,
      unsigned msg_len, float scale)
{
   return 0;
}

static const struct font_glyph *gdi_font_get_glyph(
      void *data, uint32_t code)
{
   return NULL;
}

static void gdi_render_msg(void *data, const char *msg,
      const void *userdata)
{
   gdi_raster_t *font = (gdi_raster_t*)data;
   float x, y;
   unsigned width = 0, height = 0;
   unsigned newX, newY;
   settings_t *settings = config_get_ptr();
   const struct font_params *params = (const struct font_params*)userdata;
   HDC hdc;
   HWND hwnd = win32_get_window();

   if (!font || string_is_empty(msg))
      return;

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

   if (!font->gdi)
      return;

   newX = x;//x * width;
   newY = y;//height - (y * height);

   //if (strlen(msg) + newX > width)
   //   newX -= strlen(msg) + newX - width;

   //gdi_put_str(*font->gdi->gdi_cv, newX, newY, msg);

   //gdi_refresh_display(*font->gdi->gdi_display);

   printf("drawing text: %s at %d x %d\n", msg, newX, newY);

   hdc = GetDC(hwnd);
   TextOut(hdc, newX, newY, msg, utf8len(msg));
   ReleaseDC(hwnd, hdc);

   UpdateWindow(hwnd);
}

static void gdi_font_flush_block(void* data)
{
   (void)data;
}

static void gdi_font_bind_block(void* data, void* userdata)
{
   (void)data;
}

font_renderer_t gdi_font = {
   gdi_init_font,
   gdi_render_free_font,
   gdi_render_msg,
   "gdi font",
   gdi_font_get_glyph,       /* get_glyph */
   gdi_font_bind_block,      /* bind_block */
   gdi_font_flush_block,     /* flush */
   gdi_get_message_width     /* get_message_width */
};

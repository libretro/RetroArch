/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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
#include <lists/string_list.h>

#include <windows.h>
#include <wingdi.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../retroarch.h"
#include "../common/gdi_common.h"
#include "../common/win32_common.h"

#include "../font_driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"


typedef struct
{
   const font_renderer_driver_t *gdi_font_driver;
   void *gdi_font_data;
   gdi_t *gdi;
} gdi_raster_t;

static void *gdi_init_font(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   gdi_raster_t *font  = (gdi_raster_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   font->gdi = (gdi_t*)data;

   if (!font_renderer_create_default(
            &font->gdi_font_driver,
            &font->gdi_font_data, font_path, font_size))
   {
      RARCH_WARN("Couldn't initialize font renderer.\n");
      return NULL;
   }

   return font;
}

static void gdi_render_free_font(void *data, bool is_threaded)
{
   (void)data;
   (void)is_threaded;
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

static void gdi_render_msg(
      void *userdata,
      void *data,
      const char *msg,
      const struct font_params *params)
{
   char* msg_local;
   float x, y, scale, drop_mod, drop_alpha;
   int drop_x, drop_y, msg_strlen;
   unsigned i;
   unsigned newX, newY, newDropX, newDropY;
   unsigned align;
   unsigned red, green, blue;
   unsigned drop_red, drop_green, drop_blue;
   gdi_t *gdi                       = (gdi_t*)userdata;
   gdi_raster_t *font               = (gdi_raster_t*)data;
   unsigned width                   = gdi->video_width;
   unsigned height                  = gdi->video_height;
   SIZE textSize                    = {0};
   struct string_list msg_list      = {0};
   settings_t *settings             = config_get_ptr();
   float video_msg_pos_x            = settings->floats.video_msg_pos_x;
   float video_msg_pos_y            = settings->floats.video_msg_pos_y;
   float video_msg_color_r          = settings->floats.video_msg_color_r;
   float video_msg_color_g          = settings->floats.video_msg_color_g;
   float video_msg_color_b          = settings->floats.video_msg_color_b;

   if (!font || string_is_empty(msg) || !font->gdi)
      return;

   if (params)
   {
      x          = params->x;
      y          = params->y;
      drop_x     = params->drop_x;
      drop_y     = params->drop_y;
      drop_mod   = params->drop_mod;
      drop_alpha = params->drop_alpha;
      scale      = params->scale;
      align      = params->text_align;

      red        = FONT_COLOR_GET_RED(params->color);
      green      = FONT_COLOR_GET_GREEN(params->color);
      blue       = FONT_COLOR_GET_BLUE(params->color);
   }
   else
   {
      x          = video_msg_pos_x;
      y          = video_msg_pos_y;
      drop_x     = -2;
      drop_y     = -2;
      drop_mod   = 0.3f;
      drop_alpha = 1.0f;
      scale      = 1.0f;
      align      = TEXT_ALIGN_LEFT;
      red        = video_msg_color_r * 255.0f;
      green      = video_msg_color_g * 255.0f;
      blue       = video_msg_color_b * 255.0f;
   }

   msg_local  = utf8_to_local_string_alloc(msg);
   msg_strlen = strlen(msg_local);

   GetTextExtentPoint32(font->gdi->memDC, msg_local, msg_strlen, &textSize);

   switch (align)
   {
      case TEXT_ALIGN_LEFT:
         newX     = x * width * scale;
         newDropX = drop_x * width * scale;
         break;
      case TEXT_ALIGN_RIGHT:
         newX     = (x * width * scale) - textSize.cx;
         newDropX = (drop_x * width * scale) - textSize.cx;
         break;
      case TEXT_ALIGN_CENTER:
         newX     = (x * width * scale) - (textSize.cx / 2);
         newDropX = (drop_x * width * scale) - (textSize.cx / 2);
         break;
      default:
         newX     = 0;
         newDropX = 0;
         break;
   }

   newY = height - (y * height * scale) - textSize.cy;
   newDropY = height - (drop_y * height * scale) - textSize.cy;

   font->gdi->bmp_old = (HBITMAP)SelectObject(font->gdi->memDC, font->gdi->bmp);

   SetBkMode(font->gdi->memDC, TRANSPARENT);

   string_list_initialize(&msg_list);
   string_split_noalloc(&msg_list, msg_local, "\n");

   if (drop_x || drop_y)
   {
      float dark_alpha = drop_alpha;
      drop_red   = red * drop_mod * dark_alpha;
      drop_green = green * drop_mod * dark_alpha;
      drop_blue  = blue * drop_mod * dark_alpha;

      SetTextColor(font->gdi->memDC, RGB(drop_red, drop_green, drop_blue));

      for (i = 0; i < msg_list.size; i++)
         TextOut(font->gdi->memDC, newDropX, newDropY + (textSize.cy * i),
               msg_list.elems[i].data,
               strlen(msg_list.elems[i].data));
   }

   SetTextColor(font->gdi->memDC, RGB(red, green, blue));

   for (i = 0; i < msg_list.size; i++)
      TextOut(font->gdi->memDC, newX, newY + (textSize.cy * i),
            msg_list.elems[i].data,
            strlen(msg_list.elems[i].data));

   string_list_deinitialize(&msg_list);
   free(msg_local);

   SelectObject(font->gdi->memDC, font->gdi->bmp_old);
}

font_renderer_t gdi_font = {
   gdi_init_font,
   gdi_render_free_font,
   gdi_render_msg,
   "gdi font",
   gdi_font_get_glyph,        /* get_glyph */
   NULL,                      /* bind_block */
   NULL,                      /* flush */
   gdi_get_message_width,     /* get_message_width */
   NULL                       /* get_line_metrics */
};

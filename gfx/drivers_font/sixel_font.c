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
#include <sixel.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../font_driver.h"
#include "../../verbosity.h"
#include "../common/sixel_common.h"

typedef struct
{
   const font_renderer_driver_t *sixel_font_driver;
   void *sixel_font_data;
   sixel_t *sixel;
} sixel_raster_t;

static void *sixel_init_font(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   sixel_raster_t *font  = (sixel_raster_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   font->sixel = (sixel_t*)data;

   if (!font_renderer_create_default(
            &font->sixel_font_driver,
            &font->sixel_font_data, font_path, font_size))
   {
      RARCH_WARN("Couldn't initialize font renderer.\n");
      return NULL;
   }

   return font;
}

static void sixel_render_free_font(void *data, bool is_threaded)
{
   (void)data;
   (void)is_threaded;
}

static int sixel_get_message_width(void *data, const char *msg,
      unsigned msg_len, float scale)
{
   return 0;
}

static const struct font_glyph *sixel_font_get_glyph(
      void *data, uint32_t code)
{
   return NULL;
}

static void sixel_render_msg(video_frame_info_t *video_info,
      void *data, const char *msg,
      const struct font_params *userdata)
{
   float x, y, scale;
   unsigned width, height;
   unsigned newX, newY;
   unsigned align;
   sixel_raster_t              *font = (sixel_raster_t*)data;
   const struct font_params *params = (const struct font_params*)userdata;

   (void)newX;
   (void)newY;

   if (!font || string_is_empty(msg))
      return;

   if (params)
   {
      x = params->x;
      y = params->y;
      scale = params->scale;
      align = params->text_align;
   }
   else
   {
      x = video_info->font_msg_pos_x;
      y = video_info->font_msg_pos_y;
      scale = 1.0f;
      align = TEXT_ALIGN_LEFT;
   }

   if (!font->sixel)
      return;

   width    = font->sixel->screen_width;
   height   = font->sixel->screen_height;
   newY     = height - (y * height * scale);

   switch (align)
   {
      case TEXT_ALIGN_RIGHT:
         newX = (x * width * scale) - strlen(msg);
         break;
      case TEXT_ALIGN_CENTER:
         newX = (x * width * scale) - (strlen(msg) / 2);
         break;
      case TEXT_ALIGN_LEFT:
      default:
         newX = x * width * scale;
         break;
   }

   /* FIXME: add text drawing support */
}

font_renderer_t sixel_font = {
   sixel_init_font,
   sixel_render_free_font,
   sixel_render_msg,
   "sixel font",
   sixel_font_get_glyph,       /* get_glyph */
   NULL,                       /* bind_block */
   NULL,                       /* flush */
   sixel_get_message_width     /* get_message_width */
};

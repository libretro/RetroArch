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
#include "../../configuration.h"
#include "../common/sixel_common.h"

typedef struct
{
   const font_renderer_driver_t *font_driver;
   void *font_data;
   sixel_t *sixel;
} sixel_raster_t;

static void *sixel_font_init(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   sixel_raster_t *font  = (sixel_raster_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   font->sixel = (sixel_t*)data;

   if (!font_renderer_create_default(
            &font->font_driver,
            &font->font_data, font_path, font_size))
      return NULL;

   return font;
}

static void sixel_font_free(void *data, bool is_threaded)
{
  sixel_raster_t *font  = (sixel_raster_t*)data;
  if (!font)
     return;

  if (font->font_driver && font->font_data && font->font_driver->free)
     font->font_driver->free(font->font_data);

  free(font);
}

static int sixel_font_get_message_width(void *data, const char *msg,
      size_t msg_len, float scale) { return 0; }
static const struct font_glyph *sixel_font_get_glyph(
      void *data, uint32_t code) { return NULL; }

static void sixel_font_render_msg(
      void *userdata,
      void *data,
      const char *msg,
      const struct font_params *_params)
{
   /* TODO/FIXME: add text drawing support */
#if 0
   float x, y, scale;
   size_t msg_len;
   unsigned width, height, new_x, new_y, align;
   sixel_raster_t              *font = (sixel_raster_t*)data;
   const struct font_params *params  = (const struct font_params*)_params;

   if (!font || string_is_empty(msg))
      return;

   if (params)
   {
      x                              = params->x;
      y                              = params->y;
      scale                          = params->scale;
      align                          = params->text_align;
   }
   else
   {
      settings_t *settings           = config_get_ptr();
      float video_msg_pos_x          = settings->floats.video_msg_pos_x;
      float video_msg_pos_y          = settings->floats.video_msg_pos_y;
      x                              = video_msg_pos_x;
      y                              = video_msg_pos_y;
      scale                          = 1.0f;
      align                          = TEXT_ALIGN_LEFT;
   }

   if (!font->sixel)
      return;

   width    = font->sixel->screen_width;
   height   = font->sixel->screen_height;
   new_y    = height - (y * height * scale);
   msg_len  = strlen(msg);

   switch (align)
   {
      case TEXT_ALIGN_RIGHT:
         new_x = (x * width * scale) - msg_len;
         break;
      case TEXT_ALIGN_CENTER:
         new_x =  (x * width * scale) - (msg_len / 2);
         break;
      case TEXT_ALIGN_LEFT:
      default:
         new_x = x * width * scale;
         break;
   }
#endif
}

font_renderer_t sixel_font = {
   sixel_font_init,
   sixel_font_free,
   sixel_font_render_msg,
   "sixel_font",
   sixel_font_get_glyph,
   NULL,                       /* bind_block */
   NULL,                       /* flush */
   sixel_font_get_message_width,
   NULL                        /* get_line_metrics */
};

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
#include <caca.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../common/caca_common.h"

#include "../font_driver.h"
#include "../../configuration.h"

typedef struct
{
   const font_renderer_driver_t *font_driver;
   void *font_data;
   caca_t *caca;
} caca_raster_t;

static void *caca_font_init(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   caca_raster_t *font  = (caca_raster_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   font->caca = (caca_t*)data;

   if (!font_renderer_create_default(
            &font->font_driver,
            &font->font_data, font_path, font_size))
      return NULL;

   return font;
}

static void caca_font_free(void *data, bool is_threaded)
{
  caca_raster_t *font = (caca_raster_t*)data;

  if (!font)
     return;

  if (font->font_driver && font->font_data && font->font_driver->free)
     font->font_driver->free(font->font_data);

  free(font);
}

static int caca_font_get_message_width(void *data, const char *msg,
      size_t msg_len, float scale)
{
   return 0;
}

static const struct font_glyph *caca_font_get_glyph(
      void *data, uint32_t code)
{
   return NULL;
}

static void caca_font_render_msg(
      void *userdata,
      void *data, const char *msg,
      const struct font_params *params)
{
   float x, y, scale;
   unsigned width, height;
   unsigned newX, newY;
   unsigned align;
   size_t msg_len;
   caca_raster_t              *font = (caca_raster_t*)data;
   settings_t *settings             = config_get_ptr();
   float video_msg_pos_x            = settings->floats.video_msg_pos_x;
   float video_msg_pos_y            = settings->floats.video_msg_pos_y;

   if (!font || string_is_empty(msg))
      return;

   if (params)
   {
      x     = params->x;
      y     = params->y;
      scale = params->scale;
      align = params->text_align;
   }
   else
   {
      x     = video_msg_pos_x;
      y     = video_msg_pos_y;
      scale = 1.0f;
      align = TEXT_ALIGN_LEFT;
   }

   if (!font->caca || !font->caca->cv || !font->caca->display ||
       !font->caca->cv || !font->caca->display)
      return;

   width    = caca_get_canvas_width(font->caca->cv);
   height   = caca_get_canvas_height(font->caca->cv);
   newY     = height - (y * height * scale);
   msg_len  = strlen(msg);

   switch (align)
   {
      case TEXT_ALIGN_RIGHT:
         newX = (x * width * scale) - msg_len;
         break;
      case TEXT_ALIGN_CENTER:
         newX = (x * width * scale) - (msg_len / 2);
         break;
      case TEXT_ALIGN_LEFT:
      default:
         newX = x * width * scale;
         break;
   }

   caca_put_str(font->caca->cv, newX, newY, msg);

   caca_refresh_display(font->caca->display);
}

font_renderer_t caca_font = {
   caca_font_init,
   caca_font_free,
   caca_font_render_msg,
   "caca font",
   caca_font_get_glyph,
   NULL,                      /* bind_block */
   NULL,                      /* flush */
   caca_font_get_message_width,
   NULL                       /* get_line_metrics */
};

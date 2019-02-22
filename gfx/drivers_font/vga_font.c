/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../font_driver.h"
#include "../../verbosity.h"
#include "../common/vga_common.h"

typedef struct
{
   const font_renderer_driver_t *vga_font_driver;
   void *vga_font_data;
   vga_t *vga;
} vga_raster_t;

static void *vga_init_font(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   vga_raster_t *font  = (vga_raster_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   font->vga = (vga_t*)data;

   font_size = 1;

   if (!font_renderer_create_default(
            &font->vga_font_driver,
            &font->vga_font_data, font_path, font_size))
   {
      RARCH_WARN("Couldn't initialize font renderer.\n");
      return NULL;
   }

   return font;
}

static void vga_render_free_font(void *data, bool is_threaded)
{
   (void)data;
   (void)is_threaded;
}

static int vga_get_message_width(void *data, const char *msg,
      unsigned msg_len, float scale)
{
   return 0;
}

static const struct font_glyph *vga_font_get_glyph(
      void *data, uint32_t code)
{
   return NULL;
}

static void vga_render_msg(video_frame_info_t *video_info,
      void *data, const char *msg,
      const struct font_params *params)
{
   float x, y, scale;
   unsigned width, height;
   unsigned newX, newY;
   unsigned align;
   vga_raster_t              *font = (vga_raster_t*)data;

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
      x     = video_info->font_msg_pos_x;
      y     = video_info->font_msg_pos_y;
      scale = 1.0f;
      align = TEXT_ALIGN_LEFT;
   }

   if (!font->vga)
      return;

   width    = VGA_WIDTH;
   height   = VGA_HEIGHT;
   newY     = height - (y * height * scale);

   switch (align)
   {
      case TEXT_ALIGN_LEFT:
         newX = x * width * scale;
         break;
      case TEXT_ALIGN_RIGHT:
         newX = (x * width * scale) - strlen(msg);
         break;
      case TEXT_ALIGN_CENTER:
         newX = (x * width * scale) - (strlen(msg) / 2);
         break;
      default:
         break;
   }
}

font_renderer_t vga_font = {
   vga_init_font,
   vga_render_free_font,
   vga_render_msg,
   "vga font",
   vga_font_get_glyph,       /* get_glyph */
   NULL,                     /* bind_block */
   NULL,                     /* flush */
   vga_get_message_width     /* get_message_width */
};

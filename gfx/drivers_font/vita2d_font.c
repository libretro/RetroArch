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

#include <vita2d.h>
#include "../../vita/stockfont.h"

#include "../font_renderer_driver.h"
#include "../font_driver.h"

typedef struct vita_font
{
   vita2d_font *font;
   float size;
   
} vita_font_t;

static void *vita2d_font_init_font(void *gl_data, const char *font_path, float font_size)
{
   RARCH_LOG("vita2d_font_init()\n");

   vita_font_t *vita = (vita_font_t *)calloc(1, sizeof(vita_font_t));
   
   vita->font = vita2d_load_font_mem(stockfont,stockfont_size);
   vita->size = font_size;
   RARCH_LOG("vita2d_font_init()\n");

   return vita;
}

static void vita2d_font_free_font(void *data)
{
   RARCH_LOG("vita2d_font_free()\n");

   vita_font_t *vita = (vita_font_t *)data;
   
   if(vita->font)
      vita2d_free_font(vita->font);
   
   vita->size = 0;
   
   RARCH_LOG("vita2d_font_free()\n");
}

static void vita2d_font_render_msg(void *data, const char *msg,
      const void *userdata)
{
   float x, y, scale;
   unsigned color;
   settings_t *settings = config_get_ptr();
   vita_font_t *vita = (vita_font_t *)data;
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

   vita2d_font_draw_text(vita->font, x, y, color, vita->size*scale, msg);

   if (!params)
      vita2d_font_draw_text(vita->font, x, y, color, vita->size*(scale - 0.01f), msg);
      
}

font_renderer_t vita2d_font_renderer = {
   vita2d_font_init_font,
   vita2d_font_free_font,
   vita2d_font_render_msg,
   "vita2dfont",
   NULL,                      /* get_glyph */
   NULL,                      /* bind_block */
   NULL,                      /* flush */
   NULL,                      /* get_message_width */
};

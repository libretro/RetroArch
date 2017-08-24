/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Brad Parker
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
#include "../video_driver.h"
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
      const char *font_path, float font_size,
      bool is_threaded)
{
   gdi_raster_t *font  = (gdi_raster_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   font->gdi = (gdi_t*)data;

   if (!font_renderer_create_default((const void**)&font->gdi_font_driver,
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
      video_frame_info_t *video_info,
      void *data, const char *msg,
      const void *userdata)
{
   float x, y, scale;
   gdi_raster_t *font = (gdi_raster_t*)data;
   unsigned newX, newY, len;
   unsigned align;
   const struct font_params *params = (const struct font_params*)userdata;
   unsigned width                   = video_info->width;
   unsigned height                  = video_info->height;

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

   if (!font->gdi)
      return;

   len  = utf8len(msg);

   switch (align)
   {
      case TEXT_ALIGN_LEFT:
         newX = x * width * scale;
         break;
      case TEXT_ALIGN_RIGHT:
         newX = (x * width * scale) - len;
         break;
      case TEXT_ALIGN_CENTER:
         newX = (x * width * scale) - (len / 2);
         break;
      default:
         break;
   }

   newY = height - (y * height * scale);

   uint64_t frame_count = 0;
   bool is_alive = false;
   bool is_focused = false;

   video_driver_get_status(&frame_count, &is_alive, &is_focused);

   font->gdi->bmp_old = (HBITMAP)SelectObject(font->gdi->memDC, font->gdi->bmp);
   SetBkMode(font->gdi->memDC, TRANSPARENT);
   SetTextColor(font->gdi->memDC, RGB(255,255,255));
   TextOut(font->gdi->memDC, newX, newY, msg, len);
   SelectObject(font->gdi->memDC, font->gdi->bmp_old);
}

static void gdi_font_flush_block(unsigned width, unsigned height, void* data)
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

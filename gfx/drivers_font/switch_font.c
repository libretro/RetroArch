/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018      - lifajucejo
 *  Copyright (C) 2018      - m4xw
 *  Copyright (C) 2018      - natinusala
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

#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <encodings/utf.h>

#include <retro_math.h>

#include "../font_driver.h"

#include "../../configuration.h"

#include "../common/switch_common.h"

#define AVG_GLPYH_LIMIT 140

typedef struct
{
   struct font_atlas *atlas;

   const font_renderer_driver_t *font_driver;
   void *font_data;
} switch_font_t;

static void *switch_font_init(void *data, const char *font_path,
      float font_size, bool is_threaded)
{
   switch_font_t *font = (switch_font_t *)calloc(1, sizeof(switch_font_t));

   if (!font)
      return NULL;

   if (!font_renderer_create_default(&font->font_driver,
            &font->font_data, font_path, font_size))
   {
      free(font);
      return NULL;
   }

   font->atlas = font->font_driver->get_atlas(font->font_data);

   return font;
}

static void switch_font_free(void *data, bool is_threaded)
{
   switch_font_t *font = (switch_font_t *)data;

   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   free(font);
}

static int switch_font_get_message_width(void *data, const char *msg,
      size_t msg_len, float scale)
{
   int i;
   const struct font_glyph* glyph_q = NULL;
   int         delta_x = 0;
   switch_font_t *font = (switch_font_t *)data;

   if (!font)
      return 0;

   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph *glyph;
      const char *msg_tmp = &msg[i];
      unsigned       code = utf8_walk(&msg_tmp);
      unsigned       skip = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      /* Do something smarter here ... */
      if (!(glyph =
               font->font_driver->get_glyph(font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      delta_x += glyph->advance_x;
   }

   return delta_x * scale;
}

static void switch_font_render_line(
      switch_video_t *sw,
      switch_font_t *font, const char *msg, size_t msg_len,
      float scale, const unsigned int color, float pos_x,
      float pos_y, unsigned text_align)
{
   int i;
   const struct font_glyph* glyph_q = NULL;
   int delta_x                      = 0;
   int delta_y                      = 0;
   unsigned fb_width                = sw->vp.full_width;
   unsigned fb_height               = sw->vp.full_height;
   int x                            = roundf(pos_x * fb_width);
   int y                            = roundf((1.0f - pos_y) * fb_height);

   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x -= switch_font_get_message_width(font, msg, msg_len, scale);
         break;
      case TEXT_ALIGN_CENTER:
         x -= switch_font_get_message_width(font, msg, msg_len, scale) / 2;
         break;
   }

   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph *glyph;
      int off_x, off_y, tex_x, tex_y, width, height;
      const char *msg_tmp = &msg[i];
      unsigned code       = utf8_walk(&msg_tmp);
      unsigned skip       = msg_tmp - &msg[i];

      if (skip > 1)
         i               += skip - 1;

      /* Do something smarter here ... */
      if (!(glyph =
               font->font_driver->get_glyph(font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      off_x  = x + glyph->draw_offset_x + delta_x;
      off_y  = y + glyph->draw_offset_y + delta_y;
      width  = glyph->width;
      height = glyph->height;

      tex_x = glyph->atlas_offset_x;
      tex_y = glyph->atlas_offset_y;

      for (y = tex_y; y < tex_y + height; y++)
      {
         int x;
         uint8_t *row = &font->atlas->buffer[y * font->atlas->width];
         for (x = tex_x; x < tex_x + width; x++)
         {
            int x1, y1;
            if (!row[x])
               continue;
            x1 = off_x + (x - tex_x);
            y1 = off_y + (y - tex_y);
            if (x1 < fb_width && y1 < fb_height)
               sw->out_buffer[y1 * sw->stride / sizeof(uint32_t) + x1] = color;
         }
      }

      delta_x += glyph->advance_x;
      delta_y += glyph->advance_y;
   }
}

static void switch_font_render_message(
      switch_video_t *sw,
      switch_font_t *font, const char *msg, float scale,
      const unsigned int color, float pos_x, float pos_y,
      unsigned text_align)
{
   struct font_line_metrics *line_metrics = NULL;
   int lines                              = 0;
   float line_height;

   if (!msg || !*msg || !sw)
      return;
   if (!sw || !sw->out_buffer)
      return;

   /* If font line metrics are not supported just draw as usual */
   if (!font->font_driver->get_line_metrics ||
       !font->font_driver->get_line_metrics(font->font_data, &line_metrics))
   {
      size_t msg_len = strlen(msg);
      if (msg_len <= AVG_GLPYH_LIMIT)
         switch_font_render_line(sw, font, msg, msg_len,
               scale, color, pos_x, pos_y, text_align);
      return;
   }

   line_height = scale / line_metrics->height;

   for (;;)
   {
      const char *delim = strchr(msg, '\n');
      size_t msg_len    = delim ?
         (delim - msg) : strlen(msg);

      /* Draw the line */
      if (msg_len <= AVG_GLPYH_LIMIT)
         switch_font_render_line(sw, font, msg, msg_len,
               scale, color, pos_x, pos_y - (float)lines * line_height,
               text_align);

      if (!delim)
         break;

      msg += msg_len + 1;
      lines++;
   }
}

static void switch_font_render_msg(
      void *userdata,
      void *data,
      const char *msg,
      const struct font_params *params)
{
   float x, y, scale;
   enum text_alignment text_align;
   unsigned color, r, g, b, alpha;
   switch_font_t *font              = (switch_font_t *)data;
   switch_video_t *sw               = (switch_video_t*)userdata;
   settings_t *settings             = config_get_ptr();
   float video_msg_color_r          = settings->floats.video_msg_color_r;
   float video_msg_color_g          = settings->floats.video_msg_color_g;
   float video_msg_color_b          = settings->floats.video_msg_color_b;

   if (!font || !msg || (msg && !*msg))
      return;

   if (params)
   {
      x          = params->x;
      y          = params->y;
      scale      = params->scale;
      text_align = params->text_align;

      r          = FONT_COLOR_GET_RED(params->color);
      g          = FONT_COLOR_GET_GREEN(params->color);
      b          = FONT_COLOR_GET_BLUE(params->color);
      alpha      = FONT_COLOR_GET_ALPHA(params->color);

      color      = params->color;
   }
   else
   {
      x          = 0.0f;
      y          = 0.0f;
      scale      = 1.0f;
      text_align = TEXT_ALIGN_LEFT;

      r          = (video_msg_color_r * 255);
      g          = (video_msg_color_g * 255);
      b          = (video_msg_color_b * 255);
      alpha      = 255;
      color      = COLOR_ABGR(r, g, b, alpha);

   }

   switch_font_render_message(sw, font, msg, scale,
         color, x, y, text_align);
}

static const struct font_glyph *switch_font_get_glyph(
    void *data, uint32_t code)
{
   switch_font_t *font = (switch_font_t *)data;
   if (font && font->font_driver && font->font_driver->ident)
      return font->font_driver->get_glyph((void *)font->font_driver, code);
   return NULL;
}

static bool switch_font_get_line_metrics(void* data, struct font_line_metrics **metrics)
{
   switch_font_t *font = (switch_font_t *)data;
   if (font && font->font_driver && font->font_data)
      return font->font_driver->get_line_metrics(font->font_data, metrics);
   return false;
}

font_renderer_t switch_font =
{
   switch_font_init,
   switch_font_free,
   switch_font_render_msg,
   "switch_font",
   switch_font_get_glyph,
   NULL, /* bind_block  */
   NULL, /* flush_block */
   switch_font_get_message_width,
   switch_font_get_line_metrics
};

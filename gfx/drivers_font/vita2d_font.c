/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <encodings/utf.h>

#include "../common/vita2d_common.h"

#include "../font_driver.h"

#include "../../configuration.h"
#include "../../verbosity.h"

typedef struct
{
   vita_video_t *vita;
   vita2d_texture *texture;
   const font_renderer_driver_t *font_driver;
   void *font_data;
   struct font_atlas *atlas;
} vita_font_t;

static void *vita2d_font_init_font(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   unsigned int stride, pitch, j, k;
   const uint8_t         *frame32 = NULL;
   uint8_t                 *tex32 = NULL;
   const struct font_atlas *atlas = NULL;
   vita_font_t              *font = (vita_font_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   font->vita                     = (vita_video_t*)data;

   if (!font_renderer_create_default(
            &font->font_driver,
            &font->font_data, font_path, font_size))
      goto error;

   font->atlas   = font->font_driver->get_atlas(font->font_data);
   atlas         = font->atlas;

   if (!atlas)
      goto error;

   font->texture = vita2d_create_empty_texture_format(
         atlas->width,
         atlas->height,
         SCE_GXM_TEXTURE_FORMAT_U8_R111);

   if (!font->texture)
      goto error;

   vita2d_texture_set_filters(font->texture,
         SCE_GXM_TEXTURE_FILTER_POINT,
         SCE_GXM_TEXTURE_FILTER_LINEAR);

   stride  = vita2d_texture_get_stride(font->texture);
   tex32   = vita2d_texture_get_datap(font->texture);
   frame32 = atlas->buffer;
   pitch   = atlas->width;

   for (j = 0; j < atlas->height; j++)
      for (k = 0; k < atlas->width; k++)
         tex32[k + j*stride] = frame32[k + j*pitch];

   font->atlas->dirty = false;

   return font;

error:
   RARCH_WARN("Couldn't initialize font renderer.\n");
   free(font);
   return NULL;
}

static void vita2d_font_free_font(void *data, bool is_threaded)
{
   vita_font_t *font = (vita_font_t*)data;
   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   vita2d_wait_rendering_done();
   vita2d_free_texture(font->texture);

   free(font);
}

static int vita2d_font_get_message_width(void *data, const char *msg,
      unsigned msg_len, float scale)
{
   unsigned i;
   int delta_x       = 0;
   vita_font_t *font = (vita_font_t*)data;

   if (!font)
      return 0;

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph *glyph = NULL;
      const char *msg_tmp            = &msg[i];
      unsigned code                  = utf8_walk(&msg_tmp);
      unsigned skip                  = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      glyph = font->font_driver->get_glyph(font->font_data, code);

      if (!glyph) /* Do something smarter here ... */
         glyph = font->font_driver->get_glyph(font->font_data, '?');

      if (!glyph)
         continue;

      delta_x += glyph->advance_x;
   }

   return delta_x * scale;
}

static void vita2d_font_render_line(
      vita_font_t *font, const char *msg, unsigned msg_len,
      float scale, const unsigned int color, float pos_x,
      float pos_y,
      unsigned width, unsigned height, unsigned text_align)
{
   unsigned i;
   int x           = roundf(pos_x * width);
   int y           = roundf((1.0f - pos_y) * height);
   int delta_x     = 0;
   int delta_y     = 0;

   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x -= vita2d_font_get_message_width(font, msg, msg_len, scale);
         break;
      case TEXT_ALIGN_CENTER:
         x -= vita2d_font_get_message_width(font, msg, msg_len, scale) / 2;
         break;
   }

   for (i = 0; i < msg_len; i++)
   {
      int off_x, off_y, tex_x, tex_y, width, height;
      unsigned int stride, pitch, j, k;
      const struct font_glyph *glyph = NULL;
      const uint8_t         *frame32 = NULL;
      uint8_t                 *tex32 = NULL;
      const char *msg_tmp            = &msg[i];
      unsigned code                  = utf8_walk(&msg_tmp);
      unsigned skip                  = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      glyph = font->font_driver->get_glyph(font->font_data, code);

      if (!glyph) /* Do something smarter here ... */
         glyph = font->font_driver->get_glyph(font->font_data, '?');

      if (!glyph)
         continue;

      off_x  = glyph->draw_offset_x;
      off_y  = glyph->draw_offset_y;
      tex_x  = glyph->atlas_offset_x;
      tex_y  = glyph->atlas_offset_y;
      width  = glyph->width;
      height = glyph->height;

      if (font->atlas->dirty)
      {
        stride  = vita2d_texture_get_stride(font->texture);
        tex32   = vita2d_texture_get_datap(font->texture);
        frame32 = font->atlas->buffer;
        pitch   = font->atlas->width;

        for (j = 0; j < font->atlas->height; j++)
           for (k = 0; k < font->atlas->width; k++)
              tex32[k + j*stride] = frame32[k + j*pitch];

         font->atlas->dirty = false;
      }

      vita2d_draw_texture_tint_part_scale(font->texture,
            x + (off_x + delta_x) * scale,
            y + (off_y + delta_y) * scale,
            tex_x, tex_y, width, height,
            scale,
            scale,
            color);

      delta_x += glyph->advance_x;
      delta_y += glyph->advance_y;
   }
}

static void vita2d_font_render_message(
      vita_font_t *font, const char *msg, float scale,
      const unsigned int color, float pos_x, float pos_y,
      unsigned width, unsigned height, unsigned text_align)
{
   struct font_line_metrics *line_metrics = NULL;
   int lines                              = 0;
   float line_height;

   if (!msg || !*msg)
      return;

   /* If font line metrics are not supported just draw as usual */
   if (!font->font_driver->get_line_metrics ||
       !font->font_driver->get_line_metrics(font->font_data, &line_metrics))
   {
      vita2d_font_render_line(font, msg, strlen(msg),
            scale, color, pos_x, pos_y, width, height, text_align);
      return;
   }

   line_height = line_metrics->height * scale / font->vita->vp.height;

   for (;;)
   {
      const char *delim = strchr(msg, '\n');
      unsigned msg_len  = (delim) ? 
         (unsigned)(delim - msg) : strlen(msg);

      /* Draw the line */
      vita2d_font_render_line(font, msg, msg_len,
            scale, color, pos_x, pos_y - (float)lines * line_height,
            width, height, text_align);

      if (!delim)
         break;

      msg += msg_len + 1;
      lines++;
   }
}

static void vita2d_font_render_msg(
      void *userdata,
      void *data,
      const char *msg,
      const struct font_params *params)
{
   float x, y, scale, drop_mod, drop_alpha;
   int drop_x, drop_y;
   unsigned max_glyphs;
   enum text_alignment text_align;
   bool full_screen                 = false ;
   unsigned color, color_dark, r, g, b,
            alpha, r_dark, g_dark, b_dark, alpha_dark;
   vita_video_t             *vita   = (vita_video_t *)userdata;
   vita_font_t                *font = (vita_font_t *)data;
   unsigned width                   = vita->video_width;
   unsigned height                  = vita->video_height;
   settings_t *settings             = config_get_ptr();
   float video_msg_pos_x            = settings->floats.video_msg_pos_x;
   float video_msg_pos_y            = settings->floats.video_msg_pos_y;
   float video_msg_color_r          = settings->floats.video_msg_color_r;
   float video_msg_color_g          = settings->floats.video_msg_color_g;
   float video_msg_color_b          = settings->floats.video_msg_color_b;

   if (!font || !msg || !*msg)
      return;

   if (params)
   {
      x              = params->x;
      y              = params->y;
      scale          = params->scale;
      full_screen    = params->full_screen;
      text_align     = params->text_align;
      drop_x         = params->drop_x;
      drop_y         = params->drop_y;
      drop_mod       = params->drop_mod;
      drop_alpha     = params->drop_alpha;
      r    				= FONT_COLOR_GET_RED(params->color);
      g    				= FONT_COLOR_GET_GREEN(params->color);
      b    				= FONT_COLOR_GET_BLUE(params->color);
      alpha    		= FONT_COLOR_GET_ALPHA(params->color);
      color    		= RGBA8(r,g,b,alpha);
   }
   else
   {
      x              = video_msg_pos_x;
      y              = video_msg_pos_y;
      scale          = 1.0f;
      full_screen    = true;
      text_align     = TEXT_ALIGN_LEFT;

      r              = (video_msg_color_r * 255);
      g              = (video_msg_color_g * 255);
      b              = (video_msg_color_b * 255);
      alpha			   = 255;
      color 		   = RGBA8(r,g,b,alpha);

      drop_x         = -2;
      drop_y         = -2;
      drop_mod       = 0.3f;
      drop_alpha     = 1.0f;
   }

   video_driver_set_viewport(width, height, full_screen, false);

   max_glyphs        = strlen(msg);

   if (drop_x || drop_y)
      max_glyphs    *= 2;

   if (drop_x || drop_y)
   {
      r_dark         = r * drop_mod;
      g_dark         = g * drop_mod;
      b_dark         = b * drop_mod;
      alpha_dark		= alpha * drop_alpha;
      color_dark     = RGBA8(r_dark,g_dark,b_dark,alpha_dark);

      vita2d_font_render_message(font, msg, scale, color_dark,
            x + scale * drop_x / width, y +
            scale * drop_y / height, width, height, text_align);
   }

   vita2d_font_render_message(font, msg, scale,
         color, x, y, width, height, text_align);
}

static const struct font_glyph *vita2d_font_get_glyph(
      void *data, uint32_t code)
{
   vita_font_t *font = (vita_font_t*)data;

   if (!font || !font->font_driver || !font->font_driver->ident)
       return NULL;
   return font->font_driver->get_glyph((void*)font->font_driver, code);
}

static bool vita2d_font_get_line_metrics(void* data, struct font_line_metrics **metrics)
{
   vita_font_t *font = (vita_font_t*)data;

   if (!font || !font->font_driver || !font->font_data)
      return -1;

   return font->font_driver->get_line_metrics(font->font_data, metrics);
}

font_renderer_t vita2d_vita_font = {
   vita2d_font_init_font,
   vita2d_font_free_font,
   vita2d_font_render_msg,
   "vita2dfont",
   vita2d_font_get_glyph,
   NULL,                      /* bind_block */
   NULL,                      /* flush */
   vita2d_font_get_message_width,
   vita2d_font_get_line_metrics
};

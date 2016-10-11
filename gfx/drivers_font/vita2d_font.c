/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include "../font_driver.h"

typedef struct
{
   vita2d_texture *texture;
   const font_t *font;

} vita_font_t;


static void *vita2d_font_init_font(void *gl_data, const font_t *font)
{
   unsigned int stride, pitch, j, k;
   const struct font_atlas *atlas = NULL;
   vita_font_t *self = (vita_font_t*)calloc(1, sizeof(*self));

   if (!self)
      return NULL;

   self->font = font_ref(font);

   atlas = font_get_atlas(self->font);

   self->texture = vita2d_create_empty_texture_format(atlas->width,atlas->height,SCE_GXM_TEXTURE_FORMAT_U8_R111);

   if (!self->texture) {
      font_unref(self->font);
      free(self);
      return NULL;
   }

   vita2d_texture_set_filters(self->texture,
      SCE_GXM_TEXTURE_FILTER_POINT,
      SCE_GXM_TEXTURE_FILTER_LINEAR);

   stride = vita2d_texture_get_stride(self->texture);
   uint8_t             *tex32 = vita2d_texture_get_datap(self->texture);
   const uint8_t     *frame32 = atlas->buffer;
   pitch = atlas->width;
   for (j = 0; j < atlas->height; j++)
      for (k = 0; k < atlas->width; k++)
         tex32[k + j*stride] = frame32[k + j*pitch];

   return self;
}

static void vita2d_font_free_font(void *data)
{
    vita_font_t *self= (vita_font_t*)data;
    if (!self)
       return;

    font_unref(self->font);

	 //vita2d_wait_rendering_done();
   vita2d_free_texture(self->texture);

   free(self);
}

static int vita2d_font_get_message_width(void *data, const char *msg,
      unsigned msg_len, float scale)
{
   vita_font_t *self = (vita_font_t*)data;

   unsigned i;
   int delta_x = 0;

   if (!self)
      return 0;

   for (i = 0; i < msg_len; i++)
   {
      const char *msg_tmp            = &msg[i];
      unsigned code                  = utf8_walk(&msg_tmp);
      unsigned skip                  = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      const struct font_glyph *glyph = font_get_glyph(self->font, code);

      if (glyph)
         delta_x += glyph->advance_x;
   }

   return delta_x * scale;
}

static void vita2d_font_render_line(
      vita_font_t *self, const char *msg, unsigned msg_len,
      float scale, const unsigned int color, float pos_x,
      float pos_y, unsigned text_align)
{
   int x, y, delta_x, delta_y;
	 unsigned width, height;
   unsigned i;

	 video_driver_get_size(&width, &height);

   x       = roundf(pos_x * width);
   y       = roundf((1.0f - pos_y) * height);
   delta_x = 0;
   delta_y = 0;


   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x -= vita2d_font_get_message_width(self, msg, msg_len, scale);
         break;
      case TEXT_ALIGN_CENTER:
         x -= vita2d_font_get_message_width(self, msg, msg_len, scale) / 2;
         break;
   }

   for (i = 0; i < msg_len; i++)
   {
      int off_x, off_y, tex_x, tex_y, width, height;
      const char *msg_tmp            = &msg[i];
      unsigned code                  = utf8_walk(&msg_tmp);
      unsigned skip                  = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;
         
      const struct font_glyph *glyph = font_get_glyph(self->font, code);

      if (!glyph)
         continue;

      off_x  = glyph->draw_offset_x;
      off_y  = glyph->draw_offset_y;
      tex_x  = glyph->atlas_offset_x;
      tex_y  = glyph->atlas_offset_y;
      width  = glyph->width;
      height = glyph->height;

         vita2d_draw_texture_tint_part_scale(self->texture,
				x + off_x + delta_x * scale,
				y + off_y + delta_y * scale,
				tex_x, tex_y, width, height,
				scale,
				scale,
				color);

      delta_x += glyph->advance_x;
      delta_y += glyph->advance_y;
   }
}

static void vita2d_font_render_message(
      vita_font_t *self, const char *msg, float scale,
      const unsigned int color, float pos_x, float pos_y,
      unsigned text_align)
{
   int lines = 0;
   float line_height;

   if (!msg || !*msg)
      return;

   /* If the font height is not supported just draw as usual */
   if (font_get_line_height(self->font) <= 0)
   {
      vita2d_font_render_line(self, msg, strlen(msg),
            scale, color, pos_x, pos_y, text_align);
      return;
   }

   line_height = scale / font_get_line_height(self->font);

   for (;;)
   {
      const char *delim = strchr(msg, '\n');

      /* Draw the line */
      if (delim)
      {
         unsigned msg_len = delim - msg;
         vita2d_font_render_line(self, msg, msg_len,
               scale, color, pos_x, pos_y - (float)lines * line_height,
               text_align);
         msg += msg_len + 1;
         lines++;
      }
      else
      {
         unsigned msg_len = strlen(msg);
         vita2d_font_render_line(self, msg, msg_len,
               scale, color, pos_x, pos_y - (float)lines * line_height,
               text_align);
         break;
      }
   }
}

static void vita2d_font_render_msg(void *data, const char *msg,
      const void *userdata)
{
	 float x, y, scale, drop_mod, drop_alpha;
   unsigned color, color_dark, r, g, b, alpha, r_dark, g_dark, b_dark, alpha_dark;
	 unsigned width, height;
	 int drop_x, drop_y;
	 unsigned max_glyphs;
	 enum text_alignment text_align;
   settings_t *settings = config_get_ptr();
   vita_font_t *font = (vita_font_t *)data;
   const struct font_params *params = (const struct font_params*)userdata;

	 if (!font || !msg || !*msg)
      return;

	 video_driver_get_size(&width, &height);

	 if (params)
	 {
			x           = params->x;
			y           = params->y;
			scale       = params->scale;
			text_align  = params->text_align;
			drop_x      = params->drop_x;
			drop_y      = params->drop_y;
			drop_mod    = params->drop_mod;
			drop_alpha  = params->drop_alpha;
			r    				= FONT_COLOR_GET_RED(params->color);
      g    				= FONT_COLOR_GET_GREEN(params->color);
      b    				= FONT_COLOR_GET_BLUE(params->color);
      alpha    		= FONT_COLOR_GET_ALPHA(params->color);
			color    		= params->color;
	 }
	 else
	 {
			x           = settings->video.msg_pos_x;
			y           = settings->video.msg_pos_y;
			scale       = 1.0f;
			text_align  = TEXT_ALIGN_LEFT;

			r           = (settings->video.msg_color_r * 255);
	    g           = (settings->video.msg_color_g * 255);
	    b           = (settings->video.msg_color_b * 255);
			alpha				= 255;
			color 			= RGBA8(r,g,b,alpha);

			drop_x = -2;
			drop_y = -2;
			drop_mod = 0.3f;
			drop_alpha = 1.0f;
	 }

	 max_glyphs = strlen(msg);
	 if (drop_x || drop_y)
			max_glyphs *= 2;

	 if (drop_x || drop_y)
	 {
			r_dark        = r * drop_mod;
			g_dark        = g * drop_mod;
			b_dark        = b * drop_mod;
			alpha_dark		= alpha * drop_alpha;
			color_dark = RGBA8(r_dark,g_dark,b_dark,alpha_dark);

			vita2d_font_render_message(font, msg, scale, color_dark,
						x + scale * drop_x / width, y +
						scale * drop_y / height, text_align);
	 }

	 vita2d_font_render_message(font, msg, scale,
				 color, x, y, text_align);
}

static const struct font_glyph *vita2d_font_get_glyph(
      void *data, uint32_t code)
{
   vita_font_t *self = (vita_font_t*)data;

   if (!self)
      return NULL;
   return font_get_glyph(self->font, code);
}

static void vita2d_font_flush_block(void *data)
{
   (void)data;
}

static void vita2d_font_bind_block(void *data, void *userdata)
{
   (void)data;
}


font_renderer_t vita2d_vita_font = {
   vita2d_font_init_font,
   vita2d_font_free_font,
   vita2d_font_render_msg,
   "vita2dfont",
	 vita2d_font_get_glyph,
   vita2d_font_bind_block,
   vita2d_font_flush_block,
   vita2d_font_get_message_width,
};

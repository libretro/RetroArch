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

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <encodings/utf.h>
#include <wiiu/gx2.h>

#include "../font_driver.h"
#include "../common/gx2_common.h"
#include "../../wiiu/system/memory.h"
#include "../../wiiu/wiiu_dbg.h"

#include "../../retroarch.h"
#include "../../verbosity.h"

typedef struct
{
   GX2Texture texture;
   GX2_vec2* ubo_tex;
   const font_renderer_driver_t* font_driver;
   void* font_data;
   struct font_atlas* atlas;
} wiiu_font_t;

static void* wiiu_font_init_font(void* data, const char* font_path,
      float font_size, bool is_threaded)
{
   uint32_t i;
   wiiu_font_t* font = (wiiu_font_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   if (!font_renderer_create_default(
            &font->font_driver,
            &font->font_data, font_path, font_size))
   {
      RARCH_WARN("Couldn't initialize font renderer.\n");
      free(font);
      return NULL;
   }

   font->atlas = font->font_driver->get_atlas(font->font_data);
   font->texture.surface.width       = font->atlas->width;
   font->texture.surface.height      = font->atlas->height;
   font->texture.surface.depth       = 1;
   font->texture.surface.dim         = GX2_SURFACE_DIM_TEXTURE_2D;
   font->texture.surface.tileMode    = GX2_TILE_MODE_LINEAR_ALIGNED;
   font->texture.viewNumSlices       = 1;

   font->texture.surface.format   = GX2_SURFACE_FORMAT_UNORM_R8;
   font->texture.compMap          = GX2_COMP_SEL(_R, _R, _R, _R);

   GX2CalcSurfaceSizeAndAlignment(&font->texture.surface);
   GX2InitTextureRegs(&font->texture);
   font->texture.surface.image = MEM1_alloc(font->texture.surface.imageSize,
                                 font->texture.surface.alignment);

   for (i = 0; (i < font->atlas->height) && (i < font->texture.surface.height); i++)
      memcpy((uint8_t*)font->texture.surface.image + (i * font->texture.surface.pitch),
             font->atlas->buffer + (i * font->atlas->width), font->atlas->width);

   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, font->texture.surface.image,
                 font->texture.surface.imageSize);

   font->atlas->dirty = false;

   font->ubo_tex = MEM1_alloc(sizeof(*font->ubo_tex), GX2_UNIFORM_BLOCK_ALIGNMENT);
   font->ubo_tex->width = font->texture.surface.width;
   font->ubo_tex->height = font->texture.surface.height;
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_UNIFORM_BLOCK, font->ubo_tex,
                 sizeof(*font->ubo_tex));

   return font;
}

static void wiiu_font_free_font(void* data, bool is_threaded)
{
   wiiu_font_t* font = (wiiu_font_t*)data;

   if (!font)
      return;

   if (font->font_driver && font->font_data &&
         font->font_driver->free)
      font->font_driver->free(font->font_data);

   if (font->texture.surface.image)
      MEM1_free(font->texture.surface.image);
   if (font->ubo_tex)
      MEM1_free(font->ubo_tex);
   free(font);
}

static int wiiu_font_get_message_width(void* data, const char* msg,
                                      unsigned msg_len, float scale)
{
   wiiu_font_t* font = (wiiu_font_t*)data;

   unsigned i;
   int delta_x = 0;

   if (!font)
      return 0;

   for (i = 0; i < msg_len; i++)
   {
      const char* msg_tmp            = &msg[i];
      unsigned code                  = utf8_walk(&msg_tmp);
      unsigned skip                  = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      const struct font_glyph* glyph =
         font->font_driver->get_glyph(font->font_data, code);

      if (!glyph) /* Do something smarter here ... */
         glyph = font->font_driver->get_glyph(font->font_data, '?');

      if (!glyph)
         continue;

      delta_x += glyph->advance_x;
   }

   return delta_x * scale;
}

static void wiiu_font_render_line(
      video_frame_info_t *video_info,
      wiiu_font_t* font, const char* msg, unsigned msg_len,
      float scale, const unsigned int color, float pos_x,
      float pos_y, unsigned text_align)
{
   unsigned i;
   wiiu_video_t* wiiu = (wiiu_video_t*)video_info->userdata;
   unsigned width   = video_info->width;
   unsigned height  = video_info->height;
   int x            = roundf(pos_x * width);
   int y            = roundf((1.0 - pos_y) * height);

   if(  !wiiu ||
         wiiu->vertex_cache.current + (msg_len * 4) > wiiu->vertex_cache.size)
      return;

   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x -= wiiu_font_get_message_width(font, msg, msg_len, scale);
         break;

      case TEXT_ALIGN_CENTER:
         x -= wiiu_font_get_message_width(font, msg, msg_len, scale) / 2;
         break;
   }

   sprite_vertex_t* v = wiiu->vertex_cache.v + wiiu->vertex_cache.current;

   for (i = 0; i < msg_len; i++)
   {
      const char* msg_tmp            = &msg[i];
      unsigned code                  = utf8_walk(&msg_tmp);
      unsigned skip                  = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      const struct font_glyph* glyph =
         font->font_driver->get_glyph(font->font_data, code);

      if (!glyph) /* Do something smarter here ... */
         glyph = font->font_driver->get_glyph(font->font_data, '?');

      if (!glyph)
         continue;

      v->pos.x = x + glyph->draw_offset_x * scale;
      v->pos.y = y + glyph->draw_offset_y * scale;
      v->pos.width = glyph->width * scale;
      v->pos.height = glyph->height * scale;

      v->coord.u = glyph->atlas_offset_x;
      v->coord.v = glyph->atlas_offset_y;
      v->coord.width = glyph->width;
      v->coord.height = glyph->height;

      v->color = color;

      v++;

      x += glyph->advance_x * scale;
      y += glyph->advance_y * scale;
   }

   int count = v - wiiu->vertex_cache.v - wiiu->vertex_cache.current;

   if (!count)
      return;

   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, wiiu->vertex_cache.v + wiiu->vertex_cache.current, count * sizeof(wiiu->vertex_cache.v));

   if(font->atlas->dirty)
   {
      for (i = 0; (i < font->atlas->height) && (i < font->texture.surface.height); i++)
         memcpy(font->texture.surface.image + (i * font->texture.surface.pitch),
                font->atlas->buffer + (i * font->atlas->width), font->atlas->width);

      GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, font->texture.surface.image,
                    font->texture.surface.imageSize);
      font->atlas->dirty = false;
   }

   GX2SetPixelTexture(&font->texture, sprite_shader.ps.samplerVars[0].location);
   GX2SetVertexUniformBlock(sprite_shader.vs.uniformBlocks[1].offset, sprite_shader.vs.uniformBlocks[1].size, font->ubo_tex);

   GX2DrawEx(GX2_PRIMITIVE_MODE_POINTS, count, wiiu->vertex_cache.current, 1);

   GX2SetVertexUniformBlock(sprite_shader.vs.uniformBlocks[1].offset, sprite_shader.vs.uniformBlocks[1].size, wiiu->ubo_tex);

   wiiu->vertex_cache.current = v - wiiu->vertex_cache.v;
}

static void wiiu_font_render_message(
      video_frame_info_t *video_info,
      wiiu_font_t* font, const char* msg, float scale,
      const unsigned int color, float pos_x, float pos_y,
      unsigned text_align)
{
   int lines = 0;
   float line_height;

   if (!msg || !*msg)
      return;

   /* If the font height is not supported just draw as usual */
   if (!font->font_driver->get_line_height)
   {
      wiiu_font_render_line(video_info, font, msg, strlen(msg),
                           scale, color, pos_x, pos_y, text_align);
      return;
   }

   line_height = scale / font->font_driver->get_line_height(font->font_data);

   for (;;)
   {
      const char* delim = strchr(msg, '\n');

      /* Draw the line */
      if (delim)
      {
         unsigned msg_len = delim - msg;
         wiiu_font_render_line(video_info, font, msg, msg_len,
                              scale, color, pos_x, pos_y - (float)lines * line_height,
                              text_align);
         msg += msg_len + 1;
         lines++;
      }
      else
      {
         unsigned msg_len = strlen(msg);
         wiiu_font_render_line(video_info, font, msg, msg_len,
                              scale, color, pos_x, pos_y - (float)lines * line_height,
                              text_align);
         break;
      }
   }
}

static void wiiu_font_render_msg(
      video_frame_info_t *video_info,
      void* data, const char* msg,
      const struct font_params *params)
{
   float x, y, scale, drop_mod, drop_alpha;
   int drop_x, drop_y;
   unsigned max_glyphs;
   enum text_alignment text_align;
   unsigned color, color_dark, r, g, b,
            alpha, r_dark, g_dark, b_dark, alpha_dark;
   wiiu_font_t                *font = (wiiu_font_t*)data;
   unsigned width                   = video_info->width;
   unsigned height                  = video_info->height;

   if (!font || !msg || !*msg)
      return;

   if (params)
   {
      x              = params->x;
      y              = params->y;
      scale          = params->scale;
      text_align     = params->text_align;
      drop_x         = params->drop_x;
      drop_y         = params->drop_y;
      drop_mod       = params->drop_mod;
      drop_alpha     = params->drop_alpha;

      r              = FONT_COLOR_GET_RED(params->color);
      g              = FONT_COLOR_GET_GREEN(params->color);
      b              = FONT_COLOR_GET_BLUE(params->color);
      alpha          = FONT_COLOR_GET_ALPHA(params->color);
      color          = params->color;
   }
   else
   {
      x              = video_info->font_msg_pos_x;
      y              = video_info->font_msg_pos_y;
      scale          = 1.0f;
      text_align     = TEXT_ALIGN_LEFT;

      r              = (video_info->font_msg_color_r * 255);
      g              = (video_info->font_msg_color_g * 255);
      b              = (video_info->font_msg_color_b * 255);
      alpha          = 255;
      color          = COLOR_RGBA(r, g, b, alpha);

      drop_x         = -2;
      drop_y         = -2;
      drop_mod       = 0.3f;
      drop_alpha     = 1.0f;
   }

   max_glyphs        = strlen(msg);

   if (drop_x || drop_y)
      max_glyphs    *= 2;

   if (drop_x || drop_y)
   {
      r_dark         = r * drop_mod;
      g_dark         = g * drop_mod;
      b_dark         = b * drop_mod;
      alpha_dark     = alpha * drop_alpha;
      color_dark     = COLOR_RGBA(r_dark, g_dark, b_dark, alpha_dark);

      wiiu_font_render_message(video_info, font, msg, scale, color_dark,
                              x + scale * drop_x / width, y +
                              scale * drop_y / height, text_align);
   }

   wiiu_font_render_message(video_info, font, msg, scale,
                           color, x, y, text_align);
}

static const struct font_glyph* wiiu_font_get_glyph(
   void* data, uint32_t code)
{
   wiiu_font_t* font = (wiiu_font_t*)data;

   if (!font || !font->font_driver)
      return NULL;

   if (!font->font_driver->ident)
      return NULL;

   return font->font_driver->get_glyph((void*)font->font_driver, code);
}

static int wiiu_font_get_line_height(void *data)
{
   wiiu_font_t* font = (wiiu_font_t*)data;

   if (!font || !font->font_driver || !font->font_data)
      return -1;

   return font->font_driver->get_line_height(font->font_data);
}

font_renderer_t wiiu_font =
{
   wiiu_font_init_font,
   wiiu_font_free_font,
   wiiu_font_render_msg,
   "wiiufont",
   wiiu_font_get_glyph,
   NULL,                   /* bind_block */
   NULL,                   /* flush */
   wiiu_font_get_message_width,
   wiiu_font_get_line_height
};

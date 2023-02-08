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

#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <encodings/utf.h>
#include <wiiu/gx2.h>

#include "../font_driver.h"
#include "../common/gx2_common.h"
#include "../../wiiu/system/memory.h"
#include "../../wiiu/wiiu_dbg.h"

#include "../../configuration.h"

typedef struct
{
   GX2Texture texture;
   GX2_vec2* ubo_tex;
   const font_renderer_driver_t* font_driver;
   void* font_data;
   struct font_atlas* atlas;
} wiiu_font_t;

static void* wiiu_font_init(void* data, const char* font_path,
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
      free(font);
      return NULL;
   }

   font->atlas                       = font->font_driver->get_atlas(font->font_data);
   font->texture.surface.width       = font->atlas->width;
   font->texture.surface.height      = font->atlas->height;
   font->texture.surface.depth       = 1;
   font->texture.surface.dim         = GX2_SURFACE_DIM_TEXTURE_2D;
   font->texture.surface.tileMode    = GX2_TILE_MODE_LINEAR_ALIGNED;
   font->texture.viewNumSlices       = 1;

   font->texture.surface.format      = GX2_SURFACE_FORMAT_UNORM_R8;
   font->texture.compMap             = GX2_COMP_SEL(_1, _1, _1, _R);

   GX2CalcSurfaceSizeAndAlignment(&font->texture.surface);
   GX2InitTextureRegs(&font->texture);
   font->texture.surface.image       = MEM1_alloc(
         font->texture.surface.imageSize,
         font->texture.surface.alignment);

   for (i = 0; (i < font->atlas->height) && (i < font->texture.surface.height); i++)
      memcpy((uint8_t*)font->texture.surface.image 
            + (i * font->texture.surface.pitch),
            font->atlas->buffer + (i * font->atlas->width),
            font->atlas->width);

   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE,
         font->texture.surface.image,
         font->texture.surface.imageSize);

   font->atlas->dirty    = false;
   font->ubo_tex         = MEM1_alloc(sizeof(*font->ubo_tex), GX2_UNIFORM_BLOCK_ALIGNMENT);
   font->ubo_tex->width  = font->texture.surface.width;
   font->ubo_tex->height = font->texture.surface.height;
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_UNIFORM_BLOCK, font->ubo_tex,
                 sizeof(*font->ubo_tex));

   return font;
}

static void wiiu_font_free(void* data, bool is_threaded)
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
      size_t msg_len, float scale)
{
   int i;
   int delta_x = 0;
   const struct font_glyph* glyph_q = NULL;
   wiiu_font_t                *font = (wiiu_font_t*)data;

   if (!font)
      return 0;

   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph* glyph;
      const char* msg_tmp            = &msg[i];
      unsigned code                  = utf8_walk(&msg_tmp);
      unsigned skip                  = msg_tmp - &msg[i];

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

static void wiiu_font_render_line(
      wiiu_video_t *wiiu,
      wiiu_font_t* font, const char* msg, size_t msg_len,
      float scale, const unsigned int color, float pos_x,
      float pos_y,
      unsigned width, unsigned height, unsigned text_align)
{
   int i;
   int count;
   sprite_vertex_t *v;
   const struct font_glyph* glyph_q = NULL;
   int x                            = roundf(pos_x * width);
   int y                            = roundf((1.0 - pos_y) * height);

   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x -= wiiu_font_get_message_width(font, msg, msg_len, scale);
         break;

      case TEXT_ALIGN_CENTER:
         x -= wiiu_font_get_message_width(font, msg, msg_len, scale) / 2;
         break;
   }

   v       = wiiu->vertex_cache.v + wiiu->vertex_cache.current;
   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph* glyph;
      const char* msg_tmp            = &msg[i];
      unsigned code                  = utf8_walk(&msg_tmp);
      unsigned skip                  = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      /* Do something smarter here ... */
      if (!(glyph =
               font->font_driver->get_glyph(font->font_data, code)))
         if (!(glyph  = glyph_q))
            continue;

      v->pos.x        = x + glyph->draw_offset_x * scale;
      v->pos.y        = y + glyph->draw_offset_y * scale;
      v->pos.width    = glyph->width * scale;
      v->pos.height   = glyph->height * scale;

      v->coord.u      = glyph->atlas_offset_x;
      v->coord.v      = glyph->atlas_offset_y;
      v->coord.width  = glyph->width;
      v->coord.height = glyph->height;

      v->color        = color;

      v++;

      x              += glyph->advance_x * scale;
      y              += glyph->advance_y * scale;
   }

   count = v - wiiu->vertex_cache.v - wiiu->vertex_cache.current;

   if (!count)
      return;

   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, wiiu->vertex_cache.v + wiiu->vertex_cache.current, count * sizeof(wiiu->vertex_cache.v));

   if(font->atlas->dirty)
   {
      for (i = 0; (i < font->atlas->height) && (i < font->texture.surface.height); i++)
         memcpy(font->texture.surface.image + (i * font->texture.surface.pitch),
                font->atlas->buffer + (i * font->atlas->width), font->atlas->width);

      GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE,
            font->texture.surface.image,
            font->texture.surface.imageSize);
      font->atlas->dirty = false;
   }

   GX2SetPixelTexture(&font->texture,
         sprite_shader.ps.samplerVars[0].location);
   GX2SetVertexUniformBlock(sprite_shader.vs.uniformBlocks[1].offset,
         sprite_shader.vs.uniformBlocks[1].size,
         font->ubo_tex);

   GX2DrawEx(GX2_PRIMITIVE_MODE_POINTS, count, wiiu->vertex_cache.current, 1);

   GX2SetVertexUniformBlock(sprite_shader.vs.uniformBlocks[1].offset, sprite_shader.vs.uniformBlocks[1].size, wiiu->ubo_tex);

   wiiu->vertex_cache.current = v - wiiu->vertex_cache.v;
}

static void wiiu_font_render_message(
      wiiu_video_t *wiiu,
      wiiu_font_t* font, const char* msg, float scale,
      const unsigned int color, float pos_x, float pos_y,
      unsigned width, unsigned height, unsigned text_align)
{
   struct font_line_metrics *line_metrics = NULL;
   int lines                              = 0;
   float line_height;

   if (!msg || !*msg)
      return;
   if (!wiiu)
      return;

   /* If font line metrics are not supported just draw as usual */
   if (!font->font_driver->get_line_metrics ||
       !font->font_driver->get_line_metrics(font->font_data, &line_metrics))
   {
      size_t msg_len = strlen(msg);
      if (wiiu->vertex_cache.current + (msg_len * 4) <= wiiu->vertex_cache.size) 
         wiiu_font_render_line(wiiu, font, msg, msg_len,
               scale, color, pos_x, pos_y,
               width, height, text_align);
      return;
   }

   line_height = line_metrics->height * scale / wiiu->vp.height;

   for (;;)
   {
      const char* delim = strchr(msg, '\n');
      size_t msg_len    = delim ? 
         (delim - msg) : strlen(msg);

      /* Draw the line */
      if (wiiu->vertex_cache.current + (msg_len * 4) <= wiiu->vertex_cache.size) 
         wiiu_font_render_line(wiiu, font, msg, msg_len,
               scale, color, pos_x, pos_y - (float)lines * line_height,
               width, height, text_align);

      if (!delim)
         break;

      msg += msg_len + 1;
      lines++;
   }
}

static void wiiu_font_render_msg(
      void *userdata,
      void* data,
      const char* msg,
      const struct font_params *params)
{
   float x, y, scale, drop_mod, drop_alpha;
   int drop_x, drop_y;
   enum text_alignment text_align;
   unsigned color, r, g, b, alpha;
   wiiu_video_t              *wiiu  = (wiiu_video_t*)userdata;
   wiiu_font_t                *font = (wiiu_font_t*)data;
   unsigned width                   = wiiu->vp.full_width;
   unsigned height                  = wiiu->vp.full_height;

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
      settings_t *settings    = config_get_ptr();
      float video_msg_pos_x   = settings->floats.video_msg_pos_x;
      float video_msg_pos_y   = settings->floats.video_msg_pos_y;
      float video_msg_color_r = settings->floats.video_msg_color_r;
      float video_msg_color_g = settings->floats.video_msg_color_g;
      float video_msg_color_b = settings->floats.video_msg_color_b;
      x                       = video_msg_pos_x;
      y                       = video_msg_pos_y;
      scale                   = 1.0f;
      text_align              = TEXT_ALIGN_LEFT;

      r                       = (video_msg_color_r * 255);
      g                       = (video_msg_color_g * 255);
      b                       = (video_msg_color_b * 255);
      alpha                   = 255;
      color                   = COLOR_RGBA(r, g, b, alpha);

      drop_x                  = -2;
      drop_y                  = -2;
      drop_mod                = 0.3f;
      drop_alpha              = 1.0f;
   }

   if (drop_x || drop_y)
   {
      unsigned r_dark         = r * drop_mod;
      unsigned g_dark         = g * drop_mod;
      unsigned b_dark         = b * drop_mod;
      unsigned alpha_dark     = alpha * drop_alpha;
      unsigned color_dark     = COLOR_RGBA(r_dark, g_dark, b_dark, alpha_dark);
      wiiu_font_render_message(wiiu, font, msg, scale, color_dark,
            x + scale * drop_x / width, y +
            scale * drop_y / height, width, height, text_align);
   }

   wiiu_font_render_message(wiiu, font, msg, scale,
         color, x, y, width, height, text_align);
}

static const struct font_glyph* wiiu_font_get_glyph(
   void* data, uint32_t code)
{
   wiiu_font_t* font = (wiiu_font_t*)data;
   if (font && font->font_driver && font->font_driver->ident)
      return font->font_driver->get_glyph((void*)font->font_driver, code);
   return NULL;
}

static bool wiiu_font_get_line_metrics(void* data, struct font_line_metrics **metrics)
{
   wiiu_font_t* font = (wiiu_font_t*)data;
   if (font && font->font_driver && font->font_data)
      return font->font_driver->get_line_metrics(font->font_data, metrics);
   return false;
}

font_renderer_t wiiu_font =
{
   wiiu_font_init,
   wiiu_font_free,
   wiiu_font_render_msg,
   "wiiu_font",
   wiiu_font_get_glyph,
   NULL,                   /* bind_block */
   NULL,                   /* flush */
   wiiu_font_get_message_width,
   wiiu_font_get_line_metrics
};

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2018 - Ali Bouhlel
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

#define CINTERFACE

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <encodings/utf.h>

#include "../font_driver.h"
#include "../common/d3d12_common.h"

#include "../../retroarch.h"
#include "../../verbosity.h"

typedef struct
{
   d3d12_texture_t               texture;
   const font_renderer_driver_t* font_driver;
   void*                         font_data;
   struct font_atlas*            atlas;
} d3d12_font_t;

static void*
d3d12_font_init_font(void* data, const char* font_path,
      float font_size, bool is_threaded)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;
   d3d12_font_t*  font  = (d3d12_font_t*)calloc(1, sizeof(*font));

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

   font->atlas               = font->font_driver->get_atlas(font->font_data);
   font->texture.sampler     = d3d12->samplers[RARCH_FILTER_LINEAR][RARCH_WRAP_BORDER];
   font->texture.desc.Width  = font->atlas->width;
   font->texture.desc.Height = font->atlas->height;
   font->texture.desc.Format = DXGI_FORMAT_A8_UNORM;
   font->texture.srv_heap    = &d3d12->desc.srv_heap;
   d3d12_init_texture(d3d12->device, &font->texture);
   d3d12_update_texture(
         font->atlas->width, font->atlas->height,
         font->atlas->width, DXGI_FORMAT_A8_UNORM,
         font->atlas->buffer, &font->texture);
   font->atlas->dirty = false;

   return font;
}

static void d3d12_font_free_font(void* data, bool is_threaded)
{
   d3d12_font_t* font = (d3d12_font_t*)data;

   if (!font)
      return;

   if (font->font_driver && font->font_data && font->font_driver->free)
      font->font_driver->free(font->font_data);

   d3d12_release_texture(&font->texture);

   free(font);
}

static int d3d12_font_get_message_width(void* data,
      const char* msg, unsigned msg_len, float scale)
{
   d3d12_font_t* font = (d3d12_font_t*)data;

   unsigned i;
   int      delta_x = 0;

   if (!font)
      return 0;

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph* glyph;
      const char*              msg_tmp = &msg[i];
      unsigned                 code    = utf8_walk(&msg_tmp);
      unsigned                 skip    = msg_tmp - &msg[i];

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

static void d3d12_font_render_line(
      video_frame_info_t* video_info,
      d3d12_font_t*       font,
      const char*         msg,
      unsigned            msg_len,
      float               scale,
      const unsigned int  color,
      float               pos_x,
      float               pos_y,
      unsigned            text_align)
{
   unsigned        i, count;
   void*           mapped_vbo = NULL;
   d3d12_sprite_t* v          = NULL;
   d3d12_sprite_t* vbo_start  = NULL;
   d3d12_video_t*  d3d12      = (d3d12_video_t*)video_info->userdata;
   unsigned        width      = video_info->width;
   unsigned        height     = video_info->height;
   int             x          = roundf(pos_x * width);
   int             y          = roundf((1.0 - pos_y) * height);
   D3D12_RANGE     range      = { 0, 0 };

   if (  !d3d12                  ||
         !d3d12->sprites.enabled ||
         msg_len > (unsigned)d3d12->sprites.capacity)
      return;

   if (d3d12->sprites.offset + msg_len > (unsigned)d3d12->sprites.capacity)
      d3d12->sprites.offset = 0;

   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x -= d3d12_font_get_message_width(font, msg, msg_len, scale);
         break;

      case TEXT_ALIGN_CENTER:
         x -= d3d12_font_get_message_width(font, msg, msg_len, scale) / 2;
         break;
   }

   D3D12Map(d3d12->sprites.vbo, 0, &range, (void**)&vbo_start);

   v           = vbo_start + d3d12->sprites.offset;
   range.Begin = (uintptr_t)v - (uintptr_t)vbo_start;

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph* glyph;
      const char*              msg_tmp = &msg[i];
      unsigned                 code    = utf8_walk(&msg_tmp);
      unsigned                 skip    = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      glyph = font->font_driver->get_glyph(font->font_data, code);

      if (!glyph) /* Do something smarter here ... */
         glyph = font->font_driver->get_glyph(font->font_data, '?');

      if (!glyph)
         continue;

      v->pos.x = (x + (glyph->draw_offset_x * scale)) / (float)d3d12->chain.viewport.Width;
      v->pos.y = (y + (glyph->draw_offset_y * scale)) / (float)d3d12->chain.viewport.Height;
      v->pos.w = glyph->width * scale  / (float)d3d12->chain.viewport.Width;
      v->pos.h = glyph->height * scale / (float)d3d12->chain.viewport.Height;

      v->coords.u = glyph->atlas_offset_x / (float)font->texture.desc.Width;
      v->coords.v = glyph->atlas_offset_y / (float)font->texture.desc.Height;
      v->coords.w = glyph->width          / (float)font->texture.desc.Width;
      v->coords.h = glyph->height         / (float)font->texture.desc.Height;

      v->params.scaling  = 1;
      v->params.rotation = 0;

      v->colors[0] = color;
      v->colors[1] = color;
      v->colors[2] = color;
      v->colors[3] = color;

      v++;

      x += glyph->advance_x * scale;
      y += glyph->advance_y * scale;
   }

   range.End = (uintptr_t)v - (uintptr_t)vbo_start;
   D3D12Unmap(d3d12->sprites.vbo, 0, &range);

   count = v - vbo_start - d3d12->sprites.offset;

   if (!count)
      return;

   if (font->atlas->dirty)
   {
      d3d12_update_texture(
            font->atlas->width, font->atlas->height,
            font->atlas->width, DXGI_FORMAT_A8_UNORM,
            font->atlas->buffer, &font->texture);
      font->atlas->dirty = false;
   }

   if(font->texture.dirty)
      d3d12_upload_texture(d3d12->queue.cmd, &font->texture,
            video_info->userdata);

   D3D12SetPipelineState(d3d12->queue.cmd, d3d12->sprites.pipe_font);
   d3d12_set_texture_and_sampler(d3d12->queue.cmd, &font->texture);
   D3D12DrawInstanced(d3d12->queue.cmd, count, 1, d3d12->sprites.offset, 0);

   D3D12SetPipelineState(d3d12->queue.cmd, d3d12->sprites.pipe);

   d3d12->sprites.offset += count;
}

static void d3d12_font_render_message(
      video_frame_info_t* video_info,
      d3d12_font_t*       font,
      const char*         msg,
      float               scale,
      const unsigned int  color,
      float               pos_x,
      float               pos_y,
      unsigned            text_align)
{
   int   lines = 0;
   float line_height;

   if (!msg || !*msg)
      return;

   /* If the font height is not supported just draw as usual */
   if (!font->font_driver->get_line_height)
   {
      d3d12_font_render_line(
            video_info, font, msg, strlen(msg),
            scale, color, pos_x, pos_y, text_align);
      return;
   }

   line_height = font->font_driver->get_line_height(font->font_data)
      * scale / video_info->height;

   for (;;)
   {
      const char* delim = strchr(msg, '\n');

      /* Draw the line */
      if (delim)
      {
         unsigned msg_len = delim - msg;
         d3d12_font_render_line(
               video_info, font, msg, msg_len, scale, color, pos_x,
               pos_y - (float)lines * line_height, text_align);
         msg += msg_len + 1;
         lines++;
      }
      else
      {
         unsigned msg_len = strlen(msg);
         d3d12_font_render_line(
               video_info, font, msg, msg_len, scale, color, pos_x,
               pos_y - (float)lines * line_height, text_align);
         break;
      }
   }
}

static void d3d12_font_render_msg(
      video_frame_info_t* video_info, void* data,
      const char* msg, const struct font_params *params)
{
   float                     x, y, scale, drop_mod, drop_alpha;
   int                       drop_x, drop_y;
   enum text_alignment       text_align;
   unsigned                  color, color_dark, r, g, b,
                             alpha, r_dark, g_dark, b_dark, alpha_dark;
   d3d12_font_t*             font   = (d3d12_font_t*)data;
   unsigned                  width  = video_info->width;
   unsigned                  height = video_info->height;

   if (!font || !msg || !*msg)
      return;

   if (params)
   {
      x          = params->x;
      y          = params->y;
      scale      = params->scale;
      text_align = params->text_align;
      drop_x     = params->drop_x;
      drop_y     = params->drop_y;
      drop_mod   = params->drop_mod;
      drop_alpha = params->drop_alpha;

      r          = FONT_COLOR_GET_RED(params->color);
      g          = FONT_COLOR_GET_GREEN(params->color);
      b          = FONT_COLOR_GET_BLUE(params->color);
      alpha      = FONT_COLOR_GET_ALPHA(params->color);
      color      = DXGI_COLOR_RGBA(r, g, b, alpha);
   }
   else
   {
      x          = video_info->font_msg_pos_x;
      y          = video_info->font_msg_pos_y;
      scale      = 1.0f;
      text_align = TEXT_ALIGN_LEFT;

      r          = (video_info->font_msg_color_r * 255);
      g          = (video_info->font_msg_color_g * 255);
      b          = (video_info->font_msg_color_b * 255);
      alpha      = 255;
      color      = DXGI_COLOR_RGBA(r, g, b, alpha);

      drop_x     = -2;
      drop_y     = -2;
      drop_mod   = 0.3f;
      drop_alpha = 1.0f;
   }

   if (drop_x || drop_y)
   {
      r_dark     = r * drop_mod;
      g_dark     = g * drop_mod;
      b_dark     = b * drop_mod;
      alpha_dark = alpha * drop_alpha;
      color_dark = DXGI_COLOR_RGBA(r_dark, g_dark, b_dark, alpha_dark);

      d3d12_font_render_message(
            video_info, font, msg, scale, color_dark,
            x + scale * drop_x / width,
            y + scale * drop_y / height,
            text_align);
   }

   d3d12_font_render_message(video_info, font,
         msg, scale, color, x, y, text_align);
}

static const struct font_glyph* d3d12_font_get_glyph(
      void* data, uint32_t code)
{
   d3d12_font_t* font = (d3d12_font_t*)data;

   if (!font || !font->font_driver)
      return NULL;

   if (!font->font_driver->ident)
      return NULL;

   return font->font_driver->get_glyph((void*)font->font_driver, code);
}

static int d3d12_font_get_line_height(void *data)
{
   d3d12_font_t* font = (d3d12_font_t*)data;

   if (!font || !font->font_driver || !font->font_data)
      return -1;

   return font->font_driver->get_line_height(font->font_data);
}

font_renderer_t d3d12_font = {
   d3d12_font_init_font,
   d3d12_font_free_font,
   d3d12_font_render_msg,
   "d3d12font",
   d3d12_font_get_glyph,
   NULL, /* bind_block */
   NULL, /* flush */
   d3d12_font_get_message_width,
   d3d12_font_get_line_height
};

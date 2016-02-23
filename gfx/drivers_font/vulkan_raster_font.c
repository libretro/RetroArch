/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016 - Hans-Kristian Arntzen
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

#include "../common/vulkan_common.h"
#include "../font_driver.h"

typedef struct
{
   vk_t *vk;
   struct vk_texture texture;
   const font_renderer_driver_t *font_driver;
   void *font_data;

   struct vk_vertex *pv;
   struct vk_buffer_range range;
   unsigned vertices;
} vulkan_raster_t;

static void vulkan_raster_font_free_font(void *data);

static void *vulkan_raster_font_init_font(void *data,
      const char *font_path, float font_size)
{
   const struct font_atlas *atlas = NULL;
   vulkan_raster_t *font = (vulkan_raster_t*)calloc(1, sizeof(*font));

#if 0
   VkComponentMapping swizzle = {
      VK_COMPONENT_SWIZZLE_ONE,
      VK_COMPONENT_SWIZZLE_ONE,
      VK_COMPONENT_SWIZZLE_ONE,
      VK_COMPONENT_SWIZZLE_R,
   };
#endif

   if (!font)
      return NULL;

   font->vk = (vk_t*)data;

   if (!font_renderer_create_default((const void**)&font->font_driver,
            &font->font_data, font_path, font_size))
   {
      RARCH_WARN("Couldn't initialize font renderer.\n");
      free(font);
      return NULL;
   }

   atlas = font->font_driver->get_atlas(font->font_data);
   font->texture = vulkan_create_texture(font->vk, NULL,
         atlas->width, atlas->height, VK_FORMAT_R8_UNORM, atlas->buffer,
         NULL /*&swizzle*/, VULKAN_TEXTURE_STATIC);

   return font;
}

static void vulkan_raster_font_free_font(void *data)
{
   vulkan_raster_t *font = (vulkan_raster_t*)data;
   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   vkQueueWaitIdle(font->vk->context->queue);
   vulkan_destroy_texture(font->vk->context->device, &font->texture);

   free(font);
}

static int vulkan_get_message_width(void *data, const char *msg, unsigned msg_len, float scale)
{
   vulkan_raster_t *font = (vulkan_raster_t*)data;
      
   unsigned i;
   int delta_x = 0;

   if (!font)
      return 0;

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph *glyph = 
         font->font_driver->get_glyph(font->font_data, (uint8_t)msg[i]);
      if (!glyph) /* Do something smarter here ... */
         glyph = font->font_driver->get_glyph(font->font_data, '?');
      if (!glyph)
         continue;

      delta_x += glyph->advance_x;
   }

   return delta_x * scale;
}

static void vulkan_raster_font_render_line(
      vulkan_raster_t *font, const char *msg, unsigned msg_len,
      float scale, const float color[4], float pos_x,
      float pos_y, unsigned text_align)
{
   int x, y, delta_x, delta_y;
   float inv_tex_size_x, inv_tex_size_y, inv_win_width, inv_win_height;
   unsigned i;
   struct vk_color vk_color;
   vk_t *vk = font ? font->vk : NULL;

   if (!vk)
      return;

   x       = roundf(pos_x * vk->vp.width);
   y       = roundf((1.0f - pos_y) * vk->vp.height);
   delta_x = 0;
   delta_y = 0;

   vk_color.r = color[0];
   vk_color.g = color[1];
   vk_color.b = color[2];
   vk_color.a = color[3];

   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x -= vulkan_get_message_width(font, msg, msg_len, scale);
         break;
      case TEXT_ALIGN_CENTER:
         x -= vulkan_get_message_width(font, msg, msg_len, scale) / 2;
         break;
   }

   inv_tex_size_x = 1.0f / font->texture.width;
   inv_tex_size_y = 1.0f / font->texture.height;
   inv_win_width  = 1.0f / font->vk->vp.width;
   inv_win_height = 1.0f / font->vk->vp.height;

   for (i = 0; i < msg_len; i++)
   {
      int off_x, off_y, tex_x, tex_y, width, height;
      const struct font_glyph *glyph =
         font->font_driver->get_glyph(font->font_data, (uint8_t)msg[i]);

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

      vulkan_write_quad_vbo(font->pv + font->vertices,
            (x + off_x + delta_x * scale) * inv_win_width, (y + off_y + delta_y * scale) * inv_win_height,
            width * scale * inv_win_width, height * scale * inv_win_height,
            tex_x * inv_tex_size_x, tex_y * inv_tex_size_y,
            width * inv_tex_size_x, height * inv_tex_size_y,
            &vk_color);
      font->vertices += 6;

      delta_x += glyph->advance_x;
      delta_y += glyph->advance_y;
   }
}

static void vulkan_raster_font_render_message(
      vulkan_raster_t *font, const char *msg, float scale,
      const float color[4], float pos_x, float pos_y,
      unsigned text_align)
{
   int lines = 0;
   float line_height;

   if (!msg || !*msg || !font->vk)
      return;

   /* If the font height is not supported just draw as usual */
   if (!font->font_driver->get_line_height)
   {
      vulkan_raster_font_render_line(font, msg, strlen(msg), scale, color, pos_x, pos_y, text_align);
      return;
   }

   line_height = scale / font->font_driver->get_line_height(font->font_data);

   for (;;)
   {
      const char *delim = strchr(msg, '\n');

      /* Draw the line */
      if (delim)
      {
         unsigned msg_len = delim - msg;
         vulkan_raster_font_render_line(font, msg, msg_len, scale, color, pos_x, pos_y - (float)lines * line_height, text_align);
         msg += msg_len + 1;
         lines++;
      }
      else
      {
         unsigned msg_len = strlen(msg);
         vulkan_raster_font_render_line(font, msg, msg_len, scale, color, pos_x, pos_y - (float)lines * line_height, text_align);
         break;
      }
   }
}

static void vulkan_raster_font_setup_viewport(vulkan_raster_t *font, bool full_screen)
{
   unsigned width, height;
   video_driver_get_size(&width, &height);
   video_driver_set_viewport(width, height, full_screen, false);
}

static void vulkan_raster_font_flush(vulkan_raster_t *font)
{
   const struct vk_draw_triangles call = {
      font->vk->pipelines.font,
      &font->texture,
      font->vk->samplers.nearest,
      &font->vk->mvp,
      &font->range,
      font->vertices,
   };

   vulkan_draw_triangles(font->vk, &call);
}

static void vulkan_raster_font_render_msg(void *data, const char *msg,
      const void *userdata)
{
   float x, y, scale, drop_mod;
   float color[4], color_dark[4];
   int drop_x, drop_y;
   bool full_screen;
   enum text_alignment text_align;
   vk_t *vk = NULL;
   vulkan_raster_t *font = (vulkan_raster_t*)data;
   settings_t *settings = config_get_ptr();
   const struct font_params *params = (const struct font_params*)userdata;
   unsigned max_glyphs;

   if (!font || !msg || !*msg)
      return;

   vk = font->vk;

   if (params)
   {
      x           = params->x;
      y           = params->y;
      scale       = params->scale;
      full_screen = params->full_screen;
      text_align  = params->text_align;
      drop_x      = params->drop_x;
      drop_y      = params->drop_y;
      drop_mod    = params->drop_mod;

      color[0]    = FONT_COLOR_GET_RED(params->color) / 255.0f;
      color[1]    = FONT_COLOR_GET_GREEN(params->color) / 255.0f;
      color[2]    = FONT_COLOR_GET_BLUE(params->color) / 255.0f;
      color[3]    = FONT_COLOR_GET_ALPHA(params->color) / 255.0f;

      /* If alpha is 0.0f, turn it into default 1.0f */
      if (color[3] <= 0.0f)
         color[3] = 1.0f;
   }
   else
   {
      x           = settings->video.msg_pos_x;
      y           = settings->video.msg_pos_y;
      scale       = 1.0f;
      full_screen = true;
      text_align  = TEXT_ALIGN_LEFT;

      color[0]    = settings->video.msg_color_r;
      color[1]    = settings->video.msg_color_g;
      color[2]    = settings->video.msg_color_b;
      color[3] = 1.0f;

      drop_x = -2;
      drop_y = -2;
      drop_mod = 0.3f;
   }

   vulkan_raster_font_setup_viewport(font, full_screen);

   max_glyphs = strlen(msg);
   if (drop_x || drop_y)
      max_glyphs *= 2;

   if (!vulkan_buffer_chain_alloc(font->vk->context, &font->vk->chain->vbo,
         6 * sizeof(struct vk_vertex) * max_glyphs, &font->range))
      return;

   font->vertices = 0;
   font->pv = (struct vk_vertex*)font->range.data;

   if (drop_x || drop_y)
   {
      color_dark[0] = color[0] * drop_mod;
      color_dark[1] = color[1] * drop_mod;
      color_dark[2] = color[2] * drop_mod;
      color_dark[3] = color[3];

      vulkan_raster_font_render_message(font, msg, scale, color_dark,
            x + scale * drop_x / vk->vp.width, y + 
            scale * drop_y / vk->vp.height, text_align);
   }

   vulkan_raster_font_render_message(font, msg, scale, color, x, y, text_align);
   vulkan_raster_font_flush(font);
}

static const struct font_glyph *vulkan_raster_font_get_glyph(
      void *data, uint32_t code)
{
   vulkan_raster_t *font = (vulkan_raster_t*)data;

   if (!font || !font->font_driver)
      return NULL;
   if (!font->font_driver->ident)
       return NULL;
   return font->font_driver->get_glyph((void*)font->font_driver, code);
}

static void vulkan_raster_font_flush_block(void *data)
{
   (void)data;
}

static void vulkan_raster_font_bind_block(void *data, void *userdata)
{
   (void)data;
}

font_renderer_t vulkan_raster_font = {
   vulkan_raster_font_init_font,
   vulkan_raster_font_free_font,
   vulkan_raster_font_render_msg,
   "Vulkan raster",
   vulkan_raster_font_get_glyph,
   vulkan_raster_font_bind_block,
   vulkan_raster_font_flush_block,
   vulkan_get_message_width
};

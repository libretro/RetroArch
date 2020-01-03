/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016-2017 - Hans-Kristian Arntzen
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

#include <string.h>

#include <encodings/utf.h>
#include <compat/strl.h>

#include "../common/vulkan_common.h"

#include "../font_driver.h"

typedef struct
{
   vk_t *vk;
   struct vk_texture texture;
   struct vk_texture texture_optimal;
   const font_renderer_driver_t *font_driver;
   void *font_data;
   struct font_atlas *atlas;
   bool needs_update;

   struct vk_vertex *pv;
   struct vk_buffer_range range;
   unsigned vertices;
} vulkan_raster_t;

static void vulkan_raster_font_free_font(void *data, bool is_threaded);

static void *vulkan_raster_font_init_font(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   vulkan_raster_t *font          =
      (vulkan_raster_t*)calloc(1, sizeof(*font));

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

   if (!font_renderer_create_default(
            &font->font_driver,
            &font->font_data, font_path, font_size))
   {
      RARCH_WARN("Couldn't initialize font renderer.\n");
      free(font);
      return NULL;
   }

   font->atlas = font->font_driver->get_atlas(font->font_data);
   font->texture = vulkan_create_texture(font->vk, NULL,
         font->atlas->width, font->atlas->height, VK_FORMAT_R8_UNORM, font->atlas->buffer,
         NULL /*&swizzle*/, VULKAN_TEXTURE_STAGING);

   {
      struct vk_texture *texture = &font->texture;
      VK_MAP_PERSISTENT_TEXTURE(font->vk->context->device, texture);
   }

   font->texture_optimal = vulkan_create_texture(font->vk, NULL,
         font->atlas->width, font->atlas->height, VK_FORMAT_R8_UNORM, NULL,
         NULL /*&swizzle*/, VULKAN_TEXTURE_DYNAMIC);

   font->needs_update = true;

   return font;
}

static void vulkan_raster_font_free_font(void *data, bool is_threaded)
{
   vulkan_raster_t *font = (vulkan_raster_t*)data;
   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   vkQueueWaitIdle(font->vk->context->queue);
   vulkan_destroy_texture(
         font->vk->context->device, &font->texture);
   vulkan_destroy_texture(
         font->vk->context->device, &font->texture_optimal);

   free(font);
}

static INLINE void vulkan_raster_font_update_glyph(vulkan_raster_t *font, const struct font_glyph *glyph)
{
   if(font->atlas->dirty)
   {
      unsigned row;
      for (row = glyph->atlas_offset_y; row < (glyph->atlas_offset_y + glyph->height); row++)
      {
         uint8_t *src = font->atlas->buffer + row * font->atlas->width + glyph->atlas_offset_x;
         uint8_t *dst = (uint8_t*)font->texture.mapped + row * font->texture.stride + glyph->atlas_offset_x;
         memcpy(dst, src, glyph->width);
      }

      font->atlas->dirty = false;
      font->needs_update = true;
   }
}

static int vulkan_get_message_width(void *data, const char *msg,
      unsigned msg_len, float scale)
{
   vulkan_raster_t *font = (vulkan_raster_t*)data;
   const char* msg_end   = msg + msg_len;
   int delta_x           = 0;

   if (     !font
         || !font->font_driver
         || !font->font_driver->get_glyph
         || !font->font_data )
      return 0;

   while (msg < msg_end)
   {
      uint32_t code                  = utf8_walk(&msg);
      const struct font_glyph *glyph = font->font_driver->get_glyph(
            font->font_data, code);

      if (!glyph) /* Do something smarter here ... */
         glyph = font->font_driver->get_glyph(font->font_data, '?');

      if (glyph)
      {
         vulkan_raster_font_update_glyph(font, glyph);
         delta_x += glyph->advance_x;
      }
   }

   return delta_x * scale;
}

static void vulkan_raster_font_render_line(
      vulkan_raster_t *font, const char *msg, unsigned msg_len,
      float scale, const float color[4], float pos_x,
      float pos_y, unsigned text_align)
{
   struct vk_color vk_color;
   vk_t *vk             = font->vk;
   const char* msg_end  = msg + msg_len;
   int x                = roundf(pos_x * vk->vp.width);
   int y                = roundf((1.0f - pos_y) * vk->vp.height);
   int delta_x          = 0;
   int delta_y          = 0;
   float inv_tex_size_x = 1.0f / font->texture.width;
   float inv_tex_size_y = 1.0f / font->texture.height;
   float inv_win_width  = 1.0f / font->vk->vp.width;
   float inv_win_height = 1.0f / font->vk->vp.height;

   vk_color.r           = color[0];
   vk_color.g           = color[1];
   vk_color.b           = color[2];
   vk_color.a           = color[3];

   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x -= vulkan_get_message_width(font, msg, msg_len, scale);
         break;
      case TEXT_ALIGN_CENTER:
         x -= vulkan_get_message_width(font, msg, msg_len, scale) / 2;
         break;
   }

   while (msg < msg_end)
   {
      int off_x, off_y, tex_x, tex_y, width, height;
      unsigned code                  = utf8_walk(&msg);
      const struct font_glyph *glyph =
         font->font_driver->get_glyph(font->font_data, code);

      if (!glyph) /* Do something smarter here ... */
         glyph = font->font_driver->get_glyph(font->font_data, '?');
      if (!glyph)
         continue;

      vulkan_raster_font_update_glyph(font, glyph);

      off_x  = glyph->draw_offset_x;
      off_y  = glyph->draw_offset_y;
      tex_x  = glyph->atlas_offset_x;
      tex_y  = glyph->atlas_offset_y;
      width  = glyph->width;
      height = glyph->height;

      vulkan_write_quad_vbo(font->pv + font->vertices,
            (x + (off_x + delta_x) * scale) * inv_win_width,
            (y + (off_y + delta_y) * scale) * inv_win_height,
            width * scale * inv_win_width,
            height * scale * inv_win_height,
            tex_x * inv_tex_size_x,
            tex_y * inv_tex_size_y,
            width * inv_tex_size_x,
            height * inv_tex_size_y,
            &vk_color);

      font->vertices += 6;

      delta_x        += glyph->advance_x;
      delta_y        += glyph->advance_y;
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
      if (font->vk)
         vulkan_raster_font_render_line(font, msg, strlen(msg),
               scale, color, pos_x, pos_y, text_align);
      return;
   }

   line_height = (float) font->font_driver->get_line_height(font->font_data) *
                     scale / font->vk->vp.height;

   for (;;)
   {
      const char *delim = strchr(msg, '\n');

      /* Draw the line */
      if (delim)
      {
         unsigned msg_len = delim - msg;
         if (font->vk)
            vulkan_raster_font_render_line(font, msg, msg_len,
                  scale, color, pos_x, pos_y - (float)lines * line_height,
                  text_align);
         msg += msg_len + 1;
         lines++;
      }
      else
      {
         unsigned msg_len = strlen(msg);
         if (font->vk)
            vulkan_raster_font_render_line(font, msg, msg_len,
                  scale, color, pos_x, pos_y - (float)lines * line_height,
                  text_align);
         break;
      }
   }
}

static void vulkan_raster_font_flush(vulkan_raster_t *font)
{
   struct vk_draw_triangles call;

   call.pipeline     = font->vk->pipelines.font;
   call.texture      = &font->texture_optimal;
   call.sampler      = font->vk->samplers.mipmap_linear;
   call.uniform      = &font->vk->mvp;
   call.uniform_size = sizeof(font->vk->mvp);
   call.vbo          = &font->range;
   call.vertices     = font->vertices;

   if(font->needs_update)
   {
      VkCommandBuffer staging;
      VkSubmitInfo submit_info             = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
      VkCommandBufferAllocateInfo cmd_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
      VkCommandBufferBeginInfo begin_info  = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

      cmd_info.commandPool        = font->vk->staging_pool;
      cmd_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      cmd_info.commandBufferCount = 1;
      vkAllocateCommandBuffers(font->vk->context->device, &cmd_info, &staging);

      begin_info.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      vkBeginCommandBuffer(staging, &begin_info);

      vulkan_copy_staging_to_dynamic(font->vk, staging,
            &font->texture_optimal, &font->texture);

      vkEndCommandBuffer(staging);

#ifdef HAVE_THREADS
      slock_lock(font->vk->context->queue_lock);
#endif

      submit_info.commandBufferCount = 1;
      submit_info.pCommandBuffers    = &staging;
      vkQueueSubmit(font->vk->context->queue,
            1, &submit_info, VK_NULL_HANDLE);

      vkQueueWaitIdle(font->vk->context->queue);

#ifdef HAVE_THREADS
      slock_unlock(font->vk->context->queue_lock);
#endif

      vkFreeCommandBuffers(font->vk->context->device,
            font->vk->staging_pool, 1, &staging);

      font->needs_update = false;
   }

   vulkan_draw_triangles(font->vk, &call);
}

static void vulkan_raster_font_render_msg(
      video_frame_info_t *video_info,
      void *data, const char *msg,
      const struct font_params *params)
{
   float color[4], color_dark[4];
   int drop_x, drop_y;
   bool full_screen;
   unsigned max_glyphs;
   enum text_alignment text_align;
   float x, y, scale, drop_mod, drop_alpha;
   vk_t *vk                         = NULL;
   vulkan_raster_t *font            = (vulkan_raster_t*)data;
   unsigned width                   = video_info->width;
   unsigned height                  = video_info->height;

   if (!font || !msg || !*msg)
      return;

   vk             = font->vk;

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
      drop_alpha  = params->drop_alpha;

      color[0]    = FONT_COLOR_GET_RED(params->color)   / 255.0f;
      color[1]    = FONT_COLOR_GET_GREEN(params->color) / 255.0f;
      color[2]    = FONT_COLOR_GET_BLUE(params->color)  / 255.0f;
      color[3]    = FONT_COLOR_GET_ALPHA(params->color) / 255.0f;

      /* If alpha is 0.0f, turn it into default 1.0f */
      if (color[3] <= 0.0f)
         color[3] = 1.0f;
   }
   else
   {
      x           = video_info->font_msg_pos_x;
      y           = video_info->font_msg_pos_y;
      scale       = 1.0f;
      full_screen = true;
      text_align  = TEXT_ALIGN_LEFT;
      drop_x      = -2;
      drop_y      = -2;
      drop_mod    = 0.3f;
      drop_alpha  = 1.0f;

      color[0]    = video_info->font_msg_color_r;
      color[1]    = video_info->font_msg_color_g;
      color[2]    = video_info->font_msg_color_b;
      color[3]    = 1.0f;
   }

   video_driver_set_viewport(width, height, full_screen, false);

   max_glyphs = strlen(msg);
   if (drop_x || drop_y)
      max_glyphs *= 2;

   if (!vulkan_buffer_chain_alloc(font->vk->context, &font->vk->chain->vbo,
         6 * sizeof(struct vk_vertex) * max_glyphs, &font->range))
      return;

   font->vertices   = 0;
   font->pv         = (struct vk_vertex*)font->range.data;

   if (drop_x || drop_y)
   {
      color_dark[0] = color[0] * drop_mod;
      color_dark[1] = color[1] * drop_mod;
      color_dark[2] = color[2] * drop_mod;
      color_dark[3] = color[3] * drop_alpha;

      vulkan_raster_font_render_message(font, msg, scale, color_dark,
            x + scale * drop_x / vk->vp.width, y +
            scale * drop_y / vk->vp.height, text_align);
   }

   vulkan_raster_font_render_message(font, msg, scale,
         color, x, y, text_align);
   vulkan_raster_font_flush(font);
}

static const struct font_glyph *vulkan_raster_font_get_glyph(
      void *data, uint32_t code)
{
   const struct font_glyph* glyph;
   vulkan_raster_t *font = (vulkan_raster_t*)data;

   if (!font || !font->font_driver)
      return NULL;
   if (!font->font_driver->ident)
       return NULL;

   glyph = font->font_driver->get_glyph((void*)font->font_driver, code);

   if(glyph)
      vulkan_raster_font_update_glyph(font, glyph);

   return glyph;
}

static int vulkan_get_line_height(void *data)
{
   vulkan_raster_t *font = (vulkan_raster_t*)data;

   if (!font || !font->font_driver || !font->font_data)
      return -1;

   return font->font_driver->get_line_height(font->font_data);
}

font_renderer_t vulkan_raster_font = {
   vulkan_raster_font_init_font,
   vulkan_raster_font_free_font,
   vulkan_raster_font_render_msg,
   "Vulkan raster",
   vulkan_raster_font_get_glyph,
   NULL,                            /* bind_block */
   NULL,                            /* flush_block */
   vulkan_get_message_width,
   vulkan_get_line_height
};

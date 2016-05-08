/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016 - Hans-Kristian Arntzen
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

#include <retro_miscellaneous.h>

#include "../../config.def.h"
#include "../../gfx/font_driver.h"
#include "../../gfx/video_context_driver.h"
#include "../../gfx/common/vulkan_common.h"
#include "../../gfx/video_shader_driver.h"

#include "../menu_display.h"

static const float vk_vertexes[] = {
   0, 0,
   0, 1,
   1, 0,
   1, 1
};

static const float vk_tex_coords[] = {
   0, 0,
   0, 1,
   1, 0,
   1, 1
};

static void *menu_display_vk_get_default_mvp(void)
{
   vk_t *vk = (vk_t*)video_driver_get_ptr(false);
   if (!vk)
      return NULL;
   return &vk->mvp_no_rot;
}

static const float *menu_display_vk_get_default_vertices(void)
{
   return &vk_vertexes[0];
}

static const float *menu_display_vk_get_default_tex_coords(void)
{
   return &vk_tex_coords[0];
}

static unsigned to_display_pipeline(
      enum menu_display_prim_type type, bool blend)
{
   return ((type == MENU_DISPLAY_PRIM_TRIANGLESTRIP) << 1) | (blend << 0);
}

static unsigned to_menu_pipeline(
      enum menu_display_prim_type type, unsigned pipeline)
{
   switch (pipeline)
   {
      case VIDEO_SHADER_MENU:
         return 4 + (type == MENU_DISPLAY_PRIM_TRIANGLESTRIP);
      case VIDEO_SHADER_MENU_SEC:
         return 6 + (type == MENU_DISPLAY_PRIM_TRIANGLESTRIP);
      default:
         return 0;
   }
}

static void menu_display_vk_viewport(void *data)
{
   menu_display_ctx_draw_t *draw = (menu_display_ctx_draw_t*)data;
   vk_t *vk                      = (vk_t*)video_driver_get_ptr(false);

   if (!vk || !draw)
      return;

   vk->vk_vp.x        = draw->x;
   vk->vk_vp.y        = vk->context->swapchain_height - draw->y - draw->height;
   vk->vk_vp.width    = draw->width;
   vk->vk_vp.height   = draw->height;
   vk->vk_vp.minDepth = 0.0f;
   vk->vk_vp.maxDepth = 1.0f;
}

static void menu_display_vk_draw_pipeline(void *data)
{
   menu_display_ctx_draw_t *draw = (menu_display_ctx_draw_t*)data;
   vk_t *vk                      = (vk_t*)video_driver_get_ptr(false);
   gfx_coord_array_t *ca         = NULL;
   static float t                = 0.0f;

   if (!vk || !draw)
      return;

   ca = menu_display_get_coords_array();
   draw->x                     = 0;
   draw->y                     = 0;
   draw->coords                = (struct gfx_coords*)&ca->coords;
   draw->matrix_data           = NULL;
   draw->pipeline.backend_data = &t;

   t += 0.01;
}

static void menu_display_vk_draw(void *data)
{
   unsigned i;
   struct vk_buffer_range range;
   struct vk_texture *texture    = NULL;
   const float *vertex           = NULL;
   const float *tex_coord        = NULL;
   const float *color            = NULL;
   struct vk_vertex *pv          = NULL;
   menu_display_ctx_draw_t *draw = (menu_display_ctx_draw_t*)data;
   vk_t *vk                      = (vk_t*)video_driver_get_ptr(false);

   if (!vk || !draw)
      return;

   texture            = (struct vk_texture*)draw->texture;
   vertex             = draw->coords->vertex;
   tex_coord          = draw->coords->tex_coord;
   color              = draw->coords->color;

   if (!vertex)
      vertex          = menu_display_vk_get_default_vertices();
   if (!tex_coord)
      tex_coord       = menu_display_vk_get_default_tex_coords();
   if (!draw->coords->lut_tex_coord)
      draw->coords->lut_tex_coord = menu_display_vk_get_default_tex_coords();
   if (!texture)
      texture         = &vk->display.blank_texture;

   menu_display_vk_viewport(draw);
   vk->tracker.dirty |= VULKAN_DIRTY_DYNAMIC_BIT;

   /* Bake interleaved VBO. Kinda ugly, we should probably try to move to
    * an interleaved model to begin with ... */
   if (!vulkan_buffer_chain_alloc(vk->context, &vk->chain->vbo,
         draw->coords->vertices * sizeof(struct vk_vertex), &range))
      return;

   pv = (struct vk_vertex*)range.data;
   for (i = 0; i < draw->coords->vertices; i++, pv++)
   {
      pv->x       = *vertex++;
      pv->y       = *vertex++;
      pv->tex_x   = *tex_coord++;
      pv->tex_y   = *tex_coord++;
      pv->color.r = *color++;
      pv->color.g = *color++;
      pv->color.b = *color++;
      pv->color.a = *color++;
   }

   switch (draw->pipeline.id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_SEC:
      {
         const struct vk_draw_triangles call = {
            vk->display.pipelines[
               to_menu_pipeline(draw->prim_type, draw->pipeline.id)],
               NULL,
               VK_NULL_HANDLE,
               draw->pipeline.backend_data,
               sizeof(float),
               &range,
               draw->coords->vertices,
         };
         vulkan_draw_triangles(vk, &call);
         break;
      }

      default:
      {
         const struct vk_draw_triangles call = {
            vk->display.pipelines[
               to_display_pipeline(draw->prim_type, vk->display.blend)],
               texture,
               texture->default_smooth 
                  ? vk->samplers.linear : vk->samplers.nearest,
               draw->matrix_data
                  ? draw->matrix_data : menu_display_vk_get_default_mvp(),
               sizeof(math_matrix_4x4),
               &range,
               draw->coords->vertices,
         };
         vulkan_draw_triangles(vk, &call);
         break;
      }
   }
}


static void menu_display_vk_restore_clear_color(void)
{
}

static void menu_display_vk_clear_color(menu_display_ctx_clearcolor_t *clearcolor)
{
   VkClearRect rect;
   VkClearAttachment attachment;
   vk_t *vk                      = (vk_t*)video_driver_get_ptr(false);
   if (!vk || !clearcolor)
      return;

   attachment.aspectMask                  = VK_IMAGE_ASPECT_COLOR_BIT;
   attachment.clearValue.color.float32[0] = clearcolor->r;
   attachment.clearValue.color.float32[1] = clearcolor->g;
   attachment.clearValue.color.float32[2] = clearcolor->b;
   attachment.clearValue.color.float32[3] = clearcolor->a;

   memset(&rect, 0, sizeof(rect));
   rect.rect.extent.width  = vk->context->swapchain_width;
   rect.rect.extent.height = vk->context->swapchain_height;
   rect.layerCount         = 1;

   VKFUNC(vkCmdClearAttachments)(vk->cmd, 1, &attachment, 1, &rect);
}

static void menu_display_vk_blend_begin(void)
{
   vk_t *vk = (vk_t*)video_driver_get_ptr(false);
   vk->display.blend = true;
}

static void menu_display_vk_blend_end(void)
{
   vk_t *vk = (vk_t*)video_driver_get_ptr(false);
   vk->display.blend = false;
}

static bool menu_display_vk_font_init_first(
      void **font_handle, void *video_data, const char *font_path,
      float font_size)
{
   return font_driver_init_first(NULL, font_handle, video_data,
         font_path, font_size, true, FONT_DRIVER_RENDER_VULKAN_API);
}

menu_display_ctx_driver_t menu_display_ctx_vulkan = {
   menu_display_vk_draw,
   menu_display_vk_draw_pipeline,
   menu_display_vk_viewport,
   menu_display_vk_blend_begin,
   menu_display_vk_blend_end,
   menu_display_vk_restore_clear_color,
   menu_display_vk_clear_color,
   menu_display_vk_get_default_mvp,
   menu_display_vk_get_default_vertices,
   menu_display_vk_get_default_tex_coords,
   menu_display_vk_font_init_first,
   MENU_VIDEO_DRIVER_VULKAN,
   "menu_display_vulkan",
};

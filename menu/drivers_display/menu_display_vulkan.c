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

static vk_t *vk_get_ptr(void)
{
   vk_t *vk = (vk_t*)video_driver_get_ptr(false);
   if (!vk)
      return NULL;
   return vk;
}

static void *menu_display_vk_get_default_mvp(void)
{
   vk_t *vk = vk_get_ptr();
   if (!vk)
      return NULL;
   return &vk->mvp_no_rot;
}

static unsigned to_display_pipeline(
      enum menu_display_prim_type type, bool blend)
{
   return ((type == MENU_DISPLAY_PRIM_TRIANGLESTRIP) << 1) | (blend << 0);
}

static void menu_display_vk_draw(void *data)
{
   unsigned i;
   struct vk_buffer_range range;
   struct vk_texture *texture    = NULL;
   const float *vertex           = NULL;
   const float *tex_coord        = NULL;
   const float *color            = NULL;
   math_matrix_4x4 *mat          = NULL;
   struct vk_vertex *pv          = NULL;
   menu_display_ctx_draw_t *draw = (menu_display_ctx_draw_t*)data;
   vk_t *vk                      = vk_get_ptr();
   if (!vk)
      return;

   texture            = (struct vk_texture*)draw->texture;
   mat                = (math_matrix_4x4*)draw->matrix_data;
   vertex             = draw->coords->vertex;
   tex_coord          = draw->coords->tex_coord;
   color              = draw->coords->color;

   /* TODO - edge case */
   if (draw->height <= 0)
      draw->height    = 1;

   if (!mat)
      mat             = (math_matrix_4x4*)menu_display_vk_get_default_mvp();
   if (!vertex)
      vertex          = &vk_vertexes[0];
   if (!tex_coord)
      tex_coord       = &vk_tex_coords[0];
   if (!texture)
      texture         = &vk->display.blank_texture;

   vk->vk_vp.x        = draw->x;
   vk->vk_vp.y        = vk->context->swapchain_height - draw->y - draw->height;
   vk->vk_vp.width    = draw->width;
   vk->vk_vp.height   = draw->height;
   vk->vk_vp.minDepth = 0.0f;
   vk->vk_vp.maxDepth = 1.0f;
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

   {
      const struct vk_draw_triangles call = {
         vk->display.pipelines[
            to_display_pipeline(draw->prim_type, vk->display.blend)],
         texture,
         texture->default_smooth 
            ? vk->samplers.linear : vk->samplers.nearest,
         mat,
         &range,
         draw->coords->vertices,
      };
      vulkan_draw_triangles(vk, &call);
   }
}

static void menu_display_vk_draw_bg(void *data)
{
   struct gfx_coords coords;
   const float *new_vertex       = NULL;
   const float *new_tex_coord    = NULL;
   menu_display_ctx_draw_t *draw = (menu_display_ctx_draw_t*)data;
   global_t     *global          = global_get_ptr();
   settings_t *settings          = config_get_ptr();
   vk_t             *vk          = vk_get_ptr();

   if (!vk || !draw)
      return;

   if (!new_vertex)
      new_vertex = &vk_vertexes[0];
   if (!new_tex_coord)
      new_tex_coord = &vk_tex_coords[0];

   coords.vertices      = draw->vertex_count;
   coords.vertex        = new_vertex;
   coords.tex_coord     = new_tex_coord;
   coords.color         = (const float*)draw->color;

   vk->display.blend = true;

   menu_display_ctl(MENU_DISPLAY_CTL_SET_VIEWPORT, NULL);

   if (
         (settings->menu.pause_libretro
          || !rarch_ctl(RARCH_CTL_IS_INITED, NULL) 
          || rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL)
         )
      && !draw->force_transparency && draw->texture)
      coords.color = (const float*)draw->color2;

   draw->x           = 0;
   draw->y           = 0;
   draw->coords      = &coords;
   draw->matrix_data = (math_matrix_4x4*)
      menu_display_vk_get_default_mvp();

   menu_display_vk_draw(draw);

   vk->display.blend = false;
}

static void menu_display_vk_restore_clear_color(void)
{
}

static void menu_display_vk_clear_color(void *data)
{
   VkClearRect rect;
   VkClearAttachment attachment;
   menu_display_ctx_clearcolor_t *clearcolor =
      (menu_display_ctx_clearcolor_t*)data;
   struct vulkan_context_fp *vkcfp           = NULL;
   vk_t *vk = vk_get_ptr();
   if (!vk || !clearcolor)
      return;

   vkcfp = &vk->context->fp;

   if (!vkcfp)
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

static const float *menu_display_vk_get_tex_coords(void)
{
   return &vk_tex_coords[0];
}

static void menu_display_vk_blend_begin(void)
{
   vk_t *vk = vk_get_ptr();
   vk->display.blend = true;
}

static void menu_display_vk_blend_end(void)
{
   vk_t *vk = vk_get_ptr();
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
   menu_display_vk_draw_bg,
   menu_display_vk_blend_begin,
   menu_display_vk_blend_end,
   menu_display_vk_restore_clear_color,
   menu_display_vk_clear_color,
   menu_display_vk_get_default_mvp,
   menu_display_vk_get_tex_coords,
   menu_display_vk_font_init_first,
   MENU_VIDEO_DRIVER_VULKAN,
   "menu_display_vulkan",
};

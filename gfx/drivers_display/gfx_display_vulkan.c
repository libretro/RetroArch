/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016-2017 - Hans-Kristian Arntzen
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

#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../gfx_display.h"

#include "../common/vulkan_common.h"

/* Will do Y-flip later, but try to make it similar to GL. */
static const float vk_vertexes[8] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const float vk_tex_coords[8] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static const float vk_colors[16] = {
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
};

static void *gfx_display_vk_get_default_mvp(void *data)
{
   vk_t *vk = (vk_t*)data;
   if (!vk)
      return NULL;
   return &vk->mvp_no_rot;
}

static const float *gfx_display_vk_get_default_vertices(void)
{
   return &vk_vertexes[0];
}

static const float *gfx_display_vk_get_default_tex_coords(void)
{
   return &vk_tex_coords[0];
}

#ifdef HAVE_SHADERPIPELINE
static unsigned to_menu_pipeline(
      enum gfx_display_prim_type type, unsigned pipeline)
{
   switch (pipeline)
   {
      case VIDEO_SHADER_MENU:
         return 6 + (type == GFX_DISPLAY_PRIM_TRIANGLESTRIP);
      case VIDEO_SHADER_MENU_2:
         return 8 + (type == GFX_DISPLAY_PRIM_TRIANGLESTRIP);
      case VIDEO_SHADER_MENU_3:
         return 10 + (type == GFX_DISPLAY_PRIM_TRIANGLESTRIP);
      case VIDEO_SHADER_MENU_4:
         return 12 + (type == GFX_DISPLAY_PRIM_TRIANGLESTRIP);
      case VIDEO_SHADER_MENU_5:
         return 14 + (type == GFX_DISPLAY_PRIM_TRIANGLESTRIP);
      default:
         break;
   }
   return 0;
}

static void gfx_display_vk_draw_pipeline(
      gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp,
      void *data, unsigned video_width, unsigned video_height)
{
   static uint8_t ubo_scratch_data[768];
   static struct video_coords blank_coords;
   static float t                   = 0.0f;
   float output_size[2];
   float yflip                      = 1.0f;
   video_coord_array_t *ca          = NULL;
   vk_t *vk                         = (vk_t*)data;

   if (!vk || !draw)
      return;

   draw->x                          = 0;
   draw->y                          = 0;
   draw->matrix_data                = NULL;

   output_size[0]                   = (float)vk->context->swapchain_width;
   output_size[1]                   = (float)vk->context->swapchain_height;

   switch (draw->pipeline_id)
   {
      /* Ribbon */
      default:
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
         ca                               = &p_disp->dispca;
         draw->coords                     = (struct video_coords*)&ca->coords;
         draw->backend_data               = ubo_scratch_data;
         draw->backend_data_size          = 2 * sizeof(float);

         /* Match UBO layout in shader. */
         memcpy(ubo_scratch_data, &t, sizeof(t));
         memcpy(ubo_scratch_data + sizeof(float), &yflip, sizeof(yflip));
         break;

      /* Snow simple */
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
         draw->backend_data               = ubo_scratch_data;
         draw->backend_data_size          = sizeof(math_matrix_4x4) 
            + 4 * sizeof(float);

         /* Match UBO layout in shader. */
         memcpy(ubo_scratch_data,
               &vk->mvp_no_rot,
               sizeof(math_matrix_4x4));
         memcpy(ubo_scratch_data + sizeof(math_matrix_4x4),
               output_size,
               sizeof(output_size));

         /* Shader uses FragCoord, need to fix up. */
         if (draw->pipeline_id == VIDEO_SHADER_MENU_5)
            yflip = -1.0f;

         memcpy(ubo_scratch_data + sizeof(math_matrix_4x4) 
               + 2 * sizeof(float), &t, sizeof(t));
         memcpy(ubo_scratch_data + sizeof(math_matrix_4x4) 
               + 3 * sizeof(float), &yflip, sizeof(yflip));
         draw->coords          = &blank_coords;
         blank_coords.vertices = 4;
         draw->prim_type       = GFX_DISPLAY_PRIM_TRIANGLESTRIP;
         break;
   }

   t += 0.01;
}
#endif

static void gfx_display_vk_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   unsigned i;
   struct vk_buffer_range range;
   struct vk_texture *texture    = NULL;
   const float *vertex           = NULL;
   const float *tex_coord        = NULL;
   const float *color            = NULL;
   struct vk_vertex *pv          = NULL;
   vk_t *vk                      = (vk_t*)data;

   if (!vk || !draw)
      return;

   texture                        = (struct vk_texture*)draw->texture;
   vertex                         = draw->coords->vertex;
   tex_coord                      = draw->coords->tex_coord;
   color                          = draw->coords->color;

   if (!vertex)
      vertex                      = &vk_vertexes[0];
   if (!tex_coord)
      tex_coord                   = &vk_tex_coords[0];
   if (!draw->coords->lut_tex_coord)
      draw->coords->lut_tex_coord = &vk_tex_coords[0];
   if (!texture)
      texture                     = &vk->display.blank_texture;
   if (!color)
      color                       = &vk_colors[0];

   vk->vk_vp.x                    = draw->x;
   vk->vk_vp.y                    = vk->context->swapchain_height - draw->y - draw->height;
   vk->vk_vp.width                = draw->width;
   vk->vk_vp.height               = draw->height;
   vk->vk_vp.minDepth             = 0.0f;
   vk->vk_vp.maxDepth             = 1.0f;

   vk->tracker.dirty             |= VULKAN_DIRTY_DYNAMIC_BIT;

   /* Bake interleaved VBO. Kinda ugly, we should probably try to move to
    * an interleaved model to begin with ... */
   if (!vulkan_buffer_chain_alloc(vk->context, &vk->chain->vbo,
            draw->coords->vertices * sizeof(struct vk_vertex), &range))
      return;

   pv = (struct vk_vertex*)range.data;
   for (i = 0; i < draw->coords->vertices; i++, pv++)
   {
      pv->x       = *vertex++;
      /* Y-flip. Vulkan is top-left clip space */
      pv->y       = 1.0f - (*vertex++);
      pv->tex_x   = *tex_coord++;
      pv->tex_y   = *tex_coord++;
      pv->color.r = *color++;
      pv->color.g = *color++;
      pv->color.b = *color++;
      pv->color.a = *color++;
   }

   switch (draw->pipeline_id)
   {
#ifdef HAVE_SHADERPIPELINE
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
         {
            struct vk_draw_triangles call;

            call.pipeline     = vk->display.pipelines[
               to_menu_pipeline(draw->prim_type, draw->pipeline_id)];
            call.texture      = NULL;
            call.sampler      = VK_NULL_HANDLE;
            call.uniform      = draw->backend_data;
            call.uniform_size = draw->backend_data_size;
            call.vbo          = &range;
            call.vertices     = draw->coords->vertices;

            vulkan_draw_triangles(vk, &call);
         }
         break;
#endif

      default:
         {
            struct vk_draw_triangles call;
            unsigned 
               disp_pipeline  = 
                 ((draw->prim_type == GFX_DISPLAY_PRIM_TRIANGLESTRIP) << 1)
               | (((vk->flags & VK_FLAG_DISPLAY_BLEND) > 0) << 0);
            call.pipeline     = vk->display.pipelines[disp_pipeline];
            call.texture      = texture;
            call.sampler      = (texture->flags & VK_TEX_FLAG_MIPMAP) ?
               vk->samplers.mipmap_linear :
               ((texture->flags & VK_TEX_FLAG_DEFAULT_SMOOTH) ? vk->samplers.linear
                : vk->samplers.nearest);
            call.uniform      = draw->matrix_data
               ? draw->matrix_data : &vk->mvp_no_rot;
            call.uniform_size = sizeof(math_matrix_4x4);
            call.vbo          = &range;
            call.vertices     = draw->coords->vertices;

            vulkan_draw_triangles(vk, &call);
         }
         break;
   }
}

static void gfx_display_vk_blend_begin(void *data)
{
   vk_t *vk = (vk_t*)data;

   if (vk)
      vk->flags |=  VK_FLAG_DISPLAY_BLEND;
}

static void gfx_display_vk_blend_end(void *data)
{
   vk_t *vk = (vk_t*)data;

   if (vk)
      vk->flags &= ~VK_FLAG_DISPLAY_BLEND;
}

static void gfx_display_vk_scissor_begin(
      void *data,
      unsigned video_width,
      unsigned video_height,
      int x, int y, unsigned width, unsigned height)
{
   vk_t *vk                          = (vk_t*)data;

   vk->tracker.scissor.offset.x      = x;
   vk->tracker.scissor.offset.y      = y;
   vk->tracker.scissor.extent.width  = width;
   vk->tracker.scissor.extent.height = height;
   vk->flags                        |= VK_FLAG_TRACKER_USE_SCISSOR;
   vk->tracker.dirty                |= VULKAN_DIRTY_DYNAMIC_BIT;
}

static void gfx_display_vk_scissor_end(void *data,
      unsigned video_width,
      unsigned video_height)
{
   vk_t *vk                 = (vk_t*)data;

   vk->flags               &= ~VK_FLAG_TRACKER_USE_SCISSOR;
   vk->tracker.dirty       |=  VULKAN_DIRTY_DYNAMIC_BIT;
}

gfx_display_ctx_driver_t gfx_display_ctx_vulkan = {
   gfx_display_vk_draw,
#ifdef HAVE_SHADERPIPELINE
   gfx_display_vk_draw_pipeline,
#else
   NULL,                                  /* draw_pipeline */
#endif
   gfx_display_vk_blend_begin,
   gfx_display_vk_blend_end,
   gfx_display_vk_get_default_mvp,
   gfx_display_vk_get_default_vertices,
   gfx_display_vk_get_default_tex_coords,
   FONT_DRIVER_RENDER_VULKAN_API,
   GFX_VIDEO_DRIVER_VULKAN,
   "vulkan",
   false,
   gfx_display_vk_scissor_begin,
   gfx_display_vk_scissor_end
};

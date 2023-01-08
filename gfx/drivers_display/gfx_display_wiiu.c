/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel
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

#include <wiiu/gx2.h>
#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../gfx_display.h"

#include "../common/gx2_common.h"
#include "../../wiiu/system/memory.h"
#include "../../wiiu/wiiu_dbg.h"

static void gfx_display_wiiu_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   wiiu_video_t             *wiiu  = (wiiu_video_t*)data;

   if (!wiiu || !draw)
      return;

   if (draw->pipeline_id)
   {
      GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);

      switch(draw->pipeline_id)
      {
         case VIDEO_SHADER_MENU:
            GX2SetShader(&ribbon_shader);
            break;
         case VIDEO_SHADER_MENU_2:
            GX2SetShader(&ribbon_simple_shader);
            break;
         case VIDEO_SHADER_MENU_3:
            GX2SetShader(&snow_simple_shader);
            break;
         case VIDEO_SHADER_MENU_4:
            GX2SetShader(&snow_shader);
            break;
         case VIDEO_SHADER_MENU_5:
            GX2SetShader(&bokeh_shader);
            break;
         case VIDEO_SHADER_MENU_6:
            GX2SetShader(&snowflake_shader);
            break;
         default:
            break;
      }

      switch(draw->pipeline_id)
      {
         case VIDEO_SHADER_MENU:
         case VIDEO_SHADER_MENU_2:
            GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLE_STRIP, draw->coords->vertices, 0, 1);
            GX2SetBlendControl(GX2_RENDER_TARGET_0, GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA,
                  GX2_BLEND_COMBINE_MODE_ADD,
                  GX2_ENABLE,          GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA,
                  GX2_BLEND_COMBINE_MODE_ADD);
         case VIDEO_SHADER_MENU_3:
         case VIDEO_SHADER_MENU_4:
         case VIDEO_SHADER_MENU_5:
         case VIDEO_SHADER_MENU_6:
            GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
            break;
      }

   }
   /* TODO come up with a better check for "not all vertexes are the same color" */
   else if (draw->coords->vertex || draw->coords->color[0] != draw->coords->color[12])
   {
      int i;
      if (wiiu->vertex_cache_tex.current + 4 > wiiu->vertex_cache_tex.size)
         return;

      tex_shader_vertex_t* v = wiiu->vertex_cache_tex.v + wiiu->vertex_cache_tex.current;

      GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);
      GX2SetShader(&tex_shader);
      GX2SetVertexUniformBlock(tex_shader.vs.uniformBlocks[0].offset,
            tex_shader.vs.uniformBlocks[0].size,
            wiiu->ubo_mvp);
      GX2SetAttribBuffer(0, wiiu->vertex_cache_tex.size * sizeof(*wiiu->vertex_cache_tex.v),
            sizeof(*wiiu->vertex_cache_tex.v), wiiu->vertex_cache_tex.v);

      if (!draw->coords->vertex)
      {
         /* Convert the libretro bottom-up coordinate system to GX2 - low y at
            the top of the screen, large y at the bottom
            The compiler will optimise 90% of this out anyway */
         float y = -(draw->y + draw->height - video_height);
         /* Remember: this is a triangle strip, not a quad, draw in a Z shape
            Bottom-left, right, top-left, right */
         v[0].pos.x = (draw->x               ) / video_width;
         v[0].pos.y = (y       + draw->height) / video_height;
         v[1].pos.x = (draw->x + draw->width ) / video_width;
         v[1].pos.y = (y       + draw->height) / video_height;
         v[2].pos.x = (draw->x               ) / video_width;
         v[2].pos.y = (y                     ) / video_height;
         v[3].pos.x = (draw->x + draw->width ) / video_width;
         v[3].pos.y = (y                     ) / video_height;
      }
      else
      {
         v[0].pos.x = draw->coords->vertex[0];
         v[0].pos.y = 1.0 - draw->coords->vertex[1];
         v[1].pos.x = draw->coords->vertex[2];
         v[1].pos.y = 1.0 - draw->coords->vertex[3];
         v[2].pos.x = draw->coords->vertex[4];
         v[2].pos.y = 1.0 - draw->coords->vertex[5];
         v[3].pos.x = draw->coords->vertex[6];
         v[3].pos.y = 1.0 - draw->coords->vertex[7];
      }

      if (!draw->coords->tex_coord)
      {
         v[0].coord.u = 0.0f;
         v[0].coord.v = 1.0f;
         v[1].coord.u = 1.0f;
         v[1].coord.v = 1.0f;
         v[2].coord.u = 0.0f;
         v[2].coord.v = 0.0f;
         v[3].coord.u = 1.0f;
         v[3].coord.v = 0.0f;
      }
      else
      {
         v[0].coord.u = draw->coords->tex_coord[0];
         v[0].coord.v = draw->coords->tex_coord[1];
         v[1].coord.u = draw->coords->tex_coord[2];
         v[1].coord.v = draw->coords->tex_coord[3];
         v[2].coord.u = draw->coords->tex_coord[4];
         v[2].coord.v = draw->coords->tex_coord[5];
         v[3].coord.u = draw->coords->tex_coord[6];
         v[3].coord.v = draw->coords->tex_coord[7];
      }

      for (i = 0; i < 4; i++)
      {
         v[i].color.r = draw->coords->color[(i << 2) + 0];
         v[i].color.g = draw->coords->color[(i << 2) + 1];
         v[i].color.b = draw->coords->color[(i << 2) + 2];
         v[i].color.a = draw->coords->color[(i << 2) + 3];
      }

      if (draw->texture)
         GX2SetPixelTexture((GX2Texture*)draw->texture, tex_shader.ps.samplerVars[0].location);

      GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLE_STRIP, 4, wiiu->vertex_cache_tex.current, 1);
      wiiu->vertex_cache_tex.current += 4;
   }
   else
   {
      sprite_vertex_t* v;
      if (wiiu->vertex_cache.current + 1 > wiiu->vertex_cache.size)
         return;

      v                  = wiiu->vertex_cache.v + wiiu->vertex_cache.current;
      v->pos.x           = draw->x;
      v->pos.y           = wiiu->color_buffer.surface.height - 
                           draw->y - draw->height;
      v->pos.width       = draw->width;
      v->pos.height      = draw->height;
      v->coord.u         = 0.0f;
      v->coord.v         = 0.0f;
      v->coord.width     = 1.0f;
      v->coord.height    = 1.0f;

      v->color           = COLOR_RGBA(
            0xFF * draw->coords->color[0], 0xFF * draw->coords->color[1],
            0xFF * draw->coords->color[2], 0xFF * draw->coords->color[3]);

      if (draw->texture)
         GX2SetPixelTexture((GX2Texture*)draw->texture, sprite_shader.ps.samplerVars[0].location);

      GX2DrawEx(GX2_PRIMITIVE_MODE_POINTS, 1, wiiu->vertex_cache.current, 1);
      wiiu->vertex_cache.current ++;
      return;
   }

   GX2SetShaderMode(GX2_SHADER_MODE_GEOMETRY_SHADER);
   GX2SetShader(&sprite_shader);
#if 0
   GX2SetGeometryShaderInputRingBuffer(wiiu->input_ring_buffer,
         wiiu->input_ring_buffer_size);
   GX2SetGeometryShaderOutputRingBuffer(wiiu->output_ring_buffer,
         wiiu->output_ring_buffer_size);
#endif
   GX2SetVertexUniformBlock(sprite_shader.vs.uniformBlocks[0].offset,
         sprite_shader.vs.uniformBlocks[0].size,
         wiiu->ubo_vp);
   GX2SetVertexUniformBlock(sprite_shader.vs.uniformBlocks[1].offset,
         sprite_shader.vs.uniformBlocks[1].size,
         wiiu->ubo_tex);
   GX2SetAttribBuffer(0, wiiu->vertex_cache.size 
         * sizeof(*wiiu->vertex_cache.v),
         sizeof(*wiiu->vertex_cache.v),
         wiiu->vertex_cache.v);
}

static void gfx_display_wiiu_draw_pipeline(
      gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp,
      void *data, unsigned video_width, unsigned video_height)
{
   video_coord_array_t *ca        = NULL;
   wiiu_video_t             *wiiu = (wiiu_video_t*)data;

   if (!wiiu || !draw)
      return;

   switch(draw->pipeline_id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
         ca = &p_disp->dispca;
         if (!wiiu->menu_shader_vbo)
         {
            wiiu->menu_shader_vbo = MEM2_alloc(ca->coords.vertices * 2 * sizeof(float), GX2_VERTEX_BUFFER_ALIGNMENT);
            memcpy(wiiu->menu_shader_vbo, ca->coords.vertex, ca->coords.vertices * 2 * sizeof(float));
            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, wiiu->menu_shader_vbo, ca->coords.vertices * 2 * sizeof(float));
         }

         draw->coords->vertex             = wiiu->menu_shader_vbo;
         draw->coords->vertices           = ca->coords.vertices;
         GX2SetAttribBuffer(0,
               draw->coords->vertices * 2 * sizeof(float),
               2 * sizeof(float), wiiu->menu_shader_vbo);
         GX2SetBlendControl(GX2_RENDER_TARGET_0,
               GX2_BLEND_MODE_SRC_ALPHA,
               GX2_BLEND_MODE_ONE,
               GX2_BLEND_COMBINE_MODE_ADD,
               GX2_DISABLE, 0, 0, 0);

         break;
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         GX2SetAttribBuffer(0,
               4 * sizeof(*wiiu->v),
               sizeof(*wiiu->v), wiiu->v);
         break;
      default:
         return;
   }

   if (!wiiu->menu_shader_ubo)
   {
      wiiu->menu_shader_ubo = MEM2_alloc(
            sizeof(*wiiu->menu_shader_ubo),
            GX2_UNIFORM_BLOCK_ALIGNMENT);
      matrix_4x4_ortho(wiiu->menu_shader_ubo->mvp, 0, 1, 1, 0, -1, 1);
      wiiu->menu_shader_ubo->OutputSize.width = wiiu->color_buffer.surface.width;
      wiiu->menu_shader_ubo->OutputSize.height = wiiu->color_buffer.surface.height;
      wiiu->menu_shader_ubo->time = 0.0f;
   }
   else
      wiiu->menu_shader_ubo->time += 0.01f;

   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_UNIFORM_BLOCK, wiiu->menu_shader_ubo, sizeof(*wiiu->menu_shader_ubo));
   GX2SetVertexUniformBlock(1, sizeof(*wiiu->menu_shader_ubo), wiiu->menu_shader_ubo);
   GX2SetPixelUniformBlock(1, sizeof(*wiiu->menu_shader_ubo), wiiu->menu_shader_ubo);
}

static void gfx_display_wiiu_scissor_begin(
      void *data,
      unsigned video_width,
      unsigned video_height,
      int x, int y,
      unsigned width, unsigned height)
{
   GX2SetScissor(MAX(x, 0), MAX(y, 0), MIN(width, video_width), MIN(height, video_height));
}

static void gfx_display_wiiu_scissor_end(
      void *data,
      unsigned video_width,
      unsigned video_height
      )
{
   GX2SetScissor(0, 0, video_width, video_height);
}

gfx_display_ctx_driver_t gfx_display_ctx_wiiu = {
   gfx_display_wiiu_draw,
   gfx_display_wiiu_draw_pipeline,
   NULL,                                     /* blend_begin            */
   NULL,                                     /* blend_end              */
   NULL,                                     /* get_default_mvp        */
   NULL,                                     /* get_default_vertices   */
   NULL,                                     /* get_default_tex_coords */
   FONT_DRIVER_RENDER_WIIU,
   GFX_VIDEO_DRIVER_WIIU,
   "gx2",
   true,
   gfx_display_wiiu_scissor_begin,
   gfx_display_wiiu_scissor_end
};

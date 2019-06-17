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
#include "config.h"
#endif

#include "../menu_driver.h"

#include "../../retroarch.h"
#include "../../gfx/font_driver.h"
#include "../../gfx/common/gx2_common.h"
#include "../../wiiu/system/memory.h"
#include "../../wiiu/wiiu_dbg.h"

static const float *menu_display_wiiu_get_default_vertices(void)
{
   return NULL;
}

static const float *menu_display_wiiu_get_default_tex_coords(void)
{
   return NULL;
}

static void *menu_display_wiiu_get_default_mvp(video_frame_info_t *video_info)
{
   return NULL;
}

static void menu_display_wiiu_blend_begin(video_frame_info_t *video_info)
{

}

static void menu_display_wiiu_blend_end(video_frame_info_t *video_info)
{

}

static void menu_display_wiiu_viewport(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{

}

static void menu_display_wiiu_draw(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
   wiiu_video_t             *wiiu  = (wiiu_video_t*)video_info->userdata;

   if (!wiiu || !draw)
      return;

   if(draw->pipeline.id)
   {
      GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);

      switch(draw->pipeline.id)
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

      switch(draw->pipeline.id)
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
   else if(draw->coords->vertex || draw->coords->color[0] != draw->coords->color[12])
   {
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

      if(!draw->coords->vertex)
      {
         v[0].pos.x = 0.0f;
         v[0].pos.y = 1.0f;
         v[1].pos.x = 1.0f;
         v[1].pos.y = 1.0f;
         v[2].pos.x = 0.0f;
         v[2].pos.y = 0.0f;
         v[3].pos.x = 1.0f;
         v[3].pos.y = 0.0f;
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

      if(!draw->coords->tex_coord)
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

      for(int i = 0; i < 4; i++)
      {
         v[i].color.r = draw->coords->color[(i << 2) + 0];
         v[i].color.g = draw->coords->color[(i << 2) + 1];
         v[i].color.b = draw->coords->color[(i << 2) + 2];
         v[i].color.a = draw->coords->color[(i << 2) + 3];
      }

      if(draw->texture)
         GX2SetPixelTexture((GX2Texture*)draw->texture, tex_shader.ps.samplerVars[0].location);

      GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLE_STRIP, 4, wiiu->vertex_cache_tex.current, 1);
      wiiu->vertex_cache_tex.current += 4;
   }
   else
   {
      if (wiiu->vertex_cache.current + 1 > wiiu->vertex_cache.size)
         return;

      sprite_vertex_t* v = wiiu->vertex_cache.v + wiiu->vertex_cache.current;
      v->pos.x = draw->x;
      v->pos.y = wiiu->color_buffer.surface.height - draw->y - draw->height;
      v->pos.width = draw->width;
      v->pos.height = draw->height;
      v->coord.u = 0.0f;
      v->coord.v = 0.0f;
      v->coord.width = 1.0f;
      v->coord.height = 1.0f;

      v->color = COLOR_RGBA(0xFF * draw->coords->color[0], 0xFF * draw->coords->color[1],
                          0xFF * draw->coords->color[2], 0xFF * draw->coords->color[3]);

      if(draw->texture)
         GX2SetPixelTexture((GX2Texture*)draw->texture, sprite_shader.ps.samplerVars[0].location);

      GX2DrawEx(GX2_PRIMITIVE_MODE_POINTS, 1, wiiu->vertex_cache.current, 1);
      wiiu->vertex_cache.current ++;
      return;
   }

   GX2SetShaderMode(GX2_SHADER_MODE_GEOMETRY_SHADER);
   GX2SetShader(&sprite_shader);
//      GX2SetGeometryShaderInputRingBuffer(wiiu->input_ring_buffer, wiiu->input_ring_buffer_size);
//      GX2SetGeometryShaderOutputRingBuffer(wiiu->output_ring_buffer, wiiu->output_ring_buffer_size);
   GX2SetVertexUniformBlock(sprite_shader.vs.uniformBlocks[0].offset,
                            sprite_shader.vs.uniformBlocks[0].size,
                            wiiu->ubo_vp);
   GX2SetVertexUniformBlock(sprite_shader.vs.uniformBlocks[1].offset,
                            sprite_shader.vs.uniformBlocks[1].size,
                            wiiu->ubo_tex);
   GX2SetAttribBuffer(0, wiiu->vertex_cache.size * sizeof(*wiiu->vertex_cache.v),
                      sizeof(*wiiu->vertex_cache.v), wiiu->vertex_cache.v);
}

static void menu_display_wiiu_draw_pipeline(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
   video_coord_array_t *ca        = NULL;
   wiiu_video_t             *wiiu = (wiiu_video_t*)video_info->userdata;

   if (!wiiu || !draw)
      return;

   switch(draw->pipeline.id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
         ca = menu_display_get_coords_array();
         if(!wiiu->menu_shader_vbo)
         {
            wiiu->menu_shader_vbo = MEM2_alloc(ca->coords.vertices * 2 * sizeof(float), GX2_VERTEX_BUFFER_ALIGNMENT);
            memcpy(wiiu->menu_shader_vbo, ca->coords.vertex, ca->coords.vertices * 2 * sizeof(float));
            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, wiiu->menu_shader_vbo, ca->coords.vertices * 2 * sizeof(float));
         }

         draw->coords->vertex             = wiiu->menu_shader_vbo;
         draw->coords->vertices           = ca->coords.vertices;
         GX2SetAttribBuffer(0, draw->coords->vertices * 2 * sizeof(float), 2 * sizeof(float), wiiu->menu_shader_vbo);
         GX2SetBlendControl(GX2_RENDER_TARGET_0, GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_ONE,
               GX2_BLEND_COMBINE_MODE_ADD, GX2_DISABLE, 0, 0, 0);

         break;
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         GX2SetAttribBuffer(0, 4 * sizeof(*wiiu->v), sizeof(*wiiu->v), wiiu->v);
         break;
      default:
         return;
   }

   if(!wiiu->menu_shader_ubo)
   {
      wiiu->menu_shader_ubo = MEM2_alloc(sizeof(*wiiu->menu_shader_ubo), GX2_UNIFORM_BLOCK_ALIGNMENT);
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

static void menu_display_wiiu_restore_clear_color(void)
{
#if 0
   wiiu_set_clear_color(RGBA8(0x00, 0x00, 0x00, 0xFF));
#endif
}

static void menu_display_wiiu_clear_color(
      menu_display_ctx_clearcolor_t *clearcolor,
      video_frame_info_t *video_info)
{
   if (!clearcolor)
      return;
#if 0
   wiiu_set_clear_color(RGBA8((int)(clearcolor->r*255.f),
            (int)(clearcolor->g*255.f),
            (int)(clearcolor->b*255.f),
            (int)(clearcolor->a*255.f)));
   wiiu_clear_screen();
#endif
}

static bool menu_display_wiiu_font_init_first(
      void **font_handle, void *video_data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   font_data_t **handle = (font_data_t**)font_handle;
   *handle = font_driver_init_first(video_data,
         font_path, font_size, true,
         is_threaded,
         FONT_DRIVER_RENDER_WIIU);
   return *handle;
}

static void menu_display_wiiu_scissor_begin(video_frame_info_t *video_info, int x, int y,
      unsigned width, unsigned height)
{
   GX2SetScissor(MAX(x, 0), MAX(video_info->height - y - height, 0), MIN(width, video_info->width), MIN(height, video_info->height));
}

static void menu_display_wiiu_scissor_end(video_frame_info_t *video_info)
{
   GX2SetScissor(0, 0, video_info->width, video_info->height);
}

menu_display_ctx_driver_t menu_display_ctx_wiiu = {
   menu_display_wiiu_draw,
   menu_display_wiiu_draw_pipeline,
   menu_display_wiiu_viewport,
   menu_display_wiiu_blend_begin,
   menu_display_wiiu_blend_end,
   menu_display_wiiu_restore_clear_color,
   menu_display_wiiu_clear_color,
   menu_display_wiiu_get_default_mvp,
   menu_display_wiiu_get_default_vertices,
   menu_display_wiiu_get_default_tex_coords,
   menu_display_wiiu_font_init_first,
   MENU_VIDEO_DRIVER_WIIU,
   "gx2",
   true,
   menu_display_wiiu_scissor_begin,
   menu_display_wiiu_scissor_end
};

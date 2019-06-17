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

#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../menu_driver.h"

#include "../../retroarch.h"
#include "../../gfx/font_driver.h"
#include "../../gfx/common/d3d11_common.h"

static const float* menu_display_d3d11_get_default_vertices(void)
{
   return NULL;
}

static const float* menu_display_d3d11_get_default_tex_coords(void)
{
   return NULL;
}

static void* menu_display_d3d11_get_default_mvp(video_frame_info_t *video_info)
{
   return NULL;
}

static void menu_display_d3d11_blend_begin(video_frame_info_t *video_info)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)video_info->userdata;
   D3D11SetBlendState(d3d11->context,
         d3d11->blend_enable, NULL, D3D11_DEFAULT_SAMPLE_MASK);
}

static void menu_display_d3d11_blend_end(video_frame_info_t *video_info)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)video_info->userdata;
   D3D11SetBlendState(d3d11->context,
         d3d11->blend_disable, NULL, D3D11_DEFAULT_SAMPLE_MASK);
}

static void menu_display_d3d11_viewport(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
}

static void menu_display_d3d11_draw(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
   int vertex_count;
   d3d11_video_t *d3d11 = (d3d11_video_t*)video_info->userdata;

   if (!d3d11 || !draw || !draw->texture)
      return;

   switch (draw->pipeline.id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         d3d11_set_shader(d3d11->context, &d3d11->shaders[draw->pipeline.id]);
         D3D11Draw(d3d11->context, draw->coords->vertices, 0);

         D3D11SetBlendState(d3d11->context, d3d11->blend_enable, NULL, D3D11_DEFAULT_SAMPLE_MASK);
         d3d11_set_shader(d3d11->context, &d3d11->sprites.shader);
         D3D11SetVertexBuffer(d3d11->context, 0, d3d11->sprites.vbo, sizeof(d3d11_sprite_t), 0);
         D3D11SetPrimitiveTopology(d3d11->context, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
         return;
   }

   if (draw->coords->vertex && draw->coords->tex_coord && draw->coords->color)
      vertex_count = draw->coords->vertices;
   else
      vertex_count = 1;

   if (!d3d11->sprites.enabled || vertex_count > d3d11->sprites.capacity)
      return;

   if (d3d11->sprites.offset + vertex_count > d3d11->sprites.capacity)
      d3d11->sprites.offset = 0;

   {
      D3D11_MAPPED_SUBRESOURCE mapped_vbo;
      d3d11_sprite_t*          sprite = NULL;

      D3D11MapBuffer(
            d3d11->context, d3d11->sprites.vbo, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped_vbo);

      sprite = (d3d11_sprite_t*)mapped_vbo.pData + d3d11->sprites.offset;

      if (vertex_count == 1)
      {
         sprite->pos.x = draw->x / (float)d3d11->viewport.Width;
         sprite->pos.y =
               (d3d11->viewport.Height - draw->y - draw->height) / (float)d3d11->viewport.Height;
         sprite->pos.w = draw->width / (float)d3d11->viewport.Width;
         sprite->pos.h = draw->height / (float)d3d11->viewport.Height;

         sprite->coords.u = 0.0f;
         sprite->coords.v = 0.0f;
         sprite->coords.w = 1.0f;
         sprite->coords.h = 1.0f;

         if (draw->scale_factor)
            sprite->params.scaling = draw->scale_factor;
         else
            sprite->params.scaling = 1.0f;

         sprite->params.rotation = draw->rotation;

         sprite->colors[3] = DXGI_COLOR_RGBA(
               0xFF * draw->coords->color[0], 0xFF * draw->coords->color[1],
               0xFF * draw->coords->color[2], 0xFF * draw->coords->color[3]);
         sprite->colors[2] = DXGI_COLOR_RGBA(
               0xFF * draw->coords->color[4], 0xFF * draw->coords->color[5],
               0xFF * draw->coords->color[6], 0xFF * draw->coords->color[7]);
         sprite->colors[1] = DXGI_COLOR_RGBA(
               0xFF * draw->coords->color[8], 0xFF * draw->coords->color[9],
               0xFF * draw->coords->color[10], 0xFF * draw->coords->color[11]);
         sprite->colors[0] = DXGI_COLOR_RGBA(
               0xFF * draw->coords->color[12], 0xFF * draw->coords->color[13],
               0xFF * draw->coords->color[14], 0xFF * draw->coords->color[15]);
      }
      else
      {
         int          i;
         const float* vertex    = draw->coords->vertex;
         const float* tex_coord = draw->coords->tex_coord;
         const float* color     = draw->coords->color;

         for (i = 0; i < vertex_count; i++)
         {
            d3d11_vertex_t* v = (d3d11_vertex_t*)sprite;
            v->position[0]    = *vertex++;
            v->position[1]    = *vertex++;
            v->texcoord[0]    = *tex_coord++;
            v->texcoord[1]    = *tex_coord++;
            v->color[0]       = *color++;
            v->color[1]       = *color++;
            v->color[2]       = *color++;
            v->color[3]       = *color++;

            sprite++;
         }

         d3d11_set_shader(d3d11->context, &d3d11->shaders[VIDEO_SHADER_STOCK_BLEND]);
         D3D11SetPrimitiveTopology(d3d11->context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      }

      D3D11UnmapBuffer(d3d11->context, d3d11->sprites.vbo, 0);
   }

   d3d11_set_texture_and_sampler(d3d11->context, 0, (d3d11_texture_t*)draw->texture);
   D3D11Draw(d3d11->context, vertex_count, d3d11->sprites.offset);
   d3d11->sprites.offset += vertex_count;

   if (vertex_count > 1)
   {
      d3d11_set_shader(d3d11->context, &d3d11->sprites.shader);
      D3D11SetPrimitiveTopology(d3d11->context, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
   }

   return;
}

static void menu_display_d3d11_draw_pipeline(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
   d3d11_video_t *d3d11 = (d3d11_video_t*)video_info->userdata;

   if (!d3d11 || !draw)
      return;

   switch (draw->pipeline.id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      {
         video_coord_array_t* ca = menu_display_get_coords_array();

         if (!d3d11->menu_pipeline_vbo)
         {
            D3D11_BUFFER_DESC desc = { 0 };
            desc.Usage             = D3D11_USAGE_IMMUTABLE;
            desc.ByteWidth         = ca->coords.vertices * 2 * sizeof(float);
            desc.BindFlags         = D3D11_BIND_VERTEX_BUFFER;

			{
               D3D11_SUBRESOURCE_DATA vertexData = { ca->coords.vertex };
               D3D11CreateBuffer(d3d11->device, &desc, &vertexData, &d3d11->menu_pipeline_vbo);
			}
         }
         D3D11SetVertexBuffer(d3d11->context, 0, d3d11->menu_pipeline_vbo, 2 * sizeof(float), 0);
         draw->coords->vertices = ca->coords.vertices;
         D3D11SetBlendState(d3d11->context, d3d11->blend_pipeline, NULL, D3D11_DEFAULT_SAMPLE_MASK);
         break;
      }

      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         D3D11SetVertexBuffer(d3d11->context, 0, d3d11->frame.vbo, sizeof(d3d11_vertex_t), 0);
         draw->coords->vertices = 4;
         break;
      default:
         return;
   }

   D3D11SetPrimitiveTopology(d3d11->context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

   d3d11->ubo_values.time += 0.01f;

   {
      D3D11_MAPPED_SUBRESOURCE mapped_ubo;
      D3D11MapBuffer(d3d11->context, d3d11->ubo, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_ubo);
      *(d3d11_uniform_t*)mapped_ubo.pData = d3d11->ubo_values;
      D3D11UnmapBuffer(d3d11->context, d3d11->ubo, 0);
   }
}

static void menu_display_d3d11_restore_clear_color(void) {}

static void menu_display_d3d11_clear_color(
      menu_display_ctx_clearcolor_t* clearcolor,
      video_frame_info_t *video_info)
{
   d3d11_video_t *d3d11 = (d3d11_video_t*)video_info->userdata;

   if (!d3d11 || !clearcolor)
      return;

   D3D11ClearRenderTargetView(d3d11->context,
         d3d11->renderTargetView, (float*)clearcolor);
}

static bool menu_display_d3d11_font_init_first(
      void**      font_handle,
      void*       video_data,
      const char* font_path,
      float       menu_font_size,
      bool        is_threaded)
{
   font_data_t** handle     = (font_data_t**)font_handle;
   font_data_t*  new_handle = font_driver_init_first(
         video_data, font_path, menu_font_size, true,
         is_threaded, FONT_DRIVER_RENDER_D3D11_API);
   if (!new_handle)
      return false;
   *handle = new_handle;
   return true;
}

void menu_display_d3d11_scissor_begin(video_frame_info_t *video_info, int x, int y, unsigned width, unsigned height)
{
   D3D11_RECT rect;
   d3d11_video_t *d3d11 = (d3d11_video_t*)video_info->userdata;

   if (!d3d11 || !width || !height)
      return;

   rect.left            = x;
   rect.top             = y;
   rect.right           = width + x;
   rect.bottom          = height + y;

   D3D11SetScissorRects(d3d11->context, 1, &rect);
}

void menu_display_d3d11_scissor_end(video_frame_info_t *video_info)
{
   D3D11_RECT rect;
   d3d11_video_t *d3d11 = (d3d11_video_t*)video_info->userdata;

   if (!d3d11)
      return;

   rect.left            = 0;
   rect.top             = 0;
   rect.right           = video_info->width;
   rect.bottom          = video_info->height;

   D3D11SetScissorRects(d3d11->context, 1, &rect);
}

menu_display_ctx_driver_t menu_display_ctx_d3d11 = {
   menu_display_d3d11_draw,
   menu_display_d3d11_draw_pipeline,
   menu_display_d3d11_viewport,
   menu_display_d3d11_blend_begin,
   menu_display_d3d11_blend_end,
   menu_display_d3d11_restore_clear_color,
   menu_display_d3d11_clear_color,
   menu_display_d3d11_get_default_mvp,
   menu_display_d3d11_get_default_vertices,
   menu_display_d3d11_get_default_tex_coords,
   menu_display_d3d11_font_init_first,
   MENU_VIDEO_DRIVER_DIRECT3D11,
   "d3d11",
   true,
   menu_display_d3d11_scissor_begin,
   menu_display_d3d11_scissor_end
};

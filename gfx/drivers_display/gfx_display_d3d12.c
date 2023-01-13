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
#include "../../config.h"
#endif

#include "../gfx_display.h"

#include "../common/d3d12_common.h"

static void gfx_display_d3d12_blend_begin(void *data)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   d3d12->sprites.pipe  = d3d12->sprites.pipe_blend;
   D3D12SetPipelineState(d3d12->queue.cmd, d3d12->sprites.pipe);
}

static void gfx_display_d3d12_blend_end(void *data)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   d3d12->sprites.pipe = d3d12->sprites.pipe_noblend;
   D3D12SetPipelineState(d3d12->queue.cmd, d3d12->sprites.pipe);
}

static void gfx_display_d3d12_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   int vertex_count     = 1;
   d3d12_video_t *d3d12 = (d3d12_video_t*)data;

   if (!d3d12 || !draw || !draw->texture)
      return;

   switch (draw->pipeline_id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         D3D12SetPipelineState(d3d12->queue.cmd, d3d12->pipes[draw->pipeline_id]);
         D3D12DrawInstanced(d3d12->queue.cmd, draw->coords->vertices, 1, 0, 0);
         D3D12SetPipelineState(d3d12->queue.cmd, d3d12->sprites.pipe);
         D3D12IASetPrimitiveTopology(d3d12->queue.cmd, D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
         D3D12IASetVertexBuffers(d3d12->queue.cmd, 0, 1, &d3d12->sprites.vbo_view);
         return;
   }

   if (draw->coords->vertex && draw->coords->tex_coord && draw->coords->color)
      vertex_count = draw->coords->vertices;

   if (   (!(d3d12->flags & D3D12_ST_FLAG_SPRITES_ENABLE)) 
         || (vertex_count > d3d12->sprites.capacity))
      return;

   if (d3d12->sprites.offset + vertex_count > d3d12->sprites.capacity)
      d3d12->sprites.offset = 0;

   {
      d3d12_sprite_t* sprite;
      D3D12_RANGE range;
      range.Begin           = 0;
      range.End             = 0;
      D3D12Map(d3d12->sprites.vbo, 0, &range, (void**)&sprite);
      sprite += d3d12->sprites.offset;

      if (vertex_count == 1)
      {

         sprite->pos.x    = draw->x      / (float)d3d12->chain.viewport.Width;
         sprite->pos.y    = 
            (d3d12->chain.viewport.Height - draw->y - draw->height) /
            (float)d3d12->chain.viewport.Height;
         sprite->pos.w    = draw->width  / (float)d3d12->chain.viewport.Width;
         sprite->pos.h    = draw->height / (float)d3d12->chain.viewport.Height;

         sprite->coords.u = 0.0f;
         sprite->coords.v = 0.0f;
         sprite->coords.w = 1.0f;
         sprite->coords.h = 1.0f;

         if (draw->scale_factor)
            sprite->params.scaling = draw->scale_factor;
         else
            sprite->params.scaling = 1.0f;

         sprite->params.rotation   = draw->rotation;

         sprite->colors[3]         = DXGI_COLOR_RGBA(
               0xFF * draw->coords->color[0],  0xFF * draw->coords->color[1],
               0xFF * draw->coords->color[2],  0xFF * draw->coords->color[3]);
         sprite->colors[2]         = DXGI_COLOR_RGBA(
               0xFF * draw->coords->color[4],  0xFF * draw->coords->color[5],
               0xFF * draw->coords->color[6],  0xFF * draw->coords->color[7]);
         sprite->colors[1]         = DXGI_COLOR_RGBA(
               0xFF * draw->coords->color[8],  0xFF * draw->coords->color[9],
               0xFF * draw->coords->color[10], 0xFF * draw->coords->color[11]);
         sprite->colors[0]         = DXGI_COLOR_RGBA(
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
            d3d12_vertex_t* v = (d3d12_vertex_t*)sprite;
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
         D3D12SetPipelineState(d3d12->queue.cmd,
               d3d12->pipes[VIDEO_SHADER_STOCK_BLEND]);
         D3D12IASetPrimitiveTopology(d3d12->queue.cmd,
               D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      }

      range.Begin = d3d12->sprites.offset * sizeof(*sprite);
      range.End   = (d3d12->sprites.offset + vertex_count) * sizeof(*sprite);
      D3D12Unmap(d3d12->sprites.vbo, 0, &range);
   }

   {
      d3d12_texture_t* texture = (d3d12_texture_t*)draw->texture;
      if (texture->dirty)
      {
         d3d12_upload_texture(d3d12->queue.cmd,
               texture, d3d12);

         if (vertex_count > 1)
            D3D12SetPipelineState(d3d12->queue.cmd,
                  d3d12->pipes[VIDEO_SHADER_STOCK_BLEND]);
         else
            D3D12SetPipelineState(d3d12->queue.cmd,
                  d3d12->sprites.pipe);
      }
      d3d12_set_texture_and_sampler(d3d12->queue.cmd, texture);
   }

   D3D12DrawInstanced(d3d12->queue.cmd, vertex_count, 1, d3d12->sprites.offset, 0);
   d3d12->sprites.offset += vertex_count;

   if (vertex_count > 1)
   {
      D3D12SetPipelineState(d3d12->queue.cmd, d3d12->sprites.pipe);
      D3D12IASetPrimitiveTopology(d3d12->queue.cmd, D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
   }

   return;
}

static void gfx_display_d3d12_draw_pipeline(gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp,
      void *data, unsigned video_width, unsigned video_height)
{
   d3d12_video_t *d3d12 = (d3d12_video_t*)data;

   if (!d3d12 || !draw)
      return;

   switch (draw->pipeline_id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      {
         video_coord_array_t* ca   = &p_disp->dispca;

         if (!d3d12->menu_pipeline_vbo)
         {
            D3D12_RANGE read_range;
            void*       vertex_data_begin;

            d3d12->menu_pipeline_vbo_view.StrideInBytes  = 2 * sizeof(float);
            d3d12->menu_pipeline_vbo_view.SizeInBytes    =
                  ca->coords.vertices * d3d12->menu_pipeline_vbo_view.StrideInBytes;
            d3d12->menu_pipeline_vbo_view.BufferLocation = d3d12_create_buffer(
                  d3d12->device, d3d12->menu_pipeline_vbo_view.SizeInBytes,
                  &d3d12->menu_pipeline_vbo);

            read_range.Begin           = 0;
            read_range.End             = 0;
            D3D12Map(d3d12->menu_pipeline_vbo, 0,
                  &read_range, &vertex_data_begin);
            memcpy(vertex_data_begin, ca->coords.vertex,
                  d3d12->menu_pipeline_vbo_view.SizeInBytes);
            D3D12Unmap(d3d12->menu_pipeline_vbo, 0, NULL);
         }
         D3D12IASetVertexBuffers(d3d12->queue.cmd, 0, 1,
               &d3d12->menu_pipeline_vbo_view);
         draw->coords->vertices = ca->coords.vertices;
         break;
      }

      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         D3D12IASetVertexBuffers(d3d12->queue.cmd, 0, 1,
               &d3d12->frame.vbo_view);
         draw->coords->vertices = 4;
         break;
      default:
         return;
   }
   D3D12IASetPrimitiveTopology(d3d12->queue.cmd,
         D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

   d3d12->ubo_values.time += 0.01f;

   {
      D3D12_RANGE read_range;
      d3d12_uniform_t* mapped_ubo;

      read_range.Begin     = 0;
      read_range.End       = 0;
      D3D12Map(d3d12->ubo, 0, &read_range, (void**)&mapped_ubo);
      *mapped_ubo = d3d12->ubo_values;
      D3D12Unmap(d3d12->ubo, 0, NULL);
   }
   D3D12SetGraphicsRootConstantBufferView(
         d3d12->queue.cmd, ROOT_ID_UBO,
         d3d12->ubo_view.BufferLocation);
}

void gfx_display_d3d12_scissor_begin(void *data,
      unsigned video_width, unsigned video_height,
      int x, int y, unsigned width, unsigned height)
{
   D3D12_RECT rect;
   d3d12_video_t *d3d12 = (d3d12_video_t*)data;

   if (!d3d12)
      return;

   rect.left            = x;
   rect.top             = y;
   rect.right           = width + x;
   rect.bottom          = height + y;

   D3D12RSSetScissorRects(d3d12->queue.cmd, 1, &rect);
}

void gfx_display_d3d12_scissor_end(void *data,
      unsigned video_width,
      unsigned video_height)
{
   D3D12_RECT rect;
   d3d12_video_t *d3d12  = (d3d12_video_t*)data;

   if (!d3d12)
      return;

   rect.left            = 0;
   rect.top             = 0;
   rect.right           = video_width;
   rect.bottom          = video_height;

   D3D12RSSetScissorRects(d3d12->queue.cmd, 1, &rect);
}

gfx_display_ctx_driver_t gfx_display_ctx_d3d12 = {
   gfx_display_d3d12_draw,
   gfx_display_d3d12_draw_pipeline,
   gfx_display_d3d12_blend_begin,
   gfx_display_d3d12_blend_end,
   NULL,                                     /* get_default_mvp        */
   NULL,                                     /* get_default_vertices   */
   NULL,                                     /* get_default_tex_coords */
   FONT_DRIVER_RENDER_D3D12_API,
   GFX_VIDEO_DRIVER_DIRECT3D12,
   "d3d12",
   true,
   gfx_display_d3d12_scissor_begin,
   gfx_display_d3d12_scissor_end
};

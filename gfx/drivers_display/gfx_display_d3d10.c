/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2018 - Daniel De Matteis
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

#include "../common/d3d10_common.h"

static void gfx_display_d3d10_blend_begin(void *data)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;
   d3d10->device->lpVtbl->OMSetBlendState(d3d10->device,
         d3d10->blend_enable,
         NULL, D3D10_DEFAULT_SAMPLE_MASK);
}

static void gfx_display_d3d10_blend_end(void *data)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;
   d3d10->device->lpVtbl->OMSetBlendState(d3d10->device,
         d3d10->blend_disable,
         NULL, D3D10_DEFAULT_SAMPLE_MASK);
}

static void gfx_display_d3d10_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   int vertex_count     = 1;
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   if (!d3d10 || !draw || !draw->texture)
      return;

   switch (draw->pipeline_id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         d3d10_set_shader(d3d10->device, &d3d10->shaders[draw->pipeline_id]);
         d3d10->device->lpVtbl->Draw(d3d10->device, draw->coords->vertices, 0);

         d3d10->device->lpVtbl->OMSetBlendState(d3d10->device,
               d3d10->blend_enable,
               NULL, D3D10_DEFAULT_SAMPLE_MASK);
         d3d10_set_shader(d3d10->device, &d3d10->sprites.shader);
         D3D10SetVertexBuffer(d3d10->device, 0, d3d10->sprites.vbo, sizeof(d3d10_sprite_t), 0);
         d3d10->device->lpVtbl->IASetPrimitiveTopology(d3d10->device,
               D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
         return;
      default:
         break;
   }

   if (draw->coords->vertex && draw->coords->tex_coord && draw->coords->color)
      vertex_count = draw->coords->vertices;

   if (     (!(d3d10->flags & D3D10_ST_FLAG_SPRITES_ENABLE))
         || (vertex_count > d3d10->sprites.capacity))
      return;

   if (d3d10->sprites.offset + vertex_count > d3d10->sprites.capacity)
      d3d10->sprites.offset = 0;

   {
      void*           mapped_vbo = NULL;
      d3d10_sprite_t* sprite     = NULL;

      d3d10->sprites.vbo->lpVtbl->Map(d3d10->sprites.vbo,
            D3D10_MAP_WRITE_NO_OVERWRITE, 0,
            (void**)&mapped_vbo);

      sprite = (d3d10_sprite_t*)mapped_vbo + d3d10->sprites.offset;

      if (vertex_count == 1)
      {
         sprite->pos.x       = draw->x / (float)d3d10->viewport.Width;
         sprite->pos.y       =
               (d3d10->viewport.Height - draw->y - draw->height) 
               / (float)d3d10->viewport.Height;
         sprite->pos.w       = draw->width / (float)d3d10->viewport.Width;
         sprite->pos.h       = draw->height / (float)d3d10->viewport.Height;

         sprite->coords.u    = 0.0f;
         sprite->coords.v    = 0.0f;
         sprite->coords.w    = 1.0f;
         sprite->coords.h    = 1.0f;

         if (draw->scale_factor)
            sprite->params.scaling = draw->scale_factor;
         else
            sprite->params.scaling = 1.0f;

         sprite->params.rotation   = draw->rotation;

         sprite->colors[3]         = DXGI_COLOR_RGBA(
               0xFF * draw->coords->color[0], 0xFF * draw->coords->color[1],
               0xFF * draw->coords->color[2], 0xFF * draw->coords->color[3]);
         sprite->colors[2]         = DXGI_COLOR_RGBA(
               0xFF * draw->coords->color[4], 0xFF * draw->coords->color[5],
               0xFF * draw->coords->color[6], 0xFF * draw->coords->color[7]);
         sprite->colors[1]         = DXGI_COLOR_RGBA(
               0xFF * draw->coords->color[8], 0xFF * draw->coords->color[9],
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
            d3d10_vertex_t* v = (d3d10_vertex_t*)sprite;
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

         d3d10_set_shader(d3d10->device,
               &d3d10->shaders[VIDEO_SHADER_STOCK_BLEND]);
         d3d10->device->lpVtbl->IASetPrimitiveTopology(d3d10->device,
               D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      }

      d3d10->sprites.vbo->lpVtbl->Unmap(d3d10->sprites.vbo);
   }

   d3d10_set_texture_and_sampler(d3d10->device, 0,
         (d3d10_texture_t*)draw->texture);
   d3d10->device->lpVtbl->Draw(d3d10->device, vertex_count,
         d3d10->sprites.offset);
   d3d10->sprites.offset += vertex_count;

   if (vertex_count > 1)
   {
      d3d10_set_shader(d3d10->device, &d3d10->sprites.shader);
      d3d10->device->lpVtbl->IASetPrimitiveTopology(d3d10->device,
            D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
   }
}

static void gfx_display_d3d10_draw_pipeline(gfx_display_ctx_draw_t* draw,
      gfx_display_t *p_disp,
      void *data, unsigned video_width, unsigned video_height)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   if (!d3d10 || !draw)
      return;

   switch (draw->pipeline_id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      {
         video_coord_array_t* ca   = &p_disp->dispca;

         if (!d3d10->menu_pipeline_vbo)
         {
            D3D10_BUFFER_DESC desc;
            D3D10_SUBRESOURCE_DATA vertex_data;

            desc.ByteWidth               = ca->coords.vertices * 2 * sizeof(float);
            desc.Usage                   = D3D10_USAGE_IMMUTABLE;
            desc.BindFlags               = D3D10_BIND_VERTEX_BUFFER;
            desc.CPUAccessFlags          = 0;
            desc.MiscFlags               = 0;

            vertex_data.pSysMem          = ca->coords.vertex;
            vertex_data.SysMemPitch      = 0;
            vertex_data.SysMemSlicePitch = 0;
            d3d10->device->lpVtbl->CreateBuffer(d3d10->device, &desc,
                  &vertex_data, &d3d10->menu_pipeline_vbo);
         }
         D3D10SetVertexBuffer(d3d10->device, 0,
               d3d10->menu_pipeline_vbo, 2 * sizeof(float), 0);
         draw->coords->vertices = ca->coords.vertices;
         d3d10->device->lpVtbl->OMSetBlendState(d3d10->device,
               d3d10->blend_pipeline,
               NULL, D3D10_DEFAULT_SAMPLE_MASK);
         break;
      }

      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         D3D10SetVertexBuffer(d3d10->device, 0,
               d3d10->frame.vbo, sizeof(d3d10_vertex_t), 0);
         draw->coords->vertices = 4;
         break;
      default:
         return;
   }

   d3d10->device->lpVtbl->IASetPrimitiveTopology(d3d10->device,
         D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

   d3d10->ubo_values.time += 0.01f;

   {
      void *mapped_ubo              = NULL;
      d3d10->ubo->lpVtbl->Map(d3d10->ubo, D3D10_MAP_WRITE_DISCARD, 0,
            (void**)&mapped_ubo);
      *(d3d10_uniform_t*)mapped_ubo = d3d10->ubo_values;
      d3d10->ubo->lpVtbl->Unmap(d3d10->ubo);
   }
}

void gfx_display_d3d10_scissor_begin(void *data,
      unsigned video_width, unsigned video_height,
      int x, int y, unsigned width, unsigned height)
{
   D3D10_RECT rect;
   d3d10_video_t *d3d10 = (d3d10_video_t*)data;

   if (!d3d10)
      return;

   rect.left            = x;
   rect.top             = y;
   rect.right           = width + x;
   rect.bottom          = height + y;

   d3d10->device->lpVtbl->RSSetScissorRects(d3d10->device, 1, &rect);
}

void gfx_display_d3d10_scissor_end(void *data,
      unsigned video_width, unsigned video_height)
{
   D3D10_RECT rect;
   d3d10_video_t *d3d10  = (d3d10_video_t*)data;

   if (!d3d10)
      return;

   rect.left            = 0;
   rect.top             = 0;
   rect.right           = video_width;
   rect.bottom          = video_height;

   d3d10->device->lpVtbl->RSSetScissorRects(d3d10->device, 1, &rect);
}

gfx_display_ctx_driver_t gfx_display_ctx_d3d10 = {
   gfx_display_d3d10_draw,
   gfx_display_d3d10_draw_pipeline,
   gfx_display_d3d10_blend_begin,
   gfx_display_d3d10_blend_end,
   NULL,                                     /* get_default_mvp        */
   NULL,                                     /* get_default_vertices   */
   NULL,                                     /* get_default_tex_coords */
   FONT_DRIVER_RENDER_D3D10_API,
   GFX_VIDEO_DRIVER_DIRECT3D10,
   "d3d10",
   true,
   gfx_display_d3d10_scissor_begin,
   gfx_display_d3d10_scissor_end
};

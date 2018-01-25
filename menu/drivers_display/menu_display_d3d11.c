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

#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "menu/menu_driver.h"

#include "retroarch.h"
#include "gfx/font_driver.h"
#include "gfx/video_driver.h"
#include "gfx/common/d3d11_common.h"

static const float* menu_display_d3d11_get_default_vertices(void) { return NULL; }

static const float* menu_display_d3d11_get_default_tex_coords(void) { return NULL; }

static void* menu_display_d3d11_get_default_mvp(void) { return NULL; }

static void menu_display_d3d11_blend_begin(void) {}

static void menu_display_d3d11_blend_end(void) {}

static void menu_display_d3d11_viewport(void* data) {}

static void menu_display_d3d11_draw(void* data)
{
   d3d11_video_t*           d3d11 = (d3d11_video_t*)video_driver_get_ptr(false);
   menu_display_ctx_draw_t* draw  = (menu_display_ctx_draw_t*)data;

   if (!d3d11 || !draw || !draw->texture)
      return;

   if (draw->pipeline.id)
      return;

   if (!d3d11->sprites.enabled)
      return;

   if(d3d11->sprites.offset + 1 > d3d11->sprites.capacity)
      d3d11->sprites.offset = 0;


   D3D11_MAPPED_SUBRESOURCE mapped_vbo;
   D3D11MapBuffer(d3d11->ctx, d3d11->sprites.vbo, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped_vbo);
   d3d11_sprite_t* v = (d3d11_sprite_t*)mapped_vbo.pData + d3d11->sprites.offset;

   v->pos.x = draw->x / (float)d3d11->viewport.Width;
   v->pos.y = (d3d11->viewport.Height - draw->y - draw->height) / (float)d3d11->viewport.Height;
   v->pos.w = draw->width / (float)d3d11->viewport.Width;
   v->pos.h = draw->height / (float)d3d11->viewport.Height;

   v->coords.u = 0.0f;
   v->coords.v = 0.0f;
   v->coords.w = 1.0f;
   v->coords.h = 1.0f;

   if(draw->scale_factor)
      v->params.scaling = draw->scale_factor;
   else
      v->params.scaling = 1.0f;
   v->params.rotation = draw->rotation;

   v->colors[3] = DXGI_COLOR_RGBA(
         0xFF * draw->coords->color[0], 0xFF * draw->coords->color[1],
         0xFF * draw->coords->color[2], 0xFF * draw->coords->color[3]);
   v->colors[2] = DXGI_COLOR_RGBA(
         0xFF * draw->coords->color[4], 0xFF * draw->coords->color[5],
         0xFF * draw->coords->color[6], 0xFF * draw->coords->color[7]);
   v->colors[1] = DXGI_COLOR_RGBA(
         0xFF * draw->coords->color[8], 0xFF * draw->coords->color[9],
         0xFF * draw->coords->color[10], 0xFF * draw->coords->color[11]);
   v->colors[0] = DXGI_COLOR_RGBA(
         0xFF * draw->coords->color[12], 0xFF * draw->coords->color[13],
         0xFF * draw->coords->color[14], 0xFF * draw->coords->color[15]);

   D3D11UnmapBuffer(d3d11->ctx, d3d11->sprites.vbo, 0);
#if 0
   D3D11SetPShader(d3d11->ctx, d3d11->sprites.ps, NULL, 0);
#endif
   D3D11SetPShaderResources(d3d11->ctx, 0, 1, &((d3d11_texture_t*)draw->texture)->view);


   D3D11Draw(d3d11->ctx, 1, d3d11->sprites.offset);
   d3d11->sprites.offset++;
   return;
}

static void menu_display_d3d11_draw_pipeline(void* data) {}

static void menu_display_d3d11_restore_clear_color(void) {}

static void menu_display_d3d11_clear_color(menu_display_ctx_clearcolor_t* clearcolor)
{
   DWORD          clear_color = 0;
   d3d11_video_t* d3d11       = (d3d11_video_t*)video_driver_get_ptr(false);

   if (!d3d11 || !clearcolor)
      return;

   D3D11ClearRenderTargetView(d3d11->ctx, d3d11->renderTargetView, (float*)clearcolor);
}

static bool menu_display_d3d11_font_init_first(
      void**      font_handle,
      void*       video_data,
      const char* font_path,
      float       font_size,
      bool        is_threaded)
{
   font_data_t** handle = (font_data_t**)font_handle;
   *handle              = font_driver_init_first(
         video_data, font_path, font_size, true, is_threaded, FONT_DRIVER_RENDER_D3D11_API);
   return *handle;
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
   "menu_display_d3d11",
};

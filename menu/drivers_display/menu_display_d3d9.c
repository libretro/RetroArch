/*  RetroArch - A frontend for libretro.
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

#define CINTERFACE

#include <retro_miscellaneous.h>

#include <gfx/math/matrix_4x4.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../menu_driver.h"

#include "../../retroarch.h"
#include "../../gfx/common/d3d_common.h"
#include "../../gfx/common/d3d9_common.h"

static const float d3d9_vertexes[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const float d3d9_tex_coords[] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static const float *menu_display_d3d9_get_default_vertices(void)
{
   return &d3d9_vertexes[0];
}

static const float *menu_display_d3d9_get_default_tex_coords(void)
{
   return &d3d9_tex_coords[0];
}

static void *menu_display_d3d9_get_default_mvp(video_frame_info_t *video_info)
{
   static math_matrix_4x4 id;
   matrix_4x4_identity(id);

   return &id;
}

static INT32 menu_display_prim_to_d3d9_enum(
      enum menu_display_prim_type prim_type)
{
   switch (prim_type)
   {
      case MENU_DISPLAY_PRIM_TRIANGLES:
      case MENU_DISPLAY_PRIM_TRIANGLESTRIP:
         return D3DPT_COMM_TRIANGLESTRIP;
      case MENU_DISPLAY_PRIM_NONE:
      default:
         break;
   }

   /* TOD/FIXME - hack */
   return 0;
}

static void menu_display_d3d9_blend_begin(video_frame_info_t *video_info)
{
   d3d9_video_t *d3d = (d3d9_video_t*)video_info->userdata;

   if (!d3d)
      return;

   d3d9_enable_blend_func(d3d->dev);
}

static void menu_display_d3d9_blend_end(video_frame_info_t *video_info)
{
   d3d9_video_t *d3d = (d3d9_video_t*)video_info->userdata;

   if (!d3d)
      return;

   d3d9_disable_blend_func(d3d->dev);
}

static void menu_display_d3d9_viewport(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
}

static void menu_display_d3d9_bind_texture(menu_display_ctx_draw_t *draw,
      d3d9_video_t *d3d)
{
   LPDIRECT3DDEVICE9 dev = d3d->dev;

   d3d9_set_texture(dev, 0, (LPDIRECT3DTEXTURE9)draw->texture);
   d3d9_set_sampler_address_u(dev, 0, D3DTADDRESS_COMM_CLAMP);
   d3d9_set_sampler_address_v(dev, 0, D3DTADDRESS_COMM_CLAMP);
   d3d9_set_sampler_minfilter(dev, 0, D3DTEXF_COMM_LINEAR);
   d3d9_set_sampler_magfilter(dev, 0, D3DTEXF_COMM_LINEAR);
   d3d9_set_sampler_mipfilter(dev, 0, D3DTEXF_COMM_LINEAR);
}

static void menu_display_d3d9_draw(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
   unsigned i;
   math_matrix_4x4 mop, m1, m2;
   LPDIRECT3DDEVICE9 dev;
   d3d9_video_t *d3d             = (d3d9_video_t*)video_info->userdata;
   Vertex * pv                   = NULL;
   const float *vertex           = NULL;
   const float *tex_coord        = NULL;
   const float *color            = NULL;

   if (!d3d || !draw || draw->pipeline.id)
      return;

   dev                           = d3d->dev;

   if((d3d->menu_display.offset + draw->coords->vertices )
         > (unsigned)d3d->menu_display.size)
      return;

   pv           = (Vertex*)
      d3d9_vertex_buffer_lock((LPDIRECT3DVERTEXBUFFER9)
            d3d->menu_display.buffer);

   if (!pv)
      return;

   pv          += d3d->menu_display.offset;
   vertex       = draw->coords->vertex;
   tex_coord    = draw->coords->tex_coord;
   color        = draw->coords->color;

   if (!vertex)
      vertex    = menu_display_d3d9_get_default_vertices();
   if (!tex_coord)
      tex_coord = menu_display_d3d9_get_default_tex_coords();

   for (i = 0; i < draw->coords->vertices; i++)
   {
      int colors[4];

      colors[0]   = *color++ * 0xFF;
      colors[1]   = *color++ * 0xFF;
      colors[2]   = *color++ * 0xFF;
      colors[3]   = *color++ * 0xFF;

      pv[i].x     = *vertex++;
      pv[i].y     = *vertex++;
      pv[i].z     = 0.5f;
      pv[i].u     = *tex_coord++;
      pv[i].v     = *tex_coord++;

      pv[i].color =
         D3DCOLOR_ARGB(
               colors[3], /* A */
               colors[0], /* R */
               colors[1], /* G */
               colors[2]  /* B */
               );
   }
   d3d9_vertex_buffer_unlock((LPDIRECT3DVERTEXBUFFER9)
         d3d->menu_display.buffer);

   if(!draw->matrix_data)
      draw->matrix_data = menu_display_d3d9_get_default_mvp(video_info);

   /* ugh */
   matrix_4x4_scale(m1,       2.0,  2.0, 0);
   matrix_4x4_translate(mop, -1.0, -1.0, 0);
   matrix_4x4_multiply(m2, mop, m1);
   matrix_4x4_multiply(m1,
         *((math_matrix_4x4*)draw->matrix_data), m2);
   matrix_4x4_scale(mop,
         (draw->width  / 2.0) / video_info->width,
         (draw->height / 2.0) / video_info->height, 0);
   matrix_4x4_multiply(m2, mop, m1);
   matrix_4x4_translate(mop,
         (draw->x + (draw->width  / 2.0)) / video_info->width,
         (draw->y + (draw->height / 2.0)) / video_info->height,
         0);
   matrix_4x4_multiply(m1, mop, m2);
   matrix_4x4_multiply(m2, d3d->mvp_transposed, m1);
   d3d_matrix_transpose(&m1, &m2);

   d3d9_set_mvp(d3d->dev, &m1);

   if (draw && draw->texture)
      menu_display_d3d9_bind_texture(draw, d3d);

   d3d9_draw_primitive(dev,
         (D3DPRIMITIVETYPE)menu_display_prim_to_d3d9_enum(draw->prim_type),
         d3d->menu_display.offset,
         draw->coords->vertices -
         ((draw->prim_type == MENU_DISPLAY_PRIM_TRIANGLESTRIP)
          ? 2 : 0));

   d3d->menu_display.offset += draw->coords->vertices;
}

static void menu_display_d3d9_draw_pipeline(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
#if defined(HAVE_HLSL) || defined(HAVE_CG)
   static float t                    = 0;
   video_coord_array_t *ca           = NULL;

   if (!draw)
      return;

   ca                                = menu_display_get_coords_array();

   draw->x                           = 0;
   draw->y                           = 0;
   draw->coords                      = NULL;
   draw->matrix_data                 = NULL;

   if (ca)
      draw->coords                   = (struct video_coords*)&ca->coords;

   switch (draw->pipeline.id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      case VIDEO_SHADER_MENU_3:
         {
            struct uniform_info uniform_param  = {0};
            t                                 += 0.01;

            (void)uniform_param;

            uniform_param.enabled              = true;
            uniform_param.lookup.enable        = true;
            uniform_param.lookup.add_prefix    = true;
            uniform_param.lookup.idx           = draw->pipeline.id;
            uniform_param.lookup.type          = SHADER_PROGRAM_VERTEX;
            uniform_param.type                 = UNIFORM_1F;
            uniform_param.lookup.ident         = "time";
            uniform_param.result.f.v0          = t;
         }
         break;
   }
#endif
}

static void menu_display_d3d9_restore_clear_color(void)
{
   /* not needed */
}

static void menu_display_d3d9_clear_color(
      menu_display_ctx_clearcolor_t *clearcolor,
      video_frame_info_t *video_info)
{
   LPDIRECT3DDEVICE9 dev;
   DWORD    clear_color  = 0;
   d3d9_video_t     *d3d = (d3d9_video_t*)video_info->userdata;

   if (!d3d || !clearcolor)
      return;

   dev                  = (LPDIRECT3DDEVICE9)d3d->dev;

   clear_color = D3DCOLOR_ARGB(
         BYTE_CLAMP(clearcolor->a * 255.0f), /* A */
         BYTE_CLAMP(clearcolor->r * 255.0f), /* R */
         BYTE_CLAMP(clearcolor->g * 255.0f), /* G */
         BYTE_CLAMP(clearcolor->b * 255.0f)  /* B */
         );

   d3d9_clear(dev, 0, NULL, D3D_COMM_CLEAR_TARGET, clear_color, 0, 0);
}

static bool menu_display_d3d9_font_init_first(
      void **font_handle, void *video_data,
      const char *font_path, float menu_font_size,
      bool is_threaded)
{
   font_data_t **handle = (font_data_t**)font_handle;
   if (!(*handle = font_driver_init_first(video_data,
         font_path, menu_font_size, true,
         is_threaded,
         FONT_DRIVER_RENDER_D3D9_API)))
		 return false;
   return true;
}

void menu_display_d3d9_scissor_begin(
      video_frame_info_t *video_info,
      int x, int y, unsigned width, unsigned height)
{
   RECT rect;
   d3d9_video_t *d3d9 = (d3d9_video_t*)video_info->userdata;

   if (!d3d9 || !width || !height)
      return;

   rect.left          = x;
   rect.top           = y;
   rect.right         = width + x;
   rect.bottom        = height + y;

   d3d9_set_scissor_rect(d3d9->dev, &rect);
}

void menu_display_d3d9_scissor_end(video_frame_info_t *video_info)
{
   RECT rect;
   d3d9_video_t *d3d9 = (d3d9_video_t*)video_info->userdata;

   if (!d3d9)
      return;

   rect.left            = 0;
   rect.top             = 0;
   rect.right           = video_info->width;
   rect.bottom          = video_info->height;

   d3d9_set_scissor_rect(d3d9->dev, &rect);
}

menu_display_ctx_driver_t menu_display_ctx_d3d9 = {
   menu_display_d3d9_draw,
   menu_display_d3d9_draw_pipeline,
   menu_display_d3d9_viewport,
   menu_display_d3d9_blend_begin,
   menu_display_d3d9_blend_end,
   menu_display_d3d9_restore_clear_color,
   menu_display_d3d9_clear_color,
   menu_display_d3d9_get_default_mvp,
   menu_display_d3d9_get_default_vertices,
   menu_display_d3d9_get_default_tex_coords,
   menu_display_d3d9_font_init_first,
   MENU_VIDEO_DRIVER_DIRECT3D9,
   "d3d9",
   false,
   menu_display_d3d9_scissor_begin,
   menu_display_d3d9_scissor_end
};

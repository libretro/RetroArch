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

#include <retro_miscellaneous.h>

#include <gfx/math/matrix_4x4.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../menu_driver.h"

#include "../../gfx/video_driver.h"
#include "../../gfx/drivers/d3d.h"
#include "../../gfx/common/d3d_common.h"

#define BYTE_CLAMP(i) (int) ((((i) > 255) ? 255 : (((i) < 0) ? 0 : (i))))

static const float d3d_vertexes[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const float d3d_tex_coords[] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static const float *menu_display_d3d_get_default_vertices(void)
{
   return &d3d_vertexes[0];
}

static const float *menu_display_d3d_get_default_tex_coords(void)
{
   return &d3d_tex_coords[0];
}

static void *menu_display_d3d_get_default_mvp(void)
{
   static math_matrix_4x4 id;
   matrix_4x4_identity(id);

   return &id;
}

static D3DPRIMITIVETYPE menu_display_prim_to_d3d_enum(
      enum menu_display_prim_type prim_type)
{
   switch (prim_type)
   {
      case MENU_DISPLAY_PRIM_TRIANGLES:
      case MENU_DISPLAY_PRIM_TRIANGLESTRIP:
         return D3DPT_TRIANGLESTRIP;
      case MENU_DISPLAY_PRIM_NONE:
      default:
         break;
   }

   /* TOD/FIXME - hack */
   return (D3DPRIMITIVETYPE)0;
}

static void menu_display_d3d_blend_begin(void)
{
   d3d_video_t *d3d = (d3d_video_t*)video_driver_get_ptr(false);

   if (!d3d)
      return;

   d3d_enable_blend_func(d3d->dev);
}

static void menu_display_d3d_blend_end(void)
{
   d3d_video_t *d3d = (d3d_video_t*)video_driver_get_ptr(false);

   if (!d3d)
      return;

   d3d_disable_blend_func(d3d->dev);
}

static void menu_display_d3d_viewport(void *data)
{
}

static void menu_display_d3d_bind_texture(void *data)
{
   d3d_video_t              *d3d = (d3d_video_t*)video_driver_get_ptr(false);
   menu_display_ctx_draw_t *draw = (menu_display_ctx_draw_t*)data;

   if (!d3d || !draw || !draw->texture)
      return;


   d3d_set_texture(d3d->dev, 0, (void*)draw->texture);
   d3d_set_sampler_address_u(d3d->dev, 0, D3DTADDRESS_CLAMP);
   d3d_set_sampler_address_v(d3d->dev, 0, D3DTADDRESS_CLAMP);
   d3d_set_sampler_minfilter(d3d->dev, 0, D3DTEXF_LINEAR);
   d3d_set_sampler_magfilter(d3d->dev, 0, D3DTEXF_LINEAR);
   d3d_set_sampler_mipfilter(d3d->dev, 0, D3DTEXF_LINEAR);

}

static void menu_display_d3d_draw(void *data)
{
   unsigned i;
   video_shader_ctx_mvp_t mvp;
   math_matrix_4x4 mop, m1, m2;
   unsigned width, height;
   d3d_video_t *d3d              = (d3d_video_t*)video_driver_get_ptr(false);   
   menu_display_ctx_draw_t *draw = (menu_display_ctx_draw_t*)data;
   Vertex * pv                   = NULL;
   const float *vertex           = NULL;
   const float *tex_coord        = NULL;
   const float *color            = NULL;

   if (!d3d || !draw || draw->pipeline.id)
      return;
   if((d3d->menu_display.offset + draw->coords->vertices )
         > d3d->menu_display.size)
      return;

   pv           = (Vertex*)
      d3d_vertex_buffer_lock(d3d->menu_display.buffer);

   if (!pv)
      return;

   pv          += d3d->menu_display.offset;
   vertex       = draw->coords->vertex;
   tex_coord    = draw->coords->tex_coord;
   color        = draw->coords->color;

   if (!vertex)
      vertex    = menu_display_d3d_get_default_vertices();
   if (!tex_coord)
      tex_coord = menu_display_d3d_get_default_tex_coords();

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
   d3d_vertex_buffer_unlock(d3d->menu_display.buffer);

   if(!draw->matrix_data)
      draw->matrix_data = menu_display_d3d_get_default_mvp();

   /* ugh */
   video_driver_get_size(&width, &height);
   matrix_4x4_scale(m1,       2.0,  2.0, 0);
   matrix_4x4_translate(mop, -1.0, -1.0, 0);
   matrix_4x4_multiply(m2, mop, m1);
   matrix_4x4_multiply(m1,
         *((math_matrix_4x4*)draw->matrix_data), m2);
   matrix_4x4_scale(mop,
         (draw->width  / 2.0) / width,
         (draw->height / 2.0) / height, 0);
   matrix_4x4_multiply(m2, mop, m1);
   matrix_4x4_translate(mop,
         (draw->x + (draw->width  / 2.0)) / width,
         (draw->y + (draw->height / 2.0)) / height,
         0);
   matrix_4x4_multiply(m1, mop, m2);
   matrix_4x4_multiply(m2, d3d->mvp_transposed, m1);
   d3d_matrix_transpose(&m1, &m2);

   mvp.data   = d3d;
   mvp.matrix = &m1;
   video_driver_set_mvp(&mvp);
   menu_display_d3d_bind_texture(draw);
   d3d_draw_primitive(d3d->dev,
         menu_display_prim_to_d3d_enum(draw->prim_type),
         d3d->menu_display.offset,
         draw->coords->vertices - 
         ((draw->prim_type == MENU_DISPLAY_PRIM_TRIANGLESTRIP) 
          ? 2 : 0));

   d3d->menu_display.offset += draw->coords->vertices;
}

static void menu_display_d3d_draw_pipeline(void *data)
{
#if defined(HAVE_HLSL) || defined(HAVE_CG)
   menu_display_ctx_draw_t *draw     = (menu_display_ctx_draw_t*)data;
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

static void menu_display_d3d_restore_clear_color(void)
{
   /* not needed */
}

static void menu_display_d3d_clear_color(
      menu_display_ctx_clearcolor_t *clearcolor)
{
   DWORD    clear_color = 0;
   d3d_video_t     *d3d = (d3d_video_t*)
      video_driver_get_ptr(false);

   if (!d3d || !clearcolor)
      return;

   clear_color = D3DCOLOR_ARGB(
         BYTE_CLAMP(clearcolor->a * 255.0f), /* A */
         BYTE_CLAMP(clearcolor->r * 255.0f), /* R */
         BYTE_CLAMP(clearcolor->g * 255.0f), /* G */
         BYTE_CLAMP(clearcolor->b * 255.0f)  /* B */
         );

   d3d_clear(d3d->dev, 0, NULL, D3DCLEAR_TARGET, clear_color, 0, 0);
}

static bool menu_display_d3d_font_init_first(
      void **font_handle, void *video_data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   font_data_t **handle = (font_data_t**)font_handle;
   if (!(*handle = font_driver_init_first(video_data,
         font_path, font_size, true,
         is_threaded,
         FONT_DRIVER_RENDER_DIRECT3D_API)))
		 return false;
   return true;
}

menu_display_ctx_driver_t menu_display_ctx_d3d = {
   menu_display_d3d_draw,
   menu_display_d3d_draw_pipeline,
   menu_display_d3d_viewport,
   menu_display_d3d_blend_begin,
   menu_display_d3d_blend_end,
   menu_display_d3d_restore_clear_color,
   menu_display_d3d_clear_color,
   menu_display_d3d_get_default_mvp,
   menu_display_d3d_get_default_vertices,
   menu_display_d3d_get_default_tex_coords,
   menu_display_d3d_font_init_first,
   MENU_VIDEO_DRIVER_DIRECT3D,
   "menu_display_d3d",
};

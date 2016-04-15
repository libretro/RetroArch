/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include "../../config.def.h"
#include "../../retroarch.h"
#include "../../gfx/font_driver.h"
#include "../../gfx/video_context_driver.h"
#include "../../gfx/d3d/d3d.h"
#include "../../gfx/common/d3d_common.h"

#include "../menu_display.h"

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

static d3d_video_t *d3d_get_ptr(void)
{
   d3d_video_t *d3d = (d3d_video_t*)video_driver_get_ptr(false);

   if (!d3d)
      return NULL;
   return d3d;
}

static void *menu_display_d3d_get_default_mvp(void)
{
#ifndef _XBOX
   static math_matrix_4x4 default_mvp;
   D3DXMATRIX ortho, mvp;
#endif
   d3d_video_t *d3d = d3d_get_ptr();

   if (!d3d)
      return NULL;
#ifdef _XBOX
   return NULL; /* TODO/FIXME */
#else
   D3DXMatrixOrthoOffCenterLH(&ortho, 0,
         d3d->final_viewport.Width, 0, d3d->final_viewport.Height, 0, 1);
   D3DXMatrixTranspose(&mvp, &ortho);
   memcpy(default_mvp.data, (FLOAT*)mvp, sizeof(default_mvp.data));

   return &default_mvp;
#endif
}

static unsigned menu_display_prim_to_d3d_enum(
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

   return 0;
}

static void menu_display_d3d_blend_begin(void)
{
   d3d_video_t *d3d = d3d_get_ptr();

   if (!d3d)
      return;

   d3d_enable_blend_func(d3d->dev);
}

static void menu_display_d3d_blend_end(void)
{
   d3d_video_t *d3d = d3d_get_ptr();

   if (!d3d)
      return;

   d3d_disable_blend_func(d3d->dev);
}

static void menu_display_d3d_draw(void *data)
{
   D3DVIEWPORT                vp = {0};
   math_matrix_4x4          *mat = NULL;
   d3d_video_t              *d3d = d3d_get_ptr();
   menu_display_ctx_draw_t *draw = (menu_display_ctx_draw_t*)data;

   if (!d3d || !draw)
      return;
   
   mat = (math_matrix_4x4*)draw->matrix_data;

   /* TODO - edge case */
   if (draw->height <= 0)
      draw->height = 1;

   if (!mat)
      mat                         = (math_matrix_4x4*)
         menu_display_d3d_get_default_mvp();
   if (!draw->coords->vertex)
      draw->coords->vertex        = menu_display_d3d_get_default_vertices();
   if (!draw->coords->tex_coord)
      draw->coords->tex_coord     = menu_display_d3d_get_default_tex_coords();
   if (!draw->coords->lut_tex_coord)
      draw->coords->lut_tex_coord = menu_display_d3d_get_default_tex_coords();

   vp.X      = draw->x;
   vp.Y      = draw->y;
   vp.Width  = draw->width;
   vp.Height = draw->height;
   vp.MinZ   = 0.0f;
   vp.MaxZ   = 1.0f;

   d3d_set_viewports(d3d->dev, &vp);
   d3d_set_texture(d3d->dev, 0, (LPDIRECT3DTEXTURE)draw->texture);

#if 0
   video_shader_driver_set_coords(d3d, draw->coords);
   video_shader_driver_set_mvp(d3d, mat);
#endif

   d3d_draw_primitive(d3d->dev, (D3DPRIMITIVETYPE)
         menu_display_prim_to_d3d_enum(draw->prim_type),
         0, draw->coords->vertices);
}

static void menu_display_d3d_draw_bg(void *data)
{
   struct gfx_coords coords;
   const float    *new_vertex    = NULL;
   const float    *new_tex_coord = NULL;
   menu_display_ctx_draw_t *draw = (menu_display_ctx_draw_t*)data;

   if (!draw)
      return;

   new_vertex          = draw->vertex;
   new_tex_coord        = draw->tex_coord;

   if (!new_vertex)
      new_vertex        = menu_display_d3d_get_default_vertices();
   if (!new_tex_coord)
      new_tex_coord     = menu_display_d3d_get_default_tex_coords()

   coords.vertices      = draw->vertex_count;
   coords.vertex        = new_vertex;
   coords.tex_coord     = new_tex_coord;
   coords.lut_tex_coord = new_tex_coord;
   coords.color         = (const float*)draw->color;

   if (!draw->texture)
      draw->texture     = menu_display_white_texture;

   draw->x           = 0;
   draw->y           = 0;
   draw->coords      = &coords;
   draw->matrix_data = (math_matrix_4x4*)
      menu_display_d3d_get_default_mvp();

   menu_display_d3d_draw(draw);
}

static void menu_display_d3d_restore_clear_color(void)
{
   d3d_video_t     *d3d = d3d_get_ptr();
   DWORD    clear_color = 0x00000000;

   d3d_clear(d3d->dev, 0, NULL, D3DCLEAR_TARGET, clear_color, 0, 0);
}

static void menu_display_d3d_clear_color(void *data)
{
   DWORD    clear_color                      = 0;
   d3d_video_t     *d3d                      = d3d_get_ptr();
   menu_display_ctx_clearcolor_t *clearcolor = 
      (menu_display_ctx_clearcolor_t*)data;

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
      const char *font_path, float font_size)
{
   return font_driver_init_first(NULL, font_handle, video_data,
         font_path, font_size, true, FONT_DRIVER_RENDER_DIRECT3D_API);
}

menu_display_ctx_driver_t menu_display_ctx_d3d = {
   menu_display_d3d_draw,
   menu_display_d3d_draw_bg,
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

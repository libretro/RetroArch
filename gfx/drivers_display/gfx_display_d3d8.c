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

#include "../gfx_display.h"

#include "../../retroarch.h"
#include "../common/d3d_common.h"
#include "../common/d3d8_common.h"

static const float d3d8_vertexes[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const float d3d8_tex_coords[] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static const float *gfx_display_d3d8_get_default_vertices(void)
{
   return &d3d8_vertexes[0];
}

static const float *gfx_display_d3d8_get_default_tex_coords(void)
{
   return &d3d8_tex_coords[0];
}

static void *gfx_display_d3d8_get_default_mvp(void *data)
{
   static float id[] =         { 1.0f, 0.0f, 0.0f, 0.0f,
                                 0.0f, 1.0f, 0.0f, 0.0f,
                                 0.0f, 0.0f, 1.0f, 0.0f, 
                                 0.0f, 0.0f, 0.0f, 1.0f
                               };
   return &id;
}

static INT32 gfx_display_prim_to_d3d8_enum(
      enum gfx_display_prim_type prim_type)
{
   switch (prim_type)
   {
      case GFX_DISPLAY_PRIM_TRIANGLES:
      case GFX_DISPLAY_PRIM_TRIANGLESTRIP:
         return D3DPT_COMM_TRIANGLESTRIP;
      case GFX_DISPLAY_PRIM_NONE:
      default:
         break;
   }

   /* TODO/FIXME - hack */
   return 0;
}

static void gfx_display_d3d8_blend_begin(void *data)
{
   d3d8_video_t *d3d             = (d3d8_video_t*)data;

   if (!d3d)
      return;

   IDirect3DDevice8_SetRenderState(d3d->dev, D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
   IDirect3DDevice8_SetRenderState(d3d->dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
   IDirect3DDevice8_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE, true);
}

static void gfx_display_d3d8_blend_end(void *data)
{
   d3d8_video_t *d3d             = (d3d8_video_t*)data;

   if (!d3d)
      return;

   IDirect3DDevice8_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE, false);
}

static void gfx_display_d3d8_draw(gfx_display_ctx_draw_t *draw,
      void *data,
      unsigned video_width, 
      unsigned video_height)
{
   static float default_mvp[] ={ 1.0f, 0.0f, 0.0f, 0.0f,
                                 0.0f, 1.0f, 0.0f, 0.0f,
                                 0.0f, 0.0f, 1.0f, 0.0f, 
                                 0.0f, 0.0f, 0.0f, 1.0f
                               };
   unsigned i;
   math_matrix_4x4 mop, m1, m2;
   LPDIRECT3DVERTEXBUFFER8 vbo;
   LPDIRECT3DDEVICE8 dev;
   D3DPRIMITIVETYPE type;
   unsigned start                = 0;
   unsigned count                = 0;
   d3d8_video_t *d3d             = (d3d8_video_t*)data;
   Vertex * pv                   = NULL;
   const float *vertex           = NULL;
   const float *tex_coord        = NULL;
   const float *color            = NULL;

   if (!d3d || !draw || draw->pipeline_id)
      return;
   if ((d3d->menu_display.offset + draw->coords->vertices )
         > (unsigned)d3d->menu_display.size)
      return;
   vbo                           = (LPDIRECT3DVERTEXBUFFER8)d3d->menu_display.buffer;
   dev                           = d3d->dev;
   pv                            = (Vertex*)d3d8_vertex_buffer_lock(vbo);

   if (!pv)
      return;

   pv          += d3d->menu_display.offset;
   vertex       = draw->coords->vertex;
   tex_coord    = draw->coords->tex_coord;
   color        = draw->coords->color;

   if (!vertex)
      vertex    = &d3d8_vertexes[0];
   if (!tex_coord)
      tex_coord = &d3d8_tex_coords[0];

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

      if ((void*)draw->texture)
      {
         D3DSURFACE_DESC desc;
         LPDIRECT3DTEXTURE8 tex = (LPDIRECT3DTEXTURE8)draw->texture;
         if (SUCCEEDED(IDirect3DTexture8_GetLevelDesc(tex,
                     0, (D3DSURFACE_DESC*)&desc)))
         {
            pv[i].u *= desc.Width;
            pv[i].v *= desc.Height;
         }
      }

      pv[i].color =
         D3DCOLOR_ARGB(
               colors[3], /* A */
               colors[0], /* R */
               colors[1], /* G */
               colors[2]  /* B */
               );
   }
   IDirect3DVertexBuffer8_Unlock(vbo);

   if (!draw->matrix_data)
      draw->matrix_data = &default_mvp;

   /* ugh */
   matrix_4x4_scale(m1,       2.0,  2.0, 0);
   matrix_4x4_translate(mop, -1.0, -1.0, 0);
   matrix_4x4_multiply(m2, mop, m1);
   matrix_4x4_multiply(m1,
         *((math_matrix_4x4*)draw->matrix_data), m2);
   matrix_4x4_scale(mop,
         (draw->width  / 2.0) / video_width,
         (draw->height / 2.0) / video_height, 0);
   matrix_4x4_multiply(m2, mop, m1);
   matrix_4x4_translate(mop,
         (draw->x + (draw->width  / 2.0)) / video_width,
         (draw->y + (draw->height / 2.0)) / video_height,
         0);
   matrix_4x4_multiply(m1, mop, m2);
   matrix_4x4_multiply(m2, d3d->mvp_transposed, m1);
   d3d_matrix_transpose(&m1, &m2);

   d3d8_set_mvp(dev, &m1);

   if (draw->texture)
   {
      IDirect3DDevice8_SetTexture(dev, 0,
            (IDirect3DBaseTexture8*)draw->texture);
      IDirect3DDevice8_SetTextureStageState(dev, 0,
            (D3DTEXTURESTAGESTATETYPE)D3DTSS_ADDRESSU, D3DTADDRESS_COMM_CLAMP);
      IDirect3DDevice8_SetTextureStageState(dev, 0,
            (D3DTEXTURESTAGESTATETYPE)D3DTSS_ADDRESSV, D3DTADDRESS_COMM_CLAMP);
      IDirect3DDevice8_SetTextureStageState(dev, 0,
            (D3DTEXTURESTAGESTATETYPE)D3DTSS_MINFILTER, D3DTEXF_COMM_LINEAR);
      IDirect3DDevice8_SetTextureStageState(dev, 0,
            (D3DTEXTURESTAGESTATETYPE)D3DTSS_MAGFILTER, D3DTEXF_COMM_LINEAR);
   }

   type  = gfx_display_prim_to_d3d8_enum(draw->prim_type);
   start = d3d->menu_display.offset;
   count = draw->coords->vertices -
         ((draw->prim_type == GFX_DISPLAY_PRIM_TRIANGLESTRIP)
          ? 2 : 0);

   IDirect3DDevice8_BeginScene(dev);
   IDirect3DDevice8_DrawPrimitive(dev, type, start, count);
   IDirect3DDevice8_EndScene(dev);

   d3d->menu_display.offset += draw->coords->vertices;
}

gfx_display_ctx_driver_t gfx_display_ctx_d3d8 = {
   gfx_display_d3d8_draw,
   NULL,                                        /* draw_pipeline */
   gfx_display_d3d8_blend_begin,
   gfx_display_d3d8_blend_end,
   gfx_display_d3d8_get_default_mvp,
   gfx_display_d3d8_get_default_vertices,
   gfx_display_d3d8_get_default_tex_coords,
   FONT_DRIVER_RENDER_D3D8_API,
   GFX_VIDEO_DRIVER_DIRECT3D8,
   "d3d8",
   false,
   NULL,
   NULL
};

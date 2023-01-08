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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../gfx_display.h"

#include "../common/rsx_common.h"

static const float rsx_vertexes[8] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const float rsx_tex_coords[8] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static const float *gfx_display_rsx_get_default_vertices(void)
{
   return &rsx_vertexes[0];
}

static const float *gfx_display_rsx_get_default_tex_coords(void)
{
   return &rsx_tex_coords[0];
}

static void *gfx_display_rsx_get_default_mvp(void *data)
{
   rsx_t *rsx = (rsx_t*)data;

   if (!rsx)
      return NULL;

   return &rsx->mvp_no_rot;
}

static void gfx_display_rsx_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   unsigned i;
   rsx_texture_t *texture           = NULL;
   const float *vertex              = NULL;
   const float *tex_coord           = NULL;
   const float *color               = NULL;
   rsx_t             *rsx = (rsx_t*)data;
   rsx_viewport_t vp;

   if (!rsx || !draw)
      return;

   texture            = (rsx_texture_t *)draw->texture;
   vertex             = draw->coords->vertex;
   tex_coord          = draw->coords->tex_coord;
   color              = draw->coords->color;

   if (!vertex)
      vertex          = &rsx_vertexes[0];
   if (!tex_coord)
      tex_coord       = &rsx_tex_coords[0];
   if (!draw->coords->lut_tex_coord)
      draw->coords->lut_tex_coord = &rsx_tex_coords[0];
   if (!draw->texture)
      return;

   vp.x         = fabs(draw->x);
   vp.y         = fabs(rsx->height - draw->y - draw->height);
   vp.w         = (draw->width  <= rsx->width)  ? draw->width  : rsx->width;
   vp.h         = (draw->height <= rsx->height) ? draw->height : rsx->height;
   vp.min       = 0.0f;
   vp.max       = 1.0f;
   vp.scale[0]  = vp.w*0.5f;
   vp.scale[1]  = vp.h*-0.5f;
   vp.scale[2]  = (vp.max - vp.min)*0.5f;
   vp.scale[3]  = 0.0f;
   vp.offset[0] = vp.x + vp.w*0.5f;
   vp.offset[1] = vp.y + vp.h*0.5f;
   vp.offset[2] = (vp.max + vp.min)*0.5f;
   vp.offset[3] = 0.0f;

   rsxSetViewport(rsx->context, vp.x, vp.y, vp.w, vp.h, vp.min, vp.max, vp.scale, vp.offset);

   rsxInvalidateTextureCache(rsx->context, GCM_INVALIDATE_TEXTURE);
   rsxLoadTexture(rsx->context, rsx->tex_unit->index, &texture->tex);
   rsxTextureControl(rsx->context, rsx->tex_unit->index, GCM_TRUE, 0 << 8, 12 << 8, GCM_TEXTURE_MAX_ANISO_1);
   rsxTextureFilter(rsx->context, rsx->tex_unit->index, 0, texture->min_filter, texture->mag_filter, GCM_TEXTURE_CONVOLUTION_QUINCUNX);
   rsxTextureWrapMode(rsx->context, rsx->tex_unit->index, texture->wrap_s, texture->wrap_t, GCM_TEXTURE_CLAMP_TO_EDGE, 0, GCM_TEXTURE_ZFUNC_LESS, 0);

   for (i = 0; i < draw->coords->vertices; i++)
   {
      rsx->vertices[i].x = *vertex++;
      rsx->vertices[i].y = *vertex++;
      rsx->vertices[i].z = 0.0f;
      rsx->vertices[i].u = *tex_coord++;
      rsx->vertices[i].v = *tex_coord++;
      rsx->vertices[i].r = *color++;
      rsx->vertices[i].g = *color++;
      rsx->vertices[i].b = *color++;
      rsx->vertices[i].a = *color++;
   }
   rsxBindVertexArrayAttrib(rsx->context, rsx->pos_index->index, 0, rsx->pos_offset, sizeof(rsx_vertex_t), 3, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
   rsxBindVertexArrayAttrib(rsx->context, rsx->uv_index->index, 0, rsx->uv_offset, sizeof(rsx_vertex_t), 2, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
   rsxBindVertexArrayAttrib(rsx->context, rsx->col_index->index, 0, rsx->col_offset, sizeof(rsx_vertex_t), 4, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

   rsxSetVertexProgramParameter(rsx->context,
         rsx->vpo, rsx->proj_matrix, (float *)&rsx->mvp_no_rot);
   rsxClearSurface(rsx->context, GCM_CLEAR_Z);
   rsxDrawVertexArray(rsx->context, GCM_TYPE_TRIANGLE_STRIP, 0, draw->coords->vertices);
}

static void gfx_display_rsx_scissor_begin(void *data,
      unsigned video_width,
      unsigned video_height,
      int x, int y,
      unsigned width, unsigned height)
{
   rsx_t *rsx = (rsx_t *)data;
   rsxSetScissor(rsx->context, x, video_height - y - height, width, height);
}

static void gfx_display_rsx_scissor_end(
      void *data,
      unsigned video_width,
      unsigned video_height)
{
   rsx_t *rsx = (rsx_t *)data;
   rsxSetScissor(rsx->context, 0, 0, video_width, video_height);
}

static void gfx_display_rsx_blend_begin(void *data)
{
   rsx_t *rsx = (rsx_t *)data;
	 rsxSetBlendEnable(rsx->context, GCM_TRUE);
   rsxSetBlendFunc(rsx->context, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA);
   rsxSetBlendEquation(rsx->context, GCM_FUNC_ADD, GCM_FUNC_ADD);
#if 0
   rsxSetBlendEnableMrt(rsx->context, GCM_TRUE, GCM_TRUE, GCM_TRUE);
   rsxSetDepthFunc(rsx->context, GCM_LESS);
   rsxSetDepthTestEnable(rsx->context, GCM_FALSE);
   rsxSetAlphaFunc(rsx->context, GCM_ALWAYS, 0);
   rsxSetAlphaTestEnable(rsx->context, GCM_TRUE);
#endif
}

static void gfx_display_rsx_blend_end(void *data)
{
   rsx_t *rsx = (rsx_t *)data;
   rsxSetBlendEnable(rsx->context, GCM_FALSE);
#if 0
   rsxSetBlendEnableMrt(rsx->context, GCM_FALSE, GCM_FALSE, GCM_FALSE);
#endif
}

gfx_display_ctx_driver_t gfx_display_ctx_rsx = {
   gfx_display_rsx_draw,
   NULL,                                        /* draw_pipeline */
   gfx_display_rsx_blend_begin,
   gfx_display_rsx_blend_end,
   gfx_display_rsx_get_default_mvp,
   gfx_display_rsx_get_default_vertices,
   gfx_display_rsx_get_default_tex_coords,
   FONT_DRIVER_RENDER_RSX,
   GFX_VIDEO_DRIVER_RSX,
   "rsx",
   true,
   gfx_display_rsx_scissor_begin,
   gfx_display_rsx_scissor_end
};

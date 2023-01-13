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

#include "../common/ctr_common.h"
#include "../drivers/ctr_gu.h"
#include "../../ctr/gpu_old.h"

static void gfx_display_ctr_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   ctr_scale_vector_t scale_vector;
   int colorR, colorG, colorB, colorA;
   ctr_vertex_t *v                  = NULL;
   struct ctr_texture *texture      = NULL;
   const float *color               = NULL;
   ctr_video_t             *ctr     = (ctr_video_t*)data;

   if (!ctr || !draw)
      return;

   texture            = (struct ctr_texture*)draw->texture;
   color              = draw->coords->color;

   if (!texture)
      return;

   ctr_set_scale_vector(&scale_vector,
         CTR_TOP_FRAMEBUFFER_WIDTH, CTR_TOP_FRAMEBUFFER_HEIGHT,
         texture->width, texture->height);
   GPUCMD_AddWrite(GPUREG_GSH_BOOLUNIFORM, 0);
   ctrGuSetVertexShaderFloatUniform(0, (float*)&scale_vector, 1);

   if ((ctr->vertex_cache.size - (ctr->vertex_cache.current 
               - ctr->vertex_cache.buffer)) < 1)
      ctr->vertex_cache.current = ctr->vertex_cache.buffer;

   v     = ctr->vertex_cache.current++;

   v->x0 = draw->x;
   v->y0 = 240 - draw->height - draw->y;
   v->x1 = v->x0 + draw->width;
   v->y1 = v->y0 + draw->height;
   v->u0 = 0;
   v->v0 = 0;
   v->u1 = texture->active_width;
   v->v1 = texture->active_height;

   ctrGuSetAttributeBuffers(2,
         VIRT_TO_PHYS(v),
         CTRGU_ATTRIBFMT(GPU_SHORT, 4) << 0 |
         CTRGU_ATTRIBFMT(GPU_SHORT, 4) << 4,
         sizeof(ctr_vertex_t));

   color  = draw->coords->color;
   colorR = (int)((*color++)*255.f);
   colorG = (int)((*color++)*255.f);
   colorB = (int)((*color++)*255.f);
   colorA = (int)((*color++)*255.f);

   GPU_SetTexEnv(0,
         GPU_TEVSOURCES(GPU_TEXTURE0, GPU_CONSTANT, 0),
         GPU_TEVSOURCES(GPU_TEXTURE0, GPU_CONSTANT, 0),
         0,
         0,
         GPU_MODULATE, GPU_MODULATE,
         COLOR_ABGR(colorR,colorG,colorB,colorA)
         );

#if 0
   GPU_SetTexEnv(0,
         GPU_TEVSOURCES(GPU_CONSTANT, GPU_CONSTANT, 0),
         GPU_TEVSOURCES(GPU_CONSTANT, GPU_CONSTANT, 0),
         0,
         GPU_TEVOPERANDS(GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, 0),
         GPU_REPLACE, GPU_REPLACE,
         0x3FFFFFFF);
#endif

   ctrGuSetTexture(GPU_TEXUNIT0,
         VIRT_TO_PHYS(texture->data),
         texture->width,
         texture->height,
           GPU_TEXTURE_MAG_FILTER(GPU_LINEAR)  
         | GPU_TEXTURE_MIN_FILTER(GPU_LINEAR) 
         | GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_EDGE)
         | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_EDGE),
         GPU_RGBA8);

   GPU_SetViewport(NULL,
         VIRT_TO_PHYS(ctr->drawbuffers.top.left),
         0, 0, CTR_TOP_FRAMEBUFFER_HEIGHT,
         ctr->video_mode == CTR_VIDEO_MODE_2D_800X240 
         ? CTR_TOP_FRAMEBUFFER_WIDTH * 2 
         : CTR_TOP_FRAMEBUFFER_WIDTH);

   GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, 1);

   if (ctr->video_mode == CTR_VIDEO_MODE_3D)
   {
      GPU_SetViewport(NULL,
            VIRT_TO_PHYS(ctr->drawbuffers.top.right),
            0, 0, CTR_TOP_FRAMEBUFFER_HEIGHT,
            CTR_TOP_FRAMEBUFFER_WIDTH);
      GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, 1);
   }

   GPU_SetTexEnv(0, GPU_TEXTURE0, GPU_TEXTURE0, 0, 0, GPU_REPLACE, GPU_REPLACE, 0);
}

gfx_display_ctx_driver_t gfx_display_ctx_ctr = {
   gfx_display_ctr_draw,
   NULL,                                     /* draw_pipeline          */
   NULL,                                     /* blend_begin            */
   NULL,                                     /* blend_end              */
   NULL,                                     /* get_default_mvp        */
   NULL,                                     /* get_default_vertices   */
   NULL,                                     /* get_default_tex_coords */
   FONT_DRIVER_RENDER_CTR,
   GFX_VIDEO_DRIVER_CTR,
   "ctr",
   true,
   NULL,
   NULL
};

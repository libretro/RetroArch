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

#include "../menu_driver.h"

#include "../../retroarch.h"
#include "../../gfx/font_driver.h"
#include "../../gfx/common/ctr_common.h"
#include "../../gfx/drivers/ctr_gu.h"
#include "../../ctr/gpu_old.h"

static const float *menu_display_ctr_get_default_vertices(void)
{
   return NULL;
}

static const float *menu_display_ctr_get_default_tex_coords(void)
{
   return NULL;
}

static void *menu_display_ctr_get_default_mvp(video_frame_info_t *video_info)
{
   return NULL;
}

static void menu_display_ctr_blend_begin(video_frame_info_t *video_info)
{

}

static void menu_display_ctr_blend_end(video_frame_info_t *video_info)
{

}

static void menu_display_ctr_viewport(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{

}

static void menu_display_ctr_draw(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
   struct ctr_texture *texture      = NULL;
   const float *color               = NULL;
   ctr_video_t             *ctr     = (ctr_video_t*)video_info->userdata;

   if (!ctr || !draw)
      return;

   texture            = (struct ctr_texture*)draw->texture;
   color              = draw->coords->color;

   if (!texture)
      return;

   ctr_scale_vector_t scale_vector;
   ctr_set_scale_vector(&scale_vector,
         CTR_TOP_FRAMEBUFFER_WIDTH, CTR_TOP_FRAMEBUFFER_HEIGHT,
         texture->width, texture->height);
   ctrGuSetVertexShaderFloatUniform(0, (float*)&scale_vector, 1);

   if ((ctr->vertex_cache.size - (ctr->vertex_cache.current - ctr->vertex_cache.buffer)) < 1)
      ctr->vertex_cache.current = ctr->vertex_cache.buffer;

   ctr_vertex_t* v = ctr->vertex_cache.current++;

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

   color = draw->coords->color;
   int colorR = (int)((*color++)*255.f);
   int colorG = (int)((*color++)*255.f);
   int colorB = (int)((*color++)*255.f);
   int colorA = (int)((*color++)*255.f);

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

   ctrGuSetTexture(GPU_TEXUNIT0, VIRT_TO_PHYS(texture->data), texture->width, texture->height,
         GPU_TEXTURE_MAG_FILTER(GPU_LINEAR)  | GPU_TEXTURE_MIN_FILTER(GPU_LINEAR) |
         GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_EDGE) | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_EDGE),
         GPU_RGBA8);

   GPU_SetViewport(NULL,
         VIRT_TO_PHYS(ctr->drawbuffers.top.left),
         0, 0, CTR_TOP_FRAMEBUFFER_HEIGHT,
         ctr->video_mode == CTR_VIDEO_MODE_2D_800x240 ?
         CTR_TOP_FRAMEBUFFER_WIDTH * 2 : CTR_TOP_FRAMEBUFFER_WIDTH);

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
#if 0
   printf("(%i,%i,%i,%i) , (%i,%i)\n", (int)draw->x,
         (int)draw->y, (int)draw->width, (int)draw->height,
         texture->width, texture->height);
#endif
}

static void menu_display_ctr_draw_pipeline(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
}

static void menu_display_ctr_restore_clear_color(void)
{
#if 0
   ctr_set_clear_color(RGBA8(0x00, 0x00, 0x00, 0xFF));
#endif
}

static void menu_display_ctr_clear_color(menu_display_ctx_clearcolor_t *clearcolor, video_frame_info_t *video_info)
{
   if (!clearcolor)
      return;
#if 0
   ctr_set_clear_color(RGBA8((int)(clearcolor->r*255.f),
            (int)(clearcolor->g*255.f),
            (int)(clearcolor->b*255.f),
            (int)(clearcolor->a*255.f)));
   ctr_clear_screen();
#endif
}

static bool menu_display_ctr_font_init_first(
      void **font_handle, void *video_data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   font_data_t **handle = (font_data_t**)font_handle;
   *handle = font_driver_init_first(video_data,
         font_path, font_size, true,
         is_threaded,
         FONT_DRIVER_RENDER_CTR);
   return *handle;
}

menu_display_ctx_driver_t menu_display_ctx_ctr = {
   menu_display_ctr_draw,
   menu_display_ctr_draw_pipeline,
   menu_display_ctr_viewport,
   menu_display_ctr_blend_begin,
   menu_display_ctr_blend_end,
   menu_display_ctr_restore_clear_color,
   menu_display_ctr_clear_color,
   menu_display_ctr_get_default_mvp,
   menu_display_ctr_get_default_vertices,
   menu_display_ctr_get_default_tex_coords,
   menu_display_ctr_font_init_first,
   MENU_VIDEO_DRIVER_CTR,
   "ctr",
   true,
   NULL,
   NULL
};

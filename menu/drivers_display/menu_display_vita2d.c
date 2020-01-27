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

#include <vita2d.h>

#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../menu_driver.h"

#include "../../retroarch.h"
#include "../../gfx/font_driver.h"
#include "../../gfx/common/vita2d_common.h"
#include "../../defines/psp_defines.h"

static const float vita2d_vertexes[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const float vita2d_tex_coords[] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static const float vita2d_colors[] = {
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
};

static const float *menu_display_vita2d_get_default_vertices(void)
{
   return &vita2d_vertexes[0];
}

static const float *menu_display_vita2d_get_default_color(void)
{
   return &vita2d_colors[0];
}

static const float *menu_display_vita2d_get_default_tex_coords(void)
{
   return &vita2d_tex_coords[0];
}

static void *menu_display_vita2d_get_default_mvp(
      video_frame_info_t *video_info)
{
   vita_video_t *vita2d = (vita_video_t*)video_info->userdata;

   if (!vita2d)
      return NULL;

   return &vita2d->mvp_no_rot;
}

static void menu_display_vita2d_blend_begin(video_frame_info_t *video_info)
{

}

static void menu_display_vita2d_blend_end(video_frame_info_t *video_info)
{

}

static void menu_display_vita2d_viewport(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
   if (draw){
      vita2d_set_viewport(draw->x, draw->y, draw->width, draw->height);
   }
}

static void menu_display_vita2d_draw(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
    unsigned i;
    struct vita2d_texture *texture   = NULL;
    const float *vertex              = NULL;
    const float *tex_coord           = NULL;
    const float *color               = NULL;
    vita_video_t             *vita2d = (vita_video_t*)video_info->userdata;

    if (!vita2d || !draw)
      return;

    texture            = (struct vita2d_texture*)draw->texture;
    vertex             = draw->coords->vertex;
    tex_coord          = draw->coords->tex_coord;
    color              = draw->coords->color;

    if (!vertex)
       vertex          = menu_display_vita2d_get_default_vertices();
    if (!tex_coord)
       tex_coord       = menu_display_vita2d_get_default_tex_coords();
    if (!draw->coords->lut_tex_coord)
       draw->coords->lut_tex_coord = menu_display_vita2d_get_default_tex_coords();
    if (!texture)
       return;
    if (!color)
       color           = menu_display_vita2d_get_default_color();

    menu_display_vita2d_viewport(draw, video_info);
   
    vita2d_texture_tint_vertex *vertices = (vita2d_texture_tint_vertex *)vita2d_pool_memalign(
	   draw->coords->vertices * sizeof(vita2d_texture_tint_vertex),
		sizeof(vita2d_texture_tint_vertex));

    for(i = 0; i < draw->coords->vertices; i++){
      vertices[i].x = *vertex++;
      vertices[i].y = *vertex++;
      vertices[i].z = 1.0f;
      vertices[i].u = *tex_coord++;
      vertices[i].v = *tex_coord++;
      vertices[i].r = *color++; 
      vertices[i].g = *color++;
      vertices[i].b = *color++;
      vertices[i].a = *color++;
    }

    const math_matrix_4x4 *mat = draw->matrix_data
                     ? (const math_matrix_4x4*)draw->matrix_data : (const math_matrix_4x4*)menu_display_vita2d_get_default_mvp(video_info);

   switch (draw->pipeline.id)
   {
     default:
     {
        vita2d_draw_array_textured_mat(texture, vertices, draw->coords->vertices, menu_display_vita2d_get_default_mvp(video_info));
        break;
     }
  }
}

static void menu_display_vita2d_draw_pipeline(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
}

static void menu_display_vita2d_restore_clear_color(void)
{
   vita2d_set_clear_color(RGBA8(0x00, 0x00, 0x00, 0xFF));
}

static void menu_display_vita2d_clear_color(
      menu_display_ctx_clearcolor_t *clearcolor,
      video_frame_info_t *video_info)
{
   if (!clearcolor)
      return;
   vita2d_set_clear_color(RGBA8((int)(clearcolor->r*255.f),
                                (int)(clearcolor->g*255.f),
                                (int)(clearcolor->b*255.f),
                                (int)(clearcolor->a*255.f)));
   vita2d_draw_rectangle(0,0,PSP_FB_WIDTH,PSP_FB_HEIGHT, vita2d_get_clear_color());
}

static bool menu_display_vita2d_font_init_first(
      void **font_handle, void *video_data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   font_data_t **handle = (font_data_t**)font_handle;
   *handle = font_driver_init_first(video_data,
         font_path, font_size, true,
         is_threaded,
         FONT_DRIVER_RENDER_VITA2D);
   return *handle;
}

static void menu_display_vita2d_scissor_end(video_frame_info_t *video_info)
{
   vita2d_set_region_clip(SCE_GXM_REGION_CLIP_NONE, 0, 0, video_info->width, video_info->height);
   vita2d_disable_clipping();
}

static void menu_display_vita2d_scissor_begin(video_frame_info_t *video_info, int x, int y,
      unsigned width, unsigned height)
{
   vita2d_set_clip_rectangle(x, y, x + width, y + height);  
   vita2d_set_region_clip(SCE_GXM_REGION_CLIP_OUTSIDE, x, y, x + width, y + height);
}

menu_display_ctx_driver_t menu_display_ctx_vita2d = {
   menu_display_vita2d_draw,
   menu_display_vita2d_draw_pipeline,
   menu_display_vita2d_viewport,
   menu_display_vita2d_blend_begin,
   menu_display_vita2d_blend_end,
   menu_display_vita2d_restore_clear_color,
   menu_display_vita2d_clear_color,
   menu_display_vita2d_get_default_mvp,
   menu_display_vita2d_get_default_vertices,
   menu_display_vita2d_get_default_tex_coords,
   menu_display_vita2d_font_init_first,
   MENU_VIDEO_DRIVER_VITA2D,
   "vita2d",
   true,
   menu_display_vita2d_scissor_begin,
   menu_display_vita2d_scissor_end
};

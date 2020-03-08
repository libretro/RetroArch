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

#include "../gfx_display.h"

#include "../../retroarch.h"
#include "../font_driver.h"
#include "../common/vita2d_common.h"
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

static const float *gfx_display_vita2d_get_default_vertices(void)
{
   return &vita2d_vertexes[0];
}

static const float *gfx_display_vita2d_get_default_color(void)
{
   return &vita2d_colors[0];
}

static const float *gfx_display_vita2d_get_default_tex_coords(void)
{
   return &vita2d_tex_coords[0];
}

static void *gfx_display_vita2d_get_default_mvp(void *data)
{
   vita_video_t *vita2d = (vita_video_t*)data;

   if (!vita2d)
      return NULL;

   return &vita2d->mvp_no_rot;
}

static void gfx_display_vita2d_blend_begin(void *data) { }
static void gfx_display_vita2d_blend_end(void *data) { }

static void gfx_display_vita2d_viewport(gfx_display_ctx_draw_t *draw,
      void *data)
{
   if (draw)
      vita2d_set_viewport(draw->x, draw->y, draw->width, draw->height);
}

static void gfx_display_vita2d_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
    unsigned i;
    struct vita2d_texture *texture   = NULL;
    const float *vertex              = NULL;
    const float *tex_coord           = NULL;
    const float *color               = NULL;
    vita_video_t             *vita2d = (vita_video_t*)data;

    if (!vita2d || !draw)
      return;

    texture            = (struct vita2d_texture*)draw->texture;
    vertex             = draw->coords->vertex;
    tex_coord          = draw->coords->tex_coord;
    color              = draw->coords->color;

    if (!vertex)
       vertex          = gfx_display_vita2d_get_default_vertices();
    if (!tex_coord)
       tex_coord       = gfx_display_vita2d_get_default_tex_coords();
    if (!draw->coords->lut_tex_coord)
       draw->coords->lut_tex_coord = gfx_display_vita2d_get_default_tex_coords();
    if (!texture)
       return;
    if (!color)
       color           = gfx_display_vita2d_get_default_color();

    gfx_display_vita2d_viewport(draw, vita2d);
   
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
                     ? (const math_matrix_4x4*)draw->matrix_data : (const math_matrix_4x4*)gfx_display_vita2d_get_default_mvp(vita2d);

   switch (draw->pipeline.id)
   {
     default:
     {
        vita2d_draw_array_textured_mat(texture, vertices, draw->coords->vertices, gfx_display_vita2d_get_default_mvp(vita2d));
        break;
     }
  }
}

static void gfx_display_vita2d_draw_pipeline(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height) { }

static void gfx_display_vita2d_restore_clear_color(void)
{
   vita2d_set_clear_color(RGBA8(0x00, 0x00, 0x00, 0xFF));
}

static void gfx_display_vita2d_clear_color(
      gfx_display_ctx_clearcolor_t *clearcolor, void *data)
{
   if (!clearcolor)
      return;
   vita2d_set_clear_color(RGBA8((int)(clearcolor->r*255.f),
                                (int)(clearcolor->g*255.f),
                                (int)(clearcolor->b*255.f),
                                (int)(clearcolor->a*255.f)));
   vita2d_draw_rectangle(0,0,PSP_FB_WIDTH,PSP_FB_HEIGHT, vita2d_get_clear_color());
}

static bool gfx_display_vita2d_font_init_first(
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

static void gfx_display_vita2d_scissor_begin(void *data,
      unsigned video_width,
      unsigned video_height,
      int x, int y,
      unsigned width, unsigned height)
{
   vita2d_set_clip_rectangle(x, y, x + width, y + height);  
   vita2d_set_region_clip(SCE_GXM_REGION_CLIP_OUTSIDE, x, y, x + width, y + height);
}

static void gfx_display_vita2d_scissor_end(
      void *data,
      unsigned video_width,
      unsigned video_height)
{
   vita2d_set_region_clip(SCE_GXM_REGION_CLIP_NONE, 0, 0,
         video_width, video_height);
   vita2d_disable_clipping();
}


gfx_display_ctx_driver_t gfx_display_ctx_vita2d = {
   gfx_display_vita2d_draw,
   gfx_display_vita2d_draw_pipeline,
   gfx_display_vita2d_viewport,
   gfx_display_vita2d_blend_begin,
   gfx_display_vita2d_blend_end,
   gfx_display_vita2d_restore_clear_color,
   gfx_display_vita2d_clear_color,
   gfx_display_vita2d_get_default_mvp,
   gfx_display_vita2d_get_default_vertices,
   gfx_display_vita2d_get_default_tex_coords,
   gfx_display_vita2d_font_init_first,
   GFX_VIDEO_DRIVER_VITA2D,
   "vita2d",
   true,
   gfx_display_vita2d_scissor_begin,
   gfx_display_vita2d_scissor_end
};

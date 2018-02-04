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
#include "../../gfx/video_driver.h"
#include "../../gfx/common/vita2d_common.h"
#include "../../defines/psp_defines.h"

static const float vita2d_vertexes[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const float vita2d_tex_coords[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const float *menu_display_vita2d_get_default_vertices(void)
{
   return &vita2d_vertexes[0];
}

static const float *menu_display_vita2d_get_default_tex_coords(void)
{
   return &vita2d_tex_coords[0];
}

static void *menu_display_vita2d_get_default_mvp(void)
{
   vita_video_t *vita2d = (vita_video_t*)video_driver_get_ptr(false);

   if (!vita2d)
      return NULL;

   return &vita2d->mvp_no_rot;
}

#if 0
static SceGxmPrimitiveType menu_display_prim_to_vita2d_enum(
      enum menu_display_prim_type type)
{
   switch (type)
   {
      case MENU_DISPLAY_PRIM_TRIANGLESTRIP:
         return SCE_GXM_PRIMITIVE_TRIANGLE_STRIP;
      case MENU_DISPLAY_PRIM_TRIANGLES:
         return SCE_GXM_PRIMITIVE_TRIANGLES;
      case MENU_DISPLAY_PRIM_NONE:
      default:
         break;
   }

   return 0;
}
#endif

static void menu_display_vita2d_blend_begin(void)
{

}

static void menu_display_vita2d_blend_end(void)
{

}

static void menu_display_vita2d_viewport(void *data)
{
   vita_video_t             *vita2d          = (vita_video_t*)video_driver_get_ptr(false);
   menu_display_ctx_draw_t *draw = (menu_display_ctx_draw_t*)data;

   if (!vita2d || !draw)
      return;

#if 0
   vita2d_texture_set_wvp(draw->x, draw->y, draw->width, draw->height);
#endif
}


static void menu_display_vita2d_draw(void *data)
{
#if 0
    unsigned i;
#endif
    unsigned int tex_width, tex_height;
    struct vita2d_texture *texture   = NULL;
    const float *vertex              = NULL;
    const float *tex_coord           = NULL;
    const float *color               = NULL;
    vita_video_t             *vita2d = (vita_video_t*)video_driver_get_ptr(false);
    menu_display_ctx_draw_t *draw    = (menu_display_ctx_draw_t*)data;

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
#if 0
    texture         = &vk->display.blank_texture;*/
#endif

    tex_width = vita2d_texture_get_width(texture);
    tex_height = vita2d_texture_get_height(texture);

#if 0
    vita2d_texture_set_program();
    menu_display_vita2d_viewport(draw);

    RARCH_LOG("DRAW BG %d %d \n",draw->width,draw->height);

    vita2d_texture_vertex *pv = (vita2d_texture_vertex *)vita2d_pool_memalign(
          draw->coords->vertices * sizeof(vita2d_texture_vertex), // 4 vertices
          sizeof(vita2d_texture_vertex));

    for (i = 0; i < draw->coords->vertices; i++)
    {
       pv[i].x       = *vertex++;
       pv[i].y       = *vertex++; // Y-flip. Vulkan is top-left clip space
       pv[i].z       = +0.5f;
       pv[i].u       = *tex_coord++;
       pv[i].v       = *tex_coord++;
       snprintf(msg, sizeof(msg), "%.2f %.2f %.2f %.2f %.2f\n",pv[i].x,pv[i].y,pv[i].z,pv[i].u,pv[i].v);
       RARCH_LOG(msg);
       RARCH_LOG("%x %x %x %x %x\n",pv[i].x,pv[i].y,pv[i].z,pv[i].u,pv[i].v);
    }
#endif

   switch (draw->pipeline.id)
   {
     default:
     {

        int colorR = (int)((*color++)*255.f);
        int colorG = (int)((*color++)*255.f);
        int colorB = (int)((*color++)*255.f);
        int colorA = (int)((*color++)*255.f);

#if 0
        vita2d_texture_set_tint_color_uniform(RGBA8((int)((*color++)*255.f), (int)((*color++)*255.f), (int)((*color++)*255.f), (int)((*color++)*255.f)));
        vita2d_texture_set_tint_color_uniform(RGBA8(0xFF, 0xFF, 0xFF, 0xAA));
        vita2d_draw_texture_part_generic(texture, menu_display_prim_to_vita2d_enum(
                 draw->prim_type), pv, draw->coords->vertices);
#endif

        vita2d_draw_texture_tint_scale(texture, draw->x,
                      PSP_FB_HEIGHT-draw->y-draw->height,
                      (float)draw->width/(float)tex_width,
                      (float)draw->height/(float)tex_height,
                      RGBA8(colorR,colorG,colorB,colorA));

#if 0
        if(texture)
           vita2d_draw_texture(NULL,0,0);
#endif
        break;
     }
  }
}

static void menu_display_vita2d_draw_pipeline(void *data)
{
#ifdef HAVE_SHADERPIPELINE

#endif
}

static void menu_display_vita2d_restore_clear_color(void)
{
   vita2d_set_clear_color(RGBA8(0x00, 0x00, 0x00, 0xFF));
}

static void menu_display_vita2d_clear_color(menu_display_ctx_clearcolor_t *clearcolor)
{
   if (!clearcolor)
      return;
   vita2d_set_clear_color(RGBA8((int)(clearcolor->r*255.f),
                                (int)(clearcolor->g*255.f),
                                (int)(clearcolor->b*255.f),
                                (int)(clearcolor->a*255.f)));
   vita2d_draw_rectangle(0,0,PSP_FB_WIDTH,PSP_FB_HEIGHT,RGBA8((int)(clearcolor->r*255.f),
                                (int)(clearcolor->g*255.f),
                                (int)(clearcolor->b*255.f),
                                (int)(clearcolor->a*255.f)));
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
   "menu_display_vita2d",
};

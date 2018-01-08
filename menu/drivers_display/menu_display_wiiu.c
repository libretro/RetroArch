/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel
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

#include <wiiu/gx2.h>
#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "menu/menu_driver.h"

#include "retroarch.h"
#include "gfx/font_driver.h"
#include "gfx/video_driver.h"
#include "gfx/common/gx2_common.h"
#include "wiiu/wiiu_dbg.h"

static const float *menu_display_wiiu_get_default_vertices(void)
{
   return NULL;
}

static const float *menu_display_wiiu_get_default_tex_coords(void)
{
   return NULL;
}

static void *menu_display_wiiu_get_default_mvp(void)
{
   return NULL;
}

static void menu_display_wiiu_blend_begin(void)
{

}

static void menu_display_wiiu_blend_end(void)
{

}

static void menu_display_wiiu_viewport(void *data)
{

}


static void menu_display_wiiu_draw(void *data)
{
   GX2Texture *texture = NULL;
   wiiu_video_t             *wiiu = (wiiu_video_t*)video_driver_get_ptr(false);
   menu_display_ctx_draw_t *draw    = (menu_display_ctx_draw_t*)data;


   if (!wiiu || !draw)
      return;

   texture            = (GX2Texture*)draw->texture;

   if (!texture)
      return;

   if (wiiu->vertex_cache.current + 4 > wiiu->vertex_cache.size)
      return;

   sprite_vertex_t* v = wiiu->vertex_cache.v + wiiu->vertex_cache.current;

   if(draw->coords->vertex && draw->coords->vertices == 4)
   {
      v->pos.x = MIN(MIN(MIN(draw->coords->vertex[0], draw->coords->vertex[2]), draw->coords->vertex[4]), draw->coords->vertex[6]);
      v->pos.y = 1.0 - MAX(MAX(MAX(draw->coords->vertex[1], draw->coords->vertex[3]), draw->coords->vertex[5]), draw->coords->vertex[7]);
      v->pos.width  = MAX(MAX(MAX(draw->coords->vertex[0], draw->coords->vertex[2]), draw->coords->vertex[4]), draw->coords->vertex[6]) - v->pos.x;
      v->pos.height = 1.0 - MIN(MIN(MIN(draw->coords->vertex[1], draw->coords->vertex[3]), draw->coords->vertex[5]), draw->coords->vertex[7]) - v->pos.y;
      v->pos.x *= wiiu->color_buffer.surface.width;
      v->pos.y *= wiiu->color_buffer.surface.height;
      v->pos.width *= wiiu->color_buffer.surface.width;
      v->pos.height *= wiiu->color_buffer.surface.height;
   }
   else
   {
      v->pos.x = draw->x;
      v->pos.y = wiiu->color_buffer.surface.height - draw->y - draw->height;
      v->pos.width = draw->width;
      v->pos.height = draw->height;
   }
   if(draw->coords->tex_coord && draw->coords->vertices == 4)
   {
      v->coord.u = MIN(MIN(MIN(draw->coords->tex_coord[0], draw->coords->tex_coord[2]), draw->coords->tex_coord[4]), draw->coords->tex_coord[6]);
      v->coord.v = MIN(MIN(MIN(draw->coords->tex_coord[1], draw->coords->tex_coord[3]), draw->coords->tex_coord[5]), draw->coords->tex_coord[7]);
      v->coord.width  = MAX(MAX(MAX(draw->coords->tex_coord[0], draw->coords->tex_coord[2]), draw->coords->tex_coord[4]), draw->coords->tex_coord[6]) - v->coord.u;
      v->coord.height = MAX(MAX(MAX(draw->coords->tex_coord[1], draw->coords->tex_coord[3]), draw->coords->tex_coord[5]), draw->coords->tex_coord[7]) - v->coord.v;
   }
   else
   {
      v->coord.u = 0.0f;
      v->coord.v = 0.0f;
      v->coord.width = 1.0f;
      v->coord.height = 1.0f;
   }


   v->color = COLOR_RGBA(0xFF * draw->coords->color[0], 0xFF * draw->coords->color[1],
                       0xFF * draw->coords->color[2], 0xFF * draw->coords->color[3]);

   GX2SetPixelTexture(texture, sprite_shader.ps.samplerVars[0].location);

   GX2DrawEx(GX2_PRIMITIVE_MODE_POINTS, 1, wiiu->vertex_cache.current, 1);

#if 0
   printf("(%i,%i,%i,%i) , (%i,%i)\n", (int)draw->x,
         (int)draw->y, (int)draw->width, (int)draw->height,
         texture->surface.width, texture->surface.height);
#endif

   wiiu->vertex_cache.current ++;

}

static void menu_display_wiiu_draw_pipeline(void *data)
{
}

static void menu_display_wiiu_restore_clear_color(void)
{
#if 0
   wiiu_set_clear_color(RGBA8(0x00, 0x00, 0x00, 0xFF));
#endif
}

static void menu_display_wiiu_clear_color(menu_display_ctx_clearcolor_t *clearcolor)
{
   if (!clearcolor)
      return;
#if 0
   wiiu_set_clear_color(RGBA8((int)(clearcolor->r*255.f),
            (int)(clearcolor->g*255.f),
            (int)(clearcolor->b*255.f),
            (int)(clearcolor->a*255.f)));
   wiiu_clear_screen();
#endif
}

static bool menu_display_wiiu_font_init_first(
      void **font_handle, void *video_data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   font_data_t **handle = (font_data_t**)font_handle;
   *handle = font_driver_init_first(video_data,
         font_path, font_size, true,
         is_threaded,
         FONT_DRIVER_RENDER_WIIU);
   return *handle;
}

menu_display_ctx_driver_t menu_display_ctx_wiiu = {
   menu_display_wiiu_draw,
   menu_display_wiiu_draw_pipeline,
   menu_display_wiiu_viewport,
   menu_display_wiiu_blend_begin,
   menu_display_wiiu_blend_end,
   menu_display_wiiu_restore_clear_color,
   menu_display_wiiu_clear_color,
   menu_display_wiiu_get_default_mvp,
   menu_display_wiiu_get_default_vertices,
   menu_display_wiiu_get_default_tex_coords,
   menu_display_wiiu_font_init_first,
   MENU_VIDEO_DRIVER_WIIU,
   "menu_display_wiiu",
};

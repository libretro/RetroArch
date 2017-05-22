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

   position_t* pos = wiiu->vertex_cache.positions + wiiu->vertex_cache.current;
   tex_coord_t* coord = wiiu->vertex_cache.tex_coords + wiiu->vertex_cache.current;

   float x0 = draw->x;
   float y0 = draw->y;
   float x1 = x0 + draw->width;
   float y1 = y0 + draw->height;

   pos[0].x = (2.0f * x0 / wiiu->color_buffer.surface.width) - 1.0f;
   pos[0].y = (2.0f * y0 / wiiu->color_buffer.surface.height) - 1.0f;
   pos[1].x = (2.0f * x1 / wiiu->color_buffer.surface.width) - 1.0f;;
   pos[1].y = (2.0f * y0 / wiiu->color_buffer.surface.height) - 1.0f;
   pos[2].x = (2.0f * x1 / wiiu->color_buffer.surface.width) - 1.0f;;
   pos[2].y = (2.0f * y1 / wiiu->color_buffer.surface.height) - 1.0f;
   pos[3].x = (2.0f * x0 / wiiu->color_buffer.surface.width) - 1.0f;;
   pos[3].y = (2.0f * y1 / wiiu->color_buffer.surface.height) - 1.0f;

   coord[0].u = 0.0f;
   coord[0].v = 1.0f;
   coord[1].u = 1.0f;
   coord[1].v = 1.0f;
   coord[2].u = 1.0f;
   coord[2].v = 0.0f;
   coord[3].u = 0.0f;
   coord[3].v = 0.0f;

   GX2SetPixelTexture(texture, wiiu->shader->sampler.location);

//   GX2SetBlendConstantColor(draw->coords->color[3], draw->coords->color[2], draw->coords->color[1], draw->coords->color[0]);
//   GX2SetBlendControl(GX2_RENDER_TARGET_0, GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA, GX2_BLEND_COMBINE_MODE_ADD,
//                      GX2_ENABLE,          GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA, GX2_BLEND_COMBINE_MODE_ADD);

   GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, wiiu->vertex_cache.current, 1);

#if 0
   printf("(%i,%i,%i,%i) , (%i,%i)\n", (int)draw->x,
         (int)draw->y, (int)draw->width, (int)draw->height,
         texture->surface.width, texture->surface.height);
#endif

   wiiu->vertex_cache.current += 4;

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

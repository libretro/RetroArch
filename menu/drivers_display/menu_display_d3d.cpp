/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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
#include "../../gfx/font_renderer_driver.h"
#include "../../gfx/video_context_driver.h"
#include "../../gfx/video_thread_wrapper.h"
#include "../../gfx/video_texture.h"
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

static d3d_video_t *d3d_get_ptr(void)
{
   d3d_video_t *d3d = (d3d_video_t*)video_driver_get_ptr(false);

   if (!d3d)
      return NULL;
   return d3d;
}

static void *menu_display_d3d_get_default_mvp(void)
{
   d3d_video_t *d3d = d3d_get_ptr();

   if (!d3d)
      return NULL;
   return NULL; /* TODO/FIXME */
}

static unsigned menu_display_prim_to_d3d_enum(enum menu_display_prim_type prim_type)
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

#if 0
   if (gl->shader && gl->shader->use)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);
#endif
}

static void menu_display_d3d_blend_end(void)
{
   d3d_video_t *d3d = d3d_get_ptr();

   if (!d3d)
      return;

   d3d_disable_blend_func(d3d->dev);
}

static void menu_display_d3d_draw(
      unsigned x, unsigned y,
      unsigned width, unsigned height,
      struct gfx_coords *coords,
      void *matrix_data,
      uintptr_t texture,
      enum menu_display_prim_type prim_type
      )
{
   D3DVIEWPORT vp       = {0};
   d3d_video_t     *d3d = d3d_get_ptr();
   math_matrix_4x4 *mat = (math_matrix_4x4*)matrix_data;

   if (!d3d)
      return;

   /* TODO - edge case */
   if (height <= 0)
      height = 1;

   if (!mat)
      mat = (math_matrix_4x4*)menu_display_d3d_get_default_mvp();
   if (!coords->vertex)
      coords->vertex = &d3d_vertexes[0];
   if (!coords->tex_coord)
      coords->tex_coord = &d3d_tex_coords[0];
   if (!coords->lut_tex_coord)
      coords->lut_tex_coord = &d3d_tex_coords[0];

   vp.X      = x;
   vp.Y      = y;
   vp.Width  = width;
   vp.Height = height;
   vp.MinZ   = 0.0f;
   vp.MaxZ   = 1.0f;

   d3d_set_viewport(d3d->dev, &vp);
   d3d_set_texture(d3d->dev, 0, (LPDIRECT3DTEXTURE)texture);

#if 0
   gl->shader->set_coords(coords);
   gl->shader->set_mvp(driver->video_data, mat);
#endif

   d3d_draw_primitive(d3d->dev, (D3DPRIMITIVETYPE)menu_display_prim_to_d3d_enum(prim_type), 0, coords->vertices);

#if 0
   gl->coords.color     = gl->white_color_ptr;
#endif
}

static void menu_display_d3d_draw_bg(
      unsigned width,
      unsigned height,
      uintptr_t texture,
      float handle_alpha,
      bool force_transparency,
      float *coord_color,
      float *coord_color2,
      const float *vertex,
      const float *tex_coord,
      size_t vertex_count,
      enum menu_display_prim_type prim_type)
{
   struct gfx_coords coords;
   const float *new_vertex    = NULL;
   const float *new_tex_coord = NULL;
   global_t     *global = global_get_ptr();
   settings_t *settings = config_get_ptr();
   d3d_video_t     *d3d = d3d_get_ptr();

   if (!d3d)
      return;

   new_vertex    = vertex;
   new_tex_coord = tex_coord;

   if (!new_vertex)
      new_vertex = &d3d_vertexes[0];
   if (!new_tex_coord)
      new_tex_coord = &d3d_tex_coords[0];

   coords.vertices      = vertex_count;
   coords.vertex        = new_vertex;
   coords.tex_coord     = new_tex_coord;
   coords.lut_tex_coord = new_tex_coord;
   coords.color         = (const float*)coord_color;

   menu_display_d3d_blend_begin();

   menu_display_ctl(MENU_DISPLAY_CTL_SET_VIEWPORT, NULL);

   if ((settings->menu.pause_libretro
      || !global->inited.main || (global->inited.core.type == CORE_TYPE_DUMMY))
      && !force_transparency
      && texture)
      coords.color = (const float*)coord_color2;

   menu_display_d3d_draw(0, 0, width, height,
         &coords, (math_matrix_4x4*)menu_display_d3d_get_default_mvp(),
         (uintptr_t)texture, prim_type);

   menu_display_d3d_blend_end();

#if 0
   gl->coords.color = gl->white_color_ptr;
#endif
}

static void menu_display_d3d_restore_clear_color(void)
{
   d3d_video_t     *d3d = d3d_get_ptr();
   DWORD    clear_color = 0x00000000;

   d3d_clear(d3d->dev, 0, NULL, D3DCLEAR_TARGET, clear_color, 0, 0);
}

static void menu_display_d3d_clear_color(float r, float g, float b, float a)
{
   d3d_video_t     *d3d = d3d_get_ptr();
   DWORD    clear_color = D3DCOLOR_ARGB(BYTE_CLAMP(a * 255.0f), BYTE_CLAMP(r * 255.0f), BYTE_CLAMP(g * 255.0f), BYTE_CLAMP(b * 255.0f));

   d3d_clear(d3d->dev, 0, NULL, D3DCLEAR_TARGET, clear_color, 0, 0);
}

static unsigned menu_display_d3d_texture_load(void *data, enum texture_filter_type type)
{
   return video_texture_load(data, TEXTURE_BACKEND_DIRECT3D, type);
}

static void menu_display_d3d_texture_unload(uintptr_t *id)
{
   if (!id)
      return;
   video_texture_unload(TEXTURE_BACKEND_DIRECT3D, id);
}

static const float *menu_display_d3d_get_tex_coords(void)
{
   return &d3d_tex_coords[0];
}

static bool menu_display_d3d_font_init_first(const void **font_driver,
      void **font_handle, void *video_data, const char *font_path,
      float font_size)
{
   settings_t *settings = config_get_ptr();
   const struct retro_hw_render_callback *hw_render =
      (const struct retro_hw_render_callback*)video_driver_callback();

   if (settings->video.threaded && !hw_render->context_type)
   {
      thread_packet_t pkt;
      driver_t *driver    = driver_get_ptr();
      thread_video_t *thr = (thread_video_t*)driver->video_data;

      if (!thr)
         return false;

      pkt.type                       = CMD_FONT_INIT;
      pkt.data.font_init.method      = font_init_first;
      pkt.data.font_init.font_driver = (const void**)font_driver;
      pkt.data.font_init.font_handle = font_handle;
      pkt.data.font_init.video_data  = video_data;
      pkt.data.font_init.font_path   = font_path;
      pkt.data.font_init.font_size   = font_size;
      pkt.data.font_init.api         = FONT_DRIVER_RENDER_DIRECT3D_API;

      thr->send_and_wait(thr, &pkt);

      return pkt.data.font_init.return_value;
   }

   return font_init_first(font_driver, font_handle, video_data,
         font_path, font_size, FONT_DRIVER_RENDER_DIRECT3D_API);
}

menu_display_ctx_driver_t menu_display_ctx_d3d = {
   menu_display_d3d_draw,
   menu_display_d3d_draw_bg,
   menu_display_d3d_blend_begin,
   menu_display_d3d_blend_end,
   menu_display_d3d_restore_clear_color,
   menu_display_d3d_clear_color,
   menu_display_d3d_get_default_mvp,
   menu_display_d3d_get_tex_coords,
   menu_display_d3d_texture_load,
   menu_display_d3d_texture_unload,
   menu_display_d3d_font_init_first,
   MENU_VIDEO_DRIVER_DIRECT3D,
   "menu_display_d3d",
};

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

#include "../../config.def.h"
#include "../../gfx/font_renderer_driver.h"
#include "../../gfx/video_context_driver.h"
#include "../../gfx/video_texture.h"
#include "../../gfx/common/gl_common.h"

#include "../menu_display.h"

static const GLfloat gl_vertexes[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const GLfloat gl_tex_coords[] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static gl_t *gl_get_ptr(void)
{
   gl_t *gl = (gl_t*)video_driver_get_ptr(false);
   if (!gl)
      return NULL;
   return gl;
}

static void *menu_display_gl_get_default_mvp(void)
{
   gl_t *gl = gl_get_ptr();

   if (!gl)
      return NULL;

   return &gl->mvp_no_rot;
}

static GLenum menu_display_prim_to_gl_enum(enum menu_display_prim_type prim_type)
{
   switch (prim_type)
   {
      case MENU_DISPLAY_PRIM_TRIANGLESTRIP:
         return GL_TRIANGLE_STRIP;
      case MENU_DISPLAY_PRIM_TRIANGLES:
         return GL_TRIANGLES;
      case MENU_DISPLAY_PRIM_NONE:
      default:
         break;
   }

   return 0;
}

static void menu_display_gl_blend_begin(void)
{
   gl_t *gl = gl_get_ptr();

   if (!gl)
      return;

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   video_shader_driver_use(gl->shader, NULL, GL_SHADER_STOCK_BLEND);
}

static void menu_display_gl_blend_end(void)
{
   glDisable(GL_BLEND);
}

static void menu_display_gl_draw(
      float x, float y,
      unsigned width, unsigned height,
      struct gfx_coords *coords,
      void *matrix_data,
      uintptr_t texture,
      enum menu_display_prim_type prim_type
      )
{
   gl_t             *gl = gl_get_ptr();
   math_matrix_4x4 *mat = (math_matrix_4x4*)matrix_data;

   if (!gl)
      return;

   /* TODO - edge case */
   if (height <= 0)
      height = 1;

   if (!mat)
      mat = (math_matrix_4x4*)menu_display_gl_get_default_mvp();
   if (!coords->vertex)
      coords->vertex = &gl_vertexes[0];
   if (!coords->tex_coord)
      coords->tex_coord = &gl_tex_coords[0];
   if (!coords->lut_tex_coord)
      coords->lut_tex_coord = &gl_tex_coords[0];

   glViewport(x, y, width, height);
   glBindTexture(GL_TEXTURE_2D, (GLuint)texture);

   video_shader_driver_set_coords(gl->shader, gl, coords);
   video_shader_driver_set_mvp(gl->shader, video_driver_get_ptr(false), mat);

   glDrawArrays(menu_display_prim_to_gl_enum(prim_type), 0, coords->vertices);

   gl->coords.color     = gl->white_color_ptr;
}

static void menu_display_gl_draw_bg(
      unsigned width,
      unsigned height,
      uintptr_t texture,
      float handle_alpha,
      bool force_transparency,
      GLfloat *coord_color,
      GLfloat *coord_color2,
      const float *vertex,
      const float *tex_coord,
      size_t vertex_count,
      enum menu_display_prim_type prim_type)
{
   struct gfx_coords coords;
   const GLfloat *new_vertex    = NULL;
   const GLfloat *new_tex_coord = NULL;
   global_t     *global = global_get_ptr();
   settings_t *settings = config_get_ptr();
   gl_t             *gl = gl_get_ptr();

   if (!gl)
      return;

   new_vertex    = vertex;
   new_tex_coord = tex_coord;

   if (!new_vertex)
      new_vertex = &gl_vertexes[0];
   if (!new_tex_coord)
      new_tex_coord = &gl_tex_coords[0];

   coords.vertices      = vertex_count;
   coords.vertex        = new_vertex;
   coords.tex_coord     = new_tex_coord;
   coords.lut_tex_coord = new_tex_coord;
   coords.color         = (const float*)coord_color;

   menu_display_gl_blend_begin();

   menu_display_ctl(MENU_DISPLAY_CTL_SET_VIEWPORT, NULL);

   if ((settings->menu.pause_libretro
      || !global->inited.main || (global->inited.core.type == CORE_TYPE_DUMMY))
      && !force_transparency
      && texture)
      coords.color = (const float*)coord_color2;

   menu_display_gl_draw(0, 0, width, height,
         &coords, (math_matrix_4x4*)menu_display_gl_get_default_mvp(),
         (GLuint)texture, prim_type);

   menu_display_gl_blend_end();

   gl->coords.color = gl->white_color_ptr;
}

static void menu_display_gl_restore_clear_color(void)
{
   glClearColor(0.0f, 0.0f, 0.0f, 0.00f);
}

static void menu_display_gl_clear_color(float r, float g, float b, float a)
{
   glClearColor(r, g, b, a);
   glClear(GL_COLOR_BUFFER_BIT);
}

static unsigned menu_display_gl_texture_load(void *data, enum texture_filter_type type)
{
   return video_texture_load(data, TEXTURE_BACKEND_OPENGL, type);
}

static void menu_display_gl_texture_unload(uintptr_t *id)
{
   if (!id)
      return;
   video_texture_unload(TEXTURE_BACKEND_OPENGL, id);
}

static const float *menu_display_gl_get_tex_coords(void)
{
   return &gl_tex_coords[0];
}

static bool menu_display_gl_font_init_first(
      void **font_handle, void *video_data, const char *font_path,
      float font_size)
{
   return font_driver_init_first(NULL, font_handle, video_data,
         font_path, font_size, true, FONT_DRIVER_RENDER_OPENGL_API);
}

menu_display_ctx_driver_t menu_display_ctx_gl = {
   menu_display_gl_draw,
   menu_display_gl_draw_bg,
   menu_display_gl_blend_begin,
   menu_display_gl_blend_end,
   menu_display_gl_restore_clear_color,
   menu_display_gl_clear_color,
   menu_display_gl_get_default_mvp,
   menu_display_gl_get_tex_coords,
   menu_display_gl_texture_load,
   menu_display_gl_texture_unload,
   menu_display_gl_font_init_first,
   MENU_VIDEO_DRIVER_OPENGL,
   "menu_display_gl",
};

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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
#include "../../retroarch.h"
#include "../../gfx/font_driver.h"
#include "../../gfx/video_context_driver.h"
#include "../../gfx/video_shader_driver.h"
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

static const float *menu_display_gl_get_default_vertices(void)
{
   return &gl_vertexes[0];
}

static const float *menu_display_gl_get_default_tex_coords(void)
{
   return &gl_tex_coords[0];
}

static void *menu_display_gl_get_default_mvp(void)
{
   gl_t *gl = gl_get_ptr();

   if (!gl)
      return NULL;

   return &gl->mvp_no_rot;
}

static GLenum menu_display_prim_to_gl_enum(
      enum menu_display_prim_type type)
{
   switch (type)
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
   video_shader_ctx_info_t shader_info;

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   shader_info.data = NULL;
   shader_info.idx  = GL_SHADER_STOCK_BLEND;

   video_shader_driver_ctl(SHADER_CTL_USE, &shader_info);
}

static void menu_display_gl_blend_end(void)
{
   glDisable(GL_BLEND);
}

static void menu_display_gl_draw(void *data)
{
   video_shader_ctx_mvp_t mvp;
   video_shader_ctx_coords_t coords;
   gl_t             *gl          = gl_get_ptr();
   math_matrix_4x4 *mat          = NULL;
   menu_display_ctx_draw_t *draw = (menu_display_ctx_draw_t*)data;
   
   if (!gl || !draw)
      return;

   mat = (math_matrix_4x4*)draw->matrix_data;

   /* TODO - edge case */
   if (draw->height <= 0)
      draw->height = 1;

   if (!mat)
      mat = (math_matrix_4x4*)menu_display_gl_get_default_mvp();
   if (!draw->coords->vertex)
      draw->coords->vertex = menu_display_gl_get_default_vertices();
   if (!draw->coords->tex_coord)
      draw->coords->tex_coord = menu_display_gl_get_default_tex_coords();
   if (!draw->coords->lut_tex_coord)
      draw->coords->lut_tex_coord = menu_display_gl_get_default_tex_coords();

   glViewport(draw->x, draw->y, draw->width, draw->height);
   glBindTexture(GL_TEXTURE_2D, (GLuint)draw->texture);

   coords.handle_data = gl;
   coords.data        = draw->coords;
   
   video_shader_driver_ctl(SHADER_CTL_SET_COORDS, &coords);

   mvp.data   = gl;
   mvp.matrix = mat;

   video_shader_driver_ctl(SHADER_CTL_SET_MVP, &mvp);

   glDrawArrays(menu_display_prim_to_gl_enum(
            draw->prim_type), 0, draw->coords->vertices);

   gl->coords.color     = gl->white_color_ptr;
}

static void menu_display_gl_draw_bg(void *data)
{
   struct gfx_coords coords;
   const float *new_vertex     = NULL;
   const float *new_tex_coord  = NULL;
   menu_display_ctx_draw_t *draw = (menu_display_ctx_draw_t*)data;

   if (!draw)
      return;

   new_vertex           = draw->vertex;
   new_tex_coord        = draw->tex_coord;

   if (!new_vertex)
      new_vertex        = menu_display_gl_get_default_vertices();
   if (!new_tex_coord)
      new_tex_coord     = menu_display_gl_get_default_tex_coords();

   coords.vertices      = draw->vertex_count;
   coords.vertex        = new_vertex;
   coords.tex_coord     = new_tex_coord;
   coords.lut_tex_coord = new_tex_coord;
   coords.color         = (const float*)draw->color;

   draw->x              = 0;
   draw->y              = 0;
   draw->coords         = &coords;

   if (!draw->texture)
      draw->texture     = menu_display_white_texture;

   draw->matrix_data = (math_matrix_4x4*)
      menu_display_gl_get_default_mvp();

   menu_display_gl_draw(draw);
}

static void menu_display_gl_restore_clear_color(void)
{
   glClearColor(0.0f, 0.0f, 0.0f, 0.00f);
}

static void menu_display_gl_clear_color(void *data)
{
   menu_display_ctx_clearcolor_t *clearcolor = 
      (menu_display_ctx_clearcolor_t*)data;
   if (!clearcolor)
      return;

   glClearColor(clearcolor->r,
         clearcolor->g, clearcolor->b, clearcolor->a);
   glClear(GL_COLOR_BUFFER_BIT);
}

static const float *menu_display_gl_get_tex_coords(void)
{
   return &gl_tex_coords[0];
}

static bool menu_display_gl_font_init_first(
      void **font_handle, void *video_data,
      const char *font_path, float font_size)
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
   menu_display_gl_font_init_first,
   MENU_VIDEO_DRIVER_OPENGL,
   "menu_display_gl",
};

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
   gl_t *gl = (gl_t*)video_driver_get_ptr(false);

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
   shader_info.idx  = VIDEO_SHADER_STOCK_BLEND;
   shader_info.set_active = true;

   video_shader_driver_use(&shader_info);
}

static void menu_display_gl_blend_end(void)
{
   glDisable(GL_BLEND);
}

static void menu_display_gl_viewport(void *data)
{
   gl_t             *gl          = (gl_t*)video_driver_get_ptr(false);
   menu_display_ctx_draw_t *draw = (menu_display_ctx_draw_t*)data;
   
   if (!gl || !draw)
      return;
   glViewport(draw->x, draw->y, draw->width, draw->height);
}

static void menu_display_gl_bind_texture(void *data)
{
   menu_display_ctx_draw_t *draw = (menu_display_ctx_draw_t*)data;
   
   if (!draw)
      return;

   glBindTexture(GL_TEXTURE_2D, (GLuint)draw->texture);
}

static void menu_display_gl_draw(void *data)
{
   video_shader_ctx_mvp_t mvp;
   video_shader_ctx_coords_t coords;
   gl_t             *gl          = (gl_t*)video_driver_get_ptr(false);
   menu_display_ctx_draw_t *draw = (menu_display_ctx_draw_t*)data;
   
   if (!gl || !draw)
      return;

   if (!draw->coords->vertex)
      draw->coords->vertex = menu_display_gl_get_default_vertices();
   if (!draw->coords->tex_coord)
      draw->coords->tex_coord = menu_display_gl_get_default_tex_coords();
   if (!draw->coords->lut_tex_coord)
      draw->coords->lut_tex_coord = menu_display_gl_get_default_tex_coords();

   menu_display_gl_viewport(draw);
   menu_display_gl_bind_texture(draw);

   coords.handle_data = gl;
   coords.data        = draw->coords;
   
   video_shader_driver_set_coords(&coords);

   mvp.data   = gl;
   mvp.matrix = draw->matrix_data ? (math_matrix_4x4*)draw->matrix_data 
      : (math_matrix_4x4*)menu_display_gl_get_default_mvp();

   video_shader_driver_set_mvp(&mvp);

   glDrawArrays(menu_display_prim_to_gl_enum(
            draw->prim_type), 0, draw->coords->vertices);

   gl->coords.color     = gl->white_color_ptr;
}

static void menu_display_gl_draw_pipeline(void *data)
{
#if defined(HAVE_GLSL) || defined(HAVE_CG)
   video_shader_ctx_info_t shader_info;
   menu_display_ctx_draw_t *draw     = (menu_display_ctx_draw_t*)data;
   struct uniform_info uniform_param = {0};
   static float t                    = 0;
   video_coord_array_t *ca             = NULL;

   ca = menu_display_get_coords_array();

   draw->x           = 0;
   draw->y           = 0;
   draw->coords      = (struct video_coords*)(&ca->coords);
   draw->matrix_data = NULL;

   switch (draw->pipeline.id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_SEC:
         shader_info.data       = NULL;
         shader_info.idx        = draw->pipeline.id;
         shader_info.set_active = true;

         video_shader_driver_use(&shader_info);

         t += 0.01;

         uniform_param.enabled           = true;
         uniform_param.lookup.enable     = true;
         uniform_param.lookup.add_prefix = true;
         uniform_param.lookup.idx        = draw->pipeline.id;
         uniform_param.lookup.type       = SHADER_PROGRAM_VERTEX;
         uniform_param.type              = UNIFORM_1F;
         uniform_param.lookup.ident      = "time";
         uniform_param.result.f.v0       = t;

         video_shader_driver_set_parameter(&uniform_param);
         break;
   }
#endif
}

static void menu_display_gl_restore_clear_color(void)
{
   glClearColor(0.0f, 0.0f, 0.0f, 0.00f);
}

static void menu_display_gl_clear_color(menu_display_ctx_clearcolor_t *clearcolor)
{
   if (!clearcolor)
      return;

   glClearColor(clearcolor->r,
         clearcolor->g, clearcolor->b, clearcolor->a);
   glClear(GL_COLOR_BUFFER_BIT);
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
   menu_display_gl_draw_pipeline,
   menu_display_gl_viewport,
   menu_display_gl_blend_begin,
   menu_display_gl_blend_end,
   menu_display_gl_restore_clear_color,
   menu_display_gl_clear_color,
   menu_display_gl_get_default_mvp,
   menu_display_gl_get_default_vertices,
   menu_display_gl_get_default_tex_coords,
   menu_display_gl_font_init_first,
   MENU_VIDEO_DRIVER_OPENGL,
   "menu_display_gl",
};

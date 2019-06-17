/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016-2017 - Hans-Kristian Arntzen
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
#include <gfx/common/gl_core_common.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../menu_driver.h"

#include "../../gfx/font_driver.h"
#include "../../retroarch.h"
#include "../../gfx/common/gl_core_common.h"

static const float gl_core_vertexes[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const float gl_core_tex_coords[] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static const float gl_core_colors[] = {
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
};

static void *menu_display_gl_core_get_default_mvp(video_frame_info_t *video_info)
{
   gl_core_t *gl_core = (gl_core_t*)video_info->userdata;
   if (!gl_core)
      return NULL;
   return &gl_core->mvp_no_rot;
}

static const float *menu_display_gl_core_get_default_vertices(void)
{
   return &gl_core_vertexes[0];
}

static const float *menu_display_gl_core_get_default_color(void)
{
   return &gl_core_colors[0];
}

static const float *menu_display_gl_core_get_default_tex_coords(void)
{
   return &gl_core_tex_coords[0];
}

static void menu_display_gl_core_viewport(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
   if (draw)
      glViewport(draw->x, draw->y, draw->width, draw->height);
}

static void menu_display_gl_core_draw_pipeline(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
#ifdef HAVE_SHADERPIPELINE
   float output_size[2];
   static struct video_coords blank_coords;
   static uint8_t ubo_scratch_data[768];
   static float t                = 0.0f;
   float yflip                   = 0.0f;
   video_coord_array_t *ca       = NULL;
   gl_core_t *gl_core            = (gl_core_t*)video_info->userdata;

   if (!gl_core || !draw)
      return;

   draw->x                       = 0;
   draw->y                       = 0;
   draw->matrix_data             = NULL;

   output_size[0]                = (float)video_info->width;
   output_size[1]                = (float)video_info->height;

   switch (draw->pipeline.id)
   {
      /* Ribbon */
      default:
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
         ca = menu_display_get_coords_array();
         draw->coords                     = (struct video_coords*)&ca->coords;
         draw->pipeline.backend_data      = ubo_scratch_data;
         draw->pipeline.backend_data_size = 2 * sizeof(float);

         /* Match UBO layout in shader. */
         yflip = -1.0f;
         memcpy(ubo_scratch_data, &t, sizeof(t));
         memcpy(ubo_scratch_data + sizeof(float), &yflip, sizeof(yflip));
         break;

      /* Snow simple */
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
         draw->pipeline.backend_data      = ubo_scratch_data;
         draw->pipeline.backend_data_size = sizeof(math_matrix_4x4) 
            + 4 * sizeof(float);

         /* Match UBO layout in shader. */
         memcpy(ubo_scratch_data,
               menu_display_gl_core_get_default_mvp(video_info),
               sizeof(math_matrix_4x4));
         memcpy(ubo_scratch_data + sizeof(math_matrix_4x4),
               output_size,
               sizeof(output_size));

         if (draw->pipeline.id == VIDEO_SHADER_MENU_5)
            yflip = 1.0f;

         memcpy(ubo_scratch_data + sizeof(math_matrix_4x4) 
               + 2 * sizeof(float), &t, sizeof(t));
         memcpy(ubo_scratch_data + sizeof(math_matrix_4x4) 
               + 3 * sizeof(float), &yflip, sizeof(yflip));
         draw->coords = &blank_coords;
         blank_coords.vertices = 4;
         draw->prim_type = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
         break;
   }

   t += 0.01;
#endif
}

static void menu_display_gl_core_draw(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
   const float *vertex       = NULL;
   const float *tex_coord    = NULL;
   const float *color        = NULL;
   GLuint            texture = 0;
   gl_core_t *gl             = (gl_core_t*)video_info->userdata;
   const struct gl_core_buffer_locations *loc = NULL;

   if (!gl || !draw)
      return;

   texture            = (GLuint)draw->texture;
   vertex             = draw->coords->vertex;
   tex_coord          = draw->coords->tex_coord;
   color              = draw->coords->color;

   if (!vertex)
      vertex          = menu_display_gl_core_get_default_vertices();
   if (!tex_coord)
      tex_coord       = menu_display_gl_core_get_default_tex_coords();
   if (!color)
      color           = menu_display_gl_core_get_default_color();

   menu_display_gl_core_viewport(draw, video_info);

   glActiveTexture(GL_TEXTURE1);
   glBindTexture(GL_TEXTURE_2D, texture);

   switch (draw->pipeline.id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
         glBlendFunc(GL_ONE, GL_ONE);
         break;
      default:
         glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
         break;
   }

   switch (draw->pipeline.id)
   {
#ifdef HAVE_SHADERPIPELINE
      case VIDEO_SHADER_MENU:
         glUseProgram(gl->pipelines.ribbon);
         loc = &gl->pipelines.ribbon_loc;
         break;

      case VIDEO_SHADER_MENU_2:
         glUseProgram(gl->pipelines.ribbon_simple);
         loc = &gl->pipelines.ribbon_simple_loc;
         break;

      case VIDEO_SHADER_MENU_3:
         glUseProgram(gl->pipelines.snow_simple);
         loc = &gl->pipelines.snow_simple_loc;
         break;

      case VIDEO_SHADER_MENU_4:
         glUseProgram(gl->pipelines.snow);
         loc = &gl->pipelines.snow_loc;
         break;

      case VIDEO_SHADER_MENU_5:
         glUseProgram(gl->pipelines.bokeh);
         loc = &gl->pipelines.bokeh_loc;
         break;
#endif

      default:
         glUseProgram(gl->pipelines.alpha_blend);
         loc = NULL;
         break;
   }

   if (loc && loc->flat_ubo_vertex >= 0)
      glUniform4fv(loc->flat_ubo_vertex,
                   (GLsizei)((draw->pipeline.backend_data_size + 15) / 16),
                   (const GLfloat*)draw->pipeline.backend_data);

   if (loc && loc->flat_ubo_fragment >= 0)
      glUniform4fv(loc->flat_ubo_fragment,
                   (GLsizei)((draw->pipeline.backend_data_size + 15) / 16),
                   (const GLfloat*)draw->pipeline.backend_data);

   if (!loc)
   {
      const math_matrix_4x4 *mat = draw->matrix_data
                     ? (const math_matrix_4x4*)draw->matrix_data : (const math_matrix_4x4*)menu_display_gl_core_get_default_mvp(video_info);
      if (gl->pipelines.alpha_blend_loc.flat_ubo_vertex >= 0)
         glUniform4fv(gl->pipelines.alpha_blend_loc.flat_ubo_vertex,
                      4, mat->data);
   }

   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray(1);
   glEnableVertexAttribArray(2);

   gl_core_bind_scratch_vbo(gl, vertex,
         2 * sizeof(float) * draw->coords->vertices);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
         2 * sizeof(float), (void *)(uintptr_t)0);
   gl_core_bind_scratch_vbo(gl, tex_coord,
         2 * sizeof(float) * draw->coords->vertices);
   glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
         2 * sizeof(float), (void *)(uintptr_t)0);
   gl_core_bind_scratch_vbo(gl, color,
         4 * sizeof(float) * draw->coords->vertices);
   glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE,
         4 * sizeof(float), (void *)(uintptr_t)0);

   if (draw->prim_type == MENU_DISPLAY_PRIM_TRIANGLESTRIP)
      glDrawArrays(GL_TRIANGLE_STRIP, 0, draw->coords->vertices);
   else if (draw->prim_type == MENU_DISPLAY_PRIM_TRIANGLES)
      glDrawArrays(GL_TRIANGLES, 0, draw->coords->vertices);

   glDisableVertexAttribArray(0);
   glDisableVertexAttribArray(1);
   glDisableVertexAttribArray(2);
   glBindBuffer(GL_ARRAY_BUFFER, 0);

   glBindTexture(GL_TEXTURE_2D, 0);
}

static void menu_display_gl_core_restore_clear_color(void)
{
   glClearColor(0.0f, 0.0f, 0.0f, 0.00f);
}

static void menu_display_gl_core_clear_color(
      menu_display_ctx_clearcolor_t *clearcolor,
      video_frame_info_t *video_info)
{
   if (!clearcolor)
      return;

   glClearColor(clearcolor->r, clearcolor->g, clearcolor->b, clearcolor->a);
   glClear(GL_COLOR_BUFFER_BIT);
}

static void menu_display_gl_core_blend_begin(video_frame_info_t *video_info)
{
   gl_core_t *gl = (gl_core_t*)video_info->userdata;

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glUseProgram(gl->pipelines.alpha_blend);
}

static void menu_display_gl_core_blend_end(video_frame_info_t *video_info)
{
   glDisable(GL_BLEND);
}

static bool menu_display_gl_core_font_init_first(
      void **font_handle, void *video_data, const char *font_path,
      float menu_font_size, bool is_threaded)
{
   font_data_t **handle = (font_data_t**)font_handle;
   *handle = font_driver_init_first(video_data,
         font_path, menu_font_size, true,
         is_threaded,
         FONT_DRIVER_RENDER_OPENGL_CORE_API);

   if (*handle)
      return true;

   return false;
}

static void menu_display_gl_core_scissor_begin(video_frame_info_t *video_info,
      int x, int y, unsigned width, unsigned height)
{
   glScissor(x, video_info->height - y - height, width, height);
   glEnable(GL_SCISSOR_TEST);
}

static void menu_display_gl_core_scissor_end(video_frame_info_t *video_info)
{
   glDisable(GL_SCISSOR_TEST);
}

menu_display_ctx_driver_t menu_display_ctx_gl_core = {
   menu_display_gl_core_draw,
   menu_display_gl_core_draw_pipeline,
   menu_display_gl_core_viewport,
   menu_display_gl_core_blend_begin,
   menu_display_gl_core_blend_end,
   menu_display_gl_core_restore_clear_color,
   menu_display_gl_core_clear_color,
   menu_display_gl_core_get_default_mvp,
   menu_display_gl_core_get_default_vertices,
   menu_display_gl_core_get_default_tex_coords,
   menu_display_gl_core_font_init_first,
   MENU_VIDEO_DRIVER_OPENGL_CORE,
   "glcore",
   false,
   menu_display_gl_core_scissor_begin,
   menu_display_gl_core_scissor_end
};

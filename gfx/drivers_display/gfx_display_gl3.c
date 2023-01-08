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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../gfx_display.h"

#include "../common/gl3_common.h"

static const float gl3_vertexes[8] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const float gl3_tex_coords[8] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static const float gl3_colors[16] = {
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
};

static void *gfx_display_gl3_get_default_mvp(void *data)
{
   gl3_t *gl3 = (gl3_t*)data;
   if (!gl3)
      return NULL;
   return &gl3->mvp_no_rot;
}

static const float *gfx_display_gl3_get_default_vertices(void)
{
   return &gl3_vertexes[0];
}

static const float *gfx_display_gl3_get_default_tex_coords(void)
{
   return &gl3_tex_coords[0];
}

static void gfx_display_gl3_draw_pipeline(
      gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp,
      void *data,
      unsigned video_width,
      unsigned video_height)
{
#ifdef HAVE_SHADERPIPELINE
   float output_size[2];
   static struct video_coords blank_coords;
   static uint8_t ubo_scratch_data[768];
   static float t                = 0.0f;
   float yflip                   = 0.0f;
   video_coord_array_t *ca       = NULL;
   gl3_t *gl                 = (gl3_t*)data;

   if (!gl || !draw)
      return;

   draw->x                       = 0;
   draw->y                       = 0;
   draw->matrix_data             = NULL;

   output_size[0]                = (float)video_width;
   output_size[1]                = (float)video_height;

   switch (draw->pipeline_id)
   {
      /* Ribbon */
      default:
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
         ca                               = &p_disp->dispca;
         draw->coords                     = (struct video_coords*)&ca->coords;
         draw->backend_data               = ubo_scratch_data;
         draw->backend_data_size          = 2 * sizeof(float);

         /* Match UBO layout in shader. */
         yflip = -1.0f;
         memcpy(ubo_scratch_data, &t, sizeof(t));
         memcpy(ubo_scratch_data + sizeof(float), &yflip, sizeof(yflip));
         break;

      /* Snow simple */
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
         draw->backend_data               = ubo_scratch_data;
         draw->backend_data_size          = sizeof(math_matrix_4x4) 
            + 4 * sizeof(float);

         /* Match UBO layout in shader. */
         memcpy(ubo_scratch_data,
               &gl->mvp_no_rot,
               sizeof(math_matrix_4x4));
         memcpy(ubo_scratch_data + sizeof(math_matrix_4x4),
               output_size,
               sizeof(output_size));

         if (draw->pipeline_id == VIDEO_SHADER_MENU_5)
            yflip = 1.0f;

         memcpy(ubo_scratch_data + sizeof(math_matrix_4x4) 
               + 2 * sizeof(float), &t, sizeof(t));
         memcpy(ubo_scratch_data + sizeof(math_matrix_4x4) 
               + 3 * sizeof(float), &yflip, sizeof(yflip));
         draw->coords          = &blank_coords;
         blank_coords.vertices = 4;
         draw->prim_type       = GFX_DISPLAY_PRIM_TRIANGLESTRIP;
         break;
   }

   t += 0.01;
#endif
}

static void gfx_display_gl3_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   const float *vertex       = NULL;
   const float *tex_coord    = NULL;
   const float *color        = NULL;
   GLuint            texture = 0;
   gl3_t *gl                 = (gl3_t*)data;
   const struct 
      gl3_buffer_locations 
      *loc                   = NULL;

   if (!gl || !draw)
      return;

   texture            = (GLuint)draw->texture;
   vertex             = draw->coords->vertex;
   tex_coord          = draw->coords->tex_coord;
   color              = draw->coords->color;

   if (!vertex)
      vertex          = gfx_display_gl3_get_default_vertices();
   if (!tex_coord)
      tex_coord       = &gl3_tex_coords[0];
   if (!color)
      color           = &gl3_colors[0];

   glViewport(draw->x, draw->y, draw->width, draw->height);

   glActiveTexture(GL_TEXTURE1);
   glBindTexture(GL_TEXTURE_2D, texture);

   switch (draw->pipeline_id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
         glBlendFunc(GL_ONE, GL_ONE);
         break;
      default:
         glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
         break;
   }

   switch (draw->pipeline_id)
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
         break;
   }

   if (loc)
   {
      if (loc->flat_ubo_vertex >= 0)
         glUniform4fv(loc->flat_ubo_vertex,
               (GLsizei)((draw->backend_data_size + 15) / 16),
               (const GLfloat*)draw->backend_data);

      if (loc->flat_ubo_fragment >= 0)
         glUniform4fv(loc->flat_ubo_fragment,
               (GLsizei)((draw->backend_data_size + 15) / 16),
               (const GLfloat*)draw->backend_data);
   }
   else
   {
      const math_matrix_4x4 *mat = draw->matrix_data
                     ? (const math_matrix_4x4*)draw->matrix_data 
                     : (const math_matrix_4x4*)&gl->mvp_no_rot;
      if (gl->pipelines.alpha_blend_loc.flat_ubo_vertex >= 0)
         glUniform4fv(gl->pipelines.alpha_blend_loc.flat_ubo_vertex,
                      4, mat->data);
   }

   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray(1);
   glEnableVertexAttribArray(2);

   gl3_bind_scratch_vbo(gl, vertex,
         2 * sizeof(float) * draw->coords->vertices);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
         2 * sizeof(float), (void *)(uintptr_t)0);
   gl3_bind_scratch_vbo(gl, tex_coord,
         2 * sizeof(float) * draw->coords->vertices);
   glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
         2 * sizeof(float), (void *)(uintptr_t)0);
   gl3_bind_scratch_vbo(gl, color,
         4 * sizeof(float) * draw->coords->vertices);
   glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE,
         4 * sizeof(float), (void *)(uintptr_t)0);

   switch (draw->prim_type)
   {
      case GFX_DISPLAY_PRIM_TRIANGLESTRIP:
         glDrawArrays(GL_TRIANGLE_STRIP, 0, draw->coords->vertices);
         break;
      case GFX_DISPLAY_PRIM_TRIANGLES:
         glDrawArrays(GL_TRIANGLES, 0, draw->coords->vertices);
         break;
      case GFX_DISPLAY_PRIM_NONE:
      default:
         break;
   }

   glDisableVertexAttribArray(0);
   glDisableVertexAttribArray(1);
   glDisableVertexAttribArray(2);
   glBindBuffer(GL_ARRAY_BUFFER, 0);

   glBindTexture(GL_TEXTURE_2D, 0);
}

static void gfx_display_gl3_blend_begin(void *data)
{
   gl3_t *gl = (gl3_t*)data;

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glUseProgram(gl->pipelines.alpha_blend);
}

static void gfx_display_gl3_blend_end(void *data)
{
   glDisable(GL_BLEND);
}

static void gfx_display_gl3_scissor_begin(void *data,
      unsigned video_width,
      unsigned video_height,
      int x, int y, unsigned width, unsigned height)
{
   glScissor(x, video_height - y - height, width, height);
   glEnable(GL_SCISSOR_TEST);
}

static void gfx_display_gl3_scissor_end(
      void *data,
      unsigned video_width,
      unsigned video_height)
{
   glDisable(GL_SCISSOR_TEST);
}

gfx_display_ctx_driver_t gfx_display_ctx_gl3 = {
   gfx_display_gl3_draw,
   gfx_display_gl3_draw_pipeline,
   gfx_display_gl3_blend_begin,
   gfx_display_gl3_blend_end,
   gfx_display_gl3_get_default_mvp,
   gfx_display_gl3_get_default_vertices,
   gfx_display_gl3_get_default_tex_coords,
   FONT_DRIVER_RENDER_OPENGL_CORE_API,
   GFX_VIDEO_DRIVER_OPENGL_CORE,
   "glcore",
   false,
   gfx_display_gl3_scissor_begin,
   gfx_display_gl3_scissor_end
};

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include "../../retroarch.h"
#include "../font_driver.h"
#include "../common/gl1_common.h"

#include "../gfx_display.h"

static const GLfloat gl1_menu_vertexes[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const GLfloat gl1_menu_tex_coords[] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static const float *gfx_display_gl1_get_default_vertices(void)
{
   return &gl1_menu_vertexes[0];
}

static const float *gfx_display_gl1_get_default_tex_coords(void)
{
   return &gl1_menu_tex_coords[0];
}

static void *gfx_display_gl1_get_default_mvp(void *data)
{
   gl1_t *gl1 = (gl1_t*)data;

   if (!gl1)
      return NULL;

   return &gl1->mvp_no_rot;
}

static GLenum gfx_display_prim_to_gl1_enum(
      enum gfx_display_prim_type type)
{
   switch (type)
   {
      case GFX_DISPLAY_PRIM_TRIANGLESTRIP:
         return GL_TRIANGLE_STRIP;
      case GFX_DISPLAY_PRIM_TRIANGLES:
         return GL_TRIANGLES;
      case GFX_DISPLAY_PRIM_NONE:
      default:
         break;
   }

   return 0;
}

static void gfx_display_gl1_blend_begin(void *data)
{
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void gfx_display_gl1_blend_end(void *data)
{
   glDisable(GL_BLEND);
}

static void gfx_display_gl1_draw(gfx_display_ctx_draw_t *draw,
      void *data,
      unsigned video_width,
      unsigned video_height)
{
   video_shader_ctx_mvp_t mvp;
   gl1_t             *gl1          = (gl1_t*)data;

   if (!gl1 || !draw)
      return;

   if (!draw->coords->vertex)
      draw->coords->vertex         = &gl1_menu_vertexes[0];
   if (!draw->coords->tex_coord)
      draw->coords->tex_coord      = &gl1_menu_tex_coords[0];
   if (!draw->coords->lut_tex_coord)
      draw->coords->lut_tex_coord  = &gl1_menu_tex_coords[0];
   if (!draw->texture)
      return;

   glViewport(draw->x, draw->y, draw->width, draw->height);

   glEnable(GL_TEXTURE_2D);

   glBindTexture(GL_TEXTURE_2D, (GLuint)draw->texture);

   mvp.data   = gl1;
   mvp.matrix = draw->matrix_data ? (math_matrix_4x4*)draw->matrix_data
      : (math_matrix_4x4*)&gl1->mvp_no_rot;

   glMatrixMode(GL_PROJECTION);
   glPushMatrix();
   glLoadMatrixf((const GLfloat*)mvp.matrix);

   glMatrixMode(GL_MODELVIEW);
   glPushMatrix();
   glLoadIdentity();

   glEnableClientState(GL_COLOR_ARRAY);
   glEnableClientState(GL_VERTEX_ARRAY);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);

#ifdef VITA
   {
      unsigned i;
      static float *vertices3 = NULL;

      if (vertices3)
         free(vertices3);
      vertices3 = (float*)malloc(sizeof(float) * 3 * draw->coords->vertices);
      for (i = 0; i < draw->coords->vertices; i++)
      {
         memcpy(&vertices3[i*3], &draw->coords->vertex[i*2], sizeof(float) * 2);
         vertices3[i*3+2]  = 0.0f;
      }
      glVertexPointer(3, GL_FLOAT, 0, vertices3);   
   }
#else
   glVertexPointer(2, GL_FLOAT, 0, draw->coords->vertex);
#endif

   glColorPointer(4, GL_FLOAT, 0, draw->coords->color);
   glTexCoordPointer(2, GL_FLOAT, 0, draw->coords->tex_coord);

   glDrawArrays(gfx_display_prim_to_gl1_enum(
            draw->prim_type), 0, draw->coords->vertices);

   glDisableClientState(GL_COLOR_ARRAY);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
   glDisableClientState(GL_VERTEX_ARRAY);

   glMatrixMode(GL_MODELVIEW);
   glPopMatrix();
   glMatrixMode(GL_PROJECTION);
   glPopMatrix();

   gl1->coords.color = gl1->white_color_ptr;
}

static bool gfx_display_gl1_font_init_first(
      void **font_handle, void *video_data,
      const char *font_path, float menu_font_size,
      bool is_threaded)
{
   font_data_t **handle = (font_data_t**)font_handle;
   if (!(*handle = font_driver_init_first(video_data,
         font_path, menu_font_size, true,
         is_threaded,
         FONT_DRIVER_RENDER_OPENGL1_API)))
       return false;
   return true;
}

static void gfx_display_gl1_scissor_begin(void *data,
      unsigned video_width,
      unsigned video_height,
      int x, int y,
      unsigned width, unsigned height)
{
   glScissor(x, video_height - y - height, width, height);
   glEnable(GL_SCISSOR_TEST);
}

static void gfx_display_gl1_scissor_end(
      void *data,
      unsigned video_width,
      unsigned video_height)
{
   glScissor(0, 0, video_width, video_height);
   glDisable(GL_SCISSOR_TEST);
}

gfx_display_ctx_driver_t gfx_display_ctx_gl1 = {
   gfx_display_gl1_draw,
   NULL, /* draw_pipeline */
   gfx_display_gl1_blend_begin,
   gfx_display_gl1_blend_end,
   gfx_display_gl1_get_default_mvp,
   gfx_display_gl1_get_default_vertices,
   gfx_display_gl1_get_default_tex_coords,
   gfx_display_gl1_font_init_first,
   GFX_VIDEO_DRIVER_OPENGL1,
   "gl1",
   false,
   gfx_display_gl1_scissor_begin,
   gfx_display_gl1_scissor_end
};

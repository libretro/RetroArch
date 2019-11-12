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
#include "../../gfx/font_driver.h"
#include "../../gfx/common/gl1_common.h"

#include "../menu_driver.h"

#ifdef VITA
static float *vertices3 = NULL;
#endif

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

static const float *menu_display_gl1_get_default_vertices(void)
{
   return &gl1_menu_vertexes[0];
}

static const float *menu_display_gl1_get_default_tex_coords(void)
{
   return &gl1_menu_tex_coords[0];
}

static void *menu_display_gl1_get_default_mvp(video_frame_info_t *video_info)
{
   gl1_t *gl1 = (gl1_t*)video_info->userdata;

   if (!gl1)
      return NULL;

   return &gl1->mvp_no_rot;
}

static GLenum menu_display_prim_to_gl1_enum(
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

static void menu_display_gl1_blend_begin(video_frame_info_t *video_info)
{
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void menu_display_gl1_blend_end(video_frame_info_t *video_info)
{
   glDisable(GL_BLEND);
}

static void menu_display_gl1_viewport(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
   if (draw)
      glViewport(draw->x, draw->y, draw->width, draw->height);
}

static void menu_display_gl1_draw(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
   video_shader_ctx_mvp_t mvp;
   gl1_t             *gl1          = (gl1_t*)video_info->userdata;

   if (!gl1 || !draw)
      return;

   if (!draw->coords->vertex)
      draw->coords->vertex = menu_display_gl1_get_default_vertices();
   if (!draw->coords->tex_coord)
      draw->coords->tex_coord = menu_display_gl1_get_default_tex_coords();
   if (!draw->coords->lut_tex_coord)
      draw->coords->lut_tex_coord = menu_display_gl1_get_default_tex_coords();

   menu_display_gl1_viewport(draw, video_info);

   glEnable(GL_TEXTURE_2D);

   glBindTexture(GL_TEXTURE_2D, (GLuint)draw->texture);

   mvp.data   = gl1;
   mvp.matrix = draw->matrix_data ? (math_matrix_4x4*)draw->matrix_data
      : (math_matrix_4x4*)menu_display_gl1_get_default_mvp(video_info);

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
   if (vertices3)
      free(vertices3);
   vertices3 = (float*)malloc(sizeof(float) * 3 * draw->coords->vertices);
   int i;
   for (i = 0; i < draw->coords->vertices; i++) {
      memcpy(&vertices3[i*3], &draw->coords->vertex[i*2], sizeof(float) * 2);
      vertices3[i*3] -= 0.5f;
      vertices3[i*3+2] = 0.0f;
   }
   glVertexPointer(3, GL_FLOAT, 0, vertices3);   
#else
   glVertexPointer(2, GL_FLOAT, 0, draw->coords->vertex);
#endif

   glColorPointer(4, GL_FLOAT, 0, draw->coords->color);
   glTexCoordPointer(2, GL_FLOAT, 0, draw->coords->tex_coord);

   glDrawArrays(menu_display_prim_to_gl1_enum(
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

static void menu_display_gl1_restore_clear_color(void)
{
   glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
}

static void menu_display_gl1_clear_color(
      menu_display_ctx_clearcolor_t *clearcolor,
      video_frame_info_t *video_info)
{
   if (!clearcolor)
      return;

   glClearColor(clearcolor->r,
         clearcolor->g, clearcolor->b, clearcolor->a);
   glClear(GL_COLOR_BUFFER_BIT);
}

static bool menu_display_gl1_font_init_first(
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

static void menu_display_gl1_scissor_begin(video_frame_info_t *video_info, int x, int y,
      unsigned width, unsigned height)
{
   glScissor(x, video_info->height - y - height, width, height);
   glEnable(GL_SCISSOR_TEST);
}

static void menu_display_gl1_scissor_end(video_frame_info_t *video_info)
{
   glScissor(0, 0, video_info->width, video_info->height);
   glDisable(GL_SCISSOR_TEST);
}

menu_display_ctx_driver_t menu_display_ctx_gl1 = {
   menu_display_gl1_draw,
   NULL,
   menu_display_gl1_viewport,
   menu_display_gl1_blend_begin,
   menu_display_gl1_blend_end,
   menu_display_gl1_restore_clear_color,
   menu_display_gl1_clear_color,
   menu_display_gl1_get_default_mvp,
   menu_display_gl1_get_default_vertices,
   menu_display_gl1_get_default_tex_coords,
   menu_display_gl1_font_init_first,
   MENU_VIDEO_DRIVER_OPENGL1,
   "gl1",
   false,
   menu_display_gl1_scissor_begin,
   menu_display_gl1_scissor_end
};

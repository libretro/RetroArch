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

#include "menu_display.h"
#include "menu_hash.h"
#include "menu_setting.h"
#include "menu_video.h"

#include "../gfx/video_common.h"

#ifdef HAVE_OPENGL
void menu_video_draw_frame(
      const shader_backend_t *shader,
      struct gfx_coords *coords,
      math_matrix_4x4 *mat, 
      bool blend,
      GLuint texture
      )
{
   driver_t *driver = driver_get_ptr();

   glBindTexture(GL_TEXTURE_2D, texture);

   shader->set_coords(coords);
   shader->set_mvp(driver->video_data, mat);

   if (blend)
      glEnable(GL_BLEND);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   if (blend)
      glDisable(GL_BLEND);
}

void menu_video_frame_background(
      menu_handle_t *menu,
      settings_t *settings,
      gl_t *gl,
      GLuint texture,
      float handle_alpha,
      float alpha,
      bool force_transparency)
{
   struct gfx_coords coords;
   GRfloat color[16], black_color[16],
           vertex[8], tex_coord[8];

   global_t *global = global_get_ptr();

   vertex[0] = 0;
   vertex[1] = 0;
   vertex[2] = 1;
   vertex[3] = 0;
   vertex[4] = 0;
   vertex[5] = 1;
   vertex[6] = 1;
   vertex[7] = 1;

   tex_coord[0] = 0;
   tex_coord[1] = 1;
   tex_coord[2] = 1;
   tex_coord[3] = 1;
   tex_coord[4] = 0;
   tex_coord[5] = 0;
   tex_coord[6] = 1;
   tex_coord[7] = 0;

   color[ 0] = 1.0f;
   color[ 1] = 1.0f;
   color[ 2] = 1.0f;
   color[ 3] = handle_alpha;
   color[ 4] = 1.0f;
   color[ 5] = 1.0f;
   color[ 6] = 1.0f;
   color[ 7] = handle_alpha;
   color[ 8] = 1.0f;
   color[ 9] = 1.0f;
   color[10] = 1.0f;
   color[11] = handle_alpha;
   color[12] = 1.0f;
   color[13] = 1.0f;
   color[14] = 1.0f;
   color[15] = handle_alpha;

   if (alpha > handle_alpha)
      alpha = handle_alpha;

   black_color[ 0] = 0.0f;
   black_color[ 1] = 0.0f;
   black_color[ 2] = 0.0f;
   black_color[ 3] = alpha;
   black_color[ 4] = 0.0f;
   black_color[ 5] = 0.0f;
   black_color[ 6] = 0.0f;
   black_color[ 7] = alpha;
   black_color[ 8] = 0.0f;
   black_color[ 9] = 0.0f;
   black_color[10] = 0.0f;
   black_color[11] = alpha;
   black_color[12] = 0.0f;
   black_color[13] = 0.0f;
   black_color[14] = 0.0f;
   black_color[15] = alpha;

   coords.vertices      = 4;
   coords.vertex        = vertex;
   coords.tex_coord     = tex_coord;
   coords.lut_tex_coord = tex_coord;
   coords.color         = black_color;

   if (gl->shader && gl->shader->use)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);

   menu_display_set_viewport();

   if ((settings->menu.pause_libretro
      || !global->main_is_init || (global->core_type == CORE_TYPE_DUMMY))
      && !force_transparency
      && texture)
      coords.color = color;

   menu_video_draw_frame(gl->shader, &coords,
         &gl->mvp_no_rot, true, texture);

   gl->coords.color = gl->white_color_ptr;
}
#endif

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

#include "menu_video.h"

#ifdef HAVE_THREADS
#include "../gfx/video_thread_wrapper.h"
#endif

#ifdef HAVE_OPENGL
void menu_video_draw_frame(
      unsigned x, unsigned y,
      unsigned width, unsigned height,
      const shader_backend_t *shader,
      struct gfx_coords *coords,
      math_matrix_4x4 *mat, 
      bool blend,
      GLuint texture
      )
{
   driver_t *driver = driver_get_ptr();

   glViewport(x, y, width, height);
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
      unsigned width,
      unsigned height,
      GLuint texture,
      float handle_alpha,
      bool force_transparency,
      GRfloat *coord_color,
      GRfloat *coord_color2,
      const GRfloat *vertex,
      const GRfloat *tex_coord)
{
   struct gfx_coords coords;

   global_t *global = global_get_ptr();

   coords.vertices      = 4;
   coords.vertex        = vertex;
   coords.tex_coord     = tex_coord;
   coords.lut_tex_coord = tex_coord;
   coords.color         = (const float*)coord_color;

   if (gl->shader && gl->shader->use)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);

   menu_display_ctl(MENU_DISPLAY_CTL_SET_VIEWPORT, NULL);

   if ((settings->menu.pause_libretro
      || !global->inited.main || (global->inited.core.type == CORE_TYPE_DUMMY))
      && !force_transparency
      && texture)
      coords.color = (const float*)coord_color2;

   menu_video_draw_frame(0, 0, width, height,
         gl->shader, &coords,
         &gl->mvp_no_rot, true, texture);

   gl->coords.color = gl->white_color_ptr;
}
#endif

const char *menu_video_get_ident(void)
{
#ifdef HAVE_THREADS
   settings_t *settings = config_get_ptr();

   if (settings->video.threaded)
      return rarch_threaded_video_get_ident();
#endif

   return video_driver_get_ident();
}

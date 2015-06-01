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

#ifndef _DISP_SHARED_H
#define _DISP_SHARED_H

#include <time.h>

#include "../../settings.h"

#include "../menu_display.h"
#include "../menu_entries_cbs.h"

#ifdef HAVE_OPENGL
#include "../../gfx/drivers/gl_common.h"

static INLINE void menu_gl_draw_frame(
      const shader_backend_t *shader,
      struct gl_coords *coords,
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

static INLINE void gl_menu_frame_background(
      menu_handle_t *menu,
      settings_t *settings,
      gl_t *gl,
      GLuint texture,
      float handle_alpha,
      float alpha,
      bool force_transparency)
{
   struct gl_coords coords;
   static const GLfloat vertex[] = {
      0, 0,
      1, 0,
      0, 1,
      1, 1,
   };
   global_t *global = global_get_ptr();

   static const GLfloat tex_coord[] = {
      0, 1,
      1, 1,
      0, 0,
      1, 0,
   };

   GLfloat color[] = {
      1.0f, 1.0f, 1.0f, handle_alpha,
      1.0f, 1.0f, 1.0f, handle_alpha,
      1.0f, 1.0f, 1.0f, handle_alpha,
      1.0f, 1.0f, 1.0f, handle_alpha,
   };

   if (alpha > handle_alpha)
      alpha = handle_alpha;

   GLfloat black_color[] = {
      0.0f, 0.0f, 0.0f, alpha,
      0.0f, 0.0f, 0.0f, alpha,
      0.0f, 0.0f, 0.0f, alpha,
      0.0f, 0.0f, 0.0f, alpha,
   };


   coords.vertices      = 4;
   coords.vertex        = vertex;
   coords.tex_coord     = tex_coord;
   coords.lut_tex_coord = tex_coord;
   coords.color         = black_color;

   menu_display_set_viewport();

   if ((settings->menu.pause_libretro
      || !global->main_is_init || global->libretro_dummy)
      && !force_transparency
      && texture)
      coords.color = color;

   menu_gl_draw_frame(gl->shader, &coords,
         &gl->mvp_no_rot, true, texture);

   gl->coords.color = gl->white_color_ptr;
}
#endif

static INLINE void disp_timedate_set_label(char *label, size_t label_size,
      unsigned time_mode)
{
   time_t time_;
   time(&time_);

   switch (time_mode)
   {
      case 0: /* Date and time */
         strftime(label, label_size, "%Y-%m-%d %H:%M:%S", localtime(&time_));
         break;
      case 1: /* Date */
         strftime(label, label_size, "%Y-%m-%d", localtime(&time_));
         break;
      case 2: /* Time */
         strftime(label, label_size, "%H:%M:%S", localtime(&time_));
         break;
      case 3: /* Time (hours-minutes) */
         strftime(label, label_size, "%H:%M", localtime(&time_));
         break;
   }
}
#endif

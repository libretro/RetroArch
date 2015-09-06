/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef _MENU_VIDEO_H
#define _MENU_VIDEO_H

#include "menu_shader.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_OPENGL
#include "../gfx/drivers/gl_common.h"

void menu_video_draw_frame(
      unsigned x, unsigned y,
      unsigned width, unsigned height,
      const shader_backend_t *shader,
      struct gfx_coords *coords,
      math_matrix_4x4 *mat, 
      bool blend,
      GLuint texture
      );

void menu_video_frame_background(
      menu_handle_t *menu,
      settings_t *settings,
      gl_t *gl,
      GLuint texture,
      float handle_alpha,
      bool force_transparency,
      GRfloat *color,
      const GRfloat *vertex,
      const GRfloat *tex_coord);
#endif

const char *menu_video_get_ident(void);

#ifdef __cplusplus
}
#endif

#endif

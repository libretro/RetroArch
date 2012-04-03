/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *

 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __SSNES_GLSL_H
#define __SSNES_GLSL_H

#include "../boolean.h"
#include "gl_common.h"

bool gl_glsl_init(const char *path);

void gl_glsl_deinit(void);

void gl_glsl_set_proj_matrix(void);

void gl_glsl_set_params(unsigned width, unsigned height, 
      unsigned tex_width, unsigned tex_height, 
      unsigned out_width, unsigned out_height,
      unsigned frame_counter,
      const struct gl_tex_info *info, 
      const struct gl_tex_info *prev_info,
      const struct gl_tex_info *fbo_info, unsigned fbo_info_cnt);

void gl_glsl_use(unsigned index);

unsigned gl_glsl_num(void);

bool gl_glsl_filter_type(unsigned index, bool *smooth);
void gl_glsl_shader_scale(unsigned index, struct gl_fbo_scale *scale);

#endif

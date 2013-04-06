/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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


#ifndef __RARCH_GLSL_H
#define __RARCH_GLSL_H

#include "../boolean.h"
#include "shader_common.h"

#define RARCH_GLSL_MAX_SHADERS 16
#define RARCH_GLSL_MENU_SHADER_INDEX (RARCH_GLSL_MAX_SHADERS - 1)

bool gl_glsl_init(const char *path);
void gl_glsl_deinit(void);

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
void gl_glsl_shader_scale(unsigned index, struct gfx_fbo_scale *scale);

bool gl_glsl_set_coords(const struct gl_coords *coords);
bool gl_glsl_set_mvp(const math_matrix *mat);

void gl_glsl_set_get_proc_address(gfx_ctx_proc_t (*proc)(const char*));

extern const gl_shader_backend_t gl_glsl_backend;

#endif

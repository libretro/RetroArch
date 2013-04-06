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


#ifndef __RARCH_CG_H
#define __RARCH_CG_H

#include "shader_common.h"
#include <stdint.h>

bool gl_cg_init(const char *path);
bool gl_cg_reinit(const char *path);
void gl_cg_deinit(void);

void gl_cg_set_params(unsigned width, unsigned height, 
      unsigned tex_width, unsigned tex_height, 
      unsigned out_width, unsigned out_height,
      unsigned frame_count,
      const struct gl_tex_info *info,
      const struct gl_tex_info *prev_info,
      const struct gl_tex_info *fbo_info,
      unsigned fbo_info_cnt);

void gl_cg_use(unsigned index);

unsigned gl_cg_num(void);

bool gl_cg_filter_type(unsigned index, bool *smooth);
void gl_cg_shader_scale(unsigned index, struct gfx_fbo_scale *scale);

bool gl_cg_set_mvp(const math_matrix *mat);
bool gl_cg_set_coords(const struct gl_coords *coords);


// Used on PS3, but not really platform specific.

#define RARCH_CG_MAX_SHADERS 16
#define RARCH_CG_MENU_SHADER_INDEX (RARCH_CG_MAX_SHADERS - 1)
void gl_cg_set_compiler_args(const char **argv);

bool gl_cg_load_shader(unsigned index, const char *path);

void gl_cg_invalidate_context(void); // Call when resetting GL context on PS3.
extern const gl_shader_backend_t gl_cg_backend;

#endif

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

#ifndef SHADER_COMMON_H__
#define SHADER_COMMON_H__

#include "../boolean.h"
#ifdef HAVE_OPENGL
#include "gl_common.h"
#endif
#include "gfx_context.h"
#include "shader_parse.h"
#include "math/matrix.h"

struct gl_shader_backend
{
   bool (*init)(const char *path);
   void (*deinit)(void);
   void (*set_params)(unsigned width, unsigned height, 
         unsigned tex_width, unsigned tex_height, 
         unsigned out_width, unsigned out_height,
         unsigned frame_counter,
         const struct gl_tex_info *info, 
         const struct gl_tex_info *prev_info,
         const struct gl_tex_info *fbo_info, unsigned fbo_info_cnt);

   void (*use)(unsigned index);
   unsigned (*num_shaders)(void);
   bool (*filter_type)(unsigned index, bool *smooth);
   void (*shader_scale)(unsigned index, struct gfx_fbo_scale *scale);
   bool (*set_coords)(const struct gl_coords *coords);
   bool (*set_mvp)(const math_matrix *mat);

   enum rarch_shader_type type;
};

#endif


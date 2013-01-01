/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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


#ifndef __RARCH_HLSL_H
#define __RARCH_HLSL_H

#include "../boolean.h"
#include <stdint.h>

bool hlsl_init(const char *path, IDirect3DDevice9 *device_ptr);

void hlsl_deinit(void);

void hlsl_set_proj_matrix(XMMATRIX rotation_value);

void hlsl_set_params(unsigned width, unsigned height,
      unsigned tex_width, unsigned tex_height,
      unsigned out_width, unsigned out_height,
      unsigned frame_count);

void hlsl_use(unsigned index);

bool hlsl_load_shader(unsigned index, const char *path);
unsigned d3d_hlsl_num(void);

#define RARCH_HLSL_MAX_SHADERS 16

#endif

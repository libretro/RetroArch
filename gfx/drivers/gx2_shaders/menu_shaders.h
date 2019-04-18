/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2018 - Ali Bouhlel
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

#pragma once

#include <wiiu/shader_utils.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
__attribute__((scalar_storage_order ("little-endian")))
__attribute__((aligned (16)))
{
   GX2_mat4x4 mvp;
   GX2_vec2 OutputSize;
   float time;
}menu_shader_uniform_t;

extern GX2Shader ribbon_shader;
extern GX2Shader ribbon_simple_shader;
extern GX2Shader snow_simple_shader;
extern GX2Shader snow_shader;
extern GX2Shader bokeh_shader;
extern GX2Shader snowflake_shader;

#ifdef __cplusplus
}
#endif

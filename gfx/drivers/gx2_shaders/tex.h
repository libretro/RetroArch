/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2016 - Ali Bouhlel
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

#ifndef TEX_SHADER_H
#define TEX_SHADER_H

#include <wiiu/shader_utils.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
   struct
   {
      float x;
      float y;
   }pos;

   struct
   {
      float u;
      float v;
   }coord;

   struct
   {
      float r;
      float g;
      float b;
      float a;
   }color;
}tex_shader_vertex_t;

extern GX2Shader tex_shader;

#ifdef __cplusplus
}
#endif

#endif /* TEX_SHADER_H */

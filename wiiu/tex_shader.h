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
#include <wiiu/gx2.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __attribute__((aligned(GX2_VERTEX_BUFFER_ALIGNMENT)))
{
   GX2VertexShader vs;
   GX2PixelShader ps;
   GX2SamplerVar sampler;
   struct
   {
      GX2AttribVar position;
      GX2AttribVar tex_coord;
   } attributes;
   struct
   {
      GX2AttribStream position;
      GX2AttribStream tex_coord;
   } attribute_stream;
   GX2FetchShader fs;
}tex_shader_t;

extern tex_shader_t tex_shader;

#ifdef __cplusplus
}
#endif

#endif // TEX_SHADER_H

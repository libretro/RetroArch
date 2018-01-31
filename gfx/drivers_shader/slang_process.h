/*  RetroArch - A frontend fror libretro.
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

#ifndef __GLSLANG_PROCESS_H__
#define __GLSLANG_PROCESS_H__

#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

#include "../video_shader_parse.h"
#include "slang_reflection.h"

typedef struct
{
   enum slang_semantic semantic;
   void*               data;
} uniform_map_t;

typedef struct
{
   enum slang_texture_semantic semantic;
   int                         id;
   void*                       texture_data;
   void*                       sampler_data;
   void*                       size_data;
} texture_map_t;

typedef struct
{
   texture_map_t* texture_map;
   uniform_map_t* uniform_map;
} semantics_map_t;

typedef struct
{
   void*    data;
   unsigned size;
   unsigned offset;
} uniform_sem_t;

typedef struct
{
   void*    data;
   void*    sampler_data;
   unsigned stage_mask;
   unsigned binding;
} texture_sem_t;

typedef struct
{
   unsigned       stage_mask;
   unsigned       binding;
   unsigned       size;
   int            uniform_count;
   uniform_sem_t* uniforms;
} cbuffer_sem_t;

typedef struct
{
   int            texture_count;
   texture_sem_t* textures;
   int            texture_binding_max;
   cbuffer_sem_t  cbuffers[SLANG_CBUFFER_MAX];
} pass_semantics_t;

#define SLANG_STAGE_VERTEX_MASK (1 << 0)
#define SLANG_STAGE_FRAGMENT_MASK (1 << 1)

RETRO_BEGIN_DECLS

bool slang_process(
      struct video_shader*   shader_info,
      unsigned               pass_number,
      enum rarch_shader_type dst_type,
      unsigned               version,
      const semantics_map_t* semantics_map,
      pass_semantics_t*      out);

RETRO_END_DECLS

#endif

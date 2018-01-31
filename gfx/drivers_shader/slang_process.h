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
   const char*         id;
} uniform_map_t;

typedef struct
{
   enum slang_texture_semantic semantic;
   int                         index;
   void*                       texture_data;
   const char*                 texture_id;
   void*                       sampler_data;
   const char*                 sampler_id;
   void*                       size_data;
   const char*                 size_id;
} texture_map_t;

#define SL_UNIFORM_MAP(sem, data) \
   { \
      sem, &data, #data \
   }

#define SL_TEXTURE_MAP_ARRAY(sem, index, tex, sampl, size) \
   { \
      sem, index, &tex, #tex, &sampl, #sampl, &size, #size \
   }

#define SL_TEXTURE_MAP(sem, tex, sampl, size) SL_TEXTURE_MAP_ARRAY(sem, 0, tex, sampl, size)

typedef struct
{
   texture_map_t* texture_map;
   uniform_map_t* uniform_map;
} semantics_map_t;

typedef struct
{
   void*       data;
   const char* data_id;
   unsigned    size;
   unsigned    offset;
   char        id[64];
} uniform_sem_t;

typedef struct
{
   void*       texture_data;
   const char* texture_id;
   void*       sampler_data;
   const char* sampler_id;
   unsigned    stage_mask;
   unsigned    binding;
   char        id[64];
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

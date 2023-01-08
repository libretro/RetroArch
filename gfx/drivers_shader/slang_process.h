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
#include "../../retroarch.h"
#include "slang_reflection.h"
#include "glslang_util.h"

typedef struct
{
   void*  ptr;
   size_t stride;
} data_map_t;

typedef struct
{
   void*  image;
   size_t image_stride;
   void*  size;
   size_t size_stride;
} texture_map_t;

typedef struct
{
   texture_map_t textures[SLANG_NUM_TEXTURE_SEMANTICS];
   void*         uniforms[SLANG_NUM_BASE_SEMANTICS];
} semantics_map_t;

typedef struct
{
   void*    data;
   unsigned size;
   unsigned offset;
   char     id[64];
} uniform_sem_t;

typedef struct
{
   void*              texture_data;
   enum gfx_wrap_type wrap;
   unsigned           filter;
   unsigned           stage_mask;
   unsigned           binding;
   char               id[64];
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
   glslang_format format;
} pass_semantics_t;

#define SLANG_STAGE_VERTEX_MASK (1 << 0)
#define SLANG_STAGE_FRAGMENT_MASK (1 << 1)

RETRO_BEGIN_DECLS

/* Utility function to implement the same parameter reflection
 * which happens in the slang backend.
 * This does preprocess over the input file to handle #includes and so on. */
bool slang_preprocess_parse_parameters(const char *shader_path,
      struct video_shader *shader);

bool slang_process(
      struct video_shader*   shader_info,
      unsigned               pass_number,
      enum rarch_shader_type dst_type,
      unsigned               version,
      const semantics_map_t* semantics_map,
      pass_semantics_t*      out);

RETRO_END_DECLS

#ifdef __cplusplus

#include "glslang_util_cxx.h"

bool slang_preprocess_parse_parameters(glslang_meta& meta,
      struct video_shader *shader);
#endif

#endif

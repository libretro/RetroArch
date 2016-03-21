/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2016 - Hans-Kristian Arntzen
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

#ifndef SLANG_REFLECTION_HPP
#define SLANG_REFLECTION_HPP

#include <string>
#include <stdint.h>

// Textures with built-in meaning.
enum slang_texture_semantic
{
   SLANG_TEXTURE_SEMANTIC_ORIGINAL = 0,
   SLANG_TEXTURE_SEMANTIC_SOURCE = 1,
   SLANG_TEXTURE_SEMANTIC_OUTPUT = 2,

   SLANG_TEXTURE_NUM_SEMANTICS,
   SLANG_TEXTURE_INVALID_SEMANTIC = -1
};

enum slang_stage
{
   SLANG_STAGE_VERTEX_MASK = 1 << 0,
   SLANG_STAGE_FRAGMENT_MASK = 1 << 1
};
#define SLANG_NUM_BINDINGS 16

struct slang_texture_semantic_meta
{
   size_t ubo_offset = 0;
   unsigned binding = 0;
   uint32_t stage_mask = 0;
};

struct slang_reflection
{
   size_t ubo_size = 0;
   size_t mvp_offset = 0;
   unsigned ubo_binding = 0;
   uint32_t ubo_stage_mask = 0;

   slang_texture_semantic_meta semantic_textures[SLANG_TEXTURE_NUM_SEMANTICS];
   uint32_t semantic_texture_mask = 0;
   uint32_t semantic_texture_ubo_mask = 0;
};

bool slang_reflect_spirv(const std::vector<uint32_t> &vertex,
      const std::vector<uint32_t> &fragment,
      slang_reflection *reflection);

#endif


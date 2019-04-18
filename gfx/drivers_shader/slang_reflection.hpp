/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2019 - Hans-Kristian Arntzen
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

#ifndef SLANG_REFLECTION_HPP_
#define SLANG_REFLECTION_HPP_

#include <string>
#include <unordered_map>
#include <stdint.h>
#include <spirv_cross.hpp>

struct slang_semantic_location
{
   int ubo_vertex = -1;
   int push_vertex = -1;
   int ubo_fragment = -1;
   int push_fragment = -1;
};

struct slang_texture_semantic_meta
{
   size_t ubo_offset = 0;
   size_t push_constant_offset = 0;
   unsigned binding = 0;
   uint32_t stage_mask = 0;

   bool texture = false;
   bool uniform = false;
   bool push_constant = false;

   // For APIs which need location information ala legacy GL.
   // API user fills this struct in.
   slang_semantic_location location;
};

struct slang_semantic_meta
{
   size_t ubo_offset = 0;
   size_t push_constant_offset = 0;
   unsigned num_components = 0;
   bool uniform = false;
   bool push_constant = false;

   // For APIs which need location information ala legacy GL.
   slang_semantic_location location;
};

struct slang_texture_semantic_map
{
   slang_texture_semantic semantic;
   unsigned index;
};

struct slang_semantic_map
{
   slang_semantic semantic;
   unsigned index;
};

struct slang_reflection
{
   slang_reflection();

   size_t ubo_size = 0;
   size_t push_constant_size = 0;

   unsigned ubo_binding = 0;
   uint32_t ubo_stage_mask = 0;
   uint32_t push_constant_stage_mask = 0;

   std::vector<slang_texture_semantic_meta> semantic_textures[SLANG_NUM_TEXTURE_SEMANTICS];
   slang_semantic_meta semantics[SLANG_NUM_SEMANTICS];
   std::vector<slang_semantic_meta> semantic_float_parameters;

   const std::unordered_map<std::string, slang_texture_semantic_map> *texture_semantic_map = nullptr;
   const std::unordered_map<std::string, slang_texture_semantic_map> *texture_semantic_uniform_map = nullptr;
   const std::unordered_map<std::string, slang_semantic_map> *semantic_map = nullptr;
   unsigned pass_number = 0;
};

bool slang_reflect_spirv(const std::vector<uint32_t> &vertex,
      const std::vector<uint32_t> &fragment,
      slang_reflection *reflection);

bool slang_reflect(const spirv_cross::Compiler &vertex_compiler, const spirv_cross::Compiler &fragment_compiler,
      const spirv_cross::ShaderResources &vertex, const spirv_cross::ShaderResources &fragment,
      slang_reflection *reflection);

#endif

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
   int ubo_vertex;
   int push_vertex;
   int ubo_fragment;
   int push_fragment;

   slang_semantic_location()
      : ubo_vertex(-1), push_vertex(-1),
        ubo_fragment(-1), push_fragment(-1) {}
};

struct slang_texture_semantic_meta
{
   size_t   ubo_offset;
   size_t   push_constant_offset;
   unsigned binding;
   uint32_t stage_mask;

   bool texture;
   bool uniform;
   bool push_constant;

   /* For APIs which need location information ala legacy GL.
    * API user fills this struct in. */
   slang_semantic_location location;

   slang_texture_semantic_meta()
      : ubo_offset(0), push_constant_offset(0),
        binding(0), stage_mask(0),
        texture(false), uniform(false), push_constant(false) {}
};

struct slang_semantic_meta
{
   size_t   ubo_offset;
   size_t   push_constant_offset;
   unsigned num_components;
   bool     uniform;
   bool     push_constant;

   /* For APIs which need location information ala legacy GL. */
   slang_semantic_location location;

   slang_semantic_meta()
      : ubo_offset(0), push_constant_offset(0),
        num_components(0), uniform(false), push_constant(false) {}
};

struct slang_reflection
{
   slang_reflection();

   size_t   ubo_size;
   size_t   push_constant_size;

   unsigned ubo_binding;
   uint32_t ubo_stage_mask;
   uint32_t push_constant_stage_mask;

   std::vector<slang_texture_semantic_meta>
      semantic_textures[SLANG_NUM_TEXTURE_SEMANTICS];
   slang_semantic_meta semantics[SLANG_NUM_SEMANTICS];
   std::vector<slang_semantic_meta> semantic_float_parameters;

   const std::unordered_map<std::string, slang_texture_semantic_map> *texture_semantic_map;
   const std::unordered_map<std::string, slang_texture_semantic_map> *texture_semantic_uniform_map;
   const std::unordered_map<std::string, slang_semantic_map>         *semantic_map;
   unsigned pass_number;
};

template <typename P>
static bool slang_set_unique_map(std::unordered_map<std::string, P> &m,
      const std::string &name, const P &p)
{
   typename std::unordered_map<std::string, P>::iterator itr = m.find(name);
   /* Alias already exists? */
   if (itr != m.end())
      return false;
   m[name] = p;
   return true;
}

bool slang_reflect_spirv(
      const std::vector<uint32_t> &vertex,
      const std::vector<uint32_t> &fragment,
      slang_reflection *reflection);

bool slang_reflect(
      const spirv_cross::Compiler &vertex_compiler,
      const spirv_cross::Compiler &fragment_compiler,
      const spirv_cross::ShaderResources &vertex,
      const spirv_cross::ShaderResources &fragment,
      slang_reflection *reflection);

#endif

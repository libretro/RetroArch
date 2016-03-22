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

#include "spir2cross.hpp"
#include "slang_reflection.hpp"
#include <vector>
#include <stdio.h>
#include "../../verbosity.h"

using namespace std;
using namespace spir2cross;

static const char *texture_semantic_names[] = {
   "Original",
   "Source",
};

static const char *texture_semantic_uniform_names[] = {
   "OriginalSize",
   "SourceSize",
};

static const char *semantic_uniform_names[] = {
   "MVP",
   "OutputSize",
   "FinalViewportSize",
};

static slang_texture_semantic slang_name_to_texture_semantic(const string &name)
{
   unsigned i = 0;
   for (auto n : texture_semantic_names)
   {
      if (name == n)
         return static_cast<slang_texture_semantic>(i);
      i++;
   }
   return SLANG_INVALID_TEXTURE_SEMANTIC;
}

static slang_texture_semantic slang_uniform_name_to_texture_semantic(const string &name)
{
   unsigned i = 0;
   for (auto n : texture_semantic_uniform_names)
   {
      if (name == n)
         return static_cast<slang_texture_semantic>(i);
      i++;
   }
   return SLANG_INVALID_TEXTURE_SEMANTIC;
}

static slang_semantic slang_uniform_name_to_semantic(const string &name)
{
   unsigned i = 0;
   for (auto n : semantic_uniform_names)
   {
      if (name == n)
         return static_cast<slang_semantic>(i);
      i++;
   }

   return SLANG_INVALID_SEMANTIC;
}

static bool set_ubo_texture_offset(slang_reflection *reflection, slang_texture_semantic semantic,
      size_t offset)
{
   if (reflection->semantic_texture_ubo_mask & (1u << semantic))
   {
      if (reflection->semantic_textures[semantic].ubo_offset != offset)
      {
         RARCH_ERR("[slang]: Vertex and fragment have different offsets for same semantic %s (%u vs. %u).\n",
               texture_semantic_uniform_names[semantic],
               unsigned(reflection->semantic_textures[semantic].ubo_offset),
               unsigned(offset));
         return false;
      }
   }
   reflection->semantic_texture_ubo_mask |= 1u << semantic;
   reflection->semantic_textures[semantic].ubo_offset = offset;
   return true;
}

static bool set_ubo_offset(slang_reflection *reflection, slang_semantic semantic,
      size_t offset, unsigned num_components)
{
   if (reflection->semantic_ubo_mask & (1u << semantic))
   {
      if (reflection->semantics[semantic].ubo_offset != offset)
      {
         RARCH_ERR("[slang]: Vertex and fragment have different offsets for same semantic %s (%u vs. %u).\n",
               semantic_uniform_names[semantic],
               unsigned(reflection->semantics[semantic].ubo_offset),
               unsigned(offset));
         return false;
      }

      if (reflection->semantics[semantic].num_components != num_components)
      {
         RARCH_ERR("[slang]: Vertex and fragment have different components for same semantic %s (%u vs. %u).\n",
               semantic_uniform_names[semantic],
               unsigned(reflection->semantics[semantic].num_components),
               unsigned(num_components));
      }
   }
   reflection->semantic_ubo_mask |= 1u << semantic;
   reflection->semantics[semantic].ubo_offset = offset;
   reflection->semantics[semantic].num_components = num_components;
   return true;
}

static bool validate_type_for_semantic(const SPIRType &type, slang_semantic sem)
{
   if (!type.array.empty())
      return false;
   if (type.basetype != SPIRType::Float && type.basetype != SPIRType::Int && type.basetype != SPIRType::UInt)
      return false;

   switch (sem)
   {
      case SLANG_SEMANTIC_MVP:
         // mat4
         return type.basetype == SPIRType::Float && type.vecsize == 4 && type.columns == 4;

      default:
         // vec4
         return type.basetype == SPIRType::Float && type.vecsize == 4 && type.columns == 1;
   }
}

static bool validate_type_for_texture_semantic(const SPIRType &type)
{
   if (!type.array.empty())
      return false;
   return type.basetype == SPIRType::Float && type.vecsize == 4 && type.columns == 1;
}

static bool add_active_buffer_ranges(const Compiler &compiler, const Resource &resource,
      slang_reflection *reflection)
{
   // Get which uniforms are actually in use by this shader.
   auto ranges = compiler.get_active_buffer_ranges(resource.id);
   for (auto &range : ranges)
   {
      auto &name = compiler.get_member_name(resource.type_id, range.index);
      auto &type = compiler.get_type(compiler.get_type(resource.type_id).member_types[range.index]);
      slang_semantic sem = slang_uniform_name_to_semantic(name);
      slang_texture_semantic tex_sem = slang_uniform_name_to_texture_semantic(name);

      if (sem != SLANG_INVALID_SEMANTIC)
      {
         if (!validate_type_for_semantic(type, sem))
         {
            RARCH_ERR("[slang]: Underlying type of semantic is invalid.\n");
            return false;
         }

         if (!set_ubo_offset(reflection, sem, range.offset, type.vecsize))
            return false;
      }
      else if (tex_sem != SLANG_INVALID_TEXTURE_SEMANTIC)
      {
         if (!validate_type_for_texture_semantic(type))
         {
            RARCH_ERR("[slang]: Underlying type of texture semantic is invalid.\n");
            return false;
         }

         if (!set_ubo_texture_offset(reflection, tex_sem, range.offset))
            return false;
      }
      else
      {
         // TODO: Handle invalid semantics as user defined.
      }
   }
   return true;
}

static bool slang_reflect(const Compiler &vertex_compiler, const Compiler &fragment_compiler,
      const ShaderResources &vertex, const ShaderResources &fragment,
      slang_reflection *reflection)
{
   // Validate use of unexpected types.
   if (
         !vertex.sampled_images.empty() ||
         !vertex.storage_buffers.empty() ||
         !vertex.subpass_inputs.empty() ||
         !vertex.storage_images.empty() ||
         !vertex.atomic_counters.empty() ||
         !vertex.push_constant_buffers.empty() ||
         !fragment.storage_buffers.empty() ||
         !fragment.subpass_inputs.empty() ||
         !fragment.storage_images.empty() ||
         !fragment.atomic_counters.empty() ||
         !fragment.push_constant_buffers.empty())
   {
      RARCH_ERR("[slang]: Invalid resource type detected.\n");
      return false;
   }

   // Validate vertex input.
   if (vertex.stage_inputs.size() != 2)
   {
      RARCH_ERR("[slang]: Vertex must have two attributes.\n");
      return false;
   }

   if (fragment.stage_outputs.size() != 1)
   {
      RARCH_ERR("[slang]: Multiple render targets not supported.\n");
      return false;
   }

   if (fragment_compiler.get_decoration(fragment.stage_outputs[0].id, spv::DecorationLocation) != 0)
   {
      RARCH_ERR("[slang]: Render target must use location = 0.\n");
      return false;
   }

   uint32_t location_mask = 0;
   for (auto &input : vertex.stage_inputs)
      location_mask |= 1 << vertex_compiler.get_decoration(input.id, spv::DecorationLocation);

   if (location_mask != 0x3)
   {
      RARCH_ERR("[slang]: The two vertex attributes do not use location = 0 and location = 1.\n");
      return false;
   }

   // Validate the single uniform buffer.
   if (vertex.uniform_buffers.size() > 1)
   {
      RARCH_ERR("[slang]: Vertex must use zero or one uniform buffer.\n");
      return false;
   }

   if (fragment.uniform_buffers.size() > 1)
   {
      RARCH_ERR("[slang]: Fragment must use zero or one uniform buffer.\n");
      return false;
   }

   uint32_t vertex_ubo = vertex.uniform_buffers.empty() ? 0 : vertex.uniform_buffers[0].id;
   uint32_t fragment_ubo = fragment.uniform_buffers.empty() ? 0 : fragment.uniform_buffers[0].id;

   if (vertex_ubo &&
         vertex_compiler.get_decoration(vertex_ubo, spv::DecorationDescriptorSet) != 0)
   {
      RARCH_ERR("[slang]: Resources must use descriptor set #0.\n");
      return false;
   }

   if (fragment_ubo &&
         fragment_compiler.get_decoration(fragment_ubo, spv::DecorationDescriptorSet) != 0)
   {
      RARCH_ERR("[slang]: Resources must use descriptor set #0.\n");
      return false;
   }

   unsigned vertex_ubo_binding = vertex_ubo ?
      vertex_compiler.get_decoration(vertex_ubo, spv::DecorationBinding) : -1u;
   unsigned fragment_ubo_binding = fragment_ubo ?
      fragment_compiler.get_decoration(fragment_ubo, spv::DecorationBinding) : -1u;
   bool has_ubo = vertex_ubo || fragment_ubo;

   if (vertex_ubo_binding != -1u &&
         fragment_ubo_binding != -1u &&
         vertex_ubo_binding != fragment_ubo_binding)
   {
      RARCH_ERR("[slang]: Vertex and fragment uniform buffer must have same binding.\n");
      return false;
   }

   unsigned ubo_binding = vertex_ubo_binding != -1u ? vertex_ubo_binding : fragment_ubo_binding;

   if (has_ubo && ubo_binding >= SLANG_NUM_BINDINGS)
   {
      RARCH_ERR("[slang]: Binding %u is out of range.\n", ubo_binding);
      return false;
   }

   reflection->ubo_binding = has_ubo ? ubo_binding : 0;
   reflection->ubo_stage_mask = 0;
   reflection->ubo_size = 0;

   if (vertex_ubo)
   {
      reflection->ubo_stage_mask |= SLANG_STAGE_VERTEX_MASK;
      reflection->ubo_size = max(reflection->ubo_size,
            vertex_compiler.get_declared_struct_size(vertex_compiler.get_type(vertex.uniform_buffers[0].type_id)));
   }

   if (fragment_ubo)
   {
      reflection->ubo_stage_mask |= SLANG_STAGE_FRAGMENT_MASK;
      reflection->ubo_size = max(reflection->ubo_size,
            fragment_compiler.get_declared_struct_size(fragment_compiler.get_type(fragment.uniform_buffers[0].type_id)));
   }

   // Find all relevant uniforms.
   if (vertex_ubo && !add_active_buffer_ranges(vertex_compiler, vertex.uniform_buffers[0], reflection))
      return false;
   if (fragment_ubo && !add_active_buffer_ranges(fragment_compiler, fragment.uniform_buffers[0], reflection))
      return false;

   uint32_t binding_mask = has_ubo ? (1 << ubo_binding) : 0;

   // On to textures.
   for (auto &texture : fragment.sampled_images)
   {
      unsigned set = fragment_compiler.get_decoration(texture.id,
            spv::DecorationDescriptorSet);
      unsigned binding = fragment_compiler.get_decoration(texture.id,
            spv::DecorationBinding);

      if (set != 0)
      {
         RARCH_ERR("[slang]: Resources must use descriptor set #0.\n");
         return false;
      }

      if (binding >= SLANG_NUM_BINDINGS)
      {
         RARCH_ERR("[slang]: Binding %u is out of range.\n", ubo_binding);
         return false;
      }

      if (binding_mask & (1 << binding))
      {
         RARCH_ERR("[slang]: Binding %u is already in use.\n", binding);
         return false;
      }
      binding_mask |= 1 << binding;

      slang_texture_semantic index = slang_name_to_texture_semantic(texture.name);
      
      if (index == SLANG_INVALID_TEXTURE_SEMANTIC)
      {
         RARCH_ERR("[slang]: Non-semantic textures not supported yet.\n");
         return false;
      }

      auto &semantic = reflection->semantic_textures[index];
      semantic.binding = binding;
      semantic.stage_mask = SLANG_STAGE_FRAGMENT_MASK;
      reflection->semantic_texture_mask |= 1 << index;
   }

   RARCH_LOG("[slang]: Reflection\n");
   RARCH_LOG("[slang]:   Textures:\n");
   for (unsigned i = 0; i < SLANG_NUM_TEXTURE_SEMANTICS; i++)
      if (reflection->semantic_texture_mask & (1u << i))
         RARCH_LOG("[slang]:      %s\n", texture_semantic_names[i]);

   RARCH_LOG("[slang]:\n");
   RARCH_LOG("[slang]:   Uniforms (Vertex: %s, Fragment: %s):\n",
         reflection->ubo_stage_mask & SLANG_STAGE_VERTEX_MASK ? "yes": "no",
         reflection->ubo_stage_mask & SLANG_STAGE_FRAGMENT_MASK ? "yes": "no");
   for (unsigned i = 0; i < SLANG_NUM_SEMANTICS; i++)
   {
      if (reflection->semantic_ubo_mask & (1u << i))
      {
         RARCH_LOG("[slang]:      %s (Offset: %u)\n", semantic_uniform_names[i],
               unsigned(reflection->semantics[i].ubo_offset));
      }
   }

   for (unsigned i = 0; i < SLANG_NUM_TEXTURE_SEMANTICS; i++)
   {
      if (reflection->semantic_texture_ubo_mask & (1u << i))
      {
         RARCH_LOG("[slang]:      %s (Offset: %u)\n", texture_semantic_uniform_names[i],
               unsigned(reflection->semantic_textures[i].ubo_offset));
      }
   }

   return true;
}

bool slang_reflect_spirv(const std::vector<uint32_t> &vertex,
      const std::vector<uint32_t> &fragment,
      slang_reflection *reflection)
{
   try
   {
      Compiler vertex_compiler(vertex);
      Compiler fragment_compiler(fragment);
      auto vertex_resources = vertex_compiler.get_shader_resources();
      auto fragment_resources = fragment_compiler.get_shader_resources();

      if (!slang_reflect(vertex_compiler, fragment_compiler,
               vertex_resources, fragment_resources,
               reflection))
      {
         RARCH_ERR("[slang]: Failed to reflect SPIR-V. Resource usage is inconsistent with expectations.\n");
         return false;
      }

      return true;
   }
   catch (const std::exception &e)
   {
      RARCH_ERR("[slang]: spir2cross threw exception: %s.\n", e.what());
      return false;
   }
}


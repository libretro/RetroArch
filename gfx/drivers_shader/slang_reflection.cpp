/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2017 - Hans-Kristian Arntzen
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

#include "spirv_cross.hpp"
#include "slang_reflection.h"
#include "slang_reflection.hpp"
#include <vector>
#include <algorithm>
#include <stdio.h>
#include <compat/strl.h>
#include "../../verbosity.h"

using namespace std;
using namespace spirv_cross;

static const char *texture_semantic_names[] = {
   "Original",
   "Source",
   "OriginalHistory",
   "PassOutput",
   "PassFeedback",
   "User",
   nullptr
};

static const char *texture_semantic_uniform_names[] = {
   "OriginalSize",
   "SourceSize",
   "OriginalHistorySize",
   "PassOutputSize",
   "PassFeedbackSize",
   "UserSize",
   nullptr
};

static const char *semantic_uniform_names[] = {
   "MVP",
   "OutputSize",
   "FinalViewportSize",
   "FrameCount",
   "FrameDirection",
};

static bool slang_texture_semantic_is_array(slang_texture_semantic sem)
{
   switch (sem)
   {
      case SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY:
      case SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT:
      case SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK:
      case SLANG_TEXTURE_SEMANTIC_USER:
         return true;

      default:
         break;
   }

   return false;
}

slang_reflection::slang_reflection()
{
   unsigned i;

   for (i = 0; i < SLANG_NUM_TEXTURE_SEMANTICS; i++)
      semantic_textures[i].resize(
            slang_texture_semantic_is_array(
               static_cast<slang_texture_semantic>(i))
            ? 0 : 1);
}

static slang_texture_semantic slang_name_to_texture_semantic_array(
      const string &name, const char **names,
      unsigned *index)
{
   unsigned i = 0;
   while (*names)
   {
      const char                   *n = *names;
      slang_texture_semantic semantic = static_cast<slang_texture_semantic>(i);

      if (slang_texture_semantic_is_array(semantic))
      {
         size_t baselen = strlen(n);
         int        cmp = strncmp(n, name.c_str(), baselen);

         if (cmp == 0)
         {
            *index = (unsigned)strtoul(name.c_str() + baselen, nullptr, 0);
            return semantic;
         }
      }
      else if (name == n)
      {
         *index = 0;
         return semantic;
      }

      i++;
      names++;
   }
   return SLANG_INVALID_TEXTURE_SEMANTIC;
}

static slang_texture_semantic slang_name_to_texture_semantic(
      const unordered_map<string, slang_texture_semantic_map> &semantic_map,
      const string &name, unsigned *index)
{
   auto itr = semantic_map.find(name);
   if (itr != end(semantic_map))
   {
      *index = itr->second.index;
      return itr->second.semantic;
   }

   return slang_name_to_texture_semantic_array(
         name, texture_semantic_names, index);
}

static slang_texture_semantic slang_uniform_name_to_texture_semantic(
      const unordered_map<string, slang_texture_semantic_map> &semantic_map,
      const string &name, unsigned *index)
{
   auto itr = semantic_map.find(name);
   if (itr != end(semantic_map))
   {
      *index = itr->second.index;
      return itr->second.semantic;
   }

   return slang_name_to_texture_semantic_array(name,
         texture_semantic_uniform_names, index);
}

static slang_semantic slang_uniform_name_to_semantic(
      const unordered_map<string, slang_semantic_map> &semantic_map,
      const string &name, unsigned *index)
{
   unsigned i = 0;
   auto itr   = semantic_map.find(name);

   if (itr != end(semantic_map))
   {
      *index = itr->second.index;
      return itr->second.semantic;
   }

   /* No builtin semantics are arrayed. */
   *index = 0;
   for (auto n : semantic_uniform_names)
   {
      if (name == n)
         return static_cast<slang_semantic>(i);
      i++;
   }

   return SLANG_INVALID_SEMANTIC;
}

template <typename T>
static void resize_minimum(T &vec, unsigned minimum)
{
   if (vec.size() < minimum)
      vec.resize(minimum);
}

static bool set_ubo_texture_offset(
      slang_reflection *reflection,
      slang_texture_semantic semantic,
      unsigned index,
      size_t offset, bool push_constant)
{
   resize_minimum(reflection->semantic_textures[semantic], index + 1);
   auto &sem           = reflection->semantic_textures[semantic][index];
   auto &active        = push_constant ? sem.push_constant : sem.uniform;
   auto &active_offset = push_constant ? sem.push_constant_offset : sem.ubo_offset;

   if (active)
   {
      if (active_offset != offset)
      {
         RARCH_ERR("[slang]: Vertex and fragment have"
               " different offsets for same semantic %s #%u (%u vs. %u).\n",
               texture_semantic_uniform_names[semantic],
               index,
               unsigned(active_offset),
               unsigned(offset));
         return false;
      }
   }

   active        = true;
   active_offset = offset;
   return true;
}

static bool set_ubo_float_parameter_offset(
      slang_reflection *reflection,
      unsigned index, size_t offset,
      unsigned num_components,
      bool push_constant)
{
   resize_minimum(reflection->semantic_float_parameters, index + 1);
   auto &sem           = reflection->semantic_float_parameters[index];
   auto &active        = push_constant ? sem.push_constant : sem.uniform;
   auto &active_offset = push_constant ? sem.push_constant_offset : sem.ubo_offset;

   if (active)
   {
      if (active_offset != offset)
      {
         RARCH_ERR("[slang]: Vertex and fragment have different"
               " offsets for same parameter #%u (%u vs. %u).\n",
               index,
               unsigned(active_offset),
               unsigned(offset));
         return false;
      }
   }

   if (  (sem.num_components != num_components) && 
         (sem.uniform || sem.push_constant))
   {
      RARCH_ERR("[slang]: Vertex and fragment have different "
            "components for same parameter #%u (%u vs. %u).\n",
            index,
            unsigned(sem.num_components),
            unsigned(num_components));
      return false;
   }

   active             = true;
   active_offset      = offset;
   sem.num_components = num_components;
   return true;
}

static bool set_ubo_offset(
      slang_reflection *reflection,
      slang_semantic semantic,
      size_t offset, unsigned num_components, bool push_constant)
{
   auto &sem           = reflection->semantics[semantic];
   auto &active        = push_constant ? sem.push_constant : sem.uniform;
   auto &active_offset = push_constant ? sem.push_constant_offset : sem.ubo_offset;

   if (active)
   {
      if (active_offset != offset)
      {
         RARCH_ERR("[slang]: Vertex and fragment have "
               "different offsets for same semantic %s (%u vs. %u).\n",
               semantic_uniform_names[semantic],
               unsigned(active_offset),
               unsigned(offset));
         return false;
      }

   }

   if (  (sem.num_components != num_components) && 
         (sem.uniform || sem.push_constant))
   {
      RARCH_ERR("[slang]: Vertex and fragment have different"
            " components for same semantic %s (%u vs. %u).\n",
            semantic_uniform_names[semantic],
            unsigned(sem.num_components),
            unsigned(num_components));
      return false;
   }

   active             = true;
   active_offset      = offset;
   sem.num_components = num_components;
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
         /* mat4 */
      case SLANG_SEMANTIC_MVP:
         return type.basetype == SPIRType::Float && type.vecsize == 4 && type.columns == 4;
         /* uint */
      case SLANG_SEMANTIC_FRAME_COUNT:
         return type.basetype == SPIRType::UInt  && type.vecsize == 1 && type.columns == 1;
         /* int */
      case SLANG_SEMANTIC_FRAME_DIRECTION:
         return type.basetype == SPIRType::Int   && type.vecsize == 1 && type.columns == 1;
         /* float */
      case SLANG_SEMANTIC_FLOAT_PARAMETER:
         return type.basetype == SPIRType::Float && type.vecsize == 1 && type.columns == 1;
         /* vec4 */
      default:
         return type.basetype == SPIRType::Float && type.vecsize == 4 && type.columns == 1;
   }
}

static bool validate_type_for_texture_semantic(const SPIRType &type)
{
   if (!type.array.empty())
      return false;
   return (type.basetype == SPIRType::Float) && 
          (type.vecsize  == 4)               && 
          (type.columns  == 1);
}

static bool add_active_buffer_ranges(
      const Compiler &compiler,
      const Resource &resource,
      slang_reflection *reflection,
      bool push_constant)
{
   unsigned i;
   /* Get which uniforms are actually in use by this shader. */
   auto ranges = compiler.get_active_buffer_ranges(resource.id);

   for (i = 0; i < ranges.size(); i++)
   {
      unsigned sem_index             = 0;
      unsigned tex_sem_index         = 0;
      auto &name                     = compiler.get_member_name(
            resource.base_type_id, ranges[i].index);
      auto &type                     = compiler.get_type(
            compiler.get_type(resource.base_type_id).member_types[
            ranges[i].index]);
      slang_semantic sem             = slang_uniform_name_to_semantic(
            *reflection->semantic_map, name, &sem_index);
      slang_texture_semantic tex_sem = slang_uniform_name_to_texture_semantic(
            *reflection->texture_semantic_uniform_map,
            name, &tex_sem_index);

      if (tex_sem == SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT && tex_sem_index >= reflection->pass_number)
      {
         RARCH_ERR("[slang]: Non causal filter chain detected. "
               "Shader is trying to use output from pass #%u,"
               " but this shader is pass #%u.\n",
               tex_sem_index, reflection->pass_number);
         return false;
      }

      if (sem != SLANG_INVALID_SEMANTIC)
      {
         if (!validate_type_for_semantic(type, sem))
         {
            RARCH_ERR("[slang]: Underlying type of semantic is invalid.\n");
            return false;
         }

         switch (sem)
         {
            case SLANG_SEMANTIC_FLOAT_PARAMETER:
               if (!set_ubo_float_parameter_offset(reflection, sem_index,
                        ranges[i].offset, type.vecsize, push_constant))
                  return false;
               break;

            default:
               if (!set_ubo_offset(reflection, sem,
                        ranges[i].offset,
                        type.vecsize * type.columns, push_constant))
                  return false;
               break;
         }
      }
      else if (tex_sem != SLANG_INVALID_TEXTURE_SEMANTIC)
      {
         if (!validate_type_for_texture_semantic(type))
         {
            RARCH_ERR("[slang]: Underlying type of texture"
                  " semantic is invalid.\n");
            return false;
         }

         if (!set_ubo_texture_offset(reflection, tex_sem, tex_sem_index,
                  ranges[i].offset, push_constant))
            return false;
      }
      else
      {
         RARCH_ERR("[slang]: Unknown semantic found.\n");
         return false;
      }
   }
   return true;
}

bool slang_reflect(
      const Compiler &vertex_compiler,
      const Compiler &fragment_compiler,
      const ShaderResources &vertex,
      const ShaderResources &fragment,
      slang_reflection *reflection)
{
   uint32_t location_mask = 0;
   uint32_t binding_mask  = 0;
   unsigned i             = 0;

   /* Validate use of unexpected types. */
   if (
         !vertex.sampled_images.empty()    ||
         !vertex.storage_buffers.empty()   ||
         !vertex.subpass_inputs.empty()    ||
         !vertex.storage_images.empty()    ||
         !vertex.atomic_counters.empty()   ||
         !fragment.storage_buffers.empty() ||
         !fragment.subpass_inputs.empty()  ||
         !fragment.storage_images.empty()  ||
         !fragment.atomic_counters.empty())
   {
      RARCH_ERR("[slang]: Invalid resource type detected.\n");
      return false;
   }

   /* Validate vertex input. */
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

   if (fragment_compiler.get_decoration(
            fragment.stage_outputs[0].id, spv::DecorationLocation) != 0)
   {
      RARCH_ERR("[slang]: Render target must use location = 0.\n");
      return false;
   }

   for (i = 0; i < vertex.stage_inputs.size(); i++)
      location_mask |= 1 << vertex_compiler.get_decoration(
            vertex.stage_inputs[i].id, spv::DecorationLocation);

   if (location_mask != 0x3)
   {
      RARCH_ERR("[slang]: The two vertex attributes do not"
            " use location = 0 and location = 1.\n");
      return false;
   }

   /* Validate the single uniform buffer. */
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

   /* Validate the single push constant buffer. */
   if (vertex.push_constant_buffers.size() > 1)
   {
      RARCH_ERR("[slang]: Vertex must use zero or one push constant buffers.\n");
      return false;
   }

   if (fragment.push_constant_buffers.size() > 1)
   {
      RARCH_ERR("[slang]: Fragment must use zero or one push cosntant buffer.\n");
      return false;
   }

   uint32_t vertex_ubo    = vertex.uniform_buffers.empty() ? 0 : vertex.uniform_buffers[0].id;
   uint32_t fragment_ubo  = fragment.uniform_buffers.empty() ? 0 : fragment.uniform_buffers[0].id;
   uint32_t vertex_push   = vertex.push_constant_buffers.empty() ? 0 : vertex.push_constant_buffers[0].id;
   uint32_t fragment_push = fragment.push_constant_buffers.empty() ? 0 : fragment.push_constant_buffers[0].id;

   if (vertex_ubo &&
         vertex_compiler.get_decoration(
            vertex_ubo, spv::DecorationDescriptorSet) != 0)
   {
      RARCH_ERR("[slang]: Resources must use descriptor set #0.\n");
      return false;
   }

   if (fragment_ubo &&
         fragment_compiler.get_decoration(
            fragment_ubo, spv::DecorationDescriptorSet) != 0)
   {
      RARCH_ERR("[slang]: Resources must use descriptor set #0.\n");
      return false;
   }

   unsigned vertex_ubo_binding   = vertex_ubo 
      ? vertex_compiler.get_decoration(vertex_ubo, spv::DecorationBinding) 
      : -1u;
   unsigned fragment_ubo_binding = fragment_ubo 
      ? fragment_compiler.get_decoration(fragment_ubo, spv::DecorationBinding) 
      : -1u;
   bool has_ubo                  = vertex_ubo || fragment_ubo;

   if (  (vertex_ubo_binding   != -1u) &&
         (fragment_ubo_binding != -1u) &&
         (vertex_ubo_binding   != fragment_ubo_binding))
   {
      RARCH_ERR("[slang]: Vertex and fragment uniform buffer must have same binding.\n");
      return false;
   }

   unsigned ubo_binding = (vertex_ubo_binding != -1u)
      ? vertex_ubo_binding 
      : fragment_ubo_binding;

   if (has_ubo && ubo_binding >= SLANG_NUM_BINDINGS)
   {
      RARCH_ERR("[slang]: Binding %u is out of range.\n", ubo_binding);
      return false;
   }

   reflection->ubo_binding              = has_ubo ? ubo_binding : 0;
   reflection->ubo_stage_mask           = 0;
   reflection->ubo_size                 = 0;
   reflection->push_constant_size       = 0;
   reflection->push_constant_stage_mask = 0;

   if (vertex_ubo)
   {
      reflection->ubo_stage_mask |= SLANG_STAGE_VERTEX_MASK;
      reflection->ubo_size        = max(reflection->ubo_size,
            vertex_compiler.get_declared_struct_size(
               vertex_compiler.get_type(
                  vertex.uniform_buffers[0].base_type_id)));
   }

   if (fragment_ubo)
   {
      reflection->ubo_stage_mask |= SLANG_STAGE_FRAGMENT_MASK;
      reflection->ubo_size        = max(reflection->ubo_size,
            fragment_compiler.get_declared_struct_size(
               fragment_compiler.get_type(
                  fragment.uniform_buffers[0].base_type_id)));
   }

   if (vertex_push)
   {
      reflection->push_constant_stage_mask |= SLANG_STAGE_VERTEX_MASK;
      reflection->push_constant_size        = max(
            reflection->push_constant_size,
            vertex_compiler.get_declared_struct_size(
               vertex_compiler.get_type(
                  vertex.push_constant_buffers[0].base_type_id)));
   }

   if (fragment_push)
   {
      reflection->push_constant_stage_mask |= SLANG_STAGE_FRAGMENT_MASK;
      reflection->push_constant_size        = max(
            reflection->push_constant_size,
            fragment_compiler.get_declared_struct_size(
               fragment_compiler.get_type(
                  fragment.push_constant_buffers[0].base_type_id)));
   }

   /* Validate push constant size against Vulkan's 
    * minimum spec to avoid cross-vendor issues. */
   if (reflection->push_constant_size > 128)
   {
      RARCH_ERR("[slang]: Exceeded maximum size of 128 bytes"
            " for push constant buffer.\n");
      return false;
   }

   /* Find all relevant uniforms and push constants. */
   if (vertex_ubo && !add_active_buffer_ranges(vertex_compiler,
            vertex.uniform_buffers[0], reflection, false))
      return false;
   if (fragment_ubo && !add_active_buffer_ranges(fragment_compiler,
            fragment.uniform_buffers[0], reflection, false))
      return false;
   if (vertex_push && !add_active_buffer_ranges(vertex_compiler,
            vertex.push_constant_buffers[0], reflection, true))
      return false;
   if (fragment_push && !add_active_buffer_ranges(fragment_compiler,
            fragment.push_constant_buffers[0], reflection, true))
      return false;

   if (has_ubo)
      binding_mask = 1 << ubo_binding;

   /* On to textures. */
   for (i = 0; i < fragment.sampled_images.size(); i++)
   {
      unsigned array_index = 0;
      unsigned set         = fragment_compiler.get_decoration(
            fragment.sampled_images[i].id,
            spv::DecorationDescriptorSet);
      unsigned binding     = fragment_compiler.get_decoration(
            fragment.sampled_images[i].id,
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

      slang_texture_semantic index = slang_name_to_texture_semantic(
            *reflection->texture_semantic_map,
            fragment.sampled_images[i].name, &array_index);

      if (index == SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT && array_index >= reflection->pass_number)
      {
         RARCH_ERR("[slang]: Non causal filter chain detected. "
               "Shader is trying to use output from pass #%u,"
               " but this shader is pass #%u.\n",
               array_index, reflection->pass_number);
         return false;
      }
      else if (index == SLANG_INVALID_TEXTURE_SEMANTIC)
      {
         RARCH_ERR("[slang]: Non-semantic textures not supported yet.\n");
         return false;
      }

      resize_minimum(reflection->semantic_textures[index], array_index + 1);
      slang_texture_semantic_meta &semantic = 
         reflection->semantic_textures[index][array_index];
      semantic.binding                      = binding;
      semantic.stage_mask                   = SLANG_STAGE_FRAGMENT_MASK;
      semantic.texture                      = true;
   }

   RARCH_LOG("[slang]: Reflection\n");
   RARCH_LOG("[slang]:   Textures:\n");

   for (i = 0; i < SLANG_NUM_TEXTURE_SEMANTICS; i++)
   {
      unsigned index = 0;
      for (auto &sem : reflection->semantic_textures[i])
      {
         if (sem.texture)
            RARCH_LOG("[slang]:      %s (#%u)\n",
                  texture_semantic_names[i], index);
         index++;
      }
   }

   RARCH_LOG("[slang]:\n");
   RARCH_LOG("[slang]:   Uniforms (Vertex: %s, Fragment: %s):\n",
         reflection->ubo_stage_mask & SLANG_STAGE_VERTEX_MASK ? "yes": "no",
         reflection->ubo_stage_mask & SLANG_STAGE_FRAGMENT_MASK ? "yes": "no");
   RARCH_LOG("[slang]:   Push Constants (Vertex: %s, Fragment: %s):\n",
         reflection->push_constant_stage_mask & SLANG_STAGE_VERTEX_MASK ? "yes": "no",
         reflection->push_constant_stage_mask & SLANG_STAGE_FRAGMENT_MASK ? "yes": "no");

   for (i = 0; i < SLANG_NUM_SEMANTICS; i++)
   {
      if (reflection->semantics[i].uniform)
      {
         RARCH_LOG("[slang]:      %s (Offset: %u)\n",
               semantic_uniform_names[i],
               unsigned(reflection->semantics[i].ubo_offset));
      }

      if (reflection->semantics[i].push_constant)
      {
         RARCH_LOG("[slang]:      %s (PushOffset: %u)\n",
               semantic_uniform_names[i],
               unsigned(reflection->semantics[i].push_constant_offset));
      }
   }

   for (i = 0; i < SLANG_NUM_TEXTURE_SEMANTICS; i++)
   {
      unsigned index = 0;
      for (auto &sem : reflection->semantic_textures[i])
      {
         if (sem.uniform)
         {
            RARCH_LOG("[slang]:      %s (#%u) (Offset: %u)\n",
                  texture_semantic_uniform_names[i],
                  index,
                  unsigned(sem.ubo_offset));
         }

         if (sem.push_constant)
         {
            RARCH_LOG("[slang]:      %s (#%u) (PushOffset: %u)\n",
                  texture_semantic_uniform_names[i],
                  index,
                  unsigned(sem.push_constant_offset));
         }
         index++;
      }
   }

   {
      char buf[64];
      buf[0] = '\0';
      snprintf(buf, sizeof(buf),
            "[slang]:\n%s [slang]:   Parameters:\n", FILE_PATH_LOG_INFO);
      RARCH_LOG(buf);
   }

   for (i = 0; i < reflection->semantic_float_parameters.size(); i++)
   {
      slang_semantic_meta *param = (slang_semantic_meta*)
         &reflection->semantic_float_parameters[i];

      if (!param)
         continue;

      if (param->uniform)
         RARCH_LOG("[slang]:     #%u (Offset: %u)\n", i,
               (unsigned int)param->ubo_offset);
      if (param->push_constant)
         RARCH_LOG("[slang]:     #%u (PushOffset: %u)\n", i,
               (unsigned int)param->push_constant_offset);
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
      spirv_cross::ShaderResources
         vertex_resources     = vertex_compiler.get_shader_resources();
      spirv_cross::ShaderResources 
         fragment_resources   = fragment_compiler.get_shader_resources();

      if (!slang_reflect(vertex_compiler, fragment_compiler,
               vertex_resources, fragment_resources,
               reflection))
      {
         RARCH_ERR("[slang]: Failed to reflect SPIR-V."
               " Resource usage is inconsistent with expectations.\n");
         return false;
      }

      return true;
   }
   catch (const std::exception &e)
   {
      RARCH_ERR("[slang]: SPIRV-Cross threw exception: %s.\n", e.what());
      return false;
   }
}

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

#include <fstream>
#include <iostream>
#include <spirv_glsl.hpp>
#include <spirv_hlsl.hpp>
#include <spirv_msl.hpp>
#include <compat/strl.h>
#include <string>
#include <stdint.h>
#include <algorithm>
#include <string/stdstring.h>

#include "glslang_util.h"
#include "slang_reflection.h"
#include "slang_reflection.hpp"
#include "slang_process.h"

#include "../../verbosity.h"

#ifdef HAVE_SPIRV_CROSS
using namespace spirv_cross;
#endif

template <typename M, typename S>
static const char *get_semantic_name(
      const std::unordered_map<std::string, M>* map,
      S semantic, unsigned index)
{
   for (const auto& m : *map)
   {
      if (m.second.semantic == semantic && m.second.index == index)
         return m.first.c_str();
   }
   return "";
}

static bool slang_process_reflection(
      const Compiler*        vs_compiler,
      const Compiler*        ps_compiler,
      const ShaderResources& vs_resources,
      const ShaderResources& ps_resources,
      video_shader*          shader_info,
      unsigned               pass_number,
      const semantics_map_t* map,
      pass_semantics_t*      out)
{
   int semantic;
   unsigned i;
   std::vector<texture_sem_t> textures;
   std::vector<uniform_sem_t> uniforms[SLANG_CBUFFER_MAX];
   std::unordered_map<std::string, slang_texture_semantic_map> texture_semantic_map;
   std::unordered_map<std::string, slang_texture_semantic_map> texture_semantic_uniform_map;

   for (i = 0; i <= pass_number; i++)
   {
      if (!*shader_info->pass[i].alias)
         continue;

      std::string name = shader_info->pass[i].alias;

      if (!slang_set_unique_map(
                texture_semantic_map, name,
                slang_texture_semantic_map{
                SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, i }))
         return false;

      if (!slang_set_unique_map(
                texture_semantic_uniform_map, name + "Size",
                slang_texture_semantic_map{
                SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, i }))
         return false;

      if (!slang_set_unique_map(
                texture_semantic_map, name + "Feedback",
                slang_texture_semantic_map{
                SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, i }))
         return false;

      if (!slang_set_unique_map(
                texture_semantic_uniform_map, name + "FeedbackSize",
                slang_texture_semantic_map{
                SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, i }))
         return false;
   }

   for (i = 0; i < shader_info->luts; i++)
   {
      if (!slang_set_unique_map(
                texture_semantic_map, shader_info->lut[i].id,
                slang_texture_semantic_map{
                SLANG_TEXTURE_SEMANTIC_USER, i }))
         return false;

      if (!slang_set_unique_map(
                texture_semantic_uniform_map,
                std::string(shader_info->lut[i].id) + "Size",
                slang_texture_semantic_map{
                SLANG_TEXTURE_SEMANTIC_USER, i }))
         return false;
   }

   std::unordered_map<std::string, slang_semantic_map> uniform_semantic_map;

   for (i = 0; i < shader_info->num_parameters; i++)
   {
      if (!slang_set_unique_map(
                uniform_semantic_map, shader_info->parameters[i].id,
                slang_semantic_map{ SLANG_SEMANTIC_FLOAT_PARAMETER, i }))
         return false;
   }

   slang_reflection sl_reflection;
   sl_reflection.pass_number                  = pass_number;
   sl_reflection.texture_semantic_map         = &texture_semantic_map;
   sl_reflection.texture_semantic_uniform_map = &texture_semantic_uniform_map;
   sl_reflection.semantic_map                 = &uniform_semantic_map;

   if (!slang_reflect(*vs_compiler, *ps_compiler,
            vs_resources, ps_resources, &sl_reflection))
   {
      RARCH_ERR("[slang]: Failed to reflect SPIR-V."
            " Resource usage is inconsistent with "
                "expectations.\n");
      return false;
   }

   out->cbuffers[SLANG_CBUFFER_UBO].stage_mask = sl_reflection.ubo_stage_mask;
   out->cbuffers[SLANG_CBUFFER_UBO].binding    = sl_reflection.ubo_binding;
   out->cbuffers[SLANG_CBUFFER_UBO].size       = (unsigned)((sl_reflection.ubo_size + 0xF) & ~0xF);
   out->cbuffers[SLANG_CBUFFER_PC].stage_mask  = sl_reflection.push_constant_stage_mask;
   out->cbuffers[SLANG_CBUFFER_PC].binding     = sl_reflection.ubo_binding ? 0 : 1;
   out->cbuffers[SLANG_CBUFFER_PC].size        = (unsigned)((sl_reflection.push_constant_size + 0xF) & ~0xF);

   for (semantic = 0; semantic < SLANG_NUM_BASE_SEMANTICS; semantic++)
   {
      slang_semantic_meta& src = sl_reflection.semantics[semantic];
      if (src.push_constant || src.uniform)
      {
         uniform_sem_t uniform = { map->uniforms[semantic],
            src.num_components
               * (unsigned)sizeof(float) };
         slang_semantic _semantic   = (slang_semantic)semantic;
         static const char* names[] = {
            "MVP",
            "OutputSize",
            "FinalViewportSize",
            "FrameCount",
            "FrameDirection",
            "Rotation",
            "TotalSubFrames",
            "CurrentSubFrame",
         };
         int size = sizeof(names) / sizeof(*names);
         if (semantic < size)
            strlcpy(uniform.id, names[_semantic], sizeof(uniform.id));
         else
            strlcpy(uniform.id, get_semantic_name(sl_reflection.semantic_map, _semantic, 0), sizeof(uniform.id));

         if (src.push_constant)
         {
            uniform.offset = (unsigned)src.push_constant_offset;
            uniforms[SLANG_CBUFFER_PC].push_back(uniform);
         }
         else
         {
            uniform.offset = (unsigned)src.ubo_offset;
            uniforms[SLANG_CBUFFER_UBO].push_back(uniform);
         }
      }
   }

   for (i = 0; i < sl_reflection.semantic_float_parameters.size(); i++)
   {
      slang_semantic_meta& src = sl_reflection.semantic_float_parameters[i];

      if (src.push_constant || src.uniform)
      {
         uniform_sem_t uniform = {
            &shader_info->parameters[i].current, sizeof(float) };
         strlcpy(uniform.id, get_semantic_name(sl_reflection.semantic_map, SLANG_SEMANTIC_FLOAT_PARAMETER, i), sizeof(uniform.id));

         if (src.push_constant)
         {
            uniform.offset = (unsigned)src.push_constant_offset;
            uniforms[SLANG_CBUFFER_PC].push_back(uniform);
         }
         else
         {
            uniform.offset = (unsigned)src.ubo_offset;
            uniforms[SLANG_CBUFFER_UBO].push_back(uniform);
         }
      }
   }

   for (semantic = 0; semantic < SLANG_NUM_TEXTURE_SEMANTICS; semantic++)
   {
      unsigned index;

      for (index = 0; index <
            sl_reflection.semantic_textures[semantic].size(); index++)
      {
         slang_texture_semantic_meta& src =
            sl_reflection.semantic_textures[semantic][index];

         if (src.stage_mask)
         {
            static const char* names[] = {
               "Original", "Source", "OriginalHistory", "PassOutput", "PassFeedback",
            };
            int size;
            texture_sem_t texture;
            slang_texture_semantic
               _semantic              = (slang_texture_semantic)semantic;
            texture.id[0]             = '\0';
            if (_semantic < (int)SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY)
               strlcpy(texture.id, names[semantic], sizeof(texture.id));
            else
            {
               size = sizeof(names) / sizeof(*names);
               if (semantic < size)
                  snprintf(texture.id, sizeof(texture.id), "%s%d", names[_semantic], index);
               else
                  strlcpy(texture.id, get_semantic_name(sl_reflection.texture_semantic_map, _semantic, index), sizeof(texture.id));
            }
            texture.texture_data =
               (void*)((uintptr_t)map->textures[semantic].image + index * map->textures[semantic].image_stride);

            if (semantic == SLANG_TEXTURE_SEMANTIC_USER)
            {
               texture.wrap    = shader_info->lut[index].wrap;
               texture.filter  = shader_info->lut[index].filter;
            }
            else
            {
               texture.wrap    = shader_info->pass[pass_number].wrap;
               texture.filter  = shader_info->pass[pass_number].filter;
            }
            texture.stage_mask = src.stage_mask;
            texture.binding    = src.binding;

            textures.push_back(texture);

            if (semantic == SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK)
               shader_info->pass[index].feedback = true;

            if (semantic == SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY &&
                (unsigned)shader_info->history_size < index)
               shader_info->history_size = index;
         }

         if (src.push_constant || src.uniform)
         {
            uniform_sem_t uniform = {
               (void*)((uintptr_t)map->textures[semantic].size
                     + index * map->textures[semantic].size_stride),
               4 * sizeof(float)
            };
            slang_texture_semantic _semantic = (slang_texture_semantic)semantic;
            static const char* names[] = {
               "OriginalSize", "SourceSize", "OriginalHistorySize", "PassOutputSize", "PassFeedbackSize",
            };
            if (semantic < (int)SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY)
               strlcpy(uniform.id, names[_semantic], sizeof(uniform.id));
            else
            {
               int size = sizeof(names) / sizeof(*names);
               if (semantic < size)
                  snprintf(uniform.id, sizeof(uniform.id), "%s%d", names[_semantic], index);
               else
                  strlcpy(uniform.id, get_semantic_name(sl_reflection.texture_semantic_uniform_map, _semantic, index), sizeof(uniform.id));
            }

            if (src.push_constant)
            {
               uniform.offset = (unsigned)src.push_constant_offset;
               uniforms[SLANG_CBUFFER_PC].push_back(uniform);
            }
            else
            {
               uniform.offset = (unsigned)src.ubo_offset;
               uniforms[SLANG_CBUFFER_UBO].push_back(uniform);
            }
         }
      }
   }

   out->texture_count = (int)textures.size();

   textures.push_back({ NULL });
   out->textures = (texture_sem_t*)
      malloc(textures.size() * sizeof(*textures.data()));
   memcpy(out->textures, textures.data(),
         textures.size() * sizeof(*textures.data()));

   for (i = 0; i < SLANG_CBUFFER_MAX; i++)
   {
      if (uniforms[i].empty())
         continue;

      out->cbuffers[i].uniform_count = (int)uniforms[i].size();

      uniforms[i].push_back({ NULL });
      out->cbuffers[i].uniforms =
            (uniform_sem_t*)
            malloc(uniforms[i].size() * sizeof(*uniforms[i].data()));

      memcpy(
            out->cbuffers[i].uniforms, uniforms[i].data(),
            uniforms[i].size() * sizeof(*uniforms[i].data()));
   }

   return true;
}

bool slang_preprocess_parse_parameters(glslang_meta& meta,
      struct video_shader *shader)
{
   unsigned i;
   unsigned old_num_parameters = shader->num_parameters;

   /* Assumes num_parameters is
    * initialized to something sane. */
   for (i = 0; i < meta.parameters.size(); i++)
   {
      struct video_shader_parameter *p = NULL;
      bool mismatch_dup                = false;
      auto itr                         = std::find_if(shader->parameters,
            shader->parameters + shader->num_parameters,
            [&](const video_shader_parameter &parsed_param)
            {
            return meta.parameters[i].id == parsed_param.id;
            });

      if (itr != shader->parameters + shader->num_parameters)
      {
         /* Allow duplicate #pragma parameter, but only
          * if they are exactly the same. */
         if (  meta.parameters[i].desc    != itr->desc    ||
               meta.parameters[i].initial != itr->initial ||
               meta.parameters[i].minimum != itr->minimum ||
               meta.parameters[i].maximum != itr->maximum ||
               meta.parameters[i].step    != itr->step)
         {
            RARCH_ERR("[slang]: Duplicate parameters"
                  " found for \"%s\", but arguments do not match.\n",
                  itr->id);
            mismatch_dup = true;
         }
         else
            continue;
      }

      if (mismatch_dup || shader->num_parameters == GFX_MAX_PARAMETERS)
      {
         shader->num_parameters = old_num_parameters;
         return false;
      }

      if (!(p = (struct video_shader_parameter*)
         &shader->parameters[shader->num_parameters++]))
         continue;

      strlcpy(p->id,   meta.parameters[i].id.c_str(),   sizeof(p->id));
      strlcpy(p->desc, meta.parameters[i].desc.c_str(), sizeof(p->desc));
      p->initial = meta.parameters[i].initial;
      p->minimum = meta.parameters[i].minimum;
      p->maximum = meta.parameters[i].maximum;
      p->step    = meta.parameters[i].step;
      p->current = meta.parameters[i].initial;
   }

   return true;
}

bool slang_preprocess_parse_parameters(const char *shader_path,
      struct video_shader *shader)
{
   glslang_meta meta;
   struct string_list lines = {0};

   if (!string_list_initialize(&lines))
      goto error;

   if (!glslang_read_shader_file(shader_path, &lines, true))
      goto error;
   meta = glslang_meta{};
   if (!glslang_parse_meta(&lines, &meta))
      goto error;

   string_list_deinitialize(&lines);
   return slang_preprocess_parse_parameters(meta, shader);

error:
   string_list_deinitialize(&lines);
   return false;
}

bool slang_process(
      video_shader*          shader_info,
      unsigned               pass_number,
      enum rarch_shader_type dst_type,
      unsigned               version,
      const semantics_map_t* semantics_map,
      pass_semantics_t*      out)
{
   glslang_output     output;
   Compiler*          vs_compiler = NULL;
   Compiler*          ps_compiler = NULL;
   video_shader_pass& pass        = shader_info->pass[pass_number];

   if (!glslang_compile_shader(pass.source.path, &output))
      return false;

   if (!slang_preprocess_parse_parameters(output.meta, shader_info))
      return false;

   if (!*pass.alias && !output.meta.name.empty())
      strlcpy(pass.alias, output.meta.name.c_str(), sizeof(pass.alias) - 1);

   out->format = output.meta.rt_format;

   if (out->format == SLANG_FORMAT_UNKNOWN)
   {
      if (pass.fbo.flags & FBO_SCALE_FLAG_SRGB_FBO)
         out->format = SLANG_FORMAT_R8G8B8A8_SRGB;
      else if (pass.fbo.flags & FBO_SCALE_FLAG_FP_FBO)
         out->format = SLANG_FORMAT_R16G16B16A16_SFLOAT;
      else
         out->format = SLANG_FORMAT_R8G8B8A8_UNORM;
   }

   pass.source.string.vertex   = NULL;
   pass.source.string.fragment = NULL;

   try
   {
      ShaderResources vs_resources;
      ShaderResources ps_resources;
      std::string     vs_code;
      std::string     ps_code;

      switch (dst_type)
      {
         case RARCH_SHADER_HLSL:
         case RARCH_SHADER_CG:
#ifdef HAVE_HLSL
            vs_compiler = new CompilerHLSL(output.vertex);
            ps_compiler = new CompilerHLSL(output.fragment);
#endif
            break;

         case RARCH_SHADER_METAL:
            vs_compiler = new CompilerMSL(output.vertex);
            ps_compiler = new CompilerMSL(output.fragment);
            break;

         default:
            vs_compiler = new CompilerGLSL(output.vertex);
            ps_compiler = new CompilerGLSL(output.fragment);
            break;
      }

      if (vs_compiler)
         vs_resources   = vs_compiler->get_shader_resources();
      if (ps_compiler)
         ps_resources   = ps_compiler->get_shader_resources();

      if (!vs_resources.uniform_buffers.empty())
         vs_compiler->set_decoration(
               vs_resources.uniform_buffers[0].id, spv::DecorationBinding, 0);
      if (!ps_resources.uniform_buffers.empty())
         ps_compiler->set_decoration(
               ps_resources.uniform_buffers[0].id, spv::DecorationBinding, 0);

      if (!vs_resources.push_constant_buffers.empty())
         vs_compiler->set_decoration(
               vs_resources.push_constant_buffers[0].id, spv::DecorationBinding, 1);
      if (!ps_resources.push_constant_buffers.empty())
         ps_compiler->set_decoration(
               ps_resources.push_constant_buffers[0].id, spv::DecorationBinding, 1);

      switch (dst_type)
      {
         case RARCH_SHADER_HLSL:
         case RARCH_SHADER_CG:
#ifdef HAVE_HLSL
            {
               CompilerHLSL::Options options;
               CompilerHLSL*         vs = (CompilerHLSL*)vs_compiler;
               CompilerHLSL*         ps = (CompilerHLSL*)ps_compiler;
               options.shader_model     = version;
               vs->set_hlsl_options(options);
               ps->set_hlsl_options(options);
               vs_code = vs->compile();
               ps_code = ps->compile();
            }
#endif
            break;
         case RARCH_SHADER_METAL:
            {
               CompilerMSL::Options options;
               CompilerMSL*         vs = (CompilerMSL*)vs_compiler;
               CompilerMSL*         ps = (CompilerMSL*)ps_compiler;
               options.msl_version     = version;
               vs->set_msl_options(options);
               ps->set_msl_options(options);

               const auto remap_push_constant = [](CompilerMSL *comp,
                     const ShaderResources &resources) {
                  for (const Resource& resource : resources.push_constant_buffers)
                  {
                     /* Explicit 1:1 mapping for bindings. */
                     MSLResourceBinding binding = {};
                     binding.stage              = comp->get_execution_model();
                     binding.desc_set           = kPushConstDescSet;
                     binding.binding            = kPushConstBinding;
                     /* Use earlier decoration override. */
                     binding.msl_buffer         = comp->get_decoration(
                           resource.id, spv::DecorationBinding);
                     comp->add_msl_resource_binding(binding);
                  }
               };

               const auto remap_generic_resource = [](CompilerMSL *comp,
                     const SmallVector<Resource> &resources) {
                  for (const Resource& resource : resources)
                  {
                     /* Explicit 1:1 mapping for bindings. */
                     MSLResourceBinding binding = {};
                     binding.stage              = comp->get_execution_model();
                     binding.desc_set           = comp->get_decoration(
                           resource.id, spv::DecorationDescriptorSet);

                     /* Use existing decoration override. */
                     uint32_t msl_binding       = comp->get_decoration(
                           resource.id, spv::DecorationBinding);
                     binding.binding            = msl_binding;
                     binding.msl_buffer         = msl_binding;
                     binding.msl_texture        = msl_binding;
                     binding.msl_sampler        = msl_binding;
                     comp->add_msl_resource_binding(binding);
                  }
               };

               remap_push_constant(vs, vs_resources);
               remap_push_constant(ps, ps_resources);
               remap_generic_resource(vs, vs_resources.uniform_buffers);
               remap_generic_resource(ps, ps_resources.uniform_buffers);
               remap_generic_resource(vs, vs_resources.sampled_images);
               remap_generic_resource(ps, ps_resources.sampled_images);

               vs_code = vs->compile();
               ps_code = ps->compile();
            }
            break;
         case RARCH_SHADER_GLSL:
            {
               CompilerGLSL::Options options;
               CompilerGLSL*         vs = (CompilerGLSL*)vs_compiler;
               CompilerGLSL*         ps = (CompilerGLSL*)ps_compiler;
               options.version          = version;
               ps->set_common_options(options);
               vs->set_common_options(options);

               vs_code = vs->compile();
               ps_code = ps->compile();
            }
            break;
         default:
            goto error;
      }

      pass.source.string.vertex   = strdup(vs_code.c_str());
      pass.source.string.fragment = strdup(ps_code.c_str());

      if (!slang_process_reflection(
                vs_compiler, ps_compiler,
                vs_resources, ps_resources, shader_info, pass_number,
                semantics_map, out))
         goto error;

   }
   catch (const std::exception& e)
   {
      RARCH_ERR("[slang]: SPIRV-Cross threw exception: %s.\n", e.what());
      goto error;
   }

   delete vs_compiler;
   delete ps_compiler;

   return true;

error:
   free(pass.source.string.vertex);
   free(pass.source.string.fragment);

   pass.source.string.vertex   = NULL;
   pass.source.string.fragment = NULL;

   delete vs_compiler;
   delete ps_compiler;

   return false;
}

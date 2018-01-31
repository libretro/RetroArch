
#include <fstream>
#include <iostream>
#include <spirv_hlsl.hpp>
#include <stdint.h>

#include "verbosity.h"
#include "glslang_util.hpp"
#include "slang_preprocess.h"
#include "slang_reflection.h"
#include "slang_process.h"

using namespace spirv_cross;
using namespace std;

template <typename P>
static bool set_unique_map(unordered_map<string, P>& m, const string& name, const P& p)
{
   auto itr = m.find(name);
   if (itr != end(m))
   {
      RARCH_ERR("[slang]: Alias \"%s\" already exists.\n", name.c_str());
      return false;
   }

   m[name] = p;
   return true;
}

template <typename M, typename S>
static string get_semantic_name(const unordered_map<string, M>* map, S semantic, unsigned index)
{
   for (const pair<string, M>& m : *map)
   {
      if (m.second.semantic == semantic && m.second.index == index)
         return m.first;
   }
   return string();
}

static string
get_semantic_name(slang_reflection& reflection, slang_semantic semantic, unsigned index)
{
   static const char* names[] = {
      "MVP",
      "OutputSize",
      "FinalViewportSize",
      "FrameCount",
   };
   if ((int)semantic < sizeof(names) / sizeof(*names))
      return std::string(names[semantic]);

   return get_semantic_name(reflection.semantic_map, semantic, index);
}

static string
get_semantic_name(slang_reflection& reflection, slang_texture_semantic semantic, unsigned index)
{
   static const char* names[] = {
      "Original", "Source", "OriginalHistory", "PassOutput", "PassFeedback",
   };
   if ((int)semantic < (int)SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY)
      return std::string(names[semantic]);
   else if ((int)semantic < sizeof(names) / sizeof(*names))
      return std::string(names[semantic]) + to_string(index);

   return get_semantic_name(reflection.texture_semantic_map, semantic, index);
}

static string get_size_semantic_name(
      slang_reflection& reflection, slang_texture_semantic semantic, unsigned index)
{
   static const char* names[] = {
      "OriginalSize", "SourceSize", "OriginalHistorySize", "PassOutputSize", "PassFeedbackSize",
   };
   if ((int)semantic < (int)SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY)
      return std::string(names[semantic]);
   if ((int)semantic < sizeof(names) / sizeof(*names))
      return std::string(names[semantic]) + to_string(index);

   return get_semantic_name(reflection.texture_semantic_uniform_map, semantic, index);
}

static bool slang_process_reflection(
      const Compiler*        vs_compiler,
      const Compiler*        ps_compiler,
      const ShaderResources& vs_resources,
      const ShaderResources& ps_resources,
      video_shader*          shader_info,
      unsigned               pass_number,
      const semantics_map_t* semantics_map,
      pass_semantics_t*      out)
{
   unordered_map<string, slang_texture_semantic_map> texture_semantic_map;
   unordered_map<string, slang_texture_semantic_map> texture_semantic_uniform_map;

   string name = shader_info->pass[pass_number].alias;

   if (!set_unique_map(
             texture_semantic_map, name,
             slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, pass_number }))
      return false;

   if (!set_unique_map(
             texture_semantic_uniform_map, name + "Size",
             slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, pass_number }))
      return false;

   if (!set_unique_map(
             texture_semantic_map, name + "Feedback",
             slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, pass_number }))
      return false;

   if (!set_unique_map(
             texture_semantic_uniform_map, name + "FeedbackSize",
             slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, pass_number }))
      return false;

   for (unsigned i = 0; i < shader_info->luts; i++)
   {
      if (!set_unique_map(
                texture_semantic_map, shader_info->lut[i].id,
                slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_USER, i }))
         return false;

      if (!set_unique_map(
                texture_semantic_uniform_map, string(shader_info->lut[i].id) + "Size",
                slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_USER, i }))
         return false;
   }

   unordered_map<string, slang_semantic_map> uniform_semantic_map;
   for (unsigned i = 0; i < shader_info->num_parameters; i++)
   {
      if (!set_unique_map(
                uniform_semantic_map, shader_info->parameters[i].id,
                { SLANG_SEMANTIC_FLOAT_PARAMETER, i }))
         return false;
   }

   slang_reflection sl_reflection;
   sl_reflection.pass_number                  = pass_number;
   sl_reflection.texture_semantic_map         = &texture_semantic_map;
   sl_reflection.texture_semantic_uniform_map = &texture_semantic_uniform_map;
   sl_reflection.semantic_map                 = &uniform_semantic_map;

   if (!slang_reflect(*vs_compiler, *ps_compiler, vs_resources, ps_resources, &sl_reflection))
   {
      RARCH_ERR("[slang]: Failed to reflect SPIR-V. Resource usage is inconsistent with "
                "expectations.\n");
      return false;
   }

   memset(out, 0x00, sizeof(*out));

   out->cbuffers[SLANG_CBUFFER_UBO].stage_mask = sl_reflection.ubo_stage_mask;
   out->cbuffers[SLANG_CBUFFER_UBO].binding    = sl_reflection.ubo_binding;
   out->cbuffers[SLANG_CBUFFER_UBO].size       = (sl_reflection.ubo_size + 0xF) & ~0xF;
   out->cbuffers[SLANG_CBUFFER_PC].stage_mask  = sl_reflection.push_constant_stage_mask;
   out->cbuffers[SLANG_CBUFFER_PC].binding     = sl_reflection.ubo_binding ? 0 : 1;
   out->cbuffers[SLANG_CBUFFER_PC].size        = (sl_reflection.push_constant_size + 0xF) & ~0xF;

   vector<uniform_sem_t> uniforms[SLANG_CBUFFER_MAX];
   vector<texture_sem_t> textures;

   uniform_map_t* uniform_map = semantics_map->uniform_map;
   while (uniform_map->data)
   {
      slang_semantic_meta& src     = sl_reflection.semantics[uniform_map->semantic];
      uniform_sem_t        uniform = { uniform_map->data, uniform_map->id,
                                src.num_components * (unsigned)sizeof(float) };

      string uniform_id = get_semantic_name(sl_reflection, uniform_map->semantic, 0);
      strncpy(uniform.id, uniform_id.c_str(), sizeof(uniform.id));

      if (src.push_constant)
      {
         uniform.offset = src.push_constant_offset;
         uniforms[SLANG_CBUFFER_PC].push_back(uniform);
      }
      else if (src.uniform)
      {
         uniform.offset = src.ubo_offset;
         uniforms[SLANG_CBUFFER_UBO].push_back(uniform);
      }
      uniform_map++;
   }

   /* TODO: this is emitting more uniforms than actally needed for this pass */
   for (int i = 0; i < sl_reflection.semantic_float_parameters.size(); i++)
   {
      slang_semantic_meta& src     = sl_reflection.semantic_float_parameters[i];
      uniform_sem_t        uniform = { &shader_info->parameters[i].current,
                                "shader_info->parameter[i].current", sizeof(float) };

      string uniform_id = get_semantic_name(sl_reflection, SLANG_SEMANTIC_FLOAT_PARAMETER, i);
      strncpy(uniform.id, uniform_id.c_str(), sizeof(uniform.id));

      if (src.push_constant)
      {
         uniform.offset = src.push_constant_offset;
         uniforms[SLANG_CBUFFER_PC].push_back(uniform);
      }
      else if (src.uniform)
      {
         uniform.offset = src.ubo_offset;
         uniforms[SLANG_CBUFFER_UBO].push_back(uniform);
      }
   }

   out->min_binding           = SLANG_NUM_BINDINGS;
   texture_map_t* texture_map = semantics_map->texture_map;

   while (texture_map->texture_data)
   {
      if (texture_map->index < sl_reflection.semantic_textures[texture_map->semantic].size())
      {
         slang_texture_semantic_meta& src =
               sl_reflection.semantic_textures[texture_map->semantic][texture_map->index];

         if (src.stage_mask)
         {
            texture_sem_t texture = { texture_map->texture_data, texture_map->texture_id,
                                      texture_map->sampler_data, texture_map->sampler_id };
            texture.stage_mask    = src.stage_mask;
            texture.binding       = src.binding;
            string id = get_semantic_name(sl_reflection, texture_map->semantic, texture_map->index);

            strncpy(texture.id, id.c_str(), sizeof(texture.id));

            if (out->max_binding < src.binding)
               out->max_binding = src.binding;

            if (out->min_binding > src.binding)
               out->min_binding = src.binding;

            textures.push_back(texture);

            uniform_sem_t uniform = { texture_map->size_data, texture_map->size_id,
                                      4 * sizeof(float) };

            string uniform_id =
                  get_size_semantic_name(sl_reflection, texture_map->semantic, texture_map->index);

            strncpy(uniform.id, uniform_id.c_str(), sizeof(uniform.id));

            if (src.push_constant)
            {
               uniform.offset = src.push_constant_offset;
               uniforms[SLANG_CBUFFER_PC].push_back(uniform);
            }
            else if (src.uniform)
            {
               uniform.offset = src.ubo_offset;
               uniforms[SLANG_CBUFFER_UBO].push_back(uniform);
            }
         }
      }
      texture_map++;
   }

   if (out->min_binding > out->max_binding)
      out->min_binding = out->max_binding;

   out->texture_count = textures.size();

   textures.push_back({ NULL });
   out->textures = (texture_sem_t*)malloc(textures.size() * sizeof(*textures.data()));
   memcpy(out->textures, textures.data(), textures.size() * sizeof(*textures.data()));

   for (int i = 0; i < SLANG_CBUFFER_MAX; i++)
   {
      if (uniforms[i].empty())
         continue;

      out->cbuffers[i].uniform_count = uniforms[i].size();

      uniforms[i].push_back({ NULL });
      out->cbuffers[i].uniforms =
            (uniform_sem_t*)malloc(uniforms[i].size() * sizeof(*uniforms[i].data()));
      memcpy(
            out->cbuffers[i].uniforms, uniforms[i].data(),
            uniforms[i].size() * sizeof(*uniforms[i].data()));
   }

   return true;
}

bool slang_process(
      video_shader*          shader_info,
      unsigned               pass_number,
      enum rarch_shader_type dst_type,
      unsigned               version,
      const semantics_map_t* semantics_map,
      pass_semantics_t*      out)
{
   Compiler*          vs_compiler = NULL;
   Compiler*          ps_compiler = NULL;
   video_shader_pass& pass        = shader_info->pass[pass_number];
   glslang_output     output;

   if (!glslang_compile_shader(pass.source.path, &output))
      return false;

   if (!slang_preprocess_parse_parameters(output.meta, shader_info))
      return false;

   pass.source.string.vertex   = NULL;
   pass.source.string.fragment = NULL;

   try
   {
      ShaderResources vs_resources;
      ShaderResources ps_resources;
      string          vs_code;
      string          ps_code;

      if (dst_type == RARCH_SHADER_HLSL || dst_type == RARCH_SHADER_CG)
      {
         vs_compiler = new CompilerHLSL(output.vertex);
         ps_compiler = new CompilerHLSL(output.fragment);
      }
      else
      {
         vs_compiler = new CompilerGLSL(output.vertex);
         ps_compiler = new CompilerGLSL(output.fragment);
      }

      vs_resources = vs_compiler->get_shader_resources();
      ps_resources = ps_compiler->get_shader_resources();

      if (dst_type == RARCH_SHADER_HLSL || dst_type == RARCH_SHADER_CG)
      {
         CompilerHLSL::Options options;
         CompilerHLSL*         vs = (CompilerHLSL*)vs_compiler;
         CompilerHLSL*         ps = (CompilerHLSL*)ps_compiler;
         options.shader_model     = version;
         vs->set_options(options);
         ps->set_options(options);

         std::vector<HLSLVertexAttributeRemap> vs_attrib_remap;
#if 0
         /* remaps vertex shader output too so disable for now */
         vs_attrib_remap.push_back({ 0, "POSITION" });
         vs_attrib_remap.push_back({ 1, "TEXCOORD" });
#endif
         /* not exactly a vertex attribute but this remaps
          * float2 FragCoord :TEXCOORD# to float4 FragCoord : SV_POSITION */
         std::vector<HLSLVertexAttributeRemap> ps_attrib_remap;
         for (Resource& resource : ps_resources.stage_inputs)
         {
            if (ps->get_name(resource.id) == "FragCoord")
            {
               uint32_t location = ps->get_decoration(resource.id, spv::DecorationLocation);
               ps_attrib_remap.push_back({ location, "SV_Position" });
            }
         }
         VariableTypeRemapCallback ps_var_remap_cb =
               [](const SPIRType& type, const std::string& var_name, std::string& name_of_type) {
                  if (var_name == "FragCoord")
                     name_of_type = "float4";
               };
         ps->set_variable_type_remap_callback(ps_var_remap_cb);

         vs_code = vs->compile(vs_attrib_remap);
         ps_code = ps->compile(ps_attrib_remap);
      }
      else if (shader_info->type == RARCH_SHADER_GLSL)
      {
         CompilerGLSL::Options options;
         CompilerGLSL*         vs = (CompilerGLSL*)vs_compiler;
         CompilerGLSL*         ps = (CompilerGLSL*)ps_compiler;
         options.version          = version;
         ps->set_options(options);
         vs->set_options(options);

         vs_code = vs->compile();
         ps_code = ps->compile();
      }
      else
         goto error;

      pass.source.string.vertex = (char*)malloc(vs_code.size() + 1);
      memcpy(pass.source.string.vertex, vs_code.c_str(), vs_code.size() + 1);

      pass.source.string.fragment = (char*)malloc(ps_code.size() + 1);
      memcpy(pass.source.string.fragment, ps_code.c_str(), ps_code.size() + 1);

      if (!slang_process_reflection(
                vs_compiler, ps_compiler, vs_resources, ps_resources, shader_info, pass_number,
                semantics_map, out))
         goto error;

   } catch (const std::exception& e)
   {
      RARCH_ERR("[slang]: spir2cross threw exception: %s.\n", e.what());
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

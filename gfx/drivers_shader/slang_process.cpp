/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2017 - Hans-Kristian Arntzen
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

#include <spirv_glsl.hpp>
#include <spirv_hlsl.hpp>
#include <spirv_msl.hpp>
#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include <string>
#include <stdint.h>
#include <vector>

#include "glslang_util.h"
#if defined(HAVE_GLSLANG)
#include "glslang.hpp"
#endif
#include "spirv_cross.hpp"
#include "slang_process.h"

#include "../../verbosity.h"

static const char *texture_semantic_names[] = {
   "Original",
   "Source",
   "OriginalHistory",
   "PassOutput",
   "PassFeedback",
   "User",
   NULL
};

static const char *texture_semantic_uniform_names[] = {
   "OriginalSize",
   "SourceSize",
   "OriginalHistorySize",
   "PassOutputSize",
   "PassFeedbackSize",
   "UserSize",
   NULL
};

static const char *semantic_uniform_names[] = {
   "MVP",
   "OutputSize",
   "FinalViewportSize",
   "FrameCount",
   "FrameDirection",
   "FrameTimeDelta",
   "OriginalFPS",
   "Rotation",
   "OriginalAspect",
   "OriginalAspectRotated",
   "TotalSubFrames",
   "CurrentSubFrame",
   "HDRMode",
   "BrightnessNits",
   "Scanlines",
   "SubpixelLayout",
   "ExpandGamut",
   "InverseTonemap",
   "HDR10",
   "Gyroscope",
   "Accelerometer",
   "AccelerometerRest"
};

template <typename M, typename S>
static const char *get_semantic_name(
      const std::unordered_map<std::string, M>* map,
      S semantic, unsigned index)
{
   for (typename std::unordered_map<std::string, M>::const_iterator it = map->begin();
         it != map->end(); ++it)
   {
      if (it->second.semantic == semantic && it->second.index == index)
         return it->first.c_str();
   }
   return "";
}

static slang_texture_semantic slang_name_to_texture_semantic(
      const std::unordered_map<std::string, slang_texture_semantic_map> &semantic_map,
      const std::string &name, unsigned *index)
{
   std::unordered_map<std::string, slang_texture_semantic_map>::const_iterator itr =
      semantic_map.find(name);
   if (itr != semantic_map.end())
   {
      *index = itr->second.index;
      return itr->second.semantic;
   }

   return slang_name_to_texture_semantic_array(
         name.c_str(), texture_semantic_names, index);
}

static slang_texture_semantic slang_uniform_name_to_texture_semantic(
      const std::unordered_map<std::string, slang_texture_semantic_map> &semantic_map,
      const std::string &name, unsigned *index)
{
   std::unordered_map<std::string, slang_texture_semantic_map>::const_iterator itr =
      semantic_map.find(name);
   if (itr != semantic_map.end())
   {
      *index = itr->second.index;
      return itr->second.semantic;
   }

   return slang_name_to_texture_semantic_array(name.c_str(),
         texture_semantic_uniform_names, index);
}

static slang_semantic slang_uniform_name_to_semantic(
      const std::unordered_map<std::string, slang_semantic_map> &semantic_map,
      const std::string &name, unsigned *index)
{
   unsigned i = 0;
   std::unordered_map<std::string, slang_semantic_map>::const_iterator itr =
      semantic_map.find(name);

   if (itr != semantic_map.end())
   {
      *index = itr->second.index;
      return itr->second.semantic;
   }

   /* No builtin semantics are arrayed. */
   *index = 0;
   for (i = 0; i < sizeof(semantic_uniform_names) / sizeof(semantic_uniform_names[0]); i++)
   {
      if (name == semantic_uniform_names[i])
         return static_cast<slang_semantic>(i);
   }

   return SLANG_INVALID_SEMANTIC;
}

template <typename T>
static void resize_minimum(T &vec, unsigned minimum)
{
   if (vec.size() < minimum)
      vec.resize(minimum);
}


static bool slang_process_reflection(
      const spirv_cross::Compiler*        vs_compiler,
      const spirv_cross::Compiler*        ps_compiler,
      const spirv_cross::ShaderResources& vs_resources,
      const spirv_cross::ShaderResources& ps_resources,
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
      RARCH_ERR("[Slang] Failed to reflect SPIR-V."
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
            "FrameTimeDelta",
            "OriginalFPS",
            "Rotation",
            "OriginalAspect",
            "OriginalAspectRotated",
            "TotalSubFrames",
            "CurrentSubFrame",
            "HDRMode",
            "BrightnessNits",
            "Scanlines",
            "SubpixelLayout",
            "ExpandGamut",
            "InverseTonemap",
            "HDR10",
            /* Sensor uniforms: populated by GL3, GLSL, and Vulkan
             * backends only. D3D/Metal/GX2 backends will see zero
             * values (no hardware sensor support on those platforms). */
            "Gyroscope",
            "Accelerometer",
            "AccelerometerRest",
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
               {
                  size_t _len = strlcpy(texture.id, names[_semantic], sizeof(texture.id));
                  snprintf(texture.id + _len, sizeof(texture.id) - _len, "%d", index);
               }
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
               {
                  size_t _len = strlcpy(uniform.id, names[_semantic], sizeof(uniform.id));
                  snprintf(uniform.id + _len, sizeof(uniform.id) - _len, "%d", index);
               }
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

static std::string build_stage_source(
      const struct shader_line_buf *lines, const char *stage)
{
   size_t i;
   std::string str;
   bool active = true;
   if (!lines || lines->num_lines < 1)
      return "";
   str.reserve(lines->len);
   /* Version header (line 0). */
   str.append(shader_line_buf_get(lines, 0));
   str.append("\n");
   for (i = 1; i < lines->num_lines; i++)
   {
      const char *line = shader_line_buf_get(lines, i);
      if (!memcmp(line, "#pragma", sizeof("#pragma")-1))
      {
         if (!memcmp(line, "#pragma stage ", sizeof("#pragma stage ")-1))
         {
            if (stage && *stage)
            {
               char expected[128];
               size_t _len = strlcpy(expected, "#pragma stage ", sizeof(expected));
               strlcpy(expected + _len, stage, sizeof(expected) - _len);
               active = !strcmp(expected, line);
            }
         }
         else if (
                  !memcmp(line, "#pragma name ", sizeof("#pragma name ")-1)
               || !memcmp(line, "#pragma format ", sizeof("#pragma format ")-1))
         {
            /* Ignore */
         }
         else if (active)
         {
            str.append(line);
            str.append("\n");
         }
      }
      else if (active)
      {
         str.append(line);
         str.append("\n");
      }
   }
   return str;
}

static bool glslang_parse_meta(const struct shader_line_buf *lines,
      glslang_meta *meta)
{
   char id[64];
   char desc[64];
   size_t i;

   id[0]   = '\0';
   desc[0] = '\0';

   /* Pre-count parameters to avoid vector reallocation */
   {
      size_t param_count = 0;
      for (i = 0; i < lines->num_lines; i++)
      {
         const char *line = shader_line_buf_get(lines, i);
         if (line && !memcmp(line, "#pragma parameter ",
                  sizeof("#pragma parameter ") - 1))
            param_count++;
      }
      if (param_count > 0)
         meta->parameters.reserve(param_count);
   }

   for (i = 0; i < lines->num_lines; i++)
   {
      const char *line = shader_line_buf_get(lines, i);
      if (!line)
         continue;

      if (memcmp(line, "#pragma", sizeof("#pragma") - 1))
         continue;

      /* Check for shader identifier */
      if (!memcmp(line, "#pragma name ",
               sizeof("#pragma name ") - 1))
      {
         const char *str = line + (sizeof("#pragma name ") - 1);
         while (*str == ' ' || *str == '\t')
            str++;
         if (!meta->name.empty())
         {
            RARCH_ERR("[Slang] Trying to declare multiple names for file.\n");
            return false;
         }
         meta->name = str;
      }
      /* Check for shader parameters */
      else if (!memcmp(line, "#pragma parameter ",
               sizeof("#pragma parameter ") - 1))
      {
         float initial, minimum, maximum, step;
         int fields         = 0;
         const char *s      = line + (sizeof("#pragma parameter ") - 1);
         size_t len         = 0;
         size_t id_len, desc_len;
         char *end          = NULL;

         /* Parse id */
         while (*s == ' ' || *s == '\t')
            s++;
         len = 0;
         while (s[len] && s[len] != ' ' && s[len] != '\t')
            len++;
         if (len == 0 || len >= sizeof(id))
         {
            RARCH_ERR("[Slang] Invalid #pragma parameter line: \"%s\".\n", line);
            return false;
         }
         memcpy(id, s, len);
         id[len] = '\0';
         id_len  = len;
         s += len;

         /* Parse quoted description */
         while (*s == ' ' || *s == '\t')
            s++;
         if (*s != '"')
         {
            RARCH_ERR("[Slang] Invalid #pragma parameter line: \"%s\".\n", line);
            return false;
         }
         s++;
         len = 0;
         while (s[len] && s[len] != '"')
            len++;
         if (s[len] != '"' || len >= sizeof(desc))
         {
            RARCH_ERR("[Slang] Invalid #pragma parameter line: \"%s\".\n", line);
            return false;
         }
         memcpy(desc, s, len);
         desc[len] = '\0';
         desc_len  = len;
         s += len + 1;

         /* Parse initial */
         while (*s == ' ' || *s == '\t')
            s++;
         initial = (float)strtod(s, &end);
         if (end == s)
         {
            RARCH_ERR("[Slang] Invalid #pragma parameter line: \"%s\".\n", line);
            return false;
         }
         s = end;

         /* Parse minimum */
         while (*s == ' ' || *s == '\t')
            s++;
         minimum = (float)strtod(s, &end);
         if (end == s)
         {
            RARCH_ERR("[Slang] Invalid #pragma parameter line: \"%s\".\n", line);
            return false;
         }
         s = end;

         /* Parse maximum */
         while (*s == ' ' || *s == '\t')
            s++;
         maximum = (float)strtod(s, &end);
         if (end == s)
         {
            RARCH_ERR("[Slang] Invalid #pragma parameter line: \"%s\".\n", line);
            return false;
         }
         s       = end;
         fields  = 5;

         /* Parse step (optional) */
         while (*s == ' ' || *s == '\t')
            s++;
         if (*s)
         {
            step = (float)strtod(s, &end);
            if (end != s)
               fields = 6;
         }

         if (fields == 5)
         {
            step    = 0.1f * (maximum - minimum);
            fields  = 6;
         }

         {
            bool parameter_found   = false;
            size_t parameter_index = 0;
            size_t j;

            for (j = 0; j < meta->parameters.size(); j++)
            {
               const std::string &pid = meta->parameters[j].id;
               if (pid.size() == id_len
                     && !memcmp(pid.data(), id, id_len))
               {
                  parameter_found = true;
                  parameter_index = j;
                  break;
               }
            }

            /* Allow duplicate #pragma parameter, but only
             * if they are exactly the same. */
            if (parameter_found)
            {
               const glslang_parameter *parameter =
                  &meta->parameters[parameter_index];
               if (     parameter->desc.size() != desc_len
                     || memcmp(parameter->desc.data(), desc, desc_len)
                     || (parameter->initial != initial)
                     || (parameter->minimum != minimum)
                     || (parameter->maximum != maximum)
                     || (parameter->step    != step)
                  )
               {
                  RARCH_ERR("[Slang] Duplicate parameters"
                        " found for \"%s\", but arguments"
                        " do not match.\n", id);
                  return false;
               }
            }
            else
            {
               glslang_parameter p;
               p.id.assign(id, id_len);
               p.desc.assign(desc, desc_len);
               p.initial = initial;
               p.minimum = minimum;
               p.maximum = maximum;
               p.step    = step;
               meta->parameters.push_back(p);
            }
         }
      }
      /* Check for framebuffer format */
      else if (!memcmp(line, "#pragma format ",
               sizeof("#pragma format ") - 1))
      {
         const char *str = line + (sizeof("#pragma format ") - 1);
         while (*str == ' ' || *str == '\t')
            str++;
         if (meta->rt_format != SLANG_FORMAT_UNKNOWN)
         {
            RARCH_ERR("[Slang] Trying to declare format"
                  " multiple times for file.\n");
            return false;
         }
         meta->rt_format = glslang_find_format(str);
         if (meta->rt_format == SLANG_FORMAT_UNKNOWN)
         {
            RARCH_ERR("[Slang] Failed to find format \"%s\".\n", str);
            return false;
         }
      }
   }

   return true;
}

/* -----------------------------------------------------------------------
 * glslang_compile_shader — now uses shader_line_buf
 * ----------------------------------------------------------------------- */
bool glslang_compile_shader(const char *shader_path, glslang_output *output)
{
#if defined(HAVE_GLSLANG)
   struct shader_line_buf lines;

   if (!shader_line_buf_init(&lines))
      return false;

   RARCH_LOG("[Slang] Compiling shader: \"%s\".\n", shader_path);

   if (!glslang_read_shader_file(shader_path, &lines, true, false))
      goto error;
   output->meta = glslang_meta{};
   if (!glslang_parse_meta(&lines, &output->meta))
      goto error;

   if (!glslang::compile_spirv(build_stage_source(&lines, "vertex"),
            glslang::StageVertex, &output->vertex))
   {
      RARCH_ERR("[Slang] Failed to compile vertex shader stage.\n");
      goto error;
   }

   if (!glslang::compile_spirv(build_stage_source(&lines, "fragment"),
            glslang::StageFragment, &output->fragment))
   {
      RARCH_ERR("[Slang] Failed to compile fragment shader stage.\n");
      goto error;
   }

   shader_line_buf_free(&lines);

   return true;

error:
   shader_line_buf_free(&lines);
#endif

   {
      size_t _len;
      char msg[NAME_MAX_LENGTH];

      _len = snprintf(msg, sizeof(msg), "Failed to compile shader: \"%s\".",
            path_basename(shader_path));

      runloop_msg_queue_push(msg, _len, 1, 120, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
   }

   return false;
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
      unsigned k;
      struct video_shader_parameter *p = NULL;
      bool mismatch_dup                = false;
      for (k = 0; k < shader->num_parameters; k++)
      {
         if (meta.parameters[i].id == shader->parameters[k].id)
         {
            p = &shader->parameters[k];
            break;
         }
      }

      if (p != NULL)
      {
         /* Allow duplicate #pragma parameter, but only
          * if they are exactly the same. */
         if (     meta.parameters[i].desc    != p->desc
               || meta.parameters[i].initial != p->initial
               || meta.parameters[i].minimum != p->minimum
               || meta.parameters[i].maximum != p->maximum
               || meta.parameters[i].step    != p->step)
         {
            RARCH_ERR("[Slang] Duplicate parameters"
                  " found for \"%s\", but arguments do not match.\n",
                  p->id);
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

/* -----------------------------------------------------------------------
 * slang_preprocess_parse_parameters (C-linkage overload) — uses shader_line_buf
 * ----------------------------------------------------------------------- */
bool slang_preprocess_parse_parameters(const char *shader_path,
      struct video_shader *shader)
{
   struct shader_line_buf lines;

   memset(&lines, 0, sizeof(lines));

   if (shader_line_buf_init(&lines))
   {
      if (glslang_read_shader_file(shader_path, &lines, true, false))
      {
         glslang_meta meta = glslang_meta{};
         if (glslang_parse_meta(&lines, &meta))
         {
            shader_line_buf_free(&lines);
            return slang_preprocess_parse_parameters(meta, shader);
         }
      }
   }

   shader_line_buf_free(&lines);
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
   glslang_output          output;
   spirv_cross::Compiler  *vs_compiler = NULL;
   spirv_cross::Compiler  *ps_compiler = NULL;
   video_shader_pass      &pass        = shader_info->pass[pass_number];

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
      spirv_cross::ShaderResources vs_resources;
      spirv_cross::ShaderResources ps_resources;
      std::string     vs_code;
      std::string     ps_code;

      switch (dst_type)
      {
         case RARCH_SHADER_HLSL:
         case RARCH_SHADER_CG:
#ifdef HAVE_HLSL
            vs_compiler = new spirv_cross::CompilerHLSL(output.vertex);
            ps_compiler = new spirv_cross::CompilerHLSL(output.fragment);
#endif
            break;

         case RARCH_SHADER_METAL:
            vs_compiler = new spirv_cross::CompilerMSL(output.vertex);
            ps_compiler = new spirv_cross::CompilerMSL(output.fragment);
            break;

         default:
            vs_compiler = new spirv_cross::CompilerGLSL(output.vertex);
            ps_compiler = new spirv_cross::CompilerGLSL(output.fragment);
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
               spirv_cross::CompilerHLSL::Options options;
               spirv_cross::CompilerHLSL *vs = (spirv_cross::CompilerHLSL*)vs_compiler;
               spirv_cross::CompilerHLSL *ps = (spirv_cross::CompilerHLSL*)ps_compiler;
               options.shader_model          = version;
               vs->set_hlsl_options(options);
               ps->set_hlsl_options(options);
               vs_code = vs->compile();
               ps_code = ps->compile();
            }
#endif
            break;
         case RARCH_SHADER_METAL:
            {
               spirv_cross::CompilerMSL::Options options;
               spirv_cross::CompilerMSL *vs = (spirv_cross::CompilerMSL*)vs_compiler;
               spirv_cross::CompilerMSL *ps = (spirv_cross::CompilerMSL*)ps_compiler;
               options.msl_version          = version;
               vs->set_msl_options(options);
               ps->set_msl_options(options);

               const auto remap_push_constant = [](spirv_cross::CompilerMSL *comp,
                     const spirv_cross::ShaderResources &resources) {
                  for (const spirv_cross::Resource& resource : resources.push_constant_buffers)
                  {
                     /* Explicit 1:1 mapping for bindings. */
                     spirv_cross::MSLResourceBinding binding;
                     binding.stage              = comp->get_execution_model();
                     binding.desc_set           = spirv_cross::kPushConstDescSet;
                     binding.binding            = spirv_cross::kPushConstBinding;
                     /* Use earlier decoration override. */
                     binding.basetype           = spirv_cross::SPIRType::Unknown;
                     binding.count              = 0;
                     binding.msl_buffer         = comp->get_decoration(
                           resource.id, spv::DecorationBinding);
                     binding.msl_texture        = 0;
                     binding.msl_sampler        = 0;
                     comp->add_msl_resource_binding(binding);
                  }
               };

               const auto remap_generic_resource = [](spirv_cross::CompilerMSL *comp,
                     const spirv_cross::SmallVector<spirv_cross::Resource> &resources) {
                  for (const spirv_cross::Resource& resource : resources)
                  {
                     /* Explicit 1:1 mapping for bindings. */
                     spirv_cross::MSLResourceBinding binding;
                     binding.stage              = comp->get_execution_model();
                     binding.desc_set           = comp->get_decoration(
                           resource.id, spv::DecorationDescriptorSet);
                     binding.basetype           = spirv_cross::SPIRType::Unknown;
                     binding.count              = 0;

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
               spirv_cross::CompilerGLSL::Options options;
               spirv_cross::CompilerGLSL *vs = (spirv_cross::CompilerGLSL*)vs_compiler;
               spirv_cross::CompilerGLSL *ps = (spirv_cross::CompilerGLSL*)ps_compiler;
               options.version               = version;
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
      RARCH_ERR("[Slang] SPIRV-Cross threw exception: %s.\n", e.what());
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

static bool set_ubo_texture_offset(
      slang_reflection *reflection,
      slang_texture_semantic semantic,
      unsigned index,
      size_t offset, bool push_constant)
{
   resize_minimum(reflection->semantic_textures[semantic], index + 1);
   slang_texture_semantic_meta &sem = reflection->semantic_textures[semantic][index];
   bool   &active                   = push_constant ? sem.push_constant : sem.uniform;
   size_t &active_offset            = push_constant ? sem.push_constant_offset : sem.ubo_offset;

   if (active)
   {
      if (active_offset != offset)
      {
         RARCH_ERR("[Slang] Vertex and fragment have"
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
   slang_semantic_meta &sem = reflection->semantic_float_parameters[index];
   bool   &active           = push_constant ? sem.push_constant : sem.uniform;
   size_t &active_offset    = push_constant ? sem.push_constant_offset : sem.ubo_offset;

   if (active)
   {
      if (active_offset != offset)
      {
         RARCH_ERR("[Slang] Vertex and fragment have different"
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
      RARCH_ERR("[Slang] Vertex and fragment have different "
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
   slang_semantic_meta &sem = reflection->semantics[semantic];
   bool   &active           = push_constant ? sem.push_constant : sem.uniform;
   size_t &active_offset    = push_constant ? sem.push_constant_offset : sem.ubo_offset;

   if (active)
   {
      if (active_offset != offset)
      {
         RARCH_ERR("[Slang] Vertex and fragment have "
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
      RARCH_ERR("[Slang] Vertex and fragment have different"
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

static bool validate_type_for_semantic(const spirv_cross::SPIRType &type, slang_semantic sem)
{
   if (!type.array.empty())
      return false;
   if (     type.basetype != spirv_cross::SPIRType::Float
         && type.basetype != spirv_cross::SPIRType::Int
         && type.basetype != spirv_cross::SPIRType::UInt)
      return false;

   switch (sem)
   {
         /* mat4 */
      case SLANG_SEMANTIC_MVP:
         return
               type.basetype == spirv_cross::SPIRType::Float
            && type.vecsize  == 4
            && type.columns  == 4;
         /* uint */
      case SLANG_SEMANTIC_FRAME_COUNT:
         return type.basetype == spirv_cross::SPIRType::UInt
            &&  type.vecsize  == 1
            &&  type.columns  == 1;
         /* int */
      case SLANG_SEMANTIC_TOTAL_SUBFRAMES:
         return type.basetype == spirv_cross::SPIRType::UInt
            &&  type.vecsize  == 1
            &&  type.columns  == 1;
         /* int */
      case SLANG_SEMANTIC_CURRENT_SUBFRAME:
         return type.basetype == spirv_cross::SPIRType::UInt
            &&  type.vecsize  == 1
            &&  type.columns  == 1;
         /* int */
      case SLANG_SEMANTIC_FRAME_DIRECTION:
         return type.basetype == spirv_cross::SPIRType::Int
            &&  type.vecsize  == 1
            &&  type.columns  == 1;
         /* uint */
      case SLANG_SEMANTIC_FRAME_TIME_DELTA:
         return type.basetype == spirv_cross::SPIRType::UInt
            &&  type.vecsize  == 1
            &&  type.columns  == 1;
         /* uint */
      case SLANG_SEMANTIC_ORIGINAL_FPS:
         return type.basetype == spirv_cross::SPIRType::Float
            &&  type.vecsize  == 1
            &&  type.columns  == 1;
         /* uint */
      case SLANG_SEMANTIC_ROTATION:
         return type.basetype == spirv_cross::SPIRType::UInt
            &&  type.vecsize  == 1
            &&  type.columns  == 1;
      case SLANG_SEMANTIC_CORE_ASPECT:
         return type.basetype == spirv_cross::SPIRType::Float
            &&  type.vecsize  == 1
            &&  type.columns  == 1;
      case SLANG_SEMANTIC_CORE_ASPECT_ROT:
         return type.basetype == spirv_cross::SPIRType::Float
            &&  type.vecsize  == 1
            &&  type.columns  == 1;
         /* vec3 - sensor uniforms */
      case SLANG_SEMANTIC_GYROSCOPE:
      case SLANG_SEMANTIC_ACCELEROMETER:
      case SLANG_SEMANTIC_ACCELEROMETER_REST:
         return type.basetype == spirv_cross::SPIRType::Float
            &&  type.vecsize  == 3
            &&  type.columns  == 1;
         /* float */
      case SLANG_SEMANTIC_FLOAT_PARAMETER:
         return type.basetype == spirv_cross::SPIRType::Float
            &&  type.vecsize  == 1
            &&  type.columns  == 1;
      case SLANG_SEMANTIC_HDR:
         return type.basetype == spirv_cross::SPIRType::UInt
            &&  type.vecsize  == 1
            &&  type.columns  == 1;
      case SLANG_SEMANTIC_PAPER_WHITE_NITS:
         return type.basetype == spirv_cross::SPIRType::Float
            &&  type.vecsize  == 1
            &&  type.columns  == 1;
      case SLANG_SEMANTIC_SCANLINES:
         return type.basetype == spirv_cross::SPIRType::Float
            &&  type.vecsize  == 1
            &&  type.columns  == 1;
      case SLANG_SEMANTIC_SUBPIXEL_LAYOUT:
         return type.basetype == spirv_cross::SPIRType::UInt
            &&  type.vecsize  == 1
            &&  type.columns  == 1;
      case SLANG_SEMANTIC_EXPAND_GAMUT:
         return type.basetype == spirv_cross::SPIRType::UInt
            &&  type.vecsize  == 1
            &&  type.columns  == 1;
      case SLANG_SEMANTIC_INVERSE_TONEMAP:
         return type.basetype == spirv_cross::SPIRType::Float
            &&  type.vecsize  == 1
            &&  type.columns  == 1;
      case SLANG_SEMANTIC_HDR10:
         return type.basetype == spirv_cross::SPIRType::Float
            &&  type.vecsize  == 1
            &&  type.columns  == 1;
         /* vec4 */
      default:
         break;
   }
   return type.basetype == spirv_cross::SPIRType::Float
      &&  type.vecsize  == 4
      &&  type.columns  == 1;
}

static bool validate_type_for_texture_semantic(const spirv_cross::SPIRType &type)
{
   return    (type.array.empty())
          && (type.basetype == spirv_cross::SPIRType::Float)
          && (type.vecsize  == 4)
          && (type.columns  == 1);
}

static bool add_active_buffer_ranges(
      const spirv_cross::Compiler &compiler,
      const spirv_cross::Resource &resource,
      slang_reflection *reflection,
      bool push_constant)
{
   unsigned i;
   /* Get which uniforms are actually in use by this shader. */
   spirv_cross::SmallVector<spirv_cross::BufferRange> ranges =
      compiler.get_active_buffer_ranges(resource.id);

   for (i = 0; i < ranges.size(); i++)
   {
      unsigned sem_index             = 0;
      unsigned tex_sem_index         = 0;
      const std::string &name        = compiler.get_member_name(
            resource.base_type_id, ranges[i].index);
      const spirv_cross::SPIRType &type = compiler.get_type(
            compiler.get_type(resource.base_type_id).member_types[
            ranges[i].index]);
      slang_semantic sem             = slang_uniform_name_to_semantic(
            *reflection->semantic_map, name, &sem_index);
      slang_texture_semantic tex_sem = slang_uniform_name_to_texture_semantic(
            *reflection->texture_semantic_uniform_map,
            name, &tex_sem_index);

      if (tex_sem == SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT &&
            tex_sem_index >= reflection->pass_number)
      {
         RARCH_ERR("[Slang] Non causal filter chain detected. "
               "Shader is trying to use output from pass #%u,"
               " but this shader is pass #%u.\n",
               tex_sem_index, reflection->pass_number);
         return false;
      }

      if (sem != SLANG_INVALID_SEMANTIC)
      {
         if (!validate_type_for_semantic(type, sem))
         {
            RARCH_ERR("[Slang] Underlying type of semantic is invalid.\n");
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
            RARCH_ERR("[Slang] Underlying type of texture"
                  " semantic is invalid.\n");
            return false;
         }

         if (!set_ubo_texture_offset(reflection, tex_sem, tex_sem_index,
                  ranges[i].offset, push_constant))
            return false;
      }
      else
      {
         /* TODO - Try to print name */
         RARCH_ERR("[Slang] Unknown semantic found.\n");
         return false;
      }
   }
   return true;
}


slang_reflection::slang_reflection()
   : ubo_size(0),
     push_constant_size(0),
     ubo_binding(0),
     ubo_stage_mask(0),
     push_constant_stage_mask(0),
     texture_semantic_map(NULL),
     texture_semantic_uniform_map(NULL),
     semantic_map(NULL),
     pass_number(0)
{
   unsigned i;
   for (i = 0; i < SLANG_NUM_TEXTURE_SEMANTICS; i++)
      semantic_textures[i].resize(
            slang_texture_semantic_is_array(
               static_cast<slang_texture_semantic>(i))
            ? 0 : 1);
}

bool slang_reflect(
      const spirv_cross::Compiler &vertex_compiler,
      const spirv_cross::Compiler &fragment_compiler,
      const spirv_cross::ShaderResources &vertex,
      const spirv_cross::ShaderResources &fragment,
      slang_reflection *reflection)
{
   uint32_t location_mask = 0;
   uint32_t binding_mask  = 0;
   unsigned i             = 0;

   /* Validate use of unexpected types. */
   if (
            !vertex.sampled_images.empty()
         || !vertex.storage_buffers.empty()
         || !vertex.subpass_inputs.empty()
         || !vertex.storage_images.empty()
         || !vertex.atomic_counters.empty()
         || !fragment.storage_buffers.empty()
         || !fragment.subpass_inputs.empty()
         || !fragment.storage_images.empty()
         || !fragment.atomic_counters.empty())
   {
      RARCH_ERR("[Slang] Invalid resource type detected.\n");
      return false;
   }

   /* Validate vertex input. */
   if (vertex.stage_inputs.size() != 2)
   {
      RARCH_ERR("[Slang] Vertex must have two attributes.\n");
      return false;
   }

   if (fragment.stage_outputs.size() != 1)
   {
      RARCH_ERR("[Slang] Multiple render targets not supported.\n");
      return false;
   }

   if (fragment_compiler.get_decoration(
            fragment.stage_outputs[0].id, spv::DecorationLocation) != 0)
   {
      RARCH_ERR("[Slang] Render target must use location = 0.\n");
      return false;
   }

   for (i = 0; i < vertex.stage_inputs.size(); i++)
      location_mask |= 1 << vertex_compiler.get_decoration(
            vertex.stage_inputs[i].id, spv::DecorationLocation);

   if (location_mask != 0x3)
   {
      RARCH_ERR("[Slang] The two vertex attributes do not"
            " use location = 0 and location = 1.\n");
      return false;
   }

   /* Validate the single uniform buffer. */
   if (vertex.uniform_buffers.size() > 1)
   {
      RARCH_ERR("[Slang] Vertex must use zero or one uniform buffer.\n");
      return false;
   }

   if (fragment.uniform_buffers.size() > 1)
   {
      RARCH_ERR("[Slang] Fragment must use zero or one uniform buffer.\n");
      return false;
   }

   /* Validate the single push constant buffer. */
   if (vertex.push_constant_buffers.size() > 1)
   {
      RARCH_ERR("[Slang] Vertex must use zero or one push constant buffers.\n");
      return false;
   }

   if (fragment.push_constant_buffers.size() > 1)
   {
      RARCH_ERR("[Slang] Fragment must use zero or one push constant buffer.\n");
      return false;
   }

   uint32_t vertex_ubo    = vertex.uniform_buffers.empty()
      ? 0 : (uint32_t)vertex.uniform_buffers[0].id;
   uint32_t fragment_ubo  = fragment.uniform_buffers.empty()
      ? 0 : (uint32_t)fragment.uniform_buffers[0].id;
   uint32_t vertex_push   = vertex.push_constant_buffers.empty()
      ? 0 : (uint32_t)vertex.push_constant_buffers[0].id;
   uint32_t fragment_push = fragment.push_constant_buffers.empty()
      ? 0 : (uint32_t)fragment.push_constant_buffers[0].id;

   if (vertex_ubo &&
         vertex_compiler.get_decoration(
            vertex_ubo, spv::DecorationDescriptorSet) != 0)
   {
      RARCH_ERR("[Slang] Resources must use descriptor set #0.\n");
      return false;
   }

   if (fragment_ubo &&
         fragment_compiler.get_decoration(
            fragment_ubo, spv::DecorationDescriptorSet) != 0)
   {
      RARCH_ERR("[Slang] Resources must use descriptor set #0.\n");
      return false;
   }

   unsigned vertex_ubo_binding   = vertex_ubo
      ? vertex_compiler.get_decoration(vertex_ubo, spv::DecorationBinding)
      : (unsigned)-1;
   unsigned fragment_ubo_binding = fragment_ubo
      ? fragment_compiler.get_decoration(fragment_ubo, spv::DecorationBinding)
      : (unsigned)-1;
   bool has_ubo                  = vertex_ubo || fragment_ubo;

   if (  (vertex_ubo_binding   != (unsigned)-1) &&
         (fragment_ubo_binding != (unsigned)-1) &&
         (vertex_ubo_binding   != fragment_ubo_binding))
   {
      RARCH_ERR("[Slang] Vertex and fragment uniform buffer must have same binding.\n");
      return false;
   }

   unsigned ubo_binding = (vertex_ubo_binding != (unsigned)-1)
      ? vertex_ubo_binding
      : fragment_ubo_binding;

   if (has_ubo && ubo_binding >= SLANG_NUM_BINDINGS)
   {
      RARCH_ERR("[Slang] Binding %u is out of range.\n", ubo_binding);
      return false;
   }

   reflection->ubo_binding              = has_ubo ? ubo_binding : 0;
   reflection->ubo_stage_mask           = 0;
   reflection->ubo_size                 = 0;
   reflection->push_constant_size       = 0;
   reflection->push_constant_stage_mask = 0;

   if (vertex_ubo)
   {
      size_t _y;
      reflection->ubo_stage_mask |= SLANG_STAGE_VERTEX_MASK;
      _y = vertex_compiler.get_declared_struct_size(
               vertex_compiler.get_type(
                  vertex.uniform_buffers[0].base_type_id));
      reflection->ubo_size        = MAX(reflection->ubo_size, _y);
   }

   if (fragment_ubo)
   {
      size_t _y;
      reflection->ubo_stage_mask |= SLANG_STAGE_FRAGMENT_MASK;
      _y = fragment_compiler.get_declared_struct_size(
               fragment_compiler.get_type(
                  fragment.uniform_buffers[0].base_type_id));
      reflection->ubo_size        = MAX(reflection->ubo_size, _y);
   }

   if (vertex_push)
   {
      size_t _y;
      reflection->push_constant_stage_mask |= SLANG_STAGE_VERTEX_MASK;
      _y = vertex_compiler.get_declared_struct_size(
               vertex_compiler.get_type(
                  vertex.push_constant_buffers[0].base_type_id));
      reflection->push_constant_size        = MAX(
            reflection->push_constant_size, _y);
   }

   if (fragment_push)
   {
      size_t _y;
      reflection->push_constant_stage_mask |= SLANG_STAGE_FRAGMENT_MASK;
      _y = fragment_compiler.get_declared_struct_size(
               fragment_compiler.get_type(
                  fragment.push_constant_buffers[0].base_type_id));
      reflection->push_constant_size        = MAX(
            reflection->push_constant_size, _y);
   }

   /* Validate push constant size against Vulkan's
    * minimum spec to avoid cross-vendor issues. */
   if (reflection->push_constant_size > 128)
   {
      RARCH_ERR("[Slang] Exceeded maximum size of 128 bytes"
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
         RARCH_ERR("[Slang] Resources must use descriptor set #0.\n");
         return false;
      }

      if (binding >= SLANG_NUM_BINDINGS)
      {
         RARCH_ERR("[Slang] Binding %u is out of range.\n", ubo_binding);
         return false;
      }

      if (binding_mask & (1 << binding))
      {
         RARCH_ERR("[Slang] Binding %u is already in use.\n", binding);
         return false;
      }
      binding_mask |= 1 << binding;

      slang_texture_semantic index = slang_name_to_texture_semantic(
            *reflection->texture_semantic_map,
            fragment.sampled_images[i].name, &array_index);

      if (index == SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT &&
            array_index >= reflection->pass_number)
      {
         RARCH_ERR("[Slang] Non causal filter chain detected. "
               "Shader is trying to use output from pass #%u,"
               " but this shader is pass #%u.\n",
               array_index, reflection->pass_number);
         return false;
      }
      else if (index == SLANG_INVALID_TEXTURE_SEMANTIC)
      {
         RARCH_ERR("[Slang] Texture name '%s' not found in semantic map, "
                   "Probably the texture name or pass alias is not defined "
                   "in the preset (Non-semantic textures not supported yet)\n",
                   fragment.sampled_images[i].name.c_str());
         return false;
      }

      resize_minimum(reflection->semantic_textures[index], array_index + 1);
      slang_texture_semantic_meta &semantic =
         reflection->semantic_textures[index][array_index];
      semantic.binding                      = binding;
      semantic.stage_mask                   = SLANG_STAGE_FRAGMENT_MASK;
      semantic.texture                      = true;
   }

#ifdef DEBUG
   RARCH_LOG("[Slang] Reflection\n");
   RARCH_LOG("[Slang]   Textures:\n");

   for (i = 0; i < SLANG_NUM_TEXTURE_SEMANTICS; i++)
   {
      unsigned index = 0;
      unsigned j;
      for (j = 0; j < reflection->semantic_textures[i].size(); j++)
      {
         const slang_texture_semantic_meta &sem = reflection->semantic_textures[i][j];
         if (sem.texture)
            RARCH_LOG("[Slang]      %s (#%u)\n",
                  texture_semantic_names[i], index);
         index++;
      }
   }

   RARCH_LOG("[Slang]   Uniforms (Vertex: %s, Fragment: %s):\n",
         reflection->ubo_stage_mask & SLANG_STAGE_VERTEX_MASK ? "yes": "no",
         reflection->ubo_stage_mask & SLANG_STAGE_FRAGMENT_MASK ? "yes": "no");
   RARCH_LOG("[Slang]   Push Constants (Vertex: %s, Fragment: %s):\n",
         reflection->push_constant_stage_mask & SLANG_STAGE_VERTEX_MASK ? "yes": "no",
         reflection->push_constant_stage_mask & SLANG_STAGE_FRAGMENT_MASK ? "yes": "no");

   for (i = 0; i < SLANG_NUM_SEMANTICS; i++)
   {
      if (reflection->semantics[i].uniform)
      {
         RARCH_LOG("[Slang]      %s (Offset: %u)\n",
               semantic_uniform_names[i],
               unsigned(reflection->semantics[i].ubo_offset));
      }

      if (reflection->semantics[i].push_constant)
      {
         RARCH_LOG("[Slang]      %s (PushOffset: %u)\n",
               semantic_uniform_names[i],
               unsigned(reflection->semantics[i].push_constant_offset));
      }
   }

   for (i = 0; i < SLANG_NUM_TEXTURE_SEMANTICS; i++)
   {
      unsigned index = 0;
      unsigned j;
      for (j = 0; j < reflection->semantic_textures[i].size(); j++)
      {
         const slang_texture_semantic_meta &sem = reflection->semantic_textures[i][j];
         if (sem.uniform)
         {
            RARCH_LOG("[Slang]      %s (#%u) (Offset: %u)\n",
                  texture_semantic_uniform_names[i],
                  index,
                  unsigned(sem.ubo_offset));
         }

         if (sem.push_constant)
         {
            RARCH_LOG("[Slang]      %s (#%u) (PushOffset: %u)\n",
                  texture_semantic_uniform_names[i],
                  index,
                  unsigned(sem.push_constant_offset));
         }
         index++;
      }
   }

   RARCH_LOG("[Slang]   Parameters:\n");

   for (i = 0; i < reflection->semantic_float_parameters.size(); i++)
   {
      const slang_semantic_meta &param = reflection->semantic_float_parameters[i];

      if (param.uniform)
         RARCH_LOG("[Slang]     #%u (Offset: %u)\n", i,
               (unsigned int)param.ubo_offset);
      if (param.push_constant)
         RARCH_LOG("[Slang]     #%u (PushOffset: %u)\n", i,
               (unsigned int)param.push_constant_offset);
   }
#endif

   return true;
}

bool slang_reflect_spirv(const std::vector<uint32_t> &vertex,
      const std::vector<uint32_t> &fragment,
      slang_reflection *reflection)
{
   try
   {
      spirv_cross::Compiler vertex_compiler(vertex);
      spirv_cross::Compiler fragment_compiler(fragment);
      spirv_cross::ShaderResources
         vertex_resources     = vertex_compiler.get_shader_resources();
      spirv_cross::ShaderResources
         fragment_resources   = fragment_compiler.get_shader_resources();

      if (!slang_reflect(vertex_compiler, fragment_compiler,
               vertex_resources, fragment_resources,
               reflection))
      {
         RARCH_ERR("[Slang] Failed to reflect SPIR-V."
               " Resource usage is inconsistent with expectations.\n");
         return false;
      }

      return true;
   }
   catch (const std::exception &e)
   {
      RARCH_ERR("[Slang] SPIRV-Cross threw exception: %s.\n", e.what());
      return false;
   }
}

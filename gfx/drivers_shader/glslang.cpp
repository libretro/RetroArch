/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016 - Hans-Kristian Arntzen
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

#include "glslang_compile.h"

#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <vector>
#include <iostream>
#include <cstring>
#include <cstdlib>

#include <retro_atomic.h>

#include "../../verbosity.h"

/* glslang's process state - its TLS key and the built-in symbol tables
 * every compile shares - is started once, before any thread compiles
 * (video_driver_init_internal(), ahead of the video thread and the
 * shader tasks), and kept for the life of the process: finishing it
 * frees the TLS key under any thread still inside glslang, a task left
 * running past the queue's exit included, and the tables are most of a
 * compile's cost. A compile reads the state word alone. */
#define GLSLANG_PROCESS_DOWN     0
#define GLSLANG_PROCESS_STARTING 1
#define GLSLANG_PROCESS_UP       2

static retro_atomic_int_t glslang_process_state;
static TBuiltInResource   glslang_resources;   /* fixed once up */

static void glslang_resources_fill(TBuiltInResource *r)
{
   std::memset(r, 0, sizeof(*r));

   /* Integer resource limits */
   r->maxLights                                 = 32;
   r->maxClipPlanes                             = 6;
   r->maxTextureUnits                           = 32;
   r->maxTextureCoords                          = 32;
   r->maxVertexAttribs                          = 64;
   r->maxVertexUniformComponents                = 4096;
   r->maxVaryingFloats                          = 64;
   r->maxVertexTextureImageUnits                = 32;
   r->maxCombinedTextureImageUnits              = 80;
   r->maxTextureImageUnits                      = 32;
   r->maxFragmentUniformComponents              = 4096;
   r->maxDrawBuffers                            = 32;
   r->maxVertexUniformVectors                   = 128;
   r->maxVaryingVectors                         = 8;
   r->maxFragmentUniformVectors                 = 16;
   r->maxVertexOutputVectors                    = 16;
   r->maxFragmentInputVectors                   = 15;
   r->minProgramTexelOffset                     = -8;
   r->maxProgramTexelOffset                     = 7;
   r->maxClipDistances                          = 8;
   r->maxComputeWorkGroupCountX                 = 65535;
   r->maxComputeWorkGroupCountY                 = 65535;
   r->maxComputeWorkGroupCountZ                 = 65535;
   r->maxComputeWorkGroupSizeX                  = 1024;
   r->maxComputeWorkGroupSizeY                  = 1024;
   r->maxComputeWorkGroupSizeZ                  = 64;
   r->maxComputeUniformComponents               = 1024;
   r->maxComputeTextureImageUnits               = 16;
   r->maxComputeImageUniforms                   = 8;
   r->maxComputeAtomicCounters                  = 8;
   r->maxComputeAtomicCounterBuffers            = 1;
   r->maxVaryingComponents                      = 60;
   r->maxVertexOutputComponents                 = 64;
   r->maxGeometryInputComponents                = 64;
   r->maxGeometryOutputComponents               = 128;
   r->maxFragmentInputComponents                = 128;
   r->maxImageUnits                             = 8;
   r->maxCombinedImageUnitsAndFragmentOutputs   = 8;
   r->maxCombinedShaderOutputResources          = 8;
   r->maxImageSamples                           = 0;
   r->maxVertexImageUniforms                    = 0;
   r->maxTessControlImageUniforms               = 0;
   r->maxTessEvaluationImageUniforms            = 0;
   r->maxGeometryImageUniforms                  = 0;
   r->maxFragmentImageUniforms                  = 8;
   r->maxCombinedImageUniforms                  = 8;
   r->maxGeometryTextureImageUnits              = 16;
   r->maxGeometryOutputVertices                 = 256;
   r->maxGeometryTotalOutputComponents          = 1024;
   r->maxGeometryUniformComponents              = 1024;
   r->maxGeometryVaryingComponents              = 64;
   r->maxTessControlInputComponents             = 128;
   r->maxTessControlOutputComponents            = 128;
   r->maxTessControlTextureImageUnits           = 16;
   r->maxTessControlUniformComponents           = 1024;
   r->maxTessControlTotalOutputComponents       = 4096;
   r->maxTessEvaluationInputComponents          = 128;
   r->maxTessEvaluationOutputComponents         = 128;
   r->maxTessEvaluationTextureImageUnits        = 16;
   r->maxTessEvaluationUniformComponents        = 1024;
   r->maxTessPatchComponents                    = 120;
   r->maxPatchVertices                          = 32;
   r->maxTessGenLevel                           = 64;
   r->maxViewports                              = 16;
   r->maxVertexAtomicCounters                   = 0;
   r->maxTessControlAtomicCounters              = 0;
   r->maxTessEvaluationAtomicCounters           = 0;
   r->maxGeometryAtomicCounters                 = 0;
   r->maxFragmentAtomicCounters                 = 8;
   r->maxCombinedAtomicCounters                 = 8;
   r->maxAtomicCounterBindings                  = 1;
   r->maxVertexAtomicCounterBuffers             = 0;
   r->maxTessControlAtomicCounterBuffers        = 0;
   r->maxTessEvaluationAtomicCounterBuffers     = 0;
   r->maxGeometryAtomicCounterBuffers           = 0;
   r->maxFragmentAtomicCounterBuffers           = 1;
   r->maxCombinedAtomicCounterBuffers           = 1;
   r->maxAtomicCounterBufferSize                = 16384;
   r->maxTransformFeedbackBuffers               = 4;
   r->maxTransformFeedbackInterleavedComponents = 64;
   r->maxCullDistances                          = 8;
   r->maxCombinedClipAndCullDistances           = 8;
   r->maxSamples                                = 4;

   /* Boolean capability limits */
   r->limits.nonInductiveForLoops                       = true;
   r->limits.whileLoops                                 = true;
   r->limits.doWhileLoops                               = true;
   r->limits.generalUniformIndexing                     = true;
   r->limits.generalAttributeMatrixVectorIndexing       = true;
   r->limits.generalVaryingIndexing                     = true;
   r->limits.generalSamplerIndexing                     = true;
   r->limits.generalVariableIndexing                    = true;
   r->limits.generalConstantMatrixVectorIndexing        = true;
}

extern "C" void glslang_process_init(void)
{
   if (!retro_atomic_cas_int(&glslang_process_state,
            GLSLANG_PROCESS_DOWN, GLSLANG_PROCESS_STARTING))
      return;
   glslang_resources_fill(&glslang_resources);
   glslang::InitializeProcess();
   retro_atomic_store_release_int(&glslang_process_state, GLSLANG_PROCESS_UP);
}

static bool glslang_compile_spirv_impl(const char *src, EShLanguage language,
      std::vector<uint32_t> *spirv)
{
   glslang::TProgram program;

   glslang::TShader shader(language);

   shader.setStrings(&src, 1);

   glslang::TShader::ForbidIncluder forbid_include =
      glslang::TShader::ForbidIncluder();
   EShMessages messages = static_cast<EShMessages>(
         EShMsgDefault | EShMsgVulkanRules | EShMsgSpvRules);

   std::string preprocessed_msg;
   if (!shader.preprocess(&glslang_resources,
            100, ENoProfile, false, false,
            messages, &preprocessed_msg,
            forbid_include))
   {
      RARCH_ERR("[Slang] %s\n", preprocessed_msg.c_str());
      return false;
   }

   if (!shader.parse(&glslang_resources, 100, false, messages))
   {
      RARCH_ERR("[Slang] %s\n", shader.getInfoLog());
      RARCH_ERR("[Slang] %s\n", shader.getInfoDebugLog());
      return false;
   }

   program.addShader(&shader);

   if (!program.link(messages))
   {
      RARCH_ERR("[Slang] %s\n", program.getInfoLog());
      RARCH_ERR("[Slang] %s\n", program.getInfoDebugLog());
      return false;
   }

   glslang::GlslangToSpv(*program.getIntermediate(language), *spirv);
   return true;
}

extern "C" bool glslang_compile_spirv(const char *source,
      enum glslang_compile_stage stage,
      uint32_t **spirv, size_t *spirv_len)
{
   EShLanguage language;
   std::vector<uint32_t> words;

   *spirv     = NULL;
   *spirv_len = 0;

   if (!source)
      return false;
   /* a caller that started no video driver: tools, harnesses */
   if (retro_atomic_load_acquire_int(&glslang_process_state)
         != GLSLANG_PROCESS_UP)
   {
      glslang_process_init();
      if (retro_atomic_load_acquire_int(&glslang_process_state)
            != GLSLANG_PROCESS_UP)
      {
         RARCH_ERR("[Slang] glslang is still starting on another thread.\n");
         return false;
      }
   }

   switch (stage)
   {
      case GLSLANG_COMPILE_STAGE_VERTEX:
         language = EShLangVertex;
         break;
      case GLSLANG_COMPILE_STAGE_FRAGMENT:
         language = EShLangFragment;
         break;
      default:
         return false;
   }

   if (!glslang_compile_spirv_impl(source, language, &words))
      return false;

   if (words.empty())
      return false;

   *spirv = (uint32_t*)malloc(words.size() * sizeof(uint32_t));
   if (!*spirv)
      return false;

   memcpy(*spirv, words.data(), words.size() * sizeof(uint32_t));
   *spirv_len = words.size();
   return true;
}

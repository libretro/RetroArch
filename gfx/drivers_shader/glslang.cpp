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

#include "glslang.hpp"

#ifdef HAVE_BUILTINGLSLANG
#include "../../deps/glslang/glslang/glslang/Public/ShaderLang.h"
#include "../../deps/glslang/glslang/SPIRV/GlslangToSpv.h"
#elif HAVE_GLSLANG
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#endif
#include <vector>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <mutex>

#include "../../verbosity.h"

struct SlangProcess
{
   public:
      SlangProcess();
      TBuiltInResource& GetResources() { return Resources; }

   private:
      TBuiltInResource Resources;
};

/* We don't use glslang from multiple threads, but to be sure.
 * Initializing TLS and freeing it for glslang works around
 * a really bizarre issue where the TLS key is suddenly
 * corrupted *somehow*.
 */
static std::mutex glslang_global_lock;

struct SlangProcessHolder
{
   SlangProcessHolder()
   {
      glslang_global_lock.lock();
      glslang::InitializeProcess();
   }

   ~SlangProcessHolder()
   {
      glslang::FinalizeProcess();
      glslang_global_lock.unlock();
   }
};

SlangProcess::SlangProcess()
{
   std::memset(&Resources, 0, sizeof(Resources));

   /* Integer resource limits */
   Resources.maxLights                                 = 32;
   Resources.maxClipPlanes                             = 6;
   Resources.maxTextureUnits                           = 32;
   Resources.maxTextureCoords                          = 32;
   Resources.maxVertexAttribs                          = 64;
   Resources.maxVertexUniformComponents                = 4096;
   Resources.maxVaryingFloats                          = 64;
   Resources.maxVertexTextureImageUnits                = 32;
   Resources.maxCombinedTextureImageUnits              = 80;
   Resources.maxTextureImageUnits                      = 32;
   Resources.maxFragmentUniformComponents              = 4096;
   Resources.maxDrawBuffers                            = 32;
   Resources.maxVertexUniformVectors                   = 128;
   Resources.maxVaryingVectors                         = 8;
   Resources.maxFragmentUniformVectors                 = 16;
   Resources.maxVertexOutputVectors                    = 16;
   Resources.maxFragmentInputVectors                   = 15;
   Resources.minProgramTexelOffset                     = -8;
   Resources.maxProgramTexelOffset                     = 7;
   Resources.maxClipDistances                          = 8;
   Resources.maxComputeWorkGroupCountX                 = 65535;
   Resources.maxComputeWorkGroupCountY                 = 65535;
   Resources.maxComputeWorkGroupCountZ                 = 65535;
   Resources.maxComputeWorkGroupSizeX                  = 1024;
   Resources.maxComputeWorkGroupSizeY                  = 1024;
   Resources.maxComputeWorkGroupSizeZ                  = 64;
   Resources.maxComputeUniformComponents               = 1024;
   Resources.maxComputeTextureImageUnits               = 16;
   Resources.maxComputeImageUniforms                   = 8;
   Resources.maxComputeAtomicCounters                  = 8;
   Resources.maxComputeAtomicCounterBuffers            = 1;
   Resources.maxVaryingComponents                      = 60;
   Resources.maxVertexOutputComponents                 = 64;
   Resources.maxGeometryInputComponents                = 64;
   Resources.maxGeometryOutputComponents               = 128;
   Resources.maxFragmentInputComponents                = 128;
   Resources.maxImageUnits                             = 8;
   Resources.maxCombinedImageUnitsAndFragmentOutputs   = 8;
   Resources.maxCombinedShaderOutputResources          = 8;
   Resources.maxImageSamples                           = 0;
   Resources.maxVertexImageUniforms                    = 0;
   Resources.maxTessControlImageUniforms               = 0;
   Resources.maxTessEvaluationImageUniforms            = 0;
   Resources.maxGeometryImageUniforms                  = 0;
   Resources.maxFragmentImageUniforms                  = 8;
   Resources.maxCombinedImageUniforms                  = 8;
   Resources.maxGeometryTextureImageUnits              = 16;
   Resources.maxGeometryOutputVertices                 = 256;
   Resources.maxGeometryTotalOutputComponents          = 1024;
   Resources.maxGeometryUniformComponents              = 1024;
   Resources.maxGeometryVaryingComponents              = 64;
   Resources.maxTessControlInputComponents             = 128;
   Resources.maxTessControlOutputComponents            = 128;
   Resources.maxTessControlTextureImageUnits           = 16;
   Resources.maxTessControlUniformComponents           = 1024;
   Resources.maxTessControlTotalOutputComponents       = 4096;
   Resources.maxTessEvaluationInputComponents          = 128;
   Resources.maxTessEvaluationOutputComponents         = 128;
   Resources.maxTessEvaluationTextureImageUnits        = 16;
   Resources.maxTessEvaluationUniformComponents        = 1024;
   Resources.maxTessPatchComponents                    = 120;
   Resources.maxPatchVertices                          = 32;
   Resources.maxTessGenLevel                           = 64;
   Resources.maxViewports                              = 16;
   Resources.maxVertexAtomicCounters                   = 0;
   Resources.maxTessControlAtomicCounters              = 0;
   Resources.maxTessEvaluationAtomicCounters           = 0;
   Resources.maxGeometryAtomicCounters                 = 0;
   Resources.maxFragmentAtomicCounters                 = 8;
   Resources.maxCombinedAtomicCounters                 = 8;
   Resources.maxAtomicCounterBindings                  = 1;
   Resources.maxVertexAtomicCounterBuffers             = 0;
   Resources.maxTessControlAtomicCounterBuffers        = 0;
   Resources.maxTessEvaluationAtomicCounterBuffers     = 0;
   Resources.maxGeometryAtomicCounterBuffers           = 0;
   Resources.maxFragmentAtomicCounterBuffers           = 1;
   Resources.maxCombinedAtomicCounterBuffers           = 1;
   Resources.maxAtomicCounterBufferSize                = 16384;
   Resources.maxTransformFeedbackBuffers               = 4;
   Resources.maxTransformFeedbackInterleavedComponents = 64;
   Resources.maxCullDistances                          = 8;
   Resources.maxCombinedClipAndCullDistances           = 8;
   Resources.maxSamples                                = 4;

   /* Boolean capability limits */
   Resources.limits.nonInductiveForLoops                       = true;
   Resources.limits.whileLoops                                 = true;
   Resources.limits.doWhileLoops                               = true;
   Resources.limits.generalUniformIndexing                     = true;
   Resources.limits.generalAttributeMatrixVectorIndexing       = true;
   Resources.limits.generalVaryingIndexing                     = true;
   Resources.limits.generalSamplerIndexing                     = true;
   Resources.limits.generalVariableIndexing                    = true;
   Resources.limits.generalConstantMatrixVectorIndexing        = true;
}

bool glslang::compile_spirv(const std::string &source, Stage stage,
      std::vector<uint32_t> *spirv)
{
   static SlangProcess process;
   SlangProcessHolder process_holder;
   TProgram program;

   EShLanguage language;
   switch (stage)
   {
      case StageVertex:         language = EShLangVertex;         break;
      case StageTessControl:    language = EShLangTessControl;    break;
      case StageTessEvaluation: language = EShLangTessEvaluation; break;
      case StageGeometry:       language = EShLangGeometry;       break;
      case StageFragment:       language = EShLangFragment;       break;
      case StageCompute:        language = EShLangCompute;        break;
      default:                  return false;
   }

   TShader shader(language);

   const char *src = source.c_str();
   shader.setStrings(&src, 1);

   glslang::TShader::ForbidIncluder forbid_include =
      glslang::TShader::ForbidIncluder();
   EShMessages messages = static_cast<EShMessages>(
         EShMsgDefault | EShMsgVulkanRules | EShMsgSpvRules);

   std::string preprocessed_msg;
   if (!shader.preprocess(&process.GetResources(),
            100, ENoProfile, false, false,
            messages, &preprocessed_msg,
            forbid_include))
   {
      RARCH_ERR("[Slang] %s\n", preprocessed_msg.c_str());
      return false;
   }

   if (!shader.parse(&process.GetResources(), 100, false, messages))
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

   GlslangToSpv(*program.getIntermediate(language), *spirv);
   return true;
}

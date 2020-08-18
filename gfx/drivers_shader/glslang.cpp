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

#include <string/stdstring.h>

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

using namespace glslang;
using namespace std;

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
      InitializeProcess();
   }

   ~SlangProcessHolder()
   {
      FinalizeProcess();
      glslang_global_lock.unlock();
   }
};

SlangProcess::SlangProcess()
{
   char DefaultConfig[] =
      "MaxLights 32\n"
      "MaxClipPlanes 6\n"
      "MaxTextureUnits 32\n"
      "MaxTextureCoords 32\n"
      "MaxVertexAttribs 64\n"
      "MaxVertexUniformComponents 4096\n"
      "MaxVaryingFloats 64\n"
      "MaxVertexTextureImageUnits 32\n"
      "MaxCombinedTextureImageUnits 80\n"
      "MaxTextureImageUnits 32\n"
      "MaxFragmentUniformComponents 4096\n"
      "MaxDrawBuffers 32\n"
      "MaxVertexUniformVectors 128\n"
      "MaxVaryingVectors 8\n"
      "MaxFragmentUniformVectors 16\n"
      "MaxVertexOutputVectors 16\n"
      "MaxFragmentInputVectors 15\n"
      "MinProgramTexelOffset -8\n"
      "MaxProgramTexelOffset 7\n"
      "MaxClipDistances 8\n"
      "MaxComputeWorkGroupCountX 65535\n"
      "MaxComputeWorkGroupCountY 65535\n"
      "MaxComputeWorkGroupCountZ 65535\n"
      "MaxComputeWorkGroupSizeX 1024\n"
      "MaxComputeWorkGroupSizeY 1024\n"
      "MaxComputeWorkGroupSizeZ 64\n"
      "MaxComputeUniformComponents 1024\n"
      "MaxComputeTextureImageUnits 16\n"
      "MaxComputeImageUniforms 8\n"
      "MaxComputeAtomicCounters 8\n"
      "MaxComputeAtomicCounterBuffers 1\n"
      "MaxVaryingComponents 60\n" 
      "MaxVertexOutputComponents 64\n"
      "MaxGeometryInputComponents 64\n"
      "MaxGeometryOutputComponents 128\n"
      "MaxFragmentInputComponents 128\n"
      "MaxImageUnits 8\n"
      "MaxCombinedImageUnitsAndFragmentOutputs 8\n"
      "MaxCombinedShaderOutputResources 8\n"
      "MaxImageSamples 0\n"
      "MaxVertexImageUniforms 0\n"
      "MaxTessControlImageUniforms 0\n"
      "MaxTessEvaluationImageUniforms 0\n"
      "MaxGeometryImageUniforms 0\n"
      "MaxFragmentImageUniforms 8\n"
      "MaxCombinedImageUniforms 8\n"
      "MaxGeometryTextureImageUnits 16\n"
      "MaxGeometryOutputVertices 256\n"
      "MaxGeometryTotalOutputComponents 1024\n"
      "MaxGeometryUniformComponents 1024\n"
      "MaxGeometryVaryingComponents 64\n"
      "MaxTessControlInputComponents 128\n"
      "MaxTessControlOutputComponents 128\n"
      "MaxTessControlTextureImageUnits 16\n"
      "MaxTessControlUniformComponents 1024\n"
      "MaxTessControlTotalOutputComponents 4096\n"
      "MaxTessEvaluationInputComponents 128\n"
      "MaxTessEvaluationOutputComponents 128\n"
      "MaxTessEvaluationTextureImageUnits 16\n"
      "MaxTessEvaluationUniformComponents 1024\n"
      "MaxTessPatchComponents 120\n"
      "MaxPatchVertices 32\n"
      "MaxTessGenLevel 64\n"
      "MaxViewports 16\n"
      "MaxVertexAtomicCounters 0\n"
      "MaxTessControlAtomicCounters 0\n"
      "MaxTessEvaluationAtomicCounters 0\n"
      "MaxGeometryAtomicCounters 0\n"
      "MaxFragmentAtomicCounters 8\n"
      "MaxCombinedAtomicCounters 8\n"
      "MaxAtomicCounterBindings 1\n"
      "MaxVertexAtomicCounterBuffers 0\n"
      "MaxTessControlAtomicCounterBuffers 0\n"
      "MaxTessEvaluationAtomicCounterBuffers 0\n"
      "MaxGeometryAtomicCounterBuffers 0\n"
      "MaxFragmentAtomicCounterBuffers 1\n"
      "MaxCombinedAtomicCounterBuffers 1\n"
      "MaxAtomicCounterBufferSize 16384\n"
      "MaxTransformFeedbackBuffers 4\n"
      "MaxTransformFeedbackInterleavedComponents 64\n"
      "MaxCullDistances 8\n"
      "MaxCombinedClipAndCullDistances 8\n"
      "MaxSamples 4\n"

      "nonInductiveForLoops 1\n"
      "whileLoops 1\n"
      "doWhileLoops 1\n"
      "generalUniformIndexing 1\n"
      "generalAttributeMatrixVectorIndexing 1\n"
      "generalVaryingIndexing 1\n"
      "generalSamplerIndexing 1\n"
      "generalVariableIndexing 1\n"
      "generalConstantMatrixVectorIndexing 1\n";

   const char *delims       = " \t\n\r";
   char *token              = strtok(DefaultConfig, delims);

   while (token)
   {
      const char *value_str = strtok(0, delims);
      int             value = (int)strtoul(value_str, nullptr, 0);

      if (string_starts_with_size(token, "Max", STRLEN_CONST("Max")))
      {
         if (string_starts_with_size(token, "MaxCompute", STRLEN_CONST("MaxCompute")))
         {
            if (string_starts_with_size(token, "MaxComputeWork", STRLEN_CONST("MaxComputeWork")))
            {
               if (string_is_equal(token, "MaxComputeWorkGroupCountX"))
                  Resources.maxComputeWorkGroupCountX = value;
               else if (string_is_equal(token, "MaxComputeWorkGroupCountY"))
                  Resources.maxComputeWorkGroupCountY = value;
               else if (string_is_equal(token, "MaxComputeWorkGroupCountZ"))
                  Resources.maxComputeWorkGroupCountZ = value;
               else if (string_is_equal(token, "MaxComputeWorkGroupSizeX"))
                  Resources.maxComputeWorkGroupSizeX = value;
               else if (string_is_equal(token, "MaxComputeWorkGroupSizeY"))
                  Resources.maxComputeWorkGroupSizeY = value;
               else if (string_is_equal(token, "MaxComputeWorkGroupSizeZ"))
                  Resources.maxComputeWorkGroupSizeZ = value;
            }
            else if (string_is_equal(token, "MaxComputeUniformComponents"))
               Resources.maxComputeUniformComponents = value;
            else if (string_is_equal(token, "MaxComputeTextureImageUnits"))
               Resources.maxComputeTextureImageUnits = value;
            else if (string_is_equal(token, "MaxComputeImageUniforms"))
               Resources.maxComputeImageUniforms = value;
            else if (string_is_equal(token, "MaxComputeAtomicCounters"))
               Resources.maxComputeAtomicCounters = value;
            else if (string_is_equal(token, "MaxComputeAtomicCounterBuffers"))
               Resources.maxComputeAtomicCounterBuffers = value;
         }
         else if (string_starts_with_size(token, "MaxVertex", STRLEN_CONST("MaxVertex")))
         {
            if (string_is_equal(token, "MaxVertexAttribs"))
               Resources.maxVertexAttribs = value;
            else if (string_is_equal(token, "MaxVertexUniformComponents"))
               Resources.maxVertexUniformComponents = value;
            else if (string_is_equal(token, "MaxVertexTextureImageUnits"))
               Resources.maxVertexTextureImageUnits = value;
            else if (string_is_equal(token, "MaxVertexUniformVectors"))
               Resources.maxVertexUniformVectors = value;
            else if (string_is_equal(token, "MaxVertexOutputVectors"))
               Resources.maxVertexOutputVectors = value;
            else if (string_is_equal(token, "MaxVertexOutputComponents"))
               Resources.maxVertexOutputComponents = value;
            else if (string_is_equal(token, "MaxVertexImageUniforms"))
               Resources.maxVertexImageUniforms = value;
            else if (string_is_equal(token, "MaxVertexAtomicCounters"))
               Resources.maxVertexAtomicCounters = value;
            else if (string_is_equal(token, "MaxVertexAtomicCounterBuffers"))
               Resources.maxVertexAtomicCounterBuffers = value;
         }
         else if (string_starts_with_size(token, "MaxTess", STRLEN_CONST("MaxTess")))
         {
            if (string_starts_with_size(token, "MaxTessEvaluation", STRLEN_CONST("MaxTessEvaluation")))
            {
               if (string_is_equal(token, "MaxTessEvaluationInputComponents"))
                  Resources.maxTessEvaluationInputComponents = value;
               else if (string_is_equal(token, "MaxTessEvaluationOutputComponents"))
                  Resources.maxTessEvaluationOutputComponents = value;
               else if (string_is_equal(token, "MaxTessEvaluationTextureImageUnits"))
                  Resources.maxTessEvaluationTextureImageUnits = value;
               else if (string_is_equal(token, "MaxTessEvaluationUniformComponents"))
                  Resources.maxTessEvaluationUniformComponents = value;
               else if (string_is_equal(token, "MaxTessEvaluationAtomicCounters"))
                  Resources.maxTessEvaluationAtomicCounters = value;
               else if (string_is_equal(token, "MaxTessEvaluationAtomicCounterBuffers"))
                  Resources.maxTessEvaluationAtomicCounterBuffers = value;
               else if (string_is_equal(token, "MaxTessEvaluationImageUniforms"))
                  Resources.maxTessEvaluationImageUniforms = value;
            }
            else if (string_starts_with_size(token, "MaxTessControl", STRLEN_CONST("MaxTessControl")))
            {
               if (string_is_equal(token, "MaxTessControlInputComponents"))
                  Resources.maxTessControlInputComponents = value;
               else if (string_is_equal(token, "MaxTessControlOutputComponents"))
                  Resources.maxTessControlOutputComponents = value;
               else if (string_is_equal(token, "MaxTessControlTextureImageUnits"))
                  Resources.maxTessControlTextureImageUnits = value;
               else if (string_is_equal(token, "MaxTessControlUniformComponents"))
                  Resources.maxTessControlUniformComponents = value;
               else if (string_is_equal(token, "MaxTessControlTotalOutputComponents"))
                  Resources.maxTessControlTotalOutputComponents = value;
               else if (string_is_equal(token, "MaxTessControlAtomicCounters"))
                  Resources.maxTessControlAtomicCounters = value;
               else if (string_is_equal(token, "MaxTessControlAtomicCounterBuffers"))
                  Resources.maxTessControlAtomicCounterBuffers = value;
               else if (string_is_equal(token, "MaxTessControlImageUniforms"))
                  Resources.maxTessControlImageUniforms = value;
            }
            else if (string_is_equal(token, "MaxTessPatchComponents"))
               Resources.maxTessPatchComponents = value;
            else if (string_is_equal(token, "MaxTessGenLevel"))
               Resources.maxTessGenLevel = value;
         }
         else if (string_starts_with_size(token, "MaxFragment", STRLEN_CONST("MaxFragment")))
         {
            if (string_is_equal(token, "MaxFragmentUniformComponents"))
               Resources.maxFragmentUniformComponents = value;
            else if (string_is_equal(token, "MaxFragmentUniformVectors"))
               Resources.maxFragmentUniformVectors = value;
            else if (string_is_equal(token, "MaxFragmentInputVectors"))
               Resources.maxFragmentInputVectors = value;
            else if (string_is_equal(token, "MaxFragmentInputComponents"))
               Resources.maxFragmentInputComponents = value;
            else if (string_is_equal(token, "MaxFragmentImageUniforms"))
               Resources.maxFragmentImageUniforms = value;
            else if (string_is_equal(token, "MaxFragmentAtomicCounters"))
               Resources.maxFragmentAtomicCounters = value;
            else if (string_is_equal(token, "MaxFragmentAtomicCounterBuffers"))
               Resources.maxFragmentAtomicCounterBuffers = value;
         }
         else if (string_is_equal(token, "MaxLights"))
            Resources.maxLights = value;
         else if (string_is_equal(token, "MaxClipPlanes"))
            Resources.maxClipPlanes = value;
         else if (string_is_equal(token, "MaxTextureUnits"))
            Resources.maxTextureUnits = value;
         else if (string_is_equal(token, "MaxTextureCoords"))
            Resources.maxTextureCoords = value;
         else if (string_is_equal(token, "MaxVaryingFloats"))
            Resources.maxVaryingFloats = value;
         else if (string_is_equal(token, "MaxCombinedTextureImageUnits"))
            Resources.maxCombinedTextureImageUnits = value;
         else if (string_is_equal(token, "MaxTextureImageUnits"))
            Resources.maxTextureImageUnits = value;
         else if (string_is_equal(token, "MaxDrawBuffers"))
            Resources.maxDrawBuffers = value;
         else if (string_is_equal(token, "MaxVaryingVectors"))
            Resources.maxVaryingVectors = value;
         else if (string_is_equal(token, "MaxProgramTexelOffset"))
            Resources.maxProgramTexelOffset = value;
         else if (string_is_equal(token, "MaxClipDistances"))
            Resources.maxClipDistances = value;
         else if (string_is_equal(token, "MaxVaryingComponents"))
            Resources.maxVaryingComponents = value;
         else if (string_is_equal(token, "MaxGeometryInputComponents"))
            Resources.maxGeometryInputComponents = value;
         else if (string_is_equal(token, "MaxGeometryOutputComponents"))
            Resources.maxGeometryOutputComponents = value;
         else if (string_is_equal(token, "MaxImageUnits"))
            Resources.maxImageUnits = value;
         else if (string_is_equal(token, "MaxCombinedImageUnitsAndFragmentOutputs"))
            Resources.maxCombinedImageUnitsAndFragmentOutputs = value;
         else if (string_is_equal(token, "MaxCombinedShaderOutputResources"))
            Resources.maxCombinedShaderOutputResources = value;
         else if (string_is_equal(token, "MaxImageSamples"))
            Resources.maxImageSamples = value;
         else if (string_is_equal(token, "MaxGeometryImageUniforms"))
            Resources.maxGeometryImageUniforms = value;
         else if (string_is_equal(token, "MaxCombinedImageUniforms"))
            Resources.maxCombinedImageUniforms = value;
         else if (string_is_equal(token, "MaxGeometryTextureImageUnits"))
            Resources.maxGeometryTextureImageUnits = value;
         else if (string_is_equal(token, "MaxGeometryOutputVertices"))
            Resources.maxGeometryOutputVertices = value;
         else if (string_is_equal(token, "MaxGeometryTotalOutputComponents"))
            Resources.maxGeometryTotalOutputComponents = value;
         else if (string_is_equal(token, "MaxGeometryUniformComponents"))
            Resources.maxGeometryUniformComponents = value;
         else if (string_is_equal(token, "MaxGeometryVaryingComponents"))
            Resources.maxGeometryVaryingComponents = value;
         else if (string_is_equal(token, "MaxPatchVertices"))
            Resources.maxPatchVertices = value;
         else if (string_is_equal(token, "MaxViewports"))
            Resources.maxViewports = value;
         else if (string_is_equal(token, "MaxGeometryAtomicCounters"))
            Resources.maxGeometryAtomicCounters = value;
         else if (string_is_equal(token, "MaxCombinedAtomicCounters"))
            Resources.maxCombinedAtomicCounters = value;
         else if (string_is_equal(token, "MaxAtomicCounterBindings"))
            Resources.maxAtomicCounterBindings = value;
         else if (string_is_equal(token, "MaxGeometryAtomicCounterBuffers"))
            Resources.maxGeometryAtomicCounterBuffers = value;
         else if (string_is_equal(token, "MaxCombinedAtomicCounterBuffers"))
            Resources.maxCombinedAtomicCounterBuffers = value;
         else if (string_is_equal(token, "MaxAtomicCounterBufferSize"))
            Resources.maxAtomicCounterBufferSize = value;
         else if (string_is_equal(token, "MaxTransformFeedbackBuffers"))
            Resources.maxTransformFeedbackBuffers = value;
         else if (string_is_equal(token, "MaxTransformFeedbackInterleavedComponents"))
            Resources.maxTransformFeedbackInterleavedComponents = value;
         else if (string_is_equal(token, "MaxCullDistances"))
            Resources.maxCullDistances = value;
         else if (string_is_equal(token, "MaxCombinedClipAndCullDistances"))
            Resources.maxCombinedClipAndCullDistances = value;
         else if (string_is_equal(token, "MaxSamples"))
            Resources.maxSamples = value;
      }
      else if (string_starts_with_size(token, "general", STRLEN_CONST("general")))
      {
         if (string_is_equal(token, "generalUniformIndexing"))
            Resources.limits.generalUniformIndexing = (value != 0);
         else if (string_is_equal(token, "generalAttributeMatrixVectorIndexing"))
            Resources.limits.generalAttributeMatrixVectorIndexing = (value != 0);
         else if (string_is_equal(token, "generalVaryingIndexing"))
            Resources.limits.generalVaryingIndexing = (value != 0);
         else if (string_is_equal(token, "generalSamplerIndexing"))
            Resources.limits.generalSamplerIndexing = (value != 0);
         else if (string_is_equal(token, "generalVariableIndexing"))
            Resources.limits.generalVariableIndexing = (value != 0);
         else if (string_is_equal(token, "generalConstantMatrixVectorIndexing"))
            Resources.limits.generalConstantMatrixVectorIndexing = (value != 0);
      }
      else if (string_is_equal(token, "MinProgramTexelOffset"))
         Resources.minProgramTexelOffset = value;
      else if (string_is_equal(token, "nonInductiveForLoops"))
         Resources.limits.nonInductiveForLoops = (value != 0);
      else if (string_is_equal(token, "whileLoops"))
         Resources.limits.whileLoops = (value != 0);
      else if (string_is_equal(token, "doWhileLoops"))
         Resources.limits.doWhileLoops = (value != 0);

      token = strtok(0, delims);
   }
}

bool glslang::compile_spirv(const string &source, Stage stage,
      std::vector<uint32_t> *spirv)
{
   string msg;
   static SlangProcess process;
   SlangProcessHolder process_holder;
   TProgram program;
   EShLanguage language;

   switch (stage)
   {
      case StageVertex:
         language = EShLangVertex;
         break;
      case StageTessControl:
         language = EShLangTessControl;
         break;
      case StageTessEvaluation:
         language = EShLangTessEvaluation;
         break;
      case StageGeometry:
         language = EShLangGeometry;
         break;
      case StageFragment:
         language = EShLangFragment;
         break;
      case StageCompute:
         language = EShLangCompute;
         break;
      default:
         return false;
   }
   TShader shader(language);

   const char *src = source.c_str();
   shader.setStrings(&src, 1);

   EShMessages messages = static_cast<EShMessages>(EShMsgDefault | EShMsgVulkanRules | EShMsgSpvRules);

   glslang::TShader::ForbidIncluder forbid_include = 
      glslang::TShader::ForbidIncluder();

   if (!shader.preprocess(&process.GetResources(),
            100, ENoProfile, false, false,
            messages, &msg, forbid_include))
   {
      RARCH_ERR("%s\n", msg.c_str());
      return false;
   }

   if (!shader.parse(&process.GetResources(), 100, false, messages))
   {
      RARCH_ERR("%s\n", shader.getInfoLog());
      RARCH_ERR("%s\n", shader.getInfoDebugLog());
      return false;
   }

   program.addShader(&shader);

   if (!program.link(messages))
   {
      RARCH_ERR("%s\n", program.getInfoLog());
      RARCH_ERR("%s\n", program.getInfoDebugLog());
      return false;
   }

   GlslangToSpv(*program.getIntermediate(language), *spirv);
   return true;
}

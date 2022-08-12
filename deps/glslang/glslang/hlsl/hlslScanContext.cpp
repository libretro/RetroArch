//
// Copyright (C) 2016 Google, Inc.
// Copyright (C) 2016 LunarG, Inc.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//
//    Neither the name of Google, Inc., nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

//
// HLSL scanning, leveraging the scanning done by the preprocessor.
//

#include <cstring>
#include <unordered_map>
#include <unordered_set>

#include "../glslang/Include/Types.h"
#include "../glslang/MachineIndependent/SymbolTable.h"
#include "../glslang/MachineIndependent/ParseHelper.h"
#include "hlslScanContext.h"
#include "hlslTokens.h"

// preprocessor includes
#include "../glslang/MachineIndependent/preprocessor/PpContext.h"
#include "../glslang/MachineIndependent/preprocessor/PpTokens.h"
#include "../glslang/MachineIndependent/preprocessor/Compare.h"

namespace {

// A single global usable by all threads, by all versions, by all languages.
// After a single process-level initialization, this is read only and thread safe
std::unordered_map<const char*, glslang::EHlslTokenClass, str_hash, str_eq>* hlslKeywordMap = nullptr;
std::unordered_set<const char*, str_hash, str_eq>* hlslReservedSet = nullptr;
std::unordered_map<const char*, glslang::TBuiltInVariable, str_hash, str_eq>* hlslSemanticMap = nullptr;

};

namespace glslang {

void HlslScanContext::fillInKeywordMap()
{
    if (hlslKeywordMap != nullptr) {
        // this is really an error, as this should called only once per process
        // but, the only risk is if two threads called simultaneously
        return;
    }
    hlslKeywordMap = new std::unordered_map<const char*, EHlslTokenClass, str_hash, str_eq>;

    (*hlslKeywordMap)["static"] =                  EHTokStatic;
    (*hlslKeywordMap)["const"] =                   EHTokConst;
    (*hlslKeywordMap)["unorm"] =                   EHTokUnorm;
    (*hlslKeywordMap)["snorm"] =                   EHTokSNorm;
    (*hlslKeywordMap)["extern"] =                  EHTokExtern;
    (*hlslKeywordMap)["uniform"] =                 EHTokUniform;
    (*hlslKeywordMap)["volatile"] =                EHTokVolatile;
    (*hlslKeywordMap)["precise"] =                 EHTokPrecise;
    (*hlslKeywordMap)["shared"] =                  EHTokShared;
    (*hlslKeywordMap)["groupshared"] =             EHTokGroupShared;
    (*hlslKeywordMap)["linear"] =                  EHTokLinear;
    (*hlslKeywordMap)["centroid"] =                EHTokCentroid;
    (*hlslKeywordMap)["nointerpolation"] =         EHTokNointerpolation;
    (*hlslKeywordMap)["noperspective"] =           EHTokNoperspective;
    (*hlslKeywordMap)["sample"] =                  EHTokSample;
    (*hlslKeywordMap)["row_major"] =               EHTokRowMajor;
    (*hlslKeywordMap)["column_major"] =            EHTokColumnMajor;
    (*hlslKeywordMap)["packoffset"] =              EHTokPackOffset;
    (*hlslKeywordMap)["in"] =                      EHTokIn;
    (*hlslKeywordMap)["out"] =                     EHTokOut;
    (*hlslKeywordMap)["inout"] =                   EHTokInOut;
    (*hlslKeywordMap)["layout"] =                  EHTokLayout;
    (*hlslKeywordMap)["globallycoherent"] =        EHTokGloballyCoherent;
    (*hlslKeywordMap)["inline"] =                  EHTokInline;

    (*hlslKeywordMap)["point"] =                   EHTokPoint;
    (*hlslKeywordMap)["line"] =                    EHTokLine;
    (*hlslKeywordMap)["triangle"] =                EHTokTriangle;
    (*hlslKeywordMap)["lineadj"] =                 EHTokLineAdj;
    (*hlslKeywordMap)["triangleadj"] =             EHTokTriangleAdj;

    (*hlslKeywordMap)["PointStream"] =             EHTokPointStream;
    (*hlslKeywordMap)["LineStream"] =              EHTokLineStream;
    (*hlslKeywordMap)["TriangleStream"] =          EHTokTriangleStream;

    (*hlslKeywordMap)["InputPatch"] =              EHTokInputPatch;
    (*hlslKeywordMap)["OutputPatch"] =             EHTokOutputPatch;

    (*hlslKeywordMap)["Buffer"] =                  EHTokBuffer;
    (*hlslKeywordMap)["vector"] =                  EHTokVector;
    (*hlslKeywordMap)["matrix"] =                  EHTokMatrix;

    (*hlslKeywordMap)["void"] =                    EHTokVoid;
    (*hlslKeywordMap)["string"] =                  EHTokString;
    (*hlslKeywordMap)["bool"] =                    EHTokBool;
    (*hlslKeywordMap)["int"] =                     EHTokInt;
    (*hlslKeywordMap)["uint"] =                    EHTokUint;
    (*hlslKeywordMap)["uint64_t"] =                EHTokUint64;
    (*hlslKeywordMap)["dword"] =                   EHTokDword;
    (*hlslKeywordMap)["half"] =                    EHTokHalf;
    (*hlslKeywordMap)["float"] =                   EHTokFloat;
    (*hlslKeywordMap)["double"] =                  EHTokDouble;
    (*hlslKeywordMap)["min16float"] =              EHTokMin16float;
    (*hlslKeywordMap)["min10float"] =              EHTokMin10float;
    (*hlslKeywordMap)["min16int"] =                EHTokMin16int;
    (*hlslKeywordMap)["min12int"] =                EHTokMin12int;
    (*hlslKeywordMap)["min16uint"] =               EHTokMin16uint;

    (*hlslKeywordMap)["bool1"] =                   EHTokBool1;
    (*hlslKeywordMap)["bool2"] =                   EHTokBool2;
    (*hlslKeywordMap)["bool3"] =                   EHTokBool3;
    (*hlslKeywordMap)["bool4"] =                   EHTokBool4;
    (*hlslKeywordMap)["float1"] =                  EHTokFloat1;
    (*hlslKeywordMap)["float2"] =                  EHTokFloat2;
    (*hlslKeywordMap)["float3"] =                  EHTokFloat3;
    (*hlslKeywordMap)["float4"] =                  EHTokFloat4;
    (*hlslKeywordMap)["int1"] =                    EHTokInt1;
    (*hlslKeywordMap)["int2"] =                    EHTokInt2;
    (*hlslKeywordMap)["int3"] =                    EHTokInt3;
    (*hlslKeywordMap)["int4"] =                    EHTokInt4;
    (*hlslKeywordMap)["double1"] =                 EHTokDouble1;
    (*hlslKeywordMap)["double2"] =                 EHTokDouble2;
    (*hlslKeywordMap)["double3"] =                 EHTokDouble3;
    (*hlslKeywordMap)["double4"] =                 EHTokDouble4;
    (*hlslKeywordMap)["uint1"] =                   EHTokUint1;
    (*hlslKeywordMap)["uint2"] =                   EHTokUint2;
    (*hlslKeywordMap)["uint3"] =                   EHTokUint3;
    (*hlslKeywordMap)["uint4"] =                   EHTokUint4;

    (*hlslKeywordMap)["half1"] =                   EHTokHalf1;
    (*hlslKeywordMap)["half2"] =                   EHTokHalf2;
    (*hlslKeywordMap)["half3"] =                   EHTokHalf3;
    (*hlslKeywordMap)["half4"] =                   EHTokHalf4;
    (*hlslKeywordMap)["min16float1"] =             EHTokMin16float1;
    (*hlslKeywordMap)["min16float2"] =             EHTokMin16float2;
    (*hlslKeywordMap)["min16float3"] =             EHTokMin16float3;
    (*hlslKeywordMap)["min16float4"] =             EHTokMin16float4;
    (*hlslKeywordMap)["min10float1"] =             EHTokMin10float1;
    (*hlslKeywordMap)["min10float2"] =             EHTokMin10float2;
    (*hlslKeywordMap)["min10float3"] =             EHTokMin10float3;
    (*hlslKeywordMap)["min10float4"] =             EHTokMin10float4;
    (*hlslKeywordMap)["min16int1"] =               EHTokMin16int1;
    (*hlslKeywordMap)["min16int2"] =               EHTokMin16int2;
    (*hlslKeywordMap)["min16int3"] =               EHTokMin16int3;
    (*hlslKeywordMap)["min16int4"] =               EHTokMin16int4;
    (*hlslKeywordMap)["min12int1"] =               EHTokMin12int1;
    (*hlslKeywordMap)["min12int2"] =               EHTokMin12int2;
    (*hlslKeywordMap)["min12int3"] =               EHTokMin12int3;
    (*hlslKeywordMap)["min12int4"] =               EHTokMin12int4;
    (*hlslKeywordMap)["min16uint1"] =              EHTokMin16uint1;
    (*hlslKeywordMap)["min16uint2"] =              EHTokMin16uint2;
    (*hlslKeywordMap)["min16uint3"] =              EHTokMin16uint3;
    (*hlslKeywordMap)["min16uint4"] =              EHTokMin16uint4;

    (*hlslKeywordMap)["bool1x1"] =                 EHTokBool1x1;
    (*hlslKeywordMap)["bool1x2"] =                 EHTokBool1x2;
    (*hlslKeywordMap)["bool1x3"] =                 EHTokBool1x3;
    (*hlslKeywordMap)["bool1x4"] =                 EHTokBool1x4;
    (*hlslKeywordMap)["bool2x1"] =                 EHTokBool2x1;
    (*hlslKeywordMap)["bool2x2"] =                 EHTokBool2x2;
    (*hlslKeywordMap)["bool2x3"] =                 EHTokBool2x3;
    (*hlslKeywordMap)["bool2x4"] =                 EHTokBool2x4;
    (*hlslKeywordMap)["bool3x1"] =                 EHTokBool3x1;
    (*hlslKeywordMap)["bool3x2"] =                 EHTokBool3x2;
    (*hlslKeywordMap)["bool3x3"] =                 EHTokBool3x3;
    (*hlslKeywordMap)["bool3x4"] =                 EHTokBool3x4;
    (*hlslKeywordMap)["bool4x1"] =                 EHTokBool4x1;
    (*hlslKeywordMap)["bool4x2"] =                 EHTokBool4x2;
    (*hlslKeywordMap)["bool4x3"] =                 EHTokBool4x3;
    (*hlslKeywordMap)["bool4x4"] =                 EHTokBool4x4;
    (*hlslKeywordMap)["int1x1"] =                  EHTokInt1x1;
    (*hlslKeywordMap)["int1x2"] =                  EHTokInt1x2;
    (*hlslKeywordMap)["int1x3"] =                  EHTokInt1x3;
    (*hlslKeywordMap)["int1x4"] =                  EHTokInt1x4;
    (*hlslKeywordMap)["int2x1"] =                  EHTokInt2x1;
    (*hlslKeywordMap)["int2x2"] =                  EHTokInt2x2;
    (*hlslKeywordMap)["int2x3"] =                  EHTokInt2x3;
    (*hlslKeywordMap)["int2x4"] =                  EHTokInt2x4;
    (*hlslKeywordMap)["int3x1"] =                  EHTokInt3x1;
    (*hlslKeywordMap)["int3x2"] =                  EHTokInt3x2;
    (*hlslKeywordMap)["int3x3"] =                  EHTokInt3x3;
    (*hlslKeywordMap)["int3x4"] =                  EHTokInt3x4;
    (*hlslKeywordMap)["int4x1"] =                  EHTokInt4x1;
    (*hlslKeywordMap)["int4x2"] =                  EHTokInt4x2;
    (*hlslKeywordMap)["int4x3"] =                  EHTokInt4x3;
    (*hlslKeywordMap)["int4x4"] =                  EHTokInt4x4;
    (*hlslKeywordMap)["uint1x1"] =                 EHTokUint1x1;
    (*hlslKeywordMap)["uint1x2"] =                 EHTokUint1x2;
    (*hlslKeywordMap)["uint1x3"] =                 EHTokUint1x3;
    (*hlslKeywordMap)["uint1x4"] =                 EHTokUint1x4;
    (*hlslKeywordMap)["uint2x1"] =                 EHTokUint2x1;
    (*hlslKeywordMap)["uint2x2"] =                 EHTokUint2x2;
    (*hlslKeywordMap)["uint2x3"] =                 EHTokUint2x3;
    (*hlslKeywordMap)["uint2x4"] =                 EHTokUint2x4;
    (*hlslKeywordMap)["uint3x1"] =                 EHTokUint3x1;
    (*hlslKeywordMap)["uint3x2"] =                 EHTokUint3x2;
    (*hlslKeywordMap)["uint3x3"] =                 EHTokUint3x3;
    (*hlslKeywordMap)["uint3x4"] =                 EHTokUint3x4;
    (*hlslKeywordMap)["uint4x1"] =                 EHTokUint4x1;
    (*hlslKeywordMap)["uint4x2"] =                 EHTokUint4x2;
    (*hlslKeywordMap)["uint4x3"] =                 EHTokUint4x3;
    (*hlslKeywordMap)["uint4x4"] =                 EHTokUint4x4;
    (*hlslKeywordMap)["bool1x1"] =                 EHTokBool1x1;
    (*hlslKeywordMap)["bool1x2"] =                 EHTokBool1x2;
    (*hlslKeywordMap)["bool1x3"] =                 EHTokBool1x3;
    (*hlslKeywordMap)["bool1x4"] =                 EHTokBool1x4;
    (*hlslKeywordMap)["bool2x1"] =                 EHTokBool2x1;
    (*hlslKeywordMap)["bool2x2"] =                 EHTokBool2x2;
    (*hlslKeywordMap)["bool2x3"] =                 EHTokBool2x3;
    (*hlslKeywordMap)["bool2x4"] =                 EHTokBool2x4;
    (*hlslKeywordMap)["bool3x1"] =                 EHTokBool3x1;
    (*hlslKeywordMap)["bool3x2"] =                 EHTokBool3x2;
    (*hlslKeywordMap)["bool3x3"] =                 EHTokBool3x3;
    (*hlslKeywordMap)["bool3x4"] =                 EHTokBool3x4;
    (*hlslKeywordMap)["bool4x1"] =                 EHTokBool4x1;
    (*hlslKeywordMap)["bool4x2"] =                 EHTokBool4x2;
    (*hlslKeywordMap)["bool4x3"] =                 EHTokBool4x3;
    (*hlslKeywordMap)["bool4x4"] =                 EHTokBool4x4;
    (*hlslKeywordMap)["float1x1"] =                EHTokFloat1x1;
    (*hlslKeywordMap)["float1x2"] =                EHTokFloat1x2;
    (*hlslKeywordMap)["float1x3"] =                EHTokFloat1x3;
    (*hlslKeywordMap)["float1x4"] =                EHTokFloat1x4;
    (*hlslKeywordMap)["float2x1"] =                EHTokFloat2x1;
    (*hlslKeywordMap)["float2x2"] =                EHTokFloat2x2;
    (*hlslKeywordMap)["float2x3"] =                EHTokFloat2x3;
    (*hlslKeywordMap)["float2x4"] =                EHTokFloat2x4;
    (*hlslKeywordMap)["float3x1"] =                EHTokFloat3x1;
    (*hlslKeywordMap)["float3x2"] =                EHTokFloat3x2;
    (*hlslKeywordMap)["float3x3"] =                EHTokFloat3x3;
    (*hlslKeywordMap)["float3x4"] =                EHTokFloat3x4;
    (*hlslKeywordMap)["float4x1"] =                EHTokFloat4x1;
    (*hlslKeywordMap)["float4x2"] =                EHTokFloat4x2;
    (*hlslKeywordMap)["float4x3"] =                EHTokFloat4x3;
    (*hlslKeywordMap)["float4x4"] =                EHTokFloat4x4;
    (*hlslKeywordMap)["half1x1"] =                 EHTokHalf1x1;
    (*hlslKeywordMap)["half1x2"] =                 EHTokHalf1x2;
    (*hlslKeywordMap)["half1x3"] =                 EHTokHalf1x3;
    (*hlslKeywordMap)["half1x4"] =                 EHTokHalf1x4;
    (*hlslKeywordMap)["half2x1"] =                 EHTokHalf2x1;
    (*hlslKeywordMap)["half2x2"] =                 EHTokHalf2x2;
    (*hlslKeywordMap)["half2x3"] =                 EHTokHalf2x3;
    (*hlslKeywordMap)["half2x4"] =                 EHTokHalf2x4;
    (*hlslKeywordMap)["half3x1"] =                 EHTokHalf3x1;
    (*hlslKeywordMap)["half3x2"] =                 EHTokHalf3x2;
    (*hlslKeywordMap)["half3x3"] =                 EHTokHalf3x3;
    (*hlslKeywordMap)["half3x4"] =                 EHTokHalf3x4;
    (*hlslKeywordMap)["half4x1"] =                 EHTokHalf4x1;
    (*hlslKeywordMap)["half4x2"] =                 EHTokHalf4x2;
    (*hlslKeywordMap)["half4x3"] =                 EHTokHalf4x3;
    (*hlslKeywordMap)["half4x4"] =                 EHTokHalf4x4;
    (*hlslKeywordMap)["double1x1"] =               EHTokDouble1x1;
    (*hlslKeywordMap)["double1x2"] =               EHTokDouble1x2;
    (*hlslKeywordMap)["double1x3"] =               EHTokDouble1x3;
    (*hlslKeywordMap)["double1x4"] =               EHTokDouble1x4;
    (*hlslKeywordMap)["double2x1"] =               EHTokDouble2x1;
    (*hlslKeywordMap)["double2x2"] =               EHTokDouble2x2;
    (*hlslKeywordMap)["double2x3"] =               EHTokDouble2x3;
    (*hlslKeywordMap)["double2x4"] =               EHTokDouble2x4;
    (*hlslKeywordMap)["double3x1"] =               EHTokDouble3x1;
    (*hlslKeywordMap)["double3x2"] =               EHTokDouble3x2;
    (*hlslKeywordMap)["double3x3"] =               EHTokDouble3x3;
    (*hlslKeywordMap)["double3x4"] =               EHTokDouble3x4;
    (*hlslKeywordMap)["double4x1"] =               EHTokDouble4x1;
    (*hlslKeywordMap)["double4x2"] =               EHTokDouble4x2;
    (*hlslKeywordMap)["double4x3"] =               EHTokDouble4x3;
    (*hlslKeywordMap)["double4x4"] =               EHTokDouble4x4;

    (*hlslKeywordMap)["sampler"] =                 EHTokSampler;
    (*hlslKeywordMap)["sampler1D"] =               EHTokSampler1d;
    (*hlslKeywordMap)["sampler2D"] =               EHTokSampler2d;
    (*hlslKeywordMap)["sampler3D"] =               EHTokSampler3d;
    (*hlslKeywordMap)["samplerCube"] =             EHTokSamplerCube;
    (*hlslKeywordMap)["sampler_state"] =           EHTokSamplerState;
    (*hlslKeywordMap)["SamplerState"] =            EHTokSamplerState;
    (*hlslKeywordMap)["SamplerComparisonState"] =  EHTokSamplerComparisonState;
    (*hlslKeywordMap)["texture"] =                 EHTokTexture;
    (*hlslKeywordMap)["Texture1D"] =               EHTokTexture1d;
    (*hlslKeywordMap)["Texture1DArray"] =          EHTokTexture1darray;
    (*hlslKeywordMap)["Texture2D"] =               EHTokTexture2d;
    (*hlslKeywordMap)["Texture2DArray"] =          EHTokTexture2darray;
    (*hlslKeywordMap)["Texture3D"] =               EHTokTexture3d;
    (*hlslKeywordMap)["TextureCube"] =             EHTokTextureCube;
    (*hlslKeywordMap)["TextureCubeArray"] =        EHTokTextureCubearray;
    (*hlslKeywordMap)["Texture2DMS"] =             EHTokTexture2DMS;
    (*hlslKeywordMap)["Texture2DMSArray"] =        EHTokTexture2DMSarray;
    (*hlslKeywordMap)["RWTexture1D"] =             EHTokRWTexture1d;
    (*hlslKeywordMap)["RWTexture1DArray"] =        EHTokRWTexture1darray;
    (*hlslKeywordMap)["RWTexture2D"] =             EHTokRWTexture2d;
    (*hlslKeywordMap)["RWTexture2DArray"] =        EHTokRWTexture2darray;
    (*hlslKeywordMap)["RWTexture3D"] =             EHTokRWTexture3d;
    (*hlslKeywordMap)["RWBuffer"] =                EHTokRWBuffer;
    (*hlslKeywordMap)["SubpassInput"] =            EHTokSubpassInput;
    (*hlslKeywordMap)["SubpassInputMS"] =          EHTokSubpassInputMS;

    (*hlslKeywordMap)["AppendStructuredBuffer"] =  EHTokAppendStructuredBuffer;
    (*hlslKeywordMap)["ByteAddressBuffer"] =       EHTokByteAddressBuffer;
    (*hlslKeywordMap)["ConsumeStructuredBuffer"] = EHTokConsumeStructuredBuffer;
    (*hlslKeywordMap)["RWByteAddressBuffer"] =     EHTokRWByteAddressBuffer;
    (*hlslKeywordMap)["RWStructuredBuffer"] =      EHTokRWStructuredBuffer;
    (*hlslKeywordMap)["StructuredBuffer"] =        EHTokStructuredBuffer;
    (*hlslKeywordMap)["TextureBuffer"] =           EHTokTextureBuffer;

    (*hlslKeywordMap)["class"] =                   EHTokClass;
    (*hlslKeywordMap)["struct"] =                  EHTokStruct;
    (*hlslKeywordMap)["cbuffer"] =                 EHTokCBuffer;
    (*hlslKeywordMap)["ConstantBuffer"] =          EHTokConstantBuffer;
    (*hlslKeywordMap)["tbuffer"] =                 EHTokTBuffer;
    (*hlslKeywordMap)["typedef"] =                 EHTokTypedef;
    (*hlslKeywordMap)["this"] =                    EHTokThis;
    (*hlslKeywordMap)["namespace"] =               EHTokNamespace;

    (*hlslKeywordMap)["true"] =                    EHTokBoolConstant;
    (*hlslKeywordMap)["false"] =                   EHTokBoolConstant;

    (*hlslKeywordMap)["for"] =                     EHTokFor;
    (*hlslKeywordMap)["do"] =                      EHTokDo;
    (*hlslKeywordMap)["while"] =                   EHTokWhile;
    (*hlslKeywordMap)["break"] =                   EHTokBreak;
    (*hlslKeywordMap)["continue"] =                EHTokContinue;
    (*hlslKeywordMap)["if"] =                      EHTokIf;
    (*hlslKeywordMap)["else"] =                    EHTokElse;
    (*hlslKeywordMap)["discard"] =                 EHTokDiscard;
    (*hlslKeywordMap)["return"] =                  EHTokReturn;
    (*hlslKeywordMap)["switch"] =                  EHTokSwitch;
    (*hlslKeywordMap)["case"] =                    EHTokCase;
    (*hlslKeywordMap)["default"] =                 EHTokDefault;

    // TODO: get correct set here
    hlslReservedSet = new std::unordered_set<const char*, str_hash, str_eq>;

    hlslReservedSet->insert("auto");
    hlslReservedSet->insert("catch");
    hlslReservedSet->insert("char");
    hlslReservedSet->insert("const_cast");
    hlslReservedSet->insert("enum");
    hlslReservedSet->insert("explicit");
    hlslReservedSet->insert("friend");
    hlslReservedSet->insert("goto");
    hlslReservedSet->insert("long");
    hlslReservedSet->insert("mutable");
    hlslReservedSet->insert("new");
    hlslReservedSet->insert("operator");
    hlslReservedSet->insert("private");
    hlslReservedSet->insert("protected");
    hlslReservedSet->insert("public");
    hlslReservedSet->insert("reinterpret_cast");
    hlslReservedSet->insert("short");
    hlslReservedSet->insert("signed");
    hlslReservedSet->insert("sizeof");
    hlslReservedSet->insert("static_cast");
    hlslReservedSet->insert("template");
    hlslReservedSet->insert("throw");
    hlslReservedSet->insert("try");
    hlslReservedSet->insert("typename");
    hlslReservedSet->insert("union");
    hlslReservedSet->insert("unsigned");
    hlslReservedSet->insert("using");
    hlslReservedSet->insert("virtual");

    hlslSemanticMap = new std::unordered_map<const char*, glslang::TBuiltInVariable, str_hash, str_eq>;

    // in DX9, all outputs had to have a semantic associated with them, that was either consumed
    // by the system or was a specific register assignment
    // in DX10+, only semantics with the SV_ prefix have any meaning beyond decoration
    // Fxc will only accept DX9 style semantics in compat mode
    // Also, in DX10 if a SV value is present as the input of a stage, but isn't appropriate for that
    // stage, it would just be ignored as it is likely there as part of an output struct from one stage
    // to the next
    bool bParseDX9 = false;
    if (bParseDX9) {
        (*hlslSemanticMap)["PSIZE"] = EbvPointSize;
        (*hlslSemanticMap)["FOG"] =   EbvFogFragCoord;
        (*hlslSemanticMap)["DEPTH"] = EbvFragDepth;
        (*hlslSemanticMap)["VFACE"] = EbvFace;
        (*hlslSemanticMap)["VPOS"] =  EbvFragCoord;
    }

    (*hlslSemanticMap)["SV_POSITION"] =               EbvPosition;
    (*hlslSemanticMap)["SV_VERTEXID"] =               EbvVertexIndex;
    (*hlslSemanticMap)["SV_VIEWPORTARRAYINDEX"] =     EbvViewportIndex;
    (*hlslSemanticMap)["SV_TESSFACTOR"] =             EbvTessLevelOuter;
    (*hlslSemanticMap)["SV_SAMPLEINDEX"] =            EbvSampleId;
    (*hlslSemanticMap)["SV_RENDERTARGETARRAYINDEX"] = EbvLayer;
    (*hlslSemanticMap)["SV_PRIMITIVEID"] =            EbvPrimitiveId;
    (*hlslSemanticMap)["SV_OUTPUTCONTROLPOINTID"] =   EbvInvocationId;
    (*hlslSemanticMap)["SV_ISFRONTFACE"] =            EbvFace;
    (*hlslSemanticMap)["SV_INSTANCEID"] =             EbvInstanceIndex;
    (*hlslSemanticMap)["SV_INSIDETESSFACTOR"] =       EbvTessLevelInner;
    (*hlslSemanticMap)["SV_GSINSTANCEID"] =           EbvInvocationId;
    (*hlslSemanticMap)["SV_DISPATCHTHREADID"] =       EbvGlobalInvocationId;
    (*hlslSemanticMap)["SV_GROUPTHREADID"] =          EbvLocalInvocationId;
    (*hlslSemanticMap)["SV_GROUPINDEX"] =             EbvLocalInvocationIndex;
    (*hlslSemanticMap)["SV_GROUPID"] =                EbvWorkGroupId;
    (*hlslSemanticMap)["SV_DOMAINLOCATION"] =         EbvTessCoord;
    (*hlslSemanticMap)["SV_DEPTH"] =                  EbvFragDepth;
    (*hlslSemanticMap)["SV_COVERAGE"] =               EbvSampleMask;
    (*hlslSemanticMap)["SV_DEPTHGREATEREQUAL"] =      EbvFragDepthGreater;
    (*hlslSemanticMap)["SV_DEPTHLESSEQUAL"] =         EbvFragDepthLesser;
    (*hlslSemanticMap)["SV_STENCILREF"] =             EbvFragStencilRef;
}

void HlslScanContext::deleteKeywordMap()
{
    delete hlslKeywordMap;
    hlslKeywordMap = nullptr;
    delete hlslReservedSet;
    hlslReservedSet = nullptr;
    delete hlslSemanticMap;
    hlslSemanticMap = nullptr;
}

// Wrapper for tokenizeClass() to get everything inside the token.
void HlslScanContext::tokenize(HlslToken& token)
{
    EHlslTokenClass tokenClass = tokenizeClass(token);
    token.tokenClass = tokenClass;
}

glslang::TBuiltInVariable HlslScanContext::mapSemantic(const char* upperCase)
{
    auto it = hlslSemanticMap->find(upperCase);
    if (it != hlslSemanticMap->end())
        return it->second;
    else
        return glslang::EbvNone;
}

//
// Fill in token information for the next token, except for the token class.
// Returns the enum value of the token class of the next token found.
// Return 0 (EndOfTokens) on end of input.
//
EHlslTokenClass HlslScanContext::tokenizeClass(HlslToken& token)
{
    do {
        parserToken = &token;
        TPpToken ppToken;
        int token = ppContext.tokenize(ppToken);
        if (token == EndOfInput)
            return EHTokNone;

        tokenText = ppToken.name;
        loc = ppToken.loc;
        parserToken->loc = loc;
        switch (token) {
        case ';':                       return EHTokSemicolon;
        case ',':                       return EHTokComma;
        case ':':                       return EHTokColon;
        case '=':                       return EHTokAssign;
        case '(':                       return EHTokLeftParen;
        case ')':                       return EHTokRightParen;
        case '.':                       return EHTokDot;
        case '!':                       return EHTokBang;
        case '-':                       return EHTokDash;
        case '~':                       return EHTokTilde;
        case '+':                       return EHTokPlus;
        case '*':                       return EHTokStar;
        case '/':                       return EHTokSlash;
        case '%':                       return EHTokPercent;
        case '<':                       return EHTokLeftAngle;
        case '>':                       return EHTokRightAngle;
        case '|':                       return EHTokVerticalBar;
        case '^':                       return EHTokCaret;
        case '&':                       return EHTokAmpersand;
        case '?':                       return EHTokQuestion;
        case '[':                       return EHTokLeftBracket;
        case ']':                       return EHTokRightBracket;
        case '{':                       return EHTokLeftBrace;
        case '}':                       return EHTokRightBrace;
        case '\\':
            _parseContext.error(loc, "illegal use of escape character", "\\", "");
            break;

        case PPAtomAddAssign:          return EHTokAddAssign;
        case PPAtomSubAssign:          return EHTokSubAssign;
        case PPAtomMulAssign:          return EHTokMulAssign;
        case PPAtomDivAssign:          return EHTokDivAssign;
        case PPAtomModAssign:          return EHTokModAssign;

        case PpAtomRight:              return EHTokRightOp;
        case PpAtomLeft:               return EHTokLeftOp;

        case PpAtomRightAssign:        return EHTokRightAssign;
        case PpAtomLeftAssign:         return EHTokLeftAssign;
        case PpAtomAndAssign:          return EHTokAndAssign;
        case PpAtomOrAssign:           return EHTokOrAssign;
        case PpAtomXorAssign:          return EHTokXorAssign;

        case PpAtomAnd:                return EHTokAndOp;
        case PpAtomOr:                 return EHTokOrOp;
        case PpAtomXor:                return EHTokXorOp;

        case PpAtomEQ:                 return EHTokEqOp;
        case PpAtomGE:                 return EHTokGeOp;
        case PpAtomNE:                 return EHTokNeOp;
        case PpAtomLE:                 return EHTokLeOp;

        case PpAtomDecrement:          return EHTokDecOp;
        case PpAtomIncrement:          return EHTokIncOp;

        case PpAtomColonColon:         return EHTokColonColon;

        case PpAtomConstInt:           parserToken->i = ppToken.ival;       return EHTokIntConstant;
        case PpAtomConstUint:          parserToken->i = ppToken.ival;       return EHTokUintConstant;
        case PpAtomConstFloat16:       parserToken->d = ppToken.dval;       return EHTokFloat16Constant;
        case PpAtomConstFloat:         parserToken->d = ppToken.dval;       return EHTokFloatConstant;
        case PpAtomConstDouble:        parserToken->d = ppToken.dval;       return EHTokDoubleConstant;
        case PpAtomIdentifier:
        {
            EHlslTokenClass token = tokenizeIdentifier();
            return token;
        }

        case PpAtomConstString: {
            parserToken->string = NewPoolTString(tokenText);
            return EHTokStringConstant;
        }

        case EndOfInput:               return EHTokNone;

        default:
            if (token < PpAtomMaxSingle) {
                char buf[2];
                buf[0] = (char)token;
                buf[1] = 0;
                _parseContext.error(loc, "unexpected token", buf, "");
            } else if (tokenText[0] != 0)
                _parseContext.error(loc, "unexpected token", tokenText, "");
            else
                _parseContext.error(loc, "unexpected token", "", "");
            break;
        }
    } while (true);
}

EHlslTokenClass HlslScanContext::tokenizeIdentifier()
{
    if (hlslReservedSet->find(tokenText) != hlslReservedSet->end())
        return reservedWord();

    auto it = hlslKeywordMap->find(tokenText);
    if (it == hlslKeywordMap->end()) {
        // Should have an identifier of some sort
        return identifierOrType();
    }
    keyword = it->second;

    switch (keyword) {

    // qualifiers
    case EHTokStatic:
    case EHTokConst:
    case EHTokSNorm:
    case EHTokUnorm:
    case EHTokExtern:
    case EHTokUniform:
    case EHTokVolatile:
    case EHTokShared:
    case EHTokGroupShared:
    case EHTokLinear:
    case EHTokCentroid:
    case EHTokNointerpolation:
    case EHTokNoperspective:
    case EHTokSample:
    case EHTokRowMajor:
    case EHTokColumnMajor:
    case EHTokPackOffset:
    case EHTokIn:
    case EHTokOut:
    case EHTokInOut:
    case EHTokPrecise:
    case EHTokLayout:
    case EHTokGloballyCoherent:
    case EHTokInline:
        return keyword;

    // primitive types
    case EHTokPoint:
    case EHTokLine:
    case EHTokTriangle:
    case EHTokLineAdj:
    case EHTokTriangleAdj:
        return keyword;

    // stream out types
    case EHTokPointStream:
    case EHTokLineStream:
    case EHTokTriangleStream:
        return keyword;

    // Tessellation patches
    case EHTokInputPatch:
    case EHTokOutputPatch:
        return keyword;

    case EHTokBuffer:
    case EHTokVector:
    case EHTokMatrix:
        return keyword;

    // scalar types
    case EHTokVoid:
    case EHTokString:
    case EHTokBool:
    case EHTokInt:
    case EHTokUint:
    case EHTokUint64:
    case EHTokDword:
    case EHTokHalf:
    case EHTokFloat:
    case EHTokDouble:
    case EHTokMin16float:
    case EHTokMin10float:
    case EHTokMin16int:
    case EHTokMin12int:
    case EHTokMin16uint:

    // vector types
    case EHTokBool1:
    case EHTokBool2:
    case EHTokBool3:
    case EHTokBool4:
    case EHTokFloat1:
    case EHTokFloat2:
    case EHTokFloat3:
    case EHTokFloat4:
    case EHTokInt1:
    case EHTokInt2:
    case EHTokInt3:
    case EHTokInt4:
    case EHTokDouble1:
    case EHTokDouble2:
    case EHTokDouble3:
    case EHTokDouble4:
    case EHTokUint1:
    case EHTokUint2:
    case EHTokUint3:
    case EHTokUint4:
    case EHTokHalf1:
    case EHTokHalf2:
    case EHTokHalf3:
    case EHTokHalf4:
    case EHTokMin16float1:
    case EHTokMin16float2:
    case EHTokMin16float3:
    case EHTokMin16float4:
    case EHTokMin10float1:
    case EHTokMin10float2:
    case EHTokMin10float3:
    case EHTokMin10float4:
    case EHTokMin16int1:
    case EHTokMin16int2:
    case EHTokMin16int3:
    case EHTokMin16int4:
    case EHTokMin12int1:
    case EHTokMin12int2:
    case EHTokMin12int3:
    case EHTokMin12int4:
    case EHTokMin16uint1:
    case EHTokMin16uint2:
    case EHTokMin16uint3:
    case EHTokMin16uint4:

    // matrix types
    case EHTokBool1x1:
    case EHTokBool1x2:
    case EHTokBool1x3:
    case EHTokBool1x4:
    case EHTokBool2x1:
    case EHTokBool2x2:
    case EHTokBool2x3:
    case EHTokBool2x4:
    case EHTokBool3x1:
    case EHTokBool3x2:
    case EHTokBool3x3:
    case EHTokBool3x4:
    case EHTokBool4x1:
    case EHTokBool4x2:
    case EHTokBool4x3:
    case EHTokBool4x4:
    case EHTokInt1x1:
    case EHTokInt1x2:
    case EHTokInt1x3:
    case EHTokInt1x4:
    case EHTokInt2x1:
    case EHTokInt2x2:
    case EHTokInt2x3:
    case EHTokInt2x4:
    case EHTokInt3x1:
    case EHTokInt3x2:
    case EHTokInt3x3:
    case EHTokInt3x4:
    case EHTokInt4x1:
    case EHTokInt4x2:
    case EHTokInt4x3:
    case EHTokInt4x4:
    case EHTokUint1x1:
    case EHTokUint1x2:
    case EHTokUint1x3:
    case EHTokUint1x4:
    case EHTokUint2x1:
    case EHTokUint2x2:
    case EHTokUint2x3:
    case EHTokUint2x4:
    case EHTokUint3x1:
    case EHTokUint3x2:
    case EHTokUint3x3:
    case EHTokUint3x4:
    case EHTokUint4x1:
    case EHTokUint4x2:
    case EHTokUint4x3:
    case EHTokUint4x4:
    case EHTokFloat1x1:
    case EHTokFloat1x2:
    case EHTokFloat1x3:
    case EHTokFloat1x4:
    case EHTokFloat2x1:
    case EHTokFloat2x2:
    case EHTokFloat2x3:
    case EHTokFloat2x4:
    case EHTokFloat3x1:
    case EHTokFloat3x2:
    case EHTokFloat3x3:
    case EHTokFloat3x4:
    case EHTokFloat4x1:
    case EHTokFloat4x2:
    case EHTokFloat4x3:
    case EHTokFloat4x4:
    case EHTokHalf1x1:
    case EHTokHalf1x2:
    case EHTokHalf1x3:
    case EHTokHalf1x4:
    case EHTokHalf2x1:
    case EHTokHalf2x2:
    case EHTokHalf2x3:
    case EHTokHalf2x4:
    case EHTokHalf3x1:
    case EHTokHalf3x2:
    case EHTokHalf3x3:
    case EHTokHalf3x4:
    case EHTokHalf4x1:
    case EHTokHalf4x2:
    case EHTokHalf4x3:
    case EHTokHalf4x4:
    case EHTokDouble1x1:
    case EHTokDouble1x2:
    case EHTokDouble1x3:
    case EHTokDouble1x4:
    case EHTokDouble2x1:
    case EHTokDouble2x2:
    case EHTokDouble2x3:
    case EHTokDouble2x4:
    case EHTokDouble3x1:
    case EHTokDouble3x2:
    case EHTokDouble3x3:
    case EHTokDouble3x4:
    case EHTokDouble4x1:
    case EHTokDouble4x2:
    case EHTokDouble4x3:
    case EHTokDouble4x4:
        return keyword;

    // texturing types
    case EHTokSampler:
    case EHTokSampler1d:
    case EHTokSampler2d:
    case EHTokSampler3d:
    case EHTokSamplerCube:
    case EHTokSamplerState:
    case EHTokSamplerComparisonState:
    case EHTokTexture:
    case EHTokTexture1d:
    case EHTokTexture1darray:
    case EHTokTexture2d:
    case EHTokTexture2darray:
    case EHTokTexture3d:
    case EHTokTextureCube:
    case EHTokTextureCubearray:
    case EHTokTexture2DMS:
    case EHTokTexture2DMSarray:
    case EHTokRWTexture1d:
    case EHTokRWTexture1darray:
    case EHTokRWTexture2d:
    case EHTokRWTexture2darray:
    case EHTokRWTexture3d:
    case EHTokRWBuffer:
    case EHTokAppendStructuredBuffer:
    case EHTokByteAddressBuffer:
    case EHTokConsumeStructuredBuffer:
    case EHTokRWByteAddressBuffer:
    case EHTokRWStructuredBuffer:
    case EHTokStructuredBuffer:
    case EHTokTextureBuffer:
    case EHTokSubpassInput:
    case EHTokSubpassInputMS:
        return keyword;

    // variable, user type, ...
    case EHTokClass:
    case EHTokStruct:
    case EHTokTypedef:
    case EHTokCBuffer:
    case EHTokConstantBuffer:
    case EHTokTBuffer:
    case EHTokThis:
    case EHTokNamespace:
        return keyword;

    case EHTokBoolConstant:
        if (strcmp("true", tokenText) == 0)
            parserToken->b = true;
        else
            parserToken->b = false;
        return keyword;

    // control flow
    case EHTokFor:
    case EHTokDo:
    case EHTokWhile:
    case EHTokBreak:
    case EHTokContinue:
    case EHTokIf:
    case EHTokElse:
    case EHTokDiscard:
    case EHTokReturn:
    case EHTokCase:
    case EHTokSwitch:
    case EHTokDefault:
        return keyword;

    default:
        _parseContext.infoSink.info.message(EPrefixInternalError, "Unknown glslang keyword", loc);
        return EHTokNone;
    }
}

EHlslTokenClass HlslScanContext::identifierOrType()
{
    parserToken->string = NewPoolTString(tokenText);

    return EHTokIdentifier;
}

// Give an error for use of a reserved symbol.
// However, allow built-in declarations to use reserved words, to allow
// extension support before the extension is enabled.
EHlslTokenClass HlslScanContext::reservedWord()
{
    if (! _parseContext.symbolTable.atBuiltInLevel())
        _parseContext.error(loc, "Reserved word.", tokenText, "", "");

    return EHTokNone;
}

} // end namespace glslang

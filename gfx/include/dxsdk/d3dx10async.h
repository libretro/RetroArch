
//////////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) Microsoft Corporation.  All rights reserved.
//
//  File:       D3DX10Async.h
//  Content:    D3DX10 Asynchronous Effect / Shader loaders / compilers
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __D3DX10ASYNC_H__
#define __D3DX10ASYNC_H__

#include "d3dx10.h"

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus


//----------------------------------------------------------------------------
// D3DX10Compile:
// ------------------
// Compiles an effect or shader.
//
// Parameters:
//  pSrcFile
//      Source file name.
//  hSrcModule
//      Module handle. if NULL, current module will be used.
//  pSrcResource
//      Resource name in module.
//  pSrcData
//      Pointer to source code.
//  SrcDataLen
//      Size of source code, in bytes.
//  pDefines
//      Optional NULL-terminated array of preprocessor macro definitions.
//  pInclude
//      Optional interface pointer to use for handling #include directives.
//      If this parameter is NULL, #includes will be honored when compiling
//      from file, and will error when compiling from resource or memory.
//  pFunctionName
//      Name of the entrypoint function where execution should begin.
//  pProfile
//      Instruction set to be used when generating code.  Currently supported
//      profiles are "vs_1_1",  "vs_2_0", "vs_2_a", "vs_2_sw", "vs_3_0",
//                   "vs_3_sw", "vs_4_0", "vs_4_1",
//                   "ps_2_0",  "ps_2_a", "ps_2_b", "ps_2_sw", "ps_3_0", 
//                   "ps_3_sw", "ps_4_0", "ps_4_1",
//                   "gs_4_0",  "gs_4_1",
//                   "tx_1_0",
//                   "fx_4_0",  "fx_4_1"
//      Note that this entrypoint does not compile fx_2_0 targets, for that
//      you need to use the D3DX9 function.
//  Flags1
//      See D3D10_SHADER_xxx flags.
//  Flags2
//      See D3D10_EFFECT_xxx flags.
//  ppShader
//      Returns a buffer containing the created shader.  This buffer contains
//      the compiled shader code, as well as any embedded debug and symbol
//      table info.  (See D3D10GetShaderConstantTable)
//  ppErrorMsgs
//      Returns a buffer containing a listing of errors and warnings that were
//      encountered during the compile.  If you are running in a debugger,
//      these are the same messages you will see in your debug output.
//  pHResult
//      Pointer to a memory location to receive the return value upon completion.
//      Maybe NULL if not needed.
//      If pPump != NULL, pHResult must be a valid memory location until the
//      the asynchronous execution completes.
//----------------------------------------------------------------------------

HRESULT WINAPI D3DX10CompileFromFileA(LPCSTR pSrcFile,CONST D3D10_SHADER_MACRO* pDefines, LPD3D10INCLUDE pInclude,
        LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, ID3DX10ThreadPump* pPump, ID3D10Blob** ppShader, ID3D10Blob** ppErrorMsgs, HRESULT* pHResult);

HRESULT WINAPI D3DX10CompileFromFileW(LPCWSTR pSrcFile, CONST D3D10_SHADER_MACRO* pDefines, LPD3D10INCLUDE pInclude,
        LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, ID3DX10ThreadPump* pPump, ID3D10Blob** ppShader, ID3D10Blob** ppErrorMsgs, HRESULT* pHResult);

#ifdef UNICODE
#define D3DX10CompileFromFile D3DX10CompileFromFileW
#else
#define D3DX10CompileFromFile D3DX10CompileFromFileA
#endif

HRESULT WINAPI D3DX10CompileFromResourceA(HMODULE hSrcModule, LPCSTR pSrcResource, LPCSTR pSrcFileName, CONST D3D10_SHADER_MACRO* pDefines, 
    LPD3D10INCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, ID3DX10ThreadPump* pPump, ID3D10Blob** ppShader, ID3D10Blob** ppErrorMsgs, HRESULT* pHResult);

HRESULT WINAPI D3DX10CompileFromResourceW(HMODULE hSrcModule, LPCWSTR pSrcResource, LPCWSTR pSrcFileName, CONST D3D10_SHADER_MACRO* pDefines, 
    LPD3D10INCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, ID3DX10ThreadPump* pPump, ID3D10Blob** ppShader, ID3D10Blob** ppErrorMsgs, HRESULT* pHResult);

#ifdef UNICODE
#define D3DX10CompileFromResource D3DX10CompileFromResourceW
#else
#define D3DX10CompileFromResource D3DX10CompileFromResourceA
#endif

HRESULT WINAPI D3DX10CompileFromMemory(LPCSTR pSrcData, SIZE_T SrcDataLen, LPCSTR pFileName, CONST D3D10_SHADER_MACRO* pDefines, LPD3D10INCLUDE pInclude, 
    LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, ID3DX10ThreadPump* pPump, ID3D10Blob** ppShader, ID3D10Blob** ppErrorMsgs, HRESULT* pHResult);

//----------------------------------------------------------------------------
// D3DX10CreateEffectFromXXXX:
// --------------------------
// Creates an effect from a binary effect or file
//
// Parameters:
//
// [in]
//
//
//  pFileName
//      Name of the ASCII (uncompiled) or binary (compiled) Effect file to load
//
//  hModule
//      Handle to the module containing the resource to compile from
//  pResourceName
//      Name of the resource within hModule to compile from
//
//  pData
//      Blob of effect data, either ASCII (uncompiled) or binary (compiled)
//  DataLength
//      Length of the data blob
//
//  pDefines
//      Optional NULL-terminated array of preprocessor macro definitions.
//  pInclude
//      Optional interface pointer to use for handling #include directives.
//      If this parameter is NULL, #includes will be honored when compiling
//      from file, and will error when compiling from resource or memory.
//  pProfile
//      Profile to use when compiling the effect.
//  HLSLFlags
//      Compilation flags pertaining to shaders and data types, honored by
//      the HLSL compiler
//  FXFlags
//      Compilation flags pertaining to Effect compilation, honored
//      by the Effect compiler
//  pDevice
//      Pointer to the D3D10 device on which to create Effect resources
//  pEffectPool
//      Pointer to an Effect pool to share variables with or NULL
//
// [out]
//
//  ppEffect
//      Address of the newly created Effect interface
//  ppEffectPool
//      Address of the newly created Effect pool interface
//  ppErrors
//      If non-NULL, address of a buffer with error messages that occurred 
//      during parsing or compilation
//  pHResult
//      Pointer to a memory location to receive the return value upon completion.
//      Maybe NULL if not needed.
//      If pPump != NULL, pHResult must be a valid memory location until the
//      the asynchronous execution completes.
//----------------------------------------------------------------------------


HRESULT WINAPI D3DX10CreateEffectFromFileA(LPCSTR pFileName, CONST D3D10_SHADER_MACRO *pDefines, 
    ID3D10Include *pInclude, LPCSTR pProfile, UINT HLSLFlags, UINT FXFlags, ID3D10Device *pDevice, 
    ID3D10EffectPool *pEffectPool, ID3DX10ThreadPump* pPump, ID3D10Effect **ppEffect, ID3D10Blob **ppErrors, HRESULT* pHResult);

HRESULT WINAPI D3DX10CreateEffectFromFileW(LPCWSTR pFileName, CONST D3D10_SHADER_MACRO *pDefines, 
    ID3D10Include *pInclude, LPCSTR pProfile, UINT HLSLFlags, UINT FXFlags, ID3D10Device *pDevice, 
    ID3D10EffectPool *pEffectPool, ID3DX10ThreadPump* pPump, ID3D10Effect **ppEffect, ID3D10Blob **ppErrors, HRESULT* pHResult);

HRESULT WINAPI D3DX10CreateEffectFromMemory(LPCVOID pData, SIZE_T DataLength, LPCSTR pSrcFileName, CONST D3D10_SHADER_MACRO *pDefines, 
    ID3D10Include *pInclude, LPCSTR pProfile, UINT HLSLFlags, UINT FXFlags, ID3D10Device *pDevice, 
    ID3D10EffectPool *pEffectPool, ID3DX10ThreadPump* pPump, ID3D10Effect **ppEffect, ID3D10Blob **ppErrors, HRESULT* pHResult);

HRESULT WINAPI D3DX10CreateEffectFromResourceA(HMODULE hModule, LPCSTR pResourceName, LPCSTR pSrcFileName, CONST D3D10_SHADER_MACRO *pDefines, 
    ID3D10Include *pInclude, LPCSTR pProfile, UINT HLSLFlags, UINT FXFlags, ID3D10Device *pDevice, 
    ID3D10EffectPool *pEffectPool, ID3DX10ThreadPump* pPump, ID3D10Effect **ppEffect, ID3D10Blob **ppErrors, HRESULT* pHResult);

HRESULT WINAPI D3DX10CreateEffectFromResourceW(HMODULE hModule, LPCWSTR pResourceName, LPCWSTR pSrcFileName, CONST D3D10_SHADER_MACRO *pDefines, 
    ID3D10Include *pInclude, LPCSTR pProfile, UINT HLSLFlags, UINT FXFlags, ID3D10Device *pDevice, 
    ID3D10EffectPool *pEffectPool, ID3DX10ThreadPump* pPump, ID3D10Effect **ppEffect, ID3D10Blob **ppErrors, HRESULT* pHResult);


#ifdef UNICODE
#define D3DX10CreateEffectFromFile          D3DX10CreateEffectFromFileW
#define D3DX10CreateEffectFromResource      D3DX10CreateEffectFromResourceW
#else
#define D3DX10CreateEffectFromFile          D3DX10CreateEffectFromFileA
#define D3DX10CreateEffectFromResource      D3DX10CreateEffectFromResourceA
#endif

HRESULT WINAPI D3DX10CreateEffectPoolFromFileA(LPCSTR pFileName, CONST D3D10_SHADER_MACRO *pDefines, 
    ID3D10Include *pInclude, LPCSTR pProfile, UINT HLSLFlags, UINT FXFlags, ID3D10Device *pDevice, ID3DX10ThreadPump* pPump, 
    ID3D10EffectPool **ppEffectPool, ID3D10Blob **ppErrors, HRESULT* pHResult);

HRESULT WINAPI D3DX10CreateEffectPoolFromFileW(LPCWSTR pFileName, CONST D3D10_SHADER_MACRO *pDefines, 
    ID3D10Include *pInclude, LPCSTR pProfile, UINT HLSLFlags, UINT FXFlags, ID3D10Device *pDevice, ID3DX10ThreadPump* pPump, 
    ID3D10EffectPool **ppEffectPool, ID3D10Blob **ppErrors, HRESULT* pHResult);

HRESULT WINAPI D3DX10CreateEffectPoolFromMemory(LPCVOID pData, SIZE_T DataLength, LPCSTR pSrcFileName, CONST D3D10_SHADER_MACRO *pDefines, 
    ID3D10Include *pInclude, LPCSTR pProfile, UINT HLSLFlags, UINT FXFlags, ID3D10Device *pDevice,
    ID3DX10ThreadPump* pPump, ID3D10EffectPool **ppEffectPool, ID3D10Blob **ppErrors, HRESULT* pHResult);

HRESULT WINAPI D3DX10CreateEffectPoolFromResourceA(HMODULE hModule, LPCSTR pResourceName, LPCSTR pSrcFileName, CONST D3D10_SHADER_MACRO *pDefines, 
    ID3D10Include *pInclude, LPCSTR pProfile, UINT HLSLFlags, UINT FXFlags, ID3D10Device *pDevice,
    ID3DX10ThreadPump* pPump, ID3D10EffectPool **ppEffectPool, ID3D10Blob **ppErrors, HRESULT* pHResult);
                                         
HRESULT WINAPI D3DX10CreateEffectPoolFromResourceW(HMODULE hModule, LPCWSTR pResourceName, LPCWSTR pSrcFileName, CONST D3D10_SHADER_MACRO *pDefines, 
    ID3D10Include *pInclude, LPCSTR pProfile, UINT HLSLFlags, UINT FXFlags, ID3D10Device *pDevice,
    ID3DX10ThreadPump* pPump, ID3D10EffectPool **ppEffectPool, ID3D10Blob **ppErrors, HRESULT* pHResult);

#ifdef UNICODE
#define D3DX10CreateEffectPoolFromFile      D3DX10CreateEffectPoolFromFileW
#define D3DX10CreateEffectPoolFromResource  D3DX10CreateEffectPoolFromResourceW
#else
#define D3DX10CreateEffectPoolFromFile      D3DX10CreateEffectPoolFromFileA
#define D3DX10CreateEffectPoolFromResource  D3DX10CreateEffectPoolFromResourceA
#endif

HRESULT WINAPI D3DX10PreprocessShaderFromFileA(LPCSTR pFileName, CONST D3D10_SHADER_MACRO* pDefines, 
    LPD3D10INCLUDE pInclude, ID3DX10ThreadPump *pPump, ID3D10Blob** ppShaderText, ID3D10Blob** ppErrorMsgs, HRESULT* pHResult);

HRESULT WINAPI D3DX10PreprocessShaderFromFileW(LPCWSTR pFileName, CONST D3D10_SHADER_MACRO* pDefines, 
    LPD3D10INCLUDE pInclude, ID3DX10ThreadPump *pPump, ID3D10Blob** ppShaderText, ID3D10Blob** ppErrorMsgs, HRESULT* pHResult);

HRESULT WINAPI D3DX10PreprocessShaderFromMemory(LPCSTR pSrcData, SIZE_T SrcDataSize, LPCSTR pFileName, CONST D3D10_SHADER_MACRO* pDefines, 
    LPD3D10INCLUDE pInclude, ID3DX10ThreadPump *pPump, ID3D10Blob** ppShaderText, ID3D10Blob** ppErrorMsgs, HRESULT* pHResult);

HRESULT WINAPI D3DX10PreprocessShaderFromResourceA(HMODULE hModule, LPCSTR pResourceName, LPCSTR pSrcFileName, CONST D3D10_SHADER_MACRO* pDefines, 
    LPD3D10INCLUDE pInclude, ID3DX10ThreadPump *pPump, ID3D10Blob** ppShaderText, ID3D10Blob** ppErrorMsgs, HRESULT* pHResult);

HRESULT WINAPI D3DX10PreprocessShaderFromResourceW(HMODULE hModule, LPCWSTR pResourceName, LPCWSTR pSrcFileName, CONST D3D10_SHADER_MACRO* pDefines, 
    LPD3D10INCLUDE pInclude, ID3DX10ThreadPump *pPump, ID3D10Blob** ppShaderText, ID3D10Blob** ppErrorMsgs, HRESULT* pHResult);

#ifdef UNICODE
#define D3DX10PreprocessShaderFromFile      D3DX10PreprocessShaderFromFileW
#define D3DX10PreprocessShaderFromResource  D3DX10PreprocessShaderFromResourceW
#else
#define D3DX10PreprocessShaderFromFile      D3DX10PreprocessShaderFromFileA
#define D3DX10PreprocessShaderFromResource  D3DX10PreprocessShaderFromResourceA
#endif

//----------------------------------------------------------------------------
// Async processors
//----------------------------------------------------------------------------

HRESULT WINAPI D3DX10CreateAsyncCompilerProcessor(LPCSTR pFileName, CONST D3D10_SHADER_MACRO* pDefines, LPD3D10INCLUDE pInclude, 
        LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2,
        ID3D10Blob **ppCompiledShader, ID3D10Blob **ppErrorBuffer, ID3DX10DataProcessor **ppProcessor);

HRESULT WINAPI D3DX10CreateAsyncEffectCreateProcessor(LPCSTR pFileName, CONST D3D10_SHADER_MACRO* pDefines, LPD3D10INCLUDE pInclude, 
        LPCSTR pProfile, UINT Flags, UINT FXFlags, ID3D10Device *pDevice,
        ID3D10EffectPool *pPool, ID3D10Blob **ppErrorBuffer, ID3DX10DataProcessor **ppProcessor);

HRESULT WINAPI D3DX10CreateAsyncEffectPoolCreateProcessor(LPCSTR pFileName, CONST D3D10_SHADER_MACRO* pDefines, LPD3D10INCLUDE pInclude, 
        LPCSTR pProfile, UINT Flags, UINT FXFlags, ID3D10Device *pDevice,
        ID3D10Blob **ppErrorBuffer, ID3DX10DataProcessor **ppProcessor);

HRESULT WINAPI D3DX10CreateAsyncShaderPreprocessProcessor(LPCSTR pFileName, CONST D3D10_SHADER_MACRO* pDefines, LPD3D10INCLUDE pInclude, 
        ID3D10Blob** ppShaderText, ID3D10Blob **ppErrorBuffer, ID3DX10DataProcessor **ppProcessor);



//----------------------------------------------------------------------------
// D3DX10 Asynchronous texture I/O (advanced mode)
//----------------------------------------------------------------------------

HRESULT WINAPI D3DX10CreateAsyncFileLoaderW(LPCWSTR pFileName, ID3DX10DataLoader **ppDataLoader);
HRESULT WINAPI D3DX10CreateAsyncFileLoaderA(LPCSTR pFileName, ID3DX10DataLoader **ppDataLoader);
HRESULT WINAPI D3DX10CreateAsyncMemoryLoader(LPCVOID pData, SIZE_T cbData, ID3DX10DataLoader **ppDataLoader);
HRESULT WINAPI D3DX10CreateAsyncResourceLoaderW(HMODULE hSrcModule, LPCWSTR pSrcResource, ID3DX10DataLoader **ppDataLoader);
HRESULT WINAPI D3DX10CreateAsyncResourceLoaderA(HMODULE hSrcModule, LPCSTR pSrcResource, ID3DX10DataLoader **ppDataLoader);

#ifdef UNICODE
#define D3DX10CreateAsyncFileLoader D3DX10CreateAsyncFileLoaderW
#define D3DX10CreateAsyncResourceLoader D3DX10CreateAsyncResourceLoaderW
#else
#define D3DX10CreateAsyncFileLoader D3DX10CreateAsyncFileLoaderA
#define D3DX10CreateAsyncResourceLoader D3DX10CreateAsyncResourceLoaderA
#endif

HRESULT WINAPI D3DX10CreateAsyncTextureProcessor(ID3D10Device *pDevice, D3DX10_IMAGE_LOAD_INFO *pLoadInfo, ID3DX10DataProcessor **ppDataProcessor);
HRESULT WINAPI D3DX10CreateAsyncTextureInfoProcessor(D3DX10_IMAGE_INFO *pImageInfo, ID3DX10DataProcessor **ppDataProcessor);
HRESULT WINAPI D3DX10CreateAsyncShaderResourceViewProcessor(ID3D10Device *pDevice, D3DX10_IMAGE_LOAD_INFO *pLoadInfo, ID3DX10DataProcessor **ppDataProcessor);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif //__D3DX10ASYNC_H__



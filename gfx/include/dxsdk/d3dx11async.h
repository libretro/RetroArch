
//////////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) Microsoft Corporation.  All rights reserved.
//
//  File:       D3DX11Async.h
//  Content:    D3DX11 Asynchronous Shader loaders / compilers
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __D3DX11ASYNC_H__
#define __D3DX11ASYNC_H__

#include "d3dx11.h"

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus


//----------------------------------------------------------------------------
// D3DX11Compile:
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

HRESULT WINAPI D3DX11CompileFromFileA(LPCSTR pSrcFile,CONST D3D10_SHADER_MACRO* pDefines, LPD3D10INCLUDE pInclude,
        LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, ID3DX11ThreadPump* pPump, ID3D10Blob** ppShader, ID3D10Blob** ppErrorMsgs, HRESULT* pHResult);

HRESULT WINAPI D3DX11CompileFromFileW(LPCWSTR pSrcFile, CONST D3D10_SHADER_MACRO* pDefines, LPD3D10INCLUDE pInclude,
        LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, ID3DX11ThreadPump* pPump, ID3D10Blob** ppShader, ID3D10Blob** ppErrorMsgs, HRESULT* pHResult);

#ifdef UNICODE
#define D3DX11CompileFromFile D3DX11CompileFromFileW
#else
#define D3DX11CompileFromFile D3DX11CompileFromFileA
#endif

HRESULT WINAPI D3DX11CompileFromResourceA(HMODULE hSrcModule, LPCSTR pSrcResource, LPCSTR pSrcFileName, CONST D3D10_SHADER_MACRO* pDefines, 
    LPD3D10INCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, ID3DX11ThreadPump* pPump, ID3D10Blob** ppShader, ID3D10Blob** ppErrorMsgs, HRESULT* pHResult);

HRESULT WINAPI D3DX11CompileFromResourceW(HMODULE hSrcModule, LPCWSTR pSrcResource, LPCWSTR pSrcFileName, CONST D3D10_SHADER_MACRO* pDefines, 
    LPD3D10INCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, ID3DX11ThreadPump* pPump, ID3D10Blob** ppShader, ID3D10Blob** ppErrorMsgs, HRESULT* pHResult);

#ifdef UNICODE
#define D3DX11CompileFromResource D3DX11CompileFromResourceW
#else
#define D3DX11CompileFromResource D3DX11CompileFromResourceA
#endif

HRESULT WINAPI D3DX11CompileFromMemory(LPCSTR pSrcData, SIZE_T SrcDataLen, LPCSTR pFileName, CONST D3D10_SHADER_MACRO* pDefines, LPD3D10INCLUDE pInclude, 
    LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, ID3DX11ThreadPump* pPump, ID3D10Blob** ppShader, ID3D10Blob** ppErrorMsgs, HRESULT* pHResult);

HRESULT WINAPI D3DX11PreprocessShaderFromFileA(LPCSTR pFileName, CONST D3D10_SHADER_MACRO* pDefines, 
    LPD3D10INCLUDE pInclude, ID3DX11ThreadPump *pPump, ID3D10Blob** ppShaderText, ID3D10Blob** ppErrorMsgs, HRESULT* pHResult);

HRESULT WINAPI D3DX11PreprocessShaderFromFileW(LPCWSTR pFileName, CONST D3D10_SHADER_MACRO* pDefines, 
    LPD3D10INCLUDE pInclude, ID3DX11ThreadPump *pPump, ID3D10Blob** ppShaderText, ID3D10Blob** ppErrorMsgs, HRESULT* pHResult);

HRESULT WINAPI D3DX11PreprocessShaderFromMemory(LPCSTR pSrcData, SIZE_T SrcDataSize, LPCSTR pFileName, CONST D3D10_SHADER_MACRO* pDefines, 
    LPD3D10INCLUDE pInclude, ID3DX11ThreadPump *pPump, ID3D10Blob** ppShaderText, ID3D10Blob** ppErrorMsgs, HRESULT* pHResult);

HRESULT WINAPI D3DX11PreprocessShaderFromResourceA(HMODULE hModule, LPCSTR pResourceName, LPCSTR pSrcFileName, CONST D3D10_SHADER_MACRO* pDefines, 
    LPD3D10INCLUDE pInclude, ID3DX11ThreadPump *pPump, ID3D10Blob** ppShaderText, ID3D10Blob** ppErrorMsgs, HRESULT* pHResult);

HRESULT WINAPI D3DX11PreprocessShaderFromResourceW(HMODULE hModule, LPCWSTR pResourceName, LPCWSTR pSrcFileName, CONST D3D10_SHADER_MACRO* pDefines, 
    LPD3D10INCLUDE pInclude, ID3DX11ThreadPump *pPump, ID3D10Blob** ppShaderText, ID3D10Blob** ppErrorMsgs, HRESULT* pHResult);

#ifdef UNICODE
#define D3DX11PreprocessShaderFromFile      D3DX11PreprocessShaderFromFileW
#define D3DX11PreprocessShaderFromResource  D3DX11PreprocessShaderFromResourceW
#else
#define D3DX11PreprocessShaderFromFile      D3DX11PreprocessShaderFromFileA
#define D3DX11PreprocessShaderFromResource  D3DX11PreprocessShaderFromResourceA
#endif

//----------------------------------------------------------------------------
// Async processors
//----------------------------------------------------------------------------

HRESULT WINAPI D3DX11CreateAsyncCompilerProcessor(LPCSTR pFileName, CONST D3D10_SHADER_MACRO* pDefines, LPD3D10INCLUDE pInclude, 
        LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2,
        ID3D10Blob **ppCompiledShader, ID3D10Blob **ppErrorBuffer, ID3DX11DataProcessor **ppProcessor);

HRESULT WINAPI D3DX11CreateAsyncShaderPreprocessProcessor(LPCSTR pFileName, CONST D3D10_SHADER_MACRO* pDefines, LPD3D10INCLUDE pInclude, 
        ID3D10Blob** ppShaderText, ID3D10Blob **ppErrorBuffer, ID3DX11DataProcessor **ppProcessor);

//----------------------------------------------------------------------------
// D3DX11 Asynchronous texture I/O (advanced mode)
//----------------------------------------------------------------------------

HRESULT WINAPI D3DX11CreateAsyncFileLoaderW(LPCWSTR pFileName, ID3DX11DataLoader **ppDataLoader);
HRESULT WINAPI D3DX11CreateAsyncFileLoaderA(LPCSTR pFileName, ID3DX11DataLoader **ppDataLoader);
HRESULT WINAPI D3DX11CreateAsyncMemoryLoader(LPCVOID pData, SIZE_T cbData, ID3DX11DataLoader **ppDataLoader);
HRESULT WINAPI D3DX11CreateAsyncResourceLoaderW(HMODULE hSrcModule, LPCWSTR pSrcResource, ID3DX11DataLoader **ppDataLoader);
HRESULT WINAPI D3DX11CreateAsyncResourceLoaderA(HMODULE hSrcModule, LPCSTR pSrcResource, ID3DX11DataLoader **ppDataLoader);

#ifdef UNICODE
#define D3DX11CreateAsyncFileLoader D3DX11CreateAsyncFileLoaderW
#define D3DX11CreateAsyncResourceLoader D3DX11CreateAsyncResourceLoaderW
#else
#define D3DX11CreateAsyncFileLoader D3DX11CreateAsyncFileLoaderA
#define D3DX11CreateAsyncResourceLoader D3DX11CreateAsyncResourceLoaderA
#endif

HRESULT WINAPI D3DX11CreateAsyncTextureProcessor(ID3D11Device *pDevice, D3DX11_IMAGE_LOAD_INFO *pLoadInfo, ID3DX11DataProcessor **ppDataProcessor);
HRESULT WINAPI D3DX11CreateAsyncTextureInfoProcessor(D3DX11_IMAGE_INFO *pImageInfo, ID3DX11DataProcessor **ppDataProcessor);
HRESULT WINAPI D3DX11CreateAsyncShaderResourceViewProcessor(ID3D11Device *pDevice, D3DX11_IMAGE_LOAD_INFO *pLoadInfo, ID3DX11DataProcessor **ppDataProcessor);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif //__D3DX11ASYNC_H__



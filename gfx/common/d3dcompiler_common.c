/*  RetroArch - A frontend for libretro.
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

#define CINTERFACE

#include <stdio.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "d3dcompiler_common.h"
#include "../../verbosity.h"

#if defined(HAVE_DYNAMIC) && !defined(__WINRT__)
#include <dynamic/dylib.h>

static dylib_t     d3dcompiler_dll;
static const char* d3dcompiler_dll_list[] = {
   "D3DCompiler_47.dll", "D3DCompiler_46.dll", "D3DCompiler_45.dll", "D3DCompiler_44.dll",
   "D3DCompiler_43.dll", "D3DCompiler_42.dll", "D3DCompiler_41.dll", "D3DCompiler_40.dll",
   "D3DCompiler_39.dll", "D3DCompiler_38.dll", "D3DCompiler_37.dll", "D3DCompiler_36.dll",
   "D3DCompiler_35.dll", "D3DCompiler_34.dll", "D3DCompiler_33.dll", NULL,
};

HRESULT WINAPI D3DCompile(
      LPCVOID pSrcData,
      SIZE_T  SrcDataSize,
      LPCSTR  pSourceName,
      CONST D3D_SHADER_MACRO* pDefines,
      ID3DInclude*            pInclude,
      LPCSTR                  pEntrypoint,
      LPCSTR                  pTarget,
      UINT                    Flags1,
      UINT                    Flags2,
      ID3DBlob**              ppCode,
      ID3DBlob**              ppErrorMsgs)
{
   static pD3DCompile fp;
   const char** dll_name = d3dcompiler_dll_list;
   while (!d3dcompiler_dll && *dll_name)
      d3dcompiler_dll = dylib_load(*dll_name++);

   if (!d3dcompiler_dll)
      return TYPE_E_CANTLOADLIBRARY;

   if (!fp)
      fp = (pD3DCompile)dylib_proc(d3dcompiler_dll, "D3DCompile");

   if (!fp)
      return TYPE_E_DLLFUNCTIONNOTFOUND;

   return fp(
         pSrcData, SrcDataSize, pSourceName, pDefines, pInclude, pEntrypoint, pTarget, Flags1,
         Flags2, ppCode, ppErrorMsgs);
}

HRESULT WINAPI D3DCompileFromFile(
      LPCWSTR                 pFileName,
      const D3D_SHADER_MACRO* pDefines,
      ID3DInclude*            pInclude,
      LPCSTR                  pEntrypoint,
      LPCSTR                  pTarget,
      UINT                    Flags1,
      UINT                    Flags2,
      ID3DBlob**              ppCode,
      ID3DBlob**              ppErrorMsgs)
{
   typedef HRESULT(WINAPI * pD3DCompileFromFile)(
         LPCWSTR, const D3D_SHADER_MACRO*, ID3DInclude*, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob**,
         ID3DBlob**);
   static pD3DCompileFromFile fp;
   const char** dll_name = d3dcompiler_dll_list;
   while (!d3dcompiler_dll && *dll_name)
      d3dcompiler_dll = dylib_load(*dll_name++);

   if (!d3dcompiler_dll)
	   return TYPE_E_CANTLOADLIBRARY;

   if (!fp)
      fp = (pD3DCompileFromFile)dylib_proc(d3dcompiler_dll, "D3DCompileFromFile");

   if (!fp)
      return TYPE_E_DLLFUNCTIONNOTFOUND;

   return fp(
         pFileName, pDefines, pInclude, pEntrypoint, pTarget, Flags1, Flags2, ppCode, ppErrorMsgs);
}

HRESULT WINAPI
        D3DReflect(LPCVOID pSrcData, SIZE_T SrcDataSize, REFIID pInterface, void** ppReflector)
{
   typedef HRESULT(WINAPI * pD3DCompileFromFile)(LPCVOID, SIZE_T, REFIID, void**);
   static pD3DCompileFromFile fp;

   const char** dll_name = d3dcompiler_dll_list;
   while (!d3dcompiler_dll && *dll_name)
      d3dcompiler_dll = dylib_load(*dll_name++);

   if (!d3dcompiler_dll)
      return TYPE_E_CANTLOADLIBRARY;

   if (!fp)
      fp = (pD3DCompileFromFile)dylib_proc(d3dcompiler_dll, "D3DReflect");

   if (!fp)
      return TYPE_E_DLLFUNCTIONNOTFOUND;

   return fp(pSrcData, SrcDataSize, pInterface, ppReflector);
}
#endif

bool d3d_compile(const char* src, size_t size, LPCSTR src_name, LPCSTR entrypoint, LPCSTR target, D3DBlob* out)
{
   D3DBlob error_msg;
   UINT compileflags    = 0;

#ifdef DEBUG
   compileflags        |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

   if (!size)
      size = strlen(src);

   if (FAILED(D3DCompile(
             src, size, src_name, NULL, NULL, entrypoint, target, compileflags, 0, out, &error_msg)))
   {
      if (error_msg)
      {
         RARCH_ERR("D3DCompile failed :\n%s\n", (const char*)D3DGetBufferPointer(error_msg));
         Release(error_msg);
      }
      return false;
   }

   return true;
}

bool d3d_compile_from_file(LPCWSTR filename, LPCSTR entrypoint, LPCSTR target, D3DBlob* out)
{
   D3DBlob error_msg;
   UINT compileflags    = 0;

#ifdef DEBUG
   compileflags        |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

   if (FAILED(D3DCompileFromFile(
             filename, NULL, NULL, entrypoint, target, compileflags, 0, out, &error_msg)))
   {
      if (error_msg)
      {
         RARCH_ERR("D3DCompile failed :\n%s\n", (const char*)D3DGetBufferPointer(error_msg));
         Release(error_msg);
      }
      return false;
   }

   return true;
}

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

#include <stdio.h>
#include <dynamic/dylib.h>

#include "gfx/common/d3dcompiler_common.h"
#include "verbosity.h"

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
   static dylib_t     d3dcompiler_dll;
   static const char* dll_list[] = {
      "D3DCompiler_47.dll", "D3DCompiler_46.dll", "D3DCompiler_45.dll", "D3DCompiler_44.dll",
      "D3DCompiler_43.dll", "D3DCompiler_42.dll", "D3DCompiler_41.dll", "D3DCompiler_40.dll",
      "D3DCompiler_39.dll", "D3DCompiler_38.dll", "D3DCompiler_37.dll", "D3DCompiler_36.dll",
      "D3DCompiler_35.dll", "D3DCompiler_34.dll", "D3DCompiler_33.dll", NULL,
   };
   const char** dll_name = dll_list;

   while (!d3dcompiler_dll && *dll_name)
      d3dcompiler_dll = dylib_load(*dll_name++);

   if (d3dcompiler_dll)
   {
      static pD3DCompile fp;

      if (!fp)
         fp = (pD3DCompile)dylib_proc(d3dcompiler_dll, "D3DCompile");

      if (fp)
         return fp(
               pSrcData, SrcDataSize, pSourceName, pDefines, pInclude, pEntrypoint, pTarget, Flags1,
               Flags2, ppCode, ppErrorMsgs);
   }

   return TYPE_E_CANTLOADLIBRARY;
}

bool d3d_compile(const char* src, size_t size, LPCSTR entrypoint, LPCSTR target, D3DBlob* out)
{
   D3DBlob error_msg;
   UINT    compileflags = 0;

#ifdef DEBUG
   compileflags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

   if (FAILED(D3DCompile(
             src, size, NULL, NULL, NULL, entrypoint, target, compileflags, 0, out, &error_msg)))
      return false;

   if (error_msg)
   {
      RARCH_ERR("D3DCompile failed :\n%s\n", (const char*)D3DGetBufferPointer(error_msg));
      Release(error_msg);
      return false;
   }

   return true;
}

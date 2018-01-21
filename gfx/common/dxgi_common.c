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

#include <dynamic/dylib.h>

#include "dxgi_common.h"

HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void **ppFactory)
{
   static dylib_t dxgi_dll;
   static HRESULT (WINAPI *fp)(REFIID, void **);

   if(!dxgi_dll)
      dxgi_dll = dylib_load("dxgi.dll");

   if(!dxgi_dll)
      return TYPE_E_CANTLOADLIBRARY;

   if(!fp)
      fp = (HRESULT (WINAPI *)(REFIID, void **))dylib_proc(dxgi_dll, "CreateDXGIFactory1");

   if(!fp)
      return TYPE_E_CANTLOADLIBRARY;

   return fp(riid, ppFactory);
}


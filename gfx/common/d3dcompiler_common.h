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

#pragma once

#include <retro_inline.h>
#include <boolean.h>

#include "dxgi_common.h"
#include <d3dcommon.h>
#include <d3dcompiler.h>

/* auto-generated */
typedef ID3DBlob*                D3DBlob;
/* end of auto-generated */

bool d3d_compile(const char* src, size_t size,
      LPCSTR src_name, LPCSTR entrypoint, LPCSTR target, D3DBlob* out);

bool d3d_compile_from_file(LPCWSTR filename, LPCSTR entrypoint, LPCSTR target, D3DBlob* out);

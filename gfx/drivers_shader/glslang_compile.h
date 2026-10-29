/*  RetroArch - A frontend for libretro.
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

#ifndef __GLSLANG_COMPILE_H
#define __GLSLANG_COMPILE_H

/* Minimal C ABI between the slang stack and the glslang bridge
 * (glslang.cpp), the only RetroArch header the bridge includes.  It
 * stays free of platform headers: <windows.h> in the bridge's
 * translation unit mangles glslang's headers (min()/max() macros,
 * BOOL/INT/UINT/FLOAT typedefs).  Keep the include set here to
 * stdint/stddef/boolean/retro_common_api. */

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum glslang_compile_stage
{
   GLSLANG_COMPILE_STAGE_VERTEX = 0,
   GLSLANG_COMPILE_STAGE_FRAGMENT
};

/* Compile a single GLSL stage to SPIR-V via the system's glslang.
 * On success *spirv points at a malloc'd array of *spirv_len words
 * which the caller must free().  On failure *spirv is NULL and
 * *spirv_len is 0.  Implemented in glslang.cpp - the one translation
 * unit in the slang stack that must remain C++, since glslang only
 * exposes a C++ API. */
bool glslang_compile_spirv(const char *source,
      enum glslang_compile_stage stage,
      uint32_t **spirv, size_t *spirv_len);

/* Starts glslang's process state, kept for the life of the process;
 * before any thread compiles. A compile starts it itself when nothing
 * did. */
void glslang_process_init(void);

RETRO_END_DECLS

#endif

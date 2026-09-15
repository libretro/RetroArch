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
 * (glslang.cpp).  This header is deliberately tiny and must stay
 * free of platform headers: it is the only RetroArch header the
 * bridge includes, and the bridge is amalgamated with the vendored
 * glslang library sources in griffin/griffin_glslang.cpp.  Pulling
 * <windows.h> into that translation unit breaks it in either
 * direction: included before the library, its function-like
 * min()/max() macros mangle glslang's std::numeric_limits uses;
 * included after, its BOOL/INT/UINT/FLOAT typedefs collide with the
 * global token enumerators of glslang's generated parser.  Keep the
 * include set here to stdint/stddef/boolean/retro_common_api. */

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

/* Compile a single GLSL stage to SPIR-V via the bundled glslang.
 * On success *spirv points at a malloc'd array of *spirv_len words
 * which the caller must free().  On failure *spirv is NULL and
 * *spirv_len is 0.  Implemented in glslang.cpp - the one translation
 * unit in the slang stack that must remain C++, since the vendored
 * glslang only exposes a C++ API. */
bool glslang_compile_spirv(const char *source,
      enum glslang_compile_stage stage,
      uint32_t **spirv, size_t *spirv_len);

RETRO_END_DECLS

#endif

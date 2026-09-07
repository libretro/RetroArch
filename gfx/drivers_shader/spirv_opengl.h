/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2026 - The RetroArch team
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

#ifndef __RARCH_SPIRV_OPENGL_H
#define __RARCH_SPIRV_OPENGL_H

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Worst case number of words spirv_opengl_lower() may append to a module:
 * two four word OpDecorate instructions. */
#define SPIRV_OPENGL_LOWER_EXTRA_WORDS 8

/**
 * spirv_opengl_lower:
 * @in_words      : SPIR-V module as produced by glslang with Vulkan rules
 * @in_count      : size of @in_words, in 32-bit words
 * @out_words     : destination buffer
 * @out_capacity  : size of @out_words, in 32-bit words. Must be at least
 *                  @in_count + SPIRV_OPENGL_LOWER_EXTRA_WORDS.
 * @push_binding  : uniform block binding to assign to the rewritten push
 *                  constant block. Must not collide with the binding used
 *                  by the shader's own UBO, and must be identical for every
 *                  stage of the same program.
 *
 * Rewrites a Vulkan SPIR-V module so that it satisfies the OpenGL SPIR-V
 * execution environment of GL_ARB_gl_spirv, chiefly by turning the push
 * constant block into an ordinary uniform block. Modules using features
 * that cannot be expressed under OpenGL, or whose legality cannot be
 * established cheaply, are rejected rather than rewritten.
 *
 * Returns: number of words written to @out_words, or 0 if the module cannot
 * be lowered. A return of 0 is not an error; callers are expected to fall
 * back to cross-compiling the module to GLSL.
 */
size_t spirv_opengl_lower(
      const uint32_t *in_words, size_t in_count,
      uint32_t *out_words, size_t out_capacity,
      unsigned push_binding);

RETRO_END_DECLS

#endif

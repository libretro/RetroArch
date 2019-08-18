/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2017 - Hans-Kristian Arntzen
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

#ifndef GLSLANG_UTIL_H
#define GLSLANG_UTIL_H

#include <stdint.h>
#include <retro_common_api.h>

#include <lists/string_list.h>

typedef enum glslang_format
{
   SLANG_FORMAT_UNKNOWN = 0,

   /* 8-bit */
   SLANG_FORMAT_R8_UNORM,
   SLANG_FORMAT_R8_UINT,
   SLANG_FORMAT_R8_SINT,
   SLANG_FORMAT_R8G8_UNORM,
   SLANG_FORMAT_R8G8_UINT,
   SLANG_FORMAT_R8G8_SINT,
   SLANG_FORMAT_R8G8B8A8_UNORM,
   SLANG_FORMAT_R8G8B8A8_UINT,
   SLANG_FORMAT_R8G8B8A8_SINT,
   SLANG_FORMAT_R8G8B8A8_SRGB,

   /* 10-bit */
   SLANG_FORMAT_A2B10G10R10_UNORM_PACK32,
   SLANG_FORMAT_A2B10G10R10_UINT_PACK32,

   /* 16-bit */
   SLANG_FORMAT_R16_UINT,
   SLANG_FORMAT_R16_SINT,
   SLANG_FORMAT_R16_SFLOAT,
   SLANG_FORMAT_R16G16_UINT,
   SLANG_FORMAT_R16G16_SINT,
   SLANG_FORMAT_R16G16_SFLOAT,
   SLANG_FORMAT_R16G16B16A16_UINT,
   SLANG_FORMAT_R16G16B16A16_SINT,
   SLANG_FORMAT_R16G16B16A16_SFLOAT,

   /* 32-bit */
   SLANG_FORMAT_R32_UINT,
   SLANG_FORMAT_R32_SINT,
   SLANG_FORMAT_R32_SFLOAT,
   SLANG_FORMAT_R32G32_UINT,
   SLANG_FORMAT_R32G32_SINT,
   SLANG_FORMAT_R32G32_SFLOAT,
   SLANG_FORMAT_R32G32B32A32_UINT,
   SLANG_FORMAT_R32G32B32A32_SINT,
   SLANG_FORMAT_R32G32B32A32_SFLOAT,

   SLANG_FORMAT_MAX
}glslang_format;

RETRO_BEGIN_DECLS

const char *glslang_format_to_string(glslang_format fmt);

enum glslang_format glslang_find_format(const char *fmt);

bool glslang_read_shader_file(const char *path,
      struct string_list *output, bool root_file);

RETRO_END_DECLS

#endif

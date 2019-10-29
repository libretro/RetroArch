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

#ifndef GLSLANG_UTIL_HPP
#define GLSLANG_UTIL_HPP

#include <stdint.h>
#include <retro_common_api.h>

#include <lists/string_list.h>

#include <vector>
#include <string>

struct glslang_parameter
{
   std::string id;
   std::string desc;
   float initial;
   float minimum;
   float maximum;
   float step;
};

struct glslang_meta
{
   std::vector<glslang_parameter> parameters;
   std::string name;
   glslang_format rt_format;

   glslang_meta()
   {
	   rt_format = SLANG_FORMAT_UNKNOWN;
   }
};

struct glslang_output
{
   std::vector<uint32_t> vertex;
   std::vector<uint32_t> fragment;
   glslang_meta meta;
};

bool glslang_compile_shader(const char *shader_path, glslang_output *output);

/* Helpers for internal use. */
bool glslang_parse_meta(const struct string_list *lines, glslang_meta *meta);

#endif

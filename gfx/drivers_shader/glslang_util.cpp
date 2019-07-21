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

#include <stdio.h>
#include <string.h>
#include <string>
#include <sstream>
#include <algorithm>

#include <retro_miscellaneous.h>
#include <file/file_path.h>
#include <file/config_file.h>
#include <streams/file_stream.h>
#include <lists/string_list.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "glslang_util.h"
#if defined(HAVE_GLSLANG)
#include <glslang.hpp>
#endif
#include "../../verbosity.h"

using namespace std;

bool glslang_read_shader_file(const char *path, vector<string> *output, bool root_file)
{
   vector<const char *> lines;
   char include_path[PATH_MAX_LENGTH];
   char tmp[PATH_MAX_LENGTH];
   char                          *ptr = NULL;
   char                          *buf = nullptr;
   int64_t                        len = 0;
   const char *basename               = path_basename(path);
   size_t path_size                   = PATH_MAX_LENGTH * sizeof(char);
   char *tmp_path                     = (char*)malloc(path_size);

   include_path[0] = tmp[0] = '\0';

   strlcpy(tmp_path, path, path_size);
   path_resolve_realpath(tmp_path, path_size, false);

   if (!path_is_valid(tmp_path))
      strlcpy(tmp_path, path, path_size);

   if (!filestream_read_file(tmp_path, (void**)&buf, &len))
   {
      RARCH_ERR("Failed to open shader file: \"%s\".\n", tmp_path);
      free(tmp_path);
      return false;
   }

   free(tmp_path);

   /* Remove Windows \r chars if we encounter them.
    * filestream_read_file() allocates one extra for 0 terminator. */
   auto itr = remove_if(buf, buf + len + 1, [](char c) {
      return c == '\r';
   });

   if (itr < buf + len)
      *itr = '\0';

   /* Cannot use string_split since it removes blank lines (strtok). */
   ptr = buf;

   while (ptr && *ptr)
   {
      char *next_ptr = NULL;

      lines.push_back(ptr);

      next_ptr = strchr(ptr, '\n');

      if (next_ptr)
      {
         ptr = next_ptr + 1;
         *next_ptr = '\0';
      }
      else
         ptr = nullptr;
   }

   if (lines.empty())
      goto error;

   if (root_file)
   {
      if (strstr(lines[0], "#version ") != lines[0])
      {
         RARCH_ERR("First line of the shader must contain a valid #version string.\n");
         return false;
      }

      output->push_back(lines[0]);
      /* Allows us to use #line to make dealing with shader errors easier.
       * This is supported by glslang, but since we always use glslang statically,
       * this is fine. */
      output->push_back("#extension GL_GOOGLE_cpp_style_line_directive : require");
   }

   /* At least VIM treats the first line as line #1,
    * so offset everything by one. */
   snprintf(tmp, sizeof(tmp), "#line %u \"%s\"", root_file ? 2 : 1, basename);
   output->push_back(tmp);

   for (size_t i = root_file ? 1 : 0; i < lines.size(); i++)
   {
      const char *line = lines[i];
      if (strstr(line, "#include ") == line)
      {
         char *closing = NULL;
         char *c       = (char*)strchr(line, '"');

         if (!c)
         {
            RARCH_ERR("Invalid include statement \"%s\".\n", line);
            goto error;
         }

         c++;

         closing = (char*)strchr(c, '"');

         if (!closing)
         {
            RARCH_ERR("Invalid include statement \"%s\".\n", line);
            goto error;
         }

         *closing = '\0';

         fill_pathname_resolve_relative(include_path, path, c, sizeof(include_path));

         if (!glslang_read_shader_file(include_path, output, false))
            goto error;

         /* After including a file, use line directive
          * to pull it back to current file. */
         snprintf(tmp, sizeof(tmp), "#line %u \"%s\"", unsigned(i + 1), basename);
         output->push_back(tmp);
      }
      else if (strstr(line, "#endif") || strstr(line, "#pragma"))
      {
         /* #line seems to be ignored if preprocessor tests fail,
          * so we should reapply #line after each #endif.
          * Add extra offset here since we're setting #line
          * for the line after this one.
          */
         snprintf(tmp, sizeof(tmp), "#line %u \"%s\"", unsigned(i + 2), basename);
         output->push_back(line);
         output->push_back(tmp);
      }
      else
         output->push_back(line);
   }

   free(buf);
   return true;

error:
   free(buf);
   return false;
}

static string build_stage_source(const vector<string> &lines, const char *stage)
{
   ostringstream str;
   bool active = true;

   /* Version header. */
   str << lines.front();
   str << '\n';

   for (auto itr = begin(lines) + 1; itr != end(lines); ++itr)
   {
      if (itr->find("#pragma stage ") == 0)
      {
         if (stage)
         {
            auto expected = string("#pragma stage ") + stage;
            active = itr->find(expected) != string::npos;
         }
      }
      else if (itr->find("#pragma name ") == 0 ||
               itr->find("#pragma format ") == 0)
      {
         /* Ignore */
      }
      else if (active)
         str << *itr;
      str << '\n';
   }

   return str.str();
}

static const char *glslang_formats[] = {
   "UNKNOWN",

   "R8_UNORM",
   "R8_UINT",
   "R8_SINT",
   "R8G8_UNORM",
   "R8G8_UINT",
   "R8G8_SINT",
   "R8G8B8A8_UNORM",
   "R8G8B8A8_UINT",
   "R8G8B8A8_SINT",
   "R8G8B8A8_SRGB",

   "A2B10G10R10_UNORM_PACK32",
   "A2B10G10R10_UINT_PACK32",

   "R16_UINT",
   "R16_SINT",
   "R16_SFLOAT",
   "R16G16_UINT",
   "R16G16_SINT",
   "R16G16_SFLOAT",
   "R16G16B16A16_UINT",
   "R16G16B16A16_SINT",
   "R16G16B16A16_SFLOAT",

   "R32_UINT",
   "R32_SINT",
   "R32_SFLOAT",
   "R32G32_UINT",
   "R32G32_SINT",
   "R32G32_SFLOAT",
   "R32G32B32A32_UINT",
   "R32G32B32A32_SINT",
   "R32G32B32A32_SFLOAT",
};

const char *glslang_format_to_string(enum glslang_format fmt)
{
   return glslang_formats[fmt];
}

static glslang_format glslang_find_format(const char *fmt)
{
#undef FMT
#define FMT(x) if (string_is_equal(fmt, #x)) return SLANG_FORMAT_ ## x
   FMT(R8_UNORM);
   FMT(R8_UINT);
   FMT(R8_SINT);
   FMT(R8G8_UNORM);
   FMT(R8G8_UINT);
   FMT(R8G8_SINT);
   FMT(R8G8B8A8_UNORM);
   FMT(R8G8B8A8_UINT);
   FMT(R8G8B8A8_SINT);
   FMT(R8G8B8A8_SRGB);

   FMT(A2B10G10R10_UNORM_PACK32);
   FMT(A2B10G10R10_UINT_PACK32);

   FMT(R16_UINT);
   FMT(R16_SINT);
   FMT(R16_SFLOAT);
   FMT(R16G16_UINT);
   FMT(R16G16_SINT);
   FMT(R16G16_SFLOAT);
   FMT(R16G16B16A16_UINT);
   FMT(R16G16B16A16_SINT);
   FMT(R16G16B16A16_SFLOAT);

   FMT(R32_UINT);
   FMT(R32_SINT);
   FMT(R32_SFLOAT);
   FMT(R32G32_UINT);
   FMT(R32G32_SINT);
   FMT(R32G32_SFLOAT);
   FMT(R32G32B32A32_UINT);
   FMT(R32G32B32A32_SINT);
   FMT(R32G32B32A32_SFLOAT);

   return SLANG_FORMAT_UNKNOWN;
}

bool glslang_parse_meta(const vector<string> &lines, glslang_meta *meta)
{
   char id[64];
   char desc[64];

   id[0] = desc[0] = '\0';

   *meta           = glslang_meta{};

   for (auto &line : lines)
   {
      const char *line_c = line.c_str();

      if (line.find("#pragma name ") == 0)
      {
         const char *str = NULL;

         if (!meta->name.empty())
         {
            RARCH_ERR("[slang]: Trying to declare multiple names for file.\n");
            return false;
         }

         str = line_c + STRLEN_CONST("#pragma name ");

         while (*str == ' ')
            str++;
         meta->name = str;
      }
      else if (line.find("#pragma parameter ") == 0)
      {
         float initial, minimum, maximum, step;
         int ret = sscanf(line_c, "#pragma parameter %63s \"%63[^\"]\" %f %f %f %f",
               id, desc, &initial, &minimum, &maximum, &step);

         if (ret == 5)
         {
            step = 0.1f * (maximum - minimum);
            ret  = 6;
         }

         if (ret == 6)
         {
            auto itr = find_if(begin(meta->parameters), end(meta->parameters), [&](const glslang_parameter &param) {
                     return param.id == id;
                  });

            /* Allow duplicate #pragma parameter, but only
             * if they are exactly the same. */
            if (itr != end(meta->parameters))
            {
               if (   itr->desc    != desc    ||
                      itr->initial != initial ||
                      itr->minimum != minimum ||
                      itr->maximum != maximum ||
                      itr->step    != step
                  )
               {
                  RARCH_ERR("[slang]: Duplicate parameters found for \"%s\", but arguments do not match.\n", id);
                  return false;
               }
            }
            else
               meta->parameters.push_back({ id, desc, initial, minimum, maximum, step });
         }
         else
         {
            RARCH_ERR("[slang]: Invalid #pragma parameter line: \"%s\".\n", line_c);
            return false;
         }
      }
      else if (line.find("#pragma format ") == 0)
      {
         const char *str = NULL;

         if (meta->rt_format != SLANG_FORMAT_UNKNOWN)
         {
            RARCH_ERR("[slang]: Trying to declare format multiple times for file.\n");
            return false;
         }

         str = line_c + STRLEN_CONST("#pragma format ");

         while (*str == ' ')
            str++;

         meta->rt_format = glslang_find_format(str);
         if (meta->rt_format == SLANG_FORMAT_UNKNOWN)
         {
            RARCH_ERR("[slang]: Failed to find format \"%s\".\n", str);
            return false;
         }
      }
   }
   return true;
}

#if defined(HAVE_GLSLANG)
bool glslang_compile_shader(const char *shader_path, glslang_output *output)
{
   vector<string> lines;

   RARCH_LOG("[slang]: Compiling shader \"%s\".\n", shader_path);

   if (!glslang_read_shader_file(shader_path, &lines, true))
      return false;

   if (!glslang_parse_meta(lines, &output->meta))
      return false;

   if (    !glslang::compile_spirv(build_stage_source(lines, "vertex"),
            glslang::StageVertex, &output->vertex))
   {
      RARCH_ERR("Failed to compile vertex shader stage.\n");
      return false;
   }

   if (    !glslang::compile_spirv(build_stage_source(lines, "fragment"),
            glslang::StageFragment, &output->fragment))
   {
      RARCH_ERR("Failed to compile fragment shader stage.\n");
      return false;
   }

   return true;
}
#else
bool glslang_compile_shader(const char *shader_path, glslang_output *output)
{
   return false;
}
#endif

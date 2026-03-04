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

#include <string.h>

#include <retro_miscellaneous.h>
#include <file/file_path.h>
#include <file/config_file.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "glslang_util.h"
#include "../../verbosity.h"

static char *slang_get_include_file(char *line, size_t len)
{
   char *end   = NULL;
   char *start = (char*)memchr(line, '\"', len);
   if (!start)
      return NULL;

   start++;
   len -= (size_t)(start - line);

   if (!(end = (char*)memchr(start, '\"', len)))
      return NULL;

   *end = '\0';
   return start;
}

bool slang_texture_semantic_is_array(enum slang_texture_semantic sem)
{
   switch (sem)
   {
      case SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY:
      case SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT:
      case SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK:
      case SLANG_TEXTURE_SEMANTIC_USER:
         return true;

      default:
         break;
   }

   return false;
}

enum slang_texture_semantic slang_name_to_texture_semantic_array(
      const char *name, const char **names, unsigned *index)
{
   unsigned i;
   for (i = 0; *names; i++, names++)
   {
      enum slang_texture_semantic semantic = (enum slang_texture_semantic)i;

      if (slang_texture_semantic_is_array(semantic))
      {
         size_t _len = strlen(*names);
         if (strncmp(*names, name, _len) == 0)
         {
            *index = (unsigned)strtoul(name + _len, NULL, 10);
            return semantic;
         }
      }
      else if (string_is_equal(name, *names))
      {
         *index = 0;
         return semantic;
      }
   }
   return SLANG_INVALID_TEXTURE_SEMANTIC;
}

bool glslang_read_shader_file(const char *path,
      struct string_list *output, bool root_file, bool is_optional)
{
   char tmp[PATH_MAX_LENGTH];
   union string_list_elem_attr attr;
   const char *basename      = NULL;
   uint8_t *buf              = NULL;
   int64_t buf_len           = 0;
   bool    ret               = false;

   tmp[0] = '\0';
   attr.i = 0;

   /* Sanity check */
   if (string_is_empty(path) || !output)
      return false;

   basename = path_basename_nocompression(path);

   if (string_is_empty(basename))
      return false;

   /* Read file contents */
   if (!filestream_read_file(path, (void**)&buf, &buf_len))
   {
      if (!is_optional)
         RARCH_ERR("[Slang] Failed to open shader file: \"%s\".\n", path);
      return false;
   }

   if (buf_len <= 0)
      goto cleanup;

   /* Remove Windows '\r' chars if we encounter them */
   string_remove_all_chars((char*)buf, '\r');

   {
      char *cursor     = (char*)buf;
      size_t line_idx  = 0;
      bool first_line  = true;

      /* If this is the 'parent' shader file and a slang file,
       * ensure that first line is a 'VERSION' string */
      bool check_version = root_file
            && string_is_equal(path_get_extension(path), "slang");

      while (*cursor != '\0')
      {
         char saved;
         char *line_start = cursor;
         char *newline    = cursor;
         while (*newline != '\n' && *newline != '\0')
            newline++;
         saved            = *newline;

         /* Temporarily terminate the line */
         *newline = '\0';

         if (first_line)
         {
            first_line = false;

            if (check_version)
            {
               if (strncmp("#version ", line_start, STRLEN_CONST("#version ")))
               {
                  RARCH_ERR("[Slang] First line of the shader must contain a valid "
                        "#version string.\n");
                  *newline = saved;
                  goto cleanup;
               }

               if (!string_list_append(output, line_start, attr))
               {
                  *newline = saved;
                  goto cleanup;
               }

               /* Allows us to use #line to make dealing with shader
                * errors easier. */
               if (!string_list_append(output,
                     "#extension GL_GOOGLE_cpp_style_line_directive : require",
                     attr))
               {
                  *newline = saved;
                  goto cleanup;
               }

               /* Append feature defines */
               if (!string_list_append(output, "#define _HAS_ORIGINALASPECT_UNIFORMS", attr)
                     || !string_list_append(output, "#define _HAS_FRAMETIME_UNIFORMS", attr)
                     || !string_list_append(output, "#define _HAS_SENSOR_UNIFORMS", attr))
               {
                  *newline = saved;
                  goto cleanup;
               }

               snprintf(tmp, sizeof(tmp), "#line 2 \"%s\"", basename);
               if (!string_list_append(output, tmp, attr))
               {
                  *newline = saved;
                  goto cleanup;
               }

               /* Advance to line 2 for processing below */
               *newline = saved;
               cursor   = (*newline == '\n') ? newline + 1 : newline;
               line_idx++;
               continue;
            }

            /* Non-root or non-slang: emit feature defines + #line once */
            if (root_file)
            {
               if (!string_list_append(output, "#define _HAS_ORIGINALASPECT_UNIFORMS", attr)
                     || !string_list_append(output, "#define _HAS_FRAMETIME_UNIFORMS", attr)
                     || !string_list_append(output, "#define _HAS_SENSOR_UNIFORMS", attr))
               {
                  *newline = saved;
                  goto cleanup;
               }

               snprintf(tmp, sizeof(tmp), "#line 2 \"%s\"", basename);
               if (!string_list_append(output, tmp, attr))
               {
                  *newline = saved;
                  goto cleanup;
               }
            }
            else
            {
               if (!string_list_append(output, "#define _HAS_ORIGINALASPECT_UNIFORMS", attr)
                     || !string_list_append(output, "#define _HAS_FRAMETIME_UNIFORMS", attr)
                     || !string_list_append(output, "#define _HAS_SENSOR_UNIFORMS", attr))
               {
                  *newline = saved;
                  goto cleanup;
               }

               snprintf(tmp, sizeof(tmp), "#line 1 \"%s\"", basename);
               if (!string_list_append(output, tmp, attr))
               {
                  *newline = saved;
                  goto cleanup;
               }
            }
         }

         /* Skip line 0 (the #version line) for root slang files —
          * already handled above */
         if (root_file && check_version && line_idx == 0)
         {
            *newline = saved;
            cursor   = (*newline == '\n') ? newline + 1 : newline;
            line_idx++;
            continue;
         }

         /* Process the line */
         {
            bool include_optional = !strncmp("#pragma include_optional ",
                  line_start, STRLEN_CONST("#pragma include_optional "));

            if (  !strncmp("#include ", line_start, STRLEN_CONST("#include "))
                  || include_optional)
            {
               char include_path[PATH_MAX_LENGTH];
               char *include_file = slang_get_include_file(
                     line_start, strlen(line_start));

               if (string_is_empty(include_file))
               {
                  RARCH_ERR("[Slang] Invalid include statement \"%s\".\n",
                        line_start);
                  *newline = saved;
                  goto cleanup;
               }

               include_path[0] = '\0';
               fill_pathname_resolve_relative(
                     include_path, path, include_file, sizeof(include_path));

               *newline = saved;

               if (!glslang_read_shader_file(include_path, output,
                     false, include_optional))
               {
                  if (!include_optional)
                     goto cleanup;
                  RARCH_LOG("[Slang] Optional include not found \"%s\".\n",
                        include_path);
               }

               snprintf(tmp, sizeof(tmp), "#line %u \"%s\"",
                     (unsigned)(line_idx + 1), basename);
               if (!string_list_append(output, tmp, attr))
                  goto cleanup;
            }
            else if (  !strncmp("#endif",  line_start, STRLEN_CONST("#endif"))
                    || !strncmp("#pragma", line_start, STRLEN_CONST("#pragma")))
            {
               if (!string_list_append(output, line_start, attr))
               {
                  *newline = saved;
                  goto cleanup;
               }
               snprintf(tmp, sizeof(tmp), "#line %u \"%s\"",
                     (unsigned)(line_idx + 2), basename);
               *newline = saved;
               if (!string_list_append(output, tmp, attr))
                  goto cleanup;
            }
            else
            {
               if (!string_list_append(output, line_start, attr))
               {
                  *newline = saved;
                  goto cleanup;
               }
               *newline = saved;
            }

            cursor = (*newline == '\n') ? newline + 1 : newline;
            line_idx++;
            continue;
         }

         *newline = saved;
         goto cleanup;
      }
   }

   ret = true;

cleanup:
   if (buf)
      free(buf);
   return ret;
}

const char *glslang_format_to_string(enum glslang_format fmt)
{
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
   return glslang_formats[fmt];
}

enum glslang_format glslang_find_format(const char *fmt)
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

unsigned glslang_num_miplevels(unsigned width, unsigned height)
{
   unsigned size   = MAX(width, height);
   unsigned levels = 0;
   while (size)
   {
      levels++;
      size >>= 1;
   }
   return levels;
}

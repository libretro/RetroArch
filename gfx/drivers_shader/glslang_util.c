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

#include <retro_miscellaneous.h>
#include <file/file_path.h>
#include <file/config_file.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "glslang_util.h"
#include "../../verbosity.h"

static void get_include_file(
      const char *line, char *include_file, size_t len)
{
   char *end   = NULL;
   char *start = (char*)strchr(line, '\"');

   if (!start)
      return;

   start++;
   end = (char*)strchr(start, '\"');

   if (!end)
      return;

   *end = '\0';
   strlcpy(include_file, start, len);
}

bool glslang_read_shader_file(const char *path,
      struct string_list *output, bool root_file)
{
   char tmp[PATH_MAX_LENGTH];
   union string_list_elem_attr attr;
   size_t i;
   const char *basename      = NULL;
   uint8_t *buf              = NULL;
   int64_t buf_len           = 0;
   struct string_list *lines = NULL;

   tmp[0] = '\0';
   attr.i = 0;

   /* Sanity check */
   if (string_is_empty(path) || !output)
      return false;

   basename      = path_basename(path);

   if (string_is_empty(basename))
      return false;

   /* Read file contents */
   if (!filestream_read_file(path, (void**)&buf, &buf_len))
   {
      RARCH_ERR("Failed to open shader file: \"%s\".\n", path);
      return false;
   }

   if (buf_len > 0)
   {
      /* Remove Windows '\r' chars if we encounter them */
      string_remove_all_chars((char*)buf, '\r');

      /* Split into lines
       * (Blank lines must be included) */
      lines = string_separate((char*)buf, "\n");
   }

   /* Buffer is no longer required - clean up */
   if (buf)
      free(buf);

   /* Sanity check */
   if (!lines)
      return false;

   if (lines->size < 1)
      goto error;

   /* If this is the 'parent' shader file, ensure that first
    * line is a 'VERSION' string */
   if (root_file)
   {
      const char *line = lines->elems[0].data;

      if (strncmp("#version ", line, STRLEN_CONST("#version ")))
      {
         RARCH_ERR("First line of the shader must contain a valid #version string.\n");
         goto error;
      }

      if (!string_list_append(output, line, attr))
         goto error;

      /* Allows us to use #line to make dealing with shader errors easier.
       * This is supported by glslang, but since we always use glslang statically,
       * this is fine. */

      if (!string_list_append(output, "#extension GL_GOOGLE_cpp_style_line_directive : require", attr))
         goto error;
   }

   /* At least VIM treats the first line as line #1,
    * so offset everything by one. */
   snprintf(tmp, sizeof(tmp), "#line %u \"%s\"", root_file ? 2 : 1, basename);
   if (!string_list_append(output, tmp, attr))
      goto error;

   /* Loop through lines of file */
   for (i = root_file ? 1 : 0; i < lines->size; i++)
   {
      unsigned push_line = 0;
      const char *line   = lines->elems[i].data;

      /* Check for 'include' statements */
      if (!strncmp("#include ", line, STRLEN_CONST("#include ")))
      {
         char include_file[PATH_MAX_LENGTH];
         char include_path[PATH_MAX_LENGTH];

         include_file[0] = '\0';
         include_path[0] = '\0';

         /* Build include file path */
         get_include_file(line, include_file, sizeof(include_file));

         if (string_is_empty(include_file))
         {
            RARCH_ERR("Invalid include statement \"%s\".\n", line);
            goto error;
         }

         fill_pathname_resolve_relative(
               include_path, path, include_file, sizeof(include_path));

         /* Parse include file */
         if (!glslang_read_shader_file(include_path, output, false))
            goto error;

         /* After including a file, use line directive
          * to pull it back to current file. */
         push_line = 1;
      }
      else if (!strncmp("#endif", line, STRLEN_CONST("#endif")) ||
               !strncmp("#pragma", line, STRLEN_CONST("#pragma")))
      {
         /* #line seems to be ignored if preprocessor tests fail,
          * so we should reapply #line after each #endif.
          * Add extra offset here since we're setting #line
          * for the line after this one.
          */
         push_line = 2;
         if (!string_list_append(output, line, attr))
            goto error;
      }
      else
         if (!string_list_append(output, line, attr))
            goto error;

      if (push_line != 0)
      {
         snprintf(tmp, sizeof(tmp), "#line %u \"%s\"",
               (unsigned)(i + push_line), basename);
         if (!string_list_append(output, tmp, attr))
            goto error;
      }
   }

   string_list_free(lines);

   return true;

error:

   if (lines)
      string_list_free(lines);

   return false;
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

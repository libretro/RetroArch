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
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "glslang_util.h"
#include "glslang_util_cxx.h"
#if defined(HAVE_GLSLANG)
#include <glslang.hpp>
#endif
#include "../../verbosity.h"

static std::string build_stage_source(
      const struct string_list *lines, const char *stage)
{
   /* Note: since we have to return a std::string anyway,
    * there is nothing to be gained from trying to replace
    * this ostringstream with a C-based alternative
    * (would require a rewrite of deps/glslang/glslang.cpp) */
   std::ostringstream str;
   bool active = true;
   size_t i;

   if (!lines)
      return "";

   if (lines->size < 1)
      return "";

   /* Version header. */
   str << lines->elems[0].data;;
   str << '\n';

   for (i = 1; i < lines->size; i++)
   {
      const char *line = lines->elems[i].data;

      /* Identify 'stage' (fragment/vertex) */
      if (!strncmp("#pragma stage ", line, STRLEN_CONST("#pragma stage ")))
      {
         if (!string_is_empty(stage))
         {
            char expected[128];

            expected[0] = '\0';

            strlcpy(expected, "#pragma stage ", sizeof(expected));
            strlcat(expected, stage,            sizeof(expected));

            active = strcmp(expected, line) == 0;
         }
      }
      else if (!strncmp("#pragma name ", line, STRLEN_CONST("#pragma name ")) ||
               !strncmp("#pragma format ", line, STRLEN_CONST("#pragma format ")))
      {
         /* Ignore */
      }
      else if (active)
         str << line;

      str << '\n';
   }

   return str.str();
}

bool glslang_parse_meta(const struct string_list *lines, glslang_meta *meta)
{
   char id[64];
   char desc[64];
   size_t i;

   id[0]   = '\0';
   desc[0] = '\0';

   for (i = 0; i < lines->size; i++)
   {
      const char *line = lines->elems[i].data;

      /* Check for shader identifier */
      if (!strncmp("#pragma name ", line, STRLEN_CONST("#pragma name ")))
      {
         const char *str = NULL;

         if (!meta->name.empty())
         {
            RARCH_ERR("[slang]: Trying to declare multiple names for file.\n");
            return false;
         }

         str = line + STRLEN_CONST("#pragma name ");
         while (*str == ' ')
            str++;

         meta->name = str;
      }
      /* Check for shader parameters */
      else if (!strncmp("#pragma parameter ", line, STRLEN_CONST("#pragma parameter ")))
      {
         float initial, minimum, maximum, step;
         int ret = sscanf(
               line, "#pragma parameter %63s \"%63[^\"]\" %f %f %f %f",
               id, desc, &initial, &minimum, &maximum, &step);

         if (ret == 5)
         {
            step = 0.1f * (maximum - minimum);
            ret  = 6;
         }

         if (ret == 6)
         {
            bool parameter_found   = false;
            size_t parameter_index = 0;
            size_t j;

            for (j = 0; j < meta->parameters.size(); j++)
            {
               /* Note: LHS is a std:string, RHS is a C string.
                * (the glslang_meta stuff has to be C++) */
               if (meta->parameters[j].id == id)
               {
                  parameter_found = true;
                  parameter_index = j;
                  break;
               }
            }

            /* Allow duplicate #pragma parameter, but only
             * if they are exactly the same. */
            if (parameter_found)
            {
               const glslang_parameter *parameter = 
                  &meta->parameters[parameter_index];

               if (   parameter->desc    != desc    ||
                      parameter->initial != initial ||
                      parameter->minimum != minimum ||
                      parameter->maximum != maximum ||
                      parameter->step    != step
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
            RARCH_ERR("[slang]: Invalid #pragma parameter line: \"%s\".\n", line);
            return false;
         }
      }
      /* Check for framebuffer format */
      else if (!strncmp("#pragma format ", line, STRLEN_CONST("#pragma format ")))
      {
         const char *str = NULL;

         if (meta->rt_format != SLANG_FORMAT_UNKNOWN)
         {
            RARCH_ERR("[slang]: Trying to declare format multiple times for file.\n");
            return false;
         }

         str = line + STRLEN_CONST("#pragma format ");
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

bool glslang_compile_shader(const char *shader_path, glslang_output *output)
{
#if defined(HAVE_GLSLANG)
   struct string_list *lines = string_list_new();

   if (!lines)
      return false;

   RARCH_LOG("[slang]: Compiling shader \"%s\".\n", shader_path);

   if (!glslang_read_shader_file(shader_path, lines, true))
      goto error;
   output->meta = glslang_meta{};
   if (!glslang_parse_meta(lines, &output->meta))
      goto error;

   if (    !glslang::compile_spirv(build_stage_source(lines, "vertex"),
            glslang::StageVertex, &output->vertex))
   {
      RARCH_ERR("Failed to compile vertex shader stage.\n");
      goto error;
   }

   if (    !glslang::compile_spirv(build_stage_source(lines, "fragment"),
            glslang::StageFragment, &output->fragment))
   {
      RARCH_ERR("Failed to compile fragment shader stage.\n");
      goto error;
   }

   string_list_free(lines);

   return true;

error:

   if (lines)
      string_list_free(lines);
#endif

   return false;
}

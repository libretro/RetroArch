/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2016 - Hans-Kristian Arntzen
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
#include <string>
#include <sstream>

#include "glslang_util.hpp"
#include "glslang.hpp"

#include "../../general.h"
#include <retro_file.h>
#include <string/string_list.h>

using namespace std;

bool read_file(const char *path, vector<string> *output)
{
   char              *buf   = nullptr;
   ssize_t              len = 0;
   struct string_list *list = NULL;

   if (retro_read_file(path, (void**)&buf, &len) < 0)
   {
      RARCH_ERR("Failed to open shader file: \"%s\".\n", path);
      return false;
   }

   list = string_split(buf, "\n");

   if (!list)
   {
      free(buf);
      return false;
   }

   if (list->size == 0)
   {
      free(buf);
      string_list_free(list);
      return false;
   }

   output->clear();
   for (size_t i = 0; i < list->size; i++)
      output->push_back(list->elems[i].data);

   string_list_free(list);
   return true;
}

string build_stage_source(const vector<string> &lines, const char *stage)
{
   ostringstream str;
   bool active = true;

   // Version header.
   str << lines.front();
   str << '\n';

   for (auto itr = begin(lines) + 1; itr != end(lines); ++itr)
   {
      if (itr->find("#pragma stage ") != string::npos)
      {
         auto expected = string("#pragma stage ") + stage;
         active = itr->find(expected) != string::npos;

         // Improve debuggability.
         if (active)
         {
            str << "#line ";
            str << (itr - begin(lines)) + 2;
            str << '\n';
         }
      }
      else if (active)
         str << *itr;
      str << '\n';
   }

   return str.str();
}

bool glslang_compile_shader(const char *shader_path, glslang_output *output)
{
   vector<string> lines;

   RARCH_LOG("Compiling shader \"%s\".\n", shader_path);
   if (!read_file(shader_path, &lines))
      return false;

   auto &header = lines.front();
   if (header.find_first_of("#version ") == string::npos)
   {
      RARCH_ERR("First line of the shader must contain a valid #version string.\n");
      return false;
   }

   if (!glslang::compile_spirv(build_stage_source(lines, "vertex"),
            glslang::StageVertex, &output->vertex))
   {
      RARCH_ERR("Failed to compile vertex shader stage.\n");
      return false;
   }

   if (!glslang::compile_spirv(build_stage_source(lines, "fragment"),
            glslang::StageFragment, &output->fragment))
   {
      RARCH_ERR("Failed to compile fragment shader stage.\n");
      return false;
   }

   return true;
}


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

#include "slang_preprocess.h"
#include "glslang_util.h"
#include <vector>
#include <string>
#include <algorithm>

#include <compat/strl.h>

#include "../../verbosity.h"

using namespace std;

bool slang_preprocess_parse_parameters(glslang_meta& meta,
      struct video_shader *shader)
{
   unsigned old_num_parameters = shader->num_parameters;

   /* Assumes num_parameters is
    * initialized to something sane. */
   for (auto &param : meta.parameters)
   {
      bool mismatch_dup = false;
      bool dup          = false;
      auto itr          = find_if(shader->parameters,
            shader->parameters + shader->num_parameters,
         [&](const video_shader_parameter &parsed_param)
         {
            return param.id == parsed_param.id;
         });

      if (itr != shader->parameters + shader->num_parameters)
      {
         dup = true;
         /* Allow duplicate #pragma parameter, but only
          * if they are exactly the same. */
         if (param.desc != itr->desc ||
               param.initial != itr->initial ||
               param.minimum != itr->minimum ||
               param.maximum != itr->maximum ||
               param.step != itr->step)
         {
            RARCH_ERR("[slang]: Duplicate parameters"
                  " found for \"%s\", but arguments do not match.\n",
                  itr->id);
            mismatch_dup = true;
         }
      }

      if (dup && !mismatch_dup)
         continue;

      if (mismatch_dup || shader->num_parameters == GFX_MAX_PARAMETERS)
      {
         shader->num_parameters = old_num_parameters;
         return false;
      }

      auto &p = shader->parameters[shader->num_parameters++];
      strlcpy(p.id, param.id.c_str(), sizeof(p.id));
      strlcpy(p.desc, param.desc.c_str(), sizeof(p.desc));
      p.initial = param.initial;
      p.minimum = param.minimum;
      p.maximum = param.maximum;
      p.step    = param.step;
      p.current = param.initial;
   }

   return true;
}

bool slang_preprocess_parse_parameters(const char *shader_path,
      struct video_shader *shader)
{
   glslang_meta meta;
   vector<string> lines;

   if (!glslang_read_shader_file(shader_path, &lines, true))
      return false;
   if (!glslang_parse_meta(lines, &meta))
      return false;
   return slang_preprocess_parse_parameters(meta, shader);
}

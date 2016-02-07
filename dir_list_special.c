/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <file/dir_list.h>

#include "dir_list_special.h"
#include "frontend/frontend_driver.h"
#include "configuration.h"
#include "core_info.h"

struct string_list *dir_list_new_special(const char *input_dir,
      enum dir_list_type type, const char *filter)
{
   char ext_name[PATH_MAX_LENGTH];
   const char *dir   = NULL;
   const char *exts  = NULL;
   bool include_dirs = false;

   settings_t *settings = config_get_ptr();

   (void)input_dir;
   (void)settings;

   switch (type)
   {
      case DIR_LIST_CORES:
         dir  = settings->libretro_directory;

         if (!frontend_driver_get_core_extension(ext_name, sizeof(ext_name)))
            return NULL;

         exts = ext_name;
         break;
      case DIR_LIST_CORE_INFO:
         dir  = input_dir;
         exts = core_info_list_get_all_extensions();
         break;
      case DIR_LIST_SHADERS:
         dir  = settings->video.shader_dir;
         exts = "cg|cgp|glsl|glslp";
         break;
      case DIR_LIST_COLLECTIONS:
         dir  = settings->playlist_directory;
         exts = "lpl";
         break;
      case DIR_LIST_DATABASES:
         dir  = settings->content_database;
         exts = "rdb";
         break;
      case DIR_LIST_PLAIN:
         dir  = input_dir;
         exts = filter;
         break;
      case DIR_LIST_NONE:
      default:
         return NULL;
   }

   return dir_list_new(dir, exts, include_dirs, type == DIR_LIST_CORE_INFO);
}

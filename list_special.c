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

#include <string.h>

#include <lists/dir_list.h>
#include <compat/strl.h>

#include "list_special.h"
#include "frontend/frontend_driver.h"
#include "configuration.h"
#include "core_info.h"

#ifdef HAVE_MENU
#include "menu/menu_driver.h"
#endif

#ifdef HAVE_CAMERA
#include "camera/camera_driver.h"
#endif

#ifdef HAVE_LOCATION
#include "location/location_driver.h"
#endif

#include "core_info.h"
#include "gfx/video_driver.h"
#include "input/input_driver.h"
#include "input/input_hid_driver.h"
#include "input/input_joypad_driver.h"
#include "audio/audio_driver.h"
#include "audio/audio_resampler_driver.h"
#include "record/record_driver.h"

struct string_list *dir_list_new_special(const char *input_dir,
      enum dir_list_type type, const char *filter)
{
   char ext_shaders[PATH_MAX_LENGTH];
   char ext_name[PATH_MAX_LENGTH];
   const char *dir   = NULL;
   const char *exts  = NULL;
   bool include_dirs = false;

   settings_t *settings = config_get_ptr();

   (void)input_dir;
   (void)settings;
   (void)ext_shaders;

   switch (type)
   {
      case DIR_LIST_CORES:
         dir  = settings->directory.libretro;

         if (!frontend_driver_get_core_extension(ext_name, sizeof(ext_name)))
            return NULL;

         exts = ext_name;
         break;
      case DIR_LIST_CORE_INFO:
         {
            core_info_list_t *list = NULL;
            core_info_ctl(CORE_INFO_CTL_LIST_GET, &list);

            dir  = input_dir;
            exts = list->all_ext;
         }
         break;
      case DIR_LIST_SHADERS:
         dir  = settings->directory.video_shader;
#ifdef HAVE_CG
         strlcat(ext_shaders, "cg|cgp", sizeof(ext_shaders));
#endif
#ifdef HAVE_GLSL
         strlcat(ext_shaders, "glsl|glslp", sizeof(ext_shaders));
#endif
#ifdef HAVE_VULKAN
         strlcat(ext_shaders, "slang|slangp", sizeof(ext_shaders));
#endif
         exts = ext_shaders;
         break;
      case DIR_LIST_COLLECTIONS:
         dir  = settings->directory.playlist;
         exts = "lpl";
         break;
      case DIR_LIST_DATABASES:
         dir  = settings->path.content_database;
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

struct string_list *string_list_new_special(enum string_list_type type,
      void *data, unsigned *len, size_t *list_size)
{
   union string_list_elem_attr attr;
   unsigned i;
   core_info_list_t *core_info_list = NULL;
   const core_info_t *core_info     = NULL;
   struct string_list *s            = string_list_new();

   if (!s || !len)
      return NULL;

   attr.i = 0;
   *len   = 0;

   switch (type)
   {
      case STRING_LIST_MENU_DRIVERS:
#ifdef HAVE_MENU
         for (i = 0; menu_driver_find_handle(i); i++)
         {
            const char *opt  = menu_driver_find_ident(i);
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
#endif
      case STRING_LIST_CAMERA_DRIVERS:
#ifdef HAVE_CAMERA
         for (i = 0; camera_driver_find_handle(i); i++)
         {
            const char *opt  = camera_driver_find_ident(i);
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
#endif
      case STRING_LIST_LOCATION_DRIVERS:
#ifdef HAVE_LOCATION
         for (i = 0; location_driver_find_handle(i); i++)
         {
            const char *opt  = location_driver_find_ident(i);
            *len            += strlen(opt) + 1;
            string_list_append(options_l, opt, attr);
         }
         break;
#endif
      case STRING_LIST_AUDIO_DRIVERS:
         for (i = 0; audio_driver_find_handle(i); i++)
         {
            const char *opt  = audio_driver_find_ident(i);
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_AUDIO_RESAMPLER_DRIVERS:
         for (i = 0; audio_resampler_driver_find_handle(i); i++)
         {
            const char *opt  = audio_resampler_driver_find_ident(i);
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_VIDEO_DRIVERS:
         for (i = 0; video_driver_find_handle(i); i++)
         {
            const char *opt  = video_driver_find_ident(i);
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_INPUT_DRIVERS:
         for (i = 0; input_driver_find_handle(i); i++)
         {
            const char *opt  = input_driver_find_ident(i);
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_INPUT_HID_DRIVERS:
         for (i = 0; hid_driver_find_handle(i); i++)
         {
            const char *opt  = hid_driver_find_ident(i);
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_INPUT_JOYPAD_DRIVERS:
         for (i = 0; joypad_driver_find_handle(i); i++)
         {
            const char *opt  = joypad_driver_find_ident(i);
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_RECORD_DRIVERS:
         for (i = 0; record_driver_find_handle(i); i++)
         {
            const char *opt  = record_driver_find_ident(i);
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_SUPPORTED_CORES_PATHS:
         core_info_ctl(CORE_INFO_CTL_LIST_GET, &core_info_list);

         core_info_list_get_supported_cores(core_info_list,
               (const char*)data, &core_info, list_size);

         if (*list_size == 0)
            goto error;

         for (i = 0; i < *list_size; i++)
         {
            const char *opt  = NULL;
            const core_info_t *info = (const core_info_t*)&core_info[i];
            opt              = info ? info->path : NULL;

            if (!opt)
               goto error;

            *len           += strlen(opt) + 1;
            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_SUPPORTED_CORES_NAMES:
         core_info_ctl(CORE_INFO_CTL_LIST_GET, &core_info_list);
         core_info_list_get_supported_cores(core_info_list,
               (const char*)data, &core_info, list_size);

         if (*list_size == 0)
            goto error;

         for (i = 0; i < *list_size; i++)
         {
            const char *opt  = NULL;
            const core_info_t *info  = (const core_info_t*)&core_info[i];
            opt              = info ? info->display_name : NULL;

            if (!opt)
               goto error;

            *len            += strlen(opt) + 1;
            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_NONE:
      default:
         goto error;
   }

   return s;

error:
   string_list_free(s);
   s    = NULL;
   *len = 0;
   return NULL;
}

const char *char_list_new_special(enum string_list_type type, void *data)
{
   unsigned len = 0;
   size_t list_size;
   struct string_list *s = string_list_new_special(type, data, &len, &list_size);
   char         *options = (len > 0) ? (char*)calloc(len, sizeof(char)): NULL;

   if (options && s)
      string_list_join_concat(options, len, s, "|");

   string_list_free(s);
   s = NULL;

   return options;
}

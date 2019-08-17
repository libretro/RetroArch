/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2017 - Daniel De Matteis
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <boolean.h>
#include <stddef.h>
#include <string.h>

#include <file/config_file.h>
#include <file/file_path.h>
#include <lists/dir_list.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <compat/strl.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "frontend_driver.h"
#include "../defaults.h"
#include "../verbosity.h"

struct defaults g_defaults;

/*We need to set libretro to the first entry in the cores
 * directory so that it will be saved to the config file
 */
static void find_first_libretro_core(char *first_file,
   size_t size_of_first_file, const char *dir,
   const char * ext)
{
   size_t i;
   bool                 ret = false;
   struct string_list *list = dir_list_new(dir, ext, false, true, false, false);

   if (!list)
   {
      RARCH_ERR("Couldn't read directory."
            " Cannot infer default libretro core.\n");
      return;
   }

   RARCH_LOG("Searching for valid libretro implementation in: \"%s\".\n",
         dir);

   for (i = 0; i < list->size && !ret; i++)
   {
      char fname[PATH_MAX_LENGTH]           = {0};
      char salamander_name[PATH_MAX_LENGTH] = {0};
      const char *libretro_elem             = (const char*)list->elems[i].data;

      RARCH_LOG("Checking library: \"%s\".\n", libretro_elem);

      if (!libretro_elem)
         continue;

      fill_pathname_base(fname, libretro_elem, sizeof(fname));

      if (!frontend_driver_get_salamander_basename(
               salamander_name, sizeof(salamander_name)))
         break;

      if (!strncmp(fname, salamander_name, sizeof(fname)))
      {
         if (list->size == (i + 1))
         {
            RARCH_WARN("Entry is RetroArch Salamander itself, "
                  "but is last entry. No choice but to set it.\n");
            strlcpy(first_file, fname, size_of_first_file);
         }

         continue;
      }

      strlcpy(first_file, fname, size_of_first_file);
      RARCH_LOG("First found libretro core is: \"%s\".\n", first_file);
      ret = true;
   }

   dir_list_free(list);
}

/* Last fallback - we'll need to start the first executable file
 * we can find in the RetroArch cores directory.
 */
static void find_and_set_first_file(char *s, size_t len,
      const char *ext)
{

   char first_file[PATH_MAX_LENGTH] = {0};
   find_first_libretro_core(first_file, sizeof(first_file),
         g_defaults.dirs[DEFAULT_DIR_CORE], ext);

   if (string_is_empty(first_file))
   {
      RARCH_ERR("Failed last fallback - RetroArch Salamander will exit.\n");
      return;
   }

   fill_pathname_join(s, g_defaults.dirs[DEFAULT_DIR_CORE], first_file, len);
   RARCH_LOG("libretro_path now set to: %s.\n", s);
}

static void salamander_init(char *s, size_t len)
{
   /* normal executable loading path */
   bool config_exists = config_file_exists(g_defaults.path.config);

   if (config_exists)
   {
      char tmp_str[PATH_MAX_LENGTH] = {0};
      config_file_t * conf          = config_file_new(g_defaults.path.config);

      if (conf)
      {
         config_get_array(conf, "libretro_path", tmp_str, sizeof(tmp_str));
         config_file_free(conf);

         if (memcmp(tmp_str, "builtin", 7) != 0)
            strlcpy(s, tmp_str, len);
      }
#ifdef GEKKO
      /* stupid libfat bug or something; sometimes it says
       * the file is there when it doesn't. */
      else
      {
         config_exists = false;
      }
#endif
   }

   if (!config_exists || string_is_equal(s, ""))
   {
      char executable_name[PATH_MAX_LENGTH] = {0};

      frontend_driver_get_core_extension(
            executable_name, sizeof(executable_name));
      find_and_set_first_file(s, len, executable_name);
   }
   else
      RARCH_LOG("Start [%s] found in retroarch.cfg.\n", s);

   if (!config_exists)
   {
      config_file_t *conf = config_file_new_alloc();

      if (conf)
      {
         config_set_string(conf, "libretro_path", s);
         config_file_write(conf, g_defaults.path.config, true);
         config_file_free(conf);
      }
   }
}
#ifdef HAVE_MAIN
int salamander_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
   char libretro_path[PATH_MAX_LENGTH] = {0};
   void *args                          = NULL;
   struct rarch_main_wrap *wrap_args   = NULL;
   frontend_ctx_driver_t *frontend_ctx = (frontend_ctx_driver_t*)frontend_ctx_init_first();

   if (!frontend_ctx)
      return 0;

   if (frontend_ctx && frontend_ctx->init)
      frontend_ctx->init(args);

   if (frontend_ctx && frontend_ctx->environment_get)
      frontend_ctx->environment_get(&argc, argv, args, wrap_args);

   salamander_init(libretro_path, sizeof(libretro_path));

   if (frontend_ctx && frontend_ctx->deinit)
      frontend_ctx->deinit(args);

   if (frontend_ctx && frontend_ctx->exitspawn)
      frontend_ctx->exitspawn(libretro_path, sizeof(libretro_path));

   return 1;
}

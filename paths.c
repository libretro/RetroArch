/*  RetroArch - A frontend for libretro.
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

#include <compat/strl.h>
#include <file/file_path.h>
#include <lists/string_list.h>
#include <string/stdstring.h>
#include <retro_assert.h>
#include <retro_stat.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "paths.h"

#include "configuration.h"
#include "command.h"
#include "content.h"
#include "dynamic.h"
#include "file_path_special.h"

#include "core.h"
#include "msg_hash.h"
#include "retroarch.h"
#include "runloop.h"
#include "verbosity.h"

#define MENU_VALUE_NO_CORE 0x7d5472cbU

static char current_savefile_dir[PATH_MAX_LENGTH]       = {0};

void path_set_redirect(void)
{
   char current_savestate_dir[PATH_MAX_LENGTH] = {0};
   uint32_t global_library_name_hash           = 0;
   bool check_global_library_name_hash         = false;
   global_t                *global             = global_get_ptr();
   settings_t              *settings           = config_get_ptr();
   rarch_system_info_t      *info              = NULL;

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &info);

   if (!global)
      return;

   if (info->info.library_name &&
         !string_is_empty(info->info.library_name))
      global_library_name_hash =
         msg_hash_calculate(info->info.library_name);

   /* Initialize current save directories
    * with the values from the config. */
   strlcpy(current_savefile_dir,
         global->dir.savefile,
         sizeof(current_savefile_dir));
   strlcpy(current_savestate_dir,
         global->dir.savestate,
         sizeof(current_savestate_dir));

   check_global_library_name_hash = (global_library_name_hash != 0);
#ifdef HAVE_MENU
   check_global_library_name_hash = check_global_library_name_hash &&
      (global_library_name_hash != MENU_VALUE_NO_CORE);
#endif

   if (check_global_library_name_hash)
   {
      /* per-core saves: append the library_name to the save location */
      if (settings->sort_savefiles_enable
            && !string_is_empty(global->dir.savefile))
      {
         fill_pathname_join(
               current_savefile_dir,
               global->dir.savefile,
               info->info.library_name,
               sizeof(global->dir.savefile));

         /* If path doesn't exist, try to create it,
          * if everything fails revert to the original path. */
         if(!path_is_directory(current_savefile_dir)
               && !string_is_empty(current_savefile_dir))
         {
            path_mkdir(current_savefile_dir);
            if(!path_is_directory(current_savefile_dir))
            {
               RARCH_LOG("%s %s\n",
                     msg_hash_to_str(MSG_REVERTING_SAVEFILE_DIRECTORY_TO),
                     global->dir.savefile);

               strlcpy(current_savefile_dir,
                     global->dir.savefile,
                     sizeof(current_savefile_dir));
            }
         }
      }

      /* per-core states: append the library_name to the save location */
      if (settings->sort_savestates_enable
            && !string_is_empty(global->dir.savestate))
      {
         fill_pathname_join(
               current_savestate_dir,
               global->dir.savestate,
               info->info.library_name,
               sizeof(global->dir.savestate));

         /* If path doesn't exist, try to create it.
          * If everything fails, revert to the original path. */
         if(!path_is_directory(current_savestate_dir) &&
               !string_is_empty(current_savestate_dir))
         {
            path_mkdir(current_savestate_dir);
            if(!path_is_directory(current_savestate_dir))
            {
               RARCH_LOG("%s %s\n",
                     msg_hash_to_str(MSG_REVERTING_SAVESTATE_DIRECTORY_TO),
                     global->dir.savestate);
               strlcpy(current_savestate_dir,
                     global->dir.savestate,
                     sizeof(current_savestate_dir));
            }
         }
      }
   }

   /* Set savefile directory if empty based on content directory */
   if (string_is_empty(current_savefile_dir))
   {
      global_t *global = global_get_ptr();
      strlcpy(current_savefile_dir, global->name.base,
            sizeof(current_savefile_dir));
      path_basedir(current_savefile_dir);
   }

   if(path_is_directory(current_savefile_dir))
      strlcpy(global->name.savefile, current_savefile_dir,
            sizeof(global->name.savefile));

   if(path_is_directory(current_savestate_dir))
      strlcpy(global->name.savestate, current_savestate_dir,
            sizeof(global->name.savestate));

   if (path_is_directory(global->name.savefile))
   {
      fill_pathname_dir(global->name.savefile, global->name.base,
            file_path_str(FILE_PATH_SRM_EXTENSION),
            sizeof(global->name.savefile));
      RARCH_LOG("%s \"%s\".\n",
            msg_hash_to_str(MSG_REDIRECTING_SAVEFILE_TO),
            global->name.savefile);
   }

   if (path_is_directory(global->name.savestate))
   {
      fill_pathname_dir(global->name.savestate, global->name.base,
            file_path_str(FILE_PATH_STATE_EXTENSION),
            sizeof(global->name.savestate));
      RARCH_LOG("%s \"%s\".\n",
            msg_hash_to_str(MSG_REDIRECTING_SAVESTATE_TO),
            global->name.savestate);
   }

   if (path_is_directory(global->name.cheatfile))
   {
      fill_pathname_dir(global->name.cheatfile, global->name.base,
            file_path_str(FILE_PATH_STATE_EXTENSION),
            sizeof(global->name.cheatfile));
      RARCH_LOG("%s \"%s\".\n",
            msg_hash_to_str(MSG_REDIRECTING_CHEATFILE_TO),
            global->name.cheatfile);
   }
}

void path_set_basename(const char *path)
{
   char *dst          = NULL;
   global_t *global   = global_get_ptr();

   runloop_ctl(RUNLOOP_CTL_SET_CONTENT_PATH, (void*)path);
   strlcpy(global->name.base,     path, sizeof(global->name.base));

#ifdef HAVE_COMPRESSION
   /* Removing extension is a bit tricky for compressed files.
    * Basename means:
    * /file/to/path/game.extension should be:
    * /file/to/path/game
    *
    * Two things to consider here are: /file/to/path/ is expected
    * to be a directory and "game" is a single file. This is used for
    * states and srm default paths.
    *
    * For compressed files we have:
    *
    * /file/to/path/comp.7z#game.extension and
    * /file/to/path/comp.7z#folder/game.extension
    *
    * The choice I take here is:
    * /file/to/path/game as basename. We might end up in a writable
    * directory then and the name of srm and states are meaningful.
    *
    */
   path_basedir(global->name.base);
   fill_pathname_dir(global->name.base, path, "", sizeof(global->name.base));
#endif

   if ((dst = strrchr(global->name.base, '.')))
      *dst = '\0';
}

const char *path_get_current_savefile_dir(void)
{
   char *ret = current_savefile_dir;

   /* try to infer the path in case it's still empty by calling
   path_set_redirect */
   if (string_is_empty(ret) && !content_does_not_need_content())
      path_set_redirect();

   return ret;
}

void path_set_special(char **argv, unsigned num_content)
{
   unsigned i;
   union string_list_elem_attr attr;
   global_t   *global   = global_get_ptr();

   /* First content file is the significant one. */
   path_set_basename(argv[0]);

   global->subsystem_fullpaths = string_list_new();
   retro_assert(global->subsystem_fullpaths);

   attr.i = 0;

   for (i = 0; i < num_content; i++)
      string_list_append(global->subsystem_fullpaths, argv[i], attr);

   /* We defer SRAM path updates until we can resolve it.
    * It is more complicated for special content types. */

   if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_STATE_PATH))
      fill_pathname_noext(global->name.savestate, global->name.base,
            file_path_str(FILE_PATH_STATE_EXTENSION),
            sizeof(global->name.savestate));

   if (path_is_directory(global->name.savestate))
   {
      fill_pathname_dir(global->name.savestate, global->name.base,
            file_path_str(FILE_PATH_STATE_EXTENSION),
            sizeof(global->name.savestate));
      RARCH_LOG("%s \"%s\".\n",
            msg_hash_to_str(MSG_REDIRECTING_SAVESTATE_TO),
            global->name.savestate);
   }
}

void path_init_savefile(void)
{
   global_t            *global = global_get_ptr();
   rarch_system_info_t *system = NULL;

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system);

   command_event(CMD_EVENT_SAVEFILES_DEINIT, NULL);

   global->savefiles = string_list_new();
   retro_assert(global->savefiles);

   if (system && !string_is_empty(global->subsystem))
   {
      /* For subsystems, we know exactly which RAM types are supported. */

      unsigned i, j;
      const struct retro_subsystem_info *info =
         libretro_find_subsystem_info(
               system->subsystem.data,
               system->subsystem.size,
               global->subsystem);

      /* We'll handle this error gracefully later. */
      unsigned num_content = MIN(info ? info->num_roms : 0,
            global->subsystem_fullpaths ?
            global->subsystem_fullpaths->size : 0);

      bool use_sram_dir = path_is_directory(global->dir.savefile);

      for (i = 0; i < num_content; i++)
      {
         for (j = 0; j < info->roms[i].num_memory; j++)
         {
            union string_list_elem_attr attr;
            char path[PATH_MAX_LENGTH] = {0};
            char ext[32] = {0};
            const struct retro_subsystem_memory_info *mem =
               (const struct retro_subsystem_memory_info*)
               &info->roms[i].memory[j];

            snprintf(ext, sizeof(ext), ".%s", mem->extension);

            if (use_sram_dir)
            {
               /* Redirect content fullpath to save directory. */
               strlcpy(path, global->dir.savefile, sizeof(path));
               fill_pathname_dir(path,
                     global->subsystem_fullpaths->elems[i].data, ext,
                     sizeof(path));
            }
            else
            {
               fill_pathname(path, global->subsystem_fullpaths->elems[i].data,
                     ext, sizeof(path));
            }

            attr.i = mem->type;
            string_list_append(global->savefiles, path, attr);
         }
      }

      /* Let other relevant paths be inferred from the main SRAM location. */
      if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_SAVE_PATH))
         fill_pathname_noext(global->name.savefile,
               global->name.base,
               file_path_str(FILE_PATH_SRM_EXTENSION),
               sizeof(global->name.savefile));

      if (path_is_directory(global->name.savefile))
      {
         fill_pathname_dir(global->name.savefile,
               global->name.base,
               file_path_str(FILE_PATH_SRM_EXTENSION),
               sizeof(global->name.savefile));
         RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_SAVEFILE_TO),
               global->name.savefile);
      }
   }
   else
   {
      union string_list_elem_attr attr;
      char savefile_name_rtc[PATH_MAX_LENGTH] = {0};

      attr.i = RETRO_MEMORY_SAVE_RAM;
      string_list_append(global->savefiles, global->name.savefile, attr);

      /* Infer .rtc save path from save ram path. */
      attr.i = RETRO_MEMORY_RTC;
      fill_pathname(savefile_name_rtc,
            global->name.savefile,
            file_path_str(FILE_PATH_RTC_EXTENSION),
            sizeof(savefile_name_rtc));
      string_list_append(global->savefiles, savefile_name_rtc, attr);
   }
}

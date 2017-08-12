/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <retro_miscellaneous.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <lists/string_list.h>
#include <string/stdstring.h>
#include <retro_assert.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_NETWORKING
#include "network/netplay/netplay.h"
#endif

#include "dirs.h"
#include "paths.h"

#include "configuration.h"
#include "command.h"
#include "content.h"
#include "dynamic.h"
#include "movie.h"
#include "file_path_special.h"

#include "core.h"
#include "msg_hash.h"
#include "retroarch.h"
#include "verbosity.h"
#include "tasks/tasks_internal.h"

#define MENU_VALUE_NO_CORE 0x7d5472cbU

static struct string_list *subsystem_fullpaths          = NULL;

static char subsystem_path[PATH_MAX_LENGTH]             = {0};
static char path_default_shader_preset[PATH_MAX_LENGTH] = {0};
static char path_main_basename[8192]                    = {0};
static char path_content[PATH_MAX_LENGTH]               = {0};
static char path_libretro[PATH_MAX_LENGTH]              = {0};
static char path_config_file[PATH_MAX_LENGTH]           = {0};
static char path_config_append_file[PATH_MAX_LENGTH]    = {0};
static char path_core_options_file[PATH_MAX_LENGTH]     = {0};

void path_set_redirect(void)
{
   char new_savefile_dir[PATH_MAX_LENGTH];
   char new_savestate_dir[PATH_MAX_LENGTH];
   uint32_t library_name_hash                  = 0;
   bool check_library_name_hash                = false;
   global_t                *global             = global_get_ptr();
   const char *old_savefile_dir                = dir_get(RARCH_DIR_SAVEFILE);
   const char *old_savestate_dir               = dir_get(RARCH_DIR_SAVESTATE);
   rarch_system_info_t      *info              = runloop_get_system_info();
   settings_t *settings                        = config_get_ptr();

   new_savefile_dir[0] = new_savestate_dir[0]  = '\0';

   if (info && info->info.library_name &&
         !string_is_empty(info->info.library_name))
      library_name_hash =
         msg_hash_calculate(info->info.library_name);

   /* Initialize current save directories
    * with the values from the config. */
   strlcpy(new_savefile_dir,
         old_savefile_dir,
         sizeof(new_savefile_dir));

   strlcpy(new_savestate_dir,
         old_savestate_dir,
         sizeof(new_savestate_dir));

   check_library_name_hash = (library_name_hash != 0);
#ifdef HAVE_MENU
   check_library_name_hash = check_library_name_hash &&
      (library_name_hash != MENU_VALUE_NO_CORE);
#endif

   if (check_library_name_hash)
   {
      /* per-core saves: append the library_name to the save location */
      if (settings->bools.sort_savefiles_enable
            && !string_is_empty(old_savefile_dir))
      {
         fill_pathname_join(
               new_savefile_dir,
               old_savefile_dir,
               info->info.library_name,
               sizeof(new_savefile_dir));

         /* If path doesn't exist, try to create it,
          * if everything fails revert to the original path. */
         if(!path_is_directory(new_savefile_dir)
               && !string_is_empty(new_savefile_dir))
         {
            path_mkdir(new_savefile_dir);
            if(!path_is_directory(new_savefile_dir))
            {
               RARCH_LOG("%s %s\n",
                     msg_hash_to_str(MSG_REVERTING_SAVEFILE_DIRECTORY_TO),
                     old_savefile_dir);

               strlcpy(new_savefile_dir,
                     old_savefile_dir,
                     sizeof(new_savefile_dir));
            }
         }
      }

      /* per-core states: append the library_name to the save location */
      if (settings->bools.sort_savestates_enable
            && !string_is_empty(old_savestate_dir))
      {
         fill_pathname_join(
               new_savestate_dir,
               old_savestate_dir,
               info->info.library_name,
               sizeof(new_savestate_dir));

         /* If path doesn't exist, try to create it.
          * If everything fails, revert to the original path. */
         if(!path_is_directory(new_savestate_dir) &&
               !string_is_empty(new_savestate_dir))
         {
            path_mkdir(new_savestate_dir);
            if(!path_is_directory(new_savestate_dir))
            {
               RARCH_LOG("%s %s\n",
                     msg_hash_to_str(MSG_REVERTING_SAVESTATE_DIRECTORY_TO),
                     old_savestate_dir);
               strlcpy(new_savestate_dir,
                     old_savestate_dir,
                     sizeof(new_savestate_dir));
            }
         }
      }
   }

   /* Set savefile directory if empty based on content directory */
   if (string_is_empty(new_savefile_dir) || settings->bools.savefiles_in_content_dir)
   {
      strlcpy(new_savefile_dir, path_main_basename,
            sizeof(new_savefile_dir));
      path_basedir(new_savefile_dir);
   }

   /* Set savestate directory if empty based on content directory */
   if (string_is_empty(new_savestate_dir) || settings->bools.savestates_in_content_dir)
   {
      strlcpy(new_savestate_dir, path_main_basename,
            sizeof(new_savestate_dir));
      path_basedir(new_savestate_dir);
   }

   if (global)
   {
      if(path_is_directory(new_savefile_dir))
         strlcpy(global->name.savefile, new_savefile_dir,
               sizeof(global->name.savefile));

      if(path_is_directory(new_savestate_dir))
         strlcpy(global->name.savestate, new_savestate_dir,
               sizeof(global->name.savestate));

      if (path_is_directory(global->name.savefile))
      {
         fill_pathname_dir(global->name.savefile, 
               !string_is_empty(path_main_basename) ? path_main_basename : 
                  info->info.library_name,
               file_path_str(FILE_PATH_SRM_EXTENSION),
               sizeof(global->name.savefile));
         RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_SAVEFILE_TO),
               global->name.savefile);
      }

      if (path_is_directory(global->name.savestate))
      {
         fill_pathname_dir(global->name.savestate, 
               !string_is_empty(path_main_basename) ? path_main_basename : 
                  info->info.library_name,
               file_path_str(FILE_PATH_STATE_EXTENSION),
               sizeof(global->name.savestate));
         RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_SAVESTATE_TO),
               global->name.savestate);
      }

      if (path_is_directory(global->name.cheatfile))
      {
         fill_pathname_dir(global->name.cheatfile, path_main_basename,
               file_path_str(FILE_PATH_STATE_EXTENSION),
               sizeof(global->name.cheatfile));
         RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_CHEATFILE_TO),
               global->name.cheatfile);
      }
   }

   dir_set(RARCH_DIR_CURRENT_SAVEFILE,  new_savefile_dir);
   dir_set(RARCH_DIR_CURRENT_SAVESTATE, new_savestate_dir);
}

void path_set_basename(const char *path)
{
   char *dst          = NULL;

   path_set(RARCH_PATH_CONTENT,  path);
   path_set(RARCH_PATH_BASENAME, path);

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
   path_basedir_wrapper(path_main_basename);
   fill_pathname_dir(path_main_basename, path, "", sizeof(path_main_basename));
#endif

   if ((dst = strrchr(path_main_basename, '.')))
      *dst = '\0';
}

struct string_list *path_get_subsystem_list(void)
{
   return subsystem_fullpaths;
}

void path_set_special(char **argv, unsigned num_content)
{
   unsigned i;
   union string_list_elem_attr attr;
   global_t   *global   = global_get_ptr();

   /* First content file is the significant one. */
   path_set_basename(argv[0]);

   subsystem_fullpaths = string_list_new();
   retro_assert(subsystem_fullpaths);

   attr.i = 0;

   for (i = 0; i < num_content; i++)
      string_list_append(subsystem_fullpaths, argv[i], attr);

   /* We defer SRAM path updates until we can resolve it.
    * It is more complicated for special content types. */
   if (global)
   {
      if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_STATE_PATH, NULL))
         fill_pathname_noext(global->name.savestate, path_main_basename,
               file_path_str(FILE_PATH_STATE_EXTENSION),
               sizeof(global->name.savestate));

      if (path_is_directory(global->name.savestate))
      {
         fill_pathname_dir(global->name.savestate, path_main_basename,
               file_path_str(FILE_PATH_STATE_EXTENSION),
               sizeof(global->name.savestate));
         RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_SAVESTATE_TO),
               global->name.savestate);
      }
   }
}

static bool path_init_subsystem(void)
{
   unsigned i, j;
   const struct retro_subsystem_info *info = NULL;
   global_t                        *global = global_get_ptr();
   rarch_system_info_t             *system = runloop_get_system_info();

   if (!system || path_is_empty(RARCH_PATH_SUBSYSTEM))
      return false;

   /* For subsystems, we know exactly which RAM types are supported. */

   info = libretro_find_subsystem_info(
         system->subsystem.data,
         system->subsystem.size,
         path_get(RARCH_PATH_SUBSYSTEM));

   /* We'll handle this error gracefully later. */

   if (info)
   {
      unsigned num_content = MIN(info->num_roms,
            path_is_empty(RARCH_PATH_SUBSYSTEM) ?
            0 : (unsigned)subsystem_fullpaths->size);

      for (i = 0; i < num_content; i++)
      {
         for (j = 0; j < info->roms[i].num_memory; j++)
         {
            union string_list_elem_attr attr;
            char path[PATH_MAX_LENGTH];
            char ext[32];
            const struct retro_subsystem_memory_info *mem =
               (const struct retro_subsystem_memory_info*)
               &info->roms[i].memory[j];

            path[0] = ext[0] = '\0';

            snprintf(ext, sizeof(ext), ".%s", mem->extension);

            if (path_is_directory(dir_get(RARCH_DIR_SAVEFILE)))
            {
               /* Use SRAM dir */
               /* Redirect content fullpath to save directory. */
               strlcpy(path, dir_get(RARCH_DIR_SAVEFILE), sizeof(path));
               fill_pathname_dir(path,
                     subsystem_fullpaths->elems[i].data, ext,
                     sizeof(path));
            }
            else
            {
               fill_pathname(path, subsystem_fullpaths->elems[i].data,
                     ext, sizeof(path));
            }

            attr.i = mem->type;
            string_list_append((struct string_list*)savefile_ptr_get(), path, attr);
         }
      }
   }

   if (global)
   {
      /* Let other relevant paths be inferred from the main SRAM location. */
      if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL))
         fill_pathname_noext(global->name.savefile,
               path_main_basename,
               file_path_str(FILE_PATH_SRM_EXTENSION),
               sizeof(global->name.savefile));

      if (path_is_directory(global->name.savefile))
      {
         fill_pathname_dir(global->name.savefile,
               path_main_basename,
               file_path_str(FILE_PATH_SRM_EXTENSION),
               sizeof(global->name.savefile));
         RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_SAVEFILE_TO),
               global->name.savefile);
      }
   }

   return true;
}

void path_init_savefile(void)
{
   bool should_sram_be_used = rarch_ctl(RARCH_CTL_IS_SRAM_USED, NULL) 
      && !rarch_ctl(RARCH_CTL_IS_SRAM_SAVE_DISABLED, NULL);

   if (should_sram_be_used)
      rarch_ctl(RARCH_CTL_SET_SRAM_ENABLE_FORCE, NULL);
   else
      rarch_ctl(RARCH_CTL_UNSET_SRAM_ENABLE, NULL);

   if (!rarch_ctl(RARCH_CTL_IS_SRAM_USED, NULL))
   {
      RARCH_LOG("%s\n",
            msg_hash_to_str(MSG_SRAM_WILL_NOT_BE_SAVED));
      return;
   }

   command_event(CMD_EVENT_AUTOSAVE_INIT, NULL);
}

static void path_init_savefile_internal(void)
{
   path_deinit_savefile();

   path_init_savefile_new();

   if (!path_init_subsystem())
   {
      global_t *global = global_get_ptr();
      path_init_savefile_rtc(global->name.savefile);
   }
}


void path_fill_names(void)
{
   global_t *global = global_get_ptr();

   path_init_savefile_internal();
   
   if (global)
      bsv_movie_set_path(global->name.savefile);

   if (string_is_empty(path_main_basename))
      return;

   if (global)
   {
      if (string_is_empty(global->name.ups))
         fill_pathname_noext(global->name.ups, path_main_basename,
               file_path_str(FILE_PATH_UPS_EXTENSION),
               sizeof(global->name.ups));

      if (string_is_empty(global->name.bps))
         fill_pathname_noext(global->name.bps, path_main_basename,
               file_path_str(FILE_PATH_BPS_EXTENSION),
               sizeof(global->name.bps));

      if (string_is_empty(global->name.ips))
         fill_pathname_noext(global->name.ips, path_main_basename,
               file_path_str(FILE_PATH_IPS_EXTENSION),
               sizeof(global->name.ips));
   }
}

char *path_get_ptr(enum rarch_path_type type)
{
   switch (type)
   {
      case RARCH_PATH_CONTENT:
         return path_content;
      case RARCH_PATH_DEFAULT_SHADER_PRESET:
         return path_default_shader_preset;
      case RARCH_PATH_BASENAME:
         return path_main_basename;
      case RARCH_PATH_CORE_OPTIONS:
         if (!path_is_empty(RARCH_PATH_CORE_OPTIONS))
            return path_core_options_file;
         break;
      case RARCH_PATH_SUBSYSTEM:
         return subsystem_path;
      case RARCH_PATH_CONFIG:
         if (!path_is_empty(RARCH_PATH_CONFIG))
            return path_config_file;
         break;
      case RARCH_PATH_CONFIG_APPEND:
         if (!path_is_empty(RARCH_PATH_CONFIG_APPEND))
            return path_config_append_file;
         break;
      case RARCH_PATH_CORE:
         return path_libretro;
      case RARCH_PATH_NONE:
      case RARCH_PATH_NAMES:
         break;
   }

   return NULL;
}

const char *path_get(enum rarch_path_type type)
{
   switch (type)
   {
      case RARCH_PATH_CONTENT:
         return path_content;
      case RARCH_PATH_DEFAULT_SHADER_PRESET:
         return path_default_shader_preset;
      case RARCH_PATH_BASENAME:
         return path_main_basename;
      case RARCH_PATH_CORE_OPTIONS:
         if (!path_is_empty(RARCH_PATH_CORE_OPTIONS))
            return path_core_options_file;
         break;
      case RARCH_PATH_SUBSYSTEM:
         return subsystem_path;
      case RARCH_PATH_CONFIG:
         if (!path_is_empty(RARCH_PATH_CONFIG))
            return path_config_file;
         break;
      case RARCH_PATH_CONFIG_APPEND:
         if (!path_is_empty(RARCH_PATH_CONFIG_APPEND))
            return path_config_append_file;
         break;
      case RARCH_PATH_CORE:
         return path_libretro;
      case RARCH_PATH_NONE:
      case RARCH_PATH_NAMES:
         break;
   }

   return NULL;
}

size_t path_get_realsize(enum rarch_path_type type)
{
   switch (type)
   {
      case RARCH_PATH_CONTENT:
         return sizeof(path_content);
      case RARCH_PATH_DEFAULT_SHADER_PRESET:
         return sizeof(path_default_shader_preset);
      case RARCH_PATH_BASENAME:
         return sizeof(path_main_basename);
      case RARCH_PATH_CORE_OPTIONS:
         return sizeof(path_core_options_file);
      case RARCH_PATH_SUBSYSTEM:
         return sizeof(subsystem_path);
      case RARCH_PATH_CONFIG:
         return sizeof(path_config_file);
      case RARCH_PATH_CONFIG_APPEND:
         return sizeof(path_config_append_file);
      case RARCH_PATH_CORE:
         return sizeof(path_libretro);
      case RARCH_PATH_NONE:
      case RARCH_PATH_NAMES:
         break;
   }

   return 0;
}

static void path_set_names(const char *path)
{
   global_t *global = global_get_ptr();

   path_set_basename(path);

   if (global)
   {
      if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL))
         fill_pathname_noext(global->name.savefile, path_main_basename,
               file_path_str(FILE_PATH_SRM_EXTENSION), sizeof(global->name.savefile));

      if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_STATE_PATH, NULL))
         fill_pathname_noext(global->name.savestate, path_main_basename,
               file_path_str(FILE_PATH_STATE_EXTENSION), sizeof(global->name.savestate));

      fill_pathname_noext(global->name.cheatfile, path_main_basename,
            file_path_str(FILE_PATH_CHT_EXTENSION), sizeof(global->name.cheatfile));
   }

   path_set_redirect();
}

bool path_set(enum rarch_path_type type, const char *path)
{
   if (!path)
      return false;

   switch (type)
   {
      case RARCH_PATH_BASENAME:
         strlcpy(path_main_basename, path,
               sizeof(path_main_basename));
         break;
      case RARCH_PATH_NAMES:
         path_set_names(path);
         break;
      case RARCH_PATH_CORE:
         strlcpy(path_libretro, path,
               sizeof(path_libretro));
         break;
      case RARCH_PATH_DEFAULT_SHADER_PRESET:
         strlcpy(path_default_shader_preset, path,
               sizeof(path_default_shader_preset));
         break;
      case RARCH_PATH_CONFIG_APPEND:
         strlcpy(path_config_append_file, path,
               sizeof(path_config_append_file));
         break;
      case RARCH_PATH_CONFIG:
         strlcpy(path_config_file, path,
               sizeof(path_config_file));
         break;
      case RARCH_PATH_SUBSYSTEM:
         strlcpy(subsystem_path, path,
               sizeof(subsystem_path));
         break;
      case RARCH_PATH_CORE_OPTIONS:
         strlcpy(path_core_options_file, path,
               sizeof(path_core_options_file));
         break;
      case RARCH_PATH_CONTENT:
         strlcpy(path_content, path,
               sizeof(path_content));
         break;
      case RARCH_PATH_NONE:
         break;
   }

   return true;
}

bool path_is_empty(enum rarch_path_type type)
{
   switch (type)
   {
      case RARCH_PATH_DEFAULT_SHADER_PRESET:
         if (string_is_empty(path_default_shader_preset))
            return true;
         break;
      case RARCH_PATH_SUBSYSTEM:
         if (string_is_empty(subsystem_path))
            return true;
         break;
      case RARCH_PATH_CONFIG:
         if (string_is_empty(path_config_file))
            return true;
         break;
      case RARCH_PATH_CORE_OPTIONS:
         if (string_is_empty(path_core_options_file))
            return true;
         break;
      case RARCH_PATH_CONFIG_APPEND:
         if (string_is_empty(path_config_append_file))
            return true;
         break;
      case RARCH_PATH_CONTENT:
         if (string_is_empty(path_content))
            return true;
         break;
      case RARCH_PATH_CORE:
         if (string_is_empty(path_libretro))
            return true;
         break;
      case RARCH_PATH_BASENAME:
         if (string_is_empty(path_main_basename))
            return true;
         break;
      case RARCH_PATH_NONE:
      case RARCH_PATH_NAMES:
         break;
   }

   return false;
}

void path_clear(enum rarch_path_type type)
{
   switch (type)
   {
      case RARCH_PATH_SUBSYSTEM:
         *subsystem_path = '\0';
         break;
      case RARCH_PATH_CORE:
         *path_libretro = '\0';
         break;
      case RARCH_PATH_CONFIG:
         *path_config_file = '\0';
         break;
      case RARCH_PATH_CONTENT:
         *path_content = '\0';
         break;
      case RARCH_PATH_BASENAME:
         *path_main_basename = '\0';
         break;
      case RARCH_PATH_CORE_OPTIONS:
         *path_core_options_file = '\0';
         break;
      case RARCH_PATH_DEFAULT_SHADER_PRESET:
         *path_default_shader_preset = '\0';
         break;
      case RARCH_PATH_CONFIG_APPEND:
         *path_config_append_file = '\0';
         break;
      case RARCH_PATH_NONE:
      case RARCH_PATH_NAMES:
         break;
   }
}

void path_clear_all(void)
{
   path_clear(RARCH_PATH_CONTENT);
   path_clear(RARCH_PATH_CONFIG);
   path_clear(RARCH_PATH_CONFIG_APPEND);
   path_clear(RARCH_PATH_CORE_OPTIONS);
   path_clear(RARCH_PATH_BASENAME);
}

enum rarch_content_type path_is_media_type(const char *path)
{
   char ext_lower[128];

   ext_lower[0] = '\0';

   strlcpy(ext_lower, path_get_extension(path), sizeof(ext_lower));

   string_to_lower(ext_lower);

   switch (msg_hash_to_file_type(msg_hash_calculate(ext_lower)))
   {
#ifdef HAVE_FFMPEG
      case FILE_TYPE_OGM:
      case FILE_TYPE_MKV:
      case FILE_TYPE_AVI:
      case FILE_TYPE_MP4:
      case FILE_TYPE_FLV:
      case FILE_TYPE_WEBM:
      case FILE_TYPE_3GP:
      case FILE_TYPE_3G2:
      case FILE_TYPE_F4F:
      case FILE_TYPE_F4V:
      case FILE_TYPE_MOV:
      case FILE_TYPE_WMV:
      case FILE_TYPE_MPG:
      case FILE_TYPE_MPEG:
      case FILE_TYPE_VOB:
      case FILE_TYPE_ASF:
      case FILE_TYPE_DIVX:
      case FILE_TYPE_M2P:
      case FILE_TYPE_M2TS:
      case FILE_TYPE_PS:
      case FILE_TYPE_TS:
      case FILE_TYPE_MXF:
         return RARCH_CONTENT_MOVIE;
      case FILE_TYPE_WMA:
      case FILE_TYPE_OGG:
      case FILE_TYPE_MP3:
      case FILE_TYPE_M4A:
      case FILE_TYPE_FLAC:
      case FILE_TYPE_WAV:
         return RARCH_CONTENT_MUSIC;
#endif
#ifdef HAVE_IMAGEVIEWER
      case FILE_TYPE_JPEG:
      case FILE_TYPE_PNG:
      case FILE_TYPE_TGA:
      case FILE_TYPE_BMP:
         return RARCH_CONTENT_IMAGE;
#endif
#ifdef HAVE_IBXM
      case FILE_TYPE_MOD:
      case FILE_TYPE_S3M:
      case FILE_TYPE_XM:
         return RARCH_CONTENT_MUSIC;
#endif

      case FILE_TYPE_NONE:
      default:
         break;
   }

   return RARCH_CONTENT_NONE;
}

void path_deinit_subsystem(void)
{
   if (subsystem_fullpaths)
      string_list_free(subsystem_fullpaths);
   subsystem_fullpaths = NULL;
}

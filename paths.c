/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2019 - Daniel De Matteis
 *  Copyright (C) 2017-2019 - Andrés Suárez
 *  Copyright (C) 2016-2019 - Brad Parker
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
#include <lists/dir_list.h>
#include <lists/string_list.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <retro_assert.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_NETWORKING
#include "network/netplay/netplay.h"
#endif

#include "paths.h"

#include "configuration.h"
#include "command.h"
#include "content.h"
#include "dynamic.h"
#include "defaults.h"
#include "file_path_special.h"
#include "list_special.h"

#include "core.h"
#include "msg_hash.h"
#include "retroarch.h"
#include "verbosity.h"
#include "tasks/tasks_internal.h"

#define MENU_VALUE_NO_CORE 0x7d5472cbU

struct rarch_dir_list
{
   struct string_list *list;
   size_t ptr;
};

static struct string_list *subsystem_fullpaths          = NULL;

static char subsystem_path[PATH_MAX_LENGTH]             = {0};
static char path_default_shader_preset[PATH_MAX_LENGTH] = {0};
static char path_main_basename[8192]                    = {0};
static char path_content[PATH_MAX_LENGTH]               = {0};
static char path_libretro[PATH_MAX_LENGTH]              = {0};
static char path_config_file[PATH_MAX_LENGTH]           = {0};
static char path_config_append_file[PATH_MAX_LENGTH]    = {0};
static char path_core_options_file[PATH_MAX_LENGTH]     = {0};

static struct rarch_dir_list dir_shader_list;

static char dir_system[PATH_MAX_LENGTH]                 = {0};
static char dir_savefile[PATH_MAX_LENGTH]               = {0};
static char current_savefile_dir[PATH_MAX_LENGTH]       = {0};
static char current_savestate_dir[PATH_MAX_LENGTH]      = {0};
static char dir_savestate[PATH_MAX_LENGTH]              = {0};


void path_set_redirect(void)
{
   size_t path_size                            = PATH_MAX_LENGTH * sizeof(char);
   char *new_savefile_dir                      = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   char *new_savestate_dir                     = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   global_t                *global             = global_get_ptr();
   const char *old_savefile_dir                = dir_get(RARCH_DIR_SAVEFILE);
   const char *old_savestate_dir               = dir_get(RARCH_DIR_SAVESTATE);
   struct retro_system_info *system            = runloop_get_libretro_system_info();
   settings_t *settings                        = config_get_ptr();

   new_savefile_dir[0] = new_savestate_dir[0]  = '\0';

   /* Initialize current save directories
    * with the values from the config. */
   strlcpy(new_savefile_dir,  old_savefile_dir,  path_size);
   strlcpy(new_savestate_dir, old_savestate_dir, path_size);

   if (system && !string_is_empty(system->library_name))
   {
#ifdef HAVE_MENU
      if (!string_is_equal(system->library_name,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE)))
#endif
      {
         /* per-core saves: append the library_name to the save location */
         if (settings->bools.sort_savefiles_enable
               && !string_is_empty(old_savefile_dir))
         {
            fill_pathname_join(
                  new_savefile_dir,
                  old_savefile_dir,
                  system->library_name,
                  path_size);

            /* If path doesn't exist, try to create it,
             * if everything fails revert to the original path. */
            if (!path_is_directory(new_savefile_dir))
               if (!path_mkdir(new_savefile_dir))
               {
                  RARCH_LOG("%s %s\n",
                        msg_hash_to_str(MSG_REVERTING_SAVEFILE_DIRECTORY_TO),
                        old_savefile_dir);

                  strlcpy(new_savefile_dir, old_savefile_dir, path_size);
               }
         }

         /* per-core states: append the library_name to the save location */
         if (settings->bools.sort_savestates_enable
               && !string_is_empty(old_savestate_dir))
         {
            fill_pathname_join(
                  new_savestate_dir,
                  old_savestate_dir,
                  system->library_name,
                  path_size);

            /* If path doesn't exist, try to create it.
             * If everything fails, revert to the original path. */
            if (!path_is_directory(new_savestate_dir))
               if (!path_mkdir(new_savestate_dir))
               {
                  RARCH_LOG("%s %s\n",
                        msg_hash_to_str(MSG_REVERTING_SAVESTATE_DIRECTORY_TO),
                        old_savestate_dir);
                  strlcpy(new_savestate_dir,
                        old_savestate_dir,
                        path_size);
               }
         }
      }
   }

   /* Set savefile directory if empty to content directory */
   if (string_is_empty(new_savefile_dir) || settings->bools.savefiles_in_content_dir)
   {
      strlcpy(new_savefile_dir, path_main_basename,
            path_size);
      path_basedir(new_savefile_dir);
   }

   /* Set savestate directory if empty based on content directory */
   if (string_is_empty(new_savestate_dir) || settings->bools.savestates_in_content_dir)
   {
      strlcpy(new_savestate_dir, path_main_basename,
            path_size);
      path_basedir(new_savestate_dir);
   }

   if (global)
   {
      if (path_is_directory(new_savefile_dir))
         strlcpy(global->name.savefile, new_savefile_dir,
               sizeof(global->name.savefile));

      if (path_is_directory(new_savestate_dir))
         strlcpy(global->name.savestate, new_savestate_dir,
               sizeof(global->name.savestate));

      if (path_is_directory(global->name.savefile))
      {
         fill_pathname_dir(global->name.savefile,
               !string_is_empty(path_main_basename) ? path_main_basename :
                  system && !string_is_empty(system->library_name) ? system->library_name : "",
               ".srm",
               sizeof(global->name.savefile));
         RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_SAVEFILE_TO),
               global->name.savefile);
      }

      if (path_is_directory(global->name.savestate))
      {
         fill_pathname_dir(global->name.savestate,
               !string_is_empty(path_main_basename) ? path_main_basename :
                  system && !string_is_empty(system->library_name) ? system->library_name : "",
               ".state",
               sizeof(global->name.savestate));
         RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_SAVESTATE_TO),
               global->name.savestate);
      }

      if (path_is_directory(global->name.cheatfile))
      {
         /* FIXME: Should this optionally use system->library_name like the others? */
         fill_pathname_dir(global->name.cheatfile,
               !string_is_empty(path_main_basename) ? path_main_basename : "",
               ".state",
               sizeof(global->name.cheatfile));
         RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_CHEATFILE_TO),
               global->name.cheatfile);
      }
   }

   dir_set(RARCH_DIR_CURRENT_SAVEFILE,  new_savefile_dir);
   dir_set(RARCH_DIR_CURRENT_SAVESTATE, new_savestate_dir);
   free(new_savefile_dir);
   free(new_savestate_dir);
}

static void path_set_basename(const char *path)
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
   struct string_list *subsystem_paths = NULL;
   char str[PATH_MAX_LENGTH];
   global_t *global = global_get_ptr();

   /* First content file is the significant one. */
   path_set_basename(argv[0]);

   subsystem_fullpaths = string_list_new();
   subsystem_paths = string_list_new();
   retro_assert(subsystem_fullpaths);

   attr.i = 0;

   for (i = 0; i < num_content; i++)
   {
      string_list_append(subsystem_fullpaths, argv[i], attr);
      strlcpy(str, argv[i], sizeof(str));
      path_remove_extension(str);
      string_list_append(subsystem_paths, path_basename(str), attr);
   }
   str[0] = '\0';
   string_list_join_concat(str, sizeof(str), subsystem_paths, " + ");

   /* We defer SRAM path updates until we can resolve it.
    * It is more complicated for special content types. */
   if (global)
   {
      if (path_is_directory(dir_get(RARCH_DIR_CURRENT_SAVESTATE)))
         strlcpy(global->name.savestate, dir_get(RARCH_DIR_CURRENT_SAVESTATE),
               sizeof(global->name.savestate));
      if (path_is_directory(global->name.savestate))
      {
         fill_pathname_dir(global->name.savestate,
               str,
               ".state",
               sizeof(global->name.savestate));
         RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_SAVESTATE_TO),
               global->name.savestate);
      }
   }

   if (subsystem_paths)
      string_list_free(subsystem_paths);
}

static bool path_init_subsystem(void)
{
   unsigned i, j;
   const struct retro_subsystem_info *info = NULL;
   global_t                        *global = global_get_ptr();
   rarch_system_info_t             *system = runloop_get_system_info();
   bool subsystem_path_empty               = path_is_empty(RARCH_PATH_SUBSYSTEM);

   if (!system || subsystem_path_empty)
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
            subsystem_path_empty ?
            0 : (unsigned)subsystem_fullpaths->size);

      for (i = 0; i < num_content; i++)
      {
         for (j = 0; j < info->roms[i].num_memory; j++)
         {
            union string_list_elem_attr attr;
            char ext[32];
            char savename[PATH_MAX_LENGTH];
            size_t path_size = PATH_MAX_LENGTH * sizeof(char);
            char *path       = (char*)malloc(
                  PATH_MAX_LENGTH * sizeof(char));
            const struct retro_subsystem_memory_info *mem =
               (const struct retro_subsystem_memory_info*)
               &info->roms[i].memory[j];

            path[0] = ext[0] = '\0';

            snprintf(ext, sizeof(ext), ".%s", mem->extension);
            strlcpy(savename, subsystem_fullpaths->elems[i].data, sizeof(savename));
            path_remove_extension(savename);

            if (path_is_directory(dir_get(RARCH_DIR_CURRENT_SAVEFILE)))
            {
               /* Use SRAM dir */
               /* Redirect content fullpath to save directory. */
               strlcpy(path, dir_get(RARCH_DIR_CURRENT_SAVEFILE), path_size);
               fill_pathname_dir(path,
                     savename, ext,
                     path_size);
            }
            else
               fill_pathname(path, savename, ext, path_size);

            RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_SAVEFILE_TO),
               path);

            attr.i = mem->type;
            string_list_append((struct string_list*)savefile_ptr_get(), path, attr);
            free(path);
         }
      }
   }

   if (global)
   {
      /* Let other relevant paths be inferred from the main SRAM location. */
      if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL))
         fill_pathname_noext(global->name.savefile,
               path_main_basename,
               ".srm",
               sizeof(global->name.savefile));

      if (path_is_directory(global->name.savefile))
      {
         fill_pathname_dir(global->name.savefile,
               path_main_basename,
               ".srm",
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
               ".ups",
               sizeof(global->name.ups));

      if (string_is_empty(global->name.bps))
         fill_pathname_noext(global->name.bps, path_main_basename,
               ".bps",
               sizeof(global->name.bps));

      if (string_is_empty(global->name.ips))
         fill_pathname_noext(global->name.ips, path_main_basename,
               ".ips",
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
               ".srm", sizeof(global->name.savefile));

      if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_STATE_PATH, NULL))
         fill_pathname_noext(global->name.savestate, path_main_basename,
               ".state", sizeof(global->name.savestate));

      fill_pathname_noext(global->name.cheatfile, path_main_basename,
            ".cht", sizeof(global->name.cheatfile));
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

   /* hack, to detect livestreams so the ffmpeg core can be started */
   if (
      strstr(path, "udp://")  ||
      strstr(path, "http://") ||
      strstr(path, "https://")||
      strstr(path, "tcp://")  ||
      strstr(path, "rtmp://") ||
      strstr(path, "rtp://")
   )
      return RARCH_CONTENT_MOVIE;

   switch (msg_hash_to_file_type(msg_hash_calculate(ext_lower)))
   {
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
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
#ifdef HAVE_EASTEREGG
      case FILE_TYPE_GONG:
         return RARCH_CONTENT_GONG;
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

bool dir_init_shader(void)
{
   unsigned i;
   struct rarch_dir_list *dir_list = (struct rarch_dir_list*)&dir_shader_list;
   settings_t           *settings  = config_get_ptr();

   if (!settings || !*settings->paths.directory_video_shader)
      return false;

   dir_list->list = dir_list_new_special(
         settings->paths.directory_video_shader, DIR_LIST_SHADERS, NULL);

   if (!dir_list->list || dir_list->list->size == 0)
   {
      dir_free_shader();
      return false;
   }

   dir_list->ptr  = 0;
   dir_list_sort(dir_list->list, false);

   for (i = 0; i < dir_list->list->size; i++)
      RARCH_LOG("%s \"%s\"\n",
            msg_hash_to_str(MSG_FOUND_SHADER),
            dir_list->list->elems[i].data);
   return true;
}

/* free functions */

bool dir_free_shader(void)
{
   struct rarch_dir_list *dir_list =
      (struct rarch_dir_list*)&dir_shader_list;

   dir_list_free(dir_list->list);
   dir_list->list = NULL;
   dir_list->ptr  = 0;

   return true;
}

/* check functions */

/**
 * dir_check_shader:
 * @pressed_next         : was next shader key pressed?
 * @pressed_previous     : was previous shader key pressed?
 *
 * Checks if any one of the shader keys has been pressed for this frame:
 * a) Next shader index.
 * b) Previous shader index.
 *
 * Will also immediately apply the shader.
 **/
void dir_check_shader(bool pressed_next, bool pressed_prev)
{
   struct rarch_dir_list *dir_list = (struct rarch_dir_list*)&dir_shader_list;
   static bool change_triggered = false;

   if (!dir_list || !dir_list->list)
      return;

   if (pressed_next)
   {
      if (change_triggered)
         dir_list->ptr = (dir_list->ptr + 1) %
            dir_list->list->size;
   }
   else if (pressed_prev)
   {
      if (dir_list->ptr == 0)
         dir_list->ptr = dir_list->list->size - 1;
      else
         dir_list->ptr--;
   }
   else
      return;
   change_triggered = true;

   command_set_shader(dir_list->list->elems[dir_list->ptr].data);
}

/* empty functions */

bool dir_is_empty(enum rarch_dir_type type)
{
   switch (type)
   {
      case RARCH_DIR_SYSTEM:
         return string_is_empty(dir_system);
      case RARCH_DIR_SAVEFILE:
         return string_is_empty(dir_savefile);
      case RARCH_DIR_CURRENT_SAVEFILE:
         return string_is_empty(current_savefile_dir);
      case RARCH_DIR_SAVESTATE:
         return string_is_empty(dir_savestate);
      case RARCH_DIR_CURRENT_SAVESTATE:
         return string_is_empty(current_savestate_dir);
      case RARCH_DIR_NONE:
         break;
   }

   return false;
}

/* get size functions */

size_t dir_get_size(enum rarch_dir_type type)
{
   switch (type)
   {
      case RARCH_DIR_SYSTEM:
         return sizeof(dir_system);
      case RARCH_DIR_SAVESTATE:
         return sizeof(dir_savestate);
      case RARCH_DIR_CURRENT_SAVESTATE:
         return sizeof(current_savestate_dir);
      case RARCH_DIR_SAVEFILE:
         return sizeof(dir_savefile);
      case RARCH_DIR_CURRENT_SAVEFILE:
         return sizeof(current_savefile_dir);
      case RARCH_DIR_NONE:
         break;
   }

   return 0;
}

/* clear functions */

void dir_clear(enum rarch_dir_type type)
{
   switch (type)
   {
      case RARCH_DIR_SAVEFILE:
         *dir_savefile = '\0';
         break;
      case RARCH_DIR_CURRENT_SAVEFILE:
         *current_savefile_dir = '\0';
         break;
      case RARCH_DIR_SAVESTATE:
         *dir_savestate = '\0';
         break;
      case RARCH_DIR_CURRENT_SAVESTATE:
         *current_savestate_dir = '\0';
         break;
      case RARCH_DIR_SYSTEM:
         *dir_system = '\0';
         break;
      case RARCH_DIR_NONE:
         break;
   }
}

void dir_clear_all(void)
{
   dir_clear(RARCH_DIR_SYSTEM);
   dir_clear(RARCH_DIR_SAVEFILE);
   dir_clear(RARCH_DIR_SAVESTATE);
}

/* get ptr functions */

char *dir_get_ptr(enum rarch_dir_type type)
{
   switch (type)
   {
      case RARCH_DIR_SAVEFILE:
         return dir_savefile;
      case RARCH_DIR_CURRENT_SAVEFILE:
         return current_savefile_dir;
      case RARCH_DIR_SAVESTATE:
         return dir_savestate;
      case RARCH_DIR_CURRENT_SAVESTATE:
         return current_savestate_dir;
      case RARCH_DIR_SYSTEM:
         return dir_system;
      case RARCH_DIR_NONE:
         break;
   }

   return NULL;
}

const char *dir_get(enum rarch_dir_type type)
{
   switch (type)
   {
      case RARCH_DIR_SAVEFILE:
         return dir_savefile;
      case RARCH_DIR_CURRENT_SAVEFILE:
         return current_savefile_dir;
      case RARCH_DIR_SAVESTATE:
         return dir_savestate;
      case RARCH_DIR_CURRENT_SAVESTATE:
         return current_savestate_dir;
      case RARCH_DIR_SYSTEM:
         return dir_system;
      case RARCH_DIR_NONE:
         break;
   }

   return NULL;
}

void dir_set(enum rarch_dir_type type, const char *path)
{
   switch (type)
   {
      case RARCH_DIR_CURRENT_SAVEFILE:
         strlcpy(current_savefile_dir, path,
               sizeof(current_savefile_dir));
         break;
      case RARCH_DIR_SAVEFILE:
         strlcpy(dir_savefile, path,
               sizeof(dir_savefile));
         break;
      case RARCH_DIR_CURRENT_SAVESTATE:
         strlcpy(current_savestate_dir, path,
               sizeof(current_savestate_dir));
         break;
      case RARCH_DIR_SAVESTATE:
         strlcpy(dir_savestate, path,
               sizeof(dir_savestate));
         break;
      case RARCH_DIR_SYSTEM:
         strlcpy(dir_system, path,
               sizeof(dir_system));
         break;
      case RARCH_DIR_NONE:
         break;
   }
}

void dir_check_defaults(void)
{
   unsigned i;
   /* early return for people with a custom folder setup
      so it doesn't create unnecessary directories
    */
#if defined(ORBIS) || defined(ANDROID)
   if (path_is_valid("host0:app/custom.ini"))
#else
   if (path_is_valid("custom.ini"))
#endif
      return;

   for (i = 0; i < DEFAULT_DIR_LAST; i++)
   {
      char       *new_path = NULL;
      const char *dir_path = g_defaults.dirs[i];

      if (string_is_empty(dir_path))
         continue;

      new_path = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

      if (!new_path)
         continue;

      new_path[0] = '\0';
      fill_pathname_expand_special(new_path,
            dir_path,
            PATH_MAX_LENGTH * sizeof(char));

      if (!path_is_directory(new_path))
         path_mkdir(new_path);

      free(new_path);
   }
}

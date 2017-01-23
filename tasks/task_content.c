/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Brad Parker
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

/* TODO/FIXME - turn this into actual task */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <file/file_path.h>
#include <string/stdstring.h>

#ifdef _WIN32
#ifdef _XBOX
#include <xtl.h>
#define setmode _setmode
#define INVALID_FILE_ATTRIBUTES -1
#else
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#endif
#endif

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <boolean.h>

#include <encodings/crc32.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <file/file_path.h>
#include <file/archive_file.h>
#include <string/stdstring.h>

#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <retro_stat.h>
#include <retro_assert.h>

#include <lists/string_list.h>
#include <string/stdstring.h>

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#include "../menu/menu_shader.h"
#endif

#ifdef HAVE_CHEEVOS
#include "../cheevos.h"
#endif

#include "tasks_internal.h"

#include "../command.h"
#include "../core_info.h"
#include "../content.h"
#include "../configuration.h"
#include "../defaults.h"
#include "../frontend/frontend.h"
#include "../playlist.h"
#include "../paths.h"
#include "../retroarch.h"
#include "../verbosity.h"

#include "../msg_hash.h"
#include "../content.h"
#include "../dynamic.h"
#include "../patch.h"
#include "../runloop.h"
#include "../retroarch.h"
#include "../file_path_special.h"
#include "../core.h"
#include "../dirs.h"
#include "../paths.h"
#include "../verbosity.h"

#define MAX_ARGS 32

typedef struct content_stream
{
   uint32_t a;
   const uint8_t *b;
   size_t c;
   uint32_t crc;
} content_stream_t;

typedef struct content_information_ctx
{
   struct
   {
      struct retro_subsystem_info *data;
      unsigned size;
   } subsystem;

   char *valid_extensions;
   char *directory_cache;
   char *directory_system;

   bool history_list_enable;
   bool block_extract;
   bool need_fullpath;
   bool set_supports_no_game_enable;
   bool patch_is_blocked;
   bool bios_is_missing;

   struct string_list *temporary_content;
} content_information_ctx_t;

static struct string_list *temporary_content                  = NULL;
static bool _content_is_inited                                = false;
static bool core_does_not_need_content                        = false;
static uint32_t content_crc                                   = 0;

/**
 * content_file_read:
 * @path             : path to file.
 * @buf              : buffer to allocate and read the contents of the
 *                     file into. Needs to be freed manually.
 * @length           : Number of items read, -1 on error.
 *
 * Read the contents of a file into @buf. Will call file_archive_compressed_read
 * if path contains a compressed file, otherwise will call filestream_read_file().
 *
 * Returns: 1 if file read, 0 on error.
 */
static int content_file_read(const char *path, void **buf, ssize_t *length)
{
#ifdef HAVE_COMPRESSION
   if (path_contains_compressed_file(path))
   {
      if (file_archive_compressed_read(path, buf, NULL, length))
         return 1;
   }
#endif
   return filestream_read_file(path, buf, length);
}

/**
 * content_load_init_wrap:
 * @args                 : Input arguments.
 * @argc                 : Count of arguments.
 * @argv                 : Arguments.
 *
 * Generates an @argc and @argv pair based on @args
 * of type rarch_main_wrap.
 **/
static void content_load_init_wrap(
      const struct rarch_main_wrap *args,
      int *argc, char **argv)
{
#ifdef HAVE_FILE_LOGGER
   int i;
#endif

   *argc = 0;
   argv[(*argc)++] = strdup("retroarch");

#ifdef HAVE_DYNAMIC
   if (!args->no_content)
   {
#endif
      if (args->content_path)
      {
         RARCH_LOG("Using content: %s.\n", args->content_path);
         argv[(*argc)++] = strdup(args->content_path);
      }
#ifdef HAVE_MENU
      else
      {
         RARCH_LOG("%s\n",
               msg_hash_to_str(MSG_NO_CONTENT_STARTING_DUMMY_CORE));
         argv[(*argc)++] = strdup("--menu");
      }
#endif
#ifdef HAVE_DYNAMIC
   }
#endif

   if (args->sram_path)
   {
      argv[(*argc)++] = strdup("-s");
      argv[(*argc)++] = strdup(args->sram_path);
   }

   if (args->state_path)
   {
      argv[(*argc)++] = strdup("-S");
      argv[(*argc)++] = strdup(args->state_path);
   }

   if (args->config_path)
   {
      argv[(*argc)++] = strdup("-c");
      argv[(*argc)++] = strdup(args->config_path);
   }

#ifdef HAVE_DYNAMIC
   if (args->libretro_path)
   {
      argv[(*argc)++] = strdup("-L");
      argv[(*argc)++] = strdup(args->libretro_path);
   }
#endif

   if (args->verbose)
      argv[(*argc)++] = strdup("-v");

#ifdef HAVE_FILE_LOGGER
   for (i = 0; i < *argc; i++)
      RARCH_LOG("arg #%d: %s\n", i, argv[i]);
#endif
}

/**
 * content_load:
 *
 * Loads content file and starts up RetroArch.
 * If no content file can be loaded, will start up RetroArch
 * as-is.
 *
 * Returns: false (0) if retroarch_main_init failed,
 * otherwise true (1).
 **/
static bool content_load(content_ctx_info_t *info)
{
   unsigned i;
   bool retval                       = true;
   int rarch_argc                    = 0;
   char *rarch_argv[MAX_ARGS]        = {NULL};
   char *argv_copy [MAX_ARGS]        = {NULL};
   char **rarch_argv_ptr             = (char**)info->argv;
   int *rarch_argc_ptr               = (int*)&info->argc;
   struct rarch_main_wrap *wrap_args = (struct rarch_main_wrap*)
      calloc(1, sizeof(*wrap_args));

   if (!wrap_args)
      return false;

   retro_assert(wrap_args);

   if (info->environ_get)
      info->environ_get(rarch_argc_ptr,
            rarch_argv_ptr, info->args, wrap_args);

   if (wrap_args->touched)
   {
      content_load_init_wrap(wrap_args, &rarch_argc, rarch_argv);
      memcpy(argv_copy, rarch_argv, sizeof(rarch_argv));
      rarch_argv_ptr = (char**)rarch_argv;
      rarch_argc_ptr = (int*)&rarch_argc;
   }

   rarch_ctl(RARCH_CTL_MAIN_DEINIT, NULL);

   wrap_args->argc = *rarch_argc_ptr;
   wrap_args->argv = rarch_argv_ptr;

   if (!retroarch_main_init(wrap_args->argc, wrap_args->argv))
   {
      retval = false;
      goto error;
   }

#ifdef HAVE_MENU
   menu_shader_manager_init();
#endif
   command_event(CMD_EVENT_HISTORY_INIT, NULL);
   command_event(CMD_EVENT_RESUME, NULL);
   command_event(CMD_EVENT_VIDEO_SET_ASPECT_RATIO, NULL);

   dir_check_defaults();

   frontend_driver_process_args(rarch_argc_ptr, rarch_argv_ptr);
   frontend_driver_content_loaded();

error:
   for (i = 0; i < ARRAY_SIZE(argv_copy); i++)
      free(argv_copy[i]);
   free(wrap_args);
   return retval;
}

/**
 * load_content_into_memory:
 * @path         : buffer of the content file.
 * @buf          : size   of the content file.
 * @length       : size of the content file that has been read from.
 *
 * Read the content file. If read into memory, also performs soft patching
 * (see patch_content function) in case soft patching has not been
 * blocked by the enduser.
 *
 * Returns: true if successful, false on error.
 **/
static bool load_content_into_memory(
      content_information_ctx_t *content_ctx,
      unsigned i, const char *path, void **buf,
      ssize_t *length)
{
   uint32_t *content_crc_ptr = NULL;
   uint8_t *ret_buf          = NULL;

   RARCH_LOG("%s: %s.\n",
         msg_hash_to_str(MSG_LOADING_CONTENT_FILE), path);
   if (!content_file_read(path, (void**) &ret_buf, length))
      return false;

   if (*length < 0)
      return false;

   if (i == 0)
   {
      /* First content file is significant, attempt to do patching,
       * CRC checking, etc. */

      /* Attempt to apply a patch. */
      if (!content_ctx->patch_is_blocked)
      {
         global_t *global = global_get_ptr();
         if (global)
            patch_content(
                  global->name.ips,
                  global->name.bps,
                  global->name.ups,
                  &ret_buf, length);
      }

      content_get_crc(&content_crc_ptr);

      *content_crc_ptr = encoding_crc32(0, ret_buf, *length);

      RARCH_LOG("CRC32: 0x%x .\n", (unsigned)*content_crc_ptr);
   }

   *buf = ret_buf;

   return true;
}

#ifdef HAVE_COMPRESSION
static bool load_content_from_compressed_archive(
      content_information_ctx_t *content_ctx,
      struct retro_game_info *info,
      unsigned i,
      struct string_list* additional_path_allocs,
      bool need_fullpath,
      const char *path,
      char **error_string)
{
   union string_list_elem_attr attributes;
   char new_path[PATH_MAX_LENGTH];
   char new_basedir[PATH_MAX_LENGTH];
   ssize_t new_path_len              = 0;
   bool ret                          = false;

   RARCH_LOG("Compressed file in case of need_fullpath."
         " Now extracting to temporary directory.\n");

   strlcpy(new_basedir, content_ctx->directory_cache,
         sizeof(new_basedir));

   if (string_is_empty(new_basedir) || !path_is_directory(new_basedir))
   {
      RARCH_WARN("Tried extracting to cache directory, but "
            "cache directory was not set or found. "
            "Setting cache directory to directory "
            "derived by basename...\n");
      fill_pathname_basedir(new_basedir, path,
            sizeof(new_basedir));
   }

   new_path[0]    = '\0';
   new_basedir[0] = '\0';
   attributes.i   = 0;

   fill_pathname_join(new_path, new_basedir,
         path_basename(path), sizeof(new_path));

   ret = file_archive_compressed_read(path, NULL, new_path, &new_path_len);

   if (!ret || new_path_len < 0)
   {
      char str[1024];
      snprintf(str, sizeof(str), "%s \"%s\".\n",
            msg_hash_to_str(MSG_COULD_NOT_READ_CONTENT_FILE),
            path);
      *error_string = strdup(str);
      return false;
   }

   string_list_append(additional_path_allocs, new_path, attributes);
   info[i].path =
      additional_path_allocs->elems[additional_path_allocs->size -1 ].data;

   if (!string_list_append(content_ctx->temporary_content, new_path, attributes))
      return false;

   return true;
}

static bool content_file_init_extract(
      struct string_list *content,
      content_information_ctx_t *content_ctx,
      const struct retro_subsystem_info *special,
      union string_list_elem_attr *attr,
      char **error_string
      )
{
   unsigned i;

   for (i = 0; i < content->size; i++)
   {
      char temp_content[PATH_MAX_LENGTH];
      char new_path[PATH_MAX_LENGTH];
      bool block_extract                 = content->elems[i].attr.i & 1;
      const char *path                   = content->elems[i].data;
      const char *valid_ext              = special ?
                                           special->roms[i].valid_extensions :
                                           content_ctx->valid_extensions;
      bool contains_compressed           = path_contains_compressed_file(path);

      /* Block extract check. */
      if (block_extract)
         continue;

      /* just use the first file in the archive */
      if (!contains_compressed && !path_is_compressed_file(path))
         continue;

      temp_content[0] = new_path[0] = '\0';

      strlcpy(temp_content, path, sizeof(temp_content));

      if (!valid_ext || !file_archive_extract_file(
               temp_content,
               sizeof(temp_content),
               valid_ext,
               !string_is_empty(content_ctx->directory_cache) ?
               content_ctx->directory_cache : NULL,
               new_path,
               sizeof(new_path)))
      {
         char str[1024];

         snprintf(str, sizeof(str), "%s: %s.\n",
               msg_hash_to_str(
                  MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE),
               temp_content);
         return false;
      }

      string_list_set(content, i, new_path);

      if (!string_list_append(content_ctx->temporary_content,
               new_path, *attr))
         return false;
   }

   return true;
}
#endif

/**
 * content_file_load:
 * @special          : subsystem of content to be loaded. Can be NULL.
 * content           :
 *
 * Load content file (for libretro core).
 *
 * Returns : true if successful, otherwise false.
 **/
static bool content_file_load(
      struct retro_game_info *info,
      const struct string_list *content,
      content_information_ctx_t *content_ctx,
      char **error_string,
      const struct retro_subsystem_info *special
      )
{
   unsigned i;
   retro_ctx_load_content_info_t load_info;
   char msg[1024];
   struct string_list *additional_path_allocs = string_list_new();

   msg[0] = '\0';

   if (!additional_path_allocs)
      return false;

   for (i = 0; i < content->size; i++)
   {
      int         attr     = content->elems[i].attr.i;
      const char *path     = content->elems[i].data;
      bool need_fullpath   = attr & 2;
      bool require_content = attr & 4;

      if (require_content && string_is_empty(path))
      {
         snprintf(msg, sizeof(msg),
               "%s\n",
               msg_hash_to_str(MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT));
         *error_string = strdup(msg);
         goto error;
      }

      info[i].path = NULL;

      if (!string_is_empty(path))
         info[i].path = path;

      if (!need_fullpath && !string_is_empty(path))
      {
         /* Load the content into memory. */

         ssize_t len = 0;

         if (!load_content_into_memory(
                  content_ctx,
                  i, path, (void**)&info[i].data, &len))
         {
            snprintf(msg, sizeof(msg),
                  "%s \"%s\".\n",
                  msg_hash_to_str(MSG_COULD_NOT_READ_CONTENT_FILE),
                  path);
            *error_string = strdup(msg);
            goto error;
         }

         info[i].size = len;
      }
      else
      {
         RARCH_LOG("%s\n",
               msg_hash_to_str(
                  MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT));

#ifdef HAVE_COMPRESSION
         if (     !content_ctx->block_extract
               && need_fullpath
               && path_contains_compressed_file(path)
               && !load_content_from_compressed_archive(
                  content_ctx,
                  &info[i], i,
                  additional_path_allocs, need_fullpath, path,
                  error_string))
            goto error;
#endif
      }
   }

   load_info.content = content;
   load_info.special = special;
   load_info.info    = info;

   if (!core_load_game(&load_info))
   {
      snprintf(msg, sizeof(msg),
            "%s.\n", msg_hash_to_str(MSG_FAILED_TO_LOAD_CONTENT));
      *error_string = strdup(msg);
      goto error;
   }

#ifdef HAVE_CHEEVOS
   if (!special)
   {
      const void *load_data = NULL;
      const char *path      = content->elems[0].data;

      cheevos_set_cheats();

      if (!string_is_empty(path))
         load_data = info;
      cheevos_load(load_data);
   }
#endif

   string_list_free(additional_path_allocs);

   return true;

error:
   string_list_free(additional_path_allocs);
   
   return false;
}

static const struct retro_subsystem_info *content_file_init_subsystem(
      content_information_ctx_t *content_ctx,
      char **error_string,
      bool *ret)
{
   char msg[1024];
   struct string_list *subsystem              = path_get_subsystem_list();
   const struct retro_subsystem_info *special = libretro_find_subsystem_info(
            content_ctx->subsystem.data, content_ctx->subsystem.size,
            path_get(RARCH_PATH_SUBSYSTEM));

   msg[0] = '\0';

   if (!special)
   {
      snprintf(msg, sizeof(msg),
            "Failed to find subsystem \"%s\" in libretro implementation.\n",
            path_get(RARCH_PATH_SUBSYSTEM));
      *error_string = strdup(msg);
      goto error;
   }

   if (special->num_roms && !subsystem)
   {
      snprintf(msg, sizeof(msg),
            "%s\n",
            msg_hash_to_str(MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT));
      *error_string = strdup(msg);
      goto error;
   }
   else if (special->num_roms && (special->num_roms != subsystem->size))
   {
      snprintf(msg, sizeof(msg),
            "Libretro core requires %u content files for "
            "subsystem \"%s\", but %u content files were provided.\n",
            special->num_roms, special->desc,
            (unsigned)subsystem->size);
      *error_string = strdup(msg);
      goto error;
   }
   else if (!special->num_roms && subsystem && subsystem->size)
   {
      snprintf(msg, sizeof(msg),
            "Libretro core takes no content for subsystem \"%s\", "
            "but %u content files were provided.\n",
            special->desc,
            (unsigned)subsystem->size);
      *error_string = strdup(msg);
      goto error;
   }

   *ret = true;
   return special;

error:
   *ret = false;
   return NULL;
}

static bool content_file_init_set_attribs(
      struct string_list *content,
      const struct retro_subsystem_info *special,
      content_information_ctx_t *content_ctx,
      char **error_string)
{
   union string_list_elem_attr attr;
   struct string_list *subsystem    = path_get_subsystem_list();

   attr.i                           = 0;

   if (!path_is_empty(RARCH_PATH_SUBSYSTEM) && special)
   {
      unsigned i;

      for (i = 0; i < subsystem->size; i++)
      {
         attr.i            = special->roms[i].block_extract;
         attr.i           |= special->roms[i].need_fullpath << 1;
         attr.i           |= special->roms[i].required      << 2;

         string_list_append(content, subsystem->elems[i].data, attr);
      }
   }
   else
   {
      attr.i               = content_ctx->block_extract;
      attr.i              |= content_ctx->need_fullpath << 1;
      attr.i              |= (!content_does_not_need_content())  << 2;

      if (path_is_empty(RARCH_PATH_CONTENT)
            && content_does_not_need_content()
            && content_ctx->set_supports_no_game_enable)
         string_list_append(content, "", attr);
      else
      {
         if (!path_is_empty(RARCH_PATH_CONTENT))
            string_list_append(content, path_get(RARCH_PATH_CONTENT), attr);
      }
   }

#ifdef HAVE_COMPRESSION
   /* Try to extract all content we're going to load if appropriate. */
   content_file_init_extract(content, content_ctx, special, &attr, error_string);
#endif
   return true;
}

/**
 * content_init_file:
 *
 * Initializes and loads a content file for the currently
 * selected libretro core.
 *
 * Returns : true if successful, otherwise false.
 **/
static bool content_file_init(
      content_information_ctx_t *content_ctx,
      char **error_string)
{
   struct retro_game_info               *info = NULL;
   struct string_list *content                = NULL;
   bool ret                                   = path_is_empty(RARCH_PATH_SUBSYSTEM) 
      ? true : false;
   const struct retro_subsystem_info *special = path_is_empty(RARCH_PATH_SUBSYSTEM) 
      ? NULL : content_file_init_subsystem(content_ctx, error_string, &ret);

   if (!ret)
      goto error;

   content = string_list_new();

   if (!content)
      goto error;

   if (!content_file_init_set_attribs(content, special, content_ctx, error_string))
      goto error;

   info                   = (struct retro_game_info*)
      calloc(content->size, sizeof(*info));

   if (info)
   {
      unsigned i;
      ret = content_file_load(info, content, content_ctx, error_string,
            special);

      for (i = 0; i < content->size; i++)
         free((void*)info[i].data);

      free(info);
   }

error:
   if (content)
      string_list_free(content);
   return ret;
}


#ifdef HAVE_MENU
static void menu_content_environment_get(int *argc, char *argv[],
      void *args, void *params_data)
{
   struct rarch_main_wrap *wrap_args = (struct rarch_main_wrap*)params_data;

   if (!wrap_args)
      return;

   wrap_args->no_content       = menu_driver_ctl(
         RARCH_MENU_CTL_HAS_LOAD_NO_CONTENT, NULL);

   if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_VERBOSITY, NULL))
      wrap_args->verbose       = verbosity_is_enabled();

   wrap_args->touched          = true;
   wrap_args->config_path      = NULL;
   wrap_args->sram_path        = NULL;
   wrap_args->state_path       = NULL;
   wrap_args->content_path     = NULL;

   if (!path_is_empty(RARCH_PATH_CONFIG))
      wrap_args->config_path   = path_get(RARCH_PATH_CONFIG);
   if (!dir_is_empty(RARCH_DIR_SAVEFILE))
      wrap_args->sram_path     = dir_get(RARCH_DIR_SAVEFILE);
   if (!dir_is_empty(RARCH_DIR_SAVESTATE))
      wrap_args->state_path    = dir_get(RARCH_DIR_SAVESTATE);
   if (!path_is_empty(RARCH_PATH_CONTENT))
      wrap_args->content_path  = path_get(RARCH_PATH_CONTENT);
   if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL))
      wrap_args->libretro_path = string_is_empty(path_get(RARCH_PATH_CORE)) ? NULL :
         path_get(RARCH_PATH_CORE);

}
#endif

/**
 * task_load_content:
 *
 * Loads content into currently selected core.
 * Will also optionally push the content entry to the history playlist.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
static bool task_load_content(content_ctx_info_t *content_info,
      content_information_ctx_t *content_ctx,
      bool launched_from_menu,
      enum content_mode_load mode,
      char **error_string)
{
   char name[255];
   char msg[255];

   name[0] = msg[0] = '\0';

   if (!content_load(content_info))
      goto error;

   /* Push entry to top of history playlist */
   if (_content_is_inited || content_does_not_need_content())
   {
      char tmp[PATH_MAX_LENGTH];
      struct retro_system_info *info = NULL;
      rarch_system_info_t *sys_info  = NULL;

      tmp[0] = '\0';

      runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &sys_info);
      if (sys_info)
         info = &sys_info->info;

#ifdef HAVE_MENU
      if (launched_from_menu)
         menu_driver_ctl(RARCH_MENU_CTL_SYSTEM_INFO_GET, &info);
#endif

      strlcpy(tmp, path_get(RARCH_PATH_CONTENT), sizeof(tmp));

      if (!launched_from_menu)
      {
         /* Path can be relative here.
          * Ensure we're pushing absolute path. */
         if (!string_is_empty(tmp))
            path_resolve_realpath(tmp, sizeof(tmp));
      }

      if (info && !string_is_empty(tmp))
      {
         const char *core_path      = NULL;
         const char *core_name      = NULL;
         playlist_t *playlist_tmp   = g_defaults.content_history;

         switch (path_is_media_type(tmp))
         {
            case RARCH_CONTENT_MOVIE:
#ifdef HAVE_FFMPEG
               playlist_tmp         = g_defaults.video_history;
               core_name            = "movieplayer";
               core_path            = "builtin";
#endif
               break;
            case RARCH_CONTENT_MUSIC:
#ifdef HAVE_FFMPEG
               playlist_tmp         = g_defaults.music_history;
               core_name            = "musicplayer";
               core_path            = "builtin";
#endif
               break;
            case RARCH_CONTENT_IMAGE:
#ifdef HAVE_IMAGEVIEWER
               playlist_tmp         = g_defaults.image_history;
               core_name            = "imageviewer";
               core_path            = "builtin";
#endif
               break;
            default:
               core_path            = path_get(RARCH_PATH_CORE);
               core_name            = info->library_name;
               break;
         }

         if (mode == CONTENT_MODE_LOAD_FROM_CLI)
         {
            settings_t *settings                       = config_get_ptr();
            content_ctx->history_list_enable = settings->history_list_enable;
         }

         if (
                  content_ctx->history_list_enable 
               && playlist_tmp 
               && playlist_push(
                  playlist_tmp,
                  tmp,
                  NULL,
                  core_path,
                  core_name,
                  NULL,
                  NULL)
               )
            playlist_write_file(playlist_tmp);
      }
   }

   return true;

error:
   if (launched_from_menu)
   {
      if (!path_is_empty(RARCH_PATH_CONTENT) && !string_is_empty(name))
      {
         snprintf(msg, sizeof(msg), "%s %s.\n",
               msg_hash_to_str(MSG_FAILED_TO_LOAD),
               name);
         *error_string = strdup(msg);
      }
   }
   return false;
}

static bool command_event_cmd_exec(const char *data,
      content_information_ctx_t *content_ctx,
      enum content_mode_load mode,
      char **error_string)
{
#if defined(HAVE_DYNAMIC)
   content_ctx_info_t content_info;

   content_info.argc        = 0;
   content_info.argv        = NULL;
   content_info.args        = NULL;
#ifdef HAVE_MENU
   content_info.environ_get = menu_content_environment_get;
#else
   content_info.environ_get = NULL;
#endif
#endif

   if (path_get(RARCH_PATH_CONTENT) != (void*)data)
   {
      path_clear(RARCH_PATH_CONTENT);
      if (!string_is_empty(data))
         path_set(RARCH_PATH_CONTENT, data);
   }

#if defined(HAVE_DYNAMIC)
   if (!task_load_content(&content_info, content_ctx,
            false, mode, error_string))
      return false;
#else
   frontend_driver_set_fork(FRONTEND_FORK_CORE_WITH_ARGS);
#endif

   return true;
}

static void task_push_content_update_firmware_status(
      content_information_ctx_t *content_ctx)
{
   char s[PATH_MAX_LENGTH];
   core_info_ctx_firmware_t firmware_info;

   core_info_t *core_info     = NULL;

   core_info_get_current_core(&core_info);

   if (!core_info)
      return;

   firmware_info.path         = core_info->path;

   if (!string_is_empty(content_ctx->directory_system))
      firmware_info.directory.system = content_ctx->directory_system;
   else
   {
      strlcpy(s, path_get(RARCH_PATH_CONTENT) ,sizeof(s));
      path_basedir_wrapper(s);
      firmware_info.directory.system = s;
   }

   RARCH_LOG("Updating firmware status for: %s on %s\n", core_info->path, 
         firmware_info.directory.system);
   core_info_list_update_missing_firmware(&firmware_info);
}

bool task_push_content_load_default(
      const char *core_path,
      const char *fullpath,
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      enum content_mode_load mode,
      retro_task_callback_t cb,
      void *user_data)
{
   content_information_ctx_t content_ctx;
  
   bool loading_from_menu                     = false;
   char *error_string                         = NULL;
   settings_t *settings                       = config_get_ptr();

   if (!content_info)
      return false;
   content_ctx.patch_is_blocked               = rarch_ctl(RARCH_CTL_IS_PATCH_BLOCKED, NULL);
   content_ctx.bios_is_missing                = runloop_ctl(RUNLOOP_CTL_IS_MISSING_BIOS, NULL);
   content_ctx.history_list_enable            = false;
   content_ctx.directory_system               = NULL;
   content_ctx.directory_cache                = NULL;
   content_ctx.valid_extensions               = NULL;
   content_ctx.block_extract                  = false;
   content_ctx.need_fullpath                  = false;
   content_ctx.set_supports_no_game_enable    = false;

   content_ctx.subsystem.data                 = NULL;
   content_ctx.subsystem.size                 = 0;

   if (settings)
   {
      content_ctx.history_list_enable         = settings->history_list_enable;

      if (!string_is_empty(settings->directory.system))
         content_ctx.directory_system         = strdup(settings->directory.system);
   }

   /* First we determine if we are loading from a menu */
   switch (mode)
   {
      case CONTENT_MODE_LOAD_NOTHING_WITH_NEW_CORE_FROM_MENU:
#if defined(HAVE_VIDEO_PROCESSOR)
      case CONTENT_MODE_LOAD_NOTHING_WITH_VIDEO_PROCESSOR_CORE_FROM_MENU:
#endif
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
      case CONTENT_MODE_LOAD_NOTHING_WITH_NET_RETROPAD_CORE_FROM_MENU:
#endif
      case CONTENT_MODE_LOAD_NOTHING_WITH_CURRENT_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_CURRENT_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_CURRENT_CORE_FROM_COMPANION_UI:
      case CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_COMPANION_UI:
#ifdef HAVE_DYNAMIC
      case CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_MENU:
#endif
      case CONTENT_MODE_LOAD_CONTENT_WITH_FFMPEG_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_IMAGEVIEWER_CORE_FROM_MENU:
         loading_from_menu = true;
         break;
      default:
         break;
   }

   switch (mode)
   {
      case CONTENT_MODE_LOAD_NOTHING_WITH_CURRENT_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_NOTHING_WITH_VIDEO_PROCESSOR_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_NOTHING_WITH_NET_RETROPAD_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_FROM_PLAYLIST_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_FFMPEG_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_CURRENT_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_IMAGEVIEWER_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_CURRENT_CORE_FROM_COMPANION_UI:
      case CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_COMPANION_UI:
      case CONTENT_MODE_LOAD_NOTHING_WITH_DUMMY_CORE:
#ifdef HAVE_MENU
         if (!content_info->environ_get)
            content_info->environ_get = menu_content_environment_get;
#endif
         break;
      default:
         break;
   }

   /* Clear content path */
   switch (mode)
   {
      case CONTENT_MODE_LOAD_NOTHING_WITH_DUMMY_CORE:
      case CONTENT_MODE_LOAD_NOTHING_WITH_CURRENT_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_NOTHING_WITH_VIDEO_PROCESSOR_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_NOTHING_WITH_NET_RETROPAD_CORE_FROM_MENU:
         path_clear(RARCH_PATH_CONTENT);
         break;
      default:
         break;
   }

   /* Set content path */
   switch (mode)
   {
      case CONTENT_MODE_LOAD_CONTENT_WITH_CURRENT_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_CURRENT_CORE_FROM_COMPANION_UI:
      case CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_COMPANION_UI:
      case CONTENT_MODE_LOAD_CONTENT_WITH_FFMPEG_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_IMAGEVIEWER_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_MENU:
         path_set(RARCH_PATH_CONTENT, fullpath);
         break;
      default:
         break;
   }

   /* Set libretro core path */
   switch (mode)
   {
      case CONTENT_MODE_LOAD_NOTHING_WITH_NEW_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_COMPANION_UI:
      case CONTENT_MODE_LOAD_CONTENT_FROM_PLAYLIST_FROM_MENU:
         runloop_ctl(RUNLOOP_CTL_SET_LIBRETRO_PATH, (void*)core_path);
         break;
      default:
         break;
   }

   /* Is content required by this core? */
   switch (mode)
   {
      case CONTENT_MODE_LOAD_CONTENT_FROM_PLAYLIST_FROM_MENU:
#ifdef HAVE_MENU
         if (fullpath)
            menu_driver_ctl(RARCH_MENU_CTL_UNSET_LOAD_NO_CONTENT, NULL);
         else
            menu_driver_ctl(RARCH_MENU_CTL_SET_LOAD_NO_CONTENT, NULL);
#endif
         break;
      default:
         break;
   }

   /* On targets that have no dynamic core loading support, we'd
    * execute the new core from this point. If this returns false,
    * we assume we can dynamically load the core. */
   switch (mode)
   {
      case CONTENT_MODE_LOAD_CONTENT_FROM_PLAYLIST_FROM_MENU:
         if (!command_event_cmd_exec(fullpath, &content_ctx, mode, &error_string))
            goto error;
#ifndef HAVE_DYNAMIC
         runloop_ctl(RUNLOOP_CTL_SET_SHUTDOWN, NULL);
#ifdef HAVE_MENU
         rarch_ctl(RARCH_CTL_MENU_RUNNING_FINISHED, NULL);
#endif
#endif
         break;
      default:
         break;
   }

   /* Load core */
   switch (mode)
   {
      case CONTENT_MODE_LOAD_NOTHING_WITH_NEW_CORE_FROM_MENU:
#ifdef HAVE_DYNAMIC
      case CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_COMPANION_UI:
      case CONTENT_MODE_LOAD_CONTENT_FROM_PLAYLIST_FROM_MENU:
#endif
         command_event(CMD_EVENT_LOAD_CORE, NULL);
         break;
      default:
         break;
   }

#ifndef HAVE_DYNAMIC
   /* Fork core? */
   switch (mode)
   {
     case CONTENT_MODE_LOAD_NOTHING_WITH_NEW_CORE_FROM_MENU:
         if (!frontend_driver_set_fork(FRONTEND_FORK_CORE))
            goto cleanup;
         break;
      default:
         break;
   }
#endif

   /* Preliminary stuff that has to be done before we
    * load the actual content. Can differ per mode. */
   switch (mode)
   {
      case CONTENT_MODE_LOAD_NOTHING_WITH_DUMMY_CORE:
         runloop_ctl(RUNLOOP_CTL_STATE_FREE, NULL);
#ifdef HAVE_MENU
         menu_driver_ctl(RARCH_MENU_CTL_UNSET_LOAD_NO_CONTENT, NULL);
#endif
         runloop_ctl(RUNLOOP_CTL_DATA_DEINIT, NULL);
         runloop_ctl(RUNLOOP_CTL_TASK_INIT, NULL);
         break;
      case CONTENT_MODE_LOAD_NOTHING_WITH_CURRENT_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_NOTHING_WITH_NEW_CORE_FROM_MENU:
         retroarch_set_current_core_type(type, true);
         break;
      case CONTENT_MODE_LOAD_NOTHING_WITH_NET_RETROPAD_CORE_FROM_MENU:
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
         retroarch_set_current_core_type(CORE_TYPE_NETRETROPAD, true);
         break;
#endif
      case CONTENT_MODE_LOAD_NOTHING_WITH_VIDEO_PROCESSOR_CORE_FROM_MENU:
#ifdef HAVE_VIDEO_PROCESSOR
         retroarch_set_current_core_type(CORE_TYPE_VIDEO_PROCESSOR, true);
         break;
#endif
      default:
         break;
   }

   /* Load content */
   switch (mode)
   {
      case CONTENT_MODE_LOAD_NOTHING_WITH_DUMMY_CORE:
         if (!task_load_content(content_info, &content_ctx,
                  loading_from_menu, mode, &error_string))
            goto error;
         break;
      case CONTENT_MODE_LOAD_FROM_CLI:
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
      case CONTENT_MODE_LOAD_NOTHING_WITH_NET_RETROPAD_CORE_FROM_MENU:
#endif
#ifdef HAVE_VIDEO_PROCESSOR
      case CONTENT_MODE_LOAD_NOTHING_WITH_VIDEO_PROCESSOR_CORE_FROM_MENU:
#endif
      case CONTENT_MODE_LOAD_NOTHING_WITH_CURRENT_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_CURRENT_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_CURRENT_CORE_FROM_COMPANION_UI:
      case CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_COMPANION_UI:
#ifdef HAVE_DYNAMIC
      case CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_MENU:
#endif
      case CONTENT_MODE_LOAD_CONTENT_WITH_FFMPEG_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_IMAGEVIEWER_CORE_FROM_MENU:
         task_push_content_update_firmware_status(&content_ctx);

         if(
               content_ctx.bios_is_missing && 
               settings->check_firmware_before_loading)
               goto skip;

         if (!task_load_content(content_info, &content_ctx,
                  loading_from_menu, mode, &error_string))
            goto error;
         break;
#ifndef HAVE_DYNAMIC
      case CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_MENU:
         command_event_cmd_exec(path_get(RARCH_PATH_CONTENT), &content_ctx, 
               mode, &error_string);
         command_event(CMD_EVENT_QUIT, NULL);
         break;
#endif
      case CONTENT_MODE_LOAD_NONE:
      default:
         break;
   }

   /* Push quick menu onto menu stack */
   switch (mode)
   {
      case CONTENT_MODE_LOAD_CONTENT_FROM_PLAYLIST_FROM_MENU:
      case CONTENT_MODE_LOAD_NOTHING_WITH_NEW_CORE_FROM_MENU:
         break;
      default:
#ifdef HAVE_MENU
         if (type != CORE_TYPE_DUMMY && mode != CONTENT_MODE_LOAD_FROM_CLI)
            menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, NULL);
#endif
         break;
   }

   if (content_ctx.directory_system)
      free(content_ctx.directory_system);

   return true;

error:

   if (error_string)
   {
      runloop_msg_queue_push(error_string, 2, 90, true);
      free(error_string);
   }

#ifdef HAVE_MENU
   switch (mode)
   {
      case CONTENT_MODE_LOAD_CONTENT_FROM_PLAYLIST_FROM_MENU:
      case CONTENT_MODE_LOAD_NOTHING_WITH_CURRENT_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_NOTHING_WITH_NET_RETROPAD_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_NOTHING_WITH_VIDEO_PROCESSOR_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_CURRENT_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_FFMPEG_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_IMAGEVIEWER_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_MENU:
         rarch_ctl(RARCH_CTL_MENU_RUNNING, NULL);
         break;
      default:
         break;
   }
#endif

   if (content_ctx.directory_system)
      free(content_ctx.directory_system);

   return false;

skip:
   runloop_msg_queue_push(msg_hash_to_str(MSG_FIRMWARE), 100, 500, true);
   RARCH_LOG("Load content blocked. Reason:  %s\n", msg_hash_to_str(MSG_FIRMWARE));

   return true;

#ifndef HAVE_DYNAMIC
cleanup:
   if (content_ctx.directory_system)
      free(content_ctx.directory_system);

   return false;
#endif
}

bool content_does_not_need_content(void)
{
   return core_does_not_need_content;
}

void content_set_does_not_need_content(void)
{
   core_does_not_need_content = true;
}

void content_unset_does_not_need_content(void)
{
   core_does_not_need_content = false;
}

bool content_get_crc(uint32_t **content_crc_ptr)
{
   if (!content_crc_ptr)
      return false;
   *content_crc_ptr = &content_crc;
   return true;
}

bool content_is_inited(void)
{
   return _content_is_inited;
}

void content_deinit(void)
{
   unsigned i;

   if (temporary_content)
   {
      for (i = 0; i < temporary_content->size; i++)
      {
         const char *path = temporary_content->elems[i].data;

         RARCH_LOG("%s: %s.\n",
               msg_hash_to_str(MSG_REMOVING_TEMPORARY_CONTENT_FILE), path);
         if (remove(path) < 0)
            RARCH_ERR("%s: %s.\n",
                  msg_hash_to_str(MSG_FAILED_TO_REMOVE_TEMPORARY_FILE),
                  path);
      }
      string_list_free(temporary_content);
   }

   temporary_content          = NULL;
   content_crc                = 0;
   _content_is_inited         = false;
   core_does_not_need_content = false;
}

/* Initializes and loads a content file for the currently
 * selected libretro core. */
bool content_init(void)
{
   content_information_ctx_t content_ctx;

   bool ret                                   = true;
   char *error_string                         = NULL;
   rarch_system_info_t *sys_info              = NULL;
   settings_t *settings                       = config_get_ptr();
   temporary_content                          = string_list_new();

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &sys_info);

   content_ctx.temporary_content              = temporary_content;
   content_ctx.history_list_enable            = false;
   content_ctx.directory_system               = NULL;
   content_ctx.directory_cache                = NULL;
   content_ctx.valid_extensions               = NULL;
   content_ctx.block_extract                  = false;
   content_ctx.need_fullpath                  = false;
   content_ctx.set_supports_no_game_enable    = false;

   content_ctx.subsystem.data                 = NULL;
   content_ctx.subsystem.size                 = 0;
   
   if (sys_info)
   {
      content_ctx.history_list_enable         = settings->history_list_enable;
      content_ctx.set_supports_no_game_enable = settings->set_supports_no_game_enable;

      if (!string_is_empty(settings->directory.system))
         content_ctx.directory_system         = strdup(settings->directory.system);
      if (!string_is_empty(settings->directory.cache))
         content_ctx.directory_cache          = strdup(settings->directory.cache);
      if (!string_is_empty(sys_info->info.valid_extensions))
         content_ctx.valid_extensions         = strdup(sys_info->info.valid_extensions);

      content_ctx.block_extract               = sys_info->info.block_extract;
      content_ctx.need_fullpath               = sys_info->info.need_fullpath;

      content_ctx.subsystem.data              = sys_info->subsystem.data;
      content_ctx.subsystem.size              = sys_info->subsystem.size;
   }

   if (     !temporary_content 
         || !content_file_init(&content_ctx, &error_string))
   {
      content_deinit();

      ret = false;
      goto end;
   }

   _content_is_inited = true;

end:
   if (content_ctx.directory_system)
      free(content_ctx.directory_system);
   if (content_ctx.directory_cache)
      free(content_ctx.directory_cache);
   if (content_ctx.valid_extensions)
      free(content_ctx.valid_extensions);

   if (error_string)
   {
      if (ret)
      {
         RARCH_LOG("%s\n", error_string);
      }
      else
      {
         RARCH_ERR("%s\n", error_string);
      }
      runloop_msg_queue_push(error_string, 2, ret ? 1 : 180, true);
      free(error_string);
   }

   return ret;
}

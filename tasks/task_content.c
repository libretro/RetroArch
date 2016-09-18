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

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#include "../menu/menu_display.h"
#include "../menu/menu_content.h"
#endif

#include "tasks_internal.h"

#include "../command.h"
#include "../content.h"
#include "../defaults.h"
#include "../configuration.h"
#include "../frontend/frontend.h"
#include "../paths.h"
#include "../retroarch.h"
#include "../verbosity.h"

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

#include <boolean.h>

#include <encodings/utf.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <file/file_path.h>
#ifdef HAVE_COMPRESSION
#include <file/archive_file.h>
#endif
#include <string/stdstring.h>

#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <retro_stat.h>
#include <retro_assert.h>

#include <lists/string_list.h>
#include <string/stdstring.h>

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

#ifdef HAVE_CHEEVOS
#include "../cheevos.h"
#endif

#define MAX_ARGS 32

typedef struct content_stream
{
   uint32_t a;
   const uint8_t *b;
   size_t c;
   uint32_t crc;
} content_stream_t;

static const struct file_archive_file_backend *stream_backend = NULL;
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

static void check_defaults_dir_create_dir(const char *path)
{
   char new_path[PATH_MAX_LENGTH] = {0};
   fill_pathname_expand_special(new_path,
         path, sizeof(new_path));

   if (path_is_directory(new_path))
      return;
   path_mkdir(new_path);
}

static void check_default_dirs(void)
{
   /* early return for people with a custom folder setup
      so it doesn't create unnecessary directories
    */
   if (path_file_exists("custom.ini"))
      return;

   if (!string_is_empty(g_defaults.dir.core_assets))
      check_defaults_dir_create_dir(g_defaults.dir.core_assets);
   if (!string_is_empty(g_defaults.dir.remap))
      check_defaults_dir_create_dir(g_defaults.dir.remap);
   if (!string_is_empty(g_defaults.dir.screenshot))
      check_defaults_dir_create_dir(g_defaults.dir.screenshot);
   if (!string_is_empty(g_defaults.dir.core))
      check_defaults_dir_create_dir(g_defaults.dir.core);
   if (!string_is_empty(g_defaults.dir.autoconfig))
      check_defaults_dir_create_dir(g_defaults.dir.autoconfig);
   if (!string_is_empty(g_defaults.dir.audio_filter))
      check_defaults_dir_create_dir(g_defaults.dir.audio_filter);
   if (!string_is_empty(g_defaults.dir.video_filter))
      check_defaults_dir_create_dir(g_defaults.dir.video_filter);
   if (!string_is_empty(g_defaults.dir.assets))
      check_defaults_dir_create_dir(g_defaults.dir.assets);
   if (!string_is_empty(g_defaults.dir.playlist))
      check_defaults_dir_create_dir(g_defaults.dir.playlist);
   if (!string_is_empty(g_defaults.dir.core))
      check_defaults_dir_create_dir(g_defaults.dir.core);
   if (!string_is_empty(g_defaults.dir.core_info))
      check_defaults_dir_create_dir(g_defaults.dir.core_info);
   if (!string_is_empty(g_defaults.dir.overlay))
      check_defaults_dir_create_dir(g_defaults.dir.overlay);
   if (!string_is_empty(g_defaults.dir.port))
      check_defaults_dir_create_dir(g_defaults.dir.port);
   if (!string_is_empty(g_defaults.dir.shader))
      check_defaults_dir_create_dir(g_defaults.dir.shader);
   if (!string_is_empty(g_defaults.dir.savestate))
      check_defaults_dir_create_dir(g_defaults.dir.savestate);
   if (!string_is_empty(g_defaults.dir.sram))
      check_defaults_dir_create_dir(g_defaults.dir.sram);
   if (!string_is_empty(g_defaults.dir.system))
      check_defaults_dir_create_dir(g_defaults.dir.system);
   if (!string_is_empty(g_defaults.dir.resampler))
      check_defaults_dir_create_dir(g_defaults.dir.resampler);
   if (!string_is_empty(g_defaults.dir.menu_config))
      check_defaults_dir_create_dir(g_defaults.dir.menu_config);
   if (!string_is_empty(g_defaults.dir.content_history))
      check_defaults_dir_create_dir(g_defaults.dir.content_history);
   if (!string_is_empty(g_defaults.dir.cache))
      check_defaults_dir_create_dir(g_defaults.dir.cache);
   if (!string_is_empty(g_defaults.dir.database))
      check_defaults_dir_create_dir(g_defaults.dir.database);
   if (!string_is_empty(g_defaults.dir.cursor))
      check_defaults_dir_create_dir(g_defaults.dir.cursor);
   if (!string_is_empty(g_defaults.dir.cheats))
      check_defaults_dir_create_dir(g_defaults.dir.cheats);
   if (!string_is_empty(g_defaults.dir.thumbnails))
      check_defaults_dir_create_dir(g_defaults.dir.thumbnails);
}

bool content_push_to_history_playlist(
      void *data,
      const char *path,
      const char *core_name,
      const char *core_path)
{
   playlist_t *playlist   = (playlist_t*)data;
   settings_t *settings   = config_get_ptr();

   /* If the history list is not enabled, early return. */
   if (!settings->history_list_enable)
      return false;
   if (!playlist)
      return false;

   return playlist_push(playlist,
         path,
         NULL,
         core_path,
         core_name,
         NULL,
         NULL);
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
   menu_driver_ctl(RARCH_MENU_CTL_SHADER_MANAGER_INIT, NULL);
#endif
   command_event(CMD_EVENT_HISTORY_INIT, NULL);
   command_event(CMD_EVENT_RESUME, NULL);
   command_event(CMD_EVENT_VIDEO_SET_ASPECT_RATIO, NULL);

   check_default_dirs();

   frontend_driver_process_args(rarch_argc_ptr, rarch_argv_ptr);
   frontend_driver_content_loaded();

error:
   for (i = 0; i < ARRAY_SIZE(argv_copy); i++)
      free(argv_copy[i]);
   free(wrap_args);
   return retval;
}

/**
 * read_content_file:
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
static bool read_content_file(unsigned i, const char *path, void **buf,
      ssize_t *length)
{
#ifdef HAVE_ZLIB
   content_stream_t stream_info;
   uint32_t *content_crc_ptr = NULL;
#endif
   uint8_t *ret_buf          = NULL;
   global_t *global          = global_get_ptr();

   RARCH_LOG("%s: %s.\n",
         msg_hash_to_str(MSG_LOADING_CONTENT_FILE), path);
   if (!content_file_read(path, (void**) &ret_buf, length))
      return false;

   if (*length < 0)
      return false;

   if (i != 0)
      return true;

   /* Attempt to apply a patch. */
   if (!global->patch.block_patch)
      patch_content(&ret_buf, length);

#ifdef HAVE_ZLIB
   content_get_crc(&content_crc_ptr);

   stream_info.a = 0;
   stream_info.b = ret_buf;
   stream_info.c = *length;

   if (!stream_backend)
      stream_backend = file_archive_get_zlib_file_backend();
   stream_info.crc = stream_backend->stream_crc_calculate(
         stream_info.a, stream_info.b, stream_info.c);
   *content_crc_ptr = stream_info.crc;

   RARCH_LOG("CRC32: 0x%x .\n", (unsigned)*content_crc_ptr);
#endif
   *buf = ret_buf;

   return true;
}

/**
 * dump_to_file_desperate:
 * @data         : pointer to data buffer.
 * @size         : size of @data.
 * @type         : type of file to be saved.
 *
 * Attempt to save valuable RAM data somewhere.
 **/
bool dump_to_file_desperate(const void *data,
      size_t size, unsigned type)
{
   time_t time_;
   char timebuf[256]                      = {0};
   char application_data[PATH_MAX_LENGTH] = {0};
   char path[PATH_MAX_LENGTH]             = {0};

   if (!fill_pathname_application_data(application_data,
            sizeof(application_data)))
      return false;

   snprintf(path, sizeof(path), "%s/RetroArch-recovery-%u",
      application_data, type);

   time(&time_);

   strftime(timebuf, sizeof(timebuf), "%Y-%m-%d-%H-%M-%S", localtime(&time_));
   strlcat(path, timebuf, sizeof(path));

   if (!filestream_write_file(path, data, size))
      return false;

   RARCH_WARN("Succeeded in saving RAM data to \"%s\".\n", path);
   return true;
}

/* Load the content into memory. */
static bool load_content_into_memory(
      struct retro_game_info *info,
      unsigned i,
      const char *path)
{
   ssize_t len;
   bool ret = false;

   if (i == 0)
   {
      /* First content file is significant, attempt to do patching,
       * CRC checking, etc. */
      ret = read_content_file(i, path, (void**)&info->data, &len);
   }
   else
      ret = content_file_read(path, (void**)&info->data, &len);

   if (!ret || len < 0)
      goto error;

   info->size = len;

   return true;

error:
   RARCH_ERR("%s \"%s\".\n",
         msg_hash_to_str(MSG_COULD_NOT_READ_CONTENT_FILE),
         path);
   return false;
}

#ifdef HAVE_COMPRESSION
static bool load_content_from_compressed_archive(
      struct string_list *temporary_content,
      struct retro_game_info *info, unsigned i,
      struct string_list* additional_path_allocs,
      bool need_fullpath, const char *path)
{
   union string_list_elem_attr attributes;
   char new_path[PATH_MAX_LENGTH]    = {0};
   char new_basedir[PATH_MAX_LENGTH] = {0};
   ssize_t new_path_len              = 0;
   bool ret                          = false;
   rarch_system_info_t      *sys_info= NULL;
   settings_t *settings              = config_get_ptr();

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &sys_info);

   if (sys_info && sys_info->info.block_extract)
      return true;
   if (!need_fullpath || !path_contains_compressed_file(path))
      return true;

   RARCH_LOG("Compressed file in case of need_fullpath."
         " Now extracting to temporary directory.\n");

   strlcpy(new_basedir, settings->directory.cache,
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

   attributes.i = 0;
   fill_pathname_join(new_path, new_basedir,
         path_basename(path), sizeof(new_path));

   ret = file_archive_compressed_read(path, NULL, new_path, &new_path_len);

   if (!ret || new_path_len < 0)
   {
      RARCH_ERR("%s \"%s\".\n",
            msg_hash_to_str(MSG_COULD_NOT_READ_CONTENT_FILE),
            path);
      return false;
   }

   string_list_append(additional_path_allocs, new_path, attributes);
   info[i].path =
      additional_path_allocs->elems[additional_path_allocs->size -1 ].data;

   if (!string_list_append(temporary_content, new_path, attributes))
      return false;

   return true;
}
#endif

/**
 * load_content:
 * @special          : subsystem of content to be loaded. Can be NULL.
 * content           :
 *
 * Load content file (for libretro core).
 *
 * Returns : true if successful, otherwise false.
 **/
static bool load_content(
      struct string_list *temporary_content,
      struct retro_game_info *info,
      const struct string_list *content,
      const struct retro_subsystem_info *special,
      struct string_list* additional_path_allocs
      )
{
   unsigned i;
   retro_ctx_load_content_info_t load_info;

   if (!info || !additional_path_allocs)
      return false;

   for (i = 0; i < content->size; i++)
   {
      int         attr     = content->elems[i].attr.i;
      bool need_fullpath   = attr & 2;
      bool require_content = attr & 4;
      const char *path     = content->elems[i].data;

      if (require_content && string_is_empty(path))
      {
         RARCH_LOG("%s\n",
               msg_hash_to_str(MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT));
         return false;
      }

      info[i].path = NULL;

      if (!string_is_empty(path))
         info[i].path = path;

      if (!need_fullpath && !string_is_empty(path))
      {
         if (!load_content_into_memory(&info[i], i, path))
            return false;
      }
      else
      {
         RARCH_LOG("%s\n",
               msg_hash_to_str(
                  MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT));

#ifdef HAVE_COMPRESSION
         if (!load_content_from_compressed_archive(
                  temporary_content,
                  &info[i], i,
                  additional_path_allocs, need_fullpath, path))
            return false;
#endif
      }
   }

   load_info.content = content;
   load_info.special = special;
   load_info.info    = info;

   if (!core_load_game(&load_info))
   {
      RARCH_ERR("%s.\n", msg_hash_to_str(MSG_FAILED_TO_LOAD_CONTENT));
      return false;
   }

#ifdef HAVE_CHEEVOS
   if (!special)
   {
      const void *load_data = NULL;

      cheevos_set_cheats();

      if (!string_is_empty(content->elems[0].data))
         load_data = info;
      cheevos_load(load_data);
   }
#endif

   return true;
}

static const struct retro_subsystem_info *init_content_file_subsystem(
      bool *ret, rarch_system_info_t *system)
{
   global_t *global = global_get_ptr();
   const struct retro_subsystem_info *special =
      libretro_find_subsystem_info(system->subsystem.data,
         system->subsystem.size, global->subsystem);

   if (!special)
   {
      RARCH_ERR(
            "Failed to find subsystem \"%s\" in libretro implementation.\n",
            global->subsystem);
      goto error;
   }

   if (special->num_roms && !global->subsystem_fullpaths)
   {
      RARCH_ERR("%s\n",
            msg_hash_to_str(MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT));
      goto error;
   }
   else if (special->num_roms && special->num_roms
         != global->subsystem_fullpaths->size)
   {
      RARCH_ERR("Libretro core requires %u content files for "
            "subsystem \"%s\", but %u content files were provided.\n",
            special->num_roms, special->desc,
            (unsigned)global->subsystem_fullpaths->size);
      goto error;
   }
   else if (!special->num_roms && global->subsystem_fullpaths
         && global->subsystem_fullpaths->size)
   {
      RARCH_ERR("Libretro core takes no content for subsystem \"%s\", "
            "but %u content files were provided.\n",
            special->desc,
            (unsigned)global->subsystem_fullpaths->size);
      goto error;
   }

   *ret = true;
   return special;

error:
   *ret = false;
   return NULL;
}

#ifdef HAVE_COMPRESSION
static bool init_content_file_extract(
      struct string_list *temporary_content,
      struct string_list *content,
      rarch_system_info_t *system,
      const struct retro_subsystem_info *special,
      union string_list_elem_attr *attr
      )
{
   unsigned i;
   settings_t *settings = config_get_ptr();

   for (i = 0; i < content->size; i++)
   {
      char temp_content[PATH_MAX_LENGTH] = {0};
      char new_path[PATH_MAX_LENGTH]     = {0};
      bool compressed                    = NULL;
      const char *valid_ext              = system->info.valid_extensions;

      /* Block extract check. */
      if (content->elems[i].attr.i & 1)
         continue;

      compressed            = path_contains_compressed_file(content->elems[i].data);

      if (special)
         valid_ext          = special->roms[i].valid_extensions;

      if (!compressed)
         continue;

      strlcpy(temp_content, content->elems[i].data,
            sizeof(temp_content));

      if (!file_archive_extract_first_file(temp_content,
               sizeof(temp_content), valid_ext,
               *settings->directory.cache ?
               settings->directory.cache : NULL,
               new_path, sizeof(new_path)))
      {
         RARCH_ERR("%s: %s.\n",
               msg_hash_to_str(
                  MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE),
               temp_content);
         runloop_msg_queue_push(
               msg_hash_to_str(MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE)
               , 2, 180, true);
         return false;
      }

      string_list_set(content, i, new_path);
      if (!string_list_append(temporary_content,
               new_path, *attr))
         return false;
   }

   return true;
}
#endif

static bool init_content_file_set_attribs(
      struct string_list *temporary_content,
      struct string_list *content,
      rarch_system_info_t *system,
      const struct retro_subsystem_info *special)
{
   union string_list_elem_attr attr;
   global_t *global        = global_get_ptr();

   attr.i                  = 0;

   if (!string_is_empty(global->subsystem) && special)
   {
      unsigned i;

      for (i = 0; i < global->subsystem_fullpaths->size; i++)
      {
         attr.i            = special->roms[i].block_extract;
         attr.i           |= special->roms[i].need_fullpath << 1;
         attr.i           |= special->roms[i].required      << 2;

         string_list_append(content,
               global->subsystem_fullpaths->elems[i].data, attr);
      }
   }
   else
   {
      char       *fullpath = NULL;
      settings_t *settings = config_get_ptr();

      attr.i               = system->info.block_extract;
      attr.i              |= system->info.need_fullpath << 1;
      attr.i              |= (!content_does_not_need_content())  << 2;

      if (!runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath)
            && content_does_not_need_content()
            && settings->set_supports_no_game_enable)
         string_list_append(content, "", attr);
      else
      {
         if(fullpath)
            string_list_append(content, fullpath, attr);
      }
   }

#ifdef HAVE_COMPRESSION
   /* Try to extract all content we're going to load if appropriate. */
   if (!init_content_file_extract(temporary_content,
            content, system, special, &attr))
      return false;
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
static bool content_file_init(struct string_list *temporary_content)
{
   unsigned i;
   struct retro_game_info               *info = NULL;
   bool ret                                   = false;
   struct string_list* additional_path_allocs = NULL;
   struct string_list *content                = NULL;
   const struct retro_subsystem_info *special = NULL;
   rarch_system_info_t *system                = NULL;
   global_t *global                           = global_get_ptr();

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system);

   if (!string_is_empty(global->subsystem))
   {
      special = init_content_file_subsystem(&ret, system);
      if (!ret)
         goto error;
   }

   content = string_list_new();

   if (!content)
      goto error;

   if (!init_content_file_set_attribs(temporary_content,
            content, system, special))
      goto error;

   info                   = (struct retro_game_info*)
      calloc(content->size, sizeof(*info));
   additional_path_allocs = string_list_new();

   ret = load_content(temporary_content,
         info, content, special, additional_path_allocs);

   for (i = 0; i < content->size; i++)
      free((void*)info[i].data);

   string_list_free(additional_path_allocs);
   if (info)
      free(info);

error:
   if (content)
      string_list_free(content);
   return ret;
}

static bool content_file_free(struct string_list *temporary_content)
{
   unsigned i;

   if (!temporary_content)
      return false;

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

   return true;
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
   content_file_free(temporary_content);
   temporary_content          = NULL;
   content_crc                = 0;
   _content_is_inited         = false;
   core_does_not_need_content = false;
}

/* Initializes and loads a content file for the currently
 * selected libretro core. */
bool content_init(void)
{
   temporary_content = string_list_new();
   if (!temporary_content)
      goto error;

   if (!content_file_init(temporary_content))
      goto error;

   _content_is_inited = true;
   return true;

error:
   content_deinit();
   return false;
}

#ifdef HAVE_MENU
static void menu_content_environment_get(int *argc, char *argv[],
      void *args, void *params_data)
{
   char *fullpath                    = NULL;
   struct rarch_main_wrap *wrap_args = (struct rarch_main_wrap*)params_data;

   if (!wrap_args)
      return;

   runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);

   wrap_args->no_content       = menu_driver_ctl(
         RARCH_MENU_CTL_HAS_LOAD_NO_CONTENT, NULL);

   if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_VERBOSITY))
      wrap_args->verbose       = verbosity_is_enabled();

   wrap_args->touched          = true;
   wrap_args->config_path      = NULL;
   wrap_args->sram_path        = NULL;
   wrap_args->state_path       = NULL;
   wrap_args->content_path     = NULL;

   if (!path_is_config_empty())
      wrap_args->config_path   = path_get_config();
   if (!dir_is_savefile_empty())
      wrap_args->sram_path     = dir_get_savefile();
   if (!dir_is_savestate_empty())
      wrap_args->state_path    = dir_get_savestate();
   if (fullpath && *fullpath)
      wrap_args->content_path  = fullpath;
   if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_LIBRETRO))
      wrap_args->libretro_path = string_is_empty(path_get_core()) ? NULL :
         path_get_core();

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
      bool launched_from_menu,
      enum content_mode_load mode)
{
   char name[PATH_MAX_LENGTH] = {0};
   char msg[PATH_MAX_LENGTH]  = {0};
   char *fullpath             = NULL;

   runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);

   if (launched_from_menu)
   {
#ifdef HAVE_MENU
      /* redraw menu frame */
      menu_display_set_msg_force(true);
      menu_driver_ctl(RARCH_MENU_CTL_RENDER, NULL);

      if (!string_is_empty(fullpath))
         fill_pathname_base(name, fullpath, sizeof(name));
#endif

      /** Show loading OSD message */
      if (!string_is_empty(fullpath))
      {
         snprintf(msg, sizeof(msg), "%s %s ...",
               msg_hash_to_str(MSG_LOADING),
               name);
         runloop_msg_queue_push(msg, 2, 1, true);
      }
   }

   if (!content_load(content_info))
      goto error;

   /* Push entry to top of history playlist */
   if (content_is_inited() || content_does_not_need_content())
   {
      char tmp[PATH_MAX_LENGTH]      = {0};
      struct retro_system_info *info = NULL;
      rarch_system_info_t *system    = NULL;

      runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system);
      if (system)
         info = &system->info;

#ifdef HAVE_MENU
      if (launched_from_menu)
         menu_driver_ctl(RARCH_MENU_CTL_SYSTEM_INFO_GET, &info);
#endif

      strlcpy(tmp, fullpath, sizeof(tmp));

      if (!launched_from_menu)
      {
         /* Path can be relative here.
          * Ensure we're pushing absolute path. */
         if (!string_is_empty(tmp))
            path_resolve_realpath(tmp, sizeof(tmp));
      }

      if (info && *tmp)
      {
         const char *core_path            = NULL;
         const char *core_name            = NULL;
         playlist_t *playlist_tmp         = g_defaults.content_history;

         switch (path_is_media_type(tmp))
         {
            case RARCH_CONTENT_MOVIE:
#ifdef HAVE_FFMPEG
               playlist_tmp = g_defaults.video_history;
               core_name = "movieplayer";
               core_path = "builtin";
#endif
               break;
            case RARCH_CONTENT_MUSIC:
#ifdef HAVE_FFMPEG
               playlist_tmp = g_defaults.music_history;
               core_name = "musicplayer";
               core_path = "builtin";
#endif
               break;
            case RARCH_CONTENT_IMAGE:
#ifdef HAVE_IMAGEVIEWER
               playlist_tmp = g_defaults.image_history;
               core_name = "imageviewer";
               core_path = "builtin";
#endif
               break;
            default:
               core_path            = path_get_core();
               core_name            = info->library_name;
               break;
         }

         if (content_push_to_history_playlist(playlist_tmp, tmp,
                  core_name, core_path))
            playlist_write_file(playlist_tmp);
      }
   }

   return true;

error:
   if (launched_from_menu)
   {
      if (!string_is_empty(fullpath) && !string_is_empty(name))
      {
         snprintf(msg, sizeof(msg), "%s %s.\n",
               msg_hash_to_str(MSG_FAILED_TO_LOAD),
               name);
         runloop_msg_queue_push(msg, 2, 90, true);
      }
   }
   return false;
}

static bool command_event_cmd_exec(const char *data,
      enum content_mode_load mode)
{
#if defined(HAVE_DYNAMIC)
   content_ctx_info_t content_info;
#endif
   char *fullpath           = NULL;

#if defined(HAVE_DYNAMIC)
   content_info.argc        = 0;
   content_info.argv        = NULL;
   content_info.args        = NULL;
#ifdef HAVE_MENU
   content_info.environ_get = menu_content_environment_get;
#else
   content_info.environ_get = NULL;
#endif
#endif

   runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);

   if (fullpath != (void*)data)
   {
      runloop_ctl(RUNLOOP_CTL_CLEAR_CONTENT_PATH, NULL);
      if (!string_is_empty(data))
         runloop_ctl(RUNLOOP_CTL_SET_CONTENT_PATH, (void*)data);
   }

#if defined(HAVE_DYNAMIC)
   if (!task_load_content(&content_info, false, mode))
   {
#ifdef HAVE_MENU
      rarch_ctl(RARCH_CTL_MENU_RUNNING, NULL);
#endif
      return false;
   }
#else
   frontend_driver_set_fork(FRONTEND_FORK_CORE_WITH_ARGS);
#endif

   return true;
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
   bool loading_from_menu = false;

   if (!content_info)
      return false;

   /* First we determine if we are loading from a menu */
   switch (mode)
   {
      case CONTENT_MODE_LOAD_NOTHING_WITH_NEW_CORE_FROM_MENU:
#if defined(HAVE_VIDEO_PROCESSOR)
      case CONTENT_MODE_LOAD_NOTHING_WITH_VIDEO_PROCESSOR_CORE_FROM_MENU:
#endif
#if defined(HAVE_NETPLAY) && defined(HAVE_NETWORKGAMEPAD)
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
         runloop_ctl(RUNLOOP_CTL_CLEAR_CONTENT_PATH, NULL);
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
         runloop_ctl(RUNLOOP_CTL_SET_CONTENT_PATH,  (void*)fullpath);
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
         if (!command_event_cmd_exec(fullpath, mode))
            return false;
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
            return false;
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
#if defined(HAVE_NETPLAY) && defined(HAVE_NETWORKGAMEPAD)
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
      case CONTENT_MODE_LOAD_FROM_CLI:
#if defined(HAVE_NETPLAY) && defined(HAVE_NETWORKGAMEPAD)
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
         if (!task_load_content(content_info, loading_from_menu, mode))
            goto error;
         break;
#ifndef HAVE_DYNAMIC
      case CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_MENU:
         {
            char *fullpath       = NULL;

            runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);
            command_event_cmd_exec(fullpath, mode);
            command_event(CMD_EVENT_QUIT, NULL);
         }
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

   return true;

error:
#ifdef HAVE_MENU
   switch (mode)
   {
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
   return false;
}

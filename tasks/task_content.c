/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C) 2016-2019 - Andrés Suárez
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

#ifdef __WINRT__
#include <uwp/uwp_func.h>
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
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <lists/string_list.h>
#include <lists/dir_list.h>
#include <vfs/vfs_implementation.h>

#include <retro_miscellaneous.h>
#include <retro_assert.h>

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#endif

#ifdef HAVE_GFX_WIDGETS
#include "../gfx/gfx_widgets.h"
#endif

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#include "../menu/menu_shader.h"
#endif

#ifdef HAVE_CHEEVOS
#include "../cheevos/cheevos.h"
#endif

#include "task_content.h"
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
#include "../retroarch.h"
#include "../file_path_special.h"
#include "../core.h"
#include "../paths.h"
#include "../verbosity.h"

#ifdef HAVE_DISCORD
#include "../network/discord.h"

/* TODO/FIXME - get rid of this public global */
extern bool discord_is_inited;
#endif

#define MAX_ARGS 32

typedef struct content_stream content_stream_t;
typedef struct content_information_ctx content_information_ctx_t;

struct content_stream
{
   const uint8_t *b;
   size_t c;
   uint32_t a;
   uint32_t crc;
};

struct content_information_ctx
{
   char *name_ips;
   char *name_bps;
   char *name_ups;

   char *valid_extensions;
   char *directory_cache;
   char *directory_system;

   struct string_list *temporary_content;

   struct
   {
      struct retro_subsystem_info *data;
      unsigned size;
   } subsystem;

   bool block_extract;
   bool need_fullpath;
   bool set_supports_no_game_enable;
#ifdef HAVE_PATCH
   bool is_ips_pref;
   bool is_bps_pref;
   bool is_ups_pref;
   bool patch_is_blocked;
#endif
   bool bios_is_missing;
   bool check_firmware_before_loading;
};

/********************************/
/* Content file functions START */
/********************************/

#define CONTENT_FILE_ATTR_RESET(attr) (attr.i = 0)

#define CONTENT_FILE_ATTR_SET_BLOCK_EXTRACT(attr, block_extract) (attr.i |= ((block_extract) ? 1 : 0))
#define CONTENT_FILE_ATTR_SET_NEED_FULLPATH(attr, need_fullpath) (attr.i |= ((need_fullpath) ? 2 : 0))
#define CONTENT_FILE_ATTR_SET_REQUIRED(attr, required)           (attr.i |= ((required)      ? 4 : 0))

#define CONTENT_FILE_ATTR_GET_BLOCK_EXTRACT(attr) ((attr.i & 1) != 0)
#define CONTENT_FILE_ATTR_GET_NEED_FULLPATH(attr) ((attr.i & 2) != 0)
#define CONTENT_FILE_ATTR_GET_REQUIRED(attr)      ((attr.i & 4) != 0)

/**
 * content_file_load_into_memory:
 * @path         : path of the content file.
 * @buf          : buffer into which the content file will be read.
 * @length       : size of the resultant content buffer.
 *
 * Reads the content file into memory. Also performs soft patching
 * (see patch_content function) if soft patching has not been
 * blocked by the user.
 *
 * Returns: true if successful, false on error.
 **/
static bool content_file_load_into_memory(
      content_information_ctx_t *content_ctx,
      content_state_t *p_content,
      struct retro_game_info *info,
      size_t idx, bool content_compressed,
      enum rarch_content_type first_content_type)
{
   struct retro_game_info *content_info = &info[idx];
   const char *content_path             = content_info->path;
   uint8_t *content_data                = NULL;
   int64_t length                       = 0;

   content_info->data = NULL;
   content_info->size = 0;

   RARCH_LOG("[CONTENT LOAD]: %s: %s\n",
         msg_hash_to_str(MSG_LOADING_CONTENT_FILE), content_path);

   /* Read content from file into memory buffer */
#ifdef HAVE_COMPRESSION
   if (content_compressed)
   {
      if (!file_archive_compressed_read(content_path,
            (void**)&content_data, NULL, &length))
         return false;
   }
   else
#endif
      if (!filestream_read_file(content_path,
            (void**)&content_data, &length))
         return false;

   if (length < 0)
      return false;

   /* First content file is significant: attempt to do
    * soft patching, CRC checking, etc. */
   if (idx == 0)
   {
      /* If we have a media type, ignore CRC32 calculation. */
      if (first_content_type == RARCH_CONTENT_NONE)
      {
         bool has_patch = false;

#ifdef HAVE_PATCH
         /* Attempt to apply a patch. */
         if (!content_ctx->patch_is_blocked)
            has_patch = patch_content(
                  content_ctx->is_ips_pref,
                  content_ctx->is_bps_pref,
                  content_ctx->is_ups_pref,
                  content_ctx->name_ips,
                  content_ctx->name_bps,
                  content_ctx->name_ups,
                  (uint8_t**)&content_data,
                  (void*)&length);
#endif
         /* If content is compressed or a patch has been
          * applied, must determine CRC value using the
          * actual data buffer, since the content path
          * cannot be used for this purpose...
          * In all other cases, cache the content path
          * and defer CRC calculation until the value is
          * actually needed */
         if (content_compressed || has_patch)
         {
            p_content->rom_crc = encoding_crc32(0, content_data,
                  (size_t)length);
            RARCH_LOG("[CONTENT LOAD]: CRC32: 0x%x\n",
                  (unsigned)p_content->rom_crc);
         }
         else
         {
            strlcpy(p_content->pending_rom_crc_path, content_path,
                  sizeof(p_content->pending_rom_crc_path));
            p_content->pending_rom_crc = true;
         }
      }
      else
         p_content->rom_crc = 0;
   }

   content_info->data = content_data;
   content_info->size = length;

   return true;
}

#ifdef HAVE_COMPRESSION
static bool content_file_extract_from_archive(
      content_information_ctx_t *content_ctx,
      struct retro_game_info *info,
      size_t idx, const char *valid_exts,
      char **error_string)
{
   struct retro_game_info *content_info = &info[idx];
   const char *content_path             = content_info->path;
   union string_list_elem_attr attr;
   char temp_path[PATH_MAX_LENGTH];
   char msg[1024];

   attr.i       = 0;
   temp_path[0] = '\0';
   msg[0]       = '\0';

   RARCH_LOG("[CONTENT LOAD]: Core requires uncompressed content - "
         "extracting archive to temporary directory.\n");

   /* Attempt to extract file  */
   if (!file_archive_extract_file(
         content_path, valid_exts,
         string_is_empty(content_ctx->directory_cache) ?
               NULL : content_ctx->directory_cache,
         temp_path, sizeof(temp_path)))
   {
      snprintf(msg, sizeof(msg), "%s: %s\n",
            msg_hash_to_str(MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE),
            content_path);
      *error_string = strdup(msg);
      return false;
   }

   /* Add path of extracted file to temporary content
    * list (so it can be deleted when deinitialising
    * the core) */
   if (!string_list_append(content_ctx->temporary_content,
         temp_path, attr))
      return false;

   /* Assign extracted file path to info struct */
   content_info->path = content_ctx->temporary_content->elems[
         content_ctx->temporary_content->size - 1].data;

   RARCH_LOG("[CONTENT LOAD]: Content successfully extracted to: %s\n",
         temp_path);

   return true;
}
#endif

static void content_file_set_info_path(
      struct string_list *content,
      struct retro_game_info *info,
      size_t idx,
      const char *valid_exts,
      bool *path_is_compressed)
{
   const char *content_path             = content->elems[idx].data;
   struct retro_game_info *content_info = &info[idx];
   bool path_is_archive;
   bool path_is_inside_archive;

   content_info->path  = NULL;
   *path_is_compressed = false;

   if (string_is_empty(content_path))
      return;

#ifdef HAVE_COMPRESSION
   /* Check whether we are dealing with a
    * compressed file */
   path_is_archive        = path_is_compressed_file(content_path);
   path_is_inside_archive = path_contains_compressed_file(content_path);
   *path_is_compressed    = path_is_archive || path_is_inside_archive;

   /* If extraction is permitted and content is a
    * 'parent' archive file, must determine which
    * internal file to load
    * > file_archive_compressed_read() requires
    *   a 'complete' file path:
    *   <parent_archive>#<internal_file> */
   if (!CONTENT_FILE_ATTR_GET_BLOCK_EXTRACT(content->elems[idx].attr) &&
       path_is_archive &&
       !path_is_inside_archive)
   {
      /* Get internal archive file list */
      struct string_list *archive_list =
            file_archive_get_file_list(content_path, valid_exts);

      if (archive_list &&
          (archive_list->size > 0))
      {
         const char *archive_file = NULL;

         /* Ensure that list is sorted alphabetically */
         if (archive_list->size > 1)
            dir_list_sort(archive_list, true);

         archive_file = archive_list->elems[0].data;

         if (!string_is_empty(archive_file))
         {
            char info_path[PATH_MAX_LENGTH];
            info_path[0] = '\n';

            /* Build 'complete' archive file path */
            snprintf(info_path, sizeof(info_path), "%s#%s",
                  content_path, archive_file);

            /* Update 'content' string_list */
            string_list_set(content, idx, info_path);
            content_path = content->elems[idx].data;

            string_list_free(archive_list);
         }
      }
   }
#endif

   content_info->path = content_path;
}

/**
 * content_file_load:
 * @special          : subsystem of content to be loaded. Can be NULL.
 *
 * Load content file (for libretro core).
 *
 * Returns : true if successful, otherwise false.
 **/
static bool content_file_load(
      struct retro_game_info *info,
      content_state_t *p_content,
      struct string_list *content,
      content_information_ctx_t *content_ctx,
      enum msg_hash_enums *error_enum,
      char **error_string,
      const struct retro_subsystem_info *special)
{
   size_t i;
   char msg[1024];
   retro_ctx_load_content_info_t load_info;
   bool used_vfs_fallback_copy                = false;
#ifdef __WINRT__
   rarch_system_info_t *system                = runloop_get_system_info();
#endif
   enum rarch_content_type first_content_type = RARCH_CONTENT_NONE;

   msg[0] = '\0';

   for (i = 0; i < content->size; i++)
   {
      const char *valid_exts  = special ?
            special->roms[i].valid_extensions :
                  content_ctx->valid_extensions;
      bool content_compressed = false;

      /* Get content path (note that this is always
       * assigned to the info struct, regardless of
       * whether the core requests it) */
      content_file_set_info_path(content, info, i,
            valid_exts, &content_compressed);

      /* If content is missing and core requires content,
       * return an error */
      if (string_is_empty(info[i].path))
      {
         if (CONTENT_FILE_ATTR_GET_REQUIRED(content->elems[i].attr))
         {
            *error_enum = MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT;
            return false;
         }
      }
      else
      {
         /* If this is the first item of content,
          * get content type */
         if (i == 0)
            first_content_type = path_is_media_type(info[i].path);

         /* If core does not require 'fullpath', load
          * the content into memory */
         if (!CONTENT_FILE_ATTR_GET_NEED_FULLPATH(content->elems[i].attr))
         {
            if (!content_file_load_into_memory(
                  content_ctx, p_content, info, i,
                  content_compressed, first_content_type))
            {
               snprintf(msg, sizeof(msg), "%s \"%s\".\n",
                     msg_hash_to_str(MSG_COULD_NOT_READ_CONTENT_FILE),
                     info[i].path);
               *error_string = strdup(msg);
               return false;
            }
         }
         else
         {
#ifdef HAVE_COMPRESSION
            /* If this is compressed content and need_fullpath
             * is true, extract it to a temporary file */
            if (content_compressed &&
                !CONTENT_FILE_ATTR_GET_BLOCK_EXTRACT(content->elems[i].attr) &&
                !content_file_extract_from_archive(content_ctx,
                     info, i, valid_exts, error_string))
               return false;
#endif
#ifdef __WINRT__
            /* TODO: When support for the 'actual' VFS is added,
             * there will need to be some more logic here */
            if (!system->supports_vfs &&
                !is_path_accessible_using_standard_io(info[i].path))
            {
               /* Fallback to a file copy into an accessible directory */
               char *buf;
               int64_t len;
               union string_list_elem_attr attr;
               char new_basedir[PATH_MAX_LENGTH];
               char new_path[PATH_MAX_LENGTH];

               new_path[0]    = '\0';
               new_basedir[0] = '\0';
               attr.i         = 0;

               RARCH_LOG("[CONTENT LOAD]: Core does not support VFS"
                     " - copying to cache directory\n");

               if (!string_is_empty(content_ctx->directory_cache))
                  strlcpy(new_basedir, content_ctx->directory_cache,
                        sizeof(new_basedir));

               if (string_is_empty(new_basedir) ||
                   !path_is_directory(new_basedir) ||
                  !is_path_accessible_using_standard_io(new_basedir))
               {
                  RARCH_WARN("[CONTENT LOAD]: Tried copying to cache directory, "
                        "but cache directory was not set or found. "
                        "Setting cache directory to root of writable app directory...\n");
                  strlcpy(new_basedir, uwp_dir_data, sizeof(new_basedir));
               }

               fill_pathname_join(new_path, new_basedir,
                     path_basename(info[i].path), sizeof(new_path));

               /* TODO: This may fail on very large files...
                * but copying large files is not a good idea anyway */
               if (!filestream_read_file(info[i].path, &buf, &len))
               {
                  snprintf(msg, sizeof(msg), "%s \"%s\". (during copy read)\n",
                        msg_hash_to_str(MSG_COULD_NOT_READ_CONTENT_FILE),
                        info[i].path);
                  *error_string = strdup(msg);
                  return false;
               }

               if (!filestream_write_file(new_path, buf, len))
               {
                  free(buf);
                  snprintf(msg, sizeof(msg), "%s \"%s\". (during copy write)\n",
                        msg_hash_to_str(MSG_COULD_NOT_READ_CONTENT_FILE),
                        info[i].path);
                  *error_string = strdup(msg);
                  return false;
               }

               free(buf);

               string_list_append(content_ctx->temporary_content,
                     new_path, attr);

               info[i].path = content_ctx->temporary_content->elems[
                     content_ctx->temporary_content->size - 1].data;

               used_vfs_fallback_copy = true;
            }
#endif
            RARCH_LOG("[CONTENT LOAD]: %s\n", msg_hash_to_str(
                  MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT));

            /* First content file is significant: need to
             * perform CRC calculation, but defer this
             * until value is used */
            if (i == 0)
            {
               /* If we have a media type, ignore CRC32 calculation. */
               if (first_content_type == RARCH_CONTENT_NONE)
               {
                  strlcpy(p_content->pending_rom_crc_path, info[i].path,
                        sizeof(p_content->pending_rom_crc_path));
                  p_content->pending_rom_crc = true;
               }
               else
                  p_content->rom_crc = 0;
            }
         }
      }
   }

   /* Load content into core */
   load_info.content = content;
   load_info.special = special;
   load_info.info    = info;

   if (!core_load_game(&load_info))
   {
      /* This is probably going to fail on multifile ROMs etc.
       * so give a visible explanation of what is likely wrong */
      if (used_vfs_fallback_copy)
         *error_enum = MSG_ERROR_LIBRETRO_CORE_REQUIRES_VFS;
      else
         *error_enum = MSG_FAILED_TO_LOAD_CONTENT;

      return false;
   }

#ifdef HAVE_CHEEVOS
   if (!special)
   {
      const char *first_content_path = info[0].path;
      if (!string_is_empty(first_content_path))
      {
         if (first_content_type == RARCH_CONTENT_NONE)
         {
            rcheevos_load(info);
            return true;
         }
      }
   }
   rcheevos_pause_hardcore();
#endif

   return true;
}

static const struct retro_subsystem_info *content_file_init_subsystem(
      const struct retro_subsystem_info *subsystem_data,
      size_t subsystem_current_count,
      enum msg_hash_enums *error_enum,
      char **error_string,
      bool *ret)
{
   struct string_list *subsystem              = path_get_subsystem_list();
   const struct retro_subsystem_info *special = libretro_find_subsystem_info(
            subsystem_data, (unsigned)subsystem_current_count,
            path_get(RARCH_PATH_SUBSYSTEM));
   char msg[1024];

   msg[0] = '\0';

   if (!special)
   {
      snprintf(msg, sizeof(msg),
            "Failed to find subsystem \"%s\" in libretro implementation.\n",
            path_get(RARCH_PATH_SUBSYSTEM));
      *error_string = strdup(msg);
      goto error;
   }

   if (special->num_roms)
   {
      if (!subsystem)
      {
         *error_enum = MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT;
         goto error;
      }

      if (special->num_roms != subsystem->size)
      {
         snprintf(msg, sizeof(msg),
               "Libretro core requires %u content files for "
               "subsystem \"%s\", but %u content files were provided.\n",
               special->num_roms, special->desc,
               (unsigned)subsystem->size);
         *error_string = strdup(msg);
         goto error;
      }
   }
   else if (subsystem && subsystem->size)
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

static void content_file_set_attributes(
      struct string_list *content,
      const struct retro_subsystem_info *special,
      content_information_ctx_t *content_ctx,
      char **error_string)
{
   struct string_list *subsystem = path_get_subsystem_list();

   if (!path_is_empty(RARCH_PATH_SUBSYSTEM) && special)
   {
      size_t i;

      for (i = 0; i < subsystem->size; i++)
      {
         union string_list_elem_attr attr;

         CONTENT_FILE_ATTR_RESET(attr);
         CONTENT_FILE_ATTR_SET_BLOCK_EXTRACT(attr, special->roms[i].block_extract);
         CONTENT_FILE_ATTR_SET_NEED_FULLPATH(attr, special->roms[i].need_fullpath);
         CONTENT_FILE_ATTR_SET_REQUIRED(attr, special->roms[i].required);

         string_list_append(content, subsystem->elems[i].data, attr);
      }
   }
   else
   {
      const char *content_path = path_get(RARCH_PATH_CONTENT);
      bool contentless         = false;
      bool is_inited           = false;
      union string_list_elem_attr attr;

      content_get_status(&contentless, &is_inited);

      CONTENT_FILE_ATTR_RESET(attr);
      CONTENT_FILE_ATTR_SET_BLOCK_EXTRACT(attr, content_ctx->block_extract);
      CONTENT_FILE_ATTR_SET_NEED_FULLPATH(attr, content_ctx->need_fullpath);
      CONTENT_FILE_ATTR_SET_REQUIRED(attr, !contentless);

      if (string_is_empty(content_path))
      {
         if (contentless &&
             content_ctx->set_supports_no_game_enable)
            string_list_append(content, "", attr);
      }
      else
         string_list_append(content, content_path, attr);
   }
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
      content_state_t *p_content,
      struct string_list *content,
      enum msg_hash_enums *error_enum,
      char **error_string)
{
   struct retro_game_info *info               = NULL;
   bool subsystem_path_is_empty               = path_is_empty(RARCH_PATH_SUBSYSTEM);
   bool ret                                   = subsystem_path_is_empty;
   const struct retro_subsystem_info *special = subsystem_path_is_empty ?
         NULL : content_file_init_subsystem(content_ctx->subsystem.data,
               content_ctx->subsystem.size, error_enum, error_string, &ret);

   if (!ret)
      return false;

   content_file_set_attributes(content, special, content_ctx, error_string);

   if (content->size > 0)
      info = (struct retro_game_info*)calloc(content->size, sizeof(*info));

   if (info)
   {
      size_t i;

      ret = content_file_load(info, p_content, content,
            content_ctx, error_enum, error_string, special);

      for (i = 0; i < content->size; i++)
         free((void*)info[i].data);

      free(info);
   }
   else if (!special)
   {
      *error_enum = MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT;
      ret         = false;
   }

   return ret;
}

/******************************/
/* Content file functions END */
/******************************/

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
      int *argc, char **argv,
      bool print_args)
{
   *argc = 0;
   argv[(*argc)++] = strdup("retroarch");

   if (args->content_path)
   {
      RARCH_LOG("[CORE]: Using content: %s.\n", args->content_path);
      argv[(*argc)++] = strdup(args->content_path);
   }
#ifdef HAVE_MENU
   else
   {
      RARCH_LOG("[CORE]: %s\n",
            msg_hash_to_str(MSG_NO_CONTENT_STARTING_DUMMY_CORE));
      argv[(*argc)++] = strdup("--menu");
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

   if (print_args)
   {
      int i;
      for (i = 0; i < *argc; i++)
         RARCH_LOG("[CORE]: Arg #%d: %s\n", i, argv[i]);
   }
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
static bool content_load(content_ctx_info_t *info,
      content_state_t *p_content)
{
   unsigned i                        = 0;
   bool success                      = false;
   int rarch_argc                    = 0;
   char *rarch_argv[MAX_ARGS]        = {NULL};
   char *argv_copy [MAX_ARGS]        = {NULL};
   char **rarch_argv_ptr             = (char**)info->argv;
   int *rarch_argc_ptr               = (int*)&info->argc;
   struct rarch_main_wrap *wrap_args = NULL;

   if (!(wrap_args = (struct rarch_main_wrap*)
      malloc(sizeof(*wrap_args))))
      return false;

   retro_assert(wrap_args);

   wrap_args->argv           = NULL;
   wrap_args->content_path   = NULL;
   wrap_args->sram_path      = NULL;
   wrap_args->state_path     = NULL;
   wrap_args->config_path    = NULL;
   wrap_args->libretro_path  = NULL;
   wrap_args->verbose        = false;
   wrap_args->no_content     = false;
   wrap_args->touched        = false;
   wrap_args->argc           = 0;

   if (info->environ_get)
      info->environ_get(rarch_argc_ptr,
            rarch_argv_ptr, info->args, wrap_args);

   if (wrap_args->touched)
   {
      content_load_init_wrap(wrap_args, &rarch_argc, rarch_argv,
            false);
      memcpy(argv_copy, rarch_argv, sizeof(rarch_argv));
      rarch_argv_ptr = (char**)rarch_argv;
      rarch_argc_ptr = (int*)&rarch_argc;
   }

   rarch_ctl(RARCH_CTL_MAIN_DEINIT, NULL);

   wrap_args->argc = *rarch_argc_ptr;
   wrap_args->argv = rarch_argv_ptr;

   success         = retroarch_main_init(wrap_args->argc, wrap_args->argv);

   for (i = 0; i < ARRAY_SIZE(argv_copy); i++)
      free(argv_copy[i]);
   free(wrap_args);

   if (!success)
      return false;

   if (p_content->pending_subsystem_init)
   {
      command_event(CMD_EVENT_CORE_INIT, NULL);
      content_clear_subsystem();
   }

#ifdef HAVE_GFX_WIDGETS
   /* If retroarch_main_init() returned true, we
    * can safely trigger a load content animation */
   if (gfx_widgets_ready())
   {
      /* Note: Have to read settings value here
       * (It will be invalid if we try to read
       *  it earlier...) */
#ifdef HAVE_CONFIGFILE
      settings_t *settings              = config_get_ptr();
      bool show_load_content_animation  = settings && settings->bools.menu_show_load_content_animation;
#else
      bool show_load_content_animation  = false;
#endif

      if (show_load_content_animation)
         gfx_widget_start_load_content_animation();
   }
#endif

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   menu_shader_manager_init();
#endif

   command_event(CMD_EVENT_HISTORY_INIT, NULL);
   rarch_favorites_init();
   command_event(CMD_EVENT_RESUME, NULL);
   command_event(CMD_EVENT_VIDEO_SET_ASPECT_RATIO, NULL);

   frontend_driver_process_args(rarch_argc_ptr, rarch_argv_ptr);
   frontend_driver_content_loaded();

   return true;
}

void menu_content_environment_get(int *argc, char *argv[],
      void *args, void *params_data);

/**
 * task_push_to_history_list:
 *
 * Will push the content entry to the history playlist.
 **/
static void task_push_to_history_list(
      content_state_t *p_content,
      bool launched_from_menu,
      bool launched_from_cli,
      bool launched_from_companion_ui)
{
   bool            contentless = false;
   bool            is_inited   = false;

   content_get_status(&contentless, &is_inited);

   /* Push entry to top of history playlist */
   if (is_inited || contentless)
   {
      char tmp[PATH_MAX_LENGTH];
      const char *path_content       = path_get(RARCH_PATH_CONTENT);
      struct retro_system_info *info = runloop_get_libretro_system_info();

      tmp[0] = '\0';

      if (!string_is_empty(path_content))
         strlcpy(tmp, path_content, sizeof(tmp));

      /* Path can be relative here.
       * Ensure we're pushing absolute path. */
      if (!launched_from_menu && !string_is_empty(tmp))
         path_resolve_realpath(tmp, sizeof(tmp), true);

#ifdef HAVE_MENU
      /* Push quick menu onto menu stack */
      if (launched_from_cli)
         menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, NULL);
#endif

      if (info && !string_is_empty(tmp))
      {
         const char *core_path      = NULL;
         const char *core_name      = NULL;
         const char *label          = NULL;
         const char *crc32          = NULL;
         const char *db_name        = NULL;
         playlist_t *playlist_hist  = g_defaults.content_history;
         settings_t *settings       = config_get_ptr();
         global_t *global           = global_get_ptr();

         switch (path_is_media_type(tmp))
         {
            case RARCH_CONTENT_MOVIE:
#ifdef HAVE_FFMPEG
               playlist_hist        = g_defaults.video_history;
               core_name            = "movieplayer";
               core_path            = "builtin";
#endif
               break;
            case RARCH_CONTENT_MUSIC:
               playlist_hist        = g_defaults.music_history;
               core_name            = "musicplayer";
               core_path            = "builtin";
               break;
            case RARCH_CONTENT_IMAGE:
#ifdef HAVE_IMAGEVIEWER
               playlist_hist        = g_defaults.image_history;
               core_name            = "imageviewer";
               core_path            = "builtin";
#endif
               break;
            default:
            {
               core_info_t *core_info = NULL;
               /* Set core display name
                * (As far as I can tell, core_info_get_current_core()
                * should always provide a valid pointer here...) */
               core_info_get_current_core(&core_info);

               /* Set core path */
               core_path            = path_get(RARCH_PATH_CORE);

               if (core_info)
                  core_name         = core_info->display_name;

               if (string_is_empty(core_name))
                  core_name         = info->library_name;

               if (launched_from_companion_ui)
               {
                  /* Database name + checksum are supplied
                   * by the companion UI itself */
                  if (!string_is_empty(p_content->companion_ui_crc32))
                     crc32 = p_content->companion_ui_crc32;

                  if (!string_is_empty(p_content->companion_ui_db_name))
                     db_name = p_content->companion_ui_db_name;
               }
#ifdef HAVE_MENU
               else
               {
                  menu_handle_t *menu = menu_driver_get_ptr();
                  /* Set database name + checksum */
                  if (menu)
                  {
                     playlist_t *playlist_curr = playlist_get_cached();

                     if (playlist_index_is_valid(playlist_curr, menu->rpl_entry_selection_ptr, tmp, core_path))
                     {
                        playlist_get_crc32(playlist_curr, menu->rpl_entry_selection_ptr,   &crc32);
                        playlist_get_db_name(playlist_curr, menu->rpl_entry_selection_ptr, &db_name);
                     }
                  }
               }
#endif
               break;
            }
         }

         if (global && !string_is_empty(global->name.label))
            label = global->name.label;

         if (
              settings && settings->bools.history_list_enable 
               && playlist_hist)
         {
            char subsystem_name[PATH_MAX_LENGTH];
            struct playlist_entry entry = {0};

            subsystem_name[0] = '\0';

            content_get_subsystem_friendly_name(path_get(RARCH_PATH_SUBSYSTEM), subsystem_name, sizeof(subsystem_name));

            /* The push function reads our entry as const, 
             * so these casts are safe */
            entry.path            = (char*)tmp;
            entry.label           = (char*)label;
            entry.core_path       = (char*)core_path;
            entry.core_name       = (char*)core_name;
            entry.crc32           = (char*)crc32;
            entry.db_name         = (char*)db_name;
            entry.subsystem_ident = (char*)path_get(RARCH_PATH_SUBSYSTEM);
            entry.subsystem_name  = (char*)subsystem_name;
            entry.subsystem_roms  = (struct string_list*)path_get_subsystem_list();

            command_playlist_push_write(playlist_hist, &entry);
         }
      }
   }
}

#ifdef HAVE_MENU
static bool command_event_cmd_exec(
      content_state_t *p_content,
      const char *data,
      content_information_ctx_t *content_ctx,
      bool launched_from_cli,
      char **error_string)
{
   if (path_get(RARCH_PATH_CONTENT) != data)
   {
      path_clear(RARCH_PATH_CONTENT);
      if (!string_is_empty(data))
         path_set(RARCH_PATH_CONTENT, data);
   }

#if defined(HAVE_DYNAMIC)
   {
      content_ctx_info_t content_info;

      content_info.argc        = 0;
      content_info.argv        = NULL;
      content_info.args        = NULL;
      content_info.environ_get = menu_content_environment_get;

      /* Loads content into currently selected core. */
      if (!content_load(&content_info, p_content))
         return false;
      task_push_to_history_list(p_content, true, launched_from_cli, false);
   }
#else
   frontend_driver_set_fork(FRONTEND_FORK_CORE_WITH_ARGS);
#endif

   return true;
}
#endif

static bool firmware_update_status(
      content_information_ctx_t *content_ctx)
{
   char s[PATH_MAX_LENGTH];
   core_info_ctx_firmware_t firmware_info;
   bool set_missing_firmware  = false;
   core_info_t *core_info     = NULL;
   
   core_info_get_current_core(&core_info);

   if (!core_info)
      return false;

   s[0]                       = '\0';
   firmware_info.path         = core_info->path;

   if (!string_is_empty(content_ctx->directory_system))
      firmware_info.directory.system = content_ctx->directory_system;
   else
   {
      strlcpy(s, path_get(RARCH_PATH_CONTENT), sizeof(s));
      path_basedir_wrapper(s);
      firmware_info.directory.system = s;
   }

   RARCH_LOG("[CONTENT LOAD]: Updating firmware status for: %s on %s\n",
         core_info->path,
         firmware_info.directory.system);

   rarch_ctl(RARCH_CTL_UNSET_MISSING_BIOS, NULL);

   core_info_list_update_missing_firmware(&firmware_info,
         &set_missing_firmware);

   if (set_missing_firmware)
      rarch_ctl(RARCH_CTL_SET_MISSING_BIOS, NULL);

   if (
         content_ctx->bios_is_missing &&
         content_ctx->check_firmware_before_loading)
   {
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_FIRMWARE),
            100, 500, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_LOG("[CONTENT LOAD]: Load content blocked. Reason: %s\n",
            msg_hash_to_str(MSG_FIRMWARE));

      return true;
   }

   return false;
}

bool task_push_start_dummy_core(content_ctx_info_t *content_info)
{
   content_information_ctx_t content_ctx;
   content_state_t                 *p_content = content_state_get_ptr();
   bool ret                                   = true;
   char *error_string                         = NULL;
   global_t *global                           = global_get_ptr();
   settings_t *settings                       = config_get_ptr();
   rarch_system_info_t *sys_info              = runloop_get_system_info();
   const char *path_dir_system                = settings->paths.directory_system;
   bool check_firmware_before_loading         = settings->bools.check_firmware_before_loading;

   if (!content_info)
      return false;

   content_ctx.check_firmware_before_loading  = check_firmware_before_loading;
#ifdef HAVE_PATCH
   content_ctx.is_ips_pref                    = rarch_ctl(RARCH_CTL_IS_IPS_PREF, NULL);
   content_ctx.is_bps_pref                    = rarch_ctl(RARCH_CTL_IS_BPS_PREF, NULL);
   content_ctx.is_ups_pref                    = rarch_ctl(RARCH_CTL_IS_UPS_PREF, NULL);
   content_ctx.patch_is_blocked               = rarch_ctl(RARCH_CTL_IS_PATCH_BLOCKED, NULL);
#endif
   content_ctx.bios_is_missing                = rarch_ctl(RARCH_CTL_IS_MISSING_BIOS, NULL);
   content_ctx.directory_system               = NULL;
   content_ctx.directory_cache                = NULL;
   content_ctx.name_ips                       = NULL;
   content_ctx.name_bps                       = NULL;
   content_ctx.name_ups                       = NULL;
   content_ctx.valid_extensions               = NULL;
   content_ctx.block_extract                  = false;
   content_ctx.need_fullpath                  = false;
   content_ctx.set_supports_no_game_enable    = false;

   content_ctx.subsystem.data                 = NULL;
   content_ctx.subsystem.size                 = 0;

   if (global)
   {
      if (!string_is_empty(global->name.ips))
         content_ctx.name_ips                 = strdup(global->name.ips);
      if (!string_is_empty(global->name.bps))
         content_ctx.name_bps                 = strdup(global->name.bps);
      if (!string_is_empty(global->name.ups))
         content_ctx.name_ups                 = strdup(global->name.ups);
   }

   if (!string_is_empty(path_dir_system))
      content_ctx.directory_system            = strdup(path_dir_system);

   if (!content_info->environ_get)
      content_info->environ_get = menu_content_environment_get;

   /* Clear content path */
   path_clear(RARCH_PATH_CONTENT);

   /* Preliminary stuff that has to be done before we
    * load the actual content. Can differ per mode. */
   sys_info->load_no_content = false;
   rarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   task_queue_deinit();
   retroarch_init_task_queue();

   /* Loads content into currently selected core. */
   if ((ret = content_load(content_info, p_content)))
      task_push_to_history_list(p_content, false, false, false);

   /* Handle load content failure */
   if (!ret)
   {
      if (error_string)
      {
         runloop_msg_queue_push(error_string, 2, 90, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         RARCH_ERR("[CONTENT LOAD]: %s\n", error_string);
         free(error_string);
      }
   }

   if (content_ctx.name_ips)
      free(content_ctx.name_ips);
   if (content_ctx.name_bps)
      free(content_ctx.name_bps);
   if (content_ctx.name_ups)
      free(content_ctx.name_ups);
   if (content_ctx.directory_system)
      free(content_ctx.directory_system);

   return ret;
}

#ifdef HAVE_MENU

bool task_push_load_content_from_playlist_from_menu(
      const char *core_path,
      const char *fullpath,
      const char *label,
      content_ctx_info_t *content_info,
      retro_task_callback_t cb,
      void *user_data)
{
   content_information_ctx_t content_ctx;

   content_state_t                 *p_content = content_state_get_ptr();
   bool ret                                   = true;
   char *error_string                         = NULL;
   global_t *global                           = global_get_ptr();
   settings_t *settings                       = config_get_ptr();
   rarch_system_info_t *sys_info              = runloop_get_system_info();
   const char *path_dir_system                = settings->paths.directory_system;
#ifndef HAVE_DYNAMIC
   bool force_core_reload                     = settings->bools.always_reload_core_on_run_content;
#endif

   content_ctx.check_firmware_before_loading  = settings->bools.check_firmware_before_loading;
#ifdef HAVE_PATCH
   content_ctx.is_ips_pref                    = rarch_ctl(RARCH_CTL_IS_IPS_PREF, NULL);
   content_ctx.is_bps_pref                    = rarch_ctl(RARCH_CTL_IS_BPS_PREF, NULL);
   content_ctx.is_ups_pref                    = rarch_ctl(RARCH_CTL_IS_UPS_PREF, NULL);
   content_ctx.patch_is_blocked               = rarch_ctl(RARCH_CTL_IS_PATCH_BLOCKED, NULL);
#endif
   content_ctx.bios_is_missing                = rarch_ctl(RARCH_CTL_IS_MISSING_BIOS, NULL);
   content_ctx.directory_system               = NULL;
   content_ctx.directory_cache                = NULL;
   content_ctx.name_ips                       = NULL;
   content_ctx.name_bps                       = NULL;
   content_ctx.name_ups                       = NULL;
   content_ctx.valid_extensions               = NULL;
   content_ctx.block_extract                  = false;
   content_ctx.need_fullpath                  = false;
   content_ctx.set_supports_no_game_enable    = false;

   content_ctx.subsystem.data                 = NULL;
   content_ctx.subsystem.size                 = 0;

   if (global)
   {
      if (!string_is_empty(global->name.ips))
         content_ctx.name_ips                 = strdup(global->name.ips);
      if (!string_is_empty(global->name.bps))
         content_ctx.name_bps                 = strdup(global->name.bps);
      if (!string_is_empty(global->name.ups))
         content_ctx.name_ups                 = strdup(global->name.ups);
      if (label)
         strlcpy(global->name.label, label, sizeof(global->name.label));
      else
         global->name.label[0] = '\0';
   }

   if (!string_is_empty(path_dir_system))
      content_ctx.directory_system            = strdup(path_dir_system);

   /* Is content required by this core? */
   if (fullpath)
      sys_info->load_no_content = false;
   else
      sys_info->load_no_content = true;

#ifndef HAVE_DYNAMIC
   /* Check whether specified core is already loaded
    * > If so, content can be launched directly with
    *   the currently loaded core */
   if (!force_core_reload &&
       rarch_ctl(RARCH_CTL_IS_CORE_LOADED, (void*)core_path))
   {
      if (!content_info->environ_get)
         content_info->environ_get = menu_content_environment_get;

      /* Register content path */
      path_clear(RARCH_PATH_CONTENT);
      if (!string_is_empty(fullpath))
         path_set(RARCH_PATH_CONTENT, fullpath);

      /* Load content */
      ret = content_load(content_info, p_content);

      if (!ret)
         goto end;

      /* Update content history */
      task_push_to_history_list(p_content, true, false, false);

      goto end;
   }
#endif

   /* Specified core is not loaded
    * > Load it */
   path_set(RARCH_PATH_CORE, core_path);
#ifdef HAVE_DYNAMIC
   command_event(CMD_EVENT_LOAD_CORE, NULL);
#endif

   /* Load content
    * > On targets that do not support dynamic core loading,
    *   command_event_cmd_exec() will fork a new instance */
   if (!(ret = command_event_cmd_exec(p_content,
         fullpath, &content_ctx, false, &error_string)))
      goto end;

#ifdef HAVE_COCOATOUCH
   /* This seems to be needed for iOS for some reason
    * to show the quick menu after the menu is shown */
   menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, NULL);
#endif

#ifndef HAVE_DYNAMIC
   /* No dynamic core loading support: if we reach
    * this point then a new instance has been
    * forked - have to shut down this one */
   rarch_ctl(RARCH_CTL_SET_SHUTDOWN, NULL);
   retroarch_menu_running_finished(true);
#endif

end:
   /* Handle load content failure */
   if (!ret)
   {
      if (error_string)
      {
         runloop_msg_queue_push(error_string, 2, 90, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         RARCH_ERR("[CONTENT LOAD]: %s\n", error_string);
         free(error_string);
      }

      retroarch_menu_running();
   }

   if (content_ctx.name_ips)
      free(content_ctx.name_ips);
   if (content_ctx.name_bps)
      free(content_ctx.name_bps);
   if (content_ctx.name_ups)
      free(content_ctx.name_ups);
   if (content_ctx.directory_system)
      free(content_ctx.directory_system);

   return ret;
}
#endif

bool task_push_start_current_core(content_ctx_info_t *content_info)
{
   content_information_ctx_t content_ctx;

   content_state_t                 *p_content = content_state_get_ptr();
   bool ret                                   = true;
   char *error_string                         = NULL;
   global_t *global                           = global_get_ptr();
   settings_t *settings                       = config_get_ptr();
   const char *path_dir_system                = settings->paths.directory_system;
   bool check_firmware_before_loading         = settings->bools.check_firmware_before_loading;

   if (!content_info)
      return false;

   content_ctx.check_firmware_before_loading  = check_firmware_before_loading;
#ifdef HAVE_PATCH
   content_ctx.is_ips_pref                    = rarch_ctl(RARCH_CTL_IS_IPS_PREF, NULL);
   content_ctx.is_bps_pref                    = rarch_ctl(RARCH_CTL_IS_BPS_PREF, NULL);
   content_ctx.is_ups_pref                    = rarch_ctl(RARCH_CTL_IS_UPS_PREF, NULL);
   content_ctx.patch_is_blocked               = rarch_ctl(RARCH_CTL_IS_PATCH_BLOCKED, NULL);
#endif
   content_ctx.bios_is_missing                = rarch_ctl(RARCH_CTL_IS_MISSING_BIOS, NULL);
   content_ctx.directory_system               = NULL;
   content_ctx.directory_cache                = NULL;
   content_ctx.name_ips                       = NULL;
   content_ctx.name_bps                       = NULL;
   content_ctx.name_ups                       = NULL;
   content_ctx.valid_extensions               = NULL;
   content_ctx.block_extract                  = false;
   content_ctx.need_fullpath                  = false;
   content_ctx.set_supports_no_game_enable    = false;

   content_ctx.subsystem.data                 = NULL;
   content_ctx.subsystem.size                 = 0;

   if (global)
   {
      if (!string_is_empty(global->name.ips))
         content_ctx.name_ips                 = strdup(global->name.ips);
      if (!string_is_empty(global->name.bps))
         content_ctx.name_bps                 = strdup(global->name.bps);
      if (!string_is_empty(global->name.ups))
         content_ctx.name_ups                 = strdup(global->name.ups);
   }

   if (!string_is_empty(path_dir_system))
      content_ctx.directory_system            = strdup(path_dir_system);

   if (!content_info->environ_get)
      content_info->environ_get = menu_content_environment_get;

   /* Clear content path */
   path_clear(RARCH_PATH_CONTENT);

   /* Preliminary stuff that has to be done before we
    * load the actual content. Can differ per mode. */
   retroarch_set_current_core_type(CORE_TYPE_PLAIN, true);

   /* Load content */
   if (firmware_update_status(&content_ctx))
      goto end;

   /* Loads content into currently selected core. */
   if (!(ret = content_load(content_info, p_content)))
   {
      if (error_string)
      {
         runloop_msg_queue_push(error_string, 2, 90, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         RARCH_ERR("[CONTENT LOAD]: %s\n", error_string);
         free(error_string);
      }

      retroarch_menu_running();
      goto end;
   }

   task_push_to_history_list(p_content, true, false, false);

#ifdef HAVE_MENU
   /* Push quick menu onto menu stack */
   menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, NULL);
#endif

end:
   if (content_ctx.name_ips)
      free(content_ctx.name_ips);
   if (content_ctx.name_bps)
      free(content_ctx.name_bps);
   if (content_ctx.name_ups)
      free(content_ctx.name_ups);
   if (content_ctx.directory_system)
      free(content_ctx.directory_system);

   return ret;
}

bool task_push_load_new_core(
      const char *core_path,
      const char *fullpath,
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data)
{
   path_set(RARCH_PATH_CORE, core_path);

   /* Load core */
   command_event(CMD_EVENT_LOAD_CORE, NULL);

#ifndef HAVE_DYNAMIC
   /* Fork core? */
   if (!frontend_driver_set_fork(FRONTEND_FORK_CORE))
      return false;
#endif

   /* Preliminary stuff that has to be done before we
    * load the actual content. Can differ per mode. */
   retroarch_set_current_core_type(type, true);

   return true;
}

#ifdef HAVE_MENU
bool task_push_load_content_with_new_core_from_menu(
      const char *core_path,
      const char *fullpath,
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data)
{
   content_information_ctx_t content_ctx;

   content_state_t                 *p_content = content_state_get_ptr();
   bool ret                                   = true;
   char *error_string                         = NULL;
   global_t *global                           = global_get_ptr();
   settings_t *settings                       = config_get_ptr();
   bool check_firmware_before_loading         = settings->bools.check_firmware_before_loading;
   const char *path_dir_system                = settings->paths.directory_system;
#ifndef HAVE_DYNAMIC
   bool force_core_reload                     = settings->bools.always_reload_core_on_run_content;

   /* Check whether specified core is already loaded
    * > If so, we can skip loading the core and
    *   just load the content directly */
   if (!force_core_reload &&
       (type == CORE_TYPE_PLAIN) &&
       rarch_ctl(RARCH_CTL_IS_CORE_LOADED, (void*)core_path))
      return task_push_load_content_with_core_from_menu(
            fullpath, content_info,
            type, cb, user_data);
#endif

   content_ctx.check_firmware_before_loading  = check_firmware_before_loading;
#ifdef HAVE_PATCH
   content_ctx.is_ips_pref                    = rarch_ctl(RARCH_CTL_IS_IPS_PREF, NULL);
   content_ctx.is_bps_pref                    = rarch_ctl(RARCH_CTL_IS_BPS_PREF, NULL);
   content_ctx.is_ups_pref                    = rarch_ctl(RARCH_CTL_IS_UPS_PREF, NULL);
   content_ctx.patch_is_blocked               = rarch_ctl(RARCH_CTL_IS_PATCH_BLOCKED, NULL);
#endif
   content_ctx.bios_is_missing                = rarch_ctl(RARCH_CTL_IS_MISSING_BIOS, NULL);
   content_ctx.directory_system               = NULL;
   content_ctx.directory_cache                = NULL;
   content_ctx.name_ips                       = NULL;
   content_ctx.name_bps                       = NULL;
   content_ctx.name_ups                       = NULL;
   content_ctx.valid_extensions               = NULL;
   content_ctx.block_extract                  = false;
   content_ctx.need_fullpath                  = false;
   content_ctx.set_supports_no_game_enable    = false;

   content_ctx.subsystem.data                 = NULL;
   content_ctx.subsystem.size                 = 0;

   if (global)
   {
      if (!string_is_empty(global->name.ips))
         content_ctx.name_ips                 = strdup(global->name.ips);
      if (!string_is_empty(global->name.bps))
         content_ctx.name_bps                 = strdup(global->name.bps);
      if (!string_is_empty(global->name.ups))
         content_ctx.name_ups                 = strdup(global->name.ups);

      global->name.label[0]                   = '\0';
   }

   if (!string_is_empty(path_dir_system))
      content_ctx.directory_system            = strdup(path_dir_system);

   path_set(RARCH_PATH_CONTENT, fullpath);
   path_set(RARCH_PATH_CORE, core_path);

#ifdef HAVE_DYNAMIC
   /* Load core */
   command_event(CMD_EVENT_LOAD_CORE, NULL);

   /* Load content */
   if (!content_info->environ_get)
      content_info->environ_get = menu_content_environment_get;

   if (firmware_update_status(&content_ctx))
      goto end;

   /* Loads content into currently selected core. */
   if (!(ret = content_load(content_info, p_content)))
   {
      if (error_string)
      {
         runloop_msg_queue_push(error_string, 2, 90, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         RARCH_ERR("[CONTENT LOAD]: %s\n", error_string);
         free(error_string);
      }

      retroarch_menu_running();
      goto end;
   }

   task_push_to_history_list(p_content, true, false, false);
#else
   command_event_cmd_exec(p_content,
         path_get(RARCH_PATH_CONTENT), &content_ctx,
         false, &error_string);
   command_event(CMD_EVENT_QUIT, NULL);
#endif

   /* Push quick menu onto menu stack */
   if (type != CORE_TYPE_DUMMY)
      menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, NULL);

#ifdef HAVE_DYNAMIC
end:
#endif
   if (content_ctx.name_ips)
      free(content_ctx.name_ips);
   if (content_ctx.name_bps)
      free(content_ctx.name_bps);
   if (content_ctx.name_ups)
      free(content_ctx.name_ups);
   if (content_ctx.directory_system)
      free(content_ctx.directory_system);

   return ret;
}
#endif

static bool task_load_content_internal(
      content_ctx_info_t *content_info,
      bool loading_from_menu,
      bool loading_from_cli,
      bool loading_from_companion_ui)
{
   content_information_ctx_t content_ctx;

   content_state_t                 *p_content = content_state_get_ptr();
   bool ret                                   = false;
   char *error_string                         = NULL;
   global_t *global                           = global_get_ptr();
   rarch_system_info_t *sys_info              = runloop_get_system_info();
   settings_t *settings                       = config_get_ptr();
   bool check_firmware_before_loading         = settings->bools.check_firmware_before_loading;
   bool set_supports_no_game_enable           = settings->bools.set_supports_no_game_enable;
   const char *path_dir_system                = settings->paths.directory_system;
   const char *path_dir_cache                 = settings->paths.directory_cache;

   content_ctx.check_firmware_before_loading  = check_firmware_before_loading;
#ifdef HAVE_PATCH
   content_ctx.is_ips_pref                    = rarch_ctl(RARCH_CTL_IS_IPS_PREF, NULL);
   content_ctx.is_bps_pref                    = rarch_ctl(RARCH_CTL_IS_BPS_PREF, NULL);
   content_ctx.is_ups_pref                    = rarch_ctl(RARCH_CTL_IS_UPS_PREF, NULL);
   content_ctx.patch_is_blocked               = rarch_ctl(RARCH_CTL_IS_PATCH_BLOCKED, NULL);
#endif
   content_ctx.bios_is_missing                = rarch_ctl(RARCH_CTL_IS_MISSING_BIOS, NULL);
   content_ctx.directory_system               = NULL;
   content_ctx.directory_cache                = NULL;
   content_ctx.name_ips                       = NULL;
   content_ctx.name_bps                       = NULL;
   content_ctx.name_ups                       = NULL;
   content_ctx.valid_extensions               = NULL;
   content_ctx.block_extract                  = false;
   content_ctx.need_fullpath                  = false;
   content_ctx.set_supports_no_game_enable    = false;

   content_ctx.subsystem.data                 = NULL;
   content_ctx.subsystem.size                 = 0;

   if (sys_info)
   {
      struct retro_system_info *system        = runloop_get_libretro_system_info();

      content_ctx.set_supports_no_game_enable = set_supports_no_game_enable;

      if (!string_is_empty(path_dir_cache))
         content_ctx.directory_cache          = strdup(path_dir_cache);
      if (!string_is_empty(system->valid_extensions))
         content_ctx.valid_extensions         = strdup(system->valid_extensions);

      content_ctx.block_extract               = system->block_extract;
      content_ctx.need_fullpath               = system->need_fullpath;

      content_ctx.subsystem.data              = sys_info->subsystem.data;
      content_ctx.subsystem.size              = sys_info->subsystem.size;
   }

   if (global)
   {
      if (!string_is_empty(global->name.ips))
         content_ctx.name_ips                 = strdup(global->name.ips);
      if (!string_is_empty(global->name.bps))
         content_ctx.name_bps                 = strdup(global->name.bps);
      if (!string_is_empty(global->name.ups))
         content_ctx.name_ups                 = strdup(global->name.ups);
   }

   if (!string_is_empty(path_dir_system))
      content_ctx.directory_system            = strdup(path_dir_system);

   if (!content_info->environ_get)
      content_info->environ_get = menu_content_environment_get;

   if (firmware_update_status(&content_ctx))
      goto end;

#ifdef HAVE_DISCORD
   if (discord_is_inited)
   {
      discord_userdata_t userdata;
      userdata.status = DISCORD_PRESENCE_NETPLAY_NETPLAY_STOPPED;
      command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
      userdata.status = DISCORD_PRESENCE_MENU;
      command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
   }
#endif

   /* Loads content into currently selected core. */
   if ((ret = content_load(content_info, p_content)))
      task_push_to_history_list(p_content,
            true, loading_from_cli, loading_from_companion_ui);

end:
   if (content_ctx.name_ips)
      free(content_ctx.name_ips);
   if (content_ctx.name_bps)
      free(content_ctx.name_bps);
   if (content_ctx.name_ups)
      free(content_ctx.name_ups);
   if (content_ctx.directory_system)
      free(content_ctx.directory_system);
   if (content_ctx.directory_cache)
      free(content_ctx.directory_cache);
   if (content_ctx.valid_extensions)
      free(content_ctx.valid_extensions);

   if (!ret)
   {
      if (error_string)
      {
         runloop_msg_queue_push(error_string, 2, 90, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         RARCH_ERR("[CONTENT LOAD]: %s\n", error_string);
         free(error_string);
      }

      return false;
   }

   return true;
}

bool task_push_load_content_with_new_core_from_companion_ui(
      const char *core_path,
      const char *fullpath,
      const char *label,
      const char *db_name,
      const char *crc32,
      content_ctx_info_t *content_info,
      retro_task_callback_t cb,
      void *user_data)
{
   global_t *global            = global_get_ptr();
   content_state_t  *p_content = content_state_get_ptr();

   path_set(RARCH_PATH_CONTENT, fullpath);
   path_set(RARCH_PATH_CORE, core_path);

   p_content->companion_ui_db_name[0] = '\0';
   p_content->companion_ui_crc32[0]   = '\0';

   if (!string_is_empty(db_name))
      strlcpy(p_content->companion_ui_db_name,
            db_name, sizeof(p_content->companion_ui_db_name));

   if (!string_is_empty(crc32))
      strlcpy(p_content->companion_ui_crc32,
            crc32, sizeof(p_content->companion_ui_crc32));

#ifdef HAVE_DYNAMIC
   command_event(CMD_EVENT_LOAD_CORE, NULL);
#endif

   global->launched_from_cli = false;

   if (global)
   {
      if (label)
         strlcpy(global->name.label, label, sizeof(global->name.label));
      else
         global->name.label[0] = '\0';
   }

   /* Load content */
   if (!task_load_content_internal(content_info, true, false, true))
      return false;

#ifdef HAVE_MENU
   /* Push quick menu onto menu stack */
   menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, NULL);
#endif

   return true;
}

bool task_push_load_content_from_cli(
      const char *core_path,
      const char *fullpath,
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data)
{
   return task_load_content_internal(content_info, true, true, false);
}

bool task_push_start_builtin_core(
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data)
{
   /* Clear content path */
   path_clear(RARCH_PATH_CONTENT);

   /* Preliminary stuff that has to be done before we
    * load the actual content. Can differ per mode. */
   retroarch_set_current_core_type(type, true);

   /* Load content */
   if (!task_load_content_internal(content_info, true, false, false))
   {
      retroarch_menu_running();
      return false;
   }

   /* Push quick menu onto menu stack */
#ifdef HAVE_MENU
   menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, NULL);
#endif

   return true;
}

bool task_push_load_content_with_core_from_menu(
      const char *fullpath,
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data)
{
   path_set(RARCH_PATH_CONTENT, fullpath);

   /* Load content */
   if (!task_load_content_internal(content_info, true, false, false))
   {
      retroarch_menu_running();
      return false;
   }

#ifdef HAVE_MENU
   /* Push quick menu onto menu stack */
   if (type != CORE_TYPE_DUMMY)
      menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, NULL);
#endif

   return true;
}

bool task_push_load_content_with_current_core_from_companion_ui(
      const char *fullpath,
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data)
{
   content_state_t  *p_content = content_state_get_ptr();

   /* TODO/FIXME: Enable setting of these values
    * via function arguments */
   p_content->companion_ui_db_name[0] = '\0';
   p_content->companion_ui_crc32[0]   = '\0';

   /* Load content
    * > TODO/FIXME: Set loading_from_companion_ui 'false' for
    *   now, until someone can implement the required higher
    *   level functionality in 'win32_common.c' and 'ui_cocoa.m' */
   return task_push_load_content_with_core_from_menu(fullpath,
         content_info, type, cb, user_data);
}


bool task_push_load_subsystem_with_core_from_menu(
      const char *fullpath,
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data)
{
   content_state_t  *p_content = content_state_get_ptr();

   p_content->pending_subsystem_init = true;

   /* Load content */
   if (!task_load_content_internal(content_info, true, false, false))
   {
      retroarch_menu_running();
      return false;
   }

#ifdef HAVE_MENU
   /* Push quick menu onto menu stack */
   if (type != CORE_TYPE_DUMMY)
      menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, NULL);
#endif

   return true;
}

void content_get_status(
      bool *contentless,
      bool *is_inited)
{
   content_state_t  *p_content = content_state_get_ptr();

   *contentless = p_content->core_does_not_need_content;
   *is_inited   = p_content->is_inited;
}

/* Clears the pending subsystem rom buffer*/
void content_clear_subsystem(void)
{
   unsigned i;
   content_state_t  *p_content = content_state_get_ptr();

   p_content->pending_subsystem_rom_id    = 0;
   p_content->pending_subsystem_init      = false;

   for (i = 0; i < RARCH_MAX_SUBSYSTEM_ROMS; i++)
   {
      if (p_content->pending_subsystem_roms[i])
      {
         free(p_content->pending_subsystem_roms[i]);
         p_content->pending_subsystem_roms[i] = NULL;
      }
   }
}

/* Set the current subsystem*/
void content_set_subsystem(unsigned idx)
{
   const struct retro_subsystem_info *subsystem;
   rarch_system_info_t                  *system = runloop_get_system_info();
   content_state_t  *p_content                  = content_state_get_ptr();

   /* Core fully loaded, use the subsystem data */
   if (system->subsystem.data)
      subsystem = system->subsystem.data + idx;
   /* Core not loaded completely, use the data we peeked on load core */
   else
      subsystem = subsystem_data + idx;

   p_content->pending_subsystem_id = idx;

   if (subsystem && subsystem_current_count > 0)
   {
      strlcpy(p_content->pending_subsystem_ident,
         subsystem->ident, sizeof(p_content->pending_subsystem_ident));

      p_content->pending_subsystem_rom_num = subsystem->num_roms;
   }

   RARCH_LOG("[Subsystem]: Setting current subsystem to: %d(%s) Content amount: %d\n",
      p_content->pending_subsystem_id,
      p_content->pending_subsystem_ident,
      p_content->pending_subsystem_rom_num);
}

/* Sets the subsystem by name */
bool content_set_subsystem_by_name(const char* subsystem_name)
{
   rarch_system_info_t                  *system = runloop_get_system_info();
   unsigned i                                   = 0;
   /* Core not loaded completely, use the data we peeked on load core */
   const struct retro_subsystem_info 
      *subsystem                                = subsystem_data;

   /* Core fully loaded, use the subsystem data */
   if (system->subsystem.data)
      subsystem = system->subsystem.data;

   for (i = 0; i < subsystem_current_count; i++, subsystem++)
   {
      if (string_is_equal(subsystem_name, subsystem->ident))
      {
         content_set_subsystem(i);
         return true;
      }
   }

   return false;
}

void content_get_subsystem_friendly_name(const char* subsystem_name, char* subsystem_friendly_name, size_t len)
{
   rarch_system_info_t                  *system = runloop_get_system_info();
   unsigned i                                   = 0;
   /* Core not loaded completely, use the data we peeked on load core */
   const struct retro_subsystem_info *subsystem = subsystem_data;

   /* Core fully loaded, use the subsystem data */
   if (system->subsystem.data)
      subsystem = system->subsystem.data;

   for (i = 0; i < subsystem_current_count; i++, subsystem++)
   {
      if (string_is_equal(subsystem_name, subsystem->ident))
      {
         strlcpy(subsystem_friendly_name, subsystem->desc, len);
         break;
      }
   }

   return;
}

/* Add a rom to the subsystem ROM buffer */
void content_add_subsystem(const char* path)
{
   content_state_t *p_content = content_state_get_ptr();
   size_t pending_size        = PATH_MAX_LENGTH * sizeof(char);
   p_content->pending_subsystem_roms[p_content->pending_subsystem_rom_id] = (char*)malloc(pending_size);

   strlcpy(p_content->pending_subsystem_roms[
         p_content->pending_subsystem_rom_id],
         path, pending_size);
   RARCH_LOG("[Subsystem]: Subsystem id: %d Subsystem ident:"
         " %s Content ID: %d, Content Path: %s\n",
         p_content->pending_subsystem_id,
         p_content->pending_subsystem_ident,
         p_content->pending_subsystem_rom_id,
         p_content->pending_subsystem_roms[
         p_content->pending_subsystem_rom_id]);
   p_content->pending_subsystem_rom_id++;
}

void content_set_does_not_need_content(void)
{
   content_state_t *p_content = content_state_get_ptr();
   p_content->core_does_not_need_content = true;
}

void content_unset_does_not_need_content(void)
{
   content_state_t *p_content = content_state_get_ptr();
   p_content->core_does_not_need_content = false;
}

uint32_t content_get_crc(void)
{
   content_state_t *p_content = content_state_get_ptr();
   if (p_content->pending_rom_crc)
   {
      p_content->pending_rom_crc   = false;
      p_content->rom_crc           = file_crc32(0,
            (const char*)p_content->pending_rom_crc_path);
      RARCH_LOG("[CONTENT LOAD]: CRC32: 0x%x .\n",
            (unsigned)p_content->rom_crc);
   }
   return p_content->rom_crc;
}

char* content_get_subsystem_rom(unsigned index)
{
   content_state_t *p_content = content_state_get_ptr();
   return p_content->pending_subsystem_roms[index];
}

bool content_is_inited(void)
{
   content_state_t *p_content = content_state_get_ptr();
   return p_content->is_inited;
}

void content_deinit(void)
{
   unsigned i;
   content_state_t *p_content = content_state_get_ptr();

   if (p_content->temporary_content)
   {
      for (i = 0; i < p_content->temporary_content->size; i++)
      {
         const char *path = p_content->temporary_content->elems[i].data;

         RARCH_LOG("[CONTENT LOAD]: %s: %s.\n",
               msg_hash_to_str(MSG_REMOVING_TEMPORARY_CONTENT_FILE), path);
         if (filestream_delete(path) != 0)
            RARCH_ERR("[CONTENT LOAD]: %s: %s.\n",
                  msg_hash_to_str(MSG_FAILED_TO_REMOVE_TEMPORARY_FILE),
                  path);
      }
      string_list_free(p_content->temporary_content);
   }

   p_content->temporary_content            = NULL;
   p_content->rom_crc                      = 0;
   p_content->is_inited                    = false;
   p_content->core_does_not_need_content   = false;
   p_content->pending_rom_crc              = false;
}

/* Set environment variables before a subsystem load */
void content_set_subsystem_info(void)
{
   content_state_t *p_content = content_state_get_ptr();
   if (!p_content->pending_subsystem_init)
      return;

   path_set(RARCH_PATH_SUBSYSTEM, p_content->pending_subsystem_ident);
   path_set_special(p_content->pending_subsystem_roms,
         p_content->pending_subsystem_rom_num);
}

/* Initializes and loads a content file for the currently
 * selected libretro core. */
bool content_init(void)
{
   struct string_list content;
   content_information_ctx_t content_ctx;
   enum msg_hash_enums error_enum             = MSG_UNKNOWN;
   content_state_t *p_content                 = content_state_get_ptr();

   bool ret                                   = true;
   char *error_string                         = NULL;
   global_t *global                           = global_get_ptr();
   rarch_system_info_t *sys_info              = runloop_get_system_info();
   settings_t *settings                       = config_get_ptr();
   bool check_firmware_before_loading         = settings->bools.check_firmware_before_loading;
   bool set_supports_no_game_enable           = settings->bools.set_supports_no_game_enable;
   const char *path_dir_system                = settings->paths.directory_system;
   const char *path_dir_cache                 = settings->paths.directory_cache;

   p_content->temporary_content               = string_list_new();

   content_ctx.check_firmware_before_loading  = check_firmware_before_loading;
#ifdef HAVE_PATCH
   content_ctx.is_ips_pref                    = rarch_ctl(RARCH_CTL_IS_IPS_PREF, NULL);
   content_ctx.is_bps_pref                    = rarch_ctl(RARCH_CTL_IS_BPS_PREF, NULL);
   content_ctx.is_ups_pref                    = rarch_ctl(RARCH_CTL_IS_UPS_PREF, NULL);
   content_ctx.patch_is_blocked               = rarch_ctl(RARCH_CTL_IS_PATCH_BLOCKED, NULL);
#endif
   content_ctx.temporary_content              = p_content->temporary_content;
   content_ctx.directory_system               = NULL;
   content_ctx.directory_cache                = NULL;
   content_ctx.name_ips                       = NULL;
   content_ctx.name_bps                       = NULL;
   content_ctx.name_ups                       = NULL;
   content_ctx.valid_extensions               = NULL;
   content_ctx.block_extract                  = false;
   content_ctx.need_fullpath                  = false;
   content_ctx.set_supports_no_game_enable    = false;

   content_ctx.subsystem.data                 = NULL;
   content_ctx.subsystem.size                 = 0;

   if (global)
   {
      if (!string_is_empty(global->name.ips))
         content_ctx.name_ips                 = strdup(global->name.ips);
      if (!string_is_empty(global->name.bps))
         content_ctx.name_bps                 = strdup(global->name.bps);
      if (!string_is_empty(global->name.ups))
         content_ctx.name_ups                 = strdup(global->name.ups);
   }

   if (sys_info)
   {
      struct retro_system_info *system        = runloop_get_libretro_system_info();

      content_ctx.set_supports_no_game_enable = set_supports_no_game_enable;

      if (!string_is_empty(path_dir_system))
         content_ctx.directory_system         = strdup(path_dir_system);
      if (!string_is_empty(path_dir_cache))
         content_ctx.directory_cache          = strdup(path_dir_cache);
      if (!string_is_empty(system->valid_extensions))
         content_ctx.valid_extensions         = strdup(system->valid_extensions);

      content_ctx.block_extract               = system->block_extract;
      content_ctx.need_fullpath               = system->need_fullpath;

      content_ctx.subsystem.data              = sys_info->subsystem.data;
      content_ctx.subsystem.size              = sys_info->subsystem.size;
   }

   p_content->is_inited = true;

   if (string_list_initialize(&content))
   {
      if (     !p_content->temporary_content
            || !content_file_init(&content_ctx, p_content,
               &content, &error_enum, &error_string))
      {
         content_deinit();

         ret                = false;
      }
      string_list_deinitialize(&content);
   }

   if (content_ctx.name_ips)
      free(content_ctx.name_ips);
   if (content_ctx.name_bps)
      free(content_ctx.name_bps);
   if (content_ctx.name_ups)
      free(content_ctx.name_ups);
   if (content_ctx.directory_system)
      free(content_ctx.directory_system);
   if (content_ctx.directory_cache)
      free(content_ctx.directory_cache);
   if (content_ctx.valid_extensions)
      free(content_ctx.valid_extensions);
   
   if (error_enum != MSG_UNKNOWN)
   {
      switch (error_enum)
      {
         case MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT:
         case MSG_ERROR_LIBRETRO_CORE_REQUIRES_VFS:
         case MSG_FAILED_TO_LOAD_CONTENT:
         case MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT:
            RARCH_ERR("[CONTENT LOAD]: %s\n", msg_hash_to_str(error_enum));
            runloop_msg_queue_push(msg_hash_to_str(error_enum), 2, ret ? 1 : 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            break;
         case MSG_UNKNOWN:
         default:
            break;
      }
   }

   if (error_string)
   {
      if (ret)
      {
         RARCH_LOG("[CONTENT LOAD]: %s\n", error_string);
      }
      else
      {
         RARCH_ERR("[CONTENT LOAD]: %s\n", error_string);
      }
      /* Do not flush the message queue here
       * > This allows any core-generated error messages
       *   to propagate through to the frontend */
      runloop_msg_queue_push(error_string, 2, ret ? 1 : 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      free(error_string);
   }

   return ret;
}

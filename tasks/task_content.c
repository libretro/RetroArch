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
#include <Fileapifromapp.h>
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
#include <array/rbuf.h>

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
#include "../runloop.h"
#include "../verbosity.h"

#include "../msg_hash.h"
#include "../content.h"
#include "../dynamic.h"
#include "../retroarch.h"
#include "../file_path_special.h"
#include "../core.h"
#include "../paths.h"
#include "../verbosity.h"

#ifdef HAVE_PRESENCE
#include "../network/presence.h"
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

enum content_information_flags
{
   CONTENT_INFO_FLAG_BLOCK_EXTRACT               = (1 << 0),
   CONTENT_INFO_FLAG_NEED_FULLPATH               = (1 << 1),
   CONTENT_INFO_FLAG_SET_SUPPORTS_NO_GAME_ENABLE = (1 << 2),
   CONTENT_INFO_FLAG_IS_IPS_PREF                 = (1 << 3),
   CONTENT_INFO_FLAG_IS_BPS_PREF                 = (1 << 4),
   CONTENT_INFO_FLAG_IS_UPS_PREF                 = (1 << 5),
   CONTENT_INFO_FLAG_PATCH_IS_BLOCKED            = (1 << 6),
   CONTENT_INFO_FLAG_BIOS_IS_MISSING             = (1 << 7),
   CONTENT_INFO_FLAG_CHECK_FW_BEFORE_LOADING     = (1 << 8)
};

struct content_information_ctx
{
   char *name_ips;
   char *name_bps;
   char *name_ups;

   char *valid_extensions;
   char *directory_cache;
   char *directory_system;

   struct
   {
      struct retro_subsystem_info *data;
      unsigned size;
   } subsystem;

   uint16_t flags;
};

/*************************************/
/* Content file info functions START */
/*************************************/

static void content_file_override_free(
      content_state_t *p_content)
{
   size_t i;

   if (!p_content->content_override_list)
      return;

   for (i = 0; i < RBUF_LEN(p_content->content_override_list); i++)
   {
      content_file_override_t *override =
            &p_content->content_override_list[i];
      if (override && override->ext)
         free(override->ext);
   }

   RBUF_FREE(p_content->content_override_list);
}

/* Returns true if an override for content files
 * of extension 'ext' has been set */
static bool content_file_override_get_ext(
      content_state_t *p_content,
      const char *ext,
      const content_file_override_t **override)
{
   size_t num_overrides;
   size_t i;

   if (p_content && !string_is_empty(ext))
   {
      if ((num_overrides = RBUF_LEN(p_content->content_override_list)) >= 1)
      {
         for (i = 0; i < num_overrides; i++)
         {
            content_file_override_t *content_override =
               &p_content->content_override_list[i];

            if (!content_override ||
                  !content_override->ext)
               continue;

            if (string_is_equal_noncase(
                     content_override->ext, ext))
            {
               if (override)
                  *override = content_override;

               return true;
            }
         }
      }
   }

   return false;
}

bool content_file_override_set(
      const struct retro_system_content_info_override *overrides)
{
   content_state_t *p_content = content_state_get_ptr();
   size_t i;

   if (!p_content || !overrides)
      return false;

   /* Free any existing override list */
   content_file_override_free(p_content);

   for (i = 0; overrides[i].extensions; i++)
   {
      struct string_list ext_list = {0};
      size_t j;

      /* Get list of extensions affected by override */
      string_list_initialize(&ext_list);
      if (!string_split_noalloc(&ext_list,
            overrides[i].extensions, "|"))
      {
         string_list_deinitialize(&ext_list);
         continue;
      }

      for (j = 0; j < ext_list.size; j++)
      {
         const char *ext                   = ext_list.elems[j].data;
         content_file_override_t *override = NULL;
         size_t num_entries;

         /* Check whether extension has already been
          * registered */
         if (string_is_empty(ext) ||
             content_file_override_get_ext(p_content, ext, NULL))
            continue;

         /* Add current override to the list */
         num_entries = RBUF_LEN(p_content->content_override_list);

         if (!RBUF_TRYFIT(p_content->content_override_list,
               num_entries + 1))
         {
            string_list_deinitialize(&ext_list);
            return false;
         }

         RBUF_RESIZE(p_content->content_override_list,
               num_entries + 1);

         RARCH_LOG("[Content Override]: File Extension: '%s' - need_fullpath: %s, persistent_data: %s\n",
               ext, overrides[i].need_fullpath ? "TRUE" : "FALSE",
               overrides[i].persistent_data ? "TRUE" : "FALSE");

         override                  = &p_content->content_override_list[num_entries];
         override->ext             = strdup(ext);
         override->need_fullpath   = overrides[i].need_fullpath;
         override->persistent_data = overrides[i].persistent_data;
      }

      string_list_deinitialize(&ext_list);
   }

   return true;
}

/* Frees any content data that is not flagged
 * as 'persistent'. Should be called after
 * content_file_load() */
static void content_file_list_free_transient_data(
      content_file_list_t *file_list)
{
   size_t i;

   if (!file_list)
      return;

   for (i = 0; i < file_list->size; i++)
   {
      content_file_info_t *file_info = &file_list->entries[i];

      if (file_info->data &&
          !file_info->persistent_data)
      {
         free((void*)file_info->data);

         file_info->data      = NULL;
         file_info->data_size = 0;
      }
   }
}

static void content_file_list_free_entry(
      content_file_info_t *file_info)
{
   if (!file_info)
      return;

   if (file_info->full_path)
   {
      free(file_info->full_path);
      file_info->full_path = NULL;
   }

   if (file_info->archive_path)
   {
      free(file_info->archive_path);
      file_info->archive_path = NULL;
   }

   if (file_info->archive_file)
   {
      free(file_info->archive_file);
      file_info->archive_file = NULL;
   }

   if (file_info->dir)
   {
      free(file_info->dir);
      file_info->dir = NULL;
   }

   if (file_info->name)
   {
      free(file_info->name);
      file_info->name = NULL;
   }

   if (file_info->ext)
   {
      free(file_info->ext);
      file_info->ext = NULL;
   }

   if (file_info->meta)
   {
      free(file_info->meta);
      file_info->meta = NULL;
   }

   if (file_info->data)
   {
      free((void*)file_info->data);
      file_info->data = NULL;
   }
   file_info->data_size = 0;

   file_info->file_in_archive = false;
   file_info->persistent_data = false;
}

static void content_file_list_free(
      content_file_list_t *file_list)
{
   if (!file_list)
      return;

   if (file_list->entries)
   {
      size_t i;

      for (i = 0; i < file_list->size; i++)
      {
         content_file_info_t *file_info = &file_list->entries[i];
         content_file_list_free_entry(file_info);
      }

      free(file_list->entries);
   }

   if (file_list->temporary_files)
   {
      size_t i;

      /* Remove any temporary content files from
       * the file system */
      for (i = 0; i < file_list->temporary_files->size; i++)
      {
         const char *path = file_list->temporary_files->elems[i].data;

         if (string_is_empty(path))
            continue;

         RARCH_LOG("[Content]: %s: \"%s\".\n",
               msg_hash_to_str(MSG_REMOVING_TEMPORARY_CONTENT_FILE),
               path);

         if (filestream_delete(path) != 0)
            RARCH_ERR("[Content]: %s: \"%s\".\n",
                  msg_hash_to_str(MSG_FAILED_TO_REMOVE_TEMPORARY_FILE),
                  path);
      }

      string_list_free(file_list->temporary_files);
   }

   if (file_list->game_info)
      free(file_list->game_info);

   if (file_list->game_info_ext)
      free(file_list->game_info_ext);

   free(file_list);
}

static content_file_list_t *content_file_list_init(size_t size)
{
   content_file_list_t *file_list = NULL;

   if ((file_list = (content_file_list_t *)malloc(sizeof(*file_list))))
   {
      /* Set initial 'values' */
      file_list->entries         = NULL;
      file_list->size            = 0;
      file_list->game_info       = NULL;
      file_list->game_info_ext   = NULL;
      file_list->temporary_files = string_list_new();

      if (file_list->temporary_files)
      {
         /* Create entries list */
         if ((file_list->entries             = (content_file_info_t *)
               calloc(size, sizeof(content_file_info_t))))
         {
            file_list->size                  = size;
            /* Create retro_game_info object */
            if ((file_list->game_info        = (struct retro_game_info *)
                     calloc(size, sizeof(struct retro_game_info))))
            {
               /* Create retro_game_info_ext object */
               if ((file_list->game_info_ext = 
                        (struct retro_game_info_ext *)
                        calloc(size, sizeof(struct retro_game_info_ext))))
                  return file_list;
            }
         }
      }
   }

   content_file_list_free(file_list);
   return NULL;
}

/* Convenience function: Adds an entry to the
 * temporary (i.e. extracted) content file list.
 * Returns pointer to allocated char array. */
static const char *content_file_list_append_temporary(
      content_file_list_t *file_list,
      const char *path)
{
   union string_list_elem_attr attr;

   if (!file_list ||
       string_is_empty(path))
      return NULL;

   attr.i = 0;

   if (string_list_append(file_list->temporary_files,
         path, attr))
      return file_list->temporary_files->elems[
         file_list->temporary_files->size - 1].data;

   return NULL;
}

/* Note: Takes ownership of supplied 'data' buffer */
static bool content_file_list_set_info(
      content_file_list_t *file_list,
      const char *path,
      void *data,
      size_t data_size,
      bool persistent_data,
      size_t idx)
{
   content_file_info_t *file_info            = NULL;
   struct retro_game_info *game_info         = NULL;
   struct retro_game_info_ext *game_info_ext = NULL;

   if (  !file_list
       || (idx >= file_list->size))
      return false;

   if (!(file_info = &file_list->entries[idx]))
      return false;

   if (!(game_info = &file_list->game_info[idx]))
      return false;

   if (!(game_info_ext = &file_list->game_info_ext[idx]))
      return false;

   /* Clear any existing info */
   content_file_list_free_entry(file_info);

   game_info->path                = NULL;
   game_info->data                = NULL;
   game_info->size                = 0;
   game_info->meta                = NULL;

   game_info_ext->full_path       = NULL;
   game_info_ext->archive_path    = NULL;
   game_info_ext->archive_file    = NULL;
   game_info_ext->dir             = NULL;
   game_info_ext->name            = NULL;
   game_info_ext->ext             = NULL;
   game_info_ext->meta            = NULL;
   game_info_ext->data            = NULL;
   game_info_ext->size            = 0;
   game_info_ext->file_in_archive = false;
   game_info_ext->persistent_data = false;

   /* Assign data */
   if (( data && !data_size) ||
       (!data &&  data_size))
      return false;

   file_info->data            = data;
   file_info->data_size       = data_size;
   file_info->persistent_data = persistent_data;

   /* Assign paths
    * > There is some degree of redundant data
    *   here, but we need it in this format
    *   (persistent copies of each parameter)
    *   to minimise complications when passing
    *   extended path info to cores */
   if (!string_is_empty(path))
   {
      char dir [PATH_MAX_LENGTH];
      char name[NAME_MAX_LENGTH];
      const char *archive_delim = NULL;
      const char *ext           = NULL;

      /* 'Full' path - may point to a file
       * inside an archive */
      file_info->full_path      = strdup(path);

      /* File extension - may be used core-side
       * to differentiate content types */
      if ((ext = path_get_extension(path)))
      {
         file_info->ext         = strdup(ext);
         /* File extension is always presented
          * core-side in lowercase format */
         string_to_lower(file_info->ext);
      }

      /* Check whether path corresponds to a file
       * inside an archive */
      if ((archive_delim = path_get_archive_delim(path)))
      {
         char archive_path[PATH_MAX_LENGTH];
         size_t len      = 0;
         /* Extract path of parent archive */
         if ((len = (size_t)(1 + archive_delim - path))
                 >= PATH_MAX_LENGTH)
            len = PATH_MAX_LENGTH;

         strlcpy(archive_path, path, len * sizeof(char));
         if (!string_is_empty(archive_path))
            file_info->archive_path = strdup(archive_path);

         /* Extract name of file in archive */
         archive_delim++;
         if (!string_is_empty(archive_delim))
            file_info->archive_file = strdup(archive_delim);

         /* Extract parent directory - may be used
          * core-side as a reference point for searching
          * related content (e.g. same file name, different
          * extension) */
         fill_pathname_parent_dir(dir, archive_path, sizeof(dir));

         /* Extract 'canonical' name/id of content file -
          * may be used core-side as a reference point for
          * searching related content. For archived content,
          * this is the basename of the archive file without
          * extension */
         fill_pathname_base(name, archive_path, sizeof(name));
         path_remove_extension(name);

         file_info->file_in_archive = true;
      }
      else
      {
         /* If path corresponds to a 'normal' file,
          * just extract parent directory */
         fill_pathname_parent_dir(dir, path, sizeof(dir));

         /* For uncompressed content, 'canonical' name/id
          * is the basename of the content file, without
          * extension */
         fill_pathname_base(name, path, sizeof(name));
         path_remove_extension(name);
      }

      if (!string_is_empty(dir))
      {
         /* Remove any trailing slash */
         char *last_slash         = find_last_slash(dir);
         if (last_slash && (last_slash[1] == '\0'))
            *last_slash           = '\0';

         if (!string_is_empty(dir))
            file_info->dir        = strdup(dir);
      }

      if (!string_is_empty(name))
         file_info->name          = strdup(name);
   }

   /* Assign retro_game_info pointers */
   game_info->path                = file_info->full_path;
   game_info->data                = file_info->data;
   game_info->size                = file_info->data_size;
   game_info->meta                = file_info->meta;

   /* Assign retro_game_info_ext pointers */
   game_info_ext->full_path       = file_info->full_path;
   game_info_ext->archive_path    = file_info->archive_path;
   game_info_ext->archive_file    = file_info->archive_file;
   game_info_ext->dir             = file_info->dir;
   game_info_ext->name            = file_info->name;
   game_info_ext->ext             = file_info->ext;
   game_info_ext->meta            = file_info->meta;
   game_info_ext->data            = file_info->data;
   game_info_ext->size            = file_info->data_size;
   game_info_ext->file_in_archive = file_info->file_in_archive;
   game_info_ext->persistent_data = file_info->persistent_data;

   return true;
}

/***********************************/
/* Content file info functions END */
/***********************************/

/********************************/
/* Content file functions START */
/********************************/

#define CONTENT_FILE_ATTR_RESET(attr) (attr.i = 0)

#define CONTENT_FILE_ATTR_SET_BLOCK_EXTRACT(attr, block_extract) (attr.i |= ((block_extract) ? 1 : 0))
#define CONTENT_FILE_ATTR_SET_NEED_FULLPATH(attr, need_fullpath) (attr.i |= ((need_fullpath) ? 2 : 0))
#define CONTENT_FILE_ATTR_SET_REQUIRED(attr, required)           (attr.i |= ((required)      ? 4 : 0))
#define CONTENT_FILE_ATTR_SET_PERSISTENT(attr, persistent)       (attr.i |= ((persistent)    ? 8 : 0))

#define CONTENT_FILE_ATTR_GET_BLOCK_EXTRACT(attr) ((attr.i & 1) != 0)
#define CONTENT_FILE_ATTR_GET_NEED_FULLPATH(attr) ((attr.i & 2) != 0)
#define CONTENT_FILE_ATTR_GET_REQUIRED(attr)      ((attr.i & 4) != 0)
#define CONTENT_FILE_ATTR_GET_PERSISTENT(attr)    ((attr.i & 8) != 0)

/**
 * content_file_load_into_memory:
 * @content_path : path of the content file.
 * @data         : buffer into which the content file will be read.
 * @data_size    : size of the resultant content buffer.
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
      const char *content_path,
      bool content_compressed,
      size_t idx,
      enum rarch_content_type first_content_type,
      uint8_t **data,
      size_t *data_size)
{
   uint8_t *content_data = NULL;
   int64_t content_size  = 0;

   *data                 = NULL;
   *data_size            = 0;

   RARCH_LOG("[Content]: %s: \"%s\".\n",
         msg_hash_to_str(MSG_LOADING_CONTENT_FILE), content_path);

   /* Read content from file into memory buffer */
#ifdef HAVE_COMPRESSION
   if (content_compressed)
   {
      if (!file_archive_compressed_read(content_path,
            (void**)&content_data, NULL, &content_size))
         return false;
   }
   else
#endif
      if (!filestream_read_file(content_path,
            (void**)&content_data, &content_size))
         return false;

   if (content_size < 0)
      return false;

   /* First content file is significant: attempt to do
    * soft patching, CRC checking, etc. */
   if (idx == 0)
   {
      /* If we have a media type, ignore patches/CRC32
       * calculation. */
      if (first_content_type == RARCH_CONTENT_NONE)
      {
         bool has_patch = false;

#ifdef HAVE_PATCH
         /* Attempt to apply a patch. */
         if (!(content_ctx->flags & CONTENT_INFO_FLAG_PATCH_IS_BLOCKED))
            has_patch = patch_content(
                  content_ctx->flags & CONTENT_INFO_FLAG_IS_IPS_PREF,
                  content_ctx->flags & CONTENT_INFO_FLAG_IS_BPS_PREF,
                  content_ctx->flags & CONTENT_INFO_FLAG_IS_UPS_PREF,
                  content_ctx->name_ips,
                  content_ctx->name_bps,
                  content_ctx->name_ups,
                  (uint8_t**)&content_data,
                  (void*)&content_size);
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
                  (size_t)content_size);
            RARCH_LOG("[Content]: CRC32: 0x%x.\n",
                  (unsigned)p_content->rom_crc);
         }
         else
         {
            /* We don't have the content ready inside a memory buffer,
               so we have to read it from file later (deferred)
               and then encode the CRC32 hash */
            strlcpy(p_content->pending_rom_crc_path, content_path,
                  sizeof(p_content->pending_rom_crc_path));
            p_content->flags |= CONTENT_ST_FLAG_PENDING_ROM_CRC;
         }
      }
      else
         p_content->rom_crc = 0;
   }

   *data      = content_data;
   *data_size = (size_t)content_size;

   return true;
}

#ifdef HAVE_COMPRESSION
static bool content_file_extract_from_archive(
      content_information_ctx_t *content_ctx,
      content_state_t *p_content,
      const char *valid_exts,
      const char **content_path,
      char **error_string)
{
   const char *tmp_path_ptr = NULL;
   char tmp_path[PATH_MAX_LENGTH];
   char msg[1024];

   tmp_path[0]  = '\0';
   msg[0]       = '\0';

   RARCH_LOG("[Content]: Core requires uncompressed content - "
         "extracting archive to temporary directory.\n");

   /* Attempt to extract file  */
   if (!file_archive_extract_file(
         *content_path, valid_exts,
         string_is_empty(content_ctx->directory_cache) ?
               NULL : content_ctx->directory_cache,
         tmp_path, sizeof(tmp_path)))
   {
      snprintf(msg, sizeof(msg), "%s: \"%s\".\n",
            msg_hash_to_str(MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE),
            *content_path);
      *error_string = strdup(msg);
      return false;
   }

   /* Add path of extracted file to temporary content
    * list (so it can be deleted when deinitialising
    * the core) */
   if (!(tmp_path_ptr = content_file_list_append_temporary(
         p_content->content_list, tmp_path)))
      return false;

   /* Update content path pointer */
   *content_path = tmp_path_ptr;

   RARCH_LOG("[Content]: Content successfully extracted to: \"%s\".\n",
         tmp_path);

   return true;
}
#endif

static void content_file_get_path(
      struct string_list *content,
      size_t idx,
      const char *valid_exts,
      const char **path,
      bool *path_is_compressed)
{
   const char *content_path = content->elems[idx].data;
   bool path_is_archive;
   bool path_is_inside_archive;

   *path               = NULL;
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
            /* Build 'complete' archive file path */
            size_t _len       = strlcpy(info_path,
                  content_path, sizeof(info_path));
            info_path[_len  ] = '#';
            info_path[_len+1] = '\0';
            strlcat(info_path, archive_file, sizeof(info_path));

            /* Update 'content' string_list */
            string_list_set(content, (unsigned)idx, info_path);
            content_path = content->elems[idx].data;

            string_list_free(archive_list);
         }
      }
   }
#endif

   *path = content_path;
}

static void content_file_apply_overrides(
      content_state_t *p_content,
      struct string_list *content,
      size_t idx,
      const char *path)
{
   const content_file_override_t *override = NULL;

   /* Check whether an override has been set
    * for files of this type */
   if (content_file_override_get_ext(p_content,
         path_get_extension(path), &override))
   {
      /* Get existing attributes */
      bool block_extract = CONTENT_FILE_ATTR_GET_BLOCK_EXTRACT(content->elems[idx].attr);
      bool required      = CONTENT_FILE_ATTR_GET_REQUIRED(content->elems[idx].attr);
      bool persistent    = CONTENT_FILE_ATTR_GET_PERSISTENT(content->elems[idx].attr);

      CONTENT_FILE_ATTR_RESET(content->elems[idx].attr);

      /* Apply updates
       * > Note that 'persistent' attribute must not
       *   be set false by an override if it is already
       *   true (frontend may require persistence for
       *   other purposes, e.g. runahead) */
      CONTENT_FILE_ATTR_SET_BLOCK_EXTRACT(content->elems[idx].attr,
            block_extract);
      CONTENT_FILE_ATTR_SET_NEED_FULLPATH(content->elems[idx].attr,
            override->need_fullpath);
      CONTENT_FILE_ATTR_SET_REQUIRED(content->elems[idx].attr,
            required);
      CONTENT_FILE_ATTR_SET_PERSISTENT(content->elems[idx].attr,
            (persistent || override->persistent_data));
   }
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
   rarch_system_info_t *system                = &runloop_state_get_ptr()->system;
#endif
   enum rarch_content_type first_content_type = RARCH_CONTENT_NONE;

   msg[0] = '\0';

   for (i = 0; i < content->size; i++)
   {
      const char *content_path = NULL;
      uint8_t *content_data    = NULL;
      size_t content_size      = 0;
      const char *valid_exts   = special
            ? special->roms[i].valid_extensions
            : content_ctx->valid_extensions;
      bool content_compressed  = false;

      /* Get content path */
      content_file_get_path(content, i, valid_exts,
            &content_path, &content_compressed);

      /* If content is missing and core requires content,
       * return an error */
      if (string_is_empty(content_path))
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
            first_content_type = path_is_media_type(content_path);

         /* Apply any file-type-specific content
          * handling overrides */
         if (p_content->content_override_list)
            content_file_apply_overrides(p_content, content, i, content_path);

         /* If core does not require 'fullpath', load
          * the content into memory */
         if (!CONTENT_FILE_ATTR_GET_NEED_FULLPATH(content->elems[i].attr))
         {
            if (!content_file_load_into_memory(
                  content_ctx, p_content, content_path,
                  content_compressed, i, first_content_type,
                  &content_data, &content_size))
            {
               snprintf(msg, sizeof(msg), "%s \"%s\"\n",
                     msg_hash_to_str(MSG_COULD_NOT_READ_CONTENT_FILE),
                     content_path);
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
                !content_file_extract_from_archive(content_ctx, p_content,
                     valid_exts, &content_path, error_string))
               return false;
#endif
#ifdef __WINRT__
            /* TODO: When support for the 'actual' VFS is added,
             * there will need to be some more logic here */
            if (!system->supports_vfs &&
                !is_path_accessible_using_standard_io(content_path))
            {
               /* Try to copy ACL to file first. If successful, this should mean that cores using standard I/O can still access them
               *  It would be better to set the ACL to allow full access for all application packages. However,
               *  this is substantially easier than writing out new functions to do this
               *  Copy ACL from localstate
               *  I am genuinely really proud of these work arounds 
               */
               wchar_t wcontent_path[MAX_PATH];
               mbstowcs(wcontent_path, content_path, MAX_PATH);
               uwp_set_acl(wcontent_path, L"S-1-15-2-1");
               if (!is_path_accessible_using_standard_io(content_path))
               {
                  wchar_t wnew_path[MAX_PATH];
                  /* Fallback to a file copy into an accessible directory */
                  char new_basedir[PATH_MAX_LENGTH];
                  char new_path[PATH_MAX_LENGTH];

                  RARCH_LOG("[Content]: Core does not support VFS"
                     " - copying to cache directory.\n");

                  if (!string_is_empty(content_ctx->directory_cache))
                     strlcpy(new_basedir, content_ctx->directory_cache,
                        sizeof(new_basedir));
                  else
                     new_basedir[0] = '\0';

                  if (string_is_empty(new_basedir) ||
                     !path_is_directory(new_basedir) ||
                     !is_path_accessible_using_standard_io(new_basedir))
                  {
                     DWORD basedir_attribs;
                     RARCH_WARN("[Content]: Tried copying to cache directory, "
                        "but cache directory was not set or found. "
                        "Setting cache directory to root of writable app directory...\n");
                     strlcpy(new_basedir, uwp_dir_data, sizeof(new_basedir));
                     strlcat(new_basedir, "VFSCACHE\\", sizeof(new_basedir));
                     basedir_attribs = GetFileAttributes(new_basedir);
                     if (       (basedir_attribs == INVALID_FILE_ATTRIBUTES) 
                           || (!(basedir_attribs & FILE_ATTRIBUTE_DIRECTORY)))
                     {
                        if (!CreateDirectoryA(new_basedir, NULL))
                           strlcpy(new_basedir, uwp_dir_data, sizeof(new_basedir));
                     }
                  }
                  fill_pathname_join_special(new_path, new_basedir,
                     path_basename(content_path), sizeof(new_path));

                  mbstowcs(wnew_path, new_path, MAX_PATH);
                  /* TODO: This may fail on very large files...
                   * but copying large files is not a good idea anyway
                   * (This disclaimer is out dated but I don't want to remove it)*/
                  if (!CopyFileFromAppW(wcontent_path, wnew_path, false))
                  {
                     int err = GetLastError();
                     snprintf(msg, sizeof(msg), "%s \"%s\". (during copy read or write)\n",
                        msg_hash_to_str(MSG_COULD_NOT_READ_CONTENT_FILE),
                        content_path);
                     *error_string = strdup(msg);
                     return false;
                  }

                  content_path = content_file_list_append_temporary(
                     p_content->content_list, new_path);

                  used_vfs_fallback_copy = true;
               }
            }
#endif
            RARCH_LOG("[Content]: %s\n", msg_hash_to_str(
                  MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT));

            /* First content file is significant: need to
             * perform CRC calculation, but defer this
             * until value is used */
            if (i == 0)
            {
               /* If we have a media type, ignore CRC32 calculation. */
               if (first_content_type == RARCH_CONTENT_NONE)
               {
                  strlcpy(p_content->pending_rom_crc_path, content_path,
                        sizeof(p_content->pending_rom_crc_path));
                  p_content->flags |= CONTENT_ST_FLAG_PENDING_ROM_CRC;
               }
               else
                  p_content->rom_crc = 0;
            }
         }
      }

      /* Add current entry to content file list */
      if (!content_file_list_set_info(
            p_content->content_list,
            content_path, content_data, content_size,
            CONTENT_FILE_ATTR_GET_PERSISTENT(content->elems[i].attr), i))
      {
         RARCH_LOG("[Content]: Failed to process content file: \"%s\".\n", content_path);
         if (content_data)
            free((void*)content_data);
         *error_enum = MSG_FAILED_TO_LOAD_CONTENT;
         return false;
      }
   }

   /* Load content into core */
   load_info.content = content;
   load_info.special = special;
   load_info.info    = p_content->content_list->game_info;

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
      const char *first_content_path =
            p_content->content_list->entries[0].full_path;
      if (!string_is_empty(first_content_path))
      {
         if (first_content_type == RARCH_CONTENT_NONE)
         {
            rcheevos_load(p_content->content_list->game_info);
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
      union string_list_elem_attr attr;
      const char *content_path = path_get(RARCH_PATH_CONTENT);
      uint8_t flags            = content_get_flags();

      CONTENT_FILE_ATTR_RESET(attr);
      CONTENT_FILE_ATTR_SET_BLOCK_EXTRACT(attr, content_ctx->flags &
CONTENT_INFO_FLAG_BLOCK_EXTRACT);
      CONTENT_FILE_ATTR_SET_NEED_FULLPATH(attr, content_ctx->flags &
CONTENT_INFO_FLAG_NEED_FULLPATH);
      CONTENT_FILE_ATTR_SET_REQUIRED(attr, (!(flags &
               CONTENT_ST_FLAG_CORE_DOES_NOT_NEED_CONTENT)));

#if defined(HAVE_RUNAHEAD)
      /* If runahead is supported and we are not using
       * subsystems, content data buffer must *always*
       * be persistent, since user may toggle second
       * instance runahead at any time (and secondary
       * core initialisation requires valid data) */
      CONTENT_FILE_ATTR_SET_PERSISTENT(attr, true);
#endif

      if (string_is_empty(content_path))
      {
         if (  (flags & CONTENT_ST_FLAG_CORE_DOES_NOT_NEED_CONTENT)
             && content_ctx->flags 
             & CONTENT_INFO_FLAG_SET_SUPPORTS_NO_GAME_ENABLE)
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
   bool subsystem_path_is_empty               = path_is_empty(RARCH_PATH_SUBSYSTEM);
   bool ret                                   = subsystem_path_is_empty;
   const struct retro_subsystem_info *special = subsystem_path_is_empty ?
         NULL : content_file_init_subsystem(content_ctx->subsystem.data,
               content_ctx->subsystem.size, error_enum, error_string, &ret);

   if (!ret)
      return false;

   content_file_set_attributes(content, special, content_ctx, error_string);

   if (content->size > 0)
   {
      content_file_list_t *file_list = content_file_list_init(content->size);
      if (file_list)
      {
         p_content->content_list     = file_list;
         ret = content_file_load(p_content, content, content_ctx,
               error_enum, error_string, special);

         content_file_list_free_transient_data(p_content->content_list);
         return ret;
      }
   }

   if (!special)
   {
      *error_enum = MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT;
      return false;
   }
   return true;
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
      int *argc, char **argv)
{
   *argc = 0;
   argv[(*argc)++] = strldup("retroarch", sizeof("retroarch"));

   if (args->content_path)
   {
      RARCH_LOG("[Core]: Using content: \"%s\".\n", args->content_path);
      argv[(*argc)++] = strdup(args->content_path);
   }
#ifdef HAVE_MENU
   else
   {
      RARCH_LOG("[Core]: %s\n",
            msg_hash_to_str(MSG_NO_CONTENT_STARTING_DUMMY_CORE));
      argv[(*argc)++] = strldup("--menu", sizeof("--menu"));
   }
#endif

   if (args->sram_path)
   {
      argv[(*argc)++] = strldup("-s", sizeof("-s"));
      argv[(*argc)++] = strdup(args->sram_path);
   }

   if (args->state_path)
   {
      argv[(*argc)++] = strldup("-S", sizeof("-S"));
      argv[(*argc)++] = strdup(args->state_path);
   }

   if (args->config_path)
   {
      argv[(*argc)++] = strldup("-c", sizeof("-c"));
      argv[(*argc)++] = strdup(args->config_path);
   }

#ifdef HAVE_DYNAMIC
   if (args->libretro_path)
   {
      argv[(*argc)++] = strldup("-L", sizeof("-L"));
      argv[(*argc)++] = strdup(args->libretro_path);
   }
#endif

   if (args->flags & RARCH_MAIN_WRAP_FLAG_VERBOSE)
      argv[(*argc)++] = strldup("-v", sizeof("-v"));
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
   wrap_args->flags          = 0;
   wrap_args->argc           = 0;

   if (info->environ_get)
      info->environ_get(rarch_argc_ptr,
            rarch_argv_ptr, info->args, wrap_args);

   if (wrap_args->flags & RARCH_MAIN_WRAP_FLAG_TOUCHED)
   {
      content_load_init_wrap(wrap_args, &rarch_argc, rarch_argv);
      memcpy(argv_copy, rarch_argv, sizeof(rarch_argv));
      rarch_argv_ptr = (char**)rarch_argv;
      rarch_argc_ptr = (int*)&rarch_argc;
   }

   retroarch_ctl(RARCH_CTL_MAIN_DEINIT, NULL);

   wrap_args->argc = *rarch_argc_ptr;
   wrap_args->argv = rarch_argv_ptr;

   success         = retroarch_main_init(wrap_args->argc, wrap_args->argv);

   for (i = 0; i < ARRAY_SIZE(argv_copy); i++)
      free(argv_copy[i]);
   free(wrap_args);

   if (!success)
      return false;

   if (p_content->flags & CONTENT_ST_FLAG_PENDING_SUBSYSTEM_INIT)
   {
      command_event(CMD_EVENT_CORE_INIT, NULL);
      content_clear_subsystem();
   }

#ifdef HAVE_GFX_WIDGETS
#ifdef HAVE_CONFIGFILE
   /* If retroarch_main_init() returned true, we
    * can safely trigger a load content animation */
   if (gfx_widgets_ready())
   {
      /* Note: Have to read settings value here
       * (It will be invalid if we try to read
       *  it earlier...) */
      settings_t *settings              = config_get_ptr();
      bool show_load_content_animation  = settings && settings->bools.menu_show_load_content_animation;
      if (show_load_content_animation)
         gfx_widget_start_load_content_animation();
   }
#endif
#endif

#ifdef HAVE_MENU
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   menu_shader_manager_init();
#endif
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
      void *args, void *params_data)
{
   struct rarch_main_wrap *wrap_args = (struct rarch_main_wrap*)params_data;
   runloop_state_t       *runloop_st = runloop_state_get_ptr();
   rarch_system_info_t *sys_info     = &runloop_st->system;

   if (!wrap_args)
      return;

   if (sys_info->load_no_content)
      wrap_args->flags        |= RARCH_MAIN_WRAP_FLAG_NO_CONTENT;

   if (!retroarch_override_setting_is_set(
            RARCH_OVERRIDE_SETTING_VERBOSITY, NULL))
   {
      if (verbosity_is_enabled())
         wrap_args->flags     |= RARCH_MAIN_WRAP_FLAG_VERBOSE;
   }

   wrap_args->flags           |= RARCH_MAIN_WRAP_FLAG_TOUCHED;
   wrap_args->config_path      = NULL;
   wrap_args->sram_path        = NULL;
   wrap_args->state_path       = NULL;
   wrap_args->content_path     = NULL;

   if (!path_is_empty(RARCH_PATH_CONFIG))
      wrap_args->config_path   = path_get(RARCH_PATH_CONFIG);
   if (!string_is_empty(dir_get_ptr(RARCH_DIR_SAVEFILE)))
      wrap_args->sram_path     = dir_get_ptr(RARCH_DIR_SAVEFILE);
   if (!string_is_empty(dir_get_ptr(RARCH_DIR_SAVESTATE)))
      wrap_args->state_path    = dir_get_ptr(RARCH_DIR_SAVESTATE);
   if (!path_is_empty(RARCH_PATH_CONTENT))
      wrap_args->content_path  = path_get(RARCH_PATH_CONTENT);
   if (!retroarch_override_setting_is_set(
            RARCH_OVERRIDE_SETTING_LIBRETRO, NULL))
      wrap_args->libretro_path = string_is_empty(path_get(RARCH_PATH_CORE)) 
         ? NULL
         : path_get(RARCH_PATH_CORE);
}

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
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   uint8_t flags               = content_get_flags();

   /* Push entry to top of history playlist */
   if (     (flags & CONTENT_ST_FLAG_IS_INITED) 
         || (flags & CONTENT_ST_FLAG_CORE_DOES_NOT_NEED_CONTENT))
   {
      char tmp[PATH_MAX_LENGTH];
      const char *path_content       = path_get(RARCH_PATH_CONTENT);
      struct retro_system_info *info = &runloop_st->system.info;

      if (!string_is_empty(path_content))
      {
         strlcpy(tmp, path_content, sizeof(tmp));
         /* Path can be relative here.
          * Ensure we're pushing absolute path. */
         if (!launched_from_menu)
            path_resolve_realpath(tmp, sizeof(tmp), true);
      }
      else
         tmp[0] = '\0';

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
                  menu_handle_t *menu = menu_state_get_ptr()->driver_data;
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

         if (!string_is_empty(runloop_st->name.label))
            label = runloop_st->name.label;

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
            entry.entry_slot      = runloop_st->entry_state_slot;
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

#ifndef HAVE_DYNAMIC
/**
 * task_push_to_history_list_from_playlist_pre_load_static:
 *
 * Will push content from the currently selected playlist
 * to the history playlist before loading content. Required
 * on static platforms, where essential playlist information
 * (label, db_name, etc.) would otherwise be lost when the
 * core is forked.
 **/
static bool task_push_to_history_list_from_playlist_pre_load_static(
      const char *content_path,
      const char *core)
{
   const char *core_path     = NULL;
   const char *core_name     = NULL;
   const char *label         = NULL;
   const char *crc32         = NULL;
   const char *db_name       = NULL;
   unsigned ss_entry_slot    = 0;
   playlist_t *playlist_hist = g_defaults.content_history;
   settings_t *settings      = config_get_ptr();
#ifdef HAVE_MENU
   menu_handle_t *menu       = menu_state_get_ptr()->driver_data;
#endif

   if (!settings ||
       !settings->bools.history_list_enable ||
       string_is_empty(content_path))
      return false;

   switch (path_is_media_type(content_path))
   {
      case RARCH_CONTENT_MOVIE:
#ifdef HAVE_FFMPEG
         playlist_hist       = g_defaults.video_history;
         core_name           = "movieplayer";
         core_path           = "builtin";
#endif
         break;
      case RARCH_CONTENT_MUSIC:
         playlist_hist       = g_defaults.music_history;
         core_name           = "musicplayer";
         core_path           = "builtin";
         break;
      case RARCH_CONTENT_IMAGE:
#ifdef HAVE_IMAGEVIEWER
         playlist_hist       = g_defaults.image_history;
         core_name           = "imageviewer";
         core_path           = "builtin";
#endif
         break;
      default:
      {
         core_info_t *core_info = NULL;

         if (!string_is_empty(core) &&
             core_info_find(core, &core_info))
         {
            /* Set core path and core display name */
            core_path = core_info->path;
            core_name = core_info->display_name;

#ifdef HAVE_MENU
            /* Read remaining information from currently selected
             * playlist entry (label, database name, checksum,
             * save state slot) */
            if (menu)
            {
               playlist_t *playlist_curr = playlist_get_cached();

               if (playlist_index_is_valid(playlist_curr,
                     menu->rpl_entry_selection_ptr,
                     content_path, core_path))
               {
                  const struct playlist_entry *pl_entry = NULL;

                  playlist_get_index(playlist_curr,
                        menu->rpl_entry_selection_ptr,
                        &pl_entry);

                  if (pl_entry)
                  {
                     label         = pl_entry->label;
                     crc32         = pl_entry->crc32;
                     ss_entry_slot = pl_entry->entry_slot;
                  }

                  playlist_get_db_name(playlist_curr,
                        menu->rpl_entry_selection_ptr,
                        &db_name);
               }
            }
#endif
         }

         break;
      }
   }

   if (!string_is_empty(core_path) &&
       playlist_hist)
   {
      struct playlist_entry new_entry = {0};

      /* The push function reads our entry as const,
       * so these casts are safe */
      new_entry.path       = (char*)content_path;
      new_entry.label      = (char*)label;
      new_entry.core_path  = (char*)core_path;
      new_entry.core_name  = (char*)core_name;
      new_entry.crc32      = (char*)crc32;
      new_entry.db_name    = (char*)db_name;
      new_entry.entry_slot = ss_entry_slot;

      /* TODO/FIXME: Subsystems are not properly supported
       * on static platforms, so exclude the following:
       * - subsystem_ident
       * - subsystem_name
       * - subsystem_roms */

      command_playlist_push_write(playlist_hist, &new_entry);
      return true;
   }

   return false;
}
#endif

#ifdef HAVE_MENU
static bool command_event_cmd_exec(
      content_state_t *p_content,
      const char *data,
      content_information_ctx_t *content_ctx,
      bool launched_from_cli
      )
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

   RARCH_LOG("[Content]: Updating firmware status for: \"%s\" on \"%s\".\n",
         core_info->path,
         firmware_info.directory.system);

   retroarch_ctl(RARCH_CTL_UNSET_MISSING_BIOS, NULL);

   core_info_list_update_missing_firmware(&firmware_info,
         &set_missing_firmware);

   if (set_missing_firmware)
      retroarch_ctl(RARCH_CTL_SET_MISSING_BIOS, NULL);

   if (
            (content_ctx->flags & CONTENT_INFO_FLAG_BIOS_IS_MISSING)
         && (content_ctx->flags & CONTENT_INFO_FLAG_CHECK_FW_BEFORE_LOADING))
   {
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_FIRMWARE),
            100, 500, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_LOG("[Content]: Load content blocked. Reason: %s\n",
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
   settings_t *settings                       = config_get_ptr();
   runloop_state_t *runloop_st                = runloop_state_get_ptr();
   rarch_system_info_t *sys_info              = &runloop_st->system;
   const char *path_dir_system                = settings->paths.directory_system;
   bool check_firmware_before_loading         = settings->bools.check_firmware_before_loading;
   uint16_t rarch_flags                       = retroarch_get_flags();

   if (!content_info)
      return false;

   content_ctx.flags     = 0;

   if (check_firmware_before_loading)
      content_ctx.flags |= CONTENT_INFO_FLAG_CHECK_FW_BEFORE_LOADING;
#ifdef HAVE_PATCH
   if (rarch_flags & RARCH_FLAGS_IPS_PREF)
      content_ctx.flags |= CONTENT_INFO_FLAG_IS_IPS_PREF;
   if (rarch_flags & RARCH_FLAGS_BPS_PREF)
      content_ctx.flags |= CONTENT_INFO_FLAG_IS_BPS_PREF;
   if (rarch_flags & RARCH_FLAGS_UPS_PREF)
      content_ctx.flags |= CONTENT_INFO_FLAG_IS_UPS_PREF;
   if (runloop_st->flags & RUNLOOP_FLAG_PATCH_BLOCKED)
      content_ctx.flags |= CONTENT_INFO_FLAG_PATCH_IS_BLOCKED;
#endif
   if (retroarch_ctl(RARCH_CTL_IS_MISSING_BIOS, NULL))
      content_ctx.flags |= CONTENT_INFO_FLAG_BIOS_IS_MISSING;
   content_ctx.directory_system               = NULL;
   content_ctx.directory_cache                = NULL;
   content_ctx.name_ips                       = NULL;
   content_ctx.name_bps                       = NULL;
   content_ctx.name_ups                       = NULL;
   content_ctx.valid_extensions               = NULL;

   content_ctx.subsystem.data                 = NULL;
   content_ctx.subsystem.size                 = 0;

   if (!string_is_empty(runloop_st->name.ips))
      content_ctx.name_ips                 = strdup(runloop_st->name.ips);
   if (!string_is_empty(runloop_st->name.bps))
      content_ctx.name_bps                 = strdup(runloop_st->name.bps);
   if (!string_is_empty(runloop_st->name.ups))
      content_ctx.name_ups                 = strdup(runloop_st->name.ups);

   if (!string_is_empty(path_dir_system))
      content_ctx.directory_system            = strdup(path_dir_system);

   if (!content_info->environ_get)
      content_info->environ_get = menu_content_environment_get;

   /* Clear content path */
   path_clear(RARCH_PATH_CONTENT);

   /* Preliminary stuff that has to be done before we
    * load the actual content. Can differ per mode. */
   sys_info->load_no_content = false;
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   task_queue_deinit();
   retroarch_init_task_queue();

   /* Loads content into currently selected core. */
   if ((ret = content_load(content_info, p_content)))
      task_push_to_history_list(p_content, false, false, false);

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
   settings_t *settings                       = config_get_ptr();
   runloop_state_t *runloop_st                = runloop_state_get_ptr();
   rarch_system_info_t *sys_info              = &runloop_st->system;
   const char *path_dir_system                = settings->paths.directory_system;
#ifndef HAVE_DYNAMIC
   bool force_core_reload                     = settings->bools.always_reload_core_on_run_content;
#endif
   bool check_firmware_before_loading         = settings->bools.check_firmware_before_loading;
   uint16_t rarch_flags                       = retroarch_get_flags();

   content_ctx.flags     = 0;

   if (check_firmware_before_loading)
      content_ctx.flags |= CONTENT_INFO_FLAG_CHECK_FW_BEFORE_LOADING;
#ifdef HAVE_PATCH
   if (rarch_flags & RARCH_FLAGS_IPS_PREF)
      content_ctx.flags |= CONTENT_INFO_FLAG_IS_IPS_PREF;
   if (rarch_flags & RARCH_FLAGS_BPS_PREF)
      content_ctx.flags |= CONTENT_INFO_FLAG_IS_BPS_PREF;
   if (rarch_flags & RARCH_FLAGS_UPS_PREF)
      content_ctx.flags |= CONTENT_INFO_FLAG_IS_UPS_PREF;
   if (runloop_st->flags & RUNLOOP_FLAG_PATCH_BLOCKED)
      content_ctx.flags |= CONTENT_INFO_FLAG_PATCH_IS_BLOCKED;
#endif
   if (retroarch_ctl(RARCH_CTL_IS_MISSING_BIOS, NULL))
      content_ctx.flags |= CONTENT_INFO_FLAG_BIOS_IS_MISSING;
   content_ctx.directory_system               = NULL;
   content_ctx.directory_cache                = NULL;
   content_ctx.name_ips                       = NULL;
   content_ctx.name_bps                       = NULL;
   content_ctx.name_ups                       = NULL;
   content_ctx.valid_extensions               = NULL;

   content_ctx.subsystem.data                 = NULL;
   content_ctx.subsystem.size                 = 0;

   if (!string_is_empty(runloop_st->name.ips))
      content_ctx.name_ips                    = strdup(runloop_st->name.ips);
   if (!string_is_empty(runloop_st->name.bps))
      content_ctx.name_bps                    = strdup(runloop_st->name.bps);
   if (!string_is_empty(runloop_st->name.ups))
      content_ctx.name_ups                    = strdup(runloop_st->name.ups);
   if (label)
      strlcpy(runloop_st->name.label, label, sizeof(runloop_st->name.label));
   else
      runloop_st->name.label[0] = '\0';

   if (!string_is_empty(path_dir_system))
      content_ctx.directory_system            = strdup(path_dir_system);

   /* Is content required by this core? */
   if (fullpath)
      sys_info->load_no_content               = false;
   else
      sys_info->load_no_content               = true;

#ifndef HAVE_DYNAMIC
   /* Check whether specified core is already loaded
    * > If so, content can be launched directly with
    *   the currently loaded core */
   if (!force_core_reload &&
       retroarch_ctl(RARCH_CTL_IS_CORE_LOADED, (void*)core_path))
   {
      if (!content_info->environ_get)
         content_info->environ_get = menu_content_environment_get;

      /* Register content path */
      path_clear(RARCH_PATH_CONTENT);
      if (!string_is_empty(fullpath))
         path_set(RARCH_PATH_CONTENT, fullpath);

      /* Load content */
      if (!(ret = content_load(content_info, p_content)))
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
#else
   /* On targets that do not support dynamic core loading,
    * must push content to the history list before calling
    * command_event_cmd_exec() or playlist metadata will
    * be lost */
   task_push_to_history_list_from_playlist_pre_load_static(
         fullpath, core_path);
#endif

   /* Load content
    * > On targets that do not support dynamic core loading,
    *   command_event_cmd_exec() will fork a new instance */
   if (!(ret = command_event_cmd_exec(p_content,
         fullpath, &content_ctx, false)))
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
   retroarch_ctl(RARCH_CTL_SET_SHUTDOWN, NULL);
   retroarch_menu_running_finished(true);
#endif

end:
   /* Handle load content failure */
   if (!ret)
      retroarch_menu_running();

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
   uint16_t rarch_flags;
   content_information_ctx_t content_ctx;
   bool ret                           = true;
   content_state_t *p_content         = content_state_get_ptr();
   settings_t *settings               = config_get_ptr();
   runloop_state_t *runloop_st        = runloop_state_get_ptr();
   const char *path_dir_system        = settings->paths.directory_system;
   bool check_firmware_before_loading = settings->bools.check_firmware_before_loading;

   if (!content_info)
      return false;

   rarch_flags                        = retroarch_get_flags();
   content_ctx.flags                  = 0;

   if (check_firmware_before_loading)
      content_ctx.flags |= CONTENT_INFO_FLAG_CHECK_FW_BEFORE_LOADING;
#ifdef HAVE_PATCH
   if (rarch_flags & RARCH_FLAGS_IPS_PREF)
      content_ctx.flags |= CONTENT_INFO_FLAG_IS_IPS_PREF;
   if (rarch_flags & RARCH_FLAGS_BPS_PREF)
      content_ctx.flags |= CONTENT_INFO_FLAG_IS_BPS_PREF;
   if (rarch_flags & RARCH_FLAGS_UPS_PREF)
      content_ctx.flags |= CONTENT_INFO_FLAG_IS_UPS_PREF;
   if (runloop_st->flags & RUNLOOP_FLAG_PATCH_BLOCKED)
      content_ctx.flags |= CONTENT_INFO_FLAG_PATCH_IS_BLOCKED;
#endif
   if (retroarch_ctl(RARCH_CTL_IS_MISSING_BIOS, NULL))
      content_ctx.flags |= CONTENT_INFO_FLAG_BIOS_IS_MISSING;
   content_ctx.directory_system               = NULL;
   content_ctx.directory_cache                = NULL;
   content_ctx.name_ips                       = NULL;
   content_ctx.name_bps                       = NULL;
   content_ctx.name_ups                       = NULL;
   content_ctx.valid_extensions               = NULL;

   content_ctx.subsystem.data                 = NULL;
   content_ctx.subsystem.size                 = 0;

   if (!string_is_empty(runloop_st->name.ips))
      content_ctx.name_ips                 = strdup(runloop_st->name.ips);
   if (!string_is_empty(runloop_st->name.bps))
      content_ctx.name_bps                 = strdup(runloop_st->name.bps);
   if (!string_is_empty(runloop_st->name.ups))
      content_ctx.name_ups                 = strdup(runloop_st->name.ups);

   if (!string_is_empty(path_dir_system))
      content_ctx.directory_system            = strdup(path_dir_system);

   if (!content_info->environ_get)
      content_info->environ_get = menu_content_environment_get;

   /* Clear content path */
   path_clear(RARCH_PATH_CONTENT);

   /* Preliminary stuff that has to be done before we
    * load the actual content. Can differ per mode. */
   runloop_set_current_core_type(CORE_TYPE_PLAIN, true);

   /* Load content */
   if (firmware_update_status(&content_ctx))
      goto end;

   /* Loads content into currently selected core.
    * Note that 'content_load()' can fail and yet still
    * return 'true'... In this case, the dummy core
    * will be loaded; the 'start core' operation can
    * therefore only be considered successful if the
    * dummy core is not running following 'content_load()' */
   if (!(ret = content_load(content_info, p_content)) ||
       !(ret = (runloop_st->current_core_type != CORE_TYPE_DUMMY)))
   {
#ifdef HAVE_MENU
      retroarch_menu_running();
#endif
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
   runloop_set_current_core_type(type, true);

   return true;
}

#ifdef HAVE_MENU
bool task_push_load_contentless_core_from_menu(
      const char *core_path)
{
#if defined(HAVE_DYNAMIC)
   content_ctx_info_t content_info       = {0};
#endif
   content_information_ctx_t content_ctx = {0};
   content_state_t *p_content            = content_state_get_ptr();
   bool ret                              = true;
#if defined(HAVE_DYNAMIC)
   runloop_state_t *runloop_st           = runloop_state_get_ptr();
#endif
   settings_t *settings                  = config_get_ptr();
   const char *path_dir_system           = settings->paths.directory_system;
   bool check_firmware_before_loading    = settings->bools.check_firmware_before_loading;
   bool flush_menu                       = true;
   const char *menu_label                = NULL;

   if (string_is_empty(core_path))
      return false;

   content_ctx.flags     = 0;

   if (check_firmware_before_loading)
      content_ctx.flags |= CONTENT_INFO_FLAG_CHECK_FW_BEFORE_LOADING;
   if (retroarch_ctl(RARCH_CTL_IS_MISSING_BIOS, NULL))
      content_ctx.flags |= CONTENT_INFO_FLAG_BIOS_IS_MISSING;
   if (!string_is_empty(path_dir_system))
      content_ctx.directory_system           = strdup(path_dir_system);

   /* Set core path */
   path_set(RARCH_PATH_CORE, core_path);

   /* Clear content path */
   path_clear(RARCH_PATH_CONTENT);

#if defined(HAVE_DYNAMIC)
   content_info.environ_get                  = menu_content_environment_get;
   /* Load core */
   command_event(CMD_EVENT_LOAD_CORE, NULL);

   runloop_set_current_core_type(CORE_TYPE_PLAIN, true);

   if (firmware_update_status(&content_ctx))
      goto end;

   /* Loads content into currently selected core.
    * Note that 'content_load()' can fail and yet still
    * return 'true'... In this case, the dummy core
    * will be loaded; the 'start core' operation can
    * therefore only be considered successful if the
    * dummy core is not running following 'content_load()' */
   if (!(ret = content_load(&content_info, p_content)) ||
       !(ret = (runloop_st->current_core_type != CORE_TYPE_DUMMY)))
   {
      retroarch_menu_running();
      goto end;
   }
#else
   /* TODO/FIXME: Static builds do not support running
    * a core directly from the 'command line' without
    * supplying a content path, so this *will not work*.
    * In order to support this functionality, the '-L'
    * command line argument must be enabled for static
    * builds to inform the frontend that the core should
    * run automatically on launch. We will leave this
    * non-functional code here as a place-marker for
    * future devs who may wish to implement this... */
   command_event_cmd_exec(p_content,
         path_get(RARCH_PATH_CONTENT), &content_ctx,
         false);
   command_event(CMD_EVENT_QUIT, NULL);
#endif

   /* Push quick menu onto menu stack */
   menu_entries_get_last_stack(NULL, &menu_label, NULL, NULL, NULL);

   if (string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_CONTENTLESS_CORES_TAB)) ||
       string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CONTENTLESS_CORES_LIST)))
      flush_menu = false;

   menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, &flush_menu);

#ifdef HAVE_DYNAMIC
end:
#endif
   if (content_ctx.directory_system)
      free(content_ctx.directory_system);

   return ret;
}

bool task_push_load_content_with_new_core_from_menu(
      const char *core_path,
      const char *fullpath,
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data)
{
   uint16_t rarch_flags;
   content_information_ctx_t content_ctx;
   content_state_t                 *p_content = content_state_get_ptr();
   bool ret                                   = true;
   settings_t *settings                       = config_get_ptr();
   runloop_state_t *runloop_st                = runloop_state_get_ptr();
   bool check_firmware_before_loading         = settings->bools.check_firmware_before_loading;
   const char *path_dir_system                = settings->paths.directory_system;
#ifndef HAVE_DYNAMIC
   bool force_core_reload                     = settings->bools.always_reload_core_on_run_content;

   /* Check whether specified core is already loaded
    * > If so, we can skip loading the core and
    *   just load the content directly */
   if (!force_core_reload &&
       (type == CORE_TYPE_PLAIN) &&
       retroarch_ctl(RARCH_CTL_IS_CORE_LOADED, (void*)core_path))
      return task_push_load_content_with_core(
            fullpath, content_info,
            type, cb, user_data);
#endif

   rarch_flags                        = retroarch_get_flags();
   content_ctx.flags                  = 0;

   if (check_firmware_before_loading)
      content_ctx.flags |= CONTENT_INFO_FLAG_CHECK_FW_BEFORE_LOADING;
#ifdef HAVE_PATCH
   if (rarch_flags & RARCH_FLAGS_IPS_PREF)
      content_ctx.flags |= CONTENT_INFO_FLAG_IS_IPS_PREF;
   if (rarch_flags & RARCH_FLAGS_BPS_PREF)
      content_ctx.flags |= CONTENT_INFO_FLAG_IS_BPS_PREF;
   if (rarch_flags & RARCH_FLAGS_UPS_PREF)
      content_ctx.flags |= CONTENT_INFO_FLAG_IS_UPS_PREF;
   if (runloop_st->flags & RUNLOOP_FLAG_PATCH_BLOCKED)
      content_ctx.flags |= CONTENT_INFO_FLAG_PATCH_IS_BLOCKED;
#endif
   if (retroarch_ctl(RARCH_CTL_IS_MISSING_BIOS, NULL))
      content_ctx.flags |= CONTENT_INFO_FLAG_BIOS_IS_MISSING;
   content_ctx.directory_system               = NULL;
   content_ctx.directory_cache                = NULL;
   content_ctx.name_ips                       = NULL;
   content_ctx.name_bps                       = NULL;
   content_ctx.name_ups                       = NULL;
   content_ctx.valid_extensions               = NULL;

   content_ctx.subsystem.data                 = NULL;
   content_ctx.subsystem.size                 = 0;

   if (!string_is_empty(runloop_st->name.ips))
      content_ctx.name_ips                 = strdup(runloop_st->name.ips);
   if (!string_is_empty(runloop_st->name.bps))
      content_ctx.name_bps                 = strdup(runloop_st->name.bps);
   if (!string_is_empty(runloop_st->name.ups))
      content_ctx.name_ups                 = strdup(runloop_st->name.ups);

   runloop_st->name.label[0]                   = '\0';

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
      retroarch_menu_running();
      goto end;
   }

   task_push_to_history_list(p_content, true, false, false);
#else
   command_event_cmd_exec(p_content,
         path_get(RARCH_PATH_CONTENT), &content_ctx,
         false);
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
   runloop_state_t *runloop_st                = runloop_state_get_ptr();
   rarch_system_info_t *sys_info              = &runloop_st->system;
   settings_t *settings                       = config_get_ptr();
   bool check_firmware_before_loading         = settings->bools.check_firmware_before_loading;
   bool set_supports_no_game_enable           = settings->bools.set_supports_no_game_enable;
   const char *path_dir_system                = settings->paths.directory_system;
   const char *path_dir_cache                 = settings->paths.directory_cache;

   uint16_t rarch_flags                       = retroarch_get_flags();
   content_ctx.flags                          = 0;

   if (check_firmware_before_loading)
      content_ctx.flags |= CONTENT_INFO_FLAG_CHECK_FW_BEFORE_LOADING;
#ifdef HAVE_PATCH
   if (rarch_flags & RARCH_FLAGS_IPS_PREF)
      content_ctx.flags |= CONTENT_INFO_FLAG_IS_IPS_PREF;
   if (rarch_flags & RARCH_FLAGS_BPS_PREF)
      content_ctx.flags |= CONTENT_INFO_FLAG_IS_BPS_PREF;
   if (rarch_flags & RARCH_FLAGS_UPS_PREF)
      content_ctx.flags |= CONTENT_INFO_FLAG_IS_UPS_PREF;
   if (runloop_st->flags & RUNLOOP_FLAG_PATCH_BLOCKED)
      content_ctx.flags |= CONTENT_INFO_FLAG_PATCH_IS_BLOCKED;
#endif
   if (retroarch_ctl(RARCH_CTL_IS_MISSING_BIOS, NULL))
      content_ctx.flags |= CONTENT_INFO_FLAG_BIOS_IS_MISSING;
   content_ctx.directory_system               = NULL;
   content_ctx.directory_cache                = NULL;
   content_ctx.name_ips                       = NULL;
   content_ctx.name_bps                       = NULL;
   content_ctx.name_ups                       = NULL;
   content_ctx.valid_extensions               = NULL;

   content_ctx.subsystem.data                 = NULL;
   content_ctx.subsystem.size                 = 0;

   if (sys_info)
   {
      struct retro_system_info *system        = &runloop_st->system.info;

      if (set_supports_no_game_enable)
         content_ctx.flags |= CONTENT_INFO_FLAG_SET_SUPPORTS_NO_GAME_ENABLE;

      if (!string_is_empty(path_dir_cache))
         content_ctx.directory_cache          = strdup(path_dir_cache);
      if (!string_is_empty(system->valid_extensions))
         content_ctx.valid_extensions         = strdup(system->valid_extensions);

      if (system->block_extract)
         content_ctx.flags |= CONTENT_INFO_FLAG_BLOCK_EXTRACT;
      if (system->need_fullpath)
         content_ctx.flags |= CONTENT_INFO_FLAG_NEED_FULLPATH;

      content_ctx.subsystem.data              = sys_info->subsystem.data;
      content_ctx.subsystem.size              = sys_info->subsystem.size;
   }

   if (!string_is_empty(runloop_st->name.ips))
      content_ctx.name_ips                 = strdup(runloop_st->name.ips);
   if (!string_is_empty(runloop_st->name.bps))
      content_ctx.name_bps                 = strdup(runloop_st->name.bps);
   if (!string_is_empty(runloop_st->name.ups))
      content_ctx.name_ups                 = strdup(runloop_st->name.ups);

   if (!string_is_empty(path_dir_system))
      content_ctx.directory_system            = strdup(path_dir_system);

   if (!content_info->environ_get)
      content_info->environ_get = menu_content_environment_get;

   if (firmware_update_status(&content_ctx))
      goto end;

#ifdef HAVE_PRESENCE
   {
      presence_userdata_t userdata;
      userdata.status = PRESENCE_NETPLAY_NETPLAY_STOPPED;
      command_event(CMD_EVENT_PRESENCE_UPDATE, &userdata);
      userdata.status = PRESENCE_MENU;
      command_event(CMD_EVENT_PRESENCE_UPDATE, &userdata);
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

   return ret;
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
   runloop_state_t *runloop_st = runloop_state_get_ptr();
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

   if (label)
      strlcpy(runloop_st->name.label, label, sizeof(runloop_st->name.label));
   else
      runloop_st->name.label[0] = '\0';

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
   runloop_set_current_core_type(type, true);

   /* Load content */
#ifdef HAVE_MENU
   if (!task_load_content_internal(content_info, true, false, false))
   {
      retroarch_menu_running();
      return false;
   }
   /* Push quick menu onto menu stack */
   menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, NULL);
   return true;
#else
   return task_load_content_internal(content_info, true, false, false);
#endif
}

bool task_push_load_content_with_core(
      const char *fullpath,
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data)
{
   path_set(RARCH_PATH_CONTENT, fullpath);
   /* Load content */
#ifdef HAVE_MENU
   if (!task_load_content_internal(content_info, true, false, false))
   {
      retroarch_menu_running();
      return false;
   }
   /* Push quick menu onto menu stack */
   if (type != CORE_TYPE_DUMMY)
      menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, NULL);
   return true;
#else
   return task_load_content_internal(content_info, true, false, false);
#endif
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
   path_set(RARCH_PATH_CONTENT, fullpath);

   /* Load content */
#ifdef HAVE_MENU
   if (!task_load_content_internal(content_info, true, false, false))
   {
      retroarch_menu_running();
      return false;
   }
   /* Push quick menu onto menu stack */
   if (type != CORE_TYPE_DUMMY)
      menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, NULL);
   return true;
#else
   return task_load_content_internal(content_info, true, false, false);
#endif
}


bool task_push_load_subsystem_with_core(
      const char *fullpath,
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data)
{
   content_state_t  *p_content = content_state_get_ptr();

   p_content->flags |= CONTENT_ST_FLAG_PENDING_SUBSYSTEM_INIT;
   /* Load content */
#ifdef HAVE_MENU
   if (!task_load_content_internal(content_info, true, false, false))
   {
      retroarch_menu_running();
      return false;
   }
   /* Push quick menu onto menu stack */
   if (type != CORE_TYPE_DUMMY)
      menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, NULL);
   return true;
#else
   return task_load_content_internal(content_info, true, false, false);
#endif
}

uint8_t content_get_flags(void)
{
   content_state_t  *p_content = content_state_get_ptr();
   return p_content->flags;
}

/* Clears the pending subsystem rom buffer*/
void content_clear_subsystem(void)
{
   unsigned i;
   content_state_t  *p_content          = content_state_get_ptr();

   p_content->pending_subsystem_rom_id  = 0;
   p_content->flags                    &= ~CONTENT_ST_FLAG_PENDING_SUBSYSTEM_INIT;

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
   const struct retro_subsystem_info *subsystem = NULL;
   runloop_state_t                  *runloop_st = runloop_state_get_ptr();
   content_state_t  *p_content                  = content_state_get_ptr();
   rarch_system_info_t                  *system = &runloop_st->system;

   /* Core fully loaded, use the subsystem data */
   if (system->subsystem.data)
      subsystem                                 = system->subsystem.data + idx;
   /* Core not loaded completely, use the data we peeked on load core */
   else
      subsystem                                 = runloop_st->subsystem_data + idx;

   p_content->pending_subsystem_id              = idx;

   if (      subsystem 
         && (runloop_st->subsystem_current_count > 0))
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
   runloop_state_t         *runloop_st = runloop_state_get_ptr();
   rarch_system_info_t         *system = &runloop_st->system;
   unsigned i                          = 0;
   /* Core not loaded completely, use the data we peeked on load core */
   const struct retro_subsystem_info 
      *subsystem                       = runloop_st->subsystem_data;

   /* Core fully loaded, use the subsystem data */
   if (system->subsystem.data)
      subsystem                        = system->subsystem.data;

   for (i = 0; i < runloop_st->subsystem_current_count; i++, subsystem++)
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
   unsigned i                                   = 0;
   runloop_state_t *runloop_st                  = runloop_state_get_ptr();
   rarch_system_info_t                  *system = &runloop_st->system;
   /* Core not loaded completely, use the data we peeked on load core */
   const struct retro_subsystem_info *subsystem = runloop_st->subsystem_data;

   /* Core fully loaded, use the subsystem data */
   if (system->subsystem.data)
      subsystem = system->subsystem.data;

   for (i = 0; i < runloop_st->subsystem_current_count; i++, subsystem++)
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
         " %s Content ID: %d, Content Path: \"%s\".\n",
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
   p_content->flags |= CONTENT_ST_FLAG_CORE_DOES_NOT_NEED_CONTENT;
}

void content_unset_does_not_need_content(void)
{
   content_state_t *p_content = content_state_get_ptr();
   p_content->flags &= ~CONTENT_ST_FLAG_CORE_DOES_NOT_NEED_CONTENT;
}

#ifndef CRC32_BUFFER_SIZE
#define CRC32_BUFFER_SIZE 1048576
#endif

#ifndef CRC32_MAX_MB
#define CRC32_MAX_MB 64
#endif

/**
 * Calculate a CRC32 from the first part of the given file.
 * "first part" being the first (CRC32_BUFFER_SIZE * CRC32_MAX_MB)
 * bytes.
 *
 * @return The calculated CRC32 hash, or 0 if there was an error.
 */
static uint32_t file_crc32(uint32_t crc, const char *path)
{
   unsigned i;
   RFILE *file        = NULL;
   unsigned char *buf = NULL;
   if (!path)
      return 0;

   if (!(file = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, 0)))
      return 0;

   if (!(buf = (unsigned char*)malloc(CRC32_BUFFER_SIZE)))
   {
      filestream_close(file);
      return 0;
   }

   for (i = 0; i < CRC32_MAX_MB; i++)
   {
      int64_t nread = filestream_read(file, buf, CRC32_BUFFER_SIZE);
      if (nread < 0)		
      {
         free(buf);
         filestream_close(file);
         return 0;
      }

      crc = encoding_crc32(crc, buf, (size_t)nread);
      if (filestream_eof(file))
         break;
   }
   free(buf);
   filestream_close(file);
   return crc;
}

uint32_t content_get_crc(void)
{
   content_state_t *p_content = content_state_get_ptr();
   if (p_content->flags & CONTENT_ST_FLAG_PENDING_ROM_CRC)
   {
      p_content->flags &= ~CONTENT_ST_FLAG_PENDING_ROM_CRC;
      /* TODO/FIXME - file_crc32 has a 64MB max limit -
       * get rid of this function and find a better
       * way to calculate CRC based on the file */
      p_content->rom_crc           = file_crc32(0,
            (const char*)p_content->pending_rom_crc_path);
      RARCH_LOG("[Content]: CRC32: 0x%x.\n",
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
   return ((p_content->flags & CONTENT_ST_FLAG_IS_INITED) > 0);
}

void content_deinit(void)
{
   content_state_t *p_content = content_state_get_ptr();

   content_file_override_free(p_content);
   content_file_list_free(p_content->content_list);

   p_content->content_list    = NULL;
   p_content->rom_crc         = 0;
   p_content->flags          &= ~(CONTENT_ST_FLAG_PENDING_ROM_CRC
                              | CONTENT_ST_FLAG_CORE_DOES_NOT_NEED_CONTENT
                              | CONTENT_ST_FLAG_IS_INITED);
}

/* Set environment variables before a subsystem load */
void content_set_subsystem_info(void)
{
   content_state_t *p_content = content_state_get_ptr();
   if (!(p_content->flags & CONTENT_ST_FLAG_PENDING_SUBSYSTEM_INIT))
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
   enum msg_hash_enums error_enum     = MSG_UNKNOWN;
   content_state_t *p_content         = content_state_get_ptr();

   bool ret                           = true;
   char *error_string                 = NULL;
   runloop_state_t *runloop_st        = runloop_state_get_ptr();
   rarch_system_info_t *sys_info      = &runloop_st->system;
   settings_t *settings               = config_get_ptr();
   bool check_firmware_before_loading = settings->bools.check_firmware_before_loading;
   bool set_supports_no_game_enable   = settings->bools.set_supports_no_game_enable;
   const char *path_dir_system        = settings->paths.directory_system;
   const char *path_dir_cache         = settings->paths.directory_cache;
   uint16_t rarch_flags               = retroarch_get_flags();

   content_file_list_free(p_content->content_list);
   p_content->content_list            = NULL;

   content_ctx.flags                  = 0;

   if (check_firmware_before_loading)
      content_ctx.flags |= CONTENT_INFO_FLAG_CHECK_FW_BEFORE_LOADING;
#ifdef HAVE_PATCH
   if (rarch_flags & RARCH_FLAGS_IPS_PREF)
      content_ctx.flags |= CONTENT_INFO_FLAG_IS_IPS_PREF;
   if (rarch_flags & RARCH_FLAGS_BPS_PREF)
      content_ctx.flags |= CONTENT_INFO_FLAG_IS_BPS_PREF;
   if (rarch_flags & RARCH_FLAGS_UPS_PREF)
      content_ctx.flags |= CONTENT_INFO_FLAG_IS_UPS_PREF;
   if (runloop_st->flags & RUNLOOP_FLAG_PATCH_BLOCKED)
      content_ctx.flags |= CONTENT_INFO_FLAG_PATCH_IS_BLOCKED;
#endif
   content_ctx.directory_system               = NULL;
   content_ctx.directory_cache                = NULL;
   content_ctx.name_ips                       = NULL;
   content_ctx.name_bps                       = NULL;
   content_ctx.name_ups                       = NULL;
   content_ctx.valid_extensions               = NULL;

   content_ctx.subsystem.data                 = NULL;
   content_ctx.subsystem.size                 = 0;

   if (!string_is_empty(runloop_st->name.ips))
      content_ctx.name_ips                 = strdup(runloop_st->name.ips);
   if (!string_is_empty(runloop_st->name.bps))
      content_ctx.name_bps                 = strdup(runloop_st->name.bps);
   if (!string_is_empty(runloop_st->name.ups))
      content_ctx.name_ups                 = strdup(runloop_st->name.ups);

   if (sys_info)
   {
      struct retro_system_info *system = &runloop_st->system.info;

      if (set_supports_no_game_enable)
         content_ctx.flags            |= CONTENT_INFO_FLAG_SET_SUPPORTS_NO_GAME_ENABLE;

      if (!string_is_empty(path_dir_system))
         content_ctx.directory_system  = strdup(path_dir_system);
      if (!string_is_empty(path_dir_cache))
         content_ctx.directory_cache   = strdup(path_dir_cache);
      if (!string_is_empty(system->valid_extensions))
         content_ctx.valid_extensions  = strdup(system->valid_extensions);

      if (system->block_extract)
         content_ctx.flags            |= CONTENT_INFO_FLAG_BLOCK_EXTRACT;
      if (system->need_fullpath)
         content_ctx.flags            |= CONTENT_INFO_FLAG_NEED_FULLPATH;

      content_ctx.subsystem.data       = sys_info->subsystem.data;
      content_ctx.subsystem.size       = sys_info->subsystem.size;
   }

   p_content->flags                   |= CONTENT_ST_FLAG_IS_INITED;

   if (string_list_initialize(&content))
   {
      if (!content_file_init(&content_ctx, p_content,
            &content, &error_enum, &error_string))
      {
         content_deinit();
         ret = false;
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
            RARCH_ERR("[Content]: %s\n", msg_hash_to_str(error_enum));
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
         RARCH_LOG("[Content]: %s\n", error_string);
      }
      else
      {
         RARCH_ERR("[Content]: %s\n", error_string);
      }
      /* Do not flush the message queue here
       * > This allows any core-generated error messages
       *   to propagate through to the frontend */
      runloop_msg_queue_push(error_string, 2, ret ? 1 : 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      free(error_string);
   }

   return ret;
}

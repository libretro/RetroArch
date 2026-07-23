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

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#ifdef _WIN32
#ifdef _XBOX
#include <xtl.h>
#define setmode _setmode
#define INVALID_FILE_ATTRIBUTES -1
#else
#include <io.h>
#include <fcntl.h>
#define WIN32_LEAN_AND_MEAN
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
#include <formats/data_transfer.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <lists/string_list.h>
#include <lists/dir_list.h>
#include <vfs/vfs_implementation.h>
#include <array/rbuf.h>
#include "../msg_hash_lbl_str.h"

#include <retro_miscellaneous.h>

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

#if defined(ANDROID) && defined(HAVE_SAF)
#include <vfs/vfs_implementation_saf.h>
#endif

#include "task_content.h"
#include "patch_stream.h"
#include "tasks_internal.h"
#include "task_content_prefetch.h"

#include "../command.h"
#include "../core_info.h"
#include "../content.h"
#include "../core.h"
#include "../configuration.h"
#include "../defaults.h"
#include "../dynamic.h"
#include "../file_path_special.h"
#include "../frontend/frontend.h"
#include "../msg_hash.h"
#include "../playlist.h"
#include "../paths.h"
#include "../retroarch.h"
#include "../runloop.h"
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
   CONTENT_INFO_FLAG_PATCH_IS_BLOCKED            = (1 << 3),
   CONTENT_INFO_FLAG_IS_IPS_PREF                 = (1 << 4),
   CONTENT_INFO_FLAG_IS_BPS_PREF                 = (1 << 5),
   CONTENT_INFO_FLAG_IS_UPS_PREF                 = (1 << 6),
   CONTENT_INFO_FLAG_IS_XDELTA_PREF              = (1 << 7)
};

struct content_information_ctx
{
   char *name_ips;
   char *name_bps;
   char *name_ups;
   char *name_xdelta;

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

   if (p_content && (ext && *ext))
   {
      if ((num_overrides = RBUF_LEN(p_content->content_override_list)) >= 1)
      {
         for (i = 0; i < num_overrides; i++)
         {
            content_file_override_t *content_override =
               &p_content->content_override_list[i];

            if (   !content_override
                || !content_override->ext)
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
   size_t i;
   content_state_t *p_content = content_state_get_ptr();
   if (!p_content || !overrides)
      return false;
   /* Free any existing override list */
   content_file_override_free(p_content);
   for (i = 0; overrides[i].extensions; i++)
   {
      const char *ptr = overrides[i].extensions;
      while (*ptr)
      {
         char ext[32];
         size_t num_entries, _len;
         content_file_override_t *override = NULL;
         /* Find next '|' delimiter or end of string */
         const char *delim = ptr;
         while (*delim && *delim != '|')
            delim++;
         _len              = (size_t)(delim - ptr);

         /* Extract extension token */
         if (_len > 0 && _len < sizeof(ext))
         {
            memcpy(ext, ptr, _len);
            ext[_len] = '\0';

            /* Check whether extension has already been
             * registered */
            if (!content_file_override_get_ext(p_content, ext, NULL))
            {
               /* Add current override to the list */
               num_entries = RBUF_LEN(p_content->content_override_list);
               if (!RBUF_TRYFIT(p_content->content_override_list,
                     num_entries + 1))
                  return false;

               RBUF_RESIZE(p_content->content_override_list,
                     num_entries + 1);

               RARCH_LOG("[Content Override] File Extension: '%3s' - need_fullpath: %s, persistent_data: %s\n",
                     ext, overrides[i].need_fullpath ? "TRUE" : "FALSE",
                     overrides[i].persistent_data    ? "TRUE" : "FALSE");

               override                  = &p_content->content_override_list[num_entries];
               override->ext             = strdup(ext);
               override->need_fullpath   = overrides[i].need_fullpath;
               override->persistent_data = overrides[i].persistent_data;
            }
         }

         /* Advance past token and delimiter */
         ptr += _len;
         if (*ptr == '|')
            ptr++;
      }
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
   file_info->data_size       = 0;

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

         if (!path || !*path)
            continue;

         RARCH_LOG("[Content] %s: \"%s\".\n",
               msg_hash_to_str(MSG_REMOVING_TEMPORARY_CONTENT_FILE),
               path);

         if (filestream_delete(path) != 0)
            RARCH_ERR("[Content] %s: \"%s\".\n",
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

static content_file_list_t *content_file_list_init(size_t len)
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
               calloc(len, sizeof(content_file_info_t))))
         {
            file_list->size                  = len;
            /* Create retro_game_info object */
            if ((file_list->game_info        = (struct retro_game_info *)
                     calloc(len, sizeof(struct retro_game_info))))
            {
               /* Create retro_game_info_ext object */
               if ((file_list->game_info_ext =
                        (struct retro_game_info_ext *)
                        calloc(len, sizeof(struct retro_game_info_ext))))
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
      content_file_list_t *file_list, const char *path)
{
   if (file_list && (path && *path))
   {
      union string_list_elem_attr attr;
      attr.i = 0;
      if (string_list_append(file_list->temporary_files,
               path, attr))
         return file_list->temporary_files->elems[
            file_list->temporary_files->size - 1].data;
   }
   return NULL;
}

/* NOTE: Takes ownership of supplied 'data' buffer */
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

   file_info     = &file_list->entries[idx];
   game_info     = &file_list->game_info[idx];
   game_info_ext = &file_list->game_info_ext[idx];

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
   if (   ( data && !data_size)
       || (!data &&  data_size))
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
   if (path && *path)
   {
      char dir [DIR_MAX_LENGTH];
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
         size_t _len      = 0;

         file_info->file_in_archive = true;

         /* Extract path of parent archive */
         if ((_len = (size_t)(1 + archive_delim - path))
                 >= PATH_MAX_LENGTH)
            _len = PATH_MAX_LENGTH;

         strlcpy(archive_path, path, _len * sizeof(char));
         if (*archive_path)
            file_info->archive_path = strdup(archive_path);

         /* Extract name of file in archive */
         archive_delim++;
         if (archive_delim && *archive_delim)
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
      }
      path_remove_extension(name);

      if (*dir)
      {
         /* Remove any trailing slash */
         char *last_slash     = find_last_slash(dir);
         if (last_slash && (last_slash[1] == '\0'))
            *last_slash       = '\0';

         if (*dir)
            file_info->dir    = strdup(dir);
      }

      if (*name)
         file_info->name      = strdup(name);
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

/*****************************************************/
/* Content information context helper functions START */
/*****************************************************/

/**
 * content_information_ctx_init:
 *
 * Initialises a content_information_ctx_t, reading
 * current settings and runloop state. Caller must
 * call content_information_ctx_free() when done.
 *
 * @param content_ctx          : context to initialise.
 * @param settings             : current settings (may be NULL).
 * @param runloop_st           : current runloop state (may be NULL).
 * @param include_sys_info     : if true, also populate fields from
 *                               the system info (valid_extensions,
 *                               block_extract, need_fullpath, subsystem,
 *                               directory_cache, set_supports_no_game).
 **/
static void content_information_ctx_init(
      content_information_ctx_t *content_ctx,
      settings_t *settings,
      runloop_state_t *runloop_st,
      bool include_sys_info)
{
   content_ctx->flags              = 0;
   content_ctx->directory_system   = NULL;
   content_ctx->directory_cache    = NULL;
   content_ctx->name_ips           = NULL;
   content_ctx->name_bps           = NULL;
   content_ctx->name_ups           = NULL;
   content_ctx->name_xdelta        = NULL;
   content_ctx->valid_extensions   = NULL;
   content_ctx->subsystem.data     = NULL;
   content_ctx->subsystem.size     = 0;

#ifdef HAVE_PATCH
   {
      uint32_t rarch_flags = retroarch_get_flags();
      if (rarch_flags & RARCH_FLAGS_IPS_PREF)
         content_ctx->flags |= CONTENT_INFO_FLAG_IS_IPS_PREF;
      if (rarch_flags & RARCH_FLAGS_BPS_PREF)
         content_ctx->flags |= CONTENT_INFO_FLAG_IS_BPS_PREF;
      if (rarch_flags & RARCH_FLAGS_UPS_PREF)
         content_ctx->flags |= CONTENT_INFO_FLAG_IS_UPS_PREF;
#ifdef HAVE_XDELTA
      if (rarch_flags & RARCH_FLAGS_XDELTA_PREF)
         content_ctx->flags |= CONTENT_INFO_FLAG_IS_XDELTA_PREF;
#endif /* HAVE_XDELTA */
      if (runloop_st && (runloop_st->flags & RUNLOOP_FLAG_PATCH_BLOCKED))
         content_ctx->flags |= CONTENT_INFO_FLAG_PATCH_IS_BLOCKED;
   }
#endif /* HAVE_PATCH */

   if (runloop_st)
   {
      if (*runloop_st->name.ips)
         content_ctx->name_ips      = strdup(runloop_st->name.ips);
      if (*runloop_st->name.bps)
         content_ctx->name_bps      = strdup(runloop_st->name.bps);
      if (*runloop_st->name.ups)
         content_ctx->name_ups      = strdup(runloop_st->name.ups);
      if (*runloop_st->name.xdelta)
         content_ctx->name_xdelta   = strdup(runloop_st->name.xdelta);
   }

   if (settings)
   {
      const char *path_dir_system = settings->paths.directory_system;
      if (path_dir_system && *path_dir_system)
         content_ctx->directory_system = strdup(path_dir_system);
   }

   if (include_sys_info && runloop_st)
   {
      rarch_system_info_t *sys_info    = &runloop_st->system;
      struct retro_system_info *sysinfo = &sys_info->info;

      if (settings && settings->bools.set_supports_no_game_enable)
         content_ctx->flags |= CONTENT_INFO_FLAG_SET_SUPPORTS_NO_GAME_ENABLE;

      if (settings)
      {
         const char *path_dir_cache = settings->paths.directory_cache;
         if (path_dir_cache && *path_dir_cache)
         {
            content_ctx->directory_cache = strdup(path_dir_cache);

            if (!path_is_directory(path_dir_cache))
               path_mkdir(path_dir_cache);
         }
      }

      if (sysinfo->valid_extensions && *sysinfo->valid_extensions)
         content_ctx->valid_extensions = strdup(sysinfo->valid_extensions);

      if (sysinfo->block_extract)
         content_ctx->flags |= CONTENT_INFO_FLAG_BLOCK_EXTRACT;
      if (sysinfo->need_fullpath)
         content_ctx->flags |= CONTENT_INFO_FLAG_NEED_FULLPATH;

      content_ctx->subsystem.data = sys_info->subsystem.data;
      content_ctx->subsystem.size = sys_info->subsystem.size;
   }
}

/**
 * content_information_ctx_free:
 *
 * Frees all heap-allocated members of a content_information_ctx_t.
 **/
static void content_information_ctx_free(
      content_information_ctx_t *content_ctx)
{
   if (content_ctx->name_ips)
      free(content_ctx->name_ips);
   if (content_ctx->name_bps)
      free(content_ctx->name_bps);
   if (content_ctx->name_ups)
      free(content_ctx->name_ups);
   if (content_ctx->name_xdelta)
      free(content_ctx->name_xdelta);
   if (content_ctx->directory_system)
      free(content_ctx->directory_system);
   if (content_ctx->directory_cache)
      free(content_ctx->directory_cache);
   if (content_ctx->valid_extensions)
      free(content_ctx->valid_extensions);
}

/***************************************************/
/* Content information context helper functions END */
/***************************************************/

/********************************/
/* Content file functions START */
/********************************/

#define BLCK_BLOCK_EXTRACT 1
/* Defined further down, next to the synchronous file_crc32 it backs up;
 * used by the load path well before that point. */
static void content_crc_task_push(content_state_t *p_content,
      const char *path);

#define BLCK_NEED_FULLPATH 2
#define BLCK_REQUIRED      4
#define BLCK_PERSISTENT    8

#ifdef HAVE_COMPRESSION
/* data_transfer source bridge for archive entries */
static int64_t content_file_entry_source_read(void *ud, uint8_t *dst,
      size_t n)
{
   return file_archive_entry_source_read(
         (file_archive_entry_source_t*)ud, dst, (int64_t)n);
}
#endif

/* data_transfer source bridge for a plain file */
static int64_t content_file_plain_source_read(void *ud, uint8_t *dst,
      size_t n)
{
   return filestream_read((RFILE*)ud, dst, (int64_t)n);
}

#ifdef HAVE_PATCH
/* Open a streaming applier for whatever patch this load would apply, or
 * NULL to leave the load on the existing whole-buffer patch pass. */
static patch_stream_t *content_file_patch_stream_open(
      content_information_ctx_t *content_ctx,
      size_t idx,
      enum rarch_content_type first_content_type,
      size_t src_len,
      void **patch_data,
      const char **patch_fmt)
{
   if (      idx != 0
         ||  first_content_type != RARCH_CONTENT_NONE
         || (content_ctx->flags & CONTENT_INFO_FLAG_PATCH_IS_BLOCKED))
   {
      *patch_data = NULL;
      *patch_fmt  = NULL;
      return NULL;
   }

   return patch_content_stream_open(
         content_ctx->flags & CONTENT_INFO_FLAG_IS_IPS_PREF,
         content_ctx->flags & CONTENT_INFO_FLAG_IS_BPS_PREF,
         content_ctx->flags & CONTENT_INFO_FLAG_IS_UPS_PREF,
         content_ctx->flags & CONTENT_INFO_FLAG_IS_XDELTA_PREF,
         content_ctx->name_ips,
         content_ctx->name_bps,
         content_ctx->name_ups,
         content_ctx->name_xdelta,
         src_len, patch_data, patch_fmt);
}
#endif

/* Drive a source-mode transfer to completion, advancing a streaming
 * patch (if any) over each span as it arrives, and detach the filled
 * buffer.  Shared by the archive-entry and plain-file loads so both get
 * the same interleaving and the same ownership handoff.
 *
 * Returns the detached buffer (caller frees) or NULL, in which case the
 * transfer has already been released. */
static uint8_t *content_file_pump_source(data_transfer_t *dt,
      void *patch_stream, size_t *out_len)
{
   uint8_t *out   = NULL;
   size_t   avail = 0;
#ifdef HAVE_PATCH
   patch_stream_t *ps     = (patch_stream_t*)patch_stream;
   const uint8_t  *base   = NULL;
   size_t          fed    = 0;
   size_t          total  = 0;

   if (ps)
      base = data_transfer_ptr(dt, &total);
#else
   (void)patch_stream;
#endif

   while (     !data_transfer_complete(dt)
            && !data_transfer_failed(dt))
   {
      avail = data_transfer_iterate(dt, 0);
#ifdef HAVE_PATCH
      if (ps && avail > fed)
      {
         patch_stream_feed(ps, base + fed, avail - fed);
         fed = avail;
      }
#endif
   }

#ifdef HAVE_PATCH
   if (ps)
   {
      avail = data_transfer_avail(dt);
      if (avail > fed)
         patch_stream_feed(ps, base + fed, avail - fed);
   }
#endif

   if (!(out = (uint8_t*)data_transfer_source_detach(dt, out_len)))
      data_transfer_free(dt);
   return out;
}

/* ---- prefetch cache: bytes read ahead of the load ---- */

/* Take (transfer ownership of) a prefetched buffer for this exact
 * path, if the prefetch task deposited one. */
static uint8_t *content_file_prefetch_take(content_state_t *p_content,
      const char *path, size_t *size)
{
   size_t i;
   for (i = 0; i < p_content->prefetch_count; i++)
   {
      if (     p_content->prefetch[i].path
            && string_is_equal(p_content->prefetch[i].path, path))
      {
         uint8_t *data = p_content->prefetch[i].data;
         *size         = p_content->prefetch[i].size;
         free(p_content->prefetch[i].path);
         p_content->prefetch[i].path = NULL;
         p_content->prefetch[i].data = NULL;
         p_content->prefetch[i].size = 0;
         return data;
      }
   }
   return NULL;
}

/* Deposit callback for task_push_content_prefetch. */
static void content_file_prefetch_deposit(void *ud, const char *path,
      uint8_t *data, size_t size)
{
   content_state_t *p_content = (content_state_t*)ud;
   if (p_content->prefetch_count
         >= ARRAY_SIZE(p_content->prefetch))
   {
      free(data);
      return;
   }
   if (!(p_content->prefetch[p_content->prefetch_count].path
         = strdup(path)))
   {
      free(data);
      return;
   }
   p_content->prefetch[p_content->prefetch_count].data = data;
   p_content->prefetch[p_content->prefetch_count].size = size;
   p_content->prefetch_count++;
}

/* Drop whatever the load did not consume. */
static void content_file_prefetch_free(content_state_t *p_content)
{
   size_t i;
   for (i = 0; i < p_content->prefetch_count; i++)
   {
      free(p_content->prefetch[i].path);
      free(p_content->prefetch[i].data);
      p_content->prefetch[i].path = NULL;
      p_content->prefetch[i].data = NULL;
      p_content->prefetch[i].size = 0;
   }
   p_content->prefetch_count = 0;
}

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
 * Returns: non-0 if successful, 0 on error.
 **/
static size_t content_file_load_into_memory(
      content_information_ctx_t *content_ctx,
      content_state_t *p_content,
      const char *content_path,
      bool content_compressed,
      size_t idx,
      enum rarch_content_type first_content_type,
      uint8_t **data)
{
   uint8_t *content_data = NULL;
   int64_t content_size  = 0;
#ifdef HAVE_PATCH
   /* Soft patching normally runs as a separate pass once the whole file
    * is resident.  When the patch can be resolved ahead of the load - it
    * depends only on the preference flags and which patch files exist,
    * never on the content - the applier is opened here and advanced over
    * each span as it arrives, so the patch keeps pace with the load
    * instead of following it.  NULL means there is nothing streamable
    * and the existing patch_content pass below runs unchanged. */
   void           *patch_data = NULL;
   const char     *patch_fmt  = NULL;
   bool            streamed   = false;
#endif
   /* Streaming patch handle, kept opaque and declared unconditionally so
    * the load paths can hand it to the pump without a HAVE_PATCH branch
    * of their own.  Always NULL when patching is compiled out. */
   void           *patch_ps   = NULL;

   RARCH_LOG("[Content] %s: \"%s\".\n",
         msg_hash_to_str(MSG_LOADING_CONTENT_FILE), content_path);

   /* A prefetched buffer for this exact path short-circuits the
    * read entirely: the bytes streamed in ahead of the load, a
    * budgeted slice per task tick, while the frontend kept
    * running. */
   {
      size_t pre_size = 0;
      if ((content_data = content_file_prefetch_take(p_content,
            content_path, &pre_size)))
         content_size = (int64_t)pre_size;
   }
   /* Read content from file into memory buffer */
#ifdef HAVE_COMPRESSION
   if (content_data)
   {
      /* prefetched above */
   }
   else if (content_compressed)
   {
      /* Prefer the incremental entry source on the data_transfer
       * spine: the entry inflates chunk by chunk directly into the
       * exact-size destination as it is read - decompression and
       * loading interleave instead of running as one opaque gulp -
       * and the pump takes a byte budget, so this read is ready to
       * be sliced across task ticks once the surrounding load flow
       * is.  (Today it is still pumped to completion in place:
       * behaviour and bytes are identical to the classic path.)
       * Backends without independently decodable entries (7z solid
       * blocks) and anything unexpected fall back to the classic
       * whole-entry read below. */
      int64_t src_usize                 = 0;
      file_archive_entry_source_t *src  =
            file_archive_entry_source_open(content_path, &src_usize);
      if (src)
      {
         if (src_usize > 0)
         {
            data_transfer_t *dt = data_transfer_open_source(
                  (size_t)src_usize, content_file_entry_source_read,
                  src);
            if (dt)
            {
               size_t out_len = 0;
#ifdef HAVE_PATCH
               patch_ps = content_file_patch_stream_open(content_ctx,
                     idx, first_content_type, (size_t)src_usize,
                     &patch_data, &patch_fmt);
#endif
               if ((content_data = content_file_pump_source(dt,
                     patch_ps, &out_len)))
                  content_size = (int64_t)out_len;
            }
         }
         file_archive_entry_source_close(src);
      }
      if (!content_data)
      {
         if (!file_archive_compressed_read(content_path,
               (void**)&content_data, NULL, &content_size))
            return 0;
      }
   }
   else
#endif
   if (!content_data)
   {
      /* Plain file: pump it through the same source-mode transfer the
       * archive path uses, rather than one blocking whole-file read.
       * That puts uncompressed content - the common case - on the
       * sliceable spine too, and lets a patch advance alongside the
       * read instead of running as a pass afterwards.
       *
       * Falls back to filestream_read_file if the file cannot be
       * opened, sized, or transferred, so nothing that loaded before
       * stops loading now. */
      RFILE *fp = filestream_open(content_path,
            RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

      if (fp)
      {
         int64_t fsize = filestream_get_size(fp);

         if (fsize > 0)
         {
            data_transfer_t *dt = data_transfer_open_source((size_t)fsize,
                  content_file_plain_source_read, fp);

            if (dt)
            {
               size_t out_len = 0;
#ifdef HAVE_PATCH
               patch_ps = content_file_patch_stream_open(content_ctx,
                     idx, first_content_type, (size_t)fsize,
                     &patch_data, &patch_fmt);
#endif
               if ((content_data = content_file_pump_source(dt,
                     patch_ps, &out_len)))
                  content_size = (int64_t)out_len;
#ifdef HAVE_PATCH
               else if (patch_ps)
               {
                  /* transfer failed: drop the stream, the fallback
                   * read below feeds the whole-buffer pass instead */
                  patch_stream_free((patch_stream_t*)patch_ps);
                  patch_ps = NULL;
                  free(patch_data);
                  patch_data = NULL;
               }
#endif
            }
         }
         filestream_close(fp);
      }

      if (!content_data)
      {
         if (!filestream_read_file(content_path,
               (void**)&content_data, &content_size))
            return 0;
      }
   }

   if (content_size < 0)
      return 0;

#ifdef HAVE_PATCH
   /* Complete a streamed patch.  The source is still fully resident here
    * (the transfer buffer we just detached), so a patch that turns out to
    * be malformed costs nothing: keep the unpatched buffer and let the
    * normal pass below try again, which is exactly what happens today
    * when an applier rejects a patch. */
   if (patch_ps)
   {
      uint8_t *patched = NULL;
      size_t   patched_len = 0;

      if (      content_data
            &&  patch_stream_finish((patch_stream_t*)patch_ps,
                     &patched, &patched_len)
            &&  patched)
      {
         free(content_data);
         content_data = patched;
         content_size = (int64_t)patched_len;
         streamed     = true;

         if (config_get_ptr()->bools.notification_show_patch_applied)
         {
            char msg[128];
            size_t _len = snprintf(msg, sizeof(msg),
                  msg_hash_to_str(MSG_APPLYING_PATCH),
                  patch_fmt ? patch_fmt : "");
            runloop_msg_queue_push(msg, _len, 1, 180, false, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
      }
      patch_stream_free((patch_stream_t*)patch_ps);
      patch_ps = NULL;
      free(patch_data);
      patch_data = NULL;
   }
#endif

   /* First content file is significant: attempt to do
    * soft patching, CRC checking, etc. */
   if (idx == 0)
   {
      /* If we have a media type, ignore patches/CRC32
       * calculation. */
      if (first_content_type == RARCH_CONTENT_NONE)
      {
#ifdef HAVE_PATCH
         /* Attempt to apply a patch, unless one was already streamed
          * alongside the load above. */
         if (     !streamed
               && !(content_ctx->flags & CONTENT_INFO_FLAG_PATCH_IS_BLOCKED))
            patch_content(
                  content_ctx->flags & CONTENT_INFO_FLAG_IS_IPS_PREF,
                  content_ctx->flags & CONTENT_INFO_FLAG_IS_BPS_PREF,
                  content_ctx->flags & CONTENT_INFO_FLAG_IS_UPS_PREF,
                  content_ctx->flags & CONTENT_INFO_FLAG_IS_XDELTA_PREF,
                  content_ctx->name_ips,
                  content_ctx->name_bps,
                  content_ctx->name_ups,
                  content_ctx->name_xdelta,
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
         /* The content is in a memory buffer here, whatever route it
          * arrived by, so hash it now rather than deferring.
          *
          * Deferring re-read the same file from disk on first use and
          * hashed only a fixed-size prefix of it - so content past
          * that ceiling was identified by the CRC of its prefix,
          * silently and wrongly, to netplay and to the database.  Hashing the buffer
          * costs one pass at roughly 1.9 GB/s (about 2 ms for a 4 MB
          * cartridge ROM, 33 ms for a 64 MB one), against a blocking
          * disk re-read of the whole file on the deferred path - so
          * this is cheaper whenever the value is wanted at all, and
          * correct for any size. */
         p_content->rom_crc = encoding_crc32(0, content_data,
               (size_t)content_size);
         RARCH_LOG("[Content] CRC32: 0x%x.\n",
               (unsigned)p_content->rom_crc);
      }
      else
         p_content->rom_crc = 0;
   }

   *data      = content_data;

   return (size_t)content_size;
}

#ifdef HAVE_COMPRESSION
static bool content_file_extract_from_archive(
      content_information_ctx_t *content_ctx,
      content_state_t *p_content,
      const char *valid_exts,
      const char **content_path,
      char **err_string)
{
   const char *tmp_path_ptr = NULL;
   char tmp_path[PATH_MAX_LENGTH];

   tmp_path[0]  = '\0';

   /* TODO/FIXME - localize */
   RARCH_LOG("[Content] Core requires uncompressed content - "
         "extracting archive to temporary directory...\n");

   /* Attempt to extract file  */
   if (!file_archive_extract_file(
         *content_path, valid_exts,
         (!content_ctx->directory_cache || !*content_ctx->directory_cache) ?
               NULL : content_ctx->directory_cache,
         tmp_path, sizeof(tmp_path)))
   {
      char msg[PATH_MAX_LENGTH];
      snprintf(msg, sizeof(msg), "%s: \"%s\".\n",
            msg_hash_to_str(MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE),
            *content_path);
      *err_string = strdup(msg);
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

   /* TODO/FIXME - localize */
   RARCH_LOG("[Content] Content successfully extracted to: \"%s\".\n",
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
   bool path_is_archive;
   bool path_is_inside_archive;
   const char *content_path = content->elems[idx].data;

   if (!content_path || !*content_path)
      return;

#if defined(ANDROID) && defined(HAVE_SAF)
   /* Convert content:// URIs to the path format used by the VFS */
   if (strncmp(content_path, "content://", sizeof "content://" - 1) == 0)
   {
      struct libretro_vfs_implementation_saf_path_split_result result;
      if (retro_vfs_path_split_content_saf(&result, content_path))
      {
         char *serialized_path = retro_vfs_path_join_saf(result.tree, result.path);
         free(result.path);
         free(result.tree);
         if (serialized_path != NULL)
         {
            /* Store the serialized path in the content list entry
             * itself, so it is freed when the list is freed and we
             * avoid a static buffer leak. */
            free(content->elems[idx].data);
            content->elems[idx].data = serialized_path;
            content_path             = serialized_path;
         }
      }
   }
#endif

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
   if (!((content->elems[idx].attr.i & BLCK_BLOCK_EXTRACT) != 0)
       && path_is_archive
       && !path_is_inside_archive)
   {
      /* Get internal archive file list */
      struct string_list *archive_list =
            file_archive_get_file_list(content_path, valid_exts);

      if (    archive_list
          && (archive_list->size > 0))
      {
         const char *archive_file = NULL;

         /* Ensure that list is sorted alphabetically */
         if (archive_list->size > 1)
            dir_list_sort(archive_list, true);

         archive_file = archive_list->elems[0].data;

         if (archive_file && *archive_file)
         {
            char info_path[PATH_MAX_LENGTH];
            /* Build 'complete' archive file path */
            size_t _len       = strlcpy(info_path,
                  content_path, sizeof(info_path) - 2);
            info_path[_len  ] = '#';
            info_path[_len+1] = '\0';
            _len             += 1;
            strlcpy(info_path + _len, archive_file, sizeof(info_path) - _len);

            /* Update 'content' string_list */
            free(content->elems[idx].data);
            content->elems[idx].data = strdup(info_path);
            content_path             = content->elems[idx].data;

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
      bool block_extract = ((content->elems[idx].attr.i & BLCK_BLOCK_EXTRACT) != 0);
      bool required      = ((content->elems[idx].attr.i & BLCK_REQUIRED)   != 0);
      bool persistent    = ((content->elems[idx].attr.i & BLCK_PERSISTENT) != 0);

      content->elems[idx].attr.i = 0;

      /* Apply updates
       * > Note that 'persistent' attribute must not
       *   be set false by an override if it is already
       *   true (frontend may require persistence for
       *   other purposes, e.g. runahead) */
      if (block_extract)
         content->elems[idx].attr.i |= BLCK_BLOCK_EXTRACT;
      if (override->need_fullpath)
         content->elems[idx].attr.i |= BLCK_NEED_FULLPATH;
      if (required)
         content->elems[idx].attr.i |= BLCK_REQUIRED;
      if (persistent || override->persistent_data)
         content->elems[idx].attr.i |= BLCK_PERSISTENT;
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
      char **err_string,
      const struct retro_subsystem_info *special)
{
   size_t i;
   retro_ctx_load_content_info_t load_info;
   bool used_vfs_fallback_copy                = false;
#ifdef __WINRT__
   rarch_system_info_t *sys_info              = &runloop_state_get_ptr()->system;
#endif
   enum rarch_content_type first_content_type = RARCH_CONTENT_NONE;

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
      if (!content_path || !*content_path)
      {
         if ((content->elems[i].attr.i & BLCK_REQUIRED) != 0)
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

         /* Doesn't need fullpath? */
         if (!((content->elems[i].attr.i & BLCK_NEED_FULLPATH) != 0))
         {
            content_data = NULL;
            if ((content_size = content_file_load_into_memory(
                  content_ctx, p_content, content_path,
                  content_compressed, i, first_content_type,
                  &content_data)) == 0)
            {
               char msg[PATH_MAX_LENGTH];
               snprintf(msg, sizeof(msg), "%s: \"%s\".\n",
                     msg_hash_to_str(MSG_COULD_NOT_READ_CONTENT_FILE),
                     content_path);
               *err_string = strdup(msg);
               return false;
            }
         }
         else
         {
#ifdef HAVE_COMPRESSION
            /* If this is compressed content and need_fullpath
             * is true, extract it to a temporary file */
            if (    content_compressed
                && !((content->elems[i].attr.i & BLCK_BLOCK_EXTRACT) != 0)
                && !content_file_extract_from_archive(content_ctx, p_content,
                     valid_exts, &content_path, err_string))
               return false;
#endif
#ifdef __WINRT__
            /* TODO: When support for the 'actual' VFS is added,
             * there will need to be some more logic here */
            if (   !sys_info->supports_vfs
                && !is_path_accessible_using_standard_io(content_path))
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
                  char new_basedir[DIR_MAX_LENGTH];
                  char new_path[PATH_MAX_LENGTH];

                  RARCH_LOG("[Content] Core does not support VFS"
                     " - copying to cache directory...\n");

                  if (content_ctx->directory_cache && *content_ctx->directory_cache)
                     strlcpy(new_basedir, content_ctx->directory_cache,
                        sizeof(new_basedir));
                  else
                     new_basedir[0] = '\0';

                  if ( (!new_basedir || !*new_basedir)
                     || !path_is_directory(new_basedir)
                     || !is_path_accessible_using_standard_io(new_basedir))
                  {
                     size_t _len;
                     DWORD basedir_attribs;
                     RARCH_WARN("[Content] Tried copying to cache directory, "
                        "but cache directory was not set or found. "
                        "Setting cache directory to root of writable app directory...\n");
                     _len = strlcpy(new_basedir, uwp_dir_data, sizeof(new_basedir));
                     strlcpy(new_basedir + _len,
                           "VFSCACHE\\",
                           sizeof(new_basedir) - _len);
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
                     char msg[PATH_MAX_LENGTH];
                     /* TODO/FIXME - localize */
                     snprintf(msg, sizeof(msg), "%s: \"%s\". (during copy read or write)\n",
                        msg_hash_to_str(MSG_COULD_NOT_READ_CONTENT_FILE),
                        content_path);
                     *err_string = strdup(msg);
                     return false;
                  }

                  content_path = content_file_list_append_temporary(
                     p_content->content_list, new_path);

                  used_vfs_fallback_copy = true;
               }
            }
#endif
            RARCH_LOG("[Content] %s\n", msg_hash_to_str(
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
                  /* Get the hashing under way now, in slices, so the
                   * value is ready before anything asks for it. */
                  content_crc_task_push(p_content, content_path);
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
            ((content->elems[i].attr.i & BLCK_PERSISTENT) != 0), i))
      {
         RARCH_LOG("[Content] Failed to process content file: \"%s\".\n", content_path);
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
      if (first_content_path && *first_content_path)
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
      char **err_string,
      bool *ret)
{
   struct string_list *subsystem              = path_get_subsystem_list();
   const struct retro_subsystem_info *special = libretro_find_subsystem_info(
            subsystem_data, (unsigned)subsystem_current_count,
            path_get(RARCH_PATH_SUBSYSTEM));

   if (!special)
   {
      char msg[128];
      /* TODO/FIXME - localize */
      snprintf(msg, sizeof(msg),
            "Failed to find subsystem \"%s\" in libretro implementation.\n",
            path_get(RARCH_PATH_SUBSYSTEM));
      *err_string = strdup(msg);
      *ret = false;
      return NULL;
   }

   if (special->num_roms)
   {
      if (!subsystem)
      {
         *error_enum = MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT;
         *ret = false;
         return NULL;
      }

      if (special->num_roms != subsystem->size)
      {
         char msg[128];
         /* TODO/FIXME - localize */
         snprintf(msg, sizeof(msg),
               "Libretro core requires %u content files for "
               "subsystem \"%s\", but %u content files were provided.\n",
               special->num_roms, special->desc,
               (unsigned)subsystem->size);
         *err_string = strdup(msg);
         *ret = false;
         return NULL;
      }
   }
   else if (subsystem && subsystem->size)
   {
      char msg[128];
      /* TODO/FIXME - localize */
      snprintf(msg, sizeof(msg),
            "Libretro core takes no content for subsystem \"%s\", "
            "but %u content files were provided.\n",
            special->desc,
            (unsigned)subsystem->size);
      *err_string = strdup(msg);
      *ret = false;
      return NULL;
   }

   *ret = true;
   return special;
}

static void content_file_set_attributes(
      struct string_list *content,
      const struct retro_subsystem_info *special,
      content_information_ctx_t *content_ctx)
{
   struct string_list *subsystem = path_get_subsystem_list();

   if (!path_is_empty(RARCH_PATH_SUBSYSTEM) && special)
   {
      size_t i;

      for (i = 0; i < subsystem->size; i++)
      {
         union string_list_elem_attr attr;

         attr.i = 0;
         if (special->roms[i].block_extract)
            attr.i |= BLCK_BLOCK_EXTRACT;
         if (special->roms[i].need_fullpath)
            attr.i |= BLCK_NEED_FULLPATH;
         if (special->roms[i].required)
            attr.i |= BLCK_REQUIRED;

         string_list_append(content, subsystem->elems[i].data, attr);
      }
   }
   else
   {
      union string_list_elem_attr attr;
      const char *content_path = path_get(RARCH_PATH_CONTENT);
      uint8_t flags            = content_get_flags();

      attr.i = 0;
      if (content_ctx->flags & CONTENT_INFO_FLAG_BLOCK_EXTRACT)
         attr.i |= BLCK_BLOCK_EXTRACT;
      if (content_ctx->flags & CONTENT_INFO_FLAG_NEED_FULLPATH)
         attr.i |= BLCK_NEED_FULLPATH;
      if (!(flags
            & CONTENT_ST_FLAG_CORE_DOES_NOT_NEED_CONTENT))
         attr.i |= BLCK_REQUIRED;

      /* If runahead is supported and we are not using
       * subsystems, content data buffer must *always*
       * be persistent, since user may toggle second
       * instance runahead at any time (and secondary
       * core initialisation requires valid data) */
#if defined(HAVE_RUNAHEAD)
      attr.i |= BLCK_PERSISTENT;
#endif

      if (!content_path || !*content_path)
      {
         if (  (flags & CONTENT_ST_FLAG_CORE_DOES_NOT_NEED_CONTENT)
             && content_ctx->flags
             &  CONTENT_INFO_FLAG_SET_SUPPORTS_NO_GAME_ENABLE)
            string_list_append_n(content, "", STRLEN_CONST(""), attr);
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
      char **err_string)
{
   bool subsystem_path_is_empty               = path_is_empty(RARCH_PATH_SUBSYSTEM);
   bool ret                                   = subsystem_path_is_empty;
   const struct retro_subsystem_info *special = subsystem_path_is_empty ?
         NULL : content_file_init_subsystem(content_ctx->subsystem.data,
               content_ctx->subsystem.size, error_enum, err_string, &ret);

   if (!ret)
      return false;

   content_file_set_attributes(content, special, content_ctx);

   if (content->size > 0)
   {
      content_file_list_t *file_list = content_file_list_init(content->size);
      if (file_list)
      {
         p_content->content_list     = file_list;
         ret = content_file_load(p_content, content, content_ctx,
               error_enum, err_string, special);

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
      RARCH_LOG("[Core] Using content: \"%s\".\n", args->content_path);
      argv[(*argc)++] = strdup(args->content_path);
   }
#ifdef HAVE_MENU
   else
   {
      RARCH_LOG("[Core] %s\n",
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
   size_t i;
   bool ret                          = false;
   int rarch_argc                    = 0;
   char *rarch_argv[MAX_ARGS]        = {NULL};
   char *argv_copy [MAX_ARGS]        = {NULL};
   char **rarch_argv_ptr             = (char**)info->argv;
   int *rarch_argc_ptr               = (int*)&info->argc;
   struct rarch_main_wrap *wrap_args = NULL;

   if (!(wrap_args = (struct rarch_main_wrap*)
      malloc(sizeof(*wrap_args))))
      return false;

   wrap_args->argv           = NULL;
   wrap_args->content_path   = NULL;
   wrap_args->sram_path      = NULL;
   wrap_args->state_path     = NULL;
   wrap_args->config_path    = NULL;
   wrap_args->libretro_path  = NULL;
   wrap_args->flags          = 0;
   wrap_args->argc           = 0;

   /* The following snippet breaks command-line arguments on Haiku which in turn
      prevents from using RA without a menu or to start it from a front-end like ES-DE.
      All things considered, the risk/reward is favorable to just skipping this. */
#ifndef __HAIKU__
   if (info->environ_get)
      info->environ_get(rarch_argc_ptr,
            rarch_argv_ptr, info->args, wrap_args);
#endif
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

   ret             = retroarch_main_init(wrap_args->argc, wrap_args->argv);

   for (i = 0; i < ARRAY_SIZE(argv_copy); i++)
      free(argv_copy[i]);
   free(wrap_args);

   if (!ret)
      return false;

   if (p_content->flags & CONTENT_ST_FLAG_PENDING_SUBSYSTEM_INIT)
      content_clear_subsystem();

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

#ifdef HAVE_MENU
   retroarch_favorites_init();
   command_event(CMD_EVENT_HISTORY_INIT, NULL);
#endif

   command_event(CMD_EVENT_RESUME, NULL);
   command_event(CMD_EVENT_VIDEO_SET_ASPECT_RATIO, NULL);

   frontend_driver_process_args(rarch_argc_ptr, rarch_argv_ptr);
   frontend_driver_content_loaded();

   return true;
}

void menu_content_environment_get(int *argc, char *argv[],
      void *args, void *params_data)
{
   const char *a = NULL;
   struct rarch_main_wrap *wrap_args = (struct rarch_main_wrap*)params_data;
   runloop_state_t       *runloop_st = runloop_state_get_ptr();
   rarch_system_info_t   *sys_info   = &runloop_st->system;

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
   a = dir_get_ptr(RARCH_DIR_SAVEFILE);
   if (a && *a)
      wrap_args->sram_path     = dir_get_ptr(RARCH_DIR_SAVEFILE);
   a = dir_get_ptr(RARCH_DIR_SAVESTATE);
   if (a && *a)
      wrap_args->state_path    = dir_get_ptr(RARCH_DIR_SAVESTATE);
   if (!path_is_empty(RARCH_PATH_CONTENT))
      wrap_args->content_path  = path_get(RARCH_PATH_CONTENT);
   if (!retroarch_override_setting_is_set(
            RARCH_OVERRIDE_SETTING_LIBRETRO, NULL))
   {
      a = path_get(RARCH_PATH_CORE);
      wrap_args->libretro_path = (a && *a) ? a : NULL;
   }
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
      const char *path_content          = path_get(RARCH_PATH_CONTENT);
      struct retro_system_info *sysinfo = &runloop_st->system.info;

      if (path_content && *path_content)
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
      /* Push Quick Menu onto menu stack */
      if (launched_from_cli)
         menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, NULL);
#endif

      if (sysinfo && *tmp)
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

               if (!core_name || !*core_name)
                  core_name         = sysinfo->library_name;

               if (launched_from_companion_ui)
               {
                  /* Database name + checksum are supplied
                   * by the companion UI itself */
                  if (*p_content->companion_ui_crc32)
                     crc32 = p_content->companion_ui_crc32;
                  if (*p_content->companion_ui_db_name)
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

         if (*runloop_st->name.label)
            label = runloop_st->name.label;

         if (
                  settings
               && settings->bools.history_list_enable
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
            entry.entry_slot      = runloop_st->entry_state_slot;

            command_playlist_push_write(playlist_hist, &entry);
#if TARGET_OS_TV
            update_topshelf();
#endif
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

   if (   !settings
       || !settings->bools.history_list_enable
       || (!content_path || !*content_path))
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

         if (  (core && *core)
             && core_info_find(core, &core_info))
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

   if (  (core_path && *core_path)
       && playlist_hist)
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
      if (data && *data)
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

bool task_push_start_dummy_core(content_ctx_info_t *content_info)
{
   content_information_ctx_t content_ctx;
   content_state_t              *p_content = content_state_get_ptr();
   bool ret                                = true;
   settings_t *settings                    = config_get_ptr();
   runloop_state_t *runloop_st             = runloop_state_get_ptr();
   rarch_system_info_t *sys_info           = &runloop_st->system;

   if (!content_info)
      return false;

   content_information_ctx_init(&content_ctx, settings, runloop_st, false);

   if (!content_info->environ_get)
      content_info->environ_get            = menu_content_environment_get;

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

   content_information_ctx_free(&content_ctx);

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
#ifndef HAVE_DYNAMIC
   bool force_core_reload                     = settings->bools.always_reload_core_on_run_content;
#endif

   content_information_ctx_init(&content_ctx, settings, runloop_st, false);

   if (label)
      strlcpy(runloop_st->name.label, label, sizeof(runloop_st->name.label));
   else
      runloop_st->name.label[0] = '\0';

   /* Is content required by this core? */
   sys_info->load_no_content = fullpath ? false : true;

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
      if (fullpath && *fullpath)
         path_set(RARCH_PATH_CONTENT, fullpath);

      /* Load content and update content history */
      if ((ret = content_load(content_info, p_content)))
         task_push_to_history_list(p_content, true, false, false);

      goto end;
   }
#endif

   /* Specified core is not loaded
    * > Load it
    * > Forget manually loaded core */
   path_set(RARCH_PATH_CORE, core_path);
   path_clear(RARCH_PATH_CORE_LAST);

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
   if ((ret = command_event_cmd_exec(p_content,
         fullpath, &content_ctx, false)))
   {
#ifndef HAVE_DYNAMIC
      /* No dynamic core loading support: if we reach
       * this point then a new instance has been
       * forked - have to shut down this one */
      retroarch_ctl(RARCH_CTL_SET_SHUTDOWN, NULL);
      retroarch_menu_running_finished(true);
#endif
   }

#ifndef HAVE_DYNAMIC
end:
#endif
   /* Handle load content failure */
   if (!ret)
      retroarch_menu_running();

   content_information_ctx_free(&content_ctx);

   return ret;
}
#endif

bool task_push_start_current_core(content_ctx_info_t *content_info)
{
   content_information_ctx_t content_ctx;
   bool ret                           = true;
   content_state_t *p_content         = content_state_get_ptr();
   settings_t *settings               = config_get_ptr();
   runloop_state_t *runloop_st        = runloop_state_get_ptr();

   if (!content_info)
      return false;

   content_information_ctx_init(&content_ctx, settings, runloop_st, false);

   if (!content_info->environ_get)
      content_info->environ_get            = menu_content_environment_get;

   /* Clear content path */
   path_clear(RARCH_PATH_CONTENT);

   /* Preliminary stuff that has to be done before we
    * load the actual content. Can differ per mode. */
   runloop_set_current_core_type(CORE_TYPE_PLAIN, true);

   /* Loads content into currently selected core.
    * Note that 'content_load()' can fail and yet still
    * return 'true'... In this case, the dummy core
    * will be loaded; the 'start core' operation can
    * therefore only be considered successful if the
    * dummy core is not running following 'content_load()' */
   if (   !(ret = content_load(content_info, p_content))
       || !(ret = (runloop_st->current_core_type != CORE_TYPE_DUMMY)))
   {
#ifdef HAVE_MENU
      retroarch_menu_running();
#endif
      goto end;
   }

   task_push_to_history_list(p_content, true, false, false);

#ifdef HAVE_MENU
   /* Push Quick Menu onto menu stack */
   menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, NULL);
#endif

end:
   content_information_ctx_free(&content_ctx);

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
   /* Set core path */
   path_set(RARCH_PATH_CORE, core_path);

   /* Remember core path for reloading */
   if (!string_is_equal(core_path, path_get(RARCH_PATH_CORE_LAST)))
      path_set(RARCH_PATH_CORE_LAST, core_path);

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
   content_information_ctx_t content_ctx;
   content_state_t *p_content            = content_state_get_ptr();
   bool ret                              = true;
   runloop_state_t *runloop_st           = runloop_state_get_ptr();
   settings_t *settings                  = config_get_ptr();
   bool flush_menu                       = true;
   const char *menu_label                = NULL;

   if (!core_path || !*core_path)
      return false;

   content_information_ctx_init(&content_ctx, settings, runloop_st, false);

   /* Set core path */
   path_set(RARCH_PATH_CORE, core_path);

   /* Clear content path */
   path_clear(RARCH_PATH_CONTENT);

#if defined(HAVE_DYNAMIC)
   content_info.environ_get              = menu_content_environment_get;

   /* Load core */
   command_event(CMD_EVENT_LOAD_CORE, NULL);
   runloop_set_current_core_type(CORE_TYPE_PLAIN, true);

   /* Loads content into currently selected core.
    * Note that 'content_load()' can fail and yet still
    * return 'true'... In this case, the dummy core
    * will be loaded; the 'start core' operation can
    * therefore only be considered successful if the
    * dummy core is not running following 'content_load()' */
   if (   !(ret = content_load(&content_info, p_content))
       || !(ret = (runloop_st->current_core_type != CORE_TYPE_DUMMY)))
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

   menu_entries_get_last_stack(NULL, &menu_label, NULL, NULL, NULL);

   if (   string_is_equal(menu_label, MENU_ENUM_LABEL_CONTENTLESS_CORES_TAB_STR)
       || string_is_equal(menu_label, MENU_ENUM_LABEL_DEFERRED_CONTENTLESS_CORES_LIST_STR))
      flush_menu = false;

   /* Push Quick Menu onto menu stack */
   menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, &flush_menu);

#ifdef HAVE_DYNAMIC
end:
#endif
   content_information_ctx_free(&content_ctx);

   return ret;
}

#if defined(HAVE_DYNAMIC) && defined(HAVE_MENU)
/* ---- deferred menu load: prefetch, then the identical remainder ---- */

struct content_deferred_menu_load
{
   char *fullpath;
   enum rarch_core_type type;
   content_ctx_info_t info;      /* argv-free by the deferral gate */
};

static void task_content_deferred_menu_load_done(void *ud, bool all_ok)
{
   struct content_deferred_menu_load *d =
         (struct content_deferred_menu_load*)ud;
   content_state_t *p_content = content_state_get_ptr();
   bool ret;

   p_content->flags &= ~CONTENT_ST_FLAG_DEFERRED_LOAD_PENDING;

   /* The identical remainder of
    * task_push_load_content_with_new_core_from_menu: a prefetch
    * that skipped files (all_ok false) changed nothing - the load's
    * ordinary reads cover whatever is not in the cache. */
   if (!(ret = content_load(&d->info, p_content)))
   {
      content_file_prefetch_free(p_content);
      retroarch_menu_running();
   }
   else
   {
      task_push_to_history_list(p_content, true, false, false);
      if (d->type != CORE_TYPE_DUMMY)
         menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, NULL);
   }
   content_file_prefetch_free(p_content);   /* leftovers, if any */
   free(d->fullpath);
   free(d);
}

/* Returns true when the load was taken over by the deferred path. */
static bool task_content_defer_menu_load(content_state_t *p_content,
      const char *fullpath, enum rarch_core_type type,
      content_ctx_info_t *content_info)
{
   struct content_deferred_menu_load *d = NULL;
   const char *paths[1];

   if (type != CORE_TYPE_PLAIN)
      return false;
   if (!fullpath || !*fullpath)
      return false;                /* contentless: nothing to read */
   if (content_info->argc || content_info->argv || content_info->args)
      return false;                /* only the plain menu shape     */
   if (p_content->flags & CONTENT_ST_FLAG_DEFERRED_LOAD_PENDING)
      return false;                /* one deferral at a time        */
   /* The prefetch keys on this exact path, but the load rewrites
    * some paths before reading them: a content:// SAF URI becomes a
    * VFS path, and a bare archive ("foo.zip") becomes an explicit
    * entry ("foo.zip#first").  Deferring either would prefetch under
    * a key the load never looks up - a silent miss and a wasted
    * read.  Decline them; they take the synchronous path, exactly as
    * before this feature existed.  A path already carrying its entry
    * ("foo.zip#rom") is stable and may defer. */
   if (!strncmp(fullpath, "content://", STRLEN_CONST("content://")))
      return false;
   if (       path_is_compressed_file(fullpath)
       && !path_contains_compressed_file(fullpath))
      return false;                /* bare archive: load picks entry */

   if (!(d = (struct content_deferred_menu_load*)calloc(1, sizeof(*d))))
      return false;
   if (!(d->fullpath = strdup(fullpath)))
   {
      free(d);
      return false;
   }
   d->type = type;
   d->info = *content_info;        /* argv-free: shallow is whole   */

   paths[0] = d->fullpath;
   if (!task_push_content_prefetch(paths, 1,
         content_file_prefetch_deposit,
         task_content_deferred_menu_load_done, d))
   {
      free(d->fullpath);
      free(d);
      return false;
   }
   p_content->flags |= CONTENT_ST_FLAG_DEFERRED_LOAD_PENDING;
   return true;
}
#endif

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
   settings_t *settings                       = config_get_ptr();
   runloop_state_t *runloop_st                = runloop_state_get_ptr();
#ifndef HAVE_DYNAMIC
   bool force_core_reload                     = settings->bools.always_reload_core_on_run_content;
   /* Check whether specified core is already loaded
    * > If so, we can skip loading the core and
    *   just load the content directly */
   if (   !force_core_reload
       && (type == CORE_TYPE_PLAIN)
       && retroarch_ctl(RARCH_CTL_IS_CORE_LOADED, (void*)core_path))
      return task_push_load_content_with_core(fullpath, content_info,
            type, cb, user_data);
#endif

   content_information_ctx_init(&content_ctx, settings, runloop_st, false);

   runloop_st->name.label[0]               = '\0';

   path_set(RARCH_PATH_CONTENT, fullpath);
   path_set(RARCH_PATH_CORE, core_path);

#ifdef HAVE_DYNAMIC
   /* Load core */
   command_event(CMD_EVENT_LOAD_CORE, NULL);

   /* Load content */
   if (!content_info->environ_get)
      content_info->environ_get = menu_content_environment_get;

   /* Stream the content's bytes in ahead of the load, a budgeted
    * slice per task tick, so the menu keeps running instead of
    * freezing for one long read.  The deferral is taken only in the
    * exact shape this menu path produces (no argv, plain core type,
    * no deferral already in flight); anything else keeps the
    * synchronous path below, and if the prefetch cannot even be
    * pushed, so does everything.  The continuation performs the
    * identical remainder of this function. */
   if (task_content_defer_menu_load(p_content, fullpath, type,
         content_info))
   {
      content_information_ctx_free(&content_ctx);
      return true;
   }

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

   /* Push Quick Menu onto menu stack */
   if (type != CORE_TYPE_DUMMY)
      menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, NULL);

#ifdef HAVE_DYNAMIC
end:
#endif
   content_information_ctx_free(&content_ctx);

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
   content_state_t *p_content              = content_state_get_ptr();
   bool ret                                = false;
   runloop_state_t *runloop_st             = runloop_state_get_ptr();
   settings_t *settings                    = config_get_ptr();

   content_information_ctx_init(&content_ctx, settings, runloop_st, true);

   if (!content_info->environ_get)
      content_info->environ_get            = menu_content_environment_get;

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

   content_information_ctx_free(&content_ctx);

   return ret;
}

static bool task_load_content_internal_wrap(
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      bool load_from_companion_ui)
{
   /* Load content */
   if (task_load_content_internal(content_info, true, false,
            load_from_companion_ui))
   {
#ifdef HAVE_MENU
      /* Push Quick Menu onto menu stack */
      if (type != CORE_TYPE_DUMMY)
         menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, NULL);
#endif
      return true;
   }

#ifdef HAVE_MENU
   retroarch_menu_running();
#endif
   return false;
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

   p_content->companion_ui_db_name[0] = '\0';
   p_content->companion_ui_crc32[0]   = '\0';

   path_set(RARCH_PATH_CONTENT, fullpath);
   path_set(RARCH_PATH_CORE,    core_path);

   if (db_name && *db_name)
      strlcpy(p_content->companion_ui_db_name,
            db_name, sizeof(p_content->companion_ui_db_name));

   if (crc32 && *crc32)
      strlcpy(p_content->companion_ui_crc32,
            crc32, sizeof(p_content->companion_ui_crc32));

#ifdef HAVE_DYNAMIC
   command_event(CMD_EVENT_LOAD_CORE, NULL);
#endif

   global->flags &= ~GLOB_FLG_LAUNCHED_FROM_CLI;

   if (label)
      strlcpy(runloop_st->name.label, label, sizeof(runloop_st->name.label));
   else
      runloop_st->name.label[0] = '\0';

   return task_load_content_internal_wrap(content_info, CORE_TYPE_PLAIN, true);
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

   return task_load_content_internal_wrap(content_info, type, false);
}


bool task_push_load_content_with_core(
      const char *fullpath,
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data)
{
   path_set(RARCH_PATH_CONTENT, fullpath);
   return task_load_content_internal_wrap(content_info, type, false);
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
   return task_load_content_internal_wrap(content_info, type, false);
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
   return task_load_content_internal_wrap(content_info, type, false);
}

uint8_t content_get_flags(void)
{
   content_state_t  *p_content = content_state_get_ptr();
   return p_content->flags;
}

/* Clears the pending subsystem rom buffer*/
void content_clear_subsystem(void)
{
   size_t i;
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
   rarch_system_info_t                *sys_info = &runloop_st->system;

   /* Core fully loaded, use the subsystem data */
   if (sys_info->subsystem.data)
      subsystem                                 = sys_info->subsystem.data + idx;
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

   RARCH_LOG("[Subsystem] Setting current subsystem to: %d(%s) Content amount: %d.\n",
      p_content->pending_subsystem_id,
      p_content->pending_subsystem_ident,
      p_content->pending_subsystem_rom_num);
}

/* Sets the subsystem by name */
bool content_set_subsystem_by_name(const char* subsystem_name)
{
   unsigned i;
   runloop_state_t         *runloop_st = runloop_state_get_ptr();
   rarch_system_info_t       *sys_info = &runloop_st->system;
   /* Core not loaded completely, use the data we peeked on load core */
   const struct retro_subsystem_info
      *subsystem                       = runloop_st->subsystem_data;

   /* Core fully loaded, use the subsystem data */
   if (sys_info->subsystem.data)
      subsystem                        = sys_info->subsystem.data;

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

size_t content_get_subsystem_friendly_name(const char* subsystem_name, char *s, size_t len)
{
   size_t i;
   runloop_state_t *runloop_st                  = runloop_state_get_ptr();
   rarch_system_info_t                *sys_info = &runloop_st->system;
   /* Core not loaded completely, use the data we peeked on load core */
   const struct retro_subsystem_info *subsystem = runloop_st->subsystem_data;
   /* Core fully loaded, use the subsystem data */
   if (sys_info->subsystem.data)
      subsystem = sys_info->subsystem.data;
   for (i = 0; i < runloop_st->subsystem_current_count; i++, subsystem++)
   {
      if (string_is_equal(subsystem_name, subsystem->ident))
         return strlcpy(s, subsystem->desc, len);
   }
   return 0;
}

/* Add a rom to the subsystem ROM buffer */
void content_add_subsystem(const char* path)
{
   content_state_t *p_content = content_state_get_ptr();
   size_t pending_size        = PATH_MAX_LENGTH * sizeof(char);
   char *rom_buf              = NULL;

   if (p_content->pending_subsystem_rom_id >= RARCH_MAX_SUBSYSTEM_ROMS)
   {
      RARCH_ERR("[Subsystem] Cannot add ROM - maximum subsystem ROM count reached.\n");
      return;
   }

   if (!(rom_buf = (char*)malloc(pending_size)))
   {
      RARCH_ERR("[Subsystem] Failed to allocate memory for subsystem ROM path.\n");
      return;
   }

   p_content->pending_subsystem_roms[p_content->pending_subsystem_rom_id] = rom_buf;

   strlcpy(p_content->pending_subsystem_roms[
         p_content->pending_subsystem_rom_id],
         path, pending_size);
   RARCH_LOG("[Subsystem] Subsystem id: %d Subsystem ident:"
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

/* Bytes hashed per task tick by the background CRC task.  The whole
 * point is not to stall the frontend, so this is a slice, not the
 * buffer size. */
#define CRC32_TICK_BYTES (1024 * 1024)

/* ---- background CRC32 for content the frontend never buffers ----
 *
 * A core using need_fullpath opens the file itself, so the frontend has
 * no buffer to hash.  The value was therefore computed on first use,
 * synchronously, reading the whole file while the frontend waited.
 * Hash it in budgeted slices from load time instead: by the time
 * anything asks - netplay handshakes and movie recording happen well
 * after the content is up - the answer is already there, and
 * content_get_crc still hashes synchronously if it is not. */
struct content_crc_task_state
{
   RFILE          *file;
   unsigned char  *buf;
   content_state_t *p_content;
   uint32_t        crc;
};

static void content_crc_task_handler(retro_task_t *task)
{
   struct content_crc_task_state *st =
         (struct content_crc_task_state*)task->state;
   int64_t nread;

   if (!st->file || !st->buf)
   {
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
      return;
   }

   nread = filestream_read(st->file, st->buf, CRC32_TICK_BYTES);

   if (nread < 0)
   {
      /* Leave crc_bg_done clear: content_get_crc falls back to the
       * synchronous hash, which reports its own failure. */
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
      return;
   }

   st->crc = encoding_crc32(st->crc, st->buf, (size_t)nread);

   if (nread < CRC32_TICK_BYTES || filestream_eof(st->file))
   {
      /* Publish the value before the flag, so a reader that sees the
       * flag set is guaranteed to see the finished value. */
      st->p_content->crc_bg_value = st->crc;
      st->p_content->crc_bg_done  = 1;
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   }
}

static void content_crc_task_cleanup(retro_task_t *task)
{
   struct content_crc_task_state *st =
         (struct content_crc_task_state*)task->state;
   if (!st)
      return;
   if (st->file)
      filestream_close(st->file);
   free(st->buf);
   free(st);
   task->state = NULL;
}

/* Best-effort: on any failure the deferred synchronous hash still
 * covers the value, so there is nothing to report. */
static void content_crc_task_push(content_state_t *p_content,
      const char *path)
{
   struct content_crc_task_state *st = NULL;
   retro_task_t                  *t  = NULL;

   p_content->crc_bg_done  = 0;
   p_content->crc_bg_value = 0;

   if (!(st = (struct content_crc_task_state*)calloc(1, sizeof(*st))))
      return;
   if (!(st->buf = (unsigned char*)malloc(CRC32_TICK_BYTES)))
   {
      free(st);
      return;
   }
   if (!(st->file = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
               RETRO_VFS_FILE_ACCESS_HINT_NONE)))
   {
      free(st->buf);
      free(st);
      return;
   }
   if (!(t = task_init()))
   {
      filestream_close(st->file);
      free(st->buf);
      free(st);
      return;
   }

   st->p_content = p_content;
   st->crc       = 0;
   t->state      = st;
   t->handler    = content_crc_task_handler;
   t->cleanup    = content_crc_task_cleanup;
   t->flags     |= RETRO_TASK_FLG_MUTE;
   task_queue_push(t);
}

/**
 * Calculate a CRC32 over the whole of the given file.
 *
 * This used to stop after a fixed number of buffers, which meant any
 * file larger than that ceiling was identified by the CRC of its
 * prefix - a wrong value, returned without any indication that it was
 * partial.  Only content the frontend never holds in memory reaches
 * here now (a core using need_fullpath), and for that a correct hash
 * is the whole point of computing one.
 *
 * @return The calculated CRC32 hash, or 0 if there was an error.
 */
static uint32_t file_crc32(uint32_t crc, const char *path)
{
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

   for (;;)
   {
      int64_t nread = filestream_read(file, buf, CRC32_BUFFER_SIZE);
      if (nread < 0)
      {
         free(buf);
         filestream_close(file);
         return 0;
      }

      crc = encoding_crc32(crc, buf, (size_t)nread);
      if (nread < CRC32_BUFFER_SIZE || filestream_eof(file))
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
      p_content->flags   &= ~CONTENT_ST_FLAG_PENDING_ROM_CRC;
      /* Only reached for content the frontend never loaded into
       * memory - a core using need_fullpath.  Everything that does
       * pass through a buffer is hashed at load time instead.
       *
       * The background task started at load has normally finished by
       * now; take its value if so, and otherwise hash synchronously as
       * before.  Reading the flag first orders this against the task's
       * write of the value. */
      if (p_content->crc_bg_done)
         p_content->rom_crc = p_content->crc_bg_value;
      else
         p_content->rom_crc  = file_crc32(0,
               (const char*)p_content->pending_rom_crc_path);
      RARCH_LOG("[Content] CRC32: 0x%x.\n",
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

   p_content->content_list = NULL;
   p_content->rom_crc      = 0;
   p_content->flags       &= ~(CONTENT_ST_FLAG_PENDING_ROM_CRC
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
   runloop_path_set_special(p_content->pending_subsystem_roms,
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
   char *err_string                   = NULL;
   runloop_state_t *runloop_st        = runloop_state_get_ptr();
   settings_t *settings               = config_get_ptr();

   content_file_list_free(p_content->content_list);
   p_content->content_list            = NULL;

   content_information_ctx_init(&content_ctx, settings, runloop_st, true);

   p_content->flags |= CONTENT_ST_FLAG_IS_INITED;

   if (string_list_initialize(&content))
   {
      if (!content_file_init(&content_ctx, p_content,
            &content, &error_enum, &err_string))
      {
         content_deinit();
         ret = false;
      }
      string_list_deinitialize(&content);
   }

   content_information_ctx_free(&content_ctx);

   if (error_enum != MSG_UNKNOWN)
   {
      switch (error_enum)
      {
         case MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT:
         case MSG_ERROR_LIBRETRO_CORE_REQUIRES_VFS:
         case MSG_FAILED_TO_LOAD_CONTENT:
         case MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT:
            {
               const char *_msg = msg_hash_to_str(error_enum);
               RARCH_ERR("[Content] %s\n", _msg);
               runloop_msg_queue_push(_msg, strlen(_msg), 2, ret ? 1 : 180, false, NULL,
                     MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
            }
            break;
         case MSG_UNKNOWN:
         default:
            break;
      }
   }

   if (err_string)
   {
      /* Trim ending newline */
      size_t err_len = strlen(err_string);

      if (err_string[err_len - 1] == '\n')
         err_string[err_len - 1] = '\0';

      if (ret)
         RARCH_LOG("[Content] %s\n", err_string);
      else
         RARCH_ERR("[Content] %s\n", err_string);

      /* Do not flush the message queue here
       * > This allows any core-generated error messages
       *   to propagate through to the frontend */
      runloop_msg_queue_push(err_string, strlen(err_string), 2, ret ? 1 : 180, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
      free(err_string);
   }

   return ret;
}

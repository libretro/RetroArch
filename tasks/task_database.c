/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
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

#include <math.h>
#include <compat/strcasestr.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include <retro_endianness.h>
#include <string/stdstring.h>
#include <lists/dir_list.h>
#include <file/file_path.h>
#include <encodings/crc32.h>
#include <streams/file_stream.h>
#include <streams/chd_stream.h>
#include <streams/interface_stream.h>
#include "tasks_internal.h"

#include "../core_info.h"
#include "../database_info.h"

#include "../file_path_special.h"
#include "../msg_hash.h"
#include "../playlist.h"
#ifdef RARCH_INTERNAL
#include "../configuration.h"
#include "../retroarch.h"
#include "../ui/ui_companion_driver.h"
#include "../gfx/video_display_server.h"
#endif
#include "../verbosity.h"

typedef struct database_state_handle
{
   uint32_t crc;
   uint32_t archive_crc;
   size_t list_index;
   size_t entry_index;
   uint8_t *buf;
   char archive_name[511];
   char serial[4096];
   database_info_list_t *info;
   struct string_list *list;
} database_state_handle_t;

typedef struct db_handle
{
   bool pl_fuzzy_archive_match;
   bool pl_use_old_format;
   bool is_directory;
   bool scan_started;
   bool scan_without_core_match;
   bool show_hidden_files;
   unsigned status;
   char *playlist_directory;
   char *content_database_path;
   char *fullpath;
   database_info_handle_t *handle;
   database_state_handle_t state;
} db_handle_t;

int cue_find_track(const char *cue_path, bool first,
      uint64_t *offset, uint64_t *size,
      char *track_path, uint64_t max_len);

bool cue_next_file(intfstream_t *fd, const char *cue_path,
      char *path, uint64_t max_len);

int gdi_find_track(const char *gdi_path, bool first,
      char *track_path, uint64_t max_len);

bool gdi_next_file(intfstream_t *fd, const char *gdi_path,
      char *path, uint64_t max_len);

int detect_system(intfstream_t *fd, const char** system_name);

int detect_ps1_game(intfstream_t *fd, char *game_id);

int detect_psp_game(intfstream_t *fd, char *game_id);

int detect_gc_game(intfstream_t *fd, char *game_id);

int detect_serial_ascii_game(intfstream_t *fd, char *game_id);

static void database_info_set_type(
      database_info_handle_t *handle,
      enum database_type type)
{
   if (!handle)
      return;
   handle->type = type;
}

static enum database_type database_info_get_type(
      database_info_handle_t *handle)
{
   if (!handle)
      return DATABASE_TYPE_NONE;
   return handle->type;
}

static const char *database_info_get_current_name(
      database_state_handle_t *handle)
{
   if (!handle || !handle->list)
      return NULL;
   return handle->list->elems[handle->list_index].data;
}

static const char *database_info_get_current_element_name(
      database_info_handle_t *handle)
{
   if (!handle || !handle->list)
      return NULL;
   /* Skip pruned entries */
   while (handle->list->elems[handle->list_ptr].data == NULL)
   {
      if (++handle->list_ptr >= handle->list->size)
         return NULL;
   }
   return handle->list->elems[handle->list_ptr].data;
}

static int task_database_iterate_start(retro_task_t *task,
      database_info_handle_t *db,
      const char *name)
{
   char msg[256];
   const char *basename_path = !string_is_empty(name) ?
      path_basename(name) : "";

   msg[0] = '\0';

   snprintf(msg, sizeof(msg),
         STRING_REP_USIZE "/" STRING_REP_USIZE ": %s %s...\n",
         (size_t)db->list_ptr,
         (size_t)db->list->size,
         msg_hash_to_str(MSG_SCANNING),
         basename_path);

   if (!string_is_empty(msg))
   {
#ifdef RARCH_INTERNAL
      task_free_title(task);
      task_set_title(task, strdup(msg));
      if (db->list->size != 0)
         task_set_progress(task, roundf((float)db->list_ptr / ((float)db->list->size / 100.0f)));
#else
      fprintf(stderr, "msg: %s\n", msg);
#endif
   }

   db->status = DATABASE_STATUS_ITERATE;

   return 0;
}

static int intfstream_get_serial(intfstream_t *fd, char *serial)
{
  const char *system_name = NULL;

  /* Check if the system was not auto-detected. */
  if (detect_system(fd, &system_name) < 0)
  {
    /* Attempt to read an ASCII serial, like Wii. */
    if (detect_serial_ascii_game(fd, serial))
    {
      /* ASCII serial (Wii) was detected. */
      RARCH_LOG("%s '%s'\n", msg_hash_to_str(MSG_FOUND_DISK_LABEL), serial);
      return 0;
    }

    /* Any other non-system specific detection methods? */
    return 0;
  }

  if (string_is_equal(system_name, "psp"))
  {
    if (detect_psp_game(fd, serial) == 0)
      return 0;
    RARCH_LOG("%s '%s'\n", msg_hash_to_str(MSG_FOUND_DISK_LABEL), serial);
  }
  else if (string_is_equal(system_name, "ps1"))
  {
    if (detect_ps1_game(fd, serial) == 0)
      return 0;
    RARCH_LOG("%s '%s'\n", msg_hash_to_str(MSG_FOUND_DISK_LABEL), serial);
  }
  else if (string_is_equal(system_name, "gc"))
  {
    if (detect_gc_game(fd, serial) == 0)
      return 0;
    RARCH_LOG("%s '%s'\n", msg_hash_to_str(MSG_FOUND_DISK_LABEL), serial);
  }
  else {
    return 0;
  }

  return 1;
}

static bool intfstream_file_get_serial(const char *name,
      uint64_t offset, uint64_t size, char *serial)
{
   int rv;
   uint8_t *data     = NULL;
   int64_t file_size = -1;
   intfstream_t *fd  = intfstream_open_file(name,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!fd)
      return 0;

   if (intfstream_seek(fd, 0, SEEK_END) == -1)
      goto error;

   file_size = intfstream_tell(fd);

   if (intfstream_seek(fd, 0, SEEK_SET) == -1)
      goto error;

   if (file_size < 0)
      goto error;

   if (offset != 0 || size < (uint64_t) file_size)
   {
      if (intfstream_seek(fd, (int64_t)offset, SEEK_SET) == -1)
         goto error;

      data = (uint8_t*)malloc((size_t)size);

      if (intfstream_read(fd, data, size) != (int64_t) size)
      {
         free(data);
         goto error;
      }

      intfstream_close(fd);
      free(fd);
      fd = intfstream_open_memory(data, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE,
            size);
      if (!fd)
      {
         free(data);
         return 0;
      }
   }

   rv = intfstream_get_serial(fd, serial);
   intfstream_close(fd);
   free(fd);
   free(data);
   return rv;

error:
   intfstream_close(fd);
   free(fd);
   return 0;
}

static int task_database_cue_get_serial(const char *name, char* serial)
{
   char *track_path                 = (char*)malloc(PATH_MAX_LENGTH
         * sizeof(char));
   int ret                          = 0;
   uint64_t offset                  = 0;
   uint64_t size                    = 0;
   int rv                           = 0;

   track_path[0]                    = '\0';

   rv = cue_find_track(name, true, &offset, &size, track_path, PATH_MAX_LENGTH);

   if (rv < 0)
   {
      RARCH_LOG("%s: %s\n",
            msg_hash_to_str(MSG_COULD_NOT_FIND_VALID_DATA_TRACK),
            strerror(-rv));
      free(track_path);
      return 0;
   }

   RARCH_LOG("%s\n", msg_hash_to_str(MSG_READING_FIRST_DATA_TRACK));

   ret = intfstream_file_get_serial(track_path, offset, size, serial);
   free(track_path);

   return ret;
}

static int task_database_gdi_get_serial(const char *name, char* serial)
{
   char *track_path                 = (char*)malloc(PATH_MAX_LENGTH
         * sizeof(char));
   int ret                          = 0;
   int rv                           = 0;

   track_path[0]                    = '\0';

   rv = gdi_find_track(name, true, track_path, PATH_MAX_LENGTH);

   if (rv < 0)
   {
      RARCH_LOG("%s: %s\n",
            msg_hash_to_str(MSG_COULD_NOT_FIND_VALID_DATA_TRACK),
            strerror(-rv));
      free(track_path);
      return 0;
   }

   RARCH_LOG("%s\n", msg_hash_to_str(MSG_READING_FIRST_DATA_TRACK));

   ret = intfstream_file_get_serial(track_path, 0, SIZE_MAX, serial);
   free(track_path);

   return ret;
}

static int task_database_chd_get_serial(const char *name, char* serial)
{
   int result;
   intfstream_t *fd = intfstream_open_chd_track(
         name,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE,
         CHDSTREAM_TRACK_FIRST_DATA);
   if (!fd)
      return 0;

   result = intfstream_get_serial(fd, serial);
   intfstream_close(fd);
   free(fd);
   return result;
}

static int intfstream_get_crc(intfstream_t *fd, uint32_t *crc)
{
   int64_t read = 0;
   uint32_t acc = 0;
   uint8_t buffer[4096];

   while ((read = intfstream_read(fd, buffer, sizeof(buffer))) > 0)
      acc = encoding_crc32(acc, buffer, (size_t)read);

   if (read < 0)
      return 0;

   *crc = acc;

   return 1;
}

static bool intfstream_file_get_crc(const char *name,
      uint64_t offset, size_t size, uint32_t *crc)
{
   int rv;
   intfstream_t *fd  = intfstream_open_file(name,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   uint8_t *data     = NULL;
   int64_t file_size = -1;

   if (!fd)
      return 0;

   if (intfstream_seek(fd, 0, SEEK_END) == -1)
      goto error;

   file_size = intfstream_tell(fd);

   if (intfstream_seek(fd, 0, SEEK_SET) == -1)
      goto error;

   if (file_size < 0)
      goto error;

   if (offset != 0 || size < (uint64_t) file_size)
   {
      if (intfstream_seek(fd, (int64_t)offset, SEEK_SET) == -1)
         goto error;

      data = (uint8_t*)malloc(size);

      if (intfstream_read(fd, data, size) != (int64_t) size)
         goto error;

      intfstream_close(fd);
      free(fd);
      fd = intfstream_open_memory(data, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE, size);

      if (!fd)
         goto error;
   }

   rv = intfstream_get_crc(fd, crc);
   intfstream_close(fd);
   free(fd);
   free(data);
   return rv;

error:
   if (fd)
   {
      intfstream_close(fd);
      free(fd);
   }
   if (data)
      free(data);
   return 0;
}

static int task_database_cue_get_crc(const char *name, uint32_t *crc)
{
   char *track_path = (char *)malloc(PATH_MAX_LENGTH);
   uint64_t offset  = 0;
   uint64_t size    = 0;
   int rv           = 0;

   track_path[0]    = '\0';

   rv = cue_find_track(name, false, &offset, &size,
         track_path, PATH_MAX_LENGTH);

   if (rv < 0)
   {
      RARCH_LOG("%s: %s\n",
            msg_hash_to_str(MSG_COULD_NOT_FIND_VALID_DATA_TRACK),
            strerror(-rv));
      free(track_path);
      return 0;
   }

   RARCH_LOG("CUE '%s' primary track: %s\n (%lu, %lu)\n",name, track_path, (unsigned long) offset, (unsigned long) size);

   RARCH_LOG("%s\n", msg_hash_to_str(MSG_READING_FIRST_DATA_TRACK));

   rv = intfstream_file_get_crc(track_path, offset, (size_t)size, crc);
   if (rv == 1)
   {
      RARCH_LOG("CUE '%s' crc: %x\n", name, *crc);
   }
   free(track_path);
   return rv;
}

static int task_database_gdi_get_crc(const char *name, uint32_t *crc)
{
   char *track_path = (char *)malloc(PATH_MAX_LENGTH);
   int rv           = 0;

   track_path[0] = '\0';

   rv = gdi_find_track(name, true, track_path, PATH_MAX_LENGTH);

   if (rv < 0)
   {
      RARCH_LOG("%s: %s\n", msg_hash_to_str(MSG_COULD_NOT_FIND_VALID_DATA_TRACK),
                strerror(-rv));
      free(track_path);
      return 0;
   }

   RARCH_LOG("GDI '%s' primary track: %s\n", name, track_path);

   RARCH_LOG("%s\n", msg_hash_to_str(MSG_READING_FIRST_DATA_TRACK));

   rv = intfstream_file_get_crc(track_path, 0, SIZE_MAX, crc);
   if (rv == 1)
   {
      RARCH_LOG("GDI '%s' crc: %x\n", name, *crc);
   }
   free(track_path);
   return rv;
}

static bool task_database_chd_get_crc(const char *name, uint32_t *crc)
{
   int rv;
   intfstream_t *fd = intfstream_open_chd_track(
         name,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE,
         CHDSTREAM_TRACK_PRIMARY);
   if (!fd)
      return 0;

   rv = intfstream_get_crc(fd, crc);
   if (rv == 1)
   {
      RARCH_LOG("CHD '%s' crc: %x\n", name, *crc);
   }
   if (fd)
   {
      intfstream_close(fd);
      free(fd);
   }
   return rv;
}

static void task_database_cue_prune(database_info_handle_t *db,
      const char *name)
{
   size_t i;
   char       *path = (char *)malloc(PATH_MAX_LENGTH + 1);
   intfstream_t *fd = intfstream_open_file(name,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!fd)
      goto end;

   while (cue_next_file(fd, name, path, PATH_MAX_LENGTH))
   {
      for (i = db->list_ptr; i < db->list->size; ++i)
      {
         if (db->list->elems[i].data
               && string_is_equal(path, db->list->elems[i].data))
         {
            RARCH_LOG("Pruning file referenced by cue: %s\n", path);
            free(db->list->elems[i].data);
            db->list->elems[i].data = NULL;
         }
      }
   }

end:
   if (fd)
   {
      intfstream_close(fd);
      free(fd);
   }
   free(path);
}

static void gdi_prune(database_info_handle_t *db, const char *name)
{
   size_t i;
   char       *path = (char *)malloc(PATH_MAX_LENGTH + 1);
   intfstream_t *fd = intfstream_open_file(name,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!fd)
      goto end;

   while (gdi_next_file(fd, name, path, PATH_MAX_LENGTH))
   {
      for (i = db->list_ptr; i < db->list->size; ++i)
      {
         if (db->list->elems[i].data
               && string_is_equal(path, db->list->elems[i].data))
         {
            RARCH_LOG("Pruning file referenced by gdi: %s\n", path);
            free(db->list->elems[i].data);
            db->list->elems[i].data = NULL;
         }
      }
   }

end:
   free(fd);
   free(path);
}

static enum msg_file_type extension_to_file_type(const char *ext)
{
   if (
         string_is_equal(ext, "7z")  ||
         string_is_equal(ext, "7Z")  ||
         string_is_equal(ext, "zip") ||
         string_is_equal(ext, "ZIP") ||
         string_is_equal(ext, "apk") ||
         string_is_equal(ext, "APK")
      )
      return FILE_TYPE_COMPRESSED;
   if (
         string_is_equal(ext, "cue")  ||
         string_is_equal(ext, "CUE")
      )
      return FILE_TYPE_CUE;
   if (
         string_is_equal(ext, "gdi")  ||
         string_is_equal(ext, "GDI")
      )
      return FILE_TYPE_GDI;
   if (
         string_is_equal(ext, "iso")  ||
         string_is_equal(ext, "ISO")
      )
      return FILE_TYPE_ISO;
   if (
         string_is_equal(ext, "chd")  ||
         string_is_equal(ext, "CHD")
      )
      return FILE_TYPE_CHD;
   if (
         string_is_equal(ext, "wbfs")  ||
         string_is_equal(ext, "WBFS")
      )
      return FILE_TYPE_WBFS;
   if (
         string_is_equal(ext, "lutro")  ||
         string_is_equal(ext, "LUTRO")
      )
      return FILE_TYPE_LUTRO;
   return FILE_TYPE_NONE;
}

static int task_database_iterate_playlist(
      database_state_handle_t *db_state,
      database_info_handle_t *db, const char *name)
{
   switch (extension_to_file_type(path_get_extension(name)))
   {
      case FILE_TYPE_COMPRESSED:
#ifdef HAVE_COMPRESSION
         database_info_set_type(db, DATABASE_TYPE_CRC_LOOKUP);
         /* first check crc of archive itself */
         return intfstream_file_get_crc(name,
               0, SIZE_MAX, &db_state->archive_crc);
#else
         break;
#endif
      case FILE_TYPE_CUE:
         task_database_cue_prune(db, name);
         db_state->serial[0] = '\0';
         if (task_database_cue_get_serial(name, db_state->serial))
            database_info_set_type(db, DATABASE_TYPE_SERIAL_LOOKUP);
         else
         {
            database_info_set_type(db, DATABASE_TYPE_CRC_LOOKUP);
            return task_database_cue_get_crc(name, &db_state->crc);
         }
         break;
      case FILE_TYPE_GDI:
         gdi_prune(db, name);
         db_state->serial[0] = '\0';
         /* There are no serial databases, so don't bother with
            serials at the moment */
         if (0 && task_database_gdi_get_serial(name, db_state->serial))
            database_info_set_type(db, DATABASE_TYPE_SERIAL_LOOKUP);
         else
         {
            database_info_set_type(db, DATABASE_TYPE_CRC_LOOKUP);
            return task_database_gdi_get_crc(name, &db_state->crc);
         }
         break;
      /* Consider Wii WBFS files similar to ISO files. */
      case FILE_TYPE_WBFS:
      case FILE_TYPE_ISO:
         db_state->serial[0] = '\0';
         intfstream_file_get_serial(name, 0, SIZE_MAX, db_state->serial);
         database_info_set_type(db, DATABASE_TYPE_SERIAL_LOOKUP);
         break;
      case FILE_TYPE_CHD:
         db_state->serial[0] = '\0';
         if (task_database_chd_get_serial(name, db_state->serial))
            database_info_set_type(db, DATABASE_TYPE_SERIAL_LOOKUP);
         else
         {
            database_info_set_type(db, DATABASE_TYPE_CRC_LOOKUP);
            return task_database_chd_get_crc(name, &db_state->crc);
         }
         break;
      case FILE_TYPE_LUTRO:
         database_info_set_type(db, DATABASE_TYPE_ITERATE_LUTRO);
         break;
      default:
         database_info_set_type(db, DATABASE_TYPE_CRC_LOOKUP);
         return intfstream_file_get_crc(name, 0, SIZE_MAX, &db_state->crc);
   }

   return 1;
}

static int database_info_list_iterate_end_no_match(
      database_info_handle_t *db,
      database_state_handle_t *db_state,
      const char *path)
{
   /* Reached end of database list,
    * CRC match probably didn't succeed. */

   /* If this was a compressed file and no match in the database
    * list was found then expand the search list to include the
    * archive's contents. */
   if (path_is_compressed_file(path) && !path_contains_compressed_file(path))
   {
      struct string_list *archive_list =
         file_archive_get_file_list(path, NULL);

      if (archive_list && archive_list->size > 0)
      {
         unsigned i;

         for (i = 0; i < archive_list->size; i++)
         {
            char *new_path   = (char*)malloc(
               PATH_MAX_LENGTH * sizeof(char));
            size_t path_size = PATH_MAX_LENGTH * sizeof(char);
            size_t path_len  = strlen(path);

            new_path[0] = '\0';

            strlcpy(new_path, path, path_size);

            if (path_len + strlen(archive_list->elems[i].data)
                     + 1 < PATH_MAX_LENGTH)
            {
               new_path[path_len] = '#';
               strlcpy(new_path + path_len + 1,
                  archive_list->elems[i].data,
                  path_size - path_len);
            }

            string_list_append(db->list, new_path,
               archive_list->elems[i].attr);

            free(new_path);
         }

         string_list_free(archive_list);
      }
   }

   db_state->list_index  = 0;
   db_state->entry_index = 0;

   if (db_state->crc != 0)
      db_state->crc = 0;

   if (db_state->archive_crc != 0)
      db_state->archive_crc = 0;

   return 0;
}

static int task_database_iterate_next(database_info_handle_t *db)
{
   db->list_ptr++;

   if (db->list_ptr < db->list->size)
      return 0;
   return -1;
}

static int database_info_list_iterate_new(database_state_handle_t *db_state,
      const char *query)
{
   const char *new_database = database_info_get_current_name(db_state);

#ifndef RARCH_INTERNAL
   fprintf(stderr, "Check database [%d/%d] : %s\n", (unsigned)db_state->list_index,
         (unsigned)db_state->list->size, new_database);
#endif
   if (db_state->info)
   {
      database_info_list_free(db_state->info);
      free(db_state->info);
   }
   db_state->info = database_info_list_new(new_database, query);
   return 0;
}

static int database_info_list_iterate_found_match(
      db_handle_t *_db,
      database_state_handle_t *db_state,
      database_info_handle_t *db,
      const char *archive_name
      )
{
   char *db_crc                   = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   char *db_playlist_base_str     = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   char *db_playlist_path         = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   char *entry_path_str           = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   playlist_t   *playlist         = NULL;
   const char         *db_path    =
      database_info_get_current_name(db_state);
   const char         *entry_path =
      database_info_get_current_element_name(db);
   database_info_t *db_info_entry =
      &db_state->info->list[db_state->entry_index];
   char *hash;

   db_crc[0]                      = '\0';
   db_playlist_path[0]            = '\0';
   db_playlist_base_str[0]        = '\0';
   entry_path_str[0]              = '\0';

   fill_short_pathname_representation_noext(db_playlist_base_str,
         db_path, PATH_MAX_LENGTH * sizeof(char));

   strlcat(db_playlist_base_str,
         ".lpl",
         PATH_MAX_LENGTH * sizeof(char));

   if (!string_is_empty(_db->playlist_directory))
      fill_pathname_join(db_playlist_path, _db->playlist_directory,
            db_playlist_base_str, PATH_MAX_LENGTH * sizeof(char));

   playlist = playlist_init(db_playlist_path, COLLECTION_SIZE);

   snprintf(db_crc, PATH_MAX_LENGTH * sizeof(char),
         "%08X|crc", db_info_entry->crc32);

   if (entry_path)
      strlcpy(entry_path_str, entry_path, PATH_MAX_LENGTH * sizeof(char));

   if (!string_is_empty(archive_name))
      fill_pathname_join_delim(entry_path_str,
            entry_path_str, archive_name,
            '#', PATH_MAX_LENGTH * sizeof(char));

   if (core_info_database_match_archive_member(
         db_state->list->elems[db_state->list_index].data) &&
       (hash = strchr(entry_path_str, '#')))
       *hash = '\0';

#if defined(RARCH_INTERNAL)
#if 0
   RARCH_LOG("Found match in database !\n");

   RARCH_LOG("Path: %s\n", db_path);
   RARCH_LOG("CRC : %s\n", db_crc);
   RARCH_LOG("Playlist Path: %s\n", db_playlist_path);
   RARCH_LOG("Entry Path: %s\n", entry_path);
   RARCH_LOG("Playlist not NULL: %d\n", playlist != NULL);
   RARCH_LOG("ZIP entry: %s\n", archive_name);
   RARCH_LOG("entry path str: %s\n", entry_path_str);
#endif
#else
   fprintf(stderr, "Found match in database !\n");

   fprintf(stderr, "Path: %s\n", db_path);
   fprintf(stderr, "CRC : %s\n", db_crc);
   fprintf(stderr, "Playlist Path: %s\n", db_playlist_path);
   fprintf(stderr, "Entry Path: %s\n", entry_path);
   fprintf(stderr, "Playlist not NULL: %d\n", playlist != NULL);
   fprintf(stderr, "ZIP entry: %s\n", archive_name);
   fprintf(stderr, "entry path str: %s\n", entry_path_str);
#endif

   if (!playlist_entry_exists(playlist, entry_path_str,
            _db->pl_fuzzy_archive_match))
   {
      struct playlist_entry entry;

      /* the push function reads our entry as const, so these casts are safe */
      entry.path              = entry_path_str;
      entry.label             = db_info_entry->name;
      entry.core_path         = (char*)"DETECT";
      entry.core_name         = (char*)"DETECT";
      entry.db_name           = db_playlist_base_str;
      entry.crc32             = db_crc;
      entry.subsystem_ident   = NULL;
      entry.subsystem_name    = NULL;
      entry.subsystem_roms    = NULL;
      entry.runtime_hours     = 0;
      entry.runtime_minutes   = 0;
      entry.runtime_seconds   = 0;
      entry.last_played_year  = 0;
      entry.last_played_month = 0;
      entry.last_played_day   = 0;
      entry.last_played_hour  = 0;
      entry.last_played_minute= 0;
      entry.last_played_second= 0;

      playlist_push(playlist, &entry, _db->pl_fuzzy_archive_match);
   }

   playlist_write_file(playlist, _db->pl_use_old_format);
   playlist_free(playlist);

   database_info_list_free(db_state->info);
   free(db_state->info);

   db_state->info        = NULL;
   db_state->crc         = 0;
   db_state->archive_crc = 0;

   free(entry_path_str);
   free(db_playlist_path);
   free(db_playlist_base_str);
   free(db_crc);

   /* Move database to start since we are likely to match against it
      again */
   if (db_state->list_index != 0)
   {
      struct string_list_elem entry = db_state->list->elems[db_state->list_index];
      memmove(&db_state->list->elems[1],
              &db_state->list->elems[0],
              sizeof(entry) * db_state->list_index);
      db_state->list->elems[0] = entry;
   }

   return 0;
}

/* End of entries in database info list and didn't find a
 * match, go to the next database. */
static int database_info_list_iterate_next(
      database_state_handle_t *db_state
      )
{
   db_state->list_index++;
   db_state->entry_index = 0;

   database_info_list_free(db_state->info);
   free(db_state->info);
   db_state->info        = NULL;

   return 1;
}

static int task_database_iterate_crc_lookup(
      db_handle_t *_db,
      database_state_handle_t *db_state,
      database_info_handle_t *db,
      const char *name,
      const char *archive_entry)
{

   if (!db_state->list ||
         (unsigned)db_state->list_index == (unsigned)db_state->list->size)
      return database_info_list_iterate_end_no_match(db, db_state, name);

   /* archive did not contain a CRC for this entry, or the file is empty */
   if (!db_state->crc)
   {
      db_state->crc = file_archive_get_file_crc32(name);

      if (!db_state->crc)
         return database_info_list_iterate_next(db_state);
   }

   if (db_state->entry_index == 0)
   {
      char query[50];

      query[0] = '\0';

      if (!_db->scan_without_core_match)
      {
         /* don't scan files that can't be in this database.
          *
          * Could be because of:
          * - A matching core missing
          * - Incompatible file extension */
         if (!core_info_database_supports_content_path(
               db_state->list->elems[db_state->list_index].data, name))
            return database_info_list_iterate_next(db_state);

         if (!path_contains_compressed_file(name))
         {
            if (core_info_database_match_archive_member(
                  db_state->list->elems[db_state->list_index].data))
               return database_info_list_iterate_next(db_state);
         }
      }

      snprintf(query, sizeof(query),
            "{crc:or(b\"%08X\",b\"%08X\")}",
            db_state->crc, db_state->archive_crc);

      database_info_list_iterate_new(db_state, query);
   }

   if (db_state->info)
   {
      database_info_t *db_info_entry =
         &db_state->info->list[db_state->entry_index];

      if (db_info_entry && db_info_entry->crc32)
      {
#if 0
         RARCH_LOG("CRC32: 0x%08X , entry CRC32: 0x%08X (%s).\n",
               db_state->crc, db_info_entry->crc32, db_info_entry->name);
#endif
         if (db_state->archive_crc == db_info_entry->crc32)
            return database_info_list_iterate_found_match(
                  _db,
                  db_state, db, NULL);
         if (db_state->crc == db_info_entry->crc32)
            return database_info_list_iterate_found_match(
                  _db,
                  db_state, db, archive_entry);
      }
   }

   db_state->entry_index++;

   if (db_state->info)
   {
      if (db_state->entry_index >= db_state->info->count)
         return database_info_list_iterate_next(db_state);
   }

   /* If we haven't reached the end of the database list yet,
    * continue iterating. */
   if (db_state->list_index < db_state->list->size)
      return 1;

   database_info_list_free(db_state->info);

   if (db_state->info)
      free(db_state->info);

   return 0;
}

static int task_database_iterate_playlist_archive(
      db_handle_t *_db,
      database_state_handle_t *db_state,
      database_info_handle_t *db, const char *name)
{
#ifdef HAVE_COMPRESSION
   return task_database_iterate_crc_lookup(
         _db, db_state, db, name, db_state->archive_name);
#else
   return 1;
#endif
}

static int task_database_iterate_playlist_lutro(
      db_handle_t *_db,
      database_state_handle_t *db_state,
      database_info_handle_t *db,
      const char *path)
{
   char *db_playlist_path  = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   playlist_t   *playlist  = NULL;

   db_playlist_path[0]     = '\0';

   if (!string_is_empty(_db->playlist_directory))
      fill_pathname_join(db_playlist_path,
            _db->playlist_directory,
            "Lutro.lpl",
            PATH_MAX_LENGTH * sizeof(char));

   playlist = playlist_init(db_playlist_path, COLLECTION_SIZE);

   free(db_playlist_path);

   if (!playlist_entry_exists(playlist, path,
            _db->pl_fuzzy_archive_match))
   {
      struct playlist_entry entry;
      char *game_title            = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

      game_title[0] = '\0';

      fill_short_pathname_representation_noext(game_title,
            path, PATH_MAX_LENGTH * sizeof(char));

      /* the push function reads our entry as const, so these casts are safe */
      entry.path              = (char*)path;
      entry.label             = game_title;
      entry.core_path         = (char*)"DETECT";
      entry.core_name         = (char*)"DETECT";
      entry.db_name           = (char*)"Lutro.lpl";
      entry.crc32             = (char*)"DETECT";
      entry.subsystem_ident   = NULL;
      entry.subsystem_name    = NULL;
      entry.subsystem_roms    = NULL;
      entry.runtime_hours     = 0;
      entry.runtime_minutes   = 0;
      entry.runtime_seconds   = 0;
      entry.last_played_year  = 0;
      entry.last_played_month = 0;
      entry.last_played_day   = 0;
      entry.last_played_hour  = 0;
      entry.last_played_minute= 0;
      entry.last_played_second= 0;

      playlist_push(playlist, &entry, _db->pl_fuzzy_archive_match);

      free(game_title);
   }

   playlist_write_file(playlist, _db->pl_use_old_format);
   playlist_free(playlist);

   return 0;
}

static int task_database_iterate_serial_lookup(
      db_handle_t *_db,
      database_state_handle_t *db_state,
      database_info_handle_t *db, const char *name)
{
   if (!db_state->list ||
         (unsigned)db_state->list_index == (unsigned)db_state->list->size)
      return database_info_list_iterate_end_no_match(db, db_state, name);

   if (db_state->entry_index == 0)
   {
      char query[50];
      char *serial_buf =
         bin_to_hex_alloc((uint8_t*)db_state->serial, strlen(db_state->serial) * sizeof(uint8_t));

      if (!serial_buf)
         return 1;

      query[0] = '\0';

      snprintf(query, sizeof(query), "{'serial': b'%s'}", serial_buf);
      database_info_list_iterate_new(db_state, query);

      free(serial_buf);
   }

   if (db_state->info)
   {
      database_info_t *db_info_entry = &db_state->info->list[
         db_state->entry_index];

      if (db_info_entry && db_info_entry->serial)
      {
#if 0
         RARCH_LOG("serial: %s , entry serial: %s (%s).\n",
                   db_state->serial, db_info_entry->serial,
                   db_info_entry->name);
#endif
         if (string_is_equal(db_state->serial, db_info_entry->serial))
            return database_info_list_iterate_found_match(_db,
                  db_state, db, NULL);
      }
   }

   db_state->entry_index++;

   if (db_state->info)
   {
      if (db_state->entry_index >= db_state->info->count)
         return database_info_list_iterate_next(db_state);
   }

   /* If we haven't reached the end of the database list yet,
    * continue iterating. */
   if (db_state->list_index < db_state->list->size)
      return 1;

   database_info_list_free(db_state->info);
   free(db_state->info);
   return 0;
}

static int task_database_iterate(
      db_handle_t *_db,
      database_state_handle_t *db_state,
      database_info_handle_t *db)
{
   const char *name = database_info_get_current_element_name(db);

   if (!name)
      return 0;

   if (database_info_get_type(db) == DATABASE_TYPE_ITERATE)
      if (path_contains_compressed_file(name))
         database_info_set_type(db, DATABASE_TYPE_ITERATE_ARCHIVE);

   switch (database_info_get_type(db))
   {
      case DATABASE_TYPE_ITERATE:
         return task_database_iterate_playlist(db_state, db, name);
      case DATABASE_TYPE_ITERATE_ARCHIVE:
         return task_database_iterate_playlist_archive(_db, db_state, db, name);
      case DATABASE_TYPE_ITERATE_LUTRO:
         return task_database_iterate_playlist_lutro(_db, db_state, db, name);
      case DATABASE_TYPE_SERIAL_LOOKUP:
         return task_database_iterate_serial_lookup(_db, db_state, db, name);
      case DATABASE_TYPE_CRC_LOOKUP:
         return task_database_iterate_crc_lookup(_db, db_state, db, name, NULL);
      case DATABASE_TYPE_NONE:
      default:
         break;
   }

   return 0;
}

static void task_database_cleanup_state(
      database_state_handle_t *db_state)
{
   if (!db_state)
      return;

   if (db_state->buf)
      free(db_state->buf);
   db_state->buf = NULL;
}

static void task_database_handler(retro_task_t *task)
{
   const char *name                 = NULL;
   database_info_handle_t  *dbinfo  = NULL;
   database_state_handle_t *dbstate = NULL;
   db_handle_t *db                  = NULL;

   if (!task)
      goto task_finished;

   db      = (db_handle_t*)task->state;

   if (!db)
      goto task_finished;

   if (!db->scan_started)
   {
      db->scan_started = true;

      if (!string_is_empty(db->fullpath))
      {
         if (db->is_directory)
            db->handle = database_info_dir_init(db->fullpath, DATABASE_TYPE_ITERATE, task, db->show_hidden_files);
         else
            db->handle = database_info_file_init(db->fullpath, DATABASE_TYPE_ITERATE, task);
      }

      if (db->handle)
         db->handle->status = DATABASE_STATUS_ITERATE_BEGIN;
   }

   dbinfo  = db->handle;
   dbstate = &db->state;

   if (!dbinfo || task_get_cancelled(task))
      goto task_finished;

   switch (dbinfo->status)
   {
      case DATABASE_STATUS_ITERATE_BEGIN:
         if (dbstate && !dbstate->list)
         {
            if (!string_is_empty(db->content_database_path))
               dbstate->list        = dir_list_new(
                     db->content_database_path,
                     "rdb", false,
                     db->show_hidden_files,
                     false, false);

            /* If the scan path matches a database path exactly then
             * save time by only processing that database. */
            if (dbstate->list && db->is_directory)
            {
               size_t i;
               char *dirname = NULL;

               if (!string_is_empty(db->fullpath))
                  dirname    = find_last_slash(db->fullpath) + 1;

               if (!string_is_empty(dirname))
               {
                  for (i = 0; i < dbstate->list->size; i++)
                  {
                     const char *data = dbstate->list->elems[i].data;
                     char *dbname     = NULL;
                     bool strmatch    = false;
                     char *dbpath     = strdup(data);

                     path_remove_extension(dbpath);

                     dbname         = find_last_slash(dbpath) + 1;
                     strmatch       = strcasecmp(dbname, dirname) == 0;

                     free(dbpath);

                     if (strmatch)
                     {
                        struct string_list *single_list = string_list_new();
                        string_list_append(single_list,
                              data,
                              dbstate->list->elems[i].attr);
                        dir_list_free(dbstate->list);
                        dbstate->list = single_list;
                        break;
                     }
                  }
               }
            }
         }
         dbinfo->status = DATABASE_STATUS_ITERATE_START;
         break;
      case DATABASE_STATUS_ITERATE_START:
         name = database_info_get_current_element_name(dbinfo);
         task_database_cleanup_state(dbstate);
         dbstate->list_index  = 0;
         dbstate->entry_index = 0;
         task_database_iterate_start(task, dbinfo, name);
         break;
      case DATABASE_STATUS_ITERATE:
         if (task_database_iterate(db, dbstate, dbinfo) == 0)
         {
            dbinfo->status = DATABASE_STATUS_ITERATE_NEXT;
            dbinfo->type   = DATABASE_TYPE_ITERATE;
         }
         break;
      case DATABASE_STATUS_ITERATE_NEXT:
         if (task_database_iterate_next(dbinfo) == 0)
         {
            dbinfo->status = DATABASE_STATUS_ITERATE_START;
            dbinfo->type   = DATABASE_TYPE_ITERATE;
         }
         else
         {
            const char *msg = NULL;
            if (db->is_directory)
               msg = msg_hash_to_str(MSG_SCANNING_OF_DIRECTORY_FINISHED);
            else
               msg = msg_hash_to_str(MSG_SCANNING_OF_FILE_FINISHED);
#ifdef RARCH_INTERNAL
            task_free_title(task);
            task_set_title(task, strdup(msg));
            task_set_progress(task, 100);
            ui_companion_driver_notify_refresh();
#else
            fprintf(stderr, "msg: %s\n", msg);
#endif
            goto task_finished;
         }
         break;
      default:
      case DATABASE_STATUS_FREE:
      case DATABASE_STATUS_NONE:
         goto task_finished;
   }

   return;
task_finished:
   if (task)
      task_set_finished(task, true);

   if (dbstate)
   {
      if (dbstate->list)
         dir_list_free(dbstate->list);
   }

   if (db)
   {
      if (!string_is_empty(db->playlist_directory))
         free(db->playlist_directory);
      if (!string_is_empty(db->content_database_path))
         free(db->content_database_path);
      if (!string_is_empty(db->fullpath))
         free(db->fullpath);
      if (db->state.buf)
         free(db->state.buf);

      if (db->handle)
         database_info_free(db->handle);
      free(db);
   }

   if (dbinfo)
      free(dbinfo);
}

#ifdef RARCH_INTERNAL
static void task_database_progress_cb(retro_task_t *task)
{
   if (!task)
      return;
   video_display_server_set_window_progress(task->progress, task->finished);
}
#endif

bool task_push_dbscan(
      const char *playlist_directory,
      const char *content_database,
      const char *fullpath,
      bool directory,
      bool db_dir_show_hidden_files,
      retro_task_callback_t cb)
{
   retro_task_t *t             = task_init();
#ifdef RARCH_INTERNAL
   settings_t *settings        = config_get_ptr();
#endif
   db_handle_t *db             = (db_handle_t*)calloc(1, sizeof(db_handle_t));

   if (!t || !db)
      goto error;

   t->handler                  = task_database_handler;
   t->state                    = db;
   t->callback                 = cb;
   t->title                    = strdup(msg_hash_to_str(MSG_PREPARING_FOR_CONTENT_SCAN));
   t->alternative_look         = true;

#ifdef RARCH_INTERNAL
   t->progress_cb              = task_database_progress_cb;
   db->scan_without_core_match = settings->bools.scan_without_core_match;
   db->pl_fuzzy_archive_match  = settings->bools.playlist_fuzzy_archive_match;
   db->pl_use_old_format       = settings->bools.playlist_use_old_format;
#else
   db->pl_fuzzy_archive_match  = false;
   db->pl_use_old_format       = false;
#endif
   db->show_hidden_files       = db_dir_show_hidden_files;
   db->is_directory            = directory;
   db->playlist_directory      = NULL;
   db->fullpath                = strdup(fullpath);
   db->playlist_directory      = strdup(playlist_directory);
   db->content_database_path   = strdup(content_database);

   task_queue_push(t);

   return true;

error:
   if (t)
      free(t);
   if (db)
      free(db);
   return false;
}

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
#include "../ui/ui_companion_driver.h"
#include "../gfx/video_display_server.h"
#endif
#include "../retroarch.h"
#include "../verbosity.h"
#include "task_database_cue.h"

typedef struct database_state_handle
{
   database_info_list_t *info;
   struct string_list *list;
   uint8_t *buf;
   size_t list_index;
   size_t entry_index;
   uint32_t crc;
   uint32_t archive_crc;
   char archive_name[512]; /* TODO/FIXME - check size */
   char serial[4096];      /* TODO/FIXME - check size */
} database_state_handle_t;

enum db_flags_enum
{
   DB_HANDLE_FLAG_IS_DIRECTORY            = (1 << 0),
   DB_HANDLE_FLAG_SCAN_STARTED            = (1 << 1),
   DB_HANDLE_FLAG_SCAN_WITHOUT_CORE_MATCH = (1 << 2),
   DB_HANDLE_FLAG_SHOW_HIDDEN_FILES       = (1 << 3)
};

typedef struct db_handle
{
   char *playlist_directory;
   char *content_database_path;
   char *fullpath;
   database_info_handle_t *handle;
   database_state_handle_t state;
   playlist_config_t playlist_config; /* size_t alignment */
   unsigned status;
   uint8_t flags;
} db_handle_t;

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
#if 1
   /* Don't skip pruned entries, otherwise iteration
    * ends prematurely */
   if (!handle->list->elems[handle->list_ptr].data)
      return "";
#else
   /* Skip pruned entries */
   while (!handle->list->elems[handle->list_ptr].data)
   {
      if (++handle->list_ptr >= handle->list->size)
         return NULL;
   }
#endif
   return handle->list->elems[handle->list_ptr].data;
}

static void task_database_scan_console_output(const char *label, const char *db_name, bool add)
{
   char string[32];
   const char *prefix   = (add) ? "++" : (db_name) ? "==" : "??";
   const char *no_color = getenv("NO_COLOR");
   bool color           = (no_color && no_color[0] != '0') ? false : true;

   /* Colorize prefix (add = green, dupe = yellow, not found = red) */
#ifdef _WIN32
   HANDLE con      = GetStdHandle(STD_OUTPUT_HANDLE);
   if (color && con != INVALID_HANDLE_VALUE)
   {
      unsigned red    = FOREGROUND_RED;
      unsigned green  = FOREGROUND_GREEN;
      unsigned yellow = FOREGROUND_RED | FOREGROUND_GREEN;
      unsigned reset  = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
      size_t _len     = strlcpy(string, " ", sizeof(string));
      _len += strlcpy(string + _len, prefix, sizeof(string) - _len);
      _len += strlcpy(string + _len, " ",    sizeof(string) - _len);
      SetConsoleTextAttribute(con, (add) ? green : (db_name) ? yellow : red);
      WriteConsole(con, string, _len, NULL, NULL);
      SetConsoleTextAttribute(con, reset);
   }
#else
   if (color)
   {
      const char *red    = "\x1B[31m";
      const char *green  = "\x1B[32m";
      const char *yellow = "\x1B[33m";
      const char *reset  = "\x1B[0m";
      size_t _len        = 0;
      if (add)
         _len += strlcpy(string + _len, green, sizeof(string) - _len);
      else
         _len += strlcpy(string + _len, (db_name) ? yellow : red, sizeof(string) - _len);
      _len    += strlcpy(string + _len, " ",    sizeof(string) - _len);
      _len    += strlcpy(string + _len, prefix, sizeof(string) - _len);
      _len    += strlcpy(string + _len, " ",    sizeof(string) - _len);
      strlcpy(string + _len, reset,  sizeof(string) - _len);
      fputs(string, stdout);
   }
#endif
   else
   {
      size_t _len     = strlcpy(string, " ", sizeof(string));
      _len += strlcpy(string + _len, prefix, sizeof(string) - _len);
      strlcpy(string + _len, " ", sizeof(string) - _len);
      fputs(string, stdout);
   }

   if (!db_name)
      printf("\"%s\"\n", label);
   else
      printf("\"%s / %s\"\n", db_name, label);
}

static int task_database_iterate_start(retro_task_t *task,
      database_info_handle_t *db,
      const char *name)
{
   char msg[128];
   const char *basename_path = !string_is_empty(name)
         ? path_basename_nocompression(name) : "";

   msg[0] = '\0';

   if (!string_is_empty(basename_path))
      snprintf(msg, sizeof(msg),
         STRING_REP_USIZE "/" STRING_REP_USIZE ": %s..\n",
         db->list_ptr + 1,
         (size_t)db->list->size,
         basename_path);

   if (!string_is_empty(msg))
   {
#ifdef RARCH_INTERNAL
      task_free_title(task);
      task_set_title(task, strdup(msg));
      if (db->list->size != 0)
         task_set_progress(task,
               roundf((float)db->list_ptr /
                  ((float)db->list->size / 100.0f)));
      RARCH_LOG("[Scanner]: %s", msg);
      if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_DATABASE_SCAN, NULL))
         printf("%s", msg);
#else
      fprintf(stderr, "msg: %s\n", msg);
#endif
   }

   db->status = DATABASE_STATUS_ITERATE;

   return 0;
}

static int intfstream_get_serial(intfstream_t *fd, char *s, size_t len, const char *filename)
{
   const char *system_name = NULL;
   if (detect_system(fd, &system_name, filename) >= 1)
   {
      size_t system_len = strlen(system_name);
      if (string_starts_with_size(system_name, "Sony", STRLEN_CONST("Sony")))
      {
         if (STRLEN_CONST("Sony - PlayStation Portable") == system_len &&
             string_is_equal_fast(system_name, "Sony - PlayStation Portable", system_len))
         {
            if (detect_psp_game(fd, s, len, filename) != 0)
               return 1;
         }
         else if (STRLEN_CONST("Sony - PlayStation") == system_len &&
                  string_is_equal_fast(system_name, "Sony - PlayStation", system_len))
         {
            if (detect_ps1_game(fd, s, len, filename) != 0)
               return 1;
         }
         else if (STRLEN_CONST("Sony - PlayStation 2") == system_len &&
                  string_is_equal_fast(system_name, "Sony - PlayStation 2", system_len))
         {
            if (detect_ps2_game(fd, s, len, filename) != 0)
               return 1;
         }
      }
      else if (string_starts_with_size(system_name, "Nintendo", STRLEN_CONST("Nintendo")))
      {
         if (STRLEN_CONST("Nintendo - GameCube") == system_len &&
             string_is_equal_fast(system_name, "Nintendo - GameCube", system_len))
         {
            if (detect_gc_game(fd, s, len, filename) != 0)
               return 1;
         }
         else if (STRLEN_CONST("Nintendo - Wii") == system_len &&
                  string_is_equal_fast(system_name, "Nintendo - Wii", system_len))
         {
            if (detect_wii_game(fd, s, len, filename) != 0)
               return 1;
         }
      }
      else if (string_starts_with_size(system_name, "Sega", STRLEN_CONST("Sega")))
      {
         if (STRLEN_CONST("Sega - Mega-CD - Sega CD") == system_len &&
             string_is_equal_fast(system_name, "Sega - Mega-CD - Sega CD", system_len))
         {
            if (detect_scd_game(fd, s, len, filename) != 0)
               return 1;
         }
         else if (STRLEN_CONST("Sega - Saturn") == system_len &&
                  string_is_equal_fast(system_name, "Sega - Saturn", system_len))
         {
            if (detect_sat_game(fd, s, len, filename) != 0)
               return 1;
         }
         else if (STRLEN_CONST("Sega - Dreamcast") == system_len &&
                  string_is_equal_fast(system_name, "Sega - Dreamcast", system_len))
         {
            if (detect_dc_game(fd, s, len, filename) != 0)
               return 1;
         }
      }
   }
   return 0;
}

static bool intfstream_file_get_serial(const char *name,
      uint64_t offset, size_t size, char *s, size_t len)
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

   if (offset != 0 || size < (size_t) file_size)
   {
      if (intfstream_seek(fd, (int64_t)offset, SEEK_SET) == -1)
         goto error;

      data = (uint8_t*)malloc(size);

      if (intfstream_read(fd, data, size) != (int64_t) size)
      {
         free(data);
         goto error;
      }

      intfstream_close(fd);
      free(fd);
      if (!(fd = intfstream_open_memory(data, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE,
            size)))
      {
         free(data);
         return 0;
      }
   }

   rv = intfstream_get_serial(fd, s, len, name);
   intfstream_close(fd);
   free(fd);
   free(data);
   return rv;

error:
   intfstream_close(fd);
   free(fd);
   return 0;
}

static int task_database_cue_get_serial(const char *name, char *s, size_t len)
{
   char track_path[PATH_MAX_LENGTH];
   uint64_t offset  = 0;
   size_t _len      = 0;

   track_path[0]    = '\0';

   if (cue_find_track(name, true, &offset, &_len,
         track_path, sizeof(track_path)) < 0)
   {
#ifdef DEBUG
      RARCH_LOG("%s\n",
            msg_hash_to_str(MSG_COULD_NOT_FIND_VALID_DATA_TRACK));
#endif
      return 0;
   }

   return intfstream_file_get_serial(track_path, offset, _len, s, len);
}

static int task_database_gdi_get_serial(const char *name, char *s, size_t len)
{
   char track_path[PATH_MAX_LENGTH];

   track_path[0] = '\0';

   if (gdi_find_track(name, true,
               track_path, sizeof(track_path)) < 0)
   {
#ifdef DEBUG
      RARCH_LOG("%s\n",
            msg_hash_to_str(MSG_COULD_NOT_FIND_VALID_DATA_TRACK));
#endif
      return 0;
   }

   return intfstream_file_get_serial(track_path, 0, SIZE_MAX, s, len);
}

static int task_database_chd_get_serial(const char *name, char *serial, size_t len)
{
   int result;
   intfstream_t *fd = intfstream_open_chd_track(
         name,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE,
         CHDSTREAM_TRACK_FIRST_DATA);
   if (!fd)
      return 0;

   result = intfstream_get_serial(fd, serial, len, name);
   intfstream_close(fd);
   free(fd);
   return result;
}

static bool intfstream_file_get_crc(const char *name,
      uint64_t offset, size_t len, uint32_t *crc)
{
   bool rv;
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

   if (offset != 0 || len < (uint64_t) file_size)
   {
      if (intfstream_seek(fd, (int64_t)offset, SEEK_SET) == -1)
         goto error;

      data = (uint8_t*)malloc(len);

      if (intfstream_read(fd, data, len) != (int64_t)len)
         goto error;

      intfstream_close(fd);
      free(fd);
      fd = intfstream_open_memory(data, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE, len);

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
   char track_path[PATH_MAX_LENGTH];
   uint64_t offset  = 0;
   size_t _len      = 0;

   track_path[0]    = '\0';

   if (cue_find_track(name, false, &offset, &_len,
         track_path, sizeof(track_path)) < 0)
   {
#ifdef DEBUG
      RARCH_LOG("%s\n",
            msg_hash_to_str(MSG_COULD_NOT_FIND_VALID_DATA_TRACK));
#endif
      return 0;
   }

   return intfstream_file_get_crc(track_path, offset, _len, crc);
}

static int task_database_gdi_get_crc(const char *name, uint32_t *crc)
{
   char track_path[PATH_MAX_LENGTH];

   track_path[0] = '\0';

   if (gdi_find_track(name, true,
               track_path, sizeof(track_path)) < 0)
   {
#ifdef DEBUG
      RARCH_LOG("%s\n",
            msg_hash_to_str(MSG_COULD_NOT_FIND_VALID_DATA_TRACK));
#endif
      return 0;
   }

   return intfstream_file_get_crc(track_path, 0, SIZE_MAX, crc);
}

static bool task_database_chd_get_crc(const char *name, uint32_t *crc)
{
   bool found_crc   = false;
   intfstream_t *fd = intfstream_open_chd_track(
         name,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE,
         CHDSTREAM_TRACK_PRIMARY);
   if (!fd)
      return 0;

   found_crc = intfstream_get_crc(fd, crc);
   if (fd)
   {
      intfstream_close(fd);
      free(fd);
   }
   return found_crc;
}

static void task_database_cue_prune(database_info_handle_t *db,
      const char *name)
{
   size_t i;
   char path[PATH_MAX_LENGTH];
   intfstream_t *fd = intfstream_open_file(name,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!fd)
      return;

   path[0] = '\0';

   while (cue_next_file(fd, name, path, sizeof(path)))
   {
      for (i = db->list_ptr; i < db->list->size; ++i)
      {
         if (db->list->elems[i].data
               && string_is_equal(path, db->list->elems[i].data))
         {
#ifdef DEBUG
            RARCH_LOG("Pruning file referenced by cue: %s\n", path);
#endif
            free(db->list->elems[i].data);
            db->list->elems[i].data = NULL;
         }
      }
   }

   intfstream_close(fd);
   free(fd);
}

static void gdi_prune(database_info_handle_t *db, const char *name)
{
   size_t i;
   char path[PATH_MAX_LENGTH];
   intfstream_t *fd = intfstream_open_file(name,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!fd)
      return;

   path[0] = '\0';

   while (gdi_next_file(fd, name, path, sizeof(path)))
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

   free(fd);
}

static enum msg_file_type extension_to_file_type(const char *ext)
{
   char ext_lower[6];
   /* Copy and convert to lower case */
   strlcpy(ext_lower, ext, sizeof(ext_lower));
   string_to_lower(ext_lower);

   if (
            string_is_equal(ext_lower, "7z")
         || string_is_equal(ext_lower, "zip")
         || string_is_equal(ext_lower, "apk")
      )
      return FILE_TYPE_COMPRESSED;
   if (
         string_is_equal(ext_lower, "cue")
      )
      return FILE_TYPE_CUE;
   if (
         string_is_equal(ext_lower, "gdi")
      )
      return FILE_TYPE_GDI;
   if (
         string_is_equal(ext_lower, "iso")
      )
      return FILE_TYPE_ISO;
   if (
         string_is_equal(ext_lower, "chd")
      )
      return FILE_TYPE_CHD;
   if (
         string_is_equal(ext_lower, "wbfs")
      )
      return FILE_TYPE_WBFS;
   if (
         string_is_equal(ext_lower, "rvz")
      )
      return FILE_TYPE_RVZ;
   if (
         string_is_equal(ext_lower, "wia")
      )
      return FILE_TYPE_WIA;
   if (
         string_is_equal(ext_lower, "lutro")
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
         db->type = DATABASE_TYPE_CRC_LOOKUP;
         /* first check crc of archive itself */
         return intfstream_file_get_crc(name,
               0, SIZE_MAX, &db_state->archive_crc);
#else
         break;
#endif
      case FILE_TYPE_CUE:
         task_database_cue_prune(db, name);
         db_state->serial[0] = '\0';
         if (task_database_cue_get_serial(name, db_state->serial, sizeof(db_state->serial)))
            db->type = DATABASE_TYPE_SERIAL_LOOKUP;
         else
         {
            db->type = DATABASE_TYPE_CRC_LOOKUP;
            return task_database_cue_get_crc(name, &db_state->crc);
         }
         break;
      case FILE_TYPE_GDI:
         gdi_prune(db, name);
         db_state->serial[0] = '\0';
         if (task_database_gdi_get_serial(name, db_state->serial, sizeof(db_state->serial)))
            db->type = DATABASE_TYPE_SERIAL_LOOKUP;
         else
         {
            db->type = DATABASE_TYPE_CRC_LOOKUP;
            return task_database_gdi_get_crc(name, &db_state->crc);
         }
         break;
      /* Consider WBFS, RVZ and WIA files similar to ISO files. */
      case FILE_TYPE_WBFS:
      case FILE_TYPE_RVZ:
      case FILE_TYPE_WIA:
      case FILE_TYPE_ISO:
         db_state->serial[0] = '\0';
         intfstream_file_get_serial(name, 0, SIZE_MAX, db_state->serial, sizeof(db_state->serial));
         db->type            =  DATABASE_TYPE_SERIAL_LOOKUP;
         break;
      case FILE_TYPE_CHD:
         db_state->serial[0] = '\0';
         if (task_database_chd_get_serial(name, db_state->serial, sizeof(db_state->serial)))
            db->type         = DATABASE_TYPE_SERIAL_LOOKUP;
         else
         {
            db->type         = DATABASE_TYPE_CRC_LOOKUP;
            return task_database_chd_get_crc(name, &db_state->crc);
         }
         break;
      case FILE_TYPE_LUTRO:
         db->type            = DATABASE_TYPE_ITERATE_LUTRO;
         break;
      default:
         db_state->serial[0] = '\0';
         db->type            = DATABASE_TYPE_CRC_LOOKUP;
         return intfstream_file_get_crc(name, 0, SIZE_MAX, &db_state->crc);
   }

   return 1;
}

static int database_info_list_iterate_end_no_match(
      database_info_handle_t *db,
      database_state_handle_t *db_state,
      const char *path,
      bool path_contains_compressed_file)
{
   /* Reached end of database list,
    * CRC match probably didn't succeed. */
   if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_DATABASE_SCAN, NULL))
      task_database_scan_console_output(path, NULL, false);

   /* If this was a compressed file and no match in the database
    * list was found then expand the search list to include the
    * archive's contents. */
   if (!path_contains_compressed_file && path_is_compressed_file(path))
   {
      struct string_list *archive_list =
         file_archive_get_file_list(path, NULL);

      if (archive_list && archive_list->size > 0)
      {
         unsigned i;
         size_t _len  = strlen(path);

         for (i = 0; i < archive_list->size; i++)
         {
            if (_len + strlen(archive_list->elems[i].data)
                     + 1 < PATH_MAX_LENGTH)
            {
               char new_path[PATH_MAX_LENGTH];
               strlcpy(new_path, path, sizeof(new_path));
               new_path[_len] = '#';
               strlcpy(new_path + _len + 1,
                     archive_list->elems[i].data,
                     sizeof(new_path) - _len);
               string_list_append(db->list, new_path,
                     archive_list->elems[i].attr);
            }
            else
               string_list_append(db->list, path,
                     archive_list->elems[i].attr);
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

static int database_info_list_iterate_new(database_state_handle_t *db_state,
      const char *query)
{
   const char *new_database = database_info_get_current_name(db_state);

#ifndef RARCH_INTERNAL
   fprintf(stderr, "Check database [%d/%d] : %s\n",
         (unsigned)db_state->list_index,
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
   char entry_lbl[128];
   char db_playlist_base_str[NAME_MAX_LENGTH];
   /* TODO/FIXME - heap allocations are done here to avoid
    * running out of stack space on systems with a limited stack size.
    * We should use less fullsize paths in the future so that we don't
    * need to have all these big char arrays here */
   size_t str_len                 = PATH_MAX_LENGTH * sizeof(char);
   char* db_crc                   = (char*)malloc(str_len);
   char* db_playlist_path         = (char*)malloc(str_len);
   char* entry_path_str           = (char*)malloc(str_len);
   char *hash                     = NULL;
   playlist_t   *playlist         = NULL;
   const char         *db_path    =
      database_info_get_current_name(db_state);
   const char         *entry_path =
      database_info_get_current_element_name(db);
   database_info_t *db_info_entry =
      &db_state->info->list[db_state->entry_index];

   db_crc[0]                      = '\0';
   db_playlist_path[0]            = '\0';
   entry_path_str[0]              = '\0';

   fill_pathname(db_playlist_base_str,
         path_basename_nocompression(db_path), ".lpl", str_len);

   if (!string_is_empty(_db->playlist_directory))
      fill_pathname_join_special(db_playlist_path, _db->playlist_directory,
            db_playlist_base_str, str_len);

   playlist_config_set_path(&_db->playlist_config, db_playlist_path);
   playlist = playlist_init(&_db->playlist_config);

   if (!string_is_empty(db_state->serial))
   {
      size_t _len = strlcpy(db_crc, db_state->serial, str_len);
      strlcpy(db_crc  + _len,
            "|serial",
            str_len   - _len);
   }
   else
      snprintf(db_crc, str_len, "%08lX|crc", (unsigned long)db_info_entry->crc32);

   if (entry_path)
      strlcpy(entry_path_str, entry_path, str_len);

   /* Use database name for label if found,
    * otherwise use filename without extension */
   if (!string_is_empty(db_info_entry->name))
      strlcpy(entry_lbl, db_info_entry->name, sizeof(entry_lbl));
   else if (!string_is_empty(entry_path))
   {
      char *delim = (char*)strchr(entry_path, '#');

      if (delim)
         *delim = '\0';
      fill_pathname(entry_lbl,
            path_basename_nocompression(entry_path), "", str_len);

      RARCH_LOG("[Scanner]: No match for: \"%s\", CRC: 0x%08X\n", entry_path_str, db_state->crc);
   }

   if (!string_is_empty(archive_name))
      fill_pathname_join_delim(entry_path_str,
            entry_path_str, archive_name, '#', str_len);

   if (core_info_database_match_archive_member(
         db_state->list->elems[db_state->list_index].data) &&
       (hash = strchr(entry_path_str, '#')))
       *hash = '\0';

#if !defined(RARCH_INTERNAL)
   fprintf(stderr, "Found match in database !\n");

   fprintf(stderr, "Path: %s\n", db_path);
   fprintf(stderr, "CRC : %s\n", db_crc);
   fprintf(stderr, "Playlist Path: %s\n", db_playlist_path);
   fprintf(stderr, "Entry Path: %s\n", entry_path);
   fprintf(stderr, "Playlist not NULL: %d\n", playlist != NULL);
   fprintf(stderr, "ZIP entry: %s\n", archive_name);
   fprintf(stderr, "entry path str: %s\n", entry_path_str);
#endif

   if (!playlist_entry_exists(playlist, entry_path_str))
   {
      struct playlist_entry entry;

      /* the push function reads our entry as const,
       * so these casts are safe */
      entry.path              = entry_path_str;
      entry.label             = entry_lbl;
      entry.core_path         = (char*)"DETECT";
      entry.core_name         = (char*)"DETECT";
      entry.db_name           = db_playlist_base_str;
      entry.crc32             = db_crc;
      entry.subsystem_ident   = NULL;
      entry.subsystem_name    = NULL;
      entry.subsystem_roms    = NULL;
      entry.entry_slot        = 0;
      entry.runtime_hours     = 0;
      entry.runtime_minutes   = 0;
      entry.runtime_seconds   = 0;
      entry.last_played_year  = 0;
      entry.last_played_month = 0;
      entry.last_played_day   = 0;
      entry.last_played_hour  = 0;
      entry.last_played_minute= 0;
      entry.last_played_second= 0;

      playlist_push(playlist, &entry);
      RARCH_LOG("[Scanner]: Add \"%s\" to \"%s\"\n", entry_lbl, entry.db_name);
      if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_DATABASE_SCAN, NULL))
         task_database_scan_console_output(entry_lbl, path_remove_extension(db_playlist_base_str), true);
   }
   else if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_DATABASE_SCAN, NULL))
      task_database_scan_console_output(entry_lbl, path_remove_extension(db_playlist_base_str), false);

   playlist_write_file(playlist);
   playlist_free(playlist);

   database_info_list_free(db_state->info);
   free(db_state->info);

   db_state->info        = NULL;
   db_state->crc         = 0;
   db_state->archive_crc = 0;

   /* Move database to start since we are likely to match against it
      again */
   if (db_state->list_index != 0)
   {
      struct string_list_elem entry =
         db_state->list->elems[db_state->list_index];
      memmove(&db_state->list->elems[1],
              &db_state->list->elems[0],
              sizeof(entry) * db_state->list_index);
      db_state->list->elems[0] = entry;
   }

   free(db_crc);
   free(db_playlist_path);
   free(entry_path_str);
   return 0;
}

/* End of entries in database info list and didn't find a
 * match, go to the next database. */
static int database_info_list_iterate_next(
      database_state_handle_t *db_state)
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
      const char *archive_entry,
      bool path_contains_compressed_file)
{
   if (   !db_state->list
       || (unsigned)db_state->list_index == (unsigned)db_state->list->size)
      return database_info_list_iterate_end_no_match(db, db_state, name,
            path_contains_compressed_file);

   /* Archive did not contain a CRC for this entry,
    * or the file is empty. */
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

      if (!(_db->flags & DB_HANDLE_FLAG_SCAN_WITHOUT_CORE_MATCH))
      {
         /* don't scan files that can't be in this database.
          *
          * Could be because of:
          * - A matching core missing
          * - Incompatible file extension */
         if (!core_info_database_supports_content_path(
               db_state->list->elems[db_state->list_index].data, name))
            return database_info_list_iterate_next(db_state);

         if (!path_contains_compressed_file)
         {
            if (core_info_database_match_archive_member(
                  db_state->list->elems[db_state->list_index].data))
               return database_info_list_iterate_next(db_state);
         }
      }

      snprintf(query, sizeof(query),
            "{crc:or(b\"%08lX\",b\"%08lX\")}",
            (unsigned long)db_state->crc, (unsigned long)db_state->archive_crc);

      database_info_list_iterate_new(db_state, query);
   }

   if (db_state->info)
   {
      database_info_t *db_info_entry =
         &db_state->info->list[db_state->entry_index];

      if (db_info_entry && db_info_entry->crc32)
      {
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

static int task_database_iterate_playlist_lutro(
      db_handle_t *_db,
      database_state_handle_t *db_state,
      database_info_handle_t *db,
      const char *path)
{
   char db_playlist_path[PATH_MAX_LENGTH];
   playlist_t   *playlist  = NULL;

   db_playlist_path[0]     = '\0';

   if (!string_is_empty(_db->playlist_directory))
      fill_pathname_join_special(db_playlist_path,
            _db->playlist_directory,
            "Lutro.lpl", sizeof(db_playlist_path));

   playlist_config_set_path(&_db->playlist_config, db_playlist_path);
   playlist = playlist_init(&_db->playlist_config);

   if (!playlist_entry_exists(playlist, path))
   {
      struct playlist_entry entry;
      char game_title[NAME_MAX_LENGTH];
      fill_pathname(game_title,
            path_basename(path), "", sizeof(game_title));

      /* the push function reads our entry as const,
       * so these casts are safe */
      entry.path                  = (char*)path;
      entry.label                 = game_title;
      entry.core_path             = (char*)"DETECT";
      entry.core_name             = (char*)"DETECT";
      entry.db_name               = (char*)"Lutro.lpl";
      entry.crc32                 = (char*)"DETECT";
      entry.subsystem_ident       = NULL;
      entry.subsystem_name        = NULL;
      entry.subsystem_roms        = NULL;
      entry.entry_slot            = 0;
      entry.runtime_hours         = 0;
      entry.runtime_minutes       = 0;
      entry.runtime_seconds       = 0;
      entry.last_played_year      = 0;
      entry.last_played_month     = 0;
      entry.last_played_day       = 0;
      entry.last_played_hour      = 0;
      entry.last_played_minute    = 0;
      entry.last_played_second    = 0;

      playlist_push(playlist, &entry);
   }

   playlist_write_file(playlist);
   playlist_free(playlist);

   return 0;
}

static bool task_database_check_serial_and_crc(
      database_state_handle_t *db_state)
{
#ifdef RARCH_INTERNAL
   if (!config_get_ptr()->bools.scan_serial_and_crc)
       return false;
#endif
   /* the PSP shares serials for disc/download content */
   return string_starts_with(
         path_basename_nocompression(database_info_get_current_name(db_state)),
         "Sony - PlayStation Portable");
}

static int task_database_iterate_serial_lookup(
      db_handle_t *_db,
      database_state_handle_t *db_state,
      database_info_handle_t *db, const char *name,
      bool path_contains_compressed_file)
{
   if (
         !db_state->list ||
         (unsigned)db_state->list_index == (unsigned)db_state->list->size
      )
      return database_info_list_iterate_end_no_match(db, db_state, name,
            path_contains_compressed_file);

   if (db_state->entry_index == 0)
   {
      size_t _len;
      char query[50];
      char *serial_buf = bin_to_hex_alloc(
            (uint8_t*)db_state->serial,
            strlen(db_state->serial) * sizeof(uint8_t));

      if (!serial_buf)
         return 1;

      _len  = strlcpy(query, "{'serial': b'", sizeof(query));
      _len += strlcpy(query + _len, serial_buf, sizeof(query) - _len);
      query[  _len] = '\'';
      query[++_len] = '}';
      query[++_len] = '\0';
      database_info_list_iterate_new(db_state, query);

      free(serial_buf);
   }

   if (db_state->info)
   {
      database_info_t *db_info_entry = &db_state->info->list[
         db_state->entry_index];

      if (db_info_entry && db_info_entry->serial)
      {
         if (string_is_equal(db_state->serial, db_info_entry->serial))
         {
            if (task_database_check_serial_and_crc(db_state))
            {
               if (db_state->crc == 0)
                  intfstream_file_get_crc(name, 0, SIZE_MAX, &db_state->crc);
               if (db_state->crc == db_info_entry->crc32)
                  return database_info_list_iterate_found_match(_db,
                        db_state, db, NULL);
            }
            else
               return database_info_list_iterate_found_match(_db,
                     db_state, db, NULL);
         }
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
      const char *name,
      database_state_handle_t *db_state,
      database_info_handle_t *db,
      bool path_contains_compressed_file)
{
   switch (db->type)
   {
      case DATABASE_TYPE_ITERATE:
         return task_database_iterate_playlist(db_state, db, name);
      case DATABASE_TYPE_ITERATE_ARCHIVE:
#ifdef HAVE_COMPRESSION
         return task_database_iterate_crc_lookup(
               _db, db_state, db, name, db_state->archive_name,
               path_contains_compressed_file);
#else
         return 1;
#endif
      case DATABASE_TYPE_ITERATE_LUTRO:
         return task_database_iterate_playlist_lutro(_db, db_state, db, name);
      case DATABASE_TYPE_SERIAL_LOOKUP:
         return task_database_iterate_serial_lookup(_db, db_state, db, name,
               path_contains_compressed_file);
      case DATABASE_TYPE_CRC_LOOKUP:
         return task_database_iterate_crc_lookup(_db, db_state, db, name, NULL,
               path_contains_compressed_file);
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
   uint8_t flg;
   const char *name                 = NULL;
   database_info_handle_t  *dbinfo  = NULL;
   database_state_handle_t *dbstate = NULL;
   db_handle_t *db                  = NULL;

   if (!task)
      goto task_finished;

   db      = (db_handle_t*)task->state;

   if (!db)
      goto task_finished;

   if (!(db->flags & DB_HANDLE_FLAG_SCAN_STARTED))
   {
      db->flags       |= DB_HANDLE_FLAG_SCAN_STARTED;

      if (!string_is_empty(db->fullpath))
      {
         if (db->flags & DB_HANDLE_FLAG_IS_DIRECTORY)
            db->handle = database_info_dir_init(
                  db->fullpath, DATABASE_TYPE_ITERATE,
                  task, db->flags & DB_HANDLE_FLAG_SHOW_HIDDEN_FILES);
         else
            db->handle = database_info_file_init(
                  db->fullpath, DATABASE_TYPE_ITERATE,
                  task);
      }

      if (db->handle)
         db->handle->status = DATABASE_STATUS_ITERATE_BEGIN;
   }

   dbinfo  = db->handle;
   dbstate = &db->state;
   flg     = task_get_flags(task);

   if (!dbinfo || ((flg & RETRO_TASK_FLG_CANCELLED) > 0))
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
                     db->flags & DB_HANDLE_FLAG_SHOW_HIDDEN_FILES,
                     false, false);

            RARCH_LOG("[Scanner]: %s\"%s\"..\n", msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_START), db->fullpath);
            if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_DATABASE_SCAN, NULL))
               printf("%s\"%s\"..\n", msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_START), db->fullpath);

            /* If the scan path matches a database path exactly then
             * save time by only processing that database. */
            if (dbstate->list && (db->flags & DB_HANDLE_FLAG_IS_DIRECTORY))
            {
               size_t i;
               char *dirname = NULL;

               if (!string_is_empty(db->fullpath))
               {
                  char *last_slash      = find_last_slash(db->fullpath);
                  dirname               = last_slash + 1;
               }

               if (!string_is_empty(dirname))
               {
                  for (i = 0; i < dbstate->list->size; i++)
                  {
                     char *last_slash;
                     const char *data = dbstate->list->elems[i].data;
                     bool strmatch    = false;
                     char *dbpath     = strdup(data);

                     path_remove_extension(dbpath);

                     last_slash       = find_last_slash(dbpath);
                     strmatch         = strcasecmp(last_slash + 1, dirname) == 0;

                     free(dbpath);

                     if (strmatch)
                     {
                        struct string_list *single_list = string_list_new();
                        string_list_append(single_list, data,
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
         name                 = database_info_get_current_element_name(dbinfo);
         task_database_cleanup_state(dbstate);
         dbstate->list_index  = 0;
         dbstate->entry_index = 0;
         task_database_iterate_start(task, dbinfo, name);
         break;
      case DATABASE_STATUS_ITERATE:
         {
            bool path_contains_compressed_file = false;
            const char *name                   =
               database_info_get_current_element_name(dbinfo);
            if (!name)
               goto task_finished;

            path_contains_compressed_file      = path_contains_compressed_file(name);
            if (path_contains_compressed_file)
               if (dbinfo->type == DATABASE_TYPE_ITERATE)
                  dbinfo->type   = DATABASE_TYPE_ITERATE_ARCHIVE;

            if (task_database_iterate(db, name, dbstate, dbinfo,
                     path_contains_compressed_file) == 0)
            {
               dbinfo->status    = DATABASE_STATUS_ITERATE_NEXT;
               dbinfo->type      = DATABASE_TYPE_ITERATE;
            }
         }
         break;
      case DATABASE_STATUS_ITERATE_NEXT:
         dbinfo->list_ptr++;

         if (dbinfo->list_ptr < dbinfo->list->size)
         {
            dbinfo->status = DATABASE_STATUS_ITERATE_START;
            dbinfo->type   = DATABASE_TYPE_ITERATE;
         }
         else
         {
            const char *msg = NULL;
            if (db->flags & DB_HANDLE_FLAG_IS_DIRECTORY)
               msg = msg_hash_to_str(MSG_SCANNING_OF_DIRECTORY_FINISHED);
            else
               msg = msg_hash_to_str(MSG_SCANNING_OF_FILE_FINISHED);
#ifdef RARCH_INTERNAL
            task_free_title(task);
            task_set_title(task, strdup(msg));
            task_set_progress(task, 100);
            ui_companion_driver_notify_refresh();
            RARCH_LOG("[Scanner]: %s\n", msg);
            if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_DATABASE_SCAN, NULL))
               printf("%s\n", msg);
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
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);

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
   if (task)
      video_display_server_set_window_progress(task->progress,
            ((task->flags & RETRO_TASK_FLG_FINISHED) > 0));
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
   retro_task_t *t                         = task_init();
#ifdef RARCH_INTERNAL
   settings_t *settings                    = config_get_ptr();
#endif
   db_handle_t *db                         = (db_handle_t*)calloc(1, sizeof(db_handle_t));

   if (!t || !db)
      goto error;

   t->handler                              = task_database_handler;
   t->state                                = db;
   t->callback                             = cb;
   t->title                                = strdup(msg_hash_to_str(
            MSG_PREPARING_FOR_CONTENT_SCAN));
   t->flags                               |= RETRO_TASK_FLG_ALTERNATIVE_LOOK;
#ifdef RARCH_INTERNAL
   t->progress_cb                          = task_database_progress_cb;
   if (settings->bools.scan_without_core_match)
      db->flags |= DB_HANDLE_FLAG_SCAN_WITHOUT_CORE_MATCH;
   db->playlist_config.capacity            = COLLECTION_SIZE;
   db->playlist_config.old_format          = settings->bools.playlist_use_old_format;
   db->playlist_config.compress            = settings->bools.playlist_compression;
   db->playlist_config.fuzzy_archive_match = settings->bools.playlist_fuzzy_archive_match;
   playlist_config_set_base_content_directory(&db->playlist_config, settings->bools.playlist_portable_paths ? settings->paths.directory_menu_content : NULL);
#else
   db->playlist_config.capacity            = COLLECTION_SIZE;
   db->playlist_config.old_format          = false;
   db->playlist_config.compress            = false;
   db->playlist_config.fuzzy_archive_match = false;
   playlist_config_set_base_content_directory(&db->playlist_config, NULL);
#endif
   if (db_dir_show_hidden_files)
      db->flags |= DB_HANDLE_FLAG_SHOW_HIDDEN_FILES;
   if (directory)
      db->flags |= DB_HANDLE_FLAG_IS_DIRECTORY;
   db->fullpath                            = strdup(fullpath);
   db->playlist_directory                  = strdup(playlist_directory);
   db->content_database_path               = strdup(content_database);

   task_queue_push(t);

   return true;

error:
   if (t)
      free(t);
   if (db)
      free(db);
   return false;
}

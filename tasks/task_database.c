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
#include <lists/string_list.h>
#include <file/file_path.h>
#include <formats/logiqx_dat.h>
#include <formats/m3u_file.h>
#include <encodings/crc32.h>
#include <streams/interface_stream.h>
#include "tasks_internal.h"

#include "../core_info.h"
#include "../database_info.h"
#include "../manual_content_scan.h"

#include "../file_path_special.h"
#include "../msg_hash.h"
#include "../playlist.h"
#ifdef RARCH_INTERNAL
#include "../configuration.h"
#include "../ui/ui_companion_driver.h"
#include "../gfx/video_display_server.h"
#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#endif
#include "../runloop.h"
#endif
#include "../retroarch.h"
#include "../verbosity.h"
#include "task_database_cue.h"

#define MAX_DATABASE_COUNT 256

/* Scan result structure for accumulating identification results */
typedef struct scan_result
{
   char *entry_path;       /* Full path to the ROM file */
   char *entry_label;      /* Display label (usually game name) */
   char *db_crc;           /* CRC or serial identifier */
   char *db_name;          /* Database/playlist name (e.g., "Sega - Genesis.lpl") */
   char *archive_name;     /* Archive entry name if inside zip, NULL otherwise */
} scan_result_t;

/* Result accumulation infrastructure */
typedef struct scan_results
{
   scan_result_t *results; /* Dynamic array of results */
   size_t count;           /* Number of results */
   size_t capacity;        /* Allocated capacity */
} scan_results_t;

/* Helper functions for result accumulation */
static bool scan_results_init(scan_results_t *sr, size_t initial_capacity)
{
   sr->results = (scan_result_t*)malloc(initial_capacity * sizeof(scan_result_t));
   if (!sr->results)
      return false;
   sr->count = 0;
   sr->capacity = initial_capacity;
   return true;
}

static bool scan_results_ensure_capacity(scan_results_t *sr)
{
   if (sr->count >= sr->capacity)
   {
      size_t new_capacity = sr->capacity * 2;
      scan_result_t *new_results = (scan_result_t*)realloc(
         sr->results, new_capacity * sizeof(scan_result_t));
      if (!new_results)
         return false;
      sr->results = new_results;
      sr->capacity = new_capacity;
   }
   return true;
}

static bool scan_results_add(scan_results_t *sr,
   const char *entry_path, const char *entry_label,
   const char *db_crc, const char *db_name, const char *archive_name)
{
   scan_result_t *result;

   if (!scan_results_ensure_capacity(sr))
      return false;

   result = &sr->results[sr->count];
   result->entry_path = strdup(entry_path);
   result->entry_label = strdup(entry_label);
   result->db_crc = strdup(db_crc);
   result->db_name = strdup(db_name);
   result->archive_name = archive_name ? strdup(archive_name) : NULL;

   if (!result->entry_path || !result->entry_label ||
       !result->db_crc || !result->db_name)
   {
      /* Allocation failed, cleanup */
      if (result->entry_path) free(result->entry_path);
      if (result->entry_label) free(result->entry_label);
      if (result->db_crc) free(result->db_crc);
      if (result->db_name) free(result->db_name);
      if (result->archive_name) free(result->archive_name);
      return false;
   }

   sr->count++;
   return true;
}

static void scan_results_free(scan_results_t *sr)
{
   size_t i;
   for (i = 0; i < sr->count; i++)
   {
      if (sr->results[i].entry_path) free(sr->results[i].entry_path);
      if (sr->results[i].entry_label) free(sr->results[i].entry_label);
      if (sr->results[i].db_crc) free(sr->results[i].db_crc);
      if (sr->results[i].db_name) free(sr->results[i].db_name);
      if (sr->results[i].archive_name) free(sr->results[i].archive_name);
   }
   if (sr->results)
      free(sr->results);
   sr->results = NULL;
   sr->count = 0;
   sr->capacity = 0;
}

enum db_state_flags_enum
{
   DB_STATE_FLAG_HAS_SERIAL               = (1 << 0),
   DB_STATE_FLAG_HAS_CRC                  = (1 << 1),
   DB_STATE_FLAG_HAS_SIZE                 = (1 << 2),
   DB_STATE_FLAG_MATCHED                  = (1 << 3)
};

typedef struct database_state_handle
{
   database_info_list_t *info;
   struct string_list *list;
   struct string_list *m3u_list;  /* List of M3U files found during scan */
   uint8_t *buf;
   size_t list_index;
   size_t entry_index;
   uint32_t crc;
   uint32_t archive_crc;
   uint64_t size;
   uint64_t archive_size;
   char archive_name[512]; /* TODO/FIXME - check size */
   char serial[4096];      /* TODO/FIXME - check size */
   int64_t min_sizes[MAX_DATABASE_COUNT];
   int64_t max_sizes[MAX_DATABASE_COUNT];
   uint8_t flags[MAX_DATABASE_COUNT];
} database_state_handle_t;

enum db_flags_enum
{
   DB_HANDLE_FLAG_IS_DIRECTORY            = (1 << 0),
   DB_HANDLE_FLAG_SCAN_STARTED            = (1 << 1),
   DB_HANDLE_FLAG_SCAN_WITHOUT_CORE_MATCH = (1 << 2),
   DB_HANDLE_FLAG_SHOW_HIDDEN_FILES       = (1 << 3),
   DB_HANDLE_FLAG_USE_FIRST_MATCH_ONLY    = (1 << 4)
};

typedef struct db_handle
{
   char *playlist_directory;
   char *content_database_path;
   char *fullpath;
   database_info_handle_t *handle;
   database_state_handle_t state;
   playlist_config_t playlist_config; /* size_t alignment */
   scan_results_t scan_results;
   unsigned status;
   uint8_t flags;
} db_handle_t;

enum manual_scan_status
{
   MANUAL_SCAN_BEGIN = 0,
   MANUAL_SCAN_ITERATE_CLEAN,
   MANUAL_SCAN_ITERATE_CONTENT,
   MANUAL_SCAN_ITERATE_M3U,
   MANUAL_SCAN_END
};

typedef struct manual_scan_handle
{
   manual_content_scan_task_config_t *task_config;
   playlist_t *playlist;
   struct string_list *file_exts_list;
   struct string_list *content_list;
   logiqx_dat_t *dat_file;
   struct string_list *m3u_list;
   playlist_config_t playlist_config; /* size_t alignment */
   size_t playlist_size;
   size_t playlist_index;
   size_t content_list_size;
   size_t content_list_index;
   size_t m3u_index;
   enum manual_scan_status status;
} manual_scan_handle_t;

#ifdef HAVE_LIBRETRODB

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
         STRING_REP_USIZE "/" STRING_REP_USIZE ": \"%s\"...\n",
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
      RARCH_LOG("[Scanner] %s", msg);
      if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_DATABASE_SCAN, NULL))
         printf("%s", msg);
#else
      fprintf(stderr, "msg: %s\n", msg);
#endif
   }

   db->status = DATABASE_STATUS_ITERATE;

   return 0;
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
            RARCH_DBG("[Scanner] Pruning file referenced by CUE: \"%s\".\n", path);
            free(db->list->elems[i].data);
            db->list->elems[i].data = NULL;
         }
      }
   }

   intfstream_close(fd);
   free(fd);
}

/* Remove disc indicators from title string */
/* Helper function to validate if a string is a valid disc indicator
 * Valid formats:
 *   - Single/double digit: 0-99
 *   - Single letter: A-Z
 *   - Roman numerals: I, II, III, IV, V, VI, VII, VIII, IX, X, etc.
 *   - "X of Y" format: 1 of 2, 01 of 10, etc.
 */
static bool is_valid_disc_indicator(const char *str, size_t len)
{
   const char *p = str;
   const char *end = str + len;

   if (len == 0 || len > 10) /* Sanity check */
      return false;

   /* Check for single letter (A-Z) */
   if (len == 1 && isalpha((unsigned char)*p))
      return true;

   /* Check for 1-2 digit number (0-99) */
   if (len <= 2 && isdigit((unsigned char)*p))
   {
      p++;
      if (p == end)
         return true; /* Single digit */
      if (isdigit((unsigned char)*p) && p + 1 == end)
         return true; /* Double digit */
      return false;
   }

   /* Check for "X of Y" pattern where X and Y are 1-2 digits */
   if (len >= 5 && isdigit((unsigned char)*p))
   {
      /* Parse first number (1-2 digits) */
      p++;
      if (p < end && isdigit((unsigned char)*p))
         p++;

      /* Check for " of " */
      if (p + 4 <= end && strncmp(p, " of ", 4) == 0)
      {
         p += 4;
         /* Parse second number (1-2 digits) */
         if (p < end && isdigit((unsigned char)*p))
         {
            p++;
            if (p < end && isdigit((unsigned char)*p))
               p++;
            if (p == end)
               return true;
         }
      }
      return false;
   }

   /* Check for Roman numerals (I, II, III, IV, V, VI, VII, VIII, IX, X, etc.) */
   /* Valid Roman numeral chars: I, V, X (we'll be conservative) */
   if (len >= 1 && len <= 4)
   {
      bool all_roman = true;
      const char *roman_p = str;
      while (roman_p < end)
      {
         char c = *roman_p;
         if (c != 'I' && c != 'V' && c != 'X')
         {
            all_roman = false;
            break;
         }
         roman_p++;
      }
      if (all_roman)
         return true;
   }

   return false;
}

static void remove_disc_indicators(char *title, size_t len)
{
   char *disc_pos = NULL;

   /* Search for common disc patterns */
   if ((disc_pos = strstr(title, " (Disc ")) ||
       (disc_pos = strstr(title, " (disc ")) ||
       (disc_pos = strstr(title, " (Disk ")) ||
       (disc_pos = strstr(title, " (disk ")))
   {
      /* Find the closing parenthesis */
      char *end_pos = strchr(disc_pos, ')');
      if (end_pos)
      {
         /* Extract the disc indicator text (between " (Disc " and ")") */
         const char *indicator_start = disc_pos + 7; /* Skip " (Disc " */
         size_t indicator_len = end_pos - indicator_start;

         /* Validate this is actually a disc indicator, not arbitrary text */
         if (is_valid_disc_indicator(indicator_start, indicator_len))
         {
            /* Truncate at the disc indicator */
            *disc_pos = '\0';
            /* Remove trailing whitespace */
            string_trim_whitespace_right(title);
         }
      }
   }
}

static void task_database_iterate_m3u(
      db_handle_t *_db,
      database_state_handle_t *db_state,
      const char *m3u_path)
{
   size_t i, j;
   bool found_match = false;
   char first_matched_db[NAME_MAX_LENGTH];
   char first_matched_crc[128];
   char collapsed_title[NAME_MAX_LENGTH];
   m3u_file_t *m3u_file = NULL;

   first_matched_db[0] = '\0';
   first_matched_crc[0] = '\0';
   collapsed_title[0] = '\0';

   /* Open M3U file */
   if (!(m3u_file = m3u_file_init(m3u_path)))
   {
      RARCH_ERR("[Scanner] Failed to open M3U file: \"%s\".\n", m3u_path);
      return;
   }

   /* Scan each referenced file and check if it's in scan_results */
   for (i = 0; i < m3u_file_get_size(m3u_file); i++)
   {
      m3u_file_entry_t *entry = NULL;
      const char *ref_path = NULL;

      if (!m3u_file_get_entry(m3u_file, i, &entry))
         continue;

      ref_path = entry->full_path;
      if (string_is_empty(ref_path))
         continue;

      /* Look for this file in scan results */
      for (j = 0; j < _db->scan_results.count; j++)
      {
         scan_result_t *result = &_db->scan_results.results[j];
         char result_path_resolved[PATH_MAX_LENGTH];

         result_path_resolved[0] = '\0';

         if (!result->entry_path)
            continue;

         /* Resolve the scan result path to absolute form for comparison */
         strlcpy(result_path_resolved, result->entry_path,
               sizeof(result_path_resolved));
         path_resolve_realpath(result_path_resolved,
               sizeof(result_path_resolved), false);

         if (string_is_equal(ref_path, result_path_resolved))
         {
            /* Found a match! */
            if (!found_match)
            {
               /* First match - save the info */
               found_match = true;
               strlcpy(first_matched_db, result->db_name,
                     sizeof(first_matched_db));
               strlcpy(first_matched_crc, result->db_crc,
                     sizeof(first_matched_crc));
               strlcpy(collapsed_title, result->entry_label,
                     sizeof(collapsed_title));

               /* Remove disc indicator from title */
               remove_disc_indicators(collapsed_title,
                     sizeof(collapsed_title));
            }

            /* Mark this result for removal */
            /* We'll remove it by setting entry_path to NULL */
            /* and compacting the array later */
            if (result->entry_path)
            {
               free(result->entry_path);
               result->entry_path = NULL;
            }
         }
      }
   }

   m3u_file_free(m3u_file);

   /* If we found at least one match, add M3U entry */
   if (found_match)
   {
      if (!scan_results_add(&_db->scan_results, m3u_path, collapsed_title,
                            first_matched_crc, first_matched_db, NULL))
      {
         RARCH_ERR("[Scanner] Failed to add M3U result: \"%s\".\n", m3u_path);
      }
      else
      {
         RARCH_LOG("[Scanner] Matched M3U \"%s\" to \"%s\".\n",
                  collapsed_title, first_matched_db);
      }
   }

   /* Compact scan_results to remove NULL entries */
   {
      size_t write_idx = 0;
      for (i = 0; i < _db->scan_results.count; i++)
      {
         if (_db->scan_results.results[i].entry_path != NULL)
         {
            if (write_idx != i)
               _db->scan_results.results[write_idx] =
                  _db->scan_results.results[i];
            write_idx++;
         }
         else
         {
            /* Free any remaining allocated fields */
            if (_db->scan_results.results[i].entry_label)
               free(_db->scan_results.results[i].entry_label);
            if (_db->scan_results.results[i].db_crc)
               free(_db->scan_results.results[i].db_crc);
            if (_db->scan_results.results[i].db_name)
               free(_db->scan_results.results[i].db_name);
            if (_db->scan_results.results[i].archive_name)
               free(_db->scan_results.results[i].archive_name);
         }
      }
      _db->scan_results.count = write_idx;
   }
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
            RARCH_DBG("[Scanner] Pruning file referenced by GDI: \"%s\".\n", path);
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
         return intfstream_file_get_crc_and_size(name,
               0, INT64_MAX, &db_state->archive_crc, &db_state->archive_size);
#else
         break;
#endif
      case FILE_TYPE_CUE:
         task_database_cue_prune(db, name);
         db_state->serial[0] = '\0';
         if (task_database_cue_get_serial(name, db_state->serial, sizeof(db_state->serial),&db_state->size))
            db->type = DATABASE_TYPE_SERIAL_LOOKUP;
         else
         {
            db->type = DATABASE_TYPE_CRC_LOOKUP;
            db_state->serial[0] = '\0';
            RARCH_DBG("[Scanner] CUE file serial not detected, fallback to crc.\n");
            return task_database_cue_get_crc_and_size(name, &db_state->crc, &db_state->size);
         }
         break;
      case FILE_TYPE_GDI:
         gdi_prune(db, name);
         db_state->serial[0] = '\0';
         if (task_database_gdi_get_serial(name, db_state->serial, sizeof(db_state->serial),&db_state->size))
            db->type = DATABASE_TYPE_SERIAL_LOOKUP;
         else
         {
            db->type = DATABASE_TYPE_CRC_LOOKUP;
            db_state->serial[0] = '\0';
            RARCH_DBG("[Scanner] GDI file serial not detected, fallback to crc.\n");
            return task_database_gdi_get_crc_and_size(name, &db_state->crc, &db_state->size);
         }
         break;
      /* Consider WBFS, RVZ and WIA files similar to ISO files. */
      case FILE_TYPE_WBFS:
      case FILE_TYPE_RVZ:
      case FILE_TYPE_WIA:
         db_state->serial[0] = '\0';
         intfstream_file_get_serial(name, 0, INT64_MAX, db_state->serial, sizeof(db_state->serial),&db_state->size);
         db->type            =  DATABASE_TYPE_SERIAL_LOOKUP;
         break;
      case FILE_TYPE_ISO:
         db_state->serial[0] = '\0';
         intfstream_file_get_serial(name, 0, INT64_MAX, db_state->serial, sizeof(db_state->serial),&db_state->size);
         db->type            =  DATABASE_TYPE_SERIAL_LOOKUP_SIZEHINT;
         break;
      case FILE_TYPE_CHD:
         db_state->serial[0] = '\0';
         if (task_database_chd_get_serial(name, db_state->serial, sizeof(db_state->serial),&db_state->size))
            db->type         = DATABASE_TYPE_SERIAL_LOOKUP;
         else
         {
            db->type         = DATABASE_TYPE_CRC_LOOKUP;
            db_state->serial[0] = '\0';
            RARCH_DBG("[Scanner] CHD file serial not detected, fallback to crc.\n");
            return task_database_chd_get_crc_and_size(name, &db_state->crc, &db_state->size);
         }
         break;
      case FILE_TYPE_LUTRO:
         db->type            = DATABASE_TYPE_ITERATE_LUTRO;
         break;
      default:
         db_state->serial[0] = '\0';
         db->type            = DATABASE_TYPE_CRC_LOOKUP;
         return intfstream_file_get_crc_and_size(name, 0, INT64_MAX, &db_state->crc, &db_state->size);
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

         /*if (archive_list->size == 1) TODO: flag single-file-archives for future use */
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
   else
      RARCH_LOG("[Scanner] No match for: \"%s\" (%s %08X).\n", path,
                db_state->serial, db_state->crc);

   db_state->list_index   = 0;
   db_state->entry_index  = 0;
   db_state->size         = 0;
   db_state->archive_size = 0;
   db_state->serial[0]    = '\0';

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
   char* entry_path_str           = (char*)malloc(str_len);
   char *hash                     = NULL;
   const char         *db_path    =
      database_info_get_current_name(db_state);
   const char         *entry_path =
      database_info_get_current_element_name(db);
   database_info_t *db_info_entry =
      &db_state->info->list[db_state->entry_index];

   db_crc[0]                      = '\0';
   entry_path_str[0]              = '\0';

   fill_pathname(db_playlist_base_str,
         path_basename_nocompression(db_path), ".lpl", sizeof(db_playlist_base_str));

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
   {
      /* Use the archive as path instead of the file inside the archive
       * if the file is a multidisk game, because database entry
       * matches with the last disk, which is never bootable */
      char *delim = (char*)strchr(entry_path_str, '#');

      if (delim && strcasestr(entry_path_str, " (Disk "))
         *delim = '\0';

      strlcpy(entry_lbl, db_info_entry->name, sizeof(entry_lbl));
   }
   else if (!string_is_empty(entry_path))
   {
      char *delim = (char*)strchr(entry_path, '#');

      if (delim)
         *delim = '\0';
      fill_pathname(entry_lbl,
            path_basename_nocompression(entry_path), "", sizeof(entry_lbl));

      RARCH_LOG("[Scanner] Faulty match for: \"%s\", CRC: 0x%08X.\n", entry_path_str, db_state->crc);
   }

   if (!string_is_empty(archive_name))
      fill_pathname_join_delim(entry_path_str,
            entry_path_str, archive_name, '#', str_len);

   if (core_info_database_match_archive_member(
         db_state->list->elems[db_state->list_index].data)
       && (hash = strchr(entry_path_str, '#')))
       *hash = '\0';

#if !defined(RARCH_INTERNAL)
   fprintf(stderr, "*** Found match in database! ***\n");

   fprintf(stderr, "\tPath: %s\n", db_path);
   fprintf(stderr, "\tCRC : %s\n", db_crc);
   fprintf(stderr, "\tEntry Path: %s\n", entry_path);
   fprintf(stderr, "\tZIP entry: %s\n", archive_name);
   fprintf(stderr, "\tentry path str: %s\n", entry_path_str);
#endif

   /* Accumulate result instead of immediately updating playlist */
   if (!scan_results_add(&_db->scan_results, entry_path_str, entry_lbl,
                         db_crc, db_playlist_base_str, archive_name))
      RARCH_ERR("[Scanner] Failed to add result for: \"%s\".\n", entry_lbl);

   database_info_list_free(db_state->info);
   free(db_state->info);

   db_state->info         = NULL;
   db_state->crc          = 0;
   db_state->archive_crc  = 0;
   db_state->size         = 0;
   db_state->archive_size = 0;
   db_state->serial[0]    = '\0';

   /* Move database to start since we are likely to match against it
      again */
   if (db_state->list_index != 0)
   {
      struct string_list_elem entry =
         db_state->list->elems[db_state->list_index];
      uint64_t min = db_state->min_sizes[db_state->list_index];
      uint64_t max = db_state->max_sizes[db_state->list_index];
      uint8_t flag = db_state->flags[db_state->list_index];
      memmove(&db_state->list->elems[1],
              &db_state->list->elems[0],
              sizeof(entry) * db_state->list_index);
      memmove(&db_state->min_sizes[1],
              &db_state->min_sizes[0],
              sizeof(min) * db_state->list_index);
      memmove(&db_state->max_sizes[1],
              &db_state->max_sizes[0],
              sizeof(max) * db_state->list_index);
      memmove(&db_state->flags[1],
              &db_state->flags[0],
              sizeof(flag) * db_state->list_index);

      db_state->list->elems[0] = entry;
      db_state->min_sizes[0] = min;
      db_state->max_sizes[0] = max;
      db_state->flags[0] = flag;
      db_state->flags[0] |= DB_STATE_FLAG_MATCHED;
   }

   free(db_crc);
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

static void task_database_fill_db_min_max(database_state_handle_t *db_state)
{
   char query[50];
   query[0] = '\0';

   snprintf(query, sizeof(query), "{size:min(0)}");
   database_info_list_iterate_new(db_state, query);

   if (db_state->info->count > 0)
   {
      db_state->min_sizes[db_state->list_index] = db_state->info->list[db_state->info->count-1].size;
      snprintf(query, sizeof(query), "{size:max(0)}");
      database_info_list_iterate_new(db_state, query);

      if (db_state->info->count > 0)
      {
         size_t i;
         db_state->max_sizes[db_state->list_index] = db_state->info->list[db_state->info->count-1].size;
         db_state->flags[db_state->list_index] |= DB_STATE_FLAG_HAS_SIZE;
         for(i=0 ; i < db_state->info->count; i++)
         {
            if (db_state->info->list[i].serial && strlen(db_state->info->list[i].serial)>0)
            {
               db_state->flags[db_state->list_index] |= DB_STATE_FLAG_HAS_SERIAL;
            }
            if (db_state->info->list[i].crc32 > 0)
            {
               db_state->flags[db_state->list_index] |= DB_STATE_FLAG_HAS_CRC;
            }
         }
      }
#ifdef DEBUG
      RARCH_DBG("[Scanner] Queried min/max, values %ld / %ld, size %s serial %s crc %s\n",
             db_state->min_sizes[db_state->list_index],
             db_state->max_sizes[db_state->list_index],
             db_state->flags[db_state->list_index] & DB_STATE_FLAG_HAS_SIZE   ? "yes" : "no",
             db_state->flags[db_state->list_index] & DB_STATE_FLAG_HAS_SERIAL ? "yes" : "no",
             db_state->flags[db_state->list_index] & DB_STATE_FLAG_HAS_CRC    ? "yes" : "no");
#endif
   }
   /* Unsuccessful query (no size info), use placeholder */
   else
   {
      db_state->min_sizes[db_state->list_index] = -1;
      db_state->max_sizes[db_state->list_index] = -1;
#ifdef DEBUG
      RARCH_DBG("[Scanner] Queried min/max, size field not found.\n");
#endif
   }
   db_state->entry_index = 0;
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
       || (unsigned)db_state->list_index == (unsigned)db_state->list->size
       || ( _db->flags & DB_HANDLE_FLAG_USE_FIRST_MATCH_ONLY &&
            db_state->list_index > 0 &&
            db_state->flags[0] & DB_STATE_FLAG_MATCHED))
      return database_info_list_iterate_end_no_match(db, db_state, name,
            path_contains_compressed_file);

   /* Archive did not contain a CRC for this entry,
    * or the file is empty. */
   if (!db_state->crc)
   {
#ifdef DEBUG
      RARCH_DBG("[Scanner] Extra crc check 1: %x %d / %x %d %s\n",
            db_state->crc, db_state->size, db_state->archive_crc, db_state->archive_size,
            path_contains_compressed_file ? "compressed:true" : "compressed:false");
#endif
      db_state->crc = file_archive_get_file_crc32_and_size(name, &db_state->size);
#ifdef DEBUG
      RARCH_DBG("[Scanner] Extra crc check 2: %x %d / %x %d.\n",
            db_state->crc, db_state->size, db_state->archive_crc, db_state->archive_size);
#endif
      if (!db_state->crc)
         return database_info_list_iterate_next(db_state);
   }

   /* If size boundaries are not filled for this DB, run the queries */
   if (db_state->min_sizes[db_state->list_index] == 0)
      task_database_fill_db_min_max(db_state);

   if (db_state->min_sizes[db_state->list_index] > 0)
   {
      /* Examining zip file main entry (archive size filled, but no indication of compressed file) */
      if ( !path_contains_compressed_file && db_state->archive_size > 0)
      {
         if (       ( db_state->min_sizes[db_state->list_index] > (int64_t) db_state->archive_size
                   && db_state->min_sizes[db_state->list_index] > (int64_t) db_state->size )
              || (    db_state->max_sizes[db_state->list_index] < (int64_t) db_state->archive_size
                   && db_state->max_sizes[db_state->list_index] < (int64_t) db_state->size ))
         {
#ifdef DEBUG
            RARCH_DBG("[Scanner] Skipping DB, neither archive nor uncompressed size %ld/%ld is in range.\n",
                  db_state->archive_size, db_state->size);
#endif
            return database_info_list_iterate_next(db_state);
         }
      }
      /* Any other case (non-archive file, or a file inside the archive */
      else if (         db_state->size > 0
                && (    db_state->min_sizes[db_state->list_index] > (int64_t) db_state->size
                     || db_state->max_sizes[db_state->list_index] < (int64_t) db_state->size))
      {
#ifdef DEBUG
         RARCH_DBG("[Scanner] Skipping DB, file size %ld not in range.\n", db_state->size);
#endif
         return database_info_list_iterate_next(db_state);
      }
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

      /* When scanning an archive, "first" file crc32 is also checked. */
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
   {
      free(db_state->info);
      db_state->info = NULL;
   }

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
      bool path_contains_compressed_file,
      bool size_hint_allowed)
{
#ifdef DEBUG
   RARCH_DBG("[Scanner] Serial check, list_idx %d/%d, entry_idx %d.\n",
         db_state->list_index, db_state->list->size, db_state->entry_index);
#endif

   if (
         !db_state->list ||
         (unsigned)db_state->list_index == (unsigned)db_state->list->size ||
         ( _db->flags & DB_HANDLE_FLAG_USE_FIRST_MATCH_ONLY &&
           db_state->list_index > 0 &&
           db_state->flags[0] & DB_STATE_FLAG_MATCHED)
      )
      return database_info_list_iterate_end_no_match(db, db_state, name,
            path_contains_compressed_file);

   /* If size boundaries are not filled for this DB, run the queries */
   if (db_state->min_sizes[db_state->list_index] == 0)
      task_database_fill_db_min_max(db_state);

   if (db_state->min_sizes[db_state->list_index] > 0)
   {
      if (!(db_state->flags[db_state->list_index] & DB_STATE_FLAG_HAS_SERIAL))
      {
#ifdef DEBUG
         RARCH_DBG("[Scanner] Skipping DB, no serials here.\n");
#endif
         return database_info_list_iterate_next(db_state);
      }

      /* Size check is conditional - it is unreliable in case of multitrack formats *
       * as serial is always in the first track, which may not be the actual game data.
       * Same for those compressed image formats that are not supported by VFS. */

      /* Examining zip file main entry (archive size filled, but no indication of compressed file) */
      if ( size_hint_allowed && !path_contains_compressed_file && db_state->archive_size > 0)
      {
         if (       ( db_state->min_sizes[db_state->list_index] > (int64_t) db_state->archive_size
                   && db_state->min_sizes[db_state->list_index] > (int64_t) db_state->size )
              || (    db_state->max_sizes[db_state->list_index] < (int64_t) db_state->archive_size
                   && db_state->max_sizes[db_state->list_index] < (int64_t) db_state->size ))
         {
#ifdef DEBUG
            RARCH_DBG("[Scanner] Skipping DB, neither archive nor uncompressed size %ld/%ld is in range.\n",
                  db_state->archive_size, db_state->size);
#endif
            return database_info_list_iterate_next(db_state);
         }
      }
      /* Any other case (non-archive file, or a file inside the archive */
      else if ( size_hint_allowed && db_state->size > 0
                && (    db_state->min_sizes[db_state->list_index] > (int64_t) db_state->size
                     || db_state->max_sizes[db_state->list_index] < (int64_t) db_state->size))
      {
#ifdef DEBUG
         RARCH_DBG("[Scanner] Skipping DB, file size %ld not in range.\n", db_state->size);
#endif
         return database_info_list_iterate_next(db_state);
      }
   }

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
#ifdef DEBUG
      RARCH_DBG("[Scanner] Serial orig / decoded: \"%s\" / \"%s\".\n", db_state->serial, serial_buf);
#endif
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
                  intfstream_file_get_crc_and_size(name, 0, INT64_MAX, &db_state->crc, &db_state->size);
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
   db_state->info = NULL;
   return 0;
}

static int task_database_iterate(
      db_handle_t *_db,
      const char *name,
      database_state_handle_t *db_state,
      database_info_handle_t *db,
      bool path_contains_compressed_file)
{
#ifdef DEBUG
   RARCH_DBG("[Scanner] Type %d, \"%s\" against \"%s\".\n", db->type, name, database_info_get_current_name(db_state));
   RARCH_DBG("[Scanner] Size: min %ld actual %ld max %ld.\n", db_state->min_sizes[db_state->list_index], db_state->size, db_state->max_sizes[db_state->list_index]);
#endif
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
               path_contains_compressed_file, false);
      case DATABASE_TYPE_SERIAL_LOOKUP_SIZEHINT:
         return task_database_iterate_serial_lookup(_db, db_state, db, name,
               path_contains_compressed_file, true);
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

/* Batch update playlists from accumulated scan results */
static void scan_results_batch_update_playlists(scan_results_t *sr, db_handle_t *db)
{
   size_t i;
   const char *current_playlist = NULL;
   playlist_t *playlist = NULL;
   unsigned added_count = 0;
   size_t str_len = PATH_MAX_LENGTH * sizeof(char);
   char *db_playlist_path = (char*)malloc(str_len);

   if (!db_playlist_path)
   {
      RARCH_ERR("[Scanner] Failed to allocate memory for batch playlist update.\n");
      return;
   }

   RARCH_LOG("[Scanner] Batch updating playlists with %u results...\n",
            (unsigned)sr->count);

   /* Process results, grouping by playlist */
   for (i = 0; i < sr->count; i++)
   {
      scan_result_t *result = &sr->results[i];
      char db_name_noext[PATH_MAX_LENGTH];

      strlcpy(db_name_noext, result->db_name, sizeof(db_name_noext));
      path_remove_extension(db_name_noext);

      /* Check if we need to switch to a different playlist */
      if (!current_playlist || !string_is_equal(current_playlist, result->db_name))
      {
         /* Write and close previous playlist if any */
         if (playlist)
         {
            RARCH_LOG("[Scanner] Added %u entries to \"%s\".\n", added_count, current_playlist);
            playlist_write_file(playlist);
            playlist_free(playlist);
            playlist = NULL;
            added_count = 0;
         }

         /* Open new playlist */
         current_playlist = result->db_name;
         db_playlist_path[0] = '\0';

         if (!string_is_empty(db->playlist_directory))
            fill_pathname_join_special(db_playlist_path, db->playlist_directory,
                  result->db_name, str_len);

         playlist_config_set_path(&db->playlist_config, db_playlist_path);
         playlist = playlist_init(&db->playlist_config);

         if (!playlist)
         {
            RARCH_ERR("[Scanner] Failed to open playlist: \"%s\".\n", result->db_name);
            continue;
         }

         RARCH_LOG("[Scanner] Processing playlist: \"%s\".\n", result->db_name);
      }

      /* Add entry to playlist if it doesn't already exist */
      if (playlist && !playlist_entry_exists(playlist, result->entry_path))
      {
         struct playlist_entry entry;

         /* Build entry */
         entry.path              = result->entry_path;
         entry.label             = result->entry_label;
         entry.core_path         = (char*)"DETECT";
         entry.core_name         = (char*)"DETECT";
         entry.db_name           = result->db_name;
         entry.crc32             = result->db_crc;
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
         added_count++;

         RARCH_LOG("[Scanner] Add \"%s / %s\".\n", db_name_noext, result->entry_label);

         if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_DATABASE_SCAN, NULL))
            task_database_scan_console_output(result->entry_label,
                  db_name_noext, true);
      }
      /* Entry already exists - output duplicate indicator for CLI scans */
      else if (playlist && retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_DATABASE_SCAN, NULL))
         task_database_scan_console_output(result->entry_label,
               db_name_noext, false);
   }

   /* Write and close final playlist */
   if (playlist)
   {
      RARCH_LOG("[Scanner] Added %u entries to \"%s\".\n", added_count, current_playlist);
      /* Ensure playlist is alphabetically sorted (matches manual scan behavior) */
      playlist_set_sort_mode(playlist, PLAYLIST_SORT_MODE_DEFAULT);
      playlist_qsort(playlist);
      playlist_write_file(playlist);
      playlist_free(playlist);
   }

   free(db_playlist_path);
   RARCH_LOG("[Scanner] Batch playlist update complete.\n");
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

            /* Initialize scan results accumulation */
            if (!scan_results_init(&db->scan_results, 1024))
            {
               RARCH_ERR("[Scanner] Failed to initialize scan results.\n");
               goto task_finished;
            }

            /* Initialize M3U list tracking */
            dbstate->m3u_list = string_list_new();
            if (!dbstate->m3u_list)
            {
               RARCH_ERR("[Scanner] Failed to initialize M3U list.\n");
               goto task_finished;
            }

            RARCH_LOG("[Scanner] %s\"%s\"...\n", msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_START), db->fullpath);
            if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_DATABASE_SCAN, NULL))
               printf("%s\"%s\"...\n", msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_START), db->fullpath);
         }
         dbinfo->status = DATABASE_STATUS_ITERATE_START;
         break;
      case DATABASE_STATUS_ITERATE_START:
         name                 = database_info_get_current_element_name(dbinfo);

         /* Check if this is an M3U file and add to list for post-processing */
         if (m3u_file_is_m3u(name))
         {
            union string_list_elem_attr attr;
            attr.i = 0;
            if (dbstate->m3u_list)
               string_list_append(dbstate->m3u_list, name, attr);
         }

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
            /* TODO - remove this shortcut when serial scan inside zip is solved */
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
            if (dbstate->list->size == 0)
            {
               msg = msg_hash_to_str(MSG_SCANNING_NO_DATABASE);
               task_set_error(task, strdup(msg));
            }
            else if (db->flags & DB_HANDLE_FLAG_IS_DIRECTORY)
               msg = msg_hash_to_str(MSG_SCANNING_OF_DIRECTORY_FINISHED);
            else
               msg = msg_hash_to_str(MSG_SCANNING_OF_FILE_FINISHED);
#ifdef RARCH_INTERNAL
            task_free_title(task);
            task_set_title(task, strdup(msg));
            task_set_progress(task, 100);
            ui_companion_driver_notify_refresh();
            RARCH_LOG("[Scanner] %s\n", msg);
            if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_DATABASE_SCAN, NULL))
               printf("%s\n", msg);
#else
            fprintf(stderr, "msg: %s\n", msg);
#endif
            /* Process M3U files after main scan completes */
            if (dbstate->m3u_list && dbstate->m3u_list->size > 0)
            {
               size_t m;
               RARCH_LOG("[Scanner] Processing %u M3U files...\n",
                     (unsigned)dbstate->m3u_list->size);

               /* Scan M3U files and collapse disc entries */
               for (m = 0; m < dbstate->m3u_list->size; m++)
               {
                  const char *m3u_path = dbstate->m3u_list->elems[m].data;
                  if (m3u_path)
                     task_database_iterate_m3u(db, dbstate, m3u_path);
               }
            }

            /* Batch update all playlists with accumulated results */
            if (db->scan_results.count > 0)
               scan_results_batch_update_playlists(&db->scan_results, db);

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

   /* Free accumulated scan results */
   scan_results_free(&db->scan_results);

   if (dbstate)
   {
      if (dbstate->list)
         dir_list_free(dbstate->list);
      if (dbstate->m3u_list)
         string_list_free(dbstate->m3u_list);
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

#endif

/* Frees task handle + all constituent objects */
static void free_manual_content_scan_handle(manual_scan_handle_t *manual_scan)
{
   if (!manual_scan)
      return;

   if (manual_scan->task_config)
   {
      free(manual_scan->task_config);
      manual_scan->task_config = NULL;
   }

   if (manual_scan->playlist)
   {
      playlist_free(manual_scan->playlist);
      manual_scan->playlist = NULL;
   }

   if (manual_scan->file_exts_list)
   {
      string_list_free(manual_scan->file_exts_list);
      manual_scan->file_exts_list = NULL;
   }

   if (manual_scan->content_list)
   {
      string_list_free(manual_scan->content_list);
      manual_scan->content_list = NULL;
   }

   if (manual_scan->m3u_list)
   {
      string_list_free(manual_scan->m3u_list);
      manual_scan->m3u_list = NULL;
   }

   if (manual_scan->dat_file)
   {
      logiqx_dat_free(manual_scan->dat_file);
      manual_scan->dat_file = NULL;
   }

   free(manual_scan);
   manual_scan = NULL;
}

static void cb_task_manual_content_scan(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   manual_scan_handle_t *manual_scan = NULL;
   playlist_t *cached_playlist       = playlist_get_cached();
#if defined(RARCH_INTERNAL) && defined(HAVE_MENU)
   struct menu_state *menu_st        = menu_state_get_ptr();
   if (!task)
      goto end;
#else
   if (!task)
      return;
#endif

   if (!(manual_scan = (manual_scan_handle_t*)task->state))
   {
#if defined(RARCH_INTERNAL) && defined(HAVE_MENU)
      goto end;
#else
      return;
#endif
   }

   /* If the manual content scan task has modified the
    * currently cached playlist, then it must be re-cached
    * (otherwise changes will be lost if the currently
    * cached playlist is saved to disk for any reason...) */
   if (cached_playlist)
   {
      if (string_is_equal(
            manual_scan->playlist_config.path,
            playlist_get_conf_path(cached_playlist)))
      {
         playlist_config_t playlist_config;

         /* Copy configuration of cached playlist
          * (could use manual_scan->playlist_config,
          * but doing it this way guarantees that
          * the cached playlist is preserved in
          * its original state) */
         if (playlist_config_copy(
               playlist_get_config(cached_playlist),
               &playlist_config))
         {
            playlist_free_cached();
            playlist_init_cached(&playlist_config);
         }
      }
   }

#if defined(RARCH_INTERNAL) && defined(HAVE_MENU)
end:
   /* When creating playlists, the playlist tabs of
    * any active menu driver must be refreshed */
   if (menu_st->driver_ctx->environ_cb)
      menu_st->driver_ctx->environ_cb(MENU_ENVIRON_RESET_HORIZONTAL_LIST,
            NULL, menu_st->userdata);
#endif
}

static void task_manual_content_scan_free(retro_task_t *task)
{
   manual_scan_handle_t *manual_scan = NULL;

   if (!task)
      return;

   manual_scan = (manual_scan_handle_t*)task->state;

   free_manual_content_scan_handle(manual_scan);
}

static void task_manual_content_scan_handler(retro_task_t *task)
{
   uint8_t flg;
   manual_scan_handle_t *manual_scan = NULL;

   if (!task)
      goto task_finished;

   if (!(manual_scan = (manual_scan_handle_t*)task->state))
      goto task_finished;

   flg = task_get_flags(task);

   if ((flg & RETRO_TASK_FLG_CANCELLED) > 0)
      goto task_finished;

   switch (manual_scan->status)
   {
      case MANUAL_SCAN_BEGIN:
         {
            /* Get allowed file extensions list */
            if (!string_is_empty(manual_scan->task_config->file_exts))
               manual_scan->file_exts_list = string_split(
                     manual_scan->task_config->file_exts, "|");

            /* Get content list */
            if (!(manual_scan->content_list
                     = manual_content_scan_get_content_list(
                        manual_scan->task_config)))
            {
               const char *_msg = msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT);
               runloop_msg_queue_push(_msg, strlen(_msg), 1, 100, true, NULL,\
                     MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               goto task_finished;
            }

            manual_scan->content_list_size = manual_scan->content_list->size;

            /* Load DAT file, if required */
            if (!string_is_empty(manual_scan->task_config->dat_file_path))
            {
               if (!(manual_scan->dat_file =
                     logiqx_dat_init(
                        manual_scan->task_config->dat_file_path)))
               {
                  const char *_msg = msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_DAT_FILE_LOAD_ERROR);
                  runloop_msg_queue_push(_msg, strlen(_msg), 1, 100, true, NULL,
                        MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
                  goto task_finished;
               }
            }

            /* Open playlist */
            if (!(manual_scan->playlist =
                     playlist_init(&manual_scan->playlist_config)))
               goto task_finished;

            /* Reset playlist, if required */
            if (manual_scan->task_config->overwrite_playlist)
               playlist_clear(manual_scan->playlist);

            /* Get initial playlist size */
            manual_scan->playlist_size =
               playlist_size(manual_scan->playlist);

            /* Set default core, if required */
            if (manual_scan->task_config->core_set)
            {
               playlist_set_default_core_path(manual_scan->playlist,
                     manual_scan->task_config->core_path);
               playlist_set_default_core_name(manual_scan->playlist,
                     manual_scan->task_config->core_name);
            }

            /* Record remaining scan parameters to enable
             * subsequent 'refresh playlist' operations */
            playlist_set_scan_content_dir(manual_scan->playlist,
                  manual_scan->task_config->content_dir);
            playlist_set_scan_file_exts(manual_scan->playlist,
                  manual_scan->task_config->file_exts_custom_set ?
                        manual_scan->task_config->file_exts : NULL);
            playlist_set_scan_dat_file_path(manual_scan->playlist,
                  manual_scan->task_config->dat_file_path);
            playlist_set_scan_search_recursively(manual_scan->playlist,
                  manual_scan->task_config->search_recursively);
            playlist_set_scan_search_archives(manual_scan->playlist,
                  manual_scan->task_config->search_archives);
            playlist_set_scan_filter_dat_content(manual_scan->playlist,
                  manual_scan->task_config->filter_dat_content);
            playlist_set_scan_overwrite_playlist(manual_scan->playlist,
                  manual_scan->task_config->overwrite_playlist);

            /* All good - can start iterating
             * > If playlist has content and 'validate
             *   entries' is enabled, go to clean-up phase
             * > Otherwise go straight to content scan phase */
            if (manual_scan->task_config->validate_entries &&
                (manual_scan->playlist_size > 0))
               manual_scan->status = MANUAL_SCAN_ITERATE_CLEAN;
            else
               manual_scan->status = MANUAL_SCAN_ITERATE_CONTENT;
         }
         break;
      case MANUAL_SCAN_ITERATE_CLEAN:
         {
            const struct playlist_entry *entry = NULL;
            bool delete_entry                  = false;

            /* Get current entry */
            playlist_get_index(manual_scan->playlist,
                  manual_scan->playlist_index, &entry);

            if (entry)
            {
               size_t _len;
               const char *entry_file     = NULL;
               const char *entry_file_ext = NULL;
               char task_title[128];

               /* Update progress display */
               task_free_title(task);

               _len = strlcpy(task_title,
                     msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_PLAYLIST_CLEANUP),
                     sizeof(task_title));

               if (   !string_is_empty(entry->path)
                   && (entry_file = path_basename(entry->path)))
                  strlcpy(task_title       + _len,
                        entry_file,
                        sizeof(task_title) - _len);

               task_set_title(task, strdup(task_title));
               task_set_progress(task, (manual_scan->playlist_index * 100) /
                     manual_scan->playlist_size);

               /* Check whether playlist content exists on
                * the filesystem */
               if (!playlist_content_path_is_valid(entry->path))
                  delete_entry = true;
               /* If file exists, check whether it has a
                * permitted file extension */
               else if (    manual_scan->file_exts_list
                        && (entry_file_ext = path_get_extension(entry->path))
                        && !string_list_find_elem_prefix(
                              manual_scan->file_exts_list,
                              ".", entry_file_ext))
                  delete_entry = true;

               if (delete_entry)
               {
                  /* Invalid content - delete entry */
                  playlist_delete_index(manual_scan->playlist,
                        manual_scan->playlist_index);

                  /* Update playlist_size */
                  manual_scan->playlist_size = playlist_size(manual_scan->playlist);
               }
            }

            /* Increment entry index *if* current entry still
             * exists (i.e. if entry was deleted, current index
             * will already point to the *next* entry) */
            if (!delete_entry)
               manual_scan->playlist_index++;

            if (manual_scan->playlist_index >=
                  manual_scan->playlist_size)
               manual_scan->status = MANUAL_SCAN_ITERATE_CONTENT;
         }
         break;
      case MANUAL_SCAN_ITERATE_CONTENT:
         {
            const char *content_path = manual_scan->content_list->elems[
                  manual_scan->content_list_index].data;
            int content_type         = manual_scan->content_list->elems[
                  manual_scan->content_list_index].attr.i;

            if (!string_is_empty(content_path))
            {
               size_t _len;
               char task_title[128];
               const char *content_file = path_basename(content_path);

               /* Update progress display */
               task_free_title(task);

               _len = strlcpy(task_title,
                     msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_IN_PROGRESS),
                     sizeof(task_title));

               if (!string_is_empty(content_file))
                  strlcpy(task_title       + _len,
                        content_file,
                        sizeof(task_title) - _len);

               task_set_title(task, strdup(task_title));
               task_set_progress(task,
                     (manual_scan->content_list_index * 100) /
                     manual_scan->content_list_size);

               /* Add content to playlist */
               manual_content_scan_add_content_to_playlist(
                     manual_scan->task_config, manual_scan->playlist,
                     content_path, content_type, manual_scan->dat_file);

               /* If this is an M3U file, add it to the
                * M3U list for later processing */
               if (m3u_file_is_m3u(content_path))
               {
                  union string_list_elem_attr attr;
                  attr.i = 0;
                  /* Note: If string_list_append() fails, there is
                   * really nothing we can do. The M3U file will
                   * just be ignored... */
                  string_list_append(
                        manual_scan->m3u_list, content_path, attr);
               }
            }

            /* Increment content index */
            manual_scan->content_list_index++;
            if (manual_scan->content_list_index >=
                  manual_scan->content_list_size)
            {
               /* Check whether we have any M3U files
                * to process */
               if (manual_scan->m3u_list->size > 0)
                  manual_scan->status = MANUAL_SCAN_ITERATE_M3U;
               else
                  manual_scan->status = MANUAL_SCAN_END;
            }
         }
         break;
      case MANUAL_SCAN_ITERATE_M3U:
         {
            const char *m3u_path = manual_scan->m3u_list->elems[
                  manual_scan->m3u_index].data;

            if (!string_is_empty(m3u_path))
            {
               size_t _len;
               char task_title[128];
               const char *m3u_name = path_basename_nocompression(m3u_path);
               m3u_file_t *m3u_file = NULL;

               /* Update progress display */
               task_free_title(task);

               _len = strlcpy(task_title,
                     msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_M3U_CLEANUP),
                     sizeof(task_title));

               if (!string_is_empty(m3u_name))
                  strlcpy(task_title       + _len,
                        m3u_name,
                        sizeof(task_title) - _len);

               task_set_title(task, strdup(task_title));
               task_set_progress(task, (manual_scan->m3u_index * 100) /
                     manual_scan->m3u_list->size);

               /* Load M3U file */
               if ((m3u_file = m3u_file_init(m3u_path)))
               {
                  size_t i;

                  /* Loop over M3U entries */
                  for (i = 0; i < m3u_file_get_size(m3u_file); i++)
                  {
                     m3u_file_entry_t *m3u_entry = NULL;

                     /* Delete any playlist items matching the
                      * content path of the M3U entry */
                     if (m3u_file_get_entry(m3u_file, i, &m3u_entry))
                        playlist_delete_by_path(
                              manual_scan->playlist, m3u_entry->full_path);
                  }

                  m3u_file_free(m3u_file);
               }
            }

            /* Increment M3U file index */
            manual_scan->m3u_index++;
            if (manual_scan->m3u_index >= manual_scan->m3u_list->size)
               manual_scan->status = MANUAL_SCAN_END;
         }
         break;
      case MANUAL_SCAN_END:
         {
            size_t _len;
            char task_title[128];

            /* Ensure playlist is alphabetically sorted
             * > Override user settings here */
            playlist_set_sort_mode(manual_scan->playlist, PLAYLIST_SORT_MODE_DEFAULT);
            playlist_qsort(manual_scan->playlist);

            /* Save playlist changes to disk */
            playlist_write_file(manual_scan->playlist);

            /* Update progress display */
            task_free_title(task);

            _len = strlcpy(
                  task_title, msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_END),
                  sizeof(task_title));
            strlcpy(task_title       + _len,
                  manual_scan->task_config->system_name,
                  sizeof(task_title) - _len);

            task_set_title(task, strdup(task_title));
         }
         /* fall-through */
      default:
         task_set_progress(task, 100);
         goto task_finished;
   }

   return;

task_finished:
   if (task)
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static bool task_manual_content_scan_finder(retro_task_t *task, void *user_data)
{
   manual_scan_handle_t *manual_scan = NULL;

   if (!task || !user_data)
      return false;
   if (task->handler != task_manual_content_scan_handler)
      return false;
   if (!(manual_scan = (manual_scan_handle_t*)task->state))
      return false;
   return string_is_equal(
         (const char*)user_data, manual_scan->playlist_config.path);
}

bool task_push_manual_content_scan(
      const playlist_config_t *playlist_config,
      const char *playlist_directory)
{
   size_t _len;
   task_finder_data_t find_data;
   char task_title[128];
   retro_task_t *task                = NULL;
   manual_scan_handle_t *manual_scan = NULL;

   /* Sanity check */
   if (  !playlist_config
       || string_is_empty(playlist_directory))
      return false;

   if (!(manual_scan = (manual_scan_handle_t*)
         calloc(1, sizeof(manual_scan_handle_t))))
      return false;

   /* Configure handle */
   manual_scan->task_config         = NULL;
   manual_scan->playlist            = NULL;
   manual_scan->file_exts_list      = NULL;
   manual_scan->content_list        = NULL;
   manual_scan->dat_file            = NULL;
   manual_scan->playlist_size       = 0;
   manual_scan->playlist_index      = 0;
   manual_scan->content_list_size   = 0;
   manual_scan->content_list_index  = 0;
   manual_scan->status              = MANUAL_SCAN_BEGIN;
   manual_scan->m3u_index           = 0;
   manual_scan->m3u_list            = string_list_new();

   if (!manual_scan->m3u_list)
      goto error;

   /* > Get current manual content scan configuration */
   if (!(manual_scan->task_config = (manual_content_scan_task_config_t*)
         calloc(1, sizeof(manual_content_scan_task_config_t))))
      goto error;

   if (!manual_content_scan_get_task_config(
         manual_scan->task_config, playlist_directory))
   {
      const char *_msg = msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_INVALID_CONFIG);
      runloop_msg_queue_push(_msg, strlen(_msg), 1, 100, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      goto error;
   }

   /* > Cache playlist configuration */
   if (!playlist_config_copy(playlist_config,
         &manual_scan->playlist_config))
      goto error;

   playlist_config_set_path(
         &manual_scan->playlist_config,
         manual_scan->task_config->playlist_file);

   /* Concurrent scanning of content to the same
    * playlist is not allowed */
   find_data.func     = task_manual_content_scan_finder;
   find_data.userdata = (void*)manual_scan->playlist_config.path;

   if (task_queue_find(&find_data))
      goto error;

   /* Create task */
   if (!(task = task_init()))
      goto error;

   /* > Get task title */
   _len = strlcpy(
         task_title, msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_START),
         sizeof(task_title));
   strlcpy(task_title       + _len,
         manual_scan->task_config->system_name,
         sizeof(task_title) - _len);

   /* > Configure task */
   task->handler                 = task_manual_content_scan_handler;
   task->state                   = manual_scan;
   task->title                   = strdup(task_title);
   task->progress                = 0;
   task->callback                = cb_task_manual_content_scan;
   task->cleanup                 = task_manual_content_scan_free;
   task->flags                  |= RETRO_TASK_FLG_ALTERNATIVE_LOOK;

   /* > Push task */
   task_queue_push(task);

   return true;

error:
   /* Clean up handle */
   free_manual_content_scan_handle(manual_scan);
   manual_scan = NULL;

   return false;
}

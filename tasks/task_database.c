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
#include <string/stdstring.h>
#include <lists/dir_list.h>
#include <memory/mem_stats.h>
#include <lists/string_list.h>
#include <file/file_path.h>
#include <formats/logiqx_dat.h>
#include <formats/rm3u.h>
#include <formats/rm3u_stream.h>
#include <encodings/crc32.h>
#include <streams/interface_stream.h>
#include <streams/file_stream.h>
#include "tasks_internal.h"

#include "../core_info.h"
#include "../database_info.h"
#include "../manual_content_scan.h"

#include "../file_path_special.h"
#include "../msg_hash.h"
#include "../playlist.h"
#include "../configuration.h"
#include "../ui/ui_companion_driver.h"
#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#endif
#include "../runloop.h"
#include "../retroarch.h"
#include "../verbosity.h"
#include "task_database_cue.h"

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
   sr->count    = 0;
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
      sr->results  = new_results;
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

   result               = &sr->results[sr->count];
   result->entry_path   = strdup(entry_path);
   result->entry_label  = strdup(entry_label);
   result->db_crc       = strdup(db_crc);
   result->db_name      = strdup(db_name);

#ifdef DEBUG
   RARCH_DBG("[Scanner] Adding scan result %d: %s %s\n",sr->count, entry_path, db_name);
#endif

   result->archive_name = archive_name ? strdup(archive_name) : NULL;

   if (   !result->entry_path || !result->entry_label
       || !result->db_crc     || !result->db_name)
   {
      /* Allocation failed, cleanup */
      if (result->entry_path)
         free(result->entry_path);
      if (result->entry_label)
         free(result->entry_label);
      if (result->db_crc)
         free(result->db_crc);
      if (result->db_name)
         free(result->db_name);
      if (result->archive_name)
         free(result->archive_name);
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
      if (sr->results[i].entry_path)
         free(sr->results[i].entry_path);
      if (sr->results[i].entry_label)
         free(sr->results[i].entry_label);
      if (sr->results[i].db_crc)
         free(sr->results[i].db_crc);
      if (sr->results[i].db_name)
         free(sr->results[i].db_name);
      if (sr->results[i].archive_name)
         free(sr->results[i].archive_name);
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
   DB_STATE_FLAG_MATCHED                  = (1 << 3),
   /* Set once the size range for a database has been queried,
    * whatever the answer was.  The probe used to key off
    * "min_sizes[i] == 0", which is also what an unqueried slot holds
    * and what a database whose smallest record is zero-sized
    * legitimately produces - so such a database was re-queried for
    * every content file, at two full walks a time. */
   DB_STATE_FLAG_SIZE_CHECKED             = (1 << 4)
};

/* Ceiling on the crc and serial indexes a single scan may hold.
 *
 * Taken as a share of what is actually free rather than from a
 * platform list: mem_stats_free() has implementations for the 3DS and
 * the GameCube/Wii among others, which are the platforms where this
 * matters, and task_audio_mixer_threshold() already sizes a
 * task-lifetime allocation the same way.
 *
 * An eighth is deliberately more conservative than the quarter the
 * mixer uses, because these indexes are held for the whole scan
 * rather than consulted once.  The cap keeps a large-memory host from
 * hoarding: the biggest database shipped today indexes to 237 KB
 * across roughly 30000 records, so 32 MB covers far more databases
 * than exist.  Below the
 * floor there is no point starting, and the query path is only
 * slower, never wrong. */
#define DB_STATE_INDEX_BUDGET_SHARE  8
#define DB_STATE_INDEX_BUDGET_CAP    (32 * 1024 * 1024)
#define DB_STATE_INDEX_BUDGET_FLOOR  (256 * 1024)

/* Used when mem_stats_free() has no implementation for the platform
 * and returns 0.  Small enough to be harmless where memory is tight,
 * since the only cost of running out is falling back to querying. */
#ifndef DB_STATE_INDEX_BUDGET_UNKNOWN
#define DB_STATE_INDEX_BUDGET_UNKNOWN (2 * 1024 * 1024)
#endif

static size_t task_database_index_budget(void)
{
   uint64_t free_mem = mem_stats_free();
   uint64_t share;

   if (!free_mem)
      return (size_t)DB_STATE_INDEX_BUDGET_UNKNOWN;

   share = free_mem / DB_STATE_INDEX_BUDGET_SHARE;

   if (share > (uint64_t)DB_STATE_INDEX_BUDGET_CAP)
      share = (uint64_t)DB_STATE_INDEX_BUDGET_CAP;
   if (share < (uint64_t)DB_STATE_INDEX_BUDGET_FLOOR)
      return 0;

   return (size_t)share;
}

typedef struct database_state_handle
{
   database_info_list_t *info;
   struct string_list *list;
   uint8_t *buf;
   size_t list_index;
   size_t entry_index;
   uint32_t crc;
   uint32_t archive_crc;
   uint64_t size;
   uint64_t archive_size;
   char archive_name[512]; /* TODO/FIXME - check size */
   char serial[4096];      /* TODO/FIXME - check size */
   /* One entry per database in 'list'.  These used to be
    * [MAX_DATABASE_COUNT] arrays indexed by list_index, which is
    * bounded only by list->size - the number of .rdb files in the
    * database directory.  Nothing clamped it, so a database
    * directory with more than MAX_DATABASE_COUNT entries wrote past
    * all three arrays, and the shuffle in
    * database_info_list_iterate_found_match() memmove()d past them
    * as well.  The shipped set is already over half that limit and
    * grows every release.  Allocated by
    * task_database_state_alloc_arrays(). */
   int64_t *min_sizes;
   int64_t *max_sizes;
   uint8_t *flags;
   /* One crc index per database, built the first time that database
    * is probed and kept for the rest of the scan.  Without it every
    * (content file, database) pair is a full walk of the database. */
   database_info_crc_index_t **crc_index;
   /* Likewise for the serial lookup disc content uses. */
   database_info_serial_index_t **serial_index;
   /* Bytes of index the scan may still allocate.  Indexes are held
    * for the whole scan, so without a ceiling a large database set
    * costs tens of megabytes.  See task_database_index_budget(). */
   size_t index_budget;
} database_state_handle_t;

enum db_flags_enum
{
   DB_HANDLE_FLAG_IS_DIRECTORY            = (1 << 0),
   DB_HANDLE_FLAG_SCAN_STARTED            = (1 << 1),
   DB_HANDLE_FLAG_SCAN_WITHOUT_CORE_MATCH = (1 << 2),
   DB_HANDLE_FLAG_SHOW_HIDDEN_FILES       = (1 << 3),
   DB_HANDLE_FLAG_USE_FIRST_MATCH_ONLY    = (1 << 4),
   DB_HANDLE_FLAG_DO_MENU_REFRESH         = (1 << 5)
};

enum manual_scan_status
{
   /* The BEGIN family: setup, the recursive directory walk, the DAT
    * load and the playlist setup as separate sub-states, each
    * respecting the shared per-frame I/O window (task_nbio_slice_*,
    * the same window the file-transfer spine uses).  Completed
    * sub-states collapse into one invocation while the window
    * lasts, so small libraries keep single-tick latency.  The
    * task's public identity (handler, state) is one task across all
    * sub-states, so task_queue_find()-based duplicate-scan
    * suppression is unaffected. */
   MANUAL_SCAN_BEGIN = 0,
   MANUAL_SCAN_BEGIN_DIR_LIST,
   MANUAL_SCAN_BEGIN_DAT_LOAD,
   MANUAL_SCAN_BEGIN_PLAYLIST,
   MANUAL_SCAN_ITERATE_CLEAN,
   DATABASE_SCAN_ITERATE_START,
   DATABASE_SCAN_ITERATE_CONTENT,
   DATABASE_SCAN_ITERATE_NEXT,
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
   /* Resumable walk filling content_list across gathers; non-NULL
    * only between MANUAL_SCAN_BEGIN and the completion of
    * MANUAL_SCAN_BEGIN_DIR_LIST. */
   dir_list_iter_t *content_iter;
   /* Chunked DAT read in flight (MANUAL_SCAN_BEGIN_DAT_LOAD): the
    * file handle, the destination buffer (dat_size + 1 bytes) and
    * the fill position.  dat_buf ownership passes to
    * logiqx_dat_parse_begin_owned() once the read completes, and
    * dat_parse holds the budgeted parse + index build until its
    * verdict. */
   RFILE *dat_stream;
   char *dat_buf;
   logiqx_dat_parse_t *dat_parse;
   int64_t dat_size;
   int64_t dat_read;
   /* Resumable batch playlist flush (MANUAL_SCAN_END): position in
    * scan_results, the playlist currently open for the group being
    * written, its group key and running count, and the path scratch
    * buffer, kept on the handle so the flush can yield on the
    * shared window and resume next gather.  flush_group points into
    * either the task config or the owned results array, both of
    * which outlive the flush. */
   size_t flush_pos;
   playlist_t *flush_playlist;
   const char *flush_group;
   char *flush_path_buf;
   unsigned flush_added;
   bool flush_started;
   playlist_dedup_t *flush_dedup;
   logiqx_dat_t *dat_file;
   struct string_list *m3u_list;
   playlist_config_t playlist_config; /* size_t alignment */
   size_t playlist_size;
   size_t playlist_index;
   size_t content_list_index;
   size_t m3u_index;
   enum manual_scan_status status; /* merged, the other status is in dbinfo */
   scan_results_t scan_results;
   char *playlist_directory;
#ifdef HAVE_LIBRETRODB
   char *content_database_path;
   database_info_handle_t *handle;
   database_state_handle_t state;
   uint8_t flags;
#endif
   /* The caller's completion callback, run after the task's own.
    * task_push_dbscan takes one and used to drop it, so a caller that
    * wanted to know when a scan finished never found out - see
    * cb_task_manual_content_scan. */
   retro_task_callback_t user_cb;
} manual_scan_handle_t;

enum scan_verdict
{
   SCAN_VERDICT_CONTINUE = 0,
   SCAN_VERDICT_MATCHED_DB,
   SCAN_VERDICT_ARCHIVE_CONTENTS_ADDED,
   SCAN_VERDICT_NO_DB_MATCH,
   SCAN_VERDICT_ERROR
};

static void increase_content_list_index(manual_scan_handle_t *manual_scan)
{
   /* Skip any entries pruned by cue/gdi filters */
   do
   {
      manual_scan->content_list_index++;
   }
   while (manual_scan->content_list_index < manual_scan->content_list->size && 
          !manual_scan->content_list->elems[manual_scan->content_list_index].data);
}

#ifdef HAVE_LIBRETRODB

static const char *database_info_get_current_name(
      database_state_handle_t *handle)
{
   if (!handle || !handle->list)
      return NULL;
   return handle->list->elems[handle->list_index].data;
}

static const char *database_info_get_current_element_name(
      struct string_list *handle, size_t ptr)
{
   if (!handle || !handle->elems)
      return NULL;
   /* Don't skip pruned entries, otherwise iteration
    * ends prematurely */
   if (!handle->elems[ptr].data)
      return "";
   return handle->elems[ptr].data;
}
#endif
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
#ifdef HAVE_LIBRETRODB
static enum scan_verdict task_database_iterate_start(retro_task_t *task,
      database_info_handle_t *db,
      const char *name)
{
   char msg[128];
   const char *basename_path = (name && *name)
         ? path_basename_nocompression(name) : "";
   manual_scan_handle_t *manual_scan = NULL;

   msg[0] = '\0';

   if (!task)
      return SCAN_VERDICT_ERROR;

   if (!(manual_scan = (manual_scan_handle_t*)task->state))
      return SCAN_VERDICT_ERROR;

   if (basename_path && *basename_path)
      snprintf(msg, sizeof(msg),
         STRING_REP_USIZE "/" STRING_REP_USIZE ": \"%s\"...\n",
         manual_scan->content_list_index + 1,
         (size_t)manual_scan->content_list->size,
         basename_path);

   if (*msg)
   {
      task_free_title(task);
      task_set_title(task, strdup(msg));
      if (manual_scan->content_list->size != 0)
         task_set_progress(task,
               roundf((float)manual_scan->content_list_index /
                  ((float)manual_scan->content_list->size / 100.0f)));
      RARCH_LOG("[Scanner] %s", msg);
      if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_DATABASE_SCAN, NULL))
         printf("%s", msg);
   }

   db->status = DATABASE_STATUS_ITERATE;

   return SCAN_VERDICT_CONTINUE;
}

static void task_database_cue_prune(struct string_list *list,
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
      /* change in filtering: start from 0 */
      for (i = 0; i < list->size; ++i)
      {
         if (list->elems[i].data
               && string_is_equal(path, list->elems[i].data))
         {
            RARCH_DBG("[Scanner] Pruning file referenced by CUE: \"%s\".\n", path);
            free(list->elems[i].data);
            list->elems[i].data = NULL;
         }
      }
   }

   intfstream_close(fd);
   free(fd);
}
#endif

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
   size_t i;
   char *disc_pos = NULL;
   size_t prefix_len = 0;
   /* Tape and floppy releases usually do not follow the naming
    * convention, so their prefixes skip the leading space - which
    * makes them six characters rather than seven.  The old code
    * skipped a hard-coded seven for all of them, so for "(Tape 1)"
    * the indicator was taken to start at the ')' and came out empty:
    * is_valid_disc_indicator() rejected it and no tape or side
    * indicator was ever stripped.  Carry each prefix's own length. */
   static const struct
   {
      const char *pat;
      size_t      len;
   } patterns[] = {
      { " (Disc ", 7 }, { " (disc ", 7 },
      { " (Disk ", 7 }, { " (disk ", 7 },
      { "(Tape ",  6 }, { "(tape ",  6 },
      { "(Side ",  6 }, { "(side ",  6 }
   };

   for (i = 0; i < ARRAY_SIZE(patterns); i++)
   {
      if ((disc_pos = strstr(title, patterns[i].pat)))
      {
         prefix_len = patterns[i].len;
         break;
      }
   }

   if (disc_pos)
   {
      /* Find the closing parenthesis */
      char *end_pos = strchr(disc_pos, ')');
      if (end_pos)
      {
         /* Extract the indicator text between the prefix and ")" */
         const char *indicator_start = disc_pos + prefix_len;
         size_t indicator_len        = (size_t)(end_pos - indicator_start);

         /* Validate this is actually a disc indicator, not arbitrary text */
         if (   end_pos >= indicator_start
             && is_valid_disc_indicator(indicator_start, indicator_len))
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
      manual_scan_handle_t *_db,
      const char *m3u_path)
{
   size_t i, j;
   bool found_match = false;
   char first_matched_db[NAME_MAX_LENGTH];
   char first_matched_crc[128];
   char collapsed_title[NAME_MAX_LENGTH];
   rm3u_t *m3u = NULL;

   first_matched_db[0] = '\0';
   first_matched_crc[0] = '\0';
   collapsed_title[0] = '\0';

   /* Open M3U file */
   if (!(m3u = rm3u_load_filestream(m3u_path)))
   {
      RARCH_ERR("[Scanner] Failed to open M3U file: \"%s\".\n", m3u_path);
      return;
   }

   /* Scan each referenced file and check if it's in scan_results */
   for (i = 0; i < rm3u_get_size(m3u); i++)
   {
      rm3u_entry_t *entry = NULL;
      const char *ref_path = NULL;

      if (!rm3u_get_entry(m3u, i, &entry))
         continue;

      ref_path = entry->full_path;
      if (!ref_path || !*ref_path)
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

#ifdef DEBUG
            RARCH_DBG("[Scanner] Collapsing m3u entry: %s %s -> %s\n",
                      result->entry_label, result->db_name, collapsed_title);
#endif
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

   rm3u_free(m3u);

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

#ifdef HAVE_LIBRETRODB
static void gdi_prune(struct string_list *list, const char *name)
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
      /* change in filtering */
      for (i = 0; i < list->size; ++i)
      {
         if (list->elems[i].data
               && string_is_equal(path, list->elems[i].data))
         {
            RARCH_DBG("[Scanner] Pruning file referenced by GDI: \"%s\".\n", path);
            free(list->elems[i].data);
            list->elems[i].data = NULL;
         }
      }
   }

   /* task_database_cue_prune() above closes the stream before freeing
    * the handle; this one only freed the handle, leaking the OS file
    * descriptor and the inner stream object for every .gdi in the
    * scan.  A large GDI collection exhausts the descriptor table. */
   intfstream_close(fd);
   free(fd);
}

static enum msg_file_type extension_to_file_type(const char *ext)
{
   char ext_lower[6];
   strlcpy(ext_lower, ext, sizeof(ext_lower));
   string_to_lower(ext_lower);

   /* These were memcmp() against a fixed count, which is specified to
    * read all n bytes.  For an extension shorter than the count - an
    * empty extension being the common case - those bytes are
    * uninitialised stack, which MSan reports and which vectorised
    * memcmp() implementations really do load.  string_is_equal()
    * stops at the terminator. */
   if (
            string_is_equal(ext_lower, "7z")
         || string_is_equal(ext_lower, "zip")
         || string_is_equal(ext_lower, "apk")
         || string_is_equal(ext_lower, "zst")
      )
      return FILE_TYPE_COMPRESSED;
   if (string_is_equal(ext_lower, "cue"))
      return FILE_TYPE_CUE;
   if (string_is_equal(ext_lower, "gdi"))
      return FILE_TYPE_GDI;
   if (string_is_equal(ext_lower, "iso"))
      return FILE_TYPE_ISO;
   if (string_is_equal(ext_lower, "chd"))
      return FILE_TYPE_CHD;
   if (string_is_equal(ext_lower, "wbfs"))
      return FILE_TYPE_WBFS;
   if (string_is_equal(ext_lower, "rvz"))
      return FILE_TYPE_RVZ;
   if (string_is_equal(ext_lower, "wia"))
      return FILE_TYPE_WIA;
   if (string_is_equal(ext_lower, "pbp"))
      return FILE_TYPE_PBP;
   if (string_is_equal(ext_lower, "lutro"))
      return FILE_TYPE_LUTRO;
   return FILE_TYPE_NONE;
}

static int task_database_iterate_playlist(
      manual_scan_handle_t *_db,
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
               0, INT64_MAX, &db_state->archive_crc,
               &db_state->archive_size);
#else
         break;
#endif
      case FILE_TYPE_CUE:
         task_database_cue_prune(_db->content_list, name);
         db_state->serial[0] = '\0';
         if (task_database_cue_get_serial(name, db_state->serial,
             sizeof(db_state->serial),&db_state->size))
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
         gdi_prune(_db->content_list, name);
         db_state->serial[0] = '\0';
         if (task_database_gdi_get_serial(name, db_state->serial, 
             sizeof(db_state->serial),&db_state->size))
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
      case FILE_TYPE_PBP:
         db_state->serial[0] = '\0';
         if (task_database_pbp_get_serial(name, db_state->serial, sizeof(db_state->serial),&db_state->size))
            db->type         = DATABASE_TYPE_SERIAL_LOOKUP;
         else
         {
            db->type         = DATABASE_TYPE_CRC_LOOKUP;
            db_state->serial[0] = '\0';
            RARCH_DBG("[Scanner] PBP file serial not detected, fallback to crc.\n");
            return intfstream_file_get_crc_and_size(name, 0, INT64_MAX, &db_state->crc, &db_state->size);
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
#endif
static bool add_files_from_archive(manual_scan_handle_t *_db,
      const char *path)
{
   bool archive_added = false;
   struct string_list *archive_list =
      file_archive_get_file_list(path, 
         (*_db->task_config->file_exts) ? _db->task_config->file_exts : NULL);

   if (archive_list && archive_list->size > 0)
   {
      unsigned i;
      size_t _len  = strlen(path);

      for (i = 0; i < archive_list->size; i++)
      {
         archive_added = true;
         if (_len + strlen(archive_list->elems[i].data)
                  + 1 < PATH_MAX_LENGTH)
         {
            char new_path[PATH_MAX_LENGTH];
            strlcpy(new_path, path, sizeof(new_path));
            new_path[_len] = '#';
            /* The copy starts at _len + 1, so the space left is
             * sizeof(new_path) - _len - 1.  The bound said - _len,
             * which permits one byte past the end.  The enclosing
             * length test happens to keep that unreachable today;
             * the two must agree regardless. */
            strlcpy(new_path + _len + 1,
                  archive_list->elems[i].data,
                  sizeof(new_path) - _len - 1);
            string_list_append(_db->content_list, new_path,
                  archive_list->elems[i].attr);
         }
         else
            string_list_append(_db->content_list, path,
                  archive_list->elems[i].attr);
      }
      string_list_free(archive_list);
   }
   return archive_added;
}
#ifdef HAVE_LIBRETRODB
static enum scan_verdict database_info_list_iterate_end_no_match(
      manual_scan_handle_t *_db,
      database_info_handle_t *db,
      database_state_handle_t *db_state,
      const char *path,
      bool path_contains_compressed_file)
{
   bool archive_added = false;
   /* Reached end of database list,
    * CRC match probably didn't succeed. */
   if (retroarch_override_setting_is_set(
       RARCH_OVERRIDE_SETTING_DATABASE_SCAN, NULL))
      task_database_scan_console_output(path, NULL, false);

   /* If this was a compressed file and no match in the database
    * list was found then expand the search list to include the
    * archive's contents. */
   if (!path_contains_compressed_file && path_is_compressed_file(path) && _db->task_config->search_archives)
   {
      archive_added=add_files_from_archive(_db, path);
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

   return archive_added ? SCAN_VERDICT_ARCHIVE_CONTENTS_ADDED : SCAN_VERDICT_NO_DB_MATCH;
}

static int database_info_list_iterate_new(
      database_state_handle_t *db_state,
      const char *query)
{
   const char *new_database = database_info_get_current_name(db_state);

   if (db_state->info)
   {
      database_info_list_free(db_state->info);
      free(db_state->info);
   }
   db_state->info = database_info_list_new_filtered(
         new_database, query, DB_EXTRACT_SCAN_FIELDS);
   return 0;
}

static enum scan_verdict database_info_list_iterate_found_match(
      manual_scan_handle_t *_db,
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
   /* db_crc holds one of two things: a serial with "|serial" after it,
    * or a CRC as "%08lX|crc" - thirteen bytes.  It was a PATH_MAX_LENGTH
    * heap allocation for both, per matched entry, which is four
    * kilobytes to hold thirteen.
    *
    * The worst case is still large, because db_state->serial is itself
    * a 4 KiB field, so it cannot simply move to the stack - that is
    * what the note below about limited stack sizes is about.  Take a
    * small buffer for what actually occurs and fall back to the heap
    * only for a serial that does not fit, which no real disc serial
    * comes close to. */
   char  db_crc_buf[128];
   size_t db_crc_len              = (*db_state->serial)
      ? (strlen(db_state->serial) + STRLEN_CONST("|serial") + 1)
      : (STRLEN_CONST("XXXXXXXX|crc") + 1);
   char* db_crc                   = (db_crc_len <= sizeof(db_crc_buf))
      ? db_crc_buf
      : (char*)malloc(db_crc_len);
   char* entry_path_str           = (char*)malloc(str_len);
   char *hash                     = NULL;
   const char         *db_path    =
      database_info_get_current_name(db_state);
   const char         *entry_path =
      database_info_get_current_element_name(_db->content_list, _db->content_list_index);
   database_info_t *db_info_entry = (db_state->info
         && db_state->entry_index < db_state->info->count)
      ? &db_state->info->list[db_state->entry_index]
      : NULL;

   /* NULL-check both mallocs: the 'db_crc[0] = ...' /
    * 'entry_path_str[0] = ...' writes below NULL-deref on OOM.
    * free(NULL) in the teardown at the end of the function is
    * safe, so we can bail early; clean up whichever succeeded
    * and return SCAN_VERDICT_ERROR so the scanner continues
    * with the next content entry rather than silently
    * mismatching.
    *
    * db_path is also checked here: database_info_get_current_name()
    * returns NULL when the database handle/list is missing (and the
    * matched element's data may itself be NULL). A NULL db_path is
    * fed straight into path_basename_nocompression()/fill_pathname()
    * below, where strrchr()/strlcpy() dereference it and crash. With
    * no database name there is no meaningful playlist filename to
    * build, so treat it like the OOM case and skip this entry.
    *
    * db_info_entry likewise: the matched entry used to be taken as
    * &info->list[entry_index] unconditionally, which reads info and
    * indexes list on nothing but the caller's word.  Every caller
    * does test both - each of the four reaches this function from
    * inside an "info && entry_index < info->count" - so this is the
    * invariant being stated where it is relied on rather than a
    * reachable fault.  It is the same shape as the two the callers
    * were given after they were found dereferencing list[0] on an
    * empty result, and the only place left in the file taking that
    * address without a bound. */
   if (!db_crc || !entry_path_str || !db_path || !db_info_entry)
   {
      if (db_crc != db_crc_buf)
         free(db_crc);
      free(entry_path_str);
      return SCAN_VERDICT_ERROR;
   }

   db_crc[0]                      = '\0';
   entry_path_str[0]              = '\0';

   fill_pathname(db_playlist_base_str,
         path_basename_nocompression(db_path), ".lpl", sizeof(db_playlist_base_str));

   if (*db_state->serial)
   {
      size_t _len = strlcpy(db_crc, db_state->serial, db_crc_len);
      strlcpy(db_crc  + _len,
            "|serial",
            db_crc_len - _len);
   }
   else
      snprintf(db_crc, db_crc_len, "%08lX|crc",
      (unsigned long)db_info_entry->crc32);

   if (entry_path)
      strlcpy(entry_path_str, entry_path, str_len);

   /* Use database name for label if found,
    * otherwise use filename without extension */
   if (db_info_entry->name && *db_info_entry->name)
   {
      /* Use the archive as path instead of the file inside the archive
       * if the file is a multidisk game, because database entry
       * matches with the last disk, which is never bootable */
      char *delim = (char*)strchr(entry_path_str, '#');

      if (delim && compat_strcasestr(entry_path_str, " (Disk "))
         *delim = '\0';

      strlcpy(entry_lbl, db_info_entry->name, sizeof(entry_lbl));
   }
   else if (entry_path && *entry_path)
   {
      char *delim = (char*)strchr(entry_path, '#');

      if (delim)
         *delim = '\0';
      fill_pathname(entry_lbl,
            path_basename_nocompression(entry_path), "", sizeof(entry_lbl));

      RARCH_LOG("[Scanner] Faulty match for: \"%s\", CRC: 0x%08X.\n", entry_path_str, db_state->crc);
   }

   if (archive_name && *archive_name)
      fill_pathname_join_delim(entry_path_str,
            entry_path_str, archive_name, '#', str_len);

   if (core_info_database_match_archive_member(
         db_state->list->elems[db_state->list_index].data)
       && (hash = strchr(entry_path_str, '#')))
       *hash = '\0';

   /* Accumulate result instead of immediately updating playlist */

   if (!scan_results_add(&_db->scan_results, entry_path_str, entry_lbl, db_crc, 
                         _db->task_config->omit_db_reference ? _db->task_config->dat_file_path : db_playlist_base_str, 
                         archive_name))
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

      /* The index caches are keyed by the same position, so they have
       * to travel with the entry.  Leaving them behind pairs a
       * database with another database's index, and the lookup then
       * answers with records that belong to a different system. */
      {
         database_info_crc_index_t    *ci =
            db_state->crc_index[db_state->list_index];
         database_info_serial_index_t *si =
            db_state->serial_index[db_state->list_index];

         memmove(&db_state->crc_index[1],
                 &db_state->crc_index[0],
                 sizeof(ci) * db_state->list_index);
         memmove(&db_state->serial_index[1],
                 &db_state->serial_index[0],
                 sizeof(si) * db_state->list_index);

         db_state->crc_index[0]    = ci;
         db_state->serial_index[0] = si;
      }

      db_state->list->elems[0] = entry;
      db_state->min_sizes[0]   = min;
      db_state->max_sizes[0]   = max;
      db_state->flags[0]       = flag;
      db_state->flags[0]      |= DB_STATE_FLAG_MATCHED;
   }

   if (db_crc != db_crc_buf)
      free(db_crc);
   free(entry_path_str);
   return SCAN_VERDICT_MATCHED_DB;
}

/* End of entries in database info list and didn't find a
 * match, go to the next database. */
static enum scan_verdict database_info_list_iterate_next(
      database_state_handle_t *db_state)
{
   db_state->list_index++;
   db_state->entry_index = 0;

   database_info_list_free(db_state->info);
   free(db_state->info);
   db_state->info        = NULL;

   return SCAN_VERDICT_CONTINUE;
}

/* Allocate the per-database size/flag caches once the database list
 * is known.  Returns false on OOM; the caller aborts the scan. */
static bool task_database_state_alloc_arrays(
      database_state_handle_t *db_state)
{
   size_t count;

   if (!db_state || !db_state->list)
      return false;

   count = db_state->list->size;

   /* An empty database directory is not an error: every lookup path
    * bails on "list_index == list->size" before touching these. */
   if (count == 0)
      return true;

   db_state->min_sizes = (int64_t*)calloc(count, sizeof(int64_t));
   db_state->max_sizes = (int64_t*)calloc(count, sizeof(int64_t));
   db_state->flags     = (uint8_t*)calloc(count, sizeof(uint8_t));
   db_state->crc_index = (database_info_crc_index_t**)
      calloc(count, sizeof(*db_state->crc_index));
   db_state->serial_index = (database_info_serial_index_t**)
      calloc(count, sizeof(*db_state->serial_index));
   db_state->index_budget = task_database_index_budget();

   if (   !db_state->min_sizes
       || !db_state->max_sizes
       || !db_state->flags
       || !db_state->crc_index
       || !db_state->serial_index)
      return false;

   return true;
}

static void task_database_fill_db_min_max(database_state_handle_t *db_state)
{
   char query[50];
   query[0] = '\0';

   /* Marked up front so that every exit below - including the ones
    * that record a placeholder range - counts as having been asked.
    * This is what stops the two walks repeating per content file. */
   db_state->flags[db_state->list_index] |= DB_STATE_FLAG_SIZE_CHECKED;

   /* The crc index collects the size range while it walks, so build
    * it here and take the range from it: one walk instead of this
    * function's two, and the crc lookup then finds the index already
    * built.  The queries below still run for a database the index
    * cannot cover. */
   {
      const char *rdb = db_state->list->elems[db_state->list_index].data;
      int64_t     lo  = 0;
      int64_t     hi  = 0;

      if (rdb && *rdb)
      {
         if (   !db_state->crc_index[db_state->list_index]
             && db_state->index_budget)
         {
            database_info_crc_index_t *ci =
               database_info_crc_index_new(rdb, db_state->index_budget);
            if (ci)
            {
               size_t used = database_info_crc_index_bytes(ci);
               db_state->index_budget = (used < db_state->index_budget)
                  ? db_state->index_budget - used : 0;
               db_state->crc_index[db_state->list_index] = ci;
            }
         }

         if (   db_state->crc_index[db_state->list_index]
             && database_info_crc_index_size_range(
                   db_state->crc_index[db_state->list_index], &lo, &hi))
         {
            db_state->min_sizes[db_state->list_index] = lo;
            db_state->max_sizes[db_state->list_index] = hi;
            db_state->flags[db_state->list_index]    |=
               DB_STATE_FLAG_HAS_SIZE;

            /* HAS_SERIAL gates a skip in the serial lookup, and this
             * walk does not observe serial keys - it scans crc with
             * size alongside.  Set it rather than leave it clear:
             * clear would skip every database for disc content, which
             * is a missed match, while set costs at most one wasted
             * serial-index build for a database that carries none.
             * Every database shipped today carries serials.
             *
             * HAS_CRC is set for symmetry; nothing reads it beyond a
             * debug line. */
            db_state->flags[db_state->list_index] |=
               DB_STATE_FLAG_HAS_CRC | DB_STATE_FLAG_HAS_SERIAL;

            db_state->entry_index = 0;
            return;
         }
      }
   }

   snprintf(query, sizeof(query), "{size:min(0)}");
   database_info_list_iterate_new(db_state, query);

   /* database_info_list_new_filtered() returns NULL when the .rdb
    * cannot be opened, when the query fails to compile, or on OOM, so
    * db_state->info is not guaranteed here.  Treat a failed query the
    * same way an empty result is treated below: mark the database as
    * having no usable size range and move on. */
   if (!db_state->info)
   {
      db_state->min_sizes[db_state->list_index] = -1;
      db_state->max_sizes[db_state->list_index] = -1;
      db_state->entry_index                     = 0;
      return;
   }

   if (db_state->info->count > 0)
   {
      db_state->min_sizes[db_state->list_index] = db_state->info->list[db_state->info->count-1].size;
      snprintf(query, sizeof(query), "{size:max(0)}");
      database_info_list_iterate_new(db_state, query);

      /* Second query, same failure modes as the first. */
      if (db_state->info && db_state->info->count > 0)
      {
         size_t i;
         db_state->max_sizes[db_state->list_index] = db_state->info->list[db_state->info->count-1].size;
         db_state->flags[db_state->list_index] |= DB_STATE_FLAG_HAS_SIZE;
         for(i=0 ; i < db_state->info->count; i++)
         {
            if (   db_state->info->list[i].serial 
                && strlen(db_state->info->list[i].serial)>0)
               db_state->flags[db_state->list_index] |= DB_STATE_FLAG_HAS_SERIAL;
            if (db_state->info->list[i].crc32 > 0)
               db_state->flags[db_state->list_index] |= DB_STATE_FLAG_HAS_CRC;
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

static enum scan_verdict task_database_iterate_crc_lookup(
      manual_scan_handle_t *_db,
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
      return database_info_list_iterate_end_no_match(_db, db, db_state, name,
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
   if (!(db_state->flags[db_state->list_index] & DB_STATE_FLAG_SIZE_CHECKED))
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
      else if (db_state->size == 0)
      {
#ifdef DEBUG
         RARCH_DBG("[Scanner] Zero-length file, skipping database match\n");
#endif
         return database_info_list_iterate_next(db_state);
      }

   }

   if (db_state->entry_index == 0)
   {
      char query[50];
      const char *rdb            = db_state->list->elems[
         db_state->list_index].data;
      database_info_list_t *hits = NULL;

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

      /* Answer from this database's crc index when we can.  Building
       * it costs one walk - about what a single probe used to cost -
       * and every later content file is then a binary search instead
       * of another walk.  The index is only a faster route to the
       * same records: it reports them in file order with the same
       * fields extracted, so the matching below is unchanged.
       *
       * Anything the index cannot serve falls through to the query,
       * which stays the reference path. */
      if (rdb && *rdb)
      {
         if (   !db_state->crc_index[db_state->list_index]
             && db_state->index_budget)
         {
            database_info_crc_index_t *ci =
               database_info_crc_index_new(rdb, db_state->index_budget);
            if (ci)
            {
               size_t used = database_info_crc_index_bytes(ci);
               db_state->index_budget = (used < db_state->index_budget)
                  ? db_state->index_budget - used : 0;
               db_state->crc_index[db_state->list_index] = ci;
            }
         }

         if (db_state->crc_index[db_state->list_index])
            hits = database_info_list_new_crc(
                  db_state->crc_index[db_state->list_index], rdb,
                  db_state->crc, db_state->archive_crc,
                  DB_EXTRACT_SCAN_FIELDS);
      }

      if (hits)
      {
         if (db_state->info)
         {
            database_info_list_free(db_state->info);
            free(db_state->info);
         }
         db_state->info = hits;
      }
      else
      {
         snprintf(query, sizeof(query),
               "{crc:or(b\"%08lX\",b\"%08lX\")}",
               (unsigned long)db_state->crc,
               (unsigned long)db_state->archive_crc);

         database_info_list_iterate_new(db_state, query);
      }
   }

   /* Same shape as the serial lookup below: entry_index was used to
    * index the list without checking it against count, so a query
    * that matched nothing (count == 0, list either empty or NULL)
    * still had list[0] dereferenced. */
   if (db_state->info && db_state->entry_index < db_state->info->count)
   {
      database_info_t *db_info_entry =
         &db_state->info->list[db_state->entry_index];

      /* When scanning an archive, "first" file crc32 is also checked. */
      if (db_info_entry->crc32)
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
      return SCAN_VERDICT_CONTINUE;

   database_info_list_free(db_state->info);

   if (db_state->info)
   {
      free(db_state->info);
      db_state->info = NULL;
   }

   return SCAN_VERDICT_NO_DB_MATCH;
}

/* There is a Lutro database, but without crc/serial, so all .lutro files will be recognized. */
static int task_database_iterate_playlist_lutro(
      manual_scan_handle_t *_db,
      database_state_handle_t *db_state,
      database_info_handle_t *db,
      const char *path)
{
   char game_title[NAME_MAX_LENGTH];
   fill_pathname(game_title,
         path_basename(path), "", sizeof(game_title));

   /* Skip if strict scan was asked with specific database */
   if ((_db->task_config->db_usage == MANUAL_CONTENT_SCAN_USE_DB_STRICT)
       && *_db->task_config->database_name
       && memcmp(_db->task_config->database_name, "Lutro",  6) != 0 )
      return SCAN_VERDICT_NO_DB_MATCH;

   scan_results_add(&_db->scan_results,
                    path,
                    game_title,
                    (char*)"00000000|crc",
                    "Lutro.lpl",
                    ""
                    );

   return SCAN_VERDICT_MATCHED_DB;
}

static bool task_database_check_serial_and_crc(
      database_state_handle_t *db_state)
{
   const char *db_name;
   if (!config_get_ptr()->bools.scan_serial_and_crc)
       return false;
   /* database_info_get_current_name() can return NULL (missing
    * handle/list, or a NULL element). Guard it before it reaches
    * path_basename_nocompression()/string_starts_with(), both of
    * which would dereference the NULL and crash. */
   if (!(db_name = database_info_get_current_name(db_state)))
       return false;
   db_name = path_basename_nocompression(db_name);
   /* the PSP shares serials for disc/download content */
   return (string_starts_with(db_name, "Sony - PlayStation Portable") ||
           string_starts_with(db_name, "Sega - Dreamcast"));
}

static int task_database_iterate_serial_lookup(
      manual_scan_handle_t *_db,
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
           !db_state->list
         || (unsigned)db_state->list_index == (unsigned)db_state->list->size
         || ( _db->flags & DB_HANDLE_FLAG_USE_FIRST_MATCH_ONLY
         && db_state->list_index > 0 
         && db_state->flags[0] & DB_STATE_FLAG_MATCHED)
      )
      return database_info_list_iterate_end_no_match(_db, db, db_state, name,
            path_contains_compressed_file);

   /* If size boundaries are not filled for this DB, run the queries */
   if (!(db_state->flags[db_state->list_index] & DB_STATE_FLAG_SIZE_CHECKED))
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

      /* Size check is conditional - it is unreliable in 
       * case of multitrack formats as serial is always 
       * in the first track, which may not be the actual game data.
       * Same for those compressed image formats that are not 
       * supported by VFS. */

      /* Examining ZIP file main entry (archive size filled, but 
         no indication of compressed file) */
      if (     size_hint_allowed 
           && !path_contains_compressed_file 
           && db_state->archive_size > 0)
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
      size_t query_len;
      char  query_buf[128];
      char *query      = query_buf;
      char *serial_buf;
      const char *rdb  = db_state->list->elems[db_state->list_index].data;
      database_info_list_t *hits = NULL;

      /* Same treatment as the crc path: one walk builds this
       * database's serial index, and every content file after that is
       * a lookup instead of another walk.  The index reports the same
       * records in the same order with the same fields extracted, and
       * returns NULL for anything it cannot serve so the query below
       * still runs. */
      if (rdb && *rdb)
      {
         if (   !db_state->serial_index[db_state->list_index]
             && db_state->index_budget)
         {
            database_info_serial_index_t *si =
               database_info_serial_index_new(rdb, db_state->index_budget);
            if (si)
            {
               size_t used = database_info_serial_index_bytes(si);
               db_state->index_budget = (used < db_state->index_budget)
                  ? db_state->index_budget - used : 0;
               db_state->serial_index[db_state->list_index] = si;
            }
         }

         if (db_state->serial_index[db_state->list_index])
            hits = database_info_list_new_serial(
                  db_state->serial_index[db_state->list_index], rdb,
                  db_state->serial, DB_EXTRACT_SCAN_FIELDS);
      }

      if (hits)
      {
         if (db_state->info)
         {
            database_info_list_free(db_state->info);
            free(db_state->info);
         }
         db_state->info = hits;
         goto serial_query_done;
      }

      serial_buf = bin_to_hex_alloc(
            (uint8_t*)db_state->serial,
            strlen(db_state->serial) * sizeof(uint8_t));

      if (!serial_buf)
         return SCAN_VERDICT_ERROR;

      /* strlcpy() returns the length of its *source*, not the number
       * of bytes it copied, so the previous form advanced _len by the
       * full hex expansion even when the copy had been truncated to
       * fit.  The three stores that follow then landed at
       * query[13 + 2 * strlen(serial)], past the end of a 50 byte
       * buffer for any serial longer than 17 characters:
       *
       *   AddressSanitizer: stack-buffer-overflow
       *   WRITE of size 1
       *
       * db_state->serial is 4 KiB, and detect_dc_game() alone
       * concatenates lgame_id[20] and rgame_id[20] before appending a
       * disc suffix, so it can hand us close to forty characters.
       *
       * Size the buffer from the string being built instead, keeping
       * the common case on the stack and falling back to the heap for
       * a serial that does not fit - the same pattern
       * database_info_list_iterate_found_match() uses for db_crc. */
      query_len = STRLEN_CONST("{'serial': b'")
                + strlen(serial_buf)
                + STRLEN_CONST("'}") + 1;

      if (query_len > sizeof(query_buf))
         query = (char*)malloc(query_len);

      if (!query)
      {
         free(serial_buf);
         return SCAN_VERDICT_ERROR;
      }

      snprintf(query, query_len, "{'serial': b'%s'}", serial_buf);
#ifdef DEBUG
      RARCH_DBG("[Scanner] Serial orig / decoded: \"%s\" / \"%s\".\n", db_state->serial, serial_buf);
#endif
      database_info_list_iterate_new(db_state, query);

      if (query != query_buf)
         free(query);
      free(serial_buf);

serial_query_done:
      ;
   }

   if (db_state->info)
   {
      /* "<=" walked one element past the end of the list and then
       * dereferenced it: db_info_entry->serial reads a pointer out of
       * whatever follows the allocation and hands it to
       * string_is_equal().  The list is empty on the common no-match
       * path, so this fired on ordinary scans, not just crafted ones.
       *
       * The "db_info_entry &&" test below was dead - the address of an
       * array element is never NULL - and is dropped with it. */
      while (db_state->entry_index < db_state->info->count)
      {
         database_info_t *db_info_entry = &db_state->info->list[
            db_state->entry_index];

         if (db_info_entry->serial)
         {
            if (string_is_equal(db_state->serial, db_info_entry->serial))
            {
               if (task_database_check_serial_and_crc(db_state))
               {
                  if (db_state->crc == 0)
                  {
                     if (extension_to_file_type(path_get_extension(name)) == FILE_TYPE_GDI)
                        task_database_gdi_get_crc_and_size(name, &db_state->crc, &db_state->size);
                     else
                        intfstream_file_get_crc_and_size(name, 0, INT64_MAX, &db_state->crc, &db_state->size);
                  }

                  if (db_state->crc == db_info_entry->crc32)
                     return database_info_list_iterate_found_match(_db,
                           db_state, db, NULL);
               }
               else
                  return database_info_list_iterate_found_match(_db,
                        db_state, db, NULL);
            }
         }
         db_state->entry_index++;
      }
   }

   if (db_state->info)
   {
      if (db_state->entry_index >= db_state->info->count)
         return database_info_list_iterate_next(db_state);
   }

   /* If we haven't reached the end of the database list yet,
    * continue iterating. */
   if (db_state->list_index < db_state->list->size)
      return SCAN_VERDICT_CONTINUE;

   database_info_list_free(db_state->info);
   free(db_state->info);
   db_state->info = NULL;
   return SCAN_VERDICT_NO_DB_MATCH;
}

static int task_database_iterate(
      manual_scan_handle_t *_db,
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
         if (task_database_iterate_playlist(_db, db_state, db, name))
            return SCAN_VERDICT_CONTINUE;
         else
            return SCAN_VERDICT_ERROR;
         
      case DATABASE_TYPE_ITERATE_ARCHIVE:
#ifdef HAVE_COMPRESSION
         return task_database_iterate_crc_lookup(
               _db, db_state, db, name, db_state->archive_name,
               path_contains_compressed_file);
#else
         return SCAN_VERDICT_NO_DB_MATCH;
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

   return SCAN_VERDICT_ERROR;
}

static void task_database_cleanup_state(database_state_handle_t *db_state)
{
   if (!db_state)
      return;

   if (db_state->buf)
      free(db_state->buf);
   db_state->buf = NULL;
}
#endif
/* Batch update playlists from accumulated scan results */
/* One gather's worth of the batch playlist flush, under the same
 * shared window as the BEGIN family, yielding between results and
 * resuming from the state kept on the handle.
 *
 * The group-close write for multi-playlist scans and the final
 * sort+write are single blocking operations: a playlist file write
 * is atomic by design (a partial write would corrupt the playlist
 * on a cancelled scan), so those are the floor this flush cannot
 * go below.
 *
 * Returns true when the flush has fully completed (including the
 * failure mode of the scratch allocation, which skips the batch),
 * false when yielding. */
static bool manual_scan_walk_within_budget(void *ud);

static bool manual_scan_end_flush_tick(
   manual_scan_handle_t* manual_scan, bool single_playlist)
{
   scan_results_t *sr = &manual_scan->scan_results;
   nbio_budget_t b;
   bool first = true;
   size_t str_len = PATH_MAX_LENGTH * sizeof(char);

   if (!manual_scan->flush_started)
   {
      manual_scan->flush_started = true;

      if (!(manual_scan->flush_path_buf = (char*)malloc(str_len)))
      {
         RARCH_ERR("[Scanner] Failed to allocate memory for batch playlist update.\n");
         return true;
      }

      RARCH_LOG("[Scanner] Batch updating playlists with %u results...\n",
               (unsigned)sr->count);

      if (single_playlist)
      {
         manual_scan->flush_group    = manual_scan->task_config->playlist_file;
         manual_scan->flush_playlist = manual_scan->playlist;
         /* NULL on failure: existence checks then take the linear
          * fallback below */
         manual_scan->flush_dedup    = playlist_dedup_init();
      }
   }
   else if (!manual_scan->flush_path_buf)
      return true;   /* scratch allocation failed on a previous gather */

   task_nbio_slice_open(&b);

   /* Process results, grouping by playlist */
   while (manual_scan->flush_pos < sr->count)
   {
      scan_result_t *result = &sr->results[manual_scan->flush_pos];
      char db_name_noext[PATH_MAX_LENGTH];
      /* The key identifying which playlist this result belongs to:
       * the fixed playlist file when one is set, else the database
       * name.  The fixed path never equals a db_name, so comparing
       * against db_name unconditionally here would close and reopen
       * the playlist (a full write + parse) once per result. */
      const char *group_key = (*manual_scan->task_config->playlist_file)
         ? manual_scan->task_config->playlist_file
         : result->db_name;
      bool entry_present;
      bool is_m3u;

      /* The floor: one result per gather regardless of the window,
       * then yield between any two results. */
      if (!first && !task_nbio_slice_within_budget(&b, 0, 0))
      {
         task_nbio_slice_close(&b);
         return false;
      }
      first = false;

      strlcpy(db_name_noext, result->db_name, sizeof(db_name_noext));
      path_remove_extension(db_name_noext);

      /* Check if we need to switch to a different playlist */
      if (!single_playlist && (!manual_scan->flush_group || !string_is_equal(manual_scan->flush_group, group_key)))
      {
         /* Write and close previous playlist if any.  One blocking
          * write per group. */
         if (manual_scan->flush_playlist)
         {
            RARCH_LOG("[Scanner] Added %u entries to \"%s\".\n", manual_scan->flush_added, manual_scan->flush_group);
            playlist_write_file(manual_scan->flush_playlist);
            playlist_free(manual_scan->flush_playlist);
            manual_scan->flush_playlist = NULL;
            manual_scan->flush_added = 0;
         }
         playlist_dedup_free(manual_scan->flush_dedup);
         manual_scan->flush_dedup = NULL;

         /* Open new playlist - if not fixed, use database name */
         if (!*manual_scan->task_config->playlist_file)
         {
            manual_scan->flush_group = result->db_name;
            manual_scan->flush_path_buf[0] = '\0';
            if (manual_scan->playlist_directory && *manual_scan->playlist_directory)
               fill_pathname_join_special(manual_scan->flush_path_buf, manual_scan->playlist_directory,
                     result->db_name, str_len);
            playlist_config_set_path(&manual_scan->playlist_config, manual_scan->flush_path_buf);
         }
         else
         {
            manual_scan->flush_group = manual_scan->task_config->playlist_file;
            playlist_config_set_path(&manual_scan->playlist_config, manual_scan->flush_group);
         }

         manual_scan->flush_playlist = playlist_init(&manual_scan->playlist_config);

         /* Check before use: the playlist_set_scan_* calls below are
          * not all NULL-guarded (playlist_set_scan_search_recursively,
          * playlist_set_sort_mode, playlist_qsort, playlist_write_file
          * and several others dereference unconditionally).  The test
          * used to sit after all of them. */
         if (!manual_scan->flush_playlist)
         {
            RARCH_ERR("[Scanner] Failed to open playlist: \"%s\".\n", result->db_name);
            manual_scan->flush_group = NULL;
            /* Advance past this result before continuing, or the
             * same unopenable playlist is retried every gather
             * forever. */
            manual_scan->flush_pos++;
            continue;
         }

         /* NULL on failure: existence checks then take the linear
          * fallback below */
         manual_scan->flush_dedup = playlist_dedup_init();

         /* Set default core, if required */
         if (manual_scan->task_config->core_set)
         {
            playlist_set_default_core_path(manual_scan->flush_playlist,
                  manual_scan->task_config->core_path);
            playlist_set_default_core_name(manual_scan->flush_playlist,
                  manual_scan->task_config->core_name);
         }

         /* Record remaining scan parameters to enable
          * subsequent 'refresh playlist' operations */
         playlist_set_scan_content_dir(manual_scan->flush_playlist,
               manual_scan->task_config->content_dir);
         playlist_set_scan_file_exts(manual_scan->flush_playlist,
               manual_scan->task_config->file_exts_custom_set ?
                     manual_scan->task_config->file_exts : NULL);
         if (manual_scan->task_config->db_usage == MANUAL_CONTENT_SCAN_USE_DB_DAT_LOOSE ||
             manual_scan->task_config->db_usage == MANUAL_CONTENT_SCAN_USE_DB_DAT_STRICT)
            playlist_set_scan_dat_file_path(manual_scan->flush_playlist,
                  manual_scan->task_config->dat_file_path);
         playlist_set_scan_database_name(manual_scan->flush_playlist,
                  manual_scan->task_config->database_name);
         playlist_set_scan_search_recursively(manual_scan->flush_playlist,
               manual_scan->task_config->search_recursively);
         playlist_set_scan_search_archives(manual_scan->flush_playlist,
               manual_scan->task_config->search_archives);
         playlist_set_scan_filter_dat_content(manual_scan->flush_playlist,
               manual_scan->task_config->filter_dat_content);
         playlist_set_scan_overwrite_playlist(manual_scan->flush_playlist,
               manual_scan->task_config->overwrite_playlist);
         playlist_set_scan_db_usage(manual_scan->flush_playlist,
               manual_scan->task_config->db_usage);
         playlist_set_scan_omit_db_ref(manual_scan->flush_playlist,
               manual_scan->task_config->omit_db_reference);

         RARCH_LOG("[Scanner] Processing playlist: \"%s\".\n", result->db_name);
      }

      /* Index the playlist's pre-existing entries before consulting
       * the dedup index, resuming across gathers on the shared
       * window.  flush_pos has not advanced, so a resumed gather
       * re-enters here with the same group. */
      if (   manual_scan->flush_dedup
          && manual_scan->flush_playlist
          && !playlist_dedup_seed_step(manual_scan->flush_dedup,
                manual_scan->flush_playlist,
                manual_scan_walk_within_budget, &b))
      {
         task_nbio_slice_close(&b);
         return false;
      }

      /* Add entry to playlist if it doesn't already exist */
      /* ...except for M3U, since the processing occurs at the end,
         we overwrite any previous m3u entry (which has same file,
         but less descriptive label, database, crc */
      is_m3u        = rm3u_is_m3u_filestream(result->entry_path);
      /* will_add records the path as present exactly when this
       * result is pushed below: when absent (the push is
       * unconditional), and in the m3u-present case the path
       * remains present across the delete + re-push. */
      entry_present = manual_scan->flush_dedup
         ? playlist_dedup_check_add(manual_scan->flush_dedup,
               manual_scan->flush_playlist, result->entry_path, true)
         : playlist_entry_exists(manual_scan->flush_playlist,
               result->entry_path);

      if (manual_scan->flush_playlist && (!entry_present || is_m3u))
      {
         struct playlist_entry entry;

         if(is_m3u)
            playlist_delete_by_path(manual_scan->flush_playlist, result->entry_path);
         
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

         /* Absence is proven: the existence check above said so, or
          * this is an m3u whose previous entries were just deleted.
          * The checked playlist_push() would re-scan the entire
          * playlist for the same answer. */
         playlist_push_unchecked(manual_scan->flush_playlist, &entry);
         manual_scan->flush_added++;

         RARCH_LOG("[Scanner] Add \"%s / %s\".\n", db_name_noext, result->entry_label);

         if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_DATABASE_SCAN, NULL))
            task_database_scan_console_output(result->entry_label,
                  db_name_noext, true);
      }
      /* Entry already exists - output duplicate indicator for CLI scans */
      else if (manual_scan->flush_playlist && retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_DATABASE_SCAN, NULL))
         task_database_scan_console_output(result->entry_label,
               db_name_noext, false);

      manual_scan->flush_pos++;
   }

   /* Write and close final playlist */
   if (manual_scan->flush_playlist)
   {
      RARCH_LOG("[Scanner] Added %u entries to \"%s\".\n", manual_scan->flush_added, manual_scan->flush_group);
      /* Ensure playlist is alphabetically sorted (matches manual scan behavior) */
      playlist_set_sort_mode(manual_scan->flush_playlist, PLAYLIST_SORT_MODE_DEFAULT);
      playlist_qsort(manual_scan->flush_playlist);
      playlist_write_file(manual_scan->flush_playlist);
      /* Free whatever this flush opened.  Only the single-playlist
       * path borrows manual_scan->playlist, which the handle teardown
       * owns; comparing the handles says that exactly, where the
       * previous string compare against playlist_file also matched
       * the playlist this flush had opened itself and leaked it. */
      if (manual_scan->flush_playlist != manual_scan->playlist)
         playlist_free(manual_scan->flush_playlist);
      manual_scan->flush_playlist = NULL;
   }

   playlist_dedup_free(manual_scan->flush_dedup);
   manual_scan->flush_dedup = NULL;

   free(manual_scan->flush_path_buf);
   manual_scan->flush_path_buf = NULL;
   RARCH_LOG("[Scanner] Batch playlist update complete.\n");
   task_nbio_slice_close(&b);
   return true;
}

#ifdef HAVE_LIBRETRODB
bool task_push_dbscan(
      const char *playlist_directory, /* always from settings */
      const char *content_database,   /* always from settings */
      const char *fullpath,
      bool directory,
      bool db_dir_show_hidden_files,  /* always from settings */
      retro_task_callback_t cb)
{
   manual_content_scan_set_menu_content_dir(fullpath);
   /*manual_content_scan_set_menu_scan_method(MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC);*/
   return task_push_manual_content_scan(false, cb);
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

   /* A cancelled task can die mid-flush: close the playlist the
    * flush had open.  This must run before manual_scan->playlist is
    * released below - in single-playlist mode flush_playlist borrows
    * that handle, and the identity comparison that prevents a double
    * free needs the pointer still live to say so. */
   if (  manual_scan->flush_playlist
       && manual_scan->flush_playlist != manual_scan->playlist)
      playlist_free(manual_scan->flush_playlist);
   manual_scan->flush_playlist = NULL;

   /* The dedup index owns all of its state; order relative to the
    * playlist frees is immaterial. */
   playlist_dedup_free(manual_scan->flush_dedup);
   manual_scan->flush_dedup = NULL;

   if (manual_scan->flush_path_buf)
   {
      free(manual_scan->flush_path_buf);
      manual_scan->flush_path_buf = NULL;
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

   /* A cancelled task can die mid-walk or mid-DAT-read; the
    * iterator closes any directory handles still open, and the
    * read buffer is only non-NULL while its ownership is still
    * here (it passes to logiqx_dat_init_owned() on completion). */
   if (manual_scan->content_iter)
   {
      dir_list_iter_free(manual_scan->content_iter);
      manual_scan->content_iter = NULL;
   }

   if (manual_scan->dat_stream)
   {
      filestream_close(manual_scan->dat_stream);
      manual_scan->dat_stream = NULL;
   }

   if (manual_scan->dat_buf)
   {
      free(manual_scan->dat_buf);
      manual_scan->dat_buf = NULL;
   }

   /* A cancelled task can also die mid-parse. */
   if (manual_scan->dat_parse)
   {
      logiqx_dat_parse_abort(manual_scan->dat_parse);
      manual_scan->dat_parse = NULL;
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

   /* Free accumulated scan results */
   scan_results_free(&manual_scan->scan_results);

   if (manual_scan->playlist_directory)
      free(manual_scan->playlist_directory);
   manual_scan->playlist_directory = NULL;

#ifdef HAVE_LIBRETRODB
   if (1)
   {
      database_state_handle_t *dbstate = &manual_scan->state;

      if (dbstate)
      {
         /* The per-database arrays are sized from the list, so the
          * count has to be taken before the list goes away.  Reading
          * it afterwards is a use-after-free, and the value it
          * returns then drives the loops below. */
         size_t db_count = dbstate->list ? dbstate->list->size : 0;

         if (dbstate->list)
         {
            dir_list_free(dbstate->list);
            dbstate->list = NULL;
         }
         /* db_state->info is only released along the iterate paths,
          * so cancelling a scan while a database query result was
          * live leaked the whole database_info_list_t - every
          * strdup'd field of every matched record. */
         if (dbstate->info)
         {
            database_info_list_free(dbstate->info);
            free(dbstate->info);
            dbstate->info = NULL;
         }
         if (dbstate->crc_index)
         {
            size_t ci;
            for (ci = 0; ci < db_count; ci++)
               database_info_crc_index_free(dbstate->crc_index[ci]);
            free(dbstate->crc_index);
            dbstate->crc_index = NULL;
         }
         if (dbstate->serial_index)
         {
            size_t si;
            for (si = 0; si < db_count; si++)
               database_info_serial_index_free(dbstate->serial_index[si]);
            free(dbstate->serial_index);
            dbstate->serial_index = NULL;
         }
         if (dbstate->min_sizes)
            free(dbstate->min_sizes);
         if (dbstate->max_sizes)
            free(dbstate->max_sizes);
         if (dbstate->flags)
            free(dbstate->flags);
         dbstate->min_sizes = NULL;
         dbstate->max_sizes = NULL;
         dbstate->flags     = NULL;
      }

      if (manual_scan->content_database_path)
         free(manual_scan->content_database_path);
      manual_scan->content_database_path = NULL;
      if (manual_scan->state.buf)
         free(manual_scan->state.buf);
      if (manual_scan->handle)
         free(manual_scan->handle);
   }
#endif

   free(manual_scan);
   manual_scan = NULL;
}

static void cb_task_manual_content_scan(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   manual_scan_handle_t *manual_scan = NULL;
   playlist_t *cached_playlist       = playlist_get_cached();
#if defined(HAVE_MENU)
   struct menu_state *menu_st        = menu_state_get_ptr();
   if (!task)
      goto end;
#else
   if (!task)
      return;
#endif

   if (!(manual_scan = (manual_scan_handle_t*)task->state))
   {
#if defined(HAVE_MENU)
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

#if defined(HAVE_MENU)
end:
#endif
   /* The caller's callback, if it gave one.  Read before the handle is
    * released below.
    *
    * This used to sit inside the HAVE_MENU block along with the menu
    * refresh, so a build without menu support ran the scan and then
    * dropped the callback: a caller waiting on it waited forever.
    * The in-tree callers only supply one under HAVE_MENU themselves,
    * which is why nothing noticed, but the parameter is not
    * documented as menu-only and the sample in samples/tasks/database
    * hangs on exactly this. */
   if (manual_scan && manual_scan->user_cb)
      manual_scan->user_cb(task, task_data, user_data, err);

#if defined(HAVE_MENU)
   /* When creating playlists, the playlist tabs of
    * any active menu driver must be refreshed */
   if (   
#ifdef HAVE_LIBRETRODB
         (!manual_scan || 
         (manual_scan->flags & DB_HANDLE_FLAG_DO_MENU_REFRESH)) && 
#endif
       menu_st->driver_ctx->environ_cb)
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

/* --- MANUAL_SCAN_BEGIN family ---------------------------------------
 *
 * A short sequence of budgeted sub-states sharing the per-frame I/O
 * window with the file-transfer spine (task_nbio_slice_*).  The walk
 * is incremental and lands directly in the content list, and the DAT
 * is read in chunks into a single buffer that is then handed to the
 * parser whole, so no single gather freezes the frame.  Each
 * sub-state function returns true when the task must finish (error
 * or nothing to scan). */

/* Chunk size for the budgeted DAT read.  Large enough that fast
 * storage delivers a big DAT in a handful of gathers, small enough
 * that the guaranteed floor chunk of a spent window costs well under
 * a millisecond. */
#define MANUAL_SCAN_DAT_CHUNK (256 * 1024)

/* Bytes of XML handed to logiqx_dat_parse_step() per step: small
 * enough that several steps fit one shared window in a plain build
 * and one step stays frame-sized even under sanitizer inflation,
 * large enough that a MAME-sized list completes in a few hundred
 * steps. */
#define MANUAL_SCAN_DAT_PARSE_STEP (256 * 1024)

/* dir_list_iter_step()'s budget callback carries no sizes. */
static bool manual_scan_walk_within_budget(void *ud)
{
   return task_nbio_slice_within_budget(ud, 0, 0);
}

static bool manual_scan_begin_setup(retro_task_t *task,
      manual_scan_handle_t *manual_scan)
{
   /* Initialize scan results accumulation */
   if (!scan_results_init(&manual_scan->scan_results, 1024))
   {
      RARCH_ERR("[Scanner] Failed to initialize scan results\n");
      return true;
   }

#ifdef HAVE_LIBRETRODB
   if ((manual_scan->task_config->db_usage == MANUAL_CONTENT_SCAN_USE_DB_STRICT ||
        manual_scan->task_config->db_usage == MANUAL_CONTENT_SCAN_USE_DB_LOOSE) &&
       !(manual_scan->flags & DB_HANDLE_FLAG_SCAN_STARTED))
   {
      database_state_handle_t *dbstate = &manual_scan->state;

      manual_scan->flags       |= DB_HANDLE_FLAG_SCAN_STARTED;

      /* No content dir: nothing to scan. */
      if (!*manual_scan->task_config->content_dir)
         return true;

      if (manual_scan->flags & DB_HANDLE_FLAG_IS_DIRECTORY)
      {
         /* Start the incremental walk.  Extension derivation
          * matches database_info_dir_init(): all supported core
          * extensions unless an explicit list is given.  The
          * cue/gdi-prioritising sort follows on completion in
          * database_info_dir_init_from_list(). */
         core_info_list_t *core_info_list = NULL;
         char *file_exts                  =
               manual_scan->task_config->file_exts;

         if (!file_exts || !*file_exts)
            core_info_get_list(&core_info_list);

         if ((manual_scan->content_list = string_list_new()))
            manual_scan->content_iter = dir_list_iter_new(
                  manual_scan->task_config->content_dir,
                  core_info_list ? core_info_list->all_ext : file_exts,
                  false,
                  manual_scan->flags & DB_HANDLE_FLAG_SHOW_HIDDEN_FILES,
                  manual_scan->task_config->search_archives,
                  manual_scan->task_config->search_recursively,
                  manual_scan->content_list);

         if (!manual_scan->content_iter)
            return true;
      }
      else
      {
         if (!(manual_scan->handle = database_info_file_init(
               manual_scan->task_config->content_dir,
               DATABASE_TYPE_ITERATE,
               task, &manual_scan->content_list)))
            return true;
         manual_scan->handle->status = DATABASE_STATUS_ITERATE_START;
      }

      if (dbstate && !dbstate->list)
      {
         if (manual_scan->content_database_path && *manual_scan->content_database_path)
         {
            if (manual_scan->task_config->db_selection == MANUAL_CONTENT_SCAN_SELECT_DB_SPECIFIC)
            {
               size_t str_len     = PATH_MAX_LENGTH * sizeof(char);
               char* rdb_name     = (char*)malloc(str_len);
               char* rdb_fullpath = (char*)malloc(str_len);
               union string_list_elem_attr attr;
               attr.i = 0;

               /* Bail out if either heap allocation failed.
                * fill_pathname and fill_pathname_join_special
                * both call strlcpy on their destination buffer
                * with no NULL guard, so proceeding with a NULL
                * rdb_name / rdb_fullpath would segfault.  Free
                * the one that did succeed (free(NULL) is a
                * no-op so no conditional needed) before taking
                * the task-finished exit. */
               if (!rdb_name || !rdb_fullpath)
               {
                  free(rdb_name);
                  free(rdb_fullpath);
                  return true;
               }

               fill_pathname(rdb_name,
                     manual_scan->task_config->database_name,
                     ".rdb", str_len);

               fill_pathname_join_special(rdb_fullpath,
                     manual_scan->content_database_path,
                     rdb_name, str_len);

               dbstate->list = string_list_new();
               if (!dbstate->list)
               {
                  /* Earlier code goto'd here without freeing
                   * the two buffers above, leaking ~8 KiB on
                   * this OOM path.  Free them explicitly. */
                  free(rdb_name);
                  free(rdb_fullpath);
                  return true;
               }
               string_list_append(dbstate->list, rdb_fullpath, attr);
               free(rdb_name);
               free(rdb_fullpath);
            }
            else
            {
               dbstate->list        = dir_list_new(
                     manual_scan->content_database_path,
                     "rdb", false,
                     manual_scan->flags & DB_HANDLE_FLAG_SHOW_HIDDEN_FILES,
                     false, false);
            }

            /* Size the per-database size/flag caches to the
             * database list we just built.  Both branches
             * above land here. */
            if (   dbstate->list
                && !task_database_state_alloc_arrays(dbstate))
            {
               RARCH_ERR("[Scanner] Out of memory allocating database state\n");
               return true;
            }
         }

         RARCH_LOG("[Scanner] %s\"%s\"...\n", msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_START), manual_scan->content_database_path);
         if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_DATABASE_SCAN, NULL))
            printf("%s\"%s\"...\n", msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_START), manual_scan->content_database_path);
      }
   }
   else
#endif
   {
      /* Get allowed file extensions list */
      if (*manual_scan->task_config->file_exts)
         manual_scan->file_exts_list = string_split(
               manual_scan->task_config->file_exts, "|");

      /* Start the incremental walk.  The same policy (extension
       * filter, compressed inclusion) as
       * manual_content_scan_get_content_list() is applied by the
       * shared helper, and a plain-file content dir completes here
       * with no iterator. */
      if (!manual_content_scan_content_list_iter_new(
               manual_scan->task_config,
               &manual_scan->content_list,
               &manual_scan->content_iter))
      {
         const char *_msg = msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT);
         runloop_msg_queue_push(_msg, strlen(_msg), 1, 100, true, NULL,\
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         return true;
      }
   }

   manual_scan->status = MANUAL_SCAN_BEGIN_DIR_LIST;
   return false;
}

static bool manual_scan_begin_dir_list(retro_task_t *task,
      manual_scan_handle_t *manual_scan, nbio_budget_t *b)
{
#ifdef HAVE_LIBRETRODB
   bool is_db_scan =
         (manual_scan->task_config->db_usage == MANUAL_CONTENT_SCAN_USE_DB_STRICT ||
          manual_scan->task_config->db_usage == MANUAL_CONTENT_SCAN_USE_DB_LOOSE);
#endif

   (void)task;

   if (manual_scan->content_iter)
   {
      int r = dir_list_iter_step(manual_scan->content_iter,
            manual_scan_walk_within_budget, b);

      if (r == 0)   /* window spent - resume next gather */
         return false;

      dir_list_iter_free(manual_scan->content_iter);
      manual_scan->content_iter = NULL;

      if (r < 0)
      {
         /* Allocation failure mid-walk: the same terminal the old
          * code took when dir_list_new() returned NULL. */
#ifdef HAVE_LIBRETRODB
         if (!is_db_scan)
#endif
         {
            const char *_msg = msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT);
            runloop_msg_queue_push(_msg, strlen(_msg), 1, 100, true, NULL,\
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
         return true;
      }
   }

   /* Walk complete (or was never needed): finalize per branch. */
#ifdef HAVE_LIBRETRODB
   if (is_db_scan)
   {
      if (  (manual_scan->flags & DB_HANDLE_FLAG_IS_DIRECTORY)
          && !manual_scan->handle)
      {
         /* cue, gdi prioritization in sorting */
         if (!(manual_scan->handle = database_info_dir_init_from_list(
               DATABASE_TYPE_ITERATE, manual_scan->content_list)))
            return true;
         manual_scan->handle->status = DATABASE_STATUS_ITERATE_START;
      }
   }
   else
#endif
   {
      /* The blocking getter rejected an empty listing before the
       * task ever saw it; apply the same rule, then the same
       * alphabetical sort (task status messages would be
       * unintuitive in readdir order). */
      if (  !manual_scan->content_list
          || manual_scan->content_list->size < 1)
      {
         const char *_msg = msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT);
         runloop_msg_queue_push(_msg, strlen(_msg), 1, 100, true, NULL,\
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         return true;
      }
      dir_list_sort(manual_scan->content_list, true);
   }

   manual_scan->status = MANUAL_SCAN_BEGIN_DAT_LOAD;
   return false;
}

static bool manual_scan_begin_dat_load(retro_task_t *task,
      manual_scan_handle_t *manual_scan, nbio_budget_t *b)
{
   bool first = true;

   (void)task;

   if (!((manual_scan->task_config->db_usage == MANUAL_CONTENT_SCAN_USE_DB_DAT_STRICT ||
          manual_scan->task_config->db_usage == MANUAL_CONTENT_SCAN_USE_DB_DAT_LOOSE) &&
         *manual_scan->task_config->dat_file_path))
   {
      manual_scan->status = MANUAL_SCAN_BEGIN_PLAYLIST;
      return false;
   }

   if (manual_scan->dat_parse)
      goto parse_phase;

   if (!manual_scan->dat_stream)
   {
      /* Validate path and size up front through the same
       * predicate the menu applied when the path was chosen, so
       * the task cannot drift from the menu's acceptance rules,
       * and size the destination buffer once. */
      uint64_t dat_file_size = 0;
      int64_t _len           = 0;

      if (   !manual_content_scan_dat_path_is_valid(
                manual_scan->task_config->dat_file_path, &dat_file_size)
          /* Reject any size that would not fit in size_t on this
           * platform (mirrors rxml_load_document's guard). */
          || dat_file_size >= (uint64_t)((size_t)-1))
         goto error;
      _len = (int64_t)dat_file_size;

      if (!(manual_scan->dat_buf = (char*)malloc((size_t)(_len + 1))))
         goto error;

      if (!(manual_scan->dat_stream = filestream_open(
            manual_scan->task_config->dat_file_path,
            RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE)))
         goto error;

      manual_scan->dat_size = _len;
      manual_scan->dat_read = 0;
   }

   /* Budgeted fill.  The floor guarantees one chunk per gather even
    * when the shared window is already spent, so progress is made
    * whatever the concurrent load. */
   while (manual_scan->dat_read < manual_scan->dat_size)
   {
      int64_t want = manual_scan->dat_size - manual_scan->dat_read;
      int64_t got;

      if (!first && !task_nbio_slice_within_budget(b, 0, 0))
         return false;   /* resume next gather */
      first = false;

      if (want > MANUAL_SCAN_DAT_CHUNK)
         want = MANUAL_SCAN_DAT_CHUNK;

      got = filestream_read(manual_scan->dat_stream,
            manual_scan->dat_buf + manual_scan->dat_read, want);

      if (got <= 0)   /* truncated underneath us, or a read error */
         goto error;

      manual_scan->dat_read += got;
   }

   filestream_close(manual_scan->dat_stream);
   manual_scan->dat_stream = NULL;

   /* Hand the completed document to the incremental parser.
    * Ownership of the buffer transfers unconditionally: on failure
    * logiqx_dat_parse_begin_owned() frees it. */
   manual_scan->dat_buf[manual_scan->dat_size] = '\0';
   manual_scan->dat_parse = logiqx_dat_parse_begin_owned(
         manual_scan->dat_buf, (size_t)manual_scan->dat_size);
   manual_scan->dat_buf   = NULL;

   if (!manual_scan->dat_parse)
      goto error_no_cleanup;

parse_phase:
   /* Budgeted parse and index build: one step per gather whatever
    * the window says (the floor), then further steps while it
    * lasts.  A resumed gather jumps straight back here. */
   for (;;)
   {
      int r = logiqx_dat_parse_step(manual_scan->dat_parse,
            MANUAL_SCAN_DAT_PARSE_STEP);

      if (r < 0)
      {
         logiqx_dat_parse_end(manual_scan->dat_parse);   /* discards */
         manual_scan->dat_parse = NULL;
         goto error_no_cleanup;
      }
      if (r > 0)
         break;
      if (!task_nbio_slice_within_budget(b, 0, 0))
         return false;   /* resume next gather */
   }

   manual_scan->dat_file  = logiqx_dat_parse_end(manual_scan->dat_parse);
   manual_scan->dat_parse = NULL;

   if (!manual_scan->dat_file)
      goto error_no_cleanup;

   manual_scan->status = MANUAL_SCAN_BEGIN_PLAYLIST;
   return false;

error:
   if (manual_scan->dat_stream)
   {
      filestream_close(manual_scan->dat_stream);
      manual_scan->dat_stream = NULL;
   }
   if (manual_scan->dat_buf)
   {
      free(manual_scan->dat_buf);
      manual_scan->dat_buf = NULL;
   }
error_no_cleanup:
   {
      const char *_msg = msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_DAT_FILE_LOAD_ERROR);
      runloop_msg_queue_push(_msg, strlen(_msg), 1, 100, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
   return true;
}

static bool manual_scan_begin_playlist(retro_task_t *task,
      manual_scan_handle_t *manual_scan)
{
   (void)task;

   /* Open playlist */
   if (manual_scan->task_config->target_is_single_determined_playlist &&
       !(manual_scan->playlist =
            playlist_init(&manual_scan->playlist_config)))
      return true;

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
   if (manual_scan->task_config->db_usage == MANUAL_CONTENT_SCAN_USE_DB_DAT_LOOSE ||
       manual_scan->task_config->db_usage == MANUAL_CONTENT_SCAN_USE_DB_DAT_STRICT)
      playlist_set_scan_dat_file_path(manual_scan->playlist,
            manual_scan->task_config->dat_file_path);
   playlist_set_scan_database_name(manual_scan->playlist,
         manual_scan->task_config->database_name);
   playlist_set_scan_search_recursively(manual_scan->playlist,
         manual_scan->task_config->search_recursively);
   playlist_set_scan_search_archives(manual_scan->playlist,
         manual_scan->task_config->search_archives);
   playlist_set_scan_filter_dat_content(manual_scan->playlist,
         manual_scan->task_config->filter_dat_content);
   playlist_set_scan_overwrite_playlist(manual_scan->playlist,
         manual_scan->task_config->overwrite_playlist);
   playlist_set_scan_db_usage(manual_scan->playlist,
         manual_scan->task_config->db_usage);
   playlist_set_scan_omit_db_ref(manual_scan->playlist,
         manual_scan->task_config->omit_db_reference);

   /* All good - can start iterating
    * > If playlist has content and 'validate
    *   entries' is enabled, go to clean-up phase
    * > Otherwise go straight to content scan phase */
   if (manual_scan->task_config->validate_entries &&
       (manual_scan->playlist_size > 0))
      manual_scan->status = MANUAL_SCAN_ITERATE_CLEAN;
   else
   {
#ifdef HAVE_LIBRETRODB
      if (manual_scan->task_config->db_usage == MANUAL_CONTENT_SCAN_USE_DB_LOOSE ||
          manual_scan->task_config->db_usage == MANUAL_CONTENT_SCAN_USE_DB_STRICT)
         manual_scan->status = DATABASE_SCAN_ITERATE_START;
      else
#endif
         manual_scan->status = MANUAL_SCAN_ITERATE_CONTENT;
   }
   return false;
}

/* One gather's worth of BEGIN-family work.  Opens a share of the
 * per-frame window once, then runs sub-states back to back while they
 * complete inside it - so a small library still finishes the whole
 * BEGIN sequence (and often the whole scan setup) in a single tick.
 * Returns true when the task must finish. */
static bool task_manual_scan_begin_tick(retro_task_t *task,
      manual_scan_handle_t *manual_scan)
{
   nbio_budget_t b;
   bool error = false;

   task_nbio_slice_open(&b);

   for (;;)
   {
      enum manual_scan_status entry_status = manual_scan->status;

      switch (manual_scan->status)
      {
         case MANUAL_SCAN_BEGIN:
            error = manual_scan_begin_setup(task, manual_scan);
            break;
         case MANUAL_SCAN_BEGIN_DIR_LIST:
            error = manual_scan_begin_dir_list(task, manual_scan, &b);
            break;
         case MANUAL_SCAN_BEGIN_DAT_LOAD:
            error = manual_scan_begin_dat_load(task, manual_scan, &b);
            break;
         case MANUAL_SCAN_BEGIN_PLAYLIST:
            error = manual_scan_begin_playlist(task, manual_scan);
            break;
         default:
            /* Left the BEGIN family: done for this gather. */
            task_nbio_slice_close(&b);
            return false;
      }

      if (error)
         break;
      /* A sub-state that kept its status yielded on the window. */
      if (manual_scan->status == entry_status)
         break;
      /* Collapse into the next sub-state only while budget lasts. */
      if (!task_nbio_slice_within_budget(&b, 0, 0))
         break;
   }

   task_nbio_slice_close(&b);
   return error;
}

static void task_manual_content_scan_handler(retro_task_t *task)
{
   uint8_t flg;
   manual_scan_handle_t *manual_scan = NULL;
#ifdef HAVE_LIBRETRODB
   database_info_handle_t  *dbinfo   = NULL;
   database_state_handle_t *dbstate  = NULL;
#endif

   if (!task)
      goto task_finished;

   if (!(manual_scan = (manual_scan_handle_t*)task->state))
      goto task_finished;

#ifdef HAVE_LIBRETRODB
   dbinfo  = manual_scan->handle;
   dbstate = &manual_scan->state;
#endif

   flg = task_get_flags(task);

   if ((flg & RETRO_TASK_FLG_CANCELLED) > 0)
      goto task_finished;

#ifdef DEBUG
   RARCH_DBG("[Scanner] Task handler started, status %d\n",manual_scan->status);
#endif

/* Improvements / shortcomings:
- default extension list could be the core supported list, instead
- pushing "scan" twice on the same file in the file browser will freeze, if there is still task feedback widget on screen (?)
    this also happened before rework, prob menu related
- test with desktop menu
*/

   switch (manual_scan->status)
   {
      case MANUAL_SCAN_BEGIN:
      case MANUAL_SCAN_BEGIN_DIR_LIST:
      case MANUAL_SCAN_BEGIN_DAT_LOAD:
      case MANUAL_SCAN_BEGIN_PLAYLIST:
         if (task_manual_scan_begin_tick(task, manual_scan))
            goto task_finished;
#ifdef HAVE_LIBRETRODB
         /* The BEGIN family may have created the database handle
          * this invocation; refresh the locals the ITERATE cases
          * of later ticks are fetched from at handler entry. */
         dbinfo  = manual_scan->handle;
         dbstate = &manual_scan->state;
#endif
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

               if (   _len < sizeof(task_title)
                   && (entry->path && *entry->path)
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
            {
#ifdef HAVE_LIBRETRODB
               if (manual_scan->task_config->db_usage == MANUAL_CONTENT_SCAN_USE_DB_LOOSE ||
                   manual_scan->task_config->db_usage == MANUAL_CONTENT_SCAN_USE_DB_STRICT)
                  manual_scan->status = DATABASE_SCAN_ITERATE_START;
               else
#endif
                  manual_scan->status = MANUAL_SCAN_ITERATE_CONTENT;
               }
         }
         break;

#ifdef HAVE_LIBRETRODB
      /* Start of main loop. Each file in the list will be checked against all databases, 
         unless some scan configuration restricts this.
         This first stage sets up the necessary iterators. */
      case DATABASE_SCAN_ITERATE_START:
         {
            const char *content_path = manual_scan->content_list->elems[
                  manual_scan->content_list_index].data;

            /* Check if this is an M3U file and add to list for post-processing */
            if (rm3u_is_m3u_filestream(content_path))
            {
               union string_list_elem_attr attr;
               attr.i = 0;
               if (manual_scan->m3u_list)
                  string_list_append(manual_scan->m3u_list, content_path, attr);
            }
            task_database_cleanup_state(dbstate);
            dbstate->list_index  = 0;
            dbstate->entry_index = 0;
            task_database_iterate_start(task, dbinfo, content_path);
            manual_scan->status = DATABASE_SCAN_ITERATE_CONTENT;
            dbinfo->type = DATABASE_TYPE_ITERATE;
         }
         break;
         
      /* Content match iteration, reusing earlier autoscan code. */
      case DATABASE_SCAN_ITERATE_CONTENT:
         {
            bool path_contains_compressed_file = false;
            const char *content_path = manual_scan->content_list->elems[
                  manual_scan->content_list_index].data;
            enum scan_verdict current_verdict;
            if (!content_path)
               goto task_finished;

            path_contains_compressed_file      = path_contains_compressed_file(content_path);
            /* Reminder - remove this shortcut when serial scan inside zip is solved */
            if (path_contains_compressed_file)
               if (dbinfo->type == DATABASE_TYPE_ITERATE)
                  dbinfo->type   = DATABASE_TYPE_ITERATE_ARCHIVE;

            current_verdict = (enum scan_verdict)task_database_iterate(manual_scan, content_path, dbstate, dbinfo,
                     path_contains_compressed_file);
#ifdef DEBUG
            RARCH_DBG("[Scanner] Scan verdict is %d for %s\n", current_verdict, content_path);
#endif
            switch (current_verdict)
            {
               case SCAN_VERDICT_MATCHED_DB:
                  manual_scan->status = DATABASE_SCAN_ITERATE_NEXT;
                  break;
               case SCAN_VERDICT_NO_DB_MATCH:
                  if (manual_scan->task_config->db_usage == MANUAL_CONTENT_SCAN_USE_DB_LOOSE)
                     manual_scan->status = MANUAL_SCAN_ITERATE_CONTENT;
                  else
                  {
                     manual_scan->status = DATABASE_SCAN_ITERATE_NEXT;
                  }
                  break;
               case SCAN_VERDICT_ARCHIVE_CONTENTS_ADDED:
                     manual_scan->status = DATABASE_SCAN_ITERATE_NEXT;
                  break;
               case SCAN_VERDICT_ERROR:
                  RARCH_ERR("[Scanner] Scanning of content unexpectedly failed for \"%s\"\n", content_path);
                  /* fall through */
               case SCAN_VERDICT_CONTINUE:
                  break;
            }
         }
         break;
      case DATABASE_SCAN_ITERATE_NEXT:
         /* skip any pruned entries */
         increase_content_list_index(manual_scan);

         if (manual_scan->content_list_index < manual_scan->content_list->size)
         {
            dbinfo->status = DATABASE_STATUS_ITERATE_START;
            manual_scan->status = DATABASE_SCAN_ITERATE_START;
            dbinfo->type   = DATABASE_TYPE_ITERATE;
         }
         else
         {
            manual_scan->status = MANUAL_SCAN_ITERATE_CONTENT;
         }
         break;
#endif
      case MANUAL_SCAN_ITERATE_CONTENT:
         {
            const char *content_path = manual_scan->content_list->elems[
                  manual_scan->content_list_index].data;
            int content_type         = manual_scan->content_list->elems[
                  manual_scan->content_list_index].attr.i;

            if (content_path && *content_path)
            {
               size_t _len;
               char task_title[128];
               const char *content_file = path_basename(content_path);
               /* todo: prob not here? */
               char label[NAME_MAX_LENGTH];
               char playlist_content_path[PATH_MAX_LENGTH];

               /* Update progress display */
               task_free_title(task);

               _len = strlcpy(task_title,
                     msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_IN_PROGRESS),
                     sizeof(task_title));

               if (_len < sizeof(task_title) && content_file && *content_file)
                  strlcpy(task_title       + _len,
                        content_file,
                        sizeof(task_title) - _len);

               task_set_title(task, strdup(task_title));
               task_set_progress(task,
                     (manual_scan->content_list_index * 100) /
                     manual_scan->content_list->size);

               /* If "search archives" is enabled, but compressed files are not in the list,    *
                * do not add the compressed file itself, just add the contents to the end.      *
                * This can also conflict with DAT scanning which looks for zip files typically, *
                * so it is restricted for the full-manual scan case. DB match does its own      *
                * archive addition. */
               if (manual_scan->task_config->search_archives &&
                   path_is_compressed_file(content_path) && 
                   ((*manual_scan->task_config->file_exts
                   && string_find_index_substring_string(manual_scan->task_config->file_exts,path_get_extension(content_file)) < 0)
                   || manual_scan->task_config->db_usage == MANUAL_CONTENT_SCAN_USE_DB_NONE))
               {
                  add_files_from_archive(manual_scan,content_path);
               }
               else
               {
                  /* Add content to playlist */
                  /* Get 'actual' content path */
                  playlist_content_path[0]='\0';
                  if (!manual_content_scan_get_playlist_content_path(
                        manual_scan->task_config, content_path, content_type,
                        playlist_content_path, sizeof(playlist_content_path)))
                  {
                     RARCH_WARN("[Scanner] Could not add manual scan result %s\n", content_path);
                     playlist_content_path[0]='\0';
                  }
                  else
                  {
                     /* Get entry label */
                     const char *db_name = 
                        *manual_scan->task_config->database_name
                        ? manual_scan->task_config->database_name
                        : manual_scan->task_config->dat_file_path;
                     label[0] = '\0';
                     if (!manual_content_scan_get_playlist_content_label(
                           content_path, manual_scan->dat_file,
                           manual_scan->task_config->filter_dat_content,
                           label, sizeof(label)))
                     {
                        label[0] = '\0';
#ifdef DEBUG
                        RARCH_DBG("[Scanner] Rejecting item: %s\n",content_path);
#endif
                     }
                     else
                     {
                        RARCH_DBG("[Scanner] Adding item: %s\n",content_path);
                        scan_results_add(&manual_scan->scan_results,
                                         content_path, label,
                                         (char*)"00000000|crc",
                                         db_name, "");
                     }
                  }
               }
               /* If this is an M3U file, add it to the
                * M3U list for later processing */
               if (rm3u_is_m3u_filestream(content_path))
               {
                  union string_list_elem_attr attr;
                  attr.i = 0;
                  /* NOTE: If string_list_append() fails, there is
                   * really nothing we can do. The M3U file will
                   * just be ignored... */
                  string_list_append(
                        manual_scan->m3u_list, content_path, attr);
               }
            }

            /* Increment content index, move to the end if finished */
            increase_content_list_index(manual_scan);
            if (manual_scan->content_list_index >=
                  manual_scan->content_list->size)
            {
               /* Check whether we have any M3U files
                * to process */
               if (manual_scan->m3u_list->size > 0)
                  manual_scan->status = MANUAL_SCAN_ITERATE_M3U;
               else
                  manual_scan->status = MANUAL_SCAN_END;
            }
            else
#ifdef HAVE_LIBRETRODB
               if (manual_scan->task_config->db_usage == MANUAL_CONTENT_SCAN_USE_DB_LOOSE ||
                   manual_scan->task_config->db_usage == MANUAL_CONTENT_SCAN_USE_DB_STRICT)
                  manual_scan->status = DATABASE_SCAN_ITERATE_START;
               else
#endif
                  manual_scan->status = MANUAL_SCAN_ITERATE_CONTENT;
         }
         break;
      case MANUAL_SCAN_ITERATE_M3U:
         {
            const char *m3u_path = manual_scan->m3u_list->elems[
                  manual_scan->m3u_index].data;

            if (m3u_path && *m3u_path)
            {
               size_t _len;
               char task_title[128];
               const char *m3u_name = path_basename_nocompression(m3u_path);

               /* Update progress display */
               task_free_title(task);

               _len = strlcpy(task_title,
                     msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_M3U_CLEANUP),
                     sizeof(task_title));

               if (_len < sizeof(task_title) && m3u_name && *m3u_name)
                  strlcpy(task_title       + _len,
                        m3u_name,
                        sizeof(task_title) - _len);

               task_set_title(task, strdup(task_title));
               task_set_progress(task, (manual_scan->m3u_index * 100) /
                     manual_scan->m3u_list->size);

               task_database_iterate_m3u(manual_scan, m3u_path);
            }

            /* Increment M3U file index */
            manual_scan->m3u_index++;
            if (manual_scan->m3u_index >= manual_scan->m3u_list->size)
               manual_scan->status = MANUAL_SCAN_END;
         }
         break;
      case MANUAL_SCAN_END:
         {
            const char *msg = NULL;

            /* Batch update all playlists with accumulated results,
             * spread across gathers under the shared window. */
            if (manual_scan->scan_results.count > 0)
            {
               if (!manual_scan_end_flush_tick(manual_scan,
                     manual_scan->task_config->target_is_single_determined_playlist))
                  break;   /* more results next gather */
            }
            /* If no results, still write an empty playlist, if it is specified. */
            else if (manual_scan->task_config->target_is_single_determined_playlist)
               playlist_write_file(manual_scan->playlist);

            /* Update progress display */
#ifdef HAVE_LIBRETRODB
            if (dbstate && dbstate->list && dbstate->list->size == 0 &&
                (manual_scan->task_config->db_usage == MANUAL_CONTENT_SCAN_USE_DB_LOOSE ||
                 manual_scan->task_config->db_usage == MANUAL_CONTENT_SCAN_USE_DB_STRICT))
            {
               msg = msg_hash_to_str(MSG_SCANNING_NO_DATABASE);
               task_set_error(task, strdup(msg));
            }
            else 
            {
               if (manual_scan->flags & DB_HANDLE_FLAG_IS_DIRECTORY)
                  msg = msg_hash_to_str(MSG_SCANNING_OF_DIRECTORY_FINISHED);
               else
                  msg = msg_hash_to_str(MSG_SCANNING_OF_FILE_FINISHED);
            }
#else
            msg = msg_hash_to_str(MSG_SCANNING_OF_DIRECTORY_FINISHED);
#endif
            task_free_title(task);
            task_set_title(task, strdup(msg));
            task_set_progress(task, 100);
            ui_companion_driver_notify_refresh();
            RARCH_LOG("[Scanner] %s\n", msg);
            if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_DATABASE_SCAN, NULL))
               printf("%s\n", msg);

            RARCH_DBG("[Scanner] Scan settings were:\n");
            RARCH_DBG("[Scanner]    Content dir:       \"%s\"\n",manual_scan->task_config->content_dir);
            RARCH_DBG("[Scanner]    Database name:     \"%s\"\n",manual_scan->task_config->database_name);
            RARCH_DBG("[Scanner]    DAT / fallback db: \"%s\"\n",manual_scan->task_config->dat_file_path);
            RARCH_DBG("[Scanner]    Target playlist:   \"%s\"\n",manual_scan->task_config->playlist_file);
            RARCH_DBG("[Scanner]    Core name:         \"%s\"\n",manual_scan->task_config->core_name);
            RARCH_DBG("[Scanner]    File ext %s \"%s\"\n",
                      manual_scan->task_config->file_exts_custom_set ? "(custom):" : "(auto):  ",
                      manual_scan->task_config->file_exts);
            RARCH_DBG("[Scanner]    DB usage, DB selection: %d / %d\n",manual_scan->task_config->db_usage,manual_scan->task_config->db_selection);
            RARCH_DBG("[Scanner]    Recursive, archives, single target: %s / %s / %s\n",
                      manual_scan->task_config->search_recursively  ? "yes" : "no",
                      manual_scan->task_config->search_archives     ? "yes" : "no",
                      manual_scan->task_config->target_is_single_determined_playlist ? "yes" : "no");
            RARCH_DBG("[Scanner]    Overwrite, validate, omit DB reference: %s / %s / %s\n",
                      manual_scan->task_config->overwrite_playlist ? "yes" : "no", 
                      manual_scan->task_config->validate_entries   ? "yes" : "no",
                      manual_scan->task_config->omit_db_reference  ? "yes" : "no");
         }
         /* fall-through */
      default:
         task_set_progress(task, 100);
         goto task_finished;
   }

   return;

task_finished:
#ifdef DEBUG
   RARCH_DBG("[Scanner] Task finished\n");
#endif
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
      bool do_menu_refresh,
      retro_task_callback_t user_cb)
{
   size_t _len;
   task_finder_data_t find_data;
   char task_title[128];
   retro_task_t *task                = NULL;
   manual_scan_handle_t *manual_scan = NULL;
   settings_t *settings              = config_get_ptr();
   const char *playlist_dir          = settings->paths.directory_playlist;

   /* Sanity check */
   if (!playlist_dir || !*playlist_dir)
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
   manual_scan->content_list_index  = 0;
   manual_scan->status              = MANUAL_SCAN_BEGIN;
   manual_scan->m3u_index           = 0;
   manual_scan->m3u_list            = string_list_new();

   if (!manual_scan->m3u_list)
      goto error;

   manual_scan->playlist_config.capacity            = COLLECTION_SIZE;
   manual_scan->playlist_config.old_format          = settings->bools.playlist_use_old_format;
   manual_scan->playlist_config.compress            = settings->bools.playlist_compression;
   manual_scan->playlist_config.fuzzy_archive_match = settings->bools.playlist_fuzzy_archive_match;
   playlist_config_set_base_content_directory(&manual_scan->playlist_config, settings->bools.playlist_portable_paths ? settings->paths.directory_menu_content : NULL);

#ifdef HAVE_LIBRETRODB

   if (settings->bools.scan_without_core_match)
      manual_scan->flags |= DB_HANDLE_FLAG_SCAN_WITHOUT_CORE_MATCH;

   if (settings->bools.show_hidden_files)
      manual_scan->flags |= DB_HANDLE_FLAG_SHOW_HIDDEN_FILES;

   if (do_menu_refresh)
      manual_scan->flags |= DB_HANDLE_FLAG_DO_MENU_REFRESH;

   manual_scan->content_database_path               = strdup(settings->paths.path_content_database);
#endif
   manual_scan->playlist_directory                  = strdup(playlist_dir);

   /* > Get current manual content scan configuration */
   if (!(manual_scan->task_config = (manual_content_scan_task_config_t*)
         calloc(1, sizeof(manual_content_scan_task_config_t))))
      goto error;

   if (!manual_content_scan_get_task_config(
         manual_scan->task_config, playlist_dir))
   {
      const char *_msg = msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_INVALID_CONFIG);
      RARCH_ERR("[Scanner] Invalid scan config\n");
      runloop_msg_queue_push(_msg, strlen(_msg), 1, 100, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      goto error;
   }

#ifdef HAVE_LIBRETRODB
   if (manual_scan->task_config->db_selection == MANUAL_CONTENT_SCAN_SELECT_DB_AUTO_FIRST_MATCH)
      manual_scan->flags |= DB_HANDLE_FLAG_USE_FIRST_MATCH_ONLY;

   if (path_is_directory(manual_scan->task_config->content_dir))
      manual_scan->flags |= DB_HANDLE_FLAG_IS_DIRECTORY;
#endif 

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

   /* > Get task title
    *
    * strlcpy() returns the length of its source, so _len can exceed
    * the buffer when the (translated) message does not fit.  The
    * append would then form an out-of-bounds pointer and pass a
    * wrapped size_t as the bound.  The shipped strings leave a wide
    * margin - the longest of these four is 26 bytes into 128 - so
    * this is hardening rather than a live overflow, but it is the
    * same misuse of the return value that overflowed the serial
    * query buffer, and translations come from Crowdin. */
   _len = strlcpy(
         task_title, msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_START),
         sizeof(task_title));
   if (_len < sizeof(task_title))
      strlcpy(task_title       + _len,
            manual_scan->task_config->system_name,
            sizeof(task_title) - _len);

   /* > Configure task */
   task->handler                 = task_manual_content_scan_handler;
   task->state                   = manual_scan;
   task->title                   = strdup(task_title);
   task->progress                = 0;
#ifdef HAVE_LIBRETRODB
   task->progress_cb             = task_window_progress_cb;
#else
   task->progress_cb             = NULL;
#endif

   manual_scan->user_cb          = user_cb;
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
   RARCH_ERR("[Scanner] Task creation failed\n");

   return false;
}

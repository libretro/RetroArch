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
#ifdef HAVE_THREADS
#include <retro_atomic.h>
#include <features/features_cpu.h>
#endif
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
    * whatever the answer was.  A separate flag, because
    * "min_sizes[i] == 0" cannot carry it: zero is what an unqueried
    * slot holds AND what a database whose smallest record is
    * zero-sized legitimately produces, and keying the probe off it
    * re-queries such a database for every content file, at two full
    * walks a time. */
   DB_STATE_FLAG_SIZE_CHECKED             = (1 << 4),
   /* A core claiming this database matches archive members. */
   DB_STATE_FLAG_ARCHIVE_MEMBER           = (1 << 5),
   /* Its crc index build was refused.  The budget only shrinks, so a
    * retry would walk the database again to the same answer. */
   DB_STATE_FLAG_NO_CRC_INDEX             = (1 << 6)
};

#ifdef HAVE_LIBRETRODB
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
#endif

/* Bytes a stepped CRC reads between two looks at the window. */
#ifndef SCAN_CRC_QUANTUM
#define SCAN_CRC_QUANTUM (256 * 1024)
#endif

/* Records an index build walks, and entries it merges, between two
 * looks at the window. */
#ifndef SCAN_INDEX_RECORDS
#define SCAN_INDEX_RECORDS 1024
#endif
#ifndef SCAN_INDEX_ENTRIES
#define SCAN_INDEX_ENTRIES 32768
#endif

/* Archive members whose records an index lookup reads, or held hits
 * written as entries, between two looks at the window. */
#ifndef SCAN_MEMBER_HITS
#define SCAN_MEMBER_HITS 64
#endif

/* An archive's directory, read once: each member's stored CRC (0:
 * none stored), size and name, in directory order.  Directories are
 * left out.  A first member with no stored CRC (a zstd stream, a
 * BLAKE2 RAR 5 member) is decoded for one, as the member pass looks it
 * up; a .bz2 is decoded to be listed at all.  The scan notes in a
 * member the record its CRC hit. */
typedef struct scan_member
{
   uint64_t size;
   size_t   name;      /* in names */
   size_t   hit_db;    /* the database, when hit */
   char    *hit_name;  /* the record's name, or NULL */
   uint32_t crc;
   uint32_t hit_crc;   /* the record's crc */
   bool     hit;
   bool     hit_arc;   /* the record's rom_name names this archive */
} scan_member_t;

typedef struct scan_members
{
   scan_member_t *list;
   char          *names;
   size_t         count;
   uint64_t       first_size;
   uint64_t       file_size;  /* the archive's, 0: unknown */
   uint32_t       first_crc;  /* the first member's */
   char           first_ext[16];
   bool           torrentzip; /* a zip TorrentZip wrote */
} scan_members_t;

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
   /* One entry per database in 'list', allocated to list->size:
    * list_index is bounded only by list->size - the number of .rdb
    * files in the database directory - so a fixed
    * [MAX_DATABASE_COUNT] array would need a clamp, and a database
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
   /* The extensions of the cores claiming each database, resolved
    * from core info when the scan starts, or NULL when no core
    * claims it. The per-file test is then one lookup in that list. */
   struct string_list **claim_exts;
   /* Bytes of index the scan may still allocate.  Indexes are held
    * for the whole scan, so without a ceiling a large database set
    * costs tens of megabytes.  See task_database_index_budget(). */
   size_t index_budget;
   /* A CRC hashed a few quanta per step by task_database_crc_step():
    * the open span or NULL, the bytes left (-1: to the end) and the
    * running value.  crc_done says crc (or, for DB_CRC_DEST_ARCHIVE,
    * archive_crc) holds this file's value, which may be 0. */
   intfstream_t *crc_fd;
   int64_t crc_left;
   uint32_t crc_acc;
   uint8_t crc_dest;  /* enum db_crc_dest */
   uint8_t crc_fail;  /* enum db_crc_fail */
   bool crc_done;
   /* The index build in flight, run by task_database_index_step()
    * until index_ready, then taken by the lookup that began it. */
   database_info_index_job_t *index_job;
   /* The archive's directory, for each of its passes. */
   scan_members_t *members;
   /* The member the every-member pass and the archive's entries go on
    * from in the next step, and the scan's extensions, split once. */
   size_t member_next;
   struct string_list *member_exts;
   /* The lookup's pass, and whether archive_crc holds the archive's
    * own CRC or its read is under way. */
   uint8_t crc_pass;  /* enum db_crc_pass */
   bool whole_done;
   /* The member pass hit where only the first member's extension is
    * claimed: every member is looked up next, unless one claimed for
    * the archive's own name matches too. */
   bool member_hit;
   /* The archive's own CRC was asked for, so a whole read is used. */
   bool whole_asked;
   bool index_ready;
} database_state_handle_t;

/* Where a stepped CRC and its size go. */
enum db_crc_dest
{
   DB_CRC_DEST_FILE = 0,
   DB_CRC_DEST_ARCHIVE
};

/* What a failed read of a stepped CRC does. */
enum db_crc_fail
{
   DB_CRC_FAIL_ERROR = 0,  /* the file errors out */
   DB_CRC_FAIL_ZERO        /* the CRC is 0, so no entry matches */
};

/* What the CRC lookup compares.  An archive of one member is looked up
 * by its member's CRC; if that misses, it is read whole only when a
 * database may hold it by its size.  An archive of several members is
 * read whole only when a database may hold it by its size, and each
 * database that opens its type takes either CRC.  Where neither
 * matches, every member's stored CRC is looked up. */
enum db_crc_pass
{
   DB_CRC_PASS_FILE = 0,  /* crc: any other name */
   DB_CRC_PASS_MEMBER,    /* crc, the first member's */
   DB_CRC_PASS_EVERY,     /* each member's stored crc */
   DB_CRC_PASS_SIZE,      /* the archive's size, by archive records */
   DB_CRC_PASS_WHOLE,     /* archive_crc */
   DB_CRC_PASS_BOTH       /* crc, or archive_crc once read */
};

/* None on Wii U: its newlib and libfat take no locks, and without
 * libfat its SD layer serialises every call.  None on Vita: Vita3K's
 * sceIo tables take no lock. */
#if defined(HAVE_LIBRETRODB) && defined(HAVE_THREADS) \
 && defined(RETRO_ATOMIC_LOCK_FREE) && !defined(WIIU) && !defined(VITA)
#define SCAN_HAVE_READERS
#endif

/* Reader tasks: min(SCAN_READERS_MAX, cores), none below 2, on the
 * threaded queue's parallel workers.  The scan hands them up to
 * SCAN_READER_DEPTH files each ahead of it and parks at most
 * SCAN_READER_PARK_USEC on one still being read; an idle reader parks
 * as long. */
#ifndef SCAN_READERS_MAX
#define SCAN_READERS_MAX 4
#endif
#ifndef SCAN_READER_DEPTH
#define SCAN_READER_DEPTH 64
#endif
#ifndef SCAN_READER_PARK_USEC
#define SCAN_READER_PARK_USEC 100000
#endif
/* Files at or under this size are read by the readers. */
#ifndef SCAN_READER_CUTOFF
#define SCAN_READER_CUTOFF (128 * 1024 * 1024)
#endif
/* Archives the readers may read whole ahead of the scan at most.  Each
 * whole match in a row grants 1, 2, 4 ... more. */
#ifndef SCAN_READER_AHEAD_MAX
#define SCAN_READER_AHEAD_MAX 16
#endif

#ifdef SCAN_HAVE_READERS
enum scan_slot_state
{
   SCAN_SLOT_EMPTY = 0,
   SCAN_SLOT_READY,       /* for a reader to claim */
   SCAN_SLOT_IN_FLIGHT,
   SCAN_SLOT_OK,
   SCAN_SLOT_MEMBER_OK,   /* an archive's directory, not read whole */
   SCAN_SLOT_BIG,
   SCAN_SLOT_FAIL
};

/* A file handed out.  The scan writes it while EMPTY, its reader while
 * IN_FLIGHT, and each publishes with the state.  For an archive the
 * reader reads the directory first, into members, which the scan
 * takes with the file or frees. */
typedef struct scan_slot
{
   uint64_t           size;
   size_t             index;   /* in the content list */
   char              *path;    /* PATH_MAX_LENGTH, its own copy */
   scan_members_t    *members;
   uint32_t           crc;
   retro_atomic_int_t state;   /* enum scan_slot_state */
   bool               archive;
} scan_slot_t;

/* Shared by the scan and its readers; the last reference frees it.
 * Fields marked scan are the scan's alone. */
typedef struct scan_readers
{
   scan_slot_t       *win;      /* by fill order % window */
   char              *paths;
   uint32_t          *idents;   /* the readers' */
   size_t             window;
   size_t             fill;     /* scan: next list index to look at */
   unsigned           seq;      /* scan: slots filled */
   unsigned           take;     /* scan: slots given back */
   unsigned           count;    /* scan: readers pushed */
   unsigned           grant;    /* scan: the last whole match's, else 0 */
   uint32_t           ident;    /* the scan's */
   retro_atomic_int_t filled;   /* seq, for the readers */
   retro_atomic_int_t claim;    /* the next slot to claim */
   retro_atomic_int_t idle;     /* readers parked or parking */
   retro_atomic_int_t live;     /* readers not yet gone */
   retro_atomic_int_t quit;
   retro_atomic_int_t refs;
   /* Archives the readers may still read whole ahead, beyond their
    * directories: granted when an archive matches by its own CRC,
    * taken back when one ends otherwise.  A hint: each slot's state
    * says what it holds. */
   retro_atomic_int_t whole_ahead;
   retro_atomic_int_t rearm;    /* the next slot to take back */
   bool               off;      /* scan: the readers are gone or going */
   /* Fixed before the readers start: some database opens zip, so a
    * TorrentZip set's own CRC may be asked. */
   bool               tz_free;
   /* While tz_free: the scan asked the last TorrentZip set's own CRC,
    * so the readers read the next ones whole. */
   retro_atomic_int_t tz_whole;
} scan_readers_t;

/* A reader task's own. */
typedef struct scan_reader
{
   scan_readers_t *blk;
   scan_slot_t    *slot;   /* the file in hand */
   scan_slot_t    *own;    /* its last MEMBER_OK */
   intfstream_t   *fd;
   int64_t         left;
   uint32_t        acc;
   bool            parked;
   bool            gone;   /* left: counted out of live */
} scan_reader_t;

enum scan_take
{
   SCAN_TAKE_NONE = 0,   /* the task reads the file itself */
   SCAN_TAKE_DONE,
   SCAN_TAKE_WAIT
};
#endif

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
#ifdef SCAN_HAVE_READERS
   scan_readers_t *readers;
#endif
   database_state_handle_t state;
   uint8_t flags;
#ifdef SCAN_HAVE_READERS
   bool readers_tried;
#endif
#endif
   /* The caller's completion callback, run after the task's own -
    * this is how a task_push_dbscan caller learns the scan finished;
    * see cb_task_manual_content_scan. */
   retro_task_callback_t user_cb;
} manual_scan_handle_t;

enum scan_verdict
{
   SCAN_VERDICT_CONTINUE = 0,
   SCAN_VERDICT_MATCHED_DB,
   SCAN_VERDICT_ARCHIVE_CONTENTS_ADDED,
   SCAN_VERDICT_NO_DB_MATCH,
   SCAN_VERDICT_ERROR,
   /* A reader has the file: park until its result is in. */
   SCAN_VERDICT_WAIT
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
      size_t _len     = strlcpy_lit(string, " ", sizeof(string));
      _len += strlcpy(string + _len, prefix, sizeof(string) - _len);
      _len += strlcpy_lit(string + _len, " ",    sizeof(string) - _len);
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
      _len    += strlcpy_lit(string + _len, " ",    sizeof(string) - _len);
      _len    += strlcpy(string + _len, prefix, sizeof(string) - _len);
      _len    += strlcpy_lit(string + _len, " ",    sizeof(string) - _len);
      strlcpy(string + _len, reset,  sizeof(string) - _len);
      fputs(string, stdout);
   }
#endif
   else
   {
      size_t _len     = strlcpy_lit(string, " ", sizeof(string));
      _len += strlcpy(string + _len, prefix, sizeof(string) - _len);
      strlcpy_lit(string + _len, " ", sizeof(string) - _len);
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
      if (manual_scan->task_config->cli_scan_output)
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
      /* A sheet naming itself would free the path being scanned. */
      if (string_is_equal(path, name))
         continue;
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
    * makes them six characters rather than seven.  The skip must
    * match the prefix: a hard-coded seven lands "(Tape 1)" on the
    * ')' so the indicator comes out empty,
    * is_valid_disc_indicator() rejects it, and no tape or side
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
               {
                  char disc_name[NAME_MAX_LENGTH];
                  fill_pathname(disc_name,
                        path_basename_nocompression(result->entry_path),
                        "", sizeof(disc_name));

                  /* A label that is only the disc's own file name
                   * (no database or DAT title) says nothing the M3U's
                   * name doesn't - and the user chose the M3U's name,
                   * which thumbnails and saves already follow. Keep
                   * it, as before the M3U collapse existed. A real
                   * title has its disc indicator stripped instead. */
                  if (string_is_equal(result->entry_label, disc_name))
                     fill_pathname(collapsed_title,
                           path_basename_nocompression(m3u_path),
                           "", sizeof(collapsed_title));
                  else
                  {
                     strlcpy(collapsed_title, result->entry_label,
                           sizeof(collapsed_title));
                     remove_disc_indicators(collapsed_title,
                           sizeof(collapsed_title));
                  }
               }
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
      /* A sheet naming itself would free the path being scanned. */
      if (string_is_equal(path, name))
         continue;
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
         || string_is_equal(ext_lower, "bz2")
         || string_is_equal(ext_lower, "rar")
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
   if (
            string_is_equal(ext_lower, "3ds")
         || string_is_equal(ext_lower, "cci")
         || string_is_equal(ext_lower, "cxi")
      )
      return FILE_TYPE_3DS;
   return FILE_TYPE_NONE;
}

static void task_database_members_free(scan_members_t *m)
{
   if (!m)
      return;
   {
      size_t i;
      for (i = 0; i < m->count; i++)
         free(m->list[i].hit_name);
   }
   free(m->list);
   free(m->names);
   free(m);
}

#ifdef HAVE_COMPRESSION
/* The walk's transfer and userdata hold two paths each: too large for
 * a reader's frame. */
typedef struct scan_members_walk
{
   file_archive_transfer_t         state;
   struct archive_extract_userdata userdata;
} scan_members_walk_t;
#endif

/* @path's directory; NULL when it cannot be read through. */
static scan_members_t *task_database_members_read(const char *path)
{
#ifdef HAVE_COMPRESSION
   scan_members_walk_t *w         = (scan_members_walk_t*)calloc(1, sizeof(*w));
   scan_members_t      *m         = (scan_members_t*)calloc(1, sizeof(*m));
   size_t               cap       = 0;
   size_t               used      = 0;
   size_t               names_cap = 0;
   bool                 first     = true;
   bool                 ok        = w && m;
   bool                 returnerr = false;

   if (ok)
   {
      w->state.type = ARCHIVE_TRANSFER_INIT;
      file_archive_parse_file_iterate(&w->state, &returnerr, path, NULL,
            NULL, &w->userdata);
      ok = (w->state.type == ARCHIVE_TRANSFER_ITERATE);
      if (ok && w->state.archive_size > 0)
         m->file_size = (uint64_t)w->state.archive_size;
      if (ok)
         m->torrentzip = w->state.torrentzip;
   }
   while (ok)
   {
      size_t      step = w->state.step_current;
      const char *name = w->userdata.current_file_path;
      size_t      len;

      w->userdata.crc                  = 0;
      w->userdata.size                 = 0;
      w->userdata.current_file_path[0] = '\0';
      /* A CRC the archive does not store is decoded for the first
       * member alone. */
      w->userdata.crc_wanted           = first;
      file_archive_parse_file_iterate(&w->state, &returnerr, path, NULL,
            NULL, &w->userdata);
      if (w->state.step_current == step)
      {
         ok = (w->state.type != ARCHIVE_TRANSFER_DEINIT_ERROR);
         break;
      }
      len = strlen(name);
      /* rar and 7z leave a folder unnamed, and rar a member it cannot
       * unpack. */
      if (!len || name[len - 1] == '/' || name[len - 1] == '\\')
         continue;
      if (first)
      {
         m->first_crc  = w->userdata.crc;
         m->first_size = w->userdata.size;
         strlcpy(m->first_ext, path_get_extension(name),
               sizeof(m->first_ext));
         first         = false;
      }
      if (m->count == cap)
      {
         size_t         ncap = cap ? cap * 2 : 64;
         scan_member_t *tmp  = (scan_member_t*)realloc(m->list,
               ncap * sizeof(*tmp));
         if (!tmp)
         {
            ok = false;
            break;
         }
         m->list = tmp;
         cap     = ncap;
      }
      if (used + len + 1 > names_cap)
      {
         size_t ncap = names_cap ? names_cap * 2 : 1024;
         char  *tmp;
         while (ncap < used + len + 1)
            ncap *= 2;
         if (!(tmp = (char*)realloc(m->names, ncap)))
         {
            ok = false;
            break;
         }
         m->names  = tmp;
         names_cap = ncap;
      }
      memcpy(m->names + used, name, len + 1);
      m->list[m->count].size = w->userdata.size;
      m->list[m->count].name = used;
      m->list[m->count].crc  = w->userdata.crc;
      m->list[m->count].hit_db   = 0;
      m->list[m->count].hit_name = NULL;
      m->list[m->count].hit_crc  = 0;
      m->list[m->count].hit      = false;
      m->list[m->count].hit_arc  = false;
      m->count++;
      used += len + 1;
   }
   if (w)
      file_archive_parse_file_iterate_stop(&w->state);
   free(w);
   if (!ok)
   {
      task_database_members_free(m);
      return NULL;
   }
   return m;
#else
   (void)path;
   return NULL;
#endif
}

static void task_database_crc_close(database_state_handle_t *db_state)
{
   if (db_state->crc_fd)
   {
      intfstream_close(db_state->crc_fd);
      free(db_state->crc_fd);
      db_state->crc_fd = NULL;
   }
   db_state->crc_done = false;
}

/* No serial, CRC, size or pass of one file is left for the next. */
static void task_database_crc_reset(database_state_handle_t *db_state)
{
   task_database_crc_close(db_state);
   db_state->serial[0]    = '\0';
   db_state->crc          = 0;
   db_state->archive_crc  = 0;
   db_state->size         = 0;
   db_state->archive_size = 0;
   db_state->crc_pass     = DB_CRC_PASS_FILE;
   db_state->whole_done   = false;
   db_state->member_hit   = false;
   db_state->whole_asked  = false;
   task_database_members_free(db_state->members);
   db_state->members      = NULL;
   db_state->member_next  = 0;
}

/* @m, which may be NULL, is the file's directory: its first member's
 * stored CRC and size are the member pass's. */
static void task_database_members_install(database_state_handle_t *db_state,
      scan_members_t *m)
{
   task_database_members_free(db_state->members);
   db_state->members     = m;
   db_state->member_next = 0;
   db_state->crc         = m ? m->first_crc  : 0;
   db_state->size        = m ? m->first_size : 0;
}

/* Opens @name's CRC span for task_database_crc_step() to hash over
 * the steps that follow, and records its size. */
static bool task_database_crc_begin(database_state_handle_t *db_state,
      const char *name, enum task_database_crc_kind kind,
      enum db_crc_dest dest, enum db_crc_fail fail)
{
   uint64_t size = 0;

   task_database_crc_close(db_state);
   if (!(db_state->crc_fd = task_database_crc_open(name, kind,
         &db_state->crc_left, &size)))
      return false;
   db_state->crc_acc  = 0;
   db_state->crc_dest = (uint8_t)dest;
   db_state->crc_fail = (uint8_t)fail;
   if (dest == DB_CRC_DEST_ARCHIVE)
      db_state->archive_size = size;
   else
      db_state->size         = size;
   return true;
}

/* Hashes up to one quantum of @fd's span.  -1 on a failed read (a span
 * that ends early is one), 1 at the end of the span, else 0. */
static int task_database_crc_quantum(intfstream_t *fd, int64_t *left,
      uint32_t *acc)
{
   size_t  want = SCAN_CRC_QUANTUM;
   int64_t got  = 0;

   if (*left >= 0 && *left < (int64_t)want)
      want = (size_t)*left;
   if (want)
      got = intfstream_crc_step(fd, acc, want);
   if (got < 0 || (got == 0 && *left > 0))
      return -1;
   if (*left > 0)
      *left -= got;
   return (got == 0 || *left == 0) ? 1 : 0;
}

/* One quantum always, then more while the window lasts.  At the end
 * of the span the lookup runs on the next step. */
static enum scan_verdict task_database_crc_step(
      database_state_handle_t *db_state, nbio_budget_t *b)
{
   for (;;)
   {
      int rv = task_database_crc_quantum(db_state->crc_fd,
            &db_state->crc_left, &db_state->crc_acc);

      if (rv < 0)
      {
         task_database_crc_close(db_state);
         if (db_state->crc_fail == DB_CRC_FAIL_ERROR)
            return SCAN_VERDICT_ERROR;
         db_state->crc      = 0;
         db_state->crc_done = true;
         return SCAN_VERDICT_CONTINUE;
      }
      if (rv > 0)
         break;
      if (!task_nbio_slice_within_budget(b, 0, 0))
         return SCAN_VERDICT_CONTINUE;
   }

   if (db_state->crc_dest == DB_CRC_DEST_ARCHIVE)
      db_state->archive_crc = db_state->crc_acc;
   else
      db_state->crc         = db_state->crc_acc;
   task_database_crc_close(db_state);
   db_state->crc_done = true;
   return SCAN_VERDICT_CONTINUE;
}

#ifdef SCAN_HAVE_READERS
static void scan_readers_unref(scan_readers_t *blk)
{
   size_t i;

   if (retro_atomic_fetch_sub_int(&blk->refs, 1) != 1)
      return;
   if (blk->win)
      for (i = 0; i < blk->window; i++)
         task_database_members_free(blk->win[i].members);
   free(blk->idents);
   free(blk->paths);
   free(blk->win);
   free(blk);
}

static void scan_readers_wake(scan_readers_t *blk)
{
   unsigned r;

   for (r = 0; r < blk->count; r++)
      task_queue_wake(blk->idents[r]);
}

/* No I/O: a plain path, or with an SMB client an smb:// one (its VFS
 * pools connections), that the default or the archive branch would
 * CRC whole. */
static bool scan_reader_eligible(const char *path)
{
   if (   !path
#ifdef HAVE_SMBCLIENT
       || (   strstr(path, "://")
           && !string_starts_with_size(path, "smb://",
                 STRLEN_CONST("smb://")))
#else
       || strstr(path, "://")
#endif
       || strlen(path) >= PATH_MAX_LENGTH
       || path_contains_compressed_file(path))
      return false;
   switch (extension_to_file_type(path_get_extension(path)))
   {
      case FILE_TYPE_NONE:
#ifdef HAVE_COMPRESSION
      case FILE_TYPE_COMPRESSED:
#endif
         return true;
      default:
         break;
   }
   return false;
}

/* One of the whole reads ahead the scan granted, if any are left. */
static bool scan_reader_credit(scan_readers_t *blk)
{
   int n;

   while ((n = retro_atomic_load_acquire_int(&blk->whole_ahead)) > 0)
      if (retro_atomic_cas_int(&blk->whole_ahead, n, n - 1))
         return true;
   return false;
}

/* While whole reads ahead are granted, a slot that holds only an
 * archive's directory, taken back to read whole: the next from the
 * scan's on, in order, then this reader's last (it may have missed the
 * grant). */
static bool scan_reader_rearm(scan_reader_t *rd)
{
   scan_readers_t *blk = rd->blk;

   while (retro_atomic_load_acquire_int(&blk->whole_ahead) > 0)
   {
      scan_slot_t *slot = rd->own;
      int          k    = retro_atomic_load_acquire_int(&blk->rearm);

      if (k < retro_atomic_load_acquire_int(&blk->claim))
      {
         if (!retro_atomic_cas_int(&blk->rearm, k, k + 1))
            continue;
         slot = &blk->win[(size_t)k % blk->window];
      }
      else if (!slot)
         return false;
      else
         rd->own = NULL;
      if (!retro_atomic_cas_int(&slot->state, SCAN_SLOT_MEMBER_OK,
            SCAN_SLOT_IN_FLIGHT))
         continue;
      /* The slot first, then a credit: with none left it goes back. */
      if (scan_reader_credit(blk))
      {
         rd->slot = slot;
         return true;
      }
      retro_atomic_store_release_int(&slot->state, SCAN_SLOT_MEMBER_OK);
      task_queue_wake(blk->ident);
   }
   return false;
}

/* The next filled slot, in order; one the scan took back is passed
 * over.  One to take back comes first.  False when none is filled. */
static bool scan_reader_claim(scan_reader_t *rd)
{
   scan_readers_t *blk = rd->blk;

   if (scan_reader_rearm(rd))
      return true;
   for (;;)
   {
      int          c = retro_atomic_load_acquire_int(&blk->claim);
      scan_slot_t *slot;

      if (c >= retro_atomic_load_seq_cst_int(&blk->filled))
         return false;
      if (!retro_atomic_cas_int(&blk->claim, c, c + 1))
         continue;
      slot = &blk->win[(size_t)c % blk->window];
      if (retro_atomic_cas_int(&slot->state, SCAN_SLOT_READY,
            SCAN_SLOT_IN_FLIGHT))
      {
         rd->slot = slot;
         return true;
      }
   }
}

/* Wakes the scan, which may be parked on this file. */
static void scan_reader_publish(scan_reader_t *rd, int state)
{
   if (rd->fd)
   {
      intfstream_close(rd->fd);
      free(rd->fd);
      rd->fd = NULL;
   }
   if (state == SCAN_SLOT_OK)
      rd->slot->crc = rd->acc;
   retro_atomic_store_release_int(&rd->slot->state, state);
   rd->slot = NULL;
   task_queue_wake(rd->blk->ident);
}

/* A file in hand goes back as FAIL, for the scan to read. */
static void scan_reader_leave(scan_reader_t *rd)
{
   scan_readers_t *blk = rd->blk;

   if (rd->slot)
      scan_reader_publish(rd, SCAN_SLOT_FAIL);
   if (rd->parked)
      retro_atomic_fetch_sub_int(&blk->idle, 1);
   retro_atomic_fetch_add_seq_cst_int(&blk->live, -1);
   /* With none left, the scan reads what was filled. */
   task_queue_wake(blk->ident);
   rd->gone = true;
}

/* Where every reader drops its share: at its retirement, or on the
 * worker a queue deinit left it behind on, once its read returns. */
static void scan_reader_cleanup(retro_task_t *task)
{
   scan_reader_t *rd = (scan_reader_t*)task->state;

   if (!rd->gone)
      scan_reader_leave(rd);
   scan_readers_unref(rd->blk);
   free(rd);
   task->state = NULL;
}

/* One call: a claim and the open, or one quantum of the file in hand.
 * Leaves on quit, on cancel and off the threaded queue (whose
 * unthreaded runner would run it on the main thread). */
static void scan_reader_handler(retro_task_t *task)
{
   scan_reader_t  *rd  = (scan_reader_t*)task->state;
   scan_readers_t *blk = rd->blk;
   int             rv;

   if (rd->parked)
   {
      rd->parked = false;
      retro_atomic_fetch_sub_int(&blk->idle, 1);
   }
   if (   retro_atomic_load_acquire_int(&blk->quit)
       || !task_queue_is_threaded()
       || (task_get_flags(task) & RETRO_TASK_FLG_CANCELLED))
      goto leave;
   if (!rd->slot)
   {
      uint64_t size = 0;

      if (!scan_reader_claim(rd))
      {
         /* seq_cst both ways with the scan's fill: it sees this reader
          * idle and wakes it, or this sees the fill. */
         retro_atomic_fetch_add_seq_cst_int(&blk->idle, 1);
         rd->parked = true;
         if (!scan_reader_claim(rd))
         {
            task_queue_park(task, SCAN_READER_PARK_USEC);
            return;
         }
         rd->parked = false;
         retro_atomic_fetch_sub_int(&blk->idle, 1);
      }
      /* One taken back has its directory. */
      if (rd->slot->archive && !rd->slot->members)
      {
         scan_members_t *m = task_database_members_read(rd->slot->path);

         rd->slot->members = m;
         /* Over the cutoff the scan reads it if it must: no grant is
          * spent on it. */
         if (m && m->file_size > SCAN_READER_CUTOFF)
         {
            rd->slot->size = m->file_size;
            scan_reader_publish(rd, SCAN_SLOT_BIG);
            return;
         }
         /* Only the directory unless a whole read is granted, or it is
          * a TorrentZip set of several members while a database opens
          * zip: the scan reads it whole if its passes ask for that. */
         if (   m
             && !(   m->torrentzip && m->count > 1
                  && retro_atomic_load_acquire_int(&blk->tz_whole))
             && !scan_reader_credit(blk))
         {
            rd->own = rd->slot;
            scan_reader_publish(rd, SCAN_SLOT_MEMBER_OK);
            return;
         }
      }
      if (!(rd->fd = task_database_crc_open(rd->slot->path,
            TASK_DATABASE_CRC_FILE, &rd->left, &size)))
      {
         scan_reader_publish(rd, SCAN_SLOT_FAIL);
         return;
      }
      rd->slot->size = size;
      rd->acc        = 0;
      if (size > SCAN_READER_CUTOFF)
      {
         scan_reader_publish(rd, SCAN_SLOT_BIG);
         return;
      }
   }
   if ((rv = task_database_crc_quantum(rd->fd, &rd->left, &rd->acc)))
      scan_reader_publish(rd, rv > 0 ? SCAN_SLOT_OK : SCAN_SLOT_FAIL);
   return;
leave:
   scan_reader_leave(rd);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

/* Some database opens zip: a TorrentZip set of several members is then
 * read whole with no grant, as its own CRC may be asked. */
static bool scan_readers_tz_free(manual_scan_handle_t *manual_scan)
{
   database_state_handle_t *st = &manual_scan->state;
   size_t                   i;

   if (!st->list || !st->claim_exts)
      return false;
   for (i = 0; i < st->list->size; i++)
      if (   (manual_scan->flags & DB_HANDLE_FLAG_SCAN_WITHOUT_CORE_MATCH)
          || (   string_list_find_elem(st->claim_exts[i], "zip")
              && !(st->flags[i] & DB_STATE_FLAG_ARCHIVE_MEMBER)))
         return true;
   return false;
}

/* Readers pushed are kept when a later push fails.  Tried once per
 * scan. */
static void scan_readers_start(retro_task_t *task,
      manual_scan_handle_t *manual_scan)
{
   scan_readers_t *blk;
   size_t          i;
   unsigned        n = cpu_features_get_core_amount();
   unsigned        r;

   manual_scan->readers_tried = true;
   if (n > SCAN_READERS_MAX)
      n = SCAN_READERS_MAX;
   if (n < 2 || !task_queue_is_threaded())
      return;
   if (!(blk = (scan_readers_t*)calloc(1, sizeof(*blk))))
      return;
   retro_atomic_int_init(&blk->refs, 1);
   blk->window = n * SCAN_READER_DEPTH;
   if (   !(blk->win    = (scan_slot_t*)calloc(blk->window,
            sizeof(*blk->win)))
       || !(blk->paths  = (char*)malloc(blk->window * PATH_MAX_LENGTH))
       || !(blk->idents = (uint32_t*)calloc(n, sizeof(*blk->idents))))
   {
      scan_readers_unref(blk);
      return;
   }
   for (i = 0; i < blk->window; i++)
   {
      blk->win[i].path = blk->paths + i * PATH_MAX_LENGTH;
      retro_atomic_int_init(&blk->win[i].state, SCAN_SLOT_EMPTY);
   }
   blk->ident = task->ident;
   blk->fill  = manual_scan->content_list_index;
   blk->tz_free = scan_readers_tz_free(manual_scan);
   retro_atomic_int_init(&blk->tz_whole, blk->tz_free);
   retro_atomic_int_init(&blk->filled, 0);
   retro_atomic_int_init(&blk->claim,  0);
   retro_atomic_int_init(&blk->idle,   0);
   retro_atomic_int_init(&blk->quit,   0);
   retro_atomic_int_init(&blk->whole_ahead, 0);
   retro_atomic_int_init(&blk->rearm,  0);
   retro_atomic_int_init(&blk->live,   (int)n);
   retro_atomic_int_init(&blk->refs,   (int)n + 1);
   for (r = 0; r < n; r++)
   {
      retro_task_t  *rt = NULL;
      scan_reader_t *rd = (scan_reader_t*)calloc(1, sizeof(*rd));

      if (!rd || !(rt = task_init()))
      {
         free(rd);
         break;
      }
      rd->blk              = blk;
      rt->handler          = scan_reader_handler;
      rt->cleanup          = scan_reader_cleanup;
      rt->state            = rd;
      rt->parallel_workers = n;
      task_set_flags(rt, RETRO_TASK_FLG_PARALLEL
            | RETRO_TASK_FLG_DETACHABLE | RETRO_TASK_FLG_MUTE, true);
      blk->idents[r]       = rt->ident;
      /* A refused task is still ours. */
      if (!task_queue_push(rt))
      {
         free(rd);
         free(rt);
         break;
      }
      blk->count++;
   }
   for (; r < n; r++)
   {
      retro_atomic_fetch_add_int(&blk->live, -1);
      scan_readers_unref(blk);
   }
   if (!blk->count)
   {
      scan_readers_unref(blk);
      return;
   }
   manual_scan->readers = blk;
}

/* The scan lets go: each reader leaves at its next call.  Idempotent. */
static void scan_readers_stop(manual_scan_handle_t *manual_scan)
{
   scan_readers_t *blk = manual_scan->readers;

   if (!blk)
      return;
   manual_scan->readers = NULL;
   retro_atomic_store_release_int(&blk->quit, 1);
   scan_readers_wake(blk);
   scan_readers_unref(blk);
}

/* Free slots take the eligible files of the window ahead, in list
 * order.  None while the scan is on a cue or gdi: those sort first
 * (database_info_dir_init_from_list()), and each prunes its tracks
 * when the scan reaches it. */
static void scan_readers_fill(manual_scan_handle_t *manual_scan)
{
   scan_readers_t     *blk   = manual_scan->readers;
   struct string_list *list  = manual_scan->content_list;
   int                 added = 0;
   size_t              end;

   if (!blk || blk->off)
      return;
   if (   !task_queue_is_threaded()
       || retro_atomic_load_seq_cst_int(&blk->live) <= 0)
   {
      blk->off = true;
      return;
   }
   if (manual_scan->content_list_index < list->size)
      switch (extension_to_file_type(path_get_extension(
            list->elems[manual_scan->content_list_index].data)))
      {
         case FILE_TYPE_CUE:
         case FILE_TYPE_GDI:
            return;
         default:
            break;
      }
   end = manual_scan->content_list_index + blk->window;
   if (end > list->size)
      end = list->size;
   if (blk->fill < manual_scan->content_list_index)
      blk->fill = manual_scan->content_list_index;
   while (blk->fill < end && blk->seq - blk->take < blk->window)
   {
      const char *path = list->elems[blk->fill].data;

      if (scan_reader_eligible(path))
      {
         scan_slot_t *slot = &blk->win[blk->seq % blk->window];

         slot->index      = blk->fill;
         slot->size       = 0;
         slot->crc        = 0;
         slot->archive    = extension_to_file_type(path_get_extension(path))
               == FILE_TYPE_COMPRESSED;
         /* A copy: the list moves and is pruned under the readers. */
         strlcpy(slot->path, path, PATH_MAX_LENGTH);
         retro_atomic_store_release_int(&slot->state, SCAN_SLOT_READY);
         blk->seq++;
         added++;
      }
      blk->fill++;
   }
   if (!added)
      return;
   /* seq_cst: see scan_reader_handler(). */
   retro_atomic_fetch_add_seq_cst_int(&blk->filled, added);
   if (retro_atomic_load_seq_cst_int(&blk->idle) > 0)
      scan_readers_wake(blk);
}

/* File content_list_index's result, at its own place only: an OK one
 * leaves the state a finished stepped CRC leaves.  The slots up to it
 * go back for refilling; a READY one is the readers' until they are
 * off. */
static enum scan_take scan_readers_take(manual_scan_handle_t *manual_scan,
      database_state_handle_t *db_state, database_info_handle_t *db,
      const char *name)
{
   scan_readers_t *blk = manual_scan->readers;
   size_t          i   = manual_scan->content_list_index;

   if (!blk)
      return SCAN_TAKE_NONE;
   while (blk->take != blk->seq)
   {
      scan_slot_t *slot = &blk->win[blk->take % blk->window];
      int          state;

      if (slot->index > i)
         break;
      state = retro_atomic_load_acquire_int(&slot->state);
      if (state == SCAN_SLOT_IN_FLIGHT)
         return SCAN_TAKE_WAIT;
      if (state == SCAN_SLOT_READY)
      {
         if (   !blk->off
             || !retro_atomic_cas_int(&slot->state, SCAN_SLOT_READY,
                  SCAN_SLOT_EMPTY))
            return SCAN_TAKE_WAIT;
         state = SCAN_SLOT_EMPTY;
      }
      /* A reader may take it back to read whole. */
      if (   state == SCAN_SLOT_MEMBER_OK
          && !retro_atomic_cas_int(&slot->state, SCAN_SLOT_MEMBER_OK,
               SCAN_SLOT_EMPTY))
         return SCAN_TAKE_WAIT;
      blk->take++;
      if (   slot->index == i
          && (state == SCAN_SLOT_OK || state == SCAN_SLOT_MEMBER_OK))
      {
         if (slot->archive)
         {
            scan_members_t *m = slot->members;

            slot->members      = NULL;
            task_database_members_install(db_state, m);
            db_state->crc_pass = DB_CRC_PASS_MEMBER;
            if (state == SCAN_SLOT_OK)
            {
               db_state->archive_crc  = slot->crc;
               db_state->archive_size = slot->size;
               db_state->whole_done   = true;
               /* No directory: only its own can match. */
               if (!m)
                  db_state->crc_pass  = DB_CRC_PASS_WHOLE;
            }
         }
         else
         {
            db_state->serial[0] = '\0';
            db_state->crc       = slot->crc;
            db_state->size      = slot->size;
         }
         db_state->crc_done = true;
         db->type           = DATABASE_TYPE_CRC_LOOKUP;
         retro_atomic_store_release_int(&slot->state, SCAN_SLOT_EMPTY);
         return SCAN_TAKE_DONE;
      }
      /* The scan reads it itself: its directory is in. */
      if (slot->index == i && slot->archive && slot->members)
      {
         task_database_members_install(db_state, slot->members);
         slot->members = NULL;
      }
      task_database_members_free(slot->members);
      slot->members = NULL;
      retro_atomic_store_release_int(&slot->state, SCAN_SLOT_EMPTY);
      if (slot->index == i)
         break;
   }
   /* The task reads it: not a reader too. */
   if (blk->fill <= i)
      blk->fill = i + 1;
   return SCAN_TAKE_NONE;
}

/* Adds whole reads ahead, up to SCAN_READER_AHEAD_MAX.  With some
 * granted, readers also take back what they have only the member CRC
 * of, from the scan's next file on; any parked on a full window
 * wake. */
static void scan_readers_grant(scan_readers_t *blk, int add)
{
   int had, n;

   /* Readers only take: a count of 0 stays 0 until this adds. */
   do
   {
      had = retro_atomic_load_acquire_int(&blk->whole_ahead);
      if (had >= SCAN_READER_AHEAD_MAX)
         return;
      n   = had + add;
      if (n > SCAN_READER_AHEAD_MAX)
         n = SCAN_READER_AHEAD_MAX;
      /* Before the count: a reader that sees it sees the cursor. */
      if (!had)
         retro_atomic_store_release_int(&blk->rearm, (int)blk->take);
   } while (!retro_atomic_cas_int(&blk->whole_ahead, had, n));
   if (!had)
      scan_readers_wake(blk);
}

/* An archive matched whole grants the readers whole reads ahead,
 * twice the last match's, up to SCAN_READER_AHEAD_MAX; any other
 * archive verdict takes back what is left. */
static void scan_readers_whole_ahead(manual_scan_handle_t *manual_scan,
      int whole)
{
   scan_readers_t       *blk = manual_scan->readers;
   const scan_members_t *m   = manual_scan->state.members;

   if (!blk)
      return;
   /* A TorrentZip set its sizes kept from a whole read: the readers'
    * whole reads of the next ones would be dropped. */
   if (m && m->torrentzip && m->count > 1 && blk->tz_free)
      retro_atomic_store_release_int(&blk->tz_whole,
            manual_scan->state.whole_asked);
   if (!whole)
   {
      blk->grant = 0;
      if (retro_atomic_load_acquire_int(&blk->whole_ahead))
         retro_atomic_store_release_int(&blk->whole_ahead, 0);
      return;
   }
   blk->grant = blk->grant ? blk->grant * 2 : 1;
   if (blk->grant > SCAN_READER_AHEAD_MAX)
      blk->grant = SCAN_READER_AHEAD_MAX;
   scan_readers_grant(blk, (int)blk->grant);
}

/* While the scan walks a database in a member pass mid match streak,
 * the readers may read whole up to the cap.  The streak's grant is
 * left as it is. */
static void scan_readers_top_up(manual_scan_handle_t *manual_scan)
{
   scan_readers_t *blk = manual_scan->readers;

   if (blk && blk->grant)
      scan_readers_grant(blk, SCAN_READER_AHEAD_MAX);
}
#endif

static void task_database_index_close(database_state_handle_t *db_state)
{
   database_info_index_job_free(db_state->index_job);
   db_state->index_job   = NULL;
   db_state->index_ready = false;
}

/* Begins the current database's index build, or takes what it built.
 * False while it runs: the caller returns SCAN_VERDICT_CONTINUE, the
 * steps that follow run the build, and the caller comes back here for
 * it.  A build that cannot begin gives nothing, as a failed one. */
static bool task_database_index_take(database_state_handle_t *db_state,
      database_info_crc_index_t **crc,
      database_info_serial_index_t **serial, bool *has_serial)
{
   if (!db_state->index_job)
   {
      unsigned want = 0;

      if (crc)
         want |= DATABASE_INFO_INDEX_CRC;
      /* A database matched by members is never asked for its sizes,
       * and MAME's run to 100k records. */
      if (   crc
          && !(db_state->flags[db_state->list_index]
             & DB_STATE_FLAG_ARCHIVE_MEMBER))
         want |= DATABASE_INFO_INDEX_ARCHIVE_SIZES;
      if (serial)
         want |= DATABASE_INFO_INDEX_SERIAL;
      if (has_serial)
         want |= DATABASE_INFO_INDEX_HAS_SERIAL;
      if ((db_state->index_job = database_info_index_job_new(
            db_state->list->elems[db_state->list_index].data,
            db_state->index_budget, want)))
         return false;
   }
   database_info_index_job_finish(db_state->index_job, crc, serial,
         has_serial);
   db_state->index_job   = NULL;
   db_state->index_ready = false;
   return true;
}

/* One unit of the build always, then more while the window lasts. */
static enum scan_verdict task_database_index_step(
      database_state_handle_t *db_state, nbio_budget_t *b)
{
   while (!database_info_index_job_step(db_state->index_job,
            SCAN_INDEX_RECORDS, SCAN_INDEX_ENTRIES))
      if (!task_nbio_slice_within_budget(b, 0, 0))
         return SCAN_VERDICT_CONTINUE;
   db_state->index_ready = true;
   return SCAN_VERDICT_CONTINUE;
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
         db->type           = DATABASE_TYPE_CRC_LOOKUP;
         db_state->crc_pass = DB_CRC_PASS_MEMBER;
         break;
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
            return task_database_crc_begin(db_state, name,
                  TASK_DATABASE_CRC_CUE, DB_CRC_DEST_FILE, DB_CRC_FAIL_ERROR);
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
            return task_database_crc_begin(db_state, name,
                  TASK_DATABASE_CRC_GDI, DB_CRC_DEST_FILE, DB_CRC_FAIL_ERROR);
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
            return task_database_crc_begin(db_state, name,
                  TASK_DATABASE_CRC_CHD, DB_CRC_DEST_FILE, DB_CRC_FAIL_ERROR);
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
            return task_database_crc_begin(db_state, name,
                  TASK_DATABASE_CRC_FILE, DB_CRC_DEST_FILE, DB_CRC_FAIL_ERROR);
         }
         break;
      case FILE_TYPE_3DS:
         db_state->serial[0] = '\0';
         if (task_database_3ds_get_serial(name, db_state->serial, sizeof(db_state->serial),&db_state->size))
            db->type         = DATABASE_TYPE_SERIAL_LOOKUP;
         else
         {
            db->type         = DATABASE_TYPE_CRC_LOOKUP;
            db_state->serial[0] = '\0';
            RARCH_DBG("[Scanner] 3DS file serial not detected, fallback to crc.\n");
            return task_database_crc_begin(db_state, name,
                  TASK_DATABASE_CRC_FILE, DB_CRC_DEST_FILE, DB_CRC_FAIL_ERROR);
         }
         break;
      case FILE_TYPE_LUTRO:
         db->type            = DATABASE_TYPE_ITERATE_LUTRO;
         break;
      default:
         db_state->serial[0] = '\0';
         db->type            = DATABASE_TYPE_CRC_LOOKUP;
         return task_database_crc_begin(db_state, name,
               TASK_DATABASE_CRC_FILE, DB_CRC_DEST_FILE, DB_CRC_FAIL_ERROR);
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
/* A member stores no CRC: only the expansion, which unpacks it for
 * one, can look it up. */
static bool task_database_members_crc0(const scan_members_t *m)
{
   size_t k;

   for (k = 0; m && k < m->count; k++)
      if (m->list[k].size && !m->list[k].crc)
         return true;
   return false;
}

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
   if (_db->task_config->cli_scan_output)
      task_database_scan_console_output(path, NULL, false);

   /* With its directory read, an archive's held hits were its
    * expansion.  It still expands where a member stores no CRC, and
    * for a scan that keeps unmatched files. */
   if (   !path_contains_compressed_file && path_is_compressed_file(path)
       && _db->task_config->search_archives
       && (   !db_state->members
           || task_database_members_crc0(db_state->members)
           || _db->task_config->db_usage == MANUAL_CONTENT_SCAN_USE_DB_LOOSE))
   {
      archive_added=add_files_from_archive(_db, path);
   }
   else
      RARCH_LOG("[Scanner] No match for: \"%s\" (%s %08X).\n", path,
                db_state->serial, db_state->crc);

   db_state->list_index   = 0;
   db_state->entry_index  = 0;
   task_database_crc_reset(db_state);

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

/* Adds the entry for the record @rec_name, @rec_crc of database
 * list_index; @archive_name, when set, is the archive member it names.
 * False when the entry could not be built. */
static bool task_database_result_add(manual_scan_handle_t *_db,
      database_state_handle_t *db_state, const char *rec_name,
      uint32_t rec_crc, const char *archive_name)
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

   /* NULL-check both mallocs: the 'db_crc[0] = ...' /
    * 'entry_path_str[0] = ...' writes below NULL-deref on OOM.
    * free(NULL) in the teardown at the end of the function is
    * safe, so we can bail early; clean up whichever succeeded
    * and fail, so the scanner continues with the next content
    * entry rather than silently mismatching.
    *
    * db_path is also checked here: database_info_get_current_name()
    * returns NULL when the database handle/list is missing (and the
    * matched element's data may itself be NULL). A NULL db_path is
    * fed straight into path_basename_nocompression()/fill_pathname()
    * below, where strrchr()/strlcpy() dereference it and crash. With
    * no database name there is no meaningful playlist filename to
    * build, so treat it like the OOM case and skip this entry. */
   if (!db_crc || !entry_path_str || !db_path)
   {
      if (db_crc != db_crc_buf)
         free(db_crc);
      free(entry_path_str);
      return false;
   }

   db_crc[0]                      = '\0';
   entry_path_str[0]              = '\0';

   fill_pathname(db_playlist_base_str,
         path_basename_nocompression(db_path), ".lpl", sizeof(db_playlist_base_str));

   if (*db_state->serial)
   {
      size_t _len = strlcpy(db_crc, db_state->serial, db_crc_len);
      strlcpy_lit(db_crc  + _len,
            "|serial",
            db_crc_len - _len);
   }
   else
      snprintf(db_crc, db_crc_len, "%08lX|crc",
      (unsigned long)rec_crc);

   if (entry_path)
      strlcpy(entry_path_str, entry_path, str_len);

   /* Joined first: the multidisk test below reads the member too. */
   if (archive_name && *archive_name)
      fill_pathname_join_delim(entry_path_str,
            entry_path_str, archive_name, '#', str_len);

   /* Use database name for label if found,
    * otherwise use filename without extension */
   if (rec_name && *rec_name)
   {
      /* Use the archive as path instead of the file inside the archive
       * if the file is a multidisk game, because database entry
       * matches with the last disk, which is never bootable */
      char *delim = (char*)strchr(entry_path_str, '#');

      if (delim && compat_strcasestr(entry_path_str, " (Disk "))
         *delim = '\0';

      strlcpy(entry_lbl, rec_name, sizeof(entry_lbl));
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

   if (   (db_state->flags[db_state->list_index]
            & DB_STATE_FLAG_ARCHIVE_MEMBER)
       && (hash = strchr(entry_path_str, '#')))
       *hash = '\0';

   /* Accumulate result instead of immediately updating playlist */

   if (!scan_results_add(&_db->scan_results, entry_path_str, entry_lbl, db_crc, 
                         _db->task_config->omit_db_reference ? _db->task_config->dat_file_path : db_playlist_base_str, 
                         archive_name))
      RARCH_ERR("[Scanner] Failed to add result for: \"%s\".\n", entry_lbl);

   if (db_crc != db_crc_buf)
      free(db_crc);
   free(entry_path_str);
   return true;
}

/* The file has its entries: database list_index moves to the front,
 * where the next file is likely to match it too. */
static enum scan_verdict task_database_matched(manual_scan_handle_t *_db,
      database_state_handle_t *db_state)
{
#ifdef SCAN_HAVE_READERS
   if (db_state->crc_pass != DB_CRC_PASS_FILE)
      scan_readers_whole_ahead(_db,
            db_state->crc_pass == DB_CRC_PASS_WHOLE);
#endif
   database_info_list_free(db_state->info);
   free(db_state->info);

   db_state->info         = NULL;
   task_database_crc_reset(db_state);

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
         struct string_list           *ce =
            db_state->claim_exts[db_state->list_index];

         memmove(&db_state->crc_index[1],
                 &db_state->crc_index[0],
                 sizeof(ci) * db_state->list_index);
         memmove(&db_state->serial_index[1],
                 &db_state->serial_index[0],
                 sizeof(si) * db_state->list_index);
         memmove(&db_state->claim_exts[1],
                 &db_state->claim_exts[0],
                 sizeof(ce) * db_state->list_index);

         db_state->crc_index[0]    = ci;
         db_state->serial_index[0] = si;
         db_state->claim_exts[0]   = ce;
      }

      db_state->list->elems[0] = entry;
      db_state->min_sizes[0]   = min;
      db_state->max_sizes[0]   = max;
      db_state->flags[0]       = flag;
      db_state->flags[0]      |= DB_STATE_FLAG_MATCHED;
   }

   return SCAN_VERDICT_MATCHED_DB;
}

static enum scan_verdict database_info_list_iterate_found_match(
      manual_scan_handle_t *_db,
      database_state_handle_t *db_state,
      database_info_handle_t *db,
      const char *archive_name
      )
{
   /* Taking the matched entry as &info->list[entry_index]
    * unconditionally reads info and indexes list on nothing but the
    * caller's word.  Every caller does test both - each reaches this
    * function from inside an "info && entry_index < info->count" - so
    * this is the invariant being stated where it is relied on rather
    * than a reachable fault.  It is the same shape as the two the
    * callers were given after they were found dereferencing list[0]
    * on an empty result, and the only place left in the file taking
    * that address without a bound. */
   database_info_t *db_info_entry = (db_state->info
         && db_state->entry_index < db_state->info->count)
      ? &db_state->info->list[db_state->entry_index]
      : NULL;

   if (   !db_info_entry
       || !task_database_result_add(_db, db_state, db_info_entry->name,
            db_info_entry->crc32, archive_name))
      return SCAN_VERDICT_ERROR;
   return task_database_matched(_db, db_state);
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
   size_t i;
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
   db_state->claim_exts = (struct string_list**)
      calloc(count, sizeof(*db_state->claim_exts));
   db_state->index_budget = task_database_index_budget();

   if (   !db_state->min_sizes
       || !db_state->max_sizes
       || !db_state->flags
       || !db_state->crc_index
       || !db_state->serial_index
       || !db_state->claim_exts)
      return false;

   /* Which cores claim a database does not depend on the file being
    * scanned, so it is resolved once here for the whole scan. */
   for (i = 0; i < count; i++)
   {
      bool archive_member     = false;
      db_state->claim_exts[i] = core_info_database_claim(
            db_state->list->elems[i].data, &archive_member);
      if (archive_member)
         db_state->flags[i]  |= DB_STATE_FLAG_ARCHIVE_MEMBER;
   }

   return true;
}

/* False while the index walk it began runs: it is called again once
 * the walk is done. */
static bool task_database_fill_db_min_max(database_state_handle_t *db_state,
      database_info_serial_index_t **serial)
{
   const char *rdb        = db_state->list->elems[db_state->list_index].data;
   bool        has_serial = false;
   char query[50];
   query[0] = '\0';

   /* The crc index collects the size range while it walks, so build
    * it here and take the range from it: one walk instead of this
    * function's two, and the crc lookup then finds the index already
    * built.  The same walk sees whether any record carries a serial
    * and, given @serial, builds the serial index too, so the serial
    * lookup reads the database once. */
   if (   rdb && *rdb
       && !db_state->crc_index[db_state->list_index]
       && !(db_state->flags[db_state->list_index]
          & DB_STATE_FLAG_NO_CRC_INDEX)
       && db_state->index_budget)
   {
      database_info_crc_index_t *ci = NULL;

      if (!task_database_index_take(db_state, &ci, serial, &has_serial))
         return false;
      if (!ci)
         db_state->flags[db_state->list_index] |=
            DB_STATE_FLAG_NO_CRC_INDEX;
      else
      {
         size_t used = database_info_crc_index_bytes(ci);
         db_state->index_budget = (used < db_state->index_budget)
            ? db_state->index_budget - used : 0;
         db_state->crc_index[db_state->list_index] = ci;
      }
   }

   /* Marked once the walk is in, so that every exit below - including
    * the ones that record a placeholder range - counts as having been
    * asked.  This is what stops the walks repeating per content
    * file. */
   db_state->flags[db_state->list_index] |= DB_STATE_FLAG_SIZE_CHECKED;

   /* The queries below still run for a database the index cannot
    * cover. */
   {
      int64_t lo = 0;
      int64_t hi = 0;

      if (rdb && *rdb)
      {
         if (   db_state->crc_index[db_state->list_index]
             && database_info_crc_index_size_range(
                   db_state->crc_index[db_state->list_index], &lo, &hi))
         {
            db_state->min_sizes[db_state->list_index] = lo;
            db_state->max_sizes[db_state->list_index] = hi;
            db_state->flags[db_state->list_index]    |=
               DB_STATE_FLAG_HAS_SIZE;

            /* HAS_SERIAL gates a skip in the serial lookup.  HAS_CRC
             * is set for symmetry; nothing reads it beyond a debug
             * line. */
            db_state->flags[db_state->list_index] |= DB_STATE_FLAG_HAS_CRC;
            if (has_serial)
               db_state->flags[db_state->list_index] |=
                  DB_STATE_FLAG_HAS_SERIAL;

            db_state->entry_index = 0;
            return true;
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
      return true;
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
   return true;
}

/* An installed core claims the current database for @ext; one that
 * matches archive members, only for a member. */
static bool task_database_claimed(database_state_handle_t *db_state,
      const char *ext, bool member)
{
   return string_list_find_elem(
            db_state->claim_exts[db_state->list_index], ext)
      && (   member
          || !(db_state->flags[db_state->list_index]
             & DB_STATE_FLAG_ARCHIVE_MEMBER));
}

/* Passes over the run of databases, from the current one, that no
 * installed core claims @name for, nor @member_ext when given and the
 * database has or may still get a crc index: unindexed, a member
 * lookup is a whole walk the expansion repeats.  True when it passed
 * over any. */
static bool task_database_skip_unclaimed(database_state_handle_t *db_state,
      const char *name, const char *member_ext,
      bool path_contains_compressed_file)
{
   /* Every database the gate refuses is passed over in this one
    * step, so a file costs a step per database it can match rather
    * than one per database in the directory. */
   const char *ext = path_get_extension(name);
   bool skipped    = false;

   while (   db_state->list_index < db_state->list->size
          && !task_database_claimed(db_state, ext,
               path_contains_compressed_file)
          && !(   member_ext
               && task_database_claimed(db_state, member_ext, true)
               && (   db_state->crc_index[db_state->list_index]
                   || (   !(db_state->flags[db_state->list_index]
                            & DB_STATE_FLAG_NO_CRC_INDEX)
                       && db_state->index_budget))))
   {
      database_info_list_iterate_next(db_state);
      skipped = true;
   }

   return skipped;
}

/* The databases again from the first, for the archive's next pass. */
static enum scan_verdict task_database_pass_begin(
      database_state_handle_t *db_state, enum db_crc_pass pass)
{
   database_info_list_free(db_state->info);
   free(db_state->info);
   db_state->info        = NULL;
   db_state->list_index  = 0;
   db_state->entry_index = 0;
   db_state->member_next = 0;
   db_state->crc_pass    = (uint8_t)pass;
   return SCAN_VERDICT_CONTINUE;
}

/* The cores claiming database @i open @name's archive type: its own
 * extension passes the gate the member and whole passes ask. */
static bool task_database_opens_archive(manual_scan_handle_t *_db,
      database_state_handle_t *db_state, size_t i, const char *name)
{
   if (_db->flags & DB_HANDLE_FLAG_SCAN_WITHOUT_CORE_MATCH)
      return true;
   return string_list_find_elem(db_state->claim_exts[i],
            path_get_extension(name))
      && !(db_state->flags[i] & DB_STATE_FLAG_ARCHIVE_MEMBER);
}

/* Every member is looked up when the scan looks inside archives, as
 * its expansion would, and one stores a CRC.  A loose scan expands
 * the archive instead: its unmatched members are entries too. */
static bool task_database_every_wanted(manual_scan_handle_t *_db,
      const database_state_handle_t *db_state)
{
   size_t k;

   if (   !db_state->members || !_db->task_config->search_archives
       || _db->task_config->db_usage == MANUAL_CONTENT_SCAN_USE_DB_LOOSE)
      return false;
   for (k = 0; k < db_state->members->count; k++)
      if (db_state->members->list[k].crc)
         return true;
   return false;
}

/* Whether the expansion would look @mb up in database @i: in the
 * scan's extensions, and claimed by its own. */
static bool task_database_member_gate(manual_scan_handle_t *_db,
      database_state_handle_t *db_state, size_t i, const scan_member_t *mb)
{
   const char *ext = path_get_extension(
         db_state->members->names + mb->name);

   if (!mb->crc || mb->hit)
      return false;
   if (*_db->task_config->file_exts)
   {
      if (!db_state->member_exts)
         db_state->member_exts = string_split(
               _db->task_config->file_exts, "|");
      if (   db_state->member_exts
          && !string_list_find_elem_prefix(db_state->member_exts, ".", ext))
         return false;
   }
   return (_db->flags & DB_HANDLE_FLAG_SCAN_WITHOUT_CORE_MATCH)
      || string_list_find_elem(db_state->claim_exts[i], ext);
}

/* @rom_name names the archive @path: an archive name whose name less
 * its extension is @path's, in any case, so set.zip names set.7z.  A
 * set inside another archive (outer.zip#set.zip) names only itself. */
static bool task_database_names_archive(const char *rom_name,
      const char *path)
{
   char        stem[NAME_MAX_LENGTH];
   const char *base;
   size_t      len;

   if (!rom_name || !path || !path_is_compressed_file(rom_name))
      return false;
   base = path_basename_nocompression(rom_name);
   len  = (size_t)(path_get_extension(base) - base) - 1;
   if (len >= sizeof(stem))
      return false;
   strlcpy(stem, base, len + 1);
   base = path_basename_nocompression(path);
   return string_starts_with_case_insensitive(base, stem)
      && path_get_extension(base) == base + len + 1;
}

/* @mb's first record in @hits, held as database @i's; @name is the
 * archive. */
static void task_database_every_hold(scan_member_t *mb, size_t i,
      const database_info_list_t *hits, const char *name)
{
   size_t j;

   if (!hits)
      return;
   for (j = 0; j < hits->count; j++)
   {
      const database_info_t *rec = &hits->list[j];

      if (rec->crc32 != mb->crc)
         continue;
      mb->hit      = true;
      mb->hit_db   = i;
      mb->hit_crc  = rec->crc32;
      mb->hit_name = (rec->name && *rec->name) ? strdup(rec->name) : NULL;
      mb->hit_arc  = task_database_names_archive(rec->rom_name, name);
      return;
   }
}

/* One database's lookup of each member's stored CRC, as the expansion
 * looks a member up.  A member keeps the first database it hits. */
static enum scan_verdict task_database_iterate_every(
      manual_scan_handle_t *_db, database_state_handle_t *db_state,
      const char *name)
{
   scan_members_t *m   = db_state->members;
   size_t          i   = db_state->list_index;
   const char     *rdb = db_state->list->elems[i].data;
   size_t          k;
   unsigned        n   = 0;

   /* The cheap question first: a database no member can be looked up
    * in costs no index walk. */
   for (k = db_state->member_next; k < m->count; k++)
      if (task_database_member_gate(_db, db_state, i, &m->list[k]))
         break;
   if (k == m->count || !rdb || !*rdb)
   {
      db_state->member_next = 0;
      return database_info_list_iterate_next(db_state);
   }

   if (   !(db_state->flags[i] & DB_STATE_FLAG_SIZE_CHECKED)
       && !task_database_fill_db_min_max(db_state, NULL))
      return SCAN_VERDICT_CONTINUE;

   for (; k < m->count; k++)
   {
      scan_member_t        *mb    = &m->list[k];
      database_info_list_t *hits  = NULL;
      bool                  query = false;

      if (!task_database_member_gate(_db, db_state, i, mb))
         continue;
      if (   db_state->min_sizes[i] > 0
          && (   !mb->size
              || db_state->min_sizes[i] > (int64_t)mb->size
              || db_state->max_sizes[i] < (int64_t)mb->size))
         continue;
      if (db_state->crc_index[i])
         hits = database_info_list_new_crc(db_state->crc_index[i], rdb,
               mb->crc, 0, DB_EXTRACT_SCAN_FIELDS | DB_EXTRACT_ROM_NAME);
      if (!hits)
      {
         char q[50];
         snprintf(q, sizeof(q), "{crc:b\"%08lX\"}", (unsigned long)mb->crc);
         hits  = database_info_list_new_filtered(rdb, q,
               DB_EXTRACT_SCAN_FIELDS | DB_EXTRACT_ROM_NAME);
         query = true;
      }
      task_database_every_hold(mb, i, hits, name);
      if (hits)
      {
         if (hits->count)
            n++;
         database_info_list_free(hits);
         free(hits);
      }
      /* A query walks the database: one a step. */
      if (query || n == SCAN_MEMBER_HITS)
      {
         db_state->member_next = k + 1;
         return SCAN_VERDICT_CONTINUE;
      }
   }
   db_state->member_next = 0;
   return database_info_list_iterate_next(db_state);
}

/* Every member hit is the one record; its first member in @first. */
static bool task_database_every_one(const scan_members_t *m, size_t *first)
{
   size_t k;
   size_t f = m->count;

   for (k = 0; k < m->count; k++)
   {
      if (!m->list[k].hit)
         continue;
      if (f == m->count)
         f = k;
      else if (   m->list[k].hit_db  != m->list[f].hit_db
               || m->list[k].hit_crc != m->list[f].hit_crc)
         return false;
   }
   *first = f;
   return f < m->count;
}

/* The archive's passes are over: the members it held are its entries,
 * as its expansion would have written them. */
static enum scan_verdict task_database_archive_end(
      manual_scan_handle_t *_db, database_info_handle_t *db,
      database_state_handle_t *db_state, const char *name,
      bool path_contains_compressed_file)
{
   scan_members_t *m = db_state->members;
   size_t          len = strlen(name);
   size_t          first;
   size_t          k = 0;
   unsigned        n = 0;

   if (m)
      while (k < m->count && !m->list[k].hit)
         k++;
   if (!m || k == m->count)
      return database_info_list_iterate_end_no_match(_db, db, db_state,
            name, path_contains_compressed_file);

   first = m->list[k].hit_db;
   if (k < db_state->member_next)
      k = db_state->member_next;
   for (; k < m->count; k++)
   {
      const char *member = m->names + m->list[k].name;

      if (!m->list[k].hit)
         continue;
      /* The list's end brings the next step back here. */
      if (n == SCAN_MEMBER_HITS)
      {
         db_state->member_next = k;
         db_state->list_index  = db_state->list->size;
         return SCAN_VERDICT_CONTINUE;
      }
      /* Its archive#member path would be cut short.  A member
       * database's entry and a disc set's are the archive alone. */
      if (len + 1 + strlen(member) >= PATH_MAX_LENGTH)
      {
         if (   !(db_state->flags[m->list[k].hit_db]
                  & DB_STATE_FLAG_ARCHIVE_MEMBER)
             && !(   m->list[k].hit_name && *m->list[k].hit_name
                  && (   compat_strcasestr(name, " (Disk ")
                      || compat_strcasestr(member, " (Disk "))))
            continue;
         member = NULL;
      }
      db_state->list_index = m->list[k].hit_db;
      if (!task_database_result_add(_db, db_state, m->list[k].hit_name,
               m->list[k].hit_crc, member))
         return SCAN_VERDICT_ERROR;
      n++;
   }
   db_state->list_index = first;
   return task_database_matched(_db, db_state);
}

/* A database may hold the archive, or its directory cannot be read:
 * the databases again from the first, for the archive's own CRC.  The
 * steps that follow read the archive whole unless a reader has. */
static enum scan_verdict task_database_whole_pass(
      database_state_handle_t *db_state, const char *name)
{
   database_info_list_free(db_state->info);
   free(db_state->info);
   db_state->info        = NULL;
   db_state->list_index  = 0;
   db_state->entry_index = 0;
   db_state->crc_pass    = DB_CRC_PASS_WHOLE;
   db_state->whole_asked = true;
   if (   !db_state->whole_done
       && !task_database_crc_begin(db_state, name, TASK_DATABASE_CRC_FILE,
            DB_CRC_DEST_ARCHIVE, DB_CRC_FAIL_ERROR))
      return SCAN_VERDICT_ERROR;
   return SCAN_VERDICT_CONTINUE;
}

/* Several members: each database that opens the type is asked for the
 * first member's CRC and, when @whole, for the archive's own, which the
 * steps that follow read unless a reader has.  Without @whole a CRC a
 * reader read is dropped: the sizes decide, not the readers' timing. */
static enum scan_verdict task_database_both_pass(
      database_state_handle_t *db_state, const char *name, bool whole)
{
   task_database_pass_begin(db_state, DB_CRC_PASS_BOTH);
   if (!whole)
   {
      db_state->whole_done  = false;
      db_state->archive_crc = 0;
      return SCAN_VERDICT_CONTINUE;
   }
   db_state->whole_asked = true;
   if (   !db_state->whole_done
       && !task_database_crc_begin(db_state, name, TASK_DATABASE_CRC_FILE,
            DB_CRC_DEST_ARCHIVE, DB_CRC_FAIL_ERROR))
      return SCAN_VERDICT_ERROR;
   db_state->whole_done = true;
   return SCAN_VERDICT_CONTINUE;
}

/* The archive is read whole only for a database whose cores open its
 * type: no other is asked about its own CRC. */
static bool task_database_whole_wanted(manual_scan_handle_t *_db,
      database_state_handle_t *db_state, const char *name)
{
   size_t n = db_state->list->size;
   size_t i;

   if (   (_db->flags & DB_HANDLE_FLAG_USE_FIRST_MATCH_ONLY)
       && n && (db_state->flags[0] & DB_STATE_FLAG_MATCHED))
      n = 1;
   for (i = 0; i < n; i++)
      if (task_database_opens_archive(_db, db_state, i, name))
         return true;
   return false;
}

/* One database's say on reading the archive whole: one whose cores
 * open the type may hold it, unless the archive's size is outside its
 * size range, where the whole pass would skip it too, or its index
 * says that no record named for the type has that size.  The first
 * that may begins the whole pass. */
static enum scan_verdict task_database_iterate_size(
      manual_scan_handle_t *_db, database_state_handle_t *db_state,
      const char *name)
{
   size_t   i    = db_state->list_index;
   uint64_t size = db_state->members ? db_state->members->file_size : 0;

   if (!task_database_opens_archive(_db, db_state, i, name))
      return database_info_list_iterate_next(db_state);
   if (   !(db_state->flags[i] & DB_STATE_FLAG_SIZE_CHECKED)
       && !task_database_fill_db_min_max(db_state, NULL))
      return SCAN_VERDICT_CONTINUE;
   if (   size && db_state->min_sizes[i] > 0
       && (   db_state->min_sizes[i] > (int64_t)size
           || db_state->max_sizes[i] < (int64_t)size))
      return database_info_list_iterate_next(db_state);
   if (   size
       && !database_info_crc_index_may_hold_archive(
            db_state->crc_index[i], path_get_extension(name), size))
      return database_info_list_iterate_next(db_state);
   if (db_state->members && db_state->members->count > 1)
      return task_database_both_pass(db_state, name, true);
   return task_database_whole_pass(db_state, name);
}

/* The archive's next pass.  One member: a miss asks whether a database
 * may hold the archive by its size, which reads it whole; a held hit
 * goes straight to every member's.  Several members: the size pass,
 * then either CRC where the type is opened, the archive's own only if
 * the size let it be read.  Where none of that matches, every member's
 * stored CRC, whose lookups walk each database that claims a member's
 * type. */
static enum scan_verdict task_database_archive_next(
      manual_scan_handle_t *_db, database_state_handle_t *db_state,
      database_info_handle_t *db, const char *name,
      bool path_contains_compressed_file)
{
   size_t first;

   if (   db_state->crc_pass == DB_CRC_PASS_MEMBER
       && !db_state->member_hit
       && task_database_whole_wanted(_db, db_state, name))
      return task_database_pass_begin(db_state, DB_CRC_PASS_SIZE);
   /* No database holds it by its size: its first member's CRC alone. */
   if (   db_state->crc_pass == DB_CRC_PASS_SIZE
       && db_state->members && db_state->members->count > 1
       && db_state->crc)
      return task_database_both_pass(db_state, name, false);
   if (   db_state->crc_pass != DB_CRC_PASS_EVERY
       && task_database_every_wanted(_db, db_state))
      return task_database_pass_begin(db_state, DB_CRC_PASS_EVERY);
   /* One record, named for this archive, of a database whose cores
    * open it: the archive is its entry, as a first member's hit writes
    * it.  A disc track's record never is: Flycast lists zip but loads
    * every zip as a Naomi cart. */
   if (   db_state->crc_pass == DB_CRC_PASS_EVERY
       && task_database_every_one(db_state->members, &first)
       && db_state->members->list[first].hit_arc
       && task_database_opens_archive(_db, db_state,
            db_state->members->list[first].hit_db, name))
   {
      scan_member_t *mb = &db_state->members->list[first];

      db_state->list_index = mb->hit_db;
      if (!task_database_result_add(_db, db_state, mb->hit_name,
               mb->hit_crc, NULL))
         return SCAN_VERDICT_ERROR;
      return task_database_matched(_db, db_state);
   }
#ifdef SCAN_HAVE_READERS
   scan_readers_whole_ahead(_db, 0);
#endif
   return task_database_archive_end(_db, db, db_state, name,
         path_contains_compressed_file);
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
   {
      if (db_state->list && db_state->crc_pass != DB_CRC_PASS_FILE)
         return task_database_archive_next(_db, db_state, db, name,
               path_contains_compressed_file);
      return database_info_list_iterate_end_no_match(_db, db, db_state, name,
            path_contains_compressed_file);
   }

   if (db_state->crc_pass == DB_CRC_PASS_EVERY)
      return task_database_iterate_every(_db, db_state, name);
   if (db_state->crc_pass == DB_CRC_PASS_SIZE)
      return task_database_iterate_size(_db, db_state, name);

   /* The directory, read here unless a reader has. */
   if (db_state->crc_pass == DB_CRC_PASS_MEMBER)
   {
      if (!db_state->members)
      {
         scan_members_t *m = task_database_members_read(name);

         if (!m)
            return task_database_whole_pass(db_state, name);
         task_database_members_install(db_state, m);
      }
      /* Several members take no member pass, nor does a first member
       * with no CRC, or no member at all: nothing to look up. */
      if (!db_state->crc || db_state->members->count > 1)
         return task_database_archive_next(_db, db_state, db, name,
               path_contains_compressed_file);
   }

   /* Archive did not contain a CRC for this entry,
    * or the file is empty. */
   if (!db_state->crc && db_state->crc_pass == DB_CRC_PASS_FILE)
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

   /* The core-info gate is an in-memory string check; the size gate
    * below costs a full walk of the database the first time it is
    * consulted, because task_database_fill_db_min_max() builds the
    * crc index in that walk.  Running the walk first meant every
    * database in the directory was read end to end once per scan,
    * including the ones no installed core claims and which can never
    * match - 200 MB of .rdb reads to scan a 70 MB set with the
    * shipped database directory.  Ask the cheap question first.
    * The member pass asks it for the first member too, as its
    * expansion will, until one matches; not when nothing expands. */
   if (   !(_db->flags & DB_HANDLE_FLAG_SCAN_WITHOUT_CORE_MATCH)
       && task_database_skip_unclaimed(db_state, name,
            (   db_state->crc_pass == DB_CRC_PASS_MEMBER
             && !db_state->member_hit
             && _db->task_config->search_archives)
            ? db_state->members->first_ext : NULL,
            path_contains_compressed_file))
      return SCAN_VERDICT_CONTINUE;

   /* If size boundaries are not filled for this DB, run the queries */
   if (   !(db_state->flags[db_state->list_index] & DB_STATE_FLAG_SIZE_CHECKED)
       && !task_database_fill_db_min_max(db_state, NULL))
      return SCAN_VERDICT_CONTINUE;

   if (db_state->min_sizes[db_state->list_index] > 0)
   {
      int64_t  lo   = db_state->min_sizes[db_state->list_index];
      int64_t  hi   = db_state->max_sizes[db_state->list_index];
      uint64_t size = (db_state->crc_pass == DB_CRC_PASS_WHOLE)
         ? db_state->archive_size : db_state->size;

      if (   db_state->crc_pass == DB_CRC_PASS_BOTH && db_state->whole_done
          && db_state->archive_size >= (uint64_t)lo
          && db_state->archive_size <= (uint64_t)hi)
         size = db_state->archive_size;
      if (         size > 0
                && (    db_state->min_sizes[db_state->list_index] > (int64_t) size
                     || db_state->max_sizes[db_state->list_index] < (int64_t) size))
      {
#ifdef DEBUG
         RARCH_DBG("[Scanner] Skipping DB, file size %ld not in range.\n", size);
#endif
         return database_info_list_iterate_next(db_state);
      }
      else if (size == 0)
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
      uint32_t crc               = (db_state->crc_pass == DB_CRC_PASS_WHOLE)
         ? db_state->archive_crc : db_state->crc;

      query[0] = '\0';

      /* Answer from this database's crc index when we can.  Building
       * it costs one walk - about the price of a single unindexed
       * probe - and every later content file is then a binary search
       * instead
       * of another walk.  The index is only a faster route to the
       * same records: it reports them in file order with the same
       * fields extracted, so the matching below is unchanged.
       *
       * Anything the index cannot serve falls through to the query,
       * which stays the reference path. */
      if (rdb && *rdb)
      {
         if (   !db_state->crc_index[db_state->list_index]
             && !(db_state->flags[db_state->list_index]
                & DB_STATE_FLAG_NO_CRC_INDEX)
             && db_state->index_budget)
         {
            database_info_crc_index_t *ci = NULL;

            if (!task_database_index_take(db_state, &ci, NULL, NULL))
               return SCAN_VERDICT_CONTINUE;
            if (!ci)
               db_state->flags[db_state->list_index] |=
                  DB_STATE_FLAG_NO_CRC_INDEX;
            else
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
                  crc, (   db_state->crc_pass == DB_CRC_PASS_BOTH
                        && db_state->whole_done)
                  ? db_state->archive_crc : 0, DB_EXTRACT_SCAN_FIELDS);
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
      else if (   db_state->crc_pass == DB_CRC_PASS_BOTH
               && db_state->whole_done)
      {
         snprintf(query, sizeof(query),
               "{crc:or(b\"%08lX\",b\"%08lX\")}",
               (unsigned long)crc,
               (unsigned long)db_state->archive_crc);
         database_info_list_iterate_new(db_state, query);
      }
      else
      {
         snprintf(query, sizeof(query), "{crc:b\"%08lX\"}",
               (unsigned long)crc);

         database_info_list_iterate_new(db_state, query);
      }

      /* database_info_list_new_filtered() returns NULL when the .rdb
       * cannot be opened or the query fails to compile.  Nothing below
       * advances list_index on a NULL info, so every subsequent tick
       * re-entered here with entry_index != 0, skipped the query and
       * returned SCAN_VERDICT_CONTINUE - the scan hung on the first
       * content file that reached an unreadable database.  A database
       * that cannot answer is a database with no match: move on. */
      if (!db_state->info)
         return database_info_list_iterate_next(db_state);
   }

   /* Same shape as the serial lookup below: entry_index must be
    * checked against count before indexing the list, or a query that
    * matched nothing (count == 0, list either empty or NULL) has
    * list[0] dereferenced anyway. */
   if (db_state->info && db_state->entry_index < db_state->info->count)
   {
      database_info_t *db_info_entry =
         &db_state->info->list[db_state->entry_index];

      if (db_info_entry->crc32)
      {
         if (   db_state->crc_pass == DB_CRC_PASS_WHOLE
             || (   db_state->crc_pass == DB_CRC_PASS_BOTH
                 && db_state->whole_done))
         {
            if (db_state->archive_crc == db_info_entry->crc32)
            {
               /* Its own CRC: a whole match, which grants reads ahead. */
               db_state->crc_pass = DB_CRC_PASS_WHOLE;
               return database_info_list_iterate_found_match(
                     _db,
                     db_state, db, NULL);
            }
         }
         if (   db_state->crc_pass != DB_CRC_PASS_WHOLE
             && db_state->crc == db_info_entry->crc32)
         {
            if (   db_state->crc_pass == DB_CRC_PASS_MEMBER
                && !(_db->flags & DB_HANDLE_FLAG_SCAN_WITHOUT_CORE_MATCH)
                && !task_database_claimed(db_state,
                     path_get_extension(name),
                     path_contains_compressed_file))
            {
               db_state->member_hit = true;
               return database_info_list_iterate_next(db_state);
            }
            return database_info_list_iterate_found_match(
                  _db,
                  db_state, db, archive_entry);
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
      database_state_handle_t *db_state, bool scan_serial_and_crc)
{
   const char *db_name;
   /* The toggle comes captured from the task's config: this runs on
    * the threaded task queue's worker. */
   if (!scan_serial_and_crc)
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
   /* The serial index the size walk builds: taken below, or freed
    * where a gate passes the database over. */
   database_info_serial_index_t *si = NULL;
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
   {
      /* A 3DS file the serial pass left unmatched goes to the crc
       * lookup, reusing any crc the serial check computed. */
      if (   db_state->list
          && extension_to_file_type(path_get_extension(name)) == FILE_TYPE_3DS)
      {
         db_state->serial[0]   = '\0';
         db_state->list_index  = 0;
         db_state->entry_index = 0;
         db->type              = DATABASE_TYPE_CRC_LOOKUP;
         if (   !db_state->crc_done
             && !task_database_crc_begin(db_state, name,
                  TASK_DATABASE_CRC_FILE, DB_CRC_DEST_FILE,
                  DB_CRC_FAIL_ERROR))
            return SCAN_VERDICT_ERROR;
         return SCAN_VERDICT_CONTINUE;
      }
      return database_info_list_iterate_end_no_match(_db, db, db_state, name,
            path_contains_compressed_file);
   }

   /* 3DS images, unlike discs, keep the crc lookup's core gate. */
   if (   !(_db->flags & DB_HANDLE_FLAG_SCAN_WITHOUT_CORE_MATCH)
       && extension_to_file_type(path_get_extension(name)) == FILE_TYPE_3DS
       && task_database_skip_unclaimed(db_state, name, NULL,
            path_contains_compressed_file))
      return SCAN_VERDICT_CONTINUE;

   /* If size boundaries are not filled for this DB, run the queries */
   if (   !(db_state->flags[db_state->list_index] & DB_STATE_FLAG_SIZE_CHECKED)
       && !task_database_fill_db_min_max(db_state, &si))
      return SCAN_VERDICT_CONTINUE;

   if (db_state->min_sizes[db_state->list_index] > 0)
   {
      if (!(db_state->flags[db_state->list_index] & DB_STATE_FLAG_HAS_SERIAL))
      {
#ifdef DEBUG
         RARCH_DBG("[Scanner] Skipping DB, no serials here.\n");
#endif
         database_info_serial_index_free(si);
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
            database_info_serial_index_free(si);
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
         database_info_serial_index_free(si);
         return database_info_list_iterate_next(db_state);
      }
   }

   if (db_state->entry_index == 0)
   {
      size_t query_len, serial_len;
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
         if (   !si
             && !db_state->serial_index[db_state->list_index]
             && db_state->index_budget
             && !task_database_index_take(db_state, NULL, &si, NULL))
            return SCAN_VERDICT_CONTINUE;
         if (si)
         {
            size_t used = database_info_serial_index_bytes(si);
            db_state->index_budget = (used < db_state->index_budget)
               ? db_state->index_budget - used : 0;
            db_state->serial_index[db_state->list_index] = si;
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
      serial_len = strlen(serial_buf);
      query_len  = STRLEN_CONST("{'serial': b'")
                 + serial_len
                 + STRLEN_CONST("'}") + 1;

      if (query_len > sizeof(query_buf))
         query = (char*)malloc(query_len);

      if (!query)
      {
         free(serial_buf);
         return SCAN_VERDICT_ERROR;
      }

      /* The pieces' lengths are known: copied in, rather than a
       * snprintf whose output length the compiler cannot bound. */
      memcpy(query, "{'serial': b'", STRLEN_CONST("{'serial': b'"));
      memcpy(query + STRLEN_CONST("{'serial': b'"), serial_buf, serial_len);
      memcpy(query + STRLEN_CONST("{'serial': b'") + serial_len, "'}",
            STRLEN_CONST("'}") + 1);
#ifdef DEBUG
      RARCH_DBG("[Scanner] Serial orig / decoded: \"%s\" / \"%s\".\n", db_state->serial, serial_buf);
#endif
      database_info_list_iterate_new(db_state, query);

      if (query != query_buf)
         free(query);
      free(serial_buf);

serial_query_done:
      /* See the crc lookup: a database that cannot be queried must
       * not leave list_index where it is, or the scan never ends. */
      if (!db_state->info)
         return database_info_list_iterate_next(db_state);
   }

   if (db_state->info)
   {
      /* A 3DS product code is shared by a title's revisions and
       * regions: the crc picks among them, and the first stands in
       * when it matches none.  scan_serial_and_crc makes the crc a
       * requirement, as it is for PSP and Dreamcast. */
      size_t ctr_first = db_state->info->count;
      size_t ctr_hits  = 0;
      bool   ctr_crc   = false;

      if (extension_to_file_type(path_get_extension(name)) == FILE_TYPE_3DS)
      {
         size_t i;
         for (i = 0; i < db_state->info->count; i++)
         {
            const char *serial = db_state->info->list[i].serial;
            if (!serial || !string_is_equal(db_state->serial, serial))
               continue;
            if (!ctr_hits++)
               ctr_first = i;
         }
         ctr_crc = ctr_hits > 1 || _db->task_config->scan_serial_and_crc;
      }

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
               if (   ctr_crc
                   || task_database_check_serial_and_crc(db_state,
                     _db->task_config->scan_serial_and_crc))
               {
                  /* Resumes at this entry once the CRC is in.  A file
                   * that cannot be read keeps a CRC of 0. */
                  if (!db_state->crc_done)
                  {
                     if (task_database_crc_begin(db_state, name,
                           extension_to_file_type(path_get_extension(name))
                              == FILE_TYPE_GDI
                           ? TASK_DATABASE_CRC_GDI : TASK_DATABASE_CRC_FILE,
                           DB_CRC_DEST_FILE, DB_CRC_FAIL_ZERO))
                        return SCAN_VERDICT_CONTINUE;
                     db_state->crc      = 0;
                     db_state->crc_done = true;
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

      if (ctr_hits > 1 && !_db->task_config->scan_serial_and_crc)
      {
         db_state->entry_index = ctr_first;
         return database_info_list_iterate_found_match(_db,
               db_state, db, NULL);
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
      bool path_contains_compressed_file,
      nbio_budget_t *b)
{
   if (db_state->crc_fd)
      return task_database_crc_step(db_state, b);
   if (db_state->index_job && !db_state->index_ready)
   {
#ifdef SCAN_HAVE_READERS
      if (   db_state->crc_pass == DB_CRC_PASS_MEMBER
          && !db_state->member_hit)
         scan_readers_top_up(_db);
#endif
      return task_database_index_step(db_state, b);
   }
#ifdef DEBUG
   RARCH_DBG("[Scanner] Type %d, \"%s\" against \"%s\".\n", db->type, name, database_info_get_current_name(db_state));
   /* list_index == list->size is a valid state here (every database
    * tried, the "against (null)" tick), and the size arrays have
    * list->size entries - reading them unguarded was a one-past-the-
    * end read on every content file under ASan. */
   if (db_state->list && (size_t)db_state->list_index < db_state->list->size)
      RARCH_DBG("[Scanner] Size: min %ld actual %ld max %ld.\n", db_state->min_sizes[db_state->list_index], db_state->size, db_state->max_sizes[db_state->list_index]);
#endif
   switch (db->type)
   {
      case DATABASE_TYPE_ITERATE:
#ifdef SCAN_HAVE_READERS
         switch (scan_readers_take(_db, db_state, db, name))
         {
            case SCAN_TAKE_DONE:
               return SCAN_VERDICT_CONTINUE;
            case SCAN_TAKE_WAIT:
               return SCAN_VERDICT_WAIT;
            default:
               break;
         }
#endif
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
   task_database_crc_reset(db_state);
   task_database_index_close(db_state);
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
          * and several others dereference unconditionally), so this
          * test must come before all of them. */
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

         /* Set default core, if required.
          * A file is only added: the playlist keeps its own. */
         if (    manual_scan->task_config->core_set
             && !manual_scan->task_config->content_is_file)
         {
            playlist_set_default_core_path(manual_scan->flush_playlist,
                  manual_scan->task_config->core_path);
            playlist_set_default_core_name(manual_scan->flush_playlist,
                  manual_scan->task_config->core_name);
         }

         /* Record remaining scan parameters to enable
          * subsequent 'refresh playlist' operations.
          * A file is only added, so the playlist keeps its folder's scan
          * settings. */
         if (!manual_scan->task_config->content_is_file)
         {
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
         }

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
         PLAYLIST_SET_ENTRY_SLOT(&entry, 0);
         PLAYLIST_SET_RUNTIME_HOURS(&entry, 0);
         PLAYLIST_SET_RUNTIME_MINUTES(&entry, 0);
         PLAYLIST_SET_RUNTIME_SECONDS(&entry, 0);
         PLAYLIST_SET_LAST_PLAYED_YEAR(&entry, 0);
         PLAYLIST_SET_LAST_PLAYED_MONTH(&entry, 0);
         PLAYLIST_SET_LAST_PLAYED_DAY(&entry, 0);
         PLAYLIST_SET_LAST_PLAYED_HOUR(&entry, 0);
         PLAYLIST_SET_LAST_PLAYED_MINUTE(&entry, 0);
         PLAYLIST_SET_LAST_PLAYED_SECOND(&entry, 0);

         /* Absence is proven: the existence check above said so, or
          * this is an m3u whose previous entries were just deleted.
          * The checked playlist_push() would re-scan the entire
          * playlist for the same answer. */
         playlist_push_unchecked(manual_scan->flush_playlist, &entry);
         manual_scan->flush_added++;

         RARCH_LOG("[Scanner] Add \"%s / %s\".\n", db_name_noext, result->entry_label);

         if (manual_scan->task_config->cli_scan_output)
            task_database_scan_console_output(result->entry_label,
                  db_name_noext, true);
      }
      /* Entry already exists - output duplicate indicator for CLI scans */
      else if (manual_scan->flush_playlist && manual_scan->task_config->cli_scan_output)
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
bool task_push_dbscan(const char *fullpath, retro_task_callback_t cb)
{
   manual_content_scan_set_menu_content_dir(fullpath);
   return task_push_manual_content_scan(false, cb);
}

#endif

/* Frees task handle + all constituent objects */
static void free_manual_content_scan_handle(manual_scan_handle_t *manual_scan)
{
   if (!manual_scan)
      return;

#ifdef SCAN_HAVE_READERS
   scan_readers_stop(manual_scan);
#endif

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

         task_database_crc_close(dbstate);
         task_database_members_free(dbstate->members);
         dbstate->members = NULL;
         string_list_free(dbstate->member_exts);
         dbstate->member_exts = NULL;
         task_database_index_close(dbstate);
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
         if (dbstate->claim_exts)
         {
            size_t ce;
            for (ce = 0; ce < db_count; ce++)
               string_list_free(dbstate->claim_exts[ce]);
            free(dbstate->claim_exts);
            dbstate->claim_exts = NULL;
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

   /* At retrieval, on the main thread, where the companion
    * belongs: ui_companion_driver_notify_refresh reads companion
    * state that main-thread code owns, so the handler must not
    * call it from the worker. */
   ui_companion_driver_notify_refresh();

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
    * Outside the HAVE_MENU block deliberately: inside it, a build
    * without menu support runs the scan and then drops the callback,
    * and a caller waiting on it waits forever.
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
         if (manual_scan->task_config->cli_scan_output)
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
         if (  !manual_scan->content_list
             || manual_scan->content_list->size < 1)
         {
            const char *_msg = msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT);
            runloop_msg_queue_push(_msg, strlen(_msg), 1, 100, true, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            return true;
         }

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

   /* Set default core, if required.
    * A file is only added: the playlist keeps its own. */
   if (    manual_scan->task_config->core_set
       && !manual_scan->task_config->content_is_file)
   {
      playlist_set_default_core_path(manual_scan->playlist,
            manual_scan->task_config->core_path);
      playlist_set_default_core_name(manual_scan->playlist,
            manual_scan->task_config->core_name);
   }

   /* Record remaining scan parameters to enable
    * subsequent 'refresh playlist' operations.
    * A file is only added, so the playlist keeps its folder's scan
    * settings. */
   if (!manual_scan->task_config->content_is_file)
   {
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
   }

   /* All good - can start iterating
    * > If playlist has content and 'validate
    *   entries' is enabled, go to clean-up phase
    *   (a file is only added, so its scan prunes nothing)
    * > Otherwise go straight to content scan phase */
   if (    manual_scan->task_config->validate_entries
       && !manual_scan->task_config->content_is_file
       && (manual_scan->playlist_size > 0))
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

/* What an ITERATE step leaves for the tick to do. */
enum scan_step
{
   SCAN_STEP_NEXT = 0,
   SCAN_STEP_FINISHED,
   SCAN_STEP_WAIT
};

/* One step of the ITERATE family: a cleanup entry, a database probe or
 * a playlist add. */
static enum scan_step task_manual_scan_iterate_step(retro_task_t *task,
      manual_scan_handle_t *manual_scan, nbio_budget_t *b)
{
#ifdef HAVE_LIBRETRODB
   database_info_handle_t  *dbinfo  = manual_scan->handle;
   database_state_handle_t *dbstate = &manual_scan->state;
#else
   (void)b;
#endif

#ifdef SCAN_HAVE_READERS
   switch (manual_scan->status)
   {
      case DATABASE_SCAN_ITERATE_START:
      case DATABASE_SCAN_ITERATE_CONTENT:
      case DATABASE_SCAN_ITERATE_NEXT:
         scan_readers_fill(manual_scan);
         break;
      default:
         break;
   }
#endif

   switch (manual_scan->status)
   {
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

#ifdef SCAN_HAVE_READERS
            if (   !manual_scan->readers_tried
                && manual_scan->content_list->size
                   - manual_scan->content_list_index >= 2)
               scan_readers_start(task, manual_scan);
#endif
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
               return SCAN_STEP_FINISHED;

            path_contains_compressed_file      = path_contains_compressed_file(content_path);
            /* Reminder - remove this shortcut when serial scan inside zip is solved */
            if (path_contains_compressed_file)
               if (dbinfo->type == DATABASE_TYPE_ITERATE)
                  dbinfo->type   = DATABASE_TYPE_ITERATE_ARCHIVE;

            current_verdict = (enum scan_verdict)task_database_iterate(manual_scan, content_path, dbstate, dbinfo,
                     path_contains_compressed_file, b);
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
                  /* An error verdict leaves the per-file state where
                   * it was, so treating it like CONTINUE re-ran the
                   * same failing step every tick.  Give up on this
                   * file and go to the next one. */
                  RARCH_ERR("[Scanner] Scanning of content unexpectedly failed for \"%s\"\n", content_path);
#ifdef SCAN_HAVE_READERS
                  if (dbstate->crc_pass != DB_CRC_PASS_FILE)
                     scan_readers_whole_ahead(manual_scan, 0);
#endif
                  task_database_crc_reset(dbstate);
                  manual_scan->status = DATABASE_SCAN_ITERATE_NEXT;
                  break;
               case SCAN_VERDICT_CONTINUE:
                  break;
               case SCAN_VERDICT_WAIT:
                  return SCAN_STEP_WAIT;
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
         else if (manual_scan->m3u_list->size > 0)
            manual_scan->status = MANUAL_SCAN_ITERATE_M3U;
         else
            manual_scan->status = MANUAL_SCAN_END;
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
      default:
         break;
   }

   return SCAN_STEP_NEXT;
}

static bool manual_scan_status_is_iterate(enum manual_scan_status status)
{
   switch (status)
   {
      case MANUAL_SCAN_ITERATE_CLEAN:
#ifdef HAVE_LIBRETRODB
      case DATABASE_SCAN_ITERATE_START:
      case DATABASE_SCAN_ITERATE_CONTENT:
      case DATABASE_SCAN_ITERATE_NEXT:
#endif
      case MANUAL_SCAN_ITERATE_CONTENT:
         return true;
      default:
         break;
   }
   return false;
}

/* Runs ITERATE steps while the shared window lasts, timed rather than
 * counted; its floor pays for the first, so every call makes progress. */
static bool task_manual_scan_iterate_tick(retro_task_t *task,
      manual_scan_handle_t *manual_scan)
{
   nbio_budget_t b;
   bool finished = false;

   task_nbio_slice_open(&b);
   while (task_nbio_slice_within_budget(&b, 0, 0))
   {
      enum scan_step step = task_manual_scan_iterate_step(task,
            manual_scan, &b);

      if (step == SCAN_STEP_FINISHED)
      {
         finished = true;
         break;
      }
      /* A reader's wake ends the park. */
      if (step == SCAN_STEP_WAIT)
      {
         task_queue_park(task, SCAN_READER_PARK_USEC);
         break;
      }
      if (!manual_scan_status_is_iterate(manual_scan->status))
         break;
   }
   task_nbio_slice_close(&b);
#ifdef SCAN_HAVE_READERS
   /* The list is done: M3U and END run without the readers. */
   if (!manual_scan_status_is_iterate(manual_scan->status))
      scan_readers_stop(manual_scan);
#endif
   return finished;
}

static void task_manual_content_scan_handler(retro_task_t *task)
{
   uint8_t flg;
   manual_scan_handle_t *manual_scan = NULL;
#ifdef HAVE_LIBRETRODB
   database_state_handle_t *dbstate  = NULL;
#endif

   if (!task)
      goto task_finished;

   if (!(manual_scan = (manual_scan_handle_t*)task->state))
      goto task_finished;

#ifdef HAVE_LIBRETRODB
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

   if (manual_scan_status_is_iterate(manual_scan->status))
   {
      if (task_manual_scan_iterate_tick(task, manual_scan))
         goto task_finished;
      return;
   }

   switch (manual_scan->status)
   {
      case MANUAL_SCAN_BEGIN:
      case MANUAL_SCAN_BEGIN_DIR_LIST:
      case MANUAL_SCAN_BEGIN_DAT_LOAD:
      case MANUAL_SCAN_BEGIN_PLAYLIST:
         if (task_manual_scan_begin_tick(task, manual_scan))
            goto task_finished;
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
            RARCH_LOG("[Scanner] %s\n", msg);
            if (manual_scan->task_config->cli_scan_output)
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
#ifdef SCAN_HAVE_READERS
   if (manual_scan)
      scan_readers_stop(manual_scan);
#endif
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

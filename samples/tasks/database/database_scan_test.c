/* End-to-end regression test for the content scanner.
 *
 * Drives task_push_dbscan() over content that is built to match, and
 * checks the playlists it produces name the right games under the
 * right databases.
 *
 * This is the only test here that reaches the scan as a whole: the
 * task lifecycle, the database iteration, the crc lookup, the
 * promotion of a matched database to the front of the list, the
 * playlist write and the teardown.  Two defects in this tree went
 * undetected because nothing exercised that:
 *
 *   - the index caches were keyed by a database's position in the
 *     list but not moved when a match promoted it, so content ended
 *     up filed under whichever database took the vacated slot;
 *   - playlist_init() left scan_record.overwrite_playlist
 *     uninitialised, and it is written into the playlist and read
 *     back on the next scan to decide whether to overwrite.
 *
 * The second content file therefore matches the *second* database on
 * purpose.  A match in the first would never trigger the promotion,
 * and the pairing bug would pass unnoticed again.
 *
 * Everything is built here - databases, content, core info - so the
 * test needs no fixture beyond a writable directory.  Content is made
 * to match by forcing its crc32: four bytes appended to a buffer can
 * bring the checksum to any value, which is cheaper and more precise
 * than searching for content that happens to collide.
 *
 * Run with: make scan_test SANITIZER=address,undefined
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include <queues/task_queue.h>
#include <file/file_path.h>
#include <lists/dir_list.h>
#include <retro_timers.h>
#include <streams/file_stream.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <retro_atomic.h>
#include <features/features_cpu.h>
#include <streams/interface_stream.h>
#include <file/archive_file.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include "../../../core_info.h"
#include "../../../database_info.h"
#include "../../../tasks/tasks_internal.h"
#include "../../../manual_content_scan.h"
#include "../../../list_special.h"
#include "../../../configuration.h"
#include "../../../verbosity.h"
#include "../../../libretro-db/libretrodb.h"
#ifdef HAVE_SMBCLIENT
#include "../../../libretro-common/vfs/vfs_implementation_smb.h"
#endif
#include "spend_window.h"

#include <encodings/rzstd.h>
#include "../../../libretro-common/samples/encodings/rbzip2/bz_write.h"
#include "scan_archives.h"

#define SCAN_TIMEOUT_SECONDS 120


/* Stubs for symbols referenced by the retroarch-tree sources we pull
 * in.  The real definitions live in intl/msg_hash_us.c and
 * configuration.c, but those files transitively require RARCH_INTERNAL
 * which drags in the entire frontend subsystem.  This sample only
 * exercises task_push_dbscan, and msg_hash_to_str_us() is the one stub
 * checks depend on: it names the invalid content message, and the END
 * step's message holds that step for reader_list_end_lane().
 *
 * These take enum msg_hash_enums now that configuration.h - included
 * for settings_t - brings msg_hash.h with it.  They used to be
 * declared with int, which matched at the link level but conflicts
 * once the real prototypes are visible. */
int msg_hash_get_help_us_enum(enum msg_hash_enums msg, char *s, size_t len)
{
   (void)msg;
   if (s && len)
      s[0] = '\0';
   return 0;
}

/* With end_hold set, the scan's END step waits up to 2 s for its
 * readers to leave: end_readers_out is 1 if they did, 0 if not.
 * end_hold is set only between scans. */
static bool               end_hold;
static retro_atomic_int_t end_readers_out;

static bool count_live_reader(retro_task_t *task, void *user_data)
{
   uint8_t flags = task_get_flags(task);

   if (     (flags & RETRO_TASK_FLG_PARALLEL)
         && !(flags & RETRO_TASK_FLG_FINISHED))
      (*(unsigned*)user_data)++;
   return false;
}

static void end_step_hold(void)
{
   task_finder_data_t finder;
   unsigned           i, n = 1;

   end_hold        = false;
   finder.func     = count_live_reader;
   finder.userdata = &n;
   for (i = 0; i < 2000 && n; i++)
   {
      n = 0;
      task_queue_find(&finder);
      if (n)
         retro_sleep(1);
   }
   retro_atomic_store_release_int(&end_readers_out, n ? 0 : 1);
}

const char *msg_hash_to_str_us(enum msg_hash_enums msg)
{
   if (msg == MSG_SCANNING_OF_DIRECTORY_FINISHED && end_hold)
      end_step_hold();
   if (msg == MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT)
      return "invalid content";
   return "";
}

/* The string-table index builder (added by the msg_hash strtab refactor).
 * With msg_hash_to_str_us() stubbed above, the index is never consulted, so
 * this can be an empty stub rather than linking intl/msg_hash_us.c. */
void msg_hash_us_index_init(void)
{
}

settings_t *config_get_ptr(void)
{
   /* A real settings_t, not a zeroed blob.
    *
    * The scan reads settings->paths.directory_playlist before it does
    * anything else and refuses to start if it is empty - so a stub that
    * returned zeros meant no task was ever created, the completion
    * callback never fired, and this sample sat in its loop until the CI
    * runner killed it.  The playlist directory argument it accepts on
    * the command line went nowhere.
    *
    * Static so it is zero-initialised; main() fills in the one field
    * the scan actually consults. */
   static settings_t settings;
   return &settings;
}

static bool invalid_content_pushed = false;

/* Additional stubs for retroarch-core symbols referenced transitively,
 * to avoid pulling in retroarch.c, runloop.c, frontend drivers, and the
 * UI/video subsystems.  runloop_msg_queue_push() also records the
 * invalid content message. */
void runloop_msg_queue_push(const char *msg, size_t len,
      unsigned prio, unsigned duration,
      bool flush, char *title, unsigned icon, unsigned category)
{
   (void)len; (void)prio; (void)duration;
   (void)flush; (void)title; (void)icon; (void)category;
   if (msg && !strcmp(msg, "invalid content"))
      invalid_content_pushed = true;
}

bool retroarch_override_setting_is_set(unsigned enum_idx, void *data)
{
   (void)enum_idx; (void)data;
   return false;
}

void ui_companion_driver_notify_refresh(void)
{
}

void video_display_server_set_window_progress(int progress, bool finished)
{
   (void)progress; (void)finished;
}

/* task_database.c's progress_cb is now the shared task_window_progress_cb,
 * whose definition lives in tasks/task_file_transfer.c.  Pulling that
 * file in would drag in nbio, the audio mixer, and the image-task
 * machinery, none of which the dbscan path exercises.  Stub it here
 * to satisfy the linker; the function is never called on this code
 * path because no progress_cb is invoked unless a worker thread
 * publishes progress, and this sample never reaches that state. */
void task_window_progress_cb(retro_task_t *task)
{
   (void)task;
}

#ifdef HAVE_SMBCLIENT
/* The VFS's SMB backend with no share behind it: every call fails. */
bool retro_vfs_file_open_smb(libretro_vfs_implementation_file *stream,
      const char *path, unsigned mode, unsigned hints)
{
   (void)stream; (void)path; (void)mode; (void)hints;
   return false;
}

int64_t retro_vfs_file_read_smb(libretro_vfs_implementation_file *stream,
      void *s, uint64_t len)
{
   (void)stream; (void)s; (void)len;
   return -1;
}

int64_t retro_vfs_file_view_smb(libretro_vfs_implementation_file *stream,
      uint64_t len, const uint8_t **ptr)
{
   (void)stream; (void)len; (void)ptr;
   return -1;
}

int64_t retro_vfs_file_write_smb(libretro_vfs_implementation_file *stream,
      const void *s, uint64_t len)
{
   (void)stream; (void)s; (void)len;
   return -1;
}

int64_t retro_vfs_file_seek_smb(libretro_vfs_implementation_file *stream,
      int64_t offset, int whence)
{
   (void)stream; (void)offset; (void)whence;
   return -1;
}

int64_t retro_vfs_file_tell_smb(libretro_vfs_implementation_file *stream)
{
   (void)stream;
   return -1;
}

int retro_vfs_file_close_smb(libretro_vfs_implementation_file *stream)
{
   (void)stream;
   return -1;
}

int retro_vfs_file_error_smb(libretro_vfs_implementation_file *stream)
{
   (void)stream;
   return 1;
}

smb_dir_handle *retro_vfs_opendir_smb(const char *path, bool include_hidden)
{
   (void)path; (void)include_hidden;
   return NULL;
}

struct smbc_dirent *retro_vfs_readdir_smb(smb_dir_handle *dh)
{
   (void)dh;
   return NULL;
}

int retro_vfs_closedir_smb(smb_dir_handle *dh)
{
   (void)dh;
   return -1;
}

int retro_vfs_stat_smb(const char *path, int64_t *size)
{
   (void)path; (void)size;
   return 0;
}
#endif

/* dir_list_new_special lives in retroarch.c, which we cannot link
 * without dragging in the world.
 *
 * Returning NULL here meant the scan never enumerated its databases,
 * so it could not finish, and everything downstream of a completed
 * scan - notably the task teardown - went unexercised.  The scanner
 * only asks for DIR_LIST_DATABASES, which retroarch.c answers with a
 * plain "rdb" listing, so that much is worth providing. */
struct string_list *dir_list_new_special(const char *input_dir,
      enum dir_list_type type, const char *filter, bool show_hidden_files)
{
   (void)filter;

   if (type == DIR_LIST_DATABASES)
      return dir_list_new(input_dir, "rdb", false, show_hidden_files,
            false, false);

   return NULL;
}

/* Linked with --wrap: database listings in name order while db_order
 * is 1, in reverse while it is -1.  Set only between scans. */
static int db_order;

struct string_list *__real_dir_list_new(const char *dir, const char *ext,
      bool include_dirs, bool include_hidden, bool include_compressed,
      bool recursive);
struct string_list *__wrap_dir_list_new(const char *dir, const char *ext,
      bool include_dirs, bool include_hidden, bool include_compressed,
      bool recursive)
{
   struct string_list *list = __real_dir_list_new(dir, ext, include_dirs,
         include_hidden, include_compressed, recursive);
   size_t              i;

   if (!list || !db_order || !ext || strcmp(ext, "rdb"))
      return list;
   dir_list_sort(list, false);
   for (i = 0; db_order < 0 && i < list->size / 2; i++)
   {
      struct string_list_elem e;

      e                               = list->elems[i];
      list->elems[i]                  = list->elems[list->size - 1 - i];
      list->elems[list->size - 1 - i] = e;
   }
   return list;
}

/* Linked with --wrap: the scan's content list in name order while
 * content_sorted is set, with share_paths added while that is.  Both
 * set only between scans. */
static bool               content_sorted;
static const char *const *share_paths;
static size_t             share_count;

database_info_handle_t *__real_database_info_dir_init_from_list(
      enum database_type type, struct string_list *list);
database_info_handle_t *__wrap_database_info_dir_init_from_list(
      enum database_type type, struct string_list *list)
{
   database_info_handle_t *db;
   size_t                  i;

   for (i = 0; share_paths && list && i < share_count; i++)
   {
      union string_list_elem_attr attr;

      attr.i = 0;
      string_list_append(list, share_paths[i], attr);
   }
   db = __real_database_info_dir_init_from_list(type, list);

   if (db && content_sorted)
      dir_list_sort(list, false);
   return db;
}

static int failures = 0;
static int checks   = 0;

static void check(int ok, const char *what, const char *detail)
{
   checks++;
   printf("  %-5s %-42s %s\n", ok ? "ok" : "FAIL", what,
         detail ? detail : "");
   if (!ok)
      failures++;
}

/* Linked with --wrap=libretrodb_open.  An index build opens its
 * database once and walks it once, so this counts the walks; those of
 * a path ending in open_watch also in watch_opens, set only between
 * scans. */
static unsigned           rdb_opens = 0;
static const char        *open_watch;
static retro_atomic_int_t watch_opens;
/* Opens of databases whose path holds open_name. */
static const char        *open_name;
static unsigned           named_opens;

int __real_libretrodb_open(const char *path, libretrodb_t *db, bool write);
int __wrap_libretrodb_open(const char *path, libretrodb_t *db, bool write)
{
   rdb_opens++;
   if (open_watch && path && string_ends_with(path, open_watch))
      retro_atomic_fetch_add_int(&watch_opens, 1);
   if (open_name && path && strstr(path, open_name))
      named_opens++;
   return __real_libretrodb_open(path, db, write);
}

/* The scanner's default, for what this build should do. */
#ifndef SCAN_READERS_MAX
#define SCAN_READERS_MAX 4
#endif
#ifndef SCAN_READER_AHEAD_MAX
#define SCAN_READER_AHEAD_MAX 16
#endif

/* The readers a scan on the threaded queue pushes on this host. */
static unsigned readers_expected(void)
{
#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_LOCK_FREE)
   unsigned n = cpu_features_get_core_amount();
   if (n > SCAN_READERS_MAX)
      n = SCAN_READERS_MAX;
   return n >= 2 ? n : 0;
#else
   return 0;
#endif
}

/* Linked with --wrap: the parks the scanner asks for. */
static retro_atomic_int_t scan_parks;

void __real_task_queue_park(retro_task_t *task, retro_time_t horizon_usec);
void __wrap_task_queue_park(retro_task_t *task, retro_time_t horizon_usec)
{
   retro_atomic_fetch_add_int(&scan_parks, 1);
   __real_task_queue_park(task, horizon_usec);
}

static uintptr_t current_tid(void)
{
#ifdef HAVE_THREADS
   return sthread_get_current_thread_id();
#else
   return 1;
#endif
}

/* The thread inside a scan's tick, which the window's open and close
 * bracket: a read there is the scan's own, any other a reader's.  The
 * last thread to run a tick stays in scan_thread_tid. */
static retro_atomic_size_t scan_tick_tid;
static retro_atomic_size_t scan_thread_tid;

static void scan_tick_open(void)
{
   retro_atomic_store_release_size(&scan_tick_tid, (size_t)current_tid());
   retro_atomic_store_release_size(&scan_thread_tid, (size_t)current_tid());
}

void __real_task_nbio_slice_close(nbio_budget_t *b);
void __wrap_task_nbio_slice_close(nbio_budget_t *b)
{
   __real_task_nbio_slice_close(b);
   retro_atomic_store_release_size(&scan_tick_tid, 0);
}

/* Linked with --wrap: outside the scan's tick only a reader's handler
 * asks, once a call.  Those calls are counted from the scan's first
 * whole read on (its match grants just after), and the count
 * kept as the scan first reads a stream of watch_size bytes itself
 * (-1: it read none), with the microseconds since the last call then
 * (-1: no call yet).
 * Whole reads are counted as each begins: the scan's, readers' and
 * readers' of watch_size bytes.  Reset by run_scan(). */
static retro_atomic_int_t rd_armed;
static retro_atomic_int_t rd_passes;
static retro_atomic_int_t rd_last;
static retro_time_t       rd_t0;
static int                rd_watch_passes;
static int                rd_watch_gap;
static retro_atomic_int_t scan_wholes;
static retro_atomic_int_t rd_wholes;
static retro_atomic_int_t rd_watch_wholes;

bool __real_task_queue_is_threaded(void);
bool __wrap_task_queue_is_threaded(void)
{
   if (   retro_atomic_load_acquire_int(&rd_armed)
       && retro_atomic_load_acquire_size(&scan_tick_tid)
          != (size_t)current_tid())
   {
      retro_atomic_fetch_add_int(&rd_passes, 1);
      retro_atomic_store_release_int(&rd_last,
            (int)(cpu_features_get_time_usec() - rd_t0));
   }
   return __real_task_queue_is_threaded();
}

/* A stream of slow_size bytes sleeps slow_ms before its first quantum;
 * slow_in counts those sleeps begun, slow_back those ended (slow_begun
 * holds reader_begun as each of the first four ends), slow_by_scan
 * those in the scan's tick and slow_on_scan those on the thread that
 * last ran the scan: a reader there holds every other task of the
 * queue, the scan too.  Reads of streams of watch_size bytes are
 * counted by who makes them, a reader's on the main thread apart, and
 * those of begun_size bytes as each begins (reader_begun those a
 * reader begins); so are reads begun once cb_fired is set.  A reader's
 * read of a stream of fail_size bytes fails, counted in reader_fails.
 * The scan's first quantum of a stream of hold_size bytes waits, at
 * most HOLD_USEC, until hold_want reads of begun_size bytes have begun,
 * a slow one if slow_size is set and gate_n readers are in the gate
 * (below).  All set only between scans. */
#define HOLD_USEC 10000000

static uint32_t           slow_size;
static unsigned           slow_ms;
static retro_atomic_int_t slow_in;
static retro_atomic_int_t slow_back;
static int                slow_begun[4];
static retro_atomic_int_t slow_on_scan;
static retro_atomic_int_t slow_by_scan;
static uint32_t           watch_size;
static retro_atomic_int_t watch_scan;
static retro_atomic_int_t watch_reader;
static retro_atomic_int_t watch_inline;
static uint32_t           begun_size;
static retro_atomic_int_t reads_begun;
static retro_atomic_int_t reader_begun;
static retro_atomic_int_t cb_fired;
static retro_atomic_int_t reads_after_cb;
static uint32_t           fail_size;
static retro_atomic_int_t reader_fails;
static uint32_t           hold_size;
static int                hold_want;
static int                gate_n;
static retro_atomic_int_t gate_in;
static retro_atomic_int_t hold_late;
#ifdef HAVE_THREADS
static uintptr_t          main_tid;
#endif

int64_t __real_intfstream_crc_step(intfstream_internal_t *intf,
      uint32_t *accumulator, size_t max_bytes);
int64_t __wrap_intfstream_crc_step(intfstream_internal_t *intf,
      uint32_t *accumulator, size_t max_bytes)
{
   int64_t size  = intfstream_get_size(intf);
   bool    first = intfstream_tell(intf) == 0;
   bool    scan  = retro_atomic_load_acquire_size(&scan_tick_tid)
      == (size_t)current_tid();

   if (first && scan)
   {
      retro_atomic_fetch_add_int(&scan_wholes, 1);
      retro_atomic_store_release_int(&rd_armed, 1);
      if (   watch_size && size == (int64_t)watch_size
          && rd_watch_passes < 0)
      {
         rd_watch_passes = retro_atomic_load_acquire_int(&rd_passes);
         rd_watch_gap    = -1;
         if (rd_watch_passes)
            rd_watch_gap = (int)(cpu_features_get_time_usec() - rd_t0)
               - retro_atomic_load_acquire_int(&rd_last);
      }
   }
   else if (first)
   {
      retro_atomic_fetch_add_int(&rd_wholes, 1);
      if (watch_size && size == (int64_t)watch_size)
         retro_atomic_fetch_add_int(&rd_watch_wholes, 1);
   }
   if (first && retro_atomic_load_acquire_int(&cb_fired))
      retro_atomic_fetch_add_int(&reads_after_cb, 1);
   if (slow_size && size == (int64_t)slow_size && first)
   {
      int k;

      if (retro_atomic_load_acquire_size(&scan_thread_tid)
            == (size_t)current_tid())
         retro_atomic_fetch_add_int(&slow_on_scan, 1);
      if (scan)
         retro_atomic_fetch_add_int(&slow_by_scan, 1);
      retro_atomic_fetch_add_int(&slow_in, 1);
      retro_sleep(slow_ms);
      if ((k = retro_atomic_fetch_add_int(&slow_back, 1)) < 4)
         slow_begun[k] = retro_atomic_load_acquire_int(&reader_begun);
   }
   if (hold_size && size == (int64_t)hold_size && first && scan)
   {
      retro_time_t until = cpu_features_get_time_usec() + HOLD_USEC;

      while (   (   retro_atomic_load_acquire_int(&reads_begun) < hold_want
                 || (slow_size && !retro_atomic_load_acquire_int(&slow_in))
                 || retro_atomic_load_acquire_int(&gate_in) < gate_n)
             && cpu_features_get_time_usec() < until)
         retro_sleep(1);
      if (cpu_features_get_time_usec() >= until)
         retro_atomic_fetch_add_int(&hold_late, 1);
   }
   if (begun_size && size == (int64_t)begun_size && first)
   {
      retro_atomic_fetch_add_int(&reads_begun, 1);
      if (!scan)
         retro_atomic_fetch_add_int(&reader_begun, 1);
   }
   if (watch_size && size == (int64_t)watch_size)
   {
      if (scan)
         retro_atomic_fetch_add_int(&watch_scan, 1);
      else
      {
         retro_atomic_fetch_add_int(&watch_reader, 1);
#ifdef HAVE_THREADS
         if (current_tid() == main_tid)
            retro_atomic_fetch_add_int(&watch_inline, 1);
#endif
      }
   }
   if (fail_size && size == (int64_t)fail_size && !scan)
   {
      retro_atomic_fetch_add_int(&reader_fails, 1);
      return -1;
   }
   return __real_intfstream_crc_step(intf, accumulator, max_bytes);
}

/* Linked with --wrap: of the databases and archives whose paths hold
 * step_watch, the index lookups that read records and the
 * archive#member paths built, in all and the most one check of
 * run_scan_counted() made.  step_watch is set only between scans. */
typedef struct
{
   unsigned seq;
   unsigned now;
   unsigned total;
   unsigned most;
} step_count_t;

static const char  *step_watch;
static unsigned     check_seq;
static step_count_t step_reads;
static step_count_t step_joins;

static void step_count(step_count_t *c)
{
   if (c->seq != check_seq)
   {
      c->seq = check_seq;
      c->now = 0;
   }
   c->total++;
   if (++c->now > c->most)
      c->most = c->now;
}

/* Linked with --wrap: the scan's lookups in a CRC index.  On the
 * threaded queue, one of look_crc[k] (k < look_n) waits, at most
 * HOLD_USEC, until look_want[k] whole reads by readers have begun, of
 * watch_size bytes with look_watch, and with look_files no more than
 * that many files less the scan's own.  look_hits counts the holds; one
 * that times out counts in look_late and ends the scan's holds;
 * look_end[k] keeps the count the last of look_crc[k] ended at.  All
 * set only between scans. */
#define LOOK_MAX 40

static uint32_t           look_crc[LOOK_MAX];
static int                look_want[LOOK_MAX];
static int                look_end[LOOK_MAX];
static unsigned           look_n;
static bool               look_watch;
static int                look_files;
static retro_atomic_int_t look_hits;
static retro_atomic_int_t look_late;

database_info_list_t *__real_database_info_list_new_crc(
      database_info_crc_index_t *idx, const char *rdb_path,
      uint32_t crc, uint32_t archive_crc, unsigned fields);
database_info_list_t *__wrap_database_info_list_new_crc(
      database_info_crc_index_t *idx, const char *rdb_path,
      uint32_t crc, uint32_t archive_crc, unsigned fields)
{
   database_info_list_t *list;
   unsigned              k;

   for (k = 0; k < look_n; k++)
      if (   crc == look_crc[k]
          && !retro_atomic_load_acquire_int(&look_late)
          && __real_task_queue_is_threaded())
      {
         retro_atomic_int_t *n     = look_watch
            ? &rd_watch_wholes : &rd_wholes;
         int                 want  = look_want[k];
         retro_time_t        until = cpu_features_get_time_usec()
            + HOLD_USEC;

         if (   look_files
             && want > look_files
                - retro_atomic_load_acquire_int(&scan_wholes))
            want = look_files - retro_atomic_load_acquire_int(&scan_wholes);
         retro_atomic_fetch_add_int(&look_hits, 1);
         while (   retro_atomic_load_acquire_int(n) < want
                && cpu_features_get_time_usec() < until)
            retro_sleep(1);
         if (retro_atomic_load_acquire_int(n) < want)
            retro_atomic_store_release_int(&look_late, 1);
         look_end[k] = retro_atomic_load_acquire_int(n);
         break;
      }
   list = __real_database_info_list_new_crc(idx, rdb_path, crc,
         archive_crc, fields);
   if (   step_watch && rdb_path && strstr(rdb_path, step_watch)
       && list && list->count)
      step_count(&step_reads);
   return list;
}

/* Linked with --wrap: index builds.  On the threaded queue, the build
 * of a database whose path holds walk_name waits, at most HOLD_USEC,
 * as it begins until walk_want whole reads by readers have begun, and
 * at its first step until walk_more more have, or with walk_more 0
 * sleeps WALK_IDLE_MS, time for readers to begin any they may; until
 * it ends the scan's window does not run out, so it steps in one call.
 * walk_from and walk_to keep the readers' whole reads begun as it
 * began and as it ended; walk_late is set by a wait that ran out.
 * All set only between scans. */
#define WALK_IDLE_MS 300

static const char                *walk_name;
static int                        walk_want;
static int                        walk_more;
static database_info_index_job_t *walk_job;
static int                        walk_steps;
static int                        walk_from;
static int                        walk_to;
static bool                       walk_late;

static void walk_hold(int want)
{
   retro_time_t until = cpu_features_get_time_usec() + HOLD_USEC;

   while (   retro_atomic_load_acquire_int(&rd_wholes) < want
          && cpu_features_get_time_usec() < until)
      retro_sleep(1);
   if (retro_atomic_load_acquire_int(&rd_wholes) < want)
      walk_late = true;
}

database_info_index_job_t *__real_database_info_index_job_new(
      const char *rdb_path, size_t max_bytes, unsigned want);
/* When set, the most every index build may take: 1 refuses them all,
 * as a spent or too small budget does. */
static size_t index_cap;

database_info_index_job_t *__wrap_database_info_index_job_new(
      const char *rdb_path, size_t max_bytes, unsigned want)
{
   database_info_index_job_t *job = __real_database_info_index_job_new(
         rdb_path, index_cap ? index_cap : max_bytes, want);

   if (   job && walk_name && strstr(rdb_path, walk_name)
       && __real_task_queue_is_threaded())
   {
      walk_hold(walk_want);
      walk_job   = job;
      walk_steps = 0;
      walk_from  = retro_atomic_load_acquire_int(&rd_wholes);
   }
   return job;
}

bool __real_database_info_index_job_step(database_info_index_job_t *job,
      unsigned records, size_t entries);
bool __wrap_database_info_index_job_step(database_info_index_job_t *job,
      unsigned records, size_t entries)
{
   bool done;

   if (job == walk_job && !walk_steps++ && !walk_late)
   {
      if (walk_more)
         walk_hold(walk_from + walk_more);
      else
         retro_sleep(WALK_IDLE_MS);
   }
   done = __real_database_info_index_job_step(job, records, entries);
   if (done && job == walk_job)
   {
      walk_to  = retro_atomic_load_acquire_int(&rd_wholes);
      walk_job = NULL;
   }
   return done;
}

bool __real_task_nbio_slice_within_budget(void *ud, size_t avail,
      size_t len);
bool __wrap_task_nbio_slice_within_budget(void *ud, size_t avail,
      size_t len)
{
   if (walk_job)
      return true;
   return __real_task_nbio_slice_within_budget(ud, avail, len);
}

/* Appends the reader passes and the holds to a check's detail. */
static void passes_detail(char *s)
{
   sprintf(s + strlen(s), "; passes %d, %d %dus before the scan's; held %d%s",
         retro_atomic_load_acquire_int(&rd_passes), rd_watch_passes,
         rd_watch_gap, retro_atomic_load_acquire_int(&look_hits),
         retro_atomic_load_acquire_int(&look_late) ? ", late" : "");
}

/* Linked with --wrap: directory reads of paths holding stored_name,
 * counted as each returns: a stored-CRC fetch, and a walk the scanner
 * starts itself (its INIT step).  A read of dir_hold's waits first, at
 * most HOLD_USEC, until hold_dirs have.  A walk whose first member is
 * gate_size bytes waits after that member, at most HOLD_USEC, in the
 * gate: until gate_n are in and gate_slow slow reads have begun.  One
 * on the thread that last ran the scan, which gate_on_scan counts,
 * waits only for the first: the scan cannot run.  While share_paths is
 * set, readers' reads of smb:// paths and of any other scheme's are
 * counted in share_reads.  All set only between scans. */
static const char        *stored_name;
static retro_atomic_int_t stored_reads;
static retro_atomic_int_t share_reads[2];
static const char        *dir_hold;
static int                hold_dirs;
static uint64_t           gate_size;
static int                gate_slow;
static retro_atomic_int_t gate_on_scan;

static void stored_read_hold(const char *path)
{
   if (dir_hold && path && strstr(path, dir_hold))
   {
      retro_time_t until = cpu_features_get_time_usec() + HOLD_USEC;

      while (   retro_atomic_load_acquire_int(&stored_reads) < hold_dirs
             && cpu_features_get_time_usec() < until)
         retro_sleep(1);
   }
}

static void stored_read_gate(uint64_t size)
{
   if (gate_n && size == gate_size)
   {
      retro_time_t until   = cpu_features_get_time_usec() + HOLD_USEC;
      bool         on_scan = retro_atomic_load_acquire_size(&scan_thread_tid)
         == (size_t)current_tid();

      if (on_scan)
         retro_atomic_fetch_add_int(&gate_on_scan, 1);
      retro_atomic_fetch_add_int(&gate_in, 1);
      while (   (   retro_atomic_load_acquire_int(&gate_in) < gate_n
                 || (   !on_scan
                     && retro_atomic_load_acquire_int(&slow_in) < gate_slow))
             && cpu_features_get_time_usec() < until)
         retro_sleep(1);
   }
}

static void stored_read_count(const char *path)
{
   if (stored_name && path && strstr(path, stored_name))
      retro_atomic_fetch_add_int(&stored_reads, 1);
   if (   share_paths && path
       && retro_atomic_load_acquire_size(&scan_tick_tid)
          != (size_t)current_tid())
   {
      if (!strncmp(path, "smb://", 6))
         retro_atomic_fetch_add_int(&share_reads[0], 1);
      else if (strstr(path, "://"))
         retro_atomic_fetch_add_int(&share_reads[1], 1);
   }
}

uint32_t __real_file_archive_get_file_crc32_and_size(const char *path,
      uint64_t *size);
uint32_t __wrap_file_archive_get_file_crc32_and_size(const char *path,
      uint64_t *size)
{
   uint32_t crc;

   stored_read_hold(path);
   crc = __real_file_archive_get_file_crc32_and_size(path, size);
   stored_read_count(path);
   return crc;
}

int __real_file_archive_parse_file_iterate(file_archive_transfer_t *state,
      bool *returnerr, const char *file, const char *valid_exts,
      file_archive_file_cb file_cb,
      struct archive_extract_userdata *userdata);
int __wrap_file_archive_parse_file_iterate(file_archive_transfer_t *state,
      bool *returnerr, const char *file, const char *valid_exts,
      file_archive_file_cb file_cb,
      struct archive_extract_userdata *userdata)
{
   bool init  = state && state->type == ARCHIVE_TRANSFER_INIT;
   bool first = state && state->type == ARCHIVE_TRANSFER_ITERATE
      && !state->step_current;
   int  ret;

   if (init)
      stored_read_hold(file);
   ret = __real_file_archive_parse_file_iterate(state, returnerr, file,
         valid_exts, file_cb, userdata);
   if (first && userdata)
      stored_read_gate(userdata->size);
   if (init)
      stored_read_count(file);
   return ret;
}

/* Linked with --wrap: the serial query fails, as on a failed
 * allocation, for the serial hex_fail names, set only between scans. */
static const char        *hex_fail;
static retro_atomic_int_t hex_fails;

char *__real_bin_to_hex_alloc(const uint8_t *data, size_t len);
char *__wrap_bin_to_hex_alloc(const uint8_t *data, size_t len)
{
   if (   hex_fail && len == strlen(hex_fail)
       && !memcmp(data, hex_fail, len))
   {
      retro_atomic_fetch_add_int(&hex_fails, 1);
      return NULL;
   }
   return __real_bin_to_hex_alloc(data, len);
}

size_t __real_fill_pathname_join_delim(char *s, const char *dir,
      const char *path, const char delim, size_t len);
size_t __wrap_fill_pathname_join_delim(char *s, const char *dir,
      const char *path, const char delim, size_t len)
{
   if (step_watch && delim == '#' && dir && strstr(dir, step_watch))
      step_count(&step_joins);
   return __real_fill_pathname_join_delim(s, dir, path, delim, len);
}

/* The last time the scanner asked rm3u_is_m3u() about each order file.
 * Its START step asks for every file, in content-list order, after the
 * directory walk.  wa_last does the same for whole_ahead_lane()'s and
 * hit_clears_lane()'s wa_<k>.zip, k < wa_count, not their members. */
static unsigned  order_stamp;
static unsigned  order_last[2];
static unsigned *wa_last;
static unsigned  wa_count;

bool __real_rm3u_is_m3u(const char *path);
bool __wrap_rm3u_is_m3u(const char *path)
{
   if (path && strstr(path, "reader_order"))
   {
      if (strstr(path, "o_a.rd"))
         order_last[0] = ++order_stamp;
      else if (strstr(path, "o_b.rd"))
         order_last[1] = ++order_stamp;
   }
   if (   wa_last && path && strstr(path, "whole_ahead")
       && !path_contains_compressed_file(path))
   {
      unsigned k = (unsigned)strtoul(path_basename(path) + 3, NULL, 10);
      if (k < wa_count)
         wa_last[k] = ++order_stamp;
   }
   return __real_rm3u_is_m3u(path);
}

/* Reader pushes, the tasks marked parallel: counted, and from the
 * push_fail_from'th on refused; with push_cancel_usec, pushed cancelled
 * to start that long after.  Both set only between scans. */
static unsigned           push_fail_from;
static retro_time_t       push_cancel_usec;
static retro_atomic_int_t pushes_tried;
static retro_atomic_int_t pushes_ok;

bool __real_task_queue_push(retro_task_t *task);
bool __wrap_task_queue_push(retro_task_t *task)
{
   int n;

   if (!(task_get_flags(task) & RETRO_TASK_FLG_PARALLEL))
      return __real_task_queue_push(task);
   n = retro_atomic_fetch_add_int(&pushes_tried, 1) + 1;
   if (push_fail_from && (unsigned)n >= push_fail_from)
      return false;
   retro_atomic_fetch_add_int(&pushes_ok, 1);
   if (push_cancel_usec)
   {
      task_set_flags(task, RETRO_TASK_FLG_CANCELLED, true);
      task->when = cpu_features_get_time_usec() + push_cancel_usec;
   }
   return __real_task_queue_push(task);
}

#ifdef HAVE_THREADS
/* Threaded-queue worker starts: the main thread's creates of the
 * function created while worker_learn was set.  Main thread only. */
static void   (*worker_func)(void*);
static bool     worker_learn;
static unsigned worker_starts;

sthread_t *__real_sthread_create(void (*thread_func)(void*), void *userdata);
sthread_t *__wrap_sthread_create(void (*thread_func)(void*), void *userdata)
{
   sthread_t *thread = __real_sthread_create(thread_func, userdata);

   if (thread && sthread_get_current_thread_id() == main_tid)
   {
      if (worker_learn)
         worker_func = thread_func;
      else if (thread_func == worker_func)
         worker_starts++;
   }
   return thread;
}
#endif

/* ------------------------------------------------------------------
 * crc32, and forcing a buffer to a chosen one
 * ------------------------------------------------------------------ */

static uint32_t crc_table[256];
static uint8_t  crc_top[256];          /* table index by top byte */

static void crc_init(void)
{
   unsigned i, j;
   for (i = 0; i < 256; i++)
   {
      uint32_t c = i;
      for (j = 0; j < 8; j++)
         c = (c >> 1) ^ ((c & 1) ? 0xEDB88320u : 0u);
      crc_table[i] = c;
   }
   for (i = 0; i < 256; i++)
      crc_top[crc_table[i] >> 24] = (uint8_t)i;
}

static uint32_t crc_of(const uint8_t *data, size_t len)
{
   uint32_t reg = 0xFFFFFFFFu;
   size_t   i;
   for (i = 0; i < len; i++)
      reg = (reg >> 8) ^ crc_table[(reg ^ data[i]) & 0xFF];
   return reg ^ 0xFFFFFFFFu;
}

/* Four bytes that, appended to @data, bring its crc32 to @target.
 *
 * Each step of the checksum is reversible: the top byte of the
 * register after a step identifies the table entry used, which gives
 * both the register before it and the byte that was fed in.  Walking
 * four steps back from the wanted value and then forward again from
 * the current one yields the bytes. */
static void crc_force(const uint8_t *data, size_t len, uint32_t target,
      uint8_t out[4])
{
   uint32_t reg = crc_of(data, len) ^ 0xFFFFFFFFu;
   uint32_t r   = target ^ 0xFFFFFFFFu;
   uint8_t  idx[4];
   int      i;

   for (i = 0; i < 4; i++)
   {
      idx[i] = crc_top[r >> 24];
      r      = (r ^ crc_table[idx[i]]) << 8;
   }
   for (i = 3; i >= 0; i--)
   {
      out[3 - i] = (uint8_t)((reg & 0xFF) ^ idx[i]);
      reg        = (reg >> 8) ^ crc_table[idx[i]];
   }
}

/* ------------------------------------------------------------------
 * A minimal .rdb, so the test does not depend on shipped data
 * ------------------------------------------------------------------ */

typedef struct { uint8_t *d; size_t len, cap; } buf_t;

static void bput(buf_t *b, const void *p, size_t n)
{
   if (b->len + n > b->cap)
   {
      size_t want = b->cap ? b->cap * 2 : 256;
      while (want < b->len + n)
         want *= 2;
      b->d   = (uint8_t*)realloc(b->d, want);
      b->cap = want;
   }
   memcpy(b->d + b->len, p, n);
   b->len += n;
}
static void bbyte(buf_t *b, uint8_t v)     { bput(b, &v, 1); }
static void bfixmap(buf_t *b, unsigned n)  { bbyte(b, (uint8_t)(0x80 | n)); }
static void bfixstr(buf_t *b, const char *s)
{
   size_t n = strlen(s);
   bbyte(b, (uint8_t)(0xa0 | n));
   bput(b, s, n);
}
static void bbin4(buf_t *b, uint32_t v)
{
   uint8_t c[4];
   c[0] = (uint8_t)(v >> 24); c[1] = (uint8_t)(v >> 16);
   c[2] = (uint8_t)(v >> 8);  c[3] = (uint8_t)v;
   bbyte(b, 0xc4); bbyte(b, 4); bput(b, c, 4);
}
static void buint32(buf_t *b, uint32_t v)
{
   uint8_t c[4];
   c[0] = (uint8_t)(v >> 24); c[1] = (uint8_t)(v >> 16);
   c[2] = (uint8_t)(v >> 8);  c[3] = (uint8_t)v;
   bbyte(b, 0xce); bput(b, c, 4);
}

static void bbin(buf_t *b, const char *s)
{
   size_t n = strlen(s);
   bbyte(b, 0xc4);
   bbyte(b, (uint8_t)n);
   bput(b, s, n);
}

/* A database record.  Real databases store the serial as binary. */
typedef struct
{
   const char *name;
   const char *serial;
   uint32_t    crc;
   uint32_t    size;
   const char *rom_name;
   bool        unsized;
} db_rec_t;

/* One database holding @n records; a NULL serial or rom_name, or
 * unsized, leaves the key out. */
static int write_db_set(const char *path, const db_rec_t *recs,
      unsigned n)
{
   buf_t    body, meta;
   FILE    *f;
   uint8_t  hdr[16];
   uint64_t off;
   unsigned r;
   int      i;

   memset(&body, 0, sizeof(body));
   memset(&meta, 0, sizeof(meta));

   for (r = 0; r < n; r++)
   {
      bfixmap(&body, 2 + (recs[r].serial ? 1 : 0)
            + (recs[r].rom_name ? 1 : 0) + (recs[r].unsized ? 0 : 1));
      bfixstr(&body, "name"); bfixstr(&body, recs[r].name);
      if (recs[r].rom_name)
      {
         bfixstr(&body, "rom_name");
         bfixstr(&body, recs[r].rom_name);
      }
      if (recs[r].serial)
      {
         bfixstr(&body, "serial");
         bbin(&body, recs[r].serial);
      }
      bfixstr(&body, "crc");  bbin4(&body, recs[r].crc);
      if (!recs[r].unsized)
      {
         bfixstr(&body, "size");
         buint32(&body, recs[r].size);
      }
   }
   bbyte(&body, 0xc0);

   bfixmap(&meta, 1);
   bfixstr(&meta, "count");
   if (n < 0x80)
      bbyte(&meta, (uint8_t)n);
   else
      buint32(&meta, n);

   off = 16 + (uint64_t)body.len;
   memcpy(hdr, "RARCHDB", 7);
   hdr[7] = 0;
   for (i = 0; i < 8; i++)
      hdr[8 + i] = (uint8_t)(off >> (56 - 8 * i));

   if (!(f = fopen(path, "wb")))
   {
      free(body.d);
      free(meta.d);
      return 0;
   }
   fwrite(hdr, 1, sizeof(hdr), f);
   fwrite(body.d, 1, body.len, f);
   fwrite(meta.d, 1, meta.len, f);
   fclose(f);
   free(body.d);
   free(meta.d);
   return 1;
}

/* One database holding a single named record. */
static int write_db(const char *path, const char *game, uint32_t crc,
      uint32_t size)
{
   db_rec_t rec;
   memset(&rec, 0, sizeof(rec));
   rec.name   = game;
   rec.serial = NULL;
   rec.crc    = crc;
   rec.size   = size;
   return write_db_set(path, &rec, 1);
}

/* Content of @size whose crc32 is @crc. */
static int write_content(const char *path, uint32_t crc, uint32_t size)
{
   uint8_t *d = (uint8_t*)malloc(size);
   uint32_t i;
   FILE    *f;

   if (!d || size < 8)
   {
      free(d);
      return 0;
   }
   for (i = 0; i < size - 4; i++)
      d[i] = (uint8_t)((i * 37 + 11) & 0xFF);
   crc_force(d, size - 4, crc, d + size - 4);

   if (crc_of(d, size) != crc)
   {
      free(d);
      return 0;
   }
   if (!(f = fopen(path, "wb")))
   {
      free(d);
      return 0;
   }
   fwrite(d, 1, size, f);
   fclose(f);
   free(d);
   return 1;
}

/* Brings @d's crc32 to @crc through its last four bytes, then writes
 * it out. */
static int write_forced(const char *path, uint8_t *d, uint32_t size,
      uint32_t crc)
{
   FILE *f;

   crc_force(d, size - 4, crc, d + size - 4);
   if (crc_of(d, size) != crc || !(f = fopen(path, "wb")))
      return 0;
   fwrite(d, 1, size, f);
   fclose(f);
   return 1;
}

#define PS1_SECTOR     2352
#define PS1_TRACK_SIZE (26 * PS1_SECTOR)
/* Shorter than the 60000 bytes detect_ps1_game() reads. */
#define PS1_SHORT_SIZE (24 * PS1_SECTOR)

/* A raw MODE2/2352 PS1 data track of @size bytes: the license string
 * detect_system() looks for, and @n strings at the offsets in @at. */
static int write_ps1_track_sized(const char *path, uint32_t size,
      const char *const *str, const unsigned *at, unsigned n, uint32_t crc)
{
   uint8_t *d = (uint8_t*)calloc(1, size);
   unsigned i;
   int      ok;

   if (!d)
      return 0;
   memcpy(d + 0x24f8, "Sony Computer ", 14);
   for (i = 0; i < n; i++)
      memcpy(d + at[i], str[i], strlen(str[i]));
   ok = write_forced(path, d, size, crc);
   free(d);
   return ok;
}

/* A full track: the forced crc lands past the bytes
 * detect_ps1_game() reads. */
static int write_ps1_track(const char *path, const char *const *str,
      const unsigned *at, unsigned n, uint32_t crc)
{
   return write_ps1_track_sized(path, PS1_TRACK_SIZE, str, at, n, crc);
}

static int write_cue(const char *path, const char *track)
{
   FILE *f = fopen(path, "w");

   if (!f)
      return 0;
   fprintf(f,
         "FILE \"%s\" BINARY\n"
         "  TRACK 01 MODE2/2352\n"
         "    INDEX 01 00:00:00\n", track);
   fclose(f);
   return 1;
}

#define CTR_IMAGE_SIZE 0x8000
/* Room for an NCCH at 1 << 25, where flags[6] = 16 would put one. */
#define CTR_BIG_SIZE   0x2000200
/* Ends 0x180 bytes into the NCCH header at 0x4000. */
#define CTR_CUT_SIZE   0x4180

static void put_le32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)v;         p[1] = (uint8_t)(v >> 8);
   p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* A 3DS cart image (NCSD) whose partition 0 starts @part0 media units
 * in, or a bare NCCH when @part0 is 0, with @code as its product code.
 * A zero @ncch_magic leaves the partition without its magic. */
static int write_ctr_sized(const char *path, uint32_t size,
      uint32_t part0, int ncch_magic, const char *code, uint32_t crc)
{
   uint8_t *d = (uint8_t*)calloc(1, size);
   uint8_t *ncch;
   int      ok;

   if (!d)
      return 0;
   ncch = d + part0 * 0x200;
   if (part0)
   {
      memcpy(d + 0x100, "NCSD", 4);
      put_le32(d + 0x104, size / 0x200);
      put_le32(d + 0x120, part0);
      put_le32(d + 0x124, size / 0x200 - part0);
   }
   if (ncch_magic)
      memcpy(ncch + 0x100, "NCCH", 4);
   memcpy(ncch + 0x150, code, strlen(code));
   ok = write_forced(path, d, size, crc);
   free(d);
   return ok;
}

static int write_ctr(const char *path, uint32_t part0, int ncch_magic,
      const char *code, uint32_t crc)
{
   return write_ctr_sized(path, CTR_IMAGE_SIZE, part0, ncch_magic, code,
         crc);
}

/* A cart image @size bytes long whose table puts partition 0 at @part0
 * units of 0x200 << @shift, with an NCCH header at @ncch_at (where it
 * fits) holding @len bytes of @code. */
static int write_ctr_ncsd(const char *path, uint32_t size, uint8_t shift,
      uint32_t part0, uint32_t ncch_at, const char *code, size_t len,
      uint32_t crc)
{
   uint8_t *d = (uint8_t*)calloc(1, size);
   int      ok;

   if (!d)
      return 0;
   memcpy(d + 0x100, "NCSD", 4);
   put_le32(d + 0x104, size / 0x200);
   d[0x18E] = shift;
   put_le32(d + 0x120, part0);
   if (ncch_at + 0x104 <= size)
      memcpy(d + ncch_at + 0x100, "NCCH", 4);
   if (ncch_at + 0x150 + len <= size)
      memcpy(d + ncch_at + 0x150, code, len);
   ok = write_forced(path, d, size, crc);
   free(d);
   return ok;
}

/* A CIA: its header size word, and neither NCSD nor NCCH magic. */
static int write_cia(const char *path, uint32_t crc)
{
   uint8_t *d = (uint8_t*)calloc(1, CTR_IMAGE_SIZE);
   int      ok;

   if (!d)
      return 0;
   put_le32(d, 0x2020);
   ok = write_forced(path, d, CTR_IMAGE_SIZE, crc);
   free(d);
   return ok;
}

/* A zip entry standing for @len octets with crc32 @crc: @data itself
 * when @method is 0 (stored), else @data is @clen octets compressed
 * with @method.  A name ending in '/' is a directory: no data, and a
 * stored CRC of 0. */
typedef struct
{
   const char    *name;
   const uint8_t *data;
   uint32_t       len;
   uint32_t       crc;
   unsigned       method;
   uint32_t       clen;
} zip_entry_t;

/* Little-endian fixture fields. */
#define PUT16(p, v) do { (p)[0] = (uint8_t)(v); \
   (p)[1] = (uint8_t)((v) >> 8); } while (0)
#define PUT32(p, v) do { (p)[0] = (uint8_t)(v); \
   (p)[1] = (uint8_t)((v) >> 8); (p)[2] = (uint8_t)((v) >> 16); \
   (p)[3] = (uint8_t)((v) >> 24); } while (0)

/* The end record's comment of the zips write_zip_entries() writes, or
 * NULL.  Set only between scans. */
static const char *zip_comment;

/* A zip of @n entries.  Written by hand because the point is to
 * exercise the scanner's archive handling, not to depend on a fixture
 * checked into the tree. */
static int write_zip_entries(const char *path, const zip_entry_t *e,
      unsigned n)
{
   FILE    *f = fopen(path, "wb");
   uint8_t  h[46];
   uint32_t local_off = 0, dir_size = 0;
   unsigned i;

   if (!f)
      return 0;

   /* local file headers */
   for (i = 0; i < n; i++)
   {
      uint16_t nlen = (uint16_t)strlen(e[i].name);
      uint32_t clen = e[i].method ? e[i].clen : e[i].len;

      memset(h, 0, 30);
      PUT32(h,      0x04034b50); PUT16(h + 4,  20); PUT16(h + 6, 0);
      PUT16(h + 8,  e[i].method);
      PUT16(h + 10, 0); PUT16(h + 12, 0);
      PUT32(h + 14, e[i].crc); PUT32(h + 18, clen); PUT32(h + 22, e[i].len);
      PUT16(h + 26, nlen); PUT16(h + 28, 0);
      fwrite(h, 1, 30, f);
      fwrite(e[i].name, 1, nlen, f);
      if (clen)
         fwrite(e[i].data, 1, clen, f);
   }

   /* central directory */
   for (i = 0; i < n; i++)
   {
      uint16_t nlen = (uint16_t)strlen(e[i].name);
      uint32_t clen = e[i].method ? e[i].clen : e[i].len;

      memset(h, 0, 46);
      PUT32(h,      0x02014b50); PUT16(h + 4, 20); PUT16(h + 6, 20);
      PUT16(h + 8,  0); PUT16(h + 10, e[i].method);
      PUT16(h + 12, 0); PUT16(h + 14, 0);
      PUT32(h + 16, e[i].crc); PUT32(h + 20, clen); PUT32(h + 24, e[i].len);
      PUT16(h + 28, nlen); PUT16(h + 30, 0); PUT16(h + 32, 0);
      PUT16(h + 34, 0); PUT16(h + 36, 0);
      PUT32(h + 38, e[i].name[nlen - 1] == '/' ? 0x10 : 0);
      PUT32(h + 42, local_off);
      fwrite(h, 1, 46, f);
      fwrite(e[i].name, 1, nlen, f);
      local_off += 30 + nlen + clen;
      dir_size  += 46 + nlen;
   }

   /* end of central directory */
   memset(h, 0, 22);
   PUT32(h,      0x06054b50);
   PUT16(h + 8,  n); PUT16(h + 10, n);
   PUT32(h + 12, dir_size);
   PUT32(h + 16, local_off);
   if (zip_comment)
      PUT16(h + 20, strlen(zip_comment));
   fwrite(h, 1, 22, f);
   if (zip_comment)
      fwrite(zip_comment, 1, strlen(zip_comment), f);
   fclose(f);
   return 1;
}

/* A one-entry zip whose member @data (@clen octets) is stored with
 * @method and stands for @len octets with crc32 @crc. */
static int write_zip_method(const char *path, const char *member,
      unsigned method, const uint8_t *data, uint32_t clen, uint32_t len,
      uint32_t crc)
{
   zip_entry_t e;

   e.name   = member;
   e.data   = data;
   e.len    = len;
   e.crc    = crc;
   e.method = method;
   e.clen   = clen;
   return write_zip_entries(path, &e, 1);
}

/* The same with the member stored as it is */
static int write_zip(const char *path, const char *member,
      const uint8_t *data, uint32_t len, uint32_t crc)
{
   return write_zip_method(path, member, 0, data, len, len, crc);
}

static int write_bytes(const char *path, const uint8_t *d, size_t n)
{
   FILE *f = fopen(path, "wb");
   int   ok;
   if (!f)
      return 0;
   ok = fwrite(d, 1, n, f) == n;
   return (fclose(f) == 0) && ok;
}

#ifdef HAVE_7ZIP
/* A 7z NUMBER: the high bits of the first byte count the bytes after
 * it. */
static unsigned put_7z_num(uint8_t *p, uint32_t v)
{
   if (v < 0x80)
   {
      p[0] = (uint8_t)v;
      return 1;
   }
   if (v < 0x4000)
   {
      p[0] = (uint8_t)(0x80 | (v >> 8));
      p[1] = (uint8_t)v;
      return 2;
   }
   p[0] = 0xF0;
   put_le32(p + 1, v);
   return 5;
}

/* A 7z archive of up to 8 entries, written by hand as the zip is: the
 * signature header, each entry's data as a Copy folder of its own (as
 * 7-Zip stores Copy), then a plain header with their sizes, CRCs, names
 * and attributes.  A name ending in '/' is a directory, stored as 7-Zip
 * stores one: an empty stream that is not an empty file, attribute
 * 0x10, the name without the '/'. */
static int write_7z_entries(const char *path, const zip_entry_t *e,
      unsigned n)
{
   static const uint8_t sig[8] = { '7', 'z', 0xBC, 0xAF, 0x27, 0x1C, 0, 4 };
   FILE    *f;
   uint8_t  s[32], h[1024];
   uint8_t  dirs = 0, empty = 0, empty_file = 0;
   uint32_t total = 0;
   unsigned i, j, len, nlen, streams = 0, empties = 0;
   unsigned names = 1;
   int      ok;

   if (n > 8)
      return 0;
   for (i = 0; i < n; i++)
   {
      nlen = (unsigned)strlen(e[i].name);
      if (nlen > 32)
         return 0;
      if (nlen && e[i].name[nlen - 1] == '/')
      {
         dirs |= (uint8_t)(1 << i);
         nlen--;
      }
      names += 2 * (nlen + 1);
      if (e[i].len)
      {
         total += e[i].len;
         streams++;
         continue;
      }
      empty |= (uint8_t)(0x80 >> i);
      if (!(dirs & (1 << i)))
         empty_file |= (uint8_t)(0x80 >> empties);
      empties++;
   }
   if (!streams)
      return 0;

   /* Header, MainStreamsInfo, PackInfo: the packed streams from 0. */
   memcpy(h, "\x01\x04\x06\x00", 4);
   len = 4;
   h[len++] = (uint8_t)streams;
   h[len++] = 0x09;
   for (i = 0; i < n; i++)
      if (e[i].len)
         len += put_7z_num(h + len, e[i].len);
   /* UnpackInfo: a folder of one Copy coder for each, and its size. */
   memcpy(h + len, "\x00\x07\x0B", 3);
   len += 3;
   h[len++] = (uint8_t)streams;
   h[len++] = 0;
   for (i = 0; i < streams; i++)
   {
      memcpy(h + len, "\x01\x01\x00", 3);
      len += 3;
   }
   h[len++] = 0x0C;
   for (i = 0; i < n; i++)
      if (e[i].len)
         len += put_7z_num(h + len, e[i].len);
   /* SubStreamsInfo: every CRC. */
   memcpy(h + len, "\x00\x08\x0A\x01", 4);
   len += 4;
   for (i = 0; i < n; i++)
      if (e[i].len)
      {
         put_le32(h + len, e[i].crc);
         len += 4;
      }
   /* FilesInfo: EmptyStream, EmptyFile, Name, Attributes. */
   memcpy(h + len, "\x00\x00\x05", 3);
   len += 3;
   h[len++] = (uint8_t)n;
   if (empties)
   {
      h[len++] = 0x0E;
      h[len++] = 1;
      h[len++] = empty;
      h[len++] = 0x0F;
      h[len++] = 1;
      h[len++] = empty_file;
   }
   h[len++] = 0x11;
   len += put_7z_num(h + len, names);
   h[len++] = 0;
   for (i = 0; i < n; i++)
   {
      nlen = (unsigned)strlen(e[i].name) - ((dirs >> i) & 1);
      for (j = 0; j < nlen; j++)
      {
         h[len++] = (uint8_t)e[i].name[j];
         h[len++] = 0;
      }
      h[len++] = 0;
      h[len++] = 0;
   }
   h[len++] = 0x15;
   len += put_7z_num(h + len, 2 + 4 * n);
   h[len++] = 1;
   h[len++] = 0;
   for (i = 0; i < n; i++)
   {
      put_le32(h + len, ((dirs >> i) & 1) ? 0x10 : 0x20);
      len += 4;
   }
   h[len++] = 0;
   h[len++] = 0;

   /* The header follows the data. */
   memset(s, 0, sizeof(s));
   memcpy(s, sig, sizeof(sig));
   put_le32(s + 12, total);
   put_le32(s + 20, len);
   put_le32(s + 28, crc_of(h, len));
   put_le32(s + 8,  crc_of(s + 12, 20));

   if (!(f = fopen(path, "wb")))
      return 0;
   ok = fwrite(s, 1, sizeof(s), f) == sizeof(s);
   for (i = 0; ok && i < n; i++)
      if (e[i].len)
         ok = fwrite(e[i].data, 1, e[i].len, f) == e[i].len;
   ok = ok && fwrite(h, 1, len, f) == len;
   return (fclose(f) == 0) && ok;
}

/* A 7z of one member stored by the copy coder, its CRC in the header
 * as a zip's is in its directory. */
static int write_7z(const char *path, const char *member,
      const uint8_t *data, uint32_t len, uint32_t crc)
{
   zip_entry_t e;

   memset(&e, 0, sizeof(e));
   e.name = member;
   e.data = data;
   e.len  = len;
   e.crc  = crc;
   return write_7z_entries(path, &e, 1);
}
#endif

/* The size write_zip_entries() gives @n entries. */
static uint32_t zip_bytes(const zip_entry_t *e, unsigned n)
{
   uint32_t total = 22 + (zip_comment ? (uint32_t)strlen(zip_comment) : 0);
   unsigned i;

   for (i = 0; i < n; i++)
      total += 30 + 46 + 2 * (uint32_t)strlen(e[i].name) + e[i].len;
   return total;
}

#ifdef HAVE_RRAR
/* The size write_rar_entries() gives @n entries. */
static uint32_t rar_bytes(const zip_entry_t *e, unsigned n)
{
   uint32_t total = 7 + 13 + 7;
   unsigned i;

   for (i = 0; i < n; i++)
   {
      uint32_t nlen = (uint32_t)strlen(e[i].name);

      if (nlen && e[i].name[nlen - 1] == '/')
         nlen--;
      total += 32 + nlen + e[i].len;
   }
   return total;
}

/* A RAR 4 archive of @n stored entries, written by hand as the zip is:
 * the marker, a main header, a file header and the data for each
 * entry, then the end block.  A header's CRC field is the low 16 bits
 * of the CRC-32 of the rest of that header.  A name ending in '/' is
 * a directory, stored without the '/'. */
static int write_rar_entries(const char *path, const zip_entry_t *e,
      unsigned n)
{
   static const uint8_t marker[7] = { 'R', 'a', 'r', '!', 0x1A, 0x07, 0x00 };
   FILE    *f;
   uint8_t  h[32 + 64];
   uint32_t c;
   unsigned i;

   for (i = 0; i < n; i++)
      if (strlen(e[i].name) > 64)
         return 0;
   if (!(f = fopen(path, "wb")))
      return 0;

   fwrite(marker, 1, sizeof(marker), f);

   memset(h, 0, 13);
   h[2] = 0x73;                     /* main header */
   PUT16(h + 5, 13);
   c = crc_of(h + 2, 11);
   PUT16(h, c);
   fwrite(h, 1, 13, f);

   for (i = 0; i < n; i++)
   {
      uint16_t nlen = (uint16_t)strlen(e[i].name);
      bool     dir  = nlen && e[i].name[nlen - 1] == '/';
      uint16_t size;

      if (dir)
         nlen--;
      size = (uint16_t)(32 + nlen);
      memset(h, 0, 32);
      h[2] = 0x74;                  /* file header */
      /* Data follows; all the dictionary bits mark a directory. */
      PUT16(h + 3, dir ? 0x80E0 : 0x8000);
      PUT16(h + 5, size);
      PUT32(h + 7,  e[i].len);      /* packed */
      PUT32(h + 11, e[i].len);      /* unpacked */
      PUT32(h + 16, e[i].crc);
      h[24] = 29;                   /* unpack version */
      h[25] = 0x30;                 /* stored */
      PUT16(h + 26, nlen);
      PUT32(h + 28, dir ? 0x10 : 0x20); /* directory, archive */
      memcpy(h + 32, e[i].name, nlen);
      c = crc_of(h + 2, size - 2);
      PUT16(h, c);
      fwrite(h, 1, size, f);
      if (e[i].len)
         fwrite(e[i].data, 1, e[i].len, f);
   }

   memset(h, 0, 7);
   h[2] = 0x7B;                     /* end of archive */
   PUT16(h + 3, 0x4000);
   PUT16(h + 5, 7);
   c = crc_of(h + 2, 5);
   PUT16(h, c);
   fwrite(h, 1, 7, f);
   if (fflush(f) != 0 || ferror(f))
   {
      fclose(f);
      return 0;
   }
   fclose(f);
   return 1;
}

/* A RAR 5 variable-length number, 7 bits a byte, low first. */
static size_t rar5_vint(uint8_t *p, uint64_t v)
{
   size_t n = 0;

   do
   {
      p[n] = (uint8_t)(v & 0x7f);
      v  >>= 7;
      if (v)
         p[n] |= 0x80;
      n++;
   } while (v);
   return n;
}

/* A RAR 5 header: the CRC-32 of the rest of it, its size, then @body. */
static void rar5_head(FILE *f, const uint8_t *body, size_t len)
{
   uint8_t  h[300];
   size_t   s = rar5_vint(h + 4, len);
   uint32_t c;

   memcpy(h + 4 + s, body, len);
   c = crc_of(h + 4, s + len);
   PUT32(h, c);
   fwrite(h, 1, 4 + s + len, f);
}

/* A RAR 5 archive of @n stored entries, written by hand as the zip is.
 * An entry whose crc is 0 has no CRC field, as a BLAKE2 member has
 * none: the reader unpacks it for one. */
static int write_rar5_entries(const char *path, const zip_entry_t *e,
      unsigned n)
{
   static const uint8_t sig[8]       = {
      'R', 'a', 'r', '!', 0x1A, 0x07, 0x01, 0x00 };
   static const uint8_t main_head[3] = { 1, 0, 0 };
   static const uint8_t end_head[3]  = { 5, 0, 0 };
   FILE    *f;
   unsigned i;

   if (!(f = fopen(path, "wb")))
      return 0;
   fwrite(sig, 1, sizeof(sig), f);
   rar5_head(f, main_head, sizeof(main_head));
   for (i = 0; i < n; i++)
   {
      uint8_t b[256];
      size_t  q    = 0;
      size_t  nlen = strlen(e[i].name);

      if (nlen > 200)
      {
         fclose(f);
         return 0;
      }
      q += rar5_vint(b + q, 2);                /* a file */
      q += rar5_vint(b + q, 2);                /* data follows */
      q += rar5_vint(b + q, e[i].len);
      q += rar5_vint(b + q, e[i].crc ? 4 : 0); /* a CRC field */
      q += rar5_vint(b + q, e[i].len);         /* unpacked */
      q += rar5_vint(b + q, 0);                /* attributes */
      if (e[i].crc)
      {
         PUT32(b + q, e[i].crc);
         q += 4;
      }
      q += rar5_vint(b + q, 0);                /* stored */
      q += rar5_vint(b + q, 0);                /* host */
      q += rar5_vint(b + q, nlen);
      memcpy(b + q, e[i].name, nlen);
      q += nlen;
      rar5_head(f, b, q);
      fwrite(e[i].data, 1, e[i].len, f);
   }
   rar5_head(f, end_head, sizeof(end_head));
   if (fflush(f) != 0 || ferror(f))
   {
      fclose(f);
      return 0;
   }
   fclose(f);
   return 1;
}
#endif

static int file_contains(const char *path, const char *needle)
{
   int64_t  len  = 0;
   char    *data = NULL;
   int      hit;

   if (!filestream_read_file(path, (void**)&data, &len) || !data)
      return 0;
   hit = (strstr(data, needle) != NULL);
   free(data);
   return hit;
}

/* ------------------------------------------------------------------ */

static bool loop_active   = true;
static bool scan_completed = false;
static retro_time_t cb_usec;

static void scan_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   (void)task; (void)task_data; (void)user_data;
   if (err && *err)
      fprintf(stderr, "  scan reported an error: %s\n", err);
   retro_atomic_store_release_int(&cb_fired, 1);
   cb_usec        = cpu_features_get_time_usec();
   scan_completed = true;
   loop_active    = false;
}

static void msgq_push(retro_task_t *task, const char *msg,
      unsigned prio, unsigned duration, bool flush)
{
   (void)task; (void)msg; (void)prio; (void)duration; (void)flush;
}

/* Returns false if the scan could not be started; otherwise waits for
 * it, and scan_completed says whether it finished in time. */
static bool run_scan(const char *dir)
{
   time_t started;

   loop_active    = true;
   scan_completed = false;
   retro_atomic_store_release_int(&rd_armed,  0);
   retro_atomic_store_release_int(&rd_passes, 0);
   retro_atomic_store_release_int(&rd_last,   0);
   rd_t0           = cpu_features_get_time_usec();
   rd_watch_passes = -1;
   rd_watch_gap    = 0;
   retro_atomic_store_release_int(&scan_wholes,     0);
   retro_atomic_store_release_int(&rd_wholes,       0);
   retro_atomic_store_release_int(&rd_watch_wholes, 0);
   retro_atomic_store_release_int(&look_hits,       0);
   retro_atomic_store_release_int(&look_late,       0);
   if (!task_push_dbscan(dir, scan_cb))
      return false;

   started = time(NULL);
   while (loop_active)
   {
      task_queue_check();
      if (difftime(time(NULL), started) > SCAN_TIMEOUT_SECONDS)
         break;
      retro_sleep(1);
   }
   return true;
}

/* As run_scan(), but counts the checks the scan took, less the ones
 * that parked: on the regular queue every check runs the handler
 * once, a spent window holds that call to one step, and a call that
 * parks waits on the readers.  The first park at the end also makes
 * the end's step, so the count can be one short. */
static unsigned run_scan_counted(const char *dir)
{
   unsigned gathers = 0;
   int      parks   = retro_atomic_load_acquire_int(&scan_parks);
   time_t   started;

   loop_active    = true;
   scan_completed = false;
   if (!task_push_dbscan(dir, scan_cb))
      return 0;

   started = time(NULL);
   while (loop_active)
   {
      spend_window();
      check_seq++;
      task_queue_check();
      gathers++;
      if (difftime(time(NULL), started) > SCAN_TIMEOUT_SECONDS)
         break;
   }
   return gathers
      - (unsigned)(retro_atomic_load_acquire_int(&scan_parks) - parks);
}

/* Databases no core claims, and files that match nothing, in the
 * lane below. */
#define UNCLAIMED_DBS   48
#define UNMATCHED_FILES 4

/* 560 sectors, 00:07:35, then ten of audio: over 1 MiB, so the CRCs
 * take several quanta. */
#define STEP_TRACK_SIZE (560 * 2352)
#define STEP_AUDIO_SIZE (10 * 2352)
#define STEP_CTR_SIZE   0x140000

/* Databases in the walk lane below. */
#define WALK_DBS 4

/* A database is walked once.  A cue sheet, which the scan takes
 * first, fills the one claimed database through the crc lookup: its
 * track has no serial.  Then a 3DS file that matches and one that
 * matches nothing take the serial lookup through all four, in
 * whatever order the directory lists them.  Two carry no serials, as
 * Mobile - J2ME does. */
static void walk_lane(const char *root, const char *pl_dir,
      const char *db_dir)
{
   static const db_rec_t other_recs[] = {
      { "Other The Game", "CTR-P-OTHR", 0x3A1C0002u, CTR_IMAGE_SIZE }
   };
   static const db_rec_t walk_recs[] = {
      { "Walk The Game",  "CTR-P-WALK", 0x3A1C0003u, CTR_IMAGE_SIZE }
   };
   char    *walk_db = (char*)malloc(512);
   char    *walk_in = (char*)malloc(512);
   char    *p       = (char*)malloc(1024);
   char     detail[64];
   bool     started;
   unsigned opens;

   if (!walk_db || !walk_in || !p)
   {
      check(0, "walk lane", "out of memory");
      goto end;
   }

   sprintf(walk_db, "%s/scan_db_walk", root);
   sprintf(walk_in, "%s/scan_walk",    root);
   path_mkdir(walk_db);
   path_mkdir(walk_in);

   /* The test core claims Test Alpha for .cue. */
   sprintf(p, "%s/Test Alpha.rdb", walk_db);
   if (!write_db(p, "Walk Disc Game", 0x3A1C0001u, 2 * PS1_SECTOR))
   { check(0, "fixture", "could not write database"); goto end; }
   sprintf(p, "%s/Walk Mobile.rdb", walk_db);
   if (!write_db(p, "Mobile The Game", 0x3A1C0004u, 4096))
   { check(0, "fixture", "could not write database"); goto end; }
   sprintf(p, "%s/Walk Other.rdb", walk_db);
   if (!write_db_set(p, other_recs, ARRAY_SIZE(other_recs)))
   { check(0, "fixture", "could not write database"); goto end; }
   sprintf(p, "%s/Walk Serial.rdb", walk_db);
   if (!write_db_set(p, walk_recs, ARRAY_SIZE(walk_recs)))
   { check(0, "fixture", "could not write database"); goto end; }

   sprintf(p, "%s/40_walk.bin", walk_in);
   if (!write_content(p, 0x3A1C0001u, 2 * PS1_SECTOR))
   { check(0, "fixture", "crc forcing failed"); goto end; }
   sprintf(p, "%s/40_walk.cue", walk_in);
   if (!write_cue(p, "40_walk.bin"))
   { check(0, "fixture", "could not write cue"); goto end; }
   sprintf(p, "%s/41_walk.3ds", walk_in);
   if (!write_ctr(p, 0x20, 1, "CTR-P-WALK", 0x41414141u))
   { check(0, "fixture", "could not write image"); goto end; }
   sprintf(p, "%s/42_walk.3ds", walk_in);
   if (!write_ctr(p, 0x20, 1, "CTR-P-NONE", 0x42424242u))
   { check(0, "fixture", "could not write image"); goto end; }

   strlcpy(config_get_ptr()->paths.path_content_database, walk_db,
         sizeof(config_get_ptr()->paths.path_content_database));
   rdb_opens = 0;
   started   = run_scan(walk_in);
   opens     = rdb_opens;
   strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
         sizeof(config_get_ptr()->paths.path_content_database));
   if (!started)
   {
      check(0, "walk scan started", "task_push_dbscan refused");
      goto end;
   }

   printf("  info  database walks over %u databases: %u\n", WALK_DBS,
         opens);
   check(scan_completed, "walk scan completed",
         scan_completed ? "callback fired" : "timed out");
   sprintf(detail, "%u walks for %u databases", opens, WALK_DBS);
   check(opens == WALK_DBS, "one walk per database", detail);
   sprintf(p, "%s/Walk Serial.lpl", pl_dir);
   check(file_contains(p, "41_walk.3ds"),
         "walked serial index matches", "41_walk.3ds");
   sprintf(p, "%s/Test Alpha.lpl", pl_dir);
   check(file_contains(p, "40_walk.cue"),
         "crc lookup ahead of the walk", "40_walk.cue");

end:
   free(walk_db);
   free(walk_in);
   free(p);
}

/* The folder a playlist refreshes from, its default core and
 * whether it recurses.  False when the playlist holds no scan record. */
static bool scan_record(const char *lpl, char *dir, char *core,
      size_t len, bool *recursive)
{
   playlist_config_t *cfg = (playlist_config_t*)calloc(1, sizeof(*cfg));
   playlist_t        *pl  = NULL;
   const char        *d   = NULL;
   bool               ok  = false;

   dir[0]  = '\0';
   core[0] = '\0';
   if (!cfg)
      return false;
   playlist_config_set_path(cfg, lpl);
   cfg->capacity = COLLECTION_SIZE;
   if ((pl = playlist_init(cfg)))
   {
      if ((d = playlist_get_scan_content_dir(pl)) && *d)
      {
         strlcpy(dir, d, len);
         if ((d = playlist_get_default_core_name(pl)))
            strlcpy(core, d, len);
         *recursive = playlist_get_scan_search_recursively(pl);
         ok         = true;
      }
      playlist_free(pl);
   }
   free(cfg);
   return ok;
}

/* Scans @dir while it is renamed away, as on a drive that is offline.
 * @gone is scratch for the path it is renamed to. */
static bool scan_gone(const char *dir, char *gone)
{
   bool ran;

   sprintf(gone, "%s_gone", dir);
   ran = !filestream_rename(dir, gone) && run_scan(dir) && scan_completed;
   return !filestream_rename(gone, dir) && ran;
}

/* A file given as the content location is only added: Overwrite is
 * not applied to it, the playlist keeps the folder and settings it
 * refreshes with, and with no database the file is filed under its
 * folder's name, as a scan of that folder files it. */
static void single_lane(const char *root, const char *pl_dir,
      uint32_t crc_a, uint32_t sz_a)
{
   char *in_dir  = (char*)malloc(512);
   char *one_dir = (char*)malloc(512);
   char *one_pl  = (char*)malloc(512);
   char *file    = (char*)malloc(1024);
   char *p       = (char*)malloc(1024);
   char *rec     = (char*)malloc(1024);
   char *core    = (char*)malloc(1024);
   char *kept    = (char*)malloc(1024);
   bool  recursive = false;
   bool  recorded;

   if (   !in_dir || !one_dir || !one_pl || !file || !p || !rec || !core
       || !kept)
   {
      check(0, "single file lane", "out of memory");
      goto end;
   }

   sprintf(in_dir,  "%s/scan_single",     root);
   sprintf(one_dir, "%s/scan_single_one", root);
   sprintf(one_pl,  "%s/scan_single_pl",  root);
   path_mkdir(in_dir);
   path_mkdir(one_dir);
   path_mkdir(one_pl);

   sprintf(p, "%s/50_single.bin", in_dir);
   if (!write_content(p, crc_a, sz_a))
   { check(0, "fixture", "crc forcing failed"); goto end; }
   sprintf(p, "%s/51_one.bin", one_dir);
   if (!write_content(p, crc_a, sz_a))
   { check(0, "fixture", "crc forcing failed"); goto end; }
   sprintf(p, "%s/52_named.bin", one_dir);
   if (!write_content(p, 0x52A3ED00u, 1024))
   { check(0, "fixture", "crc forcing failed"); goto end; }
   sprintf(p, "%s/53_core.bin", one_dir);
   if (!write_content(p, crc_a, sz_a))
   { check(0, "fixture", "crc forcing failed"); goto end; }

   strlcpy(config_get_ptr()->paths.directory_playlist, one_pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   sprintf(file, "%s/51_one.bin", one_dir);

   /* Automatic: the database names the playlist. */
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC);
   if (!run_scan(in_dir) || !scan_completed || !run_scan(file)
         || !scan_completed)
   {
      check(0, "single file scans ran", "refused or timed out");
      goto restore;
   }
   sprintf(p, "%s/Test Alpha.lpl", one_pl);
   check(   file_contains(p, "51_one.bin")
         && file_contains(p, "50_single.bin"),
         "one file matched into its playlist", "Test Alpha");
   recorded = scan_record(p, rec, core, 1024, &recursive);
   check(recorded && !strcmp(rec, in_dir) && recursive,
         "file scan keeps the folder it refreshes", path_basename(rec));

   /* Custom with the database and a core set: the playlist keeps the
    * default core it had. */
   strlcpy(kept, core, 1024);
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_CUSTOM);
   manual_content_scan_set_menu_core_name(MANUAL_CONTENT_SCAN_CORE_SET,
         "Scan Test");
   sprintf(file, "%s/53_core.bin", one_dir);
   if (!run_scan(file) || !scan_completed)
   {
      check(0, "core set file scan ran", "refused or timed out");
      goto restore;
   }
   sprintf(file, "%s/51_one.bin", one_dir);
   recorded = scan_record(p, rec, core, 1024, &recursive);
   check(   file_contains(p, "53_core.bin") && recorded
         && !strcmp(core, kept) && strcmp(core, "Scan Test"),
         "and its default core with a core set", core);

   /* Custom, no database, one named playlist, Overwrite on.  The
    * folder is scanned with one core and the file with another. */
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_CUSTOM);
   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_NONE);
   strlcpy(manual_content_scan_get_system_name_custom_ptr(),
         "Single Custom",
         manual_content_scan_get_system_name_custom_size());
   manual_content_scan_set_menu_system_name(
         MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM, NULL);
   *manual_content_scan_get_overwrite_playlist_ptr() = true;
   manual_content_scan_set_menu_core_name(MANUAL_CONTENT_SCAN_CORE_SET,
         "Scan Test");
   if (!run_scan(in_dir) || !scan_completed)
   {
      check(0, "overwrite scans ran", "refused or timed out");
      goto restore;
   }
   manual_content_scan_set_menu_core_name(MANUAL_CONTENT_SCAN_CORE_SET,
         "Gamma Test");
   if (!run_scan(file) || !scan_completed)
   {
      check(0, "overwrite scans ran", "refused or timed out");
      goto restore;
   }
   sprintf(p, "%s/Single Custom.lpl", one_pl);
   check(   file_contains(p, "51_one.bin")
         && file_contains(p, "50_single.bin"),
         "Overwrite leaves a file scan's playlist", "both entries");
   /* A folder that is gone counts as a file, so its playlist keeps
    * its entries and its record. */
   check(   scan_gone(in_dir, rec)
         && file_contains(p, "51_one.bin")
         && file_contains(p, "50_single.bin"),
         "and a rescan of a folder that is gone", "both entries");
   recorded = scan_record(p, rec, core, 1024, &recursive);
   check(recorded && !strcmp(rec, in_dir),
         "and the folder it refreshes", path_basename(rec));
   check(recorded && !strcmp(core, "Scan Test"),
         "and its default core", core);

   /* Validate prunes by the scan's extensions, here .gam alone */
   *manual_content_scan_get_overwrite_playlist_ptr() = false;
   *manual_content_scan_get_validate_entries_ptr()   = true;
   if (!run_scan(file) || !scan_completed)
   {
      check(0, "validate scan ran", "refused or timed out");
      goto restore;
   }
   check(   file_contains(p, "51_one.bin")
         && file_contains(p, "50_single.bin"),
         "Validate leaves a file scan's playlist", "both entries");
   check(   scan_gone(in_dir, rec)
         && file_contains(p, "51_one.bin")
         && file_contains(p, "50_single.bin"),
         "and a rescan of a folder that is gone", "both entries");
   *manual_content_scan_get_validate_entries_ptr() = false;

   /* No database, Target Playlist on Auto. */
   manual_content_scan_set_menu_core_name(MANUAL_CONTENT_SCAN_CORE_DETECT,
         NULL);
   manual_content_scan_set_menu_system_name(
         MANUAL_CONTENT_SCAN_SYSTEM_NAME_AUTO, NULL);
   sprintf(file, "%s/52_named.bin", one_dir);
   if (!run_scan(file) || !scan_completed)
   {
      check(0, "auto named scan ran", "refused or timed out");
      goto restore;
   }
   sprintf(p, "%s/scan_single_one.lpl", one_pl);
   sprintf(rec, "%s/52_named.lpl", one_pl);
   check(   file_contains(p, "52_named.bin") && !path_is_valid(rec)
         && !scan_record(p, rec, core, 1024, &recursive),
         "a file is filed under its folder's name", "with no record");

restore:
   *manual_content_scan_get_validate_entries_ptr() = false;
   *manual_content_scan_get_overwrite_playlist_ptr() = false;
   manual_content_scan_set_menu_core_name(MANUAL_CONTENT_SCAN_CORE_DETECT,
         NULL);
   manual_content_scan_get_system_name_custom_ptr()[0] = '\0';
   manual_content_scan_set_menu_system_name(
         MANUAL_CONTENT_SCAN_SYSTEM_NAME_AUTO, NULL);
   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_STRICT);
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC);
   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
end:
   free(in_dir);
   free(one_dir);
   free(one_pl);
   free(file);
   free(p);
   free(rec);
   free(core);
   free(kept);
}

/* The queue the next scans run on: threaded where threads are built,
 * else the regular one, so every build runs the same checks. */
static void use_queue(bool threaded)
{
   task_queue_deinit();
#ifdef HAVE_THREADS
   task_queue_init(threaded, msgq_push);
#else
   (void)threaded;
   task_queue_init(false, msgq_push);
#endif
}

static uint32_t lane_ident;
static int      lane_progress;

/* The lanes run one scan at a time; its readers are passed over. */
static bool find_scan(retro_task_t *task, void *user_data)
{
   (void)user_data;
   if (task_get_flags(task) & RETRO_TASK_FLG_PARALLEL)
      return false;
   lane_ident    = task->ident;
   lane_progress = task_get_progress(task);
   return true;
}

/* As run_scan(), but cancels once progress is above 0, or with
 * @on_stuck once a slow read has begun.  Returns the microseconds from
 * the cancel to the callback, or -1. */
static retro_time_t run_scan_cancelled(const char *dir, bool on_stuck)
{
   task_finder_data_t finder;
   retro_time_t       cancelled = 0;
   time_t             started;

   loop_active    = true;
   scan_completed = false;
   if (!task_push_dbscan(dir, scan_cb))
      return -1;
   finder.func     = find_scan;
   finder.userdata = NULL;
   started         = time(NULL);
   while (loop_active)
   {
      if (   !cancelled
          && (on_stuck ? retro_atomic_load_acquire_int(&slow_in) > 0
                       : (task_queue_find(&finder) && lane_progress > 0)))
      {
         if (on_stuck)
            task_queue_find(&finder);
         cancelled = cpu_features_get_time_usec();
         task_queue_cancel_ident(lane_ident);
      }
      task_queue_check();
      if (difftime(time(NULL), started) > SCAN_TIMEOUT_SECONDS)
         return -1;
      retro_sleep(1);
   }
   return (scan_completed && cancelled) ? cb_usec - cancelled : -1;
}

/* Reader tasks on the threaded queue's parallel workers.  The cancel
 * lane's files sleep 2 ms each, so the scan is still running when it
 * is cancelled. */
#define READER_CANCEL_FILES 200
#define READER_CANCEL_RUNS  20
#define READER_SLOW_SIZE    1000
/* The stuck set: one file whose read sticks, then four that match and
 * fillers. */
#define READER_STUCK_SIZE   1001
#define READER_STUCK_MS     2600
#define READER_STUCK_RUNS   2
#define READER_QUIT_MS      6000
#define READER_FILLERS      40

static const db_rec_t reader_recs[] = {
   { "Reader Zero",  NULL, 0x0EAD0000u, 1024 },
   { "Reader One",   NULL, 0x0EAD0001u, 1024 },
   { "Reader Two",   NULL, 0x0EAD0002u, 1024 },
   { "Reader Three", NULL, 0x0EAD0003u, 1024 }
};

static bool count_reader(retro_task_t *task, void *user_data)
{
   if (task_get_flags(task) & RETRO_TASK_FLG_PARALLEL)
      (*(unsigned*)user_data)++;
   return false;
}

/* Whether no reader is left on the queue, checking it for up to 5 s. */
static bool readers_gone(void)
{
   task_finder_data_t finder;
   unsigned           i, n = 0;

   finder.func     = count_reader;
   finder.userdata = &n;
   for (i = 0; i < 5000; i++)
   {
      n = 0;
      task_queue_find(&finder);
      if (!n)
         return true;
      task_queue_check();
      retro_sleep(1);
   }
   return false;
}

/* Whether every slow read begun has ended, waiting up to 10 s. */
static bool slow_reads_back(void)
{
   unsigned i;

   for (i = 0; i < 10000; i++)
   {
      if (  retro_atomic_load_acquire_int(&slow_back)
         == retro_atomic_load_acquire_int(&slow_in))
         return true;
      retro_sleep(1);
   }
   return false;
}

static void slow_reset(uint32_t size, unsigned ms)
{
   slow_size = size;
   slow_ms   = ms;
   retro_atomic_store_release_int(&slow_in,      0);
   retro_atomic_store_release_int(&slow_back,    0);
   retro_atomic_store_release_int(&slow_on_scan, 0);
}

static void use_db(const char *db_dir)
{
   strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
         sizeof(config_get_ptr()->paths.path_content_database));
}

static bool write_reader_db(const char *db_dir)
{
   char p[1024];

   path_mkdir(db_dir);
   sprintf(p, "%s/Test Reader.rdb", db_dir);
   return write_db_set(p, reader_recs, ARRAY_SIZE(reader_recs));
}

static bool write_cancel_files(char *dir, const char *root)
{
   char     p[1024];
   unsigned i;

   sprintf(dir, "%s/reader_cancel", root);
   path_mkdir(dir);
   for (i = 0; i < READER_CANCEL_FILES; i++)
   {
      sprintf(p, "%s/k_%03u.rd", dir, i);
      if (!write_content(p, 0x0EAD1000u + i, READER_SLOW_SIZE))
      {
         check(0, "fixture", "crc forcing failed");
         return false;
      }
   }
   return true;
}

static bool write_stuck_files(char *dir, const char *root)
{
   char     p[1024];
   unsigned i;

   sprintf(dir, "%s/reader_stuck", root);
   path_mkdir(dir);
   sprintf(p, "%s/j_stuck.rd", dir);
   if (!write_content(p, 0x0EAD2000u, READER_STUCK_SIZE))
      goto fail;
   for (i = 0; i < ARRAY_SIZE(reader_recs); i++)
   {
      sprintf(p, "%s/s_%u.rd", dir, i);
      if (!write_content(p, reader_recs[i].crc, reader_recs[i].size))
         goto fail;
   }
   for (i = 0; i < READER_FILLERS; i++)
   {
      sprintf(p, "%s/t_%02u.rd", dir, i);
      if (!write_content(p, 0x0EAD3000u + i, READER_SLOW_SIZE))
         goto fail;
   }
   return true;

fail:
   check(0, "fixture", "crc forcing failed");
   return false;
}

static bool reader_entries(const char *pl_dir)
{
   char     lpl[1024];
   unsigned i;

   sprintf(lpl, "%s/Test Reader.lpl", pl_dir);
   for (i = 0; i < ARRAY_SIZE(reader_recs); i++)
      if (!file_contains(lpl, reader_recs[i].name))
         return false;
   return true;
}

static void reader_cancel_lane(const char *root, const char *db_dir,
      bool threaded)
{
   char         dir[512], detail[64], what[80];
   unsigned     i, done = 0, gone = 0, over = 0, pushed = 0;
   unsigned     want = threaded ? readers_expected() : 0;
   int          most = 0;
   retro_time_t worst = 0;
   const char  *q = threaded ? "(threaded)" : "(regular)";

   if (!write_cancel_files(dir, root))
      return;

   use_queue(threaded);
   use_db(db_dir);
   slow_reset(READER_SLOW_SIZE, 2);
   for (i = 0; i < READER_CANCEL_RUNS; i++)
   {
      retro_time_t t;
      int          k;

      retro_atomic_store_release_int(&pushes_ok,      0);
      retro_atomic_store_release_int(&cb_fired,       0);
      retro_atomic_store_release_int(&reads_after_cb, 0);
      t = run_scan_cancelled(dir, false);
      if (t >= 0)
      {
         done++;
         if (t > worst)
            worst = t;
      }
      if (readers_gone())
         gone++;
      k = retro_atomic_load_acquire_int(&reads_after_cb);
      if (k > (int)want)
         over++;
      if (k > most)
         most = k;
      pushed += (unsigned)retro_atomic_load_acquire_int(&pushes_ok);
   }
   slow_size = 0;

   sprintf(detail, "%u of %u", done, READER_CANCEL_RUNS);
   sprintf(what, "cancelled reader scans finish %s", q);
   check(done == READER_CANCEL_RUNS, what, detail);
   sprintf(detail, "%u pushed in %u scans", pushed, READER_CANCEL_RUNS);
   sprintf(what, "%s readers %s", threaded ? "min(4, cores)" : "no", q);
   check(pushed == want * READER_CANCEL_RUNS, what, detail);
   sprintf(detail, "worst %u ms", (unsigned)(worst / 1000));
   sprintf(what, "the callback waits for no reader %s", q);
   check(done == READER_CANCEL_RUNS && worst < 500000, what, detail);
   sprintf(detail, "%u of %u", gone, READER_CANCEL_RUNS);
   sprintf(what, "readers leave after the callback %s", q);
   check(gone == READER_CANCEL_RUNS, what, detail);
   sprintf(detail, "%u of %u runs over %u, most %d", over,
         READER_CANCEL_RUNS, want, most);
   sprintf(what, "a stopped reader opens no new file %s", q);
   check(over == 0, what, detail);
}

/* A cancel while a reader is stuck in a read: the scan finishes at
 * once, unless the reader holds the scan's own worker, and the reader
 * leaves when the read returns. */
static void reader_stuck_lane(const char *root, const char *db_dir)
{
   char         dir[512], detail[80];
   unsigned     i, done = 0, back = 0, extra = 0;
   retro_time_t worst = 0;

   if (!write_stuck_files(dir, root))
      return;
   if (!readers_expected())
   {
      check(1, "a stuck reader holds no cancel", "no readers");
      check(1, "a stuck reader leaves once its read returns", "no readers");
      return;
   }
   use_queue(true);
   use_db(db_dir);
   for (i = 0; i < READER_STUCK_RUNS; i++)
   {
      retro_time_t t;

      slow_reset(READER_STUCK_SIZE, READER_STUCK_MS);
      t = run_scan_cancelled(dir, true);
      if (t >= 0)
         done++;
      if (t >= 0 && !retro_atomic_load_acquire_int(&slow_on_scan))
      {
         extra++;
         if (t > worst)
            worst = t;
      }
      if (slow_reads_back() && readers_gone())
         back++;
   }
   slow_size = 0;
   sprintf(detail, "%u of %u, %u on an extra worker, worst %u ms", done,
         READER_STUCK_RUNS, extra, (unsigned)(worst / 1000));
   check(done == READER_STUCK_RUNS && worst < 1000000,
         "a stuck reader holds no cancel", detail);
   sprintf(detail, "%u of %u", back, READER_STUCK_RUNS);
   check(back == READER_STUCK_RUNS,
         "a stuck reader leaves once its read returns", detail);
}

static bool scan_running(void *data)
{
   (void)data;
   return !scan_completed;
}

/* retroarch.c's quit with a reader stuck in a read: cancel everything,
 * drain for up to 3 s, deinit.  The scan's callback ends the drain,
 * unless the reader holds the scan's own worker, and the deinit leaves
 * the reader behind after its 1 s bound; its cleanup drops its share
 * when the read returns (LeakSanitizer sees the rest). */
static void reader_quit_lane(const char *root, const char *db_dir)
{
   char         dir[512], detail[64];
   retro_time_t t0, took = 0;
   time_t       started;
   bool         fired, back, on_scan;

   if (!write_stuck_files(dir, root))
      return;
   if (!readers_expected())
   {
      check(1, "quit: a stuck reader is left behind after 1 s", "no readers");
      check(1, "quit: the scan finished before the deinit", "no readers");
      check(1, "quit: the stuck reader comes back and leaves", "no readers");
      return;
   }
   use_queue(true);
   use_db(db_dir);
   slow_reset(READER_STUCK_SIZE, READER_QUIT_MS);
   loop_active    = true;
   scan_completed = false;
   if (!task_push_dbscan(dir, scan_cb))
   {
      check(0, "quit: scan started", "task_push_dbscan refused");
      return;
   }
   started = time(NULL);
   while (   !retro_atomic_load_acquire_int(&slow_in)
          && difftime(time(NULL), started) <= SCAN_TIMEOUT_SECONDS)
   {
      task_queue_check();
      retro_sleep(1);
   }
   task_queue_reset();
   task_queue_wait_timeout(scan_running, NULL, 3000000);
   fired   = scan_completed;
   on_scan = retro_atomic_load_acquire_int(&slow_on_scan) > 0;
   t0      = cpu_features_get_time_usec();
   task_queue_deinit();
   took    = cpu_features_get_time_usec() - t0;
   use_queue(true);
   /* Held by the reader, the scan finishes on the next queue. */
   started = time(NULL);
   while (   !scan_completed
          && difftime(time(NULL), started) <= SCAN_TIMEOUT_SECONDS)
   {
      task_queue_check();
      retro_sleep(1);
   }
   back    = slow_reads_back() && readers_gone();
   slow_size = 0;

   sprintf(detail, "deinit %u ms", (unsigned)(took / 1000));
   check(took >= 900000 && took < 2000000,
         "quit: a stuck reader is left behind after 1 s", detail);
   check(fired || (on_scan && scan_completed),
         "quit: the scan finished before the deinit",
         fired ? "callback fired" : on_scan ? "reader on the scan's worker"
                                            : "no callback");
   check(back, "quit: the stuck reader comes back and leaves",
         back ? "back" : "still out");
}

/* A push that fails keeps the readers before it; when none goes, the
 * scan reads its files itself, and the start is not tried again for
 * the next file. */
static void reader_push_lane(const char *root, const char *pl_dir,
      const char *db_dir)
{
   static const unsigned fail_from[2] = { 2, 1 };
   char     dir[512], p[1024], lpl[1024], detail[64];
   unsigned i, k;

   sprintf(dir, "%s/reader_push", root);
   path_mkdir(dir);
   for (i = 0; i < ARRAY_SIZE(reader_recs); i++)
   {
      sprintf(p, "%s/c_%u.rd", dir, i);
      if (!write_content(p, reader_recs[i].crc, reader_recs[i].size))
      { check(0, "fixture", "crc forcing failed"); return; }
   }
   sprintf(lpl, "%s/Test Reader.lpl", pl_dir);

   use_queue(true);
   use_db(db_dir);
   for (k = 0; k < 2; k++)
   {
      bool started, counted;
      int  tried, ok;

      filestream_delete(lpl);
      retro_atomic_store_release_int(&pushes_tried, 0);
      retro_atomic_store_release_int(&pushes_ok,    0);
      push_fail_from = fail_from[k];
      started        = run_scan(dir);
      push_fail_from = 0;
      tried          = retro_atomic_load_acquire_int(&pushes_tried);
      ok             = retro_atomic_load_acquire_int(&pushes_ok);
      if (readers_expected())
         counted = k ? (tried == 1 && ok == 0) : (tried == 2 && ok == 1);
      else
         counted = (tried == 0);
      sprintf(detail, "%d tried, %d pushed", tried, ok);
      check(started && scan_completed && reader_entries(pl_dir),
            k ? "first reader push fails: scan whole"
              : "second reader push fails: scan whole",
            "four entries");
      check(counted,
            k ? "first reader push fails: not retried"
              : "second reader push fails: one reader",
            detail);
   }
}

/* The files reader_recs match, in root/@name. */
static bool write_match_files(char *dir, const char *root, const char *name)
{
   char     p[1024];
   unsigned i;

   sprintf(dir, "%s/%s", root, name);
   path_mkdir(dir);
   for (i = 0; i < ARRAY_SIZE(reader_recs); i++)
   {
      sprintf(p, "%s/m_%u.rd", dir, i);
      if (!write_content(p, reader_recs[i].crc, reader_recs[i].size))
      {
         check(0, "fixture", "crc forcing failed");
         return false;
      }
   }
   return true;
}

#define READER_RESET_USEC 250000

/* A reset that reaches the readers before their first call, while the
 * scan waits on the first file it handed out: they read nothing and
 * leave, and the last one out wakes the scan, which reads the files
 * itself instead of sleeping out its park. */
static void reader_reset_lane(const char *root, const char *pl_dir,
      const char *db_dir)
{
   char         dir[512], lpl[1024], detail[64];
   retro_time_t t0, took;
   int          reads;
   bool         started;

   if (!readers_expected())
   {
      check(1, "readers cancelled at once read nothing", "no readers");
      check(1, "the last cancelled reader wakes the scan", "no readers");
      return;
   }
   if (!write_match_files(dir, root, "reader_reset"))
      return;
   sprintf(lpl, "%s/Test Reader.lpl", pl_dir);
   filestream_delete(lpl);
   use_queue(true);
   use_db(db_dir);
   retro_atomic_store_release_int(&watch_reader, 0);
   watch_size       = reader_recs[0].size;
   push_cancel_usec = READER_RESET_USEC;
   t0               = cpu_features_get_time_usec();
   started          = run_scan(dir);
   took             = cpu_features_get_time_usec() - t0;
   push_cancel_usec = 0;
   watch_size       = 0;
   reads            = retro_atomic_load_acquire_int(&watch_reader);

   sprintf(detail, "readers read %d", reads);
   check(started && scan_completed && reader_entries(pl_dir) && !reads,
         "readers cancelled at once read nothing", detail);
   sprintf(detail, "%u ms", (unsigned)(took / 1000));
   check(started && scan_completed && took < 5000000,
         "the last cancelled reader wakes the scan", detail);
}

/* Every reader's read fails: each file goes back, and the scan's own
 * read of it makes the same match. */
static void reader_fail_lane(const char *root, const char *pl_dir,
      const char *db_dir)
{
   char dir[512], lpl[1024], detail[64];
   int  fails;
   bool started;

   if (!readers_expected())
   {
      check(1, "failed reader reads: the same matches", "no readers");
      return;
   }
   if (!write_match_files(dir, root, "reader_fail"))
      return;
   sprintf(lpl, "%s/Test Reader.lpl", pl_dir);
   filestream_delete(lpl);
   use_queue(true);
   use_db(db_dir);
   retro_atomic_store_release_int(&reader_fails, 0);
   fail_size = reader_recs[0].size;
   started   = run_scan(dir);
   fail_size = 0;
   fails     = retro_atomic_load_acquire_int(&reader_fails);

   sprintf(detail, "%d reader reads failed", fails);
   check(   started && scan_completed && reader_entries(pl_dir)
         && fails == (int)ARRAY_SIZE(reader_recs),
         "failed reader reads: the same matches", detail);
}

#ifndef SCAN_READER_DEPTH
#define SCAN_READER_DEPTH 64
#endif

/* The scanner's window, SCAN_READER_DEPTH requests per reader. */
static unsigned reader_window(void)
{
   unsigned n = cpu_features_get_core_amount();
   if (n > SCAN_READERS_MAX)
      n = SCAN_READERS_MAX;
   return (n >= 2 ? n : 2) * SCAN_READER_DEPTH;
}

#define ORDER_FIRST_CRC  0x0BD10001u
#define ORDER_LATE_CRC   0x0BD10002u
#define ORDER_FIRST_SIZE (60 * 1024)
#define ORDER_LATE_SIZE  1024
#define ORDER_SLOW_MS    50

/* @first names the file that matches only in the database probed
 * second; the other carries the record both databases hold. */
static bool write_order_files(const char *dir, unsigned first)
{
   static const char *const names[2] = { "o_a.rd", "o_b.rd" };
   char p[1024];

   sprintf(p, "%s/%s", dir, names[first]);
   if (!write_content(p, ORDER_FIRST_CRC, ORDER_FIRST_SIZE))
      return false;
   sprintf(p, "%s/%s", dir, names[!first]);
   return write_content(p, ORDER_LATE_CRC, ORDER_LATE_SIZE) != 0;
}

/* One first-match scan of the order set, the first file's read held
 * ORDER_SLOW_MS.  True when it finished. */
static bool order_scan(const char *dir, const char *db, retro_time_t *took)
{
   retro_time_t t0;
   bool         started;

   strlcpy(config_get_ptr()->paths.path_content_database, db,
         sizeof(config_get_ptr()->paths.path_content_database));
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_CUSTOM);
   manual_content_scan_set_menu_scan_db_select(
         MANUAL_CONTENT_SCAN_SELECT_DB_AUTO_FIRST_MATCH, NULL);
   order_last[0] = 0;
   order_last[1] = 0;
   slow_size     = ORDER_FIRST_SIZE;
   slow_ms       = ORDER_SLOW_MS;
   t0            = cpu_features_get_time_usec();
   started       = run_scan(dir);
   *took         = cpu_features_get_time_usec() - t0;
   slow_size     = 0;
   manual_content_scan_set_menu_scan_db_select(
         MANUAL_CONTENT_SCAN_SELECT_DB_AUTO, NULL);
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC);
   return started && scan_completed;
}

/* Results are consumed in content-list order, which first-match
 * promotion shows: the file the scanner starts first matches only in
 * the database it probes second and promotes it, so the other file,
 * which both carry, is filed under that one.  The first file's read is
 * held, so the other's result is in first. */
static void order_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static const char *const db_names[2]   =
      { "Test Order One", "Test Order Two" };
   static const char *const late_names[2] =
      { "Late In One", "Late In Two" };
   char                db[512], dir[512], p[1024], what[64], detail[64];
   struct string_list *dbs;
   db_rec_t            recs[2];
   unsigned            i, second, first = 0;
   retro_time_t        took = 0;
   bool                ran = false, filed;
   const char         *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,  "%s/reader_order_db", root);
   sprintf(dir, "%s/reader_order",    root);
   path_mkdir(db);
   path_mkdir(dir);
   /* Both databases first, so the listing below is the scanner's. */
   for (i = 0; i < 2; i++)
   {
      sprintf(p, "%s/%s.rdb", db, db_names[i]);
      if (!write_db(p, late_names[i], ORDER_LATE_CRC, ORDER_LATE_SIZE))
      { check(0, "fixture", "could not write database"); return; }
   }
   if (   !(dbs = dir_list_new(db, "rdb", false, false, false, false))
       || dbs->size != 2)
   {
      if (dbs)
         string_list_free(dbs);
      check(0, "fixture", "could not list the order databases");
      return;
   }
   second = strstr(dbs->elems[1].data, db_names[1]) ? 1 : 0;
   string_list_free(dbs);
   memset(recs, 0, sizeof(recs));
   recs[0].name   = "First In Second";
   recs[0].serial = NULL;
   recs[0].crc    = ORDER_FIRST_CRC;
   recs[0].size   = ORDER_FIRST_SIZE;
   recs[1].name   = late_names[second];
   recs[1].serial = NULL;
   recs[1].crc    = ORDER_LATE_CRC;
   recs[1].size   = ORDER_LATE_SIZE;
   sprintf(p, "%s/%s.rdb", db, db_names[second]);
   if (!write_db_set(p, recs, 2))
   { check(0, "fixture", "could not write database"); return; }

   use_queue(threaded);
   retro_atomic_store_release_int(&reads_begun, 0);
   begun_size = ORDER_FIRST_SIZE;
   /* The slow file goes where the scanner starts. */
   for (i = 0; i < 2; i++)
   {
      if (!write_order_files(dir, first))
      { check(0, "fixture", "crc forcing failed"); return; }
      sprintf(p, "%s/%s.lpl", pl_dir, db_names[0]);
      filestream_delete(p);
      sprintf(p, "%s/%s.lpl", pl_dir, db_names[1]);
      filestream_delete(p);
      if (!(ran = order_scan(dir, db, &took))
          || !order_last[0] || !order_last[1])
         break;
      if ((order_last[0] < order_last[1]) == (first == 0))
         break;
      first = !first;
   }

   begun_size = 0;
   sprintf(p, "%s/%s.lpl", pl_dir, db_names[second]);
   filed = file_contains(p, late_names[second]);
   sprintf(p, "%s/%s.lpl", pl_dir, db_names[!second]);
   filed = filed && !file_contains(p, late_names[!second]);
   sprintf(what, "results consumed in list order %s", q);
   check(ran && i < 2 && filed, what, late_names[second]);
   /* One scan when the first try already shows the order. */
   sprintf(detail, "%d reads in %u scans",
         retro_atomic_load_acquire_int(&reads_begun), i < 2 ? i + 1 : 2);
   sprintf(what, "each file read once %s", q);
   check(ran && retro_atomic_load_acquire_int(&reads_begun)
         == (int)(i < 2 ? i + 1 : 2), what, detail);
   if (threaded)
   {
      sprintf(detail, "%u ms", (unsigned)(took / 1000));
      check(ran && took < 10000000, "a result wakes the waiting scan",
            detail);
   }
}

static void set_rec(db_rec_t *rec, const char *name, uint32_t crc,
      uint32_t size)
{
   rec->name   = name;
   rec->serial = NULL;
   rec->crc    = crc;
   rec->size   = size;
   rec->rom_name = NULL;
   rec->unsized  = false;
}

#define MIX_BIG_ONE 70000
#define MIX_BIG_TWO 131072

/* Two files over the cutoff, an archive matched by its own CRC, one
 * matched by its member's stored CRC, and a cue that prunes its track. */
static void mix_lane(const char *root, const char *pl_dir, bool threaded)
{
   char        db[512], dir[512], p[1024], lpl[1024], what[64];
   char        crcs[5][16], detail[64];
   db_rec_t    recs[5];
   uint8_t    *d   = NULL;
   int64_t     len = 0;
   uint32_t    i;
   int         by_scan, by_reader;
   bool        started, all = true;
   const char *q = threaded ? "(threaded)" : "(regular)";
   FILE       *f;

   sprintf(db,  "%s/reader_mix_db", root);
   sprintf(dir, "%s/reader_mix",    root);
   path_mkdir(db);
   path_mkdir(dir);

   sprintf(p, "%s/m1_big.rd", dir);
   if (!write_content(p, 0x0B1C0001u, MIX_BIG_ONE))
      goto fixture;
   sprintf(p, "%s/m2_big.rd", dir);
   if (!write_content(p, 0x0B1C0002u, MIX_BIG_TWO))
      goto fixture;
   sprintf(p, "%s/m0_disc.bin", dir);
   if (!write_content(p, 0x0B1C0005u, 4096))
      goto fixture;
   sprintf(p, "%s/m0_disc.cue", dir);
   if (!(f = fopen(p, "w")))
      goto fixture;
   fprintf(f,
         "FILE \"m0_disc.bin\" BINARY\n"
         "  TRACK 01 MODE1/2352\n"
         "    INDEX 01 00:00:00\n");
   fclose(f);

   /* Its member matches nothing; the archive's own CRC does. */
   if (!(d = (uint8_t*)malloc(2048)))
      goto fixture;
   for (i = 0; i < 2048; i++)
      d[i] = (uint8_t)((i * 29 + 3) & 0xFF);
   sprintf(p, "%s/m3_whole.zip", dir);
   if (!write_zip(p, "whole.rd", d, 2048, crc_of(d, 2048)))
      goto fixture;
   free(d);
   d = NULL;
   if (!filestream_read_file(p, (void**)&d, &len) || !d)
      goto fixture;
   set_rec(&recs[2], "Mix Archive", crc_of(d, (size_t)len), (uint32_t)len);
   recs[2].rom_name = "m3_whole.zip";
   free(d);
   d = NULL;

   /* Its own CRC matches nothing; its member does. */
   if (!(d = (uint8_t*)malloc(2048)))
      goto fixture;
   for (i = 0; i < 2048 - 4; i++)
      d[i] = (uint8_t)((i * 31 + 5) & 0xFF);
   crc_force(d, 2048 - 4, 0x0B1C0004u, d + 2048 - 4);
   sprintf(p, "%s/m4_bundle.zip", dir);
   if (!write_zip(p, "inner.rd", d, 2048, 0x0B1C0004u))
      goto fixture;
   free(d);
   d = NULL;

   set_rec(&recs[0], "Mix Big One", 0x0B1C0001u, MIX_BIG_ONE);
   set_rec(&recs[1], "Mix Big Two", 0x0B1C0002u, MIX_BIG_TWO);
   set_rec(&recs[3], "Mix Member",  0x0B1C0004u, 2048);
   set_rec(&recs[4], "Mix Disc",    0x0B1C0005u, 4096);
   sprintf(p, "%s/Test Mix.rdb", db);
   if (!write_db_set(p, recs, 5))
      goto fixture;
   for (i = 0; i < 5; i++)
      sprintf(crcs[i], "%08X|crc", (unsigned)recs[i].crc);

   sprintf(lpl, "%s/Test Mix.lpl", pl_dir);
   filestream_delete(lpl);
   use_queue(threaded);
   strlcpy(config_get_ptr()->paths.path_content_database, db,
         sizeof(config_get_ptr()->paths.path_content_database));
   retro_atomic_store_release_int(&watch_scan,   0);
   retro_atomic_store_release_int(&watch_reader, 0);
   watch_size = MIX_BIG_ONE;
   started    = run_scan(dir);
   watch_size = 0;
   by_scan    = retro_atomic_load_acquire_int(&watch_scan);
   by_reader  = retro_atomic_load_acquire_int(&watch_reader);
   for (i = 0; i < 5; i++)
      if (!file_contains(lpl, recs[i].name) || !file_contains(lpl, crcs[i]))
         all = false;
   sprintf(what, "every mixed entry by its crc %s", q);
   check(started && scan_completed && all, what,
         "big, archive, member, disc");
   sprintf(what, "a pruned track gets no entry %s", q);
   check(started && scan_completed && !file_contains(lpl, "m0_disc.bin"),
         what, "m0_disc.bin");
   sprintf(detail, "scan %d, readers %d", by_scan, by_reader);
   sprintf(what, "a file over the cutoff stays on the task %s", q);
   check(started && scan_completed && by_scan > 0 && by_reader == 0,
         what, detail);
   return;

fixture:
   free(d);
   check(0, "fixture", "could not write the mixed set");
}

#define ARC_LEN   2048
#define ARC_KINDS 6

/* An archive of each kind the scanner opens, matched through its
 * member: by the CRC the archive records (7z, RAR, zip), by one the
 * member is decoded for where none is recorded (RAR 5 with BLAKE2,
 * .zst), or as it is listed (.bz2). Each archive's own CRC matches
 * nothing. The core loads 7z, zip and RAR archives as they are, and
 * none of its extensions is test.bin's: that archive is matched by its
 * first member. Those it loads are written as themselves, not
 * expanded. */
static void archive_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   char        db[512], dir[512], p[1024], lpl[1024], what[96];
   char        crcs[ARC_KINDS][16], old_db[PATH_MAX_LENGTH];
   db_rec_t    recs[ARC_KINDS];
   uint8_t     d[ARC_LEN];
   uint8_t    *c   = NULL;
   size_t      cap = 16384, clen = 0;
   uint32_t    i, x;
   unsigned    k;
   bool        started;
   const char *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,  "%s/archive_db", root);
   sprintf(dir, "%s/archive_%s", root, threaded ? "t" : "r");
   path_mkdir(db);
   path_mkdir(dir);
   if (!(c = (uint8_t*)malloc(cap)))
      goto fixture;

   for (i = 0; i < ARC_LEN; i++)
      d[i] = (uint8_t)((i * 41 + 9) & 0xFF);
   set_rec(&recs[0], "Arc Seven", crc_of(d, ARC_LEN), ARC_LEN);
   sprintf(p, "%s/a1.7z", dir);
   if (!write_bytes(p, fx_scan_7z, sizeof(fx_scan_7z)))
      goto fixture;

   set_rec(&recs[1], "Arc Rar", 0x7CCA70CDu, 1200);
   sprintf(p, "%s/b2.rar", dir);
   if (!write_bytes(p, fx_scan_rar5, sizeof(fx_scan_rar5)))
      goto fixture;

   set_rec(&recs[2], "Arc Blake", 0x7E5EC49Eu, 814);
   sprintf(p, "%s/b3.rar", dir);
   if (!write_bytes(p, fx_scan_rar5_blake2, sizeof(fx_scan_rar5_blake2)))
      goto fixture;

   /* three members of their own, made here */
   for (k = 0; k < 3; k++)
   {
      static const char *const names[3] = {
         "Arc Zst", "Arc Bzip", "Arc Zip Zstd" };
      x = 5 + k;
      for (i = 0; i < ARC_LEN; i++)
      {
         x    = x * 1103515245u + 12345u;
         d[i] = (uint8_t)('a' + (x >> 16) % 6);
      }
      set_rec(&recs[3 + k], names[k], crc_of(d, ARC_LEN), ARC_LEN);
      if (k == 1)
         clen = bz_write(c, cap, d, ARC_LEN, 1, 1);
      else if (rzstd_encode(c, cap, d, ARC_LEN, 3, &clen)
            != RZSTD_PROCESS_END)
         clen = 0;
      if (!clen)
         goto fixture;
      if (k == 0)
         sprintf(p, "%s/c4.rd.zst", dir);
      else if (k == 1)
         sprintf(p, "%s/c5.rd.bz2", dir);
      else
         sprintf(p, "%s/c6.zip", dir);
      if (k < 2 ? !write_bytes(p, c, clen)
            : !write_zip_method(p, "c6.rd", 93, c, (uint32_t)clen, ARC_LEN,
               recs[5].crc))
         goto fixture;
   }
   free(c);
   c = NULL;

   sprintf(p, "%s/Test Archives.rdb", db);
   if (!write_db_set(p, recs, ARC_KINDS))
      goto fixture;
   for (k = 0; k < ARC_KINDS; k++)
      sprintf(crcs[k], "%08X|crc", (unsigned)recs[k].crc);

   sprintf(lpl, "%s/Test Archives.lpl", pl_dir);
   filestream_delete(lpl);
   use_queue(threaded);
   strlcpy(old_db, config_get_ptr()->paths.path_content_database,
         sizeof(old_db));
   strlcpy(config_get_ptr()->paths.path_content_database, db,
         sizeof(config_get_ptr()->paths.path_content_database));
   started = run_scan(dir);
   strlcpy(config_get_ptr()->paths.path_content_database, old_db,
         sizeof(config_get_ptr()->paths.path_content_database));
   for (k = 0; k < ARC_KINDS; k++)
   {
      sprintf(what, "%s matched through its member %s", recs[k].name, q);
      check(started && scan_completed && file_contains(lpl, recs[k].name)
            && file_contains(lpl, crcs[k]), what, crcs[k]);
   }
   sprintf(what, "archives the core loads not expanded %s", q);
   check(   started && scan_completed && !file_contains(lpl, ".7z#")
         && !file_contains(lpl, ".rar#") && !file_contains(lpl, ".zip#"),
         what, "a1.7z, b2.rar, b3.rar, c6.zip");
   return;

fixture:
   free(c);
   check(0, "fixture", "could not write the archives");
}

#define MF_MEMBER_LEN 1024

/* @len bytes of a pattern from @seed, brought to crc32 @crc. */
static void fill_forced(uint8_t *d, uint32_t len, unsigned seed,
      uint32_t crc)
{
   uint32_t i;

   for (i = 0; i < len - 4; i++)
      d[i] = (uint8_t)((i * seed + 3) & 0xFF);
   crc_force(d, len - 4, crc, d + len - 4);
}

/* @rec named @name, keyed by the crc32 and size of the file at @path. */
static bool set_whole_rec(db_rec_t *rec, const char *name,
      const char *path)
{
   uint8_t *d   = NULL;
   int64_t  len = 0;

   if (!filestream_read_file(path, (void**)&d, &len) || !d)
      return false;
   set_rec(rec, name, crc_of(d, (size_t)len), (uint32_t)len);
   /* A rom_name ending in .zip is enough for the size gate. */
   rec->rom_name = "set.zip";
   free(d);
   return true;
}

/* An archive is looked up by its first member's stored CRC, then by
 * its own, then by every member's; a folder is no member.  On the
 * regular queue nothing reads ahead, so a member match reads no
 * archive whole. */
static void member_first_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static uint8_t d[5][MF_MEMBER_LEN];
   static char    folder_lpl[1024];
   char           db[512], dir[512], p[1024], cart_lpl[1024];
   char           arcade_lpl[1024], what[64], detail[64], crc[2][16];
   db_rec_t       cart[2], arcade[2];
   zip_entry_t    e[2];
   uint32_t       zip_size = 0;
   int            reads, stored;
   bool           started;
   const char    *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,  "%s/member_first_db", root);
   sprintf(dir, "%s/member_first",    root);
   path_mkdir(db);
   path_mkdir(dir);

   /* Matched by its member; its own CRC matches nothing. */
   fill_forced(d[0], MF_MEMBER_LEN, 41, 0x0BF10001u);
   memset(e, 0, sizeof(e));
   e[0].name = "member.rd";
   e[0].data = d[0];
   e[0].len  = MF_MEMBER_LEN;
   e[0].crc  = 0x0BF10001u;
   sprintf(p, "%s/mf_member.zip", dir);
   if (!write_zip_entries(p, e, 1))
      goto fixture;
   zip_size = (uint32_t)path_get_size(p);
   set_rec(&cart[0], "MF Member", 0x0BF10001u, MF_MEMBER_LEN);

   /* Its first member matches nothing, its own CRC does: not expanded,
    * though its second member would match. */
   fill_forced(d[1], MF_MEMBER_LEN, 43, 0x0BF100A1u);
   e[0].name = "a1.rd";
   e[0].data = d[1];
   e[0].crc  = 0x0BF100A1u;
   e[1].name = "a2.rd";
   e[1].data = d[0];
   e[1].len  = MF_MEMBER_LEN;
   e[1].crc  = 0x0BF10001u;
   sprintf(p, "%s/mf_arcade.zip", dir);
   if (   !write_zip_entries(p, e, 2)
       || !set_whole_rec(&arcade[0], "MF Arcade", p))
      goto fixture;

   /* A directory, then a member that matches nothing: only its own CRC
    * can match. */
   e[0].name = "d/";
   e[0].data = NULL;
   e[0].len  = 0;
   e[0].crc  = 0;
   e[1].name = "d/x.rd";
   e[1].data = d[1];
   e[1].crc  = 0x0BF100A1u;
   sprintf(p, "%s/mf_dirfirst.zip", dir);
   if (   !write_zip_entries(p, e, 2)
       || !set_whole_rec(&arcade[1], "MF Dir First", p))
      goto fixture;

   /* Its first member misses and its second is a record named for no
    * archive; no database may hold it by its size: held, and written
    * as archive#member. */
   fill_forced(d[2], MF_MEMBER_LEN, 47, 0x0BF100A2u);
   fill_forced(d[3], MF_MEMBER_LEN, 53, 0x0BF10003u);
   e[0].name = "a.rd";
   e[0].data = d[2];
   e[0].len  = MF_MEMBER_LEN;
   e[0].crc  = 0x0BF100A2u;
   e[1].name = "b.rd";
   e[1].data = d[3];
   e[1].crc  = 0x0BF10003u;
   sprintf(p, "%s/mf_multi.zip", dir);
   if (!write_zip_entries(p, e, 2))
      goto fixture;
   set_rec(&cart[1], "MF Multi B", 0x0BF10003u, MF_MEMBER_LEN);

#ifdef HAVE_7ZIP
   /* A folder is no member: this 7z of a folder and one member is named
    * by that member's hit, in a database whose core opens 7z. */
   {
      db_rec_t folder;

      fill_forced(d[4], MF_MEMBER_LEN, 59, 0x0BF10007u);
      e[0].name = "d/";
      e[0].data = NULL;
      e[0].len  = 0;
      e[0].crc  = 0;
      e[1].name = "d/m.rd";
      e[1].data = d[4];
      e[1].len  = MF_MEMBER_LEN;
      e[1].crc  = 0x0BF10007u;
      sprintf(p, "%s/mf_folder.7z", dir);
      set_rec(&folder, "MF Folder 7z", 0x0BF10007u, MF_MEMBER_LEN);
      if (!write_7z_entries(p, e, 2))
         goto fixture;
      sprintf(p, "%s/Test Archives.rdb", db);
      if (!write_db_set(p, &folder, 1))
         goto fixture;
   }
#endif

   sprintf(p, "%s/Test MF Cart.rdb", db);
   if (!write_db_set(p, cart, 2))
      goto fixture;
   sprintf(p, "%s/Test MF Arcade.rdb", db);
   if (!write_db_set(p, arcade, 2))
      goto fixture;

   sprintf(cart_lpl,   "%s/Test MF Cart.lpl",   pl_dir);
   sprintf(arcade_lpl, "%s/Test MF Arcade.lpl", pl_dir);
   sprintf(folder_lpl, "%s/Test Archives.lpl", pl_dir);
   filestream_delete(cart_lpl);
   filestream_delete(arcade_lpl);
   filestream_delete(folder_lpl);
   use_queue(threaded);
   use_db(db);
   retro_atomic_store_release_int(&reads_begun, 0);
   retro_atomic_store_release_int(&stored_reads, 0);
   begun_size  = zip_size;
   stored_name = "mf_dirfirst.zip";
   started     = run_scan(dir) && scan_completed;
   begun_size  = 0;
   stored_name = NULL;
   reads       = retro_atomic_load_acquire_int(&reads_begun);
   stored      = retro_atomic_load_acquire_int(&stored_reads);

   sprintf(crc[0], "%08X|crc", (unsigned)cart[0].crc);
   sprintf(what, "archive matched by its member's CRC %s", q);
   check(   started && file_contains(cart_lpl, "MF Member")
         && file_contains(cart_lpl, crc[0]), what, "mf_member.zip");
   if (!threaded)
   {
      sprintf(detail, "%d whole reads", reads);
      check(started && reads == 0, "a member match reads no archive whole",
            detail);
   }
   sprintf(crc[1], "%08X|crc", (unsigned)arcade[0].crc);
   sprintf(what, "archive matched by its own CRC %s", q);
   check(   started && file_contains(arcade_lpl, "MF Arcade")
         && file_contains(arcade_lpl, crc[1])
         && !file_contains(cart_lpl, "mf_arcade.zip#"), what,
         "mf_arcade.zip, not expanded");
   sprintf(what, "directory first: matched by its own %s", q);
   check(started && file_contains(arcade_lpl, "MF Dir First"), what,
         "mf_dirfirst.zip");
   sprintf(detail, "%d directory reads", stored);
   sprintf(what, "directory first: its directory read once %s", q);
   check(started && stored == 1, what, detail);
   sprintf(what, "held after its first member misses %s", q);
   check(   started && file_contains(cart_lpl, "MF Multi B")
         && file_contains(cart_lpl, "mf_multi.zip#b.rd"), what,
         "mf_multi.zip#b.rd");
#ifdef HAVE_7ZIP
   sprintf(what, "7z, a folder first: named by its member %s", q);
   check(   started && file_contains(folder_lpl, "MF Folder 7z")
         && file_contains(folder_lpl, "mf_folder.7z")
         && !file_contains(folder_lpl, "mf_folder.7z#"), what,
         "mf_folder.7z");
#endif
   return;

fixture:
   check(0, "fixture", "could not write the member-first set");
}

#define GATE_ZIPS    4
/* One member named rom.<2 letters>; two named <letter>.ar. */
#define GATE_ZIP_LEN (30 + 6 + MF_MEMBER_LEN + 46 + 6 + 22)
#define ARC_ZIP_LEN  (2 * (30 + 4 + MF_MEMBER_LEN + 46 + 4) + 22)
/* A TorrentZip end comment; ARC_ZIP_LEN's zips with it. */
#define TZ_COMMENT   "TORRENTZIPPED-5C56B2DD"
#define TZ_ZIP_LEN   (ARC_ZIP_LEN + 22)

/* @dir/@file of one member, @member, forced to @crc, and its record
 * @rec, named @name. */
static bool write_gate_zip(const char *dir, const char *file,
      const char *member, const char *name, uint32_t crc, db_rec_t *rec)
{
   static uint8_t d[MF_MEMBER_LEN];
   char           p[1024];

   fill_forced(d, MF_MEMBER_LEN, 59 + (crc & 0xF), crc);
   sprintf(p, "%s/%s", dir, file);
   set_rec(rec, name, crc, MF_MEMBER_LEN);
   return write_zip(p, member, d, MF_MEMBER_LEN, crc) != 0;
}

/* Cartridge zips whose database's core lists the member's extension
 * but not zip, with "Scan without core match" off: matched by the
 * member without a whole read, and written as x.zip#member, as when
 * expanded. */
static void cart_gate_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static const char *const file[GATE_ZIPS] = {
      "cg0.zip", "cg1.zip", "cg2.zip", "cg3.zip" };
   static const char *const name[GATE_ZIPS] = {
      "Gate CG0", "Gate CG1", "Gate CG2", "Gate CG3" };
   char        db[512], dir[512], p[1024], lpl[1024];
   char        what[64], detail[64], want[32];
   db_rec_t    rec[GATE_ZIPS];
   unsigned    k;
   int         reads;
   bool        started, all = true;
   const char *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,  "%s/cart_gate_db", root);
   sprintf(dir, "%s/cart_gate",    root);
   path_mkdir(db);
   path_mkdir(dir);
   for (k = 0; k < GATE_ZIPS; k++)
      if (!write_gate_zip(dir, file[k], "rom.ct", name[k],
            0x0CA70000u + k, &rec[k]))
         goto fixture;
   sprintf(p, "%s/Test Cart Gate.rdb", db);
   if (!write_db_set(p, rec, GATE_ZIPS))
      goto fixture;

   sprintf(lpl, "%s/Test Cart Gate.lpl", pl_dir);
   filestream_delete(lpl);
   use_queue(threaded);
   use_db(db);
   retro_atomic_store_release_int(&reads_begun, 0);
   begun_size = GATE_ZIP_LEN;
   started    = run_scan(dir) && scan_completed;
   begun_size = 0;
   reads      = retro_atomic_load_acquire_int(&reads_begun);

   for (k = 0; k < GATE_ZIPS; k++)
   {
      sprintf(want, "%s#rom.ct", file[k]);
      sprintf(p, "%08X|crc", (unsigned)rec[k].crc);
      all = all && file_contains(lpl, want) && file_contains(lpl, p);
   }
   sprintf(what, "cart zip: matched as x.zip#member %s", q);
   check(started && all, what, "cg0.zip#rom.ct to cg3.zip#rom.ct");
   sprintf(what, "cart zip: no archive read whole %s", q);
   sprintf(detail, "%d whole reads", reads);
   check(started && reads == 0, what, detail);
   return;

fixture:
   check(0, "fixture", "could not write the cart gate set");
}

/* Where the database takes the archive's own name, with "Scan without
 * core match" on or a core listing zip claiming it, a member match is
 * written for x.zip, unexpanded and not read whole, whether or not
 * that core lists the member's extension. */
static void base_paths_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   char        db[2][512], dir[2][512], p[1024], lpl[2][1024];
   char        what[64], detail[64], crc[3][16];
   db_rec_t    rec[3];
   unsigned    k;
   int         reads[2];
   bool        started[2];
   const char *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db[0],  "%s/base_on_db",  root);
   sprintf(dir[0], "%s/base_on",     root);
   sprintf(db[1],  "%s/base_zip_db", root);
   sprintf(dir[1], "%s/base_zip",    root);
   for (k = 0; k < 2; k++)
   {
      path_mkdir(db[k]);
      path_mkdir(dir[k]);
   }
   if (   !write_gate_zip(dir[0], "bp0.zip", "rom.ct", "Gate BP0",
            0x0CA70010u, &rec[0])
       || !write_gate_zip(dir[1], "bz0.zip", "rom.cz", "Gate BZ0",
            0x0CA70011u, &rec[1])
       || !write_gate_zip(dir[1], "bz1.zip", "rom.cy", "Gate BZ1",
            0x0CA70012u, &rec[2]))
      goto fixture;
   sprintf(p, "%s/Test Cart Gate.rdb", db[0]);
   if (!write_db_set(p, rec, 1))
      goto fixture;
   sprintf(p, "%s/Test Cart Zip.rdb", db[1]);
   if (!write_db_set(p, rec + 1, 2))
      goto fixture;
   for (k = 0; k < 3; k++)
      sprintf(crc[k], "%08X|crc", (unsigned)rec[k].crc);
   sprintf(lpl[0], "%s/Test Cart Gate.lpl", pl_dir);
   sprintf(lpl[1], "%s/Test Cart Zip.lpl",  pl_dir);

   use_queue(threaded);
   for (k = 0; k < 2; k++)
   {
      filestream_delete(lpl[k]);
      use_db(db[k]);
      config_get_ptr()->bools.scan_without_core_match = !k;
      retro_atomic_store_release_int(&reads_begun, 0);
      begun_size = GATE_ZIP_LEN;
      started[k] = run_scan(dir[k]) && scan_completed;
      begun_size = 0;
      reads[k]   = retro_atomic_load_acquire_int(&reads_begun);
   }
   config_get_ptr()->bools.scan_without_core_match = false;

   sprintf(what, "without core match: a member match is x.zip %s", q);
   check(   started[0] && file_contains(lpl[0], "bp0.zip\"")
         && file_contains(lpl[0], crc[0])
         && !file_contains(lpl[0], "bp0.zip#"), what,
         "bp0.zip, not expanded");
   sprintf(what, "zip-listing core: a member match is x.zip %s", q);
   check(   started[1] && file_contains(lpl[1], "bz0.zip\"")
         && file_contains(lpl[1], "bz1.zip\"")
         && file_contains(lpl[1], crc[1]) && file_contains(lpl[1], crc[2])
         && !file_contains(lpl[1], ".zip#"), what,
         "bz0.zip and bz1.zip, not expanded");
   sprintf(what, "x.zip member matches: no archive read whole %s", q);
   sprintf(detail, "%d and %d whole reads", reads[0], reads[1]);
   check(started[0] && started[1] && !reads[0] && !reads[1], what, detail);
   return;

fixture:
   check(0, "fixture", "could not write the base paths set");
}

/* An arcade zip: no core claims its first member's extension, and its
 * own CRC is in a database a core listing zip claims.  The member pass
 * misses, even where a database holds the member, and the whole pass
 * writes x.zip. */
static void arc_gate_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static uint8_t d[2][MF_MEMBER_LEN];
   char           db[512], dir[512], p[1024], lpl[2][1024];
   char           what[64], detail[64], crc[16];
   db_rec_t       arc, cart;
   zip_entry_t    e[2];
   int            reads;
   bool           started;
   const char    *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,  "%s/arc_gate_db", root);
   sprintf(dir, "%s/arc_gate",    root);
   path_mkdir(db);
   path_mkdir(dir);
   fill_forced(d[0], MF_MEMBER_LEN, 61, 0x0CA70020u);
   fill_forced(d[1], MF_MEMBER_LEN, 67, 0x0CA70021u);
   memset(e, 0, sizeof(e));
   e[0].name = "a.ar";
   e[0].data = d[0];
   e[0].len  = MF_MEMBER_LEN;
   e[0].crc  = 0x0CA70020u;
   e[1].name = "b.ar";
   e[1].data = d[1];
   e[1].len  = MF_MEMBER_LEN;
   e[1].crc  = 0x0CA70021u;
   sprintf(p, "%s/ag0.zip", dir);
   if (   !write_zip_entries(p, e, 2)
       || !set_whole_rec(&arc, "Gate AG0", p))
      goto fixture;
   set_rec(&cart, "Gate AG0 Member", 0x0CA70020u, MF_MEMBER_LEN);
   sprintf(p, "%s/Test Arc Gate.rdb", db);
   if (!write_db_set(p, &arc, 1))
      goto fixture;
   sprintf(p, "%s/Test Cart Gate.rdb", db);
   if (!write_db_set(p, &cart, 1))
      goto fixture;

   sprintf(lpl[0], "%s/Test Arc Gate.lpl",  pl_dir);
   sprintf(lpl[1], "%s/Test Cart Gate.lpl", pl_dir);
   filestream_delete(lpl[0]);
   filestream_delete(lpl[1]);
   use_queue(threaded);
   use_db(db);
   retro_atomic_store_release_int(&reads_begun, 0);
   begun_size = ARC_ZIP_LEN;
   started    = run_scan(dir) && scan_completed;
   begun_size = 0;
   reads      = retro_atomic_load_acquire_int(&reads_begun);

   sprintf(crc, "%08X|crc", (unsigned)arc.crc);
   sprintf(what, "arcade zip: matched whole as x.zip %s", q);
   check(   started && file_contains(lpl[0], "ag0.zip\"")
         && file_contains(lpl[0], crc)
         && !file_contains(lpl[0], "ag0.zip#")
         && !file_contains(lpl[1], "ag0.zip"), what,
         "ag0.zip, not expanded");
   if (!threaded)
   {
      sprintf(detail, "%d whole reads", reads);
      check(started && reads == 1, "arcade zip: read whole once", detail);
   }
   return;

fixture:
   check(0, "fixture", "could not write the arcade gate set");
}

/* An arcade set whose first member is a cartridge ROM: its extension is
 * claimed and its CRC is in the cartridge database, and the set's own
 * CRC is in a database a core listing zip claims.  An archive of
 * several members takes either CRC in each database, so x.zip is
 * written for the arcade database and nothing is expanded. */
static void arc_cart_first_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static uint8_t d[2][MF_MEMBER_LEN];
   char           db[512], dir[512], p[1024], lpl[2][1024];
   char           what[64], detail[64], crc[16];
   db_rec_t       arc, cart;
   zip_entry_t    e[2];
   int            reads;
   bool           started;
   const char    *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,  "%s/arc_cart_first_db", root);
   sprintf(dir, "%s/arc_cart_first",    root);
   path_mkdir(db);
   path_mkdir(dir);
   fill_forced(d[0], MF_MEMBER_LEN, 73, 0x0CA70030u);
   fill_forced(d[1], MF_MEMBER_LEN, 79, 0x0CA70031u);
   memset(e, 0, sizeof(e));
   e[0].name = "a.ct";
   e[0].data = d[0];
   e[0].len  = MF_MEMBER_LEN;
   e[0].crc  = 0x0CA70030u;
   e[1].name = "b.ar";
   e[1].data = d[1];
   e[1].len  = MF_MEMBER_LEN;
   e[1].crc  = 0x0CA70031u;
   sprintf(p, "%s/ac0.zip", dir);
   if (   !write_zip_entries(p, e, 2)
       || !set_whole_rec(&arc, "Gate AC0", p))
      goto fixture;
   set_rec(&cart, "Gate AC0 Cart", 0x0CA70030u, MF_MEMBER_LEN);
   sprintf(p, "%s/Test Arc Gate.rdb", db);
   if (!write_db_set(p, &arc, 1))
      goto fixture;
   sprintf(p, "%s/Test Cart Gate.rdb", db);
   if (!write_db_set(p, &cart, 1))
      goto fixture;

   sprintf(lpl[0], "%s/Test Arc Gate.lpl",  pl_dir);
   sprintf(lpl[1], "%s/Test Cart Gate.lpl", pl_dir);
   filestream_delete(lpl[0]);
   filestream_delete(lpl[1]);
   use_queue(threaded);
   use_db(db);
   retro_atomic_store_release_int(&reads_begun, 0);
   begun_size = ARC_ZIP_LEN;
   started    = run_scan(dir) && scan_completed;
   begun_size = 0;
   reads      = retro_atomic_load_acquire_int(&reads_begun);

   sprintf(crc, "%08X|crc", (unsigned)arc.crc);
   sprintf(what, "arcade set, cart ROM first: x.zip %s", q);
   check(   started && file_contains(lpl[0], "ac0.zip\"")
         && file_contains(lpl[0], crc)
         && !file_contains(lpl[0], "ac0.zip#")
         && !file_contains(lpl[1], "ac0.zip"), what,
         "ac0.zip in the arcade playlist, not expanded");
   sprintf(what, "arcade set, cart ROM first: read whole once %s", q);
   sprintf(detail, "%d whole reads", reads);
   check(started && reads == 1, what, detail);
   return;

fixture:
   check(0, "fixture", "could not write the arcade set");
}

/* TorrentZip arcade sets whose readers' whole reads fail: the scan reads
 * them whole itself and keeps each reader's directory, so every
 * archive's directory is read once. */
static void reader_fail_arcade_lane(const char *root, const char *pl_dir)
{
   static uint8_t d[2][MF_MEMBER_LEN];
   static char    names[GATE_ZIPS][16];
   char           db[512], dir[512], p[1024], lpl[1024], want[32];
   char           detail[64];
   db_rec_t       arc[GATE_ZIPS];
   zip_entry_t    e[2];
   unsigned       k;
   int            fails, stored;
   bool           started, all = true;

   if (!readers_expected())
   {
      check(1, "failed arcade reads: member CRC kept", "no readers");
      return;
   }
   sprintf(db,  "%s/arc_fail_db", root);
   sprintf(dir, "%s/arc_fail",    root);
   path_mkdir(db);
   path_mkdir(dir);
   memset(e, 0, sizeof(e));
   e[0].name = "a.ar";
   e[1].name = "b.ar";
   zip_comment = TZ_COMMENT;
   for (k = 0; k < GATE_ZIPS; k++)
   {
      fill_forced(d[0], MF_MEMBER_LEN, 89 + k, 0x0CA70060u + 2 * k);
      fill_forced(d[1], MF_MEMBER_LEN, 97 + k, 0x0CA70061u + 2 * k);
      e[0].data = d[0];
      e[0].len  = MF_MEMBER_LEN;
      e[0].crc  = 0x0CA70060u + 2 * k;
      e[1].data = d[1];
      e[1].len  = MF_MEMBER_LEN;
      e[1].crc  = 0x0CA70061u + 2 * k;
      sprintf(p, "%s/rf%u.zip", dir, k);
      sprintf(names[k], "Gate RF%u", k);
      if (   !write_zip_entries(p, e, 2)
          || !set_whole_rec(&arc[k], names[k], p))
         goto fixture;
   }
   zip_comment = NULL;
   sprintf(p, "%s/Test Arc Gate.rdb", db);
   if (!write_db_set(p, arc, GATE_ZIPS))
      goto fixture;

   sprintf(lpl, "%s/Test Arc Gate.lpl", pl_dir);
   filestream_delete(lpl);
   use_queue(true);
   use_db(db);
   retro_atomic_store_release_int(&reader_fails, 0);
   retro_atomic_store_release_int(&stored_reads, 0);
   fail_size   = TZ_ZIP_LEN;
   stored_name = "/arc_fail/";
   started     = run_scan(dir) && scan_completed;
   fail_size   = 0;
   stored_name = NULL;
   fails       = retro_atomic_load_acquire_int(&reader_fails);
   stored      = retro_atomic_load_acquire_int(&stored_reads);

   for (k = 0; k < GATE_ZIPS; k++)
   {
      sprintf(want, "rf%u.zip\"", k);
      all = all && file_contains(lpl, want);
   }
   check(started && all, "failed arcade reads: matched whole as x.zip",
         "rf0.zip to rf3.zip");
   sprintf(detail, "%d reader reads failed, %d directory reads", fails,
         stored);
   check(   started && fails == GATE_ZIPS && stored == GATE_ZIPS,
         "failed arcade reads: member CRC kept", detail);
   return;

fixture:
   zip_comment = NULL;
   check(0, "fixture", "could not write the failed arcade set");
}

/* Writes @n plain .ct files of MF_MEMBER_LEN bytes into @dir, the k-th
 * with CRC @crc + k, and their records. */
static bool write_plain_ct(const char *dir, unsigned n, uint32_t crc,
      db_rec_t *rec)
{
   static uint8_t d[MF_MEMBER_LEN];
   static char    names[GATE_ZIPS][16];
   char           p[1024];
   unsigned       k;

   for (k = 0; k < n; k++)
   {
      FILE *f;

      fill_forced(d, MF_MEMBER_LEN, 83 + k, crc + k);
      sprintf(p, "%s/up%u.ct", dir, k);
      if (!(f = fopen(p, "wb")))
         return false;
      fwrite(d, 1, MF_MEMBER_LEN, f);
      fclose(f);
      sprintf(names[k], "Gate UP%u", k);
      set_rec(&rec[k], names[k], crc + k, MF_MEMBER_LEN);
   }
   return true;
}

/* Plain files against a database whose crc index build is refused:
 * the refusal is kept, so each file after the first costs its one
 * query walk and no further build attempt. */
static void unindexed_plain_lane(const char *root, const char *pl_dir)
{
   char      db[512], dir[2][512], p[1024], lpl[1024], detail[64];
   db_rec_t  rec[GATE_ZIPS];
   unsigned  k, opens[2];
   bool      started[2];

   sprintf(db,     "%s/unindexed_plain_db", root);
   sprintf(dir[0], "%s/unindexed_plain_1",  root);
   sprintf(dir[1], "%s/unindexed_plain_4",  root);
   path_mkdir(db);
   path_mkdir(dir[0]);
   path_mkdir(dir[1]);
   if (   !write_plain_ct(dir[1], GATE_ZIPS, 0x0CA70050u, rec)
       || !write_plain_ct(dir[0], 1, 0x0CA70050u, rec))
      goto fixture;
   sprintf(p, "%s/Test Cart Gate.rdb", db);
   if (!write_db_set(p, rec, GATE_ZIPS))
      goto fixture;
   sprintf(lpl, "%s/Test Cart Gate.lpl", pl_dir);
   use_queue(false);
   use_db(db);

   /* One file, then four: what the last three add. */
   for (k = 0; k < 2; k++)
   {
      filestream_delete(lpl);
      index_cap  = 1;
      rdb_opens  = 0;
      started[k] = run_scan(dir[k]) && scan_completed;
      opens[k]   = rdb_opens;
      index_cap  = 0;
   }
   check(   started[0] && started[1] && file_contains(lpl, "Gate UP0")
         && file_contains(lpl, "Gate UP3"),
         "unindexed: plain files matched", "Gate UP0 to Gate UP3");
   sprintf(detail, "%u opens for 1 file, %u for %u", opens[0], opens[1],
         GATE_ZIPS);
   check(   started[0] && started[1] && opens[1] >= opens[0]
         && opens[1] - opens[0] <= GATE_ZIPS - 1,
         "unindexed: a refused index is not built again", detail);
   return;

fixture:
   check(0, "fixture", "could not write the unindexed plain set");
}

/* Arcade sets whose first members carry a claimed cartridge extension
 * and match nothing there: the sets are matched whole and the
 * cartridge database, claimed only for that extension, is never
 * opened. */
static void arc_folder_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static uint8_t d[2][MF_MEMBER_LEN];
   static char    names[GATE_ZIPS][16];
   char           db[512], dir[512], p[1024], lpl[1024], want[32];
   char           what[64], detail[64];
   db_rec_t       arc[GATE_ZIPS], cart;
   zip_entry_t    e[2];
   unsigned       k, opens;
   bool           started, all = true;
   const char    *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,  "%s/arc_folder_db", root);
   sprintf(dir, "%s/arc_folder",    root);
   path_mkdir(db);
   path_mkdir(dir);
   memset(e, 0, sizeof(e));
   e[0].name = "a.ct";
   e[1].name = "b.ar";
   for (k = 0; k < GATE_ZIPS; k++)
   {
      fill_forced(d[0], MF_MEMBER_LEN, 101 + k, 0x0CA70070u + 2 * k);
      fill_forced(d[1], MF_MEMBER_LEN, 107 + k, 0x0CA70071u + 2 * k);
      e[0].data = d[0];
      e[0].len  = MF_MEMBER_LEN;
      e[0].crc  = 0x0CA70070u + 2 * k;
      e[1].data = d[1];
      e[1].len  = MF_MEMBER_LEN;
      e[1].crc  = 0x0CA70071u + 2 * k;
      sprintf(p, "%s/af%u.zip", dir, k);
      sprintf(names[k], "Gate AF%u", k);
      if (   !write_zip_entries(p, e, 2)
          || !set_whole_rec(&arc[k], names[k], p))
         goto fixture;
   }
   set_rec(&cart, "Gate AF Cart", 0x0CA7007Fu, MF_MEMBER_LEN);
   sprintf(p, "%s/Test Arc Gate.rdb", db);
   if (!write_db_set(p, arc, GATE_ZIPS))
      goto fixture;
   sprintf(p, "%s/Test Cart Gate.rdb", db);
   if (!write_db_set(p, &cart, 1))
      goto fixture;

   sprintf(lpl, "%s/Test Arc Gate.lpl", pl_dir);
   filestream_delete(lpl);
   use_queue(threaded);
   use_db(db);
   open_name   = "Test Cart Gate";
   named_opens = 0;
   started     = run_scan(dir) && scan_completed;
   opens       = named_opens;
   open_name   = NULL;

   for (k = 0; k < GATE_ZIPS; k++)
   {
      sprintf(want, "af%u.zip\"", k);
      all = all && file_contains(lpl, want);
   }
   sprintf(what, "arcade folder: sets matched whole %s", q);
   check(started && all, what, "af0.zip to af3.zip");
   sprintf(what, "arcade folder: no cartridge database walked %s", q);
   sprintf(detail, "%u opens", opens);
   check(started && opens == 0, what, detail);
   return;

fixture:
   check(0, "fixture", "could not write the arcade folder");
}

/* Cartridge zips whose database is claimed only for the member's
 * extension, with every index build refused.  The member pass leaves
 * that database out, so each zip costs the walks the expansion makes
 * and no more. */
static void unindexed_member_lane(const char *root, const char *pl_dir)
{
   static const char *const file[GATE_ZIPS] = {
      "ui0.zip", "ui1.zip", "ui2.zip", "ui3.zip" };
   static const char *const name[GATE_ZIPS] = {
      "Gate UI0", "Gate UI1", "Gate UI2", "Gate UI3" };
   char      db[512], dir[2][512], p[1024], lpl[1024], want[32];
   char      detail[64];
   db_rec_t  rec[GATE_ZIPS];
   unsigned  k, opens[2];
   bool      started[2], all = true;

   sprintf(db,     "%s/unindexed_db", root);
   sprintf(dir[0], "%s/unindexed_1",  root);
   sprintf(dir[1], "%s/unindexed_4",  root);
   path_mkdir(db);
   path_mkdir(dir[0]);
   path_mkdir(dir[1]);
   for (k = 0; k < GATE_ZIPS; k++)
      if (   !write_gate_zip(dir[1], file[k], "rom.ct", name[k],
               0x0CA70040u + k, &rec[k])
          || (!k && !write_gate_zip(dir[0], file[k], "rom.ct", name[k],
               0x0CA70040u + k, &rec[k])))
         goto fixture;
   sprintf(p, "%s/Test Cart Gate.rdb", db);
   if (!write_db_set(p, rec, GATE_ZIPS))
      goto fixture;
   sprintf(lpl, "%s/Test Cart Gate.lpl", pl_dir);
   use_queue(false);
   use_db(db);

   /* One zip, then four: what the last three add. */
   for (k = 0; k < 2; k++)
   {
      filestream_delete(lpl);
      index_cap  = 1;
      rdb_opens  = 0;
      started[k] = run_scan(dir[k]) && scan_completed;
      opens[k]   = rdb_opens;
      index_cap  = 0;
   }
   for (k = 0; k < GATE_ZIPS; k++)
   {
      sprintf(want, "%s#rom.ct", file[k]);
      all = all && file_contains(lpl, want);
   }
   check(started[0] && started[1] && all,
         "unindexed: cart zips matched as x.zip#member",
         "ui0.zip#rom.ct to ui3.zip#rom.ct");
   sprintf(detail, "%u opens for 1 zip, %u for %u", opens[0], opens[1],
         GATE_ZIPS);
   check(   started[0] && started[1] && opens[1] >= opens[0]
         && opens[1] - opens[0] <= GATE_ZIPS - 1,
         "unindexed: one database walk per later zip", detail);
   return;

fixture:
   check(0, "fixture", "could not write the unindexed set");
}

/* An archive of several members that no database holds by its size, a
 * disc zip as a rule: no whole read on either side, and a later
 * member's hit held as archive#member. */
static void multi_nosize_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static uint8_t d[2][MF_MEMBER_LEN];
   static char    lpl[2][1024];
   char           db[512], dir[512], p[1024];
   char           what[80], detail[64];
   db_rec_t       arc[2], cart;
   zip_entry_t    e[2];
   uint32_t       bytes;
   int            reads;
   bool           started;
   const char    *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,  "%s/multi_nosize_db", root);
   sprintf(dir, "%s/multi_nosize",    root);
   path_mkdir(db);
   path_mkdir(dir);
   fill_forced(d[0], MF_MEMBER_LEN, 131, 0x0CA70100u);
   fill_forced(d[1], MF_MEMBER_LEN, 137, 0x0CA70101u);
   memset(e, 0, sizeof(e));
   e[0].name = "Game.cue";
   e[0].data = d[0];
   e[0].len  = MF_MEMBER_LEN;
   e[0].crc  = 0x0CA70100u;
   e[1].name = "Game (Track 1).ct";
   e[1].data = d[1];
   e[1].len  = MF_MEMBER_LEN;
   e[1].crc  = 0x0CA70101u;
   sprintf(p, "%s/mn_disc.zip", dir);
   if (!write_zip_entries(p, e, 2))
      goto fixture;
   bytes = zip_bytes(e, 2);
   /* Zip records on both sides of its size: inside the size range, and
    * no record of its size. */
   set_rec(&arc[0], "MN Small", 0x0CA701F8u, bytes - 1);
   set_rec(&arc[1], "MN Big",   0x0CA701F9u, bytes + 1);
   arc[0].rom_name = "small.zip";
   arc[1].rom_name = "big.zip";
   set_rec(&cart, "MN Track", 0x0CA70101u, MF_MEMBER_LEN);
   sprintf(p, "%s/Test Arc Gate.rdb", db);
   if (!write_db_set(p, arc, 2))
      goto fixture;
   sprintf(p, "%s/Test Cart Gate.rdb", db);
   if (!write_db_set(p, &cart, 1))
      goto fixture;

   sprintf(lpl[0], "%s/Test Arc Gate.lpl",  pl_dir);
   sprintf(lpl[1], "%s/Test Cart Gate.lpl", pl_dir);
   filestream_delete(lpl[0]);
   filestream_delete(lpl[1]);
   use_queue(threaded);
   use_db(db);
   retro_atomic_store_release_int(&watch_scan,   0);
   retro_atomic_store_release_int(&watch_reader, 0);
   watch_size = bytes;
   started    = run_scan(dir) && scan_completed;
   watch_size = 0;
   reads      = retro_atomic_load_acquire_int(&watch_scan)
      + retro_atomic_load_acquire_int(&watch_reader);

   sprintf(what, "several members, no size: a later hit held %s", q);
   check(   started
         && file_contains(lpl[1], "mn_disc.zip#Game (Track 1).ct")
         && !file_contains(lpl[0], "mn_disc.zip"), what,
         "mn_disc.zip#Game (Track 1).ct");
   sprintf(what, "several members, no size: not read whole %s", q);
   sprintf(detail, "%d read steps", reads);
   check(started && reads == 0, what, detail);
   return;

fixture:
   check(0, "fixture", "could not write the no-size set");
}

/* TorrentZip sets of several members are read whole by the readers
 * with no grant while a database opens zip; a TorrentZip zip of one
 * member is not; plain ones only by the scan, once the size lets it;
 * neither while no database opens zip.  No record matches, so no
 * whole match grants a read ahead. */
static void tz_reader_lane(const char *root)
{
   static uint8_t d[2][MF_MEMBER_LEN];
   char           db[2][512], dir[2][512], p[1024], detail[64];
   db_rec_t       arc[2], cart;
   zip_entry_t    e[2];
   uint32_t       one_bytes;
   unsigned       k, j;
   int            scan, rdr, one;
   bool           started;

   if (!readers_expected())
   {
      check(1, "TorrentZip sets: readers read them whole", "no readers");
      return;
   }
   sprintf(db[0],  "%s/tz_reader_db",    root);
   sprintf(db[1],  "%s/tz_reader_nozip", root);
   sprintf(dir[0], "%s/tz_reader_tz",    root);
   sprintf(dir[1], "%s/tz_reader_plain", root);
   for (j = 0; j < 2; j++)
   {
      path_mkdir(db[j]);
      path_mkdir(dir[j]);
   }
   memset(e, 0, sizeof(e));
   e[0].name = "a.ar";
   e[1].name = "b.ar";
   for (k = 0; k < GATE_ZIPS; k++)
   {
      fill_forced(d[0], MF_MEMBER_LEN, 141 + k, 0x0CA70110u + 2 * k);
      fill_forced(d[1], MF_MEMBER_LEN, 149 + k, 0x0CA70111u + 2 * k);
      e[0].data = d[0];
      e[0].len  = MF_MEMBER_LEN;
      e[0].crc  = 0x0CA70110u + 2 * k;
      e[1].data = d[1];
      e[1].len  = MF_MEMBER_LEN;
      e[1].crc  = 0x0CA70111u + 2 * k;
      for (j = 0; j < 2; j++)
      {
         zip_comment = j ? NULL : TZ_COMMENT;
         sprintf(p, "%s/tzr%u.zip", dir[j], k);
         if (!write_zip_entries(p, e, 2))
            goto fixture;
      }
   }
   fill_forced(d[0], MF_MEMBER_LEN, 157, 0x0CA7011Fu);
   e[0].crc   = 0x0CA7011Fu;
   zip_comment = TZ_COMMENT;
   one_bytes   = zip_bytes(e, 1);
   sprintf(p, "%s/tzr_one.zip", dir[0]);
   if (!write_zip_entries(p, e, 1))
      goto fixture;
   zip_comment = NULL;
   /* Zip records of both sizes whose CRCs match nothing. */
   set_rec(&arc[0], "TZR TZ Size",    0x0CA701F0u, TZ_ZIP_LEN);
   set_rec(&arc[1], "TZR Plain Size", 0x0CA701F1u, ARC_ZIP_LEN);
   arc[0].rom_name = "set.zip";
   arc[1].rom_name = "set.zip";
   sprintf(p, "%s/Test Arc Gate.rdb", db[0]);
   if (!write_db_set(p, arc, 2))
      goto fixture;
   set_rec(&cart, "TZR Cart", 0x0CA701F2u, MF_MEMBER_LEN);
   sprintf(p, "%s/Test Cart Gate.rdb", db[1]);
   if (!write_db_set(p, &cart, 1))
      goto fixture;

   use_queue(true);
   for (j = 0; j < 3; j++)
   {
      bool tz = (j != 1);

      use_db(db[j == 2]);
      retro_atomic_store_release_int(&reads_begun,  0);
      retro_atomic_store_release_int(&reader_begun, 0);
      retro_atomic_store_release_int(&watch_scan,   0);
      retro_atomic_store_release_int(&watch_reader, 0);
      begun_size = tz ? TZ_ZIP_LEN : ARC_ZIP_LEN;
      watch_size = one_bytes;
      started    = run_scan(dir[tz ? 0 : 1]) && scan_completed;
      begun_size = 0;
      watch_size = 0;
      rdr        = retro_atomic_load_acquire_int(&reader_begun);
      scan       = retro_atomic_load_acquire_int(&reads_begun) - rdr;
      one        = retro_atomic_load_acquire_int(&watch_scan)
         + retro_atomic_load_acquire_int(&watch_reader);
      sprintf(detail, "scan %d, readers %d", scan, rdr);
      if (j == 0)
      {
         check(started && scan == 0 && rdr == GATE_ZIPS,
               "TorrentZip sets: readers read them whole", detail);
         sprintf(detail, "%d read steps", one);
         check(started && one == 0,
               "a TorrentZip zip of one member: not read whole", detail);
      }
      else if (j == 1)
         check(started && scan == GATE_ZIPS && rdr == 0,
               "plain sets: the scan reads them once sized", detail);
      else
         check(started && scan == 0 && rdr == 0,
               "TorrentZip sets: no zip database, no read", detail);
   }
   return;

fixture:
   zip_comment = NULL;
   check(0, "fixture", "could not write the TorrentZip sets");
}

#define TZU_ZIPS    24
#define TZU_SLOW_MS 150

/* TorrentZip sets a zip database cannot hold by their size: once the
 * scan has kept one from a whole read, the readers stop reading the
 * next ones whole, whose CRCs the scan would drop.  Their whole reads
 * are slowed, so each reader has one under way when the scan rules on
 * the first set and may begin one more before it does: two rounds,
 * three allowed, of TZU_ZIPS without the stop. */
static void tz_unsized_lane(const char *root)
{
   static uint8_t d[2][MF_MEMBER_LEN];
   char           db[512], dir[512], p[1024], detail[64];
   db_rec_t       arc;
   zip_entry_t    e[2];
   unsigned       k, n = readers_expected();
   int            scan, rdr;
   bool           started;

   if (!n)
   {
      check(1, "unsized TorrentZip sets: readers stop reading whole",
            "no readers");
      return;
   }
   sprintf(db,  "%s/tz_unsized_db", root);
   sprintf(dir, "%s/tz_unsized",    root);
   path_mkdir(db);
   path_mkdir(dir);
   memset(e, 0, sizeof(e));
   e[0].name   = "a.ar";
   e[1].name   = "b.ar";
   zip_comment = TZ_COMMENT;
   for (k = 0; k < TZU_ZIPS; k++)
   {
      fill_forced(d[0], MF_MEMBER_LEN, 161 + k, 0x0CA70200u + 2 * k);
      fill_forced(d[1], MF_MEMBER_LEN, 163 + k, 0x0CA70201u + 2 * k);
      e[0].data = d[0];
      e[0].len  = MF_MEMBER_LEN;
      e[0].crc  = 0x0CA70200u + 2 * k;
      e[1].data = d[1];
      e[1].len  = MF_MEMBER_LEN;
      e[1].crc  = 0x0CA70201u + 2 * k;
      sprintf(p, "%s/tzu%02u.zip", dir, k);
      if (!write_zip_entries(p, e, 2))
         goto fixture;
   }
   zip_comment = NULL;
   /* A zip record of another size, whose CRC matches nothing. */
   set_rec(&arc, "TZU Other Size", 0x0CA702F0u, TZ_ZIP_LEN + 1);
   arc.rom_name = "set.zip";
   sprintf(p, "%s/Test Arc Gate.rdb", db);
   if (!write_db_set(p, &arc, 1))
      goto fixture;

   use_queue(true);
   use_db(db);
   retro_atomic_store_release_int(&reads_begun,  0);
   retro_atomic_store_release_int(&reader_begun, 0);
   begun_size = TZ_ZIP_LEN;
   slow_reset(TZ_ZIP_LEN, TZU_SLOW_MS);
   started    = run_scan(dir) && scan_completed;
   slow_size  = 0;
   begun_size = 0;
   rdr        = retro_atomic_load_acquire_int(&reader_begun);
   scan       = retro_atomic_load_acquire_int(&reads_begun) - rdr;
   sprintf(detail, "scan %d, readers %d of %u (%u readers)", scan, rdr,
         TZU_ZIPS, n);
   check(started && scan == 0 && rdr <= (int)(3 * n),
         "unsized TorrentZip sets: readers stop reading whole", detail);
   return;

fixture:
   zip_comment = NULL;
   check(0, "fixture", "could not write the unsized TorrentZip sets");
}

/* Several members, no database holds the archive by its size, and its
 * first member's CRC is in one whose cores open zips only: the archive
 * is that record's entry, with no whole read. */
static void q1a_lane(const char *root, const char *pl_dir, bool threaded)
{
   static uint8_t d[2][MF_MEMBER_LEN];
   char           db[512], dir[512], p[1024], lpl[1024];
   char           what[80], detail[64];
   db_rec_t       rec;
   zip_entry_t    e[2];
   int            reads;
   bool           started;
   const char    *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,  "%s/q1a_db", root);
   sprintf(dir, "%s/q1a",    root);
   path_mkdir(db);
   path_mkdir(dir);
   fill_forced(d[0], MF_MEMBER_LEN, 151, 0x0CA70120u);
   fill_forced(d[1], MF_MEMBER_LEN, 157, 0x0CA70121u);
   memset(e, 0, sizeof(e));
   e[0].name = "disk1.ct";
   e[0].data = d[0];
   e[0].len  = MF_MEMBER_LEN;
   e[0].crc  = 0x0CA70120u;
   e[1].name = "disk2.ct";
   e[1].data = d[1];
   e[1].len  = MF_MEMBER_LEN;
   e[1].crc  = 0x0CA70121u;
   sprintf(p, "%s/q1a.zip", dir);
   if (!write_zip_entries(p, e, 2))
      goto fixture;
   set_rec(&rec, "Q1A Disk 1", 0x0CA70120u, MF_MEMBER_LEN);
   sprintf(p, "%s/Test Arc Gate.rdb", db);
   if (!write_db_set(p, &rec, 1))
      goto fixture;

   sprintf(lpl, "%s/Test Arc Gate.lpl", pl_dir);
   filestream_delete(lpl);
   use_queue(threaded);
   use_db(db);
   retro_atomic_store_release_int(&watch_scan,   0);
   retro_atomic_store_release_int(&watch_reader, 0);
   watch_size = zip_bytes(e, 2);
   started    = run_scan(dir) && scan_completed;
   watch_size = 0;
   reads      = retro_atomic_load_acquire_int(&watch_scan)
      + retro_atomic_load_acquire_int(&watch_reader);

   sprintf(what, "first member's CRC where zips open: x.zip %s", q);
   check(   started && file_contains(lpl, "Q1A Disk 1")
         && file_contains(lpl, "q1a.zip\"")
         && !file_contains(lpl, "q1a.zip#"), what, "q1a.zip");
   sprintf(what, "first member's CRC where zips open: no read %s", q);
   sprintf(detail, "%d read steps", reads);
   check(started && reads == 0, what, detail);
   return;

fixture:
   check(0, "fixture", "could not write the first-member set");
}

#ifdef HAVE_RRAR
/* Nothing hits in an archive of several members and a later member
 * stores no CRC: the expansion, which unpacks it for one, still looks
 * it up.  With every CRC stored and nothing hit, nothing expands. */
static void crc0_lane(const char *root, const char *pl_dir, bool threaded)
{
   static uint8_t d[4][MF_MEMBER_LEN];
   static const uint32_t crc[4] = {
      0x0CA70130u, 0x0CA70131u, 0x0CA70132u, 0x0CA70133u };
   char           db[512], dir[512], p[1024], lpl[1024];
   char           what[80], detail[64];
   db_rec_t       rec;
   zip_entry_t    e[2];
   int            stored;
   bool           started;
   unsigned       j;
   const char    *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,  "%s/crc0_db", root);
   sprintf(dir, "%s/crc0",    root);
   path_mkdir(db);
   path_mkdir(dir);
   for (j = 0; j < 4; j++)
      fill_forced(d[j], MF_MEMBER_LEN, 161 + 2 * j, crc[j]);
   memset(e, 0, sizeof(e));
   e[0].name = "a.ct";
   e[0].len  = MF_MEMBER_LEN;
   e[1].name = "b.ct";
   e[1].len  = MF_MEMBER_LEN;

   /* b.ct stores no CRC; its bytes' CRC is the record's. */
   e[0].data = d[0];
   e[0].crc  = crc[0];
   e[1].data = d[1];
   e[1].crc  = 0;
   sprintf(p, "%s/c0_nocrc.rar", dir);
   if (!write_rar5_entries(p, e, 2))
      goto fixture;
   /* Every CRC stored, none a record's. */
   e[0].data = d[2];
   e[0].crc  = crc[2];
   e[1].data = d[3];
   e[1].crc  = crc[3];
   sprintf(p, "%s/c0_stored.rar", dir);
   if (!write_rar5_entries(p, e, 2))
      goto fixture;
   set_rec(&rec, "C0 Unpacked", crc[1], MF_MEMBER_LEN);
   sprintf(p, "%s/Test Cart Gate.rdb", db);
   if (!write_db_set(p, &rec, 1))
      goto fixture;

   sprintf(lpl, "%s/Test Cart Gate.lpl", pl_dir);
   filestream_delete(lpl);
   use_queue(threaded);
   use_db(db);
   retro_atomic_store_release_int(&stored_reads, 0);
   stored_name = "c0_stored.rar#";
   started     = run_scan(dir) && scan_completed;
   stored_name = NULL;
   stored      = retro_atomic_load_acquire_int(&stored_reads);

   sprintf(what, "a later member stores no CRC: expanded %s", q);
   check(   started && file_contains(lpl, "C0 Unpacked")
         && file_contains(lpl, "c0_nocrc.rar#b.ct"), what,
         "c0_nocrc.rar#b.ct");
   sprintf(what, "every CRC stored, nothing hit: not expanded %s", q);
   sprintf(detail, "%d member lookups", stored);
   check(started && stored == 0, what, detail);
   return;

fixture:
   check(0, "fixture", "could not write the CRC-0 set");
}
#endif

/* A zip of a folder and one ROM has one member, the ROM, which is looked
 * up first: its held hit is the entry and the zip is not read whole,
 * even where a database that opens zips holds a zip of its size. */
static void folder_first_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static uint8_t d[MF_MEMBER_LEN];
   char           db[2][512], dir[512], p[1024], lpl[1024];
   char           what[80], detail[64];
   db_rec_t       arc[2], cart;
   zip_entry_t    e[2];
   uint32_t       bytes;
   unsigned       j;
   int            reads;
   bool           started;
   const char    *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db[0], "%s/folder_first_db",   root);
   sprintf(db[1], "%s/folder_first_size", root);
   sprintf(dir,   "%s/folder_first",      root);
   path_mkdir(db[0]);
   path_mkdir(db[1]);
   path_mkdir(dir);
   fill_forced(d, MF_MEMBER_LEN, 171, 0x0CA70140u);
   memset(e, 0, sizeof(e));
   e[0].name = "dir/";
   e[1].name = "dir/a.ct";
   e[1].data = d;
   e[1].len  = MF_MEMBER_LEN;
   e[1].crc  = 0x0CA70140u;
   sprintf(p, "%s/ff.zip", dir);
   if (!write_zip_entries(p, e, 2))
      goto fixture;
   bytes = zip_bytes(e, 2);
   set_rec(&cart, "FF ROM", 0x0CA70140u, MF_MEMBER_LEN);
   /* Zip records on both sides of its size, then one of its size; no
    * CRC of theirs matches. */
   set_rec(&arc[0], "FF Small", 0x0CA701E0u, bytes - 1);
   set_rec(&arc[1], "FF Big",   0x0CA701E1u, bytes + 1);
   arc[0].rom_name = "small.zip";
   arc[1].rom_name = "big.zip";
   sprintf(p, "%s/Test Arc Gate.rdb", db[0]);
   if (!write_db_set(p, arc, 2))
      goto fixture;
   set_rec(&arc[0], "FF Sized", 0x0CA701E2u, bytes);
   arc[0].rom_name = "ff.zip";
   sprintf(p, "%s/Test Arc Gate.rdb", db[1]);
   if (!write_db_set(p, arc, 1))
      goto fixture;
   for (j = 0; j < 2; j++)
   {
      sprintf(p, "%s/Test Cart Gate.rdb", db[j]);
      if (!write_db_set(p, &cart, 1))
         goto fixture;
   }

   sprintf(lpl, "%s/Test Cart Gate.lpl", pl_dir);
   use_queue(threaded);
   for (j = 0; j < 2; j++)
   {
      const char *s = j ? "sized" : "no size";

      filestream_delete(lpl);
      use_db(db[j]);
      retro_atomic_store_release_int(&watch_scan,   0);
      retro_atomic_store_release_int(&watch_reader, 0);
      watch_size = bytes;
      started    = run_scan(dir) && scan_completed;
      watch_size = 0;
      reads      = retro_atomic_load_acquire_int(&watch_scan)
         + retro_atomic_load_acquire_int(&watch_reader);

      sprintf(what, "folder first, %s: its ROM is its entry %s", s, q);
      check(started && file_contains(lpl, "ff.zip#dir/a.ct"), what,
            "ff.zip#dir/a.ct");
      sprintf(what, "folder first, %s: not read whole %s", s, q);
      sprintf(detail, "%d read steps", reads);
      check(started && reads == 0, what, detail);
   }
   return;

fixture:
   check(0, "fixture", "could not write the folder-first zip");
}

/* A loose scan with a held first-member hit: the expansion writes it as
 * archive#member in its database's playlist, and the archive is no
 * entry itself. */
static void loose_held_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static uint8_t d[MF_MEMBER_LEN];
   static char    cart_lpl[1024], loose_lpl[1024];
   char           db[512], dir[512], pl[512], p[1024], what[80];
   db_rec_t       cart;
   zip_entry_t    e;
   bool           started;
   const char    *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,  "%s/loose_held_db", root);
   sprintf(dir, "%s/loose_held",    root);
   sprintf(pl,  "%s/loose_held_pl", root);
   path_mkdir(db);
   path_mkdir(dir);
   path_mkdir(pl);
   fill_forced(d, MF_MEMBER_LEN, 181, 0x0CA70150u);
   memset(&e, 0, sizeof(e));
   e.name = "a.ct";
   e.data = d;
   e.len  = MF_MEMBER_LEN;
   e.crc  = 0x0CA70150u;
   sprintf(p, "%s/lh.zip", dir);
   if (!write_zip_entries(p, &e, 1))
      goto fixture;
   set_rec(&cart, "LH Cart", 0x0CA70150u, MF_MEMBER_LEN);
   sprintf(p, "%s/Test Cart Gate.rdb", db);
   if (!write_db_set(p, &cart, 1))
      goto fixture;

   sprintf(cart_lpl,  "%s/Test Cart Gate.lpl", pl);
   sprintf(loose_lpl, "%s/loose_held.lpl",     pl);
   filestream_delete(cart_lpl);
   filestream_delete(loose_lpl);
   use_queue(threaded);
   use_db(db);
   strlcpy(config_get_ptr()->paths.directory_playlist, pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   /* Automatic would make it strict. */
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_CUSTOM);
   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_LOOSE);
   started = run_scan(dir) && scan_completed;
   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_STRICT);
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC);
   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));

   sprintf(what, "loose, a held hit: archive#member %s", q);
   check(   started && file_contains(cart_lpl, "lh.zip#a.ct")
         && !file_contains(cart_lpl, "lh.zip\"")
         && !file_contains(loose_lpl, "lh.zip\""), what, "lh.zip#a.ct");
   return;

fixture:
   check(0, "fixture", "could not write the loose held set");
}

#ifdef HAVE_RRAR
/* Rar archives take the archive passes: a first member's hit, in a
 * database whose core opens rar, names the archive; a later member's,
 * in one whose core does not, is held; an archive of several members
 * is read whole only while a database whose cores open rar holds a rar
 * record of its size. */
static void rar_lane(const char *root, const char *pl_dir, bool threaded)
{
   static uint8_t d[4][MF_MEMBER_LEN];
   static const uint32_t crc[4] = {
      0x0BF80001u, 0x0BF80011u, 0x0BF80012u, 0x0BF80002u };
   char           db[512], noclaim[512], dir[512], p[1024];
   static char    cart_lpl[1024], member_lpl[1024];
   char           what[64], detail[64];
   db_rec_t       cart[3], held, nc;
   zip_entry_t    e[2];
   uint32_t       cart_bytes, multi_bytes;
   int            cart_reads, multi_reads;
   bool           started;
   unsigned       j;
   const char    *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,      "%s/rar_db",         root);
   sprintf(noclaim, "%s/rar_noclaim_db", root);
   sprintf(dir,     "%s/rar",            root);
   path_mkdir(db);
   path_mkdir(noclaim);
   path_mkdir(dir);
   for (j = 0; j < 4; j++)
      fill_forced(d[j], MF_MEMBER_LEN, 101 + 2 * j, crc[j]);
   memset(e, 0, sizeof(e));
   e[0].len = MF_MEMBER_LEN;
   e[1].len = MF_MEMBER_LEN;

   e[0].name = "m.rd";
   e[0].data = d[0];
   e[0].crc  = crc[0];
   sprintf(p, "%s/rar_cart.rar", dir);
   if (!write_rar_entries(p, e, 1))
      goto fixture;
   cart_bytes = rar_bytes(e, 1);
   set_rec(&cart[0], "RAR Cart", crc[0], MF_MEMBER_LEN);

   e[0].name = "a.rd";
   e[0].data = d[1];
   e[0].crc  = crc[1];
   e[1].name = "b.rd";
   e[1].data = d[2];
   e[1].crc  = crc[2];
   sprintf(p, "%s/rar_multi.rar", dir);
   if (!write_rar_entries(p, e, 2))
      goto fixture;
   multi_bytes = rar_bytes(e, 2);
   /* A rar record of its size: the size gate lets it be read whole. */
   set_rec(&cart[1], "RAR Multi Sized", 0x0BF8FFFEu, multi_bytes);
   cart[1].rom_name = "set.rar";
   set_rec(&held, "RAR Held", crc[2], MF_MEMBER_LEN);
   /* A rar record of that size whose core does not open rar. */
   set_rec(&nc, "RAR NoClaim Sized", 0x0BF8FFFCu, multi_bytes);
   nc.rom_name = "set.rar";

   /* A folder is no member: its one member's hit names it. */
   e[0].name = "d/";
   e[0].data = NULL;
   e[0].len  = 0;
   e[0].crc  = 0;
   e[1].name = "d/m.rd";
   e[1].data = d[3];
   e[1].crc  = crc[3];
   sprintf(p, "%s/rar_folder.rar", dir);
   if (   !write_rar_entries(p, e, 2)
       || path_get_size(p) != (int64_t)rar_bytes(e, 2))
      goto fixture;
   set_rec(&cart[2], "RAR Folder", crc[3], MF_MEMBER_LEN);

   sprintf(p, "%s/Test RAR Cart.rdb", db);
   if (!write_db_set(p, cart, 3))
      goto fixture;
   sprintf(p, "%s/Test EM Member.rdb", db);
   if (!write_db_set(p, &held, 1))
      goto fixture;
   sprintf(p, "%s/Test EM Member.rdb", noclaim);
   if (!write_db_set(p, &held, 1))
      goto fixture;
   sprintf(p, "%s/Test EM Cart.rdb", noclaim);
   if (!write_db_set(p, &nc, 1))
      goto fixture;

   sprintf(cart_lpl,   "%s/Test RAR Cart.lpl",  pl_dir);
   sprintf(member_lpl, "%s/Test EM Member.lpl", pl_dir);
   filestream_delete(cart_lpl);
   filestream_delete(member_lpl);
   use_queue(threaded);

   use_db(db);
   retro_atomic_store_release_int(&reads_begun, 0);
   retro_atomic_store_release_int(&watch_scan,   0);
   retro_atomic_store_release_int(&watch_reader, 0);
   begun_size  = multi_bytes;
   watch_size  = cart_bytes;
   started     = run_scan(dir) && scan_completed;
   begun_size  = 0;
   watch_size  = 0;
   cart_reads  = retro_atomic_load_acquire_int(&watch_scan)
      + retro_atomic_load_acquire_int(&watch_reader);
   multi_reads = retro_atomic_load_acquire_int(&reads_begun);
   sprintf(what, "rar: a first member's hit names it %s", q);
   check(   started && file_contains(cart_lpl, "RAR Cart")
         && file_contains(cart_lpl, "rar_cart.rar")
         && !file_contains(cart_lpl, "rar_cart.rar#"), what,
         "rar_cart.rar");
   sprintf(what, "rar, a folder first: named by its member %s", q);
   check(   started && file_contains(cart_lpl, "RAR Folder")
         && file_contains(cart_lpl, "rar_folder.rar")
         && !file_contains(cart_lpl, "rar_folder.rar#"), what,
         "rar_folder.rar");
   sprintf(what, "rar: a later member's hit is held %s", q);
   check(started && file_contains(member_lpl, "rar_multi.rar#b.rd"),
         what, "rar_multi.rar#b.rd");
   if (!threaded)
   {
      sprintf(detail, "%d read steps", cart_reads);
      check(started && cart_reads == 0,
            "rar: a first member's hit reads it not whole", detail);
      sprintf(detail, "%d whole reads", multi_reads);
      check(started && multi_reads == 1,
            "rar: read whole while a rar record has its size", detail);
   }

   filestream_delete(member_lpl);
   use_db(noclaim);
   retro_atomic_store_release_int(&reads_begun, 0);
   begun_size  = multi_bytes;
   started     = run_scan(dir) && scan_completed;
   begun_size  = 0;
   multi_reads = retro_atomic_load_acquire_int(&reads_begun);
   sprintf(what, "rar: no core opens rar: held %s", q);
   check(started && file_contains(member_lpl, "rar_multi.rar#b.rd"),
         what, "rar_multi.rar#b.rd");
   if (!threaded)
   {
      sprintf(detail, "%d whole reads", multi_reads);
      check(started && multi_reads == 0,
            "rar: no core opens rar: not read whole", detail);
   }
   return;

fixture:
   check(0, "fixture", "could not write the rar set");
}
#endif

/* Archives whose first member misses, looked up member by member from
 * the directory.  One record whose rom_name is the archive's own name
 * with an archive extension, of a database whose cores open zips,
 * names the archive; other hits, a disc track's and a set inside
 * another archive among them, are held, written as archive#member
 * once the archive's own CRC has missed, and lose to a match on it.
 * A member that stores no CRC, here the first, is not looked up. */
static void every_member_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static uint8_t d[14][MF_MEMBER_LEN];
   static const uint32_t crc[14] = {
      0x0BF50001u, 0x0BF50002u, 0x0BF50011u, 0x0BF50012u, 0x0BF50021u,
      0x0BF50022u, 0x0BF50031u, 0x0BF50032u, 0x0BF50042u, 0x0BF50052u,
      0x0BF50062u, 0x0BF50072u, 0x0BF50082u, 0x0BF50092u };
   char           db[512], dir[512];
   static char    p[1024];
   static char    cart_lpl[1024], member_lpl[1024], arcade_lpl[1024];
   char           what[64], detail[64];
   db_rec_t       cart[7], member[5], arcade;
   zip_entry_t    e[3];
   uint32_t       set_bytes, held_bytes;
   int            set_reads, held_reads;
   bool           started;
   unsigned       j;
   const char    *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,  "%s/every_member_db", root);
   sprintf(dir, "%s/every_member",    root);
   path_mkdir(db);
   path_mkdir(dir);
   for (j = 0; j < 14; j++)
      fill_forced(d[j], MF_MEMBER_LEN, 83 + 2 * j, crc[j]);
   memset(e, 0, sizeof(e));
   e[0].len = MF_MEMBER_LEN;
   e[1].len = MF_MEMBER_LEN;

   /* The second member is a record of a core that opens zips, named
    * for no archive. */
   e[0].name = "a.rd";
   e[0].data = d[0];
   e[0].crc  = crc[0];
   e[1].name = "b.rd";
   e[1].data = d[1];
   e[1].crc  = crc[1];
   sprintf(p, "%s/em_late.zip", dir);
   if (!write_zip_entries(p, e, 2))
      goto fixture;
   set_rec(&cart[0], "EM Late", crc[1], MF_MEMBER_LEN);

   /* A disc's track, in a database whose core lists zip. */
   e[0].name = "Game.cue";
   e[0].data = d[0];
   e[0].crc  = crc[0];
   e[1].name = "Game (Track 3).bin";
   e[1].data = d[9];
   e[1].crc  = crc[9];
   sprintf(p, "%s/em_game.zip", dir);
   if (!write_zip_entries(p, e, 2))
      goto fixture;
   set_rec(&cart[3], "EM Disc", crc[9], MF_MEMBER_LEN);
   cart[3].rom_name = "Game (Track 3).bin";

   /* The second member is a record named for this archive, in another
    * case and type. */
   e[0].name = "sa.rd";
   e[0].data = d[0];
   e[0].crc  = crc[0];
   e[1].name = "sb1.rd";
   e[1].data = d[10];
   e[1].crc  = crc[10];
   sprintf(p, "%s/em_set.zip", dir);
   if (!write_zip_entries(p, e, 2))
      goto fixture;
   set_bytes = zip_bytes(e, 2);
   set_rec(&cart[4], "EM Set", crc[10], MF_MEMBER_LEN);
   cart[4].rom_name = "EM_Set.7z";

   /* A set inside another archive: the record names the member. */
   e[0].name = "outer.rd";
   e[0].data = d[0];
   e[0].crc  = crc[0];
   e[1].name = "set.zip";
   e[1].data = d[11];
   e[1].crc  = crc[11];
   sprintf(p, "%s/em_outer.zip", dir);
   if (!write_zip_entries(p, e, 2))
      goto fixture;
   set_rec(&cart[5], "EM Nested", crc[11], MF_MEMBER_LEN);
   cart[5].rom_name = "set.zip";

   /* A record named set.zip is not for settle.zip. */
   e[0].name = "sl.rd";
   e[0].data = d[0];
   e[0].crc  = crc[0];
   e[1].name = "st.rd";
   e[1].data = d[12];
   e[1].crc  = crc[12];
   sprintf(p, "%s/settle.zip", dir);
   if (!write_zip_entries(p, e, 2))
      goto fixture;
   set_rec(&cart[6], "EM Settle", crc[12], MF_MEMBER_LEN);
   cart[6].rom_name = "set.zip";

   /* A record named for this archive, of a core that does not open
    * zips. */
   e[0].name = "nz0.rd";
   e[0].data = d[0];
   e[0].crc  = crc[0];
   e[1].name = "nz1.rd";
   e[1].data = d[13];
   e[1].crc  = crc[13];
   sprintf(p, "%s/em_nozip.zip", dir);
   if (!write_zip_entries(p, e, 2))
      goto fixture;
   set_rec(&member[4], "EM NoZip", crc[13], MF_MEMBER_LEN);
   member[4].rom_name = "em_nozip.zip";

   /* Two records of a core that does not open zips. */
   e[0].name = "x1.rd";
   e[0].data = d[2];
   e[0].crc  = crc[2];
   e[1].name = "x2.rd";
   e[1].data = d[3];
   e[1].crc  = crc[3];
   sprintf(p, "%s/em_held.zip", dir);
   if (!write_zip_entries(p, e, 2))
      goto fixture;
   held_bytes = zip_bytes(e, 2);
   set_rec(&member[0], "EM Held One", crc[2], MF_MEMBER_LEN);
   set_rec(&member[1], "EM Held Two", crc[3], MF_MEMBER_LEN);

   /* Two records named for this archive, though their core opens
    * zips; its first member misses. */
   e[0].name = "two0.rd";
   e[0].data = d[0];
   e[0].crc  = crc[0];
   e[1].name = "two1.rd";
   e[1].data = d[4];
   e[1].crc  = crc[4];
   e[2].name = "two2.rd";
   e[2].data = d[5];
   e[2].len  = MF_MEMBER_LEN;
   e[2].crc  = crc[5];
   sprintf(p, "%s/em_two.zip", dir);
   if (!write_zip_entries(p, e, 3))
      goto fixture;
   set_rec(&cart[1], "EM Two A", crc[4], MF_MEMBER_LEN);
   set_rec(&cart[2], "EM Two B", crc[5], MF_MEMBER_LEN);
   cart[1].rom_name = "em_two.zip";
   cart[2].rom_name = "em_two.zip";

   /* A member hit to hold, and the archive's own CRC matches. */
   e[0].name = "whole1.rd";
   e[0].data = d[6];
   e[0].crc  = crc[6];
   e[1].name = "whole2.rd";
   e[1].data = d[7];
   e[1].crc  = crc[7];
   sprintf(p, "%s/em_whole.zip", dir);
   if (   !write_zip_entries(p, e, 2)
       || !set_whole_rec(&arcade, "EM Whole", p))
      goto fixture;
   set_rec(&member[2], "EM Whole Member", crc[7], MF_MEMBER_LEN);

   /* The first member stores no CRC. */
   e[0].name = "zero0.rd";
   e[0].data = d[0];
   e[0].crc  = 0;
   e[1].name = "zero2.rd";
   e[1].data = d[8];
   e[1].crc  = crc[8];
   sprintf(p, "%s/em_zero.zip", dir);
   if (!write_zip_entries(p, e, 2))
      goto fixture;
   set_rec(&member[3], "EM Zero", crc[8], MF_MEMBER_LEN);

   sprintf(p, "%s/Test EM Cart.rdb", db);
   if (!write_db_set(p, cart, 7))
      goto fixture;
   sprintf(p, "%s/Test EM Member.rdb", db);
   if (!write_db_set(p, member, 5))
      goto fixture;
   sprintf(p, "%s/Test EM Arcade.rdb", db);
   if (!write_db_set(p, &arcade, 1))
      goto fixture;

   sprintf(cart_lpl,   "%s/Test EM Cart.lpl",   pl_dir);
   sprintf(member_lpl, "%s/Test EM Member.lpl", pl_dir);
   sprintf(arcade_lpl, "%s/Test EM Arcade.lpl", pl_dir);
   filestream_delete(cart_lpl);
   filestream_delete(member_lpl);
   filestream_delete(arcade_lpl);
   use_queue(threaded);
   use_db(db);
   retro_atomic_store_release_int(&reads_begun, 0);
   retro_atomic_store_release_int(&watch_scan, 0);
   retro_atomic_store_release_int(&watch_reader, 0);
   begun_size = set_bytes;
   watch_size = held_bytes;
   started    = run_scan(dir) && scan_completed;
   begun_size = 0;
   watch_size = 0;
   set_reads  = retro_atomic_load_acquire_int(&reads_begun);
   held_reads = retro_atomic_load_acquire_int(&watch_scan)
      + retro_atomic_load_acquire_int(&watch_reader);

   sprintf(what, "a later member's archive-named record names it %s", q);
   check(   started && file_contains(cart_lpl, "EM Set")
         && file_contains(cart_lpl, "em_set.zip")
         && !file_contains(cart_lpl, "em_set.zip#"), what,
         "em_set.zip");
   sprintf(what, "a later member's record, no archive name: held %s", q);
   check(   started && file_contains(cart_lpl, "EM Late")
         && file_contains(cart_lpl, "em_late.zip#b.rd"), what,
         "em_late.zip#b.rd");
   sprintf(what, "a disc track's record, though zip opens: held %s", q);
   check(   started && file_contains(cart_lpl, "EM Disc")
         && file_contains(cart_lpl, "em_game.zip#Game (Track 3).bin"),
         what, "em_game.zip#Game (Track 3).bin");
   sprintf(what, "set.zip's record, settle.zip: held %s", q);
   check(   started && file_contains(cart_lpl, "EM Settle")
         && file_contains(cart_lpl, "settle.zip#st.rd"), what,
         "settle.zip#st.rd");
   sprintf(what, "an archive-named record, no core opens zip: held %s", q);
   check(   started && file_contains(member_lpl, "EM NoZip")
         && file_contains(member_lpl, "em_nozip.zip#nz1.rd"), what,
         "em_nozip.zip#nz1.rd");
   sprintf(what, "a set inside an archive: held %s", q);
   check(   started && file_contains(cart_lpl, "EM Nested")
         && file_contains(cart_lpl, "em_outer.zip#set.zip"), what,
         "em_outer.zip#set.zip");
   sprintf(what, "held hits are archive#member entries %s", q);
   check(   started && file_contains(member_lpl, "em_held.zip#x1.rd")
         && file_contains(member_lpl, "em_held.zip#x2.rd"), what,
         "em_held.zip#x1.rd, #x2.rd");
   sprintf(what, "two records: held, not the archive %s", q);
   check(   started && file_contains(cart_lpl, "em_two.zip#two1.rd")
         && file_contains(cart_lpl, "em_two.zip#two2.rd"), what,
         "em_two.zip#two1.rd, #two2.rd");
   sprintf(what, "its own CRC beats held hits %s", q);
   check(   started && file_contains(arcade_lpl, "EM Whole")
         && !file_contains(member_lpl, "EM Whole Member"), what,
         "em_whole.zip");
   sprintf(what, "a member with no stored CRC is passed %s", q);
   check(   started && file_contains(member_lpl, "em_zero.zip#zero2.rd")
         && !file_contains(member_lpl, "zero0.rd"), what,
         "em_zero.zip#zero2.rd");
   if (!threaded)
   {
      sprintf(detail, "%d whole reads", set_reads);
      check(started && set_reads == 0,
            "a later member's hit reads no archive whole", detail);
      sprintf(detail, "%d read steps", held_reads);
      check(started && held_reads == 0,
            "held hits: no zip record of its size, not read whole",
            detail);
   }
   return;

fixture:
   check(0, "fixture", "could not write the every-member set");
}

/* A loose scan expands an archive whose first member misses: each
 * member is an archive#member entry, a matched one in its database's
 * playlist and the rest under the folder's name.  A record named for
 * the archive does not make it one entry. */
static void loose_member_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static uint8_t d[4][MF_MEMBER_LEN];
   static const uint32_t crc[4] = {
      0x0BF90001u, 0x0BF90002u, 0x0BF90011u, 0x0BF90012u };
   static char    db[512], dir[512], pl[512], p[1024];
   static char    cart_lpl[1024], member_lpl[1024], loose_lpl[1024];
   char           what[64];
   db_rec_t       cart, member;
   zip_entry_t    e[2];
   bool           started;
   unsigned       j;
   const char    *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,  "%s/loose_member_db", root);
   sprintf(dir, "%s/loose_member",    root);
   sprintf(pl,  "%s/loose_member_pl", root);
   path_mkdir(db);
   path_mkdir(dir);
   path_mkdir(pl);
   for (j = 0; j < 4; j++)
      fill_forced(d[j], MF_MEMBER_LEN, 131 + 2 * j, crc[j]);
   memset(e, 0, sizeof(e));
   for (j = 0; j < 2; j++)
      e[j].len = MF_MEMBER_LEN;

   e[0].name = "a.rd";
   e[0].data = d[0];
   e[0].crc  = crc[0];
   e[1].name = "b.rd";
   e[1].data = d[1];
   e[1].crc  = crc[1];
   sprintf(p, "%s/lz_set.zip", dir);
   if (!write_zip_entries(p, e, 2))
      goto fixture;
   set_rec(&cart, "LZ Set", crc[1], MF_MEMBER_LEN);
   cart.rom_name = "lz_set.zip";

   e[0].name = "x.rd";
   e[0].data = d[2];
   e[0].crc  = crc[2];
   e[1].name = "y.rd";
   e[1].data = d[3];
   e[1].crc  = crc[3];
   sprintf(p, "%s/lz_held.zip", dir);
   if (!write_zip_entries(p, e, 2))
      goto fixture;
   set_rec(&member, "LZ Held", crc[3], MF_MEMBER_LEN);

   sprintf(p, "%s/Test EM Cart.rdb", db);
   if (!write_db_set(p, &cart, 1))
      goto fixture;
   sprintf(p, "%s/Test EM Member.rdb", db);
   if (!write_db_set(p, &member, 1))
      goto fixture;

   sprintf(cart_lpl,   "%s/Test EM Cart.lpl",   pl);
   sprintf(member_lpl, "%s/Test EM Member.lpl", pl);
   sprintf(loose_lpl,  "%s/loose_member.lpl",   pl);
   filestream_delete(cart_lpl);
   filestream_delete(member_lpl);
   filestream_delete(loose_lpl);
   use_queue(threaded);
   use_db(db);
   strlcpy(config_get_ptr()->paths.directory_playlist, pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   /* Automatic would make it strict. */
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_CUSTOM);
   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_LOOSE);
   started = run_scan(dir) && scan_completed;
   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_STRICT);
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC);
   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));

   sprintf(what, "loose: matched members are archive#member %s", q);
   check(   started && file_contains(cart_lpl, "lz_set.zip#b.rd")
         && file_contains(member_lpl, "lz_held.zip#y.rd"), what,
         "lz_set.zip#b.rd, lz_held.zip#y.rd");
   sprintf(what, "loose: the others are loose entries %s", q);
   check(   started && file_contains(loose_lpl, "lz_set.zip#a.rd")
         && file_contains(loose_lpl, "lz_held.zip#x.rd"), what,
         "lz_set.zip#a.rd, lz_held.zip#x.rd");
   sprintf(what, "loose: no archive is an entry %s", q);
   check(   started && !file_contains(cart_lpl, "lz_set.zip\"")
         && !file_contains(loose_lpl, "lz_set.zip\"")
         && !file_contains(loose_lpl, "lz_held.zip\""), what,
         "lz_set.zip, lz_held.zip");
   return;

fixture:
   check(0, "fixture", "could not write the loose member set");
}

#ifndef SCAN_MEMBER_HITS
#define SCAN_MEMBER_HITS 64
#endif
#define ES_HITS 100

/* As run_scan_counted(), but cancels the scan after the check in which
 * @c reaches @at. */
static void run_scan_cancel_at(const char *dir, const step_count_t *c,
      unsigned at)
{
   task_finder_data_t finder;
   bool               cancelled = false;
   time_t             started;

   loop_active    = true;
   scan_completed = false;
   if (!task_push_dbscan(dir, scan_cb))
      return;
   finder.func     = find_scan;
   finder.userdata = NULL;
   started         = time(NULL);
   while (loop_active)
   {
      spend_window();
      check_seq++;
      task_queue_check();
      if (!cancelled && c->total >= at && task_queue_find(&finder))
      {
         task_queue_cancel_ident(lane_ident);
         cancelled = true;
      }
      if (difftime(time(NULL), started) > SCAN_TIMEOUT_SECONDS)
         break;
   }
}

/* An archive with more member hits than a step takes: on the regular
 * queue with a spent window each check is one step, and none looks up
 * or writes more than SCAN_MEMBER_HITS of them, yet each is an entry.
 * A hit whose archive#member path would not fit is none, unless its
 * entry is the archive alone: a disc set's, a member database's.  A
 * cancel between those steps writes nothing. */
static void every_step_lane(const char *root, const char *pl_dir)
{
   static uint8_t     d[ES_HITS + 5][16];
   static char        names[ES_HITS + 1][16];
   static char        labels[ES_HITS + 1][16];
   static char        long_name[4][PATH_MAX_LENGTH];
   static const char *long_labels[4] = {
      "ES Long", "ES Fits", "ES Disk", "ES Arcade" };
   static char        db[512], dir[512], p[1024], lpl[1024];
   static char        disc_lpl[1024], arc_lpl[1024];
   static zip_entry_t e[ES_HITS + 5];
   static db_rec_t    recs[ES_HITS + 2], solo[2];
   char               what[64], detail[64];
   bool               started;
   size_t             len;
   unsigned           j, found = 0;

   sprintf(db,  "%s/every_step_db", root);
   sprintf(dir, "%s/every_step",    root);
   path_mkdir(db);
   path_mkdir(dir);
   sprintf(p, "%s/es.zip", dir);

   /* m000 has no record: the first member misses. */
   for (j = 0; j < ES_HITS + 1; j++)
   {
      sprintf(names[j], "m%03u.rd", j);
      fill_forced(d[j], sizeof(d[j]), 151 + j, 0x0BFA0000u + j);
      e[j].name = names[j];
      e[j].data = d[j];
      e[j].len  = sizeof(d[j]);
      e[j].crc  = 0x0BFA0000u + j;
      if (j)
      {
         sprintf(labels[j], "ES Hit %03u", j);
         set_rec(&recs[j - 1], labels[j], e[j].crc, sizeof(d[j]));
      }
   }
   /* With the archive's path and '#', PATH_MAX_LENGTH: one byte over.
    * "ES Fits" is one shorter. */
   len = PATH_MAX_LENGTH - strlen(p);
   for (j = 0; j < 4; j++)
   {
      size_t   n   = len - (j == 1 ? 2 : 1);
      uint32_t crc = 0x0BFAFFFFu - j;

      memset(long_name[j], 'l', n - 3);
      if (j == 2)
         memcpy(long_name[j], "Set (Disk 1) ", 13);
      strcpy(long_name[j] + n - 3, ".rd");
      fill_forced(d[ES_HITS + 1 + j], sizeof(d[0]), 149 - j, crc);
      e[ES_HITS + 1 + j].name = long_name[j];
      e[ES_HITS + 1 + j].data = d[ES_HITS + 1 + j];
      e[ES_HITS + 1 + j].len  = sizeof(d[0]);
      e[ES_HITS + 1 + j].crc  = crc;
      set_rec(j < 2 ? &recs[ES_HITS + j] : &solo[j - 2], long_labels[j],
            crc, sizeof(d[0]));
   }
   if (!write_zip_entries(p, e, ES_HITS + 5))
      goto fixture;
   sprintf(p, "%s/Test EM Member.rdb", db);
   if (!write_db_set(p, recs, ES_HITS + 2))
      goto fixture;
   sprintf(p, "%s/Test ES Disc.rdb", db);
   if (!write_db_set(p, &solo[0], 1))
      goto fixture;
   sprintf(p, "%s/Test ES Arcade.rdb", db);
   if (!write_db_set(p, &solo[1], 1))
      goto fixture;

   sprintf(lpl,      "%s/Test EM Member.lpl", pl_dir);
   sprintf(disc_lpl, "%s/Test ES Disc.lpl",   pl_dir);
   sprintf(arc_lpl,  "%s/Test ES Arcade.lpl", pl_dir);
   filestream_delete(lpl);
   filestream_delete(disc_lpl);
   filestream_delete(arc_lpl);
   use_queue(false);
   use_db(db);
   memset(&step_reads, 0, sizeof(step_reads));
   memset(&step_joins, 0, sizeof(step_joins));
   step_watch = "every_step";
   started    = run_scan_counted(dir) && scan_completed;
   step_watch = NULL;

   for (j = 1; j < ES_HITS + 1; j++)
   {
      sprintf(what, "es.zip#%s", names[j]);
      if (file_contains(lpl, what))
         found++;
   }
   sprintf(detail, "%u of %u", found, ES_HITS);
   check(started && found == ES_HITS, "member hits over steps: all entries",
         detail);
   sprintf(detail, "%u lookups, at most %u a step", step_reads.total,
         step_reads.most);
   check(   started && step_reads.total == ES_HITS + 4
         && step_reads.most <= SCAN_MEMBER_HITS,
         "member hits: looked up over steps", detail);
   sprintf(detail, "%u entries, at most %u a step", step_joins.total,
         step_joins.most);
   check(   started && step_joins.total == ES_HITS + 1
         && step_joins.most <= SCAN_MEMBER_HITS,
         "member hits: written over steps", detail);
   check(started && !file_contains(lpl, "ES Long"),
         "a hit too long for archive#member: none", "ES Long");
   check(started && file_contains(lpl, "ES Fits"),
         "a hit that just fits archive#member: an entry", "ES Fits");
   check(   started && file_contains(disc_lpl, "ES Disk")
         && !file_contains(disc_lpl, "es.zip#"),
         "a disc set's hit too long for archive#member: the archive",
         "ES Disk");
   check(   started && file_contains(arc_lpl, "ES Arcade")
         && !file_contains(arc_lpl, "es.zip#"),
         "a member database's hit too long for it: the archive",
         "ES Arcade");

   for (j = 0; j < 2; j++)
   {
      filestream_delete(lpl);
      filestream_delete(disc_lpl);
      filestream_delete(arc_lpl);
      memset(&step_reads, 0, sizeof(step_reads));
      memset(&step_joins, 0, sizeof(step_joins));
      step_watch = "every_step";
      run_scan_cancel_at(dir, j ? &step_joins : &step_reads, 1);
      step_watch = NULL;
      sprintf(detail, "%u lookups, %u entries", step_reads.total,
            step_joins.total);
      check(   scan_completed
            && (j ? step_reads.total == ES_HITS + 4
                  : step_reads.total > 0 && step_reads.total < ES_HITS + 4)
            && step_joins.total == (j ? SCAN_MEMBER_HITS : 0)
            && !file_contains(lpl, "ES Hit")
            && !file_contains(arc_lpl, "ES Arcade"),
            j ? "member hits: a cancel between entry steps writes none"
              : "member hits: a cancel between lookup steps writes none",
            detail);
   }
   return;

fixture:
   check(0, "fixture", "could not write the every-step set");
}

/* Archives a database whose cores open zips holds by their size and
 * own CRC: one whose later member is a record named for it in another
 * such database, and a TorrentZip set whose members only a core that
 * does not open zips claims.  Each is its whole record's entry, read
 * whole once, and no database claimed only for its members' type is
 * opened. */
static void whole_first_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static uint8_t d[4][MF_MEMBER_LEN];
   static const uint32_t crc[4] = {
      0x0BFC0001u, 0x0BFC0002u, 0x0BFC0011u, 0x0BFC0012u };
   static char    cart_lpl[1024], member_lpl[1024], arcade_lpl[1024];
   char           db[512], bare[512], tz[512], p[1024];
   char           what[96], detail[64], whole_crc[16];
   db_rec_t       cart, member[2], arcade[2];
   zip_entry_t    e[2];
   int            opens, reads;
   bool           started;
   unsigned       j;
   const char    *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,   "%s/whole_first_db",   root);
   sprintf(bare, "%s/whole_first_bare", root);
   sprintf(tz,   "%s/whole_first_tz",   root);
   path_mkdir(db);
   path_mkdir(bare);
   path_mkdir(tz);
   for (j = 0; j < 4; j++)
      fill_forced(d[j], MF_MEMBER_LEN, 121 + 2 * j, crc[j]);
   memset(e, 0, sizeof(e));
   e[0].len = MF_MEMBER_LEN;
   e[1].len = MF_MEMBER_LEN;

   /* Its first member misses; its second is a record named for it. */
   e[0].name = "a.rd";
   e[0].data = d[0];
   e[0].crc  = crc[0];
   e[1].name = "b.rd";
   e[1].data = d[1];
   e[1].crc  = crc[1];
   sprintf(p, "%s/wf_bare.zip", bare);
   if (   !write_zip_entries(p, e, 2)
       || !set_whole_rec(&arcade[0], "WF Whole", p))
      goto fixture;
   set_rec(&cart, "WF Bare", crc[1], MF_MEMBER_LEN);
   cart.rom_name = "wf_bare.zip";
   sprintf(whole_crc, "%08X|crc", (unsigned)arcade[0].crc);

   /* Its members are records of a core that does not open zips. */
   e[0].name = "c.rd";
   e[0].data = d[2];
   e[0].crc  = crc[2];
   e[1].name = "d.rd";
   e[1].data = d[3];
   e[1].crc  = crc[3];
   sprintf(p, "%s/wf_tz.zip", tz);
   zip_comment = TZ_COMMENT;
   if (!write_zip_entries(p, e, 2))
   {
      zip_comment = NULL;
      goto fixture;
   }
   zip_comment = NULL;
   if (!set_whole_rec(&arcade[1], "WF TZ", p))
      goto fixture;
   set_rec(&member[0], "WF TZ C", crc[2], MF_MEMBER_LEN);
   set_rec(&member[1], "WF TZ D", crc[3], MF_MEMBER_LEN);

   sprintf(p, "%s/Test EM Cart.rdb", db);
   if (!write_db_set(p, &cart, 1))
      goto fixture;
   sprintf(p, "%s/Test EM Member.rdb", db);
   if (!write_db_set(p, member, 2))
      goto fixture;
   sprintf(p, "%s/Test EM Arcade.rdb", db);
   if (!write_db_set(p, arcade, 2))
      goto fixture;

   sprintf(cart_lpl,   "%s/Test EM Cart.lpl",   pl_dir);
   sprintf(member_lpl, "%s/Test EM Member.lpl", pl_dir);
   sprintf(arcade_lpl, "%s/Test EM Arcade.lpl", pl_dir);
   use_queue(threaded);
   use_db(db);
   open_watch = "Test EM Member.rdb";
   for (j = 0; j < 2; j++)
   {
      filestream_delete(cart_lpl);
      filestream_delete(member_lpl);
      filestream_delete(arcade_lpl);
      retro_atomic_store_release_int(&watch_opens, 0);
      retro_atomic_store_release_int(&reads_begun, 0);
      begun_size = arcade[1].size;
      started    = run_scan(j ? tz : bare) && scan_completed;
      begun_size = 0;
      opens      = retro_atomic_load_acquire_int(&watch_opens);
      reads      = retro_atomic_load_acquire_int(&reads_begun);
      if (!j)
      {
         sprintf(what, "its own CRC beats a later member's named "
               "record %s", q);
         check(   started && file_contains(arcade_lpl, "WF Whole")
               && file_contains(arcade_lpl, whole_crc)
               && !file_contains(arcade_lpl, "wf_bare.zip#")
               && !file_contains(cart_lpl, "WF Bare"), what,
               "wf_bare.zip");
      }
      else
      {
         sprintf(what, "a TorrentZip set of several members: its set "
               "record %s", q);
         check(   started && file_contains(arcade_lpl, "WF TZ")
               && file_contains(arcade_lpl, "wf_tz.zip\"")
               && !file_contains(member_lpl, "wf_tz.zip#"), what,
               "wf_tz.zip");
         sprintf(what, "a TorrentZip set of several members: read "
               "whole once %s", q);
         sprintf(detail, "%d whole reads", reads);
         check(started && reads == 1, what, detail);
      }
      sprintf(what, "a set of several members opens no member "
            "database %s", q);
      sprintf(detail, "%d opens of the members' database", opens);
      check(started && opens == 0, what, detail);
   }
   open_watch = NULL;
   return;

fixture:
   check(0, "fixture", "could not write the whole-first set");
}

/* Scans @dir with @db's two databases in name order, then in reverse:
 * @want with @crc in @pl[0]'s playlist, nothing of @file in @pl[1]'s,
 * and @file read whole @whole times. */
static void two_db_scan(const char *pl_dir, const char *dir,
      const char *db, const char *const pl[2], const char *file,
      const char *want, const char *crc, const char *lane, int whole,
      bool threaded)
{
   char        lpl[2][1024], what[128], detail[64];
   unsigned    k;
   int         reads;
   bool        started;
   const char *q = threaded ? "(threaded)" : "(regular)";

   for (k = 0; k < 2; k++)
      sprintf(lpl[k], "%s/%s.lpl", pl_dir, pl[k]);
   use_queue(threaded);
   use_db(db);
   for (k = 0; k < 2; k++)
   {
      filestream_delete(lpl[0]);
      filestream_delete(lpl[1]);
      db_order   = k ? -1 : 1;
      retro_atomic_store_release_int(&reads_begun, 0);
      begun_size = GATE_ZIP_LEN;
      started    = run_scan(dir) && scan_completed;
      begun_size = 0;
      db_order   = 0;
      reads      = retro_atomic_load_acquire_int(&reads_begun);

      sprintf(what, "%s, %s order %s", lane, k ? "reverse" : "name", q);
      check(   started && file_contains(lpl[0], want)
            && file_contains(lpl[0], crc) && !file_contains(lpl[1], file),
            what, want);
      sprintf(what, "%s, %s order: %s %s", lane, k ? "reverse" : "name",
            whole ? "read whole once" : "not read whole", q);
      sprintf(detail, "%d whole reads", reads);
      check(started && reads == whole, what, detail);
   }
}

/* A cartridge zip's first member is in a database claimed only for
 * it and in one a core listing zip claims: x.zip in the latter, as
 * when not looked up by the member, whichever comes first. */
static void sibling_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static const char *const pl[2] = { "Test Cart Zip", "Test Cart Gate" };
   char     db[512], dir[512], p[1024], crc[16];
   db_rec_t rec;
   unsigned k;

   sprintf(db,  "%s/sibling_db", root);
   sprintf(dir, "%s/sibling",    root);
   path_mkdir(db);
   path_mkdir(dir);
   if (!write_gate_zip(dir, "sb0.zip", "rom.ct", "Gate SB0",
         0x0CA70030u, &rec))
      goto fixture;
   for (k = 0; k < 2; k++)
   {
      sprintf(p, "%s/%s.rdb", db, pl[k]);
      if (!write_db_set(p, &rec, 1))
         goto fixture;
   }
   sprintf(crc, "%08X|crc", (unsigned)rec.crc);
   two_db_scan(pl_dir, dir, db, pl, "sb0.zip", "sb0.zip\"", crc,
         "sibling: x.zip where zip is claimed", 0, threaded);
   return;

fixture:
   check(0, "fixture", "could not write the sibling set");
}

/* The first member in a database claimed only for it, the archive's
 * own CRC in one a core listing zip claims: held as archive#member
 * either way. */
static void dual_match_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static const char *const pl[2] = { "Test Cart Gate", "Test Arc Gate" };
   char     db[512], dir[512], p[1024], crc[16];
   db_rec_t rec[2];

   sprintf(db,  "%s/dual_match_db", root);
   sprintf(dir, "%s/dual_match",    root);
   path_mkdir(db);
   path_mkdir(dir);
   sprintf(p, "%s/dm0.zip", dir);
   if (   !write_gate_zip(dir, "dm0.zip", "rom.ct", "Gate DM0 Member",
            0x0CA70040u, &rec[0])
       || !set_whole_rec(&rec[1], "Gate DM0", p))
      goto fixture;
   sprintf(p, "%s/%s.rdb", db, pl[0]);
   if (!write_db_set(p, &rec[0], 1))
      goto fixture;
   sprintf(p, "%s/%s.rdb", db, pl[1]);
   if (!write_db_set(p, &rec[1], 1))
      goto fixture;
   sprintf(crc, "%08X|crc", (unsigned)rec[0].crc);
   two_db_scan(pl_dir, dir, db, pl, "dm0.zip", "dm0.zip#rom.ct", crc,
         "dual match: x.zip#member", 0, threaded);
   return;

fixture:
   check(0, "fixture", "could not write the dual match set");
}

/* With "Scan Inside Archives" off nothing expands, so the member pass
 * looks only where zip is claimed: the dual match is x.zip there, and
 * a cartridge zip whose core lists only its member is added nowhere
 * and, with no core that opens zip, not read whole. */
static void archives_off_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static const char *const pl[2] = { "Test Arc Gate", "Test Cart Gate" };
   bool       *search_archives = manual_content_scan_get_search_archives_ptr();
   char        db[2][512], dir[2][512], p[1024], lpl[1024];
   char        what[64], detail[64], crc[16];
   db_rec_t    rec[3];
   unsigned    k;
   int         reads;
   bool        started;
   const char *q = threaded ? "(threaded)" : "(regular)";

   for (k = 0; k < 2; k++)
   {
      sprintf(db[k],  "%s/archives_off_db%u", root, k);
      sprintf(dir[k], "%s/archives_off%u",    root, k);
      path_mkdir(db[k]);
      path_mkdir(dir[k]);
   }
   sprintf(p, "%s/ao0.zip", dir[0]);
   if (   !search_archives
       || !write_gate_zip(dir[0], "ao0.zip", "rom.ct", "Gate AO0 Member",
            0x0CA70050u, &rec[0])
       || !set_whole_rec(&rec[1], "Gate AO0", p)
       || !write_gate_zip(dir[1], "ao1.zip", "rom.ct", "Gate AO1",
            0x0CA70051u, &rec[2]))
      goto fixture;
   sprintf(p, "%s/Test Cart Gate.rdb", db[0]);
   if (!write_db_set(p, &rec[0], 1))
      goto fixture;
   sprintf(p, "%s/Test Arc Gate.rdb", db[0]);
   if (!write_db_set(p, &rec[1], 1))
      goto fixture;
   sprintf(p, "%s/Test Cart Gate.rdb", db[1]);
   if (!write_db_set(p, &rec[2], 1))
      goto fixture;

   /* Automatic always searches archives. */
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_CUSTOM);
   *search_archives = false;
   sprintf(crc, "%08X|crc", (unsigned)rec[1].crc);
   two_db_scan(pl_dir, dir[0], db[0], pl, "ao0.zip", "ao0.zip\"", crc,
         "archives off: dual match x.zip", 1, threaded);

   sprintf(lpl, "%s/Test Cart Gate.lpl", pl_dir);
   filestream_delete(lpl);
   use_queue(threaded);
   use_db(db[1]);
   retro_atomic_store_release_int(&reads_begun, 0);
   begun_size = GATE_ZIP_LEN;
   started    = run_scan(dir[1]) && scan_completed;
   begun_size = 0;
   reads      = retro_atomic_load_acquire_int(&reads_begun);
   *search_archives = true;
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC);

   sprintf(what, "archives off: cart zip added nowhere %s", q);
   check(started && !file_contains(lpl, "ao1.zip"), what, "ao1.zip");
   sprintf(what, "archives off: cart zip not read whole %s", q);
   sprintf(detail, "%d whole reads", reads);
   check(started && reads == 0, what, detail);
   return;

fixture:
   check(0, "fixture", "could not write the archives off sets");
}

/* Each run of zips is the window and a few more long. */
#define WA_EXTRA 3

static bool files_equal(const char *a, const char *b)
{
   void   *da = NULL, *db = NULL;
   int64_t la = 0, lb = 0;
   bool    eq;

   if (   !filestream_read_file(a, &da, &la)
       || !filestream_read_file(b, &db, &lb))
   {
      free(da);
      return false;
   }
   eq = la == lb && !memcmp(da, db, (size_t)la);
   free(da);
   free(db);
   return eq;
}

/* Writes wa_<k>.zip for the file at @pos in content-list order, and
 * its record: two cartridge zips (1024-byte member, a 1130-byte zip),
 * @n arcade zips (1100, the last two 1200: 1306 bytes), then @n
 * cartridge zips (1000, the last two 1024). */
static bool write_wa_zip(const char *dir, unsigned k, unsigned pos,
      unsigned n, char *name, db_rec_t *rec)
{
   static uint8_t d[1200];
   char           p[1024];
   zip_entry_t    e;
   unsigned       run  = (pos < 2) ? 0 : (pos < 2 + n) ? 1 : 2;
   unsigned       at   = (run == 0) ? pos : (run == 1) ? pos - 2 : pos - 2 - n;
   bool           tail = run && at >= n - 2;
   uint32_t       len  = (run == 1) ? (tail ? 1200 : 1100)
                       : (run == 2 && !tail) ? 1000 : 1024;
   uint32_t       crc  = 0x0BF30000u + pos;

   fill_forced(d, len, 61 + pos, crc);
   memset(&e, 0, sizeof(e));
   e.name = "m.rd";
   e.data = d;
   e.len  = len;
   e.crc  = crc;
   sprintf(p, "%s/wa_%03u.zip", dir, k);
   sprintf(name, "WA %c%03u", "ABC"[run], at);
   if (!write_zip_entries(p, &e, 1))
      return false;
   if (run == 1)
      return set_whole_rec(rec, name, p);
   set_rec(rec, name, crc, len);
   return true;
}

/* Cartridge zips, arcade zips, then cartridge zips, on the threaded
 * queue.  Readers read only archive directories until an archive
 * matches by its own CRC, then archives whole until one matches
 * otherwise or misses.  A file the window reaches after a switch is
 * claimed after it: the last two arcade zips are read whole by readers
 * alone, the last two cartridge zips by no one, nor are the first
 * ones.  The playlists equal a run without readers. */
static void whole_ahead_lane(const char *root, const char *pl_dir)
{
   static const char *const pl[2] = { "Test MF Cart", "Test MF Arcade" };
   static char  first[32];
   char         db[512], dir[512], p[1024], lpl[2][1024], ref[2][1024];
   char         detail[128];
   char        *names = NULL;
   db_rec_t    *cart = NULL, *arcade = NULL;
   unsigned     n = reader_window() + WA_EXTRA;
   unsigned     i, j, nc, na;
   unsigned    *pos = NULL;
   int          begun, by_scan, by_reader, stored;
   bool         started, same = true;

   if (!readers_expected())
   {
      check(1, "whole_ahead: read whole ahead on and off", "no readers");
      return;
   }
   sprintf(db,  "%s/whole_ahead_db", root);
   sprintf(dir, "%s/whole_ahead",    root);
   path_mkdir(db);
   path_mkdir(dir);
   wa_count = 2 + 2 * n;
   names    = (char*)calloc(wa_count, 16);
   cart     = (db_rec_t*)calloc(wa_count, sizeof(*cart));
   arcade   = (db_rec_t*)calloc(n, sizeof(*arcade));
   pos      = (unsigned*)calloc(wa_count, sizeof(*pos));
   wa_last  = (unsigned*)calloc(wa_count, sizeof(*wa_last));
   if (!names || !cart || !arcade || !pos || !wa_last)
      goto fixture;
   for (j = 0; j < 2; j++)
   {
      sprintf(lpl[j], "%s/%s.lpl", pl_dir, pl[j]);
      sprintf(ref[j], "%s/%s ref.lpl", pl_dir, pl[j]);
   }
   use_db(db);
   use_queue(false);

   /* Twice: in name order, to learn the list's, then in that. */
   for (j = 0; j < 2; j++)
   {
      nc = na = 0;
      for (i = 0; i < wa_count; i++)
      {
         db_rec_t *rec = (pos[i] - 2 < n) ? &arcade[na++] : &cart[nc++];
         if (!write_wa_zip(dir, i, pos[i], n, names + i * 16, rec))
            goto fixture;
         if (pos[i] == n - 1)
            look_crc[0] = rec->crc;
      }
      sprintf(p, "%s/%s.rdb", db, pl[0]);
      if (!write_db_set(p, cart, nc))
         goto fixture;
      sprintf(p, "%s/%s.rdb", db, pl[1]);
      if (!write_db_set(p, arcade, na))
         goto fixture;
      filestream_delete(lpl[0]);
      filestream_delete(lpl[1]);
      if (j)
         break;
      if (!run_scan(dir) || !scan_completed)
         goto fixture;
      for (i = 0; i < wa_count; i++)
      {
         unsigned k;
         if (!wa_last[i])
            goto fixture;
         for (k = 0; k < wa_count; k++)
            if (wa_last[k] < wa_last[i])
               pos[i]++;
         if (!pos[i])
            sprintf(first, "wa_%03u.zip", i);
      }
   }
   free(wa_last);
   wa_last = NULL;

   filestream_delete(ref[0]);
   filestream_delete(ref[1]);
   if (!run_scan(dir) || !scan_completed)
      goto fixture;
   for (j = 0; j < 2; j++)
      if (filestream_rename(lpl[j], ref[j]))
         goto fixture;

   use_queue(true);
   retro_atomic_store_release_int(&reads_begun, 0);
   retro_atomic_store_release_int(&watch_scan, 0);
   retro_atomic_store_release_int(&watch_reader, 0);
   retro_atomic_store_release_int(&stored_reads, 0);
   begun_size   = 30 + 4 + 1024 + 46 + 4 + 22;
   watch_size   = 30 + 4 + 1200 + 46 + 4 + 22;
   stored_name  = first;
   /* The arcade zip before the last two, looked up by its own CRC,
    * waits for readers to begin them. */
   look_want[0] = 2;
   look_n       = 1;
   look_watch   = true;
   started      = run_scan(dir) && scan_completed;
   look_n       = 0;
   look_watch   = false;
   begun_size   = 0;
   watch_size   = 0;
   stored_name  = NULL;
   begun        = retro_atomic_load_acquire_int(&reads_begun);
   by_scan      = retro_atomic_load_acquire_int(&watch_scan);
   by_reader    = retro_atomic_load_acquire_int(&watch_reader);
   stored       = retro_atomic_load_acquire_int(&stored_reads);
   for (j = 0; j < 2; j++)
      same = same && files_equal(lpl[j], ref[j]);

   sprintf(detail, "%d whole reads", begun);
   check(started && begun == 0,
         "whole_ahead: cartridge zips read no archive whole", detail);
   sprintf(detail, "scan %d, readers %d", by_scan, by_reader);
   passes_detail(detail);
   check(   started && by_scan == 0 && by_reader > 0
         && retro_atomic_load_acquire_int(&look_hits)
         && !retro_atomic_load_acquire_int(&look_late),
         "whole_ahead: readers read the arcade zips whole", detail);
   sprintf(detail, "%d directory reads", stored);
   check(started && stored == 1,
         "whole_ahead: a reader's directory read is the scan's", detail);
   sprintf(p, "WA C%03u", n - 1);
   check(   started && same && file_contains(lpl[0], "WA A000")
         && file_contains(lpl[0], p) && file_contains(lpl[1], "WA B000"),
         "whole_ahead: the playlists of a run without readers",
         same ? "equal" : "differ");
   free(names);
   free(cart);
   free(arcade);
   free(pos);
   return;

fixture:
   free(wa_last);
   wa_last = NULL;
   free(names);
   free(cart);
   free(arcade);
   free(pos);
   check(0, "fixture", "could not write or scan the whole_ahead set");
}

/* Writes wa_<k>.zip for the file at @pos in content-list order, and
 * its record: @n arcade zips by their own CRC (a 900-byte member, the
 * last two 1000: 1106 bytes), then @n cartridge zips by their member,
 * rom.ct (900, the last two 1024: 1134 bytes). */
static bool write_hc_zip(const char *dir, unsigned k, unsigned pos,
      unsigned n, db_rec_t *rec)
{
   static uint8_t d[MF_MEMBER_LEN];
   char           p[1024];
   bool           arc  = pos < n;
   bool           tail = (arc ? pos : pos - n) >= n - 2;
   uint32_t       len  = !tail ? 900 : arc ? 1000 : MF_MEMBER_LEN;
   uint32_t       crc  = 0x0CA71000u + pos;

   fill_forced(d, len, 71 + pos, crc);
   sprintf(p, "%s/wa_%03u.zip", dir, k);
   if (!write_zip(p, arc ? "a.ar" : "rom.ct", d, len, crc))
      return false;
   if (arc)
      return set_whole_rec(rec, "HC Arcade", p);
   set_rec(rec, "HC Cart", crc, len);
   return true;
}

/* Arcade zips by their own CRC, then cartridge zips by their first
 * member in a database claimed only for it, on the threaded queue.
 * The first arcade zip's whole match turns read whole ahead on, the
 * first cartridge zip's member hit turns it off.  The last two of
 * each run are past the window from the switch: the arcade ones are
 * read whole by readers, the cartridge ones by no one. */
static void hit_clears_lane(const char *root, const char *pl_dir)
{
   char      db[512], dir[512], p[1024], lpl[1024], detail[128];
   db_rec_t *arc  = NULL, *cart = NULL;
   unsigned *pos  = NULL;
   unsigned  n    = reader_window() + WA_EXTRA;
   unsigned  i, j, k, na, nc;
   int       begun, by_reader;
   bool      started;

   if (!readers_expected())
   {
      check(1, "member hit: read whole ahead off", "no readers");
      return;
   }
   sprintf(db,  "%s/whole_ahead_hit_db", root);
   sprintf(dir, "%s/whole_ahead_hit",    root);
   path_mkdir(db);
   path_mkdir(dir);
   wa_count = 2 * n;
   arc      = (db_rec_t*)calloc(wa_count, sizeof(*arc));
   cart     = (db_rec_t*)calloc(wa_count, sizeof(*cart));
   pos      = (unsigned*)calloc(wa_count, sizeof(*pos));
   wa_last  = (unsigned*)calloc(wa_count, sizeof(*wa_last));
   if (!arc || !cart || !pos || !wa_last)
      goto fixture;
   sprintf(lpl, "%s/Test Cart Gate.lpl", pl_dir);
   use_db(db);
   use_queue(false);

   /* Twice: in name order, to learn the list's, then in that. */
   for (j = 0; j < 2; j++)
   {
      na = nc = 0;
      for (i = 0; i < wa_count; i++)
      {
         if (!write_hc_zip(dir, i, pos[i], n,
               (pos[i] < n) ? &arc[na++] : &cart[nc++]))
            goto fixture;
         if (pos[i] == n - 3)
            look_crc[0] = arc[na - 1].crc;
      }
      sprintf(p, "%s/Test Arc Gate.rdb", db);
      if (!write_db_set(p, arc, na))
         goto fixture;
      sprintf(p, "%s/Test Cart Gate.rdb", db);
      if (!write_db_set(p, cart, nc))
         goto fixture;
      filestream_delete(lpl);
      sprintf(p, "%s/Test Arc Gate.lpl", pl_dir);
      filestream_delete(p);
      if (j)
         break;
      if (!run_scan(dir) || !scan_completed)
         goto fixture;
      for (i = 0; i < wa_count; i++)
      {
         if (!wa_last[i])
            goto fixture;
         for (k = 0; k < wa_count; k++)
            if (wa_last[k] < wa_last[i])
               pos[i]++;
      }
   }
   free(wa_last);
   wa_last = NULL;

   use_queue(true);
   retro_atomic_store_release_int(&reads_begun, 0);
   retro_atomic_store_release_int(&watch_scan, 0);
   retro_atomic_store_release_int(&watch_reader, 0);
   begun_size   = GATE_ZIP_LEN;
   watch_size   = 1106;
   /* The arcade zip before the last two, looked up by its own CRC,
    * waits for readers to begin them. */
   look_want[0] = 2;
   look_n       = 1;
   look_watch   = true;
   started      = run_scan(dir) && scan_completed;
   look_n       = 0;
   look_watch   = false;
   begun_size   = 0;
   watch_size   = 0;
   begun        = retro_atomic_load_acquire_int(&reads_begun);
   by_reader    = retro_atomic_load_acquire_int(&watch_reader);

   sprintf(detail, "%d reader reads", by_reader);
   passes_detail(detail);
   check(   started && by_reader > 0
         && retro_atomic_load_acquire_int(&look_hits)
         && !retro_atomic_load_acquire_int(&look_late),
         "member hit: readers read the last arcade zips whole", detail);
   sprintf(detail, "%d whole reads", begun);
   check(started && begun == 0,
         "member hit: the last cartridge zips read whole by no one", detail);
   sprintf(detail, "%u cartridge zips", nc);
   check(   started && file_contains(lpl, ".zip#rom.ct")
         && file_contains(lpl, "HC Cart"),
         "member hit: cartridge zips matched as x.zip#member", detail);
   free(arc);
   free(cart);
   free(pos);
   return;

fixture:
   free(wa_last);
   wa_last = NULL;
   free(arc);
   free(cart);
   free(pos);
   check(0, "fixture", "could not write or scan the member hit set");
}

#define RA_FILES 9
#define RA_JUNK  66000
#define RA_SLOW  1306

/* Writes wa_<k>.zip for the file at @pos in content-list order, and
 * its record, by its own CRC: the first a 1206-byte zip, the second
 * RA_JUNK bytes with no directory, which no record matches, the third
 * RA_SLOW bytes with no directory, the rest 1130. */
static bool write_ra_zip(const char *dir, unsigned k, unsigned pos,
      db_rec_t *rec)
{
   static uint8_t d[RA_JUNK];
   static char    names[RA_FILES][16];
   char           p[1024];
   zip_entry_t    e;
   uint32_t       len = (pos == 0) ? 1100 : 1024;

   sprintf(p, "%s/wa_%03u.zip", dir, k);
   sprintf(names[k], "RA %03u", pos);
   if (pos == 1)
   {
      fill_forced(d, RA_JUNK, 71 + pos, 0x0BF40000u + pos);
      set_rec(rec, names[k], 0x0BF4FFFFu, RA_JUNK);
      return filestream_write_file(p, d, RA_JUNK);
   }
   if (pos == 2)
   {
      fill_forced(d, RA_SLOW, 71 + pos, 0x0BF40000u + pos);
      return    filestream_write_file(p, d, RA_SLOW)
             && set_whole_rec(rec, names[k], p);
   }
   fill_forced(d, len, 71 + pos, 0x0BF40000u + pos);
   memset(&e, 0, sizeof(e));
   e.name = "m.rd";
   e.data = d;
   e.len  = len;
   e.crc  = 0x0BF40000u + pos;
   return write_zip_entries(p, &e, 1) && set_whole_rec(rec, names[k], p);
}

/* An arcade folder smaller than the window, on the threaded queue.
 * Readers read every directory before the first archive matches by
 * its own CRC; then they take back as many archives as the matches
 * grant them and read them whole.  The two with no directory they
 * read whole at once: the second is over the cutoff, so the scan
 * reads it itself; the third, read slowly, the scan does not read
 * again, on whichever worker.  Test only: the first's directory read
 * waits for the others', and the scan's read of the second for the
 * slow read and one the first match grants to begin.  Which archive
 * that is is the readers' to pick.  The playlist equals a run without
 * readers. */
static void rearm_lane(const char *root, const char *pl_dir)
{
   static db_rec_t rec[RA_FILES];
   static unsigned last[RA_FILES];
   static char     first[32];
   char            db[512], dir[512], p[1024], lpl[1024], ref[1024];
   char            detail[64];
   unsigned        pos[RA_FILES];
   unsigned        i, j, k;
   int             begun, slow, by_scan, by_reader, late;
   bool            started, same;

   if (!readers_expected())
   {
      check(1, "rearm: read whole after the first match", "no readers");
      return;
   }
   sprintf(db,  "%s/whole_ahead_rearm_db", root);
   sprintf(dir, "%s/whole_ahead_rearm",    root);
   sprintf(lpl, "%s/Test MF Arcade.lpl", pl_dir);
   sprintf(ref, "%s/Test MF Arcade ra.lpl", pl_dir);
   path_mkdir(db);
   path_mkdir(dir);
   sprintf(p, "%s/Test MF Arcade.rdb", db);
   memset(pos,  0, sizeof(pos));
   memset(last, 0, sizeof(last));
   wa_count = RA_FILES;
   wa_last  = last;
   use_db(db);
   use_queue(false);

   /* Twice: in name order, to learn the list's, then in that. */
   for (j = 0; j < 2; j++)
   {
      for (i = 0; i < RA_FILES; i++)
         if (!write_ra_zip(dir, i, pos[i], &rec[i]))
            goto fixture;
      if (!write_db_set(p, rec, RA_FILES))
         goto fixture;
      filestream_delete(lpl);
      if (j)
         break;
      if (!run_scan(dir) || !scan_completed)
         goto fixture;
      for (i = 0; i < RA_FILES; i++)
      {
         if (!last[i])
            goto fixture;
         for (k = 0; k < RA_FILES; k++)
            if (last[k] < last[i])
               pos[i]++;
         if (!pos[i])
            sprintf(first, "wa_%03u.zip", i);
      }
   }
   wa_last = NULL;

   filestream_delete(ref);
   if (!run_scan(dir) || !scan_completed || filestream_rename(lpl, ref))
      goto fixture;

   use_queue(true);
   retro_atomic_store_release_int(&reads_begun,  0);
   retro_atomic_store_release_int(&watch_scan,   0);
   retro_atomic_store_release_int(&watch_reader, 0);
   retro_atomic_store_release_int(&slow_in,      0);
   retro_atomic_store_release_int(&slow_back,    0);
   retro_atomic_store_release_int(&slow_by_scan, 0);
   retro_atomic_store_release_int(&hold_late,    0);
   retro_atomic_store_release_int(&stored_reads, 0);
   stored_name = "whole_ahead_rearm";
   dir_hold    = first;
   hold_dirs   = RA_FILES - 1;
   begun_size  = 30 + 4 + 1024 + 46 + 4 + 22;
   watch_size  = begun_size;
   slow_size   = RA_SLOW;
   slow_ms     = 1000;
   hold_size   = RA_JUNK;
   hold_want   = 1;
   started     = run_scan(dir) && scan_completed;
   begun_size  = 0;
   watch_size  = 0;
   slow_size   = 0;
   slow_ms     = 0;
   hold_size   = 0;
   stored_name = NULL;
   dir_hold    = NULL;
   begun       = retro_atomic_load_acquire_int(&reads_begun);
   slow        = retro_atomic_load_acquire_int(&slow_in);
   by_scan     = retro_atomic_load_acquire_int(&watch_scan);
   by_reader   = retro_atomic_load_acquire_int(&watch_reader);
   same        = files_equal(lpl, ref);

   sprintf(detail, "scan %d, readers %d", by_scan, by_reader);
   check(   started && by_reader > 0
         && !retro_atomic_load_acquire_int(&slow_by_scan),
         "rearm: readers read the zips after the first", detail);
   sprintf(detail, "%d + %d whole reads", begun, slow);
   check(started && begun == RA_FILES - 3 && slow == 1,
         "rearm: each zip read whole once", detail);
   late = retro_atomic_load_acquire_int(&hold_late);
   sprintf(detail, "%d holds ran out", late);
   check(started && late == 0, "rearm: the hold ended on its event", detail);
   check(   started && same && file_contains(lpl, "RA 000")
         && file_contains(lpl, "RA 008"),
         "rearm: the playlist of a run without readers",
         same ? "equal" : "differ");
   return;

fixture:
   wa_last = NULL;
   check(0, "fixture", "could not write or scan the rearm set");
}

/* The read ahead lanes' zips, by role: K a cartridge zip its member's
 * stored CRC matches, F the same 50 bytes longer, U one nothing
 * matches, N a .zip with no directory over the readers' cutoff, which
 * the scan reads whole itself and nothing matches, M the same 200
 * bytes shorter, W an N an arcade record holds by its own CRC, A an
 * arcade zip by its own CRC, B the same 100 bytes longer, G an A whose
 * member is a cart gate ROM, H a zip whose member the gate database
 * holds and S a 7z of a gate ROM, by its own CRC in a database whose
 * core opens 7z. */
#define AH_GATE_CRC 0x0CBFFFFFu
#define AH_FILES   40
#define AH_BIG     66000
#define AH_SLOW_MS 300

static uint32_t ah_member_len(char role)
{
   switch (role)
   {
      case 'N':
      case 'W': return AH_BIG;
      case 'M': return AH_BIG - 200;
      case 'F': return 1050;
      case 'A':
      case 'G':
      case 'H':
      case 'S': return 1100;
      case 'B': return 1200;
      default:  break;
   }
   return 1000;
}

/* The zip's size: one member, m.rd or m.ct. */
static uint32_t ah_len(char role)
{
   return 30 + 4 + ah_member_len(role) + 46 + 4 + 22;
}

/* Writes wa_<k>.zip, or .7z for an S, as the file at @pos in
 * content-list order. */
static bool write_ah_zip(const char *dir, unsigned k, unsigned pos,
      char role)
{
   static uint8_t d[AH_BIG + 128];
   char           p[1024];
   zip_entry_t    e;
   uint32_t       crc = (role == 'H') ? AH_GATE_CRC : 0x0CB00000u + pos;

   if (role == 'N' || role == 'M' || role == 'W')
   {
      sprintf(p, "%s/wa_%03u.7z", dir, k);
      filestream_delete(p);
      sprintf(p, "%s/wa_%03u.zip", dir, k);
      fill_forced(d, ah_len(role), 83 + pos, crc);
      return filestream_write_file(p, d, ah_len(role));
   }
   fill_forced(d, ah_member_len(role), 83 + pos, crc);
   memset(&e, 0, sizeof(e));
   e.name = (role == 'G' || role == 'H' || role == 'S') ? "m.ct" : "m.rd";
   e.data = d;
   e.len  = ah_member_len(role);
   e.crc  = crc;
   sprintf(p, "%s/wa_%03u.%s", dir, k, (role == 'S') ? "zip" : "7z");
   filestream_delete(p);
   sprintf(p, "%s/wa_%03u.%s", dir, k, (role == 'S') ? "7z" : "zip");
#ifdef HAVE_7ZIP
   if (role == 'S')
      return write_7z(p, e.name, d, e.len, crc) != 0;
#endif
   return write_zip_entries(p, &e, 1) != 0;
}

/* Scans zips of @roles, in content-list order, on the threaded queue;
 * true when every K, F, A, B, G and W is in its playlist.  The first
 * zip's directory read waits for the others', so a reader has read
 * each directory by the scan's first verdict.  With @gate, the readers
 * instead wait in the F zips' (one each: the last), until all are in
 * and @gate slow reads have begun, and the scan's read of the first N
 * waits for them to be in.  A stream of @slow bytes sleeps AH_SLOW_MS
 * before its first quantum, time for the readers to take what they
 * may: an N after each match.  Reads of @begun-byte zips are counted as
 * each begins, those of @watch-byte ones by who makes them. */
static bool ah_scan(const char *root, const char *pl_dir, const char *tag,
      const char *roles, uint32_t slow, uint32_t begun, uint32_t watch,
      int gate)
{
   static unsigned last[AH_FILES];
   static char     names[AH_FILES][16];
   static char     first[32];
   static db_rec_t cart[AH_FILES], arc[AH_FILES], set7[AH_FILES];
   char            db[512], dir[512], p[1024], lpl[2][1024];
   unsigned        pos[AH_FILES];
   unsigned        n = (unsigned)strlen(roles);
   unsigned        i, j, k, nc = 0, na = 0, ns = 0;
   bool            ok;

   if (n > AH_FILES)
      return false;
   sprintf(db,  "%s/whole_ahead_%s_db", root, tag);
   sprintf(dir, "%s/whole_ahead_%s",    root, tag);
   sprintf(lpl[0], "%s/Test MF Cart.lpl",   pl_dir);
   sprintf(lpl[1], "%s/Test MF Arcade.lpl", pl_dir);
   path_mkdir(db);
   path_mkdir(dir);
   memset(pos,  0, sizeof(pos));
   memset(last, 0, sizeof(last));
   wa_count = n;
   wa_last  = last;
   use_db(db);
   use_queue(false);

   /* Twice: in name order, to learn the list's, then in that. */
   for (j = 0; j < 2; j++)
   {
      nc = na = ns = 0;
      for (i = 0; i < n; i++)
      {
         char role = roles[pos[i]];

         if (!write_ah_zip(dir, i, pos[i], role))
            goto fail;
         sprintf(names[pos[i]], "AH %c%03u", role, pos[i]);
         if (role == 'K' || role == 'F')
            set_rec(&cart[nc++], names[pos[i]], 0x0CB00000u + pos[i],
                  ah_member_len(role));
         else if (   role == 'A' || role == 'B' || role == 'G'
                  || role == 'W')
         {
            sprintf(p, "%s/wa_%03u.zip", dir, i);
            if (!set_whole_rec(&arc[na++], names[pos[i]], p))
               goto fail;
         }
         else if (role == 'S')
         {
            sprintf(p, "%s/wa_%03u.7z", dir, i);
            if (!set_whole_rec(&set7[ns], names[pos[i]], p))
               goto fail;
            set7[ns++].rom_name = "set.7z";
         }
      }
      sprintf(p, "%s/Test MF Cart.rdb", db);
      if (!write_db_set(p, cart, nc))
         goto fail;
      sprintf(p, "%s/Test MF Arcade.rdb", db);
      if (!write_db_set(p, arc, na))
         goto fail;
      sprintf(p, "%s/Test Archives.rdb", db);
      if (ns && !write_db_set(p, set7, ns))
         goto fail;
      if (strpbrk(roles, "GHS"))
      {
         db_rec_t gate;

         set_rec(&gate, "AH gate", AH_GATE_CRC, ah_member_len('G'));
         sprintf(p, "%s/Test Cart Gate.rdb", db);
         if (!write_db_set(p, &gate, 1))
            goto fail;
      }
      filestream_delete(lpl[0]);
      filestream_delete(lpl[1]);
      if (j)
         break;
      if (!run_scan(dir) || !scan_completed)
         goto fail;
      for (i = 0; i < n; i++)
      {
         if (!last[i])
            goto fail;
         for (k = 0; k < n; k++)
            if (last[k] < last[i])
               pos[i]++;
         if (!pos[i])
            sprintf(first, "wa_%03u.", i);
      }
   }
   wa_last = NULL;

   use_queue(true);
   retro_atomic_store_release_int(&reads_begun,  0);
   retro_atomic_store_release_int(&reader_begun, 0);
   retro_atomic_store_release_int(&watch_scan,   0);
   retro_atomic_store_release_int(&watch_reader, 0);
   retro_atomic_store_release_int(&slow_in,      0);
   retro_atomic_store_release_int(&slow_back,    0);
   retro_atomic_store_release_int(&stored_reads, 0);
   retro_atomic_store_release_int(&gate_in,      0);
   retro_atomic_store_release_int(&gate_on_scan, 0);
   memset(slow_begun, 0, sizeof(slow_begun));
   stored_name = dir;
   dir_hold    = gate ? NULL : first;
   hold_dirs   = (int)n - 1;
   gate_size   = ah_member_len('F');
   gate_slow   = gate;
   gate_n      = gate ? (int)readers_expected() : 0;
   hold_size   = gate ? ah_len('N') : 0;
   hold_want   = 0;
   begun_size  = begun;
   watch_size  = watch;
   slow_size   = slow;
   slow_ms     = AH_SLOW_MS;
   ok          = run_scan(dir) && scan_completed;
   begun_size  = 0;
   watch_size  = 0;
   slow_size   = 0;
   slow_ms     = 0;
   gate_n      = 0;
   hold_size   = 0;
   stored_name = NULL;
   dir_hold    = NULL;
   for (i = 0; i < nc; i++)
      ok = ok && file_contains(lpl[0], cart[i].name);
   for (i = 0; i < na; i++)
      ok = ok && file_contains(lpl[1], arc[i].name);
   return ok;

fail:
   wa_last = NULL;
   return false;
}

/* One arcade zip among cartridge zips: its whole match lets the
 * readers read one zip whole ahead while the scan reads the N, whose
 * miss takes back the rest.  Which one is theirs to pick. */
static void ahead_lone_lane(const char *root, const char *pl_dir)
{
   char detail[64];
   bool ok;
   int  begun;

   if (!readers_expected())
   {
      check(1, "read ahead: one after a lone match", "no readers");
      return;
   }
   ok    = ah_scan(root, pl_dir, "lone", "KANKKKKKKKK",
         ah_len('N'), ah_len('K'), 0, 0);
   begun = retro_atomic_load_acquire_int(&reader_begun);
   sprintf(detail, "%d reader reads", begun);
   check(ok && begun == 1, "read ahead: one after a lone match", detail);
}

/* Whole matches in a row let the readers read 1, 2, then 4 zips whole
 * ahead, up to SCAN_READER_AHEAD_MAX each; the N's miss takes back the
 * rest.  The W and the N the scan reads itself, so the readers read
 * only K whole.  Test only: the lookup of each after the first waits
 * for the readers to begin every read granted before it. */
static void ahead_double_lane(const char *root, const char *pl_dir)
{
   char detail[96];
   bool ok;
   int  want[3];
   int  i, g, sum, begun;

   if (!readers_expected())
   {
      check(1, "read ahead: matches in a row double it", "no readers");
      return;
   }
   for (i = 0, g = 1, sum = 0; i < 3; i++, g *= 2)
   {
      want[i]      = sum += (g < SCAN_READER_AHEAD_MAX)
            ? g : SCAN_READER_AHEAD_MAX;
      look_crc[i]  = 0x0CB00001u + (uint32_t)i;
      look_want[i] = want[i];
      look_end[i]  = 0;
   }
   look_n = 3;
   ok     = ah_scan(root, pl_dir, "double", "WWWNKKKKKKKKKKKK", 0, 0, 0,
         0);
   look_n = 0;
   begun  = retro_atomic_load_acquire_int(&rd_wholes);
   sprintf(detail, "%d, %d, %d, %d reader reads (want %d, %d, %d)",
         look_end[0], look_end[1], look_end[2], begun,
         want[0], want[1], want[2]);
   check(   ok && !retro_atomic_load_acquire_int(&look_late)
         && look_end[0] == want[0] && look_end[1] == want[1]
         && look_end[2] == want[2] && begun == want[2],
         "read ahead: matches in a row double it", detail);
}

/* Match, hit, match: a member hit ends the run, so the second match
 * lets the readers read one zip whole ahead again, not two.  Test
 * only: each K's lookup waits for the reads granted before it. */
static void ahead_streak_lane(const char *root, const char *pl_dir)
{
   char detail[64];
   bool ok;
   int  begun;

   if (!readers_expected())
   {
      check(1, "read ahead: a member hit ends the run", "no readers");
      return;
   }
   look_crc[0]  = 0x0CB00002u;
   look_want[0] = 1;
   look_crc[1]  = 0x0CB00004u;
   look_want[1] = 2;
   look_n       = 2;
   ok           = ah_scan(root, pl_dir, "streak", "KAKAKKKKKKKK", 0, 0, 0,
         0);
   look_n       = 0;
   begun        = retro_atomic_load_acquire_int(&rd_wholes);
   sprintf(detail, "%d reader reads", begun);
   check(   ok && !retro_atomic_load_acquire_int(&look_late)
         && begun == 2, "read ahead: a member hit ends the run", detail);
}

/* @head, then an F for each reader: the last zips, which the readers
 * wait in with @gate. */
static void ah_gate_roles(char *roles, size_t len, const char *head)
{
   size_t   n = strlen(head);
   unsigned r = readers_expected();

   if (n + r >= len)
      r = 0;
   memcpy(roles, head, n);
   memset(roles + n, 'F', r);
   roles[n + r] = '\0';
}

/* Each reader's last zip, an F far ahead, holds only its stored CRC
 * when the first match grants one read whole: it goes to a K by the
 * scan, not to an F.  The K outnumber the readers, so readers racing
 * for the one credit cannot all pass them.  Test only: the first K's
 * lookup waits for the read to begin. */
static void ahead_next_lane(const char *root, const char *pl_dir)
{
   char roles[64], detail[64];
   bool ok;
   int  next_reads, far_reads;

   if (!readers_expected())
   {
      check(1, "read ahead: the scan's next first", "no readers");
      return;
   }
   ah_gate_roles(roles, sizeof(roles), "NNAKKKKKKKKK");
   look_crc[0]  = 0x0CB00003u;
   look_want[0] = 1;
   look_n       = 1;
   ok           = ah_scan(root, pl_dir, "next", roles, ah_len('N'),
         ah_len('K'), ah_len('F'), 1);
   look_n       = 0;
   next_reads   = retro_atomic_load_acquire_int(&reader_begun);
   far_reads    = retro_atomic_load_acquire_int(&watch_reader);
   sprintf(detail, "next %d, far %d reader reads", next_reads,
         far_reads);
   check(   ok && !retro_atomic_load_acquire_int(&look_late)
         && next_reads == 1 && !far_reads,
         "read ahead: the scan's next first", detail);
}

/* Three whole matches grant seven reads whole while the readers wait
 * in their F, and a member hit follows: once out, they read no F.  A reader on
 * the scan's worker cannot wait there (the scan would not run): it
 * may read its own F, while the others' are held. */
static void ahead_hit_count_lane(const char *root, const char *pl_dir)
{
   char roles[64], detail[80];
   bool ok, on_scan;
   int  far_reads;

   if (!readers_expected())
   {
      check(1, "read ahead: a member hit takes back what is left",
            "no readers");
      return;
   }
   ah_gate_roles(roles, sizeof(roles), "NAAAKN");
   ok        = ah_scan(root, pl_dir, "hitcount", roles, ah_len('N'),
         ah_len('F'), 0, 2);
   far_reads = retro_atomic_load_acquire_int(&reader_begun);
   on_scan   = retro_atomic_load_acquire_int(&gate_on_scan) > 0;
   sprintf(detail, "%d reader reads of an F%s", far_reads,
         on_scan ? ", one reader on the scan's worker" : "");
   check(ok && far_reads <= (on_scan ? 1 : 0),
         "read ahead: a member hit takes back what is left", detail);
}

/* Arcade zips, each a whole match: the readers read whole ahead as the
 * matches add up, so past the first 16 the scan reads none itself.
 * Test only: each zip's lookup waits for the readers to begin every
 * read the matches before it granted. */
static void ahead_arcade_lane(const char *root, const char *pl_dir)
{
   char     detail[128];
   bool     ok;
   int      begun, by_scan, by_reader, granted = 0, grant = 1;
   unsigned k;

   if (!readers_expected())
   {
      check(1, "read ahead: arcade zips by readers", "no readers");
      return;
   }
   for (k = 0; k < 40; k++)
   {
      look_crc[k]  = 0x0CB00000u + k;
      look_want[k] = granted;
      granted     += grant;
      grant        = (grant * 2 < SCAN_READER_AHEAD_MAX)
         ? grant * 2 : SCAN_READER_AHEAD_MAX;
   }
   look_n     = 40;
   look_files = 40;
   ok         = ah_scan(root, pl_dir, "arcade",
         "AAAAAAAAAAAAAAAABBBBBBBBBBBBBBBBBBBBBBBB", 0, ah_len('B'),
         ah_len('B'), 0);
   look_n     = 0;
   look_files = 0;
   begun      = retro_atomic_load_acquire_int(&reads_begun);
   by_scan    = retro_atomic_load_acquire_int(&watch_scan);
   by_reader  = retro_atomic_load_acquire_int(&watch_reader);
   sprintf(detail, "%d whole reads: scan %d, readers %d", begun, by_scan,
         by_reader);
   passes_detail(detail);
   check(   ok && begun == 24 && by_scan == 0 && by_reader > 0
         && retro_atomic_load_acquire_int(&look_hits) >= 40
         && !retro_atomic_load_acquire_int(&look_late),
         "read ahead: arcade zips by readers", detail);
}

/* Two arcade zips match whole, then a G, whose member lets in a
 * database not walked yet: while the scan walks it, the readers read
 * whole ahead as far as they may, and the G's match after grants what
 * it would have.  Test only: the second zip's lookup waits for
 * the first's grant to be taken, the walk for the readers to begin
 * every read granted before it, then as many more as they may, and the
 * lookup of the A after the N for them to begin the G's. */
static void ahead_walk_lane(const char *root, const char *pl_dir)
{
   char detail[160];
   bool ok;
   bool late;
   int  g2 = (2 < SCAN_READER_AHEAD_MAX) ? 2 : SCAN_READER_AHEAD_MAX;
   int  g3 = (4 < SCAN_READER_AHEAD_MAX) ? 4 : SCAN_READER_AHEAD_MAX;
   int  want;

   if (!readers_expected())
   {
      check(1, "read ahead: whole while the scan walks", "no readers");
      check(1, "read ahead: a walk grants nothing after", "no readers");
      return;
   }
   walk_name    = "Test Cart Gate";
   walk_want    = 1 + g2;
   walk_more    = SCAN_READER_AHEAD_MAX;
   walk_late    = false;
   walk_from    = -1;
   walk_to      = -1;
   want         = walk_want + SCAN_READER_AHEAD_MAX + g3;
   look_crc[0]  = 0x0CB00001u;
   look_want[0] = 1;
   look_crc[1]  = 0x0CB00004u;
   look_want[1] = want;
   look_end[1]  = -1;
   look_n       = 2;
   ok           = ah_scan(root, pl_dir, "walk",
         "AAGNAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", ah_len('N'),
         ah_len('A'), 0, 0);
   look_n       = 0;
   walk_name    = NULL;
   walk_job     = NULL;
   late         = walk_late || retro_atomic_load_acquire_int(&look_late);
   sprintf(detail, "readers %d as it began, %d as it ended", walk_from,
         walk_to);
   passes_detail(detail);
   strcat(detail, walk_late ? ", walk late" : "");
   check(   ok && !late && retro_atomic_load_acquire_int(&look_hits) > 0
         && walk_from == walk_want,
         "read ahead: whole while the scan walks", detail);
   sprintf(detail, "%d reader reads by the A after the N (want %d)%s",
         look_end[1], want, late ? ", late" : "");
   check(ok && !late && look_end[1] == want,
         "read ahead: a walk grants nothing after", detail);
}

/* Two 7z match whole, by their own CRC; their gate ROMs let in only
 * the gate database.  Then a zip walks the arcade one: H after its
 * member hit there, N in the whole pass, its own CRC read at once.
 * Neither walk lets the readers read ahead.  Test only: files and
 * databases go in name order, the second 7z's lookup waits for the
 * first's grant to be taken, and each walk for the readers to begin
 * every read granted before it, then gives them time to begin more. */
static void ahead_walk_none_lane(const char *root, const char *pl_dir)
{
   static const char *const what[2] = {
      "read ahead: none for a walk after a member hit",
      "read ahead: none for a walk in the whole pass" };
#ifdef HAVE_7ZIP
   static const char *const roles[2] = {
      "SSHAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
      "SSNAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" };
   char     detail[160], lpl[1024];
   bool     ok;
   unsigned k;

   for (k = 0; k < 2; k++)
   {
      if (!readers_expected())
      {
         check(1, what[k], "no readers");
         continue;
      }
      sprintf(lpl, "%s/Test Cart Gate.lpl", pl_dir);
      filestream_delete(lpl);
      walk_name      = "Test MF Arcade";
      walk_want      = 1 + ((2 < SCAN_READER_AHEAD_MAX)
            ? 2 : SCAN_READER_AHEAD_MAX);
      walk_more      = 0;
      walk_late      = false;
      walk_from      = -1;
      walk_to        = -1;
      look_crc[0]    = 0x0CB00001u;
      look_want[0]   = 1;
      look_n         = 1;
      db_order       = 1;
      content_sorted = true;
      ok             = ah_scan(root, pl_dir,
            k ? "walk_whole" : "walk_hit", roles[k], 0, 0, 0, 0)
         && (k || file_contains(lpl, "AH gate"));
      look_n         = 0;
      db_order       = 0;
      content_sorted = false;
      walk_name      = NULL;
      walk_job       = NULL;
      filestream_delete(lpl);
      sprintf(detail, "readers %d as it began, %d as it ended%s",
            walk_from, walk_to, walk_late ? ", walk late" : "");
      check(   ok && !walk_late && !retro_atomic_load_acquire_int(&look_late)
            && walk_from == walk_want && walk_to == walk_from, what[k],
            detail);
   }
#else
   check(1, what[0], "no 7z");
   check(1, what[1], "no 7z");
#endif
}

#define AM_FILES 12
#define AM_SLOW_MS 50

/* Writes wa_<k>.zip for the file at @pos in content-list order: one
 * zip read whole that matches (into @lead), @fill read whole that
 * miss, then AM_FILES cartridge zips (into @cart), every third (from
 * the second) a slow miss instead. */
static bool write_am_zip(const char *dir, unsigned k, unsigned pos,
      unsigned fill, db_rec_t *lead, db_rec_t *cart, unsigned *nc)
{
   static uint8_t d[MF_MEMBER_LEN];
   static char    names[AM_FILES][16];
   char           p[1024];
   zip_entry_t    e;
   unsigned       at = pos - fill - 1;

   memset(&e, 0, sizeof(e));
   e.name = (pos == 0) ? "ld.rd" : (pos <= fill) ? "fil.rd"
          : (at % 3 == 1) ? "miss.rd" : "m.rd";
   e.data = d;
   e.len  = MF_MEMBER_LEN;
   e.crc  = 0x0BFD0000u + pos;
   fill_forced(d, MF_MEMBER_LEN, 131 + pos, e.crc);
   sprintf(p, "%s/wa_%03u.zip", dir, k);
   if (!write_zip_entries(p, &e, 1))
      return false;
   if (pos == 0)
      return set_whole_rec(lead, "AM Lead", p);
   if (pos > fill && at % 3 != 1)
   {
      sprintf(names[at], "AM %03u", at);
      set_rec(&cart[(*nc)++], names[at], e.crc, MF_MEMBER_LEN);
   }
   return true;
}

/* Zips on the threaded queue: one that matches by its own CRC, then a
 * window and a few more that miss by theirs, then cartridge zips,
 * every third of them a miss read slowly, which gives readers time to
 * read ahead were it on.  Every miss is read whole once.  Read-ahead
 * turns on at the match and off at the first miss, before the window
 * reaches a cartridge zip, so no reader reads one whole, nor does the
 * scan.  Positions are in content-list order, learned from a first
 * scan. */
static void ahead_miss_lane(const char *root, const char *pl_dir)
{
   static uint8_t  d[MF_MEMBER_LEN];
   static db_rec_t cart[AM_FILES];
   static db_rec_t arcade[3];
   static char     lpl[2][1024];
   char            db[512], dir[512], p[1024], detail[64];
   zip_entry_t     e;
   uint32_t        cart_bytes, fill_bytes, miss_bytes;
   unsigned        fill = reader_window() + WA_EXTRA;
   unsigned        n    = 1 + fill + AM_FILES;
   unsigned        i, j, k, nc = 0, misses = 0;
   unsigned       *pos  = NULL;
   int             by_scan, by_reader, slow, begun;
   bool            started, all = true;

   if (!readers_expected())
   {
      check(1, "ahead miss: no read-ahead after a whole miss",
            "no readers");
      return;
   }
   sprintf(db,  "%s/ahead_miss_db", root);
   sprintf(dir, "%s/whole_ahead_miss", root);
   sprintf(lpl[0], "%s/Test EM Cart.lpl", pl_dir);
   sprintf(lpl[1], "%s/Test EM Arcade.lpl", pl_dir);
   path_mkdir(db);
   path_mkdir(dir);
   memset(&e, 0, sizeof(e));
   e.data     = d;
   e.len      = MF_MEMBER_LEN;
   e.name     = "m.rd";
   cart_bytes = zip_bytes(&e, 1);
   e.name     = "fil.rd";
   fill_bytes = zip_bytes(&e, 1);
   e.name     = "miss.rd";
   miss_bytes = zip_bytes(&e, 1);
   pos        = (unsigned*)calloc(n, sizeof(*pos));
   wa_last    = (unsigned*)calloc(n, sizeof(*wa_last));
   wa_count   = n;
   if (!pos || !wa_last)
      goto fixture;
   for (k = 0; k < AM_FILES; k++)
      if (k % 3 == 1)
         misses++;
   set_rec(&arcade[1], "AM Gate", 0x0BFDFFFFu, miss_bytes);
   set_rec(&arcade[2], "AM Fill", 0x0BFDFFFEu, fill_bytes);
   arcade[1].rom_name = "set.zip";
   arcade[2].rom_name = "set.zip";
   use_queue(false);
   use_db(db);

   /* Twice: all misses, to learn the list's order (an entry's file is
    * looked at again), then in that. */
   for (j = 0; j < 2; j++)
   {
      nc = 0;
      for (i = 0; i < n; i++)
         if (!write_am_zip(dir, i, j ? pos[i] : 1, fill, &arcade[0], cart,
               &nc))
            goto fixture;
      sprintf(p, "%s/Test EM Cart.rdb", db);
      if (!write_db_set(p, cart, nc))
         goto fixture;
      sprintf(p, "%s/Test EM Arcade.rdb", db);
      if (!write_db_set(p, j ? arcade : arcade + 1, j ? 3 : 2))
         goto fixture;
      filestream_delete(lpl[0]);
      filestream_delete(lpl[1]);
      if (j)
         break;
      if (!run_scan(dir) || !scan_completed)
         goto fixture;
      for (i = 0; i < n; i++)
      {
         if (!wa_last[i])
            goto fixture;
         for (k = 0; k < n; k++)
            if (wa_last[k] < wa_last[i])
               pos[i]++;
      }
   }
   free(wa_last);
   wa_last = NULL;

   use_queue(true);
   retro_atomic_store_release_int(&watch_scan,   0);
   retro_atomic_store_release_int(&watch_reader, 0);
   retro_atomic_store_release_int(&reads_begun,  0);
   retro_atomic_store_release_int(&slow_in,      0);
   retro_atomic_store_release_int(&slow_back,    0);
   watch_size = cart_bytes;
   begun_size = fill_bytes;
   slow_size  = miss_bytes;
   slow_ms    = AM_SLOW_MS;
   started    = run_scan(dir) && scan_completed;
   watch_size = 0;
   begun_size = 0;
   slow_size  = 0;
   slow_ms    = 0;
   by_scan    = retro_atomic_load_acquire_int(&watch_scan);
   by_reader  = retro_atomic_load_acquire_int(&watch_reader);
   begun      = retro_atomic_load_acquire_int(&reads_begun);
   slow       = retro_atomic_load_acquire_int(&slow_in);
   for (k = 0; k < AM_FILES; k++)
   {
      if (k % 3 != 1)
      {
         sprintf(p, "AM %03u", k);
         all = all && file_contains(lpl[0], p);
      }
   }

   sprintf(detail, "scan %d, readers %d", by_scan, by_reader);
   check(started && by_scan == 0 && by_reader == 0,
         "ahead miss: no cartridge zip read whole after a miss", detail);
   sprintf(detail, "%d of %u, %d of %u", begun, fill, slow, misses);
   check(started && begun == (int)fill && slow == (int)misses,
         "ahead miss: each miss read whole once", detail);
   check(started && all && file_contains(lpl[1], "AM Lead"),
         "ahead miss: each cartridge zip and the lead are their entries",
         "Test EM Cart, Test EM Arcade");
   free(pos);
   return;

fixture:
   free(wa_last);
   wa_last = NULL;
   free(pos);
   check(0, "fixture", "could not write the ahead-miss set");
}

/* Archives whose first member misses and whose second is a record
 * named for that archive, on the threaded queue: none needs its whole
 * read, so the readers read only their directories. */
static void late_member_lane(const char *root, const char *pl_dir)
{
   static uint8_t d[2][MF_MEMBER_LEN];
   char           db[512], dir[512], p[1024], lpl[1024];
   char           first[32], last[32], detail[64];
   char         (*names)[16] = NULL;
   char         (*roms)[16]  = NULL;
   db_rec_t      *rec        = NULL;
   zip_entry_t    e[2];
   unsigned       n          = reader_window() + WA_EXTRA;
   unsigned       k;
   int            by_scan, by_reader;
   bool           started;

   if (!readers_expected())
   {
      check(1, "late members: readers read no archive whole", "no readers");
      return;
   }
   sprintf(db,  "%s/late_member_db", root);
   sprintf(dir, "%s/late_member",    root);
   path_mkdir(db);
   path_mkdir(dir);
   names = (char(*)[16])calloc(n, sizeof(*names));
   roms  = (char(*)[16])calloc(n, sizeof(*roms));
   rec   = (db_rec_t*)calloc(n, sizeof(*rec));
   if (!names || !roms || !rec)
      goto fixture;
   memset(e, 0, sizeof(e));
   e[0].name = "a.rd";
   e[0].data = d[0];
   e[0].len  = MF_MEMBER_LEN;
   e[1].name = "b.rd";
   e[1].data = d[1];
   e[1].len  = MF_MEMBER_LEN;
   for (k = 0; k < n; k++)
   {
      e[0].crc = 0x0BF60000u + 2 * k;
      e[1].crc = 0x0BF60001u + 2 * k;
      fill_forced(d[0], MF_MEMBER_LEN, 91 + k, e[0].crc);
      fill_forced(d[1], MF_MEMBER_LEN, 93 + k, e[1].crc);
      sprintf(p, "%s/lm_%03u.zip", dir, k);
      if (!write_zip_entries(p, e, 2))
         goto fixture;
      sprintf(names[k], "LM %03u", k);
      set_rec(&rec[k], names[k], e[1].crc, MF_MEMBER_LEN);
      sprintf(roms[k], "lm_%03u.zip", k);
      rec[k].rom_name = roms[k];
   }
   sprintf(p, "%s/Test EM Cart.rdb", db);
   if (!write_db_set(p, rec, n))
      goto fixture;

   sprintf(lpl, "%s/Test EM Cart.lpl", pl_dir);
   filestream_delete(lpl);
   use_queue(true);
   use_db(db);
   retro_atomic_store_release_int(&watch_scan, 0);
   retro_atomic_store_release_int(&watch_reader, 0);
   watch_size = zip_bytes(e, 2);
   started    = run_scan(dir) && scan_completed;
   watch_size = 0;
   by_scan    = retro_atomic_load_acquire_int(&watch_scan);
   by_reader  = retro_atomic_load_acquire_int(&watch_reader);

   sprintf(first, "lm_%03u.zip", 0u);
   sprintf(last,  "lm_%03u.zip", n - 1);
   check(   started && file_contains(lpl, first) && file_contains(lpl, last)
         && !file_contains(lpl, ".zip#"),
         "late members: each archive is its entry", last);
   sprintf(detail, "scan %d, readers %d", by_scan, by_reader);
   check(started && by_scan == 0 && by_reader == 0,
         "late members: readers read no archive whole", detail);
   free(names);
   free(roms);
   free(rec);
   return;

fixture:
   free(names);
   free(roms);
   free(rec);
   check(0, "fixture", "could not write the late-member set");
}

/* No core claiming the database opens zips, and one claims the
 * members: the hits are archive#member entries and no archive is read
 * whole, on either queue, the threaded one with a window and more of
 * them. */
static void no_claim_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static uint8_t d[2][MF_MEMBER_LEN];
   char           db[512], dir[512], p[1024], lpl[1024];
   char           first[48], last[48], what[64], detail[64];
   char         (*names)[16] = NULL;
   db_rec_t      *rec        = NULL;
   zip_entry_t    e[2];
   unsigned       n          = (threaded && readers_expected())
      ? reader_window() + WA_EXTRA : 3;
   unsigned       k;
   int            reads;
   bool           started;
   const char    *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,  "%s/no_claim_db_%d", root, threaded ? 1 : 0);
   sprintf(dir, "%s/no_claim_%d",    root, threaded ? 1 : 0);
   path_mkdir(db);
   path_mkdir(dir);
   names = (char(*)[16])calloc(n, sizeof(*names));
   rec   = (db_rec_t*)calloc(n, sizeof(*rec));
   if (!names || !rec)
      goto fixture;
   memset(e, 0, sizeof(e));
   e[0].name = "a.rd";
   e[0].data = d[0];
   e[0].len  = MF_MEMBER_LEN;
   e[1].name = "b.rd";
   e[1].data = d[1];
   e[1].len  = MF_MEMBER_LEN;
   for (k = 0; k < n; k++)
   {
      e[0].crc = 0x0BF70000u + 2 * k;
      e[1].crc = 0x0BF70001u + 2 * k;
      fill_forced(d[0], MF_MEMBER_LEN, 95 + k, e[0].crc);
      fill_forced(d[1], MF_MEMBER_LEN, 97 + k, e[1].crc);
      sprintf(p, "%s/nc_%03u.zip", dir, k);
      if (!write_zip_entries(p, e, 2))
         goto fixture;
      sprintf(names[k], "NC %03u", k);
      set_rec(&rec[k], names[k], e[1].crc, MF_MEMBER_LEN);
   }
   sprintf(p, "%s/Test EM Member.rdb", db);
   if (!write_db_set(p, rec, n))
      goto fixture;

   sprintf(lpl, "%s/Test EM Member.lpl", pl_dir);
   filestream_delete(lpl);
   use_queue(threaded);
   use_db(db);
   retro_atomic_store_release_int(&watch_scan, 0);
   retro_atomic_store_release_int(&watch_reader, 0);
   watch_size = zip_bytes(e, 2);
   started    = run_scan(dir) && scan_completed;
   watch_size = 0;
   reads      = retro_atomic_load_acquire_int(&watch_scan)
      + retro_atomic_load_acquire_int(&watch_reader);

   sprintf(first, "nc_%03u.zip#b.rd", 0u);
   sprintf(last,  "nc_%03u.zip#b.rd", n - 1);
   sprintf(what, "no core opens zips: archive#member %s", q);
   check(   started && file_contains(lpl, first)
         && file_contains(lpl, last), what, last);
   sprintf(what, "no core opens zips: none read whole %s", q);
   sprintf(detail, "%d read steps", reads);
   check(started && reads == 0, what, detail);
   free(names);
   free(rec);
   return;

fixture:
   free(names);
   free(rec);
   check(0, "fixture", "could not write the no-claim set");
}

#define SG_VARIANTS 4

/* An archive is read whole only when a database whose cores open zips
 * holds a zip record of its size, or one with no size while the
 * archive's size is inside the database's size range.  The third
 * record carries the archive's own CRC, so a whole read would name
 * it: when it is not asked the member hits are held entries.  The
 * fourth, in the last variant only, stretches the range over the
 * archive. */
static void size_gate_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static uint8_t d[3][MF_MEMBER_LEN];
   static const char *const how[SG_VARIANTS] = {
      "another size", "its size", "no size, out of range",
      "no size, in range" };
   char           db[512], dir[512], p[1024], lpl[1024];
   char           what[96], detail[64];
   db_rec_t       rec[4];
   zip_entry_t    e[3];
   uint32_t       bytes;
   int            reads;
   bool           started, whole;
   unsigned       v;
   const char    *q = threaded ? "(threaded)" : "(regular)";

   sprintf(dir, "%s/size_gate_%d", root, threaded ? 1 : 0);
   path_mkdir(dir);
   memset(e, 0, sizeof(e));
   e[0].name = "a.rd";
   e[1].name = "b.rd";
   e[2].name = "c.rd";
   for (v = 0; v < 3; v++)
   {
      e[v].data = d[v];
      e[v].len  = MF_MEMBER_LEN;
      e[v].crc  = 0x0BFA0001u + v;
      fill_forced(d[v], MF_MEMBER_LEN, 111 + 2 * v, e[v].crc);
   }
   sprintf(p, "%s/sg.zip", dir);
   if (   !write_zip_entries(p, e, 3)
       || !set_whole_rec(&rec[2], "SG Whole", p))
      goto fixture;
   bytes = rec[2].size;
   set_rec(&rec[0], "SG B", e[1].crc, MF_MEMBER_LEN);
   set_rec(&rec[1], "SG C", e[2].crc, MF_MEMBER_LEN);
   /* No member stores its CRC. */
   set_rec(&rec[3], "SG Big", 0x0BFA00F0u, bytes + 1);

   sprintf(lpl, "%s/Test EM Arcade.lpl", pl_dir);
   use_queue(threaded);
   for (v = 0; v < SG_VARIANTS; v++)
   {
      whole          = (v == 1 || v == 3);
      rec[2].size    = (v == 0) ? bytes + 1 : bytes;
      rec[2].unsized = (v >= 2);
      sprintf(db, "%s/size_gate_db%u_%d", root, v, threaded ? 1 : 0);
      path_mkdir(db);
      sprintf(p, "%s/Test EM Arcade.rdb", db);
      if (!write_db_set(p, rec, (v == 3) ? 4 : 3))
         goto fixture;
      filestream_delete(lpl);
      use_db(db);
      retro_atomic_store_release_int(&watch_scan, 0);
      retro_atomic_store_release_int(&watch_reader, 0);
      watch_size = bytes;
      started    = run_scan(dir) && scan_completed;
      watch_size = 0;
      reads      = retro_atomic_load_acquire_int(&watch_scan)
         + retro_atomic_load_acquire_int(&watch_reader);

      sprintf(detail, "%d read steps", reads);
      sprintf(what, "size gate, a zip record of %s: %s %s", how[v],
            whole ? "read whole" : "not read whole", q);
      check(started && (whole ? reads > 0 : reads == 0), what, detail);
      if (whole)
      {
         sprintf(what, "size gate, a zip record of %s: its CRC wins %s",
               how[v], q);
         check(   started && file_contains(lpl, "SG Whole")
               && !file_contains(lpl, "sg.zip#"), what, "sg.zip");
      }
      else
      {
         sprintf(what, "size gate, a zip record of %s: held entries %s",
               how[v], q);
         check(   started && file_contains(lpl, "sg.zip#b.rd")
               && file_contains(lpl, "sg.zip#c.rd")
               && !file_contains(lpl, "SG Whole"), what,
               "sg.zip#b.rd, #c.rd");
      }
   }
   return;

fixture:
   check(0, "fixture", "could not write the size-gate set");
}

#define SE_SHARED 33

/* A file that errors leaves nothing to the next: a cue whose serial
 * query fails (more records share its serial than the index holds, so
 * it takes the query), then an archive matched by its member, which
 * must be written as a crc match, not under the cue's serial. */
static void serial_error_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static const char *const str[] = {
      "PSX.EXE;1", "SLUS_001.52;1", "SYSTEM.CNF;1",
      "BOOT = cdrom:\\SLUS_001.52;1"
   };
   static const unsigned at[] = { 52407, 52517, 52577, 54120 };
   static uint8_t d[MF_MEMBER_LEN];
   char           db[512], dir[512], p[1024], lpl[1024], what[64];
   char           detail[64], crc[16];
   db_rec_t       disc[SE_SHARED], cart;
   zip_entry_t    e;
   unsigned       i;
   int            fails;
   bool           started;
   const char    *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,  "%s/serial_error_db", root);
   sprintf(dir, "%s/serial_error",    root);
   path_mkdir(db);
   path_mkdir(dir);

   sprintf(p, "%s/a_err.bin", dir);
   if (!write_ps1_track(p, str, at, ARRAY_SIZE(str), 0x5E1AE000u))
      goto fixture;
   sprintf(p, "%s/a_err.cue", dir);
   if (!write_cue(p, "a_err.bin"))
      goto fixture;
   for (i = 0; i < SE_SHARED; i++)
   {
      set_rec(&disc[i], "SE Shared", 0x5E1AE100u + i, PS1_TRACK_SIZE);
      disc[i].serial = "SLUS-00152";
   }
   sprintf(p, "%s/Test SE Disc.rdb", db);
   if (!write_db_set(p, disc, SE_SHARED))
      goto fixture;

   fill_forced(d, MF_MEMBER_LEN, 59, 0x0BF1E001u);
   memset(&e, 0, sizeof(e));
   e.name = "cart.rd";
   e.data = d;
   e.len  = MF_MEMBER_LEN;
   e.crc  = 0x0BF1E001u;
   sprintf(p, "%s/b_cart.zip", dir);
   if (!write_zip_entries(p, &e, 1))
      goto fixture;
   set_rec(&cart, "SE Cart", 0x0BF1E001u, MF_MEMBER_LEN);
   sprintf(p, "%s/Test MF Cart.rdb", db);
   if (!write_db_set(p, &cart, 1))
      goto fixture;

   sprintf(lpl, "%s/Test MF Cart.lpl", pl_dir);
   filestream_delete(lpl);
   use_queue(threaded);
   use_db(db);
   retro_atomic_store_release_int(&hex_fails, 0);
   hex_fail = "SLUS-00152";
   started  = run_scan(dir) && scan_completed;
   hex_fail = NULL;
   fails    = retro_atomic_load_acquire_int(&hex_fails);

   sprintf(crc, "%08X|crc", (unsigned)cart.crc);
   sprintf(detail, "%d failed queries", fails);
   sprintf(what, "after an error: the next file by its CRC %s", q);
   check(   started && fails == 1 && file_contains(lpl, "SE Cart")
         && file_contains(lpl, crc) && !file_contains(lpl, "|serial"),
         what, detail);
   return;

fixture:
   check(0, "fixture", "could not write the serial error set");
}

/* First-match mode: two archives matched only by their own CRCs, in
 * the database listed second.  The first match moves that database to
 * the front; the other archive's member pass then misses it, and its
 * whole pass must still try it rather than stop at the first-match
 * exit. */
static void first_match_whole_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static const char *const db_names[2] = { "Test FM One", "Test FM Two" };
   static uint8_t      d[2][MF_MEMBER_LEN];
   char                db[512], dir[512], p[1024], lpl[1024], what[64];
   db_rec_t            recs[2];
   zip_entry_t         e;
   struct string_list *dbs;
   unsigned            i, second;
   bool                started;
   const char         *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,  "%s/first_match_db", root);
   sprintf(dir, "%s/first_match",    root);
   path_mkdir(db);
   path_mkdir(dir);
   /* Both databases first, so the listing below is the scanner's. */
   for (i = 0; i < 2; i++)
   {
      sprintf(p, "%s/%s.rdb", db, db_names[i]);
      if (!write_db(p, "FM Other", 0x0BF200FFu, 64))
         goto fixture;
   }
   if (   !(dbs = dir_list_new(db, "rdb", false, false, false, false))
       || dbs->size != 2)
   {
      if (dbs)
         string_list_free(dbs);
      goto fixture;
   }
   second = strstr(dbs->elems[1].data, db_names[1]) ? 1 : 0;
   string_list_free(dbs);

   for (i = 0; i < 2; i++)
   {
      static const char *const names[2] = { "FM Whole 1", "FM Whole 2" };

      fill_forced(d[i], MF_MEMBER_LEN, 59 + 2 * i, 0x0BF200A1u + i);
      memset(&e, 0, sizeof(e));
      e.name = "m.rd";
      e.data = d[i];
      e.len  = MF_MEMBER_LEN;
      e.crc  = 0x0BF200A1u + i;
      sprintf(p, "%s/fm_%u.zip", dir, i + 1);
      if (   !write_zip_entries(p, &e, 1)
          || !set_whole_rec(&recs[i], names[i], p))
         goto fixture;
   }
   sprintf(p, "%s/%s.rdb", db, db_names[second]);
   if (!write_db_set(p, recs, 2))
      goto fixture;

   sprintf(lpl, "%s/%s.lpl", pl_dir, db_names[second]);
   filestream_delete(lpl);
   use_queue(threaded);
   use_db(db);
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_CUSTOM);
   manual_content_scan_set_menu_scan_db_select(
         MANUAL_CONTENT_SCAN_SELECT_DB_AUTO_FIRST_MATCH, NULL);
   started = run_scan(dir) && scan_completed;
   manual_content_scan_set_menu_scan_db_select(
         MANUAL_CONTENT_SCAN_SELECT_DB_AUTO, NULL);
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC);

   sprintf(what, "first match: both archives by their own CRC %s", q);
   check(   started && file_contains(lpl, "FM Whole 1")
         && file_contains(lpl, "FM Whole 2"), what, db_names[second]);
   return;

fixture:
   check(0, "fixture", "could not write the first-match set");
}

#define CUE_TRACK_SIZE 5000

/* No reader is sent a cue's or a gdi's track: they sort first, and
 * each prunes its tracks when the scan reaches it. */
static void cue_lane(const char *root, const char *db_dir, bool gdi)
{
   const char *sheet = gdi ? "gdi" : "cue";
   char        dir[512], p[1024], detail[64], what[64];
   unsigned    i;
   int         by_reader;
   bool        started;
   FILE       *f;

   sprintf(dir, "%s/reader_%s", root, sheet);
   path_mkdir(dir);
   for (i = 0; i < 2; i++)
   {
      sprintf(p, "%s/c%u_disc.bin", dir, i);
      if (!write_content(p, 0x0C0E0000u + i, CUE_TRACK_SIZE))
         goto fixture;
      sprintf(p, "%s/c%u_disc.%s", dir, i, sheet);
      if (!(f = fopen(p, "w")))
         goto fixture;
      if (gdi)
         fprintf(f, "1\n1 0 4 2352 c%u_disc.bin 0\n", i);
      else
         fprintf(f,
               "FILE \"c%u_disc.bin\" BINARY\n"
               "  TRACK 01 MODE1/2352\n"
               "    INDEX 01 00:00:00\n", i);
      fclose(f);
      sprintf(p, "%s/c%u_plain.rd", dir, i);
      if (!write_content(p, 0x0C0E0010u + i, 1024))
         goto fixture;
   }
   use_queue(true);
   use_db(db_dir);
   retro_atomic_store_release_int(&watch_reader, 0);
   watch_size = CUE_TRACK_SIZE;
   started    = run_scan(dir);
   watch_size = 0;
   by_reader  = retro_atomic_load_acquire_int(&watch_reader);
   sprintf(detail, "readers read %d", by_reader);
   sprintf(what, "no reader reads a %s's track", sheet);
   check(started && scan_completed && by_reader == 0, what, detail);
   return;

fixture:
   check(0, "fixture", gdi ? "could not write the gdi set"
                           : "could not write the cue set");
}

/* Exactly the harness's SCAN_READER_CUTOFF, which is inclusive. */
#define PADS_SIZE 65536

/* Every small file goes to the readers, also past the first window's
 * worth; on the regular queue none starts and the scan reads them. */
static void pads_lane(const char *root, bool threaded)
{
   char        dir[512], p[1024], detail[64], what[80];
   unsigned    i, n = 2 * reader_window();
   int         by_scan, by_reader, tried;
   bool        started, ok;
   const char *q = threaded ? "(threaded)" : "(regular)";

   sprintf(dir, "%s/reader_pads", root);
   path_mkdir(dir);
   for (i = 0; i < n; i++)
   {
      sprintf(p, "%s/p_%02u.rd", dir, i);
      if (!write_content(p, 0x0BAD0000u + i, PADS_SIZE))
      { check(0, "fixture", "crc forcing failed"); return; }
   }
   use_queue(threaded);
   retro_atomic_store_release_int(&watch_scan,   0);
   retro_atomic_store_release_int(&watch_reader, 0);
   retro_atomic_store_release_int(&pushes_tried, 0);
   watch_size = PADS_SIZE;
   started    = run_scan(dir);
   watch_size = 0;
   by_scan    = retro_atomic_load_acquire_int(&watch_scan);
   by_reader  = retro_atomic_load_acquire_int(&watch_reader);
   tried      = retro_atomic_load_acquire_int(&pushes_tried);
   if (threaded && readers_expected())
      ok = by_scan == 0 && by_reader > 0;
   else
      ok = by_reader == 0 && by_scan > 0 && tried == 0;
   sprintf(detail, "scan %d, readers %d, pushes %d", by_scan, by_reader,
         tried);
   sprintf(what, threaded ? "small files read by the readers %s"
                          : "no readers %s", q);
   check(started && scan_completed && ok, what, detail);
}

#define SWITCH_FILES      120
#define SWITCH_STALL_USEC 2000000

static unsigned file_count(const char *path, const char *needle)
{
   int64_t     len  = 0;
   char       *data = NULL;
   const char *s;
   unsigned    n    = 0;

   if (!filestream_read_file(path, (void**)&data, &len) || !data)
      return 0;
   for (s = data; (s = strstr(s, needle)); s += strlen(needle))
      n++;
   free(data);
   return n;
}

/* As run_scan(), switching the queue between its runners every third
 * check, except that from each start of the threaded runner to the
 * next move of the scan's progress it asks for none: a switch away
 * ends every park, so only there can a lost wake stall the scan.  A
 * stay over SWITCH_STALL_USEC counts in *stalls and ends the stays.
 * *switches counts the checks asked for the threaded runner that
 * started a worker.  Returns the longest stay in microseconds. */
static retro_time_t run_scan_switching(const char *dir, bool *started,
      unsigned *stalls, unsigned *switches)
{
#ifdef HAVE_THREADS
   task_finder_data_t finder;
   retro_time_t       now, stay = 0, longest = 0;
   unsigned           ticks    = 0;
   unsigned           starts;
   int                progress = 0;
   bool               threaded = true;
   bool               held     = true;
   bool               found, moved, asked;
#endif
   time_t             begun;

   loop_active    = true;
   scan_completed = false;
   *stalls        = 0;
   *switches      = 0;
   if (!(*started = task_push_dbscan(dir, scan_cb)))
      return 0;
#ifdef HAVE_THREADS
   finder.func     = find_scan;
   finder.userdata = NULL;
   stay            = cpu_features_get_time_usec();
#endif
   begun = time(NULL);
   while (loop_active)
   {
#ifdef HAVE_THREADS
      found = task_queue_find(&finder);
      now   = cpu_features_get_time_usec();
      asked = false;
      if (held)
      {
         moved = found && lane_progress != progress;
         if (moved || now - stay > SWITCH_STALL_USEC)
         {
            if (!moved)
               (*stalls)++;
            if (now - stay > longest)
               longest = now - stay;
            held     = false;
            threaded = false;
            ticks    = 0;
            task_queue_unset_threaded();
         }
      }
      else if (++ticks % 3 == 0)
      {
         threaded = !threaded;
         if (threaded)
         {
            task_queue_set_threaded();
            asked = true;
         }
         else
            task_queue_unset_threaded();
      }
      starts = worker_starts;
#endif
      task_queue_check();
#ifdef HAVE_THREADS
      if (asked && worker_starts != starts)
         (*switches)++;
      if (worker_starts != starts && !*stalls)
      {
         held     = true;
         stay     = now;
         if (found)
            progress = lane_progress;
      }
#endif
      if (difftime(time(NULL), begun) > SCAN_TIMEOUT_SECONDS)
         break;
      retro_sleep(1);
   }
#ifdef HAVE_THREADS
   return longest;
#else
   return 0;
#endif
}

/* The queue switches between its runners under a scan that starts
 * with readers: the readers leave at the first switch away, none reads
 * on the main thread, and the scan reads the rest itself. */
static void switch_lane(const char *root, const char *pl_dir)
{
   db_rec_t *recs  = (db_rec_t*)calloc(SWITCH_FILES, sizeof(*recs));
   char     *names = (char*)calloc(SWITCH_FILES, 16);
   char      db[512], dir[512], p[1024], lpl[1024], detail[64];
   unsigned  i, n, stalls = 0, switches = 0;
   int       inline_reads;
   retro_time_t longest;
   bool      started = false;

   if (!recs || !names)
   {
      check(0, "fixture", "out of memory");
      goto end;
   }
   sprintf(db,  "%s/reader_switch_db", root);
   sprintf(dir, "%s/reader_switch",    root);
   path_mkdir(db);
   path_mkdir(dir);
   for (i = 0; i < SWITCH_FILES; i++)
   {
      sprintf(names + i * 16, "Switch %03u", i);
      set_rec(&recs[i], names + i * 16, 0x5A000000u + i, 1024);
      sprintf(p, "%s/s_%03u.rd", dir, i);
      if (!write_content(p, recs[i].crc, 1024))
      { check(0, "fixture", "crc forcing failed"); goto end; }
   }
   sprintf(p, "%s/Test Switch.rdb", db);
   if (!write_db_set(p, recs, SWITCH_FILES))
   { check(0, "fixture", "could not write database"); goto end; }
   sprintf(lpl, "%s/Test Switch.lpl", pl_dir);
   filestream_delete(lpl);

#ifdef HAVE_THREADS
   worker_learn = true;
#endif
   use_queue(true);
#ifdef HAVE_THREADS
   worker_learn = false;
#endif
   strlcpy(config_get_ptr()->paths.path_content_database, db,
         sizeof(config_get_ptr()->paths.path_content_database));
   slow_reset(1024, 5);
   retro_atomic_store_release_int(&watch_inline, 0);
   watch_size   = 1024;
   longest      = run_scan_switching(dir, &started, &stalls, &switches);
   watch_size   = 0;
   slow_size    = 0;
   inline_reads = retro_atomic_load_acquire_int(&watch_inline);
   use_queue(false);

   n = file_count(lpl, "|crc");
   check(started && scan_completed, "scan across queue switches completed",
         scan_completed ? "callback fired" : "timed out");
   sprintf(detail, "%u of %u", n, (unsigned)SWITCH_FILES);
   check(n == SWITCH_FILES, "every entry across queue switches", detail);
   sprintf(detail, "%d reads", inline_reads);
   check(inline_reads == 0, "no reader reads on the main thread", detail);
#ifdef HAVE_THREADS
   sprintf(detail, "%u switches to threaded", switches);
   check(switches > 0, "the queue switched under the scan", detail);
   sprintf(detail, "%u stalled, longest %u ms", stalls,
         (unsigned)(longest / 1000));
   check(!stalls, "the threaded queue moves the scan in 2 s", detail);
#else
   (void)longest;
   check(1, "the queue switched under the scan", "no threads");
#endif

end:
   free(recs);
   free(names);
}

/* The queue switches away and straight back while a reader is stuck
 * in a read: the switch leaves it behind after 1 s and never calls it
 * again, so its cleanup hands the file back and drops its share when
 * the read returns (LeakSanitizer sees what it would keep), and the
 * scan finishes with every entry. */
static void switch_stuck_lane(const char *root, const char *pl_dir,
      const char *db_dir)
{
   char   dir[512], lpl[1024];
   time_t started;
   bool   switched = false, back;

   if (!write_stuck_files(dir, root))
      return;
   if (!readers_expected())
   {
      check(1, "a switch past a stuck read: scan completes", "no readers");
      check(1, "a switch past a stuck read: every entry", "no readers");
      return;
   }
   sprintf(lpl, "%s/Test Reader.lpl", pl_dir);
   filestream_delete(lpl);
   use_queue(true);
   use_db(db_dir);
   slow_reset(READER_STUCK_SIZE, READER_STUCK_MS);
   loop_active    = true;
   scan_completed = false;
   if (!task_push_dbscan(dir, scan_cb))
   {
      check(0, "a switch past a stuck read: scan completes",
            "task_push_dbscan refused");
      return;
   }
   started = time(NULL);
   while (loop_active)
   {
      if (!switched && retro_atomic_load_acquire_int(&slow_in) > 0)
      {
         switched = true;
         task_queue_unset_threaded();
         task_queue_check();
         task_queue_set_threaded();
      }
      task_queue_check();
      if (difftime(time(NULL), started) > SCAN_TIMEOUT_SECONDS)
         break;
      retro_sleep(1);
   }
   back      = slow_reads_back();
   slow_size = 0;
   check(started && scan_completed && switched && back,
         "a switch past a stuck read: scan completes",
         scan_completed ? "callback fired" : "timed out");
   check(reader_entries(pl_dir), "a switch past a stuck read: every entry",
         "four entries");
   use_queue(false);
}

/* The list done, the scan stops its readers before M3U and END. */
static void reader_list_end_lane(const char *root, const char *db_dir)
{
   char dir[512];
   int  out;
   bool started;

   if (!readers_expected())
   {
      check(1, "readers leave once the list is done", "no readers");
      return;
   }
   if (!write_match_files(dir, root, "reader_list_end"))
      return;
   use_queue(true);
   use_db(db_dir);
   retro_atomic_store_release_int(&end_readers_out, -1);
   end_hold = true;
   started  = run_scan(dir);
   end_hold = false;
   out      = retro_atomic_load_acquire_int(&end_readers_out);
   check(started && scan_completed && out == 1,
         "readers leave once the list is done",
         out < 0 ? "no END step" : out ? "gone at the END step"
                                       : "still there at the END step");
}

/* Readers take smb:// paths when an SMB backend is built, no other
 * scheme's, and none without one. */
static void share_gate_lane(const char *root, const char *db_dir)
{
   static const char *const paths[] = {
      "smb://host/share/s_0.zip",
      "smb://host/share/s_1.zip",
      "nfs://host/share/n_0.zip",
      "ftp://host/share/f_0.zip"
   };
   char dir[512], detail[64];
   int  smb, other;
   bool started;

   if (!readers_expected())
   {
      check(1, "readers' network paths", "no readers");
      return;
   }
   if (!write_match_files(dir, root, "share_gate"))
      return;
   use_queue(true);
   use_db(db_dir);
   retro_atomic_store_release_int(&share_reads[0], 0);
   retro_atomic_store_release_int(&share_reads[1], 0);
   share_paths = paths;
   share_count = ARRAY_SIZE(paths);
   started     = run_scan(dir) && scan_completed;
   share_paths = NULL;
   smb         = retro_atomic_load_acquire_int(&share_reads[0]);
   other       = retro_atomic_load_acquire_int(&share_reads[1]);
   sprintf(detail, "%d smb://, %d other", smb, other);
#ifdef HAVE_SMBCLIENT
   check(started && smb > 0 && !other,
         "readers take smb:// paths, no other scheme's", detail);
#else
   check(started && !smb && !other,
         "readers take no network path without SMB", detail);
#endif
}

int main(int argc, char **argv)
{
   const char *root = (argc > 1) ? argv[1] : "/tmp";
   char db_dir[512], core_dir[512], info_dir[512];
   char in_dir[512], pl_dir[512], p[1024];
   bool cache_supported = false;
   /* Alpha lives in the first database, Beta in the second, so the
    * second match promotes a database that was not already at the
    * front. */
   const uint32_t crc_a = 0xA1B2C3D4u, crc_b = 0x5E6F7A8Bu;
   const uint32_t sz_a  = 4096,        sz_b  = 8192;
   /* A cue sheet does not carry the checksum itself; the scanner
    * parses it, finds the track it names and checksums that.  This
    * is the only coverage of that path. */
   const uint32_t crc_c = 0x0C0FFEE0u;
   const uint32_t sz_c  = 6144;
   /* Inside an archive: the CRC its directory stores for the member,
    * which the scanner matches without expanding the archive. */
   const uint32_t crc_d = 0xD15CD15Cu;
   const uint32_t sz_d  = 2048;
   /* Claimed by a second core that supports only ".gam", so the
    * databases' claims differ and have to stay paired with their
    * database when a match moves it to the front of the list. */
   const uint32_t crc_g = 0x6A3A6A3Au;
   const uint32_t sz_g  = 3072;

   setvbuf(stdout, NULL, _IONBF, 0);
#ifdef HAVE_THREADS
   main_tid = sthread_get_current_thread_id();
#endif
   window_open_hook = scan_tick_open;
   crc_init();
   verbosity_disable();

   printf("scanner end-to-end test (dir: %s)\n\n", root);

   sprintf(db_dir,   "%s/scan_db",       root);
   sprintf(core_dir, "%s/scan_cores",    root);
   sprintf(info_dir, "%s/scan_info",     root);
   sprintf(in_dir,   "%s/scan_content",  root);
   sprintf(pl_dir,   "%s/scan_playlist", root);
   path_mkdir(db_dir); path_mkdir(core_dir); path_mkdir(info_dir);
   path_mkdir(in_dir); path_mkdir(pl_dir);

   sprintf(p, "%s/Test Alpha.rdb", db_dir);
   if (!write_db(p, "Alpha The Game", crc_a, sz_a))
   { check(0, "fixture", "could not write database"); return 1; }
   sprintf(p, "%s/Test Beta.rdb", db_dir);
   if (!write_db(p, "Beta The Game", crc_b, sz_b))
   { check(0, "fixture", "could not write database"); return 1; }
   sprintf(p, "%s/Test Disc.rdb", db_dir);
   if (!write_db(p, "Disc The Game", crc_c, sz_c))
   { check(0, "fixture", "could not write database"); return 1; }
   sprintf(p, "%s/Test Zip.rdb", db_dir);
   if (!write_db(p, "Zipped The Game", crc_d, sz_d))
   { check(0, "fixture", "could not write database"); return 1; }
   sprintf(p, "%s/Test Gamma.rdb", db_dir);
   if (!write_db(p, "Gamma The Game", crc_g, sz_g))
   { check(0, "fixture", "could not write database"); return 1; }

   /* A ".gam" file carrying the Gamma record, and one carrying the
    * Alpha record: no core claiming Test Alpha supports ".gam", so
    * the second must not land anywhere. */
   sprintf(p, "%s/05_gamma.gam", in_dir);
   if (!write_content(p, crc_g, sz_g))
   { check(0, "fixture", "crc forcing failed"); return 1; }
   sprintf(p, "%s/06_cross.gam", in_dir);
   if (!write_content(p, crc_a, sz_a))
   { check(0, "fixture", "crc forcing failed"); return 1; }

   sprintf(p, "%s/02_alpha.bin", in_dir);
   if (!write_content(p, crc_a, sz_a))
   { check(0, "fixture", "crc forcing failed"); return 1; }
   sprintf(p, "%s/01_beta.bin", in_dir);
   if (!write_content(p, crc_b, sz_b))
   { check(0, "fixture", "crc forcing failed"); return 1; }
   /* The track the cue names, and the sheet itself.  The scanner
    * reads the sheet, resolves the track beside it and checksums
    * that, so only the track carries the matching crc. */
   sprintf(p, "%s/03_disc.bin", in_dir);
   if (!write_content(p, crc_c, sz_c))
   { check(0, "fixture", "crc forcing failed"); return 1; }
   sprintf(p, "%s/03_disc.cue", in_dir);
   {
      FILE *f = fopen(p, "w");
      if (!f)
      { check(0, "fixture", "could not write cue"); return 1; }
      fprintf(f,
            "FILE \"03_disc.bin\" BINARY\n"
            "  TRACK 01 MODE1/2352\n"
            "    INDEX 01 00:00:00\n");
      fclose(f);
   }

   /* The archive itself checksums to something else entirely, so a
    * match can only come from its member's stored CRC. */
   {
      uint8_t *d = (uint8_t*)malloc(sz_d);
      uint32_t i;
      for (i = 0; i < sz_d - 4; i++)
         d[i] = (uint8_t)((i * 53 + 7) & 0xFF);
      crc_force(d, sz_d - 4, crc_d, d + sz_d - 4);
      sprintf(p, "%s/04_bundle.zip", in_dir);
      if (!write_zip(p, "inner.bin", d, sz_d, crc_d))
      { free(d); check(0, "fixture", "could not write zip"); return 1; }
      free(d);
   }

   check(1, "content forced to the databases' crcs",
         "alpha, beta, a cue track and an archive member");

   /* The scanner skips any database no installed core claims, so the
    * core info has to name both databases as well as the extension -
    * see core_info_database_claim().  Without the
    * database line the scan reports no match for content whose crc is
    * certainly present, which is easy to mistake for a lookup bug. */
   sprintf(p, "%s/test_libretro.info", info_dir);
   {
      FILE *f = fopen(p, "w");
      if (!f)
      { check(0, "fixture", "could not write core info"); return 1; }
      fprintf(f,
            "display_name = \"Scan Test\"\n"
            "corename = \"ScanTest\"\n"
            "supported_extensions = \"bin|cue|gdi|zip\"\n"
            "database = \"Test Alpha|Test Beta|Test Disc|Test Zip\"\n");
      fclose(f);
   }
   sprintf(p, "%s/test_libretro.so", core_dir);
   { FILE *f = fopen(p, "wb"); if (f) { fputs("\177ELF", f); fclose(f); } }
   sprintf(p, "%s/gamma_libretro.info", info_dir);
   {
      FILE *f = fopen(p, "w");
      if (!f)
      { check(0, "fixture", "could not write core info"); return 1; }
      fprintf(f,
            "display_name = \"Gamma Test\"\n"
            "corename = \"GammaTest\"\n"
            "supported_extensions = \"gam\"\n"
            "database = \"Test Gamma\"\n");
      fclose(f);
   }
   sprintf(p, "%s/gamma_libretro.so", core_dir);
   { FILE *f = fopen(p, "wb"); if (f) { fputs("\177ELF", f); fclose(f); } }
   sprintf(p, "%s/ps1_libretro.info", info_dir);
   {
      FILE *f = fopen(p, "w");
      if (!f)
      { check(0, "fixture", "could not write core info"); return 1; }
      fprintf(f,
            "display_name = \"PS1 Test\"\n"
            "corename = \"PS1Test\"\n"
            "supported_extensions = \"cue\"\n"
            "database = \"Test PS1\"\n");
      fclose(f);
   }
   sprintf(p, "%s/ps1_libretro.so", core_dir);
   { FILE *f = fopen(p, "wb"); if (f) { fputs("\177ELF", f); fclose(f); } }
   sprintf(p, "%s/ctr_libretro.info", info_dir);
   {
      FILE *f = fopen(p, "w");
      if (!f)
      { check(0, "fixture", "could not write core info"); return 1; }
      fprintf(f,
            "display_name = \"3DS Test\"\n"
            "corename = \"CtrTest\"\n"
            "supported_extensions = \"3ds|cci|cxi\"\n"
            "database = \"Test 3DS|Test 3DS Strict|Walk Mobile|Walk Other|"
            "Walk Serial\"\n");
      fclose(f);
   }
   sprintf(p, "%s/ctr_libretro.so", core_dir);
   { FILE *f = fopen(p, "wb"); if (f) { fputs("\177ELF", f); fclose(f); } }
   sprintf(p, "%s/reader_libretro.info", info_dir);
   {
      FILE *f = fopen(p, "w");
      if (!f)
      { check(0, "fixture", "could not write core info"); return 1; }
      fprintf(f,
            "display_name = \"Reader Test\"\n"
            "corename = \"ReaderTest\"\n"
            "supported_extensions = \"rd|zip|cue|bin\"\n"
            "database = \"Test Reader|Test Order One|Test Order Two|Test Mix|Test Switch|"
            "Test MF Cart|Test MF Arcade|Test FM One|Test FM Two|"
            "Test EM Cart|Test EM Arcade\"\n");
      fclose(f);
   }
   sprintf(p, "%s/reader_libretro.so", core_dir);
   { FILE *f = fopen(p, "wb"); if (f) { fputs("\177ELF", f); fclose(f); } }
   sprintf(p, "%s/archive_libretro.info", info_dir);
   {
      FILE *f = fopen(p, "w");
      if (!f)
      { check(0, "fixture", "could not write core info"); return 1; }
      fprintf(f,
            "display_name = \"Archive Test\"\n"
            "corename = \"ArchiveTest\"\n"
            "supported_extensions = \"rd|txt|7z|zip|rar\"\n"
            "database = \"Test Archives\"\n");
      fclose(f);
   }
   sprintf(p, "%s/archive_libretro.so", core_dir);
   { FILE *f = fopen(p, "wb"); if (f) { fputs("\177ELF", f); fclose(f); } }
   /* Cartridge cores list their ROMs' extensions, most without zip;
    * arcade cores list zip. */
   {
      static const char *const gate[3][3] = {
         { "cartgate", "ct",     "Test Cart Gate" },
         { "cartzip",  "cz|zip", "Test Cart Zip"  },
         { "arcgate",  "zip",    "Test Arc Gate"  } };
      unsigned g;

      for (g = 0; g < 3; g++)
      {
         FILE *f;

         sprintf(p, "%s/%s_libretro.info", info_dir, gate[g][0]);
         if (!(f = fopen(p, "w")))
         { check(0, "fixture", "could not write core info"); return 1; }
         fprintf(f,
               "display_name = \"%s\"\n"
               "corename = \"%s\"\n"
               "supported_extensions = \"%s\"\n"
               "database = \"%s\"\n",
               gate[g][0], gate[g][0], gate[g][1], gate[g][2]);
         fclose(f);
         sprintf(p, "%s/%s_libretro.so", core_dir, gate[g][0]);
         if ((f = fopen(p, "wb"))) { fputs("\177ELF", f); fclose(f); }
      }
   }
   sprintf(p, "%s/esarcade_libretro.info", info_dir);
   {
      FILE *f = fopen(p, "w");
      if (!f)
      { check(0, "fixture", "could not write core info"); return 1; }
      fprintf(f,
            "display_name = \"ES Arcade Test\"\n"
            "corename = \"EsArcadeTest\"\n"
            "supported_extensions = \"rd|zip\"\n"
            "database = \"Test ES Arcade\"\n"
            "database_match_archive_member = \"true\"\n");
      fclose(f);
   }
   sprintf(p, "%s/esarcade_libretro.so", core_dir);
   { FILE *f = fopen(p, "wb"); if (f) { fputs("\177ELF", f); fclose(f); } }
   sprintf(p, "%s/member_libretro.info", info_dir);
   {
      FILE *f = fopen(p, "w");
      if (!f)
      { check(0, "fixture", "could not write core info"); return 1; }
      fprintf(f,
            "display_name = \"Member Test\"\n"
            "corename = \"MemberTest\"\n"
            "supported_extensions = \"rd\"\n"
            "database = \"Test EM Member|Test ES Disc\"\n");
      fclose(f);
   }
   sprintf(p, "%s/member_libretro.so", core_dir);
   { FILE *f = fopen(p, "wb"); if (f) { fputs("\177ELF", f); fclose(f); } }
   sprintf(p, "%s/rarcart_libretro.info", info_dir);
   {
      FILE *f = fopen(p, "w");
      if (!f)
      { check(0, "fixture", "could not write core info"); return 1; }
      fprintf(f,
            "display_name = \"RAR Cart Test\"\n"
            "corename = \"RarCartTest\"\n"
            "supported_extensions = \"rd|rar\"\n"
            "database = \"Test RAR Cart\"\n");
      fclose(f);
   }
   sprintf(p, "%s/rarcart_libretro.so", core_dir);
   { FILE *f = fopen(p, "wb"); if (f) { fputs("\177ELF", f); fclose(f); } }

#ifdef HAVE_THREADS
   task_queue_init(true, msgq_push);
#else
   task_queue_init(false, msgq_push);
#endif
   core_info_init_list(info_dir, core_dir, "so", true, false,
         &cache_supported);

   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
   strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
         sizeof(config_get_ptr()->paths.path_content_database));

   /* Without this the scanner never expands an archive that matches
    * nothing, and the entries inside are invisible. */
   {
      bool *search_archives = manual_content_scan_get_search_archives_ptr();
      if (search_archives)
         *search_archives = true;
   }

   if (!manual_content_scan_set_menu_system_name(
            MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR, NULL))
      check(0, "system name", "could not be set");

   if (!run_scan(in_dir))
   {
      check(0, "scan started", "task_push_dbscan refused");
      goto done;
   }
   check(scan_completed, "scan ran to completion",
         scan_completed ? "callback fired" : "timed out");

   /* Each game must appear in its own database's playlist.  Getting
    * this wrong is not a crash - the entry is a real record from a
    * real database, just the wrong one. */
   sprintf(p, "%s/Test Alpha.lpl", pl_dir);
   check(file_contains(p, "Alpha The Game"),
         "first database's game in its playlist", "Alpha The Game");
   check(!file_contains(p, "Beta The Game"),
         "and not the other database's game", "no Beta in Alpha");

   sprintf(p, "%s/Test Beta.lpl", pl_dir);
   check(file_contains(p, "Beta The Game"),
         "promoted database's game in its playlist", "Beta The Game");
   check(!file_contains(p, "Alpha The Game"),
         "and not the other database's game", "no Alpha in Beta");

   /* The cue is the entry that lands in the playlist, not the track
    * it names: the scanner checksums the track but records the sheet,
    * which is what a frontend would load. */
   sprintf(p, "%s/Test Zip.lpl", pl_dir);
   check(file_contains(p, "Zipped The Game"),
         "archive member matched", "Zipped The Game");
   /* The member's stored CRC is what matches, but the entry records
    * the archive: that is the path a frontend loads. */
   check(   file_contains(p, "04_bundle.zip")
         && !file_contains(p, "#inner.bin"),
         "entry records the archive, not the member", "04_bundle.zip");

   sprintf(p, "%s/Test Disc.lpl", pl_dir);
   check(file_contains(p, "Disc The Game"),
         "cue resolved through its track", "Disc The Game");
   check(file_contains(p, ".cue"),
         "playlist records the sheet, not the track", "path ends .cue");

   /* Test Gamma is visited after four databases have been moved to
    * the front of the list; its claim has to have travelled with it. */
   sprintf(p, "%s/Test Gamma.lpl", pl_dir);
   check(file_contains(p, "05_gamma.gam"),
         "database claimed by a second core matched", "05_gamma.gam");
   sprintf(p, "%s/Test Alpha.lpl", pl_dir);
   check(!file_contains(p, "06_cross.gam"),
         "extension of a core not claiming the database refused",
         "no 06_cross.gam in Alpha");

   /* PS1 discs match on the serial their data track carries.  A root
    * directory listing PSX.EXE ahead of the executable the disc boots
    * must not end the search.  A disc that boots PSX.EXE, as the
    * SYSTEM.CNF BOOT target or for want of a SYSTEM.CNF, has no serial
    * whatever else the track holds, so the crc decides. */
   {
      static const db_rec_t ps1_recs[] = {
         { "Serial The Game",   "SLUS-00152", 0x5E1A0152u, PS1_TRACK_SIZE },
         { "Short Track Game",  "SLUS-00777", 0x5E1A0777u, PS1_SHORT_SIZE },
         { "PSX Boot The Game", NULL,         0x0B0071E5u, PS1_TRACK_SIZE },
         { "Decoy Boot Game",   "SLUS-00998", 0xDEC0DEC3u, PS1_TRACK_SIZE },
         { "PSX Boot Two",      NULL,         0x0B0071E6u, PS1_TRACK_SIZE },
         { "Decoy Serial Game", "SLUS-00999", 0xDEC0DEC0u, PS1_TRACK_SIZE },
         { "PSX No CNF Game",   NULL,         0x0B0071E7u, PS1_TRACK_SIZE },
         { "Decoy No CNF Game", "SLUS-01010", 0xDEC0DEC1u, PS1_TRACK_SIZE },
         { "PSX Lower Game",    NULL,         0x0B0071E8u, PS1_TRACK_SIZE },
         { "Decoy Lower Game",  "SLUS-01111", 0xDEC0DEC2u, PS1_TRACK_SIZE }
      };
      /* Where Tomb Raider (USA) has them: the root directory in sector
       * 22, SYSTEM.CNF in sector 23. */
      static const char *const serial_str[] = {
         "PSX.EXE;1", "SLUS_001.52;1", "SYSTEM.CNF;1",
         "BOOT = cdrom:\\SLUS_001.52;1"
      };
      static const unsigned serial_at[] = { 52407, 52517, 52577, 54120 };
      /* The same, on a track that ends inside the probe window. */
      static const char *const short_str[] = {
         "PSX.EXE;1", "SLUS_007.77;1", "SYSTEM.CNF;1",
         "BOOT = cdrom:\\SLUS_007.77;1"
      };
      /* No backslash after "cdrom:"; a serial-like record listed. */
      static const char *const boot_str[] = {
         "PSX.EXE;1", "SLUS_009.98;1", "SYSTEM.CNF;1",
         "BOOT=cdrom:PSX.EXE;1"
      };
      static const unsigned boot_at[]   = { 52407, 52517, 52577, 54120 };
      /* A serial-like record listed between PSX.EXE and SYSTEM.CNF. */
      static const char *const decoy_str[] = {
         "PSX.EXE;1", "SLUS_009.99;1", "SYSTEM.CNF;1",
         "BOOT = cdrom:\\PSX.EXE;1"
      };
      static const unsigned decoy_at[]  = { 52407, 52517, 52577, 54120 };
      /* No SYSTEM.CNF, so the BIOS boots PSX.EXE. */
      static const char *const nocnf_str[] = {
         "PSX.EXE;1", "SLUS_010.10;1"
      };
      static const unsigned nocnf_at[]  = { 52407, 52517 };
      static const char *const lower_str[] = {
         "PSX.EXE;1", "SLUS_011.11;1", "SYSTEM.CNF;1",
         "BOOT = cdrom:\\psx.exe;1"
      };
      static const unsigned lower_at[]  = { 52407, 52517, 52577, 54120 };
      char ps1_db[512], ps1_in[512];

      sprintf(ps1_db, "%s/scan_db_ps1", root);
      sprintf(ps1_in, "%s/scan_ps1",    root);
      path_mkdir(ps1_db);
      path_mkdir(ps1_in);

      sprintf(p, "%s/Test PS1.rdb", ps1_db);
      if (!write_db_set(p, ps1_recs, ARRAY_SIZE(ps1_recs)))
      { check(0, "fixture", "could not write database"); goto done; }
      sprintf(p, "%s/07_serial.bin", ps1_in);
      if (!write_ps1_track(p, serial_str, serial_at,
            ARRAY_SIZE(serial_str), 0x07070707u))
      { check(0, "fixture", "could not write track"); goto done; }
      sprintf(p, "%s/07_serial.cue", ps1_in);
      if (!write_cue(p, "07_serial.bin"))
      { check(0, "fixture", "could not write cue"); goto done; }
      sprintf(p, "%s/12_ps1_short.bin", ps1_in);
      if (!write_ps1_track_sized(p, PS1_SHORT_SIZE, short_str, serial_at,
            ARRAY_SIZE(short_str), 0x12121212u))
      { check(0, "fixture", "could not write track"); goto done; }
      sprintf(p, "%s/12_ps1_short.cue", ps1_in);
      if (!write_cue(p, "12_ps1_short.bin"))
      { check(0, "fixture", "could not write cue"); goto done; }
      sprintf(p, "%s/08_psx_boot.bin", ps1_in);
      if (!write_ps1_track(p, boot_str, boot_at,
            ARRAY_SIZE(boot_str), 0x0B0071E5u))
      { check(0, "fixture", "could not write track"); goto done; }
      sprintf(p, "%s/08_psx_boot.cue", ps1_in);
      if (!write_cue(p, "08_psx_boot.bin"))
      { check(0, "fixture", "could not write cue"); goto done; }
      sprintf(p, "%s/09_psx_decoy.bin", ps1_in);
      if (!write_ps1_track(p, decoy_str, decoy_at,
            ARRAY_SIZE(decoy_str), 0x0B0071E6u))
      { check(0, "fixture", "could not write track"); goto done; }
      sprintf(p, "%s/09_psx_decoy.cue", ps1_in);
      if (!write_cue(p, "09_psx_decoy.bin"))
      { check(0, "fixture", "could not write cue"); goto done; }
      sprintf(p, "%s/10_psx_nocnf.bin", ps1_in);
      if (!write_ps1_track(p, nocnf_str, nocnf_at,
            ARRAY_SIZE(nocnf_str), 0x0B0071E7u))
      { check(0, "fixture", "could not write track"); goto done; }
      sprintf(p, "%s/10_psx_nocnf.cue", ps1_in);
      if (!write_cue(p, "10_psx_nocnf.bin"))
      { check(0, "fixture", "could not write cue"); goto done; }
      sprintf(p, "%s/11_psx_lower.bin", ps1_in);
      if (!write_ps1_track(p, lower_str, lower_at,
            ARRAY_SIZE(lower_str), 0x0B0071E8u))
      { check(0, "fixture", "could not write track"); goto done; }
      sprintf(p, "%s/11_psx_lower.cue", ps1_in);
      if (!write_cue(p, "11_psx_lower.bin"))
      { check(0, "fixture", "could not write cue"); goto done; }

      strlcpy(config_get_ptr()->paths.path_content_database, ps1_db,
            sizeof(config_get_ptr()->paths.path_content_database));
      if (!run_scan(ps1_in))
      {
         check(0, "PS1 scan started", "task_push_dbscan refused");
         goto done;
      }
      strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
            sizeof(config_get_ptr()->paths.path_content_database));
      check(scan_completed, "PS1 scan completed",
            scan_completed ? "callback fired" : "timed out");

      sprintf(p, "%s/Test PS1.lpl", pl_dir);
      check(   file_contains(p, "Serial The Game")
            && file_contains(p, "SLUS-00152|serial"),
            "PS1 serial found past a listed PSX.EXE", "SLUS-00152|serial");
      check(   file_contains(p, "Short Track Game")
            && file_contains(p, "SLUS-00777|serial"),
            "PS1 serial found on a track shorter than the probe",
            "SLUS-00777|serial");
      check(   file_contains(p, "PSX Boot The Game")
            && file_contains(p, "0B0071E5|crc")
            && !file_contains(p, "Decoy Boot Game"),
            "PSX.EXE as BOOT target falls to the crc", "0B0071E5|crc");
      check(   file_contains(p, "PSX Boot Two")
            && file_contains(p, "0B0071E6|crc")
            && !file_contains(p, "Decoy Serial Game"),
            "listed serial ignored when PSX.EXE boots", "0B0071E6|crc");
      check(   file_contains(p, "PSX No CNF Game")
            && file_contains(p, "0B0071E7|crc")
            && !file_contains(p, "Decoy No CNF Game"),
            "no SYSTEM.CNF: PSX.EXE falls to the crc", "0B0071E7|crc");
      check(   file_contains(p, "PSX Lower Game")
            && file_contains(p, "0B0071E8|crc")
            && !file_contains(p, "Decoy Lower Game"),
            "lowercase PSX.EXE BOOT falls to the crc", "0B0071E8|crc");
   }

   /* 3DS images match on the product code in their NCCH header.  A
    * code several records share goes to the crc, and the first of them
    * stands in when the crc matches none.  A code no record carries, a
    * header without its magic, or a CIA, takes the crc path. */
   {
      static const db_rec_t ctr_recs[] = {
         { "Unique The Game",    "CTR-P-UNIQ", 0x0BAD0001u, CTR_IMAGE_SIZE },
         { "Shared Rev 0",       "CTR-P-SHRD", 0x5AD00000u, CTR_IMAGE_SIZE },
         { "Shared Rev 1",       "CTR-P-SHRD", 0x5AD00001u, CTR_IMAGE_SIZE },
         { "Fallback First",     "CTR-P-FALL", 0xFA110000u, CTR_IMAGE_SIZE },
         { "Fallback Second",    "CTR-P-FALL", 0xFA110001u, CTR_IMAGE_SIZE },
         { "Cia The Game",       NULL,         0xC1A00000u, CTR_IMAGE_SIZE },
         { "Bad Magic The Game", NULL,         0xBAD0BAD0u, CTR_IMAGE_SIZE },
         { "Bad Magic Decoy",    "CTR-P-BADM", 0xBAD00000u, CTR_IMAGE_SIZE },
         { "Bare The Game",      "CTR-P-BARE", 0x0BAD0002u, CTR_IMAGE_SIZE },
         /* LEGO Batman 2 (Canada) has its serial without the prefix. */
         { "Miss The Game",      "MISS",       0x3155C0DEu, CTR_IMAGE_SIZE },
         { "Unit Shift The Game", "CTR-P-UNIT", 0x0BAD0003u, CTR_IMAGE_SIZE },
         { "Big Shift Owner",    NULL,         0xB16B0000u, CTR_BIG_SIZE },
         { "Big Shift Decoy",    "CTR-P-BIGU", 0xB16B0001u, CTR_BIG_SIZE },
         { "Cut Header Owner",   NULL,         0xC0700000u, CTR_CUT_SIZE },
         { "Cut Header Decoy",   "CTR-P-CUTH", 0xC0700001u, CTR_CUT_SIZE },
         { "Sixteen The Game",   "CTR-P-SIXTEEN-16", 0x0BAD0004u,
                                                           CTR_IMAGE_SIZE },
         { "Del Code Owner",     NULL,         0xDE100000u, CTR_IMAGE_SIZE },
         { "Del Code Decoy",     "CTR-P-DEL\x7f", 0xDE100001u, CTR_IMAGE_SIZE }
      };
      char ctr_db[512], ctr_in[512];

      sprintf(ctr_db, "%s/scan_db_3ds", root);
      sprintf(ctr_in, "%s/scan_3ds",    root);
      path_mkdir(ctr_db);
      path_mkdir(ctr_in);

      sprintf(p, "%s/Test 3DS.rdb", ctr_db);
      if (!write_db_set(p, ctr_recs, ARRAY_SIZE(ctr_recs)))
      { check(0, "fixture", "could not write database"); goto done; }
      /* Partition 0 where carts have it, 0x4000. */
      sprintf(p, "%s/10_unique.3ds", ctr_in);
      if (!write_ctr(p, 0x20, 1, "CTR-P-UNIQ", 0x10101010u))
      { check(0, "fixture", "could not write image"); goto done; }
      /* Elsewhere, so the offset has to come from the table. */
      sprintf(p, "%s/11_shared.3ds", ctr_in);
      if (!write_ctr(p, 0x28, 1, "CTR-P-SHRD", 0x5AD00001u))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/12_fallback.3ds", ctr_in);
      if (!write_ctr(p, 0x20, 1, "CTR-P-FALL", 0x12121212u))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/13_cia.cci", ctr_in);
      if (!write_cia(p, 0xC1A00000u))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/14_bad_magic.3ds", ctr_in);
      if (!write_ctr(p, 0x20, 0, "CTR-P-BADM", 0xBAD0BAD0u))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/15_bare.cxi", ctr_in);
      if (!write_ctr(p, 0, 1, "CTR-P-BARE", 0x15151515u))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/16_miss.3ds", ctr_in);
      if (!write_ctr(p, 0x20, 1, "CTR-P-MISS", 0x3155C0DEu))
      { check(0, "fixture", "could not write image"); goto done; }
      /* Units of 0x400. */
      sprintf(p, "%s/17_unit.3ds", ctr_in);
      if (!write_ctr_ncsd(p, CTR_IMAGE_SIZE, 1, 0x10, 0x4000,
               "CTR-P-UNIT", 10, 0x17171717u))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/18_big_shift.3ds", ctr_in);
      if (!write_ctr_ncsd(p, CTR_BIG_SIZE, 16, 1, 0x2000000,
               "CTR-P-BIGU", 10, 0xB16B0000u))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/19_cut.3ds", ctr_in);
      if (!write_ctr_ncsd(p, CTR_CUT_SIZE, 0, 0x20, 0x4000,
               "CTR-P-CUTH", 10, 0xC0700000u))
      { check(0, "fixture", "could not write image"); goto done; }
      /* No NUL: the next field follows. */
      sprintf(p, "%s/1a_sixteen.3ds", ctr_in);
      if (!write_ctr_ncsd(p, CTR_IMAGE_SIZE, 0, 0x20, 0x4000,
               "CTR-P-SIXTEEN-16XYZ", 19, 0x1A1A1A1Au))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/1b_del.3ds", ctr_in);
      if (!write_ctr_ncsd(p, CTR_IMAGE_SIZE, 0, 0x20, 0x4000,
               "CTR-P-DEL\x7f", 10, 0xDE100000u))
      { check(0, "fixture", "could not write image"); goto done; }

      strlcpy(config_get_ptr()->paths.path_content_database, ctr_db,
            sizeof(config_get_ptr()->paths.path_content_database));
      if (!run_scan(ctr_in))
      {
         check(0, "3DS scan started", "task_push_dbscan refused");
         goto done;
      }
      strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
            sizeof(config_get_ptr()->paths.path_content_database));
      check(scan_completed, "3DS scan completed",
            scan_completed ? "callback fired" : "timed out");

      sprintf(p, "%s/Test 3DS.lpl", pl_dir);
      check(   file_contains(p, "Unique The Game")
            && file_contains(p, "CTR-P-UNIQ|serial"),
            "unique 3DS serial matched without the crc", "CTR-P-UNIQ|serial");
      check(   file_contains(p, "Shared Rev 1")
            && !file_contains(p, "Shared Rev 0")
            && file_contains(p, "CTR-P-SHRD|serial"),
            "shared 3DS serial resolved by the crc", "Shared Rev 1");
      check(   file_contains(p, "Fallback First")
            && !file_contains(p, "Fallback Second"),
            "shared serial, no crc match: first record", "Fallback First");
      check(   file_contains(p, "Cia The Game")
            && file_contains(p, "C1A00000|crc"),
            "CIA header in a .cci keeps the crc path", "C1A00000|crc");
      check(   file_contains(p, "Bad Magic The Game")
            && !file_contains(p, "Bad Magic Decoy"),
            "partition without NCCH magic: crc path", "BAD0BAD0|crc");
      check(   file_contains(p, "Bare The Game")
            && file_contains(p, "CTR-P-BARE|serial"),
            "bare .cxi matched on its serial", "CTR-P-BARE|serial");
      check(   file_contains(p, "Miss The Game")
            && file_contains(p, "3155C0DE|crc"),
            "serial in no record falls back to the crc", "3155C0DE|crc");
      check(   file_contains(p, "Unit Shift The Game")
            && file_contains(p, "CTR-P-UNIT|serial"),
            "partition offset in units of 0x200 << flags[6]",
            "CTR-P-UNIT|serial");
      check(   file_contains(p, "Big Shift Owner")
            && !file_contains(p, "Big Shift Decoy"),
            "flags[6] over 15: crc path", "B16B0000|crc");
      check(   file_contains(p, "Cut Header Owner")
            && !file_contains(p, "Cut Header Decoy"),
            "NCCH header cut short by the end: crc path", "C0700000|crc");
      check(   file_contains(p, "Sixteen The Game")
            && file_contains(p, "CTR-P-SIXTEEN-16|serial"),
            "16-byte code with no NUL ends at 16 bytes",
            "CTR-P-SIXTEEN-16|serial");
      check(   file_contains(p, "Del Code Owner")
            && !file_contains(p, "Del Code Decoy"),
            "code with a byte past '~': crc path", "DE100000|crc");
   }

   /* A 3DS database no installed core claims is passed over, as the crc
    * lookup passes it over, unless Scan Without Core Match is on.  Its
    * name must not start with "Test 3DS ": string_list_find_elem()
    * takes a space for the end of a string, so the test core would
    * claim it. */
   {
      static const db_rec_t uncl_recs[] = {
         { "Unclaimed The Game", "CTR-P-UNCL", 0x0C1A0001u, CTR_IMAGE_SIZE }
      };
      char uncl_db[512], uncl_in[512];

      sprintf(uncl_db, "%s/scan_db_3ds_unclaimed", root);
      sprintf(uncl_in, "%s/scan_3ds_unclaimed",    root);
      path_mkdir(uncl_db);
      path_mkdir(uncl_in);

      sprintf(p, "%s/Unclaimed 3DS.rdb", uncl_db);
      if (!write_db_set(p, uncl_recs, ARRAY_SIZE(uncl_recs)))
      { check(0, "fixture", "could not write database"); goto done; }
      sprintf(p, "%s/40_unclaimed.3ds", uncl_in);
      if (!write_ctr(p, 0x20, 1, "CTR-P-UNCL", 0x0C1A00FFu))
      { check(0, "fixture", "could not write image"); goto done; }

      strlcpy(config_get_ptr()->paths.path_content_database, uncl_db,
            sizeof(config_get_ptr()->paths.path_content_database));
      /* the playlist the scan below must not write, not a run's
       * before */
      sprintf(p, "%s/Unclaimed 3DS.lpl", pl_dir);
      filestream_delete(p);
      if (!run_scan(uncl_in))
      {
         check(0, "unclaimed 3DS scan started", "task_push_dbscan refused");
         goto done;
      }
      check(scan_completed && !path_is_valid(p),
            "3DS database no core claims passed over", "no playlist");

      filestream_delete(p);
      config_get_ptr()->bools.scan_without_core_match = true;
      if (!run_scan(uncl_in))
      {
         check(0, "unclaimed 3DS scan started", "task_push_dbscan refused");
         goto done;
      }
      config_get_ptr()->bools.scan_without_core_match = false;
      strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
            sizeof(config_get_ptr()->paths.path_content_database));
      check(   scan_completed
            && file_contains(p, "Unclaimed The Game")
            && file_contains(p, "CTR-P-UNCL|serial"),
            "Scan Without Core Match: unclaimed 3DS database",
            "CTR-P-UNCL|serial");
      filestream_delete(p);
   }

   /* scan_serial_and_crc makes the crc a requirement after a 3DS
    * serial hit, as it is for PSP and Dreamcast.  A hit the crc turns
    * down goes on to the crc lookup. */
   {
      static const db_rec_t strict_recs[] = {
         { "Strict Hit",            "CTR-P-STHT", 0x57A1C001u, CTR_IMAGE_SIZE },
         { "Strict Miss",           "CTR-P-STMS", 0x57A1C002u, CTR_IMAGE_SIZE },
         { "Strict Code Elsewhere", "CTR-P-SCEL", 0x57A1C010u, CTR_IMAGE_SIZE },
         { "Strict Crc Owner",      NULL,         0x57A1C011u, CTR_IMAGE_SIZE },
         { "Strict Shared Rev 0",   "CTR-P-SSHR", 0x57A1C020u, CTR_IMAGE_SIZE },
         { "Strict Shared Rev 1",   "CTR-P-SSHR", 0x57A1C021u, CTR_IMAGE_SIZE },
         { "Strict Shared Owner",   "CTR-P-SOWN", 0x57A1C022u, CTR_IMAGE_SIZE },
         { "Strict Nowhere Rev 0",  "CTR-P-SNOW", 0x57A1C030u, CTR_IMAGE_SIZE },
         { "Strict Nowhere Rev 1",  "CTR-P-SNOW", 0x57A1C031u, CTR_IMAGE_SIZE }
      };
      char strict_db[512], strict_in[512], detail[32];

      sprintf(strict_db, "%s/scan_db_3ds_strict", root);
      sprintf(strict_in, "%s/scan_3ds_strict",    root);
      path_mkdir(strict_db);
      path_mkdir(strict_in);

      sprintf(p, "%s/Test 3DS Strict.rdb", strict_db);
      if (!write_db_set(p, strict_recs, ARRAY_SIZE(strict_recs)))
      { check(0, "fixture", "could not write database"); goto done; }
      sprintf(p, "%s/20_strict_hit.3ds", strict_in);
      if (!write_ctr(p, 0x20, 1, "CTR-P-STHT", 0x57A1C001u))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/21_strict_miss.3ds", strict_in);
      if (!write_ctr(p, 0x20, 1, "CTR-P-STMS", 0x57A1C0FFu))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/22_strict_elsewhere.3ds", strict_in);
      if (!write_ctr(p, 0x20, 1, "CTR-P-SCEL", 0x57A1C011u))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/23_strict_shared_elsewhere.3ds", strict_in);
      if (!write_ctr(p, 0x20, 1, "CTR-P-SSHR", 0x57A1C022u))
      { check(0, "fixture", "could not write image"); goto done; }
      sprintf(p, "%s/24_strict_nowhere.3ds", strict_in);
      if (!write_ctr(p, 0x20, 1, "CTR-P-SNOW", 0x57A1C0EEu))
      { check(0, "fixture", "could not write image"); goto done; }

      config_get_ptr()->bools.scan_serial_and_crc = true;
      strlcpy(config_get_ptr()->paths.path_content_database, strict_db,
            sizeof(config_get_ptr()->paths.path_content_database));
      retro_atomic_store_release_int(&reads_begun, 0);
      begun_size = CTR_IMAGE_SIZE;
      if (!run_scan(strict_in))
      {
         check(0, "strict 3DS scan started", "task_push_dbscan refused");
         goto done;
      }
      begun_size = 0;
      config_get_ptr()->bools.scan_serial_and_crc = false;
      strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
            sizeof(config_get_ptr()->paths.path_content_database));
      check(scan_completed, "strict 3DS scan completed",
            scan_completed ? "callback fired" : "timed out");

      sprintf(p, "%s/Test 3DS Strict.lpl", pl_dir);
      check(   file_contains(p, "Strict Hit")
            && file_contains(p, "CTR-P-STHT|serial"),
            "scan_serial_and_crc: serial and crc match", "CTR-P-STHT|serial");
      check(!file_contains(p, "Strict Miss"),
            "scan_serial_and_crc: crc mismatch refused", "no Strict Miss");
      check(   file_contains(p, "Strict Crc Owner")
            && file_contains(p, "57A1C011|crc")
            && !file_contains(p, "Strict Code Elsewhere"),
            "scan_serial_and_crc: crc on another record", "57A1C011|crc");
      check(   file_contains(p, "Strict Shared Owner")
            && file_contains(p, "57A1C022|crc")
            && !file_contains(p, "Strict Shared Rev"),
            "scan_serial_and_crc: shared, crc elsewhere", "57A1C022|crc");
      check(!file_contains(p, "24_strict_nowhere"),
            "scan_serial_and_crc: shared, crc nowhere", "no 24_");
      sprintf(detail, "%d reads of 5 files",
            retro_atomic_load_acquire_int(&reads_begun));
      check(retro_atomic_load_acquire_int(&reads_begun) == 5,
            "scan_serial_and_crc: the crc lookup reads no file again",
            detail);
   }

   /* First-match selection moves a matched database to the front and
    * stops the search there.  A 3DS code no record carries must still
    * reach the crc: the first miss matches by crc and moves Test 3DS to
    * the front, and the others meet it there.  The database list is in
    * readdir order, so this bites only when Test 3DS is not listed
    * first; the pads make that so in creation, reverse creation and
    * name order.  A code the crc turns down reaches the crc too: of the
    * two such files, whichever comes second meets Test 3DS there. */
   {
      static const db_rec_t first_recs[] = {
         { "First Miss The Game", "FMIS", 0xF1157000u, CTR_IMAGE_SIZE },
         { "First Strict Code",   "CTR-P-FSTR", 0xF1157001u, CTR_IMAGE_SIZE },
         { "First Strict Owner A", NULL,  0xF1157002u, CTR_IMAGE_SIZE },
         { "First Strict Owner B", NULL,  0xF1157003u, CTR_IMAGE_SIZE }
      };
      static const char *const first_files[] = {
         "30_first_miss_a.3ds", "31_first_miss_b.3ds", "32_first_miss_c.3ds"
      };
      static const char *const strict_files[] = {
         "33_first_strict_a.3ds", "34_first_strict_b.3ds"
      };
      char     first_db[512], first_in[512];
      unsigned i;
      int      all_matched = 1;

      sprintf(first_db, "%s/scan_db_3ds_first", root);
      sprintf(first_in, "%s/scan_3ds_first",    root);
      path_mkdir(first_db);
      path_mkdir(first_in);

      for (i = 0; i < 6; i++)
      {
         if (i == 3)
         {
            sprintf(p, "%s/Test 3DS.rdb", first_db);
            if (!write_db_set(p, first_recs, ARRAY_SIZE(first_recs)))
            { check(0, "fixture", "could not write database"); goto done; }
         }
         sprintf(p, "%s/Test 3DS Pad %u.rdb", first_db, i);
         if (!write_db(p, "Pad The Game", 0x9AD00000u + i, CTR_IMAGE_SIZE))
         { check(0, "fixture", "could not write database"); goto done; }
      }
      for (i = 0; i < ARRAY_SIZE(first_files); i++)
      {
         sprintf(p, "%s/%s", first_in, first_files[i]);
         if (!write_ctr(p, 0x20, 1, "CTR-P-FMIS", 0xF1157000u))
         { check(0, "fixture", "could not write image"); goto done; }
      }
      for (i = 0; i < ARRAY_SIZE(strict_files); i++)
      {
         sprintf(p, "%s/%s", first_in, strict_files[i]);
         if (!write_ctr(p, 0x20, 1, "CTR-P-FSTR", 0xF1157002u + i))
         { check(0, "fixture", "could not write image"); goto done; }
      }
      {
         struct string_list *dbs = dir_list_new(first_db, "rdb", false,
               false, false, false);
         if (dbs && dbs->size
               && !strcmp(path_basename(dbs->elems[0].data),
                  "Test 3DS.rdb"))
            printf("  note  first match: Test 3DS lists first, so this "
                  "lane cannot bite\n");
         string_list_free(dbs);
      }

      manual_content_scan_set_menu_scan_method(
            MANUAL_CONTENT_SCAN_METHOD_CUSTOM);
      manual_content_scan_set_menu_scan_db_select(
            MANUAL_CONTENT_SCAN_SELECT_DB_AUTO_FIRST_MATCH, NULL);
      strlcpy(config_get_ptr()->paths.path_content_database, first_db,
            sizeof(config_get_ptr()->paths.path_content_database));
      config_get_ptr()->bools.scan_serial_and_crc = true;
      if (!run_scan(first_in))
      {
         check(0, "first-match 3DS scan started", "task_push_dbscan refused");
         goto done;
      }
      config_get_ptr()->bools.scan_serial_and_crc = false;
      manual_content_scan_set_menu_scan_db_select(
            MANUAL_CONTENT_SCAN_SELECT_DB_AUTO, NULL);
      manual_content_scan_set_menu_scan_method(
            MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC);
      strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
            sizeof(config_get_ptr()->paths.path_content_database));
      check(scan_completed, "first-match 3DS scan completed",
            scan_completed ? "callback fired" : "timed out");

      sprintf(p, "%s/Test 3DS.lpl", pl_dir);
      for (i = 0; i < ARRAY_SIZE(first_files); i++)
         if (!file_contains(p, first_files[i]))
            all_matched = 0;
      check(all_matched && file_contains(p, "F1157000|crc"),
            "first match: serial miss falls back to crc",
            "30_, 31_ and 32_first_miss");
      check(   file_contains(p, "33_first_strict_a.3ds")
            && file_contains(p, "34_first_strict_b.3ds")
            && file_contains(p, "F1157002|crc")
            && file_contains(p, "F1157003|crc")
            && !file_contains(p, "First Strict Code"),
            "first match: strict rejection falls back to crc",
            "33_ and 34_first_strict");
   }

   /* A strict scan hands every file to DATABASE_SCAN_ITERATE_NEXT.  32
    * files fill string_list_new()'s initial capacity, so ASan catches a
    * read past the last entry. */
   {
      char pad_dir[512];
      uint32_t i;

      sprintf(pad_dir, "%s/scan_pad", root);
      path_mkdir(pad_dir);
      for (i = 0; i < 32; i++)
      {
         sprintf(p, "%s/pad_%02u.bin", pad_dir, (unsigned)i);
         if (!write_content(p, 0x7AD00000u + i, 1024))
         { check(0, "fixture", "crc forcing failed"); goto done; }
      }

      if (!run_scan(pad_dir))
      {
         check(0, "full list scan started", "task_push_dbscan refused");
         goto done;
      }
      check(scan_completed, "scan of a full content list completed",
            scan_completed ? "callback fired" : "timed out");
   }

   /* An empty folder has no entry to start iterating from. */
   {
      char empty_dir[512];

      sprintf(empty_dir, "%s/scan_empty", root);
      path_mkdir(empty_dir);

      invalid_content_pushed = false;
      if (!run_scan(empty_dir))
      {
         check(0, "empty folder scan started", "task_push_dbscan refused");
         goto done;
      }
      check(scan_completed && invalid_content_pushed,
            "empty folder reported as invalid content",
            !scan_completed ? "timed out"
            : invalid_content_pushed ? "message pushed" : "no message");
   }

   /* A sheet that names itself is still the entry being scanned, so
    * pruning its tracks must not free it.  Only ASan sees the
    * use-after-free. */
   {
      char self_dir[512];
      FILE *f;

      sprintf(self_dir, "%s/scan_self", root);
      path_mkdir(self_dir);

      sprintf(p, "%s/self.cue", self_dir);
      if (!(f = fopen(p, "w")))
      { check(0, "fixture", "could not write cue"); goto done; }
      fprintf(f,
            "FILE \"self.cue\" BINARY\n"
            "  TRACK 01 MODE1/2352\n"
            "    INDEX 01 00:00:00\n");
      fclose(f);

      sprintf(p, "%s/self.gdi", self_dir);
      if (!(f = fopen(p, "w")))
      { check(0, "fixture", "could not write gdi"); goto done; }
      fprintf(f, "1\n1 0 4 2352 self.gdi 0\n");
      fclose(f);

      if (!run_scan(self_dir))
      {
         check(0, "self-naming sheet scan started", "task_push_dbscan refused");
         goto done;
      }
      check(scan_completed, "self-naming cue and gdi scanned",
            scan_completed ? "callback fired" : "timed out");
   }

   /* A database no core claims costs nothing per file: the step that
    * refuses one passes over every refused database, so on the
    * regular queue, which runs the handler once per check, a file
    * takes a check per database it can match plus a fixed few - not
    * one per database in the directory. */
   {
      char many_db[512], many_in[512];
      unsigned i, gathers;

      sprintf(many_db, "%s/scan_db_many", root);
      sprintf(many_in, "%s/scan_unmatched", root);
      path_mkdir(many_db);
      path_mkdir(many_in);

      sprintf(p, "%s/Test Alpha.rdb", many_db);
      if (!write_db(p, "Alpha The Game", crc_a, sz_a))
      { check(0, "fixture", "could not write database"); goto done; }
      /* Never opened: the gate refuses them before any read. */
      for (i = 0; i < UNCLAIMED_DBS; i++)
      {
         FILE *f;
         sprintf(p, "%s/Unclaimed %02u.rdb", many_db, i);
         if (!(f = fopen(p, "wb")))
         { check(0, "fixture", "could not write database"); goto done; }
         fclose(f);
      }
      for (i = 0; i < UNMATCHED_FILES; i++)
      {
         sprintf(p, "%s/none_%02u.bin", many_in, i);
         if (!write_content(p, 0x3C000000u + i, 1024))
         { check(0, "fixture", "crc forcing failed"); goto done; }
      }

      /* The scanner reads its database directory from the settings. */
      strlcpy(config_get_ptr()->paths.path_content_database, many_db,
            sizeof(config_get_ptr()->paths.path_content_database));
      task_queue_deinit();
      task_queue_init(false, msgq_push);
      gathers = run_scan_counted(many_in);
      strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
            sizeof(config_get_ptr()->paths.path_content_database));
      printf("  info  checks for %u files over %u databases: %u\n",
            UNMATCHED_FILES, UNCLAIMED_DBS + 1, gathers);
      check(scan_completed, "scan over unclaimed databases completed",
            scan_completed ? "callback fired" : "timed out");
      check(gathers && gathers < UNMATCHED_FILES * 8 + 32,
            "unclaimed databases passed over in one step",
            "checks bounded by files, not databases");
   }

   /* On the regular queue with a spent window, every call is one step
    * and a CRC step one quantum, so these CRCs span several calls: a
    * CUE data track that is part of its file, and the confirming CRC
    * of a shared 3DS serial, which resumes at the entry that began it. */
   {
      static const db_rec_t step_disc_recs[] = {
         { "Stepped Disc", NULL, 0x5BEDC0DEu, STEP_TRACK_SIZE + STEP_AUDIO_SIZE }
      };
      static const db_rec_t step_ctr_recs[] = {
         { "Stepped Rev 0", "CTR-P-STEP", 0x5BED0000u, STEP_CTR_SIZE },
         { "Stepped Rev 1", "CTR-P-STEP", 0x5BED0001u, STEP_CTR_SIZE }
      };
      char     step_db[512], step_in[512];
      uint8_t *d;
      FILE    *f;
      unsigned gathers;

      sprintf(step_db, "%s/scan_db_step", root);
      sprintf(step_in, "%s/scan_step",    root);
      path_mkdir(step_db);
      path_mkdir(step_in);

      sprintf(p, "%s/Test Disc.rdb", step_db);
      if (!write_db_set(p, step_disc_recs, ARRAY_SIZE(step_disc_recs)))
      { check(0, "fixture", "could not write database"); goto done; }
      sprintf(p, "%s/Test 3DS.rdb", step_db);
      if (!write_db_set(p, step_ctr_recs, ARRAY_SIZE(step_ctr_recs)))
      { check(0, "fixture", "could not write database"); goto done; }

      /* Track 1 is the first STEP_TRACK_SIZE bytes; the audio track
       * after it is not part of the CRC. */
      if (!(d = (uint8_t*)calloc(1, STEP_TRACK_SIZE + STEP_AUDIO_SIZE)))
      { check(0, "fixture", "out of memory"); goto done; }
      crc_force(d, STEP_TRACK_SIZE - 4, 0x5BEDC0DEu,
            d + STEP_TRACK_SIZE - 4);
      sprintf(p, "%s/40_step.bin", step_in);
      if (   crc_of(d, STEP_TRACK_SIZE) != 0x5BEDC0DEu
          || !(f = fopen(p, "wb")))
      { free(d); check(0, "fixture", "could not write track"); goto done; }
      fwrite(d, 1, STEP_TRACK_SIZE + STEP_AUDIO_SIZE, f);
      fclose(f);
      free(d);
      sprintf(p, "%s/40_step.cue", step_in);
      if (!(f = fopen(p, "w")))
      { check(0, "fixture", "could not write cue"); goto done; }
      fprintf(f,
            "FILE \"40_step.bin\" BINARY\n"
            "  TRACK 01 MODE1/2352\n"
            "    INDEX 01 00:00:00\n"
            "  TRACK 02 AUDIO\n"
            "    INDEX 01 00:07:35\n");
      fclose(f);
      sprintf(p, "%s/41_step.3ds", step_in);
      if (!write_ctr_sized(p, STEP_CTR_SIZE, 0x20, 1, "CTR-P-STEP",
            0x5BED0001u))
      { check(0, "fixture", "could not write image"); goto done; }

      strlcpy(config_get_ptr()->paths.path_content_database, step_db,
            sizeof(config_get_ptr()->paths.path_content_database));
      gathers = run_scan_counted(step_in);
      strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
            sizeof(config_get_ptr()->paths.path_content_database));
      printf("  info  checks for the stepped CRCs: %u\n", gathers);
      check(scan_completed, "stepped CRC scan completed",
            scan_completed ? "callback fired" : "timed out");

      sprintf(p, "%s/Test Disc.lpl", pl_dir);
      check(   file_contains(p, "Stepped Disc")
            && file_contains(p, "5BEDC0DE|crc"),
            "CUE track CRC over several calls", "5BEDC0DE|crc");
      sprintf(p, "%s/Test 3DS.lpl", pl_dir);
      check(   file_contains(p, "Stepped Rev 1")
            && !file_contains(p, "Stepped Rev 0"),
            "shared serial's CRC resumed at its entry", "Stepped Rev 1");
   }

   walk_lane(root, pl_dir, db_dir);
   single_lane(root, pl_dir, crc_a, sz_a);
   {
      char reader_db[512];

      sprintf(reader_db, "%s/reader_db", root);
      if (!write_reader_db(reader_db))
      { check(0, "fixture", "could not write database"); goto done; }
      reader_cancel_lane(root, reader_db, true);
      reader_cancel_lane(root, reader_db, false);
      reader_stuck_lane(root, reader_db);
      reader_quit_lane(root, reader_db);
      reader_push_lane(root, pl_dir, reader_db);
      reader_reset_lane(root, pl_dir, reader_db);
      reader_fail_lane(root, pl_dir, reader_db);
      order_lane(root, pl_dir, true);
      order_lane(root, pl_dir, false);
      mix_lane(root, pl_dir, true);
      mix_lane(root, pl_dir, false);
      archive_lane(root, pl_dir, true);
      archive_lane(root, pl_dir, false);
      member_first_lane(root, pl_dir, true);
      member_first_lane(root, pl_dir, false);
      cart_gate_lane(root, pl_dir, true);
      cart_gate_lane(root, pl_dir, false);
      base_paths_lane(root, pl_dir, true);
      base_paths_lane(root, pl_dir, false);
      arc_gate_lane(root, pl_dir, true);
      arc_gate_lane(root, pl_dir, false);
      arc_cart_first_lane(root, pl_dir, true);
      arc_cart_first_lane(root, pl_dir, false);
      arc_folder_lane(root, pl_dir, true);
      arc_folder_lane(root, pl_dir, false);
      unindexed_plain_lane(root, pl_dir);
      reader_fail_arcade_lane(root, pl_dir);
      unindexed_member_lane(root, pl_dir);
      multi_nosize_lane(root, pl_dir, true);
      multi_nosize_lane(root, pl_dir, false);
      tz_reader_lane(root);
      tz_unsized_lane(root);
      q1a_lane(root, pl_dir, true);
      q1a_lane(root, pl_dir, false);
#ifdef HAVE_RRAR
      crc0_lane(root, pl_dir, true);
      crc0_lane(root, pl_dir, false);
#endif
      folder_first_lane(root, pl_dir, true);
      folder_first_lane(root, pl_dir, false);
      loose_held_lane(root, pl_dir, true);
      loose_held_lane(root, pl_dir, false);
#ifdef HAVE_RRAR
      rar_lane(root, pl_dir, true);
      rar_lane(root, pl_dir, false);
#endif
      sibling_lane(root, pl_dir, true);
      sibling_lane(root, pl_dir, false);
      dual_match_lane(root, pl_dir, true);
      dual_match_lane(root, pl_dir, false);
      archives_off_lane(root, pl_dir, true);
      archives_off_lane(root, pl_dir, false);
      every_member_lane(root, pl_dir, true);
      every_member_lane(root, pl_dir, false);
      loose_member_lane(root, pl_dir, true);
      loose_member_lane(root, pl_dir, false);
      every_step_lane(root, pl_dir);
      whole_first_lane(root, pl_dir, true);
      whole_first_lane(root, pl_dir, false);
      late_member_lane(root, pl_dir);
      no_claim_lane(root, pl_dir, true);
      no_claim_lane(root, pl_dir, false);
      size_gate_lane(root, pl_dir, true);
      size_gate_lane(root, pl_dir, false);
      first_match_whole_lane(root, pl_dir, true);
      first_match_whole_lane(root, pl_dir, false);
      serial_error_lane(root, pl_dir, true);
      serial_error_lane(root, pl_dir, false);
      whole_ahead_lane(root, pl_dir);
      hit_clears_lane(root, pl_dir);
      rearm_lane(root, pl_dir);
      ahead_lone_lane(root, pl_dir);
      ahead_double_lane(root, pl_dir);
      ahead_streak_lane(root, pl_dir);
      ahead_next_lane(root, pl_dir);
      ahead_hit_count_lane(root, pl_dir);
      ahead_arcade_lane(root, pl_dir);
      ahead_walk_lane(root, pl_dir);
      ahead_walk_none_lane(root, pl_dir);
      ahead_miss_lane(root, pl_dir);
      cue_lane(root, reader_db, false);
      cue_lane(root, reader_db, true);
      strlcpy(config_get_ptr()->paths.path_content_database, reader_db,
            sizeof(config_get_ptr()->paths.path_content_database));
      pads_lane(root, true);
      pads_lane(root, false);
      switch_lane(root, pl_dir);
      switch_stuck_lane(root, pl_dir, reader_db);
      reader_list_end_lane(root, reader_db);
      share_gate_lane(root, reader_db);
      strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
            sizeof(config_get_ptr()->paths.path_content_database));
      use_queue(false);
   }

done:
   printf("\n%d checks, %d failures\n", checks, failures);
   return failures ? 1 : 0;
}

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
#ifdef _WIN32
#include <io.h>
#define cap_dup    _dup
#define cap_dup2   _dup2
#define cap_close  _close
#define cap_fileno _fileno
#else
#include <unistd.h>
#define cap_dup    dup
#define cap_dup2   dup2
#define cap_close  close
#define cap_fileno fileno
#endif

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
#include "../../../retroarch_types.h"
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

/* The English rows, for the messages a check compares whole. */
static const struct
{
   enum msg_hash_enums id;
   const char *str;
} msg_hash_us_rows[] = {
#undef MSG_HASH
#define MSG_HASH(Id, str) { Id, str },
#include "../../../intl/msg_hash_us.h"
#undef MSG_HASH
};

static const char *msg_hash_us_row(enum msg_hash_enums msg)
{
   size_t i;

   for (i = 0; i < sizeof(msg_hash_us_rows) / sizeof(msg_hash_us_rows[0]);
         i++)
      if (msg_hash_us_rows[i].id == msg)
         return msg_hash_us_rows[i].str;
   return "";
}

const char *msg_hash_to_str_us(enum msg_hash_enums msg)
{
   if (msg == MSG_SCANNING_OF_DIRECTORY_FINISHED && end_hold)
      end_step_hold();
   if (   msg == MSG_PLAYLIST_MANAGER_REFRESH_ALL_DONE
       || msg == MSG_MANUAL_CONTENT_SCAN_ALREADY_RUNNING
       || (msg >= MSG_SCAN_SMB_NO_SERVER && msg <= MSG_SCAN_SMB_NO_SHARE))
      return msg_hash_us_row(msg);
   if (msg == MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT)
      return "invalid content";
   if (msg == MSG_SCANNING_NO_DATABASE)
      return "no database";
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
/* How often a refused scan or chain said why. */
static unsigned busy_pushed = 0;
/* Set when a message equal to push_watch is pushed. */
static const char *push_watch;
static bool        push_seen;

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
   if (msg && !strcmp(msg,
            msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_ALREADY_RUNNING)))
      busy_pushed++;
   if (msg && push_watch && !strcmp(msg, push_watch))
      push_seen = true;
}

/* Whether the scan prints what --scan prints. */
static bool cli_output;

bool retroarch_override_setting_is_set(unsigned enum_idx, void *data)
{
   (void)data;
   return cli_output && enum_idx == RARCH_OVERRIDE_SETTING_DATABASE_SCAN;
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

/* How often a folder was opened */
static retro_atomic_int_t smb_stub_opendirs;

smb_dir_handle *retro_vfs_opendir_smb(const char *path, bool include_hidden)
{
   (void)path; (void)include_hidden;
   retro_atomic_fetch_add_int(&smb_stub_opendirs, 1);
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

/* Set: the share answers, names ending .bin are files and the rest
 * folders.  How often a stat was asked on the main thread, and that
 * count when a scan was pushed (-1: none was). */
static bool               smb_stub_live;
static retro_atomic_int_t smb_stub_stats_main;
static retro_atomic_int_t smb_stats_at_push;
/* Stats are counted and, while smb_stat_log is set, listed there; the
 * folders in smb_stat_valid, when set, answer as well. */
static retro_atomic_int_t smb_stat_count;
static char              *smb_stat_log;
static const char *const *smb_stat_valid;

int retro_vfs_stat_smb(const char *path, int64_t *size)
{
   const char *const *v;
   size_t n = strlen(path);

   if (size)
      *size = 0;
   retro_atomic_fetch_add_int(&smb_stat_count, 1);
   if (smb_stat_log)
   {
      strlcat(smb_stat_log, path, 512);
      strlcat(smb_stat_log, " ", 512);
   }
   if (task_is_on_main_thread())
      retro_atomic_fetch_add_int(&smb_stub_stats_main, 1);
   for (v = smb_stat_valid; v && *v; v++)
      if (string_is_equal(path, *v))
         return RETRO_VFS_STAT_IS_VALID | RETRO_VFS_STAT_IS_DIRECTORY;
   if (!smb_stub_live)
      return 0;
   if (n > 4 && !strcmp(path + n - 4, ".bin"))
      return RETRO_VFS_STAT_IS_VALID;
   return RETRO_VFS_STAT_IS_VALID | RETRO_VFS_STAT_IS_DIRECTORY;
}

/* What smb_url_check() and smb_url_failure() answer, and how often
 * each was asked. */
static enum smb_url_result smb_stub_check;
static const char         *smb_stub_detail = "";
static enum smb_failure    smb_stub_failure;
static retro_atomic_int_t  smb_stub_checks;
static retro_atomic_int_t  smb_stub_checks_main;
static retro_atomic_int_t  smb_stub_failures;

enum smb_url_result smb_url_check(const char *url, char *detail,
      size_t len)
{
   (void)url;
   retro_atomic_fetch_add_int(&smb_stub_checks, 1);
   if (task_is_on_main_thread())
      retro_atomic_fetch_add_int(&smb_stub_checks_main, 1);
   strlcpy(detail, smb_stub_detail, len);
   return smb_stub_check;
}

enum smb_failure smb_url_failure(const char *url)
{
   (void)url;
   retro_atomic_fetch_add_int(&smb_stub_failures, 1);
   return smb_stub_failure;
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

/* A stream of slow_size bytes sleeps slow_ms before its first quantum,
 * then waits while slow_hold is set, at most HOLD_USEC (below);
 * slow_in counts those sleeps begun, slow_back those ended (slow_begun
 * holds reader_begun as each of the first four ends), slow_by_scan
 * those in the scan's tick and slow_on_scan those on the thread that
 * last ran the scan: a reader there holds every other task of the
 * queue, the scan too.  Reads of streams of watch_size bytes are
 * counted by who makes them, a reader's on the main thread apart, and
 * those of begun_size bytes as each begins (reader_begun those a
 * reader begins); so are reads begun once cb_fired is set.  A reader's
 * read of a stream of fail_size bytes fails, counted in reader_fails;
 * every read of a stream of err_size bytes fails, the scan's own too.
 * The scan's first quantum of a stream of hold_size bytes waits, at
 * most HOLD_USEC, until hold_want reads of begun_size bytes have begun,
 * a slow one if slow_size is set and gate_n readers are in the gate
 * (below).  All set only between scans. */
#define HOLD_USEC 10000000

static uint32_t           slow_size;
static unsigned           slow_ms;
static retro_atomic_int_t slow_in;
static retro_atomic_int_t slow_hold;
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
static uint32_t           err_size;
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
      retro_time_t until = cpu_features_get_time_usec() + HOLD_USEC;

      if (retro_atomic_load_acquire_size(&scan_thread_tid)
            == (size_t)current_tid())
         retro_atomic_fetch_add_int(&slow_on_scan, 1);
      if (scan)
         retro_atomic_fetch_add_int(&slow_by_scan, 1);
      retro_atomic_fetch_add_int(&slow_in, 1);
      retro_sleep(slow_ms);
      while (   retro_atomic_load_acquire_int(&slow_hold)
             && cpu_features_get_time_usec() < until)
         retro_sleep(1);
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
   if (err_size && size == (int64_t)err_size)
      return -1;
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
/* When set, the most a fingerprint index build may take. */
static size_t setfp_cap;
/* Fingerprint index builds begun, refused ones too. */
static unsigned setfp_builds;
/* When set, the budget checks after the fingerprint index build's
 * steps fail: setfp_yield_n times, or with setfp_stall always, its
 * steps then doing nothing.  setfp_yields counts those failures. */
static unsigned                   setfp_yield_n;
static bool                       setfp_stall;
static bool                       setfp_stepped;
static database_info_index_job_t *setfp_job_live;
static retro_atomic_int_t         setfp_yields;

database_info_index_job_t *__wrap_database_info_index_job_new(
      const char *rdb_path, size_t max_bytes, unsigned want)
{
   size_t                     cap = index_cap ? index_cap : max_bytes;
   database_info_index_job_t *job;

   if (want & DATABASE_INFO_INDEX_SETFP)
      setfp_builds++;
   if (setfp_cap && (want & DATABASE_INFO_INDEX_SETFP))
      cap = setfp_cap;
   job = __real_database_info_index_job_new(rdb_path, cap, want);
   if ((setfp_stall || setfp_yield_n) && (want & DATABASE_INFO_INDEX_SETFP))
      setfp_job_live = job;

   if (   job && walk_name && !(want & DATABASE_INFO_INDEX_SETFP)
       && strstr(rdb_path, walk_name)
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

   if ((setfp_stall || setfp_yield_n) && job && job == setfp_job_live)
   {
      setfp_stepped = true;
      if (setfp_stall)
         return false;
   }
   if (job == walk_job && !walk_steps++ && !walk_late)
   {
      if (walk_more)
         walk_hold(walk_from + walk_more);
      else
         retro_sleep(WALK_IDLE_MS);
   }
   done = __real_database_info_index_job_step(job, records, entries);
   if (done && job == setfp_job_live)
      setfp_stepped = false;
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
   if ((setfp_stall || setfp_yield_n) && setfp_stepped)
   {
      setfp_stepped = false;
      if (setfp_yield_n)
         setfp_yield_n--;
      retro_atomic_fetch_add_int(&setfp_yields, 1);
      return false;
   }
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

/* The chain's untitled steps: the prep_cancel_at'th pushed (from 1) is
 * cancelled once its playlist-folder walk or playlist load begins.
 * Set only between runs. */
static unsigned      prep_cancel_at;
static unsigned      prep_pushes;
static retro_task_t *prep_cancel_task;

static void prep_cancel_begun(void)
{
   retro_task_t *task = prep_cancel_task;

   if (task)
   {
      prep_cancel_task = NULL;
      task_set_flags(task, RETRO_TASK_FLG_CANCELLED, true);
   }
}

/* The title of the last task pushed with one, a reader's aside. */
static char pushed_title[256];

bool __real_task_queue_push(retro_task_t *task);
bool __wrap_task_queue_push(retro_task_t *task)
{
   int n;

#ifdef HAVE_SMBCLIENT
   /* Only a scan has a title */
   if (task->title)
      retro_atomic_store_release_int(&smb_stats_at_push,
            retro_atomic_load_acquire_int(&smb_stub_stats_main));
#endif
   if (!(task_get_flags(task) & RETRO_TASK_FLG_PARALLEL))
   {
      if (task->title)
         strlcpy(pushed_title, task->title, sizeof(pushed_title));
      if (   prep_cancel_at && !task->title
          && ++prep_pushes == prep_cancel_at)
         prep_cancel_task = task;
      return __real_task_queue_push(task);
   }
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
   uint32_t    set_fp;
} db_rec_t;

/* One database holding @n records; a NULL serial or rom_name, a 0
 * set_fp, or unsized leaves the key out. */
/* As write_db_set(), each record @i given the region @regions[i] when
 * that is not NULL */
static int write_db_set_regions(const char *path, const db_rec_t *recs,
      const char *const *regions, unsigned n)
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
            + (recs[r].rom_name ? 1 : 0) + (recs[r].unsized ? 0 : 1)
            + (regions && regions[r] ? 1 : 0)
            + (recs[r].set_fp ? 1 : 0));
      bfixstr(&body, "name"); bfixstr(&body, recs[r].name);
      if (regions && regions[r])
      {
         bfixstr(&body, "region");
         bfixstr(&body, regions[r]);
      }
      if (recs[r].rom_name)
      {
         bfixstr(&body, "rom_name");
         bfixstr(&body, recs[r].rom_name);
      }
      if (recs[r].set_fp)
      {
         bfixstr(&body, "set_fp1");
         bbin4(&body, recs[r].set_fp);
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

static int write_db_set(const char *path, const db_rec_t *recs,
      unsigned n)
{
   return write_db_set_regions(path, recs, NULL, n);
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
 * of the CRC-32 of the rest of that header.  @solid marks the archive
 * solid and its entries past the first packed, which the reader then
 * cannot unpack.  A name ending in '/' is a directory, stored without
 * the '/'. */
static int write_rar_entries(const char *path, const zip_entry_t *e,
      unsigned n, bool solid)
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
   if (solid)
      PUT16(h + 3, 0x0008);         /* solid */
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
      h[25] = (solid && i) ? 0x31 : 0x30; /* packed, or stored */
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
/* What the last run_scan() was handed: its user data is this. */
static task_dbscan_result_t scan_result;
static bool                 scan_err;

static void scan_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   (void)task;
   if (err && *err)
      fprintf(stderr, "  scan reported an error: %s\n", err);
   scan_err = (err != NULL);
   if (user_data && task_data)
      *(task_dbscan_result_t*)user_data =
            *(const task_dbscan_result_t*)task_data;
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

/* While set, run_scan() pushes a group scan of these playlists, its
 * --scan lines labelled group_label, in place of a scan of its dir. */
static const struct string_list *group_names;
static const char               *group_label;

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
   /* Kept unless the callback gets both pointers. */
   scan_result.added      = 9999;
   scan_result.duplicates = 9999;
   scan_result.unmatched  = 9999;
   scan_result.errors     = 9999;
   scan_result.failed     = true;
   scan_err               = false;
   if (!(group_names
         ? task_push_manual_content_scan_group(group_names, group_label,
               scan_cb, &scan_result)
         : task_push_dbscan(dir, scan_cb, &scan_result)))
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
   if (!task_push_dbscan(dir, scan_cb, NULL))
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

#define CISO_BLOCK 0x8000

/* A CISO of one stored 32 KiB block, disc block @at, holding a disc
 * header with @id and the system's @magic at @magic_at. */
static int write_ciso(const char *path, unsigned at, const char *id,
      const char *magic, unsigned magic_at)
{
   uint8_t *d = (uint8_t*)calloc(1, 2 * CISO_BLOCK);
   int      ok;

   if (!d)
      return 0;
   memcpy(d, "CISO", 4);
   d[5]      = (uint8_t)(CISO_BLOCK >> 8);
   d[8 + at] = 1;
   memcpy(d + CISO_BLOCK, id, strlen(id));
   memcpy(d + CISO_BLOCK + magic_at, magic, 4);
   ok = write_bytes(path, d, 2 * CISO_BLOCK);
   free(d);
   return ok;
}

/* CISO files take the serial lookup through the disc image they stand
 * for: a GameCube and a Wii image match by serial, and one whose disc
 * starts with a block left out matches nothing, though its first
 * stored block holds a header. */
static void ciso_lane(const char *root, const char *pl_dir,
      const char *db_dir)
{
   static const db_rec_t gc_recs[] = {
      { "Cube The Game", "DL-DOL-GALE-USA", 0x3C150001u, 2 * CISO_BLOCK },
      { "Gap The Game",  "DL-DOL-GAPE-USA", 0x3C150003u, 2 * CISO_BLOCK }
   };
   static const db_rec_t wii_recs[] = {
      { "Wii The Game",  "RMCE01",          0x3C150002u, 2 * CISO_BLOCK }
   };
   char *ciso_db = (char*)malloc(512);
   char *ciso_in = (char*)malloc(512);
   char *p       = (char*)malloc(1024);
   bool  started;

   if (!ciso_db || !ciso_in || !p)
   {
      check(0, "ciso lane", "out of memory");
      goto end;
   }

   sprintf(ciso_db, "%s/scan_db_ciso", root);
   sprintf(ciso_in, "%s/scan_ciso",    root);
   path_mkdir(ciso_db);
   path_mkdir(ciso_in);

   sprintf(p, "%s/Test GameCube.rdb", ciso_db);
   if (!write_db_set(p, gc_recs, ARRAY_SIZE(gc_recs)))
   { check(0, "fixture", "could not write database"); goto end; }
   sprintf(p, "%s/Test Wii.rdb", ciso_db);
   if (!write_db_set(p, wii_recs, ARRAY_SIZE(wii_recs)))
   { check(0, "fixture", "could not write database"); goto end; }

   sprintf(p, "%s/50_cube.ciso", ciso_in);
   if (!write_ciso(p, 0, "GALE01", "\xc2\x33\x9f\x3d", 0x1c))
   { check(0, "fixture", "could not write image"); goto end; }
   sprintf(p, "%s/51_wii.ciso", ciso_in);
   if (!write_ciso(p, 0, "RMCE01", "\x5d\x1c\x9e\xa3", 0x18))
   { check(0, "fixture", "could not write image"); goto end; }
   sprintf(p, "%s/52_gap.ciso", ciso_in);
   if (!write_ciso(p, 1, "GAPE01", "\xc2\x33\x9f\x3d", 0x1c))
   { check(0, "fixture", "could not write image"); goto end; }

   strlcpy(config_get_ptr()->paths.path_content_database, ciso_db,
         sizeof(config_get_ptr()->paths.path_content_database));
   started = run_scan(ciso_in);
   strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
         sizeof(config_get_ptr()->paths.path_content_database));
   if (!started)
   {
      check(0, "ciso scan started", "task_push_dbscan refused");
      goto end;
   }

   check(scan_completed, "ciso scan completed",
         scan_completed ? "callback fired" : "timed out");
   sprintf(p, "%s/Test GameCube.lpl", pl_dir);
   check(   file_contains(p, "50_cube.ciso")
         && file_contains(p, "Cube The Game"),
         "GameCube CISO matches by serial", "50_cube.ciso");
   check(!file_contains(p, "52_gap.ciso"),
         "CISO read as its disc image", "52_gap.ciso unmatched");
   sprintf(p, "%s/Test Wii.lpl", pl_dir);
   check(   file_contains(p, "51_wii.ciso")
         && file_contains(p, "Wii The Game"),
         "Wii CISO matches by serial", "51_wii.ciso");

end:
   free(ciso_db);
   free(ciso_in);
   free(p);
}

#define XBOX_SECTOR 2048

static void xbox_put_be32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v >> 24);
   p[1] = (uint8_t)(v >> 16);
   p[2] = (uint8_t)(v >> 8);
   p[3] = (uint8_t)v;
}

/* An XEX's head: one optional header, the execution info, with
 * @title, disc @disc of @discs */
static void xbox_xex(uint8_t *x, uint32_t title, uint8_t disc,
      uint8_t discs)
{
   memcpy(x, "XEX2", 4);
   xbox_put_be32(x + 8, 0x200);
   xbox_put_be32(x + 16, 0x60);
   xbox_put_be32(x + 20, 1);
   xbox_put_be32(x + 0x60 + 0x178, 0x000000FFu);
   xbox_put_be32(x + 24, 0x00040006u);
   xbox_put_be32(x + 28, 0x48);
   xbox_put_be32(x + 0x48 + 12, title);
   x[0x48 + 18] = disc;
   x[0x48 + 19] = discs;
}

static void xbox_put32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)v;
   p[1] = (uint8_t)(v >> 8);
   p[2] = (uint8_t)(v >> 16);
   p[3] = (uint8_t)(v >> 24);
}

/* An Xbox disc image of @size with its game partition at @base: the
 * volume descriptor, a root holding default.xbe, and its title @title.
 * With @ps2_spaces the field at 0x8008 holds what detect_system()
 * takes for a PS2 DVD. Written sparsely: only the partition's head. */
static int write_xbox_disc(const char *path, uint32_t base, uint32_t size,
      uint32_t title, bool ps2_spaces, bool x360, uint8_t disc,
      uint8_t discs)
{
   uint8_t *d = (uint8_t*)calloc(1, 40 * XBOX_SECTOR);
   uint8_t *v = d + 32 * XBOX_SECTOR;
   uint8_t *r = d + 33 * XBOX_SECTOR;
   uint8_t *x = d + 34 * XBOX_SECTOR;
   FILE    *f;
   int      ok;

   if (!d)
      return 0;
   memcpy(v, "MICROSOFT*XBOX*MEDIA", 20);
   xbox_put32(v + 0x14, 33);
   xbox_put32(v + 0x18, XBOX_SECTOR);
   xbox_put32(r + 4, 34);
   xbox_put32(r + 8, 4 * XBOX_SECTOR);
   r[12] = 0x20;
   r[13] = 11;
   memcpy(r + 14, x360 ? "default.xex" : "default.xbe", 11);
   if (x360)
      xbox_xex(x, title, disc, discs);
   else
   {
      memcpy(x, "XBEH", 4);
      xbox_put32(x + 0x104, 0x10000);
      xbox_put32(x + 0x118, 0x10000 + 0x178);
      xbox_put32(x + 0x178 + 8, title);
      xbox_put32(x + 0x178 + 0xA0, 1);
   }

   if (!(f = fopen(path, "wb")))
   {
      free(d);
      return 0;
   }
   ok = 1;
   if (ps2_spaces)
      ok = !fseek(f, 0x8008, SEEK_SET) && fwrite("           ", 1, 11, f) == 11;
   ok = ok && !fseek(f, (long)base, SEEK_SET)
           && fwrite(d, 1, 40 * XBOX_SECTOR, f) == 40 * XBOX_SECTOR
           && !fseek(f, (long)size - 1, SEEK_SET)
           && fputc(0, f) == 0;
   ok = !fclose(f) && ok;
   free(d);
   return ok;
}

static int write_xbox(const char *path, uint32_t base, uint32_t size,
      uint32_t title, bool ps2_spaces)
{
   return write_xbox_disc(path, base, size, title, ps2_spaces,
         false, 0, 0);
}

/* Xbox discs take the serial lookup by their XBE's title: an XISO, a
 * trimmed one named .iso, which the ISO size hint would rule out, and
 * a whole XGD3 image whose video partition reads as a PS2 DVD to
 * detect_system(). */
static void xbox_lane(const char *root, const char *pl_dir,
      const char *db_dir)
{
   /* sizes no trimmed image comes near, as Redump's do */
   static const db_rec_t xbox_recs[] = {
      /* an extra of another region's release, ahead of the game */
      { "Halo Extras",    "MS-004", 0x3B0E0004u, 0xF0000000u },
      { "Halo The Game",  "MS-004", 0x3B0E0001u, 0xF0000000u },
      { "Trim The Game",  "MS-012", 0x3B0E0002u, 0xF0000000u },
      { "Whole The Game", "EA-013", 0x3B0E0003u, 0xF0000000u }
   };
   static const char *const xbox_regions[] = {
      "Japan", "USA, Europe", NULL, NULL
   };
   char *xbox_db = (char*)malloc(512);
   char *xbox_in = (char*)malloc(512);
   char *p       = (char*)malloc(1024);
   bool  started;

   if (!xbox_db || !xbox_in || !p)
   {
      check(0, "xbox lane", "out of memory");
      goto end;
   }

   sprintf(xbox_db, "%s/scan_db_xbox", root);
   sprintf(xbox_in, "%s/scan_xbox",    root);
   path_mkdir(xbox_db);
   path_mkdir(xbox_in);

   sprintf(p, "%s/Test Xbox.rdb", xbox_db);
   if (!write_db_set_regions(p, xbox_recs, xbox_regions,
         ARRAY_SIZE(xbox_recs)))
   { check(0, "fixture", "could not write database"); goto end; }

   sprintf(p, "%s/60_halo.xiso", xbox_in);
   if (!write_xbox(p, 0, 0x100000, 0x4D530004u, false))
   { check(0, "fixture", "could not write image"); goto end; }
   sprintf(p, "%s/61_trim.iso", xbox_in);
   if (!write_xbox(p, 0, 0x100000, 0x4D53000Cu, false))
   { check(0, "fixture", "could not write image"); goto end; }
   sprintf(p, "%s/62_whole.iso", xbox_in);
   if (!write_xbox(p, 0x2080000, 0x2080000 + 0x100000, 0x4541000Du, true))
   { check(0, "fixture", "could not write image"); goto end; }

   strlcpy(config_get_ptr()->paths.path_content_database, xbox_db,
         sizeof(config_get_ptr()->paths.path_content_database));
   started = run_scan(xbox_in);
   strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
         sizeof(config_get_ptr()->paths.path_content_database));
   if (!started)
   {
      check(0, "xbox scan started", "task_push_dbscan refused");
      goto end;
   }

   check(scan_completed, "xbox scan completed",
         scan_completed ? "callback fired" : "timed out");
   sprintf(p, "%s/Test Xbox.lpl", pl_dir);
   check(   file_contains(p, "60_halo.xiso")
         && file_contains(p, "Halo The Game"),
         "XISO matches by its XBE title", "60_halo.xiso");
   check(!file_contains(p, "Halo Extras"),
         "the XBE's regions pick the release", "not the extra");
   check(   file_contains(p, "61_trim.iso")
         && file_contains(p, "Trim The Game"),
         "trimmed .iso not ruled out by size", "61_trim.iso");
   check(   file_contains(p, "62_whole.iso")
         && file_contains(p, "Whole The Game"),
         "whole XGD3 image past the PS2 magic", "62_whole.iso");

end:
   free(xbox_db);
   free(xbox_in);
   free(p);
}

/* A ZArchive holding default.xex (@title) and one other file, its one
 * block compressed */
static int write_zar(const char *path, uint32_t title)
{
   uint8_t *stream = (uint8_t*)calloc(1, 65536);
   size_t   cap    = rzstd_compress_bound(65536);
   uint8_t *out    = (uint8_t*)calloc(1, cap + 4096);
   uint8_t  tree[3 * 16];
   uint8_t  names[64];
   size_t   nsize  = 0;
   size_t   bsize  = 0;
   size_t   at;
   uint8_t *ft;
   int      ok     = 0;

   if (!stream || !out)
      goto end;
   xbox_xex(stream, title, 1, 1);
   memcpy(stream + 0x1000, "readme", 6);
   if (     rzstd_encode(out, cap, stream, 65536, 3, &bsize)
               != RZSTD_PROCESS_END
         || !bsize || bsize >= 65536)
      goto end;

   /* root: a.txt then default.xex */
   names[nsize++] = 5;
   memcpy(names + nsize, "a.txt", 5);
   nsize += 5;
   names[nsize++] = 11;
   memcpy(names + nsize, "default.xex", 11);
   nsize += 11;
   memset(tree, 0, sizeof(tree));
   xbox_put_be32(tree + 0, 0x7FFFFFFFu);
   xbox_put_be32(tree + 4, 1);
   xbox_put_be32(tree + 8, 2);
   xbox_put_be32(tree + 16, 0x80000000u | 0);
   xbox_put_be32(tree + 20, 0x1000);
   xbox_put_be32(tree + 24, 6);
   xbox_put_be32(tree + 32, 0x80000000u | 6);
   xbox_put_be32(tree + 36, 0);
   xbox_put_be32(tree + 40, 0x1000);

   /* blocks, the record, names, tree, two empty sections, footer */
   at = bsize;
   memset(out + at, 0, 40);
   out[at + 8] = (uint8_t)((bsize - 1) >> 8);
   out[at + 9] = (uint8_t)(bsize - 1);
   memcpy(out + at + 40, names, nsize);
   memcpy(out + at + 40 + nsize, tree, sizeof(tree));
   ft = out + at + 40 + nsize + sizeof(tree);
   memset(ft, 0, 144);
   xbox_put_be32(ft + 12, (uint32_t)bsize);
   xbox_put_be32(ft + 20, (uint32_t)at);
   xbox_put_be32(ft + 28, 40);
   xbox_put_be32(ft + 36, (uint32_t)(at + 40));
   xbox_put_be32(ft + 44, (uint32_t)nsize);
   xbox_put_be32(ft + 52, (uint32_t)(at + 40 + nsize));
   xbox_put_be32(ft + 60, sizeof(tree));
   xbox_put_be32(ft + 68, (uint32_t)(ft - out));
   xbox_put_be32(ft + 84, (uint32_t)(ft - out));
   xbox_put_be32(ft + 132, (uint32_t)(ft - out + 144));
   xbox_put_be32(ft + 136, 0x61BF3A01u);
   xbox_put_be32(ft + 140, 0x169F52D6u);
   ok = write_bytes(path, out, (size_t)(ft - out + 144));
end:
   free(stream);
   free(out);
   return ok;
}

/* Xbox 360 content takes the serial lookup by its title: a whole XGD3
 * disc, the second of a set, an XEX and an STFS package on their own;
 * a .xex that is no Xbox 360 executable (an Atari 8-bit one) still
 * matches its own database by CRC. */
static void x360_lane(const char *root, const char *pl_dir,
      const char *db_dir)
{
   static const db_rec_t x360_recs[] = {
      { "Set The Game (Disc 1)", "FO-2046-0", 0x3B360001u, 0xF0000000u },
      { "Set The Game (Disc 2) (Japan)", "FO-2046-1", 0x3B360007u,
        0xF0000000u },
      { "Set The Game (Disc 2)", "FO-2046-1", 0x3B360002u, 0xF0000000u },
      { "Exe The Game",          "AV-2071",   0x3B360003u, 0xF0000000u },
      { "Arcade The Game",       "XA-2209",   0x3B360004u, 0xF0000000u },
      { "Zar The Game",          "SE-2142",   0x3B360006u, 0xF0000000u }
   };
   static const char *const x360_regions[] = {
      NULL, "Japan", "USA", NULL, NULL, NULL
   };
   char    *x_db = (char*)malloc(512);
   char    *x_in = (char*)malloc(512);
   char    *p    = (char*)malloc(1024);
   uint8_t *d    = (uint8_t*)calloc(1, 0x1000);
   bool     started;

   if (!x_db || !x_in || !p || !d)
   {
      check(0, "x360 lane", "out of memory");
      goto end;
   }

   sprintf(x_db, "%s/scan_db_x360", root);
   sprintf(x_in, "%s/scan_x360",    root);
   path_mkdir(x_db);
   path_mkdir(x_in);

   sprintf(p, "%s/Test Xbox 360.rdb", x_db);
   if (!write_db_set_regions(p, x360_recs, x360_regions,
         ARRAY_SIZE(x360_recs)))
   { check(0, "fixture", "could not write database"); goto end; }
   sprintf(p, "%s/Test Atari.rdb", x_db);
   if (!write_db(p, "Atari The Game", 0x3B360005u, 0x1000))
   { check(0, "fixture", "could not write database"); goto end; }

   sprintf(p, "%s/70_disc2.iso", x_in);
   if (!write_xbox_disc(p, 0x2080000, 0x2080000 + 0x100000, 0x464F07FEu,
         false, true, 2, 2))
   { check(0, "fixture", "could not write image"); goto end; }
   xbox_xex(d, 0x41560817u, 1, 1);
   sprintf(p, "%s/71_game.xex", x_in);
   if (!write_bytes(p, d, 0x1000))
   { check(0, "fixture", "could not write executable"); goto end; }
   memset(d, 0, 0x1000);
   memcpy(d, "LIVE", 4);
   xbox_put_be32(d + 0x360, 0x584108A1u);
   sprintf(p, "%s/72_arcade.xcp", x_in);
   if (!write_bytes(p, d, 0x1000))
   { check(0, "fixture", "could not write package"); goto end; }
   sprintf(p, "%s/74_game.zar", x_in);
   if (!write_zar(p, 0x5345085Eu))
   { check(0, "fixture", "could not write archive"); goto end; }
   sprintf(p, "%s/73_atari.xex", x_in);
   if (!write_content(p, 0x3B360005u, 0x1000))
   { check(0, "fixture", "crc forcing failed"); goto end; }

   strlcpy(config_get_ptr()->paths.path_content_database, x_db,
         sizeof(config_get_ptr()->paths.path_content_database));
   started = run_scan(x_in);
   strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
         sizeof(config_get_ptr()->paths.path_content_database));
   if (!started)
   {
      check(0, "x360 scan started", "task_push_dbscan refused");
      goto end;
   }

   check(scan_completed, "x360 scan completed",
         scan_completed ? "callback fired" : "timed out");
   sprintf(p, "%s/Test Xbox 360.lpl", pl_dir);
   check(   file_contains(p, "70_disc2.iso")
         && file_contains(p, "Set The Game (Disc 2)")
         && !file_contains(p, "Set The Game (Disc 1)"),
         "360 disc matches its place in the set", "70_disc2.iso");
   check(!file_contains(p, "Set The Game (Disc 2) (Japan)"),
         "the XEX's regions pick the release", "not the Japanese one");
   check(   file_contains(p, "71_game.xex")
         && file_contains(p, "Exe The Game"),
         "XEX matches by its title", "71_game.xex");
   check(   file_contains(p, "72_arcade.xcp")
         && file_contains(p, "Arcade The Game"),
         "STFS package matches by its title", "72_arcade.xcp");
   check(   file_contains(p, "74_game.zar")
         && file_contains(p, "Zar The Game"),
         "ZArchive matches by its default.xex", "74_game.zar");
   sprintf(p, "%s/Test Atari.lpl", pl_dir);
   check(   file_contains(p, "73_atari.xex")
         && file_contains(p, "Atari The Game"),
         "other .xex still matches by CRC", "73_atari.xex");

end:
   free(x_db);
   free(x_in);
   free(p);
   free(d);
}

/* A Wii U disc of @sectors 32 KiB sectors whose header says @code;
 * with @wux, as a WUX that keeps the empty sectors once */
static int write_wiiu_disc(const char *path, const char *code,
      unsigned sectors, bool wux)
{
   uint8_t *d  = (uint8_t*)calloc(sectors + 1, 0x8000);
   size_t   n  = (size_t)sectors * 0x8000;
   int      ok = 0;
   unsigned i;

   if (!d)
      return 0;
   memcpy(d, "WUP-P-", 6);
   memcpy(d + 6, code, 4);
   memcpy(d + 10, "-0-00", 5);
   if (wux)
   {
      /* the header, the index (sector 0, then the empty one for the
       * rest), padding to a sector, then the two sectors */
      uint8_t *w = (uint8_t*)calloc(3, 0x8000);
      if (!w)
      {
         free(d);
         return 0;
      }
      memcpy(w, "WUX0", 4);
      w[4] = 0x2E; w[5] = 0xD0; w[6] = 0x99; w[7] = 0x10;
      w[9] = 0x80;
      w[16] = (uint8_t)n; w[17] = (uint8_t)(n >> 8);
      w[18] = (uint8_t)(n >> 16); w[19] = (uint8_t)(n >> 24);
      for (i = 1; i < sectors; i++)
         w[32 + i * 4] = 1;
      memcpy(w + 0x8000, d, 0x8000);
      ok = write_bytes(path, w, 3 * 0x8000);
      free(w);
   }
   else
      ok = write_bytes(path, d, n);
   free(d);
   return ok;
}

/* A Wii U archive of two titles - an update listed first, then the
 * base game - each with meta/meta.xml; the update's names @upd_code,
 * the game's @code and @company. Its one block is kept as it is. */
static int write_wua(const char *path, const char *code,
      const char *company, const char *upd_code)
{
   static const char *const dirs[2] = {
      "0005000E101C9500_v32", "00050000101C9500_v0" };
   uint8_t *out = (uint8_t*)calloc(1, 65536 + 4096);
   uint8_t  tree[7 * 16];
   uint8_t  names[128];
   uint32_t name_at[7];
   size_t   nsize = 0;
   size_t   xml_len[2];
   size_t   at;
   uint8_t *ft;
   int      ok    = 0;
   unsigned i;

   if (!out)
      return 0;
   /* the stream: the update's meta.xml, then the game's at 2048 */
   for (i = 0; i < 2; i++)
      xml_len[i] = (size_t)sprintf((char*)out + i * 2048,
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<menu type=\"complex\" access=\"777\">\n"
            "  <version type=\"unsignedInt\" length=\"4\">33</version>\n"
            "  <product_code type=\"string\" length=\"32\">WUP-%c-%s"
            "</product_code>\n"
            "  <content_platform type=\"string\" length=\"32\">WUP"
            "</content_platform>\n"
            "  <company_code type=\"string\" length=\"8\">%s"
            "</company_code>\n</menu>\n",
            i ? 'P' : 'N', i ? code : upd_code, i ? company : "0001");

   /* names: the two titles, "meta" twice, "meta.xml" twice */
   name_at[0] = 0x7FFFFFFFu;
   for (i = 1; i < 7; i++)
   {
      const char *nm = (i < 3) ? dirs[i - 1] : (i < 5) ? "meta" : "meta.xml";
      size_t      l  = strlen(nm);
      name_at[i]     = (uint32_t)nsize;
      names[nsize++] = (uint8_t)l;
      memcpy(names + nsize, nm, l);
      nsize         += l;
   }
   /* root: the titles; each title: its meta; each meta: its meta.xml */
   memset(tree, 0, sizeof(tree));
   for (i = 0; i < 7; i++)
      xbox_put_be32(tree + i * 16, name_at[i] | (i >= 5 ? 0x80000000u : 0));
   xbox_put_be32(tree + 0 * 16 + 4, 1); xbox_put_be32(tree + 0 * 16 + 8, 2);
   xbox_put_be32(tree + 1 * 16 + 4, 3); xbox_put_be32(tree + 1 * 16 + 8, 1);
   xbox_put_be32(tree + 2 * 16 + 4, 4); xbox_put_be32(tree + 2 * 16 + 8, 1);
   xbox_put_be32(tree + 3 * 16 + 4, 5); xbox_put_be32(tree + 3 * 16 + 8, 1);
   xbox_put_be32(tree + 4 * 16 + 4, 6); xbox_put_be32(tree + 4 * 16 + 8, 1);
   xbox_put_be32(tree + 5 * 16 + 4, 0);
   xbox_put_be32(tree + 5 * 16 + 8, (uint32_t)xml_len[0]);
   xbox_put_be32(tree + 6 * 16 + 4, 2048);
   xbox_put_be32(tree + 6 * 16 + 8, (uint32_t)xml_len[1]);

   /* the block, the record, names, tree, two empty sections, footer */
   at = 65536;
   memset(out + at, 0, 40);
   out[at + 8] = 0xFF;
   out[at + 9] = 0xFF;
   memcpy(out + at + 40, names, nsize);
   memcpy(out + at + 40 + nsize, tree, sizeof(tree));
   ft = out + at + 40 + nsize + sizeof(tree);
   memset(ft, 0, 144);
   xbox_put_be32(ft + 12, 65536);
   xbox_put_be32(ft + 20, (uint32_t)at);
   xbox_put_be32(ft + 28, 40);
   xbox_put_be32(ft + 36, (uint32_t)(at + 40));
   xbox_put_be32(ft + 44, (uint32_t)nsize);
   xbox_put_be32(ft + 52, (uint32_t)(at + 40 + nsize));
   xbox_put_be32(ft + 60, sizeof(tree));
   xbox_put_be32(ft + 68, (uint32_t)(ft - out));
   xbox_put_be32(ft + 84, (uint32_t)(ft - out));
   xbox_put_be32(ft + 132, (uint32_t)(ft - out + 144));
   xbox_put_be32(ft + 136, 0x61BF3A01u);
   xbox_put_be32(ft + 140, 0x169F52D6u);
   ok = write_bytes(path, out, (size_t)(ft - out + 144));
   free(out);
   return ok;
}

/* Wii U content takes the serial lookup by its product code, in the
 * Wii U database alone: a release's serial adds its maker's two
 * characters, or nothing for a title sold as a download. A WUD, a WUX
 * and a Wii U disc named .iso match a disc release; an archive matches
 * the release of the maker its meta.xml names, and the download when
 * there is none, reading its base title rather than the update listed
 * before it. Another database holding the code alone is not
 * consulted. */
static void wiiu_lane(const char *root, const char *pl_dir,
      const char *db_dir)
{
   static const db_rec_t wiiu_recs[] = {
      { "Kart Download (USA)",  "AMKE",   0, 0, NULL, true },
      { "Kart Disc (USA)",      "AMKE01", 0, 0, NULL, true },
      { "Kart Other (USA)",     "AMKEXY", 0, 0, NULL, true },
      { "Kart Long (USA)",      "AMKE01Z", 0, 0, NULL, true },
      { "Pikmin Disc (Europe)", "AC3P01", 0, 0, NULL, true },
      { "Zelda Disc (USA)",     "ALZE01", 0, 0, NULL, true },
      { "Splat Disc (USA)",     "WSPE01", 0, 0, NULL, true },
      { "Splat Download (USA)", "WSPE",   0, 0, NULL, true },
      { "Mii Other (Japan)",    "WMIJ01", 0, 0, NULL, true },
      { "Mii Maker (Japan)",    "WMIJ8P", 0, 0, NULL, true },
      { "Update Trap (USA)",    "UPDE",   0, 0, NULL, true }
   };
   static const db_rec_t decoy_recs[] = {
      { "Decoy Pikmin", "AC3P", 0, 0, NULL, true },
      { "Decoy Zelda",  "ALZE", 0, 0, NULL, true },
      { "Decoy Kart",   "AMKE", 0, 0, NULL, true }
   };
   char *w_db = (char*)malloc(512);
   char *w_in = (char*)malloc(512);
   char *p    = (char*)malloc(1024);
   bool  started;

   if (!w_db || !w_in || !p)
   {
      check(0, "wiiu lane", "out of memory");
      goto end;
   }

   sprintf(w_db, "%s/scan_db_wiiu", root);
   sprintf(w_in, "%s/scan_wiiu",    root);
   path_mkdir(w_db);
   path_mkdir(w_in);

   sprintf(p, "%s/Nintendo - Wii U.rdb", w_db);
   if (!write_db_set(p, wiiu_recs, ARRAY_SIZE(wiiu_recs)))
   { check(0, "fixture", "could not write database"); goto end; }
   sprintf(p, "%s/Test Decoy.rdb", w_db);
   if (!write_db_set(p, decoy_recs, ARRAY_SIZE(decoy_recs)))
   { check(0, "fixture", "could not write database"); goto end; }

   sprintf(p, "%s/80_kart.wud", w_in);
   if (!write_wiiu_disc(p, "AMKE", 4, false))
   { check(0, "fixture", "could not write image"); goto end; }
   sprintf(p, "%s/81_pikmin.wux", w_in);
   if (!write_wiiu_disc(p, "AC3P", 4, true))
   { check(0, "fixture", "could not write image"); goto end; }
   sprintf(p, "%s/82_zelda.iso", w_in);
   if (!write_wiiu_disc(p, "ALZE", 4, false))
   { check(0, "fixture", "could not write image"); goto end; }
   sprintf(p, "%s/83_splat.wua", w_in);
   if (!write_wua(p, "WSPE", "0099", "UPDE"))
   { check(0, "fixture", "could not write archive"); goto end; }
   sprintf(p, "%s/84_mii.wua", w_in);
   if (!write_wua(p, "WMIJ", "008P", "UPDE"))
   { check(0, "fixture", "could not write archive"); goto end; }

   strlcpy(config_get_ptr()->paths.path_content_database, w_db,
         sizeof(config_get_ptr()->paths.path_content_database));
   started = run_scan(w_in);
   strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
         sizeof(config_get_ptr()->paths.path_content_database));
   if (!started)
   {
      check(0, "wiiu scan started", "task_push_dbscan refused");
      goto end;
   }

   check(scan_completed, "wiiu scan completed",
         scan_completed ? "callback fired" : "timed out");
   sprintf(p, "%s/Nintendo - Wii U.lpl", pl_dir);
   check(   file_contains(p, "80_kart.wud")
         && file_contains(p, "Kart Disc (USA)")
         && file_contains(p, "AMKE01|serial")
         && !file_contains(p, "Kart Download")
         && !file_contains(p, "Kart Other")
         && !file_contains(p, "Kart Long"),
         "a WUD matches a disc release of its product code",
         "80_kart.wud");
   check(   file_contains(p, "81_pikmin.wux")
         && file_contains(p, "Pikmin Disc (Europe)"),
         "a WUX is read through its index", "81_pikmin.wux");
   check(   file_contains(p, "82_zelda.iso")
         && file_contains(p, "Zelda Disc (USA)"),
         "a Wii U disc named .iso", "82_zelda.iso");
   check(   file_contains(p, "83_splat.wua")
         && file_contains(p, "Splat Download (USA)")
         && !file_contains(p, "Splat Disc"),
         "an archive whose maker has no release takes the download",
         "83_splat.wua");
   check(   file_contains(p, "84_mii.wua")
         && file_contains(p, "Mii Maker (Japan)")
         && !file_contains(p, "Mii Other"),
         "an archive's maker picks the release", "84_mii.wua");
   check(!file_contains(p, "Update Trap"),
         "an archive's base title, not its update", "UPDE");
   sprintf(p, "%s/Test Decoy.lpl", pl_dir);
   check(!path_is_valid(p) || !file_contains(p, "Decoy"),
         "a product code is looked up in the Wii U database alone",
         "Test Decoy.lpl");

end:
   free(w_db);
   free(w_in);
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

static void dotted_names_lane(const char *root, const char *pl_dir,
      const char *db_dir, uint32_t crc, uint32_t size)
{
   char dir[512], db[512], out[512], p[1024], lpl[1024];
   playlist_config_t *cfg = (playlist_config_t*)calloc(1, sizeof(*cfg));
   manual_content_scan_task_config_t *scan =
         (manual_content_scan_task_config_t*)calloc(1, sizeof(*scan));
   playlist_t *pl = NULL;
   bool ready;

   if (!cfg || !scan)
   {
      check(0, "dotted names", "out of memory");
      goto end;
   }

   sprintf(dir, "%s/scan.dotted", root);
   sprintf(db, "%s/scan_dotted_db", root);
   sprintf(out, "%s/scan_dotted_playlist", root);
   path_mkdir(dir);
   path_mkdir(db);
   path_mkdir(out);
   sprintf(p, "%s/Tiger - Game.com.rdb", db);
   if (!write_db(p, "Dotted Game", crc, size))
      goto fixture;
   sprintf(p, "%s/before.bin", dir);
   if (!write_content(p, crc, size))
      goto fixture;
   sprintf(p, "%s/after.bin", dir);
   filestream_delete(p);
   sprintf(lpl, "%s/Tiger - Game.com.lpl", out);
   filestream_delete(lpl);
   sprintf(p, "%s/Tiger - Game.lpl", out);
   filestream_delete(p);

   strlcpy(config_get_ptr()->paths.directory_playlist, out,
         sizeof(config_get_ptr()->paths.directory_playlist));
   strlcpy(config_get_ptr()->paths.path_content_database, db,
         sizeof(config_get_ptr()->paths.path_content_database));
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC);
   ready = run_scan(dir) && scan_completed;
   check(ready && file_contains(lpl, "Dotted Game"),
         "automatic scan keeps a dotted database", "Tiger - Game.com");

   playlist_config_set_path(cfg, lpl);
   cfg->capacity = COLLECTION_SIZE;
   pl = playlist_init(cfg);
   ready = pl && manual_content_scan_set_menu_from_playlist(pl, db, false)
         == MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_OK;
   if (pl)
      playlist_free(pl);
   sprintf(p, "%s/after.bin", dir);
   if (!write_content(p, crc, size))
      goto fixture;
   ready = ready && run_scan(dir) && scan_completed;
   check(ready && file_contains(lpl, "after.bin"),
         "refresh keeps the dotted playlist and database", "Tiger - Game.com");
   sprintf(p, "%s/Tiger - Game.lpl", out);
   check(!path_is_valid(p), "refresh creates no truncated playlist",
         "Tiger - Game.lpl");

   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_NONE);
   strlcpy(manual_content_scan_get_system_name_custom_ptr(),
         "VTech - V.Smile", manual_content_scan_get_system_name_custom_size());
   manual_content_scan_set_menu_system_name(
         MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM, NULL);
   sprintf(p, "%s/VTech - V.Smile.lpl", out);
   filestream_delete(p);
   ready = run_scan(dir) && scan_completed;
   check(ready && file_contains(p, "after.bin"),
         "custom playlist keeps dots", "VTech - V.Smile.lpl");

   manual_content_scan_set_menu_system_name(
         MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR, NULL);
   sprintf(p, "%s/scan.dotted.lpl", out);
   filestream_delete(p);
   ready = run_scan(dir) && scan_completed;
   check(ready && file_contains(p, "after.bin"),
         "folder playlist keeps dots", "scan.dotted.lpl");

   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_STRICT);
   manual_content_scan_set_menu_scan_db_select(
         MANUAL_CONTENT_SCAN_SELECT_DB_AUTO, NULL);
   ready = manual_content_scan_get_task_config(scan, out);
   check(ready && !strcmp(scan->dat_file_path, "scan.dotted.lpl"),
         "fallback database name keeps dots", "scan.dotted.lpl");
   goto restore;

fixture:
   check(0, "fixture", "could not write the dotted-name set");
restore:
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC);
   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
   strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
         sizeof(config_get_ptr()->paths.path_content_database));
end:
   free(cfg);
   free(scan);
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

/* Checks the result the last run_scan() was handed. */
static void check_result(const char *what, unsigned added,
      unsigned duplicates, unsigned unmatched, unsigned errors, bool failed)
{
   char detail[128];

   sprintf(detail, "%u added, %u duplicates, %u unmatched, %u errors%s",
         scan_result.added, scan_result.duplicates, scan_result.unmatched,
         scan_result.errors, scan_result.failed ? ", failed" : "");
   check(   scan_completed
         && scan_result.added      == added
         && scan_result.duplicates == duplicates
         && scan_result.unmatched  == unmatched
         && scan_result.errors     == errors
         && scan_result.failed     == failed,
         what, scan_completed ? detail : "refused or timed out");
}

/* Deletes the playlists an earlier run left in @dir. */
static void delete_playlists(const char *dir)
{
   size_t              i;
   struct string_list *l = dir_list_new(dir, "lpl", false, false, false,
         false);

   if (!l)
      return;
   for (i = 0; i < l->size; i++)
      filestream_delete(l->elems[i].data);
   string_list_free(l);
}

/* A scan counts each file by what became of it, across every playlist
 * it writes: an archive is not unmatched for the "??" it prints before
 * it is expanded, and a loose scan adds what matches nothing. */
static void counts_lane(const char *root, const char *pl_dir,
      uint32_t crc_a, uint32_t sz_a, uint32_t crc_d, uint32_t sz_d)
{
   char    *dir     = (char*)malloc(512);
   char    *dat_dir = (char*)malloc(512);
   char    *m3u_dir = (char*)malloc(512);
   char    *res_pl  = (char*)malloc(512);
   char    *p       = (char*)malloc(1024);
   uint8_t *d       = (uint8_t*)malloc(sz_a > sz_d ? sz_a : sz_d);
   uint32_t i;
   FILE    *f;

   if (!dir || !dat_dir || !m3u_dir || !res_pl || !p || !d)
   {
      check(0, "counts lane", "out of memory");
      goto end;
   }

   sprintf(dir,     "%s/scan_result",     root);
   sprintf(dat_dir, "%s/scan_result_dat", root);
   sprintf(m3u_dir, "%s/scan_result_m3u", root);
   sprintf(res_pl,  "%s/scan_result_pl",  root);
   path_mkdir(dir);
   path_mkdir(dat_dir);
   path_mkdir(m3u_dir);
   path_mkdir(res_pl);

   sprintf(p, "%s/01_alpha.bin", dir);
   if (!write_content(p, crc_a, sz_a))
      goto fixture;
   sprintf(p, "%s/02_none.bin", dir);
   if (!write_content(p, 0x0BADF00Du, 1024))
      goto fixture;
   /* Matched by the CRC its member stores. */
   for (i = 0; i < sz_d - 4; i++)
      d[i] = (uint8_t)((i * 53 + 7) & 0xFF);
   crc_force(d, sz_d - 4, crc_d, d + sz_d - 4);
   sprintf(p, "%s/03_bundle.zip", dir);
   if (!write_zip(p, "inner.bin", d, sz_d, crc_d))
      goto fixture;
   /* Its member stores no CRC: expanded, and the member matches
    * nothing. */
   for (i = 0; i < sz_a - 4; i++)
      d[i] = (uint8_t)((i * 31 + 5) & 0xFF);
   crc_force(d, sz_a - 4, crc_a, d + sz_a - 4);
   sprintf(p, "%s/04_expand.zip", dir);
   if (!write_zip(p, "inner.bin", d, sz_a, 0))
      goto fixture;

   sprintf(p, "%s/01_alpha.bin", dat_dir);
   if (!write_content(p, crc_a, sz_a))
      goto fixture;
   sprintf(p, "%s/02_none.bin", dat_dir);
   if (!write_content(p, 0x0BADF00Du, 1024))
      goto fixture;
   sprintf(p, "%s/Game (Disc 1).bin", m3u_dir);
   if (!write_content(p, crc_a, sz_a))
      goto fixture;
   sprintf(p, "%s/Game (Disc 2).bin", m3u_dir);
   if (!write_content(p, crc_a, sz_a))
      goto fixture;
   sprintf(p, "%s/Game.m3u", m3u_dir);
   if (!(f = fopen(p, "w")))
      goto fixture;
   fputs("Game (Disc 1).bin\nGame (Disc 2).bin\n", f);
   fclose(f);

   sprintf(p, "%s/scan_result.dat", root);
   if (!(f = fopen(p, "w")))
      goto fixture;
   fputs("<?xml version=\"1.0\"?>\n<datafile>\n"
         "<header><name>Result</name></header>\n"
         "<game name=\"01_alpha\"><description>Alpha</description></game>\n"
         "</datafile>\n", f);
   fclose(f);

   delete_playlists(res_pl);
   strlcpy(config_get_ptr()->paths.directory_playlist, res_pl,
         sizeof(config_get_ptr()->paths.directory_playlist));

   run_scan(dir);
   check_result("a scan counts what it adds and what matches nothing",
         2, 0, 2, 0, false);
   run_scan(dir);
   check_result("a rescan counts what it already has", 0, 2, 2, 0, false);

   delete_playlists(res_pl);
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_CUSTOM);
   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_LOOSE);
   run_scan(dir);
   check_result("a loose scan adds what matches nothing", 4, 0, 0, 0, false);

   delete_playlists(res_pl);
   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_DAT_STRICT);
   strlcpy(manual_content_scan_get_dat_file_path_ptr(), p,
         manual_content_scan_get_dat_file_path_size());
   run_scan(dat_dir);
   check_result("a DAT scan counts what the DAT leaves out", 1, 0, 1, 0, false);

   manual_content_scan_get_dat_file_path_ptr()[0] = '\0';
   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_STRICT);
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC);

   /* An m3u counts once, as the one entry its discs fold into. */
   delete_playlists(res_pl);
   run_scan(m3u_dir);
   check_result("an m3u is added, not also unmatched", 1, 0, 0, 0, false);
   run_scan(m3u_dir);
   check_result("an m3u rescan is one duplicate", 0, 1, 0, 0, false);

   /* One playlist, so the loose scan's own entry for the m3u and the
    * one its discs fold into meet there. */
   delete_playlists(res_pl);
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_CUSTOM);
   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_LOOSE);
   manual_content_scan_set_menu_system_name(
         MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR, NULL);
   run_scan(m3u_dir);
   check_result("a loose m3u is added, not also a duplicate",
         1, 0, 0, 0, false);
   run_scan(m3u_dir);
   check_result("a loose m3u rescan is one duplicate", 0, 1, 0, 0, false);
   manual_content_scan_set_menu_system_name(
         MANUAL_CONTENT_SCAN_SYSTEM_NAME_AUTO, NULL);
   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_STRICT);
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC);

   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
   goto end;

fixture:
   check(0, "fixture", "could not write the counts lane");
end:
   free(dir);
   free(dat_dir);
   free(m3u_dir);
   free(res_pl);
   free(p);
   free(d);
}

#define ERR_SIZE 1536

/* A file the scan cannot read is counted as an error; the scan goes
 * on and does not fail. */
static void errors_lane(const char *root, const char *pl_dir,
      uint32_t crc_a, uint32_t sz_a)
{
   static const char m3u[] = "Missing (Disc 1).bin\n";
   char dir[512], m3u_dir[512], res_pl[512], p[1024];
   FILE *f;

   sprintf(dir,     "%s/scan_errors",     root);
   sprintf(m3u_dir, "%s/scan_errors_m3u", root);
   sprintf(res_pl,  "%s/scan_errors_pl",  root);
   path_mkdir(dir);
   path_mkdir(m3u_dir);
   path_mkdir(res_pl);
   sprintf(p, "%s/01_alpha.bin", dir);
   if (!write_content(p, crc_a, sz_a))
      goto fixture;
   sprintf(p, "%s/02_unread.bin", dir);
   if (!write_content(p, 0x0E770001u, ERR_SIZE))
      goto fixture;

   delete_playlists(res_pl);
   strlcpy(config_get_ptr()->paths.directory_playlist, res_pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   err_size = ERR_SIZE;
   run_scan(dir);
   err_size = 0;
   check_result("a file the scan cannot read is an error", 1, 0, 0, 1,
         false);

   sprintf(p, "%s/Game.m3u", m3u_dir);
   if (!(f = fopen(p, "wb")))
      goto fixture;
   fputs(m3u, f);
   fclose(f);
   err_size = sizeof(m3u) - 1;
   run_scan(m3u_dir);
   err_size = 0;
   check_result("an m3u the scan cannot read is an error, not also "
         "unmatched", 0, 0, 0, 1, false);
   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
   return;

fixture:
   check(0, "fixture", "could not write the errors lane");
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
   if (!task_push_dbscan(dir, scan_cb, NULL))
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

/* As run_scan(), but cancels once the fingerprint index build has
 * yielded.  True when the scan then ends. */
static bool run_scan_cancel_setfp(const char *dir)
{
   task_finder_data_t finder;
   bool               cancelled = false;
   time_t             started;

   loop_active    = true;
   scan_completed = false;
   if (!task_push_dbscan(dir, scan_cb, NULL))
      return false;
   finder.func     = find_scan;
   finder.userdata = NULL;
   started         = time(NULL);
   while (loop_active)
   {
      if (   !cancelled
          && retro_atomic_load_acquire_int(&setfp_yields) > 0
          && task_queue_find(&finder))
      {
         task_queue_cancel_ident(lane_ident);
         cancelled = true;
      }
      task_queue_check();
      if (difftime(time(NULL), started) > SCAN_TIMEOUT_SECONDS)
         return false;
      retro_sleep(1);
   }
   return cancelled;
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

/* A scan with no database to look in, or a DAT it cannot load, says
 * it failed. */
static void failed_lane(const char *root, const char *pl_dir,
      const char *db_dir, uint32_t crc_a, uint32_t sz_a)
{
   char *dir    = (char*)malloc(512);
   char *nodb   = (char*)malloc(512);
   char *res_pl = (char*)malloc(512);
   char *p      = (char*)malloc(1024);
   FILE *f;
   bool  refused;

   if (!dir || !nodb || !res_pl || !p)
   {
      check(0, "failed lane", "out of memory");
      goto end;
   }

   sprintf(dir,    "%s/scan_failed",      root);
   sprintf(nodb,   "%s/scan_failed_nodb", root);
   sprintf(res_pl, "%s/scan_failed_pl",   root);
   path_mkdir(dir);
   path_mkdir(res_pl);

   sprintf(p, "%s/01_alpha.bin", dir);
   if (!write_content(p, crc_a, sz_a))
      goto fixture;
   sprintf(p, "%s/scan_failed.dat", root);
   if (!(f = fopen(p, "w")))
      goto fixture;
   fputs("<?xml version=\"1.0\"?>\n"
         "<notadat><game name=\"01_alpha\"/></notadat>\n", f);
   fclose(f);

   strlcpy(config_get_ptr()->paths.directory_playlist, res_pl,
         sizeof(config_get_ptr()->paths.directory_playlist));

   use_db(nodb);
   run_scan(dir);
   check_result("a missing database folder fails the scan", 0, 0, 1, 0, true);
   check(scan_err, "and reports it as an empty folder is",
         scan_err ? "error" : "no error");
   use_db("");
   run_scan(dir);
   check_result("as does no database folder set", 0, 0, 1, 0, true);
   use_db(db_dir);
   check(scan_err, "and reports it too",
         scan_err ? "error" : "no error");

   /* Not a rescan of the folder the last scan set. */
   refused = !run_scan("/");
   check(refused, "a folder the scan settings reject is refused",
         refused ? "refused" : "scanned");

   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_CUSTOM);
   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_DAT_STRICT);
   strlcpy(manual_content_scan_get_dat_file_path_ptr(), p,
         manual_content_scan_get_dat_file_path_size());
   run_scan(dir);
   check_result("a DAT that does not load fails the scan", 0, 0, 0, 0, true);

   manual_content_scan_get_dat_file_path_ptr()[0] = '\0';
   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_STRICT);
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC);
   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
   goto end;

fixture:
   check(0, "fixture", "could not write the failed lane");
end:
   free(dir);
   free(nodb);
   free(res_pl);
   free(p);
}

/* stdout and stderr while capture_begin() has them in files. */
static int cap_out = -1;
static int cap_err = -1;

static void capture_end(void)
{
   fflush(stdout);
   fflush(stderr);
   if (cap_out >= 0)
   {
      cap_dup2(cap_out, cap_fileno(stdout));
      cap_close(cap_out);
      cap_out = -1;
   }
   if (cap_err >= 0)
   {
      cap_dup2(cap_err, cap_fileno(stderr));
      cap_close(cap_err);
      cap_err = -1;
   }
}

/* Sends stdout to @out and stderr to @err until capture_end().  The
 * scan prints to the streams themselves, so their descriptors move. */
static bool capture_begin(const char *out, const char *err)
{
   FILE *o  = fopen(out, "w");
   FILE *e  = fopen(err, "w");
   bool  ok = false;

   fflush(stdout);
   fflush(stderr);
   if (o && e)
   {
      cap_out = cap_dup(cap_fileno(stdout));
      cap_err = cap_dup(cap_fileno(stderr));
      ok      =    cap_out >= 0 && cap_err >= 0
               && cap_dup2(cap_fileno(o), cap_fileno(stdout)) >= 0
               && cap_dup2(cap_fileno(e), cap_fileno(stderr)) >= 0;
   }
   if (o)
      fclose(o);
   if (e)
      fclose(e);
   if (!ok)
      capture_end();
   return ok;
}

/* Scans @dir with stdout in @out and stderr in @err; false if the
 * streams could not be moved or the scan did not finish. */
static bool capture_scan(const char *dir, const char *out, const char *err,
      bool cli)
{
   bool captured;

   cli_output = cli;
   if ((captured = capture_begin(out, err)))
   {
      run_scan(dir);
      capture_end();
   }
   cli_output = false;
   check(captured && scan_completed, "--scan output to files",
         !captured ? out : scan_completed ? "" : "timed out");
   return captured && scan_completed;
}

/* What --scan prints, to files: the marks plain, the totals line on
 * stdout, and on stderr a file the scan could not read and a scan
 * that failed, which prints no totals.  Without --scan, nothing.
 * Rescans errors_lane()'s folder. */
static void output_lane(const char *root, const char *pl_dir,
      const char *db_dir)
{
   char *dir    = (char*)malloc(512);
   char *nodb   = (char*)malloc(512);
   char *empty  = (char*)malloc(512);
   char *res_pl = (char*)malloc(512);
   char *out    = (char*)malloc(512);
   char *err    = (char*)malloc(512);
   char *log    = (char*)malloc(512);
   char *want   = (char*)malloc(1024);
   bool  logged;

   if (!dir || !nodb || !empty || !res_pl || !out || !err || !log || !want)
   {
      check(0, "output lane", "out of memory");
      goto end;
   }

   sprintf(dir,    "%s/scan_errors",       root);
   sprintf(nodb,   "%s/scan_output_nodb",  root);
   sprintf(empty,  "%s/scan_output_empty", root);
   sprintf(res_pl, "%s/scan_errors_pl",    root);
   sprintf(out,    "%s/scan_output.out",   root);
   sprintf(err,    "%s/scan_output.err",   root);
   sprintf(log,    "%s/scan_output.log",   root);
   path_mkdir(empty);
   strlcpy(config_get_ptr()->paths.directory_playlist, res_pl,
         sizeof(config_get_ptr()->paths.directory_playlist));

   err_size = ERR_SIZE;
   if (capture_scan(dir, out, err, true))
   {
      /* No "\n" in the needles: Windows ends these lines "\r\n". */
      sprintf(want, "%s: 0 added, 1 duplicates, 0 unmatched, 1 errors",
            dir);
      check(file_contains(out, want), "--scan prints its totals on stdout",
            want);
      sprintf(want, " !! \"%s/02_unread.bin\"", dir);
      check(file_contains(err, want),
            "and a file it could not read on stderr", want);
      check(   file_contains(out, " == \"")
            && !file_contains(out, "\x1B[") && !file_contains(err, "\x1B["),
            "with no colour codes in a file", out);
      sprintf(want, "\"%s\"...", dir);
      check(file_contains(out, want),
            "and starts with the folder it scans", want);
   }

   retro_main_log_file_init(log, false);
   verbosity_enable();
   logged = capture_scan(dir, out, err, false);
   verbosity_disable();
   retro_main_log_file_deinit();
   if (logged)
   {
      check(path_get_size(out) == 0 && path_get_size(err) == 0,
            "a scan without --scan prints nothing", out);
      sprintf(want, "\"%s\"...", dir);
      check(file_contains(log, want), "but logs the folder it scans", want);
   }
   err_size = 0;

   use_db(nodb);
   if (capture_scan(dir, out, err, true))
   {
      sprintf(want, "%s: no database", dir);
      check(file_contains(err, want) && !file_contains(out, " added, "),
            "a failed scan says so on stderr, no totals", want);
   }
   use_db(db_dir);

   if (capture_scan(empty, out, err, true))
   {
      sprintf(want, "%s: invalid content", empty);
      check(file_contains(err, want) && !file_contains(out, " added, "),
            "as does a scan that cannot start", want);
   }

   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
end:
   free(dir);
   free(nodb);
   free(empty);
   free(res_pl);
   free(out);
   free(err);
   free(log);
   free(want);
}

static unsigned file_count(const char *path, const char *needle);

/* A DAT scan and one that matches nothing print a database scan's
 * lines: the folder, a line per file and a "??" for each file the
 * totals count unmatched.  Rescans counts_lane()'s folders. */
static void cli_lines_lane(const char *root, const char *pl_dir)
{
   char    *dat_dir = (char*)malloc(512);
   char    *m3u_dir = (char*)malloc(512);
   char    *res_pl  = (char*)malloc(512);
   char    *dat     = (char*)malloc(512);
   char    *out     = (char*)malloc(512);
   char    *err     = (char*)malloc(512);
   char    *want    = (char*)malloc(1024);
   char     detail[64];
   unsigned qq;

   if (!dat_dir || !m3u_dir || !res_pl || !dat || !out || !err || !want)
   {
      check(0, "lines lane", "out of memory");
      goto end;
   }

   sprintf(dat_dir, "%s/scan_result_dat", root);
   sprintf(m3u_dir, "%s/scan_result_m3u", root);
   sprintf(res_pl,  "%s/scan_result_pl",  root);
   sprintf(dat,     "%s/scan_result.dat", root);
   sprintf(out,     "%s/scan_lines.out",  root);
   sprintf(err,     "%s/scan_lines.err",  root);
   strlcpy(config_get_ptr()->paths.directory_playlist, res_pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_CUSTOM);
   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_DAT_STRICT);
   strlcpy(manual_content_scan_get_dat_file_path_ptr(), dat,
         manual_content_scan_get_dat_file_path_size());

   delete_playlists(res_pl);
   if (capture_scan(dat_dir, out, err, true))
   {
      sprintf(want, "\"%s\"...", dat_dir);
      check(file_contains(out, want),
            "lines: a DAT scan starts with its folder", want);
      check(   file_contains(out, "1/2: \"01_alpha.bin\"...")
            && file_contains(out, "2/2: \"02_none.bin\"..."),
            "lines: and prints a line per file", out);
      sprintf(want, " ?? \"%s/02_none.bin\"", dat_dir);
      check(file_contains(out, want),
            "lines: and ?? for the file it leaves out", want);
      qq = file_count(out, " ?? \"");
      sprintf(detail, "%u ??, %u unmatched", qq, scan_result.unmatched);
      check(qq == 1 && scan_result.unmatched == 1,
            "lines: one ?? per unmatched file", detail);
   }

   /* Nothing the DAT lists: two discs, then the m3u in its own pass. */
   delete_playlists(res_pl);
   if (capture_scan(m3u_dir, out, err, true))
   {
      sprintf(want, " ?? \"%s/Game.m3u\"", m3u_dir);
      qq = file_count(out, " ?? \"");
      sprintf(detail, "%u ??, %u unmatched", qq, scan_result.unmatched);
      check(   file_contains(out, want)
            && qq == 3 && scan_result.unmatched == 3,
            "lines: an unmatched m3u gets its ??", detail);
   }

   delete_playlists(res_pl);
   if (capture_scan(dat_dir, out, err, false))
      check(path_get_size(out) == 0 && path_get_size(err) == 0,
            "lines: none without --scan", out);

   manual_content_scan_get_dat_file_path_ptr()[0] = '\0';
   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_NONE);
   manual_content_scan_set_menu_system_name(
         MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR, NULL);
   delete_playlists(res_pl);
   if (capture_scan(dat_dir, out, err, true))
   {
      sprintf(want, "\"%s\"...", dat_dir);
      check(   file_contains(out, want)
            && file_contains(out, "1/2: \"01_alpha.bin\"...")
            && file_contains(out, "2/2: \"02_none.bin\"..."),
            "lines: match none prints them too", want);
      qq = file_count(out, " ?? \"");
      sprintf(detail, "%u ??, %u unmatched", qq, scan_result.unmatched);
      check(qq == 0 && scan_result.unmatched == 0,
            "lines: and no ?? with nothing unmatched", detail);
   }

   manual_content_scan_set_menu_system_name(
         MANUAL_CONTENT_SCAN_SYSTEM_NAME_AUTO, NULL);
   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_STRICT);
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC);
   delete_playlists(res_pl);
   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
end:
   free(dat_dir);
   free(m3u_dir);
   free(res_pl);
   free(dat);
   free(out);
   free(err);
   free(want);
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
   if (!task_push_dbscan(dir, scan_cb, NULL))
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
   rec->set_fp   = 0;
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
 * with no grant while a database opens zip and none of those carries
 * set fingerprints; a refused fingerprint index carries none.  A
 * TorrentZip zip of one member is not read so; plain ones only by the
 * scan, once the size lets it; neither while no database opens zip.
 * No record matches, so no whole match grants a read ahead.  The
 * fingerprint index build resumes after a yield and is freed when the
 * scan is cancelled during it. */
static void tz_reader_lane(const char *root)
{
   static uint8_t d[2][MF_MEMBER_LEN];
   static char    p[1024];
   static char    db[4][512];
   char           dir[2][512], detail[64];
   db_rec_t       arc[2], cart, fp[2];
   zip_entry_t    e[2];
   uint32_t       one_bytes;
   unsigned       k, j, walks[5];
   int            scan, rdr, one, yields;
   bool           started;

   if (!readers_expected())
   {
      check(1, "TorrentZip sets: readers read them whole", "no readers");
      return;
   }
   sprintf(db[0],  "%s/tz_reader_db",    root);
   sprintf(db[1],  "%s/tz_reader_nozip", root);
   sprintf(db[2],  "%s/tz_reader_fp",    root);
   sprintf(db[3],  "%s/tz_reader_nocm",  root);
   sprintf(dir[0], "%s/tz_reader_tz",    root);
   sprintf(dir[1], "%s/tz_reader_plain", root);
   for (j = 0; j < 2; j++)
   {
      path_mkdir(db[j]);
      path_mkdir(dir[j]);
   }
   path_mkdir(db[2]);
   path_mkdir(db[3]);
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
   /* The same records, one keyed on a fingerprint no set has. */
   fp[0]        = arc[0];
   fp[1]        = arc[1];
   fp[0].set_fp = 0x0CA701FEu;
   sprintf(p, "%s/Test Arc Gate.rdb", db[2]);
   if (!write_db_set(p, fp, 2))
      goto fixture;
   set_rec(&cart, "TZR Cart", 0x0CA701F2u, MF_MEMBER_LEN);
   sprintf(p, "%s/Test Cart Gate.rdb", db[1]);
   if (!write_db_set(p, &cart, 1))
      goto fixture;

   use_queue(true);
   for (j = 0; j < 5; j++)
   {
      bool tz = (j != 1);

      use_db(db[j == 2 ? 1 : (j > 2 ? 2 : 0)]);
      setfp_cap    = (j == 4) ? 1 : 0;
      setfp_builds = 0;
      retro_atomic_store_release_int(&reads_begun,  0);
      retro_atomic_store_release_int(&reader_begun, 0);
      retro_atomic_store_release_int(&watch_scan,   0);
      retro_atomic_store_release_int(&watch_reader, 0);
      begun_size = tz ? TZ_ZIP_LEN : ARC_ZIP_LEN;
      watch_size = one_bytes;
      started    = run_scan(dir[tz ? 0 : 1]) && scan_completed;
      setfp_cap  = 0;
      walks[j]   = setfp_builds;
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
      else if (j == 2)
         check(started && scan == 0 && rdr == 0,
               "TorrentZip sets: no zip database, no read", detail);
      else if (j == 3)
         check(started && scan == GATE_ZIPS && rdr == 0,
               "TorrentZip sets: a zip database with fingerprints, "
               "the scan reads them", detail);
      else
         check(started && scan == 0 && rdr == GATE_ZIPS,
               "TorrentZip sets: a refused fingerprint index counts "
               "as none", detail);
   }
   /* One walk for each database that opens zip, none for the other. */
   sprintf(detail, "%u %u %u %u %u walks", walks[0], walks[1], walks[2],
         walks[3], walks[4]);
   check(   walks[0] == 1 && walks[1] == 1 && walks[2] == 0
         && walks[3] == 1 && walks[4] == 1,
         "TorrentZip sets: one fingerprint index walk per zip database",
         detail);

   /* Without core match every database opens zip: a keyed one whose
    * cores open no zip keeps the readers off the TorrentZip sets. */
   cart.set_fp = 0x0CA701FDu;
   sprintf(p, "%s/Test Arc Gate.rdb", db[3]);
   if (!write_db_set(p, arc, 2))
      goto fixture;
   sprintf(p, "%s/Test Cart Gate.rdb", db[3]);
   if (!write_db_set(p, &cart, 1))
      goto fixture;
   use_db(db[3]);
   config_get_ptr()->bools.scan_without_core_match = true;
   setfp_builds = 0;
   retro_atomic_store_release_int(&reads_begun,  0);
   retro_atomic_store_release_int(&reader_begun, 0);
   begun_size   = TZ_ZIP_LEN;
   started      = run_scan(dir[0]) && scan_completed;
   begun_size   = 0;
   config_get_ptr()->bools.scan_without_core_match = false;
   rdr          = retro_atomic_load_acquire_int(&reader_begun);
   scan         = retro_atomic_load_acquire_int(&reads_begun) - rdr;
   sprintf(detail, "scan %d, readers %d, %u walks for 2", scan, rdr,
         setfp_builds);
   check(started && setfp_builds == 2,
         "without core match: one fingerprint index walk per database",
         detail);
   check(started && scan == GATE_ZIPS && rdr == 0,
         "without core match: a keyed non-zip database, the scan "
         "reads them", detail);

   use_db(db[2]);
   setfp_builds  = 0;
   setfp_yield_n = 1;
   retro_atomic_store_release_int(&setfp_yields, 0);
   retro_atomic_store_release_int(&reads_begun,  0);
   retro_atomic_store_release_int(&reader_begun, 0);
   begun_size     = TZ_ZIP_LEN;
   started        = run_scan(dir[0]) && scan_completed;
   begun_size     = 0;
   setfp_yield_n  = 0;
   setfp_job_live = NULL;
   rdr            = retro_atomic_load_acquire_int(&reader_begun);
   scan           = retro_atomic_load_acquire_int(&reads_begun) - rdr;
   yields         = retro_atomic_load_acquire_int(&setfp_yields);
   sprintf(detail, "scan %d, readers %d, %u walks, %d yields", scan, rdr,
         setfp_builds, yields);
   check(   started && scan == GATE_ZIPS && rdr == 0
         && setfp_builds == 1 && yields == 1,
         "fingerprint index build: resumed after a yield", detail);

   setfp_stall = true;
   retro_atomic_store_release_int(&setfp_yields, 0);
   started        = run_scan_cancel_setfp(dir[0]);
   setfp_stall    = false;
   setfp_job_live = NULL;
   sprintf(detail, "%d yields", retro_atomic_load_acquire_int(&setfp_yields));
   check(started, "fingerprint index build: a cancel during it ends the "
         "scan", detail);
   return;

fixture:
   zip_comment = NULL;
   setfp_cap   = 0;
   check(0, "fixture", "could not write the TorrentZip sets");
}

#define TZU_ZIPS    24
#define TZU_SLOW_MS 500

/* TorrentZip sets a zip database cannot hold by their size: once the
 * scan has kept one from a whole read, the readers stop reading the
 * next ones whole, whose CRCs the scan would drop.  Their whole reads
 * are slowed, so each reader has one under way when the scan rules on
 * the first set and may begin one more before it does: two rounds,
 * three allowed, of TZU_ZIPS without the stop.  Slowed well past what
 * the scan takes to rule: at 150 ms a sanitized scan on a loaded
 * four-core runner ruled after four rounds. */
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

/* A refresh of a playlist that saved no database name leaves the
 * database to the scan, whatever the refresh before it read. */
static void refresh_db_name_lane(const char *root)
{
   static const char lpl[] =
      "{\n"
      "  \"version\": \"1.5\",\n"
      "  \"scan_content_dir\": \"%s\",\n"
      "  \"scan_database_name\": \"%s\",\n"
      "  \"scan_search_recursively\": true,\n"
      "  \"scan_search_archives\": true,\n"
      "  \"scan_db_usage\": 0,\n"
      "  \"items\": []\n"
      "}\n";
   static char        text[2048];
   manual_content_scan_task_config_t *cfg =
      (manual_content_scan_task_config_t*)calloc(1, sizeof(*cfg));
   playlist_config_t *pl_cfg =
      (playlist_config_t*)calloc(1, sizeof(*pl_cfg));
   playlist_t        *named = NULL;
   playlist_t        *plain = NULL;
   char               db[512], dir[512], pl[512], p[1024];
   db_rec_t           rec;
   bool               ran;

   if (!cfg || !pl_cfg)
   {
      check(0, "refresh database name lane", "out of memory");
      goto end;
   }
   sprintf(db,  "%s/refresh_name_db", root);
   sprintf(dir, "%s/refresh_name",    root);
   sprintf(pl,  "%s/refresh_name_pl", root);
   path_mkdir(db);
   path_mkdir(dir);
   path_mkdir(pl);
   set_rec(&rec, "Named Cart", 0x4E414D45u, 1024);
   sprintf(p, "%s/Test Named.rdb", db);
   if (!write_db_set(p, &rec, 1))
      goto fixture;
   pl_cfg->capacity = COLLECTION_SIZE;
   sprintf(p, "%s/Named.lpl", pl);
   sprintf(text, lpl, dir, "Test Named.lpl");
   playlist_config_set_path(pl_cfg, p);
   if (   !filestream_write_file(p, text, strlen(text))
       || !(named = playlist_init(pl_cfg)))
      goto fixture;
   sprintf(p, "%s/Plain.lpl", pl);
   sprintf(text, lpl, dir, "");
   playlist_config_set_path(pl_cfg, p);
   if (   !filestream_write_file(p, text, strlen(text))
       || !(plain = playlist_init(pl_cfg)))
      goto fixture;

   /* Back to back: the second call's frame is where the first left
    * its database name. */
   ran =    manual_content_scan_set_menu_from_playlist(named, db, false)
               == MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_OK
         && manual_content_scan_set_menu_from_playlist(plain, db, false)
               == MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_OK
         && manual_content_scan_get_task_config(cfg, pl);
   check(ran && !*cfg->database_name,
         "refresh, no saved database: the scan picks",
         !ran ? "refused" : *cfg->database_name ? cfg->database_name
         : "automatic");
   ran =    manual_content_scan_set_menu_from_playlist(named, db, false)
               == MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_OK
         && manual_content_scan_get_task_config(cfg, pl);
   check(ran && !strcmp(cfg->database_name, "Test Named.lpl"),
         "refresh, a saved database: that one",
         ran ? cfg->database_name : "refused");
   goto restore;

fixture:
   check(0, "fixture", "could not write the refresh playlists");
restore:
   *manual_content_scan_get_validate_entries_ptr() = false;
   manual_content_scan_set_menu_scan_db_select(
         MANUAL_CONTENT_SCAN_SELECT_DB_AUTO, NULL);
   manual_content_scan_get_system_name_custom_ptr()[0] = '\0';
   manual_content_scan_set_menu_system_name(
         MANUAL_CONTENT_SCAN_SYSTEM_NAME_AUTO, NULL);
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC);
end:
   if (named)
      playlist_free(named);
   if (plain)
      playlist_free(plain);
   free(pl_cfg);
   free(cfg);
}

/* Skip Database References names a match after the playlist it is
 * written to, and only when the scan has one playlist: left to the
 * scan, a match goes to its database's playlist. */
static void omit_ref_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static char cart_lpl[1024], folder_lpl[1024], custom_lpl[1024];
   char        db[512], dir[512], pl[512], p[1024], what[80];
   db_rec_t    cart;
   bool        started;
   const char *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,  "%s/omit_ref_db", root);
   sprintf(dir, "%s/omit_ref",    root);
   sprintf(pl,  "%s/omit_ref_pl", root);
   path_mkdir(db);
   path_mkdir(dir);
   path_mkdir(pl);
   sprintf(p, "%s/om.ct", dir);
   if (!write_content(p, 0x0A1E0F01u, 1024))
      goto fixture;
   set_rec(&cart, "Omit Cart", 0x0A1E0F01u, 1024);
   sprintf(p, "%s/Test Cart Gate.rdb", db);
   if (!write_db_set(p, &cart, 1))
      goto fixture;

   sprintf(cart_lpl,   "%s/Test Cart Gate.lpl", pl);
   sprintf(folder_lpl, "%s/omit_ref.lpl",       pl);
   sprintf(custom_lpl, "%s/Omit Custom.lpl",    pl);
   filestream_delete(cart_lpl);
   filestream_delete(folder_lpl);
   use_queue(threaded);
   use_db(db);
   strlcpy(config_get_ptr()->paths.directory_playlist, pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   *manual_content_scan_get_omit_db_ref_ptr() = true;

   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_CUSTOM);
   started = run_scan(dir) && scan_completed;
   sprintf(what, "omit ref, Automatic target %s", q);
   check(   started && file_contains(cart_lpl, "om.ct")
         && file_contains(cart_lpl, "\"db_name\": \"Test Cart Gate.lpl\"")
         && !path_is_valid(folder_lpl), what, "Test Cart Gate.lpl");

   /* Fully Automatic keeps the value Custom set. */
   filestream_delete(cart_lpl);
   filestream_delete(folder_lpl);
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC);
   started = run_scan(dir) && scan_completed;
   sprintf(what, "omit ref, Fully Automatic %s", q);
   check(   started && file_contains(cart_lpl, "om.ct")
         && file_contains(cart_lpl, "\"db_name\": \"Test Cart Gate.lpl\"")
         && !path_is_valid(folder_lpl), what, "Test Cart Gate.lpl");

   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_CUSTOM);
   strlcpy(manual_content_scan_get_system_name_custom_ptr(),
         "Omit Custom",
         manual_content_scan_get_system_name_custom_size());
   manual_content_scan_set_menu_system_name(
         MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM, NULL);
   filestream_delete(custom_lpl);
   started = run_scan(dir) && scan_completed;
   sprintf(what, "omit ref, Custom target: its name %s", q);
   check(   started
         && file_contains(custom_lpl, "\"db_name\": \"Omit Custom.lpl\""),
         what, "Omit Custom.lpl");

   /* A loose scan's non-match too, not the folder's name. */
   sprintf(p, "%s/om_none.ct", dir);
   if (!write_content(p, 0x0A1E0F02u, 1024))
      goto fixture;
   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_LOOSE);
   filestream_delete(custom_lpl);
   started = run_scan(dir) && scan_completed;
   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_STRICT);
   filestream_delete(p);
   sprintf(what, "omit ref, Custom target, loose: a non-match %s", q);
   check(   started && file_contains(custom_lpl, "om_none.ct")
         && file_contains(custom_lpl, "om.ct")
         && file_count(custom_lpl, "\"db_name\": \"Omit Custom.lpl\"") == 2
         && !file_contains(custom_lpl, "\"db_name\": \"omit_ref.lpl\"")
         && !file_contains(custom_lpl, "\"db_name\": \"Test Cart Gate"),
         what, custom_lpl);

   *manual_content_scan_get_omit_db_ref_ptr() = false;
   filestream_delete(custom_lpl);
   started = run_scan(dir) && scan_completed;
   sprintf(what, "omit ref off: the database's name %s", q);
   check(   started
         && file_contains(custom_lpl, "\"db_name\": \"Test Cart Gate.lpl\""),
         what, "Test Cart Gate.lpl");

   manual_content_scan_get_system_name_custom_ptr()[0] = '\0';
   manual_content_scan_set_menu_system_name(
         MANUAL_CONTENT_SCAN_SYSTEM_NAME_AUTO, NULL);
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC);
   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
   return;

fixture:
   check(0, "fixture", "could not write the omit ref set");
}

#ifdef HAVE_RRAR
/* Rar archives take the archive passes: a first member's hit, in a
 * database whose core opens rar, names the archive; a later member's,
 * in one whose core does not, is held; an archive of several members
 * is read whole only while a database whose cores open rar holds a rar
 * record of its size.  A fingerprint in rar writes nothing where no
 * core opens rar.  A solid archive whose members the reader
 * cannot unpack has no fingerprint, not that of the one it can; a
 * folder leaves it as it is. */
static void rar_lane(const char *root, const char *pl_dir, bool threaded)
{
   static uint8_t d[8][MF_MEMBER_LEN];
   static const uint32_t crc[8] = {
      0x0BF80001u, 0x0BF80011u, 0x0BF80012u, 0x0BF80002u, 0x0BF80021u,
      0x0BF80022u, 0x0BF80031u, 0x0BF80032u };
   char           db[512], noclaim[512], dir[512], p[1024];
   static char    cart_lpl[1024], member_lpl[1024], nc_lpl[1024];
   char           what[64], detail[64];
   uint8_t        be[8];
   db_rec_t       cart[6], held, nc[2];
   zip_entry_t    e[3];
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
   for (j = 0; j < 8; j++)
      fill_forced(d[j], MF_MEMBER_LEN, 101 + 2 * j, crc[j]);
   memset(e, 0, sizeof(e));
   e[0].len = MF_MEMBER_LEN;
   e[1].len = MF_MEMBER_LEN;

   e[0].name = "m.rd";
   e[0].data = d[0];
   e[0].crc  = crc[0];
   sprintf(p, "%s/rar_cart.rar", dir);
   if (!write_rar_entries(p, e, 1, false))
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
   if (!write_rar_entries(p, e, 2, false))
      goto fixture;
   multi_bytes = rar_bytes(e, 2);
   /* A rar record of its size: the size gate lets it be read whole. */
   set_rec(&cart[1], "RAR Multi Sized", 0x0BF8FFFEu, multi_bytes);
   cart[1].rom_name = "set.rar";
   set_rec(&held, "RAR Held", crc[2], MF_MEMBER_LEN);
   /* A rar record of that size whose core does not open rar. */
   set_rec(&nc[1], "RAR NoClaim Sized", 0x0BF8FFFCu, multi_bytes);
   nc[1].rom_name = "set.rar";

   /* A folder is no member: its one member's hit names it. */
   e[0].name = "d/";
   e[0].data = NULL;
   e[0].len  = 0;
   e[0].crc  = 0;
   e[1].name = "d/m.rd";
   e[1].data = d[3];
   e[1].crc  = crc[3];
   sprintf(p, "%s/rar_folder.rar", dir);
   if (   !write_rar_entries(p, e, 2, false)
       || path_get_size(p) != (int64_t)rar_bytes(e, 2))
      goto fixture;
   set_rec(&cart[2], "RAR Folder", crc[3], MF_MEMBER_LEN);

   /* A set whose fingerprint a database whose core opens rar holds. */
   e[0].name = "A.ROM";
   e[0].data = d[4];
   e[0].len  = MF_MEMBER_LEN;
   e[0].crc  = crc[4];
   e[1].name = "B.ROM";
   e[1].data = d[5];
   e[1].crc  = crc[5];
   sprintf(p, "%s/rar_fp.rar", dir);
   if (!write_rar_entries(p, e, 2, false))
      goto fixture;
   set_rec(&cart[3], "RAR FP Set", 0x0BF8FFFFu, 2 * MF_MEMBER_LEN);
   for (j = 0; j < 4; j++)
   {
      be[j]     = (uint8_t)(crc[4] >> (24 - 8 * j));
      be[4 + j] = (uint8_t)(crc[5] >> (24 - 8 * j));
   }
   cart[3].set_fp = crc_of(be, sizeof(be));
   nc[0]          = cart[3];

   /* Solid, its second member packed: the record holds the
    * fingerprint of the first alone. */
   sprintf(p, "%s/rar_solid.rar", dir);
   if (!write_rar_entries(p, e, 2, true))
      goto fixture;
   set_rec(&cart[4], "RAR Solid Set", 0x0BF8FFFDu, MF_MEMBER_LEN);
   cart[4].set_fp = crc_of(be, 4);

   /* A folder among the members. */
   e[0].name = "sub/";
   e[0].data = NULL;
   e[0].len  = 0;
   e[0].crc  = 0;
   e[1].name = "sub/C.ROM";
   e[1].data = d[6];
   e[1].crc  = crc[6];
   e[2].name = "D.ROM";
   e[2].data = d[7];
   e[2].len  = MF_MEMBER_LEN;
   e[2].crc  = crc[7];
   sprintf(p, "%s/rar_fpdir.rar", dir);
   if (!write_rar_entries(p, e, 3, false))
      goto fixture;
   set_rec(&cart[5], "RAR FP Dir Set", 0x0BF8FFFBu, 2 * MF_MEMBER_LEN);
   for (j = 0; j < 4; j++)
   {
      be[j]     = (uint8_t)(crc[6] >> (24 - 8 * j));
      be[4 + j] = (uint8_t)(crc[7] >> (24 - 8 * j));
   }
   cart[5].set_fp = crc_of(be, sizeof(be));

   sprintf(p, "%s/Test RAR Cart.rdb", db);
   if (!write_db_set(p, cart, 6))
      goto fixture;
   sprintf(p, "%s/Test EM Member.rdb", db);
   if (!write_db_set(p, &held, 1))
      goto fixture;
   sprintf(p, "%s/Test EM Member.rdb", noclaim);
   if (!write_db_set(p, &held, 1))
      goto fixture;
   sprintf(p, "%s/Test EM Cart.rdb", noclaim);
   if (!write_db_set(p, nc, 2))
      goto fixture;

   sprintf(cart_lpl,   "%s/Test RAR Cart.lpl",  pl_dir);
   sprintf(member_lpl, "%s/Test EM Member.lpl", pl_dir);
   sprintf(nc_lpl,     "%s/Test EM Cart.lpl",   pl_dir);
   filestream_delete(cart_lpl);
   filestream_delete(member_lpl);
   filestream_delete(nc_lpl);
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
   sprintf(what, "rar: a fingerprint is a hit where a core opens rar %s", q);
   check(   started && file_contains(cart_lpl, "RAR FP Set")
         && file_contains(cart_lpl, "rar_fp.rar")
         && !file_contains(cart_lpl, "rar_fp.rar#"), what, "rar_fp.rar");
   sprintf(what, "rar: solid, no fingerprint of one member %s", q);
   check(started && !file_contains(cart_lpl, "RAR Solid Set"), what,
         "rar_solid.rar");
   sprintf(what, "rar: a directory entry keeps the fingerprint %s", q);
   check(   started && file_contains(cart_lpl, "RAR FP Dir Set")
         && file_contains(cart_lpl, "rar_fpdir.rar"), what,
         "rar_fpdir.rar");
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
   filestream_delete(nc_lpl);
   use_db(noclaim);
   retro_atomic_store_release_int(&reads_begun, 0);
   begun_size  = multi_bytes;
   started     = run_scan(dir) && scan_completed;
   begun_size  = 0;
   multi_reads = retro_atomic_load_acquire_int(&reads_begun);
   sprintf(what, "rar: no core opens rar: held %s", q);
   check(started && file_contains(member_lpl, "rar_multi.rar#b.rd"),
         what, "rar_multi.rar#b.rd");
   sprintf(what, "rar: a fingerprint in rar writes nothing %s", q);
   check(started && !file_contains(nc_lpl, "RAR FP Set"), what,
         "rar_fp.rar");
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
   if (!task_push_dbscan(dir, scan_cb, NULL))
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
 * own CRC and fingerprint in one a core listing zip claims: held as
 * archive#member either way. */
static void dual_match_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static const char *const pl[2] = { "Test Cart Gate", "Test Arc Gate" };
   static const uint8_t     be[4] = { 0x0C, 0xA7, 0x00, 0x40 };
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
   rec[1].set_fp = crc_of(be, sizeof(be));
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

#ifdef HAVE_7ZIP
/* A 7z set's fingerprint is a hit where a core opens 7z, and stays one
 * with a directory entry among the members. */
static void sevenzip_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static uint8_t d[4][MF_MEMBER_LEN];
   static const uint32_t crc[4] = {
      0x0BFB0001u, 0x0BFB0002u, 0x0BFB0011u, 0x0BFB0012u };
   static char    lpl[1024];
   char           db[512], dir[512], p[1024], what[64];
   uint8_t        be[8];
   db_rec_t       rec[2];
   zip_entry_t    e[3];
   bool           started;
   unsigned       j, k;
   const char    *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,  "%s/7z_db", root);
   sprintf(dir, "%s/7z",    root);
   path_mkdir(db);
   path_mkdir(dir);
   for (j = 0; j < 4; j++)
      fill_forced(d[j], MF_MEMBER_LEN, 131 + 2 * j, crc[j]);

   memset(e, 0, sizeof(e));
   e[0].name = "A.ROM";
   e[0].data = d[0];
   e[0].len  = MF_MEMBER_LEN;
   e[0].crc  = crc[0];
   e[1].name = "B.ROM";
   e[1].data = d[1];
   e[1].len  = MF_MEMBER_LEN;
   e[1].crc  = crc[1];
   sprintf(p, "%s/7z_fp.7z", dir);
   if (!write_7z_entries(p, e, 2))
      goto fixture;

   e[0].name = "sub/";
   e[0].data = NULL;
   e[0].len  = 0;
   e[0].crc  = 0;
   e[1].name = "sub/C.ROM";
   e[1].data = d[2];
   e[1].crc  = crc[2];
   e[2].name = "D.ROM";
   e[2].data = d[3];
   e[2].len  = MF_MEMBER_LEN;
   e[2].crc  = crc[3];
   sprintf(p, "%s/7z_fpdir.7z", dir);
   if (!write_7z_entries(p, e, 3))
      goto fixture;

   set_rec(&rec[0], "7Z FP Set",     0x0BFBFFFFu, 2 * MF_MEMBER_LEN);
   set_rec(&rec[1], "7Z FP Dir Set", 0x0BFBFFFEu, 2 * MF_MEMBER_LEN);
   for (j = 0; j < 2; j++)
   {
      for (k = 0; k < 4; k++)
      {
         be[k]     = (uint8_t)(crc[2 * j]     >> (24 - 8 * k));
         be[4 + k] = (uint8_t)(crc[2 * j + 1] >> (24 - 8 * k));
      }
      rec[j].set_fp = crc_of(be, sizeof(be));
   }
   sprintf(p, "%s/Test 7Z Cart.rdb", db);
   if (!write_db_set(p, rec, 2))
      goto fixture;

   sprintf(lpl, "%s/Test 7Z Cart.lpl", pl_dir);
   filestream_delete(lpl);
   use_queue(threaded);
   use_db(db);
   started = run_scan(dir) && scan_completed;
   sprintf(what, "7z: a fingerprint is a hit where a core opens 7z %s", q);
   check(   started && file_contains(lpl, "7Z FP Set")
         && file_contains(lpl, "7z_fp.7z")
         && !file_contains(lpl, "7z_fp.7z#"), what, "7z_fp.7z");
   sprintf(what, "7z: a directory entry keeps the fingerprint %s", q);
   check(   started && file_contains(lpl, "7Z FP Dir Set")
         && file_contains(lpl, "7z_fpdir.7z")
         && !file_contains(lpl, "7z_fpdir.7z#"), what, "7z_fpdir.7z");
   return;

fixture:
   check(0, "fixture", "could not write the 7z set");
}
#endif

/* A set packed two ways: TorrentZip-like, and with other names, order
 * and a duplicate, as a zstd or 7z repack differs.  The record holds
 * the first's CRC, the second's size and the fingerprint of its member
 * CRCs, so the second matches by fingerprint, from its one directory
 * read and not read whole, and is written as the first would be.  So
 * do repacks with a directory first, or a cue or gdi sheet or an empty
 * member beside the ROMs; one with a member that stores no CRC has no
 * fingerprint.  Without the key nothing matches. */
static void set_fp_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static const char cue[] = "FILE \"A.ROM\" BINARY\n";
   static const char gdi[] = "1\n1 0 4 2352 \"A.ROM\" 0\n";
   static uint8_t d[2][MF_MEMBER_LEN];
   static char    p[1024], lpl[1024];
   char           db[512], nofp[512], dir[512];
   char           what[64], detail[64], crc[16];
   uint8_t        be[8];
   db_rec_t       rec;
   zip_entry_t    e[3];
   uint32_t       repack_bytes, dirfirst_bytes;
   int            reads, stored, dir_reads;
   bool           started;
   const char    *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,   "%s/set_fp_db",   root);
   sprintf(nofp, "%s/set_fp_nofp", root);
   sprintf(dir,  "%s/set_fp",      root);
   path_mkdir(db);
   path_mkdir(nofp);
   path_mkdir(dir);

   fill_forced(d[0], MF_MEMBER_LEN, 59, 0x5E7F0002u);
   fill_forced(d[1], MF_MEMBER_LEN, 61, 0x5E7F0001u);

   /* The packing the record's CRC was taken from: not scanned. */
   memset(e, 0, sizeof(e));
   e[0].name = "a.rd";
   e[0].data = d[0];
   e[0].len  = MF_MEMBER_LEN;
   e[0].crc  = 0x5E7F0002u;
   e[1].name = "b.rd";
   e[1].data = d[1];
   e[1].len  = MF_MEMBER_LEN;
   e[1].crc  = 0x5E7F0001u;
   sprintf(p, "%s/set_fp_tz.zip", root);
   if (   !write_zip_entries(p, e, 2)
       || !set_whole_rec(&rec, "FP Set", p))
      goto fixture;

   e[0].name = "B.ROM";
   e[0].data = d[1];
   e[0].crc  = 0x5E7F0001u;
   e[1].name = "A.ROM";
   e[1].data = d[0];
   e[1].crc  = 0x5E7F0002u;
   e[2]      = e[1];
   e[2].name = "copy/A.ROM";
   sprintf(p, "%s/fp_repack.zip", dir);
   if (!write_zip_entries(p, e, 3))
      goto fixture;
   repack_bytes = zip_bytes(e, 3);
   rec.size     = repack_bytes;

   /* A directory first. */
   e[0].name = "d/";
   e[0].data = NULL;
   e[0].len  = 0;
   e[0].crc  = 0;
   e[1].name = "d/A.ROM";
   e[1].data = d[0];
   e[1].len  = MF_MEMBER_LEN;
   e[1].crc  = 0x5E7F0002u;
   e[2].name = "d/B.ROM";
   e[2].data = d[1];
   e[2].len  = MF_MEMBER_LEN;
   e[2].crc  = 0x5E7F0001u;
   sprintf(p, "%s/fp_dirfirst.zip", dir);
   if (!write_zip_entries(p, e, 3))
      goto fixture;
   dirfirst_bytes = zip_bytes(e, 3);

   /* A cue sheet beside the ROMs. */
   e[0].name = "A.ROM";
   e[0].data = d[0];
   e[0].len  = MF_MEMBER_LEN;
   e[0].crc  = 0x5E7F0002u;
   e[1].name = "B.ROM";
   e[1].data = d[1];
   e[1].crc  = 0x5E7F0001u;
   e[2].name = "set.cue";
   e[2].data = (const uint8_t*)cue;
   e[2].len  = (uint32_t)(sizeof(cue) - 1);
   e[2].crc  = crc_of((const uint8_t*)cue, sizeof(cue) - 1);
   sprintf(p, "%s/fp_cue.zip", dir);
   if (!write_zip_entries(p, e, 3))
      goto fixture;

   /* A member that stores no CRC. */
   e[2].name = "C.ROM";
   e[2].data = d[0];
   e[2].len  = MF_MEMBER_LEN;
   e[2].crc  = 0;
   sprintf(p, "%s/fp_zero.zip", dir);
   if (!write_zip_entries(p, e, 3))
      goto fixture;

   /* An empty member beside the ROMs: its stored CRC is 0, but it is
    * no input. */
   e[2].name = "E.ROM";
   e[2].data = NULL;
   e[2].len  = 0;
   e[2].crc  = 0;
   sprintf(p, "%s/fp_empty.zip", dir);
   if (!write_zip_entries(p, e, 3))
      goto fixture;

   /* A gdi sheet beside the ROMs. */
   e[2].name = "set.gdi";
   e[2].data = (const uint8_t*)gdi;
   e[2].len  = (uint32_t)(sizeof(gdi) - 1);
   e[2].crc  = crc_of((const uint8_t*)gdi, sizeof(gdi) - 1);
   sprintf(p, "%s/fp_gdi.zip", dir);
   if (!write_zip_entries(p, e, 3))
      goto fixture;

   sprintf(p, "%s/Test FP Arcade.rdb", nofp);
   if (!write_db_set(p, &rec, 1))
      goto fixture;
   be[0] = 0x5E; be[1] = 0x7F; be[2] = 0x00; be[3] = 0x01;
   be[4] = 0x5E; be[5] = 0x7F; be[6] = 0x00; be[7] = 0x02;
   rec.set_fp = crc_of(be, sizeof(be));
   sprintf(p, "%s/Test FP Arcade.rdb", db);
   if (!write_db_set(p, &rec, 1))
      goto fixture;

   sprintf(lpl, "%s/Test FP Arcade.lpl", pl_dir);
   sprintf(crc, "%08X|crc", (unsigned)rec.crc);
   use_queue(threaded);

   filestream_delete(lpl);
   use_db(db);
   retro_atomic_store_release_int(&reads_begun, 0);
   retro_atomic_store_release_int(&stored_reads, 0);
   retro_atomic_store_release_int(&watch_scan, 0);
   retro_atomic_store_release_int(&watch_reader, 0);
   begun_size  = repack_bytes;
   watch_size  = dirfirst_bytes;
   stored_name = "fp_repack.zip";
   started     = run_scan(dir) && scan_completed;
   begun_size  = 0;
   watch_size  = 0;
   stored_name = NULL;
   reads       = retro_atomic_load_acquire_int(&reads_begun);
   stored      = retro_atomic_load_acquire_int(&stored_reads);
   dir_reads   = retro_atomic_load_acquire_int(&watch_scan)
      + retro_atomic_load_acquire_int(&watch_reader);
   sprintf(what, "repacked set matched by fingerprint %s", q);
   check(   started && file_contains(lpl, "FP Set")
         && file_contains(lpl, crc)
         && file_contains(lpl, "fp_repack.zip")
         && !file_contains(lpl, "fp_repack.zip#"), what,
         "fp_repack.zip, the record's CRC");
   if (!threaded)
   {
      sprintf(detail, "%d whole reads", reads);
      check(started && reads == 0,
            "a fingerprint match reads no archive whole", detail);
   }
   sprintf(detail, "%d directory reads", stored);
   sprintf(what, "fingerprint from the one directory read %s", q);
   check(started && stored == 1, what, detail);
   sprintf(what, "directory first: matched by fingerprint %s", q);
   check(started && file_contains(lpl, "fp_dirfirst.zip"), what,
         "fp_dirfirst.zip");
   sprintf(detail, "%d whole reads", dir_reads);
   sprintf(what, "directory first: not read whole %s", q);
   check(started && dir_reads == 0, what, detail);
   sprintf(what, "a cue sheet is left out of it %s", q);
   check(started && file_contains(lpl, "fp_cue.zip"), what, "fp_cue.zip");
   sprintf(what, "a gdi sheet is left out of it %s", q);
   check(started && file_contains(lpl, "fp_gdi.zip"), what, "fp_gdi.zip");
   sprintf(what, "an empty member is left out of it %s", q);
   check(started && file_contains(lpl, "fp_empty.zip"), what,
         "fp_empty.zip");
   sprintf(what, "a member with no stored CRC: none %s", q);
   check(started && !file_contains(lpl, "fp_zero.zip"), what,
         "fp_zero.zip");

   filestream_delete(lpl);
   use_db(nofp);
   started = run_scan(dir) && scan_completed;
   sprintf(what, "no fingerprint key: no match %s", q);
   check(started && !file_contains(lpl, "FP Set"), what,
         "fp_repack.zip");
   return;

fixture:
   check(0, "fixture", "could not write the set fingerprint set");
}

/* The set fingerprint of members @a and @b, @a < @b. */
static uint32_t set_fp_of(uint32_t a, uint32_t b)
{
   uint8_t  be[8];
   unsigned k;

   for (k = 0; k < 4; k++)
   {
      be[k]     = (uint8_t)(a >> (24 - 8 * k));
      be[k + 4] = (uint8_t)(b >> (24 - 8 * k));
   }
   return crc_of(be, sizeof(be));
}

/* Records that share a fingerprint, as a set's split and non-merged
 * packings do.  The one whose size is the file's is taken unread, even
 * over one named for the archive; with that size on two the whole read
 * decides, and with it on none, a repack, the one named for the
 * archive, else the first. */
static void fp_size_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static uint8_t d[2][MF_MEMBER_LEN];
   static char    p[1024], lpl[1024];
   char           db[512], dir[512], tie_dir[512], name_dir[512];
   char           what[64], detail[64], crc[9][16];
   db_rec_t       rec[9];
   zip_entry_t    e[2];
   uint32_t       pick_bytes, tie_bytes, repack_bytes;
   int            reads;
   bool           started;
   unsigned       k;
   const char    *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,       "%s/fp_size_db",   root);
   sprintf(dir,      "%s/fp_size",      root);
   sprintf(tie_dir,  "%s/fp_size_tie",  root);
   sprintf(name_dir, "%s/fp_size_name", root);
   path_mkdir(db);
   path_mkdir(dir);
   path_mkdir(tie_dir);
   path_mkdir(name_dir);
   memset(e, 0, sizeof(e));
   e[0].data = d[0];
   e[0].len  = MF_MEMBER_LEN;
   e[1].data = d[1];
   e[1].len  = MF_MEMBER_LEN;

   zip_comment = TZ_COMMENT;
   fill_forced(d[0], MF_MEMBER_LEN, 67, 0x51A5E011u);
   fill_forced(d[1], MF_MEMBER_LEN, 71, 0x51A5E012u);
   e[0].name = "a.rd";
   e[0].crc  = 0x51A5E011u;
   e[1].name = "b.rd";
   e[1].crc  = 0x51A5E012u;
   sprintf(p, "%s/fs_pick.zip", dir);
   if (   !write_zip_entries(p, e, 2)
       || !set_whole_rec(&rec[1], "FS Beta", p))
      goto fixture;
   pick_bytes = zip_bytes(e, 2);
   set_rec(&rec[0], "FS Alpha", 0x51A5F001u, pick_bytes + 100);
   set_rec(&rec[8], "FS Kappa", 0x51A5F009u, pick_bytes + 200);
   rec[0].rom_name = "set.zip";
   rec[8].rom_name = "fs_pick.zip";
   rec[0].set_fp   = rec[1].set_fp = rec[8].set_fp
                   = set_fp_of(0x51A5E011u, 0x51A5E012u);

   fill_forced(d[0], MF_MEMBER_LEN, 73, 0x51A5E021u);
   fill_forced(d[1], MF_MEMBER_LEN, 79, 0x51A5E022u);
   e[0].name = "tie_a.rd";
   e[0].crc  = 0x51A5E021u;
   e[1].name = "tie_b.rd";
   e[1].crc  = 0x51A5E022u;
   sprintf(p, "%s/fs_tie.zip", tie_dir);
   if (   !write_zip_entries(p, e, 2)
       || !set_whole_rec(&rec[3], "FS Delta", p))
      goto fixture;
   tie_bytes = zip_bytes(e, 2);
   set_rec(&rec[2], "FS Gamma", 0x51A5F003u, tie_bytes);
   rec[2].rom_name = "set.zip";
   rec[2].set_fp   = rec[3].set_fp = set_fp_of(0x51A5E021u, 0x51A5E022u);
   zip_comment = NULL;

   fill_forced(d[0], MF_MEMBER_LEN, 83, 0x51A5E031u);
   fill_forced(d[1], MF_MEMBER_LEN, 89, 0x51A5E032u);
   e[0].name = "B.ROM";
   e[0].data = d[1];
   e[0].crc  = 0x51A5E032u;
   e[1].name = "A.ROM";
   e[1].data = d[0];
   e[1].crc  = 0x51A5E031u;
   sprintf(p, "%s/fs_repack.zip", dir);
   if (!write_zip_entries(p, e, 2))
      goto fixture;
   repack_bytes = zip_bytes(e, 2);
   set_rec(&rec[4], "FS Epsilon", 0x51A5F005u, repack_bytes + 10);
   set_rec(&rec[5], "FS Zeta",    0x51A5F006u, repack_bytes + 20);
   rec[4].rom_name = rec[5].rom_name = "set.zip";
   rec[4].set_fp   = rec[5].set_fp = set_fp_of(0x51A5E031u, 0x51A5E032u);

   fill_forced(d[0], MF_MEMBER_LEN, 97, 0x51A5E041u);
   fill_forced(d[1], MF_MEMBER_LEN, 101, 0x51A5E042u);
   e[0].name = "a.rd";
   e[0].data = d[0];
   e[0].crc  = 0x51A5E041u;
   e[1].name = "b.rd";
   e[1].data = d[1];
   e[1].crc  = 0x51A5E042u;
   sprintf(p, "%s/fs_theta.zip", name_dir);
   if (!write_zip_entries(p, e, 2))
      goto fixture;
   sprintf(p, "%s/fs_iota.zip", dir);
   if (!write_zip_entries(p, e, 2))
      goto fixture;
   repack_bytes = zip_bytes(e, 2);
   set_rec(&rec[6], "FS Eta",   0x51A5F007u, repack_bytes + 10);
   set_rec(&rec[7], "FS Theta", 0x51A5F008u, repack_bytes + 20);
   rec[6].rom_name = "fs_eta.zip";
   rec[7].rom_name = "FS_Theta.zip";
   rec[6].set_fp   = rec[7].set_fp = set_fp_of(0x51A5E041u, 0x51A5E042u);

   sprintf(p, "%s/Test FP Arcade.rdb", db);
   if (!write_db_set(p, rec, 9))
      goto fixture;
   for (k = 0; k < 9; k++)
      sprintf(crc[k], "%08X|crc", (unsigned)rec[k].crc);

   sprintf(lpl, "%s/Test FP Arcade.lpl", pl_dir);
   use_queue(threaded);
   use_db(db);

   filestream_delete(lpl);
   retro_atomic_store_release_int(&reads_begun, 0);
   begun_size = pick_bytes;
   started    = run_scan(dir) && scan_completed;
   begun_size = 0;
   reads      = retro_atomic_load_acquire_int(&reads_begun);
   sprintf(what, "fingerprint tie: the size picks the record %s", q);
   check(   started && file_contains(lpl, "FS Beta")
         && file_contains(lpl, crc[1]) && !file_contains(lpl, crc[0])
         && !file_contains(lpl, crc[8])
         && file_contains(lpl, "fs_pick.zip")
         && !file_contains(lpl, "fs_pick.zip#"), what, "fs_pick.zip");
   sprintf(detail, "%d whole reads", reads);
   sprintf(what, "fingerprint tie: picked unread %s", q);
   check(started && reads == 0, what, detail);
   sprintf(what, "fingerprint tie: a repack takes the first %s", q);
   check(   started && file_contains(lpl, "FS Epsilon")
         && file_contains(lpl, crc[4]) && !file_contains(lpl, crc[5])
         && file_contains(lpl, "fs_repack.zip")
         && !file_contains(lpl, "fs_repack.zip#"), what, "fs_repack.zip");
   sprintf(what, "fingerprint tie: named for neither, the first %s", q);
   check(   started && file_contains(lpl, "FS Eta")
         && file_contains(lpl, crc[6]) && !file_contains(lpl, crc[7])
         && file_contains(lpl, "fs_iota.zip")
         && !file_contains(lpl, "fs_iota.zip#"), what, "fs_iota.zip");

   filestream_delete(lpl);
   retro_atomic_store_release_int(&reads_begun, 0);
   begun_size = tie_bytes;
   started    = run_scan(tie_dir) && scan_completed;
   begun_size = 0;
   reads      = retro_atomic_load_acquire_int(&reads_begun);
   sprintf(what, "fingerprint tie: one size, the whole read %s", q);
   check(   started && file_contains(lpl, "FS Delta")
         && file_contains(lpl, crc[3]) && !file_contains(lpl, "FS Gamma")
         && file_contains(lpl, "fs_tie.zip")
         && !file_contains(lpl, "fs_tie.zip#"), what, "fs_tie.zip");
   sprintf(detail, "%d whole reads", reads);
   sprintf(what, "fingerprint tie: read whole once %s", q);
   check(started && reads == 1, what, detail);

   filestream_delete(lpl);
   started = run_scan(name_dir) && scan_completed;
   sprintf(what, "fingerprint tie: the name picks the record %s", q);
   check(   started && file_contains(lpl, "FS Theta")
         && file_contains(lpl, crc[7]) && !file_contains(lpl, crc[6])
         && file_contains(lpl, "fs_theta.zip")
         && !file_contains(lpl, "fs_theta.zip#"), what, "fs_theta.zip");
   filestream_delete(lpl);
   return;

fixture:
   zip_comment = NULL;
   check(0, "fixture", "could not write the fingerprint tie sets");
}

/* One member whose CRC no database holds: its fingerprint is looked up
 * before its size lets it be read whole. */
static void fp_one_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static uint8_t d[MF_MEMBER_LEN];
   static char    lpl[1024];
   char           db[512], dir[512], p[1024];
   char           what[64], detail[64], crc[16];
   uint8_t        be[4];
   db_rec_t       rec;
   zip_entry_t    e;
   uint32_t       bytes;
   int            reads;
   bool           started;
   unsigned       k;
   const char    *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,  "%s/fp_one_db", root);
   sprintf(dir, "%s/fp_one",    root);
   path_mkdir(db);
   path_mkdir(dir);
   fill_forced(d, MF_MEMBER_LEN, 171, 0x0CA70301u);
   memset(&e, 0, sizeof(e));
   e.name = "solo.rd";
   e.data = d;
   e.len  = MF_MEMBER_LEN;
   e.crc  = 0x0CA70301u;
   sprintf(p, "%s/fp_one.zip", dir);
   if (!write_zip_entries(p, &e, 1))
      goto fixture;
   bytes = zip_bytes(&e, 1);
   /* The zip's size, which lets it be read whole, but not its CRC. */
   set_rec(&rec, "FP One", 0x0CA703FFu, bytes);
   rec.rom_name = "fp_one.zip";
   for (k = 0; k < 4; k++)
      be[k] = (uint8_t)(0x0CA70301u >> (24 - 8 * k));
   rec.set_fp = crc_of(be, sizeof(be));
   sprintf(p, "%s/Test FP Arcade.rdb", db);
   if (!write_db_set(p, &rec, 1))
      goto fixture;

   sprintf(lpl, "%s/Test FP Arcade.lpl", pl_dir);
   sprintf(crc, "%08X|crc", (unsigned)rec.crc);
   filestream_delete(lpl);
   use_queue(threaded);
   use_db(db);
   retro_atomic_store_release_int(&reads_begun, 0);
   begun_size = bytes;
   started    = run_scan(dir) && scan_completed;
   begun_size = 0;
   reads      = retro_atomic_load_acquire_int(&reads_begun);
   sprintf(what, "one member: matched by fingerprint %s", q);
   check(   started && file_contains(lpl, "FP One")
         && file_contains(lpl, crc)
         && !file_contains(lpl, "fp_one.zip#"), what, "fp_one.zip");
   if (!threaded)
   {
      sprintf(detail, "%d whole reads", reads);
      check(started && reads == 0,
            "one member: the fingerprint before the size", detail);
   }
   return;

fixture:
   check(0, "fixture", "could not write the one-member set");
}

/* Several members: a fingerprint in a database whose cores open zip
 * wins over the first member's hit in one claimed only for its type,
 * and the archive is not read whole. */
static void fp_cart_first_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static const char *const pl[2] = { "Test Cart Gate", "Test Arc Gate" };
   static uint8_t d[2][MF_MEMBER_LEN];
   static char    lpl[2][1024];
   char           db[512], dir[512], p[1024];
   char           what[64], detail[64], crc[16];
   uint8_t        be[8];
   db_rec_t       cart, arc;
   zip_entry_t    e[2];
   uint32_t       bytes;
   int            reads;
   bool           started;
   unsigned       k;
   const char    *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,  "%s/fp_cart_first_db", root);
   sprintf(dir, "%s/fp_cart_first",    root);
   path_mkdir(db);
   path_mkdir(dir);
   fill_forced(d[0], MF_MEMBER_LEN, 163, 0x0CA70201u);
   fill_forced(d[1], MF_MEMBER_LEN, 167, 0x0CA70202u);
   memset(e, 0, sizeof(e));
   e[0].name = "a.ct";
   e[0].data = d[0];
   e[0].len  = MF_MEMBER_LEN;
   e[0].crc  = 0x0CA70201u;
   e[1].name = "b.ct";
   e[1].data = d[1];
   e[1].len  = MF_MEMBER_LEN;
   e[1].crc  = 0x0CA70202u;
   sprintf(p, "%s/fcf0.zip", dir);
   if (!write_zip_entries(p, e, 2))
      goto fixture;
   bytes = zip_bytes(e, 2);

   set_rec(&cart, "FCF Cart", 0x0CA70201u, MF_MEMBER_LEN);
   /* The zip's size, which lets it be read whole, but not its CRC. */
   set_rec(&arc, "FCF Arcade", 0x0CA702FFu, bytes);
   arc.rom_name = "fcf0.zip";
   for (k = 0; k < 4; k++)
   {
      be[k]     = (uint8_t)(0x0CA70201u >> (24 - 8 * k));
      be[4 + k] = (uint8_t)(0x0CA70202u >> (24 - 8 * k));
   }
   arc.set_fp = crc_of(be, sizeof(be));
   sprintf(p, "%s/%s.rdb", db, pl[0]);
   if (!write_db_set(p, &cart, 1))
      goto fixture;
   sprintf(p, "%s/%s.rdb", db, pl[1]);
   if (!write_db_set(p, &arc, 1))
      goto fixture;

   for (k = 0; k < 2; k++)
   {
      sprintf(lpl[k], "%s/%s.lpl", pl_dir, pl[k]);
      filestream_delete(lpl[k]);
   }
   sprintf(crc, "%08X|crc", 0x0CA702FFu);
   use_queue(threaded);
   use_db(db);
   retro_atomic_store_release_int(&reads_begun, 0);
   begun_size = bytes;
   started    = run_scan(dir) && scan_completed;
   begun_size = 0;
   reads      = retro_atomic_load_acquire_int(&reads_begun);
   sprintf(what, "several members: the fingerprint record wins %s", q);
   check(   started && file_contains(lpl[1], "FCF Arcade")
         && file_contains(lpl[1], crc)
         && !file_contains(lpl[1], "fcf0.zip#"), what, "fcf0.zip");
   sprintf(what, "several members: no cartridge entry %s", q);
   check(started && !file_contains(lpl[0], "fcf0.zip"), what,
         "fcf0.zip#a.ct");
   if (!threaded)
   {
      sprintf(detail, "%d whole reads", reads);
      check(started && reads == 0,
            "several members, a fingerprint hit: not read whole", detail);
   }
   return;

fixture:
   check(0, "fixture", "could not write the fingerprint-first set");
}

/* A database matched by its members, as MAME's, never matches a set by
 * its fingerprint. */
static void fp_member_db_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   static uint8_t d[2][MF_MEMBER_LEN];
   static char    lpl[1024];
   char           db[512], dir[512], p[1024], what[64];
   uint8_t        be[8];
   db_rec_t       rec;
   zip_entry_t    e[2];
   bool           started;
   unsigned       k;
   const char    *q = threaded ? "(threaded)" : "(regular)";

   sprintf(db,  "%s/fp_member_db", root);
   sprintf(dir, "%s/fp_member",    root);
   path_mkdir(db);
   path_mkdir(dir);
   fill_forced(d[0], MF_MEMBER_LEN, 173, 0x0CA70401u);
   fill_forced(d[1], MF_MEMBER_LEN, 179, 0x0CA70402u);
   memset(e, 0, sizeof(e));
   e[0].name = "a.rd";
   e[0].data = d[0];
   e[0].len  = MF_MEMBER_LEN;
   e[0].crc  = 0x0CA70401u;
   e[1].name = "b.rd";
   e[1].data = d[1];
   e[1].len  = MF_MEMBER_LEN;
   e[1].crc  = 0x0CA70402u;
   sprintf(p, "%s/fp_member.zip", dir);
   if (!write_zip_entries(p, e, 2))
      goto fixture;

   set_rec(&rec, "ES FP Set", 0x0CA704FFu, zip_bytes(e, 2));
   rec.rom_name = "fp_member.zip";
   for (k = 0; k < 4; k++)
   {
      be[k]     = (uint8_t)(0x0CA70401u >> (24 - 8 * k));
      be[4 + k] = (uint8_t)(0x0CA70402u >> (24 - 8 * k));
   }
   rec.set_fp = crc_of(be, sizeof(be));
   sprintf(p, "%s/Test ES Arcade.rdb", db);
   if (!write_db_set(p, &rec, 1))
      goto fixture;

   sprintf(lpl, "%s/Test ES Arcade.lpl", pl_dir);
   filestream_delete(lpl);
   use_queue(threaded);
   use_db(db);
   started = run_scan(dir) && scan_completed;
   sprintf(what, "a member database: no fingerprint match %s", q);
   check(started && !file_contains(lpl, "ES FP Set"), what,
         "fp_member.zip");
   return;

fixture:
   check(0, "fixture", "could not write the member database set");
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
 * core opens 7z.  P is a zip of two members, m.rd and n.rd, an arcade
 * record holds by its set fingerprint alone. */
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

/* A P's second member's CRC, and the fingerprint of both. */
static uint32_t ah_p_crc(unsigned pos)
{
   return 0x0CBE0000u + pos;
}

static uint32_t ah_p_fp(unsigned pos)
{
   uint32_t c[2];
   uint8_t  be[8];
   unsigned k;

   c[0] = 0x0CB00000u + pos;
   c[1] = ah_p_crc(pos);
   for (k = 0; k < 4; k++)
   {
      be[k]     = (uint8_t)(c[0] >> (24 - 8 * k));
      be[4 + k] = (uint8_t)(c[1] >> (24 - 8 * k));
   }
   return crc_of(be, sizeof(be));
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
   if (role == 'P')
   {
      zip_entry_t pe[2];

      fill_forced(d + 2048, ah_member_len(role), 89 + pos, ah_p_crc(pos));
      pe[0]      = e;
      pe[1]      = e;
      pe[1].name = "n.rd";
      pe[1].data = d + 2048;
      pe[1].crc  = ah_p_crc(pos);
      return write_zip_entries(p, pe, 2) != 0;
   }
   return write_zip_entries(p, &e, 1) != 0;
}

/* Scans zips of @roles, in content-list order, on the threaded queue;
 * true when every K, F, A, B, G, P and W is in its playlist.  The first
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
         else if (role == 'P')
         {
            /* Neither its own CRC nor its size: only the fingerprint. */
            set_rec(&arc[na], names[pos[i]], 0x0CBEFFFFu - pos[i], 0);
            arc[na].unsized  = true;
            arc[na++].set_fp = ah_p_fp(pos[i]);
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

/* Match, fingerprint hit, match: a fingerprint hit ends the run as a
 * member hit does, so the second match lets the readers read one zip
 * whole ahead again, not two.  Test only: the P's lookup and the last
 * K's wait for the reads granted before them. */
static void ahead_fp_streak_lane(const char *root, const char *pl_dir)
{
   char detail[64];
   bool ok;
   int  begun;

   if (!readers_expected())
   {
      check(1, "read ahead: a fingerprint hit ends the run",
            "no readers");
      return;
   }
   look_crc[0]  = ah_p_fp(2);
   look_want[0] = 1;
   look_crc[1]  = 0x0CB00004u;
   look_want[1] = 2;
   look_n       = 2;
   ok           = ah_scan(root, pl_dir, "fpstreak", "KAPAKKKKKKKK", 0, 0,
         0, 0);
   look_n       = 0;
   begun        = retro_atomic_load_acquire_int(&rd_wholes);
   sprintf(detail, "%d reader reads", begun);
   check(   ok && !retro_atomic_load_acquire_int(&look_late)
         && begun == 2, "read ahead: a fingerprint hit ends the run",
         detail);
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
   if (!(*started = task_push_dbscan(dir, scan_cb, NULL)))
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
   if (!task_push_dbscan(dir, scan_cb, NULL))
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

/* Cores for core_lookup_lane(): two files with one file id, two cores
 * with one display name, a display name with a slash, a file id
 * with an underscore and two file names that differ only in case. */
static bool write_lookup_cores(const char *info_dir, const char *core_dir)
{
   static const char *const info[][3] = {
      { "dup",   "Dup Test",              "dupx"   },
      { "same1", "Same Name Test",        "samex"  },
      { "same2", "Same Name Test",        "samex"  },
      { "slash", "Slash / Test (Lookup)", "slashx" },
      { "two_part", "Two Part Test",      "twox"   }
   };
   static const char *const so[] = {
      "dup_libretro.so",   "dup_libretro_android.so",
      "same1_libretro.so", "same2_libretro.so", "slash_libretro.so",
      "two_part_libretro.so", "twin_libretro.so", "Twin_libretro.so"
   };
   char   p[1024];
   size_t i;
   FILE  *f;

   for (i = 0; i < sizeof(info) / sizeof(info[0]); i++)
   {
      sprintf(p, "%s/%s_libretro.info", info_dir, info[i][0]);
      if (!(f = fopen(p, "w")))
         return false;
      fprintf(f, "display_name = \"%s\"\nsupported_extensions = \"%s\"\n",
            info[i][1], info[i][2]);
      fclose(f);
   }
   for (i = 0; i < sizeof(so) / sizeof(so[0]); i++)
   {
      sprintf(p, "%s/%s", core_dir, so[i]);
      if (!(f = fopen(p, "wb")))
         return false;
      fputs("\177ELF", f);
      fclose(f);
   }
   return true;
}

/* The core list's path for the core whose file is @file. */
static const char *lookup_core_path(const char *file)
{
   core_info_list_t *list = NULL;
   size_t            i;

   core_info_get_list(&list);
   for (i = 0; list && i < list->count; i++)
   {
      core_info_t *info = core_info_get(list, i);
      if (info && info->path && !strcmp(path_basename(info->path), file))
         return info->path;
   }
   return "";
}

static char *ovr_str(const char *s)
{
   char *d = (char*)malloc(strlen(s) + 1);
   if (d)
      strcpy(d, s);
   return d;
}

/* A fresh record in place of @ov, which is freed. */
static manual_content_scan_overrides_t *ovr_fresh(
      manual_content_scan_overrides_t *ov)
{
   manual_content_scan_overrides_free(ov);
   return manual_content_scan_overrides_new();
}

static manual_content_scan_task_config_t *ovr_cfg;
static bool        ovr_cfg_ok;
static const char *ovr_dir;
static const char *ovr_pl;

/* A --scan item's settings: a reset, the folder, @ov.  The task config
 * lands in ovr_cfg, and ovr_cfg_ok says whether it could be built. */
static enum manual_content_scan_override_status ovr_run(
      const manual_content_scan_overrides_t *ov,
      const struct string_list *rdb, unsigned *count)
{
   enum manual_content_scan_override_status st;

   manual_content_scan_reset_settings();
   manual_content_scan_set_menu_content_dir(ovr_dir);
   st         = manual_content_scan_apply_overrides(ov, rdb, count);
   ovr_cfg_ok =    st == MANUAL_CONTENT_SCAN_OVR_OK
                && manual_content_scan_get_task_config(ovr_cfg, ovr_pl);
   return st;
}

/* After a Custom setup, every setting is back to its default, and
 * Custom over a reset builds the task Fully Automatic does. */
static void reset_lane(const char *root, const char *pl_dir)
{
   manual_content_scan_task_config_t *cfg =
         (manual_content_scan_task_config_t*)malloc(sizeof(*cfg));
   char dir[512];
   bool set_up;

   if (!cfg)
   {
      check(0, "reset lane", "out of memory");
      return;
   }
   sprintf(dir, "%s/scan_reset", root);
   path_mkdir(dir);

   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_CUSTOM);
   manual_content_scan_set_menu_content_dir(dir);
   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_DAT_LOOSE);
   strlcpy(manual_content_scan_get_dat_file_path_ptr(), "/x/reset.dat",
         manual_content_scan_get_dat_file_path_size());
   manual_content_scan_set_menu_scan_db_select(
         MANUAL_CONTENT_SCAN_SELECT_DB_SPECIFIC, "Test Alpha");
   strlcpy(manual_content_scan_get_system_name_custom_ptr(), "Reset Custom",
         manual_content_scan_get_system_name_custom_size());
   manual_content_scan_set_menu_system_name(
         MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM, NULL);
   set_up = manual_content_scan_set_menu_core_name(
         MANUAL_CONTENT_SCAN_CORE_SET, "Scan Test")
         && !strcmp(manual_content_scan_get_file_exts(), "bin cue gdi m3u zip");
   strlcpy(manual_content_scan_get_file_exts_custom_ptr(), "rst",
         manual_content_scan_get_file_exts_custom_size());
   *manual_content_scan_get_search_recursively_ptr() = false;
   *manual_content_scan_get_search_archives_ptr()    = false;
   *manual_content_scan_get_omit_db_ref_ptr()        = true;
   *manual_content_scan_get_filter_dat_content_ptr() = true;
   *manual_content_scan_get_overwrite_playlist_ptr() = true;
   *manual_content_scan_get_validate_entries_ptr()   = true;
   check(set_up, "reset: the Custom setup took", "core Scan Test");

   manual_content_scan_reset_settings();
   check(   manual_content_scan_get_scan_method_enum()
               == MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC
         && manual_content_scan_get_scan_use_db_enum()
               == MANUAL_CONTENT_SCAN_USE_DB_STRICT
         && manual_content_scan_get_scan_db_select_enum()
               == MANUAL_CONTENT_SCAN_SELECT_DB_AUTO
         && manual_content_scan_get_menu_system_name_type()
               == MANUAL_CONTENT_SCAN_SYSTEM_NAME_AUTO,
         "reset: method, database, playlist", "Automatic defaults");
   check(   !*manual_content_scan_get_content_dir_ptr()
         && !*manual_content_scan_get_dat_file_path_ptr()
         && !*manual_content_scan_get_system_name_custom_ptr()
         && !*manual_content_scan_get_file_exts_custom_ptr()
         && !*manual_content_scan_get_file_exts(),
         "reset: every path and name emptied", "");
   check(   *manual_content_scan_get_search_recursively_ptr()
         && *manual_content_scan_get_search_archives_ptr()
         && !*manual_content_scan_get_omit_db_ref_ptr()
         && !*manual_content_scan_get_filter_dat_content_ptr()
         && !*manual_content_scan_get_overwrite_playlist_ptr()
         && !*manual_content_scan_get_validate_entries_ptr(),
         "reset: the switches", "recursive, archives on, rest off");

   manual_content_scan_set_menu_content_dir(dir);
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_CUSTOM);
   check(   manual_content_scan_get_task_config(cfg, pl_dir)
         && !cfg->core_set && !*cfg->core_name && !*cfg->core_path
         && !*cfg->file_exts && !cfg->file_exts_custom_set
         && !*cfg->system_name && !*cfg->playlist_file
         && !*cfg->database_name
         && cfg->search_recursively && cfg->search_archives
         && !cfg->filter_dat_content && !cfg->overwrite_playlist
         && !cfg->validate_entries && !cfg->omit_db_reference
         && cfg->db_usage     == MANUAL_CONTENT_SCAN_USE_DB_STRICT
         && cfg->db_selection == MANUAL_CONTENT_SCAN_SELECT_DB_AUTO,
         "reset: Custom builds the automatic task", "");

   manual_content_scan_reset_settings();
   free(cfg);
}

/* Each override lands in the task config. */
static void overrides_lane(const char *root, const char *pl_dir)
{
   manual_content_scan_overrides_t *ov  = NULL;
   struct string_list              *rdb = string_list_new();
   union string_list_elem_attr      attr;
   char     dir[512], dat[1024], txt[1024], empty[1024], missing[1024];
   char     p[1024];
   unsigned count = 0;
   enum manual_content_scan_override_status st;
   FILE    *f;

   ovr_cfg = (manual_content_scan_task_config_t*)malloc(sizeof(*ovr_cfg));
   if (!rdb || !ovr_cfg)
   {
      check(0, "overrides lane", "out of memory");
      goto end;
   }
   attr.i = 0;
   string_list_append(rdb, "/db/Test Alpha.rdb", attr);
   string_list_append(rdb, "/db/Test Beta.rdb",  attr);

   sprintf(dir,     "%s/scan_ovr",             root);
   sprintf(dat,     "%s/scan_ovr/ovr.dat",     root);
   sprintf(txt,     "%s/scan_ovr/ovr.txt",     root);
   sprintf(empty,   "%s/scan_ovr/empty.dat",   root);
   sprintf(missing, "%s/scan_ovr/missing.dat", root);
   path_mkdir(dir);
   if (!(f = fopen(dat, "w")))
      goto fixture;
   fputs("<?xml version=\"1.0\"?>\n<datafile></datafile>\n", f);
   fclose(f);
   if (!(f = fopen(txt, "w")))
      goto fixture;
   fputs("not a dat\n", f);
   fclose(f);
   if (!(f = fopen(empty, "w")))
      goto fixture;
   fclose(f);
   remove(missing);
   ovr_dir = dir;
   ovr_pl  = pl_dir;

   check(   ovr_run(NULL, rdb, &count) == MANUAL_CONTENT_SCAN_OVR_OK
         && manual_content_scan_get_scan_method_enum()
               == MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC,
         "overrides: none leave Fully Automatic", "");

   /* Core: path, name and extensions */
   ov       = ovr_fresh(ov);
   ov->core = ovr_str("Scan Test");
   strlcpy(p, lookup_core_path("test_libretro.so"), sizeof(p));
   st = ovr_run(ov, rdb, &count);
   check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok
         && manual_content_scan_get_scan_method_enum()
               == MANUAL_CONTENT_SCAN_METHOD_CUSTOM
         && ovr_cfg->core_set && count == 1
         && !strcmp(ovr_cfg->core_name, "Scan Test")
         && !strcmp(ovr_cfg->core_path, p)
         && !strcmp(ovr_cfg->file_exts, "bin|cue|gdi|m3u|zip")
         && !ovr_cfg->file_exts_custom_set,
         "overrides: core, its path and extensions", ovr_cfg->core_path);

   /* Custom extensions win over the core's, scrubbed */
   ov->exts = ovr_str(".BIN gam");
   st = ovr_run(ov, rdb, &count);
   check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok
         && ovr_cfg->file_exts_custom_set
         && !strcmp(ovr_cfg->file_exts, "bin|gam"),
         "overrides: extensions", ovr_cfg->file_exts);

   /* Database Check without a DAT */
   ov        = ovr_fresh(ov);
   ov->flags = MANUAL_CONTENT_SCAN_OVR_MATCH;
   ov->match = MANUAL_CONTENT_SCAN_USE_DB_LOOSE;
   st = ovr_run(ov, rdb, &count);
   check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok
         && ovr_cfg->db_usage == MANUAL_CONTENT_SCAN_USE_DB_LOOSE,
         "overrides: match loose", "");
   ov->match = MANUAL_CONTENT_SCAN_USE_DB_STRICT;
   st = ovr_run(ov, rdb, &count);
   check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok
         && ovr_cfg->db_usage == MANUAL_CONTENT_SCAN_USE_DB_STRICT,
         "overrides: match strict", "");
   ov->match = MANUAL_CONTENT_SCAN_USE_DB_NONE;
   sprintf(p, "%s/scan_ovr.lpl", pl_dir);
   st = ovr_run(ov, rdb, &count);
   check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok
         && ovr_cfg->db_usage == MANUAL_CONTENT_SCAN_USE_DB_NONE
         && !strcmp(ovr_cfg->system_name, "scan_ovr")
         && !strcmp(ovr_cfg->playlist_file, p),
         "overrides: match none, named after the folder",
         ovr_cfg->system_name);

   /* A DAT: strict unless loose, never with none */
   ov        = ovr_fresh(ov);
   ov->dat   = ovr_str(dat);
   st = ovr_run(ov, rdb, &count);
   check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok
         && ovr_cfg->db_usage == MANUAL_CONTENT_SCAN_USE_DB_DAT_STRICT
         && ovr_cfg->filter_dat_content
         && !strcmp(ovr_cfg->dat_file_path, dat),
         "overrides: DAT alone is strict", ovr_cfg->dat_file_path);
   ov->flags = MANUAL_CONTENT_SCAN_OVR_MATCH;
   ov->match = MANUAL_CONTENT_SCAN_USE_DB_LOOSE;
   st = ovr_run(ov, rdb, &count);
   check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok
         && ovr_cfg->db_usage == MANUAL_CONTENT_SCAN_USE_DB_DAT_LOOSE
         && !ovr_cfg->filter_dat_content
         && !strcmp(ovr_cfg->dat_file_path, dat),
         "overrides: DAT with loose", "");
   ov->match = MANUAL_CONTENT_SCAN_USE_DB_STRICT;
   st = ovr_run(ov, rdb, &count);
   check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok
         && ovr_cfg->db_usage == MANUAL_CONTENT_SCAN_USE_DB_DAT_STRICT,
         "overrides: DAT with strict", "");
   ov->match = MANUAL_CONTENT_SCAN_USE_DB_NONE;
   st = ovr_run(ov, rdb, &count);
   check(   st == MANUAL_CONTENT_SCAN_OVR_DAT_WITHOUT_MATCH
         && !*manual_content_scan_get_dat_file_path_ptr(),
         "overrides: DAT with none fails untouched", "");

   /* DAT errors */
   ov = ovr_fresh(ov);
   ov->dat = ovr_str(txt);
   check(ovr_run(ov, rdb, &count) == MANUAL_CONTENT_SCAN_OVR_NOT_A_DAT,
         "overrides: DAT with another extension", "not a DAT");
   ov = ovr_fresh(ov);
   ov->dat = ovr_str(missing);
   check(ovr_run(ov, rdb, &count) == MANUAL_CONTENT_SCAN_OVR_NOT_A_DAT,
         "overrides: DAT missing", "not a DAT");
   ov = ovr_fresh(ov);
   ov->dat = ovr_str(empty);
   check(ovr_run(ov, rdb, &count) == MANUAL_CONTENT_SCAN_OVR_NOT_A_DAT,
         "overrides: DAT empty", "not a DAT");

   /* A refresh's saved DAT: the match picks its mode, none drops it */
   ov        = ovr_fresh(ov);
   ov->flags = MANUAL_CONTENT_SCAN_OVR_MATCH;
   {
      static const enum manual_content_scan_db_usage
            match[3] = { MANUAL_CONTENT_SCAN_USE_DB_LOOSE,
                         MANUAL_CONTENT_SCAN_USE_DB_STRICT,
                         MANUAL_CONTENT_SCAN_USE_DB_NONE };
      static const enum manual_content_scan_db_usage
            want[3]  = { MANUAL_CONTENT_SCAN_USE_DB_DAT_LOOSE,
                         MANUAL_CONTENT_SCAN_USE_DB_DAT_STRICT,
                         MANUAL_CONTENT_SCAN_USE_DB_NONE };
      static const char *const what[3] = {
         "overrides: saved DAT with loose",
         "overrides: saved DAT with strict",
         "overrides: saved DAT with none drops it" };
      int i;

      for (i = 0; i < 3; i++)
      {
         manual_content_scan_reset_settings();
         manual_content_scan_set_menu_content_dir(dir);
         manual_content_scan_set_menu_scan_use_db(
               MANUAL_CONTENT_SCAN_USE_DB_DAT_STRICT);
         strlcpy(manual_content_scan_get_dat_file_path_ptr(), dat,
               manual_content_scan_get_dat_file_path_size());
         ov->match = match[i];
         st = manual_content_scan_apply_overrides(ov, rdb, &count);
         check(   st == MANUAL_CONTENT_SCAN_OVR_OK
               && manual_content_scan_get_task_config(ovr_cfg, pl_dir)
               && ovr_cfg->db_usage == want[i]
               && (i == 2
                     ? !*manual_content_scan_get_dat_file_path_ptr()
                     : !strcmp(ovr_cfg->dat_file_path, dat)),
               what[i], "");
      }
   }

   /* Database To Match */
   ov           = ovr_fresh(ov);
   ov->database = ovr_str("Test Alpha");
   sprintf(p, "%s/Test Alpha.lpl", pl_dir);
   st = ovr_run(ov, rdb, &count);
   check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok
         && ovr_cfg->db_selection == MANUAL_CONTENT_SCAN_SELECT_DB_SPECIFIC
         && !strcmp(ovr_cfg->database_name, "Test Alpha.lpl")
         && !strcmp(ovr_cfg->system_name, "Test Alpha")
         && !strcmp(ovr_cfg->playlist_file, p),
         "overrides: database by name", ovr_cfg->database_name);
   check(ovr_run(ov, NULL, &count) == MANUAL_CONTENT_SCAN_OVR_NO_SUCH_DATABASE,
         "overrides: no database installed", "no such database");
   ov           = ovr_fresh(ov);
   ov->database = ovr_str("Test Nope");
   check(ovr_run(ov, rdb, &count) == MANUAL_CONTENT_SCAN_OVR_NO_SUCH_DATABASE,
         "overrides: an unknown database", "no such database");
   ov        = ovr_fresh(ov);
   ov->flags = MANUAL_CONTENT_SCAN_OVR_FIRST_DB;
   st = ovr_run(ov, rdb, &count);
   check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok
         && ovr_cfg->db_selection
               == MANUAL_CONTENT_SCAN_SELECT_DB_AUTO_FIRST_MATCH,
         "overrides: first matching database", "");

   /* Target Playlist */
   ov           = ovr_fresh(ov);
   ov->playlist = ovr_str("Test Beta");
   sprintf(p, "%s/Test Beta.lpl", pl_dir);
   st = ovr_run(ov, rdb, &count);
   check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok
         && manual_content_scan_get_menu_system_name_type()
               == MANUAL_CONTENT_SCAN_SYSTEM_NAME_DATABASE
         && !strcmp(ovr_cfg->system_name, "Test Beta")
         && !strcmp(ovr_cfg->playlist_file, p),
         "overrides: playlist named after a database", ovr_cfg->system_name);
   ov->database = ovr_str("Test Alpha");
   st = ovr_run(ov, rdb, &count);
   check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok
         && manual_content_scan_get_menu_system_name_type()
               == MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM
         && !strcmp(ovr_cfg->system_name, "Test Beta")
         && !strcmp(ovr_cfg->playlist_file, p)
         && !strcmp(ovr_cfg->database_name, "Test Alpha.lpl"),
         "overrides: and another database: Custom", ovr_cfg->database_name);
   free(ov->playlist);
   ov->playlist = ovr_str("Test Alpha");
   st = ovr_run(ov, rdb, &count);
   check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok
         && manual_content_scan_get_menu_system_name_type()
               == MANUAL_CONTENT_SCAN_SYSTEM_NAME_DATABASE
         && !strcmp(ovr_cfg->system_name, "Test Alpha"),
         "overrides: and the same database", ovr_cfg->system_name);
   /* As a refresh of a database playlist leaves them */
   ov           = ovr_fresh(ov);
   ov->database = ovr_str("Test Alpha");
   sprintf(p, "%s/Test Beta.lpl", pl_dir);
   manual_content_scan_reset_settings();
   manual_content_scan_set_menu_content_dir(ovr_dir);
   manual_content_scan_set_menu_system_name(
         MANUAL_CONTENT_SCAN_SYSTEM_NAME_DATABASE, "Test Beta");
   manual_content_scan_set_menu_scan_db_select(
         MANUAL_CONTENT_SCAN_SELECT_DB_SPECIFIC, "Test Beta");
   st         = manual_content_scan_apply_overrides(ov, rdb, &count);
   ovr_cfg_ok =    st == MANUAL_CONTENT_SCAN_OVR_OK
                && manual_content_scan_get_task_config(ovr_cfg, pl_dir);
   check(   ovr_cfg_ok
         && !strcmp(ovr_cfg->playlist_file, p)
         && !strcmp(ovr_cfg->database_name, "Test Alpha.lpl"),
         "overrides: a refresh's database playlist keeps its name",
         ovr_cfg_ok ? ovr_cfg->playlist_file : "no task config");
   ov           = ovr_fresh(ov);
   ov->playlist = ovr_str("My: List?");
   sprintf(p, "%s/My_ List_.lpl", pl_dir);
   st = ovr_run(ov, rdb, &count);
   check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok
         && manual_content_scan_get_menu_system_name_type()
               == MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM
         && !strcmp(ovr_cfg->system_name, "My_ List_")
         && !strcmp(ovr_cfg->playlist_file, p),
         "overrides: custom playlist, scrubbed", ovr_cfg->system_name);

   /* The switches */
   ov        = ovr_fresh(ov);
   ov->flags =   MANUAL_CONTENT_SCAN_OVR_NO_RECURSIVE
               | MANUAL_CONTENT_SCAN_OVR_NO_ARCHIVES
               | MANUAL_CONTENT_SCAN_OVR_OVERWRITE
               | MANUAL_CONTENT_SCAN_OVR_VALIDATE
               | MANUAL_CONTENT_SCAN_OVR_OMIT_DB_REF;
   st = ovr_run(ov, rdb, &count);
   check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok
         && !ovr_cfg->search_recursively && !ovr_cfg->search_archives
         && ovr_cfg->overwrite_playlist && ovr_cfg->validate_entries
         && ovr_cfg->omit_db_reference,
         "overrides: recursive, archives, overwrite, validate, omit", "");

   /* A step that fails stops the ones after it */
   ov           = ovr_fresh(ov);
   ov->core     = ovr_str("No Such Core");
   ov->database = ovr_str("Test Alpha");
   st = ovr_run(ov, rdb, &count);
   check(   st == MANUAL_CONTENT_SCAN_OVR_NO_SUCH_CORE && count == 0
         && manual_content_scan_get_scan_db_select_enum()
               == MANUAL_CONTENT_SCAN_SELECT_DB_AUTO
         && manual_content_scan_get_scan_method_enum()
               == MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC,
         "overrides: an error stops the rest", "");
   goto end;

fixture:
   check(0, "fixture", "could not write the DAT files");
end:
   manual_content_scan_overrides_free(ov);
   manual_content_scan_reset_settings();
   string_list_free(rdb);
   free(ovr_cfg);
   ovr_cfg = NULL;
}

/* A refresh drops a missing entry only when the folder it is judged
 * by - the content folder for an entry inside it, else its own or its
 * archive's - holds something; extensions count only inside. */
struct anchor_case
{
   const char *path;
   const char *needle;
   bool        kept;
   const char *what;
};

/* Refreshes @lpl as the menu does: an entry per case under @root,
 * saved folder @saved, extension bin, no database. */
static void anchor_refresh(const char *root, const char *lpl,
      const char *saved, const struct anchor_case *cases, size_t n)
{
   playlist_config_t    *cfg = (playlist_config_t*)calloc(1, sizeof(*cfg));
   playlist_t           *pl  = NULL;
   char                  p[1024];
   struct playlist_entry e;
   size_t                i;
   bool                  ran = false;

   if (!cfg)
   {
      check(0, "anchor lane", "out of memory");
      return;
   }
   filestream_delete(lpl);
   playlist_config_set_path(cfg, lpl);
   cfg->capacity = COLLECTION_SIZE;
   if (!(pl = playlist_init(cfg)))
      goto fixture;
   for (i = 0; i < n; i++)
   {
      sprintf(p, "%s/%s", root, cases[i].path);
      memset(&e, 0, sizeof(e));
      e.path      = p;
      e.label     = (char*)"Anchor";
      e.core_path = (char*)"DETECT";
      e.core_name = (char*)"DETECT";
      if (!playlist_push(pl, &e))
         goto fixture;
   }
   playlist_set_scan_content_dir(pl, saved);
   playlist_set_scan_file_exts(pl, "bin");
   playlist_set_scan_db_usage(pl, MANUAL_CONTENT_SCAN_USE_DB_NONE);
   playlist_write_file(pl);

   ran =    manual_content_scan_set_menu_from_playlist(pl,
               config_get_ptr()->paths.path_content_database, false)
               == MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_OK
         && run_scan(saved) && scan_completed;
   check(ran, "validate: refresh ran", lpl);
   for (i = 0; ran && i < n; i++)
      check(file_contains(lpl, cases[i].needle) == cases[i].kept,
            cases[i].what, cases[i].needle);
   goto end;

fixture:
   check(0, "fixture", "could not write the anchor lane");
end:
   if (pl)
      playlist_free(pl);
   free(cfg);
   manual_content_scan_reset_settings();
}

static void anchor_lane(const char *root, const char *pl_dir)
{
   /* The needles leave the folders out: Windows stores backslashes. */
   static const struct anchor_case cases[] = {
      { "anchor_saved/in_here.bin",      "in_here.bin",     true,
        "validate: inside, present: kept" },
      { "anchor_saved/in_gone.bin",      "in_gone.bin",     false,
        "validate: inside, missing: dropped" },
      { "anchor_saved/in_here.img",      "in_here.img",     false,
        "validate: inside, extension not saved: dropped" },
      { "anchor_full/full_gone.bin",     "full_gone.bin",   false,
        "validate: outside, missing, folder there: dropped" },
      { "anchor_absent/absent_gone.bin", "absent_gone.bin", true,
        "validate: outside, missing, folder gone: kept" },
      { "anchor_void/void_gone.bin",     "void_gone.bin",   true,
        "validate: outside, missing, folder empty: kept" },
      { "anchor_full/full_here.img",     "full_here.img",   true,
        "validate: outside, extension not saved: kept" },
      { "anchor_saved2/sib_gone.bin",    "sib_gone.bin",    true,
        "validate: outside, name shares the prefix: kept" },
      { "anchor_absent/absent.zip#sub/m.bin", "absent.zip#sub", true,
        "validate: member, archive's folder gone: kept" },
      { "anchor_full/full.zip#sub/m.bin",     "full.zip#sub",   false,
        "validate: member, archive's folder there: dropped" }
   };
#ifdef _WIN32
   static const struct anchor_case other_case[] = {
      { "anchor_saved/in_here.img", "in_here.img", false,
        "validate: saved folder in another case: inside" }
   };
#else
   /* Saved through a link to anchor_real.  Entries are saved with
    * their symlinks resolved, so a missing one names anchor_real. */
   static const struct anchor_case linked[] = {
      { "anchor_link/link_here.img",          "link_here.img", false,
        "validate: via symlink, extension not saved: dropped" },
      { "anchor_real/link_sub/link_gone.bin", "link_gone.bin", false,
        "validate: via symlink, missing: dropped" }
   };
#endif
   char saved[512], full[512], plist[512], lpl[1024];
   char p[1024];

   sprintf(saved, "%s/anchor_saved", root);
   sprintf(full,  "%s/anchor_full",  root);
   sprintf(plist, "%s/anchor_pl",    root);
   sprintf(lpl,   "%s/Anchor.lpl",   plist);
   path_mkdir(saved);
   path_mkdir(full);
   path_mkdir(plist);
   sprintf(p, "%s/anchor_void", root);
   path_mkdir(p);
   sprintf(p, "%s/in_here.bin", saved);
   if (!filestream_write_file(p, "bin", 3))
      goto fixture;
   sprintf(p, "%s/in_here.img", saved);
   if (!filestream_write_file(p, "img", 3))
      goto fixture;
   sprintf(p, "%s/full_here.img", full);
   if (!filestream_write_file(p, "img", 3))
      goto fixture;

   strlcpy(config_get_ptr()->paths.directory_playlist, plist,
         sizeof(config_get_ptr()->paths.directory_playlist));
   anchor_refresh(root, lpl, saved, cases,
         sizeof(cases) / sizeof(cases[0]));

#ifdef _WIN32
   sprintf(saved, "%s/ANCHOR_SAVED", root);
   sprintf(lpl, "%s/AnchorCase.lpl", plist);
   anchor_refresh(root, lpl, saved, other_case,
         sizeof(other_case) / sizeof(other_case[0]));
   printf("  skip  validate: via symlink (none made on Windows)\n");
#else
   sprintf(p, "%s/anchor_real", root);
   path_mkdir(p);
   sprintf(p, "%s/anchor_real/link_here.bin", root);
   if (!filestream_write_file(p, "bin", 3))
      goto fixture;
   sprintf(p, "%s/anchor_real/link_here.img", root);
   if (!filestream_write_file(p, "img", 3))
      goto fixture;
   sprintf(saved, "%s/anchor_link", root);
   sprintf(lpl, "%s/AnchorLink.lpl", plist);
   filestream_delete(saved);
   if (symlink("anchor_real", saved) != 0)
      goto fixture;
   strlcpy(p, saved, sizeof(p));
   if (   !path_resolve_realpath(p, sizeof(p), true)
       || !strcmp(p, saved))
   {
      printf("  skip  validate: via symlink (links not resolved)\n");
      goto end;
   }
   anchor_refresh(root, lpl, saved, linked,
         sizeof(linked) / sizeof(linked[0]));
#endif
   goto end;

fixture:
   check(0, "fixture", "could not write the anchor lane");
end:
   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
}

/* --scan-core: a path, then a file name, then a display name; more
 * than one match in the step that decides is ambiguous. */
static void core_lookup_lane(const char *root)
{
   manual_content_scan_overrides_t *ov = NULL;
   char     elsewhere[512], p[1024];
   char     dup_path[1024];
   unsigned count;
   size_t   i;
   enum manual_content_scan_override_status st;
   const char *core_name = NULL;
   static const char *const by_name[] = {
      "gamma", "gamma_libretro", "gamma_libretro.so", "gamma_libretro.dll",
      "gamma_libretro_android.so", "Gamma Test" };
   static const char *const two_part[] = {
      "two_part", "two_part_libretro_ios.dylib" };
   static const char *const other_case[] = {
      "GAMMA", "Gamma_libretro.so", "Gamma_Libretro.dll",
      "GAMMA_LIBRETRO_ANDROID.SO" };
   /* What each name finds; "" for both */
   static const char *const twin[][2] = {
      { "twin", "twin_libretro.so" },
      { "Twin_libretro.so", "Twin_libretro.so" },
      { "TWIN", "" } };
   bool     twins;
   FILE    *f;

   ovr_cfg = (manual_content_scan_task_config_t*)malloc(sizeof(*ovr_cfg));
   if (!ovr_cfg)
   {
      check(0, "core lookup lane", "out of memory");
      return;
   }
   sprintf(elsewhere, "%s/scan_lookup_elsewhere", root);
   path_mkdir(elsewhere);
   sprintf(p, "%s/gamma_libretro.so", elsewhere);
   if ((f = fopen(p, "wb")))
   {
      fputs("\177ELF", f);
      fclose(f);
   }
   ovr_dir = elsewhere;
   ovr_pl  = elsewhere;

   ov       = ovr_fresh(ov);
   ov->core = ovr_str(lookup_core_path("gamma_libretro.so"));
   count    = 9;
   st = ovr_run(ov, NULL, &count);
   check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok && count == 1
         && !strcmp(ovr_cfg->core_name, "Gamma Test")
         && !strcmp(ovr_cfg->core_path, ov->core),
         "core lookup: by path", ovr_cfg->core_path);

   free(ov->core);
   ov->core = ovr_str(p);
   count    = 9;
   check(   ovr_run(ov, NULL, &count) == MANUAL_CONTENT_SCAN_OVR_NO_SUCH_CORE
         && count == 0,
         "core lookup: a path outside the core folder", "no such core");

   for (i = 0; i < sizeof(by_name) / sizeof(by_name[0]); i++)
   {
      free(ov->core);
      ov->core = ovr_str(by_name[i]);
      st = ovr_run(ov, NULL, &count);
      check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok && count == 1
            && !strcmp(ovr_cfg->core_name, "Gamma Test"),
            "core lookup: by file or display name", by_name[i]);
   }

   for (i = 0; i < sizeof(two_part) / sizeof(two_part[0]); i++)
   {
      free(ov->core);
      ov->core = ovr_str(two_part[i]);
      st = ovr_run(ov, NULL, &count);
      check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok && count == 1
            && !strcmp(ovr_cfg->core_name, "Two Part Test"),
            "core lookup: a file id with an underscore", two_part[i]);
   }

   /* File names ignore case, display names do not */
   for (i = 0; i < sizeof(other_case) / sizeof(other_case[0]); i++)
   {
      free(ov->core);
      ov->core = ovr_str(other_case[i]);
      count    = 9;
      st = ovr_run(ov, NULL, &count);
      check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok && count == 1
            && !strcmp(ovr_cfg->core_name, "Gamma Test"),
            "core lookup: a file name in another case", other_case[i]);
   }
   free(ov->core);
   ov->core = ovr_str("gamma test");
   count    = 9;
   check(   ovr_run(ov, NULL, &count) == MANUAL_CONTENT_SCAN_OVR_NO_SUCH_CORE
         && count == 0,
         "core lookup: a display name in another case", "no such core");

   free(ov->core);
   ov->core = ovr_str("Slash / Test (Lookup)");
   st = ovr_run(ov, NULL, &count);
   check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok
         && !strcmp(ovr_cfg->core_name, "Slash / Test (Lookup)"),
         "core lookup: a display name with a slash", ovr_cfg->core_name);

   free(ov->core);
   ov->core = ovr_str("nope");
   count    = 9;
   check(   ovr_run(ov, NULL, &count) == MANUAL_CONTENT_SCAN_OVR_NO_SUCH_CORE
         && count == 0
         && manual_content_scan_get_menu_core_name(&core_name)
         && manual_content_scan_get_scan_method_enum()
               == MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC,
         "core lookup: unknown", "no such core");

   /* dup_libretro.so and dup_libretro_android.so share a file id */
   free(ov->core);
   ov->core = ovr_str("dup");
   check(   ovr_run(ov, NULL, &count) == MANUAL_CONTENT_SCAN_OVR_AMBIGUOUS_CORE
         && count == 2,
         "core lookup: two files with one file id", "2 cores");
   free(ov->core);
   ov->core = ovr_str("dup_libretro.so");
   check(   ovr_run(ov, NULL, &count) == MANUAL_CONTENT_SCAN_OVR_AMBIGUOUS_CORE
         && count == 2,
         "core lookup: and by its file name", "2 cores");
   free(ov->core);
   ov->core = ovr_str("dup_libretro_android.so");
   check(   ovr_run(ov, NULL, &count) == MANUAL_CONTENT_SCAN_OVR_AMBIGUOUS_CORE
         && count == 2,
         "core lookup: and by its platform file name", "2 cores");
   strlcpy(dup_path, lookup_core_path("dup_libretro_android.so"),
         sizeof(dup_path));
   free(ov->core);
   ov->core = ovr_str(dup_path);
   st = ovr_run(ov, NULL, &count);
   check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok && count == 1
         && !strcmp(ovr_cfg->core_path, dup_path),
         "core lookup: the path tells them apart", path_basename(dup_path));
#ifdef _WIN32
   /* All '\', '\' before the file name only, and all '\' in capitals */
   for (i = 0; i < 3; i++)
   {
      char *slash;

      strlcpy(p, dup_path, sizeof(p));
      if (i != 1)
         pathname_conform_slashes_to_os(p);
      else if ((slash = strrchr(p, '/')))
         *slash = '\\';
      if (i == 2)
         string_to_upper(p);
      free(ov->core);
      ov->core = ovr_str(p);
      count    = 9;
      st = ovr_run(ov, NULL, &count);
      check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok && count == 1
            && !strcmp(ovr_cfg->core_path, dup_path),
            "core lookup: the path with either slash", p);
   }
#endif

   /* The exact case first, else both are ambiguous; a file system
    * that ignores case keeps one of the two */
   twins = *lookup_core_path("twin_libretro.so")
         && *lookup_core_path("Twin_libretro.so");
   for (i = 0; i < sizeof(twin) / sizeof(twin[0]); i++)
   {
      free(ov->core);
      ov->core = ovr_str(twin[i][0]);
      count    = 9;
      st = ovr_run(ov, NULL, &count);
      if (!twins)
         check(st == MANUAL_CONTENT_SCAN_OVR_OK && count == 1,
               "core lookup: two file names in another case", "one file");
      else if (*twin[i][1])
         check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok && count == 1
               && !strcmp(ovr_cfg->core_path, lookup_core_path(twin[i][1])),
               "core lookup: the file name in the case typed", twin[i][0]);
      else
         check(st == MANUAL_CONTENT_SCAN_OVR_AMBIGUOUS_CORE && count == 2,
               "core lookup: two file names in another case", twin[i][0]);
   }

   free(ov->core);
   ov->core = ovr_str("Same Name Test");
   check(   ovr_run(ov, NULL, &count) == MANUAL_CONTENT_SCAN_OVR_AMBIGUOUS_CORE
         && count == 2,
         "core lookup: two cores with one display name", "2 cores");
   free(ov->core);
   ov->core = ovr_str("same2");
   st = ovr_run(ov, NULL, &count);
   check(   st == MANUAL_CONTENT_SCAN_OVR_OK && ovr_cfg_ok && count == 1
         && !strcmp(path_basename(ovr_cfg->core_path), "same2_libretro.so"),
         "core lookup: the file name tells them apart",
         path_basename(ovr_cfg->core_path));

   manual_content_scan_overrides_free(ov);
   manual_content_scan_reset_settings();
   free(ovr_cfg);
   ovr_cfg = NULL;
}

/* What chain_lane()'s callbacks were handed; chain_in_push, what they
 * were before task_push_dbscan_chain() returned. */
#define CHAIN_SEEN_MAX 8
static unsigned                   chain_seen;
static unsigned                   chain_dones;
static unsigned                   chain_in_push;
static bool                       chain_ud_ok;
static char                       chain_names[CHAIN_SEEN_MAX][512];
static enum task_dbscan_outcome   chain_outcomes[CHAIN_SEEN_MAX];
static int                        chain_added[CHAIN_SEEN_MAX];
/* The system name type each item's scan ran with: the chain puts the
 * Manual Scan settings back once it ends. */
static unsigned                   chain_name_types[CHAIN_SEEN_MAX];
static task_dbscan_chain_result_t chain_totals;

static void chain_item_cb(const char *name,
      enum task_dbscan_outcome outcome,
      const task_dbscan_result_t *res, void *user_data)
{
   if (user_data != &chain_seen)
      chain_ud_ok = false;
   if (chain_seen < CHAIN_SEEN_MAX)
   {
      strlcpy(chain_names[chain_seen], name ? name : "(null)",
            sizeof(chain_names[0]));
      chain_outcomes[chain_seen] = outcome;
      chain_added[chain_seen]    = res ? (int)res->added : -1;
      chain_name_types[chain_seen] =
         manual_content_scan_get_menu_system_name_type();
   }
   chain_seen++;
}

static void chain_done_cb(const task_dbscan_chain_result_t *res,
      void *user_data)
{
   if (user_data != &chain_seen)
      chain_ud_ok = false;
   chain_totals = *res;
   chain_dones++;
}

/* A done callback that starts the next chain: the one ending must no
 * longer count as running. */
static const char *chain_restart_arg;
static bool        chain_restart_ok;

static task_dbscan_item_t *chain_items(size_t n, const char *first,
      const char *rest);

static void chain_done_restart_cb(const task_dbscan_chain_result_t *res,
      void *user_data)
{
   chain_done_cb(res, user_data);
   chain_restart_ok = task_push_dbscan_chain(
         chain_items(1, chain_restart_arg, NULL), 1,
         chain_item_cb, chain_done_cb, &chain_seen);
}

/* A --scan item for @arg; with @core, an override naming it. */
static void chain_item(task_dbscan_item_t *it, const char *arg,
      const char *core)
{
   it->kind      = TASK_DBSCAN_ITEM_PATH;
   it->arg       = ovr_str(arg);
   it->overrides = NULL;
   if (core && (it->overrides = manual_content_scan_overrides_new()))
      it->overrides->core = ovr_str(core);
}

/* @n items, the first for @first, the rest for @rest. */
static task_dbscan_item_t *chain_items(size_t n, const char *first,
      const char *rest)
{
   size_t              i;
   task_dbscan_item_t *items = (task_dbscan_item_t*)calloc(n,
         sizeof(*items));

   if (items)
      for (i = 0; i < n; i++)
         chain_item(&items[i], i ? rest : first, NULL);
   return items;
}

static bool chain_push(task_dbscan_item_t *items, size_t n)
{
   bool pushed;

   chain_seen   = 0;
   chain_dones  = 0;
   chain_ud_ok  = true;
   memset(&chain_totals, 0, sizeof(chain_totals));
   pushed        = task_push_dbscan_chain(items, n, chain_item_cb,
         chain_done_cb, &chain_seen);
   chain_in_push = chain_seen + chain_dones;
   return pushed;
}

/* Runs the queue until done_cb, then a while longer to see it is not
 * called again.  With @cancel_slow, cancels the scan once a slow read
 * has begun, then lets a held one go. */
static void chain_wait(bool cancel_slow)
{
   task_finder_data_t finder;
   unsigned           i;
   bool               cancelled = false;
   time_t             started   = time(NULL);

   finder.func     = find_scan;
   finder.userdata = NULL;
   while (!chain_dones)
   {
      if (   cancel_slow && !cancelled
          && retro_atomic_load_acquire_int(&slow_in) > 0
          && task_queue_find(&finder))
      {
         task_queue_cancel_ident(lane_ident);
         retro_atomic_store_release_int(&slow_hold, 0);
         cancelled = true;
      }
      task_queue_check();
      if (difftime(time(NULL), started) > SCAN_TIMEOUT_SECONDS)
         return;
      retro_sleep(1);
   }
   for (i = 0; i < 20; i++)
   {
      task_queue_check();
      retro_sleep(1);
   }
}

static bool chain_saw(unsigned i, const char *name,
      enum task_dbscan_outcome outcome, int added)
{
   return    i < chain_seen && i < CHAIN_SEEN_MAX
          && !strcmp(chain_names[i], name)
          && chain_outcomes[i] == outcome
          && chain_added[i]    == added;
}

/* The chain runs --scan items in order, one task at a time: a missing
 * path and an unknown core fail their item, not the run, and the two
 * scans of one playlist follow each other, the second pushed from the
 * first's callback.  A cancelled scan stops the rest.  A chain is
 * refused while another runs, or a manual scan. */
static void chain_lane(const char *root, const char *pl_dir,
      uint32_t crc_a, uint32_t sz_a, bool threaded)
{
   char                a[512], b[512], slow[512], missing[512], pl[512];
   char                out[1024], err[1024], p[1024], what[96];
   char                detail[160];
   const char         *q = threaded ? "(threaded)" : "(regular)";
   task_dbscan_item_t *items;
   bool                pushed, refused, captured, back, running;
   bool                scan_refused;
   unsigned            busy_before, busy_scan;

   sprintf(a,       "%s/chain_a",       root);
   sprintf(b,       "%s/chain_b",       root);
   sprintf(slow,    "%s/chain_slow",    root);
   sprintf(missing, "%s/chain_missing", root);
   sprintf(pl,      "%s/chain_pl",      root);
   path_mkdir(a);
   path_mkdir(b);
   path_mkdir(slow);
   path_mkdir(pl);
   sprintf(p, "%s/01_alpha.bin", a);
   if (!write_content(p, crc_a, sz_a))
      goto fixture;
   sprintf(p, "%s/01_alpha.bin", b);
   if (!write_content(p, crc_a, sz_a))
      goto fixture;
   sprintf(p, "%s/00_slow.bin", slow);
   if (!write_content(p, crc_a, sz_a))
      goto fixture;
   sprintf(p, "%s/01_more.bin", slow);
   if (!write_content(p, 0x3C0FFEE0u, 1024))
      goto fixture;
   strlcpy(config_get_ptr()->paths.directory_playlist, pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   delete_playlists(pl);
   use_queue(threaded);

   pushed = chain_push(chain_items(1, missing, NULL), 1);
   chain_wait(false);
   sprintf(what, "chain %s: nothing inside the push", q);
   sprintf(detail, "%u calls in the push, done %u", chain_in_push,
         chain_dones);
   check(pushed && !chain_in_push && chain_dones == 1, what, detail);
   sprintf(what, "chain %s: a missing path fails", q);
   check(   chain_seen == 1
         && chain_saw(0, missing, TASK_DBSCAN_FAILED, -1)
         && chain_totals.failed == 1 && !chain_totals.ran
         && !chain_totals.added, what, chain_names[0]);

   sprintf(out, "%s/chain_out.txt", root);
   sprintf(err, "%s/chain_err.txt", root);
   items = (task_dbscan_item_t*)calloc(4, sizeof(*items));
   if (!items)
      goto fixture;
   chain_item(&items[0], a,       NULL);
   chain_item(&items[1], missing, NULL);
   chain_item(&items[2], b,       "nope");
   chain_item(&items[3], b,       NULL);
   cli_output = true;
   captured   = capture_begin(out, err);
   pushed     = chain_push(items, 4);
   chain_wait(false);
   if (captured)
      capture_end();
   cli_output = false;
   sprintf(what, "chain %s: items in order", q);
   sprintf(detail, "%u items, done %u", chain_seen, chain_dones);
   check(   captured && pushed && chain_seen == 4
         && chain_saw(0, a,       TASK_DBSCAN_RAN,    1)
         && chain_saw(1, missing, TASK_DBSCAN_FAILED, -1)
         && chain_saw(2, b,       TASK_DBSCAN_FAILED, -1)
         && chain_saw(3, b,       TASK_DBSCAN_RAN,    1), what, detail);
   sprintf(what, "chain %s: the totals", q);
   sprintf(detail, "ran %u, added %u, failed %u, skipped %u%s",
         chain_totals.ran, chain_totals.added, chain_totals.failed,
         chain_totals.skipped, chain_totals.cancelled ? ", cancelled" : "");
   check(   chain_totals.ran == 2 && chain_totals.added == 2
         && chain_totals.failed == 2 && !chain_totals.skipped
         && !chain_totals.cancelled, what, detail);
   sprintf(what, "chain %s: done_cb once, with the user data", q);
   check(chain_dones == 1 && chain_ud_ok && !chain_in_push, what, "");
   sprintf(what, "chain %s: one playlist twice in a row", q);
   sprintf(p, "%s/Test Alpha.lpl", pl);
   {
      char ea[1024], eb[1024];

      sprintf(ea, "%s/01_alpha.bin", a);
      sprintf(eb, "%s/01_alpha.bin", b);
      check(file_contains(p, ea) && file_contains(p, eb), what, p);
   }
   sprintf(what, "chain %s: the item lines", q);
   sprintf(p, "%s: not found", missing);
   check(   file_contains(err, p)
         && file_contains(err, "nope: no such core"), what, err);

   /* Held on the threaded queue, so the scan cannot end before the
    * cancel; the regular one reads on this thread */
   slow_reset(sz_a, 300);
   if (threaded)
      retro_atomic_store_release_int(&slow_hold, 1);
   pushed = chain_push(chain_items(2, slow, a), 2);
   chain_wait(true);
   retro_atomic_store_release_int(&slow_hold, 0);
   slow_size = 0;
   back      = slow_reads_back() && readers_gone();
   sprintf(what, "chain %s: a cancelled scan stops the rest", q);
   sprintf(detail, "%u items, ran %u%s, done %u", chain_seen,
         chain_totals.ran, chain_totals.cancelled ? ", cancelled" : "",
         chain_dones);
   check(   pushed && back && chain_seen == 1 && chain_dones == 1
         && chain_saw(0, slow, TASK_DBSCAN_RAN, chain_added[0])
         && chain_totals.ran == 1 && chain_totals.cancelled, what, detail);

   /* what the menu holds before the chain, to find again after it */
   manual_content_scan_reset_settings();
   strlcpy(manual_content_scan_get_system_name_custom_ptr(), "Kept Name",
         manual_content_scan_get_system_name_custom_size());
   pushed  = chain_push(chain_items(1, a, NULL), 1);
   running = task_dbscan_chain_is_running();
   refused = !task_push_dbscan_chain(chain_items(1, b, NULL), 1,
         chain_item_cb, chain_done_cb, &chain_seen);
   busy_before  = busy_pushed;
   scan_refused = !task_push_dbscan(b, scan_cb, NULL);
   busy_scan    = busy_pushed - busy_before;
   chain_wait(false);
   sprintf(what, "chain %s: a second chain refused", q);
   sprintf(detail, "%u items, done %u", chain_seen, chain_dones);
   check(   pushed && refused && chain_seen == 1 && chain_dones == 1
         && chain_saw(0, a, TASK_DBSCAN_RAN, 0), what, detail);
   sprintf(what, "chain %s: running from the push to the end", q);
   check(running && !task_dbscan_chain_is_running(), what, "");
   sprintf(what, "chain %s: a scan refused while it runs says why", q);
   sprintf(detail, "refused %d, told %u", (int)scan_refused, busy_scan);
   check(scan_refused && busy_scan == 1, what, detail);
   sprintf(what, "chain %s: the Manual Scan settings come back", q);
   check(!strcmp(manual_content_scan_get_system_name_custom_ptr(),
            "Kept Name"), what,
         manual_content_scan_get_system_name_custom_ptr());
   manual_content_scan_reset_settings();

   /* the done callback starts the next chain */
   chain_seen        = 0;
   chain_dones       = 0;
   chain_ud_ok       = true;
   chain_restart_ok  = false;
   chain_restart_arg = b;
   pushed = task_push_dbscan_chain(chain_items(1, a, NULL), 1,
         chain_item_cb, chain_done_restart_cb, &chain_seen);
   {
      time_t started = time(NULL);
      while (   pushed && chain_dones < 2
             && difftime(time(NULL), started) <= SCAN_TIMEOUT_SECONDS)
      {
         task_queue_check();
         retro_sleep(1);
      }
   }
   sprintf(what, "chain %s: its done callback may start another", q);
   sprintf(detail, "pushed %d, restarted %d, done %u, items %u",
         (int)pushed, (int)chain_restart_ok, chain_dones, chain_seen);
   check(   pushed && chain_restart_ok && chain_dones == 2
         && chain_saw(0, a, TASK_DBSCAN_RAN, 0)
         && chain_saw(1, b, TASK_DBSCAN_RAN, chain_added[1]),
         what, detail);

   /* Held, so the scan cannot end before the chain is pushed */
   slow_reset(sz_a, 0);
   retro_atomic_store_release_int(&slow_hold, 1);
   loop_active    = true;
   scan_completed = false;
   pushed         = task_push_dbscan(a, scan_cb, NULL);
   refused        = !chain_push(chain_items(1, b, NULL), 1);
   retro_atomic_store_release_int(&slow_hold, 0);
   {
      time_t started = time(NULL);

      while (loop_active && difftime(time(NULL), started)
            <= SCAN_TIMEOUT_SECONDS)
      {
         task_queue_check();
         retro_sleep(1);
      }
   }
   slow_size = 0;
   sprintf(what, "chain %s: refused while a manual scan runs", q);
   check(   pushed && refused && scan_completed && !chain_seen
         && !chain_dones, what, refused ? "refused" : "pushed");
   sprintf(what, "chain %s: no task_push_dbscan while it runs", q);
   check(scan_refused && pushed, what, scan_refused ? "refused" : "pushed");

   manual_content_scan_reset_settings();
   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
   return;

fixture:
   check(0, "fixture", "could not write the chain lane");
   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
}

/* Linked with --wrap: how many database listings, playlist-folder
 * walks and playlist loads began, and how many of them on the main
 * thread. */
enum { REFRESH_RDB = 0, REFRESH_LPL, REFRESH_LOAD, REFRESH_KINDS };
static retro_atomic_int_t refresh_began[REFRESH_KINDS];
static retro_atomic_int_t refresh_began_main[REFRESH_KINDS];

static void refresh_began_reset(void)
{
   int k;

   for (k = 0; k < REFRESH_KINDS; k++)
   {
      retro_atomic_store_release_int(&refresh_began[k],      0);
      retro_atomic_store_release_int(&refresh_began_main[k], 0);
   }
}

static void refresh_began_one(int k)
{
   retro_atomic_fetch_add_int(&refresh_began[k], 1);
   if (task_is_on_main_thread())
      retro_atomic_fetch_add_int(&refresh_began_main[k], 1);
}

/* Linked with --wrap: file stats on the main thread while at most
 * stat_loads playlist loads have begun (0: none counted). */
static int                stat_loads;
static retro_atomic_int_t stat_main;

int __real_retro_vfs_stat_64_impl(const char *path, int64_t *size);
int __wrap_retro_vfs_stat_64_impl(const char *path, int64_t *size)
{
   if (   task_is_on_main_thread() && stat_loads
       && retro_atomic_load_acquire_int(&refresh_began[REFRESH_LOAD])
          <= stat_loads)
      retro_atomic_fetch_add_int(&stat_main, 1);
   return __real_retro_vfs_stat_64_impl(path, size);
}

/* Set by the walk's own thread, which is the one that steps it. */
static retro_atomic_int_t lpl_walk_fail;
static dir_list_iter_t   *lpl_fail_iter;

dir_list_iter_t *__real_dir_list_iter_new(const char *dir, const char *ext,
      bool include_dirs, bool include_hidden, bool include_compressed,
      bool recursive, struct string_list *list);
dir_list_iter_t *__wrap_dir_list_iter_new(const char *dir, const char *ext,
      bool include_dirs, bool include_hidden, bool include_compressed,
      bool recursive, struct string_list *list)
{
   dir_list_iter_t *iter;
   if (ext && !strcmp(ext, "rdb"))
      refresh_began_one(REFRESH_RDB);
   else if (ext && !strcmp(ext, "lpl"))
   {
      refresh_began_one(REFRESH_LPL);
      prep_cancel_begun();
   }
   iter = __real_dir_list_iter_new(dir, ext, include_dirs, include_hidden,
         include_compressed, recursive, list);
   if (   ext && !strcmp(ext, "lpl")
       && retro_atomic_load_acquire_int(&lpl_walk_fail))
      lpl_fail_iter = iter;
   return iter;
}

/* Linked with --wrap: the playlist folder walk fails as out of
 * memory does. */
int __real_dir_list_iter_step(dir_list_iter_t *iter,
      bool (*within_budget)(void *ud), void *ud);
int __wrap_dir_list_iter_step(dir_list_iter_t *iter,
      bool (*within_budget)(void *ud), void *ud)
{
   if (iter && iter == lpl_fail_iter)
      return -1;
   return __real_dir_list_iter_step(iter, within_budget, ud);
}

/* The folder made as load mkdir_at_load begins (0: none). */
static int         mkdir_at_load;
static const char *mkdir_at_path;

/* Linked with --wrap: a refresh's load reads the playlist's header. */
playlist_parse_t *__real_playlist_parse_begin_header(
      const playlist_config_t *config);
playlist_parse_t *__wrap_playlist_parse_begin_header(
      const playlist_config_t *config)
{
   refresh_began_one(REFRESH_LOAD);
   prep_cancel_begun();
   if (   mkdir_at_load
       && retro_atomic_load_acquire_int(&refresh_began[REFRESH_LOAD])
          == mkdir_at_load)
      path_mkdir(mkdir_at_path);
   return __real_playlist_parse_begin_header(config);
}

/* Linked with --wrap: the task_init() this counts down to fails, as
 * out of memory. */
static retro_atomic_int_t task_init_fail_in;

retro_task_t *__real_task_init(void);
retro_task_t *__wrap_task_init(void)
{
   if (   retro_atomic_load_acquire_int(&task_init_fail_in) > 0
       && retro_atomic_fetch_add_int(&task_init_fail_in, -1) == 1)
      return NULL;
   return __real_task_init();
}

/* Writes @lpl with one entry, @entry, and unless @dir is NULL a scan
 * record for @dir with the saved core @core (NULL: detect). */
static bool refresh_write(const char *lpl, const char *dir,
      const char *core, const char *entry)
{
   playlist_config_t    *cfg = (playlist_config_t*)calloc(1, sizeof(*cfg));
   playlist_t           *pl  = NULL;
   struct playlist_entry e;
   bool                  ok  = false;

   if (!cfg)
      return false;
   playlist_config_set_path(cfg, lpl);
   cfg->capacity = COLLECTION_SIZE;
   if ((pl = playlist_init(cfg)))
   {
      memset(&e, 0, sizeof(e));
      e.path      = (char*)entry;
      e.label     = (char*)"Stale";
      e.core_path = (char*)"DETECT";
      e.core_name = (char*)"DETECT";
      ok = playlist_push(pl, &e);
      if (dir)
      {
         playlist_set_scan_content_dir(pl, dir);
         playlist_set_scan_search_recursively(pl, true);
         if (core)
            playlist_set_default_core_name(pl, core);
      }
      playlist_write_file(pl);
      playlist_free(pl);
      ok = ok && path_is_valid(lpl);
   }
   free(cfg);
   return ok;
}

static void refresh_item(task_dbscan_item_t *it,
      enum task_dbscan_item_kind kind, const char *arg)
{
   it->kind      = kind;
   it->arg       = arg ? ovr_str(arg) : NULL;
   it->overrides = NULL;
}

/* Runs @items with stdout in @out and stderr in @err; @cli as for a
 * --scan run. */
static bool refresh_run(task_dbscan_item_t *items, size_t n,
      const char *out, const char *err, bool cli)
{
   bool captured, pushed;

   cli_output = cli;
   captured   = capture_begin(out, err);
   pushed     = chain_push(items, n);
   chain_wait(false);
   if (captured)
      capture_end();
   cli_output = false;
   return captured && pushed && chain_dones == 1;
}

/* True if @dir lists @name in that case; @detail gets what it lists. */
static bool listed_as(const char *dir, const char *name, char *detail,
      size_t size)
{
   size_t              i;
   bool                found = false;
   struct string_list *l     = dir_list_new(dir, "lpl", false, false,
         false, false);

   *detail = '\0';
   for (i = 0; l && i < l->size; i++)
   {
      const char *base = path_basename(l->elems[i].data);

      if (i)
         strlcat(detail, " ", size);
      strlcat(detail, base, size);
      if (string_is_equal(base, name))
         found = true;
   }
   string_list_free(l);
   return found;
}

#ifdef HAVE_SMBCLIENT
/* An smb:// folder that cannot be scanned says why: --scan's line on
 * stderr (the item fails, no totals), the menu's notification with
 * the folder appended, one log line.  Without a reason, "not found"
 * and the invalid content message.  The stub VFS finds nothing;
 * smb_url_check() answers as the case says, and smb_url_failure() is
 * asked only when that passes.  A local folder asks neither. */
static void smb_reason_lane(const char *root, bool threaded)
{
   static const struct
   {
      const char         *name;
      enum smb_url_result check;
      enum smb_failure    failure;
      const char         *detail;
      enum msg_hash_enums reason; /* MSG_UNKNOWN: none */
   } cases[] = {
      { "no server",      SMB_URL_NO_SERVER,     SMB_FAILURE_UNKNOWN,
            "",          MSG_SCAN_SMB_NO_SERVER },
      { "share differs",  SMB_URL_SHARE_DIFFERS, SMB_FAILURE_UNKNOWN,
            "retro",     MSG_SCAN_SMB_SHARE_DIFFERS },
      { "cannot resolve", SMB_URL_OK,            SMB_FAILURE_RESOLVE,
            "",          MSG_SCAN_SMB_CANNOT_RESOLVE },
      { "cannot connect", SMB_URL_OK,            SMB_FAILURE_CONNECT,
            "",          MSG_SCAN_SMB_CANNOT_CONNECT },
      { "login failed",   SMB_URL_OK,            SMB_FAILURE_LOGIN,
            "",          MSG_SCAN_SMB_LOGIN_FAILED },
      { "no share",       SMB_URL_OK,            SMB_FAILURE_SHARE,
            "",          MSG_SCAN_SMB_NO_SHARE },
      { "connected",      SMB_URL_OK,            SMB_FAILURE_CONNECTED,
            "",          MSG_UNKNOWN },
      { "no record",      SMB_URL_OK,            SMB_FAILURE_UNKNOWN,
            "",          MSG_UNKNOWN }
   };
   const char         *url = "smb://host/share/roms";
   const char         *q   = threaded ? "(threaded)" : "(regular)";
   char                out[512], err[512], log[512], pl[512], local[512];
   char                line[512], menu[512], what[96], detail[64];
   struct string_list *l;
   size_t              i;
   bool                captured, pushed, off_main = false;
   char               *saved_pl = strdup(
         config_get_ptr()->paths.directory_playlist);

#ifdef HAVE_THREADS
   off_main = threaded;
#endif
   sprintf(out,   "%s/smb_reason.out",     root);
   sprintf(err,   "%s/smb_reason.err",     root);
   sprintf(log,   "%s/smb_reason.log",     root);
   sprintf(pl,    "%s/smb_reason_pl",      root);
   sprintf(local, "%s/smb_reason_missing", root);
   path_mkdir(pl);
   strlcpy(config_get_ptr()->paths.directory_playlist, pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   use_queue(threaded);
   for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
   {
      const char *text = cases[i].reason == MSG_UNKNOWN
            ? "not found" : msg_hash_to_str_us(cases[i].reason);
      int         checks_n, main_n, failures_n;

      smb_stub_check   = cases[i].check;
      smb_stub_failure = cases[i].failure;
      smb_stub_detail  = cases[i].detail;
      retro_atomic_store_release_int(&smb_stub_checks,      0);
      retro_atomic_store_release_int(&smb_stub_checks_main, 0);
      retro_atomic_store_release_int(&smb_stub_failures,    0);
      if (*cases[i].detail)
      {
         sprintf(line, "%s: %s %s", url, text, cases[i].detail);
         sprintf(menu, "%s %s: %s", text, cases[i].detail, url);
      }
      else
      {
         sprintf(line, "%s: %s", url, text);
         sprintf(menu, "%s: %s", text, url);
      }

      invalid_content_pushed = false;
      push_seen              = false;
      push_watch             = menu;
      retro_main_log_file_init(log, false);
      verbosity_enable();
      cli_output = true;
      captured   = capture_begin(out, err);
      pushed     = chain_push(chain_items(1, url, NULL), 1);
      chain_wait(false);
      if (captured)
         capture_end();
      cli_output = false;
      verbosity_disable();
      retro_main_log_file_deinit();
      push_watch = NULL;
      sprintf(what, "smb %s: %s, --scan", q, cases[i].name);
      check(   captured && pushed && chain_seen == 1
            && chain_outcomes[0] == TASK_DBSCAN_FAILED
            && !strcmp(chain_names[0], url)
            && chain_totals.failed == 1 && !chain_totals.ran
            && file_contains(err, line) && !file_contains(out, " added, "),
            what, line);
      sprintf(what, "smb %s: %s, --scan notification", q, cases[i].name);
      check(   push_seen == (cases[i].reason != MSG_UNKNOWN)
            && !invalid_content_pushed, what, menu);
      sprintf(what, "smb %s: %s, --scan log", q, cases[i].name);
      if (cases[i].reason == MSG_UNKNOWN)
         check(captured && !file_contains(log, "SMB"), what, "no SMB line");
      else
      {
         char want[600];

         sprintf(want, "[Scanner] %s", line);
         check(captured && file_contains(log, want), what, want);
      }
      checks_n   = retro_atomic_load_acquire_int(&smb_stub_checks);
      main_n     = retro_atomic_load_acquire_int(&smb_stub_checks_main);
      failures_n = retro_atomic_load_acquire_int(&smb_stub_failures);
      sprintf(what, "smb %s: %s, asked in order", q, cases[i].name);
      sprintf(detail, "check %d (%d main), failure %d", checks_n, main_n,
            failures_n);
      check(   checks_n == 1 && (!off_main || !main_n)
            && failures_n == (cases[i].check == SMB_URL_OK ? 1 : 0),
            what, detail);

      invalid_content_pushed = false;
      push_seen              = false;
      push_watch             = menu;
      retro_main_log_file_init(log, false);
      verbosity_enable();
      captured = capture_scan(url, out, err, false);
      verbosity_disable();
      retro_main_log_file_deinit();
      push_watch = NULL;
      sprintf(what, "smb %s: %s, menu", q, cases[i].name);
      if (cases[i].reason == MSG_UNKNOWN)
      {
         sprintf(detail, "failed %d, invalid content %d",
               (int)scan_result.failed, (int)invalid_content_pushed);
         check(   captured && scan_result.failed && invalid_content_pushed
               && !push_seen, what, detail);
      }
      else
         check(   captured && scan_result.failed && push_seen
               && !invalid_content_pushed, what, menu);
      sprintf(what, "smb %s: %s, log", q, cases[i].name);
      if (cases[i].reason == MSG_UNKNOWN)
         check(captured && !file_contains(log, "SMB"), what, "no SMB line");
      else
      {
         char want[600];

         sprintf(want, "[Scanner] %s", line);
         check(captured && file_contains(log, want), what, want);
      }
   }
   l = dir_list_new(pl, "lpl", false, false, false, false);
   sprintf(what, "smb %s: no playlist written", q);
   check(l && !l->size, what, pl);
   string_list_free(l);

   retro_atomic_store_release_int(&smb_stub_checks,   0);
   retro_atomic_store_release_int(&smb_stub_failures, 0);
   captured = capture_scan(local, out, err, false);
   sprintf(what, "smb %s: a local folder asks neither", q);
   check(   captured
         && !retro_atomic_load_acquire_int(&smb_stub_checks)
         && !retro_atomic_load_acquire_int(&smb_stub_failures), what, local);
   if (saved_pl)
      strlcpy(config_get_ptr()->paths.directory_playlist, saved_pl,
            sizeof(config_get_ptr()->paths.directory_playlist));
   free(saved_pl);
}

/* A refresh of a playlist saved with an smb:// folder that cannot be
 * scanned skips it with the reason and the folder, on stderr and in
 * the log, and leaves its file alone; a local playlist beside it
 * refreshes.  Without a reason, "content folder not found".
 * The load asks, off the main thread when the queue has a worker. */
static void smb_refresh_lane(const char *root, uint32_t crc_a,
      uint32_t sz_a, bool threaded)
{
   static const struct
   {
      const char         *name;
      enum smb_url_result check;
      enum smb_failure    failure;
      const char         *detail;
      enum msg_hash_enums reason; /* MSG_UNKNOWN: none */
   } cases[] = {
      { "no server",      SMB_URL_NO_SERVER,     SMB_FAILURE_UNKNOWN,
            "",          MSG_SCAN_SMB_NO_SERVER },
      { "share differs",  SMB_URL_SHARE_DIFFERS, SMB_FAILURE_UNKNOWN,
            "retro",     MSG_SCAN_SMB_SHARE_DIFFERS },
      { "cannot resolve", SMB_URL_OK,            SMB_FAILURE_RESOLVE,
            "",          MSG_SCAN_SMB_CANNOT_RESOLVE },
      { "cannot connect", SMB_URL_OK,            SMB_FAILURE_CONNECT,
            "",          MSG_SCAN_SMB_CANNOT_CONNECT },
      { "login failed",   SMB_URL_OK,            SMB_FAILURE_LOGIN,
            "",          MSG_SCAN_SMB_LOGIN_FAILED },
      { "no share",       SMB_URL_OK,            SMB_FAILURE_SHARE,
            "",          MSG_SCAN_SMB_NO_SHARE },
      { "connected",      SMB_URL_OK,            SMB_FAILURE_CONNECTED,
            "",          MSG_UNKNOWN },
      { "no record",      SMB_URL_OK,            SMB_FAILURE_UNKNOWN,
            "",          MSG_UNKNOWN }
   };
   const char         *url = "smb://host/share/roms";
   const char         *q   = threaded ? "(threaded)" : "(regular)";
   char                pl[512], a[512], ref[512], lpl_a[1024], lpl_s[1024];
   char                out[512], err[512], log[512], p[1024], stale[1024];
   char                line[256], want[320], what[96], detail[96];
   task_dbscan_item_t *items;
   size_t              i;
   bool                ran, off_main = false;
   char               *saved_pl = strdup(
         config_get_ptr()->paths.directory_playlist);

#ifdef HAVE_THREADS
   off_main = threaded;
#endif
   sprintf(pl,    "%s/smb_refresh_pl",      root);
   sprintf(a,     "%s/smb_refresh_a",       root);
   sprintf(ref,   "%s/smb_refresh_ref.lpl", root);
   sprintf(out,   "%s/smb_refresh.out",     root);
   sprintf(err,   "%s/smb_refresh.err",     root);
   sprintf(log,   "%s/smb_refresh.log",     root);
   sprintf(lpl_a, "%s/a_local.lpl",         pl);
   sprintf(lpl_s, "%s/s_smb.lpl",           pl);
   sprintf(stale, "%s/00_gone.bin",         a);
   path_mkdir(pl);
   path_mkdir(a);
   delete_playlists(pl);
   filestream_delete(ref);
   sprintf(p, "%s/01_alpha.bin", a);
   if (   !write_content(p, crc_a, sz_a)
       || !refresh_write(ref, url, NULL, stale))
      goto fixture;
   strlcpy(config_get_ptr()->paths.directory_playlist, pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   use_queue(threaded);
   for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
   {
      const char *text = cases[i].reason == MSG_UNKNOWN
            ? "content folder not found"
            : msg_hash_to_str_us(cases[i].reason);
      int         checks_n, main_n, failures_n;

      smb_stub_check   = cases[i].check;
      smb_stub_failure = cases[i].failure;
      smb_stub_detail  = cases[i].detail;
      retro_atomic_store_release_int(&smb_stub_checks,      0);
      retro_atomic_store_release_int(&smb_stub_checks_main, 0);
      retro_atomic_store_release_int(&smb_stub_failures,    0);
      /* A stale entry again, so the local one adds its file each time */
      delete_playlists(pl);
      if (   !refresh_write(lpl_a, a,   NULL, stale)
          || !refresh_write(lpl_s, url, NULL, stale)
          || !(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
         goto fixture;
      refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
      if (*cases[i].detail)
         sprintf(line, "s_smb: skipped: %s %s (%s)", text,
               cases[i].detail, url);
      else
         sprintf(line, "s_smb: skipped: %s (%s)", text, url);

      retro_main_log_file_init(log, false);
      verbosity_enable();
      ran = refresh_run(items, 1, out, err, true);
      verbosity_disable();
      retro_main_log_file_deinit();
      sprintf(what, "smb refresh %s: %s, refresh-all", q, cases[i].name);
      sprintf(detail, "%u items, ran %u, added %u, failed %u, skipped %u",
            chain_seen, chain_totals.ran, chain_totals.added,
            chain_totals.failed, chain_totals.skipped);
      check(   ran && chain_seen == 2
            && chain_saw(0, "a_local", TASK_DBSCAN_RAN,     1)
            && chain_saw(1, "s_smb",   TASK_DBSCAN_SKIPPED, -1)
            && chain_totals.ran == 1 && chain_totals.added == 1
            && !chain_totals.failed && chain_totals.skipped == 1,
            what, detail);
      sprintf(what, "smb refresh %s: %s, the skip line", q, cases[i].name);
      check(file_contains(err, line) && !file_contains(out, "s_smb"),
            what, line);
      sprintf(what, "smb refresh %s: %s, log", q, cases[i].name);
      sprintf(want, "[Playlist Refresh] %s.", line);
      check(file_contains(log, want), what, want);
      checks_n   = retro_atomic_load_acquire_int(&smb_stub_checks);
      main_n     = retro_atomic_load_acquire_int(&smb_stub_checks_main);
      failures_n = retro_atomic_load_acquire_int(&smb_stub_failures);
      sprintf(what, "smb refresh %s: %s, asked by the load", q,
            cases[i].name);
      sprintf(detail, "check %d (%d main), failure %d", checks_n, main_n,
            failures_n);
      check(   checks_n == 1 && (!off_main || !main_n)
            && failures_n == (cases[i].check == SMB_URL_OK ? 1 : 0),
            what, detail);
      sprintf(what, "smb refresh %s: %s, s_smb.lpl untouched", q,
            cases[i].name);
      check(files_equal(ref, lpl_s), what, lpl_s);
   }

   /* --scan-refresh=NAME skips it the same way. */
   smb_stub_check   = SMB_URL_SHARE_DIFFERS;
   smb_stub_failure = SMB_FAILURE_UNKNOWN;
   smb_stub_detail  = "retro";
   if (!(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH, "s_smb");
   ran = refresh_run(items, 1, out, err, true);
   sprintf(line, "s_smb: skipped: %s retro (%s)",
         msg_hash_to_str_us(MSG_SCAN_SMB_SHARE_DIFFERS), url);
   sprintf(what, "smb refresh %s: by name, skipped", q);
   check(   ran && chain_seen == 1
         && chain_saw(0, "s_smb", TASK_DBSCAN_SKIPPED, -1)
         && chain_totals.skipped == 1 && !chain_totals.ran
         && !chain_totals.failed && file_contains(err, line)
         && files_equal(ref, lpl_s), what, line);
   goto done;

fixture:
   check(0, "fixture", "could not write the smb refresh lane");
done:
   smb_stub_check   = SMB_URL_OK;
   smb_stub_failure = SMB_FAILURE_UNKNOWN;
   smb_stub_detail  = "";
   /* A refresh leaves its playlist's settings; later lanes scan with
    * the defaults. */
   manual_content_scan_reset_settings();
   if (saved_pl)
      strlcpy(config_get_ptr()->paths.directory_playlist, saved_pl,
            sizeof(config_get_ptr()->paths.directory_playlist));
   free(saved_pl);
}

enum smb_kind_push
{
   SMB_KIND_DBSCAN = 0, /* task_push_dbscan(), kind unknown */
   SMB_KIND_FOLDER,     /* task_push_dbscan_known(), a folder */
   SMB_KIND_FILE,       /* task_push_dbscan_known(), a file */
   SMB_KIND_AS_SET      /* the settings as set */
};

static void smb_kind_reset(void)
{
   retro_atomic_store_release_int(&smb_stub_stats_main, 0);
   retro_atomic_store_release_int(&smb_stats_at_push,   -1);
   retro_atomic_store_release_int(&smb_stub_opendirs,   0);
}

/* Scans @path as run_scan() does, pushed as @how says.  Returns the
 * main thread's smb:// stats before the push (since smb_kind_reset()
 * for SMB_KIND_AS_SET), or -1 if none was. */
static int smb_kind_scan(const char *path, enum smb_kind_push how)
{
   time_t started;
   bool   pushed;

   if (how != SMB_KIND_AS_SET)
      smb_kind_reset();
   loop_active        = true;
   scan_completed     = false;
   scan_result.failed = true;
   switch (how)
   {
      case SMB_KIND_DBSCAN:
         pushed = task_push_dbscan(path, scan_cb, &scan_result);
         break;
      case SMB_KIND_FOLDER:
      case SMB_KIND_FILE:
         pushed = task_push_dbscan_known(path, how == SMB_KIND_FOLDER,
               scan_cb, &scan_result);
         break;
      default:
         pushed = task_push_manual_content_scan(false, scan_cb,
               &scan_result);
         break;
   }
   if (!pushed)
      return -1;
   started = time(NULL);
   while (loop_active)
   {
      task_queue_check();
      if (difftime(time(NULL), started) > SCAN_TIMEOUT_SECONDS)
         break;
      retro_sleep(1);
   }
   return retro_atomic_load_acquire_int(&smb_stats_at_push);
}

/* A chain run of @items; the main thread's smb:// stats before its
 * scan was pushed. */
static int smb_kind_chain(task_dbscan_item_t *items, const char *out,
      const char *err, bool *ran)
{
   smb_kind_reset();
   *ran = refresh_run(items, 1, out, err, true);
   return retro_atomic_load_acquire_int(&smb_stats_at_push);
}

/* An smb:// path of known kind is not stat'd on the main thread
 * before its scan is pushed: Scan Directory and Scan File, the menu's
 * folder pick, a lone refresh, and the chain's --scan and refresh
 * items, whose path or load task stats it off the main thread
 * instead.  Unknown kind is stat'd.  A single smb:// file keeps its
 * folder's playlist name and single-file database scan, saves no
 * settings and ignores Overwrite, as when the stat told it. */
static void smb_kind_lane(const char *root, bool threaded)
{
   const char *dir  = "smb://host/share/roms";
   const char *file = "smb://host/share/roms/01_alpha.bin";
   const char *q    = threaded ? "(threaded)" : "(regular)";
   char        pl[512], lpl[1024], ref[1024], loc[512], p[1024];
   char        out[512], err[512], line[1024], what[96], detail[160];
   manual_content_scan_task_config_t *cfg = (manual_content_scan_task_config_t*)
         calloc(2, sizeof(*cfg));
   task_dbscan_result_t res;
   task_dbscan_item_t  *items;
   playlist_config_t   *pcfg = (playlist_config_t*)calloc(1, sizeof(*pcfg));
   playlist_t          *plst;
   int                  n, n2, task_n;
   bool                 ok, ok2, ran, as_folder, as_file, off_main = false;
   char                *saved_pl = strdup(
         config_get_ptr()->paths.directory_playlist);

#ifdef HAVE_THREADS
   off_main = threaded;
#endif
   /* The path or load task's one stat */
   task_n = off_main ? 0 : 1;
   if (!cfg || !pcfg)
      goto fixture;
   sprintf(pl,  "%s/smb_kind_pl",       root);
   sprintf(loc, "%s/smb_kind_local",    root);
   sprintf(lpl, "%s/roms.lpl",          pl);
   sprintf(ref, "%s/smb_kind_ref.lpl",  root);
   sprintf(out, "%s/smb_kind.out",      root);
   sprintf(err, "%s/smb_kind.err",      root);
   path_mkdir(pl);
   path_mkdir(loc);
   delete_playlists(pl);
   sprintf(p, "%s/02_beta.bin", loc);
   if (!write_content(p, 0x52A3ED00u, 1024))
      goto fixture;
   strlcpy(config_get_ptr()->paths.directory_playlist, pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   use_queue(threaded);
   smb_stub_check   = SMB_URL_OK;
   smb_stub_failure = SMB_FAILURE_UNKNOWN;
   smb_stub_detail  = "";
   smb_stub_live    = true;

   /* Unknown kind: set_menu_content_dir and get_task_config stat it;
    * the directory flag comes from the config they made. */
   manual_content_scan_reset_settings();
   n  = smb_kind_scan(dir,  SMB_KIND_DBSCAN);
   n2 = smb_kind_scan(file, SMB_KIND_DBSCAN);
   sprintf(what, "smb kind %s: unknown kind stats the path", q);
   sprintf(detail, "folder %d, file %d", n, n2);
   check(n == 2 && n2 == 2, what, detail);

   /* Scan Directory, Scan File.  Only a folder push opens the
    * folder. */
   n  = smb_kind_scan(dir,  SMB_KIND_FOLDER);
   as_folder = retro_atomic_load_acquire_int(&smb_stub_opendirs) == 1
         && manual_content_scan_get_task_config(&cfg[0], pl)
         && !cfg[0].content_is_file;
   n2 = smb_kind_scan(file, SMB_KIND_FILE);
   as_file = !retro_atomic_load_acquire_int(&smb_stub_opendirs);
   sprintf(what, "smb kind %s: Scan Directory and Scan File", q);
   sprintf(detail, "folder %d, as a folder %d, file %d, as a file %d",
         n, (int)as_folder, n2, (int)as_file);
   check(n == 0 && as_folder && n2 == 0 && as_file, what, detail);

   /* The menu's folder pick */
   smb_kind_reset();
   ok  = manual_content_scan_set_menu_content_dir_kind(dir,
         MANUAL_CONTENT_SCAN_PATH_FOLDER);
   as_folder = ok && manual_content_scan_get_task_config(&cfg[0], pl)
         && !cfg[0].content_is_file;
   n   = ok ? smb_kind_scan(dir, SMB_KIND_AS_SET) : -1;
   as_folder = as_folder
         && retro_atomic_load_acquire_int(&smb_stub_opendirs) == 1;
   smb_kind_reset();
   ok2 = manual_content_scan_set_menu_content_dir_kind(file,
         MANUAL_CONTENT_SCAN_PATH_FILE);
   n2  = ok2 ? smb_kind_scan(file, SMB_KIND_AS_SET) : -1;
   as_file = ok2 && !retro_atomic_load_acquire_int(&smb_stub_opendirs);
   sprintf(what, "smb kind %s: the menu's folder pick", q);
   sprintf(detail, "folder %d, as a folder %d, file %d, as a file %d",
         n, (int)as_folder, n2, (int)as_file);
   check(n == 0 && as_folder && n2 == 0 && as_file, what, detail);

   /* A lone refresh: the saved folder is a folder. */
   delete_playlists(pl);
   n = -1;
   if (!refresh_write(lpl, dir, NULL, "smb://host/share/roms/00_old.bin"))
      goto fixture;
   playlist_config_set_path(pcfg, lpl);
   pcfg->capacity = COLLECTION_SIZE;
   if ((plst = playlist_init(pcfg)))
   {
      manual_content_scan_reset_settings();
      smb_kind_reset();
      if (manual_content_scan_set_menu_from_playlist(plst,
            config_get_ptr()->paths.path_content_database, false)
            == MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_OK)
         n = smb_kind_scan(dir, SMB_KIND_AS_SET);
      playlist_free(plst);
   }
   sprintf(what, "smb kind %s: a lone refresh", q);
   sprintf(detail, "%d", n);
   check(n == 0, what, detail);

   /* The chain: --scan of a folder and of a file with
    * --scan-overwrite, --scan-refresh, refresh-all. */
   delete_playlists(pl);
   if (!(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
      goto fixture;
   chain_item(&items[0], dir, NULL);
   if ((items[0].overrides = manual_content_scan_overrides_new()))
      items[0].overrides->flags |= MANUAL_CONTENT_SCAN_OVR_OVERWRITE;
   n = smb_kind_chain(items, out, err, &ran);
   sprintf(what, "smb kind %s: --scan of a folder", q);
   sprintf(detail, "%d (want %d), %u items", n, task_n, chain_seen);
   check(   ran && n == task_n && chain_seen == 1
         && !file_contains(err, "single file")
         && !file_contains(err, "not found"), what, detail);
   if (!(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
      goto fixture;
   chain_item(&items[0], file, NULL);
   if ((items[0].overrides = manual_content_scan_overrides_new()))
      items[0].overrides->flags |= MANUAL_CONTENT_SCAN_OVR_OVERWRITE;
   n = smb_kind_chain(items, out, err, &ran);
   sprintf(line, "%s: --scan-overwrite ignored for a single file", file);
   sprintf(what, "smb kind %s: --scan of a file", q);
   sprintf(detail, "%d (want %d), %u items", n, task_n, chain_seen);
   check(   ran && n == task_n && chain_seen == 1
         && file_contains(err, line), what, detail);

   delete_playlists(pl);
   if (!refresh_write(lpl, dir, NULL, "smb://host/share/roms/00_old.bin")
         || !(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH, "roms");
   n  = smb_kind_chain(items, out, err, &ran);
   ok = ran;
   if (!(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
   n2 = smb_kind_chain(items, out, err, &ran);
   /* Refresh All loads the playlist again to run it. */
   sprintf(what, "smb kind %s: --scan-refresh and refresh-all", q);
   sprintf(detail, "%d, %d (want %d, %d)", n, n2, task_n, 2 * task_n);
   check(ok && ran && n == task_n && n2 == 2 * task_n, what, detail);

   /* A picked file is one still once a chain has put the settings
    * back. */
   manual_content_scan_reset_settings();
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_CUSTOM);
   manual_content_scan_set_menu_scan_use_db(MANUAL_CONTENT_SCAN_USE_DB_NONE);
   ok = manual_content_scan_set_menu_content_dir_kind(file,
         MANUAL_CONTENT_SCAN_PATH_FILE);
   delete_playlists(pl);
   if (!refresh_write(lpl, dir, NULL, "smb://host/share/roms/00_old.bin")
         || !(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
   smb_kind_chain(items, out, err, &ran);
   smb_kind_reset();
   ok2 = ok && ran && manual_content_scan_get_task_config(&cfg[0], pl);
   n   = retro_atomic_load_acquire_int(&smb_stub_stats_main);
   sprintf(what, "smb kind %s: a picked file after refresh-all", q);
   sprintf(detail, "%s, file %d, %d stats",
         ok2 ? path_basename(cfg[0].content_dir) : "-",
         ok2 && cfg[0].content_is_file, n);
   check(   ok2 && n == 0 && cfg[0].content_is_file
         && !strcmp(cfg[0].content_dir, file), what, detail);

   /* One file: the folder's playlist name, no Overwrite, from the
    * kind as from the stat. */
   manual_content_scan_reset_settings();
   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_CUSTOM);
   manual_content_scan_set_menu_scan_use_db(MANUAL_CONTENT_SCAN_USE_DB_NONE);
   *manual_content_scan_get_overwrite_playlist_ptr() = true;
   ok = manual_content_scan_set_menu_content_dir(file)
      && manual_content_scan_get_task_config(&cfg[0], pl);
   smb_kind_reset();
   ok2 = manual_content_scan_set_menu_content_dir_kind(file,
         MANUAL_CONTENT_SCAN_PATH_FILE)
      && manual_content_scan_get_task_config(&cfg[1], pl);
   n = retro_atomic_load_acquire_int(&smb_stub_stats_main);
   sprintf(what, "smb kind %s: one file's playlist and Overwrite", q);
   sprintf(detail, "%s, %s, overwrite %d, %d stats",
         ok ? path_basename(cfg[0].playlist_file) : "-",
         ok2 ? path_basename(cfg[1].playlist_file) : "-",
         (int)cfg[1].overwrite_playlist, n);
   check(   ok && ok2 && n == 0
         && cfg[0].content_is_file && cfg[1].content_is_file
         && !cfg[0].overwrite_playlist && !cfg[1].overwrite_playlist
         && !strcmp(cfg[0].playlist_file, cfg[1].playlist_file)
         && !strcmp(path_basename(cfg[1].playlist_file), "roms.lpl"),
         what, detail);
   sprintf(p, "%s/02_beta.bin", loc);
   ok = manual_content_scan_set_menu_content_dir_kind(p,
         MANUAL_CONTENT_SCAN_PATH_FILE)
      && manual_content_scan_get_task_config(&cfg[0], pl);
   sprintf(what, "smb kind %s: a local file's playlist", q);
   check(   ok && cfg[0].content_is_file
         && !strcmp(path_basename(cfg[0].playlist_file),
               "smb_kind_local.lpl"), what,
         ok ? path_basename(cfg[0].playlist_file) : "-");

   /* With no database: one entry added to the folder's playlist,
    * no settings saved, Overwrite and Validate not applied. */
   *manual_content_scan_get_validate_entries_ptr() = true;
   delete_playlists(pl);
   if (!refresh_write(lpl, NULL, NULL, "smb://host/share/roms/00_old.bin"))
      goto fixture;
   n = smb_kind_scan(file, SMB_KIND_DBSCAN);
   ok = !filestream_rename(lpl, ref);
   if (!refresh_write(lpl, NULL, NULL, "smb://host/share/roms/00_old.bin"))
      goto fixture;
   n2 = smb_kind_scan(file, SMB_KIND_FILE);
   sprintf(what, "smb kind %s: one file with no database", q);
   sprintf(detail, "%d, %d stats, same %d, new %d, old %d", n, n2,
         files_equal(ref, lpl), file_contains(lpl, "01_alpha.bin"),
         file_contains(lpl, "00_old.bin"));
   check(   ok && n == 2 && n2 == 0 && files_equal(ref, lpl)
         && file_contains(lpl, "01_alpha.bin")
         && file_contains(lpl, "00_old.bin")
         && !scan_record(lpl, line, p, sizeof(p), &ran), what, detail);
   filestream_delete(ref);

   /* With the database: the single-file scan, not a folder's. */
   manual_content_scan_reset_settings();
   delete_playlists(pl);
   smb_kind_scan(file, SMB_KIND_DBSCAN);
   res = scan_result;
   smb_kind_scan(file, SMB_KIND_FILE);
   sprintf(what, "smb kind %s: one file with the database", q);
   sprintf(detail, "added %u/%u, unmatched %u/%u, errors %u/%u, "
         "failed %d/%d", res.added, scan_result.added, res.unmatched,
         scan_result.unmatched, res.errors, scan_result.errors,
         (int)res.failed, (int)scan_result.failed);
   check(   res.added == scan_result.added
         && res.unmatched == scan_result.unmatched
         && res.errors == scan_result.errors
         && res.failed == scan_result.failed, what, detail);
   printf("  info smb kind %s: one file with the database: %s\n", q,
         detail);
   goto done;

fixture:
   check(0, "fixture", "could not write the smb kind lane");
done:
   smb_stub_live = false;
   delete_playlists(pl);
   manual_content_scan_reset_settings();
   if (saved_pl)
      strlcpy(config_get_ptr()->paths.directory_playlist, saved_pl,
            sizeof(config_get_ptr()->paths.directory_playlist));
   free(saved_pl);
   free(pcfg);
   free(cfg);
}
#endif

/* Refresh items over a playlist folder: a refresh-all takes every
 * playlist with saved settings in name order, history and favorites
 * never, skips one whose folder is gone and fails one whose core is;
 * it refreshes a playlist an earlier item wrote.  A refresh names its
 * playlist by name or by a path in the playlist folder.  The walks and
 * loads run on the worker when the queue has one. */
static void refresh_lane(const char *root, const char *pl_dir,
      uint32_t crc_a, uint32_t sz_a, bool threaded)
{
   char                pl[512], none_pl[512], a[512], gone[512];
   char                fresh[512], other[512], case_pl[512];
   char                dup_pl[512];
   char                out[1024], err[1024];
   char                p[1024], stale[1024], want[1536], what[128];
   char                detail[160];
   const char         *q = threaded ? "(threaded)" : "(regular)";
   task_dbscan_item_t *items;
   bool                ran, off_main = false;

#ifdef HAVE_THREADS
   off_main = threaded;
#endif
   sprintf(pl,      "%s/refresh_pl",    root);
   sprintf(none_pl, "%s/refresh_none",  root);
   sprintf(a,       "%s/refresh_a",     root);
   sprintf(gone,    "%s/refresh_gone",  root);
   sprintf(fresh,   "%s/refresh_new",   root);
   sprintf(other,   "%s/refresh_other", root);
   sprintf(case_pl, "%s/refresh_case",  root);
   sprintf(dup_pl,  "%s/refresh_dup",   root);
   sprintf(out,     "%s/refresh_out.txt", root);
   sprintf(err,     "%s/refresh_err.txt", root);
   sprintf(stale,   "%s/00_gone.bin",   a);
   path_mkdir(pl);
   path_mkdir(none_pl);
   path_mkdir(a);
   path_mkdir(fresh);
   path_mkdir(other);
   path_mkdir(case_pl);
   path_mkdir(dup_pl);
   delete_playlists(pl);
   delete_playlists(none_pl);
   delete_playlists(other);
   delete_playlists(case_pl);
   delete_playlists(dup_pl);
   sprintf(p, "%s/01_alpha.bin", a);
   if (!write_content(p, crc_a, sz_a))
      goto fixture;
   sprintf(p, "%s/01_alpha.bin", fresh);
   if (!write_content(p, crc_a, sz_a))
      goto fixture;
   sprintf(p, "%s/a_ok.lpl", pl);
   if (!refresh_write(p, a, NULL, stale))
      goto fixture;
   sprintf(p, "%s/b_gone.lpl", pl);
   if (!refresh_write(p, gone, NULL, stale))
      goto fixture;
   sprintf(p, "%s/c_none.lpl", pl);
   if (!refresh_write(p, NULL, NULL, stale))
      goto fixture;
   sprintf(p, "%s/e_core.lpl", pl);
   if (!refresh_write(p, a, "No Such Core", stale))
      goto fixture;
   sprintf(p, "%s/x_history.lpl", pl);
   if (!refresh_write(p, a, NULL, stale))
      goto fixture;
   sprintf(p, "%s/content_favorites.lpl", pl);
   if (!refresh_write(p, a, NULL, stale))
      goto fixture;
   sprintf(p, "%s/c_none.lpl", none_pl);
   if (!refresh_write(p, NULL, NULL, stale))
      goto fixture;
   sprintf(p, "%s/x_history.lpl", none_pl);
   if (!refresh_write(p, a, NULL, stale))
      goto fixture;
   sprintf(p, "%s/a_ok.lpl", other);
   if (!refresh_write(p, a, NULL, stale))
      goto fixture;
   strlcpy(config_get_ptr()->paths.directory_playlist, pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   use_queue(threaded);

   /* A --scan writing n_new, then the refresh-all. */
   if (!(items = (task_dbscan_item_t*)calloc(2, sizeof(*items))))
      goto fixture;
   chain_item(&items[0], fresh, NULL);
   if ((items[0].overrides = manual_content_scan_overrides_new()))
      items[0].overrides->playlist = ovr_str("n_new");
   refresh_item(&items[1], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
   refresh_began_reset();
   ran = refresh_run(items, 2, out, err, true);
   sprintf(what, "refresh %s: refresh-all in name order", q);
   snprintf(detail, sizeof(detail), "%u items: %s, %s, %s, %s", chain_seen,
         chain_names[1], chain_names[2], chain_names[3], chain_names[4]);
   check(   ran && chain_seen == 5
         && chain_saw(0, fresh,    TASK_DBSCAN_RAN,     1)
         && chain_saw(1, "a_ok",   TASK_DBSCAN_RAN,     1)
         && chain_saw(2, "b_gone", TASK_DBSCAN_SKIPPED, -1)
         && chain_saw(3, "e_core", TASK_DBSCAN_FAILED,  -1)
         && chain_saw(4, "n_new",  TASK_DBSCAN_RAN,     0), what, detail);
   sprintf(what, "refresh %s: the totals", q);
   sprintf(detail, "ran %u, added %u, failed %u, skipped %u%s",
         chain_totals.ran, chain_totals.added, chain_totals.failed,
         chain_totals.skipped, chain_totals.cancelled ? ", cancelled" : "");
   check(   chain_totals.ran == 3 && chain_totals.added == 2
         && chain_totals.failed == 1 && chain_totals.skipped == 1
         && !chain_totals.cancelled, what, detail);
   sprintf(what, "refresh %s: a stale entry removed", q);
   sprintf(p, "%s/a_ok.lpl", pl);
   sprintf(want, "%s/01_alpha.bin", a);
   check(file_contains(p, want) && !file_contains(p, stale), what, p);
   sprintf(what, "refresh %s: history and favorites never loaded", q);
   sprintf(p, "%s/x_history.lpl", pl);
   sprintf(detail, "%d loads",
         retro_atomic_load_acquire_int(&refresh_began[REFRESH_LOAD]));
   /* Five, then a_ok, e_core and n_new again to run. */
   check(   file_contains(p, stale) && !file_contains(p, want)
         && retro_atomic_load_acquire_int(&refresh_began[REFRESH_LOAD]) == 8,
         what, detail);
   sprintf(what, "refresh %s: the refresh lines", q);
   sprintf(want, "%s: skipped: content folder not found (%s)", "b_gone",
         gone);
   check(   file_contains(out,
               "a_ok: 1 added, 0 duplicates, 0 unmatched, 0 errors")
         && file_contains(err, want)
         && file_contains(err, "e_core: saved core not found (No Such Core)")
         && !file_contains(out, "b_gone")
         && !file_contains(err, "no playlists"), what, err);
   sprintf(what, "refresh %s: walks and loads %s", q,
         off_main ? "off the main thread" : "ran");
   sprintf(detail, "rdb %d (%d main), lpl %d (%d main), loads %d (%d main)",
         retro_atomic_load_acquire_int(&refresh_began[REFRESH_RDB]),
         retro_atomic_load_acquire_int(&refresh_began_main[REFRESH_RDB]),
         retro_atomic_load_acquire_int(&refresh_began[REFRESH_LPL]),
         retro_atomic_load_acquire_int(&refresh_began_main[REFRESH_LPL]),
         retro_atomic_load_acquire_int(&refresh_began[REFRESH_LOAD]),
         retro_atomic_load_acquire_int(&refresh_began_main[REFRESH_LOAD]));
   check(   retro_atomic_load_acquire_int(&refresh_began[REFRESH_RDB]) >= 1
         && retro_atomic_load_acquire_int(&refresh_began[REFRESH_LPL]) == 1
         && (!off_main || (
               !retro_atomic_load_acquire_int(&refresh_began_main[REFRESH_RDB])
            && !retro_atomic_load_acquire_int(&refresh_began_main[REFRESH_LPL])
            && !retro_atomic_load_acquire_int(
                  &refresh_began_main[REFRESH_LOAD]))), what, detail);

   /* By name with an override, by path, and the naming failures. */
   if (!(items = (task_dbscan_item_t*)calloc(5, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH, "a_ok");
   if ((items[0].overrides = manual_content_scan_overrides_new()))
      items[0].overrides->exts = ovr_str("bin");
   sprintf(p, "%s/a_ok.lpl", pl);
   refresh_item(&items[1], TASK_DBSCAN_ITEM_REFRESH, p);
   sprintf(p, "%s/a_ok.lpl", other);
   refresh_item(&items[2], TASK_DBSCAN_ITEM_REFRESH, p);
   refresh_item(&items[3], TASK_DBSCAN_ITEM_REFRESH, "nope");
   refresh_item(&items[4], TASK_DBSCAN_ITEM_REFRESH, "c_none");
   ran = refresh_run(items, 5, out, err, true);
   sprintf(what, "refresh %s: by name, by path, and what fails", q);
   sprintf(detail, "%u items, ran %u, failed %u", chain_seen,
         chain_totals.ran, chain_totals.failed);
   check(   ran && chain_seen == 5
         && chain_saw(0, "a_ok",   TASK_DBSCAN_RAN,    0)
         && chain_saw(1, "a_ok",   TASK_DBSCAN_RAN,    0)
         && chain_saw(2, p,        TASK_DBSCAN_FAILED, -1)
         && chain_saw(3, "nope",   TASK_DBSCAN_FAILED, -1)
         && chain_saw(4, "c_none", TASK_DBSCAN_FAILED, -1)
         && chain_totals.ran == 2 && chain_totals.failed == 3
         && !chain_totals.added, what, detail);
   sprintf(what, "refresh %s: an override written back", q);
   sprintf(want, "%s/a_ok.lpl", pl);
   check(file_contains(want, "\"scan_file_exts\": \"bin\""), what, want);
   sprintf(what, "refresh %s: the naming lines", q);
   sprintf(want, "%s: not in the playlist folder (%s)", p, pl);
   check(   file_contains(err, want)
         && file_contains(err, "nope: no such playlist")
         && file_contains(err, "c_none: no saved scan settings"), what, err);

   /* A file name without a folder is the playlist folder's. */
   if (!(items = (task_dbscan_item_t*)calloc(2, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH, "a_ok.lpl");
   refresh_item(&items[1], TASK_DBSCAN_ITEM_REFRESH, "A_OK.LPL");
   ran = refresh_run(items, 2, out, err, true);
   sprintf(what, "refresh %s: a file name, not a path", q);
   check(   ran && chain_saw(0, "a_ok", TASK_DBSCAN_RAN, 0)
         && chain_saw(1, "a_ok", TASK_DBSCAN_RAN, 0)
         && path_get_size(err) == 0, what, err);

#ifdef _WIN32
   /* All '\' and '\' before the file name only. */
   if (!(items = (task_dbscan_item_t*)calloc(2, sizeof(*items))))
      goto fixture;
   sprintf(p, "%s/a_ok.lpl", pl);
   pathname_conform_slashes_to_os(p);
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH, p);
   sprintf(p, "%s\\a_ok.lpl", pl);
   refresh_item(&items[1], TASK_DBSCAN_ITEM_REFRESH, p);
   ran = refresh_run(items, 2, out, err, true);
   sprintf(what, "refresh %s: a path with either slash", q);
   sprintf(detail, "%u items, ran %u, failed %u", chain_seen,
         chain_totals.ran, chain_totals.failed);
   check(   ran && chain_seen == 2
         && chain_saw(0, "a_ok", TASK_DBSCAN_RAN, 0)
         && chain_saw(1, "a_ok", TASK_DBSCAN_RAN, 0)
         && chain_totals.ran == 2 && !chain_totals.failed, what, detail);

   /* A playlist folder with '\', a path with '/'. */
   strlcpy(p, pl, sizeof(p));
   pathname_conform_slashes_to_os(p);
   strlcpy(config_get_ptr()->paths.directory_playlist, p,
         sizeof(config_get_ptr()->paths.directory_playlist));
   if (!(items = (task_dbscan_item_t*)calloc(2, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH, "a_ok");
   sprintf(p, "%s/a_ok.lpl", pl);
   refresh_item(&items[1], TASK_DBSCAN_ITEM_REFRESH, p);
   ran = refresh_run(items, 2, out, err, true);
   strlcpy(config_get_ptr()->paths.directory_playlist, pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   sprintf(what, "refresh %s: a playlist folder with '\\'", q);
   sprintf(detail, "%u items, ran %u, failed %u", chain_seen,
         chain_totals.ran, chain_totals.failed);
   check(   ran && chain_seen == 2
         && chain_saw(0, "a_ok", TASK_DBSCAN_RAN, 0)
         && chain_saw(1, "a_ok", TASK_DBSCAN_RAN, 0)
         && chain_totals.ran == 2 && !chain_totals.failed, what, detail);
#endif

   /* Another case: the lines, the system name and the save take the
    * file's own name, so a database's playlist stays one. */
   sprintf(p, "%s/Test Beta.lpl", case_pl);
   if (!refresh_write(p, a, NULL, stale))
      goto fixture;
   strlcpy(config_get_ptr()->paths.directory_playlist, case_pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   if (!(items = (task_dbscan_item_t*)calloc(2, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH, "test beta");
   sprintf(p, "%s/TEST BETA.LPL", case_pl);
   refresh_item(&items[1], TASK_DBSCAN_ITEM_REFRESH, p);
   ran = refresh_run(items, 2, out, err, true);
   strlcpy(config_get_ptr()->paths.directory_playlist, pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   sprintf(what, "refresh %s: another case, named as the file", q);
   snprintf(detail, sizeof(detail), "%u items: %s, %s", chain_seen,
         chain_names[0], chain_names[1]);
   check(   ran && chain_seen == 2
         && !strcmp(chain_names[0], "Test Beta")
         && !strcmp(chain_names[1], "Test Beta")
         && chain_outcomes[0] == TASK_DBSCAN_RAN
         && chain_outcomes[1] == TASK_DBSCAN_RAN
         && file_contains(out, "Test Beta: ")
         && !file_contains(out, "test beta")
         && !file_contains(out, "TEST BETA"), what, detail);
   sprintf(what, "refresh %s: another case, a database's playlist", q);
   check(   chain_seen == 2
         && chain_name_types[0] == MANUAL_CONTENT_SCAN_SYSTEM_NAME_DATABASE
         && chain_name_types[1] == MANUAL_CONTENT_SCAN_SYSTEM_NAME_DATABASE,
         what, "");
   sprintf(what, "refresh %s: another case keeps the file's name", q);
   check(   listed_as(case_pl, "Test Beta.lpl", detail, sizeof(detail))
         && !strcmp(detail, "Test Beta.lpl"), what, detail);

   /* Two files in other cases: neither is the name; the exact one is. */
   sprintf(p, "%s/Test Beta.lpl", dup_pl);
   if (!refresh_write(p, a, NULL, stale))
      goto fixture;
   sprintf(p, "%s/TEST BETA.lpl", dup_pl);
   /* Where case is ignored this is the same file, already written. */
   refresh_write(p, a, NULL, stale);
   if (   listed_as(dup_pl, "Test Beta.lpl", detail, sizeof(detail))
       && listed_as(dup_pl, "TEST BETA.lpl", detail, sizeof(detail)))
   {
      strlcpy(config_get_ptr()->paths.directory_playlist, dup_pl,
            sizeof(config_get_ptr()->paths.directory_playlist));
      if (!(items = (task_dbscan_item_t*)calloc(2, sizeof(*items))))
         goto fixture;
      refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH, "test beta");
      refresh_item(&items[1], TASK_DBSCAN_ITEM_REFRESH, "Test Beta");
      ran = refresh_run(items, 2, out, err, true);
      strlcpy(config_get_ptr()->paths.directory_playlist, pl,
            sizeof(config_get_ptr()->paths.directory_playlist));
      sprintf(what, "refresh %s: two in other cases, ambiguous", q);
      check(   ran && chain_saw(0, "test beta", TASK_DBSCAN_FAILED, -1)
            && file_contains(err,
                  "test beta: 2 playlists with this name in other cases, "
                  "give the exact name\n"),
            what, err);
      sprintf(what, "refresh %s: two in other cases, the exact one", q);
      snprintf(detail, sizeof(detail), "%u items, ran %u, failed %u",
            chain_seen, chain_totals.ran, chain_totals.failed);
      check(   ran && chain_seen == 2
            && !strcmp(chain_names[1], "Test Beta")
            && chain_outcomes[1] == TASK_DBSCAN_RAN
            && chain_totals.ran == 1 && chain_totals.failed == 1,
            what, detail);
   }
   else
      printf("  skip  refresh %s: two in other cases (the file system "
            "ignores case: %s)\n", q, detail);

   /* Nothing to refresh. */
   strlcpy(config_get_ptr()->paths.directory_playlist, none_pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   if (!(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
   ran = refresh_run(items, 1, out, err, true);
   sprintf(what, "refresh %s: a refresh-all with nothing to refresh", q);
   check(   ran && !chain_seen && !chain_totals.ran && !chain_totals.failed
         && !chain_totals.skipped
         && file_contains(err, "no playlists with saved scan settings"),
         what, err);

   /* The second task_init is the refresh-all's listing. */
   if (!(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
   retro_atomic_store_release_int(&task_init_fail_in, 2);
   ran = refresh_run(items, 1, out, err, true);
   retro_atomic_store_release_int(&task_init_fail_in, 0);
   sprintf(what, "refresh %s: a refresh-all out of memory", q);
   check(   ran && chain_saw(0, none_pl, TASK_DBSCAN_FAILED, -1)
         && chain_totals.failed == 1 && !chain_totals.ran
         && file_contains(err, "out of memory"), what, err);

   /* The listing fails part way. */
   if (!(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
   retro_atomic_store_release_int(&lpl_walk_fail, 1);
   ran = refresh_run(items, 1, out, err, true);
   retro_atomic_store_release_int(&lpl_walk_fail, 0);
   lpl_fail_iter = NULL;
   sprintf(what, "refresh %s: a refresh-all whose listing fails", q);
   check(   ran && chain_saw(0, none_pl, TASK_DBSCAN_FAILED, -1)
         && chain_totals.failed == 1 && !chain_totals.ran
         && file_contains(err, "out of memory")
         && !file_contains(err, "no playlists with saved scan settings"),
         what, err);

   /* Cancelled while the folder is walked, then while its second
    * playlist loads, before the first runs: nothing runs. */
   strlcpy(config_get_ptr()->paths.directory_playlist, pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   if (!(items = (task_dbscan_item_t*)calloc(2, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
   chain_item(&items[1], fresh, NULL);
   prep_pushes    = 0;
   prep_cancel_at = 2;
   ran            = refresh_run(items, 2, out, err, true);
   prep_cancel_at = 0;
   sprintf(what, "refresh %s: cancelled while walking the folder", q);
   sprintf(detail, "%u items, ran %u, done %u%s", chain_seen,
         chain_totals.ran, chain_dones,
         chain_totals.cancelled ? ", cancelled" : "");
   check(   ran && !prep_cancel_task && !chain_seen
         && chain_totals.cancelled && !chain_totals.ran
         && !chain_totals.failed, what, detail);
   if (!(items = (task_dbscan_item_t*)calloc(2, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
   chain_item(&items[1], fresh, NULL);
   prep_pushes    = 0;
   prep_cancel_at = 4; /* the databases, the walk, a_ok, b_gone */
   ran            = refresh_run(items, 2, out, err, true);
   prep_cancel_at = 0;
   sprintf(what, "refresh %s: cancelled while loading a playlist", q);
   sprintf(detail, "%u items, ran %u, done %u%s", chain_seen,
         chain_totals.ran, chain_dones,
         chain_totals.cancelled ? ", cancelled" : "");
   check(   ran && !prep_cancel_task && !chain_seen
         && chain_totals.cancelled && !chain_totals.ran
         && !chain_totals.skipped && !chain_totals.failed, what, detail);

   /* Without --scan: the outcome, no lines. */
   if (!(items = (task_dbscan_item_t*)calloc(2, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH, "b_gone");
   refresh_item(&items[1], TASK_DBSCAN_ITEM_REFRESH, "a_ok");
   ran = refresh_run(items, 2, out, err, false);
   sprintf(what, "refresh %s: nothing prints without --scan", q);
   check(   ran && chain_saw(0, "b_gone", TASK_DBSCAN_SKIPPED, -1)
         && chain_saw(1, "a_ok", TASK_DBSCAN_RAN, 0)
         && path_get_size(out) == 0 && path_get_size(err) == 0, what, err);

   manual_content_scan_reset_settings();
   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
   return;

fixture:
   check(0, "fixture", "could not write the refresh lane");
   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
}

/* File system calls on the main thread while fs_main_watch is set:
 * the scan chain's must all run in its prep tasks.  Only the main
 * thread reads the flag. */
static bool     fs_main_watch;
static unsigned fs_main_calls;
static char     fs_main_last[64];
static const char *stat_watch_path;
static unsigned    stat_watch_calls;

static void fs_main_note(const char *fn)
{
#ifdef HAVE_THREADS
   if (current_tid() == main_tid && fs_main_watch)
   {
      fs_main_calls++;
      strlcpy(fs_main_last, fn, sizeof(fs_main_last));
   }
#else
   (void)fn;
#endif
}

int __real_path_stat(const char *path);
int __wrap_path_stat(const char *path)
{
   fs_main_note("path_stat");
#ifdef HAVE_THREADS
   if (current_tid() == main_tid)
#endif
      if (stat_watch_path && !strcmp(path, stat_watch_path))
         stat_watch_calls++;
   return __real_path_stat(path);
}

bool __real_path_is_valid(const char *path);
bool __wrap_path_is_valid(const char *path)
{
   fs_main_note("path_is_valid");
   return __real_path_is_valid(path);
}

bool __real_path_is_directory(const char *path);
bool __wrap_path_is_directory(const char *path)
{
   fs_main_note("path_is_directory");
   return __real_path_is_directory(path);
}

int64_t __real_path_get_size(const char *path);
int64_t __wrap_path_get_size(const char *path)
{
   fs_main_note("path_get_size");
   return __real_path_get_size(path);
}

char *__real_path_resolve_realpath(char *s, size_t len,
      bool resolve_symlinks);
char *__wrap_path_resolve_realpath(char *s, size_t len,
      bool resolve_symlinks)
{
   fs_main_note("path_resolve_realpath");
   return __real_path_resolve_realpath(s, len, resolve_symlinks);
}

static void local_refresh_lane(const char *root, const char *pl_dir,
      bool threaded)
{
   char dir[512], lpl[512], ref[512], entry[1024], what[128], detail[128];
   playlist_config_t *cfg = (playlist_config_t*)calloc(1, sizeof(*cfg));
   playlist_t *playlist = NULL;
   void *data = NULL;
   int64_t len;
   unsigned replaced, overwrite;
   const char *q = threaded ? "threaded" : "regular";

   sprintf(lpl, "%s/Local Offline.lpl", pl_dir);
   sprintf(ref, "%s/local_refresh.ref", root);
   if (!cfg)
      goto fixture;
   use_queue(threaded);
   for (replaced = 0; replaced < 2; replaced++)
   {
      sprintf(dir, "%s/local_refresh_%s", root,
            replaced ? "file.bin" : "missing");
      if (replaced && !write_content(dir, 0x52A3ED00u, 1024))
         goto fixture;
      sprintf(entry, "%s/game.bin", dir);
      for (overwrite = 0; overwrite < 2; overwrite++)
      {
         bool pushed;
         time_t started;

         filestream_delete(lpl);
         if (!refresh_write(lpl, dir, NULL, entry))
            goto fixture;
         playlist_config_set_path(cfg, lpl);
         cfg->capacity = COLLECTION_SIZE;
         if (!(playlist = playlist_init(cfg)))
            goto fixture;
         playlist_set_scan_db_usage(playlist, MANUAL_CONTENT_SCAN_USE_DB_NONE);
         playlist_set_scan_overwrite_playlist(playlist, overwrite != 0);
         playlist_write_file(playlist);
         if (   !filestream_read_file(lpl, &data, &len)
             || !filestream_write_file(ref, data, len))
            goto fixture;
         free(data);
         data = NULL;
         manual_content_scan_reset_settings();
         stat_watch_calls = 0;
         stat_watch_path = dir;
         pushed = manual_content_scan_set_menu_from_playlist_list(
               playlist, NULL) == MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_OK;
         playlist_free(playlist);
         playlist = NULL;
         loop_active = true;
         scan_completed = false;
         pushed = pushed && task_push_manual_content_scan(
               false, scan_cb, &scan_result);
         stat_watch_path = NULL;
         started = time(NULL);
         while (pushed && loop_active
               && difftime(time(NULL), started) <= SCAN_TIMEOUT_SECONDS)
         {
            task_queue_check();
            retro_sleep(1);
         }
         sprintf(what, "local refresh (%s): %s, overwrite %u", q,
               replaced ? "folder replaced" : "offline", overwrite);
         sprintf(detail, "%u main stats, failed %d, unchanged %d",
               stat_watch_calls, (int)scan_result.failed, files_equal(ref, lpl));
         check(   pushed && scan_completed && scan_result.failed
               && stat_watch_calls == 0 && files_equal(ref, lpl), what, detail);
      }
   }
   goto done;

fixture:
   check(0, "fixture", "could not build the local refresh lane");
done:
   stat_watch_path = NULL;
   filestream_delete(lpl);
   filestream_delete(ref);
   playlist_free(playlist);
   free(data);
   free(cfg);
   manual_content_scan_reset_settings();
}

static void local_kind_lane(const char *root, const char *pl_dir)
{
   char dir[512], path[1024], what[96], detail[128];
   manual_content_scan_task_config_t *cfg = (manual_content_scan_task_config_t*)
         calloc(1, sizeof(*cfg));
   unsigned file, source;

   if (!cfg)
      goto fixture;
   sprintf(dir, "%s/local_kind", root);
   path_mkdir(dir);
   sprintf(path, "%s/game.bin", dir);
   if (!write_content(path, 0x52A3ED00u, 1024))
      goto fixture;

   for (file = 0; file < 2; file++)
   {
      if (file)
         sprintf(path, "%s/game.bin", dir);
      else
         strlcpy(path, dir, sizeof(path));
      for (source = 0; source < 3; source++)
      {
         manual_content_scan_fs_t *fs = NULL;
         bool ok;

         manual_content_scan_reset_settings();
         manual_content_scan_set_menu_scan_method(
               MANUAL_CONTENT_SCAN_METHOD_CUSTOM);
         manual_content_scan_set_menu_scan_use_db(
               MANUAL_CONTENT_SCAN_USE_DB_NONE);
         *manual_content_scan_get_overwrite_playlist_ptr() = true;
         *manual_content_scan_get_validate_entries_ptr() = true;
         if (source == 2 && !(fs = manual_content_scan_fs_read(
                     path, NULL, NULL, NULL)))
            goto fixture;

         stat_watch_calls = 0;
         stat_watch_path  = path;
         if (source == 2)
            ok = manual_content_scan_set_menu_content_dir_fs(path, fs);
         else
            ok = manual_content_scan_set_menu_content_dir_kind(path,
                  source == 0 ? MANUAL_CONTENT_SCAN_PATH_UNKNOWN
                  : file ? MANUAL_CONTENT_SCAN_PATH_FILE
                         : MANUAL_CONTENT_SCAN_PATH_FOLDER);
         ok = ok && manual_content_scan_get_task_config(cfg, pl_dir);
         stat_watch_path = NULL;
         manual_content_scan_fs_free(fs);

         sprintf(what, "local %s: %s kind", file ? "file" : "folder",
               source == 0 ? "unknown" : source == 1 ? "supplied" : "snapshot");
         sprintf(detail, "%u stats, file %d, overwrite %d, validate %d",
               stat_watch_calls, (int)cfg->content_is_file,
               (int)cfg->overwrite_playlist, (int)cfg->validate_entries);
         check(   ok && stat_watch_calls == (source == 0 ? 2u : 0u)
               && cfg->content_is_file == (file != 0)
               && cfg->overwrite_playlist == (file == 0)
               && cfg->validate_entries
               && !strcmp(path_basename(cfg->playlist_file), "local_kind.lpl"),
               what, detail);
      }
   }
   goto done;

fixture:
   check(0, "fixture", "could not build the local kind lane");
done:
   stat_watch_path = NULL;
   manual_content_scan_reset_settings();
   free(cfg);
}

/* On a threaded queue the chain asks the file system nothing on the
 * main thread before a scan: a --scan path's resolve and stat, an
 * override core path's and DAT's, a --scan-refresh path's realpaths
 * and its content folder's stat all run on the worker.  Every item
 * here stops before its scan, so the whole run is watched. */
static void fs_main_lane(const char *root, const char *pl_dir)
{
#ifdef HAVE_THREADS
   char                pl[512], a[512], gone[512], outside[512];
   char                by_path[1024], p[1024], stale[1024], detail[256];
   task_dbscan_item_t *items;
   bool                pushed;

   sprintf(pl,      "%s/fsmain_pl",  root);
   sprintf(a,       "%s/fsmain_a",   root);
   sprintf(gone,    "%s/fsmain_gone", root);
   sprintf(outside, "%s/fsmain_out", root);
   sprintf(stale,   "%s/00_gone.bin", a);
   path_mkdir(pl);
   path_mkdir(a);
   path_mkdir(outside);
   /* Both, as the check runs the test twice in one root */
   delete_playlists(pl);
   delete_playlists(outside);
   sprintf(p, "%s/f_gone.lpl", pl);
   if (!refresh_write(p, gone, NULL, stale))
      goto fixture;
   sprintf(p, "%s/f_none.lpl", pl);
   if (!refresh_write(p, NULL, NULL, stale))
      goto fixture;
   sprintf(p, "%s/x.lpl", outside);
   if (!refresh_write(p, a, NULL, stale))
      goto fixture;
   /* By its path: the check that it is in the folder realpaths both */
   sprintf(by_path, "%s/f_gone.lpl", pl);
   strlcpy(config_get_ptr()->paths.directory_playlist, pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   use_queue(true);

   if (!(items = (task_dbscan_item_t*)calloc(6, sizeof(*items))))
      goto fixture;
   chain_item(&items[0], "fsmain_missing_dir", NULL);
   chain_item(&items[1], a, "./fsmain_nocore_libretro.so");
   chain_item(&items[2], a, NULL);
   if ((items[2].overrides = manual_content_scan_overrides_new()))
      items[2].overrides->dat = ovr_str("fsmain_missing.dat");
   refresh_item(&items[3], TASK_DBSCAN_ITEM_REFRESH, by_path);
   refresh_item(&items[4], TASK_DBSCAN_ITEM_REFRESH, p);
   refresh_item(&items[5], TASK_DBSCAN_ITEM_REFRESH, "f_none");

   fs_main_calls   = 0;
   fs_main_last[0] = '\0';
   fs_main_watch   = true;
   pushed          = chain_push(items, 6);
   chain_wait(false);
   fs_main_watch   = false;

   snprintf(detail, sizeof(detail), "%u items, failed %u, skipped %u",
         chain_seen, chain_totals.failed, chain_totals.skipped);
   check(   pushed && chain_dones == 1 && chain_seen == 6
         && chain_saw(0, "fsmain_missing_dir", TASK_DBSCAN_FAILED, -1)
         && chain_saw(1, a,        TASK_DBSCAN_FAILED,  -1)
         && chain_saw(2, a,        TASK_DBSCAN_FAILED,  -1)
         && chain_saw(3, "f_gone", TASK_DBSCAN_SKIPPED, -1)
         && chain_saw(4, p,        TASK_DBSCAN_FAILED,  -1)
         && chain_saw(5, "f_none", TASK_DBSCAN_FAILED,  -1)
         && chain_totals.failed == 5 && chain_totals.skipped == 1,
         "fs off main: each item stops where it did", detail);
   snprintf(detail, sizeof(detail), "%u calls, the last %s",
         fs_main_calls, *fs_main_last ? fs_main_last : "none");
   check(fs_main_calls == 0,
         "fs off main: no file system call on the main thread", detail);

   manual_content_scan_reset_settings();
   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
   return;

fixture:
   check(0, "fixture", "could not write the fs off main lane");
   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
#else
   (void)root;
   (void)pl_dir;
#endif
}

/* The summary the menu's Refresh All Playlists shows. */
static char menu_all_msg[128];

static void menu_all_done_cb(const task_dbscan_chain_result_t *res,
      void *user_data)
{
   if (user_data)
      chain_ud_ok = false;
   snprintf(menu_all_msg, sizeof(menu_all_msg),
         msg_hash_to_str(MSG_PLAYLIST_MANAGER_REFRESH_ALL_DONE),
         res->ran, res->skipped, res->failed);
   chain_totals = *res;
   chain_dones++;
}

/* The one item the menu pushes. */
static task_dbscan_item_t *menu_all_item(void)
{
   task_dbscan_item_t *item = (task_dbscan_item_t*)calloc(1,
         sizeof(*item));

   if (item)
      item->kind = TASK_DBSCAN_ITEM_REFRESH_ALL;
   return item;
}

/* Refresh All Playlists as the menu pushes it, over A refreshable, B
 * with its folder gone, C without settings and E with a missing core:
 * the English summary, nothing printed, and a second press while it
 * runs refused with the English message. */
static void menu_refresh_all_lane(const char *root, const char *pl_dir,
      uint32_t crc_a, uint32_t sz_a, bool threaded)
{
   char        pl[512], a[512], gone[512], out[1024], err[1024];
   char        p[1024], stale[1024], what[128];
   const char *q = threaded ? "(threaded)" : "(regular)";
   const char *busy;
   bool        captured, pushed, refused;

   sprintf(pl,    "%s/menu_all_pl",      root);
   sprintf(a,     "%s/menu_all_a",       root);
   sprintf(gone,  "%s/menu_all_gone",    root);
   sprintf(out,   "%s/menu_all_out.txt", root);
   sprintf(err,   "%s/menu_all_err.txt", root);
   sprintf(stale, "%s/00_gone.bin",      a);
   path_mkdir(pl);
   path_mkdir(a);
   delete_playlists(pl);
   sprintf(p, "%s/01_alpha.bin", a);
   if (!write_content(p, crc_a, sz_a))
      goto fixture;
   sprintf(p, "%s/a_ok.lpl", pl);
   if (!refresh_write(p, a, NULL, stale))
      goto fixture;
   sprintf(p, "%s/b_gone.lpl", pl);
   if (!refresh_write(p, gone, NULL, stale))
      goto fixture;
   sprintf(p, "%s/c_none.lpl", pl);
   if (!refresh_write(p, NULL, NULL, stale))
      goto fixture;
   sprintf(p, "%s/e_core.lpl", pl);
   if (!refresh_write(p, a, "No Such Core", stale))
      goto fixture;
   strlcpy(config_get_ptr()->paths.directory_playlist, pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   use_queue(threaded);

   chain_seen      = 0;
   chain_dones     = 0;
   chain_ud_ok     = true;
   menu_all_msg[0] = '\0';
   cli_output      = false;
   captured        = capture_begin(out, err);
   pushed          = task_push_dbscan_chain(menu_all_item(), 1, NULL,
         menu_all_done_cb, NULL);
   refused         = !task_push_dbscan_chain(menu_all_item(), 1, NULL,
         menu_all_done_cb, NULL);
   chain_wait(false);
   if (captured)
      capture_end();
   sprintf(what, "menu refresh all %s: the summary", q);
   check(   pushed && chain_dones == 1 && chain_ud_ok
         && !strcmp(menu_all_msg,
               "Playlists refreshed: 1, skipped: 1, failed: 1"),
         what, menu_all_msg);
   busy = msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_ALREADY_RUNNING);
   sprintf(what, "menu refresh all %s: a second press refused", q);
   check(   refused && chain_dones == 1
         && !strcmp(busy, "A content scan is already running."),
         what, busy);
   sprintf(what, "menu refresh all %s: nothing printed", q);
   check(   captured && path_get_size(out) == 0
         && path_get_size(err) == 0, what, err);

   manual_content_scan_reset_settings();
   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
   return;

fixture:
   check(0, "fixture", "could not write the menu refresh-all lane");
   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
}

/* A refresh-all skips a playlist whose saved folder is empty, as it
 * skips one whose folder is gone: an unplugged drive's mount point.
 * The menu's single refresh is not a chain item; it stops at the
 * empty listing and leaves the playlist as it was. */
static void empty_folder_lane(const char *root, const char *pl_dir,
      uint32_t crc_a, uint32_t sz_a, bool threaded)
{
   char                pl[512], one_pl[512], a[512], gone[512];
   char                mount[512], out[1024], err[1024], p[1024];
   char                stale[1024], want[1536], what[128], detail[160];
   const char         *q = threaded ? "(threaded)" : "(regular)";
   task_dbscan_item_t *items;
   playlist_config_t  *cfg    = NULL;
   playlist_t         *one    = NULL;
   void               *before = NULL, *after = NULL;
   int64_t             before_len, after_len;
   bool                ran, captured, pushed, same;

   sprintf(pl,     "%s/empty_pl",      root);
   sprintf(one_pl, "%s/empty_one_pl",  root);
   sprintf(a,      "%s/empty_a",       root);
   sprintf(gone,   "%s/empty_gone",    root);
   sprintf(mount,  "%s/empty_mount",   root);
   sprintf(out,    "%s/empty_out.txt", root);
   sprintf(err,    "%s/empty_err.txt", root);
   path_mkdir(pl);
   path_mkdir(one_pl);
   path_mkdir(a);
   path_mkdir(mount);
   delete_playlists(pl);
   sprintf(p, "%s/01_alpha.bin", a);
   if (!write_content(p, crc_a, sz_a))
      goto fixture;
   sprintf(p, "%s/a_ok.lpl", pl);
   sprintf(stale, "%s/00_gone.bin", a);
   if (!refresh_write(p, a, NULL, stale))
      goto fixture;
   sprintf(p, "%s/b_gone.lpl", pl);
   sprintf(stale, "%s/00_gone.bin", gone);
   if (!refresh_write(p, gone, NULL, stale))
      goto fixture;
   sprintf(p, "%s/d_mount.lpl", pl);
   sprintf(stale, "%s/00_gone.bin", mount);
   if (!refresh_write(p, mount, NULL, stale))
      goto fixture;
   strlcpy(config_get_ptr()->paths.directory_playlist, pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   use_queue(threaded);

   if (!(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
   ran = refresh_run(items, 1, out, err, true);
   sprintf(what, "empty mount %s: skipped like a gone folder", q);
   snprintf(detail, sizeof(detail), "%u items, ran %u, skipped %u, "
         "failed %u", chain_seen, chain_totals.ran, chain_totals.skipped,
         chain_totals.failed);
   check(   ran && chain_seen == 3
         && chain_saw(0, "a_ok",    TASK_DBSCAN_RAN,     1)
         && chain_saw(1, "b_gone",  TASK_DBSCAN_SKIPPED, -1)
         && chain_saw(2, "d_mount", TASK_DBSCAN_SKIPPED, -1)
         && chain_totals.ran == 1 && chain_totals.skipped == 2
         && !chain_totals.failed, what, detail);
   sprintf(what, "empty mount %s: its line", q);
   sprintf(want, "%s: skipped: content folder empty (%s)", "d_mount",
         mount);
   check(file_contains(err, want), what, err);
   sprintf(what, "empty mount %s: its entry kept", q);
   sprintf(p, "%s/d_mount.lpl", pl);
   check(file_contains(p, stale), what, p);

   chain_seen      = 0;
   chain_dones     = 0;
   chain_ud_ok     = true;
   menu_all_msg[0] = '\0';
   cli_output      = false;
   captured        = capture_begin(out, err);
   pushed          = task_push_dbscan_chain(menu_all_item(), 1, NULL,
         menu_all_done_cb, NULL);
   chain_wait(false);
   if (captured)
      capture_end();
   sprintf(what, "empty mount %s: the menu's summary", q);
   check(   pushed && chain_dones == 1 && chain_ud_ok
         && !strcmp(menu_all_msg,
               "Playlists refreshed: 1, skipped: 2, failed: 0")
         && captured && path_get_size(err) == 0, what, menu_all_msg);

   if (!threaded)
   {
      sprintf(p, "%s/Empty.lpl", one_pl);
      strlcpy(config_get_ptr()->paths.directory_playlist, one_pl,
            sizeof(config_get_ptr()->paths.directory_playlist));
      filestream_delete(p);
      if (   !refresh_write(p, mount, NULL, stale)
          || !filestream_read_file(p, &before, &before_len)
          || !(cfg = (playlist_config_t*)calloc(1, sizeof(*cfg))))
         goto fixture;
      playlist_config_set_path(cfg, p);
      cfg->capacity          = COLLECTION_SIZE;
      invalid_content_pushed = false;
      one = playlist_init(cfg);
      ran =    one
            && manual_content_scan_set_menu_from_playlist(one,
                  config_get_ptr()->paths.path_content_database, false)
                  == MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_OK
            && run_scan(mount) && scan_completed;
      same =    filestream_read_file(p, &after, &after_len)
             && after_len == before_len
             && !memcmp(after, before, (size_t)before_len);
      check(   ran && invalid_content_pushed && same
            && scan_result.failed && scan_result.added == 0
            && scan_result.errors == 0,
            "empty mount: a menu refresh fails, the playlist as it was", p);
   }
   goto end;

fixture:
   check(0, "fixture", "could not write the empty folder lane");
end:
   if (one)
      playlist_free(one);
   free(cfg);
   free(before);
   free(after);
   manual_content_scan_reset_settings();
   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
}

/* Runs the queue until no scan is left on it. */
static void group_drain(void)
{
   task_finder_data_t finder;
   time_t             started = time(NULL);

   finder.func     = find_scan;
   finder.userdata = NULL;
   while (task_queue_find(&finder))
   {
      task_queue_check();
      if (difftime(time(NULL), started) > SCAN_TIMEOUT_SECONDS)
         break;
      retro_sleep(1);
   }
}

/* Linked with --wrap: the playlist sorts. */
static retro_atomic_int_t pl_sorts;

void __real_playlist_qsort(playlist_t *playlist);
void __wrap_playlist_qsort(playlist_t *playlist)
{
   retro_atomic_fetch_add_int(&pl_sorts, 1);
   __real_playlist_qsort(playlist);
}

/* The most of these playlists written, and opened, between two window
 * opens. */
static const char *spent_lpl[2];
static unsigned    spent_had;
static unsigned    spent_most;
static unsigned    spent_opens;
static unsigned    spent_opens_most;

/* Linked with --wrap: counts the opens of spent_lpl. */
playlist_t *__real_playlist_init(const playlist_config_t *config);
playlist_t *__wrap_playlist_init(const playlist_config_t *config)
{
   if (   spent_lpl[0] && config
       && (   string_is_equal(config->path, spent_lpl[0])
           || string_is_equal(config->path, spent_lpl[1])))
      spent_opens++;
   return __real_playlist_init(config);
}

static void spent_count(void)
{
   unsigned now = (unsigned)path_is_valid(spent_lpl[0])
         + (unsigned)path_is_valid(spent_lpl[1]);

   if (now - spent_had > spent_most)
      spent_most = now - spent_had;
   spent_had = now;
   if (spent_opens > spent_opens_most)
      spent_opens_most = spent_opens;
   spent_opens = 0;
}

static void spent_open(void)
{
   scan_tick_open();
   spent_count();
   spend_window();
}

/* Loads @lpl's saved settings as a refresh does, lays strict matching
 * (refresh_write() saves none), @archives and @overwrite over them as
 * overrides would, and runs the group scan of group_names; with @out,
 * its --scan lines go to @out and @err. */
static bool group_load(const char *lpl, const char *db)
{
   playlist_config_t *cfg = (playlist_config_t*)calloc(1, sizeof(*cfg));
   playlist_t        *pl  = NULL;
   bool               set = false;

   if (!cfg)
      return false;
   cfg->capacity = COLLECTION_SIZE;
   playlist_config_set_path(cfg, lpl);
   manual_content_scan_reset_settings();
   if ((pl = playlist_init(cfg)))
   {
      set = manual_content_scan_set_menu_from_playlist(pl, db, false)
            == MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_OK;
      playlist_free(pl);
   }
   free(cfg);
   if (set)
      manual_content_scan_set_menu_scan_use_db(
            MANUAL_CONTENT_SCAN_USE_DB_STRICT);
   return set;
}

static bool group_run(const char *lpl, const char *db, const char *dir,
      bool archives, bool overwrite, const char *out, const char *err)
{
   if (!group_load(lpl, db))
      return false;
   *manual_content_scan_get_search_archives_ptr()    = archives;
   *manual_content_scan_get_overwrite_playlist_ptr() = overwrite;
   if (out)
      return capture_scan(dir, out, err, true);
   return run_scan(dir) && scan_completed;
}

/* A group scan: one scan of a folder for two playlists named after
 * their databases.  Each gets only its own match and a third database
 * is never opened; each drops its stale entry and gets the settings,
 * new entries or not; overwrite clears both. */
static void group_scan_lane(const char *root, const char *pl_dir,
      const char *db_dir, bool threaded)
{
   static const char *const names[3] =
         { "Test Alpha", "Test Beta", "Test Disc" };
   static const char *const games[3] =
         { "Group Alpha", "Group Beta", "Group Disc" };
   static const uint32_t    crcs[3]  =
         { 0x6E0A0001u, 0x6E0A0002u, 0x6E0A0003u };
   char               *db    = (char*)malloc(512);
   char               *pl    = (char*)malloc(512);
   char               *dir   = (char*)malloc(512);
   char               *label = (char*)malloc(640);
   char               *out   = (char*)malloc(1024);
   char               *err   = (char*)malloc(1024);
   char               *keep  = (char*)malloc(1024);
   char               *p     = (char*)malloc(1024);
   char              (*lpl)[1024] = (char(*)[1024])malloc(3 * sizeof(*lpl));
   char                what[128], detail[128];
   db_rec_t            rec;
   union string_list_elem_attr attr;
   struct string_list *group = string_list_new();
   const char         *q     = threaded ? "(threaded)" : "(regular)";
   unsigned            k, qq, opens;
   int                 sorts;
   bool                ran;

   if (   !db || !pl || !dir || !label || !out || !err || !keep || !p
       || !lpl)
   {
      check(0, "group lane", "out of memory");
      goto end;
   }
   sprintf(db,   "%s/group_db",       root);
   sprintf(pl,   "%s/group_pl",       root);
   sprintf(dir,  "%s/group",          root);
   sprintf(out,  "%s/group_out.txt",  root);
   sprintf(err,  "%s/group_err.txt",  root);
   sprintf(keep, "%s/group_keep.bin", root);
   path_mkdir(db);
   path_mkdir(pl);
   path_mkdir(dir);
   delete_playlists(pl);
   for (k = 0; k < 2; k++)
   {
      sprintf(p, "%s/%c.bin", dir, 'd' + k);
      filestream_delete(p);
   }
   for (k = 0; k < 3; k++)
   {
      set_rec(&rec, games[k], crcs[k], 1024);
      sprintf(p, "%s/%s.rdb", db, names[k]);
      if (!write_db_set(p, &rec, 1))
         goto fixture;
      sprintf(p, "%s/%c.bin", dir, 'a' + k);
      if (!write_content(p, crcs[k], 1024))
         goto fixture;
      sprintf(lpl[k], "%s/%s.lpl", pl, names[k]);
   }
   for (k = 0; k < 2; k++)
   {
      sprintf(p, "%s/00_gone_%c.bin", dir, 'a' + k);
      if (!refresh_write(lpl[k], dir, NULL, p))
         goto fixture;
   }
   attr.i = 0;
   if (   !write_content(keep, 0x6E0A00FFu, 1024)
       || !group
       || !string_list_append(group, names[0], attr)
       || !string_list_append(group, names[1], attr))
      goto fixture;
   sprintf(label, "%s (%s, %s)", dir, names[0], names[1]);
   use_db(db);
   strlcpy(config_get_ptr()->paths.directory_playlist, pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   use_queue(threaded);
   group_names = group;
   group_label = label;

   open_name   = names[2];
   named_opens = 0;
   ran         = group_run(lpl[0], db, dir, false, false, out, err);
   opens       = named_opens;
   open_name   = NULL;
   sprintf(what, "group %s: one scan for both", q);
   check_result(what, 2, 0, 1, 0, false);
   /* Cut to the task's 128 bytes, as a long label is. */
   strlcpy(p, msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_START), 128);
   strlcat(p, label, 128);
   sprintf(what, "group %s: its title names the group", q);
   check(ran && !strcmp(pushed_title, p), what, pushed_title);
   sprintf(what, "group %s: each playlist gets its own", q);
   check(   ran
         && file_contains(lpl[0], "/a.bin\"")
         && file_contains(lpl[0], games[0])
         && !file_contains(lpl[0], "/b.bin\"")
         && file_contains(lpl[1], "/b.bin\"")
         && file_contains(lpl[1], games[1])
         && !file_contains(lpl[1], "/a.bin\""), what, "a.bin, b.bin");
   sprintf(what, "group %s: a third database never opened", q);
   sprintf(detail, "%u opens of %s", opens, names[2]);
   check(   ran && opens == 0 && !path_is_valid(lpl[2])
         && !file_contains(lpl[0], "/c.bin\"")
         && !file_contains(lpl[1], "/c.bin\""), what, detail);
   qq = file_count(out, " ?? ");
   sprintf(p, " ?? \"%s/c.bin\"", dir);
   sprintf(what, "group %s: one ?? for the file neither has", q);
   check(ran && qq == 1 && file_contains(out, p), what, p);
   sprintf(p, "%s: 2 added, 0 duplicates, 1 unmatched, 0 errors", label);
   sprintf(what, "group %s: its totals line", q);
   check(ran && file_contains(out, p), what, p);
   sprintf(what, "group %s: each drops its stale entry", q);
   check(   ran && !file_contains(lpl[0], "00_gone_a")
         && !file_contains(lpl[1], "00_gone_b"), what,
         "00_gone_a.bin, 00_gone_b.bin");
   sprintf(what, "group %s: each saves its own database name", q);
   check(   ran
         && file_contains(lpl[0],
               "\"scan_database_name\": \"Test Alpha.lpl\"")
         && file_contains(lpl[1],
               "\"scan_database_name\": \"Test Beta.lpl\""),
         what, "Test Alpha.lpl, Test Beta.lpl");

   /* Beta's file gone: Beta gets no result, Alpha a duplicate. */
   sprintf(p, "%s/b.bin", dir);
   filestream_delete(p);
   ran = group_run(lpl[0], db, dir, true, false, NULL, NULL);
   sprintf(what, "group %s: settings saved without a new entry", q);
   sprintf(detail, "%u added, %u duplicates, %u unmatched",
         scan_result.added, scan_result.duplicates, scan_result.unmatched);
   check(   ran && scan_result.added == 0 && scan_result.duplicates == 1
         && scan_result.unmatched == 1
         && !file_contains(lpl[1], "/b.bin\"")
         && file_contains(lpl[1], "\"scan_search_archives\": true")
         && file_contains(lpl[0], "\"scan_search_archives\": true"),
         what, detail);

   /* An entry a refresh keeps, in both. */
   if (   !write_content(p, crcs[1], 1024)
       || !refresh_write(lpl[0], dir, NULL, keep)
       || !refresh_write(lpl[1], dir, NULL, keep))
      goto fixture;
   ran = group_run(lpl[0], db, dir, false, true, NULL, NULL);
   sprintf(what, "group %s: overwrite clears both", q);
   sprintf(detail, "%u added, %u duplicates, %u unmatched",
         scan_result.added, scan_result.duplicates, scan_result.unmatched);
   check(   ran && scan_result.added == 2
         && !file_contains(lpl[0], "group_keep")
         && !file_contains(lpl[1], "group_keep")
         && file_contains(lpl[0], "/a.bin\"")
         && file_contains(lpl[1], "/b.bin\"")
         && file_contains(lpl[0], "\"scan_overwrite_playlist\": true")
         && file_contains(lpl[1], "\"scan_overwrite_playlist\": true"),
         what, detail);

   /* Members interleaved in content order: a, b, (c,) d, e. */
   sprintf(p, "%s/d.bin", dir);
   if (!write_content(p, crcs[0], 1024))
      goto fixture;
   sprintf(p, "%s/e.bin", dir);
   if (!write_content(p, crcs[1], 1024))
      goto fixture;
   sorts = retro_atomic_load_acquire_int(&pl_sorts);
   ran   = group_run(lpl[0], db, dir, false, false, NULL, NULL);
   sorts = retro_atomic_load_acquire_int(&pl_sorts) - sorts;
   sprintf(what, "group %s: interleaved, each sorted once", q);
   sprintf(detail, "%d sorts; %u added, %u duplicates, %u unmatched",
         sorts, scan_result.added, scan_result.duplicates,
         scan_result.unmatched);
   check(   ran && sorts == 2 && scan_result.added == 2
         && scan_result.duplicates == 2 && scan_result.unmatched == 1,
         what, detail);
   sprintf(what, "group %s: interleaved, each gets only its own", q);
   check(   ran
         && file_contains(lpl[0], "/a.bin\"")
         && file_contains(lpl[0], "/d.bin\"")
         && !file_contains(lpl[0], "/b.bin\"")
         && !file_contains(lpl[0], "/e.bin\"")
         && file_contains(lpl[1], "/b.bin\"")
         && file_contains(lpl[1], "/e.bin\"")
         && !file_contains(lpl[1], "/a.bin\"")
         && !file_contains(lpl[1], "/d.bin\""), what, "a, d / b, e");

   /* The first member empty: the second is still validated. */
   filestream_delete(lpl[0]);
   sprintf(p, "%s/00_gone_b.bin", dir);
   if (!refresh_write(lpl[1], dir, NULL, p))
      goto fixture;
   ran = group_run(lpl[1], db, dir, false, false, NULL, NULL);
   sprintf(what, "group %s: first member empty, second validated", q);
   check(   ran && file_contains(lpl[0], "/a.bin\"")
         && !file_contains(lpl[1], "00_gone_b"), what, "00_gone_b.bin");

   /* Each window held to its floor: one member written in each. */
   if (!threaded)
   {
      sprintf(p, "%s/group_src", root);
      path_mkdir(p);
      strlcat(p, "/Test Alpha.lpl", 1024);
      filestream_delete(p);
      filestream_delete(lpl[0]);
      filestream_delete(lpl[1]);
      if (!refresh_write(p, dir, NULL, keep))
         goto fixture;
      spent_lpl[0]     = lpl[0];
      spent_lpl[1]     = lpl[1];
      spent_had        = 0;
      spent_most       = 0;
      spent_opens      = 0;
      spent_opens_most = 0;
      window_open_hook = spent_open;
      ran              = group_run(p, db, dir, false, false, NULL, NULL);
      window_open_hook = scan_tick_open;
      spent_count();
      spent_lpl[0]     = NULL;
      sprintf(what, "group %s: one member written a window", q);
      sprintf(detail, "%u written at most, %u in all", spent_most,
            spent_had);
      check(ran && spent_had == 2 && spent_most == 1, what, detail);
      sprintf(what, "group %s: one member opened a window", q);
      sprintf(detail, "%u opened at most", spent_opens_most);
      check(ran && spent_opens_most == 1, what, detail);
   }

   /* A member's own refresh and the group never run at once, either
    * started first. */
   if (!threaded)
   {
      bool grouped, lone;

      grouped = group_load(lpl[0], db)
            && task_push_manual_content_scan_group(group, label,
                  NULL, NULL);
      lone    = group_load(lpl[1], db)
            && task_push_manual_content_scan(false, NULL, NULL);
      group_drain();
      sprintf(what, "group %s: no member refresh while it runs", q);
      check(grouped && !lone, what, lone ? "both pushed" : "group");
      lone    = group_load(lpl[1], db)
            && task_push_manual_content_scan(false, NULL, NULL);
      grouped = group_load(lpl[0], db)
            && task_push_manual_content_scan_group(group, label,
                  NULL, NULL);
      group_drain();
      sprintf(what, "group %s: not while a member's refresh runs", q);
      check(lone && !grouped, what, grouped ? "both pushed" : "lone");
   }

   /* Only a strict database scan sends each match to a member. */
   ran = group_load(lpl[0], db);
   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_LOOSE);
   ran = ran && !task_push_manual_content_scan_group(group, label,
         NULL, NULL);
   group_drain();
   sprintf(what, "group %s: a loose scan refused", q);
   check(ran, what, "pushed");
   goto restore;

fixture:
   check(0, "fixture", "could not write the group scan set");
restore:
   group_names = NULL;
   group_label = NULL;
   open_name   = NULL;
   manual_content_scan_reset_settings();
   use_db(db_dir);
   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
end:
   string_list_free(group);
   free(db);
   free(pl);
   free(dir);
   free(label);
   free(out);
   free(err);
   free(keep);
   free(p);
   free(lpl);
}

/* refresh_write() with strict matching, the extensions @exts and the
 * saved database name @db_name (NULL: none for either). */
static bool refresh_group_write(const char *lpl, const char *dir,
      const char *exts, const char *db_name, const char *core,
      const char *entry)
{
   playlist_config_t *cfg = NULL;
   playlist_t        *pl  = NULL;
   bool               ok  = false;

   if (   !refresh_write(lpl, dir, core, entry)
       || !(cfg = (playlist_config_t*)calloc(1, sizeof(*cfg))))
      return false;
   playlist_config_set_path(cfg, lpl);
   cfg->capacity = COLLECTION_SIZE;
   if ((pl = playlist_init(cfg)))
   {
      playlist_set_scan_db_usage(pl, MANUAL_CONTENT_SCAN_USE_DB_STRICT);
      if (exts)
         playlist_set_scan_file_exts(pl, exts);
      if (db_name)
         playlist_set_scan_database_name(pl, db_name);
      playlist_write_file(pl);
      playlist_free(pl);
      ok = true;
   }
   free(cfg);
   return ok;
}

/* Saves @usage and the DAT @dat as @lpl's match settings. */
static bool refresh_group_match(const char *lpl, int usage, const char *dat)
{
   playlist_config_t *cfg = NULL;
   playlist_t        *pl  = NULL;
   bool               ok  = false;

   if (!(cfg = (playlist_config_t*)calloc(1, sizeof(*cfg))))
      return false;
   playlist_config_set_path(cfg, lpl);
   cfg->capacity = COLLECTION_SIZE;
   if ((pl = playlist_init(cfg)))
   {
      playlist_set_scan_db_usage(pl, usage);
      playlist_set_scan_dat_file_path(pl, dat);
      playlist_write_file(pl);
      playlist_free(pl);
      ok = true;
   }
   free(cfg);
   return ok;
}

/* Refresh All over database playlists that share a folder: Alpha and
 * Beta, equal but for a saved database name empty in one and its own
 * in the other, share one scan and one line, each validated and saved;
 * Disc with other extensions and Custom, named after no database, run
 * alone.  The keys are made with no stat on the main thread.  A
 * missing folder skips the pair with one line, an override leaves it
 * one scan and is saved to both, a cancel during it stops the rest,
 * and the menu still counts playlists.  Loose, a DAT or another
 * database's name, or another load status, runs alone; a pair whose
 * scan cannot start fails under its label. */
static void refresh_group_lane(const char *root, const char *pl_dir,
      const char *db_dir, bool threaded)
{
   static const char *const names[3] =
         { "Test Alpha", "Test Beta", "Test Disc" };
   static const char *const games[3] =
         { "Refresh Group Alpha", "Refresh Group Beta", "Refresh Group Disc" };
   static const uint32_t    crcs[3]  =
         { 0x6E0B0001u, 0x6E0B0002u, 0x6E0B0003u };
   static char         db[512], pl[512], dir[512], gone[512], slow[512];
   static char         late[512], dat[512], dot_db[512], dot_lpl[1024];
   static char         label[640], gone_label[640], slow_label[640];
   static char         dot_label[640];
   static char         out[1024], err[1024], lpl[4][1024], stale[4][1024];
   static char         p[1024], want[1536];
   char                what[128], detail[192];
   db_rec_t            rec;
   task_dbscan_item_t *items;
   const char         *q = threaded ? "(threaded)" : "(regular)";
   unsigned            k;
   bool                ran, pushed, back, off_main = false;

#ifdef HAVE_THREADS
   off_main = threaded;
#endif
   sprintf(db,   "%s/rgroup_db",      root);
   sprintf(dot_db, "%s/rgroup_db_dot", root);
   sprintf(pl,   "%s/rgroup_pl",      root);
   sprintf(dir,  "%s/rgroup",         root);
   sprintf(gone, "%s/rgroup_gone",    root);
   sprintf(slow, "%s/rgroup_slow",    root);
   sprintf(late, "%s/rgroup_late_%c", root, threaded ? 't' : 'r');
   sprintf(dat,  "%s/rgroup.dat",     root);
   sprintf(out,  "%s/rgroup_out.txt", root);
   sprintf(err,  "%s/rgroup_err.txt", root);
   path_mkdir(db);
   path_mkdir(dot_db);
   path_mkdir(pl);
   path_mkdir(dir);
   path_mkdir(slow);
   for (k = 0; k < 3; k++)
   {
      set_rec(&rec, games[k], crcs[k], 1024);
      sprintf(p, "%s/%s.rdb", db, names[k]);
      if (!write_db_set(p, &rec, 1))
         goto fixture;
      sprintf(p, "%s/%c.bin", dir, 'a' + k);
      if (!write_content(p, crcs[k], 1024))
         goto fixture;
   }
   for (k = 0; k < 2; k++)
   {
      set_rec(&rec, games[k], crcs[k], 1024);
      sprintf(p, "%s/%s.rdb", dot_db, k ? "Test Dot.Com" : names[0]);
      if (!write_db_set(p, &rec, 1))
         goto fixture;
   }
   sprintf(p, "%s/00_slow.bin", slow);
   if (!write_content(p, 0x6E0B00FFu, 1536))
      goto fixture;
   {
      FILE *f = fopen(dat, "w");

      if (!f)
         goto fixture;
      fputs("<?xml version=\"1.0\"?>\n<datafile>\n"
            "<header><name>Group</name></header>\n"
            "<game name=\"c\"><description>Disc</description></game>\n"
            "</datafile>\n", f);
      fclose(f);
   }
   sprintf(lpl[0], "%s/Test Alpha.lpl",  pl);
   sprintf(lpl[1], "%s/Test Beta.lpl",   pl);
   sprintf(lpl[2], "%s/Test Custom.lpl", pl);
   sprintf(lpl[3], "%s/Test Disc.lpl",   pl);
   for (k = 0; k < 4; k++)
      sprintf(stale[k], "%s/00_gone_%c.bin", dir, 'a' + k);
   sprintf(label,      "%s (Test Alpha, Test Beta)", dir);
   sprintf(gone_label, "%s (Test Alpha, Test Beta)", gone);
   sprintf(slow_label, "%s (Test Alpha, Test Beta)", slow);
   sprintf(dot_label,  "%s (Test Alpha, Test Dot.Com)", dir);
   sprintf(dot_lpl,    "%s/Test Dot.Com.lpl", pl);
   use_db(db);
   strlcpy(config_get_ptr()->paths.directory_playlist, pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   use_queue(threaded);

   delete_playlists(pl);
   if (   !refresh_group_write(lpl[0], dir, NULL, NULL, NULL, stale[0])
       || !refresh_group_write(lpl[1], dir, NULL, "Test Beta.lpl", NULL,
            stale[1])
       || !refresh_group_write(lpl[2], dir, NULL, NULL, NULL, stale[2])
       || !refresh_group_write(lpl[3], dir, "bin", NULL, NULL, stale[3])
       || !(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
   refresh_began_reset();
   retro_atomic_store_release_int(&stat_main, 0);
   stat_loads = 4;
   ran        = refresh_run(items, 1, out, err, true);
   stat_loads = 0;
   sprintf(what, "refresh groups %s: equal keys, one scan", q);
   snprintf(detail, sizeof(detail), "%u items: %s", chain_seen,
         chain_names[0]);
   check(   ran && chain_seen == 3
         && chain_saw(0, label, TASK_DBSCAN_RAN, 2), what, detail);
   sprintf(what, "refresh groups %s: a name no database has, alone", q);
   check(ran && chain_saw(1, "Test Custom", TASK_DBSCAN_RAN, 3), what,
         chain_names[1]);
   sprintf(what, "refresh groups %s: other extensions, another scan", q);
   check(ran && chain_saw(2, "Test Disc", TASK_DBSCAN_RAN, 1), what,
         chain_names[2]);
   sprintf(what, "refresh groups %s: the totals count playlists", q);
   sprintf(detail, "ran %u, added %u, failed %u, skipped %u",
         chain_totals.ran, chain_totals.added, chain_totals.failed,
         chain_totals.skipped);
   check(   chain_totals.ran == 4 && chain_totals.added == 4
         && !chain_totals.failed && !chain_totals.skipped, what, detail);
   sprintf(what, "refresh groups %s: both refreshed and validated", q);
   check(   ran
         && file_contains(lpl[0], "/a.bin\"")
         && !file_contains(lpl[0], "/b.bin\"")
         && !file_contains(lpl[0], "00_gone_a")
         && file_contains(lpl[1], "/b.bin\"")
         && !file_contains(lpl[1], "/a.bin\"")
         && !file_contains(lpl[1], "00_gone_b"), what, "a.bin, b.bin");
   sprintf(what, "refresh groups %s: both settings saved", q);
   check(   ran
         && file_contains(lpl[0],
               "\"scan_database_name\": \"Test Alpha.lpl\"")
         && file_contains(lpl[1],
               "\"scan_database_name\": \"Test Beta.lpl\""),
         what, lpl[1]);
   sprintf(what, "refresh groups %s: one line for the pair", q);
   sprintf(want, "%s: 2 added, 0 duplicates, 1 unmatched, 0 errors",
         label);
   check(   ran && file_contains(out, want)
         && file_contains(out,
               "Test Custom: 3 added, 0 duplicates, 0 unmatched, 0 errors")
         && file_contains(out,
               "Test Disc: 1 added, 0 duplicates, 2 unmatched, 0 errors")
         && !file_contains(out, "Test Alpha: ")
         && !file_contains(out, "Test Beta: ")
         && file_count(out, " added, ") == 3, what, want);
   /* Four to group them, then one per scan. */
   sprintf(what, "refresh groups %s: loaded to group, then once per scan",
         q);
   sprintf(detail, "%d loads",
         retro_atomic_load_acquire_int(&refresh_began[REFRESH_LOAD]));
   check(retro_atomic_load_acquire_int(&refresh_began[REFRESH_LOAD]) == 7,
         what, detail);
   sprintf(what, "refresh groups %s: keys made %s", q,
         off_main ? "with no stat on the main thread" : "while loading");
   sprintf(detail, "%d stats on the main thread",
         retro_atomic_load_acquire_int(&stat_main));
   check(   ran
         && (!off_main || !retro_atomic_load_acquire_int(&stat_main)),
         what, detail);

   /* The pair's folder gone, Custom's there. */
   delete_playlists(pl);
   if (   !refresh_group_write(lpl[0], gone, NULL, NULL, NULL, stale[0])
       || !refresh_group_write(lpl[1], gone, NULL, NULL, NULL, stale[1])
       || !refresh_group_write(lpl[2], dir, NULL, NULL, NULL, stale[2])
       || !(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
   ran = refresh_run(items, 1, out, err, true);
   sprintf(what, "refresh groups %s: a missing folder skips the pair", q);
   snprintf(detail, sizeof(detail), "%u items: %s, skipped %u, ran %u",
         chain_seen, chain_names[0], chain_totals.skipped, chain_totals.ran);
   check(   ran && chain_seen == 2
         && chain_saw(0, gone_label, TASK_DBSCAN_SKIPPED, -1)
         && chain_saw(1, "Test Custom", TASK_DBSCAN_RAN, 3)
         && chain_totals.skipped == 2 && chain_totals.ran == 1, what,
         detail);
   sprintf(what, "refresh groups %s: one skipped line", q);
   sprintf(want, "%s: skipped: content folder not found (%s)", gone_label,
         gone);
   check(   ran && file_contains(err, want)
         && file_count(err, "skipped") == 1, what, want);

   /* Each of these would join Alpha but for the one setting. */
   delete_playlists(pl);
   if (   !refresh_group_write(lpl[0], dir, NULL, NULL, NULL, stale[0])
       || !refresh_group_write(lpl[1], dir, NULL, NULL, NULL, stale[1])
       || !refresh_group_match(lpl[1], MANUAL_CONTENT_SCAN_USE_DB_LOOSE,
            NULL)
       || !refresh_group_write(lpl[3], dir, NULL, NULL, NULL, stale[3])
       || !refresh_group_match(lpl[3], MANUAL_CONTENT_SCAN_USE_DB_STRICT,
            dat)
       || !(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
   ran = refresh_run(items, 1, out, err, true);
   sprintf(what, "refresh groups %s: loose and a DAT run alone", q);
   snprintf(detail, sizeof(detail), "%u items: %s | %s | %s", chain_seen,
         chain_names[0], chain_names[1], chain_names[2]);
   check(   ran && chain_seen == 3
         && !strcmp(chain_names[0], "Test Alpha")
         && !strcmp(chain_names[1], "Test Beta")
         && !strcmp(chain_names[2], "Test Disc"), what, detail);
   delete_playlists(pl);
   if (   !refresh_group_write(lpl[0], dir, NULL, NULL, NULL, stale[0])
       || !refresh_group_write(lpl[1], dir, NULL, "Test Disc.lpl", NULL,
            stale[1])
       || !(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
   ran = refresh_run(items, 1, out, err, true);
   sprintf(what, "refresh groups %s: another database's name, alone", q);
   snprintf(detail, sizeof(detail), "%u items: %s | %s", chain_seen,
         chain_names[0], chain_names[1]);
   check(   ran && chain_seen == 2
         && !strcmp(chain_names[0], "Test Alpha")
         && !strcmp(chain_names[1], "Test Beta"), what, detail);

   /* A database name with a dot, run twice: it groups both times. */
   delete_playlists(pl);
   use_db(dot_db);
   if (   !refresh_group_write(lpl[0], dir, NULL, NULL, NULL, stale[0])
       || !refresh_group_write(dot_lpl, dir, NULL, NULL, NULL, stale[1]))
      goto fixture;
   for (k = 0; k < 2; k++)
   {
      if (!(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
         goto fixture;
      refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
      ran = refresh_run(items, 1, out, err, true);
      sprintf(what, "refresh groups %s: a dotted name groups, run %u", q,
            k + 1);
      snprintf(detail, sizeof(detail), "%u items: %s, %d added",
            chain_seen, chain_names[0], chain_added[0]);
      check(   ran && chain_seen == 1
            && chain_saw(0, dot_label, TASK_DBSCAN_RAN, k ? 0 : 2)
            && file_contains(dot_lpl,
                  "\"scan_database_name\": \"Test Dot.Com.lpl\""),
            what, detail);
   }
   use_db(db);

   /* The folder appears between the pair's loads. */
   delete_playlists(pl);
   path_rmdir(late);
   if (   !refresh_group_write(lpl[0], late, NULL, NULL, NULL, stale[0])
       || !refresh_group_write(lpl[1], late, NULL, NULL, NULL, stale[1])
       || !(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
   refresh_began_reset();
   mkdir_at_path = late;
   mkdir_at_load = 2;
   ran           = refresh_run(items, 1, out, err, true);
   mkdir_at_load = 0;
   sprintf(what, "refresh groups %s: another load status, alone", q);
   snprintf(detail, sizeof(detail), "%u items: %s, skipped %u, failed %u",
         chain_seen, chain_names[0], chain_totals.skipped,
         chain_totals.failed);
   /* Beta's folder is there but empty. */
   check(   ran && chain_seen == 2
         && chain_saw(0, "Test Alpha", TASK_DBSCAN_SKIPPED, -1)
         && chain_saw(1, "Test Beta", TASK_DBSCAN_SKIPPED, -1)
         && chain_totals.skipped == 2 && chain_totals.failed == 0, what,
         detail);

   /* The listing, two loads, then the pair's second load fails. */
   delete_playlists(pl);
   if (   !refresh_group_write(lpl[0], dir, NULL, NULL, NULL, stale[0])
       || !refresh_group_write(lpl[1], dir, NULL, NULL, NULL, stale[1])
       || !(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
   retro_atomic_store_release_int(&task_init_fail_in, 5);
   ran = refresh_run(items, 1, out, err, true);
   retro_atomic_store_release_int(&task_init_fail_in, 0);
   sprintf(what, "refresh groups %s: a pair that cannot start, its label",
         q);
   sprintf(want, "%s: the scan could not start", label);
   check(   ran && chain_seen == 1
         && chain_saw(0, label, TASK_DBSCAN_FAILED, -1)
         && chain_totals.failed == 2 && file_contains(err, want)
         && !file_contains(err, "Test Alpha: "), what, err);

   /* An override over the pair alone. */
   delete_playlists(pl);
   if (   !refresh_group_write(lpl[0], dir, NULL, NULL, NULL, stale[0])
       || !refresh_group_write(lpl[1], dir, NULL, NULL, NULL, stale[1])
       || !(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
   if ((items[0].overrides = manual_content_scan_overrides_new()))
      items[0].overrides->exts = ovr_str("bin");
   ran = refresh_run(items, 1, out, err, true);
   sprintf(what, "refresh groups %s: an override, still one scan", q);
   snprintf(detail, sizeof(detail), "%u items: %s", chain_seen,
         chain_names[0]);
   check(   ran && chain_seen == 1
         && chain_saw(0, label, TASK_DBSCAN_RAN, 2), what, detail);
   sprintf(what, "refresh groups %s: the override saved to both", q);
   check(   ran
         && file_contains(lpl[0], "\"scan_file_exts\": \"bin\"")
         && file_contains(lpl[1], "\"scan_file_exts\": \"bin\""), what,
         lpl[1]);

   /* Held on the threaded queue, so the scan cannot end before the
    * cancel; the regular one reads on this thread */
   delete_playlists(pl);
   if (   !refresh_group_write(lpl[0], slow, NULL, NULL, NULL, stale[0])
       || !refresh_group_write(lpl[1], slow, NULL, NULL, NULL, stale[1])
       || !refresh_group_write(lpl[2], dir, NULL, NULL, NULL, stale[2])
       || !(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
   slow_reset(1536, 300);
   if (threaded)
      retro_atomic_store_release_int(&slow_hold, 1);
   pushed = chain_push(items, 1);
   chain_wait(true);
   retro_atomic_store_release_int(&slow_hold, 0);
   slow_size = 0;
   back      = slow_reads_back() && readers_gone();
   sprintf(what, "refresh groups %s: a cancel in the pair stops the rest",
         q);
   sprintf(detail, "%u items, ran %u%s, done %u", chain_seen,
         chain_totals.ran, chain_totals.cancelled ? ", cancelled" : "",
         chain_dones);
   check(   pushed && back && chain_seen == 1 && chain_dones == 1
         && chain_saw(0, slow_label, TASK_DBSCAN_RAN, chain_added[0])
         && chain_totals.ran == 2 && chain_totals.cancelled
         && file_contains(lpl[2], "00_gone_c"), what, detail);

   /* As the menu pushes it: the pair, and Custom with a missing core. */
   delete_playlists(pl);
   if (   !refresh_group_write(lpl[0], dir, NULL, NULL, NULL, stale[0])
       || !refresh_group_write(lpl[1], dir, NULL, NULL, NULL, stale[1])
       || !refresh_group_write(lpl[2], dir, NULL, NULL, "No Such Core",
            stale[2]))
      goto fixture;
   chain_dones     = 0;
   chain_ud_ok     = true;
   menu_all_msg[0] = '\0';
   pushed          = task_push_dbscan_chain(menu_all_item(), 1, NULL,
         menu_all_done_cb, NULL);
   chain_wait(false);
   sprintf(what, "refresh groups %s: the menu counts playlists", q);
   check(   pushed && chain_dones == 1 && chain_ud_ok
         && !strcmp(menu_all_msg,
               "Playlists refreshed: 2, skipped: 0, failed: 1"),
         what, menu_all_msg);
   goto restore;

fixture:
   check(0, "fixture", "could not write the refresh group set");
restore:
   slow_size     = 0;
   mkdir_at_load = 0;
   path_rmdir(late);
   retro_atomic_store_release_int(&task_init_fail_in, 0);
   manual_content_scan_reset_settings();
   use_db(db_dir);
   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
}

/* As many "??" lines in @out as the totals count unmatched, @want. */
static void qq_check(const char *what, const char *q, const char *out,
      unsigned want)
{
   char     name[128], detail[64];
   unsigned qq = file_count(out, " ?? \"");

   snprintf(name, sizeof(name), "?? lines %s: %s", q, what);
   snprintf(detail, sizeof(detail), "%u ??, %u unmatched, want %u", qq,
         scan_result.unmatched, want);
   check(qq == scan_result.unmatched && qq == want, name, detail);
}

/* Every scan kind prints a "??" for each file its totals count
 * unmatched and for no other.  Rescans counts_lane()'s folders; then a
 * .lutro, which matches by extension alone, in a refresh and in a
 * group without a Lutro playlist. */
static void unmatched_lines_run(const char *root, const char *db_dir,
      bool threaded)
{
   static const char *const names[2] = { "Test Alpha", "Test Beta" };
   static char         dir[512], dat_dir[512], m3u_dir[512], res_pl[512];
   static char         dat[512], db[512], gdir[512], pl[512], label[640];
   static char         out[1024], err[1024], lpl[2][1024], p[1024];
   db_rec_t            rec;
   union string_list_elem_attr attr;
   struct string_list *group = string_list_new();
   const char         *q     = threaded ? "(threaded)" : "(regular)";
   unsigned            k;

   sprintf(dir,     "%s/scan_result",     root);
   sprintf(dat_dir, "%s/scan_result_dat", root);
   sprintf(m3u_dir, "%s/scan_result_m3u", root);
   sprintf(res_pl,  "%s/scan_result_pl",  root);
   sprintf(dat,     "%s/scan_result.dat", root);
   sprintf(out,     "%s/qq_lines.out",    root);
   sprintf(err,     "%s/qq_lines.err",    root);
   strlcpy(config_get_ptr()->paths.directory_playlist, res_pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   use_queue(threaded);

   /* One archive's member stores no CRC, so it is expanded. */
   delete_playlists(res_pl);
   if (capture_scan(dir, out, err, true))
      qq_check("strict, an archive expanded", q, out, 2);
   delete_playlists(res_pl);
   if (capture_scan(m3u_dir, out, err, true))
      qq_check("an m3u its discs match", q, out, 0);

   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_CUSTOM);
   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_LOOSE);
   delete_playlists(res_pl);
   if (capture_scan(dir, out, err, true))
      qq_check("loose, all added", q, out, 0);

   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_DAT_STRICT);
   strlcpy(manual_content_scan_get_dat_file_path_ptr(), dat,
         manual_content_scan_get_dat_file_path_size());
   delete_playlists(res_pl);
   if (capture_scan(dat_dir, out, err, true))
      qq_check("DAT", q, out, 1);

   manual_content_scan_get_dat_file_path_ptr()[0] = '\0';
   manual_content_scan_set_menu_scan_use_db(
         MANUAL_CONTENT_SCAN_USE_DB_NONE);
   manual_content_scan_set_menu_system_name(
         MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR, NULL);
   delete_playlists(res_pl);
   if (capture_scan(dir, out, err, true))
      qq_check("match none", q, out, 0);
   manual_content_scan_reset_settings();
   delete_playlists(res_pl);

   sprintf(db,   "%s/qq_db",    root);
   sprintf(gdir, "%s/qq_group", root);
   sprintf(pl,   "%s/qq_pl",    root);
   path_mkdir(db);
   path_mkdir(gdir);
   path_mkdir(pl);
   delete_playlists(pl);
   for (k = 0; k < 2; k++)
   {
      set_rec(&rec, names[k], 0x6E0C0001u + k, 1024);
      sprintf(p, "%s/%s.rdb", db, names[k]);
      if (!write_db_set(p, &rec, 1))
         goto fixture;
      sprintf(lpl[k], "%s/%s.lpl", pl, names[k]);
      sprintf(p, "%s/00_gone.bin", gdir);
      if (!refresh_write(lpl[k], gdir, NULL, p))
         goto fixture;
   }
   sprintf(p, "%s/a.bin", gdir);
   if (!write_content(p, 0x6E0C0001u, 1024))
      goto fixture;
   sprintf(p, "%s/x.lutro", gdir);
   if (!write_content(p, 0x6E0C00FFu, 1024))
      goto fixture;
   attr.i = 0;
   if (   !group
       || !string_list_append(group, names[0], attr)
       || !string_list_append(group, names[1], attr))
      goto fixture;
   sprintf(label, "%s (%s, %s)", gdir, names[0], names[1]);
   use_db(db);
   strlcpy(config_get_ptr()->paths.directory_playlist, pl,
         sizeof(config_get_ptr()->paths.directory_playlist));

   if (group_run(lpl[0], db, gdir, false, false, out, err))
      qq_check("a refresh, a .lutro", q, out, 1);
   group_names = group;
   group_label = label;
   if (group_run(lpl[0], db, gdir, false, false, out, err))
      qq_check("a group, a .lutro", q, out, 1);
   goto restore;

fixture:
   check(0, "fixture", "could not write the ?? lines set");
restore:
   group_names = NULL;
   group_label = NULL;
   string_list_free(group);
   manual_content_scan_reset_settings();
   use_db(db_dir);
}

static void unmatched_lines_lane(const char *root, const char *pl_dir,
      const char *db_dir)
{
   unmatched_lines_run(root, db_dir, false);
   unmatched_lines_run(root, db_dir, true);
   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
}

#ifdef HAVE_SMBCLIENT
/* The number of times @s is in @log. */
static unsigned smb_log_count(const char *log, const char *s)
{
   unsigned n = 0;

   while ((log = strstr(log, s)))
   {
      n++;
      log += strlen(s);
   }
   return n;
}

/* A refresh-all whose first smb:// folder fails stats the share root:
 * when that fails too, later smb:// folders, --scan paths and refreshes
 * in the run fail without a stat, and local playlists still run.  When
 * the root answers, the run goes on as before.  With no share set, the
 * URL names it, so a dead one leaves the others to run and each is
 * checked once, until a root answers; with one set, all are down. */
static void smb_down_lane(const char *root, const char *pl_dir,
      uint32_t crc_a, uint32_t sz_a, bool threaded)
{
   static const char *smb_names[] = { "b_one", "d_two", "e_three" };
   static const char *smb_dirs[]  = { "smb://host/share/one",
      "smb://host/share/two", "smb://host/share/three" };
   static const char *share_names[] = { "a_games", "a_old", "b_dead" };
   static const char *share_dirs[]  = { "smb://host/games/nes",
      "smb://host/old/early", "smb://host/old/md" };
   static const char *const up_root[] = { "smb://host/share/", NULL };
   static const char *const shares_up[] = { "smb://host/games/",
      "smb://host/games/nes", "smb://host/old/early", NULL };
   static const char *mix_dirs[] = { "smb://host/aa/1", "smb://host/bb/2",
      "smb://host/aa/3", "smb://host/bb/4", "smb://host/aa/5",
      "smb://host/bb/6" };
   static const char *const bb_root[] = { "smb://host/bb/",
      "smb://host/aa/3", NULL };
   static char         pl[512], live_pl[512], two_pl[512], mix_pl[512];
   static char         a[512], log[512];
   static char         out[1024], err[1024], p[1024], stale[1024];
   static char         want[1536], detail[640];
   char                what[128];
   const char         *q = threaded ? "(threaded)" : "(regular)";
   task_dbscan_item_t *items;
   unsigned            i;
   bool                ran;

   sprintf(pl,      "%s/smb_pl",      root);
   sprintf(live_pl, "%s/smb_live_pl", root);
   sprintf(two_pl,  "%s/smb_two_pl",  root);
   sprintf(mix_pl,  "%s/smb_mix_pl",  root);
   sprintf(a,       "%s/smb_a",       root);
   sprintf(out,     "%s/smb_out.txt", root);
   sprintf(err,     "%s/smb_err.txt", root);
   sprintf(stale,   "%s/00_gone.bin", a);
   path_mkdir(pl);
   path_mkdir(live_pl);
   path_mkdir(two_pl);
   path_mkdir(mix_pl);
   path_mkdir(a);
   delete_playlists(pl);
   delete_playlists(live_pl);
   delete_playlists(two_pl);
   delete_playlists(mix_pl);
   sprintf(p, "%s/01_alpha.bin", a);
   if (!write_content(p, crc_a, sz_a))
      goto fixture;
   sprintf(p, "%s/a_local.lpl", pl);
   if (!refresh_write(p, a, NULL, stale))
      goto fixture;
   sprintf(p, "%s/c_local.lpl", pl);
   if (!refresh_write(p, a, NULL, stale))
      goto fixture;
   for (i = 0; i < 3; i++)
   {
      sprintf(p, "%s/%s.lpl", pl, smb_names[i]);
      if (!refresh_write(p, smb_dirs[i], NULL, stale))
         goto fixture;
      sprintf(p, "%s/%s.lpl", live_pl, smb_names[i]);
      if (i < 2 && !refresh_write(p, smb_dirs[i], NULL, stale))
         goto fixture;
      sprintf(p, "%s/%s.lpl", two_pl, share_names[i]);
      if (!refresh_write(p, share_dirs[i], NULL, stale))
         goto fixture;
   }
   for (i = 0; i < 6; i++)
   {
      sprintf(p, "%s/m_%u.lpl", mix_pl, i + 1);
      if (!refresh_write(p, mix_dirs[i], NULL, stale))
         goto fixture;
   }
   use_queue(threaded);
   smb_stat_log = log;

   /* A dead server: a refresh-all, then a --scan and a refresh on it. */
   if (!(items = (task_dbscan_item_t*)calloc(3, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
   chain_item(&items[1], "smb://host/share/x", NULL);
   refresh_item(&items[2], TASK_DBSCAN_ITEM_REFRESH, "b_one");
   strlcpy(config_get_ptr()->paths.directory_playlist, pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   *log = '\0';
   retro_atomic_store_release_int(&smb_stat_count, 0);
   ran = refresh_run(items, 3, out, err, true);
   sprintf(what, "smb down %s: a dead server, two stats", q);
   snprintf(detail, sizeof(detail), "%d stats: %s",
         retro_atomic_load_acquire_int(&smb_stat_count), log);
   check(   ran && retro_atomic_load_acquire_int(&smb_stat_count) == 2
         && string_is_equal(log, "smb://host/share/one smb://host/share/ "),
         what, detail);
   sprintf(what, "smb down %s: the items", q);
   snprintf(detail, sizeof(detail),
         "%u items: ran %u, added %u, failed %u, skipped %u", chain_seen,
         chain_totals.ran, chain_totals.added, chain_totals.failed,
         chain_totals.skipped);
   check(   chain_seen == 7
         && chain_saw(0, "a_local", TASK_DBSCAN_RAN,     1)
         && chain_saw(1, "b_one",   TASK_DBSCAN_SKIPPED, -1)
         && chain_saw(2, "c_local", TASK_DBSCAN_RAN,     1)
         && chain_saw(3, "d_two",   TASK_DBSCAN_SKIPPED, -1)
         && chain_saw(4, "e_three", TASK_DBSCAN_SKIPPED, -1)
         && chain_saw(5, "smb://host/share/x", TASK_DBSCAN_FAILED, -1)
         && chain_saw(6, "b_one",   TASK_DBSCAN_SKIPPED, -1)
         && chain_totals.ran == 2 && chain_totals.added == 2
         && chain_totals.failed == 1 && chain_totals.skipped == 4,
         what, detail);
   sprintf(what, "smb down %s: the lines", q);
   check(   file_contains(err,
               "b_one: skipped: content folder not found "
               "(smb://host/share/one)")
         && file_contains(err,
               "d_two: skipped: content folder not found "
               "(smb://host/share/two)")
         && file_contains(err,
               "e_three: skipped: content folder not found "
               "(smb://host/share/three)")
         && file_contains(err, "smb://host/share/x: not found")
         && file_contains(out, "a_local: 1 added")
         && file_contains(out, "c_local: 1 added"), what, err);
   sprintf(what, "smb down %s: local playlists refreshed", q);
   sprintf(want, "%s/01_alpha.bin", a);
   sprintf(p, "%s/a_local.lpl", pl);
   ran = file_contains(p, want) && !file_contains(p, stale);
   sprintf(p, "%s/c_local.lpl", pl);
   check(ran && file_contains(p, want) && !file_contains(p, stale),
         what, p);

   /* The root answers: the folders are missing, the server is not. */
   if (!(items = (task_dbscan_item_t*)calloc(2, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
   chain_item(&items[1], "smb://host/share/x", NULL);
   strlcpy(config_get_ptr()->paths.directory_playlist, live_pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   *log           = '\0';
   smb_stat_valid = up_root;
   ran            = refresh_run(items, 2, out, err, true);
   sprintf(what, "smb up %s: the root stat once, the rest as before", q);
   snprintf(detail, sizeof(detail), "%u items: %s", chain_seen, log);
   check(   ran && chain_seen == 3
         && chain_saw(0, "b_one", TASK_DBSCAN_SKIPPED, -1)
         && chain_saw(1, "d_two", TASK_DBSCAN_SKIPPED, -1)
         && chain_saw(2, "smb://host/share/x", TASK_DBSCAN_FAILED, -1)
         && string_is_equal(log,
               "smb://host/share/one smb://host/share/ "
               "smb://host/share/two smb://host/share/x "), what, detail);

   /* Two shares, no share set: "old" dies after a_games and a_old
    * loaded. */
   if (!(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
   strlcpy(config_get_ptr()->paths.directory_playlist, two_pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   *log           = '\0';
   smb_stat_valid = shares_up;
   ran            = refresh_run(items, 1, out, err, true);
   sprintf(what, "smb shares %s: a dead share, the other runs", q);
   snprintf(detail, sizeof(detail), "%u items: %s %d | %s %d | %s %d: %s",
         chain_seen, chain_names[0], chain_outcomes[0], chain_names[1],
         chain_outcomes[1], chain_names[2], chain_outcomes[2], log);
   /* Its scan starts, then fails: the stub lists no folder. */
   check(   ran && chain_seen == 3
         && chain_saw(0, "a_games", TASK_DBSCAN_FAILED, 0)
         && smb_log_count(log, "smb://host/games/nes ") > 1
         && !file_contains(err, "a_games: skipped"), what, detail);
   sprintf(what, "smb shares %s: loaded before, skipped at its run", q);
   check(   ran
         && chain_saw(1, "a_old",  TASK_DBSCAN_SKIPPED, -1)
         && chain_saw(2, "b_dead", TASK_DBSCAN_SKIPPED, -1)
         && smb_log_count(log, "smb://host/old/early ") == 1
         && smb_log_count(log, "smb://host/old/ ") == 1
         && file_contains(err,
               "a_old: skipped: content folder not found "
               "(smb://host/old/early)"), what, detail);

   /* The same with a share set: it is every path's. */
   if (!(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
   strlcpy(config_get_ptr()->arrays.smb_client_share, "games",
         sizeof(config_get_ptr()->arrays.smb_client_share));
   *log = '\0';
   ran  = refresh_run(items, 1, out, err, true);
   config_get_ptr()->arrays.smb_client_share[0] = '\0';
   sprintf(what, "smb shares %s: a share set, all down", q);
   snprintf(detail, sizeof(detail), "%u items: %s", chain_seen, log);
   check(   ran && chain_seen == 3
         && chain_saw(0, "a_games", TASK_DBSCAN_SKIPPED, -1)
         && chain_saw(1, "a_old",   TASK_DBSCAN_SKIPPED, -1)
         && chain_saw(2, "b_dead",  TASK_DBSCAN_SKIPPED, -1)
         && smb_log_count(log, "smb://host/games/nes ") == 1, what,
         detail);

   /* Two dead shares in turn, no share set: two stats each. */
   if (!(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
   strlcpy(config_get_ptr()->paths.directory_playlist, mix_pl,
         sizeof(config_get_ptr()->paths.directory_playlist));
   *log           = '\0';
   smb_stat_valid = NULL;
   ran            = refresh_run(items, 1, out, err, true);
   sprintf(what, "smb shares %s: two dead, two stats each", q);
   snprintf(detail, sizeof(detail), "%u items, skipped %u: %s",
         chain_seen, chain_totals.skipped, log);
   check(   ran && chain_seen == 6 && chain_totals.skipped == 6
         && string_is_equal(log, "smb://host/aa/1 smb://host/aa/ "
               "smb://host/bb/2 smb://host/bb/ "), what, detail);

   /* bb's root answers after aa's failed, as a waking NAS does: aa is
    * checked again, and aa/3 now answers. */
   if (!(items = (task_dbscan_item_t*)calloc(1, sizeof(*items))))
      goto fixture;
   refresh_item(&items[0], TASK_DBSCAN_ITEM_REFRESH_ALL, NULL);
   *log           = '\0';
   smb_stat_valid = bb_root;
   ran            = refresh_run(items, 1, out, err, true);
   sprintf(what, "smb shares %s: one answers, the dead one is checked again",
         q);
   snprintf(detail, sizeof(detail), "%u items: %s", chain_seen, log);
   check(   ran && chain_seen == 6
         && chain_saw(2, "m_3", TASK_DBSCAN_FAILED,  0)
         && chain_saw(4, "m_5", TASK_DBSCAN_SKIPPED, -1)
         && smb_log_count(log, "smb://host/aa/ ") == 1
         && smb_log_count(log, "smb://host/bb/ ") == 1
         && smb_log_count(log, "smb://host/aa/3 ") > 1
         && smb_log_count(log, "smb://host/aa/5 ") == 1
         && smb_log_count(log, "smb://host/bb/4 ") == 1
         && smb_log_count(log, "smb://host/bb/6 ") == 1, what, detail);
   goto restore;

fixture:
   check(0, "fixture", "could not write the smb lane");
restore:
   smb_stat_log   = NULL;
   smb_stat_valid = NULL;
   config_get_ptr()->arrays.smb_client_share[0] = '\0';
   manual_content_scan_reset_settings();
   strlcpy(config_get_ptr()->paths.directory_playlist, pl_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));
}
#endif

int main(int argc, char **argv)
{
   const char *root = (argc > 1) ? argv[1] : "/tmp";
#ifndef _WIN32
   static char root_real[512];
#endif
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
#ifndef _WIN32
   /* Playlists save paths with their symlinks resolved */
   path_mkdir(root);
   strlcpy(root_real, root, sizeof(root_real));
   if (path_resolve_realpath(root_real, sizeof(root_real), true))
      root = root_real;
#endif

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
            "supported_extensions = \"bin|cue|gdi|m3u|zip\"\n"
            "database = \"Test Alpha|Test Beta|Test Disc|Test Zip|"
            "Test Dot.Com|Tiger - Game.com\"\n");
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
   sprintf(p, "%s/gcn_libretro.info", info_dir);
   {
      FILE *f = fopen(p, "w");
      if (!f)
      { check(0, "fixture", "could not write core info"); return 1; }
      fprintf(f,
            "display_name = \"GameCube Test\"\n"
            "corename = \"GcnTest\"\n"
            "supported_extensions = \"ciso\"\n"
            "database = \"Test GameCube|Test Wii\"\n");
      fclose(f);
   }
   sprintf(p, "%s/gcn_libretro.so", core_dir);
   { FILE *f = fopen(p, "wb"); if (f) { fputs("\177ELF", f); fclose(f); } }
   sprintf(p, "%s/xbox_libretro.info", info_dir);
   {
      FILE *f = fopen(p, "w");
      if (!f)
      { check(0, "fixture", "could not write core info"); return 1; }
      fprintf(f,
            "display_name = \"Xbox Test\"\n"
            "corename = \"XboxTest\"\n"
            "supported_extensions = \"iso|xiso\"\n"
            "database = \"Test Xbox\"\n");
      fclose(f);
   }
   sprintf(p, "%s/xbox_libretro.so", core_dir);
   { FILE *f = fopen(p, "wb"); if (f) { fputs("\177ELF", f); fclose(f); } }
   sprintf(p, "%s/x360_libretro.info", info_dir);
   {
      FILE *f = fopen(p, "w");
      if (!f)
      { check(0, "fixture", "could not write core info"); return 1; }
      fprintf(f,
            "display_name = \"Xbox 360 Test\"\n"
            "corename = \"X360Test\"\n"
            "supported_extensions = \"iso|xex|xcp|zar\"\n"
            "database = \"Test Xbox 360\"\n");
      fclose(f);
   }
   sprintf(p, "%s/x360_libretro.so", core_dir);
   { FILE *f = fopen(p, "wb"); if (f) { fputs("\177ELF", f); fclose(f); } }
   sprintf(p, "%s/wiiu_libretro.info", info_dir);
   {
      FILE *f = fopen(p, "w");
      if (!f)
      { check(0, "fixture", "could not write core info"); return 1; }
      fprintf(f,
            "display_name = \"Wii U Test\"\n"
            "corename = \"WiiUTest\"\n"
            "supported_extensions = \"wud|wux|wua|iso\"\n"
            "database = \"Nintendo - Wii U\"\n");
      fclose(f);
   }
   sprintf(p, "%s/wiiu_libretro.so", core_dir);
   { FILE *f = fopen(p, "wb"); if (f) { fputs("\177ELF", f); fclose(f); } }
   sprintf(p, "%s/atari_libretro.info", info_dir);
   {
      FILE *f = fopen(p, "w");
      if (!f)
      { check(0, "fixture", "could not write core info"); return 1; }
      fprintf(f,
            "display_name = \"Atari Test\"\n"
            "corename = \"AtariTest\"\n"
            "supported_extensions = \"xex\"\n"
            "database = \"Test Atari\"\n");
      fclose(f);
   }
   sprintf(p, "%s/atari_libretro.so", core_dir);
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
            "Test MF Cart|Test MF Arcade|Test FM One|Test FM Two|Test FP Arcade|"
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
   sprintf(p, "%s/sevenzip_libretro.info", info_dir);
   {
      FILE *f = fopen(p, "w");
      if (!f)
      { check(0, "fixture", "could not write core info"); return 1; }
      fprintf(f,
            "display_name = \"7z Cart Test\"\n"
            "corename = \"SevenZipCartTest\"\n"
            "supported_extensions = \"rd|7z\"\n"
            "database = \"Test 7Z Cart\"\n");
      fclose(f);
   }
   sprintf(p, "%s/sevenzip_libretro.so", core_dir);
   { FILE *f = fopen(p, "wb"); if (f) { fputs("\177ELF", f); fclose(f); } }
   /* Lutro content matches by its extension, with no database. */
   sprintf(p, "%s/lutro_libretro.info", info_dir);
   {
      FILE *f = fopen(p, "w");
      if (!f)
      { check(0, "fixture", "could not write core info"); return 1; }
      fprintf(f,
            "display_name = \"Lutro Test\"\n"
            "corename = \"LutroTest\"\n"
            "supported_extensions = \"lutro\"\n"
            "database = \"Lutro\"\n");
      fclose(f);
   }
   sprintf(p, "%s/lutro_libretro.so", core_dir);
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
   if (!write_lookup_cores(info_dir, core_dir))
   { check(0, "fixture", "could not write core info"); return 1; }

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
      check_result("and fails the scan", 0, 0, 0, 0, true);
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
   ciso_lane(root, pl_dir, db_dir);
   xbox_lane(root, pl_dir, db_dir);
   x360_lane(root, pl_dir, db_dir);
   wiiu_lane(root, pl_dir, db_dir);
   single_lane(root, pl_dir, crc_a, sz_a);
   dotted_names_lane(root, pl_dir, db_dir, crc_a, sz_a);
   counts_lane(root, pl_dir, crc_a, sz_a, crc_d, sz_d);
   failed_lane(root, pl_dir, db_dir, crc_a, sz_a);
   errors_lane(root, pl_dir, crc_a, sz_a);
   output_lane(root, pl_dir, db_dir);
   cli_lines_lane(root, pl_dir);
   reset_lane(root, pl_dir);
   overrides_lane(root, pl_dir);
   anchor_lane(root, pl_dir);
   core_lookup_lane(root);
   chain_lane(root, pl_dir, crc_a, sz_a, false);
   chain_lane(root, pl_dir, crc_a, sz_a, true);
   refresh_lane(root, pl_dir, crc_a, sz_a, false);
   refresh_lane(root, pl_dir, crc_a, sz_a, true);
   local_kind_lane(root, pl_dir);
   local_refresh_lane(root, pl_dir, false);
   local_refresh_lane(root, pl_dir, true);
   fs_main_lane(root, pl_dir);
   menu_refresh_all_lane(root, pl_dir, crc_a, sz_a, false);
   menu_refresh_all_lane(root, pl_dir, crc_a, sz_a, true);
#ifdef HAVE_SMBCLIENT
   smb_reason_lane(root, false);
   smb_reason_lane(root, true);
   smb_refresh_lane(root, crc_a, sz_a, false);
   smb_refresh_lane(root, crc_a, sz_a, true);
   smb_kind_lane(root, false);
   smb_kind_lane(root, true);
#endif
   empty_folder_lane(root, pl_dir, crc_a, sz_a, false);
   empty_folder_lane(root, pl_dir, crc_a, sz_a, true);
   group_scan_lane(root, pl_dir, db_dir, false);
   group_scan_lane(root, pl_dir, db_dir, true);
   refresh_group_lane(root, pl_dir, db_dir, false);
   refresh_group_lane(root, pl_dir, db_dir, true);
   unmatched_lines_lane(root, pl_dir, db_dir);
#ifdef HAVE_SMBCLIENT
   smb_down_lane(root, pl_dir, crc_a, sz_a, false);
   smb_down_lane(root, pl_dir, crc_a, sz_a, true);
#endif
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
      refresh_db_name_lane(root);
      omit_ref_lane(root, pl_dir, true);
      omit_ref_lane(root, pl_dir, false);
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
#ifdef HAVE_7ZIP
      sevenzip_lane(root, pl_dir, true);
      sevenzip_lane(root, pl_dir, false);
#endif
      set_fp_lane(root, pl_dir, true);
      set_fp_lane(root, pl_dir, false);
      fp_size_lane(root, pl_dir, true);
      fp_size_lane(root, pl_dir, false);
      fp_one_lane(root, pl_dir, true);
      fp_one_lane(root, pl_dir, false);
      fp_cart_first_lane(root, pl_dir, true);
      fp_cart_first_lane(root, pl_dir, false);
      fp_member_db_lane(root, pl_dir, true);
      fp_member_db_lane(root, pl_dir, false);
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
      ahead_fp_streak_lane(root, pl_dir);
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

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
#include <retro_atomic.h>
#include <features/features_cpu.h>
#include <streams/interface_stream.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include "../../../core_info.h"
#include "../../../tasks/tasks_internal.h"
#include "../../../manual_content_scan.h"
#include "../../../list_special.h"
#include "../../../configuration.h"
#include "../../../verbosity.h"
#include "../../../libretro-db/libretrodb.h"
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
 * database once and walks it once, so this counts the walks. */
static unsigned rdb_opens = 0;

int __real_libretrodb_open(const char *path, libretrodb_t *db, bool write);
int __wrap_libretrodb_open(const char *path, libretrodb_t *db, bool write)
{
   rdb_opens++;
   return __real_libretrodb_open(path, db, write);
}

/* The scanner's default, for what this build should do. */
#ifndef SCAN_READERS_MAX
#define SCAN_READERS_MAX 4
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
   retro_atomic_store_release_size(&scan_tick_tid, 0);
   __real_task_nbio_slice_close(b);
}

/* A stream of slow_size bytes sleeps slow_ms before its first quantum;
 * slow_in counts those sleeps begun, slow_back those ended, and
 * slow_on_scan those on the thread that last ran the scan: a reader
 * there holds every other task of the queue, the scan too.  Reads of
 * streams of watch_size bytes are counted by who makes them, a
 * reader's on the main thread apart, and those of begun_size bytes as
 * each begins; so are reads begun once cb_fired is set.  A reader's
 * read of a stream of fail_size bytes fails, counted in reader_fails.
 * All set only between scans. */
static uint32_t           slow_size;
static unsigned           slow_ms;
static retro_atomic_int_t slow_in;
static retro_atomic_int_t slow_back;
static retro_atomic_int_t slow_on_scan;
static uint32_t           watch_size;
static retro_atomic_int_t watch_scan;
static retro_atomic_int_t watch_reader;
static retro_atomic_int_t watch_inline;
static uint32_t           begun_size;
static retro_atomic_int_t reads_begun;
static retro_atomic_int_t cb_fired;
static retro_atomic_int_t reads_after_cb;
static uint32_t           fail_size;
static retro_atomic_int_t reader_fails;
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

   if (first && retro_atomic_load_acquire_int(&cb_fired))
      retro_atomic_fetch_add_int(&reads_after_cb, 1);
   if (slow_size && size == (int64_t)slow_size && first)
   {
      if (retro_atomic_load_acquire_size(&scan_thread_tid)
            == (size_t)current_tid())
         retro_atomic_fetch_add_int(&slow_on_scan, 1);
      retro_atomic_fetch_add_int(&slow_in, 1);
      retro_sleep(slow_ms);
      retro_atomic_fetch_add_int(&slow_back, 1);
   }
   if (begun_size && size == (int64_t)begun_size && first)
      retro_atomic_fetch_add_int(&reads_begun, 1);
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

/* The last time the scanner asked rm3u_is_m3u() about each order file.
 * Its START step asks for every file, in content-list order, after the
 * directory walk. */
static unsigned order_stamp;
static unsigned order_last[2];

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
} db_rec_t;

/* One database holding @n records; a NULL serial leaves the key out. */
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
      bfixmap(&body, recs[r].serial ? 4 : 3);
      bfixstr(&body, "name"); bfixstr(&body, recs[r].name);
      if (recs[r].serial)
      {
         bfixstr(&body, "serial");
         bbin(&body, recs[r].serial);
      }
      bfixstr(&body, "crc");  bbin4(&body, recs[r].crc);
      bfixstr(&body, "size"); buint32(&body, recs[r].size);
   }
   bbyte(&body, 0xc0);

   bfixmap(&meta, 1);
   bfixstr(&meta, "count");
   bbyte(&meta, (uint8_t)n);

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

/* A one-entry zip whose member @data (@clen octets) is stored with
 * @method and stands for @len octets with crc32 @crc.  Written by hand
 * because the point is to exercise the scanner's archive handling, not
 * to depend on a fixture checked into the tree. */
static int write_zip_method(const char *path, const char *member,
      unsigned method, const uint8_t *data, uint32_t clen, uint32_t len,
      uint32_t crc)
{
   FILE    *f = fopen(path, "wb");
   uint16_t nlen = (uint16_t)strlen(member);
   uint8_t  h[46];
   uint32_t local_off = 0;

   if (!f)
      return 0;

#define PUT16(p, v) do { (p)[0] = (uint8_t)(v); (p)[1] = (uint8_t)((v) >> 8); } while (0)
#define PUT32(p, v) do { (p)[0] = (uint8_t)(v);        (p)[1] = (uint8_t)((v) >> 8);                          (p)[2] = (uint8_t)((v) >> 16); (p)[3] = (uint8_t)((v) >> 24); } while (0)

   /* local file header */
   memset(h, 0, 30);
   PUT32(h,      0x04034b50); PUT16(h + 4,  20); PUT16(h + 6, 0);
   PUT16(h + 8,  method);
   PUT16(h + 10, 0); PUT16(h + 12, 0);
   PUT32(h + 14, crc); PUT32(h + 18, clen); PUT32(h + 22, len);
   PUT16(h + 26, nlen); PUT16(h + 28, 0);
   fwrite(h, 1, 30, f);
   fwrite(member, 1, nlen, f);
   fwrite(data, 1, clen, f);

   /* central directory */
   local_off = 0;
   memset(h, 0, 46);
   PUT32(h,      0x02014b50); PUT16(h + 4, 20); PUT16(h + 6, 20);
   PUT16(h + 8,  0); PUT16(h + 10, method);
   PUT16(h + 12, 0); PUT16(h + 14, 0);
   PUT32(h + 16, crc); PUT32(h + 20, clen); PUT32(h + 24, len);
   PUT16(h + 28, nlen); PUT16(h + 30, 0); PUT16(h + 32, 0);
   PUT16(h + 34, 0); PUT16(h + 36, 0); PUT32(h + 38, 0);
   PUT32(h + 42, local_off);
   fwrite(h, 1, 46, f);
   fwrite(member, 1, nlen, f);

   /* end of central directory */
   memset(h, 0, 22);
   PUT32(h,      0x06054b50);
   PUT16(h + 8,  1); PUT16(h + 10, 1);
   PUT32(h + 12, 46 + nlen);
   PUT32(h + 16, 30 + nlen + clen);
   fwrite(h, 1, 22, f);
   fclose(f);
#undef PUT16
#undef PUT32
   return 1;
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
}

#define MIX_BIG_ONE 70000
#define MIX_BIG_TWO 131072

/* Two files over the cutoff, an archive matched by its own CRC, one
 * expanded into its member, and a cue that prunes its track. */
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
 * first member. */
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
   return;

fixture:
   free(c);
   check(0, "fixture", "could not write the archives");
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
   /* Inside an archive, so the scanner has to expand it and build a
    * "archive#member" path for the entry. */
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
    * match can only come from expanding it. */
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
            "database = \"Test Reader|Test Order One|Test Order Two|Test Mix|Test Switch\"\n");
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

   /* Without this the scanner never opens an archive, and the entry
    * inside is invisible. */
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
   /* The member is what gets checksummed and matched, but the entry
    * records the archive: that is the path a frontend loads.  Assert
    * it rather than the "archive#member" form the scan builds
    * internally, which does not reach the playlist. */
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
      cue_lane(root, reader_db, false);
      cue_lane(root, reader_db, true);
      strlcpy(config_get_ptr()->paths.path_content_database, reader_db,
            sizeof(config_get_ptr()->paths.path_content_database));
      pads_lane(root, true);
      pads_lane(root, false);
      switch_lane(root, pl_dir);
      switch_stuck_lane(root, pl_dir, reader_db);
      reader_list_end_lane(root, reader_db);
      strlcpy(config_get_ptr()->paths.path_content_database, db_dir,
            sizeof(config_get_ptr()->paths.path_content_database));
      use_queue(false);
   }

done:
   printf("\n%d checks, %d failures\n", checks, failures);
   return failures ? 1 : 0;
}

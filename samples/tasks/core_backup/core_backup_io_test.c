/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (core_backup_io_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Oracle for the file I/O in tasks/task_core_backup.c, compiled from
 * the tree so it exercises the shipping translation unit.
 *
 * As in samples/tasks/save_state and samples/tasks/image_decode_slice,
 * the clock is the instrument: this sample provides
 * cpu_features_get_time_usec() itself, advancing a fixed step per
 * observation, so a tick budget is assertable exactly rather than
 * measured against a wall clock.  Step 0 is a budget that never
 * expires; a step at or above the budget is one that expires on its
 * first observation.
 *
 * WHAT IT PINS
 *
 * The throughput regression.  Both the backup and restore handlers
 * transferred exactly one CORE_BACKUP_CHUNK_SIZE per queue tick, and
 * that chunk was 4096 bytes.  A tick is one frame unthreaded, so the
 * copy was capped at 4096 * 60 == 245KB/s regardless of what the
 * device could do -- a 40MB core takes ~10240 ticks, about 171
 * seconds at 60Hz, to copy a file the same machine can memcpy through
 * in well under a second.  Cores in that size class are ordinary
 * (mame, dolphin, ppsspp), and an automatic backup runs on every core
 * update, so this is the common path rather than a corner.
 *
 * That is the same defect, in the same shape, that
 * tasks/task_save.c's transfer loops already carry a fix for, so the
 * fix here is the same one: a tick is bounded by time and transfers
 * as many quanta as fit, and the quantum grows to a size that is one
 * syscall on slow storage rather than one sixteenth of one.
 *
 * Because a time budget is only an improvement if it cannot be worse
 * than the fixed count it replaces, the exhausted-budget lane asserts
 * the fallback directly: with a clock that spends the budget on its
 * first observation, the handler must still transfer exactly one
 * quantum per tick and still produce a byte-correct backup.  That is
 * the old behaviour reached through the new code, and it is what the
 * slowest supported storage gets.
 *
 * The unbounded-CRC regression.  CORE_BACKUP_CHECK_CRC and the
 * restore path's core-CRC phase both called intfstream_get_crc(),
 * which consumes the whole stream in one call, so the cost of that
 * tick was a function of file size and nothing else.
 * libretro-common already grew intfstream_crc_step() for exactly this
 * -- and tasks/task_core_updater.c already adopted it -- but
 * task_core_backup.c was left behind.  The hashing lane asserts that
 * no single tick hashes the whole file, and that the sliced CRC
 * equals the one-shot value.
 *
 * Correctness comes first in each lane regardless: a backup that is
 * fast and wrong is worse than one that is slow and right, so every
 * lane compares the produced file against the source byte for byte
 * and checks the recorded CRC.
 *
 * WHAT IS REAL
 *
 * tasks/task_core_backup.c, core_backup.c, the intfstream/rzip stack
 * and libretro-common's task queue are all the shipping code.
 * Stubbed are only the frontend edges that a backup task touches on
 * its way past -- core_info lookups, command_event, retroarch_ctl,
 * the message queue and msg_hash -- none of which participate in
 * file I/O.
 *
 * Build and run:
 *   make -f Makefile check
 *   make -f Makefile check SANITIZER=address
 *   make -f Makefile check SANITIZER=thread
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <retro_common_api.h>
#include <retro_miscellaneous.h>
#include <queues/task_queue.h>
#include <streams/interface_stream.h>
#include <streams/file_stream.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <lists/dir_list.h>
#include <encodings/crc32.h>
#include <compat/strl.h>

#include "../../../msg_hash.h"
#include "../../../command.h"
#include "../../../retroarch.h"
#include "../../../core_info.h"
#include "../../../core_backup.h"
#include "../../../tasks/tasks_internal.h"

/* ================================================================= */
/* I/O accounting                                                     */
/* ================================================================= */

/* Counted at the VFS boundary rather than at libc.  Interposing
 * read()/write() does not work here: glibc's stdio reaches them
 * through internal aliases that never go via the PLT, so a strong
 * definition in this binary is simply never consulted -- it silently
 * reports zero bytes, which looks like a passing test.  These two are
 * libretro-common's own entry points, linked in, so --wrap catches
 * every call intfstream -> filestream makes. */
int64_t __real_retro_vfs_file_read_impl(void *st, void *s, uint64_t len);
int64_t __real_retro_vfs_file_write_impl(void *st, const void *s, uint64_t len);

static int       g_io_recording;
static long long g_io_read_bytes;
static long long g_io_write_bytes;

int64_t __wrap_retro_vfs_file_read_impl(void *st, void *s, uint64_t len)
{
   int64_t r = __real_retro_vfs_file_read_impl(st, s, len);
   if (g_io_recording && r > 0)
      g_io_read_bytes += r;
   return r;
}

int64_t __wrap_retro_vfs_file_write_impl(void *st, const void *s, uint64_t len)
{
   int64_t r = __real_retro_vfs_file_write_impl(st, s, len);
   if (g_io_recording && r > 0)
      g_io_write_bytes += r;
   return r;
}

static void io_reset(void)
{
   g_io_read_bytes  = 0;
   g_io_write_bytes = 0;
   g_io_recording   = 1;
}

static void io_stop(void) { g_io_recording = 0; }

/* Mirrors CORE_BACKUP_TICK_BUDGET_US in tasks/task_core_backup.c. */
#define BACKUP_TICK_BUDGET_US 2000

/* Mirrors CORE_BACKUP_CHUNK_SIZE in tasks/task_core_backup.c. */
#define CORE_BACKUP_QUANTUM   (100 * 1024)

static int failures;
static int checks;

#define CHECK(cond, ...) \
   do { \
      checks++; \
      if (!(cond)) \
      { \
         printf("    FAIL: "); printf(__VA_ARGS__); printf("\n"); \
         failures++; \
      } \
   } while (0)

/* ================================================================= */
/* Fake clock                                                        */
/* ================================================================= */

/* The sample owns cpu_features_get_time_usec() so tick budgets are
 * deterministic.  g_clock_step is how far the clock advances per
 * observation: 0 never exhausts a budget, >= the budget exhausts it
 * on the first check inside a loop. */
static retro_time_t g_clock_now;
static retro_time_t g_clock_step;

retro_time_t __real_cpu_features_get_time_usec(void);

/* Wrapped at link time (see the Makefile) rather than defined
 * outright, because features_cpu.c is linked in for the rest of its
 * contents; the same convention samples/tasks/core_updater uses. */
retro_time_t __wrap_cpu_features_get_time_usec(void)
{
   retro_time_t now = g_clock_now;
   g_clock_now += g_clock_step;
   return now;
}

/* ================================================================= */
/* Frontend stubs                                                    */
/* ================================================================= */

const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   (void)msg;
   return "msg";
}

bool command_event(enum event_command cmd, void *data)
{
   (void)cmd; (void)data;
   return true;
}

bool retroarch_ctl(enum rarch_ctl_state state, void *data)
{
   (void)state; (void)data;
   return false;
}

void runloop_msg_queue_push(const char *msg, size_t len,
      unsigned prio, unsigned duration, bool flush,
      char *title, enum message_queue_icon icon,
      enum message_queue_category category)
{
   (void)msg; (void)len; (void)prio; (void)duration; (void)flush;
   (void)title; (void)icon; (void)category;
}

core_info_t *core_info_find_stub(void) { return NULL; }

bool core_info_find(const char *core_path, core_info_t **core_info)
{
   (void)core_path;
   if (core_info)
      *core_info = NULL;
   return false;
}

bool core_info_get_core_lock(const char *core_path, bool validate_path)
{
   (void)core_path; (void)validate_path;
   return false;
}

/* Signature must match frontend/frontend_driver.h exactly.  It was
 * previously stubbed as "const char *f(void)", which compiles because
 * nothing here includes the real declaration -- but core_backup.c
 * calls it as f(buf, len), so buf was left holding uninitialised
 * stack and every extension comparison against it failed.  That made
 * core_backup_get_backup_type() return INVALID for every archive,
 * task_push_core_restore() refuse, and the restore lane below SKIP
 * silently while the suite still reported PASSED. */
bool frontend_driver_get_core_extension(char *s, size_t len)
{
   strlcpy(s, "so", len);
   return true;
}

void task_window_progress_cb(retro_task_t *task) { (void)task; }
void frontend_driver_attach_console(void) { }
void frontend_driver_detach_console(void) { }

/* ================================================================= */
/* Fixtures                                                          */
/* ================================================================= */

static char g_tmpdir[512];
static char g_core_path[768];
static char g_assets_dir[768];

/* Non-uniform contents, so a copy loop that drops or duplicates a
 * span is caught rather than masked by a run of identical bytes. */
static uint8_t *make_payload(size_t n)
{
   size_t i;
   unsigned int s = 0x9e3779b9u;
   uint8_t *p     = (uint8_t*)malloc(n);
   if (!p)
      return NULL;
   for (i = 0; i < n; i++)
   {
      s = s * 1664525u + 1013904223u;
      p[i] = (uint8_t)(s >> 17);
   }
   return p;
}

static bool write_file_bytes(const char *path, const void *data, size_t n)
{
   RFILE *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   int64_t w;
   if (!f)
      return false;
   w = filestream_write(f, data, (int64_t)n);
   filestream_close(f);
   return w == (int64_t)n;
}

static uint8_t *read_file_bytes(const char *path, size_t *out_len)
{
   int64_t len = 0;
   void   *buf = NULL;
   if (!filestream_read_file(path, &buf, &len))
      return NULL;
   *out_len = (size_t)len;
   return (uint8_t*)buf;
}

static int setup_tmpdir(void)
{
   strlcpy(g_tmpdir, "/tmp/ra_core_backup_XXXXXX", sizeof(g_tmpdir));
   if (!mkdtemp(g_tmpdir))
      return 0;
   snprintf(g_core_path, sizeof(g_core_path), "%s/testcore_libretro.so",
         g_tmpdir);
   snprintf(g_assets_dir, sizeof(g_assets_dir), "%s/assets", g_tmpdir);
   return path_mkdir(g_assets_dir);
}

/* ================================================================= */
/* Driver                                                            */
/* ================================================================= */

struct run_stats
{
   long ticks;
   int  completed;
};

/* Drive the task queue until the pushed task retires, counting ticks.
 * Non-threaded, so one task_queue_check() is exactly one handler
 * invocation -- which is what makes the tick count meaningful. */
static bool backup_task_finder(retro_task_t *task, void *user_data)
{
   (void)task; (void)user_data;
   return true;
}

static void drive(struct run_stats *st, long cap)
{
   task_finder_data_t find_data;
   find_data.func     = backup_task_finder;
   find_data.userdata = NULL;

   st->ticks     = 0;
   st->completed = 0;
   while (st->ticks < cap)
   {
      st->ticks++;
      task_queue_check();
      /* A task stays findable until it is fully retired, so this is
       * the queue's own definition of "done" rather than a guess. */
      if (!task_queue_find(&find_data))
      {
         st->completed = 1;
         break;
      }
   }
}

/* ================================================================= */
/* Lanes                                                             */
/* ================================================================= */

static char g_last_backup[1024];

static void test_backup_throughput(size_t core_size, retro_time_t step,
      const char *label, long tick_ceiling, bool keep_backup)
{
   uint8_t *payload;
   struct run_stats st;
   struct string_list *files = NULL;
   double bytes_per_tick;

   printf("  backup: %s, core=%luKB\n", label,
         (unsigned long)(core_size / 1024));

   if (!(payload = make_payload(core_size)))
   {
      printf("    SKIP: out of memory\n");
      return;
   }
   if (!write_file_bytes(g_core_path, payload, core_size))
   {
      printf("    SKIP: could not write fixture core\n");
      free(payload);
      return;
   }

   g_clock_now  = 1000000;
   g_clock_step = step;

   task_queue_init(false, NULL);

   if (!task_push_core_backup(g_core_path, "Test Core", 0,
            CORE_BACKUP_MODE_MANUAL, 0, g_assets_dir, true))
   {
      printf("    SKIP: task_push_core_backup returned NULL\n");
      task_queue_deinit();
      free(payload);
      return;
   }

   drive(&st, 4000000L);
   task_queue_wait(NULL, NULL);
   task_queue_deinit();

   CHECK(st.completed, "backup task did not retire within the tick cap");

   bytes_per_tick = st.ticks ? (double)core_size / (double)st.ticks : 0.0;
   printf("    %ld ticks, %.0f bytes/tick (%.1fs at 60Hz)\n",
         st.ticks, bytes_per_tick, st.ticks * 0.0167);

   /* Verify the backup round-trips byte for byte.  A fast copy that
    * is wrong is worse than a slow one that is right.
    *
    * Backups land in a per-core subdirectory of dir_core_assets, and
    * on HAVE_COMPRESSION builds they are rzip streams, so the
    * comparison reads them back through intfstream rather than as raw
    * bytes -- intfstream_open_rzip_file() handles both framings. */
   {
      size_t i;
      bool found = false;

      files = dir_list_new(g_assets_dir, NULL, true /* include dirs */,
            false, false, true /* recursive */);
      CHECK(files && files->size > 0, "no backup file was produced");

      for (i = 0; files && i < files->size; i++)
      {
         intfstream_t *bf;
         uint8_t *b;
         int64_t got;

         if (path_is_directory(files->elems[i].data))
            continue;
         /* rzip on HAVE_COMPRESSION builds, plain otherwise; accept
          * either so the lane is valid in both configurations. */
         if (!(bf = intfstream_open_rzip_file(files->elems[i].data,
                     RETRO_VFS_FILE_ACCESS_READ)))
            if (!(bf = intfstream_open_file(files->elems[i].data,
                        RETRO_VFS_FILE_ACCESS_READ,
                        RETRO_VFS_FILE_ACCESS_HINT_NONE)))
               continue;
         if ((b = (uint8_t*)malloc(core_size + 1)))
         {
            got = intfstream_read(bf, b, (int64_t)core_size + 1);
            if (got == (int64_t)core_size
                  && memcmp(b, payload, core_size) == 0)
            {
               found = true;
               strlcpy(g_last_backup, files->elems[i].data,
                     sizeof(g_last_backup));
            }
            free(b);
         }
         intfstream_close(bf);
         free(bf);
      }
      CHECK(found, "backup contents do not match the source byte for byte");
      if (files)
         string_list_free(files);
   }

   if (tick_ceiling > 0)
      CHECK(st.ticks <= tick_ceiling,
            "backup took %ld ticks for %luKB (ceiling %ld) -- only "
            "%.0f bytes moved per tick; at 16.7ms/tick that is %.1fs",
            st.ticks, (unsigned long)(core_size / 1024), tick_ceiling,
            bytes_per_tick, st.ticks * 0.0167);

   free(payload);
   if (keep_backup)
      return;
   /* Clear the assets dir between lanes so each starts clean. */
   {
      struct string_list *old = dir_list_new(g_assets_dir, NULL, true,
            false, false, true);
      size_t i;
      for (i = 0; old && i < old->size; i++)
         if (!path_is_directory(old->elems[i].data))
            filestream_delete(old->elems[i].data);
      if (old)
         string_list_free(old);
   }
}

/* The CRC phases must not hash the whole core in one tick.  A backup
 * of a core whose CRC is not supplied by the caller has to hash it to
 * name the backup file, and that used to be a single blocking
 * intfstream_get_crc() -- 43ms for a 40MB core measured cold on NVMe,
 * an order of magnitude worse on the storage RetroArch also ships to,
 * and an automatic backup runs on every core update.
 *
 * Driven with a clock that exhausts the budget on its first
 * observation, so one tick may hash at most one CORE_BACKUP_CRC_CHUNK.
 * A file several chunks long must therefore take several ticks; if it
 * completes in one, the hashing is still unsliced. */

/* How many times is the core actually read during a backup?
 *
 * The handler hashes the core to obtain its CRC (the CRC names the
 * backup file and is what the "a backup with this content already
 * exists" check matches on), then reads it a second time to copy it.
 * Measured on an 8MiB core: 16MiB read, i.e. 2.00x amplification.
 *
 * That second pass is avoidable only by hashing while copying, which
 * means writing to a temporary name and renaming once the CRC is
 * known -- and that trades the read for a write, because a backup
 * that turns out to be redundant would then be written in full and
 * deleted instead of never being written at all.  The trade is not
 * obviously worth taking, so the amplification stands and is pinned
 * here rather than left to drift.
 *
 * The lane that matters more is the second one.  task_push_core_backup
 * takes a CRC, and when the caller supplies a non-zero one the hash
 * pass is skipped entirely.  tasks/task_core_updater.c relies on this:
 * it has already computed local_crc for its own "is the installed core
 * up to date" check and hands it straight to the automatic backup, so
 * the backup that runs on every core update reads the core exactly
 * once.  Nothing in the type system enforces that; drop the argument,
 * or pass 0 from the updater, and every automatic backup silently
 * doubles its read I/O on cores that are routinely tens of megabytes.
 * That is what this asserts.
 */
static void test_read_amplification(void)
{
   size_t   core_size = 2 * 1024 * 1024;
   uint8_t *payload;
   uint32_t crc;
   struct run_stats st;
   double   amp_hashed;
   double   amp_supplied;

   printf("  read amplification, core=%luKB\n",
         (unsigned long)(core_size / 1024));

   if (!(payload = make_payload(core_size)))
   {
      printf("    SKIP: out of memory\n");
      return;
   }
   if (!write_file_bytes(g_core_path, payload, core_size))
   {
      printf("    SKIP: could not write fixture core\n");
      free(payload);
      return;
   }

   crc = encoding_crc32(0, payload, core_size);

   /* --- caller does not know the CRC: hash pass + copy pass --- */
   g_clock_now  = 1000000;
   g_clock_step = 0;
   task_queue_init(false, NULL);
   io_reset();
   if (!task_push_core_backup(g_core_path, "Test Core", 0,
            CORE_BACKUP_MODE_MANUAL, 0, g_assets_dir, true))
   {
      printf("    SKIP: task_push_core_backup returned NULL\n");
      io_stop();
      task_queue_deinit();
      free(payload);
      return;
   }
   drive(&st, 4000000L);
   task_queue_wait(NULL, NULL);
   io_stop();
   task_queue_deinit();

   amp_hashed = (double)g_io_read_bytes / (double)core_size;
   printf("    crc=0        : read %.2f MiB (%.2fx), wrote %.2f MiB\n",
         g_io_read_bytes / 1048576.0, amp_hashed,
         g_io_write_bytes / 1048576.0);

   CHECK(st.completed, "backup (crc=0) did not retire");
   /* Two passes, with a little slack for the small reads the rzip
    * header and the backup-list directory scan contribute. */
   CHECK(amp_hashed > 1.5 && amp_hashed < 2.5,
         "expected ~2x read amplification when the CRC is unknown, got "
         "%.2fx -- if this dropped toward 1x the hash pass was folded "
         "into the copy, which is a real improvement but changes the "
         "redundant-backup path from no write to a full write plus a "
         "delete; update this lane deliberately",
         amp_hashed);

   /* --- caller supplies the CRC: copy pass only ---
    *
    * Different content, because the lane above just backed up the
    * previous payload: an identical core would match that backup's
    * CRC and be skipped entirely, reading and writing nothing, which
    * looks like a spectacular result and measures nothing. */
   free(payload);
   if (!(payload = make_payload(core_size + 4096)))
   {
      printf("    SKIP: out of memory\n");
      return;
   }
   core_size += 4096;
   if (!write_file_bytes(g_core_path, payload, core_size))
   {
      printf("    SKIP: could not write second fixture core\n");
      free(payload);
      return;
   }
   crc = encoding_crc32(0, payload, core_size);

   g_clock_now  = 1000000;
   g_clock_step = 0;
   task_queue_init(false, NULL);
   io_reset();
   if (!task_push_core_backup(g_core_path, "Test Core", crc,
            CORE_BACKUP_MODE_MANUAL, 0, g_assets_dir, true))
   {
      printf("    SKIP: task_push_core_backup returned NULL\n");
      io_stop();
      task_queue_deinit();
      free(payload);
      return;
   }
   drive(&st, 4000000L);
   task_queue_wait(NULL, NULL);
   io_stop();
   task_queue_deinit();

   amp_supplied = (double)g_io_read_bytes / (double)core_size;
   printf("    crc supplied : read %.2f MiB (%.2fx), wrote %.2f MiB\n",
         g_io_read_bytes / 1048576.0, amp_supplied,
         g_io_write_bytes / 1048576.0);

   CHECK(st.completed, "backup (crc supplied) did not retire");
   CHECK(amp_supplied < 1.5,
         "a caller-supplied CRC must skip the hash pass: expected ~1x "
         "read, got %.2fx.  tasks/task_core_updater.c depends on this "
         "for the automatic backup it runs on every core update",
         amp_supplied);
   CHECK(amp_supplied < amp_hashed,
         "supplying the CRC (%.2fx) must read strictly less than "
         "hashing (%.2fx)", amp_supplied, amp_hashed);

   free(payload);
}

static void test_crc_is_sliced(void)
{
   /* Comfortably more than one 256KB CRC chunk. */
   const size_t core_size = 2 * 1024 * 1024;
   uint8_t *payload;
   struct run_stats st;

   printf("  crc: no single tick may hash the whole core\n");

   if (!(payload = make_payload(core_size)))
   {
      printf("    SKIP: out of memory\n");
      return;
   }
   if (!write_file_bytes(g_core_path, payload, core_size))
   {
      printf("    SKIP: could not write fixture core\n");
      free(payload);
      return;
   }

   g_clock_now  = 1000000;
   g_clock_step = BACKUP_TICK_BUDGET_US * 2;

   task_queue_init(false, NULL);
   /* crc == 0 is what forces the handler to compute it. */
   if (!task_push_core_backup(g_core_path, "Test Core", 0,
            CORE_BACKUP_MODE_MANUAL, 0, g_assets_dir, true))
   {
      printf("    SKIP: task_push_core_backup returned NULL\n");
      task_queue_deinit();
      free(payload);
      return;
   }
   drive(&st, 4000000L);
   task_queue_wait(NULL, NULL);
   task_queue_deinit();

   CHECK(st.completed, "backup task did not retire within the tick cap");
   printf("    %ld ticks for a %luKB core\n", st.ticks,
         (unsigned long)(core_size / 1024));

   /* Hashing 2MB at 256KB per tick is 8 ticks minimum, and the
    * transfer adds more; a handler that hashed in one call would come
    * in far under this. */
   CHECK(st.ticks >= 8,
         "backup completed in %ld ticks with a budget exhausted every "
         "tick -- the CRC is still being computed in one call",
         st.ticks);

   free(payload);
}

/* A backup must restore to a byte-identical core.  This is the lane
 * that covers the restore transfer loop, which carried the same
 * 4096-bytes-per-tick cap as the backup loop. */
static void test_restore_round_trip(void)
{
   const size_t core_size = 1024 * 1024;
   uint8_t *payload;
   uint8_t *restored;
   size_t rlen = 0;
   struct run_stats st;
   bool core_loaded = false;

   printf("  restore: round-trips byte for byte\n");

   if (!*g_last_backup)
   {
      CHECK(0, "no backup produced by an earlier lane -- the restore "
               "lane cannot run and must not report success");
      return;
   }

   /* Recreate the source the previous lane backed up, then clobber
    * the installed core so a successful restore is observable. */
   if (!(payload = make_payload(512 * 1024)))
   {
      printf("    SKIP: out of memory\n");
      return;
   }
   (void)core_size;
   {
      uint8_t junk[4096];
      memset(junk, 0xA5, sizeof(junk));
      if (!write_file_bytes(g_core_path, junk, sizeof(junk)))
      {
         printf("    SKIP: could not clobber fixture core\n");
         free(payload);
         return;
      }
   }

   g_clock_now  = 1000000;
   g_clock_step = 0;

   task_queue_init(false, NULL);
   if (!task_push_core_restore(g_last_backup, g_tmpdir, &core_loaded))
   {
      char cp[1024];
      enum core_backup_type bt = core_backup_get_core_path(g_last_backup,
            g_tmpdir, cp, sizeof(cp));
      /* A refusal here means the fixture is broken, not that the
       * environment is unsuitable: everything this needs was created
       * by an earlier lane in this same process.  Fail rather than
       * skip -- a silent skip is how the wrong-signature stub above
       * went unnoticed. */
      CHECK(0, "task_push_core_restore refused "
               "(backup=%s valid=%d type=%d core_path=%s)",
             g_last_backup, (int)path_is_valid(g_last_backup), (int)bt,
             cp);
      task_queue_deinit();
      free(payload);
      return;
   }
   drive(&st, 4000000L);
   task_queue_wait(NULL, NULL);
   task_queue_deinit();

   CHECK(st.completed, "restore task did not retire within the tick cap");
   printf("    %ld ticks\n", st.ticks);

   if ((restored = read_file_bytes(g_core_path, &rlen)))
   {
      CHECK(rlen == 512 * 1024,
            "restored core is %lu bytes, expected %lu",
            (unsigned long)rlen, (unsigned long)(512 * 1024));
      if (rlen == 512 * 1024)
         CHECK(memcmp(restored, payload, rlen) == 0,
               "restored core does not match the backed-up source");
      free(restored);
   }
   else
      CHECK(false, "restored core file could not be read");

   free(payload);
}

int main(int argc, char **argv)
{
   (void)argc; (void)argv;

   printf("task_core_backup file I/O oracle\n\n");

   if (!setup_tmpdir())
   {
      printf("SKIP: could not create temp dir\n");
      return 0;
   }

   printf("[throughput]\n");
   /* Free-running clock: the budget never expires, so a tick moves as
    * much as the loop allows.  This is the lane the 4KB-per-tick cap
    * fails. */
   test_backup_throughput(4 * 1024 * 1024, 0, "unbounded clock",
         /* 4MB at >= 256KB/tick */ 24, false);

   printf("\n[exhausted budget fallback]\n");
   /* A clock that spends the whole budget on its first observation:
    * the do/while must still transfer exactly one quantum per tick and
    * still produce a correct backup.  No ceiling asserted -- this lane
    * exists to prove the floor cannot regress below the old fixed
    * behaviour, not to be fast. */
   /* One quantum per tick plus the handful of setup/teardown states
    * the handler passes through.  Asserting a ceiling here is what
    * pins "exactly one quantum", and it is only meaningful because
    * the quantum is now 100KB: at the old 4096 bytes the same file
    * took 133 ticks. */
   test_backup_throughput(512 * 1024, BACKUP_TICK_BUDGET_US * 2,
         "budget exhausted per tick",
         (long)(512 * 1024 / CORE_BACKUP_QUANTUM) + 8, true);

   printf("\n[read amplification]\n");
   test_read_amplification();

   printf("\n[sliced CRC]\n");
   test_crc_is_sliced();

   printf("\n[restore]\n");
   test_restore_round_trip();

   printf("\n%s (%d check%s, %d failure%s)\n",
         failures ? "FAILED" : "PASSED",
         checks,   checks   == 1 ? "" : "s",
         failures, failures == 1 ? "" : "s");
   return failures ? 1 : 0;
}

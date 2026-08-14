/* Pacing + throughput oracle for the manual content scanner's BEGIN
 * path in tasks/task_database.c, compiled from the tree so it
 * exercises the shipping translation unit.
 *
 * Two properties are asserted, because either one alone can be
 * satisfied by a scanner that is worse:
 *
 *  1. Pacing.  Under the regular (non-threaded) task queue, no single
 *     task_queue_check() attributable to the scanner may exceed the
 *     shared I/O window by more than bounded slack.  Before the
 *     BEGIN split, the first gather of a large scan performed the
 *     entire recursive directory walk, the whole DAT load and the
 *     playlist setup in one handler invocation - a multi-hundred-
 *     millisecond freeze that this lane fails on the pre-split tree.
 *
 *  2. Throughput.  The total wall-clock time for the same fixed scan
 *     must stay within a small factor of the blocking dir_list_new()
 *     walk plus the DAT parse it replaces.  A scanner that no longer
 *     freezes the UI but takes twice as long is a regression, so the
 *     budget lane cannot pass on pacing alone.
 *
 * The tree is built here - CONTENT_FILES content files spread over
 * CONTENT_DIRS subdirectories, plus a Logiqx DAT naming each entry -
 * so the test needs nothing but a writable directory.
 *
 * Run with: make scan_begin_budget_test SANITIZER=address,undefined
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include <queues/task_queue.h>
#include <lists/dir_list.h>
#include <lists/string_list.h>
#include <retro_timers.h>
#include <streams/file_stream.h>
#include <file/file_path.h>
#include <features/features_cpu.h>
#include <formats/logiqx_dat.h>

#include "../../../core_info.h"
#include "../../../list_special.h"
#include "../../../tasks/tasks_internal.h"
#include "../../../manual_content_scan.h"
#include "../../../playlist.h"
#include "../../../configuration.h"
#include "../../../verbosity.h"

#define SCAN_TIMEOUT_SECONDS 300

#define CONTENT_DIRS  50
#define CONTENT_FILES 5000

/* Per-gather cost for the pacing lane is measured in thread CPU
 * time, not wall time.  The property under test is how much work the
 * scanner chooses to do in one gather; on a shared CI runner a cheap
 * gather can be preempted mid-flight and charged tens of wall-clock
 * milliseconds it never spent, which fails the lane spuriously,
 * while the regression the lane exists for - the pre-split tree
 * doing the whole walk+load+flush in one invocation - burns its
 * >160 ms as CPU and is caught either way.  Wall time remains the
 * clock for the total/throughput and first-feedback lanes, where
 * elapsed time is the property. */
static retro_time_t thread_cpu_usec(void)
{
#if defined(CLOCK_THREAD_CPUTIME_ID)
   struct timespec ts;
   if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0)
      return (retro_time_t)ts.tv_sec * 1000000
           + (retro_time_t)(ts.tv_nsec / 1000);
#endif
   return cpu_features_get_time_usec();
}

/* Slack multiplier over the shared window for the pacing assertion.
 * The window bounds the work the scanner *chooses* to start in a
 * gather; a single readdir()/stat() that straddles the deadline, plus
 * qsort of the final list, plus task-queue bookkeeping, land on top.
 * 4x the 4ms window is still an order of magnitude below the
 * pre-split freeze (>200ms on the same tree, debug build), so the
 * discriminator stays sharp while absorbing CI jitter. */
#define PACING_SLACK_MULTIPLIER 4

/* Throughput guard: incremental scan total time must stay within
 * this factor of (blocking walk + DAT parse + per-entry floor).
 * With the playlist dedup index the measured ratio is ~0.35-0.41;
 * the margin absorbs shared-runner noise while still rejecting the
 * pre-index quadratic flush, which lands at ~1.1. */
#define THROUGHPUT_FACTOR 0.80

/* ------------------------------------------------------------------ */
/* Stubs (same set as database_scan_test.c - see rationale there)     */
/* ------------------------------------------------------------------ */

int msg_hash_get_help_us_enum(enum msg_hash_enums msg, char *s, size_t len)
{
   (void)msg;
   if (s && len)
      s[0] = '\0';
   return 0;
}

const char *msg_hash_to_str_us(enum msg_hash_enums msg)
{
   (void)msg;
   return "";
}

void msg_hash_us_index_init(void)
{
}

settings_t *config_get_ptr(void)
{
   static settings_t settings;
   return &settings;
}

void runloop_msg_queue_push(const char *msg, size_t len,
      unsigned prio, unsigned duration,
      bool flush, char *title, unsigned icon, unsigned category)
{
   (void)msg; (void)len; (void)prio; (void)duration;
   (void)flush; (void)title; (void)icon; (void)category;
}

bool retroarch_override_setting_is_set(unsigned enum_idx, void *data)
{
   (void)enum_idx; (void)data;
   return false;
}

struct string_list *dir_list_new_special(const char *input_dir,
      enum dir_list_type type, const char *filter, bool show_hidden_files)
{
   (void)filter;
   if (type == DIR_LIST_DATABASES)
      return dir_list_new(input_dir, "rdb", false, show_hidden_files,
            false, false);
   return NULL;
}

static void msgq_push(retro_task_t *task, const char *msg,
      unsigned prio, unsigned duration, bool flush)
{
   (void)task; (void)msg; (void)prio; (void)duration; (void)flush;
}

void ui_companion_driver_notify_refresh(void)
{
}

void task_window_progress_cb(retro_task_t *task)
{
   (void)task;
}

/* ------------------------------------------------------------------ */
/* Fixture                                                            */
/* ------------------------------------------------------------------ */

static char fixture_root[4096];

static void rm_rf(const char *path)
{
   char cmd[4352];
   snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
   if (system(cmd) != 0) { /* best effort */ }
}

static bool write_text_file(const char *path, const char *text)
{
   RFILE *f = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   int64_t len;
   if (!f)
      return false;
   len = (int64_t)strlen(text);
   if (filestream_write(f, text, len) != len)
   {
      filestream_close(f);
      return false;
   }
   filestream_close(f);
   return true;
}

/* Build CONTENT_FILES small .bin files across CONTENT_DIRS subdirs,
 * and a Logiqx DAT that names each one, into root (created here). */
static bool build_fixture(const char *root, char *dat_path, size_t dat_path_len,
      char *content_dir, size_t content_dir_len)
{
   size_t i;
   FILE *dat;
   char path[4096];

   snprintf(content_dir, content_dir_len, "%s/content", root);
   snprintf(dat_path, dat_path_len, "%s/games.dat", root);

   if (!path_mkdir(root) || !path_mkdir(content_dir))
      return false;

   for (i = 0; i < CONTENT_DIRS; i++)
   {
      snprintf(path, sizeof(path), "%s/d%02u", content_dir, (unsigned)i);
      if (!path_mkdir(path))
         return false;
   }

   dat = fopen(dat_path, "w");
   if (!dat)
      return false;
   fputs("<?xml version=\"1.0\"?>\n<datafile>\n"
         "<header><name>ScanBench</name></header>\n", dat);

   for (i = 0; i < CONTENT_FILES; i++)
   {
      char body[64];
      snprintf(path, sizeof(path), "%s/d%02u/game%04u.bin",
            content_dir, (unsigned)(i % CONTENT_DIRS), (unsigned)i);
      snprintf(body, sizeof(body), "content-%04u", (unsigned)i);
      if (!write_text_file(path, body))
      {
         fclose(dat);
         return false;
      }
      fprintf(dat,
            "<game name=\"game%04u\">\n"
            "  <description>Game %04u</description>\n"
            "  <year>2026</year>\n"
            "  <manufacturer>Bench &amp; Co.</manufacturer>\n"
            "</game>\n",
            (unsigned)i, (unsigned)i);
   }
   fputs("</datafile>\n", dat);
   fclose(dat);
   return true;
}

/* Big-DAT lane fixture: a DAT large enough that an unbudgeted parse
 * or index build would blow the pacing budget many times over, and
 * a small content set whose names sample the start, middle and end
 * of the list.  Scanning it asserts that no gather grows with DAT
 * size - the last input-proportional single-gather work in the
 * scan.  ~300k games come to ~40MB of XML; the fixture's own cost
 * is one streamed write. */
#define BIG_DAT_GAMES         300000
#define BIG_DAT_CONTENT_FILES 50

static bool build_big_fixture(const char *root, char *dat_path,
      size_t dat_path_len, char *content_dir, size_t content_dir_len,
      char *playlist_dir, size_t playlist_dir_len)
{
   size_t i;
   FILE *dat;
   char path[4096];

   snprintf(content_dir, content_dir_len, "%s/bigcontent", root);
   snprintf(dat_path, dat_path_len, "%s/biggames.dat", root);
   snprintf(playlist_dir, playlist_dir_len, "%s/bigplaylists", root);

   if (   !path_mkdir(content_dir)
       || !path_mkdir(playlist_dir))
      return false;

   /* Every 6000th game exists as a file: hits across the whole
    * list, so a search index that silently covered only a prefix
    * would mislabel the tail. */
   for (i = 0; i < BIG_DAT_CONTENT_FILES; i++)
   {
      char body[64];
      unsigned id = (unsigned)(i * (BIG_DAT_GAMES / BIG_DAT_CONTENT_FILES));
      snprintf(path, sizeof(path), "%s/big%06u.bin", content_dir, id);
      snprintf(body, sizeof(body), "big-%06u", id);
      if (!write_text_file(path, body))
         return false;
   }

   if (!(dat = fopen(dat_path, "w")))
      return false;
   fputs("<?xml version=\"1.0\"?>\n<datafile>\n"
         "<header><name>ScanBench</name></header>\n", dat);
   for (i = 0; i < BIG_DAT_GAMES; i++)
      fprintf(dat,
            "<game name=\"big%06u\">"
            "<description>Big Game %06u</description>"
            "<year>2026</year></game>\n",
            (unsigned)i, (unsigned)i);
   fputs("</datafile>\n", dat);
   fclose(dat);
   return true;
}

/* ------------------------------------------------------------------ */
/* Scan driver                                                        */
/* ------------------------------------------------------------------ */

/* The rescan lane flips this to drive the no-overwrite path. */
static bool scan_overwrite = true;

static bool scan_done;
static bool scan_err;

static void scan_cb(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   (void)task; (void)task_data; (void)user_data;
   if (err && *err)
   {
      fprintf(stderr, "scan error: %s\n", err);
      scan_err = true;
   }
   scan_done = true;
}

typedef struct
{
   retro_time_t total_usec;       /* push -> completion            */
   retro_time_t max_gather_usec;  /* worst single task_queue_check */
   retro_time_t first_feedback;   /* push -> first title/progress  */
   unsigned     gathers;
} scan_metrics_t;

static retro_task_t *found_task;
static bool find_any(retro_task_t *task, void *ud)
{
   (void)ud;
   found_task = task;
   return true;
}

/* Point the menu-backed scan settings at the fixture.  Shared by
 * every lane below. */
static bool configure_scan(const char *content_dir, const char *dat_path,
      const char *playlist_dir)
{
   strlcpy(config_get_ptr()->paths.directory_playlist, playlist_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));

   if (!manual_content_scan_set_menu_scan_method(
            MANUAL_CONTENT_SCAN_METHOD_CUSTOM))
      return false;
   if (!manual_content_scan_set_menu_content_dir(content_dir))
      return false;
   if (!manual_content_scan_set_menu_system_name(
            MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM, "ScanBench"))
      return false;
   /* For the CUSTOM type the setter only records the type; the name
    * itself lives in the menu-backed buffer. */
   strlcpy(manual_content_scan_get_system_name_custom_ptr(), "ScanBench",
         manual_content_scan_get_system_name_custom_size());
   if (!manual_content_scan_set_menu_scan_use_db(
            MANUAL_CONTENT_SCAN_USE_DB_DAT_LOOSE))
      return false;

   strlcpy(manual_content_scan_get_file_exts_custom_ptr(), "bin",
         manual_content_scan_get_file_exts_custom_size());
   strlcpy(manual_content_scan_get_dat_file_path_ptr(), dat_path,
         manual_content_scan_get_dat_file_path_size());
   if (manual_content_scan_validate_dat_file_path()
         != MANUAL_CONTENT_SCAN_DAT_FILE_OK)
   {
      fprintf(stderr, "DAT fixture rejected\n");
      return false;
   }
   *manual_content_scan_get_search_recursively_ptr() = true;
   *manual_content_scan_get_overwrite_playlist_ptr() = scan_overwrite;
   return true;
}

/* Configure and run one DAT-based manual scan of the fixture under
 * the regular (non-threaded) queue, recording pacing metrics. */
static bool run_scan(const char *content_dir, const char *dat_path,
      const char *playlist_dir, scan_metrics_t *m)
{
   retro_time_t t0;
   time_t started;
   task_finder_data_t finder;

   memset(m, 0, sizeof(*m));
   scan_done = false;
   scan_err  = false;

   if (!configure_scan(content_dir, dat_path, playlist_dir))
      return false;

   t0 = cpu_features_get_time_usec();
   if (!task_push_manual_content_scan(false, scan_cb))
   {
      fprintf(stderr, "task_push_manual_content_scan refused\n");
      return false;
   }

   finder.func     = find_any;
   finder.userdata = NULL;

   started = time(NULL);
   while (!scan_done)
   {
      retro_time_t g0, g1;

      if (!m->first_feedback)
      {
         found_task = NULL;
         if (task_queue_find(&finder) && found_task)
         {
            const char *title = found_task->title;
            if ((title && *title) || found_task->progress > 0)
               m->first_feedback =
                     cpu_features_get_time_usec() - t0;
         }
      }

      g0 = thread_cpu_usec();
      task_queue_check();
      g1 = thread_cpu_usec();

      m->gathers++;
      if (g1 - g0 > m->max_gather_usec)
         m->max_gather_usec = g1 - g0;

      if (difftime(time(NULL), started) > SCAN_TIMEOUT_SECONDS)
      {
         fprintf(stderr, "scan did not finish within %d seconds\n",
               SCAN_TIMEOUT_SECONDS);
         return false;
      }
   }
   m->total_usec = cpu_features_get_time_usec() - t0;
   return !scan_err;
}

/* Threaded lane: the same scan under the threaded task queue, where
 * the handler runs on the worker thread while push/find/cancel come
 * from this one.  Pacing is not asserted here - there is no frame to
 * protect - only completion and playlist correctness; under TSan
 * this is the lane that answers whether the BEGIN/END split kept any
 * hidden shared state (the shared window deliberately opts threaded
 * queues out of the static window, and this is where a mistake in
 * that opt-out would surface). */
static bool run_scan_threaded(const char *content_dir, const char *dat_path,
      const char *playlist_dir)
{
   time_t started;

   scan_done = false;
   scan_err  = false;

   if (!configure_scan(content_dir, dat_path, playlist_dir))
      return false;

   task_queue_init(true, msgq_push);

   if (!task_push_manual_content_scan(false, scan_cb))
   {
      fprintf(stderr, "threaded: task_push_manual_content_scan refused\n");
      task_queue_deinit();
      return false;
   }

   started = time(NULL);
   while (!scan_done)
   {
      task_queue_check();
      retro_sleep(1);
      if (difftime(time(NULL), started) > SCAN_TIMEOUT_SECONDS)
      {
         fprintf(stderr, "threaded scan did not finish in time\n");
         task_queue_deinit();
         return false;
      }
   }

   task_queue_deinit();
   return !scan_err;
}

/* Cancellation lane: push the scan, run a bounded number of gathers,
 * then cancel and drain.  Sweeping cancel_after over a range lands
 * the cancellation in every phase of the task - setup, mid-walk,
 * mid-DAT-read, content iteration, mid-flush - which is exactly the
 * set of states that now hold in-flight resources (an open directory
 * iterator, an open DAT stream and half-filled buffer, an open flush
 * playlist).  Under ASan/LSan a resource dropped by the teardown of
 * any of those states is the failure; the lane itself only asserts
 * clean termination. */
static bool run_scan_cancelled(const char *content_dir, const char *dat_path,
      const char *playlist_dir, unsigned cancel_after)
{
   retro_task_t *task = NULL;
   task_finder_data_t finder;
   unsigned i;
   time_t started;

   scan_done = false;
   scan_err  = false;

   if (!configure_scan(content_dir, dat_path, playlist_dir))
      return false;

   task_queue_init(false, msgq_push);

   if (!task_push_manual_content_scan(false, scan_cb))
   {
      fprintf(stderr, "cancel lane: push refused\n");
      task_queue_deinit();
      return false;
   }

   for (i = 0; i < cancel_after && !scan_done; i++)
      task_queue_check();

   /* Cancel whatever is still running... */
   finder.func     = find_any;
   finder.userdata = NULL;
   found_task      = NULL;
   if (!scan_done && task_queue_find(&finder) && found_task)
   {
      task = found_task;
      task_queue_cancel_task(task);
   }

   /* ...and drain to retirement. */
   started = time(NULL);
   while (!scan_done)
   {
      task_queue_check();
      if (difftime(time(NULL), started) > SCAN_TIMEOUT_SECONDS)
      {
         fprintf(stderr, "cancelled scan (after %u) did not drain\n",
               cancel_after);
         task_queue_deinit();
         return false;
      }
   }

   task_queue_deinit();
   /* A cancelled scan may or may not report an error; either way the
    * teardown must have released everything, which the sanitizers
    * decide. */
   return true;
}

/* Playlist correctness: every fixture file present exactly once. */
static bool check_playlist(const char *playlist_dir)
{
   char path[4096];
   playlist_config_t cfg;
   playlist_t *pl;
   size_t n;

   memset(&cfg, 0, sizeof(cfg));
   snprintf(path, sizeof(path), "%s/ScanBench.lpl", playlist_dir);
   playlist_config_set_path(&cfg, path);
   cfg.capacity = COLLECTION_SIZE;

   if (!(pl = playlist_init(&cfg)))
      return false;
   n = playlist_size(pl);
   playlist_free(pl);

   if (n != CONTENT_FILES)
   {
      fprintf(stderr, "playlist has %u entries, expected %u\n",
            (unsigned)n, (unsigned)CONTENT_FILES);
      return false;
   }
   return true;
}

/* Differential check: the dedup index must answer exactly like the
 * linear playlist_entry_exists() scan, with fuzzy archive matching
 * both off and on.  Path IDs are syntactic (archive detection is a
 * string operation), so none of these paths needs to exist.  Also
 * pins the will_add contract: a recorded path becomes visible to
 * the index at once, and playlist_push_unchecked() then brings the
 * playlist into agreement. */
static bool check_dedup_differential(const char *root)
{
   char plpath[4352];
   char p_plain[4352], p_zip[4352], p_inzip[4352], p_case[4352];
   char q_absent[4352], q_bare[4352], q_inother[4352], q_casevar[4352];
   int fz;

   snprintf(plpath,   sizeof(plpath),   "%s/DedupDiff.lpl", root);
   snprintf(p_plain,  sizeof(p_plain),  "%s/dd/plain.bin", root);
   snprintf(p_zip,    sizeof(p_zip),    "%s/dd/archive.zip", root);
   snprintf(p_inzip,  sizeof(p_inzip),  "%s/dd/inner.zip#rom.bin", root);
   snprintf(p_case,   sizeof(p_case),   "%s/dd/CaseFile.BIN", root);
   snprintf(q_absent, sizeof(q_absent), "%s/dd/absent.bin", root);
   snprintf(q_bare,   sizeof(q_bare),   "%s/dd/inner.zip", root);
   snprintf(q_inother,sizeof(q_inother),"%s/dd/archive.zip#other.rom", root);
   snprintf(q_casevar,sizeof(q_casevar),"%s/dd/casefile.bin", root);

   for (fz = 0; fz < 2; fz++)
   {
      playlist_config_t cfg;
      playlist_t *pl        = NULL;
      playlist_dedup_t *dd  = NULL;
      const char *entries[4];
      const char *probes[10];
      size_t i;
      bool ok = false;

      entries[0] = p_plain;
      entries[1] = p_zip;      /* bare archive entry */
      entries[2] = p_inzip;    /* inside-archive entry (distinct archive) */
      entries[3] = p_case;

      probes[0] = p_plain;     /* exact hits */
      probes[1] = p_zip;
      probes[2] = p_inzip;
      probes[3] = p_case;
      probes[4] = q_absent;    /* miss */
      probes[5] = q_bare;      /* bare probe vs inside-archive entry  */
      probes[6] = q_inother;   /* inside probe vs bare-archive entry  */
      probes[7] = q_casevar;   /* case variant: OS-dependent - exactly
                                  why this lane is differential */
      probes[8] = plpath;      /* unrelated existing file: miss */
      probes[9] = "";          /* empty probe: guard parity */

      filestream_delete(plpath);
      memset(&cfg, 0, sizeof(cfg));
      playlist_config_set_path(&cfg, plpath);
      cfg.capacity            = 100;
      cfg.fuzzy_archive_match = (fz == 1);

      if (!(pl = playlist_init(&cfg)))
         return false;

      for (i = 0; i < 4; i++)
      {
         struct playlist_entry e;
         memset(&e, 0, sizeof(e));
         e.path      = (char*)entries[i];
         e.core_path = (char*)"DETECT";
         e.core_name = (char*)"DETECT";
         if (!playlist_push(pl, &e))
         {
            fprintf(stderr, "dedup diff: seed push failed\n");
            goto lane_out;
         }
      }

      if (!(dd = playlist_dedup_init()))
         goto lane_out;
      /* NULL budget callback: seed everything in one call */
      if (!playlist_dedup_seed_step(dd, pl, NULL, NULL))
      {
         fprintf(stderr, "dedup diff: unbudgeted seeding did not finish\n");
         goto lane_out;
      }

      for (i = 0; i < sizeof(probes) / sizeof(probes[0]); i++)
      {
         bool lin = playlist_entry_exists(pl, probes[i]);
         bool idx = playlist_dedup_check_add(dd, pl, probes[i], false);
         if (lin != idx)
         {
            fprintf(stderr,
                  "dedup diff: fuzzy=%d probe %u \"%s\": linear=%d index=%d\n",
                  fz, (unsigned)i, probes[i], (int)lin, (int)idx);
            goto lane_out;
         }
      }

      /* Anchor the fuzzy semantics themselves so the differential
       * cannot rot into vacuity.  In RetroArch proper
       * (RARCH_INTERNAL) the config gates fuzzy matching; this
       * sample builds without RARCH_INTERNAL, where the gate is
       * compiled out and both archive equivalences always hold. */
      {
#ifdef RARCH_INTERNAL
         bool expect_fuzzy = (fz == 1);
#else
         bool expect_fuzzy = true;
#endif
         if (playlist_entry_exists(pl, q_bare)    != expect_fuzzy
          || playlist_entry_exists(pl, q_inother) != expect_fuzzy)
         {
            fprintf(stderr, "dedup diff: fuzzy anchor changed (fuzzy=%d)\n", fz);
            goto lane_out;
         }
      }

      /* will_add: recording makes the path visible to the index
       * immediately; the unchecked push then makes the playlist
       * agree with it. */
      if (playlist_dedup_check_add(dd, pl, q_absent, true))
      {
         fprintf(stderr, "dedup diff: absent path reported present\n");
         goto lane_out;
      }
      if (!playlist_dedup_check_add(dd, pl, q_absent, false))
      {
         fprintf(stderr, "dedup diff: recorded path not visible\n");
         goto lane_out;
      }
      {
         struct playlist_entry e;
         memset(&e, 0, sizeof(e));
         e.path      = (char*)q_absent;
         e.core_path = (char*)"DETECT";
         e.core_name = (char*)"DETECT";
         if (!playlist_push_unchecked(pl, &e))
         {
            fprintf(stderr, "dedup diff: unchecked push failed\n");
            goto lane_out;
         }
      }
      if (!playlist_entry_exists(pl, q_absent))
      {
         fprintf(stderr, "dedup diff: playlist disagrees after push\n");
         goto lane_out;
      }

      ok = true;
lane_out:
      playlist_dedup_free(dd);
      playlist_free(pl);
      if (!ok)
         return false;
   }
   filestream_delete(plpath);
   fprintf(stderr, "[pass] dedup differential lane (fuzzy off/on)\n");
   return true;
}

int main(int argc, char *argv[])
{
   char content_dir[4096];
   char dat_path[4096];
   char playlist_dir[4352];
   scan_metrics_t m;
   retro_time_t walk_usec, dat_usec, floor_usec;
   struct string_list *list;
   int64_t dat_len = 0;
   char *dat_buf   = NULL;
   int rc = 1;
   bool bench_only = (argc > 1 && strcmp(argv[1], "bench") == 0);
   /* Sanitizer mode: run every lane - parity, threaded, the
    * cancellation sweep - but skip the wall-clock assertions, which
    * a sanitized build cannot honestly meet.  The sanitizers are the
    * assertion in that mode. */
   bool sanitize   = (argc > 1 && strcmp(argv[1], "sanitize") == 0);

   (void)argc;
   (void)argv;

   snprintf(fixture_root, sizeof(fixture_root), "%s/scan_begin_fixture",
         getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
   rm_rf(fixture_root);

   /* Logging stays off for the measured lanes: the reference costs
    * (blocking walk, DAT parse) perform no logging, so a measured
    * scan writing ten thousand synchronous per-entry lines to stderr
    * would be charged for the logging, not the scanning.  Failures
    * print their own diagnostics below. */
   retro_main_log_file_init(NULL, false);

   if (!build_fixture(fixture_root, dat_path, sizeof(dat_path),
         content_dir, sizeof(content_dir)))
   {
      fprintf(stderr, "could not build fixture under %s\n", fixture_root);
      return 1;
   }
   snprintf(playlist_dir, sizeof(playlist_dir), "%s/playlists",
         fixture_root);
   if (!path_mkdir(playlist_dir))
      return 1;

   /* Reference cost: the blocking walk this change replaces, plus the
    * DAT parse, plus a small per-entry floor for the matching the
    * scan itself must do.  Taken as the min of 3 reps so a cold page
    * cache does not inflate the budget. */
   walk_usec = INT64_MAX;
   {
      int rep;
      for (rep = 0; rep < 3; rep++)
      {
         retro_time_t w0 = cpu_features_get_time_usec();
         retro_time_t w;
         list = dir_list_new(content_dir, "bin", false, false, false, true);
         if (!list || list->size != CONTENT_FILES)
         {
            fprintf(stderr, "reference walk produced %u entries\n",
                  list ? (unsigned)list->size : 0u);
            goto out;
         }
         dir_list_sort(list, true);
         string_list_free(list);
         w = cpu_features_get_time_usec() - w0;
         if (w < walk_usec)
            walk_usec = w;
      }
   }
   {
      /* Reference cost through the same I/O-free parser the task
       * uses: one whole-file read plus one owned-buffer parse.
       * logiqx_dat_init_owned() takes the buffer either way, so
       * dat_buf must not be freed below. */
      retro_time_t d0 = cpu_features_get_time_usec();
      logiqx_dat_t *dat = NULL;
      if (filestream_read_file(dat_path, (void**)&dat_buf, &dat_len) <= 0)
         goto out;
      dat     = logiqx_dat_init_owned(dat_buf, (size_t)dat_len);
      dat_buf = NULL;
      if (!dat)
      {
         fprintf(stderr, "reference DAT parse failed\n");
         goto out;
      }
      logiqx_dat_free(dat);
      dat_usec = cpu_features_get_time_usec() - d0;
   }
   floor_usec = (retro_time_t)CONTENT_FILES * 40; /* 40us/entry match floor */

   task_queue_init(false /* regular queue: the case that freezes */,
         msgq_push);

   if (!check_dedup_differential(fixture_root))
      goto out_queue;

   if (!run_scan(content_dir, dat_path, playlist_dir, &m))
      goto out_queue;
   if (!check_playlist(playlist_dir))
      goto out_queue;

   fprintf(stderr,
         "[metrics] files=%u total=%.1fms max_gather_cpu=%.2fms "
         "first_feedback=%.2fms gathers=%u walk_ref=%.1fms dat_ref=%.1fms\n",
         (unsigned)CONTENT_FILES,
         m.total_usec      / 1000.0,
         m.max_gather_usec / 1000.0,
         m.first_feedback  / 1000.0,
         m.gathers,
         walk_usec / 1000.0,
         dat_usec  / 1000.0);

   if (bench_only)
   {
      rc = 0;
      goto out_queue;
   }

   /* 1. Pacing: the worst gather must be bounded by the shared window
    *    plus slack - not by the size of the library. */
   if (!sanitize)
   {
      retro_time_t limit = 4000 * PACING_SLACK_MULTIPLIER;
      if (m.max_gather_usec > limit)
      {
         fprintf(stderr,
               "FAIL pacing: max gather %.2fms CPU exceeds %.2fms budget\n",
               m.max_gather_usec / 1000.0, limit / 1000.0);
         goto out_queue;
      }
   }

   /* 2. First feedback within ~2 frames of the push. */
   if (!sanitize && m.first_feedback > 34000)
   {
      fprintf(stderr,
            "FAIL feedback: first task feedback after %.2fms (limit 34ms)\n",
            m.first_feedback / 1000.0);
      goto out_queue;
   }

   /* 3. Throughput: total scan within factor of reference cost. */
   if (!sanitize)
   {
      double ref = (double)(walk_usec + dat_usec + floor_usec);
      if ((double)m.total_usec > ref * THROUGHPUT_FACTOR)
      {
         fprintf(stderr,
               "FAIL throughput: total %.1fms vs reference %.1fms "
               "(factor %.2f, limit %.2f)\n",
               m.total_usec / 1000.0, ref / 1000.0,
               (double)m.total_usec / ref, THROUGHPUT_FACTOR);
         goto out_queue;
      }
   }

   /* Rescan without overwrite: every result must hit the dedup
    * index against the 5000 entries the first pass wrote, nothing
    * may be added twice, and the pre-existing entries drive the
    * budgeted seeding path at scale. */
   scan_overwrite = false;
   {
      scan_metrics_t m2;
      bool rescan_ok = run_scan(content_dir, dat_path, playlist_dir, &m2)
            && check_playlist(playlist_dir);
      scan_overwrite = true;
      if (!rescan_ok)
         goto out_queue;
   }
   fprintf(stderr, "[pass] no-overwrite rescan lane (0 duplicates)\n");

   /* Big-DAT lane: the pacing contract must not scale with DAT
    * size.  Before the incremental DAT parse, this lane's whole
    * ~40MB parse (and the index build after it) sat in single
    * gathers; the assertion below fails on any return to that. */
   {
      char big_dat[4096];
      char big_content[4096];
      char big_playlists[4352];
      scan_metrics_t mb;

      if (!build_big_fixture(fixture_root,
            big_dat, sizeof(big_dat),
            big_content, sizeof(big_content),
            big_playlists, sizeof(big_playlists)))
      {
         fprintf(stderr, "big fixture build failed\n");
         goto out_queue;
      }
      if (!run_scan(big_content, big_dat, big_playlists, &mb))
         goto out_queue;

      /* Correctness: all files present, labelled from the DAT -
       * including the last sample, whose label only an index
       * covering the full list can supply. */
      {
         char plpath[4608];
         playlist_config_t cfg;
         playlist_t *pl;
         const struct playlist_entry *e = NULL;
         size_t n, k;
         bool tail_seen = false;

         memset(&cfg, 0, sizeof(cfg));
         snprintf(plpath, sizeof(plpath), "%s/ScanBench.lpl",
               big_playlists);
         playlist_config_set_path(&cfg, plpath);
         cfg.capacity = COLLECTION_SIZE;
         if (!(pl = playlist_init(&cfg)))
            goto out_queue;
         n = playlist_size(pl);
         for (k = 0; k < n; k++)
         {
            playlist_get_index(pl, k, &e);
            if (e && e->label
                  && !strcmp(e->label, "Big Game 294000"))
               tail_seen = true;
         }
         playlist_free(pl);
         if (n != BIG_DAT_CONTENT_FILES || !tail_seen)
         {
            fprintf(stderr,
                  "big-DAT lane: %u entries (want %u), tail label %s\n",
                  (unsigned)n, (unsigned)BIG_DAT_CONTENT_FILES,
                  tail_seen ? "ok" : "missing");
            goto out_queue;
         }
      }

      if (!sanitize && !bench_only)
      {
         retro_time_t limit = 4000 * PACING_SLACK_MULTIPLIER;
         if (mb.max_gather_usec > limit)
         {
            fprintf(stderr,
                  "FAIL big-DAT pacing: max gather %.2fms CPU exceeds "
                  "%.2fms budget\n",
                  mb.max_gather_usec / 1000.0, limit / 1000.0);
            goto out_queue;
         }
      }

      /* Land cancels inside the budgeted DAT read and parse, which
       * the small fixture's sweep passes in a couple of gathers. */
      {
         static const unsigned big_cancel_points[] = { 32, 128, 512 };
         size_t ci;
         for (ci = 0;
              ci < sizeof(big_cancel_points) / sizeof(big_cancel_points[0]);
              ci++)
         {
            if (!run_scan_cancelled(big_content, big_dat, big_playlists,
                  big_cancel_points[ci]))
               goto out_queue;
         }
      }
      fprintf(stderr, "[pass] big-DAT lane (%u games, pacing%s)\n",
            (unsigned)BIG_DAT_GAMES,
            sanitize ? " skipped under sanitizer" : " held");
   }

   /* The regular-queue metrics lanes are done; tear that queue down
    * before the threaded and cancellation lanes bring up their own. */
   task_queue_deinit();

   /* Threaded lane: correctness under the threaded queue (and the
    * TSan target). */
   {
      char pl[4352];
      snprintf(pl, sizeof(pl), "%s/ScanBench.lpl", playlist_dir);
      filestream_delete(pl);
   }
   if (!run_scan_threaded(content_dir, dat_path, playlist_dir))
      goto out;
   if (!check_playlist(playlist_dir))
      goto out;
   fprintf(stderr, "[pass] threaded lane\n");

   /* Cancellation sweep: land the cancel in every phase.  Gather
    counts are exponential so the sweep covers setup (0-1), the
    * budgeted walk and DAT read (single digits), content iteration
    * (tens to thousands) and the flush (the tail), without running
    * the full scan thousands of times. */
   {
      static const unsigned cancel_points[] =
            { 0, 1, 2, 3, 4, 6, 8, 12, 16, 32, 64, 256, 1024, 4096 };
      size_t ci;
      for (ci = 0; ci < sizeof(cancel_points) / sizeof(cancel_points[0]); ci++)
      {
         if (!run_scan_cancelled(content_dir, dat_path, playlist_dir,
               cancel_points[ci]))
            goto out;
      }
      fprintf(stderr, "[pass] cancellation sweep (%u points)\n",
            (unsigned)(sizeof(cancel_points) / sizeof(cancel_points[0])));
   }

   fprintf(stderr, "PASS scan_begin_budget_test\n");
   rc = 0;
   goto out;

out_queue:
   task_queue_deinit();
out:
   free(dat_buf);
   rm_rf(fixture_root);
   return rc;
}

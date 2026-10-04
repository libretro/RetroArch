/* A content load is a job the frame loop advances, and the window
 * keeps presenting while it runs.
 *
 * Links the shipping RetroArch objects with only main() replaced,
 * boots the frontend on the harness core (built next to this binary
 * by build.sh) with the null drivers, and asks the menu's entry
 * point for a reload of that same core.  The video driver's frame()
 * is hooked to count what the frontend presents; the hook lives on
 * the driver instance the load starts with, so it also says when
 * that instance is gone.
 *
 * Lanes:
 *  - staged: the entry point returns at once; the old core goes down
 *    and a frame is presented on the drivers it left up, the new one
 *    comes up and a frame is presented on the same drivers, only
 *    then are the drivers rebuilt; the new core runs afterwards.  A
 *    load that runs to completion inside the entry point presents
 *    nothing between those points and fails this lane;
 *  - one at a time: a second request while one is in flight is
 *    refused, and the first still completes;
 *  - reinit deferred: a driver reinit asked for while the load is in
 *    flight leaves the drivers as they are - the load rebuilds them;
 *  - fallback: a core that cannot be loaded ends on the dummy core
 *    with the drivers rebuilt and the menu up;
 *  - not a core: a library that opens but lacks retro_init does the
 *    same, and the failure is said.
 *
 * Requires a completed non-Qt build:
 *
 *   ./configure --disable-qt && make
 *   samples/tasks/content_load/build.sh
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <dynamic/dylib.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <time/rtime.h>
#include <retro_timers.h>
#include <rthreads/rthreads.h>
#include <queues/task_queue.h>
#include <retro_atomic.h>

#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../runloop.h"
#include "../../../command.h"
#include "../../../frontend/frontend_driver.h"
#include "../../../frontend/frontend.h"
#include "../../../verbosity.h"
#include "../../../gfx/video_driver.h"
#include "../../../gfx/video_defines.h"
#include "../../../menu/menu_driver.h"
#include "../../../tasks/task_content.h"
#include "../../../tasks/tasks_internal.h"
#include "../../../menu/menu_entries.h"
#include "../../../menu/menu_displaylist.h"
#include "../../../msg_hash_lbl_str.h"
#include "../../../playlist.h"
#include "../../../paths.h"
#include "../../../content.h"
#include "../../../input/input_driver.h"
#include "../../../runahead.h"
#include "../../../state_manager.h"
#ifdef HAVE_NETWORKING
#include "../../../network/netplay/netplay.h"
#endif
#include <features/features_cpu.h>
#include <string/stdstring.h>

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
         fprintf(stderr, __VA_ARGS__); \
         fprintf(stderr, "\n"); \
         failures++; \
      } \
   } while (0)

/* The load is three stages plus the frame the request lands in;
 * anything past this is a job that never finished. */
#define LOAD_FRAMES 10

static char core_path[512];
/* The harness's own temporary directory */
static char harness_dir[400];

/* ------------------------------------------------------------------ */
/* The presented-frame hook                                            */
/* ------------------------------------------------------------------ */

static video_driver_t hooked_video;
static const video_driver_t *hooked_from;
static unsigned presented;

static bool hooked_frame(void *data, const void *frame,
      unsigned dims, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   presented++;
   return hooked_from->frame(data, frame, dims, frame_count,
         pitch, msg, video_info);
}

/* Installs the hook on the live driver instance: a copy of its table
 * with frame() replaced.  The instance is what the frontend frees
 * when it rebuilds the drivers, and the rebuild finds the real table
 * again, so hook_installed() reads false from then on. */
static void hook_install(void)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   hooked_from        = video_st->current_video;
   hooked_video       = *hooked_from;
   hooked_video.frame = hooked_frame;
   video_st->current_video = &hooked_video;
   presented          = 0;
}

static bool hook_installed(void)
{
   return video_state_get_ptr()->current_video == &hooked_video;
}

/* ------------------------------------------------------------------ */

static bool core_is_up(void)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   return (runloop_st->current_core.flags & RETRO_CORE_FLAG_INITED) != 0;
}

static bool menu_is_up(void)
{
   return (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE) != 0;
}

static unsigned core_export(const char *name)
{
   dylib_t lib = runloop_state_get_ptr()->lib_handle;
   unsigned (*fn)(void) = lib
      ? (unsigned (*)(void))dylib_proc(lib, name) : NULL;
   return fn ? fn() : 0;
}

static unsigned core_inits(void)
{
   return core_export("harness_core_inits");
}

static unsigned long core_export_ul(const char *name)
{
   dylib_t lib = runloop_state_get_ptr()->lib_handle;
   unsigned long (*fn)(void) = lib
      ? (unsigned long (*)(void))dylib_proc(lib, name) : NULL;
   return fn ? fn() : 0;
}

/* What one frame of the load looked like. */
struct load_frame
{
   unsigned presented;   /* frames the hook saw during this one */
   bool     core_up;
   bool     hooked;      /* the starting driver instance still there */
   bool     switching;
   bool     video_data;  /* a driver instance exists */
};

/* Frames with the queue pumped, as the main loop pumps it. */
static void pump(unsigned n)
{
   unsigned i;
   for (i = 0; i < n; i++)
   {
      runloop_iterate();
      task_queue_check();
   }
}

/* The load is a task of the queue: a running task carrying the
 * main-thread flag, one at a time. */
static bool main_thread_task_finder(retro_task_t *task, void *user_data)
{
   unsigned *count = (unsigned*)user_data;
   if (task_get_flags(task) & RETRO_TASK_FLG_MAIN_THREAD)
      (*count)++;
   return false;
}

static unsigned main_thread_tasks_running(void)
{
   task_finder_data_t find;
   unsigned count = 0;
   find.func      = main_thread_task_finder;
   find.userdata  = &count;
   task_queue_find(&find);
   return count;
}

/* Runs frames until the load reports done, recording each. */
static unsigned run_load(struct load_frame *log, unsigned cap)
{
   unsigned n;
   for (n = 0; n < cap; n++)
   {
      unsigned before = presented;
      runloop_iterate();
      task_queue_check();
      log[n].presented  = presented - before;
      log[n].core_up    = core_is_up();
      log[n].hooked     = hook_installed();
      log[n].switching  = runloop_is_content_switching();
      log[n].video_data = video_state_get_ptr()->data != NULL;
      if (!log[n].switching)
         return n + 1;
   }
   return n;
}

static void open_menu(void)
{
   if (!menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   runloop_iterate();
   task_queue_check();
   CHECK(menu_is_up(), "menu did not open");
}

/* ------------------------------------------------------------------ */
/* Lane: staged                                                        */
/* ------------------------------------------------------------------ */

static void lane_staged(void)
{
   struct load_frame log[LOAD_FRAMES];
   unsigned n, i;
   unsigned down_presented = 0;   /* presented with no core, old drivers */
   unsigned up_presented   = 0;   /* presented with the new core, old drivers */
   bool seen_down          = false;
   unsigned had            = failures;

   open_menu();
   hook_install();

   CHECK(task_push_load_contentless_core_from_menu(core_path),
         "the reload was not started");
   CHECK(runloop_is_content_switching(),
         "the entry point returned without a load in flight");
   CHECK(core_is_up() && hook_installed(),
         "the entry point did work the frame loop owns");
   CHECK(main_thread_tasks_running() == 1,
         "the load is not a task of the queue (%u main-thread tasks running)",
         main_thread_tasks_running());

   n = run_load(log, LOAD_FRAMES);
   CHECK(n < LOAD_FRAMES && !runloop_is_content_switching(),
         "the load did not finish within %u frames", LOAD_FRAMES);
   CHECK(main_thread_tasks_running() == 0,
         "a load task is still on the queue after the load");

   for (i = 0; i < n; i++)
   {
      /* Whatever the stage, a driver instance is there to present on. */
      CHECK(log[i].video_data, "frame %u: no video driver instance", i);
      if (!log[i].core_up)
      {
         seen_down = true;
         CHECK(log[i].hooked,
               "frame %u: drivers rebuilt before the new core came up", i);
         down_presented += log[i].presented;
      }
      else if (seen_down && log[i].hooked)
         up_presented += log[i].presented;
   }
   CHECK(seen_down, "the old core never went down between frames");
   CHECK(down_presented >= 1,
         "no frame presented while the old core was down (%u)",
         down_presented);
   CHECK(up_presented >= 1,
         "no frame presented on the old drivers after the new core "
         "came up (%u)", up_presented);
   CHECK(!hook_installed(), "the drivers were never rebuilt");
   CHECK(core_is_up(), "no core after the load");
   if (failures != had)
      for (i = 0; i < n; i++)
         fprintf(stderr, "   frame %u: presented %u core_up %d hooked %d "
               "switching %d video_data %d\n", i, log[i].presented,
               log[i].core_up, log[i].hooked, log[i].switching,
               log[i].video_data);
   CHECK(runloop_state_get_ptr()->current_core_type != CORE_TYPE_DUMMY,
         "the dummy core is running after the load");
   CHECK(core_inits() == 1, "the core was not brought up fresh (%u)",
         core_inits());
   /* RESUME closed the menu; the core runs. */
   runloop_iterate();
   CHECK(!menu_is_up(), "menu still up after the load");

   if (failures == had)
      fprintf(stderr, "[pass] staged lane: %u frames, %u presented with "
            "the core down, %u on the old drivers after it came up\n",
            n, down_presented, up_presented);
}

/* ------------------------------------------------------------------ */
/* Lane: one at a time                                                 */
/* ------------------------------------------------------------------ */

/* The running core is opened once per load: nothing after its init
 * reopens it to re-read its system info, so its retro_set_environment
 * runs exactly once on the live instance, and what the menu shows -
 * library name and version, no-game support - comes from that open. */
static void lane_core_opened_once(void)
{
   struct load_frame log[LOAD_FRAMES];
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   unsigned n;
   unsigned had = failures;

   open_menu();

   CHECK(task_push_load_contentless_core_from_menu(core_path),
         "the load was not started");
   n = run_load(log, LOAD_FRAMES);
   CHECK(n < LOAD_FRAMES && core_is_up(),
         "the load did not bring the core up within %u frames", LOAD_FRAMES);
   CHECK(core_export("harness_core_env_sets") == 1,
         "retro_set_environment ran %u times on the running core, "
         "not once", core_export("harness_core_env_sets"));
   CHECK(string_is_equal(runloop_st->current_library_name,
            "content_load_harness")
         && string_is_equal(runloop_st->current_library_version, "1"),
         "the library strings are \"%s\" \"%s\"",
         runloop_st->current_library_name,
         runloop_st->current_library_version);
   CHECK(runloop_st->system.load_no_content,
         "the core's no-game support was not recorded");

   if (failures == had)
      fprintf(stderr, "[pass] core opened once lane\n");
}

#if defined(HAVE_DYNAMIC)
static void harness_sibling(char *s, size_t len, const char *name)
{
   const char *slash = strrchr(core_path, '/');
   snprintf(s, len, "%.*s/%s",
         slash ? (int)(slash - core_path) : 1, slash ? core_path : ".", name);
}

/* What LOAD_CORE asks of a core before loading it is kept, keyed by
 * its file's size and modification time: a core already asked is not
 * opened again at push - the core running here would see its
 * retro_set_environment called if it were - a changed file is, and the
 * record outlives the session. */
static void lane_probe_cached(void)
{
   char cache_file[600];
   int64_t mtime = 0;
   unsigned before;
   unsigned had = failures;

   snprintf(cache_file, sizeof(cache_file), "%s/core_probe.cache", harness_dir);
   open_menu();
   CHECK(task_push_load_contentless_core_from_menu(core_path),
         "the load was not started");
   pump(LOAD_FRAMES);
   CHECK(core_is_up(), "the core did not come up");

   before = core_export("harness_core_env_sets");
   CHECK(task_push_load_contentless_core_from_menu(core_path),
         "the reload was not started");
   CHECK(core_export("harness_core_env_sets") == before,
         "the push opened a core it had asked before (%u calls on the "
         "running core, not %u)", core_export("harness_core_env_sets"),
         before);
   pump(LOAD_FRAMES);
   CHECK(core_is_up(), "the reload did not go through");
   CHECK(path_is_valid(cache_file), "no %s was written", cache_file);

   /* A changed file is asked again */
   CHECK(path_get_mtime(core_path, &mtime), "no mtime for the core");
   CHECK(path_set_mtime(core_path, mtime + 10), "could not touch the core");
   before = core_export("harness_core_env_sets");
   CHECK(task_push_load_contentless_core_from_menu(core_path),
         "the load of the changed core was not started");
   CHECK(core_export("harness_core_env_sets") == before + 2,
         "the changed core was not asked again (%u calls, expected %u)",
         core_export("harness_core_env_sets"), before + 2);
   pump(LOAD_FRAMES);
   CHECK(core_is_up(), "the load of the changed core did not go through");

   /* The record of the changed file comes back from disk */
   runloop_core_probe_cache_free();
   before = core_export("harness_core_env_sets");
   CHECK(task_push_load_contentless_core_from_menu(core_path),
         "the load after the records were dropped was not started");
   CHECK(core_export("harness_core_env_sets") == before,
         "the record was not read back from %s", cache_file);
   pump(LOAD_FRAMES);
   CHECK(core_is_up(), "the load after the records were dropped did not "
         "go through");

   path_set_mtime(core_path, mtime);
   if (failures == had)
      fprintf(stderr, "[pass] probe-cached lane\n");
}

#if defined(HAVE_MENU)
/* The menu's read-ahead of content follows the core being loaded: its
 * need_fullpath and its own content info overrides, not those of the
 * core still running when the load is pushed. */
static void lane_prefetch_follows_new_core(void)
{
   content_ctx_info_t info;
   char nohw[600];
   char game[600];
   FILE *f;
   unsigned had = failures;

   harness_sibling(nohw, sizeof(nohw), "harness_core_nohw.so");
   snprintf(game, sizeof(game), "%s/game.fpath", harness_dir);
   if ((f = fopen(game, "wb")))
   {
      fputs("harness content", f);
      fclose(f);
   }
   memset(&info, 0, sizeof(info));
   open_menu();

   /* Running: the core with no overrides.  Loading: the one that takes
    * .fpath as a path - nothing to read ahead. */
   CHECK(task_push_load_contentless_core_from_menu(nohw),
         "the software core's load was not started");
   pump(LOAD_FRAMES);
   CHECK(task_push_load_content_with_new_core_from_menu(core_path, game,
            &info, CORE_TYPE_PLAIN, NULL, NULL),
         "the load of .fpath content was not started");
   CHECK(!(content_state_get_ptr()->flags & CONTENT_ST_FLAG_DEFERRED_LOAD_PENDING),
         "content the new core takes as a path was read ahead");
   pump(LOAD_FRAMES * 4);
   CHECK(core_is_up(), "the load of .fpath content did not go through");

   /* Running: the core that overrides .fpath.  Loading: one that reads
    * it into memory - read ahead. */
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_with_new_core_from_menu(nohw, game,
            &info, CORE_TYPE_PLAIN, NULL, NULL),
         "the second load of .fpath content was not started");
   CHECK(content_state_get_ptr()->flags & CONTENT_ST_FLAG_DEFERRED_LOAD_PENDING,
         "content the new core reads into memory was not read ahead");
   pump(LOAD_FRAMES * 4);
   CHECK(core_is_up(), "the second load of .fpath content did not go through");

   remove(game);
   if (failures == had)
      fprintf(stderr, "[pass] prefetch-follows-new-core lane\n");
}
#endif
#endif

#if defined(HAVE_DYNAMIC) && defined(HAVE_MENU)
/* A load without content names its saves after the core, not after
 * the content loaded before it. */
static void lane_contentless_names(void)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   content_ctx_info_t info;
   char nohw[600];
   char game[600];
   FILE *f;
   unsigned had = failures;

   harness_sibling(nohw, sizeof(nohw), "harness_core_nohw.so");
   snprintf(game, sizeof(game), "%s/names.bin", harness_dir);
   if ((f = fopen(game, "wb")))
   {
      fputs("harness content", f);
      fclose(f);
   }
   memset(&info, 0, sizeof(info));
   open_menu();

   CHECK(task_push_load_contentless_core_from_menu(core_path),
         "the contentless load was not started");
   pump(LOAD_FRAMES);
   CHECK(task_push_load_content_with_new_core_from_menu(nohw, game,
            &info, CORE_TYPE_PLAIN, NULL, NULL),
         "the content load was not started");
   pump(LOAD_FRAMES * 4);
   CHECK(core_is_up() && strstr(path_get(RARCH_PATH_BASENAME), "names"),
         "the content load did not name its saves after the content (\"%s\")",
         path_get(RARCH_PATH_BASENAME));

   CHECK(task_push_load_contentless_core_from_menu(core_path),
         "the second contentless load was not started");
   pump(LOAD_FRAMES);
   CHECK(core_is_up(), "the second contentless load did not go through");
   CHECK(path_is_empty(RARCH_PATH_BASENAME),
         "the contentless load kept the content's name \"%s\"",
         path_get(RARCH_PATH_BASENAME));
   CHECK(    !strstr(runloop_st->name.savestate, "names")
         &&  strstr(runloop_st->name.savestate, "content_load_harness"),
         "the contentless load saves states to \"%s\"",
         runloop_st->name.savestate);

   remove(game);
   if (failures == had)
      fprintf(stderr, "[pass] contentless-names lane\n");
}
#endif

#ifdef HAVE_REWIND
/* The rewind buffer is one full serialize of the core: a load leaves
 * it for the frame after, which takes it before the core runs. */
static void lane_rewind_after_load(void)
{
   struct load_frame log[LOAD_FRAMES];
   settings_t *settings        = config_get_ptr();
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   bool rewind_enable          = settings->bools.rewind_enable;
   unsigned n, i;
   unsigned had = failures;

   configuration_set_bool(settings, settings->bools.rewind_enable, true);
   open_menu();
   CHECK(task_push_load_contentless_core_from_menu(core_path),
         "the load was not started");
   for (n = 0; n < LOAD_FRAMES && runloop_is_content_switching(); n++)
   {
      runloop_iterate();
      task_queue_check();
      CHECK(!(runloop_st->rewind_st.flags
               & STATE_MGR_REWIND_ST_FLAG_INIT_ATTEMPTED),
            "frame %u of the load set up the rewind buffer", n);
   }
   CHECK(!runloop_is_content_switching() && core_is_up(),
         "the load did not finish");
   CHECK(runloop_st->rewind_st.flags & STATE_MGR_REWIND_ST_FLAG_INIT_PENDING,
         "the load did not leave the rewind buffer for the frame after");
   for (i = 0; i < 2 && !(runloop_st->rewind_st.flags
            & STATE_MGR_REWIND_ST_FLAG_INIT_ATTEMPTED); i++)
   {
      runloop_iterate();
      task_queue_check();
   }
   CHECK(i == 1,
         "the rewind buffer was set up %u frames after the load, not on "
         "the first", i);
   CHECK(!(runloop_st->rewind_st.flags & STATE_MGR_REWIND_ST_FLAG_INIT_PENDING),
         "the rewind buffer is still pending after it was set up");

   command_event(CMD_EVENT_REWIND_DEINIT, NULL);
   configuration_set_bool(settings, settings->bools.rewind_enable,
         rewind_enable);
   if (failures == had)
      fprintf(stderr, "[pass] rewind-after-load lane\n");
}
#endif

#if defined(HAVE_DYNAMIC) && defined(HAVE_THREADS)
/* A staged load opens the core's library on the task worker, off the
 * frame-loop thread, and the core stage takes that handle; a same-core
 * reload still comes up on fresh statics, because the open waits for
 * the old handle to be released. */
static void lane_core_preloaded(void)
{
   settings_t *settings = config_get_ptr();
   unsigned long load_tid, init_tid;
   unsigned had = failures;

   configuration_set_bool(settings, settings->bools.threaded_data_runloop_enable, true);
   task_queue_set_threaded();
   open_menu();

   CHECK(task_push_load_contentless_core_from_menu(core_path),
         "the load was not started");
   pump(LOAD_FRAMES);
   CHECK(core_is_up(), "the core did not come up");
   CHECK(core_inits() == 1, "the core came up %u times", core_inits());

   load_tid = core_export_ul("harness_core_load_tid");
   init_tid = core_export_ul("harness_core_init_tid");
   CHECK(load_tid != 0 && init_tid != 0, "the core did not record its threads");
   CHECK(load_tid != init_tid,
         "the library was opened on the frame-loop thread, not a worker");

   /* Same core again: the old handle is released before the open, so
    * the reloaded instance's statics are its own */
   CHECK(task_push_load_contentless_core_from_menu(core_path),
         "the reload was not started");
   pump(LOAD_FRAMES);
   CHECK(core_is_up(), "the reload did not go through");
   CHECK(core_inits() == 1,
         "the reload came up on the previous instance's statics (inits %u)",
         core_inits());
   CHECK(core_export_ul("harness_core_load_tid") != core_export_ul("harness_core_init_tid"),
         "the reload's library open was not on a worker");

   configuration_set_bool(settings, settings->bools.threaded_data_runloop_enable, false);
   task_queue_unset_threaded();
   pump(2);

   if (failures == had)
      fprintf(stderr, "[pass] core-preloaded lane\n");
}
#endif

static void lane_one_at_a_time(void)
{
   struct load_frame log[LOAD_FRAMES];
   unsigned n;
   unsigned had = failures;

   open_menu();
   hook_install();

   CHECK(task_push_load_contentless_core_from_menu(core_path),
         "the reload was not started");
   CHECK(!task_push_load_contentless_core_from_menu(core_path),
         "a second load was started over one in flight");
   CHECK(runloop_is_content_switching(),
         "the refused request ended the load in flight");

   n = run_load(log, LOAD_FRAMES);
   CHECK(n < LOAD_FRAMES && !runloop_is_content_switching(),
         "the load did not finish within %u frames", LOAD_FRAMES);
   CHECK(core_is_up() && core_inits() == 1, "the first load did not go through");
   runloop_iterate();

   if (failures == had)
      fprintf(stderr, "[pass] one-at-a-time lane\n");
}

/* ------------------------------------------------------------------ */
/* Lane: reinit deferred                                               */
/* ------------------------------------------------------------------ */

static void lane_reinit_deferred(void)
{
   struct load_frame log[LOAD_FRAMES];
   unsigned n;
   unsigned had = failures;

   open_menu();
   hook_install();

   CHECK(task_push_load_contentless_core_from_menu(core_path),
         "the reload was not started");
   /* Past the close: the old drivers are up with no core behind them. */
   pump(1);
   CHECK(!core_is_up() && hook_installed(), "not at the close stage");

   command_event(CMD_EVENT_REINIT, NULL);
   CHECK(hook_installed(),
         "a reinit rebuilt the drivers in the middle of the load");

   n = run_load(log, LOAD_FRAMES);
   CHECK(n < LOAD_FRAMES && !runloop_is_content_switching(),
         "the load did not finish within %u frames", LOAD_FRAMES);
   CHECK(!hook_installed() && core_is_up(),
         "the load did not rebuild the drivers itself");
   runloop_iterate();

   if (failures == had)
      fprintf(stderr, "[pass] reinit-deferred lane\n");
}

/* ------------------------------------------------------------------ */
/* Lane: fallback                                                      */
/* ------------------------------------------------------------------ */

static void lane_fallback(void)
{
   struct load_frame log[LOAD_FRAMES];
   char bogus[512];
   unsigned n;
   unsigned had = failures;

   snprintf(bogus, sizeof(bogus), "%.500s.missing", core_path);

   open_menu();
   hook_install();

   CHECK(task_push_load_contentless_core_from_menu(bogus),
         "the load of a missing core was not started");
   n = run_load(log, LOAD_FRAMES);
   CHECK(n < LOAD_FRAMES && !runloop_is_content_switching(),
         "the load did not finish within %u frames", LOAD_FRAMES);
   CHECK(video_state_get_ptr()->data != NULL, "no drivers after the fallback");
   CHECK(!hook_installed(), "the drivers were not rebuilt for the fallback");
   CHECK(runloop_state_get_ptr()->current_core_type == CORE_TYPE_DUMMY,
         "the fallback is not the dummy core");
   runloop_iterate();
   CHECK(menu_is_up(), "menu not up after a failed load");

   /* Back on the harness core for the shutdown. */
   CHECK(task_push_load_contentless_core_from_menu(core_path),
         "the reload after the fallback was not started");
   n = run_load(log, LOAD_FRAMES);
   CHECK(n < LOAD_FRAMES && core_is_up()
         && runloop_state_get_ptr()->current_core_type != CORE_TYPE_DUMMY,
         "the reload after the fallback did not go through");
   runloop_iterate();

   if (failures == had)
      fprintf(stderr, "[pass] fallback lane\n");
}

/* ------------------------------------------------------------------ */
/* Lane: hardware-render request                                       */
/* ------------------------------------------------------------------ */

/* The core makes its hardware-render request during the core stage,
 * while the previous session's drivers are still up.  The request is
 * for a context those drivers never built: freeing them must not
 * destroy it (the core's context_destroy before any context_reset -
 * a hardware core then runs with no context), and drivers_init must
 * still see it.  Context flags set in that window are for the next
 * context too, so they defer rather than land on the one being freed. */
/* A context that takes flags, standing in for a GL context's: the
 * null driver's has no set_flags, which defers by itself and proves
 * nothing. */
static unsigned ctx_flags_applied;
static void ctx_set_flags(void *data, uint32_t flags)
{
   (void)data; (void)flags;
   ctx_flags_applied++;
}

static void lane_hw_request(void)
{
   struct load_frame log[LOAD_FRAMES];
   video_driver_state_t *video_st = video_state_get_ptr();
   unsigned n;
   unsigned had = failures;

   open_menu();
   hook_install();

   CHECK(task_push_load_contentless_core_from_menu(core_path),
         "the reload was not started");
   /* Past the close and the core stage: the request has been made
    * on the old drivers. */
   pump(2);
   CHECK(core_is_up() && hook_installed(), "not at the core stage");
   CHECK(video_st->hw_render.context_reset != NULL,
         "the core's hardware-render request was not taken");
   {
      gfx_ctx_flags_t flags;
      void (*saved)(void*, uint32_t) = video_st->current_video_context.set_flags;
      video_st->current_video_context.set_flags = ctx_set_flags;
      ctx_flags_applied = 0;
      flags.flags = 0;
      BIT32_SET(flags.flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT);
      video_context_driver_set_flags(&flags);
      video_st->current_video_context.set_flags = saved;
      CHECK(ctx_flags_applied == 0,
            "context flags set mid-load landed on the context being freed");
      CHECK(video_st->deferred_flag_data.flags == flags.flags
            && (video_driver_get_disp_flags()
               & VIDEO_FLAG_DEFERRED_VIDEO_CTX_DRIVER_SET_FLAGS),
            "context flags set mid-load were not deferred to the next context");
   }

   n = run_load(log, LOAD_FRAMES);
   CHECK(n < LOAD_FRAMES && !hook_installed() && core_is_up(),
         "the load did not go through");
   CHECK(core_export("harness_core_hw_destroys") == 0,
         "the new core's context was destroyed before it was reset (%u)",
         core_export("harness_core_hw_destroys"));
   CHECK(video_st->hw_render.context_reset != NULL
         && video_st->hw_render.context_destroy != NULL,
         "the hardware-render request did not survive the driver rebuild");
   runloop_iterate();

   if (failures == had)
      fprintf(stderr, "[pass] hw-request lane\n");
}

/* ------------------------------------------------------------------ */
/* Lane: the queue survives the load                                   */
/* ------------------------------------------------------------------ */

/* A load onto a running session applies the task-queue settings and
 * leaves the queue alone: the worker keeps its thread and a task in
 * flight keeps running.  Under the threaded queue the worker's thread
 * id is the witness - a queue rebuilt by the load spawns a new one. */
static sthread_tls_t probe_tls;
static void         *probe_seen[2];
static unsigned      probe_done;
static retro_atomic_int_t long_task_release;
static unsigned      long_task_retired;

/* The first probe marks the worker's thread-local slot; the second
 * reads it.  A worker thread the load rebuilt carries no mark. */
static void probe_handler(retro_task_t *task)
{
   probe_seen[probe_done] = sthread_tls_get(&probe_tls);
   sthread_tls_set(&probe_tls, (void*)1);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}
static void probe_callback(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   (void)task; (void)task_data; (void)user_data; (void)err;
   probe_done++;
}
static void long_handler(retro_task_t *task)
{
   if (retro_atomic_load_acquire_int(&long_task_release))
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   else
      retro_sleep(1);
}
static void long_callback(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   (void)task; (void)task_data; (void)user_data; (void)err;
   long_task_retired++;
}

static retro_task_t *push_task(retro_task_handler_t handler,
      retro_task_callback_t callback)
{
   retro_task_t *task = task_init();
   if (!task)
      return NULL;
   task->handler  = handler;
   task->callback = callback;
   task->flags   |= RETRO_TASK_FLG_MUTE;
   task_queue_push(task);
   return task;
}

static void lane_queue_survives(void)
{
   settings_t *settings = config_get_ptr();
   unsigned had = failures;
   unsigned i;

   /* Threaded Tasks on, as the setting the menu writes; the queue
    * swaps at the next check. */
   configuration_set_bool(settings, settings->bools.threaded_data_runloop_enable, true);
   task_queue_set_threaded();
   task_queue_check();
   if (!task_queue_is_threaded())
   {
      fprintf(stderr, "[skip] queue-survives lane (no threaded queue)\n");
      return;
   }

   probe_done        = 0;
   long_task_retired = 0;
   retro_atomic_store_release_int(&long_task_release, 0);
   CHECK(sthread_tls_create(&probe_tls), "no thread-local slot");
   CHECK(push_task(probe_handler, probe_callback) != NULL, "probe not pushed");
   for (i = 0; i < 200 && probe_done < 1; i++)
      pump(1);
   CHECK(probe_done == 1, "the first probe did not retire");
   CHECK(push_task(long_handler, long_callback) != NULL, "long task not pushed");
   pump(2);

   open_menu();
   CHECK(task_push_load_contentless_core_from_menu(core_path),
         "the reload was not started");
   for (i = 0; i < LOAD_FRAMES && runloop_is_content_switching(); i++)
      pump(1);
   CHECK(!runloop_is_content_switching() && core_is_up(),
         "the load did not go through");
   CHECK(task_queue_is_threaded(), "the load turned the threaded queue off");
   CHECK(long_task_retired == 0, "the task in flight retired during the load");

   retro_atomic_store_release_int(&long_task_release, 1);
   CHECK(push_task(probe_handler, probe_callback) != NULL, "probe not pushed");
   for (i = 0; i < 200 && (probe_done < 2 || !long_task_retired); i++)
      pump(1);
   CHECK(probe_done == 2, "the second probe did not retire");
   CHECK(long_task_retired == 1, "the task in flight did not retire after the load");
   CHECK(probe_seen[0] == NULL && probe_seen[1] == (void*)1,
         "the load rebuilt the queue: a new worker thread after it");
   sthread_tls_delete(&probe_tls);

   /* Back to the inline queue for the rest. */
   configuration_set_bool(settings, settings->bools.threaded_data_runloop_enable, false);
   task_queue_unset_threaded();
   task_queue_check();
   CHECK(!task_queue_is_threaded(), "the queue did not swap back");
   pump(1);

   if (failures == had)
      fprintf(stderr, "[pass] queue-survives lane\n");
}

/* ------------------------------------------------------------------ */
/* Lane: the close waits for a save in flight, presenting             */
/* ------------------------------------------------------------------ */

/* A save state task still inside the core when content closes: the
 * close must wait for it (the task calls the core's serialize, and
 * the close unloads the library), and the wait is spent in the frame
 * loop - frames presented, retro_run never entered - rather than
 * blocking inside one frame. */
static void lane_close_waits_for_save(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   unsigned had = failures;
   unsigned i, runs_at_wait = 0, presented_waiting = 0, waited = 0;
   bool ran_while_closing = false;
   settings_t *settings = config_get_ptr();

   /* The core runs (menu closed) so a save can be taken. */
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   pump(2);
   CHECK(core_is_up() && !menu_is_up(), "not running the core");

   configuration_set_int(settings, settings->ints.state_slot, 0);
   CHECK(command_event(CMD_EVENT_SAVE_STATE, NULL), "save state not started");
   CHECK(content_save_state_in_progress(NULL), "no save task in flight");

   hook_install();
   open_menu();
   menu_st->flags |= MENU_ST_FLAG_PENDING_CLOSE_CONTENT;
   pump(1);
   CHECK(runloop_is_content_switching(), "the close did not start a staged load");

   /* The save is written under the per-frame I/O window, so the frames
    * it takes follow the machine's speed: a dozen on a release build,
    * some five hundred under TSan.  The bound only stops a close that
    * never ends. */
   for (i = 0; i < 20000 && runloop_is_content_switching(); i++)
   {
      unsigned before = presented;
      pump(1);
      if (runloop_is_content_closing())
      {
         /* The old core is still loaded while the close waits: its
          * run count must stand still. */
         if (!waited)
            runs_at_wait = core_export("harness_core_runs");
         else if (core_export("harness_core_runs") != runs_at_wait)
            ran_while_closing = true;
         waited++;
         presented_waiting += presented - before;
         CHECK(!content_save_state_in_progress(NULL) || hook_installed(),
               "drivers rebuilt while the close was still waiting");
      }
   }
   CHECK(!runloop_is_content_switching(), "the close did not finish");
   CHECK(waited >= 1, "the close never waited for the save (%u frames)", waited);
   CHECK(presented_waiting >= 1,
         "no frame presented while the close waited (%u waits)", waited);
   CHECK(!content_save_state_in_progress(NULL), "the save task did not finish");
   CHECK(!ran_while_closing, "retro_run entered while the close waited");
   CHECK(runloop_state_get_ptr()->current_core_type == CORE_TYPE_DUMMY,
         "not on the dummy core after the close");

   /* Back on the harness core. */
   pump(2);
   {
      content_ctx_info_t content_info = {0};
      struct load_frame log[LOAD_FRAMES];
      unsigned n;
      CHECK(task_push_start_current_core(&content_info), "Start Core not started");
      n = run_load(log, LOAD_FRAMES);
      CHECK(n < LOAD_FRAMES && core_is_up(), "Start Core did not go through");
      pump(1);
   }

   if (failures == had)
      fprintf(stderr, "[pass] close-waits-for-save lane (%u frames waited, "
            "%u presented)\n", waited, presented_waiting);
}

/* ------------------------------------------------------------------ */
/* Lane: host setup waits for the relay query from the frame loop      */
/* ------------------------------------------------------------------ */

#ifdef HAVE_NETWORKING
/* Hosting through a lobby relay: the relay's address is asked of the
 * lobby server, and host setup is put off to a main-thread task until
 * the answer is in (or its bound passes) instead of waiting inside the
 * command.  The query is a real HTTP task: whether the lobby answers,
 * refuses or cannot be reached, the command must return with the
 * setup queued, frames must keep presenting, and the setup must run
 * once the query is settled. */
static void lane_host_setup_deferred(void)
{
   settings_t *settings = config_get_ptr();
   unsigned had = failures;
   unsigned i, frames = 0, before;
   retro_time_t started, took;

   configuration_set_bool(settings, settings->bools.netplay_use_mitm_server, true);
   strlcpy(settings->arrays.netplay_mitm_server, "nyc",
         sizeof(settings->arrays.netplay_mitm_server));

   hook_install();
   open_menu();
   netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_SERVER, NULL);

   started = cpu_features_get_time_usec();
   command_event(CMD_EVENT_NETPLAY_INIT, NULL);
   took    = cpu_features_get_time_usec() - started;

   CHECK(netplay_host_setup_pending(),
         "host setup was not deferred on the pending relay query");
   CHECK(took < 100000, "the command blocked for %lld us", (long long)took);

   before = presented;
   for (i = 0; i < 20000 && netplay_host_setup_pending(); i++)
   {
      pump(1);
      frames++;
      retro_sleep(1);
   }
   CHECK(!netplay_host_setup_pending(), "host setup never ran");
   CHECK(presented - before >= 1,
         "no frame presented while host setup waited (%u frames)", frames);

   command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
   netplay_driver_ctl(RARCH_NETPLAY_CTL_DISABLE, NULL);
   configuration_set_bool(settings, settings->bools.netplay_use_mitm_server, false);
   pump(1);

   if (failures == had)
      fprintf(stderr, "[pass] host-setup-deferred lane (%u frames, command %lld us)\n",
            frames, (long long)took);
}
#endif

/* ------------------------------------------------------------------ */
/* Lane: acceptance - no task or list work holds a frame              */
/* ------------------------------------------------------------------ */

/* The UI non-blocking plan's acceptance test: with Threaded Tasks off,
 * a run through menu navigation, playlist browsing, overlay switching,
 * the Explore index and a content load produces no handler that holds
 * the frame thread past a frame's budget.  The watchdog is the queue's
 * own slow-handler callback, the one debug builds register; a content
 * load's stages that run the core's own load and the driver rebuild
 * are not counted (task_content_is_load_stage).  Timings mean nothing
 * under a sanitizer, so there the paths run but only report. */

#define ACCEPT_BUDGET_USEC 16000
#define ACCEPT_ENTRIES     10000

static unsigned     accept_over;
static retro_time_t accept_worst;
static char         accept_worst_title[128];

static void accept_slow_cb(retro_task_t *task, retro_time_t usec)
{
   if (task_content_is_load_stage(task))
      return;
   accept_over++;
   if (usec > accept_worst)
   {
      accept_worst = usec;
      strlcpy(accept_worst_title, task->title ? task->title : "(untitled)",
            sizeof(accept_worst_title));
   }
}

/* Frames as the main loop runs them, with the watchdog kept armed:
 * a debug build's configure re-registers its own on every load. */
static void accept_pump(unsigned n)
{
   unsigned i;
   for (i = 0; i < n; i++)
   {
      task_queue_set_slow_handler_cb(accept_slow_cb, ACCEPT_BUDGET_USEC);
      runloop_iterate();
      task_queue_check();
   }
}

static bool accept_task_any(retro_task_t *task, void *user_data)
{
   (void)task; (void)user_data;
   return true;
}

/* Frames until the queue holds nothing, bounded. */
static unsigned accept_settle(unsigned cap)
{
   task_finder_data_t find;
   unsigned n = 0;
   find.func     = accept_task_any;
   find.userdata = NULL;
   do
   {
      accept_pump(1);
      n++;
   } while (n < cap && (task_queue_find(&find)
            || playlist_init_cached_pending()
            || runloop_is_content_switching()));
   return n;
}

static file_list_t *accept_selection_buf(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t *menu_list     = menu_st->entries.list;
   return menu_list ? MENU_LIST_GET_SELECTION(menu_list, 0) : NULL;
}

/* Builds a screen the way opening it does. */
static size_t accept_open_screen(enum menu_displaylist_ctl_state type,
      const char *label, enum msg_hash_enums label_enum, const char *path)
{
   menu_displaylist_info_t info;
   struct menu_state *menu_st = menu_state_get_ptr();
   file_list_t *buf           = accept_selection_buf();
   file_list_t *menu_stack    = MENU_LIST_GET(menu_st->entries.list, 0);

   if (!buf || !menu_stack)
      return 0;
   menu_entries_clear(buf);
   menu_entries_append(menu_stack, path, label, label_enum,
         MENU_SETTING_ACTION, 0, 0, NULL);
   menu_displaylist_info_init(&info);
   info.list          = buf;
   info.path          = strdup(path);
   info.label         = strdup(label);
   info.enum_idx      = label_enum;
   info.type          = MENU_SETTING_ACTION;
   info.directory_ptr = 0;
   menu_displaylist_ctl(type, &info, config_get_ptr());
   menu_displaylist_process(&info);
   menu_displaylist_info_free(&info);
   return buf->size;
}

/* OK on the entry whose path contains @needle, as a tap does. */
static bool accept_press_ok(const char *needle)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   file_list_t *buf           = accept_selection_buf();
   size_t i;
   if (!buf)
      return false;
   for (i = 0; i < buf->size; i++)
   {
      const char *p = buf->list[i].path;
      if (p && strstr(p, needle))
      {
         menu_entry_t entry;
         menu_st->selection_ptr = i;
         MENU_ENTRY_INITIALIZE(entry);
         entry.flags |= MENU_ENTRY_FLAG_PATH_ENABLED
                      | MENU_ENTRY_FLAG_LABEL_ENABLED
                      | MENU_ENTRY_FLAG_RICH_LABEL_ENABLED
                      | MENU_ENTRY_FLAG_VALUE_ENABLED
                      | MENU_ENTRY_FLAG_SUBLABEL_ENABLED;
         menu_entry_get(&entry, 0, i, NULL, true);
         menu_entry_action(&entry, i, MENU_ACTION_OK);
         return true;
      }
   }
   return false;
}

/* A little msgpack, enough to write a libretrodb file. */
typedef struct { uint8_t *d; size_t len, cap; } accept_buf_t;

static void accept_put(accept_buf_t *b, const void *p, size_t n)
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
static void accept_byte(accept_buf_t *b, uint8_t v) { accept_put(b, &v, 1); }
static void accept_str(accept_buf_t *b, const char *s)
{
   size_t n = strlen(s);
   if (n < 32)
      accept_byte(b, (uint8_t)(0xa0 | n));
   else
   {
      accept_byte(b, 0xd9);
      accept_byte(b, (uint8_t)n);
   }
   accept_put(b, s, n);
}
static void accept_uint(accept_buf_t *b, unsigned v)
{
   accept_byte(b, 0xcd);
   accept_byte(b, (uint8_t)(v >> 8));
   accept_byte(b, (uint8_t)v);
}

static uint32_t accept_crc(unsigned sys, unsigned g)
{
   return 0x100000u * (sys + 1) + g * 7u + 1u;
}

/* One database of ACCEPT_ENTRIES records that the playlist of the
 * same name matches by crc, so the Explore index has every entry to
 * categorise. */
static bool accept_write_db(const char *path, unsigned sys)
{
   static const char *genres[] = { "Action", "Puzzle", "Racing" };
   accept_buf_t body, meta;
   uint8_t hdr[16];
   uint64_t off;
   unsigned g;
   int i;
   FILE *f;

   memset(&body, 0, sizeof(body));
   memset(&meta, 0, sizeof(meta));
   for (g = 0; g < ACCEPT_ENTRIES; g++)
   {
      char name[64];
      uint32_t crc = accept_crc(sys, g);
      uint8_t c[4];
      snprintf(name, sizeof(name), "Accept S%u N%05u", sys, g);
      c[0] = (uint8_t)(crc >> 24); c[1] = (uint8_t)(crc >> 16);
      c[2] = (uint8_t)(crc >> 8);  c[3] = (uint8_t)crc;
      accept_byte(&body, 0x85);
      accept_str(&body, "name");        accept_str(&body, name);
      accept_str(&body, "crc");
      accept_byte(&body, 0xc4); accept_byte(&body, 4); accept_put(&body, c, 4);
      accept_str(&body, "developer");   accept_str(&body, genres[(g / 3) % 3]);
      accept_str(&body, "genre");       accept_str(&body, genres[g % 3]);
      accept_str(&body, "releaseyear"); accept_uint(&body, 1985 + (g % 12));
   }
   accept_byte(&body, 0xc0);
   accept_byte(&meta, 0x81);
   accept_str(&meta, "count");
   accept_uint(&meta, ACCEPT_ENTRIES);

   off = 16 + (uint64_t)body.len;
   memcpy(hdr, "RARCHDB", 7);
   hdr[7] = 0;
   for (i = 0; i < 8; i++)
      hdr[8 + i] = (uint8_t)(off >> (56 - 8 * i));
   if ((f = fopen(path, "wb")))
   {
      fwrite(hdr, 1, sizeof(hdr), f);
      fwrite(body.d, 1, body.len, f);
      fwrite(meta.d, 1, meta.len, f);
      fclose(f);
   }
   free(body.d);
   free(meta.d);
   return f != NULL;
}

static bool accept_write_playlist(const char *path, unsigned sys,
      const char *db)
{
   unsigned g;
   FILE *f = fopen(path, "wb");
   if (!f)
      return false;
   fprintf(f, "{\n  \"version\": \"1.5\",\n  \"items\": [\n");
   for (g = 0; g < ACCEPT_ENTRIES; g++)
      fprintf(f,
            "    { \"path\": \"/nowhere/s%u/g%05u.bin\", \"label\": \"Accept S%u N%05u\","
            " \"core_path\": \"DETECT\", \"core_name\": \"DETECT\","
            " \"crc32\": \"%08X|crc\", \"db_name\": \"%s\" }%s\n",
            sys, g, sys, g, (unsigned)accept_crc(sys, g), db,
            (g + 1 < ACCEPT_ENTRIES) ? "," : "");
   fprintf(f, "  ]\n}\n");
   fclose(f);
   return true;
}

static bool accept_write_overlay(const char *dir, char *cfg, size_t len)
{
   unsigned o, d;
   char img[600];
   FILE *f;
   snprintf(img, sizeof(img), "%s/img.png", dir);
   if (!(f = fopen(img, "wb")))
      return false;
   fputc(0, f);
   fclose(f);
   snprintf(cfg, len, "%s/accept.cfg", dir);
   if (!(f = fopen(cfg, "wb")))
      return false;
   fprintf(f, "overlays = 3\n");
   for (o = 0; o < 3; o++)
   {
      fprintf(f, "overlay%u_name = ol%u\n", o, o);
      fprintf(f, "overlay%u_full_screen = true\n", o);
      fprintf(f, "overlay%u_rect = \"0.0,0.0,1.0,1.0\"\n", o);
      fprintf(f, "overlay%u_overlay = img.png\n", o);
      fprintf(f, "overlay%u_descs = 40\n", o);
      for (d = 0; d < 40; d++)
      {
         fprintf(f, "overlay%u_desc%u = \"a,0.5,0.5,rect,0.1,0.1\"\n", o, d);
         fprintf(f, "overlay%u_desc%u_overlay = img.png\n", o, d);
      }
   }
   fclose(f);
   return true;
}

static void lane_acceptance(const char *dir)
{
   settings_t *settings = config_get_ptr();
   unsigned had = failures;
   char pl_dir[512], db_dir[512], path[640], ol_cfg[600];
   unsigned s, frames = 0;
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
   bool timed = false;
#else
   bool timed = true;
#endif

   snprintf(pl_dir, sizeof(pl_dir), "%s/accept_playlists", dir);
   snprintf(db_dir, sizeof(db_dir), "%s/accept_rdb", dir);
   path_mkdir(pl_dir);
   path_mkdir(db_dir);
   for (s = 0; s < 2; s++)
   {
      char db[64];
      snprintf(db, sizeof(db), "Accept System %u.lpl", s);
      snprintf(path, sizeof(path), "%s/%s", pl_dir, db);
      CHECK(accept_write_playlist(path, s, db), "fixture playlist not written");
      snprintf(path, sizeof(path), "%s/Accept System %u.rdb", db_dir, s);
      CHECK(accept_write_db(path, s), "fixture database not written");
   }
   CHECK(accept_write_overlay(pl_dir, ol_cfg, sizeof(ol_cfg)),
         "fixture overlay not written");
   configuration_set_bool(settings, settings->bools.threaded_data_runloop_enable, false);
   task_queue_unset_threaded();
   strlcpy(settings->paths.directory_playlist, pl_dir,
         sizeof(settings->paths.directory_playlist));

   accept_over  = 0;
   accept_worst = 0;
   accept_worst_title[0] = '\0';
   open_menu();
   frames += accept_settle(200);

   /* Menu navigation: the main menu, every setting, the history. */
   accept_open_screen(DISPLAYLIST_MAIN_MENU,
         MENU_ENUM_LABEL_MAIN_MENU_STR, MENU_ENUM_LABEL_MAIN_MENU, "");
   frames += accept_settle(200);
   accept_open_screen(DISPLAYLIST_SETTINGS_ALL,
         MENU_ENUM_LABEL_SETTINGS_STR, MENU_ENUM_LABEL_SETTINGS, "");
   frames += accept_settle(200);
   accept_open_screen(DISPLAYLIST_HISTORY,
         MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY_STR,
         MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY, "");
   frames += accept_settle(200);

   /* Playlist browsing: the Playlists screen, then each playlist. */
   for (s = 0; s < 2; s++)
   {
      char leaf[32];
      snprintf(leaf, sizeof(leaf), "Accept System %u.lpl", s);
      CHECK(accept_open_screen(DISPLAYLIST_DATABASE_PLAYLISTS,
            MENU_ENUM_LABEL_PLAYLISTS_TAB_STR, MENU_ENUM_LABEL_PLAYLISTS_TAB,
            pl_dir) > 0, "the Playlists screen was empty");
      CHECK(accept_press_ok(leaf), "no %s to open", leaf);
      frames += accept_settle(2000);
      CHECK(accept_selection_buf() && accept_selection_buf()->size > 1,
            "playlist %s did not list its entries", leaf);
   }

#ifdef HAVE_OVERLAY
   /* Overlay switching: load a pack, then step through it. */
   strlcpy(settings->paths.path_overlay, ol_cfg,
         sizeof(settings->paths.path_overlay));
   configuration_set_bool(settings, settings->bools.input_overlay_enable, true);
   command_event(CMD_EVENT_OVERLAY_INIT, NULL);
   frames += accept_settle(2000);
   for (s = 0; s < 3; s++)
   {
      command_event(CMD_EVENT_OVERLAY_NEXT, NULL);
      frames += accept_settle(200);
   }
   configuration_set_bool(settings, settings->bools.input_overlay_enable, false);
   command_event(CMD_EVENT_OVERLAY_UNLOAD, NULL);
   frames += accept_settle(200);
#endif

#ifdef HAVE_LIBRETRODB
   /* The Explore index over the same playlists. */
   menu_explore_free();
   CHECK(task_push_menu_explore_init(pl_dir, db_dir), "explore not pushed");
   frames += accept_settle(4000);
   CHECK(menu_explore_state_entry_count(NULL) == 2 * ACCEPT_ENTRIES,
         "the Explore index holds %u entries, not %u",
         (unsigned)menu_explore_state_entry_count(NULL), 2 * ACCEPT_ENTRIES);
   menu_explore_free();
#endif

   /* A content load. */
   CHECK(task_push_load_contentless_core_from_menu(core_path),
         "the load was not started");
   frames += accept_settle(200);
   CHECK(core_is_up(), "the load did not go through");

   task_queue_set_slow_handler_cb(NULL, 0);
   for (s = 0; s < 2; s++)
   {
      snprintf(path, sizeof(path), "%s/Accept System %u.lpl", pl_dir, s);
      remove(path);
      snprintf(path, sizeof(path), "%s/Accept System %u.rdb", db_dir, s);
      remove(path);
   }
   remove(ol_cfg);
   snprintf(path, sizeof(path), "%s/img.png", pl_dir);
   remove(path);
   rmdir(pl_dir);
   rmdir(db_dir);

   if (timed)
      CHECK(accept_over == 0,
            "%u handler call(s) held the frame thread past %d ms; worst %lld us "
            "(%s)", accept_over, ACCEPT_BUDGET_USEC / 1000,
            (long long)accept_worst, accept_worst_title);
   if (failures == had)
      fprintf(stderr, "[%s] acceptance lane (%u frames, %u over budget%s%s)\n",
            timed ? "pass" : "report", frames, accept_over,
            accept_over ? ", worst: " : "", accept_over ? accept_worst_title : "");
}

/* ------------------------------------------------------------------ */
/* Lane: replay reply                                                  */
/* ------------------------------------------------------------------ */

#ifdef HAVE_BSV_MOVIE
/* A command interface of the test's own: which sender a reply is for
 * is 'source' at the time of the reply, as with the network
 * interface's last datagram source. */
static int      rr_source;
static unsigned rr_replies;
static int      rr_reply_to;          /* the sender the reply went to */
static char     rr_reply[128];
static bool     rr_in_command;        /* inside command_play_replay_slot */
static unsigned rr_in_command_replies;

static void rr_record(int to, const char *s, size_t len)
{
   rr_replies++;
   if (rr_in_command)
      rr_in_command_replies++;
   rr_reply_to = to;
   if (len >= sizeof(rr_reply))
      len = sizeof(rr_reply) - 1;
   memcpy(rr_reply, s, len);
   rr_reply[len] = '\0';
}

static void rr_replier(command_t *cmd, const char *s, size_t len)
{
   (void)cmd;
   rr_record(rr_source, s, len);
}

static void *rr_reply_dest(command_t *cmd)
{
   int *dest = (int*)malloc(sizeof(int));
   (void)cmd;
   if (dest)
      *dest = rr_source;
   return dest;
}

static void rr_reply_to_cb(command_t *cmd, void *dest, const char *s, size_t len)
{
   (void)cmd;
   rr_record(*(int*)dest, s, len);
}

static bool rr_play(command_t *cmd, const char *slot)
{
   bool ok;
   rr_in_command = true;
   ok            = command_play_replay_slot(cmd, slot);
   rr_in_command = false;
   return ok;
}

/* PLAY_REPLAY_SLOT answers with the replay's handle, which the movie
 * task installs from its callback.  The command returns at once and
 * the reply follows from the frame loop, to the sender that asked;
 * the frame is not held while the task runs.  With the interface torn
 * down in between, the reply is dropped and the next command is
 * served. */
static void lane_replay_reply(const char *dir)
{
   runloop_state_t      *runloop_st = runloop_state_get_ptr();
   input_driver_state_t *input_st   = input_state_get_ptr();
   settings_t           *settings   = config_get_ptr();
   command_t cmd;
   char slot[16], path[600];
   unsigned had = failures, before, n;

   if (!core_is_up())
   {
      CHECK(task_push_load_contentless_core_from_menu(core_path),
            "the load was not started");
      pump(200);
   }
   CHECK(core_is_up(), "no core to record a replay against");
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   pump(2);

   snprintf(runloop_st->name.replay, sizeof(runloop_st->name.replay),
         "%s/harness.replay", dir);
   configuration_set_bool(settings, settings->bools.replay_auto_index, false);
   configuration_set_int(settings, settings->ints.replay_slot, 3);
   CHECK(command_event(CMD_EVENT_RECORD_REPLAY, NULL), "recording did not start");
   pump(30);
   command_event(CMD_EVENT_HALT_REPLAY, NULL);
   pump(2);
   runloop_get_replay_path(path, sizeof(path), 3);
   CHECK(path_is_valid(path), "no replay was written to %s", path);

   memset(&cmd, 0, sizeof(cmd));
   cmd.replier    = rr_replier;
   cmd.reply_dest = rr_reply_dest;
   cmd.reply_to   = rr_reply_to_cb;
   snprintf(slot, sizeof(slot), "%d", 3);

   rr_replies = rr_in_command_replies = 0;
   rr_source  = 1;
   CHECK(rr_play(&cmd, slot), "PLAY_REPLAY_SLOT did not start playback");
   CHECK(rr_in_command_replies == 0,
         "PLAY_REPLAY_SLOT answered before returning: it held the frame "
         "until the movie task was through");
   CHECK(movie_playback_start_in_progress(NULL),
         "the playback start was not left to the task");
   /* Another sender's datagram arrives meanwhile. */
   rr_source = 2;
   before    = (unsigned)video_state_get_ptr()->frame_count;
   for (n = 0; n < 200 && !rr_replies; n++)
      pump(1);
   CHECK(rr_replies == 1, "%u replies, not 1", rr_replies);
   CHECK((unsigned)video_state_get_ptr()->frame_count > before,
         "no frame was drawn while the reply was owed");
   CHECK(rr_reply_to == 1, "the reply went to sender %d, not the one that asked",
         rr_reply_to);
   CHECK(strncmp(rr_reply, "PLAY_REPLAY_SLOT ", 17) == 0
         && strcmp(rr_reply, "PLAY_REPLAY_SLOT 0") != 0,
         "reply \"%s\" does not carry the replay handle", rr_reply);
   pump(5);
   CHECK(rr_replies == 1, "the reply was sent again");
   command_event(CMD_EVENT_HALT_REPLAY, NULL);
   pump(2);

   /* The interfaces torn down while the reply is owed. */
   rr_replies = rr_in_command_replies = 0;
   rr_source  = 1;
   CHECK(rr_play(&cmd, slot), "the second PLAY_REPLAY_SLOT did not start");
   input_driver_deinit_command(input_st);
   pump(20);
   CHECK(rr_replies == 0, "a reply went to a torn-down interface");
   command_event(CMD_EVENT_HALT_REPLAY, NULL);
   pump(2);
   CHECK(rr_play(&cmd, slot), "a dropped reply left the next command refused");
   pump(20);
   CHECK(rr_replies == 1, "the command after a dropped reply was not answered");
   command_event(CMD_EVENT_HALT_REPLAY, NULL);
   pump(2);

   remove(path);
   runloop_st->name.replay[0] = '\0';
   if (failures == had)
      fprintf(stderr, "[pass] replay-reply lane (answered after %u frames)\n", n);
}
#endif

/* ------------------------------------------------------------------ */
/* Lane: screenshot steps                                              */
/* ------------------------------------------------------------------ */

#if defined(HAVE_SCREENSHOTS) && defined(HAVE_RPNG)
/* A task ahead of the screenshot that spends the shared per-frame I/O
 * window on every check, so the screenshot's handler gets only the
 * window's floor: one row a check, on any machine.  The window resets
 * by the clock; waiting out a whole period first makes this open the
 * one that starts it, so the screenshot opens well inside the same
 * window, not across a reset that would hand it a fresh one. */
static bool ss_hog_stop;
static void ss_hog_handler(retro_task_t *task)
{
   nbio_budget_t b;
   if (ss_hog_stop)
   {
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
      return;
   }
   retro_sleep(17);
   task_nbio_slice_open(&b);
   while (task_nbio_slice_within_budget(&b, 0, 0)) { }
   task_nbio_slice_close(&b);
}

static bool ss_is_screenshot(retro_task_t *task, void *user_data)
{
   if (task->type != TASK_TYPE_BLOCKING)
      return false;
   if (user_data)
      *(retro_task_t**)user_data = task;
   return true;
}

static bool ss_in_flight(void)
{
   task_finder_data_t find;
   find.func     = ss_is_screenshot;
   find.userdata = NULL;
   return task_queue_find(&find);
}

static bool ss_png_complete(const char *path)
{
   static const uint8_t magic[8] = { 0x89, 'P', 'N', 'G', 13, 10, 26, 10 };
   uint8_t head[8], tail[8];
   bool ok = false;
   FILE *f = fopen(path, "rb");
   if (!f)
      return false;
   if (     fread(head, 1, 8, f) == 8
         && fseek(f, -8, SEEK_END) == 0
         && fread(tail, 1, 8, f) == 8)
      ok = !memcmp(head, magic, 8) && !memcmp(tail, "IEND", 4);
   fclose(f);
   return ok;
}

/* With Threaded Tasks off a screenshot is encoded a slice at a time
 * from the frame loop, not in one handler call that holds the frame
 * for the whole deflate; the file it leaves is whole.  Cancelled part
 * way, it leaves no file. */
static void lane_screenshot_steps(const char *dir)
{
   settings_t *settings = config_get_ptr();
   retro_task_t *hog;
   char path[600], file[640];
   unsigned had = failures, checks = 0, rows = 240;

   if (!core_is_up())
   {
      CHECK(task_push_load_contentless_core_from_menu(core_path),
            "the load was not started");
      pump(200);
   }
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   configuration_set_bool(settings, settings->bools.threaded_data_runloop_enable, false);
   task_queue_unset_threaded();
   pump(3);

   ss_hog_stop = false;
   hog = task_init();
   hog->handler = ss_hog_handler;
   hog->flags  |= RETRO_TASK_FLG_MUTE;
   task_queue_push(hog);

   /* With fullpath the name is the file, as given */
   snprintf(path, sizeof(path), "%s/harness_shot.png", dir);
   snprintf(file, sizeof(file), "%s", path);
   remove(file);
   CHECK(take_screenshot(dir, path, false, false, true, true),
         "the screenshot was not started");
   while (ss_in_flight() && checks < 4 * rows)
   {
      task_queue_check();
      checks++;
   }
   CHECK(!ss_in_flight(), "the screenshot did not finish in %u checks", checks);
   CHECK(checks >= rows,
         "the screenshot finished in %u checks for %u rows: encoded in one go",
         checks, rows);
   CHECK(ss_png_complete(file), "the screenshot file is not a whole PNG");
   remove(file);

   /* Cancelled part way through */
   CHECK(take_screenshot(dir, path, false, false, true, true),
         "the second screenshot was not started");
   {
      unsigned n;
      for (n = 0; n < 20; n++)
         task_queue_check();
   }
   CHECK(ss_in_flight(), "the second screenshot was already done");
   {
      retro_task_t *shot = NULL;
      task_finder_data_t find;
      find.func     = ss_is_screenshot;
      find.userdata = &shot;
      if (task_queue_find(&find) && shot)
         task_queue_cancel_task(shot);
   }
   {
      unsigned n;
      for (n = 0; n < 20 && ss_in_flight(); n++)
         task_queue_check();
   }
   CHECK(!ss_in_flight(), "the cancelled screenshot did not retire");
   CHECK(!path_is_valid(file), "a cancelled screenshot left a partial file");

   ss_hog_stop = true;
   pump(3);

   if (failures == had)
      fprintf(stderr, "[pass] screenshot-steps lane (%u checks for %u rows)\n",
            checks, rows);
}
#endif

/* ------------------------------------------------------------------ */
/* Lane: fallback, threaded                                            */
/* ------------------------------------------------------------------ */

/* A main-thread task that counts the checks it is run in.  The queue
 * runs it once per task_queue_check(); more means a handler re-entered
 * the queue from inside a check. */
static unsigned probe_runs;
static bool     probe_stop;
static void probe_main_handler(retro_task_t *task)
{
   probe_runs++;
   if (probe_stop)
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

/* A load that fails falls back to the dummy core, and the core stage
 * deinitialises the old core as it goes - with the close already
 * through, nothing is left for that to wait on.  With Threaded Tasks
 * on, the stage runs from inside the check: a wait there would gather
 * the queue again from within it, running the main-thread tasks a
 * second time in the same check. */
#if defined(HAVE_RUNAHEAD) && defined(HAVE_DYNAMIC) && defined(HAVE_THREADS)
/* Run-ahead's second instance on the threaded queue: the task copies
 * the core and opens the copy on the worker, and the main thread
 * takes the open handle.  The instance is the copy's own - its
 * statics are not the running core's - and comes up once.  A software
 * core, so the second instance is a whole one. */
static void lane_secondary_threaded(void)
{
   settings_t *settings        = config_get_ptr();
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   enum runahead_copy_status st = RUNAHEAD_COPY_PENDING;
   unsigned (*sec_inits)(void)  = NULL;
   unsigned (*sec_keys)(void)   = NULL;
   unsigned primary_keys_before, sec_keys_before;
   const char *slash            = strrchr(core_path, '/');
   char nohw[600];
   unsigned primary_inits;
   unsigned had = failures, n;

   snprintf(nohw, sizeof(nohw), "%.*s/harness_core_nohw.so",
         slash ? (int)(slash - core_path) : 1, slash ? core_path : ".");
   CHECK(path_is_valid(nohw), "no %s: build.sh builds it", nohw);
   configuration_set_bool(settings, settings->bools.threaded_data_runloop_enable, true);
   task_queue_set_threaded();
   open_menu();
   CHECK(task_push_load_contentless_core_from_menu(nohw),
         "the load was not started");
   pump(LOAD_FRAMES);
   CHECK(core_is_up(), "the core did not come up");
   primary_inits = core_inits();

   for (n = 0; n < LOAD_FRAMES * 4 && st == RUNAHEAD_COPY_PENDING; n++)
   {
      st = secondary_core_ensure_exists(runloop_st, settings);
      task_queue_check();
      retro_sleep(1);
   }
   CHECK(st == RUNAHEAD_COPY_READY, "the second instance was not created (%d)",
         (int)st);
   CHECK(runloop_st->secondary_lib_handle != NULL,
         "no handle for the second instance");
   if (runloop_st->secondary_lib_handle)
      sec_inits = (unsigned (*)(void))dylib_proc(
            runloop_st->secondary_lib_handle, "harness_core_inits");
   CHECK(sec_inits && sec_inits() == 1,
         "the second instance came up %u times", sec_inits ? sec_inits() : 0);
   CHECK(core_inits() == primary_inits,
         "bringing up the second instance re-ran the running core's init");
   CHECK(runloop_st->secondary_lib_handle != runloop_st->lib_handle,
         "the second instance shares the running core's handle");

   /* A key event bound for the core reaches both instances */
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   if (runloop_st->secondary_lib_handle)
      sec_keys = (unsigned (*)(void))dylib_proc(
            runloop_st->secondary_lib_handle, "harness_core_key_events");
   primary_keys_before = core_export("harness_core_key_events");
   sec_keys_before     = sec_keys ? sec_keys() : 0;
   input_keyboard_event(true, RETROK_KP7, 0, 0, RETRO_DEVICE_KEYBOARD);
   CHECK(core_export("harness_core_key_events") == primary_keys_before + 1,
         "the running core did not get the key event");
   CHECK(sec_keys && sec_keys() == sec_keys_before + 1,
         "the second instance did not get the key event");

   runahead_secondary_core_destroy(runloop_st);
   CHECK(!runloop_st->secondary_key_event,
         "the closed second instance's keyboard callback is still held");
   configuration_set_bool(settings, settings->bools.threaded_data_runloop_enable, false);
   task_queue_unset_threaded();
   pump(2);

   if (failures == had)
      fprintf(stderr, "[pass] secondary-threaded lane (%u polls)\n", n);
}
#endif

#if defined(HAVE_RUNAHEAD) && defined(HAVE_DYNAMIC) && defined(HAVE_THREADS)
/* A second instance of a hardware core: what the frontend holds a
 * pointer to - the hardware context's callbacks, the controller
 * description - stays the running core's, and closing the second
 * instance leaves nothing the next close of the running core would
 * call into. */
static void lane_secondary_keeps_callbacks(void)
{
   typedef retro_hw_context_reset_t (*hw_fn_t)(void);
   typedef const struct retro_controller_description *(*types_fn_t)(void);
   settings_t *settings          = config_get_ptr();
   runloop_state_t *runloop_st   = runloop_state_get_ptr();
   enum runahead_copy_status st  = RUNAHEAD_COPY_PENDING;
   struct retro_hw_render_callback *hwr;
   hw_fn_t    primary_hw    = NULL;
   types_fn_t primary_types = NULL;
   unsigned had = failures, n;

   configuration_set_bool(settings, settings->bools.threaded_data_runloop_enable, true);
   task_queue_set_threaded();
   open_menu();
   CHECK(task_push_load_contentless_core_from_menu(core_path),
         "the load was not started");
   pump(LOAD_FRAMES);
   CHECK(core_is_up(), "the core did not come up");
   if (runloop_st->lib_handle)
   {
      primary_hw    = (hw_fn_t)dylib_proc(runloop_st->lib_handle,
            "harness_core_hw_destroy_fn");
      primary_types = (types_fn_t)dylib_proc(runloop_st->lib_handle,
            "harness_core_port_types");
   }
   CHECK(primary_hw && primary_types, "the harness core exports are missing");

   for (n = 0; n < LOAD_FRAMES * 4 && st == RUNAHEAD_COPY_PENDING; n++)
   {
      st = secondary_core_ensure_exists(runloop_st, settings);
      task_queue_check();
      retro_sleep(1);
   }
   CHECK(st == RUNAHEAD_COPY_READY, "the second instance was not created (%d)",
         (int)st);

   hwr = video_driver_get_hw_context();
   CHECK(primary_hw && hwr && hwr->context_destroy == primary_hw(),
         "the hardware context's callbacks are the second instance's");
   CHECK(primary_types
         && runloop_st->system.ports.size > 0
         && runloop_st->system.ports.data[0].types == primary_types(),
         "the controller description is the second instance's");

   runahead_secondary_core_destroy(runloop_st);
   CHECK(task_push_load_contentless_core_from_menu(core_path),
         "the reload after the second instance was not started");
   pump(LOAD_FRAMES);
   CHECK(core_is_up(), "the reload after the second instance did not go through");

   configuration_set_bool(settings, settings->bools.threaded_data_runloop_enable, false);
   task_queue_unset_threaded();
   pump(2);

   if (failures == had)
      fprintf(stderr, "[pass] secondary-keeps-callbacks lane\n");
}
#endif

static void lane_fallback_threaded(void)
{
   settings_t *settings = config_get_ptr();
   char bogus[512];
   retro_task_t *probe;
   unsigned had = failures, checks = 0, n;

   snprintf(bogus, sizeof(bogus), "%.500s.missing", core_path);
   configuration_set_bool(settings, settings->bools.threaded_data_runloop_enable, true);
   task_queue_set_threaded();
   open_menu();

   probe_runs = 0;
   probe_stop = false;
   probe = task_init();
   probe->handler = probe_main_handler;
   probe->flags  |= RETRO_TASK_FLG_MAIN_THREAD | RETRO_TASK_FLG_MUTE;
   task_queue_push(probe);

   CHECK(task_push_load_contentless_core_from_menu(bogus),
         "the load of a missing core was not started");
   for (n = 0; n < LOAD_FRAMES && runloop_is_content_switching(); n++)
   {
      runloop_iterate();
      task_queue_check();
      checks++;
   }
   CHECK(!runloop_is_content_switching(), "the load did not finish");
   CHECK(runloop_state_get_ptr()->current_core_type == CORE_TYPE_DUMMY,
         "the fallback is not the dummy core");
   CHECK(probe_runs <= checks,
         "main-thread tasks ran %u times in %u checks: the load re-entered "
         "the queue from inside a check", probe_runs, checks);

   probe_stop = true;
   pump(3);
   CHECK(task_push_load_contentless_core_from_menu(core_path),
         "the reload after the fallback was not started");
   pump(LOAD_FRAMES);
   CHECK(core_is_up(), "the reload after the fallback did not go through");
   configuration_set_bool(settings, settings->bools.threaded_data_runloop_enable, false);
   task_queue_unset_threaded();
   pump(2);

   if (failures == had)
      fprintf(stderr, "[pass] fallback-threaded lane (%u checks)\n", checks);
}

/* ------------------------------------------------------------------ */
/* Lane: not a core                                                    */
/* ------------------------------------------------------------------ */

/* Empties the message queue: true when a message on it held @want. */
static bool queued_message(const char *want)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   bool found                  = false;
   const char *m;
   while ((m = msg_queue_pull(&runloop_st->msg_queue)))
      if (strstr(m, want))
         found = true;
   return found;
}

/* A library that opens but is not a core - it lacks retro_init - is
 * found out only once the load has committed and the old session is
 * gone.  The load falls back to the dummy core: drivers come back, the
 * menu runs against them, and the failure is said.  Frames go on
 * being presented throughout. */
static void lane_not_a_core(void)
{
   struct load_frame log[LOAD_FRAMES];
   char noinit[600];
   const char *slash = strrchr(core_path, '/');
   unsigned n, i, presented_frames = 0;
   unsigned had          = failures;
   settings_t *settings = config_get_ptr();
   bool font_enable     = settings->bools.video_font_enable;

   snprintf(noinit, sizeof(noinit), "%.*s/harness_core_noinit.so",
         slash ? (int)(slash - core_path) : 1, slash ? core_path : ".");
   CHECK(path_is_valid(noinit), "no %s: build.sh builds it", noinit);

   open_menu();
   hook_install();
   queued_message("");   /* nothing left over from earlier lanes */
   /* Frames show queued messages on the OSD, taking them off the
    * queue; with it off they stay there to be read here. */
   settings->bools.video_font_enable = false;

   CHECK(task_push_load_contentless_core_from_menu(noinit),
         "the load of a library that is not a core was not started");
   n = run_load(log, LOAD_FRAMES);
   for (i = 0; i < n; i++)
      presented_frames += log[i].presented;
   CHECK(n < LOAD_FRAMES && !runloop_is_content_switching(),
         "the load did not finish within %u frames", LOAD_FRAMES);
   CHECK(queued_message(msg_hash_to_str(MSG_FAILED_TO_LOAD_CONTENT)),
         "the failure was not said");
   CHECK(video_state_get_ptr()->data != NULL,
         "no video driver after the failed load");
   CHECK(input_state_get_ptr()->current_data != NULL,
         "no input driver after the failed load");
   CHECK(runloop_state_get_ptr()->current_core_type == CORE_TYPE_DUMMY,
         "the failed load did not fall back to the dummy core");
   CHECK(presented_frames >= 1, "no frame presented during the fallback");
   settings->bools.video_font_enable = font_enable;
   runloop_iterate();
   task_queue_check();
   CHECK(menu_is_up(), "menu not up after a failed load");

   /* And the real core loads again. */
   CHECK(task_push_load_contentless_core_from_menu(core_path),
         "the reload after the failed load was not started");
   n = run_load(log, LOAD_FRAMES);
   CHECK(n < LOAD_FRAMES && core_is_up()
         && runloop_state_get_ptr()->current_core_type != CORE_TYPE_DUMMY,
         "the reload after the failed load did not go through");
   runloop_iterate();

   if (failures == had)
      fprintf(stderr, "[pass] not-a-core lane (%u presented)\n",
            presented_frames);
}

/* ------------------------------------------------------------------ */
/* Lane: close content                                                 */
/* ------------------------------------------------------------------ */

/* The Quick Menu's Close Content: the frame loop unloads the core,
 * which loads the dummy core - a staged load - and asks for the core
 * library to be loaded back afterwards, for Start Core.  That reload
 * must follow the dummy session, not land in the middle of it. */
static void lane_close_content(void)
{
   struct load_frame log[LOAD_FRAMES];
   unsigned n;
   unsigned had = failures;
   struct menu_state *menu_st = menu_state_get_ptr();
   runloop_state_t *runloop_st = runloop_state_get_ptr();

   open_menu();
   hook_install();
   path_set(RARCH_PATH_CORE_LAST, core_path);

   menu_st->flags |= MENU_ST_FLAG_PENDING_CLOSE_CONTENT;
   pump(1);
   CHECK(runloop_is_content_switching(),
         "closing content did not start a staged load of the dummy core");
   CHECK(menu_st->flags & MENU_ST_FLAG_PENDING_RELOAD_CORE,
         "no core reload pending after the close");

   n = run_load(log, LOAD_FRAMES);
   CHECK(n < LOAD_FRAMES && !runloop_is_content_switching(),
         "the close did not finish within %u frames", LOAD_FRAMES);
   CHECK(runloop_st->current_core_type == CORE_TYPE_DUMMY,
         "the dummy core is not running after the close");
   CHECK(menu_st->flags & MENU_ST_FLAG_PENDING_RELOAD_CORE,
         "the core library was reloaded into the session being built");
   CHECK(menu_is_up(), "menu not up after the close");

   /* The frames after: the menu's flush, then the reload into the
    * finished dummy session. */
   pump(2);
   CHECK(!(menu_st->flags & MENU_ST_FLAG_PENDING_RELOAD_CORE),
         "the core reload did not follow the close");
   CHECK(string_is_equal(runloop_st->system.info.library_name,
            "content_load_harness"),
         "the session does not carry the reloaded core (\"%s\")",
         runloop_st->system.info.library_name);
   CHECK(runloop_st->current_core_type == CORE_TYPE_DUMMY,
         "the reload started the core");

   /* Start Core, as the menu would, to leave a core running. */
   {
      content_ctx_info_t content_info = {0};
      CHECK(task_push_start_current_core(&content_info),
            "Start Core was not started");
      n = run_load(log, LOAD_FRAMES);
      CHECK(n < LOAD_FRAMES && core_is_up()
            && runloop_st->current_core_type != CORE_TYPE_DUMMY,
            "Start Core did not go through");
      runloop_iterate();
   }

   if (failures == had)
      fprintf(stderr, "[pass] close-content lane\n");
}

/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
   char cfg_path[512];
   char dir[400];
   /* NULL past the last argument, as a real main()'s argv is: the
    * option parser reads up to that. */
   char *rarch_argv[10] = {0};
   char state_path[512];
   int rarch_argc = 0;
   FILE *cfg;
   (void)argc;

   {
      const char *tmp = getenv("TMPDIR");
      if (!tmp || !*tmp)
         tmp = getenv("TEMP");
      if (!tmp || !*tmp)
         tmp = "/tmp";
      snprintf(dir, sizeof(dir), "%s/content_load_harness_%ld",
            tmp, (long)getpid());
   }
   if (!path_mkdir(dir))
      return 1;
   strlcpy(harness_dir, dir, sizeof(harness_dir));

   snprintf(cfg_path, sizeof(cfg_path), "%s/harness.cfg", dir);
   if ((cfg = fopen(cfg_path, "wb")))
   {
      fprintf(cfg, "video_driver = \"null\"\n");
      fprintf(cfg, "audio_driver = \"null\"\n");
      fprintf(cfg, "input_driver = \"null\"\n");
      fprintf(cfg, "input_joypad_driver = \"null\"\n");
      fprintf(cfg, "menu_driver = \"rgui\"\n");
      fprintf(cfg, "video_threaded = \"false\"\n");
      fprintf(cfg, "video_vsync = \"false\"\n");
      fprintf(cfg, "menu_pause_libretro = \"true\"\n");
      fprintf(cfg, "config_save_on_exit = \"false\"\n");
      fprintf(cfg, "savestate_auto_save = \"false\"\n");
      fprintf(cfg, "menu_show_load_content_animation = \"false\"\n");
      fprintf(cfg, "threaded_data_runloop_enable = \"false\"\n");
      /* The records of the cores asked live next to the info cache */
      fprintf(cfg, "libretro_info_path = \"%s\"\n", dir);
      /* Where a contentless core's states go, named after the core:
       * the close-waits-for-save lane needs a save that can be
       * written, on every load */
      fprintf(cfg, "savestate_directory = \"%s\"\n", dir);
      fclose(cfg);
   }

   if (getenv("HARNESS_VERBOSE"))
      verbosity_enable();

   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);
   runloop_msg_queue_init();   /* as rarch_main() does */

   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;
   /* The harness core, built next to this binary by build.sh. Either
    * separator: Windows argv[0] carries backslashes. */
   {
      const char *slash = strrchr(argv[0], '/');
#ifdef _WIN32
      const char *bslash = strrchr(argv[0], '\\');
      if (bslash && (!slash || bslash > slash))
         slash = bslash;
#endif
      {
         int dirlen = slash ? (int)(slash - argv[0]) : 1;
         snprintf(core_path, sizeof(core_path), "%.*s/harness_core.so",
               dirlen, slash ? argv[0] : ".");
      }
   }
   rarch_argv[rarch_argc++] = (char*)"-L";
   rarch_argv[rarch_argc++] = core_path;
   snprintf(state_path, sizeof(state_path), "%s/harness.state", dir);
   if (getenv("HARNESS_VERBOSE"))
      rarch_argv[rarch_argc++] = (char*)"-v";

   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }
   {
      unsigned i;
      for (i = 0; i < 5; i++)
         runloop_iterate();
   }
   CHECK(runloop_state_get_ptr()->current_core_type != CORE_TYPE_DUMMY,
         "harness core did not start (dummy core running)");

   lane_staged();
   lane_core_opened_once();
#if defined(HAVE_DYNAMIC)
   lane_probe_cached();
#endif
   lane_one_at_a_time();
#if defined(HAVE_DYNAMIC) && defined(HAVE_THREADS)
   lane_core_preloaded();
#endif
#ifdef HAVE_REWIND
   lane_rewind_after_load();
#endif
   lane_reinit_deferred();
   lane_fallback();
   lane_fallback_threaded();
#if defined(HAVE_RUNAHEAD) && defined(HAVE_DYNAMIC) && defined(HAVE_THREADS)
   lane_secondary_threaded();
   lane_secondary_keeps_callbacks();
#endif
   lane_not_a_core();
   lane_hw_request();
   lane_queue_survives();
   lane_close_content();
   lane_close_waits_for_save();
#ifdef HAVE_NETWORKING
   lane_host_setup_deferred();
#endif
   lane_acceptance(dir);
#ifdef HAVE_BSV_MOVIE
   lane_replay_reply(dir);
#endif
#if defined(HAVE_SCREENSHOTS) && defined(HAVE_RPNG)
   lane_screenshot_steps(dir);
#endif
#if defined(HAVE_DYNAMIC) && defined(HAVE_MENU)
   /* Last: a content load leaves its save state name behind for the
    * contentless loads after it */
   lane_prefetch_follows_new_core();
   lane_contentless_names();
#endif

   main_exit(NULL);

   remove(cfg_path);
   remove(state_path);
   {
      char leftover[600];
      snprintf(leftover, sizeof(leftover), "%s/core_probe.cache", dir);
      remove(leftover);
      snprintf(leftover, sizeof(leftover), "%s/core_info.cache", dir);
      remove(leftover);
      snprintf(leftover, sizeof(leftover), "%s.png", state_path);
      remove(leftover);
      /* the contentless core's states, flat or in its own folder */
      snprintf(leftover, sizeof(leftover),
            "%s/content_load_harness.state", dir);
      remove(leftover);
      snprintf(leftover, sizeof(leftover),
            "%s/content_load_harness.state.png", dir);
      remove(leftover);
      snprintf(leftover, sizeof(leftover),
            "%s/content_load_harness/content_load_harness.state", dir);
      remove(leftover);
      snprintf(leftover, sizeof(leftover),
            "%s/content_load_harness/content_load_harness.state.png", dir);
      remove(leftover);
      /* the per-core save folder a content load makes */
      snprintf(leftover, sizeof(leftover), "%s/content_load_harness", dir);
      rmdir(leftover);
   }
   rmdir(dir);

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   fprintf(stderr, "all lanes passed\n");
   return 0;
}

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
 *    with the drivers rebuilt and the menu up.
 *
 * Requires a completed non-Qt build:
 *
 *   ./configure --disable-qt && make
 *   samples/tasks/content_load/build.sh
 */

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
#include "../../../paths.h"
#include "../../../content.h"
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

   for (i = 0; i < 400 && runloop_is_content_switching(); i++)
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
      fclose(cfg);
   }

   if (getenv("HARNESS_VERBOSE"))
      verbosity_enable();

   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);

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
   /* A state path of its own: a contentless core derives none, and
    * the close-waits-for-save lane needs a save that can be written. */
   snprintf(state_path, sizeof(state_path), "%s/harness.state", dir);
   rarch_argv[rarch_argc++] = (char*)"-S";
   rarch_argv[rarch_argc++] = state_path;
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
   lane_one_at_a_time();
   lane_reinit_deferred();
   lane_fallback();
   lane_hw_request();
   lane_queue_survives();
   lane_close_content();
   lane_close_waits_for_save();
#ifdef HAVE_NETWORKING
   lane_host_setup_deferred();
#endif

   main_exit(NULL);

   remove(cfg_path);
   remove(state_path);
   rmdir(dir);

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   fprintf(stderr, "all lanes passed\n");
   return 0;
}

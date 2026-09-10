/* Harness for threaded video, driving the real frame loop.
 *
 * The thread wrapper is installed and torn down by CMD_EVENT_REINIT,
 * which the runtime toggle in the menu fires through the setting's
 * write handler with a core loaded.  Every crash report on this path
 * has the same shape: a runloop-thread reader (presentable, alive,
 * focus, viewport) or a teardown barrier reaching the wrapper while
 * it is being swapped for the plain driver, or the other way round.
 * None of that is visible from a unit test of video_thread_wrapper.c
 * alone, because the seam is between the wrapper and the runloop.
 *
 * So this links the shipping objects with only main() replaced, boots
 * the frontend on the null drivers with the menu up, installs a
 * retro_run that hands frames to the frontend the way a core does,
 * and then does what the user does: toggle threaded video through
 * the real setting handler, leave the menu, run frames, come back,
 * toggle again - in a loop, under whatever sanitizer the objects
 * were built with.
 *
 * Nothing is stubbed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>

#include "../../../runloop.h"
#include "../../../retroarch.h"
#include "../../../configuration.h"
#include "../../../command.h"
#include "../../../driver.h"
#include "../../../frontend/frontend_driver.h"
#include "../../../frontend/frontend.h"
#include "../../../gfx/video_driver.h"
#include "../../../gfx/video_thread_wrapper.h"
#include "../../../gfx/font_driver.h"
#include "../../../menu/menu_driver.h"
#include "../../../menu/menu_setting.h"
#include "../../../verbosity.h"

#include <time/rtime.h>
#include <retro_timers.h>
#include <features/features_cpu.h>
#include <rthreads/rthreads.h>
#include <file/config_file.h>

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

/* ------------------------------------------------------------------ */

static void run_frames(unsigned n)
{
   unsigned i;
   for (i = 0; i < n; i++)
      runloop_iterate();
}

/* Frames the frontend has accepted from the core; the counter the core
 * cannot fake and the wrapper cannot skip. */
static uint64_t core_frames(void)
{
   return video_state_get_ptr()->frame_count;
}

static bool menu_is_up(void)
{
   return (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE) != 0;
}

/* What the menu does: write the setting's target, then its handler. */
static void set_threaded_via_setting(bool on)
{
   rarch_setting_t *setting = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_THREADED);
   CHECK(setting != NULL, "video_threaded setting not found");
   if (!setting)
      return;
   *setting->value.target.boolean = on;
   if (setting->actions && setting->actions->change)
      setting->actions->change(setting);
}

static void expect_wrapper(bool active, const char *when)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   CHECK(video_st->thread_wrapper_active == active,
         "%s: thread_wrapper_active=%d, expected %d",
         when, video_st->thread_wrapper_active, active);
   CHECK(video_st->data != NULL, "%s: no video driver data", when);
   if (active)
   {
      thread_video_t *thr = (thread_video_t*)video_st->data;
      CHECK(thr->thread != NULL, "%s: wrapper active but no thread", when);
      CHECK(thr->driver != NULL && thr->driver_data != NULL,
            "%s: wrapper has no wrapped driver", when);
   }
   /* Runloop-thread readers that go through the wrapper. All of them
    * have to work in either state without touching a stale handle. */
   (void)video_context_driver_presentable();
   (void)video_driver_has_focus();
   (void)video_driver_has_windowed();
   {
      struct video_viewport vp;
      memset(&vp, 0, sizeof(vp));
      video_driver_get_viewport_info(&vp);
   }
}

/* Frames rendered by the wrapper since the last call: proof that the
 * worker is consuming what the runloop hands it. */
static unsigned wrapper_frames_since(unsigned *last)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   thread_video_t *thr = (thread_video_t*)video_st->data;
   unsigned now, d;
   if (!video_st->thread_wrapper_active || !thr)
      return 0;
   now = thr->hit_count;
   d   = now - *last;
   *last = now;
   return d;
}

/* ------------------------------------------------------------------ */
/* Lane: the reported sequence, looped                                */
/*   core running, menu up, threaded off -> on, leave menu, run,      */
/*   back to menu, on -> off, leave menu, run.                        */
/* ------------------------------------------------------------------ */

static void lane_toggle_cycle(unsigned cycles)
{
   unsigned had = failures;
   unsigned c;
   unsigned last_hits = 0;

   for (c = 0; c < cycles; c++)
   {
      uint64_t before;

      CHECK(menu_is_up(), "cycle %u: menu not up at start", c);
      expect_wrapper(false, "cycle start");

      set_threaded_via_setting(true);
      expect_wrapper(true, "after threaded on");
      last_hits = 0;
      wrapper_frames_since(&last_hits);

      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
      CHECK(!menu_is_up(), "cycle %u: menu still up after toggle", c);

      before = core_frames();
      run_frames(90);
      CHECK(core_frames() > before, "cycle %u: core did not run", c);
      CHECK(wrapper_frames_since(&last_hits) > 0,
            "cycle %u: wrapper rendered nothing over 90 frames", c);
      expect_wrapper(true, "threaded in game");

      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
      CHECK(menu_is_up(), "cycle %u: menu did not come back", c);
      run_frames(5);
      expect_wrapper(true, "threaded in menu");

      set_threaded_via_setting(false);
      expect_wrapper(false, "after threaded off");

      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
      before = core_frames();
      run_frames(30);
      CHECK(core_frames() > before, "cycle %u: core did not run unthreaded", c);
      expect_wrapper(false, "unthreaded in game");

      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
      run_frames(2);
   }

   if (failures == had)
      fprintf(stderr, "[pass] toggle cycle lane (%u cycles)\n", cycles);
}

/* ------------------------------------------------------------------ */
/* Lane: reinit while threaded and in game                            */
/*   VIDEO_REINIT and full DRIVERS_REINIT with the wrapper up, which  */
/*   is what a resolution or fullscreen change does.                  */
/* ------------------------------------------------------------------ */

static void lane_reinit_under_wrapper(unsigned reps)
{
   unsigned had = failures;
   unsigned i;

   set_threaded_via_setting(true);
   command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_frames(10);
   expect_wrapper(true, "before reinit");

   for (i = 0; i < reps; i++)
   {
      int flags = DRIVER_VIDEO_MASK | DRIVER_INPUT_MASK;
      command_event(CMD_EVENT_REINIT, &flags);
      run_frames(10);
      expect_wrapper(true, "after video reinit");
      command_event(CMD_EVENT_REINIT, NULL);
      run_frames(10);
      expect_wrapper(true, "after full reinit");
   }

   command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   set_threaded_via_setting(false);
   expect_wrapper(false, "after lane");

   if (failures == had)
      fprintf(stderr, "[pass] reinit-under-wrapper lane (%u reps)\n", reps);
}

/* ------------------------------------------------------------------ */
/* Lane: toggle with the menu closed                                  */
/*   Setting written while the core is running, no menu in between,  */
/*   as a hotkey or override would do it.                            */
/* ------------------------------------------------------------------ */

static void lane_toggle_in_game(unsigned cycles)
{
   unsigned had = failures;
   unsigned c;

   command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   CHECK(!menu_is_up(), "menu still up");

   for (c = 0; c < cycles; c++)
   {
      set_threaded_via_setting(true);
      run_frames(20);
      expect_wrapper(true, "in-game on");
      set_threaded_via_setting(false);
      run_frames(20);
      expect_wrapper(false, "in-game off");
   }

   command_event(CMD_EVENT_MENU_TOGGLE, NULL);

   if (failures == had)
      fprintf(stderr, "[pass] toggle-in-game lane (%u cycles)\n", cycles);
}

/* ------------------------------------------------------------------ */
/* Lane: swap counter                                                 */
/*   Advances by presents-per-frame on both paths, and by exactly one */
/*   per present (no BFI, no sub-frames here). Read from the runloop  */
/*   thread after a barrier so the worker's tally is complete.        */
/* ------------------------------------------------------------------ */

static void lane_swap_count(void)
{
   unsigned had = failures;
   video_driver_state_t *video_st = video_state_get_ptr();
   uint64_t before, after, frames0, frames1;

   /* Direct path. */
   command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_frames(3);
   expect_wrapper(false, "swapcount direct");
   before  = video_st->swap_count;
   frames0 = core_frames();
   run_frames(40);
   after   = video_st->swap_count;
   frames1 = core_frames();
   CHECK(after - before == frames1 - frames0,
         "direct path: swap_count moved %llu over %llu presented frames",
         (unsigned long long)(after - before),
         (unsigned long long)(frames1 - frames0));

   /* Threaded path: the worker owns it; wait_idle drains it. */
   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "swapcount threaded");
   video_thread_wait_idle();
   before  = video_thread_swap_count();
   run_frames(40);
   video_thread_wait_idle();
   after   = video_thread_swap_count();
   {
      thread_video_t *thr = (thread_video_t*)video_st->data;
      CHECK(after > before, "threaded path: swap_count did not advance");
      CHECK(after - before <= 40 + 3,
            "threaded path: swap_count %llu over 40 frames",
            (unsigned long long)(after - before));
      (void)thr;
   }
   set_threaded_via_setting(false);
   command_event(CMD_EVENT_MENU_TOGGLE, NULL);

   if (failures == had)
      fprintf(stderr, "[pass] swap-count lane\n");
}

/* ------------------------------------------------------------------ */
/* Lane: presenter repeats                                            */
/*   With video_threaded_present_repeat on and a driver that can      */
/*   present its last frame again (the null driver can), a stalled    */
/*   core must not stall the display: the worker keeps presenting at  */
/*   the refresh period, each repeat advancing swap_count by one and  */
/*   never counting as a rendered frame for the idle barrier.         */
/* ------------------------------------------------------------------ */

static void lane_present_repeat(void)
{
   unsigned had = failures;
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   thread_video_t *thr;
   uint64_t swaps0, swaps1, rep0, rep1;

   settings->bools.video_threaded_present_repeat = true;

   command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   set_threaded_via_setting(true);
   run_frames(10);
   expect_wrapper(true, "repeat lane");
   thr = (thread_video_t*)video_st->data;
   video_thread_wait_idle();

   /* Core stall: no frames for roughly four display periods. */
   slock_lock(thr->lock);
   rep0   = thr->frames_repeated;
   slock_unlock(thr->lock);
   swaps0 = video_thread_swap_count();
   retro_sleep(70);
   video_thread_wait_idle();          /* must not block on repeats */
   slock_lock(thr->lock);
   rep1   = thr->frames_repeated;
   slock_unlock(thr->lock);
   swaps1 = video_thread_swap_count();
   CHECK(rep1 > rep0, "no repeats during a 70 ms core stall");
   CHECK(rep1 - rep0 >= 2 && rep1 - rep0 <= 8,
         "%llu repeats over 70 ms at 60 Hz", (unsigned long long)(rep1 - rep0));
   CHECK(swaps1 - swaps0 == rep1 - rep0,
         "swap_count moved %llu for %llu repeats",
         (unsigned long long)(swaps1 - swaps0), (unsigned long long)(rep1 - rep0));

   /* Core back: rendering resumes, repeats stop competing with it. */
   run_frames(30);
   expect_wrapper(true, "after stall");

   /* BFI: a repeat replays the whole light+dark group, so it comes
    * half as often and moves swap_count by two. */
   settings->uints.video_black_frame_insertion = 1;
   run_frames(5);
   video_thread_wait_idle();
   slock_lock(thr->lock);
   rep0   = thr->frames_repeated;
   slock_unlock(thr->lock);
   swaps0 = video_thread_swap_count();
   retro_sleep(70);
   video_thread_wait_idle();
   slock_lock(thr->lock);
   rep1   = thr->frames_repeated;
   slock_unlock(thr->lock);
   swaps1 = video_thread_swap_count();
   CHECK(rep1 > rep0, "no group repeats during a 70 ms stall under BFI");
   CHECK(rep1 - rep0 <= 4,
         "%llu group repeats over 70 ms at 60 Hz with BFI 1 (period should be ~33 ms)",
         (unsigned long long)(rep1 - rep0));
   CHECK(swaps1 - swaps0 == 2 * (rep1 - rep0),
         "swap_count moved %llu for %llu group repeats of two swaps",
         (unsigned long long)(swaps1 - swaps0), (unsigned long long)(rep1 - rep0));
   settings->uints.video_black_frame_insertion = 0;
   run_frames(5);

   /* Off: no repeats at all through the same stall. */
   settings->bools.video_threaded_present_repeat = false;
   run_frames(5);
   video_thread_wait_idle();
   slock_lock(thr->lock);
   rep0 = thr->frames_repeated;
   slock_unlock(thr->lock);
   retro_sleep(70);
   video_thread_wait_idle();
   slock_lock(thr->lock);
   rep1 = thr->frames_repeated;
   slock_unlock(thr->lock);
   CHECK(rep1 == rep0,
         "repeats happened with the setting off");

   set_threaded_via_setting(false);
   command_event(CMD_EVENT_MENU_TOGGLE, NULL);

   if (failures == had)
      fprintf(stderr, "[pass] present-repeat lane\n");
}

/* ------------------------------------------------------------------ */
/* Lane: every marshalled command replies                            */
/*   Each wrapper entry point that crosses to the video thread is    */
/*   called once with the wrapper up; a command that never replied   */
/*   would hang the caller, and one that replied twice would hand    */
/*   the next caller a stale reply. After all of them the worker     */
/*   still has to consume frames.                                    */
/* ------------------------------------------------------------------ */

static void lane_every_command_replies(void)
{
   unsigned had = failures;
   video_driver_state_t *video_st = video_state_get_ptr();
   const video_driver_t *drv;
   const video_poke_interface_t *poke = NULL;
   void *data;
   unsigned last_hits = 0;
   unsigned w = 0, h = 0;
   float hz;

   command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   set_threaded_via_setting(true);
   run_frames(5);
   expect_wrapper(true, "command lane");
   drv  = video_st->current_video;
   data = video_st->data;
   wrapper_frames_since(&last_hits);

   /* video_driver_t */
   drv->set_nonblock_state(data, true, false, 1);
   drv->set_nonblock_state(data, false, true, 2);
   drv->set_nonblock_state(data, false, false, 1);
   if (drv->set_viewport)
      drv->set_viewport(data, 640, 480, false, true);
   if (drv->set_rotation)
      drv->set_rotation(data, 1), drv->set_rotation(data, 0);
   if (drv->set_shader)
      drv->set_shader(data, RARCH_SHADER_NONE, NULL);
   (void)drv->alive(data);
   (void)drv->focus(data);
   (void)drv->has_windowed(data);
   (void)drv->suppress_screensaver(data, true);

   /* video_poke_interface_t */
   if (drv->poke_interface)
      drv->poke_interface(data, &poke);
   CHECK(poke != NULL, "wrapper has no poke interface");
   if (poke)
   {
      if (poke->set_filtering)       poke->set_filtering(data, 0, true, false);
      if (poke->get_video_output_size) poke->get_video_output_size(data, &w, &h, NULL, 0);
      if (poke->get_video_output_prev) poke->get_video_output_prev(data);
      if (poke->get_video_output_next) poke->get_video_output_next(data);
      if (poke->set_aspect_ratio)    poke->set_aspect_ratio(data, 0);
      if (poke->apply_state_changes) poke->apply_state_changes(data);
      if (poke->set_texture_enable)  poke->set_texture_enable(data, false, false);
      if (poke->show_mouse)          poke->show_mouse(data, true);
      if (poke->grab_mouse_toggle)   poke->grab_mouse_toggle(data), poke->grab_mouse_toggle(data);
      if (poke->set_hdr_menu_nits)   poke->set_hdr_menu_nits(data, 200.0f);
      if (poke->set_hdr_paper_white_nits) poke->set_hdr_paper_white_nits(data, 200.0f);
      if (poke->set_hdr_expand_gamut) poke->set_hdr_expand_gamut(data, 1);
      if (poke->set_hdr_scanlines)   poke->set_hdr_scanlines(data, false);
      if (poke->set_hdr_subpixel_layout) poke->set_hdr_subpixel_layout(data, 0);
      if (poke->get_current_shader)  (void)poke->get_current_shader(data);
      if (poke->get_flags)           (void)poke->get_flags(data);
      CHECK(poke->get_refresh_rate != NULL, "wrapper does not forward get_refresh_rate");
      if (poke->get_refresh_rate)
      {
         hz = poke->get_refresh_rate(data);
         CHECK(hz >= 0.0f, "refresh rate %f", hz);
      }
      CHECK(poke->present_last != NULL, "wrapper does not forward present_last");
      if (poke->present_last)
         (void)poke->present_last(data);
   }

   run_frames(30);
   CHECK(wrapper_frames_since(&last_hits) > 0,
         "worker stopped consuming after the command walk");
   video_thread_wait_idle();

   set_threaded_via_setting(false);
   command_event(CMD_EVENT_MENU_TOGGLE, NULL);

   if (failures == had)
      fprintf(stderr, "[pass] every-command-replies lane\n");
}

/* ------------------------------------------------------------------ */
/* Lane: a second ring waiter                                        */
/*   Ring progress is broadcast, so another thread may block in      */
/*   video_thread_wait_idle() while the main thread paces on the     */
/*   ring and drains it for the menu. Neither may starve.            */
/* ------------------------------------------------------------------ */

static slock_t *idle_lock;
static int idle_stop;
static unsigned idle_waits;
static void idle_waiter(void *p)
{
   (void)p;
   for (;;)
   {
      bool stop;
      slock_lock(idle_lock);
      stop = idle_stop != 0;
      slock_unlock(idle_lock);
      if (stop)
         break;
      video_thread_wait_idle();
      slock_lock(idle_lock);
      idle_waits++;
      slock_unlock(idle_lock);
      retro_sleep(1);
   }
}

static void lane_second_ring_waiter(void)
{
   unsigned had = failures;
   sthread_t *t;
   unsigned last_hits = 0;

   set_threaded_via_setting(true);
   run_frames(5);
   expect_wrapper(true, "second waiter");
   wrapper_frames_since(&last_hits);

   idle_lock  = slock_new();
   idle_stop  = 0;
   idle_waits = 0;
   t = sthread_create(idle_waiter, NULL);
   CHECK(t != NULL, "could not start waiter thread");

   /* In game (paced ring wait on main) and in menu (drain wait). */
   command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_frames(60);
   command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_frames(60);

   slock_lock(idle_lock);
   idle_stop = 1;
   slock_unlock(idle_lock);
   if (t)
      sthread_join(t);
   slock_free(idle_lock);
   CHECK(idle_waits > 0, "waiter thread never got through wait_idle");
   CHECK(wrapper_frames_since(&last_hits) > 0,
         "worker stopped consuming with a second ring waiter");

   set_threaded_via_setting(false);

   if (failures == had)
      fprintf(stderr, "[pass] second-ring-waiter lane (%u idle waits)\n", idle_waits);
}

/* ------------------------------------------------------------------ */
/* Lane: wrapper entry points called from inside frame()              */
/*   The menu drivers call current_video->set_viewport() while they   */
/*   draw, which under the wrapper is on the video thread. A blocking  */
/*   command sent from there waits on itself. This wraps the driver   */
/*   the wrapper wraps so its frame() does exactly that, plus the      */
/*   other marshalled calls, while the main thread keeps sending its   */
/*   own commands into the same mailbox.                               */
/* ------------------------------------------------------------------ */

static video_driver_t reentrant_driver;
static const video_driver_t *reentrant_inner;
static unsigned reentrant_frames;

static bool reentrant_frame(void *data, const void *frame, unsigned w,
      unsigned h, uint64_t count, unsigned pitch, const char *msg,
      video_frame_info_t *info)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   const video_driver_t *cur      = video_st->current_video;
   const video_poke_interface_t *poke = NULL;
   bool ret;

   /* As ozone/xmb do: through the frontend's current driver, which is
    * the wrapper, from the video thread. */
   if (cur && cur->set_viewport)
      cur->set_viewport(video_st->data, w, h, false, true);
   if (cur && cur->set_nonblock_state)
      cur->set_nonblock_state(video_st->data, false, false, 1);
   if (cur && cur->poke_interface)
      cur->poke_interface(video_st->data, &poke);
   if (poke && poke->set_aspect_ratio)
      poke->set_aspect_ratio(video_st->data, 0);
   if (poke && poke->set_hdr_menu_nits)
      poke->set_hdr_menu_nits(video_st->data, 100.0f);
   if (poke && poke->get_refresh_rate)
      (void)poke->get_refresh_rate(video_st->data);

   ret = reentrant_inner->frame(data, frame, w, h, count, pitch, msg, info);
   reentrant_frames++;
   return ret;
}

static void lane_reentrant_from_frame(void)
{
   unsigned had = failures;
   video_driver_state_t *video_st = video_state_get_ptr();
   thread_video_t *thr;
   const video_driver_t *drv;
   unsigned before, i;

   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "reentrant lane");
   thr = (thread_video_t*)video_st->data;

   /* Swap the wrapped driver for one whose frame() re-enters. */
   video_thread_wait_idle();
   reentrant_inner  = thr->driver;
   reentrant_driver = *thr->driver;
   reentrant_driver.frame = reentrant_frame;
   thr->driver      = &reentrant_driver;
   reentrant_frames = 0;

   /* Frames from the core while the main thread also sends commands
    * of its own, so the inline path and the mailbox coexist. */
   drv    = video_st->current_video;
   before = reentrant_frames;
   for (i = 0; i < 60; i++)
   {
      run_frames(1);
      if (drv->set_rotation)
         drv->set_rotation(video_st->data, i & 3);
      if ((i % 7) == 0 && drv->set_viewport)
         drv->set_viewport(video_st->data, 800, 600, false, true);
   }
   video_thread_wait_idle();
   CHECK(reentrant_frames > before,
         "re-entering frame() never completed (self-wait)");

   thr->driver = reentrant_inner;
   set_threaded_via_setting(false);

   if (failures == had)
      fprintf(stderr, "[pass] reentrant-from-frame lane (%u frames)\n", reentrant_frames);
}

/* ------------------------------------------------------------------ */
/* Lane: display-phase scheduling with a stale report                  */
/*   The presenter schedules the next repeat from the driver's display */
/*   timestamp. A driver whose report lags well behind the clock must  */
/*   not make repeats fire back-to-back: the deadline is advanced past */
/*   now in whole periods, so the cadence holds.                       */
/* ------------------------------------------------------------------ */

static video_driver_t phase_driver;
static const video_driver_t *phase_inner;
static video_poke_interface_t phase_poke;
static const video_poke_interface_t *phase_inner_poke;

static retro_time_t phase_stale_present_time(void *data)
{
   (void)data;
   return cpu_features_get_time_usec() - 50000; /* three periods late */
}

static void phase_get_poke(void *data, const video_poke_interface_t **iface)
{
   phase_inner->poke_interface(data, &phase_inner_poke);
   phase_poke = *phase_inner_poke;
   phase_poke.get_last_present_time = phase_stale_present_time;
   *iface = &phase_poke;
}

static void lane_display_phase(void)
{
   unsigned had = failures;
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   thread_video_t *thr;
   uint64_t rep0, rep1;

   settings->bools.video_threaded_present_repeat = true;
   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "phase lane");
   thr = (thread_video_t*)video_st->data;

   video_thread_wait_idle();
   phase_inner  = thr->driver;
   phase_driver = *thr->driver;
   phase_driver.poke_interface = phase_get_poke;
   thr->driver  = &phase_driver;
   /* The wrapper caches its poke at init; refresh it through the
    * swapped driver so the stale report is what the worker reads. */
   phase_driver.poke_interface(thr->driver_data, &thr->poke);

   run_frames(10);
   video_thread_wait_idle();
   slock_lock(thr->lock);
   rep0 = thr->frames_repeated;
   slock_unlock(thr->lock);
   retro_sleep(70);
   video_thread_wait_idle();
   slock_lock(thr->lock);
   rep1 = thr->frames_repeated;
   slock_unlock(thr->lock);
   CHECK(rep1 > rep0, "no repeats with a stale display report");
   CHECK(rep1 - rep0 <= 8,
         "%llu repeats over 70 ms: a stale display report piled them up",
         (unsigned long long)(rep1 - rep0));

   thr->driver = phase_inner;
   thr->poke   = phase_inner_poke;
   settings->bools.video_threaded_present_repeat = false;
   set_threaded_via_setting(false);

   if (failures == had)
      fprintf(stderr, "[pass] display-phase lane (%llu repeats)\n",
            (unsigned long long)(rep1 - rep0));
}

/* ------------------------------------------------------------------ */
/* Lane: a command runs once while the presenter is repeating          */
/*   The worker wakes on its own for repeats. A command whose reply    */
/*   the main thread has not yet consumed must not be run again on    */
/*   such a wake: for a texture unload that is a double free.          */
/* ------------------------------------------------------------------ */

static unsigned once_runs;
static uintptr_t once_command(void *p)
{
   (void)p;
   once_runs++;
   /* Long enough that the repeat deadline is past when this returns,
    * so the very next wake is a repeat wake with the reply still in
    * the mailbox. */
   retro_sleep(25);
   return 7;
}

static void lane_command_runs_once(void)
{
   unsigned had = failures;
   settings_t *settings = config_get_ptr();
   unsigned i;

   settings->bools.video_threaded_present_repeat = true;
   set_threaded_via_setting(true);
   run_frames(10);
   expect_wrapper(true, "runs-once lane");
   video_thread_wait_idle();

   once_runs = 0;
   for (i = 0; i < 20; i++)
   {
      uintptr_t r = video_thread_texture_handle(NULL, once_command);
      CHECK(r == 7, "command %u returned %lu", i, (unsigned long)r);
      /* Let a few repeats fire between commands. */
      retro_sleep(40);
   }
   video_thread_wait_idle();
   CHECK(once_runs == 20, "commands ran %u times for 20 sends", once_runs);

   settings->bools.video_threaded_present_repeat = false;
   set_threaded_via_setting(false);

   if (failures == had)
      fprintf(stderr, "[pass] command-runs-once lane\n");
}

/* ------------------------------------------------------------------ */
/* Lane: font init and free reach the video thread as context-local   */
/*   A renderer's is_threaded argument means "you are not on the      */
/*   context thread, bind it yourself"; the GL renderers answer it    */
/*   with make_current(), and on free that unbinds the context from   */
/*   the calling thread. The font driver marshals both calls to the   */
/*   video thread under the wrapper, so they must arrive there with   */
/*   is_threaded false - otherwise the deferred free unbinds the      */
/*   worker's own context and every frame after it goes nowhere.      */
/*   This renderer keeps a context flag the way GLX does: is_threaded */
/*   on init binds it, is_threaded on free releases it.               */
/* ------------------------------------------------------------------ */

static bool      fontlane_context_bound;
static uintptr_t fontlane_init_thread;
static uintptr_t fontlane_free_thread;
static int       fontlane_init_threaded;
static int       fontlane_free_threaded;
static unsigned  fontlane_frees;

static void *fontlane_renderer_init(void *data, const char *font_path,
      float font_size, bool is_threaded)
{
   (void)data; (void)font_path; (void)font_size;
   fontlane_init_thread   = sthread_get_current_thread_id();
   fontlane_init_threaded = is_threaded;
   if (is_threaded)
      fontlane_context_bound = true;
   return malloc(1);
}

static void fontlane_renderer_free(void *data, bool is_threaded)
{
   fontlane_free_thread   = sthread_get_current_thread_id();
   fontlane_free_threaded = is_threaded;
   fontlane_frees++;
   if (is_threaded)
      fontlane_context_bound = false;
   free(data);
}

static const font_renderer_t fontlane_renderer = {
   fontlane_renderer_init,
   fontlane_renderer_free,
   NULL,
   "harness",
   NULL,
   NULL,
   NULL,
   NULL,
   NULL
};

static void lane_font_marshal(void)
{
   unsigned had = failures;
   video_driver_state_t *video_st = video_state_get_ptr();
   thread_video_t *thr;
   uintptr_t worker;
   font_data_t *font;

   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "font lane");
   thr    = (thread_video_t*)video_st->data;
   worker = sthread_get_thread_id(thr->thread);

   /* The worker owns its context throughout. */
   fontlane_context_bound = true;

   /* Built from the main thread as the menu drivers do it. */
   fontlane_init_threaded = -1;
   font = font_driver_init_first(video_st->data, NULL, 12.0f,
         true, true, &fontlane_renderer);
   CHECK(font != NULL, "font init failed");
   CHECK(fontlane_init_thread == worker,
         "renderer init did not run on the video thread");
   CHECK(fontlane_init_threaded == 0,
         "renderer init reached the video thread with is_threaded set");

   /* Retired from a render path, released after the frames that
    * could still read it have gone out - the path 0e45cec310 added. */
   fontlane_free_threaded = -1;
   fontlane_frees         = 0;
   font_driver_free_deferred(font);
   run_frames(4);
   video_thread_wait_idle();
   CHECK(fontlane_frees == 1, "deferred free ran %u times", fontlane_frees);
   CHECK(fontlane_free_thread == worker,
         "deferred free did not run on the video thread");
   CHECK(fontlane_free_threaded == 0,
         "deferred free reached the video thread with is_threaded set");
   CHECK(fontlane_context_bound,
         "the video thread lost its context to a font free");

   /* The immediate path, as teardown and the queue-full fallback use. */
   font = font_driver_init_first(video_st->data, NULL, 12.0f,
         true, true, &fontlane_renderer);
   CHECK(font != NULL, "second font init failed");
   fontlane_free_threaded = -1;
   font_driver_free(font);
   CHECK(fontlane_free_thread == worker,
         "immediate free did not run on the video thread");
   CHECK(fontlane_free_threaded == 0,
         "immediate free reached the video thread with is_threaded set");
   CHECK(fontlane_context_bound,
         "the video thread lost its context to an immediate font free");

   set_threaded_via_setting(false);

   if (failures == had)
      fprintf(stderr, "[pass] font-marshal lane\n");
}

/* ------------------------------------------------------------------ */
/* Lane: display pacing holds the runloop to the display's cadence     */
/*   With the null driver, core and render both take ~0, so the pacer  */
/*   should release each frame just under a period after the last one  */
/*   and nothing should be dropped: N frames take about N periods.     */
/* ------------------------------------------------------------------ */

/* One measurement of the hold: n core frames with the display at
 * display_hz and the core at its own 60. The hold must pace to the
 * core's period on the display's grid: at 120 Hz a 60 fps core is due
 * every other vblank, and a hold that released it every vblank ran it
 * at four times speed on a real 120 Hz panel. */
static void display_pacing_measure(float display_hz, unsigned n,
      float expect_fps, const char *what)
{
   settings_t *settings = config_get_ptr();
   thread_video_t *thr  = (thread_video_t*)video_state_get_ptr()->data;
   retro_time_t t0, took, expect;
   unsigned dropped_before, dropped_after;
   float saved = settings->floats.video_refresh_rate;

   settings->floats.video_refresh_rate = display_hz;
   run_frames(6);
   video_thread_wait_idle();
   dropped_before = thr->miss_count;
   t0 = cpu_features_get_time_usec();
   run_frames(n);
   took = cpu_features_get_time_usec() - t0;
   video_thread_wait_idle();
   dropped_after = thr->miss_count;
   settings->floats.video_refresh_rate = saved;

   expect = (retro_time_t)(1000000.0 * n / expect_fps);
   CHECK(took > expect * 3 / 4,
         "display pacing, %s at %.0f Hz, did not hold the runloop: %u frames in %.1f ms, expected ~%.1f",
         what, display_hz, n, took / 1000.0, expect / 1000.0);
   CHECK(took < expect * 3 / 2,
         "display pacing, %s at %.0f Hz, held the runloop too long: %u frames in %.1f ms, expected ~%.1f",
         what, display_hz, n, took / 1000.0, expect / 1000.0);
   CHECK(dropped_after - dropped_before <= 2,
         "display pacing, %s at %.0f Hz, dropped %u frames", what, display_hz,
         dropped_after - dropped_before);
   fprintf(stderr, "   display pacing, %s at %.0f Hz: %u frames in %.1f ms\n",
         what, display_hz, n, took / 1000.0);
}

static void lane_display_pacing(void)
{
   unsigned had = failures;
   settings_t *settings = config_get_ptr();
   bool saved_pacing = settings->bools.video_threaded_display_pacing;

   settings->bools.video_threaded_display_pacing = true;
   set_threaded_via_setting(true);
   run_frames(10);
   expect_wrapper(true, "display-pacing lane");
   /* The hold is for gameplay: it stands down while the menu is up,
    * where the command drain already serialises, and the menu driver
    * re-enables its texture every iteration it is alive. Close it for
    * the measurement. Until the runloop's gap limiter was told to stay
    * out under display pacing, the limiter paced these frames and this
    * lane passed without the hold ever running. */
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   CHECK(!menu_is_up(), "display-pacing lane: menu still up");
   run_frames(3);
   video_thread_wait_idle();

   /* The core is 60 fps whatever the display does. */
   display_pacing_measure(60.0f, 60, 60.0f, "content");
   display_pacing_measure(120.0f, 60, 60.0f, "content");

   /* Back to the menu for the lanes that follow - and the menu is
    * paced too, to the display's rate: with the gap limiter standing
    * aside for display pacing, the hold is the only thing between the
    * menu and running unthrottled. */
   if (!menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   CHECK(menu_is_up(), "display-pacing lane: menu did not reopen");
#ifdef HAVE_MENU
   /* A core running under the menu is still content and keeps the
    * content's period; at the display's it ran at twice the speed on
    * a 120 Hz panel. Only with the core stopped is a frame the menu's,
    * at the display's rate. */
   {
      bool saved_pause = settings->bools.menu_pause_libretro;
      settings->bools.menu_pause_libretro = false;
      display_pacing_measure(120.0f, 60, 60.0f, "menu, core running");
      settings->bools.menu_pause_libretro = saved_pause;
   }
   /* The quick menu over a paused core is the common case, and takes
    * a different path through the runloop: the cached frame, with no
    * core run before it. */
   {
      bool saved_pause = settings->bools.menu_pause_libretro;
      settings->bools.menu_pause_libretro = true;
      display_pacing_measure(120.0f, 60, 120.0f, "menu, core paused");
      settings->bools.menu_pause_libretro = saved_pause;
   }
#endif
   settings->bools.video_threaded_display_pacing = saved_pacing;

   if (failures == had)
      fprintf(stderr, "[pass] display-pacing lane\n");
}

/* ------------------------------------------------------------------ */
/* Lane: zero-copy lends a slot and publishes it without a copy         */
/*   The harness core asks for a framebuffer each frame; with the       */
/*   setting on the wrapper should grant most asks and publish those    */
/*   frames from the lent slot. With it off, no ask is granted.         */
/* ------------------------------------------------------------------ */

/* A driver answering a synchronous command on the video thread can ask
 * for a call on the thread that is waiting for the reply - the core's
 * thread, which holds the core's GL context under the hardware ring.
 * The gl driver uses it to rebuild the core's framebuffers on a shader
 * change. Modelled here: the driver's set_shader asks for one and the
 * lane checks the call ran on the main thread, before the reply, and
 * exactly once. */
static video_driver_t        wclane_driver;
static const video_driver_t *wclane_inner;
static uintptr_t             wclane_call_thread;
static unsigned              wclane_calls;
static bool                  wclane_before_reply;

static void wclane_cb(void *data)
{
   (void)data;
   wclane_call_thread = sthread_get_current_thread_id();
   wclane_calls++;
}

static bool wclane_set_shader(void *data, enum rarch_shader_type type, const char *path)
{
   bool ret;
   video_thread_call_on_waiter(wclane_cb, NULL);
   wclane_before_reply = (wclane_calls == 1);
   ret = wclane_inner->set_shader ? wclane_inner->set_shader(data, type, path) : true;
   return ret;
}

/* A window resize under the wrapper. The driver notices it on the
 * video thread, in alive() between frames, and records the new output
 * size; a frame already pushed carries the size known when it was
 * built on the main thread. A driver that sizes its swapchain and
 * viewport from the frame info then rebuilt them at the old size and
 * laid the menu out for it - Windows, Vulkan, windowed, going back into
 * the menu. The ordering is forced here: frame K is held on the video
 * thread until the main thread has pushed K+1, then the resize is
 * reported, then K+1 is drawn - and the size K+1 is drawn at must be
 * the reported one, not the one it was pushed with. */
static video_driver_t        rslane_driver;
static const video_driver_t *rslane_inner;
static unsigned              rslane_report_w, rslane_report_h;
static unsigned              rslane_seen_w,   rslane_seen_h;
static bool                  rslane_seen;
/* 0 idle, 1 hold K, 2 K+1 pushed, 3 reported. Written by the main
 * thread, read on the video thread: an atomic, so TSan sees the
 * handoff it is. */
static retro_atomic_size_t   rslane_stage;
#define RSLANE_GET()   retro_atomic_load_acquire_size(&rslane_stage)
#define RSLANE_SET(v)  retro_atomic_store_release_size(&rslane_stage, (v))

static bool rslane_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   if (RSLANE_GET() == 1)
   {
      /* Frame K: wait for the main thread to push K+1, then report the
       * resize as a context driver's check_window would. */
      unsigned spins = 0;
      while (RSLANE_GET() == 1 && spins++ < 2000)
         retro_sleep(1);
      video_driver_set_output_size(rslane_report_w, rslane_report_h);
      RSLANE_SET(3);
   }
   else if (RSLANE_GET() == 3 && !rslane_seen)
   {
      /* Frame K+1: what size is this drawn at? The size before the
       * report may be zero in the harness, so a flag, not the value. */
      rslane_seen_w = video_info->width;
      rslane_seen_h = video_info->height;
      rslane_seen   = true;
   }
   return rslane_inner->frame(data, frame, width, height, frame_count,
         pitch, msg, video_info);
}

static void lane_resize_under_wrapper(void)
{
   unsigned had = failures;
   thread_video_t *thr;
   unsigned before_w = 0, before_h = 0;

   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "resize lane");
   thr = (thread_video_t*)video_state_get_ptr()->data;
   /* In game: a menu-frame push drains the video thread first, which
    * would serialise the push after the report. */
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   CHECK(!menu_is_up(), "resize lane: menu still up");
   run_frames(2);
   video_thread_wait_idle();

   rslane_inner  = thr->driver;
   rslane_driver = *thr->driver;
   rslane_driver.frame = rslane_frame;
   thr->driver   = &rslane_driver;

   video_driver_get_output_size(&before_w, &before_h);
   rslane_report_w = before_w + 320;
   rslane_report_h = before_h + 200;
   rslane_seen_w   = rslane_seen_h = 0;
   rslane_seen     = false;

   /* K: pushed, and held on the video thread. */
   RSLANE_SET(1);
   video_driver_cached_frame();
   retro_sleep(5);
   /* K+1: pushed while K is held, built with the size known now. */
   video_driver_cached_frame();
   RSLANE_SET(2);
   /* K reports and returns; K+1 is drawn. */
   video_thread_wait_idle();
   run_frames(2);
   video_thread_wait_idle();

   CHECK(RSLANE_GET() == 3, "resize lane: frame K never ran on the video thread");
   CHECK(rslane_seen, "resize lane: frame K+1 never ran");
   CHECK(rslane_seen_w == rslane_report_w && rslane_seen_h == rslane_report_h,
         "frame after a resize was drawn at %ux%u, the driver had reported %ux%u",
         rslane_seen_w, rslane_seen_h, rslane_report_w, rslane_report_h);

   video_thread_wait_idle();
   thr->driver  = rslane_inner;
   RSLANE_SET(0);
   /* Put the size and the menu back for the lanes that follow. */
   video_driver_set_output_size(before_w, before_h);
   if (!menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_frames(2);
   if (failures == had)
      fprintf(stderr, "[pass] resize lane (frame after resize drawn at %ux%u)\n",
            rslane_seen_w, rslane_seen_h);
}

/* Heap traffic on the frame path.
 *
 * The emulation frame path makes no general-purpose heap calls today -
 * not unthreaded, not under the wrapper - and that is a property worth
 * keeping rather than a target to aim at: an allocation per frame is a
 * lock, a possible syscall and a fragmentation source on the one path
 * that must not have any. So this lane counts them, over three hundred
 * frames of each configuration, and fails if any appear.
 *
 * The counting is glibc's malloc hooks by interposition: the harness
 * defines malloc/free/calloc/realloc, forwards to __libc_*, and counts.
 * Only where that interposition works and no allocator sanitizer is in
 * the way - ASan and TSan replace these symbols themselves, and their
 * own bookkeeping would be counted - so the lane runs on SAN=none and
 * says so otherwise. The menu is not the frame path and is not counted:
 * its lists allocate when they are built, by design. */
#if defined(__GLIBC__) && !defined(__SANITIZE_ADDRESS__) && !defined(__SANITIZE_THREAD__)
#define HARNESS_COUNT_HEAP 1
#endif

#ifdef HARNESS_COUNT_HEAP
extern void *__libc_malloc(size_t);
extern void  __libc_free(void*);
extern void *__libc_calloc(size_t, size_t);
extern void *__libc_realloc(void*, size_t);

static unsigned long heap_calls;
static bool          heap_counting;

void *malloc(size_t n)
{
   if (heap_counting)
      heap_calls++;
   return __libc_malloc(n);
}

void free(void *p)
{
   __libc_free(p);
}

void *calloc(size_t a, size_t b)
{
   if (heap_counting)
      heap_calls++;
   return __libc_calloc(a, b);
}

void *realloc(void *p, size_t n)
{
   if (heap_counting)
      heap_calls++;
   return __libc_realloc(p, n);
}

static unsigned long heap_calls_over(unsigned frames)
{
   /* Settle first: the first frames after a driver change still build
    * things. What is measured is the steady state. */
   run_frames(30);
   heap_calls    = 0;
   heap_counting = true;
   run_frames(frames);
   heap_counting = false;
   return heap_calls;
}

static void lane_frame_path_heap(void)
{
   unsigned had = failures;
   unsigned long unthreaded, threaded;

   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   CHECK(!menu_is_up(), "heap lane: menu still up");

   set_threaded_via_setting(false);
   run_frames(10);
   unthreaded = heap_calls_over(300);

   set_threaded_via_setting(true);
   run_frames(10);
   expect_wrapper(true, "heap lane");
   threaded   = heap_calls_over(300);

   CHECK(unthreaded == 0,
         "unthreaded frame path made %lu heap calls over 300 frames", unthreaded);
   CHECK(threaded == 0,
         "threaded frame path made %lu heap calls over 300 frames", threaded);

   if (!menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_frames(2);
   if (failures == had)
      fprintf(stderr, "[pass] frame-path heap lane (0 heap calls in 300 frames, threaded and not)\n");
}
#else
static void lane_frame_path_heap(void)
{
   fprintf(stderr, "[skip] frame-path heap lane (needs glibc, no allocator sanitizer)\n");
}
#endif

static void lane_waiter_call(void)
{
   thread_video_t *thr;
   uintptr_t self;
   video_driver_state_t *video_st = video_state_get_ptr();

   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "waiter-call lane");
   thr           = (thread_video_t*)video_st->data;
   video_thread_wait_idle();
   wclane_inner  = thr->driver;
   wclane_driver = *thr->driver;
   wclane_driver.set_shader = wclane_set_shader;
   thr->driver   = &wclane_driver;
   wclane_calls  = 0;
   wclane_call_thread = 0;
   self          = sthread_get_current_thread_id();

   /* Through the wrapper: the main thread sends set_shader and waits;
    * the video thread's driver asks for the call. */
   video_st->current_video->set_shader(video_st->data, RARCH_SHADER_NONE, NULL);

   CHECK(wclane_calls == 1, "waiter call ran %u times, expected 1", wclane_calls);
   CHECK(wclane_call_thread == self,
         "waiter call ran on the wrong thread (not the one waiting for the reply)");
   CHECK(wclane_before_reply, "waiter call had not run when the command continued");

   /* With no waiter - called outside a command, from the video thread's
    * own frame - it must run on the caller rather than hang: the driver
    * asks from inside frame() here. */
   video_thread_wait_idle();
   thr->driver   = wclane_inner;
   run_frames(2);
   printf("   waiter-call lane: call ran on the waiting thread, before the reply\n");
}

static void lane_zero_copy(void)
{
   unsigned had = failures;
   dylib_t lib = runloop_state_get_ptr()->lib_handle;
   void (*use_fb)(int) = lib ? (void (*)(int))dylib_proc(lib, "harness_core_use_framebuffer") : NULL;
   unsigned (*granted)(void) = lib ? (unsigned (*)(void))dylib_proc(lib, "harness_core_fb_granted") : NULL;
   thread_video_t *thr;
   unsigned g0, g1, zc0, zc1;

   CHECK(use_fb && granted, "harness core lacks the framebuffer exports");
   if (!use_fb || !granted)
      return;

   /* The core only runs with the menu closed, so close it for the lane
    * and reopen it after. */
   set_threaded_via_setting(true);
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_frames(10);
   expect_wrapper(true, "zero-copy lane");
   use_fb(1);

   /* Most asks granted, and those frames published from the lent slot.
    * Dupes (every third frame) ask and then push NULL, so the loan
    * lapses; oversize frames are declined. */
   run_frames(2);
   thr = (thread_video_t*)video_state_get_ptr()->data;
   g0  = granted();
   slock_lock(thr->lock); zc0 = (unsigned)thr->frame.zero_copy_count; slock_unlock(thr->lock);
   run_frames(60);
   video_thread_wait_idle();
   g1  = granted();
   slock_lock(thr->lock); zc1 = (unsigned)thr->frame.zero_copy_count; slock_unlock(thr->lock);
   CHECK(g1 - g0 >= 30, "zero-copy on but only %u of 60 asks granted", g1 - g0);
   CHECK(zc1 - zc0 >= 20, "only %u frames published zero-copy for %u grants", zc1 - zc0, g1 - g0);
   CHECK(zc1 - zc0 <= g1 - g0, "%u zero-copy frames for %u grants", zc1 - zc0, g1 - g0);

   use_fb(0);
   if (!menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   set_threaded_via_setting(false);

   if (failures == had)
      fprintf(stderr, "[pass] zero-copy lane (%u grants, %u zero-copy frames)\n",
            g1 - g0, zc1 - zc0);
}

/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */
/* Lane: asynchronous texture uploads                                  */
/*   video_driver_texture_load_async() must return at once, upload on  */
/*   the video thread, deliver done() on the main thread in post       */
/*   order with the driver's handle, release the image exactly once,   */
/*   and hand every in-flight load a 0 when the wrapper is torn down.  */
/* ------------------------------------------------------------------ */

static video_driver_t async_driver;
static const video_driver_t *async_inner;
static video_poke_interface_t async_poke;
static const video_poke_interface_t *async_inner_poke;
static uintptr_t async_upload_thread;
static unsigned  async_uploads;

static uintptr_t async_fake_load(void *data, void *img, bool threaded,
      enum texture_filter_type filter)
{
   (void)data; (void)img; (void)threaded; (void)filter;
   async_upload_thread = sthread_get_current_thread_id();
   return 0x1000 + ++async_uploads;
}

static void async_fake_unload(void *data, bool threaded, uintptr_t id)
{
   (void)data; (void)threaded; (void)id;
}

static void async_get_poke(void *data, const video_poke_interface_t **iface)
{
   async_inner->poke_interface(data, &async_inner_poke);
   async_poke = *async_inner_poke;
   async_poke.load_texture   = async_fake_load;
   async_poke.unload_texture = async_fake_unload;
   *iface = &async_poke;
}

#define ASYNC_N 6
static unsigned  async_done_order[ASYNC_N];
static uintptr_t async_done_handle[ASYNC_N];
static unsigned  async_done_count;
static unsigned  async_released;
static uintptr_t async_done_thread;

static void async_done_cb(void *user, uintptr_t handle)
{
   unsigned idx = (unsigned)(uintptr_t)user;
   if (async_done_count < ASYNC_N)
   {
      async_done_order[async_done_count]  = idx;
      async_done_handle[async_done_count] = handle;
   }
   async_done_count++;
   async_done_thread = sthread_get_current_thread_id();
}

static void async_release_cb(void *img)
{
   (void)img;
   async_released++;
}

static void lane_async_texture_load(void)
{
   unsigned had = failures;
   video_driver_state_t *video_st = video_state_get_ptr();
   thread_video_t *thr;
   static struct texture_image imgs[ASYNC_N];
   unsigned i;
   retro_time_t t0, t1;
   uintptr_t main_thread = sthread_get_current_thread_id();

   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "async lane");
   thr = (thread_video_t*)video_st->data;

   video_thread_wait_idle();
   async_inner  = thr->driver;
   async_driver = *thr->driver;
   async_driver.poke_interface = async_get_poke;
   thr->driver  = &async_driver;
   async_driver.poke_interface(thr->driver_data, &thr->poke);
   /* video_driver_texture_load_async() goes through video_st->poke. */
   video_st->poke = thr->poke;

   async_uploads = async_done_count = async_released = 0;
   for (i = 0; i < ASYNC_N; i++)
   {
      imgs[i].width = imgs[i].height = 4;
      imgs[i].pixels = (uint32_t*)&imgs[i];
   }

   t0 = cpu_features_get_time_usec();
   for (i = 0; i < ASYNC_N; i++)
      CHECK(video_driver_texture_load_async(&imgs[i], TEXTURE_FILTER_LINEAR,
               async_done_cb, (void*)(uintptr_t)i, async_release_cb),
            "async load %u refused", i);
   t1 = cpu_features_get_time_usec();
   CHECK(t1 - t0 < 5000, "posting %u async loads took %lld us: it blocked",
         ASYNC_N, (long long)(t1 - t0));
   CHECK(async_done_count == 0, "done() ran before any frame was pushed");

   run_frames(5);
   video_thread_wait_idle();
   run_frames(1);

   CHECK(async_uploads == ASYNC_N, "%u of %u uploads reached the driver",
         async_uploads, ASYNC_N);
   CHECK(async_upload_thread != main_thread,
         "upload ran on the main thread");
   CHECK(async_released == ASYNC_N, "%u of %u images released",
         async_released, ASYNC_N);
   CHECK(async_done_count == ASYNC_N, "%u of %u done() calls",
         async_done_count, ASYNC_N);
   CHECK(async_done_thread == main_thread, "done() ran off the main thread");
   for (i = 0; i < ASYNC_N && i < async_done_count; i++)
   {
      CHECK(async_done_order[i] == i, "done() order: slot %u got load %u",
            i, async_done_order[i]);
      CHECK(async_done_handle[i] == 0x1000 + i + 1,
            "load %u delivered handle %lx", i, (unsigned long)async_done_handle[i]);
   }

   /* Teardown with loads in flight: park the worker on a command so
    * the posts sit in the in-list, then drop threaded video. Every
    * one must be released and answered with 0, none twice. */
   async_done_count = async_released = 0;
   for (i = 0; i < ASYNC_N; i++)
      video_driver_texture_load_async(&imgs[i], TEXTURE_FILTER_LINEAR,
            async_done_cb, (void*)(uintptr_t)i, async_release_cb);
   /* The video thread reads thr->poke for every queued upload, so the
    * swap goes under the lock that guards the lists it walks - the
    * test reaches into the wrapper's own state, and doing so while
    * its thread runs is a race whoever writes it. */
   slock_lock(thr->lock);
   thr->driver = async_inner;
   thr->poke   = async_inner_poke;
   slock_unlock(thr->lock);
   video_st->poke = async_inner_poke;
   set_threaded_via_setting(false);
   run_frames(2);
   CHECK(async_released == ASYNC_N, "teardown released %u of %u images",
         async_released, ASYNC_N);
   CHECK(async_done_count == ASYNC_N, "teardown answered %u of %u loads",
         async_done_count, ASYNC_N);

   /* Without the wrapper the call is synchronous and still keeps the
    * contract: release, then done, before returning. The harness
    * driver has no load_texture, so lend it the fake one. */
   {
      const video_poke_interface_t *real_poke = video_st->poke;
      video_poke_interface_t sync_poke;
      if (real_poke)
         sync_poke = *real_poke;
      else
         memset(&sync_poke, 0, sizeof(sync_poke));
      sync_poke.load_texture   = async_fake_load;
      sync_poke.unload_texture = async_fake_unload;
      video_st->poke = &sync_poke;
      async_done_count = async_released = 0;
      CHECK(video_driver_texture_load_async(&imgs[0], TEXTURE_FILTER_LINEAR,
               async_done_cb, (void*)0, async_release_cb),
            "synchronous fallback refused");
      CHECK(async_released == 1 && async_done_count == 1,
            "synchronous fallback: released %u, done %u",
            async_released, async_done_count);
      CHECK(async_done_thread == main_thread,
            "synchronous fallback: done() off the main thread");
      video_st->poke = real_poke;
   }

   if (failures == had)
      fprintf(stderr, "[pass] async texture load lane (%u uploads)\n", ASYNC_N);
}

int main(int argc, char *argv[])
{
   char cfg_path[512];
   char cmd[600];
   char dir[400];
   char *rarch_argv[8];
   int rarch_argc = 0;
   FILE *cfg;
   char core_path[512];
   unsigned cycles = argc > 1 ? (unsigned)atoi(argv[1]) : 10;

   snprintf(dir, sizeof(dir), "/tmp/threaded_video_harness_%ld", (long)getpid());
   snprintf(cmd, sizeof(cmd), "mkdir -p %s", dir);
   if (system(cmd) != 0)
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
      /* Tunable from the environment so the task worker can be kept
       * out of the process: TSan's registry never observes that
       * thread finishing when the frontend is booted from a foreign
       * main(), and the join at exit spins inside libtsan. The
       * shipping binary exits cleanly under TSan with the same tree,
       * and the wrapper under test does not depend on this. */
      fprintf(cfg, "threaded_data_runloop_enable = \"%s\"\n",
            getenv("HARNESS_THREADED_TASKS") ? "true" : "false");
      fclose(cfg);
   }

   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);

   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;
   /* The harness core, built next to this binary by build.sh. */
   {
      const char *slash = strrchr(argv[0], '/');
      int dirlen        = slash ? (int)(slash - argv[0]) : 1;
      snprintf(core_path, sizeof(core_path), "%.*s/harness_core.so",
            dirlen, slash ? argv[0] : ".");
   }
   rarch_argv[rarch_argc++] = (char*)"-L";
   rarch_argv[rarch_argc++] = core_path;

   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }

   /* The core starts without content and the menu closed. Run it,
    * then open the menu, which is the state the reported sequence
    * starts from. */
   run_frames(5);
   CHECK(runloop_state_get_ptr()->current_core_type != CORE_TYPE_DUMMY,
         "harness core did not start (dummy core running)");
   CHECK(!menu_is_up(), "menu up after starting a core");
   command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_frames(5);
   CHECK(menu_is_up(), "menu did not open");
   expect_wrapper(false, "boot");

   lane_toggle_cycle(cycles);
   lane_reinit_under_wrapper(cycles / 2 + 1);
   lane_toggle_in_game(cycles);
   lane_swap_count();
   lane_async_texture_load();
   lane_present_repeat();
   lane_every_command_replies();
   lane_second_ring_waiter();
   lane_reentrant_from_frame();
   lane_display_phase();
   lane_command_runs_once();
   lane_font_marshal();
   lane_display_pacing();
   lane_zero_copy();
   lane_waiter_call();
   lane_frame_path_heap();
   lane_resize_under_wrapper();

   /* Orderly shutdown: the teardown barriers are part of what is
    * under test. */
   set_threaded_via_setting(true);
   run_frames(3);
   main_exit(NULL);

   snprintf(cmd, sizeof(cmd), "rm -rf %s", dir);
   if (system(cmd) != 0) { }

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   fprintf(stderr, "all lanes passed (frontend accepted %llu core frames)\n", (unsigned long long)core_frames());
   return 0;
}

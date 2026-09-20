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
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <file/file_path.h>

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

/* True when HARNESS_VIDEO_DRIVER names a real driver. The lanes that
 * instrument the null driver - fake present reports, fake sizes, fake
 * frame() hooks, heap counting through the null frame path - are
 * skipped then, because what they assert is the wrapper's handling of
 * what the null driver was told to say. Every other lane runs through
 * the real driver, on its real context, and the CI Vulkan lane adds
 * the validation layer on top. */
static bool real_driver(void)
{
   const char *drv = getenv("HARNESS_VIDEO_DRIVER");
   return drv && strcmp(drv, "null") != 0;
}

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
/* Lane: the window thread presents a cached frame                     */
/*   Win32's modal size/move loop pumps on the thread that owns the    */
/*   window, and under threaded video that is the video thread. The    */
/*   tick calls alive() then video_driver_cached_frame(), so the       */
/*   cached frame is presented from the video thread while the runloop */
/*   thread is pushing frames of its own.                              */
/*                                                                     */
/*   The invariant this guards is that the driver's frame() only ever  */
/*   runs on the video thread. A reentrancy check keyed on a shared    */
/*   flag rather than on thread identity breaks it here: the runloop   */
/*   thread reads the flag the video thread set and renders from the   */
/*   wrong thread.                                                     */
/* ------------------------------------------------------------------ */

static video_driver_t       wintick_driver;
static const video_driver_t *wintick_inner;
static uint64_t             wintick_frame_thread;
static unsigned             wintick_frames;
static unsigned             wintick_thread_mismatch;
static unsigned             wintick_alive_calls;
static uint64_t             wintick_main_thread;

static bool wintick_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   uint64_t self = (uint64_t)sthread_get_current_thread_id();

   if (!wintick_frames)
      wintick_frame_thread = self;
   else if (self != wintick_frame_thread)
      wintick_thread_mismatch++;
   if (self == wintick_main_thread)
      wintick_thread_mismatch++;
   wintick_frames++;

   return wintick_inner->frame(data, frame, width, height,
         frame_count, pitch, msg, video_info);
}

/* Stands in for win32_sizemove_tick(): alive() is handled on the video
 * thread, so a cached-frame present issued from here is issued from
 * exactly where the modal loop issues it. */
static bool wintick_alive(void *data)
{
   wintick_alive_calls++;
   if ((wintick_alive_calls % 4) == 0)
      video_driver_cached_frame();
   return wintick_inner->alive(data);
}

static void lane_window_thread_present(void)
{
   unsigned had = failures;
   video_driver_state_t *video_st = video_state_get_ptr();
   thread_video_t *thr;
   unsigned i;

   wintick_main_thread     = (uint64_t)sthread_get_current_thread_id();
   wintick_frames          = 0;
   wintick_thread_mismatch = 0;
   wintick_alive_calls     = 0;

   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "window-thread present lane");
   thr = (thread_video_t*)video_st->data;

   video_thread_wait_idle();
   wintick_inner          = thr->driver;
   wintick_driver         = *thr->driver;
   wintick_driver.frame   = wintick_frame;
   wintick_driver.alive   = wintick_alive;
   thr->driver            = &wintick_driver;

   /* Frames from the runloop thread while alive() -- on the video
    * thread -- presents cached frames underneath them. */
   for (i = 0; i < 80; i++)
      run_frames(1);
   video_thread_wait_idle();

   CHECK(wintick_frames > 0, "driver frame() never ran");
   CHECK(wintick_alive_calls > 0,
         "alive() never reached the video thread, so no cached frame "
         "was presented from there");
   CHECK(wintick_thread_mismatch == 0,
         "driver frame() ran on %u call(s) from a thread other than "
         "the video thread", wintick_thread_mismatch);

   thr->driver = wintick_inner;
   set_threaded_via_setting(false);

   if (failures == had)
      fprintf(stderr, "[pass] window-thread present lane (%u frames, "
            "%u ticks)\n", wintick_frames, wintick_alive_calls);
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
/* Lane: display pacing must not latch behind a queued frame            */
/*   A driver that presents like a vsynced two-deep swapchain: a frame  */
/*   goes out on the next vblank after the one before it, and a third   */
/*   frame in flight blocks until the first has gone out. Run the loop  */
/*   unpaced for a moment so a frame queues behind another, then turn   */
/*   the hold on: latency must come back under a period and a half and  */
/*   stay there, and the reserve must not grow to a whole period.       */
/* ------------------------------------------------------------------ */

static video_driver_t                vslane_driver;
static const video_driver_t         *vslane_inner;
static video_poke_interface_t        vslane_poke;
static const video_poke_interface_t *vslane_inner_poke;
static retro_time_t                  vslane_period;
static retro_time_t                  vslane_base;      /* vblank grid */
static retro_time_t                  vslane_next_slot; /* next free vblank */
static retro_time_t                  vslane_last_out;  /* last vblank a frame went out on */
static unsigned                      vslane_presents;

static retro_time_t vslane_grid_after(retro_time_t t)
{
   retro_time_t k = (t - vslane_base) / vslane_period + 1;
   return vslane_base + k * vslane_period;
}

static bool vslane_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   retro_time_t now  = cpu_features_get_time_usec();
   retro_time_t slot = vslane_grid_after(now);
   if (slot < vslane_next_slot)
      slot = vslane_next_slot;
   /* Two in flight: this one waits until the earlier has gone out */
   if (slot - now > vslane_period)
   {
      retro_sleep((unsigned)((slot - vslane_period - now) / 1000));
      now = cpu_features_get_time_usec();
      while (now < slot - vslane_period)
         now = cpu_features_get_time_usec();
   }
   vslane_next_slot = slot + vslane_period;
   vslane_last_out  = slot;
   vslane_presents++;
   return vslane_inner->frame(data, frame, width, height, frame_count,
         pitch, msg, video_info);
}

static retro_time_t vslane_last_present(void *data)
{
   retro_time_t now = cpu_features_get_time_usec();
   (void)data;
   /* The last vblank that has passed */
   return vslane_last_out <= now ? vslane_last_out
      : vslane_last_out - vslane_period;
}

static void lane_pacing_queue_drain(void)
{
   unsigned had = failures;
   settings_t *settings = config_get_ptr();
   thread_video_t *thr;
   bool  saved_pacing  = settings->bools.video_threaded_display_pacing;
   bool  saved_ask     = settings->bools.video_present_timing_from_display;
   float saved_refresh = settings->floats.video_refresh_rate;
   retro_time_t avg, worst, render;
   bool from_display;
   unsigned latched = 0, settled = 0, i;

   /* Display at the core's own 60 Hz: one content frame per vblank */
   settings->floats.video_refresh_rate               = 60.0f;
   settings->bools.video_present_timing_from_display = true;
   settings->bools.video_threaded_display_pacing     = false;
   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "pacing-drain lane");
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   CHECK(!menu_is_up(), "pacing-drain lane: menu still up");
   run_frames(3);
   video_thread_wait_idle();

   thr               = (thread_video_t*)video_state_get_ptr()->data;
   vslane_period     = 1000000 / 60;
   vslane_base       = cpu_features_get_time_usec();
   vslane_next_slot  = vslane_base;
   vslane_last_out   = vslane_base;
   vslane_inner      = thr->driver;
   vslane_driver     = *thr->driver;
   vslane_driver.frame = vslane_frame;
   vslane_inner_poke = thr->poke;
   vslane_poke       = *thr->poke;
   vslane_poke.get_last_present_time = vslane_last_present;
   thr->driver       = &vslane_driver;
   thr->poke         = &vslane_poke;

   /* Unpaced, the loop runs ahead of the display and a frame queues
    * behind another: every swap now waits a vblank. */
   run_frames(12);
   video_thread_wait_idle();

   /* The hold comes on with the queue full. It must drain the queue
    * and settle with the frame going out on its own vblank. */
   settings->bools.video_threaded_display_pacing = true;
   for (i = 0; i < 90; i++)
   {
      run_frames(1);
      video_thread_latency_stats(&avg, &worst, &from_display);
      slock_lock(thr->lock);
      render = thr->render_time;
      slock_unlock(thr->lock);
      if (i >= 30)
      {
         if (avg >= vslane_period * 3 / 2)
            latched++;
         else
            settled++;
      }
   }
   video_thread_wait_idle();
   video_thread_latency_stats(&avg, &worst, &from_display);
   slock_lock(thr->lock);
   render = thr->render_time;
   slock_unlock(thr->lock);
   CHECK(settled > latched,
         "pacing-drain lane: latched behind the queued frame: %u of %u "
         "frames at %.1f ms latency, render reserve %.1f ms",
         latched, latched + settled, avg / 1000.0, render / 1000.0);
   CHECK(vslane_presents >= 90,
         "pacing-drain lane: the vsync driver saw only %u presents", vslane_presents);
   CHECK(render < vslane_period / 2,
         "pacing-drain lane: render reserve grew to %.1f ms of a %.1f ms period",
         render / 1000.0, vslane_period / 1000.0);
   fprintf(stderr, "   pacing drain: latency %.1f ms, render reserve %.1f ms, %u/%u settled, %u presents\n",
         avg / 1000.0, render / 1000.0, settled, settled + latched, vslane_presents);

   thr->driver = vslane_inner;
   thr->poke   = vslane_inner_poke;
   settings->bools.video_threaded_display_pacing     = saved_pacing;
   settings->bools.video_present_timing_from_display = saved_ask;
   settings->floats.video_refresh_rate               = saved_refresh;
   if (!menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   set_threaded_via_setting(false);

   if (failures == had)
      fprintf(stderr, "[pass] pacing-drain lane\n");
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

/* A duped frame under the wrapper.
 *
 * A core that has nothing new calls video_refresh with NULL, and the
 * driver repeats what it has. The wrapper's push picked a ring slot for
 * that push like any other, put nothing in it, and the video thread
 * then handed the driver the slot's buffer as the frame: whatever frame
 * had last been copied there, two or more pushes old, or the 0x80 the
 * slots are born with. A core that dupes every other frame - the ffmpeg
 * core playing 30 fps at 60 Hz does - alternated each new frame with an
 * old one: ghosting on Vulkan and D3D12, fades to grey on D3D11.
 *
 * So: real frame, dupe, real frame, dupe, and the driver must see
 * exactly that - pixels, NULL, pixels, NULL. */
#define DUPLANE_PUSHES 6
static video_driver_t        duplane_driver;
static const video_driver_t *duplane_inner;
static int                   duplane_seen[DUPLANE_PUSHES * 4];
static retro_atomic_size_t   duplane_count;
static retro_atomic_size_t   duplane_on;

static bool duplane_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   if (retro_atomic_load_acquire_size(&duplane_on))
   {
      size_t n = retro_atomic_load_acquire_size(&duplane_count);
      if (n < sizeof(duplane_seen) / sizeof(duplane_seen[0]))
      {
         duplane_seen[n] = frame ? (int)*(const uint8_t*)frame : -1;
         retro_atomic_store_release_size(&duplane_count, n + 1);
      }
   }
   /* The inner driver is not given the test pattern to draw. */
   return duplane_inner->frame(data, NULL, width, height, frame_count,
         pitch, msg, video_info);
}

static void lane_dupe_under_wrapper(void)
{
   unsigned had = failures;
   unsigned i;
   size_t seen;
   thread_video_t *thr;
   /* Static, and never freed: video_driver_frame() keeps the pointer
    * it is given as the cached frame, borrowed, the way it keeps a
    * core's - and the menu comes back for it on the next frame it
    * draws. A buffer freed at the end of this lane was read after it
    * was freed by the run_frames() that follows. */
   static uint8_t pix[32 * 32 * sizeof(uint32_t)];
   const unsigned w = 32, h = 32;

   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "dupe lane");
   thr = (thread_video_t*)video_state_get_ptr()->data;
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   CHECK(!menu_is_up(), "dupe lane: menu still up");
   run_frames(2);
   video_thread_wait_idle();

   duplane_inner        = thr->driver;
   duplane_driver       = *thr->driver;
   duplane_driver.frame = duplane_frame;
   thr->driver          = &duplane_driver;
   retro_atomic_store_release_size(&duplane_count, 0);
   retro_atomic_store_release_size(&duplane_on, 1);

   for (i = 0; i < DUPLANE_PUSHES; i++)
   {
      if (i & 1)
         video_driver_frame(NULL, w, h, w * sizeof(uint32_t));
      else
      {
         /* 0x10, 0x20, 0x30: never 0x80, never each other. */
         memset(pix, 0x10 * (int)(i / 2 + 1), sizeof(pix));
         video_driver_frame(pix, w, h, w * sizeof(uint32_t));
      }
      /* One at a time, so no push replaces another in the ring. */
      video_thread_wait_idle();
   }

   retro_atomic_store_release_size(&duplane_on, 0);
   video_thread_wait_idle();
   thr->driver = duplane_inner;

   seen = retro_atomic_load_acquire_size(&duplane_count);
   CHECK(seen == DUPLANE_PUSHES,
         "dupe lane: %u pushes reached the driver as %u frames",
         (unsigned)DUPLANE_PUSHES, (unsigned)seen);
   for (i = 0; i < DUPLANE_PUSHES && i < seen; i++)
   {
      if (i & 1)
         CHECK(duplane_seen[i] == -1,
               "push %u was a dupe (NULL) and the driver was given pixels (0x%02x): "
               "a stale ring slot shown as a new frame", i, duplane_seen[i]);
      else
         CHECK(duplane_seen[i] == 0x10 * (int)(i / 2 + 1),
               "push %u: the driver saw 0x%02x, the core sent 0x%02x",
               i, duplane_seen[i], 0x10 * (int)(i / 2 + 1));
   }

   /* The test pattern is not a frame the lanes after this one should
    * find in the cache. */
   video_driver_cached_frame_invalidate();
   if (!menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_frames(2);
   if (failures == had)
      fprintf(stderr, "[pass] dupe lane (NULL reaches the driver as NULL)\n");
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

/* Lane: the driver's rebuild paths under the wrapper, with a core
 * running. A shader-chain rebuild (set_shader), a font reload and a
 * viewport churn each used to drain the whole queue - every one from
 * under queue_lock on Vulkan - before destroying what a frame in flight
 * still referenced. They now wait on the driver's own fences or park
 * the objects on a deferred list. Frames keep flowing through each,
 * nothing is torn down while a frame still reads it (the validation
 * layer says so on the CI Vulkan lane), and the wrapper comes back
 * intact. */
static void lane_driver_reloads(void)
{
   unsigned had = failures;
   unsigned i;
   video_driver_state_t *video_st = video_state_get_ptr();
   unsigned long long f0;
   unsigned long long ran;

   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "reload lane");
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_frames(3);
   f0 = core_frames();

   for (i = 0; i < 6; i++)
   {
      /* Chain rebuild between two frames, with the previous frame
       * possibly still on the GPU. */
      if (video_st->current_video->set_shader)
         video_st->current_video->set_shader(video_st->data,
               RARCH_SHADER_NONE, NULL);
      run_frames(2);

      /* Font reload: rebuild the OSD font at a new size, as a
       * font-size change does, with the atlas of the old one possibly
       * still bound by a frame in flight. */
      font_driver_reinit_osd(NULL, 12.0f + (float)(i & 1));
      run_frames(2);

      /* Viewport churn: the size change is what tears the swapchain
       * down on a real driver. */
      if (video_st->current_video->set_viewport)
         video_st->current_video->set_viewport(video_st->data,
               320 + 64 * (i & 1), 240 + 48 * (i & 1), true, true);
      run_frames(2);
   }
   video_thread_wait_idle();
   expect_wrapper(true, "after reloads");
   ran = core_frames() - f0;
   CHECK(ran > 0, "reload lane: the frontend accepted no frames");

   if (!menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   set_threaded_via_setting(false);
   run_frames(2);

   if (failures == had)
      fprintf(stderr, "[pass] driver-reload lane (%llu frames through 6 rebuilds)\n",
            ran);
}

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
   /* The fake handles are not the driver's; everything else (the
    * menu's white texture, unloaded while this poke is installed) is,
    * and goes through, or the driver tears down with it still alive -
    * which the Vulkan lane's validation layer reports as a leak. */
   if (id > 0x1000 && id <= 0x1000 + 64)
      return;
   if (async_inner_poke && async_inner_poke->unload_texture)
      async_inner_poke->unload_texture(data, threaded, id);
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
   /* video_st->poke stays the wrapper's own table: the upload reaches
    * the fake through thr->poke on the worker. Pointing video_st->poke
    * at the inner driver's table while video_st->data is the wrapper
    * had every poke the runloop makes between frames - set_texture_enable
    * from runloop_check_state, for one - land on the wrapper struct as
    * if it were the driver's; the null driver has no such poke, so
    * only a real driver showed it, as a write past the wrapper. */

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
   /* The fake poke stays in until the wrapper is gone: a post that the
    * worker already ran holds a handle the fake load made up, and at
    * CMD_FREE the worker hands every completed, undelivered upload
    * back to the driver through thr->poke - through the real driver
    * that would be a made-up handle to vulkan_unload_texture. The
    * copied vtable frees the driver exactly as the original does, and
    * the driver that comes up unthreaded is a fresh instance. */
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

/* ------------------------------------------------------------------ */
/* Lane: streaming surfaces through the real driver                    */
/*   A gfx_surface submits frames to whatever driver is up: under the  */
/*   wrapper a submit is QUEUED and the slot comes back through        */
/*   release() on a later frame, a second submit meanwhile is BUSY;    */
/*   without it a submit is DONE at once. On a driver with an in-place */
/*   update the texture handle never changes across frames; a surface  */
/*   freed with a frame in flight completes quietly. Under lavapipe    */
/*   and the validation layer this is what runs vulkan_update_texture  */
/*   and its stream state for real.                                    */
/* ------------------------------------------------------------------ */

#include "../../../gfx/gfx_surface.h"
#include "../../../gfx/gfx_instrument.h"
#include "../../../input/input_overlay.h"

static unsigned surf_releases;
static unsigned surf_last_slot;

static void surf_release_cb(void *user, gfx_surface_t *s, unsigned slot)
{
   (void)user; (void)s;
   surf_releases++;
   surf_last_slot = slot;
}

static void surf_fill(gfx_surface_t *s, unsigned slot, unsigned seed)
{
   unsigned i, n = s->width * s->height;
   for (i = 0; i < n; i++)
      s->slots[slot][i] = 0xff000000u | ((i * 7u + seed * 31u) & 0xffffffu);
}

static void lane_surface_update(void)
{
   unsigned had = failures;
   gfx_surface_t *s;
   enum gfx_surface_submit_result r;
   bool rgba = (video_driver_get_disp_flags() & VIDEO_FLAG_USE_RGBA) != 0;
   unsigned i;
   unsigned queued;
   uintptr_t first;

   /* Threaded: the descriptor goes to the video thread, the slot
    * stays the surface's until the frame after. */
   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "surface lane");

#ifdef HAVE_GFX_INSTRUMENT
   gfx_instrument_reset();
#endif
   s = gfx_surface_new(64, 48, 2, TEXTURE_FILTER_LINEAR, surf_release_cb, NULL);
   CHECK(s != NULL, "surface allocation failed");
   if (!s)
      return;
   surf_releases = 0;
   surf_fill(s, 0, 0);
   r = gfx_surface_submit(s, 0, rgba);
   CHECK(r == GFX_SURFACE_SUBMIT_QUEUED, "threaded submit returned %d, not QUEUED", r);
   surf_fill(s, 1, 1);
   r = gfx_surface_submit(s, 1, rgba);
   CHECK(r == GFX_SURFACE_SUBMIT_BUSY, "second submit in flight returned %d, not BUSY", r);
   run_frames(2);
   CHECK(surf_releases == 1 && surf_last_slot == 0,
         "first release: %u releases, slot %u", surf_releases, surf_last_slot);
   if (!s->handle)
   {
      /* The null driver has no texture load: nothing more to see. */
      fprintf(stderr, "[skip] surface lane: driver made no texture\n");
      gfx_surface_free(s);
      run_frames(2);
      set_threaded_via_setting(false);
      run_frames(2);
      return;
   }
   first = s->handle;

   /* The first submit above is queued and not yet released. */
   queued = 1;
   for (i = 0; i < 30; i++)
   {
      unsigned slot  = i & 1;
      unsigned tries;
      surf_fill(s, slot, i + 2);
      /* A queued submit is the video thread's until it has taken it,
       * and the slot reads BUSY until the release comes back. One
       * frame is enough on the null driver; a real one runs its own
       * schedule, so give the release the frames it needs. */
      for (tries = 0; tries < 8; tries++)
      {
         r = gfx_surface_submit(s, slot, rgba);
         if (r != GFX_SURFACE_SUBMIT_BUSY)
            break;
         run_frames(1);
      }
      CHECK(r == GFX_SURFACE_SUBMIT_QUEUED, "frame %u: submit returned %d", i, r);
      if (r == GFX_SURFACE_SUBMIT_QUEUED)
         queued++;
      run_frames(1);
   }
   run_frames(4);
   CHECK(surf_releases == queued, "%u releases for %u queued submits",
         surf_releases, queued);
   CHECK(!s->inflight, "a submit is still in flight after the frames");
   if (video_driver_texture_can_update())
      CHECK(s->handle == first,
            "in-place driver replaced the texture (%lx -> %lx)",
            (unsigned long)first, (unsigned long)s->handle);
   else
      fprintf(stderr, "[info] surface lane: driver has no in-place update; "
            "replacement loads exercised\n");

   /* Freed with a frame on its way: the completion frees it. */
   surf_fill(s, 0, 99);
   r = gfx_surface_submit(s, 0, rgba);
   CHECK(r == GFX_SURFACE_SUBMIT_QUEUED, "final submit returned %d", r);
   gfx_surface_free(s);
   run_frames(3);

#ifdef HAVE_GFX_INSTRUMENT
   /* The threaded half on its own: one texture, then an update per
    * frame, each posted as a descriptor. Counted before the mode
    * switch below, whose video reinit loads the menu's own textures
    * through the same counters. */
   {
      int loads   = gfx_instrument_get(GFX_INSTR_TEX_LOAD);
      int updates = gfx_instrument_get(GFX_INSTR_TEX_UPDATE);
      int posts   = gfx_instrument_get(GFX_INSTR_ASYNC_POST);
      int allocs  = gfx_instrument_get(GFX_INSTR_ASYNC_POST_ALLOC);
      int copies  = gfx_instrument_get(GFX_INSTR_SUBMIT_COPY);
      fprintf(stderr, "[baseline] surface, threaded: %d loads, %d updates, "
            "%d posts (%d allocated), %d copies\n",
            loads, updates, posts, allocs, copies);
      CHECK(allocs == 0, "%d of %d posts allocated a node", allocs, posts);
      CHECK(copies == 0, "%d submits copied into a slot", copies);
      if (video_driver_texture_can_update())
      {
         CHECK(loads == 1, "%d texture loads for one streaming surface", loads);
         CHECK(updates > 0, "no in-place update in 32 threaded submits");
      }
   }
#endif
   /* Direct: the submit runs the driver here and now. */
   set_threaded_via_setting(false);
   run_frames(2);
#ifdef HAVE_GFX_INSTRUMENT
   gfx_instrument_reset();
#endif
   expect_wrapper(false, "surface lane, direct");
   s = gfx_surface_new(64, 48, 1, TEXTURE_FILTER_LINEAR, surf_release_cb, NULL);
   CHECK(s != NULL, "direct surface allocation failed");
   if (!s)
      return;
   surf_fill(s, 0, 0);
   r = gfx_surface_submit(s, 0, rgba);
   CHECK(r == GFX_SURFACE_SUBMIT_DONE, "direct submit returned %d, not DONE", r);
   first = s->handle;
   CHECK(first != 0, "direct submit made no texture");
   for (i = 0; i < 10; i++)
   {
      surf_fill(s, 0, i + 1);
      r = gfx_surface_submit(s, 0, rgba);
      CHECK(r == GFX_SURFACE_SUBMIT_DONE, "direct frame %u returned %d", i, r);
      run_frames(1);
   }
   if (video_driver_texture_can_update())
      CHECK(s->handle == first, "direct in-place driver replaced the texture");
   gfx_surface_free(s);
   run_frames(2);

#ifdef HAVE_GFX_INSTRUMENT
   /* What those frames cost, on this driver, counted where it
    * happens: the plan's budget for a streaming surface is one
    * texture for the run, an update a frame where the driver can,
    * no allocation per post, and no canvas copy. */
   {
      int loads   = gfx_instrument_get(GFX_INSTR_TEX_LOAD);
      int updates = gfx_instrument_get(GFX_INSTR_TEX_UPDATE);
      int unloads = gfx_instrument_get(GFX_INSTR_TEX_UNLOAD);
      int posts   = gfx_instrument_get(GFX_INSTR_ASYNC_POST);
      int allocs  = gfx_instrument_get(GFX_INSTR_ASYNC_POST_ALLOC);
      int copies  = gfx_instrument_get(GFX_INSTR_SUBMIT_COPY);
      fprintf(stderr, "[baseline] surface, direct: %d loads, %d updates, "
            "%d unloads, %d posts (%d allocated), %d copies\n",
            loads, updates, unloads, posts, allocs, copies);
      CHECK(allocs == 0, "%d of %d posts allocated a node", allocs, posts);
      CHECK(copies == 0, "%d submits copied into a slot", copies);
      if (video_driver_texture_can_update())
      {
         /* One texture for the surface, kept for every frame of it:
          * this is the budget the whole streaming path exists for. */
         CHECK(loads == 1, "%d texture loads for one direct surface",
               loads);
         /* Every accepted submit updates in place; a submit the
          * driver drops because its own staging is still in flight
          * (Vulkan's two-fence check, D3D11's DO_NOT_WAIT, D3D12's
          * fence tag) returns true without an update, which is the
          * latest-frame policy working, not a miss. What must never
          * happen is a submit that neither updates nor is dropped:
          * that would mean a replacement load, and loads are bounded
          * above. */
         CHECK(updates > 0, "no in-place update in %d submits on a "
               "driver that advertises them", 42);
      }
   }
#endif

   if (failures == had)
      fprintf(stderr, "[pass] surface lane (31 threaded, 11 direct submits)\n");
}

/* ------------------------------------------------------------------ */
/* Lane: an overlay page of the pack's textures through the driver     */
/*   Textures made by video_driver_texture_load() are shown as an      */
/*   overlay page through load_textures(), drawn for a few frames,     */
/*   swapped for another page with no upload, disabled, and only then  */
/*   unloaded - the order input_overlay_free() keeps. Threaded and     */
/*   direct; under validation it is the driver's borrowed-texture      */
/*   page that runs.                                                   */
/* ------------------------------------------------------------------ */

static void lane_overlay_textures_pass(bool threaded)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   const video_overlay_interface_t *iface = NULL;
   static struct texture_image img[3];
   static uint32_t px[3][16 * 16];
   uintptr_t tex[3];
   unsigned i, j;

   set_threaded_via_setting(threaded);
   run_frames(3);
   expect_wrapper(threaded, "overlay lane");

   for (i = 0; i < 3; i++)
   {
      for (j = 0; j < 16 * 16; j++)
         px[i][j] = 0x80000000u | (0x0f0f0fu * (i + 1)) | j;
      img[i].width      = img[i].height = 16;
      img[i].pixels     = px[i];
      img[i].compressed = NULL;
      img[i].pix10      = false;
      tex[i] = 0;
      if (!video_driver_texture_load(&img[i], TEXTURE_FILTER_LINEAR, &tex[i]))
         tex[i] = 0;
   }
   if (!tex[0])
   {
      fprintf(stderr, "[skip] overlay lane (%s): driver made no texture\n",
            threaded ? "threaded" : "direct");
      return;
   }
   if (video_st->current_video && video_st->current_video->overlay_interface)
      video_st->current_video->overlay_interface(video_st->data, &iface);
   if (!iface || !iface->load_textures)
   {
      fprintf(stderr, "[skip] overlay lane (%s): no load_textures\n",
            threaded ? "threaded" : "direct");
      for (i = 0; i < 3; i++)
         if (tex[i])
            video_driver_texture_unload(&tex[i]);
      return;
   }

   /* Page one: two of the textures. */
   CHECK(iface->load_textures(video_st->data, tex, 2),
         "load_textures refused page one (%s)", threaded ? "threaded" : "direct");
   iface->enable(video_st->data, true);
   for (i = 0; i < 2; i++)
   {
      iface->tex_geom(video_st->data, i, 0, 0, 1, 1);
      iface->vertex_geom(video_st->data, i, 0.1f * i, 0.1f * i, 0.4f, 0.4f);
      iface->set_alpha(video_st->data, i, 0.75f);
   }
   run_frames(4);
   /* Page two: a different set of the same textures, no upload. */
   CHECK(iface->load_textures(video_st->data, tex + 1, 2),
         "load_textures refused page two (%s)", threaded ? "threaded" : "direct");
   for (i = 0; i < 2; i++)
   {
      iface->tex_geom(video_st->data, i, 0, 0, 1, 1);
      iface->vertex_geom(video_st->data, i, 0.5f, 0.1f * i, 0.3f, 0.3f);
      iface->set_alpha(video_st->data, i, 1.0f);
   }
   run_frames(4);
   iface->enable(video_st->data, false);
   run_frames(2);
   for (i = 0; i < 3; i++)
      if (tex[i])
         video_driver_texture_unload(&tex[i]);
   run_frames(3);
}

static void lane_overlay_textures(void)
{
   unsigned had = failures;
#ifdef HAVE_GFX_INSTRUMENT
   gfx_instrument_reset();
#endif
   lane_overlay_textures_pass(true);
   lane_overlay_textures_pass(false);
#ifdef HAVE_GFX_INSTRUMENT
   {
      int loads   = gfx_instrument_get(GFX_INSTR_TEX_LOAD);
      int unloads = gfx_instrument_get(GFX_INSTR_TEX_UNLOAD);
      int kib     = gfx_instrument_get(GFX_INSTR_OVERLAY_PIXEL_KIB);
      fprintf(stderr, "[baseline] overlay: %d loads, %d unloads for "
            "4 pages over 2 passes, %d KiB of pack pixels held\n",
            loads, unloads, kib);
      /* What a pack holds in system memory after its textures exist
       * is what a release-after-upload would save and a video reinit
       * would have to decode again; the lane's own images are tiny,
       * so this is a check that the accounting balances, not a
       * measurement of a real pack. */
      CHECK(kib >= 0, "pack pixel accounting went negative (%d KiB)", kib);
      /* Three images a pass, uploaded once each, and a page switch
       * adds nothing - but the menu's own textures are loaded and
       * unloaded through the same counters while these frames run,
       * so the bound is on the order, not the exact count: six
       * uploads plus the handful the menu makes, never one per page
       * switch (which would be twelve and climbing with the frames). */
      CHECK(loads <= 10, "%d texture loads for 6 overlay images: "
            "a page switch is uploading", loads);
   }
#endif
   if (failures == had)
      fprintf(stderr, "[pass] overlay page lane\n");
}


/* ------------------------------------------------------------------ */
/* Lane: a 4K surface streamed for sixty frames                        */
/*   Section 15 of the surface plan asks for the numbers, not the      */
/*   argument: a 3840x2160 surface under the wrapper, sixty submits,   */
/*   the time each submit takes on the main thread and the frames it   */
/*   takes for the slot to come back. The submit must stay a          */
/*   descriptor hand-off - microseconds, not a 33 MB copy - and the    */
/*   counters must show no allocation per post, no copy, and one       */
/*   texture for the run. Latencies are printed as p50/p95/p99 for     */
/*   the record; the check is on the budgets, since the clock here    */
/*   is a software rasteriser's.                                       */
/* ------------------------------------------------------------------ */

/* The time this thread spent, not the time that passed: on a host
 * with one core the video thread's 33 MB upload preempts the main
 * thread in the middle of a submit, and wall clock would charge that
 * to the hand-off. Thread CPU time charges only what the submit
 * itself did. */
static int64_t thread_cpu_usec(void)
{
#if defined(CLOCK_THREAD_CPUTIME_ID)
   struct timespec ts;
   if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0)
      return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
#endif
   return cpu_features_get_time_usec();
}

static int cmp_i64(const void *a, const void *b)
{
   int64_t x = *(const int64_t*)a, y = *(const int64_t*)b;
   return (x > y) - (x < y);
}

static void lane_surface_4k(void)
{
   unsigned had = failures;
   gfx_surface_t *s;
   bool rgba = (video_driver_get_disp_flags() & VIDEO_FLAG_USE_RGBA) != 0;
   int64_t submit_us[60];
   int64_t release_frames[60];
   unsigned i, n_sub = 0, n_rel = 0;
   unsigned frame_now = 0;
   uintptr_t first    = 0;

   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "4k surface lane");

   s = gfx_surface_new(3840, 2160, 2, TEXTURE_FILTER_LINEAR,
         surf_release_cb, NULL);
   CHECK(s != NULL, "4K surface allocation failed");
   if (!s)
      return;
   surf_releases = 0;
#ifdef HAVE_GFX_INSTRUMENT
   gfx_instrument_reset();
#endif

   for (i = 0; i < 60; i++)
   {
      unsigned slot = i & 1;
      int64_t t0, t1;
      unsigned before = surf_releases;
      unsigned waited = 0;
      enum gfx_surface_submit_result r;

      /* Only the top-left tile is touched: what is measured is the
       * hand-off, not the fill. */
      s->slots[slot][0] = 0xff000000u | i;
      t0 = thread_cpu_usec();
      r  = gfx_surface_submit(s, slot, rgba);
      t1 = thread_cpu_usec();
      if (r == GFX_SURFACE_SUBMIT_BUSY)
      {
         run_frames(1);
         frame_now++;
         continue;
      }
      CHECK(r == GFX_SURFACE_SUBMIT_QUEUED, "4K frame %u: submit returned %d", i, r);
      submit_us[n_sub++] = t1 - t0;
      while (surf_releases == before && waited < 8)
      {
         run_frames(1);
         frame_now++;
         waited++;
      }
      CHECK(waited < 8, "4K frame %u: slot not released within 8 frames", i);
      release_frames[n_rel++] = waited;
      if (!first)
         first = s->handle;
   }
   run_frames(2);

   if (n_sub)
   {
      qsort(submit_us, n_sub, sizeof(submit_us[0]), cmp_i64);
      qsort(release_frames, n_rel, sizeof(release_frames[0]), cmp_i64);
      fprintf(stderr, "[baseline] 4k surface: %u submits, submit us "
            "p50 %lld p95 %lld p99 %lld; slot back in frames "
            "p50 %lld p95 %lld p99 %lld\n", n_sub,
            (long long)submit_us[n_sub / 2],
            (long long)submit_us[(n_sub * 95) / 100],
            (long long)submit_us[(n_sub * 99) / 100],
            (long long)release_frames[n_rel / 2],
            (long long)release_frames[(n_rel * 95) / 100],
            (long long)release_frames[(n_rel * 99) / 100]);
      /* A submit hands over a descriptor. A 4K copy on this thread
       * would be milliseconds of its own CPU time; the budget leaves
       * room for a slow host and none for a copy. */
      CHECK(submit_us[(n_sub * 99) / 100] < 2000,
            "4K submit p99 is %lld us: that is a copy, not a hand-off",
            (long long)submit_us[(n_sub * 99) / 100]);
   }
   CHECK(n_sub >= 30, "only %u of 60 4K submits were accepted", n_sub);
   if (s->handle && video_driver_texture_can_update())
      CHECK(s->handle == first, "4K in-place driver replaced the texture");

#ifdef HAVE_GFX_INSTRUMENT
   {
      int allocs = gfx_instrument_get(GFX_INSTR_ASYNC_POST_ALLOC);
      int copies = gfx_instrument_get(GFX_INSTR_SUBMIT_COPY);
      int loads  = gfx_instrument_get(GFX_INSTR_TEX_LOAD);
      fprintf(stderr, "[baseline] 4k surface: %d loads, %d allocating "
            "posts, %d copies\n", loads, allocs, copies);
      CHECK(allocs == 0, "4K: %d posts allocated", allocs);
      CHECK(copies == 0, "4K: %d submits copied", copies);
      if (video_driver_texture_can_update())
         CHECK(loads <= 1, "4K: %d texture loads for one streaming surface", loads);
   }
#endif

   gfx_surface_free(s);
   run_frames(3);
   set_threaded_via_setting(false);
   run_frames(2);
   if (failures == had)
      fprintf(stderr, "[pass] 4k surface lane (%u submits)\n", n_sub);
}

int main(int argc, char *argv[])
{
   char cfg_path[512];
   char dir[400];
   /* NULL past the last argument, as a real main()'s argv is: the
    * option parser reads up to that. */
   char *rarch_argv[8] = {0};
   int rarch_argc = 0;
   FILE *cfg;
   char core_path[512];
   unsigned cycles = argc > 1 ? (unsigned)atoi(argv[1]) : 10;

   /* A scratch directory of our own. path_mkdir() rather than a
    * shell: on Windows system() is cmd.exe, which has no mkdir -p and
    * reads /tmp as a switch. TMPDIR, then TEMP (Windows), then /tmp. */
   {
      const char *tmp = getenv("TMPDIR");
      if (!tmp || !*tmp)
         tmp = getenv("TEMP");
      if (!tmp || !*tmp)
         tmp = "/tmp";
      snprintf(dir, sizeof(dir), "%s/threaded_video_harness_%ld",
            tmp, (long)getpid());
   }
   if (!path_mkdir(dir))
      return 1;

   snprintf(cfg_path, sizeof(cfg_path), "%s/harness.cfg", dir);
   if ((cfg = fopen(cfg_path, "wb")))
   {
      /* The null driver by default. A real driver from the
       * environment runs the same lanes through a real context: the
       * CI Vulkan lane names "vulkan" and runs on lavapipe under the
       * validation layer, so destroying an image or a buffer a frame
       * still reads is a logged validation error rather than luck. */
      fprintf(cfg, "video_driver = \"%s\"\n",
            getenv("HARNESS_VIDEO_DRIVER")
            ? getenv("HARNESS_VIDEO_DRIVER") : "null");
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

   /* Frontend logging on request: the CI Vulkan lane reads the
    * validation layer's reports off RARCH_ERR and fails on any. */
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
   /* Frontend logging on request: a real driver under the validation
    * layer reports through RARCH_ERR, and the CI Vulkan lane reads
    * those lines. */
   if (getenv("HARNESS_VERBOSE"))
      rarch_argv[rarch_argc++] = (char*)"-v";

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
   if (!real_driver())
      lane_present_repeat();
   lane_every_command_replies();
   lane_second_ring_waiter();
   lane_reentrant_from_frame();
   lane_window_thread_present();
   if (!real_driver())
      lane_display_phase();
   lane_command_runs_once();
   lane_font_marshal();
   if (!real_driver())
   {
      lane_display_pacing();
      lane_pacing_queue_drain();
   }
   lane_zero_copy();
   lane_surface_update();
   if (real_driver())
      lane_surface_4k();
   lane_overlay_textures();
   lane_driver_reloads();
   if (!real_driver())
   {
      lane_waiter_call();
      lane_frame_path_heap();
      lane_resize_under_wrapper();
      lane_dupe_under_wrapper();
   }
   else
      fprintf(stderr, "[skip] null-driver instrumented lanes (real driver: %s)\n",
            getenv("HARNESS_VIDEO_DRIVER"));

   /* Orderly shutdown: the teardown barriers are part of what is
    * under test. */
   set_threaded_via_setting(true);
   run_frames(3);
   main_exit(NULL);

   /* The scratch directory holds the config and nothing else. */
   remove(cfg_path);
   rmdir(dir);

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   fprintf(stderr, "all lanes passed (frontend accepted %llu core frames)\n", (unsigned long long)core_frames());
   return 0;
}

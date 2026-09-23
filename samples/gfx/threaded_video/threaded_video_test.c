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
#include <stddef.h>
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
#include "../../../input/input_driver.h"

#ifdef HAVE_X11
#include <X11/Xlib.h>
#include "../../../gfx/common/x11_common.h"
#endif

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
/* Lane: the viewport-parameter publish only writes on a change       */
/*   The snapshot is published once per frame as a catch-all for       */
/*   settings toggles, and the video thread reads it every frame. A    */
/*   publish that stores unconditionally takes the line away from that */
/*   reader sixty times a second to say nothing, so it compares first. */
/*   Nothing else notices if the compare goes: the values are still    */
/*   right, every lane passes, and the line goes back to moving every  */
/*   frame. So the sequence counter is watched instead - it is the one */
/*   thing that stands still exactly when the publish did nothing.     */
/* ------------------------------------------------------------------ */

/* The aspect index as the flag word in slot 0 carries it: six bits
 * above the scale_integer bit (VIDEO_VP_ASPECT_IDX_SHIFT/_BITS in
 * video_driver.c). */
static int published_aspect_idx(video_driver_state_t *video_st)
{
   return (int)(((unsigned)retro_atomic_load_relaxed_int(
               &video_st->vp_params_bits[0]) >> 1) & 0x3fu);
}

static void lane_vp_params_publish(void)
{
   unsigned had = failures;
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   int seq0, seq1;
   unsigned saved_idx;

   /* Settle: the first frames publish whatever the boot left unset. */
   run_frames(6);
   seq0 = retro_atomic_load_acquire_int(&video_st->vp_params_seq);
   run_frames(10);
   seq1 = retro_atomic_load_acquire_int(&video_st->vp_params_seq);
   CHECK(seq1 == seq0,
         "the viewport parameters were published %d times over 10 frames "
         "with nothing changed", (seq1 - seq0) / 2);

   /* And it does still publish when something moves: the aspect index
    * rides in the flag word, slot 0. */
   saved_idx = settings->uints.video_aspect_ratio_idx;
   settings->uints.video_aspect_ratio_idx =
      saved_idx ? saved_idx - 1 : saved_idx + 1;
   run_frames(2);
   seq1 = retro_atomic_load_acquire_int(&video_st->vp_params_seq);
   CHECK(seq1 > seq0,
         "a changed viewport parameter was not published (sequence still "
         "%d)", seq1);
   /* The change reached the snapshot, not just the counter. */
   {
      unsigned tries;
      int want = (int)settings->uints.video_aspect_ratio_idx;
      for (tries = 0; tries < 60; tries++)
      {
         if (published_aspect_idx(video_st) == want)
            break;
         run_frames(1);
      }
      CHECK(published_aspect_idx(video_st) == want,
            "the published aspect index is %d, the setting is %d",
            published_aspect_idx(video_st), want);
   }
   settings->uints.video_aspect_ratio_idx = saved_idx;
   run_frames(4);

   /* And still still after the change: back to standing still. */
   seq0 = retro_atomic_load_acquire_int(&video_st->vp_params_seq);
   run_frames(10);
   seq1 = retro_atomic_load_acquire_int(&video_st->vp_params_seq);
   CHECK(seq1 == seq0,
         "the publish did not settle again: %d publishes over 10 quiet "
         "frames", (seq1 - seq0) / 2);

   if (failures == had)
      fprintf(stderr, "[pass] viewport-parameter publish lane\n");
}

/* ------------------------------------------------------------------ */
/* Lane: the cache lines that must not be shared                      */
/*   Fields written by different threads are kept off each other's     */
/*   lines by padding in thread_video_t. The padding is what makes     */
/*   the separation, and nothing else notices when a field added       */
/*   above one of the pads moves the field below it back onto the      */
/*   shared line - the suite passes and the throughput goes. So the    */
/*   separations are asserted by offset, on the struct this build      */
/*   actually has.                                                     */
/* ------------------------------------------------------------------ */

#define LINE_OF(f) (offsetof(thread_video_t, f) / VIDEO_THREAD_LINE)

static void lane_line_separation(void)
{
   unsigned had = failures;

   /* The snapshots the video thread publishes every frame against the
    * command packet a sender rewrites for every synchronous command. */
   CHECK(LINE_OF(refresh_rate_bits) != LINE_OF(cmd_data),
         "refresh_rate_bits and cmd_data share line %u",
         (unsigned)LINE_OF(cmd_data));
   CHECK(LINE_OF(stats) != LINE_OF(cmd_data),
         "the statistics slots and cmd_data share line %u",
         (unsigned)LINE_OF(cmd_data));
   CHECK(LINE_OF(vp_pub) != LINE_OF(cmd_data),
         "the viewport slots and cmd_data share line %u",
         (unsigned)LINE_OF(cmd_data));

   /* The per-frame published flags, and worker_running after them
    * before the pad, against the main thread's own. */
   CHECK(LINE_OF(win_flags) != LINE_OF(nonblock),
         "win_flags and nonblock share line %u",
         (unsigned)LINE_OF(nonblock));
   CHECK(LINE_OF(worker_running) != LINE_OF(nonblock),
         "worker_running and nonblock share line %u",
         (unsigned)LINE_OF(nonblock));
   CHECK(LINE_OF(worker_running) != LINE_OF(deferred_head),
         "worker_running and deferred_head share line %u",
         (unsigned)LINE_OF(deferred_head));

   /* The deferred ring's producer and consumer indices. */
   CHECK(LINE_OF(deferred_head) != LINE_OF(deferred_tail),
         "deferred_head and deferred_tail share line %u",
         (unsigned)LINE_OF(deferred_head));

   if (failures == had)
      fprintf(stderr, "[pass] cache-line separation lane "
            "(struct %u bytes, %u lines)\n",
            (unsigned)sizeof(thread_video_t),
            (unsigned)((sizeof(thread_video_t) + VIDEO_THREAD_LINE - 1)
               / VIDEO_THREAD_LINE));
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
/* Lane: the statistics snapshot                                      */
/*   The overlay's numbers are published as a seqlock, so the readers  */
/*   take no lock. Two things about the publication have to hold: what */
/*   a reader gets is what the field holds, field by field; and the    */
/*   64-bit counters, which travel as two int-wide halves, survive a   */
/*   carry out of the low half.                                        */
/*                                                                    */
/*   Ground truth is the field read under the wrapper's own lock. The  */
/*   video thread can publish between the two reads, so each           */
/*   comparison is bracketed: truth, snapshot, truth again, and only   */
/*   an unchanged bracket is asserted on.                              */
/* ------------------------------------------------------------------ */

/* Every published field against the field it came from. Callable from
 * any lane with the wrapper up, and meant to be: a flag bit dropped
 * from the word is invisible while its source is false, so the lanes
 * that drive one of these bools true call this at the end. */
static void stats_snapshot_check(const char *when)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   thread_video_t *thr;
   unsigned tries;
   bool     settled = false;

   if (     !video_st->thread_wrapper_active
         || !(thr = (thread_video_t*)video_st->data))
      return;

   for (tries = 0; tries < 64 && !settled; tries++)
   {
      uint64_t     t_repeats, t_swaps, t_repeats2, t_swaps2;
      retro_time_t t_avg, t_max, t_core, t_render;
      retro_time_t t_avg2, t_max2, t_core2, t_render2;
      bool         t_armed, t_phase, t_latdisp, t_pacing;
      bool         t_armed2, t_phase2, t_latdisp2, t_pacing2;
      uint64_t     s_repeats, s_swaps;
      retro_time_t s_avg, s_max, s_core, s_render;
      bool         s_armed, s_phase, s_latdisp, s_pacing;

      video_thread_wait_idle();
      slock_lock(thr->lock);
      t_repeats = thr->frames_repeated;
      t_swaps   = video_st->swap_count;
      t_avg     = thr->latency_avg;
      t_max     = thr->latency_max;
      t_core    = thr->core_time;
      t_render  = thr->render_time;
      t_armed   = thr->present_repeat;
      t_phase   = thr->phase_from_display;
      t_latdisp = thr->latency_from_display;
      t_pacing  = thr->display_pacing;
      slock_unlock(thr->lock);

      s_armed   = video_thread_presenter_stats(&s_repeats, &s_phase);
      video_thread_latency_stats(&s_avg, &s_max, &s_latdisp);
      video_thread_pacing_stats(&s_pacing, &s_core, &s_render);
      s_swaps   = video_thread_swap_count();

      slock_lock(thr->lock);
      t_repeats2 = thr->frames_repeated;
      t_swaps2   = video_st->swap_count;
      t_avg2     = thr->latency_avg;
      t_max2     = thr->latency_max;
      t_core2    = thr->core_time;
      t_render2  = thr->render_time;
      t_armed2   = thr->present_repeat;
      t_phase2   = thr->phase_from_display;
      t_latdisp2 = thr->latency_from_display;
      t_pacing2  = thr->display_pacing;
      slock_unlock(thr->lock);

      if (     t_repeats != t_repeats2 || t_swaps   != t_swaps2
            || t_avg     != t_avg2     || t_max     != t_max2
            || t_core    != t_core2    || t_render  != t_render2
            || t_armed   != t_armed2   || t_phase   != t_phase2
            || t_latdisp != t_latdisp2 || t_pacing  != t_pacing2)
         continue;                    /* a publish landed; take another */

      settled = true;
      CHECK(s_repeats == t_repeats, "%s: published repeats %llu, field %llu",
            when, (unsigned long long)s_repeats, (unsigned long long)t_repeats);
      CHECK(s_swaps   == t_swaps,   "%s: published swaps %llu, field %llu",
            when, (unsigned long long)s_swaps, (unsigned long long)t_swaps);
      CHECK(s_avg     == t_avg,     "%s: published latency avg %lld, field %lld",
            when, (long long)s_avg, (long long)t_avg);
      CHECK(s_max     == t_max,     "%s: published latency worst %lld, field %lld",
            when, (long long)s_max, (long long)t_max);
      CHECK(s_core    == t_core,    "%s: published core time %lld, field %lld",
            when, (long long)s_core, (long long)t_core);
      CHECK(s_render  == t_render,  "%s: published render time %lld, field %lld",
            when, (long long)s_render, (long long)t_render);
      CHECK(s_armed   == t_armed,   "%s: published repeat armed %d, field %d",
            when, (int)s_armed, (int)t_armed);
      CHECK(s_phase   == t_phase,   "%s: published display phase %d, field %d",
            when, (int)s_phase, (int)t_phase);
      CHECK(s_latdisp == t_latdisp, "%s: published latency source %d, field %d",
            when, (int)s_latdisp, (int)t_latdisp);
      CHECK(s_pacing  == t_pacing,  "%s: published display pacing %d, field %d",
            when, (int)s_pacing, (int)t_pacing);
   }
   CHECK(settled, "%s: the published snapshot never settled against the "
         "fields", when);
}

/* The statistics text is appended to a line at a time; a buffer the
 * lines do not fit in must end terminated at its last byte, with
 * nothing written past it and nothing more taken. */
static void lane_stat_text_bounds(void)
{
   unsigned had = failures;
   static char buf[VIDEO_STAT_TEXT_SIZE + 64];
   size_t len    = 0;
   unsigned i;

   memset(buf, 0x5a, sizeof(buf));
   for (i = 0; i < 200; i++)
      len = video_driver_stat_appendf(buf, len,
            " Line %04u: %s\n", i, "a statistics line of some length");
   CHECK(len == VIDEO_STAT_TEXT_SIZE - 1,
         "an overfull statistics text ended at %u, not %u",
         (unsigned)len, (unsigned)(VIDEO_STAT_TEXT_SIZE - 1));
   CHECK(buf[VIDEO_STAT_TEXT_SIZE - 1] == '\0',
         "an overfull statistics text is not terminated");
   CHECK((unsigned char)buf[VIDEO_STAT_TEXT_SIZE] == 0x5a,
         "a statistics append wrote past the buffer");
   CHECK(video_driver_stat_appendf(buf, len, "%s", "more") == len,
         "a full statistics buffer took more text");

   if (failures == had)
      fprintf(stderr, "[pass] stat text bounds lane\n");
}

static void lane_stats_snapshot(void)
{
   unsigned had = failures;
   video_driver_state_t *video_st = video_state_get_ptr();
   thread_video_t *thr;

   set_threaded_via_setting(true);
   run_frames(8);
   expect_wrapper(true, "stats snapshot lane");
   if (!(thr = (thread_video_t*)video_st->data))
      return;
   stats_snapshot_check("stats snapshot lane");

   /* The carry. The count is put on the wire as two int-wide halves, so
    * seed it two short of the boundary and let the frames take it
    * across. */
   {
      uint64_t saved, truth, seen;
      video_thread_wait_idle();
      slock_lock(thr->lock);
      saved                = video_st->swap_count;
      video_st->swap_count = 0xFFFFFFFEull;
      slock_unlock(thr->lock);
      run_frames(12);
      video_thread_wait_idle();
      seen = video_thread_swap_count();
      slock_lock(thr->lock);
      truth = video_st->swap_count;
      slock_unlock(thr->lock);
      CHECK(truth > 0xFFFFFFFFull,
            "the count did not cross the carry (%llu)",
            (unsigned long long)truth);
      CHECK(seen >= 0x100000000ull && truth - seen <= 4,
            "across the carry the count read %llu, field %llu",
            (unsigned long long)seen, (unsigned long long)truth);
      slock_lock(thr->lock);
      video_st->swap_count = saved;
      slock_unlock(thr->lock);
   }

   set_threaded_via_setting(false);
   run_frames(2);

   if (failures == had)
      fprintf(stderr, "[pass] stats snapshot lane\n");
}

/* ------------------------------------------------------------------ */
/* Lane: the viewport and the rate, published and read without a lock */
/*   Both come from the wrapped driver on the video thread, and both   */
/*   are read from elsewhere - an input driver asks for the viewport    */
/*   every poll, the runloop asks for the rate every iteration. So two  */
/*   things: what the reader gets is what the driver reported, and the  */
/*   read does not touch the wrapper's lock.                            */
/*                                                                    */
/*   The second is asserted directly. A helper thread holds that lock   */
/*   for a window, and the readers are timed inside it: a reader that   */
/*   still takes the lock is delayed by the rest of the window, which   */
/*   fails the check, instead of deadlocking the run.                   */
/* ------------------------------------------------------------------ */

#define VPLANE_HOLD_MS   200
#define VPLANE_BUDGET_US (VPLANE_HOLD_MS * 1000 / 4)

static video_driver_t                 vplane_driver;
static const video_driver_t          *vplane_inner;
static video_poke_interface_t          vplane_poke;
static const video_poke_interface_t  *vplane_inner_poke;
/* What the fake driver reports. Written by the lane between frames with
 * the worker idle, read on the video thread. */
static retro_atomic_int_t vplane_w, vplane_h, vplane_rate_milli;
static retro_atomic_int_t vplane_held;

static void vplane_viewport_info(void *data, struct video_viewport *vp)
{
   (void)data;
   vp->pos         = VIDEO_POS_PACK(3, 5);
   vp->dims        = VIDEO_SCALE_PACK((unsigned)retro_atomic_load_acquire_int(&vplane_w),
         (unsigned)retro_atomic_load_acquire_int(&vplane_h));
   vp->full_dims   = VIDEO_SCALE_PACK(VIDEO_SCALE_W(vp->dims)  + 7, VIDEO_SCALE_H(vp->dims) + 9);
}

static float vplane_refresh(void *data)
{
   (void)data;
   return retro_atomic_load_acquire_int(&vplane_rate_milli) / 1000.0f;
}

static void vplane_get_poke(void *data, const video_poke_interface_t **iface)
{
   vplane_inner->poke_interface(data, &vplane_inner_poke);
   vplane_poke                  = *vplane_inner_poke;
   vplane_poke.get_refresh_rate = vplane_refresh;
   *iface                       = &vplane_poke;
}

/* Holds the wrapper's lock for the window, so the timed readers below
 * run against a lock that is genuinely taken. */
static void vplane_holder(void *data)
{
   thread_video_t *thr = (thread_video_t*)data;
   slock_lock(thr->lock);
   retro_atomic_store_release_int(&vplane_held, 1);
   retro_sleep(VPLANE_HOLD_MS);
   slock_unlock(thr->lock);
   retro_atomic_store_release_int(&vplane_held, 0);
}

/* The viewport the frontend is told, and whether read_vp followed it -
 * that is what CMD_READ_VIEWPORT compares against, so a screenshot's
 * readback depends on it. */
static void vplane_expect(thread_video_t *thr, unsigned w, unsigned h,
      const char *when)
{
   struct video_viewport vp;
   unsigned tries;
   memset(&vp, 0, sizeof(vp));
   for (tries = 0; tries < 200; tries++)
   {
      run_frames(1);
      video_thread_wait_idle();
      video_driver_get_viewport_info(&vp);
      if (VIDEO_SCALE_W(vp.dims) == w && VIDEO_SCALE_H(vp.dims) == h)
         break;
   }
   CHECK(VIDEO_SCALE_W(vp.dims) == w && VIDEO_SCALE_H(vp.dims) == h,
         "%s: the viewport read %ux%u, the driver reported %ux%u",
         when, VIDEO_SCALE_W(vp.dims), VIDEO_SCALE_H(vp.dims), w, h);
   CHECK(VIDEO_POS_X(vp.pos) == 3 && VIDEO_POS_Y(vp.pos) == 5,
         "%s: the viewport's origin read %d,%d, not 3,5", when, VIDEO_POS_X(vp.pos), VIDEO_POS_Y(vp.pos));
   CHECK(VIDEO_SCALE_W(vp.full_dims) == w + 7 && VIDEO_SCALE_H(vp.full_dims) == h + 9,
         "%s: the full size read %ux%u, not %ux%u", when,
         VIDEO_SCALE_W(vp.full_dims), VIDEO_SCALE_H(vp.full_dims), w + 7, h + 9);
   CHECK(VIDEO_SCALE_W(thr->read_vp.dims) == VIDEO_SCALE_W(vp.dims) && VIDEO_SCALE_H(thr->read_vp.dims) == VIDEO_SCALE_H(vp.dims)
         && VIDEO_POS_X(thr->read_vp.pos) == VIDEO_POS_X(vp.pos) && VIDEO_POS_Y(thr->read_vp.pos) == VIDEO_POS_Y(vp.pos)
         && VIDEO_SCALE_W(thr->read_vp.full_dims)  == VIDEO_SCALE_W(vp.full_dims)
         && VIDEO_SCALE_H(thr->read_vp.full_dims) == VIDEO_SCALE_H(vp.full_dims),
         "%s: read_vp did not follow the reported viewport (%ux%u vs %ux%u)",
         when, VIDEO_SCALE_W(thr->read_vp.dims), VIDEO_SCALE_H(thr->read_vp.dims), VIDEO_SCALE_W(vp.dims), VIDEO_SCALE_H(vp.dims));
}

static void lane_viewport_publish(void)
{
   unsigned had = failures;
   video_driver_state_t *video_st = video_state_get_ptr();
   thread_video_t *thr;
   sthread_t *holder;

   set_threaded_via_setting(true);
   run_frames(4);
   expect_wrapper(true, "viewport lane");
   if (!(thr = (thread_video_t*)video_st->data))
      return;

   retro_atomic_store_release_int(&vplane_w, 321);
   retro_atomic_store_release_int(&vplane_h, 241);
   retro_atomic_store_release_int(&vplane_rate_milli, 59940);
   video_thread_wait_idle();
   vplane_inner                = thr->driver;
   vplane_driver               = *thr->driver;
   vplane_driver.viewport_info = vplane_viewport_info;
   vplane_driver.poke_interface = vplane_get_poke;
   thr->driver                 = &vplane_driver;
   vplane_driver.poke_interface(thr->driver_data, &thr->poke);

   vplane_expect(thr, 321, 241, "first report");

   /* A resize: the published viewport has to follow, and read_vp with
    * it. */
   retro_atomic_store_release_int(&vplane_w, 640);
   retro_atomic_store_release_int(&vplane_h, 480);
   vplane_expect(thr, 640, 480, "after a resize");

   /* The rate the main thread is told is the driver's. */
   {
      unsigned tries;
      float rate = 0.0f;
      for (tries = 0; tries < 200; tries++)
      {
         run_frames(1);
         video_thread_wait_idle();
         /* Through the wrapper's own poke, which is what the frontend
          * holds - not the wrapped driver's, which is the fake above. */
         rate = video_st->poke && video_st->poke->get_refresh_rate
            ? video_st->poke->get_refresh_rate(video_st->data) : 0.0f;
         if (rate > 59.9f && rate < 59.95f)
            break;
      }
      CHECK(rate > 59.9f && rate < 59.95f,
            "the rate read %.3f, the driver reported 59.940", (double)rate);
      retro_atomic_store_release_int(&vplane_rate_milli, 100000);
      for (tries = 0; tries < 200; tries++)
      {
         run_frames(1);
         video_thread_wait_idle();
         rate = video_st->poke->get_refresh_rate(video_st->data);
         if (rate > 99.9f && rate < 100.1f)
            break;
      }
      CHECK(rate > 99.9f && rate < 100.1f,
            "after the driver changed its rate the read gave %.3f, not 100",
            (double)rate);
   }

   /* No lock. The viewport has not changed since the last read above,
    * which is the steady state an input driver polls in. */
   {
      retro_time_t t0, took;
      struct video_viewport vp;
      uint64_t     repeats, swaps;
      retro_time_t avg, worst, core_t, render_t;
      bool         phase, latdisp, pacing;
      float        rate;

      video_thread_wait_idle();
      video_driver_get_viewport_info(&vp);      /* read_vp is current */
      retro_atomic_store_release_int(&vplane_held, 0);
      if (!(holder = sthread_create(vplane_holder, thr)))
         CHECK(false, "could not start the lock holder");
      else
      {
         unsigned spins = 0;
         while (!retro_atomic_load_acquire_int(&vplane_held) && spins++ < 5000)
            retro_sleep(1);
         CHECK(retro_atomic_load_acquire_int(&vplane_held),
               "the lock holder never took the lock");

         t0   = cpu_features_get_time_usec();
         video_driver_get_viewport_info(&vp);
         rate = video_st->poke->get_refresh_rate(video_st->data);
         (void)video_thread_presenter_stats(&repeats, &phase);
         (void)video_thread_latency_stats(&avg, &worst, &latdisp);
         (void)video_thread_pacing_stats(&pacing, &core_t, &render_t);
         swaps = video_thread_swap_count();
         took  = cpu_features_get_time_usec() - t0;

         sthread_join(holder);
         CHECK(took < VPLANE_BUDGET_US,
               "the viewport, the rate and the statistics took %lld us to "
               "read while the wrapper's lock was held: one of them still "
               "takes it", (long long)took);
         /* The values are still the driver's, not zeroed by the timing
          * path above. */
         CHECK(VIDEO_SCALE_W(vp.dims) == 640 && VIDEO_SCALE_H(vp.dims) == 480,
               "the timed read gave %ux%u", VIDEO_SCALE_W(vp.dims), VIDEO_SCALE_H(vp.dims));
         CHECK(rate > 99.9f && rate < 100.1f,
               "the timed read gave rate %.3f", (double)rate);
         (void)repeats; (void)swaps; (void)avg; (void)worst;
         (void)core_t; (void)render_t; (void)phase; (void)latdisp;
         (void)pacing;
      }
   }

   video_thread_wait_idle();
   thr->driver = vplane_inner;
   thr->poke   = vplane_inner_poke;
   run_frames(2);
   set_threaded_via_setting(false);
   run_frames(2);

   if (failures == had)
      fprintf(stderr, "[pass] viewport publish lane\n");
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

   /* Repeats are armed here, and the presenter's phase came from the
    * driver's own timestamps, so this is where the flags word carries
    * bits the snapshot lane's own state leaves clear. */
   stats_snapshot_check("repeat lane");

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
   unsigned dims = 0;
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
      drv->set_viewport(data, VIDEO_SCALE_PACK(640, 480), false, true);
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
      if (poke->get_video_output_size) poke->get_video_output_size(data, &dims, NULL, 0);
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

static bool reentrant_frame(void *data, const void *frame,
      unsigned dims, uint64_t count, unsigned pitch, const char *msg,
      video_frame_info_t *info)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   const video_driver_t *cur      = video_st->current_video;
   const video_poke_interface_t *poke = NULL;
   bool ret;

   /* As ozone/xmb do: through the frontend's current driver, which is
    * the wrapper, from the video thread. */
   if (cur && cur->set_viewport)
      cur->set_viewport(video_st->data, dims, false, true);
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

   ret = reentrant_inner->frame(data, frame, dims, count, pitch, msg, info);
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
         drv->set_viewport(video_st->data, VIDEO_SCALE_PACK(800, 600), false, true);
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
      unsigned dims, uint64_t frame_count,
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

   return wintick_inner->frame(data, frame, dims,
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

   /* The stale report is still in place and it is in the past, so the
    * presenter's phase - and the latency's source with it - came from
    * the display here. Those two bits of the snapshot's flags word are
    * clear in every other lane. */
   stats_snapshot_check("display-phase lane");

   thr->driver = phase_inner;
   thr->poke   = phase_inner_poke;
   settings->bools.video_threaded_present_repeat = false;
   set_threaded_via_setting(false);

   if (failures == had)
      fprintf(stderr, "[pass] display-phase lane (%llu repeats)\n",
            (unsigned long long)(rep1 - rep0));
}

/* ------------------------------------------------------------------ */
/* Lane: the menu texture handoff and the drain that protects it       */
/*   RGUI pushes a menu texture through the wrapper on every frame the */
/*   software menu is up, and the worker hands it to the driver from    */
/*   thread_update_driver_state() under frame.lock. What keeps that     */
/*   lock uncontended is not the lock itself: video_thread_frame()      */
/*   waits for the ring to drain whenever the menu texture is enabled,  */
/*   so the worker is idle again before the next iteration's push.      */
/*   Take that wait away and every menu frame's push starts racing a    */
/*   render for the lock, which is why it is pinned here.               */
/*                                                                     */
/*   Two things are asserted, neither of which the null driver can show */
/*   on its own because it has no set_texture_frame at all - so this    */
/*   lane supplies one, and a slow frame() to make the drain visible:   */
/*                                                                     */
/*   1. With the menu texture enabled, video_driver_frame() returns     */
/*      with the ring drained - nothing pending, nothing being          */
/*      rendered. Checked right after the push returns.                 */
/*   2. The driver is handed one push's pixels, never a mixture. Every  */
/*      push fills the buffer with its own generation, so a staging     */
/*      buffer rewritten under the driver shows up as two values in one */
/*      texture.                                                        */
/* ------------------------------------------------------------------ */

#define MENUTEX_W          32
#define MENUTEX_H          24
#define MENUTEX_RENDER_MS  30
#define MENUTEX_PUSHES     12
/* Marks this lane's own pushes apart from the menu's framebuffer. */
#define MENUTEX_MARK       0xA500
#define MENUTEX_MARK_MASK  0xFF00

static video_driver_t                 menutex_driver;
static const video_driver_t          *menutex_inner;
static video_poke_interface_t          menutex_poke;
static const video_poke_interface_t  *menutex_inner_poke;

static retro_atomic_int_t menutex_seen;
static retro_atomic_int_t menutex_torn;
static retro_atomic_int_t menutex_frames;
static bool               menutex_slow;

static bool menutex_frame(void *data, const void *frame,
      unsigned dims, uint64_t count, unsigned pitch, const char *msg,
      video_frame_info_t *info)
{
   if (menutex_slow)
   {
      retro_atomic_fetch_add_int(&menutex_frames, 1);
      retro_sleep(MENUTEX_RENDER_MS);
   }
   if (menutex_inner->frame)
      return menutex_inner->frame(data, frame, dims, count,
            pitch, msg, info);
   return true;
}

/* The receiving end. A real driver uploads or copies here and does not
 * keep the pointer, which is what lets the worker drop the lock as soon
 * as this returns; reading the whole buffer is how this one checks the
 * bytes it was handed belong to a single push.
 *
 * The menu is up in this harness and RGUI pushes its own framebuffer
 * through the same entry point, so only this lane's own pushes are
 * checked: they are MENUTEX_W x MENUTEX_H and every pixel carries
 * MENUTEX_MARK. The lock is what keeps the two producers from
 * interleaving, so a buffer is wholly one push's or wholly the other's -
 * a marked buffer holding two generations is the tear this looks for. */
static void menutex_set_texture_frame(void *data, const void *frame,
      bool rgb32, unsigned dims, float alpha)
{
   const uint16_t *px = (const uint16_t*)frame;
   unsigned i, n      = VIDEO_SCALE_AREA(dims);

   (void)data; (void)alpha;

   if (!px || !n)
      return;
   /* Not one of ours: the menu's own framebuffer. */
   if (rgb32 || dims != VIDEO_SCALE_PACK(MENUTEX_W, MENUTEX_H))
      return;
   if ((px[0] & MENUTEX_MARK_MASK) != MENUTEX_MARK)
      return;

   for (i = 1; i < n; i++)
   {
      if (px[i] != px[0])
      {
         retro_atomic_store_release_int(&menutex_torn, 1);
         break;
      }
   }

   retro_atomic_fetch_add_int(&menutex_seen, 1);
}

static void menutex_get_poke(void *data, const video_poke_interface_t **iface)
{
   menutex_inner->poke_interface(data, &menutex_inner_poke);
   menutex_poke                   = *menutex_inner_poke;
   menutex_poke.set_texture_frame = menutex_set_texture_frame;
   *iface                         = &menutex_poke;
}

static void lane_menu_texture(void)
{
   unsigned had                   = failures;
   video_driver_state_t *video_st = video_state_get_ptr();
   thread_video_t *thr;
   static uint16_t buf[MENUTEX_W * MENUTEX_H];
   unsigned gen, i, not_drained = 0;

   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "menu texture lane");
   thr = (thread_video_t*)video_st->data;

   video_thread_wait_idle();
   menutex_inner                 = thr->driver;
   menutex_driver                = *thr->driver;
   menutex_driver.frame          = menutex_frame;
   menutex_driver.poke_interface = menutex_get_poke;
   thr->driver                   = &menutex_driver;
   /* The wrapper caches its poke at init; refresh it through the
    * swapped driver so the worker hands the texture to ours. */
   menutex_driver.poke_interface(thr->driver_data, &thr->poke);

   retro_atomic_store_release_int(&menutex_seen,   0);
   retro_atomic_store_release_int(&menutex_torn,   0);
   retro_atomic_store_release_int(&menutex_frames, 0);
   menutex_slow = true;

   for (gen = 1; gen <= MENUTEX_PUSHES; gen++)
   {
      unsigned pending;
      bool     busy;

      for (i = 0; i < MENUTEX_W * MENUTEX_H; i++)
         buf[i] = (uint16_t)(MENUTEX_MARK | gen);

      video_st->poke->set_texture_frame(video_st->data, buf, false,
            VIDEO_SCALE_PACK(MENUTEX_W, MENUTEX_H), 1.0f);

      /* One frame through the real path, then the drain the menu
       * texture asks for must have happened: the worker holds no slot
       * and has none waiting, so the next push meets no render. */
      run_frames(1);

      slock_lock(thr->lock);
      pending = thr->frame.pending;
      busy    = thr->frame.busy;
      slock_unlock(thr->lock);

      if (pending || busy)
         not_drained++;
   }

   menutex_slow = false;
   video_thread_wait_idle();

   CHECK(retro_atomic_load_acquire_int(&menutex_frames) > 0,
         "the slow render never ran, so the drain was never worth checking");
   CHECK(retro_atomic_load_acquire_int(&menutex_seen) > 0,
         "the driver was handed no menu texture: the lane proved nothing");
   CHECK(!retro_atomic_load_acquire_int(&menutex_torn),
         "the driver saw pixels from more than one push: the staging "
         "buffer was rewritten while it was being read");
   CHECK(not_drained == 0,
         "%u of %d menu frames returned with the ring still busy: the "
         "drain video_thread_frame() does for an enabled menu texture is "
         "what keeps the handoff lock uncontended",
         not_drained, MENUTEX_PUSHES);

   thr->driver = menutex_inner;
   thr->poke   = menutex_inner_poke;
   set_threaded_via_setting(false);

   if (failures == had)
      fprintf(stderr, "[pass] menu-texture handoff lane (%d handed over, "
            "%d slow renders, all drained)\n",
            retro_atomic_load_acquire_int(&menutex_seen),
            retro_atomic_load_acquire_int(&menutex_frames));
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
   /* Pacing is still on here, so the snapshot's pacing bit and the core
    * and render times it carries are all non-trivial. */
   stats_snapshot_check("display-pacing lane");
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
      unsigned dims, uint64_t frame_count,
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
   return vslane_inner->frame(data, frame, dims, frame_count,
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
/* The window's answers reach the main thread, each as itself          */
/*                                                                    */
/*   alive, focus, has_windowed and presentable are published by the  */
/*   video thread after each frame, together. Each is driven on its   */
/*   own here and read back through the wrapper on the main thread,   */
/*   so one answer landing in another's place, or an answer that does */
/*   not move, fails.                                                  */
/* ------------------------------------------------------------------ */

static video_driver_t        winlane_driver;
static bool                  winlane_alive;
static bool                  winlane_focus;
static bool                  winlane_windowed;

static bool winlane_alive_cb(void *data)    { (void)data; return winlane_alive; }
static bool winlane_focus_cb(void *data)    { (void)data; return winlane_focus; }
static bool winlane_windowed_cb(void *data) { (void)data; return winlane_windowed; }

static void lane_window_answers(void)
{
   unsigned had = failures;
   video_driver_state_t *video_st;
   const video_driver_t *inner, *wrap;
   thread_video_t *thr;
   bool presentable;
   int saved, i;

   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "window answers lane");
   video_st    = video_state_get_ptr();
   thr         = (thread_video_t*)video_st->data;
   wrap        = video_st->current_video;
   video_thread_wait_idle();
   presentable = video_context_driver_presentable();

   inner                       = thr->driver;
   winlane_driver              = *thr->driver;
   winlane_driver.alive        = winlane_alive_cb;
   winlane_driver.focus        = winlane_focus_cb;
   winlane_driver.has_windowed = winlane_windowed_cb;
   thr->driver                 = &winlane_driver;

   /* Every focus/windowed pairing, the window alive. */
   winlane_alive = true;
   for (i = 0; i < 4; i++)
   {
      winlane_focus    = (i & 1) != 0;
      winlane_windowed = (i & 2) != 0;
      run_frames(2);
      video_thread_wait_idle();
      CHECK(wrap->alive(video_st->data),
            "window answers lane: alive read false (focus %d, windowed %d)",
            winlane_focus, winlane_windowed);
      CHECK(wrap->focus(video_st->data) == winlane_focus,
            "window answers lane: focus read %d, the driver said %d",
            !winlane_focus, winlane_focus);
      CHECK(wrap->has_windowed(video_st->data) == winlane_windowed,
            "window answers lane: has_windowed read %d, the driver said %d",
            !winlane_windowed, winlane_windowed);
      CHECK(video_context_driver_presentable() == presentable,
            "window answers lane: presentable moved with focus %d, "
            "windowed %d", winlane_focus, winlane_windowed);
   }

   /* The window gone, the others held. One frame publishes it; the
    * runloop would act on it at the next iterate, so the word is put
    * back by hand while the video thread is idle. */
   saved            = retro_atomic_load_acquire_int(&thr->win_flags);
   winlane_alive    = false;
   winlane_focus    = true;
   winlane_windowed = true;
   run_frames(1);
   video_thread_wait_idle();
   CHECK(!wrap->alive(video_st->data),
         "window answers lane: alive read true after the driver said false");
   CHECK(wrap->focus(video_st->data) && wrap->has_windowed(video_st->data),
         "window answers lane: focus or has_windowed fell with alive");
   CHECK(video_context_driver_presentable() == presentable,
         "window answers lane: presentable moved with alive");

   thr->driver = inner;
   retro_atomic_store_release_int(&thr->win_flags, saved);
   run_frames(2);

   if (failures == had)
      fprintf(stderr, "[pass] window answers lane\n");
}

/* ------------------------------------------------------------------ */
/* suppress_screensaver under the wrapper reaches the driver           */
/*                                                                    */
/*   The screensaver inhibit is the window's, so it has to reach the  */
/*   wrapped driver, on the thread that owns the window, with the     */
/*   caller's enable - and the driver's answer has to come back.      */
/* ------------------------------------------------------------------ */

static video_driver_t        sslane_driver;
static int                   sslane_calls;
static int                   sslane_last_enable;
static uintptr_t             sslane_thread;

static bool sslane_suppress(void *data, bool enable)
{
   (void)data;
   sslane_calls++;
   sslane_last_enable = enable ? 1 : 0;
   sslane_thread      = sthread_get_current_thread_id();
   /* An answer the wrapper could not make up: the opposite of what
    * it was asked, so a constant true or an echo both fail. */
   return !enable;
}

static void lane_suppress_screensaver(void)
{
   unsigned had = failures;
   video_driver_state_t *video_st;
   const video_driver_t *inner;
   thread_video_t *thr;
   bool ret;

   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "screensaver lane");
   video_st = video_state_get_ptr();
   thr      = (thread_video_t*)video_st->data;
   video_thread_wait_idle();

   inner                              = thr->driver;
   sslane_driver                      = *thr->driver;
   sslane_driver.suppress_screensaver = sslane_suppress;
   thr->driver                        = &sslane_driver;
   sslane_calls                       = 0;

   ret = video_st->current_video->suppress_screensaver(video_st->data, true);
   CHECK(sslane_calls == 1 && sslane_last_enable == 1,
         "screensaver lane: suppress(true) reached the driver %d times "
         "(enable %d)", sslane_calls, sslane_last_enable);
   CHECK(sslane_thread == sthread_get_thread_id(thr->thread),
         "screensaver lane: the driver was called off the video thread");
   CHECK(!ret, "screensaver lane: the driver's answer did not come back");

   ret = video_st->current_video->suppress_screensaver(video_st->data, false);
   CHECK(sslane_calls == 2 && sslane_last_enable == 0,
         "screensaver lane: suppress(false) reached the driver %d times "
         "(enable %d)", sslane_calls, sslane_last_enable);
   CHECK(ret, "screensaver lane: the driver's answer did not come back");

   video_thread_wait_idle();
   thr->driver = inner;
   run_frames(2);

   if (failures == had)
      fprintf(stderr, "[pass] suppress-screensaver lane\n");
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
      unsigned dims, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   if (RSLANE_GET() == 1)
   {
      /* Frame K: wait for the main thread to push K+1, then report the
       * resize as a context driver's check_window would. */
      unsigned spins = 0;
      while (RSLANE_GET() == 1 && spins++ < 2000)
         retro_sleep(1);
      video_driver_set_output_dims(VIDEO_SCALE_PACK(rslane_report_w, rslane_report_h));
      RSLANE_SET(3);
   }
   else if (RSLANE_GET() == 3 && !rslane_seen)
   {
      /* Frame K+1: what size is this drawn at? The size before the
       * report may be zero in the harness, so a flag, not the value. */
      rslane_seen_w = VIDEO_SCALE_W(video_info->dims);
      rslane_seen_h = VIDEO_SCALE_H(video_info->dims);
      rslane_seen   = true;
   }
   return rslane_inner->frame(data, frame, dims, frame_count,
         pitch, msg, video_info);
}

static void lane_resize_under_wrapper(void)
{
   unsigned out_dims;
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

   out_dims = video_driver_get_output_dims();
   before_w = VIDEO_SCALE_W(out_dims);
   before_h = VIDEO_SCALE_H(out_dims);
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
   video_driver_set_output_dims(VIDEO_SCALE_PACK(before_w, before_h));
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
      unsigned dims, uint64_t frame_count,
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
   return duplane_inner->frame(data, NULL, dims, frame_count,
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

/* ------------------------------------------------------------------ */
/* Lane: a size pair survives its packed word                         */
/*   Several width/height pairs travel as one machine word in         */
/*   VIDEO_SCALE_PACK's layout. A store and a load that disagree on   */
/*   which half holds which axis give every consumer a transposed     */
/*   frame, and nothing else in the suite notices: the values are     */
/*   both there, both plausible, and every other lane passes. So      */
/*   each pair with a public accessor is round-tripped here at        */
/*   dimensions no square frame can hide, and the clamp is checked    */
/*   at the top of the range, where a mask would wrap an axis to a    */
/*   small number instead of pinning it.                              */
/* ------------------------------------------------------------------ */

static void lane_size_pair_round_trip(void)
{
   unsigned out_dims;
   unsigned had = failures;
   unsigned w, h;
   size_t   pitch;
   bool     has_pixels;
   static uint8_t pix[320 * 200 * sizeof(uint32_t)];

   /* The output size. */
   video_driver_set_output_dims(VIDEO_SCALE_PACK(1280, 720));
   out_dims = video_driver_get_output_dims();
   w = VIDEO_SCALE_W(out_dims);
   h = VIDEO_SCALE_H(out_dims);
   CHECK(w == 1280 && h == 720,
         "the output size came back %ux%u, not 1280x720", w, h);

   video_driver_set_output_dims(VIDEO_SCALE_PACK(VIDEO_SCALE_DIM_MAX + 1, 720));
   out_dims = video_driver_get_output_dims();
   w = VIDEO_SCALE_W(out_dims);
   h = VIDEO_SCALE_H(out_dims);
   CHECK(w == VIDEO_SCALE_DIM_MAX && h == 720,
         "an out-of-range output width came back as %u, not clamped to %u",
         w, VIDEO_SCALE_DIM_MAX);

   /* The cached frame's dimensions, through the seqlock the replay and
    * screenshot paths read them from. */
   memset(pix, 0x40, sizeof(pix));
   video_driver_cached_frame_publish(pix, VIDEO_SCALE_PACK(320, 200),
         320 * sizeof(uint32_t));
   CHECK(video_driver_cached_frame_info(&out_dims, &pitch, &has_pixels),
         "nothing was cached by a publish of a 320x200 frame");
   CHECK(out_dims == VIDEO_SCALE_PACK(320, 200),
         "the cached frame came back %ux%u, not 320x200",
         VIDEO_SCALE_W(out_dims), VIDEO_SCALE_H(out_dims));
   CHECK(pitch == 320 * sizeof(uint32_t),
         "the cached pitch came back %u", (unsigned)pitch);

   /* Not a frame the lanes after this one should find in the cache. */
   video_driver_cached_frame_invalidate();
   run_frames(2);

   if (failures == had)
      fprintf(stderr, "[pass] size-pair round-trip lane\n");
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
               VIDEO_SCALE_PACK(320 + 64 * (i & 1), 240 + 48 * (i & 1)), true, true);
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

/* Held inside the upload, which the worker runs with no lock held and
 * with nothing on the main thread waiting on it: the posts below are
 * made while the video thread is provably busy with one of them, and
 * whether they came back before it got out is the whole test. A wall
 * clock would answer a different question on a loaded runner. */
static retro_atomic_int_t async_hold_arm;
static retro_atomic_int_t async_hold_release;
static retro_atomic_int_t async_in_upload;

/* One upload only: the arm is spent on entry, so a load posted behind
 * this one cannot re-enter the hold and hide a post that waited for
 * it. The wait is bounded so that a post which does wait fails the
 * lane rather than hanging it. */
static void async_hold_upload(void)
{
   unsigned spun;
   if (!retro_atomic_load_acquire_int(&async_hold_arm))
      return;
   retro_atomic_store_release_int(&async_hold_arm, 0);
   retro_atomic_store_release_int(&async_in_upload, 1);
   for (spun = 0; spun < 2000
         && !retro_atomic_load_acquire_int(&async_hold_release); spun++)
      retro_sleep(1);
   retro_atomic_store_release_int(&async_in_upload, 0);
}

static uintptr_t async_fake_load(void *data, void *img, bool threaded,
      enum texture_filter_type filter)
{
   (void)data; (void)img; (void)threaded; (void)filter;
   async_upload_thread = sthread_get_current_thread_id();
   async_hold_upload();
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

   /* The first load parks the worker inside the upload; the rest are
    * posted into a video thread that is provably busy with it. */
   retro_atomic_store_release_int(&async_in_upload, 0);
   retro_atomic_store_release_int(&async_hold_release, 0);
   retro_atomic_store_release_int(&async_hold_arm, 1);
   CHECK(video_driver_texture_load_async(&imgs[0], TEXTURE_FILTER_LINEAR,
            async_done_cb, (void*)(uintptr_t)0, async_release_cb),
         "async load 0 refused");
   for (i = 0; i < 2000
         && !retro_atomic_load_acquire_int(&async_in_upload); i++)
      retro_sleep(1);
   CHECK(retro_atomic_load_acquire_int(&async_in_upload),
         "the worker never reached the held upload");

   t0 = cpu_features_get_time_usec();
   for (i = 1; i < ASYNC_N; i++)
      CHECK(video_driver_texture_load_async(&imgs[i], TEXTURE_FILTER_LINEAR,
               async_done_cb, (void*)(uintptr_t)i, async_release_cb),
            "async load %u refused", i);
   t1 = cpu_features_get_time_usec();
   CHECK(retro_atomic_load_acquire_int(&async_in_upload),
         "posting %u async loads took %lld us and outlasted the held upload: it blocked",
         ASYNC_N - 1, (long long)(t1 - t0));
   CHECK(async_done_count == 0, "done() ran before any frame was pushed");
   retro_atomic_store_release_int(&async_hold_release, 1);

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
   unsigned i, n = VIDEO_SCALE_AREA(s->dims);
   for (i = 0; i < n; i++)
      s->slots[slot][i] = 0xff000000u | ((i * 7u + seed * 31u) & 0xffffffu);
}

/* A texture back end for the null driver, which has none at all. Up to
 * now the lane below stopped at "driver made no texture" and never
 * reached what it is about: that a streaming surface loads ONE texture
 * and keeps it across every frame, that each queued submit is released
 * exactly once, and that freeing a surface - with a frame still on its
 * way - releases its texture rather than leaking it. Handles are tagged
 * and tracked individually, so the checks name the surface's own
 * texture and are not disturbed by whatever else the frontend uploads
 * through the same poke (the menu's own framebuffer comes through it).
 *
 * The back end has to be installed per video mode: each switch reinits
 * the driver, which rebuilds the poke from the real one and drops the
 * override. Threaded, it goes on the wrapped driver so the worker's
 * uploads run through it; direct, on the driver's own poke. */
#define SURFTEX_TAG   0x5000
#define SURFTEX_MAX   256

static video_driver_t                 surftex_driver;
static const video_driver_t          *surftex_inner;
static video_poke_interface_t          surftex_poke;
static const video_poke_interface_t  *surftex_inner_poke;
static const video_poke_interface_t  *surftex_saved_poke;
static unsigned char surftex_live[SURFTEX_MAX + 1];
static unsigned      surftex_loads;
static unsigned      surftex_updates;
static unsigned      surftex_unloads;
/* 0 down, 1 on the wrapped driver, 2 on the driver's own poke. */
static int           surftex_mode;

static bool surftex_ours(uintptr_t id)
{
   return id > SURFTEX_TAG && id <= SURFTEX_TAG + SURFTEX_MAX;
}

static bool surftex_is_live(uintptr_t id)
{
   return surftex_ours(id) && surftex_live[id - SURFTEX_TAG];
}

static unsigned surftex_live_count(void)
{
   unsigned i, n = 0;
   for (i = 1; i <= SURFTEX_MAX; i++)
      if (surftex_live[i])
         n++;
   return n;
}

static uintptr_t surftex_load(void *data, void *img, bool threaded,
      enum texture_filter_type filter)
{
   (void)data; (void)img; (void)threaded; (void)filter;
   if (surftex_loads >= SURFTEX_MAX)
      return 0;
   surftex_live[++surftex_loads] = 1;
   return SURFTEX_TAG + surftex_loads;
}

/* In place: the same handle comes back, which is what lets the lane
 * check that a driver with an update path is not reloading. */
static bool surftex_update(void *data, uintptr_t id,
      const struct texture_image *ti, bool threaded)
{
   (void)data; (void)ti; (void)threaded;
   if (!surftex_is_live(id))
      return false;
   surftex_updates++;
   return true;
}

static void surftex_unload(void *data, bool threaded, uintptr_t id)
{
   if (surftex_ours(id))
   {
      surftex_live[id - SURFTEX_TAG] = 0;
      surftex_unloads++;
      return;
   }
   if (surftex_inner_poke && surftex_inner_poke->unload_texture)
      surftex_inner_poke->unload_texture(data, threaded, id);
}

/* Builds the overriding poke over `base`, which the unload forwards
 * handles it did not issue to. */
static void surftex_build(const video_poke_interface_t *base)
{
   surftex_inner_poke = base;
   if (base)
      surftex_poke = *base;
   else
      memset(&surftex_poke, 0, sizeof(surftex_poke));
   surftex_poke.load_texture   = surftex_load;
   surftex_poke.unload_texture = surftex_unload;
   surftex_poke.update_texture = surftex_update;
}

static void surftex_get_poke(void *data, const video_poke_interface_t **iface)
{
   const video_poke_interface_t *base = NULL;
   if (surftex_inner && surftex_inner->poke_interface)
      surftex_inner->poke_interface(data, &base);
   surftex_build(base);
   *iface = &surftex_poke;
}

static void surftex_reset_counts(void)
{
   memset(surftex_live, 0, sizeof(surftex_live));
   surftex_loads   = 0;
   surftex_updates = 0;
   surftex_unloads = 0;
}

static bool surftex_install(void)
{
   video_driver_state_t *vst = video_state_get_ptr();
   if (surftex_mode || !vst->data)
      return false;
   surftex_reset_counts();
   if (vst->thread_wrapper_active)
   {
      thread_video_t *thr = (thread_video_t*)vst->data;
      if (!thr->driver)
         return false;
      video_thread_wait_idle();
      surftex_inner                 = thr->driver;
      surftex_driver                = *thr->driver;
      surftex_driver.poke_interface = surftex_get_poke;
      thr->driver                   = &surftex_driver;
      surftex_get_poke(thr->driver_data, &thr->poke);
      surftex_mode                  = 1;
      return true;
   }
   surftex_saved_poke = vst->poke;
   surftex_inner      = NULL;
   surftex_build(vst->poke);
   vst->poke          = &surftex_poke;
   surftex_mode       = 2;
   return true;
}

static void surftex_remove(void)
{
   video_driver_state_t *vst = video_state_get_ptr();
   if (surftex_mode == 1)
   {
      thread_video_t *thr = (thread_video_t*)vst->data;
      video_thread_wait_idle();
      thr->driver = surftex_inner;
      thr->poke   = surftex_inner_poke;
   }
   else if (surftex_mode == 2)
      vst->poke = surftex_saved_poke;
   surftex_mode  = 0;
   surftex_inner = NULL;
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

   /* Lend the null driver a texture back end, so the checks below run
    * instead of stopping at "driver made no texture". A real driver
    * brings its own. */
   if (!real_driver())
      CHECK(surftex_install(), "surface lane: no texture back end installed");

#ifdef HAVE_GFX_INSTRUMENT
   gfx_instrument_reset();
#endif
   s = gfx_surface_new(VIDEO_SCALE_PACK(64, 48), 2, TEXTURE_FILTER_LINEAR, surf_release_cb, NULL);
   CHECK(s != NULL, "surface allocation failed");
   if (!s)
   {
      surftex_remove();
      return;
   }
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
      /* A real driver that makes none: nothing more to see. */
      fprintf(stderr, "[skip] surface lane: driver made no texture\n");
      surftex_remove();
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

   /* The free above had a frame in flight, so the completion is what
    * releases the texture. On the harness back end the handle can be
    * named: it is either still loaded or it is not. */
   if (surftex_mode)
   {
      video_thread_wait_idle();
      CHECK(!surftex_is_live(first),
            "surface freed with a frame in flight left its texture loaded");
      CHECK(surftex_live_count() == 0,
            "%u textures still loaded after the threaded half "
            "(%u loads, %u unloads)", surftex_live_count(),
            surftex_loads, surftex_unloads);
      CHECK(surftex_loads == 1, "%u texture loads for one streaming "
            "surface on the harness back end", surftex_loads);
   }

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
   /* Direct: the submit runs the driver here and now. The mode switch
    * reinits video, so the back end comes off first and goes back on
    * the driver's own poke after. */
   surftex_remove();
   set_threaded_via_setting(false);
   run_frames(2);
#ifdef HAVE_GFX_INSTRUMENT
   gfx_instrument_reset();
#endif
   expect_wrapper(false, "surface lane, direct");
   if (!real_driver())
      CHECK(surftex_install(),
            "surface lane, direct: no texture back end installed");
   s = gfx_surface_new(VIDEO_SCALE_PACK(64, 48), 1, TEXTURE_FILTER_LINEAR, surf_release_cb, NULL);
   CHECK(s != NULL, "direct surface allocation failed");
   if (!s)
   {
      surftex_remove();
      return;
   }
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

   if (surftex_mode)
   {
      CHECK(!surftex_is_live(first),
            "direct free left the surface's texture loaded");
      CHECK(surftex_live_count() == 0,
            "%u textures still loaded after the direct half "
            "(%u loads, %u unloads)", surftex_live_count(),
            surftex_loads, surftex_unloads);
      CHECK(surftex_loads == 1, "%u texture loads for one direct surface "
            "on the harness back end", surftex_loads);
      CHECK(surftex_updates > 0, "no in-place update reached the harness "
            "back end in 11 direct submits");
   }

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

   surftex_remove();

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

/* An overlay back end for the null driver, which has none, so the lane
 * gets past "no load_textures" to what it is about. It goes on
 * video_null itself and BEFORE the mode switch, because the wrapper's
 * init drops its own overlay entry point when the driver it wraps has
 * none: with one there, the threaded pass runs the real path - the
 * main thread's calls cross the command ring, the video thread's
 * capture of the table happens in the CMD_OVERLAY_LOAD handler, and
 * what arrives here arrives on the video thread.
 *
 * Recording what the driver is told is what lets the pass assert the
 * two things the wrapper promises and nothing has checked: that a page
 * switch passes indices rather than uploading, and that an alpha set
 * cannot be lost. */
#define OVLFAKE_MAX 8

static unsigned  ovl_enables;
static unsigned  ovl_disables;
static unsigned  ovl_loads;           /* the uploading variant         */
static unsigned  ovl_pages;           /* load_textures()               */
static unsigned  ovl_page_num;
static uintptr_t ovl_page[OVLFAKE_MAX];
static unsigned  ovl_tex_geoms;
static unsigned  ovl_vertex_geoms;
static unsigned  ovl_alpha_sets;
static float     ovl_alpha[OVLFAKE_MAX];

/* One shot, armed by the lane and fired by the driver's set_alpha the
 * first time the apply runs: a set that lands after the apply has
 * cleared its flag and read the value it was going to use. The
 * wrapper's order - clear the flag with an acquire exchange, THEN read
 * the values - is what carries this to the next pass. Reading first
 * and clearing after drops it, and that is the defect this catches. */
static retro_atomic_int_t                ovl_inject_arm;
static float                             ovl_inject_value;
static const video_overlay_interface_t  *ovl_inject_iface;
static void                             *ovl_inject_data;

static void ovl_enable(void *data, bool state)
{
   (void)data;
   if (state)
      ovl_enables++;
   else
      ovl_disables++;
}

static bool ovl_load(void *data, const void *images, unsigned num_images)
{
   (void)data; (void)images;
   ovl_loads++;
   ovl_page_num = num_images;
   return true;
}

static bool ovl_load_textures(void *data, const uintptr_t *textures,
      unsigned num_textures)
{
   unsigned i;
   (void)data;
   if (num_textures > OVLFAKE_MAX)
      return false;
   ovl_pages++;
   ovl_page_num = num_textures;
   for (i = 0; i < num_textures; i++)
      ovl_page[i] = textures[i];
   return true;
}

static void ovl_tex_geom(void *data, unsigned image,
      float x, float y, float w, float h)
{
   (void)data; (void)image; (void)x; (void)y; (void)w; (void)h;
   ovl_tex_geoms++;
}

static void ovl_vertex_geom(void *data, unsigned image,
      float x, float y, float w, float h)
{
   (void)data; (void)image; (void)x; (void)y; (void)w; (void)h;
   ovl_vertex_geoms++;
}

static void ovl_full_screen(void *data, bool enable)
{
   (void)data; (void)enable;
}

static void ovl_set_alpha(void *data, unsigned image, float mod)
{
   (void)data;
   if (image < OVLFAKE_MAX)
      ovl_alpha[image] = mod;
   ovl_alpha_sets++;

   if (image == 0 && retro_atomic_load_relaxed_int(&ovl_inject_arm))
   {
      /* Cleared first: this must run once, or the flag it raises keeps
       * the apply coming back for good. */
      retro_atomic_store_relaxed_int(&ovl_inject_arm, 0);
      ovl_inject_iface->set_alpha(ovl_inject_data, 0, ovl_inject_value);
   }
}

static const video_overlay_interface_t ovl_iface = {
   ovl_enable,
   ovl_load,
   ovl_load_textures,
   ovl_tex_geom,
   ovl_vertex_geom,
   ovl_full_screen,
   ovl_set_alpha,
};

static void ovl_get_iface(void *data, const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &ovl_iface;
}

static void ovl_counts_reset(void)
{
   unsigned i;
   ovl_enables = ovl_disables = ovl_loads = ovl_pages = 0;
   ovl_page_num = ovl_tex_geoms = ovl_vertex_geoms = ovl_alpha_sets = 0;
   for (i = 0; i < OVLFAKE_MAX; i++)
   {
      ovl_page[i]  = 0;
      ovl_alpha[i] = -1.0f;
   }
   retro_atomic_store_relaxed_int(&ovl_inject_arm, 0);
}

/* video_null is not const, and the wrapper reads the entry off the
 * driver it wraps at init: installed here, before the mode switch,
 * both passes see an overlay-capable driver. */
static bool ovl_installed;

static void ovl_install(void)
{
   if (ovl_installed || real_driver() || video_null.overlay_interface)
      return;
   video_null.overlay_interface = ovl_get_iface;
   ovl_installed                = true;
}

static void ovl_remove(void)
{
   if (!ovl_installed)
      return;
   video_null.overlay_interface = NULL;
   ovl_installed                = false;
}

static void lane_overlay_textures_pass(bool threaded)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   const video_overlay_interface_t *iface = NULL;
   static struct texture_image img[3];
   static uint32_t px[3][16 * 16];
   uintptr_t tex[3];
   unsigned i, j;

   /* Before the switch: the wrapper decides at init whether it has an
    * overlay to offer, from the driver it is about to wrap. */
   ovl_install();
   set_threaded_via_setting(threaded);
   run_frames(3);
   expect_wrapper(threaded, "overlay lane");

   /* And a texture back end, so the pack's textures exist at all. */
   if (!real_driver())
      CHECK(surftex_install(), "overlay lane (%s): no texture back end",
            threaded ? "threaded" : "direct");
   ovl_counts_reset();

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
      surftex_remove();
      ovl_remove();
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
      surftex_remove();
      ovl_remove();
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
   if (ovl_installed)
   {
      /* What the driver was actually told, on the far side of the
       * command ring: the page it was handed is the frontend's own
       * handles, in order, and the geometry for both images arrived. */
      video_thread_wait_idle();
      CHECK(ovl_pages == 1, "%u load_textures calls reached the driver for "
            "page one", ovl_pages);
      CHECK(ovl_loads == 0, "the uploading load() ran %u times on a driver "
            "with load_textures", ovl_loads);
      CHECK(ovl_page_num == 2 && ovl_page[0] == tex[0] && ovl_page[1] == tex[1],
            "page one arrived as %u handles (%lx, %lx), not (%lx, %lx)",
            ovl_page_num, (unsigned long)ovl_page[0],
            (unsigned long)ovl_page[1], (unsigned long)tex[0],
            (unsigned long)tex[1]);
      CHECK(ovl_enables == 1 && ovl_disables == 0,
            "enable reached the driver %u on, %u off", ovl_enables,
            ovl_disables);
      CHECK(ovl_tex_geoms == 2 && ovl_vertex_geoms == 2,
            "%u tex_geom and %u vertex_geom for two images",
            ovl_tex_geoms, ovl_vertex_geoms);
   }
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
   if (ovl_installed)
   {
      video_thread_wait_idle();
      /* The switch is a pass over indices: a second page, still no
       * upload, and the handles are the ones the frontend already
       * owns. */
      CHECK(ovl_pages == 2, "%u load_textures calls for two pages",
            ovl_pages);
      CHECK(ovl_loads == 0, "a page switch fell back to the uploading "
            "load() (%u calls)", ovl_loads);
      CHECK(ovl_page[0] == tex[1] && ovl_page[1] == tex[2],
            "page two arrived as (%lx, %lx), not (%lx, %lx)",
            (unsigned long)ovl_page[0], (unsigned long)ovl_page[1],
            (unsigned long)tex[1], (unsigned long)tex[2]);
   }

   /* The alpha path. Threaded, a set is fire-and-forget: the value goes
    * into an atomic and raises a flag the video thread clears with an
    * acquire exchange BEFORE reading the values, so a set that lands
    * mid-apply is applied whole on the next pass. Fire one from inside
    * the apply - the driver's set_alpha below is on the video thread,
    * after the flag was cleared and after the value for image 0 was
    * read - and it must still arrive. Clearing the flag after the read
    * instead would drop it, and nothing else in the tree notices. */
   if (ovl_installed && threaded)
   {
      ovl_alpha[0]     = -1.0f;
      ovl_inject_iface = iface;
      ovl_inject_data  = video_st->data;
      ovl_inject_value = 0.375f;          /* exact in binary32 */
      retro_atomic_store_release_int(&ovl_inject_arm, 1);
      iface->set_alpha(video_st->data, 0, 0.5f);
      run_frames(8);
      video_thread_wait_idle();
      CHECK(!retro_atomic_load_acquire_int(&ovl_inject_arm),
            "the overlay alpha apply never ran");
      CHECK(ovl_alpha[0] == 0.375f,
            "an alpha set from inside the apply was lost: the driver last "
            "saw %.3f, not 0.375", (double)ovl_alpha[0]);
   }

   /* An apply hands the driver only the images whose alpha changed:
    * a driver may pay per set (D3D10/11/12 map the sprite buffer for
    * each), and one press on a page of twenty images is one change.
    * After a new page the driver holds nothing the wrapper sent, so
    * the next apply sets every image. */
   if (ovl_installed && threaded)
   {
      unsigned before;

      video_thread_wait_idle();
      before = ovl_alpha_sets;
      iface->set_alpha(video_st->data, 1, 0.25f);
      run_frames(3);
      video_thread_wait_idle();
      CHECK(ovl_alpha_sets - before == 1 && ovl_alpha[1] == 0.25f,
            "one changed alpha reached the driver as %u sets",
            ovl_alpha_sets - before);

      before = ovl_alpha_sets;
      iface->set_alpha(video_st->data, 1, 0.25f);
      run_frames(3);
      video_thread_wait_idle();
      CHECK(ovl_alpha_sets == before,
            "an alpha set to what the driver already has reached it "
            "(%u sets)", ovl_alpha_sets - before);

      CHECK(iface->load_textures(video_st->data, tex, 2),
            "load_textures refused page three");
      run_frames(2);
      video_thread_wait_idle();
      before = ovl_alpha_sets;
      iface->set_alpha(video_st->data, 0, 1.0f);
      run_frames(3);
      video_thread_wait_idle();
      CHECK(ovl_alpha_sets - before == 2,
            "after a new page the apply set %u of its 2 images",
            ovl_alpha_sets - before);
   }

   iface->enable(video_st->data, false);
   run_frames(2);
   if (ovl_installed)
   {
      video_thread_wait_idle();
      CHECK(ovl_disables == 1, "enable(false) reached the driver %u times",
            ovl_disables);
   }

   /* Unloading the pack. Threaded, the wrapper does NOT hand the driver
    * a release here: a texture the in-flight frame may still name is
    * put on the retire list and freed by the video thread once it has
    * drawn the frame that carries it. So nothing reaches the driver
    * until frames run, and this is the only check on that path. */
   {
      unsigned  before = surftex_unloads;
      uintptr_t held[3];
      for (i = 0; i < 3; i++)
      {
         held[i] = tex[i];
         if (tex[i])
            video_driver_texture_unload(&tex[i]);
      }
      if (ovl_installed && threaded)
         CHECK(surftex_unloads == before,
               "%u of the pack's textures were released on the main thread "
               "instead of being retired", surftex_unloads - before);
      run_frames(3);
      if (ovl_installed)
      {
         video_thread_wait_idle();
         run_frames(1);
         video_thread_wait_idle();
         /* Named one by one, so what else the frontend may have
          * uploaded through the same poke in these frames cannot
          * stand in for the pack. */
         for (i = 0; i < 3; i++)
            CHECK(!surftex_is_live(held[i]),
                  "the pack's texture %u (%lx) was never released",
                  i, (unsigned long)held[i]);
      }
   }
   run_frames(3);
   surftex_remove();
   ovl_remove();
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
   {
      /* A drawn page takes one uniform and one vertex range, whatever
       * its image count: every image has the same MVP, and its quad
       * is drawn at its offset in the page's vertices. Counted by the
       * drivers that report it (Vulkan). */
      int draws  = gfx_instrument_get(GFX_INSTR_OVERLAY_DRAW);
      int allocs = gfx_instrument_get(GFX_INSTR_OVERLAY_DRAW_ALLOC);
      if (draws > 0)
      {
         fprintf(stderr, "[baseline] overlay draw: %d pages drawn, "
               "%d buffer ranges\n", draws, allocs);
         CHECK(allocs == 2 * draws, "%d buffer ranges for %d overlay "
               "page draws: a page takes two, not two per image",
               allocs, draws);
      }
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
   int64_t submit_total = 0;
   unsigned i, n_sub = 0, n_rel = 0;
   uintptr_t first    = 0;

   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "4k surface lane");

   s = gfx_surface_new(VIDEO_SCALE_PACK(3840, 2160), 2, TEXTURE_FILTER_LINEAR,
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
         continue;
      }
      CHECK(r == GFX_SURFACE_SUBMIT_QUEUED, "4K frame %u: submit returned %d", i, r);
      submit_us[n_sub++] = t1 - t0;
      submit_total      += t1 - t0;
      while (surf_releases == before && waited < 8)
      {
         run_frames(1);
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
            "mean %lld p50 %lld p95 %lld p99 %lld; slot back in frames "
            "p50 %lld p95 %lld p99 %lld\n", n_sub,
            (long long)(submit_total / n_sub),
            (long long)submit_us[n_sub / 2],
            (long long)submit_us[(n_sub * 95) / 100],
            (long long)submit_us[(n_sub * 99) / 100],
            (long long)release_frames[n_rel / 2],
            (long long)release_frames[(n_rel * 95) / 100],
            (long long)release_frames[(n_rel * 99) / 100]);
      /* A submit hands over a descriptor. A 4K copy on this thread
       * would be milliseconds of its own CPU time; the budget leaves
       * room for a slow host and none for a copy. The mean is what is
       * checked: Windows charges thread CPU time in whole 15.625 ms
       * scheduler ticks, so a single submit that straddles a tick
       * reads as one full tick, while the sum over all submits still
       * tracks the time actually spent. A copy puts milliseconds on
       * every submit and so fails the mean on any clock. */
      CHECK(submit_total / n_sub < 2000,
            "4K submit mean is %lld us: that is a copy, not a hand-off",
            (long long)(submit_total / n_sub));
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

/* ------------------------------------------------------------------ */
/* Lane: X11 event pump against the input poll                        */
/*   Under the wrapper the X event pump (x11_alive) runs on the video */
/*   thread, while the input driver polls on the runloop thread; the  */
/*   pointer-entered flag and the wheel/button latch are shared       */
/*   between them. The events are sent from a connection of our own, */
/*   so the lane does not depend on where the server's pointer sits.  */
/* ------------------------------------------------------------------ */

#ifdef HAVE_X11
static void x11_send(Display *dpy, Window win, int type, unsigned button)
{
   XEvent ev;
   memset(&ev, 0, sizeof(ev));
   ev.type = type;
   if (type == EnterNotify || type == LeaveNotify)
   {
      ev.xcrossing.window      = win;
      ev.xcrossing.mode        = NotifyNormal;
      ev.xcrossing.same_screen = True;
   }
   else
   {
      ev.xbutton.window        = win;
      ev.xbutton.button        = button;
      ev.xbutton.same_screen   = True;
   }
   /* An empty mask delivers to the client that created the window:
    * the frontend's own connection, which x11_alive() drains. */
   XSendEvent(dpy, win, False, 0, &ev);
}
#endif

static void lane_x11_event_pump(void)
{
#ifdef HAVE_X11
   unsigned had            = failures;
   unsigned wheel_seen     = 0;
   unsigned r              = 0;
   unsigned spin;
   input_driver_state_t *input_st;
   Display *dpy;
   Window win;

   if (video_driver_display_type_get() != RARCH_DISPLAY_X11)
   {
      fprintf(stderr, "[skip] x11 event pump lane (not an X11 display)\n");
      return;
   }
   if (!(dpy = XOpenDisplay(NULL)))
   {
      fprintf(stderr, "[skip] x11 event pump lane (no X connection)\n");
      return;
   }

   command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   set_threaded_via_setting(true);
   run_frames(5);
   expect_wrapper(true, "x11 event pump");

   input_st = input_state_get_ptr();
   win      = g_x11_win;
   if (     win == None
         || !input_st->current_driver
         || strcmp(input_st->current_driver->ident, "x"))
   {
      fprintf(stderr, "[skip] x11 event pump lane (input driver %s)\n",
            input_st->current_driver
            ? input_st->current_driver->ident : "none");
      goto end;
   }

   /* A frame push waits for the frame before it, so what the worker
    * pumps in the frame just handed over is the part the runloop is
    * not ordered against. Hand one over, then send, then poll while
    * that frame's alive() drains the connection. */
   for (r = 0; r < 64; r++)
   {
      run_frames(1);
      x11_send(dpy, win, EnterNotify,   0);
      x11_send(dpy, win, ButtonPress,   4);
      x11_send(dpy, win, ButtonPress,   8);
      x11_send(dpy, win, ButtonRelease, 8);
      if (r & 1)
         x11_send(dpy, win, LeaveNotify, 0);
      XFlush(dpy);
      for (spin = 0; spin < 200; spin++)
      {
         input_st->current_driver->poll(input_st->current_data);
         (void)input_st->current_driver->input_state(
               input_st->current_data, NULL, NULL, NULL, NULL, false, 0,
               RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_BUTTON_4);
         if (input_st->current_driver->input_state(
                  input_st->current_data, NULL, NULL, NULL, NULL, false, 0,
                  RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELUP))
         {
            wheel_seen++;
            break;
         }
         retro_sleep(1);
      }
   }

   CHECK(wheel_seen > 0,
         "x11 event pump: no wheel notch reached the input driver in %u rounds",
         r);

end:
   XCloseDisplay(dpy);
   set_threaded_via_setting(false);
   run_frames(2);
   command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   if (failures == had && win != None && wheel_seen)
      fprintf(stderr, "[pass] x11 event pump lane (%u of %u rounds latched)\n",
            wheel_seen, r);
#endif
}

/* ------------------------------------------------------------------ */
/* Lane: the grabbed mouse                                            */
/*   Grabbed, the X input driver reads the pointer, takes its offset  */
/*   from the window's centre as the motion and warps it back. The    */
/*   pointer is moved from a connection of our own between polls; the */
/*   motion must come out exactly, and a poll must cost the server    */
/*   only its keymap query, its pointer query and the warp - the size */
/*   is the one the event pump recorded, and the warp is not waited   */
/*   for. Direct and under the wrapper.                               */
/* ------------------------------------------------------------------ */

#define GRABLANE_POLLS 20

static void lane_x11_grabbed_mouse(void)
{
#ifdef HAVE_X11
   unsigned had            = failures;
   unsigned i;
   unsigned pass;
   unsigned long before    = 0;
   unsigned long sent      = 0;
   unsigned long most      = 0;
   unsigned dims           = 0;
   int cx, cy;
   input_driver_state_t *input_st;
   Display *dpy;
   Window win;

   if (video_driver_display_type_get() != RARCH_DISPLAY_X11)
   {
      fprintf(stderr, "[skip] x11 grabbed mouse lane (not an X11 display)\n");
      return;
   }
   input_st = input_state_get_ptr();
   win      = g_x11_win;
   if (     win == None
         || !input_st->current_driver
         || strcmp(input_st->current_driver->ident, "x")
         || !input_st->current_driver->grab_mouse)
   {
      fprintf(stderr, "[skip] x11 grabbed mouse lane (input driver %s)\n",
            input_st->current_driver
            ? input_st->current_driver->ident : "none");
      return;
   }
   if (!(dpy = XOpenDisplay(NULL)))
   {
      fprintf(stderr, "[skip] x11 grabbed mouse lane (no X connection)\n");
      return;
   }

   /* Directly, then under the wrapper, where the size is recorded on
    * the video thread and read by the poll on this one. */
   for (pass = 0; pass < 2; pass++)
   {
   set_threaded_via_setting(pass == 1);
   run_frames(3);
   expect_wrapper(pass == 1, "x11 grabbed mouse");
   /* The toggle rebuilt the driver and its window. */
   win = g_x11_win;
   if (     win == None
         || !input_st->current_driver
         || strcmp(input_st->current_driver->ident, "x"))
      break;
   x11_get_video_size(NULL, &dims);
   cx = (int)(VIDEO_SCALE_W(dims) >> 1);
   cy = (int)(VIDEO_SCALE_H(dims) >> 1);

   /* Inside the window, as the pointer is when a core grabs it. */
   x11_send(dpy, win, EnterNotify, 0);
   XWarpPointer(dpy, None, win, 0, 0, 0, 0, cx, cy);
   XSync(dpy, False);
   run_frames(2);
   input_st->current_driver->grab_mouse(input_st->current_data, true);

   for (i = 0; i < GRABLANE_POLLS; i++)
   {
      int16_t dx, dy;
      /* Everything the last poll sent has been done, so the move below
       * is relative to the centre it warped back to. */
      XSync(g_x11_dpy, False);
      XWarpPointer(dpy, None, win, 0, 0, 0, 0, cx + 7, cy - 3);
      XSync(dpy, False);

      before = NextRequest(g_x11_dpy);
      input_st->current_driver->poll(input_st->current_data);
      XSync(g_x11_dpy, False);
      /* Less the XSync that reads the count. */
      sent   = NextRequest(g_x11_dpy) - before - 1;
      if (sent > most)
         most = sent;

      dx = input_st->current_driver->input_state(input_st->current_data,
            NULL, NULL, NULL, NULL, false, 0,
            RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
      dy = input_st->current_driver->input_state(input_st->current_data,
            NULL, NULL, NULL, NULL, false, 0,
            RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
      /* The first poll may still be taking the pointer to the centre. */
      if (i > 0)
         CHECK(dx == 7 && dy == -3,
               "x11 grabbed mouse: poll %u read motion (%d,%d), moved (7,-3)",
               i, (int)dx, (int)dy);
   }

   CHECK(most <= 3,
         "x11 grabbed mouse: a poll sent %lu requests, want keymap,"
         " pointer and warp only", most);

   input_st->current_driver->grab_mouse(input_st->current_data, false);
   }
   CHECK(pass == 2, "x11 grabbed mouse: pass %u lost the X input driver", pass);

   x11_send(dpy, win, LeaveNotify, 0);
   XSync(dpy, False);
   XCloseDisplay(dpy);
   set_threaded_via_setting(false);
   run_frames(2);

   if (failures == had)
      fprintf(stderr, "[pass] x11 grabbed mouse lane (%u polls direct and"
            " threaded, at most %lu requests each)\n",
            (unsigned)GRABLANE_POLLS, most);
#endif
}

/* ------------------------------------------------------------------ */
/* Lane: the Vulkan swapchain has an X connection of its own          */
/*   Every request on a connection is numbered, and a present on the  */
/*   frontend's connection (a software WSI's image data every frame)  */
/*   moves the frontend's numbering with it. The X input driver's     */
/*   keymap and pointer queries are the frontend's own requests - the */
/*   ones a shared connection makes wait behind a frame - so its poll */
/*   is left out while counting, and what remains over a run of       */
/*   presented frames is only what the window poll sends: nothing.    */
/* ------------------------------------------------------------------ */

#define WSILANE_FRAMES 60

static void wsilane_no_poll(void *data) { (void)data; }

static void lane_x11_wsi_connection(void)
{
#if defined(HAVE_X11) && defined(HAVE_VULKAN)
   unsigned had            = failures;
   unsigned long before    = 0;
   unsigned long sent      = 0;
   const char *drv         = getenv("HARNESS_VIDEO_DRIVER");
   input_driver_state_t *input_st;
   input_driver_t *polling;
   input_driver_t quiet;

   if (!drv || strcmp(drv, "vulkan"))
   {
      fprintf(stderr, "[skip] x11 wsi connection lane (driver %s)\n",
            drv ? drv : "null");
      return;
   }
   if (     video_driver_display_type_get() != RARCH_DISPLAY_X11
         || !g_x11_dpy)
   {
      fprintf(stderr, "[skip] x11 wsi connection lane (not an X11 display)\n");
      return;
   }

   expect_wrapper(false, "x11 wsi connection");
   input_st = input_state_get_ptr();
   polling  = input_st->current_driver;
   if (polling)
   {
      quiet                    = *polling;
      quiet.poll               = wsilane_no_poll;
      input_st->current_driver = &quiet;
   }

   run_frames(2);
   XSync(g_x11_dpy, False);
   before = NextRequest(g_x11_dpy);
   run_frames(WSILANE_FRAMES);
   XSync(g_x11_dpy, False);
   /* Less the XSync that reads the count. */
   sent   = NextRequest(g_x11_dpy) - before - 1;

   if (polling)
      input_st->current_driver = polling;

   CHECK(sent < WSILANE_FRAMES / 4,
         "x11 wsi connection: %lu requests on the frontend's connection"
         " over %u presented frames - the swapchain presents on it",
         sent, (unsigned)WSILANE_FRAMES);

   if (failures == had)
      fprintf(stderr, "[pass] x11 wsi connection lane (%lu requests over %u frames)\n",
            sent, (unsigned)WSILANE_FRAMES);
#endif
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

   lane_line_separation();
   lane_vp_params_publish();
   lane_toggle_cycle(cycles);
   lane_reinit_under_wrapper(cycles / 2 + 1);
   lane_toggle_in_game(cycles);
   lane_swap_count();
   lane_stats_snapshot();
   lane_stat_text_bounds();
   lane_viewport_publish();
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
      lane_menu_texture();
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
   if (real_driver())
      lane_x11_event_pump();
   if (real_driver())
      lane_x11_wsi_connection();
   if (real_driver())
      lane_x11_grabbed_mouse();
   if (!real_driver())
   {
      lane_waiter_call();
      lane_frame_path_heap();
      lane_resize_under_wrapper();
      lane_dupe_under_wrapper();
      lane_suppress_screensaver();
      lane_window_answers();
      lane_size_pair_round_trip();
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

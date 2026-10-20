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

#include <gfx/scaler/pixconv.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <time.h>
#include <math.h>
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
#include "../../../gfx/gfx_instrument.h"
#include "../../../gfx/video_thread_wrapper.h"
#include "../../../record/record_driver.h"
#include "../../../gfx/font_driver.h"
#include "../../../menu/menu_driver.h"
#include "../../../menu/menu_setting.h"
#include "../../../verbosity.h"
#include "../../../input/input_driver.h"
#include "../../../tasks/task_content.h"
#include "../../../gfx/common/gl_common.h"
#ifdef HAVE_VULKAN
#include <libretro_vulkan.h>
#include "../../../gfx/common/vulkan_common.h"
#endif

#ifdef HAVE_X11
#include <X11/Xlib.h>
#include "../../../gfx/common/x11_common.h"
#endif

#include <time/rtime.h>
#include <retro_timers.h>
#include <features/features_cpu.h>
#include <rthreads/rthreads.h>
#include <file/config_file.h>
#include <formats/image_yuv_blit.h>
#include <formats/video_hw_frame.h>
#include <queues/task_queue.h>

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

/* calloc() through the link's --wrap (build.sh, everywhere but Apple's
 * linker, which has no --wrap): armed, the next call fails. Only the
 * main thread arms it, before the frontend starts. */
#if !defined(__APPLE__)
#define HARNESS_WRAP_CALLOC 1
void *__real_calloc(size_t nmemb, size_t size);
static bool harness_calloc_fail_next;
void *__wrap_calloc(size_t nmemb, size_t size)
{
   if (harness_calloc_fail_next)
   {
      harness_calloc_fail_next = false;
      return NULL;
   }
   return __real_calloc(nmemb, size);
}

/* A GL driver whose own struct cannot be allocated returns no driver,
 * touching nothing (its init allocates that struct first). Run before
 * the frontend is up: the init's error path frees the current context
 * driver, and there is none yet. */
static void lane_driver_struct_alloc(void)
{
   video_info_t video;
   memset(&video, 0, sizeof(video));
#ifdef HAVE_OPENGL
   harness_calloc_fail_next = true;
   CHECK(!video_gl2.init(&video), "gl: init with no memory for its "
         "struct gave a driver");
   CHECK(!harness_calloc_fail_next, "gl: init did not allocate");
#endif
#ifdef HAVE_OPENGL_CORE
   harness_calloc_fail_next = true;
   CHECK(!video_gl3.init(&video), "glcore: init with no memory for its "
         "struct gave a driver");
   CHECK(!harness_calloc_fail_next, "glcore: init did not allocate");
#endif
   harness_calloc_fail_next = false;
   printf("[pass] driver struct allocation lane\n");
}
#endif

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

#ifdef HAVE_COCOA
/* harness_cocoa.m: the harness runs inside Cocoa's application on
 * macOS - see there. */
void harness_cocoa_pump(void);
void harness_cocoa_exit_status(int status);
#endif

static void run_frames(unsigned n)
{
   unsigned i;
   for (i = 0; i < n; i++)
   {
      runloop_iterate();
#ifdef HAVE_COCOA
      harness_cocoa_pump();
#endif
   }
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

#ifdef HAVE_GFX_WIDGETS
#include "../../../gfx/gfx_widgets.h"

static void set_driver(thread_video_t *thr, const video_driver_t *driver);
static void set_poke(thread_video_t *thr, const video_poke_interface_t *poke);
static void set_poke_from(thread_video_t *thr, const video_driver_t *driver);

/* ------------------------------------------------------------------ */
/* Lane: the widgets go only once no frame draws them                 */
/*   Threaded video turned off clears its setting before the reinit,  */
/*   so driver_uninit() waits on the wrapper being up, not on the     */
/*   setting. The widgets turned off tear down once the video thread  */
/*   is past the frame in flight, and the toggle does not wait for    */
/*   it. Either way a frame drawing them must not see them go.        */
/* ------------------------------------------------------------------ */

static const video_driver_t *wolane_inner;
static video_driver_t        wolane_driver;
static retro_atomic_int_t    wolane_slow;
static retro_atomic_int_t    wolane_frames;
static retro_atomic_int_t    wolane_raced;

static bool wolane_frame(void *data, const void *frame,
      unsigned dims, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   if (retro_atomic_load_acquire_int(&wolane_slow))
   {
      volatile gfx_widget_font_data_t *font = (volatile gfx_widget_font_data_t*)
         &dispwidget_get_ptr()->gfx_widget_fonts.regular;
      void *had          = font->font;
      retro_time_t until = cpu_features_get_time_usec() + 500000;
      retro_atomic_fetch_add_int(&wolane_frames, 1);
      /* Drawing until the widgets' font goes, or long enough that the
       * teardown would have reached it */
      while (     had
            &&  font->font == had
            &&  cpu_features_get_time_usec() < until) { }
      if (had && font->font != had)
         retro_atomic_store_release_int(&wolane_raced, 1);
   }
   return wolane_inner->frame(data, frame, dims, frame_count,
         pitch, msg, video_info);
}

static void lane_threaded_off_in_flight(void)
{
   static const char *how[3] = { "threaded off", "widgets off",
      "widgets off and on" };
   unsigned had = failures;
   settings_t *settings = config_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   int c;

   for (c = 0; c < 3; c++)
   {
      dispgfx_widget_t *w;
      thread_video_t *thr;
      retro_time_t took = 0;
      unsigned i;

      set_threaded_via_setting(true);
      run_frames(6);
      expect_wrapper(true, "threaded-off lane");
      w = dispwidget_get_ptr();
      if (!retro_atomic_load_acquire_int(&w->worker))
      {
         fprintf(stderr, "[skip] threaded-off lane (no widgets on this driver)\n");
         set_threaded_via_setting(false);
         return;
      }
      if (menu_is_up())
         command_event(CMD_EVENT_MENU_TOGGLE, NULL);
      run_frames(3);
      video_thread_wait_idle();
      CHECK(w->gfx_widget_fonts.regular.font != NULL,
            "threaded-off lane (%s): no widget font to watch", how[c]);
      thr                 = (thread_video_t*)video_st->data;
      wolane_inner        = thr->driver;
      wolane_driver       = *thr->driver;
      wolane_driver.frame = wolane_frame;
      set_driver(thr, &wolane_driver);
      retro_atomic_store_release_int(&wolane_raced, 0);
      retro_atomic_store_release_int(&wolane_frames, 0);
      retro_atomic_store_release_int(&wolane_slow, 1);
      /* A frame still drawing, then straight off */
      for (i = 0; i < 1000
            && !retro_atomic_load_acquire_int(&wolane_frames); i++)
      {
         run_frames(1);
         if (!retro_atomic_load_acquire_int(&wolane_frames))
            retro_sleep(1);
      }
      if (c == 0)
         set_threaded_via_setting(false);
      else
      {
         settings->bools.menu_enable_widgets = false;
         took = cpu_features_get_time_usec();
         command_event(CMD_EVENT_OSD_NOTIFICATION_TOGGLE, NULL);
         took = cpu_features_get_time_usec() - took;
         /* Back before the teardown could run: put off, not over it */
         if (c == 2)
         {
            settings->bools.menu_enable_widgets = true;
            command_event(CMD_EVENT_OSD_NOTIFICATION_TOGGLE, NULL);
         }
      }
      retro_atomic_store_release_int(&wolane_slow, 0);
      CHECK(retro_atomic_load_acquire_int(&wolane_frames) > 0,
            "threaded-off lane (%s): no frame was in flight", how[c]);
      CHECK(!retro_atomic_load_acquire_int(&wolane_raced),
            "threaded-off lane (%s): the widgets went while a frame "
            "drew them", how[c]);
      if (c > 0)
      {
         CHECK(took < 200000,
               "threaded-off lane (%s): the toggle waited %lld us "
               "for the frame", how[c], (long long)took);
         if (c == 1)
         {
            /* Torn down once the video thread is past the frame */
            for (i = 0; i < 60 && (w->flags & DISPGFX_WIDGET_FLAG_INITED); i++)
               run_frames(1);
            CHECK(!(w->flags & DISPGFX_WIDGET_FLAG_INITED),
                  "threaded-off lane (widgets off): never torn down");
            settings->bools.menu_enable_widgets = true;
            command_event(CMD_EVENT_OSD_NOTIFICATION_TOGGLE, NULL);
         }
         video_thread_wait_idle();
         set_driver(thr, wolane_inner);
         /* And back, on the video thread again */
         for (i = 0; i < 60 && !retro_atomic_load_acquire_int(&w->worker); i++)
            run_frames(1);
         CHECK(retro_atomic_load_acquire_int(&w->worker) && w->active,
               "threaded-off lane (%s): not back after the toggle", how[c]);
         set_threaded_via_setting(false);
      }
      expect_wrapper(false, "threaded-off lane");
      if (!menu_is_up())
         command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   }
   if (failures == had)
      fprintf(stderr, "[pass] threaded-off lane (threaded off, widgets off, off and on)\n");
}
#endif

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
/* Lane: driver pacing                                                */
/*   The runloop's side of Driver Pacing against a fake backend in    */
/*   the driver's poke slots: one wait a content frame with a fresh   */
/*   identity, the input and simulation markers on that identity in   */
/*   order, the mode asked for while owned and taken off once when    */
/*   not - the menu, threaded video, BFI, a backend that fails - and  */
/*   the pacing record saying so. On a real driver without the       */
/*   extension (lavapipe) the device reason is reported and nothing   */
/*   is released.                                                     */
/* ------------------------------------------------------------------ */

static video_poke_interface_t         dplane_poke;
static const video_poke_interface_t  *dplane_saved_poke;
static unsigned dplane_sets, dplane_last_mode, dplane_set_answer;
static unsigned dplane_waits, dplane_last_interval, dplane_last_max;
static uint64_t dplane_last_id, dplane_wait_answer_from;
static unsigned dplane_marks;
/* (id, marker) of the last few markers, oldest first */
static uint64_t dplane_mark_id[8];
static unsigned dplane_mark_kind[8];

static unsigned dplane_set(void *data, unsigned mode)
{
   (void)data;
   dplane_sets++;
   dplane_last_mode = mode;
   if (!mode)
      return VIDEO_DRIVER_PACING_REASON_OFF;
   return dplane_set_answer;
}

/* The identity the frame loop waits for: the frame it handed over
 * last, which its next release names plus one */
static uint64_t dplane_last_wait_id, dplane_last_next_id;
/* On the video thread, the identities the frames arrive with */
static retro_atomic_size_t dplane_seen_id, dplane_seen_frames, dplane_seen_interval;

static bool dplane_wait(void *data, uint64_t frame_id, uint64_t next_id,
      unsigned max_wait_us)
{
   (void)data;
   dplane_last_next_id = next_id;
   dplane_waits++;
   dplane_last_wait_id  = frame_id;
   dplane_last_max      = max_wait_us;
   if (dplane_wait_answer_from)
      return false;
   dplane_last_id = frame_id + 1;
   return true;
}

static void dplane_mark(void *data, uint64_t id, unsigned marker)
{
   (void)data;
   memmove(dplane_mark_id,   dplane_mark_id + 1,   7 * sizeof(dplane_mark_id[0]));
   memmove(dplane_mark_kind, dplane_mark_kind + 1, 7 * sizeof(dplane_mark_kind[0]));
   dplane_mark_id[7]   = id;
   dplane_mark_kind[7] = marker;
   dplane_marks++;
}

static bool dplane_focus(void *data)
{
   (void)data;
   return true;
}

/* The fake driver under the wrapper: the null driver with the
 * pacing pokes, and a frame that notes what the packet carried */
static const video_driver_t          *dplane_inner;
static video_driver_t                 dplane_driver;
static const video_poke_interface_t  *dplane_inner_poke;
static video_poke_interface_t         dplane_thread_poke;

static bool dplane_frame(void *data, const void *frame, unsigned dims,
      uint64_t frame_count, unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
   if (video_info && video_info->driver_pacing_id)
   {
      retro_atomic_store_release_size(&dplane_seen_id,
            (size_t)video_info->driver_pacing_id);
      retro_atomic_store_release_size(&dplane_seen_interval,
            (size_t)video_info->driver_pacing_interval_us);
      retro_atomic_fetch_add_size(&dplane_seen_frames, 1);
   }
   return dplane_inner->frame(data, frame, dims, frame_count, pitch, msg,
         video_info);
}

static void dplane_get_poke(void *data, const video_poke_interface_t **iface)
{
   dplane_inner->poke_interface(data, &dplane_inner_poke);
   dplane_thread_poke                    = *dplane_inner_poke;
   dplane_thread_poke.driver_pacing_set  = dplane_set;
   dplane_thread_poke.driver_pacing_wait = dplane_wait;
   dplane_thread_poke.driver_pacing_mark = dplane_mark;
   *iface                                = &dplane_thread_poke;
}

static void lane_driver_pacing(void)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   runloop_state_t *runloop_st    = runloop_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   unsigned had                   = failures;
   unsigned saved_bfi             = settings->uints.video_black_frame_insertion;
   enum poll_type_override_t saved_poll = runloop_st->core_poll_type_override;
   unsigned waits;
   video_driver_t *saved_driver;
   video_driver_t focused_driver;

   command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   CHECK(!menu_is_up(), "driver pacing lane: menu still up");
   /* The harness core never polls itself: early polling puts the
    * frame's input sample where the runloop makes it. */
   runloop_st->core_poll_type_override = POLL_TYPE_OVERRIDE_EARLY;
   set_threaded_via_setting(false);
   run_frames(3);
   expect_wrapper(false, "driver pacing lane");
   /* This lane tests pacing policy with a focused game. Desktop focus
    * may change asynchronously while the synthetic backend runs. */
   saved_driver         = video_st->current_video;
   focused_driver       = *saved_driver;
   focused_driver.focus = dplane_focus;
   video_st->current_video = &focused_driver;

   /* Explicitly remove the backend: capable hardware may otherwise
    * satisfy this request through the real driver's pacing hooks. */
   dplane_saved_poke = video_st->poke;
   dplane_poke       = *video_st->poke;
   dplane_poke.driver_pacing_set  = NULL;
   dplane_poke.driver_pacing_wait = NULL;
   dplane_poke.driver_pacing_mark = NULL;
   video_st->poke = &dplane_poke;
   settings->uints.video_driver_pacing = VIDEO_DRIVER_PACING_ON;
   run_frames(5);
   CHECK(video_st->driver_pacing_reason == VIDEO_DRIVER_PACING_REASON_DRIVER,
         "no backend: reason %u", video_st->driver_pacing_reason);
   CHECK(!(runloop_st->pace & RUNLOOP_PACE_DRIVER), "no backend: paced by the driver");
   CHECK(runloop_st->driver_pacing_frame_id == 0, "no backend: a frame identity");

   /* A backend that answers */
   dplane_poke.driver_pacing_set  = dplane_set;
   dplane_poke.driver_pacing_wait = dplane_wait;
   dplane_poke.driver_pacing_mark = dplane_mark;
   video_st->poke     = &dplane_poke;
   dplane_sets = dplane_waits = dplane_marks = 0;
   dplane_last_id = dplane_wait_answer_from = 0;
   dplane_set_answer  = VIDEO_DRIVER_PACING_REASON_NONE;

   run_frames(20);
   CHECK(dplane_last_wait_id == 19,
         "backend up: the last wait named the frame handed over, %llu",
         (unsigned long long)dplane_last_wait_id);
   CHECK(dplane_last_next_id == runloop_st->driver_pacing_frame_id,
         "backend up: the wait named the identity it released, %llu vs %llu",
         (unsigned long long)dplane_last_next_id,
         (unsigned long long)runloop_st->driver_pacing_frame_id);
   CHECK(video_st->driver_pacing_reason == VIDEO_DRIVER_PACING_REASON_NONE,
         "backend up: reason %u", video_st->driver_pacing_reason);
   CHECK(video_st->driver_pacing_mode == VIDEO_DRIVER_PACING_ON, "backend up: mode");
   CHECK(dplane_last_mode == VIDEO_DRIVER_PACING_ON, "backend up: asked for On");
   CHECK(runloop_st->pace & RUNLOOP_PACE_DRIVER, "backend up: the pacing record");
   CHECK(dplane_waits == 20, "backend up: %u waits for 20 frames", dplane_waits);
   CHECK(dplane_last_id == 20 && runloop_st->driver_pacing_frame_id == 20,
         "backend up: identity %llu, runloop holds %llu",
         (unsigned long long)dplane_last_id,
         (unsigned long long)runloop_st->driver_pacing_frame_id);
   CHECK(dplane_last_max >= 20000 && dplane_last_max <= 210000,
         "backend up: the wait bound %u us", dplane_last_max);
   /* The last frame: released (SIMULATION_START on 20) after the
    * frame before it sampled input on 19 */
   CHECK(dplane_mark_kind[7] == VIDEO_DRIVER_PACING_MARK_SIMULATION_START
         && dplane_mark_id[7] == 20,
         "the release marks the start of frame 20 (got %u on %llu)",
         dplane_mark_kind[7], (unsigned long long)dplane_mark_id[7]);
   CHECK(dplane_mark_kind[6] == VIDEO_DRIVER_PACING_MARK_INPUT_SAMPLE
         && dplane_mark_id[6] == 19,
         "frame 19 sampled its input before frame 20 was released (got %u on %llu)",
         dplane_mark_kind[6], (unsigned long long)dplane_mark_id[6]);
   CHECK(dplane_mark_kind[5] == VIDEO_DRIVER_PACING_MARK_SIMULATION_START
         && dplane_mark_id[5] == 19,
         "frame 19 was released before it sampled input");
   {
      char pbuf[64];
      runloop_pace_string(pbuf, sizeof(pbuf));
      CHECK(strstr(pbuf, "Driver") != NULL, "pacing string: %s", pbuf);
      runloop_driver_pacing_string(pbuf, sizeof(pbuf));
      CHECK(!strcmp(pbuf, "On"), "status string: %s", pbuf);
   }

   /* The menu up: the mode is taken off, once, and nothing released */
   waits       = dplane_waits;
   dplane_sets = 0;
   command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_frames(5);
   CHECK(video_st->driver_pacing_reason == VIDEO_DRIVER_PACING_REASON_SUSPENDED,
         "menu: reason %u", video_st->driver_pacing_reason);
   CHECK(dplane_sets == 1 && dplane_last_mode == VIDEO_DRIVER_PACING_OFF,
         "menu: the mode taken off once (%u sets, last %u)",
         dplane_sets, dplane_last_mode);
   CHECK(dplane_waits == waits, "menu: a release");
   CHECK(runloop_st->driver_pacing_frame_id == 0, "menu: an identity kept");
   CHECK(!(runloop_st->pace & RUNLOOP_PACE_DRIVER), "menu: the pacing record");
   command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_frames(5);
   CHECK(dplane_waits == waits + 5 && dplane_last_mode == VIDEO_DRIVER_PACING_ON,
         "back in game: releases resume");

   /* Black Frame Insertion: its extra presents go out of band on the
    * same frame, so the release stays owned */
   waits = dplane_waits;
   settings->uints.video_black_frame_insertion = 1;
   run_frames(3);
   CHECK(video_st->driver_pacing_reason == VIDEO_DRIVER_PACING_REASON_NONE
         && dplane_waits == waits + 3,
         "BFI: reason %u, %u releases", video_st->driver_pacing_reason,
         dplane_waits - waits);
   settings->uints.video_black_frame_insertion = saved_bfi;
   /* Scanline Sync paces the present itself */
   waits = dplane_waits;
   settings->bools.video_scanline_sync = true;
   run_frames(3);
   CHECK(video_st->driver_pacing_reason == VIDEO_DRIVER_PACING_REASON_MODE
         && dplane_waits == waits && dplane_last_mode == VIDEO_DRIVER_PACING_OFF,
         "Scanline Sync: reason %u, %u releases", video_st->driver_pacing_reason,
         dplane_waits - waits);
   settings->bools.video_scanline_sync = false;
   run_frames(3);
   CHECK(dplane_waits == waits + 3, "Scanline Sync off: releases resume");

   /* A release the backend could not give: that frame paces as
    * before, the next is asked again */
   waits = dplane_waits;
   dplane_wait_answer_from = 1;
   run_frames(3);
   CHECK(dplane_waits == waits + 3, "failed releases: still asked");
   CHECK(!(runloop_st->pace & RUNLOOP_PACE_DRIVER)
         && runloop_st->driver_pacing_frame_id == 0,
         "failed release: paced as before, no identity");
   dplane_wait_answer_from = 0;

   /* A backend that refuses the mode */
   dplane_set_answer = VIDEO_DRIVER_PACING_REASON_FAILED;
   waits = dplane_waits;
   run_frames(3);
   CHECK(video_st->driver_pacing_reason == VIDEO_DRIVER_PACING_REASON_FAILED
         && dplane_waits == waits,
         "backend failed: reason %u, %u releases",
         video_st->driver_pacing_reason, dplane_waits - waits);
   dplane_set_answer = VIDEO_DRIVER_PACING_REASON_NONE;
   run_frames(2);

   /* Off: taken off once, then left alone */
   dplane_sets = 0;
   settings->uints.video_driver_pacing = VIDEO_DRIVER_PACING_OFF;
   run_frames(5);
   CHECK(video_st->driver_pacing_reason == VIDEO_DRIVER_PACING_REASON_OFF
         && video_st->driver_pacing_mode == VIDEO_DRIVER_PACING_OFF,
         "off: reason %u mode %u", video_st->driver_pacing_reason,
         video_st->driver_pacing_mode);
   CHECK(dplane_sets == 1 && dplane_last_mode == VIDEO_DRIVER_PACING_OFF,
         "off: taken off once (%u sets)", dplane_sets);

   video_st->poke = dplane_saved_poke;
   video_st->current_video = saved_driver;

   /* Threaded video: the pokes cross the wrapper to the driver, the
    * identity rides the frame packet to the video thread, and the
    * wait names the frame handed over */
   settings->uints.video_driver_pacing = VIDEO_DRIVER_PACING_ON;
   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "driver pacing lane, threaded");
   {
      thread_video_t *thr = (thread_video_t*)video_st->data;
      if (thr)
      {
         size_t seen_id, seen_frames;
         /* Focus held, as above: the wrapper is the current driver now */
         video_thread_wait_idle();
         saved_driver            = video_st->current_video;
         focused_driver          = *saved_driver;
         focused_driver.focus    = dplane_focus;
         video_st->current_video = &focused_driver;
         dplane_inner                 = thr->driver;
         dplane_driver                = *thr->driver;
         dplane_driver.frame          = dplane_frame;
         dplane_driver.poke_interface = dplane_get_poke;
         set_driver(thr, &dplane_driver);
         set_poke_from(thr, &dplane_driver);
         retro_atomic_store_release_size(&dplane_seen_id, 0);
         retro_atomic_store_release_size(&dplane_seen_frames, 0);
         dplane_sets = dplane_waits = dplane_marks = 0;
         dplane_last_id = dplane_last_wait_id = 0;
         run_frames(20);
         video_thread_wait_idle();
         seen_id     = retro_atomic_load_acquire_size(&dplane_seen_id);
         seen_frames = retro_atomic_load_acquire_size(&dplane_seen_frames);
         CHECK(video_st->driver_pacing_reason == VIDEO_DRIVER_PACING_REASON_NONE,
               "threaded: reason %u", video_st->driver_pacing_reason);
         CHECK(runloop_st->pace & RUNLOOP_PACE_DRIVER, "threaded: the pacing record");
         CHECK(dplane_waits >= 19 && dplane_last_wait_id + 1 == runloop_st->driver_pacing_frame_id,
               "threaded: %u waits, the last for frame %llu with %llu released",
               dplane_waits, (unsigned long long)dplane_last_wait_id,
               (unsigned long long)runloop_st->driver_pacing_frame_id);
         /* The fake backend releases at once, so the loop runs ahead
          * and a slow presenter (lavapipe) drops frames: what it draws
          * carries an identity the loop released, never one ahead */
         CHECK(seen_frames >= 1 && seen_id <= runloop_st->driver_pacing_frame_id,
               "threaded: the video thread saw %u frames with an identity, last %u, loop at %llu",
               (unsigned)seen_frames, (unsigned)seen_id,
               (unsigned long long)runloop_st->driver_pacing_frame_id);
         CHECK(dplane_mark_kind[7] == VIDEO_DRIVER_PACING_MARK_SIMULATION_START
               && dplane_mark_id[7] == runloop_st->driver_pacing_frame_id,
               "threaded: the release marked the start of the released frame");
         /* Display pacing: the driver's sleep is the hold's release.
          * Every frame is released by the driver, at the content's
          * period, and the presenter's hold stands aside for each
          * rather than sleep a second time. */
         {
            uint64_t stood  = thr->driver_released;
            unsigned before = dplane_waits;
            unsigned content = (unsigned)runloop_content_frame_time_us(
                  video_st->core_hz);
            settings->bools.video_threaded_display_pacing = true;
            run_frames(5);
            video_thread_wait_idle();
            CHECK(video_st->driver_pacing_reason == VIDEO_DRIVER_PACING_REASON_NONE
                  && (runloop_st->pace & RUNLOOP_PACE_DRIVER)
                  && (runloop_st->pace & RUNLOOP_PACE_DISPLAY),
                  "display pacing on: reason %u, pace 0x%x",
                  video_st->driver_pacing_reason, runloop_st->pace);
            CHECK(dplane_waits - before >= 5,
                  "display pacing on: %u releases for 5 frames",
                  dplane_waits - before);
            CHECK(thr->driver_released - stood >= 4,
                  "display pacing on: the hold stood aside for %u of 5 frames",
                  (unsigned)(thr->driver_released - stood));
            CHECK(video_st->driver_pacing_interval_us == content
                  && retro_atomic_load_acquire_size(&dplane_seen_interval)
                     == content,
                  "display pacing on: the content's period %u us to the"
                  " driver (asked %u, presenter saw %u)", content,
                  video_st->driver_pacing_interval_us,
                  (unsigned)retro_atomic_load_acquire_size(
                     &dplane_seen_interval));
            /* A release the driver could not give: the hold holds. The
             * first frame pushed was released before the answer turned. */
            stood                   = thr->driver_released;
            dplane_wait_answer_from = 1;
            run_frames(3);
            CHECK(thr->driver_released - stood <= 1,
                  "display pacing, failed releases: the hold stood aside"
                  " %u times", (unsigned)(thr->driver_released - stood));
            dplane_wait_answer_from = 0;
            settings->bools.video_threaded_display_pacing = false;
            run_frames(5);
            CHECK((runloop_st->pace & RUNLOOP_PACE_DRIVER)
                  && !(runloop_st->pace & RUNLOOP_PACE_DISPLAY),
                  "display pacing off: the driver alone, pace 0x%x",
                  runloop_st->pace);
         }
         video_thread_wait_idle();
         set_driver(thr, dplane_inner);
         set_poke(thr, dplane_inner_poke);
         video_st->current_video = saved_driver;
      }
   }
   settings->uints.video_driver_pacing = VIDEO_DRIVER_PACING_OFF;
   run_frames(2);
   set_threaded_via_setting(false);
   run_frames(2);
   runloop_st->core_poll_type_override = saved_poll;

   command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   if (failures == had)
      fprintf(stderr, "[pass] driver pacing lane\n");
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
/*   Ground truth is the field read on the video thread itself. The    */
/*   video thread can publish between the two reads, so each           */
/*   comparison is bracketed: truth, snapshot, truth again, and only   */
/*   an unchanged bracket is asserted on.                              */
/* ------------------------------------------------------------------ */

/* The wrapped driver's vtable and poke table are read by the video
 * thread on every pass, so a lane that swaps them in does so on that
 * thread, through a blocking command, as the wrapper itself does. */
typedef struct
{
   thread_video_t *thr;
   const video_driver_t *driver;
   const video_poke_interface_t *poke;
   bool set_driver, set_poke;
} worker_install_t;

static uintptr_t worker_install_run(void *data)
{
   worker_install_t *w = (worker_install_t*)data;
   if (w->set_driver)
      w->thr->driver = w->driver;
   if (w->set_poke)
   {
      w->thr->poke   = w->poke;
      retro_atomic_store_release_int(&w->thr->movie_capable,
            w->poke && w->poke->import_movie_frame ? 1 : 0);
   }
   return 0;
}

static void set_driver(thread_video_t *thr, const video_driver_t *driver)
{
   worker_install_t w;
   memset(&w, 0, sizeof(w));
   w.thr        = thr;
   w.driver     = driver;
   w.set_driver = true;
   video_thread_run_blocking(worker_install_run, &w);
}

static void set_poke(thread_video_t *thr, const video_poke_interface_t *poke)
{
   worker_install_t w;
   memset(&w, 0, sizeof(w));
   w.thr      = thr;
   w.poke     = poke;
   w.set_poke = true;
   video_thread_run_blocking(worker_install_run, &w);
}

/* The poke a swapped-in driver reports, installed the same way */
static void set_poke_from(thread_video_t *thr, const video_driver_t *driver)
{
   const video_poke_interface_t *poke = NULL;
   driver->poke_interface(thr->driver_data, &poke);
   set_poke(thr, poke);
}

/* The presenter's fields belong to the video thread and are written
 * without a lock, so ground truth is read on that thread, through a
 * blocking command, while it is between frames. */
typedef struct
{
   thread_video_t *thr;
   uint64_t     repeats, swaps;
   retro_time_t avg, max, render;
   bool         armed, phase, latdisp;
   /* set the count instead of reading it, for the carry lane */
   bool         set_swaps;
   uint64_t     swaps_in;
} worker_truth_t;

static uintptr_t worker_truth_read(void *data)
{
   worker_truth_t *t   = (worker_truth_t*)data;
   thread_video_t *thr = t->thr;
   if (t->set_swaps)
      thr->video_st->swap_count = t->swaps_in;
   t->repeats = thr->frames_repeated;
   t->swaps   = thr->video_st->swap_count;
   t->avg     = thr->latency_avg;
   t->max     = thr->latency_max;
   t->render  = thr->render_time;
   t->armed   = thr->present_repeat;
   t->phase   = thr->phase_from_display;
   t->latdisp = thr->latency_from_display;
   return 0;
}

static void worker_truth(thread_video_t *thr, worker_truth_t *t)
{
   memset(t, 0, sizeof(*t));
   t->thr = thr;
   video_thread_run_blocking(worker_truth_read, t);
}

static void worker_set_swaps(thread_video_t *thr, uint64_t swaps)
{
   worker_truth_t t;
   memset(&t, 0, sizeof(t));
   t.thr       = thr;
   t.set_swaps = true;
   t.swaps_in  = swaps;
   video_thread_run_blocking(worker_truth_read, &t);
}

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

      worker_truth_t truth;

      video_thread_wait_idle();
      worker_truth(thr, &truth);
      t_repeats = truth.repeats;
      t_swaps   = truth.swaps;
      t_avg     = truth.avg;
      t_max     = truth.max;
      t_core    = thr->core_time;
      t_render  = truth.render;
      t_armed   = truth.armed;
      t_phase   = truth.phase;
      t_latdisp = truth.latdisp;
      t_pacing  = thr->display_pacing;

      s_armed   = video_thread_presenter_stats(&s_repeats, &s_phase);
      video_thread_latency_stats(&s_avg, &s_max, &s_latdisp);
      video_thread_pacing_stats(&s_pacing, &s_core, &s_render);
      s_swaps   = video_thread_swap_count();

      worker_truth(thr, &truth);
      t_repeats2 = truth.repeats;
      t_swaps2   = truth.swaps;
      t_avg2     = truth.avg;
      t_max2     = truth.max;
      t_core2    = thr->core_time;
      t_render2  = truth.render;
      t_armed2   = truth.armed;
      t_phase2   = truth.phase;
      t_latdisp2 = truth.latdisp;
      t_pacing2  = thr->display_pacing;

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
      worker_truth_t wt;
      video_thread_wait_idle();
      worker_truth(thr, &wt);
      saved = wt.swaps;
      worker_set_swaps(thr, 0xFFFFFFFEull);
      run_frames(12);
      video_thread_wait_idle();
      seen = video_thread_swap_count();
      worker_truth(thr, &wt);
      truth = wt.swaps;
      CHECK(truth > 0xFFFFFFFFull,
            "the count did not cross the carry (%llu)",
            (unsigned long long)truth);
      CHECK(seen >= 0x100000000ull && truth - seen <= 4,
            "across the carry the count read %llu, field %llu",
            (unsigned long long)seen, (unsigned long long)truth);
      worker_set_swaps(thr, saved);
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
/*   every poll, the runloop asks for the rate every iteration. What   */
/*   the reader gets must be what the driver reported.                  */
/* ------------------------------------------------------------------ */

static video_driver_t                 vplane_driver;
static const video_driver_t          *vplane_inner;
static video_poke_interface_t          vplane_poke;
static const video_poke_interface_t  *vplane_inner_poke;
/* What the fake driver reports. Written by the lane between frames with
 * the worker idle, read on the video thread. */
static retro_atomic_int_t vplane_w, vplane_h, vplane_rate_milli;

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
   {
      unsigned pos  = (unsigned)retro_atomic_load_acquire_int(&thr->read_vp[VIDEO_THREAD_VP_POS]);
      unsigned dims = (unsigned)retro_atomic_load_acquire_int(&thr->read_vp[VIDEO_THREAD_VP_WH]);
      unsigned full = (unsigned)retro_atomic_load_acquire_int(&thr->read_vp[VIDEO_THREAD_VP_FULL_WH]);
      CHECK(dims == vp.dims && pos == vp.pos && full == vp.full_dims,
            "%s: read_vp did not follow the reported viewport (%ux%u vs %ux%u)",
            when, VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims), VIDEO_SCALE_W(vp.dims), VIDEO_SCALE_H(vp.dims));
   }
}

static void lane_viewport_publish(void)
{
   unsigned had = failures;
   video_driver_state_t *video_st = video_state_get_ptr();
   thread_video_t *thr;

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
   set_driver(thr, &vplane_driver);
   set_poke_from(thr, &vplane_driver);

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

   /* The steady state an input driver polls in: the viewport has not
    * changed since the last read above. */
   {
      struct video_viewport vp;
      float        rate;

      video_thread_wait_idle();
      video_driver_get_viewport_info(&vp);
      rate = video_st->poke->get_refresh_rate(video_st->data);
      CHECK(VIDEO_SCALE_W(vp.dims) == 640 && VIDEO_SCALE_H(vp.dims) == 480,
            "the steady read gave %ux%u", VIDEO_SCALE_W(vp.dims), VIDEO_SCALE_H(vp.dims));
      CHECK(rate > 99.9f && rate < 100.1f,
            "the steady read gave rate %.3f", (double)rate);
   }

   video_thread_wait_idle();
   set_driver(thr, vplane_inner);
   set_poke(thr, vplane_inner_poke);
   run_frames(2);
   set_threaded_via_setting(false);
   run_frames(2);

   if (failures == had)
      fprintf(stderr, "[pass] viewport publish lane\n");
}

/* ------------------------------------------------------------------ */
/* Lane: the context API behind the wrapper                           */
/*   A driver without a context driver of its own - Direct3D - is     */
/*   known by its ident, and the frontend asks the current driver.    */
/*   Behind the wrapper that is the wrapper, whose ident is not the   */
/*   driver's, so the API read as NONE and the GPU names keyed on     */
/*   the API were not found. The query sees the wrapped driver.       */
/* ------------------------------------------------------------------ */

static video_driver_t        apilane_driver;
static const video_driver_t *apilane_inner;

static void lane_context_api_behind_wrapper(void)
{
   unsigned had = failures;
   video_driver_state_t *video_st = video_state_get_ptr();
   thread_video_t *thr;
   struct string_list *names;
   union string_list_elem_attr attr;
   enum gfx_ctx_api api;

   set_threaded_via_setting(true);
   run_frames(4);
   expect_wrapper(true, "context api lane");
   if (!(thr = (thread_video_t*)video_st->data))
      return;

   video_thread_wait_idle();
   apilane_inner = thr->driver;
   api           = video_context_driver_get_api();
   /* A driver with a context driver of its own is known by that;
    * one without, by its ident */
   if (api == GFX_CTX_NONE)
   {
      apilane_driver       = *thr->driver;
      apilane_driver.ident = "d3d11";
      set_driver(thr, &apilane_driver);

      CHECK(!strcmp(video_driver_get_ident(), "d3d11"),
            "ident behind the wrapper: %s", video_driver_get_ident());
      api = video_context_driver_get_api();
      CHECK(api == GFX_CTX_DIRECT3D11_API,
            "context api behind the wrapper: %d, wanted %d",
            (int)api, (int)GFX_CTX_DIRECT3D11_API);

      /* and the GPU names the driver published are found by it */
      attr.i = 0;
      names  = string_list_new();
      string_list_append(names, "harness adapter", attr);
      video_driver_set_gpu_api_devices(GFX_CTX_DIRECT3D11_API, names);
      CHECK(video_driver_get_gpu_api_devices(video_context_driver_get_api()) == names,
            "the GPU names are not found by the api the wrapper reports");
      video_driver_set_gpu_api_devices(GFX_CTX_DIRECT3D11_API, NULL);
      string_list_free(names);
   }

   video_thread_wait_idle();
   set_driver(thr, apilane_inner);
   run_frames(2);
   set_threaded_via_setting(false);
   run_frames(2);

   if (failures == had)
      fprintf(stderr, "[pass] context api behind the wrapper lane (api %d)\n",
            (int)api);
}

/* ------------------------------------------------------------------ */
/* Lane: headset calls behind the wrapper                             */
/*   The headset's rates and a stereo request go to the wrapped       */
/*   driver, and its answers come back; the status and the eye poses  */
/*   come from the headset's host. The request and the poses are      */
/*   run on the video thread, whose frame reads what they change.     */
/*   Without the forwards the menu offered the fallback rates, the    */
/*   runloop never learned the headset's rate, and a VR core saw no   */
/*   headset at all. A rate that fits still leaves threaded video on  */
/*   the window's pacing.                                             */
/* ------------------------------------------------------------------ */

static video_driver_t                hslane_driver;
static const video_driver_t         *hslane_inner;
static video_poke_interface_t        hslane_poke;
static const video_poke_interface_t *hslane_inner_poke;
#ifdef HAVE_OPENXR
static unsigned                      hslane_info_calls;
static uintptr_t                     hslane_info_thread;
static bool                          hslane_info_stereo;
static bool                          hslane_info_fail;
static unsigned                      hslane_pose_calls;
static uintptr_t                     hslane_pose_thread;
static bool                          hslane_focused;
static unsigned                      hslane_space_calls;
static unsigned                      hslane_sample_calls;
static uint32_t                      hslane_sample_flags;
static bool                          hslane_sample_fail;
static bool                          hslane_tracking = true;
static unsigned                      hslane_eye_w    = 1832;
static unsigned                      hslane_eye_h    = 1920;
#endif

/* Out of order, and fewer than any fallback list. Twice the harness
 * core's 60 fps, so the headset would pace it. */
static float hslane_hz = 120.0f;
static float hslane_refresh(void *data, float *rates, unsigned cap,
      unsigned *count)
{
   (void)data;
   *count = 0;
   if (cap >= 2)
   {
      rates[0] = 120.0f;
      rates[1] = 80.0f;
      *count   = 2;
   }
   return hslane_hz;
}

static void hslane_get_poke(void *data, const video_poke_interface_t **iface)
{
   hslane_inner->poke_interface(data, &hslane_inner_poke);
   hslane_poke                     = *hslane_inner_poke;
   hslane_poke.get_headset_refresh = hslane_refresh;
   *iface                          = &hslane_poke;
}

#ifdef HAVE_OPENXR
static uint32_t hslane_views_flags(void *data)
{
   (void)data;
   return 1u << GFX_CTX_FLAGS_VIDEO_VIEWS;
}

static uint32_t hslane_inline_flags(void *data)
{
   (void)data;
   return 1u << GFX_CTX_FLAGS_HEADSET_INLINE;
}

static unsigned hslane_host_state(void *user)
{
   (void)user;
   return VIDEO_XR_HEADSET_PRESENTING
        | (hslane_tracking ? VIDEO_XR_HEADSET_TRACKING : 0)
        | (hslane_focused ? VIDEO_XR_HEADSET_FOCUSED : 0);
}

static bool hslane_set_info(void *data, const video_vr_content_info_t *info)
{
   (void)data;
   hslane_info_calls++;
   hslane_info_thread = sthread_get_current_thread_id();
   hslane_info_stereo = info && info->stereo_native;
   return !hslane_info_fail;
}

static unsigned hslane_tokens;
static bool hslane_eye_state(void *user, struct retro_vr_eye_state out[2],
      unsigned *token)
{
   (void)user;
   hslane_pose_calls++;
   hslane_pose_thread = sthread_get_current_thread_id();
   memset(out, 0, 2 * sizeof(*out));
   out[0].position[0] = -0.032f;
   out[1].position[0] =  0.032f;
   /* Which eyes these are, for the give check */
   out[0].position[1] = (float)hslane_tokens;
   *token             = hslane_tokens++;
   return !hslane_sample_fail;
}

/* The video thread parked in a command posted from another thread, as
 * the shader params lane does */
static retro_atomic_int_t hslane_parked;
static retro_atomic_int_t hslane_sampled;
static bool               hslane_sampled_while_parked;

static uintptr_t hslane_park(void *data)
{
   unsigned waited;
   (void)data;
   retro_atomic_store_release_int(&hslane_parked, 1);
   for (waited = 0; waited < 2000
         && !retro_atomic_load_acquire_int(&hslane_sampled); waited++)
      retro_sleep(1);
   hslane_sampled_while_parked =
      retro_atomic_load_acquire_int(&hslane_sampled) != 0;
   return 0;
}

static void hslane_poster(void *data)
{
   (void)data;
   video_thread_run_blocking(hslane_park, NULL);
}

/* Everything queued for the video thread has run: a blocking command
 * runs behind it */
static uintptr_t hslane_noop(void *data) { (void)data; return 0; }
static void hslane_barrier(void)
{
   video_thread_run_blocking(hslane_noop, NULL);
}

/* A frame drawn: the video thread samples after it, past the release
 * wait_idle returns on, so a command behind it waits for the sample */
static void hslane_frame(void)
{
   run_frames(1);
   video_thread_wait_idle();
   hslane_barrier();
}

/* The eyes a frame was drawn from: which, and on which thread */
static unsigned  hslane_give_calls;
static unsigned  hslane_give_token;
static uintptr_t hslane_give_thread;
static void hslane_give(void *user, unsigned token)
{
   (void)user;
   hslane_give_calls++;
   hslane_give_token  = token;
   hslane_give_thread = sthread_get_current_thread_id();
}

static bool hslane_set_space(void *user, enum retro_vr_reference_space *space,
      unsigned *w, unsigned *h)
{
   (void)user;
   hslane_space_calls++;
   *space = RETRO_VR_REFERENCE_SPACE_LOCAL;
   *w     = hslane_eye_w;
   *h     = hslane_eye_h;
   return true;
}

static bool hslane_sample(void *user, unsigned ahead, uint32_t *frame_flags)
{
   (void)user;
   (void)ahead;
   hslane_sample_calls++;
   *frame_flags |= hslane_sample_flags;
   return !hslane_sample_fail;
}

/* The hands: located where, how often, and on which thread. */
static unsigned  hslane_hands_calls;
static uintptr_t hslane_hands_thread;
static bool hslane_locate_hands(void *user)
{
   (void)user;
   hslane_hands_calls++;
   hslane_hands_thread = sthread_get_current_thread_id();
   return true;
}

static bool hslane_controller_poses(void *user,
      struct retro_vr_controller_poses *out)
{
   (void)user;
   memset(out, 0, sizeof(*out));
   out->hands[1].flags = RETRO_VR_HAND_ACTIVE;
   return true;
}

static bool hslane_eye_size(void *user, unsigned *w, unsigned *h)
{
   (void)user;
   *w = hslane_eye_w;
   *h = hslane_eye_h;
   return true;
}
#endif

/* ------------------------------------------------------------------ */
/* Lane: shader parameter writes                                      */
/*   The frontend sets a parameter of the driver's own shader with one */
/*   atomic store, without waiting on the video thread that reads it  */
/*   each frame: parked in a command, the video thread sees the value */
/*   at once. Each write was a blocking round trip before.            */
/* ------------------------------------------------------------------ */

static video_driver_t                splane_driver;
static const video_driver_t         *splane_inner;
static video_poke_interface_t        splane_poke;
static const video_poke_interface_t *splane_inner_poke;
static struct video_shader          *splane_live;
static retro_atomic_int_t            splane_held;
static retro_atomic_int_t            splane_written;
static bool                          splane_written_while_parked;
static float                         splane_live_seen;

static struct video_shader *splane_get_shader(void *data)
{
   (void)data;
   return splane_live;
}

static void splane_get_poke(void *data, const video_poke_interface_t **iface)
{
   splane_inner->poke_interface(data, &splane_inner_poke);
   splane_poke                    = *splane_inner_poke;
   splane_poke.get_current_shader = splane_get_shader;
   *iface                         = &splane_poke;
}

/* The video thread parked in a command posted from another thread,
 * until the main thread has written or 2 s have passed: a write that
 * waited for the video thread fails the check rather than the run */
static uintptr_t splane_park(void *data)
{
   unsigned waited;
   (void)data;
   retro_atomic_store_release_int(&splane_held, 1);
   for (waited = 0; waited < 2000
         && !retro_atomic_load_acquire_int(&splane_written); waited++)
      retro_sleep(1);
   splane_written_while_parked =
      retro_atomic_load_acquire_int(&splane_written) != 0;
   splane_live_seen = video_shader_param_get(&splane_live->parameters[1]);
   return 0;
}

static void splane_poster(void *data)
{
   (void)data;
   video_thread_run_blocking(splane_park, NULL);
}

static void lane_shader_params(void)
{
   unsigned had = failures;
   video_driver_state_t *video_st = video_state_get_ptr();
   video_shader_ctx_t    shader_info;
   thread_video_t       *thr;
   sthread_t            *poster;

   set_threaded_via_setting(true);
   run_frames(4);
   expect_wrapper(true, "shader params lane");
   if (!(thr = (thread_video_t*)video_st->data))
      return;
   if (!(splane_live = (struct video_shader*)calloc(1, sizeof(*splane_live))))
      return;
   splane_live->num_parameters = 2;
   video_shader_param_set(&splane_live->parameters[0], 0.25f);
   video_shader_param_set(&splane_live->parameters[1], 0.5f);

   video_thread_wait_idle();
   splane_inner                 = thr->driver;
   splane_driver                = *thr->driver;
   splane_driver.poke_interface = splane_get_poke;
   set_driver(thr, &splane_driver);
   set_poke_from(thr, &splane_driver);

   CHECK(     video_shader_driver_get_current_shader(&shader_info)
         && shader_info.data == splane_live,
         "shader params lane: the frontend was not given the driver's "
         "shader");

   retro_atomic_store_release_int(&splane_held, 0);
   retro_atomic_store_release_int(&splane_written, 0);
   if ((poster = sthread_create(splane_poster, NULL)))
   {
      while (!retro_atomic_load_acquire_int(&splane_held))
         retro_sleep(1);
      video_shader_driver_set_parameter(splane_live, 1, 0.75f);
      retro_atomic_store_release_int(&splane_written, 1);
      sthread_join(poster);
   }
   CHECK(poster && splane_written_while_parked,
         "shader params lane: the write waited for the video thread");
   CHECK(splane_live_seen == 0.75f,
         "shader params lane: the video thread read %.2f, not the value "
         "written", (double)splane_live_seen);
   CHECK(video_shader_param_get(&splane_live->parameters[0]) == 0.25f,
         "shader params lane: a parameter not written changed");

   video_thread_wait_idle();
   set_driver(thr, splane_inner);
   set_poke_from(thr, splane_inner);
   free(splane_live);
   splane_live = NULL;
   run_frames(2);

   if (failures == had)
      fprintf(stderr, "[pass] shader params lane: written without "
            "waiting, read by the video thread\n");
}

static void lane_headset_calls(void)
{
   unsigned had = failures;
   video_driver_state_t *video_st = video_state_get_ptr();
   thread_video_t *thr;
   unsigned values[8];
   float rates[4];
   unsigned n;
   unsigned count = 0;
   float hz       = 0.0f;

   set_threaded_via_setting(true);
   run_frames(4);
   expect_wrapper(true, "headset lane");
   if (!(thr = (thread_video_t*)video_st->data))
      return;

   video_thread_wait_idle();
   hslane_inner                         = thr->driver;
   hslane_driver                        = *thr->driver;
   hslane_driver.poke_interface         = hslane_get_poke;
#ifdef HAVE_OPENXR
   hslane_driver.set_vr_content_info    = hslane_set_info;
#endif
   set_driver(thr, &hslane_driver);
   set_poke_from(thr, &hslane_driver);

   /* The menu's list: the two fixed choices, then the driver's rates
    * in order, not the list offered while no headset answers. */
   n = video_driver_headset_rate_choices(values, 8);
   CHECK(n == 4 && values[2] == 80 && values[3] == 120,
         "headset lane: %u rate choices (%u, %u, ...), wanted the "
         "driver's 80 and 120", n, n > 2 ? values[2] : 0,
         n > 3 ? values[3] : 0);
   if (video_st->poke && video_st->poke->get_headset_refresh)
      hz = video_st->poke->get_headset_refresh(video_st->data, rates, 4,
            &count);
   CHECK(hz == 120.0f && count == 2,
         "headset lane: the wrapper's poke answered %.1f Hz with %u rates",
         (double)hz, count);

#ifdef HAVE_OPENXR
   /* The runloop's poll, every iteration */
   run_frames(2);
   CHECK(video_st->headset_hz == 120.0f,
         "headset lane: the runloop took the headset's rate as %.1f Hz",
         (double)video_st->headset_hz);

   /* The rate fits, and vsync is what lets a headset pace, but threaded
    * video stays on the window's pacing. */
   {
      settings_t *settings = config_get_ptr();
      settings->bools.video_vsync = true;
      run_frames(2);
      CHECK(video_st->headset_interval == 0,
            "headset lane: the headset paced threaded video (interval %u)",
            video_st->headset_interval);
      settings->bools.video_vsync = false;
      run_frames(2);
   }

   CHECK(video_driver_headset_view_toggle() == VIDEO_HEADSET_VIEW_LAST
         && video_st->headset_view == VIDEO_HEADSET_VIEW_IMMERSIVE,
         "headset lane: the Headset View changed without a headset");

   {
      video_xr_host_t host;
      memset(&host, 0, sizeof(host));
      host.state     = hslane_host_state;
      host.eye_state = hslane_eye_state;
      host.give      = hslane_give;
      host.set_space = hslane_set_space;
      host.sample    = hslane_sample;
      host.eye_size  = hslane_eye_size;
      host.locate_hands     = hslane_locate_hands;
      host.controller_poses = hslane_controller_poses;
      video_driver_set_xr_host(&host);
   }
   CHECK(video_driver_views_status() & RETRO_VIDEO_VIEWS_STATUS_HMD,
         "headset lane: the views status did not carry the host's HMD");

   /* Headset View: Stereoscopic drops HMD, Flat also STEREO. */
   {
      unsigned view   = video_driver_headset_view_toggle();
      unsigned status = video_driver_views_status();
      CHECK(view == VIDEO_HEADSET_VIEW_STEREOSCOPIC
            && !(status & RETRO_VIDEO_VIEWS_STATUS_HMD)
            &&  (status & RETRO_VIDEO_VIEWS_STATUS_STEREO),
            "headset lane: Stereoscopic view %u gave status 0x%x",
            view, status);
      view   = video_driver_headset_view_toggle();
      status = video_driver_views_status();
      CHECK(view == VIDEO_HEADSET_VIEW_FLAT
            && !(status & (RETRO_VIDEO_VIEWS_STATUS_HMD
                         | RETRO_VIDEO_VIEWS_STATUS_STEREO)),
            "headset lane: Flat view %u gave status 0x%x", view, status);
      view   = video_driver_headset_view_toggle();
      CHECK(view == VIDEO_HEADSET_VIEW_IMMERSIVE
            && (video_driver_views_status() & RETRO_VIDEO_VIEWS_STATUS_HMD),
            "headset lane: Immersive view %u did not give HMD back", view);
   }

   hslane_focused = false;
   CHECK(!video_driver_headset_focused(),
         "headset lane: the headset was focused without the host saying so");
   hslane_focused = true;
   CHECK(video_driver_headset_focused(),
         "headset lane: the host's focus did not reach the frontend");
   hslane_focused = false;

   {
      video_vr_content_info_t info;
      bool ok = false;
      memset(&info, 0, sizeof(info));
      info.stereo_native = true;
      hslane_info_calls  = 0;
      if (video_st->current_video->set_vr_content_info)
         ok = video_st->current_video->set_vr_content_info(video_st->data,
               &info);
      CHECK(ok && hslane_info_calls == 1 && hslane_info_stereo,
            "headset lane: a stereo request reached the driver %u times "
            "(answer %d)", hslane_info_calls, ok);
      CHECK(!hslane_info_calls
            || hslane_info_thread == sthread_get_thread_id(thr->thread),
            "headset lane: the stereo request ran off the video thread");
   }

   /* The video thread samples after each frame it draws; the core reads
    * the newest without waiting on it, and the eyes it drew from are
    * given there before its frame is drawn */
   {
      struct retro_vr_frame_state fs;
      unsigned calls;
      bool ok;
      memset(&fs, 0, sizeof(fs));
      hslane_pose_calls = 0;
      video_driver_vr_sample_tracking();
      run_frames(2);
      video_thread_wait_idle();
      video_driver_vr_sample_tracking();
      calls = hslane_pose_calls;
      ok    = video_driver_get_vr_frame_state(&fs);
      CHECK(ok && calls >= 1 && fs.eyes[1].position[0] == 0.032f,
            "headset lane: the eye poses were sampled %u times "
            "(answer %d)", calls, ok);
      CHECK(hslane_pose_calls == calls,
            "headset lane: reading the frame state asked the host");
      CHECK(!calls
            || hslane_pose_thread == sthread_get_thread_id(thr->thread),
            "headset lane: the eye poses were read off the video thread");
      hslane_give_calls = 0;
      run_frames(1);
      video_thread_wait_idle();
      CHECK(     hslane_give_calls == 1
            && hslane_give_token == (unsigned)fs.eyes[0].position[1]
            && hslane_give_thread == sthread_get_thread_id(thr->thread),
            "headset lane: the eyes the core drew from were given %u "
            "times (token %u)", hslane_give_calls, hslane_give_token);
   }

   /* Sampling does not wait for the video thread: parked in a command
    * from another thread, it finds the sample taken */
   {
      sthread_t *poster;
      retro_atomic_store_release_int(&hslane_parked, 0);
      retro_atomic_store_release_int(&hslane_sampled, 0);
      if ((poster = sthread_create(hslane_poster, NULL)))
      {
         struct retro_vr_frame_state fs;
         while (!retro_atomic_load_acquire_int(&hslane_parked))
            retro_sleep(1);
         video_driver_vr_sample_tracking();
         video_driver_get_vr_frame_state(&fs);
         retro_atomic_store_release_int(&hslane_sampled, 1);
         sthread_join(poster);
      }
      CHECK(poster && hslane_sampled_while_parked,
            "headset lane: sampling waited for the video thread");
   }

   /* A core sends its map every frame: the same one is applied once. */
   {
      struct retro_video_view v[2];
      struct retro_video_views views;
      memset(v, 0, sizeof(v));
      v[0].eye    = RETRO_VIDEO_VIEW_EYE_LEFT;
      v[0].width  = 640;
      v[0].height = 480;
      v[1]        = v[0];
      v[1].eye    = RETRO_VIDEO_VIEW_EYE_RIGHT;
      v[1].x      = 640;
      memset(&views, 0, sizeof(views));
      views.views           = v;
      views.num_views       = 2;
      views.flags           = RETRO_VIDEO_VIEWS_FLAG_TRACKED;
      views.reference_space = RETRO_VR_REFERENCE_SPACE_STAGE;
      hslane_space_calls    = 0;
      hslane_info_calls     = 0;
      video_driver_set_views(&views);
      views.recommended_view_width  = 0;
      views.recommended_view_height = 0;
      video_driver_set_views(&views);
      CHECK(hslane_space_calls == 1 && hslane_info_calls == 1,
            "headset lane: a map sent twice was applied %u times",
            hslane_space_calls);
      CHECK(     views.recommended_view_width == 1832
            && views.recommended_view_height == 1920,
            "headset lane: the second send lost the eye size (%ux%u)",
            views.recommended_view_width, views.recommended_view_height);
      views.reference_space = RETRO_VR_REFERENCE_SPACE_LOCAL;
      video_driver_set_views(&views);
      CHECK(hslane_space_calls == 2,
            "headset lane: a map in another space was not applied");

      /* A refused map is asked again, not answered from the old grant. */
      hslane_info_fail      = true;
      hslane_info_calls     = 0;
      views.reference_space = RETRO_VR_REFERENCE_SPACE_STAGE;
      video_driver_set_views(&views);
      video_driver_set_views(&views);
      CHECK(hslane_info_calls == 2,
            "headset lane: a refused map was not asked again (%u)",
            hslane_info_calls);
      hslane_info_fail      = false;
      video_driver_set_views(&views);

      /* After a refusal the same map reaches the driver again. */
      hslane_info_calls     = 0;
      hslane_info_fail      = true;
      views.reference_space = RETRO_VR_REFERENCE_SPACE_LOCAL;
      video_driver_set_views(&views);
      hslane_info_fail      = false;
      views.reference_space = RETRO_VR_REFERENCE_SPACE_STAGE;
      video_driver_set_views(&views);
      CHECK(hslane_info_calls == 2,
            "headset lane: a map after a refusal was answered from the cache (%u)",
            hslane_info_calls);

      /* The core going drops its content on the video thread, once,
       * queued there rather than waited for: parked, the video thread
       * has not had it when the call returns. */
      {
         sthread_t *poster;
         hslane_info_calls = 0;
         retro_atomic_store_release_int(&hslane_parked, 0);
         retro_atomic_store_release_int(&hslane_sampled, 0);
         if ((poster = sthread_create(hslane_poster, NULL)))
         {
            while (!retro_atomic_load_acquire_int(&hslane_parked))
               retro_sleep(1);
            video_driver_vr_content_clear();
            CHECK(hslane_info_calls == 0,
                  "headset lane: the core's going ran off the video thread");
            retro_atomic_store_release_int(&hslane_sampled, 1);
            sthread_join(poster);
         }
         CHECK(poster && hslane_sampled_while_parked,
               "headset lane: the core's going waited for the video thread");
      }
      hslane_barrier();
      CHECK(hslane_info_calls == 1 && !hslane_info_stereo
            && hslane_info_thread == sthread_get_thread_id(thr->thread),
            "headset lane: the core's going reached the driver %u times",
            hslane_info_calls);
      video_driver_vr_content_clear();
      hslane_barrier();
      CHECK(hslane_info_calls == 1,
            "headset lane: a second clear reached the driver");
      views.num_views = 0;
      video_driver_set_views(&views);
   }

   /* While the headset tracks, a TRACKED map is one pair of eyes, any
    * map learns the eye size, and a size that changes behind the
    * core's back comes as TARGET_RESIZED, once. */
   {
      struct retro_video_view v[2];
      struct retro_video_views views;
      struct retro_vr_frame_state fs;
      memset(v, 0, sizeof(v));
      v[0].width      = 400;
      v[0].height     = 240;
      v[1]            = v[0];
      v[1].y          = 240;
      v[1].screen     = 1;
      memset(&views, 0, sizeof(views));
      views.views     = v;
      views.num_views = 2;
      views.flags     = RETRO_VIDEO_VIEWS_FLAG_TRACKED;
      CHECK(!video_driver_set_views(&views),
            "headset lane: a TRACKED map without a pair of eyes was taken");
      views.flags = 0;
      CHECK(     video_driver_set_views(&views)
            && views.recommended_view_width == 1832
            && views.recommended_view_height == 1920,
            "headset lane: a flat map was given %ux%u, not the eye size",
            views.recommended_view_width, views.recommended_view_height);

      hslane_tracking = false;
      views.flags     = RETRO_VIDEO_VIEWS_FLAG_TRACKED;
      CHECK(     video_driver_set_views(&views)
            && !views.recommended_view_width
            && !views.recommended_view_height,
            "headset lane: without tracking the flag counted (%ux%u)",
            views.recommended_view_width, views.recommended_view_height);
      hslane_tracking = true;

      /* The eyes side by side on screen 0. */
      v[0].eye              = RETRO_VIDEO_VIEW_EYE_LEFT;
      v[1]                  = v[0];
      v[1].eye              = RETRO_VIDEO_VIEW_EYE_RIGHT;
      v[1].x                = 400;
      views.reference_space = RETRO_VR_REFERENCE_SPACE_LOCAL;
      CHECK(video_driver_set_views(&views),
            "headset lane: the eye pair was refused");
      hslane_eye_w = 2000;
      hslane_eye_h = 2016;
      video_driver_vr_driver_changed();
      video_driver_vr_sample_tracking();
      memset(&fs, 0, sizeof(fs));
      CHECK(     video_driver_get_vr_frame_state(&fs)
            && (fs.flags & RETRO_VR_FRAME_TARGET_RESIZED),
            "headset lane: a new eye size after a reinit was not flagged "
            "(flags %u)", (unsigned)fs.flags);
      video_driver_vr_sample_tracking();
      memset(&fs, 0, sizeof(fs));
      CHECK(     video_driver_get_vr_frame_state(&fs)
            && !(fs.flags & RETRO_VR_FRAME_TARGET_RESIZED),
            "headset lane: TARGET_RESIZED came twice");
      views.recommended_view_width  = 0;
      views.recommended_view_height = 0;
      CHECK(     video_driver_set_views(&views)
            && views.recommended_view_width == 2000
            && views.recommended_view_height == 2016,
            "headset lane: the map was not answered with the new size "
            "(%ux%u)", views.recommended_view_width,
            views.recommended_view_height);
      hslane_eye_w    = 1832;
      hslane_eye_h    = 1920;
      views.num_views = 0;
      hslane_info_calls = 0;
      CHECK(video_driver_set_views(&views),
            "headset lane: an empty TRACKED map was refused");
      CHECK(hslane_info_calls == 1 && !hslane_info_stereo,
            "headset lane: an empty TRACKED map left the content up (%u)",
            hslane_info_calls);

      /* A flat map, or an empty one, after a tracked map drops it too. */
      views.num_views = 2;
      CHECK(video_driver_set_views(&views),
            "headset lane: the eye pair was refused again");
      hslane_info_calls = 0;
      views.flags       = 0;
      CHECK(video_driver_set_views(&views),
            "headset lane: a flat map was refused");
      CHECK(hslane_info_calls == 1 && !hslane_info_stereo,
            "headset lane: a flat map left the content up (%u)",
            hslane_info_calls);
      views.flags     = RETRO_VIDEO_VIEWS_FLAG_TRACKED;
      CHECK(video_driver_set_views(&views),
            "headset lane: the eye pair was refused a third time");
      hslane_info_calls = 0;
      views.num_views   = 0;
      views.flags       = 0;
      CHECK(video_driver_set_views(&views),
            "headset lane: an empty map was refused");
      CHECK(hslane_info_calls == 1 && !hslane_info_stereo,
            "headset lane: an empty map left the content up (%u)",
            hslane_info_calls);

      /* A size the core is handed with its map is no news to it. */
      views.flags           = RETRO_VIDEO_VIEWS_FLAG_TRACKED;
      views.num_views       = 2;
      views.reference_space = RETRO_VR_REFERENCE_SPACE_LOCAL;
      CHECK(video_driver_set_views(&views),
            "headset lane: the eye pair was refused a fourth time");
      hslane_eye_w          = 2000;
      hslane_eye_h          = 2016;
      views.reference_space = RETRO_VR_REFERENCE_SPACE_STAGE;
      CHECK(video_driver_set_views(&views),
            "headset lane: the eye pair in another space was refused");
      video_driver_vr_sample_tracking();
      memset(&fs, 0, sizeof(fs));
      CHECK(     video_driver_get_vr_frame_state(&fs)
            && !(fs.flags & RETRO_VR_FRAME_TARGET_RESIZED),
            "headset lane: a size given with the map came as TARGET_RESIZED");

      /* The core going takes a resize it was due with it. */
      hslane_eye_w = 1832;
      hslane_eye_h = 1920;
      video_driver_vr_driver_changed();
      video_driver_vr_content_clear();
      video_driver_vr_sample_tracking();
      memset(&fs, 0, sizeof(fs));
      CHECK(     video_driver_get_vr_frame_state(&fs)
            && !(fs.flags & RETRO_VR_FRAME_TARGET_RESIZED),
            "headset lane: a resize outlived the core's content");

      /* A map sent from retro_run is sampled before the poses answer. */
      {
         runloop_state_t *runloop_st = runloop_state_get_ptr();
         struct retro_vr_frame_state fs2;
         views.reference_space       = RETRO_VR_REFERENCE_SPACE_LOCAL;
         runloop_st->core_vr_content = false;
         hslane_sample_calls         = 0;
         CHECK(runloop_environment_cb(RETRO_ENVIRONMENT_SET_VIDEO_VIEWS,
               &views), "headset lane: the environment refused the map");
         memset(&fs2, 0, sizeof(fs2));
         CHECK(     runloop_environment_cb(
                  RETRO_ENVIRONMENT_GET_VR_FRAME_STATE, &fs2)
               && hslane_sample_calls == 1,
               "headset lane: the poses after a map were sampled %u times",
               hslane_sample_calls);
         runloop_st->core_vr_content = false;
         video_driver_vr_content_clear();
      }

      /* A flag waits for a frame state that returns it: through samples
       * while the headset does not track, and maps sent then, which
       * are given no size; and from a sample that failed. The core
       * going takes it. */
      views.flags           = RETRO_VIDEO_VIEWS_FLAG_TRACKED;
      views.num_views       = 2;
      views.reference_space = RETRO_VR_REFERENCE_SPACE_LOCAL;
      CHECK(video_driver_set_views(&views),
            "headset lane: the eye pair was refused a fifth time");
      hslane_eye_w    = 2000;
      hslane_eye_h    = 2016;
      video_driver_vr_driver_changed();
      hslane_tracking = false;
      video_driver_set_views(&views);
      video_driver_vr_sample_tracking();
      hslane_tracking = true;
      video_driver_vr_sample_tracking();
      memset(&fs, 0, sizeof(fs));
      CHECK(     video_driver_get_vr_frame_state(&fs)
            && (fs.flags & RETRO_VR_FRAME_TARGET_RESIZED),
            "headset lane: a resize was lost while the headset did not "
            "track (flags %u)", (unsigned)fs.flags);
      hslane_sample_flags = RETRO_VR_FRAME_RECENTERED;
      hslane_sample_fail  = true;
      hslane_frame();
      video_driver_vr_sample_tracking();
      CHECK(!video_driver_get_vr_frame_state(&fs),
            "headset lane: the poses of a failed sample answered");
      hslane_sample_flags = 0;
      hslane_sample_fail  = false;
      hslane_frame();
      video_driver_vr_sample_tracking();
      memset(&fs, 0, sizeof(fs));
      CHECK(     video_driver_get_vr_frame_state(&fs)
            && fs.flags == RETRO_VR_FRAME_RECENTERED,
            "headset lane: a failed sample's recenter came as flags %u",
            (unsigned)fs.flags);
      video_driver_vr_sample_tracking();
      memset(&fs, 0, sizeof(fs));
      CHECK(video_driver_get_vr_frame_state(&fs) && !fs.flags,
            "headset lane: flags a frame state returned came again (%u)",
            (unsigned)fs.flags);
      hslane_eye_w        = 1832;
      hslane_eye_h        = 1920;
      hslane_sample_flags = RETRO_VR_FRAME_RECENTERED;
      hslane_sample_fail  = true;
      hslane_frame();
      video_driver_vr_sample_tracking();
      hslane_sample_flags = 0;
      hslane_sample_fail  = false;
      video_driver_vr_content_clear();
      video_driver_vr_sample_tracking();
      hslane_frame();
      video_driver_vr_sample_tracking();
      memset(&fs, 0, sizeof(fs));
      CHECK(video_driver_get_vr_frame_state(&fs) && !fs.flags,
            "headset lane: flags outlived the core's content (%u)",
            (unsigned)fs.flags);
   }

   /* The hands: not located until a core asks, then with each sample,
    * on the video thread - under the wrapper the first ask answers from
    * the next sample rather than locating them here; not for the next
    * core. */
   {
      struct retro_vr_controller_poses hands;
      unsigned calls;
      hslane_hands_calls = 0;
      video_driver_vr_sample_tracking();
      hslane_frame();
      video_driver_vr_sample_tracking();
      CHECK(hslane_hands_calls == 0,
            "headset lane: the hands were located %u times unasked",
            hslane_hands_calls);
      memset(&hands, 0, sizeof(hands));
      CHECK(     !video_driver_get_vr_controller_poses(&hands)
            && hslane_hands_calls == 0,
            "headset lane: the first ask located the hands off the video "
            "thread (%u)", hslane_hands_calls);
      hslane_frame();
      video_driver_vr_sample_tracking();
      CHECK(     video_driver_get_vr_controller_poses(&hands)
            && hslane_hands_calls >= 1
            && hands.hands[1].flags == RETRO_VR_HAND_ACTIVE,
            "headset lane: asked hands were located %u times by the next "
            "sample", hslane_hands_calls);
      CHECK(hslane_hands_thread == sthread_get_thread_id(thr->thread),
            "headset lane: the hands were located off the video thread");
      video_driver_vr_content_clear();
      calls = hslane_hands_calls;
      video_driver_vr_sample_tracking();
      hslane_frame();
      CHECK(hslane_hands_calls == calls,
            "headset lane: the hands outlived the core's content");
   }

#endif

   video_thread_wait_idle();
   set_driver(thr, hslane_inner);
   set_poke(thr, hslane_inner_poke);
#ifdef HAVE_OPENXR
   /* No headset again: the poll puts the configured rate back. */
   run_frames(2);
   CHECK(video_st->headset_hz == 0.0f,
         "headset lane: the headset's rate stayed at %.1f Hz",
         (double)video_st->headset_hz);
#endif
   run_frames(2);
   set_threaded_via_setting(false);
   run_frames(2);
#ifdef HAVE_OPENXR
   /* The driver's free forgets the host. */
   CHECK(!video_driver_xr_headset(),
         "headset lane: the host outlived its driver");
   video_driver_set_xr_host(NULL);
   /* A view left at Flat narrows nothing once the headset is gone:
    * a desktop Stereo Mode keeps STEREO. */
   {
      settings_t *settings  = config_get_ptr();
      unsigned stereo       = settings->uints.video_stereo_mode;
      uint32_t (*get_flags)(void*) =
         video_st->current_video_context.get_flags;
      unsigned status;
      video_st->current_video_context.get_flags = hslane_views_flags;
      settings->uints.video_stereo_mode = VIDEO_STEREO_MODE_ANAGLYPH;
      video_st->headset_view            = VIDEO_HEADSET_VIEW_FLAT;
      status                            = video_driver_views_status();
      CHECK(status & RETRO_VIDEO_VIEWS_STATUS_STEREO,
            "headset lane: Flat took STEREO from a desktop Stereo Mode "
            "(status 0x%x)", status);
      video_st->headset_view            = VIDEO_HEADSET_VIEW_IMMERSIVE;
      settings->uints.video_stereo_mode = stereo;
      video_st->current_video_context.get_flags = get_flags;
   }
#endif

   if (failures == had)
      fprintf(stderr, "[pass] headset calls lane\n");
}

/* ------------------------------------------------------------------ */
/* Lane: presenter repeats                                            */
/*   With video_threaded_present_repeat on and a driver that can      */
/*   present its last frame again (the null driver can), a stalled    */
/*   core must not stall the display: the worker keeps presenting at  */
/*   the refresh period, each repeat advancing swap_count by one and  */
/*   never counting as a rendered frame for the idle barrier.         */
/* ------------------------------------------------------------------ */
/* Lane: GET_TARGET_REFRESH_RATE with a headset                       */
/*   The headset's rate is the target only while the runloop runs at  */
/*   it: a headset's own context (the Quest's) with no fit. A fitting */
/*   headset paces the core at its own rate, a window paces at the    */
/*   configured one, and the wrapper holds the core at its own, so    */
/*   each of those targets the configured rate. A core following the  */
/*   answer at 120 on a 60 Hz config rendered twice the frames.       */
/* ------------------------------------------------------------------ */

#ifdef HAVE_OPENXR
static float hslane_target(void)
{
   float hz = 0.0f;
   CHECK(runloop_environment_cb(RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE,
         &hz), "target rate lane: the environment refused the rate");
   return hz;
}
#endif

static void lane_headset_target_rate(void)
{
#ifdef HAVE_OPENXR
   unsigned had = failures;
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   float saved_hz                 = settings->floats.video_refresh_rate;
   bool saved_vsync               = settings->bools.video_vsync;
   uint32_t (*get_flags)(void*)   = video_st->current_video_context.get_flags;
   const video_poke_interface_t *saved_poke;
   thread_video_t *thr;
   video_xr_host_t host;

   settings->floats.video_refresh_rate = 60.0f;
   settings->bools.video_vsync         = true;
   memset(&host, 0, sizeof(host));
   host.state     = hslane_host_state;
   host.eye_state = hslane_eye_state;
   host.give      = hslane_give;
   host.set_space = hslane_set_space;
   host.sample    = hslane_sample;
   host.eye_size  = hslane_eye_size;

   /* Threaded: the wrapper holds the core at its own rate, whatever
    * the headset's. A driver's free forgets the host, so it is set
    * after each reinit. */
   set_threaded_via_setting(true);
   run_frames(2);
   expect_wrapper(true, "target rate lane");
   if (!(thr = (thread_video_t*)video_st->data))
      return;
   video_driver_set_xr_host(&host);
   CHECK(video_driver_xr_headset() & VIDEO_XR_HEADSET_PRESENTING,
         "target rate lane: the host is not presenting");
   video_thread_wait_idle();
   hslane_inner                 = thr->driver;
   hslane_driver                = *thr->driver;
   hslane_driver.poke_interface = hslane_get_poke;
   set_driver(thr, &hslane_driver);
   set_poke_from(thr, &hslane_driver);
   hslane_hz = 120.0f;
   video_st->current_video_context.get_flags = hslane_inline_flags;
   run_frames(2);
   CHECK(video_st->headset_hz == 120.0f && !video_st->headset_interval,
         "target rate lane: threaded took %.1f Hz, interval %u",
         (double)video_st->headset_hz, video_st->headset_interval);
   CHECK(hslane_target() == 60.0f,
         "target rate lane: threaded video targeted %.1f Hz on a 120 Hz "
         "headset", (double)hslane_target());
   hslane_hz = 90.0f;
   run_frames(2);
   CHECK(hslane_target() == 60.0f,
         "target rate lane: threaded video targeted %.1f Hz on a 90 Hz "
         "headset", (double)hslane_target());
   video_thread_wait_idle();
   set_driver(thr, hslane_inner);
   set_poke(thr, hslane_inner_poke);
   video_st->current_video_context.get_flags = get_flags;
   hslane_hz = 120.0f;
   run_frames(2);
   set_threaded_via_setting(false);
   run_frames(2);
   expect_wrapper(false, "target rate lane");
   video_driver_set_xr_host(&host);
   CHECK(video_driver_xr_headset() & VIDEO_XR_HEADSET_PRESENTING,
         "target rate lane: the host is not presenting (direct)");

   /* Direct: the poke is the driver's own. */
   saved_poke                      = video_st->poke;
   hslane_poke                     = *saved_poke;
   hslane_poke.get_headset_refresh = hslane_refresh;
   video_st->poke                  = &hslane_poke;
   get_flags = video_st->current_video_context.get_flags;

   /* A headset's own context: the fit paces at 60, so 60 is targeted;
    * with no fit the core runs at the headset's rate. */
   video_st->current_video_context.get_flags = hslane_inline_flags;
   run_frames(2);
   CHECK(video_st->headset_hz == 120.0f && video_st->headset_interval == 2,
         "target rate lane: the headset took %.1f Hz, interval %u",
         (double)video_st->headset_hz, video_st->headset_interval);
   CHECK(hslane_target() == 60.0f,
         "target rate lane: a fitting headset targeted %.1f Hz",
         (double)hslane_target());
   hslane_hz = 90.0f;
   run_frames(2);
   CHECK(video_st->headset_interval == 0 && hslane_target() == 90.0f,
         "target rate lane: a headset with no fit targeted %.1f Hz "
         "(interval %u)", (double)hslane_target(), video_st->headset_interval);

   /* A window: its vsync paces at the configured rate either way. The
    * rates are adjusted as the headset's rate changes. */
   video_st->current_video_context.get_flags = hslane_views_flags;
   hslane_hz = 120.0f;
   run_frames(2);
   CHECK(video_st->headset_interval == 2 && hslane_target() == 60.0f,
         "target rate lane: a window with a 120 Hz headset targeted %.1f Hz",
         (double)hslane_target());
   hslane_hz = 90.0f;
   run_frames(2);
   CHECK(video_st->headset_interval == 0 && hslane_target() == 60.0f,
         "target rate lane: a window with a 90 Hz headset targeted %.1f Hz",
         (double)hslane_target());

   video_st->current_video_context.get_flags = get_flags;
   video_st->poke                            = saved_poke;
   video_driver_set_xr_host(NULL);
   run_frames(2);
   CHECK(video_st->headset_hz == 0.0f && hslane_target() == 60.0f,
         "target rate lane: without a headset %.1f Hz, targeted %.1f Hz",
         (double)video_st->headset_hz, (double)hslane_target());
   settings->floats.video_refresh_rate = saved_hz;
   settings->bools.video_vsync         = saved_vsync;
   run_frames(2);

   if (failures == had)
      fprintf(stderr, "[pass] target rate lane: a headset is the target "
            "only while the runloop runs at its rate\n");
#endif
}

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
   /* Both counts from one read on the video thread, so a repeat that
    * lands between two reads cannot split the pair */
   { worker_truth_t wt; worker_truth(thr, &wt); rep0 = wt.repeats; swaps0 = wt.swaps; }
   retro_sleep(70);
   video_thread_wait_idle();          /* must not block on repeats */
   { worker_truth_t wt; worker_truth(thr, &wt); rep1 = wt.repeats; swaps1 = wt.swaps; }
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
   { worker_truth_t wt; worker_truth(thr, &wt); rep0 = wt.repeats; swaps0 = wt.swaps; }
   retro_sleep(70);
   video_thread_wait_idle();
   { worker_truth_t wt; worker_truth(thr, &wt); rep1 = wt.repeats; swaps1 = wt.swaps; }
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
   { worker_truth_t wt; worker_truth(thr, &wt); rep0 = wt.repeats; }
   retro_sleep(70);
   video_thread_wait_idle();
   { worker_truth_t wt; worker_truth(thr, &wt); rep1 = wt.repeats; }
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

typedef struct
{
   thread_video_t *thr;
   video_frame_info_t *info;
   uint64_t expected, seen[2], after[2];
   unsigned current;
   bool returned[2];
#ifdef HAVE_GFX_WIDGETS
   bool inactive_widgets_visible, retired_widgets_visible;
   bool retired_widgets_checked;
#endif
} replay_swap_t;

static replay_swap_t *replay_swap;

static bool replay_swap_frame(void *data, const void *frame,
      unsigned dims, uint64_t count, unsigned pitch, const char *msg,
      video_frame_info_t *info)
{
   (void)data;
   (void)frame;
   (void)dims;
   (void)count;
   (void)pitch;
   (void)msg;
   replay_swap->seen[replay_swap->current] = info->swap_count;
   return true;
}

static uintptr_t replay_swap_run(void *data)
{
   replay_swap_t *t = (replay_swap_t*)data;
   thread_video_t *thr = t->thr;
   const video_driver_t *inner = thr->driver;
   video_driver_t driver = *inner;
   uint64_t saved = thr->video_st->swap_count;
   unsigned i;
#ifdef HAVE_GFX_WIDGETS
   dispgfx_widget_t *widgets;
   int saved_worker;
#endif

   replay_swap = t;
   driver.frame = replay_swap_frame;
   thr->driver = &driver;
   thr->video_st->swap_count = t->expected;
   for (i = 0; i < 2; i++)
   {
      t->current = i;
      t->seen[i] = 0;
      t->info->swap_count = i ? UINT64_MAX : 0;
      t->returned[i] = thr->video_st->current_video->frame(thr, NULL,
            VIDEO_SCALE_PACK(1, 1), 0, 0, NULL, t->info);
      t->after[i] = thr->video_st->swap_count;
   }
#ifdef HAVE_GFX_WIDGETS
   widgets = dispwidget_get_ptr();
   t->info->widgets_userdata = widgets;
   t->info->widgets_active = false;
   t->info->fps_show = true;
   t->inactive_widgets_visible = gfx_widgets_visible(t->info);
   t->retired_widgets_checked = widgets->video_st != NULL;
   t->retired_widgets_visible = false;
   if (t->retired_widgets_checked)
   {
      saved_worker = retro_atomic_load_acquire_int(&widgets->worker);
      retro_atomic_store_release_int(&widgets->worker, 0);
      t->info->widgets_active = true;
      t->retired_widgets_visible = gfx_widgets_visible(t->info);
      retro_atomic_store_release_int(&widgets->worker, saved_worker);
   }
#endif
   thr->video_st->swap_count = saved;
   thr->driver = inner;
   replay_swap = NULL;
   return 0;
}

static void lane_replay_swap_count(void)
{
   unsigned had = failures;
   video_driver_state_t *video_st = video_state_get_ptr();
   replay_swap_t t;
   unsigned i;

   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "replay swap counter lane");
   t.thr = (thread_video_t*)video_st->data;
   t.info = (video_frame_info_t*)calloc(1, sizeof(*t.info));
   CHECK(t.info != NULL, "replay frame info allocation failed");
   if (!t.info)
      return;
   t.expected = (uint64_t)UINT32_MAX + 19;
   video_thread_run_blocking(replay_swap_run, &t);
   for (i = 0; i < 2; i++)
   {
      CHECK(t.returned[i], "inline replay was refused");
      CHECK(t.seen[i] == t.expected,
            "inline replay carried a stale swap counter");
      CHECK(t.after[i] == t.expected,
            "inline replay counted an extra present");
   }
#ifdef HAVE_GFX_WIDGETS
   CHECK(!t.inactive_widgets_visible,
         "inline replay treated inactive widgets as visible");
   CHECK(!t.retired_widgets_visible,
         "inline replay treated retired widgets as visible");
   if (!t.retired_widgets_checked)
      fprintf(stderr, "[skip] retired widget visibility (no widget context)\n");
#endif
   free(t.info);
   set_threaded_via_setting(false);
   if (failures == had)
      fprintf(stderr, "[pass] replay swap counter lane\n");
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
   set_driver(thr, &reentrant_driver);
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

   set_driver(thr, reentrant_inner);
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
   set_driver(thr, &wintick_driver);

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

   set_driver(thr, wintick_inner);
   set_threaded_via_setting(false);

   if (failures == had)
      fprintf(stderr, "[pass] window-thread present lane (%u frames, "
            "%u ticks)\n", wintick_frames, wintick_alive_calls);
}

/* ------------------------------------------------------------------ */
/* Lane: async completions stay on the main thread while the window    */
/*   thread presents                                                   */
/*   A cached-frame present issued from the video thread re-enters     */
/*   video_thread_frame() there. The uploads the worker finished are   */
/*   delivered to their owners from that function too, and the owners  */
/*   are main-thread code: gfx_display's registered loads, the icon    */
/*   slots of the menu drivers. With alive() presenting from the video */
/*   thread every few frames and a load posted every frame, every      */
/*   done() still has to run on the thread that pushes frames.         */
/* ------------------------------------------------------------------ */

static video_driver_t        asyncwin_driver;
static const video_driver_t *asyncwin_inner;
static video_poke_interface_t asyncwin_poke;
static const video_poke_interface_t *asyncwin_inner_poke;
static unsigned  asyncwin_alive_calls;
static unsigned  asyncwin_uploads;
static unsigned  asyncwin_done;
static unsigned  asyncwin_done_off_main;
static uintptr_t asyncwin_main_thread;

/* A load posted the way gfx_surface posts one: a node the caller owns,
 * filled in full, handed to the video thread and back through done(). */
static bool async_post_load(video_thread_async_load_t *n, void *img,
      video_thread_async_done_t done, void *user,
      video_thread_async_release_t release)
{
   memset(n, 0, sizeof(*n));
   n->img         = img;
   n->user        = user;
   n->done        = done;
   n->release     = release;
   n->filter      = TEXTURE_FILTER_LINEAR;
   n->kind        = VIDEO_THREAD_ASYNC_LOAD;
   n->lent_idx[0] = n->lent_idx[1] = -1;
   return video_thread_async_post(n);
}

static uintptr_t asyncwin_load(void *data, void *img, bool threaded,
      enum texture_filter_type filter)
{
   (void)data; (void)img; (void)threaded; (void)filter;
   return 0x2000 + ++asyncwin_uploads;
}

static void asyncwin_unload(void *data, bool threaded, uintptr_t id)
{
   if (id > 0x2000 && id <= 0x2000 + 4096)
      return;
   if (asyncwin_inner_poke && asyncwin_inner_poke->unload_texture)
      asyncwin_inner_poke->unload_texture(data, threaded, id);
}

static void asyncwin_get_poke(void *data, const video_poke_interface_t **iface)
{
   asyncwin_inner->poke_interface(data, &asyncwin_inner_poke);
   asyncwin_poke = *asyncwin_inner_poke;
   asyncwin_poke.load_texture   = asyncwin_load;
   asyncwin_poke.unload_texture = asyncwin_unload;
   *iface = &asyncwin_poke;
}

static bool asyncwin_alive(void *data)
{
   asyncwin_alive_calls++;
   if ((asyncwin_alive_calls % 3) == 0)
      video_driver_cached_frame();
   return asyncwin_inner->alive(data);
}

static void asyncwin_done_cb(void *user, uintptr_t handle)
{
   (void)user; (void)handle;
   asyncwin_done++;
   if (sthread_get_current_thread_id() != asyncwin_main_thread)
      asyncwin_done_off_main++;
}

static void asyncwin_release_cb(void *img) { (void)img; }

static void lane_async_done_main_thread(void)
{
   unsigned had = failures;
   video_driver_state_t *video_st = video_state_get_ptr();
   thread_video_t *thr;
   static struct texture_image img;
   static video_thread_async_load_t nodes[120];
   unsigned i, posted = 0;

   asyncwin_main_thread   = sthread_get_current_thread_id();
   asyncwin_alive_calls   = 0;
   asyncwin_uploads       = 0;
   asyncwin_done          = 0;
   asyncwin_done_off_main = 0;

   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "async done main-thread lane");
   thr = (thread_video_t*)video_st->data;

   video_thread_wait_idle();
   asyncwin_inner                = thr->driver;
   asyncwin_driver               = *thr->driver;
   asyncwin_driver.alive         = asyncwin_alive;
   asyncwin_driver.poke_interface = asyncwin_get_poke;
   set_driver(thr, &asyncwin_driver);
   set_poke_from(thr, &asyncwin_driver);

   img.width = img.height = 4;
   img.pixels = (uint32_t*)&img;

   for (i = 0; i < 120; i++)
   {
      if (async_post_load(&nodes[i], &img,
               asyncwin_done_cb, NULL, asyncwin_release_cb))
         posted++;
      run_frames(1);
   }
   video_thread_wait_idle();
   run_frames(3);
   video_thread_wait_idle();

   CHECK(asyncwin_alive_calls >= 3,
         "alive() never presented a cached frame from the video thread");
   CHECK(asyncwin_done == posted, "%u of %u loads answered",
         asyncwin_done, posted);
   CHECK(asyncwin_done_off_main == 0,
         "%u done() call(s) ran off the main thread", asyncwin_done_off_main);

   set_driver(thr, asyncwin_inner);
   set_poke_from(thr, asyncwin_inner);
   set_threaded_via_setting(false);
   run_frames(2);

   if (failures == had)
      fprintf(stderr, "[pass] async done main-thread lane (%u loads, "
            "%u ticks)\n", posted, asyncwin_alive_calls);
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
   set_driver(thr, &phase_driver);
   /* The wrapper caches its poke at init; refresh it through the
    * swapped driver so the stale report is what the worker reads. */
   set_poke_from(thr, &phase_driver);

   run_frames(10);
   video_thread_wait_idle();
   { worker_truth_t wt; worker_truth(thr, &wt); rep0 = wt.repeats; }
   retro_sleep(70);
   video_thread_wait_idle();
   { worker_truth_t wt; worker_truth(thr, &wt); rep1 = wt.repeats; }
   CHECK(rep1 > rep0, "no repeats with a stale display report");
   CHECK(rep1 - rep0 <= 8,
         "%llu repeats over 70 ms: a stale display report piled them up",
         (unsigned long long)(rep1 - rep0));

   /* The stale report is still in place and it is in the past, so the
    * presenter's phase - and the latency's source with it - came from
    * the display here. Those two bits of the snapshot's flags word are
    * clear in every other lane. */
   stats_snapshot_check("display-phase lane");

   set_driver(thr, phase_inner);
   set_poke(thr, phase_inner_poke);
   settings->bools.video_threaded_present_repeat = false;
   set_threaded_via_setting(false);

   if (failures == had)
      fprintf(stderr, "[pass] display-phase lane (%llu repeats)\n",
            (unsigned long long)(rep1 - rep0));
}

/* ------------------------------------------------------------------ */
/* Lane: the menu texture handoff and the drain that protects it       */
/*   RGUI pushes a menu texture through the wrapper on every frame the */
/*   software menu is up, and the worker hands the newest one to the    */
/*   driver from thread_update_driver_state(), taken from a triple      */
/*   buffer. video_thread_frame() waits for the ring to drain whenever  */
/*   the menu texture is enabled, so the menu frame on screen is the    */
/*   one just pushed; that wait is pinned here.                         */
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

/* Check one immutable publication. RGUI also publishes here, so only
 * this lane's dimensions and pixel marker identify a checked image. */
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
   set_driver(thr, &menutex_driver);
   /* The wrapper caches its poke at init; refresh it through the
    * swapped driver so the worker hands the texture to ours. */
   set_poke_from(thr, &menutex_driver);

   retro_atomic_store_release_int(&menutex_seen,   0);
   retro_atomic_store_release_int(&menutex_torn,   0);
   retro_atomic_store_release_int(&menutex_frames, 0);
   menutex_slow = true;

   CHECK(!video_thread_get_menu_framebuffer(0, false, NULL),
         "empty menu framebuffer was lent");

   for (gen = 1; gen <= MENUTEX_PUSHES; gen++)
   {
      unsigned pending;
      bool     busy;
      uint16_t *pixels = buf;

      if (gen & 1)
      {
         pixels = (uint16_t*)video_thread_get_menu_framebuffer(
               VIDEO_SCALE_PACK(MENUTEX_W, MENUTEX_H), false, NULL);
         CHECK(pixels != NULL, "menu framebuffer loan refused");
         if (!pixels)
            pixels = buf;
      }

      for (i = 0; i < MENUTEX_W * MENUTEX_H; i++)
         pixels[i] = (uint16_t)(MENUTEX_MARK | gen);

      video_st->poke->set_texture_frame(video_st->data, pixels, false,
            VIDEO_SCALE_PACK(MENUTEX_W, MENUTEX_H), 1.0f);

      /* One frame through the real path, then the drain the menu
       * texture asks for must have happened: the worker holds no slot
       * and has none waiting, so the next push meets no render. */
      run_frames(1);

      {
         int st  = retro_atomic_load_acquire_int(&thr->frame.state);
         pending = VIDEO_THREAD_RING_PENDING_OF(st);
         busy    = VIDEO_THREAD_RING_BUSY_OF(st);
      }

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

   set_driver(thr, menutex_inner);
   set_poke(thr, menutex_inner_poke);
   set_threaded_via_setting(false);

   CHECK(!video_thread_get_menu_framebuffer(
            VIDEO_SCALE_PACK(MENUTEX_W, MENUTEX_H), false, NULL),
         "inactive wrapper lent a menu framebuffer");

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
/* Lane: two threads post commands at once                            */
/*   The main thread and another both send blocking commands, as an   */
/*   achievement badge upload and the screenshot the same unlock asks */
/*   for do. Each must get its own reply: the poster slot keeps them  */
/*   to the mailbox one at a time, and a reply that went to the other */
/*   thread, or a command run twice or not at all, shows here.        */
/* ------------------------------------------------------------------ */

#define POSTLANE_SENDS 2000

static retro_atomic_int_t postlane_runs;
static retro_atomic_int_t postlane_off_thread;
static uintptr_t          postlane_video_thread;

static uintptr_t postlane_echo(void *data)
{
   if (sthread_get_current_thread_id() != postlane_video_thread)
      retro_atomic_fetch_add_int(&postlane_off_thread, 1);
   retro_atomic_fetch_add_int(&postlane_runs, 1);
   return (uintptr_t)data;
}

static retro_atomic_int_t postlane_crossed;

static void postlane_poster(void *data)
{
   uintptr_t base = (uintptr_t)data;
   unsigned  i;
   for (i = 0; i < POSTLANE_SENDS; i++)
   {
      uintptr_t tag = base + 2 * (uintptr_t)i;
      if (video_thread_texture_handle((void*)tag, postlane_echo) != tag)
         retro_atomic_fetch_add_int(&postlane_crossed, 1);
   }
}

static void lane_concurrent_posters(void)
{
   unsigned had = failures;
   thread_video_t *thr;
   sthread_t *other;

   set_threaded_via_setting(true);
   run_frames(4);
   expect_wrapper(true, "concurrent-posters lane");
   video_thread_wait_idle();
   thr = (thread_video_t*)video_state_get_ptr()->data;
   postlane_video_thread = sthread_get_thread_id(thr->thread);
   retro_atomic_store_release_int(&postlane_runs, 0);
   retro_atomic_store_release_int(&postlane_crossed, 0);
   retro_atomic_store_release_int(&postlane_off_thread, 0);

   if (!(other = sthread_create(postlane_poster, (void*)(uintptr_t)1)))
      CHECK(false, "could not start the second poster");
   else
   {
      postlane_poster((void*)(uintptr_t)1000000);
      sthread_join(other);
   }

   CHECK(!retro_atomic_load_acquire_int(&postlane_crossed),
         "%d replies went to the wrong poster or were wrong",
         retro_atomic_load_acquire_int(&postlane_crossed));
   CHECK(retro_atomic_load_acquire_int(&postlane_runs) == 2 * POSTLANE_SENDS,
         "%d commands ran for %d sends",
         retro_atomic_load_acquire_int(&postlane_runs), 2 * POSTLANE_SENDS);
   CHECK(!retro_atomic_load_acquire_int(&postlane_off_thread),
         "%d commands ran off the video thread",
         retro_atomic_load_acquire_int(&postlane_off_thread));

   set_threaded_via_setting(false);
   run_frames(2);

   if (failures == had)
      fprintf(stderr, "[pass] concurrent-posters lane\n");
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
/* Lane: a present that waits for the vertical blank                   */
/*   An OpenGL swap with a swap interval set returns when the vblank   */
/*   has passed and the frame is out. The presenter timed the whole    */
/*   frame call as drawing and took the frame to go out on the vblank  */
/*   after the call returned: so the hold started the core a wait      */
/*   earlier every frame, and the latency read a period and more for   */
/*   a frame that went out on time. A driver that says how long it     */
/*   waited, and that the frame is out, is held to its drawing time    */
/*   and measured to the vblank it met.                                */
/* ------------------------------------------------------------------ */

#define BLK_PERIOD 16667

static video_driver_t blk_driver;
static const video_driver_t *blk_inner;
static video_poke_interface_t blk_poke;
static const video_poke_interface_t *blk_inner_poke;
static retro_time_t blk_origin;
static retro_time_t blk_vblank;
static retro_time_t blk_wait;
static retro_time_t blk_wait_sum;
static unsigned blk_frames;
static unsigned blk_missed;
static bool blk_tells;
static unsigned blk_post_delay;
static bool blk_zero_wait;

static bool blk_frame(void *data, const void *frame, unsigned dims,
      uint64_t frame_count, unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
   retro_time_t at, now, v;
   bool ret = blk_inner->frame(data, frame, dims, frame_count, pitch,
         msg, video_info);
   /* the swap: to the next vblank of a 60 Hz display */
   at  = cpu_features_get_time_usec();
   v   = blk_origin + ((at - blk_origin) / BLK_PERIOD + 1) * BLK_PERIOD;
   for (now = at; now < v; now = cpu_features_get_time_usec())
      if (v - now > 2500)
         retro_sleep(1);
   blk_wait      = now - at;
   blk_wait_sum += blk_wait;
   blk_frames++;
   if (blk_wait > BLK_PERIOD / 2)
      blk_missed++;
   blk_vblank    = v;
   if (blk_post_delay)
      retro_sleep(blk_post_delay);
   return ret;
}

static retro_time_t blk_present_time(void *data)
{
   (void)data;
   return blk_vblank;
}

static retro_time_t blk_present_wait(void *data, bool *shown)
{
   (void)data;
   *shown = blk_tells;
   return blk_tells && !blk_zero_wait ? blk_wait : 0;
}

static void blk_get_poke(void *data, const video_poke_interface_t **iface)
{
   blk_inner->poke_interface(data, &blk_inner_poke);
   blk_poke = *blk_inner_poke;
   blk_poke.get_last_present_time = blk_present_time;
   blk_poke.get_last_present_wait = blk_present_wait;
   *iface = &blk_poke;
}

static void lane_blocking_present(void)
{
   unsigned had = failures;
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   bool  saved_pacing = settings->bools.video_threaded_display_pacing;
   float saved_hz     = settings->floats.video_refresh_rate;
   bool  menu_was_up;
   thread_video_t *thr;
   retro_time_t lat_untold, lat_told, render_untold, render_told;
   retro_time_t off_untold;
   unsigned i, met = 0;
   unsigned missed_first, missed_after;

   settings->bools.video_threaded_display_pacing = true;
   settings->floats.video_refresh_rate           = 60.0f;
   set_threaded_via_setting(true);
   run_frames(10);
   expect_wrapper(true, "blocking-present lane");
   thr = (thread_video_t*)video_st->data;
   menu_was_up = menu_is_up();
   if (menu_was_up)
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_frames(3);
   video_thread_wait_idle();

   blk_origin = cpu_features_get_time_usec();
   blk_vblank = blk_origin;
   blk_inner  = thr->driver;
   blk_driver = *thr->driver;
   blk_driver.frame          = blk_frame;
   blk_driver.poke_interface = blk_get_poke;
   set_driver(thr, &blk_driver);
   set_poke_from(thr, &blk_driver);

   /* as a driver that says nothing of its wait */
   blk_tells = false;
   run_frames(120);
   video_thread_wait_idle();
   lat_untold    = thr->latency_avg;
   render_untold = thr->render_time;
   off_untold    = thr->last_present_end - blk_vblank;

   /* and one that does. Told the waits, the presenter learns what
    * margin this machine wants (video_thread_margin_learn()): a frame
    * that misses its vblank adds to it. Time to learn, then a count. */
   blk_tells    = true;
   blk_missed   = 0;
   blk_frames   = 0;
   run_frames(60);
   video_thread_wait_idle();
   missed_first = blk_missed;
   run_frames(240);
   video_thread_wait_idle();
   blk_wait_sum = 0;
   blk_frames   = 0;
   blk_missed   = 0;
   run_frames(120);
   video_thread_wait_idle();
   missed_after = blk_missed;
   lat_told    = thr->latency_avg;
   render_told = thr->render_time;
   fprintf(stderr, "   blocking present, told: %u of the first 60 frames missed their vblank;"
         " after learning, %u of 120, the swap waiting %.2f ms on average,"
         " with %.2f ms added to the margin\n",
         missed_first, missed_after,
         blk_frames ? blk_wait_sum / 1000.0 / blk_frames : 0.0,
         retro_atomic_load_acquire_int(&thr->margin_extra_us) / 1000.0);
   /* a machine that keeps missing has learned nothing */
   CHECK(missed_after <= 30,
         "blocking present: %u frames of 120 still miss their vblank after"
         " the margin was learned", missed_after);

   /* What is checked is the accounting, frame by frame, not where the
    * frames landed. The hold aims a frame half a millisecond ahead of
    * its vblank; on a loaded machine some arrive after it and wait for
    * the next, a whole period, and an average over those says how the
    * machine was scheduled. Whichever vblank a frame met:
    * - it is counted to that vblank, the one the swap came back on,
    *   not to the one after;
    * - the wait for it is not drawing time, however long it was. */
   for (i = 0; i < 8; i++)
   {
      retro_time_t off;
      run_frames(1);
      video_thread_wait_idle();
      off = thr->last_present_end - blk_vblank;
      if (off > -BLK_PERIOD / 2 && off < BLK_PERIOD / 2)
         met++;
      if (thr->render_time < render_told)
         render_told = thr->render_time;
   }
   CHECK(met >= 6,
         "blocking present: %u frames of 8 were counted to the vblank their"
         " swap came back on", met);
   {
      retro_time_t shown_wait = 0;
      CHECK(video_thread_present_wait_stats(&shown_wait) && shown_wait > 0,
            "blocking present: the wait is not in the statistics");
   }
   CHECK(render_told < 4000,
         "blocking present: the wait for the vblank is still counted as drawing"
         " (render time %.2f ms)", render_told / 1000.0);
   fprintf(stderr, "   blocking present at 60 Hz: untold, latency %.2f ms,"
         " render %.2f ms, counted %.2f ms past the swap's vblank; told,"
         " latency %.2f ms, render %.2f ms, %u of 8 counted to it\n",
         lat_untold / 1000.0, render_untold / 1000.0, off_untold / 1000.0,
         lat_told / 1000.0, render_told / 1000.0, met);

   blk_post_delay = 5;
   run_frames(1);
   video_thread_wait_idle();
   CHECK(thr->last_present_end == blk_vblank,
         "completed present must exclude the next-buffer wait");
   blk_zero_wait = true;
   run_frames(1);
   video_thread_wait_idle();
   CHECK(thr->last_present_end == blk_vblank,
         "completed present feedback must work without a measured wait");
   blk_post_delay = 0;
   blk_zero_wait = false;

   video_thread_wait_idle();
   set_driver(thr, blk_inner);
   set_poke(thr, blk_inner_poke);
   if (menu_was_up && !menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_frames(3);
   settings->floats.video_refresh_rate           = saved_hz;
   settings->bools.video_threaded_display_pacing = saved_pacing;
   set_threaded_via_setting(false);

   if (failures == had)
      fprintf(stderr, "[pass] blocking-present lane\n");
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
    * a 120 Hz panel. At Menu Frame Rate 'Content Rate' the menu goes at
    * the core's pace; at 'Display Rate' the menu's frames go out at
    * the display's rate and the core keeps its own clock behind them,
    * so it runs on half of them at 120 Hz. */
   {
      bool saved_pause      = settings->bools.menu_pause_libretro;
      unsigned saved_rate   = settings->uints.menu_frame_rate;
      dylib_t lib           = runloop_state_get_ptr()->lib_handle;
      unsigned (*core_runs)(void) = lib
         ? (unsigned (*)(void))dylib_proc(lib, "harness_core_runs") : NULL;
      unsigned before;

      settings->bools.menu_pause_libretro = false;

      settings->uints.menu_frame_rate = MENU_FRAME_RATE_CONTENT;
      display_pacing_measure(120.0f, 60, 60.0f,
            "menu at content rate, core running");

      settings->uints.menu_frame_rate = MENU_FRAME_RATE_DISPLAY;
      CHECK(core_runs != NULL, "display-pacing lane: harness core has no run count");
      before = core_runs ? core_runs() : 0;
      display_pacing_measure(120.0f, 120, 120.0f,
            "menu at display rate, core running");
      if (core_runs)
      {
         /* 120 menu frames at 120 Hz is a second, with the six settle
          * frames before it: the core's 60 a second, give or take the
          * frames either side of the window */
         unsigned ran = core_runs() - before;
         CHECK(ran >= 50 && ran <= 75,
               "menu at display rate: the core ran %u times in 126 menu "
               "frames at 120 Hz, not ~63", ran);
      }

      /* The menu over a paused core: at 'Content Rate' the hold is
       * the one clock holding it to the content's period - the frame
       * limiter's timer stands aside for it, as two clocks at nearly
       * the same period beat - and at 'Display Rate' it runs at the
       * display's. */
      settings->bools.menu_pause_libretro = true;
      settings->uints.menu_frame_rate     = MENU_FRAME_RATE_CONTENT;
      display_pacing_measure(120.0f, 60, 60.0f,
            "menu at content rate, core paused");
      settings->uints.menu_frame_rate     = MENU_FRAME_RATE_DISPLAY;
      display_pacing_measure(120.0f, 120, 120.0f,
            "menu at display rate, core paused");

      settings->uints.menu_frame_rate     = saved_rate;
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
   unsigned hits0, misses0, presents0, hits, misses, presents;
   uint64_t frames0, frames;

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
   set_driver(thr, &vslane_driver);
   set_poke(thr, &vslane_poke);
   hits0     = thr->hit_count;
   misses0   = thr->miss_count;
   presents0 = vslane_presents;
   frames0   = core_frames();

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
      { bool p; retro_time_t c; video_thread_pacing_stats(&p, &c, &render); }
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
   { bool p; retro_time_t c; video_thread_pacing_stats(&p, &c, &render); }
   CHECK(settled > latched,
         "pacing-drain lane: latched behind the queued frame: %u of %u "
         "frames at %.1f ms latency, render reserve %.1f ms",
         latched, latched + settled, avg / 1000.0, render / 1000.0);
   /* Every frame reached the display, but for the ones the wrapper
    * replaced in the ring because the display was late - with vsync
    * off that is its job, and a busy machine makes the display late
    * on its own schedule.  Counted, not timed: each push either took
    * a free slot (a hit, drawn) or replaced an undrawn frame (a miss,
    * one frame lost). */
   hits     = thr->hit_count  - hits0;
   misses   = thr->miss_count - misses0;
   presents = vslane_presents - presents0;
   frames   = core_frames()   - frames0;
   CHECK(hits + misses == frames,
         "pacing-drain lane: %u of %llu frames reached the wrapper",
         hits + misses, (unsigned long long)frames);
   CHECK(presents == hits,
         "pacing-drain lane: the vsync driver saw %u presents of %u frames "
         "the wrapper kept (%u replaced)", presents, hits, misses);
   /* The statistics overlay's Pushed line reads the same count. */
   {
      video_thread_handoff_stats_t ho;
      CHECK(video_thread_get_handoff_stats(&ho) && ho.pushed == thr->hit_count,
            "pacing-drain lane: handoff stats report %u pushed, wrapper %u",
            ho.pushed, thr->hit_count);
   }
   CHECK(render < vslane_period / 2,
         "pacing-drain lane: render reserve grew to %.1f ms of a %.1f ms period",
         render / 1000.0, vslane_period / 1000.0);
   /* The input's age: how old the input a presented frame was made
    * from is, poll to present. Polls are stamped only while the
    * statistics are shown, so there is no figure before that, there
    * is one while they are, and it goes again after. It starts at the
    * poll, which is before the handover the latency starts at, and the
    * worst is no better than the average. */
   {
      retro_time_t in_avg = 0, in_max = 0;
      bool before, during, after;

      before = video_thread_input_age_stats(&in_avg, &in_max);
      settings->bools.video_statistics_show = true;
      run_frames(40);
      video_thread_wait_idle();
      during = video_thread_input_age_stats(&in_avg, &in_max);
      video_thread_latency_stats(&avg, &worst, &from_display);
      CHECK(!before, "input-age lane: a figure with the statistics off");
      CHECK(during && in_avg > 0 && in_avg < 1000000 && in_max >= in_avg,
            "input-age lane: with the statistics on: %s, %.2f ms, worst %.2f ms",
            during ? "a figure" : "no figure", in_avg / 1000.0, in_max / 1000.0);
      CHECK(!during || in_avg + avg / 4 >= avg,
            "input-age lane: %.2f ms from the poll, yet %.2f ms from the handover",
            in_avg / 1000.0, avg / 1000.0);
      fprintf(stderr, "   input age: %.2f ms poll to present (worst %.2f), latency %.2f ms\n",
            in_avg / 1000.0, in_max / 1000.0, avg / 1000.0);
      settings->bools.video_statistics_show = false;
      run_frames(4);
      video_thread_wait_idle();
      after = video_thread_input_age_stats(&in_avg, &in_max);
      CHECK(!after, "input-age lane: a figure left after the statistics went off");
   }
   fprintf(stderr, "   pacing drain: latency %.1f ms, render reserve %.1f ms, %u/%u settled, %u presents, %u replaced\n",
         avg / 1000.0, render / 1000.0, settled, settled + latched, presents, misses);

   set_driver(thr, vslane_inner);
   set_poke(thr, vslane_inner_poke);
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
/* Lane: a frame due every other vblank is not a queued one            */
/*   A presenter on a 120 Hz vblank grid with the core at 60 fps, whose */
/*   render alternates 2 and 7 ms, as a hardware core's does when the   */
/*   video thread waits out the frame's GPU work before presenting.     */
/*   A frame is due every other vblank; from its push to the vblank it  */
/*   goes out on is a render plus up to a vblank, which against the     */
/*   display's period and a half reads as queued behind another, and    */
/*   the hold then skips a content period to drain a queue that is not  */
/*   there. Against the content's period nothing is drained, and the    */
/*   core keeps its 60 frames a second.                                 */
/* ------------------------------------------------------------------ */

static video_driver_t                rvlane_driver;
static const video_driver_t         *rvlane_inner;
static video_poke_interface_t        rvlane_poke;
static const video_poke_interface_t *rvlane_inner_poke;
static retro_time_t                  rvlane_period;
static retro_time_t                  rvlane_base;
static retro_time_t                  rvlane_last_out;
static unsigned                      rvlane_presents;
/* The vblank each new content frame went out on, while logging */
#define RVLANE_LOG_MAX 128
static retro_time_t                  rvlane_log[RVLANE_LOG_MAX];
static uint64_t                      rvlane_log_frame[RVLANE_LOG_MAX];
static unsigned                      rvlane_log_n;
static bool                          rvlane_logging;
static uint64_t                      rvlane_log_count;

static retro_time_t rvlane_grid_after(retro_time_t t)
{
   retro_time_t k = (t - rvlane_base) / rvlane_period + 1;
   return rvlane_base + k * rvlane_period;
}

static bool rvlane_frame(void *data, const void *frame,
      unsigned dims, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   retro_time_t in   = cpu_features_get_time_usec();
   /* The render: short, then long, as the GPU frame varies. Slept to
    * a millisecond short and spun the rest, so a sleep's granularity
    * is not the variance measured. */
   retro_time_t took = (rvlane_presents & 1) ? 7000 : 2000;
   retro_time_t now  = in;
   retro_time_t slot;
   if (took > 1500)
      retro_sleep((unsigned)((took - 1000) / 1000));
   now = cpu_features_get_time_usec();
   while (now < in + took)
      now = cpu_features_get_time_usec();
   /* Out on the first vblank after the render, as a vsynced present */
   slot = rvlane_grid_after(now);
   if (slot <= rvlane_last_out)
      slot = rvlane_last_out + rvlane_period;
   rvlane_last_out = slot;
   rvlane_presents++;
   if (     rvlane_logging && frame_count != rvlane_log_count
         && rvlane_log_n < RVLANE_LOG_MAX)
   {
      rvlane_log_frame[rvlane_log_n] = frame_count;
      rvlane_log[rvlane_log_n++]     = slot;
      rvlane_log_count               = frame_count;
   }
   return rvlane_inner->frame(data, frame, dims, frame_count,
         pitch, msg, video_info);
}

static retro_time_t rvlane_last_present(void *data)
{
   retro_time_t now = cpu_features_get_time_usec();
   (void)data;
   return rvlane_last_out <= now ? rvlane_last_out
      : rvlane_last_out - rvlane_period;
}

static void lane_pacing_fast_display(void)
{
   unsigned had = failures;
   settings_t *settings = config_get_ptr();
   thread_video_t *thr;
   bool  saved_pacing  = settings->bools.video_threaded_display_pacing;
   bool  saved_ask     = settings->bools.video_present_timing_from_display;
   float saved_refresh = settings->floats.video_refresh_rate;
   unsigned drains_before, drains;
   uint64_t frames_before;
   retro_time_t t0, took;
   double fps;
   unsigned n, intervals = 0, on_cadence = 0;

   /* Display at twice the core's rate: a content frame every other vblank */
   settings->floats.video_refresh_rate               = 120.0f;
   settings->bools.video_present_timing_from_display = true;
   settings->bools.video_threaded_display_pacing     = true;
   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "fast-display lane");
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   CHECK(!menu_is_up(), "fast-display lane: menu still up");
   run_frames(3);
   video_thread_wait_idle();

   thr                 = (thread_video_t*)video_state_get_ptr()->data;
   rvlane_period       = 1000000 / 120;
   rvlane_base         = cpu_features_get_time_usec();
   rvlane_last_out     = rvlane_base;
   rvlane_presents     = 0;
   rvlane_inner        = thr->driver;
   rvlane_driver       = *thr->driver;
   rvlane_driver.frame = rvlane_frame;
   rvlane_inner_poke   = thr->poke;
   rvlane_poke         = *thr->poke;
   rvlane_poke.get_last_present_time = rvlane_last_present;
   set_driver(thr, &rvlane_driver);
   set_poke(thr, &rvlane_poke);

   /* Let the hold settle on the measured times, then count. */
   run_frames(60);
   video_thread_wait_idle();
   drains_before = thr->handoff.drains;   /* the worker is idle */
   frames_before = core_frames();
   rvlane_log_n     = 0;
   rvlane_log_count = 0;
   rvlane_logging   = true;
   t0 = cpu_features_get_time_usec();
   run_frames(120);
   took = cpu_features_get_time_usec() - t0;
   video_thread_wait_idle();
   rvlane_logging   = false;
   /* Content frames are due every other vblank.  The render here
    * alternates short and long, and a long one misses its vblank and
    * goes out on the next: one interval of three, the next of one.
    * Two intervals together always make four. */
   for (n = 2; n < rvlane_log_n; n++)
   {
      retro_time_t gap;
      if (rvlane_log_frame[n] != rvlane_log_frame[n - 2] + 2)
         continue;
      gap = rvlane_log[n] - rvlane_log[n - 2];
      intervals++;
      if ((gap + rvlane_period / 2) / rvlane_period == 4)
         on_cadence++;
   }
   drains = thr->handoff.drains - drains_before;   /* idle again */
   fps = took > 0 ? (double)(core_frames() - frames_before) * 1000000.0 / (double)took : 0.0;

   /* Counted, not timed.  The render and its wait for the vblank
    * mistaken for a queue drain every fifth frame (the cooldown's
    * ceiling), 24 of 120; a machine that stalls both threads makes a
    * frame late for real and drains once per stall. */
   CHECK(drains <= 6,
         "fast-display lane: %u drains over 120 frames due every other vblank "
         "(%.1f fps, render reserve %.1f ms)",
         drains, fps, thr->render_time / 1000.0);
   /* A hold too long spaces frames wider, one that releases early
    * closer; either moves every pair, where a stall moves a few. */
   CHECK(intervals >= 60 && on_cadence * 4 >= intervals * 3,
         "fast-display lane: %u of %u frames went out four vblanks after the "
         "one two before (%.1f fps)", on_cadence, intervals, fps);
   fprintf(stderr, "   fast display: %u drains of 120, %u/%u on cadence, %.1f fps, render reserve %.1f ms, %u presents\n",
         drains, on_cadence, intervals, fps, thr->render_time / 1000.0, rvlane_presents);

   set_driver(thr, rvlane_inner);
   set_poke(thr, rvlane_inner_poke);
   settings->bools.video_threaded_display_pacing     = saved_pacing;
   settings->bools.video_present_timing_from_display = saved_ask;
   settings->floats.video_refresh_rate               = saved_refresh;
   if (!menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   set_threaded_via_setting(false);

   if (failures == had)
      fprintf(stderr, "[pass] fast-display lane\n");
}

/* ------------------------------------------------------------------ */
/* Lane: no catch-up after a stall                                     */
/* ------------------------------------------------------------------ */

/* The hold measures from the vblank the video thread expected when it
 * last presented.  After the loop has stalled that vblank has passed,
 * and a hold measured from it released every frame at once: the core
 * ran at the display's rate, a 60 fps game at 120 on this display,
 * until the presenter caught up.  Counted on the display's grid, not
 * timed: content frames are due two vblanks apart, so the frames from
 * the first shown to the last must span twice as many vblanks as the
 * frame counter advanced.  A frame the wrapper replaced (vsync off)
 * still counts: it was due all the same.  Ten vblanks are allowed
 * for the restart: the first frame is pushed before any hold has run,
 * and the restart costs about four here.  Lateness after that only
 * widens the span.  The catch-up fell short by thirty to sixty. */
static void lane_pacing_after_stall(void)
{
   unsigned had = failures;
   settings_t *settings = config_get_ptr();
   thread_video_t *thr;
   bool  saved_pacing  = settings->bools.video_threaded_display_pacing;
   bool  saved_ask     = settings->bools.video_present_timing_from_display;
   float saved_refresh = settings->floats.video_refresh_rate;
   unsigned frames     = 60;
   retro_time_t span;
   unsigned vblanks, want, advanced;

   settings->floats.video_refresh_rate               = 120.0f;
   settings->bools.video_present_timing_from_display = true;
   settings->bools.video_threaded_display_pacing     = true;
   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "after-stall lane");
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   CHECK(!menu_is_up(), "after-stall lane: menu still up");
   run_frames(3);
   video_thread_wait_idle();

   thr                 = (thread_video_t*)video_state_get_ptr()->data;
   rvlane_period       = 1000000 / 120;
   rvlane_base         = cpu_features_get_time_usec();
   rvlane_last_out     = rvlane_base;
   rvlane_presents     = 0;
   rvlane_inner        = thr->driver;
   rvlane_driver       = *thr->driver;
   rvlane_driver.frame = rvlane_frame;
   rvlane_inner_poke   = thr->poke;
   rvlane_poke         = *thr->poke;
   rvlane_poke.get_last_present_time = rvlane_last_present;
   set_driver(thr, &rvlane_driver);
   set_poke(thr, &rvlane_poke);

   run_frames(60);
   video_thread_wait_idle();

   /* The loop stalls, as a descheduled process does, then resumes. */
   retro_sleep(300);
   rvlane_log_n     = 0;
   rvlane_log_count = 0;
   rvlane_logging   = true;
   run_frames(frames);
   video_thread_wait_idle();
   rvlane_logging   = false;

   CHECK(rvlane_log_n >= frames / 2, "after-stall lane: %u of %u frames presented",
         rvlane_log_n, frames);
   if (rvlane_log_n > 1)
   {
      advanced = (unsigned)(rvlane_log_frame[rvlane_log_n - 1] - rvlane_log_frame[0]);
      span     = rvlane_log[rvlane_log_n - 1] - rvlane_log[0];
      vblanks  = (unsigned)((span + rvlane_period / 2) / rvlane_period);
      want     = 2 * advanced;
      CHECK(vblanks + 10 >= want,
            "after-stall lane: %u frames went out over %u vblanks, not %u: "
            "the core caught up at the display's rate",
            advanced, vblanks, want);
      fprintf(stderr, "   after stall: %u frames over %u vblanks (%u due), %u shown\n",
            advanced, vblanks, want, rvlane_log_n);
   }

   set_driver(thr, rvlane_inner);
   set_poke(thr, rvlane_inner_poke);
   settings->bools.video_threaded_display_pacing     = saved_pacing;
   settings->bools.video_present_timing_from_display = saved_ask;
   settings->floats.video_refresh_rate               = saved_refresh;
   if (!menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   set_threaded_via_setting(false);

   if (failures == had)
      fprintf(stderr, "[pass] after-stall lane\n");
}


/* ------------------------------------------------------------------ */
/* Lane: the paced loop keeps the content's rate                       */
/*   Frames counted over two seconds under display pacing, for every   */
/*   pairing of content and display rate that ships, with and without  */
/*   the driver reporting when its presents reach the display (vsync   */
/*   off reports nothing, and the hold lays its own grid from the      */
/*   clock).  The render takes 0.6-2.1 ms, varying frame to frame.     */
/*   The loop must hold within 2% of the rate it is due at: a hold     */
/*   that picks a vblank one past the frame's due time sleeps out a    */
/*   whole period, and a 120 fps core on a 120 Hz display then runs    */
/*   at 103.  The core takes 1.3 ms a frame: the hold's cap counts     */
/*   from the push, so a core that takes nothing hides a hold that     */
/*   sleeps out a whole period.  Rate is the count over the window,    */
/*   not per-frame timing, and each case gets three tries: a busy      */
/*   runner costs a try, a hold that misses its slot fails all three.  */
/* ------------------------------------------------------------------ */

static video_driver_t                prlane_driver;
static const video_driver_t         *prlane_inner;
static video_poke_interface_t        prlane_poke;
static const video_poke_interface_t *prlane_inner_poke;
static retro_time_t                  prlane_period;   /* the panel's real period */
static retro_time_t                  prlane_base;
static retro_time_t                  prlane_last_out;
static unsigned                      prlane_seed;

static bool prlane_frame(void *data, const void *frame,
      unsigned dims, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   retro_time_t now  = cpu_features_get_time_usec();
   retro_time_t done;
   prlane_seed = prlane_seed * 1103515245u + 12345u;
   /* 0.6 ms, plus up to 1.5 ms more on some frames */
   done = now + 600 + ((prlane_seed >> 16) % 4 == 0
         ? (retro_time_t)((prlane_seed >> 8) % 1500) : 0);
   while (cpu_features_get_time_usec() < done) { }
   /* Vsync off: out on the next scanout, the present does not wait */
   now = cpu_features_get_time_usec();
   prlane_last_out = prlane_base
      + ((now - prlane_base) / prlane_period + 1) * prlane_period;
   return prlane_inner->frame(data, frame, dims, frame_count,
         pitch, msg, video_info);
}

static retro_time_t prlane_last_present(void *data)
{
   retro_time_t now = cpu_features_get_time_usec();
   (void)data;
   return prlane_last_out <= now ? prlane_last_out
      : prlane_last_out - prlane_period;
}

static double pacing_rate_measure(thread_video_t *thr, float display_hz,
      double panel_hz, double content_fps, bool timestamps)
{
   settings_t *settings = config_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   unsigned n = (unsigned)(content_fps * 2.0);
   uint64_t f0;
   retro_time_t t0, took;

   /* The fake driver reads the panel's timing on the video thread:
    * changed only with that thread idle, as the frame handed over next
    * orders it after these writes. */
   video_thread_wait_idle();
   settings->floats.video_refresh_rate               = display_hz;
   settings->bools.video_present_timing_from_display = timestamps;
   video_st->av_info.timing.fps                      = content_fps;
   prlane_period     = (retro_time_t)(1000000.0 / panel_hz);
   prlane_base       = cpu_features_get_time_usec();
   prlane_last_out   = prlane_base;
   prlane_poke.get_last_present_time = timestamps ? prlane_last_present : NULL;
   set_poke(thr, &prlane_poke);
   /* Settle: the period, render and core estimates */
   run_frames(n / 4);
   video_thread_wait_idle();
   f0 = core_frames();
   t0 = cpu_features_get_time_usec();
   run_frames(n);
   took = cpu_features_get_time_usec() - t0;
   return took > 0
      ? (double)(core_frames() - f0) * 1000000.0 / (double)took : 0.0;
}

static void lane_pacing_rate(void)
{
   static const struct
   {
      float  display_hz;  /* what the driver reports */
      double panel_hz;    /* what the panel runs at */
      double content_fps;
      double expect;
   } cases[] = {
      { 120.0f, 120.0,   120.0, 120.0 },
      { 120.0f, 119.982, 120.0, 120.0 },   /* a real panel, a hair slow */
      { 120.0f, 120.018, 120.0, 120.0 },   /* and a hair fast */
      { 120.0f, 120.0,    60.0,  60.0 },
      {  60.0f,  60.0,    60.0,  60.0 },
      {  60.0f,  59.94,   60.0,  60.0 },
      { 144.0f, 144.0,    60.0,  60.0 },
      { 144.0f, 144.0,   120.0, 120.0 },
   };
   unsigned had = failures;
   settings_t *settings = config_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   thread_video_t *thr;
   bool   saved_pacing  = settings->bools.video_threaded_display_pacing;
   bool   saved_ask     = settings->bools.video_present_timing_from_display;
   float  saved_refresh = settings->floats.video_refresh_rate;
   double saved_fps     = video_st->av_info.timing.fps;
   dylib_t lib = runloop_state_get_ptr()->lib_handle;
   void (*run_us)(unsigned) = lib
      ? (void (*)(unsigned))dylib_proc(lib, "harness_core_set_run_us") : NULL;
   unsigned c, ts;

   CHECK(run_us != NULL, "pacing-rate lane: harness core has no run time");
   if (!run_us)
      return;
   settings->bools.video_threaded_display_pacing = true;
   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "pacing-rate lane");
   /* The core takes 1.3 ms, as prboom does at 2560x1600: the loop has
    * that to spend inside every period on top of the hold. */
   run_us(1300);
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   CHECK(!menu_is_up(), "pacing-rate lane: menu still up");
   run_frames(3);
   video_thread_wait_idle();

   thr                 = (thread_video_t*)video_state_get_ptr()->data;
   prlane_seed         = 1;
   prlane_inner        = thr->driver;
   prlane_driver       = *thr->driver;
   prlane_driver.frame = prlane_frame;
   prlane_inner_poke   = thr->poke;
   prlane_poke         = *thr->poke;
   set_driver(thr, &prlane_driver);

   for (c = 0; c < ARRAY_SIZE(cases); c++)
      for (ts = 0; ts < 2; ts++)
      {
         double best = 0.0, fps;
         unsigned t;
         for (t = 0; t < 3; t++)
         {
            fps = pacing_rate_measure(thr, cases[c].display_hz,
                  cases[c].panel_hz, cases[c].content_fps, ts != 0);
            if (fabs(fps - cases[c].expect) < fabs(best - cases[c].expect))
               best = fps;
            if (fabs(best - cases[c].expect) <= cases[c].expect * 0.02)
               break;
         }
         CHECK(fabs(best - cases[c].expect) <= cases[c].expect * 0.02,
               "pacing-rate lane: %.0f fps content on a %.3f Hz panel "
               "(reported %.0f Hz), %s: ran at %.2f fps, not %.0f",
               cases[c].content_fps, cases[c].panel_hz,
               cases[c].display_hz,
               ts ? "present timestamps" : "no timestamps (vsync off)",
               best, cases[c].expect);
         fprintf(stderr, "   pacing rate: %5.1f fps on %8.3f Hz, %-13s %.2f fps\n",
               cases[c].content_fps, cases[c].panel_hz,
               ts ? "timestamps:" : "clock grid:", best);
      }

   run_us(0);
   set_driver(thr, prlane_inner);
   set_poke(thr, prlane_inner_poke);
   video_st->av_info.timing.fps                      = saved_fps;
   settings->bools.video_threaded_display_pacing     = saved_pacing;
   settings->bools.video_present_timing_from_display = saved_ask;
   settings->floats.video_refresh_rate               = saved_refresh;
   if (!menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   set_threaded_via_setting(false);

   if (failures == had)
      fprintf(stderr, "[pass] pacing-rate lane\n");
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
   set_driver(thr, &winlane_driver);

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

   set_driver(thr, inner);
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
   set_driver(thr, &sslane_driver);
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
   set_driver(thr, inner);
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
   set_driver(thr, &rslane_driver);

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
   set_driver(thr, rslane_inner);
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
   set_driver(thr, &duplane_driver);
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
   set_driver(thr, duplane_inner);

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
/* Fault injection: the first realloc of nothing to exactly this many
 * bytes fails, once. 0 when unarmed. */
static size_t        heap_fail_realloc_new;

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
   if (!p && heap_fail_realloc_new && n == heap_fail_realloc_new)
   {
      heap_fail_realloc_new = 0;
      return NULL;
   }
   return __libc_realloc(p, n);
}

/* The GL drivers keep the names of their half-float textures in a set,
 * and draw the rest as SDR: a texture the set cannot hold must not be
 * handed out to be drawn wrong. Right after a video reinit the set is
 * empty, so the half-float load's first growth of it is made to fail:
 * the load must come back refused. */
static void lane_gl_fp16_set_full(void)
{
   static uint16_t px[4 * 4 * 4];
   struct texture_image img;
   uintptr_t tex = 1;
   unsigned had  = failures;

   if (!video_driver_supports_texture_format(TEXTURE_GPU_FORMAT_RGBA16F))
   {
      fprintf(stderr, "[skip] fp16 set lane (no half-float textures)\n");
      return;
   }
   set_threaded_via_setting(true);
   run_frames(2);
   set_threaded_via_setting(false);
   run_frames(2);
   memset(&img, 0, sizeof(img));
   img.width  = 4;
   img.height = 4;
   img.pixels = (uint32_t*)px;
   img.fp16   = true;
   heap_fail_realloc_new = 32 * sizeof(unsigned);
   if (!video_driver_texture_load(&img, TEXTURE_FILTER_LINEAR, &tex))
      tex = 0;
   CHECK(heap_fail_realloc_new == 0,
         "fp16 set lane: the load never grew the set");
   heap_fail_realloc_new = 0;
   CHECK(tex == 0, "fp16 set lane: a half-float texture the set could "
         "not hold was handed out (%lx)", (unsigned long)tex);
   if (tex)
      video_driver_texture_unload(&tex);
   /* and with room, it loads */
   tex = 0;
   video_driver_texture_load(&img, TEXTURE_FILTER_LINEAR, &tex);
   CHECK(tex != 0, "fp16 set lane: no half-float texture with room");
   if (tex)
      video_driver_texture_unload(&tex);
   run_frames(2);
   if (failures == had)
      fprintf(stderr, "[pass] fp16 set lane (refused when the set cannot "
            "grow)\n");
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

static void lane_gl_fp16_set_full(void)
{
   fprintf(stderr, "[skip] fp16 set lane (needs glibc, no allocator sanitizer)\n");
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
   set_driver(thr, &wclane_driver);
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
   set_driver(thr, wclane_inner);
   run_frames(2);
   printf("   waiter-call lane: call ran on the waiting thread, before the reply\n");
}

/* Shared by the two zero-copy lanes: mode 1 pushes the loan's start,
 * mode 2 a cropped window inside it (an offset pointer, the loan's
 * pitch). Both must be published from the lent slot, no copy. */
static void lane_zero_copy_mode(int mode, const char *name)
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
   expect_wrapper(true, name);
   use_fb(mode);

   /* Most asks granted, and those frames published from the lent slot.
    * Dupes (every third frame) ask and then push NULL, so the loan
    * lapses; oversize frames are declined. */
   run_frames(2);
   thr = (thread_video_t*)video_state_get_ptr()->data;
   g0  = granted();
   zc0 = (unsigned)thr->frame.zero_copy_count;
   run_frames(60);
   video_thread_wait_idle();
   g1  = granted();
   zc1 = (unsigned)thr->frame.zero_copy_count;
   CHECK(g1 - g0 >= 30, "%s: zero-copy on but only %u of 60 asks granted", name, g1 - g0);
   CHECK(zc1 - zc0 >= 20, "%s: only %u frames published zero-copy for %u grants", name, zc1 - zc0, g1 - g0);
   CHECK(zc1 - zc0 <= g1 - g0, "%s: %u zero-copy frames for %u grants", name, zc1 - zc0, g1 - g0);

   use_fb(0);
   if (!menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   set_threaded_via_setting(false);

   if (failures == had)
      fprintf(stderr, "[pass] %s (%u grants, %u zero-copy frames)\n",
            name, g1 - g0, zc1 - zc0);
}

static void lane_zero_copy(void)
{
   lane_zero_copy_mode(1, "zero-copy lane");
   /* The offset push used to lapse the loan (the wrapper matched the
    * pointer to the slot's start) and copy every frame. */
   lane_zero_copy_mode(2, "zero-copy lane, cropped window");
}

/* ------------------------------------------------------------------ */
/* Lane: a window into the driver's own loan, then a paused redraw      */
/*   With threaded video off the driver lends its own texture through   */
/*   GET_CURRENT_SOFTWARE_FRAMEBUFFER, and a core that crops overscan   */
/*   by offset pushes a pointer partway into that loan (beetle-psx      */
/*   does). The Vulkan driver used to see only a frame of another size  */
/*   than its texture: it recreated the texture every frame, parked the */
/*   loan on its deferred list and copied out of it - and the cached    */
/*   frame kept naming the parked mapping. Open the menu, so the core   */
/*   stops and every iteration redraws the cached frame, and after the  */
/*   deferral window the redraw read freed memory (RetroArch #19652).   */
/*   The run must survive the paused redraws, and with the tree built   */
/*   GFX_INSTRUMENT=1 the window frames must be read out of the loan    */
/*   itself: no host copy, no texture created per frame.               */
/* ------------------------------------------------------------------ */

/* The Vulkan swapchain image cap, without the driver's header. */
#define LENT_LANE_MAX_IMAGES 16

/* The GL drivers keep frame memory, and lend it as the framebuffer,
 * where buffers can be mapped for good: GL 4.4 or ARB_buffer_storage,
 * GLES 3 with EXT_buffer_storage, both of which llvmpipe has. There a
 * lane that finds none fails rather than skips, so a GL that stops
 * offering it - or a build that compiles it out - shows. Not the
 * macOS GL, which has neither. */
static bool driver_must_have_frame_mem(const char *drv)
{
#if defined(HAVE_GL_FRAME_MEM) && !defined(__APPLE__)
   return drv && (!strcmp(drv, "gl") || !strcmp(drv, "glcore"));
#else
   (void)drv;
   return false;
#endif
}

static void lane_lent_window_paused(void)
{
   unsigned had = failures;
   dylib_t lib = runloop_state_get_ptr()->lib_handle;
   void (*use_fb)(int) = lib ? (void (*)(int))dylib_proc(lib, "harness_core_use_framebuffer") : NULL;
   unsigned (*granted)(void) = lib ? (unsigned (*)(void))dylib_proc(lib, "harness_core_fb_granted") : NULL;
   unsigned (*uncached)(void) = lib ? (unsigned (*)(void))dylib_proc(lib, "harness_core_fb_uncached") : NULL;
   void (*fb_access)(unsigned) = lib ? (void (*)(unsigned))dylib_proc(lib, "harness_core_fb_access") : NULL;
   const char *drv = getenv("HARNESS_VIDEO_DRIVER");
   /* GL's loan is a persistent mapping the GL may keep write-combined:
    * lent only to a core that does not read its frame */
   bool write_only = drv && (!strcmp(drv, "gl") || !strcmp(drv, "glcore"));
   unsigned g0, g1, u0 = 0;
   uint64_t f0, f1;

   CHECK(use_fb && granted && fb_access,
         "harness core lacks the framebuffer exports");
   if (!use_fb || !granted || !fb_access)
      return;

   set_threaded_via_setting(false);
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_frames(5);
   expect_wrapper(false, "lent-window lane");

   /* The loan comes from the driver now, and the core pushes its
    * window into it. Let the swapchain go round a few times. */
   use_fb(2);
   if (write_only)
   {
      g0 = granted();
      run_frames(3);
      CHECK(granted() == g0,
            "lent-window lane: %s lent its framebuffer to a core that reads it",
            drv);
      fb_access(RETRO_MEMORY_ACCESS_WRITE);
   }
   run_frames(3);
#ifdef HAVE_GFX_INSTRUMENT
   gfx_instrument_reset();
#endif
   g0 = granted();
   if (uncached)
      u0 = uncached();
   f0 = core_frames();
   run_frames(60);
   g1 = granted();
   f1 = core_frames();
   /* Uncached loans are a device's own choice where it has no cached
    * host memory, so this is reported, not judged; check-vulkan-rebar
    * holds it to zero on a device that has both. */
   if (uncached && g1 != g0)
      fprintf(stderr, "[info] lent-window lane: %u of %u grants in uncached memory\n",
            uncached() - u0, g1 - g0);
   /* A driver that has no framebuffer to lend (d3d11), or declines
    * this one (d3d12 lends only a row pitch on a 256-byte boundary,
    * which RGB565 at 320 wide is not), has no loan to push a window
    * into: nothing here to test. */
   if (g1 == g0)
   {
      CHECK(!driver_must_have_frame_mem(drv),
            "lent-window lane: %s lent no framebuffer", drv);
      fprintf(stderr, "[skip] lent-window lane (driver lent no framebuffer)\n");
      use_fb(0);
      fb_access(RETRO_MEMORY_ACCESS_WRITE | RETRO_MEMORY_ACCESS_READ);
      if (!menu_is_up())
         command_event(CMD_EVENT_MENU_TOGGLE, NULL);
      return;
   }
   CHECK(g1 - g0 >= 30,
         "lent-window lane: driver granted only %u of 60 asks with threaded video off",
         g1 - g0);
#ifdef HAVE_GFX_INSTRUMENT
   {
      int creates = gfx_instrument_get(GFX_INSTR_FRAME_TEX_CREATE);
      int copies  = gfx_instrument_get(GFX_INSTR_FRAME_COPY_HOST);
      /* Read out of a loan: a window into a lent texture, or (GL,
       * Metal) an upload from the lent frame memory */
      int windows = gfx_instrument_get(GFX_INSTR_FRAME_LENT_WINDOW)
                  + gfx_instrument_get(GFX_INSTR_FRAME_MEM);
      /* The core asks every frame and pushes a window on two of
       * three (the third is a dupe); the oversize frame it pushes
       * every 61st is its own buffer and is copied, as it should be. */
      CHECK(windows >= (int)((g1 - g0) * 2 / 3) - 4,
            "lent-window lane: %u grants but only %d frames read out of the loan",
            g1 - g0, windows);
      CHECK(copies <= 2,
            "lent-window lane: %d frames copied by the host out of a loan", copies);
      /* One loan-sized texture and one window-sized target per
       * swapchain image, once; not one per frame. */
      CHECK(creates <= 2 * LENT_LANE_MAX_IMAGES,
            "lent-window lane: %d frame textures created over %llu frames",
            creates, (unsigned long long)(f1 - f0));
      fprintf(stderr, "[baseline] lent-window lane: %u grants, %d windows, %d host copies, %d textures created over %llu frames\n",
            g1 - g0, windows, copies, creates, (unsigned long long)(f1 - f0));
   }
#else
   (void)f0; (void)f1;
#endif

   /* Pause: the menu over a paused core redraws the cached frame,
    * which is the last window pushed, every iteration. Well past the
    * deferral window of any texture the frames above retired. */
   command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   CHECK(menu_is_up(), "lent-window lane: menu did not open");
   run_frames(4 * LENT_LANE_MAX_IMAGES + 20);
   use_fb(0);
   fb_access(RETRO_MEMORY_ACCESS_WRITE | RETRO_MEMORY_ACCESS_READ);

   if (failures == had)
      fprintf(stderr, "[pass] lent-window lane, paused redraw\n");
}

/* ------------------------------------------------------------------ */
/* Lane: the wrapper's frame buffers are the driver's frame memory     */
/*   A driver with frame_mem_alloc gives the threaded wrapper the       */
/*   buffers its ring slots and spare are made of, and uploads a frame  */
/*   from where it lies in one: the push copies the core's frame in,    */
/*   or the core renders into the loan, and the video thread copies     */
/*   nothing. Each of the core's three ways of pushing - its own        */
/*   buffer, the loan, a window cropped out of the loan - must reach    */
/*   the screen exactly as it does with threaded video off, and with    */
/*   the tree built GFX_INSTRUMENT=1 by that path: every frame drawn    */
/*   from frame memory, none copied by the CPU on the video thread -    */
/*   but a loan at a pitch the driver reads only copied (D3D12 takes    */
/*   rows in place at 256 bytes; a copied frame is padded to that).     */
/* ------------------------------------------------------------------ */

static uint8_t *frame_mem_lane_read(unsigned *dims)
{
   struct video_viewport vp;
   video_driver_state_t *video_st = video_state_get_ptr();
   uint8_t *buf;
   size_t len;

   /* The read draws the cached frame again first, from wherever it
    * lies, which for a frame the core pushed from its own buffer is
    * not the path under test: with none cached, the read draws again
    * what the last frame put in the driver's texture. */
   video_driver_cached_frame_invalidate();
   memset(&vp, 0, sizeof(vp));
   video_driver_get_viewport_info(&vp);
   *dims = vp.dims;
   len   = (size_t)VIDEO_SCALE_W(vp.dims) * VIDEO_SCALE_H(vp.dims) * 3;
   if (     !len || !video_st->current_video->read_viewport
         || !(buf = (uint8_t*)malloc(len)))
      return NULL;
   if (!video_st->current_video->read_viewport(video_st->data, buf, false))
   {
      free(buf);
      return NULL;
   }
   return buf;
}

static const char *const frame_mem_lane_names[3] = {
   "frame memory lane, copied frames",
   "frame memory lane, lent frames",
   "frame memory lane, cropped window"
};

/* Mode @mode with the wrapper up: drawn from frame memory, and on screen
 * as @ref, read with threaded video off */
static void lane_frame_mem_mode(int mode, const uint8_t *ref,
      unsigned ref_dims, void (*use_fb)(int))
{
   const char *name = frame_mem_lane_names[mode];
   unsigned had     = failures;
   unsigned got_dims = 0;
   uint8_t *got;
   size_t differ = 0, len, i;
   thread_video_t *thr = (thread_video_t*)video_state_get_ptr()->data;
   /* A loan is lent at the core's own pitch, which a driver that reads
    * frame memory only at a wider multiple (D3D12's 256 bytes) copies
    * out on the CPU instead; a copied frame is padded to it */
   unsigned lent_pitch = 320 * (thr->info.rgb32 ? 4 : 2);
   bool in_place       = !mode || !thr->frame.pitch_align
      || !(lent_pitch % thr->frame.pitch_align);

   use_fb(mode);
   run_frames(10);
   video_thread_wait_idle();
#ifdef HAVE_GFX_INSTRUMENT
   gfx_instrument_reset();
#endif
   /* Measure frame storage without replacing queued frames. */
   for (i = 0; i < 60; i++)
   {
      run_frames(1);
      video_thread_wait_idle();
   }
#ifdef HAVE_GFX_INSTRUMENT
   {
      int fm     = gfx_instrument_get(GFX_INSTR_FRAME_MEM);
      int copies = gfx_instrument_get(GFX_INSTR_FRAME_COPY_HOST);
      /* Every third frame is a dupe. A frame buffer still in flight
       * is swapped for a host one, which is copied, but not often. */
      if (in_place)
      {
         CHECK(fm >= 20, "%s: only %d of 60 frames drawn from frame memory",
               name, fm);
         CHECK(copies <= fm / 4,
               "%s: %d frames copied by the CPU, %d from frame memory",
               name, copies, fm);
      }
      fprintf(stderr, "[baseline] %s: %d frames from frame memory, %d host copies\n",
            name, fm, copies);
   }
#endif

   got = frame_mem_lane_read(&got_dims);
   CHECK(ref && got, "%s: the viewport could not be read back", name);
   CHECK(ref_dims == got_dims, "%s: viewport %ux%u threaded, %ux%u not",
         name, VIDEO_SCALE_W(got_dims), VIDEO_SCALE_H(got_dims),
         VIDEO_SCALE_W(ref_dims), VIDEO_SCALE_H(ref_dims));
   if (ref && got && ref_dims == got_dims)
   {
      len = (size_t)VIDEO_SCALE_W(ref_dims) * VIDEO_SCALE_H(ref_dims) * 3;
      for (i = 0; i < len; i++)
         if (ref[i] != got[i])
            differ++;
      CHECK(!differ, "%s: %lu of %lu bytes on screen differ from the frame drawn unthreaded",
            name, (unsigned long)differ, (unsigned long)len);
   }
   free(got);

   if (failures == had)
      fprintf(stderr, "[pass] %s\n", name);
}

static void lane_frame_mem(void)
{
   dylib_t lib = runloop_state_get_ptr()->lib_handle;
   void (*use_fb)(int) = lib ? (void (*)(int))dylib_proc(lib, "harness_core_use_framebuffer") : NULL;
   void (*freeze)(unsigned) = lib ? (void (*)(unsigned))dylib_proc(lib, "harness_core_freeze") : NULL;
   void (*fb_access)(unsigned) = lib ? (void (*)(unsigned))dylib_proc(lib, "harness_core_fb_access") : NULL;
   unsigned (*uncached)(void) = lib ? (unsigned (*)(void))dylib_proc(lib, "harness_core_fb_uncached") : NULL;
   uint8_t *ref[3] = { NULL, NULL, NULL };
   unsigned ref_dims[3] = { 0, 0, 0 };
   unsigned driver_mem = 0, i;
   thread_video_t *thr;
   int mode;

   CHECK(use_fb && freeze && fb_access && uncached,
         "harness core lacks the framebuffer exports");
   if (!use_fb || !freeze || !fb_access || !uncached)
      return;

   /* The same frame every run, drawn each way with threaded video off,
    * by a core that only draws into a loan: one that reads it back is
    * lent cached memory, which driver frame memory need not be */
   fb_access(RETRO_MEMORY_ACCESS_WRITE);
   freeze(0x1234);
   set_threaded_via_setting(false);
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   for (mode = 0; mode < 3; mode++)
   {
      use_fb(mode);
      run_frames(8);
      ref[mode] = frame_mem_lane_read(&ref_dims[mode]);
   }

   set_threaded_via_setting(true);
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_frames(10);
   expect_wrapper(true, "frame memory lane");
   video_thread_wait_idle();
   thr = (thread_video_t*)video_state_get_ptr()->data;
   for (i = 0; i < VIDEO_THREAD_FRAME_MEM; i++)
      if (thr->frame.mem[i].id)
         driver_mem++;
   if (!driver_mem)
   {
      CHECK(!driver_must_have_frame_mem(getenv("HARNESS_VIDEO_DRIVER")),
            "frame memory lane: %s has no frame memory",
            getenv("HARNESS_VIDEO_DRIVER"));
      fprintf(stderr, "[skip] frame memory lane (driver has no frame memory)\n");
   }
   else
   {
      CHECK(thr->frame.mem[thr->frame.slot[0].mem].id
            && thr->frame.mem[thr->frame.slot[1].mem].id,
            "frame memory lane: the ring slots are not in the driver's frame memory");
      /* A core that reads its frame back gets cached memory under the
       * wrapper, whatever the driver's frame memory is */
      {
         unsigned u0 = uncached();
         fb_access(RETRO_MEMORY_ACCESS_WRITE | RETRO_MEMORY_ACCESS_READ);
         use_fb(1);
         run_frames(30);
         CHECK(uncached() == u0,
               "frame memory lane: %u loans to a core that reads them in uncached memory",
               uncached() - u0);
         fb_access(RETRO_MEMORY_ACCESS_WRITE);
      }
      for (mode = 0; mode < 3; mode++)
         lane_frame_mem_mode(mode, ref[mode], ref_dims[mode], use_fb);
   }

   for (mode = 0; mode < 3; mode++)
      free(ref[mode]);
   freeze(0);
   use_fb(0);
   fb_access(RETRO_MEMORY_ACCESS_WRITE | RETRO_MEMORY_ACCESS_READ);
   if (!menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   set_threaded_via_setting(false);
}

/* ------------------------------------------------------------------ */
/* Lane: the frame on screen is the core's newest                       */
/*   The core stamps each frame with its number, in bands that survive  */
/*   scaling and filtering, and the lane reads back what the driver     */
/*   draws, copied, lent and lent as a cropped window. With threaded    */
/*   video off that must be the frame the core pushed last, and so must */
/*   it be with the wrapper idle; with frames still in its ring, at     */
/*   most one pushed frame behind. A path that draws a buffer the core  */
/*   has since written again, or one that holds a frame back, shows.    */
/* ------------------------------------------------------------------ */

/* The number a frame read back carries, or -1 where a band is neither
 * set nor clear */
static int frame_lag_stamp(const uint8_t *buf, unsigned dims, bool cropped)
{
   unsigned w = VIDEO_SCALE_W(dims), h = VIDEO_SCALE_H(dims);
   unsigned b, v = 0;
   const uint8_t *row;
   if (!buf || !w || !h)
      return -1;
   /* Mid-height, clear of anything drawn over the frame */
   row = buf + (size_t)(h / 2) * w * 3;
   for (b = 0; b < 8; b++)
   {
      /* The band's middle in the core's 320, or in the 304 the cropped
       * window starts 8 into */
      unsigned sx = 40 * b + 20;
      unsigned x  = cropped ? (sx - 8) * w / 304 : sx * w / 320;
      unsigned g  = row[(size_t)x * 3 + 1];
      if (g >= 192)
         v |= 1u << b;
      else if (g > 64)
         return -1;
   }
   return (int)v;
}

/* Frames pushed after @stamp up to @last, the newest: every third run
 * is a dupe */
static unsigned frame_lag_pushes(unsigned last, int stamp)
{
   unsigned n = 0, k;
   unsigned from = last - ((last - (unsigned)stamp) & 0xff);
   for (k = from + 1; k != last + 1; k++)
      if (k % 3)
         n++;
   return n;
}

static const char *const frame_lag_modes[3] = {
   "copied", "lent", "lent, cropped"
};

static void lane_frame_lag(void)
{
   unsigned had = failures;
   dylib_t lib  = runloop_state_get_ptr()->lib_handle;
   void (*use_fb)(int) = lib ? (void (*)(int))dylib_proc(lib, "harness_core_use_framebuffer") : NULL;
   void (*stamp)(unsigned) = lib ? (void (*)(unsigned))dylib_proc(lib, "harness_core_stamp") : NULL;
   unsigned (*last_pushed)(void) = lib ? (unsigned (*)(void))dylib_proc(lib, "harness_core_last_pushed") : NULL;
   void (*fb_access)(unsigned) = lib ? (void (*)(unsigned))dylib_proc(lib, "harness_core_fb_access") : NULL;
   unsigned worst[2][3];
   int threaded, mode, i;

   CHECK(use_fb && stamp && last_pushed && fb_access,
         "harness core lacks the stamp exports");
   if (!use_fb || !stamp || !last_pushed || !fb_access)
      return;
   memset(worst, 0, sizeof(worst));

   /* Write-only, so every driver that lends does */
   fb_access(RETRO_MEMORY_ACCESS_WRITE);
   stamp(1);
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   for (threaded = 0; threaded < 2; threaded++)
   {
      set_threaded_via_setting(threaded != 0);
      run_frames(6);
      expect_wrapper(threaded != 0, "frame lag lane");
      for (mode = 0; mode < 3; mode++)
      {
         use_fb(mode);
         run_frames(6);
         for (i = 0; i < 30; i++)
         {
            unsigned dims = 0, lag;
            uint8_t *buf;
            int s;
            /* Half the reads with the wrapper idle, so its ring is
             * empty, half with frames still in it */
            bool idle = !threaded || (i & 1);
            run_frames(1);
            if (threaded && idle)
               video_thread_wait_idle();
            buf = frame_mem_lane_read(&dims);
            s   = frame_lag_stamp(buf, dims, mode == 2);
            free(buf);
            CHECK(s >= 0, "frame lag lane (%s, %s): read %d back no frame "
                  "number", threaded ? "threaded" : "unthreaded",
                  frame_lag_modes[mode], i);
            if (s < 0)
               break;
            lag = frame_lag_pushes(last_pushed(), s);
            if (idle)
            {
               CHECK(!lag, "frame lag lane (%s, %s): %u pushed frames "
                     "behind the core's newest, with nothing in flight",
                     threaded ? "threaded" : "unthreaded",
                     frame_lag_modes[mode], lag);
               if (lag)
                  break;
            }
            else if (lag > worst[threaded][mode])
               worst[threaded][mode] = lag;
         }
         if (threaded)
            CHECK(worst[threaded][mode] <= 1,
                  "frame lag lane (threaded, %s): %u pushed frames behind "
                  "the core's newest", frame_lag_modes[mode],
                  worst[threaded][mode]);
      }
   }
   fprintf(stderr, "[baseline] frame lag lane: threaded, frames in flight: "
         "copied %u, lent %u, cropped %u pushed frames behind at worst\n",
         worst[1][0], worst[1][1], worst[1][2]);

   stamp(0);
   use_fb(0);
   fb_access(RETRO_MEMORY_ACCESS_WRITE | RETRO_MEMORY_ACCESS_READ);
   set_threaded_via_setting(false);
   run_frames(2);
   if (!menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   if (failures == had)
      fprintf(stderr, "[pass] frame lag lane\n");
}

/* ------------------------------------------------------------------ */
/* Lane: the pacing flag a driver sees follows the presenter            */
/*   video_frame_info_t::threaded_display_pacing tells a driver its     */
/*   pushes are being held to the display's vblank; Metal takes its     */
/*   next drawable late on it. Set with the setting on only while the  */
/*   wrapper runs - with threaded video off nothing holds the push, and */
/*   a driver relying on its own blocking present must be told so.     */
/* ------------------------------------------------------------------ */

static void lane_pacing_flag_follows_wrapper(void)
{
   unsigned had         = failures;
   settings_t *settings = config_get_ptr();
   bool saved           = settings->bools.video_threaded_display_pacing;
   video_frame_info_t info;

   settings->bools.video_threaded_display_pacing = true;

   set_threaded_via_setting(false);
   run_frames(2);
   expect_wrapper(false, "pacing-flag lane");
   memset(&info, 0, sizeof(info));
   video_driver_build_info(&info);
   CHECK(!info.threaded_display_pacing,
         "pacing-flag lane: set with threaded video off");

   set_threaded_via_setting(true);
   run_frames(2);
   expect_wrapper(true, "pacing-flag lane");
   memset(&info, 0, sizeof(info));
   video_driver_build_info(&info);
   CHECK(info.threaded_display_pacing,
         "pacing-flag lane: clear with the wrapper running and the setting on");

   settings->bools.video_threaded_display_pacing = false;
   memset(&info, 0, sizeof(info));
   video_driver_build_info(&info);
   CHECK(!info.threaded_display_pacing,
         "pacing-flag lane: set with the setting off");

   settings->bools.video_threaded_display_pacing = saved;
   set_threaded_via_setting(false);

   if (failures == had)
      fprintf(stderr, "[pass] pacing-flag lane\n");
}

/* ------------------------------------------------------------------ */
/* Lane: zero-copy with the ring full                                  */
/*   A driver whose present blocks for longer than a display period -  */
/*   a Metal drawable or a FIFO swapchain acquire waiting on vblank -   */
/*   keeps the ring two-deep: one frame queued, one being rendered,     */
/*   every time the core asks. The loan must still be granted and the   */
/*   frames published without a copy, and every frame the driver draws */
/*   must be the one the core pushed, never an older buffer.            */
/* ------------------------------------------------------------------ */

static video_driver_t        fulllane_driver;
static const video_driver_t *fulllane_inner;
static uint16_t              fulllane_prev;
static bool                  fulllane_have_prev;
static unsigned              fulllane_drawn;
static unsigned              fulllane_stale;

static bool fulllane_frame(void *data, const void *frame,
      unsigned dims, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   if (frame && VIDEO_SCALE_H(dims) == 240)
   {
      /* The harness core writes (run + i): the first pixel is the
       * core's run number, which must move forward frame to frame */
      uint16_t cur = ((const uint16_t*)frame)[0];
      if (fulllane_have_prev)
      {
         uint16_t step = (uint16_t)(cur - fulllane_prev);
         if (!step || step >= 0x8000)
            fulllane_stale++;
      }
      fulllane_prev      = cur;
      fulllane_have_prev = true;
      fulllane_drawn++;
   }
   /* The present that waits on vblank */
   retro_sleep(30);
   return fulllane_inner->frame(data, frame, dims, frame_count,
         pitch, msg, video_info);
}

static void lane_zero_copy_ring_full(void)
{
   const char *name = "zero-copy lane, ring full";
   unsigned had = failures;
   dylib_t lib = runloop_state_get_ptr()->lib_handle;
   void (*use_fb)(int) = lib ? (void (*)(int))dylib_proc(lib, "harness_core_use_framebuffer") : NULL;
   unsigned (*granted)(void) = lib ? (unsigned (*)(void))dylib_proc(lib, "harness_core_fb_granted") : NULL;
   thread_video_t *thr;
   unsigned g0, g1, zc0, zc1;

   CHECK(use_fb && granted, "harness core lacks the framebuffer exports");
   if (!use_fb || !granted)
      return;

   set_threaded_via_setting(true);
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_frames(10);
   expect_wrapper(true, name);
   video_thread_wait_idle();

   thr                     = (thread_video_t*)video_state_get_ptr()->data;
   fulllane_inner          = thr->driver;
   fulllane_driver         = *thr->driver;
   fulllane_driver.frame   = fulllane_frame;
   fulllane_have_prev      = false;
   fulllane_drawn          = 0;
   fulllane_stale          = 0;
   set_driver(thr, &fulllane_driver);

   use_fb(1);
   /* Let the queue fill behind the slow present first */
   run_frames(6);
   g0  = granted();
   zc0 = (unsigned)thr->frame.zero_copy_count;
   run_frames(60);
   video_thread_wait_idle();
   g1  = granted();
   zc1 = (unsigned)thr->frame.zero_copy_count;

   /* The staleness check wants a fair number of draws behind the
    * slow present, and how many of the 60 frames above the video
    * thread drew is a race between this thread and a 30 ms present
    * on that GPU - 32 on an M1, 8 on the CI runner's paravirtual
    * device. Keep the core pushing until the driver has drawn its
    * ten; a driver that never gets there is the failure, not a slow
    * one. The grant counts are taken over the 60 frames above. */
   {
      unsigned extra = 0;
      while (fulllane_drawn < 10 && extra < 600)
      {
         run_frames(10);
         extra += 10;
      }
      video_thread_wait_idle();
   }
   use_fb(0);

   set_driver(thr, fulllane_inner);

   /* Every ask is granted but the oversize frame's, which does not
    * ask; every third frame is a dupe whose loan lapses. */
   CHECK(g1 - g0 >= 55, "%s: only %u of 60 asks granted", name, g1 - g0);
   CHECK(zc1 - zc0 >= 35, "%s: only %u frames published zero-copy for %u grants",
         name, zc1 - zc0, g1 - g0);
   CHECK(zc1 - zc0 <= g1 - g0, "%s: %u zero-copy frames for %u grants",
         name, zc1 - zc0, g1 - g0);
   CHECK(fulllane_drawn >= 10, "%s: the driver drew only %u frames in 660", name, fulllane_drawn);
   CHECK(!fulllane_stale, "%s: %u of %u drawn frames were not newer than the one before",
         name, fulllane_stale, fulllane_drawn);

   if (!menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   set_threaded_via_setting(false);

   if (failures == had)
      fprintf(stderr, "[pass] %s (%u grants, %u zero-copy frames, %u drawn)\n",
            name, g1 - g0, zc1 - zc0, fulllane_drawn);
}

/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */
/* Lane: asynchronous texture uploads                                  */
/*   video_thread_async_post() of a caller-owned node must return at   */
/*   once, upload on the video thread, deliver done() on the main      */
/*   thread in post order with the driver's handle, release the image  */
/*   exactly once, and hand every in-flight load a 0 when the wrapper  */
/*   is torn down. Without the wrapper it takes nothing.               */
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
   static video_thread_async_load_t nodes[ASYNC_N];
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
   set_driver(thr, &async_driver);
   set_poke_from(thr, &async_driver);
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
   CHECK(async_post_load(&nodes[0], &imgs[0],
            async_done_cb, (void*)(uintptr_t)0, async_release_cb),
         "async load 0 refused");
   for (i = 0; i < 2000
         && !retro_atomic_load_acquire_int(&async_in_upload); i++)
      retro_sleep(1);
   CHECK(retro_atomic_load_acquire_int(&async_in_upload),
         "the worker never reached the held upload");

   t0 = cpu_features_get_time_usec();
   for (i = 1; i < ASYNC_N; i++)
      CHECK(async_post_load(&nodes[i], &imgs[i],
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
      async_post_load(&nodes[i], &imgs[i],
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

   /* Without the wrapper there is nothing to hand off to: the post is
    * refused and nothing of it runs, so the caller loads directly. */
   async_done_count = async_released = 0;
   CHECK(!async_post_load(&nodes[0], &imgs[0],
            async_done_cb, (void*)0, async_release_cb),
         "a post was taken with no wrapper up");
   CHECK(async_released == 0 && async_done_count == 0,
         "a refused post ran: released %u, done %u",
         async_released, async_done_count);

   if (failures == had)
      fprintf(stderr, "[pass] async texture load lane (%u uploads)\n", ASYNC_N);
}

/* ------------------------------------------------------------------ */
/* Lane: texture uploads batched                                      */
/*   The static textures loaded between two frames go up on one       */
/*   queue submission ahead of the frame, not one each. Without the   */
/*   wrapper, on a real driver that counts its upload submissions     */
/*   (Vulkan, instrumented).                                          */
/* ------------------------------------------------------------------ */

#define UPLOAD_BATCH_N 8

static void lane_upload_batch(void)
{
#ifdef HAVE_GFX_INSTRUMENT
   unsigned had = failures;
   static struct texture_image imgs[UPLOAD_BATCH_N];
   uintptr_t handles[UPLOAD_BATCH_N];
   unsigned i;
   int before, during, after;

   set_threaded_via_setting(false);
   run_frames(2);
   expect_wrapper(false, "upload batch lane");

   before = gfx_instrument_get(GFX_INSTR_UPLOAD_SUBMIT);
   for (i = 0; i < UPLOAD_BATCH_N; i++)
   {
      imgs[i].width = imgs[i].height = 4;
      imgs[i].pixels = (uint32_t*)&imgs[i];
      handles[i]     = 0;
      CHECK(video_driver_texture_load(&imgs[i], TEXTURE_FILTER_LINEAR,
               &handles[i]) && handles[i],
            "upload %u refused", i);
   }
   during = gfx_instrument_get(GFX_INSTR_UPLOAD_SUBMIT);
   if (during == before)
   {
      run_frames(1);
      after = gfx_instrument_get(GFX_INSTR_UPLOAD_SUBMIT);
      CHECK(after - before == 1,
            "%d submission(s) for %u uploads over one frame, wanted 1",
            after - before, UPLOAD_BATCH_N);
   }
   else
   {
      /* A driver that submits as it loads: not batched */
      CHECK(false, "%d submission(s) before any frame for %u uploads",
            during - before, UPLOAD_BATCH_N);
      run_frames(1);
   }
   for (i = 0; i < UPLOAD_BATCH_N; i++)
      video_driver_texture_unload(&handles[i]);
   run_frames(2);

   if (failures == had)
      fprintf(stderr, "[pass] upload batch lane (%u uploads, one submission)\n",
            UPLOAD_BATCH_N);
#else
   fprintf(stderr, "[skip] upload batch lane (not instrumented)\n");
#endif
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
#include "../../../input/input_overlay.h"
#ifdef HAVE_GL_TEXTURE_LEND
#include <glsym/glsym.h>
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_UNSIGNED_INT_8_8_8_8_REV
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#endif

/* What a GL driver holds in texture @id, read back in the words it was
 * uploaded as; direct video only, where the context is this thread's.
 * glcore uploads bytes R,G,B,A and swizzles at sampling, gl uploads
 * the words as BGRA unless the frontend asked for RGBA. */
static bool lend_gl_readback(const char *drv, uintptr_t id, bool rgba,
      unsigned width, unsigned height, uint32_t *out)
{
   bool core = !strcmp(drv, "glcore");
#ifdef HAVE_OPENGLES3
   GLint read_fbo, draw_fbo, pack_buffer;
   GLuint fbo;
   bool ok;
   size_t i;
   glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbo);
   glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
   glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack_buffer);
   glGenFramebuffers(1, &fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
         GL_TEXTURE_2D, (GLuint)id, 0);
   glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
   glPixelStorei(GL_PACK_ALIGNMENT, 4);
   ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
   if (ok) glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, out);
   ok = glGetError() == GL_NO_ERROR && ok;
   glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)pack_buffer);
   glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)read_fbo);
   glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)draw_fbo);
   glDeleteFramebuffers(1, &fbo);
   if (ok && !core && !rgba)
      for (i = 0; i < (size_t)width * height; i++)
      {
         uint8_t *p = (uint8_t*)&out[i];
         uint8_t r = p[0]; p[0] = p[2]; p[2] = r;
      }
   return ok;
#else
   (void)width;
   (void)height;
   glBindTexture(GL_TEXTURE_2D, (GLuint)id);
   glPixelStorei(GL_PACK_ALIGNMENT, 4);
   glGetTexImage(GL_TEXTURE_2D, 0,
         (core || rgba) ? GL_RGBA : GL_BGRA,
         (core || rgba) ? GL_UNSIGNED_BYTE : GL_UNSIGNED_INT_8_8_8_8_REV,
         out);
   glBindTexture(GL_TEXTURE_2D, 0);
   return glGetError() == GL_NO_ERROR;
#endif
}
#endif

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
   unsigned x, y, tries;
   unsigned w = VIDEO_SCALE_W(s->dims), h = VIDEO_SCALE_H(s->dims);
   uint32_t *pixels;
   for (tries = 0; tries < 16 && !gfx_surface_slot_writable(s, slot); tries++)
      run_frames(1);
   CHECK(gfx_surface_slot_writable(s, slot), "surface fill: slot never writable");
   if (!gfx_surface_slot_writable(s, slot))
      return;
   pixels = gfx_surface_slot_begin(s, slot);
   for (y = 0; y < h; y++)
   {
      uint32_t *row = (uint32_t*)((uint8_t*)pixels + (size_t)y * s->pitch[slot]);
      for (x = 0; x < w; x++)
         row[x] = 0xff000000u | (((y * w + x) * 7u + seed * 31u) & 0xffffffu);
   }
   gfx_surface_slot_end(s, slot);
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
static enum video_texture_update surftex_update(void *data, uintptr_t id,
      const struct texture_image *ti, bool threaded)
{
   (void)data; (void)ti; (void)threaded;
   if (!surftex_is_live(id))
      return VIDEO_TEXTURE_UPDATE_REFUSED;
   surftex_updates++;
   return VIDEO_TEXTURE_UPDATE_DONE;
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
      set_driver(thr, &surftex_driver);
      { const video_poke_interface_t *p = NULL; surftex_get_poke(thr->driver_data, &p); set_poke(thr, p); }
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
      set_driver(thr, surftex_inner);
      set_poke(thr, surftex_inner_poke);
   }
   else if (surftex_mode == 2)
      vst->poke = surftex_saved_poke;
   surftex_mode  = 0;
   surftex_inner = NULL;
}

/* External (static) submits: what the surface was handed with a
 * QUEUED upload is freed at the completion even when the owner freed
 * the surface first and release() never runs, and nothing reaches the
 * driver in a layout it was not told about. */
static unsigned surf_payload_frees;

static bool surf_rgb10_yes(void *data, enum texture_gpu_format fmt)
{
   (void)data;
   return fmt == TEXTURE_GPU_FORMAT_RGB10A2;
}

static bool surf_rgb10_no(void *data, enum texture_gpu_format fmt)
{
   (void)data;
   (void)fmt;
   return false;
}

static void surf_payload_free(void *payload)
{
   free(payload);
   surf_payload_frees++;
}

static void lane_surface_external(void)
{
   unsigned had = failures;
   gfx_surface_t *s;
   gfx_surface_src_t src;
   enum gfx_surface_submit_result r;
   bool rgba = (video_driver_get_disp_flags() & VIDEO_FLAG_USE_RGBA) != 0;
   size_t n  = (size_t)64 * 48;
   uint32_t *px;
   size_t i;

   set_threaded_via_setting(true);
   run_frames(3);
   expect_wrapper(true, "external surface lane");
   if (!real_driver())
      CHECK(surftex_install(), "external surface lane: no texture back end");

   /* 10-bit is offered to producers when the texture path takes it,
    * whatever the context says about core frames: a GL context takes
    * 10-bit frames and uploads textures as 8-bit. */
   if (surftex_mode)
   {
      bool (*saved)(void*, enum texture_gpu_format) =
            surftex_poke.supports_texture_format;
      gfx_surface_requirements_t req;
      surftex_poke.supports_texture_format = surf_rgb10_yes;
      gfx_surface_query_requirements(0, &req);
      CHECK(req.formats & IMAGE_PIXFMT_2101010,
            "2101010 not offered by a texture path that takes it");
      surftex_poke.supports_texture_format = surf_rgb10_no;
      gfx_surface_query_requirements(0, &req);
      CHECK(!(req.formats & IMAGE_PIXFMT_2101010),
            "2101010 offered by a texture path that reads it as 8-bit");
      surftex_poke.supports_texture_format = saved;
   }

   /* Freed with its upload in flight: release() must not run, the
    * payload must still be freed - once. */
   s  = gfx_surface_new_static(VIDEO_SCALE_PACK(64, 48),
         TEXTURE_FILTER_LINEAR);
   px = (uint32_t*)malloc(n * sizeof(uint32_t));
   CHECK(s && px, "external surface lane: allocation failed");
   if (!s || !px)
   {
      free(px);
      gfx_surface_free(s);
      surftex_remove();
      return;
   }
   for (i = 0; i < n; i++)
      px[i] = 0xff000000u | (uint32_t)(i * 13u);
   surf_releases      = 0;
   surf_payload_frees = 0;
   src.pixels         = px;
   src.payload        = px;
   src.payload_free   = surf_payload_free;
   src.pixfmt         = IMAGE_PIXFMT_8888;
   src.rgba           = rgba;
   r = gfx_surface_submit_external(s, &src, surf_release_cb, NULL);
   CHECK(r == GFX_SURFACE_SUBMIT_QUEUED,
         "threaded external submit returned %d, not QUEUED", r);
   if (r != GFX_SURFACE_SUBMIT_QUEUED)
      free(px);
   gfx_surface_free(s);
   run_frames(3);
   CHECK(surf_releases == 0,
         "release() ran %u times for a surface freed in flight",
         surf_releases);
   CHECK(surf_payload_frees == (r == GFX_SURFACE_SUBMIT_QUEUED ? 1u : 0u),
         "payload of an upload cancelled in flight freed %u times",
         surf_payload_frees);

   /* Formats the driver does not sample are refused, and the payload
    * stays the caller's. FP16 has no texture encoding yet; 2101010
    * only reaches a driver that takes it. A slotless surface has no
    * slot for submit_pixels to copy into. */
   s  = gfx_surface_new_static(VIDEO_SCALE_PACK(64, 48),
         TEXTURE_FILTER_LINEAR);
   px = (uint32_t*)calloc(n * 2, sizeof(uint32_t));
   CHECK(s && px, "external surface lane: second allocation failed");
   if (s && px)
   {
      gfx_surface_requirements_t req;
      surf_payload_frees = 0;
      src.pixels       = px;
      src.payload      = px;
      src.payload_free = surf_payload_free;
      /* Half floats to a texture path that would read them as 32-bit
       * texels; one that keeps them has the fp16 lane below. */
      if (!video_driver_supports_texture_format(TEXTURE_GPU_FORMAT_RGBA16F))
      {
         src.pixfmt    = IMAGE_PIXFMT_FP16;
         r = gfx_surface_submit_external(s, &src, surf_release_cb, NULL);
         CHECK(r == GFX_SURFACE_SUBMIT_FAILED,
               "FP16 external submit returned %d, not FAILED", r);
         CHECK(!s->payload_free && !s->inflight,
               "refused submit kept a payload or went in flight");
      }
      src.pixfmt       = IMAGE_PIXFMT_GX_RGBA8;
      r = gfx_surface_submit_external(s, &src, surf_release_cb, NULL);
#ifdef GEKKO
      (void)r;
#else
      CHECK(r == GFX_SURFACE_SUBMIT_FAILED,
            "GX-tiled external submit returned %d, not FAILED", r);
#endif
      memset(&req, 0, sizeof(req));
      gfx_surface_query_requirements(0, &req);
      if (!(req.formats & IMAGE_PIXFMT_2101010))
      {
         src.pixfmt    = IMAGE_PIXFMT_2101010;
         r = gfx_surface_submit_external(s, &src, surf_release_cb, NULL);
         CHECK(r == GFX_SURFACE_SUBMIT_FAILED,
               "2101010 submit to a driver without 10-bit returned %d", r);
      }
      CHECK(!!(req.formats & IMAGE_PIXFMT_FP16)
            == (     video_driver_supports_texture_format(
                        TEXTURE_GPU_FORMAT_RGBA16F)
                  && video_driver_supports_texture_format(
                        TEXTURE_GPU_FORMAT_SCRGB)),
            "FP16 offered without a path that shows it as linear scRGB, "
            "or withheld from one");
      r = gfx_surface_submit_pixels(s, px, rgba);
      CHECK(r == GFX_SURFACE_SUBMIT_FAILED,
            "submit_pixels on a slotless surface returned %d", r);
      CHECK(surf_payload_frees == 0,
            "a refused submit freed the caller's payload");
   }
   free(px);
   gfx_surface_free(s);
   run_frames(2);

   /* A driver that takes 2101010 gets it: loaded, updated in place,
    * and reloaded when the layout under the texture changes. Only a
    * real driver has a texture path for it; under a validating Vulkan
    * a copy into a texture of the other layout is an error. */
   {
      gfx_surface_requirements_t req;
      memset(&req, 0, sizeof(req));
      gfx_surface_query_requirements(0, &req);
      if (real_driver() && (req.formats & IMAGE_PIXFMT_2101010))
      {
         uint32_t *p10 = (uint32_t*)malloc(n * sizeof(uint32_t));
         uint32_t *p8  = (uint32_t*)malloc(n * sizeof(uint32_t));
         uintptr_t first = 0;
         unsigned  k, tries;
         s = gfx_surface_new_static(VIDEO_SCALE_PACK(64, 48),
               TEXTURE_FILTER_LINEAR);
         CHECK(s && p10 && p8, "10-bit lane: allocation failed");
         if (s && p10 && p8)
         {
            for (i = 0; i < n; i++)
            {
               p10[i] = 0xC0000000u | ((uint32_t)(i & 1023) << 20)
                      | ((uint32_t)((i * 7) & 1023) << 10)
                      | (uint32_t)((i * 13) & 1023);
               p8[i]  = 0xff000000u | (uint32_t)(i * 2654435761u >> 8);
            }
            src.payload      = NULL;
            src.payload_free = NULL;
            src.rgba         = rgba;
            for (k = 0; k < 3; k++)
            {
               src.pixels = (k < 2) ? (const void*)p10 : (const void*)p8;
               src.pixfmt = (k < 2) ? IMAGE_PIXFMT_2101010
                                    : IMAGE_PIXFMT_8888;
               for (tries = 0; tries < 8; tries++)
               {
                  r = gfx_surface_submit_external(s, &src, NULL, NULL);
                  if (r != GFX_SURFACE_SUBMIT_BUSY)
                     break;
                  run_frames(1);
               }
               CHECK(r == GFX_SURFACE_SUBMIT_QUEUED
                     || r == GFX_SURFACE_SUBMIT_DONE,
                     "10-bit lane: submit %u returned %d", k, r);
               for (tries = 0; tries < 8 && s->inflight; tries++)
                  run_frames(1);
               CHECK(s->handle != 0, "10-bit lane: submit %u left no "
                     "texture", k);
               if (k == 0)
                  first = s->handle;
               else if (k == 1 && video_driver_texture_can_update())
                  CHECK(s->handle == first, "10-bit lane: a second 10-bit "
                        "frame replaced the texture instead of updating it");
            }
         }
         free(p10);
         free(p8);
         gfx_surface_free(s);
         run_frames(2);
         if (failures == had)
            fprintf(stderr, "[pass] 10-bit surface lane (load, update, "
                  "reload as 8888)\n");
      }
   }

   /* Half floats, on a real driver whose texture path keeps them: an
    * FP16 still loads, updates in place and reloads as 8888, and an
    * FP16 stream updates the same texture - from the driver's upload
    * memory where it lends it. Under a validating Vulkan a copy of the
    * wrong size or layout is an error. */
   if (     real_driver()
         && video_driver_supports_texture_format(TEXTURE_GPU_FORMAT_RGBA16F))
   {
      uint16_t *ph  = (uint16_t*)malloc(n * 8);
      uint32_t *p8  = (uint32_t*)malloc(n * sizeof(uint32_t));
      uintptr_t first = 0;
      unsigned  k, tries;
#ifdef HAVE_GFX_INSTRUMENT
      gfx_instrument_reset();
#endif
      s = gfx_surface_new_static(VIDEO_SCALE_PACK(64, 48),
            TEXTURE_FILTER_LINEAR);
      CHECK(s && ph && p8, "fp16 lane: allocation failed");
      if (s && ph && p8)
      {
         for (i = 0; i < n; i++)
         {
            ph[i * 4 + 0] = (uint16_t)(0x3c00u + (i & 0x3ff)); /* 1.0.. */
            ph[i * 4 + 1] = 0x4500u;                            /* 5.0  */
            ph[i * 4 + 2] = 0xb800u;                            /* -0.5 */
            ph[i * 4 + 3] = 0x3c00u;
            p8[i]         = 0xff000000u | (uint32_t)(i * 2654435761u >> 8);
         }
         src.payload      = NULL;
         src.payload_free = NULL;
         src.rgba         = rgba;
         for (k = 0; k < 3; k++)
         {
            src.pixels = (k < 2) ? (const void*)ph : (const void*)p8;
            src.pixfmt = (k < 2) ? IMAGE_PIXFMT_FP16
                                 : IMAGE_PIXFMT_8888;
            for (tries = 0; tries < 8; tries++)
            {
               r = gfx_surface_submit_external(s, &src, NULL, NULL);
               if (r != GFX_SURFACE_SUBMIT_BUSY)
                  break;
               run_frames(1);
            }
            CHECK(r == GFX_SURFACE_SUBMIT_QUEUED
                  || r == GFX_SURFACE_SUBMIT_DONE,
                  "fp16 lane: submit %u returned %d", k, r);
            for (tries = 0; tries < 8 && s->inflight; tries++)
               run_frames(1);
            CHECK(s->handle != 0, "fp16 lane: submit %u left no texture", k);
            if (k == 0)
               first = s->handle;
            else if (k == 1 && video_driver_texture_can_update())
               CHECK(s->handle == first, "fp16 lane: a second FP16 frame "
                     "replaced the texture instead of updating it");
         }
      }
      free(ph);
      free(p8);
      gfx_surface_free(s);
      run_frames(2);

      /* A stream of half floats through two slots. */
      s = gfx_surface_new(VIDEO_SCALE_PACK(64, 48), 2,
            IMAGE_PIXFMT_FP16, TEXTURE_FILTER_LINEAR, NULL, NULL);
      CHECK(s != NULL, "fp16 lane: no stream surface");
      if (s)
      {
         for (k = 0; k < 6; k++)
         {
            unsigned slot = k & 1;
            uint16_t *dst;
            for (tries = 0; tries < 16
                  && (s->inflight || !gfx_surface_slot_writable(s, slot));
                  tries++)
               run_frames(1);
            dst = (uint16_t*)s->slots[slot];
            for (i = 0; i < n * 4; i++)
               dst[i] = (uint16_t)(0x3c00u + ((i + k) & 0xff));
            /* a dropped frame stays in its slot: sent again */
            for (tries = 0; tries < 16; tries++)
            {
               r = gfx_surface_submit(s, slot, rgba);
               if (r != GFX_SURFACE_SUBMIT_DROPPED)
                  break;
               run_frames(1);
            }
            CHECK(r == GFX_SURFACE_SUBMIT_DONE
                  || r == GFX_SURFACE_SUBMIT_QUEUED,
                  "fp16 lane: stream submit %u returned %d", k, r);
            for (tries = 0; tries < 16 && s->inflight; tries++)
               run_frames(1);
            if (k == 0)
               first = s->handle;
            else if (s->can_update)
               CHECK(s->handle == first, "fp16 lane: stream frame %u "
                     "replaced the texture", k);
         }
         gfx_surface_free(s);
         run_frames(2);
      }
#ifdef HAVE_GFX_INSTRUMENT
      /* A refused update loads a replacement, and GL hands the deleted
       * name straight back: the handle cannot tell, the count can. */
      if (video_driver_texture_can_update())
         CHECK(gfx_instrument_get(GFX_INSTR_TEX_UPDATE_REFUSED) == 0,
               "fp16 lane: the driver refused %d half-float updates",
               gfx_instrument_get(GFX_INSTR_TEX_UPDATE_REFUSED));
#endif
      if (failures == had)
         fprintf(stderr, "[pass] fp16 surface lane (still load, update, "
               "reload as 8888; stream in place)\n");
   }

   /* One slot surface per format bit: sized from the format. */
   s = gfx_surface_new(VIDEO_SCALE_PACK(64, 48), 1, IMAGE_PIXFMT_FP16,
         TEXTURE_FILTER_LINEAR, NULL, NULL);
   CHECK(s != NULL, "FP16 slot surface not made");
   if (s)
   {
      memset(s->slots[0], 0, n * IMAGE_PIXFMT_BPP(s->pixfmt));
      if (!video_driver_supports_texture_format(TEXTURE_GPU_FORMAT_RGBA16F))
         CHECK(gfx_surface_submit(s, 0, rgba) == GFX_SURFACE_SUBMIT_FAILED,
               "FP16 slot submit reached a driver with no FP16 path");
      gfx_surface_free(s);
   }
   CHECK(gfx_surface_new(VIDEO_SCALE_PACK(64, 48), 1,
         IMAGE_PIXFMT_8888 | IMAGE_PIXFMT_2101010,
         TEXTURE_FILTER_LINEAR, NULL, NULL) == NULL,
         "a surface of two formats at once was made");

   surftex_remove();
   set_threaded_via_setting(false);
   run_frames(2);
   if (failures == had)
      fprintf(stderr, "[pass] external surface lane (cancelled payload "
            "freed, unsampled formats refused)\n");
}

/* Direct video lends a streaming surface's slots the driver's upload
 * memory: after a slot's first submit it points there, a frame written
 * into it updates the same texture with no copy into that memory, and
 * the surface's own memory is back before the texture goes. Threaded
 * video never lends. Only a driver with upload memory to lend
 * (texture_lend) is held to the direct half. */
#if defined(HAVE_OPENGL) && defined(HAVE_GL_TEXTURE_LEND)
#include "../../../gfx/common/gl2_common.h"

/* gl2 under HDR output draws a half-float texture, linear scRGB, into
 * an RGBA16F layer as the inverse of the encode at the menu nits. No
 * runner has an HDR backbuffer, so this lends the driver one in name:
 * scRGB marked active with no encode program, which leaves the frame
 * in the offscreen layers to be read. A flat texel drawn over the whole
 * layer must come out as the encode's inverse, for the plain gamut and
 * for the one that rotates through Rec.2020 - into the frame's layer
 * for an SDR source, into the UI's own for a PQ one. Direct video
 * only, where the context is this thread's. */
static void lane_gl2_linear(void)
{
   /* Red well above menu white: an 8-bit layer would clip it */
   static const uint16_t texel[4] = { 0x4800, 0x3800, 0x3400, 0x3c00 };
   static const double k709to2020[3][3] = {
      { 0.6274040, 0.3292820, 0.0433136 },
      { 0.0690970, 0.9195400, 0.0113612 },
      { 0.0163916, 0.0880132, 0.8955950 } };
   static const double lin[3] = { 8.0, 0.5, 0.25 };
   video_driver_state_t *video_st = video_state_get_ptr();
   unsigned had = failures, pass;
   uint16_t px[4 * 4 * 4];
   struct texture_image img;
   uintptr_t tex = 0;
   gl2_t *gl;
   bool was_active, was_ok, was_pq;
   unsigned i;

   set_threaded_via_setting(false);
   run_frames(2);
   gl = (gl2_t*)video_st->data;
   if (!gl || !gl->fp16_textures)
   {
      fprintf(stderr, "[skip] gl2 linear lane (no half-float textures)\n");
      return;
   }
   for (i = 0; i < 16; i++)
      memcpy(&px[i * 4], texel, sizeof(texel));
   memset(&img, 0, sizeof(img));
   img.width  = 4;
   img.height = 4;
   img.pixels = (uint32_t*)px;
   img.fp16   = true;
   CHECK(video_driver_texture_load(&img, TEXTURE_FILTER_NEAREST, &tex)
         && tex, "gl2 linear lane: no half-float texture");
   if (!tex)
      return;

   was_active          = gl->scrgb.active;
   was_ok              = gl->scrgb.fp16_ok;
   was_pq              = gl->video_info.source_hdr10;
   gl->scrgb.active    = true;
   gl->scrgb.fp16_ok   = true;
   run_frames(1);
   CHECK(gl->scrgb.fbo && gl->scrgb.tex_fp16,
         "gl2 linear lane: the layer is not RGBA16F after a linear load");

   for (pass = 0; pass < 4 && gl->scrgb.fbo; pass++)
   {
      unsigned  w, h;
      float    *out;
      GLuint    fbo, layer;
      double    want[3], t[3];
      gfx_display_ctx_draw_t draw;
      video_coords_t coords;
      unsigned  c;

      if (pass == 2)
      {
         /* A PQ source: the layers are made again, the UI its own */
         gl->video_info.source_hdr10 = true;
         gl->scrgb.dims              = 0;
         run_frames(1);
         CHECK(gl->scrgb.ui_fbo && gl->scrgb.tex_fp16,
               "gl2 linear lane: a PQ source's UI layer is not RGBA16F");
         if (!gl->scrgb.ui_fbo)
            break;
      }
      fbo   = pass >= 2 ? gl->scrgb.ui_fbo : gl->scrgb.fbo;
      layer = pass >= 2 ? gl->scrgb.ui_tex : gl->scrgb.tex;
      w     = VIDEO_SCALE_W(gl->scrgb.dims);
      h     = VIDEO_SCALE_H(gl->scrgb.dims);

      memset(&coords, 0, sizeof(coords));
      coords.vertices      = 4;
      memset(&draw, 0, sizeof(draw));
      draw.coords          = &coords;
      draw.texture         = tex;
      draw.dims            = gl->scrgb.dims;
      draw.pos             = VIDEO_POS_PACK(0, 0);
      gl->scrgb.menu_nits    = 200.0f;
      gl->scrgb.expand_gamut = (pass & 1) ? 3 : 0;

      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      gfx_display_ctx_gl.blend_begin(gl);
      gfx_display_ctx_gl.draw(&draw, gl, gl->scrgb.dims);
      gfx_display_ctx_gl.blend_end(gl);

      for (c = 0; c < 3; c++)
         t[c] = lin[c] * 80.0 / 200.0;
      for (c = 0; c < 3; c++)
         want[c] = (pass & 1)
            ? k709to2020[c][0] * t[0] + k709to2020[c][1] * t[1]
              + k709to2020[c][2] * t[2]
            : t[c];
      for (c = 0; c < 3; c++)
         want[c] = pow(want[c], 1.0 / 2.4);

      if (!(out = (float*)malloc((size_t)w * h * 4 * sizeof(float))))
         break;
#ifdef HAVE_OPENGLES3
      (void)layer;
      glReadPixels(0, 0, w, h, GL_RGBA, GL_FLOAT, out);
#else
      glBindTexture(GL_TEXTURE_2D, layer);
      glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, out);
      glBindTexture(GL_TEXTURE_2D, 0);
#endif
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      {
         const float *p = out + ((size_t)(h / 2) * w + w / 2) * 4;
         CHECK(   fabs(p[0] - want[0]) < 4e-3 && fabs(p[1] - want[1]) < 4e-3
               && fabs(p[2] - want[2]) < 4e-3 && fabs(p[3] - 1.0) < 4e-3,
               "gl2 linear lane: %s source, gamut %u drew (%.4f %.4f %.4f "
               "%.4f), the encode's inverse is (%.4f %.4f %.4f 1)",
               pass >= 2 ? "PQ" : "SDR", (pass & 1) ? 3u : 0u,
               p[0], p[1], p[2], p[3], want[0], want[1], want[2]);
      }
      free(out);
   }

   video_driver_texture_unload(&tex);
   gl->scrgb.active            = was_active;
   gl->scrgb.fp16_ok           = was_ok;
   gl->video_info.source_hdr10 = was_pq;
   gl->scrgb.dims              = 0;
   run_frames(2);
   if (failures == had)
      fprintf(stderr, "[pass] gl2 linear lane (RGBA16F layers for SDR "
            "and PQ sources, the encode's inverse for two gamuts)\n");
}
#endif

/* Fence injection uses the desktop GL symbol table. */
#if defined(HAVE_GL_TEXTURE_LEND) && !defined(HAVE_OPENGLES)
static GLsync APIENTRY lend_fence_refused(GLenum cond, GLbitfield flags)
{
   (void)cond;
   (void)flags;
   return NULL;
}

/* A lent GL slot whose upload got no fence (glFenceSync can fail) is
 * not free: the upload may still be reading it. With fences refused,
 * the slot an upload just read from must stay unwritable; once fences
 * can be made again, it comes back. Direct video only, where the
 * context is this thread's. */
static void lane_gl_lend_fence_refused(void)
{
   bool rgba = (video_driver_get_disp_flags() & VIDEO_FLAG_USE_RGBA) != 0;
   size_t n  = (size_t)64 * 48;
   RGLSYMGLFENCESYNCPROC real = __rglgen_glFenceSync;
   gfx_surface_t *s;
   unsigned k, tries;
   bool came_back = false;

   set_threaded_via_setting(false);
   run_frames(2);
   s = gfx_surface_new(VIDEO_SCALE_PACK(64, 48), 2,
         IMAGE_PIXFMT_8888, TEXTURE_FILTER_LINEAR, NULL, NULL);
   CHECK(s != NULL, "lend fence lane: no surface");
   if (!s)
      return;
   /* Two frames per slot: the first lends it, the second is read from
    * the lent buffer */
   for (k = 0; k < 4; k++)
   {
      unsigned slot = k & 1;
      for (tries = 0; tries < 16 && !gfx_surface_slot_writable(s, slot);
            tries++)
         run_frames(1);
      memset(s->slots[slot], 0x40 + k, n * sizeof(uint32_t));
      gfx_surface_submit(s, slot, rgba);
      run_frames(1);
   }
   CHECK(s->lent == 3, "lend fence lane: the surface was lent %u",
         (unsigned)s->lent);
   if (s->lent == 3)
   {
      for (tries = 0; tries < 16 && !gfx_surface_slot_writable(s, 0);
            tries++)
         run_frames(1);
      memset(s->slots[0], 0x7f, n * sizeof(uint32_t));
      __rglgen_glFenceSync = lend_fence_refused;
      CHECK(gfx_surface_submit(s, 0, rgba) == GFX_SURFACE_SUBMIT_DONE,
            "lend fence lane: the upload was not taken");
      run_frames(2);
      CHECK(!gfx_surface_slot_writable(s, 0),
            "lend fence lane: a slot whose upload got no fence was handed "
            "back as free");
      __rglgen_glFenceSync = real;
      for (tries = 0; tries < 16 && !came_back; tries++)
      {
         run_frames(1);
         came_back = gfx_surface_slot_writable(s, 0);
      }
      CHECK(came_back, "lend fence lane: the slot never came back once "
            "fences could be made");
   }
   __rglgen_glFenceSync = real;
   gfx_surface_free(s);
   run_frames(2);
}
#endif

static void lane_surface_lend(void)
{
   unsigned had = failures;
   bool rgba    = (video_driver_get_disp_flags() & VIDEO_FLAG_USE_RGBA) != 0;
   size_t n     = (size_t)64 * 48;
   gfx_surface_t *s;
   unsigned pass, k, tries;
   bool lent_any = false, lent_single = false, flipped = false;
   bool lent_threaded = false, lent_threaded_single = false;
   bool flipped_threaded = false;

   if (!real_driver())
   {
      fprintf(stderr, "[skip] surface lend lane (no real driver)\n");
      return;
   }

   /* Passes: direct and threaded with two slots, then direct and
    * threaded with one, which borrows both driver slots and alternates
    * them itself. Under threaded video the video thread lends after an
    * update and says when the GPU is done reading a slot. */
   for (pass = 0; pass < 4; pass++)
   {
      bool threaded   = (pass & 1) != 0;
      unsigned nslots = pass >= 2 ? 1 : 2;
      uintptr_t first = 0;
      uint32_t *seen  = NULL;
      set_threaded_via_setting(threaded);
      run_frames(3);
      expect_wrapper(threaded, "surface lend lane");
      s = gfx_surface_new(VIDEO_SCALE_PACK(64, 48), nslots,
            IMAGE_PIXFMT_8888, TEXTURE_FILTER_LINEAR, NULL, NULL);
      CHECK(s != NULL, "surface lend lane: no surface");
      if (!s)
         continue;
      for (k = 0; k < 8; k++)
      {
         unsigned slot = (k & 1) % nslots;
         size_t i;
         enum gfx_surface_submit_result r;
         for (tries = 0; tries < 16
               && (s->inflight || !gfx_surface_slot_writable(s, slot));
               tries++)
            run_frames(1);
         CHECK(!s->inflight && gfx_surface_slot_writable(s, slot),
               "surface lend lane: slot %u never came back", slot);
         if (nslots == 1 && s->lent)
         {
            if (seen && s->slots[0] != seen)
            {
               if (threaded)
                  flipped_threaded = true;
               else
                  flipped = true;
            }
            seen = s->slots[0];
         }
         for (i = 0; i < n; i++)
            s->slots[slot][i] = 0xff000000u | (uint32_t)(i * 2654435761u
                  >> 8) | k;
         /* A driver with no staging free drops the frame, which stays
          * in the slot: submitted again a frame later, as a producer
          * does. */
         for (tries = 0; tries < 16; tries++)
         {
            r = gfx_surface_submit(s, slot, rgba);
            if (r != GFX_SURFACE_SUBMIT_DROPPED)
               break;
            run_frames(1);
         }
         CHECK(r == GFX_SURFACE_SUBMIT_DONE || r == GFX_SURFACE_SUBMIT_QUEUED,
               "surface lend lane: submit %u returned %d", k, r);
         for (tries = 0; tries < 16 && s->inflight; tries++)
            run_frames(1);
         if (k == 0)
            first = s->handle;
         else if (s->can_update)
            CHECK(s->handle == first, "surface lend lane: frame %u "
                  "replaced the texture", k);
#ifdef HAVE_GL_TEXTURE_LEND
         /* The last frame, uploaded from a lent buffer where the driver
          * lent one, is what the texture holds */
         if (k == 7 && !threaded && s->lent)
         {
            const char *drv = getenv("HARNESS_VIDEO_DRIVER");
            uint32_t   *got = (uint32_t*)malloc(n * sizeof(uint32_t));
            if (got && drv && (!strcmp(drv, "gl") || !strcmp(drv, "glcore")))
            {
               size_t bad = n;
               CHECK(lend_gl_readback(drv, s->handle, rgba, 64, 48, got),
                     "surface lend lane: texture read back failed");
               for (i = 0; i < n && bad == n; i++)
                  if (got[i] != (0xff000000u
                           | (uint32_t)(i * 2654435761u >> 8) | k))
                     bad = i;
               CHECK(bad == n, "surface lend lane: %s texture holds "
                     "%08x at pixel %u, not the lent frame",
                     drv, bad < n ? (unsigned)got[bad] : 0u, (unsigned)bad);
            }
            free(got);
         }
#endif
      }
      if (nslots == 1)
      {
         if (threaded)
            lent_threaded_single = s->lent == 3;
         else
            lent_single = s->lent == 3;
         if (s->lent)
            CHECK(     s->lent == 3 && s->lent_spare
                    && s->slots[0] != s->own_slots[0],
                  "surface lend lane: one slot borrowed %u, not both",
                  (unsigned)s->lent);
      }
      else
      {
         if (threaded)
            lent_threaded = s->lent == 3;
         else
            lent_any = s->lent != 0;
         if (s->lent)
            CHECK(     s->slots[0] != s->own_slots[0]
                    && s->slots[1] != s->own_slots[1],
                  "surface lend lane: lent bits without lent slots");
      }
      gfx_surface_free(s);
      run_frames(2);
   }
   set_threaded_via_setting(false);
   run_frames(2);
   {
      /* The drivers that lend: there the direct half must have. The
       * GL drivers lend where buffers can be mapped for good (GL 4.4
       * or ARB_buffer_storage), which llvmpipe has, and not in a GLES
       * build (gl_common.h). */
      const char *drv = getenv("HARNESS_VIDEO_DRIVER");
      if (drv && (     !strcmp(drv, "vulkan") || !strcmp(drv, "d3d12")
#ifdef HAVE_GL_TEXTURE_LEND
                    || !strcmp(drv, "gl")     || !strcmp(drv, "glcore")
#endif
                    || !strcmp(drv, "d3d11")
                    || !strcmp(drv, "metal")))
      {
         CHECK(lent_any, "surface lend lane: %s lent no slot", drv);
         CHECK(lent_single, "surface lend lane: %s lent a one-slot "
               "surface nothing", drv);
         CHECK(lent_threaded, "surface lend lane: %s lent nothing under "
               "threaded video", drv);
         CHECK(lent_threaded_single, "surface lend lane: %s lent a "
               "one-slot surface nothing under threaded video", drv);
      }
   }
#if defined(HAVE_GL_TEXTURE_LEND) && !defined(HAVE_OPENGLES)
   {
      const char *drv = getenv("HARNESS_VIDEO_DRIVER");
      if (drv && (!strcmp(drv, "gl") || !strcmp(drv, "glcore")))
         lane_gl_lend_fence_refused();
   }
#endif
   if (failures == had)
      fprintf(stderr, "[pass] surface lend lane (direct %s, one slot %s; "
            "threaded %s, one slot %s)\n",
            lent_any ? "lent both slots" : "lent nothing",
            lent_single ? (flipped ? "double buffered" : "borrowed two")
                        : "lent nothing",
            lent_threaded ? "lent both slots" : "lent nothing",
            lent_threaded_single
               ? (flipped_threaded ? "double buffered" : "borrowed two")
               : "lent nothing");
}

static void lane_surface_update(void)
{
   unsigned had = failures;
   gfx_surface_t *s;
   enum gfx_surface_submit_result r;
   bool rgba = (video_driver_get_disp_flags() & VIDEO_FLAG_USE_RGBA) != 0;
   unsigned i, tries;
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
   s = gfx_surface_new(VIDEO_SCALE_PACK(64, 48), 2, IMAGE_PIXFMT_8888, TEXTURE_FILTER_LINEAR, surf_release_cb, NULL);
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
      int copies  = gfx_instrument_get(GFX_INSTR_SUBMIT_COPY);
      fprintf(stderr, "[baseline] surface, threaded: %d loads, %d updates, "
            "%d posts, %d copies\n",
            loads, updates, posts, copies);
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
   s = gfx_surface_new(VIDEO_SCALE_PACK(64, 48), 1, IMAGE_PIXFMT_8888, TEXTURE_FILTER_LINEAR, surf_release_cb, NULL);
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
      /* A driver with no staging free drops the frame, which stays in
       * the slot: submitted again a frame later, as a producer does. */
      for (tries = 0; tries < 16; tries++)
      {
         r = gfx_surface_submit(s, 0, rgba);
         if (r != GFX_SURFACE_SUBMIT_DROPPED)
            break;
         run_frames(1);
      }
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
      int copies  = gfx_instrument_get(GFX_INSTR_SUBMIT_COPY);
      fprintf(stderr, "[baseline] surface, direct: %d loads, %d updates, "
            "%d unloads, %d posts, %d copies\n",
            loads, updates, unloads, posts, copies);
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
 * itself did. Windows charges thread time in whole 15.625 ms ticks to
 * whichever thread holds the CPU at the tick, which says nothing about
 * a submit of microseconds: there the wall clock is read instead, and
 * the median checked. */
#if defined(_WIN32)
#define SUBMIT_CLOCK_WALL 1
#endif

static int64_t thread_cpu_usec(void)
{
#if defined(CLOCK_THREAD_CPUTIME_ID) && !defined(SUBMIT_CLOCK_WALL)
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

   s = gfx_surface_new(VIDEO_SCALE_PACK(3840, 2160), 2, IMAGE_PIXFMT_8888, TEXTURE_FILTER_LINEAR,
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
       * would be milliseconds on every submit; the budget leaves room
       * for a slow host and none for a copy. On the wall clock a
       * preempted submit is charged its wait, which the median rides
       * out and a copy on every submit does not. */
#ifdef SUBMIT_CLOCK_WALL
      CHECK(submit_us[n_sub / 2] < 2000,
            "4K submit median is %lld us: that is a copy, not a hand-off",
            (long long)submit_us[n_sub / 2]);
#else
      CHECK(submit_total / n_sub < 2000,
            "4K submit mean is %lld us: that is a copy, not a hand-off",
            (long long)(submit_total / n_sub));
#endif
   }
   CHECK(n_sub >= 30, "only %u of 60 4K submits were accepted", n_sub);
   if (s->handle && video_driver_texture_can_update())
      CHECK(s->handle == first, "4K in-place driver replaced the texture");

#ifdef HAVE_GFX_INSTRUMENT
   {
      int copies = gfx_instrument_get(GFX_INSTR_SUBMIT_COPY);
      int loads  = gfx_instrument_get(GFX_INSTR_TEX_LOAD);
      fprintf(stderr, "[baseline] 4k surface: %d loads, %d copies\n",
            loads, copies);
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
         /* the driver published its mouse in that poll; the frontend
          * answers for it */
         (void)input_driver_device_state(0,
               RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_BUTTON_4);
         if (input_driver_device_state(0,
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
   /* A frame still queued requests on the shared connection - a title
    * change is three properties - which would count as the poll's. */
   video_thread_wait_idle();

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

      /* the motion the driver published in that poll, one packed word */
      {
         uint32_t rel = input_driver_device_pos(0, RETRO_DEVICE_MOUSE, 0);
         dx           = VIDEO_POS_X(rel);
         dy           = VIDEO_POS_Y(rel);
      }
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

#if defined(HAVE_X11) && defined(HAVE_VULKAN)
static void wsilane_no_poll(void *data) { (void)data; }
#endif

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

/* ------------------------------------------------------------------ */
/* Lane: a frame that was read back goes to present once              */
/*   A synchronous read_viewport (a GPU screenshot) renders the       */
/*   cached frame with a readback pending: the backbuffer goes to     */
/*   TRANSFER_SRC for the copy and from there to PRESENT_SRC. That    */
/*   frame takes no other transition - one more from                  */
/*   COLOR_ATTACHMENT starts from a layout the image no longer has,   */
/*   which the validation layer reports. Run on a tree configured     */
/*   with OpenXR too, where the headset checks sit in the same chain. */
/*   Unthreaded, and under the wrapper through CMD_READ_VIEWPORT.     */
/*   On d3d11 and d3d12 the read must not need a recording running:   */
/*   their streamed recording ring is also what a screenshot reads.   */
/* ------------------------------------------------------------------ */

static void readback_once(bool threaded)
{
   struct video_viewport vp;
   video_driver_state_t *video_st = video_state_get_ptr();
   const char *mode               = threaded ? "threaded" : "unthreaded";
   uint8_t *buf                   = NULL;
   size_t len;
   bool ok;

   set_threaded_via_setting(threaded);
   run_frames(4);
   expect_wrapper(threaded, "readback lane");
   if (threaded)
      video_thread_wait_idle();

   memset(&vp, 0, sizeof(vp));
   video_driver_get_viewport_info(&vp);
   len = (size_t)VIDEO_SCALE_W(vp.dims) * VIDEO_SCALE_H(vp.dims) * 3;
   CHECK(len > 0, "readback lane (%s): no viewport to read", mode);
   if (!len || !video_st->current_video->read_viewport)
      return;
   if (!(buf = (uint8_t*)malloc(len)))
      return;

   ok = video_st->current_video->read_viewport(video_st->data, buf, false);
   CHECK(ok, "readback lane (%s): read_viewport failed", mode);
   free(buf);

   /* The read-back image is acquired and presented again after it. */
   run_frames(4);
}

/* ------------------------------------------------------------------ */
/* Lane: a GPU recording's frames keep the format it was opened for   */
/*   A GPU recording opened unthreaded on a driver with the 32-bit    */
/*   readback takes B,G,R,X frames. Threaded Video switched on during */
/*   it puts the wrapper's readback in charge: 32-bit rows straight   */
/*   from its read when the driver has a record reader, else BGR24    */
/*   widened into the recording's own buffer. The recorder must get   */
/*   32-bit rows either way, every row of them readable.              */
/* ------------------------------------------------------------------ */

static unsigned       reclane_pushes;
static int            reclane_pitch;
static const uint8_t *reclane_data;
static unsigned       reclane_rows;
static unsigned       reclane_sum;

static bool reclane_push_video(void *data,
      const struct record_video_data *v)
{
   unsigned y;
   size_t   row;
   (void)data;
   if (v->is_dupe || !v->data)
      return true;
   reclane_pushes++;
   reclane_pitch = v->pitch;
   reclane_data  = (const uint8_t*)v->data;
   /* Each row's last byte, as the encoder's copy reads every row */
   row           = (size_t)(v->pitch < 0 ? -v->pitch : v->pitch);
   for (y = 0; y < reclane_rows; y++)
      reclane_sum += ((const uint8_t*)v->data)[(ptrdiff_t)y * v->pitch
            + (ptrdiff_t)row - 1];
   return true;
}

static bool reclane_push_audio(void *data,
      const struct record_audio_data *a)
{
   (void)data;
   (void)a;
   return true;
}

static const record_driver_t reclane_driver = {
   NULL, NULL, reclane_push_video, reclane_push_audio, NULL, "reclane"
};

static void record_format_once(bool threaded)
{
   struct video_viewport vp;
   video_driver_state_t *video_st = video_state_get_ptr();
   recording_state_t    *rec_st   = recording_state_get_ptr();
   const char *mode               = threaded ? "threaded" : "unthreaded";
   recording_state_t saved;
   uint8_t *saved_buf;
   uint8_t *buf;
   size_t   area;
   unsigned i;

   set_threaded_via_setting(threaded);
   run_frames(4);
   expect_wrapper(threaded, "record format lane");
   /* Frames the core makes are recorded; the menu's dupes are not */
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   CHECK(!menu_is_up(), "record format lane (%s): menu still up", mode);
   run_frames(2);
   if (threaded)
      video_thread_wait_idle();

   memset(&vp, 0, sizeof(vp));
   video_driver_get_viewport_info(&vp);
   area = VIDEO_SCALE_AREA(vp.dims);
   CHECK(area > 0, "record format lane (%s): no viewport", mode);
   if (!area || !(buf = (uint8_t*)malloc(area * 4)))
      return;

   /* A recording opened for 32-bit frames, as on this driver unthreaded */
   saved                     = *rec_st;
   saved_buf                 = video_st->record_gpu_buffer;
   reclane_pushes            = 0;
   reclane_rows              = VIDEO_SCALE_H(vp.dims);
   rec_st->driver            = &reclane_driver;
   rec_st->gpu_dims          = vp.dims;
   rec_st->gpu_bgrx          = true;
   rec_st->enable            = true;
   video_st->record_gpu_buffer = buf;
   rec_st->data              = (void*)&reclane_driver;

   /* The wrapper's readback starts a few frames behind */
   for (i = 0; i < 32 && !reclane_pushes; i++)
      run_frames(1);

   rec_st->data              = NULL;
   if (threaded)
      video_thread_wait_idle();
   *rec_st                   = saved;
   video_st->record_gpu_buffer = saved_buf;

   CHECK(reclane_pushes > 0, "record format lane (%s): no frame recorded",
         mode);
   if (reclane_pushes)
   {
      CHECK(reclane_pitch == (int)VIDEO_SCALE_W(vp.dims) * 4
            || reclane_pitch == -(int)VIDEO_SCALE_W(vp.dims) * 4,
            "record format lane (%s): rows of %d bytes for a %u wide "
            "32-bit recording", mode, reclane_pitch, VIDEO_SCALE_W(vp.dims));
      if (!threaded || !video_thread_record_has_reader(video_st->data))
         CHECK(reclane_data >= buf && reclane_data < buf + area * 4,
               "record format lane (%s): the frame is not in the "
               "recording's own buffer", mode);
   }
   free(buf);
}

/* A recording stopped while a frame that reads it back is drawing:
 * the stop hands the buffers back to be freed once no frame names
 * them, and does not wait for the frame */
static const video_driver_t *rstop_inner;
static video_driver_t        rstop_driver;
static retro_atomic_int_t    rstop_hold;
static retro_atomic_int_t    rstop_held;

static bool rstop_frame(void *data, const void *frame,
      unsigned dims, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   if (retro_atomic_load_acquire_int(&rstop_hold))
   {
      retro_time_t until = cpu_features_get_time_usec() + 1000000;
      retro_atomic_store_release_int(&rstop_held, 1);
      while (     retro_atomic_load_acquire_int(&rstop_hold)
            &&  cpu_features_get_time_usec() < until) { }
   }
   if (video_info->recording_gpu) return true;
   return rstop_inner->frame(data, frame, dims, frame_count,
         pitch, msg, video_info);
}

static void record_stop_in_flight(void)
{
   struct video_viewport vp;
   video_driver_state_t *video_st = video_state_get_ptr();
   recording_state_t    *rec_st   = recording_state_get_ptr();
   recording_state_t saved;
   uint8_t *saved_buf;
   uint8_t *buf;
   thread_video_t *thr;
   retro_time_t took;
   size_t   area;
   unsigned i;

   set_threaded_via_setting(true);
   run_frames(4);
   expect_wrapper(true, "record stop lane");
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_frames(2);
   video_thread_wait_idle();
   memset(&vp, 0, sizeof(vp));
   video_driver_get_viewport_info(&vp);
   area = VIDEO_SCALE_AREA(vp.dims);
   if (!area || !(buf = (uint8_t*)malloc(area * 4)))
      return;

   saved                       = *rec_st;
   saved_buf                   = video_st->record_gpu_buffer;
   reclane_pushes              = 0;
   reclane_rows                = VIDEO_SCALE_H(vp.dims);
   rec_st->driver              = &reclane_driver;
   rec_st->gpu_dims            = vp.dims;
   rec_st->gpu_bgrx            = true;
   rec_st->enable              = true;
   video_st->record_gpu_buffer = buf;
   rec_st->data                = (void*)&reclane_driver;
   for (i = 0; i < 32 && !reclane_pushes; i++)
      run_frames(1);

   thr                 = (thread_video_t*)video_st->data;
   rstop_inner        = thr->driver;
   rstop_driver       = *thr->driver;
   rstop_driver.frame = rstop_frame;
   set_driver(thr, &rstop_driver);
   retro_atomic_store_release_int(&rstop_held, 0);
   retro_atomic_store_release_int(&rstop_hold, 1);
   /* A recorded frame drawing, then the recording stopped */
   run_frames(1);
   for (i = 0; i < 1000 && !retro_atomic_load_acquire_int(&rstop_held); i++)
      retro_sleep(1);
   took = cpu_features_get_time_usec();
   recording_deinit();
   took = cpu_features_get_time_usec() - took;
   retro_atomic_store_release_int(&rstop_hold, 0);
   video_thread_wait_idle();
   set_driver(thr, rstop_inner);
   run_frames(4);

   CHECK(retro_atomic_load_acquire_int(&rstop_held),
         "record stop lane: no recorded frame was in flight");
   CHECK(took < 200000,
         "record stop lane: the stop waited %lld us for the frame",
         (long long)took);
   CHECK(!rec_st->data && !video_st->record_gpu_buffer,
         "record stop lane: the recording is still open");

   /* recording_deinit() freed buf as the recording's buffer */
   *rec_st                     = saved;
   video_st->record_gpu_buffer = saved_buf;
}

static void record_gpu_stop_in_flight(void)
{
   video_driver_state_t *video = video_state_get_ptr();
   thread_video_t *thr;
   video_frame_info_t info;
   uint32_t pixels[64] = {0};
   unsigned i, had = failures;
   bool was_menu = menu_is_up();
   bool was_threaded = video_driver_thread_wrapper_active();
   set_threaded_via_setting(true);
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_frames(2);
   video_thread_wait_idle();
   thr = (thread_video_t*)video->data;
   rstop_inner = thr->driver;
   rstop_driver = *thr->driver;
   rstop_driver.frame = rstop_frame;
   set_driver(thr, &rstop_driver);
   memset(&info, 0, sizeof(info));
   video_driver_build_info(&info);
   info.threaded_display_pacing = false;
   info.input_driver_nonblock_state = true;
   info.recording_gpu = &rstop_driver;
   retro_atomic_store_release_int(&rstop_held, 0);
   retro_atomic_store_release_int(&rstop_hold, 1);
   video->current_video->frame(video->data, pixels, VIDEO_SCALE_PACK(8, 8),
         0, 32, NULL, &info);
   for (i = 0; i < 1000 && !retro_atomic_load_acquire_int(&rstop_held); i++)
      retro_sleep(1);
   CHECK(retro_atomic_load_acquire_int(&rstop_held), "GPU lease: no drawing frame");
   CHECK(video_thread_record_gpu_in_flight(thr, &rstop_driver),
         "GPU lease: drawing frame was missed (state %d, slots %p/%p, want %p)",
         retro_atomic_load_acquire_int(&thr->frame.state),
         thr->frame.slot[0].video_info.recording_gpu,
         thr->frame.slot[1].video_info.recording_gpu, (void*)&rstop_driver);
   info.recording_gpu = &rstop_hold;
   video->current_video->frame(video->data, pixels, VIDEO_SCALE_PACK(8, 8),
         1, 32, NULL, &info);
   CHECK(video_thread_record_gpu_in_flight(thr, &rstop_hold),
         "GPU lease: queued frame was missed");
   CHECK(!video_thread_record_gpu_in_flight(thr, &rstop_held)
         && !video_thread_record_gpu_in_flight(thr, NULL),
         "GPU lease: unrelated recorder matched");
   retro_atomic_store_release_int(&rstop_hold, 0);
   video_thread_wait_idle();
   CHECK(!video_thread_record_gpu_in_flight(thr, &rstop_driver)
         && !video_thread_record_gpu_in_flight(thr, &rstop_hold),
         "GPU lease: stale idle slot retained recorder");
   set_driver(thr, rstop_inner);
   set_threaded_via_setting(was_threaded);
   if (was_menu != menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_frames(2);
   if (failures == had)
      fprintf(stderr, "[pass] GPU recording frame leases (drawing, queued, idle)\n");
}

static void lane_record_format(void)
{
   unsigned had = failures;

   record_format_once(false);
   record_format_once(true);
   record_stop_in_flight();
   set_threaded_via_setting(false);
   run_frames(2);

   if (failures == had)
      fprintf(stderr, "[pass] gpu record format lane (and a stop in flight)\n");
}

/* ------------------------------------------------------------------ */
/* Lane: a core's 0RGB1555 and XRGB2101010 frames, converted          */
/*   A driver gets 0RGB1555 frames as RGB565 and XRGB2101010 ones     */
/*   narrowed to XRGB8888, converted on the main thread without the   */
/*   wrapper and on the video thread with it. Either way every pixel  */
/*   of the frame's first row must be the converted core pixel, at   */
/*   the converted format's pitch.                                    */
/* ------------------------------------------------------------------ */

#define PIXLANE_W     320
#define PIXLANE_FROZE 0x1234u

static video_driver_t      pixlane_driver;
static const video_driver_t *pixlane_inner;
static uint32_t            pixlane_row[PIXLANE_W];
static unsigned            pixlane_pitch;
static unsigned            pixlane_dims;
/* Frames captured; stored with release after the row */
static retro_atomic_int_t  pixlane_frames;
static bool                pixlane_wide; /* the row is 32-bit */

static bool pixlane_frame(void *data, const void *frame, unsigned dims,
      uint64_t frame_count, unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
   if (frame && VIDEO_SCALE_W(dims) == PIXLANE_W)
   {
      unsigned x;
      for (x = 0; x < PIXLANE_W; x++)
         pixlane_row[x] = pixlane_wide
            ? ((const uint32_t*)frame)[x]
            : ((const uint16_t*)frame)[x];
      pixlane_pitch = pitch;
      pixlane_dims  = dims;
      retro_atomic_inc_int(&pixlane_frames);
   }
   return pixlane_inner->frame(data, frame, dims, frame_count, pitch, msg,
         video_info);
}

static void pixlane_once(unsigned fmt, bool threaded)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   dylib_t lib = runloop_state_get_ptr()->lib_handle;
   bool (*set_format)(unsigned) = lib
      ? (bool (*)(unsigned))dylib_proc(lib, "harness_core_set_format") : NULL;
   void (*freeze)(unsigned) = lib
      ? (void (*)(unsigned))dylib_proc(lib, "harness_core_freeze") : NULL;
   const char *name = fmt == RETRO_PIXEL_FORMAT_0RGB1555
      ? "0RGB1555" : "XRGB2101010";
   const char *mode = threaded ? "threaded" : "unthreaded";
   int flags        = DRIVER_VIDEO_MASK;
   unsigned i, bad;

   CHECK(set_format && freeze, "harness core lacks the format exports");
   if (!set_format || !freeze)
      return;
   CHECK(set_format(fmt), "pixel format lane: %s refused", name);

   /* The converters are made with the driver */
   set_threaded_via_setting(threaded);
   command_event(CMD_EVENT_REINIT, &flags);
   run_frames(4);
   expect_wrapper(threaded, "pixel format lane");
   if (threaded)
      video_thread_wait_idle();

   pixlane_wide = fmt == RETRO_PIXEL_FORMAT_XRGB2101010;
   retro_atomic_store_release_int(&pixlane_frames, 0);
   if (threaded)
   {
      thread_video_t *thr = (thread_video_t*)video_st->data;
      pixlane_inner        = thr->driver;
      pixlane_driver       = *thr->driver;
      pixlane_driver.frame = pixlane_frame;
      set_driver(thr, &pixlane_driver);
   }
   else
   {
      pixlane_inner          = video_st->current_video;
      pixlane_driver         = *video_st->current_video;
      pixlane_driver.frame   = pixlane_frame;
      video_st->current_video = &pixlane_driver;
   }

   freeze(PIXLANE_FROZE);
   for (i = 0; i < 64
         && retro_atomic_load_acquire_int(&pixlane_frames) < 2; i++)
   {
      run_frames(1);
      if (threaded)
         video_thread_wait_idle();
   }
   freeze(0);

   if (threaded)
      set_driver((thread_video_t*)video_st->data, pixlane_inner);
   else
      video_st->current_video = (video_driver_t*)pixlane_inner;

   CHECK(retro_atomic_load_acquire_int(&pixlane_frames) >= 2,
         "pixel format lane (%s, %s): no frame reached the driver", name,
         mode);
   if (retro_atomic_load_acquire_int(&pixlane_frames) >= 2)
   {
      for (bad = 0, i = 0; i < PIXLANE_W; i++)
      {
         uint32_t v = PIXLANE_FROZE + i;
         uint32_t want;
         if (pixlane_wide)
            want = 0xff000000u
                 | ((v >> 22) & 0xff) << 16
                 | ((v >> 12) & 0xff) << 8
                 | ((v >>  2) & 0xff);
         else
         {
            uint16_t src = (uint16_t)v, out;
            conv_0rgb1555_rgb565(&out, &src, 1, 1, 2, 2);
            want = out;
         }
         if (pixlane_row[i] != want)
            bad++;
      }
      CHECK(!bad, "pixel format lane (%s, %s): %u of %u pixels not "
            "converted (first %08x, dims %ux%u)", name, mode, bad, PIXLANE_W,
            (unsigned)pixlane_row[0], VIDEO_SCALE_W(pixlane_dims),
            VIDEO_SCALE_H(pixlane_dims));
      CHECK(pixlane_pitch == PIXLANE_W * (pixlane_wide ? 4u : 2u),
            "pixel format lane (%s, %s): pitch %u", name, mode,
            pixlane_pitch);
   }
}

static void lane_pixel_format(void)
{
   unsigned had = failures;
   dylib_t lib  = runloop_state_get_ptr()->lib_handle;
   bool (*set_format)(unsigned) = lib
      ? (bool (*)(unsigned))dylib_proc(lib, "harness_core_set_format") : NULL;
   bool menu_was_up = menu_is_up();
   int flags        = DRIVER_VIDEO_MASK;
   enum retro_pixel_format was = video_state_get_ptr()->pix_fmt;

   if (menu_was_up)
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   pixlane_once(RETRO_PIXEL_FORMAT_0RGB1555,    false);
   pixlane_once(RETRO_PIXEL_FORMAT_0RGB1555,    true);
   pixlane_once(RETRO_PIXEL_FORMAT_XRGB2101010, false);
   pixlane_once(RETRO_PIXEL_FORMAT_XRGB2101010, true);

   /* Back to the core's own format, the driver made for it */
   if (set_format)
      set_format((unsigned)was);
   set_threaded_via_setting(false);
   command_event(CMD_EVENT_REINIT, &flags);
   run_frames(2);
   if (menu_was_up)
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_frames(2);

   if (failures == had)
      fprintf(stderr, "[pass] pixel format lane\n");
}

/* ------------------------------------------------------------------ */
/* Lane: the wrapper reads back in the recording's own format         */
/*   A driver whose poke has record_read is read back on the video    */
/*   thread, and a GPU recording opened for 32-bit frames gets them   */
/*   from that read as B,G,R,X, in the row order the driver wrote -   */
/*   never as BGR24 widened on the main thread. A BGR24 recording     */
/*   gets BGR24, bottom row first.                                    */
/* ------------------------------------------------------------------ */

#define RECREAD_W 64
#define RECREAD_H 48

static video_driver_t                recread_driver;
static const video_driver_t         *recread_inner;
static video_poke_interface_t        recread_poke;
static retro_atomic_int_t            recread_bgrx_reads;
static retro_atomic_int_t            recread_bgr24_reads;

static void recread_viewport_info(void *data, struct video_viewport *vp)
{
   (void)data;
   vp->pos       = VIDEO_POS_PACK(0, 0);
   vp->dims      = VIDEO_SCALE_PACK(RECREAD_W, RECREAD_H);
   vp->full_dims = vp->dims;
}

/* B = x, G = the row from the top, R = 0x5a: B,G,R,X top row first, or
 * BGR24 bottom row first. */
static bool recread_read(void *data, uint8_t *buffer, bool bgrx,
      bool *bottom_up)
{
   unsigned x, y;
   (void)data;
   for (y = 0; y < RECREAD_H; y++)
      for (x = 0; x < RECREAD_W; x++)
      {
         if (bgrx)
         {
            uint8_t *p = buffer + ((size_t)y * RECREAD_W + x) * 4;
            p[0] = (uint8_t)x;
            p[1] = (uint8_t)y;
            p[2] = 0x5a;
            p[3] = 0xff;
         }
         else
         {
            uint8_t *p = buffer
               + ((size_t)(RECREAD_H - 1 - y) * RECREAD_W + x) * 3;
            p[0] = (uint8_t)x;
            p[1] = (uint8_t)y;
            p[2] = 0x5a;
         }
      }
   *bottom_up = !bgrx;
   retro_atomic_inc_int(bgrx ? &recread_bgrx_reads : &recread_bgr24_reads);
   return true;
}

static void recread_get_poke(void *data, const video_poke_interface_t **iface)
{
   const video_poke_interface_t *inner = NULL;
   if (recread_inner->poke_interface)
      recread_inner->poke_interface(data, &inner);
   if (inner)
      recread_poke = *inner;
   else
      memset(&recread_poke, 0, sizeof(recread_poke));
   recread_poke.record_read = recread_read;
   *iface                   = &recread_poke;
}

static int            recread_pitch;
static const uint8_t *recread_data;
static unsigned       recread_pushes;
static unsigned       recread_bad;

static bool recread_push_video(void *data,
      const struct record_video_data *v)
{
   unsigned x, y;
   unsigned bpp;
   (void)data;
   if (v->is_dupe || !v->data)
      return true;
   recread_pushes++;
   recread_pitch = v->pitch;
   recread_data  = (const uint8_t*)v->data;
   bpp           = (unsigned)((v->pitch < 0 ? -v->pitch : v->pitch)
         / RECREAD_W);
   /* Every pixel, walked with the pitch the encoder is handed */
   for (y = 0; y < RECREAD_H; y++)
      for (x = 0; x < RECREAD_W; x++)
      {
         const uint8_t *p = (const uint8_t*)v->data
            + (ptrdiff_t)y * v->pitch + (ptrdiff_t)x * bpp;
         if (p[0] != (uint8_t)x || p[1] != (uint8_t)y || p[2] != 0x5a)
            recread_bad++;
      }
   return true;
}

static const record_driver_t recread_rec_driver = {
   NULL, NULL, recread_push_video, reclane_push_audio, NULL, "recread"
};

static void record_reader_once(thread_video_t *thr, bool bgrx)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   recording_state_t    *rec_st   = recording_state_get_ptr();
   const char *mode               = bgrx ? "BGRX" : "BGR24";
   size_t      size               = (size_t)RECREAD_W * RECREAD_H * 4;
   recording_state_t saved;
   uint8_t *saved_buf;
   uint8_t *buf;
   unsigned i;

   if (!(buf = (uint8_t*)malloc(size)))
      return;
   memset(buf, 0, size);
   retro_atomic_store_release_int(&recread_bgrx_reads, 0);
   retro_atomic_store_release_int(&recread_bgr24_reads, 0);
   recread_pushes = 0;
   recread_bad    = 0;

   saved                       = *rec_st;
   saved_buf                   = video_st->record_gpu_buffer;
   rec_st->driver              = &recread_rec_driver;
   rec_st->gpu_dims            = VIDEO_SCALE_PACK(RECREAD_W, RECREAD_H);
   rec_st->gpu_bgrx            = bgrx;
   rec_st->enable              = true;
   video_st->record_gpu_buffer = buf;
   rec_st->data                = (void*)&recread_rec_driver;

   /* The wrapper's readback starts a few frames behind */
   for (i = 0; i < 64 && !recread_pushes; i++)
      run_frames(1);

   rec_st->data                = NULL;
   video_thread_wait_idle();
   video_thread_record_stop(thr);
   *rec_st                     = saved;
   video_st->record_gpu_buffer = saved_buf;

   CHECK(recread_pushes > 0, "record reader lane (%s): no frame recorded",
         mode);
   if (recread_pushes)
   {
      int pitch = bgrx ? RECREAD_W * 4 : -(RECREAD_W * 3);
      CHECK(recread_pitch == pitch,
            "record reader lane (%s): rows of %d bytes, not %d", mode,
            recread_pitch, pitch);
      CHECK(!recread_bad,
            "record reader lane (%s): %u pixels not as the driver read "
            "them", mode, recread_bad);
      CHECK(recread_data < buf || recread_data >= buf + size,
            "record reader lane (%s): the frame went through the main "
            "thread's buffer rather than straight from the read", mode);
   }
   CHECK(retro_atomic_load_acquire_int(bgrx
            ? &recread_bgr24_reads : &recread_bgrx_reads) == 0,
         "record reader lane (%s): the driver was asked for the other "
         "format", mode);
   free(buf);
}

static void lane_record_reader(void)
{
   unsigned had = failures;
   video_driver_state_t *video_st = video_state_get_ptr();
   thread_video_t *thr;
   bool menu_was_up;

   set_threaded_via_setting(true);
   run_frames(4);
   expect_wrapper(true, "record reader lane");
   /* Frames the core makes are recorded; the menu's dupes are not */
   if ((menu_was_up = menu_is_up()))
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   if (!(thr = (thread_video_t*)video_st->data))
      return;

   CHECK(!video_thread_record_has_reader(thr) || real_driver(),
         "record reader lane: the null driver reports a reader");

   video_thread_wait_idle();
   recread_inner                 = thr->driver;
   recread_driver                = *thr->driver;
   recread_driver.viewport_info  = recread_viewport_info;
   recread_driver.poke_interface = recread_get_poke;
   set_driver(thr, &recread_driver);
   set_poke_from(thr, &recread_driver);

   CHECK(video_thread_record_has_reader(thr),
         "record reader lane: a poke with record_read is not a reader");
   record_reader_once(thr, true);
   record_reader_once(thr, false);

   set_driver(thr, recread_inner);
   set_poke_from(thr, recread_inner);
   run_frames(2);
   if (menu_was_up)
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   set_threaded_via_setting(false);
   run_frames(2);

   if (failures == had)
      fprintf(stderr, "[pass] gpu record reader lane\n");
}

/* ------------------------------------------------------------------ */
/* Lane: Record Game Only keeps the on-screen messages out            */
/*   A GPU recording's read (read_viewport_bgrx) with Record Game     */
/*   Only on takes the core's image before anything is drawn over it. */
/*   With the content paused, so the image holds still, a message is  */
/*   put on screen: an ordinary read must change with it - else there */
/*   is nothing for the lane to keep out - and a game-only read must  */
/*   not. The SDL drivers read back the core's frame alone whatever   */
/*   the setting, so there only the game-only read is held to it.     */
/* ------------------------------------------------------------------ */

static bool game_only_read(uint8_t *buf, bool game_only)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   bool bottom_up                 = true;
   config_get_ptr()->bools.video_record_game_only = game_only;
   return video_st->current_video->read_viewport_bgrx(video_st->data, buf,
         false, &bottom_up);
}

static void lane_record_game_only(bool full_read_has_ui)
{
   struct video_viewport vp;
   unsigned had                   = failures;
   settings_t *settings           = config_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   recording_state_t *rec_st      = recording_state_get_ptr();
   bool saved_game_only           = settings->bools.video_record_game_only;
   bool saved_enable              = rec_st->enable;
   static const char msg[]        = "RECORD GAME ONLY LANE MESSAGE";
   uint8_t *buf                   = NULL;
   size_t len;
   bool ok;

   set_threaded_via_setting(false);
   run_frames(4);
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   CHECK(!menu_is_up(), "game-only lane: menu still up");
   if (!video_st->current_video->read_viewport_bgrx)
   {
      fprintf(stderr, "[skip] game-only lane (no 32-bit readback)\n");
      return;
   }
   run_frames(2);
   command_event(CMD_EVENT_PAUSE, NULL);
   run_frames(2);

   memset(&vp, 0, sizeof(vp));
   video_driver_get_viewport_info(&vp);
   len = VIDEO_SCALE_AREA(vp.dims) * 4;
   CHECK(len > 0, "game-only lane: no viewport");
   if (len && (buf = (uint8_t*)malloc(len * 4)))
   {
      uint8_t *g0 = buf, *n0 = buf + len, *g1 = buf + 2 * len,
              *n1 = buf + 3 * len;
      rec_st->enable = true;
      ok             = game_only_read(g0, true) && game_only_read(n0, false);
      runloop_msg_queue_push(msg, sizeof(msg) - 1, 1, 600, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      run_frames(30);
      ok             = ok && game_only_read(g1, true) && game_only_read(n1, false);
      rec_st->enable = saved_enable;
      settings->bools.video_record_game_only = saved_game_only;

      CHECK(ok, "game-only lane: a read failed");
      if (ok)
      {
         if (full_read_has_ui)
            CHECK(memcmp(n0, n1, len) != 0,
                  "game-only lane: the message never reached an ordinary read,"
                  " so there is nothing to keep out");
         CHECK(memcmp(g0, g1, len) == 0,
               "game-only lane: a game-only read holds the message");
      }
      free(buf);
   }

   command_event(CMD_EVENT_UNPAUSE, NULL);
   run_frames(2);
   if (failures == had)
      fprintf(stderr, "[pass] record game only lane\n");
}

static void lane_gpu_readback(void)
{
   unsigned had = failures;

   readback_once(false);
   readback_once(true);
   set_threaded_via_setting(false);
   run_frames(2);

   if (failures == had)
      fprintf(stderr, "[pass] gpu readback lane\n");
}

/* ------------------------------------------------------------------ */
/* Lane: a hardware core's image is not drawn from after it is gone   */
/*   A Vulkan hardware core hands the frontend an image of its own    */
/*   and destroys it in context_destroy. A staged content close keeps */
/*   the old drivers presenting dupes of the last frame until the new */
/*   session's are built, so unless the driver is made to let go of   */
/*   the image first, every one of those dupes samples a destroyed    */
/*   VkImageView: reported as a freeze and a fault inside the GPU     */
/*   driver on closing content with LRPS2 (libretro/ps2 #171).        */
/*   The same for a core that withdraws its image in mid-session with */
/*   set_image(NULL) and then destroys it: under the wrapper the      */
/*   withdrawal reached only the ring slot the core fills next, and   */
/*   the dupes that followed re-read the slot last presented.         */
/*                                                                    */
/*   Runs with the harness core in its hardware mode, which is a      */
/*   separate invocation (HARNESS_CORE_HW_VULKAN=1) with this lane    */
/*   alone: the other lanes are about software frames. Unthreaded the */
/*   proof is the run itself - the driver's pointer to the core's     */
/*   image goes into the unloaded core, and the validation layer sees */
/*   the destroyed view. Under the wrapper the ring's installs are    */
/*   also counted: none may name an image after the core destroyed    */
/*   it.                                                              */
/* ------------------------------------------------------------------ */

static video_poke_interface_t        hwlane_poke;
static const video_poke_interface_t *hwlane_inner_poke;
static retro_hw_context_reset_t      hwlane_core_destroy;
static retro_atomic_int_t hwlane_gone;          /* the core's image is destroyed */
static retro_atomic_int_t hwlane_installs;      /* installs that named an image */
static retro_atomic_int_t hwlane_installs_gone; /* ... after it was destroyed */

static bool hwlane_install(void *data, const void *image,
      const void *semaphores, unsigned num_semaphores,
      unsigned src_queue_family, const void *cmd, unsigned num_cmd)
{
   if (image)
   {
      retro_atomic_fetch_add_int(&hwlane_installs, 1);
      if (retro_atomic_load_acquire_int(&hwlane_gone))
         retro_atomic_fetch_add_int(&hwlane_installs_gone, 1);
   }
   return hwlane_inner_poke->hw_ring_install(data, image, semaphores,
         num_semaphores, src_queue_family, cmd, num_cmd);
}

/* In front of the core's context_destroy, to know when the image went. */
static void hwlane_context_destroy(void)
{
   hwlane_core_destroy();
   retro_atomic_store_release_int(&hwlane_gone, 1);
}

static unsigned hwlane_core_call(const char *name)
{
   dylib_t lib = runloop_state_get_ptr()->lib_handle;
   unsigned (*fn)(void) = lib
      ? (unsigned (*)(void))dylib_proc(lib, name) : NULL;
   return fn ? fn() : 0;
}

/* The frame loop as the frontend's main loop runs it: the staged close
 * and load are tasks. */
static void hwlane_pump(unsigned n)
{
   unsigned i;
   for (i = 0; i < n; i++)
   {
      runloop_iterate();
      task_queue_check();
   }
}

/* Counts the wrapper's installs from here on. The wrapper is idle
 * while its table is swapped. */
static bool hwlane_probe(void)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   thread_video_t *thr            = (thread_video_t*)video_st->data;
   if (     !video_st->thread_wrapper_active
         || !thr || !thr->poke || !thr->poke->hw_ring_install)
      return false;
   video_thread_wait_idle();
   hwlane_inner_poke           = thr->poke;
   hwlane_poke                 = *thr->poke;
   hwlane_poke.hw_ring_install = hwlane_install;
   thr->poke                   = &hwlane_poke;
   return true;
}

/* Closes content from the menu, staged, and runs the frame loop until
 * the dummy core's drivers are up. Returns the frames the old drivers
 * presented after the core destroyed its image. */
static unsigned hwlane_close(const char *when)
{
   struct menu_state *menu_st     = menu_state_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   unsigned i;
   unsigned after = 0;

   if (!menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   hwlane_pump(2);
   CHECK(menu_is_up(), "%s: menu did not open", when);

   hwlane_core_destroy = video_st->hw_render.context_destroy;
   CHECK(hwlane_core_destroy != NULL, "%s: the core has no context_destroy", when);
   if (!hwlane_core_destroy)
      return 0;
   retro_atomic_store_release_int(&hwlane_gone, 0);
   retro_atomic_store_release_int(&hwlane_installs_gone, 0);
   video_st->hw_render.context_destroy = hwlane_context_destroy;

   menu_st->flags |= MENU_ST_FLAG_PENDING_CLOSE_CONTENT;
   hwlane_pump(1);
   CHECK(runloop_is_content_switching(),
         "%s: the close did not start a staged load", when);
   for (i = 0; i < 400 && runloop_is_content_switching(); i++)
   {
      if (retro_atomic_load_acquire_int(&hwlane_gone))
         after++;
      hwlane_pump(1);
   }
   CHECK(!runloop_is_content_switching(), "%s: the close did not finish", when);
   CHECK(retro_atomic_load_acquire_int(&hwlane_gone),
         "%s: the core's context_destroy never ran", when);
   CHECK(runloop_state_get_ptr()->current_core_type == CORE_TYPE_DUMMY,
         "%s: not on the dummy core after the close", when);
   /* The window the lane is about. If the close ever stops presenting
    * between the core's teardown and the new drivers, this lane no
    * longer tests anything and should say so. */
   CHECK(after >= 1,
         "%s: no frame presented between the core's teardown and the new drivers",
         when);
   hwlane_pump(5);
   return after;
}

static void lane_hw_image_lifetime(const char *core_path)
{
   unsigned had = failures;
   unsigned after_plain, after_wrapped;
   int before;
   unsigned i;

   if (!hwlane_core_call("harness_core_hw_active"))
   {
      CHECK(0, "hw image lifetime: the harness core is not a hardware core"
            " (needs the vulkan driver)");
      return;
   }

   /* Unthreaded: frames in game, then the close. */
   expect_wrapper(false, "hw unthreaded");
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   hwlane_pump(20);
   after_plain = hwlane_close("hw unthreaded close");

   /* The core again, under the wrapper this time. */
   CHECK(task_push_load_contentless_core_from_menu(core_path),
         "hw image lifetime: the core was not started again");
   for (i = 0; i < 400 && runloop_is_content_switching(); i++)
      hwlane_pump(1);
   CHECK(!runloop_is_content_switching(),
         "hw image lifetime: the core's restart did not finish");
   hwlane_pump(5);
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   set_threaded_via_setting(true);
   hwlane_pump(20);
   expect_wrapper(true, "hw under the wrapper");
   CHECK(hwlane_core_call("harness_core_hw_active"),
         "hw image lifetime: the core did not come back as a hardware core");
   if (!hwlane_probe())
   {
      CHECK(0, "hw image lifetime: the wrapper has no hardware ring for this driver");
      return;
   }

   retro_atomic_store_release_int(&hwlane_gone, 0);
   retro_atomic_store_release_int(&hwlane_installs, 0);
   retro_atomic_store_release_int(&hwlane_installs_gone, 0);
   hwlane_pump(20);
   video_thread_wait_idle();
   before = retro_atomic_load_acquire_int(&hwlane_installs);
   CHECK(before > 0, "hw image lifetime: no hardware frame reached the driver");

   /* The core withdraws its image and destroys it; dupes follow. */
   CHECK(hwlane_core_call("harness_core_hw_withdraw"),
         "hw image lifetime: the core did not withdraw its image");
   retro_atomic_store_release_int(&hwlane_gone, 1);
   hwlane_pump(20);
   video_thread_wait_idle();
   CHECK(retro_atomic_load_acquire_int(&hwlane_installs_gone) == 0,
         "hw image lifetime: the core withdrew and destroyed its image, and"
         " the ring handed it to the driver %d more time(s)",
         retro_atomic_load_acquire_int(&hwlane_installs_gone));

   /* And presents again: the ring is not left deaf. */
   retro_atomic_store_release_int(&hwlane_gone, 0);
   before = retro_atomic_load_acquire_int(&hwlane_installs);
   CHECK(hwlane_core_call("harness_core_hw_restore"),
         "hw image lifetime: the core did not restore its image");
   hwlane_pump(20);
   video_thread_wait_idle();
   CHECK(retro_atomic_load_acquire_int(&hwlane_installs) > before,
         "hw image lifetime: no hardware frame reached the driver after the"
         " core presented again");

   /* The close, under the wrapper. */
   after_wrapped = hwlane_close("hw wrapped close");
   CHECK(retro_atomic_load_acquire_int(&hwlane_installs_gone) == 0,
         "hw image lifetime: the core destroyed its image at the close, and"
         " the ring handed it to the driver %d more time(s)",
         retro_atomic_load_acquire_int(&hwlane_installs_gone));

   set_threaded_via_setting(false);
   hwlane_pump(5);

   if (failures == had)
      fprintf(stderr, "[pass] hw image lifetime lane (%u + %u frames presented"
            " after the core's teardown)\n", after_plain, after_wrapped);
}

/* ------------------------------------------------------------------ */
/* Lane: a hardware core's own images, behind a semaphore per frame   */
/*   The harness core in its ring mode renders into an image per sync */
/*   index, which the video thread reads a frame or two later, behind */
/*   a semaphore per frame, and dupes every third: a dupe draws the   */
/*   last image again. Then, behind a present slower than a frame,    */
/*   with a software frame every third and then with hardware frames  */
/*   only, hardware frames the video thread never drew are replaced   */
/*   or taken back, and the core comes back to slots still queued.    */
/*   The core signals a slot's semaphore again on every other use and */
/*   recycles it on the rest, so the validation layer reports one the */
/*   frontend never waited, and one whose wait is still on the queue. */
/*   A slot queued twice behind one signal hangs the queue. The core  */
/*   must also keep running ahead of the slow present rather than     */
/*   wait for it, and never be handed the image the video thread is   */
/*   drawing from.                                                    */
/* ------------------------------------------------------------------ */

#define HWRING_FRAMES      600
#define HWRING_SLOW_FRAMES 120

static video_driver_t        hwring_driver;
static const video_driver_t *hwring_inner;
static unsigned              hwring_hw_drawn;
static unsigned              hwring_present_ms;
static video_poke_interface_t        hwring_poke;
static const video_poke_interface_t *hwring_inner_poke;
/* The core's image the ring installed for the next frame call (the
 * video thread's own), and the one inside the frame call now. */
static uint64_t              hwring_installed;
static retro_atomic_ptr_t    hwring_drawing;
/* Images the core cleared while the video thread was drawing them. */
static unsigned              hwring_overlaps;

static bool hwring_install(void *data, const void *image,
      const void *semaphores, unsigned num_semaphores,
      unsigned src_queue_family, const void *cmd, unsigned num_cmd)
{
   hwring_installed = 0;
#ifdef HAVE_VULKAN
   if (image)
      memcpy(&hwring_installed,
            &((const struct retro_vulkan_image*)image)->create_info.image,
            sizeof(VkImage));
#endif
   return hwring_inner_poke->hw_ring_install(data, image, semaphores,
         num_semaphores, src_queue_family, cmd, num_cmd);
}

static bool hwring_frame(void *data, const void *frame,
      unsigned dims, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   bool ret;
   if (frame == RETRO_HW_FRAME_BUFFER_VALID)
      hwring_hw_drawn++;
   retro_atomic_store_release_ptr(&hwring_drawing,
         (void*)(uintptr_t)hwring_installed);
   hwring_installed = 0;
   retro_sleep(hwring_present_ms);
   ret = hwring_inner->frame(data, frame, dims, frame_count,
         pitch, msg, video_info);
   retro_atomic_store_release_ptr(&hwring_drawing, NULL);
   return ret;
}

/* The core's thread, before it clears an image. */
static void hwring_on_clear(uint64_t image)
{
   if (     image
         && (void*)(uintptr_t)image
            == retro_atomic_load_acquire_ptr(&hwring_drawing))
      hwring_overlaps++;
}

#ifdef HAVE_VULKAN
/* The Vulkan context the driver is on now; the context driver's data
 * is replaced by a reinit, so it is fetched again after each. */
static vulkan_context_t *hwcached_context(void)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   if (!video_st->context_data || !video_st->current_video_context.get_context_data)
      return NULL;
   return (vulkan_context_t*)video_st->current_video_context.get_context_data(
         video_st->context_data);
}
#endif

/* A core that caches its context has the instance reused on a reinit,
 * never re-probed: what the instance was created with must come back
 * with it. The ICD here may not offer the colorspace extension, so the
 * flag is set by hand before the reinit; the lane holds the carry-over,
 * not the probe. */
static void lane_hw_cached_context(void)
{
#ifdef HAVE_VULKAN
   unsigned had = failures;
   unsigned i;
   vulkan_context_t *ctx;

   if (!hwlane_core_call("harness_core_hw_cached"))
   {
      CHECK(0, "hw cached context: the harness core does not cache its context");
      return;
   }
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   hwlane_pump(5);
   CHECK(hwlane_core_call("harness_core_hw_active"),
         "hw cached context: the core has no image to hand over");
   ctx = hwcached_context();
   if (!ctx)
   {
      CHECK(0, "hw cached context: no Vulkan context data from the context driver");
      return;
   }
   for (i = 0; i < 3; i++)
   {
      int flags       = DRIVER_VIDEO_MASK | DRIVER_INPUT_MASK;
      unsigned resets = hwlane_core_call("harness_core_hw_resets");
      ctx->flags     |= VK_CTX_FLAG_HDR_SUPPORT;
      command_event(CMD_EVENT_REINIT, &flags);
      hwlane_pump(5);
      CHECK(hwlane_core_call("harness_core_hw_resets") == resets,
            "hw cached context: reinit %u called context_reset on a cached context", i + 1);
      ctx = hwcached_context();
      CHECK(ctx != NULL, "hw cached context: no context data after reinit %u", i + 1);
      if (!ctx)
         return;
      CHECK(ctx->flags & VK_CTX_FLAG_HDR_SUPPORT,
            "hw cached context: HDR support lost with the reused instance, reinit %u", i + 1);
      CHECK(hwlane_core_call("harness_core_hw_active"),
            "hw cached context: the core's image did not survive reinit %u", i + 1);
   }
   if (failures == had)
      fprintf(stderr, "[pass] hw cached context lane (HDR support kept over 3 reinits)\n");
#endif
}

#ifdef HAVE_VULKAN
/* The head of struct vk_texture, as gfx/drivers/vulkan.c lays it out */
typedef struct
{
   VkDeviceSize memory_size;
   void *mapped;
   VkImage image;
} planar_vk_texture_head_t;

/* What Vulkan texture @id holds, as R,G,B,A bytes; direct video only,
 * the queue taken under its lock and waited idle - a test's readback */
static bool planar_vk_readback(uintptr_t id, unsigned w, unsigned h,
      uint32_t *out)
{
   const planar_vk_texture_head_t *tex = (const planar_vk_texture_head_t*)id;
   vulkan_context_t *ctx = hwcached_context();
   VkBufferCreateInfo binfo;
   VkMemoryRequirements req;
   VkMemoryAllocateInfo alloc;
   VkCommandPoolCreateInfo pinfo;
   VkCommandBufferAllocateInfo cinfo;
   VkCommandBufferBeginInfo begin;
   VkImageMemoryBarrier bar;
   VkBufferImageCopy region;
   VkSubmitInfo submit;
   VkBuffer buf          = VK_NULL_HANDLE;
   VkDeviceMemory mem    = VK_NULL_HANDLE;
   VkCommandPool pool    = VK_NULL_HANDLE;
   VkCommandBuffer cmd   = VK_NULL_HANDLE;
   void *ptr             = NULL;
   bool ok               = false;
   uint32_t i;
   if (!ctx || !tex || !tex->image)
      return false;
   memset(&binfo, 0, sizeof(binfo));
   binfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   binfo.size        = (VkDeviceSize)w * h * 4;
   binfo.usage       = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
   binfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
   if (vkCreateBuffer(ctx->device, &binfo, NULL, &buf) != VK_SUCCESS)
      return false;
   vkGetBufferMemoryRequirements(ctx->device, buf, &req);
   memset(&alloc, 0, sizeof(alloc));
   alloc.sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   alloc.allocationSize = req.size;
   for (i = 0; i < ctx->memory_properties.memoryTypeCount; i++)
      if (     (req.memoryTypeBits & (1u << i))
            && (ctx->memory_properties.memoryTypes[i].propertyFlags
               & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                  | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
               == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                  | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
         break;
   alloc.memoryTypeIndex = i;
   memset(&pinfo, 0, sizeof(pinfo));
   pinfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   pinfo.queueFamilyIndex = ctx->graphics_queue_index;
   memset(&cinfo, 0, sizeof(cinfo));
   cinfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   cinfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   cinfo.commandBufferCount = 1;
   if (     i == ctx->memory_properties.memoryTypeCount
         || vkAllocateMemory(ctx->device, &alloc, NULL, &mem) != VK_SUCCESS
         || vkBindBufferMemory(ctx->device, buf, mem, 0) != VK_SUCCESS
         || vkCreateCommandPool(ctx->device, &pinfo, NULL, &pool) != VK_SUCCESS)
      goto end;
   cinfo.commandPool = pool;
   if (vkAllocateCommandBuffers(ctx->device, &cinfo, &cmd) != VK_SUCCESS)
      goto end;
   memset(&begin, 0, sizeof(begin));
   begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   vkBeginCommandBuffer(cmd, &begin);
   memset(&bar, 0, sizeof(bar));
   bar.sType                       = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   bar.srcAccessMask               = VK_ACCESS_SHADER_WRITE_BIT
                                   | VK_ACCESS_SHADER_READ_BIT;
   bar.dstAccessMask               = VK_ACCESS_TRANSFER_READ_BIT;
   bar.oldLayout                   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   bar.newLayout                   = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   bar.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
   bar.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
   bar.image                       = tex->image;
   bar.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   bar.subresourceRange.levelCount = 1;
   bar.subresourceRange.layerCount = 1;
   vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &bar);
   memset(&region, 0, sizeof(region));
   region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   region.imageSubresource.layerCount = 1;
   region.imageExtent.width           = w;
   region.imageExtent.height          = h;
   region.imageExtent.depth           = 1;
   vkCmdCopyImageToBuffer(cmd, tex->image,
         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf, 1, &region);
   bar.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
   bar.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
   bar.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   bar.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &bar);
   vkEndCommandBuffer(cmd);
   memset(&submit, 0, sizeof(submit));
   submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit.commandBufferCount = 1;
   submit.pCommandBuffers    = &cmd;
   slock_lock(ctx->queue_lock);
   ok = vkQueueSubmit(ctx->queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS
      && vkQueueWaitIdle(ctx->queue) == VK_SUCCESS;
   slock_unlock(ctx->queue_lock);
   if (ok && vkMapMemory(ctx->device, mem, 0, binfo.size, 0, &ptr)
         == VK_SUCCESS)
   {
      memcpy(out, ptr, (size_t)binfo.size);
      vkUnmapMemory(ctx->device, mem);
   }
   else
      ok = false;
end:
   if (pool)
      vkDestroyCommandPool(ctx->device, pool, NULL);
   if (buf)
      vkDestroyBuffer(ctx->device, buf, NULL);
   if (mem)
      vkFreeMemory(ctx->device, mem, NULL);
   return ok;
}
#endif

/* Within @tol in each channel */
static bool planar_close(uint32_t a, uint32_t b, unsigned tol)
{
   unsigned k;
   for (k = 0; k < 32; k += 8)
   {
      int d = (int)((a >> k) & 0xff) - (int)((b >> k) & 0xff);
      if (d > (int)tol || d < -(int)tol)
         return false;
   }
   return true;
}

/* Planar surfaces on the real driver, direct and threaded, I420 from
 * the caller's padded planes and NV12 from a slot, at an odd size. A
 * driver that converts on the GPU (Vulkan) is checked against the CPU
 * conversion pixel for pixel; one that does not is given the CPU's. */
static void lane_surface_planar(void)
{
   enum { PW = 63, PH = 47, PS = 72 };
   static uint8_t y[PS * PH], u[PS * 24], v[PS * 24];
   static uint32_t want[PW * PH], got[PW * PH];
   unsigned had   = failures;
   const char *drv = getenv("HARNESS_VIDEO_DRIVER");
   bool gpu       = false, checked = false;
   unsigned pass, k, i, j, tries;

   if (!real_driver())
   {
      fprintf(stderr, "[skip] surface planar lane (no real driver)\n");
      return;
   }
   for (j = 0; j < PH; j++)
      for (i = 0; i < PW; i++)
         y[j * PS + i] = (uint8_t)(16 + ((i * 3 + j * 5) % 220));
   for (j = 0; j < 24; j++)
      for (i = 0; i < 32; i++)
      {
         u[j * PS + i] = (uint8_t)(40 + ((i * 7 + j) % 180));
         v[j * PS + i] = (uint8_t)(220 - ((i + j * 9) % 180));
      }

   for (pass = 0; pass < 2; pass++)
   {
      bool threaded = pass == 1;
      unsigned fmt;
      set_threaded_via_setting(threaded);
      run_frames(3);
      expect_wrapper(threaded, "surface planar lane");
      if (!threaded)
         gpu = video_driver_supports_texture_format(
               TEXTURE_GPU_FORMAT_YUV420);
      for (fmt = 0; fmt < 2; fmt++)
      {
         bool nv12        = fmt == 1;
         unsigned flags   = nv12 ? IMAGE_YUV_FLAG_BT709 : 0;
         uintptr_t first  = 0;
         gfx_surface_t *s = gfx_surface_new(VIDEO_SCALE_PACK(PW, PH), 2,
               nv12 ? IMAGE_PIXFMT_NV12 : IMAGE_PIXFMT_I420,
               TEXTURE_FILTER_LINEAR, NULL, NULL);
         CHECK(s != NULL, "surface planar lane: no surface");
         if (!s)
            continue;
         gfx_surface_set_yuv(s, flags);
         for (k = 0; k < 4; k++)
         {
            enum gfx_surface_submit_result r = GFX_SURFACE_SUBMIT_FAILED;
            unsigned slot = k & 1;
            for (tries = 0; tries < 16 && (s->inflight
                     || !gfx_surface_slot_writable(s, slot)); tries++)
               run_frames(1);
            if (nv12)
            {
               /* A slot: the luma rows, then Cb,Cr pairs */
               uint8_t *f = (uint8_t*)s->slots[slot];
               for (j = 0; j < PH; j++)
                  memcpy(f + j * PW, y + j * PS, PW);
               f += PW * PH;
               for (j = 0; j < 24; j++)
                  for (i = 0; i < 32; i++)
                  {
                     f[(j * 32 + i) * 2]     = u[j * PS + i];
                     f[(j * 32 + i) * 2 + 1] = v[j * PS + i];
                  }
            }
            for (tries = 0; tries < 16; tries++)
            {
               if (nv12)
                  r = gfx_surface_submit(s, slot, false);
               else
               {
                  gfx_surface_planes_t p;
                  p.planes[0]   = y;
                  p.planes[1]   = u;
                  p.planes[2]   = v;
                  p.strides[0]  = p.strides[1] = p.strides[2] = PS;
                  p.chroma_step = 1;
                  r = gfx_surface_submit_planes(s, &p, false);
               }
               if (r != GFX_SURFACE_SUBMIT_DROPPED
                     && r != GFX_SURFACE_SUBMIT_BUSY)
                  break;
               run_frames(1);
            }
            CHECK(r == GFX_SURFACE_SUBMIT_DONE
                  || r == GFX_SURFACE_SUBMIT_QUEUED,
                  "surface planar lane: %s %s submit %u returned %d",
                  threaded ? "threaded" : "direct", nv12 ? "NV12" : "I420",
                  k, r);
            for (tries = 0; tries < 16 && s->inflight; tries++)
               run_frames(1);
            if (k == 0)
               first = s->handle;
            else if (s->can_update)
               CHECK(s->handle == first, "surface planar lane: frame %u "
                     "replaced the texture", k);
         }
         CHECK(s->handle != 0, "surface planar lane: no texture");
         CHECK(gpu == !s->rgb, "surface planar lane: %s converted on "
               "the %s", drv ? drv : "?", s->rgb ? "CPU" : "GPU");
#ifdef HAVE_VULKAN
         if (!threaded && gpu && drv && !strcmp(drv, "vulkan"))
         {
            unsigned bad = PW * PH;
            image_yuv_420_to_rgb32(want, PW, y, PS, u, PS, v, PS, 1,
                  PW, PH, flags | IMAGE_YUV_FLAG_RGBA);
            CHECK(planar_vk_readback(s->handle, PW, PH, got),
                  "surface planar lane: Vulkan readback failed");
            for (i = 0; i < PW * PH && bad == PW * PH; i++)
               if (!planar_close(got[i], want[i], 2))
                  bad = i;
            CHECK(bad == PW * PH, "surface planar lane: %s texel %u is "
                  "%08x, the CPU makes %08x", nv12 ? "NV12" : "I420", bad,
                  bad < PW * PH ? (unsigned)got[bad] : 0u,
                  bad < PW * PH ? (unsigned)want[bad] : 0u);
            checked = true;
         }
#endif
#if defined(HAVE_OPENGL) && defined(HAVE_GL_TEXTURE_LEND)
         /* glcore reads back R,G,B,A bytes, gl the words as BGRA */
         if (     !threaded && gpu && drv
               && (!strcmp(drv, "gl") || !strcmp(drv, "glcore")))
         {
            unsigned bad = PW * PH;
            bool core    = !strcmp(drv, "glcore");
            image_yuv_420_to_rgb32(want, PW, y, PS, u, PS, v, PS, 1,
                  PW, PH, flags | (core ? IMAGE_YUV_FLAG_RGBA : 0));
            CHECK(lend_gl_readback(drv, s->handle, false, PW, PH, got),
                  "surface planar lane: GL readback failed");
            for (i = 0; i < PW * PH && bad == PW * PH; i++)
               if (!planar_close(got[i], want[i], 2))
                  bad = i;
            CHECK(bad == PW * PH, "surface planar lane: %s %s texel %u is "
                  "%08x, the CPU makes %08x", drv, nv12 ? "NV12" : "I420",
                  bad, bad < PW * PH ? (unsigned)got[bad] : 0u,
                  bad < PW * PH ? (unsigned)want[bad] : 0u);
            checked = true;
         }
#endif
         gfx_surface_free(s);
         run_frames(2);
      }
   }
   set_threaded_via_setting(false);
   run_frames(2);
   if (drv && !strcmp(drv, "vulkan"))
      CHECK(gpu && checked, "surface planar lane: %s did not convert "
            "on the GPU", drv);
   /* Read back through the lend's GL entry points: in a GLES build
    * only that it converts */
   if (drv && (!strcmp(drv, "gl") || !strcmp(drv, "glcore")))
#if defined(HAVE_OPENGL) && defined(HAVE_GL_TEXTURE_LEND)
      CHECK(gpu && checked, "surface planar lane: %s did not convert "
            "on the GPU", drv);
#else
      CHECK(gpu, "surface planar lane: %s did not convert on the GPU",
            drv);
#endif
   /* No readback here: that it converts, and every submit landed */
   if (drv && (!strcmp(drv, "d3d11") || !strcmp(drv, "d3d12")
            || !strcmp(drv, "metal")))
      CHECK(gpu, "surface planar lane: %s did not convert on the GPU",
            drv);
   if (failures == had)
      fprintf(stderr, "[pass] surface planar lane (%s%s)\n",
            gpu ? "GPU conversion" : "CPU conversion",
            checked ? ", texels match the CPU's" : "");
}

/* ------------------------------------------------------------------ */
/* Imported frames: a producer on the driver's own device - what a     */
/* hardware decoder is - hands over NV12 images and the driver turns   */
/* them into the surface's texture on the GPU. The producer here is a  */
/* stand-in for one: it writes a linear image from the host and        */
/* signals its timeline from the host, so the contract - wait for the  */
/* producer's timeline, signal the other once the frame is read, never */
/* wait on the CPU, one texture for the stream - is held on lavapipe   */
/* under the validation layer, threaded and not, with the texels        */
/* checked against the CPU's conversion.                               */
/* ------------------------------------------------------------------ */

#ifdef HAVE_VULKAN
#include "../../../gfx/common/vulkan_video_planes.h"
typedef struct
{
   VkImage image;
   VkImageView views[2];
   VkDeviceMemory mem;
   VkSemaphore wait, done;
   VkSubresourceLayout planes[2];
   uint8_t *map;
   PFN_vkSignalSemaphoreKHR signal;
   PFN_vkGetSemaphoreCounterValueKHR value;
} import_producer_t;

static void import_release_cb(void *user, gfx_surface_t *s, unsigned slot)
{
   (void)slot;
   if (s->dropped)
      (*(unsigned*)user)++;
}

static VkSemaphore import_timeline(VkDevice dev)
{
   VkSemaphoreTypeCreateInfoKHR type;
   VkSemaphoreCreateInfo info;
   VkSemaphore sem = VK_NULL_HANDLE;
   type.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR;
   type.pNext         = NULL;
   type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE_KHR;
   type.initialValue  = 0;
   info.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
   info.pNext         = &type;
   info.flags         = 0;
   if (vkCreateSemaphore(dev, &info, NULL, &sem) != VK_SUCCESS)
      return VK_NULL_HANDLE;
   return sem;
}

/* A linear NV12/P010 image the host writes; false where the device cannot
 * have one, which is a skip rather than a failure */
static bool import_producer_new(vulkan_context_t *ctx, unsigned w,
      unsigned h, unsigned pixfmt, import_producer_t *p)
{
   VkFormatProperties fp;
   VkImageFormatProperties props;
   bool sampled = getenv("HARNESS_SAMPLED_IMPORT") != NULL;
   VkImageCreateInfo info;
   VkMemoryRequirements req;
   VkMemoryAllocateInfo alloc;
   VkImageSubresource sub;
   video_device_info_t dev;
   uint32_t i;
   VkFormat format = pixfmt == IMAGE_PIXFMT_P010
      ? VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16
      : VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;

   memset(p, 0, sizeof(*p));
   if (!video_driver_get_device(&dev))
      return false;
   vkGetPhysicalDeviceFormatProperties((VkPhysicalDevice)dev.physical_device,
         format, &fp);
   if (!(fp.linearTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT))
      return false;
   p->signal = (PFN_vkSignalSemaphoreKHR)vkGetDeviceProcAddr(ctx->device,
         "vkSignalSemaphoreKHR");
   p->value  = (PFN_vkGetSemaphoreCounterValueKHR)vkGetDeviceProcAddr(
         ctx->device, "vkGetSemaphoreCounterValueKHR");
   if (!p->signal || !p->value)
      return false;
   memset(&info, 0, sizeof(info));
   info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   info.imageType     = VK_IMAGE_TYPE_2D;
   info.format        = format;
   info.extent.width  = w;
   info.extent.height = h;
   info.extent.depth  = 1;
   info.mipLevels     = 1;
   info.arrayLayers   = 1;
   info.samples       = VK_SAMPLE_COUNT_1_BIT;
   info.tiling        = VK_IMAGE_TILING_LINEAR;
   info.usage         = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
   if (sampled)
   {
      info.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
      info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
      if (vkGetPhysicalDeviceImageFormatProperties(ctx->gpu, format, info.imageType,
               info.tiling, info.usage, info.flags, &props) != VK_SUCCESS) return false;
   }
   info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
   info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
   if (vkCreateImage(ctx->device, &info, NULL, &p->image) != VK_SUCCESS)
   {
      p->image = VK_NULL_HANDLE;
      return false;
   }
   vkGetImageMemoryRequirements(ctx->device, p->image, &req);
   memset(&alloc, 0, sizeof(alloc));
   alloc.sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   alloc.allocationSize = req.size;
   for (i = 0; i < ctx->memory_properties.memoryTypeCount; i++)
      if (     (req.memoryTypeBits & (1u << i))
            && (ctx->memory_properties.memoryTypes[i].propertyFlags
               & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                  | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
               == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                  | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
         break;
   alloc.memoryTypeIndex = i;
   if (     i == ctx->memory_properties.memoryTypeCount
         || vkAllocateMemory(ctx->device, &alloc, NULL, &p->mem) != VK_SUCCESS
         || vkBindImageMemory(ctx->device, p->image, p->mem, 0) != VK_SUCCESS
         || vkMapMemory(ctx->device, p->mem, 0, VK_WHOLE_SIZE, 0,
               (void**)&p->map) != VK_SUCCESS)
      return false;
   sub.mipLevel   = 0;
   sub.arrayLayer = 0;
   sub.aspectMask = VK_IMAGE_ASPECT_PLANE_0_BIT;
   vkGetImageSubresourceLayout(ctx->device, p->image, &sub, &p->planes[0]);
   sub.aspectMask = VK_IMAGE_ASPECT_PLANE_1_BIT;
   vkGetImageSubresourceLayout(ctx->device, p->image, &sub, &p->planes[1]);
   if (sampled && !vulkan_video_plane_views(ctx->device, p->image, pixfmt, p->views)) return false;
   p->wait = import_timeline(ctx->device);
   p->done = import_timeline(ctx->device);
   return p->wait != VK_NULL_HANDLE && p->done != VK_NULL_HANDLE;
}

static void import_producer_free(vulkan_context_t *ctx, import_producer_t *p)
{
   if (p->map)
      vkUnmapMemory(ctx->device, p->mem);
   if (p->wait)
      vkDestroySemaphore(ctx->device, p->wait, NULL);
   if (p->done)
      vkDestroySemaphore(ctx->device, p->done, NULL);
   if (p->views[0]) vkDestroyImageView(ctx->device, p->views[0], NULL);
   if (p->views[1]) vkDestroyImageView(ctx->device, p->views[1], NULL);
   if (p->image)
      vkDestroyImage(ctx->device, p->image, NULL);
   if (p->mem)
      vkFreeMemory(ctx->device, p->mem, NULL);
}

/* Frame @k into the image, and the same as tight planes for the CPU */
static void import_fill(import_producer_t *p, unsigned w, unsigned h,
      unsigned k, uint8_t *y, uint8_t *uv)
{
   unsigned i, j;
   for (j = 0; j < h; j++)
      for (i = 0; i < w; i++)
         y[j * w + i] = (uint8_t)(16 + ((i * 3 + j * 5 + k * 11) % 220));
   for (j = 0; j < h / 2; j++)
      for (i = 0; i < w / 2; i++)
      {
         uv[j * w + i * 2]     = (uint8_t)(40 + ((i * 7 + j + k * 13) % 180));
         uv[j * w + i * 2 + 1] = (uint8_t)(220 - ((i + j * 9 + k) % 180));
      }
   for (j = 0; j < h; j++)
      memcpy(p->map + p->planes[0].offset + j * p->planes[0].rowPitch,
            y + j * w, w);
   for (j = 0; j < h / 2; j++)
      memcpy(p->map + p->planes[1].offset + j * p->planes[1].rowPitch,
            uv + j * w, w);
}
#endif

static void lane_surface_import(void)
{
#ifdef HAVE_VULKAN
   enum { IW = 64, IH = 48, FRAMES = 4 };
   static uint8_t y[IW * IH], uv[IW * IH / 2];
   static uint32_t want[IW * IH], got[IW * IH];
   const char *drv = getenv("HARNESS_VIDEO_DRIVER");
   unsigned had    = failures;
   bool checked    = false;
   video_device_info_t dev;
   vulkan_context_t *ctx;
   unsigned pass;

   if (!real_driver() || !drv || strcmp(drv, "vulkan"))
      return;
   set_threaded_via_setting(false);
   run_frames(3);
   if (     !video_driver_get_device(&dev)
         || !(dev.import_formats & IMAGE_PIXFMT_NV12))
   {
      fprintf(stderr, "[skip] surface import lane (no import on this device)\n");
      return;
   }

   for (pass = 0; pass < 2; pass++)
   {
      bool threaded       = pass == 1;
      import_producer_t p;
      unsigned dropped    = 0;
      uintptr_t first     = 0;
      uint64_t value      = 0;
      unsigned layout     = VK_IMAGE_LAYOUT_PREINITIALIZED;
      gfx_surface_t *s;
      unsigned k, tries;

      set_threaded_via_setting(threaded);
      run_frames(3);
      expect_wrapper(threaded, "surface import lane");
      /* The toggle made the driver, and its device, again */
      if (!(ctx = hwcached_context()))
         break;
      CHECK(     video_driver_get_device(&dev)
            && dev.api == VIDEO_DEVICE_VULKAN
            && dev.device == (void*)ctx->device,
            "surface import lane: get_device named another device");
      if (!import_producer_new(ctx, IW, IH, IMAGE_PIXFMT_NV12, &p))
      {
         import_producer_free(ctx, &p);
         CHECK(pass == 0, "surface import lane: the producer could not "
               "be made again");
         fprintf(stderr, "[skip] surface import lane (no linear NV12)\n");
         set_threaded_via_setting(false);
         return;
      }
      s = gfx_surface_new_import(VIDEO_SCALE_PACK(IW, IH),
            TEXTURE_FILTER_LINEAR, import_release_cb, &dropped);
      CHECK(s != NULL, "surface import lane: no surface");
      for (k = 0; s && k < FRAMES; k++)
      {
         video_frame_import_t f;
         VkSemaphoreSignalInfoKHR sig;
         enum gfx_surface_submit_result r = GFX_SURFACE_SUBMIT_FAILED;
         uint64_t seen = 0;

         import_fill(&p, IW, IH, k, y, uv);
         value++;
         sig.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO_KHR;
         sig.pNext     = NULL;
         sig.semaphore = p.wait;
         sig.value     = value;
         p.signal(ctx->device, &sig);
         memset(&f, 0, sizeof(f));
         memcpy(&f.image, &p.image, sizeof(p.image));
         memcpy(&f.plane_views[0], &p.views[0], sizeof(VkImageView));
         memcpy(&f.plane_views[1], &p.views[1], sizeof(VkImageView));
         memcpy(&f.wait, &p.wait, sizeof(p.wait));
         memcpy(&f.done, &p.done, sizeof(p.done));
         f.wait_value = value;
         f.done_value = value;
         f.pixfmt     = IMAGE_PIXFMT_NV12;
         f.dims       = VIDEO_SCALE_PACK(IW, IH);
         f.layout     = layout;
         f.yuv        = IMAGE_YUV_FLAG_BT709;
         for (tries = 0; tries < 16; tries++)
         {
            r = gfx_surface_submit_import(s, &f);
            if (r != GFX_SURFACE_SUBMIT_BUSY && r != GFX_SURFACE_SUBMIT_DROPPED)
               break;
            run_frames(1);
         }
         CHECK(r == GFX_SURFACE_SUBMIT_DONE || r == GFX_SURFACE_SUBMIT_QUEUED,
               "surface import lane: %s frame %u returned %d",
               threaded ? "threaded" : "direct", k, r);
         CHECK(threaded == (r == GFX_SURFACE_SUBMIT_QUEUED),
               "surface import lane: %s frame %u came back %d",
               threaded ? "threaded" : "direct", k, r);
         for (tries = 0; tries < 64 && s->inflight; tries++)
            run_frames(1);
         /* The producer waits for its frame, the CPU never does */
         for (tries = 0; tries < 200; tries++)
         {
            p.value(ctx->device, p.done, &seen);
            if (seen >= value)
               break;
            run_frames(1);
         }
         CHECK(seen >= value, "surface import lane: %s frame %u never "
               "released (done at %llu)", threaded ? "threaded" : "direct",
               k, (unsigned long long)seen);
         layout = VK_IMAGE_LAYOUT_GENERAL;
         if (k == 0)
            first = s->handle;
         else
            CHECK(s->handle == first, "surface import lane: frame %u "
                  "replaced the texture", k);
      }
      CHECK(!dropped, "surface import lane: %u frames not taken", dropped);
      CHECK(s && s->handle, "surface import lane: no texture");
      if (s && s->handle && !threaded)
      {
         unsigned i, bad = IW * IH;
         image_yuv_420_to_rgb32(want, IW, y, IW, uv, IW, uv + 1, IW, 2,
               IW, IH, IMAGE_YUV_FLAG_BT709 | IMAGE_YUV_FLAG_RGBA);
         CHECK(planar_vk_readback(s->handle, IW, IH, got),
               "surface import lane: readback failed");
         for (i = 0; i < IW * IH && bad == IW * IH; i++)
            if (!planar_close(got[i], want[i], 2))
               bad = i;
         CHECK(bad == IW * IH, "surface import lane: texel %u is %08x, "
               "the CPU makes %08x", bad,
               bad < IW * IH ? (unsigned)got[bad] : 0u,
               bad < IW * IH ? (unsigned)want[bad] : 0u);
         checked = true;
      }
      gfx_surface_free(s);
      run_frames(2);
      import_producer_free(ctx, &p);
   }
   set_threaded_via_setting(false);
   run_frames(2);
   if (failures == had)
      fprintf(stderr, "[pass] surface import lane (%s)\n",
            checked ? "texels match the CPU's" : "not read back");
#endif
}

#ifdef HAVE_VULKAN
static retro_atomic_int_t movie_gpu_abandons;
static void movie_gpu_abandon(void *owner, uint64_t done, uint64_t value)
{
   (void)owner; (void)done; (void)value;
   retro_atomic_fetch_add_int(&movie_gpu_abandons, 1);
}

static void movie_gpu_read(uint32_t expected, bool threaded, unsigned index)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   struct video_viewport vp;
   uint8_t *pixels;
   unsigned w, h, i, j;
   bool ok;
   memset(&vp, 0, sizeof(vp));
   video_driver_get_viewport_info(&vp);
   w = VIDEO_SCALE_W(vp.dims); h = VIDEO_SCALE_H(vp.dims);
   CHECK(w && h && video_st->current_video->read_viewport,
         "movie GPU: no viewport reader");
   if (!w || !h || !video_st->current_video->read_viewport) return;
   pixels = (uint8_t*)malloc((size_t)w * h * 3);
   CHECK(pixels != NULL, "movie GPU: readback allocation");
   if (!pixels) return;
   ok = video_st->current_video->read_viewport(video_st->data, pixels, false);
   CHECK(ok, "movie GPU: %s frame %u readback", threaded ? "threaded" : "direct", index);
   for (j = 1; ok && j <= 3; j++)
      for (i = 1; i <= 3; i++)
      {
         size_t off = ((size_t)(j * h / 4) * w + i * w / 4) * 3;
         uint32_t pixel = pixels[off + 2] | ((uint32_t)pixels[off + 1] << 8)
            | ((uint32_t)pixels[off] << 16) | UINT32_C(0xff000000);
         CHECK(planar_close(pixel, expected, 2),
               "movie GPU: %s frame %u pixel %u,%u got %08x expected %08x",
               threaded ? "threaded" : "direct", index, i, j,
               (unsigned)pixel, (unsigned)expected);
      }
   free(pixels);
}
#endif

static void lane_movie_gpu(void)
{
#ifdef HAVE_VULKAN
   enum { MAX_W = 96, MAX_H = 64, FRAMES = 10 };
   static uint8_t y[MAX_W * MAX_H], uv[MAX_W * MAX_H / 2];
   static uint32_t expected[MAX_W * MAX_H];
   const char *driver = getenv("HARNESS_VIDEO_DRIVER");
   unsigned had = failures, pass, k, checked = 0;
   bool menu_was_up = menu_is_up();
   if (!real_driver() || !driver || strcmp(driver, "vulkan")) return;
   if (menu_was_up) command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   for (pass = 0; pass < 2; pass++)
   {
      bool threaded = pass != 0;
      vulkan_context_t *ctx;
      video_device_info_t dev;
      set_threaded_via_setting(threaded);
      run_frames(3);
      if (threaded) video_thread_wait_idle();
      ctx = hwcached_context();
      if (!ctx || !video_driver_get_device(&dev)
            || !(dev.import_formats & IMAGE_PIXFMT_NV12)
            || !video_driver_movie_refresh(NULL, NULL))
      {
         CHECK(false, "movie GPU: missing movie import capability");
         break;
      }
#ifdef HAVE_GFX_INSTRUMENT
      gfx_instrument_reset();
#endif
      for (k = 0; k < FRAMES; k++)
      {
         import_producer_t producer;
         video_hw_frame_t hw;
         VkSemaphoreSignalInfoKHR signal;
         unsigned pixfmt = k & 1 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12;
         unsigned w = k < 8 ? 64 : MAX_W, h = k < 8 ? 48 : MAX_H;
         unsigned flags = (k & 2 ? IMAGE_YUV_FLAG_BT709 : 0)
            | (k & 4 ? IMAGE_YUV_FLAG_FULL_RANGE : 0);
         unsigned row, col, plane, bps = pixfmt == IMAGE_PIXFMT_P010 ? 2 : 1;
         unsigned tries;
         uint64_t done = 0;
         if (!(dev.import_formats & pixfmt))
         {
            fprintf(stderr, "[skip] movie GPU format %u unavailable\n", pixfmt);
            continue;
         }
         if (!import_producer_new(ctx, w, h, pixfmt, &producer))
         {
            import_producer_free(ctx, &producer);
            fprintf(stderr, "[skip] movie GPU format %u has no linear producer\n", pixfmt);
            continue;
         }
         memset(y, 72 + k * 17, w * h);
         for (col = 0; col < w * h / 2; col += 2)
         { uv[col] = (uint8_t)(65 + k * 13); uv[col + 1] = (uint8_t)(190 - k * 11); }
         for (plane = 0; plane < 2; plane++)
            for (row = 0; row < (plane ? h / 2 : h); row++)
               for (col = 0; col < w; col++)
               {
                  uint8_t *to = producer.map + producer.planes[plane].offset
                     + row * producer.planes[plane].rowPitch + col * bps;
                  unsigned sample = plane ? uv[row * w + col] : y[row * w + col];
                  if (bps == 2)
                  {
                     uint16_t packed = (uint16_t)(sample << 8);
                     memcpy(to, &packed, sizeof(packed));
                  }
                  else *to = (uint8_t)sample;
               }
         signal.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO_KHR;
         signal.pNext = NULL; signal.semaphore = producer.wait; signal.value = 1;
         CHECK(producer.signal(ctx->device, &signal) == VK_SUCCESS, "movie GPU: ready signal");
         memset(&hw, 0, sizeof(hw));
         memcpy(&hw.image, &producer.image, sizeof(producer.image));
         memcpy(&hw.plane_views[0], &producer.views[0], sizeof(VkImageView));
         memcpy(&hw.plane_views[1], &producer.views[1], sizeof(VkImageView));
         memcpy(&hw.wait, &producer.wait, sizeof(producer.wait));
         memcpy(&hw.done, &producer.done, sizeof(producer.done));
         hw.wait_value = hw.done_value = 1;
         hw.width = w; hw.height = h; hw.pixfmt = pixfmt;
         hw.yuv = flags; hw.layout = VK_IMAGE_LAYOUT_PREINITIALIZED;
         hw.abandon = movie_gpu_abandon;
         retro_atomic_store_relaxed_int(&movie_gpu_abandons, 0);
         CHECK(video_driver_movie_refresh(NULL, &hw), "movie GPU: frame not accepted");
         video_driver_frame(NULL, w, h, 0);
         if (threaded) video_thread_wait_idle();
         for (tries = 0; tries < 2000 && !retro_atomic_load_acquire_int(&movie_gpu_abandons); tries++)
         {
            producer.value(ctx->device, producer.done, &done);
            if (done >= 1) break;
            retro_sleep(1);
         }
         CHECK(done >= 1, "movie GPU: no GPU done signal (%s %u)", threaded ? "threaded" : "direct", k);
         image_yuv_420_to_rgb32(expected, w, y, w, uv, w, uv + 1, w, 2,
               w, h, flags | IMAGE_YUV_FLAG_RGBA);
         movie_gpu_read(expected[0], threaded, k);
         /* A second screenshot redraws the cached hardware sentinel. */
         movie_gpu_read(expected[0], threaded, k);
         checked++;
         import_producer_free(ctx, &producer);
      }
#ifdef HAVE_GFX_INSTRUMENT
      CHECK(gfx_instrument_get(GFX_INSTR_FRAME_COPY_HOST) == 0,
            "movie GPU: CPU frame copies during GPU playback");
#endif
      /* A real software frame must clear the imported movie source. */
      {
         static uint32_t software32[MAX_W * MAX_H];
         static uint16_t software16[MAX_W * MAX_H];
         enum retro_pixel_format format = video_state_get_ptr()->pix_fmt;
         unsigned i;
         const void *pixels;
         unsigned pitch;
         for (i = 0; i < MAX_W * MAX_H; i++)
         {
            software32[i] = format == RETRO_PIXEL_FORMAT_XRGB2101010
               ? UINT32_C(0x3ff00000) : UINT32_C(0x00ff0000);
            software16[i] = format == RETRO_PIXEL_FORMAT_0RGB1555 ? 0x7c00 : 0xf800;
         }
         pixels = (format == RETRO_PIXEL_FORMAT_RGB565 || format == RETRO_PIXEL_FORMAT_0RGB1555)
            ? (const void*)software16 : (const void*)software32;
         pitch = pixels == software16 ? MAX_W * 2 : MAX_W * 4;
         video_driver_frame(pixels, MAX_W, MAX_H, pitch);
         if (threaded) video_thread_wait_idle();
         movie_gpu_read(UINT32_C(0xff0000ff), threaded, FRAMES);
         movie_gpu_read(UINT32_C(0xff0000ff), threaded, FRAMES);
      }
      video_driver_movie_clear();
      video_driver_frame(NULL, 64, 48, 0);
      if (threaded) video_thread_wait_idle();
   }
   set_threaded_via_setting(false);
   run_frames(3);
   if (menu_was_up && !menu_is_up()) command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   if (failures == had && checked)
      fprintf(stderr, "[pass] movie GPU presentation: %u direct/threaded cases, resize, cached redraw and software transition\n", checked);
   else if (!checked) fprintf(stderr, "[skip] movie GPU presentation: no compatible producer\n");
#endif
}

static void lane_hw_ring_sync(void)
{
   unsigned had = failures;
   dylib_t lib  = runloop_state_get_ptr()->lib_handle;
   void (*ring)(int) = lib
      ? (void (*)(int))dylib_proc(lib, "harness_core_hw_ring") : NULL;
   void (*on_clear)(void (*)(uint64_t)) = lib
      ? (void (*)(void (*)(uint64_t)))dylib_proc(lib,
            "harness_core_hw_ring_on_clear") : NULL;
   unsigned f0, f1, s0, s1, replaced;
   unsigned h0, h1, hw_drawn, hw_replaced;
   thread_video_t *thr;

   if (!ring || !on_clear || !hwlane_core_call("harness_core_hw_active"))
   {
      CHECK(0, "hw ring sync: the harness core is not a hardware core"
            " with a ring mode");
      return;
   }

   set_threaded_via_setting(true);
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   hwlane_pump(5);
   expect_wrapper(true, "hw ring sync");
   thr = (thread_video_t*)video_state_get_ptr()->data;
   if (!thr || !thr->frame.hw_ring)
   {
      CHECK(0, "hw ring sync: the wrapper has no hardware ring for this driver");
      set_threaded_via_setting(false);
      hwlane_pump(5);
      return;
   }

   ring(1);
   f0 = hwlane_core_call("harness_core_hw_ring_sent");
   hwlane_pump(HWRING_FRAMES);
   video_thread_wait_idle();
   f1 = hwlane_core_call("harness_core_hw_ring_sent");
   CHECK(f1 - f0 >= HWRING_FRAMES / 2,
         "hw ring sync: %u images handed over in %u frames",
         f1 - f0, (unsigned)HWRING_FRAMES);

   /* Every hardware frame handed over and never drawn was replaced
    * or taken back by the push. */
   hwring_inner        = thr->driver;
   hwring_driver       = *thr->driver;
   hwring_driver.frame = hwring_frame;
   hwring_hw_drawn     = 0;
   hwring_present_ms   = 30;
   hwring_installed    = 0;
   hwring_overlaps     = 0;
   retro_atomic_ptr_init(&hwring_drawing, NULL);
   set_driver(thr, &hwring_driver);
   hwring_inner_poke           = thr->poke;
   hwring_poke                 = *thr->poke;
   hwring_poke.hw_ring_install = hwring_install;
   set_poke(thr, &hwring_poke);
   on_clear(hwring_on_clear);
#ifdef HAVE_GFX_INSTRUMENT
   gfx_instrument_reset();
#endif
   ring(2);
   s0 = hwlane_core_call("harness_core_hw_ring_sent");
   hwlane_pump(HWRING_SLOW_FRAMES);
   video_thread_wait_idle();
   s1 = hwlane_core_call("harness_core_hw_ring_sent");
   replaced = s1 - s0 - hwring_hw_drawn;
   CHECK(replaced >= HWRING_SLOW_FRAMES / 12,
         "hw ring sync: %u of %u hardware frames replaced behind a"
         " slow present: the lane proved nothing",
         replaced, s1 - s0);

   /* Hardware frames only: nothing but the core's own frames to drop,
    * and the core must not be held to the present's rate for it. The
    * present is slow enough that a slow runner's core still outruns
    * it several times over. */
   hwring_hw_drawn   = 0;
   hwring_present_ms = 100;
   ring(3);
   h0 = hwlane_core_call("harness_core_hw_ring_sent");
   hwlane_pump(HWRING_SLOW_FRAMES);
   video_thread_wait_idle();
   h1 = hwlane_core_call("harness_core_hw_ring_sent");
   on_clear(NULL);
   set_poke(thr, hwring_inner_poke);
   set_driver(thr, hwring_inner);
   hw_drawn    = hwring_hw_drawn;
   hw_replaced = h1 - h0 - hw_drawn;
   CHECK(hw_replaced >= (h1 - h0) / 3,
         "hw ring sync: hardware frames only, %u handed over and %u drawn"
         " behind a slow present: the core is held to the present",
         h1 - h0, hw_drawn);
   CHECK(hwring_overlaps == 0,
         "hw ring sync: the core cleared the image being drawn %u time(s)",
         hwring_overlaps);
#ifdef HAVE_GFX_INSTRUMENT
   {
      int drops   = gfx_instrument_get(GFX_INSTR_HW_DROP);
      int submits = gfx_instrument_get(GFX_INSTR_HW_DROP_SUBMIT);
      /* Every frame handed over behind a semaphore and never drawn was
       * dropped, and a drop costs the core's thread one queue
       * submission: the semaphore wait and the slot's fence together. */
      CHECK(drops == (int)(replaced + hw_replaced),
            "hw ring sync: %u frames replaced but %d dropped on the ring",
            replaced + hw_replaced, drops);
      CHECK(submits == drops,
            "hw ring sync: %d drops cost %d queue submissions, not one each",
            drops, submits);
      fprintf(stderr, "[baseline] hw ring sync: %d drops, %d queue submissions\n",
            drops, submits);
   }
#endif

   ring(0);
   hwlane_pump(5);
   set_threaded_via_setting(false);
   hwlane_pump(5);

   if (failures == had)
      fprintf(stderr, "[pass] hw ring sync lane (%u images, %u of %u"
            " hardware frames replaced behind a slow present, %u of %u"
            " with hardware frames only)\n",
            f1 - f0, replaced, s1 - s0, hw_replaced, h1 - h0);
}

/* ------------------------------------------------------------------ */
/* Lane: a display element past the top                              */
/*   The menu draws a quad whose top reaches above the screen through */
/*   the display driver, unthreaded and under the wrapper. Its        */
/*   top-down y is negative, and a viewport or vertex the driver      */
/*   builds from it has to be too: on Vulkan a y outside             */
/*   viewportBoundsRange is VUID-VkViewport-y-01776, which the        */
/*   validation layer turns into a failure here.                      */
/* ------------------------------------------------------------------ */

static menu_ctx_driver_t         pastop_menu;
static const menu_ctx_driver_t  *pastop_inner;
static retro_atomic_int_t        pastop_draws;

static void pastop_frame(void *data, video_frame_info_t *video_info)
{
   static float white[16] = {
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f
   };
   gfx_display_t *p_disp = disp_get_ptr();
   unsigned h            = VIDEO_SCALE_H(video_info->dims);
   struct video_coords coords;
   gfx_display_ctx_draw_t draw;

   if (pastop_inner->frame)
      pastop_inner->frame(data, video_info);
   if (!p_disp->dispctx || !p_disp->dispctx->draw || h < 64)
      return;

   memset(&draw, 0, sizeof(draw));
   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = white;
   /* 64x64, bottom-up origin, its top 24 px above the screen */
   draw.pos             = VIDEO_POS_PACK(16, (int)h + 24 - 64);
   draw.dims            = VIDEO_SCALE_PACK(64, 64);
   draw.coords          = &coords;
   draw.scale_factor    = 1.0f;
   gfx_display_draw(p_disp->dispctx, &draw, video_info->userdata,
         video_info->dims);
   retro_atomic_fetch_add_int(&pastop_draws, 1);
}

static void lane_display_past_top(void)
{
   unsigned had               = failures;
   struct menu_state *menu_st = menu_state_get_ptr();
   bool opened                = false;
   int unthreaded, threaded;

   if (!menu_is_up())
   {
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
      opened = true;
   }
   CHECK(menu_is_up(), "past-top lane: menu not up");

   /* Swapped only while no video thread exists; the thread's creation
    * and join order it against the frames that read it. */
   set_threaded_via_setting(false);
   pastop_inner        = menu_st->driver_ctx;
   pastop_menu         = *pastop_inner;
   pastop_menu.frame   = pastop_frame;
   menu_st->driver_ctx = &pastop_menu;
   retro_atomic_store_release_int(&pastop_draws, 0);

   run_frames(10);
   unthreaded = retro_atomic_load_acquire_int(&pastop_draws);
   CHECK(unthreaded > 0, "past-top lane: no element drawn unthreaded");

   set_threaded_via_setting(true);
   run_frames(10);
   video_thread_wait_idle();
   set_threaded_via_setting(false);
   threaded = retro_atomic_load_acquire_int(&pastop_draws) - unthreaded;
   CHECK(threaded > 0, "past-top lane: no element drawn under the wrapper");
   CHECK(menu_st->driver_ctx == &pastop_menu,
         "past-top lane: a reinit replaced the menu driver");

   menu_st->driver_ctx = pastop_inner;
   if (opened)
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);

   if (failures == had)
      fprintf(stderr, "[pass] display element past the top lane"
            " (%d draws unthreaded, %d under the wrapper)\n",
            unthreaded, threaded);
}

#define MOVIE_RING_FRAMES 132
static retro_atomic_int_t movie_ring_released[MOVIE_RING_FRAMES + 1];
static retro_atomic_int_t movie_ring_errors;
static retro_atomic_int_t movie_ring_geometry_errors;
static video_driver_t movie_ring_driver;
static video_poke_interface_t movie_ring_poke;
static const video_driver_t *movie_ring_inner;
static unsigned movie_ring_active;
static void movie_ring_release(void *owner, uint64_t done, uint64_t value)
{
   (void)owner;
   if (done != 88 || value > MOVIE_RING_FRAMES || !value
         || !retro_atomic_cas_int(&movie_ring_released[value], 0, 2))
      retro_atomic_fetch_add_int(&movie_ring_errors, 1);
}
static bool movie_ring_device(void *data, video_device_info_t *info)
{
   (void)data;
   memset(info, 0, sizeof(*info));
   info->import_formats = IMAGE_PIXFMT_NV12 | IMAGE_PIXFMT_P010;
   return true;
}
static enum video_texture_update movie_ring_import(void *data,
      const video_frame_import_t *frame)
{
   unsigned id;
   (void)data;
   if (!frame) { movie_ring_active = 0; return VIDEO_TEXTURE_UPDATE_DONE; }
   id = (unsigned)frame->image;
   if (sthread_get_thread_id(((thread_video_t*)video_state_get_ptr()->data)->thread)
            != sthread_get_current_thread_id()
         || frame->dims != VIDEO_SCALE_PACK(1920, 1080)
         || frame->wait_value != id || frame->done_value != id
         || frame->plane_views[0] != id + 1000 || frame->plane_views[1] != id + 2000
         || frame->array_slice != 7 || frame->crop_x != 2 || frame->crop_y != 4
         || frame->output_pixfmt != 0
         || frame->color.primaries != 9 || frame->color.matrix != 9
         || frame->color.transfer != (id % 2 ? 16 : 18)
         || frame->color.range != 1 || frame->color.max_cll != id + 100
         || frame->color.max_fall != id + 50
         || frame->color.mastering.present != VIDEO_MASTERING_DISPLAY_COMPLETE
         || frame->color.mastering.chromaticity[0] != 25000
         || frame->color.mastering.luminance_max != 10240000
         || frame->wait != 34 || frame->done != 88 || !id || id > MOVIE_RING_FRAMES)
   {
      retro_atomic_fetch_add_int(&movie_ring_errors, 1);
      return VIDEO_TEXTURE_UPDATE_REFUSED;
   }
   if (!(id % 3)) return VIDEO_TEXTURE_UPDATE_DROPPED;
   if (!(id % 5)) return VIDEO_TEXTURE_UPDATE_REFUSED;
   if (!retro_atomic_cas_int(&movie_ring_released[id], 0, 1))
      retro_atomic_fetch_add_int(&movie_ring_errors, 1);
   movie_ring_active = id;
   return VIDEO_TEXTURE_UPDATE_DONE;
}
static bool movie_ring_frame(void *data, const void *frame, unsigned dims,
      uint64_t count, unsigned pitch, const char *msg, video_frame_info_t *info)
{
   if (movie_ring_active && (frame || dims != VIDEO_SCALE_PACK(1920, 1080)))
      retro_atomic_fetch_add_int(&movie_ring_geometry_errors, 1);
   return movie_ring_inner->frame(data, NULL, dims, count, pitch, msg, info);
}
static uintptr_t movie_ring_replay(void *data)
{
   (void)data;
   video_driver_cached_frame();
   return 0;
}
static void lane_movie_ring(void)
{
   unsigned had = failures;
   unsigned i;
   video_hw_frame_t hw;
   thread_video_t *thr;
   const video_poke_interface_t *inner_poke;
   bool menu_was_up;
   set_threaded_via_setting(true);
   run_frames(3);
   menu_was_up = menu_is_up();
   if (menu_was_up) command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_frames(2);
   video_thread_wait_idle();
   thr = (thread_video_t*)video_state_get_ptr()->data;
   if (!thr) return;
   movie_ring_inner = thr->driver; inner_poke = thr->poke;
   movie_ring_driver = *thr->driver; movie_ring_driver.frame = movie_ring_frame;
   if (inner_poke) movie_ring_poke = *inner_poke;
   else memset(&movie_ring_poke, 0, sizeof(movie_ring_poke));
   movie_ring_poke.get_device = movie_ring_device;
   movie_ring_poke.import_movie_frame = movie_ring_import;
   set_driver(thr, &movie_ring_driver); set_poke(thr, &movie_ring_poke);
   CHECK(video_driver_movie_refresh(NULL, NULL), "movie ring: capability missing");
   memset(&hw, 0, sizeof(hw));
   hw.width = 1920; hw.height = 1080; hw.pixfmt = IMAGE_PIXFMT_P010;
   hw.wait = 34; hw.done = 88; hw.abandon = movie_ring_release;
   hw.array_slice = 7; hw.crop_x = 2; hw.crop_y = 4;
   hw.color.primaries = hw.color.matrix = 9; hw.color.range = 1;
   hw.color.mastering.present = VIDEO_MASTERING_DISPLAY_COMPLETE;
   hw.color.mastering.chromaticity[0] = 25000;
   hw.color.mastering.luminance_max = 10240000;
   for (i = 1; i <= MOVIE_RING_FRAMES - 6; i++)
   {
      hw.image = i; hw.wait_value = hw.done_value = i;
      hw.color.transfer = i % 2 ? 16 : 18;
      hw.color.max_cll = i + 100; hw.color.max_fall = i + 50;
      hw.plane_views[0] = i + 1000; hw.plane_views[1] = i + 2000;
      if (!video_driver_movie_refresh(NULL, NULL))
         video_driver_movie_clear();
      CHECK(video_driver_movie_refresh(NULL, &hw), "movie ring: staging refused %u", i);
      if (thr->movie_next.pending)
         CHECK(!video_driver_movie_refresh(NULL, &hw), "movie ring: staging overwrite %u", i);
      else
         CHECK(!video_driver_movie_refresh(NULL, NULL), "movie ring: refusal not latched");
      video_driver_frame(NULL, 1920, 1080, 0);
   }
   video_thread_wait_idle();
   video_driver_movie_clear();
   for (i = MOVIE_RING_FRAMES - 5; i <= MOVIE_RING_FRAMES - 2; i++)
   {
      hw.image = hw.wait_value = hw.done_value = i;
      hw.color.transfer = i % 2 ? 16 : 18;
      hw.color.max_cll = i + 100; hw.color.max_fall = i + 50;
      hw.plane_views[0] = i + 1000; hw.plane_views[1] = i + 2000;
      CHECK(video_driver_movie_refresh(NULL, &hw), "movie ring: controlled stage %u", i);
      video_driver_frame(NULL, 1920, 1080, 0);
      video_thread_wait_idle();
      CHECK(video_driver_movie_refresh(NULL, NULL) == (i % 3 == 0 || i % 5 != 0),
            "movie ring: busy/refused readiness %u", i);
   }
   video_thread_run_blocking(movie_ring_replay, NULL);
   /* The accepted image before the busy/refused imports remains active.
    * A NULL repeat carries no CPU pixels and must retain movie geometry. */
   video_driver_frame(NULL, 1920, 1080, 0);
   video_thread_wait_idle();
   video_driver_movie_clear();
   CHECK(video_driver_movie_refresh(NULL, NULL), "movie ring: clear did not restore readiness");
   /* Clear releases a staged lease before its frame is published. */
   hw.image = hw.wait_value = hw.done_value = MOVIE_RING_FRAMES - 1;
   hw.color.transfer = hw.image % 2 ? 16 : 18;
   hw.color.max_cll = (unsigned)hw.image + 100;
   hw.color.max_fall = (unsigned)hw.image + 50;
   hw.plane_views[0] = hw.image + 1000; hw.plane_views[1] = hw.image + 2000;
   CHECK(video_driver_movie_refresh(NULL, &hw), "movie ring: stage before clear");
   video_driver_movie_clear();
   video_driver_frame(NULL, 1920, 1080, 0);
   video_thread_wait_idle();
   /* Teardown releases a lease that never reached the frame ring. */
   hw.image = hw.wait_value = hw.done_value = MOVIE_RING_FRAMES;
   hw.color.transfer = hw.image % 2 ? 16 : 18;
   hw.color.max_cll = (unsigned)hw.image + 100;
   hw.color.max_fall = (unsigned)hw.image + 50;
   hw.plane_views[0] = hw.image + 1000; hw.plane_views[1] = hw.image + 2000;
   CHECK(video_driver_movie_refresh(NULL, &hw), "movie ring: stage before teardown");
   set_driver(thr, movie_ring_inner); set_poke(thr, inner_poke);
   set_threaded_via_setting(false);
   for (i = 1; i <= MOVIE_RING_FRAMES; i++)
      CHECK(retro_atomic_load_acquire_int(&movie_ring_released[i]) != 0,
            "movie ring: lease %u not released", i);
   CHECK(!retro_atomic_load_acquire_int(&movie_ring_errors), "movie ring: ordering or duplicate release");
   CHECK(!retro_atomic_load_acquire_int(&movie_ring_geometry_errors),
         "movie ring: duplicate changed image or geometry");
   run_frames(2);
   if (menu_was_up && !menu_is_up()) command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   if (failures == had) fprintf(stderr, "[pass] threaded movie ring: metadata, backpressure, refusal recovery, clear and teardown\n");
}

#include "hdr_surface.h"
#include "still_surface.h"
#include "surface_pitch.h"
#include "preview_surface.h"

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
      if (getenv("HARNESS_HDR_MODE"))
      {
         fprintf(cfg, "video_hdr_enable = \"true\"\nvideo_hdr_mode = \"%s\"\n",
               getenv("HARNESS_HDR_MODE"));
         fprintf(cfg, "video_hdr_menu_nits = \"203\"\nvideo_hdr_expand_gamut = \"0\"\n");
      }
      fprintf(cfg, "audio_driver = \"null\"\n");
      fprintf(cfg, "input_driver = \"null\"\n");
      fprintf(cfg, "input_joypad_driver = \"null\"\n");
      fprintf(cfg, "menu_driver = \"%s\"\n",
            (getenv("HARNESS_STILL_ONLY") || getenv("HARNESS_PREVIEW_FILE")) ? "ozone" : "rgui");
      if (getenv("HARNESS_MENU_UPSCALE"))
         fprintf(cfg, "rgui_internal_upscale_level = \"%s\"\n",
               getenv("HARNESS_MENU_UPSCALE"));
      fprintf(cfg, "video_threaded = \"false\"\n");
      fprintf(cfg, "video_vsync = \"false\"\n");
      fprintf(cfg, "menu_pause_libretro = \"true\"\n");
      /* The window is never the active one - the harness is run from
       * a terminal, and on macOS the application never becomes key -
       * and the default pauses the core while it is not (that is the
       * "Paused" in the corner). Every lane counts frames the core
       * ran; a paused core runs none. */
      fprintf(cfg, "pause_nonactive = \"false\"\n");
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

#ifdef HARNESS_WRAP_CALLOC
   lane_driver_struct_alloc();
#endif

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
   {
      const char *requested = getenv("HARNESS_VIDEO_DRIVER");
      const char *actual    = video_driver_get_ident();
      if (requested && (!actual || strcmp(requested, actual)))
      {
         fprintf(stderr, "FAIL: requested video driver %s, initialized %s\n",
               requested, actual ? actual : "none");
         failures++;
         goto shutdown;
      }
   }

   if (getenv("HARNESS_MENU_UPSCALE"))
   {
      unsigned requested = (unsigned)strtoul(getenv("HARNESS_MENU_UPSCALE"), NULL, 10);
      CHECK(config_get_ptr()->uints.menu_rgui_internal_upscale_level == requested,
            "requested RGUI upscale %u, initialized %u", requested,
            config_get_ptr()->uints.menu_rgui_internal_upscale_level);
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

   if (getenv("HARNESS_PREVIEW_FILE"))
   {
      lane_preview_surface(getenv("HARNESS_PREVIEW_FILE"));
      goto shutdown;
   }

   if (getenv("HARNESS_SURFACE_PITCH_ONLY"))
   {
      lane_surface_pitch();
      goto shutdown;
   }

   if (getenv("HARNESS_STILL_ONLY"))
   {
      lane_still_surface();
      goto shutdown;
   }

   if (getenv("HARNESS_HDR_MODE"))
   {
      lane_hdr_surface();
      goto shutdown;
   }

   if (getenv("HARNESS_MOVIE_GPU_ONLY"))
   {
      lane_surface_import();
      lane_movie_gpu();
      goto shutdown;
   }

   if (getenv("HARNESS_MOVIE_ONLY"))
   {
      lane_movie_ring();
      goto shutdown;
   }

   /* The harness core as a hardware core: its own lanes, alone. */
   if (getenv("HARNESS_CORE_HW_VULKAN"))
   {
      if (hwlane_core_call("harness_core_hw_cached"))
         lane_hw_cached_context();
      else
      {
         lane_hw_ring_sync();
         lane_hw_image_lifetime(core_path);
      }
      goto shutdown;
   }

   if (getenv("HARNESS_RECORD_GPU_LEASE"))
   {
      record_gpu_stop_in_flight();
      goto shutdown;
   }
   lane_line_separation();
   lane_vp_params_publish();
   lane_toggle_cycle(cycles);
   lane_reinit_under_wrapper(cycles / 2 + 1);
#ifdef HAVE_GFX_WIDGETS
   lane_threaded_off_in_flight();
#endif
   lane_toggle_in_game(cycles);
   lane_driver_pacing();
   lane_swap_count();
   lane_replay_swap_count();
   lane_stats_snapshot();
   lane_stat_text_bounds();
   lane_viewport_publish();
   if (!real_driver())
      lane_record_reader();
   record_gpu_stop_in_flight();
   if (!real_driver())
      lane_pixel_format();
   lane_context_api_behind_wrapper();
   lane_headset_calls();
   lane_headset_target_rate();
   lane_shader_params();
   lane_async_texture_load();
   if (!real_driver())
      lane_present_repeat();
   lane_every_command_replies();
   lane_second_ring_waiter();
   lane_reentrant_from_frame();
   lane_window_thread_present();
   lane_async_done_main_thread();
   if (!real_driver())
      lane_display_phase();
   lane_command_runs_once();
   lane_concurrent_posters();
   lane_font_marshal();
   if (real_driver())
      lane_display_past_top();
   if (!real_driver())
      lane_menu_texture();
   if (!real_driver())
   {
      lane_display_pacing();
      lane_blocking_present();
      lane_pacing_queue_drain();
      lane_pacing_fast_display();
      lane_pacing_after_stall();
      lane_pacing_rate();
   }
   lane_zero_copy();
   lane_zero_copy_ring_full();
   if (real_driver())
      lane_lent_window_paused();
   if (     real_driver()
         && (  !strcmp(getenv("HARNESS_VIDEO_DRIVER"), "vulkan")
            || !strcmp(getenv("HARNESS_VIDEO_DRIVER"), "d3d12")
            || !strcmp(getenv("HARNESS_VIDEO_DRIVER"), "gl")
            || !strcmp(getenv("HARNESS_VIDEO_DRIVER"), "glcore")
            || !strcmp(getenv("HARNESS_VIDEO_DRIVER"), "metal")))
      lane_frame_mem();
   /* Where the readback draws what the driver last uploaded (gl1's
    * draws nothing without the cached frame) */
   if (     real_driver()
         && (  !strcmp(getenv("HARNESS_VIDEO_DRIVER"), "vulkan")
            || !strcmp(getenv("HARNESS_VIDEO_DRIVER"), "gl")
            || !strcmp(getenv("HARNESS_VIDEO_DRIVER"), "glcore")))
      lane_frame_lag();
   lane_pacing_flag_follows_wrapper();
   lane_surface_update();
   lane_surface_external();
   lane_surface_lend();
   lane_surface_pitch();
   lane_surface_planar();
   lane_surface_import();
   lane_movie_gpu();
#if defined(HAVE_OPENGL) && defined(HAVE_GL_TEXTURE_LEND)
   if (real_driver() && !strcmp(getenv("HARNESS_VIDEO_DRIVER"), "gl"))
      lane_gl2_linear();
#endif
   if (real_driver())
      lane_surface_4k();
   /* The one driver that batches and counts its upload submissions */
   if (real_driver() && !strcmp(getenv("HARNESS_VIDEO_DRIVER"), "vulkan"))
      lane_upload_batch();
   lane_overlay_textures();
   lane_driver_reloads();
   if (real_driver())
      lane_x11_event_pump();
   if (real_driver())
      lane_x11_wsi_connection();
   if (     real_driver()
         && (  !strcmp(getenv("HARNESS_VIDEO_DRIVER"), "vulkan")
            || !strcmp(getenv("HARNESS_VIDEO_DRIVER"), "d3d11")
            || !strcmp(getenv("HARNESS_VIDEO_DRIVER"), "d3d12")))
      lane_gpu_readback();
   if (real_driver() && !strcmp(getenv("HARNESS_VIDEO_DRIVER"), "vulkan"))
      lane_record_format();
   if (real_driver() && !strcmp(getenv("HARNESS_VIDEO_DRIVER"), "gl1"))
      lane_record_game_only(true);
   if (     real_driver()
         && (   !strcmp(getenv("HARNESS_VIDEO_DRIVER"), "sdl2")
             || !strcmp(getenv("HARNESS_VIDEO_DRIVER"), "sdl3")))
      lane_record_game_only(false);
   if (real_driver())
      lane_x11_grabbed_mouse();
   if (     real_driver()
         && (   !strcmp(getenv("HARNESS_VIDEO_DRIVER"), "gl")
             || !strcmp(getenv("HARNESS_VIDEO_DRIVER"), "glcore")))
      lane_gl_fp16_set_full();
   if (!real_driver())
   {
      lane_waiter_call();
      lane_frame_path_heap();
      lane_resize_under_wrapper();
      lane_dupe_under_wrapper();
      lane_movie_ring();
      lane_suppress_screensaver();
      lane_window_answers();
      lane_size_pair_round_trip();
   }
   else
      fprintf(stderr, "[skip] null-driver instrumented lanes (real driver: %s)\n",
            getenv("HARNESS_VIDEO_DRIVER"));

shutdown:
   /* Orderly shutdown: the teardown barriers are part of what is
    * under test. */
   set_threaded_via_setting(true);
   run_frames(3);
#ifdef HAVE_COCOA
   /* main_exit() terminates the Cocoa application; the status has
    * to be in place before it. */
   harness_cocoa_exit_status(failures ? 1 : 0);
#endif
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

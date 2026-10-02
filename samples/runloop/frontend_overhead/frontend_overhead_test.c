/* Frontend overhead harness: what one runloop_iterate() costs when the
 * core costs nothing.
 *
 * Links the shipping objects with only main() replaced (see build.sh),
 * boots on the null drivers, loads a core whose retro_run() does the
 * least a real core does - one frame, one audio batch, one input poll
 * and read - and measures the frame loop around it. Two things are
 * checked, both properties the frame path has and must keep:
 *
 *   - No general-purpose heap call on the frame path: not from the
 *     iterate, not from the video frame callback, not from the audio
 *     batch callback. The threaded-video harness counts this on its
 *     own core, which pushes no audio; this one covers the audio
 *     entry too, where the fold and remap staging used to grow on the
 *     callback.
 *
 *   - Microseconds per frame under fast-forward, where the pacer
 *     leaves the loop unthrottled. On a 2 GHz x86 box this is single
 *     digits; the bound here is loose (FRAME_US_LIMIT) because CI
 *     runners vary, and it exists to catch a blocking call or a sleep
 *     that lands on the frame path, not to tune. The number is printed
 *     so a trend is visible in the log.
 *
 *   - The input lanes (input plan, Phase 0): what input_poll and
 *     input_state cost a core, timed from the core's side of the
 *     libretro calls, with no pad, one pad, and eight pads from the
 *     test joypad driver - the scripted synthetic provider. They print
 *     the baseline the input redesign is measured against, and fail on
 *     a heap call in poll or state, on a cost a blocking call would
 *     cause, or on a per-query cost that grows with the query count
 *     (each input_state call should cost the same however many a frame
 *     makes).
 *
 * Nothing is stubbed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifndef _WIN32
#include <dlfcn.h>
#endif
#include <queues/task_queue.h>
#include "../../../core.h"
#include <file/file_path.h>
#include <boolean.h>
#include <features/features_cpu.h>
#include <time/rtime.h>
#include "../../../runloop.h"
#include "../../../retroarch.h"
#include "../../../configuration.h"
#include "../../../command.h"
#include "../../../verbosity.h"
#include "../../../frontend/frontend_driver.h"
#include "../../../gfx/video_driver.h"
#include "../../../input/input_driver.h"
#include "../../../menu/menu_driver.h"

#define FRAME_US_LIMIT   1000.0
#define HEAP_FRAMES      300
#define TIMED_FRAMES     20000

/* The input lanes' scripted pads arrive after the frame-cost lane has
 * run, so the lanes before them measure the loop with no pad; the
 * other seven arrive after both one-pad windows. A connect runs in
 * input_poll and allocates its autoconfig task there, so no measured
 * window may span one. */
#define PAD1_FRAME       30000
#define PADS8_FRAME      40000
/* Pad 1's B is pressed at POLL_MODE_FRAME + 1000 * mode and released
 * POLL_EDGE_SPAN later, once under each poll mode. */
#define POLL_MODE_FRAME  50000
#define POLL_EDGE_SPAN   100
#define INPUT_FRAMES     4000
/* Tripwires, not targets: CI runners vary. They catch a blocking call,
 * a sleep or a device open landing in poll or state. */
#define POLL_US_LIMIT    200.0
#define QUERY_NS_LIMIT   2000.0

static unsigned failures;

#define CHECK(cond, msg) do { \
   if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
} while (0)

static void run_frames(unsigned n)
{
   unsigned i;
   for (i = 0; i < n; i++)
      runloop_iterate();
}

static bool menu_is_up(void)
{
#ifdef HAVE_MENU
   return (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE) != 0;
#else
   return false;
#endif
}

/* Fast-forward the way the hotkey engages it, so the pacer sees the
 * real facts rather than a harness flag. */
static void fast_forward(bool on)
{
   input_driver_state_t *input_st = input_state_get_ptr();
   runloop_state_t *runloop_st    = runloop_state_get_ptr();
   if (on)
   {
      input_st->flags   |=  INP_FLAG_NONBLOCKING;
      runloop_st->flags |=  RUNLOOP_FLAG_FASTMOTION;
   }
   else
   {
      input_st->flags   &= ~INP_FLAG_NONBLOCKING;
      runloop_st->flags &= ~RUNLOOP_FLAG_FASTMOTION;
   }
   command_event(CMD_EVENT_SET_FRAME_LIMIT, NULL);
}

/* Heap counting by glibc interposition, as the threaded-video harness
 * does it; only where that works and no allocator sanitizer is in the
 * way. */
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
/* The input lanes count only while the core is inside input_poll or
 * input_state (the core's harness_core_in_input), not the whole loop. */
static volatile int *heap_scope;
#define HEAP_COUNTED() (heap_counting && (!heap_scope || *heap_scope))

void *malloc(size_t n)            { if (HEAP_COUNTED()) heap_calls++; return __libc_malloc(n); }
void  free(void *p)               { __libc_free(p); }
void *calloc(size_t a, size_t b)  { if (HEAP_COUNTED()) heap_calls++; return __libc_calloc(a, b); }
void *realloc(void *p, size_t n)  { if (HEAP_COUNTED()) heap_calls++; return __libc_realloc(p, n); }
#endif

static void lane_frame_path_heap(void)
{
#ifdef HARNESS_COUNT_HEAP
   unsigned long calls;
   unsigned had = failures;

   /* Settle: the first frames after boot still build things. */
   run_frames(60);
   heap_calls    = 0;
   heap_counting = true;
   run_frames(HEAP_FRAMES);
   heap_counting = false;
   calls = heap_calls;

   CHECK(calls == 0, "heap calls on the frame path (iterate + video frame + audio batch)");
   if (calls)
      fprintf(stderr, "       %lu heap call(s) over %u frames\n", calls, HEAP_FRAMES);
   if (failures == had)
      printf("[pass] no heap calls over %u frames of iterate + video + audio\n", HEAP_FRAMES);
#else
   printf("[skip] heap lane: no glibc interposition under this build\n");
#endif
}

static char core_path_g[512];
static void (*core_input_measure)(unsigned, unsigned);
static void (*core_input_stats)(unsigned long*, unsigned long long*,
      unsigned long long*, unsigned long long*);
static long (*core_runs)(void);
static void (*core_input_edges)(long*, long*);

/* The main loop's pass: iterate, then the task queue, as retroarch.c
 * runs them. The input lanes need the queue - a pad's autoconfig is a
 * task - so they step frames this way; the lanes above measure the
 * iterate alone. */
static void run_loop_frames(unsigned n)
{
   unsigned i;
   for (i = 0; i < n; i++)
   {
      runloop_iterate();
      task_queue_check();
   }
}

static void run_to_frame(uint64_t frame)
{
   while (video_state_get_ptr()->frame_count < frame)
   {
      runloop_iterate();
      task_queue_check();
   }
}

/* @queries input_state calls a frame over @ports ports: microseconds
 * per frame in input_poll and in the first input_state call (with the
 * default late polling the poll happens there), nanoseconds per further
 * input_state call, and the heap calls made inside them. */
static void measure_input(unsigned queries, unsigned ports,
      double *poll_us, double *first_us, double *query_ns, unsigned long *heap)
{
   unsigned long      frames;
   unsigned long long poll_ns, first_ns, state_ns;
   core_input_measure(queries, ports);
#ifdef HARNESS_COUNT_HEAP
   heap_calls    = 0;
   heap_counting = true;
#endif
   run_loop_frames(INPUT_FRAMES);
#ifdef HARNESS_COUNT_HEAP
   heap_counting = false;
   *heap         = heap_calls;
#else
   *heap         = 0;
#endif
   core_input_stats(&frames, &poll_ns, &first_ns, &state_ns);
   core_input_measure(0, 1);
   *poll_us  = frames ? (double)poll_ns  / frames / 1000.0 : 0.0;
   *first_us = frames ? (double)first_ns / frames / 1000.0 : 0.0;
   *query_ns = (frames && queries > 1)
      ? (double)state_ns / ((double)frames * (queries - 1)) : 0.0;
}

static void report_input(const char *what, unsigned queries,
      double poll_us, double first_us, double query_ns, unsigned long heap)
{
   printf("[info] input, %s: input_poll %.2f us, first input_state %.2f us"
         " (the late poll), then %.1f ns each (%u a frame)\n",
         what, poll_us, first_us, query_ns, queries);
   CHECK(heap == 0, "heap calls in input_poll / input_state");
   if (heap)
      fprintf(stderr, "       %s: %lu heap call(s) over %u frames\n", what, heap, INPUT_FRAMES);
   CHECK(poll_us + first_us < POLL_US_LIMIT, "input poll over the tripwire: something blocks in poll");
   CHECK(query_ns < QUERY_NS_LIMIT, "input_state over the tripwire: something blocks in a query");
}

static bool pad_connected(unsigned port)
{
   const char *name = input_config_get_device_name(port);
   return name && *name;
}

static void lane_input_cost(void)
{
   unsigned had = failures;
   double   poll_us, first_us, q16, q256, q8;
   unsigned long heap;
   void    *core;

#if !defined(HAVE_TEST_DRIVERS) || defined(_WIN32)
   (void)had; (void)poll_us; (void)first_us; (void)q16; (void)q256; (void)q8;
   (void)heap; (void)core;
   printf("[skip] input lanes: need the test drivers and dlopen\n");
#else
   if (   !(core = dlopen(core_path_g, RTLD_NOW))
       || !(core_input_measure = (void (*)(unsigned, unsigned))
             dlsym(core, "harness_core_input_measure"))
       || !(core_input_stats = (void (*)(unsigned long*, unsigned long long*,
             unsigned long long*, unsigned long long*))dlsym(core, "harness_core_input_stats"))
       || !(core_runs = (long (*)(void))dlsym(core, "harness_core_runs"))
       || !(core_input_edges = (void (*)(long*, long*))dlsym(core, "harness_core_input_edges")))
   {
      CHECK(false, "input lanes: the harness core's measuring entry points");
      return;
   }

#ifdef HARNESS_COUNT_HEAP
   heap_scope = (volatile int*)dlsym(core, "harness_core_in_input");
   CHECK(heap_scope != NULL, "input lanes: the harness core's input-scope flag");
   if (!heap_scope)
      return;
#endif

   fast_forward(true);

   /* no pad: the joypad driver has nothing connected */
   run_loop_frames(200);
   measure_input(16, 1, &poll_us, &first_us, &q16, &heap);
   report_input("no pad", 16, poll_us, first_us, q16, heap);

   /* one pad, autoconfigured, read 16 and 256 times a frame */
   run_to_frame(PAD1_FRAME);
   run_loop_frames(500);
   CHECK(pad_connected(0), "input lanes: the scripted pad did not connect");
   measure_input(16, 1, &poll_us, &first_us, &q16, &heap);
   report_input("one pad", 16, poll_us, first_us, q16, heap);
   measure_input(256, 1, &poll_us, &first_us, &q256, &heap);
   report_input("one pad", 256, poll_us, first_us, q256, heap);
   /* past the first, each query should cost the same however many a
    * frame makes */
   CHECK(q256 <= 2.0 * q16 + 50.0, "input_state cost per query grows with the query count");

   /* eight pads, 16 queries each */
   run_to_frame(PADS8_FRAME);
   run_loop_frames(500);
   CHECK(pad_connected(7), "input lanes: the eighth scripted pad did not connect");
   measure_input(128, 8, &poll_us, &first_us, &q8, &heap);
   report_input("eight pads", 128, poll_us, first_us, q8, heap);

   fast_forward(false);
#ifdef HARNESS_COUNT_HEAP
   heap_scope = NULL;
#endif
   if (failures == had)
      printf("[pass] input poll and state: no heap, under the tripwires, flat per query\n");
#endif
}

/* The same scripted press and release under early, normal and late
 * polling: each mode's poll cost as the core sees it, and the run in
 * which the core first sees each edge, which must be the same in every
 * mode - no poll mode adds a frame between the device and the core. */
static void lane_input_poll_modes(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
   static const char *const name[3] = { "early", "normal", "late" };
   long     press_at[3], release_at[3];
   unsigned had = failures;
   unsigned m;

   if (!core_runs || !core_input_edges)
   {
      CHECK(false, "poll modes: the harness core's edge entry points");
      return;
   }
   fast_forward(true);
   for (m = 0; m < 3; m++)
   {
      unsigned long      frames;
      unsigned long long poll_ns, first_ns, state_ns;
      uint64_t press = POLL_MODE_FRAME + 1000 * m;
      long     lag;

      core_set_poll_type(m);
      run_to_frame(press - 200);
      core_input_measure(16, 1);
      /* frames and runs advance together: their gap turns a script
       * frame into the run that polls it */
      lag = (long)video_state_get_ptr()->frame_count - core_runs();
      run_to_frame(press + POLL_EDGE_SPAN + 200);
      core_input_edges(&press_at[m], &release_at[m]);
      core_input_stats(&frames, &poll_ns, &first_ns, &state_ns);
      core_input_measure(0, 1);
      press_at[m]   = press_at[m]   < 0 ? -1 : press_at[m]   + lag - (long)press;
      release_at[m] = release_at[m] < 0 ? -1 : release_at[m] + lag - (long)(press + POLL_EDGE_SPAN);
      printf("[info] input, %s poll: input_poll %.2f us, first input_state %.2f us;"
            " press seen %ld frame(s), release %ld frame(s) after the script\n",
            name[m],
            frames ? (double)poll_ns / frames / 1000.0 : 0.0,
            frames ? (double)first_ns / frames / 1000.0 : 0.0,
            press_at[m], release_at[m]);
      CHECK(press_at[m] >= 0 && release_at[m] >= 0, "poll modes: a scripted edge never reached the core");
   }
   core_set_poll_type(POLL_TYPE_LATE);
   fast_forward(false);
   CHECK(   press_at[0] == press_at[1] && press_at[1] == press_at[2]
         && release_at[0] == release_at[1] && release_at[1] == release_at[2],
         "poll modes: an edge reaches the core in a different frame under some poll mode");
   if (failures == had)
      printf("[pass] early, normal and late polling see each edge in the same frame\n");
#else
   printf("[skip] poll-mode lane: need the test drivers and dlopen\n");
#endif
}

static void lane_frame_cost(void)
{
   unsigned had = failures;
   retro_time_t t0, t1;
   double us;

   fast_forward(true);
   run_frames(200);
   t0 = cpu_features_get_time_usec();
   run_frames(TIMED_FRAMES);
   t1 = cpu_features_get_time_usec();
   fast_forward(false);

   us = (double)(t1 - t0) / TIMED_FRAMES;
   printf("[info] %.2f us per frame, frontend only (%u frames, fast-forward, null drivers)\n",
         us, TIMED_FRAMES);
   CHECK(us < FRAME_US_LIMIT, "frame loop cost over the tripwire: something blocks or sleeps on the frame path");
   if (failures == had)
      printf("[pass] frame cost under %.0f us\n", FRAME_US_LIMIT);
}

int main(int argc, char *argv[])
{
   char cfg_path[512];
   char dir[400];
   char core_path[512];
   char *rarch_argv[8] = {0};
   int rarch_argc = 0;
   FILE *cfg;
   (void)argc;

   {
      const char *tmp = getenv("TMPDIR");
      if (!tmp || !*tmp)
         tmp = getenv("TEMP");
      if (!tmp || !*tmp)
         tmp = "/tmp";
      snprintf(dir, sizeof(dir), "%s/frontend_overhead_harness_%ld",
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
#ifdef HAVE_TEST_DRIVERS
      /* the synthetic provider: the test joypad plays a script of
       * connects for the input lanes (see PAD1_FRAME, PADS8_FRAME) */
      fprintf(cfg, "input_joypad_driver = \"test\"\n");
      fprintf(cfg, "test_input_file_joypad = \"%s/pads.ratst\"\n", dir);
      fprintf(cfg, "joypad_autoconfig_dir = \"%s\"\n", dir);
#else
      fprintf(cfg, "input_joypad_driver = \"null\"\n");
#endif
      fprintf(cfg, "menu_driver = \"rgui\"\n");
      fprintf(cfg, "video_threaded = \"false\"\n");
      fprintf(cfg, "video_vsync = \"false\"\n");
      /* Audio stays on so the batch callback is on the measured path;
       * the null sink never blocks. */
      fprintf(cfg, "audio_enable = \"true\"\n");
      fprintf(cfg, "audio_sync = \"false\"\n");
      fprintf(cfg, "fastforward_ratio = \"0.0\"\n");
      fprintf(cfg, "menu_pause_libretro = \"true\"\n");
      fprintf(cfg, "config_save_on_exit = \"false\"\n");
      fprintf(cfg, "threaded_data_runloop_enable = \"%s\"\n",
            getenv("HARNESS_THREADED_TASKS") ? "true" : "false");
      fclose(cfg);
   }

#ifdef HAVE_TEST_DRIVERS
   {
      char  path[512];
      FILE *f;
      unsigned port, b;
      snprintf(path, sizeof(path), "%s/pads.ratst", dir);
      if ((f = fopen(path, "wb")))
      {
         fprintf(f, "[\n");
         for (port = 0; port < 8; port++)
            fprintf(f, "%s{ \"action\": 1, \"param_num\": %u, "
                  "\"param_str\": \"(045e:028e) Harness pad\", \"frame\": %u }\n",
                  port ? "," : "", port, port ? PADS8_FRAME : PAD1_FRAME);
         /* pad 1's B (button 0, mask 1) pressed and released once per
          * poll mode: action 16 + pad presses, 32 + pad releases */
         for (b = 0; b < 3; b++)
            fprintf(f, ",{ \"action\": 16, \"param_num\": 1, \"frame\": %u }\n"
                  ",{ \"action\": 32, \"param_num\": 1, \"frame\": %u }\n",
                  POLL_MODE_FRAME + 1000 * b,
                  POLL_MODE_FRAME + 1000 * b + POLL_EDGE_SPAN);
         fprintf(f, "]\n");
         fclose(f);
      }
      /* a profile, so the pads read through real joypad binds */
      snprintf(path, sizeof(path), "%s/harness_pad.cfg", dir);
      if ((f = fopen(path, "wb")))
      {
         static const char *const btn[16] = { "b", "y", "select", "start",
            "up", "down", "left", "right", "a", "x", "l", "r",
            "l2", "r2", "l3", "r3" };
         fprintf(f, "input_driver = \"test\"\n");
         fprintf(f, "input_device = \"Harness pad\"\n");
         fprintf(f, "input_vendor_id = \"1118\"\n");
         fprintf(f, "input_product_id = \"654\"\n");
         for (b = 0; b < 16; b++)
            fprintf(f, "input_%s_btn = \"%u\"\n", btn[b], b);
         fclose(f);
      }
   }
#endif

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
   snprintf(core_path_g, sizeof(core_path_g), "%s", core_path);
   if (getenv("HARNESS_VERBOSE"))
      rarch_argv[rarch_argc++] = (char*)"-v";

   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }

   run_frames(5);
   CHECK(runloop_state_get_ptr()->current_core_type != CORE_TYPE_DUMMY,
         "harness core did not start (dummy core running)");
   CHECK(!menu_is_up(), "menu up after starting a core");
   if (failures)
      return 1;

   lane_frame_path_heap();
   lane_frame_cost();
   lane_input_cost();
   lane_input_poll_modes();

   if (failures)
   {
      fprintf(stderr, "%u lane(s) failed\n", failures);
      return 1;
   }
   printf("all lanes passed\n");
   return 0;
}

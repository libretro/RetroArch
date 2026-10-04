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
 *   - Where each poll mode polls (input plan, section 10). Early
 *     polls before retro_run(), normal inside input_poll, late inside
 *     the frame's first input_state - once a frame, and nowhere else.
 *     The joypad driver's poll is counted between four points of the
 *     core's retro_run() and the ends of the iterate. This is checked
 *     for each mode the core can be switched to, and the harness is
 *     run once for each mode it can be loaded with (HARNESS_LOAD_POLL):
 *     a core loaded with normal or early polling and switched to late
 *     used to keep a state callback with no poll in it, so the only
 *     poll left was the one after retro_run() and the core read input
 *     a frame old.
 *
 *   - A core's rumble calls are stored, not written (input plan,
 *     section 10: the core's call does no device I/O). The harness
 *     core sets the strong motor several times a frame and the weak
 *     one once; the joypad driver's set_rumble is counted inside
 *     retro_run() and after it. None may happen inside, and after it
 *     there is one per motor, carrying the last strength the core set.
 *
 *   - What a core sees (input plan, Phase 0: golden traces). A
 *     synthetic pad plays a fixed pattern of buttons and stick
 *     movement, and the harness core reads pad 1 every frame - once
 *     button by button, the way older cores do, and once as a single
 *     bitmask. The two readings must be the same frame for frame, and
 *     the whole sequence must match a recorded digest, under the
 *     default mapping, a remap, analog-to-d-pad, the four turbo modes,
 *     hold, and run-ahead; a recording of the pattern must play back
 *     as the same sequence; and hosting netplay with nobody connected
 *     must not change it. The mappings are run twice, reading the
 *     driver directly and reading it through the snapshot bridge
 *     (input plan, WP-03 and WP-05), and must give the same digests
 *     both ways. This is the reference the frame-local view has to reproduce: it is
 *     what the state path does today, pinned before that path changes.
 *
 *   - The device registry (input plan, WP-02) follows the drivers.
 *     Eight scripted pads connect; one is pulled and plugged back in.
 *     The registry must show each pad in its slot with its own device
 *     id, drop the pulled one, and give it the same id and a new
 *     handle when it returns. Then the joypad driver "starts over" and
 *     reports two of its controllers in each other's slots: each must
 *     be back on the port it had, and the config file must still be
 *     given what the user configured.
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
#include <rthreads/rthreads.h>
#include <retro_timers.h>
#include "../../../runloop.h"
#include "../../../runahead.h"
#ifdef HAVE_NETWORKING
#include "../../../network/netplay/netplay.h"
#endif
#include "../../../retroarch.h"
#include "../../../configuration.h"
#include "../../../command.h"
#include "../../../verbosity.h"
#include "../../../frontend/frontend_driver.h"
#include "../../../gfx/video_driver.h"
#include "../../../input/input_driver.h"
#include "../../../input/input_registry.h"
#include "../../../tasks/tasks_internal.h"
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
/* the eighth pad is pulled at this frame and plugged back in 60 later */
#define REGISTRY_FRAME   56000
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
static char harness_dir_g[400];
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

/* The registry mirrors what the joypad driver reports. */
static void lane_device_registry(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
   const input_registry_t *reg = input_driver_get_registry();
   input_device_handle_t handle7 = 0;
   uint32_t id7  = 0;
   unsigned had  = failures;
   unsigned i, j;

   /* thousands of frames to the scripted removal: do not pace them */
   fast_forward(true);

   CHECK(pad_connected(7), "registry: the scripted pads are not connected");
   CHECK(input_registry_count(reg) == 8, "registry: eight connected pads are not eight devices");
   for (i = 0; i < 8; i++)
   {
      const input_device_record_t *rec =
         input_registry_at_slot((input_registry_t*)reg, i);
      CHECK(rec != NULL, "registry: a connected pad has no device");
      if (!rec)
         continue;
      CHECK(rec->vid == 0x045e && rec->pid == 0x028e && strstr(rec->name, "Harness pad"),
            "registry: a device does not carry what its driver reported");
      CHECK(input_registry_get(reg, rec->handle) == rec,
            "registry: a present device's handle does not resolve");
      for (j = i + 1; j < 8; j++)
      {
         const input_device_record_t *other =
            input_registry_at_slot((input_registry_t*)reg, j);
         CHECK(!other || other->id != rec->id, "registry: two pads share a device id");
      }
      if (i == 7)
      {
         handle7 = rec->handle;
         id7     = rec->id;
      }
   }

   /* pad 8 is pulled */
   run_to_frame(REGISTRY_FRAME + 30);
   CHECK(!pad_connected(7), "registry: the scripted pad was not removed");
   CHECK(input_registry_count(reg) == 7, "registry: a removed pad is still a device");
   CHECK(!input_registry_get(reg, handle7), "registry: a removed pad's handle still resolves");

   /* and plugged back in */
   run_to_frame(REGISTRY_FRAME + 200);
   CHECK(pad_connected(7), "registry: the scripted pad did not come back");
   {
      const input_device_record_t *rec =
         input_registry_at_slot((input_registry_t*)reg, 7);
      CHECK(rec && rec->id == id7, "registry: a pad that came back has another device id");
      CHECK(rec && rec->handle != handle7, "registry: a new stay has the old handle");
      CHECK(!input_registry_get(reg, handle7), "registry: the old handle resolves again");
   }
   CHECK(input_registry_count(reg) == 8, "registry: not eight devices after the pad came back");

   /* A driver restart that reports the same controllers in other
    * slots. Two distinct controllers take slots 0 and 1 first. */
   {
      settings_t *settings = config_get_ptr();
      const input_device_record_t *red, *blue;
      uint32_t id_red, id_blue;
      char     path[512];
      char     line[256];
      bool     saved_as_configured = false, saved_swapped = false;
      FILE    *f;

      input_autoconfigure_connect("Red pad",  NULL, NULL, "test", 0, 0x1111, 0x0001);
      input_autoconfigure_connect("Blue pad", NULL, NULL, "test", 1, 0x1111, 0x0002);
      run_loop_frames(20);
      red     = input_registry_at_slot((input_registry_t*)reg, 0);
      blue    = input_registry_at_slot((input_registry_t*)reg, 1);
      id_red  = red  ? red->id  : 0;
      id_blue = blue ? blue->id : 0;
      CHECK(id_red && id_blue && id_red != id_blue, "registry: the two controllers were not registered");
      CHECK(   settings->uints.input_joypad_index[0] == 0
            && settings->uints.input_joypad_index[1] == 1,
            "registry: ports are not on their own slots to begin with");

      /* the driver starts over and finds them the other way round */
      input_driver_registry_restart();
      CHECK(input_registry_count(reg) == 0, "registry: a restart left controllers present");
      input_autoconfigure_connect("Blue pad", NULL, NULL, "test", 0, 0x1111, 0x0002);
      input_autoconfigure_connect("Red pad",  NULL, NULL, "test", 1, 0x1111, 0x0001);
      for (i = 2; i < 8; i++)
         input_autoconfigure_connect("Harness pad", NULL, NULL,
               "test", i, 0x045e, 0x028e);
      run_loop_frames(30);

      red  = input_registry_at_slot((input_registry_t*)reg, 1);
      blue = input_registry_at_slot((input_registry_t*)reg, 0);
      CHECK(red && red->id == id_red && blue && blue->id == id_blue,
            "registry: the controllers were not recognised in their new slots");
      CHECK(   settings->uints.input_joypad_index[0] == 1
            && settings->uints.input_joypad_index[1] == 0,
            "registry: a controller is not back on its port after the driver restarted");
      for (i = 2; i < 8; i++)
         CHECK(settings->uints.input_joypad_index[i] == i,
               "registry: a controller that did not move changed port");

      /* the config file is given what the user configured */
      CHECK(   input_config_get_saved_joypad_index(0) == 0
            && input_config_get_saved_joypad_index(1) == 1,
            "registry: a restored port would be saved to the config");
      snprintf(path, sizeof(path), "%s/saved.cfg", harness_dir_g);
      CHECK(config_save_file(path), "registry: the config could not be saved");
      if ((f = fopen(path, "rb")))
      {
         while (fgets(line, sizeof(line), f))
         {
            if (strstr(line, "input_player1_joypad_index = \"0\""))
               saved_as_configured = true;
            if (strstr(line, "input_player1_joypad_index = \"1\""))
               saved_swapped = true;
         }
         fclose(f);
      }
      CHECK(saved_as_configured && !saved_swapped,
            "registry: the saved config has the restored port, not the configured one");

      /* once the user changes the mapping, what they see is what is saved */
      settings->uints.input_joypad_index[6] = 7;
      settings->uints.input_joypad_index[7] = 6;
      CHECK(   input_config_get_saved_joypad_index(0) == 1
            && input_config_get_saved_joypad_index(6) == 7,
            "registry: a mapping the user changed is not what would be saved");

      /* back to the pads and the mapping the other lanes expect */
      for (i = 0; i < MAX_USERS; i++)
         settings->uints.input_joypad_index[i] = i;
      input_driver_registry_restart();
      for (i = 0; i < 8; i++)
         input_autoconfigure_connect("Harness pad", NULL, NULL,
               "test", i, 0x045e, 0x028e);
      run_loop_frames(30);
      CHECK(pad_connected(0) && settings->uints.input_joypad_index[0] == 0
            && settings->uints.input_joypad_index[1] == 1,
            "registry: the lane did not leave the pads as it found them");
   }

   fast_forward(false);
   if (failures == had)
      printf("[pass] registry: eight pads are eight devices; one pulled and"
            " plugged back in keeps its id; a driver restart puts each"
            " controller back on its port and the config keeps what the"
            " user set\n");
#else
   printf("[skip] registry lane: need the test drivers\n");
#endif
}

/* Where in a frame the frontend polls. The joypad driver's poll is
 * counted, and the count is read at four points of the core's
 * retro_run() and at both ends of the iterate. */
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
enum
{
   SITE_BEFORE_RUN = 0, /* before retro_run(): the early poll */
   SITE_INPUT_POLL,     /* inside input_poll: the normal poll */
   SITE_FIRST_STATE,    /* inside the first input_state: the late poll */
   SITE_REST_OF_RUN,    /* later in retro_run(): nobody's */
   SITE_AFTER_RUN,      /* after retro_run(): the fallback, too late */
   SITE_COUNT
};
static const char *const site_name[SITE_COUNT] = {
   "before retro_run", "in input_poll", "in the first input_state",
   "later in retro_run", "after retro_run" };

static unsigned long         poll_calls;
static unsigned long         site_mark;
static unsigned long         site_polls[SITE_COUNT];
static unsigned long         site_runs;
static input_device_driver_t counting_joypad;
static void                (*joypad_poll_real)(void);

static void counting_joypad_poll(void)
{
   poll_calls++;
   joypad_poll_real();
}

/* The poll's timestamp (input_driver_get_poll_time(), which the
 * statistics' input age is measured from): at which sites it moved,
 * and whether a new stamp ever lay outside the stretch of time it
 * was taken in. */
static unsigned long stamp_moves[SITE_COUNT];
static unsigned long stamp_outside;
static retro_time_t  stamp_last;
static retro_time_t  stamp_site_start;

static void site_take(unsigned site)
{
   retro_time_t now   = cpu_features_get_time_usec();
   retro_time_t stamp = input_driver_get_poll_time();

   site_polls[site] += poll_calls - site_mark;
   site_mark         = poll_calls;

   if (stamp != stamp_last)
   {
      stamp_moves[site]++;
      if (stamp < stamp_site_start || stamp > now)
         stamp_outside++;
      stamp_last = stamp;
   }
   stamp_site_start = now;
}

/* Called by the harness core from inside retro_run(). */
static void poll_probe(int point)
{
   switch (point)
   {
      case 0: site_take(SITE_BEFORE_RUN);  break;
      case 1: site_take(SITE_INPUT_POLL);  break;
      case 2: site_take(SITE_FIRST_STATE); break;
      case 3: site_take(SITE_REST_OF_RUN); site_runs++; break;
   }
}

static void run_sited_frames(unsigned n)
{
   unsigned i;
   for (i = 0; i < n; i++)
   {
      site_mark = poll_calls;
      runloop_iterate();
      site_take(SITE_AFTER_RUN);
      task_queue_check();
   }
}
#endif

static void lane_input_poll_sites(const char *loaded_with)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
   static const char *const name[3] = { "early", "normal", "late" };
   static const unsigned    home[3] = {
      SITE_BEFORE_RUN, SITE_INPUT_POLL, SITE_FIRST_STATE };
   input_driver_state_t *input_st = input_state_get_ptr();
   const input_device_driver_t *joypad_real;
   void (*set_probe)(void (*)(int));
   void (*measure)(unsigned, unsigned);
   void    *core;
   unsigned had = failures;
   unsigned m, s;

   if (   !(core = dlopen(core_path_g, RTLD_NOW))
       || !(set_probe = (void (*)(void (*)(int)))dlsym(core, "harness_core_set_probe"))
       || !(measure = (void (*)(unsigned, unsigned))dlsym(core, "harness_core_input_measure"))
       || !input_st->primary_joypad
       || !input_st->primary_joypad->poll)
   {
      CHECK(false, "poll sites: the harness core's probe or the joypad driver's poll");
      return;
   }

   /* count the joypad driver's poll: input_driver_poll() calls it once */
   joypad_real              = input_st->primary_joypad;
   counting_joypad          = *joypad_real;
   joypad_poll_real         = counting_joypad.poll;
   counting_joypad.poll     = counting_joypad_poll;
   input_st->primary_joypad = &counting_joypad;

   fast_forward(true);
   measure(16, 1);
   set_probe(poll_probe);

   /* Polls are stamped only while the statistics are shown. */
   run_sited_frames(4);
   CHECK(input_driver_get_poll_time() == 0,
         "poll stamp: taken with the statistics off");
   config_get_ptr()->bools.video_statistics_show = true;

   for (m = 0; m < 3; m++)
   {
      bool ok = true;

      core_set_poll_type(m);
      /* let the switch settle, then count */
      run_sited_frames(50);
      memset(site_polls, 0, sizeof(site_polls));
      memset(stamp_moves, 0, sizeof(stamp_moves));
      stamp_outside    = 0;
      stamp_last       = input_driver_get_poll_time();
      stamp_site_start = cpu_features_get_time_usec();
      site_runs = 0;
      run_sited_frames(300);

      /* The stamp is the poll's: it moves where this mode polls, once
       * a frame, and nowhere else - also after the mode was switched
       * while running - and each new one lies inside the stretch it
       * was taken in. (Two polls a frame apart can land on the same
       * microsecond when frames are this short, so "once a frame" is
       * held to within a few.) */
      {
         bool stamp_ok = (stamp_outside == 0)
            && stamp_moves[home[m]] <= site_runs
            && stamp_moves[home[m]] >= site_runs - site_runs / 20;
         for (s = 0; s < SITE_COUNT; s++)
            if (s != home[m] && stamp_moves[s])
               stamp_ok = false;
         if (!stamp_ok)
         {
            fprintf(stderr, "       %s polling: over %lu frames the poll stamp moved",
                  name[m], site_runs);
            for (s = 0; s < SITE_COUNT; s++)
               if (stamp_moves[s])
                  fprintf(stderr, " %lu time(s) %s;", stamp_moves[s], site_name[s]);
            fprintf(stderr, " %lu outside its stretch\n", stamp_outside);
         }
         CHECK(stamp_ok, "poll stamp: not taken where the poll is");
      }

      for (s = 0; s < SITE_COUNT; s++)
         if (site_polls[s] != (s == home[m] ? site_runs : 0))
            ok = false;

      if (!ok)
      {
         fprintf(stderr, "       loaded with %s polling, switched to %s:"
               " over %lu frames the frontend polled",
               loaded_with, name[m], site_runs);
         for (s = 0; s < SITE_COUNT; s++)
            if (site_polls[s])
               fprintf(stderr, " %lu time(s) %s;", site_polls[s], site_name[s]);
         fprintf(stderr, " expected once a frame %s and nowhere else\n",
               site_name[home[m]]);
      }
      CHECK(site_runs >= 300, "poll sites: the core did not run");
      CHECK(ok, "poll sites: a poll mode does not poll where it should");
   }

   config_get_ptr()->bools.video_statistics_show = false;
   set_probe(NULL);
   measure(0, 1);
   core_set_poll_type(POLL_TYPE_LATE);
   fast_forward(false);
   input_st->primary_joypad = joypad_real;
   if (failures == had)
      printf("[pass] loaded with %s polling: early polls before retro_run,"
            " normal in input_poll, late in the first input_state;"
            " the poll stamp is taken there\n",
            loaded_with);
#else
   (void)loaded_with;
   printf("[skip] poll-site lane: need the test drivers and dlopen\n");
#endif
}

/* A core's rumble calls: stored during retro_run(), written after it. */
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
static unsigned long         rumble_writes;
static unsigned long         rumble_mark;
static unsigned long         rumble_in_run;
static unsigned              rumble_last[2];
static input_device_driver_t rumble_joypad;

static bool counting_set_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   rumble_writes++;
   if (pad == 0 && (unsigned)effect < 2)
      rumble_last[effect] = strength;
   return true;
}

/* Called by the harness core as retro_run() ends. */
static void rumble_probe(int point)
{
   if (point == 3)
   {
      rumble_in_run += rumble_writes - rumble_mark;
      rumble_mark    = rumble_writes;
   }
}
#endif

#define RUMBLE_CALLS  5
#define RUMBLE_FRAMES 300

static void lane_output_store(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
   input_driver_state_t *input_st = input_state_get_ptr();
   const input_device_driver_t *joypad_real;
   void (*set_probe)(void (*)(int));
   void (*measure)(unsigned, unsigned);
   void (*rumble)(unsigned);
   void (*rumble_stats)(unsigned long*, unsigned long*);
   unsigned long made, answered_true, after_run = 0;
   void    *core;
   unsigned had = failures;
   unsigned i;

   if (   !(core = dlopen(core_path_g, RTLD_NOW))
       || !(set_probe = (void (*)(void (*)(int)))dlsym(core, "harness_core_set_probe"))
       || !(measure = (void (*)(unsigned, unsigned))dlsym(core, "harness_core_input_measure"))
       || !(rumble = (void (*)(unsigned))dlsym(core, "harness_core_rumble"))
       || !(rumble_stats = (void (*)(unsigned long*, unsigned long*))
             dlsym(core, "harness_core_rumble_stats"))
       || !input_st->primary_joypad)
   {
      CHECK(false, "output store: the harness core's rumble entry points or the joypad driver");
      return;
   }

   /* the test joypad has no motors: give it one that counts */
   joypad_real              = input_st->primary_joypad;
   rumble_joypad            = *joypad_real;
   rumble_joypad.set_rumble = counting_set_rumble;
   input_st->primary_joypad = &rumble_joypad;

   fast_forward(true);
   measure(16, 1);
   set_probe(rumble_probe);
   rumble(RUMBLE_CALLS);

   rumble_writes = rumble_in_run = 0;
   for (i = 0; i < RUMBLE_FRAMES; i++)
   {
      rumble_mark = rumble_writes;
      runloop_iterate();
      after_run  += rumble_writes - rumble_mark;
      task_queue_check();
   }
   rumble_stats(&made, &answered_true);

   CHECK(made == (unsigned long)RUMBLE_FRAMES * (RUMBLE_CALLS + 1),
         "output store: the core did not make its rumble calls");
   CHECK(rumble_in_run == 0,
         "output store: the driver was written from inside retro_run()");
   CHECK(after_run == (unsigned long)RUMBLE_FRAMES * 2,
         "output store: not one driver write per motor per frame");
   CHECK(   rumble_last[RETRO_RUMBLE_STRONG] == RUMBLE_CALLS * 100
         && rumble_last[RETRO_RUMBLE_WEAK]   == 50,
         "output store: the driver was not given the last strength the core set");
   CHECK(answered_true == made,
         "output store: a rumble call was answered false with a driver that rumbles");
   if (failures != had)
      fprintf(stderr, "       %lu rumble calls over %u frames: %lu driver writes"
            " inside retro_run, %lu after it; last strong %u, weak %u;"
            " %lu answered true\n",
            made, (unsigned)RUMBLE_FRAMES, rumble_in_run, after_run,
            rumble_last[RETRO_RUMBLE_STRONG], rumble_last[RETRO_RUMBLE_WEAK],
            answered_true);

   /* stopping writes zeros now and leaves nothing behind for later */
   rumble(0);
   rumble_mark = rumble_writes;
   command_event(CMD_EVENT_RUMBLE_STOP, NULL);
   CHECK(rumble_writes > rumble_mark, "output store: a stop wrote nothing");
   CHECK(   rumble_last[RETRO_RUMBLE_STRONG] == 0
         && rumble_last[RETRO_RUMBLE_WEAK]   == 0,
         "output store: a stop left a motor running");
   rumble_mark = rumble_writes;
   run_loop_frames(10);
   CHECK(rumble_writes == rumble_mark,
         "output store: a driver write with no rumble call behind it");

   set_probe(NULL);
   measure(0, 1);
   fast_forward(false);
   input_st->primary_joypad = joypad_real;
   if (failures == had)
      printf("[pass] rumble: %u calls a frame, no driver write inside retro_run,"
            " one per motor after it with the last strength\n",
            (unsigned)RUMBLE_CALLS + 1);
#else
   printf("[skip] output-store lane: need the test drivers and dlopen\n");
#endif
}

/* What a core sees of pad 1, frame by frame, from a synthetic pad. */
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
#define VIEW_FRAMES 1200
#define VIEW_SCENARIOS 11
#define VIEW_REPLAY_SLACK 64
/* Playback and netplay do not fast-forward: those two checks run at
 * the core's frame rate, so they use a shorter stretch of the pattern. */
#define VIEW_PACED_FRAMES 300

static uint32_t              syn_buttons;   /* driver buttons 0-31 */
static int16_t               syn_axes[4];   /* driver axes 0-3 */
static unsigned              syn_hat;       /* hat 0: HAT_*_MASK bits */
static input_device_driver_t syn_joypad;
/* calls into the driver, to see who reads it */
static unsigned long         syn_calls_state;
static unsigned long         syn_calls_other;

static int32_t syn_button(unsigned pad, uint16_t joykey)
{
   syn_calls_other++;
   if (pad != 0)
      return 0;
   if (GET_HAT_DIR(joykey))
      return (GET_HAT(joykey) == 0 && (syn_hat & GET_HAT_DIR(joykey))) ? 1 : 0;
   if (joykey >= 32)
      return 0;
   return (syn_buttons >> joykey) & 1;
}

static int16_t syn_axis(unsigned pad, uint32_t joyaxis)
{
   syn_calls_other++;
   if (pad != 0)
      return 0;
   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      int16_t v = syn_axes[AXIS_NEG_GET(joyaxis)];
      return (v < 0) ? v : 0;
   }
   if (AXIS_POS_GET(joyaxis) < 4)
   {
      int16_t v = syn_axes[AXIS_POS_GET(joyaxis)];
      return (v > 0) ? v : 0;
   }
   return 0;
}

static void syn_get_buttons(unsigned pad, input_bits_t *state)
{
   unsigned i;
   syn_calls_other++;
   BIT256_CLEAR_ALL_PTR(state);
   if (pad != 0)
      return;
   for (i = 0; i < 32; i++)
      if (syn_buttons & (1u << i))
         BIT256_SET_PTR(state, i);
}

/* The RetroPad mask from the binds, as every joypad driver builds it. */
static int16_t syn_state(rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds, unsigned port)
{
   unsigned i;
   int16_t  ret = 0;
   uint16_t pad = joypad_info->joy_idx;

   syn_calls_state++;
   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if ((uint16_t)joykey != NO_BTN && syn_button(pad, (uint16_t)joykey))
         ret |= (1 << i);
      else if (joyaxis != AXIS_NONE
            && ((float)abs(syn_axis(pad, joyaxis)) / 0x8000)
               > joypad_info->axis_threshold)
         ret |= (1 << i);
   }
   return ret;
}

/* The pattern: a fixed walk over the sixteen buttons and four axes,
 * with the turbo (16) and hold (17) modifiers held for stretches. */
static void syn_pattern(unsigned frame, uint32_t *rng)
{
   static const int16_t level[8] = {
      0, 32767, -32768, 12000, -9000, 3000, 20000, -20000 };

   if (frame == 0)
   {
      *rng        = 0x1234abcdu;
      syn_buttons = 0;
      syn_hat     = 0;
      memset(syn_axes, 0, sizeof(syn_axes));
   }
   *rng = *rng * 1664525u + 1013904223u;

   if (frame % 3 == 0)
      syn_buttons ^= 1u << ((*rng >> 8) & 15);
   if (frame % 5 == 0)
      syn_axes[(*rng >> 16) & 3] = level[(*rng >> 20) & 7];
   if (frame % 97 == 0)
      syn_buttons &= ~0xffffu;      /* everything up, now and then */
   if (frame % 7 == 0)
   {
      static const unsigned hat[8] = {
         0, HAT_UP_MASK, HAT_DOWN_MASK, HAT_LEFT_MASK, HAT_RIGHT_MASK,
         HAT_UP_MASK | HAT_LEFT_MASK, HAT_DOWN_MASK | HAT_RIGHT_MASK, 0 };
      syn_hat = hat[(*rng >> 24) & 7];
   }

   syn_buttons &= ~(3u << 16);
   if ((frame / 40) & 1)
      syn_buttons |= 1u << 16;      /* turbo modifier */
   if ((frame / 55) % 3 == 1)
      syn_buttons |= 1u << 17;      /* hold modifier */
}

struct view_frame
{
   unsigned buttons;
   int      axes[4];
};

static void view_record(int mode, unsigned frames, void (*trace)(int, int),
      void (*trace_last)(unsigned*, int*), struct view_frame *out)
{
   input_driver_state_t *input_st = input_state_get_ptr();
   uint32_t rng = 0;
   unsigned f;

   /* both readings start from the same turbo and hold state */
   memset(&input_st->turbo_btns, 0, sizeof(input_st->turbo_btns));
   memset(&input_st->hold_btns,  0, sizeof(input_st->hold_btns));

   trace(mode, 1);
   for (f = 0; f < frames; f++)
   {
      syn_pattern(f, &rng);
      runloop_iterate();
      task_queue_check();
      trace_last(&out[f].buttons, out[f].axes);
   }
   trace(0, 0);
}

static uint32_t view_digest(const struct view_frame *v)
{
   uint32_t h = 2166136261u;
   unsigned f, i;
   for (f = 0; f < VIEW_FRAMES; f++)
   {
      h = (h ^ (v[f].buttons & 0xffff)) * 16777619u;
      for (i = 0; i < 4; i++)
         h = (h ^ (uint32_t)(v[f].axes[i] & 0xffff)) * 16777619u;
   }
   return h;
}
#endif

static void lane_core_view(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
   /* What the state path gives a core today, per scenario. A change
    * here is a change in what cores see: update it only on purpose. */
   static const struct { const char *name; uint32_t digest; } golden[VIEW_SCENARIOS] = {
      { "default mapping",          0x9c5fa001u },
      { "A and B swapped",          0x0a584cc9u },
      { "left stick as d-pad",      0xe57635edu },
      { "turbo",                    0xf8656af0u },
      { "hold",                     0x1c41ffe0u },
      { "turbo, toggle",            0x10fe621fu },
      { "turbo, single button",     0xf9115f00u },
      { "turbo, single button hold", 0x288eb83au },
      /* run-ahead replays the frame's input to the run it shows: the
       * core must see what it sees without it */
      { "run-ahead, one frame",     0x9c5fa001u },
      { "d-pad on a hat",           0xaaea8cb1u },
      /* Mapped Port 'None': the user's pad reaches no core port */
      { "mapped to no port",        0xa6cc3885u }
   };
   static struct view_frame one_by_one[VIEW_FRAMES], as_mask[VIEW_FRAMES];
   input_driver_state_t *input_st = input_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   const input_device_driver_t *joypad_real;
   void (*trace)(int, int);
   void (*trace_last)(unsigned*, int*);
   long (*runs)(void);
   struct retro_keybind saved_auto[8], saved_dpad[4], saved_turbo, saved_hold;
   uint32_t digest_default = 0;
   void    *core;
   unsigned had = failures;
   unsigned sc, i, bridged;

   if (   !(core = dlopen(core_path_g, RTLD_NOW))
       || !(trace = (void (*)(int, int))dlsym(core, "harness_core_trace"))
       || !(trace_last = (void (*)(unsigned*, int*))dlsym(core, "harness_core_trace_last"))
       || !(runs = (long (*)(void))dlsym(core, "harness_core_runs"))
       || !input_st->primary_joypad)
   {
      CHECK(false, "core view: the harness core's trace entry points or the joypad driver");
      return;
   }
   CHECK(pad_connected(0), "core view: the scripted pad is not connected");

   /* the synthetic pad: the test driver's connection and name, with
    * buttons and axes the lane sets frame by frame */
   joypad_real              = input_st->primary_joypad;
   syn_joypad               = *joypad_real;
   syn_joypad.button        = syn_button;
   syn_joypad.axis          = syn_axis;
   syn_joypad.state         = syn_state;
   syn_joypad.get_buttons   = syn_get_buttons;
   input_st->primary_joypad = &syn_joypad;

   /* the sticks on axes 0-3; turbo and hold on buttons 16 and 17 */
   memcpy(saved_auto, &input_autoconf_binds[0][RARCH_ANALOG_LEFT_X_PLUS],
         sizeof(saved_auto));
   for (i = 0; i < 4; i++)
   {
      input_autoconf_binds[0][RARCH_ANALOG_LEFT_X_PLUS + 2 * i].joyaxis     = AXIS_POS(i);
      input_autoconf_binds[0][RARCH_ANALOG_LEFT_X_PLUS + 2 * i + 1].joyaxis = AXIS_NEG(i);
   }
   memcpy(saved_dpad, &input_autoconf_binds[0][RETRO_DEVICE_ID_JOYPAD_UP],
         sizeof(saved_dpad));
   saved_turbo = input_config_binds[0][RARCH_TURBO_ENABLE];
   saved_hold  = input_config_binds[0][RARCH_HOLD_ENABLE];

   fast_forward(true);

   /* A core's first analog read switches analog-to-d-pad off for that
    * port from the next query on. Get that behind us, so both readings
    * start from the same place. */
   trace(1, 1);
   run_loop_frames(5);
   trace(0, 0);

   /* Everything twice: read from the driver, as shipped, and read
    * through the snapshot bridge. The core must see the same. */
   for (bridged = 0; bridged < 2; bridged++)
   {
   unsigned long frames_run = 0;

   input_driver_set_snapshot_bridge(bridged != 0);
   run_loop_frames(5);
   syn_calls_state = 0;
   syn_calls_other = 0;

   for (sc = 0; sc < VIEW_SCENARIOS; sc++)
   {
      unsigned differ = 0, first = 0;
      uint32_t digest;
      long     ran;

      /* the scenario */
      switch (sc)
      {
         case 1:
            settings->uints.input_remap_ids[0][RETRO_DEVICE_ID_JOYPAD_A] = RETRO_DEVICE_ID_JOYPAD_B;
            settings->uints.input_remap_ids[0][RETRO_DEVICE_ID_JOYPAD_B] = RETRO_DEVICE_ID_JOYPAD_A;
            break;
         case 2:
            settings->uints.input_analog_dpad_mode[0] = ANALOG_DPAD_LSTICK_FORCED;
            break;
         case 3:
            settings->bools.input_turbo_enable = true;
            input_config_binds[0][RARCH_TURBO_ENABLE].joykey = 16;
            input_config_binds[0][RARCH_TURBO_ENABLE].attr  |= RETRO_KEYBIND_VALID_BIT;
            break;
         case 4:
            input_config_binds[0][RARCH_HOLD_ENABLE].joykey = 17;
            input_config_binds[0][RARCH_HOLD_ENABLE].attr  |= RETRO_KEYBIND_VALID_BIT;
            break;
         case 5:
         case 6:
         case 7:
            settings->bools.input_turbo_enable = true;
            settings->uints.input_turbo_mode   =
                 (sc == 5) ? INPUT_TURBO_MODE_CLASSIC_TOGGLE
               : (sc == 6) ? INPUT_TURBO_MODE_SINGLEBUTTON
               :             INPUT_TURBO_MODE_SINGLEBUTTON_HOLD;
            input_config_binds[0][RARCH_TURBO_ENABLE].joykey = 16;
            input_config_binds[0][RARCH_TURBO_ENABLE].attr  |= RETRO_KEYBIND_VALID_BIT;
            break;
         case 9:
            input_autoconf_binds[0][RETRO_DEVICE_ID_JOYPAD_UP].joykey    = HAT_MAP(0, HAT_UP_MASK);
            input_autoconf_binds[0][RETRO_DEVICE_ID_JOYPAD_DOWN].joykey  = HAT_MAP(0, HAT_DOWN_MASK);
            input_autoconf_binds[0][RETRO_DEVICE_ID_JOYPAD_LEFT].joykey  = HAT_MAP(0, HAT_LEFT_MASK);
            input_autoconf_binds[0][RETRO_DEVICE_ID_JOYPAD_RIGHT].joykey = HAT_MAP(0, HAT_RIGHT_MASK);
            break;
         case 10:
            settings->uints.input_remap_ports[0] = MAX_USERS;
            input_remapping_update_port_map();
            break;
         case 8:
            /* the program's own main() makes run-ahead available at
             * start-up; this harness replaces main() */
            runahead_clear_variables(runloop_state_get_ptr());
            settings->bools.run_ahead_enabled            = true;
            settings->bools.run_ahead_secondary_instance = false;
            settings->bools.run_ahead_hide_warnings      = true;
            settings->uints.run_ahead_frames             = 1;
            break;
      }
      run_loop_frames(5);

      ran = runs();
      view_record(1, VIEW_FRAMES, trace, trace_last, one_by_one);
      ran = runs() - ran;
      view_record(2, VIEW_FRAMES, trace, trace_last, as_mask);

      /* Turbo counts polls, and the count was reset as the reading
       * began: one poll a frame, run-ahead or not, and after run-ahead
       * as before it. */
      CHECK(input_st->turbo_btns.count == VIEW_FRAMES,
            "core view: input was not polled once a frame");
      if (input_st->turbo_btns.count != VIEW_FRAMES)
         fprintf(stderr, "       %s%s: %u polls in %u frames\n", golden[sc].name,
               bridged ? ", through the snapshot bridge" : "",
               (unsigned)input_st->turbo_btns.count, (unsigned)VIEW_FRAMES);

      /* run-ahead by one frame runs the core twice an iterate; if it
       * did not, this scenario tested nothing */
      if (sc == 8)
         CHECK(ran >= 2 * VIEW_FRAMES - 4, "core view: run-ahead did not run");
      else
         CHECK(ran == VIEW_FRAMES, "core view: the core did not run once a frame");

      for (i = 0; i < VIEW_FRAMES; i++)
         if (memcmp(&one_by_one[i], &as_mask[i], sizeof(one_by_one[i])))
         {
            if (!differ)
               first = i;
            differ++;
         }
      digest = view_digest(one_by_one);
      if (sc == 0)
         digest_default = digest;
      if (sc == 10)
      {
         unsigned held = 0;
         for (i = 0; i < VIEW_FRAMES; i++)
            if (one_by_one[i].buttons)
               held++;
         CHECK(!held, "core view: a user mapped to no port reached the core");
      }

      frames_run += 2 * VIEW_FRAMES;
      if (!bridged)
         printf("[info] core view, %s: digest %08x over %u frames\n",
               golden[sc].name, (unsigned)digest, (unsigned)VIEW_FRAMES);
      if (bridged && digest != golden[sc].digest)
         fprintf(stderr, "       %s, through the snapshot bridge: digest %08x\n",
               golden[sc].name, (unsigned)digest);
      if (differ)
         fprintf(stderr, "       %s: %u frame(s) differ between the two"
               " readings; frame %u read %04x button by button and %04x"
               " as a mask\n", golden[sc].name, differ, first,
               one_by_one[first].buttons, as_mask[first].buttons);
      CHECK(!differ, "core view: reading button by button and reading the mask disagree");
      CHECK(digest == golden[sc].digest, "core view: what the core sees has changed");
      if (sc > 0 && golden[sc].digest != golden[0].digest)
         CHECK(digest != digest_default, "core view: a scenario changed nothing");

      /* back to the default mapping */
      settings->uints.input_remap_ids[0][RETRO_DEVICE_ID_JOYPAD_A] = RETRO_DEVICE_ID_JOYPAD_A;
      settings->uints.input_remap_ids[0][RETRO_DEVICE_ID_JOYPAD_B] = RETRO_DEVICE_ID_JOYPAD_B;
      settings->uints.input_analog_dpad_mode[0] = ANALOG_DPAD_NONE;
      settings->bools.input_turbo_enable        = false;
      settings->uints.input_turbo_mode          = INPUT_TURBO_MODE_CLASSIC;
      settings->bools.run_ahead_enabled         = false;
      if (settings->uints.input_remap_ports[0] != 0)
      {
         settings->uints.input_remap_ports[0]   = 0;
         input_remapping_update_port_map();
      }
      memcpy(&input_autoconf_binds[0][RETRO_DEVICE_ID_JOYPAD_UP], saved_dpad,
            sizeof(saved_dpad));
      input_config_binds[0][RARCH_TURBO_ENABLE] = saved_turbo;
      input_config_binds[0][RARCH_HOLD_ENABLE]  = saved_hold;
   }

   /* Who read the driver. Through the bridge its state() is not called
    * at all, and what is called is the copy being taken. */
   printf("[info] core view, %s: %.1f calls into the driver a frame\n",
         bridged ? "through the snapshot bridge" : "read from the driver",
         frames_run ? (double)(syn_calls_state + syn_calls_other) / frames_run : 0.0);
   if (bridged)
   {
      /* Something reads every axis for a few frames, as the bind
       * screen does. Once it stops, the copy must go back to what the
       * binds use and not keep fetching sixteen axes a frame. */
      const input_device_driver_t *reader;
      unsigned long before, during, after;
      unsigned a;

      trace(1, 1);
      run_loop_frames(5);
      syn_calls_other = 0;
      run_loop_frames(20);
      before = syn_calls_other;

      syn_calls_other = 0;
      for (i = 0; i < 20; i++)
      {
         runloop_iterate();
         task_queue_check();
         reader = input_driver_joypad_for_read(input_st->primary_joypad);
         for (a = 0; a < 16; a++)
            reader->axis(0, AXIS_POS(a));
      }
      during = syn_calls_other;

      run_loop_frames(5);
      syn_calls_other = 0;
      run_loop_frames(20);
      after = syn_calls_other;
      trace(0, 0);

      CHECK(during > before, "core view: scanning every axis did not reach the driver");
      CHECK(after == before,
            "core view: the snapshot kept copying axes nobody reads any more");
      if (after != before)
         fprintf(stderr, "       driver calls over 20 frames: %lu before the"
               " scan, %lu during, %lu after\n", before, during, after);
   }
   if (bridged)
      CHECK(syn_calls_state == 0,
            "core view: the driver's state() was called through the snapshot bridge");
   else
      CHECK(syn_calls_state > 0,
            "core view: the driver's state() was not called without the bridge");
   }
   input_driver_set_snapshot_bridge(false);
   run_loop_frames(5);

#ifdef HAVE_BSV_MOVIE
   /* Input recording: record the pattern, then play the recording
    * back with the pad idle. The core must see what it saw. Recording
    * and playback start through tasks, so the two do not begin on a
    * frame the lane chooses: the played sequence is matched against
    * the recorded one at whatever offset the start left. */
   {
      static struct view_frame played[VIEW_PACED_FRAMES + VIEW_REPLAY_SLACK];
      char     path[512];
      int      offset = -1;
      unsigned f, d;

      snprintf(path, sizeof(path), "%s/core_view.replay", harness_dir_g);

      CHECK(movie_start_record(input_st, path), "core view: recording did not start");
      run_loop_frames(10);
      CHECK(BSV_MOVIE_IS_RECORDING(), "core view: not recording");
      view_record(1, VIEW_PACED_FRAMES, trace, trace_last, one_by_one);
      run_loop_frames(5);
      movie_stop(input_st);
      run_loop_frames(10);

      syn_buttons = 0;
      memset(syn_axes, 0, sizeof(syn_axes));
      CHECK(movie_start_playback(input_st, path), "core view: playback did not start");
      trace(1, 1);
      for (f = 0; f < VIEW_PACED_FRAMES + VIEW_REPLAY_SLACK; f++)
      {
         runloop_iterate();
         task_queue_check();
         trace_last(&played[f].buttons, played[f].axes);
      }
      trace(0, 0);
      movie_stop(input_st);
      run_loop_frames(10);

      for (d = 0; d < VIEW_REPLAY_SLACK && offset < 0; d++)
         if (!memcmp(&played[d], one_by_one,
                  VIEW_PACED_FRAMES * sizeof(one_by_one[0])))
            offset = (int)d;

      CHECK(offset >= 0, "core view: a recording played back is not what was recorded");
      if (offset >= 0)
         printf("[info] core view, recording: %u frames played back as"
               " recorded (%d frames in)\n", (unsigned)VIEW_PACED_FRAMES, offset);
   }
#endif

#ifdef HAVE_NETWORKING
   /* Netplay, hosting with nobody connected. The host's own input goes
    * through netplay's poll and its state callback before the core
    * reads it; the core must see what it sees without netplay. The
    * host may hold input back by its latency setting, so the sequence
    * is matched at an offset. Nothing is announced and no client
    * connects: this is the local path only. */
   {
      static struct view_frame hosted[VIEW_PACED_FRAMES + VIEW_REPLAY_SLACK];
      uint32_t rng    = 0;
      int      offset = -1;
      unsigned f, d;
      bool     up;

      bool     saved_announce = settings->bools.netplay_public_announce;
      bool     saved_nat      = settings->bools.netplay_nat_traversal;
      bool     saved_mitm     = settings->bools.netplay_use_mitm_server;
      unsigned saved_port     = settings->uints.netplay_port;

      settings->bools.netplay_public_announce = false;
      settings->bools.netplay_nat_traversal   = false;
      settings->bools.netplay_use_mitm_server = false;
      settings->uints.netplay_port            = 51963;

      netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_SERVER, NULL);
      command_event(CMD_EVENT_NETPLAY_INIT, NULL);
      run_loop_frames(10);
      up = netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL);

      /* Hosting needs a listening socket. Where the machine will not
       * give one, that is not this lane's finding. */
      if (!up)
      {
         netplay_driver_ctl(RARCH_NETPLAY_CTL_DISABLE, NULL);
         printf("[skip] core view, netplay host: could not host on this machine\n");
      }
      else
      {
         memset(&input_st->turbo_btns, 0, sizeof(input_st->turbo_btns));
         memset(&input_st->hold_btns,  0, sizeof(input_st->hold_btns));
         trace(1, 1);
         for (f = 0; f < VIEW_PACED_FRAMES + VIEW_REPLAY_SLACK; f++)
         {
            /* the pattern, then idle */
            if (f < VIEW_PACED_FRAMES)
               syn_pattern(f, &rng);
            runloop_iterate();
            task_queue_check();
            trace_last(&hosted[f].buttons, hosted[f].axes);
         }
         trace(0, 0);

         /* the reference: the same pattern with netplay off */
         command_event(CMD_EVENT_NETPLAY_DISCONNECT, NULL);
         run_loop_frames(10);
         CHECK(!netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL),
               "core view: netplay did not stop");
         view_record(1, VIEW_PACED_FRAMES, trace, trace_last, one_by_one);

         for (d = 0; d < VIEW_REPLAY_SLACK && offset < 0; d++)
            if (!memcmp(&hosted[d], one_by_one,
                     (VIEW_PACED_FRAMES - VIEW_REPLAY_SLACK) * sizeof(one_by_one[0])))
               offset = (int)d;

         CHECK(offset >= 0, "core view: hosting netplay changes what the core sees");
         if (offset >= 0)
            printf("[info] core view, netplay host: %u frames as without"
                  " netplay (%d frames late)\n",
                  (unsigned)(VIEW_PACED_FRAMES - VIEW_REPLAY_SLACK), offset);
      }

      settings->bools.netplay_public_announce = saved_announce;
      settings->bools.netplay_nat_traversal   = saved_nat;
      settings->bools.netplay_use_mitm_server = saved_mitm;
      settings->uints.netplay_port            = saved_port;
   }
#endif

   fast_forward(false);
   syn_buttons = 0;
   memset(syn_axes, 0, sizeof(syn_axes));
   memset(&input_st->turbo_btns, 0, sizeof(input_st->turbo_btns));
   memset(&input_st->hold_btns,  0, sizeof(input_st->hold_btns));
   memcpy(&input_autoconf_binds[0][RARCH_ANALOG_LEFT_X_PLUS], saved_auto,
         sizeof(saved_auto));
   input_st->primary_joypad = joypad_real;
   run_loop_frames(5);
   if (failures == had)
      printf("[pass] core view: button by button and as a mask agree, match"
            " the recorded digests read from the driver and through the"
            " snapshot bridge, play back from a recording, and are the same"
            " under netplay\n");
#else
   printf("[skip] core-view lane: need the test drivers and dlopen\n");
#endif
}

/* ---- an input driver left running across a video driver restart --- */

#define KEPT_CHECK(cond, ...) do { \
   if (!(cond)) \
   { \
      fprintf(stderr, "FAIL: "); \
      fprintf(stderr, __VA_ARGS__); \
      fprintf(stderr, "\n"); \
      failures++; \
   } \
} while (0)

#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
static input_driver_t        kept_wrap;
static input_device_driver_t kept_joy_wrap;
static void *(*kept_init_real)(const char *);
static void  (*kept_free_real)(void *);
static void  (*kept_joy_destroy_real)(void);
static unsigned kept_inits, kept_frees, kept_joy_destroys, kept_asked;
static bool     kept_answer;

/* The harness's input and video drivers are the null ones, and both
 * give the same token for their data - which the frontend reads as an
 * input driver that is the video driver's own, and does not free. The
 * wrapper has data of its own, as a real input driver does. */
static int   kept_token;
static void *kept_real_data;

static void *kept_wrap_init(const char *joypad)
{
   kept_inits++;
   if (!(kept_real_data = kept_init_real(joypad)))
      return NULL;
   return &kept_token;
}

static void kept_wrap_free(void *data)
{
   (void)data;
   kept_frees++;
   kept_free_real(kept_real_data);
}

static bool kept_wrap_survives(void *data)
{
   (void)data;
   kept_asked++;
   return kept_answer;
}

static void kept_joy_destroy(void)
{
   kept_joy_destroys++;
   kept_joy_destroy_real();
}

/* the joypad driver in use, with its destroy counted */
static void kept_wrap_joypad(input_driver_state_t *input_st)
{
   if (input_st->primary_joypad == &kept_joy_wrap)
      return;
   kept_joy_wrap            = *input_st->primary_joypad;
   kept_joy_destroy_real    = kept_joy_wrap.destroy;
   kept_joy_wrap.destroy    = kept_joy_destroy;
   input_st->primary_joypad = &kept_joy_wrap;
}
#endif

#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
/* The first user's keys, as the input driver reports them: a RetroPad
 * mask the lane sets, standing in for a keyboard. */
static uint32_t       fp_keys;
static input_driver_t fp_input;
static bool           fp_pad_there;

static bool fp_query_pad(unsigned pad) { return fp_pad_there && pad == 0; }

static int16_t fp_input_state(void *data,
      const input_device_driver_t *joypad_data,
      const input_device_driver_t *sec_joypad_data,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *retro_keybinds,
      bool keyboard_mapping_blocked,
      unsigned port, unsigned device, unsigned index, unsigned id)
{
   if (port != 0 || device != RETRO_DEVICE_JOYPAD)
      return 0;
   if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
      return (int16_t)fp_keys;
   return (id < 16) ? (int16_t)((fp_keys >> id) & 1) : 0;
}
#endif

/* First-press port assignment: with the setting on no user has a core
 * port until a button is pressed on its controller. */
static void lane_first_press(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
   input_driver_state_t *input_st = input_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   const input_device_driver_t *joypad_real;
   void (*trace)(int, int);
   void (*trace_last)(unsigned*, int*);
   void (*port_device)(unsigned, unsigned*, long*, long*);
   void    *core;
   unsigned had = failures;
   unsigned seen, device, i;
   int      axes[4];
   long     calls, calls0, in_run;
   uint32_t a_btn, b_btn;

   if (   !(core = dlopen(core_path_g, RTLD_NOW))
       || !(trace = (void (*)(int, int))dlsym(core, "harness_core_trace"))
       || !(trace_last = (void (*)(unsigned*, int*))dlsym(core, "harness_core_trace_last"))
       || !(port_device = (void (*)(unsigned, unsigned*, long*, long*))
             dlsym(core, "harness_core_port_device"))
       || !input_st->primary_joypad)
   {
      CHECK(false, "first press: the harness core's entry points or the joypad driver");
      return;
   }
   joypad_real              = input_st->primary_joypad;
   syn_joypad               = *joypad_real;
   syn_joypad.button        = syn_button;
   syn_joypad.axis          = syn_axis;
   syn_joypad.state         = syn_state;
   syn_joypad.get_buttons   = syn_get_buttons;
   input_st->primary_joypad = &syn_joypad;
   a_btn = 1u << (input_autoconf_binds[0][RETRO_DEVICE_ID_JOYPAD_A].joykey & 31);
   b_btn = 1u << (input_autoconf_binds[0][RETRO_DEVICE_ID_JOYPAD_B].joykey & 31);
   syn_hat = 0;
   memset(syn_axes, 0, sizeof(syn_axes));

   fast_forward(true);
   trace(2, 0);

   /* B is held as content starts: every user is without a core port,
    * and a button never seen released is no press */
   syn_buttons = b_btn;
   settings->bools.input_assign_ports_on_button_press = true;
   input_remapping_set_defaults(false);
   for (i = 0; i < MAX_USERS; i++)
      if (settings->uints.input_remap_ports[i] != MAX_USERS)
         break;
   CHECK(i == MAX_USERS && input_st->first_press_live,
         "first press: with the setting on a user has a core port from the start");
   run_loop_frames(5);
   trace_last(&seen, axes);
   CHECK(settings->uints.input_remap_ports[0] == MAX_USERS && !seen,
         "first press: a button held from the start was taken for a press");

   /* the d-pad is no press either */
   syn_buttons = 0;
   run_loop_frames(3);
   syn_hat = HAT_UP_MASK;
   if (!GET_HAT_DIR(input_autoconf_binds[0][RETRO_DEVICE_ID_JOYPAD_UP].joykey))
      syn_buttons = 1u << (input_autoconf_binds[0][RETRO_DEVICE_ID_JOYPAD_UP].joykey & 31);
   run_loop_frames(5);
   CHECK(settings->uints.input_remap_ports[0] == MAX_USERS,
         "first press: the d-pad was taken for a press");
   syn_hat     = 0;
   syn_buttons = 0;
   run_loop_frames(3);

   /* A: the user has core port 1 once that frame is over, and the
    * core sees the button from the next */
   port_device(0, &device, &calls0, &in_run);
   syn_buttons = a_btn;
   run_loop_frames(1);
   trace_last(&seen, axes);
   CHECK(settings->uints.input_remap_ports[0] == 0,
         "first press: a press did not give the user the first core port");
   CHECK(!seen, "first press: the core saw the press in the frame it was made in");
   run_loop_frames(1);
   trace_last(&seen, axes);
   CHECK(seen & (1u << RETRO_DEVICE_ID_JOYPAD_A),
         "first press: the core does not see the user once it has a port");
   port_device(0, &device, &calls, &in_run);
   CHECK(calls > calls0 && device == RETRO_DEVICE_JOYPAD,
         "first press: the core was not told the port's controller");
   CHECK(!in_run, "first press: the core was told a port's controller from inside its run");
   CHECK(!input_st->first_press_live || settings->uints.input_max_users > 1,
         "first press: the poll still looks for a press with every user mapped");

   /* the notification names what the user has: its controller and,
    * as the first user has keys bound, the keyboard with it */
   {
      char text[256];
      input_first_press_describe(0, 0, text, sizeof(text));
      CHECK(strstr(text, " and keyboard assigned to core port 1"),
            "first press: the notification does not name the controller and the keyboard");
      input_first_press_describe(1, 1, text, sizeof(text));
      CHECK(!strstr(text, "eyboard"),
            "first press: the notification names a keyboard for a user with no keys bound");
   }

   /* a key bound for the user gives it its port as well - the
    * keyboard and the first controller are both the first user's -
    * and is known to have been a key */
   {
      input_driver_t *input_real = input_st->current_driver;
      fp_input                   = *input_real;
      fp_input.input_state       = fp_input_state;
      input_st->current_driver   = &fp_input;
      syn_buttons                = 0;
      fp_keys                    = 0;
      input_remapping_set_defaults(false);
      run_loop_frames(3);
      fp_keys = 1u << RETRO_DEVICE_ID_JOYPAD_START;
      run_loop_frames(2);
      CHECK(settings->uints.input_remap_ports[0] == 0,
            "first press: a key did not give the user its core port");
      fp_keys                    = 0;

      /* "Waits for its Controller": the key assigns nothing while the
       * port's controller is there; the controller's button does */
      settings->uints.input_assign_ports_keyboard = 1;
      syn_joypad.query_pad       = fp_query_pad;
      fp_pad_there               = true;
      input_remapping_set_defaults(false);
      run_loop_frames(3);
      fp_keys = 1u << RETRO_DEVICE_ID_JOYPAD_START;
      run_loop_frames(3);
      CHECK(settings->uints.input_remap_ports[0] == MAX_USERS,
            "first press: a key assigned its port though the keyboard waits for the controller");
      fp_keys                    = 0;
      syn_buttons                = a_btn;
      run_loop_frames(2);
      CHECK(settings->uints.input_remap_ports[0] == 0,
            "first press: the controller's button did not assign while the keyboard waits");
      /* with no controller on the port, the key is all there is */
      syn_buttons                = 0;
      fp_pad_there               = false;
      input_remapping_set_defaults(false);
      run_loop_frames(3);
      fp_keys = 1u << RETRO_DEVICE_ID_JOYPAD_START;
      run_loop_frames(2);
      CHECK(settings->uints.input_remap_ports[0] == 0,
            "first press: with no controller on the port a key did not assign it");
      {
         char text[256];
         input_first_press_describe(0, 0, text, sizeof(text));
         CHECK(!strncmp(text, "Keyboard assigned to core port 1", 32),
               "first press: with no controller the notification does not name the keyboard alone");
      }
      fp_keys                    = 0;
      syn_joypad.query_pad       = joypad_real->query_pad;
      settings->uints.input_assign_ports_keyboard = 0;
      input_st->current_driver   = input_real;
      run_loop_frames(2);
   }

   /* the lowest free core port: with the second user on core port 1,
    * a press on the first gives it core port 2 */
   syn_buttons = 0;
   input_remapping_set_defaults(false);
   settings->uints.input_remap_ports[1] = 0;
   input_remapping_update_port_map();
   run_loop_frames(3);
   syn_buttons = a_btn;
   run_loop_frames(2);
   CHECK(settings->uints.input_max_users < 2
         || settings->uints.input_remap_ports[0] == 1,
         "first press: the user was not given the lowest free core port");

   /* a remap file saved now leaves out the port the press gave, and
    * has one the user set in the menu */
   {
      static const char *rmp = "/tmp/frontend_overhead_first_press.rmp";
      runloop_state_t *runloop_st = runloop_state_get_ptr();
      char *was_file = runloop_st->name.remapfile;
      char text[8192];
      FILE *f;
      size_t n;
      bool by_press, by_hand;

      runloop_st->name.remapfile = NULL;
      input_remapping_save_file(rmp);
      n = (f = fopen(rmp, "r")) ? fread(text, 1, sizeof(text) - 1, f) : 0;
      if (f)
         fclose(f);
      text[n]  = '\0';
      by_press = strstr(text, "input_remap_port_p1 ") != NULL;

      input_first_press_set_by_hand(0);
      input_remapping_save_file(rmp);
      n = (f = fopen(rmp, "r")) ? fread(text, 1, sizeof(text) - 1, f) : 0;
      if (f)
         fclose(f);
      text[n]  = '\0';
      by_hand  = strstr(text, "input_remap_port_p1 ") != NULL;
      remove(rmp);
      free(runloop_st->name.remapfile);
      runloop_st->name.remapfile = was_file;
      CHECK(n > 0 && !by_press,
            "first press: a saved remap file has the port a press gave");
      CHECK(n > 0 && by_hand,
            "first press: a saved remap file leaves out a port set by hand");
   }

   /* the setting is looked at when content starts: turned off now,
    * a user without a port still gets one by pressing */
   syn_buttons = 0;
   input_remapping_set_defaults(false);
   settings->bools.input_assign_ports_on_button_press = false;
   run_loop_frames(3);
   syn_buttons = a_btn;
   run_loop_frames(2);
   CHECK(settings->uints.input_remap_ports[0] == 0,
         "first press: turning the setting off took effect before content started again");
   settings->bools.input_assign_ports_on_button_press = true;

#ifdef HAVE_BSV_MOVIE
   /* a replay about to start: every user is on its own port */
   syn_buttons = 0;
   input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_START_PLAYBACK;
   input_remapping_set_defaults(false);
   input_st->bsv_movie_state.flags &= ~BSV_FLAG_MOVIE_START_PLAYBACK;
   for (i = 0; i < MAX_USERS; i++)
      if (settings->uints.input_remap_ports[i] != i)
         break;
   CHECK(i == MAX_USERS && !input_st->first_press_live,
         "first press: the policy is on with a replay about to start");

   /* one that begins with users still unmapped: the next poll gives
    * up, and each gets its own port */
   input_remapping_set_defaults(false);
   CHECK(settings->uints.input_remap_ports[0] == MAX_USERS,
         "first press: not back on after the replay");
   input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_START_PLAYBACK;
   input_driver_poll();
   input_st->bsv_movie_state.flags &= ~BSV_FLAG_MOVIE_START_PLAYBACK;
   if (input_st->first_press_pending)
      input_first_press_apply();
   for (i = 0; i < MAX_USERS; i++)
      if (settings->uints.input_remap_ports[i] != i)
         break;
   CHECK(i == MAX_USERS && !input_st->first_press_live,
         "first press: a replay that began with users unmapped did not put each on its own port");
#endif

   /* off: every user has its own port again */
   syn_buttons = 0;
   settings->bools.input_assign_ports_on_button_press = false;
   input_remapping_set_defaults(false);
   for (i = 0; i < MAX_USERS; i++)
      if (settings->uints.input_remap_ports[i] != i)
         break;
   CHECK(i == MAX_USERS && !input_st->first_press_live,
         "first press: with the setting off a user is not on its own port");
   command_event(CMD_EVENT_CONTROLLER_INIT, NULL);

   trace(0, 0);
   fast_forward(false);
   run_loop_frames(5);
   input_st->primary_joypad = joypad_real;
   dlclose(core);
   if (failures == had)
      printf("[pass] first press: no user has a core port until a button"
            " is pressed on it; a button held from the start and the d-pad"
            " are no press; the port is given between frames, the lowest"
            " free one; the notification names the controller and the"
            " keyboard that go with it; a key can be made to wait for the"
            " controller;"
            " a saved remap file leaves it out; the setting"
            " counts from content start; a replay turns it off\n");
#else
   printf("[skip] first press: needs the test drivers\n");
#endif
}

static void lane_input_kept(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
   input_driver_state_t *input_st = input_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   const input_registry_t *reg    = input_driver_get_registry();
   const input_device_record_t *rec;
   input_device_handle_t handle0;
   unsigned had = failures;
   void *data;

   fast_forward(true);
   run_frames(5);

   if (     !input_st->current_driver
         || !input_st->current_data
         || !input_st->primary_joypad
         || !(rec = input_registry_at_slot((input_registry_t*)reg, 0)))
   {
      KEPT_CHECK(false, "kept input: no input driver, joypad driver or pad to try it with");
      return;
   }
   handle0 = rec->handle;

   /* the input driver in use, with its init and free counted and a
    * say on whether it survives a video driver restart */
   kept_wrap                = *input_st->current_driver;
   kept_init_real           = kept_wrap.init;
   kept_free_real           = kept_wrap.free;
   kept_wrap.init           = kept_wrap_init;
   kept_wrap.free           = kept_wrap_free;
   kept_wrap.survives_video = kept_wrap_survives;
   kept_real_data           = input_st->current_data;
   input_st->current_driver = &kept_wrap;
   input_st->current_data   = &kept_token;
   kept_wrap_joypad(input_st);
   data                     = input_st->current_data;

   /* 1. It says it survives: a restart of the drivers leaves it, and
    *    the joypad driver, and the controllers, as they are. */
   kept_answer = true;
   command_event(CMD_EVENT_REINIT, NULL);
   run_frames(5);
   KEPT_CHECK(kept_asked >= 1, "kept input: the driver was not asked");
   KEPT_CHECK(kept_frees == 0 && kept_inits == 0 && kept_joy_destroys == 0,
         "kept input: a driver that survives was freed %u, started %u, its joypads destroyed %u time(s)",
         kept_frees, kept_inits, kept_joy_destroys);
   KEPT_CHECK(   input_st->current_driver == &kept_wrap
         && input_st->current_data   == data
         && input_st->primary_joypad == &kept_joy_wrap
         && !input_st->kept_driver,
         "kept input: the driver that survives is not the input driver after the restart");
   rec = input_registry_at_slot((input_registry_t*)reg, 0);
   KEPT_CHECK(rec && rec->handle == handle0 && pad_connected(0),
         "kept input: a controller did not come through the restart as it was");

   /* 1b. Content loaded or closed: the drivers are freed and started
    *    again at once, which is not a reset. It is kept across that
    *    too, with its joypads and controllers. */
   driver_uninit(DRIVERS_CMD_ALL, DRIVER_LIFETIME_SESSION_SWITCH);
   drivers_init(settings, DRIVERS_CMD_ALL, (enum driver_lifetime_flags)0, false);
   run_frames(5);
   KEPT_CHECK(kept_frees == 0 && kept_inits == 0 && kept_joy_destroys == 0,
         "kept input: across content loaded or closed it was freed %u, started %u, its joypads destroyed %u time(s)",
         kept_frees, kept_inits, kept_joy_destroys);
   KEPT_CHECK(   input_st->current_driver == &kept_wrap
         && input_st->current_data   == data
         && input_st->primary_joypad == &kept_joy_wrap
         && !input_st->kept_driver,
         "kept input: the driver that survives is not the input driver after content loaded or closed");
   rec = input_registry_at_slot((input_registry_t*)reg, 0);
   KEPT_CHECK(rec && rec->handle == handle0 && pad_connected(0),
         "kept input: a controller did not come through content loaded or closed as it was");

   /* 2. It says it does not: freed and started again, as always. */
   kept_answer = false;
   command_event(CMD_EVENT_REINIT, NULL);
   run_frames(5);
   KEPT_CHECK(kept_frees == 1 && kept_inits == 1 && kept_joy_destroys == 1,
         "kept input: a driver that does not survive was freed %u, started %u, its joypads destroyed %u time(s)",
         kept_frees, kept_inits, kept_joy_destroys);
   KEPT_CHECK(input_st->current_driver && input_st->current_data && input_st->primary_joypad,
         "kept input: no input driver after an ordinary restart");

   /* 3. It survives, but the joypad driver setting is not the one the
    *    joypad driver was started with: a restart is how that change
    *    is applied, so it restarts. */
   kept_wrap_joypad(input_st);
   kept_answer = true;
   strlcpy(input_st->joypad_setting_at_init, "another",
         sizeof(input_st->joypad_setting_at_init));
   command_event(CMD_EVENT_REINIT, NULL);
   run_frames(5);
   KEPT_CHECK(kept_frees == 2 && kept_inits == 2 && kept_joy_destroys == 2,
         "kept input: with the joypad setting changed it was freed %u, started %u, joypads destroyed %u time(s) in all",
         kept_frees, kept_inits, kept_joy_destroys);
   KEPT_CHECK(!strcmp(input_st->joypad_setting_at_init,
            settings->arrays.input_joypad_driver),
         "kept input: the joypad driver's setting was not noted when it started");

   /* 4. And the input driver setting: the same. */
   kept_wrap_joypad(input_st);
   {
      char saved[sizeof(settings->arrays.input_driver)];
      strlcpy(saved, settings->arrays.input_driver, sizeof(saved));
      /* the name a restart would look the driver up by; the wrapper
       * stays the driver, since it is handed on by pointer */
      strlcpy(settings->arrays.input_driver, "another",
            sizeof(settings->arrays.input_driver));
      command_event(CMD_EVENT_REINIT, NULL);
      run_frames(5);
      strlcpy(settings->arrays.input_driver, saved,
            sizeof(settings->arrays.input_driver));
   }
   KEPT_CHECK(kept_frees == 3 && kept_inits == 3,
         "kept input: with the input driver setting changed it was freed %u, started %u time(s) in all",
         kept_frees, kept_inits);

   /* 5. It survives and nothing has changed, but the drivers are shut
    *    down, not restarted: it goes. */
   kept_wrap_joypad(input_st);
   kept_answer = true;
   driver_uninit(DRIVERS_CMD_ALL, (enum driver_lifetime_flags)0);
   KEPT_CHECK(kept_frees == 4 && kept_joy_destroys == 4 && !input_st->kept_driver,
         "kept input: shut down, it was freed %u, joypads destroyed %u time(s) in all",
         kept_frees, kept_joy_destroys);
   drivers_init(settings, DRIVERS_CMD_ALL, (enum driver_lifetime_flags)0, false);
   run_frames(5);
   KEPT_CHECK(input_st->current_driver && input_st->current_data && input_st->primary_joypad
         && pad_connected(0),
         "kept input: the drivers did not come back after the shutdown");

   /* 6. Once more as in 1, after all that. */
   kept_wrap_joypad(input_st);
   data = input_st->current_data;
   command_event(CMD_EVENT_REINIT, NULL);
   run_frames(5);
   KEPT_CHECK(kept_frees == 4 && kept_inits == 4 && kept_joy_destroys == 4
         && input_st->current_data == data && pad_connected(0),
         "kept input: kept again after the others, it was freed %u, started %u time(s) in all",
         kept_frees, kept_inits);

   /* 7. The controllers' configuration was reset while running: the
    *    next restart restarts it, once, so that they are configured
    *    again; the one after keeps it as before. */
   input_driver_restart_with_next_video_restart();
   command_event(CMD_EVENT_REINIT, NULL);
   run_frames(5);
   KEPT_CHECK(kept_frees == 5 && kept_inits == 5,
         "kept input: asked to restart with the next restart, it was freed %u, started %u time(s) in all",
         kept_frees, kept_inits);
   kept_wrap_joypad(input_st);
   command_event(CMD_EVENT_REINIT, NULL);
   run_frames(5);
   KEPT_CHECK(kept_frees == 5 && kept_inits == 5 && pad_connected(0),
         "kept input: the restart after that one did not keep it (freed %u, started %u)",
         kept_frees, kept_inits);

   /* 8. Content loaded or closed (as in 1b), with a port reserved
    *    for a controller since the joypad driver started - what a
    *    config override coming or going with the content can do. Which
    *    port a controller gets is decided when the joypad driver
    *    reports it, so it restarts; and with the setting as the new
    *    driver found it, the next is kept again. */
   kept_wrap_joypad(input_st);
   {
      unsigned saved = settings->uints.input_device_reservation_type[1];
      settings->uints.input_device_reservation_type[1] = saved + 1;
      driver_uninit(DRIVERS_CMD_ALL, DRIVER_LIFETIME_SESSION_SWITCH);
      drivers_init(settings, DRIVERS_CMD_ALL, (enum driver_lifetime_flags)0, false);
      run_frames(5);
      KEPT_CHECK(kept_frees == 6 && kept_inits == 6 && kept_joy_destroys == 6,
            "kept input: with a port reservation changed it was freed %u, started %u, joypads destroyed %u time(s) in all",
            kept_frees, kept_inits, kept_joy_destroys);
      kept_wrap_joypad(input_st);
      driver_uninit(DRIVERS_CMD_ALL, DRIVER_LIFETIME_SESSION_SWITCH);
      drivers_init(settings, DRIVERS_CMD_ALL, (enum driver_lifetime_flags)0, false);
      run_frames(5);
      KEPT_CHECK(kept_frees == 6 && kept_inits == 6 && pad_connected(0),
            "kept input: with the reservation as the new driver found it, it was not kept (freed %u, started %u)",
            kept_frees, kept_inits);
      /* back as it was, by the same road */
      settings->uints.input_device_reservation_type[1] = saved;
      driver_uninit(DRIVERS_CMD_ALL, DRIVER_LIFETIME_SESSION_SWITCH);
      drivers_init(settings, DRIVERS_CMD_ALL, (enum driver_lifetime_flags)0, false);
      run_frames(5);
      KEPT_CHECK(kept_frees == 7 && kept_inits == 7,
            "kept input: with the reservation put back it was freed %u, started %u time(s) in all",
            kept_frees, kept_inits);
   }

   fast_forward(false);
   if (failures == had)
      printf("[pass] input driver across a driver restart and across content"
            " loaded or closed: kept if it says it survives, with its joypads"
            " and controllers; restarted if it does not, if a driver setting or"
            " a port reservation changed, or on a shutdown\n");
#else
   printf("[skip] kept-input lane: needs the test drivers\n");
#endif
}

/* ---- keyboard events reported from another thread ------------------ */

/* The Windows window procedure and X11's event loop report keys from
 * where the window's messages are handled, which with threaded video
 * is the video thread. input_keyboard_event() used to act on them
 * there - menu, frontend, and the core's keyboard callback, on a
 * thread the core does not run on. They wait for the poll now. This
 * reports keys from a second thread and looks at where and in what
 * order the core's callback gets them. */
#define KEYLANE_EVENTS 40

static uintptr_t keylane_main_thread;
static unsigned  keylane_got, keylane_off_thread, keylane_out_of_order;

static void RETRO_CALLCONV keylane_core_cb(bool down, unsigned keycode,
      uint32_t character, uint16_t mods)
{
   (void)mods;
   if (keycode != RETROK_F14)
      return;
   if (sthread_get_current_thread_id() != keylane_main_thread)
      keylane_off_thread++;
   /* the sender numbers them, and sends down, up, down, ... */
   if (     character != keylane_got
         || down != ((keylane_got & 1) == 0))
      keylane_out_of_order++;
   keylane_got++;
}

static void keylane_sender(void *data)
{
   unsigned i, n = *(unsigned*)data;
   bool pause    = (n == KEYLANE_EVENTS);
   for (i = 0; i < n; i++)
   {
      input_keyboard_event((i & 1) == 0, RETROK_F14, i, 0,
            RETRO_DEVICE_KEYBOARD);
      if (pause)
         retro_sleep(1);
   }
}

static void lane_key_events(void)
{
   runloop_state_t *runloop_st      = runloop_state_get_ptr();
   retro_keyboard_event_t saved     = runloop_st->key_event;
   unsigned had                     = failures;
   unsigned dropped0, n, i;
   sthread_t *thr;

   keylane_main_thread              = sthread_get_current_thread_id();
   runloop_st->key_event            = keylane_core_cb;
   fast_forward(true);
   run_frames(5);

   /* 1. Reported on this thread: acted on at once, as ever. */
   keylane_got = keylane_off_thread = keylane_out_of_order = 0;
   input_keyboard_event(true,  RETROK_F14, 0, 0, RETRO_DEVICE_KEYBOARD);
   input_keyboard_event(false, RETROK_F14, 1, 0, RETRO_DEVICE_KEYBOARD);
   CHECK(keylane_got == 2 && !keylane_out_of_order,
         "key events: one reported on the frontend's thread was not acted on at once");

   /* 2. Reported on another thread while frames run: the core gets
    *    every one, in order, and on its own thread. */
   keylane_got = keylane_off_thread = keylane_out_of_order = 0;
   dropped0    = input_driver_key_events_dropped();
   n           = KEYLANE_EVENTS;
   thr         = sthread_create(keylane_sender, &n);
   CHECK(thr != NULL, "key events: no second thread to report from");
   if (thr)
   {
      for (i = 0; i < 400 && keylane_got < KEYLANE_EVENTS; i++)
      {
         run_frames(1);
         retro_sleep(1);
      }
      sthread_join(thr);
      run_frames(2);
   }
   if (keylane_got != KEYLANE_EVENTS || keylane_off_thread || keylane_out_of_order)
      fprintf(stderr, "       %u of %u reached the core, %u on another thread, %u out of order\n",
            keylane_got, KEYLANE_EVENTS, keylane_off_thread, keylane_out_of_order);
   CHECK(keylane_got == KEYLANE_EVENTS,
         "key events: reported on another thread, not all reached the core");
   CHECK(keylane_off_thread == 0,
         "key events: the core's keyboard callback was called on a thread that is not the core's");
   CHECK(keylane_out_of_order == 0,
         "key events: reported on another thread, they reached the core out of order");
   CHECK(input_driver_key_events_dropped() == dropped0,
         "key events: some were dropped with a poll every frame");

   /* 3. A hundred with no poll in between: the queue holds
    *    sixty-four, the rest are dropped and counted, and nothing is
    *    acted on until the poll. */
   keylane_got = keylane_off_thread = keylane_out_of_order = 0;
   n           = 100;
   thr         = sthread_create(keylane_sender, &n);
   if (thr)
   {
      sthread_join(thr);
      CHECK(keylane_got == 0,
            "key events: reported on another thread, acted on before the poll");
      run_frames(2);
      if (keylane_got != 64 || input_driver_key_events_dropped() - dropped0 != 36)
         fprintf(stderr, "       %u reached the core, %u dropped\n", keylane_got,
               input_driver_key_events_dropped() - dropped0);
      CHECK(keylane_got == 64 && !keylane_off_thread && !keylane_out_of_order,
            "key events: a full queue did not give the core its sixty-four, in order");
      CHECK(input_driver_key_events_dropped() - dropped0 == 36,
            "key events: those that did not fit were not counted");
   }

   runloop_st->key_event = saved;
   fast_forward(false);
   run_frames(2);
   if (failures == had)
      printf("[pass] key events: from the frontend's thread acted on at once;"
            " from another, the core gets all of them in order on its own thread"
            " at the next poll; a full queue drops the newest and counts them\n");
}

/* ---- a joypad driver restart asked for from another thread --------- */

/* On Windows a controller plugged in or pulled reaches the frontend
 * through the window procedure, which with threaded video runs on the
 * video thread, and what it asks for is joypad_driver_reinit(): the
 * joypad driver destroyed and started again. That used to happen then
 * and there, on that thread, under a frontend that might be polling
 * the driver. It waits for the poll now. This asks from a second
 * thread and looks at when, and on which thread, the driver is
 * destroyed. */
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
static input_device_driver_t jr_wrap;
static void (*jr_destroy_real)(void);
static uintptr_t jr_main_thread;
static unsigned  jr_destroys, jr_off_thread;

static void jr_destroy(void)
{
   jr_destroys++;
   if (sthread_get_current_thread_id() != jr_main_thread)
      jr_off_thread++;
   jr_destroy_real();
}

static void jr_wrap_joypad(input_driver_state_t *input_st)
{
   jr_wrap                  = *input_st->primary_joypad;
   jr_destroy_real          = jr_wrap.destroy;
   jr_wrap.destroy          = jr_destroy;
   input_st->primary_joypad = &jr_wrap;
}

static void jr_sender(void *data)
{
   unsigned i, n = *(unsigned*)data;
   for (i = 0; i < n; i++)
      joypad_driver_reinit(NULL, config_get_ptr()->arrays.input_joypad_driver);
}
#endif

static void lane_joypad_reinit(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
   input_driver_state_t *input_st = input_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   unsigned had                   = failures;
   unsigned n;
   sthread_t *thr;

   fast_forward(true);
   run_frames(5);
   if (!input_st->primary_joypad)
   {
      CHECK(false, "joypad restart: no joypad driver to try it with");
      return;
   }
   jr_main_thread = sthread_get_current_thread_id();
   jr_destroys    = jr_off_thread = 0;

   /* 1. Asked for on another thread: nothing happens there. The next
    *    poll restarts the driver, on the frontend's thread. */
   jr_wrap_joypad(input_st);
   n   = 1;
   thr = sthread_create(jr_sender, &n);
   CHECK(thr != NULL, "joypad restart: no second thread to ask from");
   if (thr)
   {
      sthread_join(thr);
      CHECK(jr_destroys == 0 && input_st->primary_joypad == &jr_wrap,
            "joypad restart: asked for on another thread, it was done there and then");
      run_frames(2);
      CHECK(jr_destroys == 1 && jr_off_thread == 0,
            "joypad restart: the poll did not do it once, on the frontend's thread");
      CHECK(input_st->primary_joypad && input_st->primary_joypad != &jr_wrap,
            "joypad restart: no joypad driver after it");
   }

   /* 2. Asked for three times before a poll comes: one restart. */
   jr_wrap_joypad(input_st);
   n   = 3;
   thr = sthread_create(jr_sender, &n);
   if (thr)
   {
      sthread_join(thr);
      run_frames(3);
      CHECK(jr_destroys == 2 && jr_off_thread == 0,
            "joypad restart: three requests between two polls were not one restart");
   }

   /* 3. Asked for on the frontend's own thread: at once, as ever. */
   jr_wrap_joypad(input_st);
   joypad_driver_reinit(NULL, settings->arrays.input_joypad_driver);
   CHECK(jr_destroys == 3 && jr_off_thread == 0
         && input_st->primary_joypad && input_st->primary_joypad != &jr_wrap,
         "joypad restart: asked for on the frontend's thread, it was not done at once");

   run_frames(3);
   fast_forward(false);
   if (failures == had)
      printf("[pass] joypad restart: asked for on another thread it waits for the"
            " poll and is done once on the frontend's thread; on the frontend's"
            " thread it is done at once\n");
#else
   printf("[skip] joypad-restart lane: needs the test drivers\n");
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
   /* HARNESS_LOAD_POLL=early|normal|late: the poll mode the core is
    * loaded with. The state callback used to be chosen then. */
   static const char *const load_poll_name[3] = { "early", "normal", "late" };
   const char *load_poll_env = getenv("HARNESS_LOAD_POLL");
   unsigned    load_poll     = POLL_TYPE_LATE;
   (void)argc;

   if (load_poll_env)
   {
      for (load_poll = 0; load_poll < 3; load_poll++)
         if (!strcmp(load_poll_env, load_poll_name[load_poll]))
            break;
      if (load_poll == 3)
      {
         fprintf(stderr, "HARNESS_LOAD_POLL: early, normal or late\n");
         return 1;
      }
   }

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
      fprintf(cfg, "input_poll_type_behavior = \"%u\"\n", load_poll);
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
         /* pad 8 leaves and comes back: action 2 removes */
         fprintf(f, ",{ \"action\": 2, \"param_num\": 7, \"frame\": %u }\n"
               ",{ \"action\": 1, \"param_num\": 7, "
               "\"param_str\": \"(045e:028e) Harness pad\", \"frame\": %u }\n",
               REGISTRY_FRAME, REGISTRY_FRAME + 60);
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
   snprintf(harness_dir_g, sizeof(harness_dir_g), "%s", dir);
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

   /* HARNESS_SNAPSHOT=1 runs every lane through the snapshot bridge,
    * to see what it costs; the core-view lane does both regardless. */
   if (getenv("HARNESS_SNAPSHOT"))
      input_driver_set_snapshot_bridge(true);

   /* Loaded with another poll mode than the default, only the lane
    * that depends on it runs: the rest was measured in the default
    * run. */
   if (load_poll_env)
      lane_input_poll_sites(load_poll_name[load_poll]);
   else
   {
      lane_frame_path_heap();
      lane_frame_cost();
      lane_input_cost();
      lane_input_poll_modes();
      lane_device_registry();
      lane_input_poll_sites(load_poll_name[load_poll]);
      lane_output_store();
      /* before the core-view lane, whose netplay check leaves the
       * loop paused */
      lane_first_press();
      lane_core_view();
      lane_key_events();
      /* last: these restart the drivers */
      lane_input_kept();
      lane_joypad_reinit();
   }

   if (failures)
   {
      fprintf(stderr, "%u lane(s) failed\n", failures);
      return 1;
   }
   printf("all lanes passed\n");
   return 0;
}

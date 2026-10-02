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

static void site_take(unsigned site)
{
   site_polls[site] += poll_calls - site_mark;
   site_mark         = poll_calls;
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

   for (m = 0; m < 3; m++)
   {
      bool ok = true;

      core_set_poll_type(m);
      /* let the switch settle, then count */
      run_sited_frames(50);
      memset(site_polls, 0, sizeof(site_polls));
      site_runs = 0;
      run_sited_frames(300);

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

   set_probe(NULL);
   measure(0, 1);
   core_set_poll_type(POLL_TYPE_LATE);
   fast_forward(false);
   input_st->primary_joypad = joypad_real;
   if (failures == had)
      printf("[pass] loaded with %s polling: early polls before retro_run,"
            " normal in input_poll, late in the first input_state\n",
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
#define VIEW_SCENARIOS 10
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
      { "d-pad on a hat",           0xaaea8cb1u }
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
      lane_core_view();
   }

   if (failures)
   {
      fprintf(stderr, "%u lane(s) failed\n", failures);
      return 1;
   }
   printf("all lanes passed\n");
   return 0;
}

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
#include <sys/stat.h>
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
#include "../../../content.h"
#include "../../../tasks/task_content.h"
#include "../../../verbosity.h"
#include "../../../frontend/frontend_driver.h"
#include "../../../gfx/video_driver.h"
#include "../../../input/input_driver.h"
#include "../../../input/input_registry.h"
#include "../../../input/input_remapping.h"
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
/* The lanes set binds by writing the arrays, which nothing in the
 * frontend does: there every change to a bind is counted
 * (input_config_binds_changed()), and what is compiled from the binds
 * goes by the count. So that a lane's write is seen, the arrays are
 * looked at here and a change in them is counted on the lane's behalf. */
#define LANE_SHARED_A 12
#define LANE_SHARED_B 13
static void binds_written_by_a_lane(void)
{
   static uint32_t last;
   static bool own;
   uint32_t h             = 2166136261u;
   const unsigned char *p;
   size_t i;
   unsigned user;
   /* The lanes write binds where they are kept, as nothing in the
    * program may: a row is shared until input_config_bind_edit() or
    * input_autoconf_bind_edit() gives its user their own. So every
    * user and port gets a row of their own here, once - but for two,
    * LANE_SHARED_A and LANE_SHARED_B, which no lane writes directly
    * and the lane on sharing keeps as it finds them. */
   if (!own)
   {
      own = true;
      for (user = 0; user < MAX_USERS; user++)
      {
         if (user == LANE_SHARED_A || user == LANE_SHARED_B)
            continue;
         (void)input_config_bind_edit(user, 0);
         (void)input_autoconf_bind_edit(user, 0);
      }
   }
   for (user = 0; user < MAX_USERS; user++)
   {
      p = (const unsigned char*)input_config_binds[user];
      for (i = 0; i < sizeof(retro_keybind_set); i++)
         h = (h ^ p[i]) * 16777619u;
      p = (const unsigned char*)input_autoconf_binds[user];
      for (i = 0; i < sizeof(retro_keybind_set); i++)
         h = (h ^ p[i]) * 16777619u;
   }
   if (h != last)
   {
      last = h;
      input_config_binds_changed();
   }
}

static void run_loop_frames(unsigned n)
{
   unsigned i;
   binds_written_by_a_lane();
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

static unsigned syn_pad_index; /* the pad the stand-in is */

static int32_t syn_button(unsigned pad, uint16_t joykey)
{
   syn_calls_other++;
   if (pad != syn_pad_index)
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
   if (pad != syn_pad_index)
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
   if (pad != syn_pad_index)
      return;
   for (i = 0; i < 32; i++)
      if (syn_buttons & (1u << i))
         BIT256_SET_PTR(state, i);
}

/* The RetroPad mask from the binds, as every joypad driver builds it. */
static int16_t syn_state(rarch_joypad_info_t *joypad_info,
      const uint16_t *joykeys, const uint32_t *joyaxes, unsigned port)
{
   unsigned i;
   int16_t  ret = 0;
   uint16_t pad = joypad_info->joy_idx;

   syn_calls_state++;
   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      const uint64_t joykey  = joykeys[i];
      const uint32_t joyaxis = joyaxes[i];
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
            input_config_set_remap_id(0, RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_B);
            input_config_set_remap_id(0, RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_A);
            break;
         case 2:
            settings->uints.input_analog_dpad_mode[0] = ANALOG_DPAD_LSTICK_FORCED;
            break;
         case 3:
            settings->bools.input_turbo_enable = true;
            input_config_binds[0][RARCH_TURBO_ENABLE].joykey = 16;
            input_config_binds[0][RARCH_TURBO_ENABLE].attr  |= RETRO_KEYBIND_ATTR_VALID_BIT;
            break;
         case 4:
            input_config_binds[0][RARCH_HOLD_ENABLE].joykey = 17;
            input_config_binds[0][RARCH_HOLD_ENABLE].attr  |= RETRO_KEYBIND_ATTR_VALID_BIT;
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
            input_config_binds[0][RARCH_TURBO_ENABLE].attr  |= RETRO_KEYBIND_ATTR_VALID_BIT;
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
      input_config_set_remap_id(0, RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_A);
      input_config_set_remap_id(0, RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_B);
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
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *retro_keybinds,
      unsigned port, unsigned device, unsigned index, unsigned id)
{
   if (port != 0 || device != RETRO_DEVICE_JOYPAD)
      return 0;
   if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
      return (int16_t)fp_keys;
   return (id < 16) ? (int16_t)((fp_keys >> id) & 1) : 0;
}
#endif

#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
/* SOCD lane: holds the D-Pad directions in @dirs (RetroPad button
 * bits) on the scripted pad for a frame; returns what the core saw of
 * the D-Pad. */
static unsigned socd_frame(unsigned dirs, void (*trace_last)(unsigned*, int*))
{
   static const unsigned id[4] = {
      RETRO_DEVICE_ID_JOYPAD_UP, RETRO_DEVICE_ID_JOYPAD_DOWN,
      RETRO_DEVICE_ID_JOYPAD_LEFT, RETRO_DEVICE_ID_JOYPAD_RIGHT };
   unsigned seen, i;
   int axes[4];

   syn_buttons = 0;
   for (i = 0; i < 4; i++)
      if (dirs & (1u << id[i]))
         syn_buttons |= 1u << (input_autoconf_binds[0][id[i]].joykey & 31);
   run_loop_frames(1);
   trace_last(&seen, axes);
   return seen & 0xf0;
}
#endif

/* SOCD cleaning: what a core sees of opposite D-Pad directions held
 * together, per axis and per mode. */
static void lane_socd(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
   const unsigned U = 1u << RETRO_DEVICE_ID_JOYPAD_UP;
   const unsigned D = 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
   const unsigned L = 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
   const unsigned R = 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
   input_driver_state_t *input_st = input_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   const input_device_driver_t *joypad_real;
   void (*trace)(int, int);
   void (*trace_last)(unsigned*, int*);
   void    *core;
   unsigned had = failures;
   unsigned mode, reading;

   if (   !(core = dlopen(core_path_g, RTLD_NOW))
       || !(trace = (void (*)(int, int))dlsym(core, "harness_core_trace"))
       || !(trace_last = (void (*)(unsigned*, int*))dlsym(core, "harness_core_trace_last"))
       || !input_st->primary_joypad)
   {
      CHECK(false, "SOCD: the harness core's trace entry points or the joypad driver");
      return;
   }
   joypad_real              = input_st->primary_joypad;
   syn_joypad               = *joypad_real;
   syn_joypad.button        = syn_button;
   syn_joypad.axis          = syn_axis;
   syn_joypad.state         = syn_state;
   syn_joypad.get_buttons   = syn_get_buttons;
   input_st->primary_joypad = &syn_joypad;
   syn_hat = 0;
   memset(syn_axes, 0, sizeof(syn_axes));
   fast_forward(true);

   /* a core that asks for the mask, and one that reads button by
    * button, see the same */
   for (reading = 2; reading >= 1; reading--)
   {
      trace((int)reading, 0);

      /* off: both, as before */
      settings->uints.input_socd_horizontal = INPUT_SOCD_OFF;
      settings->uints.input_socd_vertical   = INPUT_SOCD_OFF;
      socd_frame(0, trace_last);
      CHECK(socd_frame(L | R, trace_last) == (L | R)
            && socd_frame(U | D, trace_last) == (U | D),
            "SOCD: off, opposite directions do not both reach the core");

      for (mode = INPUT_SOCD_NEUTRAL; mode <= INPUT_SOCD_FIRST; mode++)
      {
         /* left, then right as well; left let go; left again */
         unsigned both1 = (mode == INPUT_SOCD_NEUTRAL) ? 0
                        : (mode == INPUT_SOCD_LAST)    ? R : L;
         unsigned both2 = (mode == INPUT_SOCD_NEUTRAL) ? 0
                        : (mode == INPUT_SOCD_LAST)    ? L : R;
         settings->uints.input_socd_horizontal = mode;
         settings->uints.input_socd_vertical   = INPUT_SOCD_OFF;
         socd_frame(0, trace_last);
         CHECK(socd_frame(L, trace_last) == L, "SOCD: one direction alone is changed");
         CHECK(socd_frame(L | R, trace_last) == both1,
               "SOCD: left, then right as well, is not what the mode says");
         CHECK(socd_frame(L | R, trace_last) == both1,
               "SOCD: the answer changed while both stayed held");
         CHECK(socd_frame(R, trace_last) == R, "SOCD: right alone after both");
         CHECK(socd_frame(L | R, trace_last) == both2,
               "SOCD: right, then left as well, is not what the mode says");
         /* both at once: neutral in every mode */
         socd_frame(0, trace_last);
         CHECK(socd_frame(L | R, trace_last) == 0,
               "SOCD: both directions pressed in the same frame are not neutral");
         /* the other axis is left alone */
         socd_frame(0, trace_last);
         CHECK(socd_frame(U | D | L | R, trace_last) == (U | D),
               "SOCD: cleaning left and right touched up and down");
      }

      /* up and down: its own setting, and up priority */
      settings->uints.input_socd_horizontal = INPUT_SOCD_OFF;
      settings->uints.input_socd_vertical   = INPUT_SOCD_UP;
      socd_frame(0, trace_last);
      CHECK(socd_frame(D, trace_last) == D
            && socd_frame(U | D, trace_last) == U,
            "SOCD: up priority, down then up as well is not up");
      socd_frame(0, trace_last);
      CHECK(socd_frame(U, trace_last) == U
            && socd_frame(U | D, trace_last) == U
            && socd_frame(U | D | L | R, trace_last) == (U | L | R),
            "SOCD: up priority, up then down as well is not up, or left and right were touched");
      settings->uints.input_socd_vertical   = INPUT_SOCD_LAST;
      socd_frame(0, trace_last);
      socd_frame(U, trace_last);
      CHECK(socd_frame(U | D, trace_last) == D,
            "SOCD: last input priority on up and down");
   }

   settings->uints.input_socd_horizontal = INPUT_SOCD_OFF;
   settings->uints.input_socd_vertical   = INPUT_SOCD_OFF;
   syn_buttons = 0;
   trace(0, 0);
   fast_forward(false);
   run_loop_frames(5);
   input_st->primary_joypad = joypad_real;
   dlclose(core);
   if (failures == had)
      printf("[pass] SOCD: off, opposite directions both reach the core;"
            " neutral, last and first input priority, and up priority do"
            " what they say, per axis, whichever way the core reads\n");
#else
   printf("[skip] SOCD: needs the test drivers\n");
#endif
}

/* A user whose controls are all mapped to themselves costs the poll
 * no remap work, and the core sees exactly what it did: a control
 * mapped to another still arrives as the other, one switched off does
 * not arrive, and taking the mapping away again leaves nothing of it
 * behind. */
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
static unsigned remap_frame(unsigned id, void (*trace_last)(unsigned*, int*))
{
   unsigned seen;
   int axes[4];
   syn_buttons = 1u << (input_autoconf_binds[0][id].joykey & 31);
   run_loop_frames(1);
   trace_last(&seen, axes);
   syn_buttons = 0;
   return seen;
}
#endif

static void lane_remap_idle(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
   const unsigned B = RETRO_DEVICE_ID_JOYPAD_B;
   const unsigned A = RETRO_DEVICE_ID_JOYPAD_A;
   input_driver_state_t *input_st = input_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   const input_device_driver_t *joypad_real;
   void (*trace)(int, int);
   void (*trace_last)(unsigned*, int*);
   void    *core;
   unsigned had = failures;
   unsigned reading, was;

   if (   !(core = dlopen(core_path_g, RTLD_NOW))
       || !(trace = (void (*)(int, int))dlsym(core, "harness_core_trace"))
       || !(trace_last = (void (*)(unsigned*, int*))dlsym(core, "harness_core_trace_last"))
       || !input_st->primary_joypad)
   {
      CHECK(false, "remap idle: the harness core's trace entry points or the joypad driver");
      return;
   }
   joypad_real              = input_st->primary_joypad;
   syn_joypad               = *joypad_real;
   syn_joypad.button        = syn_button;
   syn_joypad.axis          = syn_axis;
   syn_joypad.state         = syn_state;
   syn_joypad.get_buttons   = syn_get_buttons;
   input_st->primary_joypad = &syn_joypad;
   syn_hat = 0;
   memset(syn_axes, 0, sizeof(syn_axes));
   fast_forward(true);
   was = settings->uints.input_remap_ids[0][B];

   for (reading = 2; reading >= 1; reading--)
   {
      trace((int)reading, 0);

      /* nothing mapped: B is B, and the poll did no remap work */
      input_config_set_remap_id(0, B, B);
      run_loop_frames(2);
      CHECK(remap_frame(B, trace_last) == (1u << B),
            "remap idle: with nothing mapped, B does not reach the core as B");
      CHECK(!(input_st->remap_worked & 1u),
            "remap idle: with nothing mapped the poll still did the first user's remap work");

      /* B given to A */
      input_config_set_remap_id(0, B, A);
      CHECK(remap_frame(B, trace_last) == (1u << A),
            "remap idle: B mapped to A does not reach the core as A alone");
      CHECK(input_st->remap_worked & 1u,
            "remap idle: with B mapped to A the poll did no remap work");

      /* B switched off */
      input_config_set_remap_id(0, B, RARCH_UNMAPPED);
      CHECK(remap_frame(B, trace_last) == 0,
            "remap idle: B switched off still reaches the core");

      /* and back: nothing of the mapping is left */
      input_config_set_remap_id(0, B, A);
      syn_buttons = 1u << (input_autoconf_binds[0][B].joykey & 31);
      run_loop_frames(1);
      input_config_set_remap_id(0, B, B);
      CHECK(remap_frame(B, trace_last) == (1u << B),
            "remap idle: after the mapping was taken away, B is not B again, or A is still held");
      CHECK(!(input_st->remap_worked & 1u),
            "remap idle: after the mapping was taken away the poll kept doing remap work");
   }

   input_config_set_remap_id(0, B, was);
   syn_buttons = 0;
   trace(0, 0);
   fast_forward(false);
   run_loop_frames(5);
   input_st->primary_joypad = joypad_real;
   dlclose(core);
   if (failures == had)
      printf("[pass] remap idle: a user with nothing mapped costs the poll no"
            " remap work; mapped, switched off and mapped back, the core sees"
            " what it should, whichever way it reads\n");
#else
   printf("[skip] remap idle: needs the test drivers\n");
#endif
}

/* Input rotation: the D-Pad and the sticks a core sees, turned by
 * quarter turns. */
static void lane_input_rotation(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
   const unsigned U = 1u << RETRO_DEVICE_ID_JOYPAD_UP;
   const unsigned D = 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
   const unsigned L = 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
   const unsigned R = 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
   /* what up, right, down and left on the controller give, by turns */
   const unsigned want[4][4] = {
      { U, R, D, L }, { R, D, L, U }, { D, L, U, R }, { L, U, R, D } };
   input_driver_state_t *input_st = input_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   const input_device_driver_t *joypad_real;
   struct retro_keybind saved_auto[8];
   void (*trace)(int, int);
   void (*trace_last)(unsigned*, int*);
   void    *core;
   unsigned had = failures;
   unsigned turns, reading, seen, i;
   int      axes[4];

   if (   !(core = dlopen(core_path_g, RTLD_NOW))
       || !(trace = (void (*)(int, int))dlsym(core, "harness_core_trace"))
       || !(trace_last = (void (*)(unsigned*, int*))dlsym(core, "harness_core_trace_last"))
       || !input_st->primary_joypad)
   {
      CHECK(false, "rotation: the harness core's trace entry points or the joypad driver");
      return;
   }
   joypad_real              = input_st->primary_joypad;
   syn_joypad               = *joypad_real;
   syn_joypad.button        = syn_button;
   syn_joypad.axis          = syn_axis;
   syn_joypad.state         = syn_state;
   syn_joypad.get_buttons   = syn_get_buttons;
   input_st->primary_joypad = &syn_joypad;
   syn_hat = 0;
   memset(syn_axes, 0, sizeof(syn_axes));
   /* the sticks on axes 0-3 */
   memcpy(saved_auto, &input_autoconf_binds[0][RARCH_ANALOG_LEFT_X_PLUS],
         sizeof(saved_auto));
   for (i = 0; i < 4; i++)
   {
      input_autoconf_binds[0][RARCH_ANALOG_LEFT_X_PLUS + 2 * i].joyaxis     = AXIS_POS(i);
      input_autoconf_binds[0][RARCH_ANALOG_LEFT_X_PLUS + 2 * i + 1].joyaxis = AXIS_NEG(i);
   }
   fast_forward(true);
   /* a core's first analog read switches analog-to-d-pad off */
   trace(1, 1);
   run_loop_frames(3);

   for (reading = 2; reading >= 1; reading--)
   {
      trace((int)reading, 1);
      for (turns = 0; turns < 4; turns++)
      {
         settings->uints.input_rotation = turns;
         CHECK(   socd_frame(U, trace_last) == want[turns][0]
               && socd_frame(R, trace_last) == want[turns][1]
               && socd_frame(D, trace_last) == want[turns][2]
               && socd_frame(L, trace_last) == want[turns][3],
               "rotation: a D-Pad direction is not turned as the setting says");
         CHECK(socd_frame(U | R, trace_last) == (want[turns][0] | want[turns][1]),
               "rotation: a diagonal is not turned as the setting says");

         /* the left stick pushed right and a little up, the right
          * stick pushed down: both turn as the D-Pad does */
         syn_buttons = 0;
         syn_axes[0] = 20000;
         syn_axes[1] = -5000;
         syn_axes[2] = 0;
         syn_axes[3] = 12000;
         run_loop_frames(1);
         trace_last(&seen, axes);
         memset(syn_axes, 0, sizeof(syn_axes));
         switch (turns)
         {
            case 0:
               CHECK(axes[0] == 20000 && axes[1] == -5000 && axes[2] == 0 && axes[3] == 12000,
                     "rotation: off, a stick is changed");
               break;
            case 1:
               CHECK(axes[0] == 5000 && axes[1] == 20000 && axes[2] == -12000 && axes[3] == 0,
                     "rotation: a quarter turn of the sticks");
               break;
            case 2:
               CHECK(axes[0] == -20000 && axes[1] == 5000 && axes[2] == 0 && axes[3] == -12000,
                     "rotation: a half turn of the sticks");
               break;
            case 3:
               CHECK(axes[0] == -5000 && axes[1] == -20000 && axes[2] == 12000 && axes[3] == 0,
                     "rotation: three quarter turns of the sticks");
               break;
         }
         run_loop_frames(1);
      }

      /* 'Auto' is what Video Rotation is set to */
      settings->uints.input_rotation = INPUT_ROTATION_AUTO;
      settings->uints.video_rotation = 3;
      CHECK(socd_frame(U, trace_last) == want[3][0],
            "rotation: 'Auto' does not follow Video Rotation");
      settings->uints.video_rotation = 0;
      CHECK(socd_frame(U, trace_last) == U,
            "rotation: 'Auto' turns with Video Rotation at none");

      /* turned first, cleaned after: up and down on the controller
       * are left and right in the game */
      settings->uints.input_rotation        = 1;
      settings->uints.input_socd_horizontal = INPUT_SOCD_NEUTRAL;
      socd_frame(0, trace_last);
      CHECK(socd_frame(U | D, trace_last) == 0
            && socd_frame(L | R, trace_last) == (U | D),
            "rotation: SOCD cleaning is not done on the turned D-Pad");
      settings->uints.input_socd_horizontal = INPUT_SOCD_OFF;
      settings->uints.input_rotation        = 0;
   }

   syn_buttons = 0;
   trace(0, 0);
   fast_forward(false);
   run_loop_frames(5);
   memcpy(&input_autoconf_binds[0][RARCH_ANALOG_LEFT_X_PLUS], saved_auto,
         sizeof(saved_auto));
   input_st->primary_joypad = joypad_real;
   dlclose(core);
   if (failures == had)
      printf("[pass] input rotation: the D-Pad and both sticks turn by"
            " quarter turns as set; 'Auto' follows Video Rotation; SOCD"
            " cleaning is done on the turned D-Pad\n");
#else
   printf("[skip] input rotation: needs the test drivers\n");
#endif
}

#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
/* Trigger lane: axes 2 and 3 of the scripted pad at @a2 and @a3 for a
 * frame; what the core saw of L2 and R2 as buttons, and L2's and R2's
 * analog values. */
static unsigned trig_frame(int a2, int a3, void (*trace_last)(unsigned*, int*),
      int *l2, int *r2)
{
   unsigned seen;
   int axes[4];
   syn_axes[2] = (int16_t)a2;
   syn_axes[3] = (int16_t)a3;
   run_loop_frames(1);
   trace_last(&seen, axes);
   *l2 = input_driver_state_wrapper(0, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_BUTTON, RETRO_DEVICE_ID_JOYPAD_L2);
   *r2 = input_driver_state_wrapper(0, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_BUTTON, RETRO_DEVICE_ID_JOYPAD_R2);
   return seen & (  (1u << RETRO_DEVICE_ID_JOYPAD_L2)
                  | (1u << RETRO_DEVICE_ID_JOYPAD_R2));
}
#endif

/* Full-range triggers: L2 and R2 on axes that rest at one end. */
static void lane_triggers(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
   const unsigned L2 = 1u << RETRO_DEVICE_ID_JOYPAD_L2;
   const unsigned R2 = 1u << RETRO_DEVICE_ID_JOYPAD_R2;
   input_driver_state_t *input_st = input_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   const input_device_driver_t *joypad_real;
   struct retro_keybind saved_l2, saved_r2, *bl2, *br2;
   void (*trace)(int, int);
   void (*trace_last)(unsigned*, int*);
   void    *core;
   unsigned had = failures;
   unsigned seen;
   int      l2, r2;

   if (   !(core = dlopen(core_path_g, RTLD_NOW))
       || !(trace = (void (*)(int, int))dlsym(core, "harness_core_trace"))
       || !(trace_last = (void (*)(unsigned*, int*))dlsym(core, "harness_core_trace_last"))
       || !input_st->primary_joypad)
   {
      CHECK(false, "triggers: the harness core's trace entry points or the joypad driver");
      return;
   }
   joypad_real              = input_st->primary_joypad;
   syn_joypad               = *joypad_real;
   syn_joypad.button        = syn_button;
   syn_joypad.axis          = syn_axis;
   syn_joypad.state         = syn_state;
   syn_joypad.get_buttons   = syn_get_buttons;
   input_st->primary_joypad = &syn_joypad;
   input_driver_set_snapshot_bridge(true);
   syn_hat     = 0;
   syn_buttons = 0;
   memset(syn_axes, 0, sizeof(syn_axes));
   bl2      = &input_autoconf_binds[0][RETRO_DEVICE_ID_JOYPAD_L2];
   br2      = &input_autoconf_binds[0][RETRO_DEVICE_ID_JOYPAD_R2];
   saved_l2 = *bl2;
   saved_r2 = *br2;
   fast_forward(true);
   trace(2, 0);

   /* L2 on an axis that rests at the bottom of its range, R2 on one
    * that rests in the middle */
   bl2->joykey = NO_BTN;  bl2->joyaxis = AXIS_POS(2);
   br2->joykey = NO_BTN;  br2->joyaxis = AXIS_POS(3);

   /* off, as before: only the second half of the pull counts */
   settings->bools.input_trigger_full_range = false;
   trig_frame(-32767, 0, trace_last, &l2, &r2);
   seen = trig_frame(-32767, 0, trace_last, &l2, &r2);
   CHECK(!seen && !l2, "triggers: off, a trigger at rest is pressed");
   seen = trig_frame(0, 0, trace_last, &l2, &r2);
   CHECK(!seen && !l2, "triggers: off, half a pull is counted");
   seen = trig_frame(20000, 0, trace_last, &l2, &r2);
   CHECK(seen == L2 && l2 == 20000, "triggers: off, the second half of the pull is not as it was");
   CHECK(!input_st->trigger_rest[0], "triggers: off, something was learned");

   /* on: the pull is counted from where the trigger rests */
   settings->bools.input_trigger_full_range = true;
   trig_frame(-32767, 0, trace_last, &l2, &r2);
   seen = trig_frame(-32767, 0, trace_last, &l2, &r2);
   CHECK(!seen && !l2, "triggers: on, a trigger at rest is pressed");
   seen = trig_frame(0, 0, trace_last, &l2, &r2);
   CHECK(!seen && l2 == 16383, "triggers: on, half a pull is not half the analog range");
   seen = trig_frame(4000, 16384, trace_last, &l2, &r2);
   CHECK(seen == L2 && l2 == 18383, "triggers: on, just past half a pull does not press the button");
   CHECK(r2 == 16384, "triggers: on, a trigger that rests in the middle is changed");
   seen = trig_frame(32767, 32767, trace_last, &l2, &r2);
   CHECK(seen == (L2 | R2) && l2 == 32767 && r2 == 32767,
         "triggers: on, a full pull is not the full range");

   /* one axis with a trigger on each direction is left as it is */
   input_st->trigger_rest[0]    = 0;
   input_st->trigger_two_way[0] = 0;
   br2->joyaxis = AXIS_NEG(2);
   trig_frame(-32767, 0, trace_last, &l2, &r2);
   seen = trig_frame(-32767, 0, trace_last, &l2, &r2);
   CHECK(seen == R2 && r2 == 32767 && !l2, "triggers: a combined axis, R2 fully pulled");
   seen = trig_frame(0, 0, trace_last, &l2, &r2);
   CHECK(!seen && !l2 && !r2, "triggers: a combined axis at rest reads as a pull");

   /* a trigger bound the other way round, resting at the top */
   input_st->trigger_rest[0]    = 0;
   input_st->trigger_two_way[0] = 0;
   bl2->joyaxis = AXIS_NEG(2);
   br2->joyaxis = AXIS_POS(3);
   trig_frame(32767, 0, trace_last, &l2, &r2);
   seen = trig_frame(32767, 0, trace_last, &l2, &r2);
   CHECK(!seen && !l2, "triggers: one resting at the top is pressed at rest");
   seen = trig_frame(-32767, 0, trace_last, &l2, &r2);
   CHECK(seen == L2 && l2 == 32767, "triggers: one resting at the top, fully pulled");

   settings->bools.input_trigger_full_range = false;
   input_st->trigger_rest[0]    = 0;
   input_st->trigger_two_way[0] = 0;
   *bl2 = saved_l2;
   *br2 = saved_r2;
   memset(syn_axes, 0, sizeof(syn_axes));
   trace(0, 0);
   fast_forward(false);
   run_loop_frames(5);
   input_driver_set_snapshot_bridge(false);
   input_st->primary_joypad = joypad_real;
   dlclose(core);
   if (failures == had)
      printf("[pass] triggers: off, only the second half of a pull counts,"
            " as before; on, a trigger that rests at one end is counted"
            " from there, one that rests in the middle and a combined"
            " axis are left alone\n");
#else
   printf("[skip] triggers: needs the test drivers\n");
#endif
}

#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
/* Remapped pressure lane: the scripted pad's @buttons held and axes 2
 * and 3 at @a2 and @a3 for a frame; what the core saw of A and B as
 * buttons, and A's and B's analog values. */
static unsigned pressure_frame(uint32_t buttons, int a2, int a3,
      void (*trace_last)(unsigned*, int*), int *a, int *b)
{
   unsigned seen;
   int axes[4];
   syn_buttons = buttons;
   syn_axes[2] = (int16_t)a2;
   syn_axes[3] = (int16_t)a3;
   run_loop_frames(1);
   trace_last(&seen, axes);
   *a = input_driver_state_wrapper(0, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_BUTTON, RETRO_DEVICE_ID_JOYPAD_A);
   *b = input_driver_state_wrapper(0, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_BUTTON, RETRO_DEVICE_ID_JOYPAD_B);
   return seen & (  (1u << RETRO_DEVICE_ID_JOYPAD_A)
                  | (1u << RETRO_DEVICE_ID_JOYPAD_B));
}
#endif

/* A button remapped onto another carries its pressure with it. */
static void lane_remap_pressure(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
   const unsigned A = 1u << RETRO_DEVICE_ID_JOYPAD_A;
   const unsigned B = 1u << RETRO_DEVICE_ID_JOYPAD_B;
   input_driver_state_t *input_st = input_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   unsigned *remap                = settings->uints.input_remap_ids[0];
   struct retro_keybind *a_bind   = &input_autoconf_binds[0][RETRO_DEVICE_ID_JOYPAD_A];
   struct retro_keybind *b_bind   = &input_autoconf_binds[0][RETRO_DEVICE_ID_JOYPAD_B];
   struct retro_keybind saved_a   = *a_bind;
   struct retro_keybind saved_b   = *b_bind;
   bool analog_requested          = input_st->analog_requested[0];
   const input_device_driver_t *joypad_real;
   void (*trace)(int, int);
   void (*trace_last)(unsigned*, int*);
   void    *core;
   unsigned had = failures;
   unsigned seen;
   uint32_t a_btn, b_btn;
   int16_t  stick;
   int      a, b, x;

   if (   !(core = dlopen(core_path_g, RTLD_NOW))
       || !(trace = (void (*)(int, int))dlsym(core, "harness_core_trace"))
       || !(trace_last = (void (*)(unsigned*, int*))dlsym(core, "harness_core_trace_last"))
       || !input_st->primary_joypad
       || a_bind->joykey >= 32 || b_bind->joykey >= 32)
   {
      CHECK(false, "remapped pressure: the harness core's trace entry points,"
            " the joypad driver or A's and B's buttons");
      return;
   }
   joypad_real              = input_st->primary_joypad;
   syn_joypad               = *joypad_real;
   syn_joypad.button        = syn_button;
   syn_joypad.axis          = syn_axis;
   syn_joypad.state         = syn_state;
   syn_joypad.get_buttons   = syn_get_buttons;
   input_st->primary_joypad = &syn_joypad;
   input_driver_set_snapshot_bridge(true);
   syn_hat     = 0;
   syn_buttons = 0;
   memset(syn_axes, 0, sizeof(syn_axes));
   fast_forward(true);
   trace(2, 0);

   a_btn           = 1u << a_bind->joykey;
   b_btn           = 1u << b_bind->joykey;
   a_bind->joyaxis = AXIS_POS(2);
   b_bind->joyaxis = AXIS_POS(3);

   /* not remapped: as before */
   pressure_frame(0, 0, 0, trace_last, &a, &b);
   seen = pressure_frame(a_btn, 20000, 0, trace_last, &a, &b);
   CHECK(seen == A && a == 20000 && !b,
         "remapped pressure: not remapped, A's pressure is not A's");
   seen = pressure_frame(a_btn, 0, 0, trace_last, &a, &b);
   CHECK(seen == A && a == 0x7fff,
         "remapped pressure: not remapped, a press without pressure is not a full press");

   /* A onto B */
   input_config_set_remap_id(0, RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_B);
   pressure_frame(0, 0, 0, trace_last, &a, &b);
   seen = pressure_frame(a_btn, 20000, 0, trace_last, &a, &b);
   CHECK(seen == B && b == 20000 && !a,
         "remapped pressure: A onto B, B's pressure is not A's");
   seen = pressure_frame(0, 0, 0, trace_last, &a, &b);
   CHECK(!seen && !a && !b,
         "remapped pressure: A onto B, released, a press is left behind");

   /* A and B both onto B: the harder press */
   seen = pressure_frame(a_btn | b_btn, 20000, 10000, trace_last, &a, &b);
   CHECK(seen == B && b == 20000 && !a,
         "remapped pressure: A and B onto B, A pressed harder, B is not A's");
   seen = pressure_frame(a_btn | b_btn, 5000, 10000, trace_last, &a, &b);
   CHECK(seen == B && b == 10000 && !a,
         "remapped pressure: A and B onto B, B pressed harder, B is not B's");

   /* swapped */
   input_config_set_remap_id(0, RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_A);
   seen = pressure_frame(a_btn | b_btn, 20000, 10000, trace_last, &a, &b);
   CHECK(seen == (A | B) && b == 20000 && a == 10000,
         "remapped pressure: A and B swapped, the pressures are not swapped");

   /* a button without a pressure axis gives a full press */
   a_bind->joyaxis = AXIS_NONE;
   seen = pressure_frame(a_btn, 0, 0, trace_last, &a, &b);
   CHECK(seen == B && b == 0x7fff && !a,
         "remapped pressure: A onto B without an axis, not a full press");
   a_bind->joyaxis = AXIS_POS(2);

   /* A and B both onto X: the harder press */
   input_config_set_remap_id(0, RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_X);
   input_config_set_remap_id(0, RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_X);
   seen = pressure_frame(a_btn | b_btn, 20000, 10000, trace_last, &a, &b);
   x    = input_driver_state_wrapper(0, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_BUTTON, RETRO_DEVICE_ID_JOYPAD_X);
   CHECK(!seen && !a && !b && x == 20000,
         "remapped pressure: A and B onto X, A pressed harder, X is not A's");
   seen = pressure_frame(a_btn | b_btn, 5000, 10000, trace_last, &a, &b);
   x    = input_driver_state_wrapper(0, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_BUTTON, RETRO_DEVICE_ID_JOYPAD_X);
   CHECK(!seen && !a && !b && x == 10000,
         "remapped pressure: A and B onto X, B pressed harder, X is not B's");

   /* unmapped: nothing */
   input_config_set_remap_id(0, RETRO_DEVICE_ID_JOYPAD_A, RARCH_UNMAPPED);
   input_config_set_remap_id(0, RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_B);
   seen = pressure_frame(a_btn, 20000, 0, trace_last, &a, &b);
   CHECK(!seen && !a && !b,
         "remapped pressure: A unmapped, it still reaches the core");

   /* onto a stick, as before */
   input_config_set_remap_id(0, RETRO_DEVICE_ID_JOYPAD_A, RARCH_ANALOG_LEFT_X_PLUS);
   pressure_frame(a_btn, 20000, 0, trace_last, &a, &b);
   stick = input_driver_state_wrapper(0, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
   CHECK(stick == 20000 && !a,
         "remapped pressure: A onto the left stick, the stick is not A's pressure");

   input_config_set_remap_id(0, RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_A);
   input_config_set_remap_id(0, RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_B);
   *a_bind = saved_a;
   *b_bind = saved_b;
   syn_buttons = 0;
   memset(syn_axes, 0, sizeof(syn_axes));
   trace(0, 0);
   fast_forward(false);
   run_loop_frames(5);
   input_driver_set_snapshot_bridge(false);
   input_st->primary_joypad      = joypad_real;
   input_st->analog_requested[0] = analog_requested;
   dlclose(core);
   if (failures == had)
      printf("[pass] remapped pressure: a button remapped onto another"
            " takes its pressure along, the harder of two presses wins,"
            " and a button without pressure is a full press\n");
#else
   printf("[skip] remapped pressure: needs the test drivers\n");
#endif
}

#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32) \
   && defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Network RetroPad lane: one message to the first user's port. */
static void np_send(int fd, const struct sockaddr_in *to,
      int device, int index, int id, int state)
{
   struct remote_message m;
   memset(&m, 0, sizeof(m));
   m.device = device;
   m.index  = index;
   m.id     = id;
   m.state  = (uint16_t)state;
   sendto(fd, (const char*)&m, sizeof(m), 0,
         (const struct sockaddr*)to, sizeof(*to));
}
#endif

/* Network RetroPad: a controller sent over UDP by another RetroArch. */
static void lane_network_retropad(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32) \
   && defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
   const unsigned A = 1u << RETRO_DEVICE_ID_JOYPAD_A;
   const unsigned B = 1u << RETRO_DEVICE_ID_JOYPAD_B;
   input_driver_state_t *input_st = input_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   unsigned max_users             = settings->uints.input_max_users;
   unsigned saved_port            = settings->uints.network_remote_base_port;
   bool     saved_user            = settings->bools.network_remote_enable_user[0];
   input_remote_t *saved_remote   = input_st->remote;
   struct sockaddr_in to;
   void (*trace)(int, int);
   void (*trace_last)(unsigned*, int*);
   void    *core;
   unsigned had = failures;
   unsigned seen, i;
   int      axes[4], fd;

   if (   !(core = dlopen(core_path_g, RTLD_NOW))
       || !(trace = (void (*)(int, int))dlsym(core, "harness_core_trace"))
       || !(trace_last = (void (*)(unsigned*, int*))dlsym(core, "harness_core_trace_last")))
   {
      CHECK(false, "network pad: the harness core's trace entry points");
      return;
   }

   /* a port of this run's own, and the first user listening on it */
   settings->uints.network_remote_base_port       = 56000 + (unsigned)(getpid() % 4000);
   settings->bools.network_remote_enable_user[0]  = true;
   input_st->remote = input_driver_init_remote(settings, max_users);
   fd               = socket(AF_INET, SOCK_DGRAM, 0);
   memset(&to, 0, sizeof(to));
   to.sin_family      = AF_INET;
   to.sin_port        = htons((uint16_t)settings->uints.network_remote_base_port);
   to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   if (!input_st->remote || fd < 0)
   {
      CHECK(false, "network pad: could not open the sockets");
      goto done;
   }

   fast_forward(true);
   trace(1, 1);
   run_loop_frames(3);
   trace(2, 1);

   /* three controls change in one frame of the sender: all three are
    * there at the next poll, not one a poll */
   np_send(fd, &to, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, 1);
   np_send(fd, &to, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, 1);
   np_send(fd, &to, RETRO_DEVICE_ANALOG, 0, RETRO_DEVICE_ID_ANALOG_X, 12345);
   run_loop_frames(2);
   trace_last(&seen, axes);
   CHECK((seen & (A | B)) == (A | B) && axes[0] == 12345,
         "network pad: messages sent together did not all arrive by the next frame");

   /* nothing sent: what is held stays held */
   run_loop_frames(5);
   trace_last(&seen, axes);
   CHECK((seen & (A | B)) == (A | B) && axes[0] == 12345,
         "network pad: a held control was let go with nothing sent");

   /* a release, and a message that is not one of ours */
   np_send(fd, &to, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, 0);
   sendto(fd, "xyz", 3, 0, (const struct sockaddr*)&to, sizeof(to));
   run_loop_frames(2);
   trace_last(&seen, axes);
   CHECK((seen & (A | B)) == B,
         "network pad: a release did not arrive, or a stray message let a control go");

   /* a burst longer than one poll takes (but not than the socket
    * holds): it ends up where it ended */
   for (i = 0; i < 100; i++)
      np_send(fd, &to, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, (int)(i & 1));
   np_send(fd, &to, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, 0);
   np_send(fd, &to, RETRO_DEVICE_ANALOG, 0, RETRO_DEVICE_ID_ANALOG_X, 0);
   run_loop_frames(8);
   trace_last(&seen, axes);
   CHECK(!(seen & (A | B)) && !axes[0],
         "network pad: after a burst the state is not what was sent last");

   /* "First Sender Only": a second device, at another address */
   {
      struct sockaddr_in src2;
      int fd2 = socket(AF_INET, SOCK_DGRAM, 0);
      memset(&src2, 0, sizeof(src2));
      src2.sin_family      = AF_INET;
      src2.sin_addr.s_addr = htonl(0x7f000002);
      if (fd2 >= 0 && !bind(fd2, (const struct sockaddr*)&src2, sizeof(src2)))
      {
         /* off: whoever sends is taken */
         np_send(fd2, &to, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, 1);
         run_loop_frames(2);
         trace_last(&seen, axes);
         CHECK(seen & B, "network pad: a second device is not taken with First Sender Only off");
         np_send(fd2, &to, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, 0);
         run_loop_frames(2);

         /* on, and nobody known yet: the first to send is the one */
         settings->bools.network_remote_first_sender = true;
         input_remote_free(input_st->remote, max_users);
         input_st->remote = input_driver_init_remote(settings, max_users);
         np_send(fd, &to, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, 1);
         run_loop_frames(2);
         np_send(fd2, &to, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, 1);
         run_loop_frames(2);
         trace_last(&seen, axes);
         CHECK((seen & (A | B)) == A,
               "network pad: First Sender Only took a second device's message, or lost the first's");
         np_send(fd, &to, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, 0);
         run_loop_frames(2);
         trace_last(&seen, axes);
         CHECK(!(seen & (A | B)), "network pad: the first sender is no longer heard");
         settings->bools.network_remote_first_sender = false;
      }
      else
         printf("[skip] network pad: no second loopback address for First Sender Only\n");
      if (fd2 >= 0)
         close(fd2);
   }

   /* with ports assigned on first press, a button of the network pad
    * is a press of its user's */
   settings->bools.input_assign_ports_on_button_press = true;
   input_remapping_set_defaults(false);
   run_loop_frames(3);
   CHECK(settings->uints.input_remap_ports[0] == MAX_USERS,
         "network pad: the user has a core port before any press");
   np_send(fd, &to, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, 1);
   run_loop_frames(3);
   CHECK(settings->uints.input_remap_ports[0] == 0,
         "network pad: a button of the network pad did not give its user a core port");
   np_send(fd, &to, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, 0);
   run_loop_frames(2);
   settings->bools.input_assign_ports_on_button_press = false;
   input_remapping_set_defaults(false);
   command_event(CMD_EVENT_CONTROLLER_INIT, NULL);

   trace(0, 0);
   fast_forward(false);
   run_loop_frames(3);
done:
   if (fd >= 0)
      close(fd);
   if (input_st->remote)
      input_remote_free(input_st->remote, max_users);
   input_st->remote = saved_remote;
   memset(&input_st->remote_st_ptr, 0, sizeof(input_st->remote_st_ptr));
   settings->uints.network_remote_base_port      = saved_port;
   settings->bools.network_remote_enable_user[0] = saved_user;
   dlclose(core);
   if (failures == had)
      printf("[pass] network pad: messages sent together arrive together;"
            " what is held stays held; a stray message changes nothing;"
            " a burst ends where it ended; First Sender Only keeps to the"
            " first device; a button of it counts as a first press\n");
#else
   printf("[skip] network pad: needs the test drivers and the network gamepad\n");
#endif
}

#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
/* Pointer capture lane: an input driver that notes what it is told. */
static bool     cap_grabbed;
static unsigned cap_calls;

static void cap_grab_mouse(void *data, bool state)
{
   (void)data;
   cap_grabbed = state;
   cap_calls++;
}
#endif

/* Pointer capture: held for named reasons, by whoever wants it. */
static void lane_pointer_capture(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
   input_driver_state_t *input_st = input_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   runloop_state_t *runloop_st    = runloop_state_get_ptr();
   input_driver_t *real           = input_st->current_driver;
   input_driver_t fake;
   unsigned had         = failures;
   unsigned calls;
   uint8_t  saved       = input_st->capture_reasons;
   bool     saved_auto  = settings->bools.input_auto_mouse_grab;
#define CAPTURED() ((input_st->flags & INP_FLAG_GRAB_MOUSE_STATE) != 0)

   if (!real)
   {
      CHECK(false, "pointer capture: no input driver");
      return;
   }
   fake                     = *real;
   fake.grab_mouse          = cap_grab_mouse;
   input_st->current_driver = &fake;
   input_st->capture_reasons = 0;
   input_pointer_capture_apply(false);
   cap_grabbed = false;

   /* the user's toggle, on and off */
   command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL);
   CHECK(CAPTURED() && cap_grabbed
         && input_pointer_capture_reasons() == INPUT_CAPTURE_USER,
         "pointer capture: the toggle did not capture for the user");
   command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL);
   CHECK(!CAPTURED() && !cap_grabbed && !input_pointer_capture_reasons(),
         "pointer capture: the toggle did not let go");

   /* game focus holds it; into exclusive fullscreen and out again
    * lets go of fullscreen's reason only (the grab used to be dropped
    * with game focus still on) */
   command_event(CMD_EVENT_GAME_FOCUS_TOGGLE, NULL);
   CHECK(CAPTURED() && cap_grabbed, "pointer capture: game focus did not capture");
   input_pointer_capture_set_fullscreen(true, true);
   input_pointer_capture_apply(false);
   input_pointer_capture_set_fullscreen(false, false);
   input_pointer_capture_apply(false);
   CHECK(CAPTURED() && cap_grabbed,
         "pointer capture: leaving fullscreen let go of game focus's capture");
   command_event(CMD_EVENT_GAME_FOCUS_TOGGLE, NULL);
   CHECK(!CAPTURED() && !cap_grabbed, "pointer capture: game focus off did not let go");

   /* the user's capture outlasts game focus going on and off (it
    * used to be dropped in a window) */
   command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL);
   command_event(CMD_EVENT_GAME_FOCUS_TOGGLE, NULL);
   command_event(CMD_EVENT_GAME_FOCUS_TOGGLE, NULL);
   CHECK(CAPTURED() && cap_grabbed,
         "pointer capture: game focus going off let go of the user's capture");

   /* a driver that has just started is told again, and one nothing
    * is wanted of is told nothing */
   calls = cap_calls;
   cap_grabbed = false;
   input_pointer_capture_apply(true);
   CHECK(cap_calls == calls + 1 && cap_grabbed && CAPTURED(),
         "pointer capture: a restarted driver was not given the capture again");
   command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL);
   calls = cap_calls;
   input_pointer_capture_apply(true);
   CHECK(cap_calls == calls && !CAPTURED(),
         "pointer capture: a restarted driver was told to grab or let go with nothing held");

   /* borderless fullscreen hides the cursor and grabs nothing; the
    * toggle then captures, and its second press lets go of all */
   input_pointer_capture_set_fullscreen(true, false);
   input_pointer_capture_apply(false);
   CHECK(!CAPTURED() && input_st->capture_cursor_hidden,
         "pointer capture: borderless fullscreen grabbed, or left the cursor shown");
   command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL);
   CHECK(CAPTURED(), "pointer capture: the toggle did not capture in borderless fullscreen");
   command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL);
   CHECK(!CAPTURED() && !input_st->capture_cursor_hidden,
         "pointer capture: the toggle's second press did not let go of everything");

   /* "Automatic Mouse Grab": taken when the window gains focus */
   settings->bools.input_auto_mouse_grab = true;
   runloop_st->flags &= ~RUNLOOP_FLAG_FOCUSED;
   run_loop_frames(2);
   if (runloop_st->flags & RUNLOOP_FLAG_FOCUSED)
   {
      CHECK(CAPTURED() && (input_pointer_capture_reasons() & INPUT_CAPTURE_AUTO_FOCUS),
            "pointer capture: gaining focus did not capture with Automatic Mouse Grab on");
      command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL);
      CHECK(!CAPTURED(), "pointer capture: the toggle did not let go of the automatic capture");
   }
   else
      printf("[info] pointer capture: the window has no focus here, automatic grab not run\n");
   settings->bools.input_auto_mouse_grab = saved_auto;

   /* a driver that cannot grab: the toggle says so and holds nothing */
   input_st->capture_reasons = 0;
   input_pointer_capture_apply(false);
   fake.grab_mouse           = NULL;
   CHECK(!command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL)
         && !input_pointer_capture_reasons(),
         "pointer capture: the toggle held a reason with a driver that cannot grab");

   input_st->current_driver  = real;
   input_st->capture_reasons = saved;
   input_st->flags          &= ~INP_FLAG_GRAB_MOUSE_STATE;
   input_pointer_capture_apply(false);
#undef CAPTURED
   if (failures == had)
      printf("[pass] pointer capture: held for named reasons; leaving"
            " fullscreen or game focus lets go of its own only; a"
            " restarted driver is told again; the toggle lets go of all\n");
#else
   printf("[skip] pointer capture: needs the test drivers\n");
#endif
}

#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32) && defined(HAVE_OVERLAY)
/* Menu combination lane: whether L3 + R3 opens the menu, with the
 * scripted pad holding @pad and the overlay holding @overlay. */
static bool combo_opens(unsigned pad, unsigned overlay)
{
   input_driver_state_t *input_st = input_state_get_ptr();
   input_bits_t bits;
   unsigned i;

   syn_buttons = 0;
   for (i = 0; i < 16; i++)
      if (pad & (1u << i))
         syn_buttons |= 1u << (input_autoconf_binds[0][i].joykey & 31);
   BIT256_CLEAR_ALL(input_st->overlay_ptr->overlay_state.buttons);
   for (i = 0; i < 16; i++)
      if (overlay & (1u << i))
         BIT256_SET(input_st->overlay_ptr->overlay_state.buttons, i);
   /* a poll for the pad, then the system's input as the run loop
    * collects it, and the gate as the run loop applies it */
   input_driver_poll();
   BIT256_CLEAR_ALL(bits);
   input_driver_collect_system_input(input_st, config_get_ptr(), &bits);
   input_driver_menu_combo_source_gate(&bits);
   return input_driver_button_combo(INPUT_COMBO_L3_R3, 0, &bits);
}
#endif

/* The menu combination and the overlay: buttons held on the overlay
 * alone do not add up to it when the overlay has a menu button. */
static void lane_menu_combo_gate(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32) && defined(HAVE_OVERLAY)
   const unsigned L3 = 1u << RETRO_DEVICE_ID_JOYPAD_L3;
   const unsigned R3 = 1u << RETRO_DEVICE_ID_JOYPAD_R3;
   input_driver_state_t *input_st = input_state_get_ptr();
   const input_device_driver_t *joypad_real = input_st->primary_joypad;
   input_overlay_t *saved = input_st->overlay_ptr;
   input_overlay_t  ol;
   struct overlay   layout;
   struct overlay_desc descs[2];
   unsigned had = failures;

   if (!joypad_real)
   {
      CHECK(false, "menu combination: no joypad driver");
      return;
   }
   syn_joypad               = *joypad_real;
   syn_joypad.button        = syn_button;
   syn_joypad.axis          = syn_axis;
   syn_joypad.state         = syn_state;
   syn_joypad.get_buttons   = syn_get_buttons;
   input_st->primary_joypad = &syn_joypad;
   syn_hat = 0;
   memset(syn_axes, 0, sizeof(syn_axes));

   /* an overlay of two buttons, shown but not drawn: a stick click,
    * and a menu button */
   memset(&ol, 0, sizeof(ol));
   memset(&layout, 0, sizeof(layout));
   memset(descs, 0, sizeof(descs));
   BIT256_SET(descs[0].button_mask, RETRO_DEVICE_ID_JOYPAD_L3);
   BIT256_SET(descs[1].button_mask, RARCH_MENU_TOGGLE);
   layout.descs = descs;
   layout.size  = 2;
   ol.active    = &layout;
   input_st->overlay_ptr = &ol;

   CHECK(!combo_opens(0, L3 | R3),
         "menu combination: two overlay buttons opened the menu on an overlay with a menu button");
   CHECK(combo_opens(L3 | R3, 0),
         "menu combination: the controller's own L3 + R3 no longer opens the menu");
   CHECK(combo_opens(L3 | R3, L3 | R3),
         "menu combination: buttons held on both the controller and the overlay did not count");
   CHECK(!combo_opens(L3, R3),
         "menu combination: one overlay button completed the controller's combination");

   /* and in the run loop itself: frames with the two overlay buttons
    * held and the combination set leave the menu shut */
   {
      settings_t *settings = config_get_ptr();
      unsigned saved_combo = settings->uints.input_menu_toggle_gamepad_combo;
      settings->uints.input_menu_toggle_gamepad_combo = INPUT_COMBO_L3_R3;
      syn_buttons = 0;
      BIT256_SET(ol.overlay_state.buttons, RETRO_DEVICE_ID_JOYPAD_L3);
      BIT256_SET(ol.overlay_state.buttons, RETRO_DEVICE_ID_JOYPAD_R3);
      run_loop_frames(3);
      CHECK(!(menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE),
            "menu combination: the run loop opened the menu for two overlay buttons");
      BIT256_CLEAR_ALL(ol.overlay_state.buttons);
      run_loop_frames(2);
      settings->uints.input_menu_toggle_gamepad_combo = saved_combo;
   }

   /* an overlay with no menu button: the combination is its way in */
   layout.size = 1;
   CHECK(combo_opens(0, L3 | R3),
         "menu combination: an overlay with no menu button can no longer open the menu");

   syn_buttons              = 0;
   input_st->overlay_ptr    = saved;
   input_st->primary_joypad = joypad_real;
   input_driver_poll();
   if (failures == had)
      printf("[pass] menu combination: overlay buttons alone do not add up"
            " to it on an overlay with a menu button; the controller's"
            " do, and so do an overlay's with no menu button\n");
#else
   printf("[skip] menu combination: needs the test drivers and overlays\n");
#endif
}

/* The menu, pause and the press that closes the menu. A user opens the
 * menu, closes it with B still held, lets B go and presses it again;
 * pauses with the pause hotkey and goes in and out of the menu - with
 * "Pause Content When Menu Is Active" on and off. What the core sees
 * and whether it runs are held to what they should be:
 *
 * - the menu pauses the core when the setting is on and not otherwise;
 * - leaving the menu lets the core run again, unless the pause hotkey
 *   paused it first;
 * - the B that closed the menu does not reach the core while it is
 *   held; let go and pressed again, it does. */
static void (*pm_trace)(int, int);
static void (*pm_trace_last)(unsigned*, int*);
static long (*pm_runs)(void);

/* frames run one at a time; whether the core saw B in any of them */
static bool pm_frames_saw_b(unsigned n)
{
   bool saw = false;
   unsigned i, buttons;
   int axes[4];
   for (i = 0; i < n; i++)
   {
      long before = pm_runs();
      run_loop_frames(1);
      if (pm_runs() != before)
      {
         pm_trace_last(&buttons, axes);
         if (buttons & (1u << RETRO_DEVICE_ID_JOYPAD_B))
            saw = true;
      }
   }
   return saw;
}

static void pm_set_menu(bool up)
{
   if (menu_is_up() != up)
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
}

/* The menu up and staying up, before a case that closes it: a menu
 * action a case before left pending may close it again on its first
 * frame, which is not what these cases test. */
static bool pm_menu_settled(void)
{
   unsigned tries;
   for (tries = 0; tries < 5; tries++)
   {
      pm_set_menu(true);
      run_loop_frames(4);
      if (menu_is_up())
         return true;
   }
   return false;
}

/* --- What the hotkey pass keeps of a port's binds ---
 *
 * Whether a port has a "Hotkey Enable", on a pad or a key, whether it
 * blocks, which hotkeys are usable: the pass asked the bind tables
 * every frame, and now keeps the answers by port, worked out again
 * when a bind changes or the port is read with another controller's
 * profile (input_port_hotkeys_t). Kept answers are right only while
 * what they were worked out from is as it was. So: each way that
 * changes, and what a hotkey does on the very next press.
 *
 * The pause hotkey on a pad's button, and whether the core runs. */
static bool hf_paused(void)
{
   long r0 = pm_runs();
   run_loop_frames(5);
   return pm_runs() == r0;
}

/* @first held for two frames, then @then with it for two, then nothing */
static void hf_press(uint32_t first, uint32_t then)
{
   if (first)
   {
      syn_buttons = first;
      run_loop_frames(2);
   }
   syn_buttons = first | then;
   run_loop_frames(2);
   syn_buttons = 0;
   run_loop_frames(3);
}

static void lane_hotkey_facts(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
   input_driver_state_t *input_st = input_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   const input_device_driver_t *joypad_real = input_st->primary_joypad;
   struct retro_keybind saved_pause  = input_config_binds[0][RARCH_PAUSE_TOGGLE];
   struct retro_keybind saved_en     = input_config_binds[0][RARCH_ENABLE_HOTKEY];
   struct retro_keybind saved_auto0  = input_autoconf_binds[0][RARCH_ENABLE_HOTKEY];
   struct retro_keybind saved_auto1  = input_autoconf_binds[1][RARCH_ENABLE_HOTKEY];
   unsigned saved_index              = settings->uints.input_joypad_index[0];
   unsigned saved_pad                = syn_pad_index;
   const uint32_t pause_bit = 1u << 20, en_bit = 1u << 23;
   unsigned had = failures;
   void *core;

   if (   !(core = dlopen(core_path_g, RTLD_NOW))
       || !(pm_runs = (long (*)(void))dlsym(core, "harness_core_runs"))
       || !joypad_real)
   {
      CHECK(false, "hotkey facts: the harness core's entry point or the joypad driver");
      return;
   }
   syn_joypad               = *joypad_real;
   syn_joypad.button        = syn_button;
   syn_joypad.axis          = syn_axis;
   syn_joypad.state         = syn_state;
   syn_joypad.get_buttons   = syn_get_buttons;
   input_st->primary_joypad = &syn_joypad;
   syn_hat                  = 0;
   syn_buttons              = 0;
   memset(syn_axes, 0, sizeof(syn_axes));

   /* pause on a button of its own; no enabler anywhere */
   {
      struct retro_keybind *en = &input_config_binds[0][RARCH_ENABLE_HOTKEY];
      input_config_binds[0][RARCH_PAUSE_TOGGLE].joykey = 20;
      en->joykey  = NO_BTN;
      en->joyaxis = AXIS_NONE;
      RETRO_KEYBIND_SET_KEY(en, RETROK_UNKNOWN);
      RETRO_KEYBIND_SET_MBUTTON(en, NO_BTN);
      input_autoconf_binds[0][RARCH_ENABLE_HOTKEY].joykey  = NO_BTN;
      input_autoconf_binds[0][RARCH_ENABLE_HOTKEY].joyaxis = AXIS_NONE;
      input_autoconf_binds[1][RARCH_ENABLE_HOTKEY].joykey  = NO_BTN;
      input_autoconf_binds[1][RARCH_ENABLE_HOTKEY].joyaxis = AXIS_NONE;
   }
   binds_written_by_a_lane();
   run_loop_frames(3);
   CHECK(!hf_paused(), "hotkey facts: the core is not running to begin with");

   /* no enabler: the hotkey acts alone */
   hf_press(0, pause_bit);
   CHECK(hf_paused(), "hotkey facts: with no enabler, the pause hotkey did not pause");
   hf_press(0, pause_bit);
   CHECK(!hf_paused(), "hotkey facts: with no enabler, the pause hotkey did not unpause");

   /* the port's own bind gets an enabler: blocked alone, acts with it */
   input_config_binds[0][RARCH_ENABLE_HOTKEY].joykey = 23;
   binds_written_by_a_lane();
   hf_press(0, pause_bit);
   CHECK(!hf_paused(), "hotkey facts: an enabler was bound, and the pause hotkey"
         " still paused without it");
   hf_press(en_bit, pause_bit);
   CHECK(hf_paused(), "hotkey facts: with the new enabler held, the pause hotkey did not pause");
   hf_press(en_bit, pause_bit);
   CHECK(!hf_paused(), "hotkey facts: with the new enabler held, the pause hotkey did not unpause");

   /* ... and loses it */
   input_config_binds[0][RARCH_ENABLE_HOTKEY].joykey = NO_BTN;
   binds_written_by_a_lane();
   hf_press(0, pause_bit);
   CHECK(hf_paused(), "hotkey facts: the enabler was unbound, and the pause hotkey"
         " alone did not pause");
   hf_press(0, pause_bit);
   CHECK(!hf_paused(), "hotkey facts: the enabler was unbound, and the pause hotkey"
         " alone did not unpause");

   /* the hotkey made unusable, and usable again */
   RETRO_KEYBIND_SET_VALID(&input_config_binds[0][RARCH_PAUSE_TOGGLE], false);
   binds_written_by_a_lane();
   hf_press(0, pause_bit);
   CHECK(!hf_paused(), "hotkey facts: the pause hotkey was made unusable, and paused");
   RETRO_KEYBIND_SET_VALID(&input_config_binds[0][RARCH_PAUSE_TOGGLE], true);
   binds_written_by_a_lane();
   hf_press(0, pause_bit);
   CHECK(hf_paused(), "hotkey facts: the pause hotkey was made usable again, and did not pause");
   hf_press(0, pause_bit);
   CHECK(!hf_paused(), "hotkey facts: the pause hotkey, usable again, did not unpause");

   /* the controller's profile has an enabler: blocked alone */
   input_autoconf_binds[0][RARCH_ENABLE_HOTKEY].joykey = 23;
   binds_written_by_a_lane();
   hf_press(0, pause_bit);
   CHECK(!hf_paused(), "hotkey facts: the controller's profile got an enabler, and"
         " the pause hotkey still paused without it");
   /* ... and the port is given another controller, whose profile has
    * none - a setting, which no bind change announces */
   settings->uints.input_joypad_index[0] = 1;
   syn_pad_index                         = 1;
   hf_press(0, pause_bit);
   CHECK(hf_paused(), "hotkey facts: the port was given a controller whose profile"
         " has no enabler, and the pause hotkey alone did not pause");
   hf_press(0, pause_bit);
   CHECK(!hf_paused(), "hotkey facts: ... and did not unpause");
   settings->uints.input_joypad_index[0] = saved_index;
   syn_pad_index                         = saved_pad;
   hf_press(0, pause_bit);
   CHECK(!hf_paused(), "hotkey facts: the port was given its first controller back,"
         " and the pause hotkey paused without that profile's enabler");

   /* The hotkeys' own binds are kept as one set, for the port the
    * hotkeys are read on. Moved to another port and back by a setting
    * alone - "Hotkeys Follow Player 1" - the set must be the port's it
    * is read for each time: the second port's pause hotkey is unusable
    * here, the first's is not. */
   {
      bool saved_follow    = settings->bools.input_hotkey_follows_player1;
      unsigned saved_map   = settings->uints.input_remap_port_map[0][0];
      struct retro_keybind saved_p1 = input_config_binds[1][RARCH_PAUSE_TOGGLE];

      input_autoconf_binds[0][RARCH_ENABLE_HOTKEY].joykey = NO_BTN;
      input_config_binds[1][RARCH_PAUSE_TOGGLE].joykey    = 20;
      RETRO_KEYBIND_SET_VALID(&input_config_binds[1][RARCH_PAUSE_TOGGLE], false);
      binds_written_by_a_lane();
      hf_press(0, pause_bit);
      CHECK(hf_paused(), "hotkey facts: on the first port, the pause hotkey did not pause");
      hf_press(0, pause_bit);
      CHECK(!hf_paused(), "hotkey facts: on the first port, the pause hotkey did not unpause");

      settings->bools.input_hotkey_follows_player1 = true;
      settings->uints.input_remap_port_map[0][0]   = 1;
      hf_press(0, pause_bit);
      CHECK(!hf_paused(), "hotkey facts: with the hotkeys moved to the second port,"
            " whose pause hotkey is unusable, the first port's button paused");
      settings->uints.input_remap_port_map[0][0]   = 0;
      hf_press(0, pause_bit);
      CHECK(hf_paused(), "hotkey facts: with the hotkeys moved back to the first port,"
            " its pause hotkey did not pause");
      hf_press(0, pause_bit);
      CHECK(!hf_paused(), "hotkey facts: ... and did not unpause");

      settings->bools.input_hotkey_follows_player1 = saved_follow;
      settings->uints.input_remap_port_map[0][0]   = saved_map;
      input_config_binds[1][RARCH_PAUSE_TOGGLE]    = saved_p1;
   }

   input_config_binds[0][RARCH_PAUSE_TOGGLE]     = saved_pause;
   input_config_binds[0][RARCH_ENABLE_HOTKEY]    = saved_en;
   input_autoconf_binds[0][RARCH_ENABLE_HOTKEY]  = saved_auto0;
   input_autoconf_binds[1][RARCH_ENABLE_HOTKEY]  = saved_auto1;
   binds_written_by_a_lane();
   syn_buttons              = 0;
   input_st->primary_joypad = joypad_real;
   run_loop_frames(3);
   if (failures == had)
      fprintf(stderr, "[pass] hotkey-facts lane (kept by port, made again at each change)\n");
#endif
}

/* --- The lightgun's bound buttons, for a driver that keeps its mice ---
 *
 * A lightgun's buttons are what they are bound to: a pad's button, a
 * key, a mouse button. The frontend answers them for a driver that
 * publishes its mice and says so; udev and Cocoa keep their mice and
 * worked the buttons out themselves, from the binds they were handed.
 * Now the frontend answers for them too - a driver that has
 * bind_mouse_buttons - and the driver is never asked.
 *
 * An input driver that publishes no mouse, has a key down and a mouse
 * button held when told to, and notes being asked for a button. */
static input_driver_t gk_input;
static bool           gk_key_down;
static unsigned       gk_mouse;
static unsigned       gk_asked;
static int16_t gk_input_state(void *data,
      const input_device_driver_t *joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      unsigned port, unsigned device, unsigned idx, unsigned id)
{
   (void)data; (void)joypad; (void)joypad_info;
   (void)binds; (void)port; (void)idx;
   if (     (device & RETRO_DEVICE_MASK) == RETRO_DEVICE_LIGHTGUN
         && (   id == RETRO_DEVICE_ID_LIGHTGUN_TRIGGER
             || id == RETRO_DEVICE_ID_LIGHTGUN_RELOAD
             || id == RETRO_DEVICE_ID_LIGHTGUN_AUX_A))
      gk_asked++;
   return 0;
}
static void gk_keys_down(void *data, unsigned port,
      const uint16_t *keys, const uint8_t *bind, unsigned count,
      uint32_t *down)
{
   unsigned i;
   (void)data; (void)bind;
   for (i = 0; i < count; i++)
      if (port == 0 && gk_key_down && keys[i] == RETROK_g)
         down[i >> 5] |= (1u << (i & 31));
}
static unsigned gk_bind_mouse_buttons(void *data, unsigned port)
{
   (void)data;
   return port == 0 ? gk_mouse : 0;
}
static void gk_poll(void *data)
{
   input_pointer_frame_t none;
   (void)data;
   memset(&none, 0, sizeof(none));
   input_driver_publish_pointers(&none, 0, 0);
}

static void lane_gun_buttons_kept_mice(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
   input_driver_state_t *input_st = input_state_get_ptr();
   const input_device_driver_t *joypad_real = input_st->primary_joypad;
   input_driver_t *saved_input    = input_st->current_driver;
   struct retro_keybind saved[3];
   static const unsigned bind_id[3] = {
      RARCH_LIGHTGUN_TRIGGER, RARCH_LIGHTGUN_RELOAD, RARCH_LIGHTGUN_AUX_A };
   static const unsigned gun_id[3]  = {
      RETRO_DEVICE_ID_LIGHTGUN_TRIGGER, RETRO_DEVICE_ID_LIGHTGUN_RELOAD,
      RETRO_DEVICE_ID_LIGHTGUN_AUX_A };
   static const char *what[3] = { "a key", "a mouse button", "a pad's button" };
   unsigned had = failures;
   unsigned i, c;

   if (!saved_input || !joypad_real)
   {
      CHECK(false, "gun buttons: no input driver or no joypad driver");
      return;
   }
   syn_joypad               = *joypad_real;
   syn_joypad.button        = syn_button;
   syn_joypad.axis          = syn_axis;
   syn_joypad.state         = syn_state;
   syn_joypad.get_buttons   = syn_get_buttons;
   input_st->primary_joypad = &syn_joypad;
   syn_hat                  = 0;
   syn_buttons              = 0;
   memset(syn_axes, 0, sizeof(syn_axes));

   gk_input                    = *saved_input;
   gk_input.input_state        = gk_input_state;
   gk_input.poll               = gk_poll;
   gk_input.keys_down          = gk_keys_down;
   gk_input.bind_mouse_buttons = gk_bind_mouse_buttons;
   gk_key_down                 = false;
   gk_mouse                    = 0;
   input_st->current_driver    = &gk_input;

   /* the trigger on a key, reload on the right mouse button, the first
    * auxiliary button on a button of the pad */
   for (i = 0; i < 3; i++)
   {
      struct retro_keybind *b = input_config_bind_edit(0, bind_id[i]);
      saved[i]   = *b;
      b->joykey  = NO_BTN;
      b->joyaxis = AXIS_NONE;
      RETRO_KEYBIND_SET_KEY(b, RETROK_UNKNOWN);
      RETRO_KEYBIND_SET_MBUTTON(b, NO_BTN);
      RETRO_KEYBIND_SET_VALID(b, true);
   }
   RETRO_KEYBIND_SET_KEY(input_config_bind_edit(0, bind_id[0]), RETROK_g);
   RETRO_KEYBIND_SET_MBUTTON(input_config_bind_edit(0, bind_id[1]),
         RETRO_DEVICE_ID_MOUSE_RIGHT);
   input_config_bind_edit(0, bind_id[2])->joykey = 22;
   binds_written_by_a_lane();
   run_loop_frames(2);
   gk_asked = 0;

   /* nothing held, then each alone, then nothing again */
   for (c = 0; c < 5; c++)
   {
      int only = (c >= 1 && c <= 3) ? (int)c - 1 : -1;
      char msg[200];
      gk_key_down = (only == 0);
      gk_mouse    = (only == 1) ? INPUT_POINTER_RIGHT : 0;
      syn_buttons = (only == 2) ? (1u << 22) : 0;
      run_loop_frames(2);
      for (i = 0; i < 3; i++)
      {
         int got = input_driver_state_wrapper(0, RETRO_DEVICE_LIGHTGUN, 0, gun_id[i]);
         snprintf(msg, sizeof(msg), "gun buttons, kept mice: with %s held,"
               " the button bound to %s reads %d",
               only < 0 ? "nothing" : what[only], what[i], got);
         CHECK(got == ((int)i == only ? 1 : 0), msg);
      }
   }
   /* a left click is not the right button */
   gk_mouse = INPUT_POINTER_LEFT;
   run_loop_frames(2);
   CHECK(input_driver_state_wrapper(0, RETRO_DEVICE_LIGHTGUN, 0, gun_id[1]) == 0,
         "gun buttons, kept mice: the button bound to the right mouse button"
         " read as held with the left one down");
   gk_mouse = 0;
   {
      char msg[120];
      snprintf(msg, sizeof(msg), "gun buttons, kept mice: the driver was"
            " asked for a bound button %u time(s)", gk_asked);
      CHECK(gk_asked == 0, msg);
   }

   for (i = 0; i < 3; i++)
      *input_config_bind_edit(0, bind_id[i]) = saved[i];
   binds_written_by_a_lane();
   input_st->current_driver = saved_input;
   input_st->primary_joypad = joypad_real;
   run_loop_frames(2);
   if (failures == had)
      fprintf(stderr, "[pass] gun-buttons lane (a driver that keeps its mice)\n");
#endif
}

/* an input driver that has one key down, RETROK_RETURN, the mouse's
 * left button down, or the pointer pressed, when told to */
static input_driver_t pm_input;
static bool           pm_return_down;
static bool           pm_click_down;
static bool           pm_touch_down;
static int16_t pm_input_state(void *data,
      const input_device_driver_t *joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      unsigned port, unsigned device, unsigned idx, unsigned id)
{
   (void)data; (void)joypad; (void)joypad_info;
   (void)binds;
   if (port != 0)
      return 0;
   switch (device & RETRO_DEVICE_MASK)
   {
      case RETRO_DEVICE_KEYBOARD:
         return (pm_return_down && id == RETROK_RETURN) ? 1 : 0;
      case RETRO_DEVICE_MOUSE:
         return (pm_click_down && id == RETRO_DEVICE_ID_MOUSE_LEFT) ? 1 : 0;
      case RETRO_DEVICE_POINTER:
         return (pm_touch_down && idx == 0
               && id == RETRO_DEVICE_ID_POINTER_PRESSED) ? 1 : 0;
      default:
         break;
   }
   return 0;
}

/* frames run one at a time; whether a control reached the core's view
 * in any of them, read as the core reads it */
static bool pm_frames_saw(unsigned n, unsigned device, unsigned id)
{
   bool saw = false;
   unsigned i;
   for (i = 0; i < n; i++)
   {
      run_loop_frames(1);
      if (input_driver_state_wrapper(0, device, 0, id))
         saw = true;
   }
   return saw;
}

/* frames run one at a time; whether Return reached the core's view in
 * any of them, read as the core reads a key */
static bool pm_frames_saw_return(unsigned n)
{
   bool saw = false;
   unsigned i;
   for (i = 0; i < n; i++)
   {
      run_loop_frames(1);
      if (input_driver_state_wrapper(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_RETURN))
         saw = true;
   }
   return saw;
}

static void lane_menu_pause(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
   input_driver_state_t *input_st = input_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   const input_device_driver_t *joypad_real = input_st->primary_joypad;
   struct retro_keybind saved_pause = input_config_binds[0][RARCH_PAUSE_TOGGLE];
   bool saved_setting             = settings->bools.menu_pause_libretro;
   input_driver_t *saved_input    = NULL;
   struct retro_keybind saved_toggle = input_config_binds[0][RARCH_MENU_TOGGLE];
   uint32_t b_bit, pause_bit      = 1u << 20, toggle_bit = 1u << 21;
   void *core;
   unsigned had = failures;
   unsigned s;

   if (   !(core = dlopen(core_path_g, RTLD_NOW))
       || !(pm_trace = (void (*)(int, int))dlsym(core, "harness_core_trace"))
       || !(pm_trace_last = (void (*)(unsigned*, int*))dlsym(core, "harness_core_trace_last"))
       || !(pm_runs = (long (*)(void))dlsym(core, "harness_core_runs"))
       || !joypad_real)
   {
      CHECK(false, "menu pause: the harness core's entry points or the joypad driver");
      return;
   }

   syn_joypad               = *joypad_real;
   syn_joypad.button        = syn_button;
   syn_joypad.axis          = syn_axis;
   syn_joypad.state         = syn_state;
   syn_joypad.get_buttons   = syn_get_buttons;
   input_st->primary_joypad = &syn_joypad;
   syn_hat                  = 0;
   syn_buttons              = 0;
   memset(syn_axes, 0, sizeof(syn_axes));
   b_bit = 1u << (input_autoconf_binds[0][RETRO_DEVICE_ID_JOYPAD_B].joykey & 31);
   if (!input_st->current_driver)
   {
      CHECK(false, "menu pause: no input driver");
      input_st->primary_joypad = joypad_real;
      return;
   }
   saved_input              = input_st->current_driver;
   pm_input                 = *saved_input;
   pm_input.input_state     = pm_input_state;
   pm_return_down           = false;
   input_st->current_driver = &pm_input;
   /* the pause hotkey on a button of its own */
   input_config_binds[0][RARCH_PAUSE_TOGGLE].joykey = 20;
   input_config_binds[0][RARCH_MENU_TOGGLE].joykey  = 21;
   pm_trace(2, 0);

   for (s = 0; s < 2; s++)
   {
      bool pause_in_menu = (s == 0);
      const char *with   = pause_in_menu ? "with Pause Content in the menu on"
                                         : "with Pause Content in the menu off";
      char msg[256];
      long r0, r1;

      settings->bools.menu_pause_libretro = pause_in_menu;
      pm_set_menu(false);
      syn_buttons = 0;
      run_loop_frames(10);

      /* in the menu: paused or not, as the setting says */
      pm_set_menu(true);
      run_loop_frames(2);
      r0 = pm_runs();
      run_loop_frames(10);
      r1 = pm_runs();
      snprintf(msg, sizeof(msg), "menu pause, %s: the core %s in the menu",
            with, pause_in_menu ? "ran" : "did not run");
      CHECK(pause_in_menu ? (r1 == r0) : (r1 > r0), msg);

      /* closed with Return held on the keyboard: the same for a key */
      pm_return_down = true;
      run_loop_frames(3);
      pm_set_menu(false);
      snprintf(msg, sizeof(msg), "menu pause, %s: the core saw the Return that closed the menu, while it was held", with);
      CHECK(!pm_frames_saw_return(15), msg);
      pm_return_down = false;
      snprintf(msg, sizeof(msg), "menu pause, %s: the core saw Return after it was let go", with);
      CHECK(!pm_frames_saw_return(5), msg);
      pm_return_down = true;
      snprintf(msg, sizeof(msg), "menu pause, %s: the core did not see Return pressed again after the menu", with);
      CHECK(pm_frames_saw_return(5), msg);
      pm_return_down = false;
      run_loop_frames(3);

      /* closed with the mouse's left button held, and with the pointer
       * pressed: the same, for a click and a touch */
      {
         static const struct { const char *what; unsigned device, id; bool *flag; } c[2] = {
            { "the left click", RETRO_DEVICE_MOUSE,   RETRO_DEVICE_ID_MOUSE_LEFT,      &pm_click_down },
            { "the touch",      RETRO_DEVICE_POINTER, RETRO_DEVICE_ID_POINTER_PRESSED, &pm_touch_down }
         };
         unsigned k;
         for (k = 0; k < 2; k++)
         {
            snprintf(msg, sizeof(msg), "menu pause, %s: the menu would not stay open", with);
            CHECK(pm_menu_settled(), msg);
            *c[k].flag = true;
            run_loop_frames(2);
            pm_set_menu(false);
            snprintf(msg, sizeof(msg), "menu pause, %s: the core saw %s that closed the menu, while it was held", with, c[k].what);
            CHECK(!pm_frames_saw(10, c[k].device, c[k].id), msg);
            *c[k].flag = false;
            snprintf(msg, sizeof(msg), "menu pause, %s: the core saw %s after it was let go", with, c[k].what);
            CHECK(!pm_frames_saw(4, c[k].device, c[k].id), msg);
            *c[k].flag = true;
            snprintf(msg, sizeof(msg), "menu pause, %s: the core did not see %s made again after the menu", with, c[k].what);
            CHECK(pm_frames_saw(4, c[k].device, c[k].id), msg);
            *c[k].flag = false;
            run_loop_frames(3);
         }
      }

      snprintf(msg, sizeof(msg), "menu pause, %s: the menu would not stay open", with);
      CHECK(pm_menu_settled(), msg);
      /* closed with B held: the core runs, and does not see that B */
      syn_buttons = b_bit;
      run_loop_frames(3);
      pm_set_menu(false);
      r0 = pm_runs();
      snprintf(msg, sizeof(msg), "menu pause, %s: the core saw the B that closed the menu, while it was held", with);
      CHECK(!pm_frames_saw_b(15), msg);
      snprintf(msg, sizeof(msg), "menu pause, %s: the core did not run again after the menu closed", with);
      CHECK(pm_runs() > r0, msg);
      syn_buttons = 0;
      snprintf(msg, sizeof(msg), "menu pause, %s: the core saw B after it was let go", with);
      CHECK(!pm_frames_saw_b(5), msg);
      syn_buttons = b_bit;
      snprintf(msg, sizeof(msg), "menu pause, %s: the core did not see B pressed again after the menu", with);
      CHECK(pm_frames_saw_b(5), msg);
      syn_buttons = 0;
      run_loop_frames(3);

      /* the menu opened with B held: B is held back in the menu and
       * does nothing there, but Down pressed while B is still down moves
       * the selection at once - the hold is on what was down, not on
       * all */
      {
         uint32_t down_bit = 1u << (input_autoconf_binds[0][RETRO_DEVICE_ID_JOYPAD_DOWN].joykey & 31);
         uint32_t up_bit   = 1u << (input_autoconf_binds[0][RETRO_DEVICE_ID_JOYPAD_UP].joykey & 31);
         size_t   before;
         pm_set_menu(false);
         run_loop_frames(3);
         syn_buttons = b_bit;
         run_loop_frames(3);
         pm_set_menu(true);
         run_loop_frames(3);
         before      = menu_state_get_ptr()->selection_ptr;
         syn_buttons = b_bit | down_bit;
         run_loop_frames(2);
         syn_buttons = b_bit;
         run_loop_frames(3);
         snprintf(msg, sizeof(msg), "menu pause, %s: the menu shut by itself after opening with B held", with);
         CHECK(menu_is_up(), msg);
         snprintf(msg, sizeof(msg), "menu pause, %s: Down, pressed while the B held as the menu opened was still down, did not move the selection", with);
         CHECK(menu_state_get_ptr()->selection_ptr != before, msg);
         syn_buttons = 0;
         run_loop_frames(3);
         syn_buttons = up_bit;
         run_loop_frames(2);
         syn_buttons = 0;
         run_loop_frames(3);
         pm_set_menu(false);
         run_loop_frames(5);
      }

      /* paused with the hotkey, into the menu with its own hotkey and
       * out again with that hotkey held for a while: still paused */
      syn_buttons = pause_bit;
      run_loop_frames(2);
      syn_buttons = 0;
      run_loop_frames(3);
      syn_buttons = toggle_bit;
      run_loop_frames(2);
      syn_buttons = 0;
      run_loop_frames(3);
      snprintf(msg, sizeof(msg), "menu pause, %s: the menu hotkey did not open the menu", with);
      CHECK(menu_is_up(), msg);
      /* the menu hotkey acts as it is let go: held a while, then let
       * go, it closes the menu, which stays shut */
      syn_buttons = toggle_bit;
      run_loop_frames(8);
      syn_buttons = 0;
      run_loop_frames(5);
      snprintf(msg, sizeof(msg), "menu pause, %s: the menu hotkey, held and let go, did not close the menu, or opened it again", with);
      CHECK(!menu_is_up(), msg);
      r0 = pm_runs();
      run_loop_frames(10);
      snprintf(msg, sizeof(msg), "menu pause, %s: closing the menu with its hotkey held ran a core the pause hotkey had paused", with);
      CHECK(pm_runs() == r0, msg);
      syn_buttons = pause_bit;
      run_loop_frames(2);
      syn_buttons = 0;
      run_loop_frames(3);
      r0 = pm_runs();
      run_loop_frames(5);
      snprintf(msg, sizeof(msg), "menu pause, %s: the pause hotkey did not unpause the core after the menu hotkey", with);
      CHECK(pm_runs() > r0, msg);

      /* paused with the hotkey, then into the menu and out: still
       * paused, until the hotkey again */
      syn_buttons = pause_bit;
      run_loop_frames(2);
      syn_buttons = 0;
      run_loop_frames(3);
      r0 = pm_runs();
      run_loop_frames(5);
      snprintf(msg, sizeof(msg), "menu pause, %s: the pause hotkey did not pause the core", with);
      CHECK(pm_runs() == r0, msg);
      pm_set_menu(true);
      run_loop_frames(5);
      pm_set_menu(false);
      run_loop_frames(5);
      r0 = pm_runs();
      run_loop_frames(10);
      snprintf(msg, sizeof(msg), "menu pause, %s: leaving the menu ran a core the pause hotkey had paused", with);
      CHECK(pm_runs() == r0, msg);
      syn_buttons = pause_bit;
      run_loop_frames(2);
      syn_buttons = 0;
      run_loop_frames(3);
      r0 = pm_runs();
      run_loop_frames(5);
      snprintf(msg, sizeof(msg), "menu pause, %s: the pause hotkey did not unpause the core after the menu", with);
      CHECK(pm_runs() > r0, msg);
   }

   pm_trace(0, 0);
   pm_set_menu(false);
   syn_buttons                                = 0;
   settings->bools.menu_pause_libretro        = saved_setting;
   input_config_binds[0][RARCH_PAUSE_TOGGLE]  = saved_pause;
   input_config_binds[0][RARCH_MENU_TOGGLE]   = saved_toggle;
   input_st->current_driver                   = saved_input;
   pm_return_down                             = false;
   pm_click_down                              = false;
   pm_touch_down                              = false;
   input_st->primary_joypad                   = joypad_real;
   run_loop_frames(3);
   if (failures == had)
      printf("[pass] menu pause: the menu pauses the core when set to and"
            " not otherwise; leaving it, by command or by its hotkey held,"
            " runs the core unless the pause hotkey paused it; the B or the"
            " Return that closed the menu does not reach the core until let"
            " go and pressed again - with the setting on and off\n");
#else
   printf("[skip] menu pause: needs the test drivers\n");
#endif
}

/* The AI service's presses: a button it presses reaches the core for
 * exactly one frame, and two presses before the next poll both do. */
static void lane_ai_presses(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32) && defined(HAVE_TRANSLATE) && defined(HAVE_ACCESSIBILITY)
   const unsigned A = 1u << RETRO_DEVICE_ID_JOYPAD_A;
   const unsigned X = 1u << RETRO_DEVICE_ID_JOYPAD_X;
   settings_t *settings = config_get_ptr();
   bool saved_enable    = settings->bools.ai_service_enable;
   void (*trace)(int, int);
   void (*trace_last)(unsigned*, int*);
   long (*runs)(void);
   void *core;
   unsigned had = failures;
   unsigned i, frames_a = 0, frames_ax = 0, buttons;
   int axes[4];

   if (   !(core = dlopen(core_path_g, RTLD_NOW))
       || !(trace = (void (*)(int, int))dlsym(core, "harness_core_trace"))
       || !(trace_last = (void (*)(unsigned*, int*))dlsym(core, "harness_core_trace_last"))
       || !(runs = (long (*)(void))dlsym(core, "harness_core_runs")))
   {
      CHECK(false, "AI presses: the harness core's trace entry points");
      return;
   }
   settings->bools.ai_service_enable = true;
   trace(2, 0);
   run_loop_frames(3);

   /* one press: one frame */
   input_driver_ai_gamepad_press(RETRO_DEVICE_ID_JOYPAD_A);
   for (i = 0; i < 6; i++)
   {
      long before = runs();
      run_loop_frames(1);
      if (runs() == before)
         continue;
      trace_last(&buttons, axes);
      if (buttons & A)
         frames_a++;
   }
   CHECK(frames_a == 1, "AI presses: a press did not reach the core for exactly one frame");

   /* two presses before the poll: both, in the same frame */
   input_driver_ai_gamepad_press(RETRO_DEVICE_ID_JOYPAD_A);
   input_driver_ai_gamepad_press(RETRO_DEVICE_ID_JOYPAD_X);
   for (i = 0; i < 6; i++)
   {
      long before = runs();
      run_loop_frames(1);
      if (runs() == before)
         continue;
      trace_last(&buttons, axes);
      if ((buttons & (A | X)) == (A | X))
         frames_ax++;
   }
   CHECK(frames_ax == 1, "AI presses: two presses did not both reach the core, for one frame");

   trace(0, 0);
   settings->bools.ai_service_enable = saved_enable;
   run_loop_frames(2);
   if (failures == had)
      printf("[pass] AI presses: a button the AI service presses reaches the"
            " core for exactly one frame; two pressed before a poll arrive"
            " together\n");
#else
   printf("[skip] AI presses: needs the test drivers and the AI service"
         " (HAVE_TRANSLATE, HAVE_ACCESSIBILITY)\n");
#endif
}

/* The menu's auto-repeat goes by real time and uses every frame: Down
 * held for three seconds with the menu at 30, 60, 120, 144 and 240 Hz
 * moves the same distance at each, starts repeating at Menu Scroll
 * Delay, and ends with the same acceleration and speed-up time. With
 * fast scrolling the list runs at up to 120 entries a second: a 240 Hz
 * menu covers the same distance in single-entry steps on more frames,
 * where a 60 Hz one has to jump two at a time. menu_event() is driven
 * directly with times of its own, so it takes no real seconds. */
struct repeat_run
{
   unsigned moved, first_repeat_ms, accel, max_step, frames_moving;
   retro_time_t press_time;
};

static void repeat_hold(unsigned hz, retro_time_t t0, struct repeat_run *out)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   settings_t *settings       = config_get_ptr();
   input_bits_t held, none, trig;
   retro_time_t period        = 1000000 / hz;
   retro_time_t t;
   bool first                 = true;

   memset(out, 0, sizeof(*out));
   BIT256_CLEAR_ALL(none);
   BIT256_CLEAR_ALL(held);
   BIT256_SET(held, RETRO_DEVICE_ID_JOYPAD_DOWN);
   /* let go first, so the hold starts here */
   menu_st->input_time_us = t0 - period;
   trig = none;
   menu_event(settings, &none, &trig, false);

   for (t = t0; t < t0 + 3000000; t += period)
   {
      unsigned action, step;
      trig                   = first ? held : none;
      menu_st->input_time_us = t;
      action                 = menu_event(settings, &held, &trig, false);
      if (action == MENU_ACTION_DOWN)
      {
         step = menu_st->scroll.steps ? menu_st->scroll.steps : 1;
         out->moved += step;
         out->frames_moving++;
         if (step > out->max_step)
            out->max_step = step;
         if (!first && !out->first_repeat_ms)
            out->first_repeat_ms = (unsigned)((t - t0) / 1000);
      }
      first = false;
   }
   out->accel      = menu_st->scroll.acceleration;
   out->press_time = menu_st->action_press_time;
   menu_st->input_time_us = t;
   trig = none;
   menu_event(settings, &none, &trig, false);
}

static void lane_menu_repeat_rates(void)
{
#ifdef HAVE_MENU
   static const unsigned hz[] = { 30, 60, 120, 144, 240 };
   settings_t *settings       = config_get_ptr();
   bool saved_fast            = settings->bools.menu_scroll_fast;
   unsigned delay_ms          = settings->uints.menu_scroll_delay;
   struct repeat_run run[5], fast[5];
   unsigned had = failures;
   unsigned r, k;
   char msg[200];

   pm_set_menu(true);
   run_loop_frames(3);
   settings->bools.menu_scroll_fast = false;
   for (r = 0; r < 5; r++)
      repeat_hold(hz[r], 50000000 + (retro_time_t)r * 10000000, &run[r]);
   settings->bools.menu_scroll_fast = true;
   for (r = 0; r < 5; r++)
      repeat_hold(hz[r], 200000000 + (retro_time_t)r * 10000000, &fast[r]);
   settings->bools.menu_scroll_fast = saved_fast;

   for (r = 0; r < 5; r++)
   {
      retro_time_t period_ms_x10 = 10000 / hz[r];
      /* the press, then 20 a second from the scroll delay on */
      unsigned want = 1 + 1 + (3000 - delay_ms - 1) / 50;
      snprintf(msg, sizeof(msg), "menu repeat: at %u Hz Down held 3 s moved %u entries, not %u (+-1)",
            hz[r], run[r].moved, want);
      CHECK(run[r].moved + 1 >= want && run[r].moved <= want + 1, msg);
      snprintf(msg, sizeof(msg), "menu repeat: at %u Hz the first repeat came at %u ms, not at the scroll delay (%u ms) or within a frame of it",
            hz[r], run[r].first_repeat_ms, delay_ms);
      CHECK(run[r].first_repeat_ms >= delay_ms
            && run[r].first_repeat_ms * 10 <= delay_ms * 10 + (unsigned)period_ms_x10 + 10, msg);
      snprintf(msg, sizeof(msg), "menu repeat: with fast scrolling at %u Hz Down held 3 s moved %u entries, at 60 Hz %u (+-2)",
            hz[r], fast[r].moved, fast[1].moved);
      CHECK(fast[r].moved + 2 >= fast[1].moved && fast[r].moved <= fast[1].moved + 2, msg);
      for (k = 0; k < r; k++)
      {
         snprintf(msg, sizeof(msg), "menu repeat: the acceleration after 3 s differs between %u Hz (%u) and %u Hz (%u)",
               hz[k], run[k].accel, hz[r], run[r].accel);
         CHECK(run[k].accel + 1 >= run[r].accel && run[r].accel + 1 >= run[k].accel, msg);
         snprintf(msg, sizeof(msg), "menu repeat: the speed-up time after 3 s differs by more than a frame between %u Hz and %u Hz",
               hz[k], hz[r]);
         CHECK(run[k].press_time - run[r].press_time < 35000 && run[r].press_time - run[k].press_time < 35000, msg);
      }
   }
   /* the faster the display, the finer the steps */
   snprintf(msg, sizeof(msg), "menu repeat: fast scrolling at 240 Hz moved up to %u entries in a frame, not one at a time",
         fast[4].max_step);
   CHECK(fast[4].max_step == 1, msg);
   snprintf(msg, sizeof(msg), "menu repeat: fast scrolling moved on %u frames at 240 Hz and %u at 60 Hz: the faster display was not used",
         fast[4].frames_moving, fast[1].frames_moving);
   CHECK(fast[4].frames_moving * 2 > fast[1].frames_moving * 3, msg);

   pm_set_menu(false);
   run_loop_frames(3);
   if (failures == had)
      printf("[pass] menu repeat: Down held 3 s moves %u entries at 30, 60,"
            " 120, 144 and 240 Hz alike (%u with fast scrolling), starts at"
            " the scroll delay; fast scrolling moves on %u frames at 240 Hz"
            " a step at a time, on %u at 60 Hz up to %u at a time\n",
            run[1].moved, fast[1].moved, fast[4].frames_moving,
            fast[1].frames_moving, fast[1].max_step);
#else
   printf("[skip] menu repeat: needs the menu\n");
#endif
}

/* A core asks for a stick an axis at a time, and each axis asked for
 * was its own six goes at the pad with a deadzone: the stick's four
 * axes for its magnitude, a square root, and the axis's two again. The
 * first axis asked for in a frame now reads the whole stick, and the
 * rest of the frame's reads of it are given what was read. (The pad is
 * the frontend's own copy of it by then, so this cannot be counted at
 * the driver: the lane looks at the frame's cache instead.) */
static void lane_sticks_read_once(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
   settings_t *settings           = config_get_ptr();
   input_driver_state_t *input_st = input_state_get_ptr();
   const input_device_driver_t *joypad_real = input_st->primary_joypad;
   struct retro_keybind saved_auto[8];
   float saved_deadzone = settings->floats.input_analog_deadzone;
   unsigned saved_mode  = settings->uints.input_analog_dpad_mode[0];
   unsigned had         = failures;
   int16_t  v[2][4];
   unsigned i, pass, s;

   if (!joypad_real)
   {
      CHECK(false, "sticks read once: no joypad driver");
      return;
   }
   /* the stand-in pad, with the sticks on its axes 0-3 */
   syn_joypad               = *joypad_real;
   syn_joypad.button        = syn_button;
   syn_joypad.axis          = syn_axis;
   syn_joypad.state         = syn_state;
   syn_joypad.get_buttons   = syn_get_buttons;
   input_st->primary_joypad = &syn_joypad;
   syn_hat     = 0;
   syn_buttons = 0;

   memcpy(saved_auto, &input_autoconf_binds[0][RARCH_ANALOG_LEFT_X_PLUS],
         sizeof(saved_auto));
   for (i = 0; i < 4; i++)
   {
      input_autoconf_binds[0][RARCH_ANALOG_LEFT_X_PLUS + 2 * i].joyaxis     = AXIS_POS(i);
      input_autoconf_binds[0][RARCH_ANALOG_LEFT_X_PLUS + 2 * i + 1].joyaxis = AXIS_NEG(i);
   }
   input_config_binds_changed();
   settings->floats.input_analog_deadzone = 0.2f;
   /* the sticks as sticks: a mode that gives one to the D-pad would
    * have the first read of all come back empty */
   settings->uints.input_analog_dpad_mode[0] = ANALOG_DPAD_NONE;
   syn_axes[0] =  20000;
   syn_axes[1] = -15000;
   syn_axes[2] =   9000;
   syn_axes[3] =  30000;

   run_loop_frames(2);
   input_driver_poll();
   /* a stick has a slot of its own for its read: the left one's is
    * the first, the right one's the second, and there are the two */
   CHECK(!((input_st->frame_valid.sticks[0] | input_st->frame_valid.sticks[1]) & 1),
         "sticks read once: a stick's read was kept across the poll");

   /* what a core does: x and y of each stick, and all of it once more,
    * as a core run again in the frame would */
   for (pass = 0; pass < 2; pass++)
      for (i = 0; i < 4; i++)
         v[pass][i] = input_driver_state_wrapper(0, RETRO_DEVICE_ANALOG,
               i >> 1, i & 1);

   CHECK(v[0][0] > 0 && v[0][1] < 0 && v[0][2] > 0 && v[0][3] > 0,
         "sticks read once: the four axes do not read the way the pad holds them");
   CHECK(!memcmp(v[0], v[1], sizeof(v[0])),
         "sticks read once: a second read in the frame gave another value");

   /* each stick's read is kept, x and y both, from the one read ... */
   for (s = 0; s < 2; s++)
   {
      CHECK(input_st->frame_valid.sticks[s] & 1,
            s ? "sticks read once: the right stick's read was not kept"
              : "sticks read once: the left stick's read was not kept");
      CHECK(   input_st->stick_cache[0][s][0] == v[0][s * 2]
            && input_st->stick_cache[0][s][1] == v[0][s * 2 + 1],
            s ? "sticks read once: the right stick's two axes were not kept from one read"
              : "sticks read once: the left stick's two axes were not kept from one read");
      /* ... and is what the next read of it in the frame is given,
       * either axis */
      input_st->stick_cache[0][s][0] = (int16_t)(4321 + s);
      input_st->stick_cache[0][s][1] = (int16_t)(1234 + s);
      CHECK(   input_driver_state_wrapper(0, RETRO_DEVICE_ANALOG, s,
                  RETRO_DEVICE_ID_ANALOG_X) == (int16_t)(4321 + s)
            && input_driver_state_wrapper(0, RETRO_DEVICE_ANALOG, s,
                  RETRO_DEVICE_ID_ANALOG_Y) == (int16_t)(1234 + s),
            "sticks read once: a read later in the frame went to the pad again");
   }
   /* until the next poll: it drops both, and a read goes to the pad */
   input_driver_poll();
   CHECK(!((input_st->frame_valid.sticks[0] | input_st->frame_valid.sticks[1]) & 1),
         "sticks read once: the poll did not drop the reads it found kept");
   for (s = 0; s < 2; s++)
      CHECK(   input_driver_state_wrapper(0, RETRO_DEVICE_ANALOG, s,
                  RETRO_DEVICE_ID_ANALOG_X) == v[0][s * 2]
            && input_driver_state_wrapper(0, RETRO_DEVICE_ANALOG, s,
                  RETRO_DEVICE_ID_ANALOG_Y) == v[0][s * 2 + 1],
            "sticks read once: a read was kept past the next poll");

   memcpy(&input_autoconf_binds[0][RARCH_ANALOG_LEFT_X_PLUS], saved_auto,
         sizeof(saved_auto));
   input_config_binds_changed();
   settings->floats.input_analog_deadzone = saved_deadzone;
   settings->uints.input_analog_dpad_mode[0] = saved_mode;
   memset(syn_axes, 0, sizeof(syn_axes));
   input_st->primary_joypad = joypad_real;
   run_loop_frames(2);

   if (failures == had)
      printf("[pass] sticks read once: a stick's two axes come from one read of"
            " it a frame, later reads in the frame are given that read, and the"
            " poll drops it\n");
#else
   printf("[skip] sticks read once: no test drivers in this build\n");
#endif
}

#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
/* one frame with the stand-in pad's buttons @down: what the core saw */
static unsigned map_frame(uint32_t down, void (*trace_last)(unsigned*, int*))
{
   unsigned seen;
   int axes[4];
   syn_buttons = down;
   run_loop_frames(1);
   trace_last(&seen, axes);
   return seen & 0xffff;
}
#endif

/* The frontend keeps what it works out from a port's mapping: which of
 * its binds a pad could be behind, the pad binds resolved between the
 * user's and the controller's profile, the controller's mask for the
 * poll. Kept things are only right while everything they were worked
 * out from is as it was, and one that misses a change gives the core
 * input that looks right and is not. So: every way a port's mapping
 * changes, and what the core sees in the very next frame - read button
 * by button and as a mask, with the controller read through the
 * frontend's per-poll copy as the desktop drivers' are. */
static void lane_mapping_changes(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
   enum { A = RETRO_DEVICE_ID_JOYPAD_A, B = RETRO_DEVICE_ID_JOYPAD_B };
   static const char *how[2]       = { "button by button", "as a mask" };
   input_driver_state_t *input_st  = input_state_get_ptr();
   settings_t *settings            = config_get_ptr();
   retro_keybind_set *saved_user   = (retro_keybind_set*)malloc(MAX_USERS * sizeof(*saved_user));
   retro_keybind_set *saved_auto   = (retro_keybind_set*)malloc(MAX_USERS * sizeof(*saved_auto));
   const input_device_driver_t *joypad_real;
   void (*trace)(int, int);
   void (*trace_last)(unsigned*, int*);
   void    *core;
   unsigned had = failures;
   unsigned saved_index, saved_remap_a, m, seen;
   char     why[160];

   if (   !saved_user || !saved_auto
       || !(core = dlopen(core_path_g, RTLD_NOW))
       || !(trace = (void (*)(int, int))dlsym(core, "harness_core_trace"))
       || !(trace_last = (void (*)(unsigned*, int*))dlsym(core, "harness_core_trace_last"))
       || !input_st->primary_joypad)
   {
      CHECK(false, "mapping changes: the harness core's trace entry points or the joypad driver");
      free(saved_user);
      free(saved_auto);
      return;
   }
   {
      unsigned u;
      for (u = 0; u < MAX_USERS; u++)
      {
         memcpy(saved_user[u], input_config_binds[u],   sizeof(*saved_user));
         memcpy(saved_auto[u], input_autoconf_binds[u], sizeof(*saved_auto));
      }
   }
   saved_index   = settings->uints.input_joypad_index[0];
   saved_remap_a = settings->uints.input_remap_ids[0][A];

   joypad_real              = input_st->primary_joypad;
   syn_joypad               = *joypad_real;
   syn_joypad.button        = syn_button;
   syn_joypad.axis          = syn_axis;
   syn_joypad.state         = syn_state;
   syn_joypad.get_buttons   = syn_get_buttons;
   input_st->primary_joypad = &syn_joypad;
   input_driver_set_snapshot_bridge(true);
   syn_hat = 0;
   memset(syn_axes, 0, sizeof(syn_axes));
   fast_forward(true);

#define MAP_SEES(down, want, what) do { \
      seen = map_frame((down), trace_last) & ((1u << A) | (1u << B)); \
      snprintf(why, sizeof(why), "mapping changes, read %s: %s", how[m], (what)); \
      CHECK(seen == (unsigned)(want), why); \
   } while (0)

   for (m = 0; m < 2; m++)
   {
      trace((int)m + 1, 0);

      /* where it starts: the controller's profile has A on button 3,
       * the user has bound no pad button to it */
      input_config_bind_edit(0, A)->joykey    = NO_BTN;
      input_config_bind_edit(0, A)->joyaxis   = AXIS_NONE;
      input_config_bind_edit(0, B)->joykey    = NO_BTN;
      input_config_bind_edit(0, B)->joyaxis   = AXIS_NONE;
      input_autoconf_bind_edit(0, A)->joykey  = 3;
      input_autoconf_bind_edit(0, A)->joyaxis = AXIS_NONE;
      input_autoconf_bind_edit(0, B)->joykey  = NO_BTN;
      input_autoconf_bind_edit(0, B)->joyaxis = AXIS_NONE;
      run_loop_frames(2);
      MAP_SEES(1u << 3, 1u << A, "the profile's button is not A to begin with");

      /* the user binds another button to it */
      input_config_bind_edit(0, A)->joykey = 4;
      MAP_SEES(1u << 3, 0,       "after the user's bind, the profile's button still presses A");
      MAP_SEES(1u << 4, 1u << A, "after the user's bind, the button bound does not press A");
      /* ... and takes the bind away again */
      input_config_bind_edit(0, A)->joykey = NO_BTN;
      MAP_SEES(1u << 4, 0,       "the user's bind cleared, its button still presses A");
      MAP_SEES(1u << 3, 1u << A, "the user's bind cleared, the profile's button does not press A");

      /* autoconfiguration gives the controller another profile */
      input_autoconf_bind_edit(0, A)->joykey = 5;
      MAP_SEES(1u << 3, 0,       "a new profile, the old profile's button still presses A");
      MAP_SEES(1u << 5, 1u << A, "a new profile, its button does not press A");

      /* another controller is chosen for the port (Device Index): the
       * setting is written, as the menu writes it, and nothing is told */
      input_autoconf_bind_edit(1, A)->joykey  = 6;
      input_autoconf_bind_edit(1, A)->joyaxis = AXIS_NONE;
      run_loop_frames(1);
      settings->uints.input_joypad_index[0] = 1;
      syn_pad_index                         = 1;
      MAP_SEES(1u << 5, 0,       "another controller chosen, the first one's profile still presses A");
      MAP_SEES(1u << 6, 1u << A, "another controller chosen, its profile's button does not press A");
      settings->uints.input_joypad_index[0] = 0;
      syn_pad_index                         = 0;
      MAP_SEES(1u << 6, 0,       "the first controller chosen again, the other's profile still presses A");
      MAP_SEES(1u << 5, 1u << A, "the first controller chosen again, its button does not press A");

      /* the button is remapped: what the pad's A is to the core */
      input_config_set_remap_id(0, A, B);
      MAP_SEES(1u << 5, 1u << B, "remapped to B, the button does not press B alone");
      input_config_set_remap_id(0, A, A);
      MAP_SEES(1u << 5, 1u << A, "the remap undone, the button does not press A");

      /* the controller is unplugged: its profile goes */
      input_config_reset_autoconfig_binds(0);
      MAP_SEES(1u << 5, 0,       "the controller unplugged, its profile's button still presses A");
      /* ... and plugged in again */
      input_autoconf_bind_edit(0, A)->joykey  = 5;
      input_autoconf_bind_edit(0, A)->joyaxis = AXIS_NONE;
      MAP_SEES(1u << 5, 1u << A, "the controller plugged in again, its button does not press A");

      /* (A configuration being loaded goes through the same count as
       * these; that it does is the lane before this one's to check.
       * It is not done here: it also forgets which controllers are
       * connected, which the lanes after this one need.) */

      {
         /* a row is written back only if it was changed, and through
          * the call that makes it its user's own if it is not yet */
         unsigned u;
         for (u = 0; u < MAX_USERS; u++)
         {
            if (memcmp(input_config_binds[u], saved_user[u], sizeof(*saved_user)))
               memcpy(input_config_bind_edit(u, 0), saved_user[u],
                     sizeof(*saved_user));
            if (memcmp(input_autoconf_binds[u], saved_auto[u], sizeof(*saved_auto)))
               memcpy(input_autoconf_bind_edit(u, 0), saved_auto[u],
                     sizeof(*saved_auto));
         }
      }
      input_config_binds_changed();
      syn_buttons = 0;
      run_loop_frames(2);
   }
#undef MAP_SEES

   settings->uints.input_joypad_index[0]    = saved_index;
   input_config_set_remap_id(0, A, saved_remap_a);
   syn_pad_index                            = 0;
   syn_buttons                              = 0;
   trace(0, 0);
   fast_forward(false);
   run_loop_frames(5);
   input_driver_set_snapshot_bridge(false);
   input_st->primary_joypad = joypad_real;
   dlclose(core);
   free(saved_user);
   free(saved_auto);
   if (failures == had)
      printf("[pass] mapping changes: the core sees each in the next frame, read"
            " button by button and as a mask - the user's bind set and"
            " cleared, a new profile, another controller chosen for the port"
            " and the first again, a remap and its undoing, the controller"
            " unplugged and plugged in\n");
#else
   printf("[skip] mapping changes: no test drivers in this build\n");
#endif
}

/* A controller combination that quits, with "Confirm Quit" on: one
 * press asks for a second, and is not itself the second.
 *
 * Two things made one press both. A combination of buttons is true on
 * every frame it is held, where the quit key counts on the frame it
 * goes down: the frame after "Press again to quit" was taken as the
 * press asked for (#19642). And a combination that is a button held
 * for two seconds started its two seconds again the frame after they
 * ran out, so four seconds of holding were two presses. */
static void lane_quit_combo(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
   input_driver_state_t *input_st  = input_state_get_ptr();
   settings_t *settings            = config_get_ptr();
   const input_device_driver_t *joypad_real;
   struct retro_keybind saved_l2, saved_r2;
   input_bits_t bits;
   retro_time_t t;
   unsigned had = failures;
   unsigned saved_combo, i, reports, quits;
   bool     saved_confirm;

   /* --- the combination itself, with a clock of the lane's own */

   /* buttons together: true for as long as they are held, which is
    * why whoever asks counts the frame it starts */
   BIT256_CLEAR_ALL(bits);
   BIT256_SET(bits, RETRO_DEVICE_ID_JOYPAD_L2);
   BIT256_SET(bits, RETRO_DEVICE_ID_JOYPAD_R2);
   reports = 0;
   for (i = 0, t = 1000000; i < 60; i++, t += 16667)
      if (input_driver_button_combo(INPUT_COMBO_L2_R2, t, &bits))
         reports++;
   CHECK(reports == 60, "quit combo: L2+R2 held is not reported on every frame it is held");

   /* a button held: once for the hold, however long it lasts */
   BIT256_CLEAR_ALL(bits);
   input_driver_button_combo(INPUT_COMBO_HOLD_START, t, &bits); /* let go */
   BIT256_SET(bits, RETRO_DEVICE_ID_JOYPAD_START);
   reports = 0;
   for (i = 0; i < 600; i++, t += 16667) /* ten seconds */
      if (input_driver_button_combo(INPUT_COMBO_HOLD_START, t, &bits))
         reports++;
   CHECK(reports == 1, "quit combo: Start held for ten seconds did not report once");
   /* let go, and held again: once more */
   BIT256_CLEAR_ALL(bits);
   t += 16667;
   CHECK(!input_driver_button_combo(INPUT_COMBO_HOLD_START, t, &bits),
         "quit combo: Start let go is reported");
   BIT256_SET(bits, RETRO_DEVICE_ID_JOYPAD_START);
   reports = 0;
   for (i = 0; i < 180; i++, t += 16667) /* three seconds */
      if (input_driver_button_combo(INPUT_COMBO_HOLD_START, t, &bits))
         reports++;
   CHECK(reports == 1, "quit combo: Start held again after being let go did not report once");
   /* short of the two seconds: not at all */
   BIT256_CLEAR_ALL(bits);
   t += 16667;
   input_driver_button_combo(INPUT_COMBO_HOLD_START, t, &bits);
   BIT256_SET(bits, RETRO_DEVICE_ID_JOYPAD_START);
   reports = 0;
   for (i = 0; i < 60; i++, t += 16667) /* one second */
      if (input_driver_button_combo(INPUT_COMBO_HOLD_START, t, &bits))
         reports++;
   CHECK(reports == 0, "quit combo: Start held for one second is reported");
   BIT256_CLEAR_ALL(bits);
   t += 16667;
   input_driver_button_combo(INPUT_COMBO_HOLD_START, t, &bits);

   /* --- through the frontend: L2+R2 quits, Confirm Quit on, the two
    * held for a second. Nothing may quit. */
   if (!input_st->primary_joypad)
   {
      CHECK(false, "quit combo: no joypad driver");
      return;
   }
   saved_combo   = settings->uints.input_quit_gamepad_combo;
   saved_confirm = settings->bools.confirm_quit;
   saved_l2      = input_autoconf_binds[0][RETRO_DEVICE_ID_JOYPAD_L2];
   saved_r2      = input_autoconf_binds[0][RETRO_DEVICE_ID_JOYPAD_R2];

   joypad_real              = input_st->primary_joypad;
   syn_joypad               = *joypad_real;
   syn_joypad.button        = syn_button;
   syn_joypad.axis          = syn_axis;
   syn_joypad.state         = syn_state;
   syn_joypad.get_buttons   = syn_get_buttons;
   input_st->primary_joypad = &syn_joypad;
   input_driver_set_snapshot_bridge(true);
   syn_hat     = 0;
   syn_buttons = 0;
   memset(syn_axes, 0, sizeof(syn_axes));
   input_autoconf_bind_edit(0, RETRO_DEVICE_ID_JOYPAD_L2)->joykey  = 12;
   input_autoconf_bind_edit(0, RETRO_DEVICE_ID_JOYPAD_L2)->joyaxis = AXIS_NONE;
   input_autoconf_bind_edit(0, RETRO_DEVICE_ID_JOYPAD_R2)->joykey  = 13;
   input_autoconf_bind_edit(0, RETRO_DEVICE_ID_JOYPAD_R2)->joyaxis = AXIS_NONE;
   settings->uints.input_quit_gamepad_combo = INPUT_COMBO_L2_R2;
   settings->bools.confirm_quit             = true;
   run_loop_frames(3);

   syn_buttons = (1u << 12) | (1u << 13);
   quits       = 0;
   for (i = 0; i < 60; i++)
   {
      if (runloop_iterate() == -1)
         quits++;
      task_queue_check();
   }
   CHECK(quits == 0, "quit combo: with Confirm Quit on, one press of L2+R2 held for a second quit");
   syn_buttons = 0;
   run_loop_frames(5);

   settings->uints.input_quit_gamepad_combo = saved_combo;
   settings->bools.confirm_quit             = saved_confirm;
   *input_autoconf_bind_edit(0, RETRO_DEVICE_ID_JOYPAD_L2) = saved_l2;
   *input_autoconf_bind_edit(0, RETRO_DEVICE_ID_JOYPAD_R2) = saved_r2;
   run_loop_frames(3);
   input_driver_set_snapshot_bridge(false);
   input_st->primary_joypad = joypad_real;
   if (failures == had)
      printf("[pass] quit combo: with Confirm Quit on, L2+R2 held for a second"
            " does not quit; a button-held combination reports once for a"
            " hold of ten seconds, once more when held again, and not at all"
            " short of its two seconds\n");
#else
   printf("[skip] quit combo: no test drivers in this build\n");
#endif
}

/* A hotkey held through a video driver restart is one press.
 *
 * F toggles fullscreen, which restarts the driver and may replace the
 * window. The new window is not focused the moment it is made, and an
 * input driver that reads no keys without the focus gives F as up for
 * a frame, then as down again: a press, and another restart, for as
 * long as the key stays down. After a restart nothing counts until the
 * window has had the focus for a few checks (or half a second has
 * passed), and what is down then is held back until it is let go. */
static void lane_restart_hold(void)
{
   input_bits_t bits;
   unsigned had = failures;
   unsigned i, counted = 0;
   bool enabler_kept;

   input_driver_hold_over_window_change();

   /* the frame the key reads as up, with the hotkey enabler held */
   BIT256_CLEAR_ALL(bits);
   BIT256_SET(bits, RARCH_ENABLE_HOTKEY);
   input_driver_hold_bits(&bits);
   enabler_kept = BIT256_GET(bits, RARCH_ENABLE_HOTKEY) != 0;
   CHECK(enabler_kept, "restart hold: the hotkey enabler was held back");

   /* ... and the key down again, kept down past the wait whether the
    * harness's window has the focus or not */
   for (i = 0; i < 300; i++)
   {
      BIT256_CLEAR_ALL(bits);
      BIT256_SET(bits, RARCH_FULLSCREEN_TOGGLE_KEY);
      input_driver_hold_bits(&bits);
      if (BIT256_GET(bits, RARCH_FULLSCREEN_TOGGLE_KEY))
         counted++;
      retro_sleep(2);
   }
   CHECK(counted == 0,
         "restart hold: a key held through a restart counted as pressed");

   /* let go, and pressed again: that one counts */
   BIT256_CLEAR_ALL(bits);
   input_driver_hold_bits(&bits);
   BIT256_CLEAR_ALL(bits);
   BIT256_SET(bits, RARCH_FULLSCREEN_TOGGLE_KEY);
   input_driver_hold_bits(&bits);
   CHECK(BIT256_GET(bits, RARCH_FULLSCREEN_TOGGLE_KEY) != 0,
         "restart hold: a press after the key was let go did not count");
   BIT256_CLEAR_ALL(bits);
   input_driver_hold_bits(&bits);
   input_driver_hold_clear();

   if (failures == had)
      printf("[pass] restart hold: a hotkey held through a video driver"
            " restart does not count again until it has been let go, and"
            " then does; the hotkey enabler is not held back\n");
}

/* What a port's mapping is made from is counted each time it changes,
 * so that what is compiled from it can tell without looking at a bind.
 * Each way of changing it counts; frames in which nothing changes do
 * not, or a mapping would be compiled every frame; and every frame the
 * menu is open does, which is what covers the menu's own writes. */
static void lane_binds_change_count(void)
{
   input_config_binds_copy_t *sets = input_config_binds_copy_new();
   settings_t *settings    = config_get_ptr();
   unsigned had            = failures;
   unsigned g;

   run_loop_frames(3);
   g = input_config_binds_generation();
   run_loop_frames(10);
   CHECK(input_config_binds_generation() == g,
         "binds change count: it moved over frames in which nothing changed");

#define COUNTS(what, stmt) do { \
      g = input_config_binds_generation(); \
      stmt; \
      CHECK(input_config_binds_generation() != g, \
            "binds change count: not counted: " what); \
   } while (0)

   COUNTS("a user's bind handed out to be written",
         (void)input_config_bind_edit(1, RETRO_DEVICE_ID_JOYPAD_B));
   COUNTS("a pad's autoconfig bind handed out to be written",
         (void)input_autoconf_bind_edit(15, RETRO_DEVICE_ID_JOYPAD_B));
   if (sets)
      COUNTS("every user's binds put back from a copy",
            input_config_binds_copy_restore(sets));
   COUNTS("the pad a port has",
         input_config_set_joypad_index(15, settings->uints.input_joypad_index[15]));
   COUNTS("the kind of device a port has",
         input_config_set_device(15, settings->uints.input_libretro_device[15]));
   COUNTS("a pad's autoconfig binds cleared",
         input_config_reset_autoconfig_binds(15));
   COUNTS("the port map made again", input_remapping_update_port_map());
#undef COUNTS

   /* reading one is not a change */
   g = input_config_binds_generation();
   (void)input_config_bind(0, RETRO_DEVICE_ID_JOYPAD_B);
   (void)input_autoconf_bind(0, RETRO_DEVICE_ID_JOYPAD_B);
   if (sets)
      input_config_binds_copy_take(sets);
   CHECK(input_config_binds_generation() == g,
         "binds change count: reading a bind counted as a change");

#ifdef HAVE_MENU
   /* every frame the menu is open */
   if (!menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_loop_frames(2);
   if (menu_is_up())
   {
      g = input_config_binds_generation();
      run_loop_frames(5);
      CHECK(input_config_binds_generation() - g >= 5,
            "binds change count: frames with the menu open were not counted");
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
      run_loop_frames(3);
      g = input_config_binds_generation();
      run_loop_frames(10);
      CHECK(input_config_binds_generation() == g,
            "binds change count: it kept moving after the menu closed");
   }
#endif
   input_config_binds_copy_free(sets);

   if (failures == had)
      printf("[pass] binds change count: each way of changing a port's"
            " mapping is counted, a frame in which nothing changes is not,"
            " and every frame the menu is open is\n");
}

/* The pointers an input driver publishes at its poll are the frontend's
 * to answer for: the mouse, the pointer and the lightgun's aim, the same
 * for every reader of the frame - a wheel notch is not used up by being
 * read - and placed in the viewport as the translation places them. A
 * driver that publishes nothing is asked itself, as before. */
static void lane_pointer_store(void)
{
   input_pointer_frame_t f[2];
   struct video_viewport vp;
   uint32_t want_confined = 0, want_viewport = 0, want_screen = 0, scr2 = 0;
   unsigned had = failures;
   bool placed;

   run_loop_frames(2);
   memset(f, 0, sizeof(f));
   f[0].pos     = VIDEO_POS_PACK(40, 30);
   f[0].rel     = VIDEO_POS_PACK(-3, 7);
   f[0].buttons = INPUT_POINTER_RIGHT | INPUT_POINTER_WHEEL_DOWN;
   f[1].pos     = VIDEO_POS_PACK(5, 6);
   f[1].buttons = INPUT_POINTER_LEFT;
   input_driver_publish_pointers(f, 2, INPUT_POINTERS_MOUSE_3_TOUCHES);

   /* the mouse */
   CHECK(input_driver_device_state(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X) == -3
      && input_driver_device_state(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y) == 7,
         "pointer store: the mouse's motion is not what was published");
   CHECK(input_driver_device_state(0, RARCH_DEVICE_MOUSE_SCREEN, 0, RETRO_DEVICE_ID_MOUSE_X) == 40
      && input_driver_device_state(0, RARCH_DEVICE_MOUSE_SCREEN, 0, RETRO_DEVICE_ID_MOUSE_Y) == 30,
         "pointer store: the mouse's place in the window is not what was published");
   CHECK(input_driver_device_pos(0, RARCH_DEVICE_MOUSE_SCREEN, 0) == VIDEO_POS_PACK(40, 30)
      && input_driver_device_pos(0, RETRO_DEVICE_MOUSE, 0) == VIDEO_POS_PACK(-3, 7),
         "pointer store: the packed position is not the published word");
   CHECK( input_driver_device_state(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT)
      && !input_driver_device_state(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT)
      && !input_driver_device_state(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE),
         "pointer store: the mouse's buttons are not what was published");
   CHECK(input_driver_device_state(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN)
      && input_driver_device_state(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN)
      && input_driver_device_state(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN),
         "pointer store: a wheel notch was used up by being read");
   CHECK(!input_driver_device_state(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELUP),
         "pointer store: a wheel direction that did not turn reads as turned");
   /* port 1 reads device 1 */
   CHECK( input_driver_device_state(1, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT)
      && !input_driver_device_state(1, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT)
      &&  input_driver_device_state(1, RARCH_DEVICE_MOUSE_SCREEN, 0, RETRO_DEVICE_ID_MOUSE_X) == 5,
         "pointer store: the second port does not read the second device");
   CHECK(!input_driver_device_state(2, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT)
      && !input_driver_device_state(2, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT),
         "pointer store: a port with no device reads one");

   /* the pointer and the lightgun's aim, against the translation itself */
   memset(&vp, 0, sizeof(vp));
   placed = video_driver_get_viewport_info(&vp)
      && video_driver_translate_coord_viewport(&vp, 40, 30, &want_confined, &want_screen, false)
      && video_driver_translate_coord_viewport(&vp, 40, 30, &want_viewport, &scr2, true);
   if (placed)
   {
      CHECK(input_driver_device_state(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) == VIDEO_POS_X(want_confined)
         && input_driver_device_state(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y) == VIDEO_POS_Y(want_confined)
         && input_driver_device_pos(0, RETRO_DEVICE_POINTER, 0) == want_confined,
            "pointer store: the pointer is not where the translation puts it");
      CHECK(input_driver_device_state(0, RARCH_DEVICE_POINTER_SCREEN, 0, RETRO_DEVICE_ID_POINTER_X) == VIDEO_POS_X(want_screen)
         && input_driver_device_pos(0, RARCH_DEVICE_POINTER_SCREEN, 0) == want_screen,
            "pointer store: the screen pointer is not where the translation puts it");
      CHECK(input_driver_device_state(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) == VIDEO_POS_X(want_viewport)
         && input_driver_device_state(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y) == VIDEO_POS_Y(want_viewport),
            "pointer store: the lightgun does not aim where the translation puts it");
   }
   /* a mouse stands for three touches: any button, right or middle, middle */
   CHECK( input_driver_device_state(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) == (placed ? 1 : 0)
      &&  input_driver_device_state(0, RETRO_DEVICE_POINTER, 1, RETRO_DEVICE_ID_POINTER_PRESSED) == (placed ? 1 : 0)
      && !input_driver_device_state(0, RETRO_DEVICE_POINTER, 2, RETRO_DEVICE_ID_POINTER_PRESSED)
      && !input_driver_device_state(0, RETRO_DEVICE_POINTER, 3, RETRO_DEVICE_ID_POINTER_PRESSED),
         "pointer store: the mouse's buttons do not stand for the three touches they did");
   CHECK(input_driver_device_state(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_X) == -3,
         "pointer store: the lightgun's old relative X is not the mouse's motion");

   /* the next poll, by a driver that publishes nothing: asked itself */
   run_loop_frames(2);
   CHECK(!input_driver_device_state(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT)
      && !input_driver_device_state(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN)
      && input_driver_device_state(0, RARCH_DEVICE_MOUSE_SCREEN, 0, RETRO_DEVICE_ID_MOUSE_X) != 40,
         "pointer store: what was published outlived the poll after it");

   if (failures == had)
      printf("[pass] pointer store: a published mouse answers for the mouse,"
            " the pointer%s and three touches; a wheel notch reads the same"
            " every time in its frame; the next poll by a driver that"
            " publishes nothing is asked itself\n",
            placed ? " and the lightgun's aim, placed as the translation places them" : "");
}

/* Aim From Analog Stick: a port's stick is where its lightgun or
 * pointer points. */
/* Where a stick's values come from. Its readers took the binds'
 * records for it each read; they take what is kept - whether the
 * stick's four binds are usable, with the port's keys; the pad's axis
 * and button behind each, with the port's resolved set. Held to what
 * the records say: the controller's profile where the port has nothing
 * of its own, the port's own bind over the profile, a pad's button
 * where the axis is at rest, nothing at all from a stick one of whose
 * binds is not usable - and each of those again as soon as a bind
 * changes. */
static void lane_stick_sources(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
   settings_t *settings           = config_get_ptr();
   input_driver_state_t *input_st = input_state_get_ptr();
   const input_device_driver_t *joypad_real = input_st->primary_joypad;
   struct retro_keybind saved_auto[8];
   struct retro_keybind saved_own[8];
   float saved_deadzone = settings->floats.input_analog_deadzone;
   unsigned saved_mode  = settings->uints.input_analog_dpad_mode[0];
   unsigned had         = failures;
   unsigned i;
#define SS_READ(stick, axis) (input_driver_poll(), \
      input_driver_state_wrapper(0, RETRO_DEVICE_ANALOG, (stick), (axis)))
#define SS_LX() SS_READ(RETRO_DEVICE_INDEX_ANALOG_LEFT,  RETRO_DEVICE_ID_ANALOG_X)
#define SS_LY() SS_READ(RETRO_DEVICE_INDEX_ANALOG_LEFT,  RETRO_DEVICE_ID_ANALOG_Y)
#define SS_RX() SS_READ(RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X)

   if (!joypad_real)
   {
      CHECK(false, "stick sources: no joypad driver");
      return;
   }
   syn_joypad               = *joypad_real;
   syn_joypad.button        = syn_button;
   syn_joypad.axis          = syn_axis;
   syn_joypad.state         = syn_state;
   syn_joypad.get_buttons   = syn_get_buttons;
   input_st->primary_joypad = &syn_joypad;
   syn_hat     = 0;
   syn_buttons = 0;
   memset(syn_axes, 0, sizeof(syn_axes));

   memcpy(saved_auto, &input_autoconf_binds[0][RARCH_ANALOG_LEFT_X_PLUS], sizeof(saved_auto));
   memcpy(saved_own,  &input_config_binds[0][RARCH_ANALOG_LEFT_X_PLUS],   sizeof(saved_own));
   for (i = 0; i < 8; i++)
   {
      struct retro_keybind *own = &input_config_binds[0][RARCH_ANALOG_LEFT_X_PLUS + i];
      own->joykey  = NO_BTN;
      own->joyaxis = AXIS_NONE;
      RETRO_KEYBIND_SET_VALID(own, true);
      input_autoconf_binds[0][RARCH_ANALOG_LEFT_X_PLUS + i].joykey = NO_BTN;
   }
   /* the profile: the sticks on the pad's axes 0 to 3 */
   for (i = 0; i < 4; i++)
   {
      input_autoconf_binds[0][RARCH_ANALOG_LEFT_X_PLUS + 2 * i].joyaxis     = AXIS_POS(i);
      input_autoconf_binds[0][RARCH_ANALOG_LEFT_X_PLUS + 2 * i + 1].joyaxis = AXIS_NEG(i);
   }
   settings->floats.input_analog_deadzone    = 0.0f;
   settings->uints.input_analog_dpad_mode[0] = ANALOG_DPAD_NONE;
   binds_written_by_a_lane();
   run_loop_frames(2);

   /* the controller's profile */
   syn_axes[0] = 20000;
   syn_axes[1] = -12000;
   CHECK(SS_LX() > 15000, "stick sources: the profile's axis, held right, reads wrong");
   CHECK(SS_LY() < -8000, "stick sources: the profile's axis, held up, reads wrong");

   /* the port's own bind, over the profile: x+ on the pad's axis 3 */
   input_config_binds[0][RARCH_ANALOG_LEFT_X_PLUS].joyaxis = AXIS_POS(3);
   binds_written_by_a_lane();
   CHECK(SS_LX() == 0, "stick sources: with x+ bound to another axis, at rest,"
         " the profile's axis still reads");
   syn_axes[3] = 25000;
   CHECK(SS_LX() > 20000, "stick sources: the port's own axis, held, reads wrong");
   syn_axes[3] = 0;
   input_config_binds[0][RARCH_ANALOG_LEFT_X_PLUS].joyaxis = AXIS_NONE;
   binds_written_by_a_lane();
   CHECK(SS_LX() > 15000, "stick sources: with the port's own bind taken away"
         " the profile's axis does not read again");

   /* a pad's button behind a direction, the axis at rest */
   syn_axes[0] = 0;
   input_autoconf_binds[0][RARCH_ANALOG_LEFT_X_MINUS].joykey = 9;
   binds_written_by_a_lane();
   CHECK(SS_LX() == 0, "stick sources: a button bound to left, not held, reads wrong");
   syn_buttons = 1u << 9;
   CHECK(SS_LX() == -0x7fff, "stick sources: a button bound to left, held, reads wrong");
   syn_buttons = 0;

   /* one bind of the left stick not usable: the left stick reads
    * nothing, the right one is not touched by it */
   syn_axes[0] = 20000;
   syn_axes[1] = -12000;
   syn_axes[2] = 18000;
   RETRO_KEYBIND_SET_VALID(&input_config_binds[0][RARCH_ANALOG_LEFT_Y_MINUS], false);
   binds_written_by_a_lane();
   CHECK(SS_LX() == 0 && SS_LY() == 0, "stick sources: a stick with a bind that"
         " is not usable reads something");
   CHECK(SS_RX() > 12000, "stick sources: the other stick does not read beside it");
   RETRO_KEYBIND_SET_VALID(&input_config_binds[0][RARCH_ANALOG_LEFT_Y_MINUS], true);
   binds_written_by_a_lane();
   CHECK(SS_LX() > 15000 && SS_LY() < -8000, "stick sources: usable again, the"
         " stick reads something");

   memcpy(&input_autoconf_binds[0][RARCH_ANALOG_LEFT_X_PLUS], saved_auto, sizeof(saved_auto));
   memcpy(&input_config_binds[0][RARCH_ANALOG_LEFT_X_PLUS],   saved_own,  sizeof(saved_own));
   binds_written_by_a_lane();
   settings->floats.input_analog_deadzone    = saved_deadzone;
   settings->uints.input_analog_dpad_mode[0] = saved_mode;
   memset(syn_axes, 0, sizeof(syn_axes));
   syn_buttons = 0;
   input_st->primary_joypad = joypad_real;
   run_loop_frames(2);
#undef SS_READ
#undef SS_LX
#undef SS_LY
#undef SS_RX

   if (failures == had)
      printf("[pass] stick sources: the profile, the port's own bind over it,"
            " a button behind a direction, and nothing from a stick with a bind"
            " that is not usable\n");
#else
   printf("[skip] stick sources: needs the test drivers\n");
#endif
}

/* The RetroPad as a mask, and as sixteen buttons. A core that asks for
 * the mask is answered in one go where the port has no hold or turbo
 * going, from three masks; one of them is which of the sixteen have a
 * usable bind, and that is kept with the port's keys rather than read
 * off sixteen records. A core that asks a button at a time goes by
 * each bind's record. The two must say the same: with every bind
 * usable, with a button remapped to another, with that button's bind
 * not usable as well - then the pad's own B reaches the core again,
 * which is the kept bits' doing - and as soon as either changes.
 *
 * Twice: with the harness's own input driver, which gives no list of
 * keys, so that nothing is kept and the records are read; and with one
 * that does, so that the kept bits are what is read. */
static void mb_keys_down(void *data, unsigned port,
      const uint16_t *keys, const uint8_t *bind, unsigned count,
      uint32_t *down)
{
   /* a keyboard with nothing held */
   (void)data; (void)port; (void)keys; (void)bind; (void)count; (void)down;
}

static void lane_mask_and_buttons(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
   settings_t *settings           = config_get_ptr();
   input_driver_state_t *input_st = input_state_get_ptr();
   const input_device_driver_t *joypad_real = input_st->primary_joypad;
   struct retro_keybind saved_auto[RARCH_FIRST_CUSTOM_BIND];
   struct retro_keybind saved_own[RARCH_FIRST_CUSTOM_BIND];
   unsigned saved_remap_b = settings->uints.input_remap_ids[0][RETRO_DEVICE_ID_JOYPAD_B];
   input_driver_t *saved_input = input_st->current_driver;
   static input_driver_t mb_input;
   unsigned had           = failures;
   unsigned i, step, with_keys;
   static const char *const what[] = {
      "every bind usable", "B remapped to Y",
      "B remapped to Y and its bind not usable",
      "B's bind usable again, still remapped", "the remap undone" };

   if (!joypad_real)
   {
      CHECK(false, "mask and buttons: no joypad driver");
      return;
   }
   syn_joypad               = *joypad_real;
   syn_joypad.button        = syn_button;
   syn_joypad.axis          = syn_axis;
   syn_joypad.state         = syn_state;
   syn_joypad.get_buttons   = syn_get_buttons;
   input_st->primary_joypad = &syn_joypad;
   syn_hat = 0;
   memset(syn_axes, 0, sizeof(syn_axes));

   memcpy(saved_auto, input_autoconf_binds[0], sizeof(saved_auto));
   memcpy(saved_own,  input_config_binds[0],   sizeof(saved_own));
   /* the profile: the RetroPad's n-th on the pad's n-th button */
   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      input_config_binds[0][i].joykey    = NO_BTN;
      input_config_binds[0][i].joyaxis   = AXIS_NONE;
      RETRO_KEYBIND_SET_VALID(&input_config_binds[0][i], true);
      input_autoconf_binds[0][i].joykey  = (uint16_t)i;
      input_autoconf_binds[0][i].joyaxis = AXIS_NONE;
   }
   binds_written_by_a_lane();
   run_loop_frames(2);
   /* held: B, Y, Start and A */
   syn_buttons = (1u << RETRO_DEVICE_ID_JOYPAD_B) | (1u << RETRO_DEVICE_ID_JOYPAD_Y)
               | (1u << RETRO_DEVICE_ID_JOYPAD_START) | (1u << RETRO_DEVICE_ID_JOYPAD_A);

   for (with_keys = 0; with_keys < 2; with_keys++)
   {
   if (with_keys && saved_input)
   {
      mb_input                 = *saved_input;
      mb_input.keys_down       = mb_keys_down;
      input_st->current_driver = &mb_input;
      binds_written_by_a_lane();
      run_loop_frames(2);
   }
   for (step = 0; step < 5; step++)
   {
      unsigned mask, buttons = 0;
      switch (step)
      {
         case 1:
            input_config_set_remap_id(0, RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_Y);
            break;
         case 2:
            /* remapped and not usable: its own state passes again */
            RETRO_KEYBIND_SET_VALID(&input_config_binds[0][RETRO_DEVICE_ID_JOYPAD_B], false);
            break;
         case 3:
            RETRO_KEYBIND_SET_VALID(&input_config_binds[0][RETRO_DEVICE_ID_JOYPAD_B], true);
            break;
         case 4:
            input_config_set_remap_id(0, RETRO_DEVICE_ID_JOYPAD_B, saved_remap_b);
            break;
         default:
            break;
      }
      if (step)
         binds_written_by_a_lane();
      input_driver_poll();
      mask = (unsigned)input_driver_state_wrapper(0, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_MASK) & 0xffff;
      input_driver_poll();
      for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
         if (input_driver_state_wrapper(0, RETRO_DEVICE_JOYPAD, 0, i))
            buttons |= (1u << i);
      if (mask != buttons)
      {
         fprintf(stderr, "FAIL: mask and buttons: with %s the mask is %04x and"
               " the buttons, one at a time, are %04x (input driver %s a list"
               " of keys)\n", what[step], mask, buttons,
               with_keys ? "with" : "without");
         failures++;
      }
      if (step == 0 && mask != syn_buttons)
      {
         fprintf(stderr, "FAIL: mask and buttons: four buttons held read %04x\n", mask);
         failures++;
      }
      /* B is on Y while it is remapped and usable, and itself again
       * when its bind is not */
      if (     (step == 1 && (mask & (1u << RETRO_DEVICE_ID_JOYPAD_B)))
            || (step == 2 && !(mask & (1u << RETRO_DEVICE_ID_JOYPAD_B))))
      {
         fprintf(stderr, "FAIL: mask and buttons: with %s the mask is %04x"
               " (input driver %s a list of keys)\n", what[step], mask,
               with_keys ? "with" : "without");
         failures++;
      }
   }
   }
   input_st->current_driver = saved_input;

   input_config_set_remap_id(0, RETRO_DEVICE_ID_JOYPAD_B, saved_remap_b);
   memcpy(input_autoconf_binds[0], saved_auto, sizeof(saved_auto));
   memcpy(input_config_binds[0],   saved_own,  sizeof(saved_own));
   binds_written_by_a_lane();
   syn_buttons = 0;
   input_st->primary_joypad = joypad_real;
   run_loop_frames(2);

   if (failures == had)
      printf("[pass] mask and buttons: the RetroPad read as a mask is the"
            " sixteen read one at a time, with a bind not usable and a"
            " button remapped, from the records and from what is kept\n");
#else
   printf("[skip] mask and buttons: needs the test drivers\n");
#endif
}

/* A port's keys are made current once, at the poll, and the reads of
 * the frame take them as they are without asking whether they still
 * are. A bind changing is what makes them not: from then until the
 * next poll a read has to ask again, or it answers from the binds as
 * they were. Held to that with a keyboard that has one key down: the
 * RetroPad's B on that key reads; moved to another key after a poll,
 * the read that follows does not, and moved back it does; and the key
 * let go is seen at the next poll. */
static bool kc_g_down;
static void kc_keys_down(void *data, unsigned port,
      const uint16_t *keys, const uint8_t *bind, unsigned count,
      uint32_t *down)
{
   unsigned i;
   (void)data; (void)bind;
   for (i = 0; i < count; i++)
      if (port == 0 && kc_g_down && keys[i] == RETROK_g)
         down[i >> 5] |= (1u << (i & 31));
}

static void lane_keys_current(void)
{
   input_driver_state_t *input_st = input_state_get_ptr();
   input_driver_t *saved_input    = input_st->current_driver;
   static input_driver_t kc_input;
   struct retro_keybind *b        = &input_config_binds[0][RETRO_DEVICE_ID_JOYPAD_B];
   struct retro_keybind saved_b   = *b;
   unsigned had                   = failures;
#define KC_B() ((input_driver_state_wrapper(0, RETRO_DEVICE_JOYPAD, 0, \
            RETRO_DEVICE_ID_JOYPAD_MASK) >> RETRO_DEVICE_ID_JOYPAD_B) & 1)

   if (!saved_input)
   {
      CHECK(false, "keys current: no input driver");
      return;
   }
   kc_input                 = *saved_input;
   kc_input.keys_down       = kc_keys_down;
   input_st->current_driver = &kc_input;
   RETRO_KEYBIND_SET_KEY(b, RETROK_g);
   RETRO_KEYBIND_SET_VALID(b, true);
   binds_written_by_a_lane();
   kc_g_down = true;
   run_loop_frames(2);

   input_driver_poll();
   CHECK(KC_B(), "keys current: B on a key that is down does not read");
   CHECK(KC_B(), "keys current: ... nor at a second read in the frame");

   /* The bind changes after a poll and before the frame's first read.
    * (A read later in the frame would not do: a port's pad is put
    * together for the core once a frame, at its first read, and the
    * reads after it are given that.) */
   input_driver_poll();
   RETRO_KEYBIND_SET_KEY(b, RETROK_h);
   binds_written_by_a_lane();
   CHECK(!KC_B(), "keys current: B moved to a key that is not down still reads"
         " at the first read after: it went by the binds as they were at the poll");
   input_driver_poll();
   RETRO_KEYBIND_SET_KEY(b, RETROK_g);
   binds_written_by_a_lane();
   CHECK(KC_B(), "keys current: B moved back to the key that is down does not"
         " read at the first read after");

   /* the key let go: the next poll has it */
   kc_g_down = false;
   input_driver_poll();
   CHECK(!KC_B(), "keys current: the key let go still reads after a poll");

   *b = saved_b;
   input_st->current_driver = saved_input;
   binds_written_by_a_lane();
   run_loop_frames(2);
#undef KC_B

   if (failures == had)
      printf("[pass] keys current: a port's keys are the poll's, and a bind"
            " changed since the poll is the bind a read goes by\n");
}

/* Who has the keyboard. Game Focus, a line of text being typed and the
 * menu's wait for a key each keep the keys from the binds, and each
 * lets go of its own hold: one ending does not give the keys back
 * while another still has them. (It was one flag, and whichever ended
 * first cleared it for the others.) */
static void kh_line_done(void *userdata, const char *line)
{
   (void)userdata; (void)line;
}

static void lane_keyboard_holders(void)
{
   unsigned had = failures;

   CHECK(!input_driver_keyboard_mapping_blocked(),
         "keyboard holders: the keys are kept from the binds before anyone holds them");

   /* Game Focus, and a line typed and ended under it */
   input_keyboard_capture_hold(INPUT_KEYBOARD_CAPTURE_GAME_FOCUS);
   CHECK(input_driver_keyboard_mapping_blocked(),
         "keyboard holders: Game Focus does not keep the keys from the binds");
   CHECK(!input_driver_text_entry_active(),
         "keyboard holders: Game Focus reads as a line being typed");
   input_driver_text_entry_open(NULL, kh_line_done, "test", (enum input_text_type)0);
   CHECK(input_driver_keyboard_mapping_blocked() && input_driver_text_entry_active(),
         "keyboard holders: a line being typed does not read as one");
   input_driver_keyboard_line_end();
   CHECK(input_driver_keyboard_mapping_blocked(),
         "keyboard holders: ending a line gave the keys back to the binds with Game Focus on");
   CHECK(!input_driver_text_entry_active(),
         "keyboard holders: a line that has ended still reads as being typed");
   input_keyboard_capture_release(INPUT_KEYBOARD_CAPTURE_GAME_FOCUS);
   CHECK(!input_driver_keyboard_mapping_blocked(),
         "keyboard holders: with Game Focus off and no line the keys are still kept");

   /* a line being typed, and Game Focus on and off under it */
   input_driver_text_entry_open(NULL, kh_line_done, "test", (enum input_text_type)0);
   input_keyboard_capture_hold(INPUT_KEYBOARD_CAPTURE_GAME_FOCUS);
   input_keyboard_capture_release(INPUT_KEYBOARD_CAPTURE_GAME_FOCUS);
   CHECK(input_driver_keyboard_mapping_blocked() && input_driver_text_entry_active(),
         "keyboard holders: Game Focus going off gave the keys back in the middle of a line");
   input_driver_keyboard_line_end();
   CHECK(!input_driver_keyboard_mapping_blocked(),
         "keyboard holders: the line ended and the keys are still kept");

   /* the menu's wait for a key, and a line under it */
   input_driver_set_keyboard_mapping_blocked(true);
   CHECK(input_driver_keyboard_mapping_blocked() && !input_driver_text_entry_active(),
         "keyboard holders: the wait for a key does not keep the keys, or reads as a line");
   input_driver_text_entry_open(NULL, kh_line_done, "test", (enum input_text_type)0);
   input_driver_keyboard_line_end();
   CHECK(input_driver_keyboard_mapping_blocked(),
         "keyboard holders: ending a line gave the keys back during the wait for a key");
   input_driver_set_keyboard_mapping_blocked(false);
   CHECK(!input_driver_keyboard_mapping_blocked(),
         "keyboard holders: the wait is over and the keys are still kept");

   if (failures == had)
      printf("[pass] keyboard holders: Game Focus, a line of text and the wait"
            " for a key each hold the keyboard and let go of their own hold\n");
}

/* There is one set of hotkeys: the first user's, whichever port the
 * hotkeys are read on. The configuration loads and saves only that
 * set and the menu shows only that one; but the hotkey pass read the
 * row of the port it was on, so with "Hotkeys Follow Player 1" and
 * that player on another port it read a row nothing fills, and a
 * hotkey on a key of the keyboard did nothing. Held to it with a
 * keyboard that has the pause hotkey's key down: it pauses with the
 * hotkeys on the first port, and with them on the second. */
static bool os_g_down;
static void os_keys_down(void *data, unsigned port,
      const uint16_t *keys, const uint8_t *bind, unsigned count,
      uint32_t *down)
{
   unsigned i;
   /* one keyboard, whichever port asks */
   (void)data; (void)port; (void)bind;
   for (i = 0; i < count; i++)
      if (os_g_down && keys[i] == RETROK_g)
         down[i >> 5] |= (1u << (i & 31));
}

static void os_tap(void)
{
   os_g_down = true;
   run_loop_frames(3);
   os_g_down = false;
   run_loop_frames(3);
}

static void lane_hotkeys_one_set(void)
{
   settings_t *settings           = config_get_ptr();
   input_driver_state_t *input_st = input_state_get_ptr();
   input_driver_t *saved_input    = input_st->current_driver;
   static input_driver_t os_input;
   struct retro_keybind saved_pause = input_config_binds[0][RARCH_PAUSE_TOGGLE];
   struct retro_keybind saved_en    = input_config_binds[0][RARCH_ENABLE_HOTKEY];
   struct retro_keybind saved_auto0 = input_autoconf_binds[0][RARCH_ENABLE_HOTKEY];
   struct retro_keybind saved_auto1 = input_autoconf_binds[1][RARCH_ENABLE_HOTKEY];
   bool saved_follow                = settings->bools.input_hotkey_follows_player1;
   unsigned saved_map               = settings->uints.input_remap_port_map[0][0];
   unsigned had                     = failures;

   if (!saved_input)
   {
      CHECK(false, "one hotkey set: no input driver");
      return;
   }
   os_input                 = *saved_input;
   os_input.keys_down       = os_keys_down;
   input_st->current_driver = &os_input;

   /* pause on a key, and no enabler anywhere */
   RETRO_KEYBIND_SET_KEY(&input_config_binds[0][RARCH_PAUSE_TOGGLE], RETROK_g);
   RETRO_KEYBIND_SET_VALID(&input_config_binds[0][RARCH_PAUSE_TOGGLE], true);
   RETRO_KEYBIND_SET_KEY(&input_config_binds[0][RARCH_ENABLE_HOTKEY], RETROK_UNKNOWN);
   input_config_binds[0][RARCH_ENABLE_HOTKEY].joykey    = NO_BTN;
   input_config_binds[0][RARCH_ENABLE_HOTKEY].joyaxis   = AXIS_NONE;
   input_autoconf_binds[0][RARCH_ENABLE_HOTKEY].joykey  = NO_BTN;
   input_autoconf_binds[0][RARCH_ENABLE_HOTKEY].joyaxis = AXIS_NONE;
   input_autoconf_binds[1][RARCH_ENABLE_HOTKEY].joykey  = NO_BTN;
   input_autoconf_binds[1][RARCH_ENABLE_HOTKEY].joyaxis = AXIS_NONE;
   binds_written_by_a_lane();
   run_loop_frames(3);
   CHECK(!hf_paused(), "one hotkey set: paused to begin with");

   /* the hotkeys on the first port */
   os_tap();
   CHECK(hf_paused(), "one hotkey set: the pause key did not pause with the hotkeys on the first port");
   os_tap();
   CHECK(!hf_paused(), "one hotkey set: ... and did not unpause");

   /* the hotkeys on the second: the same set, the same key */
   settings->bools.input_hotkey_follows_player1 = true;
   settings->uints.input_remap_port_map[0][0]   = 1;
   run_loop_frames(2);
   os_tap();
   CHECK(hf_paused(), "one hotkey set: the pause key did not pause with the hotkeys"
         " on the second port");
   os_tap();
   CHECK(!hf_paused(), "one hotkey set: ... and did not unpause there");
   if (hf_paused())
      os_tap();

   settings->bools.input_hotkey_follows_player1 = saved_follow;
   settings->uints.input_remap_port_map[0][0]   = saved_map;
   input_config_binds[0][RARCH_PAUSE_TOGGLE]    = saved_pause;
   input_config_binds[0][RARCH_ENABLE_HOTKEY]   = saved_en;
   input_autoconf_binds[0][RARCH_ENABLE_HOTKEY] = saved_auto0;
   input_autoconf_binds[1][RARCH_ENABLE_HOTKEY] = saved_auto1;
   input_st->current_driver = saved_input;
   binds_written_by_a_lane();
   run_loop_frames(2);

   if (failures == had)
      printf("[pass] one hotkey set: the first user's hotkeys are the hotkeys,"
            " on whichever port they are read\n");
}

/* Saving the configuration with "only what differs from the defaults".
 * To know the defaults it fills a copy of the settings with them, and
 * asks for each bind's default as it compares. It used to load the
 * defaults over everything in use and put back what it had thought to
 * copy aside - the binds and the profiles - which left the rest reset:
 * the active remap, each port's kind of device, its analog-to-digital
 * setting. Held to it through things the test changes: the changed
 * bind is written and a default one is not, and after the save all of
 * it is what it was, with no bind counted as changed. */
static void lane_save_minimal_binds(void)
{
   settings_t *settings      = config_get_ptr();
   static const char path[]  = "/tmp/frontend_overhead_binds.cfg";
   struct retro_keybind *b   = &input_config_binds[0][RETRO_DEVICE_ID_JOYPAD_B];
   struct retro_keybind saved_b = *b;
   struct retro_keybind before_y = input_config_binds[0][RETRO_DEVICE_ID_JOYPAD_Y];
   struct retro_keybind before_p2 = input_config_binds[1][RETRO_DEVICE_ID_JOYPAD_START];
   bool saved_minimal        = settings->bools.config_save_minimal;
   unsigned saved_device     = settings->uints.input_libretro_device[1];
   unsigned saved_remap      = settings->uints.input_remap_ids[0][RETRO_DEVICE_ID_JOYPAD_A];
   unsigned saved_analog     = settings->uints.input_analog_dpad_mode[0];
   unsigned had              = failures;
   unsigned changes;
   char *text                = NULL;
   long  len                 = 0;
   FILE *f;

   RETRO_KEYBIND_SET_KEY(b, RETROK_F11);
   RETRO_KEYBIND_SET_VALID(b, true);
   /* and what else of the input a user has set that is not a bind: a
    * port's kind of device, a remapped button, a stick as the d-pad */
   input_config_set_device(1, RETRO_DEVICE_ANALOG);
   input_config_set_remap_id(0, RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_X);
   settings->uints.input_analog_dpad_mode[0] = ANALOG_DPAD_NONE;
   binds_written_by_a_lane();
   settings->bools.config_save_minimal = true;
   remove(path);

   changes = input_config_binds_generation();
   CHECK(config_save_file(path), "save minimal: the configuration was not saved");
   /* saving reads the binds; it copied every one aside, loaded the
    * defaults over them and copied them back, which is a change to
    * every one and made everything kept from them be made again */
   CHECK(input_config_binds_generation() == changes,
         "save minimal: saving counted as a change to the binds");

   if ((f = fopen(path, "rb")))
   {
      fseek(f, 0, SEEK_END);
      len = ftell(f);
      fseek(f, 0, SEEK_SET);
      if (len > 0 && (text = (char*)malloc((size_t)len + 1)))
      {
         if (fread(text, 1, (size_t)len, f) != (size_t)len)
            len = 0;
         text[len] = '\0';
      }
      fclose(f);
   }
   CHECK(text != NULL, "save minimal: nothing was written");
   if (text)
   {
      CHECK(strstr(text, "input_player1_b = \"f11\"") != NULL,
            "save minimal: the bind that was changed is not in what was written");
      CHECK(strstr(text, "input_player1_y = ") == NULL,
            "save minimal: a bind that is the default was written");
      free(text);
   }
   /* the binds were put back as they were */
   CHECK(RETRO_KEYBIND_KEY(b) == RETROK_F11,
         "save minimal: the changed bind is not what it was after the save");
   CHECK(!memcmp(&before_y, &input_config_binds[0][RETRO_DEVICE_ID_JOYPAD_Y], sizeof(before_y)),
         "save minimal: the first user's Y is not what it was after the save");
   CHECK(!memcmp(&before_p2, &input_config_binds[1][RETRO_DEVICE_ID_JOYPAD_START], sizeof(before_p2)),
         "save minimal: the second user's Start is not what it was after the save");
   /* ... and so is the rest of what was set: saving is not resetting */
   CHECK(settings->uints.input_libretro_device[1] == RETRO_DEVICE_ANALOG,
         "save minimal: the second port's kind of device was reset by the save");
   CHECK(settings->uints.input_remap_ids[0][RETRO_DEVICE_ID_JOYPAD_A] == RETRO_DEVICE_ID_JOYPAD_X,
         "save minimal: a remapped button was reset by the save");
   CHECK(settings->uints.input_analog_dpad_mode[0] == ANALOG_DPAD_NONE,
         "save minimal: the first port's analog-to-digital setting was reset by the save");

   remove(path);
   settings->bools.config_save_minimal = saved_minimal;
   input_config_set_device(1, saved_device);
   input_config_set_remap_id(0, RETRO_DEVICE_ID_JOYPAD_A, saved_remap);
   settings->uints.input_analog_dpad_mode[0] = saved_analog;
   *b = saved_b;
   binds_written_by_a_lane();
   run_loop_frames(2);

   if (failures == had)
      printf("[pass] save minimal: a changed bind is written, a default one is"
            " not, and the binds, a port's device, a remap and a stick"
            " setting are what they were\n");
}

/* What a bind's button and axis are called is kept as a list of the
 * binds that are called anything, for each user and each port's
 * controller; it was a place for two names for every bind there is.
 * Held to what the tables did: a bind with no name reads as two nulls
 * and never as nothing to look at; a name given replaces the one
 * before and leaves the other alone; every bind of a port can have
 * both; one bind's names can go, and a port's can all go. */
static char *bn_dup(const char *s)
{
   size_t n = strlen(s) + 1;
   char *d  = (char*)malloc(n);
   if (d)
      memcpy(d, s, n);
   return d;
}

static void lane_bind_names(void)
{
   unsigned i;
   unsigned had = failures;
   const unsigned user = 3, port = 5;
   struct input_bind_label got;
   const struct input_bind_label *l;
   char name[32];
   bool all = true;

   /* (starts clean: nothing the harness runs names a bind for these two) */
   input_autoconf_bind_names_free(port);
   for (i = 0; i < RARCH_BIND_LIST_END; i++)
      input_config_bind_names_drop(user, i);

   l = input_config_bind_names(user, RETRO_DEVICE_ID_JOYPAD_B);
   CHECK(l && !l->joykey && !l->joyaxis,
         "bind names: a bind called nothing does not read as two nulls");
   l = input_config_bind_names(MAX_USERS + 3, RARCH_BIND_LIST_END + 3);
   CHECK(l && !l->joykey && !l->joyaxis,
         "bind names: a user and a bind there are not do not read as two nulls");

   got.joykey  = bn_dup("Cross");
   got.joyaxis = NULL;
   input_config_bind_names_take(user, RETRO_DEVICE_ID_JOYPAD_B, &got);
   CHECK(!got.joykey && !got.joyaxis, "bind names: what was handed over was not taken");
   l = input_config_bind_names(user, RETRO_DEVICE_ID_JOYPAD_B);
   CHECK(l->joykey && !strcmp(l->joykey, "Cross") && !l->joyaxis,
         "bind names: a button's name given is not the name read");

   got.joykey  = NULL;
   got.joyaxis = bn_dup("Left X");
   input_config_bind_names_take(user, RETRO_DEVICE_ID_JOYPAD_B, &got);
   l = input_config_bind_names(user, RETRO_DEVICE_ID_JOYPAD_B);
   CHECK(   l->joykey  && !strcmp(l->joykey,  "Cross")
         && l->joyaxis && !strcmp(l->joyaxis, "Left X"),
         "bind names: an axis's name given took the button's away, or did not arrive");

   got.joykey  = bn_dup("Circle");
   got.joyaxis = NULL;
   input_config_bind_names_take(user, RETRO_DEVICE_ID_JOYPAD_B, &got);
   l = input_config_bind_names(user, RETRO_DEVICE_ID_JOYPAD_B);
   CHECK(   l->joykey  && !strcmp(l->joykey,  "Circle")
         && l->joyaxis && !strcmp(l->joyaxis, "Left X"),
         "bind names: a second name for the button did not replace the first");

   /* nothing handed over changes nothing, and makes no place */
   got.joykey = got.joyaxis = NULL;
   input_config_bind_names_take(user, RETRO_DEVICE_ID_JOYPAD_Y, &got);
   l = input_config_bind_names(user, RETRO_DEVICE_ID_JOYPAD_Y);
   CHECK(!l->joykey && !l->joyaxis, "bind names: no names handed over made some");
   /* another user's, and the port's controller's, are their own */
   l = input_config_bind_names(user + 1, RETRO_DEVICE_ID_JOYPAD_B);
   CHECK(!l->joykey && !l->joyaxis, "bind names: one user's name is another's");
   l = input_autoconf_bind_names(user, RETRO_DEVICE_ID_JOYPAD_B);
   CHECK(!l->joykey && !l->joyaxis, "bind names: a user's name is the controller's");

   input_config_bind_names_drop(user, RETRO_DEVICE_ID_JOYPAD_B);
   l = input_config_bind_names(user, RETRO_DEVICE_ID_JOYPAD_B);
   CHECK(!l->joykey && !l->joyaxis, "bind names: a bind's names dropped are still read");

   /* every bind of a port's controller, both names, in an order that
    * is not theirs; then each is what it was given */
   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      unsigned id = (i * 37) % RARCH_BIND_LIST_END;
      snprintf(name, sizeof(name), "btn %u", id);
      got.joykey  = bn_dup(name);
      snprintf(name, sizeof(name), "axis %u", id);
      got.joyaxis = bn_dup(name);
      input_autoconf_bind_names_take(port, id, &got);
   }
   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      char want_b[32], want_a[32];
      snprintf(want_b, sizeof(want_b), "btn %u", i);
      snprintf(want_a, sizeof(want_a), "axis %u", i);
      l = input_autoconf_bind_names(port, i);
      if (   !l->joykey  || strcmp(l->joykey,  want_b)
          || !l->joyaxis || strcmp(l->joyaxis, want_a))
         all = false;
   }
   CHECK(all, "bind names: with every bind of a controller named, one does not read as it was given");
   /* one dropped from the middle leaves the rest */
   got.joykey  = bn_dup("x");
   got.joyaxis = NULL;
   input_config_bind_names_take(user, 7, &got);
   got.joykey  = bn_dup("y");
   input_config_bind_names_take(user, 9, &got);
   got.joykey  = bn_dup("z");
   input_config_bind_names_take(user, 11, &got);
   input_config_bind_names_drop(user, 7);
   CHECK(   !input_config_bind_names(user, 7)->joykey
         && input_config_bind_names(user, 9)->joykey
         && !strcmp(input_config_bind_names(user, 9)->joykey, "y")
         && input_config_bind_names(user, 11)->joykey
         && !strcmp(input_config_bind_names(user, 11)->joykey, "z"),
         "bind names: dropping one bind's names disturbed another's");
   input_config_bind_names_drop(user, 9);
   input_config_bind_names_drop(user, 11);

   input_autoconf_bind_names_free(port);
   all = true;
   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      l = input_autoconf_bind_names(port, i);
      if (l->joykey || l->joyaxis)
         all = false;
   }
   CHECK(all, "bind names: a controller's names freed are still read");

   /* And through the two places names come from: a controller's
    * profile, read for a port, and the configuration's binds. */
   {
      static const char profile[] =
         "input_b_btn = \"3\"\n"
         "input_b_btn_label = \"Cross\"\n"
         "input_l_x_plus_axis = \"+0\"\n"
         "input_l_x_plus_axis_label = \"Left stick right\"\n"
         "input_a_btn = \"4\"\n";
      static const char binds[] =
         "input_player4_y_btn = \"6\"\n"
         "input_player4_y_btn_label = \"Square\"\n";
      struct retro_keybind saved_b = input_autoconf_binds[port][RETRO_DEVICE_ID_JOYPAD_B];
      struct retro_keybind saved_a = input_autoconf_binds[port][RETRO_DEVICE_ID_JOYPAD_A];
      struct retro_keybind saved_x = input_autoconf_binds[port][RARCH_ANALOG_LEFT_X_PLUS];
      struct retro_keybind saved_y = input_config_binds[3][RETRO_DEVICE_ID_JOYPAD_Y];
      char *text           = bn_dup(profile);
      config_file_t *conf  = text ? config_file_new_from_string(text, NULL) : NULL;

      CHECK(conf != NULL, "bind names: no profile to read");
      if (conf)
      {
         input_config_set_autoconfig_binds(port, conf);
         config_file_free(conf);
         l = input_autoconf_bind_names(port, RETRO_DEVICE_ID_JOYPAD_B);
         CHECK(   l->joykey && !strcmp(l->joykey, "Cross")
               && input_autoconf_binds[port][RETRO_DEVICE_ID_JOYPAD_B].joykey == 3,
               "bind names: a profile's name for a button is not read after the profile is");
         l = input_autoconf_bind_names(port, RARCH_ANALOG_LEFT_X_PLUS);
         CHECK(l->joyaxis && !strcmp(l->joyaxis, "Left stick right"),
               "bind names: a profile's name for an axis is not read after the profile is");
         l = input_autoconf_bind_names(port, RETRO_DEVICE_ID_JOYPAD_A);
         CHECK(   !l->joykey && !l->joyaxis
               && input_autoconf_binds[port][RETRO_DEVICE_ID_JOYPAD_A].joykey == 4,
               "bind names: a button the profile does not name has a name");
         input_config_reset_autoconfig_binds(port);
         l = input_autoconf_bind_names(port, RETRO_DEVICE_ID_JOYPAD_B);
         CHECK(!l->joykey && !l->joyaxis,
               "bind names: a controller's names outlive its profile");
      }
      free(text);

      text = bn_dup(binds);
      conf = text ? config_file_new_from_string(text, NULL) : NULL;
      CHECK(conf != NULL, "bind names: no binds to read");
      if (conf)
      {
         config_read_keybinds_conf(conf);
         config_file_free(conf);
         l = input_config_bind_names(3, RETRO_DEVICE_ID_JOYPAD_Y);
         CHECK(   l->joykey && !strcmp(l->joykey, "Square")
               && input_config_binds[3][RETRO_DEVICE_ID_JOYPAD_Y].joykey == 6,
               "bind names: the configuration's name for a user's button is not read");
         input_config_bind_names_drop(3, RETRO_DEVICE_ID_JOYPAD_Y);
      }
      free(text);

      input_autoconf_binds[port][RETRO_DEVICE_ID_JOYPAD_B]  = saved_b;
      input_autoconf_binds[port][RETRO_DEVICE_ID_JOYPAD_A]  = saved_a;
      input_autoconf_binds[port][RARCH_ANALOG_LEFT_X_PLUS]  = saved_x;
      input_config_binds[3][RETRO_DEVICE_ID_JOYPAD_Y]       = saved_y;
      binds_written_by_a_lane();
   }

   if (failures == had)
      printf("[pass] bind names: none reads as two nulls; a name given is the name"
            " read and replaces the one before; every bind of a controller can"
            " have both; one bind's go, and a controller's all go; a profile's"
            " and the configuration's names are read\n");
}

/* A bind's setting in the menu says whose bind it is and which; it
 * used to hold the address the bind was stored at, taken when the
 * menu's settings were made, and so did a capture, from the frame it
 * began to the frame the button came. Held to "the setting is the
 * bind": what it shows is the bind as it is now, clearing it clears
 * that bind and no other user's, and a button captured for it lands
 * in it. */
static void lane_bind_settings(void)
{
#ifdef HAVE_MENU
   char shown[256];
   unsigned had                 = failures;
   input_driver_state_t *input_st = input_state_get_ptr();
   const input_device_driver_t *joypad_real = NULL;
   rarch_setting_t *b1          = menu_setting_find("p1_b");
   rarch_setting_t *b2          = menu_setting_find("p2_b");
   struct retro_keybind *bind1  = &input_config_binds[0][RETRO_DEVICE_ID_JOYPAD_B];
   struct retro_keybind *bind2  = &input_config_binds[1][RETRO_DEVICE_ID_JOYPAD_B];
   struct retro_keybind saved1  = *bind1;
   struct retro_keybind saved2  = *bind2;
   const struct retro_keybind_def *def =
      input_config_bind_def(0, RETRO_DEVICE_ID_JOYPAD_B);

   CHECK(b1 && b2 && b1->type == ST_BIND && b2->type == ST_BIND,
         "bind settings: the first two users' B settings are not found");
   if (!b1 || !b2)
      return;
   CHECK(   b1->index_offset == 0 && b2->index_offset == 1
         && b1->bind_type - MENU_SETTINGS_BIND_BEGIN == RETRO_DEVICE_ID_JOYPAD_B
         && b2->bind_type - MENU_SETTINGS_BIND_BEGIN == RETRO_DEVICE_ID_JOYPAD_B,
         "bind settings: a setting does not say whose bind it is and which");

   /* what it shows is the bind as it is now */
   RETRO_KEYBIND_SET_KEY(bind1, RETROK_F11);
   RETRO_KEYBIND_SET_KEY(bind2, RETROK_F12);
   bind1->joykey = 5;
   bind2->joykey = 6;
   binds_written_by_a_lane();
   shown[0] = '\0';
   if (b1->actions && b1->actions->repr)
      b1->actions->repr(b1, shown, sizeof(shown));
   CHECK(strstr(shown, "f11") && !strstr(shown, "f12"),
         "bind settings: the first user's setting does not show the first user's bind");
   shown[0] = '\0';
   if (b2->actions && b2->actions->repr)
      b2->actions->repr(b2, shown, sizeof(shown));
   CHECK(strstr(shown, "f12") && !strstr(shown, "f11"),
         "bind settings: the second user's setting does not show the second user's bind");

   /* clearing it clears that bind, and no other user's */
   CHECK(b1->actions && b1->actions->start,
         "bind settings: a bind's setting cannot be cleared");
   if (b1->actions && b1->actions->start)
      b1->actions->start(b1);
   CHECK(   bind1->joykey == NO_BTN
         && RETRO_KEYBIND_KEY(bind1) == RETRO_KEYBIND_DEF_KEY(def),
         "bind settings: clearing the first user's B did not clear that bind");
   CHECK(bind2->joykey == 6 && RETRO_KEYBIND_KEY(bind2) == RETROK_F12,
         "bind settings: clearing the first user's B changed the second user's");

   /* a button captured for it lands in it: with the stand-in
    * controller as the first pad, as the lanes that press buttons have */
   joypad_real = input_st->primary_joypad;
   if (joypad_real)
   {
      syn_joypad               = *joypad_real;
      syn_joypad.button        = syn_button;
      syn_joypad.axis          = syn_axis;
      syn_joypad.state         = syn_state;
      syn_joypad.get_buttons   = syn_get_buttons;
      input_st->primary_joypad = &syn_joypad;
   }
   syn_hat = 0;
   memset(syn_axes, 0, sizeof(syn_axes));
   if (!menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_loop_frames(3);
   syn_buttons = 0;
   if (!joypad_real)
      CHECK(false, "bind settings: no joypad driver to capture from");
   else if (menu_input_key_bind_set_mode(MENU_INPUT_BINDS_CTL_BIND_SINGLE, b1))
   {
      unsigned i;
      run_loop_frames(8);
      syn_buttons = (1u << 9);
      for (i = 0; i < 30 && bind1->joykey != 9; i++)
         run_loop_frames(1);
      syn_buttons = 0;
      run_loop_frames(8);
      CHECK(bind1->joykey == 9,
            "bind settings: a button captured for the first user's B did not land in that bind");
      CHECK(bind2->joykey == 6,
            "bind settings: a capture for the first user's B changed the second user's");
   }
   else
      CHECK(false, "bind settings: a capture could not be started");
   run_loop_frames(5);
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   run_loop_frames(3);
   if (joypad_real)
      input_st->primary_joypad = joypad_real;

   *bind1 = saved1;
   *bind2 = saved2;
   binds_written_by_a_lane();
   run_loop_frames(2);

   if (failures == had)
      printf("[pass] bind settings: a bind's setting shows the bind as it is,"
            " clears that bind and no other user's, and a captured button"
            " lands in it\n");
#endif
}

/* A user's binds are a row that is shared until it is written: every
 * user who has configured nothing reads the same row of defaults, and
 * every port with no profile the same row of nothing. Held to it with
 * the two users and ports the harness leaves as they start: they share
 * a row; reading does not part them; a write through the call gives
 * the one written a row of their own that says what the shared one
 * did but for the write, and leaves the other reading what they read;
 * putting the binds back to their defaults, and forgetting a
 * controller, write the defaults and the nothing into the own row
 * and do not hand it back - an address taken of it stays good. */
static void lane_binds_shared_rows(void)
{
   unsigned i;
   unsigned had   = failures;
   const unsigned a = LANE_SHARED_A, b = LANE_SHARED_B;
   const struct retro_keybind *row_a, *row_b, *own;
   struct retro_keybind was;
   bool same = true;
   unsigned changes;

   row_a = input_config_binds[a];
   row_b = input_config_binds[b];
   CHECK(row_a && row_a == row_b,
         "shared rows: two users who have configured nothing do not share a row");
   CHECK(input_autoconf_binds[a] && input_autoconf_binds[a] == input_autoconf_binds[b],
         "shared rows: two ports with no profile do not share a row");
   CHECK(input_config_binds[0] != row_a,
         "shared rows: the first user's row is the others'");

   /* reading parts nobody, and is not a change */
   changes = input_config_binds_generation();
   (void)input_config_bind(a, RETRO_DEVICE_ID_JOYPAD_B);
   (void)input_autoconf_bind(a, RETRO_DEVICE_ID_JOYPAD_B);
   CHECK(   input_config_binds[a] == row_a
         && input_config_binds_generation() == changes,
         "shared rows: reading a bind gave the user a row, or counted as a change");

   /* a write gives the user their own, which says what the shared one
    * did but for the write */
   was = row_a[RETRO_DEVICE_ID_JOYPAD_B];
   input_config_bind_edit(a, RETRO_DEVICE_ID_JOYPAD_B)->joykey = 11;
   own = input_config_binds[a];
   CHECK(own != row_a, "shared rows: a write did not give the user a row of their own");
   CHECK(input_config_binds_generation() != changes,
         "shared rows: a write was not counted as a change");
   CHECK(input_config_binds[b] == row_b,
         "shared rows: a write to one user's bind parted another from the shared row");
   CHECK(   input_config_bind(a, RETRO_DEVICE_ID_JOYPAD_B)->joykey == 11
         && input_config_bind(b, RETRO_DEVICE_ID_JOYPAD_B)->joykey == was.joykey
         && row_a[RETRO_DEVICE_ID_JOYPAD_B].joykey == was.joykey,
         "shared rows: a write to one user's bind is read by another, or was lost");
   for (i = 0; i < RARCH_BIND_LIST_END; i++)
      if (   i != RETRO_DEVICE_ID_JOYPAD_B
          && memcmp(&own[i], &row_a[i], sizeof(own[i])))
         same = false;
   CHECK(same, "shared rows: the user's own row does not start as the shared one said");
   /* a second write is to the same row */
   input_config_bind_edit(a, RETRO_DEVICE_ID_JOYPAD_A)->joykey = 12;
   CHECK(input_config_binds[a] == own,
         "shared rows: a second write gave the user another row");

   /* the same for what a controller's profile gives */
   input_autoconf_bind_edit(a, RETRO_DEVICE_ID_JOYPAD_B)->joykey = 3;
   CHECK(   input_autoconf_binds[a] != input_autoconf_binds[b]
         && input_autoconf_bind(a, RETRO_DEVICE_ID_JOYPAD_B)->joykey == 3
         && input_autoconf_bind(b, RETRO_DEVICE_ID_JOYPAD_B)->joykey != 3,
         "shared rows: a profile's bind for one port is read on another, or was lost");
   own = input_autoconf_binds[a];
   input_config_reset_autoconfig_binds(a);
   CHECK(   input_autoconf_binds[a] == own
         && input_autoconf_bind(a, RETRO_DEVICE_ID_JOYPAD_B)->joykey == NO_BTN
         && !RETRO_KEYBIND_VALID(input_autoconf_bind(a, RETRO_DEVICE_ID_JOYPAD_B)),
         "shared rows: a forgotten controller's row was handed back, or still has its bind");

   /* a user's own row put back to what the shared one says */
   {
      struct retro_keybind *row = input_config_bind_edit(a, 0);
      own = input_config_binds[a];
      memcpy(row, row_a, sizeof(retro_keybind_set));
      CHECK(   input_config_binds[a] == own
            && input_config_bind(a, RETRO_DEVICE_ID_JOYPAD_B)->joykey == was.joykey,
            "shared rows: an own row written back to the defaults is not read as them");
   }
   binds_written_by_a_lane();
   run_loop_frames(2);

   if (failures == had)
      printf("[pass] shared rows: users with nothing configured share a row,"
            " and ports with no profile; a write gives its user their own"
            " and no one else's changes; an own row is kept\n");
}

/* Entries: a hotkey or a command reached by buttons of the RetroPad
 * and a key held together (input_combo_N in the configuration). Held
 * to what they say: only what reads as an entry is taken, and no more
 * than there is room for; a command is given once when all of an
 * entry's sources are down and not again while they stay down; a
 * hotkey an entry reaches does what its own bind would; and what holds
 * a hotkey back holds an entry back - the enabler of its kind of
 * source, and the keyboard being held. */
/* paused now: whether the core runs over the next few frames, with
 * whatever is held kept held */
static bool hf_paused_now(void)
{
   long r0 = pm_runs();
   run_loop_frames(4);
   return pm_runs() == r0;
}

static void en_hold(uint32_t buttons, unsigned frames)
{
   syn_buttons = buttons;
   run_loop_frames(frames);
}

static void lane_entries(void)
{
   enum { START = RETRO_DEVICE_ID_JOYPAD_START, SELECT = RETRO_DEVICE_ID_JOYPAD_SELECT,
          L3 = RETRO_DEVICE_ID_JOYPAD_L3 };
   const uint32_t b_start = 1u << 20, b_select = 1u << 21, b_l3 = 1u << 22,
                  b_enable = 1u << 23;
   input_driver_state_t *input_st           = input_state_get_ptr();
   const input_device_driver_t *joypad_real = input_st->primary_joypad;
   input_driver_t *saved_input              = input_st->current_driver;
   static input_driver_t en_input;
   struct retro_keybind saved[4];
   struct retro_keybind saved_cfg_en;
   unsigned had = failures;
   unsigned i;
   char spec[64];

   if (!joypad_real || !saved_input)
   {
      CHECK(false, "entries: no joypad or input driver");
      return;
   }
   saved[0]     = *input_autoconf_bind(0, START);
   saved[1]     = *input_autoconf_bind(0, SELECT);
   saved[2]     = *input_autoconf_bind(0, L3);
   saved[3]     = *input_autoconf_bind(0, RARCH_ENABLE_HOTKEY);
   saved_cfg_en = *input_config_bind(0, RARCH_ENABLE_HOTKEY);

   /* --- what reads as an entry --- */
   input_entries_clear();
   CHECK(input_entry_add(1, "start+select : pause"), "entries: two buttons and a command is not read");
   CHECK(input_entry_add(2, "key_g:unpause"),         "entries: a key and a command is not read");
   CHECK(input_entry_add(3, " l3 :  pause_toggle "),  "entries: a button and a hotkey, with spaces, is not read");
   CHECK(input_entries_count() == 3, "entries: three read are not three kept");
   CHECK(   !input_entry_add(4, "l3+r3")
         && !input_entry_add(4, "l9 : pause")
         && !input_entry_add(4, "key_nosuchkey : pause")
         && !input_entry_add(4, "key_f1+key_f2 : pause")
         && !input_entry_add(4, " : pause")
         && !input_entry_add(4, "start : no_such_thing")
         && !input_entry_add(4, "start : enable_hotkey")
         && !input_entry_add(4, "start+ : pause")
         && !input_entry_add(4, NULL),
         "entries: something that is not an entry was taken as one");
   CHECK(input_entries_count() == 3, "entries: a refused entry was kept");
   /* ... and a time to be held, which is seconds to a tenth */
   CHECK(   !input_entry_add(4, "start held : pause")
         && !input_entry_add(4, "start held 2 : pause")
         && !input_entry_add(4, "start held 0s : pause")
         && !input_entry_add(4, "start held 26s : pause")
         && !input_entry_add(4, "start held 1.s : pause")
         && !input_entry_add(4, "start held 2s x : pause")
         && !input_entry_add(4, " held 2s : pause"),
         "entries: a time to be held that is not one was taken");
   CHECK(input_entries_count() == 3, "entries: a refused timed entry was kept");

   /* the controller: start, select and L3 on its buttons 20 to 22 */
   syn_joypad               = *joypad_real;
   syn_joypad.button        = syn_button;
   syn_joypad.axis          = syn_axis;
   syn_joypad.state         = syn_state;
   syn_joypad.get_buttons   = syn_get_buttons;
   input_st->primary_joypad = &syn_joypad;
   syn_hat = 0;
   memset(syn_axes, 0, sizeof(syn_axes));
   en_input                 = *saved_input;
   en_input.keys_down       = os_keys_down;
   input_st->current_driver = &en_input;
   input_autoconf_bind_edit(0, START)->joykey  = 20;
   RETRO_KEYBIND_SET_VALID(input_autoconf_bind_edit(0, START), true);
   input_autoconf_bind_edit(0, SELECT)->joykey = 21;
   RETRO_KEYBIND_SET_VALID(input_autoconf_bind_edit(0, SELECT), true);
   input_autoconf_bind_edit(0, L3)->joykey     = 22;
   RETRO_KEYBIND_SET_VALID(input_autoconf_bind_edit(0, L3), true);
   input_autoconf_bind_edit(0, RARCH_ENABLE_HOTKEY)->joykey  = NO_BTN;
   input_autoconf_bind_edit(0, RARCH_ENABLE_HOTKEY)->joyaxis = AXIS_NONE;
   input_config_bind_edit(0, RARCH_ENABLE_HOTKEY)->joykey    = NO_BTN;
   input_config_bind_edit(0, RARCH_ENABLE_HOTKEY)->joyaxis   = AXIS_NONE;
   RETRO_KEYBIND_SET_KEY(input_config_bind_edit(0, RARCH_ENABLE_HOTKEY), RETROK_UNKNOWN);
   os_g_down = false;
   en_hold(0, 4);
   CHECK(!hf_paused(), "entries: paused to begin with");

   /* --- a command, once, when all of its sources are down --- */
   en_hold(b_start, 4);
   CHECK(!hf_paused(), "entries: one button of two gave the command");
   en_hold(b_start | b_select, 4);
   CHECK(hf_paused(), "entries: both buttons held did not give the command");
   en_hold(0, 3);
   os_tap();
   CHECK(!hf_paused(), "entries: the key did not give its command");

   /* --- a hotkey an entry reaches: once for a press, however long --- */
   en_hold(b_l3, 12);
   en_hold(0, 3);
   CHECK(hf_paused(), "entries: a button held for a hotkey did not press it, or pressed it twice");
   en_hold(b_l3, 3);
   en_hold(0, 3);
   CHECK(!hf_paused(), "entries: the hotkey's second press did not undo its first");

   /* --- held back by the enabler of its kind of source --- */
   input_autoconf_bind_edit(0, RARCH_ENABLE_HOTKEY)->joykey = 23;
   RETRO_KEYBIND_SET_VALID(input_autoconf_bind_edit(0, RARCH_ENABLE_HOTKEY), true);
   en_hold(0, 3);
   en_hold(b_start | b_select, 4);
   CHECK(!hf_paused(), "entries: buttons gave their command with the controller's enabler not held");
   en_hold(0, 3);
   en_hold(b_enable, 2);
   en_hold(b_enable | b_start | b_select, 4);
   CHECK(hf_paused(), "entries: buttons with the enabler held did not give their command");
   en_hold(0, 3);
   /* the keyboard has no enabler: its entry is not held back */
   os_tap();
   CHECK(!hf_paused(), "entries: a key's entry was held back by the controller's enabler");
   input_autoconf_bind_edit(0, RARCH_ENABLE_HOTKEY)->joykey = NO_BTN;
   en_hold(0, 3);

   /* --- held back while the keyboard is held --- */
   en_hold(b_start | b_select, 4);
   en_hold(0, 3);
   CHECK(hf_paused(), "entries: not paused for the keyboard check");
   input_keyboard_capture_hold(INPUT_KEYBOARD_CAPTURE_GAME_FOCUS);
   os_tap();
   CHECK(hf_paused(), "entries: a key's entry was read while the keyboard was held");
   input_keyboard_capture_release(INPUT_KEYBOARD_CAPTURE_GAME_FOCUS);
   os_tap();
   CHECK(!hf_paused(), "entries: a key's entry was not read once the keyboard was let go");

   /* --- a time the sources must be held: by the clock --- */
   input_entries_clear();
   CHECK(input_entry_add(1, "start held 0.3s : pause"), "entries: a timed entry is not read");
   CHECK(input_entry_add(2, "key_g : unpause"),         "entries: the key's entry is not read again");
   en_hold(0, 3);
   {
      unsigned n;
      /* held for a tenth of a second, over many frames: not yet */
      for (n = 0; n < 5; n++)
      {
         en_hold(b_start, 2);
         retro_sleep(20);
      }
      CHECK(!hf_paused_now(), "entries: a timed entry was given before its time, however many frames");
      /* ... and on to past its time */
      for (n = 0; n < 15; n++)
      {
         en_hold(b_start, 2);
         retro_sleep(20);
      }
      en_hold(b_start, 2);
      CHECK(hf_paused_now(), "entries: a timed entry was not given when its time was up");
      /* still held: not given again */
      os_tap();
      for (n = 0; n < 5; n++)
      {
         en_hold(b_start, 2);
         retro_sleep(20);
      }
      CHECK(!hf_paused_now(), "entries: a timed entry kept down was given a second time");
      /* let go, and held again for less than its time: nothing */
      en_hold(0, 3);
      for (n = 0; n < 5; n++)
      {
         en_hold(b_start, 2);
         retro_sleep(20);
      }
      en_hold(0, 3);
      CHECK(!hf_paused_now(), "entries: a timed entry let go early was given");
   }

   /* --- no more than there is room for --- */
   input_entries_clear();
   for (i = 1; i <= 16; i++)
   {
      snprintf(spec, sizeof(spec), "start+l3 : pause");
      if (!input_entry_add(i, spec))
         break;
   }
   CHECK(i == 17 && input_entries_count() == 16, "entries: sixteen were not all taken");
   CHECK(!input_entry_add(17, "start : pause") && input_entries_count() == 16,
         "entries: a seventeenth was taken");

   /* --- none: nothing is given --- */
   input_entries_clear();
   en_hold(b_start | b_select, 4);
   en_hold(0, 3);
   CHECK(!hf_paused(), "entries: with none there, buttons still gave a command");

   if (hf_paused())
      command_event(CMD_EVENT_UNPAUSE, NULL);
   syn_buttons = 0;
   *input_autoconf_bind_edit(0, START)               = saved[0];
   *input_autoconf_bind_edit(0, SELECT)              = saved[1];
   *input_autoconf_bind_edit(0, L3)                  = saved[2];
   *input_autoconf_bind_edit(0, RARCH_ENABLE_HOTKEY) = saved[3];
   *input_config_bind_edit(0, RARCH_ENABLE_HOTKEY)   = saved_cfg_en;
   input_st->current_driver = saved_input;
   input_st->primary_joypad = joypad_real;
   run_loop_frames(3);

   if (failures == had)
      printf("[pass] entries: only what reads as one is taken, and sixteen at most;"
            " a command is given once when its sources are all down; a hotkey"
            " reached is pressed once; the enabler and a held keyboard hold"
            " them back\n");
}

/* Macros: buttons of the RetroPad pressed for a user a step at a time
 * (input_macro_N in the configuration), started by an entry. Held to
 * what a macro says, by what the core is given frame by frame, read a
 * button at a time and as a mask: each step's buttons for its number
 * of the core's frames and no more; added to what the user holds, not
 * in place of it; once for a press of the entry that starts it,
 * however long that is held; standing still while the core is paused;
 * for the user it was started for and no other. And only what reads as
 * a macro is taken, of no more steps than one may have, while there is
 * room for its steps. */
static void lane_macros(void)
{
#ifdef HAVE_MENU
   enum { A = RETRO_DEVICE_ID_JOYPAD_A, B = RETRO_DEVICE_ID_JOYPAD_B,
          L3 = RETRO_DEVICE_ID_JOYPAD_L3 };
   static const char *how[2]       = { "button by button", "as a mask" };
   static const unsigned want[9]   = { 1u << A, 1u << A, (1u << A) | (1u << B),
                                       0, 0, 1u << B, 1u << B, 1u << B, 0 };
   const unsigned ab               = (1u << A) | (1u << B);
   input_driver_state_t *input_st  = input_state_get_ptr();
   const input_device_driver_t *joypad_real = input_st->primary_joypad;
   struct retro_keybind saved_a, saved_l3;
   void (*trace)(int, int);
   void (*trace_last)(unsigned*, int*);
   void    *core;
   unsigned had = failures;
   unsigned m, n, i;
   char     why[200];
   char     spec[400];

   if (   !(core = dlopen(core_path_g, RTLD_NOW))
       || !(trace = (void (*)(int, int))dlsym(core, "harness_core_trace"))
       || !(trace_last = (void (*)(unsigned*, int*))dlsym(core, "harness_core_trace_last"))
       || !joypad_real)
   {
      CHECK(false, "macros: the harness core's trace entry points or the joypad driver");
      return;
   }
   saved_a  = *input_autoconf_bind(0, A);
   saved_l3 = *input_autoconf_bind(0, L3);

   /* --- what reads as a macro --- */
   input_macros_clear();
   input_entries_clear();
   CHECK(input_macro_add(1, "a 2, a+b 1, - 2, b 3"), "macros: four steps are not read");
   CHECK(   !input_macro_add(2, "a")
         && !input_macro_add(2, "a x")
         && !input_macro_add(2, "nosuch 2")
         && !input_macro_add(2, "a 0")
         && !input_macro_add(2, "a+- 2")
         && !input_macro_add(2, "a 2,")
         && !input_macro_add(2, ", a 2")
         && !input_macro_add(2, "")
         && !input_macro_add(1, "a 2")
         && !input_macro_add(0, "a 2")
         && !input_macro_add(2, NULL),
         "macros: something that is not a macro, or a second of one number, was taken");
   CHECK(input_macros_count() == 1, "macros: a refused macro was kept");
   /* thirty-two steps to one, and no more */
   spec[0] = '\0';
   for (i = 0; i < 32; i++)
      strlcat(spec, i ? ", a 1" : "a 1", sizeof(spec));
   CHECK(input_macro_add(2, spec), "macros: one of thirty-two steps is not read");
   strlcat(spec, ", a 1", sizeof(spec));
   CHECK(!input_macro_add(3, spec), "macros: one of thirty-three steps was taken");
   /* 256 steps in all: 4 + 32 are used; six more of 32 fit, a seventh does not */
   spec[strlen(spec) - 5] = '\0';
   for (i = 3; i < 9; i++)
      if (!input_macro_add(i, spec))
         break;
   CHECK(i == 9, "macros: six more of thirty-two steps did not all fit");
   CHECK(!input_macro_add(9, spec), "macros: a macro was taken with no room for its steps");
   CHECK(input_macro_add(9, "b 1"), "macros: a short one did not fit in what room was left");
   input_macros_clear();
   CHECK(input_macros_count() == 0 && input_macro_add(1, "a 2, a+b 1, - 2, b 3"),
         "macros: cleared, the first is not read again");

   syn_joypad               = *joypad_real;
   syn_joypad.button        = syn_button;
   syn_joypad.axis          = syn_axis;
   syn_joypad.state         = syn_state;
   syn_joypad.get_buttons   = syn_get_buttons;
   input_st->primary_joypad = &syn_joypad;
   input_driver_set_snapshot_bridge(true);
   syn_hat = 0;
   memset(syn_axes, 0, sizeof(syn_axes));
   fast_forward(true);
   /* the controller's button 5 is A, its 22 is L3 */
   input_autoconf_bind_edit(0, A)->joykey  = 5;
   RETRO_KEYBIND_SET_VALID(input_autoconf_bind_edit(0, A), true);
   input_autoconf_bind_edit(0, L3)->joykey = 22;
   RETRO_KEYBIND_SET_VALID(input_autoconf_bind_edit(0, L3), true);

   for (m = 0; m < 2; m++)
   {
      bool as_said = true;
      unsigned got[9];

      trace((int)m + 1, 0);
      map_frame(0, trace_last);
      CHECK(!(map_frame(0, trace_last) & ab), "macros: something is pressed before any is started");

      /* --- each step's buttons, for its frames and no more --- */
      CHECK(input_macro_start(1, 0), "macros: the macro does not start");
      CHECK(!input_macro_start(7, 0) || true, "macros: (starting)");
      for (n = 0; n < 9; n++)
      {
         got[n] = map_frame(0, trace_last) & ab;
         if (got[n] != want[n])
            as_said = false;
      }
      snprintf(why, sizeof(why), "macros, read %s: the core was given %x %x %x %x %x %x %x %x %x"
            " for \"a 2, a+b 1, - 2, b 3\"", how[m],
            got[0], got[1], got[2], got[3], got[4], got[5], got[6], got[7], got[8]);
      CHECK(as_said, why);
      CHECK(!input_macro_playing(0), "macros: one that has ended is still playing");
      CHECK(!(map_frame(0, trace_last) & ab), "macros: something is still pressed after it ended");

      /* --- added to what the user holds --- */
      input_macro_start(1, 0);
      map_frame(1u << 5, trace_last);
      map_frame(1u << 5, trace_last);
      map_frame(1u << 5, trace_last);
      snprintf(why, sizeof(why), "macros, read %s: with A held by the user, the step with nothing in it let A go", how[m]);
      CHECK((map_frame(1u << 5, trace_last) & ab) == (1u << A), why);
      map_frame(1u << 5, trace_last);
      snprintf(why, sizeof(why), "macros, read %s: with A held by the user, B's step is not A and B", how[m]);
      CHECK((map_frame(1u << 5, trace_last) & ab) == ab, why);
      for (n = 0; n < 4; n++)
         map_frame(0, trace_last);

      /* --- for the user it was started for --- */
      input_macro_start(1, 1);
      snprintf(why, sizeof(why), "macros, read %s: one started for the second user pressed the first's", how[m]);
      CHECK(!(map_frame(0, trace_last) & ab) && input_macro_playing(1) && !input_macro_playing(0), why);
      for (n = 0; n < 9; n++)
         map_frame(0, trace_last);

      /* --- standing still while the core is paused: a macro of forty
       * frames, paused after one for longer than it lasts, is still
       * playing when the core runs again, and still pressing --- */
      input_macros_clear();
      CHECK(input_macro_add(1, "a 2, a+b 1, - 2, b 3") && input_macro_add(2, "a 40"),
            "macros: the two for the pause are not read");
      input_macro_start(2, 0);
      map_frame(0, trace_last);
      fast_forward(false);
      command_event(CMD_EVENT_PAUSE, NULL);
      {
         long r0 = pm_runs();
         run_loop_frames(80);
         snprintf(why, sizeof(why), "macros, read %s: (the core ran %ld frames while paused: the pause did not hold)",
               how[m], pm_runs() - r0);
         CHECK(pm_runs() == r0, why);
      }
      snprintf(why, sizeof(why), "macros, read %s: paused for longer than it lasts, a macro played on to its end", how[m]);
      CHECK(input_macro_playing(0), why);
      command_event(CMD_EVENT_UNPAUSE, NULL);
      fast_forward(true);
      {
         unsigned a_frames = 0;
         unsigned first    = 99;
         for (n = 0; n < 60; n++)
         {
            if (map_frame(0, trace_last) & (1u << A))
            {
               a_frames++;
               if (first == 99)
                  first = n;
            }
         }
         printf("[info] macros, read %s: after the pause the core was given A for %u more"
               " frames, the first of them %u frames in\n", how[m], a_frames, first);
         snprintf(why, sizeof(why), "macros, read %s: after the pause the macro did not go on pressing, or went on past its end", how[m]);
         CHECK(a_frames > 0 && a_frames <= 39 && !input_macro_playing(0), why);
      }

      /* --- not started with "Remap Controls" off, where it would
       * never be played --- */
      {
         settings_t *settings = config_get_ptr();
         bool saved_remap     = settings->bools.input_remap_binds_enable;
         settings->bools.input_remap_binds_enable = false;
         snprintf(why, sizeof(why), "macros, read %s: one was started with Remap Controls off", how[m]);
         CHECK(!input_macro_start(1, 0) && !input_macro_playing(0)
               && !(map_frame(0, trace_last) & ab), why);
         settings->bools.input_remap_binds_enable = saved_remap;
      }

      /* --- once for a press of the entry that starts it --- */
      input_entries_clear();
      CHECK(input_entry_add(1, "l3 : macro_1"), "macros: an entry that starts one is not read");
      {
         unsigned a_frames = 0, b_frames = 0;
         for (n = 0; n < 30; n++)
         {
            unsigned seen = map_frame(1u << 22, trace_last) & ab;
            if (seen & (1u << A))
               a_frames++;
            if (seen & (1u << B))
               b_frames++;
         }
         snprintf(why, sizeof(why), "macros, read %s: L3 held for thirty frames gave A for %u frames"
               " and B for %u, where the macro once is 3 and 4", how[m], a_frames, b_frames);
         CHECK(a_frames == 3 && b_frames == 4, why);
      }
      map_frame(0, trace_last);
      map_frame(0, trace_last);
      input_entries_clear();
   }
   CHECK(   !input_entry_add(1, "l3 : macro_")
         && !input_entry_add(1, "l3 : macro_0")
         && !input_entry_add(1, "l3 : macro_1@0")
         && !input_entry_add(1, "l3 : macro_1@17")
         && !input_entry_add(1, "l3 : macro_1x")
         && input_entry_add(1, "l3 : macro_1@16"),
         "macros: an entry for a macro that does not read as one was taken, or one that does was not");

   input_entries_clear();
   input_macros_clear();
   *input_autoconf_bind_edit(0, A)  = saved_a;
   *input_autoconf_bind_edit(0, L3) = saved_l3;
   syn_buttons              = 0;
   trace(0, 0);
   fast_forward(false);
   input_driver_set_snapshot_bridge(false);
   input_st->primary_joypad = joypad_real;
   run_loop_frames(3);

   if (failures == had)
      printf("[pass] macros: each step's buttons for its frames and no more, read a"
            " button at a time and as a mask; added to what is held; for its user"
            " alone; still while paused; once for a press of its entry; and only"
            " what reads as one is taken, while there is room\n");
#endif
}

/* A macro under what changes how frames and input go: a frame stepped
 * while paused, run-ahead, a replay recorded and played back, and
 * netplay. In each the core must be given the macro as it is written -
 * the same presses for the same numbers of its frames, in order -
 * whenever it starts. What the core is given is taken frame by frame
 * and put as runs: "A3 AB2 -2 B4" for "a 3, a+b 2, - 2, b 4". */
#define MM_SAID "A3 AB2 -2 B4"

static void mm_runs(const unsigned *seen, unsigned n, char *out, size_t len)
{
   enum { A = RETRO_DEVICE_ID_JOYPAD_A, B = RETRO_DEVICE_ID_JOYPAD_B };
   unsigned first = 0, last = n, i, run = 0;
   size_t   at    = 0;
   out[0] = '\0';
   while (first < n && !(seen[first] & ((1u << A) | (1u << B))))
      first++;
   while (last > first && !(seen[last - 1] & ((1u << A) | (1u << B))))
      last--;
   for (i = first; i < last; i++)
   {
      unsigned v = seen[i] & ((1u << A) | (1u << B));
      run++;
      if (i + 1 == last || (seen[i + 1] & ((1u << A) | (1u << B))) != v)
      {
         at += (size_t)snprintf(out + at, at < len ? len - at : 0, "%s%s%u",
               at ? " " : "",
               v == ((1u << A) | (1u << B)) ? "AB" : v == (1u << A) ? "A" : v ? "B" : "-",
               run);
         run = 0;
      }
   }
}

static void lane_macro_modes(void)
{
#ifdef HAVE_MENU
   enum { FRAMES = 40 };
   input_driver_state_t *input_st  = input_state_get_ptr();
   settings_t *settings            = config_get_ptr();
   const input_device_driver_t *joypad_real = input_st->primary_joypad;
   input_driver_t *saved_input     = input_st->current_driver;
   static input_driver_t mm_input;
   void (*trace)(int, int);
   void (*trace_last)(unsigned*, int*);
   void    *core;
   unsigned had = failures;
   unsigned seen[FRAMES + 40];
   unsigned f;
   int      axes[4];
   char     got[160], why[320];
   /* which of its parts are run: all of them, unless MM_PARTS says
    * (1 stepped frames, 2 run-ahead, 4 the replay, 8 netplay) - for
    * finding which one a later lane's failure follows */
   unsigned mm_parts = getenv("MM_PARTS") ? (unsigned)atoi(getenv("MM_PARTS")) : 15;

   if (   !(core = dlopen(core_path_g, RTLD_NOW))
       || !(trace = (void (*)(int, int))dlsym(core, "harness_core_trace"))
       || !(trace_last = (void (*)(unsigned*, int*))dlsym(core, "harness_core_trace_last"))
       || !joypad_real || !saved_input)
   {
      CHECK(false, "macro modes: the harness core's trace entry points or the drivers");
      return;
   }
   input_macros_clear();
   input_entries_clear();
   CHECK(input_macro_add(1, "a 3, a+b 2, - 2, b 4"), "macro modes: the macro is not read");
   syn_joypad               = *joypad_real;
   syn_joypad.button        = syn_button;
   syn_joypad.axis          = syn_axis;
   syn_joypad.state         = syn_state;
   syn_joypad.get_buttons   = syn_get_buttons;
   input_st->primary_joypad = &syn_joypad;
   input_driver_set_snapshot_bridge(true);
   syn_hat = 0;
   syn_buttons = 0;
   memset(syn_axes, 0, sizeof(syn_axes));
   trace(1, 0);
   run_loop_frames(5);

#define MM_PLAY(n) do { \
      for (f = 0; f < (n); f++) \
      { \
         runloop_iterate(); \
         task_queue_check(); \
         trace_last(&seen[f], axes); \
      } \
   } while (0)

   /* --- as it is, to begin with --- */
   input_macro_start(1, 0);
   MM_PLAY(FRAMES);
   mm_runs(seen, FRAMES, got, sizeof(got));
   snprintf(why, sizeof(why), "macro modes: with nothing else going on the core was given \"%s\", not \"" MM_SAID "\"", got);
   CHECK(!strcmp(got, MM_SAID), why);

   /* --- a frame stepped while paused is a frame --- */
   if (mm_parts & 1)
   {
      unsigned n = 0;
      unsigned tries = 0;
      long     r0;
      mm_input                 = *saved_input;
      mm_input.keys_down       = os_keys_down;
      input_st->current_driver = &mm_input;
      CHECK(input_entry_add(1, "key_g : frame_advance"), "macro modes: an entry for frame advance is not read");
      os_g_down = false;
      input_macro_start(1, 0);
      MM_PLAY(1);
      n = 1;
      command_event(CMD_EVENT_PAUSE, NULL);
      run_loop_frames(20);
      r0 = pm_runs();
      /* eleven frames are left of it: stepped one at a time, and then some */
      while (n < 20 && pm_runs() - r0 < 16 && tries++ < 60)
      {
         long before = pm_runs();
         os_g_down = true;
         runloop_iterate();
         task_queue_check();
         os_g_down = false;
         run_loop_frames(3);
         if (pm_runs() != before)
            trace_last(&seen[n++], axes);
      }
      mm_runs(seen, n, got, sizeof(got));
      snprintf(why, sizeof(why), "macro modes: stepped a frame at a time while paused (%ld frames), the core was given \"%s\", not \"" MM_SAID "\"",
            pm_runs() - r0, got);
      CHECK(!strcmp(got, MM_SAID), why);
      CHECK(!input_macro_playing(0), "macro modes: stepped to its end, a macro is still playing");
      /* Out of the pause by the pause hotkey, as someone stepping
       * frames gets out of it: frame advance puts the run loop in a
       * pause of its own kind that only that hotkey ends, and left in
       * it the lanes after this one are not given their key events. */
      input_entries_clear();
      CHECK(input_entry_add(1, "key_g : pause_toggle"), "macro modes: an entry for the pause hotkey is not read");
      os_g_down = true;
      run_loop_frames(2);
      os_g_down = false;
      run_loop_frames(4);
      if (runloop_get_flags() & RUNLOOP_FLAG_PAUSED)
      {
         CHECK(false, "macro modes: the pause hotkey did not end the pause after stepping");
         command_event(CMD_EVENT_UNPAUSE, NULL);
      }
      input_entries_clear();
      input_st->current_driver = saved_input;
      run_loop_frames(5);
   }

   /* --- run-ahead: a frame shown is a frame, however many are run behind it --- */
   if (mm_parts & 2)
   {
      bool     saved_on     = settings->bools.run_ahead_enabled;
      bool     saved_second = settings->bools.run_ahead_secondary_instance;
      bool     saved_hide   = settings->bools.run_ahead_hide_warnings;
      unsigned saved_frames = settings->uints.run_ahead_frames;
      long     r0, ran;
      runahead_clear_variables(runloop_state_get_ptr());
      settings->bools.run_ahead_enabled            = true;
      settings->bools.run_ahead_secondary_instance = false;
      settings->bools.run_ahead_hide_warnings      = true;
      settings->uints.run_ahead_frames             = 2;
      run_loop_frames(10);
      r0 = pm_runs();
      input_macro_start(1, 0);
      MM_PLAY(FRAMES);
      ran = pm_runs() - r0;
      mm_runs(seen, FRAMES, got, sizeof(got));
      snprintf(why, sizeof(why), "macro modes: under run-ahead of two frames (the core ran %ld times for %d shown) the core was given \"%s\", not \"" MM_SAID "\"",
            ran, (int)FRAMES, got);
      CHECK(!strcmp(got, MM_SAID), why);
      printf("[info] macro modes, run-ahead: the core ran %ld times for %d frames shown\n", ran, (int)FRAMES);
      CHECK(ran > FRAMES, "macro modes: run-ahead was not running the core more than once a frame");
      settings->bools.run_ahead_enabled            = saved_on;
      settings->bools.run_ahead_secondary_instance = saved_second;
      settings->bools.run_ahead_hide_warnings      = saved_hide;
      settings->uints.run_ahead_frames             = saved_frames;
      run_loop_frames(10);
   }

#ifdef HAVE_BSV_MOVIE
   /* --- a replay: recorded with the macro playing, played back with
    * nothing started, the core is given the macro again --- */
   if (mm_parts & 4)
   {
      char path[512];
      snprintf(path, sizeof(path), "%s/macro_modes.replay", harness_dir_g);
      CHECK(movie_start_record(input_st, path), "macro modes: recording did not start");
      run_loop_frames(10);
      CHECK(BSV_MOVIE_IS_RECORDING(), "macro modes: not recording");
      input_macro_start(1, 0);
      MM_PLAY(FRAMES);
      mm_runs(seen, FRAMES, got, sizeof(got));
      snprintf(why, sizeof(why), "macro modes: while a replay was recorded the core was given \"%s\", not \"" MM_SAID "\"", got);
      CHECK(!strcmp(got, MM_SAID), why);
      run_loop_frames(5);
      movie_stop(input_st);
      run_loop_frames(10);

      /* played for fewer frames than were recorded (ten, the forty and
       * five): a replay that runs out ends itself, and leaves the core
       * as this lane must not leave it for the next */
      CHECK(movie_start_playback(input_st, path), "macro modes: playback did not start");
      MM_PLAY(FRAMES + 8);
      movie_stop(input_st);
      run_loop_frames(10);
      mm_runs(seen, FRAMES + 8, got, sizeof(got));
      snprintf(why, sizeof(why), "macro modes: the replay played back gave the core \"%s\", not \"" MM_SAID "\"", got);
      CHECK(!strcmp(got, MM_SAID), why);
      CHECK(!input_macro_playing(0), "macro modes: playing a replay back started a macro");
   }
#endif

#ifdef HAVE_NETWORKING
   /* --- netplay: a macro is not started. (Hosting with nobody
    * connected, one that was started was never given to the core.) --- */
   if (mm_parts & 8)
   {
   netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_SERVER, NULL);
   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
   {
      CHECK(!input_macro_start(1, 0) && !input_macro_playing(0),
            "macro modes: a macro was started with netplay on");
   }
   else
      printf("[skip] macro modes, netplay: could not be turned on here\n");
   netplay_driver_ctl(RARCH_NETPLAY_CTL_DISABLE, NULL);
   CHECK(input_macro_start(1, 0), "macro modes: with netplay off again a macro does not start");
   MM_PLAY(FRAMES);
   }
#endif
#undef MM_PLAY

   input_entries_clear();
   input_macros_clear();
   syn_buttons              = 0;
   trace(0, 0);
   input_driver_set_snapshot_bridge(false);
   input_st->primary_joypad = joypad_real;
   input_st->current_driver = saved_input;
   run_loop_frames(5);

   if (failures == had)
      printf("[pass] macro modes: the core is given a macro as it is written when"
            " frames are stepped while paused, under run-ahead, while a replay is"
            " recorded and when it is played back; with netplay on one is not"
            " started\n");
#endif
}

/* Entries and macros changed while the program runs, as a menu will
 * change them, and saved. Held to: one is written out as it reads back
 * - the same entry, the same macro; setting a number there is one of
 * replaces that one, and its room is handed back; one taken out leaves
 * the others as they were; and a save writes the ones there are, and
 * takes out the line of one there is no longer, so that a load of what
 * was saved gives back what there was. */
static bool ek_is(unsigned number, const char *want)
{
   char got[512];
   got[0] = '\0';
   input_entry_spec(number, got, sizeof(got));
   if (strcmp(got, want))
      fprintf(stderr, "       entry %u is \"%s\", not \"%s\"\n", number, got, want);
   return !strcmp(got, want);
}

static bool mk_is(unsigned number, const char *want)
{
   char got[512];
   got[0] = '\0';
   input_macro_spec(number, got, sizeof(got));
   if (strcmp(got, want))
      fprintf(stderr, "       macro %u is \"%s\", not \"%s\"\n", number, got, want);
   return !strcmp(got, want);
}

static void lane_entries_kept(void)
{
   static const char path[] = "/tmp/frontend_overhead_entries.cfg";
   settings_t *settings     = config_get_ptr();
   bool saved_minimal       = settings->bools.config_save_minimal;
   unsigned had             = failures;
   unsigned i;
   char spec[512];
   char *text               = NULL;
   long  len                = 0;
   FILE *f;
   config_file_t *conf;

   input_entries_clear();
   input_macros_clear();

   /* --- written out as it reads back --- */
   CHECK(   input_entry_add(2, "  r3 + l3:menu_toggle ")
         && input_entry_add(7, "key_f9 : undo_load_state")
         && input_entry_add(3, "select+start+key_g held 2s : exit_emulator")
         && input_entry_add(9, "a held 0.5s : macro_4@2")
         && input_entry_add(1, "r : macro_1"),
         "entries kept: five entries are not read");
   CHECK(   ek_is(2, "l3+r3 : menu_toggle")
         && ek_is(7, "key_f9 : undo_load_state")
         && ek_is(3, "select+start+key_g held 2s : exit_emulator")
         && ek_is(9, "a held 0.5s : macro_4@2")
         && ek_is(1, "r : macro_1")
         && !input_entry_spec(4, spec, sizeof(spec)),
         "entries kept: an entry is not written out as what it is");
   CHECK(   input_macro_set(1, " down 2 ,down+right 2, - 1,a 4")
         && input_macro_set(4, "b 30")
         && input_macro_set(6, "l+r 1, - 1, l+r 1"),
         "entries kept: three macros are not read");
   CHECK(   mk_is(1, "down 2, down+right 2, - 1, a 4")
         && mk_is(4, "b 30")
         && mk_is(6, "l+r 1, - 1, l+r 1")
         && !input_macro_spec(2, spec, sizeof(spec)),
         "entries kept: a macro is not written out as what it is");
   /* each read back from what it was written as is the same again */
   for (i = 1; i <= 9; i++)
   {
      char again[512];
      if (!input_entry_spec(i, spec, sizeof(spec)))
         continue;
      CHECK(input_entry_add(i, spec) && input_entry_spec(i, again, sizeof(again))
            && !strcmp(spec, again),
            "entries kept: an entry read back from how it was written is not the same");
   }

   /* --- a number there is one of: that one, replaced --- */
   CHECK(input_entry_add(7, "x : pause") && ek_is(7, "x : pause")
         && input_entries_count() == 5,
         "entries kept: setting a number there is an entry of did not replace it");
   CHECK(!input_entry_add(7, "nothing that reads") && ek_is(7, "x : pause"),
         "entries kept: an entry that does not read replaced one that did");
   CHECK(input_macro_set(4, "b 1, a 1") && mk_is(4, "b 1, a 1")
         && mk_is(1, "down 2, down+right 2, - 1, a 4")
         && mk_is(6, "l+r 1, - 1, l+r 1")
         && input_macros_count() == 3,
         "entries kept: a macro set again was not replaced, or disturbed another");
   CHECK(!input_macro_set(4, "not a macro") && mk_is(4, "b 1, a 1"),
         "entries kept: a macro that does not read replaced one that did");

   /* --- one taken out leaves the others --- */
   CHECK(   input_entry_remove(3) && !input_entry_remove(3)
         && input_entries_count() == 4
         && ek_is(2, "l3+r3 : menu_toggle") && ek_is(9, "a held 0.5s : macro_4@2")
         && ek_is(1, "r : macro_1") && ek_is(7, "x : pause"),
         "entries kept: taking an entry out disturbed another, or did not take it");
   CHECK(   input_macro_remove(1) && !input_macro_remove(1)
         && input_macros_count() == 2
         && mk_is(4, "b 1, a 1") && mk_is(6, "l+r 1, - 1, l+r 1"),
         "entries kept: taking a macro out disturbed another, or did not take it");
   /* the room a macro had is room again: the pool filled to the brim */
   {
      unsigned n = 0;
      spec[0] = '\0';
      for (i = 0; i < 32; i++)
         strlcat(spec, i ? ", a 1" : "a 1", sizeof(spec));
      for (i = 10; i < 18; i++)
         if (input_macro_set(i, spec))
            n++;
      /* 256 steps, 5 used: seven of thirty-two fit, an eighth does not */
      CHECK(n == 7, "entries kept: the steps of a macro taken out were not room again");
      for (i = 10; i < 18; i++)
         input_macro_remove(i);
      CHECK(input_macros_count() == 2 && mk_is(4, "b 1, a 1") && mk_is(6, "l+r 1, - 1, l+r 1"),
            "entries kept: filling and emptying the pool disturbed the macros that stayed");
   }

   /* --- saved: what there is, and not the line of what there is not --- */
   if ((f = fopen(path, "wb")))
   {
      fputs("input_combo_5 = \"start : pause\"\n"
            "input_macro_9 = \"a 1\"\n", f);
      fclose(f);
   }
   settings->bools.config_save_minimal = true;
   CHECK(config_save_file(path), "entries kept: the configuration was not saved");
   if ((f = fopen(path, "rb")))
   {
      fseek(f, 0, SEEK_END);
      len = ftell(f);
      fseek(f, 0, SEEK_SET);
      if (len > 0 && (text = (char*)malloc((size_t)len + 1)))
      {
         if (fread(text, 1, (size_t)len, f) != (size_t)len)
            len = 0;
         text[len] = '\0';
      }
      fclose(f);
   }
   CHECK(text != NULL, "entries kept: nothing was saved");
   if (text)
   {
      CHECK(   strstr(text, "input_combo_2 = \"l3+r3 : menu_toggle\"")
            && strstr(text, "input_combo_7 = \"x : pause\"")
            && strstr(text, "input_combo_9 = \"a held 0.5s : macro_4@2\"")
            && strstr(text, "input_macro_4 = \"b 1, a 1\"")
            && strstr(text, "input_macro_6 = \"l+r 1, - 1, l+r 1\""),
            "entries kept: an entry or a macro there is was not saved");
      CHECK(   !strstr(text, "input_combo_5") && !strstr(text, "input_combo_3")
            && !strstr(text, "input_macro_9") && !strstr(text, "input_macro_1 "),
            "entries kept: the line of one there is not was left in what was saved");
      free(text);
   }

   /* --- and what was saved, loaded, is what there was --- */
   input_entries_clear();
   input_macros_clear();
   if ((conf = config_file_new(path)))
   {
      input_entries_read(conf);
      config_file_free(conf);
   }
   CHECK(conf != NULL, "entries kept: what was saved cannot be read");
   CHECK(   input_entries_count() == 4 && input_macros_count() == 2
         && ek_is(2, "l3+r3 : menu_toggle") && ek_is(7, "x : pause")
         && ek_is(9, "a held 0.5s : macro_4@2") && ek_is(1, "r : macro_1")
         && mk_is(4, "b 1, a 1") && mk_is(6, "l+r 1, - 1, l+r 1"),
         "entries kept: what was saved, loaded, is not what there was");

   /* --- a line with a slip in it is left as it was written, until
    * that number is set or taken out --- */
   if ((f = fopen(path, "wb")))
   {
      fputs("input_combo_8 = \"start : puase\"\n"
            "input_combo_2 = \"l3+r3 : menu_toggle\"\n"
            "input_macro_3 = \"a two\"\n", f);
      fclose(f);
   }
   if ((conf = config_file_new(path)))
   {
      input_entries_read(conf);
      config_file_free(conf);
   }
   CHECK(input_entries_count() == 1 && input_macros_count() == 0,
         "entries kept: a line with a slip in it was read");
   CHECK(config_save_file(path), "entries kept: the configuration was not saved a second time");
   text = NULL;
   if ((f = fopen(path, "rb")))
   {
      fseek(f, 0, SEEK_END);
      len = ftell(f);
      fseek(f, 0, SEEK_SET);
      if (len > 0 && (text = (char*)malloc((size_t)len + 1)))
      {
         if (fread(text, 1, (size_t)len, f) != (size_t)len)
            len = 0;
         text[len] = '\0';
      }
      fclose(f);
   }
   if (text)
   {
      CHECK(   strstr(text, "input_combo_8 = \"start : puase\"")
            && strstr(text, "input_macro_3 = \"a two\"")
            && strstr(text, "input_combo_2 = \"l3+r3 : menu_toggle\""),
            "entries kept: a save took out a line that did not read, with its slip");
      free(text);
   }
   CHECK(input_entry_remove(8) && input_macro_remove(3)
         && config_save_file(path),
         "entries kept: a line that did not read cannot be taken out");
   text = NULL;
   if ((f = fopen(path, "rb")))
   {
      fseek(f, 0, SEEK_END);
      len = ftell(f);
      fseek(f, 0, SEEK_SET);
      if (len > 0 && (text = (char*)malloc((size_t)len + 1)))
      {
         if (fread(text, 1, (size_t)len, f) != (size_t)len)
            len = 0;
         text[len] = '\0';
      }
      fclose(f);
   }
   if (text)
   {
      CHECK(!strstr(text, "input_combo_8") && !strstr(text, "input_macro_3"),
            "entries kept: a line that did not read, taken out, is still saved");
      free(text);
   }

   remove(path);
   settings->bools.config_save_minimal = saved_minimal;
   input_entries_clear();
   input_macros_clear();

   if (failures == had)
      printf("[pass] entries kept: one is written out as it reads back; a number"
            " set again is replaced; one taken out leaves the rest and its room;"
            " a save writes what there is and takes out what there is not, and"
            " loads back the same\n");
}

static void lane_aim_stick(void)
{
#if defined(HAVE_TEST_DRIVERS) && !defined(_WIN32)
   input_driver_state_t *input_st = input_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   const input_device_driver_t *joypad_real = input_st->primary_joypad;
   struct retro_keybind saved_auto[8];
   unsigned had = failures;
   unsigned i;
   int gx, gy, off, px, py, lx;
#define AIM_READ() do { \
      run_loop_frames(1); \
      gx  = input_driver_state_wrapper(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X); \
      gy  = input_driver_state_wrapper(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y); \
      off = input_driver_state_wrapper(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN); \
      px  = input_driver_state_wrapper(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X); \
      py  = input_driver_state_wrapper(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y); \
      lx  = input_driver_state_wrapper(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X); \
   } while (0)

   if (!joypad_real)
   {
      CHECK(false, "aim stick: no joypad driver");
      return;
   }
   syn_joypad               = *joypad_real;
   syn_joypad.button        = syn_button;
   syn_joypad.axis          = syn_axis;
   syn_joypad.state         = syn_state;
   syn_joypad.get_buttons   = syn_get_buttons;
   input_st->primary_joypad = &syn_joypad;
   syn_hat     = 0;
   syn_buttons = 0;
   memset(syn_axes, 0, sizeof(syn_axes));
   /* the sticks on axes 0-3 */
   memcpy(saved_auto, &input_autoconf_binds[0][RARCH_ANALOG_LEFT_X_PLUS],
         sizeof(saved_auto));
   for (i = 0; i < 4; i++)
   {
      input_autoconf_binds[0][RARCH_ANALOG_LEFT_X_PLUS + 2 * i].joyaxis     = AXIS_POS(i);
      input_autoconf_binds[0][RARCH_ANALOG_LEFT_X_PLUS + 2 * i + 1].joyaxis = AXIS_NEG(i);
   }
   fast_forward(true);
   /* the left stick up and left, the right stick right and down */
   syn_axes[0] = -30000;
   syn_axes[1] = -12000;
   syn_axes[2] = 10000;
   syn_axes[3] = 20000;

   /* off, as before: the stick is not the gun */
   settings->uints.input_aim_stick[0] = INPUT_AIM_STICK_NONE;
   AIM_READ();
   CHECK(gx != 10000 && px != 10000 && lx == 10000,
         "aim stick: off, the stick shows up as the lightgun or the pointer");

   /* the right stick aims */
   settings->uints.input_aim_stick[0] = INPUT_AIM_STICK_RIGHT;
   AIM_READ();
   if (gx != 10000 || gy != 20000 || off || px != 10000 || py != 20000)
      printf("[info] aim stick: gun %d %d offscreen %d, pointer %d %d\n",
            gx, gy, off, px, py);
   CHECK(gx == 10000 && gy == 20000 && !off,
         "aim stick: the lightgun is not where the right stick is held");
   CHECK(px == 10000 && py == 20000,
         "aim stick: the pointer is not where the right stick is held");
   CHECK(lx == 10000, "aim stick: the stick itself is no longer read as a stick");

   /* the left stick aims */
   settings->uints.input_aim_stick[0] = INPUT_AIM_STICK_LEFT;
   AIM_READ();
   CHECK(gx == -30000 && gy == -12000 && px == -30000 && py == -12000,
         "aim stick: the lightgun or pointer is not where the left stick is held");

   /* another port's setting is its own */
   settings->uints.input_aim_stick[0] = INPUT_AIM_STICK_NONE;
   settings->uints.input_aim_stick[1] = INPUT_AIM_STICK_RIGHT;
   AIM_READ();
   CHECK(gx != 10000, "aim stick: the second port's setting aimed the first port's gun");
   settings->uints.input_aim_stick[1] = INPUT_AIM_STICK_NONE;

   memset(syn_axes, 0, sizeof(syn_axes));
   fast_forward(false);
   run_loop_frames(3);
   memcpy(&input_autoconf_binds[0][RARCH_ANALOG_LEFT_X_PLUS], saved_auto,
         sizeof(saved_auto));
   input_st->primary_joypad = joypad_real;
#undef AIM_READ
   if (failures == had)
      printf("[pass] aim stick: off, the stick is only a stick; on, the"
            " chosen stick is where the port's lightgun and pointer"
            " point, and is still read as a stick\n");
#else
   printf("[skip] aim stick: needs the test drivers\n");
#endif
}

/* The sensor snapshot is made for a shader that reads sensors, and
 * for nobody else: with no such shader the poll publishes nothing. */
static void lane_sensor_snapshot_idle(void)
{
   input_driver_state_t *input_st = input_state_get_ptr();
   unsigned had                   = failures;
   float gyro[3], accel[3], rest[3];
   int seq0, seq1, seq2, seq3, seq4;

   input_driver_set_shader_uses_sensors(false);
   run_loop_frames(3);
   seq0 = retro_atomic_load_acquire_int(&input_st->sensor_snap_seq);
   run_loop_frames(20);
   seq1 = retro_atomic_load_acquire_int(&input_st->sensor_snap_seq);
   CHECK(seq1 == seq0,
         "sensor snapshot: with no shader reading sensors the poll still publishes one");

   input_driver_set_shader_uses_sensors(true);
   run_loop_frames(10);
   seq2 = retro_atomic_load_acquire_int(&input_st->sensor_snap_seq);
   CHECK(seq2 >= seq1 + 2 * 10,
         "sensor snapshot: with a shader reading sensors it is not published every poll");

   input_driver_set_shader_uses_sensors(false);
   run_loop_frames(3);
   seq3 = retro_atomic_load_acquire_int(&input_st->sensor_snap_seq);
   run_loop_frames(20);
   seq4 = retro_atomic_load_acquire_int(&input_st->sensor_snap_seq);
   CHECK(seq3 == seq2 + 2 && seq4 == seq3,
         "sensor snapshot: after the shader went, noughts were not published once and then nothing");
   gyro[0] = accel[0] = 1.0f;
   input_driver_read_sensor_snapshot(gyro, accel, rest);
   CHECK(   gyro[0]  == 0.0f && gyro[1]  == 0.0f && gyro[2]  == 0.0f
         && accel[0] == 0.0f && accel[1] == 0.0f && accel[2] == 0.0f,
         "sensor snapshot: what is left published is not noughts");

   if (failures == had)
      printf("[pass] sensor snapshot: published every poll while a shader"
            " reads sensors, once with noughts when it stops, and not at"
            " all otherwise\n");
}

/* A driver restart under a core that renders with the GPU (the report
 * was a core option that changes the internal resolution). The core's
 * request is put back after the drivers are torn down, and the
 * frontend has to still know it has one. */
static unsigned hw_lane_resets, hw_lane_destroys;
static void hw_lane_context_reset(void)   { hw_lane_resets++; }
static void hw_lane_context_destroy(void) { hw_lane_destroys++; }

static void lane_hw_context_restart(void)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   unsigned had                   = failures;
   bool before, after;

   /* what RETRO_ENVIRONMENT_SET_HW_RENDER leaves behind */
   memset(&video_st->hw_render, 0, sizeof(video_st->hw_render));
   video_st->hw_render.context_type    = RETRO_HW_CONTEXT_OPENGL;
   video_st->hw_render.context_reset   = hw_lane_context_reset;
   video_st->hw_render.context_destroy = hw_lane_context_destroy;
   retro_atomic_store_release_int(&video_st->hw_context_type,
         (int)RETRO_HW_CONTEXT_OPENGL);
   hw_lane_resets = hw_lane_destroys   = 0;

   before = video_driver_is_hw_context();
   command_event(CMD_EVENT_REINIT, NULL);
   run_loop_frames(2);
   after  = video_driver_is_hw_context();

   CHECK(before, "hw restart: the core's request was not in place before the restart");
   CHECK(video_st->hw_render.context_type == RETRO_HW_CONTEXT_OPENGL
         && video_st->hw_render.context_reset == hw_lane_context_reset,
         "hw restart: the core's request did not come back after the restart");
   CHECK(after,
         "hw restart: after a driver restart the frontend no longer knows the core renders with the GPU");
   CHECK(hw_lane_destroys == 1,
         "hw restart: the core's context was not destroyed exactly once");

   /* as the lanes after this found it */
   memset(&video_st->hw_render, 0, sizeof(video_st->hw_render));
   retro_atomic_store_release_int(&video_st->hw_context_type,
         (int)RETRO_HW_CONTEXT_NONE);
   command_event(CMD_EVENT_REINIT, NULL);
   run_loop_frames(2);

   if (failures == had)
      printf("[pass] hw restart: across a driver restart a GPU-rendering"
            " core's request comes back, its context is destroyed once"
            " (reset %u time(s)), and the frontend still knows it has one\n",
            hw_lane_resets);
}

/* A remap file with a disabled button, content closed from the menu,
 * the core started again from the menu, and closed again
 * (issue #19698). */
#if !defined(_WIN32) && defined(HAVE_MENU)
static bool remap_has(const char *path, const char *want)
{
   char line[256];
   bool found = false;
   FILE *f    = fopen(path, "r");
   if (!f)
      return false;
   while (fgets(line, sizeof(line), f))
      if (strstr(line, want))
         found = true;
   fclose(f);
   return found;
}
#endif

static void lane_remap_close(void)
{
#if !defined(_WIN32) && defined(HAVE_MENU)
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   settings_t *settings        = config_get_ptr();
   content_ctx_info_t info     = {0};
   const char *core_name       = runloop_st->system.info.library_name;
   char dir[256], path[768], cmd[900];
   unsigned had = failures;
   unsigned i;

   if (!core_name || !*core_name)
   {
      CHECK(false, "remap close: the core has no name");
      return;
   }
   snprintf(dir, sizeof(dir), "/tmp/harness_remaps_%d", (int)getpid());
   snprintf(path, sizeof(path), "%s/%s/%s.rmp", dir, core_name, core_name);
   strlcpy(settings->paths.directory_input_remapping, dir,
         sizeof(settings->paths.directory_input_remapping));

   CHECK(runloop_st->system.input_desc_btn[0][RETRO_DEVICE_ID_JOYPAD_L] != NULL,
         "remap close: the core's buttons are not described");

   /* L is disabled and B is given to A, and the remap is saved as a
    * core remap, as the menu saves it */
   input_config_set_remap_id(0, RETRO_DEVICE_ID_JOYPAD_L, RARCH_UNMAPPED);
   input_config_set_remap_id(0, RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_A);
   CHECK(input_remapping_save_file(path), "remap close: the remap file was not saved");
   if (runloop_st->name.remapfile)
      free(runloop_st->name.remapfile);
   runloop_st->name.remapfile  = strdup(path);
   runloop_st->flags          |= RUNLOOP_FLAG_REMAPS_CORE_ACTIVE;
   CHECK(remap_has(path, "input_player1_btn_l = \"-1\""),
         "remap close: the disabled button is not in the file as saved");

   /* content closed from the menu */
   command_event(CMD_EVENT_CLOSE_CONTENT, NULL);
   for (i = 0; i < 40; i++)
      run_loop_frames(1);
   CHECK(remap_has(path, "input_player1_btn_l = \"-1\""),
         "remap close: closing content dropped the disabled button from the remap file");

   /* the core started again from the menu: the remap file is loaded */
   task_push_load_new_core(core_path_g, NULL, &info, CORE_TYPE_PLAIN, NULL, NULL);
   task_push_start_current_core(&info);
   for (i = 0; i < 60; i++)
      run_loop_frames(1);
   CHECK(   (content_get_flags() & CONTENT_ST_FLAG_IS_INITED)
         && runloop_st->name.remapfile
         && settings->uints.input_remap_ids[0][RETRO_DEVICE_ID_JOYPAD_L] == RARCH_UNMAPPED
         && runloop_st->system.input_desc_btn[0][RETRO_DEVICE_ID_JOYPAD_L],
         "remap close: started again from the menu, the core is not up with its remap and its descriptions");

   /* and closed again, this time with a configuration override in
    * use. Unloading the override loads the remap file once more, so
    * the remap is saved a second time when the core itself closes -
    * a frame later, as a staged load does it. */
   {
      char cfg[600];
      FILE *f;
      snprintf(cfg, sizeof(cfg), "%s/harness.cfg", harness_dir_g);
      if ((f = fopen(cfg, "ab")))
      {
         fprintf(f, "input_remapping_directory = \"%s\"\n", dir);
         fclose(f);
      }
   }
   runloop_st->flags |= RUNLOOP_FLAG_OVERRIDES_ACTIVE;
   command_event(CMD_EVENT_CLOSE_CONTENT, NULL);
   for (i = 0; i < 40; i++)
      run_loop_frames(1);
   CHECK(remap_has(path, "input_player1_btn_l = \"-1\""),
         "remap close: closing content with an override in use dropped the disabled button from the remap file");
   CHECK(remap_has(path, "input_player1_btn_b = \"8\""),
         "remap close: closing content dropped a remapped button from the remap file");
   /* the old core's descriptions do go, once it has closed */
   CHECK(!runloop_st->system.input_desc_btn[0][RETRO_DEVICE_ID_JOYPAD_L],
         "remap close: the closed core's button descriptions were kept");
   snprintf(cmd, sizeof(cmd), "rm -rf '%s'", dir);
   if (system(cmd)) { }
   if (failures == had)
      printf("[pass] remap close: a disabled button stays in the remap file"
            " through Close Content, with and without a configuration"
            " override in use\n");
#else
   printf("[skip] remap close: needs the menu\n");
#endif
}

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

/* Two senders at once, each with its own key: per key, every event
 * arrives and in the order sent. And a release that does not fit. */
static unsigned keylane2_got[2], keylane2_bad[2], keylane_f15_down, keylane_f15_up;

static void RETRO_CALLCONV keylane2_core_cb(bool down, unsigned keycode,
      uint32_t character, uint16_t mods)
{
   unsigned k = (keycode == RETROK_F13) ? 0 : (keycode == RETROK_F12) ? 1 : 2;
   (void)mods;
   if (keycode == RETROK_F15)
   {
      if (down)
         keylane_f15_down++;
      else
         keylane_f15_up++;
      return;
   }
   if (k > 1)
      return;
   if (     character != keylane2_got[k]
         || down != ((keylane2_got[k] & 1) == 0))
      keylane2_bad[k]++;
   keylane2_got[k]++;
}

static void keylane2_sender(void *data)
{
   unsigned key = *(unsigned*)data;
   unsigned i;
   for (i = 0; i < KEYLANE_EVENTS; i++)
   {
      input_keyboard_event((i & 1) == 0, key, i, 0, RETRO_DEVICE_KEYBOARD);
      if ((i & 7) == 7)
         retro_sleep(1);
   }
}

/* F15 down, sixty-three more to fill the queue, then F15 up */
static void keylane_fill_sender(void *data)
{
   unsigned i;
   (void)data;
   input_keyboard_event(true, RETROK_F15, 0, 0, RETRO_DEVICE_KEYBOARD);
   for (i = 0; i < 63; i++)
      input_keyboard_event((i & 1) == 0, RETROK_F14, i, 0,
            RETRO_DEVICE_KEYBOARD);
   input_keyboard_event(false, RETROK_F15, 0, 0, RETRO_DEVICE_KEYBOARD);
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

   /* 4. A release that does not fit is not lost: the key does not
    *    stay held. */
   runloop_st->key_event = keylane2_core_cb;
   keylane_f15_down = keylane_f15_up = 0;
   thr = sthread_create(keylane_fill_sender, NULL);
   if (thr)
   {
      sthread_join(thr);
      run_frames(2);
      CHECK(keylane_f15_down == 1 && keylane_f15_up == 1,
            "key events: a release that did not fit in the queue was lost, and the key stayed held");
   }

   /* 5. Two senders at once, with no lock between them: per key,
    *    every event arrives, in order, on the core's thread. */
   {
      unsigned key0 = RETROK_F13, key1 = RETROK_F12;
      sthread_t *t0, *t1;
      keylane2_got[0] = keylane2_got[1] = keylane2_bad[0] = keylane2_bad[1] = 0;
      dropped0 = input_driver_key_events_dropped();
      t0 = sthread_create(keylane2_sender, &key0);
      t1 = sthread_create(keylane2_sender, &key1);
      if (t0 && t1)
      {
         for (i = 0; i < 800 && (keylane2_got[0] < KEYLANE_EVENTS || keylane2_got[1] < KEYLANE_EVENTS); i++)
         {
            run_frames(1);
            retro_sleep(1);
         }
         sthread_join(t0);
         sthread_join(t1);
         run_frames(2);
         if (keylane2_got[0] != KEYLANE_EVENTS || keylane2_got[1] != KEYLANE_EVENTS || keylane2_bad[0] || keylane2_bad[1])
            fprintf(stderr, "       F13 %u/%u (%u out of order), F12 %u/%u (%u out of order), %u dropped\n",
                  keylane2_got[0], KEYLANE_EVENTS, keylane2_bad[0],
                  keylane2_got[1], KEYLANE_EVENTS, keylane2_bad[1],
                  input_driver_key_events_dropped() - dropped0);
         CHECK(keylane2_got[0] == KEYLANE_EVENTS && keylane2_got[1] == KEYLANE_EVENTS,
               "key events: from two threads at once, not all reached the core");
         CHECK(!keylane2_bad[0] && !keylane2_bad[1],
               "key events: from two threads at once, one thread's reached the core out of order");
      }
      else
         CHECK(false, "key events: no two threads to report from");
   }

   runloop_st->key_event = saved;
   fast_forward(false);
   run_frames(2);
   if (failures == had)
      printf("[pass] key events: from the frontend's thread acted on at once;"
            " from another, the core gets all of them in order on its own thread"
            " at the next poll; a full queue drops presses, counts them, and"
            " keeps releases; two threads at once, no lock, each in order\n");
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
      lane_socd();
   lane_remap_idle();
      lane_input_rotation();
      lane_triggers();
      lane_remap_pressure();
      lane_network_retropad();
      lane_pointer_capture();
      lane_menu_combo_gate();
      lane_menu_pause();
      lane_gun_buttons_kept_mice();
      lane_hotkey_facts();
      lane_ai_presses();
      lane_menu_repeat_rates();
      lane_pointer_store();
      lane_binds_change_count();
      lane_mapping_changes();
      lane_quit_combo();
      lane_restart_hold();
      lane_sticks_read_once();
      lane_stick_sources();
      lane_mask_and_buttons();
      lane_keys_current();
      lane_keyboard_holders();
      lane_hotkeys_one_set();
      lane_save_minimal_binds();
      lane_bind_names();
      lane_binds_shared_rows();
      lane_entries();
      lane_macros();
      lane_macro_modes();
      lane_entries_kept();
      lane_bind_settings();
      lane_aim_stick();
      lane_core_view();
      lane_key_events();
      /* last: these restart the drivers */
      lane_input_kept();
      lane_joypad_reinit();
   }

   lane_sensor_snapshot_idle();
   lane_hw_context_restart();

   /* last: it closes the content the other lanes run on */
   lane_remap_close();

   if (failures)
   {
      fprintf(stderr, "%u lane(s) failed\n", failures);
      return 1;
   }
   printf("all lanes passed\n");
   return 0;
}

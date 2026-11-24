/* audio/drivers/audioworklet.c on its threaded (PROXY_TO_PTHREAD) build.
 *
 * write() returns the bytes it took - the frontend's threaded pipeline
 * advances its pending pointer by that much, and the inline path writes
 * the rest from there - and a write that gives frames up says so once,
 * as a total when the driver is freed: a line from the write is a line
 * a frame, on the audio path.
 *
 * The worklet pulls with no lock, wait or spin, straight from the ring
 * into the planar output, across the ring's wrap.  The driver's init
 * waits on the worklet's report and nothing else, a writer out of room
 * parks until the next pull, and a ring taken down while a pull is
 * under way is freed after it, never under it.
 *
 * The driver is this translation unit, built against the tree's
 * emscripten stand-ins.  The stand-ins below play the browser: a thread
 * that delivers the init callbacks, and a thread that runs the worklet's
 * process callback, as the browser's render thread does. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <rthreads/rthreads.h>
#include <retro_timers.h>

/* Short enough that a missed wake shows as a slow test, not a hung one.
 * The park's watchdog is long enough that only a missed wake runs it
 * out, not a loaded machine scheduling the worklet's thread late. */
#define AUDIOWORKLET_INIT_WAIT_MS 2000
#define AUDIOWORKLET_PARK_WAIT_NS 100000000
#define BURST_GAP_MS              150

/* The context's JavaScript answers: a 48 kHz context, running. */
#include <emscripten.h>
#undef  EM_ASM_INT
#define EM_ASM_INT(...) 48000

#include "../../../audio/drivers/audioworklet.c"

static unsigned failures = 0;
static retro_atomic_int_t log_lines;

#define CHECK(cond, msg, a, b) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: %s (%ld, %ld)\n", __FILE__, __LINE__, \
               msg, (long)(a), (long)(b)); \
         failures++; \
      } \
   } while (0)

/* The frontend's logging, counted: the write path is to say nothing. */
void RARCH_LOG(const char *fmt, ...)  { (void)fmt; retro_atomic_fetch_add_seq_cst_int(&log_lines, 1); }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; retro_atomic_fetch_add_seq_cst_int(&log_lines, 1); }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; retro_atomic_fetch_add_seq_cst_int(&log_lines, 1); }

/* ---- wasm atomics: memory.atomic.wait32 / notify ------------------- */

static slock_t *wait_lock;
static scond_t *wait_cond;
static unsigned notify_gen;
static retro_atomic_int_t wait_timeouts;

uint32_t emscripten_atomic_add_u32(void *addr, uint32_t val)
{ return __atomic_fetch_add((uint32_t*)addr, val, __ATOMIC_SEQ_CST); }
uint32_t emscripten_atomic_load_u32(const void *addr)
{ return __atomic_load_n((const uint32_t*)addr, __ATOMIC_SEQ_CST); }
uint32_t emscripten_atomic_store_u32(void *addr, uint32_t val)
{ __atomic_store_n((uint32_t*)addr, val, __ATOMIC_SEQ_CST); return val; }

/* Returns on a notify, as the instruction does, or when the time is up;
 * the time running out is counted. */
ATOMICS_WAIT_RESULT_T emscripten_atomic_wait_u32(void *addr,
      uint32_t expected, int64_t ns)
{
   ATOMICS_WAIT_RESULT_T ret = ATOMICS_WAIT_OK;
   unsigned gen;
   slock_lock(wait_lock);
   if (emscripten_atomic_load_u32(addr) != expected)
      ret = ATOMICS_WAIT_NOT_EQUAL;
   else
   {
      gen = notify_gen;
      while (gen == notify_gen)
         if (!scond_wait_timeout(wait_cond, wait_lock, ns / 1000))
         {
            if (gen == notify_gen)
            {
               ret = ATOMICS_WAIT_TIMED_OUT;
               retro_atomic_fetch_add_seq_cst_int(&wait_timeouts, 1);
            }
            break;
         }
   }
   slock_unlock(wait_lock);
   return ret;
}

int emscripten_atomic_notify(void *addr, int64_t count)
{
   (void)addr;
   (void)count;
   slock_lock(wait_lock);
   notify_gen++;
   scond_broadcast(wait_cond);
   slock_unlock(wait_lock);
   return 0;
}

/* ---- the browser --------------------------------------------------- */

enum { INIT_REPORTS_DONE, INIT_REPORTS_ERROR };

static int init_report_mode;
static EmscriptenStartWebAudioWorkletCallback init_cb;
static void *init_user;
static sthread_t *init_thread;

/* The worklet's render thread: one process call a quantum.  Free
 * running, one pull each time a writer parks, or in bursts of four
 * with a gap longer than the writer's watchdog between them, the way a
 * browser runs its quanta. */
enum { WORKLET_FREE_RUN, WORKLET_ON_PARK, WORKLET_BURSTS };

static EmscriptenWorkletNodeProcessCallback worklet_cb;
static void *worklet_user;
static sthread_t *worklet_thread;
static retro_atomic_int_t worklet_stop;
static retro_atomic_int_t worklet_pulls;
static int worklet_mode;

#define QUANTUM 128

static void worklet_pull(void)
{
   float out[QUANTUM * 2];
   AudioSampleFrame frame = { 2, QUANTUM, out };
   worklet_cb(0, NULL, 1, &frame, 0, NULL, worklet_user);
   retro_atomic_fetch_add_seq_cst_int(&worklet_pulls, 1);
}

static void worklet_run(void *data)
{
   audioworklet_data_t *aw = (audioworklet_data_t*)worklet_user;
   (void)data;
   while (!retro_atomic_load_seq_cst_int(&worklet_stop))
   {
      if (worklet_mode == WORKLET_ON_PARK)
      {
         if (!emscripten_atomic_load_u32(&aw->waiting))
            continue;
         /* Late enough that the writer is in its wait, not before it. */
         retro_sleep(1);
      }
      else if (worklet_mode == WORKLET_BURSTS)
      {
         worklet_pull();
         worklet_pull();
         worklet_pull();
         retro_sleep(BURST_GAP_MS);
      }
      worklet_pull();
   }
}

static void init_deliver(void *data)
{
   (void)data;
   /* The worklet module loads a while after the call. */
   retro_sleep(20);
   init_cb(1, init_report_mode == INIT_REPORTS_DONE, init_user);
}

EMSCRIPTEN_WEBAUDIO_T emscripten_create_audio_context(const EmscriptenWebAudioCreateAttributes *o) { (void)o; return 1; }
int emscripten_audio_context_quantum_size(EMSCRIPTEN_WEBAUDIO_T c) { (void)c; return QUANTUM; }
void emscripten_audio_node_connect(EMSCRIPTEN_WEBAUDIO_T s, EMSCRIPTEN_WEBAUDIO_T d, int o, int i) { (void)s; (void)d; (void)o; (void)i; }
void platform_emscripten_run_on_browser_thread_sync(void (*func)(void*), void *arg) { func(arg); }

void emscripten_start_wasm_audio_worklet_thread_async(EMSCRIPTEN_WEBAUDIO_T c,
      void *s, uint32_t n, EmscriptenStartWebAudioWorkletCallback cb, void *u)
{
   (void)c; (void)s; (void)n;
   init_cb     = cb;
   init_user   = u;
   init_thread = sthread_create(init_deliver, NULL);
}

/* The processor registers on the same browser thread. */
void emscripten_create_wasm_audio_worklet_processor_async(EMSCRIPTEN_WEBAUDIO_T c,
      const WebAudioWorkletProcessorCreateOptions *o,
      EmscriptenWorkletProcessorCreatedCallback cb, void *u)
{
   (void)o;
   cb(c, true, u);
}

EMSCRIPTEN_WEBAUDIO_T emscripten_create_wasm_audio_worklet_node(EMSCRIPTEN_WEBAUDIO_T c,
      const char *n, const EmscriptenAudioWorkletNodeCreateOptions *o,
      EmscriptenWorkletNodeProcessCallback cb, void *u)
{
   (void)c; (void)n; (void)o;
   worklet_cb   = cb;
   worklet_user = u;
   return 2;
}

static void worklet_start(int mode)
{
   worklet_mode = mode;
   retro_atomic_int_init(&worklet_stop, 0);
   worklet_thread = sthread_create(worklet_run, NULL);
}

static void worklet_end(void)
{
   retro_atomic_store_release_int(&worklet_stop, 1);
   sthread_join(worklet_thread);
   worklet_thread = NULL;
}

static void init_join(void)
{
   sthread_join(init_thread);
   init_thread = NULL;
}

/* ---- the tests ----------------------------------------------------- */

#define RING_BYTES 4096u
#define FRAME      (2 * sizeof(float))

static float samples[16384];

/* A driver of a known ring size and a running context, no worklet. */
static void setup(audioworklet_data_t *aw, bool nonblock)
{
   memset(aw, 0, sizeof(*aw));
   aw->ring            = (audioworklet_ring_t*)calloc(1, sizeof(*aw->ring));
   retro_spsc_init(&aw->ring->q, RING_BYTES);
   aw->ring_size       = RING_BYTES;
   aw->driver_running  = true;
   aw->context_running = true;
   aw->nonblock        = nonblock;
   retro_atomic_size_init(&aw->dropped, 0);
   retro_atomic_size_init(&aw->underruns, 0);
}

static void teardown(audioworklet_data_t *aw)
{
   audioworklet_ring_free(aw->ring);
   aw->ring = NULL;
}

static void test_write(void)
{
   audioworklet_data_t aw;
   ssize_t w;
   size_t  queued;

   /* Non-blocking, room enough: every byte taken, and said in bytes. */
   setup(&aw, true);
   w = audioworklet_write(&aw, samples, 1000 * FRAME / 8);   /* 125 frames */
   queued = retro_spsc_read_avail(&aw.ring->q);
   CHECK(w == (ssize_t)(125 * FRAME), "the write returns the bytes it took", w, 125 * FRAME);
   CHECK((size_t)w == queued, "what it says it took is what the ring holds", w, queued);

   /* More than the room: the room, in bytes, and nothing else. */
   w = audioworklet_write(&aw, samples, sizeof(samples));
   queued = retro_spsc_read_avail(&aw.ring->q);
   CHECK(queued == RING_BYTES, "the ring is filled to its size", queued, RING_BYTES);
   CHECK((size_t)w == RING_BYTES - 125 * FRAME, "a short write returns the bytes it took",
         w, RING_BYTES - 125 * FRAME);
   CHECK(audioworklet_write_avail(&aw) == 0, "no room after the ring is full",
         audioworklet_write_avail(&aw), 0);
   teardown(&aw);

   /* Blocking, and nothing draining the ring: the write gives up after
    * its bound, returns what went, and counts the rest - no line. */
   setup(&aw, false);
   retro_atomic_int_init(&log_lines, 0);
   w = audioworklet_write(&aw, samples, sizeof(samples));
   CHECK(w == (ssize_t)RING_BYTES, "a blocking write that cannot finish returns the bytes it took",
         w, RING_BYTES);
   CHECK(retro_atomic_load_acquire_size(&aw.dropped) == (sizeof(samples) - RING_BYTES) / FRAME,
         "the frames given up are counted",
         retro_atomic_load_acquire_size(&aw.dropped), (sizeof(samples) - RING_BYTES) / FRAME);
   CHECK(retro_atomic_load_seq_cst_int(&log_lines) == 0, "the write path logs nothing",
         retro_atomic_load_seq_cst_int(&log_lines), 0);
   teardown(&aw);

   /* A span of the ring: whole frames, published by write_end, and
    * nothing lent once the ring is full. */
   {
      void  *span = NULL;
      size_t got;
      setup(&aw, true);
      got = audioworklet_write_begin(&aw, 125 * FRAME + 3, &span);
      CHECK(got == 125 * FRAME && span != NULL, "a span of whole frames",
            got, 125 * FRAME);
      CHECK(retro_spsc_read_avail(&aw.ring->q) == 0, "nothing published yet",
            retro_spsc_read_avail(&aw.ring->q), 0);
      memset(span, 0, got);
      CHECK(audioworklet_write_end(&aw, got) == (ssize_t)got,
            "write_end publishes it", got, got);
      CHECK(retro_spsc_read_avail(&aw.ring->q) == got, "the ring holds it",
            retro_spsc_read_avail(&aw.ring->q), got);
      audioworklet_write(&aw, samples, sizeof(samples));
      CHECK(audioworklet_write_begin(&aw, FRAME, &span) == 0 && !span,
            "a full ring lends nothing", 0, 0);
      teardown(&aw);
   }
}

/* The pull, called directly: left and right out of the interleaved ring,
 * across its wrap, then silence and an underrun once it runs dry. */
static void test_pull(void)
{
   audioworklet_data_t aw;
   float in[QUANTUM * 2];
   float out[QUANTUM * 2];
   AudioSampleFrame frame = { 2, QUANTUM, out };
   unsigned i, bad = 0;

   setup(&aw, true);
   audioworklet_publish(&aw);

   /* Put the ring's tail 40 frames before its end, so the next quantum
    * comes back as two spans. */
   audioworklet_write(&aw, samples, RING_BYTES - 40 * FRAME);
   retro_spsc_skip(&aw.ring->q, RING_BYTES - 40 * FRAME);
   CHECK(RING_BYTES - (aw.ring->q.tail & aw.ring->q.mask) == 40 * FRAME,
         "the tail sits 40 frames before the wrap",
         RING_BYTES - (aw.ring->q.tail & aw.ring->q.mask), 40 * FRAME);

   for (i = 0; i < QUANTUM; i++)
   {
      in[i * 2]     =  (float)i;
      in[i * 2 + 1] = -(float)i;
   }
   audioworklet_write(&aw, in, sizeof(in));
   retro_atomic_size_init(&aw.underruns, 0);
   audioworklet_process_cb(0, NULL, 1, &frame, 0, NULL, &aw);
   for (i = 0; i < QUANTUM; i++)
      if (out[i] != (float)i || out[QUANTUM + i] != -(float)i)
         bad++;
   CHECK(bad == 0, "every frame lands in its channel, across the wrap", bad, 0);
   CHECK(retro_atomic_load_acquire_size(&aw.underruns) == 0,
         "a full quantum is no underrun",
         retro_atomic_load_acquire_size(&aw.underruns), 0);

   /* Half a quantum: the rest is silence, and counted. */
   audioworklet_write(&aw, in, sizeof(in) / 2);
   memset(out, 0x7f, sizeof(out));
   audioworklet_process_cb(0, NULL, 1, &frame, 0, NULL, &aw);
   bad = 0;
   for (i = QUANTUM / 2; i < QUANTUM; i++)
      if (out[i] != 0.0f || out[QUANTUM + i] != 0.0f)
         bad++;
   CHECK(bad == 0, "the short part of a quantum is silence", bad, 0);
   CHECK(retro_atomic_load_acquire_size(&aw.underruns) == 1,
         "a short quantum is an underrun",
         retro_atomic_load_acquire_size(&aw.underruns), 1);

   /* The driver down: silence, and no underrun. */
   audioworklet_retire(&aw);
   memset(out, 0x7f, sizeof(out));
   audioworklet_process_cb(0, NULL, 1, &frame, 0, NULL, &aw);
   CHECK(out[0] == 0.0f && out[QUANTUM] == 0.0f, "a driver that is down plays silence",
         0, 0);
   CHECK(retro_atomic_load_acquire_size(&aw.underruns) == 1,
         "a driver that is down is not starved",
         retro_atomic_load_acquire_size(&aw.underruns), 1);
   CHECK(aw.ring == NULL && aw.retired == NULL,
         "a ring with no pull under it is freed at once", 0, 0);
}

/* A worklet that fails to come up: init says so as soon as it does,
 * not after its watchdog, and leaves nothing behind. */
static void test_init_error(void)
{
   unsigned rate = 0;
   void *aw;
   retro_atomic_int_init(&wait_timeouts, 0);
   init_report_mode = INIT_REPORTS_ERROR;
   aw = audioworklet_init(NULL, 48000, 64, &rate);
   init_join();
   CHECK(aw == NULL, "a worklet that failed fails the init", 0, 0);
   CHECK(retro_atomic_load_seq_cst_int(&wait_timeouts) == 0,
         "the failure wakes the init; its watchdog does not",
         retro_atomic_load_seq_cst_int(&wait_timeouts), 0);
   CHECK(audioworklet_static_data == NULL, "nothing is kept for a reuse", 0, 0);
}

/* The worklet comes up: init returns when it reports, woken by it. Then
 * the driver goes down and up again against a worklet pulling as fast
 * as it can, and no pull ever reads a ring that was freed - ASan's
 * part - and every ring is freed in the end - LSan's. */
static void test_init_and_lifetime(void)
{
   unsigned rate = 0, cycle;
   audioworklet_data_t *aw;
   retro_atomic_int_init(&wait_timeouts, 0);
   init_report_mode = INIT_REPORTS_DONE;
   aw = (audioworklet_data_t*)audioworklet_init(NULL, 48000, 64, &rate);
   init_join();
   CHECK(aw != NULL, "a worklet that came up brings the driver up", 0, 0);
   CHECK(retro_atomic_load_seq_cst_int(&wait_timeouts) == 0,
         "the report wakes the init; its watchdog does not",
         retro_atomic_load_seq_cst_int(&wait_timeouts), 0);
   if (!aw)
      return;
   aw->context_running = true;
   audioworklet_set_nonblock_state(aw, true);

   retro_atomic_int_init(&worklet_pulls, 0);
   worklet_start(WORKLET_FREE_RUN);
   /* Until the worklet has pulled through a good share of them too:
    * on a loaded machine its thread can start late. */
   for (cycle = 0;
           cycle < 3000
        || (retro_atomic_load_seq_cst_int(&worklet_pulls) < 2000 && cycle < 3000000);
        cycle++)
   {
      audioworklet_write(aw, samples, sizeof(samples));
      audioworklet_free(aw);
      if (audioworklet_init(NULL, 48000, 16 + (cycle & 63), &rate) != aw)
      {
         CHECK(0, "the driver comes back up on its context", cycle, 0);
         break;
      }
      aw->context_running = true;
   }
   worklet_end();
   CHECK(retro_atomic_load_seq_cst_int(&worklet_pulls) >= 2000,
         "the worklet pulled throughout", retro_atomic_load_seq_cst_int(&worklet_pulls), 2000);

   audioworklet_free(aw);
   CHECK(aw->ring == NULL && aw->retired == NULL,
         "with no pull under way, every ring is freed",
         aw->ring != NULL, aw->retired != NULL);
}

/* A blocking writer out of room parks, and the worklet's pull wakes it:
 * no wait runs out its watchdog, and nothing is dropped. */
static void test_park(void)
{
   audioworklet_data_t *aw = audioworklet_static_data;
   unsigned rate = 0;
   ssize_t  w;
   if (!aw)
      return;
   aw = (audioworklet_data_t*)audioworklet_init(NULL, 48000, 16, &rate);
   if (!aw)
   {
      CHECK(0, "the driver comes back up for the writer", 0, 0);
      return;
   }
   aw->context_running = true;
   audioworklet_set_nonblock_state(aw, false);
   retro_atomic_size_init(&aw->dropped, 0);
   retro_atomic_int_init(&wait_timeouts, 0);

   worklet_start(WORKLET_ON_PARK);
   /* 16 ms of ring and five quanta more: room for the rest comes only
    * from the pulls the parks wait for. */
   w = audioworklet_write(aw, samples, aw->ring_size + 5 * QUANTUM * FRAME);
   worklet_end();

   CHECK(w == (ssize_t)(aw->ring_size + 5 * QUANTUM * FRAME),
         "the blocking write takes it all",
         w, aw->ring_size + 5 * QUANTUM * FRAME);
   CHECK(retro_atomic_load_acquire_size(&aw->dropped) == 0, "nothing dropped",
         retro_atomic_load_acquire_size(&aw->dropped), 0);
   CHECK(retro_atomic_load_seq_cst_int(&wait_timeouts) == 0,
         "each park is ended by the pull, not its watchdog",
         retro_atomic_load_seq_cst_int(&wait_timeouts), 0);
   audioworklet_free(aw);
}

/* Pulls that come in bursts, with gaps the writer's watchdog runs out
 * in: a gap is not the context stopping, and the write is paced through
 * it, not cut short. */
static void test_park_bursts(void)
{
   audioworklet_data_t *aw;
   unsigned rate = 0;
   ssize_t  w;
   size_t   want;
   if (!audioworklet_static_data)
      return;
   aw = (audioworklet_data_t*)audioworklet_init(NULL, 48000, 16, &rate);
   if (!aw)
   {
      CHECK(0, "the driver comes back up for the writer", 0, 0);
      return;
   }
   aw->context_running = true;
   audioworklet_set_nonblock_state(aw, false);
   retro_atomic_size_init(&aw->dropped, 0);

   worklet_start(WORKLET_BURSTS);
   want = aw->ring_size + 40 * QUANTUM * FRAME;
   w    = audioworklet_write(aw, samples, want);
   worklet_end();

   CHECK(w == (ssize_t)want, "the write is paced through the gaps", w, want);
   CHECK(retro_atomic_load_acquire_size(&aw->dropped) == 0,
         "nothing dropped between bursts",
         retro_atomic_load_acquire_size(&aw->dropped), 0);
   audioworklet_free(aw);
}

int main(void)
{
   wait_lock = slock_new();
   wait_cond = scond_new();

   test_write();
   test_pull();
   test_init_error();
   test_init_and_lifetime();
   test_park();
   test_park_bursts();

   if (audioworklet_static_data)
   {
      audioworklet_retired_drain(audioworklet_static_data);
      free(audioworklet_static_data->worklet_stack);
      free(audioworklet_static_data);
   }
   scond_free(wait_cond);
   slock_free(wait_lock);

   if (failures)
   {
      printf("[fail] audioworklet_test: %u check(s) failed\n", failures);
      return 1;
   }
   printf("[pass] audioworklet_test\n");
   return 0;
}

/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (save_state_io_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Regression oracle for tasks/task_save.c's file I/O.
 *
 * The unit under test is the shipping tasks/task_save.c translation
 * unit, compiled from the tree, driven through the real task queue
 * against a stub frontend.  The core, the settings and the clock are
 * the test's; everything on the I/O path - task_save_handler,
 * task_load_handler, the rastate writer and the rastate block walker,
 * intfstream, the VFS - is production code.
 *
 * Why these properties
 * --------------------
 * The save and load handlers used to transfer exactly one
 * SAVE_STATE_CHUNK per queue tick.  That made throughput a function
 * of the tick rate rather than of the device: SAVE_STATE_CHUNK *
 * 60Hz is about 6 MB/s no matter what the storage can do, so a 16 MB
 * state took 164 ticks - 2.7 seconds at 60Hz - while spending 99.6%
 * of each frame idle (one 100 KB write measures ~62us of a 16667us
 * frame on NVMe).  A tick is now bounded by a wall-clock budget and
 * transfers as many quanta as fit in it.
 *
 * A time budget is only an improvement if it cannot be worse than
 * the fixed count it replaced, so that is asserted directly rather
 * than argued: with a clock that consumes the whole budget on its
 * first observation, the handler must still transfer exactly one
 * quantum per tick and still produce a correct file.  That is the old
 * behaviour, reached through the new code, and it is what a device
 * slow enough to blow the budget on a single write will see.
 *
 * The rest are correctness properties the throughput work must not
 * be allowed to trade away, and three bugs the audit turned up:
 *
 *  - A truncated state file overread the buffer.  The block walker's
 *    loop bound was 'input < stop', so it could enter the body with
 *    one byte left and then read the block length out of input[4..7]
 *    - six bytes past the end.  ASan reports it for any state
 *    truncated to 9..15 bytes.  content_deserialize_state had the
 *    same shape one level up: it reads seven bytes for the magic and
 *    input[7] for the version with no length check at all.
 *    Savestates are shared, synced and recovered off failing media,
 *    and the load handler's own short-read path produces exactly this
 *    buffer, so this is a file-format parser being handed hostile
 *    input.
 *
 *  - A block's declared 32-bit length was never checked against the
 *    bytes actually remaining, and was then passed to core_unserialize
 *    as the size of the buffer at that offset.
 *
 *  - A failed core_serialize left state->data NULL and state->size 0,
 *    at which point every test in the handler read as success
 *    (remaining == 0 == written, written == size) and the task
 *    reported a COMPLETED save.  The file had already been opened and
 *    truncated, so the slot was left holding zero bytes and the user
 *    was told the save succeeded.
 *
 *  - A failed open was a bare return: not errored, not finished.  The
 *    queue re-entered the task on the next tick and it retried the
 *    open forever.  Save tasks are TASK_TYPE_BLOCKING, so that one
 *    task wedged every other blocking task for the rest of the
 *    session.
 *
 * What this test does NOT assert
 * ------------------------------
 * Nothing about compressed (rzip) states: HAVE_COMPRESSION is off for
 * this binary, so the uncompressed intfstream path is the one under
 * test.  Nothing about cheevos or replay blocks, which are compiled
 * out.
 *
 * The clock
 * ---------
 * cpu_features_get_time_usec() is provided here rather than linked
 * from features_cpu.c, and advances a fixed step per observation.
 * That is what makes the budget assertable: step 0 means the budget
 * never expires (one tick for the whole transfer), step >=
 * SAVE_STATE_TICK_BUDGET_US means it expires on first observation
 * (one quantum per tick).  Both are exact, and neither depends on how
 * many times anything else reads the clock, because extra reads can
 * only advance it further.
 *
 * The two lanes
 * -------------
 * The threaded queue is a lane of this binary rather than a separate
 * one.  The pacing protocol is identical with and without
 * HAVE_THREADS - the locks only serialise it - so every tick-count
 * assertion belongs to the default lane, which initialises the queue
 * unthreaded and is therefore exactly reproducible.  What the "conc"
 * lane adds is a target for TSan: under a threaded queue the transfer
 * loop this change rewrote runs on the worker while
 * content_deserialize_state and the undo bookkeeping run on whichever
 * thread calls task_queue_check(), and the question worth asking of a
 * loop that newly carries a deadline across iterations is whether any
 * of it ended up shared.
 *
 * Build:  make            (SANITIZER=address,undefined, or
 *                          SANITIZER=thread, for a checked run)
 * Run:    ./save_state_io_test        default lane
 *         ./save_state_io_test conc   threaded queue
 *         make sweep                  both lanes under all three
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include <retro_common_api.h>
#include <queues/message_queue.h>
#include <queues/task_queue.h>
#include <streams/file_stream.h>
#include <retro_timers.h>

#include "../../../content.h"
#include "../../../configuration.h"
#include "../../../runloop.h"
#include "../../../core.h"
#include "../../../msg_hash.h"
#include "../../../gfx/video_driver.h"

/* Must match tasks/task_save.c.  Duplicated rather than exported: the
 * constants are an implementation detail of the handler and a test
 * that forced them into a header would be asserting against its own
 * mirror.  A drift here shows up as a failing tick-count assertion,
 * which is the intended way to find out. */
#define TEST_SAVE_STATE_CHUNK        (100 * 1024)
#define TEST_TICK_BUDGET_US          2000

static int failures = 0;

static void ok(const char *what)   { printf("[ok]   %s\n", what); }
static void fail(const char *what) { printf("[FAIL] %s\n", what); failures = 1; }

static void okf(int cond, const char *what)
{
   if (cond) ok(what); else fail(what);
}

/* -----------------------------------------------------------------
 * Deterministic clock
 * ----------------------------------------------------------------- */
static uint64_t clock_now  = 1000000;
static uint64_t clock_step = 0;

retro_time_t cpu_features_get_time_usec(void)
{
   retro_time_t t = (retro_time_t)clock_now;
   clock_now     += clock_step;
   return t;
}

/* The other half of not linking features_cpu.c.  intfstream_get_crc()
 * pulls in encoding_crc32.c, which dispatches on this; nothing on the
 * savestate path calls it, and reporting no features keeps the scalar
 * table path if anything ever does. */
uint64_t cpu_features_get(void) { return 0; }

/* -----------------------------------------------------------------
 * Stub core
 * ----------------------------------------------------------------- */
static size_t   core_len       = 0;
static uint8_t *core_mem       = NULL;
static int      core_ser_fails = 0;

static void core_fill(size_t n)
{
   size_t i;
   free(core_mem);
   core_len = n;
   core_mem = n ? (uint8_t*)malloc(n) : NULL;
   for (i = 0; i < n; i++)
      core_mem[i] = (uint8_t)(i * 31u + 7u);
}

size_t core_serialize_size(void) { return core_len; }

/* Counting allocator, for the one property here that cannot be seen
 * any other way.
 *
 * The undo snapshot keeps its allocation between captures instead of
 * freeing and remallocing it.  Pointer identity cannot observe that:
 * a state-sized block goes to mmap, munmap on free hands the region
 * back, and the next request of the same size is usually handed the
 * same address - so a run of captures looks identical whether the
 * buffer was reused or reallocated, while the page faults that make
 * it expensive are only paid in one of them.  Counting the calls is
 * the discriminator.
 *
 * Only state-sized requests are counted, so the frontend's small
 * incidental allocations (task structs, titles, paths) do not drown
 * the signal.  Linked with -Wl,--wrap so this sees the calls made by
 * the unit under test, not just by this file; under a sanitizer
 * __real_malloc resolves to the sanitizer's allocator, which is what
 * we want - the count is of calls, not of pages. */
#define BIG_ALLOC_BYTES (64 * 1024)

static int big_allocs     = 0;
static int alloc_count_on = 0;

extern void *__real_malloc(size_t n);

void *__wrap_malloc(size_t n)
{
   if (alloc_count_on && n >= BIG_ALLOC_BYTES)
      big_allocs++;
   return __real_malloc(n);
}

static void alloc_count_begin(void)
{
   big_allocs     = 0;
   alloc_count_on = 1;
}

static void alloc_count_end(void) { alloc_count_on = 0; }

static int ser_dest_calls = 0;

static void ser_dest_reset(void) { ser_dest_calls = 0; }
static void ser_dest_note(void *p) { (void)p; ser_dest_calls++; }

bool core_serialize(retro_ctx_serialize_info_t *info)
{
   if (core_ser_fails || !info || info->size < core_len)
      return false;
   ser_dest_note((void*)info->data);
   memcpy((void*)info->data, core_mem, core_len);
   return true;
}

bool core_unserialize(retro_ctx_serialize_info_t *info)
{
   const uint8_t   *src;
   volatile uint8_t sink = 0;
   size_t           i;

   if (!info)
      return false;
   if (!(src = (const uint8_t*)(info->data_const
               ? info->data_const : info->data)))
      return false;

   /* Read every byte the caller says the buffer holds, before
    * deciding whether the size is one this core likes.  That is what
    * a real core does, and it is the entire hazard in an unchecked
    * block length: task_save.c hands core_unserialize a size taken
    * verbatim from the file.  Touching the bytes here is what makes
    * ASan - rather than this stub's own opinion of the size - the
    * thing that catches a block that declares more than the state
    * contains.  A stub that checked the size first would report the
    * over-long block as refused while the read never happened, and
    * would pass against the unfixed walker. */
   for (i = 0; i < info->size; i++)
      sink ^= src[i];
   (void)sink;

   if (info->size != core_len)
      return false;
   memcpy(core_mem, src, core_len);
   return true;
}

bool core_get_memory(retro_ctx_memory_info_t *info)
{
   if (info) { info->data = NULL; info->size = 0; }
   return false;
}

bool core_info_current_supports_savestate(void) { return true; }

/* -----------------------------------------------------------------
 * Stub frontend
 * ----------------------------------------------------------------- */
static settings_t           stub_settings;
static runloop_state_t      stub_runloop;
static video_driver_state_t stub_video;

settings_t *config_get_ptr(void) { return &stub_settings; }
runloop_state_t *runloop_state_get_ptr(void) { return &stub_runloop; }
video_driver_state_t *video_state_get_ptr(void) { return &stub_video; }
bool video_driver_cached_frame_is_hw_render(void) { return false; }
void *savefile_ptr_get(void) { return NULL; }

bool runloop_get_savestate_path(char *path, size_t len, int slot)
{
   snprintf(path, len, "save_state_io_test_auto_%d.state", slot);
   return true;
}

const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   (void)msg;
   return "state message";
}

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }

static void msg_push_stub(retro_task_t *task, const char *msg,
      unsigned prio, unsigned dur, bool flush)
{
   (void)task; (void)msg; (void)prio; (void)dur; (void)flush;
}

static void frontend_reset(void)
{
   memset(&stub_settings, 0, sizeof(stub_settings));
   memset(&stub_runloop,  0, sizeof(stub_runloop));
   memset(&stub_video,    0, sizeof(stub_video));
   stub_video.frame_count      = 1000;   /* past the readiness gate */
   stub_settings.ints.state_slot = 3;
   core_ser_fails              = 0;
   clock_now                   = 1000000;
   clock_step                  = 0;
}

/* -----------------------------------------------------------------
 * Queue driving
 * ----------------------------------------------------------------- */
static bool any_task(retro_task_t *t, void *ud)
{
   (void)t; (void)ud;
   return true;
}

static bool queue_busy(void)
{
   task_finder_data_t fd;
   fd.func     = any_task;
   fd.userdata = NULL;
   return task_queue_find(&fd);
}

/* Ticks the queue until it drains or 'limit' is reached.  Returns the
 * tick count; reaching 'limit' with the queue still busy is itself a
 * finding (a task that never terminates), so callers check for it. */
static int pump(int limit)
{
   int ticks = 0;
   while (ticks < limit)
   {
      task_queue_check();
      ticks++;
      if (!queue_busy())
         break;
   }
   return ticks;
}

static long file_size(const char *p)
{
   RFILE *f = filestream_open(p, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   int64_t n;
   if (!f)
      return -1;
   n = filestream_get_size(f);
   filestream_close(f);
   return (long)n;
}

/* -----------------------------------------------------------------
 * 1. A budget that never expires transfers the whole state in one
 *    tick - the point of the change.
 * ----------------------------------------------------------------- */
static void test_budget_unlimited(void)
{
   const char *path = "sst_budget_full.state";
   size_t sz        = 64 * TEST_SAVE_STATE_CHUNK;   /* 6.25 MB */
   int ticks;

   frontend_reset();
   core_fill(sz);
   filestream_delete(path);
   clock_step = 0;   /* clock never advances: budget never expires */

   content_save_state(path, true);
   ticks = pump(1000);

   okf(ticks == 1,
       "unexpired budget writes a 64-quantum state in a single tick");
   if (ticks != 1)
      printf("       (took %d ticks)\n", ticks);

   okf(file_size(path) == (long)content_get_serialized_size(),
       "budgeted write produces a full-length file");

   filestream_delete(path);
}

/* -----------------------------------------------------------------
 * 2. A budget that expires on first observation degrades to exactly
 *    one quantum per tick.  This is the no-worst-case-regression
 *    property: it is the behaviour the handler had before the budget
 *    existed, and it is what the slowest supported storage gets.
 * ----------------------------------------------------------------- */
static void test_budget_exhausted(void)
{
   const char *path = "sst_budget_none.state";
   size_t quanta    = 12;
   size_t sz        = quanta * TEST_SAVE_STATE_CHUNK;
   size_t total;
   int    ticks, expect;

   frontend_reset();
   core_fill(sz);
   filestream_delete(path);
   /* One observation spends the entire budget. */
   clock_step = TEST_TICK_BUDGET_US;

   total  = content_get_serialized_size();
   expect = (int)((total + TEST_SAVE_STATE_CHUNK - 1)
                  / TEST_SAVE_STATE_CHUNK);

   content_save_state(path, true);
   ticks = pump(1000);

   okf(ticks == expect,
       "exhausted budget falls back to exactly one quantum per tick");
   if (ticks != expect)
      printf("       (expected %d ticks, took %d)\n", expect, ticks);

   okf(file_size(path) == (long)total,
       "one-quantum-per-tick path still writes the whole file");

   filestream_delete(path);
}

/* -----------------------------------------------------------------
 * 3. Round-trip under both pacings.  Throughput work that corrupts
 *    the payload is worse than the slow path it replaced.
 * ----------------------------------------------------------------- */
static void test_roundtrip(uint64_t step, const char *label)
{
   const char *path = "sst_roundtrip.state";
   size_t sz        = 7 * TEST_SAVE_STATE_CHUNK + 1234; /* unaligned tail */
   uint8_t *expect;

   frontend_reset();
   core_fill(sz);
   expect = (uint8_t*)malloc(sz);
   memcpy(expect, core_mem, sz);
   filestream_delete(path);
   clock_step = step;

   content_save_state(path, true);
   pump(1000);

   /* Refill the core with a different pattern so a load that does
    * nothing at all cannot pass. */
   memset(core_mem, 0xA5, sz);

   content_load_state(path, false, false);
   pump(1000);

   okf(memcmp(expect, core_mem, sz) == 0, label);

   free(expect);
   filestream_delete(path);
}

/* -----------------------------------------------------------------
 * 4. A failed serialize must fail the task and must not leave a
 *    zero-byte file in the slot.
 * ----------------------------------------------------------------- */
static void test_serialize_failure(void)
{
   const char *path = "sst_serfail.state";
   int ticks;

   frontend_reset();
   core_fill(256 * 1024);
   filestream_delete(path);

   /* Defer serialization into the handler, then refuse it there. */
   set_save_state_in_background(true);
   core_ser_fails = 1;

   content_save_state(path, true);
   ticks = pump(1000);

   core_ser_fails = 0;
   set_save_state_in_background(false);

   okf(ticks < 1000, "failed serialize terminates the save task");
   okf(file_size(path) < 0,
       "failed serialize leaves no file (not a zero-byte one)");
   if (file_size(path) == 0)
      printf("       (a zero-byte state file was left in the slot)\n");

   filestream_delete(path);
}

/* -----------------------------------------------------------------
 * 5. A failed open must terminate the task, not retry forever.  Save
 *    tasks are blocking, so a task that never finishes takes the
 *    whole blocking queue with it.
 * ----------------------------------------------------------------- */
static void test_open_failure(void)
{
   /* A path whose parent directory does not exist: open fails and
    * will keep failing, which is the case the old bare-return
    * retried on every tick for the rest of the session. */
   const char *path = "sst_no_such_dir/deeper/still/x.state";
   int ticks;

   frontend_reset();
   core_fill(128 * 1024);

   content_save_state(path, true);
   ticks = pump(500);

   okf(ticks < 500, "failed open terminates the save task");
   if (ticks >= 500)
      printf("       (task still queued after 500 ticks - it is "
             "retrying the open, and it is blocking)\n");
}

/* -----------------------------------------------------------------
 * 6. Truncated state files.  Under ASan an overread aborts the
 *    process, so reaching the end of this function at all is half the
 *    assertion; the other half is that each truncation is rejected
 *    rather than accepted as a valid state.
 * ----------------------------------------------------------------- */
static void test_truncated_state(void)
{
   size_t   sz = 4096;
   size_t   full;
   uint8_t *good;
   size_t   cut;
   int      all_rejected = 1;

   frontend_reset();
   core_fill(sz);

   full = content_get_serialized_size();
   good = (uint8_t*)malloc(full);
   content_serialize_state_rewind(good, full);

   /* 0..16 spans: shorter than the magic, shorter than the version
    * byte, and the 9..15 range where the walker used to read a block
    * length that was not there. */
   for (cut = 0; cut <= 16 && cut < full; cut++)
   {
      uint8_t *slice = (uint8_t*)malloc(cut ? cut : 1);
      if (cut)
         memcpy(slice, good, cut);
      if (content_deserialize_state(slice, cut))
      {
         printf("       (a %zu-byte truncation was accepted)\n", cut);
         all_rejected = 0;
      }
      free(slice);
   }

   okf(all_rejected,
       "state files truncated to 0..16 bytes are rejected, not parsed");

   /* A well-formed header whose first block claims far more bytes
    * than the buffer holds.  Rejection must come from the extent
    * check, not from the core happening to dislike the size. */
   {
      uint8_t *evil = (uint8_t*)malloc(full);
      memcpy(evil, good, full);
      evil[8 + 4] = 0xFF;
      evil[8 + 5] = 0xFF;
      evil[8 + 6] = 0xFF;
      evil[8 + 7] = 0x0F;              /* ~256 MB in a 4 KB buffer */
      okf(!content_deserialize_state(evil, full),
          "a block longer than the buffer is refused");
      free(evil);
   }

   /* A length that overflows when aligned up to 8 on a 32-bit
    * size_t.  Must be refused by the extent check before the
    * alignment arithmetic runs. */
   {
      uint8_t *evil = (uint8_t*)malloc(full);
      memcpy(evil, good, full);
      evil[8 + 4] = 0xFF;
      evil[8 + 5] = 0xFF;
      evil[8 + 6] = 0xFF;
      evil[8 + 7] = 0xFF;              /* UINT32_MAX */
      okf(!content_deserialize_state(evil, full),
          "a block length of UINT32_MAX is refused before alignment");
      free(evil);
   }

   free(good);
}

/* -----------------------------------------------------------------
 * 7. A state file that is shorter than its own contents claim must
 *    fail the load, not half-apply it.
 * ----------------------------------------------------------------- */
static void test_short_file(void)
{
   const char *path = "sst_short.state";
   size_t sz        = 3 * TEST_SAVE_STATE_CHUNK;
   uint8_t *expect;
   RFILE *f;
   int64_t full;
   uint8_t *raw;

   frontend_reset();
   core_fill(sz);
   expect = (uint8_t*)malloc(sz);
   memcpy(expect, core_mem, sz);
   filestream_delete(path);

   content_save_state(path, true);
   pump(1000);

   /* Rewrite the file with its last quantum missing. */
   full = file_size(path);
   raw  = (uint8_t*)malloc((size_t)full);
   f    = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   filestream_read(f, raw, full);
   filestream_close(f);
   filestream_delete(path);
   f = filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   filestream_write(f, raw, full - TEST_SAVE_STATE_CHUNK);
   filestream_close(f);
   free(raw);

   memset(core_mem, 0x5A, sz);

   content_load_state(path, false, false);
   pump(1000);

   {
      size_t i;
      int untouched = 1;
      for (i = 0; i < sz; i++)
         if (core_mem[i] != 0x5A) { untouched = 0; break; }
      okf(untouched,
          "a truncated state file is refused, not partially applied");
   }

   free(expect);
   filestream_delete(path);
}

/* -----------------------------------------------------------------
 * 8. Mutual exclusion between save/load tasks.
 *
 *    Every task this file pushes is TASK_TYPE_BLOCKING, so the queue
 *    admits at most one of them at a time and refuses the rest.  Two
 *    of the fixes above rest on that rule and one of them is only
 *    severe because of it - a save task that retried a failed open
 *    forever did not merely fail to save, it held the single blocking
 *    slot for the rest of the session, so no state could be saved or
 *    loaded again without a restart.
 *
 *    The refusal path is also where task_push_save_state has to undo
 *    everything it built - the task, its title, the state struct, and
 *    the caller's serialized buffer, which it took ownership of.
 *    LeakSan is the assertion for that half; reaching the end of this
 *    function with the queue drained is the assertion for the rest.
 * ----------------------------------------------------------------- */
static void test_blocking_exclusion(void)
{
   const char *first  = "sst_excl_a.state";
   const char *second = "sst_excl_b.state";
   int         i;

   frontend_reset();
   core_fill(40 * TEST_SAVE_STATE_CHUNK);
   filestream_delete(first);
   filestream_delete(second);

   /* One observation spends the budget, so the first save needs ~40
    * ticks and is still in flight for the whole of this test. */
   clock_step = TEST_TICK_BUDGET_US;

   content_save_state(first, true);
   task_queue_check();                 /* first save now running */

   okf(queue_busy(), "the first blocking task is running");

   /* Push several more while it runs.  Each must be refused, and each
    * refusal must free everything it allocated. */
   for (i = 0; i < 8; i++)
   {
      content_save_state(second, true);
      task_queue_check();
   }

   okf(file_size(second) == -1,
       "a second save is refused while one is in flight");

   pump(1000);

   okf(!queue_busy(), "the queue drains after the refusals");
   okf(file_size(first) == (long)content_get_serialized_size(),
       "the admitted save completes intact despite the refusals");

   filestream_delete(first);
   filestream_delete(second);
}

/* -----------------------------------------------------------------
 * The threaded lane.
 *
 * Under HAVE_THREADS the queue runs handlers on its worker thread and
 * retires them - callbacks included - on whichever thread calls
 * task_queue_check().  So the budgeted transfer loop this change
 * rewrote executes off the main thread in production, while
 * content_deserialize_state and the undo bookkeeping stay on it.
 * That split is what TSan is pointed at here: the loop now reads a
 * clock and carries a deadline across iterations, and the question is
 * whether any of that ended up shared.
 *
 * It is deliberately one task at a time.  That is not a simplification
 * - it is the shape the queue enforces, per test_blocking_exclusion
 * above, and a lane that pushed concurrent save tasks would be
 * asserting against a configuration the frontend cannot produce.
 * What runs concurrently here is the handler against the main thread's
 * gather, pump and verification, which is exactly production.
 *
 * The fake clock is left at step 0 for this lane: it is read only by
 * the handler, i.e. only from the worker, so it stays a
 * single-threaded object even though the process is not.  A lane that
 * made the budget expire would have the main thread's frontend_reset
 * writing what the worker reads, and TSan would be reporting the
 * test's own scaffolding rather than the unit.
 * ----------------------------------------------------------------- */
static void pump_threaded(void)
{
   int spins = 0;
   /* The worker owns progress; this thread only gathers.  Bounded so
    * a handler that never terminates fails the run instead of hanging
    * CI. */
   while (spins++ < 20000)
   {
      task_queue_check();
      if (!queue_busy())
         return;
      retro_sleep(1);
   }
   fail("threaded queue drained within the spin bound");
}

static void test_threaded_roundtrip(void)
{
   const char *path = "sst_threaded.state";
   size_t sz        = 9 * TEST_SAVE_STATE_CHUNK + 77;
   uint8_t *expect;

   frontend_reset();
   core_fill(sz);
   expect = (uint8_t*)malloc(sz);
   memcpy(expect, core_mem, sz);
   filestream_delete(path);

   content_save_state(path, true);
   pump_threaded();

   okf(file_size(path) == (long)content_get_serialized_size(),
       "threaded queue: the save task writes a full-length file");

   memset(core_mem, 0xA5, sz);

   content_load_state(path, false, false);
   pump_threaded();

   okf(memcmp(expect, core_mem, sz) == 0,
       "threaded queue: round-trip is byte-exact");

   free(expect);
   filestream_delete(path);
}

/* -----------------------------------------------------------------
 * 9. The undo snapshot reuses its allocation.
 *
 *    content_load_state_cb retakes the undo snapshot on every single
 *    state load, so this is the hottest serialize the frontend does,
 *    and it used to free its destination and malloc a new one each
 *    time.  A state-sized allocation is served by mmap, so the free
 *    hands the pages back to the kernel and the next serialize faults
 *    every one of them in on first touch - measured at 63% of the
 *    cost of a 16 MiB serialize, more than the copy it exists to
 *    hold.
 *
 *    The snapshot now keeps the outgoing allocation as a spare and
 *    swaps the two, so a run of snapshots at a fixed state size must
 *    settle onto exactly two buffers.  Two rather than one because it
 *    is a swap, which is what preserves the failure semantics tested
 *    below; a third would mean something is still reallocating.
 * ----------------------------------------------------------------- */
static void test_undo_snapshot_reuses(void)
{
   int i, taken = 0;

   frontend_reset();
   core_fill(8 * TEST_SAVE_STATE_CHUNK);
   content_reset_savestate_backups();
   ser_dest_reset();

   alloc_count_begin();
   for (i = 0; i < 8; i++)
      if (content_save_state("RAM", false))
         taken++;
   alloc_count_end();

   okf(taken == 8, "eight consecutive undo snapshots all succeed");
   okf(ser_dest_calls == 8, "eight snapshots ran eight serializes");

   /* Two, not one: it is a swap, and the second capture has to build
    * its snapshot somewhere while the first is still the live one.
    * From the third onwards the two allocations ping-pong and nothing
    * further is requested.  Reallocating per capture - what this
    * replaced - measures eight. */
   okf(big_allocs == 2,
       "eight snapshots allocate two state-sized buffers, not eight");
   if (big_allocs != 2)
      printf("       (%d state-sized allocations for 8 snapshots)\n",
            big_allocs);

   content_reset_savestate_backups();
}

/* -----------------------------------------------------------------
 * 10. The reused buffer still grows, and a grown snapshot is correct.
 *
 *     The serialized length is not fixed - it tracks the core's state
 *     size, which changes when content changes.  A reused buffer that
 *     failed to grow would be an overflow, and one that grew but kept
 *     a stale length would be a truncated snapshot.  Undoing back to
 *     each size is what checks both.
 * ----------------------------------------------------------------- */
static void test_undo_snapshot_grows(void)
{
   size_t   small = 3 * TEST_SAVE_STATE_CHUNK;
   size_t   big   = 11 * TEST_SAVE_STATE_CHUNK;
   uint8_t *expect;

   frontend_reset();
   content_reset_savestate_backups();

   /* Small state, snapshot it, then grow the core and snapshot again:
    * the second snapshot must not be served out of the first, smaller
    * allocation. */
   core_fill(small);
   okf(content_save_state("RAM", false), "snapshot at the smaller size");

   core_fill(big);
   expect = (uint8_t*)malloc(big);
   memcpy(expect, core_mem, big);
   okf(content_save_state("RAM", false), "snapshot after the state grew");

   memset(core_mem, 0x3C, big);
   okf(content_undo_load_state(), "undo of the grown snapshot succeeds");
   okf(memcmp(expect, core_mem, big) == 0,
       "a grown snapshot restores byte-exactly");

   free(expect);
   content_reset_savestate_backups();
}

/* -----------------------------------------------------------------
 * 11. A failed serialize leaves the previous snapshot intact.
 *
 *     This is the property the swap exists to preserve, and the one
 *     that an in-place rewrite of the live snapshot would have
 *     silently broken: serializing straight over the buffer would
 *     leave a half-written snapshot that still looks valid, so an
 *     undo after a failed capture would restore garbage.  Filling the
 *     spare and only then swapping means the live snapshot is
 *     untouched until a full one is ready to replace it.
 * ----------------------------------------------------------------- */
static void test_undo_snapshot_failure_keeps_previous(void)
{
   size_t   sz = 5 * TEST_SAVE_STATE_CHUNK;
   uint8_t *expect;

   frontend_reset();
   core_fill(sz);
   content_reset_savestate_backups();

   expect = (uint8_t*)malloc(sz);
   memcpy(expect, core_mem, sz);

   okf(content_save_state("RAM", false), "a good snapshot is taken");

   /* Move the core on, then fail the next capture. */
   memset(core_mem, 0x77, sz);
   core_ser_fails = 1;
   okf(!content_save_state("RAM", false),
       "a failed serialize reports failure");
   core_ser_fails = 0;

   /* The snapshot that survives must be the good one, not a partial
    * overwrite of it. */
   memset(core_mem, 0x11, sz);
   okf(content_undo_load_state(),
       "the previous snapshot is still undoable after a failure");
   okf(memcmp(expect, core_mem, sz) == 0,
       "a failed capture leaves the previous snapshot byte-exact");

   free(expect);
   content_reset_savestate_backups();
}

/* -----------------------------------------------------------------
 * 12. Undo allocates nothing in steady state.
 *
 *     content_undo_load_state has to hold on to the snapshot it is
 *     restoring while it captures the current state over the top of
 *     it, and it used to do that by malloc'ing a full-size temporary
 *     and copying the whole state into it - a second state-sized
 *     allocation and a second full copy, on top of the capture's own.
 *     It now detaches the snapshot's allocation instead, and hands it
 *     to the spare afterwards.
 *
 *     So a run of undos, each of which also takes a capture, must
 *     allocate nothing at all once the two buffers exist: the
 *     snapshot being restored becomes the next spare, and the spare
 *     becomes the next snapshot.  Before the change each undo cost
 *     one temporary, and before the capture change it cost a second.
 * ----------------------------------------------------------------- */
static void test_undo_allocates_nothing(void)
{
   size_t   sz = 6 * TEST_SAVE_STATE_CHUNK;
   uint8_t *first;
   int      i, undone = 0;

   frontend_reset();
   core_fill(sz);
   content_reset_savestate_backups();

   first = (uint8_t*)malloc(sz);
   memcpy(first, core_mem, sz);

   /* Reach steady state first: one capture, then one undo.  The undo
    * is what leaves both allocations in play - the snapshot it built
    * and the one it detached and handed to the spare - so only the
    * undos after it are measuring a system that has stopped growing.
    * The core alternates between the two states from here on. */
   content_save_state("RAM", false);
   memset(core_mem, 0x21, sz);
   content_undo_load_state();

   alloc_count_begin();
   for (i = 0; i < 6; i++)
      if (content_undo_load_state())
         undone++;
   alloc_count_end();

   okf(undone == 6, "six consecutive undos all succeed");
   okf(big_allocs == 0,
       "undo allocates no state-sized buffer in steady state");
   if (big_allocs != 0)
      printf("       (%d state-sized allocations for 6 undos)\n",
            big_allocs);

   /* An even number of undos of a two-state alternation lands back
    * where it started, which is what proves the buffers were actually
    * swapped and not merely not-allocated. */
   okf(memcmp(first, core_mem, sz) == 0,
       "an even number of undos returns the original state");

   free(first);
   content_reset_savestate_backups();
}

static int run_default_lane(void)
{
   printf("== task_save.c I/O regression oracle ==\n");
   printf("   quantum %d B, tick budget %d us\n\n",
         TEST_SAVE_STATE_CHUNK, TEST_TICK_BUDGET_US);

   task_queue_init(false, msg_push_stub);

   test_budget_unlimited();
   test_budget_exhausted();
   test_roundtrip(0, "round-trip is byte-exact with an unexpired budget");
   test_roundtrip(TEST_TICK_BUDGET_US,
         "round-trip is byte-exact at one quantum per tick");
   test_serialize_failure();
   test_open_failure();
   test_truncated_state();
   test_short_file();
   test_blocking_exclusion();
   test_undo_snapshot_reuses();
   test_undo_snapshot_grows();
   test_undo_snapshot_failure_keeps_previous();
   test_undo_allocates_nothing();

   content_reset_savestate_backups();
   task_queue_deinit();
   free(core_mem);
   core_mem = NULL;
   return failures;
}

static int run_threaded_lane(void)
{
   printf("== task_save.c I/O regression oracle (threaded queue) ==\n\n");

   task_queue_init(true, msg_push_stub);

   test_threaded_roundtrip();

   content_reset_savestate_backups();
   task_queue_deinit();
   free(core_mem);
   core_mem = NULL;
   return failures;
}

int main(int argc, char *argv[])
{
   if (argc > 1 && strcmp(argv[1], "conc") == 0)
      run_threaded_lane();
   else
      run_default_lane();

   printf("\n%s\n", failures ? "FAILURES" : "all checks passed");
   return failures;
}

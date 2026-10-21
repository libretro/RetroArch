/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (data_transfer_prefix_test.c).
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

/* Regression test for data_transfer's file-backed (prefix) mode.
 *
 * Prefix mode is what lets a consumer pay for only the part of a file
 * it actually needs - a thumbnail decoder reading a header, an audio
 * decoder playing a long file in a constant residency window - so its
 * contract is more than "the bytes arrive".
 *
 *   - A budgeted fill must still deliver the whole file, unchanged.
 *   - discard() releases the physical backing of bytes below a point
 *     and those bytes are then gone; refill() must bring them back
 *     with the file's real contents, because a consumer that seeks
 *     backwards depends on it.
 *   - commit_cap is a ceiling with its own terminal: a transfer that
 *     reaches it reports capped(), which is not complete(), so a
 *     caller can tell "there is more file" from "that was all of it".
 *     A cap at or above the length must not turn a normal completion
 *     into a capped one.
 *   - a file that is not there must be refused at open rather than
 *     presented as an empty success.
 *
 * Build:  make            (SANITIZER=address,undefined for a checked run)
 * Run:    ./data_transfer_prefix_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <formats/data_transfer.h>
#include <memmap.h>
#ifdef DT_COUNT_IO
#include <streams/file_stream.h>
static uint64_t inert_io_bytes, inert_io_calls;
int64_t __real_filestream_read(RFILE *stream, void *data, int64_t length);
int64_t __wrap_filestream_read(RFILE *stream, void *data, int64_t length)
{
   int64_t got = __real_filestream_read(stream, data, length);
   inert_io_calls++;
   if (got > 0) inert_io_bytes += (uint64_t)got;
   return got;
}
#endif
#if defined(__linux__)
#include <sys/mman.h>
#endif

static int bad = 0;
static void ok(const char *what) { printf("[ok]   %s\n", what); }
static void fail(const char *what) { printf("[FAIL] %s\n", what); bad = 1; }

/* allows 'ud' more reads, then refuses */
static bool hook_stop_after(void *ud, size_t avail, size_t len)
{
   int *left = (int*)ud;
   (void)avail;
   (void)len;
   if (*left <= 0)
      return false;
   (*left)--;
   return true;
}

/* A file of one repeated byte, for checks that need to tell two
 * buffers apart by content. */
static int mkfile_filled(const char *path, uint8_t v, size_t n)
{
   FILE *f = fopen(path, "wb");
   uint8_t *b;
   if (!f)
      return 0;
   if (!(b = (uint8_t*)malloc(n)))
   {
      fclose(f);
      return 0;
   }
   memset(b, v, n);
   fwrite(b, 1, n, f);
   free(b);
   fclose(f);
   return 1;
}

static void mkfile(const char *path, uint8_t *ref, size_t n)
{
   FILE *f = fopen(path, "wb"); size_t i;
   for (i = 0; i < n; i++) ref[i] = (uint8_t)(i * 167 + 29);
   if (n) fwrite(ref, 1, n, f);
   fclose(f);
}

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

/* The arena rides along in this file because it has no home of its
 * own: it shares data_transfer.c and the header, but touches no
 * transfer, so neither of the other two suites is a better fit.
 *
 * Its growth is doubling from the current capacity until it covers
 * the request.  A request with no power of two at or above it - which
 * is any request above half the address space - used to send that
 * loop through 0 and then spin on it forever, because 0 * 2 is 0 and
 * the exit test is 'nc < need'.  realloc never gets asked, so there
 * is no failure to report: the caller simply never returns.  The
 * sizes come from container-declared lengths in the webm core, which
 * is the sort of place an absurd one arrives from.
 *
 * A regression here hangs rather than fails, so the call runs in a
 * child under an alarm and the hang is read off the signal.  The
 * property under test is only that the loop terminates: a sanitizer
 * build ends the process on the oversized realloc rather than
 * returning NULL from it, which still means the allocator was
 * reached, so the child reports the two outcomes it can distinguish
 * by exit code and anything else counts as having got that far. */
static int arena_overflow_check(void)
{
   pid_t pid = fork();
   int   status = 0;

   if (pid < 0)
   {
      printf("[skip] arena overflow: fork unavailable\n");
      return 0;
   }
   if (pid == 0)
   {
      data_transfer_arena_t a;
      alarm(5);
      data_transfer_arena_init(&a, 1024);
      /* 70 and 71 are picked to not collide with the exit code a
       * sanitizer uses when it ends the process itself */
      if (data_transfer_arena_ensure(&a, (size_t)-2))
      {
         data_transfer_arena_release(&a);
         _exit(71);            /* claimed to have allocated it */
      }
      _exit(70);               /* refused by returning false */
   }
   waitpid(pid, &status, 0);

   if (WIFSIGNALED(status) && WTERMSIG(status) == SIGALRM)
   {
      printf("[FAIL] arena_ensure spun on an unrepresentable size\n");
      return 1;
   }
   if (WIFEXITED(status) && WEXITSTATUS(status) == 71)
   {
      printf("[FAIL] arena_ensure claimed to allocate an "
             "unrepresentable size\n");
      return 1;
   }
   if (WIFEXITED(status) && WEXITSTATUS(status) == 70)
   {
      printf("[ok]   arena_ensure refuses an unrepresentable size\n");
      return 0;
   }
   /* The allocator was reached and ended the process rather than
    * returning - a sanitizer build's answer to a request this size.
    * The loop terminated, which is the whole point. */
   printf("[ok]   arena_ensure reached the allocator, which refused "
          "it outright\n");
   return 0;
}
#else
static int arena_overflow_check(void)
{
   printf("[skip] arena overflow: needs fork()\n");
   return 0;
}
#endif

/* Under a strict build, discarded bytes must fault rather than read
 * as zeros.
 *
 * This is what makes a streaming consumer's look-back margin testable.
 * The webm core discards behind playback with a fixed margin and its
 * whole safety argument is that the decoder never looks further back
 * than that; without strict discard a violation reads zeros and the
 * decoder consumes them as content, exactly the silent-corruption
 * failure the window mode already protects against.  Building the
 * consumer against -DDT_STRICT turns that into a crash at the moment
 * of the mistake.
 *
 * Only meaningful in a strict build, so the default lane skips. */
/* A recycled buffer must not tell the next consumer about the last one.
 *
 * A prefix transfer over a small file recycles its reservation rather
 * than releasing it, which is the point - a fresh reservation's pages
 * cost a fault each on first touch, a recycled one's do not.  But the
 * recycled pages still hold the previous load, and the commit step is
 * coarser than avail(): a file smaller than DT_COMMIT_STEP has its
 * whole length armed by the first commit, so everything from avail()
 * to the end of the file is readable whether the buffer is recycled
 * or not.  On a fresh reservation those bytes are zero.  On a
 * recycled one they were the previous file, until the commit path
 * learned to scrub what it arms.
 *
 * That is a leak across loads - one thumbnail's buffer answering with
 * another's content - and it is invisible unless a test looks at the
 * bytes above avail() specifically after a reuse.  So: fill a slot
 * repeatedly with one pattern, then load a different file only
 * partway and read past the frontier. */
static int pool_reuse_check(void)
{
   const char *pa = "/tmp/dtprefix_pool_a.bin";
   const char *pb = "/tmp/dtprefix_pool_b.bin";
   size_t n = 256u << 10;
   data_transfer_t *dt;
   const uint8_t *base;
   size_t len = 0, av, i;
   int bad = 0, laps;

   /* Written here rather than through mkfile(): this check needs two
    * files that differ, and mkfile generates one fixed pattern. */
   if (!mkfile_filled(pa, 0xAA, n) || !mkfile_filled(pb, 0xBB, n))
   {
      remove(pa); remove(pb);
      return 0;
   }

   /* cycle enough times that the next open certainly reuses */
   for (laps = 0; laps < 8; laps++)
   {
      dt = data_transfer_open_prefix(pa, 0);
      if (!dt) { remove(pa); remove(pb); return 0; }
      data_transfer_iterate(dt, 0);
      data_transfer_free(dt);
   }

   if (!(dt = data_transfer_open_prefix(pb, 0)))
   {
      remove(pa); remove(pb);
      return 0;
   }
   data_transfer_iterate(dt, 4096);       /* a prefix, not the file */
   base = data_transfer_ptr(dt, &len);
   av   = data_transfer_avail(dt);

   if (av == 0 || av >= len)
      printf("[skip] buffer reuse: the budget did not stop the fill\n");
   else
   {
      for (i = 0; i < av; i++)
         if (base[i] != 0xBB)
         {
            fail("the filled prefix is not the file being read");
            bad = 1;
            break;
         }
      for (i = av; i < len; i++)
         if (base[i] != 0x00)
         {
            printf("[FAIL] byte %u above avail reads %02X - a recycled "
                   "buffer is leaking the previous load\n",
                  (unsigned)i, base[i]);
            bad = 1;
            break;
         }
      if (!bad)
         ok("buffer reuse: bytes above avail read as zero, not as "
            "the previous file");
   }
   data_transfer_free(dt);
   remove(pa);
   remove(pb);
   return bad;
}

#if defined(__unix__) || defined(__APPLE__)
static int strict_discard_check(int keep_filling)
{
   const char *lane = keep_filling ? "mid-fill" : "after fill";
#if !defined(DT_STRICT)
   (void)lane;
   printf("[skip] strict discard: build with -DDT_STRICT\n");
   return 0;
#elif defined(__SANITIZE_ADDRESS__)
   (void)lane;
   printf("[skip] strict discard: the sanitizer intercepts it\n");
   return 0;
#else
   const char *path = keep_filling ? "/tmp/dtprefix_strict_mid.bin"
                                  : "/tmp/dtprefix_strict_end.bin";
   size_t n = 4u << 20;
   uint8_t *ref = (uint8_t*)malloc(n);
   pid_t pid;
   int status = 0;

   if (!ref)
      return 0;
   mkfile(path, ref, n);
   free(ref);

   if (!data_transfer_reserve_supported())
   {
      printf("[skip] strict discard: no reservations on this build\n");
      remove(path);
      return 0;
   }

   if ((pid = fork()) == 0)
   {
      data_transfer_t *dt = data_transfer_open_prefix(path, 0);
      const uint8_t *base;
      size_t len = 0;
      volatile uint8_t v;

      if (!dt)
         _exit(3);
      if (keep_filling)
      {
         /* Discard mid-fill and then carry on reading, which is what
          * a streaming consumer actually does.  Discarding only after
          * the fill is over leaves the commit path untouched, so it
          * cannot see a later commit re-arming the released pages;
          * this lane crosses at least one DT_COMMIT_STEP boundary
          * after the discard before looking back. */
         data_transfer_iterate(dt, 2u << 20);
         base = data_transfer_ptr(dt, &len);
         data_transfer_discard(dt, 1u << 20);
         data_transfer_iterate(dt, 0);
         if (!data_transfer_complete(dt))
            _exit(4);
      }
      else
      {
         data_transfer_iterate(dt, 0);
         if (!data_transfer_complete(dt))
            _exit(4);
         base = data_transfer_ptr(dt, &len);
         data_transfer_discard(dt, 1u << 20);
      }
      v = base[4096];             /* well below the discard frontier */
      _exit(42);                  /* reached only if it did not fault */
      (void)v;
   }
   waitpid(pid, &status, 0);
   remove(path);

   if (WIFEXITED(status) && WEXITSTATUS(status) == 42)
   {
      printf("[FAIL] a strict build still reads zeros below the "
             "discard frontier (%s)\n", lane);
      return 1;
   }
   if (WIFEXITED(status) && WEXITSTATUS(status) >= 3
         && WEXITSTATUS(status) <= 4)
   {
      printf("[FAIL] the strict-discard child bailed at setup "
            "(%s, %d)\n", lane, WEXITSTATUS(status));
      return 1;
   }
   /* Anything else is the fault: a signal normally, or the sanitizer
    * ending the process its own way with its own exit code. */
   printf("[ok]   strict discard (%s): a look-back below the "
          "frontier faults\n", lane);
   return 0;
#endif
}
#else
static int strict_discard_check(int keep_filling)
{
   (void)keep_filling;
   printf("[skip] strict discard: needs fork()\n");
   return 0;
}
#endif

/* The continue hook: a stop is a pause, and it is fine-grained.
 *
 * Two properties matter to the callers that use it as a frame guard.
 * It has to be consulted between the fill's internal reads rather
 * than once per call, or a deadline is no better than the byte budget
 * it replaced - so a hook that stops after N calls must leave about
 * N chunks of data, not the whole file.  And stopping must settle
 * nothing: the transfer has to resume exactly where it stopped, or a
 * consumer that runs out of frame time would lose the file. */
static int continue_hook_check(void)
{
   const char *path = "/tmp/dtprefix_hook.bin";
   size_t n = 4u << 20;
   uint8_t *ref = (uint8_t*)malloc(n);
   data_transfer_t *dt;
   int calls = 0, rv = 0;
   size_t after_stop;

   if (!ref)
      return 0;
   mkfile(path, ref, n);

   /* stop immediately: no work at all, and nothing settled */
   if (!(dt = data_transfer_open_prefix(path, 0)))
   {
      printf("[FAIL] open for the hook check\n");
      free(ref); remove(path);
      return 1;
   }
   calls = 0;
   data_transfer_iterate_while(dt, 0, hook_stop_after, &calls);
   if (data_transfer_avail(dt) != 0)
   {
      printf("[FAIL] a hook refusing at once still read %u bytes\n",
            (unsigned)data_transfer_avail(dt));
      rv = 1;
   }
   else if (data_transfer_failed(dt) || data_transfer_complete(dt))
   {
      printf("[FAIL] refusing at once settled the transfer\n");
      rv = 1;
   }
   else
      ok("a hook refusing at once reads nothing and settles nothing");

   /* let it through a few reads: the stop must land near a chunk
    * boundary, not at the end of the file */
   calls = 4;
   data_transfer_iterate_while(dt, 0, hook_stop_after, &calls);
   after_stop = data_transfer_avail(dt);
   if (after_stop == 0 || after_stop >= n)
   {
      printf("[FAIL] the hook was not consulted between reads "
             "(%u of %u bytes)\n", (unsigned)after_stop, (unsigned)n);
      rv = 1;
   }
   else
      printf("[ok]   the hook lands between reads: %u KiB in, "
             "not %u KiB\n", (unsigned)(after_stop >> 10),
            (unsigned)(n >> 10));

   /* and the fill resumes from exactly there */
   data_transfer_iterate(dt, 0);
   if (!data_transfer_complete(dt))
   {
      printf("[FAIL] the transfer did not resume after a hook stop\n");
      rv = 1;
   }
   else
   {
      size_t len = 0;
      const uint8_t *base = data_transfer_ptr(dt, &len);
      if (len != n || memcmp(base, ref, n))
      {
         printf("[FAIL] resumed contents do not match the file\n");
         rv = 1;
      }
      else
         ok("a stopped fill resumes and delivers the whole file");
   }

   data_transfer_free(dt);
   free(ref); remove(path);
   return rv;
}

static void inert_mark(data_transfer_t *dt, uint8_t *live, size_t pages,
      size_t page, size_t from, size_t to)
{
   size_t i;
   if (!data_transfer_prefix_inert(dt, from, to))
   { fail("register inert prefix interval"); return; }
   for (i = 0; i < pages; i++)
      if (i * page >= from && (i + 1) * page <= to) live[i] = 0;
}

static void inert_physical_check(const uint8_t *base, const uint8_t *live,
      size_t pages, size_t page)
{
#if defined(__linux__)
   unsigned char *state;
   size_t i;
   if (!data_transfer_reserve_supported() || (uintptr_t)base % page)
   { printf("[skip] inert physical probe: no page reservation\n"); return; }
   state = (unsigned char*)calloc(pages, 1);
   if (!state) { fail("inert residency vector allocation"); return; }
   if (mincore((void*)base, pages * page, state)) fail("inert mincore query");
   else
   {
      for (i = 0; i < pages; i++)
         if (!live[i] && (state[i] & 1)) break;
      if (i < pages) fail("inert fill/refill materialized a skipped page");
      else ok("inert pages have no resident backing");
   }
   free(state);
#else
   (void)base; (void)live; (void)pages; (void)page;
   printf("[skip] inert physical probe: Linux mincore only\n");
#endif
}

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define PREFIX_TEST_ASAN 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#define PREFIX_TEST_ASAN 1
#endif
static void inert_fault_check(const uint8_t *inert)
{
#if defined(DT_STRICT) && !defined(PREFIX_TEST_ASAN) \
      && (defined(__unix__) || defined(__APPLE__))
   pid_t child = fork();
   int status;
   if (child < 0) { printf("[skip] inert fault check: fork unavailable\n"); return; }
   if (!child)
   {
      volatile uint8_t byte;
      signal(SIGSEGV, SIG_DFL); signal(SIGBUS, SIG_DFL);
      alarm(3); byte = *inert; (void)byte; _exit(99);
   }
   if (waitpid(child, &status, 0) != child || !WIFSIGNALED(status)
         || (WTERMSIG(status) != SIGSEGV && WTERMSIG(status) != SIGBUS))
      fail("inert body became readable after fill/refill");
   else ok("strict inert body faults after fill/refill");
#elif defined(_WIN32)
   MEMORY_BASIC_INFORMATION info;
   if (VirtualQuery(inert, &info, sizeof(info)) != sizeof(info)
         || info.State != MEM_RESERVE)
      fail("inert body retains committed Windows backing");
   else ok("inert body remains reserved without committed Windows backing");
#else
   (void)inert;
   printf("[skip] inert fault check: needs non-ASan strict Unix build\n");
#endif
}

static void inert_prefix_check(void)
{
   const char *path = "dt-prefix-inert.bin";
   size_t page = mempagesize(), n, i, total, before;
   uint8_t *ref = NULL, *live = NULL;
   const uint8_t *base;
   data_transfer_t *dt = NULL, *window = NULL;
   int interval;
   unsigned ticks = 0;
#ifdef DT_COUNT_IO
   uint64_t start_bytes = inert_io_bytes, refill_bytes, expected = 0;
#endif
   if (!page) page = 4096;
   n = page * 1024;
   ref = (uint8_t*)malloc(n);
   live = (uint8_t*)malloc(1024);
   if (!ref || !live) { fail("inert fixture allocation"); goto finish; }
   memset(live, 1, 1024);
   mkfile(path, ref, n);
   dt = data_transfer_open_prefix(path, 0);
   if (!dt) { fail("inert prefix open"); goto finish; }
   base = data_transfer_ptr(dt, &total);
   if (total != n) { fail("inert declared length"); goto finish; }
   data_transfer_iterate(dt, page * 16);
   before = data_transfer_avail(dt);
   /* Reverse insertion grows the index; duplicate and overlap must
    * preserve guards without relying on the implementation's rounding. */
   for (interval = 11; interval >= 0; interval--)
      inert_mark(dt, live, 1024, page,
            ((size_t)interval * 64 + 2) * page + 1,
            ((size_t)interval * 64 + 32) * page + 17);
   inert_mark(dt, live, 1024, page, (5 * 64 + 2) * page + 1,
         (5 * 64 + 32) * page + 17);
   inert_mark(dt, live, 1024, page, (5 * 64 + 16) * page + 3,
         (6 * 64 + 17) * page + 21);
   if (data_transfer_avail(dt) != before
         || data_transfer_prefix_inert(dt, 3, 2)
         || data_transfer_prefix_inert(dt, 0, n + 1)
         || data_transfer_prefix_inert(NULL, 0, 0)
         || data_transfer_failed(dt)) fail("inert registration changes prefix or accepts bad bounds");
   {
      int reads = 0;
      data_transfer_iterate_while(dt, 0, hook_stop_after, &reads);
      if (data_transfer_avail(dt) != before) fail("stopped hook still skips inert bytes");
   }
   while (!data_transfer_complete(dt) && !data_transfer_failed(dt) && ticks++ < 1024)
   {
      int reads = 1;
#ifdef DT_COUNT_IO
      uint64_t calls = inert_io_calls;
#endif
      data_transfer_iterate_while(dt, 0, hook_stop_after, &reads);
#ifdef DT_COUNT_IO
      if (inert_io_calls - calls > 1) fail("inert skip bypasses read budget hook");
#endif
   }
   if (!data_transfer_complete(dt) || data_transfer_avail(dt) != n)
   { fail("inert prefix does not complete"); goto finish; }
   for (i = 0; i < 1024; i++)
      if (live[i] && memcmp(base + i * page, ref + i * page, page))
      { fail("inert fill changed a live or boundary page"); goto finish; }
   inert_physical_check(base, live, 1024, page);
   if (data_transfer_reserve_supported()) inert_fault_check(base + (11 * 64 + 31) * page);
#ifdef DT_COUNT_IO
   if (data_transfer_reserve_supported() && (uintptr_t)base % page == 0)
   {
      for (i = 0; i < 1024; i++)
         if (live[i]) expected += page;
         else if (i * page < before)
            expected += before - i * page < page ? before - i * page : page;
      if (inert_io_bytes - start_bytes != expected) fail("inert fill read skipped bytes");
      else ok("inert fill reads only live bytes plus the prefix read before registration");
   }
   refill_bytes = inert_io_bytes;
#endif
#if defined(__linux__) && defined(MADV_HUGEPAGE)
   /* Exercise hosts that permit huge-page backing of writable holes.
    * The reservation is anonymous; this advice changes no file bytes. */
   if (data_transfer_reserve_supported())
      madvise((void*)base, n, MADV_HUGEPAGE);
#endif
   data_transfer_discard(dt, n - page);
   if (!data_transfer_refill(dt, 0)) { fail("inert refill"); goto finish; }
   for (i = 0; i < 1024; i++)
      if (live[i] && memcmp(base + i * page, ref + i * page, page))
      { fail("inert refill changed a live or boundary page"); goto finish; }
   inert_physical_check(base, live, 1024, page);
   if (data_transfer_reserve_supported()) inert_fault_check(base + (11 * 64 + 31) * page);
#ifdef DT_COUNT_IO
   if (data_transfer_reserve_supported() && (uintptr_t)base % page == 0)
   {
      expected = 0;
      for (i = 0; i < 1023; i++) if (live[i]) expected += page;
      if (inert_io_bytes - refill_bytes != expected) fail("inert refill read skipped bytes");
      else ok("inert refill never reads inert payloads");
   }
#endif
   ok("inert intervals: reverse/duplicate/overlap, late registration and refill retain all live pages");
   data_transfer_free(dt); dt = data_transfer_open_prefix(path, page * 512);
   if (!dt) { fail("inert capped open"); goto finish; }
   if (!data_transfer_prefix_inert(dt, page * 256 + 1, page * 600 + 17))
      fail("inert capped registration");
   while (!data_transfer_capped(dt) && !data_transfer_complete(dt) && !data_transfer_failed(dt))
      data_transfer_iterate(dt, page * 16);
   if (!data_transfer_capped(dt) || data_transfer_complete(dt)
         || data_transfer_avail(dt) != page * 512) fail("inert skip crosses logical cap");
   else ok("inert skip preserves cap terminal");
   data_transfer_free(dt); dt = data_transfer_open_prefix(path, 0);
   if (!dt) { fail("inert EOF open"); goto finish; }
   if (!data_transfer_prefix_inert(dt, page * 900 + 1, n)) fail("inert EOF registration");
   data_transfer_iterate(dt, 0);
   if (!data_transfer_complete(dt) || data_transfer_avail(dt) != n) fail("inert EOF does not complete");
   else ok("inert tail reaches EOF without changing completion semantics");
   window = data_transfer_open_window(path, page * 8);
   if (!window || data_transfer_prefix_inert(window, page * 16, page * 32))
      fail("window accepted prefix inert contract");
   else ok("window refuses prefix inert contract");
finish:
   data_transfer_free(window); data_transfer_free(dt);
   free(ref); free(live); remove(path);
}

int main(void)
{
   const char *path = "/tmp/dtprefix.bin";
   size_t n = 3u << 20;                 /* 3 MiB */
   uint8_t *ref = malloc(n);
   if (getenv("INERT_ONLY"))
   { free(ref); inert_prefix_check(); return bad; }
   mkfile(path, ref, n);

   /* 1. budgeted fill delivers the file, contents intact */
   {
      data_transfer_t *dt = data_transfer_open_prefix(path, 0);
      size_t total = 0; const uint8_t *base; int ticks = 0;
      if (!dt) { fail("open_prefix"); return 1; }
      base = data_transfer_ptr(dt, &total);
      if (total != n) fail("declared length");
      while (!data_transfer_complete(dt) && !data_transfer_failed(dt)
             && ticks < 100000)
      { data_transfer_iterate(dt, 65536); ticks++; }
      if (!data_transfer_complete(dt))         fail("never completed");
      else if (data_transfer_avail(dt) != n)   fail("avail != length");
      else if (memcmp(base, ref, n))           fail("contents differ");
      else ok("budgeted fill: whole file, contents intact");
      data_transfer_free(dt);
   }

   /* 2. discard then refill restores the released bytes */
   {
      data_transfer_t *dt = data_transfer_open_prefix(path, 0);
      size_t total = 0; const uint8_t *base = data_transfer_ptr(dt, &total);
      while (!data_transfer_complete(dt) && !data_transfer_failed(dt))
         data_transfer_iterate(dt, 0);
      data_transfer_discard(dt, 1u << 20);
      if (!data_transfer_refill(dt, 0))        fail("refill refused");
      else if (memcmp(base, ref, n))           fail("refill restored wrong bytes");
      else ok("discard then refill: bytes restored correctly");
      data_transfer_free(dt);
   }

   /* 3. commit_cap stops at the ceiling and reports capped, not complete */
   {
      size_t cap = 1u << 20;
      data_transfer_t *dt = data_transfer_open_prefix(path, cap);
      int ticks = 0;
      if (!dt) { fail("open_prefix with cap"); return 1; }
      while (!data_transfer_complete(dt) && !data_transfer_failed(dt)
             && !data_transfer_capped(dt) && ticks < 100000)
      { data_transfer_iterate(dt, 65536); ticks++; }
      if (!data_transfer_capped(dt))        fail("cap not reported");
      else if (data_transfer_complete(dt))  fail("capped and complete at once");
      else if (data_transfer_avail(dt) > cap) fail("filled past the cap");
      else ok("commit_cap: stops at the ceiling, distinct from complete");
      data_transfer_free(dt);
   }

   /* 4. a cap at or above the length still completes normally */
   {
      data_transfer_t *dt = data_transfer_open_prefix(path, n);
      while (!data_transfer_complete(dt) && !data_transfer_failed(dt)
             && !data_transfer_capped(dt))
         data_transfer_iterate(dt, 0);
      if (!data_transfer_complete(dt) || data_transfer_capped(dt))
         fail("cap == length should complete, not cap");
      else ok("cap equal to the length completes normally");
      data_transfer_free(dt);
   }

   /* 5. a vanished file fails rather than reporting a short success */
   {
      const char *gone = "/tmp/dtprefix_missing.bin";
      data_transfer_t *dt;
      remove(gone);
      dt = data_transfer_open_prefix(gone, 0);
      if (dt) { fail("opened a file that does not exist"); data_transfer_free(dt); }
      else ok("missing file refused at open");
   }

   free(ref); remove(path);

   /* 6. the arena's growth must not spin on a size it cannot reach */
   bad |= arena_overflow_check();

   /* 7. strict builds must fault on a look-back, not read zeros -
    *    both after the fill is over and, the case that caught a live
    *    bug, with the fill still advancing past the discard */
   bad |= strict_discard_check(0);
   bad |= strict_discard_check(1);

   /* 9. a recycled buffer must not leak the previous load */
   bad |= pool_reuse_check();

   /* 8. the continue hook pauses finely and resumes cleanly */
   bad |= continue_hook_check();
   inert_prefix_check();

   printf("%s\n", bad ? "FAILED" : "PASS");
   return bad;
}

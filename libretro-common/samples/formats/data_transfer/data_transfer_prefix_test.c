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
#if defined(__unix__) || defined(__APPLE__)
static int strict_discard_check(void)
{
#if !defined(DT_STRICT)
   printf("[skip] strict discard: build with -DDT_STRICT\n");
   return 0;
#elif defined(__SANITIZE_ADDRESS__)
   printf("[skip] strict discard: the sanitizer intercepts it\n");
   return 0;
#else
   const char *path = "/tmp/dtprefix_strict.bin";
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
      data_transfer_iterate(dt, 0);
      if (!data_transfer_complete(dt))
         _exit(4);
      base = data_transfer_ptr(dt, &len);
      data_transfer_discard(dt, 1u << 20);
      v = base[4096];             /* well below the discard frontier */
      _exit(42);                  /* reached only if it did not fault */
      (void)v;
   }
   waitpid(pid, &status, 0);
   remove(path);

   if (WIFEXITED(status) && WEXITSTATUS(status) == 42)
   {
      printf("[FAIL] a strict build still reads zeros below the "
             "discard frontier\n");
      return 1;
   }
   if (WIFEXITED(status) && WEXITSTATUS(status) >= 3
         && WEXITSTATUS(status) <= 4)
   {
      printf("[FAIL] the strict-discard child bailed at setup (%d)\n",
            WEXITSTATUS(status));
      return 1;
   }
   /* Anything else is the fault: a signal normally, or the sanitizer
    * ending the process its own way with its own exit code. */
   printf("[ok]   strict discard: a look-back below the frontier "
          "faults\n");
   return 0;
#endif
}
#else
static int strict_discard_check(void)
{
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

int main(void)
{
   const char *path = "/tmp/dtprefix.bin";
   size_t n = 3u << 20;                 /* 3 MiB */
   uint8_t *ref = malloc(n);
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

   /* 7. strict builds must fault on a look-back, not read zeros */
   bad |= strict_discard_check();

   /* 8. the continue hook pauses finely and resumes cleanly */
   bad |= continue_hook_check();

   printf("%s\n", bad ? "FAILED" : "PASS");
   return bad;
}

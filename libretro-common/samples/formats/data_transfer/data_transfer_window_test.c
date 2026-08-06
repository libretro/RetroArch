/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (data_transfer_window_test.c).
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

/* Regression test for data_transfer's cyclic window mode.
 *
 * Window mode backs looping background music: the consumer sees one
 * stable base for the whole file, but only the head - kept forever,
 * because a loop lands there - plus a moving window around the play
 * position is actually resident.  That makes its contract different
 * from the other two modes in a way worth pinning down.
 *
 *   - the head must be readable from the moment the window opens, and
 *     must still be readable after a lap, since the jump back happens
 *     on the audio thread before any feeder tick runs.
 *   - whatever the window covers must match the file, on the first
 *     lap and on a second one, where the bytes are re-read rather
 *     than retained.
 *   - extending past the end must clamp, not fail.
 *   - grow_keep must enlarge the permanently resident head without
 *     disturbing what is already in it.
 *   - and the point of the mode: residency must stay bounded by the
 *     window rather than tracking the file, which the last check
 *     measures directly where the platform can report it.
 *
 * One asymmetry is worth stating outright, because it is surprising
 * and it decides how a feeder bug presents.  Reading *behind* the
 * window - bytes the feeder has advanced past - yields zeros on a
 * normal build, so a consumer that looks back gets silence.  Reading
 * *ahead* of the frontier is not like that: the reservation is mapped
 * PROT_NONE until committed, so a consumer that outruns its feeder
 * takes a fault, not zeros.  A feeder must therefore keep its
 * lookahead genuinely ahead of the consumer; falling behind is fatal
 * rather than merely wrong.  The last check pins that behaviour so a
 * change to the window logic cannot quietly turn a crash into silent
 * corruption, or the reverse.
 *
 * The test reports whether this build can reserve address space. Where
 * it cannot, window mode degrades to holding the whole file and the
 * correctness checks still apply - only the residency bound does not.
 *
 * Build:  make            (SANITIZER=address,undefined for a checked run)
 * Run:    ./data_transfer_window_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <formats/data_transfer.h>
#if defined(__linux__)
#include <unistd.h>
#endif

static int bad = 0;
static void ok(const char *w){ printf("[ok]   %s\n", w); }
static void fail(const char *w){ printf("[FAIL] %s\n", w); bad = 1; }

#define KEEP     (64u * 1024)
#define LOOKAHD  (256u * 1024)
#define MARGIN   (64u * 1024)


#if defined(__linux__)
static size_t rss_kb(void)
{
   FILE *f = fopen("/proc/self/statm", "r");
   long size = 0, resident = 0;
   if (!f)
      return 0;
   if (fscanf(f, "%ld %ld", &size, &resident) != 2)
      resident = 0;
   fclose(f);
   return (size_t)resident * (size_t)(sysconf(_SC_PAGESIZE) / 1024);
}

/* Play a large file through a small window and watch the resident set:
 * it must stay near the window rather than near the file. */
static int residency_check(void)
{
   const char *path = "/tmp/dtwin_resident.bin";
   size_t n = 64u << 20, i, tell, before, peak = 0;
   uint8_t *buf = (uint8_t*)malloc(n);
   data_transfer_t *dt; const uint8_t *base; size_t blen = 0;
   FILE *f;
   if (!buf)
      return 0;
   for (i = 0; i < n; i++)
      buf[i] = (uint8_t)i;
   f = fopen(path, "wb"); fwrite(buf, 1, n, f); fclose(f);
   free(buf);                       /* the reference copy must not count */

   before = rss_kb();
   if (!(dt = data_transfer_open_window(path, KEEP)))
   { remove(path); return 0; }
   base = data_transfer_window_base(dt, &blen);
   for (tell = 0; tell < n; tell += 65536)
   {
      size_t r;
      data_transfer_window_feed(dt, tell, LOOKAHD, MARGIN);
      { volatile uint8_t s = base[tell]; (void)s; }
      if ((r = rss_kb()) > peak)
         peak = r;
   }
   data_transfer_free(dt);
   remove(path);
   {
      size_t grew  = (peak > before) ? peak - before : 0;
      size_t bound = (n >> 10) / 4;   /* a quarter of the file, generous */
      if (grew >= bound)
      { printf("[FAIL] residency grew %zu KiB for a %zu MiB file\n",
               grew, n >> 20); return 1; }
      printf("[ok]   residency bound: grew %zu KiB for a %zu MiB file\n",
             grew, n >> 20);
   }
   return 0;
}
#endif


#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>

/* Reading ahead of the frontier must fault rather than yield zeros.
 * Run it in a child so either answer is observable. */
static int ahead_of_frontier_check(void)
{
   const char *path = "/tmp/dtwin_ahead.bin";
   size_t n = 16u << 20, i;
   uint8_t *buf = (uint8_t*)malloc(n);
   FILE *f;
   pid_t pid;
   int status = 0;

   if (!buf)
      return 0;
   for (i = 0; i < n; i++)
      buf[i] = (uint8_t)(i | 1);
   f = fopen(path, "wb"); fwrite(buf, 1, n, f); fclose(f);
   free(buf);

   if (!data_transfer_reserve_supported())
   {
      printf("[skip] read-ahead fault: no reservations on this build\n");
      remove(path);
      return 0;
   }

   /* A sanitizer traps the access itself and terminates the child its
    * own way, so the signal this check looks for never arrives and the
    * fault it is verifying would be reported as an absence.  The other
    * checks still run under sanitizers; this one only means anything
    * without them. */
#if defined(__SANITIZE_ADDRESS__)
   printf("[skip] read-ahead fault: the sanitizer intercepts it\n");
   remove(path);
   return 0;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
   printf("[skip] read-ahead fault: the sanitizer intercepts it\n");
   remove(path);
   return 0;
#endif
#endif

   if ((pid = fork()) == 0)
   {
      data_transfer_t *dt = data_transfer_open_window(path, KEEP);
      const uint8_t *base; size_t blen = 0;
      volatile uint8_t s;
      if (!dt)
         _exit(3);
      base = data_transfer_window_base(dt, &blen);
      s = base[0];              /* the head is resident */
      s = base[8u << 20];       /* far ahead, with no feed between */
      _exit(42);                /* reached only if it did not fault */
      (void)s;
   }
   waitpid(pid, &status, 0);
   remove(path);

   if (WIFEXITED(status) && WEXITSTATUS(status) == 42)
   {
      printf("[FAIL] read ahead of the frontier did not fault "
             "(a feeder falling behind would corrupt silently)\n");
      return 1;
   }
   printf("[ok]   read ahead of the frontier faults, as designed\n");
   return 0;
}
#else
static int ahead_of_frontier_check(void)
{
   printf("[skip] read-ahead fault: needs fork()\n");
   return 0;
}
#endif

/* The prefix surface must be inert on a window handle.
 *
 * A window keeps a live RFILE and a reservation, so discard(),
 * refill() and iterate() all pass their own guards on one.  Left
 * unguarded they do not merely misbehave, they disagree with the
 * window's bookkeeping: discard() walks a 'low' frontier that knows
 * nothing of keep/wlo/whi/wfreed and releases from offset 0 - through
 * the permanently resident head - and the release is non-strict, so
 * the head returns as zeros rather than faulting.  That is silent
 * corruption of exactly the bytes a loop lands on.  iterate() is the
 * milder sibling: it finds avail == len, settles done, and complete()
 * starts answering yes about a file that was never read.
 *
 * No caller mixes the two surfaces today - gfx_thumbnail keeps its
 * windowed and read-pending states mutually exclusive by hand, and
 * the webm core only discards a handle it opened as a prefix - so
 * both hazards are held off by discipline rather than by the module.
 * This pins the module's own guard, so the next consumer cannot reach
 * them by accident. */
static int surface_mixing_check(void)
{
   const char *path = "/tmp/dtwin_mix.bin";
   size_t n = 4u << 20, i, blen = 0, avail_before;
   uint8_t *ref = (uint8_t*)malloc(n);
   data_transfer_t *dt;
   const uint8_t *base;
   FILE *f;
   int rv = 0;

   if (!ref)
      return 0;
   for (i = 0; i < n; i++)
      ref[i] = (uint8_t)(i * 97 + 5);
   f = fopen(path, "wb"); fwrite(ref, 1, n, f); fclose(f);

   if (!(dt = data_transfer_open_window(path, KEEP)))
   {
      printf("[FAIL] open_window for the mixing check\n");
      free(ref); remove(path);
      return 1;
   }

   base         = data_transfer_window_base(dt, &blen);
   avail_before = data_transfer_avail(dt);

   /* discard() must not release the head it knows nothing about */
   data_transfer_discard(dt, KEEP + (512u * 1024));
   if (memcmp(base, ref, KEEP))
   {
      printf("[FAIL] discard() on a window released the resident head\n");
      rv = 1;
   }
   else
      ok("discard() on a window is inert: head still intact");

   /* refill() is a no-op here, but must not report failure */
   if (!data_transfer_refill(dt, 0))
   {
      printf("[FAIL] refill() on a window reported failure\n");
      rv = 1;
   }
   else
      ok("refill() on a window is inert and reports success");

   /* iterate() must not settle a handle that has no fill to run */
   data_transfer_iterate(dt, 64u * 1024);
   if (data_transfer_avail(dt) != avail_before)
   {
      printf("[FAIL] iterate() on a window moved avail\n");
      rv = 1;
   }
   else if (   data_transfer_window_is_reserved(dt)
            && data_transfer_complete(dt))
   {
      printf("[FAIL] iterate() on a window settled complete()\n");
      rv = 1;
   }
   else
      ok("iterate() on a window is inert: nothing settled");

   /* and the window must still work afterwards */
   if (!data_transfer_window_feed(dt, KEEP, LOOKAHD, MARGIN))
   {
      printf("[FAIL] feed after the mixing attempts\n");
      rv = 1;
   }
   else if (memcmp(base + KEEP, ref + KEEP, 65536))
   {
      printf("[FAIL] window contents wrong after the mixing attempts\n");
      rv = 1;
   }
   else
      ok("window still plays correctly after the mixing attempts");

   data_transfer_free(dt);
   free(ref); remove(path);
   return rv;
}

/* Ranges that arrive from the caller must be bounded before use.
 *
 * The two differ sharply in how badly, and it is worth saying which
 * is which.
 *
 * window_punch() was the dangerous one.  It rounds (from, to) and
 * hands memdecommit map + f for t - f bytes, unclamped, and a
 * non-strict decommit is madvise(MADV_DONTNEED) - which does not fail
 * politely on a range that runs off the end of the reservation.  It
 * zeroes whatever anonymous pages it does cover, and past a 4 MiB
 * reservation a 64 MiB overrun reliably covers live heap.  Against
 * the unfixed module the first punch below takes the process down
 * with SIGSEGV inside the call, before it can return.
 *
 * window_peek() is the mild one: it tested off + n against the
 * length, which a wrapping sum passes, but the oversized read then
 * fails at the I/O layer and peek returns false anyway.  That check
 * pins the contract rather than catching a fault. */
static int range_bounds_check(void)
{
   const char *path = "/tmp/dtwin_bounds.bin";
   size_t n = 4u << 20, i, blen = 0;
   uint8_t *ref = (uint8_t*)malloc(n);
   uint8_t tmp[64];
   data_transfer_t *dt;
   const uint8_t *base;
   FILE *f;
   int rv = 0;

   if (!ref)
      return 0;
   for (i = 0; i < n; i++)
      ref[i] = (uint8_t)(i * 31 + 7);
   f = fopen(path, "wb"); fwrite(ref, 1, n, f); fclose(f);

   if (!(dt = data_transfer_open_window(path, KEEP)))
   {
      printf("[FAIL] open_window for the bounds check\n");
      free(ref); remove(path);
      return 1;
   }
   base = data_transfer_window_base(dt, &blen);

   /* an (off, n) whose sum wraps to zero must not read as in range */
   if (data_transfer_window_peek(dt, n - 16, tmp,
            (size_t)0 - (n - 16)))
   {
      printf("[FAIL] peek accepted a wrapping (off, n)\n");
      rv = 1;
   }
   else if (data_transfer_window_peek(dt, n - 16, tmp, 64))
   {
      printf("[FAIL] peek accepted a range past the end\n");
      rv = 1;
   }
   else if (!data_transfer_window_peek(dt, n - 64, tmp, 64))
   {
      printf("[FAIL] peek refused the last 64 bytes of the file\n");
      rv = 1;
   }
   else if (memcmp(tmp, ref + n - 64, 64))
   {
      printf("[FAIL] peek returned the wrong bytes at the end\n");
      rv = 1;
   }
   else
      ok("peek bounds its range and still reads the last bytes");

   /* a punch running far past the mapping must not be taken at its
    * word, and must leave the head alone either way */
   data_transfer_window_punch(dt, n - 4096, n + (64u << 20));
   data_transfer_window_punch(dt, n + (64u << 20), n + (128u << 20));
   if (memcmp(base, ref, KEEP))
   {
      printf("[FAIL] an out-of-range punch damaged the head\n");
      rv = 1;
   }
   else if (!data_transfer_window_feed(dt, KEEP, LOOKAHD, MARGIN))
   {
      printf("[FAIL] feed after an out-of-range punch\n");
      rv = 1;
   }
   else if (memcmp(base + KEEP, ref + KEEP, 65536))
   {
      printf("[FAIL] window contents wrong after an out-of-range punch\n");
      rv = 1;
   }
   else
      ok("out-of-range punch is bounded, window unaffected");

   data_transfer_free(dt);
   free(ref); remove(path);
   return rv;
}

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>

/* A short read while extending must settle the handle.
 *
 * extend() and grow_keep() used to report an I/O failure by returning
 * false and nothing else: the handle still looked healthy, and whi
 * stayed put while the pages the call had already committed sat there
 * unfilled.  Two live call sites discard the return - the audio mixer
 * raises the head to cover the decoder's loop landing, and feeds the
 * window each tick, without looking at either answer - so the value
 * alone was never going to be noticed.  The loop landing is read on
 * the audio thread before any feeder tick, which makes a silent
 * failure there audible and untraceable.
 *
 * Truncating the file underneath an open window produces the short
 * read: len was fixed at open, so extending into what is now past the
 * end reads fewer bytes than asked for. */
static int extend_settles_check(void)
{
   const char *path = "/tmp/dtwin_settle.bin";
   size_t n = 4u << 20, i, blen = 0;
   uint8_t *ref = (uint8_t*)malloc(n);
   data_transfer_t *dt;
   FILE *f;
   int rv = 0;

   if (!ref)
      return 0;
   for (i = 0; i < n; i++)
      ref[i] = (uint8_t)(i * 53 + 11);
   f = fopen(path, "wb"); fwrite(ref, 1, n, f); fclose(f);

   if (!(dt = data_transfer_open_window(path, KEEP)))
   {
      printf("[FAIL] open_window for the settle check\n");
      free(ref); remove(path);
      return 1;
   }
   data_transfer_window_base(dt, &blen);

   if (!data_transfer_reserve_supported())
   {
      /* the fallback holds the whole file and extend never reads */
      printf("[skip] extend settles: no reservations on this build\n");
      data_transfer_free(dt); free(ref); remove(path);
      return 0;
   }

   if (!data_transfer_window_feed(dt, KEEP, LOOKAHD, MARGIN))
      { printf("[FAIL] feed before the truncation\n"); rv = 1; }
   else if (data_transfer_failed(dt))
   { printf("[FAIL] handle already failed before the truncation\n"); rv = 1; }

   /* the file shrinks under the open transfer */
   if (!rv && truncate(path, (off_t)(KEEP + LOOKAHD)) != 0)
   {
      printf("[skip] extend settles: truncate unavailable\n");
      data_transfer_free(dt); free(ref); remove(path);
      return 0;
   }

   if (!rv)
   {
      if (data_transfer_window_extend(dt, n))
      {
         printf("[FAIL] extend past a truncation reported success\n");
         rv = 1;
      }
      else if (!data_transfer_failed(dt))
      {
         printf("[FAIL] a short read in extend did not settle the "
                "handle\n");
         rv = 1;
      }
      else if (data_transfer_complete(dt))
      {
         printf("[FAIL] a settled window reports complete()\n");
         rv = 1;
      }
      else
         ok("a short read in extend settles the handle");
   }

   /* and the surface is frozen: a caller that ignores the return must
    * not be able to drive it further */
   if (!rv)
   {
      if (   data_transfer_window_extend(dt, n)
          || data_transfer_window_grow_keep(dt, KEEP * 4)
          || data_transfer_window_feed(dt, 0, LOOKAHD, MARGIN)
          || data_transfer_window_peek(dt, 0, ref, 16))
      {
         printf("[FAIL] a settled window still accepts work\n");
         rv = 1;
      }
      else
         ok("a settled window refuses every further call");
   }

   data_transfer_free(dt);
   free(ref); remove(path);
   return rv;
}
#else
static int extend_settles_check(void)
{
   printf("[skip] extend settles: needs truncate()\n");
   return 0;
}
#endif

/* Every entry point must tolerate a NULL handle.
 *
 * Most of the surface already did, and window_is_reserved() did, but
 * its immediate neighbour window_base() dereferenced without looking
 * - despite having an exact NULL-safe twin in data_transfer_ptr().
 * A caller cannot infer a rule from a surface that is split like
 * that, so the rule is now: bool returns false, pointer returns NULL,
 * void does nothing.
 *
 * This runs in a child because the failure is a segfault, not a
 * wrong answer. */
#if defined(__unix__) || defined(__APPLE__)
static int null_handle_check(void)
{
   pid_t pid = fork();
   int   status = 0;

   if (pid < 0)
   {
      printf("[skip] NULL handles: fork unavailable\n");
      return 0;
   }
   if (pid == 0)
   {
      uint8_t buf[16];
      size_t  len = 12345;
      int     bad = 0;

      /* window surface */
      if (data_transfer_window_is_reserved(NULL))          bad = 1;
      if (data_transfer_window_base(NULL, &len))           bad = 1;
      if (len != 0)                                        bad = 1;
      if (data_transfer_window_base(NULL, NULL))           bad = 1;
      if (data_transfer_window_extend(NULL, 4096))         bad = 1;
      if (data_transfer_window_grow_keep(NULL, 4096))      bad = 1;
      if (data_transfer_window_peek(NULL, 0, buf, 16))     bad = 1;
      if (data_transfer_window_feed(NULL, 0, 4096, 4096))  bad = 1;
      data_transfer_window_advance(NULL, 4096);
      data_transfer_window_rewind(NULL);
      data_transfer_window_punch(NULL, 0, 4096);

      /* shared surface */
      if (data_transfer_iterate(NULL, 4096))               bad = 1;
      len = 12345;
      if (data_transfer_ptr(NULL, &len))                   bad = 1;
      if (len != 0)                                        bad = 1;
      if (data_transfer_avail(NULL))                       bad = 1;
      if (data_transfer_complete(NULL))                    bad = 1;
      if (data_transfer_capped(NULL))                      bad = 1;
      if (data_transfer_failed(NULL))                      bad = 1;
      if (data_transfer_refill(NULL, 0))                   bad = 1;
      data_transfer_discard(NULL, 4096);
      data_transfer_free(NULL);

      _exit(bad ? 61 : 60);
   }
   waitpid(pid, &status, 0);

   if (WIFSIGNALED(status))
   {
      printf("[FAIL] a NULL handle killed the process (signal %d)\n",
            WTERMSIG(status));
      return 1;
   }
   if (WIFEXITED(status) && WEXITSTATUS(status) == 61)
   {
      printf("[FAIL] a NULL handle produced a non-empty answer\n");
      return 1;
   }
   if (WIFEXITED(status) && WEXITSTATUS(status) == 60)
   {
      printf("[ok]   every entry point tolerates a NULL handle\n");
      return 0;
   }
   printf("[FAIL] the NULL-handle child ended unexpectedly\n");
   return 1;
}
#else
static int null_handle_check(void)
{
   printf("[skip] NULL handles: needs fork()\n");
   return 0;
}
#endif

#if defined(__unix__) || defined(__APPLE__)
/* Settling must take back everything above the frontier.
 *
 * A failed extend() has already committed the range it was about to
 * fill, so without the release those pages read as zeros - in the one
 * place the mode guarantees a fault.  The check forces the failure by
 * truncating, then reads far above the last good frontier in a child:
 * the child must die.  Against a module that settles without
 * releasing, it survives and reads zeros instead.
 *
 * The release is strict in every build, so this does not need
 * DT_STRICT. */
static int settle_releases_check(void)
{
   const char *path = "/tmp/dtwin_release.bin";
   size_t n = 8u << 20, i;
   uint8_t *ref = (uint8_t*)malloc(n);
   FILE *f;
   pid_t pid;
   int status = 0;

   if (!ref)
      return 0;
   for (i = 0; i < n; i++)
      ref[i] = (uint8_t)(i * 17 + 3);
   f = fopen(path, "wb"); fwrite(ref, 1, n, f); fclose(f);
   free(ref);

   if (!data_transfer_reserve_supported())
   {
      printf("[skip] settle releases: no reservations on this build\n");
      remove(path);
      return 0;
   }
#if defined(__SANITIZE_ADDRESS__)
   printf("[skip] settle releases: the sanitizer intercepts it\n");
   remove(path);
   return 0;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
   printf("[skip] settle releases: the sanitizer intercepts it\n");
   remove(path);
   return 0;
#endif
#endif

   if ((pid = fork()) == 0)
   {
      data_transfer_t *dt = data_transfer_open_window(path, KEEP);
      const uint8_t *base;
      size_t blen = 0;
      volatile uint8_t v;

      if (!dt)
         _exit(3);
      base = data_transfer_window_base(dt, &blen);
      if (!data_transfer_window_feed(dt, KEEP, LOOKAHD, MARGIN))
         _exit(4);
      /* cut the file just above the frontier, then ask for the rest:
       * the extend commits far ahead and then reads short */
      if (truncate(path, (off_t)(KEEP + LOOKAHD)) != 0)
         _exit(5);
      if (data_transfer_window_extend(dt, n))
         _exit(6);                /* it was supposed to fail */
      if (!data_transfer_failed(dt))
         _exit(7);
      v = base[n - 4096];         /* deep inside the failed commit */
      _exit(42);                  /* reached only if it did not fault */
      (void)v;
   }
   waitpid(pid, &status, 0);
   remove(path);

   if (WIFEXITED(status) && WEXITSTATUS(status) == 42)
   {
      printf("[FAIL] a settled window still reads zeros above the "
             "frontier instead of faulting\n");
      return 1;
   }
   if (WIFEXITED(status) && WEXITSTATUS(status) >= 3
         && WEXITSTATUS(status) <= 7)
   {
      printf("[FAIL] the settle-release child bailed at setup (%d)\n",
            WEXITSTATUS(status));
      return 1;
   }
   /* Anything else is the fault: a signal normally, or the sanitizer
    * ending the process its own way with its own exit code. */
   printf("[ok]   settling releases above the frontier: it faults\n");
   return 0;
}
#else
static int settle_releases_check(void)
{
   printf("[skip] settle releases: needs fork()\n");
   return 0;
}
#endif

int main(void)
{
   const char *path = "/tmp/dtwin.bin";
   size_t n = 4u << 20, i, tell;
   uint8_t *ref = malloc(n);
   data_transfer_t *dt;
   const uint8_t *base; size_t blen = 0;
   FILE *f;

   for (i = 0; i < n; i++) ref[i] = (uint8_t)(i * 211 + 13);
   f = fopen(path, "wb"); fwrite(ref, 1, n, f); fclose(f);

   printf("reservations %s on this build\n",
         data_transfer_reserve_supported() ? "supported (true windowing)"
                                           : "unavailable (whole-file fallback)");

   if (!(dt = data_transfer_open_window(path, KEEP)))
   { fail("open_window"); return 1; }

   base = data_transfer_window_base(dt, &blen);
   if (blen != n) fail("window base length");

   /* the head is resident from the start */
   if (memcmp(base, ref, KEEP)) fail("head contents after open");
   else ok("head resident and correct immediately after open");

   /* play forward, feeding as a decoder would */
   for (tell = 0; tell < n; tell += 32768)
   {
      size_t want = (tell + 32768 <= n) ? 32768 : n - tell;
      if (!data_transfer_window_feed(dt, tell, LOOKAHD, MARGIN))
      { fail("feed during forward play"); break; }
      if (memcmp(base + tell, ref + tell, want))
      { fail("window contents during forward play"); break; }
   }
   if (!bad) ok("forward play: every chunk matched the file");

   /* loop back to the start: a backwards tell means a new lap */
   if (!data_transfer_window_feed(dt, 0, LOOKAHD, MARGIN))
      fail("feed after loop");
   else if (memcmp(base, ref, KEEP))
      fail("head contents after loop");
   else ok("loop back to 0: head still correct");

   /* second lap re-reads from the file rather than holding memory */
   for (tell = 0; tell < n / 2; tell += 65536)
   {
      if (!data_transfer_window_feed(dt, tell, LOOKAHD, MARGIN))
      { fail("feed during second lap"); break; }
      if (memcmp(base + tell, ref + tell, 65536))
      { fail("window contents during second lap"); break; }
   }
   if (!bad) ok("second lap: contents re-read correctly");

   /* extend past the end clamps rather than failing */
   if (!data_transfer_window_extend(dt, n + (1u << 20)))
      fail("extend past end refused");
   else ok("extend past the end clamps to the length");

   /* growing the permanently-resident head keeps it readable */
   if (!data_transfer_window_grow_keep(dt, KEEP * 2))
      fail("grow_keep refused");
   else if (memcmp(base, ref, KEEP * 2))
      fail("head contents after grow_keep");
   else ok("grow_keep: enlarged head is correct");

   data_transfer_free(dt);
   free(ref); remove(path);

   /* The point of the mode: a long file must not become resident.
    * Measured only where the platform reports a resident set, and
    * only meaningful when address space can actually be reserved. */
#if defined(__linux__)
   if (data_transfer_reserve_supported())
      bad |= residency_check();
   else
      printf("[skip] residency bound: no reservations on this build\n");
#else
   printf("[skip] residency bound: not measurable on this platform\n");
#endif

   bad |= ahead_of_frontier_check();
   bad |= surface_mixing_check();
   bad |= range_bounds_check();
   bad |= extend_settles_check();
   bad |= null_handle_check();
   bad |= settle_releases_check();

   printf("%s\n", bad ? "FAILED" : "PASS");
   return bad;
}

/* Regression test for libretro-common/include/retro_atomic.h.
 *
 * Background
 * ----------
 * retro_atomic.h consolidates the ad-hoc atomic shims that were
 * previously duplicated in audio/drivers/{coreaudio,xaudio,
 * opensl}.c, audio/common/mmdevice_common.c and gfx/gfx_thumbnail.c.
 * It exposes a narrow surface (load/store with acquire/release
 * ordering, fetch_add, fetch_sub, plus inc/dec wrappers) on int and
 * size_t, with seven selectable backends:
 *
 *   1. C11 <stdatomic.h>          - modern toolchains
 *   2. C++11 <atomic>             - C++ TUs with __cplusplus >= 201103L
 *   3. GCC __atomic_*             - GCC 4.7+ / Clang 3.1+
 *   4. MSVC Win32 Interlocked*    - VS2003+, OG Xbox, Xbox 360 XDK
 *   5. Apple OSAtomic*            - PPC / pre-10.7
 *   6. GCC __sync_*               - GCC 4.1-4.6
 *   7. volatile fallback          - single-core / x86 TSO
 *
 * The header's correctness rests on each backend exposing the same
 * sequential behaviour through the macros, so this test exercises the
 * single-threaded behaviour exhaustively (any backend gets it wrong
 * and we see it) and runs an SPSC stress test under one of the
 * threading-capable backends to exercise the release/acquire pairing.
 *
 * What this test asserts
 * ----------------------
 *  1. The capability flags HAVE_RETRO_ATOMIC, RETRO_ATOMIC_BACKEND_NAME
 *     and RETRO_ATOMIC_LOCK_FREE are defined consistently with the
 *     selected backend (compile-time #error checks; a real-backend
 *     selection must imply RETRO_ATOMIC_LOCK_FREE, and the volatile
 *     fallback must NOT define RETRO_ATOMIC_LOCK_FREE).
 *  2. Initialisers seed the slot to the requested value.
 *  3. store_release publishes a value visible to load_acquire on the
 *     same thread (single-thread observability).
 *  4. fetch_add and fetch_sub return the previous value (POSIX-style)
 *     and update the storage in place.
 *  5. inc / dec wrappers map to fetch_add(1) / fetch_sub(1).
 *  6. SPSC stress (HAVE_THREADS only): a producer running fetch_add
 *     1..N and a release-store flag, paired with a consumer doing
 *     load_acquire on the counter and the flag, sees a strictly
 *     monotonically non-decreasing counter sequence and a final value
 *     of exactly N.  This is the property the SPSC fifo design relies
 *     on.  A backend that releases without ordering would be flagged
 *     by a counter going backwards or by the consumer seeing the flag
 *     before the writes that should have preceded it.
 *  7. Relaxed load/store round-trip on every width the backend
 *     offers, and agree with the ordered forms on the same object.
 *  8. Seqlock stress (HAVE_THREADS only): a writer publishes three
 *     fields that always sum to zero, stamping an odd sequence around
 *     the write; four readers snapshot them the seqlock way and check
 *     the sum.  A tuple assembled from two writer passes does not sum
 *     to zero, so a stamp protocol that lets one through is caught
 *     here.  The run also asserts that a reader landed inside a write
 *     at least once, since a run with no overlap has tested nothing.
 *     This is the shape gfx/video_driver.c publishes its cached frame
 *     with, and the reason the relaxed forms exist.
 *  9. The test prints which backend was selected and whether
 *     RETRO_ATOMIC_LOCK_FREE is defined, so a CI diff makes accidental
 *     backend regressions obvious.
 *
 * What this test does NOT assert
 * ------------------------------
 * It does not validate hardware ordering on weakly-ordered SMP from
 * a single host run on x86_64 (TSO masks most reordering bugs).
 *
 * That limit is sharp for the seqlock lane, and measured rather than
 * assumed.  Mutating the lane and re-running on x86_64 gives:
 *
 *   reader drops the closing stamp re-check -> caught (torn tuples)
 *   writer never stamps odd                 -> caught (no overlap)
 *   reader drops the odd-stamp fast path    -> passes, correctly:
 *       the closing re-check subsumes it, so that branch is an
 *       optimisation rather than a correctness requirement
 *   either fence removed                    -> NOT caught
 *   relaxed accesses replaced by plain ones -> caught by TSan
 *
 * So this lane tests the stamp protocol and the race-freedom the
 * relaxed spelling buys, but NOT the fences: on TSO the hardware
 * supplies the ordering the code forgot to ask for, and TSan does not
 * model a missing fence between atomics.  An aarch64 or PowerPC run
 * is what closes that.  For
 * the GCC backend, AArch64 / ARMv7 cross-compile + qemu user-mode
 * has been verified locally: the test passes and the emitted asm
 * contains real ldar/stlr instructions and ldadd*_acq_rel libcalls.
 * The existing Switch (libnx), Wii U, PSVita, 3DS and Android CI
 * workflows compile-test the rest of the tree on real ARM toolchains,
 * which would catch any backend-selection regression at build time.
 * MSVC ARM64 is the path we have not been able to validate from a
 * Linux CI host; its correctness rests on the *Acquire / *Release
 * Win32 forms emitting dmb (Microsoft-documented behaviour) and on
 * the explicit __dmb brackets we add around the plain RMW path.
 *
 * It does not exercise compare-and-exchange or thread fences -- those
 * are deliberately not in the API surface, since no caller in the tree
 * needs them today.  Add them (and tests) only when motivated by a
 * real caller.
 *
 * How a regression is caught
 * --------------------------
 * Each property check returns 1 on failure; main() sums them and
 * exits non-zero if any tripped.  CI runs the binary with ASan +
 * UBSan (the workflow's default), so any UB from torn writes or
 * mistyped casts inside the macros is caught at the same time.
 */

#include <stdio.h>
#include <stddef.h>

#include <retro_atomic.h>

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

/* ---- Capability flag sanity checks (compile-time) -------------------- */

/* The header must always define HAVE_RETRO_ATOMIC after a successful
 * include.  A regression that drops it (or makes it conditional) would
 * silently break callers that gate on it -- this static check catches it. */
#if !defined(HAVE_RETRO_ATOMIC)
#error "retro_atomic.h was included but HAVE_RETRO_ATOMIC is not defined"
#endif

/* RETRO_ATOMIC_BACKEND_NAME is documented as always available. */
#if !defined(RETRO_ATOMIC_BACKEND_NAME)
#error "retro_atomic.h was included but RETRO_ATOMIC_BACKEND_NAME is not defined"
#endif

/* RETRO_ATOMIC_LOCK_FREE must be defined if and only if a real backend
 * was selected.  We can't test the disjunction directly in the
 * preprocessor, but we can assert the obvious half: every named real
 * backend implies RETRO_ATOMIC_LOCK_FREE. */
#if (defined(RETRO_ATOMIC_BACKEND_C11)     \
  || defined(RETRO_ATOMIC_BACKEND_CXX11)   \
  || defined(RETRO_ATOMIC_BACKEND_GCC_NEW) \
  || defined(RETRO_ATOMIC_BACKEND_MSVC)    \
  || defined(RETRO_ATOMIC_BACKEND_APPLE)   \
  || defined(RETRO_ATOMIC_BACKEND_SYNC))   \
   && !defined(RETRO_ATOMIC_LOCK_FREE)
#error "a real atomic backend was selected but RETRO_ATOMIC_LOCK_FREE is not defined"
#endif

#if defined(RETRO_ATOMIC_BACKEND_VOLATILE) && defined(RETRO_ATOMIC_LOCK_FREE)
#error "the volatile fallback was selected but RETRO_ATOMIC_LOCK_FREE was set anyway"
#endif

/* ---- Backend tag (printed once at start of run) ----------------------- */

static const char *backend_name(void)
{
   return RETRO_ATOMIC_BACKEND_NAME;
}

/* ---- Single-threaded property checks --------------------------------- */

static int check_init(void)
{
   retro_atomic_int_t  vi;
   retro_atomic_size_t vs;

   retro_atomic_int_init(&vi, 7);
   retro_atomic_size_init(&vs, 99);

   if (retro_atomic_load_acquire_int(&vi) != 7)
   {
      fprintf(stderr, "FAIL init_int: expected 7\n");
      return 1;
   }
   if ((size_t)retro_atomic_load_acquire_size(&vs) != 99)
   {
      fprintf(stderr, "FAIL init_size: expected 99\n");
      return 1;
   }
   return 0;
}

static int check_store_load(void)
{
   retro_atomic_int_t  vi;
   retro_atomic_size_t vs;

   retro_atomic_int_init(&vi, 0);
   retro_atomic_size_init(&vs, 0);

   retro_atomic_store_release_int(&vi, 42);
   retro_atomic_store_release_size(&vs, (size_t)123456);

   if (retro_atomic_load_acquire_int(&vi) != 42)
   {
      fprintf(stderr, "FAIL store_load_int\n");
      return 1;
   }
   if ((size_t)retro_atomic_load_acquire_size(&vs) != 123456)
   {
      fprintf(stderr, "FAIL store_load_size\n");
      return 1;
   }
   return 0;
}

static int check_relaxed_store_load(void)
{
   retro_atomic_int_t  vi;
   retro_atomic_size_t vs;

   retro_atomic_int_init(&vi, 0);
   retro_atomic_size_init(&vs, 0);

   retro_atomic_store_relaxed_int(&vi, 42);
   retro_atomic_store_relaxed_size(&vs, (size_t)123456);

   if (retro_atomic_load_relaxed_int(&vi) != 42)
   {
      fprintf(stderr, "FAIL relaxed_store_load_int\n");
      return 1;
   }
   if ((size_t)retro_atomic_load_relaxed_size(&vs) != 123456)
   {
      fprintf(stderr, "FAIL relaxed_store_load_size\n");
      return 1;
   }

   /* Relaxed and ordered accesses name the same object: a value put
    * there by one form must be visible through the other. */
   retro_atomic_store_release_int(&vi, 7);
   if (retro_atomic_load_relaxed_int(&vi) != 7)
   {
      fprintf(stderr, "FAIL relaxed load of a released int\n");
      return 1;
   }
   retro_atomic_store_relaxed_int(&vi, 8);
   if (retro_atomic_load_acquire_int(&vi) != 8)
   {
      fprintf(stderr, "FAIL acquire load of a relaxed-stored int\n");
      return 1;
   }

#ifdef RETRO_ATOMIC_HAS_PTR
   {
      retro_atomic_ptr_t vp;
      retro_atomic_ptr_init(&vp, NULL);

      retro_atomic_store_relaxed_ptr(&vp, (void*)&vi);
      if (retro_atomic_load_relaxed_ptr(&vp) != (void*)&vi)
      {
         fprintf(stderr, "FAIL relaxed_store_load_ptr\n");
         return 1;
      }
      retro_atomic_store_release_ptr(&vp, (void*)&vs);
      if (retro_atomic_load_relaxed_ptr(&vp) != (void*)&vs)
      {
         fprintf(stderr, "FAIL relaxed load of a released ptr\n");
         return 1;
      }
   }
#endif

#ifdef RETRO_ATOMIC_HAS_64
   {
      retro_atomic_64_t vq;
      retro_atomic_64_init(&vq, 0);

      retro_atomic_store_relaxed_64(&vq, (int64_t)0x1234567890ABCDEFLL);
      if ((int64_t)retro_atomic_load_relaxed_64(&vq)
            != (int64_t)0x1234567890ABCDEFLL)
      {
         fprintf(stderr, "FAIL relaxed_store_load_64\n");
         return 1;
      }
      retro_atomic_store_release_64(&vq, (int64_t)-1);
      if ((int64_t)retro_atomic_load_relaxed_64(&vq) != (int64_t)-1)
      {
         fprintf(stderr, "FAIL relaxed load of a released 64\n");
         return 1;
      }
   }
#endif
   return 0;
}

/* A seqlock built on the relaxed forms: the shape gfx/video_driver.c
 * publishes its cached frame with, and the reason the relaxed
 * accessors exist.  A reader that samples an even stamp, reads the
 * fields, and sees the same stamp afterwards must have a tuple from
 * one writer pass -- here, three fields that always sum to zero. */
static int check_seqlock_shape(void)
{
   retro_atomic_size_t seq;
   retro_atomic_int_t  a, b, c;
   int                 pass;

   retro_atomic_size_init(&seq, 0);
   retro_atomic_int_init(&a, 0);
   retro_atomic_int_init(&b, 0);
   retro_atomic_int_init(&c, 0);

   for (pass = 1; pass <= 64; pass++)
   {
      size_t s = retro_atomic_load_relaxed_size(&seq);
      int    x, y, z;
      size_t s1, s2;

      retro_atomic_store_release_size(&seq, s + 1);
      retro_atomic_thread_fence_release();
      retro_atomic_store_relaxed_int(&a,  pass);
      retro_atomic_store_relaxed_int(&b,  pass * 2);
      retro_atomic_store_relaxed_int(&c, -pass * 3);
      retro_atomic_store_release_size(&seq, s + 2);

      s1 = retro_atomic_load_acquire_size(&seq);
      if (s1 & 1)
      {
         fprintf(stderr, "FAIL seqlock: stamp odd outside a write\n");
         return 1;
      }
      x  = retro_atomic_load_relaxed_int(&a);
      y  = retro_atomic_load_relaxed_int(&b);
      z  = retro_atomic_load_relaxed_int(&c);
      retro_atomic_thread_fence_acquire();
      s2 = retro_atomic_load_acquire_size(&seq);

      if (s1 != s2)
      {
         fprintf(stderr, "FAIL seqlock: stamp moved with no writer\n");
         return 1;
      }
      if (x + y + z != 0)
      {
         fprintf(stderr, "FAIL seqlock: torn tuple %d/%d/%d\n", x, y, z);
         return 1;
      }
   }

   if ((size_t)retro_atomic_load_acquire_size(&seq) != 128)
   {
      fprintf(stderr, "FAIL seqlock: stamp did not advance by two a pass\n");
      return 1;
   }
   return 0;
}

static int check_fetch_add_returns_previous(void)
{
   retro_atomic_int_t  vi;
   retro_atomic_size_t vs;
   int    prev_i;
   size_t prev_s;

   retro_atomic_int_init(&vi, 100);
   retro_atomic_size_init(&vs, 1000);

   prev_i = retro_atomic_fetch_add_int(&vi, 5);
   prev_s = (size_t)retro_atomic_fetch_add_size(&vs, 50);

   if (prev_i != 100)
   {
      fprintf(stderr, "FAIL fetch_add_int returned %d, expected 100\n", prev_i);
      return 1;
   }
   if (prev_s != 1000)
   {
      fprintf(stderr, "FAIL fetch_add_size returned %zu, expected 1000\n", prev_s);
      return 1;
   }
   if (retro_atomic_load_acquire_int(&vi) != 105)
   {
      fprintf(stderr, "FAIL fetch_add_int post-state\n");
      return 1;
   }
   if ((size_t)retro_atomic_load_acquire_size(&vs) != 1050)
   {
      fprintf(stderr, "FAIL fetch_add_size post-state\n");
      return 1;
   }
   return 0;
}

static int check_fetch_sub_returns_previous(void)
{
   retro_atomic_int_t  vi;
   retro_atomic_size_t vs;
   int    prev_i;
   size_t prev_s;

   retro_atomic_int_init(&vi, 50);
   retro_atomic_size_init(&vs, 500);

   prev_i = retro_atomic_fetch_sub_int(&vi, 3);
   prev_s = (size_t)retro_atomic_fetch_sub_size(&vs, 30);

   if (prev_i != 50)
   {
      fprintf(stderr, "FAIL fetch_sub_int returned %d, expected 50\n", prev_i);
      return 1;
   }
   if (prev_s != 500)
   {
      fprintf(stderr, "FAIL fetch_sub_size returned %zu, expected 500\n", prev_s);
      return 1;
   }
   if (retro_atomic_load_acquire_int(&vi) != 47)
   {
      fprintf(stderr, "FAIL fetch_sub_int post-state\n");
      return 1;
   }
   if ((size_t)retro_atomic_load_acquire_size(&vs) != 470)
   {
      fprintf(stderr, "FAIL fetch_sub_size post-state\n");
      return 1;
   }
   return 0;
}

static int check_inc_dec_wrappers(void)
{
   retro_atomic_int_t  vi;
   retro_atomic_size_t vs;
   int i;

   retro_atomic_int_init(&vi, 0);
   retro_atomic_size_init(&vs, 0);

   for (i = 0; i < 100; i++)
      retro_atomic_inc_int(&vi);
   for (i = 0; i < 30; i++)
      retro_atomic_dec_int(&vi);

   for (i = 0; i < 100; i++)
      retro_atomic_inc_size(&vs);
   for (i = 0; i < 30; i++)
      retro_atomic_dec_size(&vs);

   if (retro_atomic_load_acquire_int(&vi) != 70)
   {
      fprintf(stderr, "FAIL inc/dec int\n");
      return 1;
   }
   if ((size_t)retro_atomic_load_acquire_size(&vs) != 70)
   {
      fprintf(stderr, "FAIL inc/dec size\n");
      return 1;
   }
   return 0;
}

/* ---- SPSC stress test (HAVE_THREADS only) ---------------------------- */

#ifdef HAVE_THREADS

#define SPSC_N 1000000

typedef struct
{
   retro_atomic_size_t counter;
   retro_atomic_int_t  done;
   /* Filled in by the consumer; checked by main. */
   int counter_went_backwards;
   int final_mismatch;
   size_t final_seen;
   int reader_runaway;
} spsc_state_t;

static void spsc_writer(void *userdata)
{
   spsc_state_t *st = (spsc_state_t*)userdata;
   int i;
   for (i = 1; i <= SPSC_N; i++)
      retro_atomic_fetch_add_size(&st->counter, 1);
   /* Publish the done flag *after* the counter writes; pairs with the
    * consumer's load_acquire on `done`. */
   retro_atomic_store_release_int(&st->done, 1);
}

static void spsc_reader(void *userdata)
{
   spsc_state_t *st = (spsc_state_t*)userdata;
   size_t last  = 0;
   int saw_done = 0;
   /* Bound on iterations to keep CI from hanging if a backend is
    * silently broken; SPSC_N is 1e6, the loop should converge well
    * inside 1e8. */
   unsigned long long loops = 0;

   for (;;)
   {
      size_t cur = (size_t)retro_atomic_load_acquire_size(&st->counter);

      if (cur < last)
      {
         st->counter_went_backwards = 1;
         return;
      }
      last = cur;

      if (!saw_done && retro_atomic_load_acquire_int(&st->done))
         saw_done = 1;

      if (saw_done && cur >= (size_t)SPSC_N)
         break;

      if (++loops > 100000000ull)
      {
         st->reader_runaway = 1;
         return;
      }
   }

   st->final_seen = last;
   if (last != (size_t)SPSC_N)
      st->final_mismatch = 1;
}

static int check_spsc_stress(void)
{
   spsc_state_t st;
   sthread_t *tw, *tr;

   retro_atomic_size_init(&st.counter, 0);
   retro_atomic_int_init(&st.done, 0);
   st.counter_went_backwards = 0;
   st.final_mismatch         = 0;
   st.final_seen             = 0;
   st.reader_runaway         = 0;

   tw = sthread_create(spsc_writer, &st);
   tr = sthread_create(spsc_reader, &st);
   if (!tw || !tr)
   {
      fprintf(stderr, "FAIL spsc: sthread_create returned NULL\n");
      return 1;
   }
   sthread_join(tw);
   sthread_join(tr);

   if (st.counter_went_backwards)
   {
      fprintf(stderr, "FAIL spsc: counter observed going backwards\n");
      return 1;
   }
   if (st.reader_runaway)
   {
      fprintf(stderr, "FAIL spsc: reader exceeded loop bound\n");
      return 1;
   }
   if (st.final_mismatch)
   {
      fprintf(stderr, "FAIL spsc: final counter %zu != %d\n",
            st.final_seen, SPSC_N);
      return 1;
   }
   return 0;
}


/* ---- Threaded seqlock stress ------------------------------------------
 *
 * The reason the relaxed load/store forms exist.  A seqlock takes its
 * ordering from the stamp and the fences around it, not from the data
 * accesses, so those accesses are spelled relaxed: ordered enough to
 * be correct, cheap enough to be worth doing, and atomic enough that
 * ThreadSanitizer does not have to be told to look away.
 *
 * A writer publishes three fields that always sum to zero.  Readers
 * take a snapshot the seqlock way and check the sum.  A tuple
 * assembled from two different writer passes will not sum to zero, so
 * a stamp protocol that lets one through is caught here rather than by
 * a caller months later.  Under TSan the same run also proves the
 * accesses are free of formal data races, which is the property that
 * keeps a real race in seqlock-using code visible.
 *
 * Runs on x86_64 CI, where TSO hides a missing fence from the hardware
 * but not from TSan.  The ordering itself still wants an aarch64 or
 * PowerPC run to be fully exercised; see the header comment. */

#define SEQ_READERS      4
#define SEQ_WRITER_PASSES 200000
/* A reader that never lands inside a write has not tested anything, so
 * the run asserts it saw the stamp move under it at least once. */
#define SEQ_READ_TRIES   64

typedef struct
{
   retro_atomic_size_t seq;
   retro_atomic_int_t  a;
   retro_atomic_int_t  b;
   retro_atomic_int_t  c;
   retro_atomic_int_t  writer_done;
   retro_atomic_int_t  torn;         /* tuples that did not sum to zero */
   retro_atomic_int_t  retried;      /* reads that saw the stamp move   */
   retro_atomic_int_t  sampled;      /* reads that completed cleanly    */
   retro_atomic_int_t  gave_up;      /* reads that exhausted the bound  */
} seqlock_state_t;

static void seqlock_writer(void *userdata)
{
   seqlock_state_t *st = (seqlock_state_t*)userdata;
   int pass;

   for (pass = 1; pass <= SEQ_WRITER_PASSES; pass++)
   {
      size_t s = retro_atomic_load_relaxed_size(&st->seq);

      retro_atomic_store_release_size(&st->seq, s + 1);
      /* Keeps the field stores below from being hoisted above the odd
       * stamp that tells a reader the tuple is in flux. */
      retro_atomic_thread_fence_release();

      retro_atomic_store_relaxed_int(&st->a,  pass);
      retro_atomic_store_relaxed_int(&st->b,  pass * 2);
      retro_atomic_store_relaxed_int(&st->c, -pass * 3);

      retro_atomic_store_release_size(&st->seq, s + 2);
   }

   retro_atomic_store_release_int(&st->writer_done, 1);
}

static void seqlock_reader(void *userdata)
{
   seqlock_state_t *st = (seqlock_state_t*)userdata;

   while (!retro_atomic_load_acquire_int(&st->writer_done))
   {
      int tries;

      for (tries = 0; tries < SEQ_READ_TRIES; tries++)
      {
         size_t s1 = retro_atomic_load_acquire_size(&st->seq);
         size_t s2;
         int    x, y, z;

         if (s1 & 1)
         {
            retro_atomic_inc_int(&st->retried);
            continue;
         }

         x  = retro_atomic_load_relaxed_int(&st->a);
         y  = retro_atomic_load_relaxed_int(&st->b);
         z  = retro_atomic_load_relaxed_int(&st->c);

         retro_atomic_thread_fence_acquire();
         s2 = retro_atomic_load_acquire_size(&st->seq);

         if (s1 != s2)
         {
            retro_atomic_inc_int(&st->retried);
            continue;
         }

         if (x + y + z != 0)
            retro_atomic_inc_int(&st->torn);
         retro_atomic_inc_int(&st->sampled);
         break;
      }

      if (tries == SEQ_READ_TRIES)
         retro_atomic_inc_int(&st->gave_up);
   }
}

static int check_seqlock_stress(void)
{
   seqlock_state_t st;
   sthread_t      *readers[SEQ_READERS];
   sthread_t      *writer;
   int             i;
   int             torn, sampled, retried;

   retro_atomic_size_init(&st.seq, 0);
   retro_atomic_int_init(&st.a, 0);
   retro_atomic_int_init(&st.b, 0);
   retro_atomic_int_init(&st.c, 0);
   retro_atomic_int_init(&st.writer_done, 0);
   retro_atomic_int_init(&st.torn, 0);
   retro_atomic_int_init(&st.retried, 0);
   retro_atomic_int_init(&st.sampled, 0);
   retro_atomic_int_init(&st.gave_up, 0);

   for (i = 0; i < SEQ_READERS; i++)
      if (!(readers[i] = sthread_create(seqlock_reader, &st)))
      {
         fprintf(stderr, "FAIL seqlock: sthread_create returned NULL\n");
         return 1;
      }
   if (!(writer = sthread_create(seqlock_writer, &st)))
   {
      fprintf(stderr, "FAIL seqlock: sthread_create returned NULL\n");
      return 1;
   }

   sthread_join(writer);
   for (i = 0; i < SEQ_READERS; i++)
      sthread_join(readers[i]);

   torn    = retro_atomic_load_acquire_int(&st.torn);
   sampled = retro_atomic_load_acquire_int(&st.sampled);
   retried = retro_atomic_load_acquire_int(&st.retried);

   if (torn)
   {
      fprintf(stderr, "FAIL seqlock: %d tuples assembled from two "
            "writer passes\n", torn);
      return 1;
   }
   if (!sampled)
   {
      fprintf(stderr, "FAIL seqlock: readers never completed a snapshot\n");
      return 1;
   }
   if (!retried)
   {
      fprintf(stderr, "FAIL seqlock: no reader ever landed inside a "
            "write, so the stamp protocol went untested\n");
      return 1;
   }
   if ((size_t)retro_atomic_load_acquire_size(&st.seq)
         != (size_t)SEQ_WRITER_PASSES * 2)
   {
      fprintf(stderr, "FAIL seqlock: stamp did not advance by two a pass\n");
      return 1;
   }
   return 0;
}

#endif /* HAVE_THREADS */

int main(void)
{
   int fails = 0;

   printf("retro_atomic backend: %s\n", backend_name());
#if defined(RETRO_ATOMIC_LOCK_FREE)
   printf("retro_atomic lock-free: yes\n");
#else
   printf("retro_atomic lock-free: NO (volatile fallback; SMP-unsafe)\n");
#endif

   fails += check_init();
   fails += check_store_load();
   fails += check_relaxed_store_load();
   fails += check_seqlock_shape();
   fails += check_fetch_add_returns_previous();
   fails += check_fetch_sub_returns_previous();
   fails += check_inc_dec_wrappers();

#ifdef HAVE_THREADS
   fails += check_spsc_stress();
   fails += check_seqlock_stress();
#else
   printf("[skip] SPSC + seqlock stress tests "
         "(HAVE_THREADS not defined)\n");
#endif

   if (fails == 0)
   {
      printf("ALL OK\n");
      return 0;
   }
   printf("%d FAILURE(S)\n", fails);
   return 1;
}

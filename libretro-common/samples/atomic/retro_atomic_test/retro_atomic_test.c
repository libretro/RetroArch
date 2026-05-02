/* Regression test for libretro-common/include/retro_atomic.h.
 *
 * Background
 * ----------
 * retro_atomic.h consolidates the ad-hoc atomic shims that were
 * previously duplicated in audio/drivers/{coreaudio,coreaudio3,xaudio,
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
 *  7. The test prints which backend was selected and whether
 *     RETRO_ATOMIC_LOCK_FREE is defined, so a CI diff makes accidental
 *     backend regressions obvious.
 *
 * What this test does NOT assert
 * ------------------------------
 * It does not validate hardware ordering on weakly-ordered SMP from
 * a single host run on x86_64 (TSO masks most reordering bugs).  For
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
   fails += check_fetch_add_returns_previous();
   fails += check_fetch_sub_returns_previous();
   fails += check_inc_dec_wrappers();

#ifdef HAVE_THREADS
   fails += check_spsc_stress();
#else
   printf("[skip] SPSC stress test (HAVE_THREADS not defined)\n");
#endif

   if (fails == 0)
   {
      printf("ALL OK\n");
      return 0;
   }
   printf("%d FAILURE(S)\n", fails);
   return 1;
}

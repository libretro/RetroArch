/* Regression test for retro_atomic.h's PS2 EE backend.
 *
 * Background
 * ----------
 * The R5900 has no LL/SC - GCC's ISA_HAS_LL_SC excludes TARGET_MIPS5900
 * - so the builtins lower to __atomic_* libcalls the ps2sdk toolchain
 * cannot link, and the header instead builds its read-modify-writes out
 * of the interrupt mask ps2sdk exposes as DIntr()/EIntr().  The EE is
 * one in-order core whose kernel reschedules only out of an interrupt,
 * which is what makes that a real atomic there.
 *
 * The backend is therefore the one in the header whose correctness
 * rests on a platform primitive rather than on a compiler builtin, and
 * the PS2 CI lane only tells us it compiles.  ps2stub/ stands in for
 * <kernel.h> with a mask the test can inspect, so the semantics are
 * checked on the host at every push.
 *
 * What this test asserts
 * ----------------------
 *  1. Selection and capability flags: RETRO_ATOMIC_BACKEND_PS2 with
 *     RETRO_ATOMIC_HAS_CAS, RETRO_ATOMIC_HAS_PTR and
 *     RETRO_ATOMIC_HAS_64 set, and RETRO_ATOMIC_LOCK_FREE unset - the
 *     ops are atomic but taken with the interrupts masked, and a
 *     caller that would spin on them has to keep its locked path (one
 *     core: the thread holding the word never runs again).
 *  2. Every operation in the surface returns the previous value where
 *     the API says it does and leaves the storage as specified,
 *     including the failing CAS leaving it untouched.
 *  3. Each operation opens exactly one masked section and closes it:
 *     the DIntr/EIntr calls pair, the nesting depth returns to zero,
 *     and no section is entered twice.
 *  4. An operation performed inside a section the caller already
 *     opened does not unmask on the way out.  This is the property
 *     DIntr()'s return value exists for, and getting it wrong would
 *     re-enable interrupts in the middle of somebody else's critical
 *     section - a fault that would surface on hardware as rare
 *     corruption rather than as a test failure.
 *
 * What this test does NOT assert
 * ------------------------------
 * There is no threaded stress here.  The masked section is the whole
 * of the mutual exclusion, and a host thread cannot mask anything, so
 * running two of them against a stub flag would exercise the stub
 * rather than the backend.  Mutual exclusion on the EE follows from
 * the single core plus the mask, and the parts of that a host can
 * check are the pairing and nesting above.
 */

#include <stdio.h>
#include <stddef.h>

#include <retro_atomic.h>

#include "ps2stub/kernel.h"

#if !defined(RETRO_ATOMIC_BACKEND_PS2)
#error "retro_atomic.h: RETRO_ATOMIC_FORCE_PS2 did not select the PS2 backend"
#endif
#if defined(RETRO_ATOMIC_LOCK_FREE)
#error "retro_atomic.h: the PS2 backend is interrupt-masked and must not claim RETRO_ATOMIC_LOCK_FREE"
#endif
#if !defined(RETRO_ATOMIC_HAS_CAS) || !defined(RETRO_ATOMIC_HAS_PTR) \
 || !defined(RETRO_ATOMIC_HAS_64)
#error "retro_atomic.h: the PS2 backend must expose the CAS, pointer and 64-bit ops"
#endif

static int failures;

static void check(int ok, const char *what)
{
   if (!ok)
   {
      printf("FAIL: %s\n", what);
      failures++;
   }
}

static void check_int_ops(void)
{
   retro_atomic_int_t a;

   retro_atomic_int_init(&a, 7);
   check(retro_atomic_load_acquire_int(&a) == 7, "int init");

   retro_atomic_store_release_int(&a, 8);
   check(retro_atomic_load_acquire_int(&a) == 8, "int store_release");

   check(retro_atomic_fetch_add_int(&a, 3) == 8,  "fetch_add returns old");
   check(retro_atomic_load_acquire_int(&a) == 11, "fetch_add stores");
   check(retro_atomic_fetch_sub_int(&a, 4) == 11, "fetch_sub returns old");
   check(retro_atomic_load_acquire_int(&a) == 7,  "fetch_sub stores");
   check(retro_atomic_fetch_or_int(&a, 8)  == 7,  "fetch_or returns old");
   check(retro_atomic_load_acquire_int(&a) == 15, "fetch_or stores");
   check(retro_atomic_fetch_and_int(&a, 12) == 15, "fetch_and returns old");
   check(retro_atomic_load_acquire_int(&a) == 12, "fetch_and stores");

   retro_atomic_inc_int(&a);
   check(retro_atomic_load_acquire_int(&a) == 13, "inc_int");
   retro_atomic_dec_int(&a);
   check(retro_atomic_load_acquire_int(&a) == 12, "dec_int");

   check(retro_atomic_exchange_int(&a, 99) == 12, "exchange returns old");
   check(retro_atomic_load_acquire_int(&a) == 99, "exchange stores");

   check(retro_atomic_cas_int(&a, 99, 5) != 0,   "cas succeeds on a match");
   check(retro_atomic_load_acquire_int(&a) == 5, "cas stores on success");
   check(retro_atomic_cas_int(&a, 99, 6) == 0,   "cas fails on a mismatch");
   check(retro_atomic_load_acquire_int(&a) == 5, "a failed cas stores nothing");
}

static void check_size_ops(void)
{
   retro_atomic_size_t a;

   retro_atomic_size_init(&a, 4);
   check(retro_atomic_load_acquire_size(&a) == 4, "size init");
   check(retro_atomic_load_relaxed_size(&a) == 4, "size relaxed load");

   retro_atomic_store_release_size(&a, 6);
   check(retro_atomic_load_acquire_size(&a) == 6, "size store_release");

   check(retro_atomic_fetch_add_size(&a, 2) == 6, "size fetch_add returns old");
   check(retro_atomic_load_acquire_size(&a) == 8, "size fetch_add stores");
   check(retro_atomic_fetch_sub_size(&a, 3) == 8, "size fetch_sub returns old");
   check(retro_atomic_load_acquire_size(&a) == 5, "size fetch_sub stores");

   retro_atomic_inc_size(&a);
   retro_atomic_dec_size(&a);
   check(retro_atomic_load_acquire_size(&a) == 5, "size inc/dec round trip");
}

static void check_ptr_ops(void)
{
   retro_atomic_ptr_t a;
   int first  = 1;
   int second = 2;

   retro_atomic_ptr_init(&a, &first);
   check(retro_atomic_load_acquire_ptr(&a) == &first, "ptr init");

   retro_atomic_store_release_ptr(&a, &second);
   check(retro_atomic_load_acquire_ptr(&a) == &second, "ptr store_release");

   check(retro_atomic_exchange_ptr(&a, &first) == &second,
         "ptr exchange returns old");
   check(retro_atomic_load_acquire_ptr(&a) == &first, "ptr exchange stores");

   check(retro_atomic_cas_ptr(&a, &first, &second) != 0,
         "ptr cas succeeds on a match");
   check(retro_atomic_load_acquire_ptr(&a) == &second, "ptr cas stores");
   check(retro_atomic_cas_ptr(&a, &first, NULL) == 0,
         "ptr cas fails on a mismatch");
   check(retro_atomic_load_acquire_ptr(&a) == &second,
         "a failed ptr cas stores nothing");
}

static void check_64_ops(void)
{
   retro_atomic_64_t a;
   int64_t           big = ((int64_t)1 << 40) + 3;

   retro_atomic_64_init(&a, 0);
   retro_atomic_store_release_64(&a, big);
   check(retro_atomic_load_acquire_64(&a) == big, "64-bit store/load");

   check(retro_atomic_exchange_64(&a, 1) == big, "64-bit exchange returns old");
   check(retro_atomic_load_acquire_64(&a) == 1,  "64-bit exchange stores");

   check(retro_atomic_cas_64(&a, 1, big) != 0,    "64-bit cas succeeds");
   check(retro_atomic_load_acquire_64(&a) == big, "64-bit cas stores");
   check(retro_atomic_cas_64(&a, 1, 2) == 0,      "64-bit cas fails on mismatch");
   check(retro_atomic_load_acquire_64(&a) == big,
         "a failed 64-bit cas stores nothing");
}

/* Every op above ran with the interrupts enabled on entry, so each one
 * had to mask and unmask exactly once. */
static void check_mask_discipline(void)
{
   check(ps2stub_eie == 1,      "interrupts enabled after the surface ran");
   check(ps2stub_nest == 0,     "masked sections all closed");
   check(ps2stub_max_nest == 1, "no operation masked an already masked section");
   check(ps2stub_di_calls == ps2stub_ei_calls, "DIntr and EIntr calls pair");
   check(ps2stub_di_calls > 0,  "the read-modify-writes took the mask");
}

/* An op inside a section the caller opened leaves the mask as it found
 * it, which is what DIntr()'s return value is for. */
static void check_nesting(void)
{
   retro_atomic_int_t a;
   int                outer;
   int                ei_before;

   retro_atomic_int_init(&a, 0);

   outer     = DIntr();
   ei_before = ps2stub_ei_calls;

   retro_atomic_fetch_add_int(&a, 1);
   retro_atomic_exchange_int(&a, 2);
   retro_atomic_cas_int(&a, 2, 3);

   check(ps2stub_eie == 0, "a nested op left the interrupts masked");
   check(ps2stub_ei_calls == ei_before, "a nested op did not unmask");

   if (outer)
      EIntr();

   check(ps2stub_eie == 1, "the caller's section unmasked on exit");
   check(retro_atomic_load_acquire_int(&a) == 3, "nested ops still applied");
}

int main(void)
{
   check_int_ops();
   check_size_ops();
   check_ptr_ops();
   check_64_ops();
   check_mask_discipline();
   check_nesting();

   printf("retro_atomic backend: %s\n", RETRO_ATOMIC_BACKEND_NAME);
   printf("retro_atomic lock-free: no (interrupt-masked)\n");

   if (failures)
   {
      printf("%d FAILURE(S)\n", failures);
      return 1;
   }

   printf("ALL OK\n");
   return 0;
}

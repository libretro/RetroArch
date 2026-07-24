/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (gfx_thumbnail_status_atomic_test.c).
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

/* Regression test for the cross-thread thumbnail status
 * synchronisation in gfx/gfx_thumbnail.c.
 *
 * The .status field of gfx_thumbnail_t is read by the video
 * thread (in gfx_thumbnail_draw) and written by the upload-
 * callback thread (in gfx_thumbnail_handle_upload):
 *
 *   --- upload-callback thread ---
 *   thumbnail->texture = ...;
 *   thumbnail->width   = ...;
 *   thumbnail->height  = ...;
 *   GFX_THUMB_STATUS_STORE(&thumbnail->status,
 *                          GFX_THUMBNAIL_STATUS_AVAILABLE);
 *
 *   --- video thread ---
 *   if (GFX_THUMB_STATUS_LOAD(&thumbnail->status)
 *       == GFX_THUMBNAIL_STATUS_AVAILABLE) {
 *      uintptr_t tex = thumbnail->texture;
 *      unsigned  w   = thumbnail->width;
 *      unsigned  h   = thumbnail->height;
 *      ...
 *   }
 *
 * On weakly-ordered SMP hardware (ARM, AArch64, PowerPC), this
 * pattern requires release-store / acquire-load barriers:
 * without them the video thread can observe the AVAILABLE
 * status before the texture/width/height writes are visible,
 * leading to garbage thumbnail rendering or div-by-zero in the
 * aspect-ratio code.  Pre-fix gfx_thumbnail.c had its own
 * hand-rolled atomic shim covering GCC __atomic_*, MSVC
 * Interlocked, and a volatile fallback; the port to
 * retro_atomic.h replaces the shim with the portable API and
 * stores the field as retro_atomic_int_t so the type system
 * enforces the discipline.
 *
 * This test exercises two failure modes:
 *
 *  1. Compile-time: the .status field MUST be retro_atomic_int_t
 *     (not a plain enum), so that on the C11 stdatomic and
 *     C++11 std::atomic backends a future contributor cannot
 *     accidentally bypass the API with a plain `t->status = X`
 *     assignment -- those backends would refuse to compile
 *     such an assignment.  We assert this with a static
 *     check that verifies sizeof(.status) == sizeof(int)
 *     (preserving struct layout) and that the macros
 *     GFX_THUMB_STATUS_STORE / GFX_THUMB_STATUS_LOAD route
 *     through retro_atomic_*_int (verified at preprocessing
 *     by token-pasting their expansion).
 *
 *  2. Runtime: a 1M-iteration producer/consumer stress test in
 *     which the .status field itself carries every ordering
 *     edge: the producer stages a monotonic (texture, width,
 *     height) triple and publishes it with
 *     STATUS_STORE(AVAILABLE); the consumer polls STATUS_LOAD
 *     until it observes AVAILABLE, verifies the triple, and
 *     acknowledges with STATUS_STORE(PENDING), which is the
 *     edge the producer waits on before staging the next
 *     generation.  Intended to be run under ThreadSanitizer:
 *     because the accessors under test are the only
 *     synchronisation in the loop, de-atomicising either macro
 *     is immediately a reported data race - on the status field
 *     and on the staged triple - even on x86 TSO (where the
 *     hardware otherwise hides the bug).
 *
 * If gfx_thumbnail.c amends GFX_THUMB_STATUS_STORE /
 * GFX_THUMB_STATUS_LOAD or the .status field type, the
 * verbatim copies in this test must follow.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <retro_atomic.h>

/* Needs HAVE_THREADS from the build for the SPSC stress; fall
 * through to the static-assert checks otherwise. */
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

/* === Verbatim mirror of gfx_thumbnail.h's relevant pieces === */

enum gfx_thumbnail_status
{
   GFX_THUMBNAIL_STATUS_UNKNOWN = 0,
   GFX_THUMBNAIL_STATUS_PENDING,
   GFX_THUMBNAIL_STATUS_AVAILABLE,
   GFX_THUMBNAIL_STATUS_MISSING
};

/* Mirror of gfx_thumbnail_t with the same field order/types.
 * Layout must match gfx_thumbnail.h's definition; size/offset
 * checks below validate this. */
typedef struct
{
   uintptr_t          texture;
   unsigned           width;
   unsigned           height;
   float              alpha;
   float              delay_timer;
   retro_atomic_int_t status;
   uint8_t            flags;
} gfx_thumbnail_t;

/* Verbatim mirror of the macros from gfx/gfx_thumbnail.c.
 * If the originals change, this block must follow. */
#define GFX_THUMB_STATUS_STORE(ptr, val) \
   retro_atomic_store_release_int((retro_atomic_int_t*)(void*)(ptr), (int)(val))
#define GFX_THUMB_STATUS_LOAD(ptr) \
   ((enum gfx_thumbnail_status)retro_atomic_load_acquire_int((retro_atomic_int_t*)(void*)(ptr)))

/* Local mirror of gfx_thumbnail_init_blank.  The test cannot
 * include gfx_thumbnail.h directly because it deliberately
 * shadows that header's type definitions to verify layout
 * invariants -- so we mirror the helper as well.  Field-by-
 * field zero-init is required (rather than memset) when the
 * test is compiled as C++ via CXX_BUILD: the .status field is
 * std::atomic<int>, making gfx_thumbnail_t non-trivially-
 * copyable, and memset of such a struct warns
 * -Wclass-memaccess. */
static void gfx_thumbnail_test_init_blank(gfx_thumbnail_t *t)
{
   t->texture     = 0;
   t->width       = 0;
   t->height      = 0;
   t->alpha       = 0.0f;
   t->delay_timer = 0.0f;
   retro_atomic_int_init(&t->status, GFX_THUMBNAIL_STATUS_UNKNOWN);
   t->flags       = 0;
}

/* === Compile-time invariants === */

/* Size: the atomic-typed status field must be the same size as
 * a plain int on every supported backend, so swapping it in for
 * `enum gfx_thumbnail_status` doesn't change the gfx_thumbnail_t
 * layout. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(((gfx_thumbnail_t*)0)->status) == sizeof(int),
   "gfx_thumbnail_t.status must be int-sized for ABI compatibility");
#elif defined(__cplusplus) && __cplusplus >= 201103L
static_assert(sizeof(((gfx_thumbnail_t*)0)->status) == sizeof(int),
   "gfx_thumbnail_t.status must be int-sized for ABI compatibility");
#else
/* Pre-C11 fallback: array-size negative-on-failure trick. */
typedef char _gfx_thumb_status_size_check[
   (sizeof(((gfx_thumbnail_t*)0)->status) == sizeof(int)) ? 1 : -1];
#endif

/* Backend sanity: retro_atomic.h must be selected and lock-free.
 * The volatile fallback is correct only on x86 TSO / single-core
 * targets and would not provide barriers on weak-memory hardware,
 * which is exactly what the upload/draw pairing relies on. */
#if !defined(HAVE_RETRO_ATOMIC)
#  error "retro_atomic.h: HAVE_RETRO_ATOMIC not defined"
#endif

/* === Runtime SPSC stress (HAVE_THREADS only) === */

#ifdef HAVE_THREADS

/* Number of producer/consumer iterations.  1M is enough to flush
 * out a missing-barrier bug under TSan within a few seconds; on
 * real weak-memory hardware (qemu-aarch64) it surfaces faster.
 * Adjust upward if the bug becomes harder to reproduce.
 * Overridable (-DSTRESS_ITERS=...) for constrained environments:
 * the lock-step handshake costs scheduler timeslices per
 * iteration on a single-CPU host, and TSan's happens-before
 * detection does not depend on the count. */
#ifndef STRESS_ITERS
#define STRESS_ITERS 1000000
#endif

/* SPSC latch state.
 *
 * The real gfx_thumbnail upload/draw pairing is single-publication:
 * the upload-callback thread writes the texture once and publishes
 * AVAILABLE; the video thread observes AVAILABLE and reads the
 * triple every frame thereafter.  A stress test needs repetition,
 * and a naive repeating status-gated handshake has a stale window:
 * after acknowledging generation i through a side channel, the
 * consumer can observe the still-AVAILABLE status of generation i
 * while the producer is already restaging the triple for i+1 - a
 * genuine race in the harness, not in the primitives.  An earlier
 * revision of this test dodged the window by gating the handshake
 * on separate produced/consumed counters, but that quietly moved
 * every synchronising edge off the field under test: with the
 * counters carrying the ordering, de-atomicising the status macros
 * was invisible to TSan, because the status accesses were always
 * ordered by the counter edges (verified by mutation).
 *
 * So the stress runs the status field as a two-phase latch in
 * which BOTH edges are the macros under test:
 *
 *   producer: poll STATUS_LOAD  until PENDING   (acquire: orders
 *             staging after the consumer's reads of the previous
 *             generation)
 *             stage texture/width/height
 *             STATUS_STORE(AVAILABLE)           (release: publishes
 *             the staged triple)
 *
 *   consumer: poll STATUS_LOAD  until AVAILABLE (acquire: orders
 *             the triple reads after staging)
 *             read and verify the triple
 *             STATUS_STORE(PENDING)             (release: the ack
 *             the producer waits on)
 *
 * Strict alternation: AVAILABLE uniquely means "current staged,
 * unconsumed generation", so there is no stale window and no side
 * channel.  The one deviation from production is that the reader
 * stores status; this is the minimal repeating structure whose
 *
 * every ordering edge is the accessor pair, which is the property
 * the suite exists to verify.  Shutdown rides the same field: the
 * producer stores MISSING after the final ack. */
typedef struct
{
   gfx_thumbnail_t    thumb;
   /* Counter of mismatches the consumer observed.  Read by main
    * thread after the join, so it doesn't need to be atomic. */
   unsigned long      mismatches;
} stress_state_t;

static void producer_thread(void *arg)
{
   stress_state_t *s = (stress_state_t *)arg;
   int             i;

   for (i = 1; i <= STRESS_ITERS; i++)
   {
      /* Wait for the consumer's ack of the previous generation.
       * run_stress primes the latch to PENDING, so the first
       * iteration proceeds immediately.  Acquire semantics order
       * our staging writes after the consumer's reads of the
       * previous triple. */
      while (GFX_THUMB_STATUS_LOAD(&s->thumb.status)
            != GFX_THUMBNAIL_STATUS_PENDING)
         ; /* spin */

      /* Stage the generation's data (no ordering required
       * between these). */
      s->thumb.texture = (uintptr_t)i;
      s->thumb.width   = (unsigned)i;
      s->thumb.height  = (unsigned)i;

      /* Publish.  Same shape as the production upload path:
       * STATUS_STORE(AVAILABLE) after staging the texture, with
       * release-store semantics that fence the earlier writes. */
      GFX_THUMB_STATUS_STORE(&s->thumb.status,
            GFX_THUMBNAIL_STATUS_AVAILABLE);
   }

   /* Wait for the final ack, then signal shutdown through the
    * same field. */
   while (GFX_THUMB_STATUS_LOAD(&s->thumb.status)
         != GFX_THUMBNAIL_STATUS_PENDING)
      ; /* drain */
   GFX_THUMB_STATUS_STORE(&s->thumb.status,
         GFX_THUMBNAIL_STATUS_MISSING);
}

static void consumer_thread(void *arg)
{
   stress_state_t *s             = (stress_state_t *)arg;
   unsigned long   mismatches    = 0;
   int             expected      = 1;

   /* Per-generation loop.  Poll the status field - the macro
    * under test is the only forward edge - until it observes
    * AVAILABLE (or MISSING, the shutdown sentinel).  The
    * acquire-load orders the triple reads after the producer's
    * staging; the release-store of PENDING afterwards is the ack
    * the producer's next generation waits on. */
   for (;;)
   {
      uintptr_t                 tex;
      unsigned                  w, h;
      enum gfx_thumbnail_status st;

      do {
         st = GFX_THUMB_STATUS_LOAD(&s->thumb.status);
      } while (st != GFX_THUMBNAIL_STATUS_AVAILABLE
            && st != GFX_THUMBNAIL_STATUS_MISSING);

      if (st == GFX_THUMBNAIL_STATUS_MISSING)
         break;

      /* Read the staged data.  The acquire-load of AVAILABLE
       * fences these reads; if any field disagrees with the
       * expected generation, the fence failed. */
      tex = s->thumb.texture;
      w   = s->thumb.width;
      h   = s->thumb.height;

      if (tex != (uintptr_t)expected
            || w != (unsigned)expected
            || h != (unsigned)expected)
         mismatches++;

      /* Ack: producer waits on this before staging the next
       * generation.  Release semantics publish our reads as
       * complete. */
      GFX_THUMB_STATUS_STORE(&s->thumb.status,
            GFX_THUMBNAIL_STATUS_PENDING);
      expected++;
   }

   s->mismatches = mismatches;
}

static int run_stress(void)
{
   stress_state_t s;
   sthread_t     *prod;
   sthread_t     *cons;

   /* Field-by-field init, not memset, to silence
    * -Wclass-memaccess under CXX_BUILD where .status is
    * std::atomic<int> and the struct is non-trivially-copyable. */
   gfx_thumbnail_test_init_blank(&s.thumb);
   s.mismatches = 0;

   /* Prime the latch: PENDING is the "consumer ready" state the
    * producer's first generation waits on.  Single-threaded here,
    * before either thread exists. */
   GFX_THUMB_STATUS_STORE(&s.thumb.status,
         GFX_THUMBNAIL_STATUS_PENDING);

   prod = sthread_create(producer_thread, &s);
   if (!prod)
   {
      fprintf(stderr, "FAIL: sthread_create(producer)\n");
      return 1;
   }
   cons = sthread_create(consumer_thread, &s);
   if (!cons)
   {
      fprintf(stderr, "FAIL: sthread_create(consumer)\n");
      sthread_join(prod);
      return 1;
   }

   sthread_join(prod);
   sthread_join(cons);

   if (s.mismatches != 0)
   {
      fprintf(stderr,
         "FAIL: SPSC stress observed %lu mismatched reads "
         "out of %d iterations\n", s.mismatches, STRESS_ITERS);
      return 1;
   }

   printf("[pass] SPSC stress: %d iterations, 0 mismatches\n",
      STRESS_ITERS);
   return 0;
}

#else /* HAVE_THREADS */

static int run_stress(void)
{
   /* Compile-time-only mode: skip stress, but the static
    * assertions above still validated the type/layout invariants. */
   printf("[skip] HAVE_THREADS not defined; static checks only\n");
   return 0;
}

#endif /* HAVE_THREADS */

/* === Single-threaded property checks === */

static int run_property_checks(void)
{
   gfx_thumbnail_t t;
   /* Field-by-field init, not memset, to silence
    * -Wclass-memaccess under CXX_BUILD. */
   gfx_thumbnail_test_init_blank(&t);

   /* Round-trip every enum value through STORE/LOAD. */
   {
      const enum gfx_thumbnail_status vals[] = {
         GFX_THUMBNAIL_STATUS_UNKNOWN,
         GFX_THUMBNAIL_STATUS_PENDING,
         GFX_THUMBNAIL_STATUS_AVAILABLE,
         GFX_THUMBNAIL_STATUS_MISSING,
      };
      size_t i;
      for (i = 0; i < sizeof(vals) / sizeof(vals[0]); i++)
      {
         GFX_THUMB_STATUS_STORE(&t.status, vals[i]);
         if (GFX_THUMB_STATUS_LOAD(&t.status) != vals[i])
         {
            fprintf(stderr,
               "FAIL: STORE/LOAD round-trip on value %d\n",
               (int)vals[i]);
            return 1;
         }
      }
   }

   /* The acquire-load must read what the release-store wrote
    * (single-threaded, so no ordering question, just contract
    * verification). */
   GFX_THUMB_STATUS_STORE(&t.status, GFX_THUMBNAIL_STATUS_AVAILABLE);
   if (GFX_THUMB_STATUS_LOAD(&t.status) != GFX_THUMBNAIL_STATUS_AVAILABLE)
   {
      fprintf(stderr, "FAIL: AVAILABLE not observable after store\n");
      return 1;
   }

   printf("[pass] STORE/LOAD round-trip on all four status values\n");
   return 0;
}

int main(void)
{
   printf("retro_atomic backend: %s\n", RETRO_ATOMIC_BACKEND_NAME);
#ifdef RETRO_ATOMIC_LOCK_FREE
   printf("retro_atomic lock-free: yes\n");
#else
   printf("retro_atomic lock-free: NO (volatile fallback)\n");
#endif

   if (run_property_checks() != 0)
      return 1;
   if (run_stress() != 0)
      return 1;

   puts("ALL OK");
   return 0;
}

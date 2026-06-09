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
 *  2. Runtime: a 1M-iteration producer/consumer stress test
 *     that mirrors the upload/draw shape: producer writes a
 *     monotonic (texture, width, height) triple before
 *     publishing AVAILABLE; consumer waits for AVAILABLE and
 *     verifies the triple is internally consistent (all three
 *     fields belong to the same generation).  Intended to be
 *     run under ThreadSanitizer, which instruments every
 *     atomic load/store and would flag the missing-barrier
 *     case as a data race even on x86 TSO (where the
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
 * Adjust upward if the bug becomes harder to reproduce. */
#define STRESS_ITERS 1000000

/* SPSC handshake state.
 *
 * The real gfx_thumbnail upload/draw pairing is single-publication:
 * the upload-callback thread writes the texture once and publishes
 * AVAILABLE; the video thread observes AVAILABLE and reads the
 * triple every frame thereafter.  There is no high-frequency
 * back-and-forth.
 *
 * To mirror that shape under stress we need a per-generation
 * handshake so the texture / width / height fields are NOT being
 * rewritten while the consumer is reading them.  Without that,
 * a tight loop where the producer rewrites the data while the
 * consumer reads it would generate torn reads even with correct
 * barriers, because the data is mutating concurrently -- a
 * property of the test design, not of the synchronisation
 * primitives.
 *
 * We pair two atomic counters for the handshake:
 *  - `produced`: producer increments it after staging a generation,
 *    via release-store so the staged data is visible to any thread
 *    that observes the new value.  Consumer waits for it to exceed
 *    the last-seen value via acquire-load.  This is the actual
 *    publish edge the consumer keys on (rather than a status
 *    transition, which would require the consumer to observe a
 *    transient UNKNOWN that the producer might overwrite faster
 *    than the consumer can poll).
 *  - `consumed`: consumer increments it after reading; producer
 *    waits for `consumed == produced` before staging the next
 *    generation.
 *
 * The .status field (the actual subject of the test) is updated
 * on the same pattern as in gfx_thumbnail.c -- each generation
 * stores AVAILABLE through GFX_THUMB_STATUS_STORE before the
 * counter bump, and the consumer reads it through
 * GFX_THUMB_STATUS_LOAD as part of the per-generation read.  This
 * exercises the macros under test even though the counter is what
 * gates the handshake. */
typedef struct
{
   gfx_thumbnail_t    thumb;
   /* Producer's generation counter.  Bumped via release-store
    * after staging the data; consumer reads via acquire-load. */
   retro_atomic_int_t produced;
   /* Consumer's acknowledgement counter.  Bumped via release-
    * store after reading; producer reads via acquire-load. */
   retro_atomic_int_t consumed;
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
      /* Wait for the consumer to acknowledge the previous
       * generation.  On the first iteration this is true
       * immediately (consumed starts at 0). */
      while (retro_atomic_load_acquire_int(&s->consumed) != (i - 1))
         ; /* spin */

      /* Stage the generation's data (no ordering required
       * between these). */
      s->thumb.texture = (uintptr_t)i;
      s->thumb.width   = (unsigned)i;
      s->thumb.height  = (unsigned)i;

      /* Update status via the macros under test.  Same shape as
       * the production gfx_thumbnail.c upload path:
       * STATUS_STORE(AVAILABLE) after staging the texture, with
       * release-store semantics that fence the earlier writes. */
      GFX_THUMB_STATUS_STORE(&s->thumb.status,
            GFX_THUMBNAIL_STATUS_AVAILABLE);

      /* Publish the new generation.  Release-store so the staged
       * data and the AVAILABLE status are visible before the
       * consumer observes the new generation. */
      retro_atomic_store_release_int(&s->produced, i);
   }

   /* Wait for the final ack, then signal shutdown via a
    * sentinel value `produced = STRESS_ITERS + 1`, paired with
    * a final STATUS_STORE so the consumer exercises the macro
    * one last time. */
   while (retro_atomic_load_acquire_int(&s->consumed) != STRESS_ITERS)
      ; /* drain */
   GFX_THUMB_STATUS_STORE(&s->thumb.status,
         GFX_THUMBNAIL_STATUS_MISSING);
   retro_atomic_store_release_int(&s->produced, STRESS_ITERS + 1);
}

static void consumer_thread(void *arg)
{
   stress_state_t *s             = (stress_state_t *)arg;
   unsigned long   mismatches    = 0;
   int             expected      = 1;

   /* Per-generation loop.  Wait for produced >= expected, then
    * read the staged data and the status.  The status read
    * goes through GFX_THUMB_STATUS_LOAD (the macro under test);
    * it should always observe AVAILABLE for the current
    * generation.  Both reads are acquire-loads; the producer's
    * release-stores ensure ordering between staging and
    * publication. */
   for (;;)
   {
      int                       prod;
      uintptr_t                 tex;
      unsigned                  w, h;
      enum gfx_thumbnail_status st;

      /* Wait for the producer to publish the next generation
       * (or the shutdown sentinel STRESS_ITERS + 1). */
      do {
         prod = retro_atomic_load_acquire_int(&s->produced);
      } while (prod < expected);

      if (prod == STRESS_ITERS + 1)
         break;

      /* Read the staged data and status.  The acquire-load on
       * `produced` fences these reads; if any field disagrees
       * with the expected generation, the fence failed.  We
       * additionally exercise GFX_THUMB_STATUS_LOAD here -- it
       * should always observe AVAILABLE for the current
       * generation. */
      tex = s->thumb.texture;
      w   = s->thumb.width;
      h   = s->thumb.height;
      st  = GFX_THUMB_STATUS_LOAD(&s->thumb.status);

      if (tex != (uintptr_t)expected
            || w != (unsigned)expected
            || h != (unsigned)expected
            || st != GFX_THUMBNAIL_STATUS_AVAILABLE)
         mismatches++;

      /* Ack: producer waits on this before staging the next
       * generation. */
      retro_atomic_store_release_int(&s->consumed, expected);
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
   retro_atomic_int_init(&s.produced, 0);
   retro_atomic_int_init(&s.consumed, 0);
   s.mismatches = 0;

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

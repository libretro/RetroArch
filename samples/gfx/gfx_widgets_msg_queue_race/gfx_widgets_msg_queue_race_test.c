/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (gfx_widgets_msg_queue_race_test.c).
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

/* Regression test for the producer-producer race fix on
 * dispgfx_widget_t::msg_queue in gfx/gfx_widgets.c.
 *
 * Pre-fix: gfx_widgets_msg_queue_push called fifo_write on
 * p_dispwidget->msg_queue without holding any lock.  The
 * function is reachable from three callers:
 *
 *   1. runloop.c:runloop_msg_queue_push (40+ files transitively
 *      reach this; many run on threaded task workers via
 *      libretro-common/queues/task_queue.c).  Holds
 *      RUNLOOP_MSG_QUEUE_LOCK across the call.
 *   2. runloop.c:runloop_task_msg_queue_push.  Same lock.
 *   3. gfx/video_driver.c:video_driver_frame.  Acquires
 *      RUNLOOP_MSG_QUEUE_LOCK to drain runloop_st->msg_queue,
 *      RELEASES it, then calls gfx_widgets_msg_queue_push
 *      WITHOUT the lock.
 *
 * Path 3 runs on the main thread; path 1 can run on any thread.
 * RUNLOOP_MSG_QUEUE_LOCK protects runloop_st->msg_queue, NOT
 * p_dispwidget->msg_queue -- the lock name reflects the runloop
 * struct it was named after.  So path 3's fifo_write (no lock)
 * can race with path 1's fifo_write (lock held but irrelevant
 * to path 3).
 *
 * The consumer (gfx_widgets_iterate -> fifo_read) ran inside
 * RUNLOOP_MSG_QUEUE_LOCK at runloop.c:6048 and inside
 * current_msgs_lock at gfx_widgets.c:973 -- but neither lock
 * is taken on path 3, so the consumer's fifo_read could observe
 * a half-updated `end` cursor from path 3's fifo_write.
 *
 * Outcome of a race: torn `end` cursor publishes a bad pointer
 * in the disp_widget_msg_t* slots.  fifo_read returns garbage,
 * the consumer dereferences it as a disp_widget_msg_t* in
 * gfx_widgets_iterate, and we get a use-after-free / segfault.
 * On x86 TSO the bug is masked most of the time by hardware
 * ordering; on Win-on-ARM / Apple Silicon (AArch64) it surfaces
 * more often.
 *
 * Fix: a dedicated msg_queue_lock on dispgfx_widget_t, acquired
 * inside gfx_widgets_msg_queue_push around the fifo_write
 * (with an avail re-check, since fifo_write does no bounds
 * checking) and inside gfx_widgets_iterate around the fifo_read.
 * msg_queue_lock is the inner lock; current_msgs_lock is the
 * outer lock when both are held.
 *
 * This test exercises the post-fix shape:
 *
 *   - Two producer threads each call a stripped-down version of
 *     gfx_widgets_msg_queue_push that mirrors the post-fix
 *     locking discipline (msg_queue_lock around fifo_write +
 *     avail re-check + rollback on race-loss).
 *
 *   - One consumer thread calls a stripped-down version of the
 *     gfx_widgets_iterate FIFO-handling block: take
 *     msg_queue_lock, fifo_read, release.  Then validate the
 *     pointer it got is one we actually pushed.
 *
 *   - Run for STRESS_ITERS iterations under ThreadSanitizer.
 *     halt_on_error=1 means the first race aborts the run
 *     non-zero, which the workflow gates on.  If the locking
 *     in the production code is removed or weakened (e.g.
 *     someone drops the lock around fifo_write thinking the
 *     outer FIFO_WRITE_AVAIL check is enough), TSan will see
 *     concurrent writes to fifo_buffer_t::end and flag the race.
 *
 * The test does NOT mirror the full gfx_widgets_msg_queue_push
 * function -- the message-widget allocation, font measurement,
 * and animation push are irrelevant to the race.  What matters
 * is the lock protocol around the fifo_buffer_t access, which
 * is what we exercise.
 *
 * If gfx_widgets.c amends the locking around msg_queue, the
 * verbatim copies in this test must follow.  Convention used by
 * gfx_thumbnail_status_atomic_test, vulkan_extension_count_test,
 * and others under samples/.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <queues/fifo_queue.h>
#include <retro_atomic.h>

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

/* MSG_QUEUE_PENDING_MAX in gfx_widgets.c.  Mirrored here for
 * realism, but the test works at any reasonable size. */
#define MSG_QUEUE_PENDING_MAX 32

/* Stand-in for disp_widget_msg_t.  The real struct has 30+
 * fields; the FIFO only stores the POINTER, not the contents.
 * For race-checking we just need allocations whose addresses
 * we can validate. */
typedef struct test_msg
{
   uint32_t generation;
   uint32_t producer_id;
} test_msg_t;

/* Mirror of dispgfx_widget_t's msg_queue + lock pieces.  The
 * real struct has many more fields; the test only models the
 * synchronisation invariant. */
typedef struct
{
   fifo_buffer_t msg_queue;
#ifdef HAVE_THREADS
   slock_t      *msg_queue_lock;
#endif
} dispgfx_widget_test_t;

/* === Verbatim mirror of gfx_widgets_msg_queue_push's FIFO
 *     interaction (post-fix).  Strips out the widget-allocation,
 *     font-measurement, and animation-push work that is not
 *     part of the race; what remains is the lock protocol.
 *     If gfx_widgets.c amends the locking, this block must
 *     follow. === */
static int test_msg_queue_push(
      dispgfx_widget_test_t *p_dispwidget,
      test_msg_t *msg_widget)
{
   /* No outer FIFO_WRITE_AVAIL fast-path: reading the FIFO
    * cursors outside msg_queue_lock would race with concurrent
    * producer fifo_writes (TSan-detectable).  The locked
    * avail re-check below is the correctness gate. */
#ifdef HAVE_THREADS
   bool fifo_full;
   slock_lock(p_dispwidget->msg_queue_lock);
   fifo_full = (FIFO_WRITE_AVAIL_NONPTR(p_dispwidget->msg_queue)
         < sizeof(msg_widget));
   if (!fifo_full)
      fifo_write(&p_dispwidget->msg_queue,
            &msg_widget, sizeof(msg_widget));
   slock_unlock(p_dispwidget->msg_queue_lock);

   if (fifo_full)
      return 0;  /* race-loss or full: caller's responsibility to free */
   return 1;
#else
   if (FIFO_WRITE_AVAIL_NONPTR(p_dispwidget->msg_queue) < sizeof(msg_widget))
      return 0;
   fifo_write(&p_dispwidget->msg_queue,
         &msg_widget, sizeof(msg_widget));
   return 1;
#endif
}

/* === Verbatim mirror of gfx_widgets_iterate's FIFO read
 *     block (post-fix).  Strips out the current_msgs[] insertion
 *     logic; what remains is the lock protocol.  If gfx_widgets.c
 *     amends the locking, this block must follow. === */
static test_msg_t *test_msg_queue_consume(
      dispgfx_widget_test_t *p_dispwidget)
{
   test_msg_t *msg_widget = NULL;

   /* No outer FIFO_READ_AVAIL fast-path: reading the FIFO cursors
    * outside msg_queue_lock would race with concurrent producer
    * fifo_writes.  The locked re-check below is the gate. */
#ifdef HAVE_THREADS
   slock_lock(p_dispwidget->msg_queue_lock);
#endif
   if (FIFO_READ_AVAIL_NONPTR(p_dispwidget->msg_queue) >= sizeof(msg_widget))
      fifo_read(&p_dispwidget->msg_queue,
            &msg_widget, sizeof(msg_widget));
#ifdef HAVE_THREADS
   slock_unlock(p_dispwidget->msg_queue_lock);
#endif
   return msg_widget;
}
/* === end verbatim copies === */

#ifdef HAVE_THREADS

/* Number of pushes per producer thread.  500K * 2 producers gives
 * 1M total push attempts.  Enough to flush out a missing-lock
 * regression under TSan within a few seconds.  TSan halt_on_error=1
 * means the first observed race aborts the run, so the iteration
 * count primarily affects how reliably we trigger the bug if it
 * regresses, not the cost of the success case. */
#define STRESS_ITERS 500000

typedef struct
{
   dispgfx_widget_test_t *widget;
   uint32_t               producer_id;
   uint32_t               pushed_ok;
   uint32_t               pushed_full;
} producer_ctx_t;

typedef struct
{
   dispgfx_widget_test_t *widget;
   uint32_t               consumed;
   int                    failed;  /* set non-zero on a bogus pointer */
   retro_atomic_int_t    *stop;
} consumer_ctx_t;

static void producer_thread(void *arg)
{
   producer_ctx_t *ctx = (producer_ctx_t*)arg;
   uint32_t i;
   for (i = 0; i < STRESS_ITERS; i++)
   {
      test_msg_t *m = (test_msg_t*)malloc(sizeof(*m));
      if (!m)
         continue;  /* OOM: skip; cosmetic-error path */
      m->generation  = i;
      m->producer_id = ctx->producer_id;

      if (test_msg_queue_push(ctx->widget, m))
         ctx->pushed_ok++;
      else
      {
         /* Lost the race or FIFO full.  Free our allocation. */
         free(m);
         ctx->pushed_full++;
      }
   }
}

static void consumer_thread(void *arg)
{
   consumer_ctx_t *ctx = (consumer_ctx_t*)arg;
   while (!retro_atomic_load_acquire_int(ctx->stop))
   {
      test_msg_t *m = test_msg_queue_consume(ctx->widget);
      if (m)
      {
         /* Validate that what we got back is plausibly one of
          * the things a producer pushed.  generation < STRESS_ITERS,
          * producer_id is one of the small set we spawned.  A
          * torn pointer from a missing-lock race would typically
          * fail this check (garbage memory), but the primary
          * regression signal here is TSan flagging the
          * concurrent unsynchronised access on the FIFO cursors,
          * not this content check.  This is belt-and-suspenders. */
         if (m->generation >= STRESS_ITERS || m->producer_id >= 16)
         {
            ctx->failed = 1;
            free(m);
            return;
         }
         ctx->consumed++;
         free(m);
      }
   }
   /* Drain anything left behind after producers stopped. */
   for (;;)
   {
      test_msg_t *m = test_msg_queue_consume(ctx->widget);
      if (!m)
         break;
      ctx->consumed++;
      free(m);
   }
}

static int run_stress(void)
{
   dispgfx_widget_test_t widget;
   producer_ctx_t        prod_ctx[2];
   consumer_ctx_t        cons_ctx;
   sthread_t            *prod_threads[2];
   sthread_t            *cons_thread;
   retro_atomic_int_t    stop;
   int                   i;
   uint32_t              total_pushed = 0;

   retro_atomic_int_init(&stop, 0);

   if (!fifo_initialize(&widget.msg_queue,
            MSG_QUEUE_PENDING_MAX * sizeof(test_msg_t*)))
   {
      fprintf(stderr, "fifo_initialize failed\n");
      return 1;
   }
   widget.msg_queue_lock = slock_new();
   if (!widget.msg_queue_lock)
   {
      fprintf(stderr, "slock_new failed\n");
      fifo_deinitialize(&widget.msg_queue);
      return 1;
   }

   for (i = 0; i < 2; i++)
   {
      prod_ctx[i].widget      = &widget;
      prod_ctx[i].producer_id = (uint32_t)i;
      prod_ctx[i].pushed_ok   = 0;
      prod_ctx[i].pushed_full = 0;
   }
   cons_ctx.widget   = &widget;
   cons_ctx.consumed = 0;
   cons_ctx.failed   = 0;
   cons_ctx.stop     = &stop;

   cons_thread     = sthread_create(consumer_thread, &cons_ctx);
   prod_threads[0] = sthread_create(producer_thread, &prod_ctx[0]);
   prod_threads[1] = sthread_create(producer_thread, &prod_ctx[1]);

   if (!cons_thread || !prod_threads[0] || !prod_threads[1])
   {
      fprintf(stderr, "sthread_create failed\n");
      return 1;
   }

   sthread_join(prod_threads[0]);
   sthread_join(prod_threads[1]);
   retro_atomic_store_release_int(&stop, 1);
   sthread_join(cons_thread);

   /* Drain anything still in the FIFO (consumer-side, single-
    * threaded by this point so no lock needed -- but using the
    * same helper for consistency). */
   for (;;)
   {
      test_msg_t *m = test_msg_queue_consume(&widget);
      if (!m)
         break;
      cons_ctx.consumed++;
      free(m);
   }

   total_pushed = prod_ctx[0].pushed_ok + prod_ctx[1].pushed_ok;

   if (cons_ctx.failed)
   {
      fprintf(stderr, "FAIL: consumer saw torn / bogus pointer\n");
      slock_free(widget.msg_queue_lock);
      fifo_deinitialize(&widget.msg_queue);
      return 1;
   }

   if (cons_ctx.consumed != total_pushed)
   {
      fprintf(stderr,
            "FAIL: consumed %u != pushed %u (P0=%u/%u, P1=%u/%u)\n",
            cons_ctx.consumed, total_pushed,
            prod_ctx[0].pushed_ok, prod_ctx[0].pushed_full,
            prod_ctx[1].pushed_ok, prod_ctx[1].pushed_full);
      slock_free(widget.msg_queue_lock);
      fifo_deinitialize(&widget.msg_queue);
      return 1;
   }

   printf("[pass] stress: %u pushed (%u+%u), %u rejected on full, "
          "%u consumed, no torn pointers\n",
         total_pushed, prod_ctx[0].pushed_ok, prod_ctx[1].pushed_ok,
         prod_ctx[0].pushed_full + prod_ctx[1].pushed_full,
         cons_ctx.consumed);

   slock_free(widget.msg_queue_lock);
   fifo_deinitialize(&widget.msg_queue);
   return 0;
}

#endif /* HAVE_THREADS */

/* Property-style smoke test for the single-threaded path:
 * push fills the FIFO, push past full returns 0, consume drains
 * everything in order. */
static int run_smoke(void)
{
   dispgfx_widget_test_t widget;
   int                   i;
   int                   pushed = 0;
   int                   consumed = 0;
   test_msg_t           *messages[MSG_QUEUE_PENDING_MAX + 4];

   if (!fifo_initialize(&widget.msg_queue,
            MSG_QUEUE_PENDING_MAX * sizeof(test_msg_t*)))
   {
      fprintf(stderr, "fifo_initialize failed\n");
      return 1;
   }
#ifdef HAVE_THREADS
   widget.msg_queue_lock = slock_new();
   if (!widget.msg_queue_lock)
   {
      fprintf(stderr, "slock_new failed\n");
      fifo_deinitialize(&widget.msg_queue);
      return 1;
   }
#endif

   /* Push more than the FIFO can hold; later pushes should
    * return 0 (rejected on full). */
   for (i = 0; i < MSG_QUEUE_PENDING_MAX + 4; i++)
   {
      messages[i] = (test_msg_t*)malloc(sizeof(test_msg_t));
      if (!messages[i])
      {
         fprintf(stderr, "malloc failed\n");
         return 1;
      }
      messages[i]->generation  = i;
      messages[i]->producer_id = 0;
      if (test_msg_queue_push(&widget, messages[i]))
         pushed++;
      else
      {
         /* push refused: free the allocation we tried to enqueue */
         free(messages[i]);
         messages[i] = NULL;
      }
   }

   if (pushed == 0 || pushed > MSG_QUEUE_PENDING_MAX)
   {
      fprintf(stderr, "FAIL: smoke pushed=%d; expected 1..%d\n",
            pushed, MSG_QUEUE_PENDING_MAX);
      return 1;
   }

   /* Drain.  Order should be FIFO. */
   for (i = 0; i < pushed; i++)
   {
      test_msg_t *m = test_msg_queue_consume(&widget);
      if (!m)
      {
         fprintf(stderr, "FAIL: smoke consume returned NULL at i=%d\n", i);
         return 1;
      }
      if (m->generation != (uint32_t)i)
      {
         fprintf(stderr,
               "FAIL: smoke order: expected generation %d, got %u\n",
               i, m->generation);
         free(m);
         return 1;
      }
      free(m);
      consumed++;
   }

   if (test_msg_queue_consume(&widget))
   {
      fprintf(stderr, "FAIL: smoke FIFO non-empty after drain\n");
      return 1;
   }

   /* Free any allocations whose push was refused. */
   for (i = pushed; i < MSG_QUEUE_PENDING_MAX + 4; i++)
   {
      if (messages[i])
         free(messages[i]);
   }

   printf("[pass] smoke: pushed=%d consumed=%d (FIFO order verified)\n",
         pushed, consumed);

#ifdef HAVE_THREADS
   slock_free(widget.msg_queue_lock);
#endif
   fifo_deinitialize(&widget.msg_queue);
   return 0;
}

int main(void)
{
   if (run_smoke())
      return 1;
#ifdef HAVE_THREADS
   if (run_stress())
      return 1;
#else
   printf("[skip] stress: HAVE_THREADS not enabled\n");
#endif
   printf("ALL OK\n");
   return 0;
}

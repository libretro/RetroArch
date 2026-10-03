/* Regression test for the widget message ring in gfx/gfx_widgets.c.
 *
 * The ring is a single-producer, single-consumer queue of widget
 * pointers with no lock: gfx_widgets_msg_queue_push() on the main
 * thread publishes through msg_queue_tail, the widgets' owner pops
 * through msg_queue_head, each cursor released by its writer and
 * acquired by the other side.  This test builds the production ring
 * code itself - gfx_widgets_pending_push() and
 * gfx_widgets_pending_pop() straight out of gfx_widgets.c - and drives
 * it with one producer thread and one consumer thread:
 *
 *   - every pointer the consumer takes is one the producer pushed,
 *     in push order, with none lost and none duplicated;
 *   - a full ring refuses the push rather than overwriting;
 *   - the consumer's role hands over between threads the way the
 *     widgets do at deinit: the producer stops, the main thread
 *     drains what is left.
 *
 * Under ThreadSanitizer a weakened cursor store or load shows as a
 * race on the slot array; the value checks show a lost or reordered
 * pointer.  halt_on_error=1 fails the run on the first race.
 *
 * It also pins that the main thread's 'persisting' and the drawing
 * thread's flags are separate memory: closing content clears the
 * former while a message animation ends on the video thread and
 * clears DISPGFX_WIDGET_FLAG_MOVING in the latter.
 *
 * Build:  make  (SANITIZER=thread for the checked run)
 * Run:    ./gfx_widgets_msg_queue_race_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../../../gfx/gfx_widgets.c"

#include <rthreads/rthreads.h>

#define STRESS_ITERS 200000

typedef struct test_msg
{
   disp_widget_msg_t widget;   /* what the ring stores a pointer to */
   uint32_t seq;
} test_msg_t;

static test_msg_t *pool;
static retro_atomic_int_t producer_done;
static unsigned pushed;
static unsigned popped;
static unsigned refused;
static int test_fails;

static void producer(void *data)
{
   uint32_t i;
   (void)data;
   for (i = 0; i < STRESS_ITERS; i++)
   {
      pool[i].seq = i;
      /* A full ring refuses; wait for the consumer rather than drop */
      while (!gfx_widgets_pending_push(&dispwidget_st, &pool[i].widget))
      {
         refused++;
         sthread_yield();
      }
      pushed++;
   }
   retro_atomic_store_release_int(&producer_done, 1);
}

/* Takes every pointer the ring holds, checking the sequence; returns
 * false at the first hole */
static void animation_ends(void *data)
{
   unsigned i;
   (void)data;
   for (i = 0; i < STRESS_ITERS; i++)
   {
      dispwidget_st.flags |= DISPGFX_WIDGET_FLAG_MOVING;
      gfx_widgets_move_end(NULL);
   }
}

static bool drain(uint32_t *expect)
{
   disp_widget_msg_t *w;
   while ((w = gfx_widgets_pending_pop(&dispwidget_st)))
   {
      test_msg_t *m = (test_msg_t*)w;
      if (m->seq != *expect)
      {
         printf("FAIL: popped seq %u, expected %u\n", m->seq, *expect);
         return false;
      }
      (*expect)++;
      popped++;
   }
   return true;
}

static void consumer(void *data)
{
   uint32_t expect = 0;
   (void)data;
   for (;;)
   {
      if (!drain(&expect))
      {
         test_fails++;
         return;
      }
      /* The producer is done: whatever it pushed after this thread's
       * last look is the main thread's to drain, as at deinit */
      if (retro_atomic_load_acquire_int(&producer_done))
         return;
      sthread_yield();
   }
}

int main(void)
{
   sthread_t *p;
   sthread_t *c;
   uint32_t expect;
   unsigned i;

   pool = (test_msg_t*)calloc(STRESS_ITERS, sizeof(*pool));
   if (!pool)
      return 1;

   retro_atomic_int_init(&dispwidget_st.msg_queue_head, 0);
   retro_atomic_int_init(&dispwidget_st.msg_queue_tail, 0);
   retro_atomic_int_init(&producer_done, 0);

   /* Full ring refuses: fill it from this thread, no consumer running */
   for (i = 0; i < MSG_QUEUE_PENDING_MAX; i++)
      if (!gfx_widgets_pending_push(&dispwidget_st, &pool[i].widget))
      {
         printf("FAIL: push %u refused before the ring was full\n", i);
         test_fails++;
      }
   if (gfx_widgets_pending_push(&dispwidget_st, &pool[0].widget))
   {
      printf("FAIL: a full ring accepted a push\n");
      test_fails++;
   }
   for (i = 0; i < MSG_QUEUE_PENDING_MAX; i++)
   {
      disp_widget_msg_t *w = gfx_widgets_pending_pop(&dispwidget_st);
      if (w != &pool[i].widget)
      {
         printf("FAIL: pop %u returned the wrong pointer\n", i);
         test_fails++;
      }
   }
   if (gfx_widgets_pending_pop(&dispwidget_st))
   {
      printf("FAIL: an empty ring returned a pointer\n");
      test_fails++;
   }
   printf("ok:   full ring refuses, order kept through a wrap\n");

   /* Cross-thread stress: one producer, one consumer */
   pushed = popped = refused = 0;
   p = sthread_create(producer, NULL);
   c = sthread_create(consumer, NULL);
   if (!p || !c)
      return 1;
   sthread_join(p);
   sthread_join(c);

   /* Consumer role hands to this thread: drain whatever is left */
   expect = popped;
   if (!drain(&expect))
      test_fails++;

   if (popped != STRESS_ITERS)
   {
      printf("FAIL: pushed %u, popped %u\n", pushed, popped);
      test_fails++;
   }
   else
      printf("ok:   %u pointers crossed in order (%u refusals on full)\n",
            popped, refused);

   /* The video thread ends animations while the main thread closes
    * content and reinits drivers */
   p = sthread_create(animation_ends, NULL);
   if (!p)
      return 1;
   for (i = 0; i < STRESS_ITERS; i++)
      dispwidget_st.persisting = !(i & 1);
   sthread_join(p);
   if (dispwidget_st.flags & DISPGFX_WIDGET_FLAG_MOVING)
   {
      printf("FAIL: an ended animation left the queue moving\n");
      test_fails++;
   }
   else
      printf("ok:   persisting and the drawing thread's flags do not race\n");

   free(pool);

   if (test_fails)
   {
      printf("== %d FAILURES ==\n", test_fails);
      return 1;
   }
   printf("== gfx_widgets_msg_queue_race_test: all tests pass ==\n");
   return 0;
}

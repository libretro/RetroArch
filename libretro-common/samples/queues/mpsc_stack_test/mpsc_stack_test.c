/* The many-producer single-consumer stack's contract: every push is
 * drained exactly once, a producer's own pushes come back in their
 * push order once the batch is reversed, the payload a producer wrote
 * before the push is what the consumer reads after the drain, and the
 * empty probe agrees with the drain.  Producers run concurrently with
 * a consumer draining on its own cadence; run under TSan to hold the
 * ordering claims. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <queues/mpsc_stack.h>
#include <rthreads/rthreads.h>
#include <retro_atomic.h>

#define PRODUCERS   4
#define PER_THREAD  20000

struct test_node
{
   mpsc_stack_node_t link;
   unsigned producer;
   unsigned seq;
   unsigned payload; /* producer ^ seq: written before push, read after drain */
};

static mpsc_stack_t stack;
static retro_atomic_int_t producers_done;

static void producer(void *arg)
{
   unsigned id = (unsigned)(uintptr_t)arg;
   unsigned i;
   for (i = 0; i < PER_THREAD; i++)
   {
      struct test_node *node = (struct test_node *)malloc(sizeof(*node));
      if (!node)
         exit(2);
      node->producer = id;
      node->seq      = i;
      node->payload  = id ^ i;
      mpsc_stack_push(&stack, &node->link);
   }
   retro_atomic_fetch_add_int(&producers_done, 1);
}

int main(void)
{
   sthread_t *threads[PRODUCERS];
   unsigned next_seq[PRODUCERS];
   unsigned long drained = 0;
   unsigned failures    = 0;
   unsigned i;

   mpsc_stack_init(&stack);
   retro_atomic_int_init(&producers_done, 0);
   for (i = 0; i < PRODUCERS; i++)
      next_seq[i] = 0;

   for (i = 0; i < PRODUCERS; i++)
      threads[i] = sthread_create(producer, (void *)(uintptr_t)i);

   /* Drain on the consumer's own cadence while producers run, then
    * once more after they are done: the shape of an iterate. */
   for (;;)
   {
      bool done = retro_atomic_load_acquire_int(&producers_done)
            == PRODUCERS;
      mpsc_stack_node_t *link;

      if (!mpsc_stack_empty(&stack))
      {
         link = mpsc_stack_reverse(mpsc_stack_drain(&stack));
         while (link)
         {
            struct test_node *node = (struct test_node *)link;
            link = link->next;
            if (node->producer >= PRODUCERS)
            {
               failures++;
            }
            else
            {
               /* a producer's own pushes replay in push order */
               if (node->seq != next_seq[node->producer])
                  failures++;
               next_seq[node->producer] = node->seq + 1;
               if (node->payload != (node->producer ^ node->seq))
                  failures++;
            }
            drained++;
            free(node);
         }
      }
      else if (done)
      {
         /* empty agrees with drain when the world is quiet */
         if (mpsc_stack_drain(&stack))
            failures++;
         break;
      }
   }

   for (i = 0; i < PRODUCERS; i++)
      sthread_join(threads[i]);
   for (i = 0; i < PRODUCERS; i++)
      if (next_seq[i] != PER_THREAD)
         failures++;
   if (drained != (unsigned long)PRODUCERS * PER_THREAD)
      failures++;

   if (failures)
   {
      printf("FAIL mpsc_stack_test: %u failures, %lu drained\n",
            failures, drained);
      return 1;
   }
   printf("PASS mpsc_stack_test: %lu pushes drained exactly once, in order\n",
         drained);
   return 0;
}

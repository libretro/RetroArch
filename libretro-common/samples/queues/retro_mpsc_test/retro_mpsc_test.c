/* retro_mpsc: several producers push, one consumer takes batches.
 *
 * Every node pushed is taken exactly once, each producer's nodes come
 * out in the order it pushed them, and a batch is in push order. A
 * node handed back by the consumer is pushed again at once by its
 * producer, so reuse right after take_all is exercised too.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <retro_mpsc.h>
#include <rthreads/rthreads.h>

#define PRODUCERS 4
#define PER       200000u
#define POOL      64u

typedef struct item
{
   retro_mpsc_node_t node;   /* first: a node is its item */
   unsigned          producer;
   uint32_t          seq;
} item_t;

static retro_mpsc_t        q;
static item_t              pool[PRODUCERS][POOL];
/* nodes the consumer has finished with, per producer: a count each
 * side moves on, so a producer reuses its oldest returned node */
static retro_atomic_int_t  returned[PRODUCERS];

static void producer(void *arg)
{
   unsigned p = (unsigned)(uintptr_t)arg;
   uint32_t seq;
   for (seq = 0; seq < PER; seq++)
   {
      item_t *it = &pool[p][seq % POOL];
      /* wait until the consumer is done with this slot */
      while (seq >= POOL
            && (uint32_t)retro_atomic_load_acquire_int(&returned[p]) + POOL <= seq)
         ;
      it->producer = p;
      it->seq      = seq;
      retro_mpsc_push(&q, &it->node);
   }
}

int main(void)
{
   sthread_t *t[PRODUCERS];
   uint32_t   next[PRODUCERS];
   unsigned   p, taken = 0, batches = 0;
   int        bad = 0;

   retro_mpsc_init(&q);
   if (!retro_mpsc_empty(&q) || retro_mpsc_take_all(&q))
   {
      printf("FAIL: a new queue is not empty\n");
      return 1;
   }
   for (p = 0; p < PRODUCERS; p++)
   {
      next[p] = 0;
      retro_atomic_int_init(&returned[p], 0);
   }
   for (p = 0; p < PRODUCERS; p++)
      t[p] = sthread_create(producer, (void*)(uintptr_t)p);

   while (taken < PRODUCERS * PER && !bad)
   {
      retro_mpsc_node_t *n = retro_mpsc_take_all(&q);
      if (n)
         batches++;
      while (n)
      {
         retro_mpsc_node_t *after = n->next;
         item_t *it = (item_t*)n;
         if (it->producer >= PRODUCERS || it->seq != next[it->producer])
         {
            printf("FAIL: producer %u item %u out of order (expected %u)\n",
                  it->producer, (unsigned)it->seq,
                  it->producer < PRODUCERS ? (unsigned)next[it->producer] : 0u);
            bad = 1;
            break;
         }
         next[it->producer]++;
         taken++;
         /* hand the node back: its producer may push it again now */
         retro_atomic_fetch_add_int(&returned[it->producer], 1);
         n = after;
      }
   }
   for (p = 0; p < PRODUCERS; p++)
      sthread_join(t[p]);
   if (!bad && (!retro_mpsc_empty(&q) || taken != PRODUCERS * PER))
   {
      printf("FAIL: %u taken of %u, queue %s\n", taken, PRODUCERS * PER,
            retro_mpsc_empty(&q) ? "empty" : "not empty");
      bad = 1;
   }
   printf("%u items in %u batches\n", taken, batches);
   printf(bad ? "retro_mpsc_test: FAIL\n" : "retro_mpsc_test: PASS\n");
   return bad;
}

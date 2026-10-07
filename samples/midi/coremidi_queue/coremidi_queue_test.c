/* midi/drivers/coremidi_queue.h, the CoreMIDI driver's input queue from
 * CoreMIDI's receive thread to the thread that calls read().
 *
 * read() hands out a pointer into the queue, and the caller uses it
 * after read() returns. What is asserted:
 *  - an event read is not overwritten while the caller still has it,
 *    with the producer flooding a full queue the whole time,
 *  - events come out once each, in order, none invented,
 *  - a clear from another thread empties the queue at the next read,
 *    and what is written after it still arrives. */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#include "midi/drivers/coremidi_queue.h"

#define EVENTS 200000

static unsigned failures;
#define CHECK(cond, ...) \
   do { if (!(cond)) { printf("FAIL line %d: ", __LINE__); \
        printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

static coremidi_queue_t q;
static retro_atomic_int_t produced;

/* Event n: its number in the first four bytes, the rest a pattern of
 * it, and a size that varies with it. */
static size_t make_event(uint32_t n, uint8_t *buf)
{
   size_t size = 4 + (n % (COREMIDI_MAX_EVENT_SIZE - 4));
   size_t i;
   memcpy(buf, &n, 4);
   for (i = 4; i < size; i++)
      buf[i] = (uint8_t)(n + i);
   return size;
}

static int event_intact(const coremidi_event_t *ev, uint32_t *n)
{
   uint8_t want[COREMIDI_MAX_EVENT_SIZE];
   size_t size;
   memcpy(n, ev->data, 4);
   size = make_event(*n, want);
   return ev->data_size == size && ev->delta_time == *n
       && !memcmp(ev->data, want, size);
}

static void *producer(void *arg)
{
   uint8_t buf[COREMIDI_MAX_EVENT_SIZE];
   uint32_t n = 0;
   (void)arg;
   while (n < EVENTS)
   {
      size_t size = make_event(n, buf);
      if (coremidi_queue_write(&q, buf, size, n))
      {
         n++;
         retro_atomic_store_release_int(&produced, (int)n);
      }
   }
   return NULL;
}

static void test_held_event_survives(void)
{
   pthread_t thread;
   uint32_t expect = 0, overwritten = 0, torn = 0, order = 0;
   unsigned spins = 0;

   coremidi_queue_init(&q);
   retro_atomic_store_release_int(&produced, 0);
   pthread_create(&thread, NULL, producer, NULL);

   while (expect < EVENTS)
   {
      const coremidi_event_t *ev = coremidi_queue_read(&q);
      uint32_t n, again;
      volatile unsigned k;
      if (!ev)
      {
         if (++spins > 200000000u)
            break;
         continue;
      }
      if (!event_intact(ev, &n))
      {
         torn++;
         expect++;
         continue;
      }
      if (n != expect)
         order++;
      expect = n + 1;
      /* The caller is still using the event: give the producer, which
       * is filling every free slot, time to come round to this one. */
      for (k = 0; k < 2000; k++)
         ;
      if (!event_intact(ev, &again) || again != n)
         overwritten++;
   }
   pthread_join(thread, NULL);

   CHECK(expect == EVENTS, "read up to event %u of %u", expect, EVENTS);
   CHECK(torn == 0, "%u events read torn", torn);
   CHECK(order == 0, "%u events out of order or missing", order);
   CHECK(overwritten == 0,
         "%u events overwritten while the caller still held them",
         overwritten);
   CHECK(coremidi_queue_read(&q) == NULL, "an event invented at the end");
}

static void test_clear(void)
{
   uint8_t buf[COREMIDI_MAX_EVENT_SIZE];
   const coremidi_event_t *ev;
   uint32_t n;
   unsigned i;

   coremidi_queue_init(&q);
   for (i = 0; i < 10; i++)
      CHECK(coremidi_queue_write(&q, buf, make_event(i, buf), i),
            "write %u refused", i);
   ev = coremidi_queue_read(&q);
   CHECK(ev && event_intact(ev, &n) && n == 0, "first event");

   /* Cleared from another thread while the consumer holds event 0. */
   coremidi_queue_clear(&q);
   CHECK(coremidi_queue_read(&q) == NULL, "the clear left events behind");

   CHECK(coremidi_queue_write(&q, buf, make_event(42, buf), 42),
         "write after clear refused");
   ev = coremidi_queue_read(&q);
   CHECK(ev && event_intact(ev, &n) && n == 42,
         "the event written after the clear did not arrive");
   CHECK(coremidi_queue_read(&q) == NULL, "more than was written");

   /* Full means full: one slot is the gap, the rest hold events. */
   coremidi_queue_init(&q);
   for (i = 0; i < COREMIDI_QUEUE_SIZE - 1; i++)
      CHECK(coremidi_queue_write(&q, buf, make_event(i, buf), i),
            "write %u refused below capacity", i);
   CHECK(!coremidi_queue_write(&q, buf, make_event(i, buf), i),
         "a full queue took another event");
   CHECK(!coremidi_queue_write(&q, buf, 0, 0), "an empty event was taken");
}

int main(void)
{
   test_clear();
   test_held_event_survives();
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] coremidi_queue_test (%u events)\n", EVENTS);
   return 0;
}

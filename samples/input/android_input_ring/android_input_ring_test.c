/* android_input_ring_test.c -- the Android input reader's ring.
 *
 * The input reader (input/drivers/android_input.c) writes each event
 * into a ring as a record of the size the event needs, and the poll
 * applies it where it lies (input/drivers/android_input_ring.h). The
 * claims:
 *
 *   1. A record is read where it was written: the poll's pointer is
 *      the reader's, inside the ring, and nothing is copied.
 *   2. A record never crosses the ring's end, lap after lap, whatever
 *      the sizes: the rest of a lap is a pad the poll steps over.
 *   3. Nothing is seen before the burst is published.
 *   4. A record with a merge key replaces the last one built when that
 *      has the same key and is unpublished; another key, a record in
 *      between or a publish keeps them apart.
 *   5. A full ring gives no slot, and room comes back once the poll
 *      gives records back.
 *   6. On two threads, every record arrives once, in order, whole.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <rthreads/rthreads.h>
#include <retro_atomic.h>

#include "android_input_ring.h"

static unsigned failures = 0;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

/* A record of the test's: a sequence number, and a byte pattern of it */
typedef struct
{
   android_input_rec_t rec;
   uint32_t seq;
   uint32_t value;
} test_rec_t;

static void fill(uint8_t *p, size_t size, uint32_t seq)
{
   size_t i;
   for (i = sizeof(test_rec_t); i < size; i++)
      p[i] = (uint8_t)(seq * 7 + i);
}

static bool whole(const uint8_t *p, size_t size, uint32_t seq)
{
   size_t i;
   for (i = sizeof(test_rec_t); i < size; i++)
      if (p[i] != (uint8_t)(seq * 7 + i))
         return false;
   return true;
}

static test_rec_t *put(android_input_ring_writer_t *w, size_t size,
      uint32_t merge, uint32_t seq, uint32_t value)
{
   test_rec_t *r = (test_rec_t*)android_input_ring_slot(w, size, merge);
   if (!r)
      return NULL;
   r->seq   = seq;
   r->value = value;
   fill((uint8_t*)r, size, seq);
   android_input_ring_built(w, size, merge);
   return r;
}

static bool in_ring(const retro_spsc_t *q, const void *p, size_t size)
{
   const uint8_t *b = (const uint8_t*)p;
   return b >= q->buffer && b + size <= q->buffer + q->capacity;
}

static void lane_in_place(void)
{
   retro_spsc_t q;
   android_input_ring_writer_t w;
   android_input_ring_reader_t r;
   const android_input_rec_t *rec;
   test_rec_t *a, *b;

   CHECK(retro_spsc_init(&q, 1024), "no ring");
   android_input_ring_writer_init(&w, &q);
   a = put(&w, 48, 0, 1, 10);
   b = put(&w, 128, 0, 2, 20);
   CHECK(a && b, "no slot in an empty ring");

   /* 3: not seen before it is published */
   android_input_ring_reader_begin(&r, &q);
   CHECK(!android_input_ring_next(&r), "a record was seen before it was published");
   android_input_ring_reader_end(&r);

   android_input_ring_publish(&w);
   android_input_ring_reader_begin(&r, &q);
   rec = android_input_ring_next(&r);
   CHECK(rec == &a->rec && in_ring(&q, rec, 48),
         "the first record was not read where it was written");
   CHECK(rec && rec->size == 48, "a record's size is not the one built");
   android_input_ring_taken(&r, rec);
   rec = android_input_ring_next(&r);
   CHECK(rec == &b->rec && rec && rec->size == 128,
         "the second record was not read where it was written");
   android_input_ring_taken(&r, rec);
   CHECK(!android_input_ring_next(&r), "a record that was never written was read");
   CHECK(android_input_ring_reader_end(&r), "what was read was not given back");
   CHECK(retro_spsc_read_avail(&q) == 0, "the ring is not empty after the poll");
   retro_spsc_free(&q);
}

static void lane_laps(void)
{
   retro_spsc_t q;
   android_input_ring_writer_t w;
   android_input_ring_reader_t r;
   uint32_t seq = 0, seen = 0;
   unsigned lap;
   bool split = false, order = true;

   CHECK(retro_spsc_init(&q, 256), "no ring");
   android_input_ring_writer_init(&w, &q);
   for (lap = 0; lap < 200; lap++)
   {
      const android_input_rec_t *rec;
      /* sizes that do not divide the ring */
      size_t size = 16 + (size_t)(lap % 9) * 8;
      while (put(&w, size, 0, seq + 1, 0))
         seq++;
      android_input_ring_publish(&w);
      android_input_ring_reader_begin(&r, &q);
      while ((rec = android_input_ring_next(&r)))
      {
         const test_rec_t *t = (const test_rec_t*)rec;
         if (!in_ring(&q, rec, rec->size) || !whole((const uint8_t*)rec, rec->size, t->seq))
            split = true;
         if (t->seq != seen + 1)
            order = false;
         seen = t->seq;
         android_input_ring_taken(&r, rec);
      }
      android_input_ring_reader_end(&r);
   }
   CHECK(!split, "a record crossed the ring's end, or was not whole");
   CHECK(order && seen == seq && seq > 400, "records were lost or out of order over the laps");
   retro_spsc_free(&q);
}

static void lane_merge(void)
{
   retro_spsc_t q;
   android_input_ring_writer_t w;
   android_input_ring_reader_t r;
   const android_input_rec_t *rec;
   uint32_t got[8];
   unsigned n = 0, i;

   CHECK(retro_spsc_init(&q, 1024), "no ring");
   android_input_ring_writer_init(&w, &q);
   put(&w, 88, 5, 1, 1);   /* a stick's moves, merged into the last */
   put(&w, 88, 5, 2, 2);
   put(&w, 88, 5, 3, 3);
   put(&w, 88, 6, 4, 4);   /* another key: kept */
   put(&w, 48, 0, 5, 5);   /* a key between */
   put(&w, 88, 6, 6, 6);   /* not over 4, a record came between */
   android_input_ring_publish(&w);
   put(&w, 88, 6, 7, 7);   /* not over 6, which is published */
   android_input_ring_publish(&w);

   android_input_ring_reader_begin(&r, &q);
   while ((rec = android_input_ring_next(&r)) && n < 8)
   {
      got[n++] = ((const test_rec_t*)rec)->value;
      android_input_ring_taken(&r, rec);
   }
   android_input_ring_reader_end(&r);
   CHECK(n == 5, "merging kept the wrong number of records");
   for (i = 0; i < n && i < 5; i++)
   {
      static const uint32_t want[5] = { 3, 4, 5, 6, 7 };
      CHECK(got[i] == want[i], "a merged record does not hold the last move,"
            " or records that must stay apart were merged");
   }
   retro_spsc_free(&q);
}

static void lane_full(void)
{
   retro_spsc_t q;
   android_input_ring_writer_t w;
   android_input_ring_reader_t r;
   const android_input_rec_t *rec;
   unsigned n = 0;

   CHECK(retro_spsc_init(&q, 256), "no ring");
   android_input_ring_writer_init(&w, &q);
   while (put(&w, 64, 0, n + 1, 0))
      n++;
   CHECK(n == 4, "a ring of 256 did not take four records of 64");
   CHECK(!android_input_ring_slot(&w, 64, 0), "a full ring gave a slot");
   android_input_ring_publish(&w);
   android_input_ring_reader_begin(&r, &q);
   rec = android_input_ring_next(&r);
   android_input_ring_taken(&r, rec);
   android_input_ring_reader_end(&r);
   CHECK(put(&w, 64, 0, 9, 0) != NULL, "room given back was not used");
   retro_spsc_free(&q);
}

/* ---- two threads ---- */

#define STRESS_RECORDS 200000

static retro_spsc_t      stress_q;
static retro_atomic_int_t stress_done;

static void stress_writer(void *data)
{
   android_input_ring_writer_t w;
   uint32_t seq = 1;
   (void)data;
   android_input_ring_writer_init(&w, &stress_q);
   while (seq <= STRESS_RECORDS)
   {
      size_t size = 16 + (size_t)(seq % 30) * 8;
      if (put(&w, size, 0, seq, seq))
      {
         seq++;
         /* a burst ends now and then */
         if (!(seq % 7))
            android_input_ring_publish(&w);
      }
      else
         android_input_ring_publish(&w);   /* full: the poll makes room */
   }
   android_input_ring_publish(&w);
   retro_atomic_store_release_int(&stress_done, 1);
}

static void lane_threads(void)
{
   sthread_t *t;
   uint32_t seen = 0;
   bool bad      = false;

   CHECK(retro_spsc_init(&stress_q, 4096), "no ring");
   retro_atomic_int_init(&stress_done, 0);
   if (!(t = sthread_create(stress_writer, NULL)))
   {
      CHECK(false, "no thread");
      return;
   }
   for (;;)
   {
      const android_input_rec_t *rec;
      android_input_ring_reader_t r;
      bool done = retro_atomic_load_acquire_int(&stress_done) != 0;
      android_input_ring_reader_begin(&r, &stress_q);
      while ((rec = android_input_ring_next(&r)))
      {
         const test_rec_t *tr = (const test_rec_t*)rec;
         if (     tr->seq != seen + 1
               || rec->size != 16 + (tr->seq % 30) * 8
               || !whole((const uint8_t*)rec, rec->size, tr->seq))
            bad = true;
         seen = tr->seq;
         android_input_ring_taken(&r, rec);
      }
      android_input_ring_reader_end(&r);
      if (done && !retro_spsc_read_avail(&stress_q))
         break;
   }
   sthread_join(t);
   CHECK(!bad, "on two threads a record arrived out of order, or not whole");
   CHECK(seen == STRESS_RECORDS, "on two threads records were lost");
   retro_spsc_free(&stress_q);
}

int main(void)
{
   lane_in_place();
   lane_laps();
   lane_merge();
   lane_full();
   lane_threads();
   if (failures)
   {
      fprintf(stderr, "FAIL android_input_ring_test: %u failures\n", failures);
      return 1;
   }
   fprintf(stderr, "PASS android_input_ring_test: read where written, never"
         " across the ring's end, merged only while unpublished,"
         " %u records whole and in order on two threads\n", STRESS_RECORDS);
   return 0;
}

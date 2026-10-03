/* retro_spsc_bench: cross-thread throughput of retro_spsc, current
 * implementation against a cached-remote-index variant.
 *
 * The question it answers: in retro_spsc every write() loads the
 * consumer's tail and every read() loads the producer's head, so each
 * operation pulls the other core's cache line.  The standard fix is
 * for each side to cache the remote index and only reload it when the
 * cached value says the ring is full/empty.  Whether that is worth
 * carrying depends entirely on the hardware, so measure it: run this
 * on a machine with at least two physical cores (it is meaningless on
 * one) and compare the two rows for each payload size.
 *
 *   $ make && ./retro_spsc_bench
 *
 * Reports ns per operation (one write plus one read of the payload)
 * and MB/s through the ring.  "cached" is the variant; if it is not
 * clearly faster for the small payloads on the cores you care about,
 * the change is not worth making. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <retro_spsc.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <features/features_cpu.h>

#if defined(_WIN32)
#include <windows.h>
#define bench_yield() SwitchToThread()
#else
#include <sched.h>
#define bench_yield() sched_yield()
#endif

#define RING_BYTES   (64 * 1024)
#ifndef TOTAL_BYTES
#define TOTAL_BYTES  (256u * 1024 * 1024)
#endif

/* ---- variant: cached remote indices ------------------------------- */

typedef struct
{
   uint8_t             *buffer;
   size_t               capacity;
   uint8_t              _pad0[64];
   retro_atomic_size_t  head;        /* producer writes */
   size_t               cached_tail; /* producer-private */
   uint8_t              _pad1[64];
   retro_atomic_size_t  tail;        /* consumer writes */
   size_t               cached_head; /* consumer-private */
   uint8_t              _pad2[64];
} cspsc_t;

static void cspsc_init(cspsc_t *q, size_t cap)
{
   q->buffer      = (uint8_t*)malloc(cap);
   q->capacity    = cap;
   q->cached_tail = 0;
   q->cached_head = 0;
   retro_atomic_size_init(&q->head, 0);
   retro_atomic_size_init(&q->tail, 0);
}

static size_t cspsc_write(cspsc_t *q, const void *data, size_t bytes)
{
   size_t head  = retro_atomic_load_relaxed_size(&q->head);
   size_t avail = q->capacity - (head - q->cached_tail);
   size_t mask, idx, first;
   if (avail < bytes)
   {
      /* Only now touch the consumer's line. */
      q->cached_tail = retro_atomic_load_acquire_size(&q->tail);
      avail          = q->capacity - (head - q->cached_tail);
      if (bytes > avail)
         bytes = avail;
      if (bytes == 0)
         return 0;
   }
   mask  = q->capacity - 1;
   idx   = head & mask;
   first = q->capacity - idx;
   if (first > bytes)
      first = bytes;
   memcpy(q->buffer + idx, data, first);
   memcpy(q->buffer, (const uint8_t*)data + first, bytes - first);
   retro_atomic_store_release_size(&q->head, head + bytes);
   return bytes;
}

static size_t cspsc_read(cspsc_t *q, void *data, size_t bytes)
{
   size_t tail  = retro_atomic_load_relaxed_size(&q->tail);
   size_t avail = q->cached_head - tail;
   size_t mask, idx, first;
   if (avail < bytes)
   {
      q->cached_head = retro_atomic_load_acquire_size(&q->head);
      avail          = q->cached_head - tail;
      if (bytes > avail)
         bytes = avail;
      if (bytes == 0)
         return 0;
   }
   mask  = q->capacity - 1;
   idx   = tail & mask;
   first = q->capacity - idx;
   if (first > bytes)
      first = bytes;
   memcpy(data, q->buffer + idx, first);
   memcpy((uint8_t*)data + first, q->buffer, bytes - first);
   retro_atomic_store_release_size(&q->tail, tail + bytes);
   return bytes;
}

/* ---- harness ------------------------------------------------------- */

typedef struct
{
   int      variant;      /* 0 = retro_spsc, 1 = cached */
   size_t   payload;
   size_t   total;
   retro_spsc_t *q;
   cspsc_t      *c;
   uint8_t  *buf;
} job_t;

static void producer(void *p)
{
   job_t *j    = (job_t*)p;
   size_t done = 0;
   while (done < j->total)
   {
      size_t n = j->variant
         ? cspsc_write(j->c, j->buf, j->payload)
         : retro_spsc_write(j->q, j->buf, j->payload);
      done += n;
      /* Full: give the other side a turn.  Matters on a shared core,
       * where a pure spin would only stall the consumer; with two
       * cores the yield is rarely hit and costs nothing. */
      if (!n)
         bench_yield();
   }
}

static void consumer(void *p)
{
   job_t *j    = (job_t*)p;
   size_t done = 0;
   while (done < j->total)
   {
      size_t n = j->variant
         ? cspsc_read(j->c, j->buf, j->payload)
         : retro_spsc_read(j->q, j->buf, j->payload);
      done += n;
      if (!n)
         bench_yield();
   }
}

static double run(int variant, size_t payload)
{
   retro_spsc_t q;
   cspsc_t      c;
   job_t        pj, cj;
   sthread_t   *pt, *ct;
   retro_time_t t0, t1;
   size_t total = TOTAL_BYTES - (TOTAL_BYTES % payload);

   retro_spsc_init(&q, RING_BYTES);
   cspsc_init(&c, RING_BYTES);

   pj.variant = cj.variant = variant;
   pj.payload = cj.payload = payload;
   pj.total   = cj.total   = total;
   pj.q = cj.q = &q;
   pj.c = cj.c = &c;
   pj.buf = (uint8_t*)calloc(1, payload);
   cj.buf = (uint8_t*)calloc(1, payload);

   t0 = cpu_features_get_time_usec();
   ct = sthread_create(consumer, &cj);
   pt = sthread_create(producer, &pj);
   sthread_join(pt);
   sthread_join(ct);
   t1 = cpu_features_get_time_usec();

   free(pj.buf); free(cj.buf);
   retro_spsc_free(&q);
   free(c.buffer);
   return (double)(t1 - t0);
}

int main(void)
{
   static const size_t sizes[] = { 8, 32, 128, 1024, 4096, 16384 };
   size_t i;
   int rep;

   printf("%8s  %-12s  %10s  %10s\n", "payload", "impl", "ns/op", "MB/s");
   for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
   {
      int v;
      for (v = 0; v < 2; v++)
      {
         double best = 1e30;
         for (rep = 0; rep < 3; rep++)
         {
            double us = run(v, sizes[i]);
            if (us < best)
               best = us;
         }
         {
            size_t total = TOTAL_BYTES - (TOTAL_BYTES % sizes[i]);
            double ops   = (double)total / (double)sizes[i];
            printf("%8u  %-12s  %10.1f  %10.0f\n", (unsigned)sizes[i],
                  v ? "cached" : "retro_spsc",
                  best * 1000.0 / ops,
                  (double)total / best);
         }
      }
   }
   return 0;
}

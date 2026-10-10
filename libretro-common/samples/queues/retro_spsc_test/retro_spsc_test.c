/* SPSC stress test: producer pushes N bytes total; consumer reads N
 * bytes; verify the byte stream is exactly an expected sequence.
 * Validates ordering, no torn reads, no duplicates, no drops.
 *
 * Design notes:
 *   - Producer writes incrementing 32-bit "tokens" (i = 1, 2, 3, ...).
 *   - Consumer reads the byte stream and reassembles tokens.
 *   - Each token is checked against expected sequence; mismatch =>
 *     reordering or torn write.
 *   - Producer/consumer race in tight loops with no per-iteration
 *     handshake; a real lock-free SPSC must handle this concurrency
 *     without external synchronisation.
 *   - Run under TSan for race detection.  Run without sanitizer for
 *     throughput sanity. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <retro_spsc.h>
#include <rthreads/rthreads.h>

/* Total tokens to push.  ~10M gives a reproducible test of <2s on
 * x86_64; under qemu-aarch64 and TSan it's ~30s. */
#define TOTAL_TOKENS 10000000

/* Buffer capacity in bytes.  Smaller -> more producer/consumer
 * interleaving (better race coverage); larger -> better throughput
 * but less torture. */
#define BUF_BYTES 4096

typedef struct
{
   retro_spsc_t  q;
   /* Captured by the producer's loop sentinel and read by main after
    * join, so it doesn't need to be atomic. */
   unsigned long mismatches;
   unsigned long produced_tokens;
   unsigned long consumed_tokens;
} test_state_t;

static void producer_thread(void *arg)
{
   test_state_t *s = (test_state_t*)arg;
   uint32_t      token;

   for (token = 1; token <= TOTAL_TOKENS; token++)
   {
      /* Spin until there is room for a full token; the yield keeps a
       * one-core host from spending each of this thread's slices on
       * the spin while the other side waits its turn. */
      while (retro_spsc_write_avail(&s->q) < sizeof(token))
         sthread_yield();
      retro_spsc_write(&s->q, &token, sizeof(token));
      s->produced_tokens++;
   }
}

static void consumer_thread(void *arg)
{
   test_state_t *s              = (test_state_t*)arg;
   uint32_t      expected_token = 1;

   while (expected_token <= TOTAL_TOKENS)
   {
      uint32_t got;
      while (retro_spsc_read_avail(&s->q) < sizeof(got))
         sthread_yield();
      retro_spsc_read(&s->q, &got, sizeof(got));
      if (got != expected_token)
         s->mismatches++;
      expected_token++;
      s->consumed_tokens++;
   }
}

static int run_stress(void)
{
   test_state_t s;
   sthread_t   *prod;
   sthread_t   *cons;

   memset(&s, 0, sizeof(s));
   if (!retro_spsc_init(&s.q, BUF_BYTES))
   {
      fprintf(stderr, "FAIL: retro_spsc_init\n");
      return 1;
   }

   prod = sthread_create(producer_thread, &s);
   if (!prod)
   {
      fprintf(stderr, "FAIL: sthread_create(producer)\n");
      retro_spsc_free(&s.q);
      return 1;
   }
   cons = sthread_create(consumer_thread, &s);
   if (!cons)
   {
      fprintf(stderr, "FAIL: sthread_create(consumer)\n");
      sthread_join(prod);
      retro_spsc_free(&s.q);
      return 1;
   }

   sthread_join(prod);
   sthread_join(cons);
   retro_spsc_free(&s.q);

   if (s.mismatches != 0)
   {
      fprintf(stderr,
         "FAIL: %lu mismatched tokens out of %lu\n",
         s.mismatches, s.consumed_tokens);
      return 1;
   }
   if (s.produced_tokens != TOTAL_TOKENS
         || s.consumed_tokens != TOTAL_TOKENS)
   {
      fprintf(stderr,
         "FAIL: produced=%lu consumed=%lu (expected %d each)\n",
         s.produced_tokens, s.consumed_tokens, TOTAL_TOKENS);
      return 1;
   }

   printf("[pass] stress: %d tokens through %d-byte buffer, "
          "0 mismatches\n",
      TOTAL_TOKENS, BUF_BYTES);
   return 0;
}

/* Single-threaded property checks: verify the API contracts that
 * don't require concurrency. */
static int run_property_checks(void)
{
   retro_spsc_t q;
   uint8_t      buf[64];
   uint8_t      readback[64];
   size_t       i, n;

   /* init with non-power-of-2 should round up */
   if (!retro_spsc_init(&q, 100))
   {
      fprintf(stderr, "FAIL: init(100)\n");
      return 1;
   }
   if (q.capacity != 128)
   {
      fprintf(stderr, "FAIL: capacity %zu != 128 (round up)\n",
         q.capacity);
      retro_spsc_free(&q);
      return 1;
   }

   /* fresh queue: read_avail = 0, write_avail = capacity */
   if (retro_spsc_read_avail(&q) != 0
         || retro_spsc_write_avail(&q) != 128)
   {
      fprintf(stderr, "FAIL: fresh-queue avails\n");
      retro_spsc_free(&q);
      return 1;
   }

   /* write 64, read 64, verify */
   for (i = 0; i < sizeof(buf); i++)
      buf[i] = (uint8_t)(i + 1);
   n = retro_spsc_write(&q, buf, sizeof(buf));
   if (n != sizeof(buf))
   {
      fprintf(stderr, "FAIL: write returned %zu\n", n);
      retro_spsc_free(&q);
      return 1;
   }
   if (retro_spsc_read_avail(&q) != sizeof(buf))
   {
      fprintf(stderr, "FAIL: read_avail after write\n");
      retro_spsc_free(&q);
      return 1;
   }
   memset(readback, 0, sizeof(readback));
   n = retro_spsc_read(&q, readback, sizeof(readback));
   if (n != sizeof(buf) || memcmp(buf, readback, sizeof(buf)) != 0)
   {
      fprintf(stderr, "FAIL: read content mismatch\n");
      retro_spsc_free(&q);
      return 1;
   }

   /* peek does not advance */
   retro_spsc_write(&q, buf, 32);
   if (retro_spsc_peek(&q, readback, 32) != 32)
   {
      fprintf(stderr, "FAIL: peek returned wrong size\n");
      retro_spsc_free(&q);
      return 1;
   }
   if (retro_spsc_read_avail(&q) != 32)
   {
      fprintf(stderr, "FAIL: peek advanced read cursor\n");
      retro_spsc_free(&q);
      return 1;
   }
   /* drain so wraparound test starts from a known state */
   retro_spsc_read(&q, readback, 32);

   /* wraparound: write 100 (forces wrap given capacity 128 + 32-byte
    * peek state above) */
   for (i = 0; i < sizeof(buf); i++)
      buf[i] = (uint8_t)(0x80 + i);
   n = retro_spsc_write(&q, buf, sizeof(buf));
   if (n != sizeof(buf))
   {
      fprintf(stderr, "FAIL: wraparound write\n");
      retro_spsc_free(&q);
      return 1;
   }
   memset(readback, 0, sizeof(readback));
   n = retro_spsc_read(&q, readback, sizeof(buf));
   if (n != sizeof(buf) || memcmp(buf, readback, sizeof(buf)) != 0)
   {
      fprintf(stderr, "FAIL: wraparound read content mismatch\n");
      retro_spsc_free(&q);
      return 1;
   }

   /* clear discards unread data without reallocating buffer */
   retro_spsc_write(&q, buf, 50);
   if (retro_spsc_read_avail(&q) != 50)
   {
      fprintf(stderr, "FAIL: pre-clear read_avail\n");
      retro_spsc_free(&q);
      return 1;
   }
   retro_spsc_clear(&q);
   if (retro_spsc_read_avail(&q) != 0
         || retro_spsc_write_avail(&q) != 128)
   {
      fprintf(stderr, "FAIL: clear did not reset cursors\n");
      retro_spsc_free(&q);
      return 1;
   }
   /* queue is reusable after clear */
   retro_spsc_write(&q, buf, 16);
   memset(readback, 0, sizeof(readback));
   n = retro_spsc_read(&q, readback, 16);
   if (n != 16 || memcmp(buf, readback, 16) != 0)
   {
      fprintf(stderr, "FAIL: post-clear read mismatch\n");
      retro_spsc_free(&q);
      return 1;
   }

   retro_spsc_free(&q);
   printf("[pass] property checks\n");
   return 0;
}

typedef struct
{
   retro_spsc_t q;
   size_t width;
   unsigned mismatches;
   unsigned borrowed, copied;
   int use_spans;
} frame_stress_t;

static void frame_consumer(void *arg)
{
   frame_stress_t *s = (frame_stress_t*)arg;
   uint8_t frame[44];
   unsigned token;
   size_t i;
   for (token = 0; token < 100000; token++)
   {
      const void *data = frame;
      int borrowed = 0;
      while (retro_spsc_read_avail(&s->q) < s->width)
         sthread_yield();
      if (s->use_spans)
      {
         borrowed = retro_spsc_read_begin(&s->q, &data) >= s->width;
         if (!borrowed) retro_spsc_read_end(&s->q, 0);
      }
      if (!borrowed)
      {
         if (retro_spsc_read(&s->q, frame, s->width) != s->width)
            s->mismatches++;
         data = frame;
         s->copied++;
      }
      for (i = 0; i < s->width; i++)
         if (((const uint8_t*)data)[i] != (uint8_t)(token * 13 + i))
            s->mismatches++;
      if (borrowed)
      {
         s->borrowed++;
         retro_spsc_read_end(&s->q, s->width);
      }
   }
}

static int run_frame_stress(int use_spans)
{
   frame_stress_t s;
   sthread_t *consumer;
   uint8_t frame[44];
   unsigned token;
   size_t i;
   for (s.width = 22; s.width <= 44; s.width *= 2)
   {
      s.mismatches = s.borrowed = s.copied = 0;
      s.use_spans = use_spans;
      if (!retro_spsc_init(&s.q, 128)) return 1;
      consumer = sthread_create(frame_consumer, &s);
      if (!consumer) { retro_spsc_free(&s.q); return 1; }
      for (token = 0; token < 100000; token++)
      {
         for (i = 0; i < s.width; i++) frame[i] = (uint8_t)(token * 13 + i);
         while (!retro_spsc_write_frames(&s.q, frame, 1, s.width))
            sthread_yield();
      }
      sthread_join(consumer);
      retro_spsc_free(&s.q);
      if (s.mismatches || (use_spans && (!s.borrowed || !s.copied))) return 1;
   }
   printf("[pass] frame stress (%s): 100000 frames each of 22/44 bytes, 0 mismatches\n",
         use_spans ? "borrow/copy" : "copy");
   return 0;
}

static int run_frame_checks(void)
{
   static const size_t widths[] = {1, 4, 8, 22, 44, 129};
   unsigned w, wrap, cycle;
   for (w = 0; w < sizeof(widths) / sizeof(widths[0]); w++)
      for (wrap = 0; wrap < 2; wrap++)
      {
         retro_spsc_t q;
         uint8_t input[512], model[128], actual[128];
         size_t held = 0, width = widths[w];
         if (!retro_spsc_init(&q, 128)) return 1;
         if (wrap)
         {
            size_t start = SIZE_MAX - 63;
            retro_atomic_size_init(&q.head, start);
            retro_atomic_size_init(&q.tail, start);
            q.cached_head = q.cached_tail = start;
         }
         for (cycle = 0; cycle < 128; cycle++)
         {
            size_t i, n, take, expected, requested = cycle % 3 ? 8 : SIZE_MAX;
            for (i = 0; i < sizeof(input); i++) input[i] = (uint8_t)(cycle * 13 + i);
            expected = (q.capacity - held) / width;
            if (expected > requested) expected = requested;
            n = retro_spsc_write_frames(&q, input, requested, width);
            if (n != expected) goto fail;
            memcpy(model + held, input, n * width); held += n * width;
            if (retro_spsc_read_avail(&q) != held) goto fail;
            if (retro_spsc_write_frames(&q, NULL, 0, width)
                  || retro_spsc_write_frames(&q, NULL, 8, 0)) goto fail;
            if (retro_spsc_read_avail(&q) != held) goto fail;
            take = cycle == 127 ? held / width : cycle % 3 + 1;
            if (take > held / width) take = held / width;
            n = retro_spsc_read(&q, actual, take * width);
            if (n != take * width || memcmp(actual, model, n)) goto fail;
            memmove(model, model + n, held - n); held -= n;
         }
         retro_spsc_free(&q);
         continue;
fail:
         fprintf(stderr, "FAIL: frame write width=%u wrap=%u cycle=%u\n",
               (unsigned)width, wrap, cycle);
         retro_spsc_free(&q);
         return 1;
      }
   puts("[pass] whole-frame writes, short space, ring/cursor wrap and oversized requests");
   return 0;
}

/* All physical offsets, including frames split across the ring end.
 * Full queues must keep borrowed bytes unavailable to the producer. */
static int run_frame_span_checks(void)
{
   size_t width, offset, request;
   unsigned overflow, stale, cases = 0;
   for (width = 22; width <= 44; width *= 2)
      for (offset = 0; offset < 128; offset++)
         for (overflow = 0; overflow < 2; overflow++)
            for (request = 1; request <= 2; request++)
               for (stale = 0; stale < 2; stale++)
               {
                  retro_spsc_t q;
                  uint8_t input[128], next[44], output[128], expected[128];
                  const void *span;
                  size_t i, count = 128 / width, bytes = request * width;
                  size_t start = overflow ? SIZE_MAX - 127 + offset : offset;
                  size_t available, physical, cached, tail;
                  int borrowed;
                  if (!retro_spsc_init(&q, 128)) return 1;
                  retro_atomic_size_init(&q.head, start);
                  retro_atomic_size_init(&q.tail, start);
                  q.cached_head = q.cached_tail = start;
                  for (i = 0; i < sizeof(input); i++) input[i] = (uint8_t)(i * 13 + offset);
                  memset(next, 0xa5, sizeof(next));
                  if (retro_spsc_write_frames(&q, input, count, width) != count) goto fail;
                  /* A nonempty but stale cache: read_begin re-reads the
                   * published head rather than expose less than it. */
                  if (stale) q.cached_head = start + width;
                  tail = retro_atomic_load_relaxed_size(&q.tail);
                  available = retro_spsc_read_begin(&q, &span);
                  physical = 128 - offset;
                  cached = count * width;
                  if (available != (physical < cached ? physical : cached)) goto fail;
                  if (memcmp(span, input, available)) goto fail;
                  if (retro_spsc_write_frames(&q, next, 1, width)) goto fail;
                  if (retro_atomic_load_relaxed_size(&q.tail) != tail) goto fail;
                  if (memcmp(span, input, available)) goto fail;
                  borrowed = available >= bytes;
                  if (borrowed)
                     retro_spsc_read_end(&q, bytes);
                  else
                  {
                     retro_spsc_read_end(&q, 0);
                     if (retro_atomic_load_relaxed_size(&q.tail) != tail) goto fail;
                     if (retro_spsc_read(&q, output, bytes) != bytes) goto fail;
                     if (memcmp(output, input, bytes)) goto fail;
                  }
                  if (retro_atomic_load_relaxed_size(&q.tail) != start + bytes) goto fail;
                  if (retro_spsc_write_frames(&q, next, 1, width) != 1) goto fail;
                  memcpy(expected, input + bytes, count * width - bytes);
                  memcpy(expected + count * width - bytes, next, width);
                  bytes = (count - request + 1) * width;
                  if (retro_spsc_read(&q, output, bytes) != bytes) goto fail;
                  if (memcmp(output, expected, bytes) || retro_spsc_read_avail(&q)) goto fail;
                  retro_spsc_free(&q);
                  cases++;
                  continue;
fail:
                  fprintf(stderr, "FAIL: span width=%u offset=%u overflow=%u request=%u stale=%u\n",
                        (unsigned)width, (unsigned)offset, overflow, (unsigned)request, stale);
                  retro_spsc_free(&q);
                  return 1;
               }
   printf("[pass] %u frame span cases: ownership, fallback, stale cache refreshed and cursor wrap\n", cases);
   return 0;
}

/* ---- mirrored rings ---------------------------------------------- */

/* Single-threaded: the mirrored ring reads like a plain one - capacity,
 * room and fill - and a reservation or a read that crosses the
 * buffer's end is one span, landing where the plain ring would have
 * split it. Run on a ring larger than a page and on one smaller, whose
 * buffer is a page but whose fill stops at the capacity asked for. */
static int run_mirror_case(size_t min_capacity)
{
   retro_spsc_t q;
   uint8_t     *buf;
   void        *wp;
   const void  *rp;
   size_t       cap, size, n, i;

   if (!retro_spsc_init_mirrored(&q, min_capacity))
   {
      fprintf(stderr, "FAIL: mirror %u: init\n", (unsigned)min_capacity);
      return 1;
   }
   cap  = q.capacity;
   size = q.mask + 1;
   if (!q.mirror)
   {
      printf("[skip] mirror %u: this platform maps no mirror, plain ring\n",
            (unsigned)min_capacity);
      retro_spsc_free(&q);
      return 0;
   }
   if (     cap != min_capacity || size < cap || (size & (size - 1))
         || retro_spsc_write_avail(&q) != cap || retro_spsc_read_avail(&q))
   {
      fprintf(stderr, "FAIL: mirror %u: capacity %u in a %u-byte buffer\n",
            (unsigned)min_capacity, (unsigned)cap, (unsigned)size);
      return 1;
   }
   if (!(buf = (uint8_t*)malloc(cap + 512)))
      return 1;
   /* Fill stops at the capacity, not the buffer. */
   memset(buf, 0x11, cap + 512);
   if (retro_spsc_write(&q, buf, cap + 512) != cap
         || retro_spsc_write_avail(&q) != 0
         || retro_spsc_read(&q, buf, cap + 512) != cap)
   {
      fprintf(stderr, "FAIL: mirror %u: fill past the capacity\n",
            (unsigned)min_capacity);
      return 1;
   }
   /* Walk the cursors to 24 bytes short of the buffer's end. */
   n = (size - 24 + size - (cap & (size - 1))) & (size - 1);
   while (n)
   {
      size_t k = n > cap ? cap : n;
      retro_spsc_write(&q, buf, k);
      retro_spsc_read(&q, buf, k);
      n -= k;
   }
   /* One reservation of the whole capacity, across the end. */
   if (retro_spsc_write_begin(&q, &wp) != cap)
   {
      fprintf(stderr, "FAIL: mirror %u: reservation cut at the wrap\n",
            (unsigned)min_capacity);
      return 1;
   }
   for (i = 0; i < cap; i++)
      ((uint8_t*)wp)[i] = (uint8_t)(i * 7 + 3);
   retro_spsc_write_end(&q, cap);
   /* What went past the end is at the buffer's start. */
   if (q.buffer[0] != (uint8_t)(24 * 7 + 3))
   {
      fprintf(stderr, "FAIL: mirror %u: the mirror is not the start\n",
            (unsigned)min_capacity);
      return 1;
   }
   if (retro_spsc_read_begin(&q, &rp) != cap)
   {
      fprintf(stderr, "FAIL: mirror %u: read cut at the wrap\n",
            (unsigned)min_capacity);
      return 1;
   }
   for (i = 0; i < cap; i++)
      if (((const uint8_t*)rp)[i] != (uint8_t)(i * 7 + 3))
      {
         fprintf(stderr, "FAIL: mirror %u: byte %u\n",
               (unsigned)min_capacity, (unsigned)i);
         return 1;
      }
   retro_spsc_read_end(&q, cap);
   /* The copying paths agree across the end. */
   memset(buf, 0x22, 64);
   n = retro_spsc_write(&q, buf, 64);
   memset(buf, 0, 64);
   if (n != 64 || retro_spsc_read(&q, buf, 64) != 64 || buf[0] != 0x22
         || buf[63] != 0x22)
   {
      fprintf(stderr, "FAIL: mirror %u: copy across the end\n",
            (unsigned)min_capacity);
      return 1;
   }
   free(buf);
   retro_spsc_free(&q);
   if (q.buffer || q.mirror)
   {
      fprintf(stderr, "FAIL: mirror %u: free left state\n",
            (unsigned)min_capacity);
      return 1;
   }
   printf("[pass] mirror %u: capacity %u in a %u-byte buffer, spans cross the end whole\n",
         (unsigned)min_capacity, (unsigned)cap, (unsigned)size);
   return 0;
}

/* Two threads, spans only: the producer reserves a varying amount and
 * fills it with a running byte count, the consumer drains spans and
 * checks the count. On a mirrored ring no span is shorter than the
 * free space or the fill it reports. */
#define MIRROR_BYTES (64u << 20)
typedef struct
{
   retro_spsc_t  q;
   unsigned long mismatches, short_writes, short_reads;
} mirror_state_t;

static void mirror_producer(void *arg)
{
   mirror_state_t *s = (mirror_state_t*)arg;
   size_t   sent = 0, want = 1;
   uint8_t  v    = 0;
   while (sent < MIRROR_BYTES)
   {
      void  *p;
      size_t free_now = retro_spsc_write_avail(&s->q);
      size_t span     = retro_spsc_write_begin(&s->q, &p);
      size_t i, n;
      if (s->q.mirror && span < free_now)
         s->short_writes++;
      n = span < want ? span : want;
      if (n > MIRROR_BYTES - sent)
         n = MIRROR_BYTES - sent;
      if (!n)
      {
         retro_spsc_write_end(&s->q, 0);
         sthread_yield();
         continue;
      }
      for (i = 0; i < n; i++)
         ((uint8_t*)p)[i] = v++;
      retro_spsc_write_end(&s->q, n);
      sent += n;
      want  = want * 5 % 997 + 1;
   }
}

static void mirror_consumer(void *arg)
{
   mirror_state_t *s = (mirror_state_t*)arg;
   size_t   got = 0;
   uint8_t  v   = 0;
   while (got < MIRROR_BYTES)
   {
      const void *p;
      size_t fill = retro_spsc_read_avail(&s->q);
      size_t span = retro_spsc_read_begin(&s->q, &p);
      size_t i;
      if (s->q.mirror && span < fill)
         s->short_reads++;
      if (!span)
      {
         sthread_yield();
         continue;
      }
      for (i = 0; i < span; i++)
         if (((const uint8_t*)p)[i] != v++)
            s->mismatches++;
      retro_spsc_read_end(&s->q, span);
      got += span;
   }
}

static int run_mirror_stress(size_t min_capacity)
{
   mirror_state_t s;
   sthread_t     *prod, *cons;
   memset(&s, 0, sizeof(s));
   if (!retro_spsc_init_mirrored(&s.q, min_capacity))
      return 1;
   prod = sthread_create(mirror_producer, &s);
   cons = sthread_create(mirror_consumer, &s);
   if (!prod || !cons)
   {
      fprintf(stderr, "FAIL: mirror stress: threads\n");
      return 1;
   }
   sthread_join(prod);
   sthread_join(cons);
   if (s.mismatches || s.short_writes || s.short_reads)
   {
      fprintf(stderr, "FAIL: mirror stress %u: %lu mismatched, %lu short reservations, %lu short reads\n",
            (unsigned)min_capacity, s.mismatches, s.short_writes, s.short_reads);
      retro_spsc_free(&s.q);
      return 1;
   }
   printf("[pass] mirror stress: %u MiB through a %s %u-byte ring in spans, 0 mismatches%s\n",
         MIRROR_BYTES >> 20, s.q.mirror ? "mirrored" : "plain",
         (unsigned)s.q.capacity, s.q.mirror ? ", no span cut short" : "");
   retro_spsc_free(&s.q);
   return 0;
}

int main(void)
{
   if (     run_mirror_case(1024) != 0 || run_mirror_case(65536) != 0
         || run_mirror_stress(1024) != 0 || run_mirror_stress(4096) != 0)
      return 1;
   if (run_frame_checks() != 0 || run_frame_span_checks() != 0
         || run_frame_stress(0) != 0 || run_frame_stress(1) != 0) return 1;
   if (run_property_checks() != 0)
      return 1;
   if (run_stress() != 0)
      return 1;
   puts("ALL OK");
   return 0;
}

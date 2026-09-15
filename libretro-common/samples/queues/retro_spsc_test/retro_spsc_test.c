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
      /* Spin until there is room for a full token. */
      while (retro_spsc_write_avail(&s->q) < sizeof(token))
         ; /* spin */
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
         ; /* spin */
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
      while (retro_spsc_read_avail(&s->q) < s->width) ;
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
         while (!retro_spsc_write_frames(&s.q, frame, 1, s.width)) ;
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
                  /* A nonempty cache can expose less than the published head. */
                  if (stale) q.cached_head = start + width;
                  tail = retro_atomic_load_relaxed_size(&q.tail);
                  available = retro_spsc_read_begin(&q, &span);
                  physical = 128 - offset;
                  cached = stale ? width : count * width;
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
   printf("[pass] %u frame span cases: ownership, fallback, stale cache and cursor wrap\n", cases);
   return 0;
}

int main(void)
{
   if (run_frame_checks() != 0 || run_frame_span_checks() != 0
         || run_frame_stress(0) != 0 || run_frame_stress(1) != 0) return 1;
   if (run_property_checks() != 0)
      return 1;
   if (run_stress() != 0)
      return 1;
   puts("ALL OK");
   return 0;
}

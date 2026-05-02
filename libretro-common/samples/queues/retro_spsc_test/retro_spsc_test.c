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

int main(void)
{
   if (run_property_checks() != 0)
      return 1;
   if (run_stress() != 0)
      return 1;
   puts("ALL OK");
   return 0;
}

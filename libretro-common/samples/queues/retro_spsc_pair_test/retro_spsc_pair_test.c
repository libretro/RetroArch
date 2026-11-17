/* retro_spsc pair writes, two-span borrowed reads and reservations.
 *
 *  - write_pair: a record made of two spans goes in whole or not at all,
 *    at every offset round the ring, and reads back as the two joined;
 *    one past the room, an empty one, and lengths whose sum would wrap
 *    are refused without a byte written.
 *  - read_begin_pair: what the ring holds, wrapped or not, is lent as at
 *    most two spans in order, and read_end consumes a prefix of them.
 *  - write_reserve: room in one piece is lent and published by write_end
 *    or abandoned by write_end(q, 0); a record that would cross the end
 *    of a plain ring is refused, and a mirrored ring never cuts one.
 *  - a producer thread writing pairs and a consumer reading views move
 *    a counted stream across with nothing lost or reordered.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <retro_spsc.h>
#include <rthreads/rthreads.h>

static int fails;
#define CHECK(c, what) do { if (!(c)) { printf("FAIL: %s\n", what); fails++; } } while (0)

static uint8_t pat(size_t i) { return (uint8_t)(i * 31u + 7u); }

static void test_write_pair(void)
{
   retro_spsc_t q;
   uint8_t a[512], b[64], got[512];
   size_t i, off, cap, la, lb;
   for (i = 0; i < sizeof(a); i++) a[i] = pat(i);
   for (i = 0; i < sizeof(b); i++) b[i] = pat(i + 64);
   if (!retro_spsc_init(&q, 100)) { CHECK(0, "init"); return; }
   cap = retro_spsc_write_avail(&q);
   /* at every offset round the ring */
   for (off = 0; off < cap + 3; off++)
   {
      la = (off * 7) % 40;
      lb = 1 + (off * 13) % 40;
      CHECK(retro_spsc_write_pair(&q, a, la, b, lb) == la + lb, "a pair goes in whole");
      memset(got, 0, sizeof(got));
      CHECK(retro_spsc_read(&q, got, la + lb) == la + lb
            && !memcmp(got, a, la) && !memcmp(got + la, b, lb),
            "and reads back as the two spans joined");
      /* move the ring on by one so the next pair starts elsewhere */
      CHECK(retro_spsc_write(&q, a, 1) == 1 && retro_spsc_read(&q, got, 1) == 1, "step");
   }
   /* all or nothing */
   CHECK(retro_spsc_write(&q, a, cap - 10) == cap - 10, "fill");
   CHECK(retro_spsc_write_pair(&q, a, 6, b, 5) == 0
         && retro_spsc_read_avail(&q) == cap - 10, "one past the room writes nothing");
   CHECK(retro_spsc_write_pair(&q, a, 6, b, 4) == 10, "exactly the room fits");
   retro_spsc_clear(&q);
   CHECK(retro_spsc_write_pair(&q, NULL, 0, NULL, 0) == 0, "an empty record is refused");
   CHECK(retro_spsc_write_pair(&q, NULL, 0, b, 3) == 3, "a NULL span of length zero is fine");
   CHECK(retro_spsc_write_pair(&q, a, (size_t)-1, b, 2) == 0
         && retro_spsc_read_avail(&q) == 3, "a sum that wraps is refused unread");
   retro_spsc_free(&q);
}

static void test_read_begin_pair(void)
{
   retro_spsc_t q;
   uint8_t a[256], got[256];
   const void *p1, *p2;
   size_t i, n, f, cap;
   for (i = 0; i < sizeof(a); i++) a[i] = pat(i);
   if (!retro_spsc_init(&q, 64)) { CHECK(0, "init"); return; }
   cap = retro_spsc_write_avail(&q);
   CHECK(retro_spsc_read_begin_pair(&q, &p1, &f, &p2, 16) == 0
         && !p1 && !p2 && !f, "an empty ring lends nothing");
   /* put the data across the end of the ring */
   CHECK(retro_spsc_write(&q, a, cap - 8) == cap - 8
         && retro_spsc_read(&q, got, cap - 8) == cap - 8, "move to the end");
   CHECK(retro_spsc_write(&q, a, 20) == 20, "write across the end");
   n = retro_spsc_read_begin_pair(&q, &p1, &f, &p2, 64);
   CHECK(n == 20 && f > 0 && f < 20 && p1 && p2, "a wrapped record is lent as two spans");
   if (n == 20 && p1 && p2)
   {
      memcpy(got, p1, f);
      memcpy(got + f, p2, n - f);
      CHECK(!memcmp(got, a, 20), "in order");
   }
   retro_spsc_read_end(&q, 5);
   CHECK(retro_spsc_read_avail(&q) == 15, "read_end consumes a prefix");
   n = retro_spsc_read_begin_pair(&q, &p1, &f, &p2, 4);
   if (n == 4 && p1)
   {
      memcpy(got, p1, f);
      if (f < 4 && p2)
         memcpy(got + f, p2, 4 - f);
   }
   CHECK(n == 4 && f <= 4 && (f == 4 ? !p2 : p2 != NULL)
         && !memcmp(got, a + 5, 4), "capped at what is asked for");
   retro_spsc_read_end(&q, 0);
   CHECK(retro_spsc_read_avail(&q) == 15, "read_end(0) abandons the view");
   retro_spsc_free(&q);
}

static void test_reserve(void)
{
   retro_spsc_t q, m;
   uint8_t got[512];
   void *p;
   size_t cap;
   if (!retro_spsc_init(&q, 64)) { CHECK(0, "init"); return; }
   cap = retro_spsc_write_avail(&q);
   CHECK(retro_spsc_write_reserve(&q, &p, 16) && p, "room in one piece is lent");
   if (p) memset(p, 0x5A, 16);
   retro_spsc_write_end(&q, 16);
   CHECK(retro_spsc_read(&q, got, 16) == 16 && got[0] == 0x5A && got[15] == 0x5A,
         "and write_end publishes it");
   CHECK(retro_spsc_write_reserve(&q, &p, 8) && p, "a second reservation");
   retro_spsc_write_end(&q, 0);
   CHECK(retro_spsc_read_avail(&q) == 0, "write_end(0) abandons it");
   CHECK(!retro_spsc_write_reserve(&q, &p, 0) && !p, "zero bytes are refused");
   CHECK(!retro_spsc_write_reserve(&q, &p, cap + 1) && !p, "more than the ring is refused");
   /* a record across the end of a plain ring is refused, nothing written */
   CHECK(retro_spsc_write(&q, got, cap - 16 - 4) == cap - 20
         && retro_spsc_read(&q, got, cap - 20) == cap - 20, "move near the end");
   CHECK(!retro_spsc_write_reserve(&q, &p, 16) && !p
         && retro_spsc_read_avail(&q) == 0, "one that would cross the end is refused");
   retro_spsc_free(&q);
   if (retro_spsc_init_mirrored(&m, 64))
   {
      cap = retro_spsc_write_avail(&m);
      CHECK(retro_spsc_write(&m, got, cap - 4) == cap - 4
            && retro_spsc_read(&m, got, cap - 4) == cap - 4, "move near the end (mirrored)");
      CHECK(retro_spsc_write_reserve(&m, &p, 16) && p, "a mirrored ring never cuts a record");
      retro_spsc_write_end(&m, 16);
      retro_spsc_free(&m);
   }
   else
      printf("note: no mirrored ring here\n");
}

/* a counted stream across two threads */
#define STREAM (4u << 20)
static retro_spsc_t sq;

static void producer(void *arg)
{
   uint8_t a[40], b[40];
   uint32_t at = 0;
   (void)arg;
   while (at < STREAM)
   {
      size_t la = 1 + at % 37, lb = at % 29, i;
      if (at + la + lb > STREAM) { la = STREAM - at; lb = 0; }
      for (i = 0; i < la; i++) a[i] = pat(at + i);
      for (i = 0; i < lb; i++) b[i] = pat(at + la + i);
      if (retro_spsc_write_pair(&sq, a, la, b, lb) == la + lb)
         at += (uint32_t)(la + lb);
   }
}

static void test_threads(void)
{
   sthread_t *t;
   uint32_t at = 0;
   int bad = 0;
   if (!retro_spsc_init(&sq, 4096)) { CHECK(0, "init"); return; }
   t = sthread_create(producer, NULL);
   while (at < STREAM && !bad)
   {
      const void *p1, *p2;
      size_t f, n = retro_spsc_read_begin_pair(&sq, &p1, &f, &p2, 1000), i;
      for (i = 0; i < n && !bad; i++)
      {
         uint8_t v = i < f ? ((const uint8_t*)p1)[i] : ((const uint8_t*)p2)[i - f];
         if (v != pat(at + i)) bad = 1;
      }
      retro_spsc_read_end(&sq, n);
      at += (uint32_t)n;
   }
   sthread_join(t);
   CHECK(!bad && at == STREAM, "a stream of pairs read through views arrives whole and in order");
   retro_spsc_free(&sq);
}

int main(void)
{
   test_write_pair();
   test_read_begin_pair();
   test_reserve();
   test_threads();
   printf(fails ? "retro_spsc_pair_test: %d FAIL\n" : "retro_spsc_pair_test: PASS\n", fails);
   return fails != 0;
}

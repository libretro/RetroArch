/* rbzip2_decode() against streams bzip2 itself wrote (rbzip2_fixtures.h:
 * six tables, three blocks, runs of every length and octet), and
 * against bz_write.h's, whose shapes are chosen here: every block size,
 * two to six tables, selectors of every rank, all 256 octets, runs that
 * reach the 255 the run coding holds and runs of rank 0 thousands long,
 * blocks filled to the last octet. Each must decode octet for octet,
 * into a buffer of its size and into one grown as it decodes, whether
 * a block's walk runs from one end or from both, and block by block
 * from input in two pieces split anywhere, as a ring buffer wraps.
 * Then what must fail: every prefix of a stream, a block's CRC wrong,
 * the stream's changed, a randomised block, a buffer one octet short,
 * and a stream with octets flipped, which must fail or decode, never
 * read or write out of bounds. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <encodings/rbzip2.h>

#include "bz_write.h"
#include "rbzip2_fixtures.h"

static int      failures;
static uint32_t x;

static uint32_t next(void)
{
   x = x * 1103515245u + 12345u;
   return x;
}

static void fail(const char *what, unsigned i)
{
   printf("FAIL: %s (%u)\n", what, i);
   failures++;
}

/* the fixtures' plaintexts, as fx.py made them */
static size_t gen_a(uint8_t *d)
{
   size_t i;
   x = 1;
   for (i = 0; i < 3000; i++)
      d[i] = (uint8_t)('a' + ((next() >> 16) % 16));
   return 3000;
}

static size_t gen_b(uint8_t *d)
{
   static const char p[] = "the quick brown fox jumps over the lazy dog ";
   size_t i;
   x = 1;
   for (i = 0; i < 250000; i++)
   {
      d[i] = (uint8_t)p[i % (sizeof(p) - 1)];
      if (i % 997 == 0)
         d[i] = (uint8_t)('A' + ((next() >> 16) % 26));
   }
   return 250000;
}

static size_t gen_c(uint8_t *d)
{
   size_t n = 0;
   x = 1;
   while (n < 60000)
   {
      uint32_t v = next();
      size_t   r = 1 + (v >> 4) % 600;
      if (r > 60000 - n)
         r = 60000 - n;
      memset(d + n, (int)((v >> 16) & 0xff), r);
      n += r;
   }
   return n;
}

/* into a buffer of the size, and into one grown as it goes */
static void decodes(rbzip2_t *bz, const uint8_t *s, size_t sn,
      const uint8_t *want, size_t n, uint8_t *out, const char *what,
      unsigned i)
{
   uint8_t *grown = NULL;
   size_t   w     = 0;
   if (     rbzip2_decode(bz, out, n, s, sn, &w) != RBZIP2_OK
         || w != n || memcmp(out, want, n))
      fail(what, i);
   if (     rbzip2_decode_alloc(bz, s, sn, &grown, &w) != RBZIP2_OK
         || w != n || memcmp(grown, want, n))
      fail(what, i + 10000);
   else
      grown[n] = 0;    /* the octet to spare */
   free(grown);
}

static uint8_t *copy_of(const uint8_t *s, size_t n)
{
   uint8_t *c = (uint8_t*)malloc(n ? n : 1);
   if (c && n)
      memcpy(c, s, n);
   return c;
}

/* @s handed over in two pieces each its own buffer, as a ring buffer
 * wraps: for even @i split anew before every block from where the
 * decoder has taken it to, for odd the same two throughout; the blocks
 * must be @want's */
static void ring_decodes(rbzip2_t *bz, const uint8_t *s, size_t sn,
      const uint8_t *want, size_t n, unsigned i)
{
   size_t         k  = 4 + next() % (sn - 3);
   uint8_t       *a  = copy_of(s, k);
   uint8_t       *b  = copy_of(s + k, sn - k);
   const uint8_t *blk;
   size_t         len, at = 0;
   int            e;
   if (rbzip2_begin_ring(bz, a, k, b, sn - k) != RBZIP2_OK)
   {
      fail("a stream begun in two pieces", i);
      free(a);
      free(b);
      return;
   }
   while ((e = rbzip2_step_block(bz, &blk, &len)) == RBZIP2_PENDING)
   {
      uint64_t t = rbzip2_taken(bz);
      if (!len || len > n - at || memcmp(blk, want + at, len) || t > sn)
      {
         fail("a block from two pieces", i);
         break;
      }
      at += len;
      if (i & 1)
         continue;   /* the same two pieces all through */
      free(a);
      free(b);
      k = (size_t)t + next() % (sn - (size_t)t + 1);
      a = copy_of(s + t, k - (size_t)t);
      b = copy_of(s + k, sn - k);
      rbzip2_input(bz, a, k - (size_t)t, b, sn - k);
   }
   if (e != RBZIP2_OK || at != n || rbzip2_taken(bz) > sn)
      fail("a stream from two pieces", i);
   free(a);
   free(b);
}

/* plaintext shape @k, @n octets */
static void shape(uint8_t *d, size_t n, unsigned k)
{
   size_t i = 0;
   switch (k % 6)
   {
      case 0:   /* every octet, no order */
         for (; i < n; i++)
            d[i] = (uint8_t)(next() >> 16);
         break;
      case 1:   /* a few letters */
         for (; i < n; i++)
            d[i] = (uint8_t)('a' + (next() >> 16) % 3);
         break;
      case 2:   /* runs, short and up past 255 */
         while (i < n)
         {
            uint32_t v = next();
            size_t   r = 1 + (v >> 3) % ((k & 8) ? 700 : 9);
            for (; r && i < n; r--)
               d[i++] = (uint8_t)(v >> 24);
         }
         break;
      case 3:   /* one octet throughout */
         memset(d, 0x5a, n);
         break;
      case 4:   /* a phrase over and over: rank 0 for thousands */
         for (; i < n; i++)
            d[i] = (uint8_t)"repetition"[i % 10];
         break;
      default:  /* counting, then noise */
         for (; i < n; i++)
            d[i] = (uint8_t)(i < n / 2 ? i : next() >> 20);
         break;
   }
}

int main(void)
{
   static uint8_t d[400000], s[460000], o[400016];
   rbzip2_t *bz = rbzip2_new();
   size_t    n, sn, w;
   unsigned  i;

   if (!bz)
      return 2;

   n = gen_a(d);
   decodes(bz, fx_a, sizeof(fx_a), d, n, o, "bzip2's stream A", 0);
   n = gen_b(d);
   decodes(bz, fx_b, sizeof(fx_b), d, n, o, "bzip2's stream B", 0);
   n = gen_c(d);
   decodes(bz, fx_c, sizeof(fx_c), d, n, o, "bzip2's stream C", 0);

   /* nothing at all */
   sn = bz_write(s, sizeof(s), d, 0, 9, 1);
   if (rbzip2_decode(bz, o, 0, s, sn, &w) != RBZIP2_OK || w)
      fail("an empty stream", 0);

   x = 99;
   for (i = 0; i < 96; i++)
   {
      unsigned level = 1 + i % 9;
      n  = (i % 4 == 0) ? 100000u * (i % 3 ? 1 : 3) + i
         : (i % 4 == 1) ? 1 + next() % 300 : next() % 40000;
      if (level < 3 && i % 5 == 0)
         n = 100000u * level - 19;    /* exactly one full block */
      shape(d, n, i);
      sn = bz_write(s, sizeof(s), d, n, level, i);
      if (!sn)
      {
         fail("the writer had no room", i);
         continue;
      }
      decodes(bz, s, sn, d, n, o, "a written stream", i);
      if (n && rbzip2_decode(bz, o, n - 1, s, sn, &w) != RBZIP2_ERROR_SPACE)
         fail("a buffer one octet short", i);
      /* what follows a stream is not read */
      s[sn] = 0x42;
      decodes(bz, s, sn + 1, d, n, o, "a stream with more after it", i);
   }

   /* a block at a time: what each step has written is final, and
    * the last step is the stream's end */
   shape(d, 350000, 7);
   sn = bz_write(s, sizeof(s), d, 350000, 1, 7);
   if (rbzip2_begin(bz, o, 350000, s, sn) != RBZIP2_OK)
      fail("a stream begun", 0);
   else
   {
      size_t   last = 0;
      unsigned steps = 0;
      int      e;
      while ((e = rbzip2_step(bz, &w)) == RBZIP2_PENDING)
      {
         if (w <= last || w > 350000 || memcmp(o, d, w))
            fail("a step's blocks", steps);
         last = w;
         steps++;
      }
      if (e != RBZIP2_OK || w != 350000 || steps < 4)
         fail("a stream a block at a time", steps);
   }
   /* each block where the decoder leaves it, one after another the
    * stream's octets */
   if (rbzip2_begin_blocks(bz, s, sn) != RBZIP2_OK)
      fail("a stream begun a block at a time", 0);
   else
   {
      const uint8_t *blk;
      size_t         len, at = 0;
      unsigned       blocks = 0;
      int            e;
      while ((e = rbzip2_step_block(bz, &blk, &len)) == RBZIP2_PENDING)
      {
         if (!len || len > 350000 - at || memcmp(blk, d + at, len))
            fail("a block where the decoder leaves it", blocks);
         at += len;
         blocks++;
      }
      if (e != RBZIP2_OK || at != 350000 || blocks < 4)
         fail("a stream's blocks one after another", blocks);
   }
   for (i = 0; i < 40; i++)
      ring_decodes(bz, s, sn, d, 350000, i);
   /* half of it given, as if the rest were still to be read: a block
    * past the half fails, taking nothing more */
   {
      uint8_t       *h = copy_of(s, sn / 2);
      const uint8_t *blk;
      size_t         len;
      int            e;
      if (rbzip2_begin_ring(bz, h, sn / 2, NULL, 0) == RBZIP2_OK)
      {
         while ((e = rbzip2_step_block(bz, &blk, &len)) == RBZIP2_PENDING) { }
         if (e != RBZIP2_ERROR_DATA)
            fail("half a stream in a ring", 0);
      }
      else
         fail("half a stream begun", 0);
      free(h);
   }
   if (rbzip2_begin(bz, o, 349999, s, sn) == RBZIP2_OK)
   {
      int e;
      while ((e = rbzip2_step(bz, &w)) == RBZIP2_PENDING) { }
      if (e != RBZIP2_ERROR_SPACE)
         fail("a block at a time, a buffer one octet short", 0);
   }

   /* what must fail */
   n  = gen_a(d);
   for (i = 0; i < sizeof(fx_a); i++)
      if (rbzip2_decode(bz, o, n, fx_a, i, &w) == RBZIP2_OK)
         fail("a prefix of a stream", i);
   /* a block's CRC wrong, and the stream's made from it */
   bzw_lie = 1;
   sn      = bz_write(s, sizeof(s), d, n, 9, 3);
   bzw_lie = 0;
   if (rbzip2_decode(bz, o, n, s, sn, &w) != RBZIP2_ERROR_DATA)
      fail("a block CRC wrong", 0);
   if (rbzip2_begin(bz, o, n, s, sn) == RBZIP2_OK)
   {
      int e;
      while ((e = rbzip2_step(bz, &w)) == RBZIP2_PENDING) { }
      if (e != RBZIP2_ERROR_DATA)
         fail("a block CRC wrong, a block at a time", 0);
   }
   if (rbzip2_begin_blocks(bz, s, sn) == RBZIP2_OK)
   {
      const uint8_t *blk;
      size_t         len;
      int            e;
      while ((e = rbzip2_step_block(bz, &blk, &len)) == RBZIP2_PENDING) { }
      if (e != RBZIP2_ERROR_DATA)
         fail("a block CRC wrong, where the decoder leaves it", 0);
   }
   memcpy(s, fx_a, sizeof(fx_a));
   s[sizeof(fx_a) - 2] ^= 0x10;      /* the stream's */
   if (rbzip2_decode(bz, o, n, s, sizeof(fx_a), &w) != RBZIP2_ERROR_DATA)
      fail("the stream CRC changed", 0);
   memcpy(s, fx_a, sizeof(fx_a));
   s[14] |= 0x80;                    /* the randomised bit */
   if (rbzip2_decode(bz, o, n, s, sizeof(fx_a), &w) != RBZIP2_ERROR_DATA)
      fail("a randomised block", 0);
   if (rbzip2_decode(bz, o, n, (const uint8_t*)"BZh0", 4, &w) != RBZIP2_ERROR_DATA)
      fail("block size 0", 0);
   for (i = 0; i < 3000; i++)
   {
      const uint8_t *f  = i % 3 == 0 ? fx_a : i % 3 == 1 ? fx_b : fx_c;
      size_t         fn = i % 3 == 0 ? sizeof(fx_a) : i % 3 == 1 ? sizeof(fx_b)
                                     : sizeof(fx_c);
      unsigned       k, flips = 1 + next() % 4;
      memcpy(s, f, fn);
      for (k = 0; k < flips; k++)
         s[4 + next() % (fn - 4)] ^= (uint8_t)(1u << (next() % 8));
      rbzip2_decode(bz, o, sizeof(o), s, fn, &w);
   }

   rbzip2_free(bz);
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("rbzip2_test: every stream decoded, whole and a block at a time, every bad one refused\n");
   return 0;
}

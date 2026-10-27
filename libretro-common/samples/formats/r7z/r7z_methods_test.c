/* The coders 7-Zip writes besides LZMA: PPMd, BZip2, Deflate and
 * Deflate64. An archive of each that 7-Zip wrote (r7z_methods_fixtures.h)
 * extracts octet for octet, whole and in slices. Deflate64 itself,
 * through rinflate64_decode(): a stream written here reaches what
 * RFC 1951 cannot, distance codes 30 and 31 and a length taken from 16
 * extra bits, and a stream that refers back before its start or is cut
 * short is refused. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <7z/r7z_archive.h>
#include <encodings/deflate.h>

#include "r7z_methods_fixtures.h"

static int failures;

static void check(int ok, const char *what, const char *how)
{
   if (!ok)
   {
      printf("FAIL: %s, %s\n", what, how);
      failures++;
   }
}

static uint32_t lcg(uint32_t x)
{
   return x * 1103515245u + 12345u;
}

/* mgen.py's t.txt and b.bin */
static void text(uint8_t *d, size_t n)
{
   uint8_t  vocab[40][9];
   unsigned vlen[40], w, k;
   uint32_t x = 1;
   size_t   at = 0;
   for (w = 0; w < 40; w++)
   {
      x       = lcg(x);
      vlen[w] = 2 + (x >> 16) % 7;
      for (k = 0; k < vlen[w]; k++)
      {
         x           = lcg(x);
         vocab[w][k] = (uint8_t)(97 + (x >> 16) % 26);
      }
   }
   while (at < n)
   {
      x = lcg(x);
      w = (x >> 16) % 40;
      for (k = 0; k < vlen[w] && at < n; k++)
         d[at++] = vocab[w][k];
      if (at < n)
         d[at++] = ' ';
   }
}

static void binary(uint8_t *d, size_t n)
{
   size_t i;
   for (i = 0; i < n; i++)
      d[i] = (uint8_t)(i * 7 + (i >> 9));
}

static void archive(const char *what, const uint8_t *fx, size_t fx_len)
{
   r7z_archive_t *a = NULL;
   uint32_t       i;
   if (r7z_archive_open(&a, fx, fx_len) != R7Z_OK)
   {
      check(0, what, "opens");
      return;
   }
   check(r7z_archive_num_entries(a) == 2, what, "two members");
   for (i = 0; i < r7z_archive_num_entries(a); i++)
   {
      const r7z_entry_t *e = r7z_archive_entry(a, i);
      uint8_t            want[3000];
      uint8_t           *got = NULL;
      size_t             len = 0;
      int                r;
      if (e->size == 1500)
         binary(want, 1500);
      else
         text(want, 3000);
      r = r7z_archive_extract(a, i, &got, &len);
      check(r == R7Z_OK && len == e->size && !memcmp(got, want, len), what,
            "extracted whole");
      free(got);
      got = NULL;
      while ((r = r7z_archive_extract_slice(a, i, &got, &len)) == R7Z_PENDING) { }
      check(r == R7Z_OK && len == e->size && !memcmp(got, want, len), what,
            "extracted in slices");
      free(got);
   }
   r7z_archive_close(a);
}

/* big.txt's folder, decoded a step at a time: it takes several, what
 * is decoded so far is there to be read from the front, at one place,
 * and is big.txt's; a byte changed in the packed stream is refused. */
static void staged(const char *what, const uint8_t *fx, size_t fx_len)
{
   size_t         n    = 300000, last = 0, k;
   uint8_t       *want = (uint8_t*)malloc(n);
   uint8_t       *bad  = (uint8_t*)malloc(fx_len);
   uint8_t       *got  = NULL;
   const uint8_t *at   = NULL;
   r7z_archive_t *a    = NULL;
   unsigned       steps = 0, partial = 0;
   int            r, moved = 0, wrong = 0;

   text(want, 20000);
   for (k = 1; k < 15; k++)
      memcpy(want + k * 20000, want, 20000);

   if (r7z_archive_open(&a, fx, fx_len) != R7Z_OK)
      check(0, what, "opens");
   else
   {
      do
      {
         const uint8_t *data  = NULL;
         size_t         avail = 0;
         r = r7z_archive_decode_step(a, 0);
         steps++;
         if (r7z_archive_decode_peek(a, 0, &data, &avail) == R7Z_OK
               && avail)
         {
            if (at && data != at)
               moved = 1;
            at = data;
            if (avail < last || avail > n || memcmp(data, want, avail))
               wrong = 1;
            if (avail < n)
               partial++;
            last = avail;
         }
      } while (r == R7Z_PENDING && steps < 1000);
      check(r == R7Z_OK, what, "decoded a step at a time");
      check(steps >= 4, what, "over several steps");
      check(partial >= 2, what, "readable part way");
      check(!moved && !wrong && last == n, what,
            "read from the front, in one place");
      r = r7z_archive_extract(a, 0, &got, &k);
      check(r == R7Z_OK && k == n && !memcmp(got, want, n), what,
            "extracted");
      free(got);
      r7z_archive_close(a);
   }

   /* a byte in the middle of the packed stream, which begins at 32 */
   memcpy(bad, fx, fx_len);
   bad[32 + (fx_len - 32) / 4] ^= 0x40;
   a = NULL;
   if (r7z_archive_open(&a, bad, fx_len) == R7Z_OK)
   {
      steps = 0;
      while ((r = r7z_archive_decode_step(a, 0)) == R7Z_PENDING
            && ++steps < 1000) { }
      /* the folder may carry no CRC of its own: then the member's
       * refuses it */
      if (r == R7Z_OK)
      {
         got = NULL;
         r   = r7z_archive_extract(a, 0, &got, &k);
         free(got);
      }
      check(r < 0, what, "a changed byte refused");
      r7z_archive_close(a);
   }
   free(bad);
   free(want);
}

/* A Deflate64 stream with the fixed codes, written LSB first */
struct bits
{
   uint8_t *p;
   size_t   at;
   uint64_t acc;
   unsigned n;
};

static void put(struct bits *b, uint32_t v, unsigned k)
{
   b->acc |= (uint64_t)v << b->n;
   b->n   += k;
   while (b->n >= 8)
   {
      b->p[b->at++] = (uint8_t)b->acc;
      b->acc >>= 8;
      b->n    -= 8;
   }
}

/* a Huffman code, most significant bit first */
static void put_code(struct bits *b, uint32_t code, unsigned len)
{
   uint32_t rev = 0;
   unsigned i;
   for (i = 0; i < len; i++)
      rev |= ((code >> i) & 1u) << (len - 1 - i);
   put(b, rev, len);
}

static void put_lit(struct bits *b, unsigned sym)
{
   if (sym < 144)
      put_code(b, 0x30 + sym, 8);
   else if (sym < 256)
      put_code(b, 0x190 + sym - 144, 9);
   else if (sym < 280)
      put_code(b, sym - 256, 7);
   else
      put_code(b, 0xc0 + sym - 280, 8);
}

/* a length of 3 + 0..65535 through code 285, a distance through 30/31 */
static void put_match(struct bits *b, uint32_t len, uint32_t dist)
{
   put_lit(b, 285);
   put(b, len - 3, 16);
   if (dist >= 49153)
   {
      put_code(b, 31, 5);
      put(b, dist - 49153, 14);
   }
   else
   {
      put_code(b, 30, 5);
      put(b, dist - 32769, 14);
   }
}

static void deflate64(void)
{
   size_t   n    = 40000 + 65538 + 300;
   uint8_t *want = (uint8_t*)malloc(n);
   uint8_t *got  = (uint8_t*)malloc(n);
   uint8_t *s    = (uint8_t*)malloc(80000);
   struct bits b;
   size_t   i, w = 0, len;
   uint32_t x = 9;

   for (i = 0; i < 40000; i++)
   {
      x       = lcg(x);
      want[i] = (uint8_t)(x >> 16);
   }
   /* 65538 back 34000, then 300 back 50000 */
   for (; i < 40000 + 65538; i++)
      want[i] = want[i - 34000];
   for (; i < n; i++)
      want[i] = want[i - 50000];

   b.p = s; b.at = 0; b.acc = 0; b.n = 0;
   put(&b, 1, 1);                /* final */
   put(&b, 1, 2);                /* fixed codes */
   for (i = 0; i < 40000; i++)
      put_lit(&b, want[i]);
   put_match(&b, 65538, 34000);
   put_match(&b, 300, 50000);
   put_lit(&b, 256);
   if (b.n)
      put(&b, 0, 8 - b.n);
   len = b.at;

   check(rinflate64_decode(got, n, s, len, &w) == RDEFLATE_PROCESS_END
         && w == n && !memcmp(got, want, n), "Deflate64", "the long codes");
   check(rinflate64_decode(got, n - 1, s, len, &w) == RDEFLATE_PROCESS_ERROR,
         "Deflate64", "more than the buffer holds");
   check(rinflate64_decode(got, n, s, len - 2, &w) == RDEFLATE_PROCESS_ERROR,
         "Deflate64", "cut short");

   /* a distance back before the start */
   b.p = s; b.at = 0; b.acc = 0; b.n = 0;
   put(&b, 1, 1);
   put(&b, 1, 2);
   put_lit(&b, 'x');
   put_match(&b, 10, 40000);
   put_lit(&b, 256);
   if (b.n)
      put(&b, 0, 8 - b.n);
   check(rinflate64_decode(got, n, s, b.at, &w) == RDEFLATE_PROCESS_ERROR,
         "Deflate64", "a distance before the start");

   free(want);
   free(got);
   free(s);
}

int main(void)
{
   archive("PPMd", fx_ppmd, sizeof(fx_ppmd));
   archive("PPMd, order 2", fx_ppmd_o2, sizeof(fx_ppmd_o2));
#ifdef HAVE_RBZIP2
   archive("BZip2", fx_bzip2, sizeof(fx_bzip2));
#endif
   archive("Deflate", fx_deflate, sizeof(fx_deflate));
   archive("Deflate64", fx_deflate64, sizeof(fx_deflate64));
   staged("PPMd, stepped", fx_big_ppmd, sizeof(fx_big_ppmd));
   staged("Deflate, stepped", fx_big_deflate, sizeof(fx_big_deflate));
   deflate64();
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("r7z_methods_test: every coder's archive extracted, PPMd and Deflate a step at a time, Deflate64 whole\n");
   return 0;
}

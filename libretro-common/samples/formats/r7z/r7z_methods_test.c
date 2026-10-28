/* The coders 7-Zip writes besides LZMA: PPMd, BZip2, Deflate and
 * Deflate64. An archive of each that 7-Zip wrote (r7z_methods_fixtures.h)
 * extracts octet for octet, whole and in slices, and streams as it
 * extracts. Deflate64 itself,
 * through rinflate64_decode(): a stream written here reaches what
 * RFC 1951 cannot, distance codes 30 and 31 and a length taken from 16
 * extra bits, and a stream that refers back before its start or is cut
 * short is refused. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <7z/r7z_archive.h>
#include <7z/r7z_filters.h>
#include <7z/r7z_lzma2.h>
#include <7z/r7z_lzma_stream.h>
#include <encodings/crc32.h>
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

struct mem
{
   const uint8_t *d;
   size_t         n;
};

static int64_t read_at(void *ud, uint64_t off, void *dst, size_t len)
{
   const struct mem *m = (const struct mem*)ud;
   if (off >= m->n)
      return 0;
   if (len > m->n - off)
      len = (size_t)(m->n - off);
   memcpy(dst, m->d + off, len);
   return (int64_t)len;
}

/* Every member of a 7-Zip archive streamed, in order, as it extracts:
 * from memory, and through positioned reads */
static void streamed_way(const char *what, const uint8_t *fx, size_t fx_len,
      int read)
{
   r7z_archive_t *a = NULL;
   uint32_t       i;
   struct mem     m;
   m.d = fx;
   m.n = fx_len;
   if ((read ? r7z_archive_open_read(&a, fx_len, read_at, &m)
             : r7z_archive_open(&a, fx, fx_len)) != R7Z_OK)
   {
      check(0, what, "opens to stream");
      return;
   }
   for (i = 0; i < r7z_archive_num_entries(a); i++)
   {
      uint8_t       *want = NULL;
      size_t         want_len = 0, at = 0, n;
      const uint8_t *d;
      int            r, ok = 1;
      if (     r7z_archive_entry(a, i)->is_dir
            || !r7z_archive_entry(a, i)->size)
         continue;
      if (     r7z_archive_extract(a, i, &want, &want_len) != R7Z_OK
            || r7z_archive_stream_begin(a, i) != R7Z_OK)
      {
         check(0, what, "streams");
         free(want);
         continue;
      }
      while ((r = r7z_archive_stream_step(a, &d, &n)) == R7Z_PENDING)
      {
         if (n > want_len - at || (n && memcmp(d, want + at, n)))
            ok = 0;
         at += n;
      }
      check(ok && r == R7Z_OK && at == want_len, what,
            read ? "streamed through reads" : "streamed from memory");
      free(want);
   }
   r7z_archive_close(a);
}

static void streamed(const char *what, const uint8_t *fx, size_t fx_len)
{
   streamed_way(what, fx, fx_len, 0);
   streamed_way(what, fx, fx_len, 1);
}

/* big.txt's folder, or rep.txt's: @unit octets of t.txt's words 300000
 * / @unit times over, decoded a step at a time: it takes several, what
 * is decoded so far is there to be read from the front, at one place,
 * and is big.txt's; a byte changed in the packed stream is refused. */
static void staged(const char *what, const uint8_t *fx, size_t fx_len,
      size_t unit)
{
   size_t         n    = 300000, last = 0, k;
   uint8_t       *want = (uint8_t*)malloc(n);
   uint8_t       *bad  = (uint8_t*)malloc(fx_len);
   uint8_t       *got  = NULL;
   const uint8_t *at   = NULL;
   r7z_archive_t *a    = NULL;
   unsigned       steps = 0, partial = 0;
   int            r, moved = 0, wrong = 0;

   text(want, unit);
   for (k = 1; k < n / unit; k++)
      memcpy(want + k * unit, want, unit);

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

/* blocks.txt out of LZMA2 blocks that each start the dictionary over:
 * whole, and in slices */
static void lzma2_blocks(void)
{
   size_t         n    = 600000, k, len = 0;
   uint8_t       *want = (uint8_t*)malloc(n);
   uint8_t       *got  = NULL;
   r7z_archive_t *a    = NULL;
   int            r;
   text(want, 2000);
   for (k = 1; k < 300; k++)
      memcpy(want + k * 2000, want, 2000);
   if (r7z_archive_open(&a, fx_lzma2_blocks, sizeof(fx_lzma2_blocks))
         != R7Z_OK)
      check(0, "LZMA2 blocks", "opens");
   else
   {
      r = r7z_archive_extract(a, 0, &got, &len);
      check(r == R7Z_OK && len == n && !memcmp(got, want, n),
            "LZMA2 blocks", "extracted whole");
      free(got);
      r7z_archive_close(a);
   }
   a = NULL;
   if (r7z_archive_open(&a, fx_lzma2_blocks, sizeof(fx_lzma2_blocks))
         == R7Z_OK)
   {
      got = NULL;
      while ((r = r7z_archive_extract_slice(a, 0, &got, &len))
            == R7Z_PENDING) { }
      check(r == R7Z_OK && len == n && !memcmp(got, want, n),
            "LZMA2 blocks", "extracted in slices");
      free(got);
      r7z_archive_close(a);
   }
   free(want);
}

/* lits.bin: 8200 octets of 64 values from '0' on */
static void lits(uint8_t *d, size_t n)
{
   uint32_t x = 7;
   size_t   i;
   for (i = 0; i < n; i++)
   {
      x    = lcg(x);
      d[i] = (uint8_t)(0x30 + (x >> 16) % 64);
   }
}

/* An LZMA stream of nearly all literals through a window of its 4 KiB
 * dictionary, wrapping twice: each wrap's first literal takes its
 * context from the window's last octet, and its position from the
 * stream's start. Fed input 100 octets at a time. */
static void lzma_ring(void)
{
   size_t          n     = 8200, made = 0, fed = 13;
   uint8_t        *want  = (uint8_t*)malloc(n);
   uint8_t        *got   = (uint8_t*)malloc(n);
   uint8_t         win[4096];
   rlzma_stream_t *ls    = (rlzma_stream_t*)malloc(sizeof(*ls));
   uint16_t       *probs = (uint16_t*)malloc(
         RLZMA_STREAM_NUM_PROBS(3, 0) * sizeof(uint16_t));
   int             ok;
   lits(want, n);
   ok = rlzma_stream_init(ls, fx_lzma_ring, probs, win, sizeof(win))
      == RLZMA_OK;
   if (ok)
      rlzma_stream_reset(ls);
   while (ok && made < n)
   {
      size_t from  = ls->dic_pos;
      size_t limit = sizeof(win);
      size_t take  = sizeof(fx_lzma_ring) - fed;
      int    status;
      if (take > 100)
         take = 100;
      if (limit - from > n - made)
         limit = from + (n - made);
      if (rlzma_stream_decode(ls, limit, fx_lzma_ring + fed, &take,
               fed + take == sizeof(fx_lzma_ring), &status) != RLZMA_OK
            || (!take && ls->dic_pos == from))
         ok = 0;
      fed += take;
      memcpy(got + made, win + from, ls->dic_pos - from);
      made += ls->dic_pos - from;
      if (ls->dic_pos == sizeof(win))
         ls->dic_pos = 0;
   }
   check(ok && made == n && !memcmp(got, want, n), "LZMA",
         "through a window its dictionary's size, wrapping");
   free(want);
   free(got);
   free(ls);
   free(probs);
}

/* code.so: its size and CRC */
#define ARM64_LEN 13736
#define ARM64_CRC 0xc73465bdu

/* 96 instruction words: a BL, an ADRP within the filter's range (at
 * both ends of it first) and one past it, by turns */
static void arm64_edges(uint8_t *d)
{
   int k;
   for (k = 0; k < 96; k++)
   {
      uint32_t w;
      int32_t  imm;
      if (k % 3 == 0)
         w = 0x94000000u | ((uint32_t)(k * 7919 - 300000) & 0x3FFFFFFu);
      else
      {
         if (k % 3 == 1)
            imm = k == 1 ? -0x20000 : k == 4 ? 0x1FFFF
               : (k * 4099) % 0x40000 - 0x20000;
         else
            imm = (0x20000 + k * 37) * (k % 2 ? 1 : -1);
         w = 0x90000000u | (((uint32_t)imm & 3) << 29)
           | ((((uint32_t)imm >> 2) & 0x7FFFF) << 5) | (uint32_t)(k & 31);
      }
      d[k * 4]     = (uint8_t)w;
      d[k * 4 + 1] = (uint8_t)(w >> 8);
      d[k * 4 + 2] = (uint8_t)(w >> 16);
      d[k * 4 + 3] = (uint8_t)(w >> 24);
   }
}

/* code.so back out of 7-Zip's ARM64 filter, and out of XZ Utils', which
 * started at 4096: converted from 0 it is not code.so */
static void arm64(void)
{
   r7z_archive_t *a   = NULL;
   uint8_t       *got = NULL;
   uint8_t       *buf = (uint8_t*)malloc(ARM64_LEN);
   uint16_t      *probs = (uint16_t*)malloc(RLZMA2_NUM_PROBS * sizeof(uint16_t));
   rlzma2_dec_t  *dec = (rlzma2_dec_t*)malloc(sizeof(*dec));
   size_t         len = 0, fed = sizeof(fx_arm64_xz);
   int            status = 0;

   if (r7z_archive_open(&a, fx_arm64, sizeof(fx_arm64)) != R7Z_OK)
      check(0, "ARM64", "opens");
   else
   {
      int r = r7z_archive_extract(a, 0, &got, &len);
      check(r == R7Z_OK && len == ARM64_LEN
            && encoding_crc32(0, got, len) == ARM64_CRC, "ARM64",
            "7-Zip's extracted");
      free(got);
      r7z_archive_close(a);
   }

   /* 64 KiB of dictionary: property 8 */
   check(   rlzma2_dec_init(dec, 8, probs, buf, ARM64_LEN) == RLZMA_OK
         && rlzma2_dec_decode(dec, ARM64_LEN, fx_arm64_xz, &fed, 1,
               &status) == RLZMA_OK
         && dec->lzma.dic_pos == ARM64_LEN, "ARM64", "XZ Utils' unpacked");
   {
      uint8_t *at0 = (uint8_t*)malloc(ARM64_LEN);
      memcpy(at0, buf, ARM64_LEN);
      rfilters_arm64_decode(4096, buf, ARM64_LEN);
      check(encoding_crc32(0, buf, ARM64_LEN) == ARM64_CRC, "ARM64",
            "XZ Utils' converted back from 4096");
      rfilters_arm64_decode(0, at0, ARM64_LEN);
      check(encoding_crc32(0, at0, ARM64_LEN) != ARM64_CRC, "ARM64",
            "and not from 0");
      free(at0);
   }
   {
      uint8_t want[384], got_e[384];
      fed = sizeof(fx_arm64_edges_xz);
      arm64_edges(want);
      check(   rlzma2_dec_init(dec, 8, probs, got_e, sizeof(got_e)) == RLZMA_OK
            && rlzma2_dec_decode(dec, sizeof(got_e), fx_arm64_edges_xz,
                  &fed, 1, &status) == RLZMA_OK
            && dec->lzma.dic_pos == sizeof(got_e), "ARM64, edges",
            "XZ Utils' unpacked");
      rfilters_arm64_decode(4194304, got_e, sizeof(got_e));
      check(!memcmp(got_e, want, sizeof(want)), "ARM64, edges",
            "back, forward, at the ends of the range and past them");
   }
   free(buf);
   free(probs);
   free(dec);
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

   /* a piece at a time: input in parts, what each leaves unread given
    * again in front of the next, out through windows that matches
    * reach back past */
   {
      static const size_t parts[3] = { 1100, 4096, 65536 };
      /* the last by turns: 777, 70000, 1, 65536 and 30000 */
      static const size_t wins[4]  = { 1, 777, 70000, 0 };
      static const size_t turns[5] = { 777, 70000, 1, 65536, 30000 };
      unsigned a, b;
      for (a = 0; a < 3; a++)
         for (b = 0; b < 4; b++)
         {
            void    *z    = rinflate64_new();
            uint8_t *in   = (uint8_t*)malloc(parts[a] + RINFLATE64_LOOKAHEAD);
            uint8_t *win  = (uint8_t*)malloc(70000);
            unsigned call = 0;
            size_t   fed  = 0, have = 0, made = 0;
            int      st   = RDEFLATE_PROCESS_NEXT, ok = 1, more = 1;
            while (st == RDEFLATE_PROCESS_NEXT && ok)
            {
               size_t used = 0, wr = 0;
               size_t room = wins[b] ? wins[b] : turns[call++ % 5];
               /* more input only when it was asked for: the window did
                * not fill */
               if (more && fed < len)
               {
                  size_t k = len - fed < parts[a] ? len - fed : parts[a];
                  memcpy(in + have, s + fed, k);
                  fed  += k;
                  have += k;
               }
               st    = rinflate64_process(z, in, have, fed == len, &used,
                     win, room, &wr);
               more  = wr < room;
               if (     wr > n - made || memcmp(win, want + made, wr)
                     || (more && fed < len
                        && have - used >= RINFLATE64_LOOKAHEAD)
                     || (!used && !wr && fed == len))
                  ok = 0;
               made += wr;
               memmove(in, in + used, have - used);
               have -= used;
            }
            check(ok && st == RDEFLATE_PROCESS_END && made == n,
                  "Deflate64", "a piece at a time");
            rinflate64_free(z);
            free(in);
            free(win);
         }
   }

   /* a distance back to the first octet, and one before it, the
    * first decoded through a window short of where it reaches */
   b.p = s; b.at = 0; b.acc = 0; b.n = 0;
   put(&b, 1, 1);
   put(&b, 1, 2);
   for (i = 0; i < 40000; i++)
      put_lit(&b, want[i]);
   put_match(&b, 300, 40000);
   put_lit(&b, 256);
   if (b.n)
      put(&b, 0, 8 - b.n);
   {
      void  *z = rinflate64_new();
      size_t used = 0, wr = 0, made = 0;
      int    st;
      do
      {
         st    = rinflate64_process(z, s + used, b.at - used, 1, &w, got + made,
               5000, &wr);
         used += w;
         made += wr;
      } while (st == RDEFLATE_PROCESS_NEXT && wr);
      check(st == RDEFLATE_PROCESS_END && made == 40300
            && !memcmp(got, want, 40000) && !memcmp(got + 40000, want, 300),
            "Deflate64", "a distance back to the first octet");
      rinflate64_free(z);
   }
   b.p = s; b.at = 0; b.acc = 0; b.n = 0;
   put(&b, 1, 1);
   put(&b, 1, 2);
   for (i = 0; i < 40000; i++)
      put_lit(&b, want[i]);
   put_match(&b, 300, 40001);
   put_lit(&b, 256);
   if (b.n)
      put(&b, 0, 8 - b.n);
   check(rinflate64_decode(got, n, s, b.at, &w) == RDEFLATE_PROCESS_ERROR,
         "Deflate64", "a distance one before the first octet");

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
   staged("PPMd, stepped", fx_big_ppmd, sizeof(fx_big_ppmd), 20000);
   staged("Deflate, stepped", fx_big_deflate, sizeof(fx_big_deflate),
         20000);
#ifdef HAVE_RBZIP2
   staged("BZip2, stepped", fx_big_bzip2, sizeof(fx_big_bzip2), 2000);
#endif
   streamed("PPMd", fx_ppmd, sizeof(fx_ppmd));
   streamed("PPMd, order 2", fx_ppmd_o2, sizeof(fx_ppmd_o2));
   streamed("Deflate", fx_deflate, sizeof(fx_deflate));
   streamed("Deflate64", fx_deflate64, sizeof(fx_deflate64));
   streamed("PPMd, big", fx_big_ppmd, sizeof(fx_big_ppmd));
   streamed("Deflate, big", fx_big_deflate, sizeof(fx_big_deflate));
#ifdef HAVE_RBZIP2
   streamed("BZip2", fx_bzip2, sizeof(fx_bzip2));
   streamed("BZip2, blocks", fx_big_bzip2, sizeof(fx_big_bzip2));
#endif
   streamed("LZMA2 blocks", fx_lzma2_blocks, sizeof(fx_lzma2_blocks));
   streamed("ARM64", fx_arm64, sizeof(fx_arm64));
   deflate64();
   lzma2_blocks();
   lzma_ring();
   arm64();
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("r7z_methods_test: every coder's archive extracted and streamed, PPMd, Deflate and BZip2 a step at a time, Deflate64 whole, LZMA2 blocks, LZMA through a ring, ARM64\n");
   return 0;
}

/* A small bzip2 writer for the tests: real streams, simply made. The
 * transform is a sort of the block's rotations by prefix doubling,
 * the tables two to six, one sized to the symbols' frequencies and
 * the others plain or lengthened from it, chosen for each fifty
 * symbols by a seeded generator so every selector shape is written.
 * Not for production: it is slow, and its ratio is not bzip2's. */

#ifndef BZ_WRITE_H
#define BZ_WRITE_H

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* nonzero: each block's CRC written wrong, and the stream's from those */
static int bzw_lie;

struct bzw
{
   uint8_t *out;
   size_t   cap;
   size_t   len;
   uint64_t acc;
   unsigned nb;
   uint32_t crc[256];
   uint32_t seed;
   int      fail;
};

static void bzw_bits(struct bzw *w, uint32_t v, unsigned k)
{
   w->acc = (w->acc << k) | (v & ((1u << k) - 1));
   w->nb += k;
   while (w->nb >= 8)
   {
      if (w->len < w->cap)
         w->out[w->len] = (uint8_t)(w->acc >> (w->nb - 8));
      else
         w->fail = 1;
      w->len++;
      w->nb -= 8;
   }
}

static uint32_t bzw_rand(struct bzw *w)
{
   w->seed = w->seed * 1103515245u + 12345u;
   return w->seed >> 8;
}

/* the rotations' order, by prefix doubling */
static const uint32_t *bzw_rank;
static uint32_t        bzw_n, bzw_k;

static int bzw_cmp(const void *a, const void *b)
{
   uint32_t i = *(const uint32_t*)a, j = *(const uint32_t*)b;
   if (bzw_rank[i] != bzw_rank[j])
      return bzw_rank[i] < bzw_rank[j] ? -1 : 1;
   i = (i + bzw_k) % bzw_n;
   j = (j + bzw_k) % bzw_n;
   if (bzw_rank[i] != bzw_rank[j])
      return bzw_rank[i] < bzw_rank[j] ? -1 : 1;
   return 0;
}

static void bzw_sort(const uint8_t *blk, uint32_t n, uint32_t *sa)
{
   uint32_t *rank = (uint32_t*)malloc(n * sizeof(uint32_t));
   uint32_t *next = (uint32_t*)malloc(n * sizeof(uint32_t));
   uint32_t  i, k;
   for (i = 0; i < n; i++)
   {
      sa[i]   = i;
      rank[i] = blk[i];
   }
   bzw_rank = rank;
   bzw_n    = n;
   for (k = 1; ; k <<= 1)
   {
      bzw_k = k;
      qsort(sa, n, sizeof(uint32_t), bzw_cmp);
      next[sa[0]] = 0;
      for (i = 1; i < n; i++)
         next[sa[i]] = next[sa[i - 1]] + (bzw_cmp(&sa[i - 1], &sa[i]) != 0);
      memcpy(rank, next, n * sizeof(uint32_t));
      if (rank[sa[n - 1]] == n - 1 || k >= n)
         break;
   }
   free(rank);
   free(next);
}

static uint32_t bzw_crc(const struct bzw *w, uint32_t v, const uint8_t *p,
      size_t n)
{
   for (; n; n--, p++)
      v = (v << 8) ^ w->crc[(v >> 24) ^ *p];
   return v;
}

/* @z ranks of 0 as RUNA (0) and RUNB (1), bijective base 2 */
static void bzw_zrun(uint16_t *syms, uint32_t *nsym, uint32_t z)
{
   if (!z)
      return;
   z--;
   for (;;)
   {
      syms[(*nsym)++] = (uint16_t)(z & 1);
      if (z < 2)
         break;
      z = (z - 2) / 2;
   }
}

/* One block of the run-coded @blk (@n octets), which stands for
 * octets whose CRC is @crc. */
static void bzw_block(struct bzw *w, const uint8_t *blk, uint32_t n,
      uint32_t crc)
{
   uint32_t *sa   = (uint32_t*)malloc(n * sizeof(uint32_t));
   uint16_t *syms = (uint16_t*)malloc((n + 1) * sizeof(uint16_t));
   uint8_t   used[256], mtf[256], len[6][258], sel_pos[6];
   uint32_t  freq[258];
   uint32_t  i, j, orig = 0, nsym = 0, zrun = 0, n_in_use = 0, alpha;
   uint32_t  n_groups, n_sel, flat;

   bzw_sort(blk, n, sa);
   memset(used, 0, sizeof(used));
   for (i = 0; i < n; i++)
      used[blk[i]] = 1;
   for (i = 0; i < 256; i++)
      if (used[i])
         mtf[n_in_use++] = (uint8_t)i;
   alpha = n_in_use + 2;

   /* the last column, move-to-front, runs of rank 0 in base 2 */
   memset(freq, 0, sizeof(freq));
   for (i = 0; i < n; i++)
   {
      uint8_t c = blk[(sa[i] + n - 1) % n];
      uint32_t r = 0;
      if (sa[i] == 0)
         orig = i;
      while (mtf[r] != c)
         r++;
      if (!r)
      {
         zrun++;
         continue;
      }
      bzw_zrun(syms, &nsym, zrun);
      zrun = 0;
      memmove(mtf + 1, mtf, r);
      mtf[0] = c;
      syms[nsym++] = (uint16_t)(r + 1);
   }
   bzw_zrun(syms, &nsym, zrun);
   syms[nsym++] = (uint16_t)(n_in_use + 1);
   for (i = 0; i < nsym; i++)
      freq[syms[i]]++;

   /* table 0 from the frequencies, kept to Kraft's inequality */
   for (j = 0; j < alpha; j++)
   {
      uint32_t f = freq[j] + 1, l = 1;
      while (l < 17 && ((uint64_t)f << l) < (uint64_t)nsym + alpha)
         l++;
      len[0][j] = (uint8_t)l;
   }
   for (;;)
   {
      uint64_t kraft = 0;
      uint32_t worst = alpha;
      for (j = 0; j < alpha; j++)
         kraft += (uint64_t)1 << (17 - len[0][j]);
      if (kraft <= ((uint64_t)1 << 17))
         break;
      for (j = 0; j < alpha; j++)
         if (len[0][j] < 17 && (worst == alpha || freq[j] < freq[worst]))
            worst = j;
      len[0][worst]++;
   }
   /* table 1 plain, the others table 0 or 1 lengthened */
   for (flat = 1; (1u << flat) < alpha; flat++) { }
   n_groups = 2 + bzw_rand(w) % 5;
   for (j = 0; j < alpha; j++)
   {
      len[1][j] = (uint8_t)flat;
      for (i = 2; i < n_groups; i++)
      {
         uint32_t l = len[i & 1][j] + (bzw_rand(w) % 3);
         len[i][j] = (uint8_t)(l > 20 ? 20 : l);
      }
   }

   n_sel = (nsym + 49) / 50;
   bzw_bits(w, 0x314159, 24);
   bzw_bits(w, 0x265359, 24);
   bzw_bits(w, crc >> 16, 16);
   bzw_bits(w, crc, 16);
   bzw_bits(w, 0, 1);
   bzw_bits(w, orig, 24);
   {
      uint32_t m = 0;
      for (i = 0; i < 16; i++)
         for (j = 0; j < 16; j++)
            if (used[i * 16 + j])
            {
               m |= 0x8000u >> i;
               break;
            }
      bzw_bits(w, m, 16);
      for (i = 0; i < 16; i++)
         if (m & (0x8000u >> i))
         {
            uint32_t b = 0;
            for (j = 0; j < 16; j++)
               if (used[i * 16 + j])
                  b |= 0x8000u >> j;
            bzw_bits(w, b, 16);
         }
   }
   bzw_bits(w, n_groups, 3);
   bzw_bits(w, n_sel, 15);
   {
      uint8_t *sel = (uint8_t*)malloc(n_sel);
      for (i = 0; i < n_groups; i++)
         sel_pos[i] = (uint8_t)i;
      for (i = 0; i < n_sel; i++)
      {
         uint8_t t = (uint8_t)(bzw_rand(w) % 4 ? (bzw_rand(w) & 1)
               : bzw_rand(w) % n_groups);
         sel[i] = t;
         for (j = 0; sel_pos[j] != t; j++)
            bzw_bits(w, 1, 1);
         bzw_bits(w, 0, 1);
         memmove(sel_pos + 1, sel_pos, j);
         sel_pos[0] = t;
      }
      for (i = 0; i < n_groups; i++)
      {
         uint32_t cur = len[i][0];
         bzw_bits(w, cur, 5);
         for (j = 0; j < alpha; j++)
         {
            while (cur < len[i][j]) { bzw_bits(w, 2, 2); cur++; }
            while (cur > len[i][j]) { bzw_bits(w, 3, 2); cur--; }
            bzw_bits(w, 0, 1);
         }
      }
      /* canonical codes, as the reader assigns them */
      {
         uint32_t code[6][258];
         for (i = 0; i < n_groups; i++)
         {
            uint32_t c = 0, l;
            for (l = 1; l <= 20; l++)
            {
               for (j = 0; j < alpha; j++)
                  if (len[i][j] == l)
                     code[i][j] = c++;
               c <<= 1;
            }
         }
         for (i = 0; i < nsym; i++)
         {
            uint32_t t = sel[i / 50];
            bzw_bits(w, code[t][syms[i]], len[t][syms[i]]);
         }
      }
      free(sel);
   }
   free(sa);
   free(syms);
}

/* @src (@n octets) as one stream at block size @level (1-9) into @out;
 * returns the stream's length, 0 when @cap is short. */
static size_t bz_write(uint8_t *out, size_t cap, const uint8_t *src,
      size_t n, unsigned level, uint32_t seed)
{
   struct bzw w;
   uint32_t   max = 100000u * level - 19, combined = 0, i, j;
   uint8_t   *blk = (uint8_t*)malloc(max + 5);
   size_t     at  = 0;

   memset(&w, 0, sizeof(w));
   w.out  = out;
   w.cap  = cap;
   w.seed = seed;
   for (i = 0; i < 256; i++)
   {
      uint32_t c = i << 24;
      for (j = 0; j < 8; j++)
         c = (c & 0x80000000u) ? (c << 1) ^ 0x04c11db7u : c << 1;
      w.crc[i] = c;
   }
   bzw_bits(&w, 'B', 8);
   bzw_bits(&w, 'Z', 8);
   bzw_bits(&w, 'h', 8);
   bzw_bits(&w, '0' + level, 8);
   while (at < n)
   {
      uint32_t bn = 0, crc;
      size_t   from = at;
      /* runs of four or more as four and a count, never split */
      while (at < n && bn + 5 <= max)
      {
         size_t r = 1;
         while (at + r < n && src[at + r] == src[at] && r < 255)
            r++;
         if (r < 4)
         {
            blk[bn++] = src[at++];
            continue;
         }
         memset(blk + bn, src[at], 4);
         blk[bn + 4] = (uint8_t)(r - 4);
         bn += 5;
         at += r;
      }
      crc      = ~bzw_crc(&w, 0xffffffffu, src + from, at - from)
               ^ (bzw_lie ? 1u : 0u);
      combined = ((combined << 1) | (combined >> 31)) ^ crc;
      bzw_block(&w, blk, bn, crc);
   }
   bzw_bits(&w, 0x177245, 24);
   bzw_bits(&w, 0x385090, 24);
   bzw_bits(&w, combined >> 16, 16);
   bzw_bits(&w, combined, 16);
   if (w.nb)
      bzw_bits(&w, 0, 8 - w.nb);
   free(blk);
   return w.fail ? 0 : w.len;
}

#endif

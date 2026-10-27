/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (encoding_rbzip2.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* bzip2 stream decoding; see <encodings/rbzip2.h> for the format. */

#include <stdlib.h>
#include <string.h>

#include <encodings/rbzip2.h>

#define RBZ_GROUPS_MAX 6
#define RBZ_ALPHA_MAX  258        /* 256 octets, two run symbols */
#define RBZ_SEL_MAX    18002      /* 900 000 / 50, rounded up, and one */
#define RBZ_LEN_MAX    20
#define RBZ_LUT_BITS   10         /* codes this short decode in one look */
#define RBZ_GROUP_SIZE 50         /* symbols coded with one table */

/* A Huffman table, canonical: codes of one length are consecutive and
 * in symbol order, each length's after the shorter ones'. */
struct rbz_table
{
   uint16_t lut[1 << RBZ_LUT_BITS]; /* symbol << 5 | length; 0: longer */
   uint16_t perm[RBZ_ALPHA_MAX];    /* symbols by length, then value */
   uint16_t count[RBZ_LEN_MAX + 1]; /* codes of each length */
   unsigned max_len;
};

struct rbzip2
{
   uint32_t        *tt;             /* a block, then its inverse */
   uint32_t         tt_cap;
   uint32_t         crc[8][256];    /* CRC-32, MSB first, eight at a time */
   uint32_t         unz[256];       /* each octet's count in the block */
   struct rbz_table table[RBZ_GROUPS_MAX];
   uint8_t          len[RBZ_GROUPS_MAX][RBZ_ALPHA_MAX];
   uint8_t          selector[RBZ_SEL_MAX];
};

/* The input, read most significant bit first through a 64-bit window.
 * Past the end it reads zeros, counted in @over: a stream is short when
 * more of them were taken than the window still holds. */
struct rbz_in
{
   const uint8_t *p;
   const uint8_t *end;
   uint64_t       buf;
   size_t         over;
   unsigned       n;
};

#define RBZ_FILL(p, end, buf, n, over) \
   while ((n) <= 56) \
   { \
      (buf) = ((buf) << 8) | ((p) < (end) ? *(p)++ : ((over)++, 0u)); \
      (n)  += 8; \
   }

/* @k octets wide or less, 1 <= k <= 24 */
static uint32_t rbz_bits(struct rbz_in *in, unsigned k)
{
   RBZ_FILL(in->p, in->end, in->buf, in->n, in->over)
   in->n -= k;
   return (uint32_t)(in->buf >> in->n) & ((1u << k) - 1);
}

static int rbz_short(const struct rbz_in *in)
{
   return in->over * 8 > in->n;
}

rbzip2_t *rbzip2_new(void)
{
   rbzip2_t *bz = (rbzip2_t*)calloc(1, sizeof(*bz));
   unsigned  i, j;
   if (!bz)
      return NULL;
   for (i = 0; i < 256; i++)
   {
      uint32_t c = (uint32_t)i << 24;
      for (j = 0; j < 8; j++)
         c = (c & 0x80000000u) ? (c << 1) ^ 0x04c11db7u : c << 1;
      bz->crc[0][i] = c;
   }
   for (j = 1; j < 8; j++)
      for (i = 0; i < 256; i++)
         bz->crc[j][i] = (bz->crc[j - 1][i] << 8)
            ^ bz->crc[0][bz->crc[j - 1][i] >> 24];
   return bz;
}

void rbzip2_free(rbzip2_t *bz)
{
   if (!bz)
      return;
   free(bz->tt);
   free(bz);
}

/* The table for code lengths @len over @alpha symbols. Codes may leave
 * some of the code space unused, which decodes as an error, but none
 * may claim space another already has. */
static int rbz_table_build(struct rbz_table *t, const uint8_t *len,
      unsigned alpha)
{
   uint16_t offs[RBZ_LEN_MAX + 2];
   unsigned i, l;
   int32_t  left = 1;
   uint32_t code = 0;
   unsigned at   = 0;

   memset(t->count, 0, sizeof(t->count));
   for (i = 0; i < alpha; i++)
      t->count[len[i]]++;
   t->max_len = 0;
   for (l = 1; l <= RBZ_LEN_MAX; l++)
   {
      left = (left << 1) - t->count[l];
      if (left < 0)
         return RBZIP2_ERROR_DATA;
      if (t->count[l])
         t->max_len = l;
   }
   offs[1] = 0;
   for (l = 1; l <= RBZ_LEN_MAX; l++)
      offs[l + 1] = (uint16_t)(offs[l] + t->count[l]);
   for (i = 0; i < alpha; i++)
      t->perm[offs[len[i]]++] = (uint16_t)i;

   /* codes up to RBZ_LUT_BITS long, every window they begin */
   memset(t->lut, 0, sizeof(t->lut));
   for (l = 1; l <= RBZ_LUT_BITS; l++)
   {
      unsigned c;
      for (c = 0; c < t->count[l]; c++, code++, at++)
      {
         unsigned span  = 1u << (RBZ_LUT_BITS - l);
         unsigned first = code << (RBZ_LUT_BITS - l);
         uint16_t e     = (uint16_t)((t->perm[at] << 5) | l);
         unsigned j;
         for (j = 0; j < span; j++)
            t->lut[first + j] = e;
      }
      code <<= 1;
   }
   return RBZIP2_OK;
}

/* A code longer than the table looks at: @peek holds the next max_len
 * bits. Returns the symbol, its length in *@used, or -1. */
static int rbz_slow(const struct rbz_table *t, uint32_t peek,
      unsigned *used)
{
   uint32_t code  = 0;
   uint32_t first = 0;
   unsigned index = 0;
   unsigned l;
   for (l = 1; l <= t->max_len; l++)
   {
      uint32_t cnt = t->count[l];
      code |= (peek >> (t->max_len - l)) & 1;
      if (code - first < cnt)
      {
         *used = l;
         return t->perm[index + code - first];
      }
      index += cnt;
      first  = (first + cnt) << 1;
      code <<= 1;
   }
   return -1;
}

/* The block CRC of @len octets: eight at a time, the table for each
 * octet giving its effect that many octets on. */
static uint32_t rbz_crc(const rbzip2_t *bz, const uint8_t *p, size_t len)
{
   uint32_t v = 0xffffffffu;
   for (; len >= 8; p += 8, len -= 8)
   {
      uint32_t a = v ^ (((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
                     | ((uint32_t)p[2] << 8) | p[3]);
      v = bz->crc[7][a >> 24]         ^ bz->crc[6][(a >> 16) & 0xff]
        ^ bz->crc[5][(a >> 8) & 0xff] ^ bz->crc[4][a & 0xff]
        ^ bz->crc[3][p[4]]            ^ bz->crc[2][p[5]]
        ^ bz->crc[1][p[6]]            ^ bz->crc[0][p[7]];
   }
   for (; len; p++, len--)
      v = (v << 8) ^ bz->crc[0][(v >> 24) ^ *p];
   return ~v;
}

/* One block, after its magic: decoded and appended at dst + *@o.
 * *@crc receives the block's stored CRC once it has been checked. */
static int rbz_block(rbzip2_t *bz, struct rbz_in *in, uint32_t max_block,
      uint8_t *dst, size_t cap, size_t *o, uint32_t *crc)
{
   uint8_t  mtf[256];
   uint8_t  pos[RBZ_GROUPS_MAX];
   uint32_t stored, orig, used, i, j;
   uint32_t nblock = 0, n_sel, n_groups, n_in_use = 0, alpha, eob;
   uint32_t run = 0, run_bit = 1, sel = 0, left = 0;
   uint32_t *tt = bz->tt;
   const struct rbz_table *t = NULL;
   const uint8_t *p, *end;
   uint64_t buf;
   unsigned n;
   size_t   over;
   int      e;

   stored  = rbz_bits(in, 16) << 16;
   stored |= rbz_bits(in, 16);
   if (rbz_bits(in, 1))
      return RBZIP2_ERROR_DATA;    /* randomised */
   orig = rbz_bits(in, 24);

   /* the octets the block uses, sixteen at a time */
   used = rbz_bits(in, 16);
   for (i = 0; i < 16; i++)
      if (used & (0x8000u >> i))
      {
         uint32_t m = rbz_bits(in, 16);
         for (j = 0; j < 16; j++)
            if (m & (0x8000u >> j))
               mtf[n_in_use++] = (uint8_t)(i * 16 + j);
      }
   if (!n_in_use)
      return RBZIP2_ERROR_DATA;
   alpha = n_in_use + 2;
   eob   = n_in_use + 1;

   n_groups = rbz_bits(in, 3);
   n_sel    = rbz_bits(in, 15);
   if (n_groups < 2 || n_groups > RBZ_GROUPS_MAX || !n_sel)
      return RBZIP2_ERROR_DATA;
   /* each table's selector, move-to-front coded in unary */
   for (i = 0; i < n_groups; i++)
      pos[i] = (uint8_t)i;
   for (i = 0; i < n_sel; i++)
   {
      uint8_t v;
      j = 0;
      while (rbz_bits(in, 1))
         if (++j >= n_groups)
            return RBZIP2_ERROR_DATA;
      v = pos[j];
      for (; j > 0; j--)
         pos[j] = pos[j - 1];
      pos[0] = v;
      if (i < RBZ_SEL_MAX)
         bz->selector[i] = v;
   }
   if (n_sel > RBZ_SEL_MAX)
      n_sel = RBZ_SEL_MAX;

   /* each table's code lengths, as steps from a five-bit start */
   for (i = 0; i < n_groups; i++)
   {
      uint32_t cur = rbz_bits(in, 5);
      for (j = 0; j < alpha; j++)
      {
         for (;;)
         {
            if (cur < 1 || cur > RBZ_LEN_MAX)
               return RBZIP2_ERROR_DATA;
            if (!rbz_bits(in, 1))
               break;
            cur = rbz_bits(in, 1) ? cur - 1 : cur + 1;
         }
         bz->len[i][j] = (uint8_t)cur;
      }
      if ((e = rbz_table_build(&bz->table[i], bz->len[i], alpha)) != RBZIP2_OK)
         return e;
   }
   if (rbz_short(in))
      return RBZIP2_ERROR_DATA;

   /* the symbols: runs of rank 0, other ranks, the end of the block */
   memset(bz->unz, 0, sizeof(bz->unz));
   p    = in->p;
   end  = in->end;
   buf  = in->buf;
   n    = in->n;
   over = in->over;
   for (;;)
   {
      uint32_t sym, ent;
      if (!left)
      {
         if (sel >= n_sel)
            return RBZIP2_ERROR_DATA;
         t    = &bz->table[bz->selector[sel++]];
         left = RBZ_GROUP_SIZE;
      }
      left--;
      RBZ_FILL(p, end, buf, n, over)
      ent = t->lut[(uint32_t)(buf >> (n - RBZ_LUT_BITS)) & ((1u << RBZ_LUT_BITS) - 1)];
      if (ent)
      {
         n  -= ent & 31;
         sym = ent >> 5;
      }
      else
      {
         unsigned l = 0;
         int      s = rbz_slow(t, (uint32_t)(buf >> (n - t->max_len))
               & ((1u << t->max_len) - 1), &l);
         if (s < 0)
            return RBZIP2_ERROR_DATA;
         n  -= l;
         sym = (uint32_t)s;
      }

      if (sym <= 1)
      {
         /* RUNA adds one at this digit, RUNB two */
         if (run_bit > max_block)
            return RBZIP2_ERROR_DATA;
         run     += run_bit << sym;
         run_bit <<= 1;
         continue;
      }
      if (run)
      {
         uint8_t c = mtf[0];
         if (run > max_block - nblock)
            return RBZIP2_ERROR_DATA;
         bz->unz[c] += run;
         while (run--)
            tt[nblock++] = c;
         run     = 0;
         run_bit = 1;
      }
      if (sym == eob)
         break;
      if (sym >= alpha || nblock >= max_block)
         return RBZIP2_ERROR_DATA;
      {
         uint32_t r = sym - 1;
         uint8_t  c = mtf[r];
         memmove(mtf + 1, mtf, r);
         mtf[0] = c;
         bz->unz[c]++;
         tt[nblock++] = c;
      }
   }
   in->p    = p;
   in->buf  = buf;
   in->n    = n;
   in->over = over;
   if (rbz_short(in) || orig >= nblock)
      return RBZIP2_ERROR_DATA;

   /* the inverse transform: each octet linked to the one before it */
   {
      uint32_t sum = 0;
      for (i = 0; i < 256; i++)
      {
         uint32_t c = bz->unz[i];
         bz->unz[i] = sum;
         sum       += c;
      }
      for (i = 0; i < nblock; i++)
         tt[bz->unz[tt[i] & 0xff]++] |= i << 8;
   }

   /* and out, undoing the runs of four and a count */
   {
      size_t   at   = *o;
      size_t   from = at;
      uint32_t tp   = tt[orig] >> 8;
      uint32_t last = 0x100;      /* no octet */
      unsigned rep  = 0;
      uint32_t k    = 0;
      while (k < nblock)
      {
         uint32_t c;
         tp = tt[tp];
         c  = tp & 0xff;
         tp >>= 8;
         k++;
         if (at >= cap)
            return RBZIP2_ERROR_SPACE;
         dst[at++] = (uint8_t)c;
         if (c != last)
         {
            last = c;
            rep  = 1;
            continue;
         }
         if (++rep < 4)
            continue;
         /* four alike: the next octet counts more of them */
         rep = 0;
         if (k == nblock)
            break;
         tp = tt[tp];
         c  = tp & 0xff;
         tp >>= 8;
         k++;
         if (c > cap - at)
            return RBZIP2_ERROR_SPACE;
         memset(dst + at, (int)last, c);
         at += c;
      }
      if (rbz_crc(bz, dst + from, at - from) != stored)
         return RBZIP2_ERROR_DATA;
      *o = at;
   }
   *crc = stored;
   return RBZIP2_OK;
}

int rbzip2_decode(rbzip2_t *bz, uint8_t *dst, size_t dst_len,
      const uint8_t *src, size_t src_len, size_t *wrote)
{
   struct rbz_in in;
   uint32_t max_block, combined = 0;
   size_t   o = 0;

   if (wrote)
      *wrote = 0;
   if (     !bz || src_len < 4
         || src[0] != 'B' || src[1] != 'Z' || src[2] != 'h'
         || src[3] < '1' || src[3] > '9')
      return RBZIP2_ERROR_DATA;
   max_block = (uint32_t)(src[3] - '0') * 100000u;
   if (bz->tt_cap < max_block)
   {
      uint32_t *tt = (uint32_t*)malloc(max_block * sizeof(uint32_t));
      if (!tt)
         return RBZIP2_ERROR_MEM;
      free(bz->tt);
      bz->tt     = tt;
      bz->tt_cap = max_block;
   }

   in.p    = src + 4;
   in.end  = src + src_len;
   in.buf  = 0;
   in.n    = 0;
   in.over = 0;
   for (;;)
   {
      uint32_t hi = rbz_bits(&in, 24);
      uint32_t lo = rbz_bits(&in, 24);
      if (hi == 0x314159u && lo == 0x265359u)
      {
         uint32_t crc;
         int      e = rbz_block(bz, &in, max_block, dst, dst_len, &o, &crc);
         if (e != RBZIP2_OK)
            return e;
         combined = ((combined << 1) | (combined >> 31)) ^ crc;
      }
      else if (hi == 0x177245u && lo == 0x385090u)
      {
         uint32_t want = rbz_bits(&in, 16) << 16;
         want |= rbz_bits(&in, 16);
         if (rbz_short(&in) || want != combined)
            return RBZIP2_ERROR_DATA;
         break;
      }
      else
         return RBZIP2_ERROR_DATA;
   }
   if (wrote)
      *wrote = o;
   return RBZIP2_OK;
}

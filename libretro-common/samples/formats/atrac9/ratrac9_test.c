/* Self-test for ratrac9.c, built from the specification alone.
 * Includes ratrac9.c to reach its static tables and DCT.
 * Usage: ratrac9_test [path/to/tables.txt] */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../../formats/atrac9/ratrac9.c"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int failures = 0;

#define CHECK(cond, msg) \
   do { if (!(cond)) { failures++; printf("FAIL: %s (line %d)\n", msg, __LINE__); } } while (0)

/* ---- bit writer ---- */
typedef struct
{
   uint8_t buf[32768];
   size_t  pos;
} bw_t;

static void bw_put(bw_t *w, uint32_t v, unsigned n)
{
   while (n--)
   {
      if ((v >> n) & 1)
         w->buf[w->pos >> 3] |= (uint8_t)(0x80 >> (w->pos & 7));
      w->pos++;
   }
}

static uint32_t rng_state = 12345;
static uint32_t rng(void)
{
   rng_state = rng_state * 1103515245u + 12345u;
   return rng_state >> 8;
}

/* ---- valid frame generator, written from the spec ----
 * Produces random but valid frames for any config so multi-channel, stereo,
 * band extension (with noise) and every scale factor method get exercised. */
typedef struct
{
   unsigned rate, cc, fps, index;
   unsigned hr;
} gen_t;

static unsigned rr(unsigned lo, unsigned hi)   /* inclusive */
{
   return lo + rng() % (hi - lo + 1);
}

/* Writes one random Huffman symbol of book bk; returns the symbol. A LUT slot
 * is chosen uniformly (so short codes are likelier); its code is used only if
 * every window starting with it decodes the same way. */
static unsigned gen_huff(bw_t *w, const r9_book *bk)
{
   for (;;)
   {
      unsigned j  = rng() & ((1u << bk->bits) - 1);
      unsigned e  = r9_huff_lut[bk->off + j];
      unsigned ln = e & 15, sh, c, k, ok = 1;
      if (!ln)
         continue;
      sh = bk->bits - ln;
      c  = j >> sh;
      for (k = 0; k < (1u << sh); k++)
         if (r9_huff_lut[bk->off + (c << sh) + k] != e)
            ok = 0;
      if (!ok)
         continue;
      bw_put(w, c, ln);
      return e >> 4;
   }
}

static int sext(unsigned v, unsigned bits)
{
   unsigned h = 1u << (bits - 1);
   return (int)(v ^ h) - (int)h;
}

/* one mono or stereo block */
static void gen_block(gen_t *g, bw_t *w, int stereo)
{
   unsigned minb = r9_min_band[g->hr];
   unsigned band, units, sband = 0, sunits = 0, ext_en = 0, eband, eunits, bands = 0;
   unsigned gm, su, eu, sv, ev, gb, primary = 0, nch = stereo ? 2 : 1, c, i, k;
   unsigned bex_mode[2];
   int grad[31], sf[2][31];
   int first = g->index == 0;

   bw_put(w, first ? 0 : 1, 1);
   bw_put(w, 0, 1);                                   /* no reuse */
   band  = rr(minb, r9_max_band[g->rate]);
   units = r9_band_units[band];
   bw_put(w, band - minb, 4);
   if (stereo)
   {
      sband  = rr(minb, band);
      sunits = r9_band_units[sband];
      bw_put(w, sband - minb, 4);
   }
   if (units >= 13 && units <= 20 && (rng() & 3))
      ext_en = 1;
   bw_put(w, ext_en, 1);
   eband = band;
   if (ext_en)
   {
      eband = rr(band, r9_max_ext_band[g->hr]);
      bw_put(w, eband - minb, 4);
   }
   eunits = r9_band_units[eband];

   gm = rng() & 3;
   bw_put(w, gm, 2);
   if (gm)
   {
      eu = 31; ev = 31;
      su = rr(0, 31); sv = rr(0, 31);
      bw_put(w, su, 5); bw_put(w, sv, 5);
   }
   else
   {
      su = rr(0, 30); eu = rr(su > 0 ? su : 1, 31);
      sv = rr(2, 14); ev = rr(2, 14);
      bw_put(w, su, 6); bw_put(w, eu - 1, 6);
      bw_put(w, sv, 5); bw_put(w, ev, 5);
   }
   gb = rr(0, units < 15 ? units : 15);
   bw_put(w, gb, 4);

   /* gradient, spec 4.3 */
   for (i = 0; i < 31; i++)
      grad[i] = (int)(i < eu ? sv : ev);
   if (eu > su && ev != sv)
   {
      int n = (int)(eu - su), dv = (int)ev - (int)sv;
      double scale = ((dv > 0 ? dv : -dv) - 1) / 31.0;
      for (i = su; i < eu && i < 31; i++)
      {
         int t = (int)(r9_grad_base[(i - su) * 48 / (unsigned)n] * scale);
         grad[i] = dv > 0 ? (int)sv + 1 + t : (int)sv - 1 - t;
      }
   }

   if (stereo)
   {
      unsigned hs = rng() & 1;
      primary = rng() & 1;
      bw_put(w, primary, 1);
      bw_put(w, hs, 1);
      if (hs)
         for (i = sunits; i < units; i++)
            bw_put(w, rng() & 1, 1);
   }

   if (ext_en)
   {
      bands = r9_ext_groups[units - 13][2];
      if (stereo)
      {
         unsigned m = rng() & 3;
         bw_put(w, m, 2);
         bex_mode[1] = bands > 2 ? m : 4;
      }
      else
         bw_put(w, rng() & 1, 1);
   }
   if (rng() % 5)
   {
      bw_put(w, 1, 1);                                /* has extension data */
      if (!ext_en)
      {
         unsigned len = rr(0, 31);
         bw_put(w, rng() & 3, 2);
         bw_put(w, len, 5);
         bw_put(w, rng(), len > 24 ? 24 : len);
         if (len > 24)
            bw_put(w, rng(), len - 24);
      }
      else
      {
         unsigned m = rng() & 3, total = 0, len;
         bw_put(w, m, 2);
         bex_mode[0] = bands > 2 ? m : 4;
         for (c = 0; c < nch; c++)
            for (k = 0; k < r9_ext_count[bex_mode[c]][bands]; k++)
               total += r9_ext_bits[bex_mode[c]][bands][k];
         len = (rng() % 8 == 0) ? 0 : rr(total, 31);
         bw_put(w, len, 5);
         if (len)
            for (c = 0; c < nch; c++)
               for (k = 0; k < r9_ext_count[bex_mode[c]][bands]; k++)
                  bw_put(w, rng(), r9_ext_bits[bex_mode[c]][bands][k]);
      }
   }
   else
      bw_put(w, 0, 1);

   for (c = 0; c < nch; c++)
   {
      unsigned coded = (c == primary) ? units : sunits;
      int mask[31], p[31], fine[31];
      int *s = sf[c];
      int done = 0;

      for (i = 0; i < 31; i++)
         s[i] = 0;

      if (c == 0 && (rng() & 1))
      {
         /* delta offset (mode 0), if some base keeps every value in range */
         unsigned wr = rng() & 7, bits = rr(3, 6), mk = (1u << bits) - 1;
         const r9_book *bk = &r9_book_sfu[bits];
         int v[31], lo = 0, hi = 31;
         bw_t save = *w;
         bw_put(w, 0, 2);
         bw_put(w, wr, 3);
         {
            size_t basepos = w->pos;
            bw_put(w, 0, 5);
            bw_put(w, bits - 3, 2);
            v[0] = (int)(rng() & 7);
            bw_put(w, (unsigned)v[0], bits);
            for (i = 1; i < eunits; i++)
               v[i] = (int)(((unsigned)v[i - 1] + gen_huff(w, bk)) & mk);
            for (i = 0; i < eunits; i++)
            {
               int a = (int)r9_sf_weights[wr][i] - v[i];
               int b = 31 - v[i] + (int)r9_sf_weights[wr][i];
               if (a > lo) lo = a;
               if (b < hi) hi = b;
            }
            if (lo <= hi)
            {
               int base = lo + (int)(rng() % (unsigned)(hi - lo + 1));
               size_t keep = w->pos;
               w->pos = basepos;
               bw_put(w, (unsigned)base, 5);
               w->pos = keep;
               for (i = 0; i < eunits; i++)
                  s[i] = v[i] + base - (int)r9_sf_weights[wr][i];
               done = 1;
            }
            else
               *w = save;
         }
      }
      if (!done && c == 1 && (rng() & 1))
      {
         /* distance to channel 0 (mode 1) */
         unsigned bits = rr(2, 5);
         bw_put(w, 1, 2);
         bw_put(w, bits - 2, 2);
         for (i = 0; i < eunits; i++)
            s[i] = (sf[0][i] + sext(gen_huff(w, &r9_book_sfs[bits]), bits)) & 31;
         done = 1;
      }
      if (!done && c == 0)
      {
         /* fixed length (mode 1) */
         unsigned bits = rr(2, 5), base = 0;
         bw_put(w, 1, 2);
         bw_put(w, bits - 2, 2);
         if (bits < 5)
         {
            base = rr(0, 31 - ((1u << bits) - 1));
            if (rng() & 1)
               base = rr(0, 6);
            bw_put(w, base, 5);
         }
         for (i = 0; i < eunits; i++)
         {
            unsigned v = rng() & ((1u << bits) - 1);
            bw_put(w, v, bits);
            s[i] = (int)(v + base);
         }
         done = 1;
      }
      if (!done)
      {
         /* channel 1, delta offset with channel 0 as baseline (mode 2) */
         unsigned bits = rr(1, 4), mk = (1u << bits) - 1;
         int v[31], lo = 0, hi = 31, off;
         bw_t save = *w;
         size_t offpos;
         bw_put(w, 2, 2);
         offpos = w->pos;
         bw_put(w, 0, 5);
         bw_put(w, bits - 1, 2);
         v[0] = (int)(rng() & mk);
         bw_put(w, (unsigned)v[0], bits);
         for (i = 1; i < eunits; i++)
            v[i] = (int)(((unsigned)v[i - 1] + gen_huff(w, &r9_book_sfu[bits])) & mk);
         lo = -16; hi = 15;
         for (i = 0; i < eunits; i++)
         {
            if (-v[i] - sf[0][i] > lo) lo = -v[i] - sf[0][i];
            if (31 - v[i] - sf[0][i] < hi) hi = 31 - v[i] - sf[0][i];
         }
         if (lo <= hi)
         {
            size_t keep = w->pos;
            off = lo + (int)(rng() % (unsigned)(hi - lo + 1));
            w->pos = offpos;
            bw_put(w, (unsigned)(off + 16), 5);
            w->pos = keep;
            for (i = 0; i < eunits; i++)
               s[i] = v[i] + off + sf[0][i];
         }
         else
         {
            /* fall back: distance mode 1 with all-zero deltas is always valid */
            *w = save;
            bw_put(w, 1, 2);
            bw_put(w, 0, 2);                          /* sf_signed[2] */
            for (i = 0; i < eunits; i++)
            {
               bw_put(w, 0, 1);                       /* symbol 0: code 0, length 1 */
               s[i] = sf[0][i];
            }
         }
      }

      /* spec 4.8 - 4.9 */
      for (i = 0; i < units; i++)
         mask[i] = 0;
      for (i = 1; i < units; i++)
      {
         int d = s[i] - s[i - 1];
         if (d > 1)
            mask[i] += d - 1 < 5 ? d - 1 : 5;
         if (d < -1)
            mask[i - 1] += -d - 1 < 5 ? -d - 1 : 5;
      }
      for (i = 0; i < units; i++)
      {
         if (gm == 0)
            p[i] = s[i] - grad[i];
         else
         {
            p[i] = s[i] + mask[i] - grad[i];
            if (p[i] > 0)
               p[i] = gm == 1 ? p[i] / 2 : gm == 2 ? 3 * p[i] / 8 : p[i] / 4;
         }
         if (p[i] < 1)
            p[i] = 1;
         if (i < gb)
            p[i]++;
         fine[i] = 0;
         if (p[i] > 15)
         {
            fine[i] = p[i] - 15;
            p[i]    = 15;
         }
      }

      /* spec 4.10 - 4.12 */
      for (i = 0; i < coded; i++)
      {
         unsigned prec = (unsigned)p[i] + 1;
         if (prec <= r9_max_huff_prec[g->hr])
         {
            unsigned set = 0;
            const r9_book *bk;
            if (!g->hr && coded > 1 && i >= 8)
            {
               int avg = 0, a = s[i - 1];
               int b = (i + 1 < coded) ? s[i + 1] : s[coded - 1];
               int mn = a < b ? a : b;
               if (coded > 12)
               {
                  for (k = 0; k < 12; k++)
                     avg += s[k];
                  avg = (avg + 6) / 12;
               }
               if (s[i] - mn >= 3 || (s[i] - a) + (s[i] - b) >= 3)
                  set = 1;
               if (!set && i >= 12 && s[i] - mn >= 2
                     && s[i] >= avg - (r9_unit_count[i] == 16 ? 1 : 0))
                  set = 1;
            }
            bk = &r9_book_spec[set][prec][r9_unit_col[i]];
            for (k = 0; k < r9_unit_count[i]; k += bk->vps)
               gen_huff(w, bk);
         }
         else
            for (k = 0; k < r9_unit_count[i]; k++)
            {
               unsigned v = rng() & ((1u << (prec < 6 ? prec : 6)) - 1);
               bw_put(w, (unsigned)sext(v, prec < 6 ? prec : 6) & ((1u << prec) - 1), prec);
            }
      }
      for (i = 0; i < coded; i++)
         if (fine[i])
            for (k = 0; k < r9_unit_count[i]; k++)
               bw_put(w, rng() & ((1u << (fine[i] + 1)) - 1), (unsigned)fine[i] + 1);
   }
}

static void gen_lfe(gen_t *g, bw_t *w)
{
   unsigned reuse = rng() & 1, k;
   bw_put(w, g->index == 0 ? 0 : 1, 1);
   bw_put(w, reuse, 1);
   bw_put(w, rr(0, 31), 5);
   bw_put(w, rr(0, 31), 5);
   for (k = 0; k < 4; k++)
      bw_put(w, rng(), reuse ? 9 : 5);
}

/* Builds one frame into w (cleared first); returns its byte length. */
static size_t gen_frame(gen_t *g, bw_t *w)
{
   unsigned bi;
   memset(w, 0, sizeof(*w));
   for (bi = 0; bi < r9_chan_cfg[g->cc][0]; bi++)
   {
      unsigned t = r9_chan_cfg[g->cc][2 + bi];
      if (t == R9_LFE)
         gen_lfe(g, w);
      else
         gen_block(g, w, t == R9_STEREO);
      w->pos = (w->pos + 7) & ~(size_t)7;
   }
   g->index = (g->index + 1) % g->fps;
   return w->pos >> 3;
}

/* ---- 0. bit reader ---- */
static void test_bits(void)
{
   static bw_t w;
   static unsigned width[3000];
   static uint32_t val[3000];
   unsigned i, n = 0, t;
   for (t = 0; t < 50; t++)
   {
      r9_bits b;
      int ok = 1;
      size_t len;
      memset(&w, 0, sizeof(w));
      for (n = 0; w.pos < 8 * 4000 - 40 && n < 3000; n++)
      {
         width[n] = rng() % 33;
         val[n]   = width[n] ? ((rng() << 9) ^ rng()) & (uint32_t)(0xFFFFFFFFu >> (32 - width[n])) : 0;
         bw_put(&w, val[n], width[n]);
      }
      len = (w.pos + 7) >> 3;
      len -= t % 9;   /* cut the end off: the cut bits must read as zero */
      b.buf = w.buf; b.len = len; b.idx = 0; b.cache = 0; b.avail = 0;
      {
         size_t at = 0;
         for (i = 0; i < n; i++)
         {
            uint32_t want = val[i], got;
            if (at + width[i] > len * 8)
            {
               unsigned keep = at >= len * 8 ? 0 : (unsigned)(len * 8 - at);
               want = keep ? (want >> (width[i] - keep)) << (width[i] - keep) : 0;
            }
            got = r9_read(&b, width[i]);
            at += width[i];
            if (got != want || r9_pos(&b) != at)
               ok = 0;
         }
      }
      CHECK(ok, "bit reader round trip, zero past end");
   }
   printf("bit reader: done\n");
}

/* ---- 1. Huffman codebooks ---- */
static const r9_book *book_by_name(const char *nm)
{
   int s, p, c, bits;
   if (sscanf(nm, "sf_unsigned[%d]", &bits) == 1)
      return (bits >= 1 && bits <= 6) ? &r9_book_sfu[bits] : NULL;
   if (sscanf(nm, "sf_signed[%d]", &bits) == 1)
      return (bits >= 2 && bits <= 5) ? &r9_book_sfs[bits] : NULL;
   if (sscanf(nm, "spectrum[%d][%d][%d]", &s, &p, &c) == 3)
      return (s >= 0 && s < 2 && p >= 0 && p < 8 && c >= 0 && c < 4)
         ? &r9_book_spec[s][p][c] : NULL;
   return NULL;
}

static void test_codebook(const char *nm, int nsym, int vps, int vb, int mx,
      const int *len, const unsigned *code)
{
   const r9_book *bk = book_by_name(nm);
   int i, j, t;
   char msg[160];

   sprintf(msg, "%s descriptor", nm);
   CHECK(bk && bk->bits == mx && bk->vps == vps && bk->vb == vb, msg);
   if (!bk)
      return;

   /* prefix free */
   for (i = 0; i < nsym; i++)
   {
      if (!len[i])
         continue;
      CHECK(len[i] <= mx && (code[i] >> len[i]) == 0, "code fits its length");
      for (j = 0; j < nsym; j++)
      {
         int l;
         if (j == i || !len[j] || len[j] < len[i])
            continue;
         l = len[i];
         if ((code[j] >> (len[j] - l)) == code[i])
         {
            sprintf(msg, "%s: code of %d is a prefix of %d", nm, i, j);
            CHECK(0, msg);
         }
      }
   }

   /* spectrum books: the precomputed values are q * step[prec - 1] */
   {
      int ss, pp, cc2;
      if (sscanf(nm, "spectrum[%d][%d][%d]", &ss, &pp, &cc2) == 3)
         for (i = 0; i < nsym; i++)
            for (j = 0; j < vps; j++)
            {
               int v = (i >> (j * vb)) & ((1 << vb) - 1);
               if (v >= 1 << (vb - 1))
                  v -= 1 << vb;
               if (r9_spec_vals[bk->vals + i * 4 + j] != (double)v * r9_step[pp - 1])
               {
                  sprintf(msg, "%s: value table of symbol %d", nm, i);
                  CHECK(0, msg);
               }
            }
   }

   /* every symbol decodes back, whatever follows it */
   for (i = 0; i < nsym; i++)
   {
      if (!len[i])
         continue;
      for (t = 0; t < 3; t++)
      {
         bw_t w;
         r9_bits b;
         unsigned sym;
         memset(&w, 0, sizeof(w));
         bw_put(&w, code[i], (unsigned)len[i]);
         bw_put(&w, t == 0 ? 0 : t == 1 ? 0xFFFFFFFFu : rng(), 32);
         b.buf = w.buf; b.len = (w.pos + 7) >> 3; b.idx = 0; b.cache = 0; b.avail = 0;
         sym = r9_huff(&b, bk);
         if (sym != (unsigned)i || r9_pos(&b) != (size_t)len[i])
         {
            sprintf(msg, "%s: symbol %d decoded as %u, %u bits", nm, i, sym,
                  (unsigned)r9_pos(&b));
            CHECK(0, msg);
         }
      }
   }
}

static void test_codebooks(const char *path)
{
   FILE *f = fopen(path, "r");
   char line[256];
   int nbooks = 0, nnone = 0;
   static int len[4096];
   static unsigned code[4096];

   if (!f)
   {
      printf("FAIL: cannot open %s\n", path);
      failures++;
      return;
   }
   while (fgets(line, sizeof(line), f))
   {
      char nm[64];
      int nsym, vps, vb, mx, i;
      if (strncmp(line, "codebook ", 9))
         continue;
      if (strstr(line, " none"))
      {
         sscanf(line, "codebook %63s", nm);
         CHECK(book_by_name(nm) && book_by_name(nm)->bits == 0, "absent book has no table");
         nnone++;
         continue;
      }
      if (sscanf(line, "codebook %63s symbols %d values_per_symbol %d value_bits %d "
               "max_code_bits %d", nm, &nsym, &vps, &vb, &mx) != 5 || nsym > 4096)
      {
         CHECK(0, "codebook header parse");
         continue;
      }
      for (i = 0; i < nsym; i++)
      {
         int s;
         if (!fgets(line, sizeof(line), f)
               || sscanf(line, " symbol %d length %d code 0x%x", &s, &len[i], &code[i]) != 3
               || s != i)
         {
            CHECK(0, "codebook symbol parse");
            break;
         }
      }
      test_codebook(nm, nsym, vps, vb, mx, len, code);
      nbooks++;
   }
   fclose(f);
   printf("codebooks: %d checked (%d absent)\n", nbooks, nnone);
   CHECK(nbooks == 52 && nnone == 6, "codebook count");
}

/* ---- 2. DCT-IV ---- */
static void test_dct(void)
{
   unsigned lg;
   for (lg = 6; lg <= 8; lg++)
   {
      unsigned n = 1u << lg, k, i, t;
      double x[256], y[256], scratch[256], maxerr = 0.0;
      for (t = 0; t < 20; t++)
      {
         for (i = 0; i < n; i++)
            x[i] = ((double)(rng() & 0xFFFF) / 32768.0 - 1.0) * (t & 1 ? 32768.0 : 1.0);
         r9_dct4(x, y, scratch, lg);
         for (k = 0; k < n; k++)
         {
            double ref = 0.0, e;
            for (i = 0; i < n; i++)
               ref += x[i] * cos(M_PI / n * (i + 0.5) * (k + 0.5));
            e = fabs(ref - y[k]) / (t & 1 ? 32768.0 : 1.0);
            if (e > maxerr)
               maxerr = e;
         }
      }
      printf("dct4 N=%u: max error %.3g (per unit input)\n", n, maxerr);
      CHECK(maxerr < 1e-9, "dct4 matches direct formula");
   }
}

/* ---- 3. configuration ---- */
static void make_config(uint8_t c[4], unsigned sync, unsigned rate, unsigned cc,
      unsigned zero, unsigned fb, unsigned sfi, unsigned rest)
{
   uint32_t w = (sync << 24) | (rate << 20) | (cc << 17) | (zero << 16)
              | ((fb - 1) << 5) | (sfi << 3) | rest;
   c[0] = (uint8_t)(w >> 24); c[1] = (uint8_t)(w >> 16);
   c[2] = (uint8_t)(w >> 8);  c[3] = (uint8_t)w;
}

static void test_config(void)
{
   static const unsigned rates[16] = { 11025, 12000, 16000, 22050, 24000, 32000,
      44100, 48000, 44100, 48000, 64000, 88200, 96000, 128000, 176400, 192000 };
   static const unsigned fs[16] = { 64, 64, 128, 128, 128, 256, 256, 256,
      64, 64, 128, 128, 128, 256, 256, 256 };
   static const unsigned chans[6] = { 1, 2, 2, 6, 8, 4 };
   unsigned r, cc, sfi;
   uint8_t c[4];
   ratrac9_t *d;

   for (r = 0; r < 16; r++)
      for (cc = 0; cc < 8; cc++)
         for (sfi = 0; sfi < 4; sfi++)
         {
            unsigned fb = 1 + ((r * 131 + cc * 17 + sfi * 3) % 2048);
            make_config(c, 0xFE, r, cc, 0, fb, sfi, 7);
            d = ratrac9_open(c);
            if (cc > 5)
            {
               CHECK(d == NULL, "channel config 6/7 rejected");
               ratrac9_close(d);
               continue;
            }
            CHECK(d != NULL, "valid config accepted");
            if (!d)
               continue;
            CHECK(ratrac9_sample_rate(d) == rates[r], "sample rate");
            CHECK(ratrac9_frame_samples(d) == fs[r], "frame samples");
            CHECK(ratrac9_channels(d) == chans[cc], "channels");
            CHECK(ratrac9_channel_config(d) == cc, "channel config");
            CHECK(ratrac9_frame_bytes(d) == fb, "frame bytes");
            CHECK(ratrac9_frames_per_superframe(d) == (1u << sfi), "frames per superframe");
            CHECK(ratrac9_superframe_bytes(d) == (fb << sfi), "superframe bytes");
            ratrac9_close(d);
         }
   make_config(c, 0xFF, 7, 0, 0, 64, 0, 0);
   CHECK(ratrac9_open(c) == NULL, "bad sync rejected");
   make_config(c, 0x7E, 7, 0, 0, 64, 0, 0);
   CHECK(ratrac9_open(c) == NULL, "bad sync rejected");
   make_config(c, 0xFE, 7, 0, 1, 64, 0, 0);
   CHECK(ratrac9_open(c) == NULL, "must-be-zero bit rejected");
   CHECK(ratrac9_open(NULL) == NULL, "NULL config rejected");
   make_config(c, 0xFE, 7, 0, 0, 2048, 3, 0);
   d = ratrac9_open(c);
   CHECK(d && ratrac9_frame_bytes(d) == 2048 && ratrac9_superframe_bytes(d) == 16384,
         "largest frame size");
   ratrac9_close(d);
   printf("config: done\n");
}

/* ---- 4. hand-built mono frames ----
 * rate 7 (48 kHz, N = 256), mono, band 3 (10 units, 24 coefficients),
 * fixed-length scale factors of value 'sfv', gradient 0, so every unit has
 * precision sfv (sfv >= 7 forces read_signed coefficients of sfv + 1 bits). */
static size_t build_mono(uint8_t *out, unsigned sfv, const int *q)
{
   bw_t w;
   unsigned i;
   memset(&w, 0, sizeof(w));
   bw_put(&w, 0, 1);             /* first in superframe */
   bw_put(&w, 0, 1);             /* no reuse */
   bw_put(&w, 0, 4);             /* band_count 3 */
   bw_put(&w, 0, 1);             /* no extension */
   bw_put(&w, 0, 2);             /* gradient mode 0 */
   bw_put(&w, 0, 6);             /* start unit 0 */
   bw_put(&w, 0, 6);             /* end unit 1 */
   bw_put(&w, 0, 5);             /* start value 0 */
   bw_put(&w, 0, 5);             /* end value 0 */
   bw_put(&w, 0, 4);             /* boundary 0 */
   bw_put(&w, 0, 1);             /* no extension data */
   bw_put(&w, 1, 2);             /* sf mode 1: fixed length */
   bw_put(&w, 0, 2);             /* 2 bits */
   bw_put(&w, sfv, 5);           /* base */
   for (i = 0; i < 10; i++)
      bw_put(&w, 0, 2);
   for (i = 0; i < 24; i++)
      bw_put(&w, (uint32_t)q[i] & ((1u << (sfv + 1)) - 1), sfv + 1);
   memcpy(out, w.buf, (w.pos + 7) >> 3);
   return (w.pos + 7) >> 3;
}

/* Straight from the spec: dequantise, scale, direct DCT-IV, window, overlap. */
static void ref_mono(const int *q, unsigned sfv, double *prev, double *out)
{
   unsigned n = 256, h = 128, i, k;
   double x[256], y[256], wf[256], w[256];
   for (i = 0; i < n; i++)
      x[i] = 0.0;
   for (i = 0; i < 24; i++)
      x[i] = q[i] * r9_step[sfv] * r9_spectrum_scale[sfv];
   for (k = 0; k < n; k++)
   {
      double s = 0.0;
      for (i = 0; i < n; i++)
         s += x[i] * cos(M_PI / n * (i + 0.5) * (k + 0.5));
      y[k] = s;
   }
   for (i = 0; i < n; i++)
      wf[i] = (sin(((i + 0.5) / n - 0.5) * M_PI) + 1) * 0.5;
   for (i = 0; i < n; i++)
      w[i] = wf[i] / (wf[n - 1 - i] * wf[n - 1 - i] + wf[i] * wf[i]);
   for (i = 0; i < h; i++)
   {
      out[i]     = w[i] * y[i + h] + prev[i];
      out[i + h] = w[i + h] * -y[n - 1 - i] - prev[i + h];
      prev[i]     = w[n - 1 - i] * -y[h - 1 - i];
      prev[i + h] = w[h - 1 - i] * y[i];
   }
}

static void test_frames(void)
{
   uint8_t cfg[4], frame[512];
   int q[40];
   int16_t s16[256];
   int32_t s32[256];
   float f32[256];
   double prev[256], ref[256];
   size_t used, flen;
   unsigned i, f;
   int rc, ok;
   ratrac9_t *d;

   make_config(cfg, 0xFE, 7, 0, 0, 64, 0, 0);
   d = ratrac9_open(cfg);
   CHECK(d != NULL, "open mono 48k");
   if (!d)
      return;

   /* silence */
   memset(q, 0, sizeof(q));
   memset(frame, 0, sizeof(frame));
   flen = build_mono(frame, 10, q);
   CHECK(flen == 42, "silent frame is 329 bits = 42 bytes");
   for (f = 0; f < 3; f++)
   {
      used = 0;
      memset(s16, 0x55, sizeof(s16));
      rc = ratrac9_decode_s16(d, frame, 42, s16, false, &used);
      ok = 1;
      for (i = 0; i < 256; i++)
         ok &= s16[i] == 0;
      CHECK(rc == 0 && used == 42 && ok, "silent frame decodes to silence");
   }
   rc = ratrac9_decode_f32(d, frame, 42, f32, true, &used);
   ok = 1;
   for (i = 0; i < 256; i++)
      ok &= f32[i] == 0.0f;
   CHECK(rc == 0 && used == 42 && ok, "silent frame f32");
   rc = ratrac9_decode_s32(d, frame, 42, s32, false, &used);
   ok = 1;
   for (i = 0; i < 256; i++)
      ok &= s32[i] == 0;
   CHECK(rc == 0 && used == 42 && ok, "silent frame s32");

   /* the silent frame's tail is zero bits, so any cut from byte 9 on is identical */
   for (flen = 0; flen <= 42; flen++)
   {
      uint8_t *cut = (uint8_t*)malloc(flen ? flen : 1);
      memcpy(cut, frame, flen);
      ratrac9_reset(d);
      used = 0;
      rc = ratrac9_decode_s16(d, flen ? cut : NULL, flen, s16, false, &used);
      if (flen >= 9)
      {
         ok = 1;
         for (i = 0; i < 256; i++)
            ok &= s16[i] == 0;
         CHECK(rc == 0 && used == 42 && ok, "truncated silent frame: same result, used 42");
      }
      free(cut);
   }

   /* non-silent: random coefficients over two frames, against the spec math */
   ratrac9_reset(d);
   memset(prev, 0, sizeof(prev));
   for (f = 0; f < 4; f++)
   {
      unsigned sfv = 7 + f * 2;
      int maxd = 0;
      double maxf = 0.0;
      for (i = 0; i < 24; i++)
      {
         int lim = 1 << sfv;
         q[i] = (int)(rng() % (unsigned)(2 * lim)) - lim;
      }
      memset(frame, 0, sizeof(frame));
      flen = build_mono(frame, sfv, q);
      ref_mono(q, sfv, prev, ref);
      {
         /* same frame decoded three ways from identical state */
         ratrac9_t *d2 = ratrac9_open(cfg);
         ratrac9_t *d3 = ratrac9_open(cfg);
         memcpy(d2, d, sizeof(*d));
         memcpy(d3, d, sizeof(*d));
         rc = ratrac9_decode_s16(d, frame, flen, s16, false, &used);
         CHECK(rc == 0 && used == flen, "random frame s16");
         rc = ratrac9_decode_s32(d2, frame, flen, s32, true, &used);
         CHECK(rc == 0 && used == flen, "random frame s32");
         rc = ratrac9_decode_f32(d3, frame, flen, f32, false, &used);
         CHECK(rc == 0 && used == flen, "random frame f32");
         ratrac9_close(d2);
         ratrac9_close(d3);
      }
      for (i = 0; i < 256; i++)
      {
         double r  = floor(ref[i] + 0.5);
         long   rl = (long)r;
         long   rs = rl > 32767 ? 32767 : rl < -32768 ? -32768 : rl;
         if (labs(rs - s16[i]) > maxd)
            maxd = (int)labs(rs - s16[i]);
         if (labs(rl - s32[i]) > maxd)
            maxd = (int)labs(rl - s32[i]);
         if (fabs((double)(float)(ref[i] / 32767.0) - f32[i]) > maxf)
            maxf = fabs((double)(float)(ref[i] / 32767.0) - f32[i]);
      }
      printf("frame %u (sf %u): max |s16/s32 - ref| = %d, max |f32 - ref| = %.3g\n",
            f, sfv, maxd, maxf);
      CHECK(maxd == 0, "integer output equals spec math");
      CHECK(maxf <= 1e-6, "float output equals spec math");
   }
   ratrac9_close(d);
}

/* ---- 4b. fine precision 17 (spec 4.9) ----
 * mono 48 kHz, 10 units, scale factors 31, gradient 0 and boundary 10:
 * precision 32, so 16-bit coarse values and 18-bit fine values. */
static void test_fine17(void)
{
   uint8_t cfg[4];
   bw_t w;
   int q[24], r[24];
   int32_t s32[256];
   double prev[256], out[256], x[256], y[256], wf[256], win[256];
   double fs17 = r9_step[15] / 262143.0, s = r9_spectrum_scale[31];
   size_t used = 0;
   unsigned i, k, n = 256, h = 128;
   int rc, maxd = 0;
   ratrac9_t *d;

   memset(&w, 0, sizeof(w));
   bw_put(&w, 0, 1); bw_put(&w, 0, 1); bw_put(&w, 0, 4); bw_put(&w, 0, 1);
   bw_put(&w, 0, 2); bw_put(&w, 0, 6); bw_put(&w, 0, 6); bw_put(&w, 0, 5); bw_put(&w, 0, 5);
   bw_put(&w, 10, 4);                    /* boundary 10 */
   bw_put(&w, 0, 1);
   bw_put(&w, 1, 2); bw_put(&w, 0, 2); bw_put(&w, 31, 5);
   for (i = 0; i < 10; i++)
      bw_put(&w, 0, 2);
   for (i = 0; i < 24; i++)
   {
      q[i] = (int)(rng() % 65536) - 32768;
      bw_put(&w, (uint32_t)q[i] & 0xFFFF, 16);
   }
   for (i = 0; i < 24; i++)
   {
      r[i] = (int)(rng() % 262144) - 131072;
      bw_put(&w, (uint32_t)r[i] & 0x3FFFF, 18);
   }
   make_config(cfg, 0xFE, 7, 0, 0, 200, 0, 0);
   d  = ratrac9_open(cfg);
   rc = ratrac9_decode_s32(d, w.buf, (w.pos + 7) >> 3, s32, false, &used);
   CHECK(rc == 0 && used == 111, "fine precision 17 decodes, 881 bits");
   CHECK(d->ch[0].fine[0] == 17, "fine precision reaches 17");

   for (i = 0; i < n; i++)
      x[i] = 0.0;
   for (i = 0; i < 24; i++)
      x[i] = (q[i] * r9_step[15] + r[i] * fs17) * s;
   for (k = 0; k < n; k++)
   {
      double acc = 0.0;
      for (i = 0; i < n; i++)
         acc += x[i] * cos(M_PI / n * (i + 0.5) * (k + 0.5));
      y[k] = acc;
   }
   for (i = 0; i < n; i++)
      wf[i] = (sin(((i + 0.5) / n - 0.5) * M_PI) + 1) * 0.5;
   for (i = 0; i < n; i++)
      win[i] = wf[i] / (wf[n - 1 - i] * wf[n - 1 - i] + wf[i] * wf[i]);
   memset(prev, 0, sizeof(prev));
   for (i = 0; i < h; i++)
   {
      out[i]     = win[i] * y[i + h] + prev[i];
      out[i + h] = win[i + h] * -y[n - 1 - i] - prev[i + h];
   }
   for (i = 0; i < n; i++)
   {
      long v = (long)floor(out[i] + 0.5);
      if (labs(v - s32[i]) > maxd)
         maxd = (int)labs(v - s32[i]);
   }
   printf("fine 17: max |s32 - ref| = %d\n", maxd);
   CHECK(maxd == 0, "fine precision 17 output equals spec math");
   ratrac9_close(d);
}

/* ---- 5. errors ---- */
static void test_errors(void)
{
   uint8_t cfg[4], frame[512];
   int q[40];
   int16_t s16[256];
   size_t used = 12345;
   ratrac9_t *d;
   int rc;

   make_config(cfg, 0xFE, 7, 0, 0, 64, 0, 0);
   d = ratrac9_open(cfg);
   memset(q, 0, sizeof(q));
   memset(frame, 0, sizeof(frame));
   build_mono(frame, 10, q);

   frame[0] |= 0x40;            /* reuse in the first frame of a superframe */
   rc = ratrac9_decode_s16(d, frame, 64, s16, false, &used);
   CHECK(rc == RATRAC9_ERR_REUSE_PARAMS && used == 12345, "reuse error, used unset");
   frame[0] &= ~0x40;

   frame[0] |= 0x3C;            /* band_count 3 + 15 = 18: fine at 48 kHz */
   rc = ratrac9_decode_s16(d, frame, 64, s16, false, &used);
   CHECK(rc != RATRAC9_ERR_BAND_PARAMS, "band 18 allowed at rate 7");
   frame[0] &= ~0x3C;
   ratrac9_close(d);

   make_config(cfg, 0xFE, 0, 0, 0, 64, 0, 0);  /* rate 0: max band 8 */
   d = ratrac9_open(cfg);
   frame[0] |= 0x3C;
   rc = ratrac9_decode_s16(d, frame, 64, s16, false, &used);
   CHECK(rc == RATRAC9_ERR_BAND_PARAMS, "band 18 rejected at rate 0");
   /* reuse after a failed parameter read */
   frame[0] = (uint8_t)((frame[0] & ~0x3C) | 0xC0);
   rc = ratrac9_decode_s16(d, frame, 64, s16, false, &used);
   CHECK(rc == RATRAC9_ERR_BAND_PARAMS, "reuse of failed band parameters rejected");
   frame[0] &= ~0xC0;
   ratrac9_close(d);

   make_config(cfg, 0xFE, 7, 0, 0, 64, 1, 0);  /* 2 frames per superframe */
   d = ratrac9_open(cfg);
   rc = ratrac9_decode_s16(d, frame, 64, s16, false, &used);
   CHECK(rc == 0, "first frame of superframe");
   rc = ratrac9_decode_s16(d, frame, 64, s16, false, &used);
   CHECK(rc == RATRAC9_ERR_SUPERFRAME, "first-in-superframe mid superframe");
   ratrac9_reset(d);
   rc = ratrac9_decode_s16(d, frame, 64, s16, false, &used);
   CHECK(rc == 0, "reset restores superframe position");
   ratrac9_close(d);
   printf("errors: done\n");
}

/* ---- 6. random input under the sanitizers ---- */
static void test_fuzz(void)
{
   unsigned it, total = 0, okc = 0;
   static int16_t s16[8 * 256];
   static float f32[8 * 256];
   for (it = 0; it < 20000; it++)
   {
      uint8_t cfg[4];
      unsigned cc   = rng() % 6, rate = rng() % 16, j;
      size_t len    = rng() % 600, used = 0;
      uint8_t *buf  = (uint8_t*)malloc(len ? len : 1);
      ratrac9_t *d;
      unsigned fr;
      make_config(cfg, 0xFE, rate, cc, 0, 256, rng() % 4, 0);
      d = ratrac9_open(cfg);
      for (fr = 0; fr < 8; fr++)
      {
         int rc;
         for (j = 0; j < len; j++)
            buf[j] = (uint8_t)rng();
         /* bias towards plausible headers so deeper paths run */
         if (len && (rng() & 1))
            buf[0] &= 0x3F;
         rc = (fr & 1) ? ratrac9_decode_f32(d, len ? buf : NULL, len, f32, (rng() & 1) != 0, &used)
                       : ratrac9_decode_s16(d, len ? buf : NULL, len, s16, (rng() & 1) != 0, &used);
         total++;
         if (rc == 0)
            okc++;
         CHECK(rc <= 0 && rc >= RATRAC9_ERR_EXT_LENGTH, "return code range");
      }
      ratrac9_close(d);
      free(buf);
   }
   printf("fuzz: %u random frames, %u decoded without error\n", total, okc);
}

/* ---- 7. generated valid frames, every config ---- */
static void test_generated(void)
{
   static bw_t w;
   static int16_t out[8 * 256];
   unsigned rate, cc, sfi, f, bad = 0, frames = 0, noise = 0;
   for (rate = 0; rate < 16; rate++)
      for (cc = 0; cc < 6; cc++)
         for (sfi = 0; sfi < 4; sfi++)
         {
            uint8_t cfg[4];
            gen_t g;
            ratrac9_t *d;
            make_config(cfg, 0xFE, rate, cc, 0, 512, sfi, 0);
            d = ratrac9_open(cfg);
            g.rate = rate; g.cc = cc; g.fps = 1u << sfi; g.index = 0; g.hr = rate > 7;
            for (f = 0; f < 40; f++)
            {
               size_t len = gen_frame(&g, &w), used = 0;
               uint8_t *buf = (uint8_t*)malloc(len);
               int rc;
               unsigned c;
               memcpy(buf, w.buf, len);
               rc = ratrac9_decode_s16(d, buf, len, out, (f & 1) != 0, &used);
               if (rc != 0 || used != len)
               {
                  if (bad < 5)
                     printf("generated frame: rate %u cc %u sfi %u frame %u: rc %d used %u len %u\n",
                           rate, cc, sfi, f, rc, (unsigned)used, (unsigned)len);
                  bad++;
               }
               for (c = 0; c < d->channels; c++)
                  noise += d->ch[c].seeded;
               frames++;
               free(buf);
            }
            ratrac9_close(d);
         }
   printf("generated: %u frames over every config, %u failed, noise seeded %u times\n",
         frames, bad, noise);
   CHECK(bad == 0, "generated valid frames decode with bytes used = length");
   CHECK(noise > 0, "band extension noise exercised");
}

/* ---- 8. savestates ---- */
static void test_state(void)
{
   static const unsigned cfgs[][3] = {   /* rate, channel config, superframe index */
      { 7, 3, 0 }, { 7, 4, 2 }, { 7, 1, 1 }, { 4, 3, 2 }, { 12, 4, 0 }, { 0, 5, 3 }, { 15, 2, 1 }
   };
   static bw_t w;
   static int16_t a16[8 * 256], b16[8 * 256];
   static float af[8 * 256], bf[8 * 256];
   unsigned t, noisy = 0, compared = 0;

   for (t = 0; t < sizeof(cfgs) / sizeof(cfgs[0]); t++)
   {
      uint8_t cfg[4], other[4];
      gen_t g;
      ratrac9_t *a, *b, *o;
      unsigned f, c, split = 5 + t * 3;
      size_t sz, used_a, used_b;
      uint8_t *st, *st2;
      int ok = 1;

      make_config(cfg, 0xFE, cfgs[t][0], cfgs[t][1], 0, 512, cfgs[t][2], 0);
      a = ratrac9_open(cfg);
      b = ratrac9_open(cfg);
      g.rate = cfgs[t][0]; g.cc = cfgs[t][1]; g.fps = 1u << cfgs[t][2];
      g.index = 0; g.hr = cfgs[t][0] > 7;

      for (f = 0; f < split; f++)
      {
         size_t len = gen_frame(&g, &w);
         ratrac9_decode_s16(a, w.buf, len, a16, false, &used_a);
      }
      for (c = 0; c < a->channels; c++)
         if (a->ch[c].seeded)
            noisy++;

      sz  = ratrac9_state_size(a);
      st  = (uint8_t*)malloc(sz + 1);
      st2 = (uint8_t*)malloc(sz + 1);
      ratrac9_save_state(a, st);
      CHECK(!ratrac9_load_state(b, st, sz - 1), "load refuses a short state");
      CHECK(!ratrac9_load_state(b, st, sz + 1), "load refuses a long state");
      CHECK(ratrac9_load_state(b, st, sz), "load accepts its own state");
      ratrac9_save_state(b, st2);
      CHECK(!memcmp(st, st2, sz), "save after load gives the same bytes");

      /* a broken field is refused and leaves the decoder as it was */
      st2[16 + 4] = 31;                     /* block 0 units */
      CHECK(!ratrac9_load_state(b, st2, sz), "load refuses an out-of-range field");
      st2[16 + 4] = st[16 + 4];
      st2[sz - 1] = 0xFF; st2[sz - 2] = 0xF0; /* last overlap value: NaN */
      CHECK(!ratrac9_load_state(b, st2, sz), "load refuses a non-finite overlap value");
      {
         /* each out-of-range field on its own: offset, byte value */
         size_t c0 = 16 + (size_t)a->nblocks * 66;
         size_t muts[8][2];
         unsigned mi, nm = 0;
         muts[nm][0] = 12;      muts[nm++][1] = (size_t)(1u << cfgs[t][2]);  /* superframe position */
         muts[nm][0] = c0;      muts[nm++][1] = 64;                          /* sf_prev[0] */
         muts[nm][0] = c0 + 39; muts[nm++][1] = 2;                           /* seeded */
         muts[nm][0] = c0 + 40; muts[nm++][1] = 5;                           /* extension mode */
         muts[nm][0] = c0 + 41; muts[nm++][1] = 64;                          /* extension value */
         for (mi = 0; mi < nm; mi++)
         {
            memcpy(st2, st, sz);
            st2[muts[mi][0]] = (uint8_t)muts[mi][1];
            CHECK(!ratrac9_load_state(b, st2, sz), "load refuses an out-of-range field");
         }
         if (a->blk_type[0] == R9_STEREO)
         {
            /* valid parameters with stereo units above units, then
             * extension enabled with too few units */
            memcpy(st2, st, sz);
            st2[16 + 4]  = 10; st2[16 + 12] = 12; st2[16 + 24] = 0; st2[16 + 32] = 0;
            CHECK(!ratrac9_load_state(b, st2, sz), "load refuses stereo units above units");
            st2[16 + 12] = 4;  st2[16 + 24] = 1;
            CHECK(!ratrac9_load_state(b, st2, sz), "load refuses extension with 10 units");
         }
         memcpy(st2, st, sz);
      }
      ratrac9_save_state(b, st2);
      CHECK(!memcmp(st, st2, sz), "refused loads leave the decoder unchanged");

      make_config(other, 0xFE, cfgs[t][0], cfgs[t][1], 0, 256, cfgs[t][2], 0);
      o = ratrac9_open(other);
      CHECK(ratrac9_state_size(o) == sz && !ratrac9_load_state(o, st, sz),
            "load refuses a state from another config");
      ratrac9_close(o);

      for (f = 0; f < 40; f++)
      {
         size_t len = gen_frame(&g, &w);
         unsigned n = 256 * a->channels;
         int ra, rb;
         if (f & 1)
         {
            ra = ratrac9_decode_f32(a, w.buf, len, af, true, &used_a);
            rb = ratrac9_decode_f32(b, w.buf, len, bf, true, &used_b);
            ok &= !memcmp(af, bf, n * sizeof(float));
         }
         else
         {
            ra = ratrac9_decode_s16(a, w.buf, len, a16, false, &used_a);
            rb = ratrac9_decode_s16(b, w.buf, len, b16, false, &used_b);
            ok &= !memcmp(a16, b16, n * sizeof(int16_t));
         }
         ok &= ra == 0 && rb == 0 && used_a == used_b;
         compared++;
      }
      CHECK(ok, "decoder restored from a state continues bit-identically");
      free(st);
      free(st2);
      ratrac9_close(a);
      ratrac9_close(b);
   }
   printf("state: %u frames compared after restore, %u channels had noise seeded at save\n",
         compared, noisy);
   CHECK(noisy > 0, "state test covers the noise generator");
}

/* ---- 9. WAVE 'fmt ' ---- */
static void test_wave_fmt(void)
{
   static const uint8_t guid[16] = { 0xD2, 0x42, 0xE1, 0x47, 0xBA, 0x36, 0x8D, 0x4D,
      0x88, 0xFC, 0x61, 0x65, 0x4F, 0x8C, 0x83, 0x6C };
   uint8_t fmt[60], cfg[4] = { 0, 0, 0, 0 };
   unsigned i;
   memset(fmt, 0, sizeof(fmt));
   fmt[0] = 0xFE; fmt[1] = 0xFF;          /* WAVE_FORMAT_EXTENSIBLE */
   fmt[2] = 2;                            /* channels */
   fmt[16] = 34;                          /* cbSize */
   memcpy(fmt + 24, guid, 16);
   fmt[40] = 1;                           /* version */
   fmt[44] = 0xFE; fmt[45] = 0x72; fmt[46] = 0x7F; fmt[47] = 0xE0;
   CHECK(ratrac9_parse_wave_fmt(fmt, 52, cfg) && cfg[0] == 0xFE && cfg[1] == 0x72
         && cfg[2] == 0x7F && cfg[3] == 0xE0, "fmt chunk accepted, config copied");
   CHECK(!ratrac9_parse_wave_fmt(fmt, 51, cfg), "fmt chunk: short length rejected");
   CHECK(!ratrac9_parse_wave_fmt(fmt, 53, cfg), "fmt chunk: long length rejected");
   CHECK(!ratrac9_parse_wave_fmt(NULL, 52, cfg), "fmt chunk: NULL rejected");
   fmt[0] = 0x01; fmt[1] = 0x00;
   CHECK(!ratrac9_parse_wave_fmt(fmt, 52, cfg), "fmt chunk: wrong tag rejected");
   fmt[0] = 0xFE; fmt[1] = 0xFF;
   for (i = 24; i < 40; i++)
   {
      fmt[i] ^= 0x01;
      CHECK(!ratrac9_parse_wave_fmt(fmt, 52, cfg), "fmt chunk: wrong GUID rejected");
      fmt[i] ^= 0x01;
   }
   printf("wave fmt: done\n");
}

int main(int argc, char **argv)
{
   test_bits();
   test_codebooks(argc > 1 ? argv[1] : "/tmp/at9clean/spec/tables.txt");
   test_dct();
   test_config();
   test_frames();
   test_fine17();
   test_errors();
   test_fuzz();
   test_generated();
   test_state();
   test_wave_fmt();
   if (failures)
   {
      printf("%d FAILURES\n", failures);
      return 1;
   }
   printf("all tests passed\n");
   return 0;
}

/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rac3_encode.c).
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* A basic AC-3 encoder from ATSC A/52:2018 section 8 and the syntax
 * of section 5: the forward transform of 8.2.3 (long blocks only),
 * exponents by 8.2.7 with D15 coding in block 0 and reuse after,
 * the core bit allocation of 7.2 run forward with 8.2.12's nominal
 * parameters and the SNR offset searched to fill the frame, the
 * quantisers of 7.3 with 7.3.5's grouping, and the frame packed as
 * Tables 5.1 to 5.5 with both CRCs of 7.10.1. No coupling, no
 * rematrixing, no block switching: everything an encoder may leave
 * out, left out. */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <formats/rac3.h>
#include "rac3_tables.h"

#define RAC3_PI 3.14159265358979323846
#define ENC_NCH_MAX 5

/* ---- bit writer ---------------------------------------------------- */

typedef struct
{
   uint8_t *p;
   size_t   cap;    /* bytes; writes past it are counted, not stored */
   size_t   bit;
} rac3_bw_t;

static void bw_put(rac3_bw_t *w, unsigned v, unsigned n)
{
   /* n bits of v, MSB first, into the byte stream; past cap the bits
    * are counted and not stored. Bytes are cleared as they are
    * entered, so a buffer need not be zeroed first. */
   while (n)
   {
      size_t   byte = w->bit >> 3;
      unsigned off  = (unsigned)(w->bit & 7);
      unsigned room = 8 - off;
      unsigned take = n < room ? n : room;
      unsigned bits = (v >> (n - take)) & ((1u << take) - 1u);
      if (byte < w->cap)
      {
         if (off == 0) w->p[byte] = 0;
         w->p[byte] |= (uint8_t)(bits << (room - take));
      }
      w->bit += take;
      n      -= take;
   }
}

/* ---- the encoder --------------------------------------------------- */

struct rac3_encoder
{
   unsigned fscod, rate, kbps, frmsizecod_even;
   unsigned acmod, lfeon, nfchans, nchans;
   uint32_t layout;
   unsigned slot[ENC_NCH_MAX];     /* stream channel -> input slot */
   unsigned lfe_slot;
   unsigned words_even, words_odd; /* 44.1 kHz: the two sizes */
   long     size_acc;              /* which of the two this frame takes */
   /* the previous frame's last 256 samples per stream channel (+LFE) */
   float    prev[ENC_NCH_MAX + 1][256];
   /* per block per channel: coefficients, exponents, mantissas */
   float    coef[6][ENC_NCH_MAX + 1][256];
   uint8_t  exps[ENC_NCH_MAX + 1][256];   /* the frame's shared exponents */
   uint8_t  bap[ENC_NCH_MAX + 1][256];
   unsigned endmant[ENC_NCH_MAX];
   float    window512[512];
   float    x[512];            /* the windowed block, here rather than on a small stack */
   /* the block's mantissas, queued then written (see write_mantissas) */
   uint8_t  mq_bap[ENC_NCH_MAX * 256 + 7];
   uint16_t mq_code[ENC_NCH_MAX * 256 + 7];
   unsigned mq_n;
   rac3_bw_t *w;
   /* allocation scratch */
   int      psd[256], bndpsd[50], excite[50], mask[50];
};

static const uint8_t enc_nfchans[8] = { 2, 1, 2, 3, 3, 4, 4, 5 };

/* The acmod for a layout and the slot of each stream channel in the
 * input's mask order (FL FR FC LFE BC SL SR ascending). */
typedef struct
{
   unsigned acmod, lfeon, nfchans, nchans, lfe_slot;
   unsigned slot[ENC_NCH_MAX];
   uint32_t layout;
} enc_layout_t;

static bool enc_layout(enc_layout_t *e, uint32_t layout)
{
   uint32_t fbw = layout & ~0x008u;
   unsigned lfe = (layout & 0x008u) ? 1 : 0;
   unsigned i, n = 0, pos[16], slot_of_bit[16];
   /* slots in ascending bit order */
   for (i = 0; i < 16; i++) slot_of_bit[i] = 0;
   for (i = 0; i < 16; i++)
      if (layout & (1u << i)) slot_of_bit[i] = n++;
   (void)pos;
   switch (fbw)
   {
      case 0x004u: e->acmod = 1; e->slot[0] = slot_of_bit[2]; break;                              /* C */
      case 0x003u: e->acmod = 2; e->slot[0] = slot_of_bit[0]; e->slot[1] = slot_of_bit[1]; break; /* L R */
      case 0x007u: e->acmod = 3; e->slot[0] = slot_of_bit[0]; e->slot[1] = slot_of_bit[2]; e->slot[2] = slot_of_bit[1]; break;
      case 0x103u: e->acmod = 4; e->slot[0] = slot_of_bit[0]; e->slot[1] = slot_of_bit[1]; e->slot[2] = slot_of_bit[8]; break;
      case 0x107u: e->acmod = 5; e->slot[0] = slot_of_bit[0]; e->slot[1] = slot_of_bit[2]; e->slot[2] = slot_of_bit[1]; e->slot[3] = slot_of_bit[8]; break;
      case 0x603u: e->acmod = 6; e->slot[0] = slot_of_bit[0]; e->slot[1] = slot_of_bit[1]; e->slot[2] = slot_of_bit[9]; e->slot[3] = slot_of_bit[10]; break;
      case 0x607u: e->acmod = 7; e->slot[0] = slot_of_bit[0]; e->slot[1] = slot_of_bit[2]; e->slot[2] = slot_of_bit[1]; e->slot[3] = slot_of_bit[9]; e->slot[4] = slot_of_bit[10]; break;
      default: return false;
   }
   e->lfeon    = lfe;
   e->lfe_slot = lfe ? slot_of_bit[3] : 0;
   e->nfchans  = enc_nfchans[e->acmod];
   e->nchans   = n;
   e->layout   = layout;
   return true;
}

int rac3_layout_acmod(uint32_t layout)
{
   enc_layout_t l;
   if (!enc_layout(&l, layout))
      return -1;
   return (int)l.acmod;
}

rac3_encoder_t *rac3_encoder_new(unsigned rate, uint32_t layout, unsigned kbps)
{
   static const uint16_t rates[19] = { 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 448, 512, 576, 640 };
   rac3_encoder_t *e;
   unsigned i, code = 19;
   for (i = 0; i < 19; i++) if (rates[i] == kbps) code = i;
   if (code == 19) return NULL;
   if (rate != 48000 && rate != 44100 && rate != 32000) return NULL;
   e = (rac3_encoder_t*)calloc(1, sizeof(*e));
   if (!e) return NULL;
   {
      enc_layout_t l;
      if (!enc_layout(&l, layout)) { free(e); return NULL; }
      e->acmod = l.acmod; e->lfeon = l.lfeon; e->nfchans = l.nfchans;
      e->nchans = l.nchans; e->lfe_slot = l.lfe_slot; e->layout = l.layout;
      memcpy(e->slot, l.slot, sizeof(e->slot));
   }
   e->rate  = rate;
   e->kbps  = kbps;
   e->fscod = rate == 48000 ? 0 : rate == 44100 ? 1 : 2;
   e->frmsizecod_even = code * 2;
   switch (e->fscod)
   {
      case 0: e->words_even = e->words_odd = kbps * 2; break;
      case 1: e->words_even = (kbps * 320u) / 147u; e->words_odd = e->words_even + 1; break;
      default: e->words_even = e->words_odd = kbps * 3; break;
   }
   for (i = 0; i < 256; i++)
   {
      e->window512[i]       = rac3_window[i];
      e->window512[511 - i] = rac3_window[i];
   }
   for (i = 0; i < ENC_NCH_MAX; i++) e->endmant[i] = 253;   /* chbwcod 60 */
   return e;
}

void rac3_encoder_free(rac3_encoder_t *e)
{
   free(e);
}

size_t rac3_encoder_frame_bytes(const rac3_encoder_t *e)
{
   return e ? (size_t)e->words_odd * 2 : 0;
}

/* ---- 8.2.3: the forward transform ------------------------------------ */

/* XD[k] = -2/N sum x[n] cos(2 pi/(4N) (2n+1)(2k+1) + pi/4 (2k+1)), the
 * long transform (alpha = 0), on a 512-sample windowed block. A
 * direct sum against a full table of the cosines, 256 rows of 512,
 * built once: a plain dot product per output, which the compiler
 * vectorises, at 512 KiB of memory the process pays once. */
static float mdct_cos[256][512];
static bool  mdct_cos_have = false;

static void mdct512(const float *x, float *X)
{
   unsigned n, k;
   if (!mdct_cos_have)
   {
      for (k = 0; k < 256; k++)
         for (n = 0; n < 512; n++)
            mdct_cos[k][n] = (float)(-2.0 / 512.0 * cos(2.0 * RAC3_PI / 2048.0 * (double)((2 * n + 1) * (2 * k + 1))
                  + RAC3_PI / 4.0 * (double)(2 * k + 1)));
      mdct_cos_have = true;
   }
   for (k = 0; k < 256; k++)
   {
      const float *c = mdct_cos[k];
      /* four partial sums: the compiler may not reorder a float sum,
       * so the independence is written out for it */
      float a0 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
      for (n = 0; n < 512; n += 4)
      {
         a0 += x[n]     * c[n];
         a1 += x[n + 1] * c[n + 1];
         a2 += x[n + 2] * c[n + 2];
         a3 += x[n + 3] * c[n + 3];
      }
      X[k] = (a0 + a1) + (a2 + a3);
   }
}

/* ---- 8.2.7 / 8.2.10: exponents ----------------------------------------- */

static unsigned exponent_of(float c)
{
   /* leading zeros of |c| as a fraction: the e with |c| 2^e in [0.5, 1) */
   int e;
   float a = (float)fabs((double)c);
   if (a < 1e-9f) return 24;
   e = 0;
   while (a < 0.5f && e < 24) { a *= 2.0f; e++; }
   return (unsigned)e;
}

/* The frame's shared exponent per bin is the smallest over the six
 * blocks (the largest coefficient decides), then made D15-legal:
 * exps[0] within 4 bits and every step within +/-2, by lowering
 * exponents (a lower exponent only costs mantissa precision, never
 * legality). */
static void frame_exponents(rac3_encoder_t *e, unsigned ch, unsigned nmant)
{
   unsigned blk, i;
   uint8_t *ex = e->exps[ch];
   for (i = 0; i < nmant; i++)
   {
      unsigned m = 24;
      for (blk = 0; blk < 6; blk++)
      {
         unsigned v = exponent_of(e->coef[blk][ch][i]);
         if (v < m) m = v;
      }
      ex[i] = (uint8_t)m;
   }
   if (ex[0] > 15) ex[0] = 15;
   for (i = 1; i < nmant; i++)
      if (ex[i] > ex[i - 1] + 2) ex[i] = (uint8_t)(ex[i - 1] + 2);
   for (i = nmant - 1; i > 0; i--)
      if (ex[i - 1] > ex[i] + 2) ex[i - 1] = (uint8_t)(ex[i] + 2);
   if (ex[0] > 15) ex[0] = 15;
}

/* ---- 7.2.2 forward: the allocation --------------------------------- */

static int enc_logadd(int a, int b)
{
   int c = a - b;
   int address = (c < 0 ? -c : c) >> 1;
   if (address > 255) address = 255;
   return (c >= 0 ? a : b) + (int)rac3_latab[address];
}

static int enc_calc_lowcomp(int a, int b0, int b1, int bin)
{
   if (bin < 7)
   {
      if ((b0 + 256) == b1) a = 384;
      else if (b0 > b1) a = a - 64 > 0 ? a - 64 : 0;
   }
   else if (bin < 20)
   {
      if ((b0 + 256) == b1) a = 320;
      else if (b0 > b1) a = a - 64 > 0 ? a - 64 : 0;
   }
   else
      a = a - 128 > 0 ? a - 128 : 0;
   return a;
}

/* 8.2.12's nominal parameters, fixed. */
#define ENC_SDCYCOD 2
#define ENC_FDCYCOD 1
#define ENC_SGAINCOD 1
#define ENC_DBPBCOD 2
#define ENC_FLOORCOD 4
#define ENC_FGAINCOD 4

static void allocate(rac3_encoder_t *e, const uint8_t *exps, unsigned start, unsigned end,
      unsigned csnroffst, unsigned fsnroffst, uint8_t *bap)
{
   int *psd = e->psd, *bndpsd = e->bndpsd, *excite = e->excite, *mask = e->mask;
   int sdecay = rac3_slowdec[ENC_SDCYCOD], fdecay = rac3_fastdec[ENC_FDCYCOD];
   int sgain = rac3_slowgain[ENC_SGAINCOD], dbknee = rac3_dbpbtab[ENC_DBPBCOD];
   int floor_ = rac3_floortab[ENC_FLOORCOD], fgain = rac3_fastgain[ENC_FGAINCOD];
   int snroffset = ((((int)csnroffst - 15) * 16) + (int)fsnroffst) * 4;
   int fastleak = 0, slowleak = 0, lowcomp = 0;
   unsigned bin, j, kk, bndstrt, bndend, begin, lastbin;

   if (csnroffst == 0 && fsnroffst == 0)
   {
      memset(bap + start, 0, end - start);
      return;
   }
   for (bin = start; bin < end; bin++)
      psd[bin] = 3072 - ((int)exps[bin] << 7);
   j = start; kk = rac3_masktab[start];
   do
   {
      unsigned i;
      lastbin = rac3_bndtab[kk] + rac3_bndsz[kk];
      if (lastbin > end) lastbin = end;
      bndpsd[kk] = psd[j]; j++;
      for (i = j; i < lastbin; i++) { bndpsd[kk] = enc_logadd(bndpsd[kk], psd[j]); j++; }
      kk++;
   } while (end > lastbin);
   bndstrt = rac3_masktab[start];
   bndend  = rac3_masktab[end - 1] + 1;
   {
      lowcomp = enc_calc_lowcomp(lowcomp, bndpsd[0], bndpsd[1], 0);
      excite[0] = bndpsd[0] - fgain - lowcomp;
      lowcomp = enc_calc_lowcomp(lowcomp, bndpsd[1], bndpsd[2], 1);
      excite[1] = bndpsd[1] - fgain - lowcomp;
      begin = 7;
      for (bin = 2; bin < 7; bin++)
      {
         if (!(bndend == 7 && bin == 6))
            lowcomp = enc_calc_lowcomp(lowcomp, bndpsd[bin], bndpsd[bin + 1], (int)bin);
         fastleak = bndpsd[bin] - fgain;
         slowleak = bndpsd[bin] - sgain;
         excite[bin] = fastleak - lowcomp;
         if (!(bndend == 7 && bin == 6))
            if (bndpsd[bin] <= bndpsd[bin + 1]) { begin = bin + 1; break; }
      }
      for (bin = begin; bin < (bndend < 22 ? bndend : 22); bin++)
      {
         if (!(bndend == 7 && bin == 6))
            lowcomp = enc_calc_lowcomp(lowcomp, bndpsd[bin], bndpsd[bin + 1], (int)bin);
         fastleak -= fdecay; if (fastleak < bndpsd[bin] - fgain) fastleak = bndpsd[bin] - fgain;
         slowleak -= sdecay; if (slowleak < bndpsd[bin] - sgain) slowleak = bndpsd[bin] - sgain;
         excite[bin] = (fastleak - lowcomp > slowleak) ? fastleak - lowcomp : slowleak;
      }
      begin = 22;
   }
   for (bin = begin; bin < bndend; bin++)
   {
      fastleak -= fdecay; if (fastleak < bndpsd[bin] - fgain) fastleak = bndpsd[bin] - fgain;
      slowleak -= sdecay; if (slowleak < bndpsd[bin] - sgain) slowleak = bndpsd[bin] - sgain;
      excite[bin] = fastleak > slowleak ? fastleak : slowleak;
   }
   for (bin = bndstrt; bin < bndend; bin++)
   {
      int m;
      if (bndpsd[bin] < dbknee) excite[bin] += (dbknee - bndpsd[bin]) >> 2;
      m = (int)rac3_hth[e->fscod][bin];
      mask[bin] = excite[bin] > m ? excite[bin] : m;
   }
   {
      unsigned i = start;
      j = rac3_masktab[start];
      do
      {
         unsigned c;
         lastbin = rac3_bndtab[j] + rac3_bndsz[j];
         if (lastbin > end) lastbin = end;
         mask[j] -= snroffset;
         mask[j] -= floor_;
         if (mask[j] < 0) mask[j] = 0;
         mask[j] &= 0x1fe0;
         mask[j] += floor_;
         for (c = i; c < lastbin; c++)
         {
            int address = (psd[i] - mask[j]) >> 5;
            if (address < 0) address = 0;
            if (address > 63) address = 63;
            bap[i] = rac3_baptab[address];
            i++;
         }
         j++;
      } while (end > lastbin);
   }
}

/* ---- 7.3 forward: quantisation and packing --------------------------- */

static unsigned sym_code(float m, unsigned levels)
{
   /* inverse of (2c + 1 - L) / L: c = (m L + L - 1) / 2, rounded */
   float c = (m * (float)levels + (float)levels - 1.0f) * 0.5f;
   int q = (int)floor((double)c + 0.5);
   if (q < 0) q = 0;
   if (q >= (int)levels) q = (int)levels - 1;
   return (unsigned)q;
}

/* A block's mantissas are collected first, then written: a grouped
 * codeword (bap 1, 2 and 4, 7.3.5) sits in the stream where the
 * FIRST mantissa of its group is, so the later members must be
 * known before it can be written. The groups run across channels
 * within the block and the block's last, partial ones are padded
 * with zero codes. */
static unsigned mantissa_code(unsigned bap, float m)
{
   switch (bap)
   {
      case 1: return sym_code(m, 3);
      case 2: return sym_code(m, 5);
      case 3: return sym_code(m, 7);
      case 4: return sym_code(m, 11);
      case 5: return sym_code(m, 15);
      default:
      {
         unsigned bits = rac3_bap_bits[bap];
         int lim = 1 << (bits - 1);
         int q = (int)floor((double)m * (double)lim + 0.5);
         if (q >= lim) q = lim - 1;
         if (q < -lim) q = -lim;
         return (unsigned)q & ((1u << bits) - 1);
      }
   }
}

static void queue_mantissa(rac3_encoder_t *e, unsigned bap, float m)
{
   if (!bap) return;
   e->mq_bap[e->mq_n]  = (uint8_t)bap;
   e->mq_code[e->mq_n] = (uint16_t)mantissa_code(bap, m);
   e->mq_n++;
}

static void write_mantissas(rac3_encoder_t *e)
{
   rac3_bw_t *w = e->w;
   unsigned i, left1 = 0, left2 = 0, left4 = 0;
   for (i = 0; i < e->mq_n; i++)
   {
      unsigned bap = e->mq_bap[i];
      switch (bap)
      {
         case 1: case 2: case 4:
         {
            unsigned *left = bap == 1 ? &left1 : bap == 2 ? &left2 : &left4;
            if (*left) { (*left)--; break; }
            {
               unsigned n = bap == 4 ? 2 : 3, radix = bap == 1 ? 3 : bap == 2 ? 5 : 11;
               unsigned bits = bap == 1 ? 5 : 7;
               unsigned got = 0, v = 0, j;
               for (j = i; j < e->mq_n && got < n; j++)
                  if (e->mq_bap[j] == bap) { v = v * radix + e->mq_code[j]; got++; }
               while (got < n) { v = v * radix; got++; }
               bw_put(w, v, bits);
               *left = n - 1;
            }
            break;
         }
         default:
            bw_put(w, e->mq_code[i], bap == 3 ? 3 : bap == 5 ? 4 : rac3_bap_bits[bap]);
            break;
      }
   }
   e->mq_n = 0;
}

/* D15 exponents: exps[0] absolute (4 bits), then groups of three
 * differentials mapped +2 into a 7-bit value 25 a + 5 b + c. */
static void put_exponents(rac3_bw_t *w, const uint8_t *exps, unsigned nmant, bool absexp_4bit)
{
   unsigned ngrps = (nmant - 1) / 3, g, prev;
   if (absexp_4bit) bw_put(w, exps[0], 4);
   prev = exps[0];
   for (g = 0; g < ngrps; g++)
   {
      unsigned v = 0, i;
      for (i = 0; i < 3; i++)
      {
         unsigned idx = 1 + g * 3 + i;
         unsigned cur = idx < nmant ? exps[idx] : prev;
         int d = (int)cur - (int)prev + 2;
         if (d < 0) d = 0;
         if (d > 4) d = 4;
         v = v * 5 + (unsigned)d;
         prev = cur;
      }
      bw_put(w, v, 7);
   }
}

/* ---- the frame ------------------------------------------------------- */

/* Packs the frame into w with the given SNR offset; the mantissas are
 * quantised on the way. Returns the bits used. */
static size_t pack_frame(rac3_encoder_t *e, rac3_bw_t *w, unsigned csnroffst, unsigned fsnroffst, unsigned frmsizecod)
{
   unsigned blk, ch, i;
   e->w = w;
   w->bit = 0;
   /* syncinfo: crc1 filled in later */
   bw_put(w, 0x0B77, 16);
   bw_put(w, 0, 16);
   bw_put(w, e->fscod, 2);
   bw_put(w, frmsizecod, 6);
   /* bsi */
   bw_put(w, 8, 5);              /* bsid */
   bw_put(w, 0, 3);              /* bsmod: complete main */
   bw_put(w, e->acmod, 3);
   if ((e->acmod & 1) && e->acmod != 1) bw_put(w, 0, 2);   /* cmixlev -3 dB */
   if (e->acmod & 4) bw_put(w, 0, 2);                        /* surmixlev -3 dB */
   if (e->acmod == 2) bw_put(w, 0, 2);                       /* dsurmod */
   bw_put(w, e->lfeon, 1);
   bw_put(w, 31, 5);             /* dialnorm: -31 dB, no attenuation in decoders that apply it */
   bw_put(w, 0, 1);              /* compre */
   bw_put(w, 0, 1);              /* langcode */
   bw_put(w, 0, 1);              /* audprodie */
   if (e->acmod == 0)
   {
      bw_put(w, 31, 5);
      bw_put(w, 0, 3);
   }
   bw_put(w, 0, 1);              /* copyrightb */
   bw_put(w, 1, 1);              /* origbs */
   bw_put(w, 0, 1);              /* timecod1e */
   bw_put(w, 0, 1);              /* timecod2e */
   bw_put(w, 0, 1);              /* addbsie */

   for (blk = 0; blk < 6; blk++)
   {
      for (ch = 0; ch < e->nfchans; ch++) bw_put(w, 0, 1);   /* blksw */
      for (ch = 0; ch < e->nfchans; ch++) bw_put(w, 0, 1);   /* dithflag: off */
      bw_put(w, 0, 1);                                        /* dynrnge */
      if (e->acmod == 0) bw_put(w, 0, 1);
      /* coupling: block 0 says off, later blocks reuse */
      if (blk == 0) { bw_put(w, 1, 1); bw_put(w, 0, 1); }
      else bw_put(w, 0, 1);
      if (e->acmod == 2)
      {
         if (blk == 0) { bw_put(w, 1, 1); for (i = 0; i < 4; i++) bw_put(w, 0, 1); }
         else bw_put(w, 0, 1);
      }
      /* exponent strategy: D15 in block 0, reuse after */
      for (ch = 0; ch < e->nfchans; ch++) bw_put(w, blk == 0 ? 1 : 0, 2);
      if (e->lfeon) bw_put(w, blk == 0 ? 1 : 0, 1);
      if (blk == 0)
      {
         for (ch = 0; ch < e->nfchans; ch++) bw_put(w, 60, 6);   /* chbwcod */
         for (ch = 0; ch < e->nfchans; ch++)
         {
            put_exponents(w, e->exps[ch], e->endmant[ch], true);
            bw_put(w, 0, 2);                                     /* gainrng */
         }
         if (e->lfeon)
            put_exponents(w, e->exps[ENC_NCH_MAX], 7, true);
      }
      /* bit allocation parameters and SNR offsets: block 0 only */
      if (blk == 0)
      {
         bw_put(w, 1, 1);
         bw_put(w, ENC_SDCYCOD, 2); bw_put(w, ENC_FDCYCOD, 2); bw_put(w, ENC_SGAINCOD, 2);
         bw_put(w, ENC_DBPBCOD, 2); bw_put(w, ENC_FLOORCOD, 3);
         bw_put(w, 1, 1);
         bw_put(w, csnroffst, 6);
         for (ch = 0; ch < e->nfchans; ch++) { bw_put(w, fsnroffst, 4); bw_put(w, ENC_FGAINCOD, 3); }
         if (e->lfeon) { bw_put(w, fsnroffst, 4); bw_put(w, ENC_FGAINCOD, 3); }
      }
      else
      {
         bw_put(w, 0, 1);
         bw_put(w, 0, 1);
      }
      bw_put(w, 0, 1);            /* deltbaie */
      bw_put(w, 0, 1);            /* skiple */
      /* mantissas */
      e->mq_n = 0;
      for (ch = 0; ch < e->nfchans; ch++)
         for (i = 0; i < e->endmant[ch]; i++)
            queue_mantissa(e, e->bap[ch][i], (float)ldexp((double)e->coef[blk][ch][i], (int)e->exps[ch][i]));
      if (e->lfeon)
         for (i = 0; i < 7; i++)
            queue_mantissa(e, e->bap[ENC_NCH_MAX][i], (float)ldexp((double)e->coef[blk][ENC_NCH_MAX][i], (int)e->exps[ENC_NCH_MAX][i]));
      write_mantissas(e);
   }
   /* auxdata: auxdatae = 0 is the last bit before errorcheck; the
    * caller pads the auxbits to put it and crc2 at the end */
   return w->bit;
}

/* crc1 such that the CRC over the first five eighths, crc1 field
 * included, is zero: X with CRC(X || rest) = 0. CRC(X || 0^n) is
 * linear in X, so its 16 columns come from the unit vectors and the
 * system is solved over GF(2). */
static uint16_t solve_crc1(const uint8_t *rest, size_t n)
{
   uint16_t target = rac3_crc16(rest, n);   /* CRC(0^2 || rest) = CRC(rest) */
   uint16_t col[16];
   unsigned i, r, c;
   uint16_t x = 0;
   /* columns: CRC(e_i || 0^n): run the unit vector then n zero bytes */
   for (i = 0; i < 16; i++)
   {
      uint16_t crc = (uint16_t)(1u << (15 - i));   /* the two bytes with bit i set, as the CRC state after them (init 0, no xor) */
      size_t z;
      /* CRC state after feeding the two bytes with only bit i set is
       * those bytes shifted through the polynomial: feed them */
      uint8_t two[2];
      two[0] = (uint8_t)((1u << (15 - i)) >> 8);
      two[1] = (uint8_t)(1u << (15 - i));
      crc = rac3_crc16(two, 2);
      for (z = 0; z < n; z++)
      {
         unsigned k;
         crc ^= 0;
         for (k = 0; k < 8; k++)
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x8005u) : (uint16_t)(crc << 1);
      }
      col[i] = crc;
   }
   /* solve M x = target by Gaussian elimination: rows are bit
    * positions of the CRC, unknowns the bits of X */
   {
      uint32_t rows[16];   /* each: 16 coefficient bits | rhs bit at 16 */
      for (r = 0; r < 16; r++)
      {
         rows[r] = 0;
         for (c = 0; c < 16; c++)
            if (col[c] & (1u << (15 - r))) rows[r] |= 1u << c;
         if (target & (1u << (15 - r))) rows[r] |= 1u << 16;
      }
      for (c = 0; c < 16; c++)
      {
         unsigned piv = 16;
         for (r = c; r < 16; r++) if (rows[r] & (1u << c)) { piv = r; break; }
         if (piv == 16) continue;
         if (piv != c) { uint32_t t = rows[c]; rows[c] = rows[piv]; rows[piv] = t; }
         for (r = 0; r < 16; r++)
            if (r != c && (rows[r] & (1u << c))) rows[r] ^= rows[c];
      }
      for (c = 0; c < 16; c++)
         if (rows[c] & (1u << 16)) x |= (uint16_t)(1u << (15 - c));
   }
   return x;
}

size_t rac3_encode_frame(rac3_encoder_t *e, const float *in, uint8_t *out, size_t cap)
{
   unsigned blk, ch, i, words, frmsizecod;
   size_t   frame_bytes, frame_bits, used;
   unsigned lo, hi, best_snr = 0;
   rac3_bw_t w;
   float *x;

   if (!e || !in || !out)
      return 0;
   /* 44.1 kHz: the size that keeps the rate exact, alternating */
   if (e->fscod == 1)
   {
      e->size_acc += (long)e->kbps * 320 - (long)e->words_even * 147;
      if (e->size_acc >= 147) { words = e->words_odd; e->size_acc -= 147; frmsizecod = e->frmsizecod_even + 1; }
      else { words = e->words_even; frmsizecod = e->frmsizecod_even; }
   }
   else
   {
      words = e->words_even;
      frmsizecod = e->frmsizecod_even;
   }
   frame_bytes = words * 2;
   frame_bits  = frame_bytes * 8;
   if (cap < frame_bytes)
      return 0;

   /* the transforms: block b covers prev's last 256 and the new 256 */
   x = e->x;
   for (ch = 0; ch < e->nfchans + e->lfeon; ch++)
   {
      unsigned sch  = ch < e->nfchans ? ch : ENC_NCH_MAX;
      unsigned slot = ch < e->nfchans ? e->slot[ch] : e->lfe_slot;
      for (blk = 0; blk < 6; blk++)
      {
         for (i = 0; i < 256; i++)
         {
            x[i]       = (blk == 0 ? e->prev[sch][i] : in[((blk - 1) * 256 + i) * e->nchans + slot]) * e->window512[i];
            x[256 + i] = in[(blk * 256 + i) * e->nchans + slot] * e->window512[256 + i];
         }
         mdct512(x, e->coef[blk][sch]);
      }
      for (i = 0; i < 256; i++)
         e->prev[sch][i] = in[(5 * 256 + i) * e->nchans + slot];
   }
   /* exponents for the frame */
   for (ch = 0; ch < e->nfchans; ch++) frame_exponents(e, ch, e->endmant[ch]);
   if (e->lfeon) frame_exponents(e, ENC_NCH_MAX, 7);

   /* the SNR offset: the largest that fits, by bisection on the
    * combined (csnroffst, fsnroffst) in 0..1023 */
   lo = 0; hi = 1023;
   while (lo <= hi)
   {
      unsigned mid = (lo + hi) / 2;
      unsigned cs = mid >> 4, fs = mid & 15;
      for (ch = 0; ch < e->nfchans; ch++) allocate(e, e->exps[ch], 0, e->endmant[ch], cs, fs, e->bap[ch]);
      if (e->lfeon) allocate(e, e->exps[ENC_NCH_MAX], 0, 7, cs, fs, e->bap[ENC_NCH_MAX]);
      w.p = NULL; w.cap = 0;
      used = pack_frame(e, &w, cs, fs, frmsizecod) + 1 + 1 + 16;   /* + auxdatae + crcrsv + crc2 */
      if (used <= frame_bits) { best_snr = mid; lo = mid + 1; }
      else { if (mid == 0) break; hi = mid - 1; }
   }
   /* the final pack, for real */
   {
      unsigned cs = best_snr >> 4, fs = best_snr & 15;
      size_t pad;
      for (ch = 0; ch < e->nfchans; ch++) allocate(e, e->exps[ch], 0, e->endmant[ch], cs, fs, e->bap[ch]);
      if (e->lfeon) allocate(e, e->exps[ENC_NCH_MAX], 0, 7, cs, fs, e->bap[ENC_NCH_MAX]);
      memset(out, 0, frame_bytes);
      w.p = out; w.cap = frame_bytes;
      used = pack_frame(e, &w, cs, fs, frmsizecod);
      /* auxbits to fill, then auxdatae, crcrsv, crc2 */
      pad = frame_bits - 18 - used;
      while (pad--) bw_put(&w, 0, 1);
      bw_put(&w, 0, 1);      /* auxdatae */
      bw_put(&w, 0, 1);      /* crcrsv */
   }
   /* crc1 over the first five eighths (7.10.1), crc2 over the rest */
   {
      size_t five_eighths = ((words >> 1) + (words >> 3)) * 2;
      uint16_t c1 = solve_crc1(out + 4, five_eighths - 4);
      uint16_t c2;
      out[2] = (uint8_t)(c1 >> 8);
      out[3] = (uint8_t)c1;
      c2 = rac3_crc16(out + five_eighths, frame_bytes - 2 - five_eighths);
      out[frame_bytes - 2] = (uint8_t)(c2 >> 8);
      out[frame_bytes - 1] = (uint8_t)c2;
   }
   return frame_bytes;
}

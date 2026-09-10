/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rac3_decode.c).
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

/* An AC-3 decoder, from ATSC A/52:2018 sections 5, 6 and 7 and
 * nothing else: the syntax of section 5.3, exponent decoding of
 * 7.1.3, the parametric bit allocation of 7.2.2 in its seven steps,
 * the mantissas of 7.3, coupling of 7.4, rematrixing of 7.5, dynamic
 * range of 7.7.1, and the transforms of 7.9.4 with the window of
 * 7.33. Section numbers are given where the code follows the text. */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <formats/rac3.h>
#include "rac3_tables.h"

#define RAC3_PI 3.14159265358979323846

/* ---- bit reader ---------------------------------------------------- */

typedef struct
{
   const uint8_t *p;
   size_t len;     /* bytes */
   size_t bit;
   bool   over;
} rac3_br_t;

static unsigned br_get(rac3_br_t *b, unsigned n)
{
   unsigned v = 0;
   while (n--)
   {
      size_t byte = b->bit >> 3;
      if (byte >= b->len)
      {
         b->over = true;
         return 0;
      }
      v = (v << 1) | ((b->p[byte] >> (7 - (b->bit & 7))) & 1u);
      b->bit++;
   }
   return v;
}

static int br_gets(rac3_br_t *b, unsigned n)
{
   /* two's complement of n bits */
   unsigned v = br_get(b, n);
   return (v & (1u << (n - 1))) ? (int)v - (int)(1u << n) : (int)v;
}

/* ---- the decoder's state ----------------------------------------- */

#define RAC3_NCH_MAX 5   /* full-bandwidth channels */

typedef struct
{
   /* per audio block, kept across blocks where the syntax reuses */
   unsigned blksw[RAC3_NCH_MAX];
   unsigned dithflag[RAC3_NCH_MAX];
   unsigned dynrng, dynrng2;
   unsigned cplinu;
   unsigned chincpl[RAC3_NCH_MAX];
   unsigned phsflginu;
   unsigned cplbegf, cplendf;
   unsigned ncplsubnd, ncplbnd;
   unsigned cplbndstrc[18];
   float    cplco[RAC3_NCH_MAX][18];   /* per sub-band after expansion */
   unsigned phsflg[18];
   unsigned rematflg[4];
   unsigned chexpstr[RAC3_NCH_MAX], cplexpstr, lfeexpstr;
   unsigned chbwcod[RAC3_NCH_MAX];
   uint8_t  exps[RAC3_NCH_MAX][256];
   uint8_t  cplexps[256];
   uint8_t  lfeexps[7];
   unsigned endmant[RAC3_NCH_MAX];
   unsigned cplstrtmant, cplendmant;
   /* bit allocation parameters */
   unsigned sdcycod, fdcycod, sgaincod, dbpbcod, floorcod;
   unsigned csnroffst;
   unsigned fsnroffst[RAC3_NCH_MAX], fgaincod[RAC3_NCH_MAX];
   unsigned cplfsnroffst, cplfgaincod;
   unsigned lfefsnroffst, lfefgaincod;
   unsigned cplfleak, cplsleak;
   /* delta bit allocation */
   unsigned cpldeltbae, deltbae[RAC3_NCH_MAX];
   unsigned cpldeltnseg, cpldeltoffst[8], cpldeltlen[8], cpldeltba[8];
   unsigned deltnseg[RAC3_NCH_MAX], deltoffst[RAC3_NCH_MAX][8], deltlen[RAC3_NCH_MAX][8], deltba[RAC3_NCH_MAX][8];
   /* bit allocation output */
   uint8_t  bap[RAC3_NCH_MAX][256];
   uint8_t  cplbap[256];
   uint8_t  lfebap[7];
} rac3_block_t;

struct rac3_decoder
{
   bool     drc;
   unsigned acmod, nfchans, lfeon, fscod;
   unsigned cmixlev, surmixlev;
   /* the transform coefficients of the block, per channel, and the LFE */
   float    coef[RAC3_NCH_MAX][256];
   float    lfecoef[256];
   /* overlap-add delay per output channel (fbw + lfe) */
   float    delay[RAC3_NCH_MAX + 1][256];
   /* transform twiddles and the IFFT tables, made once */
   float    xcos1[128], xsin1[128], xcos2[64], xsin2[64];
   float    fft_cos[128], fft_sin[128];
   /* grouped-mantissa state carried across channels within a block */
   int      grp1_left, grp1_codes[3];
   int      grp2_left, grp2_codes[3];
   int      grp4_left, grp4_codes[2];
   uint32_t dither;
   rac3_block_t blk;
   /* scratch, here rather than on the stack: the decoder runs on
    * threads with small stacks on some targets */
   float    cplcoef[256];
   float    x[512];
   int      psd[256], bndpsd[50], excite[50], mask[50];
   float    zr[128], zi[128];               /* the transforms' complex scratch */
   float    z2r[64], z2i[64], X1[128], X2[128];
};

/* ---- setup ---------------------------------------------------------- */

rac3_decoder_t *rac3_decoder_new(void)
{
   rac3_decoder_t *d = (rac3_decoder_t*)calloc(1, sizeof(*d));
   unsigned k;
   if (!d)
      return NULL;
   d->drc    = true;
   d->dither = 0x12345678u;
   /* 7.9.4.1 step 2 and 7.9.4.2 step 2: the pre/post twiddles */
   for (k = 0; k < 128; k++)
   {
      d->xcos1[k] = (float)-cos(2.0 * RAC3_PI * (8.0 * k + 1.0) / (8.0 * 512.0));
      d->xsin1[k] = (float)-sin(2.0 * RAC3_PI * (8.0 * k + 1.0) / (8.0 * 512.0));
      d->fft_cos[k] = (float)cos(2.0 * RAC3_PI * k / 128.0);
      d->fft_sin[k] = (float)sin(2.0 * RAC3_PI * k / 128.0);
   }
   for (k = 0; k < 64; k++)
   {
      d->xcos2[k] = (float)-cos(2.0 * RAC3_PI * (8.0 * k + 1.0) / (4.0 * 512.0));
      d->xsin2[k] = (float)-sin(2.0 * RAC3_PI * (8.0 * k + 1.0) / (4.0 * 512.0));
   }
   return d;
}

void rac3_decoder_free(rac3_decoder_t *d)
{
   free(d);
}

void rac3_decoder_set_drc(rac3_decoder_t *d, bool on)
{
   if (d)
      d->drc = on;
}

/* ---- 7.1.3 exponent decoding --------------------------------------- */

/* ngrps groups of 7-bit mapped values from the stream, starting from
 * the absolute exponent absexp, into exps[0..]; grpsize is 1, 2 or 4
 * for D15, D25, D45. Writes exps[0] = absexp then the decoded ones.
 * Returns false if an exponent would leave 0..24. */
static bool decode_exponents(rac3_br_t *b, unsigned ngrps, unsigned grpsize,
      unsigned absexp, uint8_t *exps, bool first_is_absexp)
{
   unsigned grp, i;
   int prev = (int)absexp;
   uint8_t *out = exps;
   if (first_is_absexp)
      *out++ = (uint8_t)absexp;
   for (grp = 0; grp < ngrps; grp++)
   {
      unsigned gexp = br_get(b, 7);
      int dexp[3];
      dexp[0] = (int)(gexp / 25);
      dexp[1] = (int)((gexp % 25) / 5);
      dexp[2] = (int)((gexp % 25) % 5);
      for (i = 0; i < 3; i++)
      {
         unsigned r;
         int e = prev + dexp[i] - 2;
         if (e < 0 || e > 24)
            return false;
         for (r = 0; r < grpsize; r++)
            *out++ = (uint8_t)e;
         prev = e;
      }
   }
   return !b->over;
}

/* ---- 7.2.2 parametric bit allocation ------------------------------- */

static int logadd(int a, int b)
{
   int c = a - b;
   int address = (c < 0 ? -c : c) >> 1;
   if (address > 255) address = 255;
   return (c >= 0 ? a : b) + (int)rac3_latab[address];
}

static int calc_lowcomp(int a, int b0, int b1, int bin)
{
   if (bin < 7)
   {
      if ((b0 + 256) == b1)
         a = 384;
      else if (b0 > b1)
         a = a - 64 > 0 ? a - 64 : 0;
   }
   else if (bin < 20)
   {
      if ((b0 + 256) == b1)
         a = 320;
      else if (b0 > b1)
         a = a - 64 > 0 ? a - 64 : 0;
   }
   else
      a = a - 128 > 0 ? a - 128 : 0;
   return a;
}

/* One exponent set through the seven steps, into bap[start..end).
 * is_cpl selects the coupling initialisation; deltbae/nseg/... the
 * delta allocation of that set. */
static void bit_allocation(rac3_decoder_t *d, const rac3_block_t *k,
      const uint8_t *exps, unsigned start, unsigned end,
      unsigned fgaincod, unsigned fsnroffst, bool is_cpl, bool is_lfe,
      unsigned deltbae, unsigned deltnseg, const unsigned *deltoffst,
      const unsigned *deltlen, const unsigned *deltba,
      uint8_t *bap)
{
   int *psd = d->psd, *bndpsd = d->bndpsd, *excite = d->excite, *mask = d->mask;
   int sdecay, fdecay, sgain, dbknee, floor_, fgain, snroffset;
   int fastleak = 0, slowleak = 0, lowcomp = 0;
   unsigned bin, j, kk, bndstrt, bndend, begin, lastbin;

   /* 7.2.2.1.1: every SNR offset zero means no bits anywhere */
   if (k->csnroffst == 0 && fsnroffst == 0)
   {
      memset(bap + start, 0, end - start);
      return;
   }
   sdecay = rac3_slowdec[k->sdcycod];
   fdecay = rac3_fastdec[k->fdcycod];
   sgain  = rac3_slowgain[k->sgaincod];
   dbknee = rac3_dbpbtab[k->dbpbcod];
   floor_ = rac3_floortab[k->floorcod];
   if (floor_ == 0xf800)
      floor_ = (int)(int16_t)0xf800;
   fgain  = rac3_fastgain[fgaincod];
   /* (((csnroffst - 15) << 4) + fsnroffst) << 2, as multiplies so a
    * negative csnroffst - 15 is defined. */
   snroffset = ((((int)k->csnroffst - 15) * 16) + (int)fsnroffst) * 4;
   if (is_cpl)
   {
      fastleak = ((int)k->cplfleak << 8) + 768;
      slowleak = ((int)k->cplsleak << 8) + 768;
   }

   /* 7.2.2.2 */
   for (bin = start; bin < end; bin++)
      psd[bin] = 3072 - ((int)exps[bin] << 7);

   /* 7.2.2.3 */
   j  = start;
   kk = rac3_masktab[start];
   do
   {
      unsigned i;
      lastbin = rac3_bndtab[kk] + rac3_bndsz[kk];
      if (lastbin > end) lastbin = end;
      bndpsd[kk] = psd[j];
      j++;
      for (i = j; i < lastbin; i++)
      {
         bndpsd[kk] = logadd(bndpsd[kk], psd[j]);
         j++;
      }
      kk++;
   } while (end > lastbin);

   /* 7.2.2.4 */
   bndstrt = rac3_masktab[start];
   bndend  = rac3_masktab[end - 1] + 1;
   if (bndstrt == 0)   /* fbw and lfe */
   {
      lowcomp = calc_lowcomp(lowcomp, bndpsd[0], bndpsd[1], 0);
      excite[0] = bndpsd[0] - fgain - lowcomp;
      lowcomp = calc_lowcomp(lowcomp, bndpsd[1], bndpsd[2], 1);
      excite[1] = bndpsd[1] - fgain - lowcomp;
      begin = 7;
      for (bin = 2; bin < 7; bin++)
      {
         if (!(bndend == 7 && bin == 6))
            lowcomp = calc_lowcomp(lowcomp, bndpsd[bin], bndpsd[bin + 1], (int)bin);
         fastleak = bndpsd[bin] - fgain;
         slowleak = bndpsd[bin] - sgain;
         excite[bin] = fastleak - lowcomp;
         if (!(bndend == 7 && bin == 6))
         {
            if (bndpsd[bin] <= bndpsd[bin + 1])
            {
               begin = bin + 1;
               break;
            }
         }
      }
      for (bin = begin; bin < (bndend < 22 ? bndend : 22); bin++)
      {
         if (!(bndend == 7 && bin == 6))
            lowcomp = calc_lowcomp(lowcomp, bndpsd[bin], bndpsd[bin + 1], (int)bin);
         fastleak -= fdecay;
         if (fastleak < bndpsd[bin] - fgain) fastleak = bndpsd[bin] - fgain;
         slowleak -= sdecay;
         if (slowleak < bndpsd[bin] - sgain) slowleak = bndpsd[bin] - sgain;
         excite[bin] = (fastleak - lowcomp > slowleak) ? fastleak - lowcomp : slowleak;
      }
      begin = 22;
   }
   else                /* coupling channel */
      begin = bndstrt;
   for (bin = begin; bin < bndend; bin++)
   {
      fastleak -= fdecay;
      if (fastleak < bndpsd[bin] - fgain) fastleak = bndpsd[bin] - fgain;
      slowleak -= sdecay;
      if (slowleak < bndpsd[bin] - sgain) slowleak = bndpsd[bin] - sgain;
      excite[bin] = fastleak > slowleak ? fastleak : slowleak;
   }

   /* 7.2.2.5 */
   for (bin = bndstrt; bin < bndend; bin++)
   {
      int m;
      if (bndpsd[bin] < dbknee)
         excite[bin] += (dbknee - bndpsd[bin]) >> 2;
      m = (int)rac3_hth[d->fscod][bin];
      mask[bin] = excite[bin] > m ? excite[bin] : m;
   }

   /* 7.2.2.6 */
   if (deltbae == 0 || deltbae == 1)
   {
      unsigned seg, band = 0;
      for (seg = 0; seg < deltnseg + 1; seg++)
      {
         int delta;
         unsigned c;
         band += deltoffst[seg];
         if (deltba[seg] >= 4)
            delta = ((int)deltba[seg] - 3) * 128;
         else
            delta = ((int)deltba[seg] - 4) * 128;
         for (c = 0; c < deltlen[seg]; c++)
         {
            if (band < 50)
               mask[band] += delta;
            band++;
         }
      }
   }

   /* 7.2.2.7 */
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
         if (mask[j] < 0)
            mask[j] = 0;
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
   (void)is_lfe;
}

/* ---- 7.3 mantissas --------------------------------------------------- */

/* Symmetric quantizer value for level L and code c: (2c + 1 - L) / L. */
static float sym_value(unsigned levels, unsigned code)
{
   return (float)(2 * (int)code + 1 - (int)levels) / (float)levels;
}

/* The dither of 7.3.4: uniform in about +/-0.707. */
static float next_dither(rac3_decoder_t *d)
{
   d->dither = d->dither * 1664525u + 1013904223u;
   return ((float)(int16_t)(d->dither >> 16) / 32768.0f) * 0.707f;
}

/* One mantissa by its bap, as a fraction, from the stream; the
 * grouped kinds are shared across sets within a block (7.3.5). */
static float read_mantissa(rac3_decoder_t *d, rac3_br_t *b, unsigned bap, bool dith)
{
   unsigned code;
   switch (bap)
   {
      case 0:
         return dith ? next_dither(d) : 0.0f;
      case 1:
         if (d->grp1_left == 0)
         {
            code = br_get(b, 5);
            d->grp1_codes[0] = (int)(code / 9);
            d->grp1_codes[1] = (int)((code % 9) / 3);
            d->grp1_codes[2] = (int)((code % 9) % 3);
            d->grp1_left = 3;
         }
         code = (unsigned)d->grp1_codes[3 - d->grp1_left];
         d->grp1_left--;
         return sym_value(3, code < 3 ? code : 2);
      case 2:
         if (d->grp2_left == 0)
         {
            code = br_get(b, 7);
            d->grp2_codes[0] = (int)(code / 25);
            d->grp2_codes[1] = (int)((code % 25) / 5);
            d->grp2_codes[2] = (int)((code % 25) % 5);
            d->grp2_left = 3;
         }
         code = (unsigned)d->grp2_codes[3 - d->grp2_left];
         d->grp2_left--;
         return sym_value(5, code < 5 ? code : 4);
      case 3:
         code = br_get(b, 3);
         return sym_value(7, code < 7 ? code : 6);
      case 4:
         if (d->grp4_left == 0)
         {
            code = br_get(b, 7);
            d->grp4_codes[0] = (int)(code / 11);
            d->grp4_codes[1] = (int)(code % 11);
            d->grp4_left = 2;
         }
         code = (unsigned)d->grp4_codes[2 - d->grp4_left];
         d->grp4_left--;
         return sym_value(11, code < 11 ? code : 10);
      case 5:
         code = br_get(b, 4);
         return sym_value(15, code < 15 ? code : 14);
      default:
      {
         /* 7.3.2: asymmetric two's complement fraction of bap_bits bits */
         unsigned bits = rac3_bap_bits[bap];
         int v = br_gets(b, bits);
         return (float)v / (float)(1u << (bits - 1));
      }
   }
}

/* A set of mantissas into coef[start..end) scaled by their exponents. */
static void read_mantissas(rac3_decoder_t *d, rac3_br_t *b, const uint8_t *bap,
      const uint8_t *exps, unsigned start, unsigned end, bool dith, float *coef)
{
   unsigned bin;
   for (bin = start; bin < end; bin++)
   {
      float m = read_mantissa(d, b, bap[bin], dith);
      coef[bin] = ldexpf(m, -(int)exps[bin]);
   }
}

/* ---- 7.9.4 the transforms -------------------------------------------- */

/* N/4 = 128 point complex IFFT, radix-2, in place. Also serves the
 * 64-point one with stride 2 in the twiddles. */
static void ifft(const rac3_decoder_t *d, float *re, float *im, unsigned n)
{
   unsigned i, j, k, len, stride = 128 / n;
   /* bit reversal */
   for (i = 1, j = 0; i < n; i++)
   {
      unsigned bit = n >> 1;
      for (; j & bit; bit >>= 1) j ^= bit;
      j ^= bit;
      if (i < j)
      {
         float t = re[i]; re[i] = re[j]; re[j] = t;
         t = im[i]; im[i] = im[j]; im[j] = t;
      }
   }
   for (len = 2; len <= n; len <<= 1)
   {
      unsigned step = n / len;
      for (i = 0; i < n; i += len)
      {
         for (k = 0; k < len / 2; k++)
         {
            /* e^{+j 2 pi k / len}: an inverse transform */
            float wr = d->fft_cos[k * step * stride];
            float wi = d->fft_sin[k * step * stride];
            float ur = re[i + k], ui = im[i + k];
            float vr = re[i + k + len / 2] * wr - im[i + k + len / 2] * wi;
            float vi = re[i + k + len / 2] * wi + im[i + k + len / 2] * wr;
            re[i + k] = ur + vr;            im[i + k] = ui + vi;
            re[i + k + len / 2] = ur - vr;  im[i + k + len / 2] = ui - vi;
         }
      }
   }
}

/* 7.9.4.1: 512-point, X[0..255] to x[0..511] windowed. */
static void imdct512(rac3_decoder_t *d, const float *X, float *x)
{
   float *zr = d->zr, *zi = d->zi;
   unsigned k, n;
   const unsigned N = 512;
   for (k = 0; k < N / 4; k++)
   {
      float a = X[N / 2 - 2 * k - 1], bb = X[2 * k];
      zr[k] = a * d->xcos1[k] - bb * d->xsin1[k];
      zi[k] = bb * d->xcos1[k] + a * d->xsin1[k];
   }
   ifft(d, zr, zi, 128);
   for (n = 0; n < N / 4; n++)
   {
      float r = zr[n], i = zi[n];
      zr[n] = r * d->xcos1[n] - i * d->xsin1[n];
      zi[n] = i * d->xcos1[n] + r * d->xsin1[n];
   }
   for (n = 0; n < N / 8; n++)
   {
      x[2 * n]               = -zi[N / 8 + n]         * rac3_window[2 * n];
      x[2 * n + 1]           =  zr[N / 8 - n - 1]     * rac3_window[2 * n + 1];
      x[N / 4 + 2 * n]       = -zr[n]                 * rac3_window[N / 4 + 2 * n];
      x[N / 4 + 2 * n + 1]   =  zi[N / 4 - n - 1]     * rac3_window[N / 4 + 2 * n + 1];
      x[N / 2 + 2 * n]       = -zr[N / 8 + n]         * rac3_window[N / 2 - 2 * n - 1];
      x[N / 2 + 2 * n + 1]   =  zi[N / 8 - n - 1]     * rac3_window[N / 2 - 2 * n - 2];
      x[3 * N / 4 + 2 * n]   =  zi[n]                 * rac3_window[N / 4 - 2 * n - 1];
      x[3 * N / 4 + 2 * n + 1] = -zr[N / 4 - n - 1]   * rac3_window[N / 4 - 2 * n - 2];
   }
}

/* 7.9.4.2: two 256-point transforms on the even and odd coefficients. */
static void imdct256(rac3_decoder_t *d, const float *X, float *x)
{
   float *X1 = d->X1, *X2 = d->X2, *z1r = d->zr, *z1i = d->zi, *z2r = d->z2r, *z2i = d->z2i;
   unsigned k, n;
   const unsigned N = 512;
   for (k = 0; k < N / 4; k++)
   {
      X1[k] = X[2 * k];
      X2[k] = X[2 * k + 1];
   }
   for (k = 0; k < N / 8; k++)
   {
      float a1 = X1[N / 4 - 2 * k - 1], b1 = X1[2 * k];
      float a2 = X2[N / 4 - 2 * k - 1], b2 = X2[2 * k];
      z1r[k] = a1 * d->xcos2[k] - b1 * d->xsin2[k];
      z1i[k] = b1 * d->xcos2[k] + a1 * d->xsin2[k];
      z2r[k] = a2 * d->xcos2[k] - b2 * d->xsin2[k];
      z2i[k] = b2 * d->xcos2[k] + a2 * d->xsin2[k];
   }
   ifft(d, z1r, z1i, 64);
   ifft(d, z2r, z2i, 64);
   for (n = 0; n < N / 8; n++)
   {
      float r, i;
      r = z1r[n]; i = z1i[n];
      z1r[n] = r * d->xcos2[n] - i * d->xsin2[n];
      z1i[n] = i * d->xcos2[n] + r * d->xsin2[n];
      r = z2r[n]; i = z2i[n];
      z2r[n] = r * d->xcos2[n] - i * d->xsin2[n];
      z2i[n] = i * d->xcos2[n] + r * d->xsin2[n];
   }
   for (n = 0; n < N / 8; n++)
   {
      x[2 * n]                 = -z1i[n]              * rac3_window[2 * n];
      x[2 * n + 1]             =  z1r[N / 8 - n - 1]  * rac3_window[2 * n + 1];
      x[N / 4 + 2 * n]         = -z1r[n]              * rac3_window[N / 4 + 2 * n];
      x[N / 4 + 2 * n + 1]     =  z1i[N / 8 - n - 1]  * rac3_window[N / 4 + 2 * n + 1];
      x[N / 2 + 2 * n]         = -z2r[n]              * rac3_window[N / 2 - 2 * n - 1];
      x[N / 2 + 2 * n + 1]     =  z2i[N / 8 - n - 1]  * rac3_window[N / 2 - 2 * n - 2];
      x[3 * N / 4 + 2 * n]     =  z2i[n]              * rac3_window[N / 4 - 2 * n - 1];
      x[3 * N / 4 + 2 * n + 1] = -z2r[N / 8 - n - 1]  * rac3_window[N / 4 - 2 * n - 2];
   }
}

/* The 512-entry window from the 256 stored: w[n] for n >= 256 is
 * w[511 - n]. The indexing above uses only the first half's indices
 * except the mirrored ones, which are written as N/2 - 2n - 1 etc,
 * all below 256. */

/* Overlap-add of 7.9.4.1 step 6, into out (stride, one channel). */
static void overlap_add(float *delay, const float *x, float *out, unsigned stride)
{
   unsigned n;
   for (n = 0; n < 256; n++)
   {
      out[n * stride] = 2.0f * (x[n] + delay[n]);
      delay[n] = x[256 + n];
   }
}

/* ---- 7.7.1 dynamic range -------------------------------------------- */

static float dynrng_gain(unsigned dynrng)
{
   int x;
   float y;
   if (dynrng == 0)
      return 1.0f;
   x = (int)(dynrng >> 5);          /* 3-bit signed */
   if (x >= 4) x -= 8;
   y = (float)(32 + (dynrng & 31)) / 64.0f;   /* 0.1YYYYY */
   return ldexpf(y, x + 1);
}

/* ---- the frame ----------------------------------------------------- */

/* Coupling sub-band k spans bins 37 + 12k .. 48 + 12k (Table 7.24). */

static bool decode_block(rac3_decoder_t *d, rac3_br_t *b, unsigned blk, float *out, unsigned nchans_out)
{
   rac3_block_t *k = &d->blk;
   unsigned ch, bnd, i;
   unsigned nfchans = d->nfchans;
   float   *cplcoef = d->cplcoef;

   /* block switch and dither flags */
   for (ch = 0; ch < nfchans; ch++) k->blksw[ch]    = br_get(b, 1);
   for (ch = 0; ch < nfchans; ch++) k->dithflag[ch] = br_get(b, 1);
   /* dynamic range */
   if (br_get(b, 1)) k->dynrng = br_get(b, 8);
   else if (blk == 0) k->dynrng = 0;
   if (d->acmod == 0)
   {
      if (br_get(b, 1)) k->dynrng2 = br_get(b, 8);
      else if (blk == 0) k->dynrng2 = 0;
   }
   /* coupling strategy */
   if (br_get(b, 1))
   {
      k->cplinu = br_get(b, 1);
      if (k->cplinu)
      {
         for (ch = 0; ch < nfchans; ch++) k->chincpl[ch] = br_get(b, 1);
         k->phsflginu = (d->acmod == 2) ? br_get(b, 1) : 0;
         k->cplbegf = br_get(b, 4);
         k->cplendf = br_get(b, 4);
         if (3 + k->cplendf < k->cplbegf)
            return false;
         k->ncplsubnd = 3 + k->cplendf - k->cplbegf;
         k->cplbndstrc[0] = 0;
         k->ncplbnd = k->ncplsubnd;
         for (bnd = 1; bnd < k->ncplsubnd; bnd++)
         {
            k->cplbndstrc[bnd] = br_get(b, 1);
            k->ncplbnd -= k->cplbndstrc[bnd];
         }
         k->cplstrtmant = 37 + 12 * k->cplbegf;
         k->cplendmant  = 37 + 12 * (k->cplendf + 3);
      }
      else
         for (ch = 0; ch < nfchans; ch++) k->chincpl[ch] = 0;
   }
   else if (blk == 0)
      return false;
   /* coupling coordinates and phase flags */
   if (k->cplinu)
   {
      unsigned cplcoe[RAC3_NCH_MAX] = {0};
      for (ch = 0; ch < nfchans; ch++)
      {
         if (!k->chincpl[ch]) continue;
         cplcoe[ch] = br_get(b, 1);
         if (cplcoe[ch])
         {
            unsigned mstrcplco = br_get(b, 2);
            unsigned sb = 0;
            float bandco[18];
            for (bnd = 0; bnd < k->ncplbnd; bnd++)
            {
               unsigned e = br_get(b, 4), m = br_get(b, 4);
               float t = (e == 15) ? (float)m / 16.0f : (float)(m + 16) / 32.0f;
               bandco[bnd] = ldexpf(t, -(int)(e + 3 * mstrcplco));
            }
            /* bands to sub-bands by cplbndstrc */
            bnd = 0;
            for (sb = 0; sb < k->ncplsubnd; sb++)
            {
               if (sb > 0 && !k->cplbndstrc[sb]) bnd++;
               k->cplco[ch][sb] = bandco[bnd];
            }
         }
      }
      if (d->acmod == 2 && k->phsflginu && (cplcoe[0] || cplcoe[1]))
      {
         float bandph[18];
         unsigned sb;
         for (bnd = 0; bnd < k->ncplbnd; bnd++) bandph[bnd] = br_get(b, 1) ? -1.0f : 1.0f;
         bnd = 0;
         for (sb = 0; sb < k->ncplsubnd; sb++)
         {
            if (sb > 0 && !k->cplbndstrc[sb]) bnd++;
            k->phsflg[sb] = bandph[bnd] < 0 ? 1 : 0;
         }
      }
   }
   /* rematrixing */
   if (d->acmod == 2)
   {
      if (br_get(b, 1))
      {
         unsigned nremat = 4;
         if (k->cplinu)
         {
            if (k->cplbegf > 2)       nremat = 4;
            else if (k->cplbegf > 0)  nremat = 3;
            else                      nremat = 2;
         }
         for (bnd = 0; bnd < nremat; bnd++) k->rematflg[bnd] = br_get(b, 1);
      }
      else if (blk == 0)
         return false;
   }
   /* exponent strategy */
   if (k->cplinu) k->cplexpstr = br_get(b, 2);
   for (ch = 0; ch < nfchans; ch++) k->chexpstr[ch] = br_get(b, 2);
   if (d->lfeon) k->lfeexpstr = br_get(b, 1);
   for (ch = 0; ch < nfchans; ch++)
   {
      if (k->chexpstr[ch] != 0 && !k->chincpl[ch])
      {
         k->chbwcod[ch] = br_get(b, 6);
         if (k->chbwcod[ch] > 60)
            return false;
      }
   }
   /* end mantissas */
   for (ch = 0; ch < nfchans; ch++)
      k->endmant[ch] = k->chincpl[ch] ? k->cplstrtmant : ((k->chbwcod[ch] + 12) * 3) + 37;
   /* exponents */
   if (k->cplinu && k->cplexpstr != 0)
   {
      unsigned grpsize = 1u << (k->cplexpstr - 1);
      unsigned ngrps   = (k->cplendmant - k->cplstrtmant) / (3 * grpsize);
      unsigned absexp  = br_get(b, 4) << 1;
      if (!decode_exponents(b, ngrps, grpsize, absexp, k->cplexps + k->cplstrtmant, false))
         return false;
   }
   for (ch = 0; ch < nfchans; ch++)
   {
      if (k->chexpstr[ch] != 0)
      {
         unsigned grpsize = 1u << (k->chexpstr[ch] - 1);
         unsigned ngrps   = (k->endmant[ch] - 1 + 3 * grpsize - 3) / (3 * grpsize);
         unsigned absexp  = br_get(b, 4);
         if (!decode_exponents(b, ngrps, grpsize, absexp, k->exps[ch], true))
            return false;
         br_get(b, 2);   /* gainrng: for a fixed-point transform; not used here */
      }
   }
   if (d->lfeon && k->lfeexpstr != 0)
   {
      unsigned absexp = br_get(b, 4);
      if (!decode_exponents(b, 2, 1, absexp, k->lfeexps, true))
         return false;
   }
   /* bit allocation parameters */
   if (br_get(b, 1))
   {
      k->sdcycod  = br_get(b, 2);
      k->fdcycod  = br_get(b, 2);
      k->sgaincod = br_get(b, 2);
      k->dbpbcod  = br_get(b, 2);
      k->floorcod = br_get(b, 3);
   }
   else if (blk == 0)
      return false;
   if (br_get(b, 1))
   {
      k->csnroffst = br_get(b, 6);
      if (k->cplinu)
      {
         k->cplfsnroffst = br_get(b, 4);
         k->cplfgaincod  = br_get(b, 3);
      }
      for (ch = 0; ch < nfchans; ch++)
      {
         k->fsnroffst[ch] = br_get(b, 4);
         k->fgaincod[ch]  = br_get(b, 3);
      }
      if (d->lfeon)
      {
         k->lfefsnroffst = br_get(b, 4);
         k->lfefgaincod  = br_get(b, 3);
      }
   }
   else if (blk == 0)
      return false;
   if (k->cplinu)
   {
      if (br_get(b, 1))
      {
         k->cplfleak = br_get(b, 3);
         k->cplsleak = br_get(b, 3);
      }
   }
   /* delta bit allocation */
   if (br_get(b, 1))
   {
      if (k->cplinu) k->cpldeltbae = br_get(b, 2);
      for (ch = 0; ch < nfchans; ch++) k->deltbae[ch] = br_get(b, 2);
      if (k->cplinu && k->cpldeltbae == 1)
      {
         unsigned seg;
         k->cpldeltnseg = br_get(b, 3);
         for (seg = 0; seg <= k->cpldeltnseg; seg++)
         {
            k->cpldeltoffst[seg] = br_get(b, 5);
            k->cpldeltlen[seg]   = br_get(b, 4);
            k->cpldeltba[seg]    = br_get(b, 3);
         }
      }
      for (ch = 0; ch < nfchans; ch++)
      {
         if (k->deltbae[ch] == 1)
         {
            unsigned seg;
            k->deltnseg[ch] = br_get(b, 3);
            for (seg = 0; seg <= k->deltnseg[ch]; seg++)
            {
               k->deltoffst[ch][seg] = br_get(b, 5);
               k->deltlen[ch][seg]   = br_get(b, 4);
               k->deltba[ch][seg]    = br_get(b, 3);
            }
         }
      }
      if (k->cpldeltbae == 3) return false;
      for (ch = 0; ch < nfchans; ch++) if (k->deltbae[ch] == 3) return false;
   }
   else if (blk == 0)
   {
      k->cpldeltbae = 2;
      for (ch = 0; ch < nfchans; ch++) k->deltbae[ch] = 2;
   }
   /* skip field */
   if (br_get(b, 1))
   {
      unsigned skipl = br_get(b, 9);
      while (skipl--) br_get(b, 8);
   }
   if (b->over)
      return false;

   /* bit allocation: every set, then the mantissas in stream order */
   for (ch = 0; ch < nfchans; ch++)
      bit_allocation(d, k, k->exps[ch], 0, k->endmant[ch], k->fgaincod[ch], k->fsnroffst[ch],
            false, false, k->deltbae[ch], k->deltnseg[ch], k->deltoffst[ch], k->deltlen[ch], k->deltba[ch], k->bap[ch]);
   if (k->cplinu)
      bit_allocation(d, k, k->cplexps, k->cplstrtmant, k->cplendmant, k->cplfgaincod, k->cplfsnroffst,
            true, false, k->cpldeltbae, k->cpldeltnseg, k->cpldeltoffst, k->cpldeltlen, k->cpldeltba, k->cplbap);
   if (d->lfeon)
      bit_allocation(d, k, k->lfeexps, 0, 7, k->lfefgaincod, k->lfefsnroffst,
            false, true, 2, 0, NULL, NULL, NULL, k->lfebap);

   /* mantissas: the grouped kinds restart each block */
   d->grp1_left = d->grp2_left = d->grp4_left = 0;
   memset(cplcoef, 0, 256 * sizeof(float));
   {
      bool got_cpl = false;
      for (ch = 0; ch < nfchans; ch++)
      {
         memset(d->coef[ch], 0, sizeof(d->coef[ch]));
         read_mantissas(d, b, k->bap[ch], k->exps[ch], 0, k->endmant[ch], k->dithflag[ch] != 0, d->coef[ch]);
         if (k->cplinu && k->chincpl[ch] && !got_cpl)
         {
            /* The coupling channel's dither is applied per coupled
             * channel after decoupling (7.3.4); read it undithered
             * and dither the zero-bap bins per channel below. */
            read_mantissas(d, b, k->cplbap, k->cplexps, k->cplstrtmant, k->cplendmant, false, cplcoef);
            got_cpl = true;
         }
      }
      if (d->lfeon)
      {
         memset(d->lfecoef, 0, sizeof(d->lfecoef));
         read_mantissas(d, b, k->lfebap, k->lfeexps, 0, 7, false, d->lfecoef);
      }
   }
   if (b->over)
      return false;

   /* 7.4: decoupling */
   if (k->cplinu)
   {
      for (ch = 0; ch < nfchans; ch++)
      {
         unsigned sb;
         if (!k->chincpl[ch]) continue;
         for (sb = 0; sb < k->ncplsubnd; sb++)
         {
            float co = k->cplco[ch][sb] * 8.0f;
            if (d->acmod == 2 && ch == 1 && k->phsflginu && k->phsflg[sb])
               co = -co;
            for (i = 0; i < 12; i++)
            {
               unsigned bin = k->cplstrtmant + sb * 12 + i;
               float v;
               if (k->cplbap[bin] == 0 && k->dithflag[ch])
                  v = ldexpf(next_dither(d), -(int)k->cplexps[bin]);
               else
                  v = cplcoef[bin];
               d->coef[ch][bin] = v * co;
            }
         }
      }
   }

   /* 7.5: rematrixing, 2/0 only */
   if (d->acmod == 2)
   {
      unsigned bandlo[4] = { 13, 25, 37, 61 };
      unsigned bandhi[4] = { 25, 37, 61, 253 };
      unsigned nremat = 4, end = k->endmant[0] < k->endmant[1] ? k->endmant[0] : k->endmant[1];
      if (k->cplinu)
      {
         if (k->cplbegf > 2)      { nremat = 4; bandhi[3] = k->cplstrtmant; }
         else if (k->cplbegf > 0) { nremat = 3; bandhi[2] = k->cplstrtmant; }
         else                     { nremat = 2; bandhi[1] = k->cplstrtmant; }
      }
      else
         bandhi[3] = end;
      for (bnd = 0; bnd < nremat; bnd++)
      {
         if (!k->rematflg[bnd]) continue;
         for (i = bandlo[bnd]; i < bandhi[bnd] && i < end; i++)
         {
            float l = d->coef[0][i], r = d->coef[1][i];
            d->coef[0][i] = l + r;
            d->coef[1][i] = l - r;
         }
      }
   }

   /* 7.7.1: dynamic range */
   if (d->drc)
   {
      float g1 = dynrng_gain(k->dynrng);
      float g2 = (d->acmod == 0) ? dynrng_gain(k->dynrng2) : g1;
      for (ch = 0; ch < nfchans; ch++)
      {
         float g = (d->acmod == 0 && ch == 1) ? g2 : g1;
         if (g != 1.0f)
            for (i = 0; i < 256; i++) d->coef[ch][i] *= g;
      }
      if (d->lfeon && g1 != 1.0f)
         for (i = 0; i < 7; i++) d->lfecoef[i] *= g1;
   }

   /* 7.9: transforms and overlap-add, into the output in the mask's
    * order: the stream's channel order per acmod is mapped to slots. */
   {
      /* Table 5.8's order per acmod, as slots of the mask order. For
       * 3/2: L C R Ls Rs -> FL FR FC (LFE) SL SR: slots 0 2 1 [3+lfe] ... */
      static const uint8_t order[8][5] = {
         { 0, 1, 0, 0, 0 },            /* 1+1: Ch1 Ch2 as L R */
         { 0, 0, 0, 0, 0 },            /* 1/0: C */
         { 0, 1, 0, 0, 0 },            /* 2/0: L R */
         { 0, 2, 1, 0, 0 },            /* 3/0: L C R -> FL FR FC */
         { 0, 1, 2, 0, 0 },            /* 2/1: L R S -> FL FR BC */
         { 0, 2, 1, 3, 0 },            /* 3/1: L C R S -> FL FR FC BC */
         { 0, 1, 2, 3, 0 },            /* 2/2: L R Ls Rs -> FL FR SL SR */
         { 0, 2, 1, 3, 4 }             /* 3/2: L C R Ls Rs -> FL FR FC SL SR */
      };
      /* The LFE's slot in the mask order is after FC (bit 8 follows
       * bit 4): for layouts with a centre it is slot 3, else 2. */
      unsigned lfe_slot = 0;
      float *x = d->x;
      if (d->lfeon)
      {
         static const uint8_t lfeslot[8] = { 2, 1, 2, 3, 2, 3, 2, 3 };
         lfe_slot = lfeslot[d->acmod];
      }
      for (ch = 0; ch < nfchans; ch++)
      {
         unsigned slot = order[d->acmod][ch];
         /* channels after the LFE's slot move up one */
         if (d->lfeon && slot >= lfe_slot) slot++;
         if (k->blksw[ch]) imdct256(d, d->coef[ch], x);
         else              imdct512(d, d->coef[ch], x);
         overlap_add(d->delay[ch], x, out + blk * 256 * nchans_out + slot, nchans_out);
      }
      if (d->lfeon)
      {
         imdct512(d, d->lfecoef, x);
         overlap_add(d->delay[RAC3_NCH_MAX], x, out + blk * 256 * nchans_out + lfe_slot, nchans_out);
      }
   }
   return true;
}

size_t rac3_decode_frame(rac3_decoder_t *d, const uint8_t *src, size_t len,
      float *out, rac3_frame_info_t *info)
{
   rac3_br_t b;
   unsigned blk, acmod;
   rac3_frame_info_t local;

   if (!info) info = &local;
   if (rac3_parse_frame_info(src, len, info) != RAC3_OK)
      return 0;
   if (info->kind != RAC3_KIND_AC3 || info->frame_bytes > len)
      return 0;
   if (!rac3_frame_crc_ok(src, info->frame_bytes))
      return 0;

   /* A change of configuration resets the overlap. */
   if (d->acmod != info->acmod || d->lfeon != (unsigned)info->lfe || d->fscod != (info->sample_rate == 48000 ? 0 : info->sample_rate == 44100 ? 1 : 2))
   {
      memset(d->delay, 0, sizeof(d->delay));
      memset(&d->blk, 0, sizeof(d->blk));
   }
   d->acmod   = info->acmod;
   d->lfeon   = info->lfe ? 1 : 0;
   d->fscod   = info->sample_rate == 48000 ? 0 : info->sample_rate == 44100 ? 1 : 2;
   {
      static const uint8_t nfchans_of[8] = { 2, 1, 2, 3, 3, 4, 4, 5 };
      d->nfchans = nfchans_of[d->acmod];
   }

   /* Walk the BSI to the audio blocks: syncinfo is 40 bits; then the
    * bsi as Table 5.2. */
   b.p = src; b.len = info->frame_bytes; b.bit = 40; b.over = false;
   br_get(&b, 5);                     /* bsid */
   br_get(&b, 3);                     /* bsmod */
   acmod = br_get(&b, 3);
   if ((acmod & 1) && acmod != 1) d->cmixlev = br_get(&b, 2);
   if (acmod & 4) d->surmixlev = br_get(&b, 2);
   if (acmod == 2) br_get(&b, 2);
   br_get(&b, 1);                     /* lfeon */
   br_get(&b, 5);                     /* dialnorm */
   if (br_get(&b, 1)) br_get(&b, 8);  /* compr */
   if (br_get(&b, 1)) br_get(&b, 8);  /* langcod */
   if (br_get(&b, 1)) br_get(&b, 7);  /* mixlevel, roomtyp */
   if (acmod == 0)
   {
      br_get(&b, 5);
      if (br_get(&b, 1)) br_get(&b, 8);
      if (br_get(&b, 1)) br_get(&b, 8);
      if (br_get(&b, 1)) br_get(&b, 7);
   }
   br_get(&b, 2);                     /* copyrightb, origbs */
   if (br_get(&b, 1)) br_get(&b, 14); /* timecod1 */
   if (br_get(&b, 1)) br_get(&b, 14); /* timecod2 */
   if (br_get(&b, 1))                 /* addbsi */
   {
      unsigned n = br_get(&b, 6) + 1;
      while (n--) br_get(&b, 8);
   }
   if (b.over)
      return 0;

   for (blk = 0; blk < 6; blk++)
      if (!decode_block(d, &b, blk, out, info->channels))
         return 0;
   return 1536;
}

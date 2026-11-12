/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (raac_encode.c).
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

/* AAC-LC encoder. See raac_encode.h. Written from ISO/IEC 14496-3,
 * subpart 4, its tables shared with raac (raac_tables.h).
 *
 * The MDCT is a DCT-IV of the folded window, by a complex FFT of a
 * quarter its length. A band's scalefactor is sf = B + 2 log2(rms),
 * its step growing with the square root of the band's level: halfway
 * between the same noise in every band (the most signal-to-noise ratio
 * overall) and the same ratio in every band (quiet bands kept as well
 * as loud). B, one number a frame, is searched for the frame's bits;
 * no step is finer than 60 dB under the frame's loudest band. */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <formats/raac_encode.h>

#include "raac_tables.h"

#define N      2048
#define M      1024
#define MAX_SFB 51

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct
{
   float re, im;
} cplx_t;

typedef struct
{
   float  prev[M];          /* the last frame's samples */
   float  spec[M];          /* this frame's MDCT */
   int    q[M];             /* its quantised values */
   int    sf[MAX_SFB];
   uint8_t cb[MAX_SFB];
   int    sf_floor;         /* the frame's finest scalefactor */
} enc_channel_t;

struct raac_encoder
{
   raac_encoder_config_t cfg;
   unsigned  sfi;
   unsigned  num_swb, max_sfb;
   const uint16_t *swb;
   int       debt;           /* bits over the rate so far */
   int       last_b;         /* the last frame's B, where the search starts */
   bool      flushed;

   float     window[N];
   cplx_t    twiddle[M / 4];     /* the FFT's */
   cplx_t    pre[M / 2], post[M / 2];
   uint16_t  bitrev[M / 2];

   enc_channel_t ch[2];

   /* scratch */
   float     x[N];
   cplx_t    z[M / 2];

   /* output */
   uint8_t  *out;
   size_t    cap, pos;
   uint32_t  acc;
   unsigned  nbits;
   bool      overflow;
};

/* --------------------------------------------------------------------- */
/* Transform                                                             */
/* --------------------------------------------------------------------- */

/* In place, radix 2, M/2 points */
static void fft(raac_encoder_t *e, cplx_t *z)
{
   unsigned n = M / 2, i, len;
   for (i = 0; i < n; i++)
   {
      unsigned j = e->bitrev[i];
      if (j > i)
      {
         cplx_t t = z[i];
         z[i] = z[j];
         z[j] = t;
      }
   }
   for (len = 2; len <= n; len <<= 1)
   {
      unsigned half = len / 2, step = n / len, k;
      for (i = 0; i < n; i += len)
         for (k = 0; k < half; k++)
         {
            cplx_t w = e->twiddle[k * step];
            cplx_t a = z[i + k], b = z[i + k + half], t;
            t.re = b.re * w.re - b.im * w.im;
            t.im = b.re * w.im + b.im * w.re;
            z[i + k].re        = a.re + t.re;
            z[i + k].im        = a.im + t.im;
            z[i + k + half].re = a.re - t.re;
            z[i + k + half].im = a.im - t.im;
         }
   }
}

/* The MDCT of N windowed samples into M coefficients:
 * X[k] = 2 sum x[n] cos(2 pi / N (n + 1/2 + N/4)(k + 1/2)) */
/* sample @k of the fold, (-c_r - d, a - b_r) of the window's quarters */
static float fold(const float *x, unsigned k)
{
   return k < M / 2 ? -x[3 * M / 2 - 1 - k] - x[3 * M / 2 + k]
                    :  x[k - M / 2]          - x[3 * M / 2 - 1 - k];
}

static void mdct(raac_encoder_t *e, const float *x, float *out)
{
   unsigned i;
   /* the DCT-IV of the fold by an M/2 point FFT, folded straight into
    * its input */
   for (i = 0; i < M / 2; i++)
   {
      float a = fold(x, 2 * i), b = fold(x, M - 1 - 2 * i);
      e->z[i].re = a * e->pre[i].re - b * e->pre[i].im;
      e->z[i].im = a * e->pre[i].im + b * e->pre[i].re;
   }
   fft(e, e->z);
   for (i = 0; i < M / 2; i++)
   {
      float re = e->z[i].re * e->post[i].re - e->z[i].im * e->post[i].im;
      float im = e->z[i].re * e->post[i].im + e->z[i].im * e->post[i].re;
      out[2 * i]         = 2.0f * re;
      out[M - 1 - 2 * i] = -2.0f * im;
   }
}

/* --------------------------------------------------------------------- */
/* Setup                                                                 */
/* --------------------------------------------------------------------- */

raac_encoder_t *raac_encoder_new(const raac_encoder_config_t *c)
{
   raac_encoder_t *e;
   unsigned i, bits, cutoff, per_ch;

   if (!c || (c->channels != 1 && c->channels != 2) || !c->bitrate)
      return NULL;
   for (i = 0; i < 13; i++)
      if (raac_sample_rates[i] == c->sample_rate)
         break;
   if (i == 13)
      return NULL;
   if (!(e = (raac_encoder_t*)calloc(1, sizeof(*e))))
      return NULL;
   e->cfg     = *c;
   e->sfi     = i;
   e->num_swb = raac_num_swb_1024[i];
   e->swb     = raac_swb_offset_1024[i];

   /* the bandwidth the rate allows a channel */
   per_ch = c->bitrate / c->channels;
   cutoff = per_ch >= 96000 ? 20000 : per_ch >= 64000 ? 16000
          : per_ch >= 48000 ? 13000 : per_ch >= 32000 ? 10000 : 7000;
   if (cutoff > c->sample_rate / 2)
      cutoff = c->sample_rate / 2;
   for (e->max_sfb = 0; e->max_sfb < e->num_swb; e->max_sfb++)
      if ((unsigned)e->swb[e->max_sfb] * c->sample_rate / (2 * M) >= cutoff)
         break;
   if (!e->max_sfb)
      e->max_sfb = 1;
   e->last_b = 60;

   for (i = 0; i < N; i++)
      e->window[i] = (float)sin(M_PI * (i + 0.5) / N);
   for (i = 0; i < M / 4; i++)
   {
      e->twiddle[i].re = (float)cos(-2.0 * M_PI * i / (M / 2));
      e->twiddle[i].im = (float)sin(-2.0 * M_PI * i / (M / 2));
   }
   for (i = 0; i < M / 2; i++)
   {
      unsigned r = 0, k;
      for (k = 0, bits = i; k < 9; k++, bits >>= 1)
         r = (r << 1) | (bits & 1);
      e->bitrev[i]  = (uint16_t)r;
      e->pre[i].re  = (float)cos(-M_PI * (i + 0.25) / M);
      e->pre[i].im  = (float)sin(-M_PI * (i + 0.25) / M);
      e->post[i].re = (float)cos(-M_PI * i / M);
      e->post[i].im = (float)sin(-M_PI * i / M);
   }
   return e;
}

void raac_encoder_free(raac_encoder_t *e)
{
   free(e);
}

size_t raac_encoder_asc(const raac_encoder_t *e, uint8_t *dst, size_t cap)
{
   if (!e || !dst || cap < 2)
      return 0;
   /* AAC LC (2), the rate's index, the channels; 1024-sample frames,
    * no core coder, no extension */
   dst[0] = (uint8_t)((2 << 3) | (e->sfi >> 1));
   dst[1] = (uint8_t)(((e->sfi & 1) << 7) | (e->cfg.channels << 3));
   return 2;
}

/* --------------------------------------------------------------------- */
/* Bits                                                                  */
/* --------------------------------------------------------------------- */

static void put_bits(raac_encoder_t *e, uint32_t v, unsigned n)
{
   while (n)
   {
      unsigned k = n > 24 ? 24 : n;
      uint32_t part = (v >> (n - k)) & ((1u << k) - 1);
      e->acc    = (e->acc << k) | part;
      e->nbits += k;
      n        -= k;
      while (e->nbits >= 8)
      {
         e->nbits -= 8;
         if (e->pos < e->cap)
            e->out[e->pos++] = (uint8_t)(e->acc >> e->nbits);
         else
            e->overflow = true;
      }
   }
}

/* --------------------------------------------------------------------- */
/* Quantisation and codebooks                                            */
/* --------------------------------------------------------------------- */

#define BIG 0x3FFFFFFF

static const uint8_t book_lav[12] = { 0, 1, 1, 2, 2, 4, 4, 7, 7, 12, 12, 16 };

/* A value's escape, for codebook 11: n ones, a zero, n + 4 bits */
static unsigned esc_bits(int v)
{
   unsigned n = 0;
   if (v < 16)
      return 0;
   while ((unsigned)v >= (32u << n))
      n++;
   return 2 * n + 5;
}

/* Bits (or BIG) for a band's values in a codebook; with @put, codes them */
static unsigned book_bits(raac_encoder_t *e, const int *q, unsigned w,
      unsigned cb, bool put)
{
   const uint16_t *code = raac_hcb_code[cb - 1];
   const uint8_t  *len  = raac_hcb_bits[cb - 1];
   unsigned        bits = 0, i;
   bool            sign = cb == 3 || cb == 4 || cb >= 7;

   if (cb <= 4)
   {
      for (i = 0; i < w; i += 4)
      {
         int a[4], k, idx = 0;
         for (k = 0; k < 4; k++)
         {
            a[k] = q[i + k];
            if (abs(a[k]) > book_lav[cb])
               return BIG;
            idx = idx * 3 + (sign ? abs(a[k]) : a[k] + 1);
         }
         bits += len[idx];
         if (put)
            put_bits(e, code[idx], len[idx]);
         if (sign)
            for (k = 0; k < 4; k++)
               if (a[k])
               {
                  bits++;
                  if (put)
                     put_bits(e, a[k] < 0, 1);
               }
      }
      return bits;
   }
   for (i = 0; i < w; i += 2)
   {
      int y = q[i], z = q[i + 1], idx, k, ay = abs(y), az = abs(z);
      if (cb <= 6)
      {
         if (ay > 4 || az > 4)
            return BIG;
         idx = 9 * (y + 4) + (z + 4);
      }
      else if (cb <= 10)
      {
         unsigned lav = cb <= 8 ? 8 : 13;
         if ((unsigned)ay >= lav || (unsigned)az >= lav)
            return BIG;
         idx = (int)lav * ay + az;
      }
      else
         idx = 17 * (ay > 16 ? 16 : ay) + (az > 16 ? 16 : az);
      bits += len[idx];
      if (put)
         put_bits(e, code[idx], len[idx]);
      if (sign)
         for (k = 0; k < 2; k++)
         {
            int v = k ? z : y;
            if (v)
            {
               bits++;
               if (put)
                  put_bits(e, v < 0, 1);
            }
         }
      if (cb == 11)
         for (k = 0; k < 2; k++)
         {
            int v = k ? az : ay;
            unsigned eb = esc_bits(v);
            bits += eb;
            if (put && eb)
            {
               unsigned n = (eb - 5) / 2;
               put_bits(e, (1u << (n + 1)) - 2, n + 1);   /* n ones, a zero */
               put_bits(e, (uint32_t)v - (16u << n), n + 4);
            }
         }
   }
   return bits;
}

/* The codebook that codes a band in the fewest bits; 0 if it is zero */
static unsigned pick_book(raac_encoder_t *e, const int *q, unsigned w, unsigned *bits)
{
   int      m = 0;
   unsigned i, first, best = 0, best_bits = BIG, cb;
   for (i = 0; i < w; i++)
      if (abs(q[i]) > m)
         m = abs(q[i]);
   if (!m)
   {
      *bits = 0;
      return 0;
   }
   first = m <= 1 ? 1 : m <= 2 ? 3 : m <= 4 ? 5 : m <= 7 ? 7 : m <= 12 ? 9 : 11;
   for (cb = first; cb <= 11 && cb < first + 4; cb++)
   {
      unsigned b = book_bits(e, q, w, cb, false);
      if (b < best_bits)
      {
         best_bits = b;
         best      = cb;
      }
   }
   *bits = best_bits;
   return best;
}

/* The channel quantised at B: scalefactors, values, codebooks; returns
 * the bits its section, scalefactor and spectral data take */
static unsigned quantise(raac_encoder_t *e, enc_channel_t *c, int B)
{
   unsigned b, bits = 0;
   int      prev = -1;

   for (b = 0; b < e->max_sfb; b++)
   {
      unsigned s0 = e->swb[b], w = e->swb[b + 1] - s0, i, bb;
      double   en = 0, mx = 0, gain;
      int      sf;
      for (i = 0; i < w; i++)
      {
         double v = fabs(c->spec[s0 + i]);
         en += v * v;
         if (v > mx)
            mx = v;
      }
      if (mx < 1.0)
      {
         for (i = 0; i < w; i++)
            c->q[s0 + i] = 0;
         c->cb[b] = 0;
         continue;
      }
      sf = (int)floor(B + 2.0 * log(sqrt(en / w)) / log(2.0) + 0.5);
      /* no finer than the frame's floor: what lies that far under its
       * loudest band, or under the quietest 16-bit signal, is not heard */
      if (sf < c->sf_floor)
         sf = c->sf_floor;
      /* no value past 8191 */
      {
         int lo = (int)ceil(100 + 16.0 / 3.0 * (0.75 * log(mx) / log(2.0) - log(8191.0) / log(2.0)));
         if (sf < lo)
            sf = lo;
      }
      if (prev >= 0)
      {
         if (sf < prev - 60)
            sf = prev - 60;
         if (sf > prev + 60)
            sf = prev + 60;
      }
      if (sf < 0)
         sf = 0;
      if (sf > 255)
         sf = 255;
      gain = pow(2.0, -0.1875 * (sf - 100));
      for (i = 0; i < w; i++)
      {
         float v = c->spec[s0 + i];
         int   l = (int)(pow(fabs(v), 0.75) * gain + 0.4054);
         if (l > 8191)
            l = 8191;
         c->q[s0 + i] = v < 0 ? -l : l;
      }
      c->cb[b] = (uint8_t)pick_book(e, c->q + s0, w, &bb);
      c->sf[b] = sf;
      if (!c->cb[b])
         continue;
      bits += bb;
      /* the scalefactor's delta */
      bits += raac_sf_bits[(prev < 0 ? 0 : sf - prev) + 60];
      prev = sf;
   }
   /* sections: a codebook, then its run in 5-bit pieces */
   for (b = 0; b < e->max_sfb; b++)
   {
      unsigned run = 1;
      while (b + 1 < e->max_sfb && c->cb[b + 1] == c->cb[b])
      {
         b++;
         run++;
      }
      bits += 4 + 5 * (run / 31 + 1);
   }
   return bits;
}

static void write_ics_info(raac_encoder_t *e)
{
   put_bits(e, 0, 1);                    /* ics_reserved_bit */
   put_bits(e, 0, 2);                    /* ONLY_LONG_SEQUENCE */
   put_bits(e, 0, 1);                    /* sine window */
   put_bits(e, e->max_sfb, 6);
   put_bits(e, 0, 1);                    /* no prediction */
}

/* individual_channel_stream(): global_gain, ics_info unless a channel
 * pair's common window gave it, sections, scalefactors, no pulse, TNS
 * or gain control, the spectrum */
static void write_ics(raac_encoder_t *e, enc_channel_t *c, bool info)
{
   unsigned b, global = 100;
   int      prev = -1;

   for (b = 0; b < e->max_sfb; b++)
      if (c->cb[b])
      {
         global = (unsigned)c->sf[b];
         break;
      }
   put_bits(e, global, 8);
   if (info)
      write_ics_info(e);

   for (b = 0; b < e->max_sfb; b++)
   {
      unsigned run = 1, cb = c->cb[b];
      while (b + 1 < e->max_sfb && c->cb[b + 1] == cb)
      {
         b++;
         run++;
      }
      put_bits(e, cb, 4);
      while (run >= 31)
      {
         put_bits(e, 31, 5);
         run -= 31;
      }
      put_bits(e, run, 5);
   }

   for (b = 0; b < e->max_sfb; b++)
   {
      int d;
      if (!c->cb[b])
         continue;
      d = prev < 0 ? 0 : c->sf[b] - prev;
      put_bits(e, raac_sf_code[d + 60], raac_sf_bits[d + 60]);
      prev = c->sf[b];
   }
   put_bits(e, 0, 3);                    /* pulse, tns, gain control */

   for (b = 0; b < e->max_sfb; b++)
      if (c->cb[b])
      {
         unsigned s0 = e->swb[b];
         book_bits(e, c->q + s0, e->swb[b + 1] - s0, c->cb[b], true);
      }
}

/* --------------------------------------------------------------------- */

static size_t encode(raac_encoder_t *e, const int16_t *s16, const float *f32,
      uint8_t *dst, size_t cap, bool adts)
{
   unsigned ch, i, n = e->cfg.channels;
   unsigned header = adts ? 7 : 0;
   int      target, share, lo, hi, best_b, limit;
   unsigned bits_fixed = n == 1 ? 3 + 4 + 11 + 8 + 3 + 3
                                : 3 + 4 + 1 + 11 + 2 + 2 * (8 + 3) + 3;

   if (!e || !dst)
      return 0;
   if (!s16 && !f32)
      e->flushed = true;

   for (ch = 0; ch < n; ch++)
   {
      enc_channel_t *c = &e->ch[ch];
      for (i = 0; i < M; i++)
      {
         float cur = s16 ? (float)s16[i * n + ch]
                   : f32 ? f32[i * n + ch] * 32768.0f : 0.0f;
         e->x[i]     = c->prev[i] * e->window[i];
         e->x[M + i] = cur * e->window[M + i];
         c->prev[i]  = cur;
      }
      mdct(e, e->x, c->spec);

      /* the floor: a step 60 dB under the loudest band's level, and at
       * least what a one-LSB sine comes to */
      {
         unsigned bb;
         double   peak = 0;
         for (bb = 0; bb < e->max_sfb; bb++)
         {
            unsigned s0 = e->swb[bb], w = e->swb[bb + 1] - s0, k;
            double   en = 0;
            for (k = 0; k < w; k++)
               en += (double)c->spec[s0 + k] * c->spec[s0 + k];
            if (en / w > peak)
               peak = en / w;
         }
         peak = sqrt(peak) * 0.001;             /* -60 dB */
         if (peak < 64.0)
            peak = 64.0;
         c->sf_floor = (int)floor(100 + 4.0 * log(peak) / log(2.0) + 0.5);
      }
   }

   /* the frame's bits: its share of the rate, the debt paid over eight
    * frames, within AAC's 6144 bits a channel */
   share  = (int)((uint64_t)e->cfg.bitrate * M / e->cfg.sample_rate);
   target = share - e->debt / 8;
   limit  = 6144 * (int)n - 8;
   if (target < share / 4)
      target = share / 4;
   if (target > limit)
      target = limit;

   /* the largest B (coarsest) is never wanted; search for the smallest
    * B whose bits fit */
   lo = -120;
   hi = 255;
   best_b = hi;
   {
      int b0 = e->last_b;
      /* bisection over B, starting where the last frame ended */
      while (lo <= hi)
      {
         int mid = (lo + hi) / 2, total = (int)bits_fixed;
         if (b0 >= lo && b0 <= hi)
         {
            mid = b0;
            b0  = -1000;
         }
         for (ch = 0; ch < n; ch++)
            total += (int)quantise(e, &e->ch[ch], mid);
         if (total <= target)
         {
            best_b = mid;
            hi = mid - 1;
         }
         else
            lo = mid + 1;
      }
   }
   e->last_b = best_b;
   for (ch = 0; ch < n; ch++)
      quantise(e, &e->ch[ch], best_b);

   e->out = dst;
   e->cap = cap;
   e->pos = 0;
   e->acc = 0;
   e->nbits = 0;
   e->overflow = false;
   if (adts)
      for (i = 0; i < 7; i++)
         put_bits(e, 0, 8);              /* filled in below */

   if (n == 1)
   {
      put_bits(e, 0, 3);                 /* ID_SCE */
      put_bits(e, 0, 4);
      write_ics(e, &e->ch[0], true);
   }
   else
   {
      put_bits(e, 1, 3);                 /* ID_CPE */
      put_bits(e, 0, 4);
      put_bits(e, 1, 1);                 /* common_window */
      write_ics_info(e);
      put_bits(e, 0, 2);                 /* no M/S */
      write_ics(e, &e->ch[0], false);
      write_ics(e, &e->ch[1], false);
   }
   put_bits(e, 7, 3);                    /* ID_END */
   if (e->nbits)
      put_bits(e, 0, 8 - e->nbits);
   if (e->overflow)
      return 0;

   if (adts)
   {
      size_t   len = e->pos;
      uint8_t *h   = dst;
      h[0] = 0xFF;
      h[1] = 0xF1;                       /* MPEG-4, no CRC */
      h[2] = (uint8_t)((1 << 6) | (e->sfi << 2) | (n >> 2));
      h[3] = (uint8_t)(((n & 3) << 6) | ((len >> 11) & 3));
      h[4] = (uint8_t)(len >> 3);
      h[5] = (uint8_t)(((len & 7) << 5) | 0x1F);
      h[6] = 0xFC;
   }

   e->debt += (int)((e->pos - header) * 8) - share;
   if (e->debt > 64 * share)
      e->debt = 64 * share;
   if (e->debt < -64 * share)
      e->debt = -64 * share;
   return e->pos;
}

size_t raac_encoder_encode_s16(raac_encoder_t *e, const int16_t *pcm,
      uint8_t *dst, size_t cap, bool adts)
{
   return encode(e, pcm, NULL, dst, cap, adts);
}

size_t raac_encoder_encode_f32(raac_encoder_t *e, const float *pcm,
      uint8_t *dst, size_t cap, bool adts)
{
   return encode(e, NULL, pcm, dst, cap, adts);
}

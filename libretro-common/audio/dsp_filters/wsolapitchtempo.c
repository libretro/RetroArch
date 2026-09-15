/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (wsolapitchtempo.c).
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

/* Pitch shift at unchanged tempo: a WSOLA duration change by the pitch
 * ratio, followed by a polyphase windowed-sinc resampler that restores
 * the duration. Output runs at the input rate, so the filter never
 * changes how much audio the frontend has to play. */

#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <retro_inline.h>
#include <libretro_dspfilter.h>

#include <audio/wsola_search.h>

#define WSOLA_CH      2u
#define WSOLA_PI      3.141592653589793238462643383279502884
#define WSOLA_TAPS    32u
#define WSOLA_HALF    (WSOLA_TAPS / 2u)
#define WSOLA_PHASES  1024u
/* Kaiser window for the resampler, ~72 dB stopband; the transition
 * width follows from it and WSOLA_TAPS, in units of Nyquist. */
#define WSOLA_KAISER_BETA 7.0
#define WSOLA_TRANSITION  0.29
#define WSOLA_BLOCK   8192u
#define WSOLA_SIZE_MAX ((size_t)-1)
/* Resampler phase index from a 32-bit position fraction. */
#define WSOLA_PHASE_SHIFT 22u
/* int16 lane: OLA and resampler history hold Q8 samples (1 LSB of s16
 * is 256); resampler taps are Q15. */
#define WSOLA_Q8_ONE  256
#define WSOLA_Q15_ONE 32768

/* A frame position: whole frames plus a 32-bit fraction. Scheduling in
 * integers keeps both lanes making identical segment decisions. */
struct wsola_pos
{
   uint64_t whole;
   uint32_t frac;
};

/* The float lane runs entirely in float and the int16 lane entirely in
 * integers; each keeps its own buffers and schedule, so neither ever
 * converts samples. Element types per lane:
 *              input    mono     ola/resamp  output   reference
 *   float      float    float    float       float    float
 *   int16      int16    int32    int32 (Q8)  int16    int32 */
struct wsola_lane
{
   uint64_t synth_start;
   uint64_t input_base, input_write;
   uint64_t ola_base, ola_read, ola_end;
   uint64_t resamp_base, resamp_write;
   struct wsola_pos next_analysis;
   struct wsola_pos resamp_pos;

   void *input;
   size_t input_cap;
   void *mono;
   size_t mono_cap;
   void *ola;
   size_t ola_cap;
   void *resamp;
   size_t resamp_cap;
   void *output;
   size_t output_cap;
   void *reference;

   unsigned output_frames;
   /* Output pacing: frames handed out last call, shortfall carried to
    * the next, and whether production has run far enough ahead. */
   unsigned out_read;
   unsigned out_owed;
   int out_started;
   int first;
   int failed;
};

struct wsola
{
   double pitch_ratio;
   struct wsola_lane lane[2];
   struct wsola_pos ha;
   struct wsola_pos step;
   float *table;
   int16_t *table_i;
   unsigned seq, overlap, search, hs;
   unsigned out_prime;
   int bypass;
   wsola_corr_func_t corr;
};

#define WSOLA_LANE_F 0
#define WSOLA_LANE_I 1
/* mono, ola, resamp and reference are 4-byte elements in both lanes. */
#define WSOLA_W_SZ       4u

static double wsola_clampd(double x, double lo, double hi)
{
   return x < lo ? lo : (x > hi ? hi : x);
}

static size_t wsola_next_pow2(size_t n)
{
   size_t p = 1;
   while (p < n && p <= (WSOLA_SIZE_MAX >> 1))
      p <<= 1;
   return p < n ? n : p;
}

/* Grows *ptr to hold need elements of elem bytes; cap counts elements. */
static int wsola_grow(void **ptr, size_t elem, size_t *cap, size_t need)
{
   void *p;
   size_t nc;
   if (need <= *cap)
      return 1;
   nc = wsola_next_pow2(need < 256 ? 256 : need);
   if (nc > WSOLA_SIZE_MAX / elem)
      return 0;
   if (!(p = realloc(*ptr, nc * elem)))
      return 0;
   *ptr = p;
   *cap = nc;
   return 1;
}

static unsigned wsola_align_up(unsigned v, unsigned a)
{
   return ((v + a - 1u) / a) * a;
}

static struct wsola_pos wsola_pos_from(double v)
{
   struct wsola_pos p;
   double w = floor(v);
   double f = floor((v - w) * 4294967296.0 + 0.5);
   if (f >= 4294967296.0)
   {
      w += 1.0;
      f  = 0.0;
   }
   p.whole = (uint64_t)w;
   p.frac  = (uint32_t)f;
   return p;
}

static INLINE void wsola_pos_add(struct wsola_pos *p,
      const struct wsola_pos *step)
{
   uint32_t f = p->frac + step->frac;
   p->whole  += step->whole + (f < p->frac ? 1u : 0u);
   p->frac    = f;
}

static INLINE uint64_t wsola_pos_round(const struct wsola_pos *p)
{
   return p->whole + (p->frac >> 31);
}

static INLINE int64_t wsola_rsh(int64_t v, unsigned s)
{
   int64_t h = (int64_t)1 << (s - 1);
   return (v >= 0) ? ((v + h) >> s) : -(((-v) + h) >> s);
}

static INLINE int16_t wsola_sat16(int64_t v)
{
   if (v > 32767)
      return 32767;
   if (v < -32768)
      return -32768;
   return (int16_t)v;
}

/* Per-lane sample work. The float lane's functions touch only floats,
 * the int16 lane's only integers; the scheduling code below is shared
 * and reaches them through this table. */
struct wsola_lane_ops
{
   void (*append_input)(struct wsola_lane *L, size_t at,
         const void *src, unsigned frames);
   int (*process_resamp)(const struct wsola *d, struct wsola_lane *L);
   void (*build_reference)(const struct wsola *d, struct wsola_lane *L,
         uint64_t output_start);
   uint64_t (*best_candidate)(const struct wsola *d, struct wsola_lane *L,
         uint64_t lo, uint64_t hi, uint64_t best);
   void (*copy_segment)(const struct wsola *d, struct wsola_lane *L,
         size_t inoff, size_t outoff, unsigned faded);
   size_t in_sz;
   size_t out_sz;
};

static void wsola_compact_input(const struct wsola *d, struct wsola_lane *L,
      const struct wsola_lane_ops *ops)
{
   uint64_t prediction, keep, discard;
   size_t remain;
   if (L->first)
      return;
   prediction = L->next_analysis.whole;
   keep       = prediction > d->search + 8u
      ? prediction - d->search - 8u : 0u;
   if (keep < L->input_base)
      keep = L->input_base;
   if (keep > L->input_write)
      keep = L->input_write;
   discard = keep - L->input_base;
   if (discard < (uint64_t)d->seq * 8u)
      return;
   remain = (size_t)(L->input_write - keep);
   if (remain)
   {
      memmove(L->input, (uint8_t*)L->input
            + (size_t)discard * WSOLA_CH * ops->in_sz,
            remain * WSOLA_CH * ops->in_sz);
      memmove(L->mono, (uint8_t*)L->mono + (size_t)discard * WSOLA_W_SZ,
            remain * WSOLA_W_SZ);
   }
   L->input_base = keep;
}

static int wsola_append_input(const struct wsola *d, struct wsola_lane *L,
      const struct wsola_lane_ops *ops, const void *src, unsigned frames)
{
   size_t existing, need;
   wsola_compact_input(d, L, ops);
   existing = (size_t)(L->input_write - L->input_base);
   need     = existing + frames;
   if (!wsola_grow(&L->input, ops->in_sz, &L->input_cap, need * WSOLA_CH))
      return 0;
   if (!wsola_grow(&L->mono, WSOLA_W_SZ, &L->mono_cap, need))
      return 0;
   ops->append_input(L, existing, src, frames);
   L->input_write += frames;
   return 1;
}

static void wsola_compact_ola(const struct wsola *d, struct wsola_lane *L)
{
   size_t remain;
   uint64_t used = L->ola_read - L->ola_base;
   if (used < (uint64_t)d->seq * 8u)
      return;
   remain = (size_t)(L->ola_end - L->ola_read);
   if (remain)
      memmove(L->ola, (uint8_t*)L->ola
            + (size_t)used * WSOLA_CH * WSOLA_W_SZ,
            remain * WSOLA_CH * WSOLA_W_SZ);
   L->ola_base = L->ola_read;
   L->ola_end  = L->ola_base + remain;
}

static int wsola_ensure_ola(const struct wsola *d, struct wsola_lane *L,
      uint64_t end_abs)
{
   size_t oldf, needf;
   if (end_abs <= L->ola_end)
      return 1;
   wsola_compact_ola(d, L);
   oldf  = (size_t)(L->ola_end - L->ola_base);
   needf = (size_t)(end_abs - L->ola_base);
   if (!wsola_grow(&L->ola, WSOLA_W_SZ, &L->ola_cap, needf * WSOLA_CH))
      return 0;
   /* All-zero bytes are 0.0f and 0 alike. */
   memset((uint8_t*)L->ola + oldf * WSOLA_CH * WSOLA_W_SZ, 0,
         (needf - oldf) * WSOLA_CH * WSOLA_W_SZ);
   L->ola_end = end_abs;
   return 1;
}

static void wsola_compact_resamp(struct wsola_lane *L)
{
   uint64_t discard;
   size_t remain;
   uint64_t center = L->resamp_pos.whole;
   uint64_t keep   = center > WSOLA_HALF + 8u
      ? center - WSOLA_HALF - 8u : 0u;
   if (keep < L->resamp_base)
      keep = L->resamp_base;
   if (keep > L->resamp_write)
      keep = L->resamp_write;
   discard = keep - L->resamp_base;
   if (discard < 8192u)
      return;
   remain = (size_t)(L->resamp_write - keep);
   if (remain)
      memmove(L->resamp, (uint8_t*)L->resamp
            + (size_t)discard * WSOLA_CH * WSOLA_W_SZ,
            remain * WSOLA_CH * WSOLA_W_SZ);
   L->resamp_base = keep;
}

/* Appends frames of OLA output (or silence when src is NULL) to the
 * resampler history; both lanes store 4-byte elements there. */
static int wsola_append_resamp(struct wsola_lane *L, const void *src,
      unsigned frames)
{
   size_t existing;
   uint8_t *dst;
   wsola_compact_resamp(L);
   existing = (size_t)(L->resamp_write - L->resamp_base);
   if (!wsola_grow(&L->resamp, WSOLA_W_SZ, &L->resamp_cap,
            (existing + frames) * WSOLA_CH))
      return 0;
   dst = (uint8_t*)L->resamp + existing * WSOLA_CH * WSOLA_W_SZ;
   if (src)
      memcpy(dst, src, (size_t)frames * WSOLA_CH * WSOLA_W_SZ);
   else
      memset(dst, 0, (size_t)frames * WSOLA_CH * WSOLA_W_SZ);
   L->resamp_write += frames;
   return 1;
}

/* Output frame at resamp_pos reads input frames
 * floor(pos) - (WSOLA_HALF - 1) .. floor(pos) + WSOLA_HALF. */
static INLINE int wsola_resamp_can_output(const struct wsola_lane *L)
{
   uint64_t c = L->resamp_pos.whole;
   return c >= WSOLA_HALF - 1u && c + WSOLA_HALF < L->resamp_write;
}

static INLINE size_t wsola_resamp_first(const struct wsola_lane *L)
{
   return (size_t)((L->resamp_pos.whole - (WSOLA_HALF - 1u))
         - L->resamp_base) * WSOLA_CH;
}

static int wsola_feed_until(const struct wsola *d, struct wsola_lane *L,
      const struct wsola_lane_ops *ops, uint64_t safe)
{
   uint64_t end = safe < L->ola_end ? safe : L->ola_end;
   while (L->ola_read < end)
   {
      size_t off;
      unsigned frames = (unsigned)(end - L->ola_read);
      if (frames > WSOLA_BLOCK)
         frames = WSOLA_BLOCK;
      off = (size_t)(L->ola_read - L->ola_base);
      if (     !wsola_append_resamp(L, (uint8_t*)L->ola
                  + off * WSOLA_CH * WSOLA_W_SZ, frames)
            || !ops->process_resamp(d, L))
         return 0;
      L->ola_read += frames;
   }
   wsola_compact_ola(d, L);
   return 1;
}

/* ---- float lane ---- */

static void wsola_append_input_f(struct wsola_lane *L, size_t at,
      const void *src, unsigned frames)
{
   unsigned f;
   const float *s = (const float*)src;
   float *mono    = (float*)L->mono + at;
   memcpy((float*)L->input + at * WSOLA_CH, s,
         (size_t)frames * WSOLA_CH * sizeof(float));
   for (f = 0; f < frames; ++f)
      mono[f] = 0.5f * (s[f * 2u] + s[f * 2u + 1u]);
}

static int wsola_process_resamp_f(const struct wsola *d,
      struct wsola_lane *L)
{
   while (wsola_resamp_can_output(L))
   {
      float *dst;
      unsigned made = 0;
      if (!wsola_grow(&L->output, sizeof(float), &L->output_cap,
               ((size_t)L->output_frames + WSOLA_BLOCK) * WSOLA_CH))
         return 0;
      dst = (float*)L->output + (size_t)L->output_frames * WSOLA_CH;
      while (made < WSOLA_BLOCK && wsola_resamp_can_output(L))
      {
         unsigned t;
         const float *coef = d->table + (size_t)
            (L->resamp_pos.frac >> WSOLA_PHASE_SHIFT) * WSOLA_TAPS;
         const float *src  = (const float*)L->resamp + wsola_resamp_first(L);
         float sum_l       = 0.0f;
         float sum_r       = 0.0f;
         for (t = 0; t < WSOLA_TAPS; ++t)
         {
            sum_l += src[t * 2u]      * coef[t];
            sum_r += src[t * 2u + 1u] * coef[t];
         }
         dst[made * 2u]      = sum_l;
         dst[made * 2u + 1u] = sum_r;
         ++made;
         wsola_pos_add(&L->resamp_pos, &d->step);
      }
      L->output_frames += made;
      wsola_compact_resamp(L);
      if (!made)
         break;
   }
   return 1;
}

static void wsola_build_reference_f(const struct wsola *d,
      struct wsola_lane *L, uint64_t output_start)
{
   unsigned f;
   const float *ola = (const float*)L->ola
      + (size_t)(output_start - L->ola_base) * WSOLA_CH;
   float *ref       = (float*)L->reference;
   for (f = 0; f < d->overlap; ++f)
      ref[f] = 0.5f * (ola[f * 2u] + ola[f * 2u + 1u]);
}

static uint64_t wsola_best_candidate_f(const struct wsola *d,
      struct wsola_lane *L, uint64_t lo, uint64_t hi, uint64_t best)
{
   unsigned i;
   uint64_t c, rlo, rhi;
   double ae         = 0.0;
   double best_score = -2.0;
   const float *ref  = (const float*)L->reference;
   const float *mono = (const float*)L->mono;
   for (i = 0; i < d->overlap; ++i)
   {
      double x = ref[i];
      ae      += x * x;
   }
   if (ae <= 1.0e-20)
      return best;

   /* Coarse pass on an 8-frame grid, then refine around the winner. */
   c = lo + ((8u - (lo & 7u)) & 7u);
   while (c <= hi)
   {
      double s = d->corr(ref, mono + (size_t)(c - L->input_base),
            d->overlap, ae);
      if (s > best_score)
      {
         best_score = s;
         best       = c;
      }
      if (hi - c < 8u)
         break;
      c += 8u;
   }

   rlo = best > 8u ? best - 8u : lo;
   rhi = best + 8u;
   if (rlo < lo)
      rlo = lo;
   if (rhi > hi)
      rhi = hi;
   for (c = rlo; c <= rhi; ++c)
   {
      double s = d->corr(ref, mono + (size_t)(c - L->input_base),
            d->overlap, ae);
      if (s > best_score)
      {
         best_score = s;
         best       = c;
      }
      if (c == rhi)
         break;
   }
   return best;
}

/* Copies seq frames of input into the OLA buffer, cross-fading the first
 * faded frames against what is already there. */
static void wsola_copy_segment_f(const struct wsola *d, struct wsola_lane *L,
      size_t inoff, size_t outoff, unsigned faded)
{
   unsigned f;
   const float *src = (const float*)L->input + inoff;
   float *dst       = (float*)L->ola + outoff;
   for (f = 0; f < faded; ++f)
   {
      float fi = (float)(f + 1u) / (float)(d->overlap + 1u);
      float fo = 1.0f - fi;
      dst[f * 2u]      = dst[f * 2u]      * fo + src[f * 2u]      * fi;
      dst[f * 2u + 1u] = dst[f * 2u + 1u] * fo + src[f * 2u + 1u] * fi;
   }
   memcpy(dst + (size_t)faded * WSOLA_CH, src + (size_t)faded * WSOLA_CH,
         (size_t)(d->seq - faded) * WSOLA_CH * sizeof(float));
}

/* ---- int16 lane ---- */

static void wsola_append_input_i(struct wsola_lane *L, size_t at,
      const void *src, unsigned frames)
{
   unsigned f;
   const int16_t *s = (const int16_t*)src;
   int32_t *mono    = (int32_t*)L->mono + at;
   memcpy((int16_t*)L->input + at * WSOLA_CH, s,
         (size_t)frames * WSOLA_CH * sizeof(int16_t));
   /* The mono sum is kept unhalved, so it is exact. */
   for (f = 0; f < frames; ++f)
      mono[f] = (int32_t)s[f * 2u] + s[f * 2u + 1u];
}

static int wsola_process_resamp_i(const struct wsola *d,
      struct wsola_lane *L)
{
   while (wsola_resamp_can_output(L))
   {
      int16_t *dst;
      unsigned made = 0;
      if (!wsola_grow(&L->output, sizeof(int16_t), &L->output_cap,
               ((size_t)L->output_frames + WSOLA_BLOCK) * WSOLA_CH))
         return 0;
      dst = (int16_t*)L->output + (size_t)L->output_frames * WSOLA_CH;
      while (made < WSOLA_BLOCK && wsola_resamp_can_output(L))
      {
         unsigned t;
         const int16_t *coef = d->table_i + (size_t)
            (L->resamp_pos.frac >> WSOLA_PHASE_SHIFT) * WSOLA_TAPS;
         const int32_t *src  = (const int32_t*)L->resamp
            + wsola_resamp_first(L);
         int64_t sum_l       = 0;
         int64_t sum_r       = 0;
         for (t = 0; t < WSOLA_TAPS; ++t)
         {
            sum_l += (int64_t)src[t * 2u]      * coef[t];
            sum_r += (int64_t)src[t * 2u + 1u] * coef[t];
         }
         /* Q8 samples x Q15 taps -> s16. */
         dst[made * 2u]      = wsola_sat16(wsola_rsh(sum_l, 23));
         dst[made * 2u + 1u] = wsola_sat16(wsola_rsh(sum_r, 23));
         ++made;
         wsola_pos_add(&L->resamp_pos, &d->step);
      }
      L->output_frames += made;
      wsola_compact_resamp(L);
      if (!made)
         break;
   }
   return 1;
}

static void wsola_build_reference_i(const struct wsola *d,
      struct wsola_lane *L, uint64_t output_start)
{
   unsigned f;
   const int32_t *ola = (const int32_t*)L->ola
      + (size_t)(output_start - L->ola_base) * WSOLA_CH;
   int32_t *ref       = (int32_t*)L->reference;
   /* Q8 left + right, back to the scale of the unhalved mono sum. */
   for (f = 0; f < d->overlap; ++f)
      ref[f] = (int32_t)wsola_rsh((int64_t)ola[f * 2u] + ola[f * 2u + 1u], 8);
}

static uint64_t wsola_best_candidate_i(const struct wsola *d,
      struct wsola_lane *L, uint64_t lo, uint64_t hi, uint64_t best)
{
   unsigned i;
   uint64_t c, rlo, rhi;
   int64_t best_score  = 0;
   int have            = 0;
   const int32_t *ref  = (const int32_t*)L->reference;
   const int32_t *mono = (const int32_t*)L->mono;
   for (i = 0; i < d->overlap; ++i)
      if (ref[i])
         break;
   if (i == d->overlap)
      return best;

   c = lo + ((8u - (lo & 7u)) & 7u);
   while (c <= hi)
   {
      int64_t s = wsola_corr_i(ref, mono + (size_t)(c - L->input_base),
            d->overlap);
      if (!have || s > best_score)
      {
         best_score = s;
         best       = c;
         have       = 1;
      }
      if (hi - c < 8u)
         break;
      c += 8u;
   }

   rlo = best > 8u ? best - 8u : lo;
   rhi = best + 8u;
   if (rlo < lo)
      rlo = lo;
   if (rhi > hi)
      rhi = hi;
   for (c = rlo; c <= rhi; ++c)
   {
      int64_t s = wsola_corr_i(ref, mono + (size_t)(c - L->input_base),
            d->overlap);
      if (!have || s > best_score)
      {
         best_score = s;
         best       = c;
         have       = 1;
      }
      if (c == rhi)
         break;
   }
   return best;
}

static void wsola_copy_segment_i(const struct wsola *d, struct wsola_lane *L,
      size_t inoff, size_t outoff, unsigned faded)
{
   unsigned f;
   size_t i, rest;
   const int16_t *src = (const int16_t*)L->input + inoff;
   int32_t *dst       = (int32_t*)L->ola + outoff;
   for (f = 0; f < faded; ++f)
   {
      /* Q15 cross-fade weights. */
      int32_t fi = (int32_t)(((int64_t)(f + 1u) * WSOLA_Q15_ONE
               + (d->overlap + 1u) / 2u) / (d->overlap + 1u));
      int32_t fo = WSOLA_Q15_ONE - fi;
      dst[f * 2u]      = (int32_t)wsola_rsh((int64_t)dst[f * 2u] * fo
            + (int64_t)src[f * 2u] * WSOLA_Q8_ONE * fi, 15);
      dst[f * 2u + 1u] = (int32_t)wsola_rsh((int64_t)dst[f * 2u + 1u] * fo
            + (int64_t)src[f * 2u + 1u] * WSOLA_Q8_ONE * fi, 15);
   }
   rest = (size_t)(d->seq - faded) * WSOLA_CH;
   src += (size_t)faded * WSOLA_CH;
   dst += (size_t)faded * WSOLA_CH;
   for (i = 0; i < rest; i++)
      dst[i] = (int32_t)src[i] * WSOLA_Q8_ONE;
}

static const struct wsola_lane_ops wsola_ops[2] = {
   {
      wsola_append_input_f, wsola_process_resamp_f,
      wsola_build_reference_f, wsola_best_candidate_f,
      wsola_copy_segment_f,
      sizeof(float), sizeof(float)
   },
   {
      wsola_append_input_i, wsola_process_resamp_i,
      wsola_build_reference_i, wsola_best_candidate_i,
      wsola_copy_segment_i,
      sizeof(int16_t), sizeof(int16_t)
   }
};

/* ---- shared scheduling ---- */

static int wsola_run(struct wsola *d, struct wsola_lane *L,
      const struct wsola_lane_ops *ops)
{
   for (;;)
   {
      uint64_t prediction, outstart, maxcand, cand, lo, hi;
      if (L->first)
      {
         if (L->input_write < d->seq)
            return 1;
         if (!wsola_ensure_ola(d, L, d->seq))
            return 0;
         ops->copy_segment(d, L, 0, 0, 0);
         L->synth_start   = 0;
         L->next_analysis = d->ha;
         L->first         = 0;
         if (!wsola_feed_until(d, L, ops, d->hs))
            return 0;
         continue;
      }
      prediction = wsola_pos_round(&L->next_analysis);
      if (prediction + d->search + d->seq > L->input_write)
         return 1;
      outstart = L->synth_start + d->hs;
      ops->build_reference(d, L, outstart);
      maxcand  = L->input_write - d->seq;
      lo       = prediction > d->search ? prediction - d->search : 0u;
      hi       = prediction + d->search;
      if (lo < L->input_base)
         lo = L->input_base;
      if (hi > maxcand)
         hi = maxcand;
      if (hi < lo)
         cand = lo;
      else
         cand = ops->best_candidate(d, L, lo, hi,
               prediction < lo ? lo : (prediction > hi ? hi : prediction));
      if (!wsola_ensure_ola(d, L, outstart + d->seq))
         return 0;
      ops->copy_segment(d, L, (size_t)(cand - L->input_base) * WSOLA_CH,
            (size_t)(outstart - L->ola_base) * WSOLA_CH, d->overlap);
      L->synth_start = outstart;
      wsola_pos_add(&L->next_analysis, &d->ha);
      if (!wsola_feed_until(d, L, ops, outstart + d->hs))
         return 0;
      wsola_compact_input(d, L, ops);
   }
}

/* Runs one call's input through a lane. Returns 0 if the lane failed
 * (the caller then passes its input through), else sets the output. */
static int wsola_step(struct wsola *d, int li, const void *src,
      unsigned frames, void **out, unsigned *out_frames)
{
   unsigned want, avail;
   struct wsola_lane *L             = &d->lane[li];
   const struct wsola_lane_ops *ops = &wsola_ops[li];
   size_t osz                       = ops->out_sz;

   /* What the previous call handed out has been consumed by now. */
   if (L->out_read)
   {
      avail = L->output_frames - L->out_read;
      if (avail)
         memmove(L->output, (uint8_t*)L->output
               + (size_t)L->out_read * WSOLA_CH * osz,
               (size_t)avail * WSOLA_CH * osz);
      L->output_frames = avail;
      L->out_read      = 0;
   }

   if (     !wsola_append_input(d, L, ops, src, frames)
         || !wsola_run(d, L, ops))
   {
      L->failed = 1;
      return 0;
   }

   want        = frames + L->out_owed;
   L->out_owed = 0;
   avail       = L->output_frames;

   if (!L->out_started)
   {
      if (avail < want + d->out_prime)
      {
         /* Still priming: emit silence at the nominal rate, from the
          * space past the pending output. */
         if (!wsola_grow(&L->output, osz, &L->output_cap,
                  ((size_t)avail + want) * WSOLA_CH))
         {
            L->failed = 1;
            return 0;
         }
         memset((uint8_t*)L->output + (size_t)avail * WSOLA_CH * osz, 0,
               (size_t)want * WSOLA_CH * osz);
         *out        = (uint8_t*)L->output + (size_t)avail * WSOLA_CH * osz;
         *out_frames = want;
         return 1;
      }
      L->out_started = 1;
   }

   /* A shortfall is carried and made up once production catches up. */
   if (want > avail)
   {
      L->out_owed = want - avail;
      want        = avail;
   }
   *out        = L->output;
   *out_frames = want;
   L->out_read = want;
   return 1;
}

static double wsola_bessel_i0(double x)
{
   unsigned k;
   double sum  = 1.0;
   double term = 1.0;
   double q    = x * x * 0.25;
   for (k = 1; k < 64; k++)
   {
      term *= q / ((double)k * (double)k);
      sum  += term;
      if (term < sum * 1.0e-16)
         break;
   }
   return sum;
}

static int wsola_prepare_resampler(struct wsola *d)
{
   unsigned ph, t, lane;
   double onyq, cutoff, inv_i0;
   double c[WSOLA_TAPS];
   if (!(d->table = (float*)malloc(
               (size_t)WSOLA_PHASES * WSOLA_TAPS * sizeof(float))))
      return 0;
   if (!(d->table_i = (int16_t*)malloc(
               (size_t)WSOLA_PHASES * WSOLA_TAPS * sizeof(int16_t))))
      return 0;
   /* Kaiser-windowed sinc with the transition band just below the lower
    * of the two Nyquist rates, so what would alias (pitch up) or image
    * (pitch down) falls in the stopband. */
   onyq   = d->pitch_ratio > 1.0 ? 1.0 / d->pitch_ratio : 1.0;
   cutoff = onyq - WSOLA_TRANSITION * 0.5;
   inv_i0 = 1.0 / wsola_bessel_i0(WSOLA_KAISER_BETA);
   for (ph = 0; ph < WSOLA_PHASES; ++ph)
   {
      float *tf   = d->table   + (size_t)ph * WSOLA_TAPS;
      int16_t *ti = d->table_i + (size_t)ph * WSOLA_TAPS;
      double frac = (double)ph / WSOLA_PHASES;
      double sum  = 0.0;
      int32_t isum = 0;
      unsigned peak = 0;
      for (t = 0; t < WSOLA_TAPS; ++t)
      {
         double dist = (double)((int)t - (int)(WSOLA_HALF - 1u)) - frac;
         double norm = dist / WSOLA_HALF;
         double win  = fabs(norm) < 1.0
            ? wsola_bessel_i0(WSOLA_KAISER_BETA * sqrt(1.0 - norm * norm))
               * inv_i0
            : 0.0;
         double x    = cutoff * dist;
         double sinc = fabs(x) < 1.0e-12
            ? 1.0 : sin(WSOLA_PI * x) / (WSOLA_PI * x);
         c[t]        = cutoff * sinc * win;
         tf[t]       = (float)c[t];
         sum        += c[t];
      }
      if (fabs(sum) > 1.0e-15)
      {
         float inv = (float)(1.0 / sum);
         for (t = 0; t < WSOLA_TAPS; ++t)
            tf[t] *= inv;
      }
      else
         sum = 1.0;
      /* Q15 taps whose sum is exactly unity, so DC passes unchanged. */
      for (t = 0; t < WSOLA_TAPS; ++t)
      {
         ti[t] = (int16_t)floor(c[t] / sum * WSOLA_Q15_ONE + 0.5);
         isum += ti[t];
         if (c[t] > c[peak])
            peak = t;
      }
      ti[peak] = (int16_t)(ti[peak] + (WSOLA_Q15_ONE - isum));
   }
   for (lane = 0; lane < 2; lane++)
   {
      struct wsola_lane *L = &d->lane[lane];
      if (!wsola_append_resamp(L, NULL, WSOLA_HALF - 1u))
         return 0;
      L->resamp_pos.whole = WSOLA_HALF - 1u;
      L->resamp_pos.frac  = 0;
   }
   return 1;
}

static void wsola_free(void *opaque)
{
   unsigned lane;
   struct wsola *d = (struct wsola*)opaque;
   if (!d)
      return;
   for (lane = 0; lane < 2; lane++)
   {
      struct wsola_lane *L = &d->lane[lane];
      free(L->input);
      free(L->mono);
      free(L->ola);
      free(L->resamp);
      free(L->output);
      free(L->reference);
   }
   free(d->table);
   free(d->table_i);
   free(d);
}

static void *wsola_init_common(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata,
      enum wsola_simd simd)
{
   unsigned rate, lane;
   float pitch;
   double pitch_st;
   struct wsola *d;

   if (!info || info->input_rate <= 0.0f)
      return NULL;
   if (!(d = (struct wsola*)calloc(1, sizeof(*d))))
      return NULL;

   config->get_float(userdata, "pitch", &pitch, 0.0f);

   pitch_st          = wsola_clampd(pitch, -12.0, 12.0);
   d->pitch_ratio    = pow(2.0, pitch_st / 12.0);
   d->bypass         = fabs(d->pitch_ratio - 1.0) < 1.0e-9;
   d->corr           = wsola_corr_get(simd);

   /* 40 ms segments, 8 ms cross-fade, +/-12 ms search, 8-frame aligned. */
   rate       = (unsigned)(info->input_rate + 0.5f);
   d->seq     = wsola_align_up((unsigned)(rate * 0.040 + 0.5), 8u);
   if (d->seq < 64u)
      d->seq = 64u;
   d->overlap = wsola_align_up((unsigned)(rate * 0.008 + 0.5), 8u);
   if (d->overlap < 32u)
      d->overlap = 32u;
   if (d->overlap >= d->seq)
      d->overlap = d->seq / 4u;
   d->search  = wsola_align_up((unsigned)(rate * 0.012 + 0.5), 8u);
   if (d->search < 16u)
      d->search = 16u;
   d->hs      = d->seq - d->overlap;
   d->ha      = wsola_pos_from((double)d->hs / d->pitch_ratio);
   d->step    = wsola_pos_from(d->pitch_ratio);

   /* Output lands one synthesis hop, as resampled, at a time; with that
    * much held back, every call can emit as many frames as it takes. */
   d->out_prime = 64u + (unsigned)ceil((double)d->hs / d->pitch_ratio);

   if (d->bypass)
      return d;

   for (lane = 0; lane < 2; lane++)
   {
      d->lane[lane].first = 1;
      if (!(d->lane[lane].reference = malloc((size_t)d->overlap * WSOLA_W_SZ)))
      {
         wsola_free(d);
         return NULL;
      }
   }
   if (!wsola_prepare_resampler(d))
   {
      wsola_free(d);
      return NULL;
   }
   return d;
}

static void *wsola_init_scalar(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   return wsola_init_common(info, config, userdata, WSOLA_SIMD_SCALAR);
}

#if WSOLA_HAVE_SSE2
static void *wsola_init_sse2(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   return wsola_init_common(info, config, userdata, WSOLA_SIMD_SSE2);
}
#endif

#if WSOLA_HAVE_NEON
static void *wsola_init_neon(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   return wsola_init_common(info, config, userdata, WSOLA_SIMD_NEON);
}
#endif

static void wsola_process(void *opaque, struct dspfilter_output *out,
      const struct dspfilter_input *in)
{
   void *o;
   unsigned frames;
   struct wsola *d = (struct wsola*)opaque;

   /* The unprocessed input is the fallback for anything that stops the
    * filter; the output pointer is never NULL. */
   out->samples    = in->samples;
   out->frames     = in->frames;
   if (d->bypass || d->lane[WSOLA_LANE_F].failed || !in->frames)
      return;
   if (wsola_step(d, WSOLA_LANE_F, in->samples, in->frames, &o, &frames))
   {
      out->samples = (float*)o;
      out->frames  = frames;
   }
}

static void wsola_process_i16(void *opaque,
      struct dspfilter_output_i16 *out,
      const struct dspfilter_input_i16 *in)
{
   void *o;
   unsigned frames;
   struct wsola *d = (struct wsola*)opaque;

   out->samples    = in->samples;
   out->frames     = in->frames;
   if (d->bypass || d->lane[WSOLA_LANE_I].failed || !in->frames)
      return;
   if (wsola_step(d, WSOLA_LANE_I, in->samples, in->frames, &o, &frames))
   {
      out->samples = (int16_t*)o;
      out->frames  = frames;
   }
}

static const struct dspfilter_implementation wsola_plug_scalar = {
   wsola_init_scalar,
   wsola_process,
   wsola_free,

   DSPFILTER_API_VERSION,
   "WSOLA Pitch Shift",
   "wsolapitchtempo",

   wsola_process_i16,
};

#if WSOLA_HAVE_SSE2
static const struct dspfilter_implementation wsola_plug_sse2 = {
   wsola_init_sse2,
   wsola_process,
   wsola_free,

   DSPFILTER_API_VERSION,
   "WSOLA Pitch Shift (SSE2)",
   "wsolapitchtempo",

   wsola_process_i16,
};
#endif

#if WSOLA_HAVE_NEON
static const struct dspfilter_implementation wsola_plug_neon = {
   wsola_init_neon,
   wsola_process,
   wsola_free,

   DSPFILTER_API_VERSION,
   "WSOLA Pitch Shift (NEON)",
   "wsolapitchtempo",

   wsola_process_i16,
};
#endif

#ifdef HAVE_FILTERS_BUILTIN
#define dspfilter_get_implementation wsolapitchtempo_dspfilter_get_implementation
#endif

const struct dspfilter_implementation *dspfilter_get_implementation(dspfilter_simd_mask_t mask)
{
#if WSOLA_HAVE_NEON
   if (mask & DSPFILTER_SIMD_NEON)
      return &wsola_plug_neon;
#endif
#if WSOLA_HAVE_SSE2
   if (mask & DSPFILTER_SIMD_SSE2)
      return &wsola_plug_sse2;
#endif
   (void)mask;
   return &wsola_plug_scalar;
}

#undef dspfilter_get_implementation

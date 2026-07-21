/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include <file/file_path.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <retro_miscellaneous.h>

#ifdef WIIU
#include <wiiu/os.h>
#endif

#include "../font_driver.h"

/* ====================================================================
 * Cleanroom TrueType glyph loader / rasterizer.
 *
 * Implements, from the OpenType/TrueType specification, exactly the
 * subset this font renderer needs:
 *   - sfnt / TTC directory parsing with full bounds checking
 *   - cmap formats 0, 4, 6, 12 (codepoint -> glyph index)
 *   - hmtx / hhea / head / maxp metrics
 *   - glyf outlines (simple + composite), quadratic flattening, and an
 *     exact-coverage anti-aliased rasterizer (signed-area delta
 *     accumulation + per-row prefix sum, SSE2/NEON resolve kernels)
 *
 * Coverage is accumulated in float and quantized only in the final
 * per-row resolve step, so a future higher-bit-depth output path
 * (e.g. 10-bit HDR) only needs an alternate resolve routine.
 *
 * CFF ('OTTO') fonts are rejected at init (no glyf/loca), matching the
 * capability of the stb_truetype version this replaces; the font
 * driver falls back to the next backend.
 * ==================================================================== */

/* Max composite recursion; TrueType nesting deeper than this is
 * either malicious or broken. */
#define RTT_MAX_COMPOSITE_DEPTH 6

typedef struct rtt_font
{
   const uint8_t *data;
   size_t size;

   uint32_t glyf;
   uint32_t loca;
   uint32_t head;
   uint32_t hhea;
   uint32_t hmtx;
   uint32_t cmap_sub;

   uint32_t glyf_len;
   uint32_t loca_len;
   uint32_t hmtx_len;
   uint32_t cmap_sub_len;

   int num_glyphs;
   int num_hmetrics;
   int units_per_em;
   int loca_long;
   int cmap_format;
} rtt_font_t;

/* ------------------------------------------------------------------ */
/* Bounds-checked big-endian readers. Out-of-range reads yield 0.     */
/* ------------------------------------------------------------------ */

static uint8_t rtt__u8(const rtt_font_t *f, uint32_t off)
{
   if (off >= f->size)
      return 0;
   return f->data[off];
}

static uint16_t rtt__u16(const rtt_font_t *f, uint32_t off)
{
   if (off + 2 > f->size || off + 2 < off)
      return 0;
   return (uint16_t)((f->data[off] << 8) | f->data[off + 1]);
}

static int16_t rtt__s16(const rtt_font_t *f, uint32_t off)
{
   return (int16_t)rtt__u16(f, off);
}

static uint32_t rtt__u32(const rtt_font_t *f, uint32_t off)
{
   if (off + 4 > f->size || off + 4 < off)
      return 0;
   return ((uint32_t)f->data[off]     << 24) |
          ((uint32_t)f->data[off + 1] << 16) |
          ((uint32_t)f->data[off + 2] <<  8) |
          ((uint32_t)f->data[off + 3]);
}

/* ------------------------------------------------------------------ */
/* Font directory                                                     */
/* ------------------------------------------------------------------ */

static int rtt__is_sfnt(const uint8_t *d, size_t size, size_t off)
{
   uint32_t v;
   if (off + 4 > size)
      return 0;
   v = ((uint32_t)d[off] << 24) | ((uint32_t)d[off + 1] << 16) |
       ((uint32_t)d[off + 2] << 8) | (uint32_t)d[off + 3];
   return (v == 0x00010000u || v == 0x74727565u); /* 1.0 or 'true' */
}

static int rtt_font_offset_for_index(const uint8_t *data, size_t size,
      int index)
{
   uint32_t tag;
   if (!data || size < 12)
      return -1;
   tag = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
         ((uint32_t)data[2] << 8) | (uint32_t)data[3];
   if (tag == 0x74746366u) /* 'ttcf' */
   {
      uint32_t n = ((uint32_t)data[8] << 24) | ((uint32_t)data[9] << 16) |
                   ((uint32_t)data[10] << 8) | (uint32_t)data[11];
      uint32_t rec;
      if (index < 0 || (uint32_t)index >= n)
         return -1;
      rec = 12u + 4u * (uint32_t)index;
      if (rec + 4 > size)
         return -1;
      return (int)(((uint32_t)data[rec]     << 24) |
                   ((uint32_t)data[rec + 1] << 16) |
                   ((uint32_t)data[rec + 2] <<  8) |
                   ((uint32_t)data[rec + 3]));
   }
   if (index != 0)
      return -1;
   if (rtt__is_sfnt(data, size, 0))
      return 0;
   return -1;
}

static uint32_t rtt__find_table(const rtt_font_t *f, uint32_t fontstart,
      const char *tag, uint32_t *len)
{
   uint32_t num = rtt__u16(f, fontstart + 4);
   uint32_t i;
   for (i = 0; i < num; i++)
   {
      uint32_t rec = fontstart + 12 + 16 * i;
      if (rec + 16 > f->size)
         return 0;
      if (   f->data[rec]     == (uint8_t)tag[0]
          && f->data[rec + 1] == (uint8_t)tag[1]
          && f->data[rec + 2] == (uint8_t)tag[2]
          && f->data[rec + 3] == (uint8_t)tag[3])
      {
         uint32_t off = rtt__u32(f, rec + 8);
         uint32_t l   = rtt__u32(f, rec + 12);
         if (off >= f->size || l > f->size - off)
            return 0;
         if (len)
            *len = l;
         return off;
      }
   }
   return 0;
}

static void rtt__select_cmap(rtt_font_t *f, uint32_t cmap, uint32_t cmap_len)
{
   uint32_t ntab = rtt__u16(f, cmap + 2);
   uint32_t best = 0;
   int best_rank = -1;
   uint32_t i;

   for (i = 0; i < ntab; i++)
   {
      uint32_t rec  = cmap + 4 + 8 * i;
      uint16_t plat = rtt__u16(f, rec);
      uint16_t enc  = rtt__u16(f, rec + 2);
      uint32_t off  = rtt__u32(f, rec + 4);
      int rank      = -1;

      if (off >= cmap_len)
         continue;

      if (plat == 3 && enc == 10)      rank = 4; /* Windows UCS-4    */
      else if (plat == 3 && enc == 1)  rank = 3; /* Windows BMP      */
      else if (plat == 0)              rank = 2; /* Unicode platform */
      else if (plat == 3 && enc == 0)  rank = 1; /* Windows symbol   */

      if (rank > best_rank)
      {
         best_rank = rank;
         best      = cmap + off;
      }
   }

   if (best_rank < 0)
      return;

   f->cmap_sub     = best;
   f->cmap_sub_len = cmap_len - (best - cmap);
   f->cmap_format  = rtt__u16(f, best);
}

static int rtt_init(rtt_font_t *f, const uint8_t *data, size_t size,
      int fontstart)
{
   uint32_t cmap, cmap_len = 0, maxp, head, hhea, dummy;

   memset(f, 0, sizeof(*f));
   if (!data || fontstart < 0 || (size_t)fontstart >= size)
      return 0;
   f->data = data;
   f->size = size;

   if (!rtt__is_sfnt(data, size, (size_t)fontstart))
      return 0;

   cmap    = rtt__find_table(f, (uint32_t)fontstart, "cmap", &cmap_len);
   head    = rtt__find_table(f, (uint32_t)fontstart, "head", &dummy);
   hhea    = rtt__find_table(f, (uint32_t)fontstart, "hhea", &dummy);
   maxp    = rtt__find_table(f, (uint32_t)fontstart, "maxp", &dummy);
   f->hmtx = rtt__find_table(f, (uint32_t)fontstart, "hmtx", &f->hmtx_len);
   f->glyf = rtt__find_table(f, (uint32_t)fontstart, "glyf", &f->glyf_len);
   f->loca = rtt__find_table(f, (uint32_t)fontstart, "loca", &f->loca_len);

   if (!cmap || !head || !hhea || !maxp || !f->hmtx || !f->glyf || !f->loca)
      return 0;

   f->head         = head;
   f->hhea         = hhea;
   f->units_per_em = rtt__u16(f, head + 18);
   f->loca_long    = rtt__s16(f, head + 50) != 0;
   f->num_glyphs   = rtt__u16(f, maxp + 4);
   f->num_hmetrics = rtt__u16(f, hhea + 34);

   if (f->units_per_em <= 0 || f->num_glyphs <= 0)
      return 0;

   rtt__select_cmap(f, cmap, cmap_len);
   if (!f->cmap_sub)
      return 0;

   return 1;
}

/* ------------------------------------------------------------------ */
/* cmap lookup                                                        */
/* ------------------------------------------------------------------ */

static int rtt_find_glyph(const rtt_font_t *f, uint32_t cp)
{
   uint32_t sub = f->cmap_sub;

   switch (f->cmap_format)
   {
      case 0:
         if (cp < 256)
            return rtt__u8(f, sub + 6 + cp);
         return 0;

      case 4:
      {
         uint32_t segx2   = rtt__u16(f, sub + 6);
         uint32_t segs    = segx2 >> 1;
         uint32_t end_a   = sub + 14;
         uint32_t start_a = sub + 16 + segx2;
         uint32_t delta_a = sub + 16 + 2 * segx2;
         uint32_t range_a = sub + 16 + 3 * segx2;
         uint32_t lo = 0, hi = segs, seg;
         uint16_t start_c, range;

         if (cp > 0xFFFFu || segs == 0)
            return 0;

         while (lo < hi) /* first segment with endCode >= cp */
         {
            uint32_t mid = (lo + hi) >> 1;
            if (rtt__u16(f, end_a + 2 * mid) < cp)
               lo = mid + 1;
            else
               hi = mid;
         }
         seg = lo;
         if (seg >= segs)
            return 0;
         start_c = rtt__u16(f, start_a + 2 * seg);
         if (cp < start_c)
            return 0;
         range = rtt__u16(f, range_a + 2 * seg);
         if (range == 0)
            return (int)((cp + rtt__u16(f, delta_a + 2 * seg)) & 0xFFFFu);
         else
         {
            uint32_t gaddr = range_a + 2 * seg + range + 2 * (cp - start_c);
            uint16_t g     = rtt__u16(f, gaddr);
            if (!g)
               return 0;
            return (int)((g + rtt__u16(f, delta_a + 2 * seg)) & 0xFFFFu);
         }
      }

      case 6:
      {
         uint32_t first = rtt__u16(f, sub + 6);
         uint32_t count = rtt__u16(f, sub + 8);
         if (cp < first || cp >= first + count)
            return 0;
         return rtt__u16(f, sub + 10 + 2 * (cp - first));
      }

      case 12:
      {
         uint32_t ngroups = rtt__u32(f, sub + 12);
         uint32_t maxg    = f->cmap_sub_len >= 16 ?
               (f->cmap_sub_len - 16) / 12u : 0;
         uint32_t lo = 0, hi;
         if (ngroups > maxg)
            ngroups = maxg;
         hi = ngroups;
         while (lo < hi)
         {
            uint32_t mid = (lo + hi) >> 1;
            if (rtt__u32(f, sub + 16 + 12 * mid + 4) < cp) /* endCharCode */
               lo = mid + 1;
            else
               hi = mid;
         }
         if (lo < ngroups)
         {
            uint32_t g = sub + 16 + 12 * lo;
            uint32_t s = rtt__u32(f, g);
            if (cp >= s && cp <= rtt__u32(f, g + 4))
               return (int)(rtt__u32(f, g + 8) + (cp - s));
         }
         return 0;
      }

      default:
         break;
   }
   return 0;
}

/* ------------------------------------------------------------------ */
/* Metrics                                                            */
/* ------------------------------------------------------------------ */

static void rtt_glyph_hmetrics(const rtt_font_t *f, int gi,
      int *advance, int *lsb)
{
   int nh = f->num_hmetrics;
   if (gi < 0 || gi >= f->num_glyphs || nh <= 0)
   {
      if (advance) *advance = 0;
      if (lsb)     *lsb     = 0;
      return;
   }
   if (gi < nh)
   {
      if (advance) *advance = rtt__u16(f, f->hmtx + 4 * (uint32_t)gi);
      if (lsb)     *lsb     = rtt__s16(f, f->hmtx + 4 * (uint32_t)gi + 2);
   }
   else
   {
      if (advance) *advance = rtt__u16(f, f->hmtx + 4 * ((uint32_t)nh - 1));
      if (lsb)     *lsb     = rtt__s16(f, f->hmtx + 4 * (uint32_t)nh
            + 2 * ((uint32_t)gi - (uint32_t)nh));
   }
}

static uint32_t rtt__glyf_offset(const rtt_font_t *f, int gi, uint32_t *glen)
{
   uint32_t o0, o1;
   *glen = 0;
   if (gi < 0 || gi >= f->num_glyphs)
      return 0;
   if (f->loca_long)
   {
      o0 = rtt__u32(f, f->loca + 4 * (uint32_t)gi);
      o1 = rtt__u32(f, f->loca + 4 * (uint32_t)gi + 4);
   }
   else
   {
      o0 = 2u * rtt__u16(f, f->loca + 2 * (uint32_t)gi);
      o1 = 2u * rtt__u16(f, f->loca + 2 * (uint32_t)gi + 2);
   }
   if (o1 <= o0 || o1 > f->glyf_len)
      return 0;
   *glen = o1 - o0;
   return f->glyf + o0;
}

static int rtt_glyph_box(const rtt_font_t *f, int gi,
      int *x0, int *y0, int *x1, int *y1)
{
   uint32_t glen;
   uint32_t g = rtt__glyf_offset(f, gi, &glen);
   if (!g || glen < 10)
      return 0;
   if (x0) *x0 = rtt__s16(f, g + 2);
   if (y0) *y0 = rtt__s16(f, g + 4);
   if (x1) *x1 = rtt__s16(f, g + 6);
   if (y1) *y1 = rtt__s16(f, g + 8);
   return 1;
}

static void rtt_vmetrics(const rtt_font_t *f, int *ascent, int *descent,
      int *line_gap)
{
   if (ascent)   *ascent   = rtt__s16(f, f->hhea + 4);
   if (descent)  *descent  = rtt__s16(f, f->hhea + 6);
   if (line_gap) *line_gap = rtt__s16(f, f->hhea + 8);
}

static float rtt_scale_for_pixel_height(const rtt_font_t *f, float h)
{
   int fh = rtt__s16(f, f->hhea + 4) - rtt__s16(f, f->hhea + 6);
   if (fh == 0)
      return 0.0f;
   return h / (float)fh;
}

static float rtt_scale_for_em_to_pixels(const rtt_font_t *f, float px)
{
   return px / (float)f->units_per_em;
}

/* ------------------------------------------------------------------ */
/* Rasterizer: signed-area delta accumulation.                        */
/*                                                                    */
/* For each line segment and each scanline it crosses, the exact area */
/* swept is distributed as per-cell deltas into a float buffer of     */
/* (w + 2) columns per row. A left-to-right prefix sum of a row gives */
/* the winding-weighted coverage of each pixel; |cov| clamped to 1    */
/* implements the TrueType non-zero fill rule. For cells fully inside */
/* the x-ramp of a crossing the per-cell delta is constant (dy * s);  */
/* the entry and exit cells carry the quadratic caps                  */
/* 0.5*s*(1-x0f)^2 and 0.5*s*x1f^2.                                   */
/* ------------------------------------------------------------------ */

typedef struct rtt__raster
{
   float *acc;   /* (w + 2) * h delta cells */
   int   *rowmax; /* per-row dirty extent (exclusive), for resolve */
   int    w;
   int    h;
   /* font units -> device transform (y flipped inside emit) */
   float  sx;
   float  sy;
   float  ox;
   float  oy;
   /* current point in device space */
   float  cx, cy;
   float  sx0, sy0; /* contour start */
} rtt__raster_t;

static void rtt__acc_line(rtt__raster_t *r, float x0, float y0,
      float x1, float y1)
{
   float dir, dxdy;
   int   y, ylast;
   int   W  = r->w;
   float fw = (float)W;

   if (y0 == y1)
      return;
   if (y0 < y1)
      dir = 1.0f;
   else
   {
      float t;
      dir = -1.0f;
      t = x0; x0 = x1; x1 = t;
      t = y0; y0 = y1; y1 = t;
   }

   if (y1 <= 0.0f || y0 >= (float)r->h)
      return;
   dxdy = (x1 - x0) / (y1 - y0);
   if (y0 < 0.0f)
   {
      x0 -= dxdy * y0;
      y0  = 0.0f;
   }
   if (y1 > (float)r->h)
   {
      x1 += dxdy * ((float)r->h - y1);
      y1  = (float)r->h;
   }

   y     = (int)y0;
   ylast = (int)ceil((double)y1) - 1;
   if (ylast >= r->h)
      ylast = r->h - 1;

   for (; y <= ylast; y++)
   {
      float *row = r->acc + (size_t)y * (size_t)(W + 2);
      float xa, xb, dy, xl, xr, d;
      float ytop = (float)y;
      float ybot = ytop + 1.0f;
      int   ixl, ixr;

      if (ytop < y0) ytop = y0;
      if (ybot > y1) ybot = y1;
      dy = ybot - ytop;
      if (dy <= 0.0f)
         continue;

      xa = x0 + dxdy * (ytop - y0);
      xb = x0 + dxdy * (ybot - y0);

      if (xa < xb) { xl = xa; xr = xb; }
      else         { xl = xb; xr = xa; }

      /* horizontal clamp: geometry outside collapses onto the border,
       * matching a polygon clip for coverage purposes */
      if (xl < 0.0f) xl = 0.0f;
      if (xr < 0.0f) xr = 0.0f;
      if (xl > fw)   xl = fw;
      if (xr > fw)   xr = fw;

      d   = dir * dy;
      ixl = (int)xl;
      ixr = (int)xr;
      if (ixl > W - 1) ixl = W - 1;
      if (ixr > W - 1) ixr = W - 1;

      if (ixr + 2 > r->rowmax[y])
         r->rowmax[y] = ixr + 2;

      if (ixl == ixr)
      {
         float xmf = 0.5f * (xl + xr) - (float)ixl;
         row[ixl]     += d * (1.0f - xmf);
         row[ixl + 1] += d * xmf;
      }
      else
      {
         float s   = 1.0f / (xr - xl);
         float x0f = xl - (float)ixl;
         float x1f = xr - (float)ixr;
         float a0  = 0.5f * s * (1.0f - x0f) * (1.0f - x0f);
         float am  = 0.5f * s * x1f * x1f;

         row[ixl] += d * a0;
         if (ixr == ixl + 1)
            row[ixr] += d * (1.0f - a0 - am);
         else
         {
            float a1 = s * (1.5f - x0f);
            float a2 = a1 + (float)(ixr - ixl - 2) * s;
            int   ix;
            row[ixl + 1] += d * (a1 - a0);
            for (ix = ixl + 2; ix < ixr; ix++)
               row[ix] += d * s;
            row[ixr] += d * (1.0f - a2 - am);
         }
         row[ixr + 1] += d * am;
      }
   }
}

/* Resolve one row of deltas to 8-bit coverage. The ONLY 8-bit specific
 * code in the rasterizer; a 10-bit HDR output path adds a sibling of
 * this routine and nothing else.
 *
 * rtt__resolve_row_u8() is defined once per architecture below (SSE2,
 * NEON, scalar). All variants sum with the same fixed block-of-4
 * association tree, so every build produces byte-identical output. */
static uint8_t rtt__cov_to_u8(float v)
{
   float c = v < 0.0f ? -v : v;
   if (c > 1.0f)
      c = 1.0f;
   return (uint8_t)(c * 255.0f + 0.5f);
}

#if defined(__SSE2__) || (defined(_MSC_VER) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)))
#include <emmintrin.h>

static void rtt__resolve_row_u8(const float *acc, uint8_t *dst, int w)
{
   __m128 carry = _mm_setzero_ps();
   __m128 sign  = _mm_castsi128_ps(_mm_set1_epi32((int)0x80000000u));
   __m128 one   = _mm_set1_ps(1.0f);
   __m128 k255  = _mm_set1_ps(255.0f);
   __m128 half  = _mm_set1_ps(0.5f);
   int i = 0;

   for (; i + 4 <= w; i += 4)
   {
      __m128 x = _mm_loadu_ps(acc + i);
      __m128i q;
      /* 4-lane inclusive prefix sum */
      x = _mm_add_ps(x, _mm_castsi128_ps(
            _mm_slli_si128(_mm_castps_si128(x), 4)));
      x = _mm_add_ps(x, _mm_castsi128_ps(
            _mm_slli_si128(_mm_castps_si128(x), 8)));
      x = _mm_add_ps(x, carry);
      carry = _mm_shuffle_ps(x, x, _MM_SHUFFLE(3, 3, 3, 3));

      /* |sum| clamped to 1, scaled; truncate(x*255+0.5) matches the
       * scalar (uint8_t)(c*255.0f + 0.5f) exactly */
      x = _mm_andnot_ps(sign, x);
      x = _mm_min_ps(x, one);
      q = _mm_cvttps_epi32(_mm_add_ps(_mm_mul_ps(x, k255), half));
      q = _mm_packs_epi32(q, q);
      q = _mm_packus_epi16(q, q);
      {
         int32_t v4 = _mm_cvtsi128_si32(q);
         memcpy(dst + i, &v4, 4);
      }
   }

   if (i < w)
   {
      float sum = _mm_cvtss_f32(carry);
      for (; i < w; i++)
      {
         sum   += acc[i];
         dst[i] = rtt__cov_to_u8(sum);
      }
   }
}

#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>

static void rtt__resolve_row_u8(const float *acc, uint8_t *dst, int w)
{
   float32x4_t carry = vdupq_n_f32(0.0f);
   float32x4_t one   = vdupq_n_f32(1.0f);
   float32x4_t k255  = vdupq_n_f32(255.0f);
   float32x4_t half  = vdupq_n_f32(0.5f);
   float32x4_t zero  = vdupq_n_f32(0.0f);
   int i = 0;

   for (; i + 4 <= w; i += 4)
   {
      float32x4_t x = vld1q_f32(acc + i);
      uint32x4_t  q;
      uint16x4_t  q16;
      uint8x8_t   q8;
      /* 4-lane inclusive prefix sum via lane shifts */
      x = vaddq_f32(x, vextq_f32(zero, x, 3));
      x = vaddq_f32(x, vextq_f32(zero, x, 2));
      x = vaddq_f32(x, carry);
      carry = vdupq_n_f32(vgetq_lane_f32(x, 3));

      x   = vabsq_f32(x);
      x   = vminq_f32(x, one);
      /* vcvtq truncates, matching the scalar cast */
      q   = vcvtq_u32_f32(vaddq_f32(vmulq_f32(x, k255), half));
      q16 = vmovn_u32(q);
      q8  = vmovn_u16(vcombine_u16(q16, q16));
      {
         uint32_t v4 = vget_lane_u32(vreinterpret_u32_u8(q8), 0);
         memcpy(dst + i, &v4, 4);
      }
   }

   if (i < w)
   {
      float sum = vgetq_lane_f32(carry, 0);
      for (; i < w; i++)
      {
         sum   += acc[i];
         dst[i] = rtt__cov_to_u8(sum);
      }
   }
}

#else

static void rtt__resolve_row_u8(const float *acc, uint8_t *dst, int w)
{
   float carry = 0.0f;
   int   i = 0;

   for (; i + 4 <= w; i += 4)
   {
      /* identical association to the 4-lane prefix sum:
       * t1=x1+x0 t2=x2+x1 t3=x3+x2; u2=t2+x0 u3=t3+t1; +carry */
      float x0 = acc[i],     x1 = acc[i + 1];
      float x2 = acc[i + 2], x3 = acc[i + 3];
      float t1 = x1 + x0;
      float t2 = x2 + x1;
      float t3 = x3 + x2;
      float u2 = t2 + x0;
      float u3 = t3 + t1;
      dst[i]     = rtt__cov_to_u8(x0 + carry);
      dst[i + 1] = rtt__cov_to_u8(t1 + carry);
      dst[i + 2] = rtt__cov_to_u8(u2 + carry);
      dst[i + 3] = rtt__cov_to_u8(u3 + carry);
      carry      = u3 + carry;
   }
   for (; i < w; i++)
   {
      carry += acc[i];
      dst[i] = rtt__cov_to_u8(carry);
   }
}

#endif

/* ------------------------------------------------------------------ */
/* Outline loading / flattening                                       */
/* ------------------------------------------------------------------ */

static void rtt__emit_move(rtt__raster_t *r, float x, float y)
{
   /* close previous contour if left open */
   if (r->cx != r->sx0 || r->cy != r->sy0)
      rtt__acc_line(r, r->cx, r->cy, r->sx0, r->sy0);
   r->cx = r->sx0 = x;
   r->cy = r->sy0 = y;
}

static void rtt__emit_line(rtt__raster_t *r, float x, float y)
{
   rtt__acc_line(r, r->cx, r->cy, x, y);
   r->cx = x;
   r->cy = y;
}

/* Flatten a quadratic bezier (device space) with subdivision count
 * derived from its deviation from the chord. */
static void rtt__emit_quad(rtt__raster_t *r, float cx, float cy,
      float x, float y)
{
   float x0  = r->cx, y0 = r->cy;
   float ddx = x0 - 2.0f * cx + x;
   float ddy = y0 - 2.0f * cy + y;
   /* squared midpoint deviation of the curve from its chord is
    * (ddx^2 + ddy^2) / 16; each midpoint subdivision divides it by 16.
    * Subdivide until it is within 0.35px, mirroring the tessellation
    * of the rasterizer this replaces so glyph output matches. */
   float md2 = (ddx * ddx + ddy * ddy) * (1.0f / 16.0f);
   int   n   = 1;
   int   i;

   while (md2 > 0.1225f && n < 256)
   {
      md2 *= (1.0f / 16.0f);
      n  <<= 1;
   }

   for (i = 1; i < n; i++)
   {
      float t  = (float)i / (float)n;
      float u  = 1.0f - t;
      float qx = u * u * x0 + 2.0f * u * t * cx + t * t * x;
      float qy = u * u * y0 + 2.0f * u * t * cy + t * t * y;
      rtt__emit_line(r, qx, qy);
   }
   rtt__emit_line(r, x, y);
}

/* 2x3 transform (composite glyphs): device = M * unit_point + offset */
typedef struct
{
   float a, b, c, d; /* [a b; c d] */
   float dx, dy;     /* offset in font units */
} rtt__xform_t;

static void rtt__xf_point(const rtt__xform_t *m, float x, float y,
      float *ox, float *oy)
{
   *ox = m->a * x + m->c * y + m->dx;
   *oy = m->b * x + m->d * y + m->dy;
}

/* Convert a font-unit point through the component transform chain and
 * the device scale/flip into device space, then emit. */
static void rtt__dev(const rtt__raster_t *r, const rtt__xform_t *m,
      float ux, float uy, float *dx, float *dy)
{
   float tx, ty;
   rtt__xf_point(m, ux, uy, &tx, &ty);
   *dx =  tx * r->sx - r->ox;
   *dy = -ty * r->sy - r->oy;
}

/* Walk one glyph's outline, emitting flattened segments. Returns 0 on
 * malformed data (treated as empty). */
static int rtt__walk_glyph(const rtt_font_t *f, rtt__raster_t *r,
      int gi, const rtt__xform_t *m, int depth)
{
   uint32_t glen;
   uint32_t g = rtt__glyf_offset(f, gi, &glen);
   int ncont;

   if (!g || glen < 10 || depth > RTT_MAX_COMPOSITE_DEPTH)
      return 0;

   ncont = rtt__s16(f, g);

   if (ncont >= 0)
   {
      /* ---- simple glyph ---- */
      uint32_t endpts = g + 10;
      uint32_t inslen, p;
      int npts, i, c;
      uint8_t *flags = NULL;
      float   *xs    = NULL;
      float   *ys    = NULL;
      int ok = 0;

      if (ncont == 0)
         return 1;
      npts = (int)rtt__u16(f, endpts + 2 * ((uint32_t)ncont - 1)) + 1;
      if (npts <= 0 || npts > 0xFFFF)
         return 0;

      inslen = rtt__u16(f, g + 10 + 2 * (uint32_t)ncont);
      p      = g + 12 + 2 * (uint32_t)ncont + inslen;

      flags = (uint8_t*)malloc((size_t)npts);
      xs    = (float*)malloc(sizeof(float) * (size_t)npts);
      ys    = (float*)malloc(sizeof(float) * (size_t)npts);
      if (!flags || !xs || !ys)
         goto simple_done;

      /* flags, run-length encoded */
      for (i = 0; i < npts; )
      {
         uint8_t fl = rtt__u8(f, p++);
         flags[i++] = fl;
         if (fl & 0x08) /* REPEAT */
         {
            uint8_t rep = rtt__u8(f, p++);
            while (rep-- && i < npts)
               flags[i++] = fl;
         }
      }

      /* x deltas */
      {
         int v = 0;
         for (i = 0; i < npts; i++)
         {
            uint8_t fl = flags[i];
            if (fl & 0x02) /* X_SHORT */
            {
               uint8_t d8 = rtt__u8(f, p++);
               v += (fl & 0x10) ? (int)d8 : -(int)d8;
            }
            else if (!(fl & 0x10))
            {
               v += rtt__s16(f, p);
               p += 2;
            }
            xs[i] = (float)v;
         }
      }
      /* y deltas */
      {
         int v = 0;
         for (i = 0; i < npts; i++)
         {
            uint8_t fl = flags[i];
            if (fl & 0x04) /* Y_SHORT */
            {
               uint8_t d8 = rtt__u8(f, p++);
               v += (fl & 0x20) ? (int)d8 : -(int)d8;
            }
            else if (!(fl & 0x20))
            {
               v += rtt__s16(f, p);
               p += 2;
            }
            ys[i] = (float)v;
         }
      }
      if (p > f->size)
         goto simple_done;

      /* contours -> segments */
      {
         int start = 0;
         for (c = 0; c < ncont; c++)
         {
            int end = (int)rtt__u16(f, endpts + 2 * (uint32_t)c);
            int n, j, first_on;
            float fx, fy;       /* contour start point, device */
            float px, py;       /* pending control point       */
            int   have_ctrl;

            if (end < start || end >= npts)
               goto simple_done;
            n = end - start + 1;
            if (n < 2)
            {
               start = end + 1;
               continue;
            }

            /* find the starting on-curve point; if none, synthesize
             * the midpoint of the first two off-curve points */
            first_on = -1;
            for (j = 0; j < n; j++)
               if (flags[start + j] & 0x01)
               {
                  first_on = j;
                  break;
               }

            if (first_on >= 0)
               rtt__dev(r, m, xs[start + first_on], ys[start + first_on],
                     &fx, &fy);
            else
            {
               float ax, ay, bx, by;
               rtt__dev(r, m, xs[start], ys[start], &ax, &ay);
               rtt__dev(r, m, xs[start + 1], ys[start + 1], &bx, &by);
               fx = 0.5f * (ax + bx);
               fy = 0.5f * (ay + by);
               first_on = 0; /* iterate from point 0; start is implicit */
            }

            rtt__emit_move(r, fx, fy);
            have_ctrl = 0;
            px = py = 0.0f;

            for (j = 1; j <= n; j++)
            {
               int   idx = start + (first_on + j) % n;
               float vx, vy;
               int   on = flags[idx] & 0x01;
               rtt__dev(r, m, xs[idx], ys[idx], &vx, &vy);

               if (on)
               {
                  if (have_ctrl)
                  {
                     rtt__emit_quad(r, px, py, vx, vy);
                     have_ctrl = 0;
                  }
                  else
                     rtt__emit_line(r, vx, vy);
               }
               else
               {
                  if (have_ctrl)
                  {
                     /* two consecutive off-curve points: implied
                      * on-curve midpoint between them */
                     float mx = 0.5f * (px + vx);
                     float my = 0.5f * (py + vy);
                     rtt__emit_quad(r, px, py, mx, my);
                  }
                  px = vx;
                  py = vy;
                  have_ctrl = 1;
               }
            }
            /* close: back to the contour start */
            if (have_ctrl)
               rtt__emit_quad(r, px, py, fx, fy);
            else
               rtt__emit_line(r, fx, fy);

            start = end + 1;
         }
      }
      ok = 1;

simple_done:
      free(flags);
      free(xs);
      free(ys);
      return ok;
   }
   else
   {
      /* ---- composite glyph ---- */
      uint32_t p = g + 10;
      int guard  = 0;

      for (;;)
      {
         uint16_t fl  = rtt__u16(f, p);
         uint16_t cgi = rtt__u16(f, p + 2);
         float dx = 0.0f, dy = 0.0f;
         rtt__xform_t cm;
         p += 4;

         if (fl & 0x0001) /* ARG_1_AND_2_ARE_WORDS */
         {
            if (fl & 0x0002) /* ARGS_ARE_XY_VALUES */
            {
               dx = (float)rtt__s16(f, p);
               dy = (float)rtt__s16(f, p + 2);
            }
            p += 4;
         }
         else
         {
            if (fl & 0x0002)
            {
               dx = (float)(int8_t)rtt__u8(f, p);
               dy = (float)(int8_t)rtt__u8(f, p + 1);
            }
            p += 2;
         }
         /* point-matching placement (rare) is unsupported: offsets 0 */

         cm.a = cm.d = 1.0f;
         cm.b = cm.c = 0.0f;
         if (fl & 0x0008) /* WE_HAVE_A_SCALE */
         {
            cm.a = cm.d = (float)rtt__s16(f, p) / 16384.0f;
            p += 2;
         }
         else if (fl & 0x0040) /* X_AND_Y_SCALE */
         {
            cm.a = (float)rtt__s16(f, p)     / 16384.0f;
            cm.d = (float)rtt__s16(f, p + 2) / 16384.0f;
            p += 4;
         }
         else if (fl & 0x0080) /* 2x2 */
         {
            cm.a = (float)rtt__s16(f, p)     / 16384.0f;
            cm.b = (float)rtt__s16(f, p + 2) / 16384.0f;
            cm.c = (float)rtt__s16(f, p + 4) / 16384.0f;
            cm.d = (float)rtt__s16(f, p + 6) / 16384.0f;
            p += 8;
         }

         /* compose child transform with parent: child point q ->
          * parent-space M_parent * (M_child * q + t_child) */
         {
            rtt__xform_t out;
            out.a  = m->a * cm.a + m->c * cm.b;
            out.b  = m->b * cm.a + m->d * cm.b;
            out.c  = m->a * cm.c + m->c * cm.d;
            out.d  = m->b * cm.c + m->d * cm.d;
            out.dx = m->a * dx + m->c * dy + m->dx;
            out.dy = m->b * dx + m->d * dy + m->dy;
            rtt__walk_glyph(f, r, (int)cgi, &out, depth + 1);
         }

         if (!(fl & 0x0020)) /* MORE_COMPONENTS */
            break;
         if (++guard > 64 || p >= f->size)
            break;
      }
      return 1;
   }
}

/* Render glyph 'gi' into dst (out_w x out_h, given stride), scaled by
 * sx/sy, positioned exactly like the code this replaces: the bitmap
 * origin is (floor(xmin*sx), floor(-ymax*sy)) of the glyph bbox. The
 * full out_w x out_h region is always written. */
static void rtt_render_glyph(const rtt_font_t *f, uint8_t *dst,
      int out_w, int out_h, int stride, float sx, float sy, int gi)
{
   rtt__raster_t r;
   rtt__xform_t  ident;
   int bx0, by0, bx1, by1, y;
   size_t cells;

   if (!dst || out_w <= 0 || out_h <= 0)
      return;

   if (!rtt_glyph_box(f, gi, &bx0, &by0, &bx1, &by1))
   {
      for (y = 0; y < out_h; y++)
         memset(dst + (size_t)y * (size_t)stride, 0, (size_t)out_w);
      return;
   }

   r.w  = out_w;
   r.h  = out_h;
   r.sx = sx;
   r.sy = sy;
   r.ox = (float)floor((double)((float)bx0 * sx));
   r.oy = (float)floor((double)(-(float)by1 * sy));
   r.cx = r.sx0 = 0.0f;
   r.cy = r.sy0 = 0.0f;

   cells = (size_t)(out_w + 2) * (size_t)out_h;
   /* one allocation: float cells + per-row dirty extents */
   r.acc = (float*)calloc(cells + (size_t)out_h, sizeof(float));
   if (!r.acc)
   {
      for (y = 0; y < out_h; y++)
         memset(dst + (size_t)y * (size_t)stride, 0, (size_t)out_w);
      return;
   }
   r.rowmax = (int*)(r.acc + cells);

   ident.a = ident.d = 1.0f;
   ident.b = ident.c = 0.0f;
   ident.dx = ident.dy = 0.0f;

   rtt__walk_glyph(f, &r, gi, &ident, 0);
   /* close the final contour */
   if (r.cx != r.sx0 || r.cy != r.sy0)
      rtt__acc_line(&r, r.cx, r.cy, r.sx0, r.sy0);

   for (y = 0; y < out_h; y++)
   {
      uint8_t *drow = dst + (size_t)y * (size_t)stride;
      int      dw   = r.rowmax[y];
      if (dw > out_w)
         dw = out_w;
      if (dw > 0)
         rtt__resolve_row_u8(r.acc + (size_t)y * (size_t)(out_w + 2),
               drow, dw);
      /* past the last edge the winding sum of a closed outline is
       * zero, so the tail of the row is empty */
      if (dw < out_w)
         memset(drow + dw, 0, (size_t)(out_w - dw));
   }

   free(r.acc);
}

/* ==================== end cleanroom TrueType ==================== */


#define STB_ATLAS_ROWS 16
#define STB_ATLAS_COLS 16
#define STB_ATLAS_SIZE (STB_ATLAS_ROWS * STB_ATLAS_COLS)
/* Padding is required between each glyph in
 * the atlas to prevent texture bleed when
 * drawing with linear filtering enabled */
#define STB_ATLAS_PADDING 1

/* Improved hash: mix in upper bits to reduce clustering
 * for CJK and other non-Latin codepoints */
#define STB_HASH_SIZE 0x100
#define STB_HASH(c) (((c) ^ ((c) >> 8)) & (STB_HASH_SIZE - 1))

typedef struct stb_atlas_slot
{
   struct stb_atlas_slot* next;
   struct font_glyph glyph;      /* unsigned alignment */
   unsigned charcode;
   unsigned last_used;
} stb_atlas_slot_t;

typedef struct
{
   uint8_t *font_data;
   size_t font_data_size;
   /* Whether font_data was allocated by us (and must be freed) or points
    * to OS-owned shared memory (WiiU) that must not be freed. */
   bool font_data_owned;
   struct font_atlas atlas;               /* ptr alignment */
   stb_atlas_slot_t* uc_map[STB_HASH_SIZE];
   stb_atlas_slot_t atlas_slots[STB_ATLAS_SIZE];
   rtt_font_t info;                       /* ptr alignment */
   int max_glyph_width;
   int max_glyph_height;
   unsigned usage_counter;
   float scale_factor;
   struct font_line_metrics line_metrics; /* float alignment */
} stb_font_renderer_t;

static struct font_atlas *font_renderer_stb_get_atlas(void *data)
{
   stb_font_renderer_t *self = (stb_font_renderer_t*)data;
   return &self->atlas;
}

static void font_renderer_stb_free(void *data)
{
   stb_font_renderer_t *self = (stb_font_renderer_t*)data;

   free(self->atlas.buffer);
   /* Do not free WiiU shared-memory font data; it is owned by the OS. */
   if (self->font_data_owned)
      free(self->font_data);
   free(self);
}

static stb_atlas_slot_t* font_renderer_stb_get_slot(stb_font_renderer_t *handle)
{
   int i;
   unsigned map_id;
   unsigned oldest = 0;
   stb_atlas_slot_t *ptr;
   /* Find the least-recently-used slot.
    * Unsigned subtraction handles wrap-around correctly. */
   unsigned oldest_age = handle->usage_counter -
      handle->atlas_slots[0].last_used;

   for (i = 1; i < STB_ATLAS_SIZE; i++)
   {
      unsigned age = handle->usage_counter - handle->atlas_slots[i].last_used;
      if (age > oldest_age)
      {
         oldest_age = age;
         oldest     = i;
      }
   }

   /* Remove from map */
   map_id = STB_HASH(handle->atlas_slots[oldest].charcode);
   if (handle->uc_map[map_id] == &handle->atlas_slots[oldest])
      handle->uc_map[map_id] = handle->atlas_slots[oldest].next;
   else if (handle->uc_map[map_id])
   {
      ptr = handle->uc_map[map_id];
      while (ptr->next && ptr->next != &handle->atlas_slots[oldest])
         ptr = ptr->next;
      ptr->next = handle->atlas_slots[oldest].next;
   }

   return &handle->atlas_slots[oldest];
}

static const struct font_glyph *font_renderer_stb_get_glyph(
      void *data, uint32_t charcode)
{
   int glyph_index                      = 0;
   int x0                               = 0;
   int y1                               = 0;
   int advance_width                    = 0;
   int left_side_bearing                = 0;
   unsigned map_id                      = 0;
   uint8_t *dst                         = NULL;
   stb_atlas_slot_t* atlas_slot = NULL;
   stb_font_renderer_t *self    = (stb_font_renderer_t*)data;
   float glyph_advance_x               = 0.0f;
   float glyph_draw_offset_y           = 0.0f;

   if (!self)
      return NULL;

   map_id                               = STB_HASH(charcode);
   atlas_slot                           = self->uc_map[map_id];

   while (atlas_slot)
   {
      if (atlas_slot->charcode == charcode)
      {
         atlas_slot->last_used = self->usage_counter++;
         return &atlas_slot->glyph;
      }
      atlas_slot = atlas_slot->next;
   }

   atlas_slot             = font_renderer_stb_get_slot(self);
   atlas_slot->charcode   = charcode;
   atlas_slot->next       = self->uc_map[map_id];
   self->uc_map[map_id]   = atlas_slot;

   glyph_index            = rtt_find_glyph(&self->info, charcode);

   dst = (uint8_t*)self->atlas.buffer + atlas_slot->glyph.atlas_offset_x
         + atlas_slot->glyph.atlas_offset_y * self->atlas.width;

   rtt_glyph_hmetrics(&self->info, glyph_index, &advance_width, &left_side_bearing);

   if (rtt_glyph_box(&self->info, glyph_index, &x0, NULL, NULL, &y1))
      rtt_render_glyph(&self->info, dst,
         self->max_glyph_width, self->max_glyph_height,
         self->atlas.width, self->scale_factor,
         self->scale_factor, glyph_index);
   else
   {
      /* This means the glyph is empty; zero its atlas region.
       * Use row-major memset for cache-friendly clearing. */
      int row;
      for (row = 0; row < self->max_glyph_height; row++)
         memset(dst + (row * self->atlas.width), 0,
                (size_t)self->max_glyph_width);
   }

   atlas_slot->glyph.width          = self->max_glyph_width;
   atlas_slot->glyph.height         = self->max_glyph_height;

   /* advance_x must always be rounded to the
    * *nearest* integer */
   glyph_advance_x                  = (float)advance_width * self->scale_factor;
   atlas_slot->glyph.advance_x      = (int)((glyph_advance_x > 0.0f)
         ? (glyph_advance_x + 0.5f)
         : (glyph_advance_x - 0.5f));
   /* advance_y is always zero */
   atlas_slot->glyph.advance_y      = 0;

   /* draw_offset_x must always be rounded *down*
    * to the nearest integer */
   atlas_slot->glyph.draw_offset_x  = (int)((float)x0 * self->scale_factor);

   /* draw_offset_y must always be rounded *up*
    * to the nearest integer */
   glyph_draw_offset_y              = (float)(-y1) * self->scale_factor;
   atlas_slot->glyph.draw_offset_y  = (int)((glyph_draw_offset_y < 0.0f)
         ? floor((double)glyph_draw_offset_y)
         : ceil((double)glyph_draw_offset_y));

   self->atlas.dirty                = true;
   atlas_slot->last_used            = self->usage_counter++;
   return &atlas_slot->glyph;
}

static bool font_renderer_stb_create_atlas(
      stb_font_renderer_t *self, float font_size)
{
   unsigned i, x, y;
   stb_atlas_slot_t* slot = NULL;
   int max_glyph_size             = (font_size < 0) ? (int)(-font_size) : (int)font_size;

   /* Bound the glyph size. Every glyph cell is max_glyph_size on a side and
    * the atlas is (size + padding) * 16 in each dimension, so an unchecked
    * size lets self->atlas.width * self->atlas.height overflow 'unsigned',
    * after which the calloc below is undersized and every subsequent
    * rtt_render_glyph()/memset writes past the buffer. Keep the
    * 2048-per-axis ceiling. */
   if (max_glyph_size < 1)
      return false;
   if (max_glyph_size > 2048 / STB_ATLAS_COLS - STB_ATLAS_PADDING)
      max_glyph_size = 2048 / STB_ATLAS_COLS - STB_ATLAS_PADDING;

   self->max_glyph_width          = max_glyph_size;
   self->max_glyph_height         = max_glyph_size;

   self->atlas.width              = (self->max_glyph_width  + STB_ATLAS_PADDING) * STB_ATLAS_COLS;
   self->atlas.height             = (self->max_glyph_height + STB_ATLAS_PADDING) * STB_ATLAS_ROWS;

   /* Pass the two dimensions separately so the C library's calloc overflow
    * check applies, rather than pre-multiplying into a single argument. */
   self->atlas.buffer             = (uint8_t*)calloc(
      self->atlas.height, self->atlas.width);

   if (!self->atlas.buffer)
      return false;

   slot = self->atlas_slots;

   for (y = 0; y < STB_ATLAS_ROWS; y++)
   {
      for (x = 0; x < STB_ATLAS_COLS; x++)
      {
         slot->glyph.atlas_offset_x = x * (self->max_glyph_width  + STB_ATLAS_PADDING);
         slot->glyph.atlas_offset_y = y * (self->max_glyph_height + STB_ATLAS_PADDING);
         slot++;
      }
   }

   /* Pre-populate only printable ASCII (32-126).
    * Control characters (0-31) are never rendered and
    * would waste atlas slots that could hold real glyphs. */
   for (i = 32; i < 127; i++)
      font_renderer_stb_get_glyph(self, i);

   return true;
}

static void *font_renderer_stb_init(const char *font_path, float font_size)
{
   int ascent, descent, line_gap;
   stb_font_renderer_t *self =
      (stb_font_renderer_t*)calloc(1, sizeof(*self));

   if (!self || font_size < 1.0f)
      goto error;

   /* Negative size selects em-mapped scaling below (the convention the
    * previous STBTT_POINT_SIZE() macro encoded). */
   font_size = -font_size;

#ifdef WIIU
   if (!*font_path)
   {
      uint32_t size = 0;
      /* OS-owned shared memory: borrowed, not owned - must not be freed. */
      if (!OSGetSharedData(SHARED_FONT_DEFAULT, 0, (void**)&self->font_data, &size))
         goto error;
      self->font_data_size  = size;
      self->font_data_owned = false;
   }
   else
#endif
   {
      int64_t len = 0;
      if (!path_is_valid(font_path) || !filestream_read_file(font_path, (void**)&self->font_data, &len))
         goto error;
      if (len <= 0)
         goto error;
      self->font_data_size  = (size_t)len;
      self->font_data_owned = true;
   }

   /* Guard against empty/corrupt font files */
   if (!self->font_data)
      goto error;

   {
      int font_offs = rtt_font_offset_for_index(self->font_data,
            self->font_data_size, 0);
      if (font_offs < 0 || !rtt_init(&self->info, self->font_data,
               self->font_data_size, font_offs))
         goto error;
   }

   rtt_vmetrics(&self->info, &ascent, &descent, &line_gap);

   if (font_size < 0)
      self->scale_factor = rtt_scale_for_em_to_pixels(&self->info, -font_size);
   else
      self->scale_factor = rtt_scale_for_pixel_height(&self->info, font_size);

   if (self->scale_factor <= 0.0f)
      goto error;

   /* Ascender, descender and line_gap values always
    * end up ~0.5 pixels too small when scaled...
    * > Add a manual correction factor */
   self->line_metrics.ascender  = 0.5f + (float)ascent * self->scale_factor;
   self->line_metrics.descender = 0.5f + ((float)(-descent) * self->scale_factor);
   self->line_metrics.height    = 0.5f + (float)(ascent - descent + line_gap) * self->scale_factor;

   if (!font_renderer_stb_create_atlas(self, font_size))
      goto error;

   return self;

error:
   if (self)
      font_renderer_stb_free(self);
   return NULL;
}

static const char *font_renderer_stb_get_default_font(void)
{
#ifdef WIIU
   return "";
#else
   static const char *paths[] = {
#if defined(_WIN32) && !defined(__WINRT__)
      "C:\\Windows\\Fonts\\consola.ttf",
      "C:\\Windows\\Fonts\\verdana.ttf",
#elif defined(__APPLE__)
      "/Library/Fonts/Microsoft/Candara.ttf",
      "/Library/Fonts/Verdana.ttf",
      "/Library/Fonts/Tahoma.ttf",
      "/Library/Fonts/Andale Mono.ttf",
      "/Library/Fonts/Courier New.ttf",
#elif defined(__ANDROID_API__)
      "/system/fonts/DroidSansMono.ttf",
      "/system/fonts/CutiveMono.ttf",
      "/system/fonts/DroidSans.ttf",
#elif defined(VITA)
      "vs0:data/external/font/pvf/c041056ts.ttf",
      "vs0:data/external/font/pvf/d013013ds.ttf",
      "vs0:data/external/font/pvf/e046323ms.ttf",
      "vs0:data/external/font/pvf/e046323ts.ttf",
      "vs0:data/external/font/pvf/k006004ds.ttf",
      "vs0:data/external/font/pvf/n023055ms.ttf",
      "vs0:data/external/font/pvf/n023055ts.ttf",
#elif defined(ORBIS)
      "/preinst/common/font/c041056ts.ttf",
      "/preinst/common/font/d013013ds.ttf",
      "/preinst/common/font/e046323ms.ttf",
      "/preinst/common/font/e046323ts.ttf",
      "/preinst/common/font/k006004ds.ttf",
      "/preinst/common/font/n023055ms.ttf",
      "/preinst/common/font/n023055ts.ttf",
#elif !defined(__WINRT__)
      "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
      "/usr/share/fonts/TTF/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSansMono.ttf",
      "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "osd-font.ttf",
#endif
      NULL
   };

   const char **p;

   for (p = paths; *p; ++p)
      if (path_is_valid(*p))
         return *p;

   return NULL;
#endif
}

static void font_renderer_stb_get_line_metrics(
      void* data, struct font_line_metrics **metrics)
{
   stb_font_renderer_t *handle = (stb_font_renderer_t*)data;
   *metrics = &handle->line_metrics;
}

font_renderer_driver_t stb_font_renderer = {
   font_renderer_stb_init,
   font_renderer_stb_get_atlas,
   font_renderer_stb_get_glyph,
   font_renderer_stb_free,
   font_renderer_stb_get_default_font,
   "font_renderer_stb",
   font_renderer_stb_get_line_metrics
};

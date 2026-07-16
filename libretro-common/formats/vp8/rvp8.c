/* rvp8 -- self-contained VP8 decoder (key frame and inter frame) for
 * libretro-common.
 *
 * The intra path was extracted verbatim from the WebP decoder
 * (formats/webp/rwebp.c): coefficient tokens, dequantisation, the DCT and
 * Walsh-Hadamard inverse transforms, 4x4 and 16x16 intra prediction, both
 * loop filters, fancy chroma upsampling and YUV->RGB.
 *
 * The inter path implements the rest of RFC 6386: inter frame headers,
 * per-macroblock mode and motion-vector decode (ZEROMV/NEAREST/NEAR/NEW/
 * SPLITMV, intra-in-inter), near-MV prediction with reference sign bias,
 * six-tap sub-pixel motion compensation, per-mode/ref loop-filter deltas,
 * and last/golden/altref reference-frame management (rvp8_video_*).
 * Every path is verified byte-identical against libvpx.
 *
 * Public entry points are declared in <formats/rvp8.h>:
 *   - rvp8_decode / the resumable rvp8_* row API for single key frames
 *     (WebP 'VP8 ' chunks are always a single key frame);
 *   - the rvp8_video_* persistent decoder for VP8 video streams.
 *
 * SPDX-License-Identifier: MIT  (RetroArch libretro-common)
 */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <retro_inline.h>
#include <formats/rvp8.h>

/* Private scalar abs; the WebP file has its own rvp8_abs, and keeping a
 * local copy lets rvp8 and rwebp coexist in a single-TU (griffin) build. */
static INLINE int rvp8_abs(int x) { return x < 0 ? -x : x; }

/* ===== VP8 Lossy — full decode with coefficients ===== */


/* vp8b_norm[r] = number of doublings that bring r into [128, 255];
 * exactly what the former while (range < 128) loop computed. Index 0
 * is unused (range never reaches 0: split >= 1 and range - split >= 1
 * whenever the corresponding branch is taken). */
static const uint8_t vp8b_norm[256] = {
   0, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
   3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
   2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
   2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static void vp8b_fill(rvp8_bool *b)
{
   int shift = 48 - b->count;
   if (shift >= 0 && b->end - b->buf >= 8)
   {
      /* Bulk path: one big-endian load of the exact bytes the loop
       * below would consume. Byte k of the load sits at bit 56 - 8k;
       * after >> (count + 8) it lands at 48 - count - 8k, the loop's
       * shift for byte k. Masking to nbytes drops the tail bytes the
       * loop would not have loaded (an unmasked shift would leak
       * their high bits into the low bits of the value register). */
      int nbytes = shift / 8 + 1;
      uint64_t big;
      memcpy(&big, b->buf, 8);
#if defined(MSB_FIRST)
      /* already big-endian in memory */
#elif defined(__GNUC__)
      big = __builtin_bswap64(big);
#else
      big = ((big & 0x00000000000000FFull) << 56)
          | ((big & 0x000000000000FF00ull) << 40)
          | ((big & 0x0000000000FF0000ull) << 24)
          | ((big & 0x00000000FF000000ull) <<  8)
          | ((big & 0x000000FF00000000ull) >>  8)
          | ((big & 0x0000FF0000000000ull) >> 24)
          | ((big & 0x00FF000000000000ull) >> 40)
          | ((big & 0xFF00000000000000ull) >> 56);
#endif
      big      &= ~0ull << (64 - 8 * nbytes);
      b->value |= big >> (b->count + 8);
      b->buf   += nbytes;
      b->count += nbytes * 8;
      return;
   }
   while (shift >= 0 && b->buf < b->end)
   {
      b->count += 8;
      b->value |= (uint64_t)(*b->buf++) << shift;
      shift -= 8;
   }
}

static void vp8b_init(rvp8_bool *b, const uint8_t *d, size_t s)
{
   b->buf = d; b->end = d + s; b->range = 255;
   b->value = 0; b->count = -8;
   vp8b_fill(b);
}

static INLINE int vp8b_get(rvp8_bool *b, int prob)
{
   uint32_t split = 1 + (((b->range - 1) * (uint32_t)prob) >> 8);
   uint64_t bigsplit = (uint64_t)split << 56;
   int bit, shift;
   if (b->value >= bigsplit)
   {
      bit = 1; b->range -= split; b->value -= bigsplit;
   }
   else
   {
      bit = 0; b->range = split;
   }
   shift = vp8b_norm[b->range];
   b->range <<= shift;
   b->value <<= shift;
   b->count -= shift;
   if (b->count < 0) vp8b_fill(b);
   return bit;
}


static INLINE int     vp8b_bit(rvp8_bool *b)       { return vp8b_get(b, 128); }
static INLINE uint32_t vp8b_lit(rvp8_bool *b, int n)
{ uint32_t v = 0; int i; for (i = n-1; i >= 0; i--) v |= (uint32_t)vp8b_get(b,128) << i; return v; }
static INLINE int32_t vp8b_sig(rvp8_bool *b, int n)
{ int32_t v = (int32_t)vp8b_lit(b,n); return vp8b_bit(b) ? -v : v; }

static INLINE uint8_t vp8_cl(int v) { return (uint8_t)(v<0?0:v>255?255:v); }

/* libwebp fixed-point YUV -> RGB (yuv.h, YUV_FIX2 = 6): matches dwebp
 * output exactly. */
static INLINE int vp8_mulhi(int v, int coeff) { return (v * coeff) >> 8; }

static INLINE uint8_t vp8_clip8(int v)
{
   return ((v & ~16383) == 0) ? (uint8_t)(v >> 6) : (v < 0) ? 0 : 255;
}

static void vp8_yuv2rgb(int y, int u, int v, uint8_t *r, uint8_t *g, uint8_t *bo)
{
   int yg = vp8_mulhi(y, 19077);
   *r  = vp8_clip8(yg + vp8_mulhi(v, 26149) - 14234);
   *g  = vp8_clip8(yg - vp8_mulhi(u, 6419) - vp8_mulhi(v, 13320) + 8708);
   *bo = vp8_clip8(yg + vp8_mulhi(u, 33050) - 17685);
}

/* ---- Row YUV -> ARGB/ABGR conversion ----
 * Converts a full row of co-sited (already chroma-interpolated) YUV
 * samples to 0xFFRRGGBB words (or memory-order R,G,B,A words when
 * swap_rb is set: the order is selected at store time for free, which
 * lets callers skip a whole-image post-pass swizzle). The scalar body
 * matches vp8_yuv2rgb
 * exactly; the SSE2/NEON paths reproduce it bit-for-bit by loading
 * samples into the upper byte of each 16-bit lane, so an unsigned
 * high-multiply computes (x * coeff) >> 8 with the same truncation
 * (the same construction libwebp's yuv_sse2.c uses). */

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define RWEBP_YUV_SSE2 1
#include <emmintrin.h>
#endif
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#define RWEBP_YUV_NEON 1
#include <arm_neon.h>
#endif

static void vp8_yuv2rgb_row(const uint8_t *y, const uint8_t *u,
      const uint8_t *v, uint32_t *dst, int len, int swap_rb)
{
   int i = 0;

#if defined(RWEBP_YUV_SSE2)
   {
      const __m128i k19077 = _mm_set1_epi16(19077);
      const __m128i k26149 = _mm_set1_epi16(26149);
      const __m128i k14234 = _mm_set1_epi16(14234);
      /* 33050 does not fit in a signed short: unsigned arithmetic only */
      const __m128i k33050 = _mm_set1_epi16((short)33050);
      const __m128i k17685 = _mm_set1_epi16(17685);
      const __m128i k6419  = _mm_set1_epi16(6419);
      const __m128i k13320 = _mm_set1_epi16(13320);
      const __m128i k8708  = _mm_set1_epi16(8708);
      const __m128i zero   = _mm_setzero_si128();
      const __m128i alpha  = _mm_set1_epi16(255);

      for (; i + 8 <= len; i += 8)
      {
         /* load into the UPPER byte of each lane: value << 8 */
         __m128i Y0 = _mm_unpacklo_epi8(zero, _mm_loadl_epi64((const __m128i*)(y + i)));
         __m128i U0 = _mm_unpacklo_epi8(zero, _mm_loadl_epi64((const __m128i*)(u + i)));
         __m128i V0 = _mm_unpacklo_epi8(zero, _mm_loadl_epi64((const __m128i*)(v + i)));
         __m128i Y1 = _mm_mulhi_epu16(Y0, k19077);
         __m128i R2 = _mm_add_epi16(_mm_sub_epi16(Y1, k14234),
                                    _mm_mulhi_epu16(V0, k26149));
         __m128i G4 = _mm_sub_epi16(_mm_add_epi16(Y1, k8708),
                                    _mm_add_epi16(_mm_mulhi_epu16(U0, k6419),
                                                  _mm_mulhi_epu16(V0, k13320)));
         /* B path saturates in unsigned 16-bit, then logical shift */
         __m128i B2 = _mm_subs_epu16(_mm_adds_epu16(_mm_mulhi_epu16(U0, k33050), Y1),
                                     k17685);
         __m128i R  = _mm_srai_epi16(R2, 6);
         __m128i G  = _mm_srai_epi16(G4, 6);
         __m128i B  = _mm_srli_epi16(B2, 6);
         /* pack to words 0xFFrrggbb (memory order b,g,r,FF), or with
          * R and B lanes exchanged (memory order r,g,b,FF) when
          * swap_rb - channel order costs nothing at store time */
         __m128i r8 = _mm_packus_epi16(R, R);
         __m128i g8 = _mm_packus_epi16(G, G);
         __m128i b8 = _mm_packus_epi16(B, B);
         __m128i a8 = _mm_packus_epi16(alpha, alpha);
         __m128i bg = _mm_unpacklo_epi8(swap_rb ? r8 : b8, g8);
         __m128i ra = _mm_unpacklo_epi8(swap_rb ? b8 : r8, a8);
         _mm_storeu_si128((__m128i*)(dst + i),     _mm_unpacklo_epi16(bg, ra));
         _mm_storeu_si128((__m128i*)(dst + i + 4), _mm_unpackhi_epi16(bg, ra));
      }
   }
#elif defined(RWEBP_YUV_NEON)
   {
      const uint16x4_t c19077 = vdup_n_u16(19077);
      const uint16x4_t c26149 = vdup_n_u16(26149);
      const uint16x4_t c6419  = vdup_n_u16(6419);
      const uint16x4_t c13320 = vdup_n_u16(13320);
      const uint16x4_t c33050 = vdup_n_u16(33050);

      for (; i + 8 <= len; i += 8)
      {
         uint16x8_t Y0, U0, V0, Y1, R0, G0, G1, B0, B2;
         int16x8_t  R2, G4;
         uint8x8_t  r8, b8;
         uint8x8x4_t px;
         Y0 = vshll_n_u8(vld1_u8(y + i), 8);
         U0 = vshll_n_u8(vld1_u8(u + i), 8);
         V0 = vshll_n_u8(vld1_u8(v + i), 8);
#define RWEBP_MH8(A, C) \
         vcombine_u16(vshrn_n_u32(vmull_u16(vget_low_u16(A),  (C)), 16), \
                      vshrn_n_u32(vmull_u16(vget_high_u16(A), (C)), 16))
         Y1 = RWEBP_MH8(Y0, c19077);
         R0 = RWEBP_MH8(V0, c26149);
         G0 = RWEBP_MH8(U0, c6419);
         G1 = RWEBP_MH8(V0, c13320);
         B0 = RWEBP_MH8(U0, c33050);
#undef RWEBP_MH8
         R2 = vaddq_s16(vsubq_s16(vreinterpretq_s16_u16(Y1), vdupq_n_s16(14234)),
                        vreinterpretq_s16_u16(R0));
         G4 = vsubq_s16(vaddq_s16(vreinterpretq_s16_u16(Y1), vdupq_n_s16(8708)),
                        vreinterpretq_s16_u16(vaddq_u16(G0, G1)));
         B2 = vqsubq_u16(vqaddq_u16(B0, Y1), vdupq_n_u16(17685));
         b8 = vqmovn_u16(vshrq_n_u16(B2, 6));
         r8 = vqshrun_n_s16(R2, 6);
         px.val[0] = swap_rb ? r8 : b8;                /* b (or r) */
         px.val[1] = vqshrun_n_s16(G4, 6);             /* g        */
         px.val[2] = swap_rb ? b8 : r8;                /* r (or b) */
         px.val[3] = vdup_n_u8(255);                   /* a        */
         vst4_u8((uint8_t*)(dst + i), px);
      }
   }
#endif

   {
      unsigned rs = swap_rb ? 0 : 16;
      unsigned bs = swap_rb ? 16 : 0;
      for (; i < len; i++)
      {
         uint8_t r, g, b2;
         vp8_yuv2rgb(y[i], u[i], v[i], &r, &g, &b2);
         dst[i] = 0xFF000000u | ((uint32_t)r << rs)
                | ((uint32_t)g << 8) | ((uint32_t)b2 << bs);
      }
   }
}

/* Fancy chroma upsampling (libwebp upsampling.c): interpolate the chroma
 * plane bilinearly with the 9-3-3-1 diagonal scheme while converting a
 * pair of luma rows. top/cur are chroma rows; either may alias for the
 * mirrored first and last rows. bot_y may be NULL (single-row case). */
/* Fill one output row's interpolated U and V samples (co-sited with the
 * luma row) into scratch, following the exact 9-3-3-1 fancy scheme, then
 * hand the co-sited Y/U/V to the vectorized row converter. Splitting the
 * (serial) chroma interpolation from the (uniform) colour conversion lets
 * the expensive conversion run 8 pixels at a time while staying bit-exact
 * with the original fused loop. tu/tv select which diagonal pair each
 * half of the interpolation uses for this row (top vs bottom of the pair). */
/* One channel of one interpolated chroma output row. Row A is the row
 * co-sited with the luma row being converted (weighted 3x / 9x in the
 * scheme), row B the other row of the pair: the 'top' output row of a
 * pair is fancy_uv_channel(top, cur, ...) and the 'bottom' row is
 * fancy_uv_channel(cur, top, ...) - the 9-3-3-1 scheme is symmetric
 * under swapping the rows, diagonals included (d12 and d03 exchange
 * roles), and the edge samples follow the same swap.
 *
 * The SIMD paths compute the identical 16-bit integer expressions as
 * the scalar loop (no averaging-instruction shortcuts), so they are
 * bit-exact by construction: all intermediates fit u16
 * (avg <= 1028, avg + 2*(x+y) <= 2048, diagonals <= 256,
 * diag + sample <= 511). */
static void vp8_fancy_uv_channel(const uint8_t *A, const uint8_t *B,
      uint8_t *d, int len)
{
   int x = 1, last_pair = (len - 1) >> 1;

   d[0] = (uint8_t)((3*A[0] + B[0] + 2) >> 2);

#if defined(RWEBP_YUV_SSE2)
   {
      const __m128i zero = _mm_setzero_si128();
      const __m128i k8   = _mm_set1_epi16(8);
      for (; x + 7 <= last_pair; x += 8)
      {
         __m128i a0 = _mm_unpacklo_epi8(
               _mm_loadl_epi64((const __m128i*)(A + x - 1)), zero);
         __m128i a1 = _mm_unpacklo_epi8(
               _mm_loadl_epi64((const __m128i*)(A + x)), zero);
         __m128i b0 = _mm_unpacklo_epi8(
               _mm_loadl_epi64((const __m128i*)(B + x - 1)), zero);
         __m128i b1 = _mm_unpacklo_epi8(
               _mm_loadl_epi64((const __m128i*)(B + x)), zero);
         __m128i avg = _mm_add_epi16(_mm_add_epi16(a0, a1),
               _mm_add_epi16(_mm_add_epi16(b0, b1), k8));
         __m128i d12 = _mm_srli_epi16(_mm_add_epi16(avg,
               _mm_slli_epi16(_mm_add_epi16(a1, b0), 1)), 3);
         __m128i d03 = _mm_srli_epi16(_mm_add_epi16(avg,
               _mm_slli_epi16(_mm_add_epi16(a0, b1), 1)), 3);
         __m128i odd  = _mm_srli_epi16(_mm_add_epi16(d12, a0), 1);
         __m128i even = _mm_srli_epi16(_mm_add_epi16(d03, a1), 1);
         _mm_storeu_si128((__m128i*)(d + 2*x - 1),
               _mm_unpacklo_epi8(_mm_packus_epi16(odd, odd),
                                 _mm_packus_epi16(even, even)));
      }
   }
#elif defined(RWEBP_YUV_NEON)
   for (; x + 7 <= last_pair; x += 8)
   {
      uint16x8_t a0 = vmovl_u8(vld1_u8(A + x - 1));
      uint16x8_t a1 = vmovl_u8(vld1_u8(A + x));
      uint16x8_t b0 = vmovl_u8(vld1_u8(B + x - 1));
      uint16x8_t b1 = vmovl_u8(vld1_u8(B + x));
      uint16x8_t avg = vaddq_u16(vaddq_u16(a0, a1),
            vaddq_u16(vaddq_u16(b0, b1), vdupq_n_u16(8)));
      uint16x8_t d12 = vshrq_n_u16(vaddq_u16(avg,
            vshlq_n_u16(vaddq_u16(a1, b0), 1)), 3);
      uint16x8_t d03 = vshrq_n_u16(vaddq_u16(avg,
            vshlq_n_u16(vaddq_u16(a0, b1), 1)), 3);
      uint8x8x2_t oe;
      oe.val[0] = vmovn_u16(vshrq_n_u16(vaddq_u16(d12, a0), 1));
      oe.val[1] = vmovn_u16(vshrq_n_u16(vaddq_u16(d03, a1), 1));
      vst2_u8(d + 2*x - 1, oe);
   }
#endif

   for (; x <= last_pair; x++)
   {
      int tl = A[x-1], t = A[x];
      int l  = B[x-1], c = B[x];
      int avg = tl + t + l + c + 8;
      int d12 = (avg + 2*(t + l))  >> 3;
      int d03 = (avg + 2*(tl + c)) >> 3;
      d[2*x-1] = (uint8_t)((d12 + tl) >> 1);
      d[2*x]   = (uint8_t)((d03 + t)  >> 1);
   }
   if (!(len & 1))
      d[len-1] = (uint8_t)((3*A[last_pair] + B[last_pair] + 2) >> 2);
}

static void vp8_fancy_uv_top(const uint8_t *top_u, const uint8_t *top_v,
      const uint8_t *cur_u, const uint8_t *cur_v,
      uint8_t *du, uint8_t *dv, int len)
{
   vp8_fancy_uv_channel(top_u, cur_u, du, len);
   vp8_fancy_uv_channel(top_v, cur_v, dv, len);
}

static void vp8_fancy_uv_bot(const uint8_t *top_u, const uint8_t *top_v,
      const uint8_t *cur_u, const uint8_t *cur_v,
      uint8_t *du, uint8_t *dv, int len)
{
   vp8_fancy_uv_channel(cur_u, top_u, du, len);
   vp8_fancy_uv_channel(cur_v, top_v, dv, len);
}

/* scratch: caller-provided buffer of 2*len bytes for the interpolated
 * chroma rows (kept per decode so concurrent decodes cannot interfere);
 * when NULL (allocation failed upstream) a fused scalar path that needs
 * no scratch is used instead. */
static void vp8_fancy_pair(const uint8_t *top_y, const uint8_t *bot_y,
      const uint8_t *top_u, const uint8_t *top_v,
      const uint8_t *cur_u, const uint8_t *cur_v,
      uint32_t *top_dst, uint32_t *bot_dst, int len,
      uint8_t *scratch, int swap_rb)
{
   uint8_t *du, *dv;
   if (len <= 0)
      return;
   if (!scratch)
   {
      {
         /* No scratch (allocation failed upstream): fused scalar path.
          * rsh/bsh select the channel order at store time (see
          * vp8_yuv2rgb_row). */
         int x, last_pair = (len - 1) >> 1;
         int tl_u = top_u[0], tl_v = top_v[0];
         int l_u = cur_u[0], l_v = cur_v[0];
         unsigned rsh = swap_rb ? 0 : 16;
         unsigned bsh = swap_rb ? 16 : 0;
         uint8_t r, g, b2;
         vp8_yuv2rgb(top_y[0], (3*tl_u+l_u+2)>>2, (3*tl_v+l_v+2)>>2, &r, &g, &b2);
         top_dst[0] = 0xFF000000u | ((uint32_t)r<<rsh) | ((uint32_t)g<<8) | ((uint32_t)b2<<bsh);
         if (bot_y)
         {
            vp8_yuv2rgb(bot_y[0], (3*l_u+tl_u+2)>>2, (3*l_v+tl_v+2)>>2, &r, &g, &b2);
            bot_dst[0] = 0xFF000000u | ((uint32_t)r<<rsh) | ((uint32_t)g<<8) | ((uint32_t)b2<<bsh);
         }
         for (x = 1; x <= last_pair; x++)
         {
            int t_u = top_u[x], t_v = top_v[x];
            int c_u = cur_u[x], c_v = cur_v[x];
            int avg_u = tl_u+t_u+l_u+c_u+8, avg_v = tl_v+t_v+l_v+c_v+8;
            int d12_u=(avg_u+2*(t_u+l_u))>>3, d12_v=(avg_v+2*(t_v+l_v))>>3;
            int d03_u=(avg_u+2*(tl_u+c_u))>>3, d03_v=(avg_v+2*(tl_v+c_v))>>3;
            vp8_yuv2rgb(top_y[2*x-1],(d12_u+tl_u)>>1,(d12_v+tl_v)>>1,&r,&g,&b2);
            top_dst[2*x-1]=0xFF000000u|((uint32_t)r<<rsh)|((uint32_t)g<<8)|((uint32_t)b2<<bsh);
            vp8_yuv2rgb(top_y[2*x],(d03_u+t_u)>>1,(d03_v+t_v)>>1,&r,&g,&b2);
            top_dst[2*x]=0xFF000000u|((uint32_t)r<<rsh)|((uint32_t)g<<8)|((uint32_t)b2<<bsh);
            if (bot_y)
            {
               vp8_yuv2rgb(bot_y[2*x-1],(d03_u+l_u)>>1,(d03_v+l_v)>>1,&r,&g,&b2);
               bot_dst[2*x-1]=0xFF000000u|((uint32_t)r<<rsh)|((uint32_t)g<<8)|((uint32_t)b2<<bsh);
               vp8_yuv2rgb(bot_y[2*x],(d12_u+c_u)>>1,(d12_v+c_v)>>1,&r,&g,&b2);
               bot_dst[2*x]=0xFF000000u|((uint32_t)r<<rsh)|((uint32_t)g<<8)|((uint32_t)b2<<bsh);
            }
            tl_u=t_u; tl_v=t_v; l_u=c_u; l_v=c_v;
         }
         if (!(len & 1))
         {
            vp8_yuv2rgb(top_y[len-1],(3*tl_u+l_u+2)>>2,(3*tl_v+l_v+2)>>2,&r,&g,&b2);
            top_dst[len-1]=0xFF000000u|((uint32_t)r<<rsh)|((uint32_t)g<<8)|((uint32_t)b2<<bsh);
            if (bot_y)
            {
               vp8_yuv2rgb(bot_y[len-1],(3*l_u+tl_u+2)>>2,(3*l_v+tl_v+2)>>2,&r,&g,&b2);
               bot_dst[len-1]=0xFF000000u|((uint32_t)r<<rsh)|((uint32_t)g<<8)|((uint32_t)b2<<bsh);
            }
         }
         return;
      }
   }
   du = scratch;
   dv = scratch + len;

   vp8_fancy_uv_top(top_u, top_v, cur_u, cur_v, du, dv, len);
   vp8_yuv2rgb_row(top_y, du, dv, top_dst, len, swap_rb);
   if (bot_y)
   {
      vp8_fancy_uv_bot(top_u, top_v, cur_u, cur_v, du, dv, len);
      vp8_yuv2rgb_row(bot_y, du, dv, bot_dst, len, swap_rb);
   }
}

/* Coefficient tables */
static const uint8_t vp8_bands[16 + 1] = {0,1,2,3,6,4,5,6,6,6,6,6,6,6,6,7, 0};
static const uint8_t vp8_zigzag[16] = {0,1,4,8,5,2,3,6,9,12,13,10,7,11,14,15};

static const int16_t vp8_dc_qlut[128] = {
   4,5,6,7,8,9,10,10,11,12,13,14,15,16,17,17,18,19,20,20,21,21,22,22,23,23,24,25,25,26,27,28,
   29,30,31,32,33,34,35,36,37,37,38,39,40,41,42,43,44,45,46,46,47,48,49,50,51,52,53,54,55,56,57,58,
   59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,76,77,78,79,80,81,82,83,84,85,86,87,88,89,
   91,93,95,96,98,100,101,102,104,106,108,110,112,114,116,118,122,124,126,128,130,132,134,136,138,140,143,145,148,151,154,157};
static const int16_t vp8_ac_qlut[128] = {
   4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,
   36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,60,62,64,66,68,70,72,74,76,
   78,80,82,84,86,88,90,92,94,96,98,100,102,104,106,108,110,112,114,116,119,122,125,128,131,134,137,140,143,146,149,152,
   155,158,161,164,167,170,173,177,181,185,189,193,197,201,205,209,213,217,221,225,229,234,239,245,249,254,259,264,269,274,279,284};

/* The canonical VP8 default coefficient probabilities (RFC 6386 s13.5):
 * the full 4 block-types * 8 bands * 3 contexts * 11 tree-node table,
 * 1056 bytes, embedded directly. A key frame resets to these before any
 * per-frame updates from the bitstream are applied. */
static void vp8_init_default_cprob(uint8_t dst[4][8][3][11])
{
   static const uint8_t def[4][8][3][11] = {
   {
    {{128,128,128,128,128,128,128,128,128,128,128},{128,128,128,128,128,128,128,128,128,128,128},{128,128,128,128,128,128,128,128,128,128,128}},
    {{253,136,254,255,228,219,128,128,128,128,128},{189,129,242,255,227,213,255,219,128,128,128},{106,126,227,252,214,209,255,255,128,128,128}},
    {{1,98,248,255,236,226,255,255,128,128,128},{181,133,238,254,221,234,255,154,128,128,128},{78,134,202,247,198,180,255,219,128,128,128}},
    {{1,185,249,255,243,255,128,128,128,128,128},{184,150,247,255,236,224,128,128,128,128,128},{77,110,216,255,236,230,128,128,128,128,128}},
    {{1,101,251,255,241,255,128,128,128,128,128},{170,139,241,252,236,209,255,255,128,128,128},{37,116,196,243,228,255,255,255,128,128,128}},
    {{1,204,254,255,245,255,128,128,128,128,128},{207,160,250,255,238,128,128,128,128,128,128},{102,103,231,255,211,171,128,128,128,128,128}},
    {{1,152,252,255,240,255,128,128,128,128,128},{177,135,243,255,234,225,128,128,128,128,128},{80,129,211,255,194,224,128,128,128,128,128}},
    {{1,1,255,128,128,128,128,128,128,128,128},{246,1,255,128,128,128,128,128,128,128,128},{255,128,128,128,128,128,128,128,128,128,128}},
   },
   {
    {{198,35,237,223,193,187,162,160,145,155,62},{131,45,198,221,172,176,220,157,252,221,1},{68,47,146,208,149,167,221,162,255,223,128}},
    {{1,149,241,255,221,224,255,255,128,128,128},{184,141,234,253,222,220,255,199,128,128,128},{81,99,181,242,176,190,249,202,255,255,128}},
    {{1,129,232,253,214,197,242,196,255,255,128},{99,121,210,250,201,198,255,202,128,128,128},{23,91,163,242,170,187,247,210,255,255,128}},
    {{1,200,246,255,234,255,128,128,128,128,128},{109,178,241,255,231,245,255,255,128,128,128},{44,130,201,253,205,192,255,255,128,128,128}},
    {{1,132,239,251,219,209,255,165,128,128,128},{94,136,225,251,218,190,255,255,128,128,128},{22,100,174,245,186,161,255,199,128,128,128}},
    {{1,182,249,255,232,235,128,128,128,128,128},{124,143,241,255,227,234,128,128,128,128,128},{35,77,181,251,193,211,255,205,128,128,128}},
    {{1,157,247,255,236,231,255,255,128,128,128},{121,141,235,255,225,227,255,255,128,128,128},{45,99,188,251,195,217,255,224,128,128,128}},
    {{1,1,251,255,213,255,128,128,128,128,128},{203,1,248,255,255,128,128,128,128,128,128},{137,1,177,255,224,255,128,128,128,128,128}},
   },
   {
    {{253,9,248,251,207,208,255,192,128,128,128},{175,13,224,243,193,185,249,198,255,255,128},{73,17,171,221,161,179,236,167,255,234,128}},
    {{1,95,247,253,212,183,255,255,128,128,128},{239,90,244,250,211,209,255,255,128,128,128},{155,77,195,248,188,195,255,255,128,128,128}},
    {{1,24,239,251,218,219,255,205,128,128,128},{201,51,219,255,196,186,128,128,128,128,128},{69,46,190,239,201,218,255,228,128,128,128}},
    {{1,191,251,255,255,128,128,128,128,128,128},{223,165,249,255,213,255,128,128,128,128,128},{141,124,248,255,255,128,128,128,128,128,128}},
    {{1,16,248,255,255,128,128,128,128,128,128},{190,36,230,255,236,255,128,128,128,128,128},{149,1,255,128,128,128,128,128,128,128,128}},
    {{1,226,255,128,128,128,128,128,128,128,128},{247,192,255,128,128,128,128,128,128,128,128},{240,128,255,128,128,128,128,128,128,128,128}},
    {{1,134,252,255,255,128,128,128,128,128,128},{213,62,250,255,255,128,128,128,128,128,128},{55,93,255,128,128,128,128,128,128,128,128}},
    {{128,128,128,128,128,128,128,128,128,128,128},{128,128,128,128,128,128,128,128,128,128,128},{128,128,128,128,128,128,128,128,128,128,128}},
   },
   {
    {{202,24,213,235,186,191,220,160,240,175,255},{126,38,182,232,169,184,228,174,255,187,128},{61,46,138,219,151,178,240,170,255,216,128}},
    {{1,112,230,250,199,191,247,159,255,255,128},{166,109,228,252,211,215,255,174,128,128,128},{39,77,162,232,172,180,245,178,255,255,128}},
    {{1,52,220,246,198,199,249,220,255,255,128},{124,74,191,243,183,193,250,221,255,255,128},{24,71,130,219,154,170,243,182,255,255,128}},
    {{1,182,225,249,219,240,255,224,128,128,128},{149,150,226,252,216,205,255,171,128,128,128},{28,108,170,242,183,194,254,223,255,255,128}},
    {{1,81,230,252,204,203,255,192,128,128,128},{123,102,209,247,188,196,255,233,128,128,128},{20,95,153,243,164,173,255,203,128,128,128}},
    {{1,222,248,255,216,213,128,128,128,128,128},{168,175,246,252,235,205,255,255,128,128,128},{47,116,215,255,211,212,255,255,128,128,128}},
    {{1,121,236,253,212,214,255,255,128,128,128},{141,84,213,252,201,202,255,219,128,128,128},{42,80,160,240,162,185,255,205,128,128,128}},
    {{1,1,255,128,128,128,128,128,128,128,128},{244,1,255,128,128,128,128,128,128,128,128},{238,1,255,128,128,128,128,128,128,128,128}},
   },
   };
   memcpy(dst, def, sizeof(def));
}

/* Decode one 4x4 block of DCT coefficients (matching libvpx GetCoeffs).
 * init_ctx: initial probability context from neighbor non-zero status
 * Returns the position of the last non-zero coeff + 1 (0 if all zero). */
static int vp8_decode_block(rvp8_bool *br, int16_t coeffs[16],
      uint8_t probs[8][3][11], int start_at, int init_ctx)
{
   static const uint8_t kCat3[] = {173,148,140};
   static const uint8_t kCat4[] = {176,155,140,135};
   static const uint8_t kCat5[] = {180,157,141,134,130};
   static const uint8_t kCat6[] = {254,254,243,230,196,177,153,140,133,130,129};
   int n = start_at;
   const uint8_t *p = probs[n][init_ctx];
   memset(coeffs, 0, 16 * sizeof(int16_t));

   /* First "CBP" bit: EOB for entire block */
   if (!vp8b_get(br, p[0]))
      return 0;

   for (;;)
   {
      int v;
      ++n;
      if (!vp8b_get(br, p[1]))
      {
         /* zero coefficient */
         p = probs[vp8_bands[n]][0];
      }
      else
      {
         /* non-zero coefficient */
         if (!vp8b_get(br, p[2]))
         {
            v = 1;
            p = probs[vp8_bands[n]][1];
         }
         else
         {
            if (!vp8b_get(br, p[3]))
            {
               if (!vp8b_get(br, p[4]))
                  v = 2;
               else
                  v = 3 + vp8b_get(br, p[5]);
            }
            else
            {
               if (!vp8b_get(br, p[6]))
               {
                  if (!vp8b_get(br, p[7]))
                     v = 5 + vp8b_get(br, 159);
                  else
                  {
                     v = 7 + 2 * vp8b_get(br, 165);
                     v += vp8b_get(br, 145);
                  }
               }
               else
               {
                  int bit1 = vp8b_get(br, p[8]);
                  int bit0 = vp8b_get(br, p[9 + bit1]);
                  int cat = 2 * bit1 + bit0, k;
                  v = 0;
                  if (cat == 0) { for(k=0;k<3;k++) v = v*2 + vp8b_get(br, kCat3[k]); v += 11; }
                  else if (cat == 1) { for(k=0;k<4;k++) v = v*2 + vp8b_get(br, kCat4[k]); v += 19; }
                  else if (cat == 2) { for(k=0;k<5;k++) v = v*2 + vp8b_get(br, kCat5[k]); v += 35; }
                  else { for(k=0;k<11;k++) v = v*2 + vp8b_get(br, kCat6[k]); v += 67; }
               }
            }
            p = probs[vp8_bands[n]][2];
         }
         /* Sign bit and store */
         coeffs[vp8_zigzag[n-1]] = (int16_t)(vp8b_get(br, 128) ? -v : v);

         if (n == 16 || !vp8b_get(br, p[0])) /* EOB */
            return n;
      }
      if (n == 16)
         return 16;
   }
}

/* VP8 4x4 inverse DCT (from RFC 6386 §14.3) */
/* VP8 4x4 inverse DCT + add. Pass order matches libvpx
 * vp8_short_idct4x4llm_c: columns first, then rows with final rounding.
 * The >>16 truncations do not commute, so pass order matters, and the
 * pass-1 intermediate is 16-bit (libvpx stores it as short).
 *
 * The 35468 constant exceeds the signed-16 range, so both the SSE2 and
 * NEON paths compute (x*35468)>>16 as x + ((x * (int16)0x8A8C) >> 16):
 * 0x8A8C is 35468 - 65536, and adding x back recovers the unsigned
 * coefficient exactly (verified against the scalar form). 20091 fits in
 * signed 16 and multiplies directly. */
#if defined(RWEBP_YUV_SSE2)
#define RWEBP_IDCT_SSE2 1
#endif
#if defined(RWEBP_YUV_NEON)
#define RWEBP_IDCT_NEON 1
#endif

#if defined(RWEBP_IDCT_SSE2)
static void vp8_idct4x4_add(const int16_t in[16], uint8_t *dst, int stride)
{
   const __m128i k35  = _mm_set1_epi16((short)0x8A8C);
   const __m128i k20  = _mm_set1_epi16((short)20091);
   const __m128i four = _mm_set1_epi16(4);
   __m128i R0 = _mm_loadl_epi64((const __m128i*)(in + 0));
   __m128i R1 = _mm_loadl_epi64((const __m128i*)(in + 4));
   __m128i R2 = _mm_loadl_epi64((const __m128i*)(in + 8));
   __m128i R3 = _mm_loadl_epi64((const __m128i*)(in + 12));
   __m128i a, b, c, d, T0, T1, T2, T3;
   __m128i ua, ub, tl, th;
   int i;
   int16_t O0[8], O1[8], O2[8], O3[8];
#define RWEBP_MH35(x) _mm_add_epi16((x), _mm_mulhi_epi16((x), k35))
#define RWEBP_MH20(x) _mm_mulhi_epi16((x), k20)
   a = _mm_add_epi16(R0, R2);
   b = _mm_sub_epi16(R0, R2);
   c = _mm_sub_epi16(RWEBP_MH35(R1), _mm_add_epi16(R3, RWEBP_MH20(R3)));
   d = _mm_add_epi16(_mm_add_epi16(R1, RWEBP_MH20(R1)), RWEBP_MH35(R3));
   T0 = _mm_add_epi16(a, d);
   T1 = _mm_add_epi16(b, c);
   T2 = _mm_sub_epi16(b, c);
   T3 = _mm_sub_epi16(a, d);
   /* transpose the four rows (only low 4 lanes are live) */
   ua = _mm_unpacklo_epi16(T0, T1);
   ub = _mm_unpacklo_epi16(T2, T3);
   tl = _mm_unpacklo_epi32(ua, ub);
   th = _mm_unpackhi_epi32(ua, ub);
   T0 = tl;
   T1 = _mm_srli_si128(tl, 8);
   T2 = th;
   T3 = _mm_srli_si128(th, 8);
   a = _mm_add_epi16(T0, T2);
   b = _mm_sub_epi16(T0, T2);
   c = _mm_sub_epi16(RWEBP_MH35(T1), _mm_add_epi16(T3, RWEBP_MH20(T3)));
   d = _mm_add_epi16(_mm_add_epi16(T1, RWEBP_MH20(T1)), RWEBP_MH35(T3));
#undef RWEBP_MH35
#undef RWEBP_MH20
   _mm_storeu_si128((__m128i*)O0, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(a, d), four), 3));
   _mm_storeu_si128((__m128i*)O1, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(b, c), four), 3));
   _mm_storeu_si128((__m128i*)O2, _mm_srai_epi16(_mm_add_epi16(_mm_sub_epi16(b, c), four), 3));
   _mm_storeu_si128((__m128i*)O3, _mm_srai_epi16(_mm_add_epi16(_mm_sub_epi16(a, d), four), 3));
   for (i = 0; i < 4; i++)
   {
      dst[i*stride+0] = vp8_cl(dst[i*stride+0] + O0[i]);
      dst[i*stride+1] = vp8_cl(dst[i*stride+1] + O1[i]);
      dst[i*stride+2] = vp8_cl(dst[i*stride+2] + O2[i]);
      dst[i*stride+3] = vp8_cl(dst[i*stride+3] + O3[i]);
   }
}
#elif defined(RWEBP_IDCT_NEON)
static INLINE int16x8_t rwebp_idct_mh(int16x8_t x, int16_t c)
{
   int32x4_t lo = vmull_n_s16(vget_low_s16(x),  c);
   int32x4_t hi = vmull_n_s16(vget_high_s16(x), c);
   return vcombine_s16(vshrn_n_s32(lo, 16), vshrn_n_s32(hi, 16));
}
static void vp8_idct4x4_add(const int16_t in[16], uint8_t *dst, int stride)
{
   int16x4_t r0 = vld1_s16(in), r1 = vld1_s16(in+4);
   int16x4_t r2 = vld1_s16(in+8), r3 = vld1_s16(in+12);
   int16x8_t R0 = vcombine_s16(r0, r0), R1 = vcombine_s16(r1, r1);
   int16x8_t R2 = vcombine_s16(r2, r2), R3 = vcombine_s16(r3, r3);
   int16x8_t a, b, c, d;
   int16x4_t T0, T1, T2, T3, o0, o1, o2, o3;
   int16x4x2_t p, q; int32x2x2_t s, u;
   int16x8_t W0, W1, W2, W3;
   int16_t O0[4], O1[4], O2[4], O3[4]; int i;
#define RWEBP_MH35(x) vaddq_s16((x), rwebp_idct_mh((x), (int16_t)0x8A8C))
#define RWEBP_MH20(x) rwebp_idct_mh((x), 20091)
   a = vaddq_s16(R0, R2);
   b = vsubq_s16(R0, R2);
   c = vsubq_s16(RWEBP_MH35(R1), vaddq_s16(R3, RWEBP_MH20(R3)));
   d = vaddq_s16(vaddq_s16(R1, RWEBP_MH20(R1)), RWEBP_MH35(R3));
   T0 = vget_low_s16(vaddq_s16(a, d));
   T1 = vget_low_s16(vaddq_s16(b, c));
   T2 = vget_low_s16(vsubq_s16(b, c));
   T3 = vget_low_s16(vsubq_s16(a, d));
   p = vtrn_s16(T0, T1);
   q = vtrn_s16(T2, T3);
   s = vtrn_s32(vreinterpret_s32_s16(p.val[0]), vreinterpret_s32_s16(q.val[0]));
   u = vtrn_s32(vreinterpret_s32_s16(p.val[1]), vreinterpret_s32_s16(q.val[1]));
   W0 = vcombine_s16(vreinterpret_s16_s32(s.val[0]), vreinterpret_s16_s32(s.val[0]));
   W2 = vcombine_s16(vreinterpret_s16_s32(s.val[1]), vreinterpret_s16_s32(s.val[1]));
   W1 = vcombine_s16(vreinterpret_s16_s32(u.val[0]), vreinterpret_s16_s32(u.val[0]));
   W3 = vcombine_s16(vreinterpret_s16_s32(u.val[1]), vreinterpret_s16_s32(u.val[1]));
   a = vaddq_s16(W0, W2);
   b = vsubq_s16(W0, W2);
   c = vsubq_s16(RWEBP_MH35(W1), vaddq_s16(W3, RWEBP_MH20(W3)));
   d = vaddq_s16(vaddq_s16(W1, RWEBP_MH20(W1)), RWEBP_MH35(W3));
#undef RWEBP_MH35
#undef RWEBP_MH20
   o0 = vget_low_s16(vshrq_n_s16(vaddq_s16(vaddq_s16(a, d), vdupq_n_s16(4)), 3));
   o1 = vget_low_s16(vshrq_n_s16(vaddq_s16(vaddq_s16(b, c), vdupq_n_s16(4)), 3));
   o2 = vget_low_s16(vshrq_n_s16(vaddq_s16(vsubq_s16(b, c), vdupq_n_s16(4)), 3));
   o3 = vget_low_s16(vshrq_n_s16(vaddq_s16(vsubq_s16(a, d), vdupq_n_s16(4)), 3));
   vst1_s16(O0, o0); vst1_s16(O1, o1); vst1_s16(O2, o2); vst1_s16(O3, o3);
   for (i = 0; i < 4; i++)
   {
      dst[i*stride+0] = vp8_cl(dst[i*stride+0] + O0[i]);
      dst[i*stride+1] = vp8_cl(dst[i*stride+1] + O1[i]);
      dst[i*stride+2] = vp8_cl(dst[i*stride+2] + O2[i]);
      dst[i*stride+3] = vp8_cl(dst[i*stride+3] + O3[i]);
   }
}
#else
static void vp8_idct4x4_add(const int16_t in[16], uint8_t *dst, int stride)
{
   int i;
   int16_t tmp[16];
   for (i = 0; i < 4; i++)
   {
      int a = in[i] + in[8+i];
      int b = in[i] - in[8+i];
      int c = (in[4+i] * 35468 >> 16) - (in[12+i] + (in[12+i] * 20091 >> 16));
      int d = (in[4+i] + (in[4+i] * 20091 >> 16)) + (in[12+i] * 35468 >> 16);
      tmp[i]    = (int16_t)(a + d); tmp[4+i]  = (int16_t)(b + c);
      tmp[8+i]  = (int16_t)(b - c); tmp[12+i] = (int16_t)(a - d);
   }
   for (i = 0; i < 4; i++)
   {
      int a = tmp[i*4+0] + tmp[i*4+2];
      int b = tmp[i*4+0] - tmp[i*4+2];
      int c = (tmp[i*4+1] * 35468 >> 16) - (tmp[i*4+3] + (tmp[i*4+3] * 20091 >> 16));
      int d = (tmp[i*4+1] + (tmp[i*4+1] * 20091 >> 16)) + (tmp[i*4+3] * 35468 >> 16);
      dst[i*stride+0] = vp8_cl(dst[i*stride+0] + ((a+d+4) >> 3));
      dst[i*stride+1] = vp8_cl(dst[i*stride+1] + ((b+c+4) >> 3));
      dst[i*stride+2] = vp8_cl(dst[i*stride+2] + ((b-c+4) >> 3));
      dst[i*stride+3] = vp8_cl(dst[i*stride+3] + ((a-d+4) >> 3));
   }
}
#endif

/* Inverse Walsh-Hadamard Transform for Y2 DC block.
 * Output goes directly as DC coefficients to the 4x4 IDCT,
 * so NO >>3 normalization here (IDCT applies its own). */
static void vp8_iwht4x4(const int16_t in[16], int16_t out[16])
{
   int i, tmp[16];
   for (i = 0; i < 4; i++)
   {
      int a = in[i*4+0]+in[i*4+3], b = in[i*4+1]+in[i*4+2];
      int c = in[i*4+1]-in[i*4+2], d = in[i*4+0]-in[i*4+3];
      tmp[i*4+0]=a+b; tmp[i*4+1]=c+d; tmp[i*4+2]=a-b; tmp[i*4+3]=d-c;
   }
   for (i = 0; i < 4; i++)
   {
      int a = tmp[i]+tmp[12+i], b = tmp[4+i]+tmp[8+i];
      int c = tmp[4+i]-tmp[8+i], d = tmp[i]-tmp[12+i];
      out[i]=(int16_t)((a+b+3)>>3); out[4+i]=(int16_t)((c+d+3)>>3);
      out[8+i]=(int16_t)((a-b+3)>>3); out[12+i]=(int16_t)((d-c+3)>>3);
   }
}

static const uint8_t vp8_ymp[4] = {145,156,163,128};
static const uint8_t vp8_uvmp[3] = {142,114,183};

/* ==================================================================== */
/* VP8 inter-frame decode (RFC 6386 sections 9.7-9.10, 16-18).          */
/* ==================================================================== */

/* Reference frame indices. */
#define RVP8_REF_LAST    1  /* matches decoded ref_frame values */
#define RVP8_REF_GOLDEN  2
#define RVP8_REF_ALTREF  3

/* Inter 16x16 Y prediction modes (after the intra/inter split). */
#define MV_ZERO   0   /* ZEROMV  */
#define MV_NEAREST 1  /* NEARESTMV */
#define MV_NEAR   2   /* NEARMV  */
#define MV_NEW    3   /* NEWMV   */
#define MV_SPLIT  4   /* SPLITMV */

typedef struct { int16_t x, y; } rvp8_mv;

/* Per-macroblock decoded info the MV predictor and reconstruction need. */
typedef struct
{
   uint8_t  ref_frame;   /* 0=intra, 1=last, 2=golden, 3=altref            */
   uint8_t  mode;        /* MV_ZERO/NEAREST/NEAR/NEW/SPLIT (inter) or intra */
   rvp8_mv  mv;          /* 16x16 motion vector (1/8-pel: whole = *8? no,  */
                         /* VP8 MVs are in quarter-pel*2 = eighth? see below*/
   rvp8_mv  bmv[16];     /* per-sub-block MVs for SPLITMV                    */
   uint8_t  is_split;
   uint8_t  bmodes[16];  /* B_PRED sub-block modes (intra-in-inter)         */
   uint8_t  uvmode;      /* chroma intra mode (intra-in-inter)              */
} rvp8_mbinfo;

/* 16x16 inter mode tree probabilities (RFC 6386 s16.1, vp8_mode_contexts
 * default row when no context; but VP8 uses per-context probs indexed by
 * near/nearest/zero counts). We use the mode_contexts table. */
static const uint8_t vp8_mode_contexts[6][4] = {
   {  7,   1,   1, 143 },
   { 14,  18,  14, 107 },
   {135,  64,  57,  68 },
   { 60,  56, 128,  65 },
   {159, 134, 128,  34 },
   {234, 188, 128,  28 }
};

/* MV component entropy: [is_short][...] per RFC 6386 s17.2.
 * Layout matches libvpx vp8_default_mv_context: for each of 2 components,
 * 19 probabilities. */
#define MVPCOUNT 19
static const uint8_t vp8_default_mv_context[2][MVPCOUNT] = {
   { /* row (y) */
      162, 128, 225, 146, 172, 147, 214, 39, 156,
      128, 129, 132,  75, 145, 178, 206, 239, 254, 254
   },
   { /* col (x) */
      164, 128, 204, 170, 119, 235, 140, 230, 228,
      128, 130, 130,  74, 148, 180, 203, 236, 254, 254
   }
};

/* MV update probabilities (RFC 6386 s17.2, vp8_mv_update_probs). */
static const uint8_t vp8_mv_update_probs[2][MVPCOUNT] = {
   {
      237, 246, 253, 253, 254, 254, 254, 254, 254,
      254, 254, 254, 254, 254, 250, 250, 252, 254, 254
   },
   {
      231, 243, 245, 253, 254, 254, 254, 254, 254,
      254, 254, 254, 254, 254, 251, 251, 254, 254, 254
   }
};

/* Long-vector bit-probability indices within the 19-entry array. */
#define MVPbits   9   /* offset of the 10 long-bit probs (mvlong_width=10) */
#define MVPshort  2   /* offset of the 7 short-tree probs                  */
#define mvlong_width 10
#define mvnum_short  8

/* 6-tap sub-pixel interpolation filters (RFC 6386 s16.2, table). Indexed
 * by the 3-bit fractional MV; each is a 6-tap kernel. */
static const int16_t vp8_sixtap_filters[8][6] = {
   { 0,  0, 128,   0,  0, 0 },
   { 0, -6, 123,  12, -1, 0 },
   { 2, -11, 108, 36, -8, 1 },
   { 0, -9,  93,  50, -6, 0 },
   { 3, -16, 77,  77, -16, 3 },
   { 0, -6,  50,  93, -9, 0 },
   { 1, -8,  36, 108, -11, 2 },
   { 0, -1,  12, 123, -6, 0 }
};

/* Bilinear filters (used when the frame header selects them; VP8 uses the
 * sixtap set for the "normal" filter and bilinear for the "simple"/version
 * variants). */
static const int16_t vp8_bilinear_filters[8][2] = {
   { 128,   0 }, { 112,  16 }, { 96, 32 }, { 80, 48 },
   { 64,  64 },  { 48,  80 },  { 32, 96 }, { 16, 112 }
};


/* Key-frame B_PRED sub-block mode probabilities (RFC 6386 §12.1)
 * Indexed by [above_bmode][left_bmode][tree_node 0..8] */
static const uint8_t kf_bmode_prob[10][10][9] = {
 {{231,120,48,89,115,113,120,152,112},{152,179,64,126,170,118,46,70,95},{175,69,143,80,85,82,72,155,103},{56,58,10,171,218,189,17,13,152},{144,71,10,38,171,213,144,34,26},{114,26,17,163,44,195,21,10,173},{121,24,80,195,26,62,44,64,85},{170,46,55,19,136,160,33,206,71},{63,20,8,114,114,208,12,9,226},{81,40,11,96,182,84,29,16,36}},
 {{134,183,89,137,98,101,106,165,148},{72,187,100,130,157,111,32,75,80},{66,102,167,99,74,62,40,234,128},{41,53,9,178,241,141,26,8,107},{104,79,12,27,217,255,87,17,7},{74,43,26,146,73,166,49,23,157},{65,38,105,160,51,52,31,115,128},{87,68,71,44,114,51,15,186,23},{47,41,14,110,182,183,21,17,194},{66,45,25,102,197,189,23,18,22}},
 {{88,88,147,150,42,46,45,196,205},{43,97,183,117,85,38,35,179,61},{39,53,200,87,26,21,43,232,171},{56,34,51,104,114,102,29,93,77},{107,54,32,26,51,1,81,43,31},{39,28,85,171,58,165,90,98,64},{34,22,116,206,23,34,43,166,73},{68,25,106,22,64,171,36,225,114},{34,19,21,102,132,188,16,76,124},{62,18,78,95,85,57,50,48,51}},
 {{193,101,35,159,215,111,89,46,111},{60,148,31,172,219,228,21,18,111},{112,113,77,85,179,255,38,120,114},{40,42,1,196,245,209,10,25,109},{100,80,8,43,154,1,51,26,71},{88,43,29,140,166,213,37,43,154},{61,63,30,155,67,45,68,1,209},{142,78,78,16,255,128,34,197,171},{41,40,5,102,211,183,4,1,221},{51,50,17,168,209,192,23,25,82}},
 {{125,98,42,88,104,85,117,175,82},{95,84,53,89,128,100,113,101,45},{75,79,123,47,51,128,81,171,1},{57,17,5,71,102,57,53,41,49},{115,21,2,10,102,255,166,23,6},{38,33,13,121,57,73,26,1,85},{41,10,67,138,77,110,90,47,114},{101,29,16,10,85,128,101,196,26},{57,18,10,102,102,213,34,20,43},{117,20,15,36,163,128,68,1,26}},
 {{138,31,36,171,27,166,38,44,229},{67,87,58,169,82,115,26,59,179},{63,59,90,180,59,166,93,73,154},{40,40,21,116,143,209,34,39,175},{57,46,22,24,128,1,54,17,37},{47,15,16,183,34,223,49,45,183},{46,17,33,183,6,98,15,32,183},{65,32,73,115,28,128,23,128,205},{40,3,9,115,51,192,18,6,223},{87,37,9,115,59,77,64,21,47}},
 {{104,55,44,218,9,54,53,130,226},{64,90,70,205,40,41,23,26,57},{54,57,112,184,5,41,38,166,213},{30,34,26,133,152,116,10,32,134},{75,32,12,51,192,255,160,43,51},{39,19,53,221,26,114,32,73,255},{31,9,65,234,2,15,1,118,73},{88,31,35,67,102,85,55,186,85},{56,21,23,111,59,205,45,37,192},{55,38,70,124,73,102,1,34,98}},
 {{102,61,71,37,34,53,31,243,192},{69,60,71,38,73,119,28,222,37},{68,45,128,34,1,47,11,245,171},{62,17,19,70,146,85,55,62,70},{75,15,9,9,64,255,184,119,16},{37,43,37,154,100,163,85,160,1},{63,9,92,136,28,64,32,201,85},{86,6,28,5,64,255,25,248,1},{56,8,17,132,137,255,55,116,128},{58,15,20,82,135,57,26,121,40}},
 {{164,50,31,137,154,133,25,35,218},{51,103,44,131,131,123,31,6,158},{86,40,64,135,148,224,45,183,128},{22,26,17,131,240,154,14,1,209},{83,12,13,54,192,255,68,47,28},{45,16,21,91,64,222,7,1,197},{56,21,39,155,60,138,23,102,213},{85,26,85,85,128,128,32,146,171},{18,11,7,63,144,171,4,4,246},{35,27,10,146,174,171,12,26,128}},
 {{190,80,35,99,180,80,126,54,45},{85,126,47,87,176,51,41,20,32},{101,75,128,139,118,146,116,128,85},{56,41,15,176,236,85,37,9,62},{146,36,19,30,171,255,97,27,20},{71,30,17,119,118,255,17,18,138},{101,38,60,138,55,70,43,26,142},{138,45,61,62,219,1,81,188,64},{32,41,20,117,151,142,20,21,163},{112,19,12,61,195,128,48,4,24}}
};

/* Decode a B_PRED sub-block mode from the key-frame tree (RFC 6386 §12.1) */
/* Inter-frame B_PRED sub-block mode: fixed (non-contextual) probabilities
 * vp8_bmode_prob, walked over the same bmode tree as the key-frame path. */
static const uint8_t rvp8_inter_bmode_prob[9] =
   { 120, 90, 79, 133, 87, 85, 80, 111, 151 };
static int vp8_read_bmode_inter(rvp8_bool *br)
{
   const uint8_t *p = rvp8_inter_bmode_prob;
   if (!vp8b_get(br, p[0])) return 0; /* B_DC_PRED */
   if (!vp8b_get(br, p[1])) return 1; /* B_TM_PRED */
   if (!vp8b_get(br, p[2])) return 2; /* B_VE_PRED */
   if (!vp8b_get(br, p[3])) {
      if (!vp8b_get(br, p[4])) return 3; /* B_HE_PRED */
      if (!vp8b_get(br, p[5])) return 5; /* B_RD_PRED */
      return 6; /* B_VR_PRED */
   } else {
      if (!vp8b_get(br, p[6])) return 4; /* B_LD_PRED */
      if (!vp8b_get(br, p[7])) return 7; /* B_VL_PRED */
      if (!vp8b_get(br, p[8])) return 8; /* B_HD_PRED */
      return 9; /* B_HU_PRED */
   }
}

static int vp8_read_bmode(rvp8_bool *br, int above, int left)
{
   const uint8_t *p = kf_bmode_prob[above][left];
   if (!vp8b_get(br, p[0])) return 0; /* B_DC_PRED */
   if (!vp8b_get(br, p[1])) return 1; /* B_TM_PRED */
   if (!vp8b_get(br, p[2])) return 2; /* B_VE_PRED */
   if (!vp8b_get(br, p[3])) {
      if (!vp8b_get(br, p[4])) return 3; /* B_HE_PRED */
      if (!vp8b_get(br, p[5])) return 5; /* B_RD_PRED */
      return 6; /* B_VR_PRED */
   } else {
      if (!vp8b_get(br, p[6])) return 4; /* B_LD_PRED */
      if (!vp8b_get(br, p[7])) return 7; /* B_VL_PRED */
      if (!vp8b_get(br, p[8])) return 8; /* B_HD_PRED */
      return 9; /* B_HU_PRED */
   }
}

/* 4x4 sub-block intra prediction for B_PRED.
 * dst: output 4x4 block, stride s.
 * a[0..7]: 8 above pixels (a[0..3]=directly above, a[4..7]=above-right)
 * l[0..3]: left pixels, tl: top-left pixel */
static void vp8_pred4x4(uint8_t *d, int s, int m,
      const uint8_t *a, const uint8_t *l, uint8_t tl)
{
   int i, j;
   switch (m)
   {
   case 0: /* B_DC_PRED */
   {  int sum=0;
      for(i=0;i<4;i++) sum+=a[i]+l[i];
      { uint8_t dc=(uint8_t)((sum+4)>>3);
        for(j=0;j<4;j++) memset(d+j*s,dc,4); }
      break;
   }
   case 1: /* B_TM_PRED */
      for(j=0;j<4;j++) for(i=0;i<4;i++)
         d[j*s+i]=vp8_cl((int)a[i]+(int)l[j]-(int)tl);
      break;
   case 2: /* B_VE_PRED (vertical/above with smoothing) */
      for(i=0;i<4;i++) {
         int v = (i==0) ? (tl+2*a[0]+a[1]+2)>>2 : (a[i-1]+2*a[i]+a[i+1]+2)>>2;
         for(j=0;j<4;j++) d[j*s+i]=(uint8_t)v;
      }
      break;
   case 3: /* B_HE_PRED (horizontal/left with smoothing) */
      for(j=0;j<4;j++) {
         int v = (j==0) ? (tl+2*l[0]+l[1]+2)>>2 : (j==3) ? (l[2]+3*l[3]+2)>>2 : (l[j-1]+2*l[j]+l[j+1]+2)>>2;
         memset(d+j*s,(uint8_t)v,4);
      }
      break;
   case 6: /* B_VR_PRED */
      d[3*s+0]=(uint8_t)((l[2]+2*l[1]+l[0]+2)>>2);
      d[2*s+0]=(uint8_t)((l[1]+2*l[0]+tl+2)>>2);
      d[1*s+0]=d[3*s+1]=(uint8_t)((l[0]+2*tl+a[0]+2)>>2);
      d[0*s+0]=d[2*s+1]=(uint8_t)((tl+a[0]+1)>>1);
      d[0*s+1]=d[2*s+2]=(uint8_t)((a[0]+a[1]+1)>>1);
      d[1*s+1]=d[3*s+2]=(uint8_t)((tl+2*a[0]+a[1]+2)>>2);
      d[0*s+2]=d[2*s+3]=(uint8_t)((a[1]+a[2]+1)>>1);
      d[1*s+2]=d[3*s+3]=(uint8_t)((a[0]+2*a[1]+a[2]+2)>>2);
      d[0*s+3]=(uint8_t)((a[2]+a[3]+1)>>1);
      d[1*s+3]=(uint8_t)((a[1]+2*a[2]+a[3]+2)>>2);
      break;
   case 4: /* B_LD_PRED */
      d[0*s+0]=(uint8_t)((a[0]+2*a[1]+a[2]+2)>>2);
      d[0*s+1]=d[1*s+0]=(uint8_t)((a[1]+2*a[2]+a[3]+2)>>2);
      d[0*s+2]=d[1*s+1]=d[2*s+0]=(uint8_t)((a[2]+2*a[3]+a[4]+2)>>2);
      d[0*s+3]=d[1*s+2]=d[2*s+1]=d[3*s+0]=(uint8_t)((a[3]+2*a[4]+a[5]+2)>>2);
      d[1*s+3]=d[2*s+2]=d[3*s+1]=(uint8_t)((a[4]+2*a[5]+a[6]+2)>>2);
      d[2*s+3]=d[3*s+2]=(uint8_t)((a[5]+2*a[6]+a[7]+2)>>2);
      d[3*s+3]=(uint8_t)((a[6]+2*a[7]+a[7]+2)>>2);
      break;
   case 5: /* B_RD_PRED */
      d[3*s+0]=(uint8_t)((l[3]+2*l[2]+l[1]+2)>>2);
      d[2*s+0]=d[3*s+1]=(uint8_t)((l[2]+2*l[1]+l[0]+2)>>2);
      d[1*s+0]=d[2*s+1]=d[3*s+2]=(uint8_t)((l[1]+2*l[0]+tl+2)>>2);
      d[0*s+0]=d[1*s+1]=d[2*s+2]=d[3*s+3]=(uint8_t)((l[0]+2*tl+a[0]+2)>>2);
      d[0*s+1]=d[1*s+2]=d[2*s+3]=(uint8_t)((tl+2*a[0]+a[1]+2)>>2);
      d[0*s+2]=d[1*s+3]=(uint8_t)((a[0]+2*a[1]+a[2]+2)>>2);
      d[0*s+3]=(uint8_t)((a[1]+2*a[2]+a[3]+2)>>2);
      break;
   case 7: /* B_VL_PRED */
      d[0*s+0]=(uint8_t)((a[0]+a[1]+1)>>1); d[1*s+0]=(uint8_t)((a[0]+2*a[1]+a[2]+2)>>2);
      d[0*s+1]=d[2*s+0]=(uint8_t)((a[1]+a[2]+1)>>1); d[1*s+1]=d[3*s+0]=(uint8_t)((a[1]+2*a[2]+a[3]+2)>>2);
      d[0*s+2]=d[2*s+1]=(uint8_t)((a[2]+a[3]+1)>>1); d[1*s+2]=d[3*s+1]=(uint8_t)((a[2]+2*a[3]+a[4]+2)>>2);
      d[0*s+3]=d[2*s+2]=(uint8_t)((a[3]+a[4]+1)>>1); d[1*s+3]=d[3*s+2]=(uint8_t)((a[3]+2*a[4]+a[5]+2)>>2);
      d[2*s+3]=(uint8_t)((a[4]+2*a[5]+a[6]+2)>>2); d[3*s+3]=(uint8_t)((a[5]+2*a[6]+a[7]+2)>>2);
      break;
   case 8: /* B_HD_PRED */
      d[3*s+0]=(uint8_t)((l[3]+l[2]+1)>>1); d[3*s+1]=(uint8_t)((l[3]+2*l[2]+l[1]+2)>>2);
      d[2*s+0]=d[3*s+2]=(uint8_t)((l[2]+l[1]+1)>>1); d[2*s+1]=d[3*s+3]=(uint8_t)((l[2]+2*l[1]+l[0]+2)>>2);
      d[1*s+0]=d[2*s+2]=(uint8_t)((l[1]+l[0]+1)>>1); d[1*s+1]=d[2*s+3]=(uint8_t)((l[1]+2*l[0]+tl+2)>>2);
      d[0*s+0]=d[1*s+2]=(uint8_t)((l[0]+tl+1)>>1); d[0*s+1]=d[1*s+3]=(uint8_t)((l[0]+2*tl+a[0]+2)>>2);
      d[0*s+2]=(uint8_t)((tl+2*a[0]+a[1]+2)>>2); d[0*s+3]=(uint8_t)((a[0]+2*a[1]+a[2]+2)>>2);
      break;
   case 9: /* B_HU_PRED */
      d[0*s+0]=(uint8_t)((l[0]+l[1]+1)>>1); d[0*s+1]=(uint8_t)((l[0]+2*l[1]+l[2]+2)>>2);
      d[0*s+2]=d[1*s+0]=(uint8_t)((l[1]+l[2]+1)>>1); d[0*s+3]=d[1*s+1]=(uint8_t)((l[1]+2*l[2]+l[3]+2)>>2);
      d[1*s+2]=d[2*s+0]=(uint8_t)((l[2]+l[3]+1)>>1); d[1*s+3]=d[2*s+1]=(uint8_t)((l[2]+2*l[3]+l[3]+2)>>2);
      d[2*s+2]=d[2*s+3]=d[3*s+0]=d[3*s+1]=d[3*s+2]=d[3*s+3]=(uint8_t)l[3];
      break;
   default:
   {  int sum=0; for(i=0;i<4;i++) sum+=a[i]+l[i];
      { uint8_t dc=(uint8_t)((sum+4)>>3); for(j=0;j<4;j++) memset(d+j*s,dc,4); } break; }
   }
}


/* VP8 Simple Loop Filter (RFC 6386 §15.2) */
static INLINE int vp8_sc(int v) { return v < -128 ? -128 : v > 127 ? 127 : v; }

static void vp8_simple_lf_edge(uint8_t *p1p, uint8_t *p0p, uint8_t *q0p, uint8_t *q1p, int lim)
{
   int p1 = *p1p, p0 = *p0p, q0 = *q0p, q1 = *q1p;
   int d0 = p0 - q0, d1 = p1 - q1;
   int mask = ((d0 < 0 ? -d0 : d0) * 2 + (d1 < 0 ? -d1 : d1) / 2) <= lim;
   if (mask)
   {
      int fv = vp8_sc(vp8_sc(d1) + 3 * (q0 - p0));
      int f1 = vp8_sc(fv + 4) >> 3;
      int f2 = vp8_sc(fv + 3) >> 3;
      *q0p = vp8_cl(q0 - f1);
      *p0p = vp8_cl(p0 + f2);
   }
}

/* Per-MB loop-filter level: base/segment level plus the per-MB ref-frame
 * and mode deltas (libvpx vp8_loop_filter_frame_init).  Shared by the
 * normal and simple filters -- the level computation is identical, only
 * the edge kernels differ.  mb_info is the decoded rvp8_mbinfo array for
 * an inter frame, NULL for a key frame. */
static int rvp8_mb_filter_level(int n, int lf_level,
   int seg_enabled, int seg_abs, const int *seg_lf,
   int lf_delta_enabled, const int *ref_lf_delta, const int *mode_lf_delta,
   const uint8_t *seg_map, const uint8_t *bpred_map, const void *mb_info)
{
   const rvp8_mbinfo *mbi = (const rvp8_mbinfo*)mb_info;
   int lvl = lf_level;
   int is_bpred;

   if (seg_enabled && seg_abs)
      lvl = seg_lf[seg_map ? seg_map[n] : 0];
   else if (seg_enabled)
      lvl = lf_level + seg_lf[seg_map ? seg_map[n] : 0];
   if (lvl < 0) lvl = 0;
   if (lvl > 63) lvl = 63;
   is_bpred = bpred_map ? bpred_map[n] : 0;
   /* For an inter frame the ref index and mode index come from the decoded
    * mbinfo; for a key frame every MB is INTRA + its intra mode (the bpred
    * flag distinguishes mode 0 vs 1). */
   if (lf_delta_enabled)
   {
      int ref_idx = 0;   /* INTRA_FRAME */
      int add_mode_delta = 0;
      int mode_idx = 0;
      if (mbi && mbi[n].ref_frame != 0)
      {
         /* Inter MB: ref + mode both index the delta tables. */
         const rvp8_mbinfo *m = &mbi[n];
         ref_idx = m->ref_frame;
         switch (m->mode)
         {
            case MV_ZERO:    mode_idx = 1; break;
            case MV_NEAREST:
            case MV_NEAR:
            case MV_NEW:     mode_idx = 2; break;
            case MV_SPLIT:   mode_idx = 3; break;
            default:         mode_idx = 1; break;
         }
         add_mode_delta = 1;
      }
      else
      {
         /* Intra MB (key frame or intra-in-inter): the mode delta is
          * applied ONLY to B_PRED (libvpx vp8_loop_filter_frame_init:
          * mode 0 gets mode_lf_deltas[0]; all other intra modes get
          * no mode delta). */
         ref_idx = 0;
         if (is_bpred) { mode_idx = 0; add_mode_delta = 1; }
      }
      lvl += ref_lf_delta[ref_idx];
      if (add_mode_delta) lvl += mode_lf_delta[mode_idx];
      if (lvl < 0) lvl = 0;
      if (lvl > 63) lvl = 63;
   }
   return lvl;
}

static void vp8_loop_filter_simple(uint8_t *y, int ys,
   int mbw, int lf_my0, int lf_my1, int lf_level, int sharpness,
   int seg_enabled, int seg_abs, const int *seg_lf,
   int lf_delta_enabled, const int *ref_lf_delta, const int *mode_lf_delta,
   const uint8_t *seg_map, const uint8_t *skip_lf_map, const uint8_t *bpred_map,
   const void *mb_info)
{
   int mx, my, i, e;
   for (my = lf_my0; my < lf_my1; my++)
   {
      for (mx = 0; mx < mbw; mx++)
      {
         int mb_lf;
         int lim, blim, mblim, skip_lf;
         uint8_t *my0 = y + my * 16 * ys + mx * 16;
         mb_lf = rvp8_mb_filter_level(my * mbw + mx, lf_level,
               seg_enabled, seg_abs, seg_lf,
               lf_delta_enabled, ref_lf_delta, mode_lf_delta,
               seg_map, bpred_map, mb_info);
         if (mb_lf == 0) continue;
         /* Limits per libvpx vp8_loop_filter_update_sharpness */
         lim = mb_lf >> ((sharpness > 0) + (sharpness > 4));
         if (sharpness > 0 && lim > 9 - sharpness) lim = 9 - sharpness;
         if (lim < 1) lim = 1;
         mblim = 2 * (mb_lf + 2) + lim;
         blim  = 2 * mb_lf + lim;
         skip_lf = skip_lf_map ? skip_lf_map[my * mbw + mx] : 0;
         /* Edge order per libvpx: mbv, bv, mbh, bh */
         if (mx > 0)
            for (i = 0; i < 16; i++) {
               uint8_t *r = my0 + i * ys;
               vp8_simple_lf_edge(r - 2, r - 1, r, r + 1, mblim);
            }
         if (!skip_lf)
            for (e = 4; e <= 12; e += 4)
               for (i = 0; i < 16; i++) {
                  uint8_t *r = my0 + i * ys + e;
                  vp8_simple_lf_edge(r - 2, r - 1, r, r + 1, blim);
               }
         if (my > 0)
            for (i = 0; i < 16; i++) {
               uint8_t *c = my0 + i;
               vp8_simple_lf_edge(c - 2 * ys, c - ys, c, c + ys, mblim);
            }
         if (!skip_lf)
            for (e = 4; e <= 12; e += 4)
               for (i = 0; i < 16; i++) {
                  uint8_t *c = my0 + e * ys + i;
                  vp8_simple_lf_edge(c - 2 * ys, c - ys, c, c + ys, blim);
               }
      }
   }
}

/* VP8 Normal Loop Filter (RFC 6386 section 15.3), ported from libvpx
 * loopfilter_filters.c. Kernels operate in the signed (^0x80) domain. */

static INLINE int vp8_nlf_mask(int lim, int blim,
      int p3, int p2, int p1, int p0, int q0, int q1, int q2, int q3)
{
   int m = 0;
   m |= (rvp8_abs(p3 - p2) > lim);
   m |= (rvp8_abs(p2 - p1) > lim);
   m |= (rvp8_abs(p1 - p0) > lim);
   m |= (rvp8_abs(q1 - q0) > lim);
   m |= (rvp8_abs(q2 - q1) > lim);
   m |= (rvp8_abs(q3 - q2) > lim);
   m |= (rvp8_abs(p0 - q0) * 2 + rvp8_abs(p1 - q1) / 2 > blim);
   return m - 1; /* 0 -> -1 (all ones), 1 -> 0 */
}

static INLINE int vp8_nlf_hev(int thr, int p1, int p0, int q0, int q1)
{
   int h = 0;
   h |= (rvp8_abs(p1 - p0) > thr) * -1;
   h |= (rvp8_abs(q1 - q0) > thr) * -1;
   return h;
}

/* Inner (sub-block) 4-tap filter: adjusts p1,p0,q0,q1. */
static void vp8_nlf_inner(int mask, int hev,
      uint8_t *op1, uint8_t *op0, uint8_t *oq0, uint8_t *oq1)
{
   int ps1 = (int)*op1 - 128, ps0 = (int)*op0 - 128;
   int qs0 = (int)*oq0 - 128, qs1 = (int)*oq1 - 128;
   int fv, f1, f2;

   fv = vp8_sc(ps1 - qs1);
   fv &= hev;
   fv = vp8_sc(fv + 3 * (qs0 - ps0));
   fv &= mask;

   f1 = vp8_sc(fv + 4) >> 3;
   f2 = vp8_sc(fv + 3) >> 3;
   *oq0 = (uint8_t)(vp8_sc(qs0 - f1) + 128);
   *op0 = (uint8_t)(vp8_sc(ps0 + f2) + 128);

   fv = (f1 + 1) >> 1;
   fv &= ~hev;
   *oq1 = (uint8_t)(vp8_sc(qs1 - fv) + 128);
   *op1 = (uint8_t)(vp8_sc(ps1 + fv) + 128);
}

/* Macroblock-edge 6-tap filter: adjusts p2..q2. */
static void vp8_nlf_mb(int mask, int hev,
      uint8_t *op2, uint8_t *op1, uint8_t *op0,
      uint8_t *oq0, uint8_t *oq1, uint8_t *oq2)
{
   int ps2 = (int)*op2 - 128, ps1 = (int)*op1 - 128, ps0 = (int)*op0 - 128;
   int qs0 = (int)*oq0 - 128, qs1 = (int)*oq1 - 128, qs2 = (int)*oq2 - 128;
   int fv, f1, f2, u;

   fv = vp8_sc(ps1 - qs1);
   fv = vp8_sc(fv + 3 * (qs0 - ps0));
   fv &= mask;

   f2 = fv & hev;
   f1 = vp8_sc(f2 + 4) >> 3;
   f2 = vp8_sc(f2 + 3) >> 3;
   qs0 = vp8_sc(qs0 - f1);
   ps0 = vp8_sc(ps0 + f2);

   fv &= ~hev;

   u = vp8_sc((63 + fv * 27) >> 7);
   *oq0 = (uint8_t)(vp8_sc(qs0 - u) + 128);
   *op0 = (uint8_t)(vp8_sc(ps0 + u) + 128);

   u = vp8_sc((63 + fv * 18) >> 7);
   *oq1 = (uint8_t)(vp8_sc(qs1 - u) + 128);
   *op1 = (uint8_t)(vp8_sc(ps1 + u) + 128);

   u = vp8_sc((63 + fv * 9) >> 7);
   *oq2 = (uint8_t)(vp8_sc(qs2 - u) + 128);
   *op2 = (uint8_t)(vp8_sc(ps2 + u) + 128);
}

#if defined(RWEBP_YUV_SSE2)
#define RWEBP_LF_SSE2 1
#endif
#if defined(RWEBP_YUV_NEON)
#define RWEBP_LF_NEON 1
#endif
#if defined(RWEBP_LF_SSE2)
/* 16-lane normal loop filter. The per-lane math mirrors vp8_nlf_inner /
 * vp8_nlf_mb exactly: the +-128 bias is the 0x80 xor, vp8_sc is the
 * saturating signed-8 arithmetic, 3*(qs0-ps0) is three saturating adds,
 * and the (63 + fv*K) >> 7 taps widen to 16-bit lanes. Verified
 * bit-exact against the scalar filters over randomized exhaustive
 * sweeps and whole-plane edge tests in both orientations. */
#define RWEBP_ABSD(a,b) _mm_or_si128(_mm_subs_epu8(a,b),_mm_subs_epu8(b,a))

static INLINE __m128i rwebp_gtu8(__m128i a, __m128i b)
{
   return _mm_xor_si128(_mm_cmpeq_epi8(_mm_subs_epu8(a, b),
         _mm_setzero_si128()), _mm_set1_epi8((char)0xFF));
}

static void vp8_nlf_mask_hev_x(__m128i p3, __m128i p2, __m128i p1,
      __m128i p0, __m128i q0, __m128i q1, __m128i q2, __m128i q3,
      int lim, int blim, int thr, __m128i *maskv, __m128i *hevv)
{
   __m128i L = _mm_set1_epi8((char)lim);
   __m128i T = _mm_set1_epi8((char)thr);
   __m128i m = _mm_setzero_si128();
   m = _mm_or_si128(m, rwebp_gtu8(RWEBP_ABSD(p3, p2), L));
   m = _mm_or_si128(m, rwebp_gtu8(RWEBP_ABSD(p2, p1), L));
   m = _mm_or_si128(m, rwebp_gtu8(RWEBP_ABSD(p1, p0), L));
   m = _mm_or_si128(m, rwebp_gtu8(RWEBP_ABSD(q1, q0), L));
   m = _mm_or_si128(m, rwebp_gtu8(RWEBP_ABSD(q2, q1), L));
   m = _mm_or_si128(m, rwebp_gtu8(RWEBP_ABSD(q3, q2), L));
   {
      __m128i d0 = RWEBP_ABSD(p0, q0), d1 = RWEBP_ABSD(p1, q1);
      __m128i z  = _mm_setzero_si128();
      __m128i bl = _mm_set1_epi16((short)blim);
      __m128i sl = _mm_add_epi16(_mm_slli_epi16(_mm_unpacklo_epi8(d0, z), 1),
                                 _mm_srli_epi16(_mm_unpacklo_epi8(d1, z), 1));
      __m128i sh = _mm_add_epi16(_mm_slli_epi16(_mm_unpackhi_epi8(d0, z), 1),
                                 _mm_srli_epi16(_mm_unpackhi_epi8(d1, z), 1));
      m = _mm_or_si128(m, _mm_packs_epi16(_mm_cmpgt_epi16(sl, bl),
                                          _mm_cmpgt_epi16(sh, bl)));
   }
   *maskv = _mm_cmpeq_epi8(m, _mm_setzero_si128());
   *hevv  = _mm_or_si128(rwebp_gtu8(RWEBP_ABSD(p1, p0), T),
                         rwebp_gtu8(RWEBP_ABSD(q1, q0), T));
}

static void vp8_nlf_inner_x(__m128i maskv, __m128i hevv,
      __m128i *p1, __m128i *p0, __m128i *q0, __m128i *q1)
{
   __m128i t80 = _mm_set1_epi8((char)0x80);
   __m128i ps1 = _mm_xor_si128(*p1, t80), ps0 = _mm_xor_si128(*p0, t80);
   __m128i qs0 = _mm_xor_si128(*q0, t80), qs1 = _mm_xor_si128(*q1, t80);
   __m128i fv  = _mm_subs_epi8(ps1, qs1);
   __m128i qp  = _mm_subs_epi8(qs0, ps0);
   __m128i f1, f2;
   __m128i z   = _mm_setzero_si128();
   fv = _mm_and_si128(fv, hevv);
   fv = _mm_adds_epi8(fv, qp);
   fv = _mm_adds_epi8(fv, qp);
   fv = _mm_adds_epi8(fv, qp);
   fv = _mm_and_si128(fv, maskv);
   f1 = _mm_adds_epi8(fv, _mm_set1_epi8(4));
   f2 = _mm_adds_epi8(fv, _mm_set1_epi8(3));
   {
      __m128i a = _mm_srai_epi16(_mm_unpacklo_epi8(z, f1), 11);
      __m128i b = _mm_srai_epi16(_mm_unpackhi_epi8(z, f1), 11);
      __m128i c = _mm_srai_epi16(_mm_unpacklo_epi8(z, f2), 11);
      __m128i d = _mm_srai_epi16(_mm_unpackhi_epi8(z, f2), 11);
      f1 = _mm_packs_epi16(a, b);
      f2 = _mm_packs_epi16(c, d);
   }
   qs0 = _mm_subs_epi8(qs0, f1);
   ps0 = _mm_adds_epi8(ps0, f2);
   {
      __m128i one = _mm_set1_epi16(1);
      __m128i l = _mm_srai_epi16(_mm_add_epi16(
            _mm_srai_epi16(_mm_unpacklo_epi8(z, f1), 8), one), 1);
      __m128i h = _mm_srai_epi16(_mm_add_epi16(
            _mm_srai_epi16(_mm_unpackhi_epi8(z, f1), 8), one), 1);
      fv = _mm_packs_epi16(l, h);
   }
   fv  = _mm_andnot_si128(hevv, fv);
   qs1 = _mm_subs_epi8(qs1, fv);
   ps1 = _mm_adds_epi8(ps1, fv);
   *q0 = _mm_xor_si128(qs0, t80); *p0 = _mm_xor_si128(ps0, t80);
   *q1 = _mm_xor_si128(qs1, t80); *p1 = _mm_xor_si128(ps1, t80);
}

static void vp8_nlf_mb_x(__m128i maskv, __m128i hevv,
      __m128i *p2, __m128i *p1, __m128i *p0,
      __m128i *q0, __m128i *q1, __m128i *q2)
{
   __m128i t80 = _mm_set1_epi8((char)0x80);
   __m128i ps2 = _mm_xor_si128(*p2, t80), ps1 = _mm_xor_si128(*p1, t80);
   __m128i ps0 = _mm_xor_si128(*p0, t80), qs0 = _mm_xor_si128(*q0, t80);
   __m128i qs1 = _mm_xor_si128(*q1, t80), qs2 = _mm_xor_si128(*q2, t80);
   __m128i fv  = _mm_subs_epi8(ps1, qs1);
   __m128i qp  = _mm_subs_epi8(qs0, ps0);
   __m128i f1, f2;
   __m128i z   = _mm_setzero_si128();
   fv = _mm_adds_epi8(fv, qp);
   fv = _mm_adds_epi8(fv, qp);
   fv = _mm_adds_epi8(fv, qp);
   fv = _mm_and_si128(fv, maskv);
   f2 = _mm_and_si128(fv, hevv);
   f1 = _mm_adds_epi8(f2, _mm_set1_epi8(4));
   f2 = _mm_adds_epi8(f2, _mm_set1_epi8(3));
   {
      __m128i a = _mm_srai_epi16(_mm_unpacklo_epi8(z, f1), 11);
      __m128i b = _mm_srai_epi16(_mm_unpackhi_epi8(z, f1), 11);
      __m128i c = _mm_srai_epi16(_mm_unpacklo_epi8(z, f2), 11);
      __m128i d = _mm_srai_epi16(_mm_unpackhi_epi8(z, f2), 11);
      f1 = _mm_packs_epi16(a, b);
      f2 = _mm_packs_epi16(c, d);
   }
   qs0 = _mm_subs_epi8(qs0, f1);
   ps0 = _mm_adds_epi8(ps0, f2);
   fv = _mm_andnot_si128(hevv, fv);
   {
      __m128i fl  = _mm_srai_epi16(_mm_unpacklo_epi8(z, fv), 8);
      __m128i fh  = _mm_srai_epi16(_mm_unpackhi_epi8(z, fv), 8);
      __m128i s63 = _mm_set1_epi16(63);
      __m128i u27 = _mm_packs_epi16(
            _mm_srai_epi16(_mm_add_epi16(s63, _mm_mullo_epi16(fl, _mm_set1_epi16(27))), 7),
            _mm_srai_epi16(_mm_add_epi16(s63, _mm_mullo_epi16(fh, _mm_set1_epi16(27))), 7));
      __m128i u18 = _mm_packs_epi16(
            _mm_srai_epi16(_mm_add_epi16(s63, _mm_mullo_epi16(fl, _mm_set1_epi16(18))), 7),
            _mm_srai_epi16(_mm_add_epi16(s63, _mm_mullo_epi16(fh, _mm_set1_epi16(18))), 7));
      __m128i u9  = _mm_packs_epi16(
            _mm_srai_epi16(_mm_add_epi16(s63, _mm_mullo_epi16(fl, _mm_set1_epi16(9))), 7),
            _mm_srai_epi16(_mm_add_epi16(s63, _mm_mullo_epi16(fh, _mm_set1_epi16(9))), 7));
      qs0 = _mm_subs_epi8(qs0, u27); ps0 = _mm_adds_epi8(ps0, u27);
      qs1 = _mm_subs_epi8(qs1, u18); ps1 = _mm_adds_epi8(ps1, u18);
      qs2 = _mm_subs_epi8(qs2, u9);  ps2 = _mm_adds_epi8(ps2, u9);
   }
   *q0 = _mm_xor_si128(qs0, t80); *p0 = _mm_xor_si128(ps0, t80);
   *q1 = _mm_xor_si128(qs1, t80); *p1 = _mm_xor_si128(ps1, t80);
   *q2 = _mm_xor_si128(qs2, t80); *p2 = _mm_xor_si128(ps2, t80);
}
#endif /* RWEBP_LF_SSE2 */

#if defined(RWEBP_LF_NEON)
/* NEON port of the same 16-lane filters; identical decomposition
 * (saturating signed-8 arithmetic, 16-bit widening for the wide taps). */
static void vp8_nlf_mask_hev_n(uint8x16_t p3, uint8x16_t p2, uint8x16_t p1,
      uint8x16_t p0, uint8x16_t q0, uint8x16_t q1, uint8x16_t q2,
      uint8x16_t q3, int lim, int blim, int thr,
      uint8x16_t *maskv, uint8x16_t *hevv)
{
   uint8x16_t L = vdupq_n_u8((uint8_t)lim);
   uint8x16_t T = vdupq_n_u8((uint8_t)thr);
   uint8x16_t m = vcgtq_u8(vabdq_u8(p3, p2), L);
   uint16x8_t bl, sl, sh;
   uint8x16_t d0, d1;
   m = vorrq_u8(m, vcgtq_u8(vabdq_u8(p2, p1), L));
   m = vorrq_u8(m, vcgtq_u8(vabdq_u8(p1, p0), L));
   m = vorrq_u8(m, vcgtq_u8(vabdq_u8(q1, q0), L));
   m = vorrq_u8(m, vcgtq_u8(vabdq_u8(q2, q1), L));
   m = vorrq_u8(m, vcgtq_u8(vabdq_u8(q3, q2), L));
   d0 = vabdq_u8(p0, q0);
   d1 = vabdq_u8(p1, q1);
   bl = vdupq_n_u16((uint16_t)blim);
   sl = vaddq_u16(vshlq_n_u16(vmovl_u8(vget_low_u8(d0)), 1),
                  vshrq_n_u16(vmovl_u8(vget_low_u8(d1)), 1));
   sh = vaddq_u16(vshlq_n_u16(vmovl_u8(vget_high_u8(d0)), 1),
                  vshrq_n_u16(vmovl_u8(vget_high_u8(d1)), 1));
   m = vorrq_u8(m, vcombine_u8(vmovn_u16(vcgtq_u16(sl, bl)),
                               vmovn_u16(vcgtq_u16(sh, bl))));
   *maskv = vceqq_u8(m, vdupq_n_u8(0));
   *hevv  = vorrq_u8(vcgtq_u8(vabdq_u8(p1, p0), T),
                     vcgtq_u8(vabdq_u8(q1, q0), T));
}

static void vp8_nlf_inner_n(uint8x16_t maskv, uint8x16_t hevv,
      uint8x16_t *p1, uint8x16_t *p0, uint8x16_t *q0, uint8x16_t *q1)
{
   uint8x16_t t80 = vdupq_n_u8(0x80);
   int8x16_t ps1 = vreinterpretq_s8_u8(veorq_u8(*p1, t80));
   int8x16_t ps0 = vreinterpretq_s8_u8(veorq_u8(*p0, t80));
   int8x16_t qs0 = vreinterpretq_s8_u8(veorq_u8(*q0, t80));
   int8x16_t qs1 = vreinterpretq_s8_u8(veorq_u8(*q1, t80));
   int8x16_t fv  = vqsubq_s8(ps1, qs1);
   int8x16_t qp  = vqsubq_s8(qs0, ps0);
   int8x16_t f1, f2, fo;
   fv = vandq_s8(fv, vreinterpretq_s8_u8(hevv));
   fv = vqaddq_s8(fv, qp);
   fv = vqaddq_s8(fv, qp);
   fv = vqaddq_s8(fv, qp);
   fv = vandq_s8(fv, vreinterpretq_s8_u8(maskv));
   f1 = vqaddq_s8(fv, vdupq_n_s8(4));
   f2 = vqaddq_s8(fv, vdupq_n_s8(3));
   f1 = vshrq_n_s8(f1, 3);
   f2 = vshrq_n_s8(f2, 3);
   qs0 = vqsubq_s8(qs0, f1);
   ps0 = vqaddq_s8(ps0, f2);
   fo  = vrshrq_n_s8(f1, 1); /* (f1+1)>>1 rounding shift */
   fo  = vbicq_s8(fo, vreinterpretq_s8_u8(hevv));
   qs1 = vqsubq_s8(qs1, fo);
   ps1 = vqaddq_s8(ps1, fo);
   *q0 = veorq_u8(vreinterpretq_u8_s8(qs0), t80);
   *p0 = veorq_u8(vreinterpretq_u8_s8(ps0), t80);
   *q1 = veorq_u8(vreinterpretq_u8_s8(qs1), t80);
   *p1 = veorq_u8(vreinterpretq_u8_s8(ps1), t80);
}

static void vp8_nlf_mb_n(uint8x16_t maskv, uint8x16_t hevv,
      uint8x16_t *p2, uint8x16_t *p1, uint8x16_t *p0,
      uint8x16_t *q0, uint8x16_t *q1, uint8x16_t *q2)
{
   uint8x16_t t80 = vdupq_n_u8(0x80);
   int8x16_t ps2 = vreinterpretq_s8_u8(veorq_u8(*p2, t80));
   int8x16_t ps1 = vreinterpretq_s8_u8(veorq_u8(*p1, t80));
   int8x16_t ps0 = vreinterpretq_s8_u8(veorq_u8(*p0, t80));
   int8x16_t qs0 = vreinterpretq_s8_u8(veorq_u8(*q0, t80));
   int8x16_t qs1 = vreinterpretq_s8_u8(veorq_u8(*q1, t80));
   int8x16_t qs2 = vreinterpretq_s8_u8(veorq_u8(*q2, t80));
   int8x16_t fv  = vqsubq_s8(ps1, qs1);
   int8x16_t qp  = vqsubq_s8(qs0, ps0);
   int8x16_t f1, f2;
   int16x8_t fl, fh, s63;
   int8x16_t u27, u18, u9;
   fv = vqaddq_s8(fv, qp);
   fv = vqaddq_s8(fv, qp);
   fv = vqaddq_s8(fv, qp);
   fv = vandq_s8(fv, vreinterpretq_s8_u8(maskv));
   f2 = vandq_s8(fv, vreinterpretq_s8_u8(hevv));
   f1 = vshrq_n_s8(vqaddq_s8(f2, vdupq_n_s8(4)), 3);
   f2 = vshrq_n_s8(vqaddq_s8(f2, vdupq_n_s8(3)), 3);
   qs0 = vqsubq_s8(qs0, f1);
   ps0 = vqaddq_s8(ps0, f2);
   fv = vbicq_s8(fv, vreinterpretq_s8_u8(hevv));
   fl = vmovl_s8(vget_low_s8(fv));
   fh = vmovl_s8(vget_high_s8(fv));
   s63 = vdupq_n_s16(63);
   u27 = vcombine_s8(
         vqmovn_s16(vshrq_n_s16(vmlaq_n_s16(s63, fl, 27), 7)),
         vqmovn_s16(vshrq_n_s16(vmlaq_n_s16(s63, fh, 27), 7)));
   u18 = vcombine_s8(
         vqmovn_s16(vshrq_n_s16(vmlaq_n_s16(s63, fl, 18), 7)),
         vqmovn_s16(vshrq_n_s16(vmlaq_n_s16(s63, fh, 18), 7)));
   u9  = vcombine_s8(
         vqmovn_s16(vshrq_n_s16(vmlaq_n_s16(s63, fl, 9), 7)),
         vqmovn_s16(vshrq_n_s16(vmlaq_n_s16(s63, fh, 9), 7)));
   qs0 = vqsubq_s8(qs0, u27); ps0 = vqaddq_s8(ps0, u27);
   qs1 = vqsubq_s8(qs1, u18); ps1 = vqaddq_s8(ps1, u18);
   qs2 = vqsubq_s8(qs2, u9);  ps2 = vqaddq_s8(ps2, u9);
   *q0 = veorq_u8(vreinterpretq_u8_s8(qs0), t80);
   *p0 = veorq_u8(vreinterpretq_u8_s8(ps0), t80);
   *q1 = veorq_u8(vreinterpretq_u8_s8(qs1), t80);
   *p1 = veorq_u8(vreinterpretq_u8_s8(ps1), t80);
   *q2 = veorq_u8(vreinterpretq_u8_s8(qs2), t80);
   *p2 = veorq_u8(vreinterpretq_u8_s8(ps2), t80);
}
#endif /* RWEBP_LF_NEON */

/* Walk one edge: n filtered positions, taps tp apart, positions sp apart.
 * With SSE2 the whole edge (16 luma / 8 chroma positions) is filtered as
 * one vector: a horizontal edge (tp==stride) loads each tap row
 * directly; a vertical edge (tp==1) gathers the 8 taps of every
 * position through a small transpose buffer. The scalar loop remains
 * the fallback for other targets and odd call shapes. */
static void vp8_nlf_edge(uint8_t *s, int tp, int sp, int n,
      int edge_lim, int lim, int thr, int is_mb)
{
#if defined(RWEBP_LF_SSE2)
   if ((n == 16 || n == 8) && tp != 1)
   {
      /* Horizontal edge: tap rows are contiguous runs of n bytes. */
      __m128i p3, p2, p1, p0, q0, q1, q2, q3, mv, hv;
      uint8_t buf[16];
      if (n == 16)
      {
         p3 = _mm_loadu_si128((const __m128i*)(s - 4*tp));
         p2 = _mm_loadu_si128((const __m128i*)(s - 3*tp));
         p1 = _mm_loadu_si128((const __m128i*)(s - 2*tp));
         p0 = _mm_loadu_si128((const __m128i*)(s - 1*tp));
         q0 = _mm_loadu_si128((const __m128i*)(s));
         q1 = _mm_loadu_si128((const __m128i*)(s + 1*tp));
         q2 = _mm_loadu_si128((const __m128i*)(s + 2*tp));
         q3 = _mm_loadu_si128((const __m128i*)(s + 3*tp));
      }
      else
      {
         memset(buf, 0, sizeof(buf));
#define RWEBP_LD8(v, off) do { memcpy(buf, s + (off)*tp, 8);          (v) = _mm_loadu_si128((const __m128i*)buf); } while (0)
         RWEBP_LD8(p3, -4); RWEBP_LD8(p2, -3); RWEBP_LD8(p1, -2);
         RWEBP_LD8(p0, -1); RWEBP_LD8(q0,  0); RWEBP_LD8(q1,  1);
         RWEBP_LD8(q2,  2); RWEBP_LD8(q3,  3);
#undef RWEBP_LD8
      }
      vp8_nlf_mask_hev_x(p3, p2, p1, p0, q0, q1, q2, q3,
            lim, edge_lim, thr, &mv, &hv);
      if (is_mb)
         vp8_nlf_mb_x(mv, hv, &p2, &p1, &p0, &q0, &q1, &q2);
      else
         vp8_nlf_inner_x(mv, hv, &p1, &p0, &q0, &q1);
      if (n == 16)
      {
         if (is_mb)
         {
            _mm_storeu_si128((__m128i*)(s - 3*tp), p2);
            _mm_storeu_si128((__m128i*)(s + 2*tp), q2);
         }
         _mm_storeu_si128((__m128i*)(s - 2*tp), p1);
         _mm_storeu_si128((__m128i*)(s - 1*tp), p0);
         _mm_storeu_si128((__m128i*)(s),        q0);
         _mm_storeu_si128((__m128i*)(s + 1*tp), q1);
      }
      else
      {
#define RWEBP_ST8(v, off) do { _mm_storeu_si128((__m128i*)buf, v);          memcpy(s + (off)*tp, buf, 8); } while (0)
         if (is_mb) { RWEBP_ST8(p2, -3); RWEBP_ST8(q2, 2); }
         RWEBP_ST8(p1, -2); RWEBP_ST8(p0, -1);
         RWEBP_ST8(q0,  0); RWEBP_ST8(q1,  1);
#undef RWEBP_ST8
      }
      return;
   }
   if ((n == 16 || n == 8) && tp == 1)
   {
      /* Vertical edge: gather the 8 taps of each position (transpose). */
      uint8_t tmp[8][16];
      int i, k;
      __m128i p3, p2, p1, p0, q0, q1, q2, q3, mv, hv;
      for (i = 0; i < n; i++)
      {
         const uint8_t *r = s + i*sp - 4;
         for (k = 0; k < 8; k++)
            tmp[k][i] = r[k];
      }
      for (; i < 16; i++)
         for (k = 0; k < 8; k++)
            tmp[k][i] = 0;
      p3 = _mm_loadu_si128((const __m128i*)tmp[0]);
      p2 = _mm_loadu_si128((const __m128i*)tmp[1]);
      p1 = _mm_loadu_si128((const __m128i*)tmp[2]);
      p0 = _mm_loadu_si128((const __m128i*)tmp[3]);
      q0 = _mm_loadu_si128((const __m128i*)tmp[4]);
      q1 = _mm_loadu_si128((const __m128i*)tmp[5]);
      q2 = _mm_loadu_si128((const __m128i*)tmp[6]);
      q3 = _mm_loadu_si128((const __m128i*)tmp[7]);
      vp8_nlf_mask_hev_x(p3, p2, p1, p0, q0, q1, q2, q3,
            lim, edge_lim, thr, &mv, &hv);
      if (is_mb)
         vp8_nlf_mb_x(mv, hv, &p2, &p1, &p0, &q0, &q1, &q2);
      else
         vp8_nlf_inner_x(mv, hv, &p1, &p0, &q0, &q1);
      _mm_storeu_si128((__m128i*)tmp[1], p2);
      _mm_storeu_si128((__m128i*)tmp[2], p1);
      _mm_storeu_si128((__m128i*)tmp[3], p0);
      _mm_storeu_si128((__m128i*)tmp[4], q0);
      _mm_storeu_si128((__m128i*)tmp[5], q1);
      _mm_storeu_si128((__m128i*)tmp[6], q2);
      for (i = 0; i < n; i++)
      {
         uint8_t *r = s + i*sp - 4;
         if (is_mb) { r[1] = tmp[1][i]; r[6] = tmp[6][i]; }
         r[2] = tmp[2][i]; r[3] = tmp[3][i];
         r[4] = tmp[4][i]; r[5] = tmp[5][i];
      }
      return;
   }
#endif /* RWEBP_LF_SSE2 */
#if defined(RWEBP_LF_NEON)
   if ((n == 16 || n == 8) && tp != 1)
   {
      uint8x16_t p3, p2, p1, p0, q0, q1, q2, q3, mv, hv;
      uint8_t buf[16];
      if (n == 16)
      {
         p3 = vld1q_u8(s - 4*tp); p2 = vld1q_u8(s - 3*tp);
         p1 = vld1q_u8(s - 2*tp); p0 = vld1q_u8(s - 1*tp);
         q0 = vld1q_u8(s);        q1 = vld1q_u8(s + 1*tp);
         q2 = vld1q_u8(s + 2*tp); q3 = vld1q_u8(s + 3*tp);
      }
      else
      {
         memset(buf, 0, sizeof(buf));
#define RWEBP_LD8N(v, off) do { memcpy(buf, s + (off)*tp, 8);          (v) = vld1q_u8(buf); } while (0)
         RWEBP_LD8N(p3, -4); RWEBP_LD8N(p2, -3); RWEBP_LD8N(p1, -2);
         RWEBP_LD8N(p0, -1); RWEBP_LD8N(q0,  0); RWEBP_LD8N(q1,  1);
         RWEBP_LD8N(q2,  2); RWEBP_LD8N(q3,  3);
#undef RWEBP_LD8N
      }
      vp8_nlf_mask_hev_n(p3, p2, p1, p0, q0, q1, q2, q3,
            lim, edge_lim, thr, &mv, &hv);
      if (is_mb)
         vp8_nlf_mb_n(mv, hv, &p2, &p1, &p0, &q0, &q1, &q2);
      else
         vp8_nlf_inner_n(mv, hv, &p1, &p0, &q0, &q1);
      if (n == 16)
      {
         if (is_mb) { vst1q_u8(s - 3*tp, p2); vst1q_u8(s + 2*tp, q2); }
         vst1q_u8(s - 2*tp, p1); vst1q_u8(s - 1*tp, p0);
         vst1q_u8(s,        q0); vst1q_u8(s + 1*tp, q1);
      }
      else
      {
#define RWEBP_ST8N(v, off) do { vst1q_u8(buf, v);          memcpy(s + (off)*tp, buf, 8); } while (0)
         if (is_mb) { RWEBP_ST8N(p2, -3); RWEBP_ST8N(q2, 2); }
         RWEBP_ST8N(p1, -2); RWEBP_ST8N(p0, -1);
         RWEBP_ST8N(q0,  0); RWEBP_ST8N(q1,  1);
#undef RWEBP_ST8N
      }
      return;
   }
   if ((n == 16 || n == 8) && tp == 1)
   {
      uint8_t tmp[8][16];
      int i, k;
      uint8x16_t p3, p2, p1, p0, q0, q1, q2, q3, mv, hv;
      for (i = 0; i < n; i++)
      {
         const uint8_t *r = s + i*sp - 4;
         for (k = 0; k < 8; k++)
            tmp[k][i] = r[k];
      }
      for (; i < 16; i++)
         for (k = 0; k < 8; k++)
            tmp[k][i] = 0;
      p3 = vld1q_u8(tmp[0]); p2 = vld1q_u8(tmp[1]);
      p1 = vld1q_u8(tmp[2]); p0 = vld1q_u8(tmp[3]);
      q0 = vld1q_u8(tmp[4]); q1 = vld1q_u8(tmp[5]);
      q2 = vld1q_u8(tmp[6]); q3 = vld1q_u8(tmp[7]);
      vp8_nlf_mask_hev_n(p3, p2, p1, p0, q0, q1, q2, q3,
            lim, edge_lim, thr, &mv, &hv);
      if (is_mb)
         vp8_nlf_mb_n(mv, hv, &p2, &p1, &p0, &q0, &q1, &q2);
      else
         vp8_nlf_inner_n(mv, hv, &p1, &p0, &q0, &q1);
      vst1q_u8(tmp[1], p2); vst1q_u8(tmp[2], p1);
      vst1q_u8(tmp[3], p0); vst1q_u8(tmp[4], q0);
      vst1q_u8(tmp[5], q1); vst1q_u8(tmp[6], q2);
      for (i = 0; i < n; i++)
      {
         uint8_t *r = s + i*sp - 4;
         if (is_mb) { r[1] = tmp[1][i]; r[6] = tmp[6][i]; }
         r[2] = tmp[2][i]; r[3] = tmp[3][i];
         r[4] = tmp[4][i]; r[5] = tmp[5][i];
      }
      return;
   }
#endif /* RWEBP_LF_NEON */
   {
      int i;
      for (i = 0; i < n; i++)
      {
         int p3 = s[-4*tp], p2 = s[-3*tp], p1 = s[-2*tp], p0 = s[-1*tp];
         int q0 = s[0],     q1 = s[1*tp],  q2 = s[2*tp],  q3 = s[3*tp];
         int mask = vp8_nlf_mask(lim, edge_lim, p3, p2, p1, p0, q0, q1, q2, q3);
         int hev  = vp8_nlf_hev(thr, p1, p0, q0, q1);
         if (is_mb)
            vp8_nlf_mb(mask, hev, s-3*tp, s-2*tp, s-1*tp, s, s+1*tp, s+2*tp);
         else
            vp8_nlf_inner(mask, hev, s-2*tp, s-1*tp, s, s+1*tp);
         s += sp;
      }
   }
}

static void vp8_loop_filter_normal(uint8_t *y, int ys,
   uint8_t *u, uint8_t *v_plane, int uvs,
   int mbw, int lf_my0, int lf_my1, int lf_level, int sharpness,
   int seg_enabled, int seg_abs, const int *seg_lf,
   int lf_delta_enabled, const int *ref_lf_delta, const int *mode_lf_delta,
   const uint8_t *seg_map, const uint8_t *skip_lf_map, const uint8_t *bpred_map,
   const void *mb_info)
{
   /* Mode -> loop-filter delta index (libvpx mode_lf_lut): intra 16x16
    * modes = 1, B_PRED = 0, ZEROMV = 1, NEAREST/NEAR/NEW = 2, SPLIT = 3. */
   const rvp8_mbinfo *mbi = (const rvp8_mbinfo*)mb_info;
   int mx, my, e;
   for (my = lf_my0; my < lf_my1; my++)
   {
      for (mx = 0; mx < mbw; mx++)
      {
         int n = my * mbw + mx;
         int lvl;
         int lim, blim, mblim, thr, skip_lf;
         uint8_t *my0 = y + my * 16 * ys + mx * 16;
         uint8_t *mu0 = u + my * 8 * uvs + mx * 8;
         uint8_t *mv0 = v_plane + my * 8 * uvs + mx * 8;

         lvl = rvp8_mb_filter_level(n, lf_level,
               seg_enabled, seg_abs, seg_lf,
               lf_delta_enabled, ref_lf_delta, mode_lf_delta,
               seg_map, bpred_map, mb_info);
         if (lvl == 0) continue;

         /* Limits per vp8_loop_filter_update_sharpness */
         lim = lvl >> ((sharpness > 0) + (sharpness > 4));
         if (sharpness > 0 && lim > 9 - sharpness) lim = 9 - sharpness;
         if (lim < 1) lim = 1;
         mblim = 2 * (lvl + 2) + lim;
         blim  = 2 * lvl + lim;
         /* High-edge-variance threshold (libvpx hev_thr_lut): frame-type
          * dependent. Inter frames use a higher index at each level. The
          * filter receives a non-NULL mbi only for inter frames. */
         if (mbi)  /* inter frame */
            thr = (lvl >= 40) ? 3 : (lvl >= 20) ? 2 : (lvl >= 15) ? 1 : 0;
         else      /* key frame */
            thr = (lvl >= 40) ? 2 : (lvl >= 15) ? 1 : 0;

         skip_lf = skip_lf_map ? skip_lf_map[n] : 0;

         /* Edge order per libvpx: mbv, bv, mbh, bh; chroma included. */
         if (mx > 0)
         {
            vp8_nlf_edge(my0, 1, ys, 16, mblim, lim, thr, 1);
            vp8_nlf_edge(mu0, 1, uvs, 8, mblim, lim, thr, 1);
            vp8_nlf_edge(mv0, 1, uvs, 8, mblim, lim, thr, 1);
         }
         if (!skip_lf)
         {
            for (e = 4; e <= 12; e += 4)
               vp8_nlf_edge(my0 + e, 1, ys, 16, blim, lim, thr, 0);
            vp8_nlf_edge(mu0 + 4, 1, uvs, 8, blim, lim, thr, 0);
            vp8_nlf_edge(mv0 + 4, 1, uvs, 8, blim, lim, thr, 0);
         }
         if (my > 0)
         {
            vp8_nlf_edge(my0, ys, 1, 16, mblim, lim, thr, 1);
            vp8_nlf_edge(mu0, uvs, 1, 8, mblim, lim, thr, 1);
            vp8_nlf_edge(mv0, uvs, 1, 8, mblim, lim, thr, 1);
         }
         if (!skip_lf)
         {
            for (e = 4; e <= 12; e += 4)
               vp8_nlf_edge(my0 + e * ys, ys, 1, 16, blim, lim, thr, 0);
            vp8_nlf_edge(mu0 + 4 * uvs, uvs, 1, 8, blim, lim, thr, 0);
            vp8_nlf_edge(mv0 + 4 * uvs, uvs, 1, 8, blim, lim, thr, 0);
         }
      }
   }
}

static void vp8_pred16(uint8_t *d, int s, int m, const uint8_t *a, const uint8_t *l, uint8_t tl,
      int up_avail, int left_avail)
{
   int i, j;
   switch (m) {
   case 0: { /* DC with libvpx availability logic */
             int dc = 128;
             if (up_avail || left_avail) {
                int sum = 0, shift = 3 + up_avail + left_avail;
                if (up_avail)   for(i=0;i<16;i++) sum += a[i];
                if (left_avail) for(i=0;i<16;i++) sum += l[i];
                dc = (sum + (1 << (shift - 1))) >> shift;
             }
             for(j=0;j<16;j++) memset(d+j*s,(uint8_t)dc,16);
             break; }
   case 1: for(j=0;j<16;j++) memcpy(d+j*s,a,16); break;
   case 2: for(j=0;j<16;j++) memset(d+j*s,l[j],16); break;
   case 3: for(j=0;j<16;j++) for(i=0;i<16;i++) d[j*s+i]=vp8_cl((int)a[i]+(int)l[j]-(int)tl); break;
   }
}

static void vp8_pred8(uint8_t *d, int s, int m, const uint8_t *a, const uint8_t *l, uint8_t tl,
      int up_avail, int left_avail)
{
   int i, j;
   switch (m) {
   case 0: { /* DC with libvpx availability logic */
             int dc = 128;
             if (up_avail || left_avail) {
                int sum = 0, shift = 2 + up_avail + left_avail;
                if (up_avail)   for(i=0;i<8;i++) sum += a[i];
                if (left_avail) for(i=0;i<8;i++) sum += l[i];
                dc = (sum + (1 << (shift - 1))) >> shift;
             }
             for(j=0;j<8;j++) memset(d+j*s,(uint8_t)dc,8);
             break; }
   case 1: for(j=0;j<8;j++) memcpy(d+j*s,a,8); break;
   case 2: for(j=0;j<8;j++) memset(d+j*s,l[j],8); break;
   case 3: for(j=0;j<8;j++) for(i=0;i<8;i++) d[j*s+i]=vp8_cl((int)a[i]+(int)l[j]-(int)tl); break;
   }
}

/* Resumable VP8 decode state. All state that must survive between
 * row-batch calls lives here (including the coefficient probabilities
 * and the chroma-interpolation scratch, which used to be file globals:
 * a suspended decode must not share mutable state with a concurrent
 * one). */

void rvp8_abort(rvp8_dec *s)
{
   free(s->seg_map_buf); free(s->skip_lf_buf); free(s->bpred_buf);
   free(s->yb); free(s->ub); free(s->vb);
   free(s->above_nz_y); free(s->above_nz_u); free(s->above_nz_v);
   free(s->above_nz_dc); free(s->above_bmodes);
   free(s->fancy_uv);
   memset(s, 0, sizeof(*s));
}

/* Parse the frame + picture headers, size all buffers, initialize the
 * token partitions and probability tables. Returns 0 on success; on
 * failure the state needs no cleanup. */
int rvp8_begin(const uint8_t *data, size_t len, rvp8_dec *s)
{
   uint32_t ft;
   int kf, w, h, mbw, mbh, i;
   uint32_t p0s;
   int base_qp, y1dc_dq, y2dc_dq, y2ac_dq, uvdc_dq, uvac_dq;
   int skip_enabled, prob_skip, log2parts, num_parts;
   int filter_type, lf_level, sharpness;
   int lf_delta_enabled = 0;
   int ref_lf_delta[4] = {0,0,0,0}, mode_lf_delta[4] = {0,0,0,0};
   int seg_enabled, seg_abs, seg_qp[4], seg_lf[4], seg_prob[3];
   rvp8_bool br;
   rvp8_bool tbr[8]; /* up to 8 token partitions */
   const uint8_t *p0;

   /* Preserve caller-supplied inter setup across the state reset: for an
    * inter frame the persistent decoder fills in dimensions, reference
    * planes and the inherited probability context before calling begin. */
   int      save_is_inter;
   int      save_w, save_h;
   const uint8_t *save_ry[3], *save_ru[3], *save_rv[3];
   int      save_rys[3], save_ruvs[3];
   uint8_t  save_cprob[4][8][3][11];
   uint8_t  save_mvc[2][19], save_ym[4], save_uvm[3];
   int      save_reflfd[4], save_modelfd[4];
   void    *save_mbinfo;

   /* Free any per-frame buffers from a previous decode on this state
    * (the persistent video decoder reuses one rvp8_dec across frames).
    * The reference frames live in the wrapper, not here, so freeing these
    * scratch buffers is safe. Persistent probability/delta/mbinfo state is
    * saved and restored around the memset below. */
   free(s->seg_map_buf); free(s->skip_lf_buf); free(s->bpred_buf);
   free(s->yb); free(s->ub); free(s->vb);
   free(s->above_nz_y); free(s->above_nz_u); free(s->above_nz_v);
   free(s->above_nz_dc); free(s->above_bmodes);
   free(s->fancy_uv);
   s->seg_map_buf = s->skip_lf_buf = s->bpred_buf = NULL;
   s->yb = s->ub = s->vb = NULL;
   s->above_nz_y = s->above_nz_u = s->above_nz_v = NULL;
   s->above_nz_dc = s->above_bmodes = s->fancy_uv = NULL;

   save_is_inter = s->is_inter;
   save_w = s->w; save_h = s->h;
   save_mbinfo = s->mb_info;
   {
      int a;
      for (a = 0; a < 3; a++)
      {
         save_ry[a]  = s->ref_y[a];  save_ru[a] = s->ref_u[a];
         save_rv[a]  = s->ref_v[a];
         save_rys[a] = s->ref_ys[a]; save_ruvs[a] = s->ref_uvs[a];
      }
      if (save_is_inter)
      {
         memcpy(save_cprob, s->cprob, sizeof(save_cprob));
         memcpy(save_mvc, s->mvc, sizeof(save_mvc));
         memcpy(save_ym,  s->ymode_prob,  sizeof(save_ym));
         memcpy(save_uvm, s->uvmode_prob, sizeof(save_uvm));
         memcpy(save_reflfd, s->ref_lf_delta, sizeof(save_reflfd));
         memcpy(save_modelfd, s->mode_lf_delta, sizeof(save_modelfd));
      }
   }

   memset(s, 0, sizeof(*s));

   if (save_is_inter)
   {
      int a;
      s->is_inter = 1;
      s->w = save_w; s->h = save_h;
      for (a = 0; a < 3; a++)
      {
         s->ref_y[a]  = save_ry[a];  s->ref_u[a] = save_ru[a];
         s->ref_v[a]  = save_rv[a];
         s->ref_ys[a] = save_rys[a]; s->ref_uvs[a] = save_ruvs[a];
      }
      s->mb_info = save_mbinfo;
      memcpy(s->cprob, save_cprob, sizeof(save_cprob));
      memcpy(s->mvc, save_mvc, sizeof(save_mvc));
      memcpy(s->ymode_prob,  save_ym,  sizeof(save_ym));
      memcpy(s->uvmode_prob, save_uvm, sizeof(save_uvm));
      memcpy(s->ref_lf_delta, save_reflfd, sizeof(save_reflfd));
      memcpy(s->mode_lf_delta, save_modelfd, sizeof(save_modelfd));
   }

   if (len < 3) return -1;
   ft = (uint32_t)data[0] | ((uint32_t)data[1]<<8) | ((uint32_t)data[2]<<16);
   kf = !(ft & 1);
   /* Version (RFC 6386 s9.1) selects the reconstruction filters:
    * 0 = six-tap sub-pel MC; 1-3 = bilinear MC; 3 additionally restricts
    * chroma MVs to whole pixels.  (Versions 2/3 also imply the encoder
    * writes loop-filter level 0, which the existing level gate honours;
    * reserved versions 4-7 fall back to the version-0 filters, matching
    * libvpx vp8_setup_version.) */
   {
      int version = (int)((ft >> 1) & 7);
      s->use_bilinear = (version >= 1 && version <= 3);
      s->full_pixel   = (version == 3);
   }
   p0s = (ft >> 5) & 0x7FFFF;
   if (kf)
   {
      if (len < 10) return -1;
      if (data[3]!=0x9D || data[4]!=0x01 || data[5]!=0x2A) return -1;
      w = (data[6]|(data[7]<<8)) & 0x3FFF;
      h = (data[8]|(data[9]<<8)) & 0x3FFF;
      if (!w || !h || w > 16384 || h > 16384) return -1;
      p0 = data + 10;
   }
   else
   {
      /* Inter frame: no sync code or dimensions. The caller must supply
       * them via the persistent decoder (s->w/s->h are preserved across
       * the memset below by the wrapper). Here they arrive in s->w/s->h
       * having been set by rvp8_inter_begin before the memset -- but the
       * memset cleared them, so inter decode is driven through the
       * persistent path (rvp8_video_*), never this raw entry. */
      if (s->w <= 0 || s->h <= 0) return -1;
      w = s->w; h = s->h;
      p0 = data + 3;
   }
   mbw = (w+15) >> 4; mbh = (h+15) >> 4;
   if ((size_t)(p0 - data) + p0s > len) return -1;
   vp8b_init(&br, p0, (size_t)(data + len - p0));

   if (kf)
      { vp8b_bit(&br); vp8b_bit(&br); } /* color_space, clamping */

   /* Segmentation */
   seg_enabled = vp8b_bit(&br);
   seg_abs = 0;
   seg_qp[0] = seg_qp[1] = seg_qp[2] = seg_qp[3] = 0;
   seg_prob[0] = seg_prob[1] = seg_prob[2] = 255;
   seg_lf[0] = seg_lf[1] = seg_lf[2] = seg_lf[3] = 0;
   if (seg_enabled)
   {
      int um = vp8b_bit(&br), ud = vp8b_bit(&br);
      if (ud) {
         seg_abs = vp8b_bit(&br);
         for (i=0;i<4;i++) seg_qp[i] = vp8b_bit(&br) ? vp8b_sig(&br,7) : 0;
         for (i=0;i<4;i++) seg_lf[i] = vp8b_bit(&br) ? vp8b_sig(&br,6) : 0;
      }
      if (um) for (i=0;i<3;i++) { if (vp8b_bit(&br)) seg_prob[i] = (int)vp8b_lit(&br,8); }
   }

   filter_type = vp8b_bit(&br); lf_level = (int)vp8b_lit(&br,6); sharpness = (int)vp8b_lit(&br,3);
   /* Loop-filter deltas persist across frames (VP8 keeps them in the
    * decoder state); seed from the inherited values so an inter frame that
    * doesn't re-send them keeps the key frame's deltas. */
   for (i = 0; i < 4; i++) { ref_lf_delta[i] = s->ref_lf_delta[i]; mode_lf_delta[i] = s->mode_lf_delta[i]; }
   lf_delta_enabled = vp8b_bit(&br);
   if (lf_delta_enabled && vp8b_bit(&br))
   {
      for(i=0;i<4;i++) if(vp8b_bit(&br)) ref_lf_delta[i] = vp8b_sig(&br,6);
      for(i=0;i<4;i++) if(vp8b_bit(&br)) mode_lf_delta[i] = vp8b_sig(&br,6);
   }
   log2parts = vp8b_lit(&br,2);
   num_parts = 1 << log2parts;

   /* Quantizer */
   base_qp = vp8b_lit(&br,7);
   y1dc_dq = vp8b_bit(&br) ? vp8b_sig(&br,4) : 0;
   y2dc_dq = vp8b_bit(&br) ? vp8b_sig(&br,4) : 0;
   y2ac_dq = vp8b_bit(&br) ? vp8b_sig(&br,4) : 0;
   uvdc_dq = vp8b_bit(&br) ? vp8b_sig(&br,4) : 0;
   uvac_dq = vp8b_bit(&br) ? vp8b_sig(&br,4) : 0;

   /* Inter-only reference-refresh + sign-bias flags (RFC 6386 s9.7). */
   if (!kf)
   {
      s->refresh_golden = vp8b_bit(&br);
      s->refresh_altref = vp8b_bit(&br);
      s->copy_golden    = s->refresh_golden ? 0 : (int)vp8b_lit(&br, 2);
      s->copy_altref    = s->refresh_altref ? 0 : (int)vp8b_lit(&br, 2);
      s->sign_bias[RVP8_REF_GOLDEN] = vp8b_bit(&br);
      s->sign_bias[RVP8_REF_ALTREF] = vp8b_bit(&br);
   }
   else
   {
      s->refresh_golden = s->refresh_altref = 1;
      s->copy_golden = s->copy_altref = 0;
      s->sign_bias[RVP8_REF_GOLDEN] = s->sign_bias[RVP8_REF_ALTREF] = 0;
   }

   /* refresh_entropy_probs: when 0, this frame's probability updates are
    * temporary (the persistent decoder restores the saved probs after the
    * frame). The wrapper handles save/restore; here we just record it. */
   s->refresh_entropy = vp8b_bit(&br);

   /* refresh_last_frame comes immediately after refresh_entropy_probs and
    * before the coefficient-probability updates (key frames always
    * refresh). */
   s->refresh_last = kf ? 1 : vp8b_bit(&br);

   /* Coefficient probabilities: a key frame resets to defaults; an inter
    * frame inherits the persistent set (already copied into s->cprob by
    * the wrapper). Updates from the bitstream are applied on top of that. */
   if (kf)
      vp8_init_default_cprob(s->cprob);

   /* Read coefficient probability updates using the fixed update probabilities
    * defined in RFC 6386 §13.4 (Table 2). Each prob may be updated if a flag
    * is read with the corresponding update probability. */
   {
      static const uint8_t cup[4][8][3][11] = {
      {{{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}},
       {{176,246,255,255,255,255,255,255,255,255,255},{223,241,252,255,255,255,255,255,255,255,255},{249,253,253,255,255,255,255,255,255,255,255}},
       {{255,244,252,255,255,255,255,255,255,255,255},{234,254,254,255,255,255,255,255,255,255,255},{253,255,255,255,255,255,255,255,255,255,255}},
       {{255,246,254,255,255,255,255,255,255,255,255},{239,253,254,255,255,255,255,255,255,255,255},{254,255,254,255,255,255,255,255,255,255,255}},
       {{255,248,254,255,255,255,255,255,255,255,255},{251,255,254,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}},
       {{255,253,254,255,255,255,255,255,255,255,255},{251,254,254,255,255,255,255,255,255,255,255},{254,255,254,255,255,255,255,255,255,255,255}},
       {{255,254,253,255,254,255,255,255,255,255,255},{250,255,254,255,254,255,255,255,255,255,255},{254,255,255,255,255,255,255,255,255,255,255}},
       {{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}}},
      {{{217,255,255,255,255,255,255,255,255,255,255},{225,252,241,253,255,255,254,255,255,255,255},{234,250,241,250,253,255,253,254,255,255,255}},
       {{255,254,255,255,255,255,255,255,255,255,255},{223,254,254,255,255,255,255,255,255,255,255},{238,253,254,254,255,255,255,255,255,255,255}},
       {{255,248,254,255,255,255,255,255,255,255,255},{249,254,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}},
       {{255,253,255,255,255,255,255,255,255,255,255},{247,254,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}},
       {{255,253,254,255,255,255,255,255,255,255,255},{252,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}},
       {{255,254,254,255,255,255,255,255,255,255,255},{253,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}},
       {{255,254,253,255,255,255,255,255,255,255,255},{250,255,255,255,255,255,255,255,255,255,255},{254,255,255,255,255,255,255,255,255,255,255}},
       {{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}}},
      {{{186,251,250,255,255,255,255,255,255,255,255},{234,251,244,254,255,255,255,255,255,255,255},{251,251,243,253,254,255,254,255,255,255,255}},
       {{255,253,254,255,255,255,255,255,255,255,255},{236,253,254,255,255,255,255,255,255,255,255},{251,253,253,254,254,255,255,255,255,255,255}},
       {{255,254,254,255,255,255,255,255,255,255,255},{254,254,254,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}},
       {{255,254,255,255,255,255,255,255,255,255,255},{254,254,255,255,255,255,255,255,255,255,255},{254,255,255,255,255,255,255,255,255,255,255}},
       {{255,255,255,255,255,255,255,255,255,255,255},{254,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}},
       {{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}},
       {{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}},
       {{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}}},
      {{{248,255,255,255,255,255,255,255,255,255,255},{250,254,252,254,255,255,255,255,255,255,255},{248,254,249,253,255,255,255,255,255,255,255}},
       {{255,253,253,255,255,255,255,255,255,255,255},{246,253,253,255,255,255,255,255,255,255,255},{252,254,251,254,254,255,255,255,255,255,255}},
       {{255,254,252,255,255,255,255,255,255,255,255},{248,254,253,255,255,255,255,255,255,255,255},{253,255,254,254,255,255,255,255,255,255,255}},
       {{255,251,254,255,255,255,255,255,255,255,255},{245,251,254,255,255,255,255,255,255,255,255},{253,253,254,255,255,255,255,255,255,255,255}},
       {{255,251,253,255,255,255,255,255,255,255,255},{252,253,254,255,255,255,255,255,255,255,255},{255,254,255,255,255,255,255,255,255,255,255}},
       {{255,252,255,255,255,255,255,255,255,255,255},{249,255,254,255,255,255,255,255,255,255,255},{255,255,254,255,255,255,255,255,255,255,255}},
       {{255,255,253,255,255,255,255,255,255,255,255},{250,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}},
       {{255,255,255,255,255,255,255,255,255,255,255},{254,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}}}
      };
      int t, b, c, p;
      for (t = 0; t < 4; t++)
         for (b = 0; b < 8; b++)
            for (c = 0; c < 3; c++)
               for (p = 0; p < 11; p++)
                  if (vp8b_get(&br, cup[t][b][c][p]))
                     s->cprob[t][b][c][p] = (uint8_t)vp8b_lit(&br, 8);
   }

   /* Dump some probs */
   skip_enabled = vp8b_bit(&br);
   prob_skip = skip_enabled ? (int)vp8b_lit(&br, 8) : 0;

   /* Inter-only: refresh_last, the intra/inter and ref-frame selection
    * probabilities, optional Y/UV intra-mode prob updates, and the motion
    * vector probability updates (RFC 6386 s9.9-9.10). */
   if (!kf)
   {
      int c, k;
      s->prob_intra   = (int)vp8b_lit(&br, 8);
      s->prob_last    = (int)vp8b_lit(&br, 8);
      s->prob_golden  = (int)vp8b_lit(&br, 8);
      /* Y intra-mode probs update (used by intra MBs in an inter frame). */
      if (vp8b_bit(&br))
         for (k = 0; k < 4; k++) s->ymode_prob[k] = (uint8_t)vp8b_lit(&br, 8);
      /* UV intra-mode probs update. */
      if (vp8b_bit(&br))
         for (k = 0; k < 3; k++) s->uvmode_prob[k] = (uint8_t)vp8b_lit(&br, 8);
      /* MV probability updates. */
      for (c = 0; c < 2; c++)
         for (k = 0; k < MVPCOUNT; k++)
            if (vp8b_get(&br, vp8_mv_update_probs[c][k]))
            {
               int v = (int)vp8b_lit(&br, 7);
               s->mvc[c][k] = (uint8_t)(v ? (v << 1) : 1);
            }
   }

   /* Initialize token partitions. Everything below is bounds-checked
    * against [data, data+len) so a truncated or hostile size table can
    * never form an out-of-range pointer or a wrapped length. */
   {
      const uint8_t *const end = data + len;
      const uint8_t *tp_base = p0 + p0s;
      const uint8_t *tp_data;
      size_t part_sizes[8];
      size_t avail, hdr_bytes;
      int np;

      if (num_parts > 8) num_parts = 8;

      /* The (num_parts - 1) 3-byte size entries must fit before the data. */
      if (tp_base < data || tp_base > end) goto pfail_tp;
      hdr_bytes = (size_t)(num_parts - 1) * 3;
      if ((size_t)(end - tp_base) < hdr_bytes) goto pfail_tp;
      tp_data = tp_base + hdr_bytes;

      for (np = 0; np < num_parts - 1; np++)
      {
         const uint8_t *e = tp_base + np * 3;
         part_sizes[np] = (size_t)e[0]
                        | ((size_t)e[1] << 8)
                        | ((size_t)e[2] << 16);
      }

      /* Clamp each declared size to what actually remains, then give the
       * final partition whatever is left. */
      avail = (size_t)(end - tp_data);
      {
         size_t used = 0;
         for (np = 0; np < num_parts - 1; np++)
         {
            if (part_sizes[np] > avail - used)
               part_sizes[np] = avail - used;
            used += part_sizes[np];
         }
         part_sizes[num_parts - 1] = avail - used;
      }

      for (np = 0; np < num_parts; np++)
      {
         vp8b_init(&tbr[np], tp_data, part_sizes[np]);
         tp_data += part_sizes[np];
      }
   }
   goto tp_ok;
pfail_tp:
   /* Truncated partition header: point every partition at an empty span
    * so the bool decoders read the padding pattern rather than OOB. */
   {
      int np;
      if (num_parts > 8) num_parts = 8;
      for (np = 0; np < num_parts; np++)
         vp8b_init(&tbr[np], data + len, 0);
   }
tp_ok:
   ;


   s->w = w; s->h = h; s->mbw = mbw; s->mbh = mbh;
   s->ys = mbw * 16; s->uvs = mbw * 8;
   s->base_qp = base_qp;
   s->y1dc_dq = y1dc_dq; s->y2dc_dq = y2dc_dq; s->y2ac_dq = y2ac_dq;
   s->uvdc_dq = uvdc_dq; s->uvac_dq = uvac_dq;
   s->skip_enabled = skip_enabled; s->prob_skip = prob_skip;
   s->num_parts = num_parts;
   s->filter_type = filter_type; s->lf_level = lf_level;
   s->sharpness = sharpness; s->lf_delta_enabled = lf_delta_enabled;
   for (i = 0; i < 4; i++)
   {
      s->ref_lf_delta[i]  = ref_lf_delta[i];
      s->mode_lf_delta[i] = mode_lf_delta[i];
      s->seg_qp[i] = seg_qp[i];
      s->seg_lf[i] = seg_lf[i];
   }
   s->seg_enabled = seg_enabled; s->seg_abs = seg_abs;
   for (i = 0; i < 3; i++)
      s->seg_prob[i] = seg_prob[i];
   s->br = br;
   for (i = 0; i < 8; i++)
      s->tbr[i] = tbr[i];
   s->my = 0;

   s->seg_map_buf = (uint8_t*)calloc(mbw * mbh, 1);
   s->skip_lf_buf = (uint8_t*)calloc(mbw * mbh, 1);
   s->bpred_buf   = (uint8_t*)calloc(mbw * mbh, 1);
   s->yb = (uint8_t*)calloc(s->ys * mbh * 16, 1);
   s->ub = (uint8_t*)calloc(s->uvs * mbh * 8, 1);
   s->vb = (uint8_t*)calloc(s->uvs * mbh * 8, 1);
   s->above_nz_y   = (uint8_t*)calloc(mbw * 4, 1);
   s->above_nz_u   = (uint8_t*)calloc(mbw * 2, 1);
   s->above_nz_v   = (uint8_t*)calloc(mbw * 2, 1);
   s->above_nz_dc  = (uint8_t*)calloc(mbw, 1);
   s->above_bmodes = (uint8_t*)calloc(mbw * 4, 1);
   s->fancy_uv     = (uint8_t*)malloc((size_t)w * 2); /* NULL tolerated */
   /* Per-MB info for inter prediction (allocated once; persists across the
    * frame). The persistent decoder may pass one in via s->mb_info; if not
    * present and this is an inter frame, allocate it here. */
   if (s->is_inter && !s->mb_info)
      s->mb_info = calloc((size_t)mbw * mbh, sizeof(rvp8_mbinfo));
   if (!s->yb || !s->ub || !s->vb
         || !s->above_nz_y || !s->above_nz_u || !s->above_nz_v
         || !s->above_nz_dc || !s->above_bmodes)
   {
      rvp8_abort(s);
      return -1;
   }
   memset(s->yb, 127, s->ys * mbh * 16);
   memset(s->ub, 127, s->uvs * mbh * 8);
   memset(s->vb, 127, s->uvs * mbh * 8);
   return 0;
}

/* Decode up to nrows MB rows (resumable). Returns 1 while rows remain,
 * 0 when the last row has been decoded. The body below is the original
 * decode loop verbatim: read-only configuration is copied to same-named
 * locals, and only the bool decoders and probability tables (which
 * mutate across calls) go through the state struct. */

/* ==================================================================== */
/* Inter MB mode + motion-vector decode (RFC 6386 s16-17;               */
/* mirrors libvpx vp8/decoder/decodemv.c read_mb_modes_mv).             */
/* ==================================================================== */


/* Small-MV tree (RFC 6386): magnitudes 0..7. */
static const int8_t rvp8_small_mvtree[14] =
   { 2, 8, 4, 6, 0, -1, -2, -3, 10, 12, -4, -5, -6, -7 };

/* Walk a VP8 probability tree. Negative entries are leaves (return ~leaf,
 * i.e. -entry). */
static int rvp8_treed_read(rvp8_bool *b, const int8_t *tree,
      const uint8_t *probs)
{
   int i = 0;
   while ((i = tree[i + vp8b_get(b, probs[i >> 1])]) > 0)
      ;
   return -i;
}

/* MV component indices within the 19-entry prob array. */
#define RVP8_mvpis_short 0
#define RVP8_MVPsign     1
#define RVP8_MVPshort    2
#define RVP8_MVPbits     9
#define RVP8_mvlong_width 10

static int rvp8_read_mvcomponent(rvp8_bool *b, const uint8_t *p)
{
   int x = 0;
   if (vp8b_get(b, p[RVP8_mvpis_short]))       /* large */
   {
      int i = 0;
      do { x += vp8b_get(b, p[RVP8_MVPbits + i]) << i; } while (++i < 3);
      i = RVP8_mvlong_width - 1;
      do { x += vp8b_get(b, p[RVP8_MVPbits + i]) << i; } while (--i > 3);
      if (!(x & 0xFFF0) || vp8b_get(b, p[RVP8_MVPbits + 3])) x += 8;
   }
   else                                         /* small */
      x = rvp8_treed_read(b, rvp8_small_mvtree, p + RVP8_MVPshort);
   if (x && vp8b_get(b, p[RVP8_MVPsign])) x = -x;
   return x;
}

/* Read a full MV (row then col), each component doubled (VP8 stores MVs in
 * quarter-pel; the *2 makes them eighth-pel for the interpolation stage). */
static void rvp8_read_mv(rvp8_bool *b, rvp8_mv *mv, uint8_t mvc[2][19])
{
   mv->y = (int16_t)(rvp8_read_mvcomponent(b, mvc[0]) * 2);
   mv->x = (int16_t)(rvp8_read_mvcomponent(b, mvc[1]) * 2);
}

/* Sign-bias adjustment: if the neighbour's reference has opposite sign bias
 * to ours, negate its MV. */
static INLINE void rvp8_mv_bias(int nb_bias, int my_bias, rvp8_mv *mv)
{
   if (nb_bias != my_bias)
   {
      mv->x = (int16_t)-mv->x;
      mv->y = (int16_t)-mv->y;
   }
}

#define RVP8_CNT_INTRA   0
#define RVP8_CNT_NEAREST 1
#define RVP8_CNT_NEAR    2
#define RVP8_CNT_SPLIT   3

/* Decode one inter/intra MB's mode and MV. 'above','left','aboveleft' are
 * the neighbour mbinfos (may be NULL at frame edges -> treated as intra
 * with zero MV). Fills 'out'. Advances the boolean decoder s->br. */

/* SPLITMV sub-block motion decode (RFC 6386 s16.2; libvpx decode_split_mv). */
static const uint8_t rvp8_mbsplit_offset[4][16] = {
   { 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 2, 8, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }
};
static const uint8_t rvp8_mbsplit_fill_count[4] = { 8, 8, 4, 1 };
static const uint8_t rvp8_mbsplit_fill_offset[4][16] = {
   { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
   { 0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15 },
   { 0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15 },
   { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }
};
static const uint8_t rvp8_sub_mv_ref_prob2[5][3] = {
   { 147, 136,  18 },
   { 106, 145,   1 },
   { 179, 121,   1 },
   { 223,   1,  34 },
   { 208,   1,   1 }
};

/* SUBMVREF selection from left/above sub-block MVs (get_sub_mv_ref_prob). */
static const uint8_t *rvp8_sub_mv_ref_prob(int32_t left, int32_t above)
{
   int lez = (left == 0), aez = (above == 0), lea = (left == above);
   int idx;
   if (lea && lez) idx = 4;              /* LEFT_ABOVE_ZED   */
   else if (lea)   idx = 3;              /* LEFT_ABOVE_SAME  */
   else if (aez)   idx = 2;              /* ABOVE_ZED        */
   else if (lez)   idx = 1;              /* LEFT_ZED         */
   else            idx = 0;              /* NORMAL           */
   return rvp8_sub_mv_ref_prob2[idx];
}

static INLINE int32_t rvp8_mvint(rvp8_mv m)
{
   /* Pack x/y into one word for equality tests. Shift in unsigned to
    * avoid signed-overflow UB when x has its high bit set. */
   return (int32_t)(((uint32_t)(uint16_t)m.x << 16) | (uint16_t)m.y);
}

/* Decode the SPLITMV sub-block MVs into out->bmv[16]; best_mv is the
 * near_mvs[near_index] "best" MV. left/above are neighbour mbinfos. */
static void rvp8_decode_split_mv(rvp8_dec *s, rvp8_mbinfo *out,
      const rvp8_mbinfo *left, const rvp8_mbinfo *above, rvp8_mv best_mv)
{
   int sc, num_p, j;
   sc = 3; num_p = 16;
   if (vp8b_get(&s->br, 110))
   {
      sc = 2; num_p = 4;
      if (vp8b_get(&s->br, 111))
      {
         sc = vp8b_get(&s->br, 150);
         num_p = 2;
      }
   }
   for (j = 0; j < num_p; j++)
   {
      rvp8_mv leftmv, abovemv, blockmv;
      const uint8_t *prob;
      int k = rvp8_mbsplit_offset[sc][j];
      int fc, fi;

      if (!(k & 3))
      {
         if (left && left->is_split)
            leftmv = left->bmv[k + 4 - 1];
         else if (left)
            leftmv = left->mv;
         else
            leftmv.x = leftmv.y = 0;
      }
      else
         leftmv = out->bmv[k - 1];

      if (!(k >> 2))
      {
         if (above && above->is_split)
            abovemv = above->bmv[k + 16 - 4];
         else if (above)
            abovemv = above->mv;
         else
            abovemv.x = abovemv.y = 0;
      }
      else
         abovemv = out->bmv[k - 4];

      prob = rvp8_sub_mv_ref_prob(rvp8_mvint(leftmv), rvp8_mvint(abovemv));

      if (vp8b_get(&s->br, prob[0]))
      {
         if (vp8b_get(&s->br, prob[1]))
         {
            blockmv.x = blockmv.y = 0;
            if (vp8b_get(&s->br, prob[2]))
            {
               blockmv.y = (int16_t)(rvp8_read_mvcomponent(&s->br, s->mvc[0]) * 2 + best_mv.y);
               blockmv.x = (int16_t)(rvp8_read_mvcomponent(&s->br, s->mvc[1]) * 2 + best_mv.x);
            }
         }
         else
            blockmv = abovemv;
      }
      else
         blockmv = leftmv;

      fc = rvp8_mbsplit_fill_count[sc];
      for (fi = 0; fi < fc; fi++)
         out->bmv[rvp8_mbsplit_fill_offset[sc][j * rvp8_mbsplit_fill_count[sc] + fi]] = blockmv;
   }
   out->mv = out->bmv[15];  /* MB mv = last sub-block (libvpx) */
}

/* Clamp an MV so its reference fetch stays within the frame plus the
 * 16-pixel border libvpx assumes (vp8_clamp_mv2). Edges are in 1/8-pel:
 * to_left/top are <= 0, to_right/bottom >= 0 for interior MBs. */
#define RVP8_MV_MARGIN (16 << 3)
static INLINE void rvp8_clamp_mv(rvp8_mv *mv,
      int to_left, int to_right, int to_top, int to_bottom)
{
   if (mv->x < to_left  - RVP8_MV_MARGIN) mv->x = (int16_t)(to_left  - RVP8_MV_MARGIN);
   else if (mv->x > to_right + RVP8_MV_MARGIN) mv->x = (int16_t)(to_right + RVP8_MV_MARGIN);
   if (mv->y < to_top   - RVP8_MV_MARGIN) mv->y = (int16_t)(to_top   - RVP8_MV_MARGIN);
   else if (mv->y > to_bottom + RVP8_MV_MARGIN) mv->y = (int16_t)(to_bottom + RVP8_MV_MARGIN);
}

static void rvp8_decode_mb_mode(rvp8_dec *s, rvp8_mbinfo *out,
      const rvp8_mbinfo *above, const rvp8_mbinfo *left,
      const rvp8_mbinfo *aboveleft, int mx, int my, int mbw, int mbh)
{
   rvp8_mbinfo intra_nb;
   /* Edges in 1/8-pel (libvpx decodeframe.c): left/top <= 0, right/bottom
    * >= 0 for interior MBs. */
   int to_left   = -((mx * 16) << 3);
   int to_right  = ((mbw - 1 - mx) * 16) << 3;
   int to_top    = -((my * 16) << 3);
   int to_bottom = ((mbh - 1 - my) * 16) << 3;
   memset(&intra_nb, 0, sizeof(intra_nb)); /* ref_frame=0 (intra), mv=0 */
   if (!above)     above     = &intra_nb;
   if (!left)      left      = &intra_nb;
   if (!aboveleft) aboveleft = &intra_nb;

   out->is_split = 0;
   out->mv.x = out->mv.y = 0;
   memset(out->bmv, 0, sizeof(out->bmv));

   out->ref_frame = (uint8_t)vp8b_get(&s->br, s->prob_intra);
   if (out->ref_frame)  /* inter */
   {
      rvp8_mv near_mvs[4];
      int cnt[4];
      int ci = 0;      /* index into near_mvs, mirrors libvpx nmv pointer */
      int cx = 0;      /* index into cnt, mirrors cntx pointer            */
      int near_index;
      const rvp8_mbinfo *nb[3];
      int wj;

      if (vp8b_get(&s->br, s->prob_last))
         out->ref_frame = (uint8_t)(2 + vp8b_get(&s->br, s->prob_golden));

      near_mvs[0].x = near_mvs[0].y = 0;
      near_mvs[1].x = near_mvs[1].y = 0;
      near_mvs[2].x = near_mvs[2].y = 0;
      near_mvs[3].x = near_mvs[3].y = 0;
      cnt[0] = cnt[1] = cnt[2] = cnt[3] = 0;

      nb[0] = above; nb[1] = left; nb[2] = aboveleft;
      for (wj = 0; wj < 3; wj++)
      {
         const rvp8_mbinfo *n = nb[wj];
         int weight = (wj == 2) ? 1 : 2; /* above/left weight 2, aboveleft 1 */
         if (n->ref_frame != 0)  /* neighbour is inter */
         {
            if (n->mv.x || n->mv.y)
            {
               rvp8_mv tm = n->mv;
               rvp8_mv_bias(s->sign_bias[n->ref_frame], s->sign_bias[out->ref_frame], &tm);
               if (ci == 0 || tm.x != near_mvs[ci].x || tm.y != near_mvs[ci].y)
               {
                  ci++;
                  near_mvs[ci] = tm;
                  cx = ci;
               }
               cnt[cx] += weight;
            }
            else
               cnt[RVP8_CNT_INTRA] += weight;
         }
      }

      /* mode tree */
      if (vp8b_get(&s->br, vp8_mode_contexts[cnt[RVP8_CNT_INTRA]][0]))
      {
         cnt[RVP8_CNT_NEAREST] += ((cnt[RVP8_CNT_SPLIT] > 0) &
               (near_mvs[ci].x == near_mvs[RVP8_CNT_NEAREST].x &&
                near_mvs[ci].y == near_mvs[RVP8_CNT_NEAREST].y));
         if (cnt[RVP8_CNT_NEAR] > cnt[RVP8_CNT_NEAREST])
         {
            int t = cnt[RVP8_CNT_NEAREST];
            rvp8_mv tv = near_mvs[RVP8_CNT_NEAREST];
            cnt[RVP8_CNT_NEAREST] = cnt[RVP8_CNT_NEAR];
            cnt[RVP8_CNT_NEAR] = t;
            near_mvs[RVP8_CNT_NEAREST] = near_mvs[RVP8_CNT_NEAR];
            near_mvs[RVP8_CNT_NEAR] = tv;
         }
         if (vp8b_get(&s->br, vp8_mode_contexts[cnt[RVP8_CNT_NEAREST]][1]))
         {
            if (vp8b_get(&s->br, vp8_mode_contexts[cnt[RVP8_CNT_NEAR]][2]))
            {
               near_index = RVP8_CNT_INTRA +
                  (cnt[RVP8_CNT_NEAREST] >= cnt[RVP8_CNT_INTRA]);
               rvp8_clamp_mv(&near_mvs[near_index], to_left, to_right, to_top, to_bottom);
               /* cnt[SPLITMV] is (re)computed from neighbour SPLITMV modes
                * for the split-vs-new decision (libvpx). above/left weight
                * 2, aboveleft weight 1. */
               cnt[RVP8_CNT_SPLIT] =
                  (((above->ref_frame != 0 && above->mode == MV_SPLIT) +
                    (left->ref_frame  != 0 && left->mode  == MV_SPLIT)) * 2) +
                  (aboveleft->ref_frame != 0 && aboveleft->mode == MV_SPLIT);
               if (vp8b_get(&s->br, vp8_mode_contexts[cnt[RVP8_CNT_SPLIT]][3]))
               {
                  out->mode = MV_SPLIT;
                  out->is_split = 1;
                  rvp8_decode_split_mv(s, out, left, above,
                        near_mvs[near_index]);
               }
               else
               {
                  out->mode = MV_NEW;
                  rvp8_read_mv(&s->br, &out->mv, s->mvc);
                  out->mv.x = (int16_t)(out->mv.x + near_mvs[near_index].x);
                  out->mv.y = (int16_t)(out->mv.y + near_mvs[near_index].y);
               }
            }
            else
            {
               out->mode = MV_NEAR;
               out->mv = near_mvs[RVP8_CNT_NEAR];
               rvp8_clamp_mv(&out->mv, to_left, to_right, to_top, to_bottom);
            }
         }
         else
         {
            out->mode = MV_NEAREST;
            out->mv = near_mvs[RVP8_CNT_NEAREST];
            rvp8_clamp_mv(&out->mv, to_left, to_right, to_top, to_bottom);
         }
      }
      else
      {
         out->mode = MV_ZERO;
         out->mv.x = out->mv.y = 0;
      }
   }
   else  /* intra MB in inter frame */
   {
      /* read_ymode over vp8_ymode_tree {-DC,2,4,6,-V,-H,-TM,-B}: DC first,
       * then V/H, then TM/B_PRED (distinct from the key-frame tree which
       * puts B_PRED first). Then, if B_PRED, 16 bmodes; then uv mode. */
      int ym;
      if (!vp8b_get(&s->br, s->ymode_prob[0]))
         ym = 0; /* DC_PRED */
      else if (!vp8b_get(&s->br, s->ymode_prob[1]))
         ym = vp8b_get(&s->br, s->ymode_prob[2]) ? 2 : 1; /* H : V */
      else
         ym = vp8b_get(&s->br, s->ymode_prob[3]) ? 4 : 3; /* B_PRED : TM */
      out->mode = (uint8_t)(0x80 | ym); /* high bit flags intra-in-inter */
      if (ym == 4)
      {
         int j;
         for (j = 0; j < 16; j++)
            out->bmodes[j] = (uint8_t)vp8_read_bmode_inter(&s->br);
      }
      /* uv mode */
      if      (!vp8b_get(&s->br, s->uvmode_prob[0])) out->uvmode = 0;
      else if (!vp8b_get(&s->br, s->uvmode_prob[1])) out->uvmode = 1;
      else     out->uvmode = vp8b_get(&s->br, s->uvmode_prob[2]) ? 3 : 2;
   }
}


/* ==================================================================== */
/* Inter (motion-compensated) prediction.                               */
/* ==================================================================== */

/* Clamp a source coordinate so the (possibly sub-pel) fetch stays inside
 * the reference plane with room for the 6-tap kernel's 2-pixel halo. */
static INLINE int rvp8_clampc(int v, int lo, int hi)
{
   return v < lo ? lo : v > hi ? hi : v;
}

/* Copy/interpolate one plane block from the reference. (mvx,mvy) are in
 * eighth-pel for luma; chroma uses the same MV but the plane is half-res so
 * the effective fractional precision differs (handled by the caller passing
 * the right subsample shift). For ZEROMV (frac==0) this is a plain copy. */
/* ---- Interior (clamp-free) six-tap motion compensation ----
 * When a block's whole source footprint lies inside the reference
 * frame - the overwhelmingly common case - the per-pixel edge clamps
 * are pure overhead and block vectorization. The kernels below
 * evaluate exactly the scalar expressions: the horizontal pass is
 * pmaddwd/vmlal tap pairs with (acc + 64) >> 7 and a 0..255 clamp
 * into a u8 intermediate (the scalar stores the same clamped values
 * in its int temp), the vertical pass the same from the u8 rows.
 * Sample loads are composed so nothing outside the scalar footprint
 * (columns sx-2 .. sx+bw+2) is touched. */

#if defined(RWEBP_YUV_SSE2)
static void rvp8_mc_h6_sse2(const uint8_t *srcp, int rstride,
      uint8_t *tmp, int bw, int th, const int16_t *hf)
{
   const __m128i zero = _mm_setzero_si128();
   const __m128i rnd  = _mm_set1_epi32(64);
   const __m128i f01  = _mm_set1_epi32(((uint32_t)(uint16_t)hf[1] << 16) | (uint16_t)hf[0]);
   const __m128i f23  = _mm_set1_epi32(((uint32_t)(uint16_t)hf[3] << 16) | (uint16_t)hf[2]);
   const __m128i f45  = _mm_set1_epi32(((uint32_t)(uint16_t)hf[5] << 16) | (uint16_t)hf[4]);
   int i, j;
   for (j = 0; j < th; j++)
   {
      const uint8_t *s = srcp + (size_t)j * rstride;
      uint8_t *d       = tmp + (size_t)j * bw;
      for (i = 0; i + 8 <= bw; i += 8)
      {
         /* 8 outputs need s[i .. i+12] exactly */
         __m128i v0 = _mm_unpacklo_epi8(
               _mm_loadl_epi64((const __m128i*)(s + i)), zero);     /* 0..7  */
         __m128i v5 = _mm_unpacklo_epi8(
               _mm_loadl_epi64((const __m128i*)(s + i + 5)), zero); /* 5..12 */
         __m128i s1 = _mm_or_si128(_mm_srli_si128(v0, 2),
                                   _mm_slli_si128(v5, 8));
         __m128i s2 = _mm_or_si128(_mm_srli_si128(v0, 4),
                                   _mm_slli_si128(v5, 6));
         __m128i s3 = _mm_or_si128(_mm_srli_si128(v0, 6),
                                   _mm_slli_si128(v5, 4));
         __m128i s4 = _mm_or_si128(_mm_srli_si128(v0, 8),
                                   _mm_slli_si128(v5, 2));
         __m128i a_lo, a_hi, r16;
         a_lo = _mm_add_epi32(
               _mm_add_epi32(_mm_madd_epi16(_mm_unpacklo_epi16(v0, s1), f01),
                             _mm_madd_epi16(_mm_unpacklo_epi16(s2, s3), f23)),
                             _mm_madd_epi16(_mm_unpacklo_epi16(s4, v5), f45));
         a_hi = _mm_add_epi32(
               _mm_add_epi32(_mm_madd_epi16(_mm_unpackhi_epi16(v0, s1), f01),
                             _mm_madd_epi16(_mm_unpackhi_epi16(s2, s3), f23)),
                             _mm_madd_epi16(_mm_unpackhi_epi16(s4, v5), f45));
         a_lo = _mm_srai_epi32(_mm_add_epi32(a_lo, rnd), 7);
         a_hi = _mm_srai_epi32(_mm_add_epi32(a_hi, rnd), 7);
         r16  = _mm_packs_epi32(a_lo, a_hi);
         _mm_storel_epi64((__m128i*)(d + i),
               _mm_packus_epi16(r16, r16));
      }
      if (i + 4 <= bw)
      {
         /* 4 outputs need s[i .. i+8] exactly */
         __m128i v0 = _mm_unpacklo_epi8(
               _mm_loadl_epi64((const __m128i*)(s + i)), zero);     /* 0..7 */
         __m128i v1 = _mm_unpacklo_epi8(
               _mm_loadl_epi64((const __m128i*)(s + i + 1)), zero); /* 1..8 */
         __m128i s2 = _mm_srli_si128(v0, 4);
         __m128i s3 = _mm_srli_si128(v0, 6);
         __m128i s4 = _mm_srli_si128(v0, 8);
         __m128i s5 = _mm_srli_si128(v1, 8);
         __m128i a_lo, r16;
         int32_t ov;
         a_lo = _mm_add_epi32(
               _mm_add_epi32(_mm_madd_epi16(_mm_unpacklo_epi16(v0, v1), f01),
                             _mm_madd_epi16(_mm_unpacklo_epi16(s2, s3), f23)),
                             _mm_madd_epi16(_mm_unpacklo_epi16(s4, s5), f45));
         a_lo = _mm_srai_epi32(_mm_add_epi32(a_lo, rnd), 7);
         r16  = _mm_packs_epi32(a_lo, a_lo);
         ov   = _mm_cvtsi128_si32(_mm_packus_epi16(r16, r16));
         memcpy(d + i, &ov, 4);
         i += 4;
      }
      for (; i < bw; i++)
      {
         int acc = 0, t;
         for (t = 0; t < 6; t++)
            acc += hf[t] * s[i + t];
         acc = (acc + 64) >> 7;
         d[i] = (uint8_t)(acc < 0 ? 0 : acc > 255 ? 255 : acc);
      }
   }
}

static void rvp8_mc_v6_sse2(const uint8_t *tmp, uint8_t *dst, int dstride,
      int bw, int bh, const int16_t *vf)
{
   const __m128i zero = _mm_setzero_si128();
   const __m128i rnd  = _mm_set1_epi32(64);
   const __m128i f01  = _mm_set1_epi32(((uint32_t)(uint16_t)vf[1] << 16) | (uint16_t)vf[0]);
   const __m128i f23  = _mm_set1_epi32(((uint32_t)(uint16_t)vf[3] << 16) | (uint16_t)vf[2]);
   const __m128i f45  = _mm_set1_epi32(((uint32_t)(uint16_t)vf[5] << 16) | (uint16_t)vf[4]);
   int i, j;
   for (j = 0; j < bh; j++)
   {
      const uint8_t *s = tmp + (size_t)j * bw;
      uint8_t *d       = dst + (size_t)j * dstride;
      for (i = 0; i + 8 <= bw; i += 8)
      {
         __m128i r0 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*)(s + i + 0*bw)), zero);
         __m128i r1 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*)(s + i + 1*bw)), zero);
         __m128i r2 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*)(s + i + 2*bw)), zero);
         __m128i r3 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*)(s + i + 3*bw)), zero);
         __m128i r4 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*)(s + i + 4*bw)), zero);
         __m128i r5 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*)(s + i + 5*bw)), zero);
         __m128i a_lo, a_hi, r16;
         a_lo = _mm_add_epi32(
               _mm_add_epi32(_mm_madd_epi16(_mm_unpacklo_epi16(r0, r1), f01),
                             _mm_madd_epi16(_mm_unpacklo_epi16(r2, r3), f23)),
                             _mm_madd_epi16(_mm_unpacklo_epi16(r4, r5), f45));
         a_hi = _mm_add_epi32(
               _mm_add_epi32(_mm_madd_epi16(_mm_unpackhi_epi16(r0, r1), f01),
                             _mm_madd_epi16(_mm_unpackhi_epi16(r2, r3), f23)),
                             _mm_madd_epi16(_mm_unpackhi_epi16(r4, r5), f45));
         a_lo = _mm_srai_epi32(_mm_add_epi32(a_lo, rnd), 7);
         a_hi = _mm_srai_epi32(_mm_add_epi32(a_hi, rnd), 7);
         r16  = _mm_packs_epi32(a_lo, a_hi);
         _mm_storel_epi64((__m128i*)(d + i), _mm_packus_epi16(r16, r16));
      }
      if (i + 4 <= bw)
      {
         __m128i r0, r1, r2, r3, r4, r5, a_lo, r16;
         int32_t t;
#define RVP8_LD4(K) \
         (memcpy(&t, s + i + (K)*bw, 4), \
          _mm_unpacklo_epi8(_mm_cvtsi32_si128(t), zero))
         r0 = RVP8_LD4(0); r1 = RVP8_LD4(1); r2 = RVP8_LD4(2);
         r3 = RVP8_LD4(3); r4 = RVP8_LD4(4); r5 = RVP8_LD4(5);
#undef RVP8_LD4
         a_lo = _mm_add_epi32(
               _mm_add_epi32(_mm_madd_epi16(_mm_unpacklo_epi16(r0, r1), f01),
                             _mm_madd_epi16(_mm_unpacklo_epi16(r2, r3), f23)),
                             _mm_madd_epi16(_mm_unpacklo_epi16(r4, r5), f45));
         a_lo = _mm_srai_epi32(_mm_add_epi32(a_lo, rnd), 7);
         r16  = _mm_packs_epi32(a_lo, a_lo);
         t    = _mm_cvtsi128_si32(_mm_packus_epi16(r16, r16));
         memcpy(d + i, &t, 4);
         i += 4;
      }
      for (; i < bw; i++)
      {
         int acc = 0, t;
         for (t = 0; t < 6; t++)
            acc += vf[t] * s[i + t*bw];
         acc = (acc + 64) >> 7;
         d[i] = (uint8_t)(acc < 0 ? 0 : acc > 255 ? 255 : acc);
      }
   }
}
#endif

#if defined(RWEBP_YUV_NEON)
static void rvp8_mc_h6_neon(const uint8_t *srcp, int rstride,
      uint8_t *tmp, int bw, int th, const int16_t *hf)
{
   int16_t ftail[4];
   int16x4_t flo, fhi;
   int i, j;
   memcpy(ftail, hf + 4, 2 * sizeof(int16_t));
   ftail[2] = ftail[3] = 0;
   flo = vld1_s16(hf);
   fhi = vld1_s16(ftail);
   for (j = 0; j < th; j++)
   {
      const uint8_t *s = srcp + (size_t)j * rstride;
      uint8_t *d       = tmp + (size_t)j * bw;
      for (i = 0; i + 8 <= bw; i += 8)
      {
         /* 8 outputs need s[i .. i+12] exactly */
         int16x8_t v0 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s + i)));
         int16x8_t v5 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s + i + 5)));
         int16x8_t s1 = vextq_s16(v0, v5, 1);
         int16x8_t s2 = vextq_s16(v0, v5, 2);
         int16x8_t s3 = vextq_s16(v0, v5, 3);
         int16x8_t s4 = vextq_s16(v0, v5, 4);
         int32x4_t a_lo, a_hi;
         int16x8_t r16;
         a_lo = vmull_lane_s16(vget_low_s16(v0), flo, 0);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(s1), flo, 1);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(s2), flo, 2);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(s3), flo, 3);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(s4), fhi, 0);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(v5), fhi, 1);
         a_hi = vmull_lane_s16(vget_high_s16(v0), flo, 0);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(s1), flo, 1);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(s2), flo, 2);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(s3), flo, 3);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(s4), fhi, 0);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(v5), fhi, 1);
         r16 = vcombine_s16(vqrshrn_n_s32(a_lo, 7), vqrshrn_n_s32(a_hi, 7));
         vst1_u8(d + i, vqmovun_s16(r16));
      }
      for (; i < bw; i++)
      {
         int acc = 0, t;
         for (t = 0; t < 6; t++)
            acc += hf[t] * s[i + t];
         acc = (acc + 64) >> 7;
         d[i] = (uint8_t)(acc < 0 ? 0 : acc > 255 ? 255 : acc);
      }
   }
}

static void rvp8_mc_v6_neon(const uint8_t *tmp, uint8_t *dst, int dstride,
      int bw, int bh, const int16_t *vf)
{
   int16_t ftail[4];
   int16x4_t flo, fhi;
   int i, j;
   memcpy(ftail, vf + 4, 2 * sizeof(int16_t));
   ftail[2] = ftail[3] = 0;
   flo = vld1_s16(vf);
   fhi = vld1_s16(ftail);
   for (j = 0; j < bh; j++)
   {
      const uint8_t *s = tmp + (size_t)j * bw;
      uint8_t *d       = dst + (size_t)j * dstride;
      for (i = 0; i + 8 <= bw; i += 8)
      {
         int16x8_t r0 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s + i + 0*bw)));
         int16x8_t r1 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s + i + 1*bw)));
         int16x8_t r2 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s + i + 2*bw)));
         int16x8_t r3 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s + i + 3*bw)));
         int16x8_t r4 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s + i + 4*bw)));
         int16x8_t r5 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s + i + 5*bw)));
         int32x4_t a_lo, a_hi;
         int16x8_t r16;
         a_lo = vmull_lane_s16(vget_low_s16(r0), flo, 0);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(r1), flo, 1);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(r2), flo, 2);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(r3), flo, 3);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(r4), fhi, 0);
         a_lo = vmlal_lane_s16(a_lo, vget_low_s16(r5), fhi, 1);
         a_hi = vmull_lane_s16(vget_high_s16(r0), flo, 0);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(r1), flo, 1);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(r2), flo, 2);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(r3), flo, 3);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(r4), fhi, 0);
         a_hi = vmlal_lane_s16(a_hi, vget_high_s16(r5), fhi, 1);
         r16 = vcombine_s16(vqrshrn_n_s32(a_lo, 7), vqrshrn_n_s32(a_hi, 7));
         vst1_u8(d + i, vqmovun_s16(r16));
      }
      for (; i < bw; i++)
      {
         int acc = 0, t;
         for (t = 0; t < 6; t++)
            acc += vf[t] * s[i + t*bw];
         acc = (acc + 64) >> 7;
         d[i] = (uint8_t)(acc < 0 ? 0 : acc > 255 ? 255 : acc);
      }
   }
}
#endif

static void rvp8_mc_block(const uint8_t *ref, int rstride,
      uint8_t *dst, int dstride,
      int x, int y, int mvx, int mvy, int bw, int bh,
      int refw, int refh, int use_bilinear)
{
   int fx = mvx & 7, fy = mvy & 7;
   int sx = x + (mvx >> 3);
   int sy = y + (mvy >> 3);
   int i, j;
   if (fx == 0 && fy == 0)
   {
      /* Interior fast path: whole footprint in-bounds */
      if (sx >= 0 && sy >= 0 && sx + bw <= refw && sy + bh <= refh)
      {
         const uint8_t *s = ref + (size_t)sy * rstride + sx;
         for (j = 0; j < bh; j++)
            memcpy(dst + (size_t)j * dstride, s + (size_t)j * rstride, bw);
         return;
      }
      for (j = 0; j < bh; j++)
      {
         int cy = rvp8_clampc(sy + j, 0, refh - 1);
         for (i = 0; i < bw; i++)
         {
            int cx = rvp8_clampc(sx + i, 0, refw - 1);
            dst[j*dstride + i] = ref[cy*rstride + cx];
         }
      }
      return;
   }
   if (use_bilinear)
   {
      /* Sub-pixel: separable 2-tap bilinear (VP8 versions 1-3; libvpx
       * filter_block2d_bil).  First pass filters bh+1 rows horizontally
       * into an unsigned intermediate (taps are non-negative so no
       * clamping is needed); second pass filters vertically from row
       * pairs.  Taps {128-8f, 8f} with the usual +64 >> 7 rounding. */
      const int16_t *hf = vp8_bilinear_filters[fx];
      const int16_t *vf = vp8_bilinear_filters[fy];
      uint16_t tmp[17*16]; /* (bh+1) rows x bw cols, bw<=16, bh<=16 */
      int th = bh + 1;
      for (j = 0; j < th; j++)
      {
         int cy = rvp8_clampc(sy + j, 0, refh - 1);
         for (i = 0; i < bw; i++)
         {
            int c0 = rvp8_clampc(sx + i,     0, refw - 1);
            int c1 = rvp8_clampc(sx + i + 1, 0, refw - 1);
            tmp[j*bw + i] = (uint16_t)((hf[0]*ref[cy*rstride + c0] +
                                        hf[1]*ref[cy*rstride + c1] + 64) >> 7);
         }
      }
      for (j = 0; j < bh; j++)
         for (i = 0; i < bw; i++)
         {
            int acc = (vf[0]*tmp[j*bw + i] + vf[1]*tmp[(j+1)*bw + i] + 64) >> 7;
            dst[j*dstride + i] = (uint8_t)acc;
         }
      return;
   }
   /* Sub-pixel: separable 6-tap. Horizontal then vertical. */
   /* Interior fast path: the scalar footprint is columns
    * sx-2 .. sx+bw+2 and rows sy-2 .. sy+bh+2; when it lies fully
    * inside the reference frame no clamping is needed and the
    * vectorized clamp-free kernels apply. Border blocks (motion
    * vectors reaching off-frame) take the clamped path below. */
   if (   sx - 2 >= 0 && sy - 2 >= 0
       && sx + bw + 3 <= refw && sy + bh + 3 <= refh)
   {
      const int16_t *hf = vp8_sixtap_filters[fx];
      const int16_t *vf = vp8_sixtap_filters[fy];
      const uint8_t *srcp = ref + (size_t)(sy - 2) * rstride + (sx - 2);
      uint8_t tmpb[21*16]; /* (bh+5) rows x bw cols, bw<=16, bh<=16 */
      int th = bh + 5;
#if defined(RWEBP_YUV_SSE2)
      rvp8_mc_h6_sse2(srcp, rstride, tmpb, bw, th, hf);
      rvp8_mc_v6_sse2(tmpb, dst, dstride, bw, bh, vf);
#elif defined(RWEBP_YUV_NEON)
      rvp8_mc_h6_neon(srcp, rstride, tmpb, bw, th, hf);
      rvp8_mc_v6_neon(tmpb, dst, dstride, bw, bh, vf);
#else
      for (j = 0; j < th; j++)
      {
         const uint8_t *s = srcp + (size_t)j * rstride;
         for (i = 0; i < bw; i++)
         {
            int acc = 0, t;
            for (t = 0; t < 6; t++)
               acc += hf[t] * s[i + t];
            acc = (acc + 64) >> 7;
            tmpb[j*bw + i] = (uint8_t)(acc < 0 ? 0 : acc > 255 ? 255 : acc);
         }
      }
      for (j = 0; j < bh; j++)
         for (i = 0; i < bw; i++)
         {
            int acc = 0, t;
            for (t = 0; t < 6; t++)
               acc += vf[t] * tmpb[(j + t)*bw + i];
            acc = (acc + 64) >> 7;
            dst[j*dstride + i] =
                  (uint8_t)(acc < 0 ? 0 : acc > 255 ? 255 : acc);
         }
#endif
      return;
   }
   {
      const int16_t *hf = vp8_sixtap_filters[fx];
      const int16_t *vf = vp8_sixtap_filters[fy];
      int tmp[21*16]; /* (bh+5) rows x bw cols, bw<=16, bh<=16 */
      int th = bh + 5;
      for (j = 0; j < th; j++)
      {
         int cy = rvp8_clampc(sy + j - 2, 0, refh - 1);
         for (i = 0; i < bw; i++)
         {
            int acc = 0, t;
            for (t = 0; t < 6; t++)
            {
               int cx = rvp8_clampc(sx + i + t - 2, 0, refw - 1);
               acc += hf[t] * ref[cy*rstride + cx];
            }
            acc = (acc + 64) >> 7;
            tmp[j*bw + i] = acc < 0 ? 0 : acc > 255 ? 255 : acc;
         }
      }
      for (j = 0; j < bh; j++)
         for (i = 0; i < bw; i++)
         {
            int acc = 0, t;
            for (t = 0; t < 6; t++)
               acc += vf[t] * tmp[(j + t)*bw + i];
            acc = (acc + 64) >> 7;
            dst[j*dstride + i] = (uint8_t)(acc < 0 ? 0 : acc > 255 ? 255 : acc);
         }
   }
}

/* Motion-compensate a whole MB (16x16 Y + 8x8 U + 8x8 V) from reference r. */
static void rvp8_inter_predict(rvp8_dec *s, int r, int mx, int my,
      const rvp8_mv *mv, uint8_t *ydst, int ys,
      uint8_t *udst, uint8_t *vdst, int uvs)
{
   const uint8_t *ry = s->ref_y[r], *ru = s->ref_u[r], *rv = s->ref_v[r];
   int rys = s->ref_ys[r], ruvs = s->ref_uvs[r];
   int yw = s->mbw * 16, yh = s->mbh * 16;
   int cw = s->mbw * 8,  ch = s->mbh * 8;
   if (!ry) return;
   /* Luma: MV is in eighth-pel (quarter-pel * 2). */
   rvp8_mc_block(ry, rys, ydst, ys, mx*16, my*16, mv->x, mv->y, 16, 16,
         yw, yh, s->use_bilinear);
   /* Chroma: the UV MV is the luma MV halved, rounded toward its sign
    * (libvpx build_inter16x16_predictors_mb), then applied on the
    * half-resolution plane with the usual >>3 / &7 split.  A version-3
    * (full-pixel) stream restricts the derived chroma MV to whole pels. */
   {
      int cmvx = mv->x, cmvy = mv->y;
      cmvy += 1 | (cmvy >> (int)(sizeof(int)*8 - 1));
      cmvx += 1 | (cmvx >> (int)(sizeof(int)*8 - 1));
      cmvy /= 2; cmvx /= 2;
      if (s->full_pixel) { cmvx &= ~7; cmvy &= ~7; }
      rvp8_mc_block(ru, ruvs, udst, uvs, mx*8, my*8, cmvx, cmvy, 8, 8,
            cw, ch, s->use_bilinear);
      rvp8_mc_block(rv, ruvs, vdst, uvs, mx*8, my*8, cmvx, cmvy, 8, 8,
            cw, ch, s->use_bilinear);
   }
}

/* SPLITMV prediction: per-4x4 luma from each sub-block MV, per-2x2 chroma
 * from the averaged sub-block MVs (libvpx build_4x4uvmvs +
 * build_inter4x4_predictors_mb). */
static void rvp8_inter_predict_split(rvp8_dec *s, int r, int mx, int my,
      const rvp8_mv bmv[16], uint8_t *ydst, int ys,
      uint8_t *udst, uint8_t *vdst, int uvs)
{
   const uint8_t *ry = s->ref_y[r], *ru = s->ref_u[r], *rv = s->ref_v[r];
   int rys = s->ref_ys[r], ruvs = s->ref_uvs[r];
   int yw = s->mbw * 16, yh = s->mbh * 16;
   int cw = s->mbw * 8,  ch = s->mbh * 8;
   int i, j, bx, by;
   if (!ry) return;
   /* Luma: 16 4x4 sub-blocks. Neighbouring sub-blocks very often share
    * a motion vector (SPLITMV's 2/4/8-partition patterns express 16x8,
    * 8x16 and 8x8 regions through per-4x4 syntax), so merge equal-MV
    * sub-blocks into maximal rectangles greedily and compensate each
    * rectangle with one call. This is bit-exact: every output pixel of
    * a motion-compensated block depends only on its own source window
    * and the (shared) motion vector, never on the block extents - and
    * merging vertically also removes real work, because the six-tap
    * first pass overlaps by five rows at every seam between vertically
    * adjacent blocks. */
   {
      unsigned done = 0;
      for (by = 0; by < 4; by++)
         for (bx = 0; bx < 4; bx++)
         {
            const rvp8_mv *m;
            int w, h, xx, yy;
            if (done & (1u << (by*4 + bx)))
               continue;
            m = &bmv[by*4 + bx];
            w = 1;
            while (bx + w < 4 && !(done & (1u << (by*4 + bx + w)))
                  && bmv[by*4 + bx + w].x == m->x
                  && bmv[by*4 + bx + w].y == m->y)
               w++;
            h = 1;
            for (yy = by + 1; yy < 4; yy++)
            {
               int ok = 1;
               for (xx = 0; xx < w; xx++)
                  if (   (done & (1u << (yy*4 + bx + xx)))
                      || bmv[yy*4 + bx + xx].x != m->x
                      || bmv[yy*4 + bx + xx].y != m->y)
                  {
                     ok = 0;
                     break;
                  }
               if (!ok)
                  break;
               h++;
            }
            for (yy = 0; yy < h; yy++)
               for (xx = 0; xx < w; xx++)
                  done |= 1u << ((by + yy)*4 + bx + xx);
            rvp8_mc_block(ry, rys, ydst + by*4*ys + bx*4, ys,
                  mx*16 + bx*4, my*16 + by*4, m->x, m->y, 4*w, 4*h,
                  yw, yh, s->use_bilinear);
         }
   }
   /* Chroma: 2x2 blocks of 4x4 px, each MV = average of the 4 luma
    * sub-block MVs, rounded (+4 sign-adjusted) then /8. Merge the tiny
    * 2x2 grid the same way. */
   {
      rvp8_mv cmv[4];
      unsigned done = 0;
      for (i = 0; i < 2; i++)
         for (j = 0; j < 2; j++)
         {
            int yo = i*8 + j*2;
            int tr = bmv[yo+0].y + bmv[yo+1].y + bmv[yo+4].y + bmv[yo+5].y;
            int tc = bmv[yo+0].x + bmv[yo+1].x + bmv[yo+4].x + bmv[yo+5].x;
            tr += 4 + ((tr >> (int)(sizeof(int)*8 - 1)) * 8);
            tc += 4 + ((tc >> (int)(sizeof(int)*8 - 1)) * 8);
            cmv[i*2 + j].y = (int16_t)(tr / 8);
            cmv[i*2 + j].x = (int16_t)(tc / 8);
            if (s->full_pixel)
            {
               cmv[i*2 + j].x &= ~7;
               cmv[i*2 + j].y &= ~7;
            }
         }
      for (i = 0; i < 2; i++)
         for (j = 0; j < 2; j++)
         {
            const rvp8_mv *m;
            int w, h;
            if (done & (1u << (i*2 + j)))
               continue;
            m = &cmv[i*2 + j];
            /* Extension probes must skip already-covered cells, or a
             * later anchor can re-cover a cell an earlier rectangle
             * claimed (e.g. [A,B,B,B]: (0,1) merges down over (1,1),
             * then (1,0) must not merge right over it). */
            w = (j == 0 && !(done & (1u << (i*2 + 1)))
                        && cmv[i*2 + 1].x == m->x
                        && cmv[i*2 + 1].y == m->y) ? 2 : 1;
            h = 1;
            if (i == 0)
            {
               int ok = 1, xx;
               for (xx = 0; xx < w; xx++)
                  if (   (done & (1u << (2 + j + xx)))
                      || cmv[2 + j + xx].x != m->x
                      || cmv[2 + j + xx].y != m->y)
                  {
                     ok = 0;
                     break;
                  }
               if (ok)
                  h = 2;
            }
            {
               int yy, xx;
               for (yy = 0; yy < h; yy++)
                  for (xx = 0; xx < w; xx++)
                     done |= 1u << ((i + yy)*2 + j + xx);
            }
            rvp8_mc_block(ru, ruvs, udst + i*4*uvs + j*4, uvs,
                  mx*8 + j*4, my*8 + i*4, m->x, m->y, 4*w, 4*h,
                  cw, ch, s->use_bilinear);
            rvp8_mc_block(rv, ruvs, vdst + i*4*uvs + j*4, uvs,
                  mx*8 + j*4, my*8 + i*4, m->x, m->y, 4*w, 4*h,
                  cw, ch, s->use_bilinear);
         }
   }
}

int rvp8_rows(rvp8_dec *s, int nrows)
{
   const int w = s->w, h = s->h, mbw = s->mbw, mbh = s->mbh;
   const int ys = s->ys, uvs = s->uvs;
   const int base_qp = s->base_qp;
   const int y1dc_dq = s->y1dc_dq, y2dc_dq = s->y2dc_dq, y2ac_dq = s->y2ac_dq;
   const int uvdc_dq = s->uvdc_dq, uvac_dq = s->uvac_dq;
   const int skip_enabled = s->skip_enabled, prob_skip = s->prob_skip;
   const int num_parts = s->num_parts;
   const int seg_enabled = s->seg_enabled, seg_abs = s->seg_abs;
   const int *seg_qp = s->seg_qp;
   const int *seg_prob = s->seg_prob;
   uint8_t *seg_map_buf = s->seg_map_buf;
   uint8_t *skip_lf_buf = s->skip_lf_buf;
   uint8_t *bpred_buf   = s->bpred_buf;
   uint8_t *yb = s->yb, *ub = s->ub, *vb = s->vb;
   uint8_t *above_nz_y = s->above_nz_y, *above_nz_u = s->above_nz_u;
   uint8_t *above_nz_v = s->above_nz_v, *above_nz_dc = s->above_nz_dc;
   uint8_t *above_bmodes = s->above_bmodes;
   int y1_dc_q, y1_ac_q, y2_dc_q, y2_ac_q, uv_dc_q, uv_ac_q;
   int mx, my, i, j;
   int my_end = s->my + nrows;
   uint8_t left_nz_y[4], left_nz_u[2], left_nz_v[2];
   uint8_t left_bmodes[4];
   int left_nz_dc;

   (void)w; (void)h; (void)base_qp;
   if (my_end > mbh)
      my_end = mbh;

   for (my = s->my; my < my_end; my++)
   {
      rvp8_bool *tp = &s->tbr[my % num_parts]; /* token partition for this row */
      /* Reset left context at start of each row */
      memset(left_nz_y, 0, sizeof(left_nz_y));
      memset(left_nz_u, 0, sizeof(left_nz_u));
      memset(left_nz_v, 0, sizeof(left_nz_v));
      memset(left_bmodes, 0, sizeof(left_bmodes));
      left_nz_dc = 0;
      for (mx = 0; mx < mbw; mx++)
      {
         int ym, uvm, is_skip = 0, seg_id = 0, mb_has_coeffs = 0, has_y2 = 0;
         uint8_t ay[16], ly[16], au[8], lu[8], av[8], lv[8];
         uint8_t tly=128, tlu=128, tlv=128;
         int16_t coeffs[16], y2_block[16], dc_vals[16];
         uint8_t bmodes[16]; /* B_PRED sub-block modes */
         int bx, by;
         int mb_qp;

         /* Read segment ID if segmentation is enabled */
         if (seg_enabled)
         {
            /* VP8 segment tree: prob[0] -> left(prob[1]->seg0/seg1) / right(prob[2]->seg2/seg3) */
            if (vp8b_get(&s->br, seg_prob[0]))
               seg_id = 2 + vp8b_get(&s->br, seg_prob[2]);
            else
               seg_id = vp8b_get(&s->br, seg_prob[1]);
         }

         if (seg_map_buf) seg_map_buf[my * mbw + mx] = (uint8_t)seg_id;
         /* Compute per-MB quantizer based on segment */
         if (seg_enabled && seg_abs)
            mb_qp = seg_qp[seg_id];
         else if (seg_enabled)
            mb_qp = base_qp + seg_qp[seg_id];
         else
            mb_qp = base_qp;
         if (mb_qp < 0) mb_qp = 0;
         if (mb_qp > 127) mb_qp = 127;
         /* Recompute quantizer tables for this MB's QP */
         { int q2 = mb_qp + y1dc_dq; y1_dc_q = vp8_dc_qlut[q2<0?0:q2>127?127:q2]; }
         y1_ac_q = vp8_ac_qlut[mb_qp];
         { int q2 = mb_qp + y2dc_dq; y2_dc_q = vp8_dc_qlut[q2<0?0:q2>127?127:q2] * 2; }
         { int q2 = mb_qp + y2ac_dq; y2_ac_q = vp8_ac_qlut[q2<0?0:q2>127?127:q2] * 155 / 100;
           if (y2_ac_q < 8) y2_ac_q = 8; }
         { int q2 = mb_qp + uvdc_dq; uv_dc_q = vp8_dc_qlut[q2<0?0:q2>127?127:q2]; if(uv_dc_q>132)uv_dc_q=132; }
         { int q2 = mb_qp + uvac_dq; uv_ac_q = vp8_ac_qlut[q2<0?0:q2>127?127:q2]; }

         /* Skip flag (after segment, before y_mode — libvpx order) */
         if (skip_enabled)
            is_skip = vp8b_get(&s->br, prob_skip);

         /* Mode decode. For an inter frame the mode/MV come from the first
          * partition via the inter decoder; the result may be an inter MB
          * (motion-compensated) or an intra MB coded inside an inter frame.
          * For a key frame, the original intra Y-mode read applies. */
         if (s->is_inter)
         {
            rvp8_mbinfo *mi   = (rvp8_mbinfo*)s->mb_info + my * mbw + mx;
            rvp8_mbinfo *ab   = (my > 0) ? mi - mbw : NULL;
            rvp8_mbinfo *lf   = (mx > 0) ? mi - 1   : NULL;
            rvp8_mbinfo *al   = (my > 0 && mx > 0) ? mi - mbw - 1 : NULL;
            rvp8_decode_mb_mode(s, mi, ab, lf, al, mx, my, mbw, mbh);
            if (mi->ref_frame == 0)
               ym = mi->mode & 0x7F;   /* intra-in-inter: low bits = Y mode */
            else
               ym = -1;                /* sentinel: inter MB                 */
         }
         else
         {
            /* Y mode */
            if (!vp8b_get(&s->br, vp8_ymp[0])) {
               ym = 4; /* B_PRED */
            } else if (!vp8b_get(&s->br, vp8_ymp[1])) {
               /* Left subtree: DC, V */
               ym = vp8b_get(&s->br, vp8_ymp[2]) ? 1 : 0;
            } else {
               /* Right subtree: H, TM */
               ym = vp8b_get(&s->br, vp8_ymp[3]) ? 3 : 2;
            }
         }

         /* Y2 (second-order DC) block present for every mode EXCEPT B_PRED
          * and SPLITMV (libvpx: mode != B_PRED && mode != SPLITMV). */
         {
            int is_split_mb = 0;
            if (s->is_inter)
            {
               rvp8_mbinfo *mm = (rvp8_mbinfo*)s->mb_info + my * mbw + mx;
               is_split_mb = (mm->ref_frame != 0 && mm->is_split);
            }
            has_y2 = (ym != 4) && !is_split_mb;
         }

         if (ym == 4)
         {
            if (s->is_inter)
            {
               /* Intra-in-inter B_PRED: bmodes were already read (with the
                * fixed inter probability table) during mode decode. Reuse
                * the stored values; no context tracking needed since inter
                * B_PRED uses non-contextual probabilities. */
               rvp8_mbinfo *mm = (rvp8_mbinfo*)s->mb_info + my * mbw + mx;
               for (i = 0; i < 16; i++)
                  bmodes[i] = mm->bmodes[i];
            }
            else
            {
            /* B_PRED: read 16 sub-block modes using key-frame context probs. */
            for (i = 0; i < 16; i++)
            {
               int sb_row = i / 4, sb_col = i % 4;
               int above_mode, left_mode;
               /* Above mode: from previous MB row's bottom sub-block, or default 0 (DC) */
               if (sb_row > 0)
                  above_mode = bmodes[i - 4];
               else if (my > 0)
                  above_mode = above_bmodes[mx * 4 + sb_col];
               else
                  above_mode = 0;
               /* Left mode: from left sub-block in this MB, or previous MB's right col */
               if (sb_col > 0)
                  left_mode = bmodes[i - 1];
               else if (mx > 0)
                  left_mode = left_bmodes[sb_row];
               else
                  left_mode = 0;
               bmodes[i] = (uint8_t)vp8_read_bmode(&s->br, above_mode, left_mode);
            }
            /* Store bottom row for next MB row's above context */
            for (i = 0; i < 4; i++)
               above_bmodes[mx * 4 + i] = bmodes[12 + i];
            /* Store right column for next MB's left context */
            for (i = 0; i < 4; i++)
               left_bmodes[i] = bmodes[i * 4 + 3];
            }
         }
         else
         {
            /* Non B_PRED: clear bmode context (default DC=0 for neighbors) */
            /* Map 16x16 mode to equivalent bmode for context (libvpx
             * above_block_mode/left_block_mode): DC->B_DC(0), V->B_VE(2),
             * H->B_HE(3), TM->B_TM(1). */
            {
               uint8_t eq = (ym == 1) ? 2 : (ym == 2) ? 3 : (ym == 3) ? 1 : 0;
               for (i = 0; i < 4; i++) above_bmodes[mx * 4 + i] = eq;
               for (i = 0; i < 4; i++) left_bmodes[i] = eq;
            }
         }

         /* UV mode — only for intra MBs (key frame, or intra-in-inter).
          * An inter MB's chroma prediction is motion-compensated. For an
          * intra-in-inter MB the uv mode was already read during mode
          * decode, so reuse the stored value rather than re-reading. */
         if (ym >= 0)
         {
            if (s->is_inter)
            {
               rvp8_mbinfo *mm = (rvp8_mbinfo*)s->mb_info + my * mbw + mx;
               uvm = mm->uvmode;
            }
            else
            {
               if      (!vp8b_get(&s->br, vp8_uvmp[0])) uvm = 0;
               else if (!vp8b_get(&s->br, vp8_uvmp[1])) uvm = 1;
               else if (!vp8b_get(&s->br, vp8_uvmp[2])) uvm = 2;
               else uvm = 3;
            }
         }
         else
            uvm = 0;

         if (ym < 0)
         {
            /* Inter MB: motion-compensated prediction from the reference
             * frame. For ZEROMV/skip this is a straight copy; non-zero MVs
             * use sub-pixel interpolation (rvp8_inter_predict). The bmode
             * context for a following intra MB is DC (0). */
            rvp8_mbinfo *mi = (rvp8_mbinfo*)s->mb_info + my * mbw + mx;
            int r = mi->ref_frame - 1;   /* 0=last,1=golden,2=altref        */
            for (i = 0; i < 4; i++) above_bmodes[mx * 4 + i] = 0;
            for (i = 0; i < 4; i++) left_bmodes[i] = 0;
            if (mi->is_split)
               rvp8_inter_predict_split(s, r, mx, my, mi->bmv,
                     yb + my*16*ys + mx*16, ys,
                     ub + my*8*uvs + mx*8, vb + my*8*uvs + mx*8, uvs);
            else
               rvp8_inter_predict(s, r, mx, my, &mi->mv,
                     yb + my*16*ys + mx*16, ys,
                     ub + my*8*uvs + mx*8, vb + my*8*uvs + mx*8, uvs);
            goto residual;  /* skip intra context + prediction below */
         }

         /* Gather prediction context. Border semantics per libvpx
          * vp8_setup_intra_recon: row above frame = 127 (including the
          * top-left corner), column left of frame = 129. */
         if (my > 0) {
            memcpy(ay, yb+(my*16-1)*ys+mx*16, 16);
            memcpy(au, ub+(my*8-1)*uvs+mx*8, 8);
            memcpy(av, vb+(my*8-1)*uvs+mx*8, 8);
            if (mx > 0) { tly=yb[(my*16-1)*ys+mx*16-1]; tlu=ub[(my*8-1)*uvs+mx*8-1]; tlv=vb[(my*8-1)*uvs+mx*8-1]; }
            else        { tly=129; tlu=129; tlv=129; }
         } else {
            memset(ay,127,16); memset(au,127,8); memset(av,127,8);
            tly=127; tlu=127; tlv=127;
         }
         if (mx > 0) {
            for(j=0;j<16;j++) ly[j]=yb[(my*16+j)*ys+mx*16-1];
            for(j=0;j<8;j++) lu[j]=ub[(my*8+j)*uvs+mx*8-1];
            for(j=0;j<8;j++) lv[j]=vb[(my*8+j)*uvs+mx*8-1];
         } else { memset(ly,129,16); memset(lu,129,8); memset(lv,129,8); }

         /* Predict */
         if (ym != 4)
            vp8_pred16(yb+my*16*ys+mx*16, ys, ym, ay, ly, tly, my > 0, mx > 0);
         /* B_PRED Y prediction is done per sub-block below */
         vp8_pred8(ub+my*8*uvs+mx*8, uvs, uvm, au, lu, tlu, my > 0, mx > 0);
         vp8_pred8(vb+my*8*uvs+mx*8, uvs, uvm, av, lv, tlv, my > 0, mx > 0);

         /* Decode and add residual */
        residual:
         mb_has_coeffs = 0;
         if (!is_skip || ym == 4)
         {
            /* Non-zero coefficient tracking for context. Skipped B_PRED
             * MBs still take this path: sub-block prediction must run
             * even when all residuals are skipped (libvpx zeroes eobs
             * but still predicts). */
            int nz_y2 = 0;

            /* Y2 block (DC for 16x16 prediction) */
            memset(dc_vals, 0, sizeof(dc_vals));
            if (has_y2) /* has second-order block */
            {
               int y2_above = (my > 0) ? above_nz_dc[mx] : 0;
               int y2_left  = (mx > 0) ? left_nz_dc : 0;
               int y2_ctx   = (y2_above + y2_left > 1) ? 2 : (y2_above + y2_left);
               memset(y2_block, 0, sizeof(y2_block));
               nz_y2 = vp8_decode_block(tp, y2_block, s->cprob[1], 0, y2_ctx);
               above_nz_dc[mx] = (nz_y2 > 0) ? 1 : 0;
               left_nz_dc = (nz_y2 > 0) ? 1 : 0;
               if (nz_y2 > 0) mb_has_coeffs = 1;
               /* Dequantize Y2 */
               y2_block[0] = (int16_t)(y2_block[0] * y2_dc_q);
               for (i = 1; i < 16; i++)
                  y2_block[i] = (int16_t)(y2_block[i] * y2_ac_q);
               /* Inverse WHT to get DC values for each sub-block */
               vp8_iwht4x4(y2_block, dc_vals);
            }

            /* 16 Y sub-blocks */
            for (by = 0; by < 4; by++)
            {
               for (bx = 0; bx < 4; bx++)
               {
                  int sb_above = (my > 0 || by > 0) ? above_nz_y[mx*4+bx] : 0;
                  int sb_left  = (mx > 0 || bx > 0) ? left_nz_y[by] : 0;
                  int sb_ctx   = (sb_above + sb_left > 1) ? 2 : (sb_above + sb_left);
                  int start, nz_cnt;
                  uint8_t *sb_dst = yb + (my*16 + by*4) * ys + mx*16 + bx*4;

                  if (ym == 4) /* B_PRED: per-sub-block prediction */
                  {
                     uint8_t sa[8], sl[4]; uint8_t stl;
                     int sb_idx = by * 4 + bx;
                     /* Gather 4x4 context from already-reconstructed neighbors */
                     if (by > 0) { for(i=0;i<4;i++) sa[i]=sb_dst[-ys+i]; }
                     else if (my > 0) { for(i=0;i<4;i++) sa[i]=yb[(my*16-1)*ys+mx*16+bx*4+i]; }
                     else { memset(sa,127,4); }
                     /* Above-right: next 4 pixels. For bx==3 libvpx
                      * (intra_prediction_down_copy) replicates the above
                      * MB row's pixels at cols +16..19 down the right
                      * edge, so ALL bx==3 sub-blocks see the above-right
                      * MB's bottom-left 4 pixels (127 border if my==0 or
                      * at the last MB column). */
                     if (bx < 3 && by > 0) { for(i=0;i<4;i++) sa[4+i]=sb_dst[-ys+4+i]; }
                     else if (bx < 3 && my > 0) { for(i=0;i<4;i++) sa[4+i]=yb[(my*16-1)*ys+mx*16+bx*4+4+i]; }
                     else if (bx < 3) { for(i=0;i<4;i++) sa[4+i]=127; }
                     else if (my > 0 && mx < mbw-1) { for(i=0;i<4;i++) sa[4+i]=yb[(my*16-1)*ys+mx*16+16+i]; }
                     else if (my > 0)
                     {
                        /* Last MB column: libvpx's border extension
                         * (vp8_extend_mb_row) replicates the last real
                         * pixel of the row above across the right border,
                         * so above-right = 4 copies of that pixel. */
                        uint8_t rep = yb[(my*16-1)*ys + mbw*16 - 1];
                        for(i=0;i<4;i++) sa[4+i]=rep;
                     }
                     else { for(i=0;i<4;i++) sa[4+i]=127; }
                     if (bx > 0) { for(i=0;i<4;i++) sl[i]=sb_dst[i*ys-1]; }
                     else if (mx > 0) { for(i=0;i<4;i++) sl[i]=yb[(my*16+by*4+i)*ys+mx*16-1]; }
                     else { memset(sl,129,4); }
                     if (by > 0) {
                        if (bx > 0 || mx > 0) stl = sb_dst[-ys-1];
                        else stl = 129; /* mx==0: left frame border */
                     } else if (my > 0) {
                        if (bx > 0 || mx > 0) stl = yb[(my*16-1)*ys+mx*16+bx*4-1];
                        else stl = 129; /* mx==0: left frame border */
                     } else
                        stl = 127; /* my==0: above frame border (covers corner) */
                     vp8_pred4x4(sb_dst, ys, bmodes[sb_idx], sa, sl, stl);
                     start = 0; /* B_PRED: decode DC from tokens (type 1) */
                  }
                  else if (!has_y2)
                  {
                     start = 0; /* SPLITMV: no Y2, DC comes from tokens */
                  }
                  else
                  {
                     start = 1; /* non-B_PRED: DC comes from Y2 */
                  }

                  if (is_skip)
                  {
                     memset(coeffs, 0, sizeof(coeffs[0]) * 16);
                     nz_cnt = 0;
                  }
                  else
                     nz_cnt = vp8_decode_block(tp, coeffs,
                           s->cprob[has_y2 ? 0 : 3], start, sb_ctx);
                  /* Dequantize */
                  if (has_y2)
                     coeffs[0] = dc_vals[by * 4 + bx]; /* DC from WHT */
                  else
                     coeffs[0] = (int16_t)(coeffs[0] * y1_dc_q);
                  for (i = 1; i < 16; i++)
                     coeffs[i] = (int16_t)(coeffs[i] * y1_ac_q);
                  /* Inverse DCT + add to prediction */
                  vp8_idct4x4_add(coeffs, sb_dst, ys);
                  /* Update context tracking */
                  above_nz_y[mx*4+bx] = (nz_cnt > 0) ? 1 : 0;
                  left_nz_y[by] = (nz_cnt > 0) ? 1 : 0;
                  if (nz_cnt > 0) mb_has_coeffs = 1;
               }
            }

            /* 4 U sub-blocks */
            for (by = 0; by < 2; by++)
            {
               for (bx = 0; bx < 2; bx++)
               {
                  int sb_above = (my > 0 || by > 0) ? above_nz_u[mx*2+bx] : 0;
                  int sb_left  = (mx > 0 || bx > 0) ? left_nz_u[by] : 0;
                  int sb_ctx   = (sb_above + sb_left > 1) ? 2 : (sb_above + sb_left);
                  int nz_cnt;
                  if (is_skip)
                  {
                     memset(coeffs, 0, sizeof(coeffs[0]) * 16);
                     nz_cnt = 0;
                  }
                  else
                     nz_cnt = vp8_decode_block(tp, coeffs, s->cprob[2], 0, sb_ctx);
                  coeffs[0] = (int16_t)(coeffs[0] * uv_dc_q);
                  for (i = 1; i < 16; i++)
                     coeffs[i] = (int16_t)(coeffs[i] * uv_ac_q);
                  vp8_idct4x4_add(coeffs,
                        ub + (my*8 + by*4) * uvs + mx*8 + bx*4, uvs);
                  above_nz_u[mx*2+bx] = (nz_cnt > 0) ? 1 : 0;
                  left_nz_u[by] = (nz_cnt > 0) ? 1 : 0;
                  if (nz_cnt > 0) mb_has_coeffs = 1;
               }
            }

            /* 4 V sub-blocks */
            for (by = 0; by < 2; by++)
            {
               for (bx = 0; bx < 2; bx++)
               {
                  int sb_above = (my > 0 || by > 0) ? above_nz_v[mx*2+bx] : 0;
                  int sb_left  = (mx > 0 || bx > 0) ? left_nz_v[by] : 0;
                  int sb_ctx   = (sb_above + sb_left > 1) ? 2 : (sb_above + sb_left);
                  int nz_cnt;
                  if (is_skip)
                  {
                     memset(coeffs, 0, sizeof(coeffs[0]) * 16);
                     nz_cnt = 0;
                  }
                  else
                     nz_cnt = vp8_decode_block(tp, coeffs, s->cprob[2], 0, sb_ctx);
                  coeffs[0] = (int16_t)(coeffs[0] * uv_dc_q);
                  for (i = 1; i < 16; i++)
                     coeffs[i] = (int16_t)(coeffs[i] * uv_ac_q);
                  vp8_idct4x4_add(coeffs,
                        vb + (my*8 + by*4) * uvs + mx*8 + bx*4, uvs);
                  above_nz_v[mx*2+bx] = (nz_cnt > 0) ? 1 : 0;
                  left_nz_v[by] = (nz_cnt > 0) ? 1 : 0;
                  if (nz_cnt > 0) mb_has_coeffs = 1;
               }
            }
         }
         else
         {
            /* Skipped MB: clear non-zero context (libvpx
             * vp8_reset_mb_tokens_context). The Y2 context is only
             * reset when this MB actually has a Y2 block (non-B_PRED). */
            for (bx = 0; bx < 4; bx++) above_nz_y[mx*4+bx] = 0;
            for (bx = 0; bx < 2; bx++) { above_nz_u[mx*2+bx] = 0; above_nz_v[mx*2+bx] = 0; }
            memset(left_nz_y, 0, sizeof(left_nz_y));
            memset(left_nz_u, 0, sizeof(left_nz_u));
            memset(left_nz_v, 0, sizeof(left_nz_v));
            if (has_y2) { above_nz_dc[mx] = 0; left_nz_dc = 0; }
         }
         /* libvpx: filter inner edges unless MB has no coefficients
          * (parsed skip OR eobtotal==0) and is not B_PRED. */
         if (skip_lf_buf)
            skip_lf_buf[my * mbw + mx] =
               (uint8_t)(((is_skip || !mb_has_coeffs) && has_y2) ? 1 : 0);
         if (bpred_buf)
            bpred_buf[my * mbw + mx] = (uint8_t)(ym == 4 ? 1 : 0);

      }
   }


   s->my = my_end;
   return (my_end < mbh) ? 1 : 0;
}

/* Post-decode stages, resumable by row range:
 * rvp8_filter_rows applies the loop filter over MB rows [my0, my1);
 * rvp8_upsample_rows converts luma row pairs into the output buffer.
 * rvp8_output allocates the destination (call once, before either). */
uint32_t *rvp8_output(rvp8_dec *s)
{
   return (uint32_t*)malloc((size_t)s->w * s->h * sizeof(uint32_t));
}

void rvp8_filter_rows(rvp8_dec *s, int my0, int my1)
{
   if (s->lf_level <= 0)
      return;
   if (my1 > s->mbh) my1 = s->mbh;
   if (my0 >= my1)   return;
   if (s->filter_type == 1)
      vp8_loop_filter_simple(s->yb, s->ys, s->mbw, my0, my1,
            s->lf_level, s->sharpness, s->seg_enabled, s->seg_abs,
            s->seg_lf, s->lf_delta_enabled, s->ref_lf_delta,
            s->mode_lf_delta, s->seg_map_buf, s->skip_lf_buf,
            s->bpred_buf, s->is_inter ? s->mb_info : NULL);
   else
      vp8_loop_filter_normal(s->yb, s->ys, s->ub, s->vb, s->uvs,
            s->mbw, my0, my1, s->lf_level, s->sharpness,
            s->seg_enabled, s->seg_abs, s->seg_lf,
            s->lf_delta_enabled, s->ref_lf_delta, s->mode_lf_delta,
            s->seg_map_buf, s->skip_lf_buf, s->bpred_buf,
            s->is_inter ? s->mb_info : NULL);
}

/* Convert a bounded batch of luma rows starting at pair cursor j0 into
 * pix, running at most max_rows luma rows before yielding. Mirrors the
 * original whole-image loop exactly: row 0 (on the first call) and the
 * final even row mirror the chroma border; interior pairs (j+1, j+2)
 * interpolate chroma rows (j/2, j/2+1). The loop itself defines the
 * resumption point: the returned value is the next pair cursor, or h
 * when the image is complete (the caller passes it back unchanged). */
int rvp8_upsample_rows(rvp8_dec *s, uint32_t *pix, int j0, int max_rows)
{
   const int w = s->w, h = s->h, ys = s->ys, uvs = s->uvs;
   uint8_t *yb = s->yb, *ub = s->ub, *vb = s->vb;
   int j     = j0;
   int limit = j0 + max_rows;
   if (j0 == 0)
      vp8_fancy_pair(yb, NULL, ub, vb, ub, vb, pix, NULL, w,
            s->fancy_uv, s->swap_rb);
   for (; j + 2 < h && j < limit; j += 2)
   {
      const uint8_t *tu = ub + (j >> 1) * uvs, *tv = vb + (j >> 1) * uvs;
      vp8_fancy_pair(yb + (j+1)*ys, yb + (j+2)*ys,
            tu, tv, tu + uvs, tv + uvs,
            pix + (size_t)(j+1)*w, pix + (size_t)(j+2)*w, w,
            s->fancy_uv, s->swap_rb);
   }
   if (j + 2 >= h)
   {
      if (!(h & 1) && h >= 2)
      {
         const uint8_t *lu = ub + ((h-1) >> 1) * uvs;
         const uint8_t *lv = vb + ((h-1) >> 1) * uvs;
         vp8_fancy_pair(yb + (size_t)(h-1)*ys, NULL, lu, lv, lu, lv,
               pix + (size_t)(h-1)*w, NULL, w, s->fancy_uv,
               s->swap_rb);
      }
      return h;
   }
   return j;
}

/* One-shot wrapper preserving the original entry point: used by the
 * still-image fallback paths and the animation decoder, and doubling as
 * the reference composition of the resumable stages. */
uint32_t *rvp8_decode(const uint8_t *data, size_t len,
      unsigned *ow, unsigned *oh, int swap_rb)
{
   rvp8_dec s;
   uint32_t *pix;
   /* rvp8_begin frees the per-frame scratch pointers of a previous decode
    * on this state (the persistent decoder reuses one rvp8_dec across
    * frames); a fresh stack instance must be zeroed or those frees hit
    * uninitialized garbage. */
   memset(&s, 0, sizeof(s));
   if (rvp8_begin(data, len, &s) != 0)
      return NULL;
   while (rvp8_rows(&s, s.mbh) > 0)
      ;
   pix = rvp8_output(&s);
   if (!pix)
   {
      rvp8_abort(&s);
      return NULL;
   }
   rvp8_filter_rows(&s, 0, s.mbh);
   s.swap_rb = swap_rb;
   rvp8_upsample_rows(&s, pix, 0, s.h);
   *ow = (unsigned)s.w;
   *oh = (unsigned)s.h;
   rvp8_abort(&s);
   return pix;
}

/* ==================================================================== */
/* Persistent video decoder: drives per-frame decode while retaining    */
/* the last/golden/altref reference frames across frames.               */
/* ==================================================================== */

struct rvp8_video
{
   rvp8_dec s;
   int      w, h, mbw, mbh, ys, uvs;
   int      have_refs;
   /* three reference frames (last, golden, altref), each a full padded
    * plane set matching the decoder's yb/ub/vb geometry. */
   uint8_t *ry[3], *ru[3], *rv[3];
   size_t   ysz, uvsz;
};

static void rvp8_video_free_refs(rvp8_video *v)
{
   int i;
   for (i = 0; i < 3; i++)
   {
      free(v->ry[i]); free(v->ru[i]); free(v->rv[i]);
      v->ry[i] = v->ru[i] = v->rv[i] = NULL;
   }
}

rvp8_video *rvp8_video_open(void)
{
   rvp8_video *v = (rvp8_video*)calloc(1, sizeof(*v));
   return v;
}

void rvp8_video_close(rvp8_video *v)
{
   if (!v) return;
   rvp8_video_free_refs(v);
   /* Free the decoder's per-frame scratch buffers from the last decode. */
   free(v->s.seg_map_buf); free(v->s.skip_lf_buf); free(v->s.bpred_buf);
   free(v->s.yb); free(v->s.ub); free(v->s.vb);
   free(v->s.above_nz_y); free(v->s.above_nz_u); free(v->s.above_nz_v);
   free(v->s.above_nz_dc); free(v->s.above_bmodes);
   free(v->s.fancy_uv);
   free(v->s.mb_info);
   free(v);
}

/* Allocate the reference planes once the dimensions are known. */
static int rvp8_video_alloc_refs(rvp8_video *v)
{
   int i;
   v->ysz  = (size_t)v->ys  * v->mbh * 16;
   v->uvsz = (size_t)v->uvs * v->mbh * 8;
   for (i = 0; i < 3; i++)
   {
      v->ry[i] = (uint8_t*)malloc(v->ysz);
      v->ru[i] = (uint8_t*)malloc(v->uvsz);
      v->rv[i] = (uint8_t*)malloc(v->uvsz);
      if (!v->ry[i] || !v->ru[i] || !v->rv[i])
         return -1;
   }
   return 0;
}

/* Decode one frame (key or inter). On success the decoded YUV is in
 * v->s.yb/ub/vb and also promoted into the reference frames per the
 * frame's refresh flags. Returns 0 on success. */
int rvp8_video_decode(rvp8_video *v, const uint8_t *data, size_t len)
{
   int kf;
   if (len < 3) return -1;
   kf = !((uint32_t)data[0] & 1);

   if (kf)
   {
      /* Key frame: plain decode, then all references := this frame. The
       * decoder state must reset fully (a key frame carries no inter
       * context), so clear the inter flag before begin. Free the mbinfo
       * array too — begin's reset would otherwise drop the pointer. */
      v->s.is_inter = 0;
      free(v->s.mb_info);
      v->s.mb_info = NULL;
      if (rvp8_begin(data, len, &v->s) != 0)
         return -1;
      while (rvp8_rows(&v->s, v->s.mbh) > 0)
         ;
      rvp8_filter_rows(&v->s, 0, v->s.mbh);

      v->w = v->s.w; v->h = v->s.h; v->mbw = v->s.mbw; v->mbh = v->s.mbh;
      v->ys = v->s.ys; v->uvs = v->s.uvs;
      if (!v->have_refs)
      {
         if (rvp8_video_alloc_refs(v) != 0) return -1;
         v->have_refs = 1;
      }
      {
         int i;
         for (i = 0; i < 3; i++)
         {
            memcpy(v->ry[i], v->s.yb, v->ysz);
            memcpy(v->ru[i], v->s.ub, v->uvsz);
            memcpy(v->rv[i], v->s.vb, v->uvsz);
         }
      }
      return 0;
   }

   /* Inter frame: set up references + inherited context, decode, then
    * update references per refresh/copy flags. */
   if (!v->have_refs)
      return -1;   /* inter before any key frame */

   /* Prime the decoder struct with what begin() must preserve. */
   v->s.is_inter = 1;
   v->s.w = v->w; v->s.h = v->h;
   v->s.ref_y[0] = v->ry[0]; v->s.ref_u[0] = v->ru[0]; v->s.ref_v[0] = v->rv[0];
   v->s.ref_y[1] = v->ry[1]; v->s.ref_u[1] = v->ru[1]; v->s.ref_v[1] = v->rv[1];
   v->s.ref_y[2] = v->ry[2]; v->s.ref_u[2] = v->ru[2]; v->s.ref_v[2] = v->rv[2];
   v->s.ref_ys[0] = v->s.ref_ys[1] = v->s.ref_ys[2] = v->ys;
   v->s.ref_uvs[0] = v->s.ref_uvs[1] = v->s.ref_uvs[2] = v->uvs;
   /* cprob/mvc/ymode/uvmode already hold the inherited context from the
    * previous frame (begin preserves them for inter). Initialise the MV
    * and intra-mode probs on the very first inter frame from defaults. */
   if (!v->s.mvc[0][0])
   {
      memcpy(v->s.mvc[0], vp8_default_mv_context[0], MVPCOUNT);
      memcpy(v->s.mvc[1], vp8_default_mv_context[1], MVPCOUNT);
   }
   if (!v->s.ymode_prob[0])
   {
      static const uint8_t dy[4] = {112, 86, 140, 37};
      static const uint8_t du[3] = {162, 101, 204};
      memcpy(v->s.ymode_prob, dy, 4);
      memcpy(v->s.uvmode_prob, du, 3);
   }

   if (rvp8_begin(data, len, &v->s) != 0)
      return -1;
   while (rvp8_rows(&v->s, v->s.mbh) > 0)
      ;
   rvp8_filter_rows(&v->s, 0, v->s.mbh);

   /* Update references. copy_golden/altref: 0=no change,1=from last,2=from
    * the other; refresh_*: replace with the just-decoded frame. Order per
    * libvpx: copies use the OLD last frame, then refreshes apply. */
   {
      /* Reference updates in libvpx order (onyxd_if.c): copy_altref first
       * (so a following copy_golden==2 sees the updated altref), then
       * copy_golden, then the refreshes from the newly decoded frame.
       * Copies read the pre-refresh buffers. */
      if (!v->s.refresh_altref && v->s.copy_altref == 1)
      { memcpy(v->ry[2], v->ry[0], v->ysz); memcpy(v->ru[2], v->ru[0], v->uvsz); memcpy(v->rv[2], v->rv[0], v->uvsz); }
      else if (!v->s.refresh_altref && v->s.copy_altref == 2)
      { memcpy(v->ry[2], v->ry[1], v->ysz); memcpy(v->ru[2], v->ru[1], v->uvsz); memcpy(v->rv[2], v->rv[1], v->uvsz); }
      if (!v->s.refresh_golden && v->s.copy_golden == 1)
      { memcpy(v->ry[1], v->ry[0], v->ysz); memcpy(v->ru[1], v->ru[0], v->uvsz); memcpy(v->rv[1], v->rv[0], v->uvsz); }
      else if (!v->s.refresh_golden && v->s.copy_golden == 2)
      { memcpy(v->ry[1], v->ry[2], v->ysz); memcpy(v->ru[1], v->ru[2], v->uvsz); memcpy(v->rv[1], v->rv[2], v->uvsz); }
      if (v->s.refresh_golden)
      { memcpy(v->ry[1], v->s.yb, v->ysz); memcpy(v->ru[1], v->s.ub, v->uvsz); memcpy(v->rv[1], v->s.vb, v->uvsz); }
      if (v->s.refresh_altref)
      { memcpy(v->ry[2], v->s.yb, v->ysz); memcpy(v->ru[2], v->s.ub, v->uvsz); memcpy(v->rv[2], v->s.vb, v->uvsz); }
      if (v->s.refresh_last)
      { memcpy(v->ry[0], v->s.yb, v->ysz); memcpy(v->ru[0], v->s.ub, v->uvsz); memcpy(v->rv[0], v->s.vb, v->uvsz); }
   }
   return 0;
}

/* Access the just-decoded YUV planes. */
const uint8_t *rvp8_video_plane(const rvp8_video *v, int plane,
      int *stride, int *width, int *height)
{
   int st, w, h;
   const uint8_t *p;
   if (plane == 0)
   {
      st = v->s.ys;  w = v->w;           h = v->h;           p = v->s.yb;
   }
   else
   {
      st = v->s.uvs; w = (v->w + 1) / 2; h = (v->h + 1) / 2;
      p  = (plane == 1) ? v->s.ub : v->s.vb;
   }
   if (stride) *stride = st;
   if (width)  *width  = w;
   if (height) *height = h;
   return p;
}

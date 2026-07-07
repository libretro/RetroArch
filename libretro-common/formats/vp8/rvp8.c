/* rvp8 -- self-contained VP8 key-frame (intra) decoder for libretro-common.
 *
 * Extracted verbatim from the WebP decoder (formats/webp/rwebp.c): this is
 * the lossy VP8 image path -- coefficient tokens, dequantisation, the DCT
 * and Walsh-Hadamard inverse transforms, 4x4 and 16x16 intra prediction,
 * both loop filters, fancy chroma upsampling and YUV->RGB.  A WebP 'VP8 '
 * chunk is always a single key frame, so no inter-frame machinery (motion
 * vectors, reference frames) is present.
 *
 * The public entry points (rvp8_decode and the resumable rvp8_* row API)
 * are declared in <formats/rvp8.h>; everything else here is private.
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


static void vp8b_fill(rvp8_bool *b)
{
   int shift = 48 - b->count;
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
   shift = 0;
   while (b->range < 128) { b->range <<= 1; shift++; }
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

/* ---- Row YUV -> ARGB conversion ----
 * Converts a full row of co-sited (already chroma-interpolated) YUV
 * samples to 0xFFRRGGBB words. The scalar body matches vp8_yuv2rgb
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
      const uint8_t *v, uint32_t *dst, int len)
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
         /* pack to words 0xFFrrggbb (memory order b,g,r,FF) */
         __m128i r8 = _mm_packus_epi16(R, R);
         __m128i g8 = _mm_packus_epi16(G, G);
         __m128i b8 = _mm_packus_epi16(B, B);
         __m128i a8 = _mm_packus_epi16(alpha, alpha);
         __m128i bg = _mm_unpacklo_epi8(b8, g8);
         __m128i ra = _mm_unpacklo_epi8(r8, a8);
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
         px.val[0] = vqmovn_u16(vshrq_n_u16(B2, 6));   /* b */
         px.val[1] = vqshrun_n_s16(G4, 6);             /* g */
         px.val[2] = vqshrun_n_s16(R2, 6);             /* r */
         px.val[3] = vdup_n_u8(255);                   /* a */
         vst4_u8((uint8_t*)(dst + i), px);
      }
   }
#endif

   for (; i < len; i++)
   {
      uint8_t r, g, b2;
      vp8_yuv2rgb(y[i], u[i], v[i], &r, &g, &b2);
      dst[i] = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b2;
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
static void vp8_fancy_uv_top(const uint8_t *top_u, const uint8_t *top_v,
      const uint8_t *cur_u, const uint8_t *cur_v,
      uint8_t *du, uint8_t *dv, int len)
{
   int x, last_pair = (len - 1) >> 1;
   int tl_u = top_u[0], tl_v = top_v[0];
   int l_u = cur_u[0], l_v = cur_v[0];
   du[0] = (uint8_t)((3*tl_u + l_u + 2) >> 2);
   dv[0] = (uint8_t)((3*tl_v + l_v + 2) >> 2);
   for (x = 1; x <= last_pair; x++)
   {
      int t_u = top_u[x], t_v = top_v[x];
      int c_u = cur_u[x], c_v = cur_v[x];
      int avg_u = tl_u + t_u + l_u + c_u + 8;
      int avg_v = tl_v + t_v + l_v + c_v + 8;
      int d12_u = (avg_u + 2*(t_u + l_u)) >> 3, d12_v = (avg_v + 2*(t_v + l_v)) >> 3;
      int d03_u = (avg_u + 2*(tl_u + c_u)) >> 3, d03_v = (avg_v + 2*(tl_v + c_v)) >> 3;
      du[2*x-1] = (uint8_t)((d12_u + tl_u) >> 1);
      dv[2*x-1] = (uint8_t)((d12_v + tl_v) >> 1);
      du[2*x]   = (uint8_t)((d03_u + t_u) >> 1);
      dv[2*x]   = (uint8_t)((d03_v + t_v) >> 1);
      tl_u = t_u; tl_v = t_v; l_u = c_u; l_v = c_v;
   }
   if (!(len & 1))
   {
      du[len-1] = (uint8_t)((3*tl_u + l_u + 2) >> 2);
      dv[len-1] = (uint8_t)((3*tl_v + l_v + 2) >> 2);
   }
}

static void vp8_fancy_uv_bot(const uint8_t *top_u, const uint8_t *top_v,
      const uint8_t *cur_u, const uint8_t *cur_v,
      uint8_t *du, uint8_t *dv, int len)
{
   int x, last_pair = (len - 1) >> 1;
   int tl_u = top_u[0], tl_v = top_v[0];
   int l_u = cur_u[0], l_v = cur_v[0];
   du[0] = (uint8_t)((3*l_u + tl_u + 2) >> 2);
   dv[0] = (uint8_t)((3*l_v + tl_v + 2) >> 2);
   for (x = 1; x <= last_pair; x++)
   {
      int t_u = top_u[x], t_v = top_v[x];
      int c_u = cur_u[x], c_v = cur_v[x];
      int avg_u = tl_u + t_u + l_u + c_u + 8;
      int avg_v = tl_v + t_v + l_v + c_v + 8;
      int d12_u = (avg_u + 2*(t_u + l_u)) >> 3, d12_v = (avg_v + 2*(t_v + l_v)) >> 3;
      int d03_u = (avg_u + 2*(tl_u + c_u)) >> 3, d03_v = (avg_v + 2*(tl_v + c_v)) >> 3;
      du[2*x-1] = (uint8_t)((d03_u + l_u) >> 1);
      dv[2*x-1] = (uint8_t)((d03_v + l_v) >> 1);
      du[2*x]   = (uint8_t)((d12_u + c_u) >> 1);
      dv[2*x]   = (uint8_t)((d12_v + c_v) >> 1);
      tl_u = t_u; tl_v = t_v; l_u = c_u; l_v = c_v;
   }
   if (!(len & 1))
   {
      du[len-1] = (uint8_t)((3*l_u + tl_u + 2) >> 2);
      dv[len-1] = (uint8_t)((3*l_v + tl_v + 2) >> 2);
   }
}

/* scratch: caller-provided buffer of 2*len bytes for the interpolated
 * chroma rows (kept per decode so concurrent decodes cannot interfere);
 * when NULL (allocation failed upstream) a fused scalar path that needs
 * no scratch is used instead. */
static void vp8_fancy_pair(const uint8_t *top_y, const uint8_t *bot_y,
      const uint8_t *top_u, const uint8_t *top_v,
      const uint8_t *cur_u, const uint8_t *cur_v,
      uint32_t *top_dst, uint32_t *bot_dst, int len,
      uint8_t *scratch)
{
   uint8_t *du, *dv;
   if (len <= 0)
      return;
   if (!scratch)
   {
      {
         /* No scratch (allocation failed upstream): fused scalar path. */
         int x, last_pair = (len - 1) >> 1;
         int tl_u = top_u[0], tl_v = top_v[0];
         int l_u = cur_u[0], l_v = cur_v[0];
         uint8_t r, g, b2;
         vp8_yuv2rgb(top_y[0], (3*tl_u+l_u+2)>>2, (3*tl_v+l_v+2)>>2, &r, &g, &b2);
         top_dst[0] = 0xFF000000u | ((uint32_t)r<<16) | ((uint32_t)g<<8) | b2;
         if (bot_y)
         {
            vp8_yuv2rgb(bot_y[0], (3*l_u+tl_u+2)>>2, (3*l_v+tl_v+2)>>2, &r, &g, &b2);
            bot_dst[0] = 0xFF000000u | ((uint32_t)r<<16) | ((uint32_t)g<<8) | b2;
         }
         for (x = 1; x <= last_pair; x++)
         {
            int t_u = top_u[x], t_v = top_v[x];
            int c_u = cur_u[x], c_v = cur_v[x];
            int avg_u = tl_u+t_u+l_u+c_u+8, avg_v = tl_v+t_v+l_v+c_v+8;
            int d12_u=(avg_u+2*(t_u+l_u))>>3, d12_v=(avg_v+2*(t_v+l_v))>>3;
            int d03_u=(avg_u+2*(tl_u+c_u))>>3, d03_v=(avg_v+2*(tl_v+c_v))>>3;
            vp8_yuv2rgb(top_y[2*x-1],(d12_u+tl_u)>>1,(d12_v+tl_v)>>1,&r,&g,&b2);
            top_dst[2*x-1]=0xFF000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|b2;
            vp8_yuv2rgb(top_y[2*x],(d03_u+t_u)>>1,(d03_v+t_v)>>1,&r,&g,&b2);
            top_dst[2*x]=0xFF000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|b2;
            if (bot_y)
            {
               vp8_yuv2rgb(bot_y[2*x-1],(d03_u+l_u)>>1,(d03_v+l_v)>>1,&r,&g,&b2);
               bot_dst[2*x-1]=0xFF000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|b2;
               vp8_yuv2rgb(bot_y[2*x],(d12_u+c_u)>>1,(d12_v+c_v)>>1,&r,&g,&b2);
               bot_dst[2*x]=0xFF000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|b2;
            }
            tl_u=t_u; tl_v=t_v; l_u=c_u; l_v=c_v;
         }
         if (!(len & 1))
         {
            vp8_yuv2rgb(top_y[len-1],(3*tl_u+l_u+2)>>2,(3*tl_v+l_v+2)>>2,&r,&g,&b2);
            top_dst[len-1]=0xFF000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|b2;
            if (bot_y)
            {
               vp8_yuv2rgb(bot_y[len-1],(3*l_u+tl_u+2)>>2,(3*l_v+tl_v+2)>>2,&r,&g,&b2);
               bot_dst[len-1]=0xFF000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|b2;
            }
         }
         return;
      }
   }
   du = scratch;
   dv = scratch + len;

   vp8_fancy_uv_top(top_u, top_v, cur_u, cur_v, du, dv, len);
   vp8_yuv2rgb_row(top_y, du, dv, top_dst, len);
   if (bot_y)
   {
      vp8_fancy_uv_bot(top_u, top_v, cur_u, cur_v, du, dv, len);
      vp8_yuv2rgb_row(bot_y, du, dv, bot_dst, len);
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

static void vp8_loop_filter_simple(uint8_t *y, int ys,
   int mbw, int lf_my0, int lf_my1, int lf_level, int sharpness,
   int seg_enabled, int seg_abs, const int *seg_lf,
   const uint8_t *seg_map, const uint8_t *skip_lf_map)
{
   int mx, my, i, e;
   for (my = lf_my0; my < lf_my1; my++)
   {
      for (mx = 0; mx < mbw; mx++)
      {
         int mb_lf = lf_level;
         int lim, blim, mblim, skip_lf;
         uint8_t *my0 = y + my * 16 * ys + mx * 16;
         if (seg_enabled && seg_abs)
            mb_lf = seg_lf[seg_map ? seg_map[my * mbw + mx] : 0];
         else if (seg_enabled)
            mb_lf = lf_level + seg_lf[seg_map ? seg_map[my * mbw + mx] : 0];
         if (mb_lf < 0) mb_lf = 0;
         if (mb_lf > 63) mb_lf = 63;
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
   const uint8_t *seg_map, const uint8_t *skip_lf_map, const uint8_t *bpred_map)
{
   int mx, my, e;
   for (my = lf_my0; my < lf_my1; my++)
   {
      for (mx = 0; mx < mbw; mx++)
      {
         int n = my * mbw + mx;
         int lvl = lf_level;
         int lim, blim, mblim, thr, skip_lf, is_bpred;
         uint8_t *my0 = y + my * 16 * ys + mx * 16;
         uint8_t *mu0 = u + my * 8 * uvs + mx * 8;
         uint8_t *mv0 = v_plane + my * 8 * uvs + mx * 8;

         if (seg_enabled && seg_abs)
            lvl = seg_lf[seg_map ? seg_map[n] : 0];
         else if (seg_enabled)
            lvl = lf_level + seg_lf[seg_map ? seg_map[n] : 0];
         if (lvl < 0) lvl = 0;
         if (lvl > 63) lvl = 63;
         is_bpred = bpred_map ? bpred_map[n] : 0;
         /* Keyframe delta adjustment (libvpx vp8_loop_filter_frame_init):
          * INTRA ref delta applies to all MBs; mode delta 0 to B_PRED. */
         if (lf_delta_enabled)
         {
            lvl += ref_lf_delta[0];
            if (is_bpred) lvl += mode_lf_delta[0];
            if (lvl < 0) lvl = 0;
            if (lvl > 63) lvl = 63;
         }
         if (lvl == 0) continue;

         /* Limits per vp8_loop_filter_update_sharpness */
         lim = lvl >> ((sharpness > 0) + (sharpness > 4));
         if (sharpness > 0 && lim > 9 - sharpness) lim = 9 - sharpness;
         if (lim < 1) lim = 1;
         mblim = 2 * (lvl + 2) + lim;
         blim  = 2 * lvl + lim;
         /* Keyframe high-edge-variance threshold */
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

   memset(s, 0, sizeof(*s));

   if (len < 10) return -1;
   ft = (uint32_t)data[0] | ((uint32_t)data[1]<<8) | ((uint32_t)data[2]<<16);
   kf = !(ft & 1);
   p0s = (ft >> 5) & 0x7FFFF;
   if (!kf) return -1;
   if (data[3]!=0x9D || data[4]!=0x01 || data[5]!=0x2A) return -1;
   w = (data[6]|(data[7]<<8)) & 0x3FFF;
   h = (data[8]|(data[9]<<8)) & 0x3FFF;
   if (!w || !h || w > 16384 || h > 16384) return -1;
   mbw = (w+15) >> 4; mbh = (h+15) >> 4;
   p0 = data + 10;
   if ((size_t)(p0 - data) + p0s > len) return -1;
   vp8b_init(&br, p0, (size_t)(data + len - p0));

   vp8b_bit(&br); vp8b_bit(&br); /* color_space, clamping */

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

   /* refresh_entropy_probs (RFC 6386) */
   (void)vp8b_bit(&br);

   /* Initialize coefficient probabilities */
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
         int ym, uvm, is_skip = 0, seg_id = 0, mb_has_coeffs = 0;
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

         if (ym == 4)
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

         /* UV mode */
         if      (!vp8b_get(&s->br, vp8_uvmp[0])) uvm = 0;
         else if (!vp8b_get(&s->br, vp8_uvmp[1])) uvm = 1;
         else if (!vp8b_get(&s->br, vp8_uvmp[2])) uvm = 2;
         else uvm = 3;

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
            if (ym != 4) /* not B_PRED */
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
                           s->cprob[(ym == 4) ? 3 : 0], start, sb_ctx);
                  /* Dequantize */
                  if (ym != 4)
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
            if (ym != 4) { above_nz_dc[mx] = 0; left_nz_dc = 0; }
         }
         /* libvpx: filter inner edges unless MB has no coefficients
          * (parsed skip OR eobtotal==0) and is not B_PRED. */
         if (skip_lf_buf)
            skip_lf_buf[my * mbw + mx] =
               (uint8_t)(((is_skip || !mb_has_coeffs) && ym != 4) ? 1 : 0);
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
            s->seg_lf, s->seg_map_buf, s->skip_lf_buf);
   else
      vp8_loop_filter_normal(s->yb, s->ys, s->ub, s->vb, s->uvs,
            s->mbw, my0, my1, s->lf_level, s->sharpness,
            s->seg_enabled, s->seg_abs, s->seg_lf,
            s->lf_delta_enabled, s->ref_lf_delta, s->mode_lf_delta,
            s->seg_map_buf, s->skip_lf_buf, s->bpred_buf);
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
            s->fancy_uv);
   for (; j + 2 < h && j < limit; j += 2)
   {
      const uint8_t *tu = ub + (j >> 1) * uvs, *tv = vb + (j >> 1) * uvs;
      vp8_fancy_pair(yb + (j+1)*ys, yb + (j+2)*ys,
            tu, tv, tu + uvs, tv + uvs,
            pix + (size_t)(j+1)*w, pix + (size_t)(j+2)*w, w,
            s->fancy_uv);
   }
   if (j + 2 >= h)
   {
      if (!(h & 1) && h >= 2)
      {
         const uint8_t *lu = ub + ((h-1) >> 1) * uvs;
         const uint8_t *lv = vb + ((h-1) >> 1) * uvs;
         vp8_fancy_pair(yb + (size_t)(h-1)*ys, NULL, lu, lv, lu, lv,
               pix + (size_t)(h-1)*w, NULL, w, s->fancy_uv);
      }
      return h;
   }
   return j;
}

/* One-shot wrapper preserving the original entry point: used by the
 * still-image fallback paths and the animation decoder, and doubling as
 * the reference composition of the resumable stages. */
uint32_t *rvp8_decode(const uint8_t *data, size_t len,
      unsigned *ow, unsigned *oh)
{
   rvp8_dec s;
   uint32_t *pix;
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
   rvp8_upsample_rows(&s, pix, 0, s.h);
   *ow = (unsigned)s.w;
   *oh = (unsigned)s.h;
   rvp8_abort(&s);
   return pix;
}

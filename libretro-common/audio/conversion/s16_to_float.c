/* Copyright  (C) 2010-2021 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (s16_to_float.c).
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
/* retro_environment.h spells MSVC's SSE/SSE2 targets as __SSE__/__SSE2__,
 * which the SIMD paths below test. */
#include <retro_environment.h>

#if defined(__SSE2__)
#include <emmintrin.h>
#elif defined(__ALTIVEC__)
#include <altivec.h>
#endif

#include <boolean.h>
#include <features/features_cpu.h>
#include <audio/conversion/s16_to_float.h>

#if (defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(HAVE_NEON))
static bool s16_to_float_neon_enabled = false;

#ifdef HAVE_ARM_NEON_ASM_OPTIMIZATIONS
/* Avoid potential hard-float/soft-float ABI issues. */
void convert_s16_float_asm(float *s, const int16_t *in,
      size_t len, const float *gain);
#else
#include <arm_neon.h>
#endif

void convert_s16_to_float(float *s,
      const int16_t *in, size_t len, float gain)
{
   unsigned i      = 0;

   if (s16_to_float_neon_enabled)
   {
#ifdef HAVE_ARM_NEON_ASM_OPTIMIZATIONS
      size_t aligned_samples = len & ~7;
      if (aligned_samples)
         convert_s16_float_asm(s, in, aligned_samples, &gain);

      /* Could do all conversion in ASM, but keep it simple for now. */
      s                 += aligned_samples;
      in                += aligned_samples;
      len               -= aligned_samples;
      i                  = 0;
#else
      float        gf    = gain / (1 << 15);
      float32x4_t vgf    = {gf, gf, gf, gf};
      while (len >= 8)
      {
         float32x4x2_t oreg;
         int16x4x2_t inreg   = vld2_s16(in);
         int32x4_t      p1   = vmovl_s16(inreg.val[0]);
         int32x4_t      p2   = vmovl_s16(inreg.val[1]);
         oreg.val[0]         = vmulq_f32(vcvtq_f32_s32(p1), vgf);
         oreg.val[1]         = vmulq_f32(vcvtq_f32_s32(p2), vgf);
         vst2q_f32(s, oreg);
         in                 += 8;
         s                  += 8;
         len                -= 8;
      }
#endif
   }

   gain /= 0x8000;

   for (; i < len; i++)
      s[i] = (float)in[i] * gain;
}

void convert_s16_to_float_init_simd(void)
{
   uint64_t cpu = cpu_features_get();

   if (cpu & RETRO_SIMD_NEON)
      s16_to_float_neon_enabled = true;
}
#else
/* AVX2 and AVX-512 forms of the SSE2 loop below, built per function and
 * chosen by convert_s16_to_float_init_simd(), so older CPUs keep SSE2
 * and compilers without per-function targets leave them out. Each
 * sample is one exact widen and one multiply by gain / 32768 - the same
 * single rounding as the SSE2 and scalar paths, so every target gives
 * the same float for the same s16. */
#if defined(__SSE2__) && (defined(__x86_64__) || defined(__i386__) \
      || defined(_M_X64) || defined(_M_IX86))
#if defined(_MSC_VER) && !defined(__clang__)
#if _MSC_VER >= 1911
#define S16_TO_FLOAT_HAVE_WIDE 1
#define S16_TO_FLOAT_HAVE_AVX512 1
#define S16_TO_FLOAT_TARGET_AVX2
#define S16_TO_FLOAT_TARGET_AVX512
#endif
#elif defined(__clang__) && (defined(__SCE__) || defined(_MSC_VER))
/* Clang's immintrin.h for PlayStation and clang-cl only declares the
 * intrinsics of ISAs enabled for the whole build, so per-function
 * targets cannot reach them; build a tier only when the baseline has
 * it (PS5's Zen 2 keeps AVX2, PS4's Jaguar has neither). */
#ifdef __AVX2__
#define S16_TO_FLOAT_HAVE_WIDE 1
#define S16_TO_FLOAT_TARGET_AVX2
#if defined(__AVX512F__) && defined(__AVX512BW__) && defined(__AVX512VL__)
#define S16_TO_FLOAT_HAVE_AVX512 1
#define S16_TO_FLOAT_TARGET_AVX512
#endif
#endif
#elif (defined(__clang__) && __clang_major__ >= 4) \
   || (defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 5)
#define S16_TO_FLOAT_HAVE_WIDE 1
#define S16_TO_FLOAT_HAVE_AVX512 1
#define S16_TO_FLOAT_TARGET_AVX2   __attribute__((target("avx2")))
#define S16_TO_FLOAT_TARGET_AVX512 \
   __attribute__((target("avx512f,avx512bw,avx512vl")))
#endif
#endif

#ifdef S16_TO_FLOAT_HAVE_WIDE
#include <immintrin.h>

/* 0 = SSE2, 1 = AVX2, 2 = AVX-512; set once by init, before audio runs. */
static int s16_to_float_tier = 0;

#ifdef S16_TO_FLOAT_TEST_HOOKS
/* -1 leaves the tier alone; otherwise caps it for a test. */
int s16_to_float_tier_cap = -1;
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#define S16_TO_FLOAT_ZEROUPPER() _mm256_zeroupper()
#else
#define S16_TO_FLOAT_ZEROUPPER()
#endif

/* Sixteen samples a step; returns how many it converted, leaving the
 * rest (under 16) to the SSE2 loop and the scalar tail. */
S16_TO_FLOAT_TARGET_AVX2
static size_t s16_to_float_avx2(float *s, const int16_t *in, size_t len,
      float gain)
{
   size_t i;
   const __m256 factor = _mm256_set1_ps(gain / 0x8000);
   for (i = 0; i + 16 <= len; i += 16)
   {
      _mm256_storeu_ps(s + i, _mm256_mul_ps(_mm256_cvtepi32_ps(
                  _mm256_cvtepi16_epi32(_mm_loadu_si128(
                        (const __m128i*)(in + i)))), factor));
      _mm256_storeu_ps(s + i + 8, _mm256_mul_ps(_mm256_cvtepi32_ps(
                  _mm256_cvtepi16_epi32(_mm_loadu_si128(
                        (const __m128i*)(in + i + 8)))), factor));
   }
   S16_TO_FLOAT_ZEROUPPER();
   return i;
}

/* Sixteen samples a step, the last masked, so no scalar tail. */
#ifdef S16_TO_FLOAT_HAVE_AVX512
S16_TO_FLOAT_TARGET_AVX512
static void s16_to_float_avx512(float *s, const int16_t *in, size_t len,
      float gain)
{
   size_t i;
   const __m512 factor = _mm512_set1_ps(gain / 0x8000);
   for (i = 0; i < len; i += 16)
   {
      size_t    left = len - i;
      __mmask16 m    = (left >= 16) ? (__mmask16)0xffff
         : (__mmask16)((1u << left) - 1u);
      _mm512_mask_storeu_ps(s + i, m, _mm512_mul_ps(_mm512_cvtepi32_ps(
                  _mm512_cvtepi16_epi32(_mm256_maskz_loadu_epi16(m,
                        in + i))), factor));
   }
   S16_TO_FLOAT_ZEROUPPER();
}
#endif
#endif

/* The SSE2 / AltiVec / Allegrex loop and the scalar tail. */
static void convert_s16_to_float_base(float *s,
      const int16_t *in, size_t len, float gain)
{
   unsigned i      = 0;

#if defined(__SSE2__)
   float fgain   = gain / UINT32_C(0x80000000);
   __m128 factor = _mm_set1_ps(fgain);

   for (i = 0; i + 8 <= len; i += 8, in += 8, s += 8)
   {
      __m128i input    = _mm_loadu_si128((const __m128i *)in);
      __m128i regs_l   = _mm_unpacklo_epi16(_mm_setzero_si128(), input);
      __m128i regs_r   = _mm_unpackhi_epi16(_mm_setzero_si128(), input);
      __m128 output_l  = _mm_mul_ps(_mm_cvtepi32_ps(regs_l), factor);
      __m128 output_r  = _mm_mul_ps(_mm_cvtepi32_ps(regs_r), factor);

      _mm_storeu_ps(s + 0, output_l);
      _mm_storeu_ps(s + 4, output_r);
   }

   len     = len - i;
   i       = 0;
#elif defined(__ALTIVEC__)
   size_t samples_in = len;

   /* Unaligned loads/store is a bit expensive, so we
    * optimize for the good path (very likely). */
   if (((uintptr_t)s & 15) + ((uintptr_t)in & 15) == 0)
   {
      const vector float gain_vec = { gain, gain , gain, gain };
      const vector float zero_vec = { 0.0f, 0.0f, 0.0f, 0.0f};

      for (i = 0; i + 8 <= len; i += 8, in += 8, s += 8)
      {
         vector signed short input = vec_ld(0, in);
         vector signed int hi      = vec_unpackh(input);
         vector signed int lo      = vec_unpackl(input);
         vector float out_hi       = vec_madd(vec_ctf(hi, 15), gain_vec, zero_vec);
         vector float out_lo       = vec_madd(vec_ctf(lo, 15), gain_vec, zero_vec);

         vec_st(out_hi,  0, s);
         vec_st(out_lo, 16, s);
      }

      samples_in -= i;
   }

   len     = samples_in;
   i       = 0;
#endif

   gain   /= 0x8000;

#if defined(_MIPS_ARCH_ALLEGREX)
#ifdef DEBUG
   /* Make sure the buffer is 16 byte aligned, this should be the
    * default behaviour of malloc in the PSPSDK.
    * Only the output buffer can be assumed to be 16-byte aligned. */
   retro_assert(((uintptr_t)s & 0xf) == 0);
#endif

   __asm__ (
         ".set    push                    \n"
         ".set    noreorder               \n"
         "mtv     %0, s200                \n"
         ".set    pop                     \n"
         ::"r"(gain));

   for (i = 0; i + 16 <= len; i += 16)
   {
      __asm__ (
            ".set    push                 \n"
            ".set    noreorder            \n"

            "lv.s    s100,  0(%0)         \n"
            "lv.s    s101,  4(%0)         \n"
            "lv.s    s110,  8(%0)         \n"
            "lv.s    s111, 12(%0)         \n"
            "lv.s    s120, 16(%0)         \n"
            "lv.s    s121, 20(%0)         \n"
            "lv.s    s130, 24(%0)         \n"
            "lv.s    s131, 28(%0)         \n"

            "vs2i.p  c100, c100           \n"
            "vs2i.p  c110, c110           \n"
            "vs2i.p  c120, c120           \n"
            "vs2i.p  c130, c130           \n"

            "vi2f.q  c100, c100, 16       \n"
            "vi2f.q  c110, c110, 16       \n"
            "vi2f.q  c120, c120, 16       \n"
            "vi2f.q  c130, c130, 16       \n"

            "vmscl.q e100, e100, s200     \n"

            "sv.q    c100,  0(%1)         \n"
            "sv.q    c110, 16(%1)         \n"
            "sv.q    c120, 32(%1)         \n"
            "sv.q    c130, 48(%1)         \n"

            ".set    pop                  \n"
            :: "r"(in + i), "r"(s + i));
   }
#endif

   for (; i < len; i++)
      s[i] = (float)in[i] * gain;
}

void convert_s16_to_float(float *s,
      const int16_t *in, size_t len, float gain)
{
#ifdef S16_TO_FLOAT_HAVE_WIDE
   int tier = s16_to_float_tier;
#ifdef S16_TO_FLOAT_TEST_HOOKS
   if (s16_to_float_tier_cap >= 0 && tier > s16_to_float_tier_cap)
      tier = s16_to_float_tier_cap;
#endif
#ifdef S16_TO_FLOAT_HAVE_AVX512
   if (tier == 2)
   {
      s16_to_float_avx512(s, in, len, gain);
      return;
   }
#endif
   if (tier == 1)
   {
      size_t done = s16_to_float_avx2(s, in, len, gain);
      s          += done;
      in         += done;
      len        -= done;
   }
#endif
   convert_s16_to_float_base(s, in, len, gain);
}

void convert_s16_to_float_init_simd(void)
{
#ifdef S16_TO_FLOAT_HAVE_WIDE
   uint64_t cpu      = cpu_features_get();
   s16_to_float_tier = 0;
   if (cpu & RETRO_SIMD_AVX2)
      s16_to_float_tier = 1;
#ifdef S16_TO_FLOAT_HAVE_AVX512
   if (cpu & RETRO_SIMD_AVX512)
      s16_to_float_tier = 2;
#endif
#endif
}
#endif


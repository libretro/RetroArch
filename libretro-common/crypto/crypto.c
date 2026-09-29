/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (crypto.c).
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

#include <string.h>
#include <crypto/crypto.h>

/* AES-NI + PCLMULQDQ path for x86, chosen at run time. Same gating as
 * the SHA-NI path in lrc_hash.c: GCC / Clang with the target
 * attribute, so the instructions are confined to the functions that
 * carry them and the TU's own baseline is untouched. */
#if (defined(__x86_64__) || defined(__i386__)) && !defined(_MSC_VER) && !defined(AES_NO_X86)
#if defined(__has_attribute) && defined(__has_include)
#if __has_attribute(target) && __has_include(<immintrin.h>) \
   && (!defined(__SCE__) || (defined(__AES__) && defined(__PCLMUL__) && defined(__SSSE3__)))
#define AES_HAVE_X86_PATH 1
#endif
#endif
#endif

#if defined(AES_HAVE_X86_PATH)
#include <immintrin.h>
#include <features/features_cpu.h>
#define AES_TARGET_X86 __attribute__((target("aes,pclmul,ssse3")))
#endif

/* ARMv8 AES + PMULL, same shape as lrc_hash.c's SHA path: used
 * unconditionally when the baseline has the crypto extension, else
 * compiled in under the target attribute and chosen at run time on
 * toolchains whose arm_neon.h declares the intrinsics regardless of
 * the baseline (Clang 16+, GCC 9+). */
#if (defined(__aarch64__) || defined(_M_ARM64)) && !defined(_MSC_VER) && !defined(AES_NO_ARM)
#if defined(__ARM_FEATURE_CRYPTO) || (defined(__ARM_FEATURE_AES) && defined(__ARM_FEATURE_SHA2))
#define AES_HAVE_ARM_PATH 1
#elif (defined(__clang__) && __clang_major__ >= 16) \
   || (!defined(__clang__) && defined(__GNUC__) && __GNUC__ >= 9)
#define AES_HAVE_ARM_PATH    1
#define AES_ARM_NEEDS_TARGET 1
#define AES_ARM_DISPATCH     1
#endif
#endif

#if defined(AES_HAVE_ARM_PATH)
#include <arm_neon.h>
#include <features/features_cpu.h>
#if defined(AES_ARM_NEEDS_TARGET)
#define AES_TARGET_ARM __attribute__((target("+crypto")))
#else
#define AES_TARGET_ARM
#endif
#endif

int crypto_memeq_ct(const void *a, const void *b, size_t len)
{
   const volatile uint8_t *pa = (const volatile uint8_t*)a;
   const volatile uint8_t *pb = (const volatile uint8_t*)b;
   uint8_t acc = 0;
   size_t i;

   for (i = 0; i < len; i++)
      acc |= (uint8_t)(pa[i] ^ pb[i]);

   /* 1 when every octet matched, with no data-dependent branch. */
   return (int)((((unsigned)acc - 1) >> 8) & 1);
}

void crypto_memzero(void *p, size_t len)
{
   volatile uint8_t *vp = (volatile uint8_t*)p;
   while (len--)
      *vp++ = 0;
}

static const uint8_t aes_sbox[256] = {
   0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
   0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
   0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
   0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
   0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
   0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
   0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
   0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
   0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
   0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
   0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
   0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
   0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
   0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
   0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
   0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

static const uint8_t aes_rcon[10] = {
   0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36
};

#define XTIME(x) ((uint8_t)(((x) << 1) ^ ((((x) >> 7) & 1) * 0x1b)))

static uint32_t aes_sub_word(uint32_t w)
{
   return ((uint32_t)aes_sbox[(w >> 24) & 0xff] << 24)
        | ((uint32_t)aes_sbox[(w >> 16) & 0xff] << 16)
        | ((uint32_t)aes_sbox[(w >>  8) & 0xff] <<  8)
        |  (uint32_t)aes_sbox[ w        & 0xff];
}

/* Round tables: ft[0][x] = (2s, s, s, 3s) with s = sbox[x], the other
 * three are its byte rotations. Filled on first use; every writer
 * stores the same values, so a race on first use is harmless. The
 * inverse S-box and imc[x] = (14x, 9x, 13x, 11x) serve the inverse
 * cipher, which runs in the standard order on the encryption schedule
 * (InvMixColumns on the state, so no second schedule is kept). */
static uint32_t aes_ft[4][256];
static uint8_t  aes_isbox[256];
static uint32_t aes_imc[256];
static int      aes_ft_ready = 0;

static void aes_ft_init(void)
{
   unsigned x;
   for (x = 0; x < 256; x++)
   {
      uint32_t sv = aes_sbox[x];
      uint32_t x2 = XTIME(sv);
      uint32_t x3 = x2 ^ sv;
      uint32_t t  = (x2 << 24) | (sv << 16) | (sv << 8) | x3;
      uint32_t m2 = XTIME((uint8_t)x), m4 = XTIME((uint8_t)m2), m8 = XTIME((uint8_t)m4);
      uint32_t m9 = m8 ^ x, m11 = m8 ^ m2 ^ x, m13 = m8 ^ m4 ^ x, m14 = m8 ^ m4 ^ m2;
      aes_ft[0][x] = t;
      aes_ft[1][x] = (t >>  8) | (t << 24);
      aes_ft[2][x] = (t >> 16) | (t << 16);
      aes_ft[3][x] = (t >> 24) | (t <<  8);
      aes_isbox[sv] = (uint8_t)x;
      aes_imc[x]    = (m14 << 24) | (m9 << 16) | (m13 << 8) | m11;
   }
   aes_ft_ready = 1;
}

#if defined(AES_HAVE_X86_PATH)
static int aes_x86_ok = -1;

static int aes_x86_available(void)
{
   if (aes_x86_ok < 0)
   {
      uint64_t f  = cpu_features_get();
      aes_x86_ok  = (f & RETRO_SIMD_AES) && (f & RETRO_SIMD_PCLMUL) ? 1 : 0;
   }
   return aes_x86_ok;
}

AES_TARGET_X86
static __m128i aes_x86_bswap32(__m128i v)
{
   const __m128i m = _mm_set_epi8(12,13,14,15, 8,9,10,11, 4,5,6,7, 0,1,2,3);
   return _mm_shuffle_epi8(v, m);
}

AES_TARGET_X86
static __m128i aes_x86_encrypt(const struct aes_ctx *ctx, __m128i b)
{
   unsigned r;
   b = _mm_xor_si128(b, aes_x86_bswap32(_mm_loadu_si128((const __m128i*)ctx->rk)));
   for (r = 1; r < ctx->rounds; r++)
      b = _mm_aesenc_si128(b, aes_x86_bswap32(_mm_loadu_si128((const __m128i*)(ctx->rk + 4 * r))));
   return _mm_aesenclast_si128(b, aes_x86_bswap32(_mm_loadu_si128((const __m128i*)(ctx->rk + 4 * r))));
}

AES_TARGET_X86
static void aes_x86_encrypt_block(const struct aes_ctx *ctx,
      const uint8_t *in, uint8_t *out)
{
   _mm_storeu_si128((__m128i*)out,
         aes_x86_encrypt(ctx, _mm_loadu_si128((const __m128i*)in)));
}

/* Inverse cipher on the encryption schedule: aesdec wants the round
 * keys through InvMixColumns, applied here per block, as decryption
 * only serves the short Kerberos messages and keeps the context and
 * every other caller unchanged. */
AES_TARGET_X86
static void aes_x86_decrypt_block(const struct aes_ctx *ctx,
      const uint8_t *in, uint8_t *out)
{
   unsigned r = ctx->rounds;
   __m128i b  = _mm_loadu_si128((const __m128i*)in);
   b = _mm_xor_si128(b, aes_x86_bswap32(_mm_loadu_si128((const __m128i*)(ctx->rk + 4 * r))));
   for (r--; r > 0; r--)
      b = _mm_aesdec_si128(b, _mm_aesimc_si128(
               aes_x86_bswap32(_mm_loadu_si128((const __m128i*)(ctx->rk + 4 * r)))));
   b = _mm_aesdeclast_si128(b, aes_x86_bswap32(_mm_loadu_si128((const __m128i*)ctx->rk)));
   _mm_storeu_si128((__m128i*)out, b);
}

/* CTR with four blocks in flight so the AES units overlap. */
AES_TARGET_X86
static void aes_x86_ctr(const struct aes_ctx *ctx, uint8_t *counter,
      const uint8_t *in, uint8_t *out, size_t len)
{
   uint32_t c = crypto_load32_be(counter + 12);
   __m128i  base = _mm_loadu_si128((const __m128i*)counter);
   const __m128i lanemask = _mm_set_epi32(0, -1, -1, -1);

   while (len >= 64)
   {
      __m128i b0, b1, b2, b3;
      b0 = _mm_or_si128(_mm_and_si128(base, lanemask), _mm_set_epi32((int)__builtin_bswap32(c),     0, 0, 0));
      b1 = _mm_or_si128(_mm_and_si128(base, lanemask), _mm_set_epi32((int)__builtin_bswap32(c + 1), 0, 0, 0));
      b2 = _mm_or_si128(_mm_and_si128(base, lanemask), _mm_set_epi32((int)__builtin_bswap32(c + 2), 0, 0, 0));
      b3 = _mm_or_si128(_mm_and_si128(base, lanemask), _mm_set_epi32((int)__builtin_bswap32(c + 3), 0, 0, 0));
      {
         unsigned r;
         __m128i k = aes_x86_bswap32(_mm_loadu_si128((const __m128i*)ctx->rk));
         b0 = _mm_xor_si128(b0, k); b1 = _mm_xor_si128(b1, k);
         b2 = _mm_xor_si128(b2, k); b3 = _mm_xor_si128(b3, k);
         for (r = 1; r < ctx->rounds; r++)
         {
            k  = aes_x86_bswap32(_mm_loadu_si128((const __m128i*)(ctx->rk + 4 * r)));
            b0 = _mm_aesenc_si128(b0, k); b1 = _mm_aesenc_si128(b1, k);
            b2 = _mm_aesenc_si128(b2, k); b3 = _mm_aesenc_si128(b3, k);
         }
         k  = aes_x86_bswap32(_mm_loadu_si128((const __m128i*)(ctx->rk + 4 * r)));
         b0 = _mm_aesenclast_si128(b0, k); b1 = _mm_aesenclast_si128(b1, k);
         b2 = _mm_aesenclast_si128(b2, k); b3 = _mm_aesenclast_si128(b3, k);
      }
      _mm_storeu_si128((__m128i*)(out),      _mm_xor_si128(b0, _mm_loadu_si128((const __m128i*)(in))));
      _mm_storeu_si128((__m128i*)(out + 16), _mm_xor_si128(b1, _mm_loadu_si128((const __m128i*)(in + 16))));
      _mm_storeu_si128((__m128i*)(out + 32), _mm_xor_si128(b2, _mm_loadu_si128((const __m128i*)(in + 32))));
      _mm_storeu_si128((__m128i*)(out + 48), _mm_xor_si128(b3, _mm_loadu_si128((const __m128i*)(in + 48))));
      c   += 4;
      in  += 64;
      out += 64;
      len -= 64;
   }
   while (len)
   {
      size_t  n = (len < 16) ? len : 16;
      size_t  i;
      uint8_t ks[16];
      __m128i b = _mm_or_si128(_mm_and_si128(base, lanemask), _mm_set_epi32((int)__builtin_bswap32(c), 0, 0, 0));
      _mm_storeu_si128((__m128i*)ks, aes_x86_encrypt(ctx, b));
      for (i = 0; i < n; i++)
         out[i] = in[i] ^ ks[i];
      c++;
      in  += n;
      out += n;
      len -= n;
   }
   crypto_store32_be(counter + 12, c);
}

/* GHASH multiply with PCLMULQDQ, operands byte-reversed so the
 * reflected GCM bit order becomes a plain polynomial product
 * (Intel's "Algorithm 1"); the reduction is modulo x^128 + x^7 +
 * x^2 + x + 1. */
AES_TARGET_X86
static __m128i aes_x86_gfmul(__m128i a, __m128i b)
{
   __m128i t3 = _mm_clmulepi64_si128(a, b, 0x00);
   __m128i t4 = _mm_clmulepi64_si128(a, b, 0x10);
   __m128i t5 = _mm_clmulepi64_si128(a, b, 0x01);
   __m128i t6 = _mm_clmulepi64_si128(a, b, 0x11);
   __m128i t7, t8, t9, t2;

   t4 = _mm_xor_si128(t4, t5);
   t5 = _mm_slli_si128(t4, 8);
   t4 = _mm_srli_si128(t4, 8);
   t3 = _mm_xor_si128(t3, t5);
   t6 = _mm_xor_si128(t6, t4);

   t7 = _mm_srli_epi32(t3, 31);
   t8 = _mm_srli_epi32(t6, 31);
   t3 = _mm_slli_epi32(t3, 1);
   t6 = _mm_slli_epi32(t6, 1);
   t9 = _mm_srli_si128(t7, 12);
   t8 = _mm_slli_si128(t8, 4);
   t7 = _mm_slli_si128(t7, 4);
   t3 = _mm_or_si128(t3, t7);
   t6 = _mm_or_si128(t6, t8);
   t6 = _mm_or_si128(t6, t9);

   t7 = _mm_slli_epi32(t3, 31);
   t8 = _mm_slli_epi32(t3, 30);
   t9 = _mm_slli_epi32(t3, 25);
   t7 = _mm_xor_si128(t7, t8);
   t7 = _mm_xor_si128(t7, t9);
   t8 = _mm_srli_si128(t7, 4);
   t7 = _mm_slli_si128(t7, 12);
   t3 = _mm_xor_si128(t3, t7);

   t2 = _mm_srli_epi32(t3, 1);
   t4 = _mm_srli_epi32(t3, 2);
   t5 = _mm_srli_epi32(t3, 7);
   t2 = _mm_xor_si128(t2, t4);
   t2 = _mm_xor_si128(t2, t5);
   t2 = _mm_xor_si128(t2, t8);
   t3 = _mm_xor_si128(t3, t2);
   return _mm_xor_si128(t6, t3);
}

AES_TARGET_X86
static void aes_x86_ghash(const struct aes_gcm_ctx *ctx,
      uint64_t *y_hi, uint64_t *y_lo, const uint8_t *data, size_t len)
{
   const __m128i rev = _mm_set_epi8(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15);
   /* h_hi / h_lo are the big-endian halves of the 16 memory octets;
    * byte-reversing that whole block is what the multiply wants. */
   __m128i h = _mm_shuffle_epi8(_mm_set_epi64x((long long)__builtin_bswap64(ctx->h_lo), (long long)__builtin_bswap64(ctx->h_hi)), rev);
   __m128i y = _mm_shuffle_epi8(_mm_set_epi64x((long long)__builtin_bswap64(*y_lo), (long long)__builtin_bswap64(*y_hi)), rev);
   uint8_t tail[16];

   while (len >= 16)
   {
      __m128i d = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)data), rev);
      y = aes_x86_gfmul(_mm_xor_si128(y, d), h);
      data += 16;
      len  -= 16;
   }
   if (len)
   {
      memset(tail, 0, 16);
      memcpy(tail, data, len);
      y = aes_x86_gfmul(_mm_xor_si128(y, _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)tail), rev)), h);
   }

   {
      uint8_t o[16];
      _mm_storeu_si128((__m128i*)o, _mm_shuffle_epi8(y, rev));
      *y_hi = crypto_load64_be(o);
      *y_lo = crypto_load64_be(o + 8);
   }
}
#endif

#if defined(AES_HAVE_ARM_PATH)
static int aes_arm_available(void)
{
#if defined(AES_ARM_DISPATCH)
   static int ok = -1;
   if (ok < 0)
      ok = (cpu_features_get() & RETRO_SIMD_AES) ? 1 : 0;
   return ok;
#else
   return 1;
#endif
}

/* Round key r as the 16 memory octets: the schedule holds big-endian
 * words, so a little-endian load needs each word's bytes reversed. */
AES_TARGET_ARM
static uint8x16_t aes_arm_rk(const struct aes_ctx *ctx, unsigned r)
{
   return vrev32q_u8(vreinterpretq_u8_u32(vld1q_u32(ctx->rk + 4 * r)));
}

AES_TARGET_ARM
static uint8x16_t aes_arm_encrypt(const struct aes_ctx *ctx, uint8x16_t b)
{
   unsigned r;
   for (r = 0; r + 1 < ctx->rounds; r++)
      b = vaesmcq_u8(vaeseq_u8(b, aes_arm_rk(ctx, r)));
   b = vaeseq_u8(b, aes_arm_rk(ctx, r));
   return veorq_u8(b, aes_arm_rk(ctx, r + 1));
}

AES_TARGET_ARM
static void aes_arm_encrypt_block(const struct aes_ctx *ctx,
      const uint8_t *in, uint8_t *out)
{
   vst1q_u8(out, aes_arm_encrypt(ctx, vld1q_u8(in)));
}

/* Equivalent inverse cipher: AESD fuses the round key add with the
 * inverse shift and substitution, so the InvMixColumns that the
 * standard order puts after the key add moves onto the round key. */
AES_TARGET_ARM
static void aes_arm_decrypt_block(const struct aes_ctx *ctx,
      const uint8_t *in, uint8_t *out)
{
   unsigned r    = ctx->rounds;
   uint8x16_t b  = vaesdq_u8(vld1q_u8(in), aes_arm_rk(ctx, r));
   for (r--; r > 0; r--)
      b = vaesdq_u8(vaesimcq_u8(b), vaesimcq_u8(aes_arm_rk(ctx, r)));
   vst1q_u8(out, veorq_u8(b, aes_arm_rk(ctx, 0)));
}

AES_TARGET_ARM
static void aes_arm_ctr(const struct aes_ctx *ctx, uint8_t *counter,
      const uint8_t *in, uint8_t *out, size_t len)
{
   uint32_t   c    = crypto_load32_be(counter + 12);
   uint8x16_t base = vld1q_u8(counter);
   uint8x16_t ks;

   while (len >= 64)
   {
      uint8x16_t b0 = vreinterpretq_u8_u32(vsetq_lane_u32(__builtin_bswap32(c),     vreinterpretq_u32_u8(base), 3));
      uint8x16_t b1 = vreinterpretq_u8_u32(vsetq_lane_u32(__builtin_bswap32(c + 1), vreinterpretq_u32_u8(base), 3));
      uint8x16_t b2 = vreinterpretq_u8_u32(vsetq_lane_u32(__builtin_bswap32(c + 2), vreinterpretq_u32_u8(base), 3));
      uint8x16_t b3 = vreinterpretq_u8_u32(vsetq_lane_u32(__builtin_bswap32(c + 3), vreinterpretq_u32_u8(base), 3));
      unsigned r;
      for (r = 0; r + 1 < ctx->rounds; r++)
      {
         uint8x16_t k = aes_arm_rk(ctx, r);
         b0 = vaesmcq_u8(vaeseq_u8(b0, k)); b1 = vaesmcq_u8(vaeseq_u8(b1, k));
         b2 = vaesmcq_u8(vaeseq_u8(b2, k)); b3 = vaesmcq_u8(vaeseq_u8(b3, k));
      }
      {
         uint8x16_t k  = aes_arm_rk(ctx, r);
         uint8x16_t kl = aes_arm_rk(ctx, r + 1);
         b0 = veorq_u8(vaeseq_u8(b0, k), kl); b1 = veorq_u8(vaeseq_u8(b1, k), kl);
         b2 = veorq_u8(vaeseq_u8(b2, k), kl); b3 = veorq_u8(vaeseq_u8(b3, k), kl);
      }
      vst1q_u8(out,      veorq_u8(b0, vld1q_u8(in)));
      vst1q_u8(out + 16, veorq_u8(b1, vld1q_u8(in + 16)));
      vst1q_u8(out + 32, veorq_u8(b2, vld1q_u8(in + 32)));
      vst1q_u8(out + 48, veorq_u8(b3, vld1q_u8(in + 48)));
      c   += 4;
      in  += 64;
      out += 64;
      len -= 64;
   }
   while (len)
   {
      size_t  n = (len < 16) ? len : 16;
      size_t  i;
      uint8_t tmp[16];
      ks = aes_arm_encrypt(ctx, vreinterpretq_u8_u32(vsetq_lane_u32(__builtin_bswap32(c), vreinterpretq_u32_u8(base), 3)));
      vst1q_u8(tmp, ks);
      for (i = 0; i < n; i++)
         out[i] = in[i] ^ tmp[i];
      c++;
      in  += n;
      out += n;
      len -= n;
   }
   crypto_store32_be(counter + 12, c);
}

/* GHASH multiply on PMULL: the same byte-reversed-operand algorithm
 * as the x86 path, with the SSE lane shifts spelled as vext and the
 * word shifts as vshl / vshr. */
AES_TARGET_ARM
static uint8x16_t aes_arm_gfmul(uint8x16_t a, uint8x16_t b)
{
   const uint8x16_t z = vdupq_n_u8(0);
   poly64x2_t pa = vreinterpretq_p64_u8(a);
   poly64x2_t pb = vreinterpretq_p64_u8(b);
   uint8x16_t t3 = vreinterpretq_u8_p128(vmull_p64((poly64_t)vgetq_lane_p64(pa, 0), (poly64_t)vgetq_lane_p64(pb, 0)));
   uint8x16_t t4 = vreinterpretq_u8_p128(vmull_p64((poly64_t)vgetq_lane_p64(pa, 0), (poly64_t)vgetq_lane_p64(pb, 1)));
   uint8x16_t t5 = vreinterpretq_u8_p128(vmull_p64((poly64_t)vgetq_lane_p64(pa, 1), (poly64_t)vgetq_lane_p64(pb, 0)));
   uint8x16_t t6 = vreinterpretq_u8_p128(vmull_high_p64(pa, pb));
   uint8x16_t t7, t8, t9, t2;
   uint32x4_t w;

   t4 = veorq_u8(t4, t5);
   t5 = vextq_u8(z, t4, 8);            /* slli_si128 8 */
   t4 = vextq_u8(t4, z, 8);            /* srli_si128 8 */
   t3 = veorq_u8(t3, t5);
   t6 = veorq_u8(t6, t4);

   w  = vreinterpretq_u32_u8(t3);
   t7 = vreinterpretq_u8_u32(vshrq_n_u32(w, 31));
   t3 = vreinterpretq_u8_u32(vshlq_n_u32(w, 1));
   w  = vreinterpretq_u32_u8(t6);
   t8 = vreinterpretq_u8_u32(vshrq_n_u32(w, 31));
   t6 = vreinterpretq_u8_u32(vshlq_n_u32(w, 1));
   t9 = vextq_u8(t7, z, 12);           /* srli_si128 12 */
   t8 = vextq_u8(z, t8, 12);           /* slli_si128 4 */
   t7 = vextq_u8(z, t7, 12);           /* slli_si128 4 */
   t3 = vorrq_u8(t3, t7);
   t6 = vorrq_u8(t6, t8);
   t6 = vorrq_u8(t6, t9);

   w  = vreinterpretq_u32_u8(t3);
   t7 = vreinterpretq_u8_u32(vshlq_n_u32(w, 31));
   t8 = vreinterpretq_u8_u32(vshlq_n_u32(w, 30));
   t9 = vreinterpretq_u8_u32(vshlq_n_u32(w, 25));
   t7 = veorq_u8(t7, t8);
   t7 = veorq_u8(t7, t9);
   t8 = vextq_u8(t7, z, 4);            /* srli_si128 4 */
   t7 = vextq_u8(z, t7, 4);            /* slli_si128 12 */
   t3 = veorq_u8(t3, t7);

   w  = vreinterpretq_u32_u8(t3);
   t2 = vreinterpretq_u8_u32(vshrq_n_u32(w, 1));
   t4 = vreinterpretq_u8_u32(vshrq_n_u32(w, 2));
   t5 = vreinterpretq_u8_u32(vshrq_n_u32(w, 7));
   t2 = veorq_u8(t2, t4);
   t2 = veorq_u8(t2, t5);
   t2 = veorq_u8(t2, t8);
   t3 = veorq_u8(t3, t2);
   return veorq_u8(t6, t3);
}

AES_TARGET_ARM
static void aes_arm_ghash(const struct aes_gcm_ctx *ctx,
      uint64_t *y_hi, uint64_t *y_lo, const uint8_t *data, size_t len)
{
   uint8_t    o[16];
   uint8x16_t h, y;

   crypto_store64_be(o,     ctx->h_hi);
   crypto_store64_be(o + 8, ctx->h_lo);
   h = vrev64q_u8(vextq_u8(vld1q_u8(o), vld1q_u8(o), 8));   /* full byte reverse */
   crypto_store64_be(o,     *y_hi);
   crypto_store64_be(o + 8, *y_lo);
   y = vrev64q_u8(vextq_u8(vld1q_u8(o), vld1q_u8(o), 8));

   while (len >= 16)
   {
      uint8x16_t d = vld1q_u8(data);
      d = vrev64q_u8(vextq_u8(d, d, 8));
      y = aes_arm_gfmul(veorq_u8(y, d), h);
      data += 16;
      len  -= 16;
   }
   if (len)
   {
      uint8x16_t d;
      memset(o, 0, 16);
      memcpy(o, data, len);
      d = vld1q_u8(o);
      d = vrev64q_u8(vextq_u8(d, d, 8));
      y = aes_arm_gfmul(veorq_u8(y, d), h);
   }

   vst1q_u8(o, vrev64q_u8(vextq_u8(y, y, 8)));
   *y_hi = crypto_load64_be(o);
   *y_lo = crypto_load64_be(o + 8);
}
#endif

int aes_init(struct aes_ctx *ctx, const uint8_t *key, size_t key_len)
{
   unsigned i;
   unsigned nk;
   unsigned total;

   switch (key_len)
   {
      case 16: nk = 4; ctx->rounds = 10; break;
      case 24: nk = 6; ctx->rounds = 12; break;
      case 32: nk = 8; ctx->rounds = 14; break;
      default:
         return -1;
   }

   if (!aes_ft_ready)
      aes_ft_init();

   total = 4 * (ctx->rounds + 1);

   for (i = 0; i < nk; i++)
      ctx->rk[i] = crypto_load32_be(key + 4 * i);

   for (i = nk; i < total; i++)
   {
      uint32_t t = ctx->rk[i - 1];
      if ((i % nk) == 0)
         t = aes_sub_word(crypto_rotl32(t, 8)) ^ ((uint32_t)aes_rcon[i / nk - 1] << 24);
      else if (nk > 6 && (i % nk) == 4)
         t = aes_sub_word(t);
      ctx->rk[i] = ctx->rk[i - nk] ^ t;
   }

   return 0;
}

static void aes_encrypt_block_c(const struct aes_ctx *ctx,
      const uint8_t *in, uint8_t *out)
{
   const uint32_t *rk = ctx->rk;
   uint32_t s0, s1, s2, s3, t0, t1, t2, t3;
   unsigned r;

   s0 = crypto_load32_be(in)      ^ rk[0];
   s1 = crypto_load32_be(in + 4)  ^ rk[1];
   s2 = crypto_load32_be(in + 8)  ^ rk[2];
   s3 = crypto_load32_be(in + 12) ^ rk[3];
   rk += 4;

#define AES_ROUND(o0, o1, o2, o3, i0, i1, i2, i3)                        \
   o0 = aes_ft[0][i0 >> 24] ^ aes_ft[1][(i1 >> 16) & 0xff]              \
      ^ aes_ft[2][(i2 >> 8) & 0xff] ^ aes_ft[3][i3 & 0xff] ^ rk[0];      \
   o1 = aes_ft[0][i1 >> 24] ^ aes_ft[1][(i2 >> 16) & 0xff]              \
      ^ aes_ft[2][(i3 >> 8) & 0xff] ^ aes_ft[3][i0 & 0xff] ^ rk[1];      \
   o2 = aes_ft[0][i2 >> 24] ^ aes_ft[1][(i3 >> 16) & 0xff]              \
      ^ aes_ft[2][(i0 >> 8) & 0xff] ^ aes_ft[3][i1 & 0xff] ^ rk[2];      \
   o3 = aes_ft[0][i3 >> 24] ^ aes_ft[1][(i0 >> 16) & 0xff]              \
      ^ aes_ft[2][(i1 >> 8) & 0xff] ^ aes_ft[3][i2 & 0xff] ^ rk[3];      \
   rk += 4

   for (r = 1; r < ctx->rounds; r += 2)
   {
      AES_ROUND(t0, t1, t2, t3, s0, s1, s2, s3);
      if (r + 1 == ctx->rounds)
      {
         s0 = t0; s1 = t1; s2 = t2; s3 = t3;
         break;
      }
      AES_ROUND(s0, s1, s2, s3, t0, t1, t2, t3);
   }
#undef AES_ROUND

   /* Last round: SubBytes + ShiftRows only. */
   t0 = ((uint32_t)aes_sbox[s0 >> 24] << 24) ^ ((uint32_t)aes_sbox[(s1 >> 16) & 0xff] << 16)
      ^ ((uint32_t)aes_sbox[(s2 >> 8) & 0xff] << 8) ^ aes_sbox[s3 & 0xff] ^ rk[0];
   t1 = ((uint32_t)aes_sbox[s1 >> 24] << 24) ^ ((uint32_t)aes_sbox[(s2 >> 16) & 0xff] << 16)
      ^ ((uint32_t)aes_sbox[(s3 >> 8) & 0xff] << 8) ^ aes_sbox[s0 & 0xff] ^ rk[1];
   t2 = ((uint32_t)aes_sbox[s2 >> 24] << 24) ^ ((uint32_t)aes_sbox[(s3 >> 16) & 0xff] << 16)
      ^ ((uint32_t)aes_sbox[(s0 >> 8) & 0xff] << 8) ^ aes_sbox[s1 & 0xff] ^ rk[2];
   t3 = ((uint32_t)aes_sbox[s3 >> 24] << 24) ^ ((uint32_t)aes_sbox[(s0 >> 16) & 0xff] << 16)
      ^ ((uint32_t)aes_sbox[(s1 >> 8) & 0xff] << 8) ^ aes_sbox[s2 & 0xff] ^ rk[3];

   crypto_store32_be(out,      t0);
   crypto_store32_be(out + 4,  t1);
   crypto_store32_be(out + 8,  t2);
   crypto_store32_be(out + 12, t3);
}

/* InvMixColumns of one column: each output byte is the matching
 * byte of imc[b] rotated into place. */
static uint32_t aes_imc_col(uint32_t c)
{
   uint32_t t;
   t  = aes_imc[(c >> 24) & 0xff];
   t ^= crypto_rotl32(aes_imc[(c >> 16) & 0xff], 24);
   t ^= crypto_rotl32(aes_imc[(c >>  8) & 0xff], 16);
   t ^= crypto_rotl32(aes_imc[ c        & 0xff],  8);
   return t;
}

/* Inverse cipher, standard order: AddRoundKey with the last round key,
 * then per round InvShiftRows + InvSubBytes, AddRoundKey, and
 * InvMixColumns on the state (skipped for the final round). */
static void aes_decrypt_block_c(const struct aes_ctx *ctx,
      const uint8_t *in, uint8_t *out)
{
   const uint32_t *rk = ctx->rk + 4 * ctx->rounds;
   uint32_t s0, s1, s2, s3, t0, t1, t2, t3;
   unsigned r;

   s0 = crypto_load32_be(in)      ^ rk[0];
   s1 = crypto_load32_be(in + 4)  ^ rk[1];
   s2 = crypto_load32_be(in + 8)  ^ rk[2];
   s3 = crypto_load32_be(in + 12) ^ rk[3];

   for (r = ctx->rounds; r > 0; r--)
   {
      rk -= 4;
      /* InvShiftRows moves row i right by i; column j gathers byte
       * row i from column (j - i) mod 4 */
      t0 = ((uint32_t)aes_isbox[(s0 >> 24) & 0xff] << 24)
         | ((uint32_t)aes_isbox[(s3 >> 16) & 0xff] << 16)
         | ((uint32_t)aes_isbox[(s2 >>  8) & 0xff] <<  8)
         |  (uint32_t)aes_isbox[ s1        & 0xff];
      t1 = ((uint32_t)aes_isbox[(s1 >> 24) & 0xff] << 24)
         | ((uint32_t)aes_isbox[(s0 >> 16) & 0xff] << 16)
         | ((uint32_t)aes_isbox[(s3 >>  8) & 0xff] <<  8)
         |  (uint32_t)aes_isbox[ s2        & 0xff];
      t2 = ((uint32_t)aes_isbox[(s2 >> 24) & 0xff] << 24)
         | ((uint32_t)aes_isbox[(s1 >> 16) & 0xff] << 16)
         | ((uint32_t)aes_isbox[(s0 >>  8) & 0xff] <<  8)
         |  (uint32_t)aes_isbox[ s3        & 0xff];
      t3 = ((uint32_t)aes_isbox[(s3 >> 24) & 0xff] << 24)
         | ((uint32_t)aes_isbox[(s2 >> 16) & 0xff] << 16)
         | ((uint32_t)aes_isbox[(s1 >>  8) & 0xff] <<  8)
         |  (uint32_t)aes_isbox[ s0        & 0xff];
      t0 ^= rk[0]; t1 ^= rk[1]; t2 ^= rk[2]; t3 ^= rk[3];
      if (r > 1)
      {
         s0 = aes_imc_col(t0); s1 = aes_imc_col(t1);
         s2 = aes_imc_col(t2); s3 = aes_imc_col(t3);
      }
      else
      {
         s0 = t0; s1 = t1; s2 = t2; s3 = t3;
      }
   }

   crypto_store32_be(out,      s0);
   crypto_store32_be(out + 4,  s1);
   crypto_store32_be(out + 8,  s2);
   crypto_store32_be(out + 12, s3);
}

void aes_decrypt_block(const struct aes_ctx *ctx,
      const uint8_t *in, uint8_t *out)
{
#if defined(AES_HAVE_X86_PATH)
   if (aes_x86_available())
   {
      aes_x86_decrypt_block(ctx, in, out);
      return;
   }
#elif defined(AES_HAVE_ARM_PATH)
   if (aes_arm_available())
   {
      aes_arm_decrypt_block(ctx, in, out);
      return;
   }
#endif
   aes_decrypt_block_c(ctx, in, out);
}

void aes_encrypt_block(const struct aes_ctx *ctx,
      const uint8_t *in, uint8_t *out)
{
#if defined(AES_HAVE_X86_PATH)
   if (aes_x86_available())
   {
      aes_x86_encrypt_block(ctx, in, out);
      return;
   }
#elif defined(AES_HAVE_ARM_PATH)
   if (aes_arm_available())
   {
      aes_arm_encrypt_block(ctx, in, out);
      return;
   }
#endif
   aes_encrypt_block_c(ctx, in, out);
}

static void aes_ctr_inc32(uint8_t *counter)
{
   uint32_t v = crypto_load32_be(counter + 12) + 1;
   crypto_store32_be(counter + 12, v);
}

void aes_ctr_crypt(const struct aes_ctx *ctx, uint8_t *counter,
      const uint8_t *in, uint8_t *out, size_t len)
{
   uint8_t ks[16];

#if defined(AES_HAVE_X86_PATH)
   if (aes_x86_available())
   {
      aes_x86_ctr(ctx, counter, in, out, len);
      return;
   }
#elif defined(AES_HAVE_ARM_PATH)
   if (aes_arm_available())
   {
      aes_arm_ctr(ctx, counter, in, out, len);
      return;
   }
#endif
   while (len)
   {
      size_t n = (len < 16) ? len : 16;
      size_t i;

      aes_encrypt_block(ctx, counter, ks);
      aes_ctr_inc32(counter);

      for (i = 0; i < n; i++)
         out[i] = in[i] ^ ks[i];

      in  += n;
      out += n;
      len -= n;
   }

   crypto_memzero(ks, sizeof(ks));
}

/* Double in GF(2^128) with the CMAC polynomial. */
static void aes_cmac_dbl(uint8_t *b)
{
   unsigned i;
   uint8_t carry = 0;
   uint8_t msb   = b[0] >> 7;

   for (i = 16; i-- > 0; )
   {
      uint8_t next = b[i] >> 7;
      b[i] = (uint8_t)((b[i] << 1) | carry);
      carry = next;
   }
   b[15] ^= (uint8_t)(0x87 & (0 - msb));
}

void aes_cmac(const struct aes_ctx *ctx,
      const uint8_t *msg, size_t len, uint8_t *mac)
{
   unsigned i;
   size_t  nblocks;
   uint8_t k1[16];
   uint8_t x[16];
   uint8_t last[16];

   memset(x, 0, 16);
   aes_encrypt_block(ctx, x, k1);
   aes_cmac_dbl(k1);

   nblocks = (len + 15) / 16;
   if (nblocks == 0)
      nblocks = 1;

   /* Every block but the last, straight through the chain. */
   while (nblocks > 1)
   {
      for (i = 0; i < 16; i++)
         x[i] ^= msg[i];
      aes_encrypt_block(ctx, x, x);
      msg     += 16;
      len     -= 16;
      nblocks--;
   }

   if (len == 16)
      memcpy(last, msg, 16);
   else
   {
      /* Partial or empty last block: pad 10* and use K2 = dbl(K1). */
      aes_cmac_dbl(k1);
      memset(last, 0, 16);
      memcpy(last, msg, len);
      last[len] = 0x80;
   }

   for (i = 0; i < 16; i++)
      x[i] ^= last[i] ^ k1[i];
   aes_encrypt_block(ctx, x, mac);

   crypto_memzero(k1,   sizeof(k1));
   crypto_memzero(x,    sizeof(x));
   crypto_memzero(last, sizeof(last));
}

/* ---- CCM (RFC 3610) ---------------------------------------------- */

/* CBC-MAC over B_0 || AAD (length-prefixed, padded) || payload
 * (padded), then CTR with A_i counters; the tag is the MAC under
 * counter zero. */
static int aes_ccm_crypt(const struct aes_ctx *ctx, int encrypt,
      const uint8_t *nonce, size_t nonce_len,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *in, size_t len, uint8_t *out,
      uint8_t *tag, size_t tag_len)
{
   uint8_t  x[16];
   uint8_t  ctr[16];
   uint8_t  s0[16];
   size_t   L, i, off;
   size_t   remaining;

   if (nonce_len < 7 || nonce_len > 13 || tag_len < 4 || tag_len > 16 || (tag_len & 1))
      return -1;
   L = 15 - nonce_len;
   if (L < 8 && (len >> (8 * L)) != 0)
      return -1;

   /* B_0: flags | nonce | length of the message */
   x[0] = (uint8_t)((aad_len ? 0x40 : 0) | (((tag_len - 2) / 2) << 3) | (L - 1));
   memcpy(x + 1, nonce, nonce_len);
   remaining = len;
   for (i = 0; i < L; i++)
   {
      x[15 - i]  = (uint8_t)remaining;
      remaining >>= 8;
   }
   aes_encrypt_block(ctx, x, x);

   /* AAD, length-prefixed (2 or 6 octets here; > 2^32 is not needed) */
   if (aad_len)
   {
      uint8_t blk[16];
      size_t  n;
      memset(blk, 0, 16);
      if (aad_len < 0xff00)
      {
         blk[0] = (uint8_t)(aad_len >> 8);
         blk[1] = (uint8_t)aad_len;
         off    = 2;
      }
      else
      {
         blk[0] = 0xff; blk[1] = 0xfe;
         crypto_store32_be(blk + 2, (uint32_t)aad_len);
         off = 6;
      }
      n = 16 - off;
      if (n > aad_len)
         n = aad_len;
      memcpy(blk + off, aad, n);
      for (i = 0; i < 16; i++)
         x[i] ^= blk[i];
      aes_encrypt_block(ctx, x, x);
      aad += n;
      aad_len -= n;
      while (aad_len)
      {
         n = aad_len < 16 ? aad_len : 16;
         for (i = 0; i < n; i++)
            x[i] ^= aad[i];
         aes_encrypt_block(ctx, x, x);
         aad += n;
         aad_len -= n;
      }
   }

   /* Counter blocks: flags = L - 1, nonce, counter */
   memset(ctr, 0, 16);
   ctr[0] = (uint8_t)(L - 1);
   memcpy(ctr + 1, nonce, nonce_len);
   aes_encrypt_block(ctx, ctr, s0);           /* S_0, for the tag */

   off = 0;
   while (off < len)
   {
      uint8_t ks[16];
      size_t  n = (len - off < 16) ? len - off : 16;
      size_t  c = off / 16 + 1;
      for (i = 0; i < L; i++)
      {
         ctr[15 - i] = (uint8_t)c;
         c >>= 8;
      }
      aes_encrypt_block(ctx, ctr, ks);
      if (encrypt)
      {
         for (i = 0; i < n; i++)
            x[i] ^= in[off + i];
         aes_encrypt_block(ctx, x, x);
         for (i = 0; i < n; i++)
            out[off + i] = in[off + i] ^ ks[i];
      }
      else
      {
         for (i = 0; i < n; i++)
         {
            out[off + i] = in[off + i] ^ ks[i];
            x[i]        ^= out[off + i];
         }
         aes_encrypt_block(ctx, x, x);
      }
      off += n;
   }

   for (i = 0; i < tag_len; i++)
      tag[i] = x[i] ^ s0[i];
   crypto_memzero(x, sizeof(x));
   crypto_memzero(s0, sizeof(s0));
   return 0;
}

int aes_ccm_encrypt(const struct aes_ctx *ctx,
      const uint8_t *nonce, size_t nonce_len,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *pt, size_t pt_len,
      uint8_t *ct, uint8_t *tag, size_t tag_len)
{
   return aes_ccm_crypt(ctx, 1, nonce, nonce_len, aad, aad_len,
         pt, pt_len, ct, tag, tag_len);
}

int aes_ccm_decrypt(const struct aes_ctx *ctx,
      const uint8_t *nonce, size_t nonce_len,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *ct, size_t ct_len,
      const uint8_t *tag, size_t tag_len, uint8_t *pt)
{
   uint8_t calc[16];
   if (aes_ccm_crypt(ctx, 0, nonce, nonce_len, aad, aad_len,
            ct, ct_len, pt, calc, tag_len) != 0)
      return -1;
   if (!crypto_memeq_ct(calc, tag, tag_len))
   {
      crypto_memzero(pt, ct_len);
      return -1;
   }
   return 0;
}

/* GHASH, scalar: 4-bit tables of H (16 entries, built at init) and a
 * 16-entry reduction table, two lookups per octet. */
static const uint64_t aes_gcm_last4[16] = {
   0x0000, 0x1c20, 0x3840, 0x2460, 0x7080, 0x6ca0, 0x48c0, 0x54e0,
   0xe100, 0xfd20, 0xd940, 0xc560, 0x9180, 0x8da0, 0xa9c0, 0xb5e0
};

static void aes_gcm_mul(const struct aes_gcm_ctx *ctx,
      uint64_t *y_hi, uint64_t *y_lo)
{
   uint8_t  x[16];
   uint64_t zh, zl, rem;
   int      i;
   unsigned lo, hi;

   crypto_store64_be(x,     *y_hi);
   crypto_store64_be(x + 8, *y_lo);

   lo = x[15] & 0x0f;
   zh = ctx->hh[lo];
   zl = ctx->hl[lo];

   for (i = 15; i >= 0; i--)
   {
      lo = x[i] & 0x0f;
      hi = (x[i] >> 4) & 0x0f;

      if (i != 15)
      {
         rem = zl & 0x0f;
         zl  = (zh << 60) | (zl >> 4);
         zh  = (zh >> 4);
         zh ^= aes_gcm_last4[rem] << 48;
         zh ^= ctx->hh[lo];
         zl ^= ctx->hl[lo];
      }
      rem = zl & 0x0f;
      zl  = (zh << 60) | (zl >> 4);
      zh  = (zh >> 4);
      zh ^= aes_gcm_last4[rem] << 48;
      zh ^= ctx->hh[hi];
      zl ^= ctx->hl[hi];
   }

   *y_hi = zh;
   *y_lo = zl;
}

static void aes_gcm_ghash_update(const struct aes_gcm_ctx *ctx,
      uint64_t *y_hi, uint64_t *y_lo, const uint8_t *data, size_t len)
{
#if defined(AES_HAVE_X86_PATH)
   if (aes_x86_available())
   {
      aes_x86_ghash(ctx, y_hi, y_lo, data, len);
      return;
   }
#elif defined(AES_HAVE_ARM_PATH)
   if (aes_arm_available())
   {
      aes_arm_ghash(ctx, y_hi, y_lo, data, len);
      return;
   }
#endif
   while (len)
   {
      uint8_t blk[16];
      size_t  n = (len < 16) ? len : 16;

      if (n < 16)
      {
         memset(blk, 0, 16);
         memcpy(blk, data, n);
         data = blk;
      }

      *y_hi ^= crypto_load64_be(data);
      *y_lo ^= crypto_load64_be(data + 8);
      aes_gcm_mul(ctx, y_hi, y_lo);

      data += n;
      len  -= n;
   }
}

static void aes_gcm_tables(struct aes_gcm_ctx *ctx)
{
   uint64_t vh = ctx->h_hi, vl = ctx->h_lo;
   int i, j;

   ctx->hh[0] = 0; ctx->hl[0] = 0;
   ctx->hh[8] = vh; ctx->hl[8] = vl;

   for (i = 4; i > 0; i >>= 1)
   {
      uint32_t t = (uint32_t)(vl & 1) * 0xe1000000u;
      vl = (vh << 63) | (vl >> 1);
      vh = (vh >> 1) ^ ((uint64_t)t << 32);
      ctx->hl[i] = vl;
      ctx->hh[i] = vh;
   }
   for (i = 2; i <= 8; i *= 2)
   {
      vh = ctx->hh[i];
      vl = ctx->hl[i];
      for (j = 1; j < i; j++)
      {
         ctx->hh[i + j] = vh ^ ctx->hh[j];
         ctx->hl[i + j] = vl ^ ctx->hl[j];
      }
   }
}

int aes_gcm_init(struct aes_gcm_ctx *ctx,
      const uint8_t *key, size_t key_len)
{
   uint8_t h[16];

   if (aes_init(&ctx->aes, key, key_len) != 0)
      return -1;

   memset(h, 0, 16);
   aes_encrypt_block(&ctx->aes, h, h);
   ctx->h_hi = crypto_load64_be(h);
   ctx->h_lo = crypto_load64_be(h + 8);
   aes_gcm_tables(ctx);
   crypto_memzero(h, sizeof(h));
   return 0;
}

static void aes_gcm_j0(const struct aes_gcm_ctx *ctx,
      const uint8_t *nonce, size_t nonce_len, uint8_t *j0)
{
   if (nonce_len == 12)
   {
      memcpy(j0, nonce, 12);
      j0[12] = 0; j0[13] = 0; j0[14] = 0; j0[15] = 1;
   }
   else
   {
      uint64_t y_hi = 0, y_lo = 0;
      aes_gcm_ghash_update(ctx, &y_hi, &y_lo, nonce, nonce_len);
      y_lo ^= (uint64_t)nonce_len * 8;
      aes_gcm_mul(ctx, &y_hi, &y_lo);
      crypto_store64_be(j0,     y_hi);
      crypto_store64_be(j0 + 8, y_lo);
   }
}

static void aes_gcm_tag(const struct aes_gcm_ctx *ctx, const uint8_t *j0,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *ct, size_t ct_len, uint8_t *tag)
{
   unsigned i;
   uint64_t y_hi = 0, y_lo = 0;
   uint8_t  ek[16];

   aes_gcm_ghash_update(ctx, &y_hi, &y_lo, aad, aad_len);
   aes_gcm_ghash_update(ctx, &y_hi, &y_lo, ct, ct_len);
   y_hi ^= (uint64_t)aad_len * 8;
   y_lo ^= (uint64_t)ct_len  * 8;
   aes_gcm_mul(ctx, &y_hi, &y_lo);

   aes_encrypt_block(&ctx->aes, j0, ek);
   crypto_store64_be(tag,     y_hi);
   crypto_store64_be(tag + 8, y_lo);
   for (i = 0; i < 16; i++)
      tag[i] ^= ek[i];
   crypto_memzero(ek, sizeof(ek));
}

int aes_gcm_encrypt(const struct aes_gcm_ctx *ctx,
      const uint8_t *nonce, size_t nonce_len,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *pt, size_t pt_len,
      uint8_t *ct, uint8_t *tag)
{
   uint8_t j0[16];
   uint8_t ctr[16];

   aes_gcm_j0(ctx, nonce, nonce_len, j0);
   memcpy(ctr, j0, 16);
   aes_ctr_inc32(ctr);
   aes_ctr_crypt(&ctx->aes, ctr, pt, ct, pt_len);
   aes_gcm_tag(ctx, j0, aad, aad_len, ct, pt_len, tag);
   return 0;
}

int aes_gcm_decrypt(const struct aes_gcm_ctx *ctx,
      const uint8_t *nonce, size_t nonce_len,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *ct, size_t ct_len,
      const uint8_t *tag, uint8_t *pt)
{
   uint8_t j0[16];
   uint8_t ctr[16];
   uint8_t calc[16];
   int ok;

   aes_gcm_j0(ctx, nonce, nonce_len, j0);
   aes_gcm_tag(ctx, j0, aad, aad_len, ct, ct_len, calc);
   ok = crypto_memeq_ct(calc, tag, 16);
   crypto_memzero(calc, sizeof(calc));
   if (!ok)
      return -1;

   memcpy(ctr, j0, 16);
   aes_ctr_inc32(ctr);
   aes_ctr_crypt(&ctx->aes, ctr, ct, pt, ct_len);
   return 0;
}

#define QR(a, b, c, d)                                  \
   a += b; d ^= a; d = crypto_rotl32(d, 16);           \
   c += d; b ^= c; b = crypto_rotl32(b, 12);           \
   a += b; d ^= a; d = crypto_rotl32(d,  8);           \
   c += d; b ^= c; b = crypto_rotl32(b,  7)

static void chacha20_core(const uint8_t *key, const uint8_t *nonce,
      uint32_t counter, uint32_t *out)
{
   unsigned i;
   uint32_t s[16];
   uint32_t x[16];

   s[0]  = 0x61707865; s[1]  = 0x3320646e;
   s[2]  = 0x79622d32; s[3]  = 0x6b206574;
   for (i = 0; i < 8; i++)
      s[4 + i] = crypto_load32_le(key + 4 * i);
   s[12] = counter;
   s[13] = crypto_load32_le(nonce);
   s[14] = crypto_load32_le(nonce + 4);
   s[15] = crypto_load32_le(nonce + 8);

   memcpy(x, s, sizeof(x));

   for (i = 0; i < 10; i++)
   {
      QR(x[0], x[4], x[ 8], x[12]);
      QR(x[1], x[5], x[ 9], x[13]);
      QR(x[2], x[6], x[10], x[14]);
      QR(x[3], x[7], x[11], x[15]);
      QR(x[0], x[5], x[10], x[15]);
      QR(x[1], x[6], x[11], x[12]);
      QR(x[2], x[7], x[ 8], x[13]);
      QR(x[3], x[4], x[ 9], x[14]);
   }

   for (i = 0; i < 16; i++)
      out[i] = x[i] + s[i];
}

void chacha20_block(const uint8_t *key, const uint8_t *nonce,
      uint32_t counter, uint8_t *out)
{
   unsigned i;
   uint32_t w[16];

   chacha20_core(key, nonce, counter, w);
   for (i = 0; i < 16; i++)
      crypto_store32_le(out + 4 * i, w[i]);
}

void chacha20_xor(const uint8_t *key, const uint8_t *nonce,
      uint32_t counter, const uint8_t *in, uint8_t *out, size_t len)
{
   uint32_t w[16];
   unsigned i;

   /* Whole blocks a word at a time, no keystream bounce buffer. */
   while (len >= 64)
   {
      chacha20_core(key, nonce, counter, w);
      counter++;
      for (i = 0; i < 16; i++)
         crypto_store32_le(out + 4 * i, crypto_load32_le(in + 4 * i) ^ w[i]);
      in  += 64;
      out += 64;
      len -= 64;
   }
   if (len)
   {
      uint8_t ks[64];
      chacha20_block(key, nonce, counter, ks);
      for (i = 0; i < len; i++)
         out[i] = in[i] ^ ks[i];
      crypto_memzero(ks, sizeof(ks));
   }
   crypto_memzero(w, sizeof(w));
}

#define P1305_MASK 0x3ffffff

void poly1305_init(struct poly1305_ctx *ctx, const uint8_t *key)
{
   uint32_t t0 = crypto_load32_le(key);
   uint32_t t1 = crypto_load32_le(key + 4);
   uint32_t t2 = crypto_load32_le(key + 8);
   uint32_t t3 = crypto_load32_le(key + 12);

   /* r &= 0xffffffc0ffffffc0ffffffc0fffffff, split into 26-bit limbs. */
   ctx->r[0] = t0 & 0x3ffffff; t0 >>= 26; t0 |= t1 << 6;
   ctx->r[1] = t0 & 0x3ffff03; t1 >>= 20; t1 |= t2 << 12;
   ctx->r[2] = t1 & 0x3ffc0ff; t2 >>= 14; t2 |= t3 << 18;
   ctx->r[3] = t2 & 0x3f03fff; t3 >>= 8;
   ctx->r[4] = t3 & 0x00fffff;

   ctx->h[0] = ctx->h[1] = ctx->h[2] = ctx->h[3] = ctx->h[4] = 0;

   ctx->pad[0] = crypto_load32_le(key + 16);
   ctx->pad[1] = crypto_load32_le(key + 20);
   ctx->pad[2] = crypto_load32_le(key + 24);
   ctx->pad[3] = crypto_load32_le(key + 28);

   ctx->leftover = 0;
   ctx->final    = 0;
}

static void poly1305_blocks(struct poly1305_ctx *ctx,
      const uint8_t *m, size_t len)
{
   const uint32_t hibit = ctx->final ? 0 : ((uint32_t)1 << 24);
   uint32_t r0 = ctx->r[0], r1 = ctx->r[1], r2 = ctx->r[2];
   uint32_t r3 = ctx->r[3], r4 = ctx->r[4];
   uint32_t s1 = r1 * 5, s2 = r2 * 5, s3 = r3 * 5, s4 = r4 * 5;
   uint32_t h0 = ctx->h[0], h1 = ctx->h[1], h2 = ctx->h[2];
   uint32_t h3 = ctx->h[3], h4 = ctx->h[4];

   while (len >= 16)
   {
      uint64_t d0, d1, d2, d3, d4;
      uint32_t c;
      uint32_t t0 = crypto_load32_le(m);
      uint32_t t1 = crypto_load32_le(m + 4);
      uint32_t t2 = crypto_load32_le(m + 8);
      uint32_t t3 = crypto_load32_le(m + 12);

      /* h += m[i] */
      h0 += t0 & P1305_MASK;
      h1 += ((t0 >> 26) | (t1 <<  6)) & P1305_MASK;
      h2 += ((t1 >> 20) | (t2 << 12)) & P1305_MASK;
      h3 += ((t2 >> 14) | (t3 << 18)) & P1305_MASK;
      h4 += (t3 >> 8) | hibit;

      /* h *= r, mod 2^130 - 5 */
      d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * s4 + (uint64_t)h2 * s3
         + (uint64_t)h3 * s2 + (uint64_t)h4 * s1;
      d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * s4
         + (uint64_t)h3 * s3 + (uint64_t)h4 * s2;
      d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0
         + (uint64_t)h3 * s4 + (uint64_t)h4 * s3;
      d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1
         + (uint64_t)h3 * r0 + (uint64_t)h4 * s4;
      d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2
         + (uint64_t)h3 * r1 + (uint64_t)h4 * r0;

      /* partial reduction */
                    c = (uint32_t)(d0 >> 26); h0 = (uint32_t)d0 & P1305_MASK;
      d1 += c;      c = (uint32_t)(d1 >> 26); h1 = (uint32_t)d1 & P1305_MASK;
      d2 += c;      c = (uint32_t)(d2 >> 26); h2 = (uint32_t)d2 & P1305_MASK;
      d3 += c;      c = (uint32_t)(d3 >> 26); h3 = (uint32_t)d3 & P1305_MASK;
      d4 += c;      c = (uint32_t)(d4 >> 26); h4 = (uint32_t)d4 & P1305_MASK;
      h0 += c * 5;  c = h0 >> 26;             h0 &= P1305_MASK;
      h1 += c;

      m   += 16;
      len -= 16;
   }

   ctx->h[0] = h0; ctx->h[1] = h1; ctx->h[2] = h2;
   ctx->h[3] = h3; ctx->h[4] = h4;
}

void poly1305_update(struct poly1305_ctx *ctx,
      const uint8_t *m, size_t len)
{
   size_t want;

   if (ctx->leftover)
   {
      want = 16 - ctx->leftover;
      if (want > len)
         want = len;
      memcpy(ctx->buffer + ctx->leftover, m, want);
      len           -= want;
      m             += want;
      ctx->leftover += want;
      if (ctx->leftover < 16)
         return;
      poly1305_blocks(ctx, ctx->buffer, 16);
      ctx->leftover = 0;
   }

   if (len >= 16)
   {
      want = len & ~(size_t)15;
      poly1305_blocks(ctx, m, want);
      m   += want;
      len -= want;
   }

   if (len)
   {
      memcpy(ctx->buffer, m, len);
      ctx->leftover = len;
   }
}

void poly1305_final(struct poly1305_ctx *ctx, uint8_t *tag)
{
   uint32_t h0, h1, h2, h3, h4, c;
   uint32_t g0, g1, g2, g3, g4;
   uint64_t f;
   uint32_t mask;

   if (ctx->leftover)
   {
      size_t i = ctx->leftover;
      ctx->buffer[i++] = 1;
      for (; i < 16; i++)
         ctx->buffer[i] = 0;
      ctx->final = 1;
      poly1305_blocks(ctx, ctx->buffer, 16);
   }

   h0 = ctx->h[0]; h1 = ctx->h[1]; h2 = ctx->h[2];
   h3 = ctx->h[3]; h4 = ctx->h[4];

   /* full carry */
                c = h1 >> 26; h1 &= P1305_MASK;
   h2 +=     c; c = h2 >> 26; h2 &= P1305_MASK;
   h3 +=     c; c = h3 >> 26; h3 &= P1305_MASK;
   h4 +=     c; c = h4 >> 26; h4 &= P1305_MASK;
   h0 += c * 5; c = h0 >> 26; h0 &= P1305_MASK;
   h1 +=     c;

   /* g = h + 5 - 2^130; select g when it did not go negative */
   g0 = h0 + 5; c = g0 >> 26; g0 &= P1305_MASK;
   g1 = h1 + c; c = g1 >> 26; g1 &= P1305_MASK;
   g2 = h2 + c; c = g2 >> 26; g2 &= P1305_MASK;
   g3 = h3 + c; c = g3 >> 26; g3 &= P1305_MASK;
   g4 = h4 + c - ((uint32_t)1 << 26);

   mask = (g4 >> 31) - 1;
   g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
   mask = ~mask;
   h0 = (h0 & mask) | g0;
   h1 = (h1 & mask) | g1;
   h2 = (h2 & mask) | g2;
   h3 = (h3 & mask) | g3;
   h4 = (h4 & mask) | g4;

   /* h = h % 2^128 as four 32-bit words */
   h0 = ((h0      ) | (h1 << 26));
   h1 = ((h1 >>  6) | (h2 << 20));
   h2 = ((h2 >> 12) | (h3 << 14));
   h3 = ((h3 >> 18) | (h4 <<  8));

   /* tag = (h + pad) % 2^128 */
   f = (uint64_t)h0 + ctx->pad[0];             h0 = (uint32_t)f;
   f = (uint64_t)h1 + ctx->pad[1] + (f >> 32); h1 = (uint32_t)f;
   f = (uint64_t)h2 + ctx->pad[2] + (f >> 32); h2 = (uint32_t)f;
   f = (uint64_t)h3 + ctx->pad[3] + (f >> 32); h3 = (uint32_t)f;

   crypto_store32_le(tag,      h0);
   crypto_store32_le(tag +  4, h1);
   crypto_store32_le(tag +  8, h2);
   crypto_store32_le(tag + 12, h3);

   crypto_memzero(ctx, sizeof(*ctx));
}

void poly1305_auth(uint8_t *tag, const uint8_t *m, size_t len,
      const uint8_t *key)
{
   struct poly1305_ctx ctx;
   poly1305_init(&ctx, key);
   poly1305_update(&ctx, m, len);
   poly1305_final(&ctx, tag);
}

static const uint8_t aead_zero[16] = { 0 };

/* Poly1305 over pad16(aad) || pad16(ct) || le64(aad_len) || le64(ct_len),
 * keyed with ChaCha20 block 0. */
static void aead_chacha_tag(const uint8_t *key, const uint8_t *nonce,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *ct, size_t ct_len, uint8_t *tag)
{
   struct poly1305_ctx p;
   uint8_t otk[64];
   uint8_t lens[16];

   chacha20_block(key, nonce, 0, otk);
   poly1305_init(&p, otk);
   poly1305_update(&p, aad, aad_len);
   if (aad_len & 15)
      poly1305_update(&p, aead_zero, 16 - (aad_len & 15));
   poly1305_update(&p, ct, ct_len);
   if (ct_len & 15)
      poly1305_update(&p, aead_zero, 16 - (ct_len & 15));
   crypto_store64_le(lens,     (uint64_t)aad_len);
   crypto_store64_le(lens + 8, (uint64_t)ct_len);
   poly1305_update(&p, lens, 16);
   poly1305_final(&p, tag);

   crypto_memzero(otk, sizeof(otk));
}

int aead_encrypt(enum aead_alg alg,
      const uint8_t *key, size_t key_len,
      const uint8_t *nonce, size_t nonce_len,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *pt, size_t pt_len,
      uint8_t *ct, uint8_t *tag, size_t tag_len)
{
   if (key_len != AEAD_KEY_SIZE || nonce_len != AEAD_NONCE_SIZE
         || tag_len != AEAD_TAG_SIZE)
      return -1;

   switch (alg)
   {
      case AEAD_CHACHA20_POLY1305:
         chacha20_xor(key, nonce, 1, pt, ct, pt_len);
         aead_chacha_tag(key, nonce, aad, aad_len, ct, pt_len, tag);
         return 0;
      case AEAD_AES256_GCM:
      {
         struct aes_gcm_ctx g;
         int ret = aes_gcm_init(&g, key, key_len);
         if (ret == 0)
            ret = aes_gcm_encrypt(&g, nonce, nonce_len, aad, aad_len,
                  pt, pt_len, ct, tag);
         crypto_memzero(&g, sizeof(g));
         return ret;
      }
      default:
         break;
   }
   return -1;
}

int aead_decrypt(enum aead_alg alg,
      const uint8_t *key, size_t key_len,
      const uint8_t *nonce, size_t nonce_len,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *ct, size_t ct_len,
      const uint8_t *tag, size_t tag_len,
      uint8_t *pt)
{
   if (key_len != AEAD_KEY_SIZE || nonce_len != AEAD_NONCE_SIZE
         || tag_len != AEAD_TAG_SIZE)
      return -1;

   switch (alg)
   {
      case AEAD_CHACHA20_POLY1305:
      {
         uint8_t calc[16];
         int ok;
         aead_chacha_tag(key, nonce, aad, aad_len, ct, ct_len, calc);
         ok = crypto_memeq_ct(calc, tag, 16);
         crypto_memzero(calc, sizeof(calc));
         if (!ok)
            return -2;
         chacha20_xor(key, nonce, 1, ct, pt, ct_len);
         return 0;
      }
      case AEAD_AES256_GCM:
      {
         struct aes_gcm_ctx g;
         int ret = aes_gcm_init(&g, key, key_len);
         if (ret == 0)
            ret = aes_gcm_decrypt(&g, nonce, nonce_len, aad, aad_len,
                  ct, ct_len, tag, pt) ? -2 : 0;
         crypto_memzero(&g, sizeof(g));
         return ret;
      }
      default:
         break;
   }
   return -1;
}


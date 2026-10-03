/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (lrc_hash.c).
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
#include <stdio.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#if defined(__SSE2__) || (defined(_MSC_VER) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)))
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <emmintrin.h>
#endif
#endif

#include <lrc_hash.h>
#include <retro_miscellaneous.h>
#include <retro_endianness.h>
#include <streams/file_stream.h>

/*
 * SHA-256 and SHA-1 block compression, in descending order of speed:
 *
 *   1. ARMv8 SHA1/SHA2 instructions.
 *   2. x86 SHA-NI.
 *   3. Portable scalar round loop, which every target keeps as the
 *      fallback for a CPU whose runtime probe comes back empty.
 *
 * The two instruction sets cover the same two digests, so a target
 * takes at most one of them and the choice is made here rather than
 * per digest.
 */

#if (defined(__aarch64__) || defined(_M_ARM64)) && !defined(_MSC_VER)
#if defined(__ARM_FEATURE_SHA2) || defined(__ARM_FEATURE_CRYPTO)
/* Baseline already has the instructions, so use them unconditionally. */
#define SHA_HAVE_ARM_PATH 1
#elif (defined(__clang__) && __clang_major__ >= 16) \
   || (!defined(__clang__) && defined(__GNUC__) && __GNUC__ >= 9)
/* Baseline does not, but the toolchain can build them into one function
 * without raising the ISA for the translation unit, so compile them in
 * and choose at runtime. Nothing in tree passes +crypto on aarch64, so
 * without this branch Android, Linux ARM, iOS and tvOS would all take
 * the scalar path on silicon that has the instructions.
 *
 * The version predicates are about the ACLE header rather than codegen,
 * as in encoding_crc32.c: target("+crypto") lets the compiler emit the
 * instructions, but vsha256hq_u32() and friends still have to be
 * declared, and both compilers gated them on the baseline for a long
 * time. */
#define SHA_HAVE_ARM_PATH    1
#define SHA_ARM_NEEDS_TARGET 1
#define SHA_ARM_DISPATCH     1
#endif
#elif (defined(__x86_64__) || defined(__i386__)) && !defined(_MSC_VER)
#if defined(__has_attribute) && defined(__has_include)
#if __has_attribute(target) && __has_include(<immintrin.h>) \
   && (!defined(__SCE__) \
      || (defined(__SHA__) && defined(__SSSE3__) && defined(__SSE4_1__)))
/* SHA-NI enumerates both digests through one CPUID bit, so one runtime
 * question answers for both.
 *
 * The last clause is about the header rather than codegen, as the ACLE
 * predicates in encoding_crc32.c are: immintrin.h pulls its per-feature
 * sub-headers in only when the baseline already has the feature on
 * targets that define __SCE__, and shaintrin.h refuses to be included
 * on its own, so a Sony toolchain built for a CPU without the
 * instructions leaves the intrinsics undeclared however the function
 * carrying them is attributed. */
#define SHA_HAVE_X86_PATH 1
#define SHA_X86_DISPATCH  1
#endif
#endif
#endif

#if defined(SHA_ARM_DISPATCH) || defined(SHA_X86_DISPATCH)
#include <features/features_cpu.h>
#endif

#if defined(SHA_HAVE_ARM_PATH)
#include <arm_neon.h>
#if defined(SHA_ARM_NEEDS_TARGET)
#define SHA_TARGET_ARM __attribute__((target("+crypto")))
#else
#define SHA_TARGET_ARM
#endif
#elif defined(SHA_HAVE_X86_PATH)
#include <immintrin.h>
#define SHA_TARGET_X86 __attribute__((target("sha,sse4.1")))
#endif

#define LSL32(x, n) ((uint32_t)(x) << (n))
#define LSR32(x, n) ((uint32_t)(x) >> (n))
#define ROR32(x, n) (LSR32(x, n) | LSL32(x, 32 - (n)))

/* First 32 bits of the fractional parts of the square roots of the first 8 primes 2..19 */
static const uint32_t T_H[8] = {
   0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
};

/* SHA-224 differs from SHA-256 only in this starting state and in
 * publishing seven of the eight words. FIPS 180-4, section 5.3.2. */
static const uint32_t T_H224[8] = {
   0xc1059ed8, 0x367cd507, 0x3070dd17, 0xf70e5939, 0xffc00b31, 0x68581511, 0x64f98fa7, 0xbefa4fa4,
};

/* First 32 bits of the fractional parts of the cube roots of the first 64 primes 2..311 */
static const uint32_t T_K[64] = {
   0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
   0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
   0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
   0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
   0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
   0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
   0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
   0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

/* SHA256 implementation from bSNES. Written by valditx. */


static void sha256_init(struct sha256_state *p)
{
   memset(p, 0, sizeof(struct sha256_state));
   memcpy(p->h, T_H, sizeof(T_H));
}


#if defined(SHA_HAVE_ARM_PATH)
SHA_TARGET_ARM
static void sha256_block_hw(uint32_t *h, const uint8_t *in)
{
   uint32x4_t m[4], s0, s1, s0_save, s1_save;
   unsigned i;

   s0      = vld1q_u32(h + 0);
   s1      = vld1q_u32(h + 4);
   s0_save = s0;
   s1_save = s1;

   for (i = 0; i < 4; i++)
      m[i] = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(in + 16 * i)));

   for (i = 0; i < 16; i++)
   {
      uint32x4_t w, t;

      if (i >= 4)
         m[i & 3] = vsha256su1q_u32(
               vsha256su0q_u32(m[i & 3], m[(i + 1) & 3]),
               m[(i + 2) & 3], m[(i + 3) & 3]);

      w  = vaddq_u32(m[i & 3], vld1q_u32(T_K + 4 * i));
      t  = s0;
      s0 = vsha256hq_u32(s0, s1, w);
      s1 = vsha256h2q_u32(s1, t, w);
   }

   vst1q_u32(h + 0, vaddq_u32(s0, s0_save));
   vst1q_u32(h + 4, vaddq_u32(s1, s1_save));
}
#elif defined(SHA_HAVE_X86_PATH)
SHA_TARGET_X86
static void sha256_block_hw(uint32_t *h, const uint8_t *in)
{
   /* Byte-reverse within each word: SHA256RNDS2 wants the message
    * words in memory order with big-endian bytes undone. */
   const __m128i mask = _mm_set_epi64x(
         (long long)0x0c0d0e0f08090a0bULL, (long long)0x0405060700010203ULL);
   __m128i m[4], s0, s1, tmp, s0_save, s1_save;
   unsigned i;

   /* The instruction operates on {A,B,E,F} and {C,D,G,H} rather than
    * on the digest in its natural order. */
   tmp = _mm_loadu_si128((const __m128i *)(const void *)(h + 0));
   s1  = _mm_loadu_si128((const __m128i *)(const void *)(h + 4));
   tmp = _mm_shuffle_epi32(tmp, 0xB1);
   s1  = _mm_shuffle_epi32(s1,  0x1B);
   s0  = _mm_alignr_epi8(tmp, s1, 8);
   s1  = _mm_blend_epi16(s1, tmp, 0xF0);

   s0_save = s0;
   s1_save = s1;

   for (i = 0; i < 4; i++)
      m[i] = _mm_shuffle_epi8(
            _mm_loadu_si128((const __m128i *)(const void *)(in + 16 * i)), mask);

   for (i = 0; i < 16; i++)
   {
      __m128i w, k;

      if (i >= 4)
      {
         w = _mm_sha256msg1_epu32(m[i & 3], m[(i + 1) & 3]);
         w = _mm_add_epi32(w,
               _mm_alignr_epi8(m[(i + 3) & 3], m[(i + 2) & 3], 4));
         m[i & 3] = _mm_sha256msg2_epu32(w, m[(i + 3) & 3]);
      }

      k  = _mm_loadu_si128((const __m128i *)(const void *)(T_K + 4 * i));
      w  = _mm_add_epi32(m[i & 3], k);
      s1 = _mm_sha256rnds2_epu32(s1, s0, w);
      w  = _mm_shuffle_epi32(w, 0x0E);
      s0 = _mm_sha256rnds2_epu32(s0, s1, w);
   }

   s0  = _mm_add_epi32(s0, s0_save);
   s1  = _mm_add_epi32(s1, s1_save);

   s0  = _mm_shuffle_epi32(s0, 0x1B);
   s1  = _mm_shuffle_epi32(s1, 0xB1);
   tmp = _mm_blend_epi16(s0, s1, 0xF0);
   s1  = _mm_alignr_epi8(s1, s0, 8);

   _mm_storeu_si128((__m128i *)(void *)(h + 0), tmp);
   _mm_storeu_si128((__m128i *)(void *)(h + 4), s1);
}
#endif

static void sha256_block_scalar(struct sha256_state *p)
{
   unsigned i;
   uint32_t s0, s1;
   uint32_t a, b, c, d, e, f, g, h;

   for (i = 0; i < 16; i++)
      p->w[i] = load32be(p->in.u32 + i);

   for (i = 16; i < 64; i++)
   {
      s0 = ROR32(p->w[i - 15],  7) ^ ROR32(p->w[i - 15], 18) ^ LSR32(p->w[i - 15],  3);
      s1 = ROR32(p->w[i -  2], 17) ^ ROR32(p->w[i -  2], 19) ^ LSR32(p->w[i -  2], 10);
      p->w[i] = p->w[i - 16] + s0 + p->w[i - 7] + s1;
   }

   a = p->h[0]; b = p->h[1]; c = p->h[2]; d = p->h[3];
   e = p->h[4]; f = p->h[5]; g = p->h[6]; h = p->h[7];

   for (i = 0; i < 64; i++)
   {
      uint32_t t1, t2, maj, ch;

      s0 = ROR32(a, 2) ^ ROR32(a, 13) ^ ROR32(a, 22);
      maj = (a & b) ^ (a & c) ^ (b & c);
      t2  = s0 + maj;
      s1  = ROR32(e, 6) ^ ROR32(e, 11) ^ ROR32(e, 25);
      ch  = (e & f) ^ (~e & g);
      t1  = h + s1 + ch + T_K[i] + p->w[i];

      h   = g;
      g   = f;
      f   = e;
      e   = d + t1;
      d   = c;
      c   = b;
      b   = a;
      a   = t1 + t2;
   }

   p->h[0] += a; p->h[1] += b; p->h[2] += c; p->h[3] += d;
   p->h[4] += e; p->h[5] += f; p->h[6] += g; p->h[7] += h;
}

static void sha256_block(struct sha256_state *p)
{
#if defined(SHA_ARM_DISPATCH) || defined(SHA_X86_DISPATCH)
   if (cpu_features_get() & RETRO_SIMD_SHA256)
      sha256_block_hw(p->h, p->in.u8);
   else
      sha256_block_scalar(p);
#elif defined(SHA_HAVE_ARM_PATH) || defined(SHA_HAVE_X86_PATH)
   sha256_block_hw(p->h, p->in.u8);
#else
   sha256_block_scalar(p);
#endif

   /* Next block */
   p->inlen = 0;
}

static void sha256_chunk(struct sha256_state *p,
      const uint8_t *s, size_t len)
{
   p->len += len;

   while (len)
   {
      size_t l   = 64 - p->inlen;

      if (len < l)
         l       = len;

      memcpy(p->in.u8 + p->inlen, s, l);

      s         += l;
      p->inlen  += l;
      len       -= l;

      if (p->inlen == 64)
         sha256_block(p);
   }
}

static void sha256_final(struct sha256_state *p)
{
   uint64_t len;
   p->in.u8[p->inlen++] = 0x80;

   if (p->inlen > 56)
   {
      memset(p->in.u8 + p->inlen, 0, 64 - p->inlen);
      sha256_block(p);
   }

   memset(p->in.u8 + p->inlen, 0, 56 - p->inlen);

   len = p->len << 3;
   store32be(p->in.u32 + 14, (uint32_t)(len >> 32));
   store32be(p->in.u32 + 15, (uint32_t)len);
   sha256_block(p);
}

static void sha256_subhash(struct sha256_state *p, uint32_t *t)
{
   unsigned i;
   for (i = 0; i < 8; i++)
      store32be(t++, p->h[i]);
}

/**
 * sha256_hash:
 * @s                 : Output.
 * @in                : Input.
 * @size              : Size of @s.
 *
 * Hashes SHA256 and outputs a human readable string.
 **/
void sha256_stream_init(struct sha256_state *p, unsigned is224)
{
   memset(p, 0, sizeof(struct sha256_state));
   memcpy(p->h, is224 ? T_H224 : T_H, sizeof(T_H));
   p->is224 = is224 ? 1 : 0;
}

void sha256_stream_update(struct sha256_state *p,
      const uint8_t *data, size_t len)
{
   if (!len)
      return;
   sha256_chunk(p, data, len);
}

void sha256_stream_final(struct sha256_state *p, uint8_t *digest)
{
   unsigned i;
   unsigned words = p->is224 ? 7 : 8;

   sha256_final(p);

   for (i = 0; i < words; i++)
   {
      digest[4 * i    ] = (uint8_t)(p->h[i] >> 24);
      digest[4 * i + 1] = (uint8_t)(p->h[i] >> 16);
      digest[4 * i + 2] = (uint8_t)(p->h[i] >>  8);
      digest[4 * i + 3] = (uint8_t) p->h[i];
   }
}

void sha256_stream_block(struct sha256_state *p, const uint8_t *data)
{
   memcpy(p->in.u8, data, 64);
   p->inlen = 64;
   sha256_block(p);
}

void sha256_hash(char *s, const uint8_t *in, size_t len)
{
   unsigned i;
   struct sha256_state sha;

   union
   {
      uint32_t u32[8];
      uint8_t u8[32];
   } shahash;

   sha256_init(&sha);
   sha256_chunk(&sha, in, len);
   sha256_final(&sha);
   sha256_subhash(&sha, shahash.u32);

   /* lowercase hex, one byte at a time; no stdio in the hot path and
    * nothing the strict C89 lane rejects */
   for (i = 0; i < 32; i++)
   {
      static const char hex[] = "0123456789abcdef";
      s[2 * i]     = hex[shahash.u8[i] >> 4];
      s[2 * i + 1] = hex[shahash.u8[i] & 0x0f];
   }
   s[64] = '\0';
}

/* SHA-1 implementation. */

/* Block size for the copying branch of sha1_calculate(). Heap rather
 * than stack, so it is bounded by what a read is worth rather than by
 * the frame budget, and the same 256 KB intfstream_crc_step() reads
 * for the same reason: both walk a whole scanned file. */
#define SHA1_FILE_BLOCK_SIZE (256 * 1024)

/*
 *  sha1.c
 *
 *  Copyright (C) 1998, 2009
 *  Paul E. Jones <paulej@packetizer.com>
 *  All Rights Reserved
 *
 *****************************************************************************
 *  $Id: sha1.c 12 2009-06-22 19:34:25Z paulej $
 *****************************************************************************
 *
 *  Description:
 *      This file implements the Secure Hashing Standard as defined
 *      in FIPS PUB 180-1 published April 17, 1995.
 *
 *      The Secure Hashing Standard, which uses the Secure Hashing
 *      Algorithm (SHA), produces a 160-bit message digest for a
 *      given data stream.  In theory, it is highly improbable that
 *      two messages will produce the same message digest.  Therefore,
 *      this algorithm can serve as a means of providing a "fingerprint"
 *      for a message.
 *
 *  Portability Issues:
 *      SHA-1 is defined in terms of 32-bit "words".  This code was
 *      written with the expectation that the processor has at least
 *      a 32-bit machine word size.  If the machine word size is larger,
 *      the code should still function properly.  One caveat to that
 *      is that the input functions taking characters and character
 *      arrays assume that only 8 bits of information are stored in each
 *      character.
 *
 *  Caveats:
 *      SHA-1 is designed to work with messages less than 2^64 bits
 *      long. Although SHA-1 allows a message digest to be generated for
 *      messages of any number of bits less than 2^64, this
 *      implementation only works with messages with a length that is a
 *      multiple of the size of an 8-bit character.
 *
 */

/* Define the circular shift macro */
#define SHA1CircularShift(bits,word) ((((word) << (bits)) & 0xFFFFFFFF) | ((word) >> (32-(bits))))



static void SHA1Reset(struct sha1_state *context)
{
   if (!context)
      return;

   context->length_low             = 0;
   context->length_high            = 0;
   context->block_index    = 0;

   context->digest[0]      = 0x67452301;
   context->digest[1]      = 0xEFCDAB89;
   context->digest[2]      = 0x98BADCFE;
   context->digest[3]      = 0x10325476;
   context->digest[4]      = 0xC3D2E1F0;

   context->computed   = 0;
   context->corrupted  = 0;
}

#if defined(SHA_HAVE_ARM_PATH)
SHA_TARGET_ARM
static void sha1_block_hw(uint32_t *state, const uint8_t *in)
{
   static const uint32_t hw_k[4] =
   {
      0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6
   };
   uint32x4_t m[4], abcd, abcd_save;
   uint32_t   e, e_save, e1;
   unsigned   i;

   abcd      = vld1q_u32(state);
   e         = state[4];
   abcd_save = abcd;
   e_save    = e;

   for (i = 0; i < 4; i++)
      m[i] = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(in + 16 * i)));

   for (i = 0; i < 20; i++)
   {
      uint32x4_t w;

      if (i >= 4)
         m[i & 3] = vsha1su1q_u32(
               vsha1su0q_u32(m[i & 3], m[(i + 1) & 3], m[(i + 2) & 3]),
               m[(i + 3) & 3]);

      w  = vaddq_u32(m[i & 3], vdupq_n_u32(hw_k[i / 5]));
      /* Taken from the current A, so it has to be read before the
       * round overwrites it. */
      e1 = vsha1h_u32(vgetq_lane_u32(abcd, 0));

      if (i < 5)
         abcd = vsha1cq_u32(abcd, e, w);
      else if (i < 10 || i >= 15)
         abcd = vsha1pq_u32(abcd, e, w);
      else
         abcd = vsha1mq_u32(abcd, e, w);

      e = e1;
   }

   vst1q_u32(state, vaddq_u32(abcd, abcd_save));
   state[4] = e + e_save;
}
#elif defined(SHA_HAVE_X86_PATH)
/* SHA1RNDS4 takes the round function as an immediate, so the twenty
 * groups are written out rather than looped. */
#define SHA1_X86_GROUP(f, idx) \
   do { \
      __m128i _w, _t; \
      if ((idx) >= 4) \
      { \
         _w = _mm_sha1msg1_epu32(m[(idx) & 3], m[((idx) + 1) & 3]); \
         _w = _mm_xor_si128(_w, m[((idx) + 2) & 3]); \
         m[(idx) & 3] = _mm_sha1msg2_epu32(_w, m[((idx) + 3) & 3]); \
      } \
      if ((idx) == 0) \
         ea = _mm_add_epi32(ea, m[0]); \
      else \
         ea = _mm_sha1nexte_epu32(ea, m[(idx) & 3]); \
      eb   = abcd; \
      abcd = _mm_sha1rnds4_epu32(abcd, ea, (f)); \
      _t   = ea; ea = eb; eb = _t; \
   } while (0)

SHA_TARGET_X86
static void sha1_block_hw(uint32_t *state, const uint8_t *in)
{
   /* A full 128-bit lane reverse rather than the per-word swap SHA-256
    * uses: SHA1RNDS4 consumes the message words in the opposite order
    * within the vector. */
   const __m128i mask = _mm_set_epi64x(
         (long long)0x0001020304050607ULL, (long long)0x08090a0b0c0d0e0fULL);
   __m128i m[4], abcd, ea, eb, abcd_save, e_save;
   unsigned i;

   abcd = _mm_shuffle_epi32(
         _mm_loadu_si128((const __m128i *)(const void *)state), 0x1B);
   ea   = _mm_set_epi32((int)state[4], 0, 0, 0);

   abcd_save = abcd;
   e_save    = ea;

   for (i = 0; i < 4; i++)
      m[i] = _mm_shuffle_epi8(
            _mm_loadu_si128((const __m128i *)(const void *)(in + 16 * i)), mask);

   SHA1_X86_GROUP(0,  0); SHA1_X86_GROUP(0,  1);
   SHA1_X86_GROUP(0,  2); SHA1_X86_GROUP(0,  3);
   SHA1_X86_GROUP(0,  4); SHA1_X86_GROUP(1,  5);
   SHA1_X86_GROUP(1,  6); SHA1_X86_GROUP(1,  7);
   SHA1_X86_GROUP(1,  8); SHA1_X86_GROUP(1,  9);
   SHA1_X86_GROUP(2, 10); SHA1_X86_GROUP(2, 11);
   SHA1_X86_GROUP(2, 12); SHA1_X86_GROUP(2, 13);
   SHA1_X86_GROUP(2, 14); SHA1_X86_GROUP(3, 15);
   SHA1_X86_GROUP(3, 16); SHA1_X86_GROUP(3, 17);
   SHA1_X86_GROUP(3, 18); SHA1_X86_GROUP(3, 19);

   ea   = _mm_sha1nexte_epu32(ea, e_save);
   abcd = _mm_add_epi32(abcd, abcd_save);

   _mm_storeu_si128((__m128i *)(void *)state,
         _mm_shuffle_epi32(abcd, 0x1B));
   state[4] = (uint32_t)_mm_extract_epi32(ea, 3);
}
#endif

static void SHA1ProcessMessageBlockScalar(struct sha1_state *context)
{
   const unsigned K[] =            /* Constants defined in SHA-1   */
   {
      0x5A827999,
      0x6ED9EBA1,
      0x8F1BBCDC,
      0xCA62C1D6
   };
   int         t;                  /* Loop counter                 */
   unsigned    temp;               /* Temporary word value         */
   unsigned    W[80];              /* Word sequence                */
   unsigned    A, B, C, D, E;      /* Word buffers                 */

   /* Initialize the first 16 words in the array W */
   for (t = 0; t < 16; t++)
   {
      W[t] = ((unsigned) context->block[t * 4]) << 24;
      W[t] |= ((unsigned) context->block[t * 4 + 1]) << 16;
      W[t] |= ((unsigned) context->block[t * 4 + 2]) << 8;
      W[t] |= ((unsigned) context->block[t * 4 + 3]);
   }

   for (t = 16; t < 80; t++)
      W[t] = SHA1CircularShift(1,W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);

   A = context->digest[0];
   B = context->digest[1];
   C = context->digest[2];
   D = context->digest[3];
   E = context->digest[4];

   for (t = 0; t < 20; t++)
   {
      temp  =  SHA1CircularShift(5,A) +
         ((B & C) | ((~B) & D)) + E + W[t] + K[0];
      temp &= 0xFFFFFFFF;
      E     = D;
      D     = C;
      C     = SHA1CircularShift(30,B);
      B     = A;
      A     = temp;
   }

   for (t = 20; t < 40; t++)
   {
      temp  = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[1];
      temp &= 0xFFFFFFFF;
      E     = D;
      D     = C;
      C     = SHA1CircularShift(30,B);
      B     = A;
      A     = temp;
   }

   for (t = 40; t < 60; t++)
   {
      temp  = SHA1CircularShift(5,A) +
         ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
      temp &= 0xFFFFFFFF;
      E     = D;
      D     = C;
      C     = SHA1CircularShift(30,B);
      B     = A;
      A     = temp;
   }

   for (t = 60; t < 80; t++)
   {
      temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[3];
      temp &= 0xFFFFFFFF;
      E = D;
      D = C;
      C = SHA1CircularShift(30,B);
      B = A;
      A = temp;
   }

   context->digest[0] =
      (context->digest[0] + A) & 0xFFFFFFFF;
   context->digest[1] =
      (context->digest[1] + B) & 0xFFFFFFFF;
   context->digest[2] =
      (context->digest[2] + C) & 0xFFFFFFFF;
   context->digest[3] =
      (context->digest[3] + D) & 0xFFFFFFFF;
   context->digest[4] =
      (context->digest[4] + E) & 0xFFFFFFFF;
}

static void SHA1ProcessMessageBlock(struct sha1_state *context)
{
#if defined(SHA_ARM_DISPATCH) || defined(SHA_X86_DISPATCH)
   if (cpu_features_get() & RETRO_SIMD_SHA1)
      sha1_block_hw(context->digest, context->block);
   else
      SHA1ProcessMessageBlockScalar(context);
#elif defined(SHA_HAVE_ARM_PATH) || defined(SHA_HAVE_X86_PATH)
   sha1_block_hw(context->digest, context->block);
#else
   SHA1ProcessMessageBlockScalar(context);
#endif

   context->block_index = 0;
}

static void SHA1PadMessage(struct sha1_state *context)
{
   if (!context)
      return;

   /*
    *  Check to see if the current message block is too small to hold
    *  the initial padding bits and length.  If so, we will pad the
    *  block, process it, and then continue padding into a second
    *  block.
    */
   context->block[context->block_index++] = 0x80;

   /* The index has already advanced past the 0x80, so the block is out
    * of room only above 56: at exactly 56 the eight length octets fill
    * 56..63 and no second block is needed. */
   if (context->block_index > 56)
   {
      while (context->block_index < 64)
         context->block[context->block_index++] = 0;

      SHA1ProcessMessageBlock(context);
   }

   while (context->block_index < 56)
      context->block[context->block_index++] = 0;

   /*  Store the message length as the last 8 octets */
   context->block[56] = (context->length_high >> 24) & 0xFF;
   context->block[57] = (context->length_high >> 16) & 0xFF;
   context->block[58] = (context->length_high >> 8) & 0xFF;
   context->block[59] = (context->length_high) & 0xFF;
   context->block[60] = (context->length_low >> 24) & 0xFF;
   context->block[61] = (context->length_low >> 16) & 0xFF;
   context->block[62] = (context->length_low >> 8) & 0xFF;
   context->block[63] = (context->length_low) & 0xFF;

   SHA1ProcessMessageBlock(context);
}

static int SHA1Result(struct sha1_state *context, unsigned char digest[20])
{
   unsigned i;

   if (context->corrupted)
      return 0;

   if (!context->computed)
   {
      SHA1PadMessage(context);
      context->computed = 1;
   }

   if (digest)
   {
      /* Convert digest to byte array */
      for (i = 0; i < 20; i++)
      {
         digest[i] = (unsigned char)
            ((context->digest[i>>2] >> 8 * (3 - (i & 0x03))) & 0xFF);
      }
   }

   return 1;
}

static void SHA1Input(struct sha1_state *context,
      const unsigned char *message_array,
      unsigned len)
{
   uint64_t total;
   uint64_t next;

   if (!len)
      return;

   if (context->computed || context->corrupted)
   {
      context->corrupted = 1;
      return;
   }

   /* The bit count is carried as two 32-bit halves, so advance it once
    * for the whole call. Wrapping past 2^64 bits is the length limit
    * the algorithm is defined up to. */
   total = ((uint64_t)context->length_high << 32) | context->length_low;
   next  = total + ((uint64_t)len << 3);

   if (next < total)
   {
      context->corrupted = 1;
      return;
   }

   context->length_high = (uint32_t)(next >> 32);
   context->length_low  = (uint32_t)next;

   while (len)
   {
      unsigned l = 64 - (unsigned)context->block_index;

      if (len < l)
         l = len;

      memcpy(context->block + context->block_index,
            message_array, l);

      message_array                += l;
      context->block_index += (int)l;
      len                          -= l;

      if (context->block_index == 64)
         SHA1ProcessMessageBlock(context);
   }
}

void sha1_stream_init(struct sha1_state *p)
{
   SHA1Reset(p);
}

void sha1_stream_update(struct sha1_state *p,
      const uint8_t *data, size_t len)
{
   /* SHA1Input() counts in a 32-bit pair, so hand it the data in
    * pieces small enough that one call cannot overflow the count. */
   while (len)
   {
      unsigned l = (len > 0x20000000u) ? 0x20000000u : (unsigned)len;
      SHA1Input(p, data, l);
      data += l;
      len  -= l;
   }
}

bool sha1_stream_final(struct sha1_state *p, uint8_t *digest)
{
   return SHA1Result(p, digest) ? true : false;
}

void sha1_stream_block(struct sha1_state *p, const uint8_t *data)
{
   memcpy(p->block, data, 64);
   p->block_index = 64;
   SHA1ProcessMessageBlock(p);
}

void SHA1Digest(const uint8_t* data, size_t len, uint8_t digest[20])
{
   struct sha1_state sha;

   SHA1Reset(&sha);
   SHA1Input(&sha, data, len);

   if (!SHA1Result(&sha, digest))
      memset(digest, 0, 20);
}

int sha1_calculate(const char *path, char *result)
{
   struct sha1_state sha;
   const uint8_t *map = NULL;
   int64_t        map_len = 0;
   unsigned char *buff = NULL;
   int rv    = 1;
   /* Ask for a mapping: where the platform provides one the whole
    * file is already addressable and there is nothing to copy. */
   RFILE *fd = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS);

   if (!fd)
      goto error;

   SHA1Reset(&sha);

   if ((map = filestream_get_mapped_ptr(fd, &map_len)) && map_len >= 0)
      SHA1Input(&sha, map, (unsigned)map_len);
   else
   {
      /* No mapping, so copy a block at a time.  The buffer is heap
       * rather than a local: as unsigned char buff[4096] this was a
       * 4304-byte frame, over half of the 8 KiB a GEKKO thread gets
       * (the GEKKO STACKSIZE in rthreads.c), and hashing a whole
       * file dwarfs one allocation either way.
       *
       * A block far larger than a page keeps the number of reads down,
       * which is what this path costs on a VFS where each one is a
       * round trip rather than a memcpy out of the page cache. */
      if (!(buff = (unsigned char*)malloc(SHA1_FILE_BLOCK_SIZE)))
         goto error;

      do
      {
         rv = (int)filestream_read(fd, buff, SHA1_FILE_BLOCK_SIZE);
         if (rv < 0)
            goto error;

         SHA1Input(&sha, buff, rv);
      } while (rv);

      free(buff);
      buff = NULL;
   }

   if (!SHA1Result(&sha, NULL))
      goto error;

   sprintf(result, "%08X%08X%08X%08X%08X",
         sha.digest[0],
         sha.digest[1],
         sha.digest[2],
         sha.digest[3], sha.digest[4]);

   filestream_close(fd);
   return 0;

error:
   free(buff);
   if (fd)
      filestream_close(fd);
   return -1;
}

#if defined(__SSE2__) || (defined(_MSC_VER) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)))

#if _MSC_VER
#define DJB2_ALIGN(x) __declspec(align(x))
#else
#define DJB2_ALIGN(x) __attribute__((aligned(x)))
#endif

static const DJB2_ALIGN(16) uint32_t DJB2_W8[8] = {
   0xEC41D4E1, /* 33^7 */
   0x4CFA3CC1, /* 33^6 */
   0x025528A1, /* 33^5 */
   0x00121881, /* 33^4 */
   0x00008C61, /* 33^3 */
   0x00000441, /* 33^2 */
   0x00000021, /* 33^1 */
   0x00000001, /* 33^0 */
};
#endif

uint32_t djb2_calculate(const char *str)
{
   uint32_t h = 5381;
   const unsigned char *p = (const unsigned char*)str;
#if defined(__SSE2__) || (defined(_MSC_VER) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)))
   __m128i w_lo = _mm_load_si128((const __m128i *)&DJB2_W8[0]);
   __m128i w_hi = _mm_load_si128((const __m128i *)&DJB2_W8[4]);
   size_t len   = strlen((const char*)p);
   const unsigned char *end8 = p + (len & ~(size_t)7);

   while (p < end8)
   {
      uint32_t sum = 0;
      __m128i raw  = _mm_loadl_epi64((const __m128i *)p);
      __m128i zero = _mm_setzero_si128();
      __m128i b16  = _mm_unpacklo_epi8(raw, zero);
      __m128i b_lo = _mm_unpacklo_epi16(b16, zero);
      __m128i b_hi = _mm_unpackhi_epi16(b16, zero);

     /* _mm_mul_epu32 multiplies lanes 0,2 → 64-bit results.
      * Shuffle to access lanes 1,3. */
      __m128i p02_lo = _mm_mul_epu32(b_lo, w_lo);
      __m128i p13_lo = _mm_mul_epu32(_mm_shuffle_epi32(b_lo, 0xF5),
                                     _mm_shuffle_epi32(w_lo, 0xF5));
      __m128i p02_hi = _mm_mul_epu32(b_hi, w_hi);
      __m128i p13_hi = _mm_mul_epu32(_mm_shuffle_epi32(b_hi, 0xF5),
                                     _mm_shuffle_epi32(w_hi, 0xF5));
      sum += (uint32_t)_mm_cvtsi128_si32(p02_lo);
      sum += (uint32_t)_mm_cvtsi128_si32(_mm_srli_si128(p02_lo, 8));
      sum += (uint32_t)_mm_cvtsi128_si32(p13_lo);
      sum += (uint32_t)_mm_cvtsi128_si32(_mm_srli_si128(p13_lo, 8));
      sum += (uint32_t)_mm_cvtsi128_si32(p02_hi);
      sum += (uint32_t)_mm_cvtsi128_si32(_mm_srli_si128(p02_hi, 8));
      sum += (uint32_t)_mm_cvtsi128_si32(p13_hi);
      sum += (uint32_t)_mm_cvtsi128_si32(_mm_srli_si128(p13_hi, 8));

      h    = h * UINT32_C(0x747C7101) + sum;
      p   += 8;
    }
#endif
    while (*p)
        h = (h << 5) + h + *p++;
    return h;
}

/* SHA-512 / SHA-384 */

static uint64_t sha512_load64(const uint8_t *p)
{
   unsigned i;
   uint64_t v = 0;
   for (i = 0; i < 8; i++)
      v = (v << 8) | p[i];
   return v;
}

static void sha512_store64(uint8_t *p, uint64_t v)
{
   unsigned i;
   for (i = 8; i-- > 0; )
   {
      p[i] = (uint8_t)v;
      v  >>= 8;
   }
}

#define U64(hi, lo) (((uint64_t)(hi) << 32) | (uint32_t)(lo))

static const uint64_t sha512_k[80] = {
   U64(0x428a2f98,0xd728ae22), U64(0x71374491,0x23ef65cd), U64(0xb5c0fbcf,0xec4d3b2f), U64(0xe9b5dba5,0x8189dbbc),
   U64(0x3956c25b,0xf348b538), U64(0x59f111f1,0xb605d019), U64(0x923f82a4,0xaf194f9b), U64(0xab1c5ed5,0xda6d8118),
   U64(0xd807aa98,0xa3030242), U64(0x12835b01,0x45706fbe), U64(0x243185be,0x4ee4b28c), U64(0x550c7dc3,0xd5ffb4e2),
   U64(0x72be5d74,0xf27b896f), U64(0x80deb1fe,0x3b1696b1), U64(0x9bdc06a7,0x25c71235), U64(0xc19bf174,0xcf692694),
   U64(0xe49b69c1,0x9ef14ad2), U64(0xefbe4786,0x384f25e3), U64(0x0fc19dc6,0x8b8cd5b5), U64(0x240ca1cc,0x77ac9c65),
   U64(0x2de92c6f,0x592b0275), U64(0x4a7484aa,0x6ea6e483), U64(0x5cb0a9dc,0xbd41fbd4), U64(0x76f988da,0x831153b5),
   U64(0x983e5152,0xee66dfab), U64(0xa831c66d,0x2db43210), U64(0xb00327c8,0x98fb213f), U64(0xbf597fc7,0xbeef0ee4),
   U64(0xc6e00bf3,0x3da88fc2), U64(0xd5a79147,0x930aa725), U64(0x06ca6351,0xe003826f), U64(0x14292967,0x0a0e6e70),
   U64(0x27b70a85,0x46d22ffc), U64(0x2e1b2138,0x5c26c926), U64(0x4d2c6dfc,0x5ac42aed), U64(0x53380d13,0x9d95b3df),
   U64(0x650a7354,0x8baf63de), U64(0x766a0abb,0x3c77b2a8), U64(0x81c2c92e,0x47edaee6), U64(0x92722c85,0x1482353b),
   U64(0xa2bfe8a1,0x4cf10364), U64(0xa81a664b,0xbc423001), U64(0xc24b8b70,0xd0f89791), U64(0xc76c51a3,0x0654be30),
   U64(0xd192e819,0xd6ef5218), U64(0xd6990624,0x5565a910), U64(0xf40e3585,0x5771202a), U64(0x106aa070,0x32bbd1b8),
   U64(0x19a4c116,0xb8d2d0c8), U64(0x1e376c08,0x5141ab53), U64(0x2748774c,0xdf8eeb99), U64(0x34b0bcb5,0xe19b48a8),
   U64(0x391c0cb3,0xc5c95a63), U64(0x4ed8aa4a,0xe3418acb), U64(0x5b9cca4f,0x7763e373), U64(0x682e6ff3,0xd6b2b8a3),
   U64(0x748f82ee,0x5defb2fc), U64(0x78a5636f,0x43172f60), U64(0x84c87814,0xa1f0ab72), U64(0x8cc70208,0x1a6439ec),
   U64(0x90befffa,0x23631e28), U64(0xa4506ceb,0xde82bde9), U64(0xbef9a3f7,0xb2c67915), U64(0xc67178f2,0xe372532b),
   U64(0xca273ece,0xea26619c), U64(0xd186b8c7,0x21c0c207), U64(0xeada7dd6,0xcde0eb1e), U64(0xf57d4f7f,0xee6ed178),
   U64(0x06f067aa,0x72176fba), U64(0x0a637dc5,0xa2c898a6), U64(0x113f9804,0xbef90dae), U64(0x1b710b35,0x131c471b),
   U64(0x28db77f5,0x23047d84), U64(0x32caab7b,0x40c72493), U64(0x3c9ebe0a,0x15c9bebc), U64(0x431d67c4,0x9c100d4c),
   U64(0x4cc5d4be,0xcb3e42b6), U64(0x597f299c,0xfc657e2a), U64(0x5fcb6fab,0x3ad6faec), U64(0x6c44198c,0x4a475817)
};

#define ROTR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))
#define CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA512_BSIG0(x) (ROTR64(x, 28) ^ ROTR64(x, 34) ^ ROTR64(x, 39))
#define SHA512_BSIG1(x) (ROTR64(x, 14) ^ ROTR64(x, 18) ^ ROTR64(x, 41))
#define SHA512_SSIG0(x) (ROTR64(x,  1) ^ ROTR64(x,  8) ^ ((x) >> 7))
#define SHA512_SSIG1(x) (ROTR64(x, 19) ^ ROTR64(x, 61) ^ ((x) >> 6))

static void sha512_block(struct sha512_state *p, const uint8_t *d)
{
   uint64_t w[80];
   uint64_t a, b, c, dd, e, f, g, h;
   unsigned i;

   for (i = 0; i < 16; i++)
      w[i] = sha512_load64(d + 8 * i);
   for (i = 16; i < 80; i++)
      w[i] = SHA512_SSIG1(w[i - 2]) + w[i - 7] + SHA512_SSIG0(w[i - 15]) + w[i - 16];

   a = p->h[0]; b = p->h[1]; c = p->h[2]; dd = p->h[3];
   e = p->h[4]; f = p->h[5]; g = p->h[6]; h  = p->h[7];

#define SHA512_STEP(a, b, c, d, e, f, g, h, i)                         \
   do {                                                                \
      uint64_t t1 = h + SHA512_BSIG1(e) + CH(e, f, g) + sha512_k[i] + w[i];   \
      uint64_t t2 = SHA512_BSIG0(a) + MAJ(a, b, c);                            \
      d += t1;                                                         \
      h  = t1 + t2;                                                    \
   } while (0)

   /* Eight rounds per trip with the registers renamed in place of
    * rotating them. */
   for (i = 0; i < 80; i += 8)
   {
      SHA512_STEP(a, b, c, dd, e, f, g, h, i);
      SHA512_STEP(h, a, b, c, dd, e, f, g, i + 1);
      SHA512_STEP(g, h, a, b, c, dd, e, f, i + 2);
      SHA512_STEP(f, g, h, a, b, c, dd, e, i + 3);
      SHA512_STEP(e, f, g, h, a, b, c, dd, i + 4);
      SHA512_STEP(dd, e, f, g, h, a, b, c, i + 5);
      SHA512_STEP(c, dd, e, f, g, h, a, b, i + 6);
      SHA512_STEP(b, c, dd, e, f, g, h, a, i + 7);
   }
#undef SHA512_STEP

   p->h[0] += a; p->h[1] += b; p->h[2] += c; p->h[3] += dd;
   p->h[4] += e; p->h[5] += f; p->h[6] += g; p->h[7] += h;
}

void sha512_stream_init(struct sha512_state *p, unsigned is384)
{
   if (is384)
   {
      p->h[0] = U64(0xcbbb9d5d,0xc1059ed8); p->h[1] = U64(0x629a292a,0x367cd507);
      p->h[2] = U64(0x9159015a,0x3070dd17); p->h[3] = U64(0x152fecd8,0xf70e5939);
      p->h[4] = U64(0x67332667,0xffc00b31); p->h[5] = U64(0x8eb44a87,0x68581511);
      p->h[6] = U64(0xdb0c2e0d,0x64f98fa7); p->h[7] = U64(0x47b5481d,0xbefa4fa4);
   }
   else
   {
      p->h[0] = U64(0x6a09e667,0xf3bcc908); p->h[1] = U64(0xbb67ae85,0x84caa73b);
      p->h[2] = U64(0x3c6ef372,0xfe94f82b); p->h[3] = U64(0xa54ff53a,0x5f1d36f1);
      p->h[4] = U64(0x510e527f,0xade682d1); p->h[5] = U64(0x9b05688c,0x2b3e6c1f);
      p->h[6] = U64(0x1f83d9ab,0xfb41bd6b); p->h[7] = U64(0x5be0cd19,0x137e2179);
   }
   p->len    = 0;
   p->buflen = 0;
   p->is384  = is384;
}

void sha512_stream_update(struct sha512_state *p,
      const uint8_t *data, size_t len)
{
   /* An empty update may carry a NULL pointer (a PRF with no second
    * seed); nothing to copy, and memcpy must not see it. */
   if (!len)
      return;
   p->len += len;
   if (p->buflen)
   {
      size_t take = 128 - p->buflen;
      if (take > len)
         take = len;
      memcpy(p->buf + p->buflen, data, take);
      p->buflen += (unsigned)take;
      data      += take;
      len       -= take;
      if (p->buflen < 128)
         return;
      sha512_block(p, p->buf);
      p->buflen = 0;
   }
   while (len >= 128)
   {
      sha512_block(p, data);
      data += 128;
      len  -= 128;
   }
   if (len)
   {
      memcpy(p->buf, data, len);
      p->buflen = (unsigned)len;
   }
}

void sha512_stream_final(struct sha512_state *p, uint8_t *digest)
{
   unsigned i;
   unsigned n = p->is384 ? 6 : 8;
   uint64_t bits = p->len * 8;

   p->buf[p->buflen++] = 0x80;
   if (p->buflen > 112)
   {
      memset(p->buf + p->buflen, 0, 128 - p->buflen);
      sha512_block(p, p->buf);
      p->buflen = 0;
   }
   memset(p->buf + p->buflen, 0, 128 - p->buflen);
   /* 128-bit length; the high 64 bits are zero for anything we hash. */
   sha512_store64(p->buf + 120, bits);
   sha512_block(p, p->buf);

   for (i = 0; i < n; i++)
      sha512_store64(digest + 8 * i, p->h[i]);
   memset(p, 0, sizeof(*p));
}

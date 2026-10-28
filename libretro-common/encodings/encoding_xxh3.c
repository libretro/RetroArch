/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (encoding_xxh3.c).
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

/* XXH3, 64-bit, as xxHash specifies it.
 *
 * Up to 240 octets the hash is a few multiplies of the input against
 * the secret, in scalar code. Past that it is eight accumulators taken
 * a 64-octet stripe at a time: each stripe's octets, keyed with the
 * secret, are multiplied low half by high half into the accumulators,
 * and added to them as they are with each pair of accumulators
 * swapped; every sixteen stripes the accumulators are scrambled.
 *
 * Two things here the specification's own code does differently, the
 * value being the same:
 *
 * - The octets are added unswapped into an accumulator of their own,
 *   and that sum swapped into the others once, before each scramble:
 *   a swap is a permutation and the adds wrap, so swapping a sum is
 *   summing the swaps. A stripe is a load, an exclusive or, a shift, a
 *   multiply and two adds, and each accumulator is one add a stripe
 *   long.
 *
 * - A seed does not make a secret of its own to be written out and
 *   read back. The seeded secret is the default one with the seed
 *   added to its even words and taken from its odd ones, so a key is
 *   the default key plus a constant; the two keys that do not start on
 *   a word are put together from their neighbours. The AVX-512 path
 *   holds all of them in registers for the whole input. */

#include <string.h>
#if defined(_MSC_VER)
#include <stdlib.h>
#include <intrin.h>
#endif

#include <retro_inline.h>
#include <retro_endianness.h>
#include <encodings/xxh3.h>

/* ---- which vector paths can be built ---- */

#if defined(__x86_64__) || defined(__i386__) \
   || defined(_M_X64)   || defined(_M_IX86)
#  if defined(__clang__) && (defined(__SCE__) || defined(_MSC_VER))
     /* Clang's immintrin.h for PlayStation and clang-cl declares only
      * the intrinsics of ISAs the whole build enables: a path is built
      * when the baseline has it */
#    if defined(__AVX512F__) && defined(__AVX512BW__)
#      define XXH3_HAVE_AVX512 1
#    endif
#    if defined(__AVX2__)
#      define XXH3_HAVE_AVX2 1
#    endif
#    if defined(__SSE2__) || defined(_M_X64)
#      define XXH3_HAVE_SSE2 1
#    endif
#    define XXH3_TARGET(t)
#  elif defined(_MSC_VER)
     /* AVX-512 intrinsics from Visual Studio 2017 15.3, AVX2 from 2012 */
#    if _MSC_VER >= 1911
#      define XXH3_HAVE_AVX512 1
#    endif
#    if _MSC_VER >= 1700
#      define XXH3_HAVE_AVX2 1
#    endif
#    define XXH3_HAVE_SSE2 1
#    define XXH3_TARGET(t)
#  elif defined(__has_attribute) && defined(__has_include)
#    if __has_attribute(target) && __has_include(<immintrin.h>)
#      define XXH3_HAVE_AVX512 1
#      define XXH3_HAVE_AVX2   1
#      define XXH3_HAVE_SSE2   1
#      define XXH3_TARGET(t) __attribute__((target(t)))
#    endif
#  elif defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 5
     /* GCC 5 brought AVX-512BW and target attributes for it */
#    define XXH3_HAVE_AVX512 1
#    define XXH3_HAVE_AVX2   1
#    define XXH3_HAVE_SSE2   1
#    define XXH3_TARGET(t) __attribute__((target(t)))
#  elif defined(__GNUC__) && !defined(__clang__) \
     && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9))
#    define XXH3_HAVE_AVX2   1
#    define XXH3_HAVE_SSE2   1
#    define XXH3_TARGET(t) __attribute__((target(t)))
#  endif
   /* AVX-512's keys are held in registers, which 32-bit x86 has eight
    * of: there it would spill more than it saves */
#  if defined(XXH3_HAVE_AVX512) && !defined(__x86_64__) && !defined(_M_X64)
#    undef XXH3_HAVE_AVX512
#  endif
#  if defined(XXH3_HAVE_SSE2)
#    include <immintrin.h>
#    include <retro_atomic.h>
#    include <features/features_cpu.h>
#  endif
#elif (defined(__aarch64__) || defined(_M_ARM64) || defined(__ARM_NEON)) \
   && RETRO_IS_LITTLE_ENDIAN
#  define XXH3_HAVE_NEON 1
#  if defined(_M_ARM64) && defined(_MSC_VER)
#    include <arm64_neon.h>
#  else
#    include <arm_neon.h>
#  endif
#endif

/* ---- constants ---- */

#define XXH3_P32_1 0x9E3779B1U
#define XXH3_P32_2 0x85EBCA77U
#define XXH3_P32_3 0xC2B2AE3DU
#define XXH3_P64_1 0x9E3779B185EBCA87ULL
#define XXH3_P64_2 0xC2B2AE3D27D4EB4FULL
#define XXH3_P64_3 0x165667B19E3779F9ULL
#define XXH3_P64_4 0x85EBCA77C2B2AE63ULL
#define XXH3_P64_5 0x27D4EB2F165667C5ULL

#define XXH3_SECRET  192
#define XXH3_STRIPE  64
/* stripes between scrambles: (secret - stripe) / 8 */
#define XXH3_STRIPES 16
#define XXH3_BLOCK   (XXH3_STRIPE * XXH3_STRIPES)
/* the key of the last sixteen octets from 129 to 240: 17 short of the
 * smallest secret the format allows, 136 */
#define XXH3_SECRET_MIN_LAST (136 - 17)

static const uint8_t xxh3_secret[XXH3_SECRET] = {
   0xb8, 0xfe, 0x6c, 0x39, 0x23, 0xa4, 0x4b, 0xbe, 0x7c, 0x01, 0x81, 0x2c, 0xf7, 0x21, 0xad, 0x1c,
   0xde, 0xd4, 0x6d, 0xe9, 0x83, 0x90, 0x97, 0xdb, 0x72, 0x40, 0xa4, 0xa4, 0xb7, 0xb3, 0x67, 0x1f,
   0xcb, 0x79, 0xe6, 0x4e, 0xcc, 0xc0, 0xe5, 0x78, 0x82, 0x5a, 0xd0, 0x7d, 0xcc, 0xff, 0x72, 0x21,
   0xb8, 0x08, 0x46, 0x74, 0xf7, 0x43, 0x24, 0x8e, 0xe0, 0x35, 0x90, 0xe6, 0x81, 0x3a, 0x26, 0x4c,
   0x3c, 0x28, 0x52, 0xbb, 0x91, 0xc3, 0x00, 0xcb, 0x88, 0xd0, 0x65, 0x8b, 0x1b, 0x53, 0x2e, 0xa3,
   0x71, 0x64, 0x48, 0x97, 0xa2, 0x0d, 0xf9, 0x4e, 0x38, 0x19, 0xef, 0x46, 0xa9, 0xde, 0xac, 0xd8,
   0xa8, 0xfa, 0x76, 0x3f, 0xe3, 0x9c, 0x34, 0x3f, 0xf9, 0xdc, 0xbb, 0xc7, 0xc7, 0x0b, 0x4f, 0x1d,
   0x8a, 0x51, 0xe0, 0x4b, 0xcd, 0xb4, 0x59, 0x31, 0xc8, 0x9f, 0x7e, 0xc9, 0xd9, 0x78, 0x73, 0x64,
   0xea, 0xc5, 0xac, 0x83, 0x34, 0xd3, 0xeb, 0xc3, 0xc5, 0x81, 0xa0, 0xff, 0xfa, 0x13, 0x63, 0xeb,
   0x17, 0x0d, 0xdd, 0x51, 0xb7, 0xf0, 0xda, 0x49, 0xd3, 0x16, 0x55, 0x26, 0x29, 0xd4, 0x68, 0x9e,
   0x2b, 0x16, 0xbe, 0x58, 0x7d, 0x47, 0xa1, 0xfc, 0x8f, 0xf8, 0xb8, 0xd1, 0x7a, 0xd0, 0x31, 0xce,
   0x45, 0xcb, 0x3a, 0x8f, 0x95, 0x16, 0x04, 0x28, 0xaf, 0xd7, 0xfb, 0xca, 0xbb, 0x4b, 0x40, 0x7e,
};

static const uint64_t xxh3_init_acc[8] = {
   XXH3_P32_3, XXH3_P64_1, XXH3_P64_2, XXH3_P64_3,
   XXH3_P64_4, XXH3_P32_2, XXH3_P64_5, XXH3_P32_1
};

/* Each stripe asks for the line eight stripes on, where the next turns
 * find it in cache, and the one sixty-four on, which is what keeps up
 * with memory once the input is bigger than the caches. A request past
 * the end of the input is dropped. */
#if defined(XXH3_HAVE_SSE2)
#define XXH3_PREFETCH(p) do { \
   _mm_prefetch((const char*)(p) + 512,  _MM_HINT_T0); \
   _mm_prefetch((const char*)(p) + 4096, _MM_HINT_T0); \
} while (0)
#elif defined(__GNUC__)
#define XXH3_PREFETCH(p) do { \
   __builtin_prefetch((const char*)(p) + 512); \
   __builtin_prefetch((const char*)(p) + 4096); \
} while (0)
#else
#define XXH3_PREFETCH(p) do { } while (0)
#endif

/* ---- scalar pieces ---- */

#if defined(_MSC_VER)
#define XXH3_ROTL(x, r) _rotl64((x), (r))
#else
#define XXH3_ROTL(x, r) (((x) << (r)) | ((x) >> (64 - (r))))
#endif

static INLINE uint64_t xxh3_rd64(const uint8_t *p)
{
   uint64_t v;
   memcpy(&v, p, 8);
   return swap_if_big64(v);
}

static INLINE uint32_t xxh3_rd32(const uint8_t *p)
{
   uint32_t v;
   memcpy(&v, p, 4);
   return swap_if_big32(v);
}

/* The 128-bit product of @a and @b, its halves folded together */
static INLINE uint64_t xxh3_fold(uint64_t a, uint64_t b)
{
#if defined(__SIZEOF_INT128__)
   __extension__ typedef unsigned __int128 xxh3_u128;
   xxh3_u128 p = (xxh3_u128)a * b;
   return (uint64_t)p ^ (uint64_t)(p >> 64);
#elif defined(_MSC_VER) && defined(_M_X64)
   uint64_t hi;
   uint64_t lo = _umul128(a, b, &hi);
   return lo ^ hi;
#elif defined(_MSC_VER) && defined(_M_ARM64)
   return (a * b) ^ __umulh(a, b);
#else
   uint64_t lo_lo = (a & 0xFFFFFFFFU) * (b & 0xFFFFFFFFU);
   uint64_t hi_lo = (a >> 32)         * (b & 0xFFFFFFFFU);
   uint64_t lo_hi = (a & 0xFFFFFFFFU) * (b >> 32);
   uint64_t hi_hi = (a >> 32)         * (b >> 32);
   uint64_t cross = (lo_lo >> 32) + (hi_lo & 0xFFFFFFFFU) + lo_hi;
   uint64_t upper = (hi_lo >> 32) + (cross >> 32) + hi_hi;
   uint64_t lower = (cross << 32) | (lo_lo & 0xFFFFFFFFU);
   return lower ^ upper;
#endif
}

static INLINE uint64_t xxh3_xxh64_avalanche(uint64_t h)
{
   h ^= h >> 33;
   h *= XXH3_P64_2;
   h ^= h >> 29;
   h *= XXH3_P64_3;
   return h ^ (h >> 32);
}

static INLINE uint64_t xxh3_avalanche(uint64_t h)
{
   h ^= h >> 37;
   h *= 0x165667919E3779F9ULL;
   return h ^ (h >> 32);
}

static uint64_t xxh3_rrmxmx(uint64_t h, uint64_t len)
{
   h ^= XXH3_ROTL(h, 49) ^ XXH3_ROTL(h, 24);
   h *= 0x9FB21C651E98DF25ULL;
   h ^= (h >> 35) + len;
   h *= 0x9FB21C651E98DF25ULL;
   return h ^ (h >> 28);
}

static INLINE uint64_t xxh3_mix16(const uint8_t *in, const uint8_t *sec,
      uint64_t seed)
{
   return xxh3_fold(xxh3_rd64(in)     ^ (xxh3_rd64(sec)     + seed),
                    xxh3_rd64(in + 8) ^ (xxh3_rd64(sec + 8) - seed));
}

/* ---- up to 240 octets ---- */

static uint64_t xxh3_0to16(const uint8_t *in, size_t len, uint64_t seed)
{
   const uint8_t *k = xxh3_secret;

   if (len > 8)
   {
      uint64_t lo = xxh3_rd64(in)
         ^ ((xxh3_rd64(k + 24) ^ xxh3_rd64(k + 32)) + seed);
      uint64_t hi = xxh3_rd64(in + len - 8)
         ^ ((xxh3_rd64(k + 40) ^ xxh3_rd64(k + 48)) - seed);
      return xxh3_avalanche((uint64_t)len + SWAP64(lo) + hi
            + xxh3_fold(lo, hi));
   }
   if (len >= 4)
   {
      uint64_t s  = seed ^ ((uint64_t)SWAP32((uint32_t)seed) << 32);
      uint64_t in64 = (uint64_t)xxh3_rd32(in + len - 4)
         + ((uint64_t)xxh3_rd32(in) << 32);
      return xxh3_rrmxmx(in64
            ^ ((xxh3_rd64(k + 8) ^ xxh3_rd64(k + 16)) - s), len);
   }
   if (len)
   {
      uint32_t c = ((uint32_t)in[0] << 16) | ((uint32_t)in[len >> 1] << 24)
                 | (uint32_t)in[len - 1]   | ((uint32_t)len << 8);
      return xxh3_xxh64_avalanche((uint64_t)c
            ^ ((uint64_t)(xxh3_rd32(k) ^ xxh3_rd32(k + 4)) + seed));
   }
   return xxh3_xxh64_avalanche(seed ^ (xxh3_rd64(k + 56) ^ xxh3_rd64(k + 64)));
}

static uint64_t xxh3_17to128(const uint8_t *in, size_t len, uint64_t seed)
{
   const uint8_t *k   = xxh3_secret;
   uint64_t       acc = (uint64_t)len * XXH3_P64_1;

   if (len > 32)
   {
      if (len > 64)
      {
         if (len > 96)
         {
            acc += xxh3_mix16(in + 48,       k + 96,  seed);
            acc += xxh3_mix16(in + len - 64, k + 112, seed);
         }
         acc += xxh3_mix16(in + 32,       k + 64, seed);
         acc += xxh3_mix16(in + len - 48, k + 80, seed);
      }
      acc += xxh3_mix16(in + 16,       k + 32, seed);
      acc += xxh3_mix16(in + len - 32, k + 48, seed);
   }
   acc += xxh3_mix16(in,            k,      seed);
   acc += xxh3_mix16(in + len - 16, k + 16, seed);
   return xxh3_avalanche(acc);
}

static uint64_t xxh3_129to240(const uint8_t *in, size_t len, uint64_t seed)
{
   const uint8_t *k      = xxh3_secret;
   uint64_t       acc    = (uint64_t)len * XXH3_P64_1;
   size_t         rounds = len / 16;
   size_t         i;

   for (i = 0; i < 8; i++)
      acc += xxh3_mix16(in + 16 * i, k + 16 * i, seed);
   acc = xxh3_avalanche(acc);
   for (i = 8; i < rounds; i++)
      acc += xxh3_mix16(in + 16 * i, k + 16 * (i - 8) + 3, seed);
   acc += xxh3_mix16(in + len - 16, k + XXH3_SECRET_MIN_LAST, seed);
   return xxh3_avalanche(acc);
}

/* ---- past 240: the accumulators ----
 *
 * Each path leaves the eight accumulators in @acc and the eight words
 * of secret they are merged with in @mkey. */

/* The seeded secret's word @j */
#define XXH3_WORD(j, seed) \
   (xxh3_rd64(xxh3_secret + 8 * (j)) + (((j) & 1) ? 0 - (seed) : (seed)))

/* The seeded secret, where a path reads it from memory. With no seed it
 * is the default, read where it is. */
static const uint8_t *xxh3_keys(uint8_t *buf, uint64_t seed)
{
   unsigned j;
   if (!seed)
      return xxh3_secret;
   for (j = 0; j < XXH3_SECRET / 8; j++)
   {
      uint64_t w = swap_if_big64(XXH3_WORD(j, seed));
      memcpy(buf + 8 * j, &w, 8);
   }
   return buf;
}

/* The accumulators merged with the words of secret at @mkey, and the
 * avalanche */
static INLINE uint64_t xxh3_merge(const uint64_t *acc, const uint64_t *mkey,
      uint64_t len)
{
   uint64_t h = len * XXH3_P64_1;
   h += xxh3_fold(acc[0] ^ mkey[0], acc[1] ^ mkey[1]);
   h += xxh3_fold(acc[2] ^ mkey[2], acc[3] ^ mkey[3]);
   h += xxh3_fold(acc[4] ^ mkey[4], acc[5] ^ mkey[5]);
   h += xxh3_fold(acc[6] ^ mkey[6], acc[7] ^ mkey[7]);
   return xxh3_avalanche(h);
}

/* The same, merged with the secret at @sec, eleven octets in */
static INLINE uint64_t xxh3_merge_sec(const uint64_t *acc,
      const uint8_t *sec, uint64_t len)
{
   uint64_t h = len * XXH3_P64_1;
   h += xxh3_fold(acc[0] ^ xxh3_rd64(sec + 11), acc[1] ^ xxh3_rd64(sec + 19));
   h += xxh3_fold(acc[2] ^ xxh3_rd64(sec + 27), acc[3] ^ xxh3_rd64(sec + 35));
   h += xxh3_fold(acc[4] ^ xxh3_rd64(sec + 43), acc[5] ^ xxh3_rd64(sec + 51));
   h += xxh3_fold(acc[6] ^ xxh3_rd64(sec + 59), acc[7] ^ xxh3_rd64(sec + 67));
   return xxh3_avalanche(h);
}

/* Where the last stripe's key starts: not on a word, so unlike any
 * other */
#define XXH3_LAST_KEY (XXH3_SECRET - XXH3_STRIPE - 7)

/* scalar */

#define XXH3_LANE(i) do { \
   uint64_t d_  = xxh3_rd64(p + 8 * (i)); \
   uint64_t dk_ = d_ ^ xxh3_rd64(k + 8 * (i)); \
   a[(i) ^ 1] += d_; \
   a[i] += (dk_ & 0xFFFFFFFFU) * (dk_ >> 32); \
} while (0)

static void xxh3_stripe_scalar(uint64_t *a, const uint8_t *p,
      const uint8_t *k)
{
   XXH3_LANE(0); XXH3_LANE(1); XXH3_LANE(2); XXH3_LANE(3);
   XXH3_LANE(4); XXH3_LANE(5); XXH3_LANE(6); XXH3_LANE(7);
}

static uint64_t xxh3_long_scalar(const uint8_t *in, size_t len, uint64_t seed)
{
   const size_t   total = len;
   uint64_t       acc[8];
   uint8_t        buf[XXH3_SECRET];
   const uint8_t *sec    = xxh3_keys(buf, seed);
   size_t         blocks = (len - 1) / XXH3_BLOCK;
   size_t         b, n, tail;
   unsigned       i;

   memcpy(acc, xxh3_init_acc, sizeof(xxh3_init_acc));
   for (b = 0; b < blocks; b++, in += XXH3_BLOCK)
   {
      for (n = 0; n < XXH3_STRIPES; n++)
         xxh3_stripe_scalar(acc, in + n * XXH3_STRIPE, sec + 8 * n);
      for (i = 0; i < 8; i++)
      {
         uint64_t x = acc[i];
         x ^= x >> 47;
         x ^= xxh3_rd64(sec + XXH3_SECRET - XXH3_STRIPE + 8 * i);
         acc[i] = x * XXH3_P32_1;
      }
      len -= XXH3_BLOCK;
   }
   tail = (len - 1) / XXH3_STRIPE;
   for (n = 0; n < tail; n++)
      xxh3_stripe_scalar(acc, in + n * XXH3_STRIPE, sec + 8 * n);
   xxh3_stripe_scalar(acc, in + len - XXH3_STRIPE, sec + XXH3_LAST_KEY);
   return xxh3_merge_sec(acc, sec, total);
}

/* ---- a stripe at a time, for xxh3_update() ----
 *
 * @n stripes at @in, then @n2 at @in2, into @acc: the first the
 * *@pos'th of its block, keyed with @sec. A block is scrambled as its
 * sixteenth stripe goes in, so a caller hands over only stripes with
 * more input after them. Within a block the stripes only add, so those
 * at @in, what xxh3_update() held and has just made whole, go in at
 * the end of their block or of the call when they do not end it: by
 * then the copies that made them can be read back without waiting on
 * them. Whole blocks take the loop the one-call paths do. Each path has
 * one, round these loops. */

#define XXH3_LATE(STRIPE) do { \
   for (; lc_; lc_--, lp_ += XXH3_STRIPE, lk_++) \
      STRIPE(lp_, sec + 8 * lk_); \
} while (0)

#define XXH3_RUN(STRIPE, SCRAMBLE, BLOCKS) do { \
   const uint8_t *lp_ = in; \
   size_t         lc_ = 0; \
   unsigned       lk_ = k; \
   int            seg_; \
   if (k + n < XXH3_STRIPES) \
   { \
      lc_ = n; \
      k  += (unsigned)n; \
      n   = 0; \
   } \
   for (seg_ = 0; ; seg_++) \
   { \
      for (; k && n; n--, in += XXH3_STRIPE) \
      { \
         STRIPE(in, sec + 8 * k); \
         if (++k == XXH3_STRIPES) \
         { \
            XXH3_LATE(STRIPE); \
            SCRAMBLE; \
            k = 0; \
         } \
      } \
      BLOCKS; \
      for (; n; n--, in += XXH3_STRIPE, k++) \
         STRIPE(in, sec + 8 * k); \
      if (seg_) \
         break; \
      in = in2; \
      n  = n2; \
   } \
   XXH3_LATE(STRIPE); \
} while (0)

/* the whole blocks, keyed from memory */
#define XXH3_BLOCKS(STRIPE, SCRAMBLE) \
   for (; n >= XXH3_STRIPES; n -= XXH3_STRIPES, in += XXH3_BLOCK) \
   { \
      size_t s_; \
      for (s_ = 0; s_ < XXH3_STRIPES; s_++) \
         STRIPE(in + s_ * XXH3_STRIPE, sec + 8 * s_); \
      SCRAMBLE; \
   }

/* A stripe is taken only once there is more after it: the input's last
 * stripe is keyed apart, and the hash of 240 octets or fewer is not the
 * accumulators'. Between calls up to XXH3_HELD octets are held, after
 * the last stripe taken; pieces over XXH3_HOLD go where they lie. */
#define XXH3_HELD 1024
#define XXH3_HOLD 256

/* @len octets from @p to @d, the short ones in two fixed copies that
 * overlap */
static INLINE void xxh3_hold(uint8_t *d, const uint8_t *p, size_t len)
{
   if (len > 64)
      memcpy(d, p, len);
   else if (len > 32)
   {
      memcpy(d, p, 32);
      memcpy(d + len - 32, p + len - 32, 32);
   }
   else if (len >= 16)
   {
      memcpy(d, p, 16);
      memcpy(d + len - 16, p + len - 16, 16);
   }
   else if (len >= 8)
   {
      memcpy(d, p, 8);
      memcpy(d + len - 8, p + len - 8, 8);
   }
   else if (len >= 4)
   {
      memcpy(d, p, 4);
      memcpy(d + len - 4, p + len - 4, 4);
   }
   else if (len)
   {
      d[0]       = p[0];
      d[len / 2] = p[len / 2];
      d[len - 1] = p[len - 1];
   }
}

/* What xxh3_update() does past what it holds, round each path's run:
 * the stripes held, the one they end inside made whole from the front
 * of the input, which goes on past it; then the input's own, where
 * they lie, at least one octet left */
#define XXH3_FILL(d, p, fill) memcpy((d), (p), (fill))
#define XXH3_KEEP(d, p, len)  memcpy((d), (p), (len))
#define XXH3_TAKE(RUN) XXH3_TAKE_WITH(RUN, XXH3_FILL, XXH3_KEEP)
#define XXH3_TAKE_WITH(RUN, FILL, KEEP) \
{ \
   const uint8_t *sec  = s->seed ? s->sec : xxh3_secret; \
   uint8_t       *held = s->buf + XXH3_STRIPE; \
   size_t         h    = s->held; \
   size_t         n    = h / XXH3_STRIPE; \
   size_t         m; \
   if (h & (XXH3_STRIPE - 1)) \
   { \
      size_t fill = XXH3_STRIPE - (h & (XXH3_STRIPE - 1)); \
      FILL(held + h, p, fill); \
      p   += fill; \
      len -= fill; \
      n++; \
   } \
   m = (len - 1) / XXH3_STRIPE; \
   RUN(s->acc, held, n, p, m, &s->pos, sec); \
   len -= m * XXH3_STRIPE; \
   s->held = (uint32_t)len; \
   /* the last stripe taken and what is left after it, in one copy where \
    * they lie together */ \
   if (m) \
      KEEP(s->buf, p + (m - 1) * XXH3_STRIPE, XXH3_STRIPE + len); \
   else \
   { \
      KEEP(s->buf, held + (n - 1) * XXH3_STRIPE, XXH3_STRIPE); \
      KEEP(held, p, len); \
   } \
}

static void xxh3_scramble_scalar(uint64_t *acc, const uint8_t *sec)
{
   unsigned i;
   for (i = 0; i < 8; i++)
   {
      uint64_t x = acc[i];
      x ^= x >> 47;
      x ^= xxh3_rd64(sec + XXH3_SECRET - XXH3_STRIPE + 8 * i);
      acc[i] = x * XXH3_P32_1;
   }
}

#define XXH3_STRIPE_SC(p, key) xxh3_stripe_scalar(acc, (p), (key))
static INLINE void xxh3_run_scalar(uint64_t *acc, const uint8_t *in,
      size_t n, const uint8_t *in2, size_t n2, unsigned *pos,
      const uint8_t *sec)
{
   unsigned k = *pos;
   XXH3_RUN(XXH3_STRIPE_SC, xxh3_scramble_scalar(acc, sec),
         XXH3_BLOCKS(XXH3_STRIPE_SC, xxh3_scramble_scalar(acc, sec)));
   *pos = k;
}

#if !defined(XXH3_HAVE_NEON)
static void xxh3_take_scalar(xxh3_state_t *s, const uint8_t *p, size_t len)
XXH3_TAKE(xxh3_run_scalar)
#endif
#undef XXH3_STRIPE_SC

#if defined(XXH3_HAVE_SSE2)

/* SSE2: four vectors a stripe, the octets' sum beside the products */

#define XXH3_LD128(p) _mm_loadu_si128((const __m128i*)(const void*)(p))

#define XXH3_PART_SSE2(P, D, p, k) do { \
   __m128i d_  = XXH3_LD128(p); \
   __m128i dk_ = _mm_xor_si128(d_, XXH3_LD128(k)); \
   (P) = _mm_add_epi64((P), _mm_mul_epu32(dk_, _mm_srli_epi64(dk_, 32))); \
   (D) = _mm_add_epi64((D), d_); \
} while (0)

#define XXH3_STRIPE_SSE2(p, k) do { \
   XXH3_PREFETCH(p); \
   XXH3_PART_SSE2(P0, D0, (p),      (k)); \
   XXH3_PART_SSE2(P1, D1, (p) + 16, (k) + 16); \
   XXH3_PART_SSE2(P2, D2, (p) + 32, (k) + 32); \
   XXH3_PART_SSE2(P3, D3, (p) + 48, (k) + 48); \
} while (0)

#define XXH3_SCRAMBLE_SSE2(P, D, k) do { \
   __m128i x_ = _mm_add_epi64((P), _mm_shuffle_epi32((D), 0x4E)); \
   x_ = _mm_xor_si128(x_, _mm_srli_epi64(x_, 47)); \
   x_ = _mm_xor_si128(x_, XXH3_LD128(k)); \
   (P) = _mm_add_epi64(_mm_mul_epu32(x_, prime), \
         _mm_slli_epi64(_mm_mul_epu32(_mm_srli_epi64(x_, 32), prime), 32)); \
   (D) = _mm_setzero_si128(); \
} while (0)

/* The seeded secret, sixteen octets at a time */
XXH3_TARGET("sse2")
static const uint8_t *xxh3_keys_sse2(uint8_t *buf, uint64_t seed)
{
   unsigned i;
   __m128i  s;
   if (!seed)
      return xxh3_secret;
   s = _mm_set_epi32((int)((0 - seed) >> 32), (int)(0 - seed),
         (int)(seed >> 32), (int)seed);
   for (i = 0; i < XXH3_SECRET; i += 16)
      _mm_storeu_si128((__m128i*)(void*)(buf + i),
            _mm_add_epi64(XXH3_LD128(xxh3_secret + i), s));
   return buf;
}

XXH3_TARGET("sse2")
static uint64_t xxh3_long_sse2(const uint8_t *in, size_t len, uint64_t seed)
{
   const size_t   total = len;
   uint64_t       acc[8];
   uint8_t        buf[XXH3_SECRET];
   const uint8_t *sec    = xxh3_keys_sse2(buf, seed);
   const uint8_t *ks     = sec + XXH3_SECRET - XXH3_STRIPE;
   size_t         blocks = (len - 1) / XXH3_BLOCK;
   size_t         b, n, tail;
   const __m128i  prime  = _mm_set1_epi32((int)XXH3_P32_1);
   __m128i        P0 = XXH3_LD128(xxh3_init_acc);
   __m128i        P1 = XXH3_LD128(xxh3_init_acc + 2);
   __m128i        P2 = XXH3_LD128(xxh3_init_acc + 4);
   __m128i        P3 = XXH3_LD128(xxh3_init_acc + 6);
   __m128i        D0 = _mm_setzero_si128();
   __m128i        D1 = _mm_setzero_si128();
   __m128i        D2 = _mm_setzero_si128();
   __m128i        D3 = _mm_setzero_si128();

   for (b = 0; b < blocks; b++, in += XXH3_BLOCK)
   {
      for (n = 0; n < XXH3_STRIPES; n++)
         XXH3_STRIPE_SSE2(in + n * XXH3_STRIPE, sec + 8 * n);
      XXH3_SCRAMBLE_SSE2(P0, D0, ks);
      XXH3_SCRAMBLE_SSE2(P1, D1, ks + 16);
      XXH3_SCRAMBLE_SSE2(P2, D2, ks + 32);
      XXH3_SCRAMBLE_SSE2(P3, D3, ks + 48);
      len -= XXH3_BLOCK;
   }
   tail = (len - 1) / XXH3_STRIPE;
   for (n = 0; n < tail; n++)
      XXH3_STRIPE_SSE2(in + n * XXH3_STRIPE, sec + 8 * n);
   XXH3_STRIPE_SSE2(in + len - XXH3_STRIPE, sec + XXH3_LAST_KEY);
   _mm_storeu_si128((__m128i*)(void*)acc,       _mm_add_epi64(P0, _mm_shuffle_epi32(D0, 0x4E)));
   _mm_storeu_si128((__m128i*)(void*)(acc + 2), _mm_add_epi64(P1, _mm_shuffle_epi32(D1, 0x4E)));
   _mm_storeu_si128((__m128i*)(void*)(acc + 4), _mm_add_epi64(P2, _mm_shuffle_epi32(D2, 0x4E)));
   _mm_storeu_si128((__m128i*)(void*)(acc + 6), _mm_add_epi64(P3, _mm_shuffle_epi32(D3, 0x4E)));
   return xxh3_merge_sec(acc, sec, total);
}

XXH3_TARGET("sse2")
static INLINE void xxh3_run_sse2(uint64_t *acc, const uint8_t *in,
      size_t n, const uint8_t *in2, size_t n2, unsigned *pos,
      const uint8_t *sec)
{
   const uint8_t *ks    = sec + XXH3_SECRET - XXH3_STRIPE;
   const __m128i  prime = _mm_set1_epi32((int)XXH3_P32_1);
   unsigned       k     = *pos;
   __m128i        P0 = XXH3_LD128(acc);
   __m128i        P1 = XXH3_LD128(acc + 2);
   __m128i        P2 = XXH3_LD128(acc + 4);
   __m128i        P3 = XXH3_LD128(acc + 6);
   __m128i        D0 = _mm_setzero_si128();
   __m128i        D1 = _mm_setzero_si128();
   __m128i        D2 = _mm_setzero_si128();
   __m128i        D3 = _mm_setzero_si128();
#define XXH3_SCRAMBLE4_SSE2 do { \
   XXH3_SCRAMBLE_SSE2(P0, D0, ks); \
   XXH3_SCRAMBLE_SSE2(P1, D1, ks + 16); \
   XXH3_SCRAMBLE_SSE2(P2, D2, ks + 32); \
   XXH3_SCRAMBLE_SSE2(P3, D3, ks + 48); \
} while (0)
   XXH3_RUN(XXH3_STRIPE_SSE2, XXH3_SCRAMBLE4_SSE2,
         XXH3_BLOCKS(XXH3_STRIPE_SSE2, XXH3_SCRAMBLE4_SSE2));
#undef XXH3_SCRAMBLE4_SSE2
   _mm_storeu_si128((__m128i*)(void*)acc,       _mm_add_epi64(P0, _mm_shuffle_epi32(D0, 0x4E)));
   _mm_storeu_si128((__m128i*)(void*)(acc + 2), _mm_add_epi64(P1, _mm_shuffle_epi32(D1, 0x4E)));
   _mm_storeu_si128((__m128i*)(void*)(acc + 4), _mm_add_epi64(P2, _mm_shuffle_epi32(D2, 0x4E)));
   _mm_storeu_si128((__m128i*)(void*)(acc + 6), _mm_add_epi64(P3, _mm_shuffle_epi32(D3, 0x4E)));
   *pos = k;
}

XXH3_TARGET("sse2")
static void xxh3_take_sse2(xxh3_state_t *s, const uint8_t *p, size_t len)
XXH3_TAKE(xxh3_run_sse2)
#undef XXH3_PART_SSE2
#undef XXH3_STRIPE_SSE2
#undef XXH3_SCRAMBLE_SSE2
#endif

#if defined(XXH3_HAVE_AVX2)

/* The seeded secret, thirty-two octets at a time */
XXH3_TARGET("avx2")
static const uint8_t *xxh3_keys_avx2(uint8_t *buf, uint64_t seed)
{
   unsigned i;
   __m256i  s;
   if (!seed)
      return xxh3_secret;
   s = _mm256_set_epi32((int)((0 - seed) >> 32), (int)(0 - seed),
         (int)(seed >> 32), (int)seed, (int)((0 - seed) >> 32),
         (int)(0 - seed), (int)(seed >> 32), (int)seed);
   for (i = 0; i < XXH3_SECRET; i += 32)
      _mm256_storeu_si256((__m256i*)(void*)(buf + i), _mm256_add_epi64(
               _mm256_loadu_si256((const __m256i*)(const void*)(xxh3_secret + i)), s));
   return buf;
}

XXH3_TARGET("avx2")
static uint64_t xxh3_long_avx2(const uint8_t *in, size_t len, uint64_t seed)
{
   const size_t   total = len;
   uint64_t       acc[8];
   uint8_t        buf[XXH3_SECRET];
   const uint8_t *sec    = xxh3_keys_avx2(buf, seed);
   size_t         blocks = (len - 1) / XXH3_BLOCK;
   size_t         b, n, tail;
   const __m256i  prime  = _mm256_set1_epi32((int)XXH3_P32_1);
   __m256i        P0 = _mm256_loadu_si256((const __m256i*)(const void*)xxh3_init_acc);
   __m256i        P1 = _mm256_loadu_si256((const __m256i*)(const void*)(xxh3_init_acc + 4));
   __m256i        D0 = _mm256_setzero_si256();
   __m256i        D1 = _mm256_setzero_si256();

#define XXH3_STRIPE_AVX2(p, k) do { \
   __m256i d0_  = _mm256_loadu_si256((const __m256i*)(const void*)(p)); \
   __m256i d1_  = _mm256_loadu_si256((const __m256i*)(const void*)((p) + 32)); \
   __m256i dk0_ = _mm256_xor_si256(d0_, \
         _mm256_loadu_si256((const __m256i*)(const void*)(k))); \
   __m256i dk1_ = _mm256_xor_si256(d1_, \
         _mm256_loadu_si256((const __m256i*)(const void*)((k) + 32))); \
   XXH3_PREFETCH(p); \
   P0 = _mm256_add_epi64(P0, _mm256_mul_epu32(dk0_, _mm256_srli_epi64(dk0_, 32))); \
   P1 = _mm256_add_epi64(P1, _mm256_mul_epu32(dk1_, _mm256_srli_epi64(dk1_, 32))); \
   D0 = _mm256_add_epi64(D0, d0_); \
   D1 = _mm256_add_epi64(D1, d1_); \
} while (0)

#define XXH3_SCRAMBLE_AVX2(P, D, k) do { \
   __m256i x_ = _mm256_add_epi64((P), _mm256_shuffle_epi32((D), 0x4E)); \
   x_ = _mm256_xor_si256(x_, _mm256_srli_epi64(x_, 47)); \
   x_ = _mm256_xor_si256(x_, _mm256_loadu_si256((const __m256i*)(const void*)(k))); \
   (P) = _mm256_add_epi64(_mm256_mul_epu32(x_, prime), \
         _mm256_slli_epi64(_mm256_mul_epu32(_mm256_srli_epi64(x_, 32), prime), 32)); \
   (D) = _mm256_setzero_si256(); \
} while (0)

   for (b = 0; b < blocks; b++, in += XXH3_BLOCK)
   {
      for (n = 0; n < XXH3_STRIPES; n++)
         XXH3_STRIPE_AVX2(in + n * XXH3_STRIPE, sec + 8 * n);
      XXH3_SCRAMBLE_AVX2(P0, D0, sec + XXH3_SECRET - XXH3_STRIPE);
      XXH3_SCRAMBLE_AVX2(P1, D1, sec + XXH3_SECRET - XXH3_STRIPE + 32);
      len -= XXH3_BLOCK;
   }
   tail = (len - 1) / XXH3_STRIPE;
   for (n = 0; n < tail; n++)
      XXH3_STRIPE_AVX2(in + n * XXH3_STRIPE, sec + 8 * n);
   XXH3_STRIPE_AVX2(in + len - XXH3_STRIPE, sec + XXH3_LAST_KEY);
   _mm256_storeu_si256((__m256i*)(void*)acc,
         _mm256_add_epi64(P0, _mm256_shuffle_epi32(D0, 0x4E)));
   _mm256_storeu_si256((__m256i*)(void*)(acc + 4),
         _mm256_add_epi64(P1, _mm256_shuffle_epi32(D1, 0x4E)));
#undef XXH3_STRIPE_AVX2
#undef XXH3_SCRAMBLE_AVX2
   return xxh3_merge_sec(acc, sec, total);
}

XXH3_TARGET("avx2")
static INLINE void xxh3_run_avx2(uint64_t *acc, const uint8_t *in,
      size_t n, const uint8_t *in2, size_t n2, unsigned *pos,
      const uint8_t *sec)
{
   const __m256i  prime = _mm256_set1_epi32((int)XXH3_P32_1);
   unsigned       k     = *pos;
   __m256i        P0 = _mm256_loadu_si256((const __m256i*)(const void*)acc);
   __m256i        P1 = _mm256_loadu_si256((const __m256i*)(const void*)(acc + 4));
   __m256i        D0 = _mm256_setzero_si256();
   __m256i        D1 = _mm256_setzero_si256();
#define XXH3_STRIPE_AVX2(p, kk) do { \
   __m256i d0_  = _mm256_loadu_si256((const __m256i*)(const void*)(p)); \
   __m256i d1_  = _mm256_loadu_si256((const __m256i*)(const void*)((p) + 32)); \
   __m256i dk0_ = _mm256_xor_si256(d0_, \
         _mm256_loadu_si256((const __m256i*)(const void*)(kk))); \
   __m256i dk1_ = _mm256_xor_si256(d1_, \
         _mm256_loadu_si256((const __m256i*)(const void*)((kk) + 32))); \
   XXH3_PREFETCH(p); \
   P0 = _mm256_add_epi64(P0, _mm256_mul_epu32(dk0_, _mm256_srli_epi64(dk0_, 32))); \
   P1 = _mm256_add_epi64(P1, _mm256_mul_epu32(dk1_, _mm256_srli_epi64(dk1_, 32))); \
   D0 = _mm256_add_epi64(D0, d0_); \
   D1 = _mm256_add_epi64(D1, d1_); \
} while (0)
#define XXH3_SCRAMBLE_AVX2(P, D, kk) do { \
   __m256i x_ = _mm256_add_epi64((P), _mm256_shuffle_epi32((D), 0x4E)); \
   x_ = _mm256_xor_si256(x_, _mm256_srli_epi64(x_, 47)); \
   x_ = _mm256_xor_si256(x_, _mm256_loadu_si256((const __m256i*)(const void*)(kk))); \
   (P) = _mm256_add_epi64(_mm256_mul_epu32(x_, prime), \
         _mm256_slli_epi64(_mm256_mul_epu32(_mm256_srli_epi64(x_, 32), prime), 32)); \
   (D) = _mm256_setzero_si256(); \
} while (0)
#define XXH3_SCRAMBLE2_AVX2 do { \
   XXH3_SCRAMBLE_AVX2(P0, D0, sec + XXH3_SECRET - XXH3_STRIPE); \
   XXH3_SCRAMBLE_AVX2(P1, D1, sec + XXH3_SECRET - XXH3_STRIPE + 32); \
} while (0)
   XXH3_RUN(XXH3_STRIPE_AVX2, XXH3_SCRAMBLE2_AVX2,
         XXH3_BLOCKS(XXH3_STRIPE_AVX2, XXH3_SCRAMBLE2_AVX2));
#undef XXH3_SCRAMBLE2_AVX2
#undef XXH3_STRIPE_AVX2
#undef XXH3_SCRAMBLE_AVX2
   _mm256_storeu_si256((__m256i*)(void*)acc,
         _mm256_add_epi64(P0, _mm256_shuffle_epi32(D0, 0x4E)));
   _mm256_storeu_si256((__m256i*)(void*)(acc + 4),
         _mm256_add_epi64(P1, _mm256_shuffle_epi32(D1, 0x4E)));
   *pos = k;
}

XXH3_TARGET("avx2")
static void xxh3_take_avx2(xxh3_state_t *s, const uint8_t *p, size_t len)
XXH3_TAKE(xxh3_run_avx2)
#endif

#if defined(XXH3_HAVE_AVX512)

/* AVX-512: a stripe is one vector, and every key the input needs is
 * made once, in a register: the sixteen stripes', the scramble's, the
 * last stripe's and the merge's. */

/* With @seeded 0 the keys are the default secret's, read where they
 * are; with 1 they are made in registers. One body, inlined twice. */
XXH3_TARGET("avx512f")
static INLINE uint64_t xxh3_avx512(const uint8_t *in, size_t len,
      uint64_t seed, int seeded)
{
   const size_t   total = len;
   uint64_t       acc[8];
   uint64_t       mkey[8];
   size_t         blocks = (len - 1) / XXH3_BLOCK;
   size_t         n, tail;
   __m512i        P = _mm512_loadu_si512((const void*)xxh3_init_acc);
   __m512i        D = _mm512_setzero_si512();
   __m512i        kl;
   /* the seed into the even words and out of the odd ones; a key at an
    * odd word starts on one of those */
   const __m512i  se = _mm512_mask_set1_epi64(
         _mm512_set1_epi64((long long)seed), 0xAA, (long long)(0 - seed));
   const __m512i  so = _mm512_mask_set1_epi64(
         _mm512_set1_epi64((long long)(0 - seed)), 0xAA, (long long)seed);
#define XXH3_LD512(n) _mm512_loadu_si512((const void*)(xxh3_secret + 8 * (n)))
#define XXH3_KEY512(n) (seeded \
      ? _mm512_add_epi64(XXH3_LD512(n), ((n) & 1) ? so : se) \
      : XXH3_LD512(n))
#define XXH3_STRIPE512(p, k) do { \
   __m512i d_  = _mm512_loadu_si512((const void*)(p)); \
   __m512i dk_ = _mm512_xor_si512(d_, (k)); \
   XXH3_PREFETCH(p); \
   P = _mm512_add_epi64(P, _mm512_mul_epu32(dk_, _mm512_srli_epi64(dk_, 32))); \
   D = _mm512_add_epi64(D, d_); \
} while (0)

   if (blocks)
   {
      const __m512i prime = _mm512_set1_epi32((int)XXH3_P32_1);
      const __m512i k0  = XXH3_KEY512(0),  k1  = XXH3_KEY512(1);
      const __m512i k2  = XXH3_KEY512(2),  k3  = XXH3_KEY512(3);
      const __m512i k4  = XXH3_KEY512(4),  k5  = XXH3_KEY512(5);
      const __m512i k6  = XXH3_KEY512(6),  k7  = XXH3_KEY512(7);
      const __m512i k8  = XXH3_KEY512(8),  k9  = XXH3_KEY512(9);
      const __m512i k10 = XXH3_KEY512(10), k11 = XXH3_KEY512(11);
      const __m512i k12 = XXH3_KEY512(12), k13 = XXH3_KEY512(13);
      const __m512i k14 = XXH3_KEY512(14), k15 = XXH3_KEY512(15);
      /* the scramble's: words 16 to 23 */
      const __m512i ks  = XXH3_KEY512(16);
      size_t        b;

      for (b = 0; b < blocks; b++, in += XXH3_BLOCK)
      {
         __m512i x;
         XXH3_STRIPE512(in,       k0);  XXH3_STRIPE512(in + 64,  k1);
         XXH3_STRIPE512(in + 128, k2);  XXH3_STRIPE512(in + 192, k3);
         XXH3_STRIPE512(in + 256, k4);  XXH3_STRIPE512(in + 320, k5);
         XXH3_STRIPE512(in + 384, k6);  XXH3_STRIPE512(in + 448, k7);
         XXH3_STRIPE512(in + 512, k8);  XXH3_STRIPE512(in + 576, k9);
         XXH3_STRIPE512(in + 640, k10); XXH3_STRIPE512(in + 704, k11);
         XXH3_STRIPE512(in + 768, k12); XXH3_STRIPE512(in + 832, k13);
         XXH3_STRIPE512(in + 896, k14); XXH3_STRIPE512(in + 960, k15);
         /* the sum swapped in, then x ^ x >> 47 ^ key in one operation */
         x = _mm512_add_epi64(P, _mm512_shuffle_epi32(D, (_MM_PERM_ENUM)0x4E));
         x = _mm512_ternarylogic_epi64(x, _mm512_srli_epi64(x, 47), ks, 0x96);
         P = _mm512_add_epi64(_mm512_mul_epu32(x, prime),
               _mm512_slli_epi64(_mm512_mul_epu32(_mm512_srli_epi64(x, 32),
                     prime), 32));
         D = _mm512_setzero_si512();
      }
      len -= blocks * XXH3_BLOCK;
   }
   tail = (len - 1) / XXH3_STRIPE;
   for (n = 0; n < tail; n++)
      XXH3_STRIPE512(in + n * XXH3_STRIPE, XXH3_KEY512(n));
   /* the last stripe's key, a byte into word 15 */
   kl = seeded
      ? _mm512_or_si512(_mm512_srli_epi64(XXH3_KEY512(15), 8),
            _mm512_slli_epi64(XXH3_KEY512(16), 56))
      : _mm512_loadu_si512((const void*)(xxh3_secret + XXH3_LAST_KEY));
   XXH3_STRIPE512(in + len - XXH3_STRIPE, kl);
   _mm512_storeu_si512((void*)acc,
         _mm512_add_epi64(P, _mm512_shuffle_epi32(D, (_MM_PERM_ENUM)0x4E)));
   if (!seeded)
      return xxh3_merge_sec(acc, xxh3_secret, total);
   /* the merge's, three bytes into word 1 */
   _mm512_storeu_si512((void*)mkey, _mm512_or_si512(
            _mm512_srli_epi64(XXH3_KEY512(1), 24),
            _mm512_slli_epi64(XXH3_KEY512(2), 40)));
   return xxh3_merge(acc, mkey, total);
#undef XXH3_STRIPE512
#undef XXH3_KEY512
#undef XXH3_LD512
}

XXH3_TARGET("avx512f")
static uint64_t xxh3_long_avx512(const uint8_t *in, size_t len, uint64_t seed)
{
   return seed ? xxh3_avx512(in, len, seed, 1) : xxh3_avx512(in, len, 0, 0);
}

XXH3_TARGET("avx512f")
static INLINE void xxh3_run_avx512(uint64_t *acc, const uint8_t *in,
      size_t n, const uint8_t *in2, size_t n2, unsigned *pos,
      const uint8_t *sec)
{
#define XXH3_K(j) _mm512_loadu_si512((const void*)(sec + 8 * (j)))
#define XXH3_S(p, key) do { \
   __m512i d_  = _mm512_loadu_si512((const void*)(p)); \
   __m512i dk_ = _mm512_xor_si512(d_, (key)); \
   XXH3_PREFETCH(p); \
   P = _mm512_add_epi64(P, _mm512_mul_epu32(dk_, _mm512_srli_epi64(dk_, 32))); \
   D = _mm512_add_epi64(D, d_); \
} while (0)
#define XXH3_SM(p, kp) XXH3_S(p, _mm512_loadu_si512((const void*)(kp)))
#define XXH3_SCRAMBLE512 do { \
   __m512i x_ = _mm512_add_epi64(P, _mm512_shuffle_epi32(D, (_MM_PERM_ENUM)0x4E)); \
   x_ = _mm512_ternarylogic_epi64(x_, _mm512_srli_epi64(x_, 47), ks, 0x96); \
   P  = _mm512_add_epi64(_mm512_mul_epu32(x_, prime), \
         _mm512_slli_epi64(_mm512_mul_epu32(_mm512_srli_epi64(x_, 32), \
               prime), 32)); \
   D  = _mm512_setzero_si512(); \
} while (0)
/* the whole blocks with their keys in registers */
#define XXH3_BLOCKS512 \
   if (n >= XXH3_STRIPES) \
   { \
      const __m512i k0  = XXH3_K(0),  k1  = XXH3_K(1),  k2  = XXH3_K(2); \
      const __m512i k3  = XXH3_K(3),  k4  = XXH3_K(4),  k5  = XXH3_K(5); \
      const __m512i k6  = XXH3_K(6),  k7  = XXH3_K(7),  k8  = XXH3_K(8); \
      const __m512i k9  = XXH3_K(9),  k10 = XXH3_K(10), k11 = XXH3_K(11); \
      const __m512i k12 = XXH3_K(12), k13 = XXH3_K(13), k14 = XXH3_K(14); \
      const __m512i k15 = XXH3_K(15); \
      for (; n >= XXH3_STRIPES; n -= XXH3_STRIPES, in += XXH3_BLOCK) \
      { \
         XXH3_S(in,       k0);  XXH3_S(in + 64,  k1); \
         XXH3_S(in + 128, k2);  XXH3_S(in + 192, k3); \
         XXH3_S(in + 256, k4);  XXH3_S(in + 320, k5); \
         XXH3_S(in + 384, k6);  XXH3_S(in + 448, k7); \
         XXH3_S(in + 512, k8);  XXH3_S(in + 576, k9); \
         XXH3_S(in + 640, k10); XXH3_S(in + 704, k11); \
         XXH3_S(in + 768, k12); XXH3_S(in + 832, k13); \
         XXH3_S(in + 896, k14); XXH3_S(in + 960, k15); \
         XXH3_SCRAMBLE512; \
      } \
   }
   const __m512i prime = _mm512_set1_epi32((int)XXH3_P32_1);
   const __m512i ks    = XXH3_K(16);
   unsigned      k     = *pos;
   __m512i       P     = _mm512_loadu_si512((const void*)acc);
   __m512i       D     = _mm512_setzero_si512();

   XXH3_RUN(XXH3_SM, XXH3_SCRAMBLE512, XXH3_BLOCKS512);
#undef XXH3_BLOCKS512
#undef XXH3_SCRAMBLE512
#undef XXH3_SM
#undef XXH3_S
#undef XXH3_K
   _mm512_storeu_si512((void*)acc,
         _mm512_add_epi64(P, _mm512_shuffle_epi32(D, (_MM_PERM_ENUM)0x4E)));
   *pos = k;
}

/* Each stripe the next run reads is stored as one vector, which the
 * load finds whole in the store queue: the one the held octets end
 * inside, made whole in a register; the last stripe taken; and the
 * 1 to 64 octets after it, read under a mask */
#define XXH3_FILL512(d, p, fill) do { \
   size_t   off_ = XXH3_STRIPE - (fill); \
   uint8_t *st_  = (d) - off_; \
   _mm512_storeu_si512((void*)st_, _mm512_mask_loadu_epi8( \
         _mm512_loadu_si512((const void*)st_), \
         (__mmask64)(~0ULL << off_), (const void*)((p) - off_))); \
} while (0)
#define XXH3_KEEP512(d, p, len) do { \
   size_t         n_ = (len); \
   uint8_t       *d_ = (d); \
   const uint8_t *p_ = (p); \
   if (n_ > XXH3_STRIPE) \
   { \
      _mm512_storeu_si512((void*)d_, _mm512_loadu_si512((const void*)p_)); \
      d_ += XXH3_STRIPE; \
      p_ += XXH3_STRIPE; \
      n_ -= XXH3_STRIPE; \
   } \
   _mm512_storeu_si512((void*)d_, _mm512_maskz_loadu_epi8( \
         (__mmask64)(~0ULL >> (XXH3_STRIPE - n_)), (const void*)p_)); \
} while (0)
XXH3_TARGET("avx512f,avx512bw")
static void xxh3_take_avx512(xxh3_state_t *s, const uint8_t *p, size_t len)
XXH3_TAKE_WITH(xxh3_run_avx512, XXH3_FILL512, XXH3_KEEP512)
#undef XXH3_KEEP512
#undef XXH3_FILL512
#endif

#if defined(XXH3_HAVE_NEON)

#define XXH3_LDN(p) vreinterpretq_u64_u8(vld1q_u8(p))

#define XXH3_PART_NEON(P, D, p, k) do { \
   uint64x2_t d_  = XXH3_LDN(p); \
   uint64x2_t dk_ = veorq_u64(d_, XXH3_LDN(k)); \
   (P) = vmlal_u32((P), vmovn_u64(dk_), vshrn_n_u64(dk_, 32)); \
   (D) = vaddq_u64((D), d_); \
} while (0)

#define XXH3_STRIPE_NEON(p, k) do { \
   XXH3_PREFETCH(p); \
   XXH3_PART_NEON(P0, D0, (p),      (k)); \
   XXH3_PART_NEON(P1, D1, (p) + 16, (k) + 16); \
   XXH3_PART_NEON(P2, D2, (p) + 32, (k) + 32); \
   XXH3_PART_NEON(P3, D3, (p) + 48, (k) + 48); \
} while (0)

#define XXH3_SCRAMBLE_NEON(P, D, k) do { \
   uint64x2_t x_ = vaddq_u64((P), vextq_u64((D), (D), 1)); \
   x_ = veorq_u64(x_, vshrq_n_u64(x_, 47)); \
   x_ = veorq_u64(x_, XXH3_LDN(k)); \
   (P) = vaddq_u64(vmull_u32(vmovn_u64(x_), prime), \
         vshlq_n_u64(vmull_u32(vshrn_n_u64(x_, 32), prime), 32)); \
   (D) = vdupq_n_u64(0); \
} while (0)

/* The seeded secret, sixteen octets at a time */
static const uint8_t *xxh3_keys_neon(uint8_t *buf, uint64_t seed)
{
   unsigned   i;
   uint64x2_t s;
   if (!seed)
      return xxh3_secret;
   s = vcombine_u64(vcreate_u64(seed), vcreate_u64(0 - seed));
   for (i = 0; i < XXH3_SECRET; i += 16)
      vst1q_u8(buf + i, vreinterpretq_u8_u64(
               vaddq_u64(XXH3_LDN(xxh3_secret + i), s)));
   return buf;
}

static uint64_t xxh3_long_neon(const uint8_t *in, size_t len, uint64_t seed)
{
   const size_t   total = len;
   uint64_t       acc[8];
   uint8_t          buf[XXH3_SECRET];
   const uint8_t   *sec    = xxh3_keys_neon(buf, seed);
   const uint8_t   *ks     = sec + XXH3_SECRET - XXH3_STRIPE;
   size_t           blocks = (len - 1) / XXH3_BLOCK;
   size_t           b, n, tail;
   const uint32x2_t prime  = vdup_n_u32(XXH3_P32_1);
   uint64x2_t       P0 = vld1q_u64(xxh3_init_acc);
   uint64x2_t       P1 = vld1q_u64(xxh3_init_acc + 2);
   uint64x2_t       P2 = vld1q_u64(xxh3_init_acc + 4);
   uint64x2_t       P3 = vld1q_u64(xxh3_init_acc + 6);
   uint64x2_t       D0 = vdupq_n_u64(0);
   uint64x2_t       D1 = D0, D2 = D0, D3 = D0;

   for (b = 0; b < blocks; b++, in += XXH3_BLOCK)
   {
      for (n = 0; n < XXH3_STRIPES; n++)
         XXH3_STRIPE_NEON(in + n * XXH3_STRIPE, sec + 8 * n);
      XXH3_SCRAMBLE_NEON(P0, D0, ks);
      XXH3_SCRAMBLE_NEON(P1, D1, ks + 16);
      XXH3_SCRAMBLE_NEON(P2, D2, ks + 32);
      XXH3_SCRAMBLE_NEON(P3, D3, ks + 48);
      len -= XXH3_BLOCK;
   }
   tail = (len - 1) / XXH3_STRIPE;
   for (n = 0; n < tail; n++)
      XXH3_STRIPE_NEON(in + n * XXH3_STRIPE, sec + 8 * n);
   XXH3_STRIPE_NEON(in + len - XXH3_STRIPE, sec + XXH3_LAST_KEY);
   vst1q_u64(acc,     vaddq_u64(P0, vextq_u64(D0, D0, 1)));
   vst1q_u64(acc + 2, vaddq_u64(P1, vextq_u64(D1, D1, 1)));
   vst1q_u64(acc + 4, vaddq_u64(P2, vextq_u64(D2, D2, 1)));
   vst1q_u64(acc + 6, vaddq_u64(P3, vextq_u64(D3, D3, 1)));
   return xxh3_merge_sec(acc, sec, total);
}

static INLINE void xxh3_run_neon(uint64_t *acc, const uint8_t *in,
      size_t n, const uint8_t *in2, size_t n2, unsigned *pos,
      const uint8_t *sec)
{
   const uint8_t   *ks    = sec + XXH3_SECRET - XXH3_STRIPE;
   const uint32x2_t prime = vdup_n_u32(XXH3_P32_1);
   unsigned         k     = *pos;
   uint64x2_t       P0 = vld1q_u64(acc);
   uint64x2_t       P1 = vld1q_u64(acc + 2);
   uint64x2_t       P2 = vld1q_u64(acc + 4);
   uint64x2_t       P3 = vld1q_u64(acc + 6);
   uint64x2_t       D0 = vdupq_n_u64(0);
   uint64x2_t       D1 = D0, D2 = D0, D3 = D0;
#define XXH3_SCRAMBLE4_NEON do { \
   XXH3_SCRAMBLE_NEON(P0, D0, ks); \
   XXH3_SCRAMBLE_NEON(P1, D1, ks + 16); \
   XXH3_SCRAMBLE_NEON(P2, D2, ks + 32); \
   XXH3_SCRAMBLE_NEON(P3, D3, ks + 48); \
} while (0)
   XXH3_RUN(XXH3_STRIPE_NEON, XXH3_SCRAMBLE4_NEON,
         XXH3_BLOCKS(XXH3_STRIPE_NEON, XXH3_SCRAMBLE4_NEON));
#undef XXH3_SCRAMBLE4_NEON
   vst1q_u64(acc,     vaddq_u64(P0, vextq_u64(D0, D0, 1)));
   vst1q_u64(acc + 2, vaddq_u64(P1, vextq_u64(D1, D1, 1)));
   vst1q_u64(acc + 4, vaddq_u64(P2, vextq_u64(D2, D2, 1)));
   vst1q_u64(acc + 6, vaddq_u64(P3, vextq_u64(D3, D3, 1)));
   *pos = k;
}

static void xxh3_take_neon(xxh3_state_t *s, const uint8_t *p, size_t len)
XXH3_TAKE(xxh3_run_neon)
#undef XXH3_PART_NEON
#undef XXH3_STRIPE_NEON
#undef XXH3_SCRAMBLE_NEON
#endif

/* ---- which path ---- */

#if defined(XXH3_HAVE_SSE2)
/* The path this CPU takes, plus one; 0 until the first long input */
static retro_atomic_int_t xxh3_path;

static int xxh3_choose(void)
{
   uint64_t simd = cpu_features_get();
   int      path = 1;
#if defined(XXH3_HAVE_AVX512)
   if (simd & RETRO_SIMD_AVX512)
      path = 4;
   else
#endif
#if defined(XXH3_HAVE_AVX2)
   if (simd & RETRO_SIMD_AVX2)
      path = 3;
   else
#endif
   if (simd & RETRO_SIMD_SSE2)
      path = 2;
   retro_atomic_store_release_int(&xxh3_path, path);
   return path;
}
#endif

static uint64_t xxh3_long(const uint8_t *in, size_t len, uint64_t seed)
{
#if defined(XXH3_HAVE_SSE2)
   int path = retro_atomic_load_acquire_int(&xxh3_path);
   if (!path)
      path = xxh3_choose();
#if defined(XXH3_HAVE_AVX512)
   if (path == 4)
      return xxh3_long_avx512(in, len, seed);
#endif
#if defined(XXH3_HAVE_AVX2)
   if (path == 3)
      return xxh3_long_avx2(in, len, seed);
#endif
   if (path == 2)
      return xxh3_long_sse2(in, len, seed);
   return xxh3_long_scalar(in, len, seed);
#elif defined(XXH3_HAVE_NEON)
   return xxh3_long_neon(in, len, seed);
#else
   return xxh3_long_scalar(in, len, seed);
#endif
}

uint64_t xxh3_64(const void *data, size_t len, uint64_t seed)
{
   const uint8_t *in = (const uint8_t*)data;
   if (len <= 16)
      return xxh3_0to16(in, len, seed);
   if (len <= 128)
      return xxh3_17to128(in, len, seed);
   if (len <= 240)
      return xxh3_129to240(in, len, seed);
   return xxh3_long(in, len, seed);
}

/* The path's take, which a state keeps from its reset on */
static void (*xxh3_take_fn(void))(xxh3_state_t *, const uint8_t *, size_t)
{
#if defined(XXH3_HAVE_SSE2)
   int path = retro_atomic_load_acquire_int(&xxh3_path);
   if (!path)
      path = xxh3_choose();
#if defined(XXH3_HAVE_AVX512)
   if (path == 4)
      return xxh3_take_avx512;
#endif
#if defined(XXH3_HAVE_AVX2)
   if (path == 3)
      return xxh3_take_avx2;
#endif
   if (path == 2)
      return xxh3_take_sse2;
   return xxh3_take_scalar;
#elif defined(XXH3_HAVE_NEON)
   return xxh3_take_neon;
#else
   return xxh3_take_scalar;
#endif
}

void xxh3_update(xxh3_state_t *s, const void *data, size_t len)
{
   size_t h = s->held;
   s->total += len;
   if (h + len > XXH3_HELD || len > XXH3_HOLD)
      s->take(s, (const uint8_t*)data, len);
   else
   {
      xxh3_hold(s->buf + XXH3_STRIPE + h, (const uint8_t*)data, len);
      s->held = (uint32_t)(h + len);
   }
}

void xxh3_reset(xxh3_state_t *s, uint64_t seed)
{
   memcpy(s->acc, xxh3_init_acc, sizeof(xxh3_init_acc));
   s->total = 0;
   s->seed  = seed;
   s->held  = 0;
   s->pos   = 0;
   s->take  = xxh3_take_fn();
   if (seed)
      xxh3_keys(s->sec, seed);
}

uint64_t xxh3_digest(const xxh3_state_t *s)
{
   const uint8_t *held = s->buf + XXH3_STRIPE;
   const uint8_t *sec;
   uint64_t       acc[8];
   unsigned       pos = s->pos;

   if (s->total <= 240)
      return xxh3_64(held, (size_t)s->total, s->seed);
   sec = s->seed ? s->sec : xxh3_secret;
   memcpy(acc, s->acc, sizeof(acc));
   xxh3_run_scalar(acc, held, (s->held - 1) / XXH3_STRIPE, NULL, 0, &pos,
         sec);
   /* which may start in the stripe before */
   xxh3_stripe_scalar(acc, held + s->held - XXH3_STRIPE, sec + XXH3_LAST_KEY);
   return xxh3_merge_sec(acc, sec, s->total);
}

/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (xxh3.h).
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

#ifndef _LIBRETRO_ENCODINGS_XXH3_H
#define _LIBRETRO_ENCODINGS_XXH3_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#if defined(_MSC_VER)
#include <stdlib.h>
#include <intrin.h>
#endif

#include <retro_common_api.h>
#include <retro_inline.h>
#include <retro_endianness.h>

RETRO_BEGIN_DECLS

/* ---- inputs up to 128 octets, inline: a length the compiler knows
 * folds them to the one range it is in ---- */

#if defined(_MSC_VER)
#define XXH3_FORCEINLINE __forceinline
#elif defined(__GNUC__)
#define XXH3_FORCEINLINE INLINE __attribute__((always_inline))
#else
#define XXH3_FORCEINLINE INLINE
#endif

#if defined(_MSC_VER)
#define XXH3_ROTL(x, r) _rotl64((x), (r))
#else
#define XXH3_ROTL(x, r) (((x) << (r)) | ((x) >> (64 - (r))))
#endif

#define XXH3_P32_1 0x9E3779B1U
#define XXH3_P32_2 0x85EBCA77U
#define XXH3_P32_3 0xC2B2AE3DU
#define XXH3_P64_1 0x9E3779B185EBCA87ULL
#define XXH3_P64_2 0xC2B2AE3D27D4EB4FULL
#define XXH3_P64_3 0x165667B19E3779F9ULL
#define XXH3_P64_4 0x85EBCA77C2B2AE63ULL
#define XXH3_P64_5 0x27D4EB2F165667C5ULL

/* The default secret's words these ranges read, little-endian */
#define XXH3_K0   0xBE4BA423396CFEB8ULL
#define XXH3_K8   0x1CAD21F72C81017CULL
#define XXH3_K16  0xDB979083E96DD4DEULL
#define XXH3_K24  0x1F67B3B7A4A44072ULL
#define XXH3_K32  0x78E5C0CC4EE679CBULL
#define XXH3_K40  0x2172FFCC7DD05A82ULL
#define XXH3_K48  0x8E2443F7744608B8ULL
#define XXH3_K56  0x4C263A81E69035E0ULL
#define XXH3_K64  0xCB00C391BB52283CULL
#define XXH3_K72  0xA32E531B8B65D088ULL
#define XXH3_K80  0x4EF90DA297486471ULL
#define XXH3_K88  0xD8ACDEA946EF1938ULL
#define XXH3_K96  0x3F349CE33F76FAA8ULL
#define XXH3_K104 0x1D4F0BC7C7BBDCF9ULL
#define XXH3_K112 0x3159B4CD4BE0518AULL
#define XXH3_K120 0x647378D9C97E9FC8ULL

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

static INLINE uint64_t xxh3_rrmxmx(uint64_t h, uint64_t len)
{
   h ^= XXH3_ROTL(h, 49) ^ XXH3_ROTL(h, 24);
   h *= 0x9FB21C651E98DF25ULL;
   h ^= (h >> 35) + len;
   h *= 0x9FB21C651E98DF25ULL;
   return h ^ (h >> 28);
}

static XXH3_FORCEINLINE uint64_t xxh3_mix16k(const uint8_t *in,
      uint64_t k0, uint64_t k1, uint64_t seed)
{
   return xxh3_fold(xxh3_rd64(in) ^ (k0 + seed), xxh3_rd64(in + 8) ^ (k1 - seed));
}

static XXH3_FORCEINLINE uint64_t xxh3_0to16(const uint8_t *in, size_t len,
      uint64_t seed)
{
   if (len > 8)
   {
      uint64_t lo = xxh3_rd64(in) ^ ((XXH3_K24 ^ XXH3_K32) + seed);
      uint64_t hi = xxh3_rd64(in + len - 8) ^ ((XXH3_K40 ^ XXH3_K48) - seed);
      return xxh3_avalanche((uint64_t)len + SWAP64(lo) + hi
            + xxh3_fold(lo, hi));
   }
   if (len >= 4)
   {
      uint64_t s    = seed ^ ((uint64_t)SWAP32((uint32_t)seed) << 32);
      uint64_t in64 = (uint64_t)xxh3_rd32(in + len - 4)
         + ((uint64_t)xxh3_rd32(in) << 32);
      return xxh3_rrmxmx(in64 ^ ((XXH3_K8 ^ XXH3_K16) - s), len);
   }
   if (len)
   {
      uint32_t c = ((uint32_t)in[0] << 16) | ((uint32_t)in[len >> 1] << 24)
                 | (uint32_t)in[len - 1]   | ((uint32_t)len << 8);
      return xxh3_xxh64_avalanche((uint64_t)c
            ^ ((uint64_t)((uint32_t)XXH3_K0 ^ (uint32_t)(XXH3_K0 >> 32))
               + seed));
   }
   return xxh3_xxh64_avalanche(seed ^ (XXH3_K56 ^ XXH3_K64));
}

static XXH3_FORCEINLINE uint64_t xxh3_17to128(const uint8_t *in, size_t len,
      uint64_t seed)
{
   uint64_t acc = (uint64_t)len * XXH3_P64_1;

   if (len > 32)
   {
      if (len > 64)
      {
         if (len > 96)
         {
            acc += xxh3_mix16k(in + 48,       XXH3_K96,  XXH3_K104, seed);
            acc += xxh3_mix16k(in + len - 64, XXH3_K112, XXH3_K120, seed);
         }
         acc += xxh3_mix16k(in + 32,       XXH3_K64, XXH3_K72, seed);
         acc += xxh3_mix16k(in + len - 48, XXH3_K80, XXH3_K88, seed);
      }
      acc += xxh3_mix16k(in + 16,       XXH3_K32, XXH3_K40, seed);
      acc += xxh3_mix16k(in + len - 32, XXH3_K48, XXH3_K56, seed);
   }
   acc += xxh3_mix16k(in,            XXH3_K0,  XXH3_K8,  seed);
   acc += xxh3_mix16k(in + len - 16, XXH3_K16, XXH3_K24, seed);
   return xxh3_avalanche(acc);
}

static XXH3_FORCEINLINE uint64_t xxh3_64_short(const void *data, size_t len,
      uint64_t seed)
{
   const uint8_t *in = (const uint8_t*)data;
   if (len <= 16)
      return xxh3_0to16(in, len, seed);
   return xxh3_17to128(in, len, seed);
}

/**
 * xxh3_64:
 * @data       : the octets
 * @len        : how many
 * @seed       : the seed; 0 is the unseeded hash
 *
 * The 64-bit XXH3 of @data, as xxHash specifies it (XXH3_64bits and
 * XXH3_64bits_withSeed), with its default secret. Inputs over 240
 * octets go through AVX-512, AVX2 or SSE2 where the CPU has them,
 * chosen once, or NEON; the value is the same every way and on every
 * host.
 *
 * Returns: the hash.
 */
uint64_t xxh3_64(const void *data, size_t len, uint64_t seed);

/* A length the compiler knows to be short is hashed in place */
#if defined(__GNUC__)
#define xxh3_64(data, len, seed) \
   ((__builtin_constant_p(len) && (len) <= 128) \
      ? xxh3_64_short((data), (len), (seed)) \
      : (xxh3_64)((data), (len), (seed)))
#endif

/* The same hash taken over pieces handed to it one after another,
 * XXH3_64bits_update() and its digest. Pieces are taken where they lie
 * a 64-octet stripe at a time; at most 1024 octets are held between
 * calls, after the last stripe taken. */
typedef struct xxh3_state
{
   uint64_t acc[8];        /* the accumulators */
   uint64_t total;         /* octets taken */
   uint64_t seed;
   uint8_t  sec[192];      /* the seeded secret, with a seed */
   uint8_t  buf[64 + 1024]; /* the last stripe taken, then what is held */
   uint32_t held;          /* how much is held */
   unsigned pos;           /* the next stripe's place in its block */
   /* what takes the pieces past what is held, on this CPU's path */
   void   (*take)(struct xxh3_state *s, const uint8_t *p, size_t len);
} xxh3_state_t;

/**
 * xxh3_reset:
 *
 * Starts @s over with @seed: nothing taken yet.
 */
void xxh3_reset(xxh3_state_t *s, uint64_t seed);

/**
 * xxh3_update:
 *
 * Takes @len more octets at @data, which may be any length.
 */
void xxh3_update(xxh3_state_t *s, const void *data, size_t len);

/**
 * xxh3_digest:
 *
 * Returns: xxh3_64() of what @s has taken, which may go on taking more.
 */
uint64_t xxh3_digest(const xxh3_state_t *s);

RETRO_END_DECLS

#endif

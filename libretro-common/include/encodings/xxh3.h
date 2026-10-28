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

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

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

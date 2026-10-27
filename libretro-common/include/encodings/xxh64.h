/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (xxh64.h).
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

#ifndef _LIBRETRO_ENCODINGS_XXH64_H
#define _LIBRETRO_ENCODINGS_XXH64_H

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* XXH64, as xxHash specifies it: the hash Zstandard frames are checked
 * with, and the same value for the same octets and seed on every
 * host. */

/* A hash taken over pieces handed to it one after another */
typedef struct xxh64_state
{
   uint64_t v[4];       /* the four lanes */
   uint64_t total;      /* octets taken */
   uint8_t  mem[32];    /* a stripe not yet whole */
   uint32_t memsize;    /* how much of it there is */
} xxh64_state_t;

/**
 * xxh64:
 * @data       : the octets
 * @len        : how many
 * @seed       : the seed
 *
 * Returns: the XXH64 of @data.
 */
uint64_t xxh64(const void *data, size_t len, uint64_t seed);

/**
 * xxh64_reset:
 *
 * Starts @s over with @seed: nothing taken yet.
 */
void xxh64_reset(xxh64_state_t *s, uint64_t seed);

/**
 * xxh64_update:
 *
 * Takes @len more octets at @data, which may be any length.
 */
void xxh64_update(xxh64_state_t *s, const void *data, size_t len);

/**
 * xxh64_digest:
 *
 * Returns: the XXH64 of what @s has taken, which may go on taking more.
 */
uint64_t xxh64_digest(const xxh64_state_t *s);

RETRO_END_DECLS

#endif

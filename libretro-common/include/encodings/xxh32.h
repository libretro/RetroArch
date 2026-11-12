/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (xxh32.h).
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

#ifndef _LIBRETRO_ENCODINGS_XXH32_H
#define _LIBRETRO_ENCODINGS_XXH32_H

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* XXH32, as xxHash specifies it: the same value for the same octets and
 * seed on every host, for hashes kept or compared outside the program
 * (a texture pack's names, a known image's). */

/* A hash taken over pieces handed to it one after another */
typedef struct xxh32_state
{
   uint32_t v[4];       /* the four lanes */
   uint32_t total;      /* octets taken, modulo 2^32 */
   uint32_t large;      /* 16 or more taken */
   uint8_t  mem[16];    /* a stripe not yet whole */
   uint32_t memsize;    /* how much of it there is */
} xxh32_state_t;

/**
 * xxh32:
 * @data       : the octets
 * @len        : how many
 * @seed       : the seed
 *
 * Returns: the XXH32 of @data.
 */
uint32_t xxh32(const void *data, size_t len, uint32_t seed);

/**
 * xxh32_reset:
 *
 * Starts @s over with @seed: nothing taken yet.
 */
void xxh32_reset(xxh32_state_t *s, uint32_t seed);

/**
 * xxh32_update:
 *
 * Takes @len more octets at @data, which may be any length.
 */
void xxh32_update(xxh32_state_t *s, const void *data, size_t len);

/**
 * xxh32_digest:
 *
 * Returns: the XXH32 of what @s has taken, which may go on taking more.
 */
uint32_t xxh32_digest(const xxh32_state_t *s);

RETRO_END_DECLS

#endif

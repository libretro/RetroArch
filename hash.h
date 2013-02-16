/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __RARCH_HASH_H
#define __RARCH_HASH_H

#include <stdint.h>
#include <stddef.h>

#include "msvc/msvc_compat.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// Hashes sha256 and outputs a human readable string for comparing with the cheat XML values.
void sha256_hash(char *out, const uint8_t *in, size_t size);

#ifdef HAVE_ZLIB
#ifdef WANT_RZLIB
#include "deps/miniz/zlib.h"
#else
#include <zlib.h>
#endif
static inline uint32_t crc32_calculate(const uint8_t *data, size_t length)
{
   return crc32(0, data, length);
}

static inline uint32_t crc32_adjust(uint32_t crc, uint8_t data)
{
   // zlib and nall have different assumptions on "sign" for this function.
   return ~crc32(~crc, &data, 1);
}
#else
uint32_t crc32_calculate(const uint8_t *data, size_t length);
uint32_t crc32_adjust(uint32_t crc, uint8_t data);
#endif

#endif


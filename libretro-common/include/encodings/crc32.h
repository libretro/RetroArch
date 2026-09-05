/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (crc32.h).
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

#ifndef _LIBRETRO_ENCODINGS_CRC32_H
#define _LIBRETRO_ENCODINGS_CRC32_H

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/**
 * Computes a buffer's CRC32 checksum.
 *
 * @param crc The initial CRC32 value.
 * @param buf The buffer to calculate the CRC32 checksum of.
 * @param len The length of the data in \c buf.
 * @return The CRC32 checksum of the given buffer.
 */
uint32_t encoding_crc32(uint32_t crc, const uint8_t *buf, size_t len);

/* Ogg page CRC: the same generator polynomial taken MSB-first
 * (0x04C11DB7) rather than reflected, seeded with zero and with no
 * final complement. Ogg specifies it that way, so it is not
 * interchangeable with encoding_crc32() above. */
uint32_t encoding_crc32_ogg(uint32_t crc, const uint8_t *buf, size_t len);

/* CRC-16/CCITT-FALSE: polynomial 0x1021 MSB-first, no final xor. The
 * seed is the caller's; this variant is conventionally started at
 * 0xFFFF. Sixteen bits and a different polynomial from either
 * function above, so it is not interchangeable with them. */
uint16_t encoding_crc16_ccitt(uint16_t crc, const uint8_t *buf, size_t len);

RETRO_END_DECLS

#endif

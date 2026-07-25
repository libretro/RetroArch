/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (huffman.h).
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

/* Most-significant-bit-first bit reader and canonical Huffman decoder.
 *
 * The decoder is canonical Huffman -- codes derived from code lengths
 * alone, decoded by a single table lookup on max_bits peeked bits --
 * but not with the ordering RFC 1951 uses. Here the lowest numeric
 * codes go to the LONGEST lengths, where DEFLATE gives them to the
 * shortest. For lengths {1,2,2} this produces 1, 00, 01 rather than
 * DEFLATE's 0, 10, 11. Both are valid prefix codes and neither can
 * decode the other's bit stream, so this is a dialect and not merely a
 * different way of computing the same thing.
 *
 * What is not general purpose is how a tree gets *serialised*. The two
 * reader functions below implement the two encodings used by MAME's CHD
 * container -- one for the compressed hunk map, one for the 'huff' hunk
 * codec and for A/V hunks. They are specific to those formats and are
 * not interchangeable with the code-length encoding used by DEFLATE.
 *
 * Nothing here allocates. The lookup table is supplied by the caller,
 * which lets a small tree live on the stack and a large one live inside
 * whatever handle already exists.
 *
 * All reads are byte-at-a-time, so behaviour does not depend on host
 * endianness or on the alignment of the buffer being read.
 */

#ifndef _LIBRETRO_ENCODINGS_HUFFMAN_H
#define _LIBRETRO_ENCODINGS_HUFFMAN_H

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define RHUFF_OK            0
#define RHUFF_ERROR_DATA  (-1)
#define RHUFF_ERROR_PARAM (-2)

/* Ceilings. The map tree is 16 codes at 8 bits; the 'huff' codec and the
 * A/V codec use 256 codes at 16 bits. A code index and its length share
 * one uint16_t lookup entry, five bits for the length and the rest for
 * the index, which is what caps the code count at 2048. */
#define RHUFF_MAX_CODES   2048
#define RHUFF_MAX_BITS    16

/* Entries a lookup table must hold for a given max_bits. */
#define RHUFF_LOOKUP_ENTRIES(max_bits) ((size_t)1 << (max_bits))

/**
 * rhuff_bits_t:
 *
 * Reads bits from a caller-owned buffer, most significant bit first.
 * Reading past the end yields zero bits and latches the overflow flag
 * rather than failing, so a caller can decode optimistically and test
 * once at the end.
 */
typedef struct rhuff_bits
{
   const uint8_t *data;
   size_t         size;
   size_t         offset;   /* next byte to pull into the cache  */
   uint64_t       used;     /* bits consumed, for overflow tests */
   uint32_t       cache;    /* left-aligned; bit 31 is next out   */
   int            bits;     /* valid bits held in cache           */
   int            overflow;
} rhuff_bits_t;

/**
 * rhuff_bits_init:
 * @b          : reader, supplied by the caller
 * @data       : buffer to read from, must outlive @b
 * @size       : bytes readable at @data
 */
void rhuff_bits_init(rhuff_bits_t *b, const uint8_t *data, size_t size);

/**
 * rhuff_bits_peek:
 * @b          : reader
 * @numbits    : how many bits to look at, 0 to 24
 *
 * Returns: the next @numbits bits, most significant first, right
 * aligned. Zero when @numbits is zero. Does not consume.
 */
uint32_t rhuff_bits_peek(rhuff_bits_t *b, int numbits);

/**
 * rhuff_bits_remove:
 *
 * Consumes @numbits bits previously peeked.
 */
void rhuff_bits_remove(rhuff_bits_t *b, int numbits);

/**
 * rhuff_bits_read:
 *
 * Returns: rhuff_bits_peek(), having consumed the bits.
 */
uint32_t rhuff_bits_read(rhuff_bits_t *b, int numbits);

/**
 * rhuff_bits_flush:
 * @b          : reader
 *
 * Discards the partially consumed byte and any whole bytes still held
 * in the cache, leaving the reader positioned on the next byte
 * boundary. Used where a bit stream is followed by byte-aligned data.
 *
 * Returns: the byte offset the reader is now positioned at.
 */
size_t rhuff_bits_flush(rhuff_bits_t *b);

/**
 * rhuff_bits_overflow:
 *
 * Returns: nonzero if any read ran past the end of the buffer.
 */
int rhuff_bits_overflow(const rhuff_bits_t *b);

/**
 * rhuff_dec_t:
 *
 * A decoder bound to a caller-owned lookup table. Holding the lengths
 * inline costs RHUFF_MAX_CODES bytes and keeps the struct copyable.
 */
typedef struct rhuff_dec
{
   uint16_t *lookup;
   size_t    lookup_entries;
   uint32_t  num_codes;
   uint32_t  max_bits;
   uint8_t   lengths[RHUFF_MAX_CODES];
} rhuff_dec_t;

/**
 * rhuff_dec_init:
 * @d              : decoder, supplied by the caller
 * @num_codes      : alphabet size, up to RHUFF_MAX_CODES
 * @max_bits       : longest permitted code, up to RHUFF_MAX_BITS
 * @lookup         : table of at least RHUFF_LOOKUP_ENTRIES(@max_bits)
 * @lookup_entries : entries available at @lookup
 *
 * Binds the table and clears the tree. No allocation is performed and
 * @lookup is not written until a tree is built.
 *
 * Returns: RHUFF_OK, or RHUFF_ERROR_PARAM.
 */
int rhuff_dec_init(rhuff_dec_t *d, uint32_t num_codes, uint32_t max_bits,
      uint16_t *lookup, size_t lookup_entries);

/**
 * rhuff_dec_build:
 * @d          : initialised decoder, with @d->lengths already populated
 *
 * Assigns canonical codes from the code lengths and fills the lookup
 * table. Rejects length sets that do not describe a complete tree.
 *
 * Returns: RHUFF_OK, or RHUFF_ERROR_DATA if the lengths are not a valid
 * canonical tree.
 */
int rhuff_dec_build(rhuff_dec_t *d);

/**
 * rhuff_dec_decode_one:
 * @d          : decoder with a tree built
 * @b          : reader positioned on a code
 *
 * Returns: the decoded symbol, having consumed its bits. A symbol
 * decoded past the end of @b is meaningless; test
 * rhuff_bits_overflow() once decoding finishes.
 */
uint32_t rhuff_dec_decode_one(rhuff_dec_t *d, rhuff_bits_t *b);

/**
 * rhuff_read_tree_rle:
 * @d          : initialised decoder
 * @b          : reader positioned on a serialised tree
 *
 * Reads the code lengths of a tree stored as fixed-width values with an
 * escape for runs, then builds it. This is the encoding used for the
 * compressed hunk map of a CHD.
 *
 * Returns: RHUFF_OK, or a negative RHUFF_ERROR_* code.
 */
int rhuff_read_tree_rle(rhuff_dec_t *d, rhuff_bits_t *b);

/**
 * rhuff_read_tree_packed:
 * @d          : initialised decoder
 * @b          : reader positioned on a serialised tree
 *
 * Reads the code lengths of a tree that are themselves Huffman coded by
 * a small preceding tree, then builds it. This is the encoding used by
 * the 'huff' hunk codec and by A/V hunks.
 *
 * Returns: RHUFF_OK, or a negative RHUFF_ERROR_* code.
 */
int rhuff_read_tree_packed(rhuff_dec_t *d, rhuff_bits_t *b);

/**
 * rhuff_decode_block:
 * @d          : initialised decoder
 * @b          : reader positioned on a serialised tree
 * @dst        : output buffer
 * @dst_len    : bytes to produce
 *
 * Reads a packed tree and then decodes exactly @dst_len symbols with
 * it, one byte each. This is the whole of the 'huff' hunk codec.
 *
 * Returns: RHUFF_OK, or a negative RHUFF_ERROR_* code.
 */
int rhuff_decode_block(rhuff_dec_t *d, rhuff_bits_t *b,
      uint8_t *dst, size_t dst_len);

RETRO_END_DECLS

#endif

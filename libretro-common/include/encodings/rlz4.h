/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rlz4.h).
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

#ifndef __LIBRETRO_SDK_ENCODINGS_RLZ4_H__
#define __LIBRETRO_SDK_ENCODINGS_RLZ4_H__

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

/* Clean-room decoder for the LZ4 block format.
 *
 * A block is a plain sequence of (literals, match) pairs: a token byte
 * whose high nibble is the literal length and low nibble the match
 * length, each extended by 255-run bytes when saturated, the literals
 * themselves, then a two-byte little-endian offset into the bytes
 * already produced.  There is no entropy coding, no bit buffer, and no
 * header; the final sequence is literals only.  That is the whole
 * format, which is why this file is small.
 *
 * Decoding is complete for the block format: every block the reference
 * implementation produces is accepted.  The frame format (magic,
 * descriptors, checksums) is not read here - no consumer in this tree
 * stores frames; ZSO disc images and CHD's lz4 tag both store raw
 * blocks.
 *
 * The output buffer is the back-reference window, so a block is decoded
 * in one call against the whole of what it has produced.  A block whose
 * decode would run past @dst_len stops cleanly at the boundary and
 * reports what it produced - the semantics of the reference partial
 * decode with target == capacity, which is how the one caller that
 * needs it (ZSO frames sized to exactly one uncompressed frame) uses
 * it.  A block that ends inside a sequence decodes to its longest
 * whole-sequence prefix - the reference decoder's behavior, matched
 * byte for byte - while a corrupt offset is an error, not a short
 * success.
 */

RETRO_BEGIN_DECLS

/* Error codes.  Success is the byte count, which is never negative. */
#define RLZ4_ERROR_PARAM     (-1) /* NULL pointer with nonzero length   */
#define RLZ4_ERROR_TRUNCATED (-2) /* a structure was cut short       */
#define RLZ4_ERROR_OFFSET    (-3) /* match reaches before output start  */

/**
 * rlz4_decode:
 * @dst        : output buffer
 * @dst_len    : output capacity in bytes
 * @src        : one LZ4 block
 * @src_len    : its length in bytes
 *
 * Decodes one block.  Returns the number of bytes written to @dst
 * (stopping cleanly at @dst_len if the block encodes more), or a
 * negative RLZ4_ERROR_* code.  A zero-length input decodes to zero
 * bytes.
 */
int64_t rlz4_decode(uint8_t *dst, size_t dst_len,
      const uint8_t *src, size_t src_len);

RETRO_END_DECLS

#endif

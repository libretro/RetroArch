/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rzstd.h).
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

/* Clean-room implementation of the Zstandard frame format, RFC 8878.
 *
 * The two directions are deliberately asymmetric, because what they have
 * to interoperate with is asymmetric.
 *
 * Decoding is complete: every frame the reference implementation can
 * produce is accepted, since the data this reads was written by it.
 * That covers raw, RLE and compressed blocks; literals sections in raw,
 * RLE, Huffman and Huffman-repeat modes including the four-stream
 * layout; sequence sections in predefined, RLE, FSE and repeat modes;
 * repeated offsets; skippable frames; and the optional whole-frame
 * XXH64 checksum. Dictionaries are the one omission: no format this
 * tree reads uses them, and a dictionary-compressed frame is rejected
 * rather than silently mis-decoded.
 *
 * Encoding is minimal: it produces frames any conforming decoder reads,
 * but makes no attempt to match the reference implementation's ratio or
 * its output byte-for-byte. There is no optimal parse, no long-distance
 * matching and no dictionary support. This is sized for the one caller
 * that compresses -- input replay payloads -- where the input is small,
 * the work happens off the critical path, and a few percent of ratio is
 * not worth an order of magnitude more code.
 *
 * Both directions are non-blocking and resumable in the style of
 * <encodings/deflate.h>, with a one-shot entry point for the callers
 * that already hold the whole frame and know the output size.
 *
 * What has to work, taken from what actually calls Zstandard today:
 *
 *   input/bsv/bsvmovie.c      compresses at level 3 and decompresses,
 *                             both one-shot, and needs a bound on the
 *                             compressed size before it allocates.
 *                             This is the only caller that compresses,
 *                             and the reason the encoder exists at all.
 *   cheevos/cheevos_rvz.c     decompresses one-shot, and reads a
 *                             frame's declared content size to size
 *                             its buffer.
 *   file/archive_file_zstd.c  decompresses an archive member, and
 *                             checks the magic and header size to
 *                             recognise one.
 *   formats/chd/rchd.c        decompresses a hunk, output size known
 *                             in advance.
 *   formats/libchdr/          decompresses a hunk, output size known in
 *     libchdr_zstd.c          advance, same as rchd. It reaches the
 *                             reference library through a streaming
 *                             interface when built against that, but a
 *                             hunk is one whole frame either way, so
 *                             nothing here has to stream.
 *
 * None of them uses a dictionary.
 */

#ifndef _LIBRETRO_ENCODINGS_RZSTD_H
#define _LIBRETRO_ENCODINGS_RZSTD_H

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum
{
   RZSTD_PROCESS_ERROR = -2,
   RZSTD_PROCESS_END   =  1,
   RZSTD_PROCESS_NEXT  =  0
};

/* Returned by rzstd_frame_content_size() when the frame header omits the
 * decompressed size, which is legal and means the caller must either
 * stream the frame or already know how large it is. */
#define RZSTD_CONTENT_SIZE_UNKNOWN ((int64_t)-1)
#define RZSTD_CONTENT_SIZE_ERROR   ((int64_t)-2)

/* Largest frame header this accepts, in bytes. */
#define RZSTD_FRAME_HEADER_MAX 18

/* -------- one-shot -------- */

/**
 * rzstd_frame_content_size:
 * @src        : start of a frame; RZSTD_FRAME_HEADER_MAX bytes is enough
 * @src_len    : bytes readable at @src
 *
 * Reads the decompressed size out of the frame header without decoding
 * anything.
 *
 * Returns: the size, RZSTD_CONTENT_SIZE_UNKNOWN if the header does not
 * record one, or RZSTD_CONTENT_SIZE_ERROR if @src is not a frame.
 */
int64_t rzstd_frame_content_size(const uint8_t *src, size_t src_len);

/**
 * rzstd_frame_header_size:
 * @src        : start of a frame
 * @src_len    : bytes readable at @src
 *
 * Reports how many bytes the frame header occupies, so a caller can
 * find the first block without decoding anything.
 *
 * Returns: the header size, or RZSTD_PROCESS_ERROR when @src_len is
 * too short to tell or the header is malformed.
 */
int rzstd_frame_header_size(const uint8_t *src, size_t src_len);

/**
 * rzstd_decode:
 * @dst        : output buffer
 * @dst_len    : capacity of @dst
 * @src        : one complete frame
 * @src_len    : exact length of @src
 * @wrote      : receives the number of bytes produced (may be NULL)
 *
 * Decodes a single complete frame in one call. Fails rather than
 * truncating if the frame expands past @dst_len. Trailing bytes after
 * the frame are an error, so callers splitting a buffer into several
 * frames must pass exact lengths.
 *
 * Returns: RZSTD_PROCESS_END on success, or RZSTD_PROCESS_ERROR.
 */
int rzstd_decode(uint8_t *dst, size_t dst_len,
      const uint8_t *src, size_t src_len, size_t *wrote);

/**
 * rzstd_compress_bound:
 * @src_len    : length of the input to be compressed
 *
 * Returns: an output size guaranteed to be sufficient, worst case
 * included.
 */
size_t rzstd_compress_bound(size_t src_len);

/**
 * rzstd_encode:
 * @dst        : output buffer, at least rzstd_compress_bound(@src_len)
 * @dst_len    : capacity of @dst
 * @src        : input
 * @src_len    : length of @src
 * @level      : 1 (fastest) .. 9 (best); clamped into range
 * @wrote      : receives the frame length (may be NULL)
 *
 * Produces one complete frame, content size recorded in the header and
 * no checksum. @level selects match-finder effort only; every level
 * emits the same frame shape.
 *
 * Returns: RZSTD_PROCESS_END on success, or RZSTD_PROCESS_ERROR.
 */
int rzstd_encode(uint8_t *dst, size_t dst_len,
      const uint8_t *src, size_t src_len, int level, size_t *wrote);

/* Window size the decoder is willing to allocate for a frame. Frames
 * declaring a larger window are rejected; this is well above what any
 * encoder in this tree produces and above the reference
 * implementation's default. */
#define RZSTD_WINDOW_LOG_MAX 27

/* No streaming interface is offered.
 *
 * One was declared here, both directions, for a caller that cannot hold
 * a whole frame. No caller in this tree is one: both CHD readers hold a
 * hunk entire, and every other user above starts from a complete frame.
 * Eleven entry points were declared and none was ever implemented:
 * nothing would have noticed them breaking, because nothing called
 * them. Absent is honest where declared was not.
 */

/**
 * rzstd_error_name:
 * @code       : an RZSTD_PROCESS_* value
 *
 * Returns: a short static description, never NULL.
 */
const char *rzstd_error_name(int code);

RETRO_END_DECLS

#endif

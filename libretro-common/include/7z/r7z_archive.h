/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (r7z_archive.h).
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

/* 7z container reader for libretro-common, derived from the
 * public-domain LZMA SDK by Igor Pavlov.
 *
 * This reads the archive structure and hands folder payloads to the
 * decoders in this directory: r7z_lzma, r7z_lzma2, r7z_bcj2 and
 * r7z_filters.
 *
 * Deliberately not a general 7z implementation. libretro-common opens
 * archives of ROMs, so this covers what those contain and rejects
 * everything else rather than half-supporting it:
 *
 *   - LZMA, LZMA2, Copy, Delta, BCJ (all architectures) and BCJ2
 *   - plain and encoded (compressed) headers
 *   - solid and non-solid archives, multiple folders
 *   - empty files, empty streams, directories
 *
 * Not supported, and rejected at parse time: encryption, multi-volume
 * archives, external streams, archives whose folders form anything
 * other than a simple coder chain.
 *
 * ------------------------------------------------------------------
 * On untrusted input
 * ------------------------------------------------------------------
 *
 * Everything here comes from the file. Sizes, counts, indices and
 * offsets are all attacker-controlled, and a 7z header is a nested
 * structure of variable-length integers that can claim any value they
 * like. So:
 *
 *   - every count is checked against what the remaining header could
 *     actually hold before anything is allocated for it
 *   - every offset and size is checked against the archive length, and
 *     against each other, with overflow-safe comparisons
 *   - every index into a table is bounds-checked at the point of use,
 *     not merely where it was parsed
 *   - a folder's coder graph must be a simple chain; anything else is
 *     rejected rather than followed
 *
 * The parser never trusts a length to be consistent with a count.
 */

#ifndef __LIBRETRO_SDK_R7Z_ARCHIVE_H
#define __LIBRETRO_SDK_R7Z_ARCHIVE_H

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define R7Z_OK               0
#define R7Z_ERROR_DATA     (-1)
#define R7Z_ERROR_PARAM    (-2)
#define R7Z_ERROR_MEM      (-3)
#define R7Z_ERROR_UNSUPPORTED (-4)
#define R7Z_ERROR_CRC      (-5)

/* Not an error: more calls are needed. See r7z_archive_extract_slice(). */
#define R7Z_PENDING          1

/* Signature length at the head of every archive. */
#define R7Z_SIGNATURE_SIZE 6

/* One entry in the archive. */
typedef struct r7z_entry
{
   /* UTF-16LE name as stored, NUL terminated. Owned by the archive. */
   const uint16_t *name;
   uint64_t        size;
   uint32_t        crc;
   uint32_t        has_crc;
   uint32_t        is_dir;
   /* Which folder holds this entry's data, and where inside it.
    * Meaningless for directories and empty files. */
   uint32_t        folder;
   uint64_t        offset_in_folder;
} r7z_entry_t;

typedef struct r7z_archive r7z_archive_t;

/**
 * r7z_archive_open:
 * @out        : receives the opened archive
 * @data       : the whole archive file in memory
 * @len        : length of @data
 *
 * Parses the archive structure. @data must stay valid and unmodified
 * until r7z_archive_close(); the archive borrows it rather than
 * copying.
 *
 * Returns: R7Z_OK, or a negative R7Z_ERROR_* code. A malformed or
 * unsupported archive is an error, never a partial success.
 */
int r7z_archive_open(r7z_archive_t **out,
      const uint8_t *data, size_t len);

/**
 * r7z_archive_close:
 * @a          : archive, may be NULL
 *
 * Releases everything r7z_archive_open() allocated.
 */
void r7z_archive_close(r7z_archive_t *a);

/**
 * r7z_archive_num_entries:
 * @a          : opened archive
 *
 * Returns: the number of entries, including directories.
 */
uint32_t r7z_archive_num_entries(const r7z_archive_t *a);

/**
 * r7z_archive_entry:
 * @a          : opened archive
 * @index      : entry index, below r7z_archive_num_entries()
 *
 * Returns: the entry, or NULL if @index is out of range.
 */
const r7z_entry_t *r7z_archive_entry(const r7z_archive_t *a,
      uint32_t index);

/**
 * r7z_archive_extract_slice:
 * @a          : opened archive
 * @index      : entry index
 * @out        : receives a malloc'd buffer holding the entry's data
 * @out_len    : receives the entry's size
 *
 * The same as r7z_archive_extract(), but bounded: it decodes at most a
 * slice of the entry's folder per call and returns R7Z_PENDING when
 * there is more to do. Call it again until it returns something else.
 * A 2.8 MiB solid folder takes about 67 ms decoded in one go; a slice
 * is around 1.8 ms.
 *
 * @out is written only on R7Z_OK.
 *
 * Requesting a different entry mid-decode abandons the pending work,
 * so callers should finish one entry before starting another.
 *
 * Folders whose coder chain this cannot slice (BCJ2) fall back to
 * decoding whole, so a single call may still block for that shape.
 *
 * Returns: R7Z_OK when the entry is ready, R7Z_PENDING when more calls
 * are needed, or a negative R7Z_ERROR_* code.
 */
int r7z_archive_extract_slice(r7z_archive_t *a, uint32_t index,
      uint8_t **out, size_t *out_len);

/**
 * r7z_archive_extract:
 * @a          : opened archive
 * @index      : entry index
 * @out        : receives a malloc'd buffer holding the entry's data
 * @out_len    : receives the entry's size
 *
 * Decodes the folder containing @index and returns that entry's bytes.
 * The caller owns @out and must free() it.
 *
 * Decoding a folder is all-or-nothing, so extracting several entries
 * from one solid folder decodes it once per call. Callers that want
 * the whole folder should extract in index order and cache.
 *
 * If the archive records a CRC for the entry, it is checked, and a
 * mismatch is an error.
 *
 * Returns: R7Z_OK, or a negative R7Z_ERROR_* code.
 */
int r7z_archive_extract(r7z_archive_t *a, uint32_t index,
      uint8_t **out, size_t *out_len);

RETRO_END_DECLS

#endif

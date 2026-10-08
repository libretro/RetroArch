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

/* Positioned read of @len bytes at @off into @dst; returns the bytes
 * read, 0 at the end, negative on error. */
typedef int64_t (*r7z_read_t)(void *ud, uint64_t off, void *dst,
      size_t len);

/**
 * r7z_archive_open_read:
 * @out        : receives the opened archive
 * @len        : length of the archive file
 * @read_cb    : positioned reads of it
 * @ud         : handed to @read_cb
 *
 * The same, for an archive that is not in memory and is not to be put
 * there whole: the header is read at open, and a folder's packed bytes
 * are read in when it is decoded and let go of when it has been. A
 * member is extracted as it is from an archive in memory.
 *
 * Returns: R7Z_OK, or a negative R7Z_ERROR_* code.
 */
int r7z_archive_open_read(r7z_archive_t **out, uint64_t len,
      r7z_read_t read_cb, void *ud);

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

/**
 * r7z_archive_extract_detach:
 *
 * The same as r7z_archive_extract(), except that an entry which is its
 * folder's entire output - every member of a non-solid archive - is
 * handed over as the decoded folder buffer itself, with nothing copied
 * and nothing left in the folder cache. Any other entry extracts
 * exactly as r7z_archive_extract() does.
 */
int r7z_archive_extract_detach(r7z_archive_t *a, uint32_t index,
      uint8_t **out, size_t *out_len);

/**
 * r7z_archive_entry_borrow:
 *
 * The entry's bytes inside the decoded folder cache, decoding the folder
 * first if it is not the one held. Nothing is copied: the pointer is
 * owned by the archive and stays valid until a different folder is
 * decoded, the folder is detached, or the archive is closed. The CRC is
 * checked on every call, so a caller keeping the pointer calls once.
 * A solid archive is one folder, so every member of it can be borrowed
 * at the same time.
 */
int r7z_archive_entry_borrow(r7z_archive_t *a, uint32_t index,
      const uint8_t **out, size_t *out_len);

/**
 * r7z_archive_folder_size:
 *
 * The decoded length of folder @folder (an entry's is in its
 * r7z_entry_t), or 0 if there is none. What decoding it will take.
 */
uint64_t r7z_archive_folder_size(const r7z_archive_t *a, uint32_t folder);

/**
 * r7z_archive_decode_step:
 * @a          : opened archive
 * @index      : an entry of the folder to decode
 *
 * One slice of decoding the folder that holds @index: for a thread that
 * decodes a large folder while another reads what has been decoded so
 * far, with r7z_archive_decode_peek(). Call it until it returns
 * something other than R7Z_PENDING. On R7Z_OK the folder is decoded,
 * checked against its CRC and held, as r7z_archive_entry_borrow()
 * leaves it.
 *
 * One thread, and nothing else done with @a meanwhile but
 * r7z_archive_decode_peek() from other threads: an archive decodes one
 * folder at a time. A caller that wants another folder at the same time
 * opens the archive a second time for it.
 *
 * Returns: R7Z_PENDING, R7Z_OK, or a negative R7Z_ERROR_* code.
 */
int r7z_archive_decode_step(r7z_archive_t *a, uint32_t index);

/**
 * r7z_archive_decode_peek:
 * @a          : the archive r7z_archive_decode_step() is called on
 * @index      : an entry of the folder being decoded
 * @data       : receives where the entry's bytes are, or NULL
 * @avail      : receives how many of them are there to be read
 *
 * How much of entry @index has been decoded. A folder whose last coder
 * decodes in order - LZMA or LZMA2, which is to say nearly all of them -
 * fills the buffer it will be held in from the front, and what is in it
 * is final: @data points into it, at the same place from the first call
 * to the last and for as long as the folder is held, and @avail grows
 * to the entry's size. A folder that ends in a filter gives nothing
 * until it is done, and then everything. The bytes are not checked
 * against a CRC until the folder is.
 *
 * Any thread.
 *
 * Returns: R7Z_OK, or a negative R7Z_ERROR_* code.
 */
int r7z_archive_decode_peek(const r7z_archive_t *a, uint32_t index,
      const uint8_t **data, size_t *avail);

RETRO_END_DECLS

#endif

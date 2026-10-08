/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rrar_archive.h).
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* RAR archive reader for libretro-common.
 *
 * The container and the decoder follow libarchive's RAR reader
 * (archive_read_support_format_rar.c, Tim Kientzle and Andres Mejia,
 * 2-clause BSD - see rrar_archive.c for its notice); the PPMd model next
 * to it is Igor Pavlov's public-domain Ppmd7.
 *
 * Deliberately not a general RAR implementation. libretro-common opens
 * archives of ROMs, so this covers what those are packed as and rejects
 * everything else rather than half-supporting it:
 *
 *   - the RAR 1.5 to 4.x container ("Rar!\x1a\x07\x00")
 *   - stored members of any version
 *   - members packed by RAR 2.9 to 4.x (unpack version 29 and 36), all
 *     five methods: the LZ coder with its Huffman tables, PPMd blocks,
 *     and the standard filters (delta, x86 E8 and E8/E9, RGB, audio)
 *
 * Not supported, and said so per entry or at open: the RAR 5 container,
 * members packed by RAR 1.5 or 2.x, solid archives, encryption,
 * multi-volume archives, self-extracting archives, and filter programs
 * other than the standard ones (there is no RAR virtual machine here).
 *
 * ------------------------------------------------------------------
 * On untrusted input
 * ------------------------------------------------------------------
 *
 * Everything here comes from the file. Header sizes, member sizes,
 * match distances and lengths, filter positions and the code tables are
 * all the archive's to claim. So:
 *
 *   - every header is checked against the archive's length before it is
 *     read, and against its own checksum
 *   - a member is decoded into a buffer of the size its header gives and
 *     never past it; a match that reaches back before the start of the
 *     member reads zeros
 *   - a code table that cannot be a prefix code is refused
 *   - filters are checked against the member's size before they run
 *   - what comes out is checked against the member's CRC-32
 */

#ifndef __LIBRETRO_SDK_RRAR_ARCHIVE_H
#define __LIBRETRO_SDK_RRAR_ARCHIVE_H

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define RRAR_OK                  0
#define RRAR_ERROR_DATA        (-1)
#define RRAR_ERROR_PARAM       (-2)
#define RRAR_ERROR_MEM         (-3)
#define RRAR_ERROR_UNSUPPORTED (-4)
#define RRAR_ERROR_CRC         (-5)

/* Signature length at the head of every archive. */
#define RRAR_SIGNATURE_SIZE 7

/* One entry in the archive. */
typedef struct rrar_entry
{
   /* UTF-8 name, '/' between its parts, NUL terminated. Owned by the
    * archive. */
   const char *name;
   uint64_t    size;         /* unpacked */
   uint64_t    packed_size;
   uint64_t    data_offset;  /* of the packed bytes in the archive */
   uint32_t    crc;
   uint8_t     method;       /* 0x30 stored, 0x31 to 0x35 packed */
   uint8_t     version;      /* of RAR needed to unpack: 29 is 2.9 */
   uint8_t     is_dir;
   /* 0: this reader cannot extract the entry (see the list above) */
   uint8_t     supported;
} rrar_entry_t;

typedef struct rrar_archive rrar_archive_t;

/**
 * rrar_archive_open:
 * @out        : receives the opened archive
 * @data       : the whole archive file in memory
 * @len        : length of @data
 *
 * Parses the archive's headers. @data must stay valid and unmodified
 * until rrar_archive_close(); the archive borrows it rather than
 * copying.
 *
 * Returns: RRAR_OK, or a negative RRAR_ERROR_* code. A malformed archive
 * is an error, never a partial success; an archive of a kind this reader
 * does not read (RAR 5, multi-volume, encrypted headers) is
 * RRAR_ERROR_UNSUPPORTED.
 */
int rrar_archive_open(rrar_archive_t **out,
      const uint8_t *data, size_t len);

/**
 * rrar_archive_close:
 * @a          : archive, may be NULL
 *
 * Releases everything rrar_archive_open() allocated.
 */
void rrar_archive_close(rrar_archive_t *a);

/**
 * rrar_archive_num_entries:
 * @a          : opened archive
 *
 * Returns: the number of entries, including directories.
 */
uint32_t rrar_archive_num_entries(const rrar_archive_t *a);

/**
 * rrar_archive_entry:
 * @a          : opened archive
 * @index      : entry index, below rrar_archive_num_entries()
 *
 * Returns: the entry, or NULL if @index is out of range.
 */
const rrar_entry_t *rrar_archive_entry(const rrar_archive_t *a,
      uint32_t index);

/**
 * rrar_archive_extract:
 * @a          : opened archive
 * @index      : entry index
 * @out        : receives a malloc'd buffer holding the entry's data
 * @out_len    : receives the entry's size
 *
 * Unpacks one entry and checks it against the archive's CRC-32. The
 * caller frees *@out. An empty entry yields a valid one-byte buffer and
 * a length of zero.
 *
 * Returns: RRAR_OK, or a negative RRAR_ERROR_* code, in which case
 * *@out is NULL.
 */
int rrar_archive_extract(rrar_archive_t *a, uint32_t index,
      uint8_t **out, size_t *out_len);

RETRO_END_DECLS

#endif

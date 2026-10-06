/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rzip_archive.h).
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

#ifndef __LIBRETRO_SDK_RZIP_ARCHIVE_H
#define __LIBRETRO_SDK_RZIP_ARCHIVE_H

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Clean-room reader for the zip container: the central directory, zip64
 * records and extra fields, stored and deflated members. It performs no
 * file I/O of its own: the archive is either handed over whole in memory
 * or read through a positioned-read callback, which is what lets a
 * mapped archive serve stored members in place and an unmapped one be
 * walked without being loaded. */

#define RZIP_OK                 0
#define RZIP_ERROR_DATA       (-1)
#define RZIP_ERROR_PARAM      (-2)
#define RZIP_ERROR_MEM        (-3)
#define RZIP_ERROR_UNSUPPORTED (-4)
#define RZIP_ERROR_CRC        (-5)
#define RZIP_ERROR_IO         (-6)

#define RZIP_METHOD_STORED    0
#define RZIP_METHOD_DEFLATE   8

typedef struct rzip_entry
{
   /* Name as stored (UTF-8 or the archiver's code page), NUL terminated.
    * Owned by the archive. */
   const char *name;
   uint64_t    size;      /* uncompressed */
   uint64_t    csize;     /* bytes in the archive */
   uint64_t    data_off;  /* where those bytes start in the archive */
   uint32_t    crc;
   uint32_t    method;    /* RZIP_METHOD_* or another the format allows */
   uint32_t    is_dir;
} rzip_entry_t;

typedef struct rzip_archive rzip_archive_t;

/* Positioned read of @len bytes at @off into @dst; returns the bytes
 * read, 0 at the end, negative on error. */
typedef int64_t (*rzip_read_t)(void *ud, uint64_t off, void *dst,
      size_t len);

/**
 * rzip_archive_open:
 * @out        : receives the archive
 * @data       : the whole archive in memory, or NULL
 * @len        : length of the archive in bytes, in memory or not
 * @read_cb    : positioned reads of the archive when @data is NULL
 * @ud         : handed to @read_cb
 *
 * Parses the central directory and locates every member's data. With
 * @data the bytes must stay valid and unmodified until
 * rzip_archive_close(); with @read_cb only the directory and each
 * member's local header are read, at open.
 *
 * Returns: RZIP_OK, or a negative RZIP_ERROR_* code.
 */
int rzip_archive_open(rzip_archive_t **out, const uint8_t *data,
      uint64_t len, rzip_read_t read_cb, void *ud);

void rzip_archive_close(rzip_archive_t *a);

uint32_t rzip_archive_num_entries(const rzip_archive_t *a);

const rzip_entry_t *rzip_archive_entry(const rzip_archive_t *a,
      uint32_t index);

/* Index of the member named @name exactly, or -1. */
int rzip_archive_find(const rzip_archive_t *a, const char *name);

/**
 * rzip_archive_entry_view:
 *
 * A stored member of an in-memory archive, in place: a pointer into
 * @data, valid as long as @data is. NULL for a deflated member, a
 * member of an archive read through a callback, or one whose data lies
 * outside the archive.
 */
const uint8_t *rzip_archive_entry_view(const rzip_archive_t *a,
      uint32_t index, size_t *len);

/**
 * rzip_archive_extract:
 * @out        : receives a malloc'd buffer holding the member, which the
 *               caller frees
 * @out_len    : receives its length
 *
 * The member decoded into its own buffer: a stored one copied, a
 * deflated one inflated, either checked against the directory's CRC.
 * A member of a size this build cannot address, or of a method it
 * cannot decode, is refused.
 */
int rzip_archive_extract(rzip_archive_t *a, uint32_t index,
      uint8_t **out, size_t *out_len);

RETRO_END_DECLS

#endif

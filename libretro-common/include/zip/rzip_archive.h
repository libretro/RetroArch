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

/**
 * rzip_archive_extract_into:
 * @dst        : where the member goes
 * @dst_size   : how much room there is at @dst
 * @out_len    : receives the member's length
 *
 * The same as rzip_archive_extract(), into memory the caller has: for a
 * member whose bytes have a place of their own to go to, which would
 * otherwise be decoded into a buffer and copied from it. A member
 * longer than @dst_size is refused with RZIP_ERROR_PARAM and nothing is
 * written; on any other error @dst may hold part of the member.
 */
int rzip_archive_extract_into(rzip_archive_t *a, uint32_t index,
      uint8_t *dst, size_t dst_size, size_t *out_len);

/* ---- a deflated member read at any offset, without decoding it whole ----
 *
 * A deflate stream can only be decoded from its start, so reading the
 * end of a large member means decoding all of it - and keeping all of
 * it, if the middle is wanted next. The index remembers restart points
 * along the stream instead: at a block boundary, the position in the
 * compressed bytes and the 32 KiB of output before it are all it takes
 * to decode from there. With a point about every @span bytes, any byte
 * of the member is within one span's decoding of a point, and nothing
 * but the points (32 KiB each) and a few decoded spans is kept.
 *
 * The points are found by decoding the member through once: that is
 * rzip_seek_build(), which may be called a piece at a time, and on a
 * thread of its own while another thread reads what is already covered.
 * One thread builds and one thread reads; the two share only the points
 * already published. (An archive opened with a read callback has both
 * calling it, so there the two must be the same thread unless the
 * callback takes that.)
 */
typedef struct rzip_seek rzip_seek_t;

#define RZIP_SEEK_SPAN_DEFAULT (1024 * 1024)

/* Member @index of @a, which must outlive the index. NULL unless it is a
 * deflated file. @span: bytes between restart points, 0 for the default. */
rzip_seek_t *rzip_seek_new(rzip_archive_t *a, uint32_t index, uint32_t span);
void rzip_seek_free(rzip_seek_t *s);

/* The member's decoded length. */
uint64_t rzip_seek_size(const rzip_seek_t *s);

/* Decode on from where the index ends until it covers @upto bytes of the
 * member, or all of it. Returns 1 when the whole member is indexed (and
 * its checksum was right), 0 when there is more to do, and a negative
 * RZIP_ERROR_ when the stream is bad - nothing more is covered then. */
int rzip_seek_build(rzip_seek_t *s, uint64_t upto);

/* How much of the member, from its start, can be read now. Any thread. */
uint64_t rzip_seek_covered(const rzip_seek_t *s);

/* Where the index stands: 0 while there is more to index, 1 when the
 * whole member is, a negative RZIP_ERROR_ once the stream has turned out
 * bad. Any thread. */
int rzip_seek_state(const rzip_seek_t *s);

/* @len bytes at @offset to @dst. Returns RZIP_OK; RZIP_ERROR_PARAM when
 * the index does not cover them yet (or they are past the end); another
 * RZIP_ERROR_ when they cannot be decoded. */
int rzip_seek_read(rzip_seek_t *s, uint64_t offset, uint8_t *dst, size_t len);

/* @len bytes at @offset where they lie decoded, for a reader that would
 * only copy them on: *@data points at them until the next call on @s, or
 * is NULL when they straddle two spans and have to be read instead.
 * Returns as rzip_seek_read() does. The reader's thread. */
int rzip_seek_view(rzip_seek_t *s, uint64_t offset, size_t len,
      const uint8_t **data);

RETRO_END_DECLS

#endif

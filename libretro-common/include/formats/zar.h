/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (zar.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_ZAR_H
#define __LIBRETRO_SDK_FORMAT_ZAR_H

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* ZArchive, a file tree stored as one stream cut into 64 KiB blocks,
 * each kept as it is or compressed with Zstandard (which needs
 * HAVE_RZSTD).
 *
 * The reader does no file I/O of its own and holds no file. Each range
 * of the file it needs is described to the caller as a request, which
 * names where the octets go: the caller puts up to @length octets from
 * @offset there, however it gets them, and says how many with
 * zar_feed(). A short supply is legal and asked for again; none at all
 * fails the operation.
 *
 *    zar_request_t req;
 *    int           err;
 *
 *    while ((err = zar_open_step(z, &req)) == ZAR_PENDING)
 *       zar_feed(z, pull(req.offset, req.dst, req.length));
 *
 * Nothing is read that an operation does not look at: a lookup reads
 * the entries and names its binary search of each directory probes, a
 * listing a directory's entries a run at a time, a block's place comes
 * from its one record. A read of a block kept as it is asks for its
 * octets straight into the read's destination; a
 * compressed block is decoded into the destination when the read
 * wants all of it, and otherwise into a block kept for the reads that
 * follow. */

#define ZAR_OK                 0
#define ZAR_PENDING            1
#define ZAR_END                2   /* a listing is over */
#define ZAR_ERROR_DATA       (-1)
#define ZAR_ERROR_PARAM      (-2)
#define ZAR_ERROR_MEM        (-3)
#define ZAR_ERROR_STATE      (-4)
#define ZAR_ERROR_NOT_FOUND  (-5)
#define ZAR_ERROR_UNSUPPORTED (-6)
#define ZAR_ERROR_WRITE      (-7)

typedef struct zar zar_t;

/**
 * zar_request_t:
 *
 * A range of the file the reader needs, and where to put it. Valid
 * until the next call on the same zar_t.
 */
typedef struct zar_request
{
   uint64_t offset;   /* in the file */
   size_t   length;   /* octets wanted; fewer may be supplied */
   uint8_t *dst;      /* where they go */
} zar_request_t;

/* A file in the archive, or a directory: a file's octets in the
 * stream, a directory's first entry's node and its entry count */
typedef struct zar_file
{
   uint64_t offset;
   uint64_t size;
   unsigned dir;
} zar_file_t;

/* @file_size: the archive's size, which its footer must agree with */
zar_t *zar_new(uint64_t file_size);

void zar_free(zar_t *z);

/**
 * zar_open_step:
 *
 * Reads the archive's footer and the root of its tree.
 *
 * Returns: ZAR_OK once open, ZAR_PENDING with @req filled in when it
 * needs a range of the file, or a negative ZAR_ERROR_* code when the
 * file is not one this reads.
 **/
int zar_open_step(zar_t *z, zar_request_t *req);

/**
 * zar_feed:
 * @len : octets put at the last request's @dst
 *
 * Returns: ZAR_OK, ZAR_ERROR_DATA when @len is 0 (the operation is
 * then over), or ZAR_ERROR_STATE with no request outstanding.
 **/
int zar_feed(zar_t *z, size_t len);

/**
 * zar_lookup_begin:
 * @path : '/' or '\' separated, matched without regard to ASCII case;
 *         it must stay valid until the lookup is over. A file or a
 *         directory; an empty path is the root.
 *
 * Returns: ZAR_OK, or ZAR_ERROR_STATE before the archive is open.
 **/
int zar_lookup_begin(zar_t *z, const char *path);

/**
 * zar_lookup_step:
 *
 * Returns: ZAR_OK with @out filled in once the file is found,
 * ZAR_PENDING with @req filled in when it needs a range of the file,
 * ZAR_ERROR_NOT_FOUND, or another negative code.
 **/
int zar_lookup_step(zar_t *z, zar_request_t *req, zar_file_t *out);

/* A directory's entry. @name is terminated, and valid until the next
 * call on the same zar_t. */
typedef struct zar_dirent
{
   const char *name;
   size_t      len;
   int         is_dir;
   zar_file_t  file;
} zar_dirent_t;

/**
 * zar_dir_begin:
 * @path : the directory, as for zar_lookup_begin(); "" for the root
 *
 * Arms a listing of a directory, abandoning a read, lookup or listing
 * in progress.
 *
 * Returns: ZAR_OK, or ZAR_ERROR_STATE before the archive is open.
 **/
int zar_dir_begin(zar_t *z, const char *path);

/**
 * zar_dir_step:
 *
 * Returns: ZAR_OK with @ent filled in for each entry in name order,
 * ZAR_END past the last one, ZAR_PENDING with @req filled in
 * when it needs a range of the file, ZAR_ERROR_NOT_FOUND when @path is
 * no directory, or another negative code.
 **/
int zar_dir_step(zar_t *z, zar_request_t *req, zar_dirent_t *ent);

/* Where the octets past the stream's blocks start, from which a caller
 * can keep the rest of the file in memory and answer every request a
 * lookup or listing makes without I/O; 0 before the archive is open */
uint64_t zar_index_offset(const zar_t *z);

/**
 * zar_read_begin:
 *
 * Arms a read of @len octets of @file at @off into @dst, short at the
 * end of the file, abandoning a read or lookup in progress.
 *
 * Returns: ZAR_OK, ZAR_ERROR_PARAM for a directory, or ZAR_ERROR_STATE
 * before the archive is open.
 **/
int zar_read_begin(zar_t *z, const zar_file_t *file, uint64_t off,
      void *dst, size_t len);

/**
 * zar_read_step:
 *
 * Returns: ZAR_OK when the read is complete, ZAR_PENDING with @req
 * filled in when it needs a range of the file, ZAR_ERROR_UNSUPPORTED
 * for a compressed block without HAVE_RZSTD, or another negative code.
 **/
int zar_read_step(zar_t *z, zar_request_t *req);

/* octets the armed read has put at its @dst so far */
size_t zar_read_progress(const zar_t *z);

/* The writer, in zar_write.c: an archive built front to back, every
 * octet handed to @write as it is made. Blocks are compressed with
 * Zstandard at @level (1 to 9) where that shrinks them and HAVE_RZSTD
 * is defined, and kept as they are otherwise. Paths are matched without
 * regard to ASCII case, as the reader matches them. */

typedef struct zar_writer zar_writer_t;

/* Returns nonzero when all @len octets were written */
typedef int (*zar_write_fn)(void *ud, const void *data, size_t len);

zar_writer_t *zar_writer_new(zar_write_fn write, void *ud, int level);

void zar_writer_free(zar_writer_t *w);

/**
 * zar_writer_dir:
 *
 * Adds directory @path and those above it that are missing.
 *
 * Returns: ZAR_OK, or ZAR_ERROR_PARAM when a part of it is a file.
 **/
int zar_writer_dir(zar_writer_t *w, const char *path);

/**
 * zar_writer_file:
 *
 * Adds file @path, the directories above it that are missing, and
 * makes it the one zar_writer_append() adds to.
 *
 * Returns: ZAR_OK, or ZAR_ERROR_PARAM when it is there already or a
 * part above it is a file.
 **/
int zar_writer_file(zar_writer_t *w, const char *path);

int zar_writer_append(zar_writer_t *w, const void *data, size_t len);

/**
 * zar_writer_finish:
 *
 * Writes the rest of the archive.
 *
 * Returns: ZAR_OK, ZAR_ERROR_WRITE when @write failed, or
 * ZAR_ERROR_MEM.
 **/
int zar_writer_finish(zar_writer_t *w);

RETRO_END_DECLS

#endif

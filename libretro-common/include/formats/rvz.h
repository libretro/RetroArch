/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rvz.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RVZ_H
#define __LIBRETRO_SDK_FORMAT_RVZ_H

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* RVZ and WIA, the GameCube and Wii disc image formats, read as the
 * disc image they stand for: every read returns the octets an ISO of
 * the same disc holds at that offset. Wii partitions, stored decrypted
 * and without their hashes, come back hashed and encrypted, with the
 * file's hash exceptions applied; padding stored as a generator seed
 * comes back as the padding.
 *
 * Groups stored uncompressed, with bzip2, LZMA or LZMA2, with
 * Zstandard (RVZ, needing HAVE_RZSTD) or PURGE (WIA) are read.
 *
 * The reader does no file I/O of its own and holds no file. Each range
 * of the file it needs is described to the caller as a request, which
 * names where the octets go: the caller puts up to @length octets from
 * @offset there, however it gets them, and says how many with
 * rvz_feed(). A short supply is legal and asked for again; none at all
 * fails the operation.
 *
 *    rvz_request_t req;
 *    int           err;
 *
 *    while ((err = rvz_open_step(rvz, &req)) == RVZ_PENDING)
 *       rvz_feed(rvz, pull(req.offset, req.dst, req.length));
 *
 *    rvz_read_begin(rvz, offset, dst, len);
 *    while ((err = rvz_read_step(rvz, &req)) == RVZ_PENDING)
 *       rvz_feed(rvz, pull(req.offset, req.dst, req.length));
 *
 * Where a read can be served from the file as it is, the request's
 * @dst is the read's own destination, so nothing is copied twice. */

#define RVZ_OK                  0
#define RVZ_PENDING             1
#define RVZ_ERROR_DATA        (-1)
#define RVZ_ERROR_PARAM       (-2)
#define RVZ_ERROR_MEM         (-3)
#define RVZ_ERROR_UNSUPPORTED (-4)
#define RVZ_ERROR_STATE       (-5)

#define RVZ_DISC_GAMECUBE 1
#define RVZ_DISC_WII      2

typedef struct rvz rvz_t;

/**
 * rvz_request_t:
 *
 * A range of the file the reader needs, and where to put it. Valid
 * until the next call on the same rvz_t.
 */
typedef struct rvz_request
{
   uint64_t offset;   /* in the file */
   size_t   length;   /* octets wanted; fewer may be supplied */
   uint8_t *dst;      /* where they go */
} rvz_request_t;

rvz_t *rvz_new(void);

void rvz_free(rvz_t *rvz);

/**
 * rvz_open_step:
 *
 * Reads the file's heads and tables.
 *
 * Returns: RVZ_OK once the image is open, RVZ_PENDING with @req filled
 * in when it needs a range of the file, or a negative RVZ_ERROR_* code
 * when the file is not one this reads.
 **/
int rvz_open_step(rvz_t *rvz, rvz_request_t *req);

/**
 * rvz_feed:
 * @len : octets put at the last request's @dst
 *
 * Returns: RVZ_OK, RVZ_ERROR_DATA when @len is 0 (the operation is
 * then over), or RVZ_ERROR_STATE with no request outstanding.
 **/
int rvz_feed(rvz_t *rvz, size_t len);

/* RVZ_DISC_GAMECUBE, RVZ_DISC_WII, or 0 when the file does not say */
int rvz_disc_type(const rvz_t *rvz);

/* the size of the disc image */
uint64_t rvz_disc_size(const rvz_t *rvz);

/**
 * rvz_read_begin:
 *
 * Arms a read of @len octets of the disc image at @off into @dst,
 * short at the end of the image. Nothing is read until
 * rvz_read_step(). An armed read abandons one still in progress.
 *
 * Returns: RVZ_OK, or RVZ_ERROR_STATE before the image is open.
 **/
int rvz_read_begin(rvz_t *rvz, uint64_t off, void *dst, size_t len);

/**
 * rvz_read_step:
 *
 * Returns: RVZ_OK when the read is complete, RVZ_PENDING with @req
 * filled in when it needs a range of the file, or a negative
 * RVZ_ERROR_* code.
 **/
int rvz_read_step(rvz_t *rvz, rvz_request_t *req);

/* octets the armed read has put at its @dst so far */
size_t rvz_read_progress(const rvz_t *rvz);

RETRO_END_DECLS

#endif

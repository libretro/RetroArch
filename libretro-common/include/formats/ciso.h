/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (ciso.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_CISO_H
#define __LIBRETRO_SDK_FORMAT_CISO_H

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* CISO, the GameCube and Wii disc image format that leaves out unused
 * blocks, read as the disc image it stands for: every read returns
 * the octets an ISO of the same disc holds at that offset, zeros for
 * the blocks left out. The image ends with its last stored block.
 *
 * The reader does no file I/O of its own and holds no file. Each range
 * of the file it needs is described to the caller as a request, which
 * names where the octets go: the caller puts up to @length octets from
 * @offset there, however it gets them, and says how many with
 * ciso_feed(). A short supply is legal and asked for again; none at
 * all fails the operation.
 *
 *    ciso_request_t req;
 *    int            err;
 *
 *    while ((err = ciso_open_step(c, &req)) == CISO_PENDING)
 *       ciso_feed(c, pull(req.offset, req.dst, req.length));
 *
 *    ciso_read_begin(c, offset, dst, len);
 *    while ((err = ciso_read_step(c, &req)) == CISO_PENDING)
 *       ciso_feed(c, pull(req.offset, req.dst, req.length));
 *
 * A read's requests always point into the read's own destination, one
 * request per run of stored blocks, so nothing is copied twice. */

#define CISO_OK            0
#define CISO_PENDING       1
#define CISO_ERROR_DATA  (-1)
#define CISO_ERROR_PARAM (-2)
#define CISO_ERROR_MEM   (-3)
#define CISO_ERROR_STATE (-4)

typedef struct ciso ciso_t;

/**
 * ciso_request_t:
 *
 * A range of the file the reader needs, and where to put it. Valid
 * until the next call on the same ciso_t.
 */
typedef struct ciso_request
{
   uint64_t offset;   /* in the file */
   size_t   length;   /* octets wanted; fewer may be supplied */
   uint8_t *dst;      /* where they go */
} ciso_request_t;

ciso_t *ciso_new(void);

void ciso_free(ciso_t *c);

/**
 * ciso_open_step:
 *
 * Reads the file's head and block map.
 *
 * Returns: CISO_OK once the image is open, CISO_PENDING with @req
 * filled in when it needs a range of the file, or a negative
 * CISO_ERROR_* code when the file is not one this reads.
 **/
int ciso_open_step(ciso_t *c, ciso_request_t *req);

/**
 * ciso_feed:
 * @len : octets put at the last request's @dst
 *
 * Returns: CISO_OK, CISO_ERROR_DATA when @len is 0 (the operation is
 * then over), or CISO_ERROR_STATE with no request outstanding.
 **/
int ciso_feed(ciso_t *c, size_t len);

/* the size of a block, in octets */
uint32_t ciso_block_size(const ciso_t *c);

/* the size of the disc image, through the last stored block */
uint64_t ciso_disc_size(const ciso_t *c);

/**
 * ciso_read_begin:
 *
 * Arms a read of @len octets of the disc image at @off into @dst,
 * short at the end of the image. Nothing is read until
 * ciso_read_step(). An armed read abandons one still in progress.
 *
 * Returns: CISO_OK, or CISO_ERROR_STATE before the image is open.
 **/
int ciso_read_begin(ciso_t *c, uint64_t off, void *dst, size_t len);

/**
 * ciso_read_step:
 *
 * Returns: CISO_OK when the read is complete, CISO_PENDING with @req
 * filled in when it needs a range of the file, or a negative
 * CISO_ERROR_* code.
 **/
int ciso_read_step(ciso_t *c, ciso_request_t *req);

/* octets the armed read has put at its @dst so far */
size_t ciso_read_progress(const ciso_t *c);

RETRO_END_DECLS

#endif

/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (xdvdfs.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_XDVDFS_H
#define __LIBRETRO_SDK_FORMAT_XDVDFS_H

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* XDVDFS (GDFX), the file system of Xbox and Xbox 360 game discs, and
 * the title of the executable a disc boots: default.xex on an Xbox 360
 * disc, default.xbe on an Xbox one. The game partition is found where a
 * whole disc image keeps it (XGD1, XGD2, XGD3) or at the start of the
 * image, as a trimmed XISO has it. A title is also read from an XEX,
 * an XBE or an STFS package (CON, LIVE, PIRS) on its own.
 *
 * The reader does no file I/O of its own and holds no file. Each range
 * of the image it needs is described to the caller as a request, which
 * names where the octets go: the caller puts up to @length octets from
 * @offset there, however it gets them, and says how many with
 * xdvdfs_feed(). A short supply is legal and asked for again; none at
 * all fails the operation.
 *
 *    xdvdfs_request_t req;
 *    int              err;
 *
 *    while ((err = xdvdfs_open_step(x, &req)) == XDVDFS_PENDING)
 *       xdvdfs_feed(x, pull(req.offset, req.dst, req.length));
 *
 * A directory is searched one entry at a time, as its tree leads, and
 * nothing is read that the search does not look at. */

#define XDVDFS_OK               0
#define XDVDFS_PENDING          1
#define XDVDFS_ERROR_DATA     (-1)
#define XDVDFS_ERROR_PARAM    (-2)
#define XDVDFS_ERROR_MEM      (-3)
#define XDVDFS_ERROR_STATE    (-4)
#define XDVDFS_ERROR_NOT_FOUND (-5)

#define XDVDFS_XBOX    1
#define XDVDFS_XBOX360 2

typedef struct xdvdfs xdvdfs_t;

/**
 * xdvdfs_request_t:
 *
 * A range of the image the reader needs, and where to put it. Valid
 * until the next call on the same xdvdfs_t.
 */
typedef struct xdvdfs_request
{
   uint64_t offset;   /* in the image */
   size_t   length;   /* octets wanted; fewer may be supplied */
   uint8_t *dst;      /* where they go */
} xdvdfs_request_t;

/* A file's octets in the image */
typedef struct xdvdfs_extent
{
   uint64_t offset;
   uint32_t size;
} xdvdfs_extent_t;

typedef struct xdvdfs_title
{
   uint32_t id;
   uint8_t  platform;   /* XDVDFS_XBOX or XDVDFS_XBOX360 */
   uint8_t  disc;       /* of the set, from 1; 0 when not said */
   uint8_t  discs;      /* in the set; 0 when not said */
} xdvdfs_title_t;

/* @image_size: the image's size, so a partition past its end is not
 * looked for */
xdvdfs_t *xdvdfs_new(uint64_t image_size);

void xdvdfs_free(xdvdfs_t *x);

/**
 * xdvdfs_open_step:
 *
 * Finds the game partition and its root directory.
 *
 * Returns: XDVDFS_OK once found, XDVDFS_PENDING with @req filled in
 * when it needs a range of the image, XDVDFS_ERROR_NOT_FOUND when the
 * image holds no game partition, or another negative code.
 **/
int xdvdfs_open_step(xdvdfs_t *x, xdvdfs_request_t *req);

/**
 * xdvdfs_feed:
 * @len : octets put at the last request's @dst
 *
 * Returns: XDVDFS_OK, XDVDFS_ERROR_DATA when @len is 0 (the operation
 * is then over), or XDVDFS_ERROR_STATE with no request outstanding.
 **/
int xdvdfs_feed(xdvdfs_t *x, size_t len);

/* where the game partition starts in the image */
uint64_t xdvdfs_partition(const xdvdfs_t *x);

/**
 * xdvdfs_lookup_begin:
 * @path : '/' or '\' separated, matched without regard to case; it
 *         must stay valid until the lookup is over
 *
 * Arms a lookup of a file, abandoning one in progress.
 *
 * Returns: XDVDFS_OK, or XDVDFS_ERROR_STATE before the image is open.
 **/
int xdvdfs_lookup_begin(xdvdfs_t *x, const char *path);

/**
 * xdvdfs_lookup_step:
 *
 * Returns: XDVDFS_OK with @out filled in once the file is found,
 * XDVDFS_PENDING with @req filled in when it needs a range of the
 * image, XDVDFS_ERROR_NOT_FOUND, or another negative code.
 **/
int xdvdfs_lookup_step(xdvdfs_t *x, xdvdfs_request_t *req,
      xdvdfs_extent_t *out);

/**
 * xdvdfs_title_begin:
 * @exe : the executable or package to read, or NULL for the one the
 *        open disc boots
 *
 * Arms a read of a title, abandoning one in progress. With @exe the
 * image need not be open.
 *
 * Returns: XDVDFS_OK, or XDVDFS_ERROR_STATE with no @exe before the
 * image is open.
 **/
int xdvdfs_title_begin(xdvdfs_t *x, const xdvdfs_extent_t *exe);

/**
 * xdvdfs_title_step:
 *
 * Returns: XDVDFS_OK with @title filled in, XDVDFS_PENDING with @req
 * filled in when it needs a range of the image,
 * XDVDFS_ERROR_NOT_FOUND when the disc boots neither executable or
 * @exe is none of them, or another negative code.
 **/
int xdvdfs_title_step(xdvdfs_t *x, xdvdfs_request_t *req,
      xdvdfs_title_t *title);

RETRO_END_DECLS

#endif

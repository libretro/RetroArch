/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (wux.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_WUX_H
#define __LIBRETRO_SDK_FORMAT_WUX_H

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* WUX, the Wii U disc image format that keeps each distinct sector
 * once, read as the WUD (the plain disc image) it stands for: every
 * read returns the octets the WUD holds at that offset.
 *
 * The reader does no file I/O of its own and holds no file. Each range
 * of the file it needs is described to the caller as a request, which
 * names where the octets go: the caller puts up to @length octets from
 * @offset there, however it gets them, and says how many with
 * wux_feed(). A short supply is legal and asked for again; none at all
 * fails the operation.
 *
 *    wux_request_t req;
 *    int           err;
 *
 *    while ((err = wux_open_step(w, &req)) == WUX_PENDING)
 *       wux_feed(w, pull(req.offset, req.dst, req.length));
 *
 *    wux_read_begin(w, offset, dst, len);
 *    while ((err = wux_read_step(w, &req)) == WUX_PENDING)
 *       wux_feed(w, pull(req.offset, req.dst, req.length));
 *
 * Opening reads the 32-octet header alone. The sector index is read a
 * window at a time, only where a read needs it, and a read's data
 * requests point into the read's own destination, one per run of
 * sectors the file keeps in order, so nothing is copied twice. */

#define WUX_OK            0
#define WUX_PENDING       1
#define WUX_ERROR_DATA  (-1)
#define WUX_ERROR_PARAM (-2)
#define WUX_ERROR_MEM   (-3)
#define WUX_ERROR_STATE (-4)

/* the octets wux_is_wux() looks at */
#define WUX_MAGIC_SIZE 8

typedef struct wux wux_t;

/**
 * wux_request_t:
 *
 * A range of the file the reader needs, and where to put it. Valid
 * until the next call on the same wux_t.
 */
typedef struct wux_request
{
   uint64_t offset;   /* in the file */
   size_t   length;   /* octets wanted; fewer may be supplied */
   uint8_t *dst;      /* where they go */
} wux_request_t;

/* Whether the first WUX_MAGIC_SIZE octets of a file are a WUX's */
int wux_is_wux(const uint8_t *head);

/* @file_size: the file's size, which the header must fit */
wux_t *wux_new(uint64_t file_size);

void wux_free(wux_t *w);

/**
 * wux_open_step:
 *
 * Reads the file's header.
 *
 * Returns: WUX_OK once the image is open, WUX_PENDING with @req filled
 * in when it needs a range of the file, or a negative WUX_ERROR_* code
 * when the file is not one this reads.
 **/
int wux_open_step(wux_t *w, wux_request_t *req);

/**
 * wux_feed:
 * @len : octets put at the last request's @dst
 *
 * Returns: WUX_OK, WUX_ERROR_DATA when @len is 0 (the operation is
 * then over), or WUX_ERROR_STATE with no request outstanding.
 **/
int wux_feed(wux_t *w, size_t len);

/* the size of the WUD the image stands for */
uint64_t wux_disc_size(const wux_t *w);

/**
 * wux_read_begin:
 *
 * Arms a read of @len octets of the WUD at @off into @dst, short at
 * its end. Nothing is read until wux_read_step(). An armed read
 * abandons one still in progress.
 *
 * Returns: WUX_OK, or WUX_ERROR_STATE before the image is open.
 **/
int wux_read_begin(wux_t *w, uint64_t off, void *dst, size_t len);

/**
 * wux_read_step:
 *
 * Returns: WUX_OK when the read is complete, WUX_PENDING with @req
 * filled in when it needs a range of the file, or a negative
 * WUX_ERROR_* code.
 **/
int wux_read_step(wux_t *w, wux_request_t *req);

/* octets the armed read has put at its @dst so far */
size_t wux_read_progress(const wux_t *w);

RETRO_END_DECLS

#endif

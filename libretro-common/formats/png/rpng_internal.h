/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rpng_internal.h).
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

#ifndef _RPNG_COMMON_H
#define _RPNG_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <filters.h>
#include <formats/rpng.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

static const uint8_t png_magic[8] = {
   0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a,
};

/* A run of compressed image data in a caller's buffer: payload offset
 * and length.  An APNG frame is a list of these, its IDAT/fdAT
 * payloads as they lie in the file. */
struct rpng_ext_span
{
   size_t off;
   size_t len;
};

/* Buffers a decode can hand back for the next one to take instead of
 * allocating, for a caller that decodes many images of similar size in
 * a row - an APNG stream, every frame.  One per role; a slot holds at
 * most one buffer, kept if it is the larger.  See rpng.c. */
enum rpng_pool_slot
{
   RPNG_POOL_RING = 0,   /* the inflate ring                         */
   RPNG_POOL_LINES,      /* the three out-of-place scanlines, one block */
   RPNG_POOL_PASS,       /* the Adam7 pass buffer                     */
   RPNG_POOL_OUT,        /* the decoded image, given back by the caller */
   RPNG_POOL_SLOTS
};

struct trans_stream_backend;

struct rpng_pool
{
   void  *buf[RPNG_POOL_SLOTS];
   size_t cap[RPNG_POOL_SLOTS];
   /* An inflate stream finished with, kept for the next decode when
    * its backend can reset one (trans_stream_backend.reset). */
   void  *stream;
   const struct trans_stream_backend *stream_backend;
};

void  rpng_set_pool(rpng_t *rpng, struct rpng_pool *pool);
void *rpng_pool_take(struct rpng_pool *pool, int slot, size_t size);
void  rpng_pool_give(struct rpng_pool *pool, int slot, void *p,
      size_t size);
void  rpng_pool_release(struct rpng_pool *pool);

/* See rpng.c. */
bool rpng_add_idat_spans(rpng_t *rpng, const uint8_t *base,
      const struct rpng_ext_span *v, int n);

struct png_ihdr
{
   uint32_t width;
   uint32_t height;
   uint8_t depth;
   uint8_t color_type;
   uint8_t compression;
   uint8_t filter;
   uint8_t interlace;
};

#endif

/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (trans_stream.h).
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

#ifndef LIBRETRO_SDK_TRANS_STREAM_H__
#define LIBRETRO_SDK_TRANS_STREAM_H__

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <retro_miscellaneous.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum trans_stream_error
{
    TRANS_STREAM_ERROR_NONE = 0,
    TRANS_STREAM_ERROR_AGAIN, /* more work to do */
    TRANS_STREAM_ERROR_ALLOCATION_FAILURE, /* malloc failure */
    TRANS_STREAM_ERROR_INVALID, /* invalid state */
    TRANS_STREAM_ERROR_BUFFER_FULL, /* output buffer full */
    TRANS_STREAM_ERROR_OTHER
};

struct trans_stream_backend
{
   const char *ident;
   const struct trans_stream_backend *reverse;

   /* Create a stream data structure */
   void *(*stream_new)(void);

   /* Free it */
   void  (*stream_free)(void *);

   /* (Optional) Set extra properties, defined per transcoder */
   bool  (*define)(void *, const char *, uint32_t);

   /* Set our input source */
   void  (*set_in)(void *, const uint8_t *, uint32_t);

   /* Set our output target */
   void  (*set_out)(void *, uint8_t *, uint32_t);

   /* Perform a transcoding, flushing/finalizing if asked to. Writes out how
    * many bytes were read and written. Error target optional. */
   bool  (*trans)(void *, bool, uint32_t *, uint32_t *, enum trans_stream_error *);
};

/**
 * trans_stream_trans_full:
 * @backend                     : transcoding backend
 * @data                        : (optional) existing stream data, or a target
 *                                for the new stream data to be saved
 * @in                          : input data
 * @in_size                     : input size
 * @out                         : output data
 * @out_size                    : output size
 * @error                       : (optional) output for error code
 *
 * Perform a full transcoding from a source to a destination.
 */
bool trans_stream_trans_full(
    struct trans_stream_backend *backend, void **data,
    const uint8_t *in, uint32_t in_size,
    uint8_t *out, uint32_t out_size,
    enum trans_stream_error *error);

const struct trans_stream_backend* trans_stream_get_zlib_deflate_backend(void);
const struct trans_stream_backend* trans_stream_get_zlib_inflate_backend(void);
const struct trans_stream_backend* trans_stream_get_pipe_backend(void);

extern const struct trans_stream_backend zlib_deflate_backend;
extern const struct trans_stream_backend zlib_inflate_backend;
extern const struct trans_stream_backend pipe_backend;

RETRO_END_DECLS

#endif

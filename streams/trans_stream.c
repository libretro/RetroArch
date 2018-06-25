/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (trans_stream.c).
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

#include <streams/trans_stream.h>

/**
 * trans_stream_trans_full:
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
    enum trans_stream_error *error)
{
   void *rdata;
   bool ret;
   uint32_t rd, wn;

   if (data && *data)
   {
      rdata = *data;
   }
   else
   {
      rdata = backend->stream_new();
      if (!rdata)
      {
         if (error)
            *error = TRANS_STREAM_ERROR_ALLOCATION_FAILURE;
         return false;
      }
   }

   backend->set_in(rdata, in, in_size);
   backend->set_out(rdata, out, out_size);
   ret = backend->trans(rdata, true, &rd, &wn, error);

   if (data)
      *data = rdata;
   else
      backend->stream_free(rdata);

   return ret;
}

const struct trans_stream_backend* trans_stream_get_zlib_deflate_backend(void)
{
#if HAVE_ZLIB
   return &zlib_deflate_backend;
#else
   return NULL;
#endif
}

const struct trans_stream_backend* trans_stream_get_zlib_inflate_backend(void)
{
#if HAVE_ZLIB
   return &zlib_inflate_backend;
#else
   return NULL;
#endif
}

const struct trans_stream_backend* trans_stream_get_pipe_backend(void)
{
   return &pipe_backend;
}

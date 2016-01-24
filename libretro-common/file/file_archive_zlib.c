/* Copyright  (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (file_archive_zlib.c).
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

#include <stdlib.h>

#include <compat/zlib.h>
#include <file/file_archive.h>
#include <retro_file.h>

static void *zlib_stream_new(void)
{
   return (z_stream*)calloc(1, sizeof(z_stream));
}

static void zlib_stream_free(void *data)
{
   z_stream *ret = (z_stream*)data;
   if (ret)
      inflateEnd(ret);
}

static uint32_t zlib_stream_get_avail_in(void *data)
{
   z_stream *stream = (z_stream*)data;

   if (!stream)
      return 0;

   return stream->avail_in;
}

static uint32_t zlib_stream_get_avail_out(void *data)
{
   z_stream *stream = (z_stream*)data;

   if (!stream)
      return 0;

   return stream->avail_out;
}

static uint64_t zlib_stream_get_total_out(void *data)
{
   z_stream *stream = (z_stream*)data;

   if (!stream)
      return 0;

   return stream->total_out;
}

static void zlib_stream_decrement_total_out(void *data, unsigned subtraction)
{
   z_stream *stream = (z_stream*)data;

   if (stream)
      stream->total_out  -= subtraction;
}

static void zlib_stream_compress_free(void *data)
{
   z_stream *ret = (z_stream*)data;
   if (ret)
      deflateEnd(ret);
}

const struct zlib_file_backend zlib_backend = {
   zlib_stream_new,
   zlib_stream_free,
   zlib_stream_get_avail_in,
   zlib_stream_get_avail_out,
   zlib_stream_get_total_out,
   zlib_stream_decrement_total_out,
   zlib_stream_compress_free,
   "zlib"
};

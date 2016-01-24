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

static void zlib_stream_set(void *data,
      uint32_t       avail_in,
      uint32_t       avail_out,
      const uint8_t *next_in,
      uint8_t       *next_out
      )
{
   z_stream *stream = (z_stream*)data;

   if (!stream)
      return;

   stream->avail_in  = avail_in;
   stream->avail_out = avail_out;

   stream->next_in   = (uint8_t*)next_in;
   stream->next_out  = next_out;
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

static int zlib_stream_compress_data_to_file(void *data)
{
   int zstatus;
   z_stream *stream = (z_stream*)data;

   if (!stream)
      return -1;

   zstatus = deflate(stream, Z_FINISH);

   if (zstatus == Z_STREAM_END)
      return 1;

   return 0;
}

static bool zlib_stream_decompress_init(void *data)
{
   z_stream *stream = (z_stream*)data;

   if (!stream)
      return false;
   if (inflateInit(stream) != Z_OK)
      return false;
   return true;
}

static bool zlib_stream_decompress_data_to_file_init(
      zlib_file_handle_t *handle,
      const uint8_t *cdata,  uint32_t csize, uint32_t size)
{
   if (!handle)
      return false;

   if (!(handle->stream = (z_stream*)zlib_stream_new()))
      goto error;
   
   if (inflateInit2(handle->stream, -MAX_WBITS) != Z_OK)
      goto error;

   handle->data = (uint8_t*)malloc(size);

   if (!handle->data)
      goto error;

   zlib_stream_set(handle->stream, csize, size,
         (const uint8_t*)cdata, handle->data);

   return true;

error:
   zlib_stream_free(handle->stream);
   free(handle->stream);
   if (handle->data)
      free(handle->data);

   return false;
}

static int zlib_stream_decompress_data_to_file_iterate(void *data)
{
   int zstatus;
   z_stream *stream = (z_stream*)data;

   if (!stream)
      return -1;

   zstatus = inflate(stream, Z_NO_FLUSH);

   if (zstatus == Z_STREAM_END)
      return 1;

   if (zstatus != Z_OK && zstatus != Z_BUF_ERROR)
      return -1;

   return 0;
}

static void zlib_stream_compress_init(void *data, int level)
{
   z_stream *stream = (z_stream*)data;

   if (stream)
      deflateInit(stream, level);
}

const struct zlib_file_backend zlib_backend = {
   zlib_stream_new,
   zlib_stream_free,
   zlib_stream_set,
   zlib_stream_get_avail_in,
   zlib_stream_get_avail_out,
   zlib_stream_get_total_out,
   zlib_stream_decrement_total_out,
   zlib_stream_decompress_init,
   zlib_stream_decompress_data_to_file_init,
   zlib_stream_decompress_data_to_file_iterate,
   zlib_stream_compress_init,
   zlib_stream_compress_free,
   zlib_stream_compress_data_to_file,
   "zlib"
};

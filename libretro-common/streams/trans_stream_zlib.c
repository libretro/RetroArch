/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (trans_stream_zlib.c).
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
#include <string.h>

#include <zlib.h>
#include <string/stdstring.h>
#include <streams/trans_stream.h>

struct zlib_trans_stream
{
   z_stream z;
   int window_bits;
   int level;
   bool inited;
};

static void *zlib_deflate_stream_new(void)
{
   struct zlib_trans_stream *ret = (struct zlib_trans_stream*)
      malloc(sizeof(*ret));
   if (!ret)
      return NULL;
   ret->inited      = false;
   ret->level       = 9;
   ret->window_bits = 15;

   ret->z.next_in   = NULL;
   ret->z.avail_in  = 0;
   ret->z.total_in  = 0;
   ret->z.next_out  = NULL;
   ret->z.avail_out = 0;
   ret->z.total_out = 0;

   ret->z.msg       = NULL;
   ret->z.state     = NULL;

   ret->z.zalloc    = NULL;
   ret->z.zfree     = NULL;
   ret->z.opaque    = NULL;

   ret->z.data_type = 0;
   ret->z.adler     = 0;
   ret->z.reserved  = 0;
   return (void *)ret;
}

static void *zlib_inflate_stream_new(void)
{
   struct zlib_trans_stream *ret = (struct zlib_trans_stream*)
      malloc(sizeof(*ret));
   if (!ret)
      return NULL;
   ret->inited      = false;
   ret->window_bits = MAX_WBITS;

   ret->z.next_in   = NULL;
   ret->z.avail_in  = 0;
   ret->z.total_in  = 0;
   ret->z.next_out  = NULL;
   ret->z.avail_out = 0;
   ret->z.total_out = 0;

   ret->z.msg       = NULL;
   ret->z.state     = NULL;

   ret->z.zalloc    = NULL;
   ret->z.zfree     = NULL;
   ret->z.opaque    = NULL;

   ret->z.data_type = 0;
   ret->z.adler     = 0;
   ret->z.reserved  = 0;
   return (void *)ret;
}

static void zlib_deflate_stream_free(void *data)
{
   struct zlib_trans_stream *z = (struct zlib_trans_stream *) data;
   if (!z)
      return;
   if (z->inited)
      deflateEnd(&z->z);
   free(z);
}

static void zlib_inflate_stream_free(void *data)
{
   struct zlib_trans_stream *z = (struct zlib_trans_stream *) data;
   if (!z)
      return;
   if (z->inited)
      inflateEnd(&z->z);
   if (z)
      free(z);
}

static bool zlib_deflate_define(void *data, const char *prop, uint32_t val)
{
   struct zlib_trans_stream *z = (struct zlib_trans_stream*)data;
   if (!data)
      return false;

   if (string_is_equal(prop, "level"))
      z->level = (int) val;
   else if (string_is_equal(prop, "window_bits"))
      z->window_bits = (int) val;
   else
      return false;

   return true;
}

static bool zlib_inflate_define(void *data, const char *prop, uint32_t val)
{
   struct zlib_trans_stream *z = (struct zlib_trans_stream*)data;
   if (!data)
      return false;

   if (string_is_equal(prop, "window_bits"))
   {
      z->window_bits = (int) val;
      return true;
   }
   return false;
}

static void zlib_deflate_set_in(void *data, const uint8_t *in, uint32_t in_size)
{
   struct zlib_trans_stream *z = (struct zlib_trans_stream *) data;

   if (!z)
      return;

   z->z.next_in                = (uint8_t *) in;
   z->z.avail_in               = in_size;

   if (!z->inited)
   {
      deflateInit2(&z->z, z->level, Z_DEFLATED , z->window_bits, 8,  Z_DEFAULT_STRATEGY );
      z->inited = true;
   }
}

static void zlib_inflate_set_in(void *data, const uint8_t *in, uint32_t in_size)
{
   struct zlib_trans_stream *z = (struct zlib_trans_stream *) data;

   if (!z)
      return;

   z->z.next_in                = (uint8_t *) in;
   z->z.avail_in               = in_size;
   if (!z->inited)
   {
      inflateInit2(&z->z, z->window_bits);
      z->inited = true;
   }
}

static void zlib_set_out(void *data, uint8_t *out, uint32_t out_size)
{
   struct zlib_trans_stream *z = (struct zlib_trans_stream *) data;

   if (!z)
      return;

   z->z.next_out               = out;
   z->z.avail_out              = out_size;
}

static bool zlib_deflate_trans(
   void *data, bool flush,
   uint32_t *rd, uint32_t *wn,
   enum trans_stream_error *error)
{
   int zret                     = 0;
   bool ret                     = false;
   uint32_t pre_avail_in        = 0;
   uint32_t pre_avail_out       = 0;
   struct zlib_trans_stream *zt = (struct zlib_trans_stream *) data;
   z_stream                  *z = &zt->z;

   if (!zt->inited)
   {
      deflateInit2(z, zt->level, Z_DEFLATED , zt->window_bits, 8,  Z_DEFAULT_STRATEGY );
      zt->inited = true;
   }

   pre_avail_in  = z->avail_in;
   pre_avail_out = z->avail_out;
   zret          = deflate(z, flush ? Z_FINISH : Z_NO_FLUSH);

   if (zret == Z_OK)
   {
      if (error)
         *error = TRANS_STREAM_ERROR_AGAIN;
   }
   else if (zret == Z_STREAM_END)
   {
      if (error)
         *error = TRANS_STREAM_ERROR_NONE;
   }
   else
   {
      if (error)
         *error = TRANS_STREAM_ERROR_OTHER;
      return false;
   }
   ret = true;

   if (z->avail_out == 0)
   {
      /* Filled buffer, maybe an error */
      if (z->avail_in != 0)
      {
         ret = false;
         if (error)
            *error = TRANS_STREAM_ERROR_BUFFER_FULL;
      }
   }

   *rd = pre_avail_in - z->avail_in;
   *wn = pre_avail_out - z->avail_out;

   if (flush && zret == Z_STREAM_END)
   {
      deflateEnd(z);
      zt->inited = false;
   }

   return ret;
}

static bool zlib_inflate_trans(
   void *data, bool flush,
   uint32_t *rd, uint32_t *wn,
   enum trans_stream_error *error)
{
   int zret;
   bool ret                     = false;
   uint32_t pre_avail_in        = 0;
   uint32_t pre_avail_out       = 0;
   struct zlib_trans_stream *zt = (struct zlib_trans_stream *) data;
   z_stream                  *z = &zt->z;

   if (!zt->inited)
   {
      inflateInit2(z, zt->window_bits);
      zt->inited = true;
   }

   pre_avail_in  = z->avail_in;
   pre_avail_out = z->avail_out;
   zret          = inflate(z, flush ? Z_FINISH : Z_NO_FLUSH);

   if (zret == Z_OK)
   {
      if (error)
         *error = TRANS_STREAM_ERROR_AGAIN;
   }
   else if (zret == Z_STREAM_END)
   {
      if (error)
         *error = TRANS_STREAM_ERROR_NONE;
   }
   else
   {
      if (error)
         *error = TRANS_STREAM_ERROR_OTHER;
      return false;
   }
   ret = true;

   if (z->avail_out == 0)
   {
      /* Filled buffer, maybe an error */
      if (z->avail_in != 0)
      {
         ret = false;
         if (error)
            *error = TRANS_STREAM_ERROR_BUFFER_FULL;
      }
   }

   *rd = pre_avail_in - z->avail_in;
   *wn = pre_avail_out - z->avail_out;

   if (flush && zret == Z_STREAM_END)
   {
      inflateEnd(z);
      zt->inited = false;
   }

   return ret;
}

const struct trans_stream_backend zlib_deflate_backend = {
   "zlib_deflate",
   &zlib_inflate_backend,
   zlib_deflate_stream_new,
   zlib_deflate_stream_free,
   zlib_deflate_define,
   zlib_deflate_set_in,
   zlib_set_out,
   zlib_deflate_trans
};

const struct trans_stream_backend zlib_inflate_backend = {
   "zlib_inflate",
   &zlib_deflate_backend,
   zlib_inflate_stream_new,
   zlib_inflate_stream_free,
   zlib_inflate_define,
   zlib_inflate_set_in,
   zlib_set_out,
   zlib_inflate_trans
};

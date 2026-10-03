/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (trans_stream_rzstd.c).
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* A trans_stream backend over the built-in Zstandard codec. The codec
 * is one-shot - a frame in, a frame out - so a transform here is the
 * whole input to the whole output on the call that flushes; a call
 * that does not flush moves nothing and asks to be called again with
 * the flush, which is how the RZIP container drives every backend (one
 * chunk per transform, flushed). The encode level is a 'level'
 * property, as it is for zlib. */

#include <stdlib.h>
#include <string.h>

#include <streams/trans_stream.h>
#include <encodings/rzstd.h>

struct rzstd_trans_stream
{
   const uint8_t *in;
   uint8_t       *out;
   uint32_t       in_size;
   uint32_t       out_size;
   uint32_t       level;
};

static void *rzstd_stream_new(void)
{
   struct rzstd_trans_stream *s = (struct rzstd_trans_stream*)calloc(1, sizeof(*s));
   if (s)
      s->level = 3;
   return s;
}

static void rzstd_stream_free(void *data)
{
   free(data);
}

static bool rzstd_define(void *data, const char *prop, uint32_t val)
{
   struct rzstd_trans_stream *s = (struct rzstd_trans_stream*)data;
   if (!s)
      return false;
   if (!strcmp(prop, "level"))
   {
      s->level = val;
      return true;
   }
   return false;
}

static void rzstd_set_in(void *data, const uint8_t *in, uint32_t in_size)
{
   struct rzstd_trans_stream *s = (struct rzstd_trans_stream*)data;
   s->in      = in;
   s->in_size = in_size;
}

static void rzstd_set_out(void *data, uint8_t *out, uint32_t out_size)
{
   struct rzstd_trans_stream *s = (struct rzstd_trans_stream*)data;
   s->out      = out;
   s->out_size = out_size;
}

static bool rzstd_encode_trans(void *data, bool flush,
      uint32_t *rd, uint32_t *wn, enum trans_stream_error *error)
{
   struct rzstd_trans_stream *s = (struct rzstd_trans_stream*)data;
   size_t wrote = 0;
   int    rc;

   if (error)
      *error = TRANS_STREAM_ERROR_NONE;
   *rd = 0;
   *wn = 0;
   if (!flush)
      return true;

   rc = rzstd_encode(s->out, s->out_size, s->in, s->in_size, (int)s->level, &wrote);
   if (rc != RZSTD_PROCESS_END)
   {
      if (error)
         *error = TRANS_STREAM_ERROR_BUFFER_FULL;
      return false;
   }
   *rd = s->in_size;
   *wn = (uint32_t)wrote;
   s->in      += s->in_size;
   s->in_size  = 0;
   s->out     += wrote;
   s->out_size -= (uint32_t)wrote;
   return true;
}

static bool rzstd_decode_trans(void *data, bool flush,
      uint32_t *rd, uint32_t *wn, enum trans_stream_error *error)
{
   struct rzstd_trans_stream *s = (struct rzstd_trans_stream*)data;
   size_t wrote = 0;
   int    rc;

   if (error)
      *error = TRANS_STREAM_ERROR_NONE;
   *rd = 0;
   *wn = 0;
   if (!flush)
      return true;

   rc = rzstd_decode(s->out, s->out_size, s->in, s->in_size, &wrote);
   if (rc != RZSTD_PROCESS_END)
   {
      if (error)
         *error = TRANS_STREAM_ERROR_INVALID;
      return false;
   }
   *rd = s->in_size;
   *wn = (uint32_t)wrote;
   s->in      += s->in_size;
   s->in_size  = 0;
   s->out     += wrote;
   s->out_size -= (uint32_t)wrote;
   return true;
}

const struct trans_stream_backend rzstd_decode_backend;

const struct trans_stream_backend rzstd_encode_backend = {
   "rzstd_encode",
   &rzstd_decode_backend,
   rzstd_stream_new,
   rzstd_stream_free,
   rzstd_define,
   rzstd_set_in,
   rzstd_set_out,
   rzstd_encode_trans
};

const struct trans_stream_backend rzstd_decode_backend = {
   "rzstd_decode",
   &rzstd_encode_backend,
   rzstd_stream_new,
   rzstd_stream_free,
   NULL,
   rzstd_set_in,
   rzstd_set_out,
   rzstd_decode_trans
};

const struct trans_stream_backend *trans_stream_get_rzstd_encode_backend(void)
{
   return &rzstd_encode_backend;
}

const struct trans_stream_backend *trans_stream_get_rzstd_decode_backend(void)
{
   return &rzstd_decode_backend;
}

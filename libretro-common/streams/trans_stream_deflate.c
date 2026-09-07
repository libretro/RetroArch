/* Copyright  (C) 2010-2024 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (trans_stream_deflate.c).
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

/* A trans_stream backend built on the clean-room, zlib-free DEFLATE codec in
 * encodings/deflate.c.  It provides the same deflate/inflate transcoding
 * service as the zlib backend, so RetroArch's compression paths (rzip,
 * rpng, netplay) work with no external zlib dependency. */

#include <stdlib.h>
#include <string.h>

#include <encodings/deflate.h>
#include <streams/trans_stream.h>

struct deflate_trans_stream
{
   void    *stream;       /* rdeflate/rinflate handle                     */
   int      window_bits;
   int      level;
   int      is_inflate;
   int      finished;     /* END has been reached                         */
   uint32_t in_size;      /* size of the buffer last given to set_in       */
   uint32_t in_done;      /* input consumed so far from the current set_in  */
   uint32_t out_size;     /* size of the buffer last given to set_out      */
};

static void *deflate_trans_stream_new(void)
{
   struct deflate_trans_stream *st = (struct deflate_trans_stream*)
      calloc(1, sizeof(*st));
   if (!st)
      return NULL;
   st->window_bits = 15;   /* zlib-wrapped by default                     */
   st->level       = 9;
   st->is_inflate  = 0;
   return (void*)st;
}

static void *inflate_trans_stream_new(void)
{
   struct deflate_trans_stream *st = (struct deflate_trans_stream*)
      calloc(1, sizeof(*st));
   if (!st)
      return NULL;
   st->window_bits = 15;
   st->is_inflate  = 1;
   return (void*)st;
}

static void deflate_trans_stream_free(void *data)
{
   struct deflate_trans_stream *st = (struct deflate_trans_stream*)data;
   if (!st)
      return;
   if (st->stream)
   {
      if (st->is_inflate)
         rinflate_free(st->stream);
      else
         rdeflate_free(st->stream);
   }
   free(st);
}

static bool deflate_define(void *data, const char *prop, uint32_t val)
{
   struct deflate_trans_stream *st = (struct deflate_trans_stream*)data;
   if (!st)
      return false;
   if (strcmp(prop, "level") == 0)
   {
      st->level = (int)val;
      return true;
   }
   else if (strcmp(prop, "window_bits") == 0)
   {
      st->window_bits = (int)val;
      return true;
   }
   return false;
}

static bool inflate_define(void *data, const char *prop, uint32_t val)
{
   struct deflate_trans_stream *st = (struct deflate_trans_stream*)data;
   if (!st)
      return false;
   if (strcmp(prop, "window_bits") == 0)
   {
      st->window_bits = (int)val;
      return true;
   }
   return false;
}

static void deflate_set_in(void *data, const uint8_t *in, uint32_t in_size)
{
   struct deflate_trans_stream *st = (struct deflate_trans_stream*)data;
   if (!st)
      return;
   if (!st->stream)
      st->stream = rdeflate_new(st->level, st->window_bits);
   st->in_size = in_size;
   st->in_done = 0;
   if (st->stream)
      rdeflate_set_in(st->stream, in, (size_t)in_size);
}

static void inflate_set_in(void *data, const uint8_t *in, uint32_t in_size)
{
   struct deflate_trans_stream *st = (struct deflate_trans_stream*)data;
   if (!st)
      return;
   if (!st->stream)
      st->stream = rinflate_new(st->window_bits);
   st->in_size = in_size;
   st->in_done = 0;
   if (st->stream)
      rinflate_set_in(st->stream, in, (size_t)in_size);
}

static void deflate_set_out(void *data, uint8_t *out, uint32_t out_size)
{
   struct deflate_trans_stream *st = (struct deflate_trans_stream*)data;
   if (!st)
      return;
   /* Create the codec lazily here too: callers may set the output buffer
    * before the first set_in (the zlib backend tolerates this because its
    * z_stream always exists). */
   if (!st->stream)
   {
      if (st->is_inflate)
         st->stream = rinflate_new(st->window_bits);
      else
         st->stream = rdeflate_new(st->level, st->window_bits);
   }
   if (!st->stream)
      return;
   st->out_size = out_size;
   if (st->is_inflate)
      rinflate_set_out(st->stream, out, (size_t)out_size);
   else
      rdeflate_set_out(st->stream, out, (size_t)out_size);
}

static bool deflate_trans(
      void *data, bool flush,
      uint32_t *rd, uint32_t *wn,
      enum trans_stream_error *err)
{
   struct deflate_trans_stream *st = (struct deflate_trans_stream*)data;
   size_t read_amt = 0, wrote_amt = 0;
   int status;

   if (!st || !st->stream)
   {
      if (err)
         *err = TRANS_STREAM_ERROR_INVALID;
      return false;
   }

   /* `flush` means the caller has supplied all remaining input in the
    * current buffer and wants the stream finalized. */
   if (flush && !st->is_inflate)
      rdeflate_finish(st->stream);

   /* The codec's process() already loops internally -- ingesting input,
    * parsing, emitting blocks and sliding its window -- until either the
    * output buffer fills or the stream ends.  One call therefore consumes
    * as much of the provided input as the output room allows. */
   {
      size_t r = 0, w = 0;
      if (st->is_inflate)
         status = rinflate_process(st->stream, &r, &w);
      else
         status = rdeflate_process(st->stream, &r, &w);
      read_amt  = r;
      wrote_amt = w;
      st->in_done += (uint32_t)r;
   }

   if (rd)
      *rd = (uint32_t)read_amt;
   if (wn)
      *wn = (uint32_t)wrote_amt;

   if (status == RDEFLATE_PROCESS_ERROR)
   {
      if (err)
         *err = TRANS_STREAM_ERROR_OTHER;
      return false;
   }

   if (status == RDEFLATE_PROCESS_END)
   {
      /* On a finalized stream, dispose of the codec so the next set_in on
       * this backend object starts a fresh stream (matches how the zlib
       * backend calls deflateEnd/inflateEnd here).  This lets callers like
       * rzip reuse one backend object across many independent chunks. */
      if (flush)
      {
         if (st->is_inflate)
            rinflate_free(st->stream);
         else
            rdeflate_free(st->stream);
         st->stream   = NULL;
         st->finished = 0;
      }
      else
         st->finished = 1;
      if (err)
         *err = TRANS_STREAM_ERROR_NONE;
      return true;
   }

   /* status == RDEFLATE_PROCESS_NEXT: more work remains.  If any input is
    * still unconsumed the caller must not advance past it.  Because a
    * caller may invoke trans() several times for one set_in (re-pointing
    * the output on each BUFFER_FULL), the test uses the cumulative input
    * consumed since the last set_in, not just this call's amount.  When
    * output filled with input still pending, report BUFFER_FULL so the
    * caller drains and retries; once all input is consumed, report AGAIN
    * (the codec may be holding buffered data awaiting more input or a
    * finalizing flush -- the normal "send more" signal). */
   if (st->in_done < st->in_size)
   {
      if (err)
         *err = TRANS_STREAM_ERROR_BUFFER_FULL;
      return false;
   }

   if (err)
      *err = TRANS_STREAM_ERROR_AGAIN;
   return true;
}

const struct trans_stream_backend deflate_deflate_backend;
const struct trans_stream_backend deflate_inflate_backend;

const struct trans_stream_backend deflate_deflate_backend = {
   "deflate_deflate",
   &deflate_inflate_backend,
   deflate_trans_stream_new,
   deflate_trans_stream_free,
   deflate_define,
   deflate_set_in,
   deflate_set_out,
   deflate_trans
};

const struct trans_stream_backend deflate_inflate_backend = {
   "deflate_inflate",
   &deflate_deflate_backend,
   inflate_trans_stream_new,
   deflate_trans_stream_free,
   inflate_define,
   inflate_set_in,
   deflate_set_out,
   deflate_trans
};

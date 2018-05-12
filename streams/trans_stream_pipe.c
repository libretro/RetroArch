/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (trans_stream_pipe.c).
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

#include <streams/trans_stream.h>

struct pipe_trans_stream
{
   const uint8_t *in;
   uint8_t *out;
   uint32_t in_size, out_size;
};

static void *pipe_stream_new(void)
{
   return (struct pipe_trans_stream*)calloc(1, sizeof(struct pipe_trans_stream));
}

static void pipe_stream_free(void *data)
{
   free(data);
}

static void pipe_set_in(void *data, const uint8_t *in, uint32_t in_size)
{
   struct pipe_trans_stream *p = (struct pipe_trans_stream *) data;
   p->in = in;
   p->in_size = in_size;
}

static void pipe_set_out(void *data, uint8_t *out, uint32_t out_size)
{
   struct pipe_trans_stream *p = (struct pipe_trans_stream *) data;
   p->out = out;
   p->out_size = out_size;
}

static bool pipe_trans(
   void *data, bool flush,
   uint32_t *rd, uint32_t *wn,
   enum trans_stream_error *error)
{
   struct pipe_trans_stream *p = (struct pipe_trans_stream *) data;

   if (p->out_size < p->in_size)
   {
      memcpy(p->out, p->in, p->out_size);
      *rd = *wn = p->out_size;
      p->in += p->out_size;
      p->out += p->out_size;
      *error = TRANS_STREAM_ERROR_BUFFER_FULL;
      return false;
   }
   else
   {
      memcpy(p->out, p->in, p->in_size);
      *rd = *wn = p->in_size;
      p->in += p->in_size;
      p->out += p->in_size;
      *error = TRANS_STREAM_ERROR_NONE;
      return true;
   }
}

const struct trans_stream_backend pipe_backend = {
   "pipe",
   &pipe_backend,
   pipe_stream_new,
   pipe_stream_free,
   NULL,
   pipe_set_in,
   pipe_set_out,
   pipe_trans
};

/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (fifo_queue.c).
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

#include <retro_common_api.h>
#include <retro_inline.h>
#include <boolean.h>

#include <queues/fifo_queue.h>

/* Branchless wrap: equivalent to (pos + delta) % size, but avoids
 * integer division. Valid only when pos + delta < 2 * size, which
 * is guaranteed here because callers limit delta to available space. */
static INLINE size_t fifo_wrap(size_t pos, size_t delta, size_t size)
{
   pos += delta;
   if (pos >= size)
      pos -= size;
   return pos;
}

static bool fifo_initialize_internal(fifo_buffer_t *buf, size_t len)
{
   /* malloc instead of calloc: first/end are set explicitly below,
    * so zero-initialising every byte is unnecessary work. */
   uint8_t *buffer = (uint8_t*)malloc(len + 1);

   if (!buffer)
      return false;

   buf->buffer = buffer;
   buf->size   = len + 1;
   buf->first  = 0;
   buf->end    = 0;

   return true;
}

bool fifo_initialize(fifo_buffer_t *buf, size_t len)
{
   return (buf && fifo_initialize_internal(buf, len));
}

void fifo_free(fifo_buffer_t *buffer)
{
   if (!buffer)
      return;

   free(buffer->buffer);
   free(buffer);
}

bool fifo_deinitialize(fifo_buffer_t *buffer)
{
   if (!buffer)
      return false;

   /* buffer->buffer may legitimately be NULL if a previous
    * deinitialize already ran; free(NULL) is defined by C89 §4.10.3.2
    * to be a no-op, so the explicit NULL guard is unnecessary. */
   free(buffer->buffer);
   buffer->buffer = NULL;
   buffer->size   = 0;
   buffer->first  = 0;
   buffer->end    = 0;

   return true;
}

fifo_buffer_t *fifo_new(size_t len)
{
   fifo_buffer_t *buf = (fifo_buffer_t*)malloc(sizeof(*buf));

   if (!buf)
      return NULL;

   if (!fifo_initialize_internal(buf, len))
   {
      free(buf);
      return NULL;
   }

   return buf;
}

void fifo_write(fifo_buffer_t *buffer, const void *in_buf, size_t len)
{
   /* Cache to avoid repeated pointer dereferences in the hot path. */
   const size_t  size       = buffer->size;
   const size_t  end        = buffer->end;
   const uint8_t *src       = (const uint8_t*)in_buf;

   if (end + len <= size)
   {
      /* Common case: data fits without wrapping. Single copy. */
      memcpy(buffer->buffer + end, src, len);
   }
   else
   {
      /* Wrap-around case: split into two copies. */
      const size_t first_write = size - end;
      memcpy(buffer->buffer + end, src,                first_write);
      memcpy(buffer->buffer,       src + first_write,  len - first_write);
   }

   /* Subtract instead of modulo: end + len < 2 * size is guaranteed
    * by the caller honouring FIFO_WRITE_AVAIL. */
   buffer->end = fifo_wrap(end, len, size);
}

void fifo_read(fifo_buffer_t *buffer, void *in_buf, size_t len)
{
   /* Cache to avoid repeated pointer dereferences in the hot path. */
   const size_t size    = buffer->size;
   const size_t first   = buffer->first;
   uint8_t      *dst    = (uint8_t*)in_buf;

   /* Common case: data is contiguous. Single copy. */
   if (first + len <= size)
      memcpy(dst, buffer->buffer + first, len);
   else
   {
      /* Wrap-around case: split into two copies. */
      const size_t first_read = size - first;
      memcpy(dst,              buffer->buffer + first, first_read);
      memcpy(dst + first_read, buffer->buffer,         len - first_read);
   }

   /* Same wrap optimisation as fifo_write. */
   buffer->first = fifo_wrap(first, len, size);
}

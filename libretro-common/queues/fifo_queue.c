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

static bool fifo_initialize_internal(fifo_buffer_t *buf, size_t len)
{
   uint8_t *buffer;

   /* The ring is len + 1 bytes; SIZE_MAX would wrap that to nothing. */
   if (len == (size_t)-1)
      return false;
   if (!(buffer = (uint8_t*)calloc(1, len + 1)))
      return false;

   buf->buffer        = buffer;
   buf->size          = len + 1;
   buf->first         = 0;
   buf->end           = 0;

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

   if (buffer->buffer)
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

/* The precondition, documented in the header: len is at most what is
 * available, so the copy crosses the ring's end at most once. The
 * tail test is written so it cannot wrap size_t. */
void fifo_write(fifo_buffer_t *buffer, const void *in_buf, size_t len)
{
   size_t tail        = buffer->size - buffer->end;
   size_t first_write = len;
   size_t rest_write  = 0;

   if (len > tail)
   {
      first_write = tail;
      rest_write  = len - tail;
   }

   memcpy(buffer->buffer + buffer->end, in_buf, first_write);
   if (rest_write > 0)
      memcpy(buffer->buffer, (const uint8_t*)in_buf + first_write, rest_write);

   /* len is under size, so the index crosses the ring's end at most
    * once: a compare and a subtract, not a division by a size that is
    * never a power of two. */
   buffer->end += len;
   if (buffer->end >= buffer->size)
      buffer->end -= buffer->size;
}

void fifo_read(fifo_buffer_t *buffer, void *in_buf, size_t len)
{
   size_t tail       = buffer->size - buffer->first;
   size_t first_read = len;
   size_t rest_read  = 0;

   if (len > tail)
   {
      first_read = tail;
      rest_read  = len - tail;
   }

   memcpy(in_buf, (const uint8_t*)buffer->buffer + buffer->first, first_read);
   if (rest_read > 0)
      memcpy((uint8_t*)in_buf + first_read, buffer->buffer, rest_read);

   buffer->first += len;
   if (buffer->first >= buffer->size)
      buffer->first -= buffer->size;
}

/* The checked calls clamp to what is available and say how much moved;
 * a length past it is a short write or read, never a copy past the
 * ring. For a caller that has already clamped - the audio drivers,
 * which read the availability under their own lock - the unchecked
 * calls above do not recompute it. */
size_t fifo_write_checked(fifo_buffer_t *buffer, const void *in_buf, size_t len)
{
   size_t avail = FIFO_WRITE_AVAIL(buffer);
   if (len > avail)
      len = avail;
   if (len)
      fifo_write(buffer, in_buf, len);
   return len;
}

size_t fifo_read_checked(fifo_buffer_t *buffer, void *in_buf, size_t len)
{
   size_t avail = FIFO_READ_AVAIL(buffer);
   if (len > avail)
      len = avail;
   if (len)
      fifo_read(buffer, in_buf, len);
   return len;
}

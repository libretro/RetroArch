/* Copyright  (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rthreads.c).
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

#include <queues/fifo_buffer.h>

struct fifo_buffer
{
   uint8_t *buffer;
   size_t bufsize;
   size_t first;
   size_t end;
};

fifo_buffer_t *fifo_new(size_t size)
{
   fifo_buffer_t *buf = (fifo_buffer_t*)calloc(1, sizeof(*buf));

   if (!buf)
      return NULL;

   buf->buffer = (uint8_t*)calloc(1, size + 1);
   if (!buf->buffer)
   {
      free(buf);
      return NULL;
   }
   buf->bufsize = size + 1;

   return buf;
}

void fifo_clear(fifo_buffer_t *buffer)
{
   buffer->first = 0;
   buffer->end   = 0;
}

void fifo_free(fifo_buffer_t *buffer)
{
   if (!buffer)
      return;

   free(buffer->buffer);
   free(buffer);
}

size_t fifo_read_avail(fifo_buffer_t *buffer)
{
   size_t first = buffer->first;
   size_t end   = buffer->end;

   if (end < first)
      end += buffer->bufsize;
   return end - first;
}

size_t fifo_write_avail(fifo_buffer_t *buffer)
{
   size_t first = buffer->first;
   size_t end   = buffer->end;

   if (end < first)
      end += buffer->bufsize;

   return (buffer->bufsize - 1) - (end - first);
}

void fifo_write(fifo_buffer_t *buffer, const void *in_buf, size_t size)
{
   size_t first_write = size;
   size_t rest_write  = 0;

   if (buffer->end + size > buffer->bufsize)
   {
      first_write = buffer->bufsize - buffer->end;
      rest_write = size - first_write;
   }

   memcpy(buffer->buffer + buffer->end, in_buf, first_write);
   memcpy(buffer->buffer, (const uint8_t*)in_buf + first_write, rest_write);

   buffer->end = (buffer->end + size) % buffer->bufsize;
}


void fifo_read(fifo_buffer_t *buffer, void *in_buf, size_t size)
{
   size_t first_read = size;
   size_t rest_read  = 0;

   if (buffer->first + size > buffer->bufsize)
   {
      first_read = buffer->bufsize - buffer->first;
      rest_read = size - first_read;
   }

   memcpy(in_buf, (const uint8_t*)buffer->buffer + buffer->first, first_read);
   memcpy((uint8_t*)in_buf + first_read, buffer->buffer, rest_read);

   buffer->first = (buffer->first + size) % buffer->bufsize;
}


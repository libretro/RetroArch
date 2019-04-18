/* Copyright  (C) 2010-2018 The RetroArch team
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

#include <queues/fifo_queue.h>

fifo_buffer_t *fifo_new(size_t size)
{
   uint8_t    *buffer = NULL;
   fifo_buffer_t *buf = (fifo_buffer_t*)calloc(1, sizeof(*buf));

   if (!buf)
      return NULL;

   buffer = (uint8_t*)calloc(1, size + 1);

   if (!buffer)
   {
      free(buf);
      return NULL;
   }

   buf->buffer = buffer;
   buf->size   = size + 1;

   return buf;
}

void fifo_write(fifo_buffer_t *buffer, const void *in_buf, size_t size)
{
   size_t first_write = size;
   size_t rest_write  = 0;

   if (buffer->end + size > buffer->size)
   {
      first_write = buffer->size - buffer->end;
      rest_write  = size - first_write;
   }

   memcpy(buffer->buffer + buffer->end, in_buf, first_write);
   memcpy(buffer->buffer, (const uint8_t*)in_buf + first_write, rest_write);

   buffer->end = (buffer->end + size) % buffer->size;
}

void fifo_read(fifo_buffer_t *buffer, void *in_buf, size_t size)
{
   size_t first_read = size;
   size_t rest_read  = 0;

   if (buffer->first + size > buffer->size)
   {
      first_read = buffer->size - buffer->first;
      rest_read  = size - first_read;
   }

   memcpy(in_buf, (const uint8_t*)buffer->buffer + buffer->first, first_read);
   memcpy((uint8_t*)in_buf + first_read, buffer->buffer, rest_read);

   buffer->first = (buffer->first + size) % buffer->size;
}

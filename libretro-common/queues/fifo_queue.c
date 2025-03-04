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
   uint8_t *buffer    = (uint8_t*)calloc(1, len + 1);

   if (!buffer)
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

void fifo_write(fifo_buffer_t *buffer, const void *in_buf, size_t len)
{
   size_t first_write = len;
   size_t rest_write  = 0;

   if (buffer->end + len > buffer->size)
   {
      first_write = buffer->size - buffer->end;
      rest_write  = len - first_write;
   }

   memcpy(buffer->buffer + buffer->end, in_buf, first_write);
   memcpy(buffer->buffer, (const uint8_t*)in_buf + first_write, rest_write);

   buffer->end = (buffer->end + len) % buffer->size;
}

void fifo_read(fifo_buffer_t *buffer, void *in_buf, size_t len)
{
   size_t first_read = len;
   size_t rest_read  = 0;

   if (buffer->first + len > buffer->size)
   {
      first_read = buffer->size - buffer->first;
      rest_read  = len - first_read;
   }

   memcpy(in_buf, (const uint8_t*)buffer->buffer + buffer->first, first_read);
   memcpy((uint8_t*)in_buf + first_read, buffer->buffer, rest_read);

   buffer->first = (buffer->first + len) % buffer->size;
}

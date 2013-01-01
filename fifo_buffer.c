/*  RSound - A PCM audio client/server
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * 
 *  RSound is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RSound is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RSound.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "fifo_buffer.h"
#include <stdint.h>

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
   if (buf == NULL)
      return NULL;

   buf->buffer = (uint8_t*)calloc(1, size + 1);
   if (buf->buffer == NULL)
   {
      free(buf);
      return NULL;
   }
   buf->bufsize = size + 1;

   return buf;
}

void fifo_free(fifo_buffer_t *buffer)
{
   free(buffer->buffer);
   free(buffer);
}

size_t fifo_read_avail(fifo_buffer_t *buffer)
{
   size_t first = buffer->first;
   size_t end = buffer->end;
   if (end < first)
      end += buffer->bufsize;
   return end - first;
}

size_t fifo_write_avail(fifo_buffer_t *buffer)
{
   size_t first = buffer->first;
   size_t end = buffer->end;
   if (end < first)
      end += buffer->bufsize;

   return (buffer->bufsize - 1) - (end - first);
}

void fifo_write(fifo_buffer_t *buffer, const void *in_buf, size_t size)
{
   size_t first_write = size;
   size_t rest_write = 0;
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
   size_t rest_read = 0;
   if (buffer->first + size > buffer->bufsize)
   {
      first_read = buffer->bufsize - buffer->first;
      rest_read = size - first_read;
   }

   memcpy(in_buf, (const uint8_t*)buffer->buffer + buffer->first, first_read);
   memcpy((uint8_t*)in_buf + first_read, buffer->buffer, rest_read);

   buffer->first = (buffer->first + size) % buffer->bufsize;
}


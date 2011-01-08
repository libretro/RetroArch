/*  RSound - A PCM audio client/server
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
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

#include "buffer.h"

struct rsound_fifo_buffer
{
   char *buffer;
   size_t bufsize;
   size_t first;
   size_t end;
};

rsound_fifo_buffer_t* rsnd_fifo_new(size_t size)
{
   rsound_fifo_buffer_t *buf = calloc(1, sizeof(*buf));
   if (buf == NULL)
      return NULL;

   buf->buffer = calloc(1, size + 1);
   if (buf->buffer == NULL)
   {
      free(buf);
      return NULL;
   }
   buf->bufsize = size + 1;

   return buf;
}

void rsnd_fifo_free(rsound_fifo_buffer_t* buffer)
{
   assert(buffer);
   assert(buffer->buffer);

   free(buffer->buffer);
   free(buffer);
}

size_t rsnd_fifo_read_avail(rsound_fifo_buffer_t* buffer)
{
   assert(buffer);
   assert(buffer->buffer);

   size_t first = buffer->first;
   size_t end = buffer->end;
   if (end < first)
      end += buffer->bufsize;
   return end - first;
}

size_t rsnd_fifo_write_avail(rsound_fifo_buffer_t* buffer)
{
   assert(buffer);
   assert(buffer->buffer);

   size_t first = buffer->first;
   size_t end = buffer->end;
   if (end < first)
      end += buffer->bufsize;

   return (buffer->bufsize - 1) - (end - first);
}

void rsnd_fifo_write(rsound_fifo_buffer_t* buffer, const void* in_buf, size_t size)
{
   assert(buffer);
   assert(buffer->buffer);
   assert(in_buf);
   assert(rsnd_fifo_write_avail(buffer) >= size);

   size_t first_write = size;
   size_t rest_write = 0;
   if (buffer->end + size > buffer->bufsize)
   {
      first_write = buffer->bufsize - buffer->end;
      rest_write = size - first_write;
   }

   memcpy(buffer->buffer + buffer->end, in_buf, first_write);
   if (rest_write > 0)
      memcpy(buffer->buffer, (const char*)in_buf + first_write, rest_write);

   buffer->end = (buffer->end + size) % buffer->bufsize;
}


void rsnd_fifo_read(rsound_fifo_buffer_t* buffer, void* in_buf, size_t size)
{
   assert(buffer);
   assert(buffer->buffer);
   assert(in_buf);
   assert(rsnd_fifo_read_avail(buffer) >= size);

   size_t first_read = size;
   size_t rest_read = 0;
   if (buffer->first + size > buffer->bufsize)
   {
      first_read = buffer->bufsize - buffer->first;
      rest_read = size - first_read;
   }

   memcpy(in_buf, (const char*)buffer->buffer + buffer->first, first_read);
   if (rest_read > 0)
      memcpy((char*)in_buf + first_read, buffer->buffer, rest_read);

   buffer->first = (buffer->first + size) % buffer->bufsize;
}


/* Copyright  (C) 2010-2016 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (memory_stream.c).
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <streams/memory_stream.h>

static uint8_t* g_buffer     = NULL;
static size_t g_size         = 0;
static size_t last_file_size = 0;

struct memstream
{
   uint8_t *m_buf;
   size_t m_size;
   size_t m_ptr;
};

void memstream_set_buffer(uint8_t *buffer, size_t size)
{
   g_buffer = buffer;
   g_size = size;
}

size_t memstream_get_last_size(void)
{
   return last_file_size;
}

static void memstream_init(memstream_t *stream, uint8_t *buffer, size_t max_size)
{
   stream->m_buf = buffer;
   stream->m_size = max_size;
   stream->m_ptr = 0;
}

memstream_t *memstream_open(void)
{
	memstream_t *stream;
   if (!g_buffer || !g_size)
      return NULL;

   stream = (memstream_t*)calloc(1, sizeof(*stream));
   memstream_init(stream, g_buffer, g_size);

   g_buffer = NULL;
   g_size = 0;
   return stream;
}

void memstream_close(memstream_t *stream)
{
   last_file_size = stream->m_ptr;
   free(stream);
}

size_t memstream_read(memstream_t *stream, void *data, size_t bytes)
{
   size_t avail = stream->m_size - stream->m_ptr;
   if (bytes > avail)
      bytes = avail;

   memcpy(data, stream->m_buf + stream->m_ptr, bytes);
   stream->m_ptr += bytes;
   return bytes;
}

size_t memstream_write(memstream_t *stream, const void *data, size_t bytes)
{
   size_t avail = stream->m_size - stream->m_ptr;
   if (bytes > avail)
      bytes = avail;

   memcpy(stream->m_buf + stream->m_ptr, data, bytes);
   stream->m_ptr += bytes;
   return bytes;
}

int memstream_seek(memstream_t *stream, int offset, int whence)
{
   size_t ptr;

   switch (whence)
   {
      case SEEK_SET:
         ptr = offset;
         break;
      case SEEK_CUR:
         ptr = stream->m_ptr + offset;
         break;
      case SEEK_END:
         ptr = stream->m_size + offset;
         break;
      default:
         return -1;
   }

   if (ptr <= stream->m_size)
   {
      stream->m_ptr = ptr;
      return 0;
   }
   return -1;
}

size_t memstream_pos(memstream_t *stream)
{
   return stream->m_ptr;
}

char *memstream_gets(memstream_t *stream, char *buffer, size_t len)
{
   return NULL;
}

int memstream_getc(memstream_t *stream)
{
   if (stream->m_ptr >= stream->m_size)
      return EOF;
   return stream->m_buf[stream->m_ptr++];
}

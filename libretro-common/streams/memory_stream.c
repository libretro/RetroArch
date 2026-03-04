/* Copyright  (C) 2010-2020 The RetroArch team
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

/* TODO/FIXME - static globals */
static uint8_t* g_buffer      = NULL;
static uint64_t g_size         = 0;
static uint64_t last_file_size = 0;

struct memstream
{
   uint64_t size;
   uint64_t ptr;
   uint64_t max_ptr;
   uint8_t *buf;
   unsigned writing;
};

void memstream_set_buffer(uint8_t *s, uint64_t len)
{
   g_buffer = s;
   g_size   = len;
}

memstream_t *memstream_open(unsigned writing)
{
   memstream_t *stream;
   if (!g_buffer || !g_size)
      return NULL;

   stream = (memstream_t*)malloc(sizeof(*stream));

   if (!stream)
      return NULL;

   stream->buf       = g_buffer;
   stream->size      = g_size;
   stream->ptr       = 0;
   stream->max_ptr   = 0;
   stream->writing   = writing;

   g_buffer          = NULL;
   g_size            = 0;

   return stream;
}

void memstream_close(memstream_t *stream)
{
   if (!stream)
      return;

   last_file_size = stream->writing ? stream->max_ptr : stream->size;
   free(stream);
}

uint64_t memstream_get_ptr(memstream_t *stream)
{
   return stream->ptr;
}

uint64_t memstream_read(memstream_t *stream, void *data, uint64_t bytes)
{
   uint64_t avail = 0;

   if (!stream)
      return 0;

   avail               = stream->size - stream->ptr;
   if (bytes > avail)
      bytes            = avail;

   memcpy(data, stream->buf + stream->ptr, (size_t)bytes);
   stream->ptr        += bytes;
   if (stream->ptr > stream->max_ptr)
      stream->max_ptr  = stream->ptr;
   return bytes;
}

uint64_t memstream_write(memstream_t *stream,
      const void *data, uint64_t bytes)
{
   uint64_t avail = 0;

   if (!stream)
      return 0;

   avail = stream->size - stream->ptr;
   if (bytes > avail)
      bytes = avail;

   memcpy(stream->buf + stream->ptr, data, (size_t)bytes);
   stream->ptr += bytes;
   if (stream->ptr > stream->max_ptr)
      stream->max_ptr = stream->ptr;
   return bytes;
}

int64_t memstream_seek(memstream_t *stream, int64_t offset, int whence)
{
   uint64_t ptr;

   switch (whence)
   {
      case SEEK_SET:
         ptr = offset;
         break;
      case SEEK_CUR:
         ptr = stream->ptr + offset;
         break;
      case SEEK_END:
         ptr = (stream->writing ? stream->max_ptr : stream->size) + offset;
         break;
      default:
         return -1;
   }

   if (ptr <= stream->size)
   {
      stream->ptr = ptr;
      return 0;
   }

   return -1;
}

void memstream_rewind(memstream_t *stream)
{
   memstream_seek(stream, 0L, SEEK_SET);
}

uint64_t memstream_pos(memstream_t *stream) { return stream->ptr; }
char *memstream_gets(memstream_t *stream, char *s, size_t len) { return NULL; }
uint64_t memstream_get_last_size(void) { return last_file_size; }

int memstream_getc(memstream_t *stream)
{
   int ret = 0;
   if (stream->ptr >= stream->size)
      return EOF;
   ret = stream->buf[stream->ptr++];

   if (stream->ptr > stream->max_ptr)
      stream->max_ptr = stream->ptr;

   return ret;
}

void memstream_putc(memstream_t *stream, int c)
{
   if (stream->ptr < stream->size)
      stream->buf[stream->ptr++] = c;

   if (stream->ptr > stream->max_ptr)
      stream->max_ptr = stream->ptr;
}

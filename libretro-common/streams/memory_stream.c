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
static uint64_t g_size        = 0;
static uint64_t last_file_size = 0;

struct memstream
{
   uint8_t  *buf;      /* pointer first: avoids padding on 64-bit targets   */
   uint64_t  size;
   uint64_t  ptr;
   uint64_t  max_ptr;
   uint8_t   writing;  /* uint8_t instead of unsigned: no trailing padding  */
};

/* -------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------- */

/* Branchless max: returns the larger of a and b without a conditional jump */
#define MEMSTREAM_MAX(a, b) ((a) ^ (((a) ^ (b)) & -((b) > (a))))

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

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

   stream->buf     = g_buffer;
   stream->size    = g_size;
   stream->ptr     = 0;
   stream->max_ptr = 0;
   stream->writing = (uint8_t)writing;

   g_buffer = NULL;
   g_size   = 0;

   return stream;
}

void memstream_close(memstream_t *stream)
{
   if (!stream)
      return;

   last_file_size = stream->writing ? stream->max_ptr : stream->size;
   free(stream);
}

uint64_t memstream_get_last_size(void) { return last_file_size; }

uint64_t memstream_get_ptr(memstream_t *stream) { return stream->ptr; }
uint64_t memstream_pos(memstream_t *stream)     { return stream->ptr; }

uint64_t memstream_read(memstream_t *stream, void *data, uint64_t bytes)
{
   uint64_t avail;

   if (!stream)
      return 0;

   avail = stream->size - stream->ptr;
   if (bytes > avail)
      bytes = avail;

   memcpy(data, stream->buf + stream->ptr, (size_t)bytes);
   stream->ptr += bytes;
   /* max_ptr is only meaningful for write streams; skip the update on reads */
   return bytes;
}

uint64_t memstream_write(memstream_t *stream,
      const void *data, uint64_t bytes)
{
   uint64_t avail;
   uint64_t new_ptr;

   if (!stream)
      return 0;

   avail = stream->size - stream->ptr;
   if (bytes > avail)
      bytes = avail;

   memcpy(stream->buf + stream->ptr, data, (size_t)bytes);
   new_ptr         = stream->ptr + bytes;
   /* Branchless max_ptr update */
   stream->max_ptr = MEMSTREAM_MAX(stream->max_ptr, new_ptr);
   stream->ptr     = new_ptr;
   return bytes;
}

int64_t memstream_seek(memstream_t *stream, int64_t offset, int whence)
{
   uint64_t base;
   uint64_t new_ptr;

   switch (whence)
   {
      case SEEK_SET:
         /* Reject negative absolute offsets before casting */
         if (offset < 0)
            return -1;
         base = 0;
         break;
      case SEEK_CUR:
         base = stream->ptr;
         break;
      case SEEK_END:
         base = stream->writing ? stream->max_ptr : stream->size;
         break;
      default:
         return -1;
   }

   /* Guard against signed underflow wrapping to a huge uint64_t */
   if (offset < 0 && (uint64_t)(-offset) > base)
      return -1;

   new_ptr = base + (uint64_t)offset;

   if (new_ptr <= stream->size)
   {
      stream->ptr = new_ptr;
      return 0;
   }

   return -1;
}

void memstream_rewind(memstream_t *stream)
{
   /* Direct reset is faster than routing through memstream_seek's switch */
   stream->ptr = 0;
}

int memstream_getc(memstream_t *stream)
{
   int c;
   uint64_t new_ptr;

   if (stream->ptr >= stream->size)
      return EOF;

   c               = stream->buf[stream->ptr];
   new_ptr         = stream->ptr + 1;
   /* Branchless max_ptr update */
   stream->max_ptr = MEMSTREAM_MAX(stream->max_ptr, new_ptr);
   stream->ptr     = new_ptr;
   return c;
}

void memstream_putc(memstream_t *stream, int c)
{
   uint64_t new_ptr;

   if (stream->ptr >= stream->size)
      return;

   stream->buf[stream->ptr] = (uint8_t)c;
   new_ptr                  = stream->ptr + 1;
   /* Branchless max_ptr update */
   stream->max_ptr          = MEMSTREAM_MAX(stream->max_ptr, new_ptr);
   stream->ptr              = new_ptr;
}

/* Stub - no line-oriented data in a raw memory buffer */
char *memstream_gets(memstream_t *stream, char *s, size_t len)
{
   (void)stream;
   (void)s;
   (void)len;
   return NULL;
}

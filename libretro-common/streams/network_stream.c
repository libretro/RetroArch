/* Copyright  (C) 2022 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (network_stream.c).
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

#include <retro_endianness.h>

#include <streams/network_stream.h>

bool netstream_open(netstream_t *stream, void *buf, size_t len, size_t used)
{
   if (buf)
   {
      /* Pre-allocated buffer must have a non-zero size. */
      if (!len || used > len)
         return false;
   }
   else
   {
      if (len)
      {
         buf = malloc(len);
         if (!buf)
            return false;
      }

      used = 0;
   }

   stream->buf  = buf;
   stream->size = len;
   stream->used = used;
   stream->pos  = 0;

   return true;
}

void netstream_close(netstream_t *stream, bool dealloc)
{
   if (dealloc)
      free(stream->buf);
   memset(stream, 0, sizeof(*stream));
}

void netstream_reset(netstream_t *stream)
{
   stream->pos  = 0;
   stream->used = 0;
}

bool netstream_truncate(netstream_t *stream, size_t used)
{
   if (used > stream->size)
      return false;

   stream->used = used;

   /* If the current stream position is past our new end of stream,
      set the current position to the end of the stream. */
   if (stream->pos > used)
      stream->pos = used;

   return true;
}

void netstream_data(netstream_t *stream, void **data, size_t *len)
{
   *data = stream->buf;
   *len  = stream->used;
}

bool netstream_eof(netstream_t *stream)
{
   return stream->pos >= stream->used;
}

size_t netstream_tell(netstream_t *stream)
{
   return stream->pos;
}

bool netstream_seek(netstream_t *stream, long offset, int origin)
{
   long pos  = (long)stream->pos;
   long used = (long)stream->used;

   switch (origin)
   {
      case NETSTREAM_SEEK_SET:
         pos  = offset;
         break;
      case NETSTREAM_SEEK_CUR:
         pos += offset;
         break;
      case NETSTREAM_SEEK_END:
         pos  = used + offset;
         break;
      default:
         return false;
   }

   if (pos < 0 || pos > used)
      return false;

   stream->pos = (size_t)pos;

   return true;
}

bool netstream_read(netstream_t *stream, void *data, size_t len)
{
   size_t remaining = stream->used - stream->pos;

   if (!data || !remaining || len > remaining)
      return false;

   /* If len is 0, read all remaining bytes. */
   if (!len)
      len = remaining;

   memcpy(data, (uint8_t*)stream->buf + stream->pos, len);

   stream->pos += len;

   return true;
}

/* This one doesn't require any swapping. */
bool netstream_read_byte(netstream_t *stream, uint8_t *data)
{
   return netstream_read(stream, data, sizeof(*data));
}

#define NETSTREAM_READ_TYPE(name, type, swap) \
bool netstream_read_##name(netstream_t *stream, type *data) \
{ \
   if (!netstream_read(stream, data, sizeof(*data))) \
      return false; \
   *data = swap(*data); \
   return true; \
}

NETSTREAM_READ_TYPE(word,  uint16_t, retro_be_to_cpu16)
NETSTREAM_READ_TYPE(dword, uint32_t, retro_be_to_cpu32)
NETSTREAM_READ_TYPE(qword, uint64_t, retro_be_to_cpu64)

#undef NETSTREAM_READ_TYPE

#ifdef __STDC_IEC_559__
#define NETSTREAM_READ_TYPE(name, type, type_alt, swap) \
bool netstream_read_##name(netstream_t *stream, type *data) \
{ \
   type_alt *data_alt = (type_alt*)data; \
   if (!netstream_read(stream, data, sizeof(*data))) \
      return false; \
   *data_alt = swap(*data_alt); \
   return true; \
}

NETSTREAM_READ_TYPE(float,  float,  uint32_t, retro_be_to_cpu32)
NETSTREAM_READ_TYPE(double, double, uint64_t, retro_be_to_cpu64)

#undef NETSTREAM_READ_TYPE
#endif

int netstream_read_string(netstream_t *stream, char *s, size_t len)
{
   char c;
   int ret = 0;

   if (!s || !len)
      return -1;

   for (; --len; ret++)
   {
      if (!netstream_read(stream, &c, sizeof(c)))
         return -1;

      *s++ = c;

      if (!c)
         break;
   }

   if (!len)
   {
      *s = '\0';

      for (;; ret++)
      {
         if (!netstream_read(stream, &c, sizeof(c)))
            return -1;
         if (!c)
            break;
      }
   }

   return ret;
}

bool netstream_read_fixed_string(netstream_t *stream, char *s, size_t len)
{
   if (!len)
      return false;

   if (!netstream_read(stream, s, len))
      return false;

   /* Ensure the string is always null-terminated. */
   s[len - 1] = '\0';

   return true;
}

bool netstream_write(netstream_t *stream, const void *data, size_t len)
{
   size_t remaining = stream->size - stream->pos;

   if (!data || !len)
      return false;

   if (len > remaining)
   {
      if (!stream->size)
      {
         if (stream->buf)
            free(stream->buf);
         stream->buf  = malloc(len);
         if (!stream->buf)
            return false;
         stream->size = len;
      }
      else
      {
         size_t _len = stream->size + (len - remaining);
         void   *buf = realloc(stream->buf, _len);

         if (!buf)
            return false;

         stream->buf  = buf;
         stream->size = _len;
      }
   }

   memcpy((uint8_t*)stream->buf + stream->pos, data, len);

   stream->pos += len;

   if (stream->pos > stream->used)
      stream->used = stream->pos;

   return true;
}

/* This one doesn't require any swapping. */
bool netstream_write_byte(netstream_t *stream, uint8_t data)
{
   return netstream_write(stream, &data, sizeof(data));
}

#define NETSTREAM_WRITE_TYPE(name, type, swap) \
bool netstream_write_##name(netstream_t *stream, type data) \
{ \
   data = swap(data); \
   return netstream_write(stream, &data, sizeof(data)); \
}

NETSTREAM_WRITE_TYPE(word,  uint16_t, retro_cpu_to_be16)
NETSTREAM_WRITE_TYPE(dword, uint32_t, retro_cpu_to_be32)
NETSTREAM_WRITE_TYPE(qword, uint64_t, retro_cpu_to_be64)

#undef NETSTREAM_WRITE_TYPE

#ifdef __STDC_IEC_559__
#define NETSTREAM_WRITE_TYPE(name, type, type_alt, swap) \
bool netstream_write_##name(netstream_t *stream, type data) \
{ \
   type_alt *data_alt = (type_alt*)&data; \
   *data_alt = swap(*data_alt); \
   return netstream_write(stream, &data, sizeof(data)); \
}

NETSTREAM_WRITE_TYPE(float,  float,  uint32_t, retro_cpu_to_be32)
NETSTREAM_WRITE_TYPE(double, double, uint64_t, retro_cpu_to_be64)

#undef NETSTREAM_WRITE_TYPE
#endif

bool netstream_write_string(netstream_t *stream, const char *s)
{
   if (!s)
      return false;

   return netstream_write(stream, s, strlen(s) + 1);
}

bool netstream_write_fixed_string(netstream_t *stream, const char *s,
      size_t len)
{
   char end = '\0';

   if (!netstream_write(stream, s, len))
      return false;

   /* Ensure the string is always null-terminated. */
   netstream_seek(stream, -1, NETSTREAM_SEEK_CUR);
   netstream_write(stream, &end, sizeof(end));

   return true;
}

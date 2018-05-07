/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rmsgpack.c).
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
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <retro_endianness.h>

#include "rmsgpack.h"

#define _MPF_FIXMAP     0x80
#define _MPF_MAP16      0xde
#define _MPF_MAP32      0xdf

#define _MPF_FIXARRAY   0x90
#define _MPF_ARRAY16    0xdc
#define _MPF_ARRAY32    0xdd

#define _MPF_FIXSTR     0xa0
#define _MPF_STR8       0xd9
#define _MPF_STR16      0xda
#define _MPF_STR32      0xdb

#define _MPF_BIN8       0xc4
#define _MPF_BIN16      0xc5
#define _MPF_BIN32      0xc6

#define _MPF_FALSE      0xc2
#define _MPF_TRUE       0xc3

#define _MPF_INT8       0xd0
#define _MPF_INT16      0xd1
#define _MPF_INT32      0xd2
#define _MPF_INT64      0xd3

#define _MPF_UINT8      0xcc
#define _MPF_UINT16     0xcd
#define _MPF_UINT32     0xce
#define _MPF_UINT64     0xcf

#define _MPF_NIL        0xc0

static const uint8_t MPF_FIXMAP   = _MPF_FIXMAP;
static const uint8_t MPF_MAP16    = _MPF_MAP16;
static const uint8_t MPF_MAP32    = _MPF_MAP32;

static const uint8_t MPF_FIXARRAY = _MPF_FIXARRAY;
static const uint8_t MPF_ARRAY16  = _MPF_ARRAY16;
static const uint8_t MPF_ARRAY32  = _MPF_ARRAY32;

static const uint8_t MPF_FIXSTR   = _MPF_FIXSTR;
static const uint8_t MPF_STR8     = _MPF_STR8;
static const uint8_t MPF_STR16    = _MPF_STR16;
static const uint8_t MPF_STR32    = _MPF_STR32;

static const uint8_t MPF_BIN8     = _MPF_BIN8;
static const uint8_t MPF_BIN16    = _MPF_BIN16;
static const uint8_t MPF_BIN32    = _MPF_BIN32;

static const uint8_t MPF_FALSE    = _MPF_FALSE;
static const uint8_t MPF_TRUE     = _MPF_TRUE;

static const uint8_t MPF_INT8     = _MPF_INT8;
static const uint8_t MPF_INT16    = _MPF_INT16;
static const uint8_t MPF_INT32    = _MPF_INT32;
static const uint8_t MPF_INT64    = _MPF_INT64;

static const uint8_t MPF_UINT8    = _MPF_UINT8;
static const uint8_t MPF_UINT16   = _MPF_UINT16;
static const uint8_t MPF_UINT32   = _MPF_UINT32;
static const uint8_t MPF_UINT64   = _MPF_UINT64;

static const uint8_t MPF_NIL      = _MPF_NIL;

int rmsgpack_write_array_header(RFILE *fd, uint32_t size)
{
   uint16_t tmp_i16;
   uint32_t tmp_i32;

   if (size < 16)
   {
      size = (size | MPF_FIXARRAY);
      if (filestream_write(fd, &size, sizeof(int8_t)) == -1)
         goto error;
      return sizeof(int8_t);
   }
   else if (size == (uint16_t)size)
   {
      if (filestream_write(fd, &MPF_ARRAY16, sizeof(MPF_ARRAY16)) == -1)
         goto error;
      tmp_i16 = swap_if_little16(size);
      if (filestream_write(fd, (void *)(&tmp_i16), sizeof(uint16_t)) == -1)
         goto error;
      return sizeof(int8_t) + sizeof(uint16_t);
   }

   if (filestream_write(fd, &MPF_ARRAY32, sizeof(MPF_ARRAY32)) == -1)
      goto error;

   tmp_i32 = swap_if_little32(size);

   if (filestream_write(fd, (void *)(&tmp_i32), sizeof(uint32_t)) == -1)
      goto error;

   return sizeof(int8_t) + sizeof(uint32_t);

error:
   return -errno;
}

int rmsgpack_write_map_header(RFILE *fd, uint32_t size)
{
   uint16_t tmp_i16;
   uint32_t tmp_i32;

   if (size < 16)
   {
      size = (size | MPF_FIXMAP);
      if (filestream_write(fd, &size, sizeof(int8_t)) == -1)
         goto error;
      return sizeof(int8_t);
   }
   else if (size < (uint16_t)size)
   {
      if (filestream_write(fd, &MPF_MAP16, sizeof(MPF_MAP16)) == -1)
         goto error;
      tmp_i16 = swap_if_little16(size);
      if (filestream_write(fd, (void *)(&tmp_i16), sizeof(uint16_t)) == -1)
         goto error;
      return sizeof(uint8_t) + sizeof(uint16_t);
   }

   tmp_i32 = swap_if_little32(size);
   if (filestream_write(fd, &MPF_MAP32, sizeof(MPF_MAP32)) == -1)
      goto error;
   if (filestream_write(fd, (void *)(&tmp_i32), sizeof(uint32_t)) == -1)
      goto error;

   return sizeof(int8_t) + sizeof(uint32_t);

error:
   return -errno;
}

int rmsgpack_write_string(RFILE *fd, const char *s, uint32_t len)
{
   uint16_t tmp_i16;
   uint32_t tmp_i32;
   int8_t fixlen = 0;
   int written   = sizeof(int8_t);

   if (len < 32)
   {
      fixlen = len | MPF_FIXSTR;
      if (filestream_write(fd, &fixlen, sizeof(int8_t)) == -1)
         goto error;
   }
   else if (len < (1 << 8))
   {
      if (filestream_write(fd, &MPF_STR8, sizeof(MPF_STR8)) == -1)
         goto error;
      if (filestream_write(fd, &len, sizeof(uint8_t)) == -1)
         goto error;
      written += sizeof(uint8_t);
   }
   else if (len < (1 << 16))
   {
      if (filestream_write(fd, &MPF_STR16, sizeof(MPF_STR16)) == -1)
         goto error;
      tmp_i16 = swap_if_little16(len);
      if (filestream_write(fd, &tmp_i16, sizeof(uint16_t)) == -1)
         goto error;
      written += sizeof(uint16_t);
   }
   else
   {
      if (filestream_write(fd, &MPF_STR32, sizeof(MPF_STR32)) == -1)
         goto error;
      tmp_i32 = swap_if_little32(len);
      if (filestream_write(fd, &tmp_i32, sizeof(uint32_t)) == -1)
         goto error;
      written += sizeof(uint32_t);
   }

   if (filestream_write(fd, s, len) == -1)
      goto error;

   written += len;

   return written;

error:
   return -errno;
}

int rmsgpack_write_bin(RFILE *fd, const void *s, uint32_t len)
{
   uint16_t tmp_i16;
   uint32_t tmp_i32;

   if (len == (uint8_t)len)
   {
      if (filestream_write(fd, &MPF_BIN8, sizeof(MPF_BIN8)) == -1)
         goto error;
      if (filestream_write(fd, &len, sizeof(uint8_t)) == -1)
         goto error;
   }
   else if (len == (uint16_t)len)
   {
      if (filestream_write(fd, &MPF_BIN16, sizeof(MPF_BIN16)) == -1)
         goto error;
      tmp_i16 = swap_if_little16(len);
      if (filestream_write(fd, &tmp_i16, sizeof(uint16_t)) == -1)
         goto error;
   }
   else
   {
      if (filestream_write(fd, &MPF_BIN32, sizeof(MPF_BIN32)) == -1)
         goto error;
      tmp_i32 = swap_if_little32(len);
      if (filestream_write(fd, &tmp_i32, sizeof(uint32_t)) == -1)
         goto error;
   }

   if (filestream_write(fd, s, len) == -1)
      goto error;

   return 0;

error:
   return -errno;
}

int rmsgpack_write_nil(RFILE *fd)
{
   if (filestream_write(fd, &MPF_NIL, sizeof(MPF_NIL)) == -1)
      return -errno;
   return sizeof(uint8_t);
}

int rmsgpack_write_bool(RFILE *fd, int value)
{
   if (value)
   {
      if (filestream_write(fd, &MPF_TRUE, sizeof(MPF_TRUE)) == -1)
         goto error;
   }

   if (filestream_write(fd, &MPF_FALSE, sizeof(MPF_FALSE)) == -1)
      goto error;

   return sizeof(uint8_t);

error:
   return -errno;
}

int rmsgpack_write_int(RFILE *fd, int64_t value)
{
   int16_t tmp_i16;
   int32_t tmp_i32;
   uint8_t tmpval  = 0;
   int     written = sizeof(uint8_t);

   if (value >=0 && value < 128)
   {
      if (filestream_write(fd, &value, sizeof(int8_t)) == -1)
         goto error;
   }
   else if (value < 0 && value > -32)
   {
      tmpval = (value) | 0xe0;
      if (filestream_write(fd, &tmpval, sizeof(uint8_t)) == -1)
         goto error;
   }
   else if (value == (int8_t)value)
   {
      if (filestream_write(fd, &MPF_INT8, sizeof(MPF_INT8)) == -1)
         goto error;

      if (filestream_write(fd, &value, sizeof(int8_t)) == -1)
         goto error;
      written += sizeof(int8_t);
   }
   else if (value == (int16_t)value)
   {
      if (filestream_write(fd, &MPF_INT16, sizeof(MPF_INT16)) == -1)
         goto error;

      tmp_i16 = swap_if_little16((uint16_t)value);
      if (filestream_write(fd, &tmp_i16, sizeof(int16_t)) == -1)
         goto error;
      written += sizeof(int16_t);
   }
   else if (value == (int32_t)value)
   {
      if (filestream_write(fd, &MPF_INT32, sizeof(MPF_INT32)) == -1)
         goto error;

      tmp_i32 = swap_if_little32((uint32_t)value);
      if (filestream_write(fd, &tmp_i32, sizeof(int32_t)) == -1)
         goto error;
      written += sizeof(int32_t);
   }
   else
   {
      if (filestream_write(fd, &MPF_INT64, sizeof(MPF_INT64)) == -1)
         goto error;

      value = swap_if_little64(value);
      if (filestream_write(fd, &value, sizeof(int64_t)) == -1)
         goto error;
      written += sizeof(int64_t);
   }

   return written;

error:
   return -errno;
}

int rmsgpack_write_uint(RFILE *fd, uint64_t value)
{
   uint16_t tmp_i16;
   uint32_t tmp_i32;
   int written = sizeof(uint8_t);

   if (value == (uint8_t)value)
   {
      if (filestream_write(fd, &MPF_UINT8, sizeof(MPF_UINT8)) == -1)
         goto error;

      if (filestream_write(fd, &value, sizeof(uint8_t)) == -1)
         goto error;
      written += sizeof(uint8_t);
   }
   else if (value == (uint16_t)value)
   {
      if (filestream_write(fd, &MPF_UINT16, sizeof(MPF_UINT16)) == -1)
         goto error;

      tmp_i16 = swap_if_little16((uint16_t)value);
      if (filestream_write(fd, &tmp_i16, sizeof(uint16_t)) == -1)
         goto error;
      written += sizeof(uint16_t);
   }
   else if (value == (uint32_t)value)
   {
      if (filestream_write(fd, &MPF_UINT32, sizeof(MPF_UINT32)) == -1)
         goto error;

      tmp_i32 = swap_if_little32((uint32_t)value);
      if (filestream_write(fd, &tmp_i32, sizeof(uint32_t)) == -1)
         goto error;
      written += sizeof(uint32_t);
   }
   else
   {
      if (filestream_write(fd, &MPF_UINT64, sizeof(MPF_UINT64)) == -1)
         goto error;

      value = swap_if_little64(value);
      if (filestream_write(fd, &value, sizeof(uint64_t)) == -1)
         goto error;
      written += sizeof(uint64_t);
   }
   return written;

error:
   return -errno;
}

static int read_uint(RFILE *fd, uint64_t *out, size_t size)
{
   uint64_t tmp;

   if (filestream_read(fd, &tmp, size) == -1)
      goto error;

   switch (size)
   {
      case 1:
         *out = *(uint8_t *)(&tmp);
         break;
      case 2:
         *out = swap_if_little16((uint16_t)tmp);
         break;
      case 4:
         *out = swap_if_little32((uint32_t)tmp);
         break;
      case 8:
         *out = swap_if_little64(tmp);
         break;
   }
   return 0;

error:
   return -errno;
}

static int read_int(RFILE *fd, int64_t *out, size_t size)
{
   uint8_t tmp8 = 0;
   uint16_t tmp16;
   uint32_t tmp32;
   uint64_t tmp64;

   if (filestream_read(fd, &tmp64, size) == -1)
      goto error;

   (void)tmp8;

   switch (size)
   {
      case 1:
         *out = *((int8_t *)(&tmp64));
         break;
      case 2:
         tmp16 = swap_if_little16((uint16_t)tmp64);
         *out = *((int16_t *)(&tmp16));
         break;
      case 4:
         tmp32 = swap_if_little32((uint32_t)tmp64);
         *out = *((int32_t *)(&tmp32));
         break;
      case 8:
         tmp64 = swap_if_little64(tmp64);
         *out = *((int64_t *)(&tmp64));
         break;
   }
   return 0;

error:
   return -errno;
}

static int read_buff(RFILE *fd, size_t size, char **pbuff, uint64_t *len)
{
   uint64_t tmp_len = 0;
   ssize_t read_len = 0;

   if (read_uint(fd, &tmp_len, size) == -1)
      return -errno;

   *pbuff = (char *)malloc((size_t)(tmp_len + 1) * sizeof(char));

   if ((read_len = filestream_read(fd, *pbuff, (size_t)tmp_len)) == -1)
      goto error;

   *len = read_len;
   (*pbuff)[read_len] = 0;

   /* Throw warning on read_len != tmp_len ? */

   return 0;

error:
   free(*pbuff);
   *pbuff = NULL;
   return -errno;
}

static int read_map(RFILE *fd, uint32_t len,
        struct rmsgpack_read_callbacks *callbacks, void *data)
{
   int rv;
   unsigned i;

   if (callbacks->read_map_start &&
         (rv = callbacks->read_map_start(len, data)) < 0)
      return rv;

   for (i = 0; i < len; i++)
   {
      if ((rv = rmsgpack_read(fd, callbacks, data)) < 0)
         return rv;
      if ((rv = rmsgpack_read(fd, callbacks, data)) < 0)
         return rv;
   }

   return 0;
}

static int read_array(RFILE *fd, uint32_t len,
      struct rmsgpack_read_callbacks *callbacks, void *data)
{
   int rv;
   unsigned i;

   if (callbacks->read_array_start &&
         (rv = callbacks->read_array_start(len, data)) < 0)
      return rv;

   for (i = 0; i < len; i++)
   {
      if ((rv = rmsgpack_read(fd, callbacks, data)) < 0)
         return rv;
   }

   return 0;
}

int rmsgpack_read(RFILE *fd,
      struct rmsgpack_read_callbacks *callbacks, void *data)
{
   int rv;
   uint64_t tmp_len  = 0;
   uint64_t tmp_uint = 0;
   int64_t tmp_int   = 0;
   uint8_t type      = 0;
   char *buff        = NULL;

   if (filestream_read(fd, &type, sizeof(uint8_t)) == -1)
      goto error;

   if (type < MPF_FIXMAP)
   {
      if (!callbacks->read_int)
         return 0;
      return callbacks->read_int(type, data);
   }
   else if (type < MPF_FIXARRAY)
   {
      tmp_len = type - MPF_FIXMAP;
      return read_map(fd, (uint32_t)tmp_len, callbacks, data);
   }
   else if (type < MPF_FIXSTR)
   {
      tmp_len = type - MPF_FIXARRAY;
      return read_array(fd, (uint32_t)tmp_len, callbacks, data);
   }
   else if (type < MPF_NIL)
   {
      ssize_t read_len = 0;
      tmp_len = type - MPF_FIXSTR;
      buff = (char *)malloc((size_t)(tmp_len + 1) * sizeof(char));
      if (!buff)
         return -ENOMEM;
      if ((read_len = filestream_read(fd, buff, (ssize_t)tmp_len)) == -1)
      {
         free(buff);
         goto error;
      }
      buff[read_len] = '\0';
      if (!callbacks->read_string)
      {
         free(buff);
         return 0;
      }
      return callbacks->read_string(buff, (uint32_t)read_len, data);
   }
   else if (type > MPF_MAP32)
   {
      if (!callbacks->read_int)
         return 0;
      return callbacks->read_int(type - 0xff - 1, data);
   }

   switch (type)
   {
      case _MPF_NIL:
         if (callbacks->read_nil)
            return callbacks->read_nil(data);
         break;
      case _MPF_FALSE:
         if (callbacks->read_bool)
            return callbacks->read_bool(0, data);
         break;
      case _MPF_TRUE:
         if (callbacks->read_bool)
            return callbacks->read_bool(1, data);
         break;
      case _MPF_BIN8:
      case _MPF_BIN16:
      case _MPF_BIN32:
         if ((rv = read_buff(fd, (size_t)(1 << (type - _MPF_BIN8)),
                     &buff, &tmp_len)) < 0)
            return rv;

         if (callbacks->read_bin)
            return callbacks->read_bin(buff, (uint32_t)tmp_len, data);
         break;
      case _MPF_UINT8:
      case _MPF_UINT16:
      case _MPF_UINT32:
      case _MPF_UINT64:
         tmp_len  = UINT64_C(1) << (type - _MPF_UINT8);
         tmp_uint = 0;
         if (read_uint(fd, &tmp_uint, (size_t)tmp_len) == -1)
            goto error;

         if (callbacks->read_uint)
            return callbacks->read_uint(tmp_uint, data);
         break;
      case _MPF_INT8:
      case _MPF_INT16:
      case _MPF_INT32:
      case _MPF_INT64:
         tmp_len = UINT64_C(1) << (type - _MPF_INT8);
         tmp_int = 0;
         if (read_int(fd, &tmp_int, (size_t)tmp_len) == -1)
            goto error;

         if (callbacks->read_int)
            return callbacks->read_int(tmp_int, data);
         break;
      case _MPF_STR8:
      case _MPF_STR16:
      case _MPF_STR32:
         if ((rv = read_buff(fd, (size_t)(1 << (type - _MPF_STR8)), &buff, &tmp_len)) < 0)
            return rv;

         if (callbacks->read_string)
            return callbacks->read_string(buff, (uint32_t)tmp_len, data);
         break;
      case _MPF_ARRAY16:
      case _MPF_ARRAY32:
         if (read_uint(fd, &tmp_len, 2<<(type - _MPF_ARRAY16)) == -1)
            goto error;
         return read_array(fd, (uint32_t)tmp_len, callbacks, data);
      case _MPF_MAP16:
      case _MPF_MAP32:
         if (read_uint(fd, &tmp_len, 2<<(type - _MPF_MAP16)) == -1)
            goto error;
         return read_map(fd, (uint32_t)tmp_len, callbacks, data);
   }

   if (buff)
      free(buff);
   return 0;

error:
   return -errno;
}

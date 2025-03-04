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
static const uint8_t MPF_MAP32    = _MPF_MAP32;

static const uint8_t MPF_FIXARRAY = _MPF_FIXARRAY;

static const uint8_t MPF_FIXSTR   = _MPF_FIXSTR;

static const uint8_t MPF_NIL      = _MPF_NIL;

int rmsgpack_write_array_header(RFILE *fd, uint32_t size)
{
   if (size < 16)
   {
      size = (size | MPF_FIXARRAY);
      if (filestream_write(fd, &size, sizeof(int8_t)) != -1)
         return sizeof(int8_t);
   }
   else if (size == (uint16_t)size)
   {
      static const uint8_t MPF_ARRAY16  = _MPF_ARRAY16;
      if (filestream_write(fd, &MPF_ARRAY16, sizeof(MPF_ARRAY16)) != -1)
      {
         uint16_t tmp_i16 = swap_if_little16(size);
         if (filestream_write(fd, (void *)(&tmp_i16), sizeof(uint16_t)) != -1)
            return sizeof(int8_t) + sizeof(uint16_t);
      }
   }
   else
   {
      static const uint8_t MPF_ARRAY32  = _MPF_ARRAY32;
      if (filestream_write(fd, &MPF_ARRAY32, sizeof(MPF_ARRAY32)) != -1)
      {
         uint32_t tmp_i32 = swap_if_little32(size);
         if (filestream_write(fd, (void *)(&tmp_i32), sizeof(uint32_t)) != -1)
            return sizeof(int8_t) + sizeof(uint32_t);
      }
   }
   return -1;
}

int rmsgpack_write_map_header(RFILE *fd, uint32_t size)
{
   if (size < 16)
   {
      size = (size | MPF_FIXMAP);
      if (filestream_write(fd, &size, sizeof(int8_t)) != -1)
         return sizeof(int8_t);
   }
   else if (size == (uint16_t)size)
   {
      static const uint8_t MPF_MAP16    = _MPF_MAP16;
      if (filestream_write(fd, &MPF_MAP16, sizeof(MPF_MAP16)) != -1)
      {
         uint16_t tmp_i16 = swap_if_little16(size);
         if (filestream_write(fd, (void *)(&tmp_i16), sizeof(uint16_t)) != -1)
            return sizeof(uint8_t) + sizeof(uint16_t);
      }
   }
   else
   {
      if (filestream_write(fd, &MPF_MAP32, sizeof(MPF_MAP32)) != -1)
      {
         uint32_t tmp_i32 = swap_if_little32(size);
         if (filestream_write(fd, (void *)(&tmp_i32), sizeof(uint32_t)) != -1)
            return sizeof(int8_t) + sizeof(uint32_t);
      }
   }
   return -1;
}

int rmsgpack_write_string(RFILE *fd, const char *s, uint32_t len)
{
   if (len < 32)
   {
      uint8_t tmp_i8 = len | MPF_FIXSTR;
      if (filestream_write(fd, &tmp_i8, sizeof(uint8_t)) != -1)
         if (filestream_write(fd, s, len) != -1)
            return (sizeof(uint8_t) + len);
   }
   else if (len == (uint8_t)len)
   {
      static const uint8_t MPF_STR8     = _MPF_STR8;
      if (filestream_write(fd, &MPF_STR8, sizeof(MPF_STR8)) != -1)
      {
         uint8_t tmp_i8 = (uint8_t)len;
         if (filestream_write(fd, &tmp_i8, sizeof(uint8_t)) != -1)
         {
            int written = sizeof(uint8_t) + sizeof(uint8_t);
            if (filestream_write(fd, s, len) != -1)
               return written + len;
         }
      }
   }
   else if (len == (uint16_t)len)
   {
      static const uint8_t MPF_STR16    = _MPF_STR16;
      if (filestream_write(fd, &MPF_STR16, sizeof(MPF_STR16)) != -1)
      {
         uint16_t tmp_i16 = swap_if_little16(len);
         if (filestream_write(fd, &tmp_i16, sizeof(uint16_t)) != -1)
         {
            int written = sizeof(uint8_t) + sizeof(uint16_t);
            if (filestream_write(fd, s, len) != -1)
               return written + len;
         }
      }
   }
   else
   {
      static const uint8_t MPF_STR32    = _MPF_STR32;
      if (filestream_write(fd, &MPF_STR32, sizeof(MPF_STR32)) != -1)
      {
         uint32_t tmp_i32 = swap_if_little32(len);
         if (filestream_write(fd, &tmp_i32, sizeof(uint32_t)) != -1)
         {
            int written = sizeof(uint8_t) + sizeof(uint32_t);
            if (filestream_write(fd, s, len) != -1)
               return written + len;
         }
      }
   }
   return -1;
}

int rmsgpack_write_bin(RFILE *fd, const void *s, uint32_t len)
{
   if (len == (uint8_t)len)
   {
      static const uint8_t MPF_BIN8     = _MPF_BIN8;
      if (filestream_write(fd, &MPF_BIN8, sizeof(MPF_BIN8)) != -1)
      {
         uint8_t tmp_i8 = (uint8_t)len;
         if (filestream_write(fd, &tmp_i8, sizeof(uint8_t)) != -1)
            if (filestream_write(fd, s, len) != -1)
               return 0;
      }
   }
   else if (len == (uint16_t)len)
   {
      static const uint8_t MPF_BIN16    = _MPF_BIN16;
      if (filestream_write(fd, &MPF_BIN16, sizeof(MPF_BIN16)) != -1)
      {
         uint16_t tmp_i16 = swap_if_little16(len);
         if (filestream_write(fd, &tmp_i16, sizeof(uint16_t)) != -1)
            if (filestream_write(fd, s, len) != -1)
               return 0;
      }
   }
   else
   {
      static const uint8_t MPF_BIN32    = _MPF_BIN32;
      if (filestream_write(fd, &MPF_BIN32, sizeof(MPF_BIN32)) != -1)
      {
         uint32_t tmp_i32 = swap_if_little32(len);
         if (filestream_write(fd, &tmp_i32, sizeof(uint32_t)) != -1)
            if (filestream_write(fd, s, len) != -1)
               return 0;
      }
   }
   return -1;
}

int rmsgpack_write_nil(RFILE *fd)
{
   if (filestream_write(fd, &MPF_NIL, sizeof(MPF_NIL)) == -1)
      return -1;
   return sizeof(uint8_t);
}

int rmsgpack_write_bool(RFILE *fd, int value)
{
   static const uint8_t MPF_FALSE    = _MPF_FALSE;
   if (value)
   {
      static const uint8_t MPF_TRUE  = _MPF_TRUE;
      if (filestream_write(fd, &MPF_TRUE, sizeof(MPF_TRUE)) == -1)
         return -1;
   }

   if (filestream_write(fd, &MPF_FALSE, sizeof(MPF_FALSE)) == -1)
      return -1;

   return sizeof(uint8_t);
}

int rmsgpack_write_int(RFILE *fd, int64_t value)
{
   if (value >= 0 && value < 128)
   {
      uint8_t tmpval = (uint8_t)value;
      if (filestream_write(fd, &tmpval, sizeof(uint8_t)) != -1)
         return sizeof(uint8_t);
   }
   else if (value >= -32 && value < 0)
   {
      uint8_t tmpval = (uint8_t)(value + 256); /* -32..-1 => 0xE0 .. 0xFF */
      if (filestream_write(fd, &tmpval, sizeof(uint8_t)) != -1)
         return sizeof(uint8_t);
   }
   else if (value == (int8_t)value)
   {
      static const uint8_t MPF_INT8     = _MPF_INT8;
      if (filestream_write(fd, &MPF_INT8, sizeof(MPF_INT8)) != -1)
      {
         int8_t tmp_i8 = (int8_t)value;
         if (filestream_write(fd, &tmp_i8, sizeof(int8_t)) != -1)
            return (sizeof(uint8_t) + sizeof(int8_t));
      }
   }
   else if (value == (int16_t)value)
   {
      static const uint8_t MPF_INT16    = _MPF_INT16;
      if (filestream_write(fd, &MPF_INT16, sizeof(MPF_INT16)) != -1)
      {
         int16_t tmp_i16 = swap_if_little16((uint16_t)value);
         if (filestream_write(fd, &tmp_i16, sizeof(int16_t)) != -1)
            return (sizeof(uint8_t) + sizeof(int16_t));
      }
   }
   else if (value == (int32_t)value)
   {
      static const uint8_t MPF_INT32    = _MPF_INT32;
      if (filestream_write(fd, &MPF_INT32, sizeof(MPF_INT32)) != -1)
      {
         int32_t tmp_i32 = swap_if_little32((uint32_t)value);
         if (filestream_write(fd, &tmp_i32, sizeof(int32_t)) != -1)
            return (sizeof(uint8_t) + sizeof(int32_t));
      }
   }
   else
   {
      static const uint8_t MPF_INT64    = _MPF_INT64;
      if (filestream_write(fd, &MPF_INT64, sizeof(MPF_INT64)) != -1)
      {
         value = swap_if_little64(value);
         if (filestream_write(fd, &value, sizeof(int64_t)) != -1)
            return (sizeof(uint8_t) + sizeof(int64_t));
      }
   }

   return -1;
}

int rmsgpack_write_uint(RFILE *fd, uint64_t value)
{
   if (value == (uint8_t)value)
   {
      static const uint8_t MPF_UINT8    = _MPF_UINT8;
      if (filestream_write(fd, &MPF_UINT8, sizeof(MPF_UINT8)) != -1)
      {
         uint8_t tmp_i8 = (uint8_t)value;
         if (filestream_write(fd, &tmp_i8, sizeof(uint8_t)) != -1)
            return (sizeof(uint8_t) + sizeof(uint8_t));
      }
   }
   else if (value == (uint16_t)value)
   {
      static const uint8_t MPF_UINT16   = _MPF_UINT16;
      if (filestream_write(fd, &MPF_UINT16, sizeof(MPF_UINT16)) != -1)
      {
         uint16_t tmp_i16 = swap_if_little16((uint16_t)value);
         if (filestream_write(fd, &tmp_i16, sizeof(uint16_t)) != -1)
            return (sizeof(uint8_t) + sizeof(uint16_t));
      }
   }
   else if (value == (uint32_t)value)
   {
      static const uint8_t MPF_UINT32   = _MPF_UINT32;
      if (filestream_write(fd, &MPF_UINT32, sizeof(MPF_UINT32)) != -1)
      {
         uint32_t tmp_i32 = swap_if_little32((uint32_t)value);
         if (filestream_write(fd, &tmp_i32, sizeof(uint32_t)) != -1)
            return (sizeof(uint8_t) + sizeof(uint32_t));
      }
   }
   else
   {
      static const uint8_t MPF_UINT64   = _MPF_UINT64;
      if (filestream_write(fd, &MPF_UINT64, sizeof(MPF_UINT64)) != -1)
      {
         value = swap_if_little64(value);
         if (filestream_write(fd, &value, sizeof(uint64_t)) != -1)
            return (sizeof(uint8_t) + sizeof(uint64_t));
      }
   }
   return -1;
}

static int rmsgpack_read_uint(RFILE *fd, uint64_t *s, size_t len)
{
   union { uint64_t u64; uint32_t u32; uint16_t u16; uint8_t u8; } tmp;

   if (filestream_read(fd, &tmp, len) == -1)
      return -1;

   switch (len)
   {
      case 1:
         *s = tmp.u8;
         break;
      case 2:
         *s = swap_if_little16(tmp.u16);
         break;
      case 4:
         *s = swap_if_little32(tmp.u32);
         break;
      case 8:
         *s = swap_if_little64(tmp.u64);
         break;
   }
   return 0;
}

static int rmsgpack_read_int(RFILE *fd, int64_t *s, size_t len)
{
   union { uint64_t u64; uint32_t u32; uint16_t u16; uint8_t u8; } tmp;

   if (filestream_read(fd, &tmp, len) == -1)
      return -1;

   switch (len)
   {
      case 1:
         *s = (int8_t)tmp.u8;
         break;
      case 2:
         *s = (int16_t)swap_if_little16(tmp.u16);
         break;
      case 4:
         *s = (int32_t)swap_if_little32(tmp.u32);
         break;
      case 8:
         *s = (int64_t)swap_if_little64(tmp.u64);
         break;
   }
   return 0;
}

static int rmsgpack_read_buff(RFILE *fd, size_t size, char **pbuff, uint64_t *len)
{
   ssize_t read_len;
   uint64_t tmp_len   = 0;

   if (rmsgpack_read_uint(fd, &tmp_len, size) == -1)
      return -1;

   *pbuff             = (char *)malloc((size_t)(tmp_len + 1) * sizeof(char));

   if ((read_len      = filestream_read(fd, *pbuff, (size_t)tmp_len)) == -1)
   {
      free(*pbuff);
      *pbuff = NULL;
      return -1;
   }

   *len               = read_len;
   (*pbuff)[read_len] = 0;

   /* Throw warning on read_len != tmp_len ? */
   return 0;
}

static int rmsgpack_read_map(RFILE *fd, uint32_t len,
        struct rmsgpack_read_callbacks *callbacks, void *data)
{
   int rv;
   unsigned i;

   if (     (     callbacks->read_map_start)
         && (rv = callbacks->read_map_start(len, data)) < 0)
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

static int rmsgpack_read_array(RFILE *fd, uint32_t len,
      struct rmsgpack_read_callbacks *callbacks, void *data)
{
   int rv;
   unsigned i;

   if (     (     callbacks->read_array_start)
         && (rv = callbacks->read_array_start(len, data)) < 0)
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
      return -1;

   if (type < MPF_FIXMAP)
   {
      if (callbacks->read_int)
         return callbacks->read_int(type, data);
      return 0;
   }
   else if (type < MPF_FIXARRAY)
   {
      tmp_len = type - MPF_FIXMAP;
      return rmsgpack_read_map(fd, (uint32_t)tmp_len, callbacks, data);
   }
   else if (type < MPF_FIXSTR)
   {
      tmp_len = type - MPF_FIXARRAY;
      return rmsgpack_read_array(fd, (uint32_t)tmp_len, callbacks, data);
   }
   else if (type < MPF_NIL)
   {
      ssize_t read_len = 0;
      tmp_len          = type - MPF_FIXSTR;
      if (!(buff = (char *)malloc((size_t)(tmp_len + 1) * sizeof(char))))
         return -1;
      if ((read_len = filestream_read(fd, buff, (ssize_t)tmp_len)) == -1)
      {
         free(buff);
         return -1;
      }
      buff[read_len] = '\0';
      if (callbacks->read_string)
         return callbacks->read_string(buff, (uint32_t)read_len, data);
      goto end;
   }
   else if (type > MPF_MAP32)
   {
      if (callbacks->read_int)
         return callbacks->read_int(type - 0xff - 1, data);
      return 0;
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
         if ((rv = rmsgpack_read_buff(fd, (size_t)(1 << (type - _MPF_BIN8)),
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
         if (rmsgpack_read_uint(fd, &tmp_uint, (size_t)tmp_len) == -1)
            return -1;

         if (callbacks->read_uint)
            return callbacks->read_uint(tmp_uint, data);
         break;
      case _MPF_INT8:
      case _MPF_INT16:
      case _MPF_INT32:
      case _MPF_INT64:
         tmp_len = UINT64_C(1) << (type - _MPF_INT8);
         tmp_int = 0;
         if (rmsgpack_read_int(fd, &tmp_int, (size_t)tmp_len) == -1)
            return -1;

         if (callbacks->read_int)
            return callbacks->read_int(tmp_int, data);
         break;
      case _MPF_STR8:
      case _MPF_STR16:
      case _MPF_STR32:
         if ((rv = rmsgpack_read_buff(fd, (size_t)(1 << (type - _MPF_STR8)), &buff, &tmp_len)) < 0)
            return rv;

         if (callbacks->read_string)
            return callbacks->read_string(buff, (uint32_t)tmp_len, data);
         break;
      case _MPF_ARRAY16:
      case _MPF_ARRAY32:
         if (rmsgpack_read_uint(fd, &tmp_len, 2<<(type - _MPF_ARRAY16)) != -1)
            return rmsgpack_read_array(fd, (uint32_t)tmp_len, callbacks, data);
         return -1;
      case _MPF_MAP16:
      case _MPF_MAP32:
         if (rmsgpack_read_uint(fd, &tmp_len, 2<<(type - _MPF_MAP16)) != -1)
            return rmsgpack_read_map(fd, (uint32_t)tmp_len, callbacks, data);
         return -1;
   }

end:
   if (buff)
      free(buff);
   return 0;
}

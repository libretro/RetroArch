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

/* Read path still uses these for type-tag comparisons */
static const uint8_t MPF_FIXMAP   = _MPF_FIXMAP;
static const uint8_t MPF_MAP32    = _MPF_MAP32;
static const uint8_t MPF_FIXARRAY = _MPF_FIXARRAY;
static const uint8_t MPF_FIXSTR   = _MPF_FIXSTR;
static const uint8_t MPF_NIL      = _MPF_NIL;

int rmsgpack_write_array_header(intfstream_t *fd, uint32_t size)
{
   uint8_t buf[5];
   size_t  len;

   if (size < 16)
   {
      buf[0] = (uint8_t)(size | _MPF_FIXARRAY);
      len    = 1;
   }
   else if (size == (uint16_t)size)
   {
      uint16_t tmp = swap_if_little16(size);
      buf[0] = _MPF_ARRAY16;
      memcpy(buf + 1, &tmp, sizeof(uint16_t));
      len    = 1 + sizeof(uint16_t);
   }
   else
   {
      uint32_t tmp = swap_if_little32(size);
      buf[0] = _MPF_ARRAY32;
      memcpy(buf + 1, &tmp, sizeof(uint32_t));
      len    = 1 + sizeof(uint32_t);
   }

   if (intfstream_write(fd, buf, len) == -1)
      return -1;
   return (int)len;
}

int rmsgpack_write_map_header(intfstream_t *fd, uint32_t size)
{
   uint8_t buf[5];
   size_t  len;

   if (size < 16)
   {
      buf[0] = (uint8_t)(size | _MPF_FIXMAP);
      len    = 1;
   }
   else if (size == (uint16_t)size)
   {
      uint16_t tmp = swap_if_little16(size);
      buf[0] = _MPF_MAP16;
      memcpy(buf + 1, &tmp, sizeof(uint16_t));
      len    = 1 + sizeof(uint16_t);
   }
   else
   {
      uint32_t tmp = swap_if_little32(size);
      buf[0] = _MPF_MAP32;
      memcpy(buf + 1, &tmp, sizeof(uint32_t));
      len    = 1 + sizeof(uint32_t);
   }

   if (intfstream_write(fd, buf, len) == -1)
      return -1;
   return (int)len;
}

int rmsgpack_write_string(intfstream_t *fd, const char *s, uint32_t len)
{
   uint8_t hdr[5];
   size_t  hdr_len;

   if (len < 32)
   {
      hdr[0]  = (uint8_t)(len | _MPF_FIXSTR);
      hdr_len = 1;
   }
   else if (len == (uint8_t)len)
   {
      hdr[0]  = _MPF_STR8;
      hdr[1]  = (uint8_t)len;
      hdr_len = 2;
   }
   else if (len == (uint16_t)len)
   {
      uint16_t tmp = swap_if_little16(len);
      hdr[0]  = _MPF_STR16;
      memcpy(hdr + 1, &tmp, sizeof(uint16_t));
      hdr_len = 1 + sizeof(uint16_t);
   }
   else
   {
      uint32_t tmp = swap_if_little32(len);
      hdr[0]  = _MPF_STR32;
      memcpy(hdr + 1, &tmp, sizeof(uint32_t));
      hdr_len = 1 + sizeof(uint32_t);
   }

   /* Short strings (most map keys): single write for header + payload */
   if (hdr_len + len <= 64)
   {
      uint8_t tmp[64];
      memcpy(tmp, hdr, hdr_len);
      memcpy(tmp + hdr_len, s, len);
      if (intfstream_write(fd, tmp, hdr_len + len) == -1)
         return -1;
   }
   else
   {
      /* Longer strings: coalesced header + separate payload */
      if (intfstream_write(fd, hdr, hdr_len) == -1)
         return -1;
      if (intfstream_write(fd, s, len) == -1)
         return -1;
   }

   return (int)(hdr_len + len);
}

int rmsgpack_write_bin(intfstream_t *fd, const void *s, uint32_t len)
{
   uint8_t hdr[5];
   size_t  hdr_len;

   if (len == (uint8_t)len)
   {
      hdr[0]  = _MPF_BIN8;
      hdr[1]  = (uint8_t)len;
      hdr_len = 2;
   }
   else if (len == (uint16_t)len)
   {
      uint16_t tmp = swap_if_little16(len);
      hdr[0]  = _MPF_BIN16;
      memcpy(hdr + 1, &tmp, sizeof(uint16_t));
      hdr_len = 1 + sizeof(uint16_t);
   }
   else
   {
      uint32_t tmp = swap_if_little32(len);
      hdr[0]  = _MPF_BIN32;
      memcpy(hdr + 1, &tmp, sizeof(uint32_t));
      hdr_len = 1 + sizeof(uint32_t);
   }

   /* Short binary (CRC=4, MD5=16, SHA1=20): single write */
   if (hdr_len + len <= 64)
   {
      uint8_t tmp[64];
      memcpy(tmp, hdr, hdr_len);
      memcpy(tmp + hdr_len, s, len);
      if (intfstream_write(fd, tmp, hdr_len + len) == -1)
         return -1;
   }
   else
   {
      if (intfstream_write(fd, hdr, hdr_len) == -1)
         return -1;
      if (intfstream_write(fd, s, len) == -1)
         return -1;
   }

   return (int)(hdr_len + len);
}

int rmsgpack_write_nil(intfstream_t *fd)
{
   static const uint8_t val = _MPF_NIL;
   if (intfstream_write(fd, &val, 1) == -1)
      return -1;
   return 1;
}

int rmsgpack_write_bool(intfstream_t *fd, int value)
{
   uint8_t val = value ? _MPF_TRUE : _MPF_FALSE;
   if (intfstream_write(fd, &val, 1) == -1)
      return -1;
   return 1;
}

int rmsgpack_write_int(intfstream_t *fd, int64_t value)
{
   uint8_t buf[9];
   size_t  len;

   if (value >= 0 && value < 128)
   {
      buf[0] = (uint8_t)value;
      len    = 1;
   }
   else if (value >= -32 && value < 0)
   {
      buf[0] = (uint8_t)(value + 256); /* -32..-1 => 0xE0..0xFF */
      len    = 1;
   }
   else if (value == (int8_t)value)
   {
      int8_t tmp = (int8_t)value;
      buf[0] = _MPF_INT8;
      memcpy(buf + 1, &tmp, sizeof(int8_t));
      len    = 1 + sizeof(int8_t);
   }
   else if (value == (int16_t)value)
   {
      int16_t tmp = swap_if_little16((uint16_t)value);
      buf[0] = _MPF_INT16;
      memcpy(buf + 1, &tmp, sizeof(int16_t));
      len    = 1 + sizeof(int16_t);
   }
   else if (value == (int32_t)value)
   {
      int32_t tmp = swap_if_little32((uint32_t)value);
      buf[0] = _MPF_INT32;
      memcpy(buf + 1, &tmp, sizeof(int32_t));
      len    = 1 + sizeof(int32_t);
   }
   else
   {
      int64_t tmp = swap_if_little64(value);
      buf[0] = _MPF_INT64;
      memcpy(buf + 1, &tmp, sizeof(int64_t));
      len    = 1 + sizeof(int64_t);
   }

   if (intfstream_write(fd, buf, len) == -1)
      return -1;
   return (int)len;
}

int rmsgpack_write_uint(intfstream_t *fd, uint64_t value)
{
   uint8_t buf[9];
   size_t  len;

   if (value == (uint8_t)value)
   {
      buf[0] = _MPF_UINT8;
      buf[1] = (uint8_t)value;
      len    = 2;
   }
   else if (value == (uint16_t)value)
   {
      uint16_t tmp = swap_if_little16((uint16_t)value);
      buf[0] = _MPF_UINT16;
      memcpy(buf + 1, &tmp, sizeof(uint16_t));
      len    = 1 + sizeof(uint16_t);
   }
   else if (value == (uint32_t)value)
   {
      uint32_t tmp = swap_if_little32((uint32_t)value);
      buf[0] = _MPF_UINT32;
      memcpy(buf + 1, &tmp, sizeof(uint32_t));
      len    = 1 + sizeof(uint32_t);
   }
   else
   {
      uint64_t tmp = swap_if_little64(value);
      buf[0] = _MPF_UINT64;
      memcpy(buf + 1, &tmp, sizeof(uint64_t));
      len    = 1 + sizeof(uint64_t);
   }

   if (intfstream_write(fd, buf, len) == -1)
      return -1;
   return (int)len;
}

int rmsgpack_read_uint(intfstream_t *fd, uint64_t *s, size_t len)
{
   union { uint64_t u64; uint32_t u32; uint16_t u16; uint8_t u8; } tmp;

   if (intfstream_read(fd, &tmp, len) == -1)
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

static int rmsgpack_read_int(intfstream_t *fd, int64_t *s, size_t len)
{
   union { uint64_t u64; uint32_t u32; uint16_t u16; uint8_t u8; } tmp;

   if (intfstream_read(fd, &tmp, len) == -1)
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

static int rmsgpack_read_buff(intfstream_t *fd, size_t size, char **pbuff, uint64_t *len)
{
   ssize_t read_len;
   uint64_t tmp_len   = 0;

   if (rmsgpack_read_uint(fd, &tmp_len, size) == -1)
      return -1;

   *pbuff             = (char *)malloc((size_t)(tmp_len + 1) * sizeof(char));

   if ((read_len      = intfstream_read(fd, *pbuff, (size_t)tmp_len)) == -1)
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

static int rmsgpack_read_map(intfstream_t *fd, uint32_t len,
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

static int rmsgpack_read_array(intfstream_t *fd, uint32_t len,
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

int rmsgpack_read(intfstream_t *fd,
      struct rmsgpack_read_callbacks *callbacks, void *data)
{
   int rv;
   uint64_t tmp_len  = 0;
   uint64_t tmp_uint = 0;
   int64_t tmp_int   = 0;
   uint8_t type      = 0;
   char *buff        = NULL;

   if (intfstream_read(fd, &type, sizeof(uint8_t)) == -1)
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
      ssize_t _len = 0;
      tmp_len      = type - MPF_FIXSTR;
      if (!(buff = (char *)malloc((size_t)(tmp_len + 1) * sizeof(char))))
         return -1;
      if ((_len = intfstream_read(fd, buff, (ssize_t)tmp_len)) == -1)
      {
         free(buff);
         return -1;
      }
      buff[_len] = '\0';
      if (callbacks->read_string)
         return callbacks->read_string(buff, (uint32_t)_len, data);
      if (buff)
         free(buff);
      return 0;
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

   if (buff)
      free(buff);
   return 0;
}

/**
 * rmsgpack_skip_bytes:
 *
 * Read and discard @len bytes from the stream. Using read+discard
 * instead of seek preserves the fread buffer — seeking with a
 * FILE* forces a buffer flush and refill, which is catastrophic
 * for performance.
 */
static int rmsgpack_skip_bytes(intfstream_t *fd, uint64_t len)
{
   uint8_t tmp[256];
   while (len > 0)
   {
      uint64_t chunk = len > sizeof(tmp) ? sizeof(tmp) : len;
      if (intfstream_read(fd, tmp, (size_t)chunk) == -1)
         return -1;
      len -= chunk;
   }
   return 0;
}

/**
 * rmsgpack_skip_value:
 *
 * Skip one complete MsgPack value in the stream without allocating
 * any heap memory. For scalar types this reads past the payload.
 * For containers (map, array) it recursively skips all contained
 * values.
 *
 * Returns: 0 on success, -1 on error.
 */
int rmsgpack_skip_value(intfstream_t *fd)
{
   uint8_t  type  = 0;
   uint64_t len   = 0;
   uint64_t i;

   if (intfstream_read(fd, &type, 1) == -1)
      return -1;

   /* positive fixint (0x00..0x7f) — no payload */
   if (type < _MPF_FIXMAP)
      return 0;

   /* fixmap (0x80..0x8f) — skip N*2 values */
   if (type < _MPF_FIXARRAY)
   {
      len = type - _MPF_FIXMAP;
      for (i = 0; i < len * 2; i++)
         if (rmsgpack_skip_value(fd) < 0)
            return -1;
      return 0;
   }

   /* fixarray (0x90..0x9f) — skip N values */
   if (type < _MPF_FIXSTR)
   {
      len = type - _MPF_FIXARRAY;
      for (i = 0; i < len; i++)
         if (rmsgpack_skip_value(fd) < 0)
            return -1;
      return 0;
   }

   /* fixstr (0xa0..0xbf) — read past N bytes */
   if (type < _MPF_NIL)
      return rmsgpack_skip_bytes(fd, type - _MPF_FIXSTR);

   /* negative fixint (0xe0..0xff) — no payload */
   if (type > _MPF_MAP32)
      return 0;

   switch (type)
   {
      case _MPF_NIL:
      case _MPF_FALSE:
      case _MPF_TRUE:
         return 0;

      case _MPF_BIN8:
      case _MPF_STR8:
         if (rmsgpack_read_uint(fd, &len, 1) == -1) return -1;
         return rmsgpack_skip_bytes(fd, len);

      case _MPF_BIN16:
      case _MPF_STR16:
         if (rmsgpack_read_uint(fd, &len, 2) == -1) return -1;
         return rmsgpack_skip_bytes(fd, len);

      case _MPF_BIN32:
      case _MPF_STR32:
         if (rmsgpack_read_uint(fd, &len, 4) == -1) return -1;
         return rmsgpack_skip_bytes(fd, len);

      case _MPF_UINT8:  case _MPF_INT8:
         return rmsgpack_skip_bytes(fd, 1);
      case _MPF_UINT16: case _MPF_INT16:
         return rmsgpack_skip_bytes(fd, 2);
      case _MPF_UINT32: case _MPF_INT32:
         return rmsgpack_skip_bytes(fd, 4);
      case _MPF_UINT64: case _MPF_INT64:
         return rmsgpack_skip_bytes(fd, 8);

      case _MPF_ARRAY16:
         if (rmsgpack_read_uint(fd, &len, 2) == -1) return -1;
         for (i = 0; i < len; i++)
            if (rmsgpack_skip_value(fd) < 0) return -1;
         return 0;
      case _MPF_ARRAY32:
         if (rmsgpack_read_uint(fd, &len, 4) == -1) return -1;
         for (i = 0; i < len; i++)
            if (rmsgpack_skip_value(fd) < 0) return -1;
         return 0;

      case _MPF_MAP16:
         if (rmsgpack_read_uint(fd, &len, 2) == -1) return -1;
         for (i = 0; i < len * 2; i++)
            if (rmsgpack_skip_value(fd) < 0) return -1;
         return 0;
      case _MPF_MAP32:
         if (rmsgpack_read_uint(fd, &len, 4) == -1) return -1;
         for (i = 0; i < len * 2; i++)
            if (rmsgpack_skip_value(fd) < 0) return -1;
         return 0;
   }

   return -1;
}

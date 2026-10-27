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
#include <boolean.h>
#include <streams/file_stream.h>

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

/* Maximum container nesting accepted from a stream.
 *
 * The readers below recurse once per level of map/array nesting and
 * had no limit, while the "len > remaining bytes" guards in
 * rmsgpack_read_map()/rmsgpack_read_array() bound only how *wide* a
 * container may be.  Depth costs one byte per level: a run of 0x91
 * (fixarray of one element) recurses once per byte, so a 200 KB .rdb
 * of 0x91 exhausts the stack:
 *
 *   AddressSanitizer: stack-overflow
 *     rmsgpack_read        rmsgpack.c:530
 *     rmsgpack_read_array  rmsgpack.c:513
 *     ... repeated to exhaustion
 *
 * The DOM reader's own MAX_DEPTH does not catch this - a one-element
 * array pops one entry and pushes one, so its stack index never
 * grows.
 *
 * Records written by the converter nest two deep (a map of scalars);
 * 32 leaves room for any plausible schema change while keeping worst
 * case recursion bounded. */
#define RMSGPACK_MAX_DEPTH 32

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

/**
 * rmsgpack_read_exact:
 *
 * intfstream_read() reports a hard error as -1, but signals
 * end-of-file as a zero-length or short read.  Every length in a
 * MsgPack stream is exact - a type byte is one byte, a declared
 * payload is that many bytes - so a short read is a truncated or
 * malformed stream and never a successful parse.
 *
 * Checking only for -1 (as every read site here used to) makes the
 * reader treat EOF as success: rmsgpack_read() would leave its
 * zero-initialised type byte untouched, take the 'positive fixint'
 * branch and hand the caller a fabricated integer 0, forever.  A
 * .rdb whose nil sentinel is missing - a truncated download is
 * enough - then spins libretrodb_cursor_read_item() without bound
 * and the scanner task never completes.
 *
 * Returns: 0 when @len bytes were read, -1 otherwise.
 */
static int rmsgpack_read_exact(intfstream_t *fd, void *s, size_t len)
{
   if (len == 0)
      return 0;
   if (intfstream_read(fd, s, len) != (int64_t)len)
      return -1;
   return 0;
}

int rmsgpack_read_uint(intfstream_t *fd, uint64_t *s, size_t len)
{
   union { uint64_t u64; uint32_t u32; uint16_t u16; uint8_t u8; } tmp;

   tmp.u64 = 0;
   if (rmsgpack_read_exact(fd, &tmp, len) == -1)
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

/* ---- reading a value ----
 *
 * A value is read from a window the stream lends where it can
 * (intfstream_view), so a field is taken from memory rather than
 * through a read call of its own; what the window does not hold is
 * read. The stream is left where the value ends. */

#define RMSGPACK_VIEW 65536

struct rmsgpack_src
{
   intfstream_t  *fd;
   const uint8_t *p;     /* lent octets not yet taken */
   const uint8_t *end;
   uint64_t       pos;   /* the stream position p stands for */
   uint64_t       size;
   bool           lent;  /* the stream is past pos */
   bool           view;  /* the stream lends at all */
};

static void rmsgpack_src_init(struct rmsgpack_src *s, intfstream_t *fd)
{
   int64_t pos  = intfstream_tell(fd);
   int64_t size = intfstream_get_size(fd);
   s->fd   = fd;
   s->p    = NULL;
   s->end  = NULL;
   s->pos  = pos  > 0 ? (uint64_t)pos  : 0;
   /* a stream that does not know its size bounds nothing */
   s->size = size >= 0 ? (uint64_t)size : ~(uint64_t)0;
   s->lent = false;
   s->view = true;
}

/* Puts the stream back on the position */
static void rmsgpack_src_done(struct rmsgpack_src *s)
{
   if (s->lent)
      intfstream_seek(s->fd, (int64_t)s->pos, RETRO_VFS_SEEK_POSITION_START);
   s->lent = false;
   s->p    = s->end = NULL;
}

/* Whether @len octets at the position are lent */
static bool rmsgpack_src_window(struct rmsgpack_src *s, size_t len)
{
   const uint8_t *ptr = NULL;
   int64_t        n;
   if ((size_t)(s->end - s->p) >= len)
      return true;
   if (!s->view)
      return false;
   if (     s->lent
         && intfstream_seek(s->fd, (int64_t)s->pos,
            RETRO_VFS_SEEK_POSITION_START) < 0)
      return false;
   n       = intfstream_view(s->fd, len > RMSGPACK_VIEW ? len : RMSGPACK_VIEW, &ptr);
   s->lent = n > 0;
   if (n < 0)
      s->view = false;
   if (n <= 0)
   {
      s->p = s->end = NULL;
      return false;
   }
   s->p   = ptr;
   s->end = ptr + n;
   return (size_t)n >= len;
}

/* @len octets at the position into @dst, and past them */
static int rmsgpack_src_read(struct rmsgpack_src *s, void *dst, size_t len)
{
   if (!len)
      return 0;
   if (rmsgpack_src_window(s, len))
   {
      memcpy(dst, s->p, len);
      s->p   += len;
      s->pos += len;
      return 0;
   }
   /* more than a window: read where the position is */
   rmsgpack_src_done(s);
   if (rmsgpack_read_exact(s->fd, dst, len) == -1)
      return -1;
   s->pos += len;
   return 0;
}

/* A big endian number of @len octets (1, 2, 4 or 8) */
static int rmsgpack_src_uint(struct rmsgpack_src *s, uint64_t *v, size_t len)
{
   uint8_t        tmp[8];
   const uint8_t *q = tmp;
   size_t         i;
   if (rmsgpack_src_window(s, len))
   {
      q       = s->p;
      s->p   += len;
      s->pos += len;
   }
   else if (rmsgpack_src_read(s, tmp, len) == -1)
      return -1;
   *v = 0;
   for (i = 0; i < len; i++)
      *v = (*v << 8) | q[i];
   return 0;
}

/* @len octets, the payload of a string or binary, in a fresh
 * allocation ending in a 0 */
/* the octets the stream has left */
static uint64_t rmsgpack_src_left(const struct rmsgpack_src *s)
{
   return s->size > s->pos ? s->size - s->pos : 0;
}

static int rmsgpack_src_buff(struct rmsgpack_src *s, uint64_t len, char **pbuff)
{
   /* a payload claims no more than the stream has left */
   if (len > rmsgpack_src_left(s) || len >= (uint64_t)((size_t)-1))
      return -1;
   if (!(*pbuff = (char*)malloc((size_t)len + 1)))
      return -1;
   if (rmsgpack_src_read(s, *pbuff, (size_t)len) == -1)
   {
      free(*pbuff);
      *pbuff = NULL;
      return -1;
   }
   (*pbuff)[len] = 0;
   return 0;
}

static int rmsgpack_src_value(struct rmsgpack_src *s,
      struct rmsgpack_read_callbacks *callbacks, void *data,
      unsigned depth);

/* A map or array of @len entries (@pairs: a map) */
static int rmsgpack_src_items(struct rmsgpack_src *s, uint64_t len,
      bool pairs, struct rmsgpack_read_callbacks *callbacks, void *data,
      unsigned depth)
{
   int      rv;
   uint64_t i;

   /* No container claims more entries than the stream has octets
    * left for: an entry is one octet at least, a pair two. */
   if (len > rmsgpack_src_left(s) / (pairs ? 2u : 1u))
      return -1;
   if (pairs)
   {
      if (     callbacks->read_map_start
            && (rv = callbacks->read_map_start((uint32_t)len, data)) < 0)
         return rv;
   }
   else if (callbacks->read_array_start
         && (rv = callbacks->read_array_start((uint32_t)len, data)) < 0)
      return rv;

   for (i = 0; i < (pairs ? 2 * len : len); i++)
      if ((rv = rmsgpack_src_value(s, callbacks, data, depth)) < 0)
         return rv;
   return 0;
}

static int rmsgpack_src_value(struct rmsgpack_src *s,
      struct rmsgpack_read_callbacks *callbacks, void *data,
      unsigned depth)
{
   uint64_t tmp_len  = 0;
   uint64_t tmp_uint = 0;
   uint8_t  type;
   char    *buff     = NULL;

   if (depth >= RMSGPACK_MAX_DEPTH)
      return -1;
   depth++;

   if (s->p < s->end)
   {
      type = *s->p++;
      s->pos++;
   }
   else if (rmsgpack_src_read(s, &type, 1) == -1)
      return -1;

   if (type < MPF_FIXMAP)
      return callbacks->read_int ? callbacks->read_int(type, data) : 0;
   else if (type < MPF_FIXARRAY)
      return rmsgpack_src_items(s, type - MPF_FIXMAP, true, callbacks, data, depth);
   else if (type < MPF_FIXSTR)
      return rmsgpack_src_items(s, type - MPF_FIXARRAY, false, callbacks, data, depth);
   else if (type < MPF_NIL)
   {
      if (rmsgpack_src_buff(s, type - MPF_FIXSTR, &buff) == -1)
         return -1;
      if (callbacks->read_string)
         return callbacks->read_string(buff, (uint32_t)(type - MPF_FIXSTR), data);
      free(buff);
      return 0;
   }
   else if (type > MPF_MAP32)
      return callbacks->read_int ? callbacks->read_int(type - 0xff - 1, data) : 0;

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
      case _MPF_STR8:
      case _MPF_STR16:
      case _MPF_STR32:
      {
         bool str = type >= _MPF_STR8;
         if (     rmsgpack_src_uint(s, &tmp_len,
                     (size_t)1 << (type - (str ? _MPF_STR8 : _MPF_BIN8))) == -1
               || rmsgpack_src_buff(s, tmp_len, &buff) == -1)
            return -1;
         if (str && callbacks->read_string)
            return callbacks->read_string(buff, (uint32_t)tmp_len, data);
         if (!str && callbacks->read_bin)
            return callbacks->read_bin(buff, (uint32_t)tmp_len, data);
         break;
      }
      case _MPF_UINT8:
      case _MPF_UINT16:
      case _MPF_UINT32:
      case _MPF_UINT64:
         if (rmsgpack_src_uint(s, &tmp_uint, (size_t)1 << (type - _MPF_UINT8)) == -1)
            return -1;
         if (callbacks->read_uint)
            return callbacks->read_uint(tmp_uint, data);
         break;
      case _MPF_INT8:
      case _MPF_INT16:
      case _MPF_INT32:
      case _MPF_INT64:
      {
         size_t  len = (size_t)1 << (type - _MPF_INT8);
         int64_t v;
         if (rmsgpack_src_uint(s, &tmp_uint, len) == -1)
            return -1;
         /* sign extended from its width */
         if (len < 8 && (tmp_uint >> (8 * len - 1)) & 1)
            tmp_uint |= ~(uint64_t)0 << (8 * len);
         v = (int64_t)tmp_uint;
         if (callbacks->read_int)
            return callbacks->read_int(v, data);
         break;
      }
      case _MPF_ARRAY16:
      case _MPF_ARRAY32:
         if (rmsgpack_src_uint(s, &tmp_len, (size_t)2 << (type - _MPF_ARRAY16)) == -1)
            return -1;
         return rmsgpack_src_items(s, tmp_len, false, callbacks, data, depth);
      case _MPF_MAP16:
      case _MPF_MAP32:
         if (rmsgpack_src_uint(s, &tmp_len, (size_t)2 << (type - _MPF_MAP16)) == -1)
            return -1;
         return rmsgpack_src_items(s, tmp_len, true, callbacks, data, depth);
   }

   free(buff);
   return 0;
}

int rmsgpack_read(intfstream_t *fd,
      struct rmsgpack_read_callbacks *callbacks, void *data)
{
   struct rmsgpack_src s;
   int                 rv;
   rmsgpack_src_init(&s, fd);
   rv = rmsgpack_src_value(&s, callbacks, data, 0);
   rmsgpack_src_done(&s);
   return rv;
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
      if (rmsgpack_read_exact(fd, tmp, (size_t)chunk) == -1)
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
static int rmsgpack_skip_value_depth(intfstream_t *fd, unsigned depth);

int rmsgpack_skip_value(intfstream_t *fd)
{
   return rmsgpack_skip_value_depth(fd, 0);
}

static int rmsgpack_skip_value_depth(intfstream_t *fd, unsigned depth)
{
   uint8_t  type  = 0;
   uint64_t len   = 0;
   uint64_t i;

   if (depth >= RMSGPACK_MAX_DEPTH)
      return -1;
   depth++;

   if (rmsgpack_read_exact(fd, &type, 1) == -1)
      return -1;

   /* positive fixint (0x00..0x7f) — no payload */
   if (type < _MPF_FIXMAP)
      return 0;

   /* fixmap (0x80..0x8f) — skip N*2 values */
   if (type < _MPF_FIXARRAY)
   {
      len = type - _MPF_FIXMAP;
      for (i = 0; i < len * 2; i++)
         if (rmsgpack_skip_value_depth(fd, depth) < 0)
            return -1;
      return 0;
   }

   /* fixarray (0x90..0x9f) — skip N values */
   if (type < _MPF_FIXSTR)
   {
      len = type - _MPF_FIXARRAY;
      for (i = 0; i < len; i++)
         if (rmsgpack_skip_value_depth(fd, depth) < 0)
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
            if (rmsgpack_skip_value_depth(fd, depth) < 0) return -1;
         return 0;
      case _MPF_ARRAY32:
         if (rmsgpack_read_uint(fd, &len, 4) == -1) return -1;
         for (i = 0; i < len; i++)
            if (rmsgpack_skip_value_depth(fd, depth) < 0) return -1;
         return 0;

      case _MPF_MAP16:
         if (rmsgpack_read_uint(fd, &len, 2) == -1) return -1;
         for (i = 0; i < len * 2; i++)
            if (rmsgpack_skip_value_depth(fd, depth) < 0) return -1;
         return 0;
      case _MPF_MAP32:
         if (rmsgpack_read_uint(fd, &len, 4) == -1) return -1;
         for (i = 0; i < len * 2; i++)
            if (rmsgpack_skip_value_depth(fd, depth) < 0) return -1;
         return 0;
   }

   return -1;
}

/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rzip_archive.c).
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

#include <encodings/crc32.h>
#include <encodings/deflate.h>
#include <zip/rzip_archive.h>

#define SIG_LOCAL        0x04034b50u
#define SIG_CENTRAL      0x02014b50u
#define SIG_EOCD         0x06054b50u
#define SIG_EOCD64       0x06064b50u
#define SIG_EOCD64_LOC   0x07064b50u

#define EOCD_LEN         22
#define EOCD_MAX_TAIL    (EOCD_LEN + 0xffff)   /* a comment may follow */
#define CENTRAL_LEN      46
#define LOCAL_LEN        30
#define EXTRA_ZIP64      0x0001

/* Compressed input is pulled in pieces of this size when the archive is
 * read through the callback; it is the inflater's window, not a limit
 * on member size. */
#define READ_CHUNK       (64 * 1024)

struct rzip_archive
{
   const uint8_t *data;
   rzip_read_t    read_cb;
   void          *ud;
   rzip_entry_t  *entries;
   char          *names;
   uint64_t       len;
   uint32_t       num_entries;
};

static uint32_t rd16(const uint8_t *p)
{
   return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

static uint32_t rd32(const uint8_t *p)
{
   return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t rd64(const uint8_t *p)
{
   return (uint64_t)rd32(p) | ((uint64_t)rd32(p + 4) << 32);
}

/* [off, off + n) of the archive: in place from memory, or read into
 * @buf. NULL when the range is outside the archive or the read fails. */
static const uint8_t *rzip_bytes(const rzip_archive_t *a, uint64_t off,
      size_t n, uint8_t *buf)
{
   if (off > a->len || n > a->len - off)
      return NULL;
   if (a->data)
      return a->data + (size_t)off;
   if (a->read_cb(a->ud, off, buf, n) != (int64_t)n)
      return NULL;
   return buf;
}

/* Finds the end-of-central-directory record, honouring the zip64
 * record when the 32-bit one defers to it. */
static int rzip_find_directory(rzip_archive_t *a, uint64_t *cd_off,
      uint64_t *cd_size, uint64_t *cd_count)
{
   uint8_t       *tail_buf = NULL;
   const uint8_t *tail;
   uint64_t       tail_off;
   size_t         tail_len, i;
   int            res = RZIP_ERROR_DATA;

   if (a->len < EOCD_LEN)
      return RZIP_ERROR_DATA;
   tail_len = a->len < EOCD_MAX_TAIL ? (size_t)a->len : EOCD_MAX_TAIL;
   tail_off = a->len - tail_len;

   if (!a->data && !(tail_buf = (uint8_t*)malloc(tail_len)))
      return RZIP_ERROR_MEM;
   if (!(tail = rzip_bytes(a, tail_off, tail_len, tail_buf)))
   {
      free(tail_buf);
      return RZIP_ERROR_IO;
   }

   for (i = tail_len - EOCD_LEN + 1; i-- > 0; )
   {
      const uint8_t *e = tail + i;
      uint64_t       eocd_off;

      if (rd32(e) != SIG_EOCD)
         continue;
      /* the comment must fit between the record and the end */
      if (rd16(e + 20) > tail_len - EOCD_LEN - i)
         continue;

      eocd_off  = tail_off + i;
      *cd_count = rd16(e + 10);
      *cd_size  = rd32(e + 12);
      *cd_off   = rd32(e + 16);

      if ((*cd_count == 0xffff || *cd_size == 0xffffffffu
               || *cd_off == 0xffffffffu) && eocd_off >= 20)
      {
         uint8_t        loc_buf[20];
         uint8_t        rec_buf[56];
         const uint8_t *loc;
         const uint8_t *rec;

         if (!(loc = rzip_bytes(a, eocd_off - 20, 20, loc_buf))
               || rd32(loc) != SIG_EOCD64_LOC)
            break;
         if (!(rec = rzip_bytes(a, rd64(loc + 8), 56, rec_buf))
               || rd32(rec) != SIG_EOCD64)
            break;
         *cd_count = rd64(rec + 32);
         *cd_size  = rd64(rec + 40);
         *cd_off   = rd64(rec + 48);
      }
      res = RZIP_OK;
      break;
   }

   free(tail_buf);
   return res;
}

/* The zip64 extra field carries, in this order, only the fields whose
 * 32-bit slots are saturated. */
static void rzip_parse_zip64(const uint8_t *extra, size_t extra_len,
      uint64_t *size, uint64_t *csize, uint64_t *local)
{
   while (extra_len >= 4)
   {
      size_t flen = rd16(extra + 2);

      if (flen > extra_len - 4)
         return;
      if (rd16(extra) == EXTRA_ZIP64)
      {
         const uint8_t *q   = extra + 4;
         const uint8_t *end = q + flen;

         if (*size == 0xffffffffu && end - q >= 8)
         {
            *size = rd64(q);
            q    += 8;
         }
         if (*csize == 0xffffffffu && end - q >= 8)
         {
            *csize = rd64(q);
            q     += 8;
         }
         if (*local == 0xffffffffu && end - q >= 8)
            *local = rd64(q);
         return;
      }
      extra     += 4 + flen;
      extra_len -= 4 + flen;
   }
}

/* A member's data follows its local header, whose name and extra
 * fields need not be the central directory's. A member whose local
 * header cannot be read keeps data_off at the end of the archive, so
 * no range can be served from it. */
static void rzip_locate(rzip_archive_t *a, rzip_entry_t *e, uint64_t local)
{
   uint8_t        hdr_buf[LOCAL_LEN];
   const uint8_t *hdr = rzip_bytes(a, local, LOCAL_LEN, hdr_buf);

   e->data_off = a->len;
   if (hdr && rd32(hdr) == SIG_LOCAL)
      e->data_off = local + LOCAL_LEN + rd16(hdr + 26) + rd16(hdr + 28);
}

int rzip_archive_open(rzip_archive_t **out, const uint8_t *data,
      uint64_t len, rzip_read_t read_cb, void *ud)
{
   rzip_archive_t *a;
   uint8_t        *cd_buf = NULL;
   const uint8_t  *cd;
   const uint8_t  *p;
   uint64_t        cd_off, cd_size, cd_count;
   size_t          name_bytes = 0;
   uint32_t        n = 0, i;
   char           *name_out;
   int             res;

   if (!out || (!data && !read_cb))
      return RZIP_ERROR_PARAM;
   *out = NULL;

   if (!(a = (rzip_archive_t*)calloc(1, sizeof(*a))))
      return RZIP_ERROR_MEM;
   a->data    = data;
   a->read_cb = read_cb;
   a->ud      = ud;
   a->len     = len;

   if ((res = rzip_find_directory(a, &cd_off, &cd_size, &cd_count)) != RZIP_OK)
      goto fail;

   res = RZIP_ERROR_DATA;
   if (cd_size > (uint64_t)((size_t)-1) - 1 || cd_count > 0x7fffffffu)
      goto fail;
   if (!a->data && !(cd_buf = (uint8_t*)malloc((size_t)cd_size + 1)))
   {
      res = RZIP_ERROR_MEM;
      goto fail;
   }
   if (!(cd = rzip_bytes(a, cd_off, (size_t)cd_size, cd_buf)))
   {
      res = RZIP_ERROR_IO;
      goto fail;
   }

   /* count the records that fit, and size the name pool */
   for (p = cd; (size_t)(p - cd) + CENTRAL_LEN <= (size_t)cd_size
         && n < cd_count; n++)
   {
      size_t vlen;

      if (rd32(p) != SIG_CENTRAL)
         break;
      vlen = rd16(p + 28) + rd16(p + 30) + rd16(p + 32);
      if ((size_t)(p - cd) + CENTRAL_LEN + vlen > (size_t)cd_size)
         break;
      name_bytes += rd16(p + 28) + 1;
      p          += CENTRAL_LEN + vlen;
   }

   res = RZIP_ERROR_MEM;
   if (!(a->entries = (rzip_entry_t*)calloc(n ? n : 1, sizeof(*a->entries))))
      goto fail;
   if (!(a->names = (char*)malloc(name_bytes ? name_bytes : 1)))
      goto fail;
   a->num_entries = n;

   name_out = a->names;
   for (i = 0, p = cd; i < n; i++)
   {
      rzip_entry_t *e         = &a->entries[i];
      size_t        name_len  = rd16(p + 28);
      size_t        extra_len = rd16(p + 30);
      uint64_t      size      = rd32(p + 24);
      uint64_t      csize     = rd32(p + 20);
      uint64_t      local     = rd32(p + 42);

      memcpy(name_out, p + CENTRAL_LEN, name_len);
      name_out[name_len] = '\0';
      e->name            = name_out;
      name_out          += name_len + 1;

      rzip_parse_zip64(p + CENTRAL_LEN + name_len, extra_len,
            &size, &csize, &local);

      e->size   = size;
      e->csize  = csize;
      e->crc    = rd32(p + 16);
      e->method = rd16(p + 10);
      e->is_dir = name_len > 0
         && (e->name[name_len - 1] == '/' || e->name[name_len - 1] == '\\');
      if (e->is_dir)
         e->data_off = a->len;
      else
         rzip_locate(a, e, local);

      p += CENTRAL_LEN + name_len + extra_len + rd16(p + 32);
   }

   free(cd_buf);
   *out = a;
   return RZIP_OK;

fail:
   free(cd_buf);
   rzip_archive_close(a);
   return res;
}

void rzip_archive_close(rzip_archive_t *a)
{
   if (!a)
      return;
   free(a->entries);
   free(a->names);
   free(a);
}

uint32_t rzip_archive_num_entries(const rzip_archive_t *a)
{
   return a ? a->num_entries : 0;
}

const rzip_entry_t *rzip_archive_entry(const rzip_archive_t *a,
      uint32_t index)
{
   if (!a || index >= a->num_entries)
      return NULL;
   return &a->entries[index];
}

int rzip_archive_find(const rzip_archive_t *a, const char *name)
{
   uint32_t i;

   if (!a || !name)
      return -1;
   for (i = 0; i < a->num_entries; i++)
      if (!a->entries[i].is_dir && !strcmp(a->entries[i].name, name))
         return (int)i;
   return -1;
}

/* True when the member's bytes lie inside the archive. */
static int rzip_entry_in_range(const rzip_archive_t *a, const rzip_entry_t *e)
{
   return e->data_off <= a->len && e->csize <= a->len - e->data_off;
}

const uint8_t *rzip_archive_entry_view(const rzip_archive_t *a,
      uint32_t index, size_t *len)
{
   const rzip_entry_t *e;

   if (!a || !a->data || index >= a->num_entries)
      return NULL;
   e = &a->entries[index];
   if (e->is_dir || e->method != RZIP_METHOD_STORED
         || e->size > (uint64_t)((size_t)-1) || !rzip_entry_in_range(a, e))
      return NULL;
   if (len)
      *len = (size_t)e->size;
   return a->data + (size_t)e->data_off;
}

/* Inflates @e into @out, which holds @e->size bytes, from memory or in
 * chunks through the callback. */
static int rzip_inflate(rzip_archive_t *a, const rzip_entry_t *e,
      uint8_t *out)
{
   void    *inf;
   uint8_t *chunk   = NULL;
   uint64_t in_off  = e->data_off;
   uint64_t in_left = e->csize;
   size_t   out_pos = 0;
   int      res     = RZIP_ERROR_DATA;

   if (!(inf = rinflate_new(-15)))
      return RZIP_ERROR_MEM;
   if (!a->data && !(chunk = (uint8_t*)malloc(READ_CHUNK)))
   {
      rinflate_free(inf);
      return RZIP_ERROR_MEM;
   }

   rinflate_set_out(inf, out, (size_t)e->size);
   for (;;)
   {
      size_t take, rd, wr;
      int    st;

      if (a->data)
      {
         take = (size_t)in_left;
         rinflate_set_in(inf, a->data + (size_t)in_off, take);
      }
      else
      {
         take = in_left < READ_CHUNK ? (size_t)in_left : READ_CHUNK;
         if (take && a->read_cb(a->ud, in_off, chunk, take) != (int64_t)take)
         {
            res = RZIP_ERROR_IO;
            break;
         }
         rinflate_set_in(inf, chunk, take);
      }
      in_off  += take;
      in_left -= take;

      st       = rinflate_process(inf, &rd, &wr);
      out_pos += wr;
      if (st == RDEFLATE_PROCESS_ERROR)
         break;
      if (st == RDEFLATE_PROCESS_END)
      {
         res = out_pos == (size_t)e->size ? RZIP_OK : RZIP_ERROR_DATA;
         break;
      }
      /* more input wanted, and there is none */
      if (!in_left)
         break;
   }

   free(chunk);
   rinflate_free(inf);
   return res;
}

int rzip_archive_extract(rzip_archive_t *a, uint32_t index,
      uint8_t **out, size_t *out_len)
{
   const rzip_entry_t *e;
   uint8_t            *buf;
   int                 res;

   if (!a || !out || !out_len || index >= a->num_entries)
      return RZIP_ERROR_PARAM;
   e        = &a->entries[index];
   *out     = NULL;
   *out_len = 0;

   if (e->is_dir)
      return RZIP_ERROR_PARAM;
   if (e->method != RZIP_METHOD_STORED && e->method != RZIP_METHOD_DEFLATE)
      return RZIP_ERROR_UNSUPPORTED;
   if (e->size > (uint64_t)((size_t)-1) || !rzip_entry_in_range(a, e))
      return RZIP_ERROR_DATA;
   if (e->method == RZIP_METHOD_STORED && e->csize != e->size)
      return RZIP_ERROR_DATA;

   if (!(buf = (uint8_t*)malloc(e->size ? (size_t)e->size : 1)))
      return RZIP_ERROR_MEM;

   if (e->method == RZIP_METHOD_STORED)
   {
      if (a->data)
         memcpy(buf, a->data + (size_t)e->data_off, (size_t)e->size);
      else if (e->size && a->read_cb(a->ud, e->data_off, buf,
               (size_t)e->size) != (int64_t)e->size)
      {
         free(buf);
         return RZIP_ERROR_IO;
      }
      res = RZIP_OK;
   }
   else
      res = rzip_inflate(a, e, buf);

   if (res == RZIP_OK
         && encoding_crc32(0, buf, (size_t)e->size) != e->crc)
      res = RZIP_ERROR_CRC;

   if (res != RZIP_OK)
   {
      free(buf);
      return res;
   }
   *out     = buf;
   *out_len = (size_t)e->size;
   return RZIP_OK;
}

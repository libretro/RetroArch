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
#include <retro_atomic.h>
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

int rzip_archive_extract_into(rzip_archive_t *a, uint32_t index,
      uint8_t *dst, size_t dst_size, size_t *out_len)
{
   const rzip_entry_t *e;
   int                 res;

   if (!a || !dst || !out_len || index >= a->num_entries)
      return RZIP_ERROR_PARAM;
   e        = &a->entries[index];
   *out_len = 0;

   if (e->is_dir)
      return RZIP_ERROR_PARAM;
   if (e->method != RZIP_METHOD_STORED && e->method != RZIP_METHOD_DEFLATE)
      return RZIP_ERROR_UNSUPPORTED;
   if (e->size > (uint64_t)((size_t)-1) || !rzip_entry_in_range(a, e))
      return RZIP_ERROR_DATA;
   if (e->method == RZIP_METHOD_STORED && e->csize != e->size)
      return RZIP_ERROR_DATA;
   if (e->size > (uint64_t)dst_size)
      return RZIP_ERROR_PARAM;

   if (e->method == RZIP_METHOD_STORED)
   {
      if (a->data)
         memcpy(dst, a->data + (size_t)e->data_off, (size_t)e->size);
      else if (e->size && a->read_cb(a->ud, e->data_off, dst,
               (size_t)e->size) != (int64_t)e->size)
         return RZIP_ERROR_IO;
      res = RZIP_OK;
   }
   else
      res = rzip_inflate(a, e, dst);

   if (res == RZIP_OK
         && encoding_crc32(0, dst, (size_t)e->size) != e->crc)
      res = RZIP_ERROR_CRC;
   if (res != RZIP_OK)
      return res;
   *out_len = (size_t)e->size;
   return RZIP_OK;
}

int rzip_archive_extract(rzip_archive_t *a, uint32_t index,
      uint8_t **out, size_t *out_len)
{
   const rzip_entry_t *e;
   uint8_t            *buf;
   size_t              size;
   int                 res;

   if (!a || !out || !out_len || index >= a->num_entries)
      return RZIP_ERROR_PARAM;
   e        = &a->entries[index];
   *out     = NULL;
   *out_len = 0;

   if (e->is_dir)
      return RZIP_ERROR_PARAM;
   if (e->size > (uint64_t)((size_t)-1))
      return RZIP_ERROR_DATA;
   size = e->size ? (size_t)e->size : 1;
   if (!(buf = (uint8_t*)malloc(size)))
      return RZIP_ERROR_MEM;
   if ((res = rzip_archive_extract_into(a, index, buf, size, out_len)) != RZIP_OK)
   {
      free(buf);
      return res;
   }
   *out = buf;
   return RZIP_OK;
}

/* ------------------------------------------------------------------ */
/* A deflated member read at any offset: see rzip_archive.h            */
/* ------------------------------------------------------------------ */

#define RZIP_SEEK_WINDOW 32768
#define RZIP_SEEK_WORK   (256 * 1024)
#define RZIP_SEEK_CACHE  8

typedef struct
{
   uint64_t out_off;    /* where in the member decoding can start */
   uint64_t in_bit;     /* and where that is in its compressed bytes */
   uint8_t *window;     /* the member's bytes before it, up to 32 KiB */
   uint32_t window_len;
} rzip_seek_point_t;

typedef struct
{
   uint8_t *data;
   size_t   len;
   size_t   cap;
   uint32_t age;
   int32_t  point;      /* the span from this point to the next; -1 none */
} rzip_seek_span_t;

struct rzip_seek
{
   rzip_archive_t     *a;
   const rzip_entry_t *e;
   rzip_seek_point_t  *points;
   uint32_t            max_points;
   uint32_t            span;
   /* What the builder has published. Points below num_points are never
    * written again; state is 0 while there is more to index, 1 when the
    * member is indexed to its end, negative when the stream is bad. */
   retro_atomic_int_t  num_points;
   retro_atomic_int_t  state;

   /* The builder's. Output goes to b_buf after a 32 KiB prefix that
    * holds the end of what came before, so the 32 KiB before any
    * position are always in one piece. */
   void     *b_inf;
   uint8_t  *b_buf;
   uint8_t  *b_chunk;       /* compressed bytes, when they are read in */
   size_t    b_fill;
   uint64_t  b_out;
   uint64_t  b_in_off;      /* of the compressed bytes, how many are fed */
   uint64_t  b_in_base;     /* and where the current input window starts */
   size_t    b_in_take;
   uint32_t  b_crc;
   int       b_in_set;

   /* The reader's. */
   void            *r_inf;
   uint8_t         *r_chunk;
   uint32_t         r_clock;
   rzip_seek_span_t r_span[RZIP_SEEK_CACHE];
};

rzip_seek_t *rzip_seek_new(rzip_archive_t *a, uint32_t index, uint32_t span)
{
   rzip_seek_t        *s;
   const rzip_entry_t *e;
   unsigned            i;

   if (!a || index >= a->num_entries)
      return NULL;
   e = &a->entries[index];
   if (     e->is_dir || e->method != RZIP_METHOD_DEFLATE
         || !rzip_entry_in_range(a, e))
      return NULL;
   if (!span)
      span = RZIP_SEEK_SPAN_DEFAULT;
   if (span < RZIP_SEEK_WINDOW)
      span = RZIP_SEEK_WINDOW;
   /* a point is at least a span after the one before it */
   if (e->size / span >= 0x7ffffff0u)
      return NULL;
   if (!(s = (rzip_seek_t*)calloc(1, sizeof(*s))))
      return NULL;
   s->a          = a;
   s->e          = e;
   s->span       = span;
   s->max_points = (uint32_t)(e->size / span) + 2;
   s->points     = (rzip_seek_point_t*)calloc(s->max_points, sizeof(*s->points));
   s->b_inf      = rinflate_new(-15);
   s->r_inf      = rinflate_new(-15);
   s->b_buf      = (uint8_t*)malloc(RZIP_SEEK_WINDOW + RZIP_SEEK_WORK);
   if (!a->data)
   {
      s->b_chunk = (uint8_t*)malloc(READ_CHUNK);
      s->r_chunk = (uint8_t*)malloc(READ_CHUNK);
   }
   if (     !s->points || !s->b_inf || !s->r_inf || !s->b_buf
         || (!a->data && (!s->b_chunk || !s->r_chunk)))
   {
      rzip_seek_free(s);
      return NULL;
   }
   for (i = 0; i < RZIP_SEEK_CACHE; i++)
      s->r_span[i].point = -1;
   rinflate_set_stop_at_block(s->b_inf, 1);
   /* the first point is the start of the stream */
   retro_atomic_int_init(&s->num_points, 1);
   retro_atomic_int_init(&s->state, 0);
   return s;
}

void rzip_seek_free(rzip_seek_t *s)
{
   unsigned i;

   if (!s)
      return;
   if (s->points)
   {
      for (i = 0; i < s->max_points; i++)
         free(s->points[i].window);
      free(s->points);
   }
   for (i = 0; i < RZIP_SEEK_CACHE; i++)
      free(s->r_span[i].data);
   if (s->b_inf)
      rinflate_free(s->b_inf);
   if (s->r_inf)
      rinflate_free(s->r_inf);
   free(s->b_buf);
   free(s->b_chunk);
   free(s->r_chunk);
   free(s);
}

uint64_t rzip_seek_size(const rzip_seek_t *s)
{
   return s ? s->e->size : 0;
}

uint64_t rzip_seek_covered(const rzip_seek_t *s)
{
   /* state after the count: a count read first is never ahead of it */
   int n;

   if (!s)
      return 0;
   n = retro_atomic_load_acquire_int((retro_atomic_int_t*)&s->num_points);
   if (retro_atomic_load_acquire_int((retro_atomic_int_t*)&s->state) == 1)
      return s->e->size;
   return s->points[n - 1].out_off;
}

int rzip_seek_state(const rzip_seek_t *s)
{
   if (!s)
      return RZIP_ERROR_PARAM;
   return retro_atomic_load_acquire_int((retro_atomic_int_t*)&s->state);
}

int rzip_seek_build(rzip_seek_t *s, uint64_t upto)
{
   const rzip_entry_t *e;
   int                 n;
   int                 state;

   if (!s)
      return RZIP_ERROR_PARAM;
   if ((state = retro_atomic_load_acquire_int(&s->state)) != 0)
      return state;
   e = s->e;
   n = retro_atomic_load_acquire_int(&s->num_points);

   while (s->points[n - 1].out_off < upto)
   {
      size_t rd = 0;
      size_t wr = 0;
      int    st;

      /* More compressed bytes: all of them at once from a mapping, a
       * chunk at a time through the callback. */
      if (s->a->data)
      {
         if (!s->b_in_set)
         {
            rinflate_set_in(s->b_inf, s->a->data + (size_t)e->data_off,
                  (size_t)e->csize);
            s->b_in_set  = 1;
            s->b_in_take = (size_t)e->csize;
            s->b_in_off  = e->csize;
         }
      }
      else if (!s->b_in_set)
      {
         size_t take = (e->csize - s->b_in_off) < READ_CHUNK
            ? (size_t)(e->csize - s->b_in_off) : READ_CHUNK;

         if (take && s->a->read_cb(s->a->ud, e->data_off + s->b_in_off,
                  s->b_chunk, take) != (int64_t)take)
         {
            retro_atomic_store_release_int(&s->state, RZIP_ERROR_IO);
            return RZIP_ERROR_IO;
         }
         rinflate_set_in(s->b_inf, s->b_chunk, take);
         s->b_in_base  = s->b_in_off;
         s->b_in_off  += take;
         s->b_in_take  = take;
         s->b_in_set   = 1;
      }

      rinflate_set_out(s->b_inf, s->b_buf + RZIP_SEEK_WINDOW + s->b_fill,
            RZIP_SEEK_WORK - s->b_fill);
      st = rinflate_process(s->b_inf, &rd, &wr);
      if (wr)
      {
         s->b_crc   = encoding_crc32(s->b_crc,
               s->b_buf + RZIP_SEEK_WINDOW + s->b_fill, wr);
         s->b_fill += wr;
         s->b_out  += wr;
      }

      if (st == RDEFLATE_PROCESS_ERROR || s->b_out > e->size)
      {
         retro_atomic_store_release_int(&s->state, RZIP_ERROR_DATA);
         return RZIP_ERROR_DATA;
      }
      if (st == RDEFLATE_PROCESS_END)
      {
         state = (s->b_out == e->size && s->b_crc == e->crc)
            ? 1 : (s->b_out == e->size ? RZIP_ERROR_CRC : RZIP_ERROR_DATA);
         retro_atomic_store_release_int(&s->state, state);
         return state;
      }
      if (     st == RDEFLATE_PROCESS_BLOCK
            && s->b_out - s->points[n - 1].out_off >= s->span
            && (uint32_t)n < s->max_points)
      {
         /* A place decoding can start from: here, with what came before */
         rzip_seek_point_t *pt = &s->points[n];
         uint32_t           wl = s->b_out < RZIP_SEEK_WINDOW
            ? (uint32_t)s->b_out : RZIP_SEEK_WINDOW;

         if (!(pt->window = (uint8_t*)malloc(wl)))
         {
            retro_atomic_store_release_int(&s->state, RZIP_ERROR_MEM);
            return RZIP_ERROR_MEM;
         }
         memcpy(pt->window,
               s->b_buf + RZIP_SEEK_WINDOW + s->b_fill - wl, wl);
         pt->window_len = wl;
         pt->out_off    = s->b_out;
         pt->in_bit     = s->b_in_base * 8 + rinflate_tell_bits(s->b_inf);
         n++;
         retro_atomic_store_release_int(&s->num_points, n);
      }
      /* The buffer is full: its last 32 KiB go in front, and it is
       * filled from its start again. */
      if (s->b_fill == RZIP_SEEK_WORK)
      {
         memmove(s->b_buf, s->b_buf + RZIP_SEEK_WORK, RZIP_SEEK_WINDOW);
         s->b_fill = 0;
      }
      /* All of the input window taken: the next chunk, if there is one */
      if (st == RDEFLATE_PROCESS_NEXT && !wr && rd == 0)
      {
         if (s->a->data || s->b_in_off >= e->csize)
         {
            /* it wants more and there is none */
            retro_atomic_store_release_int(&s->state, RZIP_ERROR_DATA);
            return RZIP_ERROR_DATA;
         }
         s->b_in_set = 0;
      }
   }
   return 0;
}

/* The span that starts at point @p, decoded: from the cache, or decoded
 * now in place of the span used longest ago. */
static int rzip_seek_span(rzip_seek_t *s, uint32_t p, uint64_t end,
      const uint8_t **data)
{
   const rzip_seek_point_t *pt   = &s->points[p];
   const rzip_entry_t      *e    = s->e;
   size_t                   len  = (size_t)(end - pt->out_off);
   rzip_seek_span_t        *slot = &s->r_span[0];
   uint64_t                 in_off;
   size_t                   out_pos = 0;
   unsigned                 i;

   for (i = 0; i < RZIP_SEEK_CACHE; i++)
   {
      if (s->r_span[i].point == (int32_t)p)
      {
         s->r_span[i].age = ++s->r_clock;
         *data            = s->r_span[i].data;
         return RZIP_OK;
      }
      if (s->r_span[i].age < slot->age)
         slot = &s->r_span[i];
   }

   slot->point = -1;
   if (len > slot->cap)
   {
      free(slot->data);
      slot->cap  = 0;
      if (!(slot->data = (uint8_t*)malloc(len)))
         return RZIP_ERROR_MEM;
      slot->cap  = len;
   }

   rinflate_reset(s->r_inf, -15);
   if (pt->window_len)
      rinflate_set_dictionary(s->r_inf, pt->window, pt->window_len);
   rinflate_set_start_bit(s->r_inf, (int)(pt->in_bit & 7));
   rinflate_set_out(s->r_inf, slot->data, len);
   in_off = pt->in_bit >> 3;
   if (in_off > e->csize)
      return RZIP_ERROR_DATA;

   while (out_pos < len)
   {
      size_t take, rd = 0, wr = 0;
      int    st;

      if (s->a->data)
      {
         take = (size_t)(e->csize - in_off);
         rinflate_set_in(s->r_inf,
               s->a->data + (size_t)(e->data_off + in_off), take);
      }
      else
      {
         take = (e->csize - in_off) < READ_CHUNK
            ? (size_t)(e->csize - in_off) : READ_CHUNK;
         if (take && s->a->read_cb(s->a->ud, e->data_off + in_off,
                  s->r_chunk, take) != (int64_t)take)
            return RZIP_ERROR_IO;
         rinflate_set_in(s->r_inf, s->r_chunk, take);
      }
      in_off  += take;
      st       = rinflate_process(s->r_inf, &rd, &wr);
      out_pos += wr;
      if (st == RDEFLATE_PROCESS_ERROR)
         return RZIP_ERROR_DATA;
      if (out_pos >= len)
         break;
      /* the end of the stream, or of its bytes, before the span's */
      if (st == RDEFLATE_PROCESS_END || in_off >= e->csize)
         return RZIP_ERROR_DATA;
   }

   slot->len   = len;
   slot->point = (int32_t)p;
   slot->age   = ++s->r_clock;
   *data       = slot->data;
   return RZIP_OK;
}

int rzip_seek_read(rzip_seek_t *s, uint64_t offset, uint8_t *dst, size_t len)
{
   int n;
   int complete;

   if (!s || (!dst && len))
      return RZIP_ERROR_PARAM;
   /* the count, then whether it is the last: see rzip_seek_covered() */
   n        = retro_atomic_load_acquire_int(&s->num_points);
   complete = retro_atomic_load_acquire_int(&s->state) == 1;
   if (complete)
      n     = retro_atomic_load_acquire_int(&s->num_points);
   if (offset > s->e->size || len > s->e->size - offset)
      return RZIP_ERROR_PARAM;

   while (len)
   {
      const uint8_t *span;
      uint64_t       end;
      size_t         take;
      int            res;
      uint32_t       lo = 0;
      uint32_t       hi = (uint32_t)n;

      /* the last point at or before the offset */
      while (hi - lo > 1)
      {
         uint32_t mid = lo + (hi - lo) / 2;
         if (s->points[mid].out_off <= offset)
            lo = mid;
         else
            hi = mid;
      }
      if (lo + 1 < (uint32_t)n)
         end = s->points[lo + 1].out_off;
      else if (complete)
         end = s->e->size;
      else
         return RZIP_ERROR_PARAM;   /* not indexed this far yet */

      if ((res = rzip_seek_span(s, lo, end, &span)) != RZIP_OK)
         return res;
      take = (size_t)(end - offset) < len ? (size_t)(end - offset) : len;
      memcpy(dst, span + (size_t)(offset - s->points[lo].out_off), take);
      dst    += take;
      offset += take;
      len    -= take;
   }
   return RZIP_OK;
}

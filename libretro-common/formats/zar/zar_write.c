/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (zar_write.c).
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

/* The archive zar.c reads, written front to back: the blocks as the
 * files' octets fill them, then the records, the names, the tree with
 * each directory's entries sorted, and the footer, whose hash is
 * SHA-256 over the whole file with the hash itself zeroed. */

#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <lrc_hash.h>
#include <formats/zar.h>
#ifdef HAVE_RZSTD
#include <encodings/rzstd.h>
#endif

#define ZAR_W_BLOCK   65536u
#define ZAR_W_RECORD  40
#define ZAR_W_NODE    16
#define ZAR_W_FOOTER  144
#define ZAR_W_NONE    0xFFFFFFFFu
#define ZAR_W_NAME    0x7FFFu
#define ZAR_W_MAX     ((((uint64_t)1) << 48) - 1)  /* a file's offset or size */

typedef struct zar_wnode
{
   uint64_t offset;   /* a file's, in the stream */
   uint64_t size;
   uint32_t name;     /* in the names, ZAR_W_NONE for the root */
   uint32_t child;    /* a directory's last entry added */
   uint32_t sibling;  /* the entry added before this one */
   uint32_t first;    /* a directory's first entry's node, written */
   uint32_t count;
   uint8_t  file;
} zar_wnode_t;

struct zar_writer
{
   struct sha256_state sha;
   zar_write_fn  write;
   void         *ud;
   uint8_t      *block;
   uint8_t      *enc;
   uint8_t      *names;
   uint8_t      *recs;
   zar_wnode_t  *nodes;
   uint64_t      out;      /* octets written */
   uint64_t      in;       /* octets of the stream */
   size_t        fill;     /* in the block */
   size_t        enc_cap;
   size_t        names_len;
   size_t        names_cap;
   size_t        recs_len;
   size_t        recs_cap;
   uint64_t      blocks;
   uint32_t      nnodes;
   uint32_t      nodes_cap;
   uint32_t      current;  /* the file appended to */
   int           level;
   bool          failed;
};

static void zar_w_be32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v >> 24);
   p[1] = (uint8_t)(v >> 16);
   p[2] = (uint8_t)(v >> 8);
   p[3] = (uint8_t)v;
}

static void zar_w_be64(uint8_t *p, uint64_t v)
{
   zar_w_be32(p, (uint32_t)(v >> 32));
   zar_w_be32(p + 4, (uint32_t)v);
}

static bool zar_w_grow(void **p, size_t *cap, size_t need, size_t size)
{
   size_t n;
   void  *q;
   if (need <= *cap)
      return true;
   n = *cap ? *cap : 64;
   while (n < need)
      n *= 2;
   if (!(q = realloc(*p, n * size)))
      return false;
   *p   = q;
   *cap = n;
   return true;
}

static int zar_w_out(zar_writer_t *w, const void *data, size_t len)
{
   if (w->failed)
      return ZAR_ERROR_WRITE;
   if (len && !w->write(w->ud, data, len))
   {
      w->failed = true;
      return ZAR_ERROR_WRITE;
   }
   sha256_stream_update(&w->sha, (const uint8_t*)data, len);
   w->out += len;
   return ZAR_OK;
}

static uint32_t zar_w_node(zar_writer_t *w, uint32_t parent,
      uint32_t name, bool file)
{
   zar_wnode_t *n;
   size_t       cap = w->nodes_cap;
   if (!zar_w_grow((void**)&w->nodes, &cap, (size_t)w->nnodes + 1,
            sizeof(zar_wnode_t)))
      return ZAR_W_NONE;
   w->nodes_cap = (uint32_t)cap;
   n            = &w->nodes[w->nnodes];
   n->offset    = w->in;
   n->size      = 0;
   n->name      = name;
   n->child     = ZAR_W_NONE;
   n->sibling   = ZAR_W_NONE;
   n->first     = 0;
   n->count     = 0;
   n->file      = file ? 1 : 0;
   if (parent != ZAR_W_NONE)
   {
      n->sibling              = w->nodes[parent].child;
      w->nodes[parent].child  = w->nnodes;
      w->nodes[parent].count++;
   }
   return w->nnodes++;
}

zar_writer_t *zar_writer_new(zar_write_fn write, void *ud, int level)
{
   zar_writer_t *w;
   if (!write || !(w = (zar_writer_t*)calloc(1, sizeof(*w))))
      return NULL;
   w->write   = write;
   w->ud      = ud;
   w->level   = level;
   w->current = ZAR_W_NONE;
   sha256_stream_init(&w->sha, 0);
   if (     !(w->block = (uint8_t*)malloc(ZAR_W_BLOCK))
         || zar_w_node(w, ZAR_W_NONE, ZAR_W_NONE, false) != 0)
   {
      zar_writer_free(w);
      return NULL;
   }
   return w;
}

void zar_writer_free(zar_writer_t *w)
{
   if (!w)
      return;
   free(w->block);
   free(w->enc);
   free(w->names);
   free(w->recs);
   free(w->nodes);
   free(w);
}

static int zar_w_lower(int c)
{
   return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

static const uint8_t *zar_w_name(const zar_writer_t *w, uint32_t at,
      size_t *len)
{
   const uint8_t *p = w->names + at;
   if (p[0] & 0x80)
   {
      *len = (p[0] & 0x7F) | ((size_t)p[1] << 7);
      return p + 2;
   }
   *len = p[0];
   return p + 1;
}

/* the order zar.c's search expects */
static int zar_w_cmp(const uint8_t *a, size_t alen,
      const uint8_t *b, size_t blen)
{
   size_t i;
   for (i = 0; i < alen && i < blen; i++)
   {
      int c = zar_w_lower(a[i]) - zar_w_lower(b[i]);
      if (c)
         return c;
   }
   return (alen > blen) - (alen < blen);
}

static uint32_t zar_w_find(const zar_writer_t *w, uint32_t dir,
      const char *name, size_t len)
{
   uint32_t c;
   for (c = w->nodes[dir].child; c != ZAR_W_NONE; c = w->nodes[c].sibling)
   {
      size_t         nlen;
      const uint8_t *n = zar_w_name(w, w->nodes[c].name, &nlen);
      if (nlen == len && !zar_w_cmp(n, nlen, (const uint8_t*)name, len))
         return c;
   }
   return ZAR_W_NONE;
}

static uint32_t zar_w_add_name(zar_writer_t *w, const char *name,
      size_t len)
{
   uint32_t at = (uint32_t)w->names_len;
   if (     w->names_len + len + 2 > 0x7FFFFFFFu
         || !zar_w_grow((void**)&w->names, &w->names_cap,
            w->names_len + len + 2, 1))
      return ZAR_W_NONE;
   if (len >= 0x80)
   {
      w->names[w->names_len++] = (uint8_t)((len & 0x7F) | 0x80);
      w->names[w->names_len++] = (uint8_t)(len >> 7);
   }
   else
      w->names[w->names_len++] = (uint8_t)len;
   memcpy(w->names + w->names_len, name, len);
   w->names_len += len;
   return at;
}

/* Walks @path from the root, adding the directories it lacks; the last
 * part is added as a file when @file */
static int zar_w_path(zar_writer_t *w, const char *path, bool file,
      uint32_t *out)
{
   uint32_t at = 0;
   for (;;)
   {
      size_t   len;
      uint32_t c;
      bool     last;

      while (*path == '/' || *path == '\\')
         path++;
      for (len = 0; path[len] && path[len] != '/' && path[len] != '\\'; len++) { }
      if (!len)
         break;
      if (len > ZAR_W_NAME)
         return ZAR_ERROR_PARAM;
      last = !path[len];
      if (!last)
      {
         const char *q = path + len;
         while (*q == '/' || *q == '\\')
            q++;
         last = !*q;
      }
      c = zar_w_find(w, at, path, len);
      if (c != ZAR_W_NONE)
      {
         if (w->nodes[c].file || (last && file))
            return ZAR_ERROR_PARAM;
      }
      else
      {
         uint32_t name = zar_w_add_name(w, path, len);
         if (     name == ZAR_W_NONE
               || (c = zar_w_node(w, at, name, last && file)) == ZAR_W_NONE)
            return ZAR_ERROR_MEM;
      }
      at    = c;
      path += len;
   }
   if (file && !at)
      return ZAR_ERROR_PARAM;
   *out = at;
   return ZAR_OK;
}

int zar_writer_dir(zar_writer_t *w, const char *path)
{
   uint32_t at;
   if (!w || !path)
      return ZAR_ERROR_PARAM;
   return zar_w_path(w, path, false, &at);
}

int zar_writer_file(zar_writer_t *w, const char *path)
{
   uint32_t at;
   int      e;
   if (!w || !path)
      return ZAR_ERROR_PARAM;
   w->current = ZAR_W_NONE;
   if ((e = zar_w_path(w, path, true, &at)) == ZAR_OK)
      w->current = at;
   return e;
}

static int zar_w_block(zar_writer_t *w, const uint8_t *src)
{
   const uint8_t *data = src;
   size_t         len  = ZAR_W_BLOCK;
   size_t         r;
   int            e;

#ifdef HAVE_RZSTD
   {
      size_t wrote = 0;
      if (!w->enc)
      {
         w->enc_cap = rzstd_compress_bound(ZAR_W_BLOCK);
         if (!(w->enc = (uint8_t*)malloc(w->enc_cap)))
            return ZAR_ERROR_MEM;
      }
      if (     rzstd_encode(w->enc, w->enc_cap, src, ZAR_W_BLOCK,
                  w->level, &wrote) == RZSTD_PROCESS_END
            && wrote && wrote < ZAR_W_BLOCK)
      {
         data = w->enc;
         len  = wrote;
      }
   }
#endif

   r = (size_t)(w->blocks % 16);
   if (!r)
   {
      if (!zar_w_grow((void**)&w->recs, &w->recs_cap,
               w->recs_len + ZAR_W_RECORD, 1))
         return ZAR_ERROR_MEM;
      memset(w->recs + w->recs_len, 0, ZAR_W_RECORD);
      zar_w_be64(w->recs + w->recs_len, w->out);
      w->recs_len += ZAR_W_RECORD;
   }
   w->recs[w->recs_len - ZAR_W_RECORD + 8 + r * 2]     = (uint8_t)((len - 1) >> 8);
   w->recs[w->recs_len - ZAR_W_RECORD + 8 + r * 2 + 1] = (uint8_t)(len - 1);
   w->blocks++;
   if ((e = zar_w_out(w, data, len)) != ZAR_OK)
      return e;
   return ZAR_OK;
}

static int zar_w_append(zar_writer_t *w, const uint8_t *src, size_t len)
{
   int e;
   w->in += len;
   while (len)
   {
      size_t take;
      /* a whole block straight from the caller */
      if (!w->fill && len >= ZAR_W_BLOCK)
      {
         if ((e = zar_w_block(w, src)) != ZAR_OK)
            return e;
         src += ZAR_W_BLOCK;
         len -= ZAR_W_BLOCK;
         continue;
      }
      take = ZAR_W_BLOCK - w->fill;
      if (take > len)
         take = len;
      memcpy(w->block + w->fill, src, take);
      w->fill += take;
      src     += take;
      len     -= take;
      if (w->fill == ZAR_W_BLOCK)
      {
         w->fill = 0;
         if ((e = zar_w_block(w, w->block)) != ZAR_OK)
            return e;
      }
   }
   return ZAR_OK;
}

int zar_writer_append(zar_writer_t *w, const void *data, size_t len)
{
   if (!w || (!data && len) || w->current == ZAR_W_NONE)
      return ZAR_ERROR_PARAM;
   if (w->failed)
      return ZAR_ERROR_WRITE;
   if (len > ZAR_W_MAX - w->in)
      return ZAR_ERROR_PARAM;
   w->nodes[w->current].size += len;
   return zar_w_append(w, (const uint8_t*)data, len);
}

/* Sorts @ids by name, @tmp as long */
static void zar_w_sort(const zar_writer_t *w, uint32_t *ids,
      uint32_t *tmp, uint32_t n)
{
   uint32_t width;
   for (width = 1; width < n; width *= 2)
   {
      uint32_t lo;
      for (lo = 0; lo < n; lo += 2 * width)
      {
         uint32_t mid = lo + width < n ? lo + width : n;
         uint32_t hi  = lo + 2 * width < n ? lo + 2 * width : n;
         uint32_t i   = lo;
         uint32_t j   = mid;
         uint32_t k   = lo;
         while (i < mid && j < hi)
         {
            size_t         alen, blen;
            const uint8_t *a = zar_w_name(w, w->nodes[ids[i]].name, &alen);
            const uint8_t *b = zar_w_name(w, w->nodes[ids[j]].name, &blen);
            tmp[k++] = zar_w_cmp(b, blen, a, alen) < 0 ? ids[j++] : ids[i++];
         }
         while (i < mid)
            tmp[k++] = ids[i++];
         while (j < hi)
            tmp[k++] = ids[j++];
      }
      memcpy(ids, tmp, n * sizeof(*ids));
   }
}

int zar_writer_finish(zar_writer_t *w)
{
   uint8_t   ft[ZAR_W_FOOTER];
   uint8_t   digest[32];
   uint64_t  sec[6][2];
   uint32_t *order;
   uint32_t *tmp;
   uint32_t  k;
   uint32_t  len;
   int       e = ZAR_OK;

   if (!w)
      return ZAR_ERROR_PARAM;
   w->current = ZAR_W_NONE;
   if (w->fill)
   {
      memset(w->block + w->fill, 0, ZAR_W_BLOCK - w->fill);
      w->fill = 0;
      if ((e = zar_w_block(w, w->block)) != ZAR_OK)
         return e;
   }
   sec[0][0] = 0;
   sec[0][1] = w->out;
   memset(ft, 0, sizeof(ft));
   if ((e = zar_w_out(w, ft, (size_t)((8 - w->out % 8) % 8))) != ZAR_OK)
      return e;

   sec[1][0] = w->out;
   sec[1][1] = w->recs_len;
   if ((e = zar_w_out(w, w->recs, w->recs_len)) != ZAR_OK)
      return e;
   sec[2][0] = w->out;
   sec[2][1] = w->names_len;
   if ((e = zar_w_out(w, w->names, w->names_len)) != ZAR_OK)
      return e;

   /* breadth first: each directory's entries together and sorted */
   order = (uint32_t*)malloc(w->nnodes * sizeof(uint32_t));
   tmp   = (uint32_t*)malloc(w->nnodes * sizeof(uint32_t));
   if (!order || !tmp)
   {
      free(order);
      free(tmp);
      return ZAR_ERROR_MEM;
   }
   order[0] = 0;
   len      = 1;
   for (k = 0; k < len; k++)
   {
      zar_wnode_t *d = &w->nodes[order[k]];
      uint32_t     c;
      uint32_t     at;
      if (d->file)
         continue;
      d->first = len;
      at       = len + d->count;
      for (c = d->child; c != ZAR_W_NONE; c = w->nodes[c].sibling)
         order[--at] = c;
      zar_w_sort(w, order + len, tmp, d->count);
      len += d->count;
   }
   sec[3][0] = w->out;
   sec[3][1] = (uint64_t)w->nnodes * ZAR_W_NODE;
   for (k = 0; k < w->nnodes && e == ZAR_OK; k++)
   {
      uint8_t            n[ZAR_W_NODE];
      const zar_wnode_t *d = &w->nodes[order[k]];
      zar_w_be32(n, (d->name == ZAR_W_NONE ? 0x7FFFFFFFu : d->name)
            | (d->file ? 0x80000000u : 0));
      if (d->file)
      {
         zar_w_be32(n + 4, (uint32_t)d->offset);
         zar_w_be32(n + 8, (uint32_t)d->size);
         zar_w_be32(n + 12, (uint32_t)((d->offset >> 32) & 0xFFFFu)
               | (uint32_t)((d->size >> 16) & 0xFFFF0000u));
      }
      else
      {
         zar_w_be32(n + 4, d->first);
         zar_w_be32(n + 8, d->count);
         zar_w_be32(n + 12, 0);
      }
      e = zar_w_out(w, n, sizeof(n));
   }
   free(order);
   free(tmp);
   if (e != ZAR_OK)
      return e;

   sec[4][0] = sec[5][0] = w->out;
   sec[4][1] = sec[5][1] = 0;
   for (k = 0; k < 6; k++)
   {
      zar_w_be64(ft + k * 16, sec[k][0]);
      zar_w_be64(ft + k * 16 + 8, sec[k][1]);
   }
   zar_w_be64(ft + 128, w->out + ZAR_W_FOOTER);
   zar_w_be32(ft + 136, 0x61BF3A01u);
   zar_w_be32(ft + 140, 0x169F52D6u);
   sha256_stream_update(&w->sha, ft, sizeof(ft));
   sha256_stream_final(&w->sha, digest);
   memcpy(ft + 96, digest, sizeof(digest));
   if (w->failed || !w->write(w->ud, ft, sizeof(ft)))
   {
      w->failed = true;
      return ZAR_ERROR_WRITE;
   }
   w->out += sizeof(ft);
   return ZAR_OK;
}

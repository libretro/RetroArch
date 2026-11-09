/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (zar.c).
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

/* No file I/O: every range of the file is asked of the caller (see
 * zar.h).
 *
 * The file ends in a footer: six sections' places and sizes - the
 * compressed blocks, their records, the names, the tree, and two of
 * metadata - each as two big-endian 64-bit words, then a hash, the
 * file's size, a version and a magic word. A record places sixteen
 * blocks: their run's start in the block section, then each one's
 * stored size less one; 64 KiB stored is a block kept as it is. A tree
 * node is 16 octets: its name's place in the names (the top bit set
 * for a file), then a file's place and size in the stream (48 bits
 * each, their top 16 bits in the last word) or a directory's first
 * child and count. A name is its length, in one octet or two, and its
 * octets. A writer may sort a directory's entries, but Cemu's reader
 * does not count on it: it takes the first entry whose name matches,
 * A-Z folded to a-z, and archives made by other tools hold directories
 * out of order and names more than once. So does this reader. */

#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <formats/zar.h>
#ifdef HAVE_RZSTD
#include <encodings/rzstd.h>
#endif

#define ZAR_FOOTER       144
#define ZAR_MAGIC        0x169F52D6u
#define ZAR_VERSION      0x61BF3A01u
#define ZAR_BLOCK        65536u
#define ZAR_RECORD       40
#define ZAR_NODE         16
#define ZAR_NO_NAME      0x7FFFFFFFu
#define ZAR_NAME_MAX     255
#define ZAR_BATCH        32       /* directory entries asked for at once */
#define ZAR_NONE         (~(uint64_t)0)

#define ZAR_SEC_BLOCKS  0
#define ZAR_SEC_RECORDS 1
#define ZAR_SEC_NAMES   2
#define ZAR_SEC_TREE    3

#define ZAR_OP_OPEN     0
#define ZAR_OP_NONE     1
#define ZAR_OP_LOOKUP   2
#define ZAR_OP_READ     3
#define ZAR_OP_DIR      4

/* the lookup */
#define ZAR_L_ROOT      0   /* the root's node */
#define ZAR_L_HAVE      1   /* a node in hand, the path's next part to find */
#define ZAR_L_SCAN      2   /* the directory's next run of entries */
#define ZAR_L_NAME      3   /* a name of the run */
/* the listing */
#define ZAR_L_BATCH     4   /* a run of its entries */
#define ZAR_L_LIST      5   /* a listed entry's name, in full */

/* where the range asked for goes */
#define ZAR_TO_BUF      0
#define ZAR_TO_NAME     1
#define ZAR_TO_RECORD   2
#define ZAR_TO_PACKED   3
#define ZAR_TO_READ     4

struct zar
{
   uint64_t      file_size;
   uint64_t      sec_off[4];
   uint64_t      sec_size[4];
   uint64_t      blocks;      /* the stream's, from the records */
   uint64_t      rec_group;   /* the record held, ZAR_NONE for none */
   uint64_t      blk_index;   /* the decoded block held, ZAR_NONE for none */
   uint64_t      rd_off;      /* in the stream */
   uint8_t      *rd_dst;
   uint8_t      *packed;      /* a compressed block, as stored */
   uint8_t      *block;       /* a decoded block, for partial reads */
#ifdef HAVE_RZSTD
   rzstd_decoder_t *decoder;
#endif
   const char   *path;
   size_t        rd_len;
   size_t        rd_done;
   size_t        got;
   size_t        cap;
   uint32_t      nodes;
   uint32_t      dir_first;
   uint32_t      dir_count;
   uint32_t      dir_index;   /* the first entry of the run in hand */
   uint32_t      batch;       /* entries in the run */
   uint32_t      entry;       /* the one whose name is looked at */
   uint32_t      packed_size;
   int           op;
   int           stage;
   int           to;
   uint8_t       buf[ZAR_BATCH * ZAR_NODE];
   uint8_t       node[ZAR_NODE];  /* the lookup's, in hand */
   uint8_t       name[ZAR_NAME_MAX + 3]; /* length, octets, terminator */
   uint8_t       rec[ZAR_RECORD];
   bool          fetching;
   bool          failed;
   bool          listing;     /* a listing's directory is found */
};

static uint32_t zar_be32(const uint8_t *p)
{
   return ((uint32_t)p[0] << 24)
        | ((uint32_t)p[1] << 16)
        | ((uint32_t)p[2] << 8)
        |  (uint32_t)p[3];
}

static uint64_t zar_be64(const uint8_t *p)
{
   return ((uint64_t)zar_be32(p) << 32) | zar_be32(p + 4);
}

zar_t *zar_new(uint64_t file_size)
{
   unsigned i;
   zar_t   *z = (zar_t*)malloc(sizeof(zar_t));
   if (!z)
      return NULL;
   z->file_size   = file_size;
   for (i = 0; i < 4; i++)
   {
      z->sec_off[i]  = 0;
      z->sec_size[i] = 0;
   }
   z->blocks      = 0;
   z->rec_group   = ZAR_NONE;
   z->blk_index   = ZAR_NONE;
   z->rd_off      = 0;
   z->rd_dst      = NULL;
   z->packed      = NULL;
   z->block       = NULL;
#ifdef HAVE_RZSTD
   z->decoder     = NULL;
#endif
   z->path        = NULL;
   z->rd_len      = 0;
   z->rd_done     = 0;
   z->got         = 0;
   z->cap         = 0;
   z->nodes       = 0;
   z->dir_first   = 0;
   z->dir_count   = 0;
   z->dir_index   = 0;
   z->batch       = 0;
   z->entry       = 0;
   z->packed_size = 0;
   z->op          = ZAR_OP_OPEN;
   z->stage       = 0;
   z->to          = ZAR_TO_BUF;
   z->fetching    = false;
   z->failed      = false;
   z->listing     = false;
   return z;
}

void zar_free(zar_t *z)
{
   if (!z)
      return;
   free(z->packed);
   free(z->block);
#ifdef HAVE_RZSTD
   rzstd_decoder_free(z->decoder);
#endif
   free(z);
}

/* Asks for @cap octets at @off into @dst, of which @got have come */
static int zar_ask(zar_t *z, zar_request_t *req, int to, uint64_t off,
      uint8_t *dst, size_t cap)
{
   z->to        = to;
   z->cap       = cap;
   req->offset  = off + z->got;
   req->length  = cap - z->got;
   req->dst     = dst + z->got;
   z->fetching  = true;
   return ZAR_PENDING;
}

int zar_feed(zar_t *z, size_t len)
{
   if (!z || !z->fetching)
      return ZAR_ERROR_STATE;
   z->fetching = false;
   if (!len)
   {
      z->failed = true;
      return ZAR_ERROR_DATA;
   }
   if (len > z->cap - z->got)
      len = z->cap - z->got;
   z->got += len;
   return ZAR_OK;
}

static int zar_open_run(zar_t *z, zar_request_t *req)
{
   const uint8_t *f = z->buf;
   unsigned       i;

   if (z->file_size <= ZAR_FOOTER)
      return ZAR_ERROR_DATA;
   if (z->got < ZAR_FOOTER)
      return zar_ask(z, req, ZAR_TO_BUF, z->file_size - ZAR_FOOTER,
            z->buf, ZAR_FOOTER);
   z->got = 0;
   if (     zar_be32(f + 140) != ZAR_MAGIC
         || zar_be32(f + 136) != ZAR_VERSION
         || zar_be64(f + 128) != z->file_size)
      return ZAR_ERROR_DATA;
   for (i = 0; i < 6; i++)
   {
      uint64_t off  = zar_be64(f + i * 16);
      uint64_t size = zar_be64(f + i * 16 + 8);
      if (off > z->file_size || size > z->file_size - off)
         return ZAR_ERROR_DATA;
      if (i < 4)
      {
         z->sec_off[i]  = off;
         z->sec_size[i] = size;
      }
   }
   if (     z->sec_size[ZAR_SEC_RECORDS] > 0xFFFFFFFFu
         || z->sec_size[ZAR_SEC_RECORDS] % ZAR_RECORD
         || z->sec_size[ZAR_SEC_NAMES] > 0x7FFFFFFFu
         || !z->sec_size[ZAR_SEC_TREE]
         || z->sec_size[ZAR_SEC_TREE] > 0xFFFFFFFFu
         || z->sec_size[ZAR_SEC_TREE] % ZAR_NODE)
      return ZAR_ERROR_DATA;
   z->nodes  = (uint32_t)(z->sec_size[ZAR_SEC_TREE] / ZAR_NODE);
   z->blocks = z->sec_size[ZAR_SEC_RECORDS] / ZAR_RECORD * 16;
   return ZAR_OK;
}

int zar_open_step(zar_t *z, zar_request_t *req)
{
   int e;
   if (!z || !req)
      return ZAR_ERROR_PARAM;
   if (z->op != ZAR_OP_OPEN)
      return z->nodes ? ZAR_OK : ZAR_ERROR_STATE;
   e = z->failed ? ZAR_ERROR_DATA : zar_open_run(z, req);
   if (e == ZAR_OK)
      z->op = ZAR_OP_NONE;
   else if (e < 0)
      z->failed = true;
   return e;
}

int zar_lookup_begin(zar_t *z, const char *path)
{
   if (!z || !path)
      return ZAR_ERROR_PARAM;
   if (z->op == ZAR_OP_OPEN)
      return ZAR_ERROR_STATE;
   z->op        = ZAR_OP_LOOKUP;
   z->stage     = ZAR_L_ROOT;
   z->path      = path;
   z->got       = 0;
   z->fetching  = false;
   z->failed    = false;
   z->listing   = false;
   return ZAR_OK;
}

int zar_dir_begin(zar_t *z, const char *path)
{
   int e = zar_lookup_begin(z, path);
   if (e == ZAR_OK)
      z->op = ZAR_OP_DIR;
   return e;
}

static int zar_lower(int c)
{
   return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

/* A tree node as the caller sees it */
static int zar_node_file(const zar_t *z, const uint8_t *n, zar_file_t *out)
{
   if (n[0] & 0x80)
   {
      out->offset = zar_be32(n + 4)
            | ((uint64_t)(zar_be32(n + 12) & 0xFFFFu) << 32);
      out->size   = zar_be32(n + 8)
            | ((uint64_t)(zar_be32(n + 12) & 0xFFFF0000u) << 16);
      out->dir    = 0;
      if (     out->offset > z->blocks * ZAR_BLOCK
            || out->size > z->blocks * ZAR_BLOCK - out->offset)
         return ZAR_ERROR_DATA;
   }
   else
   {
      out->offset = zar_be32(n + 4);
      out->size   = zar_be32(n + 8);
      out->dir    = 1;
      if (     out->offset > z->nodes
            || out->size > z->nodes - out->offset)
         return ZAR_ERROR_DATA;
   }
   return ZAR_OK;
}

/* Asks for the name @n points at, at most @want octets of it past its
 * length; *@hdr and *@nlen say where it starts and how long it is */
static int zar_name(zar_t *z, zar_request_t *req, const uint8_t *n,
      size_t want, size_t *cap, size_t *hdr, size_t *nlen)
{
   uint32_t name_at = zar_be32(n) & 0x7FFFFFFFu;
   if (     name_at == ZAR_NO_NAME
         || name_at >= z->sec_size[ZAR_SEC_NAMES])
      return ZAR_ERROR_DATA;
   *cap = 2 + want;
   if (*cap > z->sec_size[ZAR_SEC_NAMES] - name_at)
      *cap = (size_t)(z->sec_size[ZAR_SEC_NAMES] - name_at);
   if (z->got < *cap)
      return zar_ask(z, req, ZAR_TO_NAME,
            z->sec_off[ZAR_SEC_NAMES] + name_at, z->name, *cap);
   z->got = 0;
   *nlen  = z->name[0] & 0x7F;
   *hdr   = 1;
   if (z->name[0] & 0x80)
   {
      if (*cap < 2)
         return ZAR_ERROR_DATA;
      *nlen |= (size_t)z->name[1] << 7;
      *hdr   = 2;
   }
   return ZAR_OK;
}

/* Down the tree from the root, each directory's entries in their
 * order, a run at a time, for the first named the path's next part */
static int zar_lookup_run(zar_t *z, zar_request_t *req, zar_file_t *out)
{
   for (;;)
   {
      const uint8_t *n;
      const char    *p;
      size_t         plen;
      size_t         cap;
      size_t         hdr;
      size_t         nlen;
      size_t         i;
      bool           same;
      int            e;

      while (*z->path == '/' || *z->path == '\\')
         z->path++;
      p = z->path;
      for (plen = 0; p[plen] && p[plen] != '/' && p[plen] != '\\'; plen++) { }

      switch (z->stage)
      {
         case ZAR_L_ROOT:
            if (z->got < ZAR_NODE)
               return zar_ask(z, req, ZAR_TO_BUF,
                     z->sec_off[ZAR_SEC_TREE], z->node, ZAR_NODE);
            z->got   = 0;
            z->stage = ZAR_L_HAVE;
            break;
         case ZAR_L_HAVE:
            if ((e = zar_node_file(z, z->node, out)) != ZAR_OK)
               return e;
            if (!plen)
               return ZAR_OK;
            if (!out->dir || plen > ZAR_NAME_MAX)
               return ZAR_ERROR_NOT_FOUND;
            z->dir_first = (uint32_t)out->offset;
            z->dir_count = (uint32_t)out->size;
            z->dir_index = 0;
            z->stage     = ZAR_L_SCAN;
            break;
         case ZAR_L_SCAN:
            if (z->dir_index >= z->dir_count)
               return ZAR_ERROR_NOT_FOUND;
            z->batch = z->dir_count - z->dir_index;
            if (z->batch > ZAR_BATCH)
               z->batch = ZAR_BATCH;
            if (z->got < (size_t)z->batch * ZAR_NODE)
               return zar_ask(z, req, ZAR_TO_BUF,
                     z->sec_off[ZAR_SEC_TREE]
                     + ((uint64_t)z->dir_first + z->dir_index) * ZAR_NODE,
                     z->buf, (size_t)z->batch * ZAR_NODE);
            z->got   = 0;
            z->entry = 0;
            z->stage = ZAR_L_NAME;
            break;
         case ZAR_L_NAME:
            if (z->entry >= z->batch)
            {
               z->dir_index += z->batch;
               z->stage      = ZAR_L_SCAN;
               break;
            }
            n = z->buf + z->entry * ZAR_NODE;
            if ((e = zar_name(z, req, n, plen, &cap, &hdr, &nlen)) != ZAR_OK)
               return e;
            if (hdr + (nlen < plen ? nlen : plen) > cap)
               return ZAR_ERROR_DATA;
            same = nlen == plen;
            for (i = 0; i < plen && same; i++)
               same = zar_lower((unsigned char)p[i])
                     == zar_lower(z->name[hdr + i]);
            if (!same)
            {
               z->entry++;
               break;
            }
            memcpy(z->node, n, ZAR_NODE);
            z->path += plen;
            z->stage = ZAR_L_HAVE;
            break;
         default:
            return ZAR_ERROR_STATE;
      }
   }
}

int zar_lookup_step(zar_t *z, zar_request_t *req, zar_file_t *out)
{
   int e;
   if (!z || !req || !out)
      return ZAR_ERROR_PARAM;
   if (z->op != ZAR_OP_LOOKUP)
      return ZAR_ERROR_STATE;
   e = z->failed ? ZAR_ERROR_DATA : zar_lookup_run(z, req, out);
   if (e != ZAR_PENDING)
      z->op = ZAR_OP_NONE;
   return e;
}

/* The directory's entries a run at a time, each one's name read in
 * full */
static int zar_list_run(zar_t *z, zar_request_t *req, zar_dirent_t *ent)
{
   for (;;)
   {
      const uint8_t *n;
      size_t         cap;
      size_t         hdr;
      size_t         nlen;
      int            e;

      switch (z->stage)
      {
         case ZAR_L_BATCH:
            if (z->dir_index >= z->dir_count)
               return ZAR_END;
            z->batch = z->dir_count - z->dir_index;
            if (z->batch > ZAR_BATCH)
               z->batch = ZAR_BATCH;
            if (z->got < (size_t)z->batch * ZAR_NODE)
               return zar_ask(z, req, ZAR_TO_BUF,
                     z->sec_off[ZAR_SEC_TREE]
                     + ((uint64_t)z->dir_first + z->dir_index) * ZAR_NODE,
                     z->buf, (size_t)z->batch * ZAR_NODE);
            z->got   = 0;
            z->entry = 0;
            z->stage = ZAR_L_LIST;
            break;
         case ZAR_L_LIST:
            if (z->entry >= z->batch)
            {
               z->dir_index += z->batch;
               z->stage      = ZAR_L_BATCH;
               break;
            }
            n = z->buf + z->entry * ZAR_NODE;
            if ((e = zar_name(z, req, n, ZAR_NAME_MAX,
                        &cap, &hdr, &nlen)) != ZAR_OK)
               return e;
            if (nlen > ZAR_NAME_MAX || hdr + nlen > cap)
               return ZAR_ERROR_DATA;
            if ((e = zar_node_file(z, n, &ent->file)) != ZAR_OK)
               return e;
            z->name[hdr + nlen] = '\0';
            ent->name   = (const char*)z->name + hdr;
            ent->len    = nlen;
            ent->is_dir = (int)ent->file.dir;
            z->entry++;
            return ZAR_OK;
         default:
            return ZAR_ERROR_STATE;
      }
   }
}

int zar_dir_step(zar_t *z, zar_request_t *req, zar_dirent_t *ent)
{
   int e;
   if (!z || !req || !ent)
      return ZAR_ERROR_PARAM;
   if (z->op != ZAR_OP_DIR)
      return ZAR_ERROR_STATE;
   if (z->failed)
      e = ZAR_ERROR_DATA;
   else if (z->listing)
      e = zar_list_run(z, req, ent);
   else
   {
      zar_file_t d;
      if ((e = zar_lookup_run(z, req, &d)) == ZAR_OK)
      {
         if (!d.dir)
            e = ZAR_ERROR_NOT_FOUND;
         else
         {
            z->dir_first = (uint32_t)d.offset;
            z->dir_count = (uint32_t)d.size;
            z->dir_index = 0;
            z->batch     = 0;
            z->got       = 0;
            z->stage     = ZAR_L_BATCH;
            z->listing   = true;
            e            = zar_list_run(z, req, ent);
         }
      }
   }
   if (e != ZAR_PENDING && e != ZAR_OK)
      z->op = ZAR_OP_NONE;
   return e;
}

uint64_t zar_index_offset(const zar_t *z)
{
   uint64_t off;
   if (!z || !z->nodes)
      return 0;
   off = z->sec_off[ZAR_SEC_RECORDS];
   if (z->sec_off[ZAR_SEC_NAMES] < off)
      off = z->sec_off[ZAR_SEC_NAMES];
   if (z->sec_off[ZAR_SEC_TREE] < off)
      off = z->sec_off[ZAR_SEC_TREE];
   return off;
}

int zar_read_begin(zar_t *z, const zar_file_t *file, uint64_t off,
      void *dst, size_t len)
{
   if (!z || !file || file->dir || (!dst && len))
      return ZAR_ERROR_PARAM;
   if (z->op == ZAR_OP_OPEN)
      return ZAR_ERROR_STATE;
   if (off >= file->size)
      len = 0;
   else if (len > file->size - off)
      len = (size_t)(file->size - off);
   z->op       = ZAR_OP_READ;
   z->rd_off   = file->offset + off;
   z->rd_dst   = (uint8_t*)dst;
   z->rd_len   = len;
   z->rd_done  = 0;
   z->got      = 0;
   z->fetching = false;
   z->failed   = false;
   return ZAR_OK;
}

static int zar_read_run(zar_t *z, zar_request_t *req)
{
   while (z->rd_done < z->rd_len)
   {
      uint64_t pos    = z->rd_off + z->rd_done;
      uint64_t index  = pos / ZAR_BLOCK;
      size_t   within = (size_t)(pos % ZAR_BLOCK);
      size_t   step   = ZAR_BLOCK - within;
      uint64_t base;
      uint32_t prefix = 0;
      uint32_t size;
      unsigned sub;
      unsigned i;

      if (step > z->rd_len - z->rd_done)
         step = z->rd_len - z->rd_done;
      if (index >= z->blocks)
         return ZAR_ERROR_DATA;

      if (index == z->blk_index)
      {
         /* a block decoded for an earlier read */
         memcpy(z->rd_dst + z->rd_done, z->block + within, step);
         z->rd_done += step;
         continue;
      }

      /* the record that places this block */
      if (z->rec_group != index / 16)
      {
         if (z->got < ZAR_RECORD)
            return zar_ask(z, req, ZAR_TO_RECORD,
                  z->sec_off[ZAR_SEC_RECORDS] + (index / 16) * ZAR_RECORD,
                  z->rec, ZAR_RECORD);
         z->got       = 0;
         z->rec_group = index / 16;
      }
      base = zar_be64(z->rec);
      sub  = (unsigned)(index % 16);
      for (i = 0; i < sub; i++)
         prefix += ((uint32_t)z->rec[8 + i * 2] << 8) + z->rec[9 + i * 2] + 1;
      size = ((uint32_t)z->rec[8 + sub * 2] << 8) + z->rec[9 + sub * 2] + 1;
      if (     base > z->sec_size[ZAR_SEC_BLOCKS]
            || prefix + size > z->sec_size[ZAR_SEC_BLOCKS] - base)
         return ZAR_ERROR_DATA;
      base += z->sec_off[ZAR_SEC_BLOCKS] + prefix;

      if (size == ZAR_BLOCK)
      {
         /* kept as it is: straight to where the read goes */
         z->to        = ZAR_TO_READ;
         z->cap       = step;
         z->got       = 0;
         req->offset  = base + within;
         req->length  = step;
         req->dst     = z->rd_dst + z->rd_done;
         z->fetching  = true;
         return ZAR_PENDING;
      }

#ifdef HAVE_RZSTD
      {
         size_t   wrote = 0;
         uint8_t *into;

         if (!z->packed && !(z->packed = (uint8_t*)malloc(ZAR_BLOCK)))
            return ZAR_ERROR_MEM;
         if (z->got < size)
            return zar_ask(z, req, ZAR_TO_PACKED, base, z->packed, size);
         z->got = 0;
         if (!z->decoder && !(z->decoder = rzstd_decoder_new()))
            return ZAR_ERROR_MEM;
         /* the whole block wanted: decoded where the read goes */
         if (!within && step == ZAR_BLOCK)
            into = z->rd_dst + z->rd_done;
         else
         {
            if (!z->block && !(z->block = (uint8_t*)malloc(ZAR_BLOCK)))
               return ZAR_ERROR_MEM;
            z->blk_index = ZAR_NONE;
            into         = z->block;
         }
         if (     rzstd_decode_reuse(z->decoder, into, ZAR_BLOCK,
                     z->packed, size, &wrote) != RZSTD_PROCESS_END
               || wrote != ZAR_BLOCK)
            return ZAR_ERROR_DATA;
         if (into == z->block)
         {
            z->blk_index = index;
            memcpy(z->rd_dst + z->rd_done, z->block + within, step);
         }
         z->rd_done += step;
      }
#else
      return ZAR_ERROR_UNSUPPORTED;
#endif
   }
   return ZAR_OK;
}

int zar_read_step(zar_t *z, zar_request_t *req)
{
   int e;
   if (!z || !req)
      return ZAR_ERROR_PARAM;
   if (z->op != ZAR_OP_READ)
      return ZAR_ERROR_STATE;
   if (z->failed)
      e = ZAR_ERROR_DATA;
   else
   {
      /* a block read straight to the destination counts as it comes */
      if (z->to == ZAR_TO_READ && z->got)
      {
         z->rd_done += z->got;
         z->got      = 0;
         z->to       = ZAR_TO_BUF;
      }
      e = zar_read_run(z, req);
   }
   if (e != ZAR_PENDING)
      z->op = ZAR_OP_NONE;
   return e;
}

size_t zar_read_progress(const zar_t *z)
{
   return z ? z->rd_done : 0;
}

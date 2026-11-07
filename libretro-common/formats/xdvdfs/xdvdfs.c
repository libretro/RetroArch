/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (xdvdfs.c).
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

/* No file I/O: every range of the image is asked of the caller (see
 * xdvdfs.h).
 *
 * The volume descriptor is sector 32 of the game partition:
 * "MICROSOFT*XBOX*MEDIA", then the root directory's sector and size.
 * A directory is a binary tree of entries, each on four-octet
 * boundaries and never across a sector: the left and right subtrees'
 * offsets in four-octet units (0 for none), the file's sector and
 * size, its attributes and its name. The tree is ordered by name,
 * compared without regard to ASCII case. */

#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <formats/xdvdfs.h>

#define XDVDFS_SECTOR      2048
#define XDVDFS_VOLUME      (32 * XDVDFS_SECTOR)
#define XDVDFS_MAGIC       "MICROSOFT*XBOX*MEDIA"
#define XDVDFS_MAGIC_LEN   20
#define XDVDFS_VOLUME_HEAD 0x1C   /* magic, root sector, root size */
#define XDVDFS_ENTRY_HEAD  14
#define XDVDFS_NAME_MAX    255
#define XDVDFS_ATTR_DIR    0x10
#define XDVDFS_XBE_HEAD    0x11C  /* through the certificate address */

/* Where whole disc images keep the game partition, after a trimmed
 * XISO's start: XGD1, XGD3, XGD2 */
static const uint32_t xdvdfs_parts[] = {
   0, 0x18300000, 0x2080000, 0xFD90000
};
#define XDVDFS_N_PARTS (sizeof(xdvdfs_parts) / sizeof(xdvdfs_parts[0]))

#define XDVDFS_OP_NONE   0
#define XDVDFS_OP_OPEN   1
#define XDVDFS_OP_LOOKUP 2
#define XDVDFS_OP_TITLE  3

/* the title read, after its lookup */
#define XDVDFS_TITLE_LOOKUP 0
#define XDVDFS_TITLE_HEAD   1
#define XDVDFS_TITLE_ID     2

struct xdvdfs
{
   uint64_t      image_size;
   uint64_t      part;       /* the game partition, in the image */
   uint64_t      dir;        /* the directory searched, in the image */
   uint64_t      root;
   xdvdfs_extent_t file;     /* what the lookup found */
   const char   *path;       /* the lookup's next component */
   size_t        got;        /* octets of the range in hand */
   size_t        cap;        /* octets of it asked for */
   uint32_t      dir_size;
   uint32_t      root_size;
   uint32_t      node;       /* the entry looked at, in four-octet units */
   uint32_t      visits;     /* entries looked at in this directory */
   uint32_t      xbe_cert;   /* the certificate, in the XBE */
   int           op;
   int           stage;      /* the title read's, or the partition probed */
   uint8_t       buf[XDVDFS_XBE_HEAD]; /* an entry, or the XBE's head */
   bool          fetching;
   bool          failed;
   bool          open;
};

xdvdfs_t *xdvdfs_new(uint64_t image_size)
{
   xdvdfs_t *x = (xdvdfs_t*)malloc(sizeof(xdvdfs_t));
   if (!x)
      return NULL;
   x->image_size  = image_size;
   x->part        = 0;
   x->dir         = 0;
   x->root        = 0;
   x->file.offset = 0;
   x->file.size   = 0;
   x->path        = NULL;
   x->got         = 0;
   x->cap         = 0;
   x->dir_size    = 0;
   x->root_size   = 0;
   x->node        = 0;
   x->visits      = 0;
   x->xbe_cert    = 0;
   x->op          = XDVDFS_OP_OPEN;
   x->stage       = 0;
   x->fetching    = false;
   x->failed      = false;
   x->open        = false;
   return x;
}

void xdvdfs_free(xdvdfs_t *x)
{
   free(x);
}

static uint32_t xdvdfs_le32(const uint8_t *p)
{
   return (uint32_t)p[0]
        | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16)
        | ((uint32_t)p[3] << 24);
}

/* Asks for @cap octets at @off into the buffer, keeping what of them
 * has come */
static int xdvdfs_ask(xdvdfs_t *x, xdvdfs_request_t *req, uint64_t off,
      size_t cap)
{
   x->cap       = cap;
   req->offset  = off + x->got;
   req->length  = cap - x->got;
   req->dst     = x->buf + x->got;
   x->fetching  = true;
   return XDVDFS_PENDING;
}

int xdvdfs_feed(xdvdfs_t *x, size_t len)
{
   if (!x || !x->fetching)
      return XDVDFS_ERROR_STATE;
   x->fetching = false;
   if (!len)
   {
      x->failed = true;
      return XDVDFS_ERROR_DATA;
   }
   if (len > x->cap - x->got)
      len = x->cap - x->got;
   x->got += len;
   return XDVDFS_OK;
}

static int xdvdfs_open_run(xdvdfs_t *x, xdvdfs_request_t *req)
{
   while (x->stage < (int)XDVDFS_N_PARTS)
   {
      uint64_t base = xdvdfs_parts[x->stage];
      uint64_t at   = base + XDVDFS_VOLUME;

      if (     at > x->image_size
            || x->image_size - at < XDVDFS_VOLUME_HEAD)
      {
         x->stage++;
         continue;
      }
      if (x->got < XDVDFS_VOLUME_HEAD)
         return xdvdfs_ask(x, req, at, XDVDFS_VOLUME_HEAD);
      x->got = 0;
      if (memcmp(x->buf, XDVDFS_MAGIC, XDVDFS_MAGIC_LEN))
      {
         x->stage++;
         continue;
      }
      x->part      = base;
      x->root      = base + (uint64_t)xdvdfs_le32(x->buf + 0x14)
                   * XDVDFS_SECTOR;
      x->root_size = xdvdfs_le32(x->buf + 0x18);
      x->open      = true;
      return XDVDFS_OK;
   }
   return XDVDFS_ERROR_NOT_FOUND;
}

int xdvdfs_open_step(xdvdfs_t *x, xdvdfs_request_t *req)
{
   int e;
   if (!x || !req)
      return XDVDFS_ERROR_PARAM;
   if (x->open)
      return XDVDFS_OK;
   if (x->op != XDVDFS_OP_OPEN)
      return XDVDFS_ERROR_STATE;
   if (x->failed)
      e = XDVDFS_ERROR_DATA;
   else
      e = xdvdfs_open_run(x, req);
   if (e != XDVDFS_PENDING)
      x->op = e == XDVDFS_OK ? XDVDFS_OP_NONE : XDVDFS_OP_OPEN;
   if (e < 0)
      x->failed = true;
   return e;
}

uint64_t xdvdfs_partition(const xdvdfs_t *x)
{
   return x ? x->part : 0;
}

static void xdvdfs_arm(xdvdfs_t *x, int op, const char *path)
{
   x->op       = op;
   x->stage    = 0;
   x->path     = path;
   x->dir      = x->root;
   x->dir_size = x->root_size;
   x->node     = 0;
   x->visits   = 0;
   x->got      = 0;
   x->fetching = false;
   x->failed   = false;
}

int xdvdfs_lookup_begin(xdvdfs_t *x, const char *path)
{
   if (!x || !path)
      return XDVDFS_ERROR_PARAM;
   if (!x->open)
      return XDVDFS_ERROR_STATE;
   while (*path == '/' || *path == '\\')
      path++;
   xdvdfs_arm(x, XDVDFS_OP_LOOKUP, path);
   return XDVDFS_OK;
}

static int xdvdfs_upper(int c)
{
   return (c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c;
}

/* The lookup, an entry at a time */
static int xdvdfs_lookup_run(xdvdfs_t *x, xdvdfs_request_t *req)
{
   for (;;)
   {
      uint64_t       at  = (uint64_t)x->node * 4;
      const char    *p   = x->path;
      size_t         plen;
      size_t         cap;
      size_t         nlen;
      size_t         i;
      int            cmp = 0;
      const uint8_t *e   = x->buf;

      for (plen = 0; p[plen] && p[plen] != '/' && p[plen] != '\\'; plen++) { }
      if (!plen || plen > XDVDFS_NAME_MAX || !x->dir_size)
         return XDVDFS_ERROR_NOT_FOUND;
      if (at + XDVDFS_ENTRY_HEAD > x->dir_size)
         return XDVDFS_ERROR_DATA;

      /* the entry and its name, read at once: it ends inside its
       * sector and its directory */
      cap = XDVDFS_SECTOR - (size_t)(at % XDVDFS_SECTOR);
      if (cap > x->dir_size - at)
         cap = (size_t)(x->dir_size - at);
      if (cap > XDVDFS_ENTRY_HEAD + plen)
         cap = XDVDFS_ENTRY_HEAD + plen;
      if (x->got < XDVDFS_ENTRY_HEAD)
         return xdvdfs_ask(x, req, x->dir + at, cap);
      nlen = e[13];
      if (     !nlen
            || XDVDFS_ENTRY_HEAD + (nlen < plen ? nlen : plen) > cap)
         return XDVDFS_ERROR_DATA;

      /* only as much of the name as can still decide the order */
      for (i = 0; i < nlen && i < plen; i++)
      {
         if (x->got < XDVDFS_ENTRY_HEAD + i + 1)
            return xdvdfs_ask(x, req, x->dir + at, cap);
         cmp = xdvdfs_upper((unsigned char)p[i])
             - xdvdfs_upper(e[XDVDFS_ENTRY_HEAD + i]);
         if (cmp)
            break;
      }
      if (!cmp)
         cmp = (plen > nlen) - (plen < nlen);
      x->got = 0;

      if (cmp)
      {
         uint32_t next = cmp < 0
               ? (uint32_t)e[0] | ((uint32_t)e[1] << 8)
               : (uint32_t)e[2] | ((uint32_t)e[3] << 8);
         if (!next)
            return XDVDFS_ERROR_NOT_FOUND;
         /* a tree that loops runs out of entries to look at */
         if (++x->visits > x->dir_size / XDVDFS_ENTRY_HEAD)
            return XDVDFS_ERROR_DATA;
         x->node = next;
         continue;
      }

      p += plen;
      while (*p == '/' || *p == '\\')
         p++;
      x->file.offset = x->part + (uint64_t)xdvdfs_le32(e + 4) * XDVDFS_SECTOR;
      x->file.size   = xdvdfs_le32(e + 8);
      if (!*p)
         return XDVDFS_OK;
      if (!(e[12] & XDVDFS_ATTR_DIR))
         return XDVDFS_ERROR_NOT_FOUND;
      x->path     = p;
      x->dir      = x->file.offset;
      x->dir_size = x->file.size;
      x->node     = 0;
      x->visits   = 0;
   }
}

int xdvdfs_lookup_step(xdvdfs_t *x, xdvdfs_request_t *req,
      xdvdfs_extent_t *out)
{
   int e;
   if (!x || !req || !out)
      return XDVDFS_ERROR_PARAM;
   if (x->op != XDVDFS_OP_LOOKUP)
      return XDVDFS_ERROR_STATE;
   e = x->failed ? XDVDFS_ERROR_DATA : xdvdfs_lookup_run(x, req);
   if (e == XDVDFS_PENDING)
      return e;
   x->op = XDVDFS_OP_NONE;
   if (e == XDVDFS_OK)
      *out = x->file;
   return e;
}

int xdvdfs_title_begin(xdvdfs_t *x)
{
   if (!x)
      return XDVDFS_ERROR_PARAM;
   if (!x->open)
      return XDVDFS_ERROR_STATE;
   xdvdfs_arm(x, XDVDFS_OP_TITLE, "default.xbe");
   return XDVDFS_OK;
}

static int xdvdfs_title_run(xdvdfs_t *x, xdvdfs_request_t *req,
      uint32_t *title_id)
{
   uint32_t base;
   int      e;

   switch (x->stage)
   {
      case XDVDFS_TITLE_LOOKUP:
         if ((e = xdvdfs_lookup_run(x, req)) != XDVDFS_OK)
            return e;
         if (x->file.size < XDVDFS_XBE_HEAD)
            return XDVDFS_ERROR_DATA;
         x->stage = XDVDFS_TITLE_HEAD;
         /* fall through */
      case XDVDFS_TITLE_HEAD:
         if (x->got < XDVDFS_XBE_HEAD)
            return xdvdfs_ask(x, req, x->file.offset, XDVDFS_XBE_HEAD);
         x->got = 0;
         if (memcmp(x->buf, "XBEH", 4))
            return XDVDFS_ERROR_DATA;
         base        = xdvdfs_le32(x->buf + 0x104);
         x->xbe_cert = xdvdfs_le32(x->buf + 0x118) - base;
         if (     xdvdfs_le32(x->buf + 0x118) < base
               || x->xbe_cert > x->file.size
               || x->file.size - x->xbe_cert < 12)
            return XDVDFS_ERROR_DATA;
         x->stage = XDVDFS_TITLE_ID;
         /* fall through */
      case XDVDFS_TITLE_ID:
         if (x->got < 4)
            return xdvdfs_ask(x, req,
                  x->file.offset + x->xbe_cert + 8, 4);
         x->got    = 0;
         *title_id = xdvdfs_le32(x->buf);
         return XDVDFS_OK;
   }
   return XDVDFS_ERROR_STATE;
}

int xdvdfs_title_step(xdvdfs_t *x, xdvdfs_request_t *req,
      uint32_t *title_id)
{
   int e;
   if (!x || !req || !title_id)
      return XDVDFS_ERROR_PARAM;
   if (x->op != XDVDFS_OP_TITLE)
      return XDVDFS_ERROR_STATE;
   e = x->failed ? XDVDFS_ERROR_DATA : xdvdfs_title_run(x, req, title_id);
   if (e != XDVDFS_PENDING)
      x->op = XDVDFS_OP_NONE;
   return e;
}

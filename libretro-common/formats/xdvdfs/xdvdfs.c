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
 * compared without regard to ASCII case. The rest of a sector no entry
 * fits in, and the one sector of an empty directory, is 0xFF.
 *
 * A title is read from the head of an XBE (little endian), the
 * optional headers of an XEX or the header of an STFS package (big
 * endian), each only as far as the title needs. */

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
#define XDVDFS_ROOT_MAX    (32 * 1024 * 1024)
#define XDVDFS_BUF         (XDVDFS_ENTRY_HEAD + XDVDFS_NAME_MAX + 3)
#define XDVDFS_EXE_HEAD    24
#define XDVDFS_XBE_FIELDS  0x104  /* base address .. certificate address */
#define XDVDFS_XEX_INFO_KEY 0x00040006u
#define XDVDFS_XEX_INFO    24
#define XDVDFS_STFS_FIELDS 0x360  /* title, platform, type, disc, discs */

/* Where whole disc images keep the game partition, after a trimmed
 * XISO's start: XGD1, XGD3, XGD2, and two seen in development images */
static const uint32_t xdvdfs_parts[] = {
   0, 0x18300000, 0x2080000, 0xFD90000, 0xFB20, 0x20600
};
#define XDVDFS_N_PARTS (sizeof(xdvdfs_parts) / sizeof(xdvdfs_parts[0]))

#define XDVDFS_OP_NONE   0
#define XDVDFS_OP_OPEN   1
#define XDVDFS_OP_LOOKUP 2
#define XDVDFS_OP_TITLE  3

/* the title read */
#define XDVDFS_TITLE_LOOKUP    0
#define XDVDFS_TITLE_HEAD      1
#define XDVDFS_TITLE_XBE       2
#define XDVDFS_TITLE_XBE_ID    3
#define XDVDFS_TITLE_XEX_TABLE 4
#define XDVDFS_TITLE_XEX_INFO  5
#define XDVDFS_TITLE_STFS      6

struct xdvdfs
{
   uint64_t      image_size;
   uint64_t      part;       /* the game partition, in the image */
   uint64_t      dir;        /* the directory searched, in the image */
   uint64_t      root;
   xdvdfs_extent_t file;     /* what the lookup found, or the executable */
   const char   *path;       /* the lookup's next component */
   size_t        got;        /* octets of the range in hand */
   size_t        cap;        /* octets of it asked for */
   uint32_t      dir_size;
   uint32_t      root_size;
   uint32_t      node;       /* the entry looked at, in four-octet units */
   uint32_t      visits;     /* entries looked at in this directory */
   uint32_t      exe_at;     /* the next range the title read needs, in it */
   uint32_t      xex_head;   /* the XEX's header size */
   uint32_t      xex_count;  /* its optional headers */
   uint32_t      xex_index;  /* the next of them to look at */
   int           op;
   int           stage;      /* the title read's, or the partition probed */
   uint8_t       buf[XDVDFS_BUF]; /* an entry, or a run of an executable */
   bool          disc;       /* the title read looks its executable up */
   bool          xbe;        /* looking up default.xbe, after default.xex */
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
   x->exe_at      = 0;
   x->xex_head    = 0;
   x->xex_count   = 0;
   x->xex_index   = 0;
   x->disc        = false;
   x->xbe         = false;
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

static uint32_t xdvdfs_be32(const uint8_t *p)
{
   return ((uint32_t)p[0] << 24)
        | ((uint32_t)p[1] << 16)
        | ((uint32_t)p[2] << 8)
        |  (uint32_t)p[3];
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
      x->root_size = xdvdfs_le32(x->buf + 0x18);
      if (     x->root_size < XDVDFS_ENTRY_HEAD
            || x->root_size > XDVDFS_ROOT_MAX)
      {
         x->stage++;
         continue;
      }
      x->part      = base;
      x->root      = base + (uint64_t)xdvdfs_le32(x->buf + 0x14)
                   * XDVDFS_SECTOR;
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
   x->disc     = false;
   x->xbe      = false;
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
      /* padding: an empty directory */
      if (e[0] == 0xFF && e[1] == 0xFF)
         return XDVDFS_ERROR_NOT_FOUND;
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

int xdvdfs_title_begin(xdvdfs_t *x, const xdvdfs_extent_t *exe)
{
   if (!x)
      return XDVDFS_ERROR_PARAM;
   if (exe)
   {
      xdvdfs_arm(x, XDVDFS_OP_TITLE, NULL);
      x->file  = *exe;
      x->stage = XDVDFS_TITLE_HEAD;
      return XDVDFS_OK;
   }
   if (!x->open)
      return XDVDFS_ERROR_STATE;
   xdvdfs_arm(x, XDVDFS_OP_TITLE, "default.xex");
   x->disc = true;
   return XDVDFS_OK;
}

/* Asks for @len octets at @at in the executable, which it must hold */
static int xdvdfs_exe_ask(xdvdfs_t *x, xdvdfs_request_t *req, uint32_t at,
      size_t len)
{
   if (at > x->file.size || x->file.size - at < len)
      return XDVDFS_ERROR_DATA;
   return xdvdfs_ask(x, req, x->file.offset + at, len);
}

static int xdvdfs_title_run(xdvdfs_t *x, xdvdfs_request_t *req,
      xdvdfs_title_t *title)
{
   uint32_t v;
   uint32_t i;
   size_t   n;
   int      e;

   for (;;)
   {
      switch (x->stage)
      {
         case XDVDFS_TITLE_LOOKUP:
            if ((e = xdvdfs_lookup_run(x, req)) == XDVDFS_ERROR_NOT_FOUND
                  && !x->xbe)
            {
               /* an Xbox 360 disc boots default.xex, an Xbox one
                * default.xbe */
               x->xbe      = true;
               x->path     = "default.xbe";
               x->dir      = x->root;
               x->dir_size = x->root_size;
               x->node     = 0;
               x->visits   = 0;
               x->got      = 0;
               continue;
            }
            if (e != XDVDFS_OK)
               return e;
            x->stage = XDVDFS_TITLE_HEAD;
            break;
         case XDVDFS_TITLE_HEAD:
            if (x->got < XDVDFS_EXE_HEAD)
               return xdvdfs_exe_ask(x, req, 0, XDVDFS_EXE_HEAD);
            x->got = 0;
            if (!memcmp(x->buf, "XBEH", 4))
               x->stage = XDVDFS_TITLE_XBE;
            else if (!memcmp(x->buf, "XEX2", 4) || !memcmp(x->buf, "XEX1", 4))
            {
               x->xex_head  = xdvdfs_be32(x->buf + 8);
               x->xex_count = xdvdfs_be32(x->buf + 20);
               x->xex_index = 0;
               if (     x->xex_head < XDVDFS_EXE_HEAD
                     || x->xex_head > x->file.size
                     || x->xex_count > (x->xex_head - XDVDFS_EXE_HEAD) / 8)
                  return XDVDFS_ERROR_DATA;
               x->stage = XDVDFS_TITLE_XEX_TABLE;
            }
            else if (   !memcmp(x->buf, "CON ", 4)
                     || !memcmp(x->buf, "LIVE", 4)
                     || !memcmp(x->buf, "PIRS", 4))
               x->stage = XDVDFS_TITLE_STFS;
            else
               return XDVDFS_ERROR_NOT_FOUND;
            break;
         case XDVDFS_TITLE_XBE:
            if (x->got < 24)
               return xdvdfs_exe_ask(x, req, XDVDFS_XBE_FIELDS, 24);
            x->got = 0;
            v      = xdvdfs_le32(x->buf);           /* base address */
            i      = xdvdfs_le32(x->buf + 0x14);    /* certificate */
            if (i < v || i - v > x->file.size || x->file.size - (i - v) < 12)
               return XDVDFS_ERROR_DATA;
            x->exe_at = i - v + 8;
            x->stage  = XDVDFS_TITLE_XBE_ID;
            /* fall through */
         case XDVDFS_TITLE_XBE_ID:
            if (x->got < 4)
               return xdvdfs_exe_ask(x, req, x->exe_at, 4);
            x->got          = 0;
            title->id       = xdvdfs_le32(x->buf);
            title->platform = XDVDFS_XBOX;
            title->disc     = 0;
            title->discs    = 0;
            return XDVDFS_OK;
         case XDVDFS_TITLE_XEX_TABLE:
            /* the optional headers, as many at a time as the buffer
             * holds, until the execution info's */
            if (x->xex_index >= x->xex_count)
               return XDVDFS_ERROR_NOT_FOUND;
            n = x->xex_count - x->xex_index;
            if (n > XDVDFS_BUF / 8)
               n = XDVDFS_BUF / 8;
            if (x->got < n * 8)
               return xdvdfs_exe_ask(x, req,
                     XDVDFS_EXE_HEAD + x->xex_index * 8, n * 8);
            x->got = 0;
            for (i = 0; i < n; i++)
               if (xdvdfs_be32(x->buf + i * 8) == XDVDFS_XEX_INFO_KEY)
                  break;
            if (i == n)
            {
               x->xex_index += (uint32_t)n;
               break;
            }
            x->exe_at = xdvdfs_be32(x->buf + i * 8 + 4);
            if (     x->exe_at > x->xex_head
                  || x->xex_head - x->exe_at < XDVDFS_XEX_INFO)
               return XDVDFS_ERROR_DATA;
            x->stage = XDVDFS_TITLE_XEX_INFO;
            /* fall through */
         case XDVDFS_TITLE_XEX_INFO:
            if (x->got < XDVDFS_XEX_INFO)
               return xdvdfs_exe_ask(x, req, x->exe_at, XDVDFS_XEX_INFO);
            x->got          = 0;
            title->id       = xdvdfs_be32(x->buf + 12);
            title->platform = XDVDFS_XBOX360;
            title->disc     = x->buf[18];
            title->discs    = x->buf[19];
            return XDVDFS_OK;
         case XDVDFS_TITLE_STFS:
            if (x->got < 8)
               return xdvdfs_exe_ask(x, req, XDVDFS_STFS_FIELDS, 8);
            x->got          = 0;
            title->id       = xdvdfs_be32(x->buf);
            title->platform = XDVDFS_XBOX360;
            title->disc     = x->buf[6];
            title->discs    = x->buf[7];
            return XDVDFS_OK;
         default:
            return XDVDFS_ERROR_STATE;
      }
   }
}

int xdvdfs_title_step(xdvdfs_t *x, xdvdfs_request_t *req,
      xdvdfs_title_t *title)
{
   int e;
   if (!x || !req || !title)
      return XDVDFS_ERROR_PARAM;
   if (x->op != XDVDFS_OP_TITLE)
      return XDVDFS_ERROR_STATE;
   e = x->failed ? XDVDFS_ERROR_DATA : xdvdfs_title_run(x, req, title);
   if (e != XDVDFS_PENDING)
      x->op = XDVDFS_OP_NONE;
   return e;
}

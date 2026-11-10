/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rgif.c).
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

#include <retro_endianness.h>
#include <formats/image.h>
#include <formats/rgif.h>

#define RG_CODES 4096 /* 12-bit LZW codes */

struct rgif
{
   /* the LZW table: each code is its prefix code and one last index */
   int16_t  prefix[RG_CODES];
   uint8_t  first[RG_CODES];   /* the first index of the code's string */
   uint8_t  suffix[RG_CODES];
   uint8_t  stack[RG_CODES];   /* a string, last index first */

   uint8_t  pal[256][4];       /* the global table: R, G, B, alpha */
   uint8_t  lpal[256][4];      /* the image's own */
   uint8_t  px[256][4];        /* each index as written: in @order, or
                                  0 where it is transparent */

   /* the image_transfer interface's file */
   const uint8_t *buf;
   size_t         len;
   size_t         avail;
};

/* The file, read as stb_image reads it: past the end, every byte is 0 */
struct rg_in
{
   const uint8_t *p;
   size_t         at, len;
};

static unsigned rg_get8(struct rg_in *in)
{
   return in->at < in->len ? in->p[in->at++] : 0;
}

static unsigned rg_get16(struct rg_in *in)
{
   unsigned lo = rg_get8(in);
   return lo | (rg_get8(in) << 8);
}

static void rg_skip(struct rg_in *in, size_t n)
{
   in->at = n < in->len - in->at ? in->at + n : in->len;
}

static void rg_put(uint8_t *d, unsigned order, unsigned r, unsigned g,
      unsigned b, unsigned a)
{
   switch (order)
   {
      case RGIF_ARGB:
         d[0] = (uint8_t)a; d[1] = (uint8_t)r; d[2] = (uint8_t)g; d[3] = (uint8_t)b;
         break;
      case RGIF_BGRA:
         d[0] = (uint8_t)b; d[1] = (uint8_t)g; d[2] = (uint8_t)r; d[3] = (uint8_t)a;
         break;
      case RGIF_ABGR:
         d[0] = (uint8_t)a; d[1] = (uint8_t)b; d[2] = (uint8_t)g; d[3] = (uint8_t)r;
         break;
      default:
         d[0] = (uint8_t)r; d[1] = (uint8_t)g; d[2] = (uint8_t)b; d[3] = (uint8_t)a;
         break;
   }
}

static void rg_colours(struct rg_in *in, uint8_t pal[256][4], unsigned n,
      int transparent)
{
   unsigned i;
   for (i = 0; i < n; i++)
   {
      pal[i][0] = (uint8_t)rg_get8(in);
      pal[i][1] = (uint8_t)rg_get8(in);
      pal[i][2] = (uint8_t)rg_get8(in);
      pal[i][3] = (int)i == transparent ? 0 : 255;
   }
}

static int rg_header(struct rg_in *in, unsigned *w, unsigned *h)
{
   unsigned v;
   if (rg_get8(in) != 'G' || rg_get8(in) != 'I' || rg_get8(in) != 'F'
         || rg_get8(in) != '8')
      return 0;
   v = rg_get8(in);
   if ((v != '7' && v != '9') || rg_get8(in) != 'a')
      return 0;
   *w = rg_get16(in);
   *h = rg_get16(in);
   return 1;
}

bool rgif_size(const uint8_t *data, size_t len,
      unsigned *width, unsigned *height)
{
   struct rg_in in;
   in.p = data; in.at = 0; in.len = data ? len : 0;
   return rg_header(&in, width, height) != 0;
}

rgif_t *rgif_new(void)
{
   return (rgif_t*)calloc(1, sizeof(rgif_t));
}

void rgif_free(rgif_t *g)
{
   free(g);
}

/* Where the image's pixels go, in the order its LZW stream gives them */
struct rg_raster
{
   uint8_t *out;
   size_t   stride;
   unsigned x0, x1;       /* the image's columns, x0 to x1 - 1 */
   unsigned y0, y1;       /* and rows */
   unsigned x, y;         /* the next pixel */
   unsigned step;         /* rows from one to the next in this pass */
   unsigned parse;        /* interlace passes still to come */
   uint8_t *row;          /* out + y * stride */
};

/* A code's string, out to the image; the rows of an interlaced image in
 * GIF's four passes */
static void rg_emit(rgif_t *g, struct rg_raster *r, unsigned code)
{
   unsigned n = 0;

   do
   {
      g->stack[n++] = g->suffix[code];
      code          = (unsigned)g->prefix[code];
   } while (code < RG_CODES);

   while (n)
   {
      if (r->y >= r->y1)
         return; /* past the image: the rest of the stream is dropped */
      memcpy(r->row + (size_t)r->x * 4, g->px[g->stack[--n]], 4);
      if (++r->x >= r->x1)
      {
         r->x  = r->x0;
         r->y += r->step;
         while (r->y >= r->y1 && r->parse)
         {
            r->step = 1u << r->parse; /* 8, 4, 2: from row 4, 2, 1 */
            r->y    = r->y0 + r->step / 2;
            r->parse--;
         }
         if (r->y < r->y1)
            r->row = r->out + (size_t)r->y * r->stride;
      }
   }
}

/* The image's LZW stream, as stb_image reads it; 0 where it is broken */
static int rg_raster(rgif_t *g, struct rg_in *in, struct rg_raster *r)
{
   unsigned lzw_cs = rg_get8(in);
   unsigned clear, codesize, codemask, avail, bits = 0, valid = 0, len = 0;
   int      oldcode = -1, first = 1;
   unsigned i;

   if (lzw_cs > 12)
      return 0;
   clear    = 1u << lzw_cs;
   codesize = lzw_cs + 1;
   codemask = (1u << codesize) - 1;
   for (i = 0; i < clear; i++)
   {
      g->prefix[i] = -1;
      g->first[i]  = (uint8_t)i;
      g->suffix[i] = (uint8_t)i;
   }
   avail = clear + 2;

   for (;;)
   {
      unsigned code;
      if (valid < codesize)
      {
         if (!len)
         {
            len = rg_get8(in); /* the next sub-block */
            if (!len)
               return 1;
         }
         len--;
         bits  |= rg_get8(in) << valid;
         valid += 8;
         continue;
      }
      code   = bits & codemask;
      bits >>= codesize;
      valid -= codesize;

      if (code == clear)
      {
         codesize = lzw_cs + 1;
         codemask = (1u << codesize) - 1;
         avail    = clear + 2;
         oldcode  = -1;
         first    = 0;
      }
      else if (code == clear + 1)
      {
         /* the end: what is left of the sub-blocks is skipped */
         rg_skip(in, len);
         while ((len = rg_get8(in)) > 0)
            rg_skip(in, len);
         return 1;
      }
      else if (code <= avail)
      {
         if (first)
            return 0; /* stb_image wants a clear code first */
         if (oldcode >= 0)
         {
            /* past 4096 a code can never be named; stb_image still
             * counts them, up to 8192 */
            if (avail < RG_CODES)
            {
               g->prefix[avail] = (int16_t)oldcode;
               g->first[avail]  = g->first[oldcode];
               g->suffix[avail] = code == avail ? g->first[oldcode]
                  : g->first[code];
            }
            if (++avail > 8192)
               return 0;
         }
         else if (code == avail)
            return 0;

         rg_emit(g, r, code);

         if ((avail & codemask) == 0 && avail <= 0x0fff)
         {
            codesize++;
            codemask = (1u << codesize) - 1;
         }
         oldcode = (int)code;
      }
      else
         return 0;
   }
}

bool rgif_decode(rgif_t *g, const uint8_t *data, size_t len,
      uint8_t *out, size_t stride, unsigned order)
{
   struct rg_in in;
   unsigned     w, h, flags, bgindex, eflags = 0, i;
   int          transparent = -1;
   uint8_t      bg[4];

   in.p = data; in.at = 0; in.len = data ? len : 0;
   if (!g || !rg_header(&in, &w, &h))
      return false;
   flags   = rg_get8(&in);
   bgindex = rg_get8(&in);
   rg_get8(&in); /* aspect ratio */

   memset(g->pal, 0, sizeof(g->pal));
   memset(g->lpal, 0, sizeof(g->lpal));
   if (flags & 0x80)
      rg_colours(&in, g->pal, 2u << (flags & 7), -1);

   /* Where the image does not reach: the background, opaque, unless the
    * file names entry 0 (none of it, in stb_image's reading) */
   if (bgindex > 0)
      rg_put(bg, order, g->pal[bgindex][0], g->pal[bgindex][1],
            g->pal[bgindex][2], 255);
   else
      memset(bg, 0, sizeof(bg));
   {
      unsigned y, x;
      for (y = 0; y < h; y++)
      {
         uint8_t *row = out + (size_t)y * stride;
         if (!bgindex)
            memset(row, 0, (size_t)w * 4);
         else
            for (x = 0; x < w; x++)
               memcpy(row + (size_t)x * 4, bg, 4);
      }
   }

   for (;;)
   {
      unsigned tag = rg_get8(&in);

      if (tag == 0x2c) /* an image */
      {
         struct rg_raster r;
         unsigned x = rg_get16(&in), y = rg_get16(&in);
         unsigned iw = rg_get16(&in), ih = rg_get16(&in);
         unsigned lflags;
         uint8_t (*table)[4];

         if (x + iw > w || y + ih > h)
            return false;
         lflags = rg_get8(&in);
         if (lflags & 0x80)
         {
            rg_colours(&in, g->lpal, 2u << (lflags & 7),
                  (eflags & 1) ? transparent : -1);
            table = g->lpal;
         }
         else if (flags & 0x80)
            table = g->pal;
         else
            return false;

         /* a transparent index, or one past the table, leaves the pixel
          * as stb_image's first frame has it: transparent black */
         for (i = 0; i < 256; i++)
         {
            if (table[i][3] > 128)
               rg_put(g->px[i], order, table[i][0], table[i][1], table[i][2],
                     table[i][3]);
            else
               memset(g->px[i], 0, 4);
         }

         r.out    = out;
         r.stride = stride;
         r.x0     = x;
         r.x1     = x + iw;
         r.y0     = y;
         r.y1     = y + ih;
         r.x      = x;
         r.y      = iw ? y : r.y1; /* no columns: nothing to draw */
         if (lflags & 0x40)
         {
            r.step  = 8;
            r.parse = 3;
         }
         else
         {
            r.step  = 1;
            r.parse = 0;
         }
         r.row = out + (size_t)y * stride;
         return rg_raster(g, &in, &r) != 0;
      }
      else if (tag == 0x21) /* an extension */
      {
         unsigned n;
         if (rg_get8(&in) == 0xf9) /* graphic control */
         {
            n = rg_get8(&in);
            if (n != 4)
            {
               /* stb_image skips it and reads on from there */
               rg_skip(&in, n);
               continue;
            }
            eflags = rg_get8(&in);
            rg_get16(&in); /* delay */
            if (transparent >= 0)
               g->pal[transparent][3] = 255;
            if (eflags & 1)
            {
               transparent = (int)rg_get8(&in);
               g->pal[transparent][3] = 0;
            }
            else
            {
               rg_skip(&in, 1);
               transparent = -1;
            }
         }
         while ((n = rg_get8(&in)) != 0)
            rg_skip(&in, n);
      }
      else
         return false; /* the trailer before any image, or no GIF block */
   }
}

bool rgif_header_ready(const uint8_t *data, size_t len)
{
   unsigned w, h;
   return data && len >= 13 && rgif_size(data, len, &w, &h);
}

bool rgif_set_buf_ptr(rgif_t *g, void *data, size_t len)
{
   if (!g)
      return false;
   g->buf   = (const uint8_t*)data;
   g->len   = len;
   g->avail = len;
   return true;
}

void rgif_set_avail(rgif_t *g, size_t avail)
{
   if (g)
      g->avail = avail < g->len ? avail : g->len;
}

int rgif_process_image(rgif_t *g, void **buf, size_t size,
      unsigned *width, unsigned *height, bool supports_rgba)
{
   unsigned w, h, order;
   size_t   len;
   uint8_t *px;

   if (!g || !g->buf || !buf)
      return IMAGE_PROCESS_ERROR;
   len = g->len ? g->len : size;
   if (g->avail < len)
      return IMAGE_PROCESS_WAIT; /* nothing taken: the whole file first */
   if (!rgif_size(g->buf, len, &w, &h) || !w || !h)
      return IMAGE_PROCESS_ERROR;
   if (!(px = (uint8_t*)malloc((size_t)w * h * 4)))
      return IMAGE_PROCESS_ERROR;
   /* the 32-bit pixel, ARGB or ABGR from the high byte, as it lies in
    * memory */
   if (is_little_endian())
      order = supports_rgba ? RGIF_RGBA : RGIF_BGRA;
   else
      order = supports_rgba ? RGIF_ABGR : RGIF_ARGB;
   if (!rgif_decode(g, g->buf, len, px, (size_t)w * 4, order))
   {
      free(px);
      return IMAGE_PROCESS_ERROR;
   }
   *buf    = px;
   *width  = w;
   *height = h;
   return IMAGE_PROCESS_END;
}

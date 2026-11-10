/* rgif: GIFs written here - every colour table size, plain and
 * interlaced, a global or the image's own table, transparency, an image
 * that does not fill the screen with and without a background colour,
 * LZW tables that fill and are cleared or left full - decoded into rows
 * of any pitch in each byte order and checked pixel for pixel; files cut
 * short decoded as far as they go; broken files refused. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <formats/image.h>
#include <formats/rgif.h>

static int fails;

#define CHECK(c, what) do { if (!(c)) { printf("FAIL: %s (%s:%d)\n", what, __FILE__, __LINE__); fails++; } } while (0)

/* ---- a small GIF writer ---- */

struct out
{
   uint8_t *p;
   size_t   n, cap;
};

static void put(struct out *o, const void *d, size_t n)
{
   if (o->n + n > o->cap)
   {
      o->cap = (o->n + n) * 2 + 64;
      o->p   = (uint8_t*)realloc(o->p, o->cap);
   }
   memcpy(o->p + o->n, d, n);
   o->n += n;
}

static void put8(struct out *o, unsigned v)
{
   uint8_t b = (uint8_t)v;
   put(o, &b, 1);
}

static void put16(struct out *o, unsigned v)
{
   put8(o, v & 0xff);
   put8(o, v >> 8);
}

/* LZW, packed into sub-blocks; the code size steps as a decoder's does */
struct lzw
{
   struct out *o;
   uint8_t     block[255];
   unsigned    block_n;
   uint32_t    bits;
   unsigned    nbits;
   unsigned    min, cs, next, avail;
   int         first;
   int16_t    *dict;     /* [4096][256]: the code for a string and an index */
};

static void lzw_byte(struct lzw *z, unsigned b)
{
   z->block[z->block_n++] = (uint8_t)b;
   if (z->block_n == 255)
   {
      put8(z->o, 255);
      put(z->o, z->block, 255);
      z->block_n = 0;
   }
}

static void lzw_bits(struct lzw *z, unsigned code)
{
   z->bits  |= (uint32_t)code << z->nbits;
   z->nbits += z->cs;
   while (z->nbits >= 8)
   {
      lzw_byte(z, z->bits & 0xff);
      z->bits  >>= 8;
      z->nbits  -= 8;
   }
}

static void lzw_reset(struct lzw *z)
{
   z->cs    = z->min + 1;
   z->next  = (1u << z->min) + 2;
   z->avail = z->next;
   z->first = 1;
   memset(z->dict, 0xff, sizeof(int16_t) * 4096 * 256);
}

static void lzw_clear(struct lzw *z)
{
   lzw_bits(z, 1u << z->min);
   lzw_reset(z);
}

static void lzw_code(struct lzw *z, unsigned code)
{
   unsigned mask;
   lzw_bits(z, code);
   if (!z->first)
      z->avail++;
   z->first = 0;
   mask = (1u << z->cs) - 1;
   if ((z->avail & mask) == 0 && z->avail <= 0x0fff)
      z->cs++;
}

/* @full: 1 clears a full table, 0 goes on with it full */
static void lzw_encode(struct out *o, const uint8_t *idx, size_t n,
      unsigned min, int full)
{
   struct lzw z;
   size_t     i;
   unsigned   w;

   memset(&z, 0, sizeof(z));
   z.o    = o;
   z.min  = min;
   z.dict = (int16_t*)malloc(sizeof(int16_t) * 4096 * 256);
   put8(o, min);
   lzw_reset(&z);
   lzw_clear(&z);
   w = idx[0];
   for (i = 1; i < n; i++)
   {
      unsigned k = idx[i];
      int16_t  c = z.dict[w * 256 + k];
      if (c >= 0)
      {
         w = (unsigned)c;
         continue;
      }
      lzw_code(&z, w);
      if (z.next < 4096)
      {
         z.dict[w * 256 + k] = (int16_t)z.next++;
         if (z.next == 4096 && full)
            lzw_clear(&z);
      }
      w = k;
   }
   lzw_code(&z, w);
   lzw_bits(&z, (1u << min) + 1);
   if (z.nbits)
      lzw_byte(&z, z.bits & 0xff);
   if (z.block_n)
   {
      put8(o, z.block_n);
      put(o, z.block, z.block_n);
   }
   put8(o, 0);
   free(z.dict);
}

struct gif_desc
{
   unsigned w, h;                  /* the screen */
   unsigned x, y, iw, ih;          /* the image */
   unsigned bits;                  /* colour table: 2 << (bits - 1) entries */
   int      local;                 /* the image's own table, no global one */
   int      bgindex;
   int      transparent;           /* -1: none */
   int      interlace;
   int      full_clear;
   const uint8_t *idx;             /* iw x ih indices */
};

static void table(uint8_t *pal, unsigned n, unsigned seed)
{
   unsigned i;
   for (i = 0; i < n * 3; i++)
      pal[i] = (uint8_t)(i * 37 + seed * 11 + 5);
}

static void write_gif(struct out *o, const struct gif_desc *d)
{
   unsigned n = 2u << (d->bits - 1);
   uint8_t  pal[768];
   uint8_t *order = (uint8_t*)malloc((size_t)d->iw * d->ih + 1);
   size_t   k = 0;

   put(o, "GIF89a", 6);
   put16(o, d->w);
   put16(o, d->h);
   put8(o, d->local ? 0 : 0x80 | (d->bits - 1));
   put8(o, d->bgindex);
   put8(o, 0);
   if (!d->local)
   {
      table(pal, n, 1);
      put(o, pal, n * 3);
   }
   put(o, "\x21\xfe\x03" "abc" "\0", 7); /* a comment */
   if (d->transparent >= 0)
   {
      put(o, "\x21\xf9\x04\x01\x00\x00", 6);
      put8(o, d->transparent);
      put8(o, 0);
   }
   put8(o, 0x2c);
   put16(o, d->x);
   put16(o, d->y);
   put16(o, d->iw);
   put16(o, d->ih);
   put8(o, (d->local ? 0x80 | (d->bits - 1) : 0) | (d->interlace ? 0x40 : 0));
   if (d->local)
   {
      table(pal, n, 2);
      put(o, pal, n * 3);
   }
   /* the rows in the order they are stored */
   if (d->interlace)
   {
      static const unsigned start[4] = { 0, 4, 2, 1 }, step[4] = { 8, 8, 4, 2 };
      unsigned p, y;
      for (p = 0; p < 4; p++)
         for (y = start[p]; y < d->ih; y += step[p])
         {
            memcpy(order + k, d->idx + (size_t)y * d->iw, d->iw);
            k += d->iw;
         }
   }
   else
   {
      memcpy(order, d->idx, (size_t)d->iw * d->ih);
      k = (size_t)d->iw * d->ih;
   }
   lzw_encode(o, order, k ? k : 1, d->bits < 2 ? 2 : d->bits, d->full_clear);
   put8(o, 0x3b);
   free(order);
}

/* what the screen should hold, RGBA */
static void expect(const struct gif_desc *d, uint8_t *e)
{
   unsigned n = 2u << (d->bits - 1);
   uint8_t  gpal[768], lpal[768];
   const uint8_t *pal;
   unsigned x, y;

   memset(gpal, 0, sizeof(gpal));
   if (!d->local)
      table(gpal, n, 1);
   table(lpal, n, 2);
   pal = d->local ? lpal : gpal;
   for (y = 0; y < d->h; y++)
      for (x = 0; x < d->w; x++)
      {
         uint8_t *p = e + ((size_t)y * d->w + x) * 4;
         if (d->bgindex > 0)
         {
            p[0] = gpal[d->bgindex * 3];
            p[1] = gpal[d->bgindex * 3 + 1];
            p[2] = gpal[d->bgindex * 3 + 2];
            p[3] = 255;
         }
         else
            memset(p, 0, 4);
      }
   for (y = 0; y < d->ih; y++)
      for (x = 0; x < d->iw; x++)
      {
         unsigned i = d->idx[(size_t)y * d->iw + x];
         uint8_t *p = e + ((size_t)(d->y + y) * d->w + d->x + x) * 4;
         if ((int)i == d->transparent || i >= n)
            memset(p, 0, 4);
         else
         {
            p[0] = pal[i * 3];
            p[1] = pal[i * 3 + 1];
            p[2] = pal[i * 3 + 2];
            p[3] = 255;
         }
      }
}

static void check_gif(rgif_t *g, const struct gif_desc *d, const char *what)
{
   static const unsigned orders[3] = { RGIF_RGBA, RGIF_ARGB, RGIF_BGRA };
   struct out o;
   size_t     stride = (size_t)d->w * 4 + 12;
   uint8_t   *e      = (uint8_t*)malloc((size_t)d->w * d->h * 4 + 4);
   uint8_t   *got    = (uint8_t*)malloc(stride * d->h + 4);
   unsigned   w, h, k;

   memset(&o, 0, sizeof(o));
   write_gif(&o, d);
   expect(d, e);
   CHECK(rgif_size(o.p, o.n, &w, &h) && w == d->w && h == d->h, what);
   for (k = 0; k < 3; k++)
   {
      unsigned y, x;
      int      ok = 1;
      memset(got, 0xa5, stride * d->h + 4);
      CHECK(rgif_decode(g, o.p, o.n, got, stride, orders[k]), what);
      for (y = 0; y < d->h; y++)
      {
         const uint8_t *row = got + y * stride;
         for (x = 0; x < d->w; x++)
         {
            const uint8_t *s = e + ((size_t)y * d->w + x) * 4;
            const uint8_t *p = row + x * 4;
            uint8_t want[4];
            if (orders[k] == RGIF_ARGB)
            {
               want[0] = s[3]; want[1] = s[0]; want[2] = s[1]; want[3] = s[2];
            }
            else if (orders[k] == RGIF_BGRA)
            {
               want[0] = s[2]; want[1] = s[1]; want[2] = s[0]; want[3] = s[3];
            }
            else
               memcpy(want, s, 4);
            if (memcmp(p, want, 4))
               ok = 0;
         }
         /* the pitch's padding is the caller's */
         for (x = d->w * 4; x < stride; x++)
            if (row[x] != 0xa5)
               ok = 0;
      }
      CHECK(ok, what);
   }

   /* cut short: what came before the cut, without reading past it */
   if (o.n > 40)
   {
      uint8_t *cut = (uint8_t*)malloc(o.n / 2);
      memcpy(cut, o.p, o.n / 2);
      rgif_decode(g, cut, o.n / 2, got, stride, RGIF_RGBA);
      free(cut);
   }

   free(o.p);
   free(e);
   free(got);
}

static uint8_t *noise(unsigned n, unsigned colours, unsigned seed, int smooth)
{
   uint8_t *p = (uint8_t*)malloc(n + 1);
   uint32_t x = seed * 2654435761u + 1;
   unsigned i;
   for (i = 0; i < n; i++)
   {
      x = x * 1103515245u + 12345u;
      /* runs, so the table grows with long strings too */
      p[i] = (uint8_t)(smooth && i && (x >> 28) < 12 ? p[i - 1]
            : (x >> 16) % colours);
   }
   return p;
}

static void broken(rgif_t *g)
{
   static const uint8_t pal[6] = { 0, 0, 0, 255, 255, 255 };
   static const uint8_t lzw[] = { 2, 2, 0x44, 0x0a, 0 };
   uint8_t    px[64];
   struct out o;
   unsigned   w, h;

   /* a 2x1 screen, one image: index 0 then 1 */
   memset(&o, 0, sizeof(o));
   put(&o, "GIF87a\x02\x00\x01\x00\x80\x00\x00", 13);
   put(&o, pal, 6);
   put(&o, "\x2c\0\0\0\0\x02\0\x01\0\0", 10);
   put(&o, lzw, sizeof(lzw));
   put8(&o, 0x3b);
   CHECK(rgif_decode(g, o.p, o.n, px, 8, RGIF_RGBA), "a hand-made GIF87a");
   CHECK(!memcmp(px, "\0\0\0\xff\xff\xff\xff\xff", 8), "a hand-made GIF87a: its pixels");

   o.p[3] = '7'; o.p[4] = '8';
   CHECK(!rgif_size(o.p, o.n, &w, &h) && !rgif_decode(g, o.p, o.n, px, 8, RGIF_RGBA),
         "not a GIF");
   o.p[3] = '8'; o.p[4] = '7';

   o.p[13 + 6 + 5] = 3; /* the image 3 wide on a screen 2 wide */
   CHECK(!rgif_decode(g, o.p, o.n, px, 8, RGIF_RGBA), "an image past the screen");
   o.p[13 + 6 + 5] = 2;

   o.p[10] = 0; /* no global table, and none of its own */
   CHECK(!rgif_decode(g, o.p, o.n, px, 8, RGIF_RGBA), "no colour table");
   o.p[10] = 0x80;

   o.p[13 + 6 + 10] = 13; /* LZW codes over 12 bits */
   CHECK(!rgif_decode(g, o.p, o.n, px, 8, RGIF_RGBA), "an LZW code size of 13");
   o.p[13 + 6 + 10] = 2;

   o.p[13 + 6 + 12] = 0x40; /* the first code is not a clear */
   CHECK(!rgif_decode(g, o.p, o.n, px, 8, RGIF_RGBA), "no clear code first");
   o.p[13 + 6 + 12] = 0x44;

   o.p[13 + 6] = 0x2b; /* no such block */
   CHECK(!rgif_decode(g, o.p, o.n, px, 8, RGIF_RGBA), "an unknown block");
   o.p[13 + 6] = 0x3b; /* the trailer, and no image */
   CHECK(!rgif_decode(g, o.p, o.n, px, 8, RGIF_RGBA), "no image");
   CHECK(!rgif_decode(g, o.p, 5, px, 8, RGIF_RGBA), "a header cut short");
   CHECK(!rgif_decode(g, NULL, 0, px, 8, RGIF_RGBA), "no file");
   free(o.p);
}

/* The image_transfer interface: a 32-bit texture of its own, ARGB32 or
 * ABGR32 in native order; nothing until the whole file is resident */
static void texture(rgif_t *g)
{
   struct gif_desc d;
   struct out      o;
   uint8_t        *idx = noise(13 * 9, 16, 77, 1);
   uint8_t        *ref = (uint8_t*)malloc(13 * 9 * 4);
   void           *px  = NULL;
   unsigned        w = 0, h = 0, k;
   uint32_t        one = 1;
   int             le  = *(uint8_t*)&one == 1;

   memset(&d, 0, sizeof(d));
   memset(&o, 0, sizeof(o));
   d.w = d.iw = 13; d.h = d.ih = 9; d.bits = 4; d.transparent = 3;
   d.idx = idx;
   write_gif(&o, &d);

   CHECK(rgif_header_ready(o.p, o.n) && !rgif_header_ready(o.p, 12),
         "header_ready");
   for (k = 0; k < 2; k++)
   {
      int rgba = (int)k;
      unsigned order = le ? (rgba ? RGIF_RGBA : RGIF_BGRA)
                          : (rgba ? RGIF_ABGR : RGIF_ARGB);
      rgif_set_buf_ptr(g, o.p, o.n);
      rgif_set_avail(g, o.n - 1);
      CHECK(rgif_process_image(g, &px, o.n, &w, &h, rgba != 0)
            == IMAGE_PROCESS_WAIT && !px, "a file not all in: wait");
      rgif_set_avail(g, o.n);
      CHECK(rgif_process_image(g, &px, o.n, &w, &h, rgba != 0)
            == IMAGE_PROCESS_END && px && w == 13 && h == 9,
            "the texture");
      rgif_decode(g, o.p, o.n, ref, 13 * 4, order);
      CHECK(px && !memcmp(px, ref, 13 * 9 * 4),
            "the texture: ARGB32 / ABGR32 in native order");
      free(px);
      px = NULL;
   }
   o.p[0] = 'X';
   rgif_set_buf_ptr(g, o.p, o.n);
   CHECK(rgif_process_image(g, &px, o.n, &w, &h, true) == IMAGE_PROCESS_ERROR
         && !px, "the texture of no GIF");
   free(o.p);
   free(idx);
   free(ref);
}

int main(void)
{
   static const unsigned sizes[][2] = { { 1, 1 }, { 7, 5 }, { 33, 17 }, { 160, 120 } };
   rgif_t  *g = rgif_new();
   unsigned s, bits, il, var;
   char     what[128];

   for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++)
   for (bits = 1; bits <= 8; bits++)
   for (il = 0; il < 2; il++)
   for (var = 0; var < 4; var++)
   {
      struct gif_desc d;
      unsigned colours = 2u << (bits - 1);
      memset(&d, 0, sizeof(d));
      d.w = sizes[s][0]; d.h = sizes[s][1];
      d.iw = d.w; d.ih = d.h;
      d.bits = bits;
      d.interlace = (int)il;
      d.transparent = -1;
      d.full_clear = 1;
      switch (var)
      {
         case 1: /* its own table and a transparent index */
            d.local = 1;
            d.transparent = (int)(colours / 2);
            break;
         case 2: /* not the whole screen, a background colour */
            d.x = d.w / 3; d.y = d.h / 4;
            d.iw = d.w - d.x; d.ih = d.h - d.y - (d.h > 2);
            d.bgindex = (int)(colours - 1);
            d.transparent = 0;
            break;
         case 3: /* not the whole screen, background entry 0: none */
            d.x = d.w > 1; d.iw = d.w - d.x;
            d.ih = d.h / 2 + 1 > d.h ? d.h : d.h / 2 + 1;
            break;
      }
      /* every index the LZW codes can name: past a 2-entry table too */
      d.idx = noise(d.iw * d.ih, bits < 2 ? 4 : colours,
            s * 64 + bits * 8 + il * 4 + var, var & 1);
      sprintf(what, "%ux%u, %u colours, %s, case %u", d.w, d.h, colours,
            il ? "interlaced" : "plain", var);
      check_gif(g, &d, what);
      free((void*)d.idx);
   }

   /* a full table: cleared, or kept as it is */
   {
      struct gif_desc d;
      memset(&d, 0, sizeof(d));
      d.w = d.iw = 300; d.h = d.ih = 200; d.bits = 8; d.transparent = -1;
      d.idx = noise(d.iw * d.ih, 256, 7, 0);
      d.full_clear = 1;
      check_gif(g, &d, "a table that fills, cleared");
      free((void*)d.idx);
      d.w = d.iw = 70; d.h = d.ih = 40; d.full_clear = 0;
      d.idx = noise(d.iw * d.ih, 256, 9, 0);
      check_gif(g, &d, "a table that fills and stays full");
      free((void*)d.idx);
   }

   broken(g);
   texture(g);
   rgif_free(g);

   printf("rgif_test: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
   return fails ? 1 : 0;
}

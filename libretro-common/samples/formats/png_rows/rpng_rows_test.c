/* rpng_rows: every colour type and bit depth, plain and Adam7, written
 * here with every filter type, decoded a few bytes at a time and put
 * back together from the rows; the chunks before and after the image
 * counted and dropped as libpng would; the colour chunks' contents kept,
 * and dropped where libpng drops them; broken files refused. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <encodings/crc32.h>
#include <encodings/deflate.h>
#include <formats/rpng_rows.h>

static int fails;

#define CHECK(c, what) do { if (!(c)) { printf("FAIL: %s (%s:%d)\n", what, __FILE__, __LINE__); fails++; } } while (0)

static const uint8_t x0s[7] = { 0, 4, 0, 2, 0, 1, 0 };
static const uint8_t y0s[7] = { 0, 0, 4, 0, 2, 0, 1 };
static const uint8_t dxs[7] = { 8, 8, 4, 4, 2, 2, 1 };
static const uint8_t dys[7] = { 8, 8, 8, 4, 4, 2, 2 };

/* ---- a small PNG writer ---- */

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

static void put32(struct out *o, uint32_t v)
{
   uint8_t b[4];
   b[0] = (uint8_t)(v >> 24); b[1] = (uint8_t)(v >> 16);
   b[2] = (uint8_t)(v >> 8);  b[3] = (uint8_t)v;
   put(o, b, 4);
}

static void chunk(struct out *o, const char *type, const uint8_t *d,
      uint32_t n, int bad_crc)
{
   uint32_t crc;
   put32(o, n);
   put(o, type, 4);
   if (n)
      put(o, d, n);
   crc = encoding_crc32(0, (const uint8_t*)type, 4);
   crc = encoding_crc32(crc, d, n);
   put32(o, bad_crc ? crc ^ 1 : crc);
}

static unsigned get_px(const uint8_t *row, uint32_t x, unsigned bits)
{
   if (bits >= 8)
      return 0; /* not used for whole bytes */
   return (row[(x * bits) / 8] >> (8 - bits - (x * bits) % 8)) & ((1u << bits) - 1);
}

static void set_px(uint8_t *row, uint32_t x, unsigned bits, unsigned v)
{
   unsigned sh = 8 - bits - (x * bits) % 8;
   uint8_t  m  = (uint8_t)(((1u << bits) - 1) << sh);
   row[(x * bits) / 8] = (uint8_t)((row[(x * bits) / 8] & ~m) | ((v << sh) & m));
}

/* pixel x of a row, any depth, into dst at pixel i */
static void copy_px(uint8_t *dst, uint32_t i, const uint8_t *src, uint32_t x,
      unsigned bits)
{
   if (bits < 8)
      set_px(dst, i, bits, get_px(src, x, bits));
   else
      memcpy(dst + (size_t)i * (bits / 8), src + (size_t)x * (bits / 8), bits / 8);
}

static int paeth(int a, int b, int c)
{
   int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
   if (pa <= pb && pa <= pc) return a;
   return pb <= pc ? b : c;
}

/* the image's scanlines, filtered, cycling through the filter types */
static size_t scanlines(uint8_t *raw, const uint8_t *img, uint32_t w,
      uint32_t h, unsigned bits, int interlace)
{
   size_t   stride = ((size_t)w * bits + 7) / 8;
   size_t   bpp    = bits >= 8 ? bits / 8 : 1;
   size_t   n      = 0;
   unsigned p, np  = interlace ? 7 : 1, ft = 0;
   uint8_t *line   = (uint8_t*)calloc(1, stride + 1);
   uint8_t *prev   = (uint8_t*)calloc(1, stride + 1);

   for (p = 0; p < np; p++)
   {
      uint32_t pw = interlace ? (w > x0s[p] ? (w - x0s[p] + dxs[p] - 1) / dxs[p] : 0) : w;
      uint32_t ph = interlace ? (h > y0s[p] ? (h - y0s[p] + dys[p] - 1) / dys[p] : 0) : h;
      size_t   rb = ((size_t)pw * bits + 7) / 8;
      uint32_t r;
      if (!pw || !ph)
         continue;
      memset(prev, 0, stride + 1);
      for (r = 0; r < ph; r++)
      {
         uint32_t y = interlace ? y0s[p] + r * dys[p] : r;
         uint32_t i;
         size_t   k;
         uint8_t *out = raw + n;
         memset(line, 0, rb);
         for (i = 0; i < pw; i++)
            copy_px(line, i, img + y * stride, interlace ? x0s[p] + i * dxs[p] : i, bits);
         out[0] = (uint8_t)(ft % 5);
         for (k = 0; k < rb; k++)
         {
            int a = k >= bpp ? line[k - bpp] : 0, b = prev[k], c = k >= bpp ? prev[k - bpp] : 0;
            int v = line[k];
            switch (ft % 5)
            {
               case 1: v -= a; break;
               case 2: v -= b; break;
               case 3: v -= (a + b) >> 1; break;
               case 4: v -= paeth(a, b, c); break;
            }
            out[1 + k] = (uint8_t)v;
         }
         memcpy(prev, line, rb);
         n += rb + 1;
         ft++;
      }
   }
   free(line);
   free(prev);
   return n;
}

static void write_png(struct out *o, const uint8_t *img, uint32_t w, uint32_t h,
      unsigned depth, unsigned ct, int interlace, int extras)
{
   static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
   unsigned ch    = ct == 2 ? 3 : ct == 4 ? 2 : ct == 6 ? 4 : 1;
   unsigned bits  = depth * ch;
   size_t   rawn  = (((size_t)w * bits + 7) / 8 + 1) * h * 2 + 64;
   uint8_t *raw   = (uint8_t*)malloc(rawn);
   uint8_t *z     = (uint8_t*)malloc(rawn * 2 + 1024);
   uint8_t  ihdr[13];
   size_t   n, zn = 0, rd = 0, wr = 0, off;
   void    *d;

   put(o, sig, 8);
   ihdr[0] = (uint8_t)(w >> 24); ihdr[1] = (uint8_t)(w >> 16); ihdr[2] = (uint8_t)(w >> 8); ihdr[3] = (uint8_t)w;
   ihdr[4] = (uint8_t)(h >> 24); ihdr[5] = (uint8_t)(h >> 16); ihdr[6] = (uint8_t)(h >> 8); ihdr[7] = (uint8_t)h;
   ihdr[8] = (uint8_t)depth; ihdr[9] = (uint8_t)ct; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = (uint8_t)interlace;
   chunk(o, "IHDR", ihdr, 13, 0);
   if (extras)
   {
      static const uint8_t gama[4] = { 0, 1, 0x86, 0xa0 };
      chunk(o, "gAMA", gama, 4, 0);
      chunk(o, "tEXt", (const uint8_t*)"Title\0x", 7, 0);
      chunk(o, "prVt", (const uint8_t*)"abc", 3, 0);    /* unknown, safe to copy */
      chunk(o, "prvT", (const uint8_t*)"abc", 3, 0);    /* unknown, not safe */
      chunk(o, "sRGB", (const uint8_t*)"\0", 1, 1);     /* bad CRC: dropped */
   }
   if (ct == 3)
   {
      uint8_t plte[256 * 3];
      unsigned i, np = 1u << depth;
      for (i = 0; i < np * 3; i++)
         plte[i] = (uint8_t)(i * 7);
      chunk(o, "PLTE", plte, np * 3, 0);
      if (extras)
      {
         static const uint8_t gama[4] = { 0, 1, 0x86, 0xa0 };
         chunk(o, "cHRM", gama, 4, 0);    /* after PLTE: dropped */
         chunk(o, "tRNS", (const uint8_t*)"\x10\x20", 2, 0);
      }
   }
   n = scanlines(raw, img, w, h, bits, interlace);
   d = rdeflate_new(6, 15);
   rdeflate_set_in(d, raw, n);
   rdeflate_finish(d);
   rdeflate_set_out(d, z, rawn * 2 + 1024);
   rdeflate_process(d, &rd, &zn);
   rdeflate_free(d);
   (void)wr;
   /* split over several IDATs */
   for (off = 0; off < zn; )
   {
      size_t part = zn - off < 97 ? zn - off : 97;
      chunk(o, "IDAT", z + off, (uint32_t)part, 0);
      off += part;
   }
   if (extras)
   {
      static const uint8_t t[7] = { 7, 0xe6, 1, 2, 3, 4, 5 };
      chunk(o, "tIME", t, 7, 0);
      chunk(o, "zTXt", (const uint8_t*)"C\0\0x\x9c\x03\0\0\0\0\x01", 11, 0);
      chunk(o, "bKGD", (const uint8_t*)"\0\0", 2, 0);  /* after IDAT: dropped */
   }
   chunk(o, "IEND", NULL, 0, 0);
   free(raw);
   free(z);
}

/* decodes in pieces of @step bytes, scattering rows into img */
static int decode(const uint8_t *png, size_t n, size_t step, uint8_t *img,
      size_t stride, struct rpng_rows_info *info_out, unsigned *rows_out)
{
   rpng_rows_t *z = rpng_rows_new();
   size_t at = 0, have = 0;
   int    ev, headers = 0;
   unsigned rows = 0;

   for (;;)
   {
      struct rpng_rows_row r;
      size_t used = 0;
      if (have == at && have < n)
         have = have + step < n ? have + step : n;
      ev = rpng_rows_step(z, png + at, have - at, &used, &r);
      at += used;
      if (ev == RPNG_ROWS_HEADER)
         headers++;
      else if (ev == RPNG_ROWS_ROW)
      {
         const struct rpng_rows_info *in = rpng_rows_info(z);
         unsigned bits = in->depth * in->channels;
         uint32_t i;
         for (i = 0; i < r.width; i++)
            copy_px(img + (size_t)r.y * stride, r.x0 + i * r.dx, r.data, i, bits);
         rows++;
      }
      else if (ev == RPNG_ROWS_END || ev == RPNG_ROWS_ERROR)
         break;
      else if (have == n)
         break; /* NEXT with nothing more */
   }
   if (info_out)
      *info_out = *rpng_rows_info(z);
   if (rows_out)
      *rows_out = rows;
   rpng_rows_free(z);
   return ev == RPNG_ROWS_END && headers == 1 ? 1 : ev == RPNG_ROWS_ERROR ? -1 : 0;
}

static void check_image(unsigned depth, unsigned ct, uint32_t w, uint32_t h,
      int interlace, size_t step, int extras)
{
   unsigned ch     = ct == 2 ? 3 : ct == 4 ? 2 : ct == 6 ? 4 : 1;
   size_t   stride = ((size_t)w * depth * ch + 7) / 8;
   uint8_t *img    = (uint8_t*)malloc(stride * h + 1);
   uint8_t *back   = (uint8_t*)calloc(1, stride * h + 1);
   struct out o;
   struct rpng_rows_info info;
   size_t i;
   int    r;
   char   what[96];

   memset(&o, 0, sizeof(o));
   srand(depth * 131 + ct * 17 + w + h * 3 + interlace);
   for (i = 0; i < stride * h; i++)
      img[i] = (uint8_t)(rand() >> 4);
   /* the padding bits of each row are zero in what comes back */
   if ((w * depth * ch) % 8)
   {
      uint32_t y;
      unsigned pad = 8 - (unsigned)((w * depth * ch) % 8);
      for (y = 0; y < h; y++)
         img[y * stride + stride - 1] &= (uint8_t)(0xff << pad);
   }
   /* a palette image's indices stay inside the palette */
   write_png(&o, img, w, h, depth, ct, interlace, extras);
   r = decode(o.p, o.n, step, back, stride, &info, NULL);
   sprintf(what, "depth %u type %u %ux%u %s step %u",
         depth, ct, (unsigned)w, (unsigned)h, interlace ? "Adam7" : "plain", (unsigned)step);
   CHECK(r == 1, what);
   CHECK(!memcmp(img, back, stride * h), what);
   CHECK(info.width == w && info.height == h && info.depth == depth
         && info.color_type == ct && info.channels == ch, what);
   if (extras)
   {
      uint32_t want = RPNG_ROWS_GAMA | RPNG_ROWS_IDAT | RPNG_ROWS_TIME
         | (ct == 3 ? RPNG_ROWS_PLTE | RPNG_ROWS_TRNS : 0);
      CHECK(info.chunks == want, "chunks seen, misplaced and broken ones dropped");
      CHECK(info.text_chunks == 2, "text chunks");
      CHECK(info.unknown_chunks == 1, "unknown chunks that are safe to copy");
      if (ct == 3)
         CHECK(info.trns_len == 2 && info.trns[1] == 0x20
               && info.palette_len == (1u << depth) && info.palette[3] == 21,
               "palette and tRNS");
   }

   /* cut short: not an image */
   r = decode(o.p, o.n - 13, step, back, stride, NULL, NULL);
   CHECK(r != 1, "truncated file");

   free(o.p);
   free(img);
   free(back);
}

/* A small image of type @ct with the chunks in @pre between IHDR and the
 * image data, read back */
static int colour_png(unsigned depth, unsigned ct, const struct out *pre,
      struct rpng_rows_info *info)
{
   struct out o;
   uint8_t    img[16], back[16];
   int        r;

   memset(&o, 0, sizeof(o));
   memset(img, 0, sizeof(img));
   write_png(&o, img, 1, 1, depth, ct, 0, 0);
   /* after the signature and IHDR */
   o.p = (uint8_t*)realloc(o.p, o.n + pre->n);
   memmove(o.p + 33 + pre->n, o.p + 33, o.n - 33);
   memcpy(o.p + 33, pre->p, pre->n);
   r = decode(o.p, o.n + pre->n, 3, back, 16, info, NULL);
   free(o.p);
   return r;
}

static void be32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
   p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

static void check_colour_chunks(void)
{
   static const uint32_t chrm[8] = { 31270, 32900, 64000, 33000,
      30000, 60000, 15000, 0xfffffc18u /* -1000 */ };
   static const uint8_t mdcv[24] = { 0x83, 0xe8, 0x3f, 0x7a, 0x34, 0xbc,
      0x89, 0x58, 0x1d, 0x4c, 0x0b, 0xb8, 0x3d, 0x13, 0x40, 0x42,
      0x00, 0x98, 0x96, 0x80, 0x00, 0x00, 0x00, 0x32 };
   struct rpng_rows_info info;
   struct out pre;
   uint8_t    b[32];
   unsigned   i;

   /* each kind, kept, in the order they came */
   memset(&pre, 0, sizeof(pre));
   chunk(&pre, "cICP", (const uint8_t*)"\x01\x0d\x00\x01", 4, 0);
   be32(b, 45455);
   chunk(&pre, "gAMA", b, 4, 0);
   for (i = 0; i < 8; i++)
      be32(b + i * 4, chrm[i]);
   chunk(&pre, "cHRM", b, 32, 0);
   chunk(&pre, "sRGB", (const uint8_t*)"\x02", 1, 0);
   chunk(&pre, "sBIT", (const uint8_t*)"\x05\x06\x05", 3, 0);
   chunk(&pre, "mDCV", mdcv, 24, 0);
   CHECK(colour_png(8, 2, &pre, &info) == 1, "colour chunks: decode");
   CHECK(info.chunks == (RPNG_ROWS_GAMA | RPNG_ROWS_CHRM | RPNG_ROWS_SRGB
         | RPNG_ROWS_SBIT | RPNG_ROWS_CICP | RPNG_ROWS_MDCV | RPNG_ROWS_IDAT),
         "colour chunks: all seen");
   CHECK(info.gama == 45455, "gAMA");
   CHECK(info.chrm[0] == 31270 && info.chrm[5] == 60000
         && info.chrm[7] == -1000, "cHRM, negative values too");
   CHECK(info.srgb == 2, "sRGB");
   CHECK(info.sbit[0] == 5 && info.sbit[1] == 6 && info.sbit[2] == 5
         && info.sbit[3] == 0, "sBIT");
   CHECK(!memcmp(info.cicp, "\x01\x0d\x00\x01", 4), "cICP");
   CHECK(info.mdcv[0] == 0x83e8 && info.mdcv[7] == 0x4042
         && info.mdcv_luminance[0] == 10000000 && info.mdcv_luminance[1] == 50,
         "mDCV");
   CHECK(info.cicp_at == 1 && info.gama_at == 2 && info.chrm_at == 3
         && info.srgb_at == 4 && info.sbit_at == 5 && info.mdcv_at == 6,
         "the order they came in");
   free(pre.p);

   /* the broken, the second of a kind: dropped; a good one after a broken
    * one is the first */
   memset(&pre, 0, sizeof(pre));
   be32(b, 0x80000000u);
   chunk(&pre, "gAMA", b, 4, 0);                 /* over 31 bits */
   be32(b, 50000);
   chunk(&pre, "gAMA", b, 4, 1);                 /* bad CRC */
   chunk(&pre, "gAMA", b, 3, 0);                 /* short */
   be32(b, 220000);
   chunk(&pre, "gAMA", b, 4, 0);                 /* this one */
   be32(b, 100000);
   chunk(&pre, "gAMA", b, 4, 0);                 /* a second */
   memset(b, 0, 32);
   be32(b + 12, 0x80000000u);
   chunk(&pre, "cHRM", b, 32, 0);                /* no such int32 */
   chunk(&pre, "sRGB", (const uint8_t*)"\x04", 1, 0);   /* no such intent */
   chunk(&pre, "sBIT", (const uint8_t*)"\x05\x09\x05", 3, 0); /* over 8 */
   chunk(&pre, "sBIT", (const uint8_t*)"\x05\x00\x05", 3, 0); /* none */
   chunk(&pre, "sBIT", (const uint8_t*)"\x05\x05\x05\x05", 4, 0); /* RGB: 3 */
   chunk(&pre, "cICP", (const uint8_t*)"\x01\x0d\x00", 3, 0);
   chunk(&pre, "mDCV", mdcv, 23, 0);
   CHECK(colour_png(8, 2, &pre, &info) == 1, "broken colour chunks: decode");
   CHECK(info.chunks == (RPNG_ROWS_GAMA | RPNG_ROWS_IDAT),
         "broken colour chunks dropped");
   CHECK(info.gama == 220000 && info.gama_at == 1,
         "the first good gAMA, not the second");
   free(pre.p);

   /* sBIT by colour type: a value for each channel, a palette's three of 8
    * bits, 16-bit gray up to 16 */
   memset(&pre, 0, sizeof(pre));
   chunk(&pre, "sBIT", (const uint8_t*)"\x10\x03", 2, 0);
   CHECK(colour_png(16, 4, &pre, &info) == 1 && (info.chunks & RPNG_ROWS_SBIT)
         && info.sbit[0] == 16 && info.sbit[1] == 3, "sBIT, 16-bit gray and alpha");
   free(pre.p);
   memset(&pre, 0, sizeof(pre));
   chunk(&pre, "sBIT", (const uint8_t*)"\x08\x01\x07", 3, 0);
   CHECK(colour_png(2, 3, &pre, &info) == 1 && (info.chunks & RPNG_ROWS_SBIT)
         && info.sbit[2] == 7, "sBIT, palette");
   free(pre.p);
   memset(&pre, 0, sizeof(pre));
   chunk(&pre, "sBIT", (const uint8_t*)"\x03", 1, 0);
   CHECK(colour_png(2, 0, &pre, &info) == 1 && !(info.chunks & RPNG_ROWS_SBIT),
         "sBIT over a gray image's depth");
   free(pre.p);

   /* after PLTE: out of place */
   memset(&pre, 0, sizeof(pre));
   chunk(&pre, "PLTE", (const uint8_t*)"\x01\x02\x03", 3, 0);
   chunk(&pre, "sRGB", (const uint8_t*)"\x00", 1, 0);
   chunk(&pre, "mDCV", mdcv, 24, 0);
   CHECK(colour_png(8, 2, &pre, &info) == 1
         && !(info.chunks & (RPNG_ROWS_SRGB | RPNG_ROWS_MDCV)),
         "colour chunks after PLTE");
   free(pre.p);
}

int main(void)
{
   static const unsigned cases[][2] = {
      { 1, 0 }, { 2, 0 }, { 4, 0 }, { 8, 0 }, { 16, 0 },
      { 8, 2 }, { 16, 2 },
      { 1, 3 }, { 2, 3 }, { 4, 3 }, { 8, 3 },
      { 8, 4 }, { 16, 4 },
      { 8, 6 }, { 16, 6 } };
   static const uint32_t sizes[][2] = { { 1, 1 }, { 3, 2 }, { 13, 9 }, { 33, 17 } };
   unsigned c, s, il;

   for (c = 0; c < sizeof(cases) / sizeof(cases[0]); c++)
      for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++)
         for (il = 0; il < 2; il++)
            check_image(cases[c][0], cases[c][1], sizes[s][0], sizes[s][1],
                  (int)il, s & 1 ? 1 : 4096, s == 2);

   check_colour_chunks();

   /* broken files */
   {
      struct out o;
      uint8_t img[64], back[64];
      size_t  i;
      memset(&o, 0, sizeof(o));
      for (i = 0; i < sizeof(img); i++)
         img[i] = (uint8_t)i;
      write_png(&o, img, 8, 8, 8, 0, 0, 0);
      o.p[1] = 'Q';
      CHECK(decode(o.p, o.n, 4096, back, 8, NULL, NULL) == -1, "not a PNG");
      o.p[1] = 'P';
      o.p[8 + 8 + 13] ^= 1; /* IHDR's CRC */
      CHECK(decode(o.p, o.n, 4096, back, 8, NULL, NULL) == -1, "IHDR with a bad CRC");
      free(o.p);
   }

   printf("rpng_rows_test: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
   return fails ? 1 : 0;
}

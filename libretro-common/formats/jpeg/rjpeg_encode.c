/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rjpeg_encode.c).
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

/* Baseline JPEG encoder. See rjpeg_encode.h. Written from ITU-T T.81:
 * the forward DCT is the scaled one of Arai, Agui and Nakajima, its
 * output scaling folded into the quantiser's divisors. */

#include <string.h>

#include <formats/rjpeg_encode.h>

/* T.81 Table K.1 and K.2, raster order */
static const uint8_t rjpeg_std_q[2][64] =
{
   {
      16, 11, 10, 16,  24,  40,  51,  61,
      12, 12, 14, 19,  26,  58,  60,  55,
      14, 13, 16, 24,  40,  57,  69,  56,
      14, 17, 22, 29,  51,  87,  80,  62,
      18, 22, 37, 56,  68, 109, 103,  77,
      24, 35, 55, 64,  81, 104, 113,  92,
      49, 64, 78, 87, 103, 121, 120, 101,
      72, 92, 95, 98, 112, 100, 103,  99
   },
   {
      17, 18, 24, 47, 99, 99, 99, 99,
      18, 21, 26, 66, 99, 99, 99, 99,
      24, 26, 56, 99, 99, 99, 99, 99,
      47, 66, 99, 99, 99, 99, 99, 99,
      99, 99, 99, 99, 99, 99, 99, 99,
      99, 99, 99, 99, 99, 99, 99, 99,
      99, 99, 99, 99, 99, 99, 99, 99,
      99, 99, 99, 99, 99, 99, 99, 99
   }
};

/* zigzag position -> raster index, T.81 Figure 5 */
static const uint8_t rjpeg_zigzag[64] =
{
    0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
   12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
   35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
   58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

/* T.81 Tables K.3 to K.6: code counts by length 1..16, then the symbols */
static const uint8_t rjpeg_dc_lum_bits[16] = { 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t rjpeg_dc_chr_bits[16] = { 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
static const uint8_t rjpeg_dc_vals[12]     = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

static const uint8_t rjpeg_ac_lum_bits[16] = { 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };
static const uint8_t rjpeg_ac_lum_vals[162] =
{
   0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
   0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
   0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
   0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
   0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
   0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
   0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
   0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
   0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
   0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
   0xf9, 0xfa
};

static const uint8_t rjpeg_ac_chr_bits[16] = { 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };
static const uint8_t rjpeg_ac_chr_vals[162] =
{
   0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
   0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
   0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
   0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
   0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
   0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
   0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
   0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
   0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
   0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
   0xf9, 0xfa
};

/* The DCT's output scaling, cos(k pi / 16) * sqrt(2) for k > 0 */
static const double rjpeg_aan[8] =
{
   1.0, 1.387039845, 1.306562965, 1.175875602,
   1.0, 0.785694958, 0.541196100, 0.275899379
};

/* Canonical codes from the counts, T.81 Annex C */
static void build_huff(rjpeg_encoder_t *e, unsigned t, const uint8_t *bits,
      const uint8_t *vals)
{
   unsigned len, k = 0, code = 0;
   for (len = 1; len <= 16; len++)
   {
      unsigned n;
      for (n = 0; n < bits[len - 1]; n++, k++)
      {
         e->code[t][vals[k]] = (uint16_t)code++;
         e->len[t][vals[k]]  = (uint8_t)len;
      }
      code <<= 1;
   }
}

void rjpeg_encoder_init(rjpeg_encoder_t *e, unsigned quality)
{
   unsigned scale, t, i;

   if (quality < 1)
      quality = 1;
   if (quality > 100)
      quality = 100;
   scale = quality < 50 ? 5000 / quality : 200 - quality * 2;

   for (t = 0; t < 2; t++)
      for (i = 0; i < 64; i++)
      {
         unsigned r = rjpeg_zigzag[i];
         unsigned q = (rjpeg_std_q[t][r] * scale + 50) / 100;
         double   d;
         if (q < 1)
            q = 1;
         if (q > 255)
            q = 255;
         e->qt[t][i] = (uint8_t)q;
         d = q * rjpeg_aan[r >> 3] * rjpeg_aan[r & 7] * 8.0;
         e->recip[t][r] = (uint32_t)(65536.0 / d + 0.5);
      }
   memset(e->code, 0, sizeof(e->code));
   memset(e->len, 0, sizeof(e->len));
   build_huff(e, 0, rjpeg_dc_lum_bits, rjpeg_dc_vals);
   build_huff(e, 1, rjpeg_ac_lum_bits, rjpeg_ac_lum_vals);
   build_huff(e, 2, rjpeg_dc_chr_bits, rjpeg_dc_vals);
   build_huff(e, 3, rjpeg_ac_chr_bits, rjpeg_ac_chr_vals);
}

size_t rjpeg_encode_bound(unsigned width, unsigned height)
{
   /* a block can take 64 codes of up to 26 bits, every byte of it
    * stuffed: under 420 bytes; three blocks a luma block at most */
   size_t blocks = (size_t)((width + 15) / 16) * ((height + 15) / 16) * 8;
   return blocks * 420 + 1024;
}

/* --------------------------------------------------------------------- */

typedef struct
{
   uint8_t *p;
   size_t   pos, cap;
   uint32_t acc;
   unsigned nbits;
   int      overflow;
} rjpeg_bw_t;

static void put_byte(rjpeg_bw_t *b, unsigned v)
{
   if (b->pos < b->cap)
      b->p[b->pos++] = (uint8_t)v;
   else
      b->overflow = 1;
}

/* Entropy-coded bits, 0xFF stuffed with a zero byte */
static void put_bits(rjpeg_bw_t *b, uint32_t v, unsigned n)
{
   b->acc    = (b->acc << n) | (v & ((1u << n) - 1));
   b->nbits += n;
   while (b->nbits >= 8)
   {
      unsigned byte = (b->acc >> (b->nbits - 8)) & 0xFF;
      b->nbits -= 8;
      put_byte(b, byte);
      if (byte == 0xFF)
         put_byte(b, 0);
   }
}

static void flush_bits(rjpeg_bw_t *b)
{
   if (b->nbits)
      put_bits(b, 0x7F, 8 - b->nbits);   /* padded with ones */
}

static void put_marker_len(rjpeg_bw_t *b, unsigned marker, unsigned len)
{
   put_byte(b, 0xFF);
   put_byte(b, marker);
   put_byte(b, len >> 8);
   put_byte(b, len & 0xFF);
}

static void put_dht(rjpeg_bw_t *b, unsigned cls_id, const uint8_t *bits,
      const uint8_t *vals)
{
   unsigned i, n = 0;
   for (i = 0; i < 16; i++)
      n += bits[i];
   put_marker_len(b, 0xC4, 2 + 1 + 16 + n);
   put_byte(b, cls_id);
   for (i = 0; i < 16; i++)
      put_byte(b, bits[i]);
   for (i = 0; i < n; i++)
      put_byte(b, vals[i]);
}

/* --------------------------------------------------------------------- */
/* Forward DCT                                                           */
/* --------------------------------------------------------------------- */

/* Arai, Agui and Nakajima, constants in 12 fractional bits. The outputs
 * carry 8 * aan[u] * aan[v] of gain, which rjpeg_encoder_init() folds
 * into the reciprocals. Products are written as multiplications: the
 * operands are routinely negative. */
#define C0_382683433 1567
#define C0_541196100 2217
#define C0_707106781 2896
#define C1_306562965 5352
#define MULC(x, c) (((x) * (c) + 2048) >> 12)

static void fdct_1d(int *d, unsigned step)
{
   int tmp0 = d[0 * step] + d[7 * step], tmp7 = d[0 * step] - d[7 * step];
   int tmp1 = d[1 * step] + d[6 * step], tmp6 = d[1 * step] - d[6 * step];
   int tmp2 = d[2 * step] + d[5 * step], tmp5 = d[2 * step] - d[5 * step];
   int tmp3 = d[3 * step] + d[4 * step], tmp4 = d[3 * step] - d[4 * step];
   int tmp10 = tmp0 + tmp3, tmp13 = tmp0 - tmp3;
   int tmp11 = tmp1 + tmp2, tmp12 = tmp1 - tmp2;
   int z1, z2, z3, z4, z5, z11, z13;

   d[0 * step] = tmp10 + tmp11;
   d[4 * step] = tmp10 - tmp11;
   z1 = MULC(tmp12 + tmp13, C0_707106781);
   d[2 * step] = tmp13 + z1;
   d[6 * step] = tmp13 - z1;

   tmp10 = tmp4 + tmp5;
   tmp11 = tmp5 + tmp6;
   tmp12 = tmp6 + tmp7;
   z5  = MULC(tmp10 - tmp12, C0_382683433);
   z2  = MULC(tmp10, C0_541196100) + z5;
   z4  = MULC(tmp12, C1_306562965) + z5;
   z3  = MULC(tmp11, C0_707106781);
   z11 = tmp7 + z3;
   z13 = tmp7 - z3;
   d[5 * step] = z13 + z2;
   d[3 * step] = z13 - z2;
   d[1 * step] = z11 + z4;
   d[7 * step] = z11 - z4;
}

/* --------------------------------------------------------------------- */

typedef struct
{
   const uint8_t *p;
   size_t         stride;
   unsigned       w, h;     /* the plane's own size */
} rjpeg_plane_t;

/* One block from (x, y), past-the-edge samples repeating the last ones;
 * transformed, quantised and coded */
static void code_block(rjpeg_bw_t *b, const rjpeg_plane_t *pl, unsigned x,
      unsigned y, const rjpeg_encoder_t *e, unsigned t, int *pred)
{
   int      d[64];
   int      q[64];
   unsigned i, j, run;
   int      diff, mag, cat;
   const uint32_t *recip = e->recip[t];
   const uint16_t *dcc = e->code[2 * t],     *acc = e->code[2 * t + 1];
   const uint8_t  *dcl = e->len[2 * t],      *acl = e->len[2 * t + 1];

   if (x + 8 <= pl->w && y + 8 <= pl->h)
      for (j = 0; j < 8; j++)
      {
         const uint8_t *row = pl->p + (y + j) * pl->stride + x;
         for (i = 0; i < 8; i++)
            d[j * 8 + i] = (int)row[i] - 128;
      }
   else
      for (j = 0; j < 8; j++)
      {
         unsigned       yy  = y + j < pl->h ? y + j : pl->h - 1;
         const uint8_t *row = pl->p + yy * pl->stride;
         for (i = 0; i < 8; i++)
            d[j * 8 + i] = (int)row[x + i < pl->w ? x + i : pl->w - 1] - 128;
      }

   for (j = 0; j < 8; j++)
      fdct_1d(d + j * 8, 1);
   for (i = 0; i < 8; i++)
      fdct_1d(d + i, 8);

   for (i = 0; i < 64; i++)
   {
      unsigned r = rjpeg_zigzag[i];
      int      c = d[r];
      uint32_t m = (uint32_t)(c < 0 ? -c : c);
      int      v = (int)((m * recip[r] + 32768u) >> 16);
      q[i] = c < 0 ? -v : v;
   }

   /* DC: the difference from the last block of the component */
   diff  = q[0] - *pred;
   *pred = q[0];
   mag   = diff < 0 ? -diff : diff;
   for (cat = 0; mag >> cat; cat++)
      ;
   put_bits(b, dcc[cat], dcl[cat]);
   if (cat)
      put_bits(b, (uint32_t)(diff < 0 ? diff - 1 : diff), (unsigned)cat);

   /* AC: run, size and the bits, ZRL for sixteen zeros, EOB after the last */
   run = 0;
   for (i = 1; i < 64; i++)
   {
      int v = q[i];
      if (!v)
      {
         run++;
         continue;
      }
      while (run > 15)
      {
         put_bits(b, acc[0xF0], acl[0xF0]);
         run -= 16;
      }
      mag = v < 0 ? -v : v;
      for (cat = 0; mag >> cat; cat++)
         ;
      put_bits(b, acc[(run << 4) | (unsigned)cat], acl[(run << 4) | (unsigned)cat]);
      put_bits(b, (uint32_t)(v < 0 ? v - 1 : v), (unsigned)cat);
      run = 0;
   }
   if (run)
      put_bits(b, acc[0], acl[0]);
}

size_t rjpeg_encode(const rjpeg_encoder_t *e, uint8_t *dst, size_t cap,
      unsigned width, unsigned height, enum rjpeg_encode_sampling sampling,
      const uint8_t *y, size_t y_stride,
      const uint8_t *cb, size_t cb_stride,
      const uint8_t *cr, size_t cr_stride)
{
   static const uint8_t jfif[14] =
   { 'J', 'F', 'I', 'F', 0, 1, 1, 0, 0, 1, 0, 1, 0, 0 };
   rjpeg_plane_t pl[3];
   rjpeg_bw_t    b;
   unsigned      mcu_h = sampling == RJPEG_ENCODE_420 ? 16 : 8;
   unsigned      vs    = sampling == RJPEG_ENCODE_420 ? 2 : 1;
   unsigned      i, mx, my;
   int           pred[3] = { 0, 0, 0 };

   if (!e || !dst || !y || !cb || !cr || !width || !height
         || width > 65535 || height > 65535)
      return 0;

   b.p = dst;
   b.pos = 0;
   b.cap = cap;
   b.acc = 0;
   b.nbits = 0;
   b.overflow = 0;

   pl[0].p = y;  pl[0].stride = y_stride;  pl[0].w = width;  pl[0].h = height;
   pl[1].p = cb; pl[1].stride = cb_stride;
   pl[2].p = cr; pl[2].stride = cr_stride;
   pl[1].w = pl[2].w = (width + 1) / 2;
   pl[1].h = pl[2].h = vs == 2 ? (height + 1) / 2 : height;

   put_byte(&b, 0xFF);
   put_byte(&b, 0xD8);
   put_marker_len(&b, 0xE0, 2 + sizeof(jfif));
   for (i = 0; i < sizeof(jfif); i++)
      put_byte(&b, jfif[i]);

   put_marker_len(&b, 0xDB, 2 + 2 * 65);
   for (i = 0; i < 2; i++)
   {
      unsigned k;
      put_byte(&b, i);
      for (k = 0; k < 64; k++)
         put_byte(&b, e->qt[i][k]);
   }

   put_marker_len(&b, 0xC0, 8 + 3 * 3);
   put_byte(&b, 8);
   put_byte(&b, height >> 8);
   put_byte(&b, height & 0xFF);
   put_byte(&b, width >> 8);
   put_byte(&b, width & 0xFF);
   put_byte(&b, 3);
   put_byte(&b, 1); put_byte(&b, 0x20 | vs); put_byte(&b, 0);
   put_byte(&b, 2); put_byte(&b, 0x11);      put_byte(&b, 1);
   put_byte(&b, 3); put_byte(&b, 0x11);      put_byte(&b, 1);

   put_dht(&b, 0x00, rjpeg_dc_lum_bits, rjpeg_dc_vals);
   put_dht(&b, 0x10, rjpeg_ac_lum_bits, rjpeg_ac_lum_vals);
   put_dht(&b, 0x01, rjpeg_dc_chr_bits, rjpeg_dc_vals);
   put_dht(&b, 0x11, rjpeg_ac_chr_bits, rjpeg_ac_chr_vals);

   put_marker_len(&b, 0xDA, 6 + 2 * 3);
   put_byte(&b, 3);
   put_byte(&b, 1); put_byte(&b, 0x00);
   put_byte(&b, 2); put_byte(&b, 0x11);
   put_byte(&b, 3); put_byte(&b, 0x11);
   put_byte(&b, 0);
   put_byte(&b, 63);
   put_byte(&b, 0);
   if (b.overflow)
      return 0;

   for (my = 0; my < height; my += mcu_h)
   {
      for (mx = 0; mx < width; mx += 16)
      {
         unsigned by;
         for (by = 0; by < vs; by++)
         {
            code_block(&b, &pl[0], mx,     my + by * 8, e, 0, &pred[0]);
            code_block(&b, &pl[0], mx + 8, my + by * 8, e, 0, &pred[0]);
         }
         code_block(&b, &pl[1], mx / 2, my / vs, e, 1, &pred[1]);
         code_block(&b, &pl[2], mx / 2, my / vs, e, 1, &pred[2]);
      }
      if (b.overflow)
         return 0;
   }
   flush_bits(&b);
   put_byte(&b, 0xFF);
   put_byte(&b, 0xD9);
   return b.overflow ? 0 : b.pos;
}

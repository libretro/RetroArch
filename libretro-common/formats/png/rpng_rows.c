/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rpng_rows.c).
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

/* A PNG a row at a time; see <formats/rpng_rows.h>. */

#include <stdlib.h>
#include <string.h>

#include <encodings/crc32.h>
#include <encodings/deflate.h>
#include <formats/rpng_rows.h>

/* Where the decoder is in the file */
enum
{
   RR_SIG = 0,   /* the 8-byte signature */
   RR_HDR,       /* a chunk's length and type */
   RR_DATA,      /* its data */
   RR_CRC,       /* its CRC */
   RR_END,       /* after IEND */
   RR_FAILED
};

/* What is done with a chunk's data */
enum
{
   RR_KEEP = 0,  /* copied to keep[], read at its end */
   RR_SKIP,      /* checked and dropped */
   RR_IMAGE      /* IDAT: inflated into rows */
};

#define RR_KEEP_MAX 768   /* the biggest chunk read: PLTE */
#define RR_DIM_MAX  0x7fffffffu

#define RR_TYPE(a, b, c, d) \
   (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))

struct rpng_rows
{
   struct rpng_rows_info info;

   int      stage;
   int      mode;              /* RR_KEEP, RR_SKIP or RR_IMAGE */
   uint32_t type;              /* the chunk's */
   uint32_t left;              /* bytes of it to go */
   uint32_t crc;               /* of its type and data so far */
   uint32_t keep_len;
   uint8_t  head[8];           /* signature, chunk header or CRC */
   unsigned head_fill;
   uint8_t  keep[RR_KEEP_MAX];

   int      have_ihdr;
   int      have_plte;
   unsigned colour_n;          /* colour chunks taken */
   int      header_sent;
   int      idat_done;         /* a chunk other than IDAT came after IDATs */
   int      image_done;        /* every row is out */
   int      inflate_done;

   /* the image data */
   void    *inflate;
   uint8_t *rows;              /* two rows, each with its filter byte */
   size_t   rows_cap;
   uint8_t *cur;               /* the row being inflated */
   uint8_t *prev;              /* the row before it in this pass */
   size_t   fill;              /* bytes of cur so far */
   size_t   rowbytes;          /* of this pass's rows, without the filter byte */
   unsigned bpp;               /* bytes a pixel, at least 1, for the filters */
   unsigned pass;              /* 0..6 for Adam7, 0 otherwise */
   uint32_t pass_width;
   uint32_t pass_height;
   uint32_t pass_row;          /* rows of the pass out so far */
   int      pass_end;          /* the last row out ended its pass */
};

static const uint8_t rr_x0[7] = { 0, 4, 0, 2, 0, 1, 0 };
static const uint8_t rr_y0[7] = { 0, 0, 4, 0, 2, 0, 1 };
static const uint8_t rr_dx[7] = { 8, 8, 4, 4, 2, 2, 1 };
static const uint8_t rr_dy[7] = { 8, 8, 8, 4, 4, 2, 2 };

static uint32_t rr_be32(const uint8_t *p)
{
   return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
        | ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

static unsigned rr_bits(const struct rpng_rows_info *info)
{
   return (unsigned)info->depth * info->channels;
}

size_t rpng_rows_rowbytes(const struct rpng_rows_info *info, uint32_t width)
{
   return ((size_t)width * rr_bits(info) + 7) / 8;
}

rpng_rows_t *rpng_rows_new(void)
{
   return (rpng_rows_t*)calloc(1, sizeof(rpng_rows_t));
}

void rpng_rows_free(rpng_rows_t *z)
{
   if (!z)
      return;
   rinflate_free(z->inflate);
   free(z->rows);
   free(z);
}

void rpng_rows_reset(rpng_rows_t *z)
{
   void    *inflate  = z->inflate;
   uint8_t *rows     = z->rows;
   size_t   rows_cap = z->rows_cap;
   memset(z, 0, sizeof(*z));
   z->inflate  = inflate;
   z->rows     = rows;
   z->rows_cap = rows_cap;
}

const struct rpng_rows_info *rpng_rows_info(const rpng_rows_t *z)
{
   return &z->info;
}

/* The next pass with any pixels, from z->pass on; returns 0 when there is
 * none left */
static int rr_pass_begin(rpng_rows_t *z)
{
   const struct rpng_rows_info *info = &z->info;
   for (; z->pass < 7; z->pass++)
   {
      if (!info->interlace)
      {
         z->pass_width  = info->width;
         z->pass_height = info->height;
      }
      else
      {
         unsigned p = z->pass;
         z->pass_width  = info->width  > rr_x0[p]
            ? (info->width  - rr_x0[p] + rr_dx[p] - 1) / rr_dx[p] : 0;
         z->pass_height = info->height > rr_y0[p]
            ? (info->height - rr_y0[p] + rr_dy[p] - 1) / rr_dy[p] : 0;
      }
      if (z->pass_width && z->pass_height)
      {
         z->rowbytes = rpng_rows_rowbytes(info, z->pass_width);
         z->pass_row = 0;
         z->fill     = 0;
         /* the row above the first is all zeroes */
         memset(z->prev, 0, z->rowbytes + 1);
         return 1;
      }
      if (!info->interlace)
         break;
   }
   return 0;
}

static int rr_paeth(int a, int b, int c)
{
   int p  = a + b - c;
   int pa = p > a ? p - a : a - p;
   int pb = p > b ? p - b : b - p;
   int pc = p > c ? p - c : c - p;
   if (pa <= pb && pa <= pc)
      return a;
   return pb <= pc ? b : c;
}

/* Undoes cur's filter against prev; returns 0 for a filter that is not one */
static int rr_unfilter(rpng_rows_t *z)
{
   uint8_t       *r   = z->cur + 1;
   const uint8_t *p   = z->prev + 1;
   size_t         n   = z->rowbytes;
   size_t         bpp = z->bpp;
   size_t         i;

   switch (z->cur[0])
   {
      case 0:
         break;
      case 1:
         for (i = bpp; i < n; i++)
            r[i] = (uint8_t)(r[i] + r[i - bpp]);
         break;
      case 2:
         for (i = 0; i < n; i++)
            r[i] = (uint8_t)(r[i] + p[i]);
         break;
      case 3:
         for (i = 0; i < bpp && i < n; i++)
            r[i] = (uint8_t)(r[i] + (p[i] >> 1));
         for (; i < n; i++)
            r[i] = (uint8_t)(r[i] + ((r[i - bpp] + p[i]) >> 1));
         break;
      case 4:
         for (i = 0; i < bpp && i < n; i++)
            r[i] = (uint8_t)(r[i] + p[i]);
         for (; i < n; i++)
            r[i] = (uint8_t)(r[i] + rr_paeth(r[i - bpp], p[i], p[i - bpp]));
         break;
      default:
         return 0;
   }
   return 1;
}

/* A colour chunk, from keep[]: dropped where libpng drops it */
static void rr_colour(rpng_rows_t *z)
{
   struct rpng_rows_info *info = &z->info;
   const uint8_t         *k    = z->keep;
   uint32_t               len  = z->keep_len;
   uint32_t               bit;
   uint8_t               *at;
   unsigned               i;

   switch (z->type)
   {
      case RR_TYPE('g', 'A', 'M', 'A'):
         if (len != 4 || (k[0] & 0x80u))
            return;
         bit        = RPNG_ROWS_GAMA;
         at         = &info->gama_at;
         if (info->chunks & bit)
            return;
         info->gama = rr_be32(k);
         break;
      case RR_TYPE('c', 'H', 'R', 'M'):
         if (len != 32)
            return;
         for (i = 0; i < 8; i++)
            if (rr_be32(k + i * 4) == 0x80000000u)
               return; /* no 32-bit value is its negative */
         bit = RPNG_ROWS_CHRM;
         at  = &info->chrm_at;
         if (info->chunks & bit)
            return;
         for (i = 0; i < 8; i++)
         {
            uint32_t v   = rr_be32(k + i * 4);
            info->chrm[i] = (v & 0x80000000u)
               ? -(int32_t)(~v + 1u) : (int32_t)v;
         }
         break;
      case RR_TYPE('s', 'R', 'G', 'B'):
         if (len != 1 || k[0] > 3)
            return;
         bit        = RPNG_ROWS_SRGB;
         at         = &info->srgb_at;
         if (info->chunks & bit)
            return;
         info->srgb = k[0];
         break;
      case RR_TYPE('s', 'B', 'I', 'T'):
      {
         unsigned n   = info->color_type == 3 ? 3u : info->channels;
         unsigned max = info->color_type == 3 ? 8u : info->depth;
         if (len != n)
            return;
         for (i = 0; i < n; i++)
            if (!k[i] || k[i] > max)
               return;
         bit = RPNG_ROWS_SBIT;
         at  = &info->sbit_at;
         if (info->chunks & bit)
            return;
         memcpy(info->sbit, k, n);
         break;
      }
      case RR_TYPE('c', 'I', 'C', 'P'):
         if (len != 4)
            return;
         bit = RPNG_ROWS_CICP;
         at  = &info->cicp_at;
         if (info->chunks & bit)
            return;
         memcpy(info->cicp, k, 4);
         break;
      default: /* mDCV */
         if (len != 24)
            return;
         bit = RPNG_ROWS_MDCV;
         at  = &info->mdcv_at;
         if (info->chunks & bit)
            return;
         for (i = 0; i < 8; i++)
            info->mdcv[i] = (uint16_t)((k[i * 2] << 8) | k[i * 2 + 1]);
         info->mdcv_luminance[0] = rr_be32(k + 16);
         info->mdcv_luminance[1] = rr_be32(k + 20);
         break;
   }
   info->chunks |= bit;
   *at           = (uint8_t)++z->colour_n;
}

/* IHDR, from keep[] */
static int rr_ihdr(rpng_rows_t *z)
{
   struct rpng_rows_info *info = &z->info;
   const uint8_t         *d    = z->keep;
   unsigned               bits;

   if (z->have_ihdr || z->keep_len != 13)
      return 0;
   info->width      = rr_be32(d);
   info->height     = rr_be32(d + 4);
   info->depth      = d[8];
   info->color_type = d[9];
   info->interlace  = d[12];
   if (!info->width || !info->height || info->width > RR_DIM_MAX
         || info->height > RR_DIM_MAX || d[10] || d[11] || d[12] > 1)
      return 0;
   switch (info->color_type)
   {
      case 0:
         info->channels = 1;
         bits = 1 | 2 | 4 | 8 | 16;
         break;
      case 3:
         info->channels = 1;
         bits = 1 | 2 | 4 | 8;
         break;
      case 2:
         info->channels = 3;
         bits = 8 | 16;
         break;
      case 4:
         info->channels = 2;
         bits = 8 | 16;
         break;
      case 6:
         info->channels = 4;
         bits = 8 | 16;
         break;
      default:
         return 0;
   }
   if ((info->depth & (info->depth - 1)) || !(bits & info->depth))
      return 0;
   /* a row's bytes, and two of them with their filter bytes, must fit */
   if ((size_t)info->width > ((size_t)-1 - 16) / 2 / 8 / rr_bits(info))
      return 0;
   z->bpp       = rr_bits(info) >= 8 ? rr_bits(info) / 8 : 1;
   z->have_ihdr = 1;
   return 1;
}

/* The end of a chunk the decoder kept or checked, its CRC right: what it
 * says goes into info. Returns 0 for a broken critical chunk. */
static int rr_chunk_end(rpng_rows_t *z)
{
   struct rpng_rows_info *info = &z->info;
   uint32_t               bit  = 0;
   int                    before_plte = 0;

   switch (z->type)
   {
      case RR_TYPE('I', 'H', 'D', 'R'):
         return rr_ihdr(z);
      case RR_TYPE('P', 'L', 'T', 'E'):
         if (z->have_plte || z->header_sent || !z->keep_len || z->keep_len % 3
               || z->keep_len / 3 > (1u << (info->color_type == 3 ? info->depth : 8)))
            return info->color_type != 3; /* only an error where it is needed */
         if (info->color_type == 0 || info->color_type == 4)
            return 1; /* a palette on a gray image is ignored */
         memcpy(info->palette, z->keep, z->keep_len);
         info->palette_len = (uint16_t)(z->keep_len / 3);
         z->have_plte      = 1;
         info->chunks     |= RPNG_ROWS_PLTE;
         return 1;
      case RR_TYPE('t', 'R', 'N', 'S'):
         if (z->header_sent || (info->chunks & RPNG_ROWS_TRNS))
            return 1;
         if (info->color_type == 3)
         {
            if (!z->have_plte || !z->keep_len || z->keep_len > info->palette_len)
               return 1;
         }
         else if (z->keep_len != (info->color_type == 0 ? 2u
                  : info->color_type == 2 ? 6u : 0u) || !z->keep_len)
            return 1;
         memcpy(info->trns, z->keep, z->keep_len);
         info->trns_len = (uint16_t)z->keep_len;
         info->chunks  |= RPNG_ROWS_TRNS;
         return 1;
      case RR_TYPE('t', 'E', 'X', 't'):
      case RR_TYPE('z', 'T', 'X', 't'):
      case RR_TYPE('i', 'T', 'X', 't'):
         info->text_chunks++;
         return 1;
      case RR_TYPE('g', 'A', 'M', 'A'):
      case RR_TYPE('c', 'H', 'R', 'M'):
      case RR_TYPE('s', 'R', 'G', 'B'):
      case RR_TYPE('s', 'B', 'I', 'T'):
      case RR_TYPE('c', 'I', 'C', 'P'):
      case RR_TYPE('m', 'D', 'C', 'V'):
         /* before PLTE and the image data */
         if (!z->header_sent && !z->have_plte)
            rr_colour(z);
         return 1;
      case RR_TYPE('i', 'C', 'C', 'P'): bit = RPNG_ROWS_ICCP; before_plte = 1; break;
      case RR_TYPE('b', 'K', 'G', 'D'): bit = RPNG_ROWS_BKGD; break;
      case RR_TYPE('h', 'I', 'S', 'T'):
         if (!z->have_plte)
            return 1;
         bit = RPNG_ROWS_HIST;
         break;
      case RR_TYPE('p', 'H', 'Y', 's'): bit = RPNG_ROWS_PHYS; break;
      case RR_TYPE('o', 'F', 'F', 's'): bit = RPNG_ROWS_OFFS; break;
      case RR_TYPE('p', 'C', 'A', 'L'): bit = RPNG_ROWS_PCAL; break;
      case RR_TYPE('s', 'C', 'A', 'L'): bit = RPNG_ROWS_SCAL; break;
      case RR_TYPE('s', 'P', 'L', 'T'): bit = RPNG_ROWS_SPLT; break;
      case RR_TYPE('t', 'I', 'M', 'E'):
         info->chunks |= RPNG_ROWS_TIME;
         return 1;
      /* known, not reported */
      case RR_TYPE('e', 'X', 'I', 'f'):
      case RR_TYPE('c', 'L', 'L', 'I'):
         return 1;
      default:
         /* an unknown chunk; critical ones fail at their header */
         if (z->type & 0x20u)
            info->unknown_chunks++;
         return 1;
   }
   /* ancillary chunks with a place: before PLTE, or before IDAT */
   if (z->header_sent || (before_plte && z->have_plte))
      return 1;
   info->chunks |= bit;
   return 1;
}

/* A chunk header in head[]; sets up how its data is taken */
static int rr_chunk_begin(rpng_rows_t *z)
{
   uint32_t len  = rr_be32(z->head);
   uint32_t type = rr_be32(z->head + 4);
   unsigned i;

   if (len > RR_DIM_MAX)
      return 0;
   for (i = 4; i < 8; i++)
   {
      uint8_t c = z->head[i];
      if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
         return 0;
   }
   z->type     = type;
   z->left     = len;
   z->keep_len = 0;
   z->crc      = encoding_crc32(0, z->head + 4, 4);

   if (!z->have_ihdr && type != RR_TYPE('I', 'H', 'D', 'R'))
      return 0;

   if (type == RR_TYPE('I', 'D', 'A', 'T'))
   {
      if (z->idat_done)
         return 0; /* IDATs are consecutive */
      z->mode = RR_IMAGE;
      return 1;
   }
   if (z->header_sent)
      z->idat_done = 1;

   switch (type)
   {
      case RR_TYPE('I', 'H', 'D', 'R'):
      case RR_TYPE('P', 'L', 'T', 'E'):
      case RR_TYPE('t', 'R', 'N', 'S'):
         z->mode = len <= RR_KEEP_MAX ? RR_KEEP : RR_SKIP;
         if (z->mode == RR_SKIP && type != RR_TYPE('t', 'R', 'N', 'S'))
            return 0;
         return 1;
      /* the colour chunks: one too long to keep is the wrong length */
      case RR_TYPE('g', 'A', 'M', 'A'):
      case RR_TYPE('c', 'H', 'R', 'M'):
      case RR_TYPE('s', 'R', 'G', 'B'):
      case RR_TYPE('s', 'B', 'I', 'T'):
      case RR_TYPE('c', 'I', 'C', 'P'):
      case RR_TYPE('m', 'D', 'C', 'V'):
         z->mode = len <= RR_KEEP_MAX ? RR_KEEP : RR_SKIP;
         return 1;
      case RR_TYPE('I', 'E', 'N', 'D'):
         z->mode = RR_SKIP;
         return z->header_sent && !len;
      default:
         /* an unknown critical chunk cannot be skipped */
         if (!(type & 0x20000000u))
            return 0;
         z->mode = RR_SKIP;
         return 1;
   }
}

/* Image setup at the first IDAT */
static int rr_image_begin(rpng_rows_t *z)
{
   size_t need = 2 * (rpng_rows_rowbytes(&z->info, z->info.width) + 1);

   if (z->info.color_type == 3 && !z->have_plte)
      return 0;
   if (need > z->rows_cap)
   {
      uint8_t *rows = (uint8_t*)malloc(need);
      if (!rows)
         return 0;
      free(z->rows);
      z->rows     = rows;
      z->rows_cap = need;
   }
   z->cur  = z->rows;
   z->prev = z->rows + need / 2;
   if (z->inflate)
      rinflate_reset(z->inflate, 15);
   else if (!(z->inflate = rinflate_new(15)))
      return 0;
   z->pass = 0;
   if (!rr_pass_begin(z))
      return 0;
   z->info.chunks |= RPNG_ROWS_IDAT;
   z->header_sent  = 1;
   return 1;
}

int rpng_rows_step(rpng_rows_t *z, const uint8_t *in, size_t len,
      size_t *consumed, struct rpng_rows_row *row)
{
   static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
   size_t at = 0;
   int    ev = RPNG_ROWS_NEXT;

   if (z->pass_end)
   {
      z->pass_end = 0;
      z->pass++;
      if (!z->info.interlace || !rr_pass_begin(z))
         z->image_done = 1;
   }

   for (;;)
   {
      if (z->stage == RR_FAILED)
      {
         ev = RPNG_ROWS_ERROR;
         break;
      }
      if (z->stage == RR_END)
      {
         ev = RPNG_ROWS_END;
         break;
      }

      if (z->stage == RR_SIG || z->stage == RR_HDR || z->stage == RR_CRC)
      {
         unsigned want = z->stage == RR_CRC ? 4 : 8;
         while (z->head_fill < want && at < len)
            z->head[z->head_fill++] = in[at++];
         if (z->head_fill < want)
            break;
         z->head_fill = 0;

         if (z->stage == RR_SIG)
         {
            if (memcmp(z->head, sig, 8))
               goto fail;
            z->stage = RR_HDR;
            continue;
         }
         if (z->stage == RR_CRC)
         {
            int crc_ok = rr_be32(z->head) == z->crc;
            z->stage   = RR_HDR;
            /* critical chunks must be right; ancillary ones are dropped */
            if (!crc_ok && !(z->type & 0x20000000u))
               goto fail;
            if (z->type == RR_TYPE('I', 'D', 'A', 'T'))
               continue;
            if (z->type == RR_TYPE('I', 'E', 'N', 'D'))
            {
               if (!z->image_done)
                  goto fail;
               z->stage = RR_END;
               continue;
            }
            if (crc_ok && !rr_chunk_end(z))
               goto fail;
            continue;
         }
         /* a chunk header */
         if (!rr_chunk_begin(z))
            goto fail;
         z->stage = RR_DATA;
         if (z->mode == RR_IMAGE && !z->header_sent)
         {
            if (!rr_image_begin(z))
               goto fail;
            ev = RPNG_ROWS_HEADER;
            break;
         }
         continue;
      }

      /* RR_DATA. Image rows first: inflate can hold a row's worth of
       * output for input it has taken already, also at a chunk's end */
      if (z->mode == RR_IMAGE && !z->image_done && !z->inflate_done)
      {
         size_t n     = len - at < z->left ? len - at : z->left;
         size_t taken = 0, wrote = 0;
         int    r;

         rinflate_set_in(z->inflate, in + at, n);
         rinflate_set_out(z->inflate, z->cur + z->fill,
               z->rowbytes + 1 - z->fill);
         r = rinflate_process(z->inflate, &taken, &wrote);
         z->crc   = encoding_crc32(z->crc, in + at, taken);
         at      += taken;
         z->left -= (uint32_t)taken;
         z->fill += wrote;
         if (r == RDEFLATE_PROCESS_ERROR)
            goto fail;
         if (r == RDEFLATE_PROCESS_END)
            z->inflate_done = 1;

         if (z->fill == z->rowbytes + 1)
         {
            uint8_t *t;
            if (!rr_unfilter(z))
               goto fail;
            row->data  = z->cur + 1;
            row->width = z->pass_width;
            if (z->info.interlace)
            {
               row->y    = rr_y0[z->pass] + z->pass_row * rr_dy[z->pass];
               row->x0   = rr_x0[z->pass];
               row->dx   = rr_dx[z->pass];
               row->pass = (uint8_t)(z->pass + 1);
            }
            else
            {
               row->y    = z->pass_row;
               row->x0   = 0;
               row->dx   = 1;
               row->pass = 0;
            }
            t       = z->prev;
            z->prev = z->cur;
            z->cur  = t;
            z->fill = 0;
            /* the next pass starts in the next call: this row is in prev
             * until then */
            if (++z->pass_row == z->pass_height)
               z->pass_end = 1;
            ev = RPNG_ROWS_ROW;
            break;
         }
         if (z->inflate_done)
            goto fail; /* the stream ended before the image did */
         if (taken || wrote)
            continue;
         /* nothing more without more input */
      }

      if (!z->left)
      {
         z->stage = RR_CRC;
         continue;
      }
      if (at >= len)
         break;

      if (z->mode != RR_IMAGE)
      {
         size_t n = len - at < z->left ? len - at : z->left;
         z->crc   = encoding_crc32(z->crc, in + at, n);
         if (z->mode == RR_KEEP)
         {
            memcpy(z->keep + z->keep_len, in + at, n);
            z->keep_len += (uint32_t)n;
         }
         at      += n;
         z->left -= (uint32_t)n;
         continue;
      }

      /* image data past the last row: the zlib trailer, or data libpng
       * warns about and ignores */
      {
         size_t n     = len - at < z->left ? len - at : z->left;
         size_t taken = 0, wrote = 0;
         if (!z->inflate_done)
         {
            uint8_t sink[64];
            int     r;
            rinflate_set_in(z->inflate, in + at, n);
            rinflate_set_out(z->inflate, sink, sizeof(sink));
            r = rinflate_process(z->inflate, &taken, &wrote);
            if (r == RDEFLATE_PROCESS_END || r == RDEFLATE_PROCESS_ERROR)
               z->inflate_done = 1;
            else if (!taken && !wrote)
               taken = n; /* nothing it would take */
         }
         else
            taken = n;
         z->crc   = encoding_crc32(z->crc, in + at, taken);
         at      += taken;
         z->left -= (uint32_t)taken;
      }
   }

   if (consumed)
      *consumed = at;
   return ev;

fail:
   z->stage = RR_FAILED;
   if (consumed)
      *consumed = at;
   return RPNG_ROWS_ERROR;
}

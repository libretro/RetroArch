#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_inline.h>
#include <formats/image.h>

#define RJPEG_DECODE_SOF         0xC0
#define RJPEG_DECODE_DHT         0xC4
#define RJPEG_DECODE_DQT         0xDB
#define RJPEG_DECODE_DRI         0xDD
#define RJPEG_DECODE_SCAN        0xDA
#define RJPEG_DECODE_SKIP_MARKER 0xFE

#define CF(x) rjpeg_clip(((x) + 64) >> 7)
#define JPEG_DECODER_THROW(ctx, e) do { ctx->error = e; return; } while (0)

enum rjpeg_decode_result
{
   RJPEG_OK = 0,
   RJPEG_NOT_A_FILE,
   RJPEG_UNSUPPORTED,
   RJPEG_OOM,
   RJPEG_INTERNAL_ERROR,
   RJPEG_SYNTAX_ERROR,
   RJPEG_INTERNAL_FINISHED
};

enum
{
   CF4A = (-9),
   CF4B = (111),
   CF4C = (29),
   CF4D = (-3),
   CF3A = (28),
   CF3B = (109),
   CF3C = (-9),
   CF3X = (104),
   CF3Y = (27),
   CF3Z = (-3),
   CF2A = (139),
   CF2B = (-11)
};

enum
{
   W1 = 2841,
   W2 = 2676,
   W3 = 2408,
   W5 = 1609,
   W6 = 1108,
   W7 = 565
};

struct rjpeg_vlc_code
{
   uint8_t bits;
   uint8_t code;
};

struct rjpeg_component
{
   int cid;
   int ssx, ssy;
   int width, height;
   int stride;
   int qtsel;
   int actabsel;
   int dctabsel;
   int dcpred;
   uint8_t *pixels;
};

struct rjpeg_data
{
   enum rjpeg_decode_result error;
   const uint8_t *pos;
   int size;
   int length;
   int width, height;
   int mbwidth;
   int mbheight;
   int mbsizex;
   int mbsizey;
   int ncomp;
   struct rjpeg_component comp[3];
   int qtused;
   int qtavail;
   uint8_t qtab[4][64];
   struct rjpeg_vlc_code vlctab[4][65536];
   int buf, bufbits;
   int block[64];
   int rstinterval;
   uint8_t *rgb;
   char ZZ[64];
};

static INLINE uint8_t rjpeg_clip(const int x)
{
   if (x < 0)
      return 0;
   return ((x > 0xFF) ? 0xFF : (unsigned char) x);
}

static void rjpeg_skip(struct rjpeg_data *ctx, int count)
{
   ctx->pos    += count;
   ctx->size   -= count;
   ctx->length -= count;
   if (ctx->size < 0)
      ctx->error = RJPEG_SYNTAX_ERROR;
}

static INLINE uint16_t rjpeg_decode_16(const uint8_t *pos)
{
   return (pos[0] << 8) | pos[1];
}

static INLINE void rjpeg_decode_length(struct rjpeg_data *ctx)
{
   if (ctx->size < 2)
      JPEG_DECODER_THROW(ctx, RJPEG_SYNTAX_ERROR);
   ctx->length = rjpeg_decode_16(ctx->pos);
   if (ctx->length > ctx->size)
      JPEG_DECODER_THROW(ctx, RJPEG_SYNTAX_ERROR);
   rjpeg_skip(ctx, 2);
}

static void rjpeg_decode_dqt(struct rjpeg_data *ctx)
{
   unsigned char *t = NULL;

   rjpeg_decode_length(ctx);

   while (ctx->length >= 65)
   {
      int i = ctx->pos[0];
      if (i & 0xFC)
         JPEG_DECODER_THROW(ctx, RJPEG_SYNTAX_ERROR);
      ctx->qtavail |= 1 << i;
      t = &ctx->qtab[i][0];
      for (i = 0;  i < 64;  ++i)
         t[i] = ctx->pos[i + 1];
      rjpeg_skip(ctx, 65);
   }

   if (ctx->length)
      JPEG_DECODER_THROW(ctx, RJPEG_SYNTAX_ERROR);
}

static void rjpeg_decode_dri(struct rjpeg_data *ctx)
{
   rjpeg_decode_length(ctx);
   if (ctx->length < 2)
      JPEG_DECODER_THROW(ctx, RJPEG_SYNTAX_ERROR);
   ctx->rstinterval = rjpeg_decode_16(ctx->pos);
   rjpeg_skip(ctx, ctx->length);
}

static void rjpeg_decode_dht(struct rjpeg_data *ctx)
{
   unsigned char counts[16];
   struct rjpeg_vlc_code *vlc = NULL;

   rjpeg_decode_length(ctx);

   while (ctx->length >= 17)
   {
      int codelen;
      int spread = 65536;
      int remain = 65536;
      int i      = ctx->pos[0];

      if (i & 0xEC)
         JPEG_DECODER_THROW(ctx, RJPEG_SYNTAX_ERROR);

      if (i & 0x02)
         JPEG_DECODER_THROW(ctx, RJPEG_UNSUPPORTED);

      i = (i | (i >> 3)) & 3;  /* combined DC/AC + tableid value */
      for (codelen = 1;  codelen <= 16;  ++codelen)
         counts[codelen - 1] = ctx->pos[codelen];
      rjpeg_skip(ctx, 17);
      vlc = &ctx->vlctab[i][0];

      for (codelen = 1;  codelen <= 16;  ++codelen)
      {
         int currcnt;

         spread >>= 1;
         currcnt = counts[codelen - 1];
         if (!currcnt)
            continue;

         if (ctx->length < currcnt)
            JPEG_DECODER_THROW(ctx, RJPEG_SYNTAX_ERROR);
         remain -= currcnt << (16 - codelen);

         if (remain < 0)
            JPEG_DECODER_THROW(ctx, RJPEG_SYNTAX_ERROR);

         for (i = 0;  i < currcnt;  ++i)
         {
            int j;
            register unsigned char code = ctx->pos[i];

            for (j = spread;  j;  --j)
            {
               vlc->bits = (unsigned char) codelen;
               vlc->code = code;
               ++vlc;
            }
         }
         rjpeg_skip(ctx, currcnt);
      }

      while (remain--)
      {
         vlc->bits = 0;
         ++vlc;
      }
   }

   if (ctx->length)
      JPEG_DECODER_THROW(ctx, RJPEG_SYNTAX_ERROR);
}

static int rjpeg_show_bits(struct rjpeg_data *ctx, int bits)
{
   unsigned char newbyte;
   if (!bits)
      return 0;

   while (ctx->bufbits < bits)
   {
      if (ctx->size <= 0)
      {
         ctx->buf      = (ctx->buf << 8) | 0xFF;
         ctx->bufbits += 8;
         continue;
      }

      newbyte       = *ctx->pos++;
      ctx->size--;
      ctx->bufbits += 8;
      ctx->buf      = (ctx->buf << 8) | newbyte;

      if (newbyte == 0xFF)
      {
         if (ctx->size)
         {
            unsigned char marker = *ctx->pos++;
            ctx->size--;
            switch (marker)
            {
               case 0:
                  break;
               case 0xD9:
                  ctx->size = 0;
                  break;
               default:
                  if ((marker & 0xF8) != 0xD0)
                     ctx->error = RJPEG_SYNTAX_ERROR;
                  else
                  {
                     ctx->buf      = (ctx->buf << 8) | marker;
                     ctx->bufbits += 8;
                  }
            }
         } else
            ctx->error = RJPEG_SYNTAX_ERROR;
      }
   }
   return (ctx->buf >> (ctx->bufbits - bits)) & ((1 << bits) - 1);
}

static void rjpeg_skip_bits(struct rjpeg_data *ctx, int bits)
{
   if (ctx->bufbits < bits)
      rjpeg_show_bits(ctx, bits);
   ctx->bufbits -= bits;
}

static int rjpeg_get_bits(struct rjpeg_data *ctx, int bits)
{
   int res = rjpeg_show_bits(ctx, bits);
   rjpeg_skip_bits(ctx, bits);
   return res;
}

static int rjpeg_get_vlc(struct rjpeg_data *ctx, 
      struct rjpeg_vlc_code *vlc, unsigned char* code)
{
   int value = rjpeg_show_bits(ctx, 16);
   int bits  = vlc[value].bits;

   if (!bits)
   {
      ctx->error = RJPEG_SYNTAX_ERROR;
      return 0;
   }

   rjpeg_skip_bits(ctx, bits);
   value = vlc[value].code;
   if (code)
      *code = (unsigned char) value;
   bits = value & 15;
   if (!bits)
      return 0;
   value = rjpeg_get_bits(ctx, bits);
   if (value < (1 << (bits - 1)))
      value += ((-1) << bits) + 1;
   return value;
}

static void rjpeg_row_idct(int* blk)
{
   int x0, x1, x2, x3, x4, x5, x6, x7, x8;
   if (!((x1 = blk[4] << 11)
            | (x2 = blk[6])
            | (x3 = blk[2])
            | (x4 = blk[1])
            | (x5 = blk[7])
            | (x6 = blk[5])
            | (x7 = blk[3])))
   {
      unsigned i;
      int val = blk[0] << 3;

      for (i = 0; i < 8; i++)
         blk[i] = val;
      return;
   }

   x0 = (blk[0] << 11) + 128;
   x8 = W7 * (x4 + x5);
   x4 = x8 + (W1 - W7) * x4;
   x5 = x8 - (W1 + W7) * x5;
   x8 = W3 * (x6 + x7);
   x6 = x8 - (W3 - W5) * x6;
   x7 = x8 - (W3 + W5) * x7;
   x8 = x0 + x1;
   x0 -= x1;
   x1 = W6 * (x3 + x2);
   x2 = x1 - (W2 + W6) * x2;
   x3 = x1 + (W2 - W6) * x3;
   x1 = x4 + x6;
   x4 -= x6;
   x6 = x5 + x7;
   x5 -= x7;
   x7 = x8 + x3;
   x8 -= x3;
   x3 = x0 + x2;
   x0 -= x2;
   x2 = (181 * (x4 + x5) + 128) >> 8;
   x4 = (181 * (x4 - x5) + 128) >> 8;
   blk[0] = (x7 + x1) >> 8;
   blk[1] = (x3 + x2) >> 8;
   blk[2] = (x0 + x4) >> 8;
   blk[3] = (x8 + x6) >> 8;
   blk[4] = (x8 - x6) >> 8;
   blk[5] = (x0 - x4) >> 8;
   blk[6] = (x3 - x2) >> 8;
   blk[7] = (x7 - x1) >> 8;
}

static void rjpeg_col_idct(const int* blk, unsigned char *out, int stride)
{
   int x0, x1, x2, x3, x4, x5, x6, x7, x8;
   if (!((x1 = blk[8*4] << 8)
            | (x2 = blk[8*6])
            | (x3 = blk[8*2])
            | (x4 = blk[8*1])
            | (x5 = blk[8*7])
            | (x6 = blk[8*5])
            | (x7 = blk[8*3])))
   {
      x1 = rjpeg_clip(((blk[0] + 32) >> 6) + 128);
      for (x0 = 8;  x0;  --x0)
      {
         *out = (unsigned char) x1;
         out += stride;
      }
      return;
   }
   x0 = (blk[0] << 8) + 8192;
   x8 = W7 * (x4 + x5) + 4;
   x4 = (x8 + (W1 - W7) * x4) >> 3;
   x5 = (x8 - (W1 + W7) * x5) >> 3;
   x8 = W3 * (x6 + x7) + 4;
   x6 = (x8 - (W3 - W5) * x6) >> 3;
   x7 = (x8 - (W3 + W5) * x7) >> 3;
   x8 = x0 + x1;
   x0 -= x1;
   x1 = W6 * (x3 + x2) + 4;
   x2 = (x1 - (W2 + W6) * x2) >> 3;
   x3 = (x1 + (W2 - W6) * x3) >> 3;
   x1 = x4 + x6;
   x4 -= x6;
   x6 = x5 + x7;
   x5 -= x7;
   x7 = x8 + x3;
   x8 -= x3;
   x3 = x0 + x2;
   x0 -= x2;
   x2 = (181 * (x4 + x5) + 128) >> 8;
   x4 = (181 * (x4 - x5) + 128) >> 8;
   *out = rjpeg_clip(((x7 + x1) >> 14) + 128);
   out += stride;
   *out = rjpeg_clip(((x3 + x2) >> 14) + 128);
   out += stride;
   *out = rjpeg_clip(((x0 + x4) >> 14) + 128);
   out += stride;
   *out = rjpeg_clip(((x8 + x6) >> 14) + 128);
   out += stride;
   *out = rjpeg_clip(((x8 - x6) >> 14) + 128);
   out += stride;
   *out = rjpeg_clip(((x0 - x4) >> 14) + 128);
   out += stride;
   *out = rjpeg_clip(((x3 - x2) >> 14) + 128);
   out += stride;
   *out = rjpeg_clip(((x7 - x1) >> 14) + 128);
}

static INLINE void rjpeg_decode_block(
      struct rjpeg_data *ctx,
      struct rjpeg_component *c,
      unsigned char* out)
{
   unsigned char code = 0;
   int coef = 0;

   memset(ctx->block, 0, sizeof(ctx->block));

   c->dcpred    += rjpeg_get_vlc(ctx, &ctx->vlctab[c->dctabsel][0], NULL);
   ctx->block[0]  = (c->dcpred) * ctx->qtab[c->qtsel][0];

   do
   {
      int value = rjpeg_get_vlc(ctx, &ctx->vlctab[c->actabsel][0], &code);

      if (!code)
         break;  /* EOB */

      if (!(code & 0x0F) && (code != 0xF0))
         JPEG_DECODER_THROW(ctx, RJPEG_SYNTAX_ERROR);
      coef += (code >> 4) + 1;
      if (coef > 63)
         JPEG_DECODER_THROW(ctx, RJPEG_SYNTAX_ERROR);
      ctx->block[(int) ctx->ZZ[coef]] = value * ctx->qtab[c->qtsel][coef];
   } while (coef < 63);

   for (coef = 0;  coef < 64;  coef += 8)
      rjpeg_row_idct(&ctx->block[coef]);

   for (coef = 0;  coef < 8;  ++coef)
      rjpeg_col_idct(&ctx->block[coef], &out[coef], c->stride);
}


static INLINE void rjpeg_byte_align(struct rjpeg_data *ctx)
{
   ctx->bufbits &= 0xF8;
}

static INLINE void rjpeg_skip_marker(struct rjpeg_data *ctx)
{
   rjpeg_decode_length(ctx);
   rjpeg_skip(ctx, ctx->length);
}

static void rjpeg_decode_sof(struct rjpeg_data *ctx)
{
   int i;
   int ssxmax = 0;
   int ssymax = 0;
   struct rjpeg_component *c = NULL;

   rjpeg_decode_length(ctx);

   if (ctx->length < 9)
      JPEG_DECODER_THROW(ctx, RJPEG_SYNTAX_ERROR);
   if (ctx->pos[0] != 8)
      JPEG_DECODER_THROW(ctx, RJPEG_UNSUPPORTED);
   ctx->height = rjpeg_decode_16(ctx->pos+1);
   ctx->width  = rjpeg_decode_16(ctx->pos+3);
   ctx->ncomp  = ctx->pos[5];
   rjpeg_skip(ctx, 6);

   switch (ctx->ncomp)
   {
      case 1:
      case 3:
         break;
      default:
         JPEG_DECODER_THROW(ctx, RJPEG_UNSUPPORTED);
   }

   if (ctx->length < (ctx->ncomp * 3))
      JPEG_DECODER_THROW(ctx, RJPEG_SYNTAX_ERROR);

   for (i = 0, c = ctx->comp;  i < ctx->ncomp;  ++i, ++c)
   {
      c->cid = ctx->pos[0];
      if (!(c->ssx = ctx->pos[1] >> 4))
         JPEG_DECODER_THROW(ctx, RJPEG_SYNTAX_ERROR);
      if (c->ssx & (c->ssx - 1))
         JPEG_DECODER_THROW(ctx, RJPEG_UNSUPPORTED);  /* non-power of two */
      if (!(c->ssy = ctx->pos[1] & 15))
         JPEG_DECODER_THROW(ctx, RJPEG_SYNTAX_ERROR);
      if (c->ssy & (c->ssy - 1))
         JPEG_DECODER_THROW(ctx, RJPEG_UNSUPPORTED);  /* non-power of two */
      if ((c->qtsel = ctx->pos[2]) & 0xFC)
         JPEG_DECODER_THROW(ctx, RJPEG_SYNTAX_ERROR);
      rjpeg_skip(ctx, 3);
      ctx->qtused |= 1 << c->qtsel;
      if (c->ssx > ssxmax) ssxmax = c->ssx;
      if (c->ssy > ssymax) ssymax = c->ssy;
   }
   ctx->mbsizex = ssxmax << 3;
   ctx->mbsizey = ssymax << 3;
   ctx->mbwidth = (ctx->width + ctx->mbsizex - 1) / ctx->mbsizex;
   ctx->mbheight = (ctx->height + ctx->mbsizey - 1) / ctx->mbsizey;

   for (i = 0, c = ctx->comp;  i < ctx->ncomp;  ++i, ++c)
   {
      c->width  = (ctx->width * c->ssx + ssxmax - 1) / ssxmax;
      c->stride = (c->width + 7) & 0x7FFFFFF8;
      c->height = (ctx->height * c->ssy + ssymax - 1) / ssymax;
      c->stride = ctx->mbwidth * ctx->mbsizex * c->ssx / ssxmax;
      if (((c->width < 3) && (c->ssx != ssxmax)) || ((c->height < 3) && (c->ssy != ssymax)))
         JPEG_DECODER_THROW(ctx, RJPEG_UNSUPPORTED);
      if (!(c->pixels = (unsigned char*)malloc(c->stride * (ctx->mbheight * ctx->mbsizey * c->ssy / ssymax))))
         JPEG_DECODER_THROW(ctx, RJPEG_OOM);
   }

   if (ctx->ncomp == 3)
   {
      ctx->rgb = (unsigned char*)malloc(ctx->width * ctx->height * ctx->ncomp);
      if (!ctx->rgb)
         JPEG_DECODER_THROW(ctx, RJPEG_OOM);
   }
   rjpeg_skip(ctx, ctx->length);
}

static void rjpeg_decode_scan(struct rjpeg_data *ctx)
{
   int i, mbx, mby, sbx, sby;
   int              rstcount = ctx->rstinterval;
   int               nextrst = 0;
   struct rjpeg_component *c = NULL;

   rjpeg_decode_length(ctx);

   if (ctx->length < (4 + 2 * ctx->ncomp))
      JPEG_DECODER_THROW(ctx, RJPEG_SYNTAX_ERROR);
   if (ctx->pos[0] != ctx->ncomp)
      JPEG_DECODER_THROW(ctx, RJPEG_UNSUPPORTED);
   rjpeg_skip(ctx, 1);
   for (i = 0, c = ctx->comp;  i < ctx->ncomp;  ++i, ++c)
   {
      if (ctx->pos[0] != c->cid)
         JPEG_DECODER_THROW(ctx, RJPEG_SYNTAX_ERROR);
      if (ctx->pos[1] & 0xEE)
         JPEG_DECODER_THROW(ctx, RJPEG_SYNTAX_ERROR);
      c->dctabsel  = ctx->pos[1] >> 4;
      c->actabsel = (ctx->pos[1] & 1) | 2;
      rjpeg_skip(ctx, 2);
   }

   if (ctx->pos[0] || (ctx->pos[1] != 63) || ctx->pos[2])
      JPEG_DECODER_THROW(ctx, RJPEG_UNSUPPORTED);

   rjpeg_skip(ctx, ctx->length);

   for (mby = 0;  mby < ctx->mbheight;  ++mby)
   {
      for (mbx = 0;  mbx < ctx->mbwidth;  ++mbx)
      {
         for (i = 0, c = ctx->comp;  i < ctx->ncomp;  ++i, ++c)
         {
            for (sby = 0;  sby < c->ssy;  ++sby)
            {
               for (sbx = 0;  sbx < c->ssx;  ++sbx)
               {
                  rjpeg_decode_block(ctx, c,
                        &c->pixels[((mby * c->ssy + sby) * c->stride + mbx * c->ssx + sbx) << 3]);
                  if (ctx->error)
                     return;
               }
            }
         }

         if (ctx->rstinterval && !(--rstcount))
         {
            rjpeg_byte_align(ctx);
            i = rjpeg_get_bits(ctx, 16);
            if (((i & 0xFFF8) != 0xFFD0) || ((i & 7) != nextrst))
               JPEG_DECODER_THROW(ctx, RJPEG_SYNTAX_ERROR);
            nextrst  = (nextrst + 1) & 7;
            rstcount = ctx->rstinterval;

            for (i = 0;  i < 3;  ++i)
               ctx->comp[i].dcpred = 0;
         }
      }
   }

   ctx->error = RJPEG_INTERNAL_FINISHED;
}

static void rjpeg_upsample_h(struct rjpeg_data *ctx, struct rjpeg_component *c)
{
   int x, y;
   unsigned char *lin  = NULL;
   unsigned char *lout = NULL;
   const int xmax      = c->width - 3;
   uint8_t *out        = (uint8_t*)malloc((c->width * c->height) << 1);
   if (!out)
      JPEG_DECODER_THROW(ctx, RJPEG_OOM);
   lin = c->pixels;
   lout = out;
   for (y = c->height;  y;  --y)
   {
      lout[0] = CF(CF2A * lin[0] + CF2B * lin[1]);
      lout[1] = CF(CF3X * lin[0] + CF3Y * lin[1] + CF3Z * lin[2]);
      lout[2] = CF(CF3A * lin[0] + CF3B * lin[1] + CF3C * lin[2]);

      for (x = 0;  x < xmax;  ++x)
      {
         lout[(x << 1) + 3] = CF(CF4A * lin[x] + CF4B * lin[x + 1] + CF4C * lin[x + 2] + CF4D * lin[x + 3]);
         lout[(x << 1) + 4] = CF(CF4D * lin[x] + CF4C * lin[x + 1] + CF4B * lin[x + 2] + CF4A * lin[x + 3]);
      }

      lin     += c->stride;
      lout    += c->width << 1;
      lout[-3] = CF(CF3A * lin[-1] + CF3B * lin[-2] + CF3C * lin[-3]);
      lout[-2] = CF(CF3X * lin[-1] + CF3Y * lin[-2] + CF3Z * lin[-3]);
      lout[-1] = CF(CF2A * lin[-1] + CF2B * lin[-2]);
   }
   c->width <<= 1;
   c->stride = c->width;
   free(c->pixels);
   c->pixels = out;
}

static void rjpeg_upsample_v(struct rjpeg_data *ctx, struct rjpeg_component *c)
{
   int x;
   const int         w = c->width, s1 = c->stride, s2 = s1 + s1;
   unsigned char *out = (unsigned char*)malloc((c->width * c->height) << 1);

   for (x = 0;  x < w;  ++x)
   {
      int y;
      unsigned char *cin  = &c->pixels[x];
      unsigned char *cout = &out[x];

      *cout = CF(CF2A * cin[0] + CF2B * cin[s1]);
      cout += w;

      *cout = CF(CF3X * cin[0] + CF3Y * cin[s1] + CF3Z * cin[s2]);
      cout += w;

      *cout = CF(CF3A * cin[0] + CF3B * cin[s1] + CF3C * cin[s2]);
      cout += w;

      cin += s1;
      for (y = c->height - 3;  y;  --y)
      {
         *cout = CF(CF4A * cin[-s1] + CF4B * cin[0] + CF4C * cin[s1] + CF4D * cin[s2]);
         cout += w;
         *cout = CF(CF4D * cin[-s1] + CF4C * cin[0] + CF4B * cin[s1] + CF4A * cin[s2]);
         cout += w;
         cin  += s1;
      }
      cin  += s1;
      *cout = CF(CF3A * cin[0] + CF3B * cin[-s1] + CF3C * cin[-s2]);
      cout += w;
      *cout = CF(CF3X * cin[0] + CF3Y * cin[-s1] + CF3Z * cin[-s2]);
      cout += w;
      *cout = CF(CF2A * cin[0] + CF2B * cin[-s1]);
   }

   c->height <<= 1;
   c->stride   = c->width;

   free(c->pixels);
   c->pixels = out;
}


static void rjpeg_convert(struct rjpeg_data *ctx)
{
   int i;
   struct rjpeg_component *c = NULL;

   for (i = 0, c = ctx->comp;  i < ctx->ncomp;  ++i, ++c)
   {
      while ((c->width < ctx->width) || (c->height < ctx->height))
      {
         if (c->width < ctx->width)
            rjpeg_upsample_h(ctx, c);

         if (ctx->error)
            return;

         if (c->height < ctx->height)
            rjpeg_upsample_v(ctx, c);

         if (ctx->error)
            return;
      }
      if ((c->width < ctx->width) || (c->height < ctx->height))
         JPEG_DECODER_THROW(ctx, RJPEG_INTERNAL_ERROR);
   }

   if (ctx->ncomp == 3)
   {
      /* convert to RGB */
      int x, yy;
      unsigned char *prgb      = ctx->rgb;
      const unsigned char *py  = ctx->comp[0].pixels;
      const unsigned char *pcb = ctx->comp[1].pixels;
      const unsigned char *pcr = ctx->comp[2].pixels;

      for (yy = ctx->height;  yy;  --yy)
      {
         for (x = 0;  x < ctx->width;  ++x)
         {
            register int y  = py[x] << 8;
            register int cb = pcb[x] - 128;
            register int cr = pcr[x] - 128;
            *prgb++         = rjpeg_clip((y            + 359 * cr + 128) >> 8);
            *prgb++         = rjpeg_clip((y -  88 * cb - 183 * cr + 128) >> 8);
            *prgb++         = rjpeg_clip((y + 454 * cb            + 128) >> 8);
         }
         py  += ctx->comp[0].stride;
         pcb += ctx->comp[1].stride;
         pcr += ctx->comp[2].stride;
      }
   }
   else if (ctx->comp[0].width != ctx->comp[0].stride)
   {
      /* grayscale -> only remove stride */
      int y;
      unsigned char *pin  = &ctx->comp[0].pixels[ctx->comp[0].stride];
      unsigned char *pout = &ctx->comp[0].pixels[ctx->comp[0].width];

      for (y = ctx->comp[0].height - 1;  y;  --y)
      {
         memcpy(pout, pin, ctx->comp[0].width);
         pin  += ctx->comp[0].stride;
         pout += ctx->comp[0].width;
      }
      ctx->comp[0].stride = ctx->comp[0].width;
   }
}


enum rjpeg_decode_result rjpeg_decode(
      struct rjpeg_data *ctx,
      const unsigned char* jpeg,
      const int size)
{
   ctx->pos  = (const unsigned char*) jpeg;
   ctx->size = size & 0x7FFFFFFF;

   if (ctx->size < 2)
      return RJPEG_NOT_A_FILE;
   if ((ctx->pos[0] ^ 0xFF) | (ctx->pos[1] ^ 0xD8))
      return RJPEG_NOT_A_FILE;

   rjpeg_skip(ctx, 2);

   while (!ctx->error)
   {
      if ((ctx->size < 2) || (ctx->pos[0] != 0xFF))
         return RJPEG_SYNTAX_ERROR;

      rjpeg_skip(ctx, 2);

      switch (ctx->pos[-1])
      {
         case RJPEG_DECODE_SOF:
            rjpeg_decode_sof(ctx); 
            break;
         case RJPEG_DECODE_DHT:
            rjpeg_decode_dht(ctx);
            break;
         case RJPEG_DECODE_DQT:
            rjpeg_decode_dqt(ctx); 
            break;
         case RJPEG_DECODE_DRI:
            rjpeg_decode_dri(ctx); 
            break;
         case RJPEG_DECODE_SCAN:
            rjpeg_decode_scan(ctx);
            break;
         case RJPEG_DECODE_SKIP_MARKER:
            rjpeg_skip_marker(ctx);
            break;
         default:
            if ((ctx->pos[-1] & 0xF0) != 0xE0)
               return RJPEG_UNSUPPORTED;
            rjpeg_skip_marker(ctx);
            break;
      }
   }
   if (ctx->error != RJPEG_INTERNAL_FINISHED)
      return ctx->error;

   ctx->error = RJPEG_OK;
   rjpeg_convert(ctx);

   return RJPEG_OK;
}

struct rjpeg_data *rjpeg_new(const uint8_t* data, size_t size)
{
    char temp[64] = { 0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18,
        11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28, 35,
        42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45,
        38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63 };
   struct rjpeg_data *ctx = (struct rjpeg_data*)calloc(1, sizeof(*ctx));

   if (!ctx)
      return NULL;

    memcpy(ctx->ZZ, temp, sizeof(ctx->ZZ));
    rjpeg_decode(ctx, data, size);

    return ctx;
}

static void rjpeg_free(struct rjpeg_data *ctx)
{
    int i;

    for (i = 0;  i < 3;  ++i)
        if (ctx->comp[i].pixels)
           free((void*) ctx->comp[i].pixels);
    if (ctx->rgb)
       free((void*)ctx->rgb);
}

bool rjpeg_image_load(uint8_t *buf, void *data, size_t size,
      unsigned a_shift, unsigned r_shift,
      unsigned g_shift, unsigned b_shift)
{
   struct rjpeg_data       *rjpg = rjpeg_new(buf, size);
   struct texture_image *out_img = (struct texture_image*)data;

   if (!rjpg)
      goto error;

   out_img->width   = rjpg->width;
   out_img->height  = rjpg->height;
   out_img->pixels  = (uint32_t*)malloc(rjpg->width * rjpg->height * rjpg->ncomp);

   if (!out_img->pixels)
      goto error;

   if (rjpg->ncomp == 3)
   {
      /* convert to RGB */
      int x, yy;
      uint32_t *prgb      = (uint32_t*)out_img->pixels;
      const unsigned char *py  = rjpg->comp[0].pixels;
      const unsigned char *pcb = rjpg->comp[1].pixels;
      const unsigned char *pcr = rjpg->comp[2].pixels;

      for (yy = rjpg->height;  yy;  --yy)
      {
         for (x = 0;  x < rjpg->width;  ++x)
         {
            register int y  = py[x] << 8;
            register int cb = pcb[x] - 128;
            register int cr = pcr[x] - 128;
            *prgb++         = rjpeg_clip((y            + 359 * cr + 128) >> 8);
            *prgb++         = rjpeg_clip((y -  88 * cb - 183 * cr + 128) >> 8);
            *prgb++         = rjpeg_clip((y + 454 * cb            + 128) >> 8);
         }
         py  += rjpg->comp[0].stride;
         pcb += rjpg->comp[1].stride;
         pcr += rjpg->comp[2].stride;
      }
   }

   rjpeg_free(rjpg);

   return true;

error:
   if (out_img->pixels)
      free(out_img->pixels);

   out_img->pixels = NULL;
   out_img->width = out_img->height = 0;

   if (rjpg)
      rjpeg_free(rjpg);
   return false;
}

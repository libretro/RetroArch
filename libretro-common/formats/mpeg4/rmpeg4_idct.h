/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rmpeg4_idct.h).
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

/* The inverse DCT rmpeg4_video decodes with and rmpeg4_encode
 * reconstructs its references with: one transform, so that what the
 * encoder predicts from is sample for sample what the decoder will have.
 * Static functions, for the two files that include it. */

#ifndef __LIBRETRO_SDK_FORMAT_RMPEG4_IDCT_H
#define __LIBRETRO_SDK_FORMAT_RMPEG4_IDCT_H

#include <stdint.h>

/* --------------------------------------------------------------------- */
/* Inverse DCT: the same integer transform as rmpeg1_video               */
/* --------------------------------------------------------------------- */

#define W1 2841   /* cos(1*pi/16) * sqrt(2) * 2048 */
#define W2 2676
#define W3 2408
#define W5 1609
#define W6 1108
#define W7 565

static void idct_row(const int16_t *in, int *b)
{
   int x0, x1, x2, x3, x4, x5, x6, x7, x8;

   x1 = (int)in[4] * 2048;
   x2 = in[6];
   x3 = in[2];
   x4 = in[1];
   x5 = in[7];
   x6 = in[5];
   x7 = in[3];

   /* All-zero AC row: the DC term alone, replicated. Common enough in real
    * content that skipping the butterflies is worth the branch. */
   if (!(x1 | x2 | x3 | x4 | x5 | x6 | x7))
   {
      int dc = (int)in[0] * 8;
      b[0] = b[1] = b[2] = b[3] = b[4] = b[5] = b[6] = b[7] = dc;
      return;
   }

   x0 = (int)in[0] * 2048 + 128;

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
   x2 = (int)(((int64_t)181 * (x4 + x5) + 128) >> 8);
   x4 = (int)(((int64_t)181 * (x4 - x5) + 128) >> 8);

   b[0] = (x7 + x1) >> 8;
   b[1] = (x3 + x2) >> 8;
   b[2] = (x0 + x4) >> 8;
   b[3] = (x8 + x6) >> 8;
   b[4] = (x8 - x6) >> 8;
   b[5] = (x0 - x4) >> 8;
   b[6] = (x3 - x2) >> 8;
   b[7] = (x7 - x1) >> 8;
}

static void idct_col_store(const int *b, uint8_t *dst, unsigned stride)
{
   int x0, x1, x2, x3, x4, x5, x6, x7, x8;
   int i;
   int out[8];

   x1 = b[8 * 4] * 256;
   x2 = b[8 * 6];
   x3 = b[8 * 2];
   x4 = b[8 * 1];
   x5 = b[8 * 7];
   x6 = b[8 * 5];
   x7 = b[8 * 3];

   if (!(x1 | x2 | x3 | x4 | x5 | x6 | x7))
   {
      int dc = (b[0] + 32) >> 6;
      for (i = 0; i < 8; i++)
         out[i] = dc;
   }
   else
   {
      x0 = b[0] * 256 + 8192;

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
      /* The rotation by 181/256 (i.e. sqrt(2)/2) is the one place where a
       * 32-bit product is not wide enough. By this point in the column pass
       * x4 and x5 have accumulated two stages of gain, so their difference
       * can reach ~1.3e7; multiplied by 181 that is ~2.3e9, past INT32_MAX.
       *
       * Legal MPEG-1 content never gets there -- the dequantiser saturates
       * and real coefficients are nothing like uniformly maximal -- which is
       * why the usual 32-bit formulation survives in practice. An IEEE 1180
       * sweep at L=300 reaches it in roughly one block in ten thousand, and
       * the wrap turns a saturated-white sample into black. Widen the two
       * products rather than hope. */
      x2 = (int)(((int64_t)181 * (x4 + x5) + 128) >> 8);
      x4 = (int)(((int64_t)181 * (x4 - x5) + 128) >> 8);

      out[0] = (x7 + x1) >> 14;
      out[1] = (x3 + x2) >> 14;
      out[2] = (x0 + x4) >> 14;
      out[3] = (x8 + x6) >> 14;
      out[4] = (x8 - x6) >> 14;
      out[5] = (x0 - x4) >> 14;
      out[6] = (x3 - x2) >> 14;
      out[7] = (x7 - x1) >> 14;
   }

   for (i = 0; i < 8; i++)
   {
      /* An intra block reconstructs the sample value itself, not a residual:
       * the DC coefficient already carries the block mean (predictor 1024,
       * which the IDCT scales back to 128). Adding a 128 bias here -- as a
       * non-intra block would need before summing with its prediction --
       * shifts the whole picture and clips the highlights. */
      int p = out[i];

      if (p < 0)
         p = 0;
      else if (p > 255)
         p = 255;

      dst[(size_t)i * stride] = (uint8_t)p;
   }
}

static void idct_block(const int16_t *blk, uint8_t *dst, unsigned stride)
{
   /* Row results go to a 32-bit scratch rather than back into the 16-bit
    * coefficient block. Storing them as int16 is the usual shortcut and is
    * safe for real content, but it overflows on pathological input -- an
    * IEEE 1180 sweep at L=300 turns bright blocks into dark ones -- and 256
    * bytes of scratch is cheap next to getting that wrong. */
   int tmp[64];
   int i;

   for (i = 0; i < 8; i++)
      idct_row(blk + i * 8, tmp + i * 8);
   for (i = 0; i < 8; i++)
      idct_col_store(tmp + i, dst + i, stride);
}

/* Same transform, but summed onto an existing prediction instead of
 * replacing it. A non-intra block codes the residual, not the sample. */
static void idct_col_add(const int *b, uint8_t *dst, unsigned stride)
{
   int x0, x1, x2, x3, x4, x5, x6, x7, x8;
   int i;
   int out[8];

   x1 = b[8 * 4] * 256;
   x2 = b[8 * 6];
   x3 = b[8 * 2];
   x4 = b[8 * 1];
   x5 = b[8 * 7];
   x6 = b[8 * 5];
   x7 = b[8 * 3];

   if (!(x1 | x2 | x3 | x4 | x5 | x6 | x7))
   {
      int dc = (b[0] + 32) >> 6;
      for (i = 0; i < 8; i++)
         out[i] = dc;
   }
   else
   {
      x0 = b[0] * 256 + 8192;

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
      x2 = (int)(((int64_t)181 * (x4 + x5) + 128) >> 8);
      x4 = (int)(((int64_t)181 * (x4 - x5) + 128) >> 8);

      out[0] = (x7 + x1) >> 14;
      out[1] = (x3 + x2) >> 14;
      out[2] = (x0 + x4) >> 14;
      out[3] = (x8 + x6) >> 14;
      out[4] = (x8 - x6) >> 14;
      out[5] = (x0 - x4) >> 14;
      out[6] = (x3 - x2) >> 14;
      out[7] = (x7 - x1) >> 14;
   }

   for (i = 0; i < 8; i++)
   {
      int p = (int)dst[(size_t)i * stride] + out[i];

      if (p < 0)
         p = 0;
      else if (p > 255)
         p = 255;

      dst[(size_t)i * stride] = (uint8_t)p;
   }
}

static void idct_block_add(const int16_t *blk, uint8_t *dst, unsigned stride)
{
   int tmp[64];
   int i;

   for (i = 0; i < 8; i++)
      idct_row(blk + i * 8, tmp + i * 8);
   for (i = 0; i < 8; i++)
      idct_col_add(tmp + i, dst + i, stride);
}

#endif

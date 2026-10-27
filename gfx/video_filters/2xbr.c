/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Hyllian's 2xBR v3.3a
 * Copyright (C) 2011, 2014 Hyllian/Jararaca - sergiogdb@gmail.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "softfilter.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <retro_endianness.h>
#include <boolean.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation twoxbr_get_implementation
#define softfilter_thread_data twoxbr_softfilter_thread_data
#define filter_data twoxbr_filter_data
#endif

#define TWOXBR_SCALE 2

struct softfilter_thread_data
{
   void *out_data;
   const void *in_data;
   size_t out_pitch;
   size_t in_pitch;
   unsigned colfmt;
   unsigned width;
   unsigned height;
   int first;
   int last;
   /* This worker's YUV keys: one per source pixel of its rows and the
    * two rows either side, each row padded by two keys on both ends.
    * Rows past the frame and the padding repeat the nearest edge key,
    * which is the clamp the 5x5 neighbourhood reads through. Sized at
    * create for the largest frame, and filled by the worker itself. */
   uint16_t *keys;
};

struct filter_data
{
   unsigned threads;
   struct softfilter_thread_data *workers;
   unsigned in_fmt;
   /* Rows in the whole frame, for clamping the 5x5 neighbourhood at
    * its top and bottom whichever slice a worker has. */
   unsigned frame_height;
   /* The largest frame create was told of; a worker's keys hold its
    * share of one that size. */
   unsigned max_width;
   unsigned max_height;
   uint16_t RGBtoYUV[65536];
   uint16_t tbl_5_to_8[32];
   uint16_t tbl_6_to_8[64];
};

static unsigned twoxbr_generic_input_fmts(void)
{
   return SOFTFILTER_FMT_RGB565 | SOFTFILTER_FMT_XRGB8888;
}

static unsigned twoxbr_generic_output_fmts(unsigned input_fmts)
{
   return input_fmts;
}

static unsigned twoxbr_generic_threads(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

#define XRGB8888_TO_RGB565(p) \
   ((((p) >> 8) & 0xf800) | (((p) >> 5) & 0x07e0) | (((p) >> 3) & 0x001f))

#define RED_MASK565   0xF800
#define GREEN_MASK565 0x07E0
#define BLUE_MASK565  0x001F
#define PG_LBMASK565 0xF7DE

#ifdef MSB_FIRST
 #define RED_MASK8888   0xFF000000
 #define GREEN_MASK8888 0x00FF0000
 #define BLUE_MASK8888  0x0000FF00
 #define PG_LBMASK8888  0xFEFEFEFE
 #define ALPHA_MASK8888 0x000000FF
#else
 #define RED_MASK8888   0x000000FF
 #define GREEN_MASK8888 0x0000FF00
 #define BLUE_MASK8888  0x00FF0000
 #define PG_LBMASK8888  0xFEFEFEFE
 #define ALPHA_MASK8888 0xFF000000
#endif

static void SetupFormat(void * data)
{
   uint32_t c;
   struct filter_data *filt = (struct filter_data*)data;

   filt->tbl_5_to_8[0]  = 0;
   filt->tbl_5_to_8[1]  = 8;
   filt->tbl_5_to_8[2]  = 16;
   filt->tbl_5_to_8[3]  = 25;
   filt->tbl_5_to_8[4]  = 33;
   filt->tbl_5_to_8[5]  = 41;
   filt->tbl_5_to_8[6]  = 49;
   filt->tbl_5_to_8[7]  = 58;
   filt->tbl_5_to_8[8]  = 66;
   filt->tbl_5_to_8[9]  = 74;
   filt->tbl_5_to_8[10] = 82;
   filt->tbl_5_to_8[11] = 90;
   filt->tbl_5_to_8[12] = 99;
   filt->tbl_5_to_8[13] = 107;
   filt->tbl_5_to_8[14] = 115;
   filt->tbl_5_to_8[15] = 123;
   filt->tbl_5_to_8[16] = 132;
   filt->tbl_5_to_8[17] = 140;
   filt->tbl_5_to_8[18] = 148;
   filt->tbl_5_to_8[19] = 156;
   filt->tbl_5_to_8[20] = 165;
   filt->tbl_5_to_8[21] = 173;
   filt->tbl_5_to_8[22] = 181;
   filt->tbl_5_to_8[23] = 189;
   filt->tbl_5_to_8[24] = 197;
   filt->tbl_5_to_8[25] = 206;
   filt->tbl_5_to_8[26] = 214;
   filt->tbl_5_to_8[27] = 222;
   filt->tbl_5_to_8[28] = 230;
   filt->tbl_5_to_8[29] = 239;
   filt->tbl_5_to_8[30] = 247;
   filt->tbl_5_to_8[31] = 255;

   filt->tbl_6_to_8[0]   = 0;
   filt->tbl_6_to_8[1]   = 4;
   filt->tbl_6_to_8[2]   = 8;
   filt->tbl_6_to_8[3]   = 12;
   filt->tbl_6_to_8[4]   = 16;
   filt->tbl_6_to_8[5]   = 20;
   filt->tbl_6_to_8[6]   = 24;
   filt->tbl_6_to_8[7]   = 28;
   filt->tbl_6_to_8[8]   = 32;
   filt->tbl_6_to_8[9]   = 36;
   filt->tbl_6_to_8[10]  = 40;
   filt->tbl_6_to_8[11]  = 45;
   filt->tbl_6_to_8[12]  = 49;
   filt->tbl_6_to_8[13]  = 53;
   filt->tbl_6_to_8[14]  = 57;
   filt->tbl_6_to_8[15]  = 61;
   filt->tbl_6_to_8[16]  = 65;
   filt->tbl_6_to_8[17]  = 69;
   filt->tbl_6_to_8[18]  = 73;
   filt->tbl_6_to_8[19]  = 77;
   filt->tbl_6_to_8[20]  = 81;
   filt->tbl_6_to_8[21]  = 85;
   filt->tbl_6_to_8[22]  = 89;
   filt->tbl_6_to_8[23]  = 93;
   filt->tbl_6_to_8[24]  = 97;
   filt->tbl_6_to_8[25]  = 101;
   filt->tbl_6_to_8[26]  = 105;
   filt->tbl_6_to_8[27]  = 109;
   filt->tbl_6_to_8[28]  = 113;
   filt->tbl_6_to_8[29]  = 117;
   filt->tbl_6_to_8[30]  = 121;
   filt->tbl_6_to_8[31]  = 125;
   filt->tbl_6_to_8[32]  = 130;
   filt->tbl_6_to_8[33]  = 134;
   filt->tbl_6_to_8[34]  = 138;
   filt->tbl_6_to_8[35]  = 142;
   filt->tbl_6_to_8[36]  = 146;
   filt->tbl_6_to_8[37]  = 150;
   filt->tbl_6_to_8[38]  = 154;
   filt->tbl_6_to_8[39]  = 158;
   filt->tbl_6_to_8[40]  = 162;
   filt->tbl_6_to_8[41]  = 166;
   filt->tbl_6_to_8[42]  = 170;
   filt->tbl_6_to_8[43]  = 174;
   filt->tbl_6_to_8[44]  = 178;
   filt->tbl_6_to_8[45]  = 182;
   filt->tbl_6_to_8[46]  = 186;
   filt->tbl_6_to_8[47]  = 190;
   filt->tbl_6_to_8[48]  = 194;
   filt->tbl_6_to_8[49]  = 198;
   filt->tbl_6_to_8[50]  = 202;
   filt->tbl_6_to_8[51]  = 206;
   filt->tbl_6_to_8[52]  = 210;
   filt->tbl_6_to_8[53]  = 215;
   filt->tbl_6_to_8[54]  = 219;
   filt->tbl_6_to_8[55]  = 223;
   filt->tbl_6_to_8[56]  = 227;
   filt->tbl_6_to_8[57]  = 231;
   filt->tbl_6_to_8[58]  = 235;
   filt->tbl_6_to_8[59]  = 239;
   filt->tbl_6_to_8[60]  = 243;
   filt->tbl_6_to_8[61]  = 247;
   filt->tbl_6_to_8[62]  = 251;
   filt->tbl_6_to_8[63]  = 255;

   for (c = 0; c < 65536; c++)
   {
      uint16_t r = filt->tbl_5_to_8[(c &   RED_MASK565) >> 11];
      uint16_t g = filt->tbl_6_to_8[(c & GREEN_MASK565) >>  5];
      uint16_t b = filt->tbl_5_to_8[(c &  BLUE_MASK565)      ];
      uint16_t y = ((r << 4) + (g << 5) + (b << 2));
      uint16_t u = (   -r  - (g << 1) + (b << 2));
      uint16_t v = ((r << 1) - (g << 1) - (b >> 1));
      filt->RGBtoYUV[c] = y + u + v;
   }
}

static void twoxbr_generic_destroy(void *data);

static void *twoxbr_generic_create(const struct softfilter_config *config,
      unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd, void *userdata)
{
   struct filter_data *filt = (struct filter_data*)calloc(1, sizeof(*filt));
   (void)simd;
   (void)config;
   (void)userdata;
   if (!filt)
      return NULL;
   filt->workers = (struct softfilter_thread_data*)
      calloc(threads, sizeof(struct softfilter_thread_data));
   filt->threads    = threads;
   filt->in_fmt     = in_fmt;
   filt->max_width  = max_width;
   filt->max_height = max_height;
   if (!filt->workers)
   {
      free(filt);
      return NULL;
   }
   {
      /* A worker gets at most ceil(height / threads) rows, plus two
       * either side; a row is the width plus two keys at each end. */
      size_t rows = (max_height + threads - 1) / threads + 4;
      size_t cols = (size_t)max_width + 4;
      unsigned i;
      for (i = 0; i < threads; i++)
      {
         if (!(filt->workers[i].keys = (uint16_t*)
                  malloc(rows * cols * sizeof(uint16_t))))
         {
            twoxbr_generic_destroy(filt);
            return NULL;
         }
      }
   }

   SetupFormat(filt);

   return filt;
}

static void twoxbr_generic_output(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   *out_width = width * TWOXBR_SCALE;
   *out_height = height * TWOXBR_SCALE;
}

static void twoxbr_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;

   if (!filt)
      return;

   if (filt->workers)
   {
      unsigned i;
      for (i = 0; i < filt->threads; i++)
         free(filt->workers[i].keys);
      free(filt->workers);
   }
   free(filt);
}

#define ALPHA_BLEND_128_W(dst, src) dst = ((src & pg_lbmask) >> 1) + ((dst & pg_lbmask) >> 1)

#define ALPHA_BLEND_32_W(dst, src) \
        dst = ( \
    (pg_red_mask & ((dst & pg_red_mask) + \
        ((((src & pg_red_mask) - \
        (dst & pg_red_mask))) >>3))) | \
    (pg_green_mask & ((dst & pg_green_mask) + \
        ((((src & pg_green_mask) - \
        (dst & pg_green_mask))) >>3))) | \
    (pg_blue_mask & ((dst & pg_blue_mask) + \
        ((((src & pg_blue_mask) - \
        (dst & pg_blue_mask))) >>3))) )

#define ALPHA_BLEND_8888_32_W(dst, src) \
	dst = ( \
    (pg_red_mask & ((dst & pg_red_mask) + \
        ((((src & pg_red_mask) - \
        (dst & pg_red_mask))) >>3))) | \
    (pg_green_mask & ((dst & pg_green_mask) + \
        ((((src & pg_green_mask) - \
        (dst & pg_green_mask))) >>3))) | \
    (pg_blue_mask & ((dst & pg_blue_mask) + \
        ((((src & pg_blue_mask) - \
        (dst & pg_blue_mask))) >>3))) ) +\
        pg_alpha_mask

#define ALPHA_BLEND_64_W(dst, src) \
        dst = ( \
    (pg_red_mask & ((dst & pg_red_mask) + \
        ((((src & pg_red_mask) - \
        (dst & pg_red_mask))) >>2))) | \
    (pg_green_mask & ((dst & pg_green_mask) + \
        ((((src & pg_green_mask) - \
        (dst & pg_green_mask))) >>2))) | \
    (pg_blue_mask & ((dst & pg_blue_mask) + \
        ((((src & pg_blue_mask) - \
        (dst & pg_blue_mask))) >>2))) )

#define ALPHA_BLEND_8888_64_W(dst, src) \
	dst = ( \
    (pg_red_mask & ((dst & pg_red_mask) + \
        ((((src & pg_red_mask) - \
        (dst & pg_red_mask))) >>2))) | \
    (pg_green_mask & ((dst & pg_green_mask) + \
        ((((src & pg_green_mask) - \
        (dst & pg_green_mask))) >>2))) | \
    (pg_blue_mask & ((dst & pg_blue_mask) + \
        ((((src & pg_blue_mask) - \
        (dst & pg_blue_mask))) >>2))) ) +\
        pg_alpha_mask

#define ALPHA_BLEND_192_W(dst, src) \
        dst = ( \
    (pg_red_mask & ((dst & pg_red_mask) + \
        ((((src & pg_red_mask) - \
        (dst & pg_red_mask)) * 192) >>8))) | \
    (pg_green_mask & ((dst & pg_green_mask) + \
        ((((src & pg_green_mask) - \
        (dst & pg_green_mask)) * 192) >>8))) | \
    (pg_blue_mask & ((dst & pg_blue_mask) + \
        ((((src & pg_blue_mask) - \
        (dst & pg_blue_mask)) * 192) >>8))) )

#define ALPHA_BLEND_8888_192_W(dst, src) \
	dst = ( \
    (pg_red_mask & ((dst & pg_red_mask) + \
        ((((src & pg_red_mask) - \
        (dst & pg_red_mask)) * 192) >>8))) | \
    (pg_green_mask & ((dst & pg_green_mask) + \
        ((((src & pg_green_mask) - \
        (dst & pg_green_mask)) * 192) >>8))) | \
    (pg_blue_mask & ((dst & pg_blue_mask) + \
        ((((src & pg_blue_mask) - \
        (dst & pg_blue_mask)) * 192) >>8))) ) +\
        pg_alpha_mask

#define ALPHA_BLEND_224_W(dst, src) \
        dst = ( \
    (pg_red_mask & ((dst & pg_red_mask) + \
        ((((src & pg_red_mask) - \
        (dst & pg_red_mask)) * 224) >>8))) | \
    (pg_green_mask & ((dst & pg_green_mask) + \
        ((((src & pg_green_mask) - \
        (dst & pg_green_mask)) * 224) >>8))) | \
    (pg_blue_mask & ((dst & pg_blue_mask) + \
        ((((src & pg_blue_mask) - \
        (dst & pg_blue_mask)) * 224) >>8))) );

#define ALPHA_BLEND_8888_224_W(dst, src) \
	dst = ( \
    (pg_red_mask & ((dst & pg_red_mask) + \
        ((((src & pg_red_mask) - \
        (dst & pg_red_mask)) * 224) >>8))) | \
    (pg_green_mask & ((dst & pg_green_mask) + \
        ((((src & pg_green_mask) - \
        (dst & pg_green_mask)) * 224) >>8))) | \
    (pg_blue_mask & ((dst & pg_blue_mask) + \
        ((((src & pg_blue_mask) - \
        (dst & pg_blue_mask)) * 224) >>8))) ) +\
        pg_alpha_mask

#define LEFT_UP_2_2X(N3, N2, N1, PIXEL)\
             ALPHA_BLEND_224_W(E[N3], PIXEL); \
             ALPHA_BLEND_64_W( E[N2], PIXEL); \
             E[N1] = E[N2]; \

#define LEFT_2_2X(N3, N2, PIXEL)\
             ALPHA_BLEND_192_W(E[N3], PIXEL); \
             ALPHA_BLEND_64_W( E[N2], PIXEL); \

#define UP_2_2X(N3, N1, PIXEL)\
             ALPHA_BLEND_192_W(E[N3], PIXEL); \
             ALPHA_BLEND_64_W( E[N1], PIXEL); \

#define DIA_2X(N3, PIXEL)\
             ALPHA_BLEND_128_W(E[N3], PIXEL); \

#define LEFT_UP_2_8888_2X(N3, N2, N1, PIXEL)\
             ALPHA_BLEND_8888_224_W(E[N3], PIXEL); \
             ALPHA_BLEND_8888_64_W( E[N2], PIXEL); \
             E[N1] = E[N2]; \

#define LEFT_2_8888_2X(N3, N2, PIXEL)\
             ALPHA_BLEND_8888_192_W(E[N3], PIXEL); \
             ALPHA_BLEND_8888_64_W( E[N2], PIXEL); \

#define UP_2_8888_2X(N3, N1, PIXEL)\
             ALPHA_BLEND_8888_192_W(E[N3], PIXEL); \
             ALPHA_BLEND_8888_64_W( E[N1], PIXEL); \

#define DIA_8888_2X(N3, PIXEL)\
             ALPHA_BLEND_128_W(E[N3], PIXEL); \

/* Every comparison names two of the 5x5 neighbours; K_<name> is that
 * neighbour's precomputed YUV key. */
#define df(Z, A, B)\
        abs((int)K_##A - (int)K_##B)\

#define eq(Z, A, B)\
        (df(Z, A, B) < 155)\

/* The XRGB8888 path makes its edge decisions at RGB565 precision,
 * through the same YUV keys as the RGB565 path; the blending itself
 * stays at full 8-bit precision.  The masks are the RGB565 path's
 * arguments and are not needed here. */
#define df8(A, B, rm, gm, bm) abs((int)K_##A - (int)K_##B)
#define eq8(A, B, rm, gm, bm) (df8(A, B, rm, gm, bm) < 155)

#define FILTRO_RGB565(Z, PE, _PI, PH, PF, PG, PC, PD, PB, PA, G5, C4, G0, D0, C1, B1, F4, I4, H5, I5, A0, A1, N0, N1, N2, N3, pg_red_mask, pg_green_mask, pg_blue_mask) \
     ex   = (PE!=PH && PE!=PF); \
     if ( ex )\
     {\
          e = (df(Z, PE,PC)+df(Z, PE,PG)+df(Z, _PI,H5)+df(Z, _PI,F4))+(df(Z, PH,PF)<<2); \
          i = (df(Z, PH,PD)+df(Z, PH,I5)+df(Z, PF,I4)+df(Z, PF,PB))+(df(Z, PE,_PI)<<2); \
          if ((e<i)  && ( (!eq(Z, PF,PB) && !eq(Z, PF,PC)) || (!eq(Z, PH,PD) && !eq(Z, PH,PG)) || (eq(Z, PE,_PI) && ((!eq(Z, PF,F4) && !eq(Z, PF,I4)) || (!eq(Z, PH,H5) && !eq(Z, PH,I5)))) || eq(Z, PE,PG) || eq(Z, PE,PC)) )\
          {\
              ke=df(Z, PF,PG); \
              ki=df(Z, PH,PC); \
              ex2 = (PE!=PC && PB!=PC); ex3 = (PE!=PG && PD!=PG); px = (df(Z, PE,PF) <= df(Z, PE,PH)) ? PF : PH; \
              if ( ((ke<<1)<=ki) && ex3 && (ke>=(ki<<1)) && ex2 ) \
              {\
                     LEFT_UP_2_2X(N3, N2, N1, px)\
              }\
              else if ( ((ke<<1)<=ki) && ex3 ) \
              {\
                     LEFT_2_2X(N3, N2, px);\
              }\
              else if ( (ke>=(ki<<1)) && ex2 ) \
              {\
                     UP_2_2X(N3, N1, px);\
              }\
              else \
              {\
                     DIA_2X(N3, px);\
              }\
          }\
          else if (e<=i)\
          {\
               ALPHA_BLEND_128_W( E[N3], ((df(Z, PE,PF) <= df(Z, PE,PH)) ? PF : PH)); \
          }\
     }\

#define FILTRO_RGB8888(Z, PE, _PI, PH, PF, PG, PC, PD, PB, PA, G5, C4, G0, D0, C1, B1, F4, I4, H5, I5, A0, A1, N0, N1, N2, N3, pg_red_mask, pg_green_mask, pg_blue_mask) \
     ex   = (PE!=PH && PE!=PF); \
     if ( ex )\
     {\
          e = (df8(PE,PC, pg_red_mask, pg_green_mask, pg_blue_mask ) + df8(PE,PG, pg_red_mask, pg_green_mask, pg_blue_mask) + \
                df8(_PI,H5, pg_red_mask, pg_green_mask, pg_blue_mask ) + df8(_PI,F4, pg_red_mask, pg_green_mask, pg_blue_mask))+(4 * (df8(PH,PF, pg_red_mask, pg_green_mask, pg_blue_mask))); \
          i = (df8(PH,PD, pg_red_mask, pg_green_mask, pg_blue_mask) + df8(PH,I5, pg_red_mask, pg_green_mask, pg_blue_mask) + \
                df8(PF,I4, pg_red_mask, pg_green_mask, pg_blue_mask) + df8(PF,PB, pg_red_mask, pg_green_mask, pg_blue_mask))+(4 * (df8(PE,_PI, pg_red_mask, pg_green_mask, pg_blue_mask))); \
          if ((e<i)  && ( (!eq8(PF,PB, pg_red_mask, pg_green_mask, pg_blue_mask) && !eq8(PF,PC, pg_red_mask, pg_green_mask, pg_blue_mask)) || (!eq8(PH,PD, pg_red_mask, pg_green_mask, pg_blue_mask) && !eq8(PH,PG, pg_red_mask, pg_green_mask, pg_blue_mask)) || (eq8(PE,_PI, pg_red_mask, pg_green_mask, pg_blue_mask) && ((!eq8(PF,F4, pg_red_mask, pg_green_mask, pg_blue_mask) && !eq8(PF,I4, pg_red_mask, pg_green_mask, pg_blue_mask)) || (!eq8(PH,H5, pg_red_mask, pg_green_mask,pg_blue_mask) && !eq8(PH,I5, pg_red_mask, pg_green_mask, pg_blue_mask)))) || eq8(PE,PG, pg_red_mask, pg_green_mask, pg_blue_mask) || eq8(PE,PC, pg_red_mask, pg_green_mask, pg_blue_mask)) )\
          {\
              ke=df8(PF,PG, pg_red_mask, pg_green_mask, pg_blue_mask); ki=df8(PH,PC, pg_red_mask, pg_green_mask, pg_blue_mask); \
              ex2 = (PE!=PC && PB!=PC); ex3 = (PE!=PG && PD!=PG); px = (df8(PE,PF, pg_red_mask, pg_green_mask, pg_blue_mask) <= df8(PE,PH, pg_red_mask, pg_green_mask, pg_blue_mask)) ? PF : PH; \
              if ( ((ke<<1)<=ki) && ex3 && (ke>=(ki<<1)) && ex2 ) \
              {\
                     LEFT_UP_2_8888_2X(N3, N2, N1, px)\
              }\
              else if ( ((ke<<1)<=ki) && ex3 ) \
              {\
                     LEFT_2_8888_2X(N3, N2, px);\
              }\
              else if ( (ke>=(ki<<1)) && ex2 ) \
              {\
                     UP_2_8888_2X(N3, N1, px);\
              }\
              else \
              {\
                     DIA_8888_2X(N3, px);\
              }\
          }\
          else if (e<=i)\
          {\
               ALPHA_BLEND_128_W( E[N3], ((df8(PE,PF, pg_red_mask, pg_green_mask, pg_blue_mask) <= df8(PE,PH, pg_red_mask, pg_green_mask, pg_blue_mask)) ? PF : PH)); \
          }\
     }\

#ifndef twoxbr_function
#define twoxbr_function(FILTRO, Z) \
            E[0] = E[1] = E[2] = E[3] = PE;\
            FILTRO(Z, PE, _PI, PH, PF, PG, PC, PD, PB, PA, G5, C4, G0, D0, C1, B1, F4, I4, H5, I5, A0, A1, 0, 1, 2, 3, pg_red_mask, pg_green_mask, pg_blue_mask);\
            FILTRO(Z, PE, PC, PF, PB, _PI, PA, PH, PD, PG, I4, A1, I5, H5, A0, D0, B1, C1, F4, C4, G5, G0, 2, 0, 3, 1, pg_red_mask, pg_green_mask, pg_blue_mask);\
            FILTRO(Z, PE, PA, PB, PD, PC, PG, PF, PH, _PI, C1, G0, C4, F4, G5, H5, D0, A0, B1, A1, I4, I5, 3, 2, 1, 0, pg_red_mask, pg_green_mask, pg_blue_mask);\
            FILTRO(Z, PE, PG, PD, PH, PA, _PI, PB, PF, PC, A0, I5, A1, B1, I4, F4, H5, G5, D0, G0, C1, C4, 1, 3, 0, 2, pg_red_mask, pg_green_mask, pg_blue_mask);\
         out[0] = E[0]; \
         out[1] = E[1]; \
         out[dst_stride] = E[2]; \
         out[dst_stride + 1] = E[3]; \
         out += 2
#endif

/* Fills a worker's padded key rows: row j holds the frame row
 * first - 2 + j, clamped into the frame, and each row repeats its end
 * keys twice past either side. */
#define TWOXBR_FILL_KEYS(TYPE, KEY_OF) \
   { \
      unsigned j_, x_; \
      unsigned kw_ = width + 4; \
      for (j_ = 0; j_ < height + 4; j_++) \
      { \
         int fy_          = first - 2 + (int)j_; \
         const TYPE *row_; \
         uint16_t *k_     = keys + (size_t)j_ * kw_; \
         if (fy_ < 0) \
            fy_ = 0; \
         if (fy_ > (int)frame_height - 1) \
            fy_ = (int)frame_height - 1; \
         row_ = (const TYPE*)((const uint8_t*)src \
               + ((long)fy_ - first) * (long)src_stride * (long)sizeof(TYPE)); \
         for (x_ = 0; x_ < width; x_++) \
            k_[x_ + 2] = KEY_OF(row_[x_]); \
         k_[0] = k_[1] = k_[2]; \
         k_[width + 2] = k_[width + 3] = k_[width + 1]; \
      } \
   }

/* One worker's rows. The 5x5 neighbourhood is clamped to the frame:
 * the keys through the padded rows above, the 3x3 pixels through
 * clamped indices. A pixel equal to enough of its four direct
 * neighbours that none of the four corner rules can fire (each needs
 * PE to differ from both pixels beside that corner) is copied out
 * without loading the rest of its neighbourhood. */
#define TWOXBR_ROWS(TYPE, FILTRO) \
   for (y = 0; y < height; y++) \
   { \
      unsigned abs_y   = (unsigned)first + y; \
      int up1          = abs_y >= 1 ? -1 : 0; \
      int dn1          = abs_y + 1 < frame_height ? 1 : 0; \
      const TYPE *rm1  = src + up1 * (int)src_stride; \
      const TYPE *r0   = src; \
      const TYPE *rp1  = src + dn1 * (int)src_stride; \
      const uint16_t *km2 = keys + (size_t)(y + 0) * (width + 4) + 2; \
      const uint16_t *km1 = km2 + (width + 4); \
      const uint16_t *k0  = km1 + (width + 4); \
      const uint16_t *kp1 = k0  + (width + 4); \
      const uint16_t *kp2 = kp1 + (width + 4); \
      TYPE *out           = dst; \
      for (x = 0; x < width; x++) \
      { \
         TYPE E[4]; \
         TYPE ex, e, i, ke, ki, ex2, ex3, px; \
         unsigned xm1   = x ? x - 1 : 0; \
         unsigned xp1   = x + 1 < width ? x + 1 : x; \
         TYPE PB = rm1[x]; \
         TYPE PD = r0[xm1]; \
         TYPE PE = r0[x]; \
         TYPE PF = r0[xp1]; \
         TYPE PH = rp1[x]; \
         if (     (PE == PH || PE == PF) && (PE == PF || PE == PB) \
               && (PE == PB || PE == PD) && (PE == PD || PE == PH)) \
         { \
            out[0] = out[1] = out[dst_stride] = out[dst_stride + 1] = PE; \
            out += 2; \
            continue; \
         } \
         { \
            int xi          = (int)x; \
            TYPE PA         = rm1[xm1]; \
            TYPE PC         = rm1[xp1]; \
            TYPE PG         = rp1[xm1]; \
            TYPE _PI        = rp1[xp1]; \
            uint16_t K_A1 = km2[xi - 1], K_B1 = km2[xi],   K_C1 = km2[xi + 1]; \
            uint16_t K_A0 = km1[xi - 2], K_PA = km1[xi - 1], K_PB = km1[xi]; \
            uint16_t K_PC = km1[xi + 1], K_C4 = km1[xi + 2]; \
            uint16_t K_D0 = k0[xi - 2],  K_PD = k0[xi - 1],  K_PE = k0[xi]; \
            uint16_t K_PF = k0[xi + 1],  K_F4 = k0[xi + 2]; \
            uint16_t K_G0 = kp1[xi - 2], K_PG = kp1[xi - 1], K_PH = kp1[xi]; \
            uint16_t K__PI = kp1[xi + 1], K_I4 = kp1[xi + 2]; \
            uint16_t K_G5 = kp2[xi - 1], K_H5 = kp2[xi],   K_I5 = kp2[xi + 1]; \
            /* \
             * Map of the pixels:          A1 B1 C1 \
             *                          A0 PA PB PC C4 \
             *                          D0 PD PE PF F4 \
             *                          G0 PG PH _PI I4 \
             *                             G5 H5 I5 \
             */ \
            twoxbr_function(FILTRO, filt); \
         } \
      } \
      src += src_stride; \
      dst += 2 * dst_stride; \
   }

#define TWOXBR_KEY_565(p)  filt->RGBtoYUV[p]
#define TWOXBR_KEY_8888(p) filt->RGBtoYUV[XRGB8888_TO_RGB565(p)]

static void twoxbr_generic_xrgb8888(void *data, uint16_t *keys,
      unsigned width, unsigned height, int first, const uint32_t *src,
      unsigned src_stride, uint32_t *dst, unsigned dst_stride)
{
   unsigned x, y;
   uint32_t pg_red_mask      = RED_MASK8888;
   uint32_t pg_green_mask    = GREEN_MASK8888;
   uint32_t pg_blue_mask     = BLUE_MASK8888;
   uint32_t pg_lbmask        = PG_LBMASK8888;
   uint32_t pg_alpha_mask    = ALPHA_MASK8888;
   struct filter_data *filt  = (struct filter_data*)data;
   unsigned frame_height     = filt->frame_height;

   TWOXBR_FILL_KEYS(uint32_t, TWOXBR_KEY_8888)
   TWOXBR_ROWS(uint32_t, FILTRO_RGB8888)
   (void)pg_alpha_mask;
}

static void twoxbr_generic_rgb565(void *data, uint16_t *keys,
      unsigned width, unsigned height, int first, const uint16_t *src,
      unsigned src_stride, uint16_t *dst, unsigned dst_stride)
{
   unsigned x, y;
   struct filter_data *filt = (struct filter_data*)data;
   uint16_t pg_red_mask     = RED_MASK565;
   uint16_t pg_green_mask   = GREEN_MASK565;
   uint16_t pg_blue_mask    = BLUE_MASK565;
   uint16_t pg_lbmask       = PG_LBMASK565;
   unsigned frame_height    = filt->frame_height;

   TWOXBR_FILL_KEYS(uint16_t, TWOXBR_KEY_565)
   TWOXBR_ROWS(uint16_t, FILTRO_RGB565)
}

/* Plain 2x pixel doubling, for a frame whose keys could not be
 * allocated: the workers cannot run the edge detector without them,
 * and the output buffer still has to be filled with something
 * sensible for this frame rather than left as it was. */
static void twoxbr_nearest_cb_rgb565(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr =
      (struct softfilter_thread_data*)thread_data;
   const uint16_t *src = (const uint16_t*)thr->in_data;
   uint16_t *dst       = (uint16_t*)thr->out_data;
   unsigned src_stride = (unsigned)(thr->in_pitch  / SOFTFILTER_BPP_RGB565);
   unsigned dst_stride = (unsigned)(thr->out_pitch / SOFTFILTER_BPP_RGB565);
   unsigned x, y;
   for (y = 0; y < thr->height; y++)
   {
      uint16_t *o0 = dst;
      uint16_t *o1 = dst + dst_stride;
      for (x = 0; x < thr->width; x++)
      {
         uint16_t p = src[x];
         o0[2 * x] = o0[2 * x + 1] = p;
         o1[2 * x] = o1[2 * x + 1] = p;
      }
      src += src_stride;
      dst += 2 * dst_stride;
   }
}

static void twoxbr_nearest_cb_xrgb8888(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr =
      (struct softfilter_thread_data*)thread_data;
   const uint32_t *src = (const uint32_t*)thr->in_data;
   uint32_t *dst       = (uint32_t*)thr->out_data;
   unsigned src_stride = (unsigned)(thr->in_pitch  / SOFTFILTER_BPP_XRGB8888);
   unsigned dst_stride = (unsigned)(thr->out_pitch / SOFTFILTER_BPP_XRGB8888);
   unsigned x, y;
   for (y = 0; y < thr->height; y++)
   {
      uint32_t *o0 = dst;
      uint32_t *o1 = dst + dst_stride;
      for (x = 0; x < thr->width; x++)
      {
         uint32_t p = src[x];
         o0[2 * x] = o0[2 * x + 1] = p;
         o1[2 * x] = o1[2 * x + 1] = p;
      }
      src += src_stride;
      dst += 2 * dst_stride;
   }
}

static void twoxbr_work_cb_rgb565(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr =
      (struct softfilter_thread_data*)thread_data;
   uint16_t *input = (uint16_t*)thr->in_data;
   uint16_t *output = (uint16_t*)thr->out_data;
   unsigned width = thr->width;
   unsigned height = thr->height;

   twoxbr_generic_rgb565(data, thr->keys, width, height,
         thr->first, input,
         (unsigned)(thr->in_pitch / SOFTFILTER_BPP_RGB565),
         output,
         (unsigned)(thr->out_pitch / SOFTFILTER_BPP_RGB565));
}

static void twoxbr_work_cb_xrgb8888(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr =
      (struct softfilter_thread_data*)thread_data;
   uint32_t *input = (uint32_t*)thr->in_data;
   uint32_t *output = (uint32_t*)thr->out_data;
   unsigned width = thr->width;
   unsigned height = thr->height;

   twoxbr_generic_xrgb8888(data, thr->keys, width, height,
         thr->first, input,
         (unsigned)(thr->in_pitch / SOFTFILTER_BPP_XRGB8888),
         output,
         (unsigned)(thr->out_pitch / SOFTFILTER_BPP_XRGB8888));
}

static void twoxbr_generic_packets(void *data,
      struct softfilter_work_packet *packets,
      void *output, size_t output_stride,
      const void *input, unsigned width,
      unsigned height, size_t input_stride)
{
   unsigned i;
   struct filter_data *filt = (struct filter_data*)data;
   bool have_keys           = true;

   filt->frame_height = height;

   /* The workers' keys are sized for the largest frame create was told
    * of; a bigger one (which the softfilter contract rules out) is
    * pixel-doubled rather than read past them. */
   if (width > filt->max_width || height > filt->max_height)
      have_keys = false;

   for (i = 0; i < filt->threads; i++)
   {
      struct softfilter_thread_data *thr =
         (struct softfilter_thread_data*)&filt->workers[i];

      unsigned y_start = (height * i) / filt->threads;
      unsigned y_end = (height * (i + 1)) / filt->threads;

      thr->out_data = (uint8_t*)output + y_start *
         TWOXBR_SCALE * output_stride;
      thr->in_data = (const uint8_t*)input + y_start * input_stride;
      thr->out_pitch = output_stride;
      thr->in_pitch = input_stride;
      thr->width = width;
      thr->height = y_end - y_start;

      /* Workers need to know if they can access
       * pixels outside their given buffer. */
      thr->first = y_start;
      thr->last = y_end == height;

      if (filt->in_fmt == SOFTFILTER_FMT_RGB565)
         packets[i].work = have_keys
            ? twoxbr_work_cb_rgb565  : twoxbr_nearest_cb_rgb565;
      else if (filt->in_fmt == SOFTFILTER_FMT_XRGB8888)
         packets[i].work = have_keys
            ? twoxbr_work_cb_xrgb8888 : twoxbr_nearest_cb_xrgb8888;
      packets[i].thread_data = thr;
   }
}

static const struct softfilter_implementation twoxbr_generic = {
   twoxbr_generic_input_fmts,
   twoxbr_generic_output_fmts,

   twoxbr_generic_create,
   twoxbr_generic_destroy,

   twoxbr_generic_threads,
   twoxbr_generic_output,
   twoxbr_generic_packets,
   SOFTFILTER_API_VERSION,
   "2xBR",
   "2xbr",
};

const struct softfilter_implementation *softfilter_get_implementation(
      softfilter_simd_mask_t simd)
{
   (void)simd;
   return &twoxbr_generic;
}

#undef XRGB8888_TO_RGB565
#undef df8
#undef eq8

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif

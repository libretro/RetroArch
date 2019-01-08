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
};

struct filter_data
{
   unsigned threads;
   struct softfilter_thread_data *workers;
   unsigned in_fmt;
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
   filt->threads = 1;
   filt->in_fmt  = in_fmt;
   if (!filt->workers)
   {
      free(filt);
      return NULL;
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

   free(filt->workers);
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

#define df(Z, A, B)\
        abs(Z->RGBtoYUV[A] - Z->RGBtoYUV[B])\

#define eq(Z, A, B)\
        (df(Z, A, B) < 155)\

float df8(uint32_t A, uint32_t B,
      uint32_t pg_red_mask, uint32_t pg_green_mask, uint32_t pg_blue_mask)
{
   uint32_t r, g, b;
   uint32_t y, u, v;

#ifdef MSB_FIRST
   r = abs((int)(((A & pg_red_mask  )>>24) - ((B & pg_red_mask  )>> 24)));
   g = abs((int)(((A & pg_green_mask  )>>16) - ((B & pg_green_mask  )>> 16)));
   b = abs((int)(((A & pg_blue_mask  )>>8 ) - ((B & pg_blue_mask  )>> 8 )));
#else
   b = abs((int)(((A & pg_blue_mask  )>>16) - ((B & pg_blue_mask  )>> 16)));
   g = abs((int)(((A & pg_green_mask)>>8  ) - ((B & pg_green_mask )>>  8)));
   r = abs((int)(((A & pg_red_mask        ) -  (B & pg_red_mask         ))));
#endif

   y = fabs(0.299*r + 0.587*g + 0.114*b);
   u = fabs(-0.169*r - 0.331*g + 0.500*b);
   v = fabs(0.500*r - 0.419*g - 0.081*b);

   return 48*y + 7*u + 6*v;
}

int eq8(uint32_t A, uint32_t B,
      uint32_t pg_red_mask, uint32_t pg_green_mask, uint32_t pg_blue_mask)
{
    uint32_t r, g, b;
    uint32_t y, u, v;

#ifdef MSB_FIRST
   r = abs((int)(((A & pg_red_mask  )>>24) - ((B & pg_red_mask  )>> 24)));
   g = abs((int)(((A & pg_green_mask  )>>16) - ((B & pg_green_mask  )>> 16)));
   b = abs((int)(((A & pg_blue_mask  )>>8 ) - ((B & pg_blue_mask  )>> 8 )));
#else
   b = abs((int)(((A & pg_blue_mask  )>>16) - ((B & pg_blue_mask  )>> 16)));
   g = abs((int)(((A & pg_green_mask)>>8  ) - ((B & pg_green_mask )>>  8)));
   r = abs((int)(((A & pg_red_mask        ) -  (B & pg_red_mask         ))));
#endif

    y = fabs(0.299*r + 0.587*g + 0.114*b);
    u = fabs(-0.169*r - 0.331*g + 0.500*b);
    v = fabs(0.500*r - 0.419*g - 0.081*b);

    return ((48 >= y) && (7 >= u) && (6 >= v)) ? 1 : 0;
}

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
         ++in; \
         out += 2
#endif

static void twoxbr_generic_xrgb8888(void *data, unsigned width, unsigned height,
      int first, int last, uint32_t *src,
      unsigned src_stride, uint32_t *dst, unsigned dst_stride)
{
   unsigned nextline, finish;
   uint32_t pg_red_mask      = RED_MASK8888;
   uint32_t pg_green_mask    = GREEN_MASK8888;
   uint32_t pg_blue_mask     = BLUE_MASK8888;
   uint32_t pg_lbmask        = PG_LBMASK8888;
   uint32_t pg_alpha_mask    = ALPHA_MASK8888;
   struct filter_data *filt = (struct filter_data*)data;

   (void)filt;

   nextline = (last) ? 0 : src_stride;

   for (; height; height--)
   {
      uint32_t *in  = (uint32_t*)src;
      uint32_t *out = (uint32_t*)dst;

      for (finish = width; finish; finish -= 1)
      {
         uint32_t E[4];
         uint32_t ex, e, i, ke, ki, ex2, ex3, px;
         uint32_t A1 = *(in - nextline - nextline - 1);
         uint32_t B1 = *(in - nextline - nextline);
         uint32_t C1 = *(in - nextline - nextline + 1);
         uint32_t A0 = *(in - nextline - 2);
         uint32_t PA = *(in - nextline - 1);
         uint32_t PB = *(in - nextline);
         uint32_t PC = *(in - nextline + 1);
         uint32_t C4 = *(in - nextline + 2);
         uint32_t D0 = *(in - 2);
         uint32_t PD = *(in - 1);
         uint32_t PE = *(in);
         uint32_t PF = *(in + 1);
         uint32_t F4 = *(in + 2);
         uint32_t G0 = *(in + nextline - 2);
         uint32_t PG = *(in + nextline - 1);
         uint32_t PH = *(in + nextline);
         uint32_t _PI = *(in + nextline + 1);
         uint32_t I4 = *(in + nextline + 2);
         uint32_t G5 = *(in + nextline + nextline - 1);
         uint32_t H5 = *(in + nextline + nextline);
         uint32_t I5 = *(in + nextline + nextline + 1);

         /*
          * Map of the pixels:          A1 B1 C1
          *                          A0 PA PB PC C4
          *                          D0 PD PE PF F4
          *                          G0 PG PH _PI I4
          *                             G5 H5 I5
          */

         twoxbr_function(FILTRO_RGB8888, filt);
      }

      src += src_stride;
      dst += 2 * dst_stride;
   }
}

static void twoxbr_generic_rgb565(void *data, unsigned width, unsigned height,
      int first, int last, uint16_t *src,
      unsigned src_stride, uint16_t *dst, unsigned dst_stride)
{
   unsigned finish;
   struct filter_data *filt = (struct filter_data*)data;
   uint16_t pg_red_mask     = RED_MASK565;
   uint16_t pg_green_mask   = GREEN_MASK565;
   uint16_t pg_blue_mask    = BLUE_MASK565;
   uint16_t pg_lbmask       = PG_LBMASK565;
   unsigned nextline        = (last) ? 0 : src_stride;

   for (; height; height--)
   {
      uint16_t *in  = (uint16_t*)src;
      uint16_t *out = (uint16_t*)dst;

      for (finish = width; finish; finish -= 1)
      {
         uint16_t E[4];
         uint16_t ex, e, i, ke, ki, ex2, ex3, px;
         uint16_t A1 = *(in - nextline - nextline - 1);
         uint16_t B1 = *(in - nextline - nextline);
         uint16_t C1 = *(in - nextline - nextline + 1);
         uint16_t A0 = *(in - nextline - 2);
         uint16_t PA = *(in - nextline - 1);
         uint16_t PB = *(in - nextline);
         uint16_t PC = *(in - nextline + 1);
         uint16_t C4 = *(in - nextline + 2);
         uint16_t D0 = *(in - 2);
         uint16_t PD = *(in - 1);
         uint16_t PE = *(in);
         uint16_t PF = *(in + 1);
         uint16_t F4 = *(in + 2);
         uint16_t G0 = *(in + nextline - 2);
         uint16_t PG = *(in + nextline - 1);
         uint16_t PH = *(in + nextline);
         uint16_t _PI = *(in + nextline + 1);
         uint16_t I4 = *(in + nextline + 2);
         uint16_t G5 = *(in + nextline + nextline - 1);
         uint16_t H5 = *(in + nextline + nextline);
         uint16_t I5 = *(in + nextline + nextline + 1);

         /*
          * Map of the pixels:          A1 B1 C1
          *                          A0 PA PB PC C4
          *                          D0 PD PE PF F4
          *                          G0 PG PH _PI I4
          *                             G5 H5 I5
          */

         twoxbr_function(FILTRO_RGB565, filt);
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

   twoxbr_generic_rgb565(data, width, height,
         thr->first, thr->last, input,
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

   twoxbr_generic_xrgb8888(data, width, height,
         thr->first, thr->last, input,
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
         packets[i].work = twoxbr_work_cb_rgb565;
#if 0
      else if (filt->in_fmt == SOFTFILTER_FMT_RGB4444)
         packets[i].work = twoxbr_work_cb_rgb4444;
#endif
      else if (filt->in_fmt == SOFTFILTER_FMT_XRGB8888)
         packets[i].work = twoxbr_work_cb_xrgb8888;
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

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif

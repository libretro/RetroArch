/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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
   Hyllian's 2xBR v3.3a
   
   Copyright (C) 2011, 2014 Hyllian/Jararaca - sergiogdb@gmail.com
 
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 
*/
 
// Compile: gcc -o twoxbr.so -shared twoxbr.c -std=c99 -O3 -Wall -pedantic -fPIC
 
#include "softfilter.h"
#include <stdlib.h>
 
#ifdef RARCH_INTERNAL
#define softfilter_get_implementation twoxbr_get_implementation
#endif
 
#define TWOXBR_SCALE 2
 
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
 
static void *twoxbr_generic_create(unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd)
{
   (void)simd;
 
   struct filter_data *filt = (struct filter_data*)calloc(1, sizeof(*filt));
   if (!filt)
      return NULL;
   filt->workers = (struct softfilter_thread_data*)calloc(threads, sizeof(struct softfilter_thread_data));
   filt->threads = threads;
   filt->in_fmt  = in_fmt;
   if (!filt->workers)
   {
      free(filt);
      return NULL;
   }
   return filt;
}
 
static void twoxbr_generic_output(void *data, unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   *out_width = width * TWOXBR_SCALE;
   *out_height = height * TWOXBR_SCALE;
}
 
static void twoxbr_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   free(filt->workers);
   free(filt);
}
 
static uint8_t initialized = 0;
uint16_t        RGBtoYUV[65536];
const static uint16_t tbl_5_to_8[32]={0, 8, 16, 25, 33, 41, 49,  58, 66, 74, 82, 90, 99, 107, 115, 123, 132, 140, 148, 156, 165, 173, 181, 189,  197, 206, 214, 222, 230, 239, 247, 255};
const static uint16_t tbl_6_to_8[64]={0, 4, 8, 12, 16, 20, 24,  28, 32, 36, 40, 45, 49, 53, 57, 61, 65, 69, 73, 77, 81, 85, 89, 93, 97, 101,  105, 109, 113, 117, 121, 125, 130, 134, 138, 142, 146, 150, 154, 158, 162, 166,  170, 174, 178, 182, 186, 190, 194, 198, 202, 206, 210, 215, 219, 223, 227, 231,  235, 239, 243, 247, 251, 255};
 
//---------------------------------------------------------------------------------------------------------------------------
 
 
 
#define RED_MASK565   0xF800
#define GREEN_MASK565 0x07E0
#define BLUE_MASK565  0x001F
#define PG_LBMASK565 0xF7DE

#define RED_MASK8888   0x000000FF
#define GREEN_MASK8888 0x0000FF00
#define BLUE_MASK8888  0x00FF0000
#define PG_LBMASK8888  0xFEFEFEFE
#define ALPHA_MASK8888 0xFF000000

 
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


#define df(A, B)\
        abs(RGBtoYUV[A] - RGBtoYUV[B])\
 
#define eq(A, B)\
        (df(A, B) < 155)\
 


float df8(uint32_t A, uint32_t B, uint32_t pg_red_mask, uint32_t pg_green_mask, uint32_t pg_blue_mask)
{
   uint32_t r, g, b;
   uint32_t y, u, v;

   b = abs(((A & pg_blue_mask  )>>16) - ((B & pg_blue_mask  )>> 16));
   g = abs(((A & pg_green_mask)>>8  ) - ((B & pg_green_mask )>>  8));
   r = abs(( A & pg_red_mask        ) - ( B & pg_red_mask         ));

   y = abs(0.299*r + 0.587*g + 0.114*b);
   u = abs(-0.169*r - 0.331*g + 0.500*b);
   v = abs(0.500*r - 0.419*g - 0.081*b);

   return 48*y + 7*u + 6*v;
}

int eq8(uint32_t A, uint32_t B, uint32_t pg_red_mask, uint32_t pg_green_mask, uint32_t pg_blue_mask)
{
    uint32_t r, g, b;
    uint32_t y, u, v;
   
    b = abs(((A & pg_blue_mask  )>>16) - ((B & pg_blue_mask  )>> 16));
    g = abs(((A & pg_green_mask)>>8  ) - ((B & pg_green_mask )>>  8));
    r = abs(( A & pg_red_mask        ) - ( B & pg_red_mask         ));
   
    y = abs(0.299*r + 0.587*g + 0.114*b);
    u = abs(-0.169*r - 0.331*g + 0.500*b);
    v = abs(0.500*r - 0.419*g - 0.081*b);

    return ((48 >= y) && (7 >= u) && (6 >= v)) ? 1 : 0;
}


#define FILTRO_RGB565(PE, PI, PH, PF, PG, PC, PD, PB, PA, G5, C4, G0, D0, C1, B1, F4, I4, H5, I5, A0, A1, N0, N1, N2, N3, pg_red_mask, pg_green_mask, pg_blue_mask) \
     ex   = (PE!=PH && PE!=PF); \
     if ( ex )\
     {\
          e = (df(PE,PC)+df(PE,PG)+df(PI,H5)+df(PI,F4))+(df(PH,PF)<<2); \
          i = (df(PH,PD)+df(PH,I5)+df(PF,I4)+df(PF,PB))+(df(PE,PI)<<2); \
          if ((e<i)  && ( (!eq(PF,PB) && !eq(PF,PC)) || (!eq(PH,PD) && !eq(PH,PG)) || (eq(PE,PI) && ((!eq(PF,F4) && !eq(PF,I4)) || (!eq(PH,H5) && !eq(PH,I5)))) || eq(PE,PG) || eq(PE,PC)) )\
          {\
              ke=df(PF,PG); ki=df(PH,PC); \
              ex2 = (PE!=PC && PB!=PC); ex3 = (PE!=PG && PD!=PG); px = (df(PE,PF) <= df(PE,PH)) ? PF : PH; \
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
               ALPHA_BLEND_128_W( E[N3], ((df(PE,PF) <= df(PE,PH)) ? PF : PH)); \
          }\
     }\
 
#define FILTRO_RGB8888(PE, PI, PH, PF, PG, PC, PD, PB, PA, G5, C4, G0, D0, C1, B1, F4, I4, H5, I5, A0, A1, N0, N1, N2, N3, pg_red_mask, pg_green_mask, pg_blue_mask) \
     ex   = (PE!=PH && PE!=PF); \
     if ( ex )\
     {\
          e = (df8(PE,PC, pg_red_mask, pg_green_mask, pg_blue_mask ) + df8(PE,PG, pg_red_mask, pg_green_mask, pg_blue_mask) + \
                df8(PI,H5, pg_red_mask, pg_green_mask, pg_blue_mask ) + df8(PI,F4, pg_red_mask, pg_green_mask, pg_blue_mask))+(4 * (df8(PH,PF, pg_red_mask, pg_green_mask, pg_blue_mask))); \
          i = (df8(PH,PD, pg_red_mask, pg_green_mask, pg_blue_mask) + df8(PH,I5, pg_red_mask, pg_green_mask, pg_blue_mask) + \
                df8(PF,I4, pg_red_mask, pg_green_mask, pg_blue_mask) + df8(PF,PB, pg_red_mask, pg_green_mask, pg_blue_mask))+(4 * (df8(PE,PI, pg_red_mask, pg_green_mask, pg_blue_mask))); \
          if ((e<i)  && ( (!eq8(PF,PB, pg_red_mask, pg_green_mask, pg_blue_mask) && !eq8(PF,PC, pg_red_mask, pg_green_mask, pg_blue_mask)) || (!eq8(PH,PD, pg_red_mask, pg_green_mask, pg_blue_mask) && !eq8(PH,PG, pg_red_mask, pg_green_mask, pg_blue_mask)) || (eq8(PE,PI, pg_red_mask, pg_green_mask, pg_blue_mask) && ((!eq8(PF,F4, pg_red_mask, pg_green_mask, pg_blue_mask) && !eq8(PF,I4, pg_red_mask, pg_green_mask, pg_blue_mask)) || (!eq8(PH,H5, pg_red_mask, pg_green_mask,pg_blue_mask) && !eq8(PH,I5, pg_red_mask, pg_green_mask, pg_blue_mask)))) || eq8(PE,PG, pg_red_mask, pg_green_mask, pg_blue_mask) || eq8(PE,PC, pg_red_mask, pg_green_mask, pg_blue_mask)) )\
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
 
 
 
#define twoxbr_declare_variables(typename_t, in, nextline) \
         typename_t E[4]; \
         typename_t ex, e, i, ke, ki, ex2, ex3, px; \
         typename_t A1 = *(in - nextline - nextline - 1); \
         typename_t B1 = *(in - nextline - nextline); \
         typename_t C1 = *(in - nextline - nextline + 1); \
         typename_t A0 = *(in - nextline - 2); \
         typename_t PA = *(in - nextline - 1); \
         typename_t PB = *(in - nextline); \
         typename_t PC = *(in - nextline + 1); \
         typename_t C4 = *(in - nextline + 2); \
         typename_t D0 = *(in - 2); \
         typename_t PD = *(in - 1); \
         typename_t PE = *(in); \
         typename_t PF = *(in + 1); \
         typename_t F4 = *(in + 2); \
         typename_t G0 = *(in + nextline - 2); \
         typename_t PG = *(in + nextline - 1); \
         typename_t PH = *(in + nextline); \
         typename_t PI = *(in + nextline + 1); \
         typename_t I4 = *(in + nextline + 2); \
         typename_t G5 = *(in + nextline + nextline - 1); \
         typename_t H5 = *(in + nextline + nextline); \
         typename_t I5 = *(in + nextline + nextline + 1); \
 
#ifndef twoxbr_function
#define twoxbr_function(FILTRO) \
            E[0] = E[1] = E[2] = E[3] = PE;\
            FILTRO(PE, PI, PH, PF, PG, PC, PD, PB, PA, G5, C4, G0, D0, C1, B1, F4, I4, H5, I5, A0, A1, 0, 1, 2, 3, pg_red_mask, pg_green_mask, pg_blue_mask);\
            FILTRO(PE, PC, PF, PB, PI, PA, PH, PD, PG, I4, A1, I5, H5, A0, D0, B1, C1, F4, C4, G5, G0, 2, 0, 3, 1, pg_red_mask, pg_green_mask, pg_blue_mask);\
            FILTRO(PE, PA, PB, PD, PC, PG, PF, PH, PI, C1, G0, C4, F4, G5, H5, D0, A0, B1, A1, I4, I5, 3, 2, 1, 0, pg_red_mask, pg_green_mask, pg_blue_mask);\
            FILTRO(PE, PG, PD, PH, PA, PI, PB, PF, PC, A0, I5, A1, B1, I4, F4, H5, G5, D0, G0, C1, C4, 1, 3, 0, 2, pg_red_mask, pg_green_mask, pg_blue_mask);\
         out[0] = E[0]; \
         out[1] = E[1]; \
         out[dst_stride] = E[2]; \
         out[dst_stride + 1] = E[3]; \
         ++in; \
         out += 2
#endif
 
static void SetupFormat(void)
{
   uint16_t r, g, b, y, u, v;
   uint32_t c;

   for (c = 0; c < 65536; c++)
   {
      r = tbl_5_to_8[(c &   RED_MASK565) >> 11];
      g = tbl_6_to_8[(c & GREEN_MASK565) >>  5];
      b = tbl_5_to_8[(c &  BLUE_MASK565)      ];
      y = ((r<<4) + (g<<5) + (b<<2));
      u = (   -r  - (g<<1) + (b<<2));
      v = ((r<<1) - (g<<1) - (b>>1));
      RGBtoYUV[c] = y + u + v;
   }
}
 
 
static void twoxbr_generic_xrgb8888(unsigned width, unsigned height,
      int first, int last, uint32_t *src,
      unsigned src_stride, uint32_t *dst, unsigned dst_stride)
{
   uint32_t pg_red_mask      = RED_MASK8888;
   uint32_t pg_green_mask    = GREEN_MASK8888;
   uint32_t pg_blue_mask     = BLUE_MASK8888;
   uint32_t pg_lbmask        = PG_LBMASK8888;
   uint32_t pg_alpha_mask    = ALPHA_MASK8888;
   unsigned nextline, finish;
   nextline = (last) ? 0 : src_stride;
   
   if (!initialized)
   {
      initialized = 1;
   }

 
   for (; height; height--)
   {
      uint32_t *in  = (uint32_t*)src;
      uint32_t *out = (uint32_t*)dst;
 
      for (finish = width; finish; finish -= 1)
      {
         twoxbr_declare_variables(uint32_t, in, nextline);
 
         //---------------------------------------
         // Map of the pixels:          A1 B1 C1
         //                          A0 PA PB PC C4
         //                          D0 PD PE PF F4
         //                          G0 PG PH PI I4
         //                             G5 H5 I5
 
         twoxbr_function(FILTRO_RGB8888);
      }
 
      src += src_stride;
      dst += 2 * dst_stride;
   }
}
 
static void twoxbr_generic_rgb565(unsigned width, unsigned height,
      int first, int last, uint16_t *src,
      unsigned src_stride, uint16_t *dst, unsigned dst_stride)
{
   uint16_t pg_red_mask   = RED_MASK565;
   uint16_t pg_green_mask = GREEN_MASK565;
   uint16_t pg_blue_mask  = BLUE_MASK565;
   uint16_t pg_lbmask     = PG_LBMASK565;
   uint16_t pg_alpha_mask;
   unsigned nextline, finish;
   nextline = (last) ? 0 : src_stride;
   
   if (!initialized)
   {
      SetupFormat();
      initialized = 1;
   }
 
   for (; height; height--)
   {
      uint16_t *in  = (uint16_t*)src;
      uint16_t *out = (uint16_t*)dst;
 
      for (finish = width; finish; finish -= 1)
      {
         twoxbr_declare_variables(uint16_t, in, nextline);
 
         //---------------------------------------
         // Map of the pixels:          A1 B1 C1
         //                          A0 PA PB PC C4
         //                          D0 PD PE PF F4
         //                          G0 PG PH PI I4
         //                             G5 H5 I5
 
         twoxbr_function(FILTRO_RGB565);
      }
 
      src += src_stride;
      dst += 2 * dst_stride;
   }
}
 
static void twoxbr_work_cb_rgb565(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   uint16_t *input = (uint16_t*)thr->in_data;
   uint16_t *output = (uint16_t*)thr->out_data;
   unsigned width = thr->width;
   unsigned height = thr->height;
 
   twoxbr_generic_rgb565(width, height,
         thr->first, thr->last, input, thr->in_pitch / SOFTFILTER_BPP_RGB565, output, thr->out_pitch / SOFTFILTER_BPP_RGB565);
}
 
static void twoxbr_work_cb_xrgb8888(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   uint32_t *input = (uint32_t*)thr->in_data;
   uint32_t *output = (uint32_t*)thr->out_data;
   unsigned width = thr->width;
   unsigned height = thr->height;
 
   twoxbr_generic_xrgb8888(width, height,
         thr->first, thr->last, input, thr->in_pitch / SOFTFILTER_BPP_XRGB8888, output, thr->out_pitch / SOFTFILTER_BPP_XRGB8888);
}
 
static void twoxbr_generic_packets(void *data,
      struct softfilter_work_packet *packets,
      void *output, size_t output_stride,
      const void *input, unsigned width, unsigned height, size_t input_stride)
{
   struct filter_data *filt = (struct filter_data*)data;
   unsigned i;
   for (i = 0; i < filt->threads; i++)
   {
      struct softfilter_thread_data *thr = (struct softfilter_thread_data*)&filt->workers[i];
 
      unsigned y_start = (height * i) / filt->threads;
      unsigned y_end = (height * (i + 1)) / filt->threads;
      thr->out_data = (uint8_t*)output + y_start * TWOXBR_SCALE * output_stride;
      thr->in_data = (const uint8_t*)input + y_start * input_stride;
      thr->out_pitch = output_stride;
      thr->in_pitch = input_stride;
      thr->width = width;
      thr->height = y_end - y_start;
 
      // Workers need to know if they can access pixels outside their given buffer.
      thr->first = y_start;
      thr->last = y_end == height;
 
      if (filt->in_fmt == SOFTFILTER_FMT_RGB565)
         packets[i].work = twoxbr_work_cb_rgb565;
      //else if (filt->in_fmt == SOFTFILTER_FMT_RGB4444)
         //packets[i].work = twoxbr_work_cb_rgb4444;
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
   "2xBR",
   SOFTFILTER_API_VERSION,
};
 
const struct softfilter_implementation *softfilter_get_implementation(softfilter_simd_mask_t simd)
{
   (void)simd;
   return &twoxbr_generic;
}
 
#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#endif

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

// Compile: gcc -o twoxbr.so -shared twoxbr.c -std=c99 -O3 -Wall -pedantic -fPIC

#include "softfilter.h"
#include <stdlib.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation twoxbr_get_implementation
#endif

#define TWOXBR_SCALE 2

static uint8_t initialized = 0;

uint16_t        RGBtoYUV[65536];
const static uint16_t tbl_5_to_8[32]={0, 8, 16, 25, 33, 41, 49,  58, 66, 74, 82, 90, 99, 107, 115, 123, 132, 140, 148, 156, 165, 173, 181, 189,  197, 206, 214, 222, 230, 239, 247, 255};
const static uint16_t tbl_6_to_8[64]={0, 4, 8, 12, 16, 20, 24,  28, 32, 36, 40, 45, 49, 53, 57, 61, 65, 69, 73, 77, 81, 85, 89, 93, 97, 101,  105, 109, 113, 117, 121, 125, 130, 134, 138, 142, 146, 150, 154, 158, 162, 166,  170, 174, 178, 182, 186, 190, 194, 198, 202, 206, 210, 215, 219, 223, 227, 231,  235, 239, 243, 247, 251, 255};

uint16_t pg_red_mask;
uint16_t pg_green_mask;
uint16_t pg_blue_mask;
uint16_t pg_lbmask;
 
#define RED_MASK565   0xF800
#define GREEN_MASK565 0x07E0
#define BLUE_MASK565  0x001F
#define PG_LBMASK565  0xF7DE
 
 
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
 
#define df(A, B)\
        abs(RGBtoYUV[A] - RGBtoYUV[B])\
 
#define eq(A, B)\
        (df(A, B) < 155)\
 
#define FILTRO(PE, PI, PH, PF, PG, PC, PD, PB, PA, G5, C4, G0, D0, C1, B1, F4, I4, H5, I5, A0, A1, N0, N1, N2, N3) \
     ex   = (PE!=PH && PE!=PF); \
     if ( ex )\
     {\
          e = (df(PE,PC)+df(PE,PG)+df(PI,H5)+df(PI,F4))+(df(PH,PF)<<2); \
          i = (df(PH,PD)+df(PH,I5)+df(PF,I4)+df(PF,PB))+(df(PE,PI)<<2); \
          if ((e<i)  && ( !eq(PF,PB) && !eq(PH,PD) || eq(PE,PI) && (!eq(PF,I4) && !eq(PH,I5)) || eq(PE,PG) || eq(PE,PC)) )\
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

void filter_2xBR(unsigned width, unsigned height,
      int first, int last,
      const uint16_t *src, unsigned src_stride,
      uint16_t *dst, unsigned dst_stride)
{
   uint16_t e, i, px;
   uint16_t ex, ex2, ex3;
   uint16_t ke, ki;
   unsigned y, x;

   uint16_t *out0 = (uint16_t*)dst;
   uint16_t *out1 = (uint16_t*)(dst + dst_stride);
   uint16_t    *E = (uint16_t*)dst;

   if (!initialized)
   {
      pg_red_mask   = RED_MASK565;
      pg_green_mask = GREEN_MASK565;
      pg_blue_mask  = BLUE_MASK565;
      pg_lbmask     = PG_LBMASK565;
      SetupFormat();
      initialized = 1;
   }

   for (y = 0; y < height; ++y)
   {
      const int prevline  = (((y == 0) && first) ? 0 : src_stride);
      const int prevline2 = (((y <= 1) && first) ? 0 : src_stride);
      const int nextline  = (((y == height - 1) && last) ? 0 : src_stride);
      const int nextline2 = (((y >= height - 2) && last) ? 0 : src_stride);

      for (x = 0; x < width; ++x)
      {
         const uint16_t A0 = (x > 1) ? *(src - prevline - 2) : ((x > 0) ? *(src - prevline - 1) : *(src - prevline));
         const uint16_t D0 = (x > 1) ? *(src - 2) : ((x > 0) ? *(src - 1) : *(src));
         const uint16_t G0 = (x > 1) ? *(src + nextline - 2) : ((x > 0) ? *(src + nextline - 1) : *(src + nextline));

         const uint16_t A1 = (x > 0) ? *(src - prevline2 - 1) : *(src - prevline2);
         const uint16_t PA = (x > 0) ? *(src - prevline  - 1) : *(src - prevline );
         const uint16_t PD = (x > 0) ? *(src - 1) : *(src);
         const uint16_t PG = (x > 0) ? *(src + nextline  - 1) : *(src + nextline );
         const uint16_t G5 = (x > 0) ? *(src + nextline2 - 1) : *(src + nextline2 );

         const uint16_t B1 = *(src - prevline2);
         const uint16_t PB = *(src - prevline );
         const uint16_t PE = *(src);
         const uint16_t PH = *(src + nextline );
         const uint16_t H5 = *(src + nextline2);

         const uint16_t C1 = (x < width - 1) ? *(src - prevline2 + 1) : *(src - prevline2);
         const uint16_t PC = (x < width - 1) ? *(src - prevline  + 1) : *(src - prevline );
         const uint16_t PF = (x < width - 1) ? *(src + 1) : *(src);
         const uint16_t PI = (x < width - 1) ? *(src + nextline  + 1) : *(src + nextline );
         const uint16_t I5 = (x < width - 1) ? *(src + nextline2 + 1) : *(src + nextline2 );

         const uint16_t C4 = (x < width - 2) ? *(src - prevline + 2) : ((x < width - 1) ? *(src - prevline + 1) : *(src - prevline));
         const uint16_t F4 = (x < width - 2) ? *(src + 2) : ((x < width - 1) ? *(src + 1) : *(src));
         const uint16_t I4 = (x < width - 2) ? *(src + nextline + 2) : ((x < width - 1) ? *(src + nextline + 1) : *(src + nextline));

         E[0] = E[1] = E[dst_stride] = E[dst_stride+1] = PE; // 0, 1, 2, 3

         FILTRO(PE, PI, PH, PF, PG, PC, PD, PB, PA, G5, C4, G0, D0, C1, B1, F4, I4, H5, I5, A0, A1, 0, 1, dst_stride, dst_stride+1);
         FILTRO(PE, PC, PF, PB, PI, PA, PH, PD, PG, I4, A1, I5, H5, A0, D0, B1, C1, F4, C4, G5, G0, dst_stride, 0, dst_stride+1, 1);
         FILTRO(PE, PA, PB, PD, PC, PG, PF, PH, PI, C1, G0, C4, F4, G5, H5, D0, A0, B1, A1, I4, I5, dst_stride+1, dst_stride, 1, 0);
         FILTRO(PE, PG, PD, PH, PA, PI, PB, PF, PC, A0, I5, A1, B1, I4, F4, H5, G5, D0, G0, C1, C4, 1, dst_stride+1, 0, dst_stride);            

         E += 2;
      }

      src += src_stride - width;
      out0 += dst_stride + dst_stride - (width << 1);
      out1 += dst_stride + dst_stride - (width << 1);
      E += dst_stride;                         
   }
}

static void twoxbr_generic_rgb565(unsigned width, unsigned height,
      int first, int last,
      const uint16_t *src, unsigned src_stride,
      uint16_t *dst, unsigned dst_stride)
{
   filter_2xBR(width, height, first, last, src, src_stride, dst, dst_stride);
}

static unsigned twoxbr_generic_input_fmts(void)
{
   return SOFTFILTER_FMT_RGB565;
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

static void twoxbr_work_cb_rgb565(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint16_t *input = (const uint16_t*)thr->in_data;
   uint16_t *output = (uint16_t*)thr->out_data;
   unsigned width = thr->width;
   unsigned height = thr->height;

   twoxbr_generic_rgb565(width, height,
         thr->first, thr->last, input, thr->in_pitch / SOFTFILTER_BPP_RGB565, output, thr->out_pitch / SOFTFILTER_BPP_RGB565);
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

/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rtga.c).
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

/* Modified version of stb_image's TGA sources. */

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h> /* ptrdiff_t on osx */
#include <stdlib.h>
#include <string.h>

#include <retro_assert.h>
#include <retro_inline.h>

#include <formats/image.h>
#include <formats/rtga.h>

#define RTGA_COMPUTE_Y(r, g, b) ((uint8_t)((((r) * 77) + ((g) * 150) +  (29 * (b))) >> 8))

struct rtga
{
   uint8_t *buff_data;
   uint32_t *output_image;
};

typedef struct
{
   uint32_t img_x, img_y;
   int img_n, img_out_n;

   int buflen;
   uint8_t buffer_start[128];

   uint8_t *img_buffer;
   uint8_t *img_buffer_end;
   uint8_t *img_buffer_original;
} rtga_context;

static INLINE uint8_t rtga_get8(rtga_context *s)
{
   if (s->img_buffer < s->img_buffer_end)
      return *s->img_buffer++;
   return 0;
}

static void rtga_skip(rtga_context *s, int n)
{
   if (n < 0)
   {
      s->img_buffer = s->img_buffer_end;
      return;
   }
   s->img_buffer += n;
}

static int rtga_get16le(rtga_context *s)
{
   return rtga_get8(s) + (rtga_get8(s) << 8);
}

static unsigned char *rtga_convert_format(
      unsigned char *data,
      int img_n,
      int req_comp,
      unsigned int x,
      unsigned int y)
{
   int i,j;
   unsigned char *good = (unsigned char *) malloc(req_comp * x * y);

   if (!good)
   {
      free(data);
      return NULL;
   }

   for (j=0; j < (int) y; ++j)
   {
      unsigned char *src  = data + j * x * img_n   ;
      unsigned char *dest = good + j * x * req_comp;

      switch (((img_n)*8+(req_comp)))
      {
         case ((1)*8+(2)):
            for(i=x-1; i >= 0; --i, src += 1, dest += 2)
            {
               dest[0]=src[0];
               dest[1]=255;
            }
            break;
         case ((1)*8+(3)):
            for(i=x-1; i >= 0; --i, src += 1, dest += 3)
               dest[0]=dest[1]=dest[2]=src[0];
            break;
         case ((1)*8+(4)):
            for(i=x-1; i >= 0; --i, src += 1, dest += 4)
            {
               dest[0]=dest[1]=dest[2]=src[0];
               dest[3]=255;
            }
            break;
         case ((2)*8+(1)):
            for(i=x-1; i >= 0; --i, src += 2, dest += 1)
               dest[0]=src[0];
            break;
         case ((2)*8+(3)):
            for(i=x-1; i >= 0; --i, src += 2, dest += 3)
               dest[0]=dest[1]=dest[2]=src[0];
            break;
         case ((2)*8+(4)):
            for(i=x-1; i >= 0; --i, src += 2, dest += 4)
            {
               dest[0]=dest[1]=dest[2]=src[0];
               dest[3]=src[1];
            }
            break;
         case ((3)*8+(4)):
            for(i=x-1; i >= 0; --i, src += 3, dest += 4)
            {
               dest[0]=src[0];
               dest[1]=src[1];
               dest[2]=src[2];
               dest[3]=255;
            }
            break;
         case ((3)*8+(1)):
            for(i=x-1; i >= 0; --i, src += 3, dest += 1)
               dest[0] = RTGA_COMPUTE_Y(src[0],src[1],src[2]);
            break;
         case ((3)*8+(2)):
            for(i=x-1; i >= 0; --i, src += 3, dest += 2)
            {
               dest[0] = RTGA_COMPUTE_Y(src[0],src[1],src[2]);
               dest[1] = 255;
            }
            break;
         case ((4)*8+(1)):
            for(i=x-1; i >= 0; --i, src += 4, dest += 1)
               dest[0] = RTGA_COMPUTE_Y(src[0],src[1],src[2]);
            break;
         case ((4)*8+(2)):
            for(i=x-1; i >= 0; --i, src += 4, dest += 2)
            {
               dest[0] = RTGA_COMPUTE_Y(src[0],src[1],src[2]);
               dest[1] = src[3];
            }
            break;
         case ((4)*8+(3)):
            for(i=x-1; i >= 0; --i, src += 4, dest += 3)
            {
               dest[0]=src[0];
               dest[1]=src[1];
               dest[2]=src[2];
            }
            break;
         default:
            break;
      }
   }

   free(data);
   return good;
}

static uint8_t *rtga_tga_load(rtga_context *s,
      unsigned *x, unsigned *y, int *comp, int req_comp)
{
   /* Read in the TGA header stuff */
   int tga_offset          = rtga_get8(s);
   int tga_indexed         = rtga_get8(s);
   int tga_image_type      = rtga_get8(s);
   int tga_is_RLE          = 0;
   int tga_palette_start   = rtga_get16le(s);
   int tga_palette_len     = rtga_get16le(s);
   int tga_palette_bits    = rtga_get8(s);
   int tga_x_origin        = rtga_get16le(s);
   int tga_y_origin        = rtga_get16le(s);
   int tga_width           = rtga_get16le(s);
   int tga_height          = rtga_get16le(s);
   int tga_bits_per_pixel  = rtga_get8(s);
   int tga_comp            = tga_bits_per_pixel / 8;
   int tga_inverted        = rtga_get8(s);

   /*   image data */
   unsigned char *tga_data = NULL;

   (void)tga_palette_start;
   (void)tga_x_origin;
   (void)tga_y_origin;

   /*   do a tiny bit of precessing */
   if (tga_image_type >= 8)
   {
      tga_image_type -= 8;
      tga_is_RLE = 1;
   }

   /* int tga_alpha_bits = tga_inverted & 15; */
   tga_inverted = 1 - ((tga_inverted >> 5) & 1);

   /*   error check */
   if (
         (tga_width < 1) || (tga_height < 1) ||
         (tga_image_type < 1) || (tga_image_type > 3) ||
         (
          (tga_bits_per_pixel != 8)  && (tga_bits_per_pixel != 16) &&
          (tga_bits_per_pixel != 24) && (tga_bits_per_pixel != 32)
         )
      )
      return NULL; /* we don't report this as a bad TGA because we don't even know if it's TGA */

   /*   If paletted, then we will use the number of bits from the palette */
   if (tga_indexed)
      tga_comp = tga_palette_bits / 8;

   /*   TGA info */
   *x = tga_width;
   *y = tga_height;
   if (comp)
      *comp = tga_comp;

   tga_data = (unsigned char*)malloc((size_t)tga_width * tga_height * tga_comp);
   if (!tga_data)
      return NULL;

   /* skip to the data's starting position (offset usually = 0) */
   rtga_skip(s, tga_offset );

   if (!tga_indexed && !tga_is_RLE)
   {
      int i;
      for (i=0; i < tga_height; ++i)
      {
         int _y           = tga_inverted ? (tga_height -i - 1) : i;
         uint8_t *tga_row = tga_data + _y * tga_width * tga_comp;
         int n            = tga_width * tga_comp;

         if (s->img_buffer + n <= s->img_buffer_end)
         {
            memcpy(tga_row, s->img_buffer, n);
            s->img_buffer += n;
         }
      }
   }
   else
   {
      int i, j;
      int RLE_repeating          = 0;
      int RLE_count              = 0;
      int read_next_pixel        = 1;
      unsigned char raw_data[4]  = {0};
      unsigned char *tga_palette = NULL;

      /*   Do I need to load a palette? */
      if (tga_indexed)
      {
         int n;
         /* Any data to skip? (offset usually = 0) */
         rtga_skip(s, tga_palette_start );
         /* Load the palette */
         tga_palette = (unsigned char*)malloc(tga_palette_len * tga_palette_bits / 8);

         if (!tga_palette)
         {
            free(tga_data);
            return NULL;
         }

         n = tga_palette_len * tga_palette_bits / 8;

         if (s->img_buffer+n <= s->img_buffer_end)
         {
            memcpy(tga_palette, s->img_buffer, n);
            s->img_buffer += n;
         }
         else
         {
            free(tga_data);
            free(tga_palette);
            return NULL;
         }
      }

      /*   load the data */
      for (i=0; i < tga_width * tga_height; ++i)
      {
         /*   if I'm in RLE mode, do I need to get a RLE rtga_png chunk? */
         if (tga_is_RLE)
         {
            if (RLE_count == 0)
            {
               /*   yep, get the next byte as a RLE command */
               int RLE_cmd     = rtga_get8(s);
               RLE_count       = 1 + (RLE_cmd & 127);
               RLE_repeating   = RLE_cmd >> 7;
               read_next_pixel = 1;
            }
            else if (!RLE_repeating)
               read_next_pixel = 1;
         }
         else
            read_next_pixel = 1;

         /*   OK, if I need to read a pixel, do it now */
         if (read_next_pixel)
         {
            /*   load however much data we did have */
            if (tga_indexed)
            {
               /*   read in 1 byte, then perform the lookup */
               int pal_idx = rtga_get8(s);
               if (pal_idx >= tga_palette_len) /* invalid index */
                  pal_idx = 0;
               pal_idx *= tga_bits_per_pixel / 8;
               for (j = 0; j*8 < tga_bits_per_pixel; ++j)
                  raw_data[j] = tga_palette[pal_idx+j];
            }
            else
            {
               /* read in the data raw */
               for (j = 0; j*8 < tga_bits_per_pixel; ++j)
                  raw_data[j] = rtga_get8(s);
            }

            /*   clear the reading flag for the next pixel */
            read_next_pixel = 0;
         } /* end of reading a pixel */

         /* copy data */
         for (j = 0; j < tga_comp; ++j)
            tga_data[i*tga_comp+j] = raw_data[j];

         /*   in case we're in RLE mode, keep counting down */
         --RLE_count;
      }

      /*   do I need to invert the image? */
      if (tga_inverted)
      {
         for (j = 0; j*2 < tga_height; ++j)
         {
            int index1 = j * tga_width * tga_comp;
            int index2 = (tga_height - 1 - j) * tga_width * tga_comp;

            for (i = tga_width * tga_comp; i > 0; --i)
            {
               unsigned char temp = tga_data[index1];
               tga_data[index1] = tga_data[index2];
               tga_data[index2] = temp;
               ++index1;
               ++index2;
            }
         }
      }

      /* Clear my palette, if I had one */
      if (tga_palette)
         free(tga_palette);
   }

   /* swap RGB */
   if (tga_comp >= 3)
   {
      int i;
      unsigned char* tga_pixel = tga_data;

      for (i = 0; i < tga_width * tga_height; ++i)
      {
         unsigned char temp  = tga_pixel[0];
         tga_pixel[0]        = tga_pixel[2];
         tga_pixel[2]        = temp;
         tga_pixel          += tga_comp;
      }
   }

   /* convert to target component count */
   if (     (req_comp)
         && (req_comp >= 1 && req_comp <= 4)
         && (req_comp != tga_comp))
   {
      tga_data = rtga_convert_format(tga_data, tga_comp, req_comp, tga_width, tga_height);
   }

   return tga_data;
}

static uint8_t *rtga_load_from_memory(uint8_t const *buffer, int len,
      unsigned *x, unsigned *y, int *comp, int req_comp)
{
   rtga_context s;

   s.img_buffer          = (uint8_t *)buffer;
   s.img_buffer_original = (uint8_t *) buffer;
   s.img_buffer_end      = (uint8_t *) buffer+len;

   return rtga_tga_load(&s,x,y,comp,req_comp);
}

int rtga_process_image(rtga_t *rtga, void **buf_data,
      size_t size, unsigned *width, unsigned *height)
{
   int comp;
   unsigned size_tex     = 0;

   if (!rtga)
      return IMAGE_PROCESS_ERROR;

   rtga->output_image   = (uint32_t*)rtga_load_from_memory(rtga->buff_data,
                           (int)size, width, height, &comp, 4);
   *buf_data             = rtga->output_image;
   size_tex              = (*width) * (*height);

   /* Convert RGBA to ARGB */
   while(size_tex--)
   {
      unsigned int texel = rtga->output_image[size_tex];
      unsigned int A     = texel & 0xFF000000;
      unsigned int B     = texel & 0x00FF0000;
      unsigned int G     = texel & 0x0000FF00;
      unsigned int R     = texel & 0x000000FF;
      ((unsigned int*)rtga->output_image)[size_tex] = A | (R << 16) | G | (B >> 16);
   };

   return IMAGE_PROCESS_END;
}

bool rtga_set_buf_ptr(rtga_t *rtga, void *data)
{
   if (!rtga)
      return false;

   rtga->buff_data = (uint8_t*)data;

   return true;
}

void rtga_free(rtga_t *rtga)
{
   if (!rtga)
      return;

   free(rtga);
}

rtga_t *rtga_alloc(void)
{
   rtga_t *rtga = (rtga_t*)calloc(1, sizeof(*rtga));
   if (!rtga)
      return NULL;
   return rtga;
}

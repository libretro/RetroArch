/* Copyright  (C) 2010-2020 The RetroArch team
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

#include <retro_inline.h>

#include <formats/image.h>
#include <formats/rtga.h>

struct rtga
{
   uint8_t *buff_data;
   uint32_t *output_image;
};

typedef struct
{
   uint8_t *img_buffer;
   uint8_t *img_buffer_end;
   uint8_t *img_buffer_original;
   int buflen;
   int img_n, img_out_n;
   uint32_t img_x, img_y;
   uint8_t buffer_start[128];
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

static uint32_t *rtga_tga_load(rtga_context *s,
      unsigned *x, unsigned *y, int *comp,
      bool supports_rgba)
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

   /* Output buffer — always 32bpp ARGB or ABGR */
   uint32_t *output        = NULL;

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
      return NULL;

   /*   If paletted, then we will use the number of bits from the palette */
   if (tga_indexed)
      tga_comp = tga_palette_bits / 8;

   /*   TGA info */
   *x = tga_width;
   *y = tga_height;
   if (comp)
      *comp = tga_comp;

   output = (uint32_t*)malloc((size_t)tga_width * tga_height * sizeof(uint32_t));
   if (!output)
      return NULL;

   /* skip to the data's starting position (offset usually = 0) */
   rtga_skip(s, tga_offset);

   /* --- Decode all pixels directly into uint32 output --- */
   {
      int i, j;
      int RLE_repeating          = 0;
      int RLE_count              = 0;
      int read_next_pixel        = 1;
      unsigned char raw_data[4]  = {0};
      unsigned char *tga_palette = NULL;
      int pixel_count            = tga_width * tga_height;

      /* Load palette if indexed */
      if (tga_indexed)
      {
         int n;
         rtga_skip(s, tga_palette_start);
         tga_palette = (unsigned char*)malloc(tga_palette_len * tga_palette_bits / 8);
         if (!tga_palette)
         {
            free(output);
            return NULL;
         }
         n = tga_palette_len * tga_palette_bits / 8;
         if (s->img_buffer + n <= s->img_buffer_end)
         {
            memcpy(tga_palette, s->img_buffer, n);
            s->img_buffer += n;
         }
         else
         {
            free(output);
            free(tga_palette);
            return NULL;
         }
      }

      for (i = 0; i < pixel_count; ++i)
      {
         int src_row, dst_row, px_in_row;
         uint32_t pixel;
         unsigned char b, g, r, a;

         /* RLE handling */
         if (tga_is_RLE)
         {
            if (RLE_count == 0)
            {
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

         /* Read raw pixel data */
         if (read_next_pixel)
         {
            if (tga_indexed)
            {
               int pal_idx = rtga_get8(s);
               if (pal_idx >= tga_palette_len)
                  pal_idx = 0;
               pal_idx *= tga_palette_bits / 8;
               for (j = 0; j * 8 < tga_palette_bits; ++j)
                  raw_data[j] = tga_palette[pal_idx + j];
            }
            else
            {
               j = 0;
               switch (tga_bits_per_pixel)
               {
                  case 32:
                     raw_data[j++] = rtga_get8(s); /* fallthrough */
                  case 24:
                     raw_data[j++] = rtga_get8(s); /* fallthrough */
                  case 16:
                     raw_data[j++] = rtga_get8(s); /* fallthrough */
                  case  8:
                     raw_data[j++] = rtga_get8(s);
               }
            }
            read_next_pixel = 0;
         }

         /* Assemble pixel in correct byte order directly.
          * TGA stores pixels as BGR(A). We convert to the target
          * format here — no separate swap pass needed.
          *
          * For tga_comp < 3 (grayscale), treat as R=G=B=raw_data[0].
          * For tga_comp == 3 (24-bit), alpha = 0xFF.
          * For tga_comp == 4 (32-bit), alpha from raw_data[3]. */
         if (tga_comp >= 3)
         {
            b = raw_data[0];
            g = raw_data[1];
            r = raw_data[2];
            a = (tga_comp >= 4) ? raw_data[3] : 0xFF;
         }
         else
         {
            /* Grayscale or 16-bit — R=G=B */
            r = g = b = raw_data[0];
            a = (tga_comp >= 2) ? raw_data[1] : 0xFF;
         }

         /* Pack to uint32:
          * supports_rgba → ABGR: (a<<24)|(b<<16)|(g<<8)|r
          * !supports_rgba → ARGB: (a<<24)|(r<<16)|(g<<8)|b */
         if (supports_rgba)
            pixel = ((uint32_t)a << 24) | ((uint32_t)b << 16)
                  | ((uint32_t)g << 8)  | (uint32_t)r;
         else
            pixel = ((uint32_t)a << 24) | ((uint32_t)r << 16)
                  | ((uint32_t)g << 8)  | (uint32_t)b;

         /* Write to correct position, handling row flip inline.
          * For non-RLE non-inverted images, rows are already in
          * top-to-bottom order. For inverted (the common case in TGA),
          * flip the row index. */
         src_row    = i / tga_width;
         px_in_row  = i % tga_width;
         dst_row    = tga_inverted ? (tga_height - 1 - src_row) : src_row;
         output[dst_row * tga_width + px_in_row] = pixel;

         --RLE_count;
      }

      if (tga_palette)
         free(tga_palette);
   }

   return output;
}

static uint32_t *rtga_load_from_memory(uint8_t const *buffer, int len,
      unsigned *x, unsigned *y, int *comp, bool supports_rgba)
{
   rtga_context s;

   s.img_buffer          = (uint8_t *)buffer;
   s.img_buffer_original = (uint8_t *) buffer;
   s.img_buffer_end      = (uint8_t *) buffer+len;

   return rtga_tga_load(&s, x, y, comp, supports_rgba);
}

int rtga_process_image(rtga_t *rtga, void **buf_data,
      size_t size, unsigned *width, unsigned *height,
      bool supports_rgba)
{
   int comp;

   if (!rtga)
      return IMAGE_PROCESS_ERROR;

   rtga->output_image   = rtga_load_from_memory(rtga->buff_data,
                           (int)size, width, height, &comp, supports_rgba);
   *buf_data             = rtga->output_image;

   if (!rtga->output_image)
      return IMAGE_PROCESS_ERROR;

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

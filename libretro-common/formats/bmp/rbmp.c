/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rbmp.c).
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

/* Modified version of stb_image's BMP sources. */

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h> /* ptrdiff_t on osx */
#include <stdlib.h>
#include <string.h>

#include <retro_inline.h>

#include <formats/image.h>
#include <formats/rbmp.h>

/* truncate int to byte without warnings */
#define RBMP_BYTECAST(x)  ((unsigned char) ((x) & 255))

typedef struct
{
   unsigned char *img_buffer;
   unsigned char *img_buffer_end;
   unsigned char *img_buffer_original;
   int img_n;
   int img_out_n;
   int buflen;
   uint32_t img_x;
   uint32_t img_y;
   unsigned char buffer_start[128];
} rbmp_context;

struct rbmp
{
   uint8_t *buff_data;
   uint32_t *output_image;
};

static INLINE unsigned char rbmp_get8(rbmp_context *s)
{
   if (s->img_buffer < s->img_buffer_end)
      return *s->img_buffer++;

   return 0;
}

static void rbmp_skip(rbmp_context *s, int n)
{
   if (n < 0)
   {
      s->img_buffer = s->img_buffer_end;
      return;
   }

   s->img_buffer += n;
}

static int rbmp_get16le(rbmp_context *s)
{
   return rbmp_get8(s) + (rbmp_get8(s) << 8);
}

#define RBMP_GET32LE(s) (rbmp_get16le(s) + (rbmp_get16le(s) << 16))

/* Microsoft/Windows BMP image */

/* returns 0..31 for the highest set bit */
static int rbmp_high_bit(unsigned int z)
{
   int n=0;
   if (z == 0)
      return -1;

   if (z >= 0x10000)
   {
      n  += 16;
      z >>= 16;
   }
   if (z >= 0x00100)
   {
      n  +=  8;
      z >>=  8;
   }
   if (z >= 0x00010)
   {
      n  +=  4;
      z >>=  4;
   }
   if (z >= 0x00004)
   {
      n  +=  2;
      z >>=  2;
   }
   if (z >= 0x00002)
      n +=  1;
   return n;
}

static int rbmp_bitcount(unsigned int a)
{
   a = (a & 0x55555555) + ((a >>  1) & 0x55555555); /* max 2 */
   a = (a & 0x33333333) + ((a >>  2) & 0x33333333); /* max 4 */
   a = (a + (a >> 4)) & 0x0f0f0f0f; /* max 8 per 4, now 8 bits */
   a = (a + (a >> 8));              /* max 16 per 8 bits */
   a = (a + (a >> 16));             /* max 32 per 8 bits */
   return a & 0xff;
}

static int rbmp_shiftsigned(int v, int shift, int bits)
{
   int ret;
   int z = bits;

   if (shift < 0)
      v <<= -shift;
   else
      v >>= shift;

   ret = v;

   while (z < 8)
   {
      ret += v >> z;
      z   += bits;
   }
   return ret;
}

static uint32_t *rbmp_bmp_load(rbmp_context *s, unsigned *x, unsigned *y,
      int *comp, bool supports_rgba)
{
   uint32_t *output;
   int bpp, flip_vertically, pad, offset, hsz;
   int psize=0,i,j,width;
   unsigned int mr=0,mg=0,mb=0,ma=0;

   /* Corrupt BMP? */
   if (rbmp_get8(s) != 'B' || rbmp_get8(s) != 'M')
      return 0;

   /* discard filesize */
   rbmp_get16le(s);
   rbmp_get16le(s);
   /* discard reserved */
   rbmp_get16le(s);
   rbmp_get16le(s);

   offset = (uint32_t)RBMP_GET32LE(s);
   hsz    = (uint32_t)RBMP_GET32LE(s);

   /* BMP type not supported? */
   if (hsz != 12 && hsz != 40 && hsz != 56 && hsz != 108 && hsz != 124)
      return 0;

   if (hsz == 12)
   {
      s->img_x = rbmp_get16le(s);
      s->img_y = rbmp_get16le(s);
   }
   else
   {
      s->img_x = (uint32_t)RBMP_GET32LE(s);
      s->img_y = (uint32_t)RBMP_GET32LE(s);
   }

   /* Bad BMP? */
   if (rbmp_get16le(s) != 1)
      return 0;

   bpp = rbmp_get16le(s);

   /* BMP 1-bit type not supported? */
   if (bpp == 1)
      return 0;

   flip_vertically = ((int) s->img_y) > 0;
   s->img_y        = abs((int) s->img_y);

   if (hsz == 12)
   {
      if (bpp < 24)
         psize = (offset - 14 - 24) / 3;
   }
   else
   {
      int compress = (uint32_t)RBMP_GET32LE(s);

      /* BMP RLE type not supported? */
      if (compress == 1 || compress == 2)
         return 0;

      /* discard sizeof */
      rbmp_get16le(s);
      rbmp_get16le(s);
      /* discard hres */
      rbmp_get16le(s);
      rbmp_get16le(s);
      /* discard vres */
      rbmp_get16le(s);
      rbmp_get16le(s);
      /* discard colors used */
      rbmp_get16le(s);
      rbmp_get16le(s);
      /* discard max important */
      rbmp_get16le(s);
      rbmp_get16le(s);

      if (hsz == 40 || hsz == 56)
      {
         if (hsz == 56)
         {
            rbmp_get16le(s);
            rbmp_get16le(s);
            rbmp_get16le(s);
            rbmp_get16le(s);
            rbmp_get16le(s);
            rbmp_get16le(s);
            rbmp_get16le(s);
            rbmp_get16le(s);
         }
         if (bpp == 16 || bpp == 32)
         {
            switch (compress)
            {
               case 0:
                  break;
               case 3:
                  mr = (uint32_t)RBMP_GET32LE(s);
                  mg = (uint32_t)RBMP_GET32LE(s);
                  mb = (uint32_t)RBMP_GET32LE(s);
                  /* not documented, but generated by
                   * Photoshop and handled by MS Paint */
                  /* Bad BMP ?*/
                  if (mr == mg && mg == mb)
                     return 0;
                  break;
               default:
                  break;
            }

            /* Bad BMP? */
            return 0;
         }
      }
      else
      {
         mr = (uint32_t)RBMP_GET32LE(s);
         mg = (uint32_t)RBMP_GET32LE(s);
         mb = (uint32_t)RBMP_GET32LE(s);
         ma = (uint32_t)RBMP_GET32LE(s);
         /* Discard color space */
         rbmp_get16le(s);
         rbmp_get16le(s);
         for (i = 0; i < 12; ++i)
         {
            /* Discard color space parameters */
            rbmp_get16le(s);
            rbmp_get16le(s);
         }
         if (hsz == 124)
         {
            /* Discard rendering intent */
            rbmp_get16le(s);
            rbmp_get16le(s);
            /* Discard offset of profile data */
            rbmp_get16le(s);
            rbmp_get16le(s);
            /* Discard size of profile data */
            rbmp_get16le(s);
            rbmp_get16le(s);
            /* Discard reserved */
            rbmp_get16le(s);
            rbmp_get16le(s);
         }
      }
      if (bpp < 16)
         psize = (offset - 14 - hsz) >> 2;
   }
   s->img_n = ma ? 4 : 3;

   /* Always output as uint32 (4 bytes per pixel) */
   output = (uint32_t*)malloc(s->img_x * s->img_y * sizeof(uint32_t));
   if (!output)
      return 0;

   if (bpp < 16)
   {
      /* Palette mode: pre-convert palette to uint32 in target byte order */
      uint32_t pal32[256];
      int z = 0;

      if (psize == 0 || psize > 256)
      {
         free(output);
         return 0;
      }

      for (i = 0; i < psize; ++i)
      {
         unsigned char b = rbmp_get8(s);
         unsigned char g = rbmp_get8(s);
         unsigned char r = rbmp_get8(s);
         if (hsz != 12)
            rbmp_get8(s);
         if (supports_rgba)
            pal32[i] = 0xFF000000u | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
         else
            pal32[i] = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
      }

      rbmp_skip(s, offset - 14 - hsz - psize * (hsz == 12 ? 3 : 4));
      if (bpp == 4)
         width = (s->img_x + 1) >> 1;
      else if (bpp == 8)
         width = s->img_x;
      else
      {
         free(output);
         return 0;
      }

      pad = (-width)&3;
      for (j = 0; j < (int)s->img_y; ++j)
      {
         int dst_row = flip_vertically ? (int)(s->img_y - 1 - j) : j;
         uint32_t *dst = output + dst_row * s->img_x;
         int col = 0;

         for (i = 0; i < (int)s->img_x; i += 2)
         {
            int v  = rbmp_get8(s);
            int v2 = 0;
            if (bpp == 4)
            {
               v2  = v & 15;
               v >>= 4;
            }
            dst[col++] = pal32[v];

            if (i + 1 == (int)s->img_x)
               break;

            v = (bpp == 8) ? rbmp_get8(s) : v2;
            dst[col++] = pal32[v];
         }
         rbmp_skip(s, pad);
      }
   }
   else
   {
      int rshift = 0;
      int gshift = 0;
      int bshift = 0;
      int ashift = 0;
      int rcount = 0;
      int gcount = 0;
      int bcount = 0;
      int acount = 0;
      int easy   = 0;

      rbmp_skip(s, offset - 14 - hsz);

      if (bpp == 24)
         width = 3 * s->img_x;
      else if (bpp == 16)
         width = 2 * s->img_x;
      else /* bpp = 32 and pad = 0 */
         width = 0;

      pad = (-width) & 3;

      switch (bpp)
      {
         case 24:
            easy = 1;
            break;
         case 32:
            if (mb == 0xff && mg == 0xff00 && mr == 0x00ff0000 && ma == 0xff000000)
               easy = 2;
            break;
         default:
            break;
      }

      if (!easy)
      {
         /* Corrupt BMP? */
         if (!mr || !mg || !mb)
         {
            free(output);
            return 0;
         }

         rshift = rbmp_high_bit(mr)-7;
         rcount = rbmp_bitcount(mr);
         gshift = rbmp_high_bit(mg)-7;
         gcount = rbmp_bitcount(mg);
         bshift = rbmp_high_bit(mb)-7;
         bcount = rbmp_bitcount(mb);
         ashift = rbmp_high_bit(ma)-7;
         acount = rbmp_bitcount(ma);
      }

      for (j = 0; j < (int)s->img_y; ++j)
      {
         int dst_row = flip_vertically ? (int)(s->img_y - 1 - j) : j;
         uint32_t *dst = output + dst_row * s->img_x;

         if (easy)
         {
            if (easy == 2)
            {
               /* 32bpp BGRA — can read directly */
               if (!supports_rgba)
               {
                  /* BMP bytes [B,G,R,A] = uint32 ARGB on LE — memcpy */
                  int bytes_needed = s->img_x * 4;
                  if (s->img_buffer + bytes_needed <= s->img_buffer_end)
                  {
                     memcpy(dst, s->img_buffer, bytes_needed);
                     s->img_buffer += bytes_needed;
                  }
               }
               else
               {
                  /* Need ABGR — swap R and B during read */
                  for (i = 0; i < (int)s->img_x; ++i)
                  {
                     unsigned char b = rbmp_get8(s);
                     unsigned char g = rbmp_get8(s);
                     unsigned char r = rbmp_get8(s);
                     unsigned char a = rbmp_get8(s);
                     dst[i] = ((uint32_t)a << 24) | ((uint32_t)b << 16)
                            | ((uint32_t)g << 8)  | (uint32_t)r;
                  }
               }
            }
            else
            {
               /* 24bpp BGR */
               if (!supports_rgba)
               {
                  /* Need ARGB */
                  for (i = 0; i < (int)s->img_x; ++i)
                  {
                     unsigned char b = rbmp_get8(s);
                     unsigned char g = rbmp_get8(s);
                     unsigned char r = rbmp_get8(s);
                     dst[i] = 0xFF000000u | ((uint32_t)r << 16)
                            | ((uint32_t)g << 8)  | (uint32_t)b;
                  }
               }
               else
               {
                  /* Need ABGR */
                  for (i = 0; i < (int)s->img_x; ++i)
                  {
                     unsigned char b = rbmp_get8(s);
                     unsigned char g = rbmp_get8(s);
                     unsigned char r = rbmp_get8(s);
                     dst[i] = 0xFF000000u | ((uint32_t)b << 16)
                            | ((uint32_t)g << 8)  | (uint32_t)r;
                  }
               }
            }
         }
         else
         {
            /* Bitmask mode (16bpp or non-standard 32bpp) */
            for (i = 0; i < (int)s->img_x; ++i)
            {
               unsigned char r, g, b, a;
               uint32_t v = (bpp == 16)
                  ? (uint32_t)rbmp_get16le(s)
                  : (uint32_t)RBMP_GET32LE(s);
               r = RBMP_BYTECAST(rbmp_shiftsigned(v & mr, rshift, rcount));
               g = RBMP_BYTECAST(rbmp_shiftsigned(v & mg, gshift, gcount));
               b = RBMP_BYTECAST(rbmp_shiftsigned(v & mb, bshift, bcount));
               a = ma ? RBMP_BYTECAST(rbmp_shiftsigned(v & ma, ashift, acount)) : 255;

               if (supports_rgba)
                  dst[i] = ((uint32_t)a << 24) | ((uint32_t)b << 16)
                         | ((uint32_t)g << 8)  | (uint32_t)r;
               else
                  dst[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16)
                         | ((uint32_t)g << 8)  | (uint32_t)b;
            }
         }
         rbmp_skip(s, pad);
      }
   }

   *x = s->img_x;
   *y = s->img_y;

   if (comp)
      *comp = s->img_n;

   return output;
}

static uint32_t *rbmp_load_from_memory(unsigned char const *buffer, int len,
      unsigned *x, unsigned *y, int *comp, bool supports_rgba)
{
   rbmp_context s;

   s.img_buffer          = (unsigned char*)buffer;
   s.img_buffer_original = (unsigned char*)buffer;
   s.img_buffer_end      = (unsigned char*)buffer+len;

   return rbmp_bmp_load(&s, x, y, comp, supports_rgba);
}

int rbmp_process_image(rbmp_t *rbmp, void **buf_data,
      size_t size, unsigned *width, unsigned *height,
      bool supports_rgba)
{
   int comp;

   if (!rbmp)
      return IMAGE_PROCESS_ERROR;

   rbmp->output_image   = rbmp_load_from_memory(rbmp->buff_data,
                           (int)size, width, height, &comp, supports_rgba);
   *buf_data             = rbmp->output_image;

   if (!rbmp->output_image)
      return IMAGE_PROCESS_ERROR;

   return IMAGE_PROCESS_END;
}

bool rbmp_set_buf_ptr(rbmp_t *rbmp, void *data)
{
   if (!rbmp)
      return false;

   rbmp->buff_data = (uint8_t*)data;

   return true;
}

void rbmp_free(rbmp_t *rbmp)
{
   if (!rbmp)
      return;

   free(rbmp);
}

rbmp_t *rbmp_alloc(void)
{
   rbmp_t *rbmp = (rbmp_t*)calloc(1, sizeof(*rbmp));
   if (!rbmp)
      return NULL;
   return rbmp;
}

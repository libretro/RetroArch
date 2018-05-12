/* Copyright  (C) 2010-2018 The RetroArch team
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
#define RBMP__BYTECAST(x)  ((unsigned char) ((x) & 255))

#define RBMP_COMPUTE_Y(r, g, b) ((unsigned char) ((((r) * 77) + ((g) * 150) +  (29 * (b))) >> 8))

typedef struct
{
   uint32_t img_x;
   uint32_t img_y;
   int img_n;
   int img_out_n;

   int buflen;
   unsigned char buffer_start[128];

   unsigned char *img_buffer;
   unsigned char *img_buffer_end;
   unsigned char *img_buffer_original;
} rbmp__context;

struct rbmp
{
   uint8_t *buff_data;
   uint32_t *output_image;
};

static INLINE unsigned char rbmp__get8(rbmp__context *s)
{
   if (s->img_buffer < s->img_buffer_end)
      return *s->img_buffer++;

   return 0;
}

static void rbmp__skip(rbmp__context *s, int n)
{
   if (n < 0)
   {
      s->img_buffer = s->img_buffer_end;
      return;
   }

   s->img_buffer += n;
}

static int rbmp__get16le(rbmp__context *s)
{
   int z = rbmp__get8(s);
   return z + (rbmp__get8(s) << 8);
}

static uint32_t rbmp__get32le(rbmp__context *s)
{
   uint32_t z = rbmp__get16le(s);
   return z + (rbmp__get16le(s) << 16);
}

static unsigned char *rbmp__convert_format(
      unsigned char *data,
      int img_n,
      int req_comp,
      unsigned int x,
      unsigned int y)
{
   int i,j;
   unsigned char *good = (unsigned char *)malloc(req_comp * x * y);

   if (!good)
      return NULL;

   for (j=0; j < (int) y; ++j)
   {
      unsigned char *src  = data + j * x * img_n   ;
      unsigned char *dest = good + j * x * req_comp;

      switch (((img_n)*8+(req_comp)))
      {
         case ((1)*8+(2)):
            for(i=x-1; i >= 0; --i, src += 1, dest += 2)
               dest[0]=src[0], dest[1]=255;
            break;
         case ((1)*8+(3)):
            for(i=x-1; i >= 0; --i, src += 1, dest += 3)
               dest[0]=dest[1]=dest[2]=src[0];
            break;
         case ((1)*8+(4)):
            for(i=x-1; i >= 0; --i, src += 1, dest += 4)
               dest[0]=dest[1]=dest[2]=src[0], dest[3]=255;
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
               dest[0]=dest[1]=dest[2]=src[0], dest[3]=src[1];
            break;
         case ((3)*8+(4)):
            for(i=x-1; i >= 0; --i, src += 3, dest += 4)
               dest[0]=src[0],dest[1]=src[1],dest[2]=src[2],dest[3]=255;
            break;
         case ((3)*8+(1)):
            for(i=x-1; i >= 0; --i, src += 3, dest += 1)
               dest[0] = RBMP_COMPUTE_Y(src[0],src[1],src[2]);
            break;
         case ((3)*8+(2)):
            for(i=x-1; i >= 0; --i, src += 3, dest += 2)
               dest[0] = RBMP_COMPUTE_Y(src[0],src[1],src[2]), dest[1] = 255;
            break;
         case ((4)*8+(1)):
            for(i=x-1; i >= 0; --i, src += 4, dest += 1)
               dest[0] = RBMP_COMPUTE_Y(src[0],src[1],src[2]);
            break;
         case ((4)*8+(2)):
            for(i=x-1; i >= 0; --i, src += 4, dest += 2)
               dest[0] = RBMP_COMPUTE_Y(src[0],src[1],src[2]), dest[1] = src[3];
            break;
         case ((4)*8+(3)):
            for(i=x-1; i >= 0; --i, src += 4, dest += 3)
               dest[0]=src[0],dest[1]=src[1],dest[2]=src[2];
            break;
         default:
            break;
      }

   }

   return good;
}

/* Microsoft/Windows BMP image */

/* returns 0..31 for the highest set bit */
static int rbmp__high_bit(unsigned int z)
{
   int n=0;
   if (z == 0)
      return -1;
   if (z >= 0x10000) n += 16, z >>= 16;
   if (z >= 0x00100) n +=  8, z >>=  8;
   if (z >= 0x00010) n +=  4, z >>=  4;
   if (z >= 0x00004) n +=  2, z >>=  2;
   if (z >= 0x00002) n +=  1, z >>=  1;
   return n;
}

static int rbmp__bitcount(unsigned int a)
{
   a = (a & 0x55555555) + ((a >>  1) & 0x55555555); /* max 2 */
   a = (a & 0x33333333) + ((a >>  2) & 0x33333333); /* max 4 */
   a = (a + (a >> 4)) & 0x0f0f0f0f; /* max 8 per 4, now 8 bits */
   a = (a + (a >> 8));              /* max 16 per 8 bits */
   a = (a + (a >> 16));             /* max 32 per 8 bits */
   return a & 0xff;
}

static int rbmp__shiftsigned(int v, int shift, int bits)
{
   int result;
   int z=0;

   if (shift < 0)
      v <<= -shift;
   else
      v >>= shift;

   result = v;
   z      = bits;

   while (z < 8)
   {
      result += v >> z;
      z += bits;
   }
   return result;
}

static unsigned char *rbmp__bmp_load(rbmp__context *s, unsigned *x, unsigned *y,
      int *comp, int req_comp)
{
   unsigned char *out;
   int bpp, flip_vertically, pad, target, offset, hsz;
   int psize=0,i,j,width;
   unsigned int mr=0,mg=0,mb=0,ma=0;

   /* Corrupt BMP? */
   if (rbmp__get8(s) != 'B' || rbmp__get8(s) != 'M')
      return 0;

   rbmp__get32le(s); /* discard filesize */
   rbmp__get16le(s); /* discard reserved */
   rbmp__get16le(s); /* discard reserved */
   offset = rbmp__get32le(s);
   hsz = rbmp__get32le(s);

   /* BMP type not supported? */
   if (hsz != 12 && hsz != 40 && hsz != 56 && hsz != 108 && hsz != 124)
      return 0;

   if (hsz == 12)
   {
      s->img_x = rbmp__get16le(s);
      s->img_y = rbmp__get16le(s);
   }
   else
   {
      s->img_x = rbmp__get32le(s);
      s->img_y = rbmp__get32le(s);
   }

   /* Bad BMP? */
   if (rbmp__get16le(s) != 1)
      return 0;

   bpp = rbmp__get16le(s);

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
      int compress = rbmp__get32le(s);

      /* BMP RLE type not supported? */
      if (compress == 1 || compress == 2)
         return 0;

      rbmp__get32le(s); /* discard sizeof */
      rbmp__get32le(s); /* discard hres */
      rbmp__get32le(s); /* discard vres */
      rbmp__get32le(s); /* discard colors used */
      rbmp__get32le(s); /* discard max important */

      if (hsz == 40 || hsz == 56)
      {
         if (hsz == 56)
         {
            rbmp__get32le(s);
            rbmp__get32le(s);
            rbmp__get32le(s);
            rbmp__get32le(s);
         }
         if (bpp == 16 || bpp == 32)
         {
            switch (compress)
            {
               case 0:
#if 0
                  if (bpp == 32)
                  {
                     mr = 0xffu << 16;
                     mg = 0xffu <<  8;
                     mb = 0xffu <<  0;
                     ma = 0xffu << 24;
                  }
                  else
                  {
                     mr = 31u << 10;
                     mg = 31u <<  5;
                     mb = 31u <<  0;
                  }
#endif
                  break;
               case 3:
                  mr = rbmp__get32le(s);
                  mg = rbmp__get32le(s);
                  mb = rbmp__get32le(s);
                  /* not documented, but generated by
                   * Photoshop and handled by MS Paint */
                  /* Bad BMP ?*/
                  if (mr == mg && mg == mb)
                     return 0;
                  break;
               default:
#if 0
                  mr = mg = mb = 0;
#endif
                  break;
            }

            /* Bad BMP? */
            return 0;
         }
      }
      else
      {
         mr = rbmp__get32le(s);
         mg = rbmp__get32le(s);
         mb = rbmp__get32le(s);
         ma = rbmp__get32le(s);
         rbmp__get32le(s); /* Discard color space */
         for (i=0; i < 12; ++i)
            rbmp__get32le(s); /* Discard color space parameters */
         if (hsz == 124)
         {
            rbmp__get32le(s); /* Discard rendering intent */
            rbmp__get32le(s); /* Discard offset of profile data */
            rbmp__get32le(s); /* Discard size of profile data */
            rbmp__get32le(s); /* Discard reserved */
         }
      }
      if (bpp < 16)
         psize = (offset - 14 - hsz) >> 2;
   }
   s->img_n = ma ? 4 : 3;
   if (req_comp && req_comp >= 3) /* We can directly decode 3 or 4 */
      target = req_comp;
   else
      target = s->img_n; /* If they want monochrome, we'll post-convert */

   out = (unsigned char *) malloc(target * s->img_x * s->img_y);

   if (!out)
      return 0;

   if (bpp < 16)
   {
      unsigned char pal[256][4];
      int z=0;

      /* Corrupt BMP? */
      if (psize == 0 || psize > 256)
      {
         free(out);
         return 0;
      }

      for (i=0; i < psize; ++i)
      {
         pal[i][2] = rbmp__get8(s);
         pal[i][1] = rbmp__get8(s);
         pal[i][0] = rbmp__get8(s);
         if (hsz != 12) rbmp__get8(s);
         pal[i][3] = 255;
      }

      rbmp__skip(s, offset - 14 - hsz - psize * (hsz == 12 ? 3 : 4));
      if (bpp == 4)
         width = (s->img_x + 1) >> 1;
      else if (bpp == 8)
         width = s->img_x;
      else
      {
         /* Corrupt BMP */
         free(out);
         return 0;
      }

      pad = (-width)&3;
      for (j=0; j < (int) s->img_y; ++j)
      {
         for (i=0; i < (int) s->img_x; i += 2)
         {
            int v=rbmp__get8(s),v2=0;
            if (bpp == 4)
            {
               v2 = v & 15;
               v >>= 4;
            }
            out[z++] = pal[v][0];
            out[z++] = pal[v][1];
            out[z++] = pal[v][2];
            if (target == 4)
               out[z++] = 255;

            if (i+1 == (int)s->img_x)
               break;

            v = (bpp == 8) ? rbmp__get8(s) : v2;
            out[z++] = pal[v][0];
            out[z++] = pal[v][1];
            out[z++] = pal[v][2];

            if (target == 4)
               out[z++] = 255;
         }
         rbmp__skip(s, pad);
      }
   }
   else
   {
      int rshift=0;
      int gshift=0;
      int bshift=0;
      int ashift=0;
      int rcount=0;
      int gcount=0;
      int bcount=0;
      int acount=0;
      int z = 0;
      int easy=0;

      rbmp__skip(s, offset - 14 - hsz);

      if (bpp == 24)
         width = 3 * s->img_x;
      else if (bpp == 16)
         width = 2*s->img_x;
      else /* bpp = 32 and pad = 0 */
         width=0;

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
            free(out);
            return 0;
         }

         /* right shift amt to put high bit in position #7 */
         rshift = rbmp__high_bit(mr)-7;
         rcount = rbmp__bitcount(mr);
         gshift = rbmp__high_bit(mg)-7;
         gcount = rbmp__bitcount(mg);
         bshift = rbmp__high_bit(mb)-7;
         bcount = rbmp__bitcount(mb);
         ashift = rbmp__high_bit(ma)-7;
         acount = rbmp__bitcount(ma);
      }
      for (j=0; j < (int) s->img_y; ++j)
      {
         if (easy)
         {
            for (i=0; i < (int) s->img_x; ++i)
            {
               unsigned char a;
               out[z+2] = rbmp__get8(s);
               out[z+1] = rbmp__get8(s);
               out[z+0] = rbmp__get8(s);
               z += 3;
               a = (easy == 2 ? rbmp__get8(s) : 255);
               if (target == 4)
                  out[z++] = a;
            }
         }
         else
         {
            for (i=0; i < (int) s->img_x; ++i)
            {
               uint32_t v = (bpp == 16 ? (uint32_t) rbmp__get16le(s) : rbmp__get32le(s));
               int a;
               out[z++] = RBMP__BYTECAST(rbmp__shiftsigned(v & mr, rshift, rcount));
               out[z++] = RBMP__BYTECAST(rbmp__shiftsigned(v & mg, gshift, gcount));
               out[z++] = RBMP__BYTECAST(rbmp__shiftsigned(v & mb, bshift, bcount));
               a = (ma ? rbmp__shiftsigned(v & ma, ashift, acount) : 255);
               if (target == 4)
                  out[z++] = RBMP__BYTECAST(a);
            }
         }
         rbmp__skip(s, pad);
      }
   }

   if (flip_vertically)
   {
      unsigned char t;
      for (j=0; j < (int) s->img_y>>1; ++j)
      {
         unsigned char *p1 = out +      j     *s->img_x*target;
         unsigned char *p2 = out + (s->img_y-1-j)*s->img_x*target;
         for (i=0; i < (int) s->img_x*target; ++i)
         {
            t = p1[i], p1[i] = p2[i], p2[i] = t;
         }
      }
   }

   if (
            req_comp
         && (req_comp >= 1 && req_comp <= 4)
         && (req_comp != target))
   {
      unsigned char *tmp = rbmp__convert_format(out, target, req_comp, s->img_x, s->img_y);

      free(out);
      out = NULL;

      if (!tmp)
         return NULL;

      out = tmp;
   }

   *x = s->img_x;
   *y = s->img_y;

   if (comp)
      *comp = s->img_n;

   return out;
}

static unsigned char *rbmp_load_from_memory(unsigned char const *buffer, int len,
      unsigned *x, unsigned *y, int *comp, int req_comp)
{
   rbmp__context s;

   s.img_buffer          = (unsigned char*)buffer;
   s.img_buffer_original = (unsigned char*)buffer;
   s.img_buffer_end      = (unsigned char*)buffer+len;

   return rbmp__bmp_load(&s,x,y,comp,req_comp);
}

static void rbmp_convert_frame(uint32_t *frame, unsigned width, unsigned height)
{
   uint32_t *end = frame + (width * height * sizeof(uint32_t))/4;

   while(frame < end)
   {
      uint32_t pixel = *frame;
      *frame = (pixel & 0xff00ff00) | ((pixel << 16) & 0x00ff0000) | ((pixel >> 16) & 0xff);
      frame++;
   }
}

int rbmp_process_image(rbmp_t *rbmp, void **buf_data,
      size_t size, unsigned *width, unsigned *height)
{
   int comp;

   if (!rbmp)
      return IMAGE_PROCESS_ERROR;

   rbmp->output_image   = (uint32_t*)rbmp_load_from_memory(rbmp->buff_data,
                           (int)size, width, height, &comp, 4);
   *buf_data             = rbmp->output_image;

   rbmp_convert_frame(rbmp->output_image, *width, *height);

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

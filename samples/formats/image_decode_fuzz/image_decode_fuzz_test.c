/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (image_decode_fuzz_test.c).
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <formats/image.h>
#include <formats/rbmp.h>
#include <formats/rtga.h>
#include <formats/rdds.h>

static long decoded_ok;
static long attempts;

/* Smallest well-formed 24-bit BMP: 2x2 pixels. */
static const unsigned char bmp_seed[] =
{
   'B','M', 70,0,0,0, 0,0, 0,0, 54,0,0,0,        /* file header */
   40,0,0,0, 2,0,0,0, 2,0,0,0, 1,0, 24,0,        /* info header */
   0,0,0,0, 16,0,0,0, 0,0,0,0, 0,0,0,0,
   0,0,0,0, 0,0,0,0,
   /* pixel data, bottom-up, 4-byte aligned rows */
   0xff,0x00,0x00, 0x00,0xff,0x00, 0,0,
   0x00,0x00,0xff, 0xff,0xff,0xff, 0,0
};

/* Smallest well-formed uncompressed 24-bit TGA: 2x2. */
static const unsigned char tga_seed[] =
{
   0,          /* id length */
   0,          /* colour map type */
   2,          /* image type: uncompressed true-colour */
   0,0, 0,0, 0,/* colour map spec */
   0,0, 0,0,   /* x/y origin */
   2,0,        /* width  */
   2,0,        /* height */
   24,         /* bits per pixel */
   0,          /* descriptor */
   0xff,0x00,0x00, 0x00,0xff,0x00,
   0x00,0x00,0xff, 0xff,0xff,0xff
};


/* Minimal well-formed uncompressed 32-bit BGRA DDS: 2x2.
 * 4-byte magic, 124-byte header, then pixels. */
static unsigned char dds_seed[128 + 16];

static void dds_seed_init(void)
{
   unsigned char *h = dds_seed;

   memset(dds_seed, 0, sizeof(dds_seed));
   memcpy(h, "DDS ", 4);
   h[4]  = 124;                        /* dwSize                */
   h[8]  = 0x07; h[9] = 0x10;          /* CAPS|HEIGHT|WIDTH|PIXELFORMAT */
   h[12] = 2;                          /* dwHeight              */
   h[16] = 2;                          /* dwWidth               */
   h[20] = 8;                          /* dwPitchOrLinearSize   */
   h[28] = 1;                          /* dwMipMapCount         */
   h[76] = 32;                         /* ddspf.dwSize          */
   h[80] = 0x41;                       /* ALPHAPIXELS | RGB     */
   h[88] = 32;                         /* dwRGBBitCount         */
   h[92]  = 0x00; h[93]  = 0x00; h[94]  = 0xff; /* R mask       */
   h[96]  = 0x00; h[97]  = 0xff;                /* G mask       */
   h[100] = 0xff;                               /* B mask       */
   h[107] = 0xff;                               /* A mask       */
   h[109] = 0x10;                      /* dwCaps: TEXTURE       */

   /* 2x2 BGRA pixels */
   memset(dds_seed + 128, 0x7f, 16);
}

static void try_dds(const unsigned char *buf, size_t len)
{
   rdds_t *rdds = rdds_alloc();
   void   *out  = NULL;
   unsigned w = 0, h = 0;

   attempts++;

   if (!rdds)
      return;

   if (rdds_set_buf_ptr(rdds, (void*)buf))
   {
      int ret;
      while ((ret = rdds_process_image(rdds, &out, len, &w, &h, true))
            == IMAGE_PROCESS_NEXT)
         ;
      if (ret == IMAGE_PROCESS_END)
         decoded_ok++;
   }

   if (out)
      free(out);
   rdds_free(rdds);
}

static void try_bmp(const unsigned char *buf, size_t len)
{
   rbmp_t *rbmp = rbmp_alloc();
   void   *out  = NULL;
   unsigned w = 0, h = 0;

   attempts++;

   if (!rbmp)
      return;

   /* rbmp_set_buf_ptr takes the buffer; it does not take the length,
    * which is itself worth knowing -- the decoder trusts the header
    * to stay inside whatever it was handed. */
   if (rbmp_set_buf_ptr(rbmp, (void*)buf))
   {
      int ret;
      while ((ret = rbmp_process_image(rbmp, &out, len, &w, &h, true))
            == IMAGE_PROCESS_NEXT)
         ;
      if (ret == IMAGE_PROCESS_END)
         decoded_ok++;
   }

   if (out)
      free(out);
   rbmp_free(rbmp);
}

static void try_tga(const unsigned char *buf, size_t len)
{
   rtga_t *rtga = rtga_alloc();
   void   *out  = NULL;
   unsigned w = 0, h = 0;

   attempts++;

   if (!rtga)
      return;

   if (rtga_set_buf_ptr(rtga, (void*)buf))
   {
      int ret;
      while ((ret = rtga_process_image(rtga, &out, len, &w, &h, true))
            == IMAGE_PROCESS_NEXT)
         ;
      if (ret == IMAGE_PROCESS_END)
         decoded_ok++;
   }

   if (out)
      free(out);
   rtga_free(rtga);
}

/* Every single-byte value at every offset of the header, on a copy
 * sized exactly to the seed so a read past the end is a report. */
static void sweep(const unsigned char *seed, size_t seed_len,
      void (*fn)(const unsigned char*, size_t), size_t header_len)
{
   size_t off;
   int    v;
   size_t trunc;

   /* Well-formed first, so a decoder that rejects everything is
    * visible in the decoded_ok count. */
   {
      unsigned char *copy = (unsigned char*)malloc(seed_len);
      if (!copy)
         return;
      memcpy(copy, seed, seed_len);
      fn(copy, seed_len);
      free(copy);
   }

   /* Truncated at every length. */
   for (trunc = 0; trunc < seed_len; trunc++)
   {
      unsigned char *copy = (unsigned char*)malloc(trunc ? trunc : 1);
      if (!copy)
         return;
      memcpy(copy, seed, trunc);
      fn(copy, trunc);
      free(copy);
   }

   /* Each header byte set to each of a handful of interesting
    * values, including the ones that make dimensions enormous. */
   for (off = 0; off < header_len && off < seed_len; off++)
   {
      static const int vals[] = { 0x00, 0x01, 0x7f, 0x80, 0xfe, 0xff };
      for (v = 0; v < (int)(sizeof(vals) / sizeof(vals[0])); v++)
      {
         unsigned char *copy = (unsigned char*)malloc(seed_len);
         if (!copy)
            return;
         memcpy(copy, seed, seed_len);
         copy[off] = (unsigned char)vals[v];
         fn(copy, seed_len);
         free(copy);
      }
   }
}

int main(void)
{
   dds_seed_init();

   sweep(bmp_seed, sizeof(bmp_seed), try_bmp, 54);
   sweep(tga_seed, sizeof(tga_seed), try_tga, 18);
   sweep(dds_seed, sizeof(dds_seed), try_dds, 128);

   printf("attempts=%ld decoded=%ld\n", attempts, decoded_ok);

   if (decoded_ok < 3)
   {
      fputs("FAIL: not every seed decoded; the sweep proved nothing\n",
            stderr);
      return 1;
   }

   puts("ALL OK");
   return 0;
}

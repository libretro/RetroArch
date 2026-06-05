/* Regression tests for libretro-common/formats/tga/rtga.c
 *
 * Targets:
 *
 *  1. Header-level palette validation.  Pre-patch a crafted TGA
 *     with tga_indexed=1 and tga_palette_bits=255 let the indexed
 *     read loop
 *          for (j = 0; j * 8 < tga_palette_bits; ++j)
 *              raw_data[j] = tga_palette[pal_idx + j];
 *     iterate j=0..31, writing up to 32 bytes into a 4-byte stack
 *     array raw_data[4].  This is a directly reachable stack
 *     buffer overflow driven by a single attacker-controlled
 *     header byte.  Under ASan the unpatched decoder reports
 *     "stack-buffer-overflow WRITE of size 1" inside the indexed
 *     read loop.  Post-patch the header check rejects
 *     palette_bits not in {15, 16, 24, 32} and the decoder
 *     returns IMAGE_PROCESS_ERROR cleanly.
 *
 *  2. Rejection of indexed TGA with empty palette (palette_len=0).
 *     Pre-patch the indexed code clamped pal_idx to 0 and then
 *     read tga_palette[0] from a malloc(0) buffer -- OOB read.
 *
 *  3. Happy-path decode of a tiny 2x2 uncompressed RGB TGA --
 *     smoke test for the fast path.
 *
 *  4. Rejection of a TGA whose declared size > INT_MAX.  Pre-patch
 *     the (int)size cast in rtga_process_image truncated and
 *     propagated a negative "len" into the buffer-end pointer.
 *     This is hard to reproduce without > 2 GiB of memory so we
 *     simulate with a small buffer and a size_t value that would
 *     pre-patch have truncated; post-patch we get an early
 *     IMAGE_PROCESS_ERROR without even calling the decoder.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <formats/rtga.h>
#include <formats/image.h>

static int failures = 0;

/* Build a TGA header into `out` starting at offset 0.
 * Returns the header size (always 18). */
static int make_tga_header(uint8_t *out,
      uint8_t id_len,
      uint8_t cmap_type,
      uint8_t image_type,
      uint16_t cmap_start,
      uint16_t cmap_len,
      uint8_t cmap_bits,
      uint16_t width,
      uint16_t height,
      uint8_t bpp,
      uint8_t descriptor)
{
   out[0]  = id_len;
   out[1]  = cmap_type;
   out[2]  = image_type;
   out[3]  = (uint8_t)(cmap_start & 0xFF);
   out[4]  = (uint8_t)(cmap_start >> 8);
   out[5]  = (uint8_t)(cmap_len & 0xFF);
   out[6]  = (uint8_t)(cmap_len >> 8);
   out[7]  = cmap_bits;
   out[8]  = 0; out[9]  = 0; /* x origin */
   out[10] = 0; out[11] = 0; /* y origin */
   out[12] = (uint8_t)(width & 0xFF);
   out[13] = (uint8_t)(width >> 8);
   out[14] = (uint8_t)(height & 0xFF);
   out[15] = (uint8_t)(height >> 8);
   out[16] = bpp;
   out[17] = descriptor;
   return 18;
}

static void test_malicious_palette_bits(void)
{
   /* Indexed uncompressed TGA, 1x1 pixel, palette_bits = 255.
    * Pre-patch: stack-buffer-overflow on raw_data[4..31]. */
   uint8_t file[128];
   rtga_t *rtga;
   void *out = NULL;
   unsigned w = 0, h = 0;
   int rc;
   int hdr_len;
   memset(file, 0, sizeof(file));

   hdr_len = make_tga_header(file,
         /* id_len      */ 0,
         /* cmap_type   */ 1,
         /* image_type  */ 1,     /* indexed uncompressed */
         /* cmap_start  */ 0,
         /* cmap_len    */ 1,
         /* cmap_bits   */ 255,   /* THE ATTACK */
         /* width       */ 1,
         /* height      */ 1,
         /* bpp         */ 8,
         /* descriptor  */ 0);
   /* A few bytes of "palette" and one pixel index -- doesn't matter
    * what they are, we expect to fail at the header check. */
   file[hdr_len]   = 0xFF;
   file[hdr_len+1] = 0xFF;
   file[hdr_len+2] = 0x00;  /* pal_idx = 0 */

   rtga = rtga_alloc();
   rtga_set_buf_ptr(rtga, file);
   rc = rtga_process_image(rtga, &out, (size_t)hdr_len + 16, &w, &h, true);

   if (rc != IMAGE_PROCESS_ERROR)
   {
      printf("[ERROR] malicious palette_bits=255 was not rejected "
             "(rc=%d, w=%u h=%u out=%p)\n", rc, w, h, out);
      failures++;
      free(out);
   }
   else
      printf("[SUCCESS] TGA palette_bits=255 rejected at header\n");
   rtga_free(rtga);
}

static void test_empty_indexed_palette(void)
{
   /* Indexed TGA with cmap_len = 0.  Pre-patch this malloc'd a
    * 0-byte palette and the indexed read was OOB. */
   uint8_t file[64];
   rtga_t *rtga;
   void *out = NULL;
   unsigned w = 0, h = 0;
   int rc;
   int hdr_len;
   memset(file, 0, sizeof(file));

   hdr_len = make_tga_header(file, 0, 1, 1, 0,
         /* cmap_len=   */ 0,
         /* cmap_bits=  */ 24,
         1, 1, 8, 0);
   file[hdr_len] = 0x00;

   rtga = rtga_alloc();
   rtga_set_buf_ptr(rtga, file);
   rc = rtga_process_image(rtga, &out, (size_t)hdr_len + 8, &w, &h, true);

   if (rc != IMAGE_PROCESS_ERROR)
   {
      printf("[ERROR] empty palette was not rejected "
             "(rc=%d out=%p)\n", rc, out);
      failures++;
      free(out);
   }
   else
      printf("[SUCCESS] TGA indexed with empty palette rejected\n");
   rtga_free(rtga);
}

static void test_happy_path_rgb(void)
{
   /* 2x2 uncompressed 24bpp BGR.  Just a smoke test. */
   uint8_t file[64];
   rtga_t *rtga;
   void *out = NULL;
   unsigned w = 0, h = 0;
   int rc;
   int hdr_len;
   int i;
   memset(file, 0, sizeof(file));

   hdr_len = make_tga_header(file, 0, 0,
         /* image_type= */ 2,    /* uncompressed RGB */
         0, 0, 0,
         /* width */ 2,
         /* height*/ 2,
         /* bpp   */ 24,
         /* descr */ 0);
   /* 4 pixels * 3 bytes */
   for (i = 0; i < 4 * 3; ++i)
      file[hdr_len + i] = (uint8_t)(0x10 * i);

   rtga = rtga_alloc();
   rtga_set_buf_ptr(rtga, file);
   rc = rtga_process_image(rtga, &out, (size_t)hdr_len + 12, &w, &h, true);

   if (rc != IMAGE_PROCESS_END || w != 2 || h != 2 || !out)
   {
      printf("[ERROR] happy-path decode: rc=%d w=%u h=%u out=%p\n",
            rc, w, h, out);
      failures++;
   }
   else
      printf("[SUCCESS] TGA 2x2 RGB happy path\n");
   free(out);
   rtga_free(rtga);
}

static void test_oversized_size(void)
{
   /* A size > INT_MAX should be rejected before the (int)size
    * cast that pre-patch produced a negative buffer-length. */
   uint8_t file[32];
   rtga_t *rtga;
   void *out = NULL;
   unsigned w = 0, h = 0;
   int rc;
   memset(file, 0, sizeof(file));
   /* The file contents don't matter -- we expect an early-out. */

   rtga = rtga_alloc();
   rtga_set_buf_ptr(rtga, file);
   rc = rtga_process_image(rtga, &out, (size_t)0x100000000ULL,
         &w, &h, true);

   if (rc != IMAGE_PROCESS_ERROR)
   {
      printf("[ERROR] oversized size not rejected (rc=%d)\n", rc);
      failures++;
      free(out);
   }
   else
      printf("[SUCCESS] TGA with size > INT_MAX rejected\n");
   rtga_free(rtga);
}

int main(void)
{
   test_malicious_palette_bits();
   test_empty_indexed_palette();
   test_happy_path_rgb();
   test_oversized_size();

   if (failures)
   {
      printf("\n%d rtga test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll rtga regression tests passed.\n");
   return 0;
}

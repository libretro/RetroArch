/* Regression tests for libretro-common/formats/bmp/rbmp.c
 *
 * Targets (a mix of testable discriminators + honest smokes):
 *
 *  1. img_y = 0x80000000 rejection.  Pre-patch the code did
 *       flip_vertically = ((int) s->img_y) > 0;
 *       s->img_y        = abs((int) s->img_y);
 *     When img_y == 0x80000000u, the cast to int is INT_MIN and
 *     abs(INT_MIN) is undefined behaviour.  On 2's-complement
 *     systems abs(INT_MIN) returns INT_MIN, then the uint32_t
 *     assignment leaves img_y == 0x80000000 -- a huge dimension
 *     that then drives the buffer allocation in a bad direction.
 *     Post-patch this is rejected at the header step and
 *     rbmp_process_image returns IMAGE_PROCESS_ERROR cleanly.
 *
 *  2. img_x * img_y overflow rejection.  A crafted BMP with
 *     width = 0x10001 (65537) and height = 0x10000 (65536)
 *     would pre-patch have wrapped the uint32_t multiplication
 *     to 0x10000 on 32-bit builds, producing a 256 KiB malloc
 *     for 16 GiB of claimed image data -- heap overflow at the
 *     first pixel write.  On 64-bit size_t doesn't wrap, so
 *     we can only directly verify the rejection path on 32-bit,
 *     but the size_t arithmetic IS exercised here and the
 *     17 GiB allocation reliably fails on any realistic test
 *     machine -- the test asserts the decoder returns
 *     IMAGE_PROCESS_ERROR rather than corrupting memory.
 *
 *  3. Happy path: a tiny 1x1 24bpp BMP decodes without error.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <formats/rbmp.h>
#include <formats/image.h>

static int failures = 0;

static void put16le(uint8_t *p, uint16_t v)
{
   p[0] = (uint8_t)(v & 0xFF);
   p[1] = (uint8_t)(v >> 8);
}

static void put32le(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v & 0xFF);
   p[1] = (uint8_t)((v >> 8) & 0xFF);
   p[2] = (uint8_t)((v >> 16) & 0xFF);
   p[3] = (uint8_t)((v >> 24) & 0xFF);
}

/* Build a BITMAPFILEHEADER + BITMAPINFOHEADER (hsz=40, 24bpp, BI_RGB)
 * into out.  Returns 14 + 40 = 54. */
static int make_bmp_header(uint8_t *out,
      uint32_t width, uint32_t height, uint16_t bpp,
      uint32_t compression)
{
   memset(out, 0, 54);
   /* BITMAPFILEHEADER */
   out[0] = 'B';
   out[1] = 'M';
   put32le(out + 2, 0);     /* file size, ignored */
   put32le(out + 6, 0);     /* reserved */
   put32le(out + 10, 54);   /* offset to pixel data */
   /* BITMAPINFOHEADER */
   put32le(out + 14, 40);   /* hsz */
   put32le(out + 18, width);
   put32le(out + 22, height);
   put16le(out + 26, 1);    /* planes */
   put16le(out + 28, bpp);
   put32le(out + 30, compression);
   /* remaining 40-byte header fields = 0 from memset */
   return 54;
}

static void test_img_y_int_min(void)
{
   /* img_y = 0x80000000 -- pre-patch abs((int)0x80000000) is UB.
    * Post-patch header check bails cleanly. */
   uint8_t file[128];
   rbmp_t *rbmp;
   void *out = NULL;
   unsigned w = 0, h = 0;
   int rc;
   int hdr_len = make_bmp_header(file, 1u, 0x80000000u, 24, 0);
   /* a few bytes of pseudo pixel data */
   memset(file + hdr_len, 0, 16);

   rbmp = rbmp_alloc();
   rbmp_set_buf_ptr(rbmp, file);
   rc = rbmp_process_image(rbmp, &out, (size_t)hdr_len + 16, &w, &h, true);

   if (rc != IMAGE_PROCESS_ERROR)
   {
      printf("[ERROR] img_y=0x80000000 not rejected (rc=%d w=%u h=%u)\n",
            rc, w, h);
      failures++;
      free(out);
   }
   else
      printf("[SUCCESS] BMP img_y=0x80000000 (abs(INT_MIN) UB) rejected\n");
   rbmp_free(rbmp);
}

static void test_dimension_overflow(void)
{
   /* width * height would overflow uint32_t on 32-bit and also
    * requests an unrealistic allocation on 64-bit.  Post-patch
    * this is rejected via the SIZE_MAX overflow guard OR via
    * malloc failure, either way the decoder returns
    * IMAGE_PROCESS_ERROR rather than corrupting memory. */
   uint8_t file[128];
   rbmp_t *rbmp;
   void *out = NULL;
   unsigned w = 0, h = 0;
   int rc;
   int hdr_len = make_bmp_header(file, 0x10001u, 0x10000u, 24, 0);
   memset(file + hdr_len, 0, 16);

   rbmp = rbmp_alloc();
   rbmp_set_buf_ptr(rbmp, file);
   rc = rbmp_process_image(rbmp, &out, (size_t)hdr_len + 16, &w, &h, true);

   if (rc != IMAGE_PROCESS_ERROR)
   {
      printf("[ERROR] oversized dimensions not rejected "
            "(rc=%d w=%u h=%u out=%p)\n", rc, w, h, out);
      failures++;
      free(out);
   }
   else
      printf("[SUCCESS] BMP 0x10001 x 0x10000 rejected (no heap overflow)\n");
   rbmp_free(rbmp);
}

static void test_happy_1x1_24bpp(void)
{
   /* Tiny valid 1x1 24bpp BGR. */
   uint8_t file[64];
   rbmp_t *rbmp;
   void *out = NULL;
   unsigned w = 0, h = 0;
   int rc;
   int hdr_len = make_bmp_header(file, 1, 1, 24, 0);
   /* One pixel: B=0x12 G=0x34 R=0x56.  BMPs are padded to 4-byte
    * row alignment, so one 24bpp row = 3 bytes pixel + 1 byte pad. */
   file[hdr_len + 0] = 0x12;
   file[hdr_len + 1] = 0x34;
   file[hdr_len + 2] = 0x56;
   file[hdr_len + 3] = 0x00;

   rbmp = rbmp_alloc();
   rbmp_set_buf_ptr(rbmp, file);
   rc = rbmp_process_image(rbmp, &out, (size_t)hdr_len + 4, &w, &h, true);

   if (rc != IMAGE_PROCESS_END || w != 1 || h != 1 || !out)
   {
      printf("[ERROR] happy-path 1x1 BMP: rc=%d w=%u h=%u out=%p\n",
            rc, w, h, out);
      failures++;
   }
   else
      printf("[SUCCESS] BMP 1x1 24bpp happy path\n");
   free(out);
   rbmp_free(rbmp);
}

int main(void)
{
   test_img_y_int_min();
   test_dimension_overflow();
   test_happy_1x1_24bpp();

   if (failures)
   {
      printf("\n%d rbmp test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll rbmp regression tests passed.\n");
   return 0;
}

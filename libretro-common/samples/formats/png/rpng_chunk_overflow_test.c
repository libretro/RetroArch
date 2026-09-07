/* Regression tests for libretro-common/formats/png/rpng.c
 *
 * Exercises the chunk-header phase of rpng_iterate_image after
 * the integer-overflow hardening in the accompanying patch.
 *
 * Honest note on discriminator status: the headline bug fixed
 * here is a pointer-arithmetic overflow in
 *      if (buf + 8 + chunk_size > rpng->buff_end) return false;
 * that is only genuinely reachable on 32-bit builds -- on a
 * 64-bit host the pointer has enough headroom that the compare
 * still returns the right answer (the arithmetic itself is UB
 * per C99, but no sanitizer flags it in practice on 64-bit).
 * These tests therefore exercise the NEW size_t-based guard and
 * serve as regression protection against anyone reintroducing
 * unguarded pointer arithmetic; they are not pre/post
 * discriminators on a 64-bit CI host.
 *
 * Subtests:
 *   1. chunk_size near UINT32_MAX is rejected.
 *   2. chunk_size larger than remaining input is rejected.
 *   3. Valid minimal PNG with just IHDR iterates cleanly.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <formats/rpng.h>
#include <formats/image.h>

static int failures = 0;

static void put32be(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v >> 24);
   p[1] = (uint8_t)(v >> 16);
   p[2] = (uint8_t)(v >> 8);
   p[3] = (uint8_t)(v & 0xFF);
}

/* Minimal PNG magic. */
static const uint8_t png_magic[8] = {
   0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
};

static void test_chunk_size_uint32_max(void)
{
   /* PNG with a single chunk declaring chunk_size = 0xFFFFFFF8.
    * On 32-bit pre-patch this defeated the pointer compare and
    * the IDAT handler's memcpy could read up to ~4 GiB past
    * the end of the input.  Post-patch the size compare rejects
    * on any word size. */
   uint8_t file[64];
   rpng_t *rpng;
   bool iter;
   size_t off = 0;

   memcpy(file + off, png_magic, 8); off += 8;
   put32be(file + off, 0xFFFFFFF8u); off += 4;      /* size */
   memcpy(file + off, "IHDR", 4);    off += 4;      /* type */
   memset(file + off, 0xAA, sizeof(file) - off);

   rpng = rpng_alloc();
   if (!rpng) { printf("[ERROR] alloc\n"); failures++; return; }
   rpng_set_buf_ptr(rpng, file, sizeof(file));
   rpng_start(rpng);
   iter = rpng_iterate_image(rpng);
   if (iter)
   {
      printf("[ERROR] chunk_size=0xFFFFFFF8 was accepted\n");
      failures++;
   }
   else
      printf("[SUCCESS] chunk_size=0xFFFFFFF8 rejected\n");
   rpng_free(rpng);
}

static void test_chunk_size_larger_than_input(void)
{
   /* chunk_size = 1000 with only 40 bytes of input.  Must
    * reject -- the size compare catches this on all hosts. */
   uint8_t file[40];
   rpng_t *rpng;
   bool iter;
   size_t off = 0;

   memcpy(file + off, png_magic, 8); off += 8;
   put32be(file + off, 1000);        off += 4;
   memcpy(file + off, "zzzz", 4);    off += 4;
   memset(file + off, 0, sizeof(file) - off);

   rpng = rpng_alloc();
   rpng_set_buf_ptr(rpng, file, sizeof(file));
   rpng_start(rpng);
   iter = rpng_iterate_image(rpng);
   if (iter)
   {
      printf("[ERROR] chunk_size=1000 in 40-byte file accepted\n");
      failures++;
   }
   else
      printf("[SUCCESS] oversized chunk_size rejected\n");
   rpng_free(rpng);
}

static void test_happy_ihdr(void)
{
   /* Minimal valid framing: magic + IHDR (13 bytes) + space for
    * CRC.  iterate_image should return true for IHDR. */
   uint8_t file[64];
   uint8_t ihdr[13];
   rpng_t *rpng;
   size_t off = 0;
   bool iter;

   memset(file, 0, sizeof(file));
   memcpy(file + off, png_magic, 8); off += 8;

   /* IHDR body: 8x8 RGBA-8. */
   put32be(ihdr + 0, 8);          /* width  */
   put32be(ihdr + 4, 8);          /* height */
   ihdr[8]  = 8;                  /* depth = 8 */
   ihdr[9]  = 6;                  /* color_type = RGBA */
   ihdr[10] = 0;                  /* compression */
   ihdr[11] = 0;                  /* filter */
   ihdr[12] = 0;                  /* interlace */

   put32be(file + off, 13);             off += 4;   /* chunk size */
   memcpy(file + off, "IHDR", 4);       off += 4;   /* type */
   memcpy(file + off, ihdr, 13);        off += 13;  /* data */
   /* 4-byte CRC space (value doesn't matter, not verified here) */
   memset(file + off, 0, 4);            off += 4;

   rpng = rpng_alloc();
   rpng_set_buf_ptr(rpng, file, sizeof(file));
   rpng_start(rpng);
   iter = rpng_iterate_image(rpng);
   if (!iter)
   {
      printf("[ERROR] IHDR iteration failed unexpectedly\n");
      failures++;
   }
   else
      printf("[SUCCESS] IHDR chunk iterated successfully\n");
   rpng_free(rpng);
}

int main(void)
{
   test_chunk_size_uint32_max();
   test_chunk_size_larger_than_input();
   test_happy_ihdr();

   if (failures)
   {
      printf("\n%d rpng chunk-overflow test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll rpng chunk-overflow regression tests passed.\n");
   return 0;
}

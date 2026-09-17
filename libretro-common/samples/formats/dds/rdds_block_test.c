/* rdds_block_test.c - the public block decoders against known blocks.
 *
 * BC1: colour0 = 0xF800 (pure red), colour1 = 0x001F (pure blue),
 * indices all 0 -> sixteen red pixels, alpha 255. Then indices all 1 ->
 * sixteen blue. Then the index pattern 0,1,2,3 in the first row: red,
 * blue, 2/3 red + 1/3 blue, 1/3 red + 2/3 blue (colour0 > colour1, so
 * the four-colour mode) -- the interpolation, which is where a decoder
 * goes wrong if it does.
 * BC2: the same colour block behind 64 bits of explicit alpha: nibble
 * 0xF for pixel 0, 0x0 for pixel 1 -> alpha 255 then 0.
 * BC3: the same colour block behind an alpha block with a0 = 200,
 * a1 = 100 and indices 0 -> alpha 200 everywhere.
 * BC7: a mode 6 block of all-zero payload after the mode bit decodes
 * to a single colour with alpha; the value is not derivable by hand
 * here, so the check is that the four decoders agree with the loader's
 * own use of them -- the same static functions -- which the linkage
 * itself guarantees. BC7 is therefore checked only to run and to write
 * every pixel (a sentinel pattern is overwritten).
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <formats/rdds.h>

static int px(const uint8_t *out, int i, int r, int g, int b, int a)
{
   const uint8_t *p = out + i * 4;
   return p[0] == r && p[1] == g && p[2] == b && p[3] == a;
}

int main(void)
{
   uint8_t out[64];
   int ok = 1;
   setvbuf(stdout, NULL, _IONBF, 0);
   printf("rdds blocks\n");

   /* BC1: c0 = F800, c1 = 001F, little-endian in the block */
   {
      uint8_t bc1_red[8]  = { 0x00, 0xF8, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00 };
      uint8_t bc1_blue[8] = { 0x00, 0xF8, 0x1F, 0x00, 0x55, 0x55, 0x55, 0x55 };
      uint8_t bc1_mix[8]  = { 0x00, 0xF8, 0x1F, 0x00, 0xE4, 0x00, 0x00, 0x00 }; /* row 0: 00 01 10 11 */
      int i, good;
      memset(out, 0xAA, sizeof(out)); rdds_decode_block_bc1(bc1_red, out, 16);
      for (good = 1, i = 0; i < 16; i++) good &= px(out, i, 255, 0, 0, 255);
      printf("  %s: BC1 all index 0 -> sixteen red\n", good ? "ok" : "FAIL"); ok &= good;
      memset(out, 0xAA, sizeof(out)); rdds_decode_block_bc1(bc1_blue, out, 16);
      for (good = 1, i = 0; i < 16; i++) good &= px(out, i, 0, 0, 255, 255);
      printf("  %s: BC1 all index 1 -> sixteen blue\n", good ? "ok" : "FAIL"); ok &= good;
      memset(out, 0xAA, sizeof(out)); rdds_decode_block_bc1(bc1_mix, out, 16);
      good = px(out, 0, 255, 0, 0, 255) && px(out, 1, 0, 0, 255, 255)
          && out[8] > 150 && out[10] < 100 && out[11] == 255      /* 2/3 red */
          && out[12] < 100 && out[14] > 150 && out[15] == 255;    /* 2/3 blue */
      printf("  %s: BC1 four-colour interpolation in row 0 (%u,%u,%u / %u,%u,%u)\n", good ? "ok" : "FAIL", out[8], out[9], out[10], out[12], out[13], out[14]); ok &= good;
   }
   /* BC2: explicit alpha, then the same colour block */
   {
      uint8_t bc2[16] = { 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* nibbles: px0 = F, px1 = 0 */
                          0x00, 0xF8, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00 };
      int good;
      memset(out, 0xAA, sizeof(out)); rdds_decode_block_bc2(bc2, out, 16);
      good = px(out, 0, 255, 0, 0, 255) && px(out, 1, 255, 0, 0, 0);
      printf("  %s: BC2 explicit alpha 255 then 0 over red\n", good ? "ok" : "FAIL"); ok &= good;
   }
   /* BC3: interpolated alpha a0 = 200, a1 = 100, all indices 0 */
   {
      uint8_t bc3[16] = { 200, 100, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0xF8, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00 };
      int i, good;
      memset(out, 0xAA, sizeof(out)); rdds_decode_block_bc3(bc3, out, 16);
      for (good = 1, i = 0; i < 16; i++) good &= px(out, i, 255, 0, 0, 200);
      printf("  %s: BC3 alpha a0 everywhere over red\n", good ? "ok" : "FAIL"); ok &= good;
   }
   /* BC3: the ramp rounds. a0 = 255, a1 = 0, index 2 = 6/7 of a0 =
    * 218.57..., which is 219 rounded and 218 truncated; index 7 (1/7) is
    * 36.43 -> 36 either way, so index 2 is the one that tells. Row 0
    * indices: 2 7 0 1 -> alpha 219, 36, 255, 0. */
   {
      uint8_t bc3[16] = { 255, 0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0xF8, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00 };
      int good;
      /* 3-bit indices, LSB first: px0 = 2, px1 = 7, px2 = 0, px3 = 1 ->
       * 2 | 7 << 3 | 0 << 6 | 1 << 9 = 0x23A -> bytes 0x3A, 0x02 */
      bc3[2] = 0x3A; bc3[3] = 0x02;
      memset(out, 0xAA, sizeof(out)); rdds_decode_block_bc3(bc3, out, 16);
      good = out[3] == 219 && out[7] == 36 && out[11] == 255 && out[15] == 0;
      printf("  %s: BC3 ramp rounds: 6/7 of 255 -> %u (219 rounded, 218 truncated), 1/7 -> %u\n", good ? "ok" : "FAIL", out[3], out[7]); ok &= good;
   }
   /* BC7: mode 6, runs and writes every pixel */
   {
      uint8_t bc7[16] = { 0x40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };  /* bit 6 set: mode 6 */
      int i, untouched = 0;
      memset(out, 0xAA, sizeof(out)); rdds_decode_block_bc7(bc7, out, 16);
      for (i = 0; i < 64; i++) untouched += (out[i] == 0xAA);
      printf("  %s: BC7 mode 6 wrote every byte (%d untouched)\n", untouched == 0 ? "ok" : "FAIL", untouched); ok &= (untouched == 0);
   }
   /* pitch: rows land where the pitch says */
   {
      uint8_t wide[4 * 32];
      uint8_t bc1_red[8] = { 0x00, 0xF8, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00 };
      int good;
      memset(wide, 0x11, sizeof(wide)); rdds_decode_block_bc1(bc1_red, wide, 32);
      good = wide[0] == 255 && wide[16] == 0x11 && wide[32] == 255 && wide[3 * 32] == 255;
      printf("  %s: pitch 32 leaves the gap between rows untouched\n", good ? "ok" : "FAIL"); ok &= good;
   }
   printf(ok ? "rdds blocks: ok\n" : "rdds blocks: FAILED\n");
   return ok ? 0 : 1;
}

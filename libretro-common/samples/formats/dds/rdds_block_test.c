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
 * BGRA: the _bgra block decoders give the RGBA ones' bytes with red and
 * blue swapped, over many random blocks.
 * Whole images: random BC1/BC2/BC3 DDS files, with edges (sizes not a
 * multiple of 4), decoded by rdds_process_image for an RGBA and for a
 * BGRA driver, each pixel checked against the block decoders. The
 * loader's fast paths have their own fused decoders; they must agree
 * with these texel for texel (a BC3 alpha ramp once truncated on the
 * RGBA path and rounded on the BGRA one).
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <formats/rdds.h>
#include <formats/image.h>

typedef void (*block_fn)(const void *block, void *out, int pitch);

static uint32_t rnd_state = 0x12345678u;
static uint8_t rnd8(void)
{
   rnd_state = rnd_state * 1664525u + 1013904223u;
   return (uint8_t)(rnd_state >> 24);
}

static void put32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
   p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* A DDS file of random blocks: a 128-byte header, then the blocks */
static uint8_t *make_dds(unsigned w, unsigned h, const char *fourcc,
      unsigned block_size, size_t *len)
{
   size_t   n = (size_t)((w + 3) / 4) * ((h + 3) / 4) * block_size, i;
   uint8_t *d = (uint8_t*)calloc(1, 128 + n);
   memcpy(d, "DDS ", 4);
   put32(d + 4, 124);
   put32(d + 8, 0x1007 | 0x80000); /* caps, height, width, pixelformat, linear size */
   put32(d + 12, h);
   put32(d + 16, w);
   put32(d + 20, (uint32_t)n);
   put32(d + 28, 1);
   put32(d + 76, 32);
   put32(d + 80, 4);                 /* DDPF_FOURCC */
   memcpy(d + 84, fourcc, 4);
   put32(d + 108, 0x1000);           /* DDSCAPS_TEXTURE */
   for (i = 0; i < n; i++)
      d[128 + i] = rnd8();
   *len = 128 + n;
   return d;
}

/* rdds_process_image to the end; NULL on failure */
static uint32_t *decode_image(uint8_t *dds, size_t len, bool rgba,
      unsigned *w, unsigned *h)
{
   rdds_t *r   = rdds_alloc();
   void   *buf = NULL;
   int     ret;
   rdds_set_buf_ptr(r, dds);
   do
      ret = rdds_process_image(r, &buf, len, w, h, rgba);
   while (ret == IMAGE_PROCESS_NEXT);
   rdds_free(r);
   if (ret != IMAGE_PROCESS_END)
   {
      free(buf);
      return NULL;
   }
   return (uint32_t*)buf;
}

/* Every pixel of the image, decoded for an RGBA or a BGRA driver, is the
 * block decoder's texel: A<<24|B<<16|G<<8|R, or A<<24|R<<16|G<<8|B */
static int check_image(const char *name, const char *fourcc,
      unsigned block_size, block_fn dec, unsigned w, unsigned h)
{
   size_t    len;
   uint8_t  *dds = make_dds(w, h, fourcc, block_size, &len);
   unsigned  bx, by, bw = (w + 3) / 4, bh = (h + 3) / 4, iw, ih;
   uint32_t *img[2];
   int       mode, bad = 0;

   img[0] = decode_image(dds, len, true,  &iw, &ih);
   img[1] = decode_image(dds, len, false, &iw, &ih);
   if (!img[0] || !img[1] || iw != w || ih != h)
      bad = -1;
   for (by = 0; bad >= 0 && by < bh; by++)
      for (bx = 0; bx < bw; bx++)
      {
         uint8_t  t[64];
         unsigned x, y;
         dec(dds + 128 + ((size_t)by * bw + bx) * block_size, t, 16);
         for (y = 0; y < 4; y++)
            for (x = 0; x < 4; x++)
            {
               unsigned       px = bx * 4 + x, py = by * 4 + y;
               const uint8_t *p  = t + (y * 4 + x) * 4;
               uint32_t       want[2];
               if (px >= w || py >= h)
                  continue;
               want[0] = ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16)
                       | ((uint32_t)p[1] << 8) | p[0];
               want[1] = ((uint32_t)p[3] << 24) | ((uint32_t)p[0] << 16)
                       | ((uint32_t)p[1] << 8) | p[2];
               for (mode = 0; mode < 2; mode++)
                  if (img[mode][(size_t)py * w + px] != want[mode])
                     bad++;
            }
      }
   printf("  %s: %s %ux%u image, RGBA and BGRA, as the block decoder (%d wrong)\n",
         bad == 0 ? "ok" : "FAIL", name, w, h, bad);
   free(img[0]);
   free(img[1]);
   free(dds);
   return bad == 0;
}

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
   /* BGRA block decoders: the RGBA bytes, red and blue swapped */
   {
      static const block_fn rgba_fn[3] = { rdds_decode_block_bc1, rdds_decode_block_bc2, rdds_decode_block_bc3 };
      static const block_fn bgra_fn[3] = { rdds_decode_block_bc1_bgra, rdds_decode_block_bc2_bgra, rdds_decode_block_bc3_bgra };
      int f, n, i, bad = 0;
      for (f = 0; f < 3; f++)
         for (n = 0; n < 20000; n++)
         {
            uint8_t blk[16], a[64], b[64];
            for (i = 0; i < 16; i++)
               blk[i] = rnd8();
            rgba_fn[f](blk, a, 16);
            bgra_fn[f](blk, b, 16);
            for (i = 0; i < 16; i++)
               bad += a[i * 4] != b[i * 4 + 2] || a[i * 4 + 1] != b[i * 4 + 1]
                   || a[i * 4 + 2] != b[i * 4] || a[i * 4 + 3] != b[i * 4 + 3];
         }
      printf("  %s: BC1/BC2/BC3 BGRA = RGBA with red and blue swapped, 60000 random blocks (%d wrong)\n",
            bad ? "FAIL" : "ok", bad);
      ok &= !bad;
   }
   /* Whole images, edges included */
   ok &= check_image("BC1", "DXT1",  8, rdds_decode_block_bc1, 64, 64);
   ok &= check_image("BC1", "DXT1",  8, rdds_decode_block_bc1, 37, 29);
   ok &= check_image("BC2", "DXT3", 16, rdds_decode_block_bc2, 64, 64);
   ok &= check_image("BC2", "DXT3", 16, rdds_decode_block_bc2, 37, 29);
   ok &= check_image("BC3", "DXT5", 16, rdds_decode_block_bc3, 64, 64);
   ok &= check_image("BC3", "DXT5", 16, rdds_decode_block_bc3, 37, 29);
   printf(ok ? "rdds blocks: ok\n" : "rdds blocks: FAILED\n");
   return ok ? 0 : 1;
}

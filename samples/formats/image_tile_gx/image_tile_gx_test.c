/* image_texture_tile_gx() against the GX RGBA8 layout, which no other
 * test sees: the gx driver samples it straight from memory on
 * hardware, so a wrong tile is invisible anywhere else.
 *
 * Two oracles. The layout itself, from its definition: texel (x, y)
 * lands in tile (y/4)*(w/4) + x/4 at position k = (y%4)*4 + x%4, its
 * first 16-bit half (in memory order - AR on the big-endian GX) at
 * halfword k of the tile and its second at 16 + k, with width and
 * height rounded down to multiples of 4. And the in-place band
 * conversion image_transfer_process() used to apply under GEKKO,
 * kept here verbatim, which the helper must match byte for byte
 * including the untouched tail of the buffer - the source pitch is
 * the unmasked width, so a width that is not a multiple of four is
 * exactly where a rewrite would drift. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <formats/image.h>

static int old_tile(uint32_t **buf, unsigned *width, unsigned *height)
{
      unsigned tmp_pitch, width2, i;
      uint16_t *dst      = NULL;
      size_t    bandsz;
      tmp_pitch = (unsigned)(((size_t)(*width) * sizeof(uint32_t)) >> 1);
      bandsz    = (size_t)tmp_pitch * 4 * sizeof(uint16_t);
      *width  &= ~3;
      *height &= ~3;
      width2   = (*width) << 1;
      dst      = (uint16_t*)*buf;
      {
         void *tmp = malloc(bandsz);
         if (!tmp)
            return -1;
         for (i = 0; i < *height; i += 4, dst += 4 * width2)
         {
            const uint16_t *src;
            memcpy(tmp, (const uint16_t*)*buf + (size_t)i * tmp_pitch,
                  bandsz);
            src = (const uint16_t*)tmp;
#define GX_BLIT_LINE_32(off) \
            { \
               unsigned x; \
               const uint16_t *tmp_src = src; \
               uint16_t       *tmp_dst = dst; \
               for (x = 0; x < width2 >> 3; x++, tmp_src += 8, tmp_dst += 32) \
               { \
                  tmp_dst[  0 + off] = tmp_src[0]; \
                  tmp_dst[ 16 + off] = tmp_src[1]; \
                  tmp_dst[  1 + off] = tmp_src[2]; \
                  tmp_dst[ 17 + off] = tmp_src[3]; \
                  tmp_dst[  2 + off] = tmp_src[4]; \
                  tmp_dst[ 18 + off] = tmp_src[5]; \
                  tmp_dst[  3 + off] = tmp_src[6]; \
                  tmp_dst[ 19 + off] = tmp_src[7]; \
               } \
               src += tmp_pitch; \
            }
            GX_BLIT_LINE_32(0)
            GX_BLIT_LINE_32(4)
            GX_BLIT_LINE_32(8)
            GX_BLIT_LINE_32(12)
#undef GX_BLIT_LINE_32
         }
         free(tmp);
      }
      return 0;
}

static unsigned check_layout(const uint32_t *lin, const uint32_t *tiled,
      unsigned w, unsigned h, unsigned tw, unsigned th)
{
   unsigned x, y, bad = 0;
   const uint16_t *t = (const uint16_t*)tiled;
   if (tw != (w & ~3u) || th != (h & ~3u))
      return 1;
   for (y = 0; y < th; y++)
      for (x = 0; x < tw; x++)
      {
         const uint16_t *s = (const uint16_t*)&lin[(size_t)y * w + x];
         size_t tile       = (size_t)(y / 4) * (tw / 4) + x / 4;
         unsigned k        = (y % 4) * 4 + x % 4;
         if (t[tile * 32 + k] != s[0] || t[tile * 32 + 16 + k] != s[1])
            bad++;
      }
   return bad;
}

int main(void)
{
   unsigned w, h, n = 0, bad_oracle = 0, bad_layout = 0;
   srand(1);
   for (w = 1; w <= 70; w++)
      for (h = 1; h <= 44; h++)
      {
         size_t k, cnt = (size_t)w * h;
         uint32_t *a   = (uint32_t*)malloc(cnt * 4);
         uint32_t *b   = (uint32_t*)malloc(cnt * 4);
         uint32_t *lin = (uint32_t*)malloc(cnt * 4);
         struct texture_image img;
         unsigned ow = w, oh = h;
         if (!a || !b || !lin)
            return 2;
         for (k = 0; k < cnt; k++)
            lin[k] = ((uint32_t)rand() << 16) ^ (uint32_t)rand();
         memcpy(a, lin, cnt * 4);
         memcpy(b, lin, cnt * 4);
         if (old_tile(&a, &ow, &oh))
            return 2;
         memset(&img, 0, sizeof(img));
         img.pixels = b;
         img.width  = w;
         img.height = h;
         if (     !image_texture_tile_gx(&img)
               || img.width != ow || img.height != oh
               || memcmp(a, b, cnt * 4))
            bad_oracle++;
         bad_layout += check_layout(lin, b, w, h, img.width, img.height)
            ? 1 : 0;
         n++;
         free(a);
         free(b);
         free(lin);
      }
   {
      struct texture_image img;
      memset(&img, 0, sizeof(img));
      if (image_texture_tile_gx(&img) || image_texture_tile_gx(NULL))
         bad_layout++;
   }
   printf("%u sizes: %u differ from the band conversion, "
         "%u break the GX RGBA8 layout\n", n, bad_oracle, bad_layout);
   if (bad_oracle || bad_layout)
      return 1;
   printf("[pass] image_tile_gx_test\n");
   return 0;
}

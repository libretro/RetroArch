/* image_yuv_blit_test: the fixed-point 4:2:0 -> RGB32 blits against
 * the floating-point matrices they stand for, in every layout and
 * colour space they take.
 *
 *   1. Each of the four matrix/range pairs is within one step of the
 *      exact conversion for every pixel of a frame that covers the
 *      sample space, and exact for the great majority of them.
 *   2. NV12, NV21 and I420 of the same picture give the same RGB, by
 *      their own entry points and by the general one a camera frame
 *      maps onto, with the chroma order honoured.
 *   3. Odd sizes: the last column and row are converted, and nothing
 *      past the frame is written.
 *   4. The word order: RGBA puts R in the low byte, XRGB in the high
 *      one, alpha set either way. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <formats/image_yuv_blit.h>

static unsigned failures;
#define CHECK(c, ...) \
   do { if (!(c)) { printf("[fail] "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

static int clamp255(double v)
{
   int i = (int)floor(v + 0.5);
   return i < 0 ? 0 : (i > 255 ? 255 : i);
}

/* The exact conversion, as ITU-R BT.601 / BT.709 write it */
static void ref_pixel(int y, int cb, int cr, unsigned flags, int *r, int *g, int *b)
{
   double kr = (flags & IMAGE_YUV_FLAG_BT709) ? 0.2126 : 0.299;
   double kb = (flags & IMAGE_YUV_FLAG_BT709) ? 0.0722 : 0.114;
   double kg = 1.0 - kr - kb;
   double yy, pb, pr;
   if (flags & IMAGE_YUV_FLAG_FULL_RANGE)
   {
      yy = y;
      pb = (cb - 128) / 255.0;
      pr = (cr - 128) / 255.0;
   }
   else
   {
      yy = (y - 16) * 255.0 / 219.0;
      pb = (cb - 128) / 224.0;
      pr = (cr - 128) / 224.0;
   }
   *r = clamp255(yy + 2.0 * (1.0 - kr) * pr * 255.0);
   *b = clamp255(yy + 2.0 * (1.0 - kb) * pb * 255.0);
   *g = clamp255(yy - (2.0 * kb * (1.0 - kb) * pb + 2.0 * kr * (1.0 - kr) * pr) / kg * 255.0);
}

#define W 64
#define H 48

static uint8_t Y[H][W], U[H / 2][W / 2], V[H / 2][W / 2];
static uint8_t UV[H / 2][W];       /* NV12 */
static uint8_t VU[H / 2][W];       /* NV21 */
static uint32_t out[H][W], out2[H][W];

static void fill(unsigned seed)
{
   unsigned x, y;
   srand(seed);
   for (y = 0; y < H; y++)
      for (x = 0; x < W; x++)
         Y[y][x] = (uint8_t)rand();
   for (y = 0; y < H / 2; y++)
      for (x = 0; x < W / 2; x++)
      {
         U[y][x]        = (uint8_t)rand();
         V[y][x]        = (uint8_t)rand();
         UV[y][2 * x]     = U[y][x];
         UV[y][2 * x + 1] = V[y][x];
         VU[y][2 * x]     = V[y][x];
         VU[y][2 * x + 1] = U[y][x];
      }
}

static void lane_matrices(void)
{
   static const unsigned sets[4] = {
      0, IMAGE_YUV_FLAG_FULL_RANGE, IMAGE_YUV_FLAG_BT709,
      IMAGE_YUV_FLAG_BT709 | IMAGE_YUV_FLAG_FULL_RANGE
   };
   unsigned s, x, y;
   fill(1);
   for (s = 0; s < 4; s++)
   {
      unsigned off = 0, exact = 0;
      image_yuv_i420_to_rgb32(&out[0][0], W, &Y[0][0], W, &U[0][0], W / 2,
            &V[0][0], W / 2, W, H, sets[s]);
      for (y = 0; y < H; y++)
         for (x = 0; x < W; x++)
         {
            int r, g, b;
            uint32_t p = out[y][x];
            int pr = (p >> 16) & 0xff, pg = (p >> 8) & 0xff, pb = p & 0xff;
            ref_pixel(Y[y][x], U[y / 2][x / 2], V[y / 2][x / 2], sets[s], &r, &g, &b);
            if (abs(pr - r) > 1 || abs(pg - g) > 1 || abs(pb - b) > 1)
               off++;
            else if (pr == r && pg == g && pb == b)
               exact++;
            CHECK((p >> 24) == 0xff, "set %u: alpha not set at %u,%u", s, x, y);
         }
      CHECK(off == 0, "set %u: %u pixels more than one step off", s, off);
      CHECK(exact * 100 >= (unsigned)(W * H) * 90,
            "set %u: only %u of %u pixels exact", s, exact, W * H);
   }
}

static void lane_layouts(void)
{
   unsigned x, y;
   fill(2);
   image_yuv_i420_to_rgb32(&out[0][0], W, &Y[0][0], W, &U[0][0], W / 2,
         &V[0][0], W / 2, W, H, 0);
   image_yuv_nv12_to_rgb32(&out2[0][0], W, &Y[0][0], W, &UV[0][0], W, W, H, 0);
   CHECK(!memcmp(out, out2, sizeof(out)), "NV12 differs from I420");
   image_yuv_nv12_to_rgb32(&out2[0][0], W, &Y[0][0], W, &VU[0][0], W, W, H,
         IMAGE_YUV_FLAG_VU);
   CHECK(!memcmp(out, out2, sizeof(out)), "NV21 differs from I420");
   /* the general form, as a camera frame describes NV21: Cb and Cr by
    * their own pointers into the one interleaved plane */
   image_yuv_420_to_rgb32(&out2[0][0], W, &Y[0][0], W, &VU[0][1], W,
         &VU[0][0], W, 2, W, H, 0);
   CHECK(!memcmp(out, out2, sizeof(out)), "general form of NV21 differs from I420");
   image_yuv_420_to_rgb32(&out2[0][0], W, &Y[0][0], W, &U[0][0], W / 2,
         &V[0][0], W / 2, 1, W, H, 0);
   CHECK(!memcmp(out, out2, sizeof(out)), "general form of I420 differs");
   /* swapped chroma swaps the colour cast: red and blue change places
    * in the matrix, so the red-heavy pixels come out blue-heavy */
   image_yuv_i420_to_rgb32(&out2[0][0], W, &Y[0][0], W, &U[0][0], W / 2,
         &V[0][0], W / 2, W, H, IMAGE_YUV_FLAG_VU);
   {
      unsigned same = 0;
      for (y = 0; y < H; y++)
         for (x = 0; x < W; x++)
            if (out[y][x] == out2[y][x])
               same++;
      CHECK(same < (unsigned)(W * H) / 4, "VU on I420 left %u pixels unchanged", same);
   }
   image_yuv_nv12_to_rgb32(&out2[0][0], W, &Y[0][0], W, &UV[0][0], W, W, H,
         IMAGE_YUV_FLAG_RGBA);
   for (y = 0; y < H; y++)
      for (x = 0; x < W; x++)
      {
         uint32_t a = out[y][x], b = out2[y][x];
         CHECK(((a >> 16) & 0xff) == (b & 0xff) && (a & 0xff) == ((b >> 16) & 0xff)
               && ((a >> 8) & 0xff) == ((b >> 8) & 0xff) && (b >> 24) == 0xff,
               "RGBA order wrong at %u,%u: %08x vs %08x", x, y, a, b);
      }
}

static void lane_odd(void)
{
   unsigned x, y;
   fill(3);
   memset(out, 0xee, sizeof(out));
   image_yuv_i420_to_rgb32(&out[0][0], W, &Y[0][0], W, &U[0][0], W / 2,
         &V[0][0], W / 2, W - 1, H - 1, 0);
   for (y = 0; y < H; y++)
      for (x = 0; x < W; x++)
      {
         int inside = (x < W - 1 && y < H - 1);
         CHECK(inside ? out[y][x] != 0xeeeeeeeeu : out[y][x] == 0xeeeeeeeeu,
               "odd: pixel %u,%u %s", x, y, inside ? "not written" : "written");
      }
   /* the odd column and row share chroma with their neighbours */
   {
      int r, g, b;
      uint32_t p = out[H - 2][W - 2];
      ref_pixel(Y[H - 2][W - 2], U[(H - 2) / 2][(W - 2) / 2],
            V[(H - 2) / 2][(W - 2) / 2], 0, &r, &g, &b);
      CHECK(abs((int)((p >> 16) & 0xff) - r) <= 1, "odd: last column wrong");
   }
}

int main(void)
{
   lane_matrices();
   lane_layouts();
   lane_odd();
   if (failures)
   {
      printf("image_yuv_blit_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("image_yuv_blit_test: all lanes pass\n");
   return 0;
}

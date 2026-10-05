/* The HDR video blits against references worked here in double
 * precision, independently of the tables the blits use:
 *
 *   - PQ and HLG to linear scRGB half floats (rwebm_video_blit_i420_fp16):
 *     a grey ramp lands on the ST 2084 EOTF / the BT.2100 HLG inverse
 *     OETF and 1000-nit OOTF, over 80 nits; a BT.2020 primary keeps the
 *     components outside 709 that scRGB carries as negatives; any other
 *     transfer is refused with nothing written;
 *   - HLG through the SDR paths (_hbd, _10bit) is tone-mapped as HDR:
 *     a grey ramp rises monotonically to white, where taking it as SDR
 *     would leave it at the signal's own code values. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include <formats/rwebm_video.h>

static unsigned failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; \
   fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
   fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static double half_to_double(uint16_t h)
{
   int      e = (h >> 10) & 0x1f;
   unsigned m = h & 0x3ff;
   double   v = e ? ldexp(1.0 + m / 1024.0, e - 15) : ldexp(m / 1024.0, -14);
   return (h & 0x8000) ? -v : v;
}

static double pq_nits(double e)
{
   const double m1 = 0.1593017578125, m2 = 78.84375;
   const double c1 = 0.8359375, c2 = 18.8515625, c3 = 18.6875;
   double ep  = pow(e, 1.0 / m2);
   double num = ep - c1 > 0.0 ? ep - c1 : 0.0;
   return 10000.0 * pow(num / (c2 - c3 * ep), 1.0 / m1);
}

static double hlg_scene(double e)
{
   const double a = 0.17883277, b = 1.0 - 4.0 * 0.17883277;
   const double c = 0.5 - a * log(4.0 * a);
   return e <= 0.5 ? e * e / 3.0 : (exp((e - c) / a) + b) / 12.0;
}

/* Limited-range neutral grey: the blits' integer YCbCr step maps luma
 * code Y to R'=G'=B' = (298 * (Y - 64) + 128) >> 8. */
static int grey_code(int yc)
{
   int r = (298 * (yc - 64) + 128) >> 8;
   return r < 0 ? 0 : r > 1023 ? 1023 : r;
}

#define W 64
#define H 2

static uint16_t ybuf[W * H], ubuf[W / 2], vbuf[W / 2];
static uint16_t out16[W * H * 4];
static uint32_t out32[W * H];

static void fill_grey(int step)
{
   int i;
   for (i = 0; i < W * H; i++)
      ybuf[i] = (uint16_t)(64 + ((i % W) * step) % 877);
   for (i = 0; i < W / 2; i++)
   {
      ubuf[i] = 512;
      vbuf[i] = 512;
   }
}

static void lane_fp16_ramp(unsigned transfer, const char *name)
{
   int i;
   double worst = 0.0;
   fill_grey(14);
   CHECK(rwebm_video_blit_i420_fp16(out16, W, W, H, ybuf, W, ubuf, vbuf,
         W / 2, 9, transfer, 1), "%s: refused", name);
   for (i = 0; i < W; i++)
   {
      double e   = grey_code(ybuf[i]) / 1023.0;
      double ref = transfer == 16
         ? pq_nits(e) / 80.0
         : 1000.0 * pow(hlg_scene(e), 1.2) / 80.0;
      double got;
      int k;
      for (k = 0; k < 3; k++)
      {
         double err;
         got = half_to_double(out16[i * 4 + k]);
         /* grey stays grey through the gamut matrix; half precision is
          * 2^-11 relative, the matrix rows sum to 1 within 1e-5 */
         err = fabs(got - ref) / (ref > 1e-3 ? ref : 1e-3);
         if (err > worst)
            worst = err;
      }
      CHECK(out16[i * 4 + 3] == 0x3c00, "%s: alpha not 1.0", name);
   }
   CHECK(worst < 2e-3, "%s: grey ramp off its reference by %.5f relative",
         name, worst);
   if (!failures)
      printf("[ok]   %s to scRGB: grey ramp within %.5f of the reference\n",
            name, worst);
}

static void lane_fp16_gamut(void)
{
   /* Full-range BT.2020 red at PQ 100 nits-ish: R' high, G'=B'=0. Built
    * by inverting the full-range matrix is not needed: Cr high, Y low. */
   int i;
   double r, g, b;
   for (i = 0; i < W * H; i++)
      ybuf[i] = 300;
   for (i = 0; i < W / 2; i++)
   {
      ubuf[i] = 400;
      vbuf[i] = 900;
   }
   rwebm_video_blit_i420_fp16(out16, W, W, H, ybuf, W, ubuf, vbuf, W / 2,
         9, 16, 1);
   r = half_to_double(out16[0]);
   g = half_to_double(out16[1]);
   b = half_to_double(out16[2]);
   CHECK(r > 0.0 && (g < 0.0 || b < 0.0),
         "a saturated BT.2020 red kept no component outside 709 "
         "(%.4f %.4f %.4f)", r, g, b);
   if (!failures)
      printf("[ok]   wide gamut kept: 2020 red is (%.3f, %.3f, %.3f) in "
             "scRGB\n", r, g, b);
}

static void lane_fp16_refuses_sdr(void)
{
   unsigned t;
   static const unsigned sdr[] = { 0, 1, 6, 13, 14, 15 };
   for (t = 0; t < sizeof(sdr) / sizeof(sdr[0]); t++)
   {
      memset(out16, 0xab, sizeof(out16));
      CHECK(!rwebm_video_blit_i420_fp16(out16, W, W, H, ybuf, W, ubuf,
            vbuf, W / 2, 1, sdr[t], 1),
            "transfer %u taken as HDR", sdr[t]);
      CHECK(out16[0] == 0xabab, "transfer %u wrote while refusing",
            sdr[t]);
   }
   if (!failures)
      printf("[ok]   SDR transfers refused, nothing written\n");
}

static void lane_hlg_tonemapped(void)
{
   int i, prev = -1, mono = 1, top;
   fill_grey(14);
   rwebm_video_blit_i420_hbd(out32, W, W, H, ybuf, W, ubuf, vbuf, W / 2,
         9, 18, 1, 0, 1);
   for (i = 0; i < W; i++)
   {
      int ri = (int)(out32[i] & 0xff);
      if (ybuf[i] > ybuf[i ? i - 1 : 0] && ri < prev)
         mono = 0;
      prev = ri;
   }
   /* the ramp's brightest sample: well above SDR white in HLG */
   top = 0;
   for (i = 0; i < W; i++)
      if ((int)(out32[i] & 0xff) > top)
         top = (int)(out32[i] & 0xff);
   CHECK(mono, "HLG tone map not monotonic over a grey ramp");
   CHECK(top >= 240, "HLG peak tone-mapped to %d, not near white", top);
   /* As SDR the darkest steps would sit at their own code values;
    * tone-mapped, the OOTF's gamma pulls them below. */
   CHECK((int)(out32[2] & 0xff) < (grey_code(ybuf[2]) + 2) / 4,
         "HLG darks left at their SDR code values");
   rwebm_video_blit_i420_10bit(out32, W, W, H, ybuf, W, ubuf, vbuf, W / 2,
         9, 18, 1, 0);
   {
      int top10 = 0;
      for (i = 0; i < W; i++)
         if ((int)((out32[i] >> 20) & 0x3ff) > top10)
            top10 = (int)((out32[i] >> 20) & 0x3ff);
      CHECK(top10 >= 960, "HLG 10-bit peak tone-mapped to %d, not near "
            "white", top10);
   }
   if (!failures)
      printf("[ok]   HLG through the SDR paths is tone-mapped (peak %d)\n",
            top);
}

int main(void)
{
   lane_fp16_ramp(16, "PQ");
   lane_fp16_ramp(18, "HLG");
   lane_fp16_gamut();
   lane_fp16_refuses_sdr();
   lane_hlg_tonemapped();
   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] image_hdr_blit_test\n");
   return 0;
}

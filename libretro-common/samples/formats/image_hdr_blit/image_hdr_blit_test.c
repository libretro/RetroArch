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

static double tone_curve(double x)
{
   return (x * (0.15 * x + 0.05) + 0.004)
      / (x * (0.15 * x + 0.5) + 0.06) - 0.02 / 0.3;
}

/* Reference starts at normalized source code values, without the
 * production integer matrix or transfer/output lookup tables. */
static void reference_sdr(unsigned y, unsigned u, unsigned v,
      unsigned full, unsigned transfer, unsigned primaries,
      unsigned peak_nits, double rgb[3])
{
   static const double gamut[3][3] = {
      {6801.0/4096,-2407.0/4096,-298.0/4096},
      {-510.0/4096,4640.0/4096,-34.0/4096},
      {-75.0/4096,-412.0/4096,4583.0/4096}
   };
   double yy = ((double)y - (full ? 0 : 64)) / (full ? 1023 : 876);
   double cb = ((double)u - 512) / (full ? 1023 : 896);
   double cr = ((double)v - 512) / (full ? 1023 : 896);
   double linear[3], lo = 1, hi = 8, gain = 2, norm;
   unsigned i, j;
   rgb[0] = yy + 1.4746 * cr;
   rgb[1] = yy - 0.16455313 * cb - 0.57135313 * cr;
   rgb[2] = yy + 1.8814 * cb;
   for (i = 0; i < 48; i++)
   {
      gain = (lo + hi) / 2;
      if (tone_curve(gain) / tone_curve(gain * peak_nits / 203) < 0.8)
         lo = gain;
      else
         hi = gain;
   }
   norm = tone_curve(gain * peak_nits / 203);
   for (i = 0; i < 3; i++)
   {
      double e = rgb[i] < 0 ? 0 : rgb[i] > 1 ? 1 : rgb[i];
      double light = transfer == 16 ? pq_nits(e)
         : 1000 * pow(hlg_scene(e), 1.2);
      linear[i] = tone_curve(gain * light / 203) / norm;
      if (linear[i] > 1) linear[i] = 1;
   }
   for (i = 0; i < 3; i++)
   {
      double x = 0;
      if (primaries == 1)
         x = linear[i];
      else
         for (j = 0; j < 3; j++) x += gamut[i][j] * linear[j];
      if (x < 0) x = 0;
      if (x > 1) x = 1;
      rgb[i] = x <= 0.0031308 ? 12.92 * x
         : 1.055 * pow(x, 1.0 / 2.4) - 0.055;
   }
}

static void lane_sdr_color_precision(void)
{
   unsigned transfer, full, y, u, v, c, i;
   int worst8 = 0, worst10 = 0;
   for (transfer = 16; transfer <= 18; transfer += 2)
      for (full = 0; full < 2; full++)
         for (y = full ? 0 : 64; y <= (full ? 1023 : 940); y += 17)
            for (u = 448; u <= 576; u += 64)
               for (v = 448; v <= 576; v += 64)
               {
                  uint16_t luma[4], cb = (uint16_t)u, cr = (uint16_t)v;
                  uint32_t got8[4], got10[4];
                  double ref[3];
                  reference_sdr(y, u, v, full, transfer, 9, 1000, ref);
                  for (i = 0; i < 4; i++) luma[i] = (uint16_t)y;
                  rwebm_video_blit_i420_hbd(got8, 2, 2, 2, luma, 2,
                        &cb, &cr, 1, 9, transfer, full ? 2 : 1, 0, 0);
                  rwebm_video_blit_i420_10bit(got10, 2, 2, 2, luma, 2,
                        &cb, &cr, 1, 9, transfer, full ? 2 : 1, 0);
                  for (c = 0; c < 3; c++)
                  {
                     int e8 = abs((int)((got8[0] >> (16 - 8 * c)) & 255)
                           - (int)(ref[c] * 255 + 0.5));
                     int e10 = abs((int)((got10[0] >> (20 - 10 * c)) & 1023)
                           - (int)(ref[c] * 1023 + 0.5));
                     if (e8 > worst8) worst8 = e8;
                     if (e10 > worst10) worst10 = e10;
                  }
               }
   CHECK(worst8 <= 2, "PQ/HLG SDR color differs by %d eight-bit units", worst8);
   CHECK(worst10 <= 6, "PQ/HLG SDR color differs by %d ten-bit units", worst10);
   printf("[info] PQ/HLG full/limited color precision: max %d / %d units\n",
         worst8, worst10);
}

static void lane_source_primaries(void)
{
   unsigned transfer, full, primaries, peak, c, i;
   static const unsigned values[][3] = {
      {128,448,576}, {253,568,448}, {400,448,576},
      {512,576,448}, {700,448,576}, {900,576,448}
   };
   int worst8 = 0, worst10 = 0;
   for (transfer = 16; transfer <= 18; transfer += 2)
      for (full = 0; full < 2; full++)
         for (primaries = 1; primaries <= 9; primaries += 8)
            for (peak = 1000; peak <= 4000; peak += 3000)
               for (i = 0; i < sizeof(values) / sizeof(values[0]); i++)
               {
                  uint16_t luma[4], cb = (uint16_t)values[i][1];
                  uint16_t cr = (uint16_t)values[i][2];
                  uint32_t got8[4], got10[4], legacy[4];
                  double ref[3];
                  unsigned k;
                  reference_sdr(values[i][0], cb, cr, full, transfer,
                        primaries, transfer == 16 ? peak : 1000, ref);
                  for (k = 0; k < 4; k++) luma[k] = (uint16_t)values[i][0];
                  rwebm_video_blit_i420_hbd_color(got8, 2, 2, 2, luma, 2,
                        &cb, &cr, 1, 9, transfer, full ? 2 : 1, peak, primaries, 0);
                  rwebm_video_blit_i420_10bit_color(got10, 2, 2, 2, luma, 2,
                        &cb, &cr, 1, 9, transfer, full ? 2 : 1, peak, primaries);
                  for (c = 0; c < 3; c++)
                  {
                     int e8 = abs((int)((got8[0] >> (16 - 8 * c)) & 255)
                           - (int)(ref[c] * 255 + 0.5));
                     int e10 = abs((int)((got10[0] >> (20 - 10 * c)) & 1023)
                           - (int)(ref[c] * 1023 + 0.5));
                     if (e8 > worst8) worst8 = e8;
                     if (e10 > worst10) worst10 = e10;
                  }
                  if (primaries == 9)
                  {
                     rwebm_video_blit_i420_hbd(legacy, 2, 2, 2, luma, 2,
                           &cb, &cr, 1, 9, transfer, full ? 2 : 1, peak, 0);
                     CHECK(!memcmp(legacy, got8, sizeof(legacy)), "legacy 8-bit wrapper changed");
                     rwebm_video_blit_i420_10bit(legacy, 2, 2, 2, luma, 2,
                           &cb, &cr, 1, 9, transfer, full ? 2 : 1, peak);
                     CHECK(!memcmp(legacy, got10, sizeof(legacy)), "legacy 10-bit wrapper changed");
                  }
               }
   CHECK(worst8 <= 2 && worst10 <= 6,
         "source-primary/peak SDR mismatch %d / %d", worst8, worst10);
   printf("[info] source primaries and peak: max %d / %d units\n", worst8, worst10);
}

static void lane_709_scrgb(void)
{
   uint16_t y[4] = {400,400,400,400}, u = 448, v = 576;
   uint16_t out[16];
   unsigned transfer, c;
   /* The unchanged linear blit's quantized encoded RGB step. The
    * reference then evaluates PQ/HLG directly in the source primaries. */
   double encoded[3] = {
      ((298 * (400-64) + 431 * (576-512) + 128) >> 8) / 1023.0,
      ((298 * (400-64) - 48 * (448-512) - 167 * (576-512) + 128) >> 8) / 1023.0,
      ((298 * (400-64) + 548 * (448-512) + 128) >> 8) / 1023.0
   };
   for (transfer = 16; transfer <= 18; transfer += 2)
   {
      double ref[3];
      CHECK(rwebm_video_blit_i420_fp16_color(out, 2, 2, 2, y, 2,
            &u, &v, 1, 9, transfer, 1, 1), "BT709 HDR linear blit refused");
      if (transfer == 16)
         for (c = 0; c < 3; c++) ref[c] = pq_nits(encoded[c]) / 80;
      else
      {
         double scene[3], ys;
         for (c = 0; c < 3; c++) scene[c] = hlg_scene(encoded[c]);
         ys = .2126 * scene[0] + .7152 * scene[1] + .0722 * scene[2];
         for (c = 0; c < 3; c++) ref[c] = scene[c] * 1000 / 80 * pow(ys, .2);
      }
      for (c = 0; c < 3; c++)
         CHECK(fabs(half_to_double(out[c]) - ref[c]) <= fabs(ref[c]) * .002 + .002,
               "BT709 linear HDR changed primaries: transfer %u channel %u", transfer, c);
      CHECK(out[3] == 0x3c00, "BT709 linear HDR alpha changed");
   }
}

static void lane_unspecified_hdr_matrix(void)
{
   static const unsigned matrices[] = {0,2,9};
   static const unsigned primaries[] = {0,1,2,9};
   uint16_t y[4] = {256,400,600,900}, u = 448, v = 576;
   uint32_t out8[3][4], out10[3][4];
   uint16_t linear[3][16];
   unsigned t, full, p, m;
   for (t = 16; t <= 18; t += 2)
      for (full = 0; full < 2; full++)
         for (p = 0; p < sizeof(primaries) / sizeof(primaries[0]); p++)
         {
            for (m = 0; m < 3; m++)
            {
               rwebm_video_blit_i420_hbd_color(out8[m],2,2,2,y,2,&u,&v,1,
                     matrices[m],t,full?2:1,1000,primaries[p],0);
               rwebm_video_blit_i420_10bit_color(out10[m],2,2,2,y,2,&u,&v,1,
                     matrices[m],t,full?2:1,1000,primaries[p]);
               CHECK(rwebm_video_blit_i420_fp16_color(linear[m],2,2,2,y,2,&u,&v,1,
                     matrices[m],t,full?2:1,primaries[p]), "HDR default matrix refused");
            }
            for (m = 0; m < 2; m++)
            {
               CHECK(!memcmp(out8[m],out8[2],sizeof(out8[m])),
                     "HDR matrix %u differs from BT2020 in SDR8",matrices[m]);
               CHECK(!memcmp(out10[m],out10[2],sizeof(out10[m])),
                     "HDR matrix %u differs from BT2020 in SDR10",matrices[m]);
               CHECK(!memcmp(linear[m],linear[2],sizeof(linear[m])),
                     "HDR matrix %u differs from BT2020 in scRGB",matrices[m]);
            }
         }
}

static void lane_prepared_tone(void)
{
   static rwebm_video_hdr_tone_t tone;
   static const unsigned peaks[] = {0, 399, 400, 1000, 4000, 10000, 10001, 65535};
   static const uint16_t y[4] = {256, 400, 600, 900}, u[1] = {448}, v[1] = {576};
   uint32_t expected[4], actual[4];
   unsigned p, transfer, range, primaries;
   for (p = 0; p < sizeof(peaks) / sizeof(peaks[0]); p++)
   {
      rwebm_video_hdr_tone_init(&tone, peaks[p]);
      for (transfer = 16; transfer <= 18; transfer += 2)
         for (range = 1; range <= 2; range++)
            for (primaries = 1; primaries <= 9; primaries += 8)
            {
               rwebm_video_blit_i420_hbd_color(expected, 2, 2, 2, y, 2, u, v, 1,
                     9, transfer, range, peaks[p], primaries, 0);
               rwebm_video_blit_i420_hbd_tone(actual, 2, 2, 2, y, 2, u, v, 1,
                     9, transfer, range, peaks[p], primaries, 0, &tone);
               CHECK(!memcmp(expected, actual, sizeof(actual)), "prepared SDR8 peak %u", peaks[p]);
               rwebm_video_blit_i420_10bit_color(expected, 2, 2, 2, y, 2, u, v, 1,
                     9, transfer, range, peaks[p], primaries);
               rwebm_video_blit_i420_10bit_tone(actual, 2, 2, 2, y, 2, u, v, 1,
                     9, transfer, range, peaks[p], primaries, &tone);
               CHECK(!memcmp(expected, actual, sizeof(actual)), "prepared SDR10 peak %u", peaks[p]);
            }
   }
}

static void lane_metadata_peak(void)
{
   video_mastering_display_t mastering;
   memset(&mastering, 0, sizeof(mastering));
   mastering.present = 0x100u;
   mastering.luminance_max = 40000000;
   CHECK(video_color_tonemap_peak_nits(16, 0, &mastering) == 4000, "mastering peak fallback");
   CHECK(video_color_tonemap_peak_nits(16, 400, &mastering) == 400, "content peak precedes mastering");
   CHECK(video_color_tonemap_peak_nits(16, 399, &mastering) == 1000, "invalid signalled content peak defaults");
   CHECK(video_color_tonemap_peak_nits(18, 400, &mastering) == 1000, "HLG ignores source peak metadata");
   mastering.present = 0;
   CHECK(video_color_tonemap_peak_nits(16, 0, &mastering) == 1000, "absent mastering maximum");
   CHECK(video_color_tonemap_peak_nits(16, 0, NULL) == 1000, "absent peak metadata");
}

int main(void)
{
   lane_fp16_ramp(16, "PQ");
   lane_fp16_ramp(18, "HLG");
   lane_fp16_gamut();
   lane_fp16_refuses_sdr();
   lane_hlg_tonemapped();
   lane_sdr_color_precision();
   lane_source_primaries();
   lane_709_scrgb();
   lane_unspecified_hdr_matrix();
   lane_prepared_tone();
   lane_metadata_peak();
   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] image_hdr_blit_test\n");
   return 0;
}

/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <formats/rav1_grain.h>

static rav1_sequence_t seq;
static rav1_frame_prefix_t frame;
static rav1_frame_setup_t setup;
static rav1_grain_header_t grain, zero;
static rav1_grain_t previous, saved;
static const rav1_grain_t *references[8];
static uint8_t data[512];
static unsigned position, failures;

#define CHECK(x)                                                               \
   do                                                                          \
   {                                                                           \
      if (!(x))                                                                \
      {                                                                        \
         fprintf(stderr, "line %u: %s\n", (unsigned)__LINE__, #x);             \
         failures++;                                                           \
      }                                                                        \
   } while (0)

static void put(unsigned value, unsigned count)
{
   unsigned i;
   for (i = count; i; i--)
   {
      if ((value >> (i - 1)) & 1)
         data[position / 8] |= 1u << (7 - position % 8);
      position++;
   }
}

static rav1_bits_t reader(size_t size)
{
   rav1_bits_t bits;
   bits.data = data;
   bits.size = size;
   bits.byte = 0;
   bits.bit = 3;
   bits.failed = 0;
   return bits;
}

static void reset(void)
{
   unsigned i;
   memset(&seq, 0, sizeof(seq));
   memset(&frame, 0, sizeof(frame));
   memset(&setup, 0, sizeof(setup));
   memset(data, 0, sizeof(data));
   memset(references, 0, sizeof(references));
   position = 3;
   seq.film_grain_params_present = 1;
   frame.frame_type = RAV1_FRAME_KEY;
   frame.show_frame = 1;
   setup.valid_mask = 255;
   for (i = 0; i < 7; i++)
      setup.ref_frame_idx[i] = i;
}

static void reject(size_t size)
{
   rav1_bits_t bits = reader(size);
   saved = previous;
   memset(&grain, 255, sizeof(grain));
   CHECK(!rav1_grain_parse(&grain, &bits, &seq, &frame, &setup, references));
   CHECK(!memcmp(&grain, &zero, sizeof(zero)));
   CHECK(!memcmp(&previous, &saved, sizeof(saved)));
   CHECK(!bits.byte && bits.bit == 3 && !bits.failed);
}

static void full(unsigned points, unsigned lag, unsigned mono,
                 unsigned from_luma)
{
   unsigned plane, i, count;
   reset();
   seq.monochrome = mono;
   put(1, 1);
   put(0xabcd, 16);
   put(points ? 14 : 0, 4);
   for (plane = 0; plane < 3; plane++)
   {
      count = points ? (plane ? 10 : 14) : 0;
      if (plane == 1)
      {
         if (!mono)
            put(from_luma, 1);
         if (mono || from_luma)
            break;
      }
      if (plane)
         put(count, 4);
      for (i = 0; i < count; i++)
      {
         put(2 * i, 8);
         put(255 - i, 8);
      }
   }
   put(3, 2);
   put(lag, 2);
   for (plane = 0; plane < 3; plane++)
   {
      count = 2 * lag * (lag + 1) + (plane && points);
      if ((plane && !mono && from_luma) ||
          (points && (!plane || (!mono && !from_luma))))
         for (i = 0; i < count; i++)
            put(i & 1 ? 255 : 0, 8);
   }
   put(3, 2);
   put(2, 2);
   if (!mono && !from_luma && points)
      for (plane = 0; plane < 2; plane++)
      {
         put(255, 8);
         put(128, 8);
         put(511, 9);
      }
   put(1, 1);
   put(1, 1);
}

static void test_full(void)
{
   rav1_bits_t bits;
   unsigned i, lag, mono, from_luma, points, plane, count;
   size_t size;
   for (lag = 0; lag < 4; lag++)
      for (mono = 0; mono < 2; mono++)
         for (from_luma = 0; from_luma < 2; from_luma++)
            for (points = 0; points < 2; points++)
            {
               full(points, lag, mono, from_luma);
               size = (position + 7) / 8;
               bits = reader(size);
               CHECK(rav1_grain_parse(&grain, &bits, &seq, &frame, &setup,
                                      references));
               CHECK(grain.params.apply && grain.params.seed == 0xabcd &&
                     grain.update);
               CHECK(grain.params.scaling_shift == 11 &&
                     grain.params.ar_coeff_shift == 9);
               CHECK(grain.params.scale_shift == 2 &&
                     grain.params.ar_coeff_lag == lag);
               CHECK(grain.params.overlap &&
                     grain.params.clip_to_restricted_range);
               for (plane = 0; plane < 3; plane++)
               {
                  unsigned active = !plane || (!mono && !from_luma);
                  CHECK(grain.params.num_points[plane] ==
                        (points && active ? (plane ? 10u : 14u) : 0));
                  count = 2 * lag * (lag + 1) + (plane && points);
                  if ((plane && !mono && from_luma) || (points && active))
                     for (i = 0; i < count; i++)
                        CHECK(grain.params.ar_coeffs[plane][i] ==
                              (i & 1 ? 127 : -128));
               }
               CHECK(bits.byte * 8 + bits.bit == position);
               for (i = 0; i < size; i++)
                  reject(i);
            }
}

static void test_inheritance(void)
{
   rav1_bits_t bits;
   unsigned i;
   full(1, 3, 0, 0);
   bits = reader((position + 7) / 8);
   CHECK(rav1_grain_parse(&grain, &bits, &seq, &frame, &setup, references));
   previous = grain.params;
   reset();
   frame.frame_type = RAV1_FRAME_INTER;
   references[3] = &previous;
   put(1, 1);
   put(1234, 16);
   put(0, 1);
   put(3, 3);
   bits = reader(3);
   CHECK(rav1_grain_parse(&grain, &bits, &seq, &frame, &setup, references));
   CHECK(!grain.update && grain.reference_idx == 3 &&
         grain.params.seed == 1234);
   grain.params.seed = previous.seed;
   CHECK(!memcmp(&grain.params, &previous, sizeof(previous)));
   for (i = 0; i < 3; i++)
      reject(i);
   setup.valid_mask &= ~(1u << 3);
   reject(3);
   setup.valid_mask = 255;
   setup.ref_frame_idx[3] = 2;
   reject(3);
   setup.ref_frame_idx[3] = 3;
   references[3] = NULL;
   reject(3);
   references[3] = &grain.params;
   grain.params = previous;
   bits = reader(3);
   CHECK(!rav1_grain_parse(&grain, &bits, &seq, &frame, &setup, references));
   CHECK(!memcmp(&grain.params, &previous, sizeof(previous)));
}

static void test_bad_and_gates(void)
{
   rav1_bits_t bits;
   unsigned i;
   reset();
   seq.film_grain_params_present = 0;
   bits = reader(0);
   CHECK(rav1_grain_parse(&grain, &bits, &seq, &frame, NULL, NULL));
   CHECK(!memcmp(&grain, &zero, sizeof(zero)) && bits.bit == 3);
   seq.film_grain_params_present = 1;
   frame.show_frame = frame.showable_frame = 0;
   CHECK(rav1_grain_parse(&grain, &bits, &seq, &frame, NULL, NULL));
   CHECK(!memcmp(&grain, &zero, sizeof(zero)) && bits.bit == 3);
   frame.showable_frame = 1;
   put(0, 1);
   bits = reader(1);
   CHECK(rav1_grain_parse(&grain, &bits, &seq, &frame, NULL, NULL));
   CHECK(!memcmp(&grain, &zero, sizeof(zero)) && bits.bit == position);
   for (i = 0; i < 4; i++)
   {
      reset();
      put(1, 1);
      put(0, 16);
      put(i == 0 ? 15 : 2, 4);
      put(10, 8);
      put(1, 8);
      put(i == 1 ? 10 : i == 2 ? 9 : 20, 8);
      put(2, 8);
      if (i == 3)
      {
         put(0, 1);
         put(11, 4);
      }
      reject((position + 7) / 8);
   }
   reset();
   seq.subsampling_x = seq.subsampling_y = 1;
   put(1, 1);
   put(0, 16);
   put(1, 4);
   put(0, 16);
   put(0, 1);
   put(0, 4);
   put(1, 4);
   put(0, 16);
   reject((position + 7) / 8);
}

int main(void)
{
   test_full();
   test_inheritance();
   test_bad_and_gates();
   printf("AV1 film grain: %u failures\n", failures);
   return failures ? 1 : 0;
}

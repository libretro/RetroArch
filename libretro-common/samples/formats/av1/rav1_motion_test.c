/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <formats/rav1_motion.h>

static rav1_sequence_t seq;
static rav1_frame_prefix_t frame;
static rav1_frame_setup_t setup;
static rav1_reference_t refs[8];
static rav1_motion_t motion, zero;
static rav1_global_motion_t previous, saved;
static uint8_t data[256];
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

static void put_ns(unsigned value, unsigned n)
{
   unsigned width = 0, m;
   while (n >> width)
      width++;
   m = (1u << width) - n;
   if (value < m)
      put(value, width - 1);
   else
      put(value + m, width);
}

static void put_ref(unsigned value, unsigned n, unsigned reference)
{
   unsigned width = 3, base = 0, step, encoded;
   if (2 * reference > n)
   {
      value = n - 1 - value;
      reference = n - 1 - reference;
   }
   encoded = value > 2 * reference ? value
             : value >= reference  ? 2 * (value - reference)
                                   : 2 * (reference - value) - 1;
   for (;;)
   {
      step = 1u << width;
      if (n - base <= 3 * step)
      {
         put_ns(encoded - base, n - base);
         return;
      }
      put(encoded - base >= step, 1);
      if (encoded - base < step)
      {
         put(encoded - base, width);
         return;
      }
      base += step;
      if (base != 8)
         width++;
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
   position = 3;
   seq.enable_order_hint = seq.enable_warped_motion = 1;
   seq.order_hint_bits = 4;
   frame.frame_type = RAV1_FRAME_INTER;
   frame.primary_ref_frame = 7;
   frame.order_hint = 8;
   setup.valid_mask = 255;
   for (i = 0; i < 8; i++)
   {
      refs[i].order_hint = i;
      if (i < 7)
         setup.ref_frame_idx[i] = i;
   }
}

static void reject(size_t size)
{
   rav1_bits_t bits = reader(size);
   memcpy(&saved, &previous, sizeof(saved));
   memset(&motion, 255, sizeof(motion));
   CHECK(!rav1_motion_parse(&motion, &bits, &seq, &frame, &setup, refs,
                            &previous));
   CHECK(!memcmp(&zero, &motion, sizeof(zero)));
   CHECK(!memcmp(&saved, &previous, sizeof(saved)));
   CHECK(!bits.byte && bits.bit == 3 && !bits.failed);
}

static void test_subexp(void)
{
   static const unsigned sizes[] = {1, 2, 3, 17, 513, 1025, 8193, 65536};
   rav1_bits_t bits;
   unsigned i, j, value, n, reference;
   for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
   {
      n = sizes[i];
      for (j = 0; j < 3; j++)
      {
         reference = j == 0 ? 0 : j == 1 ? n / 2 : n - 1;
         for (value = 0; value < n; value++)
         {
            memset(data, 0, sizeof(data));
            position = 3;
            put_ref(value, n, reference);
            bits = reader((position + 7) / 8);
            CHECK(rav1_bits_subexp_ref(&bits, n, reference) == value);
            CHECK(!bits.failed && bits.byte * 8 + bits.bit == position);
         }
      }
   }
   bits = reader(0);
   rav1_bits_subexp_ref(&bits, 8193, 4096);
   CHECK(bits.failed);
   bits = reader(1);
   rav1_bits_subexp_ref(&bits, 0, 0);
   CHECK(bits.failed);
   bits = reader(1);
   rav1_bits_subexp_ref(&bits, 3, 3);
   CHECK(bits.failed);
}

static void test_skip(void)
{
   rav1_bits_t bits;
   unsigned i, mode;
   for (mode = 0; mode < 6; mode++)
   {
      reset();
      for (i = 0; i < 8; i++)
         refs[i].order_hint = mode == 0 ? 8 : mode == 1 ? 9 : 6;
      if (mode == 2)
         refs[2].order_hint = 7;
      if (mode == 3)
         refs[4].order_hint = 9;
      if (mode == 4)
      {
         refs[0].order_hint = 7;
         refs[1].order_hint = 15;
      }
      if (mode == 5)
      {
         for (i = 0; i < 8; i++)
            refs[i].order_hint = 8;
         refs[0].order_hint = 0;
      }
      put(1, 1);
      if (mode >= 2)
         put(1, 1);
      put(1, 1);
      put(0, 1);
      put(0, 7);
      bits = reader((position + 7) / 8);
      CHECK(
          rav1_motion_parse(&motion, &bits, &seq, &frame, &setup, refs, NULL));
      CHECK(motion.reference_select && motion.allow_warped_motion);
      CHECK(motion.skip_mode_allowed == (mode >= 2));
      CHECK(motion.skip_mode_present == (mode >= 2));
      if (mode == 2)
         CHECK(motion.skip_mode_frame[0] == 1 &&
               motion.skip_mode_frame[1] == 3);
      if (mode == 3)
         CHECK(motion.skip_mode_frame[0] == 1 &&
               motion.skip_mode_frame[1] == 5);
      if (mode == 4 || mode == 5)
         CHECK(motion.skip_mode_frame[0] == 1 &&
               motion.skip_mode_frame[1] == 2);
      CHECK(bits.byte * 8 + bits.bit == position);
      for (i = 0; i < (position + 7) / 8; i++)
         reject(i);
   }
   reset();
   frame.frame_is_intra = 1;
   put(1, 1);
   bits = reader(1);
   CHECK(rav1_motion_parse(&motion, &bits, &seq, &frame, &setup, NULL, NULL));
   CHECK(motion.reduced_tx_set && !motion.reference_select &&
         !motion.allow_warped_motion);
   CHECK(bits.bit == position);
   for (i = 0; i < 7; i++)
      CHECK(motion.global.params[i][2] == 65536 &&
            motion.global.params[i][5] == 65536);
}

static void test_global(void)
{
   rav1_bits_t bits;
   unsigned type, hp, pass, i, j, index, abs_bits, prec_bits, mx;
   int value, sub, expected[6];
   for (type = 1; type < 4; type++)
      for (hp = 0; hp < 2; hp++)
         for (pass = 0; pass < 2; pass++)
         {
            reset();
            frame.primary_ref_frame = pass ? 0 : 7;
            setup.allow_high_precision_mv = hp;
            if (pass)
               previous.params[0][0] = -1;
            put(0, 1); /* reference_select */
            put(0, 1); /* warped motion */
            put(1, 1); /* reduced TX */
            put(1, 1);
            put(type == 2, 1);
            if (type != 2)
               put(type == 1, 1);
            memset(expected, 0, sizeof(expected));
            expected[2] = expected[5] = 65536;
            for (j = 0; j < 6; j++)
            {
               index = (j + 2) % 6;
               if ((type == 1 && index >= 2) ||
                   (type == 2 && (index == 4 || index == 5)))
                  continue;
               abs_bits = index < 2 && type == 1 ? 8 + hp : 12;
               prec_bits = index < 2 ? (type == 1 ? 2 + hp : 6) : 15;
               mx = 1u << abs_bits;
               sub = index % 3 == 2 ? 1 << prec_bits : 0;
               value = j == 0 ? -(int)mx : j == 1 ? (int)mx : (int)j - 4;
               if (pass)
                  put_ref((unsigned)(value + mx), 2 * mx + 1,
                          (unsigned)((previous.params[0][index] >>
                                      (16 - prec_bits)) -
                                     sub + mx));
               else
                  put_ref((unsigned)(value + mx), 2 * mx + 1, mx);
               expected[index] = value * (1 << (16 - prec_bits)) +
                                 (index % 3 == 2 ? 65536 : 0);
            }
            if (type == 2)
            {
               expected[4] = -expected[3];
               expected[5] = expected[2];
            }
            put(0, 6);
            bits = reader((position + 7) / 8);
            CHECK(rav1_motion_parse(&motion, &bits, &seq, &frame, &setup, refs,
                                    pass ? &previous : NULL));
            CHECK(motion.global.type[0] == type);
            for (i = 0; i < 6; i++)
               CHECK(motion.global.params[0][i] == expected[i]);
            CHECK(bits.byte * 8 + bits.bit == position);
            for (i = 0; i < (position + 7) / 8; i++)
               reject(i);
            /* Retain the decoded coefficients across rejection probes. */
            for (i = 0; i < 6; i++)
               previous.params[0][i] = expected[i];
         }
   saved = motion.global;
   bits = reader(sizeof(data));
   CHECK(!rav1_motion_parse(&motion, &bits, &seq, &frame, &setup, refs,
                            &motion.global));
   CHECK(!memcmp(&saved, &motion.global, sizeof(saved)));
   previous.params[0][2] = INT32_MAX;
   reject(sizeof(data));
}

static void test_gates(void)
{
   rav1_bits_t bits;
   unsigned i, resilient, order, compound;
   for (resilient = 0; resilient < 2; resilient++)
      for (order = 0; order < 2; order++)
         for (compound = 0; compound < 2; compound++)
         {
            reset();
            frame.error_resilient = resilient;
            seq.enable_order_hint = order;
            put(compound, 1);
            if (order && compound)
               put(0, 1);
            if (!resilient)
               put(0, 1);
            put(1, 1);
            put(0, 7);
            bits = reader((position + 7) / 8);
            CHECK(rav1_motion_parse(&motion, &bits, &seq, &frame, &setup, refs,
                                    NULL));
            CHECK(motion.reference_select == compound &&
                  !motion.allow_warped_motion);
            CHECK(motion.skip_mode_allowed == (order && compound));
            CHECK(bits.byte * 8 + bits.bit == position);
            for (i = 0; i < (position + 7) / 8; i++)
               reject(i);
            setup.ref_frame_idx[0] = 8;
            reject(sizeof(data));
         }
}

int main(void)
{
   test_subexp();
   test_skip();
   test_global();
   test_gates();
   printf("AV1 motion headers: %u failures\n", failures);
   return failures ? 1 : 0;
}

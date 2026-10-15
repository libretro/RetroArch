/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <formats/rav1_sequence.h>

static rav1_sequence_t sequence, zero;
static uint8_t payload[512];
static unsigned position, failures;
static unsigned timing_position, uvlc_position, op_position[32];
static unsigned frame_id_position, chroma_position;

#define CHECK(x)                                                               \
   do                                                                          \
   {                                                                           \
      if (!(x))                                                                \
      {                                                                        \
         fprintf(stderr, "line %u: %s\n", (unsigned)__LINE__, #x);             \
         failures++;                                                           \
      }                                                                        \
   } while (0)

static void put(uint32_t value, unsigned count)
{
   unsigned i;
   for (i = count; i; i--)
   {
      if ((value >> (i - 1)) & 1)
         payload[position / 8] |= 1 << (7 - position % 8);
      position++;
   }
}

static size_t make_sequence(unsigned profile, unsigned depth, unsigned mono,
                            unsigned x, unsigned y, unsigned rgb,
                            unsigned reduced, unsigned model)
{
   unsigned i;
   position = 0;
   memset(payload, 0, sizeof(payload));
   put(profile, 3);
   put(reduced, 1);
   put(reduced, 1);
   if (reduced)
      put(0, 5);
   else
   {
      put(model, 1);
      if (model)
      {
         timing_position = position;
         put(1001, 32);
         put(60000, 32);
         put(1, 1);
         uvlc_position = position;
         put(6, 5); /* UVLC 5: 00 110. */
         put(1, 1);
         put(31, 5);
         put(1000, 32);
         put(16, 5);
         put(23, 5);
      }
      put(model, 1);
      put(model ? 31 : 0, 5);
      for (i = 0; i < (model ? 32u : 1u); i++)
      {
         op_position[i] = position;
         put(model ? 0x101 + i : 0, 12);
         put(model ? 8 : 0, 5);
         if (model)
         {
            put(i & 1, 1);
            put(i & 1, 1);
            if (i & 1)
            {
               put(UINT32_MAX - i, 32);
               put(i, 32);
               put(1, 1);
            }
            put(i & 1, 1);
            if (i & 1)
               put(15, 4);
         }
      }
   }
   put(15, 4);
   put(15, 4);
   put(65535, 16);
   put(32767, 16);
   if (!reduced)
   {
      put(model, 1);
      if (model)
      {
         frame_id_position = position;
         put(6, 4);
         put(7, 3);
      }
   }
   put(7, 3);
   if (!reduced)
   {
      put(127, 7);
      put(1, 1); /* Select screen tools. */
      put(1, 1); /* Select integer MV. */
      put(7, 3);
   }
   put(7, 3);
   put(depth > 8, 1);
   if (profile == 2 && depth > 8)
      put(depth == 12, 1);
   if (profile != 1)
      put(mono, 1);
   put(rgb, 1);
   if (rgb)
   {
      put(1, 8);
      put(13, 8);
      put(0, 8);
   }
   if (mono || !rgb)
      put(0, 1);
   if (!mono)
   {
      if (!rgb)
      {
         if (profile == 2 && depth == 12)
         {
            put(x, 1);
            if (x)
               put(y, 1);
         }
         if (x && y)
         {
            chroma_position = position;
            put(2, 2);
         }
      }
      put(1, 1);
   }
   put(1, 1); /* Film grain. */
   put(1, 1); /* Trailing bit. */
   return (position + 7) / 8;
}

static void reject(size_t size)
{
   memset(&sequence, 255, sizeof(sequence));
   CHECK(!rav1_sequence_parse(&sequence, payload, size));
   CHECK(!memcmp(&sequence, &zero, sizeof(sequence)));
}

static void test_layouts(void)
{
   static const unsigned layouts[][6] = {
       {0, 8, 0, 1, 1, 0},  {0, 10, 0, 1, 1, 0}, {0, 8, 1, 1, 1, 0},
       {2, 12, 1, 1, 1, 0}, {1, 8, 0, 0, 0, 0},  {1, 10, 0, 0, 0, 1},
       {2, 8, 0, 1, 0, 0},  {2, 10, 0, 1, 0, 0}, {2, 12, 0, 1, 1, 0},
       {2, 12, 0, 1, 0, 0}, {2, 12, 0, 0, 0, 0}, {2, 12, 0, 0, 0, 1}};
   unsigned i, reduced;
   size_t size, n;
   for (i = 0; i < sizeof(layouts) / sizeof(layouts[0]); i++)
      for (reduced = 0; reduced < 2; reduced++)
      {
         const unsigned *l = layouts[i];
         size = make_sequence(l[0], l[1], l[2], l[3], l[4], l[5], reduced, 0);
         CHECK(rav1_sequence_parse(&sequence, payload, size));
         CHECK(sequence.profile == l[0] && sequence.bit_depth == l[1]);
         CHECK(sequence.monochrome == l[2] && sequence.subsampling_x == l[3] &&
               sequence.subsampling_y == l[4]);
         CHECK(sequence.max_width == 65536 && sequence.max_height == 32768);
         CHECK(sequence.force_screen_content_tools == 2 &&
               sequence.force_integer_mv == 2);
         CHECK(sequence.order_hint_bits == (reduced ? 0u : 8u));
         CHECK(sequence.film_grain_params_present &&
               sequence.enable_restoration);
         CHECK(sequence.color_range == l[5]);
         for (n = 0; n < size; n++)
            reject(n);
         payload[(position - 1) / 8] ^= 1 << (7 - (position - 1) % 8);
         reject(size);
      }
   size = make_sequence(0, 8, 0, 1, 1, 1, 0, 0);
   reject(size); /* RGB identity is incompatible with profile zero. */
   size = make_sequence(2, 10, 0, 0, 0, 1, 0, 0);
   reject(size);
   size = make_sequence(0, 8, 0, 1, 1, 0, 1, 0);
   payload[0] &= ~0x10;
   reject(size);
}

static void test_models(void)
{
   size_t size = make_sequence(0, 10, 0, 1, 1, 0, 0, 1), n;
   unsigned i;
   CHECK(rav1_sequence_parse(&sequence, payload, size));
   CHECK(sequence.num_units_in_display_tick == 1001 &&
         sequence.time_scale == 60000);
   CHECK(sequence.num_ticks_per_picture_minus_one == 5);
   CHECK(sequence.buffer_delay_length == 32 &&
         sequence.buffer_removal_time_length == 17);
   CHECK(sequence.frame_presentation_time_length == 24 &&
         sequence.frame_id_length == 16);
   CHECK(sequence.operating_point_count == 32);
   for (i = 0; i < 32; i++)
   {
      const rav1_operating_point_t *op = &sequence.operating_points[i];
      CHECK(op->idc == 0x101 + i && op->level == 8 && op->tier == (i & 1));
      CHECK(op->decoder_model_present == (i & 1));
      CHECK(op->initial_display_delay == ((i & 1) ? 16u : 0u));
      if (i & 1)
         CHECK(op->decoder_buffer_delay == UINT32_MAX - i &&
               op->encoder_buffer_delay == i);
   }
   for (n = 0; n < size; n++)
      reject(n);
}

static void overwrite(unsigned offset, uint32_t value, unsigned count)
{
   unsigned i;
   for (i = count; i; i--)
   {
      unsigned mask = 1 << (7 - offset % 8);
      payload[offset / 8] &= ~mask;
      if ((value >> (i - 1)) & 1)
         payload[offset / 8] |= mask;
      offset++;
   }
}

static void test_invalid(void)
{
   size_t size;
   unsigned i;
   size = make_sequence(0, 10, 0, 1, 1, 0, 0, 1);
   overwrite(timing_position, 0, 32);
   reject(size);
   size = make_sequence(0, 10, 0, 1, 1, 0, 0, 1);
   overwrite(timing_position + 32, 0, 32);
   reject(size);
   size = make_sequence(0, 10, 0, 1, 1, 0, 0, 1);
   overwrite(uvlc_position, 0, 32);
   reject(size);
   size = make_sequence(0, 10, 0, 1, 1, 0, 0, 1);
   overwrite(op_position[1], 0x101, 12);
   reject(size);
   size = make_sequence(0, 10, 0, 1, 1, 0, 0, 1);
   overwrite(op_position[0] + 12, 24, 5);
   reject(size);
   size = make_sequence(0, 10, 0, 1, 1, 0, 0, 1);
   overwrite(frame_id_position, 15, 4);
   reject(size);
   size = make_sequence(0, 10, 0, 1, 1, 0, 1, 0);
   overwrite(chroma_position, 3, 2);
   reject(size);
   size = make_sequence(0, 10, 0, 1, 1, 0, 1, 0);
   payload[size] = 1;
   reject(size + 1);
   /* Every single-bit mutation must either parse safely or clear the output. */
   size = make_sequence(0, 10, 0, 1, 1, 0, 0, 1);
   for (i = 0; i < size * 8; i++)
   {
      payload[i / 8] ^= 1 << (7 - i % 8);
      if (!rav1_sequence_parse(&sequence, payload, size))
         CHECK(!memcmp(&sequence, &zero, sizeof(sequence)));
      else
         CHECK(sequence.operating_point_count <= 32 &&
               sequence.bit_depth <= 12);
      payload[i / 8] ^= 1 << (7 - i % 8);
   }
}

static void test_config(void)
{
   rav1_config_t config;
   size_t size = make_sequence(0, 10, 0, 1, 1, 0, 1, 0);
   unsigned *field;
   unsigned i;
   memset(&config, 0, sizeof(config));
   config.sequence.type = 1;
   config.sequence.payload = payload;
   config.sequence.payload_size = size;
   config.bit_depth = 10;
   config.subsampling_x = config.subsampling_y = 1;
   config.chroma_sample_position = 2;
   CHECK(rav1_config_sequence(&sequence, &config));
   for (i = 0; i < 8; i++)
   {
      switch (i)
      {
      case 0:
         field = &config.profile;
         break;
      case 1:
         field = &config.level;
         break;
      case 2:
         field = &config.tier;
         break;
      case 3:
         field = &config.bit_depth;
         break;
      case 4:
         field = &config.monochrome;
         break;
      case 5:
         field = &config.subsampling_x;
         break;
      case 6:
         field = &config.subsampling_y;
         break;
      default:
         field = &config.chroma_sample_position;
         break;
      }
      (*field)++;
      CHECK(!rav1_config_sequence(&sequence, &config));
      CHECK(!memcmp(&sequence, &zero, sizeof(sequence)));
      (*field)--;
   }
   CHECK(!rav1_sequence_parse(NULL, payload, size));
   CHECK(!rav1_sequence_parse(&sequence, NULL, size));
   CHECK(!rav1_config_sequence(&sequence, NULL));
}

static void test_bits(void)
{
   static const uint8_t maximum[] = {0, 0, 0, 1, 255, 255, 255, 254};
   rav1_bits_t bits;
   bits.data = maximum;
   bits.size = sizeof(maximum);
   bits.byte = 0;
   bits.bit = bits.failed = 0;
   CHECK(rav1_bits_uvlc(&bits) == UINT32_MAX - 1 && !bits.failed);
   CHECK(bits.byte == 7 && bits.bit == 7);
   CHECK(!rav1_bits_read(&bits, 1) && bits.byte == 8 && !bits.failed);
   CHECK(!rav1_bits_read(&bits, 1) && bits.failed);
   CHECK(!rav1_bits_read(&bits, 32) && bits.failed);
   bits.byte = 0;
   bits.bit = bits.failed = 0;
   CHECK(!rav1_bits_read(&bits, 33) && bits.failed);
}

int main(void)
{
   test_bits();
   test_layouts();
   test_models();
   test_invalid();
   test_config();
   printf("AV1 sequence headers: %u failures\n", failures);
   return failures ? 1 : 0;
}

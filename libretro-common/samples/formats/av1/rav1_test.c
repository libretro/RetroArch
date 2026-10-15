/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <formats/rav1.h>

static unsigned failures;
#define CHECK(x)                                                               \
   do                                                                          \
   {                                                                           \
      if (!(x))                                                                \
      {                                                                        \
         fprintf(stderr, "line %u: %s\n", (unsigned)__LINE__, #x);             \
         failures++;                                                           \
      }                                                                        \
   } while (0)

static void reject_obu(const uint8_t *data, size_t size, bool allow)
{
   rav1_obu_t out, zero;
   size_t offset = 0;
   memset(&zero, 0, sizeof(zero));
   memset(&out, 255, sizeof(out));
   CHECK(rav1_obu_next(&out, data, size, &offset, allow) == -1);
   CHECK(!offset && !memcmp(&out, &zero, sizeof(out)));
}

static void test_obus(void)
{
   static const uint8_t data[] = {0x12, 0, 0x36, 0xb0, 2, 0xaa, 0xbb};
   static const uint8_t final[] = {0x30, 0xaa, 0xbb};
   static const uint8_t bad_header[] = {0x83, 0};
   static const uint8_t bad_extension[] = {0x36, 1, 0};
   static const uint8_t long_leb[] = {0x32, 0x80, 0x80, 0x80, 0x80,
                                      0x80, 0x80, 0x80, 0x80, 0};
   static const uint8_t overflow[] = {0x32, 0x80, 0x80, 0x80, 0x80, 0x10};
   static const uint8_t padded_leb[] = {0x32, 0x81, 0x80, 0x80, 0x80,
                                        0x80, 0x80, 0x80, 0,    0x5a};
   rav1_obu_t out;
   size_t offset = 0, i;
   CHECK(rav1_obu_next(&out, data, sizeof(data), &offset, false) == 1);
   CHECK(out.data == data && out.payload == data + 2 && !out.payload_size &&
         out.type == 2 && offset == 2);
   CHECK(rav1_obu_next(&out, data, sizeof(data), &offset, false) == 1);
   CHECK(out.data == data + 2 && out.payload == data + 5 && out.size == 5 &&
         out.payload_size == 2 && out.type == 6 && out.temporal_id == 5 &&
         out.spatial_id == 2 && offset == sizeof(data));
   CHECK(rav1_obu_next(&out, data, sizeof(data), &offset, false) == 0 &&
         !out.data);
   offset = 0;
   CHECK(rav1_obu_next(&out, final, sizeof(final), &offset, true) == 1 &&
         !out.has_size && out.payload == final + 1 && out.payload_size == 2 &&
         offset == sizeof(final));
   reject_obu(final, sizeof(final), false);
   reject_obu(bad_header, sizeof(bad_header), false);
   reject_obu(bad_extension, sizeof(bad_extension), false);
   reject_obu(long_leb, sizeof(long_leb), false);
   reject_obu(overflow, sizeof(overflow), false);
   for (i = 1; i < sizeof(padded_leb); i++)
      reject_obu(padded_leb, i, false);
   offset = 0;
   CHECK(rav1_obu_next(&out, padded_leb, sizeof(padded_leb), &offset, false) ==
             1 &&
         out.payload_size == 1 && out.payload[0] == 0x5a);
   offset = sizeof(data) + 1;
   CHECK(rav1_obu_next(&out, data, sizeof(data), &offset, false) == -1 &&
         offset == sizeof(data) + 1);
   CHECK(rav1_obu_next(&out, data, sizeof(data), NULL, false) == -1);
   offset = 0;
   CHECK(rav1_obu_next(&out, NULL, 1, &offset, false) == -1);
   CHECK(rav1_obu_next(&out, NULL, 0, &offset, false) == 0);
   CHECK(rav1_obu_next(NULL, data, sizeof(data), &offset, false) == -1);
   /* Reserved types remain length-delimited, for a decoder to skip. */
   for (i = 0; i < 16; i++)
   {
      uint8_t reserved[2];
      reserved[0] = (uint8_t)((i << 3) | 2);
      reserved[1] = 0;
      offset = 0;
      CHECK(rav1_obu_next(&out, reserved, 2, &offset, false) == 1 &&
            out.type == i && offset == 2);
   }
}

static void reject_config(const uint8_t *data, size_t size)
{
   rav1_config_t out, zero;
   memset(&zero, 0, sizeof(zero));
   memset(&out, 255, sizeof(out));
   CHECK(!rav1_config_parse(&out, data, size));
   CHECK(!memcmp(&out, &zero, sizeof(out)));
}

static void test_config(void)
{
   static const uint8_t valid[][4] = {
       {0x81, 0, 0x0c, 0},    /* Main 8-bit 4:2:0 */
       {0x81, 0, 0x4c, 0x1f}, /* Main 10-bit, delay 16 */
       {0x81, 0x20, 0x00, 0}, /* High 8-bit 4:4:4 */
       {0x81, 0x20, 0x40, 0}, /* High 10-bit 4:4:4 */
       {0x81, 0x40, 0x08, 0}, /* Professional 8-bit 4:2:2 */
       {0x81, 0x40, 0x48, 0}, /* Professional 10-bit 4:2:2 */
       {0x81, 0x40, 0x60, 0}, /* Professional 12-bit 4:4:4 */
       {0x81, 0x40, 0x6e, 0}, /* Professional 12-bit 4:2:0 */
       {0x81, 0x40, 0x7c, 0}, /* Professional 12-bit monochrome */
       {0x81, 0x1f, 0xcc, 0}  /* Unconstrained level, high tier */
   };
   uint8_t config[] = {0x81, 0, 0x4c, 0, 0x0a, 1, 0xaa, 0x2a, 1, 0xbb};
   uint8_t bad[sizeof(config) + 3];
   rav1_config_t out;
   size_t i;
   for (i = 0; i < sizeof(valid) / sizeof(valid[0]); i++)
   {
      CHECK(rav1_config_parse(&out, valid[i], 4));
      CHECK(out.obus == valid[i] + 4 && !out.obus_size && !out.sequence.data);
   }
   CHECK(rav1_config_parse(&out, config, sizeof(config)));
   CHECK(out.profile == 0 && out.bit_depth == 10 && out.subsampling_x &&
         out.subsampling_y);
   CHECK(out.obus == config + 4 && out.obus_size == 6 &&
         out.sequence.data == config + 4 &&
         out.sequence.payload == config + 6 && out.sequence.payload_size == 1);
   config[6] = 0x55;
   CHECK(out.sequence.payload[0] == 0x55);
   for (i = 0; i < 4; i++)
      reject_config(config, i);
   reject_config(NULL, 4);
   CHECK(!rav1_config_parse(NULL, config, sizeof(config)));
#define BAD(byte, value)                                                       \
   do                                                                          \
   {                                                                           \
      memcpy(bad, config, sizeof(config));                                     \
      bad[byte] = value;                                                       \
      reject_config(bad, sizeof(config));                                      \
   } while (0)
   BAD(0, 1);
   BAD(0, 0x82);
   BAD(1, 0x60);
   BAD(1, 24);
   BAD(2, 0xcc);
   BAD(2, 0x2c);
   BAD(2, 0x6c);
   BAD(2, 0x40);
   BAD(2, 0x4f);
   BAD(2, 0x5d);
   BAD(3, 1);
   BAD(3, 0xe0);
   BAD(4, 0x08);
   BAD(5, 127);
#undef BAD
   memcpy(bad, config, sizeof(config));
   memcpy(bad + sizeof(config), config + 4, 3);
   reject_config(bad, sizeof(bad));
   /* A sequence header may occur only at the start of configOBUs. */
   memcpy(bad, config, sizeof(config));
   bad[4] = 0x2a;
   bad[7] = 0x0a;
   reject_config(bad, sizeof(config));
}

int main(void)
{
   test_obus();
   test_config();
   printf("AV1 configuration/OBU views: %u failures\n", failures);
   return failures ? 1 : 0;
}

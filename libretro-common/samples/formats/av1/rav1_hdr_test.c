/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <formats/rav1_hdr.h>
static unsigned failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %u: %s\n", (unsigned)__LINE__, #x); failures++; } } while (0)
static void put32(uint8_t *p, uint32_t value)
{
   p[0] = (uint8_t)(value >> 24); p[1] = (uint8_t)(value >> 16);
   p[2] = (uint8_t)(value >> 8); p[3] = (uint8_t)value;
}
int main(void)
{
   uint8_t cll[] = {1, 3, 232, 1, 144, 128, 0, 0};
   uint8_t mastering[28] = {2};
   uint8_t unknown[] = {4, 128};
   uint8_t extended[] = {46, 0, 6, 1, 3, 232, 1, 144, 128};
   uint8_t wide_type[] = {129, 0, 3, 232, 1, 144, 128};
   uint8_t zero_cll[] = {1, 0, 0, 0, 0, 128};
   uint8_t unterminated[8];
   uint8_t config_bytes[40];
   rav1_hdr_metadata_t metadata, before;
   video_color_metadata_t color, saved;
   rav1_config_t config;
   unsigned i;
   memset(&metadata, 0, sizeof(metadata));
   memset(&color, 0, sizeof(color));
   memset(&config, 0, sizeof(config));
   memset(unterminated, 128, sizeof(unterminated));
   CHECK(rav1_hdr_metadata_parse(&metadata, cll, sizeof(cll)) == 1);
   CHECK(metadata.max_cll == 1000 && metadata.max_fall == 400 && metadata.present == 1);
   mastering[1] = 128; mastering[7] = 128; /* R x=0.5, G y=0.5 */
   put32(mastering + 17, 256000); /* 1000 cd/m2 */
   put32(mastering + 21, 8192);   /* 0.5 cd/m2 */
   mastering[25] = 128;
   CHECK(rav1_hdr_metadata_parse(&metadata, mastering, sizeof(mastering)) == 1);
   CHECK(metadata.present == 3 && metadata.max_cll == 1000);
   CHECK(metadata.mastering.present == VIDEO_MASTERING_DISPLAY_COMPLETE);
   CHECK(metadata.mastering.chromaticity[0] == 25000 && metadata.mastering.chromaticity[1] == 0);
   CHECK(metadata.mastering.chromaticity[3] == 25000);
   CHECK(metadata.mastering.luminance_max == 10000000 && metadata.mastering.luminance_min == 5000);
   color.primaries = 9; color.transfer = 16; color.matrix = 9; color.range = 2;
   rav1_hdr_metadata_apply(&color, &metadata);
   CHECK(color.transfer == 16 && color.range == 2 && color.primaries == 9 && color.matrix == 9);
   CHECK(color.max_cll == 1000 && color.max_fall == 400 && color.mastering.luminance_max == 10000000);
   before = metadata;
   saved = color;
   CHECK(rav1_hdr_metadata_merge(&color, &metadata));
   color.max_cll++; saved = color;
   CHECK(!rav1_hdr_metadata_merge(&color, &metadata));
   CHECK(!memcmp(&color, &saved, sizeof(color)));
   color.max_cll--; color.mastering.luminance_max += 40;
   CHECK(rav1_hdr_metadata_merge(&color, &metadata));
   color.mastering.luminance_max += 41; saved = color;
   CHECK(!rav1_hdr_metadata_merge(&color, &metadata));
   CHECK(!memcmp(&color, &saved, sizeof(color)));
   rav1_hdr_metadata_apply(&color, &metadata);
   color.mastering.chromaticity[0]++;
   CHECK(rav1_hdr_metadata_merge(&color, &metadata));
   color.mastering.chromaticity[0] += 2;
   CHECK(!rav1_hdr_metadata_merge(&color, &metadata));
   rav1_hdr_metadata_apply(&color, &metadata);

   for (i = 0; i < 6; i++)
   {
      CHECK(rav1_hdr_metadata_parse(&metadata, cll, i) == -1);
      CHECK(!memcmp(&metadata, &before, sizeof(metadata)));
   }
   for (i = 0; i < 26; i++)
   {
      CHECK(rav1_hdr_metadata_parse(&metadata, mastering, i) == -1);
      CHECK(!memcmp(&metadata, &before, sizeof(metadata)));
   }
   cll[5] = 0; CHECK(rav1_hdr_metadata_parse(&metadata, cll, sizeof(cll)) == -1);
   cll[5] = 128; cll[7] = 1; CHECK(rav1_hdr_metadata_parse(&metadata, cll, sizeof(cll)) == -1);
   cll[7] = 0;
   CHECK(rav1_hdr_metadata_parse(&metadata, unknown, sizeof(unknown)) == 0);
   CHECK(rav1_hdr_metadata_parse(&metadata, unterminated, sizeof(unterminated)) == -1);
   CHECK(!memcmp(&metadata, &before, sizeof(metadata)));
   memset(config_bytes, 0, sizeof(config_bytes));
   config_bytes[0] = 42; config_bytes[1] = 6; memcpy(config_bytes + 2, cll, 6);
   config_bytes[8] = 42; config_bytes[9] = 26; memcpy(config_bytes + 10, mastering, 26);
   config.obus = config_bytes; config.obus_size = 36;
   memset(&metadata, 0, sizeof(metadata));
   CHECK(rav1_hdr_metadata_config(&metadata, &config));
   CHECK(!memcmp(&metadata, &before, sizeof(metadata)));
   config_bytes[35] = 0;
   CHECK(!rav1_hdr_metadata_config(&metadata, &config));
   CHECK(!memcmp(&metadata, &before, sizeof(metadata)));
   config.obus = extended; config.obus_size = sizeof(extended);
   CHECK(!rav1_hdr_metadata_config(&metadata, &config));
   CHECK(!memcmp(&metadata, &before, sizeof(metadata)));
   CHECK(rav1_hdr_metadata_parse(&metadata, wide_type, sizeof(wide_type)) == 1);
   CHECK(metadata.max_cll == 1000 && metadata.max_fall == 400);
   put32(mastering + 17, UINT32_MAX);
   CHECK(rav1_hdr_metadata_parse(&metadata, mastering, sizeof(mastering)) == 1);
   CHECK(metadata.mastering.present == 0x2ffu && !metadata.mastering.luminance_max);
   put32(mastering + 17, 1); put32(mastering + 21, 1);
   CHECK(rav1_hdr_metadata_parse(&metadata, mastering, sizeof(mastering)) == 1);
   CHECK(metadata.mastering.luminance_max == 39 && metadata.mastering.luminance_min == 1);
   CHECK(rav1_hdr_metadata_parse(&metadata, zero_cll, sizeof(zero_cll)) == 1);
   rav1_hdr_metadata_apply(&color, &metadata);
   CHECK(!color.max_cll && !color.max_fall && color.transfer == 16);
   printf("AV1 HDR metadata: %u failures\n", failures);
   return failures ? 1 : 0;
}

/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#ifdef NDEBUG
#error Assertions are required for this test
#endif
#include "../../../gfx/common/videotoolbox_av1_config.h"
#include "../vulkan_video_caps/av1_session_data.h"

static rav1_sequence_t sequence, decoded;
static videotoolbox_av1_config_t config, empty;

int main(void)
{
   uint8_t obu_data[128], record[132];
   rav1_obu_t obu;
   rav1_config_t parsed;
   video_color_metadata_t color, resolved;
   size_t i, offset;
   unsigned bit, target;
   for (i = 0;
         i < sizeof(av1_sequence_fixtures) / sizeof(av1_sequence_fixtures[0]);
         i++)
   {
      const struct av1_sequence_fixture *fixture = &av1_sequence_fixtures[i];
      assert(fixture->size < 127);
      obu_data[0] = 0x0a;
      obu_data[1] = (uint8_t)fixture->size;
      memcpy(obu_data + 2, fixture->data, fixture->size);
      offset = 0;
      assert(rav1_obu_next(&obu, obu_data, fixture->size + 2, &offset, false)
            == 1);
      assert(rav1_sequence_parse(&sequence, obu.payload, obu.payload_size));
      memset(&color, 0, sizeof(color));
      color.primaries = color.transfer = color.matrix = color.range = 1;
      color.max_cll = 1000;
      assert(videotoolbox_av1_configure(&sequence, &obu, &color, &config));
      assert(config.sequence_obu == obu_data
            && config.sequence_size == obu.size);
      assert(config.color.max_cll == 1000 && !config.av1c[3]);
      assert(config.pixfmt
            == (fixture->depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12));
      memcpy(record, config.av1c, 4);
      memcpy(record + 4, config.sequence_obu, config.sequence_size);
      assert(rav1_config_parse(&parsed, record, config.sequence_size + 4));
      assert(rav1_config_sequence(&decoded, &parsed));
      assert(!memcmp(&decoded, &sequence, sizeof(sequence)));
      assert(videotoolbox_av1_configure(
            &sequence, &obu, &config.color, &config));
      assert(config.color.max_cll == 1000);
      sequence.max_width++;
      assert(!videotoolbox_av1_configure(&sequence, &obu, &color, &config));
      assert(!memcmp(&config, &empty, sizeof(config)));
      sequence.max_width--;
      color.range = 2;
      assert(!videotoolbox_av1_configure(&sequence, &obu, &color, &config));
      assert(!memcmp(&config, &empty, sizeof(config)));
      obu_data[0] = 0x08;
      assert(!videotoolbox_av1_configure(&sequence, &obu, NULL, &config));
   }
   memset(obu_data, 0, sizeof(obu_data));
   obu_data[0] = 0x0a;
   obu_data[1] = 11;
   for (bit = 0; bit <= 80; bit++)
   {
      target = bit + (bit >= 29 ? 5 : 0);
      if (bit == 6 || ((av1_sequence_8_normal[bit / 8] >> (7 - bit % 8)) & 1))
         obu_data[2 + target / 8] |= (uint8_t)(1 << (7 - target % 8));
   }
   for (bit = 0; bit < 5; bit++)
      if ((0x12 >> (4 - bit)) & 1)
         obu_data[2 + (29 + bit) / 8] |= (uint8_t)(1 << (7 - (29 + bit) % 8));
   offset = 0;
   assert(rav1_obu_next(&obu, obu_data, 13, &offset, false) == 1);
   assert(rav1_sequence_parse(&sequence, obu.payload, obu.payload_size));
   assert(sequence.operating_points[0].initial_display_delay == 3);
   assert(videotoolbox_av1_configure(&sequence, &obu, NULL, &config));
   assert(!config.av1c[3]);
   memset(&sequence, 0, sizeof(sequence));
   sequence.color_primaries = 9;
   sequence.transfer_characteristics = 16;
   sequence.matrix_coefficients = 9;
   memset(&color, 0, sizeof(color));
   assert(videotoolbox_av1_color(&sequence, &color, &resolved));
   assert(resolved.primaries == 9 && resolved.transfer == 16);
   color.transfer = 18;
   memset(&resolved, 0xa5, sizeof(resolved));
   color.max_cll = resolved.max_cll;
   assert(!videotoolbox_av1_color(&sequence, &color, &resolved));
   assert(resolved.max_cll == color.max_cll);
   puts("PASS: AV1 cold configuration, color agreement and av1C roundtrip");
   return 0;
}

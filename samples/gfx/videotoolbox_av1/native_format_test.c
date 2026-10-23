/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#ifdef NDEBUG
#error Assertions are required for this test
#endif
#include "../../../gfx/common/videotoolbox_av1_decoder.h"
#include "../vulkan_video_caps/av1_session_data.h"

static rav1_sequence_t sequence;

int main(void)
{
   uint8_t bytes[128];
   rav1_obu_t obu;
   video_color_metadata_t color;
   CMVideoFormatDescriptionRef format;
   CFDictionaryRef atoms;
   CFDataRef data;
   size_t i, offset;
   for (i = 0; i < sizeof(av1_sequence_fixtures)
         / sizeof(av1_sequence_fixtures[0]); i++)
   {
      const struct av1_sequence_fixture *fixture = &av1_sequence_fixtures[i];
      bytes[0] = 0x0a;
      bytes[1] = (uint8_t)fixture->size;
      memcpy(bytes + 2, fixture->data, fixture->size);
      offset = 0;
      assert(rav1_obu_next(&obu, bytes, fixture->size + 2, &offset, false) == 1);
      assert(rav1_sequence_parse(&sequence, obu.payload, obu.payload_size));
      memset(&color, 0, sizeof(color));
      color.primaries = color.transfer = color.matrix = color.range = 1;
      assert(!videotoolbox_av1_format_create(&sequence, &color, &obu, &format));
      assert(format && CMFormatDescriptionGetMediaSubType(format)
            == VIDEOTOOLBOX_AV1_CODEC);
      atoms = (CFDictionaryRef)CMFormatDescriptionGetExtension(format,
            kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms);
      assert(atoms);
      data = (CFDataRef)CFDictionaryGetValue(atoms, CFSTR("av1C"));
      assert(data && CFDataGetLength(data) == (CFIndex)(4 + obu.size));
      assert(CFDataGetBytePtr(data)[0] == 0x81
            && !CFDataGetBytePtr(data)[3]);
      assert(!memcmp(CFDataGetBytePtr(data) + 4, obu.data, obu.size));
      CFRelease(format);
   }
   puts("PASS: native AV1 format atoms; decoder hardware remains untested");
   return 0;
}

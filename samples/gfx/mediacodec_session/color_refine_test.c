/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../gfx/common/android_frame.h"
int main(void)
{
   android_frame_view_t view, unchanged;
   video_color_metadata_t color;
   unsigned i;
   memset(&view, 0, sizeof(view));
   memset(&color, 0, sizeof(color));
   view.color.primaries = view.color.matrix = 9;
   view.color.transfer = 16;
   view.color.range = 2;
   view.color.max_cll = 1000;
   view.color.mastering.present = 0x201u;
   view.color.mastering.chromaticity[0] = 15000;
   view.color.mastering.luminance_min = 10;
   unchanged = view;
   color.primaries = color.transfer = color.matrix = 2;
   assert(android_frame_resolve_color(&view, &color));
   assert(!memcmp(&view, &unchanged, sizeof(view)));
   color.max_cll = 4000;
   color.max_fall = 600;
   color.mastering.present = 0x102u;
   color.mastering.chromaticity[1] = 30000;
   color.mastering.luminance_max = 20000000;
   assert(android_frame_resolve_color(&view, &color));
   assert(view.color.max_cll == 4000 && view.color.max_fall == 600);
   assert(view.color.mastering.present == 0x303u
         && view.color.mastering.chromaticity[0] == 15000);
   assert(view.color.mastering.chromaticity[1] == 30000
         && view.color.mastering.luminance_min == 10);
   assert(view.color.mastering.luminance_max == 20000000);
   unchanged = view;
   for (i = 0; i < 4; i++)
   {
      memset(&color, 0, sizeof(color));
      if (i == 0)
         color.primaries = 1;
      if (i == 1)
         color.transfer = 18;
      if (i == 2)
         color.matrix = 1;
      if (i == 3)
         color.range = 1;
      assert(!android_frame_resolve_color(&view, &color));
      assert(!memcmp(&view, &unchanged, sizeof(view)));
   }
   memset(&color, 0, sizeof(color));
   color.range = 2;
   assert(android_frame_resolve_color(&view, &color));
   assert(!android_frame_resolve_color(NULL, &color));
   assert(!android_frame_resolve_color(&view, NULL));
   puts("PASS: container HDR refinement, partial mastering inheritance and "
        "explicit CICP/range conflict refusal");
   return 0;
}

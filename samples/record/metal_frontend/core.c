/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define retro_run metal_fixture_run
#include "../nvenc_frontend/core.c"
#undef retro_run
void retro_run(void)
{
   if (frames==90 && getenv("NATIVE_RECORD_VIEWPORT_CHANGE"))
   {
      struct retro_game_geometry geometry;
      memset(&geometry,0,sizeof(geometry));
      geometry.base_width=256; geometry.base_height=HEIGHT;
      geometry.max_width=WIDTH; geometry.max_height=HEIGHT;
      geometry.aspect_ratio=4.0f/3.0f;
      if (!environment(RETRO_ENVIRONMENT_SET_GEOMETRY,&geometry)) abort();
      fprintf(stderr,"[fixture] viewport geometry changed at frame 90\n");
   }
   metal_fixture_run();
}

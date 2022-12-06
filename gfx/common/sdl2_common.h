/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2019 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SDL2_COMMON_H__
#define SDL2_COMMON_H__

#include <stdint.h>
#include <boolean.h>

#include "SDL.h"

#include "../video_defines.h"
#include "../font_driver.h"
#include "../../retroarch.h"

typedef struct sdl2_tex
{
   SDL_Texture *tex;

   unsigned w;
   unsigned h;
   size_t pitch;
   bool active;
   bool rgb32;
} sdl2_tex_t;

typedef struct _sdl2_video
{
   double rotation;

   struct video_viewport vp;
   video_info_t video;

   sdl2_tex_t frame; /* ptr alignment */
   sdl2_tex_t menu;  /* ptr alignment */
   sdl2_tex_t font;  /* ptr alignment */

   SDL_Window *window;
   SDL_Renderer *renderer;

   void *font_data;
   const font_renderer_driver_t *font_driver;

   uint8_t font_r;
   uint8_t font_g;
   uint8_t font_b;

   bool gl;
   bool quitting;
   bool should_resize;
} sdl2_video_t;

void sdl2_set_handles(void *data, enum rarch_display_type 
      display_type);

#endif

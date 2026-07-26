/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2019 - Daniel De Matteis
 *  Copyright (C)      2026 - Rob Loach
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

#ifndef SDL3_COMMON_H__
#define SDL3_COMMON_H__

#include <stdint.h>
#include <boolean.h>

#include <SDL3/SDL.h>

#include "../video_defines.h"
#include "../../retroarch.h"

enum sdl3_flags
{
   SDL3_FLAG_QUITTING       = (1 << 0),
   SDL3_FLAG_SHOULD_RESIZE  = (1 << 1),
   SDL3_FLAG_ADAPTIVE_VSYNC = (1 << 2)
};

typedef struct sdl3_tex
{
   SDL_Texture *tex;

   unsigned w;
   unsigned h;
   bool active;
   bool rgb32;
} sdl3_tex_t;

typedef struct _sdl3_video
{
   double rotation;

   struct video_viewport vp;
   video_info_t video;

   sdl3_tex_t frame; /* ptr alignment */
   sdl3_tex_t menu;  /* ptr alignment */

   SDL_Window *window;
   SDL_Renderer *renderer;

   uint8_t flags;
} sdl3_video_t;

void sdl3_set_handles(SDL_Window *window);

#endif

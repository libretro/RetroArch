/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

// Compatibility wrapper between SDL 1.2/1.3 for OpenGL.
// Wraps functions which differ in 1.2 and 1.3.

#ifndef __SDLWRAP_H
#define __SDLWRAP_H

#include <stdbool.h>

#include "SDL.h"
#include "SDL_version.h"
#include "SDL_syswm.h"

#if SDL_VERSION_ATLEAST(1, 3, 0)
#define SDL_MODERN 1
#else
#define SDL_MODERN 0
#endif

// Not legal to cast void* to fn-pointer. Need workaround to be compliant.
#define SDL_SYM_WRAP(sym, symbol) { \
   assert(sizeof(void*) == sizeof(void (*)(void))); \
   void *sym__ = SDL_GL_GetProcAddress(symbol); \
   memcpy(&(sym), &sym__, sizeof(void*)); \
}

void sdlwrap_set_swap_interval(unsigned interval, bool inited);

bool sdlwrap_set_video_mode(
      unsigned width, unsigned height,
      unsigned bits, bool fullscreen);

bool sdlwrap_init(void);
void sdlwrap_destroy(void);

void sdlwrap_wm_set_caption(const char *str);

void sdlwrap_swap_buffers(void);

bool sdlwrap_key_pressed(int key);

void sdlwrap_check_window(bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count);

void sdlwrap_set_resize(unsigned width, unsigned height);

bool sdlwrap_get_wm_info(SDL_SysWMinfo *info);

bool sdlwrap_window_has_focus(void);

#endif


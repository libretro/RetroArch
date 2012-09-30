/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#include "../../driver.h"
#include "../gfx_context.h"
#include "../gl_common.h"

#include <stdint.h>

int gfx_ctx_check_resolution(unsigned resolution_id)
{
   (void)resolution_id;
   return 0;
}

unsigned gfx_ctx_get_resolution_width(unsigned resolution_id)
{
   (void)resolution_id;
   return 0;
}

unsigned gfx_ctx_get_resolution_height(unsigned resolution_id)
{
   (void)resolution_id;
   return 0;
}

float gfx_ctx_get_aspect_ratio(void)
{
   return 4.0f / 3.0f;
}

void gfx_ctx_get_available_resolutions(void)
{}

void gfx_ctx_set_swap_interval(unsigned interval, bool inited)
{
   (void)inited;
   (void)interval;
}

void gfx_ctx_check_window(bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   (void)quit;
   (void)resize;
   (void)width;
   (void)height;
   (void)frame_count;
}

void gfx_ctx_swap_buffers(void)
{}

void gfx_ctx_clear(void)
{}

void gfx_ctx_set_blend(bool enable)
{}

void gfx_ctx_set_resize(unsigned width, unsigned height)
{
   (void)width;
   (void)height;
}

bool gfx_ctx_menu_init(void)
{}

void gfx_ctx_update_window_title(bool reset)
{
   (void)reset;
}

void gfx_ctx_get_video_size(unsigned *width, unsigned *height)
{
   (void)width;
   (void)height;
}

bool gfx_ctx_init(void)
{
   return true;
}

bool gfx_ctx_set_video_mode(
      unsigned width, unsigned height,
      unsigned bits, bool fullscreen)
{
   (void)width;
   (void)height;
   (void)bits;
   (void)fullscreen;
   return true;
}

void gfx_ctx_destroy(void)
{}

void gfx_ctx_input_driver(const input_driver_t **input, void **input_data)
{
   *input = NULL;
   *input_data = NULL;
}

void gfx_ctx_set_filtering(unsigned index, bool set_smooth)
{
   (void)index;
   (void)set_smooth;
}

void gfx_ctx_set_fbo(bool enable)
{
   (void)enable;
}

void gfx_ctx_apply_fbo_state_changes(unsigned mode)
{
   (void)mode;
}

void gfx_ctx_set_aspect_ratio(void *data, unsigned aspectratio_index)
{
   (void)data;
   (void)aspectratio_index;
}

void gfx_ctx_set_overscan(void)
{}

// Enforce void (*)(void) as it's not really legal to cast void* to fn-pointer.
// POSIX allows this, but strict C99 doesn't.
#ifndef __PSL1GHT__
gfx_ctx_proc_t gfx_ctx_get_proc_address(const char *symbol)
{
   rarch_assert(sizeof(void*) == sizeof(void (*)(void)));
   gfx_ctx_proc_t ret;

   void *sym__ = eglGetProcAddress(symbol);
   memcpy(&ret, &sym__, sizeof(void*));

   return ret;
}
#endif

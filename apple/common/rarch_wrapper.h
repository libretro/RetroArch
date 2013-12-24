/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#ifndef __APPLE_RARCH_WRAPPER_H__
#define __APPLE_RARCH_WRAPPER_H__

#include "gfx/gfx_context.h"

// These functions should only be called as arguments to dispatch_sync
void apple_rarch_exited (void* result);

// These functions must only be called in gfx/context/apple_gl_context.c
bool apple_gfx_ctx_init(void);
void apple_gfx_ctx_destroy(void);
bool apple_gfx_ctx_bind_api(enum gfx_ctx_api api, unsigned major, unsigned minor);
void apple_gfx_ctx_swap_interval(unsigned interval);
bool apple_gfx_ctx_set_video_mode(unsigned width, unsigned height, bool fullscreen);
void apple_gfx_ctx_get_video_size(unsigned* width, unsigned* height);
void apple_gfx_ctx_update_window_title(void);
bool apple_gfx_ctx_has_focus(void);
void apple_gfx_ctx_swap_buffers(void);
gfx_ctx_proc_t apple_gfx_ctx_get_proc_address(const char *symbol_name);

#ifdef IOS
void apple_bind_game_view_fbo(void);
#endif

#endif

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

// The result needs to be free()'d
char* ios_get_rarch_system_directory();

// These functions should only be called as arguments to dispatch_sync
void apple_rarch_exited (void* result);

// These functions must only be called in gfx/context/ioseagl_ctx.c
bool apple_init_game_view(void);
void apple_destroy_game_view(void);
bool apple_set_video_mode(unsigned width, unsigned height, bool fullscreen);
void apple_flip_game_view(void);
void apple_set_game_view_sync(unsigned interval);
void apple_get_game_view_size(unsigned *width, unsigned *height);
void *apple_get_proc_address(const char *symbol_name);

#ifdef IOS
void apple_bind_game_view_fbo(void);
#endif

void ios_add_log_message(const char* format, ...);

#endif

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
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

#ifndef __IOS_RARCH_WRAPPER_H__
#define __IOS_RARCH_WRAPPER_H__

// The result needs to be free()'d
char* ios_get_rarch_system_directory();

// These functions should only be called as arguments to dispatch_sync
void ios_rarch_exited(void* result);

// These functions must only be called in gfx/context/ioseagl_ctx.c
bool ios_init_game_view();
void ios_destroy_game_view();
void ios_flip_game_view();
void ios_set_game_view_sync(unsigned interval);
void ios_get_game_view_size(unsigned *width, unsigned *height);
void ios_bind_game_view_fbo();

// Thread safe
void ios_add_log_message(const char* format, ...);
#endif

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

bool ios_load_game(const char* path);
void ios_close_game();
void ios_pause_emulator();
void ios_resume_emulator();
void ios_suspend_emulator();
void ios_activate_emulator();
bool ios_init_game_view();
void ios_destroy_game_view();
void ios_flip_game_view();
void ios_set_game_view_sync(bool on);
void ios_get_game_view_size(unsigned *width, unsigned *height);
void ios_bind_game_view_fbo();

#endif

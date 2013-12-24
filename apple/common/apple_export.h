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

#ifndef __APPLE_EXPORT_H
#define __APPLE_EXPORT_H

#include <stdint.h>

enum basic_event_t {
   RESET = 1,
   LOAD_STATE = 2,
   SAVE_STATE = 3,
   QUIT = 4
};
extern void apple_event_basic_command(enum basic_event_t action);

extern void apple_refresh_config(void);
extern int apple_rarch_load_content(int argc, char* argv[]);
extern int apple_rarch_iterate_once();

#endif

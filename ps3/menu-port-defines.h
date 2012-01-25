/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#define FONT_SIZE 1.0f
#define EMU_MENU_TITLE "SSNES |"
#define VIDEO_MENU_TITLE "SSNES VIDEO |"
#define AUDIO_MENU_TITLE "SSNES AUDIO |"

#define EMULATOR_NAME "SSNES"
#define EMULATOR_VERSION PACKAGE_VERSION

#define cell_console_poll()
#define ps3graphics_draw_menu()
#define Emulator_GetFontSize() FONT_SIZE

#define EXTRA_SELECT_FILE_PART1()
#define EXTRA_SELECT_FILE_PART2()

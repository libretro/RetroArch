/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

#ifndef _CONSOLE_FONT_H
#define _CONSOLE_FONT_H

#define FONT_WIDTH 5
#define FONT_HEIGHT 10

#define FONT_OFFSET(x) ((x) * ((FONT_HEIGHT * FONT_WIDTH + 7) / 8))

//extern uint8_t _binary_console_font_bmp_start[];
extern uint8_t _binary_console_font_bin_start[];

#endif

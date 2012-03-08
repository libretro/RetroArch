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

#define MAX_PATH_LENGTH 1024

enum
{
	DPAD_EMULATION_NONE,
	DPAD_EMULATION_LSTICK,
	DPAD_EMULATION_RSTICK
};

enum
{
	MODE_EMULATION,
	MODE_MENU,
	MODE_EXIT
};

enum
{
	ORIENTATION_NORMAL,
	ORIENTATION_VERTICAL,
	ORIENTATION_FLIPPED,
	ORIENTATION_FLIPPED_ROTATED,
	ORIENTATION_END
};

enum {
   MENU_ITEM_LOAD_STATE = 0,
   MENU_ITEM_SAVE_STATE,
   MENU_ITEM_HARDWARE_FILTERING,
   MENU_ITEM_KEEP_ASPECT_RATIO,
   MENU_ITEM_OVERSCAN_AMOUNT,
   MENU_ITEM_ORIENTATION,
   MENU_ITEM_RESIZE_MODE,
   MENU_ITEM_FRAME_ADVANCE,
   MENU_ITEM_SCREENSHOT_MODE,
   MENU_ITEM_RESET,
   MENU_ITEM_RETURN_TO_GAME,
   MENU_ITEM_RETURN_TO_DASHBOARD
};

#define MENU_ITEM_LAST MENU_ITEM_RETURN_TO_DASHBOARD+1

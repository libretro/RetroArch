/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 *

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

#ifndef _PS3_SHARED_H
#define _PS3_SHARED_H

#define MAX_PATH_LENGTH 1024

/* ABGR color format */

#define WHITE		0xffffffffu
#define RED		0xff0000ffu
#define GREEN		0xff00ff00u
#define BLUE		0xffff0000u
#define YELLOW		0xff00ffffu
#define PURPLE		0xffff00ffu
#define CYAN		0xffffff00u
#define ORANGE		0xff0063ffu
#define SILVER		0xff8c848cu
#define LIGHTBLUE	0xFFFFE0E0U
#define LIGHTORANGE	0xFFE0EEFFu

enum
{
	DPAD_EMULATION_NONE,
	DPAD_EMULATION_LSTICK,
	DPAD_EMULATION_RSTICK
};

enum
{
	EXTERN_LAUNCHER_SALAMANDER,
	EXTERN_LAUNCHER_MULTIMAN
};

enum
{
	MODE_EMULATION,
	MODE_MENU,
	MODE_EXIT
};

enum {
	ORIENTATION_NORMAL,
	ORIENTATION_VERTICAL,
	ORIENTATION_FLIPPED,
	ORIENTATION_FLIPPED_ROTATED,
	ORIENTATION_END
};

enum {
	CONFIG_FILE,
	SHADER_PRESET_FILE,
	INPUT_PRESET_FILE
};

enum {
	SOUND_MODE_NORMAL,
	SOUND_MODE_RSOUND,
	SOUND_MODE_HEADSET
};

enum {
   MENU_ITEM_LOAD_STATE = 0,
   MENU_ITEM_SAVE_STATE,
   MENU_ITEM_KEEP_ASPECT_RATIO,
   MENU_ITEM_OVERSCAN_AMOUNT,
   MENU_ITEM_ORIENTATION,
   MENU_ITEM_SCALE_FACTOR,
   MENU_ITEM_RESIZE_MODE,
   MENU_ITEM_FRAME_ADVANCE,
   MENU_ITEM_SCREENSHOT_MODE,
   MENU_ITEM_RESET,
   MENU_ITEM_RETURN_TO_GAME,
   MENU_ITEM_RETURN_TO_MENU,
   MENU_ITEM_CHANGE_LIBSNES,
   MENU_ITEM_RETURN_TO_MULTIMAN,
   MENU_ITEM_RETURN_TO_XMB
};

#define MENU_ITEM_LAST           MENU_ITEM_RETURN_TO_XMB+1

extern unsigned g_frame_count;
extern bool g_quitting;

extern char contentInfoPath[MAX_PATH_LENGTH];
extern char usrDirPath[MAX_PATH_LENGTH];
extern char DEFAULT_PRESET_FILE[MAX_PATH_LENGTH];
extern char DEFAULT_BORDER_FILE[MAX_PATH_LENGTH];
extern char DEFAULT_MENU_BORDER_FILE[MAX_PATH_LENGTH];
extern char PRESETS_DIR_PATH[MAX_PATH_LENGTH];
extern char INPUT_PRESETS_DIR_PATH[MAX_PATH_LENGTH];
extern char BORDERS_DIR_PATH[MAX_PATH_LENGTH];
extern char SHADERS_DIR_PATH[MAX_PATH_LENGTH];
extern char DEFAULT_SHADER_FILE[MAX_PATH_LENGTH];
extern char DEFAULT_MENU_SHADER_FILE[MAX_PATH_LENGTH];
extern char LIBSNES_DIR_PATH[MAX_PATH_LENGTH];
extern char SYS_CONFIG_FILE[MAX_PATH_LENGTH];
extern char MULTIMAN_EXECUTABLE[MAX_PATH_LENGTH];

#endif

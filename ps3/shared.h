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
	MODE_EMULATION,
	MODE_MENU,
#ifdef MULTIMAN_SUPPORT
	MODE_MULTIMAN_STARTUP,
#endif
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
   MENU_ITEM_LOAD_STATE = 0,
   MENU_ITEM_SAVE_STATE,
   MENU_ITEM_KEEP_ASPECT_RATIO,
   MENU_ITEM_OVERSCAN_AMOUNT,
   MENU_ITEM_ORIENTATION,
   MENU_ITEM_RESIZE_MODE,
   MENU_ITEM_FRAME_ADVANCE,
   MENU_ITEM_SCREENSHOT_MODE,
   MENU_ITEM_RESET,
   MENU_ITEM_RETURN_TO_GAME,
   MENU_ITEM_RETURN_TO_MENU,
#ifdef MULTIMAN_SUPPORT
   MENU_ITEM_RETURN_TO_MULTIMAN,
#endif
   MENU_ITEM_RETURN_TO_XMB
};

#define MENU_ITEM_LAST           MENU_ITEM_RETURN_TO_XMB+1

extern char special_action_msg[256];
extern uint32_t special_action_msg_expired;
extern unsigned g_frame_count;
extern bool g_quitting;

extern char contentInfoPath[MAX_PATH_LENGTH];
extern char usrDirPath[MAX_PATH_LENGTH];
extern char DEFAULT_PRESET_FILE[MAX_PATH_LENGTH];
extern char DEFAULT_BORDER_FILE[MAX_PATH_LENGTH];
extern char DEFAULT_MENU_BORDER_FILE[MAX_PATH_LENGTH];
extern char GAME_AWARE_SHADER_DIR_PATH[MAX_PATH_LENGTH];
extern char PRESETS_DIR_PATH[MAX_PATH_LENGTH];
extern char INPUT_PRESETS_DIR_PATH[MAX_PATH_LENGTH];
extern char BORDERS_DIR_PATH[MAX_PATH_LENGTH];
extern char SHADERS_DIR_PATH[MAX_PATH_LENGTH];
extern char DEFAULT_SHADER_FILE[MAX_PATH_LENGTH];
extern char DEFAULT_MENU_SHADER_FILE[MAX_PATH_LENGTH];
extern char SYS_CONFIG_FILE[MAX_PATH_LENGTH];
extern char MULTIMAN_GAME_TO_BOOT[MAX_PATH_LENGTH];

extern void set_text_message(const char * message, uint32_t speed);

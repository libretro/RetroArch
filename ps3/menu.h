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

#ifndef MENU_H_
#define MENU_H_

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

#define FONT_SIZE 1.0f

typedef struct
{
	uint32_t enum_id;			/* enum ID of item				*/
	char text[256];				/* item label					*/
	float text_xpos;			/* text X position (upper left corner)		*/
	float text_ypos;			/* text Y position (upper left corner)		*/
	uint32_t text_selected_color;		/* text color if selected			*/
	uint32_t text_unselected_color;		/* text color if not selected			*/
	char comment[256];			/* item comment					*/
	uint32_t comment_color;			/* color of item comment			*/
	float comment_scalefont;		/* font scale of item comment			*/ 
	float comment_xpos;			/* comment X position (upper left corner)	*/
	float comment_ypos;			/* comment Y position (upper left corner)	*/
	unsigned int * setting_ptr;		/* associated pointer to setting member		*/
	char comment_yes[256];			/* item comment (yes - if setting_ptr true)	*/
	char comment_no[256];			/* item comment (no - if setting_ptr false)	*/
	uint32_t default_value;			/* default value of item			*/
	uint32_t enabled;			/* is the item enabled?				*/
	uint32_t page;				/* page						*/
} item;

typedef struct
{
	char title[64];			/* menu title					*/
	uint32_t enum_id;		/* enum ID of menu				*/
	uint32_t selected;		/* index of selected item			*/
	uint32_t page;			/* page						*/
	uint32_t refreshpage;		/* bit whether or not to refresh page		*/
	uint32_t first_setting;		/* first setting				*/
	uint32_t max_settings;		/* max no of settings in menu			*/
	item *items;			/* menu items					*/
} menu;


#define FILE_BROWSER_MENU		0
#define GENERAL_VIDEO_MENU		1
#define GENERAL_AUDIO_MENU		2
#define EMU_GENERAL_MENU		3
#define EMU_VIDEO_MENU			4
#define EMU_AUDIO_MENU			5
#define PATH_MENU			6
#define CONTROLS_MENU			7
#define SHADER_CHOICE			8
#define PRESET_CHOICE			9
#define BORDER_CHOICE			10
#define PATH_CHOICE			11
#define GAME_AWARE_SHADER_CHOICE	12
#define PATH_SAVESTATES_DIR_CHOICE	13
#define PATH_DEFAULT_ROM_DIR_CHOICE	14
#define PATH_CHEATS_DIR_CHOICE		15
#define PATH_SRAM_DIR_CHOICE		16
#define PATH_BASE_DIR_CHOICE		17
#define INPUT_PRESET_CHOICE		18

enum
{
	SETTING_CHANGE_RESOLUTION,
	SETTING_SHADER_PRESETS,
	SETTING_BORDER,
	SETTING_SHADER,
	SETTING_SHADER_2,
	SETTING_GAME_AWARE_SHADER,
	SETTING_FONT_SIZE,
	SETTING_KEEP_ASPECT_RATIO,
	SETTING_HW_TEXTURE_FILTER,
	SETTING_HW_TEXTURE_FILTER_2,
	SETTING_SCALE_ENABLED,
	SETTING_SCALE_FACTOR,
	SETTING_HW_OVERSCAN_AMOUNT,
	SETTING_THROTTLE_MODE,
	SETTING_TRIPLE_BUFFERING,
	SETTING_ENABLE_SCREENSHOTS,
	SETTING_SAVE_SHADER_PRESET,
	SETTING_APPLY_SHADER_PRESET_ON_STARTUP,
	SETTING_DEFAULT_VIDEO_ALL,
	SETTING_SOUND_MODE,
	SETTING_RSOUND_SERVER_IP_ADDRESS,
	SETTING_DEFAULT_AUDIO_ALL,
	SETTING_EMU_CURRENT_SAVE_STATE_SLOT,         
	SETTING_EMU_DEFAULT_ALL,              
	SETTING_EMU_VIDEO_DEFAULT_ALL,
	SETTING_EMU_AUDIO_DEFAULT_ALL,
	SETTING_PATH_DEFAULT_ROM_DIRECTORY,
	SETTING_PATH_SAVESTATES_DIRECTORY,
	SETTING_PATH_SRAM_DIRECTORY,
	SETTING_PATH_CHEATS,
	SETTING_PATH_BASE_DIRECTORY,
	SETTING_PATH_DEFAULT_ALL,
	SETTING_CONTROLS_SCHEME,
	SETTING_CONTROLS_NUMBER,
	SETTING_CONTROLS_DPAD_UP,
	SETTING_CONTROLS_DPAD_DOWN,
	SETTING_CONTROLS_DPAD_LEFT,
	SETTING_CONTROLS_DPAD_RIGHT,
	SETTING_CONTROLS_BUTTON_CIRCLE,
	SETTING_CONTROLS_BUTTON_CROSS,
	SETTING_CONTROLS_BUTTON_TRIANGLE,
	SETTING_CONTROLS_BUTTON_SQUARE,
	SETTING_CONTROLS_BUTTON_SELECT,
	SETTING_CONTROLS_BUTTON_START,
	SETTING_CONTROLS_BUTTON_L1,
	SETTING_CONTROLS_BUTTON_R1,
	SETTING_CONTROLS_BUTTON_L2,
	SETTING_CONTROLS_BUTTON_R2,
	SETTING_CONTROLS_BUTTON_L3,
	SETTING_CONTROLS_BUTTON_R3,
	SETTING_CONTROLS_BUTTON_L2_BUTTON_L3,
	SETTING_CONTROLS_BUTTON_L2_BUTTON_R3,
	SETTING_CONTROLS_BUTTON_L2_ANALOG_R_RIGHT,
	SETTING_CONTROLS_BUTTON_L2_ANALOG_R_LEFT,
	SETTING_CONTROLS_BUTTON_L2_ANALOG_R_UP,
	SETTING_CONTROLS_BUTTON_L2_ANALOG_R_DOWN,
	SETTING_CONTROLS_BUTTON_R2_ANALOG_R_RIGHT,
	SETTING_CONTROLS_BUTTON_R2_ANALOG_R_LEFT,
	SETTING_CONTROLS_BUTTON_R2_ANALOG_R_UP,
	SETTING_CONTROLS_BUTTON_R2_ANALOG_R_DOWN,
	SETTING_CONTROLS_BUTTON_R2_BUTTON_R3,
	SETTING_CONTROLS_BUTTON_R3_BUTTON_L3,
	SETTING_CONTROLS_ANALOG_R_UP,
	SETTING_CONTROLS_ANALOG_R_DOWN,
	SETTING_CONTROLS_ANALOG_R_LEFT,
	SETTING_CONTROLS_ANALOG_R_RIGHT,
	SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS,
	SETTING_CONTROLS_DEFAULT_ALL
};

#define FIRST_VIDEO_SETTING				0
#define FIRST_AUDIO_SETTING				SETTING_DEFAULT_VIDEO_ALL+1
#define FIRST_EMU_SETTING				SETTING_DEFAULT_AUDIO_ALL+1
#define FIRST_EMU_VIDEO_SETTING				SETTING_EMU_DEFAULT_ALL+1
#define FIRST_EMU_AUDIO_SETTING				SETTING_EMU_VIDEO_DEFAULT_ALL+1
#define FIRST_PATH_SETTING				SETTING_EMU_AUDIO_DEFAULT_ALL+1
#define FIRST_CONTROLS_SETTING_PAGE_1			SETTING_PATH_DEFAULT_ALL+1
#define FIRST_CONTROL_BIND				SETTING_CONTROLS_DPAD_UP

#define MAX_NO_OF_VIDEO_SETTINGS			SETTING_DEFAULT_VIDEO_ALL+1
#define MAX_NO_OF_AUDIO_SETTINGS			SETTING_DEFAULT_AUDIO_ALL+1
#define MAX_NO_OF_EMU_SETTINGS				SETTING_EMU_DEFAULT_ALL+1
#define MAX_NO_OF_EMU_VIDEO_SETTINGS			SETTING_EMU_VIDEO_DEFAULT_ALL+1
#define MAX_NO_OF_EMU_AUDIO_SETTINGS			SETTING_EMU_AUDIO_DEFAULT_ALL+1
#define MAX_NO_OF_PATH_SETTINGS				SETTING_PATH_DEFAULT_ALL+1
#define MAX_NO_OF_CONTROLS_SETTINGS			SETTING_CONTROLS_DEFAULT_ALL+1

void menu_init(void);
void menu_loop(void);

extern uint32_t menu_is_running;

#endif /* MENU_H_ */

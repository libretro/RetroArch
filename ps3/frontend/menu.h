/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#ifndef MENU_H_
#define MENU_H_

#if defined(__CELLOS_LV2__)
#define DEVICE_CAST gl_t*
#define DEVICE_PTR device_ptr
#define FONT_SIZE (g_console.menu_font_size)
#define render_msg_pre_func() gl_render_msg_pre(DEVICE_PTR)
#define render_msg_place_func(xpos, ypos, scale, color, msg) gl_render_msg_place(xpos, ypos, scale, color, msg)
#define render_msg_post_func() gl_render_msg_post(DEVICE_PTR)
#elif defined(_XBOX1)
#define DEVICE_CAST xdk_d3d_video_t*
#define DEVICE_PTR device_ptr
#define FONT_SIZE 0
#define render_msg_pre_func() xfonts_render_msg_pre(DEVICE_PTR)
#define render_msg_place_func(xpos, ypos, scale, color, msg) xfonts_render_msg_place(DEVICE_PTR, xpos, ypos, scale, msg)
#define render_msg_post_func() xfonts_render_msg_post(DEVICE_PTR)
#endif

typedef struct
{
   unsigned char enum_id;		/* enum ID of item				*/
   char text[128];			/* item label					*/
   char setting_text[256];		/* setting label				*/
   float text_xpos;			/* text X position (upper left corner)		*/
   float text_ypos;			/* text Y position (upper left corner)		*/
   unsigned text_color;			/* text color					*/
   char comment[256];			/* item comment					*/
   unsigned item_color;			/* color of item 				*/
   unsigned char page;			/* page						*/
} item;

typedef struct
{
   char title[64];
   unsigned char enum_id;
   unsigned char selected;
   unsigned char page;
   unsigned char first_setting;
   unsigned char max_settings;
   unsigned char category_id;
} menu;

enum
{
   CATEGORY_FILEBROWSER,
   CATEGORY_SETTINGS,
   CATEGORY_INGAME_MENU
};

enum
{
   FILE_BROWSER_MENU,
   GENERAL_VIDEO_MENU,
   GENERAL_AUDIO_MENU,
   EMU_GENERAL_MENU,
   EMU_VIDEO_MENU,
   EMU_AUDIO_MENU,
   PATH_MENU,
   CONTROLS_MENU,
   SHADER_CHOICE,
   PRESET_CHOICE,
   BORDER_CHOICE,
   LIBRETRO_CHOICE,
   PATH_SAVESTATES_DIR_CHOICE,
   PATH_DEFAULT_ROM_DIR_CHOICE,
   PATH_CHEATS_DIR_CHOICE,
   PATH_SRAM_DIR_CHOICE,
   PATH_SYSTEM_DIR_CHOICE,
   INPUT_PRESET_CHOICE,
   INGAME_MENU,
   INGAME_MENU_RESIZE,
   INGAME_MENU_SCREENSHOT
};

enum
{
   SETTING_CHANGE_RESOLUTION,
   SETTING_SHADER_PRESETS,
   SETTING_SHADER,
   SETTING_SHADER_2,
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
   SETTING_ENABLE_CUSTOM_BGM,
   SETTING_DEFAULT_AUDIO_ALL,
   SETTING_EMU_CURRENT_SAVE_STATE_SLOT,
   SETTING_EMU_SHOW_INFO_MSG,
   SETTING_ZIP_EXTRACT,
   SETTING_RARCH_DEFAULT_EMU,
   SETTING_EMU_DEFAULT_ALL,
   SETTING_EMU_REWIND_ENABLED,
   SETTING_EMU_VIDEO_DEFAULT_ALL,
   SETTING_EMU_AUDIO_MUTE,
   SETTING_EMU_AUDIO_DEFAULT_ALL,
   SETTING_PATH_DEFAULT_ROM_DIRECTORY,
   SETTING_PATH_SAVESTATES_DIRECTORY,
   SETTING_PATH_SRAM_DIRECTORY,
   SETTING_PATH_CHEATS,
   SETTING_PATH_SYSTEM,
   SETTING_ENABLE_SRAM_PATH,
   SETTING_ENABLE_STATE_PATH,
   SETTING_PATH_DEFAULT_ALL,
   SETTING_CONTROLS_SCHEME,
   SETTING_CONTROLS_NUMBER,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_B,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_Y,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_SELECT,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_START,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_UP,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_DOWN,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_LEFT,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_RIGHT,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_A,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_X,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L2,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R2,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L3,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R3,
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
#define FIRST_CONTROL_BIND				SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_B

#define MAX_NO_OF_VIDEO_SETTINGS			SETTING_DEFAULT_VIDEO_ALL+1
#define MAX_NO_OF_AUDIO_SETTINGS			SETTING_DEFAULT_AUDIO_ALL+1
#define MAX_NO_OF_EMU_SETTINGS				SETTING_EMU_DEFAULT_ALL+1
#define MAX_NO_OF_EMU_VIDEO_SETTINGS			SETTING_EMU_VIDEO_DEFAULT_ALL+1
#define MAX_NO_OF_EMU_AUDIO_SETTINGS			SETTING_EMU_AUDIO_DEFAULT_ALL+1
#define MAX_NO_OF_PATH_SETTINGS				SETTING_PATH_DEFAULT_ALL+1
#define MAX_NO_OF_CONTROLS_SETTINGS			SETTING_CONTROLS_DEFAULT_ALL+1

#ifndef _XBOX
void menu_init (void);
void menu_loop (void);
void menu_free (void);
#endif

#endif /* MENU_H_ */

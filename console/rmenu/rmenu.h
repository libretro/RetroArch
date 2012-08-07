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

#ifndef _RMENU_H_
#define _RMENU_H_

#if defined(__CELLOS_LV2__)
#define DEVICE_CAST gl_t*
#define input_ptr input_ps3
#define video_ptr video_gl
#define DEVICE_PTR device_ptr
#define HARDCODE_FONT_SIZE 0.91f
#define FONT_SIZE (g_console.menu_font_size)
#define render_msg_place_func(xpos, ypos, scale, color, msg) gl_render_msg_place(DEVICE_PTR, xpos, ypos, scale, color, msg)

#define NUM_ENTRY_PER_PAGE 15
#define POSITION_X 0.09f
#define POSITION_X_CENTER 0.5f
#define POSITION_Y_START 0.17f
#define POSITION_Y_INCREMENT 0.035f
#define POSITION_Y_BEGIN (POSITION_Y_START + POSITION_Y_INCREMENT)
#define COMMENT_TWO_Y_POSITION 0.91f
#define COMMENT_Y_POSITION 0.82f

#define MSG_QUEUE_X_POSITION g_settings.video.msg_pos_x
#define MSG_QUEUE_Y_POSITION 0.76f
#define MSG_QUEUE_FONT_SIZE 1.03f

#define MSG_PREV_NEXT_Y_POSITION 0.03f
#define CURRENT_PATH_Y_POSITION 0.15f
#define CURRENT_PATH_FONT_SIZE FONT_SIZE
#elif defined(_XBOX1)
#define DEVICE_CAST xdk_d3d_video_t*
#define input_ptr input_xinput
#define video_ptr video_xdk_d3d
#define DEVICE_PTR device_ptr
#define HARDCODE_FONT_SIZE 21
#define FONT_SIZE 21
#define render_msg_place_func(xpos, ypos, scale, color, msg) xfonts_render_msg_place(DEVICE_PTR, xpos, ypos, scale, msg)

#define NUM_ENTRY_PER_PAGE 12
#define POSITION_X m_menuMainRomListPos_x
#define POSITION_X_CENTER (m_menuMainRomListPos_x + 350)
#define POSITION_Y_START m_menuMainRomListPos_y
#define POSITION_Y_BEGIN (POSITION_Y_START + POSITION_Y_INCREMENT)
#define POSITION_Y_INCREMENT 20
#define COMMENT_Y_POSITION (ypos - ((POSITION_Y_INCREMENT/2) * 3))
#define COMMENT_TWO_Y_POSITION (ypos - ((POSITION_Y_INCREMENT/2) * 1))

#define MSG_QUEUE_X_POSITION POSITION_X
#define MSG_QUEUE_Y_POSITION (ypos - ((POSITION_Y_INCREMENT/2) * 7) + 5)
#define MSG_QUEUE_FONT_SIZE HARDCODE_FONT_SIZE

#define MSG_PREV_NEXT_Y_POSITION 24

#define CURRENT_PATH_Y_POSITION (m_menuMainRomListPos_y - ((POSITION_Y_INCREMENT/2)))
#define CURRENT_PATH_FONT_SIZE 21
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
#ifdef HAVE_XML
   PATH_CHEATS_DIR_CHOICE,
#endif
   PATH_SRAM_DIR_CHOICE,
   PATH_SYSTEM_DIR_CHOICE,
   INPUT_PRESET_CHOICE,
   INGAME_MENU,
   INGAME_MENU_RESIZE,
   INGAME_MENU_SCREENSHOT
};

enum
{
#ifdef __CELLOS_LV2__
   SETTING_CHANGE_RESOLUTION,
#endif
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
   SETTING_SHADER_PRESETS,
   SETTING_SHADER,
   SETTING_SHADER_2,
#endif
   SETTING_FONT_SIZE,
   SETTING_KEEP_ASPECT_RATIO,
   SETTING_HW_TEXTURE_FILTER,
#ifdef HAVE_FBO
   SETTING_HW_TEXTURE_FILTER_2,
   SETTING_SCALE_ENABLED,
   SETTING_SCALE_FACTOR,
#endif
#ifdef _XBOX1
   SETTING_FLICKER_FILTER,
   SETTING_SOFT_DISPLAY_FILTER,
#endif
   SETTING_HW_OVERSCAN_AMOUNT,
   SETTING_THROTTLE_MODE,
   SETTING_TRIPLE_BUFFERING,
   SETTING_ENABLE_SCREENSHOTS,
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
   SETTING_SAVE_SHADER_PRESET,
   SETTING_APPLY_SHADER_PRESET_ON_STARTUP,
#endif
   SETTING_DEFAULT_VIDEO_ALL,
   SETTING_SOUND_MODE,
#ifdef HAVE_RSOUND
   SETTING_RSOUND_SERVER_IP_ADDRESS,
#endif
   SETTING_ENABLE_CUSTOM_BGM,
   SETTING_DEFAULT_AUDIO_ALL,
   SETTING_EMU_CURRENT_SAVE_STATE_SLOT,
   SETTING_EMU_SHOW_DEBUG_INFO_MSG,
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
#ifdef HAVE_XML
   SETTING_PATH_CHEATS,
#endif
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

void menu_init (void);
void menu_loop (void);
void menu_free (void);

#endif /* MENU_H_ */

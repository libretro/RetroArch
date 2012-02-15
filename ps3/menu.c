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

#include <cell/sysmodule.h>
#include <sysutil/sysutil_screenshot.h>
#include <cell/dbgfont.h>

#include "cellframework2/input/pad_input.h"
#include "cellframework2/fileio/file_browser.h"

#include "../console/rom_ext.h"

#include "ps3_video_psgl.h"

#include "shared.h"
#include "../file.h"
#include "../general.h"
#include "../gfx/shader_cg.h"

#include "menu-port-defines.h"
#include "menu.h"
#include "menu-entries.h"

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

#define NUM_ENTRY_PER_PAGE 19

menu menuStack[25];
int menuStackindex = 0;
static bool set_initial_dir_tmpbrowser;
filebrowser_t browser;				/* main file browser->for rom browser*/
filebrowser_t tmpBrowser;			/* tmp file browser->for everything else*/
uint32_t set_shader = 0;
static uint32_t currently_selected_controller_menu = 0;

static menu menu_filebrowser = {
	"FILE BROWSER |",		/* title*/
	FILE_BROWSER_MENU,		/* enum*/
	0,				/* selected item*/
	0,				/* page*/
	1,				/* maxpages */
	1,				/* refreshpage*/
	NULL				/* items*/
};

static menu menu_generalvideosettings = {
	"VIDEO |",			/* title*/
	GENERAL_VIDEO_MENU,		/* enum*/
	FIRST_VIDEO_SETTING,		/* selected item*/
	0,				/* page*/
	MAX_NO_OF_VIDEO_SETTINGS/NUM_ENTRY_PER_PAGE,	/* max pages */
	1,				/* refreshpage*/
	FIRST_VIDEO_SETTING,		/* first setting*/
	MAX_NO_OF_VIDEO_SETTINGS,	/* max no of path settings*/
	items_generalsettings		/* items*/
};

static menu menu_generalaudiosettings = {
	"AUDIO |",			/* title*/
	GENERAL_AUDIO_MENU,		/* enum*/
	FIRST_AUDIO_SETTING,		/* selected item*/
	0,				/* page*/
	MAX_NO_OF_AUDIO_SETTINGS/NUM_ENTRY_PER_PAGE,	/* max pages */
	1,				/* refreshpage*/
	FIRST_AUDIO_SETTING,		/* first setting*/
	MAX_NO_OF_AUDIO_SETTINGS,	/* max no of path settings*/
	items_generalsettings		/* items*/
};

static menu menu_emu_settings = {
	EMU_MENU_TITLE,						/* title*/
	EMU_GENERAL_MENU,					/* enum*/
	FIRST_EMU_SETTING,					/* selected item*/
	0,							/* page*/
	MAX_NO_OF_EMU_SETTINGS/NUM_ENTRY_PER_PAGE,		/* max pages*/
	1,                      				/* refreshpage*/
	FIRST_EMU_SETTING,					/* first setting*/
	MAX_NO_OF_EMU_SETTINGS,					/* max no of path settings*/
	items_generalsettings					/* items*/
};

static menu menu_emu_videosettings = {
	VIDEO_MENU_TITLE,					/* title*/
	EMU_VIDEO_MENU,						/* enum */
	FIRST_EMU_VIDEO_SETTING,				/* selected item*/
	0,							/* page*/
	MAX_NO_OF_EMU_VIDEO_SETTINGS/NUM_ENTRY_PER_PAGE,	/* max pages */
	1,							/* refreshpage*/
	FIRST_EMU_VIDEO_SETTING,				/* first setting*/
	MAX_NO_OF_EMU_VIDEO_SETTINGS,				/* max no of settings*/
	items_generalsettings					/* items*/
};

static menu menu_emu_audiosettings = {
	AUDIO_MENU_TITLE,					/* title*/
	EMU_AUDIO_MENU,						/* enum*/
	FIRST_EMU_AUDIO_SETTING,				/* selected item*/
	0,							/* page*/
	MAX_NO_OF_EMU_AUDIO_SETTINGS/NUM_ENTRY_PER_PAGE,	/* max pages*/
	1,							/* refreshpage*/
	FIRST_EMU_AUDIO_SETTING,				/* first setting*/
	MAX_NO_OF_EMU_AUDIO_SETTINGS,				/* max no of path settings*/
	items_generalsettings					/* items*/
};

static menu menu_pathsettings = {
	"PATH |",						/* title*/
	PATH_MENU,						/* enum*/
	FIRST_PATH_SETTING,					/* selected item*/
	0,							/* page*/
	MAX_NO_OF_PATH_SETTINGS/NUM_ENTRY_PER_PAGE,		/* max pages*/
	1,							/* refreshpage*/
	FIRST_PATH_SETTING,					/* first setting*/
	MAX_NO_OF_PATH_SETTINGS,				/* max no of path settings*/
	items_generalsettings					/* items*/
};

static menu menu_controlssettings = {
	"CONTROLS |",						/* title */
	CONTROLS_MENU,						/* enum */
	FIRST_CONTROLS_SETTING_PAGE_1,				/* selected item */
	0,							/* page */
	MAX_NO_OF_CONTROLS_SETTINGS/NUM_ENTRY_PER_PAGE,		/* max pages */
	1,							/* refreshpage */
	FIRST_CONTROLS_SETTING_PAGE_1,				/* first setting */
	MAX_NO_OF_CONTROLS_SETTINGS,				/* max no of path settings*/
	items_generalsettings					/* items */
};

static void display_menubar(uint32_t menu_enum)
{
	cellDbgFontPuts    (0.09f,  0.05f,  Emulator_GetFontSize(),  menu_enum == GENERAL_VIDEO_MENU ? RED : GREEN,   menu_generalvideosettings.title);
	cellDbgFontPuts    (0.19f,  0.05f,  Emulator_GetFontSize(),  menu_enum == GENERAL_AUDIO_MENU ? RED : GREEN,  menu_generalaudiosettings.title);
	cellDbgFontPuts    (0.29f,  0.05f,  Emulator_GetFontSize(),  menu_enum == EMU_GENERAL_MENU ? RED : GREEN,  menu_emu_settings.title);
	cellDbgFontPuts    (0.39f,  0.05f,  Emulator_GetFontSize(),  menu_enum == EMU_VIDEO_MENU ? RED : GREEN,   menu_emu_videosettings.title);
	cellDbgFontPuts    (0.57f,  0.05f,  Emulator_GetFontSize(),  menu_enum == EMU_AUDIO_MENU ? RED : GREEN,   menu_emu_audiosettings.title);
	cellDbgFontPuts    (0.75f,  0.05f,  Emulator_GetFontSize(),  menu_enum == PATH_MENU ? RED : GREEN,  menu_pathsettings.title);
	cellDbgFontPuts    (0.84f,  0.05f,  Emulator_GetFontSize(), menu_enum == CONTROLS_MENU ? RED : GREEN,  menu_controlssettings.title); 
	cellDbgFontDraw();
}

static void browser_update(filebrowser_t * b)
{
	static uint64_t old_state = 0;
	uint64_t state, diff_state, button_was_pressed;

	state = cell_pad_input_poll_device(0);
	diff_state = old_state ^ state;
	button_was_pressed = old_state & diff_state;

	if(IS_TIMER_EXPIRED())
	{
		if (CTRL_LSTICK_DOWN(state))
		{
			if(b->currently_selected < b->file_count-1)
			{
				FILEBROWSER_INCREMENT_ENTRY_POINTER(b);
				set_text_message("", 4);
			}
		}

		if (CTRL_DOWN(state))
		{
			if(b->currently_selected < b->file_count-1)
			{
				FILEBROWSER_INCREMENT_ENTRY_POINTER(b);
				set_text_message("", 7);
			}
		}

		if (CTRL_LSTICK_UP(state))
		{
			if(b->currently_selected > 0)
			{
				FILEBROWSER_DECREMENT_ENTRY_POINTER(b);
				set_text_message("", 4);
			}
		}

		if (CTRL_UP(state))
		{
			if(b->currently_selected > 0)
			{
				FILEBROWSER_DECREMENT_ENTRY_POINTER(b);
				set_text_message("", 7);
			}
		}

		if (CTRL_RIGHT(state))
		{
			b->currently_selected = (MIN(b->currently_selected + 5, b->file_count-1));
			set_text_message("", 7);
		}

		if (CTRL_LSTICK_RIGHT(state))
		{
			b->currently_selected = (MIN(b->currently_selected + 5, b->file_count-1));
			set_text_message("", 4);
		}

		if (CTRL_LEFT(state))
		{
			if (b->currently_selected <= 5)
				b->currently_selected = 0;
			else
				b->currently_selected -= 5;

			set_text_message("", 7);
		}

		if (CTRL_LSTICK_LEFT(state))
		{
			if (b->currently_selected <= 5)
				b->currently_selected = 0;
			else
				b->currently_selected -= 5;

			set_text_message("", 4);
		}

		if (CTRL_R1(state))
		{
			b->currently_selected = (MIN(b->currently_selected + NUM_ENTRY_PER_PAGE, b->file_count-1));
			set_text_message("", 7);
		}

		if (CTRL_R2(state))
		{
			b->currently_selected = (MIN(b->currently_selected + 50, b->file_count-1));
			set_text_message("", 7);
		}

		if (CTRL_L2(state))
		{
			if (b->currently_selected <= NUM_ENTRY_PER_PAGE)
				b->currently_selected= 0;
			else
				b->currently_selected -= 50;

			set_text_message("", 7);
		}

		if (CTRL_L1(state))
		{
			if (b->currently_selected <= NUM_ENTRY_PER_PAGE)
				b->currently_selected= 0;
			else
				b->currently_selected -= NUM_ENTRY_PER_PAGE;

			set_text_message("", 7);
		}

		if (CTRL_CIRCLE(button_was_pressed))
		{
			old_state = state;
			filebrowser_pop_directory(b);
		}


		g_console.menu_enable = !((CTRL_L3(state) && CTRL_R3(state) && g_console.emulator_initialized));
		g_console.mode_switch = g_console.menu_enable ? MODE_MENU : MODE_EMULATION;

		old_state = state;
	}
}

static void browser_render(filebrowser_t * b)
{
	uint32_t file_count = b->file_count;
	int current_index, page_number, page_base, i;
	float currentX, currentY, ySpacing;

	current_index = b->currently_selected;
	page_number = current_index / NUM_ENTRY_PER_PAGE;
	page_base = page_number * NUM_ENTRY_PER_PAGE;

	currentX = 0.09f;
	currentY = 0.10f;
	ySpacing = 0.035f;

	for ( i = page_base; i < file_count && i < page_base + NUM_ENTRY_PER_PAGE; ++i)
	{
		currentY = currentY + ySpacing;
		cellDbgFontPuts(currentX, currentY, Emulator_GetFontSize(), i == current_index ? RED : b->cur[i].d_type == CELL_FS_TYPE_DIRECTORY ? GREEN : WHITE, b->cur[i].d_name);
		cellDbgFontDraw();
	}
	cellDbgFontDraw();
}

static void set_setting_label(menu * menu_obj, int currentsetting)
{
	switch(currentsetting)
	{
		case SETTING_CHANGE_RESOLUTION:
			if(g_console.initial_resolution_id == g_console.supported_resolutions[g_console.current_resolution_index])
				menu_obj->items[currentsetting].text_color = GREEN;
			else
				menu_obj->items[currentsetting].text_color = ORANGE;

			snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), ps3_get_resolution_label(g_console.supported_resolutions[g_console.current_resolution_index]));
			break;
		case SETTING_SHADER_PRESETS:
			/* add a comment */
			break;
		case SETTING_BORDER:
			break;
		case SETTING_SHADER:
			{
				char fname[MAX_PATH_LENGTH];
				fill_pathname_base(&fname, g_settings.video.cg_shader_path, sizeof(fname));
				snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "%s", fname);

				if(strcmp(g_settings.video.cg_shader_path,DEFAULT_SHADER_FILE) == 0)
					menu_obj->items[currentsetting].text_color = GREEN;
				else
					menu_obj->items[currentsetting].text_color = ORANGE;
			}
			break;
		case SETTING_SHADER_2:
			{
				char fname[MAX_PATH_LENGTH];
				fill_pathname_base(&fname, g_settings.video.second_pass_shader, sizeof(fname));
				snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "%s", fname);

				if(strcmp(g_settings.video.second_pass_shader,DEFAULT_SHADER_FILE) == 0)
					menu_obj->items[currentsetting].text_color = GREEN;
				else
					menu_obj->items[currentsetting].text_color = ORANGE;
			}
			break;
		case SETTING_GAME_AWARE_SHADER:
			break;
		case SETTING_FONT_SIZE:
			if(g_console.menu_font_size == 1.0f)
				menu_obj->items[currentsetting].text_color = GREEN;
			else
				menu_obj->items[currentsetting].text_color = ORANGE;
			snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "%f", g_console.menu_font_size);
			break;
		case SETTING_KEEP_ASPECT_RATIO:
			if(g_console.aspect_ratio_index == ASPECT_RATIO_4_3)
				menu_obj->items[currentsetting].text_color = GREEN;
			else
				menu_obj->items[currentsetting].text_color = ORANGE;
			snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), g_console.aspect_ratio_name);
			break;
		case SETTING_HW_TEXTURE_FILTER:
			if(g_settings.video.smooth)
			{
				snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "Linear interpolation");
				menu_obj->items[currentsetting].text_color = GREEN;
			}
			else
			{
				snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "Point filtering");
				menu_obj->items[currentsetting].text_color = ORANGE;
			}
			break;
		case SETTING_HW_TEXTURE_FILTER_2:
			if(g_settings.video.second_pass_smooth)
			{
				snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "Linear interpolation");
				menu_obj->items[currentsetting].text_color = GREEN;
			}
			else
			{
				snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "Point filtering");
				menu_obj->items[currentsetting].text_color = ORANGE;
			}
			break;
		case SETTING_SCALE_ENABLED:
			if(g_settings.video.render_to_texture)
			{
				snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "ON");
				menu_obj->items[currentsetting].text_color = GREEN;
			}
			else
			{
				snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "OFF");
				menu_obj->items[currentsetting].text_color = ORANGE;
			}
			break;
		case SETTING_SCALE_FACTOR:
			if(g_settings.video.fbo_scale_x == 2.0f)
				menu_obj->items[currentsetting].text_color = GREEN;
			else
				menu_obj->items[currentsetting].text_color = ORANGE;

			snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "%fx (X) / %fx (Y)", g_settings.video.fbo_scale_x, g_settings.video.fbo_scale_y);
			snprintf(menu_obj->items[currentsetting].comment, sizeof(menu_obj->items[currentsetting].comment), "INFO - [Custom Scaling Factor] is set to: '%fx (X) / %fx (Y)'.", g_settings.video.fbo_scale_x, g_settings.video.fbo_scale_y);
			break;
		case SETTING_HW_OVERSCAN_AMOUNT:
			if(g_console.overscan_amount == 0.0f)
				menu_obj->items[currentsetting].text_color = GREEN;
			else
				menu_obj->items[currentsetting].text_color = ORANGE;

			snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "%f", g_console.overscan_amount);
			break;
		case SETTING_THROTTLE_MODE:
			if(g_console.throttle_enable)
			{
				snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "ON");
				menu_obj->items[currentsetting].text_color = GREEN;
			}
			else
			{
				snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "OFF");
				menu_obj->items[currentsetting].text_color = ORANGE;
			}
			break;
		case SETTING_TRIPLE_BUFFERING:
			if(g_console.triple_buffering_enable)
			{
				snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "ON");
				menu_obj->items[currentsetting].text_color = GREEN;
			}
			else
			{
				snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "OFF");
				menu_obj->items[currentsetting].text_color = ORANGE;
			}
			break;
		case SETTING_ENABLE_SCREENSHOTS:
			if(g_console.screenshots_enable)
			{
				snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "ON");
				menu_obj->items[currentsetting].text_color = GREEN;
			}
			else
			{
				snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "OFF");
				menu_obj->items[currentsetting].text_color = ORANGE;
			}
			break;
		case SETTING_SAVE_SHADER_PRESET:
			break;
		case SETTING_APPLY_SHADER_PRESET_ON_STARTUP:
			break;
		case SETTING_DEFAULT_VIDEO_ALL:
			break;
		case SETTING_SOUND_MODE:
			break;
		case SETTING_RSOUND_SERVER_IP_ADDRESS:
			break;
		case SETTING_DEFAULT_AUDIO_ALL:
			break;
		case SETTING_EMU_CURRENT_SAVE_STATE_SLOT:
			if(g_extern.state_slot == 0)
				menu_obj->items[currentsetting].text_color = GREEN;
			else
				menu_obj->items[currentsetting].text_color = ORANGE;

			snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "%d", g_extern.state_slot);
			break;
		/* emu-specific */
		case SETTING_EMU_DEFAULT_ALL:
			if(menu_obj->selected == currentsetting)
				menu_obj->items[currentsetting].text_color = GREEN;
			else
				menu_obj->items[currentsetting].text_color = ORANGE;
			break;
		case SETTING_EMU_REWIND_ENABLED:
			if(g_settings.rewind_enable)
			{
				snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "ON");
				menu_obj->items[currentsetting].text_color = ORANGE;
				snprintf(menu_obj->items[currentsetting].comment, sizeof(menu_obj->items[currentsetting].comment), "INFO - [Rewind] feature is set to 'ON'. You can rewind the game in real-time.");
			}
			else
			{
				snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "OFF");
				menu_obj->items[currentsetting].text_color = GREEN;
				snprintf(menu_obj->items[currentsetting].comment, sizeof(menu_obj->items[currentsetting].comment), "INFO - [Rewind] feature is set to 'OFF'.");
			}
			break;
		case SETTING_EMU_AUDIO_MUTE:
			if(g_extern.audio_data.mute)
			{
				snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "ON");
				menu_obj->items[currentsetting].text_color = ORANGE;
				snprintf(menu_obj->items[currentsetting].comment, sizeof(menu_obj->items[currentsetting].comment), "INFO - [Audio Mute] feature is set to 'ON'. The game audio will be muted.");
			}
			else
			{
				snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "OFF");
				menu_obj->items[currentsetting].text_color = GREEN;
				snprintf(menu_obj->items[currentsetting].comment, sizeof(menu_obj->items[currentsetting].comment), "INFO - [Audio Mute] feature is set to 'OFF'.");
			}
			break;
		case SETTING_EMU_VIDEO_DEFAULT_ALL:
			if(menu_obj->selected == currentsetting)
				menu_obj->items[currentsetting].text_color = GREEN;
			else
				menu_obj->items[currentsetting].text_color = ORANGE;
			break;
		case SETTING_EMU_AUDIO_DEFAULT_ALL:
			if(menu_obj->selected == currentsetting)
				menu_obj->items[currentsetting].text_color = GREEN;
			else
				menu_obj->items[currentsetting].text_color = ORANGE;
			break;
		case SETTING_PATH_DEFAULT_ROM_DIRECTORY:
			if(!(strcmp(g_console.default_rom_startup_dir, "/")))
				menu_obj->items[currentsetting].text_color = GREEN;
			else
				menu_obj->items[currentsetting].text_color = ORANGE;

			snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), g_console.default_rom_startup_dir);
			break;
		case SETTING_PATH_SAVESTATES_DIRECTORY:
			if(!(strcmp(g_console.default_savestate_dir, usrDirPath)))
				menu_obj->items[currentsetting].text_color = GREEN;
			else
				menu_obj->items[currentsetting].text_color = ORANGE;

			snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), g_console.default_savestate_dir);
			break;
		case SETTING_PATH_SRAM_DIRECTORY:
			if(!(strcmp(g_console.default_sram_dir, usrDirPath)))
				menu_obj->items[currentsetting].text_color = GREEN;
			else
				menu_obj->items[currentsetting].text_color = ORANGE;

			snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), g_console.default_sram_dir);
			break;
		case SETTING_PATH_CHEATS:
			if(!(strcmp(g_settings.cheat_database, usrDirPath)))
				menu_obj->items[currentsetting].text_color = GREEN;
			else
				menu_obj->items[currentsetting].text_color = ORANGE;

			snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), g_settings.cheat_database);
			break;
		case SETTING_ENABLE_SRAM_PATH:
			if(g_console.default_sram_dir_enable)
			{
				menu_obj->items[currentsetting].text_color = ORANGE;
				snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "ON");
			}
			else
			{
				menu_obj->items[currentsetting].text_color = GREEN;
				snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "OFF");
			}

			break;
		case SETTING_ENABLE_STATE_PATH:
			if(g_console.default_savestate_dir_enable)
			{
				menu_obj->items[currentsetting].text_color = ORANGE;
				snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "ON");
			}
			else
			{
				menu_obj->items[currentsetting].text_color = GREEN;
				snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "OFF");
			}
			break;
		case SETTING_PATH_DEFAULT_ALL:
			if(menu_obj->selected == currentsetting)
				menu_obj->items[currentsetting].text_color = GREEN;
			else
				menu_obj->items[currentsetting].text_color = ORANGE;
			break;
		case SETTING_CONTROLS_SCHEME:
			break;
		case SETTING_CONTROLS_NUMBER:
			break;
		case SETTING_CONTROLS_DPAD_UP:
		case SETTING_CONTROLS_DPAD_DOWN:
		case SETTING_CONTROLS_DPAD_LEFT:
		case SETTING_CONTROLS_DPAD_RIGHT:
		case SETTING_CONTROLS_BUTTON_CIRCLE:
		case SETTING_CONTROLS_BUTTON_CROSS:
		case SETTING_CONTROLS_BUTTON_TRIANGLE:
		case SETTING_CONTROLS_BUTTON_SQUARE:
		case SETTING_CONTROLS_BUTTON_SELECT:
		case SETTING_CONTROLS_BUTTON_START:
		case SETTING_CONTROLS_BUTTON_L1:
		case SETTING_CONTROLS_BUTTON_R1:
		case SETTING_CONTROLS_BUTTON_L2:
		case SETTING_CONTROLS_BUTTON_R2:
		case SETTING_CONTROLS_BUTTON_L3:
		case SETTING_CONTROLS_BUTTON_R3:
		case SETTING_CONTROLS_BUTTON_L2_BUTTON_L3:
		case SETTING_CONTROLS_BUTTON_L2_BUTTON_R3:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_RIGHT:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_LEFT:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_UP:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_DOWN:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_RIGHT:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_LEFT:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_UP:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_DOWN:
		case SETTING_CONTROLS_BUTTON_R2_BUTTON_R3:
		case SETTING_CONTROLS_BUTTON_R3_BUTTON_L3:
		case SETTING_CONTROLS_ANALOG_R_UP:
		case SETTING_CONTROLS_ANALOG_R_DOWN:
		case SETTING_CONTROLS_ANALOG_R_LEFT:
		case SETTING_CONTROLS_ANALOG_R_RIGHT:
			break;
		case SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS:
			if(menu_obj->selected == currentsetting)
				menu_obj->items[currentsetting].text_color = GREEN;
			else
				menu_obj->items[currentsetting].text_color = ORANGE;
			break;
		case SETTING_CONTROLS_DEFAULT_ALL:
			if(menu_obj->selected == currentsetting)
				menu_obj->items[currentsetting].text_color = GREEN;
			else
				menu_obj->items[currentsetting].text_color = ORANGE;
			break;
		default:
			break;
	}
}

static void menu_init_settings_pages(menu * menu_obj)
{
	int page, i, j;
	float increment;

	page = 0;
	j = 0;
	increment = 0.13f;

	for(i = menu_obj->first_setting; i < menu_obj->max_settings; i++)
	{
		if(!(j < (NUM_ENTRY_PER_PAGE)))
		{
			j = 0;
			increment = 0.13f;
			page++;
		}

		menu_obj->items[i].text_xpos = 0.09f;
		menu_obj->items[i].text_ypos = increment; 
		menu_obj->items[i].page = page;
		set_setting_label(menu_obj, i);
		increment += 0.03f;
		j++;
	}
	menu_obj->refreshpage = 0;
}

static void menu_reinit_settings (void)
{
	menu_init_settings_pages(&menu_generalvideosettings);
	menu_init_settings_pages(&menu_generalaudiosettings);
	menu_init_settings_pages(&menu_emu_settings);
	menu_init_settings_pages(&menu_emu_videosettings);
	menu_init_settings_pages(&menu_emu_audiosettings);
	menu_init_settings_pages(&menu_pathsettings);
	menu_init_settings_pages(&menu_controlssettings);
}

static void select_file(uint32_t menu_id)
{
	char extensions[256], title[256], object[256], comment[256], dir_path[MAX_PATH_LENGTH],
	path[MAX_PATH_LENGTH], *separatorslash;
	uint64_t state, diff_state, button_was_pressed;
	static uint64_t old_state = 0;

	state = cell_pad_input_poll_device(0);
	diff_state = old_state ^ state;
	button_was_pressed = old_state & diff_state;

	switch(menu_id)
	{
		case GAME_AWARE_SHADER_CHOICE:
			strncpy(dir_path, GAME_AWARE_SHADER_DIR_PATH, sizeof(dir_path));
			strncpy(extensions, "cfg|CFG", sizeof(extensions));
			strncpy(title, "GAME AWARE SHADER SELECTION", sizeof(title));
			strncpy(object, "Game Aware Shader", sizeof(object));
			strncpy(comment, "INFO - Select a 'Game Aware Shader' script from the menu by pressing X.", sizeof(comment));
			break;
		case SHADER_CHOICE:
			strncpy(dir_path, SHADERS_DIR_PATH, sizeof(dir_path));
			strncpy(extensions, "cg|CG", sizeof(extensions));
			strncpy(title, "SHADER SELECTION", sizeof(title));
			strncpy(object, "Shader", sizeof(object));
			strncpy(comment, "INFO - Select a shader from the menu by pressing the X button.", sizeof(comment));
			break;
		case PRESET_CHOICE:
			strncpy(dir_path, PRESETS_DIR_PATH, sizeof(dir_path));
			strncpy(extensions, "conf|CONF", sizeof(extensions));
			strncpy(title, "SHADER PRESETS SELECTION", sizeof(title));
			strncpy(object, "Shader", sizeof(object));
			strncpy(object, "Shader preset", sizeof(object));
                        strncpy(comment, "INFO - Select a shader preset from the menu by pressing the X button. ", sizeof(comment));
			break;
		case INPUT_PRESET_CHOICE:
			strncpy(dir_path, INPUT_PRESETS_DIR_PATH, sizeof(dir_path));
			strncpy(extensions, "conf|CONF", sizeof(extensions));
			strncpy(title, "INPUT PRESETS SELECTION", sizeof(title));
			strncpy(object, "Input", sizeof(object));
			strncpy(object, "Input preset", sizeof(object));
                        strncpy(comment, "INFO - Select an input preset from the menu by pressing the X button. ", sizeof(comment));
			break;
		case BORDER_CHOICE:
			strncpy(dir_path, BORDERS_DIR_PATH, sizeof(dir_path));
			strncpy(extensions, "png|PNG|jpg|JPG|JPEG|jpeg", sizeof(extensions));
			strncpy(title, "BORDER SELECTION", sizeof(title));
			strncpy(object, "Border", sizeof(object));
			strncpy(object, "Border image file", sizeof(object));
			strncpy(comment, "INFO - Select a border image file from the menu by pressing the X button. ", sizeof(comment));
			break;
		EXTRA_SELECT_FILE_PART1();
	}

	if(set_initial_dir_tmpbrowser)
	{
		filebrowser_new(&tmpBrowser, dir_path, extensions);
		set_initial_dir_tmpbrowser = false;
	}

	browser_update(&tmpBrowser);

	if (CTRL_START(button_was_pressed))
		filebrowser_reset_start_directory(&tmpBrowser, "/", extensions);

	if (CTRL_CROSS(button_was_pressed))
	{
		if(FILEBROWSER_IS_CURRENT_A_DIRECTORY(tmpBrowser))
		{
			/*if 'filename' is in fact '..' - then pop back directory instead of 
			adding '..' to filename path */
			if(tmpBrowser.currently_selected == 0)
			{
				old_state = state;
				filebrowser_pop_directory(&tmpBrowser);
			}
			else
			{
                                separatorslash = (strcmp(FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser),"/") == 0) ? "" : "/";
				snprintf(path, sizeof(path), "%s%s%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser), separatorslash, FILEBROWSER_GET_CURRENT_FILENAME(tmpBrowser));
				filebrowser_push_directory(&tmpBrowser, path, true);
			}
		}
		else if (FILEBROWSER_IS_CURRENT_A_FILE(tmpBrowser))
		{
			snprintf(path, sizeof(path), "%s/%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser), FILEBROWSER_GET_CURRENT_FILENAME(tmpBrowser));
			printf("path: %s\n", path);

			switch(menu_id)
			{
				case GAME_AWARE_SHADER_CHOICE:
					break;
				case SHADER_CHOICE:
					gl_cg_load_shader(set_shader+1, path);
					switch(set_shader+1)
					{
						case 1:
							strlcpy(g_settings.video.cg_shader_path, path, sizeof(g_settings.video.cg_shader_path));
							break;
						case 2:
							strlcpy(g_settings.video.second_pass_shader, path, sizeof(g_settings.video.second_pass_shader));
							break;
					}
					menu_reinit_settings();
					break;
				case PRESET_CHOICE:
					break;
				case INPUT_PRESET_CHOICE:
					break;
				case BORDER_CHOICE:
					break;
				EXTRA_SELECT_FILE_PART2();
			}

			menuStackindex--;
		}
	}

	if (CTRL_TRIANGLE(button_was_pressed))
		menuStackindex--;

        cellDbgFontPrintf(0.09f, 0.09f, Emulator_GetFontSize(), YELLOW, "PATH: %s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser));
	cellDbgFontPuts	(0.09f,	0.05f,	Emulator_GetFontSize(),	RED,	title);
	cellDbgFontPrintf(0.09f, 0.92f, 0.92, YELLOW, "X - Select %s  /\\ - return to settings  START - Reset Startdir", object);
	cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "%s", comment);
	cellDbgFontDraw();

	browser_render(&tmpBrowser);
	old_state = state;
}

static void select_directory(uint32_t menu_id)
{
        char path[1024], newpath[1024], *separatorslash;
	uint64_t state, diff_state, button_was_pressed;
        static uint64_t old_state = 0;

        state = cell_pad_input_poll_device(0);
        diff_state = old_state ^ state;
        button_was_pressed = old_state & diff_state;

	if(set_initial_dir_tmpbrowser)
	{
		filebrowser_new(&tmpBrowser, "/\0", "empty");
		set_initial_dir_tmpbrowser = false;
	}

        browser_update(&tmpBrowser);

        if (CTRL_START(button_was_pressed))
		filebrowser_reset_start_directory(&tmpBrowser, "/","empty");

        if (CTRL_SQUARE(button_was_pressed))
        {
                if(FILEBROWSER_IS_CURRENT_A_DIRECTORY(tmpBrowser))
                {
                        snprintf(path, sizeof(path), "%s/%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser), FILEBROWSER_GET_CURRENT_FILENAME(tmpBrowser));
                        switch(menu_id)
                        {
                                case PATH_SAVESTATES_DIR_CHOICE:
                                        strcpy(g_console.default_savestate_dir, path);
                                        break;
                                case PATH_SRAM_DIR_CHOICE:
					strcpy(g_console.default_sram_dir, path);
                                        break;
                                case PATH_DEFAULT_ROM_DIR_CHOICE:
                                        strcpy(g_console.default_rom_startup_dir, path);
                                        break;
				case PATH_CHEATS_DIR_CHOICE:
					strcpy(g_settings.cheat_database, path);
					break;
                        }
                        menuStackindex--;
                }
        }
        if (CTRL_TRIANGLE(button_was_pressed))
        {
                strcpy(path, usrDirPath);
                switch(menu_id)
                {
                        case PATH_SAVESTATES_DIR_CHOICE:
				strcpy(g_console.default_savestate_dir, path);
                                break;
                        case PATH_SRAM_DIR_CHOICE:
				strcpy(g_console.default_sram_dir, path);
                                break;
                        case PATH_DEFAULT_ROM_DIR_CHOICE:
				strcpy(g_console.default_rom_startup_dir, path);
                                break;
			case PATH_CHEATS_DIR_CHOICE:
				strcpy(g_settings.cheat_database, path);
				break;
                }
                menuStackindex--;
        }
        if (CTRL_CROSS(button_was_pressed))
        {
                if(FILEBROWSER_IS_CURRENT_A_DIRECTORY(tmpBrowser))
                {
                        /* if 'filename' is in fact '..' - then pop back 
			directory instead of adding '..' to filename path */

                        if(tmpBrowser.currently_selected == 0)
                        {
                                old_state = state;
				filebrowser_pop_directory(&tmpBrowser);
                        }
                        else
                        {
                                separatorslash = (strcmp(FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser),"/") == 0) ? "" : "/";
                                snprintf(newpath, sizeof(newpath), "%s%s%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser), separatorslash, FILEBROWSER_GET_CURRENT_FILENAME(tmpBrowser));
                                filebrowser_push_directory(&tmpBrowser, newpath, false);
                        }
                }
        }

        cellDbgFontPrintf (0.09f,  0.09f, Emulator_GetFontSize(), YELLOW, 
	"PATH: %s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser));
        cellDbgFontPuts (0.09f, 0.05f,  Emulator_GetFontSize(), RED,    "DIRECTORY SELECTION");
        cellDbgFontPuts(0.09f, 0.93f, 0.92f, YELLOW,
	"X - Enter dir  /\\ - return to settings  START - Reset Startdir");
        cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "%s",
	"INFO - Browse to a directory and assign it as the path by\npressing SQUARE button.");
        cellDbgFontDraw();

        browser_render(&tmpBrowser);
        old_state = state;
}




static void apply_scaling(void)
{
}

static void producesettingentry(menu * menu_obj, uint64_t switchvalue)
{
	uint64_t state;

	state = cell_pad_input_poll_device(0);

	switch(switchvalue)
	{
		case SETTING_CHANGE_RESOLUTION:
			if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) )
			{
				ps3_next_resolution();
				set_text_message("", 7);
			}
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) )
			{
				ps3_previous_resolution();
				set_text_message("", 7);
			}
			if(CTRL_CROSS(state))
			{
				if (g_console.supported_resolutions[g_console.current_resolution_index] == CELL_VIDEO_OUT_RESOLUTION_576)
				{
					if(ps3_check_resolution(CELL_VIDEO_OUT_RESOLUTION_576))
					{
						//ps3graphics_set_pal60hz(Settings.PS3PALTemporalMode60Hz);
						ps3graphics_video_reinit();
					}
				}
				else
				{
					//ps3graphics_set_pal60hz(0);
					ps3graphics_video_reinit();
				}
			}
			break;
			/*
			   case SETTING_PAL60_MODE:
			   if(CTRL_RIGHT(state) || CTRL_LSTICK_LEFT(state) || CTRL_CROSS(state) || CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state))
			   {
			   if (Graphics->GetCurrentResolution() == CELL_VIDEO_OUT_RESOLUTION_576)
			   {
			   if(Graphics->CheckResolution(CELL_VIDEO_OUT_RESOLUTION_576))
			   {
			   Settings.PS3PALTemporalMode60Hz = !Settings.PS3PALTemporalMode60Hz;
			   Graphics->SetPAL60Hz(Settings.PS3PALTemporalMode60Hz);
			   Graphics->SwitchResolution(Graphics->GetCurrentResolution(), Settings.PS3PALTemporalMode60Hz, Settings.TripleBuffering);
			   }
			   }

			   }
			   break;
			 */
		case SETTING_GAME_AWARE_SHADER:
			break;
		case SETTING_SHADER_PRESETS:
			break;
		case SETTING_BORDER:
			break;
		case SETTING_SHADER:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				menuStackindex++;
				menuStack[menuStackindex] = menu_filebrowser;
				menuStack[menuStackindex].enum_id = SHADER_CHOICE;
				set_shader = 0;
				set_initial_dir_tmpbrowser = true;
			}
			if(CTRL_START(state))
			{
				gl_cg_load_shader(1, DEFAULT_SHADER_FILE);
				strlcpy(g_settings.video.cg_shader_path, DEFAULT_SHADER_FILE, sizeof(g_settings.video.cg_shader_path));
				menu_reinit_settings();
			}
			break;
		case SETTING_SHADER_2:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				menuStackindex++;
				menuStack[menuStackindex] = menu_filebrowser;
				menuStack[menuStackindex].enum_id = SHADER_CHOICE;
				set_shader = 1;
				set_initial_dir_tmpbrowser = true;
			}
			if(CTRL_START(state))
			{
				gl_cg_load_shader(2, DEFAULT_SHADER_FILE);
				strlcpy(g_settings.video.second_pass_shader, DEFAULT_SHADER_FILE, sizeof(g_settings.video.second_pass_shader));
				menu_reinit_settings();
			}
			break;
		case SETTING_FONT_SIZE:
			if(CTRL_LEFT(state)  || CTRL_LSTICK_LEFT(state) || CTRL_CROSS(state))
			{
				if(g_console.menu_font_size > 0) 
				{
					g_console.menu_font_size -= 0.01f;
					set_text_message("", 7);
				}
			}
			if(CTRL_RIGHT(state)  || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				if((g_console.menu_font_size < 2.0f))
				{
					g_console.menu_font_size += 0.01f;
					set_text_message("", 7);
				}
			}
			if(CTRL_START(state))
				g_console.menu_font_size = 1.0f;
			break;
		case SETTING_KEEP_ASPECT_RATIO:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state))
			{
				if(g_console.aspect_ratio_index > 0)
				{
					g_console.aspect_ratio_index--;
					ps3graphics_set_aspect_ratio(g_console.aspect_ratio_index);
					set_text_message("", 7);
				}
			}
			if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state))
			{
				if(g_console.aspect_ratio_index < LAST_ASPECT_RATIO)
				{
					g_console.aspect_ratio_index++;
					ps3graphics_set_aspect_ratio(g_console.aspect_ratio_index);
					set_text_message("", 7);
				}
			}
			if(CTRL_START(state))
			{
				g_console.aspect_ratio_index = ASPECT_RATIO_4_3;
				ps3graphics_set_aspect_ratio(g_console.aspect_ratio_index);
				set_text_message("", 7);
			}
			break;
		case SETTING_HW_TEXTURE_FILTER:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				g_settings.video.smooth = !g_settings.video.smooth;
				ps3_set_filtering(1, g_settings.video.smooth);
				set_text_message("", 7);
			}
			if(CTRL_START(state))
			{
				g_settings.video.smooth = 1;
				ps3_set_filtering(1, g_settings.video.smooth);
			}
			break;
		case SETTING_HW_TEXTURE_FILTER_2:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				g_settings.video.second_pass_smooth = !g_settings.video.second_pass_smooth;
				ps3_set_filtering(2, g_settings.video.second_pass_smooth);
				set_text_message("", 7);
			}
			if(CTRL_START(state))
			{
				g_settings.video.second_pass_smooth = 1;
				ps3_set_filtering(2, g_settings.video.second_pass_smooth);
			}
			break;
		case SETTING_SCALE_ENABLED:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				g_settings.video.render_to_texture = !g_settings.video.render_to_texture;

				#if 0
				if(g_settings.video.render_to_texture)
					ps3graphics_set_fbo_scale(1, Settings.ScaleFactor);
				else
					ps3graphics_set_fbo_scale(0, 0);
				#endif

				set_text_message("", 7);
			}
			if(CTRL_START(state))
			{
				g_settings.video.render_to_texture = 2;
				//ps3graphics_set_fbo_scale(1, Settings.ScaleFactor);
			}
			break;
		case SETTING_SCALE_FACTOR:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state))
			{
				if((g_settings.video.fbo_scale_x > 1.0f))
				{
					g_settings.video.fbo_scale_x -= 1.0f;
					g_settings.video.fbo_scale_y -= 1.0f;
					apply_scaling();
				}
				set_text_message("", 7);
			}
			if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				if((g_settings.video.fbo_scale_x < 5.0f))
				{
					g_settings.video.fbo_scale_x += 1.0f;
					g_settings.video.fbo_scale_y += 1.0f;
					apply_scaling();
				}
				set_text_message("", 7);
			}
			if(CTRL_START(state))
			{
				g_settings.video.fbo_scale_x = 2.0f;
				g_settings.video.fbo_scale_y = 2.0f;
				apply_scaling();
			}
			break;
		case SETTING_HW_OVERSCAN_AMOUNT:
			if(CTRL_LEFT(state)  ||  CTRL_LSTICK_LEFT(state) || CTRL_CROSS(state))
			{
				g_console.overscan_amount -= 0.01f;
				g_console.overscan_enable = true;

				if(g_console.overscan_amount == 0.0f)
					g_console.overscan_enable = false;

				ps3graphics_set_overscan(g_console.overscan_enable, g_console.overscan_amount, 1);
			}
			if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				g_console.overscan_amount += 0.01f;
				g_console.overscan_enable = true;

				if(g_console.overscan_amount == 0.0f)
					g_console.overscan_enable = 0;

				ps3graphics_set_overscan(g_console.overscan_enable, g_console.overscan_amount, 1);
			}
			if(CTRL_START(state))
			{
				g_console.overscan_amount = 0.0f;
				g_console.overscan_enable = false;
				ps3graphics_set_overscan(g_console.overscan_enable, g_console.overscan_amount, 1);
			}
			break;
		case SETTING_THROTTLE_MODE:
			if(CTRL_LEFT(state)  || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state))
			{
				g_console.throttle_enable = !g_console.throttle_enable;
				ps3graphics_set_vsync(g_console.throttle_enable);
				set_text_message("", 7);
			}
			if(CTRL_START(state))
			{
				g_console.throttle_enable = true;
				ps3graphics_set_vsync(g_console.throttle_enable);
				set_text_message("", 7);
			}
			break;
		case SETTING_TRIPLE_BUFFERING:
			if(CTRL_LEFT(state)  || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state))
			{
				g_console.triple_buffering_enable = !g_console.triple_buffering_enable;
				ps3graphics_video_reinit();
				set_text_message("", 7);
			}
			if(CTRL_START(state))
			{
				if(!g_console.triple_buffering_enable)
				{
					g_console.triple_buffering_enable = true;
					ps3graphics_video_reinit();
				}
			}
			break;
		case SETTING_ENABLE_SCREENSHOTS:
			if(CTRL_LEFT(state)  || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state))
			{
#if(CELL_SDK_VERSION > 0x340000)
				g_console.screenshots_enable = !g_console.screenshots_enable;
				if(g_console.screenshots_enable)
				{
					cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_SCREENSHOT);
					CellScreenShotSetParam screenshot_param = {0, 0, 0, 0};

					screenshot_param.photo_title = EMULATOR_NAME;
					screenshot_param.game_title = EMULATOR_NAME;
					cellScreenShotSetParameter (&screenshot_param);
					cellScreenShotEnable();
				}
				else
				{
					cellScreenShotDisable();
					cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_SCREENSHOT);
				}

				set_text_message("", 7);
#endif
			}
			if(CTRL_START(state))
			{
#if(CELL_SDK_VERSION > 0x340000)
				g_console.screenshots_enable = false;
#endif
			}
			break;
		case SETTING_SAVE_SHADER_PRESET:
			break;
		case SETTING_APPLY_SHADER_PRESET_ON_STARTUP:
			break;
		case SETTING_DEFAULT_VIDEO_ALL:
			break;
		case SETTING_SOUND_MODE:
			break;
		case SETTING_RSOUND_SERVER_IP_ADDRESS:
			break;
		case SETTING_DEFAULT_AUDIO_ALL:
			break;
		case SETTING_EMU_CURRENT_SAVE_STATE_SLOT:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_CROSS(state))
			{
				if(g_extern.state_slot != 0)
					g_extern.state_slot--;

				set_text_message("", 7);
			}
			if(CTRL_RIGHT(state)  || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				g_extern.state_slot++;
				set_text_message("", 7);
			}

			if(CTRL_START(state))
				g_extern.state_slot = 0;
			break;
		case SETTING_EMU_REWIND_ENABLED:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				g_settings.rewind_enable = !g_settings.rewind_enable;

				set_text_message("", 7);
			}
			if(CTRL_START(state))
			{
				g_settings.rewind_enable = false;
			}
			break;
		case SETTING_EMU_AUDIO_MUTE:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				g_extern.audio_data.mute = !g_extern.audio_data.mute;

				set_text_message("", 7);
			}
			if(CTRL_START(state))
			{
				g_extern.audio_data.mute = false;
			}
			break;
		case SETTING_EMU_VIDEO_DEFAULT_ALL:
			break;
		case SETTING_EMU_AUDIO_DEFAULT_ALL:
			break;
		case SETTING_PATH_DEFAULT_ROM_DIRECTORY:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				menuStackindex++;
				menuStack[menuStackindex] = menu_filebrowser;
				menuStack[menuStackindex].enum_id = PATH_DEFAULT_ROM_DIR_CHOICE;
				set_initial_dir_tmpbrowser = true;
			}

			if(CTRL_START(state))
				strcpy(g_console.default_rom_startup_dir, "/");
			break;
		case SETTING_PATH_SAVESTATES_DIRECTORY:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				menuStackindex++;
				menuStack[menuStackindex] = menu_filebrowser;
				menuStack[menuStackindex].enum_id = PATH_SAVESTATES_DIR_CHOICE;
				set_initial_dir_tmpbrowser = true;
			}

			if(CTRL_START(state))
				strcpy(g_console.default_savestate_dir, usrDirPath);

			break;
		case SETTING_PATH_SRAM_DIRECTORY:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) ||  CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				menuStackindex++;
				menuStack[menuStackindex] = menu_filebrowser;
				menuStack[menuStackindex].enum_id = PATH_SRAM_DIR_CHOICE;
				set_initial_dir_tmpbrowser = true;
			}

			if(CTRL_START(state))
				strcpy(g_console.default_sram_dir, "");
			break;
		case SETTING_PATH_CHEATS:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) ||  CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				menuStackindex++;
				menuStack[menuStackindex] = menu_filebrowser;
				menuStack[menuStackindex].enum_id = PATH_CHEATS_DIR_CHOICE;
				set_initial_dir_tmpbrowser = true;
			}

			if(CTRL_START(state))
				strcpy(g_settings.cheat_database, usrDirPath);
			break;
		case SETTING_ENABLE_SRAM_PATH:
			if(CTRL_LEFT(state)  || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state))
			{
				g_console.default_sram_dir_enable = !g_console.default_sram_dir_enable;
				menu_reinit_settings();
				set_text_message("", 7);
			}
			if(CTRL_START(state))
			{
				g_console.default_sram_dir_enable = true;
				menu_reinit_settings();
				set_text_message("", 7);
			}
			break;
		case SETTING_ENABLE_STATE_PATH:
			if(CTRL_LEFT(state)  || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state))
			{
				g_console.default_savestate_dir_enable = !g_console.default_savestate_dir_enable;
				menu_reinit_settings();
				set_text_message("", 7);
			}
			if(CTRL_START(state))
			{
				g_console.default_savestate_dir_enable = true;
				menu_reinit_settings();
				set_text_message("", 7);
			}
			break;
		case SETTING_PATH_DEFAULT_ALL:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) ||  CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state) || CTRL_START(state))
			{
				strcpy(g_console.default_rom_startup_dir, "/");
				strcpy(g_console.default_savestate_dir, usrDirPath);
				strcpy(g_settings.cheat_database, usrDirPath);
				strcpy(g_console.default_sram_dir, "");

				menu_reinit_settings();
			}
			break;
		case SETTING_CONTROLS_SCHEME:
			break;
		case SETTING_CONTROLS_NUMBER:
			break; 
		case SETTING_CONTROLS_DPAD_UP:
		case SETTING_CONTROLS_DPAD_DOWN:
		case SETTING_CONTROLS_DPAD_LEFT:
		case SETTING_CONTROLS_DPAD_RIGHT:
		case SETTING_CONTROLS_BUTTON_CIRCLE:
		case SETTING_CONTROLS_BUTTON_CROSS:
		case SETTING_CONTROLS_BUTTON_TRIANGLE:
		case SETTING_CONTROLS_BUTTON_SQUARE:
		case SETTING_CONTROLS_BUTTON_SELECT:
		case SETTING_CONTROLS_BUTTON_START:
		case SETTING_CONTROLS_BUTTON_L1:
		case SETTING_CONTROLS_BUTTON_R1:
		case SETTING_CONTROLS_BUTTON_L2:
		case SETTING_CONTROLS_BUTTON_R2:
		case SETTING_CONTROLS_BUTTON_L3:
		case SETTING_CONTROLS_BUTTON_R3:
		case SETTING_CONTROLS_BUTTON_L2_BUTTON_L3:
		case SETTING_CONTROLS_BUTTON_L2_BUTTON_R3:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_RIGHT:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_LEFT:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_UP:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_DOWN:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_RIGHT:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_LEFT:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_UP:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_DOWN:
		case SETTING_CONTROLS_BUTTON_R2_BUTTON_R3:
		case SETTING_CONTROLS_BUTTON_R3_BUTTON_L3:
		case SETTING_CONTROLS_ANALOG_R_UP:
		case SETTING_CONTROLS_ANALOG_R_DOWN:
		case SETTING_CONTROLS_ANALOG_R_LEFT:
		case SETTING_CONTROLS_ANALOG_R_RIGHT:
			break;
		case SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS:
			break;
		case SETTING_CONTROLS_DEFAULT_ALL:
			break;
	}

	set_setting_label(menu_obj, switchvalue);
}

static void select_setting(menu * menu_obj)
{
	uint64_t state, diff_state, button_was_pressed, i;
	static uint64_t old_state = 0;

	state = cell_pad_input_poll_device(0);
	diff_state = old_state ^ state;
	button_was_pressed = old_state & diff_state;


	if(IS_TIMER_EXPIRED())
	{
		/* back to ROM menu if CIRCLE is pressed */
		if (CTRL_L1(button_was_pressed) || CTRL_CIRCLE(button_was_pressed))
		{
			menuStackindex--;
			old_state = state;
			return;
		}

		if (CTRL_R1(button_was_pressed))
		{
			switch(menu_obj->enum_id)
			{
				case GENERAL_VIDEO_MENU:
					menuStackindex++;
					menuStack[menuStackindex] = menu_generalaudiosettings;
					old_state = state;
					break;
				case GENERAL_AUDIO_MENU:
					menuStackindex++;
					menuStack[menuStackindex] = menu_emu_settings;
					old_state = state;
					break;
				case EMU_GENERAL_MENU:
					menuStackindex++;
					menuStack[menuStackindex] = menu_emu_videosettings;
					old_state = state;
					break;
				case EMU_VIDEO_MENU:
					menuStackindex++;
					menuStack[menuStackindex] = menu_emu_audiosettings;
					old_state = state;
					break;
				case EMU_AUDIO_MENU:
					menuStackindex++;
					menuStack[menuStackindex] = menu_pathsettings;
					old_state = state;
					break;
				case PATH_MENU:
					menuStackindex++;
					menuStack[menuStackindex] = menu_controlssettings;
					old_state = state;
					break;
				case CONTROLS_MENU:
					break;
			}
		}

		/* down to next setting */

		if (CTRL_DOWN(state) || CTRL_LSTICK_DOWN(state))
		{
			menu_obj->selected++;

			if (menu_obj->selected >= menu_obj->max_settings)
				menu_obj->selected = menu_obj->first_setting; 

			if (menu_obj->items[menu_obj->selected].page != menu_obj->page)
				menu_obj->page = menu_obj->items[menu_obj->selected].page;

			set_text_message("", 7);
		}

		/* up to previous setting */

		if (CTRL_UP(state) || CTRL_LSTICK_UP(state))
		{
			if (menu_obj->selected == menu_obj->first_setting)
				menu_obj->selected = menu_obj->max_settings-1;
			else
				menu_obj->selected--;

			if (menu_obj->items[menu_obj->selected].page != menu_obj->page)
				menu_obj->page = menu_obj->items[menu_obj->selected].page;

			set_text_message("", 7);
		}

		/* if a rom is loaded then resume it */

		if (CTRL_L3(state) && CTRL_R3(state))
		{
			if (g_console.emulator_initialized)
			{
				g_console.menu_enable = false;
				g_console.mode_switch = MODE_EMULATION;
				set_text_message("", 15);
			}
			old_state = state;
			return;
		}


		producesettingentry(menu_obj, menu_obj->selected);
	}

	display_menubar(menu_obj->enum_id);
	cellDbgFontDraw();

	for ( i = menu_obj->first_setting; i < menu_obj->max_settings; i++)
	{
		if(menu_obj->items[i].page == menu_obj->page)
		{
			cellDbgFontPuts(menu_obj->items[i].text_xpos, menu_obj->items[i].text_ypos, Emulator_GetFontSize(), menu_obj->selected == menu_obj->items[i].enum_id ? YELLOW : menu_obj->items[i].item_color, menu_obj->items[i].text);
			cellDbgFontPuts(0.5f, menu_obj->items[i].text_ypos, Emulator_GetFontSize(), menu_obj->items[i].text_color, menu_obj->items[i].setting_text);
			cellDbgFontDraw();
		}
	}

	cellDbgFontPuts(0.09f, menu_obj->items[menu_obj->selected].comment_ypos, 0.86f, LIGHTBLUE, menu_obj->items[menu_obj->selected].comment);

	cellDbgFontPuts(0.09f, 0.91f, Emulator_GetFontSize(), YELLOW, "UP/DOWN - select  L3+R3 - resume game   X/LEFT/RIGHT - change");
	cellDbgFontPuts(0.09f, 0.95f, Emulator_GetFontSize(), YELLOW, "START - default   L1/CIRCLE - go back   R1 - go forward");
	cellDbgFontDraw();
	old_state = state;
}

static void select_rom(void)
{
	char newpath[1024], *separatorslash;
	uint64_t state, diff_state, button_was_pressed;
	static uint64_t old_state = 0;

	state = cell_pad_input_poll_device(0);
	diff_state = old_state ^ state;
	button_was_pressed = old_state & diff_state;

	browser_update(&browser);

	if (CTRL_SELECT(button_was_pressed))
	{
		menuStackindex++;
		menuStack[menuStackindex] = menu_generalvideosettings;
	}

	if (CTRL_START(button_was_pressed))
		filebrowser_reset_start_directory(&browser, "/", ssnes_console_get_rom_ext());

	if (CTRL_CROSS(button_was_pressed))
	{
		if(FILEBROWSER_IS_CURRENT_A_DIRECTORY(browser))
		{
			/*if 'filename' is in fact '..' - then pop back directory 
			instead of adding '..' to filename path */

			if(browser.currently_selected == 0)
			{
				old_state = state;
				filebrowser_pop_directory(&browser);
			}
			else
			{
				separatorslash = (strcmp(FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(browser),"/") == 0) ? "" : "/";
				snprintf(newpath, sizeof(newpath), "%s%s%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(browser), separatorslash, FILEBROWSER_GET_CURRENT_FILENAME(browser));
				filebrowser_push_directory(&browser, newpath, true);
			}
		}
		else if (FILEBROWSER_IS_CURRENT_A_FILE(browser))
		{
			char rom_path_temp[MAX_PATH_LENGTH];

			snprintf(rom_path_temp, sizeof(rom_path_temp), "%s/%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(browser), FILEBROWSER_GET_CURRENT_FILENAME(browser));

			g_console.menu_enable = false;
			snprintf(g_console.rom_path, sizeof(g_console.rom_path), "%s/%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(browser), FILEBROWSER_GET_CURRENT_FILENAME(browser));
			g_console.initialize_ssnes_enable = 1;
			g_console.mode_switch = MODE_EMULATION;

			old_state = state;
			return;
		}
	}


	if (FILEBROWSER_IS_CURRENT_A_DIRECTORY(browser))
	{
		if(!strcmp(FILEBROWSER_GET_CURRENT_FILENAME(browser),"app_home") || !strcmp(FILEBROWSER_GET_CURRENT_FILENAME(browser),"host_root"))
			cellDbgFontPrintf(0.09f, 0.83f, 0.91f, RED, "WARNING - This path only works on DEX PS3 systems. Do not attempt to open\n this directory on CEX PS3 systems, or you might have to restart.");
		else if(!strcmp(FILEBROWSER_GET_CURRENT_FILENAME(browser),".."))
			cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - Press X to go back to the previous directory.");
		else
			cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - Press X to enter the directory.");
	}

	if (FILEBROWSER_IS_CURRENT_A_FILE(browser))
		cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - Press X to load the game. ");

	cellDbgFontPuts	(0.09f,	0.05f,	Emulator_GetFontSize(),	RED,	"FILE BROWSER");
	cellDbgFontPrintf (0.3f, 0.05f, 0.82f, WHITE, "Libsnes core: %s", snes_library_id());
	cellDbgFontPrintf (0.7f, 0.05f, 0.82f, WHITE, "%s v%s", EMULATOR_NAME, EMULATOR_VERSION);
	cellDbgFontPrintf (0.09f, 0.09f, Emulator_GetFontSize(), YELLOW,
	"PATH: %s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(browser));
	cellDbgFontPuts   (0.09f, 0.93f, Emulator_GetFontSize(), YELLOW,
	"L3 + R3 - resume game           SELECT - Settings screen");
	cellDbgFontDraw();

	browser_render(&browser);
	old_state = state;
}

#define MENU_ITEM_SELECTED(index) (menuitem_colors[index])

static void return_to_game (void)
{
	g_console.frame_advance_enable = false;
	g_console.ingame_menu_item = 0;
	g_console.menu_enable = false;
	g_console.mode_switch = MODE_EMULATION;
}

static void ingame_menu(uint32_t menu_id)
{
	char comment[256], msg_temp[256];
	static uint32_t menuitem_colors[MENU_ITEM_LAST];

	for(int i = 0; i < MENU_ITEM_LAST; i++)
		menuitem_colors[i] = GREEN;
	
	menuitem_colors[g_console.ingame_menu_item] = RED;

	uint64_t state = cell_pad_input_poll_device(0);
	static uint64_t old_state = 0;
	uint64_t stuck_in_loop = 1;
	static uint64_t blocking = 0;

	if(IS_TIMER_EXPIRED() && blocking == false)
	{
		if(CTRL_CIRCLE(state))
			return_to_game();

		switch(g_console.ingame_menu_item)
		{
			case MENU_ITEM_LOAD_STATE:
				if(CTRL_CROSS(state))
				{
					ssnes_load_state();
					return_to_game();
				}
				if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state))
				{
					if(g_extern.state_slot > 0)
					{
						char msg[512];
						g_extern.state_slot--;
						snprintf(msg, sizeof(msg), "Save state slot changed to: #%d", g_extern.state_slot);
						set_text_message(msg, 30);
					}
					blocking = 0;
				}
				if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state))
				{
					char msg[512];
					g_extern.state_slot++;
					snprintf(msg, sizeof(msg), "Save state slot changed to: #%d", g_extern.state_slot);
					set_text_message(msg, 30);
					blocking = 0;
				}

				strcpy(comment, "Press LEFT or RIGHT to change the current save state slot.\nPress CROSS to load the state from the currently selected save state slot.");
				break;
			case MENU_ITEM_SAVE_STATE:
				if(CTRL_CROSS(state))
				{
					ssnes_save_state();
					return_to_game();
				}
				if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state))
				{
					if(g_extern.state_slot > 0)
					{
						char msg[512];
						g_extern.state_slot--;
						snprintf(msg, sizeof(msg), "Save state slot changed to: #%d", g_extern.state_slot);
						set_text_message(msg, 30);
					}
					blocking = 0;
				}
				if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state))
				{
					char msg[512];
					g_extern.state_slot++;
					snprintf(msg, sizeof(msg), "Save state slot changed to: #%d", g_extern.state_slot);
					set_text_message(msg, 30);
					blocking = 0;
				}

				strcpy(comment, "Press LEFT or RIGHT to change the current save state slot.\nPress CROSS to save the state to the currently selected save state slot.");
				break;
			case MENU_ITEM_KEEP_ASPECT_RATIO:
				if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state))
				{
					if(g_console.aspect_ratio_index > 0)
					{
						g_console.aspect_ratio_index--;
						ps3graphics_set_aspect_ratio(g_console.aspect_ratio_index);
						set_text_message("", 7);
					}
				}
				if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state))
				{
					if(g_console.aspect_ratio_index < LAST_ASPECT_RATIO)
					{
						g_console.aspect_ratio_index++;
						ps3graphics_set_aspect_ratio(g_console.aspect_ratio_index);
						set_text_message("", 7);
					}
				}
				if(CTRL_START(state))
				{
					g_console.aspect_ratio_index = ASPECT_RATIO_4_3;
					ps3graphics_set_aspect_ratio(g_console.aspect_ratio_index);
				}
				strcpy(comment, "Press LEFT or RIGHT to change the [Aspect Ratio].\nPress START to reset back to default values.");
				break;
			case MENU_ITEM_OVERSCAN_AMOUNT:
				if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_CROSS(state) || CTRL_LSTICK_LEFT(state))
				{
					g_console.overscan_amount -= 0.01f;
					g_console.overscan_enable = true;

					if(g_console.overscan_amount == 0.00f)
						g_console.overscan_enable = false;

					ps3graphics_set_overscan(g_console.overscan_enable, g_console.overscan_amount, 1);
					set_text_message("", 7);
				}
				if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state) || CTRL_LSTICK_RIGHT(state))
				{
					g_console.overscan_amount += 0.01f;
					g_console.overscan_enable = true;
					if(g_console.overscan_amount == 0.0f)
						g_console.overscan_amount = false;

					ps3graphics_set_overscan(g_console.overscan_enable, g_console.overscan_amount, 1);
					set_text_message("", 7);
				}
				if(CTRL_START(state))
				{
					g_console.overscan_amount = 0.0f;
					g_console.overscan_enable = false;
					ps3graphics_set_overscan(g_console.overscan_enable, g_console.overscan_amount, 1);
				}
				strcpy(comment, "Press LEFT or RIGHT to change the [Overscan] settings.\nPress START to reset back to default values.");
				break;
			case MENU_ITEM_ORIENTATION:
				if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_CROSS(state) || CTRL_LSTICK_LEFT(state))
				{
					if(g_console.screen_orientation > ORIENTATION_NORMAL)
					{
						g_console.screen_orientation--;
						ps3graphics_set_orientation(g_console.screen_orientation);
						set_text_message("", 7);
					}
				}

				if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state) || CTRL_LSTICK_RIGHT(state))
				{
					if((g_console.screen_orientation+1) < ORIENTATION_END)
					{
						g_console.screen_orientation++;
						ps3graphics_set_orientation(g_console.screen_orientation);
						set_text_message("", 7);
					}
				}

				if(CTRL_START(state))
				{
					g_console.screen_orientation = ORIENTATION_NORMAL;
					ps3graphics_set_orientation(g_console.screen_orientation);
				}
				strcpy(comment, "Press LEFT or RIGHT to change the [Orientation] settings.\nPress START to reset back to default values.");
				break;
			case MENU_ITEM_FRAME_ADVANCE:
				if(CTRL_CROSS(state) || CTRL_R2(state) || CTRL_L2(state))
				{
					g_console.frame_advance_enable = true;
					g_console.ingame_menu_item = MENU_ITEM_FRAME_ADVANCE;
					g_console.menu_enable = false;
					g_console.mode_switch = MODE_EMULATION;
				}
				strcpy(comment, "Press 'CROSS', 'L2' or 'R2' button to step one frame. Pressing the button\nrapidly will advance the frame more slowly.");
				break;
			case MENU_ITEM_RESIZE_MODE:
				if(CTRL_CROSS(state))
				{
				}
				strcpy(comment, "Allows you to resize the screen by moving around the two analog sticks.\nPress TRIANGLE to reset to default values, and CIRCLE to go back.");
				break;
			case MENU_ITEM_SCREENSHOT_MODE:
				if(CTRL_CROSS(state))
				{
					while(stuck_in_loop && g_console.ingame_menu_enable)
					{
						state = cell_pad_input_poll_device(0);
						if(CTRL_CIRCLE(state))
						{
							set_text_message("", 7);
							stuck_in_loop = 0;
						}

						ssnes_render_cached_frame();

						video_gl.swap(NULL);
						old_state = state;
					}
				}

				strcpy(comment, "Allows you to take a screenshot without any text clutter.\nPress CIRCLE to go back to the in-game menu while in 'Screenshot Mode'.");
				break;
			case MENU_ITEM_RETURN_TO_GAME:
				if(CTRL_CROSS(state))
					return_to_game();

				strcpy(comment, "Press 'CROSS' to return back to the game.");
				break;
			case MENU_ITEM_RESET:
				if(CTRL_CROSS(state))
				{
					return_to_game();
					ssnes_game_reset();
				}
				strcpy(comment, "Press 'CROSS' to reset the game.");
				break;
			case MENU_ITEM_RETURN_TO_MENU:
				if(CTRL_CROSS(state))
				{
					g_console.ingame_menu_item = 0;
					g_console.menu_enable = false;
					g_console.mode_switch = MODE_MENU;
				}

				strcpy(comment, "Press 'CROSS' to return to the ROM Browser menu.");
				break;
#ifdef MULTIMAN_SUPPORT
			case MENU_ITEM_RETURN_TO_MULTIMAN:
				if(CTRL_CROSS(state))
				{
					g_console.menu_enable = false;
					g_console.mode_switch = MODE_EXIT;
				}

				strcpy(comment, "Press 'CROSS' to quit the emulator and return to multiMAN.");
				break;
#endif
			case MENU_ITEM_RETURN_TO_XMB:
				if(CTRL_CROSS(state))
				{
#ifdef MULTIMAN_SUPPORT
					return_to_MM = false;
#endif
					g_console.menu_enable = false;
					g_console.mode_switch = MODE_EXIT;
				}

				strcpy(comment, "Press 'CROSS' to quit the emulator and return to the XMB.");
				break;
		}

		if(CTRL_UP(state) || CTRL_LSTICK_UP(state))
		{
			if(g_console.ingame_menu_item > 0)
			{
				g_console.ingame_menu_item--;
				set_text_message("", 14);
			}
		}

		if(CTRL_DOWN(state) || CTRL_LSTICK_DOWN(state))
		{
			if(g_console.ingame_menu_item < (MENU_ITEM_LAST-1))
			{
				g_console.ingame_menu_item++;
				set_text_message("", 14);
			}
		}
	}

	float x_position = 0.3f;
	float font_size = 1.1f;
	float ypos = 0.19f;
	float ypos_increment = 0.04f;

	switch(g_console.screen_orientation)
	{
		case ORIENTATION_NORMAL:
			snprintf(msg_temp, sizeof(msg_temp), "Normal");
			break;
		case ORIENTATION_VERTICAL:
			snprintf(msg_temp, sizeof(msg_temp), "Vertical");
			break;
		case ORIENTATION_FLIPPED:
			snprintf(msg_temp, sizeof(msg_temp), "Flipped");
			break;
		case ORIENTATION_FLIPPED_ROTATED:
			snprintf(msg_temp, sizeof(msg_temp), "Flipped Rotated");
			break;
	}

	cellDbgFontPrintf(x_position, 0.14f, 1.4f, WHITE, "Quick Menu");

	cellDbgFontPrintf(x_position, ypos, font_size, MENU_ITEM_SELECTED(MENU_ITEM_LOAD_STATE), "Load State #%d", g_extern.state_slot);

	cellDbgFontPrintf(x_position, ypos+(ypos_increment*MENU_ITEM_SAVE_STATE), font_size, MENU_ITEM_SELECTED(MENU_ITEM_SAVE_STATE), "Save State #%d", g_extern.state_slot);
	cellDbgFontDraw();

	cellDbgFontPrintf(x_position, (ypos+(ypos_increment*MENU_ITEM_KEEP_ASPECT_RATIO)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_KEEP_ASPECT_RATIO), "Aspect Ratio: %s", g_console.aspect_ratio_name);

	cellDbgFontPrintf(x_position, (ypos+(ypos_increment*MENU_ITEM_OVERSCAN_AMOUNT)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_OVERSCAN_AMOUNT), "Overscan: %f", g_console.overscan_amount);

	cellDbgFontPrintf (x_position, (ypos+(ypos_increment*MENU_ITEM_ORIENTATION)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_ORIENTATION), "Orientation: %s", msg_temp);
	cellDbgFontDraw();

	cellDbgFontPrintf(x_position, (ypos+(ypos_increment*MENU_ITEM_RESIZE_MODE)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RESIZE_MODE), "Resize Mode");

	cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_FRAME_ADVANCE)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_FRAME_ADVANCE), "Frame Advance");

	cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_SCREENSHOT_MODE)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_SCREENSHOT_MODE), "Screenshot Mode");

	cellDbgFontDraw();

	cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_RESET)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RESET), "Reset");

	cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_RETURN_TO_GAME)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RETURN_TO_GAME), "Return to Game");

	cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_RETURN_TO_MENU)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RETURN_TO_MENU), "Return to Menu");
	cellDbgFontDraw();
#ifdef MULTIMAN_SUPPORT
	cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_RETURN_TO_MULTIMAN)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RETURN_TO_MULTIMAN), "Return to multiMAN");
#endif

	cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_RETURN_TO_XMB)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RETURN_TO_XMB), "Return to XMB");
	cellDbgFontDraw();

	cellDbgFontPuts	(0.09f,	0.05f,	Emulator_GetFontSize(),	RED,	"QUICK MENU");
	cellDbgFontPrintf (0.3f, 0.05f, 0.82f, WHITE, "Libsnes core: %s", snes_library_id());
	cellDbgFontPrintf (0.7f, 0.05f, 0.82f, WHITE, "%s v%s", EMULATOR_NAME, EMULATOR_VERSION);
	cellDbgFontDraw();
	if(IS_TIMER_NOT_EXPIRED())
	{
		cellDbgFontPrintf (0.05f, 0.90f, 1.10f, WHITE, special_action_msg);
		cellDbgFontDraw();
	}
	cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, comment);
	cellDbgFontDraw();
}

void menu_init (void)
{
	filebrowser_new(&browser, g_console.default_rom_startup_dir, ssnes_console_get_rom_ext());
}

void menu_loop(void)
{
	menuStack[0] = menu_filebrowser;
	menuStack[0].enum_id = FILE_BROWSER_MENU;

	g_console.menu_enable = true;

	gl_t * gl = g_gl;

	menu_reinit_settings();

	if(g_console.emulator_initialized)
		video_gl.set_swap_block_state(NULL, true);

	if(g_console.ingame_menu_enable)
	{
		menuStackindex++;
		menuStack[menuStackindex] = menu_filebrowser;
		menuStack[menuStackindex].enum_id = INGAME_MENU;
	}

	do
	{
		glClear(GL_COLOR_BUFFER_BIT);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		if(g_console.emulator_initialized)
			ssnes_render_cached_frame();

		gl_frame_menu();

		switch(menuStack[menuStackindex].enum_id)
		{
			case FILE_BROWSER_MENU:
				select_rom();
				break;
			case GENERAL_VIDEO_MENU:
			case GENERAL_AUDIO_MENU:
			case EMU_GENERAL_MENU:
			case EMU_VIDEO_MENU:
			case EMU_AUDIO_MENU:
			case PATH_MENU:
			case CONTROLS_MENU:
				select_setting(&menuStack[menuStackindex]);
				break;
			case GAME_AWARE_SHADER_CHOICE:
			case SHADER_CHOICE:
			case PRESET_CHOICE:
			case BORDER_CHOICE:
			case INPUT_PRESET_CHOICE:
				select_file(menuStack[menuStackindex].enum_id);
				break;
			case PATH_SAVESTATES_DIR_CHOICE:
			case PATH_DEFAULT_ROM_DIR_CHOICE:
			case PATH_CHEATS_DIR_CHOICE:
			case PATH_SRAM_DIR_CHOICE:
				select_directory(menuStack[menuStackindex].enum_id);
				break;
			case INGAME_MENU:
				if(g_console.ingame_menu_enable)
					ingame_menu(menuStack[menuStackindex].enum_id);
				break;
		}

		if(g_console.mode_switch == MODE_EMULATION && !g_console.frame_advance_enable)
		{
			SET_TIMER_EXPIRATION(30);
		}

		video_gl.swap(NULL);
		glDisable(GL_BLEND);
	}while (g_console.menu_enable);

	if(g_console.ingame_menu_enable)
		menuStackindex--;		// pop ingame menu from stack

	if(g_console.emulator_initialized)
		video_gl.set_swap_block_state(NULL, false);
	
	g_console.ingame_menu_enable = false;
}

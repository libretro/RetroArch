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

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <sys/process.h>
#include <cell/sysmodule.h>
#include <sysutil/sysutil_screenshot.h>
#include <sysutil/sysutil_common.h>
#include <sys/spu_initialize.h>
#include <sysutil/sysutil_gamecontent.h>

#include "ps3_input.h"
#include "ps3_video_psgl.h"

#include "../conf/config_file.h"
#include "../general.h"

#include "shared.h"

#include "menu.h"

#define MAX_PATH_LENGTH 1024

#define EMULATOR_CONTENT_DIR	"SSNE10000"

#define init_setting_uint(charstring, setting, defaultvalue) \
	if(!(config_get_uint(currentconfig, charstring, &setting))) \
		setting = defaultvalue; 

#define init_setting_int(charstring, setting, defaultvalue) \
	if(!(config_get_int(currentconfig, charstring, &setting))) \
		setting = defaultvalue; 

#define init_setting_float(charstring, setting, defaultvalue) \
	if(!(config_get_float(currentconfig, charstring, &setting))) \
		setting = defaultvalue; 

#define init_setting_bool(charstring, setting, defaultvalue) \
	if(!(config_get_bool(currentconfig, charstring, &setting))) \
		setting = defaultvalue; 

#define init_setting_bool(charstring, setting, defaultvalue) \
	if(!(config_get_bool(currentconfig, charstring, &setting))) \
		setting =	defaultvalue;

#define init_setting_char(charstring, setting, defaultvalue) \
	if(!(config_get_array(currentconfig, charstring, setting, sizeof(setting)))) \
		strncpy(setting,defaultvalue, sizeof(setting));


uint32_t g_emulator_initialized = 0;

char special_action_msg[256];		/* message which should be overlaid on top of the screen*/
uint32_t special_action_msg_expired;	/* time at which the message no longer needs to be overlaid onscreen*/
uint32_t mode_switch = MODE_MENU;
bool init_ssnes = false;
uint64_t ingame_menu_item = 0;

char contentInfoPath[MAX_PATH_LENGTH];
char usrDirPath[MAX_PATH_LENGTH];
char DEFAULT_PRESET_FILE[MAX_PATH_LENGTH];
char DEFAULT_BORDER_FILE[MAX_PATH_LENGTH];
char DEFAULT_MENU_BORDER_FILE[MAX_PATH_LENGTH];
char GAME_AWARE_SHADER_DIR_PATH[MAX_PATH_LENGTH];
char PRESETS_DIR_PATH[MAX_PATH_LENGTH];
char INPUT_PRESETS_DIR_PATH[MAX_PATH_LENGTH];
char BORDERS_DIR_PATH[MAX_PATH_LENGTH];
char SHADERS_DIR_PATH[MAX_PATH_LENGTH];
char DEFAULT_SHADER_FILE[MAX_PATH_LENGTH];
char DEFAULT_MENU_SHADER_FILE[MAX_PATH_LENGTH];
char SYS_CONFIG_FILE[MAX_PATH_LENGTH];
char MULTIMAN_GAME_TO_BOOT[MAX_PATH_LENGTH];

int ssnes_main(int argc, char *argv[]);

SYS_PROCESS_PARAM(1001, 0x100000)

#undef main

uint32_t set_text_message_speed(uint32_t value)
{
	return g_frame_count + value;
}

void set_text_message(const char * message, uint32_t speed)
{
	snprintf(special_action_msg, sizeof(special_action_msg), message);
	special_action_msg_expired = set_text_message_speed(speed);
}

static bool file_exists(const char * filename)
{
	CellFsStat sb;
	if(cellFsStat(filename,&sb) == CELL_FS_SUCCEEDED)
		return true;
	else
		return false;
}

static void init_settings(void)
{
	if(!file_exists(SYS_CONFIG_FILE))
	{
		FILE * f;
		f = fopen(SYS_CONFIG_FILE, "w");
		fclose(f);
	}

	config_file_t * currentconfig = config_file_new(SYS_CONFIG_FILE);

	init_setting_bool("video_smooth", g_settings.video.smooth, 1);
	init_setting_bool("video_second_pass_smooth", g_settings.video.second_pass_smooth, 1);
	init_setting_char("video_cg_shader", g_settings.video.cg_shader_path, DEFAULT_SHADER_FILE);
	init_setting_char("video_second_pass_shader", g_settings.video.second_pass_shader, DEFAULT_SHADER_FILE);
	init_setting_float("video_fbo_scale_x", g_settings.video.fbo_scale_x, 2.0f);
	init_setting_float("video_fbo_scale_y", g_settings.video.fbo_scale_y, 2.0f);
	init_setting_bool("video_render_to_texture", g_settings.video.render_to_texture, 1);
	init_setting_bool("video_vsync", g_settings.video.vsync, 1);
	init_setting_uint("state_slot",  g_extern.state_slot, 0);
	init_setting_uint("screenshots_enabled", g_console.screenshots_enable, 0);
	init_setting_char("cheat_database_path", g_settings.cheat_database, usrDirPath);
}

static void get_path_settings(bool multiman_support)
{
	unsigned int get_type;
	unsigned int get_attributes;
	CellGameContentSize size;
	char dirName[CELL_GAME_DIRNAME_SIZE];

	memset(&size, 0x00, sizeof(CellGameContentSize));

	int ret = cellGameBootCheck(&get_type, &get_attributes, &size, dirName); 
	if(ret < 0)
	{
		printf("cellGameBootCheck() Error: 0x%x\n", ret);
	}
	else
	{
		printf("cellGameBootCheck() OK\n");
		printf("  get_type = [%d] get_attributes = [0x%08x] dirName = [%s]\n", get_type, get_attributes, dirName);
		printf("  hddFreeSizeKB = [%d] sizeKB = [%d] sysSizeKB = [%d]\n", size.hddFreeSizeKB, size.sizeKB, size.sysSizeKB);

		ret = cellGameContentPermit(contentInfoPath, usrDirPath);

		if(multiman_support)
		{
			snprintf(contentInfoPath, sizeof(contentInfoPath), "/dev_hdd0/game/%s", EMULATOR_CONTENT_DIR);
			snprintf(usrDirPath, sizeof(usrDirPath), "/dev_hdd0/game/%s/USRDIR", EMULATOR_CONTENT_DIR);
		}

		if(ret < 0)
		{
			printf("cellGameContentPermit() Error: 0x%x\n", ret);
		}
		else
		{
			printf("cellGameContentPermit() OK\n");
			printf("contentInfoPath:[%s]\n", contentInfoPath);
			printf("usrDirPath:[%s]\n",  usrDirPath);
		}

		/* now we fill in all the variables */
		snprintf(DEFAULT_PRESET_FILE, sizeof(DEFAULT_PRESET_FILE), "%s/presets/stock.conf", usrDirPath);
		snprintf(DEFAULT_BORDER_FILE, sizeof(DEFAULT_BORDER_FILE), "%s/borders/Centered-1080p/mega-man-2.png", usrDirPath);
		snprintf(DEFAULT_MENU_BORDER_FILE, sizeof(DEFAULT_MENU_BORDER_FILE), "%s/borders/Menu/main-menu.png", usrDirPath);
		snprintf(GAME_AWARE_SHADER_DIR_PATH, sizeof(GAME_AWARE_SHADER_DIR_PATH), "%s/gameaware", usrDirPath);
		snprintf(PRESETS_DIR_PATH, sizeof(PRESETS_DIR_PATH), "%s/presets", usrDirPath); 
		snprintf(INPUT_PRESETS_DIR_PATH, sizeof(INPUT_PRESETS_DIR_PATH), "%s/input-presets", usrDirPath); 
		snprintf(BORDERS_DIR_PATH, sizeof(BORDERS_DIR_PATH), "%s/borders", usrDirPath); 
		snprintf(SHADERS_DIR_PATH, sizeof(SHADERS_DIR_PATH), "%s/shaders", usrDirPath);
		snprintf(DEFAULT_SHADER_FILE, sizeof(DEFAULT_SHADER_FILE), "%s/shaders/stock.cg", usrDirPath);
		snprintf(DEFAULT_MENU_SHADER_FILE, sizeof(DEFAULT_MENU_SHADER_FILE), "%s/shaders/Borders/Menu/border-only.cg", usrDirPath);
		snprintf(SYS_CONFIG_FILE, sizeof(SYS_CONFIG_FILE), "%s/ssnes.cfg", usrDirPath);
	}
}

static void callback_sysutil_exit(uint64_t status, uint64_t param, void *userdata)
{
	(void) param;
	(void) userdata;

	switch (status)
	{
		case CELL_SYSUTIL_REQUEST_EXITGAME:
			menu_is_running = 0;
			g_quitting = true;
			g_console.in_game_menu = false;
			mode_switch = MODE_EXIT;
			if(g_emulator_initialized)
				ssnes_main_deinit();
			break;
	}
}

#define ingame_menu_reset_entry_colors(ingame_menu_item) \
{ \
   for(int i = 0; i < MENU_ITEM_LAST; i++) \
      menuitem_colors[i] = GREEN; \
   menuitem_colors[ingame_menu_item] = RED; \
}

static void ingame_menu(void)
{
	uint32_t menuitem_colors[MENU_ITEM_LAST];
	char comment[256], msg_temp[256];

	do
	{
		uint64_t state = cell_pad_input_poll_device(0);
		static uint64_t old_state = 0;
		uint64_t stuck_in_loop = 1;
		const uint64_t button_was_pressed = old_state & (old_state ^ state);
		const uint64_t button_was_held = old_state & state;
		static uint64_t blocking = 0;

		if(g_frame_count < special_action_msg_expired && blocking)
		{
		}
		else
		{
			if(CTRL_CIRCLE(state))
			{
				ingame_menu_item = 0;
				g_console.in_game_menu = false;
				mode_switch = MODE_EMULATION;
			}

			switch(ingame_menu_item)
			{
				case MENU_ITEM_LOAD_STATE:
					if(CTRL_CROSS(button_was_pressed))
					{
					}
					if(CTRL_LEFT(button_was_pressed) || CTRL_LSTICK_LEFT(button_was_pressed))
					{
					}
					if(CTRL_RIGHT(button_was_pressed) || CTRL_LSTICK_RIGHT(button_was_pressed))
					{
					}

					ingame_menu_reset_entry_colors(ingame_menu_item);
					strcpy(comment, "Press LEFT or RIGHT to change the current save state slot.\nPress CROSS to load the state from the currently selected save state slot.");
					break;
				case MENU_ITEM_SAVE_STATE:
					if(CTRL_CROSS(button_was_pressed))
					{
					}
					if(CTRL_LEFT(button_was_pressed) || CTRL_LSTICK_LEFT(button_was_pressed))
					{
					}
					if(CTRL_RIGHT(button_was_pressed) || CTRL_LSTICK_RIGHT(button_was_pressed))
					{
					}

					ingame_menu_reset_entry_colors (ingame_menu_item);
					strcpy(comment, "Press LEFT or RIGHT to change the current save state slot.\nPress CROSS to save the state to the currently selected save state slot.");
					break;
				case MENU_ITEM_KEEP_ASPECT_RATIO:
					if(CTRL_LEFT(button_was_pressed) || CTRL_LSTICK_LEFT(button_was_pressed))
					{
					}
					if(CTRL_RIGHT(button_was_pressed)  || CTRL_LSTICK_RIGHT(button_was_pressed))
					{
					}
					if(CTRL_START(button_was_pressed))
					{
					}
					ingame_menu_reset_entry_colors (ingame_menu_item);
					strcpy(comment, "Press LEFT or RIGHT to change the [Aspect Ratio].\nPress START to reset back to default values.");
					break;
				case MENU_ITEM_OVERSCAN_AMOUNT:
					if(CTRL_LEFT(button_was_pressed)  ||  CTRL_LSTICK_LEFT(button_was_pressed) || CTRL_CROSS(button_was_pressed) || CTRL_LSTICK_LEFT(button_was_held))
					{
					}
					if(CTRL_RIGHT(button_was_pressed) || CTRL_LSTICK_RIGHT(button_was_pressed) || CTRL_CROSS(button_was_pressed) || CTRL_LSTICK_RIGHT(button_was_held))
					{
					}
					if(CTRL_START(button_was_pressed))
					{
					}
					ingame_menu_reset_entry_colors (ingame_menu_item);
					strcpy(comment, "Press LEFT or RIGHT to change the [Overscan] settings.\nPress START to reset back to default values.");
					break;
				case MENU_ITEM_ORIENTATION:
					if(CTRL_LEFT(button_was_pressed)  ||  CTRL_LSTICK_LEFT(button_was_pressed) || CTRL_CROSS(button_was_pressed) || CTRL_LSTICK_LEFT(button_was_held))
					{
					}

					if(CTRL_RIGHT(button_was_pressed) || CTRL_LSTICK_RIGHT(button_was_pressed) || CTRL_CROSS(button_was_pressed) || CTRL_LSTICK_RIGHT(button_was_held))
					{
					}

					if(CTRL_START(button_was_pressed))
					{
					}
					ingame_menu_reset_entry_colors (ingame_menu_item);
					strcpy(comment, "Press LEFT or RIGHT to change the [Orientation] settings.\nPress START to reset back to default values.");
					break;
				case MENU_ITEM_FRAME_ADVANCE:
					if(CTRL_CROSS(state) || CTRL_R2(state) || CTRL_L2(state))
					{
					}
					ingame_menu_reset_entry_colors (ingame_menu_item);
					strcpy(comment, "Press 'CROSS', 'L2' or 'R2' button to step one frame.\nNOTE: Pressing the button rapidly will advance the frame more slowly\nand prevent buttons from being input.");
					break;
				case MENU_ITEM_RESIZE_MODE:
					if(CTRL_CROSS(state))
					{
					}
					ingame_menu_reset_entry_colors (ingame_menu_item);
					strcpy(comment, "Allows you to resize the screen by moving around the two analog sticks.\nPress TRIANGLE to reset to default values, and CIRCLE to go back to the\nin-game menu.");
					break;
				case MENU_ITEM_SCREENSHOT_MODE:
					if(CTRL_CROSS(state))
					{
					}

					ingame_menu_reset_entry_colors (ingame_menu_item);
					strcpy(comment, "Allows you to take a screenshot without any text clutter.\nPress CIRCLE to go back to the in-game menu while in 'Screenshot Mode'.");
					break;
				case MENU_ITEM_RETURN_TO_GAME:
					if(CTRL_CROSS(button_was_pressed))
					{
						ingame_menu_item = 0;
						g_console.in_game_menu = false;
						mode_switch = MODE_EMULATION;
					}
					ingame_menu_reset_entry_colors (ingame_menu_item);
					strcpy(comment, "Press 'CROSS' to return back to the game.");
					break;
				case MENU_ITEM_RESET:
					if(CTRL_CROSS(button_was_pressed))
					{
						ingame_menu_item = 0;
						g_console.in_game_menu = false;
						mode_switch = MODE_EMULATION;
					}
					ingame_menu_reset_entry_colors (ingame_menu_item);
					strcpy(comment, "Press 'CROSS' to reset the game.");
					break;
				case MENU_ITEM_RETURN_TO_MENU:
					if(CTRL_CROSS(button_was_pressed))
					{
						ingame_menu_item = 0;
						g_console.in_game_menu = false;
						menu_is_running = 0;
						mode_switch = MODE_MENU;
					}

					ingame_menu_reset_entry_colors (ingame_menu_item);
					strcpy(comment, "Press 'CROSS' to return to the ROM Browser menu.");
					break;
#ifdef MULTIMAN_SUPPORT
				case MENU_ITEM_RETURN_TO_MULTIMAN:
					if(CTRL_CROSS(button_was_pressed))
					{
						g_console.in_game_menu = false;
						mode_switch = MODE_EXIT; 
					}

					ingame_menu_reset_entry_colors (ingame_menu_item);
					strcpy(comment, "Press 'CROSS' to quit the emulator and return to multiMAN.");
					break;
#endif
				case MENU_ITEM_RETURN_TO_XMB:
					if(CTRL_CROSS(button_was_pressed))
					{
						g_console.in_game_menu = false;
#ifdef MULTIMAN_SUPPORT
						return_to_MM = false;
#endif
						mode_switch = MODE_EXIT; 
					}

					ingame_menu_reset_entry_colors (ingame_menu_item);
					strcpy(comment, "Press 'CROSS' to quit the emulator and return to the XMB.");
					break;
			}

			if(CTRL_UP(button_was_pressed) || CTRL_LSTICK_UP(button_was_pressed))
			{
				if(ingame_menu_item > 0)
					ingame_menu_item--;
			}

			if(CTRL_DOWN(button_was_pressed) || CTRL_LSTICK_DOWN(button_was_pressed))
			{
				if(ingame_menu_item < MENU_ITEM_LAST)
					ingame_menu_item++;
			}
		}

		float x_position = 0.3f;
		float font_size = 1.1f;
		float ypos = 0.19f;
		float ypos_increment = 0.04f;

		cellDbgFontPrintf	(x_position,	0.10f,	1.4f+0.01f,	BLUE,               "Quick Menu");
		cellDbgFontPrintf(x_position,	0.10f,	1.4f,	WHITE,               "Quick Menu");

		cellDbgFontPrintf	(x_position,	ypos,	font_size+0.01f,	BLUE,	"Load State #%d", g_extern.state_slot);
		cellDbgFontPrintf(x_position,	ypos,	font_size,	menuitem_colors[MENU_ITEM_LOAD_STATE],	"Load State #%d", g_extern.state_slot);

		cellDbgFontPrintf	(x_position,	ypos+(ypos_increment*MENU_ITEM_SAVE_STATE),	font_size+0.01f,	BLUE,	"Save State #%d", g_extern.state_slot);
		cellDbgFontPrintf(x_position,	ypos+(ypos_increment*MENU_ITEM_SAVE_STATE),	font_size,	menuitem_colors[MENU_ITEM_SAVE_STATE],	"Save State #%d", g_extern.state_slot);
		cellDbgFontDraw();

		cellDbgFontPrintf	(x_position,	(ypos+(ypos_increment*MENU_ITEM_KEEP_ASPECT_RATIO)),	font_size+0.01f,	BLUE,	"Aspect Ratio: ");
		cellDbgFontPrintf(x_position,	(ypos+(ypos_increment*MENU_ITEM_KEEP_ASPECT_RATIO)),	font_size,	menuitem_colors[MENU_ITEM_KEEP_ASPECT_RATIO],	"Aspect Ratio:");

		cellDbgFontPrintf(x_position,	(ypos+(ypos_increment*MENU_ITEM_OVERSCAN_AMOUNT)),	font_size,	menuitem_colors[MENU_ITEM_OVERSCAN_AMOUNT],	"Overscan: ");

		cellDbgFontPrintf	(x_position,	(ypos+(ypos_increment*MENU_ITEM_ORIENTATION)),	font_size+0.01f,	BLUE,	"Orientation: ");
		cellDbgFontPrintf	(x_position,	(ypos+(ypos_increment*MENU_ITEM_ORIENTATION)),	font_size,	menuitem_colors[MENU_ITEM_ORIENTATION],	"Orientation:");

		cellDbgFontPrintf	(x_position,	(ypos+(ypos_increment*MENU_ITEM_RESIZE_MODE)),	font_size+0.01f,	BLUE,	"Resize Mode");
		cellDbgFontPrintf(x_position,	(ypos+(ypos_increment*MENU_ITEM_RESIZE_MODE)),	font_size,	menuitem_colors[MENU_ITEM_RESIZE_MODE],	"Resize Mode");

		cellDbgFontPuts	(x_position,	(ypos+(ypos_increment*MENU_ITEM_FRAME_ADVANCE)),	font_size+0.01f,	BLUE,	"Frame Advance");
		cellDbgFontPuts(x_position,	(ypos+(ypos_increment*MENU_ITEM_FRAME_ADVANCE)),	font_size,	menuitem_colors[MENU_ITEM_FRAME_ADVANCE],	"Frame Advance");

		cellDbgFontPuts	(x_position,	(ypos+(ypos_increment*MENU_ITEM_SCREENSHOT_MODE)),	font_size+0.01f,	BLUE,	"Screenshot Mode");
		cellDbgFontPuts(x_position,	(ypos+(ypos_increment*MENU_ITEM_SCREENSHOT_MODE)),	font_size,	menuitem_colors[MENU_ITEM_SCREENSHOT_MODE],	"Screenshot Mode");

		cellDbgFontDraw();

		cellDbgFontPuts	(x_position, (ypos+(ypos_increment*MENU_ITEM_RESET)), font_size+0.01f, BLUE, "Reset");
		cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_RESET)), font_size, menuitem_colors[MENU_ITEM_RESET],	"Reset");

		cellDbgFontPuts   (x_position,   (ypos+(ypos_increment*MENU_ITEM_RETURN_TO_GAME)),   font_size+0.01f,  BLUE,  "Return to Game");
		cellDbgFontPuts(x_position,   (ypos+(ypos_increment*MENU_ITEM_RETURN_TO_GAME)),   font_size,  menuitem_colors[MENU_ITEM_RETURN_TO_GAME],  "Return to Game");

		cellDbgFontPuts	(x_position,	(ypos+(ypos_increment*MENU_ITEM_RETURN_TO_MENU)),	font_size+0.01f,	BLUE,	"Return to Menu");
		cellDbgFontPuts(x_position,	(ypos+(ypos_increment*MENU_ITEM_RETURN_TO_MENU)),	font_size,	menuitem_colors[MENU_ITEM_RETURN_TO_MENU],	"Return to Menu");
#ifdef MULTIMAN_SUPPORT
		cellDbgFontPuts	(x_position,	(ypos+(ypos_increment*MENU_ITEM_RETURN_TO_MULTIMAN)),	font_size+0.01f,	BLUE,	"Return to multiMAN");
		cellDbgFontPuts(x_position,	(ypos+(ypos_increment*MENU_ITEM_RETURN_TO_MULTIMAN)),	font_size,	menuitem_colors[MENU_ITEM_RETURN_TO_MULTIMAN],	"Return to multiMAN");
#endif
		cellDbgFontDraw();

		cellDbgFontPuts	(x_position,	(ypos+(ypos_increment*MENU_ITEM_RETURN_TO_XMB)),	font_size+0.01f,	BLUE,	"Return to XMB");
		cellDbgFontPuts(x_position,	(ypos+(ypos_increment*MENU_ITEM_RETURN_TO_XMB)),	font_size,	menuitem_colors[MENU_ITEM_RETURN_TO_XMB],	"Return to XMB");

		if(g_frame_count < special_action_msg_expired)
		{
			cellDbgFontPrintf (0.09f, 0.90f, 1.51f, BLUE,	special_action_msg);
			cellDbgFontPrintf (0.09f, 0.90f, 1.50f, WHITE,	special_action_msg);
			cellDbgFontDraw();
		}
		else
		{
			special_action_msg_expired = 0;
			cellDbgFontPrintf (0.09f,   0.90f,   0.98f+0.01f,      BLUE,           comment);
			cellDbgFontPrintf (0.09f,   0.90f,   0.98f,      LIGHTBLUE,           comment);
		}
		cellDbgFontDraw();
		psglSwap();
		old_state = state;
		cellSysutilCheckCallback();
	}while(g_console.in_game_menu);
}

// Temporary, a more sane implementation should go here.
int main(int argc, char *argv[])
{
   // Initialize 6 SPUs but reserve 1 SPU as a raw SPU for PSGL
   sys_spu_initialize(6, 1);

   cellSysmoduleLoadModule(CELL_SYSMODULE_FS);
   cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_GAME);

   memset(&g_extern, 0, sizeof(g_extern));
   memset(&g_settings, 0, sizeof(g_settings));
   memset(&g_console, 0, sizeof(g_console));

   SSNES_LOG("Registering Callback\n");
   cellSysutilRegisterCallback(0, callback_sysutil_exit, NULL);

#ifdef MULTIMAN_SUPPORT
   g_console.return_to_multiman_enable = true;

   if(argc > 1)
   {
	   strncpy(MULTIMAN_GAME_TO_BOOT, argv[1], sizeof(MULTIMAN_GAME_TO_BOOT));
   }
#else
   g_console.return_to_multiman_enable = false;
#endif

   get_path_settings(g_console.return_to_multiman_enable);
   init_settings();

#if(CELL_SDK_VERSION > 0x340000)
	if (g_console.screenshots_enable)
	{
		cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_SCREENSHOT);
		CellScreenShotSetParam  screenshot_param = {0, 0, 0, 0};

		screenshot_param.photo_title = "SSNES PS3";
		screenshot_param.game_title = "SSNES PS3";
		cellScreenShotSetParameter (&screenshot_param);
		cellScreenShotEnable();
	}
#endif

   ps3_video_init();
   ps3_input_init();

   menu_init();

begin_loop:
   if(mode_switch == MODE_EMULATION)
   {
	input_ps3.poll(NULL);
   	while(ssnes_main_iterate());
	if(g_console.in_game_menu)
		ingame_menu();
   }
   else if(mode_switch == MODE_MENU)
   {
	   menu_loop();
	   if(init_ssnes)
	   {
		   if(g_emulator_initialized)
			   ssnes_main_deinit();

		   char arg1[] = "ssnes";
		   char arg2[PATH_MAX];

		   snprintf(arg2, sizeof(arg2), g_console.rom_path);
		   char arg3[] = "-v";
		   char arg4[] = "-c";
		   char arg5[MAX_PATH_LENGTH];

		   snprintf(arg5, sizeof(arg5), SYS_CONFIG_FILE);
		   char *argv_[] = { arg1, arg2, arg3, arg4, arg5, NULL };

		   int argc = sizeof(argv_) / sizeof(argv_[0]) - 1;
		   int init_ret = ssnes_main_init(argc, argv_);
		   g_emulator_initialized = 1;
		   init_ssnes = 0;
	   }
   }
#ifdef MULTIMAN_SUPPORT
   else if(mode_switch == MODE_MULTIMAN_STARTUP)
   {
   }
#endif
   else
	   goto begin_shutdown;

   goto begin_loop;

begin_shutdown:
   ps3_input_deinit();
   ps3_video_deinit();
   sys_process_exit(0);
}

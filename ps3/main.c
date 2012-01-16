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

bool g_rom_loaded;
bool return_to_MM;			/* launch multiMAN on exit if ROM is passed*/
uint32_t g_emulator_initialized = 0;

char special_action_msg[256];		/* message which should be overlaid on top of the screen*/
uint32_t special_action_msg_expired;	/* time at which the message no longer needs to be overlaid onscreen*/

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
			sys_process_exit(0);
			break;
	}
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

   g_rom_loaded = false;
#ifdef MULTIMAN_SUPPORT
   return_to_MM = true;

   if(argc > 1)
   {
	   strncpy(MULTIMAN_GAME_TO_BOOT, argv[1], sizeof(MULTIMAN_GAME_TO_BOOT));
   }
#else
   return_to_MM = false;
#endif

   get_path_settings(return_to_MM);
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
   menu_loop();

   char arg1[] = "ssnes";
   char arg2[PATH_MAX];
   
   snprintf(arg2, sizeof(arg2), g_extern.system.fullpath);
   char arg3[] = "-v";
   char arg4[] = "-c";
   char arg5[MAX_PATH_LENGTH];

   snprintf(arg5, sizeof(arg5), SYS_CONFIG_FILE);
   char *argv_[] = { arg1, arg2, arg3, arg4, arg5, NULL };

   g_emulator_initialized = 1;

   return ssnes_main(sizeof(argv_) / sizeof(argv_[0]) - 1, argv_);

   ps3_input_deinit();
   ps3_video_deinit();
}

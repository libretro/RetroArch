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

#include <xtl.h>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <xbdm.h>
#include "menu.h"
#include "xdk360_video.h"

#include "../console/main_wrap.h"
#include "../conf/config_file.h"
#include "../conf/config_file_macros.h"
#include "../file.h"
#include "../general.h"
#include "shared.h"

#define DEVICE_MEMORY_UNIT0 1
#define DEVICE_MEMORY_UNIT1 2
#define DEVICE_MEMORY_ONBOARD 3
#define DEVICE_CDROM0 4
#define DEVICE_HARDISK0_PART1 5
#define DEVICE_HARDISK0_SYSPART 6
#define DEVICE_USB0 7
#define DEVICE_USB1 8
#define DEVICE_USB2 9
#define DEVICE_TEST 10
#define DEVICE_CACHE 11

typedef struct _STRING {
	unsigned short Length;
	unsigned short MaximumLength;
	char * Buffer;
} STRING;

char SYS_CONFIG_FILE[MAX_PATH_LENGTH];

extern "C" int __stdcall ObCreateSymbolicLink( STRING*, STRING*);

int Mounted[20];

int ssnes_main(int argc, char *argv[]);

#undef main

static int DriveMounted(std::string path)
{
	WIN32_FIND_DATA findFileData;
	memset(&findFileData,0,sizeof(WIN32_FIND_DATA));
	std::string searchcmd = path + "\\*.*";
	HANDLE hFind = FindFirstFile(searchcmd.c_str(), &findFileData);

	if (hFind == INVALID_HANDLE_VALUE)
		return 0;

	FindClose(hFind);

	return 1;
}


static int Mount( int Device, char* MountPoint )
{
	char MountConv[260];
	sprintf_s( MountConv,"\\??\\%s", MountPoint );

	char * SysPath = NULL;
	switch( Device )
	{
		case DEVICE_MEMORY_UNIT0:
			SysPath = "\\Device\\Mu0";
			break;
		case DEVICE_MEMORY_UNIT1:
			SysPath = "\\Device\\Mu1";
			break;
		case DEVICE_MEMORY_ONBOARD:
			SysPath = "\\Device\\BuiltInMuSfc";
			break;
		case DEVICE_CDROM0:
			SysPath = "\\Device\\Cdrom0";
			break;
		case DEVICE_HARDISK0_PART1:
			SysPath = "\\Device\\Harddisk0\\Partition1";
			break;
		case DEVICE_HARDISK0_SYSPART:
			SysPath = "\\Device\\Harddisk0\\SystemPartition";
			break;
		case DEVICE_USB0:
			SysPath = "\\Device\\Mass0";
			break;
		case DEVICE_USB1:
			SysPath = "\\Device\\Mass1";
			break;
		case DEVICE_USB2:
			SysPath = "\\Device\\Mass2";
			break;
		case DEVICE_CACHE:
			SysPath = "\\Device\\Harddisk0\\Cache0";
			break;

	}

	STRING sSysPath = { (USHORT)strlen( SysPath ), (USHORT)strlen( SysPath ) + 1, SysPath };
	STRING sMountConv = { (USHORT)strlen( MountConv ), (USHORT)strlen( MountConv ) + 1, MountConv };
	int res = ObCreateSymbolicLink( &sMountConv, &sSysPath );

	if (res != 0)
		return res;

	return DriveMounted(MountPoint);
}

static void set_default_settings (void)
{
	//g_settings
	g_settings.rewind_enable = false;
	g_settings.video.vsync = true;
	g_settings.video.smooth = true;

	for(uint32_t x = 0; x < MAX_PLAYERS; x++)
	{
		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_B].id = SNES_DEVICE_ID_JOYPAD_B;
		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_B].joykey = XINPUT_GAMEPAD_A;

		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_Y].id = SNES_DEVICE_ID_JOYPAD_Y;
		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_Y].joykey = XINPUT_GAMEPAD_X;

		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_SELECT].id = SNES_DEVICE_ID_JOYPAD_SELECT;
		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_SELECT].joykey = XINPUT_GAMEPAD_BACK;

		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_START].id = SNES_DEVICE_ID_JOYPAD_START;
		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_START].joykey = XINPUT_GAMEPAD_START;

		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_UP].id = SNES_DEVICE_ID_JOYPAD_UP;
		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_UP].joykey = XINPUT_GAMEPAD_DPAD_UP;
		//g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_UP].joyaxis = CTRL_LSTICK_UP_MASK;

		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_DOWN].id = SNES_DEVICE_ID_JOYPAD_DOWN;
		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_DOWN].joykey = XINPUT_GAMEPAD_DPAD_DOWN;
		//g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_DOWN].joyaxis = CTRL_LSTICK_DOWN_MASK;

		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_LEFT].id = SNES_DEVICE_ID_JOYPAD_LEFT;
		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_LEFT].joykey = XINPUT_GAMEPAD_DPAD_LEFT;
		//g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_LEFT].joyaxis = CTRL_LSTICK_LEFT_MASK;

		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_RIGHT].id = SNES_DEVICE_ID_JOYPAD_RIGHT;
		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_RIGHT].joykey = XINPUT_GAMEPAD_DPAD_RIGHT;
		//g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_RIGHT].joyaxis = CTRL_LSTICK_RIGHT_MASK;

		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_A].id = SNES_DEVICE_ID_JOYPAD_A;
		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_A].joykey = XINPUT_GAMEPAD_B;

		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_X].id = SNES_DEVICE_ID_JOYPAD_X;
		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_X].joykey = XINPUT_GAMEPAD_Y;

		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_L].id = SNES_DEVICE_ID_JOYPAD_L;
		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_L].joykey = XINPUT_GAMEPAD_LEFT_SHOULDER;

		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_R].id = SNES_DEVICE_ID_JOYPAD_R;
		g_settings.input.binds[x][SNES_DEVICE_ID_JOYPAD_R].joykey = XINPUT_GAMEPAD_RIGHT_SHOULDER;
	}


	//g_console
	g_console.block_config_read = true;
	g_console.throttle_enable = true;
	g_console.initialize_ssnes_enable = false;
	g_console.emulator_initialized = 0;
	g_console.mode_switch = MODE_MENU;
	strlcpy(g_console.default_rom_startup_dir, "game:\\roms\\", sizeof(g_console.default_rom_startup_dir));

	//g_extern
	g_extern.state_slot = 0;
	g_extern.audio_data.mute = 0;
	g_extern.verbose = true;
}

static void init_settings (void)
{
	if(!path_file_exists(SYS_CONFIG_FILE))
	{
		SSNES_ERR("Config file \"%s\" desn't exist. Creating...\n", "game:\\ssnes.cfg");
		FILE * f;
		f = fopen(SYS_CONFIG_FILE, "w");
		fclose(f);
	}

	config_file_t * conf = config_file_new(SYS_CONFIG_FILE);

	// g_settings
	CONFIG_GET_BOOL(rewind_enable, "rewind_enable");
	CONFIG_GET_BOOL(video.smooth, "video_smooth");
	CONFIG_GET_BOOL(video.vsync, "video_vsync");

	// g_console
	CONFIG_GET_BOOL_CONSOLE(throttle_enable, "throttle_enable");
	CONFIG_GET_STRING_CONSOLE(default_rom_startup_dir, "default_rom_startup_dir");

	// g_extern
	CONFIG_GET_INT_EXTERN(state_slot, "state_slot");
	CONFIG_GET_INT_EXTERN(audio_data.mute, "audio_mute");
}

static void save_settings (void)
{
	if(!path_file_exists(SYS_CONFIG_FILE))
	{
		FILE * f;
		f = fopen(SYS_CONFIG_FILE, "w");
		fclose(f);
	}

	config_file_t * conf = config_file_new(SYS_CONFIG_FILE);

	if(conf == NULL)
			conf = config_file_new(NULL);

	// g_settings
	config_set_bool(conf, "rewind_enable", g_settings.rewind_enable);
	config_set_bool(conf, "video_smooth", g_settings.video.smooth);
	config_set_bool(conf, "video_vsync", g_settings.video.vsync);

	// g_console
	config_set_string(conf, "default_rom_startup_dir", g_console.default_rom_startup_dir);
	config_set_bool(conf, "throttle_enable", g_console.throttle_enable);

	// g_extern
	config_set_int(conf, "state_slot", g_extern.state_slot);
	config_set_int(conf, "audio_mute", g_extern.audio_data.mute);

	if (!config_file_write(conf, SYS_CONFIG_FILE))
			SSNES_ERR("Failed to write config file to \"%s\". Check permissions.\n", SYS_CONFIG_FILE);

	free(conf);
}

static void get_environment_settings (void)
{
	//for devkits only, we will need to mount all partitions for retail
	//in a different way
	//DmMapDevkitDrive();
	
	memset(&Mounted, 0, 20);

 	Mounted[DEVICE_USB0] = Mount(DEVICE_USB0,"Usb0:");
 	Mounted[DEVICE_USB1] = Mount(DEVICE_USB1,"Usb1:");
 	Mounted[DEVICE_USB2] = Mount(DEVICE_USB2,"Usb2:");
 	Mounted[DEVICE_HARDISK0_PART1] = Mount(DEVICE_HARDISK0_PART1,"Hdd1:");
 	Mounted[DEVICE_HARDISK0_SYSPART] = Mount(DEVICE_HARDISK0_SYSPART,"HddX:");
 	Mounted[DEVICE_MEMORY_UNIT0] = Mount(DEVICE_MEMORY_UNIT0,"Memunit0:");
 	Mounted[DEVICE_MEMORY_UNIT1] = Mount(DEVICE_MEMORY_UNIT1,"Memunit1:");
	Mounted[DEVICE_MEMORY_ONBOARD] = Mount(DEVICE_MEMORY_ONBOARD,"OnBoardMU:"); 
	Mounted[DEVICE_CDROM0] = Mount(DEVICE_CDROM0,"Dvd:"); 

	int result_filecache = XSetFileCacheSize(0x100000);

	if(result_filecache != TRUE)
	{
		SSNES_ERR("Couldn't change number of bytes reserved for file system cache.\n");
	}
	unsigned long result = XMountUtilityDriveEx(XMOUNTUTILITYDRIVE_FORMAT0,8192, 0);

	if(result != ERROR_SUCCESS)
	{
		SSNES_ERR("Couldn't mount/format utility drive.\n");
	}

	// detect install environment
	unsigned long license_mask;

	if (XContentGetLicenseMask(&license_mask, NULL) != ERROR_SUCCESS)
	{
		printf("SSNES was launched as a standalone DVD, or using DVD emulation, or from the development area of the HDD.\n");
	}
	else
	{
		XContentQueryVolumeDeviceType("GAME",&g_console.volume_device_type, NULL);

		switch(g_console.volume_device_type)
		{
			case XCONTENTDEVICETYPE_HDD:
				printf("SSNES was launched from a content package on HDD.\n");
				break;
			case XCONTENTDEVICETYPE_MU:
				printf("SSNES was launched from a content package on USB or Memory Unit.\n");
				break;
			case XCONTENTDEVICETYPE_ODD:
				printf("SSNES was launched from a content package on Optical Disc Drive.\n");
				break;
			default:
				printf("SSNES was launched from a content package on an unknown device type.\n");
				break;

		}
	}

	strlcpy(SYS_CONFIG_FILE, "game:\\ssnes.cfg", sizeof(SYS_CONFIG_FILE));
}

int main(int argc, char *argv[])
{
	get_environment_settings();

	ssnes_main_clear_state();
	config_set_defaults();

	set_default_settings();
	init_settings();

	xdk360_video_init();
	menu_init();

begin_loop:
	if(g_console.mode_switch == MODE_EMULATION)
	{
		bool repeat = false;

		input_xdk360.poll(NULL);

		do{
			repeat = ssnes_main_iterate();
		}while(repeat && !g_console.frame_advance_enable);
	}
	else if(g_console.mode_switch == MODE_MENU)
	{
		menu_loop();

		if(g_console.initialize_ssnes_enable)
		{
			if(g_console.emulator_initialized)
				ssnes_main_deinit();

			struct ssnes_main_wrap args = {0};

			args.verbose = g_extern.verbose;
			args.config_path = SYS_CONFIG_FILE;
			args.rom_path = g_console.rom_path;
			
			int init_ret = ssnes_main_init_wrap(&args);
			g_console.emulator_initialized = 1;
			g_console.initialize_ssnes_enable = 0;
		}
	}
	else
			goto begin_shutdown;

	goto begin_loop;

begin_shutdown:
	if(path_file_exists(SYS_CONFIG_FILE))
		save_settings();
	xdk360_video_deinit();
}


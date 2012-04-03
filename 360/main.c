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

#include <xtl.h>
#include <xfilecache.h>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <xbdm.h>
#include "menu.h"
#include "xdk360_input.h"
#include "xdk360_video.h"

#include "../console/console_ext.h"
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

char DEFAULT_SHADER_FILE[MAX_PATH_LENGTH];
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
	strlcpy(g_settings.video.cg_shader_path, DEFAULT_SHADER_FILE, sizeof(g_settings.video.cg_shader_path));
	g_settings.video.vsync = true;
	g_settings.video.smooth = true;
	g_settings.video.aspect_ratio = -1.0f;

	ssnes_default_keybind_lut[SNES_DEVICE_ID_JOYPAD_B]		=	ssnes_platform_keybind_lut[XDK360_DEVICE_ID_JOYPAD_A];
	ssnes_default_keybind_lut[SNES_DEVICE_ID_JOYPAD_Y]		=	ssnes_platform_keybind_lut[XDK360_DEVICE_ID_JOYPAD_X];
	ssnes_default_keybind_lut[SNES_DEVICE_ID_JOYPAD_SELECT]	=	ssnes_platform_keybind_lut[XDK360_DEVICE_ID_JOYPAD_BACK];
	ssnes_default_keybind_lut[SNES_DEVICE_ID_JOYPAD_START]	=	ssnes_platform_keybind_lut[XDK360_DEVICE_ID_JOYPAD_START];
	ssnes_default_keybind_lut[SNES_DEVICE_ID_JOYPAD_UP]		=	ssnes_platform_keybind_lut[XDK360_DEVICE_ID_JOYPAD_UP];
	ssnes_default_keybind_lut[SNES_DEVICE_ID_JOYPAD_DOWN]		=	ssnes_platform_keybind_lut[XDK360_DEVICE_ID_JOYPAD_DOWN];
	ssnes_default_keybind_lut[SNES_DEVICE_ID_JOYPAD_LEFT]		=	ssnes_platform_keybind_lut[XDK360_DEVICE_ID_JOYPAD_LEFT];
	ssnes_default_keybind_lut[SNES_DEVICE_ID_JOYPAD_RIGHT]	=	ssnes_platform_keybind_lut[XDK360_DEVICE_ID_JOYPAD_RIGHT];
	ssnes_default_keybind_lut[SNES_DEVICE_ID_JOYPAD_A]		=	ssnes_platform_keybind_lut[XDK360_DEVICE_ID_JOYPAD_B];
	ssnes_default_keybind_lut[SNES_DEVICE_ID_JOYPAD_X]		=	ssnes_platform_keybind_lut[XDK360_DEVICE_ID_JOYPAD_Y];
	ssnes_default_keybind_lut[SNES_DEVICE_ID_JOYPAD_L]		=	ssnes_platform_keybind_lut[XDK360_DEVICE_ID_JOYPAD_LB];
	ssnes_default_keybind_lut[SNES_DEVICE_ID_JOYPAD_R]		=	ssnes_platform_keybind_lut[XDK360_DEVICE_ID_JOYPAD_RB];

	for(uint32_t x = 0; x < MAX_PLAYERS; x++)
	{
		for(uint32_t y = 0; y < SSNES_FIRST_META_KEY; y++)
		{
			g_settings.input.binds[x][y].id = y;
			g_settings.input.binds[x][y].joykey = ssnes_default_keybind_lut[y];
		}
		g_settings.input.dpad_emulation[x] = DPAD_EMULATION_LSTICK;
	}

	//g_console
	g_console.block_config_read = true;
	g_console.gamma_correction_enable = false;
	g_console.initialize_ssnes_enable = false;
	g_console.emulator_initialized = 0;
	g_console.mode_switch = MODE_MENU;
	g_console.screen_orientation = ORIENTATION_NORMAL;
	g_console.throttle_enable = true;
	g_console.aspect_ratio_index = 0;
	strlcpy(g_console.aspect_ratio_name, "4:3", sizeof(g_console.aspect_ratio_name));
	strlcpy(g_console.default_rom_startup_dir, "game:", sizeof(g_console.default_rom_startup_dir));
	g_console.custom_viewport_width = 0;
	g_console.custom_viewport_height = 0;
	g_console.custom_viewport_x = 0;
	g_console.custom_viewport_y = 0;

	//g_extern
	g_extern.state_slot = 0;
	g_extern.audio_data.mute = 0;
	g_extern.verbose = true;
}

static char **dir_list_new_360(const char *dir, const char *ext)
{
	size_t cur_ptr = 0;
	size_t cur_size = 32;
	char **dir_list = NULL;

	WIN32_FIND_DATA ffd;
	HANDLE hFind = INVALID_HANDLE_VALUE;

	char path_buf[PATH_MAX];

	if (strlcpy(path_buf, dir, sizeof(path_buf)) >= sizeof(path_buf))
	goto error;
	if (strlcat(path_buf, "*", sizeof(path_buf)) >= sizeof(path_buf))
	goto error;

	if (ext)
	{
	if (strlcat(path_buf, ext, sizeof(path_buf)) >= sizeof(path_buf))
		goto error;
	}

	hFind = FindFirstFile(path_buf, &ffd);
	if (hFind == INVALID_HANDLE_VALUE)
	goto error;

	dir_list = (char**)calloc(cur_size, sizeof(char*));
	if (!dir_list)
	goto error;

	do
	{
	// Not a perfect search of course, but hopefully good enough in practice.
	if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		continue;
	if (ext && !strstr(ffd.cFileName, ext))
		continue;

	dir_list[cur_ptr] = (char*)malloc(PATH_MAX);
	if (!dir_list[cur_ptr])
		goto error;

	strlcpy(dir_list[cur_ptr], dir, PATH_MAX);
	strlcat(dir_list[cur_ptr], ffd.cFileName, PATH_MAX);

	cur_ptr++;
	if (cur_ptr + 1 == cur_size) // Need to reserve for NULL.
	{
		cur_size *= 2;
		dir_list = (char**)realloc(dir_list, cur_size * sizeof(char*));
		if (!dir_list)
		goto error;

		// Make sure it's all NULL'd out since we cannot rely on realloc to do this.
		memset(dir_list + cur_ptr, 0, (cur_size - cur_ptr) * sizeof(char*));
	}
	}while (FindNextFile(hFind, &ffd) != 0);

	FindClose(hFind);
	return dir_list;

	error:
	SSNES_ERR("Failed to open directory: \"%s\"\n", dir);
	if (hFind != INVALID_HANDLE_VALUE)
	FindClose(hFind);
	dir_list_free(dir_list);
	return NULL;
}


static void init_settings (bool load_libsnes_path)
{
	char fname_tmp[MAX_PATH_LENGTH];

	if(!path_file_exists(SYS_CONFIG_FILE))
	{
		SSNES_ERR("Config file \"%s\" desn't exist. Creating...\n", "game:\\ssnes.cfg");
		FILE * f;
		f = fopen(SYS_CONFIG_FILE, "w");
		fclose(f);
	}

	config_file_t * conf = config_file_new(SYS_CONFIG_FILE);

	if(load_libsnes_path)
	{
		CONFIG_GET_STRING(libsnes, "libsnes_path");

		if(!strcmp(g_settings.libsnes, ""))
		{
			//We need to set libsnes to the first entry in the cores
			//directory so that it will be saved to the config file
			char ** dir_list = dir_list_new_360("game:\\", ".xex");
			if (!dir_list)
			{
				SSNES_ERR("Couldn't read directory.\n");
				return;
			}

			const char * first_xex = dir_list[0];

			if(first_xex)
			{
				fill_pathname_base(fname_tmp, first_xex, sizeof(fname_tmp));

				if(strcmp(fname_tmp, "SSNES-Salamander.xex") == 0)
				{
					SSNES_WARN("First entry is SSNES Salamander itself, increment entry by one and check if it exists.\n");
					first_xex = dir_list[1];
					fill_pathname_base(fname_tmp, first_xex, sizeof(fname_tmp));
					
					if(!first_xex)
					{
						//This is very unlikely to happen
						SSNES_WARN("There is no second entry - no choice but to set it to SSNES Salamander\n");
						first_xex = dir_list[0];
						fill_pathname_base(fname_tmp, first_xex, sizeof(fname_tmp));
					}
				}
				SSNES_LOG("Set first .xex entry in dir: [%s] to libsnes path.\n", fname_tmp);
				snprintf(g_settings.libsnes, sizeof(g_settings.libsnes), "game:\\%s", fname_tmp);
			}
			else
			{
				SSNES_ERR("Failed to set first .xex entry to libsnes path.\n");
			}

			dir_list_free(dir_list);
		}
	}

	// g_settings
	CONFIG_GET_BOOL(rewind_enable, "rewind_enable");
	CONFIG_GET_BOOL(video.smooth, "video_smooth");
	CONFIG_GET_BOOL(video.vsync, "video_vsync");
	CONFIG_GET_FLOAT(video.aspect_ratio, "video_aspect_ratio");

	// g_console
	CONFIG_GET_BOOL_CONSOLE(throttle_enable, "throttle_enable");
	CONFIG_GET_BOOL_CONSOLE(gamma_correction_enable, "gamma_correction_enable");
	CONFIG_GET_STRING_CONSOLE(default_rom_startup_dir, "default_rom_startup_dir");
	CONFIG_GET_INT_CONSOLE(aspect_ratio_index, "aspect_ratio_index");
	CONFIG_GET_INT_CONSOLE(screen_orientation, "screen_orientation");
	CONFIG_GET_STRING_CONSOLE(aspect_ratio_name, "aspect_ratio_name");

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
	config_set_string(conf, "libsnes_path", g_settings.libsnes);
	config_set_bool(conf, "rewind_enable", g_settings.rewind_enable);
	config_set_bool(conf, "video_smooth", g_settings.video.smooth);
	config_set_bool(conf, "video_vsync", g_settings.video.vsync);

	// g_console
	config_set_string(conf, "default_rom_startup_dir", g_console.default_rom_startup_dir);
	config_set_bool(conf, "gamma_correction_enable", g_console.gamma_correction_enable);
	config_set_bool(conf, "throttle_enable", g_console.throttle_enable);
	config_set_int(conf, "aspect_ratio_index", g_console.aspect_ratio_index);
	config_set_int(conf, "custom_viewport_width", g_console.custom_viewport_width);
	config_set_int(conf, "custom_viewport_height", g_console.custom_viewport_height);
	config_set_int(conf, "custom_viewport_x", g_console.custom_viewport_x);
	config_set_int(conf, "custom_viewport_y", g_console.custom_viewport_y);
	config_set_int(conf, "screen_orientation", g_console.screen_orientation);
	config_set_string(conf, "aspect_ratio_name", g_console.aspect_ratio_name);

	// g_extern
	config_set_int(conf, "state_slot", g_extern.state_slot);
	config_set_int(conf, "audio_mute", g_extern.audio_data.mute);

	if (!config_file_write(conf, SYS_CONFIG_FILE))
			SSNES_ERR("Failed to write config file to \"%s\". Check permissions.\n", SYS_CONFIG_FILE);

	free(conf);
}

static void get_environment_settings (void)
{
	DWORD ret;

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

	ret = XSetFileCacheSize(0x100000);

	if(ret != TRUE)
	{
		SSNES_ERR("Couldn't change number of bytes reserved for file system cache.\n");
	}

	ret = XFileCacheInit(XFILECACHE_CLEAR_ALL, 0x100000, XFILECACHE_DEFAULT_THREAD, 0, 1);

	if(ret != ERROR_SUCCESS)
	{
		SSNES_ERR("File cache could not be initialized.\n");
	}

	XFlushUtilityDrive();
	//unsigned long result = XMountUtilityDriveEx(XMOUNTUTILITYDRIVE_FORMAT0,8192, 0);

	//if(result != ERROR_SUCCESS)
	//{
	//	SSNES_ERR("Couldn't mount/format utility drive.\n");
	//}

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
	
	strlcpy(DEFAULT_SHADER_FILE, "game:\\media\\shaders\\stock.cg", sizeof(DEFAULT_SHADER_FILE));
	strlcpy(SYS_CONFIG_FILE, "game:\\ssnes.cfg", sizeof(SYS_CONFIG_FILE));
}

static bool manage_libsnes_core(void)
{
	g_extern.verbose = true;
	bool return_code;

	bool set_libsnes_path = false;
	char tmp_path[1024], tmp_path2[1024], tmp_pathnewfile[1024];
	snprintf(tmp_path, sizeof(tmp_path), "game:\\CORE.xex");
	SSNES_LOG("Assumed path of CORE.xex: [%s]\n", tmp_path);
	if(path_file_exists(tmp_path))
	{
		//if CORE.xex exists, this indicates we have just installed
		//a new libsnes port and that we need to change it to a more
		//sane name.

		int ret;

		ssnes_console_name_from_id(tmp_path2, sizeof(tmp_path2));
		strlcat(tmp_path2, ".xex", sizeof(tmp_path2));
		snprintf(tmp_pathnewfile, sizeof(tmp_pathnewfile), "game:\\%s", tmp_path2);

		if(path_file_exists(tmp_pathnewfile))
		{
			SSNES_LOG("Upgrading emulator core...\n");
			//if libsnes core already exists, then that means we are
			//upgrading the libsnes core - so delete pre-existing
			//file first
			ret = DeleteFile(tmp_pathnewfile);
			if(ret != 0)
			{
				SSNES_LOG("Succeeded in removing pre-existing libsnes core: [%s].\n", tmp_pathnewfile);
			}
			else
			{
				SSNES_LOG("Failed to remove pre-existing libsnes core: [%s].\n", tmp_pathnewfile);
			}
		}

		//now attempt the renaming
		ret = MoveFileExA(tmp_path, tmp_pathnewfile, NULL);
		if(ret == 0)
		{
			SSNES_ERR("Failed to rename CORE.xex.\n");
		}
		else
		{
			SSNES_LOG("Libsnes core [%s] renamed to: [%s].\n", tmp_path, tmp_pathnewfile);
			set_libsnes_path = true;
		}
	}
	else
	{
		SSNES_LOG("CORE.xex was not found, libsnes core path will be loaded from config file.\n");
	}

	if(set_libsnes_path)
	{
		//CORE.xex has been renamed, libsnes path will now be set to the recently
		//renamed new libsnes core
		strlcpy(g_settings.libsnes, tmp_pathnewfile, sizeof(g_settings.libsnes));
		return_code = 0;
	}
	else
	{
		//There was no CORE.xex present, or the CORE.xex file was not renamed.
		//The libsnes core path will still be loaded from the config file
		return_code = 1;
	}

	g_extern.verbose = false;

	return return_code;
}

int main(int argc, char *argv[])
{
	get_environment_settings();

	ssnes_main_clear_state();
	config_set_defaults();
	
	bool load_libsnes_path = manage_libsnes_core();

	set_default_settings();
	init_settings(load_libsnes_path);
	init_libsnes_sym();

	xdk360_video_init();
	xdk360_input_init();

	ssnes_input_set_default_keybind_names_for_emulator();

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

	if(g_console.return_to_launcher)
	{
		SSNES_LOG("Attempt to load XEX: [%s].\n", g_console.launch_app_on_exit);
		XLaunchNewImage(g_console.launch_app_on_exit, NULL);
	}
	return 0;
}


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
	USHORT Length;
	USHORT MaximumLength;
	PCHAR Buffer;
} STRING;

extern "C" int __stdcall ObCreateSymbolicLink( STRING*, STRING*);

bool init_ssnes = false;
int Mounted[20];
uint32_t g_emulator_initialized = 0;

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

static void set_default_settings(void)
{
	//g_settings
	g_settings.rewind_enable = false;

	//g_console
	g_console.block_config_read = true;
	g_console.mode_switch = MODE_MENU;
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

	BOOL result_filecache = XSetFileCacheSize(0x100000);

	if(result_filecache != TRUE)
	{
		SSNES_ERR("Couldn't change number of bytes reserved for file system cache.\n");
	}
	DWORD result = XMountUtilityDriveEx(XMOUNTUTILITYDRIVE_FORMAT0,8192, 0);

	if(result != ERROR_SUCCESS)
	{
		SSNES_ERR("Couldn't mount/format utility drive.\n");
	}

	// detect install environment
	DWORD license_mask;

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
}

int main(int argc, char *argv[])
{
	get_environment_settings();

	ssnes_main_clear_state();
	config_set_defaults();

	set_default_settings();

	xdk360_video_init();
	menu_init();

begin_loop:
	if(g_console.mode_switch == MODE_EMULATION)
	{
		while(ssnes_main_iterate());
	}
	else if(g_console.mode_switch == MODE_MENU)
	{
		menu_loop();

		if(init_ssnes)
		{
			if(g_emulator_initialized)
				ssnes_main_deinit();

			struct ssnes_main_wrap args;
			args.verbose = g_extern.verbose;
			args.sram_path = NULL;
			args.state_path = NULL;
			args.config_path = NULL;

			args.rom_path = g_console.rom_path;
			
			int init_ret = ssnes_main_init_wrap(&args);
			g_emulator_initialized = 1;
			init_ssnes = 0;
		}
	}
	else
			goto begin_shutdown;

	goto begin_loop;

begin_shutdown:
	xdk360_video_deinit();
}


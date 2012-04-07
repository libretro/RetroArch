/* SSNES - A frontend for libretro.
 * SSNES Salamander - A frontend for managing some pre-launch tasks.
 * Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2012 - Daniel De Matteis
 *
 * SSNES is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with SSNES.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <xtl.h>

#include "../../compat/strl.h"
#include "../../conf/config_file.h"
#include "../../msvc/msvc_compat.h"

#define MAX_PATH_LENGTH 1024

#define SSNES_LOG(...) do { \
      fprintf(stderr, "SSNES Salamander: " __VA_ARGS__); \
      fflush(stderr); \
   } while (0)

#define SSNES_ERR(...) do { \
      fprintf(stderr, "SSNES Salamander [ERROR] :: " __VA_ARGS__); \
      fflush(stderr); \
   } while (0)

#define SSNES_WARN(...) do { \
      fprintf(stderr, "SSNES Salamander [WARN] :: " __VA_ARGS__); \
      fflush(stderr); \
   } while (0)

char LIBSNES_DIR_PATH[MAX_PATH_LENGTH];
char SYS_CONFIG_FILE[MAX_PATH_LENGTH];
char libsnes_path[MAX_PATH_LENGTH];
DWORD volume_device_type;

static bool path_file_exists(const char *path)
{
	FILE *dummy = fopen(path, "rb");
	if (dummy)
	{
		fclose(dummy);
		return true;
	}
	return false;
}

static void dir_list_free(char **dir_list)
{
	if (!dir_list)
		return;

	char **orig = dir_list;
	while (*dir_list)
		free(*dir_list++);
	free(orig);
}

static void fill_pathname_base(char *out_dir, const char *in_path, size_t size)
{
   const char *ptr = strrchr(in_path, '/');
   if (!ptr)
      ptr = strrchr(in_path, '\\');

   if (ptr)
      ptr++;
   else
      ptr = in_path;
   
   strlcpy(out_dir, ptr, size);
}

static char **dir_list_new(const char *dir, const char *ext)
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

static void find_and_set_first_file(void)
{
	//Last fallback - we'll need to start the first .xex file 
	// we can find in the SSNES cores directory
	char ** dir_list = dir_list_new("game:\\", ".xex");
	if (!dir_list)
	{
		SSNES_ERR("Failed last fallback - SSNES Salamander will exit.\n");
		return;
	}

	char * first_xex = dir_list[0];

	if(first_xex)
	{
		//Check if it's SSNES Salamander itself - if so, first_xex needs to
		//be overridden
		char fname_tmp[MAX_PATH_LENGTH], fname[MAX_PATH_LENGTH];

		fill_pathname_base(fname_tmp, first_xex, sizeof(fname_tmp));

		if(strcmp(fname_tmp, "SSNES-Salamander.xex") == 0)
		{
			SSNES_WARN("First entry is SSNES Salamander itself, increment entry by one and check if it exists.\n");
			first_xex = dir_list[1];
			fill_pathname_base(fname_tmp, first_xex, sizeof(fname_tmp));

			if(!first_xex)
			{
				SSNES_WARN("There is no second entry - no choice but to boot SSNES Salamander\n");
				first_xex = dir_list[0];
				fill_pathname_base(fname_tmp, first_xex, sizeof(fname_tmp));
			}
		}

		SSNES_LOG("Start first entry in libsnes cores dir: [%s].\n", first_xex);

		snprintf(fname, sizeof(fname), "game:\\%s", fname_tmp);
		strlcpy(libsnes_path, fname, sizeof(libsnes_path));
	}
	else
	{
		SSNES_ERR("Failed last fallback - SSNES Salamander will exit.\n");
	}

	dir_list_free(dir_list);
}

static void init_settings(void)
{
	char tmp_str[MAX_PATH_LENGTH];
	bool config_file_exists;


	if(!path_file_exists(SYS_CONFIG_FILE))
	{
		config_file_exists = false;
		SSNES_ERR("Config file \"%s\" doesn't exist. Creating...\n", SYS_CONFIG_FILE);
		FILE * f;
		f = fopen(SYS_CONFIG_FILE, "w");
		fclose(f);
	}
	else
		config_file_exists = true;
		

	//try to find CORE.xex
	char core_xex[1024];
	snprintf(core_xex, sizeof(core_xex), "game:\\CORE.xex");

	if(path_file_exists(core_xex))
	{
		//Start CORE.xex
		snprintf(libsnes_path, sizeof(libsnes_path), core_xex);
		SSNES_LOG("Start [%s].\n", libsnes_path);
	}
	else
	{
		if(config_file_exists)
		{
			config_file_t * conf = config_file_new(SYS_CONFIG_FILE);
			config_get_array(conf, "libsnes_path", tmp_str, sizeof(tmp_str));
			snprintf(libsnes_path, sizeof(libsnes_path), tmp_str);
		}

		if(!config_file_exists || !strcmp(libsnes_path, ""))
			find_and_set_first_file();
		else
		{
			SSNES_LOG("Start [%s] found in ssnes.cfg.\n", libsnes_path);
		}
	}
}

static void get_environment_settings (void)
{
	//for devkits only, we will need to mount all partitions for retail
	//in a different way
	//DmMapDevkitDrive();

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
		SSNES_LOG("SSNES was launched as a standalone DVD, or using DVD emulation, or from the development area of the HDD.\n");
	}
	else
	{
		XContentQueryVolumeDeviceType("GAME",&volume_device_type, NULL);

		switch(volume_device_type)
		{
			case XCONTENTDEVICETYPE_HDD:
				SSNES_LOG("SSNES was launched from a content package on HDD.\n");
				break;
			case XCONTENTDEVICETYPE_MU:
				SSNES_LOG("SSNES was launched from a content package on USB or Memory Unit.\n");
				break;
			case XCONTENTDEVICETYPE_ODD:
				SSNES_LOG("SSNES was launched from a content package on Optical Disc Drive.\n");
				break;
			default:
				SSNES_LOG("SSNES was launched from a content package on an unknown device type.\n");
				break;

		}
	}

	strlcpy(SYS_CONFIG_FILE, "game:\\ssnes.cfg", sizeof(SYS_CONFIG_FILE));
}

int main(int argc, char *argv[])
{
	int ret;
	XINPUT_STATE state;

	get_environment_settings();
	
	XInputGetState(0, &state);

	if(state.Gamepad.wButtons & XINPUT_GAMEPAD_Y)
	{
		//override path, boot first XEX in cores directory
		SSNES_LOG("Fallback - Will boot first XEX in SSNES directory.\n");
		find_and_set_first_file();
	}
	else
	{
		//normal XEX loading path
		init_settings();
	}
	
	XLaunchNewImage(libsnes_path, NULL);
	SSNES_LOG("Launch libsnes core: [%s] (return code: %x]).\n", libsnes_path, ret);

	return 1;
}

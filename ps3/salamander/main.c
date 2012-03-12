/* SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 * SSNES Salamander - A frontend for managing some pre-launch tasks.
 * Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2012 - Daniel De Matteis
 *
 * Some code herein may be based on code found in BSNES.
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
#include <cell/pad.h>
#include <cell/sysmodule.h>
#include <sysutil/sysutil_gamecontent.h>
#include <sys/process.h>
#include <netex/net.h>
#include <np.h>
#include <np/drm.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

#include "../../strl.h"
#include "../../conf/config_file.h"

#define PATH_MAX (512UL)

#define NP_POOL_SIZE (128*1024)
#define MAX_PATH_LENGTH 1024

#ifdef HAVE_LOGGER
#include "logger.h"
#define SSNES_LOG(...) logger_send("SSNES Salamander: " __VA_ARGS__);
#define SSNES_ERR(...) logger_send("SSNES Salamander [ERROR] :: " __VA_ARGS__);
#define SSNES_WARN(...) logger_send("SSNES Salamander [WARN] :: " __VA_ARGS__);
#else
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
#endif

static uint8_t np_pool[NP_POOL_SIZE];
char contentInfoPath[MAX_PATH_LENGTH];
char usrDirPath[MAX_PATH_LENGTH];
char LIBSNES_DIR_PATH[MAX_PATH_LENGTH];
char SYS_CONFIG_FILE[MAX_PATH_LENGTH];
char libsnes_path[MAX_PATH_LENGTH];

SYS_PROCESS_PARAM(1001, 0x100000)

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

static char **dir_list_new(const char *dir, const char *ext)
{
	size_t cur_ptr = 0;
	size_t cur_size = 32;
	char **dir_list = NULL;

	DIR *directory = NULL;
	const struct dirent *entry = NULL;

	directory = opendir(dir);
	if (!directory)
		goto error;

	dir_list = (char**)calloc(cur_size, sizeof(char*));
	if (!dir_list)
		goto error;

	while ((entry = readdir(directory)))
	{
		// Not a perfect search of course, but hopefully good enough in practice.
		if (ext && !strstr(entry->d_name, ext))
			continue;

		dir_list[cur_ptr] = (char*)malloc(PATH_MAX);
		if (!dir_list[cur_ptr])
			goto error;

		strlcpy(dir_list[cur_ptr], dir, PATH_MAX);
		strlcat(dir_list[cur_ptr], "/", PATH_MAX);
		strlcat(dir_list[cur_ptr], entry->d_name, PATH_MAX);

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
	}

	closedir(directory);
	return dir_list;

error:
	SSNES_ERR("Failed to open directory: \"%s\"\n", dir);
	if (directory)
		closedir(directory);
	dir_list_free(dir_list);
	return NULL;
}

static void find_and_set_first_file(void)
{
	//Last fallback - we'll need to start the first .SELF file 
	// we can find in the SSNES cores directory
	char ** dir_list = dir_list_new(LIBSNES_DIR_PATH, ".SELF");
	if (!dir_list)
	{
		SSNES_ERR("Failed last fallback - SSNES Salamander will exit.\n");
		return;
	}

	const char * first_self = dir_list[0];

	if(first_self)
	{
		SSNES_LOG("Start first entry in libsnes cores dir: [%s].\n", first_self);
		strlcpy(libsnes_path, first_self, sizeof(libsnes_path));
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
		

	//try to find CORE.SELF 
	char core_self[1024];
	snprintf(core_self, sizeof(core_self), "%s/CORE.SELF", LIBSNES_DIR_PATH);

	if(path_file_exists(core_self))
	{
		//Start CORE.SELF
		snprintf(libsnes_path, sizeof(libsnes_path), core_self);
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
	unsigned int get_type;
	unsigned int get_attributes;
	CellGameContentSize size;
	char dirName[CELL_GAME_DIRNAME_SIZE];

	memset(&size, 0x00, sizeof(CellGameContentSize));

	int ret = cellGameBootCheck(&get_type, &get_attributes, &size, dirName);
	if(ret < 0)
	{
		SSNES_ERR("cellGameBootCheck() Error: 0x%x.\n", ret);
	}
	else
	{
		SSNES_LOG("cellGameBootCheck() OK.\n");
		SSNES_LOG("Directory name: [%s].\n", dirName);
		SSNES_LOG(" HDD Free Size (in KB) = [%d] Size (in KB) = [%d] System Size (in KB) = [%d].\n", size.hddFreeSizeKB, size.sizeKB, size.sysSizeKB);

		switch(get_type)
		{
			case CELL_GAME_GAMETYPE_DISC:
				SSNES_LOG("SSNES was launched on Optical Disc Drive.\n");
				break;
			case CELL_GAME_GAMETYPE_HDD:
				SSNES_LOG("SSNES was launched on HDD.\n");
				break;
		}

		if((get_attributes & CELL_GAME_ATTRIBUTE_APP_HOME) == CELL_GAME_ATTRIBUTE_APP_HOME)
			SSNES_LOG("SSNES was launched from host machine (APP_HOME).\n");

		ret = cellGameContentPermit(contentInfoPath, usrDirPath);

		if(ret < 0)
		{
			SSNES_ERR("cellGameContentPermit() Error: 0x%x\n", ret);
		}
		else
		{
			SSNES_LOG("cellGameContentPermit() OK.\n");
			SSNES_LOG("contentInfoPath : [%s].\n", contentInfoPath);
			SSNES_LOG("usrDirPath : [%s].\n", usrDirPath);
		}

		/* now we fill in all the variables */
		snprintf(SYS_CONFIG_FILE, sizeof(SYS_CONFIG_FILE), "%s/ssnes.cfg", usrDirPath);
		snprintf(LIBSNES_DIR_PATH, sizeof(LIBSNES_DIR_PATH), "%s/cores", usrDirPath);
	}
}

int main(int argc, char *argv[])
{
	CellPadData pad_data;
	char spawn_data[256], spawn_data_size[16];
	SceNpDrmKey * k_licensee = NULL;
	int ret;

	cellSysmoduleLoadModule(CELL_SYSMODULE_IO);
	cellSysmoduleLoadModule(CELL_SYSMODULE_FS);
	cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_GAME);
	cellSysmoduleLoadModule(CELL_SYSMODULE_NET);

	cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_NP);

	sys_net_initialize_network();

#ifdef HAVE_LOGGER
	logger_init();
#endif

	sceNpInit(NP_POOL_SIZE, np_pool);

	get_environment_settings();
	
	cellPadInit(7);

	cellPadGetData(0, &pad_data);

	if(pad_data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_TRIANGLE)
	{
		//override path, boot first SELF in cores directory
		SSNES_LOG("Fallback - Will boot first SELF in SSNES cores/ directory.\n");
		find_and_set_first_file();
	}
	else
	{
		//normal SELF loading path
		init_settings();
	}

	cellPadEnd();

#ifdef HAVE_LOGGER
	logger_shutdown();
#endif

	for(unsigned int i = 0; i < sizeof(spawn_data); ++i)
		spawn_data[i] = i & 0xff;

	sprintf(spawn_data_size, "%d", 256);

	const char * const spawn_argv[] = {
		spawn_data_size,
		"test argv for",
		"sceNpDrmProcessExitSpawn2()",
		NULL
	};

	ret = sceNpDrmProcessExitSpawn2(k_licensee, libsnes_path, (const char** const)spawn_argv, NULL, (sys_addr_t)spawn_data, 256, 1000, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
	SSNES_LOG("Launch libsnes core: [%s] (return code: %x]).\n", libsnes_path, ret);
	if(ret < 0)
	{
		SSNES_LOG("SELF file is not of NPDRM type, trying another approach to boot it...\n");
		sys_game_process_exitspawn2(libsnes_path, NULL, NULL, NULL, 0, 1000, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);

	}
	sceNpTerm();

	sys_net_finalize_network();

	cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_NP);

	cellSysmoduleUnloadModule(CELL_SYSMODULE_NET);
	cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_GAME);
	cellSysmoduleLoadModule(CELL_SYSMODULE_FS);
	cellSysmoduleLoadModule(CELL_SYSMODULE_IO);

	return 1;
}

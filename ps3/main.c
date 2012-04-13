/* SSNES - A frontend for libretro.
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

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <sdk_version.h>
#include <sys/process.h>
#include <cell/sysmodule.h>
#include <sysutil/sysutil_screenshot.h>
#include <sysutil/sysutil_common.h>
#include <sysutil/sysutil_gamecontent.h>
#include <sysutil/sysutil_syscache.h>

#if(CELL_SDK_VERSION > 0x340000)
#include <sysutil/sysutil_bgmplayback.h>
#endif

#include <cell/sysmodule.h>
#include <sysutil/sysutil_common.h>
#include <sys/process.h>
#include <netex/net.h>
#include <np.h>
#include <np/drm.h>

#include "ps3_input.h"
#include "ps3_video_psgl.h"

#include "../console/console_ext.h"
#include "../conf/config_file.h"
#include "../conf/config_file_macros.h"
#include "../general.h"
#include "../file.h"

#include "shared.h"

#include "menu.h"

#define MAX_PATH_LENGTH 1024

#define EMULATOR_CONTENT_DIR "SSNE10000"
#define EMULATOR_CORE_DIR "cores"

#define CACHE_ID "ABCD12345"
#define NP_POOL_SIZE (128*1024)

static uint8_t np_pool[NP_POOL_SIZE];
char contentInfoPath[MAX_PATH_LENGTH];
char usrDirPath[MAX_PATH_LENGTH];
char DEFAULT_PRESET_FILE[MAX_PATH_LENGTH];
char DEFAULT_BORDER_FILE[MAX_PATH_LENGTH];
char DEFAULT_MENU_BORDER_FILE[MAX_PATH_LENGTH];
char PRESETS_DIR_PATH[MAX_PATH_LENGTH];
char INPUT_PRESETS_DIR_PATH[MAX_PATH_LENGTH];
char BORDERS_DIR_PATH[MAX_PATH_LENGTH];
char SHADERS_DIR_PATH[MAX_PATH_LENGTH];
char LIBSNES_DIR_PATH[MAX_PATH_LENGTH];
char DEFAULT_SHADER_FILE[MAX_PATH_LENGTH];
char DEFAULT_MENU_SHADER_FILE[MAX_PATH_LENGTH];
char SYS_CONFIG_FILE[MAX_PATH_LENGTH];
char EMULATOR_CORE_SELF[MAX_PATH_LENGTH];
char MULTIMAN_EXECUTABLE[MAX_PATH_LENGTH];

int ssnes_main(int argc, char *argv[]);

SYS_PROCESS_PARAM(1001, 0x100000)

#undef main

static void set_default_settings(void)
{
   // g_settings
   strlcpy(g_settings.cheat_database, usrDirPath, sizeof(g_settings.cheat_database));
   g_settings.rewind_enable = false;
   strlcpy(g_settings.video.cg_shader_path, DEFAULT_SHADER_FILE, sizeof(g_settings.video.cg_shader_path));
   g_settings.video.fbo_scale_x = 2.0f;
   g_settings.video.fbo_scale_y = 2.0f;
   g_settings.video.render_to_texture = true;
   strlcpy(g_settings.video.second_pass_shader, DEFAULT_SHADER_FILE, sizeof(g_settings.video.second_pass_shader));
   g_settings.video.second_pass_smooth = true;
   g_settings.video.smooth = true;
   g_settings.video.vsync = true;
   strlcpy(g_settings.cheat_database, usrDirPath, sizeof(g_settings.cheat_database));
   g_settings.video.msg_pos_x = 0.05f;
   g_settings.video.msg_pos_y = 0.90f;
   g_settings.video.aspect_ratio = -1.0f;

   ssnes_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_B]		=	platform_keys[PS3_DEVICE_ID_JOYPAD_CROSS].joykey;
   ssnes_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_Y]		=	platform_keys[PS3_DEVICE_ID_JOYPAD_SQUARE].joykey;
   ssnes_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_SELECT]	=	platform_keys[PS3_DEVICE_ID_JOYPAD_SELECT].joykey;
   ssnes_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_START]	=	platform_keys[PS3_DEVICE_ID_JOYPAD_START].joykey;
   ssnes_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_UP]		=	platform_keys[PS3_DEVICE_ID_JOYPAD_UP].joykey;
   ssnes_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_DOWN]	=	platform_keys[PS3_DEVICE_ID_JOYPAD_DOWN].joykey;
   ssnes_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_LEFT]	=	platform_keys[PS3_DEVICE_ID_JOYPAD_LEFT].joykey;
   ssnes_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_RIGHT]	=	platform_keys[PS3_DEVICE_ID_JOYPAD_RIGHT].joykey;
   ssnes_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_A]		=	platform_keys[PS3_DEVICE_ID_JOYPAD_CIRCLE].joykey;
   ssnes_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_X]		=	platform_keys[PS3_DEVICE_ID_JOYPAD_TRIANGLE].joykey;
   ssnes_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_L]		=	platform_keys[PS3_DEVICE_ID_JOYPAD_L1].joykey;
   ssnes_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_R]		=	platform_keys[PS3_DEVICE_ID_JOYPAD_R1].joykey;

   for(uint32_t x = 0; x < MAX_PLAYERS; x++)
      ssnes_input_set_default_keybinds(x);

   // g_console
   g_console.block_config_read = true;
   g_console.frame_advance_enable = false;
   g_console.emulator_initialized = 0;
   g_console.screenshots_enable = true;
   g_console.throttle_enable = true;
   g_console.initialize_ssnes_enable = false;
   g_console.triple_buffering_enable = true;
   g_console.default_savestate_dir_enable = false;
   g_console.default_sram_dir_enable = false;
   g_console.mode_switch = MODE_MENU;
   g_console.screen_orientation = ORIENTATION_NORMAL;
   g_console.current_resolution_id = CELL_VIDEO_OUT_RESOLUTION_UNDEFINED;
   strlcpy(g_console.default_rom_startup_dir, "/", sizeof(g_console.default_rom_startup_dir));
   strlcpy(g_console.default_savestate_dir, usrDirPath, sizeof(g_console.default_savestate_dir));
   strlcpy(g_console.default_sram_dir, usrDirPath, sizeof(g_console.default_sram_dir));
   g_console.aspect_ratio_index = 0;
   g_console.menu_font_size = 1.0f;
   g_console.overscan_enable = false;
   g_console.overscan_amount = 0.0f;
   g_console.sound_mode = SOUND_MODE_NORMAL;
   g_console.custom_viewport_width = 0;
   g_console.custom_viewport_height = 0;
   g_console.custom_viewport_x = 0;
   g_console.custom_viewport_y = 0;
   strlcpy(g_console.rsound_ip_address, "0.0.0.0", sizeof(g_console.rsound_ip_address));
   g_console.custom_bgm_enable = true;

   // g_extern
   g_extern.state_slot = 0;
   g_extern.audio_data.mute = 0;
   g_extern.verbose = true;
}

static void init_settings(bool load_libretro_path)
{
   if(!path_file_exists(SYS_CONFIG_FILE))
   {
	   SSNES_ERR("Config file \"%s\" doesn't exist. Creating...\n", SYS_CONFIG_FILE);
	   FILE * f;
	   f = fopen(SYS_CONFIG_FILE, "w");
	   fclose(f);
   }
   else
   {
      config_file_t * conf = config_file_new(SYS_CONFIG_FILE);

      // g_settings

      if(load_libretro_path)
      {
         CONFIG_GET_STRING(libretro, "libretro_path");

	 if(!strcmp(g_settings.libretro, ""))
	 {
            //We need to set libretro to the first entry in the cores
	    //directory so that it will be saved to the config file
            char ** dir_list = dir_list_new(LIBSNES_DIR_PATH, ".SELF");

	    if (!dir_list)
	    {
               SSNES_ERR("Couldn't read %s directory.\n", EMULATOR_CORE_DIR);
	       return;
	    }

	    const char * first_self = dir_list[0];

	    if(first_self)
	    {
               SSNES_LOG("Set first entry in libretro %s dir: [%s] to libretro path.\n", EMULATOR_CORE_DIR, first_self);
	       strlcpy(g_settings.libretro, first_self, sizeof(g_settings.libretro));
	    }
	    else
	    {
               SSNES_ERR("Failed to set first entry in libretro %s dir to libretro path.\n", EMULATOR_CORE_DIR);
	    }

	    dir_list_free(dir_list);
	 }
      }

      CONFIG_GET_STRING(cheat_database, "cheat_database");
      CONFIG_GET_BOOL(rewind_enable, "rewind_enable");
      CONFIG_GET_STRING(video.cg_shader_path, "video_cg_shader");
      CONFIG_GET_STRING(video.second_pass_shader, "video_second_pass_shader");
      CONFIG_GET_FLOAT(video.fbo_scale_x, "video_fbo_scale_x");
      CONFIG_GET_FLOAT(video.fbo_scale_y, "video_fbo_scale_y");
      CONFIG_GET_BOOL(video.render_to_texture, "video_render_to_texture");
      CONFIG_GET_BOOL(video.second_pass_smooth, "video_second_pass_smooth");
      CONFIG_GET_BOOL(video.smooth, "video_smooth");
      CONFIG_GET_BOOL(video.vsync, "video_vsync");
      CONFIG_GET_FLOAT(video.aspect_ratio, "video_aspect_ratio");

      for (unsigned i = 0; i < 7; i++)
      {
         char cfg[64];
	 snprintf(cfg, sizeof(cfg), "input_dpad_emulation_p%u", i + 1);
	 CONFIG_GET_INT(input.dpad_emulation[i], cfg);
      }

      // g_console

      CONFIG_GET_BOOL_CONSOLE(custom_bgm_enable, "custom_bgm_enable");
      CONFIG_GET_BOOL_CONSOLE(overscan_enable, "overscan_enable");
      CONFIG_GET_BOOL_CONSOLE(screenshots_enable, "screenshots_enable");
      CONFIG_GET_BOOL_CONSOLE(throttle_enable, "throttle_enable");
      CONFIG_GET_BOOL_CONSOLE(triple_buffering_enable, "triple_buffering_enable");
      CONFIG_GET_INT_CONSOLE(aspect_ratio_index, "aspect_ratio_index");
      CONFIG_GET_INT_CONSOLE(current_resolution_id, "current_resolution_id");
      CONFIG_GET_INT_CONSOLE(custom_viewport_x, "custom_viewport_x");
      CONFIG_GET_INT_CONSOLE(custom_viewport_y, "custom_viewport_y");
      CONFIG_GET_INT_CONSOLE(custom_viewport_width, "custom_viewport_width");
      CONFIG_GET_INT_CONSOLE(custom_viewport_height, "custom_viewport_height");
      CONFIG_GET_INT_CONSOLE(screen_orientation, "screen_orientation");
      CONFIG_GET_INT_CONSOLE(sound_mode, "sound_mode");
      CONFIG_GET_STRING_CONSOLE(default_rom_startup_dir, "default_rom_startup_dir");
      CONFIG_GET_FLOAT_CONSOLE(menu_font_size, "menu_font_size");
      CONFIG_GET_FLOAT_CONSOLE(overscan_amount, "overscan_amount");

      // g_extern
      CONFIG_GET_INT_EXTERN(state_slot, "state_slot");
      CONFIG_GET_INT_EXTERN(audio_data.mute, "audio_mute");
   }
}

static void save_settings(void)
{
	if(!path_file_exists(SYS_CONFIG_FILE))
	{
		SSNES_ERR("Config file \"%s\" doesn't exist. Creating...\n", SYS_CONFIG_FILE);
		FILE * f;
		f = fopen(SYS_CONFIG_FILE, "w");
		fclose(f);
	}
	else
	{

		config_file_t * conf = config_file_new(SYS_CONFIG_FILE);

		if(conf == NULL)
			conf = config_file_new(NULL);

		// g_settings
		config_set_string(conf, "libretro_path", g_settings.libretro);
		config_set_string(conf, "cheat_database_path", g_settings.cheat_database);
		config_set_bool(conf, "rewind_enable", g_settings.rewind_enable);
		config_set_string(conf, "video_cg_shader", g_settings.video.cg_shader_path);
		config_set_string(conf, "video_second_pass_shader", g_settings.video.second_pass_shader);
		config_set_float(conf, "video_aspect_ratio", g_settings.video.aspect_ratio);
		config_set_float(conf, "video_fbo_scale_x", g_settings.video.fbo_scale_x);
		config_set_float(conf, "video_fbo_scale_y", g_settings.video.fbo_scale_y);
		config_set_bool(conf, "video_render_to_texture", g_settings.video.render_to_texture);
		config_set_bool(conf, "video_second_pass_smooth", g_settings.video.second_pass_smooth);
		config_set_bool(conf, "video_smooth", g_settings.video.smooth);
		config_set_bool(conf, "video_vsync", g_settings.video.vsync);

      for (unsigned i = 0; i < 7; i++)
      {
         char cfg[64];
         snprintf(cfg, sizeof(cfg), "input_dpad_emulation_p%u", i + 1);
         config_set_int(conf, cfg, g_settings.input.dpad_emulation[i]);
      }

		// g_console
		config_set_bool(conf, "custom_bgm_enable", g_console.custom_bgm_enable);
		config_set_bool(conf, "overscan_enable", g_console.overscan_enable);
		config_set_bool(conf, "screenshots_enable", g_console.screenshots_enable);
		config_set_bool(conf, "throttle_enable", g_console.throttle_enable);
		config_set_bool(conf, "triple_buffering_enable", g_console.triple_buffering_enable);
		config_set_int(conf, "sound_mode", g_console.sound_mode);
		config_set_int(conf, "aspect_ratio_index", g_console.aspect_ratio_index);
		config_set_int(conf, "current_resolution_id", g_console.current_resolution_id);
		config_set_int(conf, "custom_viewport_width", g_console.custom_viewport_width);
		config_set_int(conf, "custom_viewport_height", g_console.custom_viewport_height);
		config_set_int(conf, "custom_viewport_x", g_console.custom_viewport_x);
		config_set_int(conf, "custom_viewport_y", g_console.custom_viewport_y);
		config_set_int(conf, "screen_orientation", g_console.screen_orientation);
		config_set_string(conf, "default_rom_startup_dir", g_console.default_rom_startup_dir);
		config_set_float(conf, "menu_font_size", g_console.menu_font_size);
		config_set_float(conf, "overscan_amount", g_console.overscan_amount);

		// g_extern
		config_set_int(conf, "state_slot", g_extern.state_slot);
		config_set_int(conf, "audio_mute", g_extern.audio_data.mute);

		if (!config_file_write(conf, SYS_CONFIG_FILE))
			SSNES_ERR("Failed to write config file to \"%s\". Check permissions.\n", SYS_CONFIG_FILE);

		free(conf);
	}
}

static void callback_sysutil_exit(uint64_t status, uint64_t param, void *userdata)
{
   (void) param;
   (void) userdata;

   switch (status)
   {
      case CELL_SYSUTIL_REQUEST_EXITGAME:
         g_console.menu_enable = false;
	 g_quitting = true;
	 g_console.ingame_menu_enable = false;
	 g_console.mode_switch = MODE_EXIT;
	 break;
      case CELL_SYSUTIL_OSKDIALOG_FINISHED:
	 oskutil_close(&g_console.oskutil_handle);
	 oskutil_finished(&g_console.oskutil_handle);
	 break;
      case CELL_SYSUTIL_OSKDIALOG_UNLOADED:
	 oskutil_unload(&g_console.oskutil_handle);
	 break;
   }
}

static void get_environment_settings(int argc, char *argv[])
{
   g_extern.verbose = true;

   int ret;
   unsigned int get_type;
   unsigned int get_attributes;
   CellGameContentSize size;
   char dirName[CELL_GAME_DIRNAME_SIZE];
   CellSysCacheParam param;
   memset(&param, 0x00, sizeof(CellSysCacheParam));
   strncpy(param.cacheId,CACHE_ID, sizeof(CellSysCacheParam));

   ret = cellSysCacheMount(&param);
   if(ret != CELL_SYSCACHE_RET_OK_CLEARED)
   {
      SSNES_ERR("System cache partition could not be mounted, it might be already mounted.\n");
   }

   if(argc > 1)
   {
      /* launched from external launcher */
      strncpy(MULTIMAN_EXECUTABLE, argv[2], sizeof(MULTIMAN_EXECUTABLE));
   }
   else
   {
      /* not launched from external launcher, set default path */
      strncpy(MULTIMAN_EXECUTABLE, "/dev_hdd0/game/BLES80608/USRDIR/RELOAD.SELF",
		   sizeof(MULTIMAN_EXECUTABLE));
   }

   if(path_file_exists(MULTIMAN_EXECUTABLE) && argc > 1 &&  path_file_exists(argv[1]))
   {
	   g_console.external_launcher_support = EXTERN_LAUNCHER_MULTIMAN;
	   SSNES_LOG("Started from multiMAN, auto-game start enabled.\n");
   }
   else
   {
	   g_console.external_launcher_support = EXTERN_LAUNCHER_SALAMANDER;
	   SSNES_WARN("Not started from multiMAN, auto-game start disabled.\n");
   }

   memset(&size, 0x00, sizeof(CellGameContentSize));

   ret = cellGameBootCheck(&get_type, &get_attributes, &size, dirName);
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

	   if(g_console.external_launcher_support == EXTERN_LAUNCHER_MULTIMAN)
	   {
		   snprintf(contentInfoPath, sizeof(contentInfoPath), "/dev_hdd0/game/%s", EMULATOR_CONTENT_DIR);
		   snprintf(usrDirPath, sizeof(usrDirPath), "/dev_hdd0/game/%s/USRDIR", EMULATOR_CONTENT_DIR);
	   }

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
	   snprintf(DEFAULT_PRESET_FILE, sizeof(DEFAULT_PRESET_FILE), "%s/%s/presets/stock.conf", usrDirPath, EMULATOR_CORE_DIR);
	   snprintf(DEFAULT_BORDER_FILE, sizeof(DEFAULT_BORDER_FILE), "%s/%s/borders/Centered-1080p/mega-man-2.png", usrDirPath, EMULATOR_CORE_DIR);
	   snprintf(DEFAULT_MENU_BORDER_FILE, sizeof(DEFAULT_MENU_BORDER_FILE), "%s/%s/borders/Menu/main-menu.png", usrDirPath, EMULATOR_CORE_DIR);
	   snprintf(PRESETS_DIR_PATH, sizeof(PRESETS_DIR_PATH), "%s/%s/presets", usrDirPath, EMULATOR_CORE_DIR);
	   snprintf(INPUT_PRESETS_DIR_PATH, sizeof(INPUT_PRESETS_DIR_PATH), "%s/input", PRESETS_DIR_PATH);
	   snprintf(LIBSNES_DIR_PATH, sizeof(LIBSNES_DIR_PATH), "%s/%s", usrDirPath, EMULATOR_CORE_DIR);
	   snprintf(BORDERS_DIR_PATH, sizeof(BORDERS_DIR_PATH), "%s/%s/borders", usrDirPath, EMULATOR_CORE_DIR);
	   snprintf(SHADERS_DIR_PATH, sizeof(SHADERS_DIR_PATH), "%s/%s/shaders", usrDirPath, EMULATOR_CORE_DIR);
	   snprintf(DEFAULT_SHADER_FILE, sizeof(DEFAULT_SHADER_FILE), "%s/%s/shaders/stock.cg", usrDirPath, EMULATOR_CORE_DIR);
	   snprintf(DEFAULT_MENU_SHADER_FILE, sizeof(DEFAULT_MENU_SHADER_FILE), "%s/%s/shaders/Borders/Menu/border-only-ssnes.cg", usrDirPath, EMULATOR_CORE_DIR);
	   snprintf(SYS_CONFIG_FILE, sizeof(SYS_CONFIG_FILE), "%s/ssnes.cfg", usrDirPath);
   }

   g_extern.verbose = false;
}

int main(int argc, char *argv[])
{
   SSNES_LOG("Registering system utility callback...\n");
   cellSysutilRegisterCallback(0, callback_sysutil_exit, NULL);

   cellSysmoduleLoadModule(CELL_SYSMODULE_IO);
   cellSysmoduleLoadModule(CELL_SYSMODULE_FS);
   cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_GAME);
   cellSysmoduleLoadModule(CELL_SYSMODULE_AVCONF_EXT);
   cellSysmoduleLoadModule(CELL_SYSMODULE_PNGDEC);
   cellSysmoduleLoadModule(CELL_SYSMODULE_JPGDEC);
   cellSysmoduleLoadModule(CELL_SYSMODULE_NET);
   cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_NP);

   sys_net_initialize_network();

#ifdef HAVE_LOGGER
   logger_init();
#endif

   sceNpInit(NP_POOL_SIZE, np_pool);

   ssnes_main_clear_state();
   get_environment_settings(argc, argv);

   config_set_defaults();

   char full_path[1024], tmp_path[1024];
   snprintf(full_path, sizeof(full_path), "%s/%s/CORE.SELF", usrDirPath, EMULATOR_CORE_DIR);
   snprintf(tmp_path, sizeof(tmp_path), "%s/%s/", usrDirPath, EMULATOR_CORE_DIR);
   bool load_libretro_path = ssnes_manage_libretro_core(full_path, tmp_path, ".SELF");

   set_default_settings();
   init_settings(load_libretro_path);
   init_libretro_sym();

#if(CELL_SDK_VERSION > 0x340000)
   if (g_console.screenshots_enable)
   {
      cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_SCREENSHOT);
      CellScreenShotSetParam screenshot_param = {0, 0, 0, 0};

      screenshot_param.photo_title = "SSNES PS3";
      screenshot_param.game_title = "SSNES PS3";
      cellScreenShotSetParameter (&screenshot_param);
      cellScreenShotEnable();
   }
   if (g_console.custom_bgm_enable)
      cellSysutilEnableBgmPlayback();
#endif

   ps3graphics_video_init(true);
   ps3_input_init();
   oskutil_init(&g_console.oskutil_handle, 0);

   ssnes_input_set_default_keybind_names_for_emulator();

   menu_init();

   switch(g_console.external_launcher_support)
   {
      case EXTERN_LAUNCHER_SALAMANDER:
         g_console.mode_switch = MODE_MENU;
	 break;
      case EXTERN_LAUNCHER_MULTIMAN:
	 SSNES_LOG("Started from multiMAN, will auto-start game.\n");
	 strncpy(g_console.rom_path, argv[1], sizeof(g_console.rom_path));
	 g_console.initialize_ssnes_enable = 1;
	 g_console.mode_switch = MODE_EMULATION;
	 ssnes_startup(SYS_CONFIG_FILE);
	 break;
   }

begin_loop:
   if(g_console.mode_switch == MODE_EMULATION)
   {
      bool repeat = false;

      input_ps3.poll(NULL);

      do{
         repeat = ssnes_main_iterate();
      }while(repeat && !g_console.frame_advance_enable);
   }
   else if(g_console.mode_switch == MODE_MENU)
   {
      menu_loop();
      ssnes_startup(SYS_CONFIG_FILE);
   }
   else
      goto begin_shutdown;

   goto begin_loop;

begin_shutdown:
   if(path_file_exists(SYS_CONFIG_FILE))
      save_settings();
   if(g_console.emulator_initialized)
      ssnes_main_deinit();
   cell_pad_input_deinit();
   ps3_video_deinit();
   if(g_console.oskutil_handle.is_running)
      oskutil_unload(&g_console.oskutil_handle);
#ifdef HAVE_LOGGER
   logger_shutdown();
#endif

   if(g_console.screenshots_enable)
      cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_SCREENSHOT);
   cellSysmoduleUnloadModule(CELL_SYSMODULE_JPGDEC);
   cellSysmoduleUnloadModule(CELL_SYSMODULE_PNGDEC);
   cellSysmoduleUnloadModule(CELL_SYSMODULE_AVCONF_EXT);
   cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_GAME);

   int ret = cellSysCacheClear();

   if(ret != CELL_SYSCACHE_RET_OK_CLEARED)
   {
      SSNES_ERR("System cache partition could not be cleared on exit.\n");
   }

   ssnes_exec();

   return 1;
}

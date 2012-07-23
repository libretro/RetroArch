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

#include <xtl.h>

#include <stddef.h>
#include <stdint.h>
#include <string>

#ifdef _XBOX360
#include <xfilecache.h>
#include "../../360/frontend-xdk/menu.h"
#endif

#include <xbdm.h>

#ifdef _XBOX
#if defined(_XBOX1)
#include "../../xbox1/xdk_d3d8.h"
#elif defined(_XBOX360)
#include "../../360/xdk_d3d9.h"
#endif
#endif

#include "../../console/retroarch_console.h"
#include "../../conf/config_file.h"
#include "../../conf/config_file_macros.h"
#include "../../file.h"
#include "../../general.h"

char DEFAULT_SHADER_FILE[PATH_MAX];
char SYS_CONFIG_FILE[PATH_MAX];

int rarch_main(int argc, char *argv[]);

#undef main

static void set_default_settings (void)
{
   //g_settings
   g_settings.rewind_enable = false;
   strlcpy(g_settings.video.cg_shader_path, DEFAULT_SHADER_FILE, sizeof(g_settings.video.cg_shader_path));
   g_settings.video.fbo_scale_x = 2.0f;
   g_settings.video.fbo_scale_y = 2.0f;
   g_settings.video.render_to_texture = true;
   strlcpy(g_settings.video.second_pass_shader, DEFAULT_SHADER_FILE, sizeof(g_settings.video.second_pass_shader));
   g_settings.video.second_pass_smooth = true;
   g_settings.video.smooth = true;
   g_settings.video.vsync = true;
   strlcpy(g_settings.cheat_database, "game:", sizeof(g_settings.cheat_database));
   g_settings.video.aspect_ratio = -1.0f;

   rarch_input_set_controls_default();

   //g_console
   g_console.block_config_read = true;
   g_console.frame_advance_enable = false;
   g_console.emulator_initialized = 0;
   g_console.gamma_correction_enable = true;
   g_console.initialize_rarch_enable = false;
   g_console.fbo_enabled = true;
   g_console.mode_switch = MODE_MENU;
   g_console.screen_orientation = ORIENTATION_NORMAL;
   g_console.throttle_enable = true;
   g_console.aspect_ratio_index = 0;
   strlcpy(g_console.default_rom_startup_dir, "game:", sizeof(g_console.default_rom_startup_dir));
   g_console.viewports.custom_vp.width = 0;
   g_console.viewports.custom_vp.height = 0;
   g_console.viewports.custom_vp.x = 0;
   g_console.viewports.custom_vp.y = 0;
   g_console.color_format = 0;
   g_console.info_msg_enable = true;

   //g_extern
   g_extern.state_slot = 0;
   g_extern.audio_data.mute = 0;
   g_extern.verbose = true;
}

static void get_environment_settings (void)
{
   HRESULT ret;
#ifdef HAVE_HDD_CACHE_PARTITION
   ret = XSetFileCacheSize(0x100000);

   if(ret != TRUE)
   {
      RARCH_ERR("Couldn't change number of bytes reserved for file system cache.\n");
   }

   ret = XFileCacheInit(XFILECACHE_CLEAR_ALL, 0x100000, XFILECACHE_DEFAULT_THREAD, 0, 1);

   if(ret != ERROR_SUCCESS)
   {
      RARCH_ERR("File cache could not be initialized.\n");
   }

   XFlushUtilityDrive();
#endif

#ifdef _XBOX360
   // detect install environment
   unsigned long license_mask;

   if (XContentGetLicenseMask(&license_mask, NULL) != ERROR_SUCCESS)
   {
      printf("RetroArch was launched as a standalone DVD, or using DVD emulation, or from the development area of the HDD.\n");
   }
   else
   {
      XContentQueryVolumeDeviceType("GAME",&g_console.volume_device_type, NULL);

      switch(g_console.volume_device_type)
      {
         case XCONTENTDEVICETYPE_HDD:
            printf("RetroArch was launched from a content package on HDD.\n");
	    break;
	 case XCONTENTDEVICETYPE_MU:
	    printf("RetroArch was launched from a content package on USB or Memory Unit.\n");
	    break;
	 case XCONTENTDEVICETYPE_ODD:
	    printf("RetroArch was launched from a content package on Optical Disc Drive.\n");
	    break;
	 default:
	    printf("RetroArch was launched from a content package on an unknown device type.\n");
	    break;
      }
   }
#endif

   strlcpy(DEFAULT_SHADER_FILE, "game:\\media\\shaders\\stock.cg", sizeof(DEFAULT_SHADER_FILE));
#ifdef _XBOX1
   /* FIXME: Hardcoded */
   strlcpy(SYS_CONFIG_FILE, "D:\\retroarch.cfg", sizeof(SYS_CONFIG_FILE));
   strlcpy(g_settings.system_directory, "D:\\system\\", sizeof(g_settings.system_directory));
#else
   strlcpy(SYS_CONFIG_FILE, "game:\\retroarch.cfg", sizeof(SYS_CONFIG_FILE));
   strlcpy(g_settings.system_directory, "game:\\system\\", sizeof(g_settings.system_directory));
#endif
}

static void configure_libretro(const char *path_prefix, const char * extension)
{
   char full_path[1024];
   snprintf(full_path, sizeof(full_path), "%sCORE%s", path_prefix, extension);

   bool find_libretro_file = rarch_configure_libretro_core(full_path, path_prefix, path_prefix, 
   SYS_CONFIG_FILE, extension);

   set_default_settings();
   rarch_config_load(SYS_CONFIG_FILE, path_prefix, extension, find_libretro_file);
   init_libretro_sym();
}

#ifdef _XBOX1
#include "../../xbox1/RetroLaunch/Global.h"
#include "../../xbox1/RetroLaunch/Video.h"
#include "../../xbox1/RetroLaunch/IniFile.h"
#include "../../xbox1/RetroLaunch/IoSupport.h"
#include "../../xbox1/RetroLaunch/Input.h"
#include "../../xbox1/RetroLaunch/Debug.h"
#include "../../xbox1/RetroLaunch/Font.h"
#include "../../xbox1/RetroLaunch/MenuManager.h"
#include "../../xbox1/RetroLaunch/RomList.h"

bool g_bExit = false;

static void menu_init(void)
{
	g_debug.Print("Starting RetroLaunch\n");

	// Set file cache size
	XSetFileCacheSize(8 * 1024 * 1024);

	// Mount drives
	g_IOSupport.Mount("A:", "cdrom0");
	g_IOSupport.Mount("E:", "Harddisk0\\Partition1");
	g_IOSupport.Mount("Z:", "Harddisk0\\Partition2");
	g_IOSupport.Mount("F:", "Harddisk0\\Partition6");
	g_IOSupport.Mount("G:", "Harddisk0\\Partition7");


	// Initialize Direct3D
	if (!g_video.Create(NULL, false))
		return;

	// Parse ini file for settings
	g_iniFile.CheckForIniEntry();

	// Load the rom list if it isn't already loaded
	if (!g_romList.IsLoaded()) {
		g_romList.Load();
	}

	// Init input here
	g_input.Create();

	// Load the font here
	g_font.Create();

	// Build menu here (Menu state -> Main Menu)
	g_menuManager.Create();
}

static void menu_free(void) {}
static void menu_loop(void)
{
   //rarch_console_load_game("D:\\ssf2x.gba");
	// Loop the app
	while (!g_bExit)
	{
		g_video.BeginRender();
		g_input.GetInput();
		g_menuManager.Update();
		g_video.EndRender();
	}
}
#endif

int main(int argc, char *argv[])
{
   rarch_main_clear_state();
   get_environment_settings();

   config_set_defaults();
   
#ifdef _XBOX1
   configure_libretro("D:\\", ".xbe");
#else
   configure_libretro("game:\\", ".xex");
#endif

#if defined(HAVE_D3D8) || defined(HAVE_D3D9)
   video_xdk_d3d.start();
#else
   video_null.start();
#endif
   input_xinput.init();

   rarch_input_set_default_keybind_names_for_emulator();

   menu_init();

begin_loop:
   if(g_console.mode_switch == MODE_EMULATION)
   {
      bool repeat = false;

      input_xinput.poll(NULL);

      rarch_set_auto_viewport(g_extern.frame_cache.width, g_extern.frame_cache.height);

      do{
         repeat = rarch_main_iterate();
      }while(repeat && !g_console.frame_advance_enable);
   }
   else if(g_console.mode_switch == MODE_MENU)
   {
      menu_loop();
      rarch_startup(SYS_CONFIG_FILE);
   }
   else
      goto begin_shutdown;

   goto begin_loop;

begin_shutdown:
   if(path_file_exists(SYS_CONFIG_FILE))
      rarch_config_save(SYS_CONFIG_FILE);

   menu_free();
#if defined(HAVE_D3D8) || defined(HAVE_D3D9)
   video_xdk_d3d.stop();
#else
   video_null.stop();
#endif
   input_xinput.free(NULL);
   rarch_exec();

   return 0;
}

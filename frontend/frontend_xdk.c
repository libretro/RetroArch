/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#if defined(_XBOX360)
#include <xfilecache.h>
#include "../360/frontend-xdk/menu.h"
#include "../xdk/menu_shared.h"
#elif defined(_XBOX1)
#include "../xbox1/frontend/RetroLaunch/IoSupport.h"
#include "../console/rmenu/rmenu.h"
#endif

#include <xbdm.h>

#ifdef _XBOX
#include "../xdk/xdk_d3d.h"
#endif

#include "../console/rarch_console.h"
#include "../console/rarch_console_exec.h"
#include "../console/rarch_console_libretro_mgmt.h"
#include "../console/rarch_console_config.h"
#include "../conf/config_file.h"
#include "../conf/config_file_macros.h"
#include "../file.h"
#include "../general.h"

int rarch_main(int argc, char *argv[]);

#undef main

static void get_environment_settings (void)
{
   HRESULT ret;
   (void)ret;
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
      RARCH_LOG("RetroArch was launched as a standalone DVD, or using DVD emulation, or from the development area of the HDD.\n");
   }
   else
   {
      XContentQueryVolumeDeviceType("GAME",&g_extern.file_state.volume_device_type, NULL);

      switch(g_extern.file_state.volume_device_type)
      {
         case XCONTENTDEVICETYPE_HDD:
            RARCH_LOG("RetroArch was launched from a content package on HDD.\n");
	    break;
	 case XCONTENTDEVICETYPE_MU:
	    RARCH_LOG("RetroArch was launched from a content package on USB or Memory Unit.\n");
	    break;
	 case XCONTENTDEVICETYPE_ODD:
	    RARCH_LOG("RetroArch was launched from a content package on Optical Disc Drive.\n");
	    break;
	 default:
	    RARCH_LOG("RetroArch was launched from a content package on an unknown device type.\n");
	    break;
      }
   }
#endif

#if defined(_XBOX1)
   strlcpy(default_paths.core_dir, "D:", sizeof(default_paths.core_dir));
   strlcpy(g_extern.config_path, "D:\\retroarch.cfg", sizeof(g_extern.config_path));
   strlcpy(default_paths.system_dir, "D:\\system", sizeof(default_paths.system_dir));
   strlcpy(default_paths.filesystem_root_dir, "D:", sizeof(default_paths.filesystem_root_dir));
   strlcpy(default_paths.executable_extension, ".xbe", sizeof(default_paths.executable_extension));
   strlcpy(default_paths.filebrowser_startup_dir, "D:", sizeof(default_paths.filebrowser_startup_dir));
   strlcpy(default_paths.screenshots_dir, "D:\\screenshots", sizeof(default_paths.screenshots_dir));
   snprintf(default_paths.salamander_file, sizeof(default_paths.salamander_file), "default.xbe");
#elif defined(_XBOX360)
#ifdef HAVE_HDD_CACHE_PARTITION
   strlcpy(default_paths.cache_dir, "cache:\\", sizeof(default_paths.cache_dir));
#endif
   strlcpy(default_paths.filesystem_root_dir, "game:\\", sizeof(default_paths.filesystem_root_dir));
   strlcpy(default_paths.screenshots_dir, "game:", sizeof(default_paths.screenshots_dir));
   strlcpy(default_paths.shader_file, "game:\\media\\shaders\\stock.cg", sizeof(default_paths.shader_file));
   strlcpy(g_extern.config_path, "game:\\retroarch.cfg", sizeof(g_extern.config_path));
   strlcpy(default_paths.system_dir, "game:\\system", sizeof(default_paths.system_dir));
   strlcpy(default_paths.executable_extension, ".xex", sizeof(default_paths.executable_extension));
   strlcpy(default_paths.filebrowser_startup_dir, "game:", sizeof(default_paths.filebrowser_startup_dir));
   snprintf(default_paths.salamander_file, sizeof(default_paths.salamander_file), "default.xex");
#endif
}

static void system_init(void)
{
#ifdef _XBOX1
   // Mount drives
   xbox_io_mount("A:", "cdrom0");
   xbox_io_mount("C:", "Harddisk0\\Partition0");
   xbox_io_mount("E:", "Harddisk0\\Partition1");
   xbox_io_mount("Z:", "Harddisk0\\Partition2");
   xbox_io_mount("F:", "Harddisk0\\Partition6");
   xbox_io_mount("G:", "Harddisk0\\Partition7");
#endif
}

int main(int argc, char *argv[])
{
   rarch_main_clear_state();
   get_environment_settings();

   config_set_defaults();
   
   init_drivers_pre();
   driver.input->init();

#ifdef _XBOX1
   char path_prefix[256];
   snprintf(path_prefix, sizeof(path_prefix), "D:\\");
#else
   const char *path_prefix = default_paths.filesystem_root_dir;
#endif
   const char *extension = default_paths.executable_extension;

   char full_path[1024];
   snprintf(full_path, sizeof(full_path), "%sCORE%s", path_prefix, extension);

   rarch_settings_set_default();
   rarch_input_set_controls_default(driver.input);
   rarch_config_load();

#ifdef HAVE_LIBRETRO_MANAGEMENT
   if (rarch_configure_libretro_core(full_path, path_prefix, path_prefix, 
   g_extern.config_path, extension))
   {
      RARCH_LOG("New default libretro core saved to config file: %s.\n", g_settings.libretro);
      config_save_file(g_extern.config_path);
   }
#endif

   init_libretro_sym();

   driver.input->init();
   driver.video->start();

   system_init();

   menu_init();

begin_loop:
   if(g_extern.console.rmenu.mode == MODE_EMULATION)
   {

      driver.input->poll(NULL);
      driver.video->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);

      while(rarch_main_iterate());
   }
   else if (g_extern.console.rmenu.mode == MODE_INIT)
   {
      if(g_extern.main_is_init)
         rarch_main_deinit();

      struct rarch_main_wrap args = {0};

      args.verbose = g_extern.verbose;
      args.config_path = g_extern.config_path;
      args.sram_path = g_extern.console.main_wrap.state.default_sram_dir.enable ? g_extern.console.main_wrap.paths.default_sram_dir : NULL,
         args.state_path = g_extern.console.main_wrap.state.default_savestate_dir.enable ? g_extern.console.main_wrap.paths.default_savestate_dir : NULL,
         args.rom_path = g_extern.file_state.rom_path;
      args.libretro_path = g_settings.libretro;

      int init_ret = rarch_main_init_wrap(&args);

      if (init_ret == 0)
         RARCH_LOG("rarch_main_init succeeded.\n");
      else
         RARCH_ERR("rarch_main_init failed.\n");
   }
   else if(g_extern.console.rmenu.mode == MODE_MENU)
      while(rmenu_iterate());
   else
      goto begin_shutdown;

   goto begin_loop;

begin_shutdown:
   config_save_file(g_extern.config_path);

   menu_free();
   driver.video->stop();
   driver.input->free(NULL);

   if(g_extern.console.external_launch.enable)
      rarch_console_exec(g_extern.console.external_launch.launch_app);

   return 0;
}

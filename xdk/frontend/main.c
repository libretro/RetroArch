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

#include "../../xdk/menu_shared.h"

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

#include "../../console/rarch_console.h"
#include "../../console/rarch_console_config.h"
#include "../../console/rarch_console_main_wrap.h"
#include "../../conf/config_file.h"
#include "../../conf/config_file_macros.h"
#include "../../file.h"
#include "../../general.h"

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

#if defined(_XBOX1)
   /* FIXME: Hardcoded */
   strlcpy(default_paths.config_file, "D:\\retroarch.cfg", sizeof(default_paths.config_file));
   strlcpy(default_paths.system_dir, "D:\\system\\", sizeof(default_paths.system_dir));
   strlcpy(default_paths.filesystem_root_dir, "D:\\", sizeof(default_paths.filesystem_root_dir));
   strlcpy(default_paths.executable_extension, ".xbe", sizeof(default_paths.executable_extension));
#elif defined(_XBOX360)
#ifdef HAVE_HDD_CACHE_PARTITION
   strlcpy(default_paths.cache_dir, "cache:\\", sizeof(default_paths.cache_dir));
#endif
   strlcpy(default_paths.filesystem_root_dir, "game:\\", sizeof(default_paths.filesystem_root_dir));
   strlcpy(default_paths.shader_file, "game:\\media\\shaders\\stock.cg", sizeof(default_paths.shader_file));
   strlcpy(default_paths.config_file, "game:\\retroarch.cfg", sizeof(default_paths.config_file));
   strlcpy(default_paths.system_dir, "game:\\system\\", sizeof(default_paths.system_dir));
   strlcpy(default_paths.executable_extension, ".xex", sizeof(default_paths.executable_extension));
#endif
}

int main(int argc, char *argv[])
{
   rarch_main_clear_state();
   get_environment_settings();

   config_set_defaults();
   
   input_xinput.init();
   rarch_configure_libretro(&input_xinput, default_paths.filesystem_root_dir, default_paths.executable_extension);

   input_xinput.post_init();

#if defined(HAVE_D3D8) || defined(HAVE_D3D9)
   video_xdk_d3d.start();
#else
   video_null.start();
#endif

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
      rarch_startup(default_paths.config_file);
   }
   else
      goto begin_shutdown;

   goto begin_loop;

begin_shutdown:
   if(path_file_exists(default_paths.config_file))
      rarch_config_save(default_paths.config_file);

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

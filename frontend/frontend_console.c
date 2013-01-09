/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2013 - Daniel De Matteis
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include "../boolean.h"
#include <stddef.h>
#include <string.h>

//forward declarations
static void rarch_console_exec(const char *path);

#if defined(__CELLOS_LV2__)
#include "platform/platform_ps3.c"
#include "platform/platform_ps3_exec.c"
#elif defined(GEKKO)
#include "platform/platform_gx.c"
#include "platform/platform_gx_exec.c"
#elif defined(_XBOX)
#include "platform/platform_xdk.c"
#include "platform/platform_xdk_exec.c"
#elif defined(PSP)
#include "platform/platform_psp.c"
#endif

#undef main

#ifdef IS_SALAMANDER

int main(int argc, char *argv[])
{
   system_init();
   get_environment_settings(argc, argv);
   salamander_init_settings();
   system_deinit();
   system_exitspawn();

   return 1;
}

#else

// Only called once on init and deinit.
// Video and input drivers need to be active (owned)
// before retroarch core starts.
static void init_console_drivers(void)
{
   init_drivers_pre(); // Set driver.* function callbacks.
   driver.video->start(); // Statically starts video driver. Sets driver.video_data.
   driver.input_data = driver.input->init();
   rarch_input_set_controls_default(driver.input);
   driver.input->post_init();

   // Core handles audio.
}

static void uninit_console_drivers(void)
{
   if (driver.video_data)
   {
      driver.video->stop();
      driver.video_data = NULL;
   }

   if (driver.input_data)
   {
      driver.input->free(NULL);
      driver.input_data = NULL;
   }
}

int main(int argc, char *argv[])
{
   system_init();

   rarch_main_clear_state();

   get_environment_settings(argc, argv);
   config_set_defaults();
   rarch_settings_set_default();
   rarch_config_load();

   config_load();
   init_libretro_sym();
   rarch_init_system_info();

   init_console_drivers();

#ifdef HAVE_LIBRETRO_MANAGEMENT
   char core_exe_path[PATH_MAX];
   char path_prefix[PATH_MAX];
   const char *extension = default_paths.executable_extension;
#if defined(_XBOX1)
   snprintf(path_prefix, sizeof(path_prefix), "D:\\");
#elif defined(_XBOX360)
   snprintf(path_prefix, sizeof(path_prefix), default_paths.filesystem_root_dir);
#else
   snprintf(path_prefix, sizeof(path_prefix), "%s/", default_paths.core_dir);
#endif
   snprintf(core_exe_path, sizeof(core_exe_path), "%sCORE%s", path_prefix, extension);


   if (path_file_exists(core_exe_path))
   {
      if (rarch_libretro_core_install(core_exe_path, path_prefix, path_prefix, 
               g_extern.config_path, extension))
      {
         RARCH_LOG("New default libretro core saved to config file: %s.\n", g_settings.libretro);
         config_save_file(g_extern.config_path);
      }
   }
#endif

   init_libretro_sym();

   system_post_init();

   menu_init();

   system_process_args(argc, argv);

begin_loop:
   if(g_extern.console.rmenu.mode == MODE_EMULATION)
   {
      driver.input->poll(NULL);
      driver.video->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);
      audio_start_func();
      while(rarch_main_iterate());
      audio_stop_func();
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
      {
         RARCH_LOG("rarch_main_init succeeded.\n");
         g_extern.console.rmenu.mode = MODE_EMULATION;
      }
      else
      {
         RARCH_ERR("rarch_main_init failed.\n");
         g_extern.console.rmenu.mode = MODE_MENU;
         rarch_settings_msg(S_MSG_ROM_LOADING_ERROR, S_DELAY_180);
      }
   }
   else if(g_extern.console.rmenu.mode == MODE_MENU)
      while(rmenu_iterate());
   else
      goto begin_shutdown;

   goto begin_loop;

begin_shutdown:
   config_save_file(g_extern.config_path);

   system_deinit_save();

   if(g_extern.main_is_init)
      rarch_main_deinit();

   menu_free();
   uninit_console_drivers();

#ifdef PERF_TEST
   rarch_perf_log();
#endif

   system_deinit();
   system_exitspawn();

   return 1;
}

#endif

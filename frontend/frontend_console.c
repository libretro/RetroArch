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

#include "../config.def.h"
#include "menu/rmenu.h"

char input_path[1024];

#if defined(__CELLOS_LV2__)
#include "platform/platform_ps3_exec.c"
#include "platform/platform_ps3.c"
#elif defined(GEKKO)
#ifdef HW_RVL
#include "platform/platform_gx_exec.c"
#endif
#include "platform/platform_gx.c"
#elif defined(_XBOX)
#include "platform/platform_xdk_exec.c"
#include "platform/platform_xdk.c"
#elif defined(PSP)
#include "platform/platform_psp.c"
#endif

#undef main

default_paths_t default_paths;

// Rename core filename executable to a more sane name.

static bool libretro_install_core(const char *path_prefix,
      const char *core_exe_path)
{
   char old_path[PATH_MAX], new_path[PATH_MAX];

   libretro_get_current_core_pathname(old_path, sizeof(old_path));

   strlcat(old_path, DEFAULT_EXE_EXT, sizeof(old_path));
   snprintf(new_path, sizeof(new_path), "%s%s", path_prefix, old_path);

   /* If core already exists, we are upgrading the core - 
    * delete existing file first. */
   if (path_file_exists(new_path))
   {
      RARCH_LOG("Removing temporary ROM file: %s.\n", new_path);
      if (remove(new_path) < 0)
         RARCH_ERR("Failed to remove file: %s.\n", new_path);
   }

   /* Now attempt the renaming of the core. */
   RARCH_LOG("Renaming core to: %s.\n", new_path);
   if (rename(core_exe_path, new_path) < 0)
   {
      RARCH_ERR("Failed to rename core.\n");
      return false;
   }

   strlcpy(g_settings.libretro, new_path,
         sizeof(g_settings.libretro));

   return true;
}

int rarch_main(int argc, char *argv[])
{
   system_init();
#if defined(HAVE_LOGGER)
   g_extern.verbose = true;
   logger_init();
#elif defined(HAVE_FILE_LOGGER)
   g_extern.verbose = true;
   g_extern.log_file = fopen("/retroarch-log.txt", "w");
#endif

   rarch_main_clear_state();

   g_extern.verbose = true;

   get_environment_settings(argc, argv);
   config_load();

   /* FIXME - when dummy loading becomes possible perhaps change this param  */
   init_libretro_sym(false);
   rarch_init_system_info();

   global_init_drivers();

#ifdef HAVE_LIBRETRO_MANAGEMENT
   char path_prefix[PATH_MAX];
#if defined(_WIN32)
   char slash = '\\';
#else
   char slash = '/';
#endif

   snprintf(path_prefix, sizeof(path_prefix), "%s%c", default_paths.core_dir, slash);

   char core_exe_path[256];
   snprintf(core_exe_path, sizeof(core_exe_path), "%sCORE%s", path_prefix, DEFAULT_EXE_EXT);

   // Save new libretro core path to config file
   if (path_file_exists(core_exe_path))
      if (libretro_install_core(path_prefix, core_exe_path))
         config_save_file(g_extern.config_path);
#endif

#ifdef GEKKO
   /* Per-core input config loading */
   char core_name[64];

   libretro_get_current_core_pathname(core_name, sizeof(core_name));
   snprintf(input_path, sizeof(input_path), "%s/%s.cfg", default_paths.input_presets_dir, core_name);
   config_read_keybinds(input_path);
#endif

   menu_init();

   system_process_args(argc, argv);

   g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU);
   g_extern.lifecycle_mode_state |= (1ULL << MODE_INIT);

   for (;;)
   {
      if (g_extern.system.shutdown)
         break;
      else if (g_extern.lifecycle_mode_state & (1ULL << MODE_GAME))
      {
         driver.input->poll(NULL);

         if (driver.video_poke->set_aspect_ratio)
            driver.video_poke->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);

         if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_THROTTLE_ENABLE))
            audio_start_func();

         while(rarch_main_iterate());

         if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_THROTTLE_ENABLE))
            audio_stop_func();
         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_GAME);
      }
      else if (g_extern.lifecycle_mode_state & (1ULL << MODE_INIT))
      {
         if (g_extern.main_is_init)
            rarch_main_deinit();

         struct rarch_main_wrap args = {0};

         args.verbose = g_extern.verbose;
         args.config_path   = *g_extern.config_path ? g_extern.config_path : NULL;
         args.sram_path = (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE)) ? g_extern.console.main_wrap.default_sram_dir : NULL;
         args.state_path = (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE)) ? g_extern.console.main_wrap.default_savestate_dir : NULL;
         args.rom_path = *g_extern.fullpath ? g_extern.fullpath : NULL;
         args.libretro_path = g_settings.libretro;

         int init_ret = rarch_main_init_wrap(&args);
         if (init_ret == 0)
         {
            RARCH_LOG("rarch_main_init succeeded.\n");
            g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
         }
         else
         {
            RARCH_ERR("rarch_main_init failed.\n");
            g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU);
            msg_queue_push(g_extern.msg_queue, "ERROR - An error occurred during ROM loading.", 1, 180);
         }

         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_INIT);
      }
      else if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU))
      {
         g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_PREINIT);
         while (!g_extern.system.shutdown && menu_iterate());
         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU);
      }
      else
         break;
   }

   g_extern.system.shutdown = false;

   menu_free();

   config_save_file(g_extern.config_path);

#ifdef GEKKO
   /* Per-core input config saving */
   config_save_keybinds(input_path);
#endif

   if (g_extern.main_is_init)
      rarch_main_deinit();

   global_uninit_drivers();

#ifdef PERF_TEST
   rarch_perf_log();
#endif

#if defined(HAVE_LOGGER)
   logger_shutdown();
#elif defined(HAVE_FILE_LOGGER)
   if (g_extern.log_file)
      fclose(g_extern.log_file);
   g_extern.log_file = NULL;
#endif

   system_deinit();

   if (g_extern.lifecycle_mode_state & (1ULL << MODE_EXITSPAWN))
      system_exitspawn();

   return 1;
}

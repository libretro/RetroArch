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

#include "frontend_console.h"
#include "menu/rmenu.h"
#include "menu/menu_settings.h"

#if defined(__CELLOS_LV2__)
#include "platform/platform_ps3.c"
#include "platform/platform_ps3_exec.c"
#elif defined(GEKKO)
#include "platform/platform_gx.c"
#ifdef HW_RVL
#include "platform/platform_gx_exec.c"
#endif
#elif defined(_XBOX)
#include "platform/platform_xdk.c"
#include "platform/platform_xdk_exec.c"
#elif defined(PSP)
#include "platform/platform_psp.c"
#endif

#undef main

default_paths_t default_paths;

#ifdef IS_SALAMANDER

//We need to set libretro to the first entry in the cores
//directory so that it will be saved to the config file
static void find_first_libretro_core(char *first_file,
   size_t size_of_first_file, const char *dir,
   const char * ext)
{
   bool ret = false;

   RARCH_LOG("Searching for valid libretro implementation in: \"%s\".\n", dir);

   struct string_list *list = dir_list_new(dir, ext, false);
   if (!list)
   {
      RARCH_ERR("Couldn't read directory. Cannot infer default libretro core.\n");
      return;
   }
   
   for (size_t i = 0; i < list->size && !ret; i++)
   {
      RARCH_LOG("Checking library: \"%s\".\n", list->elems[i].data);
      const char * libretro_elem = list->elems[i].data;

      if (libretro_elem)
      {
         char fname[PATH_MAX];
         fill_pathname_base(fname, libretro_elem, sizeof(fname));

         if (strncmp(fname, default_paths.salamander_file, sizeof(fname)) == 0)
         {
            if ((i + 1) == list->size)
            {
               RARCH_WARN("Entry is RetroArch Salamander itself, but is last entry. No choice but to set it.\n");
               strlcpy(first_file, fname, size_of_first_file);
            }

            continue;
         }

         strlcpy(first_file, fname, size_of_first_file);
         RARCH_LOG("First found libretro core is: \"%s\".\n", first_file);
         ret = true;
      }
   }

   dir_list_free(list);
}

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

static void verbose_log_init(void)
{
   if (g_extern.verbose)
      return;

   g_extern.verbose = true;
   RARCH_LOG("Turning on verbose logging...\n");
}

// Transforms a library id to a name suitable as a pathname.
static void get_libretro_core_name(char *name, size_t size)
{
   if (size == 0)
      return;

   struct retro_system_info info;
   retro_get_system_info(&info);
   const char *id = info.library_name ? info.library_name : "Unknown";

   if (!id || strlen(id) >= size)
   {
      name[0] = '\0';
      return;
   }

   name[strlen(id)] = '\0';

   for (size_t i = 0; id[i] != '\0'; i++)
   {
      char c = id[i];
      if (isspace(c) || isblank(c))
         name[i] = '_';
      else
         name[i] = tolower(c);
   }
}

#ifdef HAVE_LIBRETRO_MANAGEMENT

// If a CORE executable of name CORE.extension exists, rename filename
// to a more sane name.
static bool install_libretro_core(const char *core_exe_path, const char *tmp_path, const char *extension)
{
   int ret = 0;
   char tmp_path2[PATH_MAX], tmp_pathnewfile[PATH_MAX];

   get_libretro_core_name(tmp_path2, sizeof(tmp_path2));

   strlcat(tmp_path2, extension, sizeof(tmp_path2));
   snprintf(tmp_pathnewfile, sizeof(tmp_pathnewfile), "%s%s", tmp_path, tmp_path2);

   if (path_file_exists(tmp_pathnewfile))
   {
      // If core already exists, we are upgrading the core -
      // delete existing file first.

      RARCH_LOG("Upgrading emulator core...\n");
      ret = remove(tmp_pathnewfile);

      if (ret == 0)
         RARCH_LOG("Succeeded in removing pre-existing libretro core: [%s].\n", tmp_pathnewfile);
      else
         RARCH_ERR("Failed to remove pre-existing libretro core: [%s].\n", tmp_pathnewfile);
   }

   // Now attempt the renaming of the core.
   ret = rename(core_exe_path, tmp_pathnewfile);

   if (ret == 0)
   {
      RARCH_LOG("Libretro core [%s] successfully renamed to: [%s].\n", core_exe_path, tmp_pathnewfile);
      strlcpy(g_settings.libretro, tmp_pathnewfile, sizeof(g_settings.libretro));
   }
   else
   {
      RARCH_ERR("Failed to rename CORE executable.\n");
      RARCH_WARN("CORE executable was not found, or some other error occurred. Will attempt to load libretro core path from config file.\n");
      return false;
   }

   return true;
}


#endif

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

int rarch_main(int argc, char *argv[])
{
   system_init();

   rarch_main_clear_state();

   verbose_log_init();

   get_environment_settings(argc, argv);
   config_load();

   init_libretro_sym();
   rarch_init_system_info();

   init_console_drivers();

#ifdef HAVE_LIBRETRO_MANAGEMENT
   char core_exe_path[PATH_MAX];
   char path_prefix[PATH_MAX];
   const char *extension = default_paths.executable_extension;
   char slash;
#if defined(_WIN32)
   slash = '\\';
#else
   slash = '/';
#endif

   snprintf(path_prefix, sizeof(path_prefix), "%s%c", default_paths.core_dir, slash);
   snprintf(core_exe_path, sizeof(core_exe_path), "%sCORE%s", path_prefix, extension);

   if (path_file_exists(core_exe_path))
   {
      RARCH_LOG("core_exe_path: %s\n", core_exe_path);
      if (install_libretro_core(core_exe_path, path_prefix, extension))
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
   if(g_extern.lifecycle_mode_state & (1ULL << MODE_GAME))
   {
      driver.input->poll(NULL);
      driver.video->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);

      if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_THROTTLE_ENABLE))
         audio_start_func();

      while(rarch_main_iterate());

      if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_THROTTLE_ENABLE))
         audio_stop_func();
      g_extern.lifecycle_mode_state &= ~(1ULL << MODE_GAME);
   }
   else if (g_extern.lifecycle_mode_state & (1ULL << MODE_INIT))
   {
      if(g_extern.main_is_init)
         rarch_main_deinit();

      struct rarch_main_wrap args = {0};

      args.verbose = g_extern.verbose;
      args.sram_path = (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE)) ? g_extern.console.main_wrap.default_sram_dir : NULL;
      args.state_path = (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE)) ? g_extern.console.main_wrap.default_savestate_dir : NULL;
      args.rom_path = g_extern.fullpath;
      args.libretro_path = g_settings.libretro;

      if (path_file_exists(g_extern.config_path))
         args.config_path = g_extern.config_path;
      else
         args.config_path = NULL;

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
         menu_settings_msg(S_MSG_ROM_LOADING_ERROR, 180);
      }
      g_extern.lifecycle_mode_state &= ~(1ULL << MODE_INIT);
   }
   else if(g_extern.lifecycle_mode_state & (1ULL << MODE_MENU))
   {
      g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_PREINIT);
      while (menu_iterate());
      g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU);
   }
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
   if (g_extern.lifecycle_mode_state & (1ULL << MODE_EXITSPAWN))
      system_exitspawn();

   return 1;
}

#endif

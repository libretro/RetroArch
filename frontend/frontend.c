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

#include "../general.h"
#include "../conf/config_file.h"
#include "../file.h"

#include "frontend_context.h"
frontend_ctx_driver_t *frontend_ctx;

#if defined(HAVE_RGUI) || defined(HAVE_RMENU) || defined(HAVE_RMENU_XUI)
#define HAVE_MENU
#include "menu/menu_common.h"
#else
#undef HAVE_MENU
#endif

#include "../file_ext.h"

#ifdef RARCH_CONSOLE
#include "../config.def.h"

default_paths_t default_paths;

static void rarch_get_environment_console(void)
{
   path_mkdir(default_paths.port_dir);
   path_mkdir(default_paths.system_dir);
   path_mkdir(default_paths.savestate_dir);
   path_mkdir(default_paths.sram_dir);

   config_load();

   init_libretro_sym(false);
   rarch_init_system_info();

#ifdef HAVE_LIBRETRO_MANAGEMENT
   char basename[PATH_MAX];
   char basename_new[PATH_MAX];
   char old_path[PATH_MAX];
   char new_path[PATH_MAX];

   strlcpy(basename, "CORE", sizeof(basename));
   strlcat(basename, DEFAULT_EXE_EXT, sizeof(basename));
   fill_pathname_join(old_path, default_paths.core_dir, basename, sizeof(old_path));

   libretro_get_current_core_pathname(basename_new, sizeof(basename_new));
   strlcat(basename_new, DEFAULT_EXE_EXT, sizeof(basename_new));
   fill_pathname_join(new_path, default_paths.core_dir, basename_new, sizeof(new_path));

   if (path_file_exists(old_path))
   {
      // Rename core filename executable (old_path) to a more sane name (new_path).

      if (path_file_exists(new_path))
      {
         /* If new_path already exists, we are upgrading the core - 
          * delete existing file first. */

         if (remove(new_path) < 0)
            RARCH_ERR("Failed to remove file: %s.\n", new_path);
         else
            RARCH_LOG("Removed temporary ROM file: %s.\n", new_path);
      }

      /* Now attempt the renaming of the core. */
      if (rename(old_path, new_path) < 0)
         RARCH_ERR("Failed to rename core.\n");
      else
      {
         rarch_environment_cb(RETRO_ENVIRONMENT_SET_LIBRETRO_PATH, (void*)new_path);
         RARCH_LOG("Renamed core successfully to: %s.\n", new_path);
      }
   }
#endif

   global_init_drivers();
}
#endif

#if defined(ANDROID)
#define main_entry android_app_entry
#define returntype void
#define signature() void* data
#define returnfunc() exit(0)
#define return_negative() return
#define return_var(var) return
#define declare_argc() int argc = 0;
#define declare_argv() char *argv[1]
#define args_type() struct android_app*
#define args_initial_ptr() data
#elif defined(IOS) || defined(OSX) || defined(HAVE_BB10)
#define main_entry rarch_main
#define returntype int
#define signature() int argc, char *argv[]
#define returnfunc() return 0
#define return_negative() return 1
#define return_var(var) return var
#define declare_argc()
#define declare_argv()
#define args_type() void*
#define args_initial_ptr() NULL
#else
#define main_entry main
#define returntype int
#define signature() int argc, char *argv[]
#define returnfunc() return 0
#define return_negative() return 1
#define return_var(var) return var
#define declare_argc()
#define declare_argv()
#define args_type() void*
#define args_initial_ptr() NULL
#endif

#if defined(HAVE_BB10) || defined(ANDROID)
#define ra_preinited true
#else
#define ra_preinited false
#endif

#if defined(HAVE_BB10) || defined(RARCH_CONSOLE)
#define attempt_load_game false
#else
#define attempt_load_game true
#endif

#if defined(RARCH_CONSOLE) || defined(HAVE_BB10) || defined(ANDROID)
#define initial_menu_lifecycle_state (1ULL << MODE_LOAD_GAME)
#else
#define initial_menu_lifecycle_state (1ULL << MODE_GAME)
#endif

#if !defined(RARCH_CONSOLE) && !defined(HAVE_BB10) && !defined(ANDROID)
#define attempt_load_game_push_history false
#else
#define attempt_load_game_push_history true
#endif

#ifndef RARCH_CONSOLE
#define rarch_get_environment_console() (void)0
#endif

#if defined(RARCH_CONSOLE) || defined(__QNX__) || defined(ANDROID)
#define attempt_load_game_fails (1ULL << MODE_MENU)
#else
#define attempt_load_game_fails (1ULL << MODE_EXIT)
#endif

returntype main_entry(signature())
{
   declare_argc();
   declare_argv();
   args_type() args = (args_type())args_initial_ptr();
   unsigned i;
   frontend_ctx = (frontend_ctx_driver_t*)frontend_ctx_init_first();

   if (frontend_ctx && frontend_ctx->init)
      frontend_ctx->init(args);

   if (!ra_preinited)
   {
      rarch_main_clear_state();
      rarch_init_msg_queue();
   }

   if (frontend_ctx && frontend_ctx->environment_get)
   {
      frontend_ctx->environment_get(argc, argv, args);
      rarch_get_environment_console();
   }

   if (attempt_load_game)
   {
      int init_ret;
      if ((init_ret = rarch_main_init(argc, argv))) return_var(init_ret);
   }

#if defined(HAVE_MENU) || defined(HAVE_BB10)
   menu_init();

   if (frontend_ctx && frontend_ctx->process_args)
      frontend_ctx->process_args(argc, argv, args);

   g_extern.lifecycle_mode_state |= initial_menu_lifecycle_state;

   if (attempt_load_game_push_history)
   {
      // If we started a ROM directly from command line,
      // push it to ROM history.
      if (!g_extern.libretro_dummy)
         menu_rom_history_push_current();
   }

   for (;;)
   {
      if (g_extern.system.shutdown)
         break;
      else if (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME))
      {
         load_menu_game_prepare();

         // If ROM load fails, we exit RetroArch. On console it might make more sense to go back to menu though ...
         if (load_menu_game())
            g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
         else
         {
            g_extern.lifecycle_mode_state = attempt_load_game_fails;

            if (g_extern.lifecycle_mode_state & (1ULL << MODE_EXIT))
            {
               if (frontend_ctx && frontend_ctx->shutdown)
                  frontend_ctx->shutdown(true);

               return_negative();
            }
         }

         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_GAME);
      }
      else if (g_extern.lifecycle_mode_state & (1ULL << MODE_GAME))
      {
         if (driver.video_poke->set_aspect_ratio)
            driver.video_poke->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);

         while ((g_extern.is_paused && !g_extern.is_oneshot) ? rarch_main_idle_iterate() : rarch_main_iterate())
         {
            if (frontend_ctx && frontend_ctx->process_events)
               frontend_ctx->process_events(args);

            if (!(g_extern.lifecycle_mode_state & (1ULL << MODE_GAME)))
               break;
         }
         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_GAME);
      }
      else if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU))
      {
         g_extern.lifecycle_mode_state |= 1ULL << MODE_MENU_PREINIT;
         // Menu should always run with vsync on.
         video_set_nonblock_state_func(false);
         // Stop all rumbling when entering RGUI.
         for (i = 0; i < MAX_PLAYERS; i++)
         {
            driver_set_rumble_state(i, RETRO_RUMBLE_STRONG, 0);
            driver_set_rumble_state(i, RETRO_RUMBLE_WEAK, 0);
         }

         // Override keyboard callback to redirect to menu instead.
         // We'll use this later for something ...
         // FIXME: This should probably be moved to menu_common somehow.
         retro_keyboard_event_t key_event = g_extern.system.key_event;
         g_extern.system.key_event = menu_key_event;

         if (driver.audio_data)
            audio_stop_func();

         while (!g_extern.system.shutdown && menu_iterate())
         {
            if (frontend_ctx && frontend_ctx->process_events)
               frontend_ctx->process_events(args);

            if (!(g_extern.lifecycle_mode_state & (1ULL << MODE_MENU)))
               break;
         }

         driver_set_nonblock_state(driver.nonblock_state);

         if (driver.audio_data && !g_extern.audio_data.mute && !audio_start_func())
         {
            RARCH_ERR("Failed to resume audio driver. Will continue without audio.\n");
            g_extern.audio_active = false;
         }

         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU);

         // Restore libretro keyboard callback.
         g_extern.system.key_event = key_event;
      }
      else
         break;
   }

   g_extern.system.shutdown = false;
   menu_free();

   if (g_extern.config_save_on_exit && *g_extern.config_path)
      config_save_file(g_extern.config_path);
#else
   while ((g_extern.is_paused && !g_extern.is_oneshot) ? rarch_main_idle_iterate() : rarch_main_iterate());
#endif

   rarch_main_deinit();
   rarch_deinit_msg_queue();
   global_uninit_drivers();

#ifdef PERF_TEST
   rarch_perf_log();
#endif

#if defined(HAVE_LOGGER) && !defined(ANDROID)
   logger_shutdown();
#elif defined(HAVE_FILE_LOGGER)
   if (g_extern.log_file)
      fclose(g_extern.log_file);
   g_extern.log_file = NULL;
#endif

   if (frontend_ctx && frontend_ctx->deinit)
      frontend_ctx->deinit(args);

   if (g_extern.lifecycle_mode_state & (1ULL << MODE_EXITSPAWN) && frontend_ctx
         && frontend_ctx->exitspawn)
      frontend_ctx->exitspawn();

   rarch_main_clear_state();

   if (frontend_ctx && frontend_ctx->shutdown)
      frontend_ctx->shutdown(false);

   returnfunc();
}

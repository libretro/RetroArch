/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2014 - Daniel De Matteis
 * Copyright (C) 2012-2014 - Michael Lelli
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

#include "frontend.h"
#include "../general.h"
#include "../conf/config_file.h"
#include "../file.h"

#include "frontend_context.h"

#if defined(HAVE_MENU)
#include "menu/menu_input_line_cb.h"
#include "menu/menu_common.h"
#endif

#include "../file_ext.h"

#if defined(RARCH_CONSOLE) || defined(__QNX__)
#include "../config.def.h"

#ifdef RARCH_CONSOLE
default_paths_t default_paths;
#endif
#endif

#if defined(ANDROID)

#define main_entry android_app_entry
#define returntype void
#define signature_expand() data
#define returnfunc() exit(0)
#define return_negative() return
#define return_var(var) return
#define declare_argc() int argc = 0;
#define declare_argv() char *argv[1]
#define args_initial_ptr() data
#else

#if defined(__APPLE__) || defined(HAVE_BB10)
#define main_entry rarch_main
#elif defined(EMSCRIPTEN)
#define main_entry _fakemain
#else
#define main_entry main
#endif

#define returntype int
#define signature_expand() argc, argv
#define returnfunc() return 0
#define return_negative() return 1
#define return_var(var) return var
#define declare_argc()
#define declare_argv()
#define args_initial_ptr() NULL

#endif

#if !defined(RARCH_CONSOLE) && !defined(__QNX__) && !defined(ANDROID)
#define attempt_load_game_push_history true
#else
#define attempt_load_game_push_history false
#endif

static retro_keyboard_event_t key_event;

#ifdef HAVE_MENU
static int main_entry_iterate_clear_input(args_type() args)
{
   (void)args;

   rarch_input_poll();
   if (!menu_input())
   {
      // Restore libretro keyboard callback.
      g_extern.system.key_event = key_event;

      g_extern.lifecycle_state &= ~(1ULL << MODE_CLEAR_INPUT);
   }

   return 0;
}

static int main_entry_iterate_shutdown(args_type() args)
{
   (void)args;

#ifdef HAVE_MENU
   // Load dummy core instead of exiting RetroArch completely.
   if (g_settings.load_dummy_on_core_shutdown)
      load_menu_game_prepare_dummy();
   else
#endif
      return 1;

   return 0;
}


static int main_entry_iterate_content(args_type() args)
{
   bool r;
   if (g_extern.is_paused && !g_extern.is_oneshot)
      r = rarch_main_idle_iterate();
   else
      r = rarch_main_iterate();

   if (r)
   {
      if (driver.frontend_ctx && driver.frontend_ctx->process_events)
         driver.frontend_ctx->process_events(args);
   }
   else
      g_extern.lifecycle_state &= ~(1ULL << MODE_GAME);

   return 0;
}

static int main_entry_iterate_load_content(args_type() args)
{
   load_menu_game_prepare();

   if (load_menu_game())
   {
      g_extern.lifecycle_state |= (1ULL << MODE_GAME);
      if (driver.video_data && driver.video_poke && driver.video_poke->set_aspect_ratio)
         driver.video_poke->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);
   }
   else
   {
#if defined(RARCH_CONSOLE) || defined(RARCH_MOBILE)
      // If ROM load fails, we go back to menu.
      g_extern.lifecycle_state = (1ULL << MODE_MENU_PREINIT);
#else
      // If ROM load fails, we exit RetroArch.
      g_extern.system.shutdown = true;
#endif
   }

   g_extern.lifecycle_state &= ~(1ULL << MODE_LOAD_GAME);

   return 0;
}

static int main_entry_iterate_menu_preinit(args_type() args)
{
   int i;
   rgui_handle_t *rgui = (rgui_handle_t*)driver.menu;

   if (!rgui)
      return 1;

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
   key_event = g_extern.system.key_event;
   g_extern.system.key_event = menu_key_event;

   if (driver.audio_data)
      audio_stop_func();

   rgui->need_refresh = true;
   rgui->old_input_state |= 1ULL << RARCH_MENU_TOGGLE;

   g_extern.lifecycle_state &= ~(1ULL << MODE_MENU_PREINIT);
   g_extern.lifecycle_state |= (1ULL << MODE_MENU);

   return 0;
}

static int main_entry_iterate_menu(args_type() args)
{
   if (menu_iterate())
   {
      if (driver.frontend_ctx && driver.frontend_ctx->process_events)
         driver.frontend_ctx->process_events(args);
   }
   else
   {
      g_extern.lifecycle_state &= ~(1ULL << MODE_MENU);
      driver_set_nonblock_state(driver.nonblock_state);

      if (driver.audio_data && !g_extern.audio_data.mute && !audio_start_func())
      {
         RARCH_ERR("Failed to resume audio driver. Will continue without audio.\n");
         g_extern.audio_active = false;
      }

      g_extern.lifecycle_state |= (1ULL << MODE_CLEAR_INPUT);

      // If QUIT state came from command interface, we'll only see it once due to MODE_CLEAR_INPUT.
      if (input_key_pressed_func(RARCH_QUIT_KEY) || !video_alive_func())
         return 1;
   }

   return 0;
}

int main_entry_iterate(signature(), args_type() args)
{
   if (g_extern.system.shutdown)
      return main_entry_iterate_shutdown(args);
   else if (g_extern.lifecycle_state & (1ULL << MODE_CLEAR_INPUT))
      return main_entry_iterate_clear_input(args);
   else if (g_extern.lifecycle_state & (1ULL << MODE_LOAD_GAME))
      return main_entry_iterate_load_content(args);
   else if (g_extern.lifecycle_state & (1ULL << MODE_GAME))
      return main_entry_iterate_content(args);
#ifdef HAVE_MENU
   else if (g_extern.lifecycle_state & (1ULL << MODE_MENU_PREINIT))
      main_entry_iterate_menu_preinit(args);
   else if (g_extern.lifecycle_state & (1ULL << MODE_MENU))
      return main_entry_iterate_menu(args);
#endif
   else
      return 1;

   return 0;
}
#endif


void main_exit(args_type() args)
{
   g_extern.system.shutdown = false;

   if (g_extern.config_save_on_exit && *g_extern.config_path)
   {
      // save last core-specific config to the default config location, needed on
      // consoles for core switching and reusing last good config for new cores.
      config_save_file(g_extern.config_path);

      // Flush out the core specific config.
      if (*g_extern.core_specific_config_path && g_settings.core_specific_config)
         config_save_file(g_extern.core_specific_config_path);
   }

   if (g_extern.main_is_init)
      rarch_main_deinit();
   rarch_deinit_msg_queue();

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

   if (driver.frontend_ctx && driver.frontend_ctx->deinit)
      driver.frontend_ctx->deinit(args);

   if (g_extern.lifecycle_state & (1ULL << MODE_EXITSPAWN) && driver.frontend_ctx
         && driver.frontend_ctx->exitspawn)
      driver.frontend_ctx->exitspawn();

   rarch_main_clear_state();

   if (driver.frontend_ctx && driver.frontend_ctx->shutdown)
      driver.frontend_ctx->shutdown(false);
}

returntype main_entry(signature())
{
   declare_argc();
   declare_argv();
   args_type() args = (args_type())args_initial_ptr();

   driver.frontend_ctx = (frontend_ctx_driver_t*)frontend_ctx_init_first();

   if (!driver.frontend_ctx)
   {
      RARCH_WARN("Frontend context could not be initialized.\n");
   }

   if (driver.frontend_ctx && driver.frontend_ctx->init)
      driver.frontend_ctx->init(args);

   rarch_main_clear_state();

   if (driver.frontend_ctx && driver.frontend_ctx->environment_get)
   {
      driver.frontend_ctx->environment_get(argc, argv, args);
#if defined(RARCH_CONSOLE) || defined(__QNX__)
      if (*default_paths.autoconfig_dir)
         path_mkdir(default_paths.autoconfig_dir);
      if (*default_paths.assets_dir)
         path_mkdir(default_paths.assets_dir);
      if (*default_paths.core_dir)
         path_mkdir(default_paths.core_dir);
      if (*default_paths.core_info_dir)
         path_mkdir(default_paths.core_info_dir);
      if (*default_paths.overlay_dir)
         path_mkdir(default_paths.overlay_dir);
      if (*default_paths.port_dir)
         path_mkdir(default_paths.port_dir);
      if (*default_paths.shader_dir)
         path_mkdir(default_paths.shader_dir);
      if (*default_paths.savestate_dir)
         path_mkdir(default_paths.savestate_dir);
      if (*default_paths.sram_dir)
         path_mkdir(default_paths.sram_dir);
      if (*default_paths.system_dir)
         path_mkdir(default_paths.system_dir);
#endif
   }

   {
      int init_ret;
      if ((init_ret = rarch_main_init(argc, argv))) return_var(init_ret);
   }

#if defined(HAVE_MENU)
   if (driver.frontend_ctx && driver.frontend_ctx->process_args)
      driver.frontend_ctx->process_args(argc, argv, args);

   g_extern.lifecycle_state |= (1ULL << MODE_GAME);

   if (attempt_load_game_push_history)
   {
      // If we started a ROM directly from command line,
      // push it to ROM history.
      if (!g_extern.libretro_dummy)
         menu_rom_history_push_current();
   }

   while (!main_entry_iterate(signature_expand(), args));
#else
   while ((g_extern.is_paused && !g_extern.is_oneshot) ? rarch_main_idle_iterate() : rarch_main_iterate());
#endif

   main_exit(args);

   returnfunc();
}

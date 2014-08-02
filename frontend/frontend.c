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

#if defined(RARCH_CONSOLE) || defined(RARCH_MOBILE)
#include "../config.def.h"
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

#if defined(__APPLE__) || defined(HAVE_BB10) || defined(EMSCRIPTEN)
#define main_entry rarch_main
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

#if !defined(__APPLE__) && !defined(EMSCRIPTEN)
#define HAVE_MAIN_LOOP
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

   if (!g_settings.load_dummy_on_core_shutdown)
      return 1;

   // Load dummy core instead of exiting RetroArch completely.
   rarch_main_command(RARCH_CMD_PREPARE_DUMMY);

   return 0;
}


static int main_entry_iterate_content(args_type() args)
{
   if (rarch_main_iterate())
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
   // If ROM load fails, we go back to menu.
   if (!load_menu_content())
      g_extern.lifecycle_state = (1ULL << MODE_MENU_PREINIT);

   g_extern.lifecycle_state &= ~(1ULL << MODE_LOAD_GAME);

   return 0;
}

static int main_entry_iterate_menu_preinit(args_type() args)
{
   int i;

   if (!driver.menu)
      return 1;

   // Menu should always run with vsync on.
   video_set_nonblock_state_func(false);
   // Stop all rumbling when entering the menu.
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

   rarch_main_command(RARCH_CMD_AUDIO_STOP);

   driver.menu->need_refresh = true;
   driver.menu->old_input_state |= 1ULL << RARCH_MENU_TOGGLE;

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

      rarch_main_command(RARCH_CMD_AUDIO_START);

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
   {
      driver.menu_data_own = false; // Do not want menu context to live any more.
      rarch_main_deinit();
   }

   rarch_deinit_msg_queue();

   rarch_perf_log();

#if defined(HAVE_LOGGER) && !defined(ANDROID)
   logger_shutdown();
#endif

   if (driver.frontend_ctx && driver.frontend_ctx->deinit)
      driver.frontend_ctx->deinit(args);

   if (g_extern.lifecycle_state & (1ULL << MODE_EXITSPAWN) && driver.frontend_ctx
         && driver.frontend_ctx->exitspawn)
      driver.frontend_ctx->exitspawn(g_settings.libretro, sizeof(g_settings.libretro));

   rarch_main_clear_state();

   if (driver.frontend_ctx && driver.frontend_ctx->shutdown)
      driver.frontend_ctx->shutdown(false);
}

static void check_defaults_dirs(void)
{
   if (*g_defaults.autoconfig_dir)
      path_mkdir(g_defaults.autoconfig_dir);
   if (*g_defaults.audio_filter_dir)
      path_mkdir(g_defaults.audio_filter_dir);
   if (*g_defaults.assets_dir)
      path_mkdir(g_defaults.assets_dir);
   if (*g_defaults.core_dir)
      path_mkdir(g_defaults.core_dir);
   if (*g_defaults.core_info_dir)
      path_mkdir(g_defaults.core_info_dir);
   if (*g_defaults.overlay_dir)
      path_mkdir(g_defaults.overlay_dir);
   if (*g_defaults.port_dir)
      path_mkdir(g_defaults.port_dir);
   if (*g_defaults.shader_dir)
      path_mkdir(g_defaults.shader_dir);
   if (*g_defaults.savestate_dir)
      path_mkdir(g_defaults.savestate_dir);
   if (*g_defaults.sram_dir)
      path_mkdir(g_defaults.sram_dir);
   if (*g_defaults.system_dir)
      path_mkdir(g_defaults.system_dir);
}

bool main_load_content(int argc, char **argv, args_type() args, environment_get_t environ_get,
      process_args_t process_args)
{
   bool retval = true;
   int *rarch_argc_ptr;
   char **rarch_argv_ptr;
   struct rarch_main_wrap *wrap_args;
   int i, ret, rarch_argc = 0;
   char *rarch_argv[MAX_ARGS] = {NULL};
   char *argv_copy[MAX_ARGS] = {NULL};

   rarch_argv_ptr = (char**)argv;
   rarch_argc_ptr = (int*)&argc;

   (void)rarch_argc_ptr;
   (void)rarch_argv_ptr;

   wrap_args = (struct rarch_main_wrap*)calloc(1, sizeof(*wrap_args));
   rarch_assert(wrap_args);

   if (environ_get)
      environ_get(rarch_argc_ptr, rarch_argv_ptr, args, wrap_args);

   check_defaults_dirs();

   if (wrap_args->touched)
   {
      rarch_main_init_wrap(wrap_args, &rarch_argc, rarch_argv);
      memcpy(argv_copy, rarch_argv, sizeof(rarch_argv));
      rarch_argv_ptr = (char**)rarch_argv;
      rarch_argc_ptr = (int*)&rarch_argc;
   }

   if (g_extern.main_is_init)
      rarch_main_deinit();

   if ((ret = rarch_main_init(*rarch_argc_ptr, rarch_argv_ptr)))
   {
      retval = false;
      goto error;
   }

   g_extern.lifecycle_state |= (1ULL << MODE_GAME);

   if (process_args)
      process_args(rarch_argc_ptr, rarch_argv_ptr);

error:
   for (i = 0; i < ARRAY_SIZE(argv_copy); i++)
      free(argv_copy[i]);
   free(wrap_args);
   return retval;
}

returntype main_entry(signature())
{
   declare_argc();
   declare_argv();
   args_type() args = (args_type())args_initial_ptr();
   int ret = 0;

   driver.frontend_ctx = (frontend_ctx_driver_t*)frontend_ctx_init_first();

   if (!driver.frontend_ctx)
      RARCH_WARN("Frontend context could not be initialized.\n");

   if (driver.frontend_ctx && driver.frontend_ctx->init)
      driver.frontend_ctx->init(args);

   rarch_main_clear_state();

   if (!(ret = (main_load_content(argc, argv, args, driver.frontend_ctx->environment_get,
         driver.frontend_ctx->process_args))))
      return_var(ret);

#if defined(HAVE_MENU)
#if defined(RARCH_CONSOLE) || defined(RARCH_MOBILE)
   if (ret)
#endif
   {
      // If we started content directly from command line,
      // push it to content history.
      if (!g_extern.libretro_dummy)
         menu_content_history_push_current();
   }
#endif

#if defined(HAVE_MAIN_LOOP)
#if defined(HAVE_MENU)
   while (!main_entry_iterate(signature_expand(), args));
#else
   while (rarch_main_iterate());
#endif

   main_exit(args);
#endif

   returnfunc();
}

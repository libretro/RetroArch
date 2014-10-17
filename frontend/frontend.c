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

#include "../driver.h"
#include "frontend.h"
#include "../general.h"

#if defined(RARCH_CONSOLE) || defined(RARCH_MOBILE)
#include "../config.def.h"
#endif

#if defined(ANDROID)

#define returntype void
#define returnfunc() exit(0)
#define return_var(var) return
#define declare_argc() int argc = 0;
#define declare_argv() char *argv[1]
#define args_initial_ptr() data
#else

#define returntype int
#define returnfunc() return 0
#define return_var(var) return var
#define declare_argc()
#define declare_argv()
#define args_initial_ptr() NULL

#endif

#if !defined(__APPLE__) && !defined(EMSCRIPTEN)
#define HAVE_MAIN_LOOP
#endif

#define MAX_ARGS 32

int main_entry_decide(signature(), args_type() args)
{
   int ret = rarch_main_iterate();

   if (ret == -1)
   {
      if (g_extern.core_shutdown_initiated 
            && g_settings.load_dummy_on_core_shutdown)
      {
         /* Load dummy core instead of exiting RetroArch completely. */
         rarch_main_command(RARCH_CMD_PREPARE_DUMMY);
         g_extern.core_shutdown_initiated = false;
         return 0;
      }
   }

   if (driver.frontend_ctx && driver.frontend_ctx->process_events)
      driver.frontend_ctx->process_events(args);

   return ret;
}

void main_exit(args_type() args)
{
   g_extern.system.shutdown = false;

   if (g_settings.config_save_on_exit && *g_extern.config_path)
   {
      /* Save last core-specific config to the default config location,
       * needed on consoles for core switching and reusing last good 
       * config for new cores.
       */
      config_save_file(g_extern.config_path);

      /* Flush out the core specific config. */
      if (*g_extern.core_specific_config_path &&
            g_settings.core_specific_config)
         config_save_file(g_extern.core_specific_config_path);
   }

   if (g_extern.main_is_init)
   {
#ifdef HAVE_MENU
      /* Do not want menu context to live any more. */
      driver.menu_data_own = false;
#endif
      rarch_main_deinit();
   }

   rarch_main_command(RARCH_CMD_PERFCNT_REPORT_FRONTEND_LOG);

#if defined(HAVE_LOGGER) && !defined(ANDROID)
   logger_shutdown();
#endif

   if (driver.frontend_ctx && driver.frontend_ctx->deinit)
      driver.frontend_ctx->deinit(args);

   if (driver.frontend_ctx && driver.frontend_ctx->exitspawn)
      driver.frontend_ctx->exitspawn(g_settings.libretro,
            sizeof(g_settings.libretro));

   rarch_main_state_free();

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
   if (*g_defaults.playlist_dir)
      path_mkdir(g_defaults.playlist_dir);
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
   if (*g_defaults.resampler_dir)
      path_mkdir(g_defaults.resampler_dir);
}

static void playlist_push(content_playlist_t *playlist,
      const char *path, const char *core_path,
      struct retro_system_info *info)
{
   char tmp[PATH_MAX];

   if (!playlist || !g_extern.libretro_dummy || !info)
      return;

   /* path can be relative here.
    * Ensure we're pushing absolute path. */

   strlcpy(tmp, path, sizeof(tmp));

   if (*tmp)
      path_resolve_realpath(tmp, sizeof(tmp));

   if (g_extern.system.no_content || *tmp)
      content_playlist_push(playlist,
            *tmp ? tmp : NULL,
            core_path,
            info->library_name);
}

bool main_load_content(int argc, char **argv, args_type() args,
      environment_get_t environ_get,
      process_args_t process_args)
{
   bool retval = true;
   int i, ret = 0, rarch_argc = 0;
   char *rarch_argv[MAX_ARGS] = {NULL};
   char *argv_copy [MAX_ARGS] = {NULL};
   char **rarch_argv_ptr = (char**)argv;
   int *rarch_argc_ptr = (int*)&argc;
   struct rarch_main_wrap *wrap_args = (struct rarch_main_wrap*)
      calloc(1, sizeof(*wrap_args));

   (void)rarch_argc_ptr;
   (void)rarch_argv_ptr;
   (void)ret;

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

   rarch_main_command(RARCH_CMD_RESUME);

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

   rarch_main_state_new();

   if (driver.frontend_ctx)
   {
      if (!(ret = (main_load_content(argc, argv, args,
         driver.frontend_ctx->environment_get,
         driver.frontend_ctx->process_args))))
      {
         return_var(ret);
      }
   }

   rarch_main_command(RARCH_CMD_HISTORY_INIT);

   if (g_settings.history_list_enable)
   {
#if defined(RARCH_CONSOLE) || defined(RARCH_MOBILE)
      if (ret)
#endif
         playlist_push(g_defaults.history,
               g_extern.fullpath,
               g_settings.libretro,
               &g_extern.system.info);
   }

#if defined(HAVE_MAIN_LOOP)
   while (main_entry_decide(signature_expand(), args) != -1);

   main_exit(args);
#endif

   returnfunc();
}

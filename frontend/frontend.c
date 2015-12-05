/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2015 - Daniel De Matteis
 * Copyright (C) 2012-2015 - Michael Lelli
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

#include <file/file_path.h>
#include <retro_assert.h>
#include <retro_stat.h>
#include <rthreads/async_job.h>

#include "frontend.h"

#include "../defaults.h"
#include "../driver.h"
#include "../system.h"
#include "../driver.h"
#include "../retroarch.h"
#include "../runloop.h"
#include "../verbosity.h"

#define MAX_ARGS 32

/**
 * main_exit:
 *
 * Cleanly exit RetroArch.
 *
 * Also saves configuration files to disk,
 * and (optionally) autosave state.
 **/
void main_exit(void *args)
{
   driver_t *driver                      = driver_get_ptr();
   settings_t *settings                  = config_get_ptr();
   global_t   *global                    = global_get_ptr();
   const frontend_ctx_driver_t *frontend = frontend_get_ptr();
   const ui_companion_driver_t *ui       = ui_companion_get_ptr();

   event_command(EVENT_CMD_MENU_SAVE_CURRENT_CONFIG);

   if (global->inited.main)
   {
#ifdef HAVE_MENU
      /* Do not want menu context to live any more. */
      driver->menu_data_own = false;
#endif
      rarch_main_deinit();
   }

   event_command(EVENT_CMD_PERFCNT_REPORT_FRONTEND_LOG);

#if defined(HAVE_LOGGER) && !defined(ANDROID)
   logger_shutdown();
#endif

   if (frontend)
   {
      if (frontend->deinit)
         frontend->deinit(args);

      if (frontend->exitspawn)
         frontend->exitspawn(settings->libretro,
               sizeof(settings->libretro));
   }

   rarch_main_free();

   if (ui)
   {
      if (ui->deinit)
         ui->deinit(driver->ui_companion_data);
   }

   if (frontend)
   {
      if (frontend->shutdown)
         frontend->shutdown(false);
   }

   driver_free();
}

static void check_defaults_dirs(void)
{
   if (*g_defaults.dir.core_assets)
      path_mkdir(g_defaults.dir.core_assets);
   if (*g_defaults.dir.remap)
      path_mkdir(g_defaults.dir.remap);
   if (*g_defaults.dir.screenshot)
      path_mkdir(g_defaults.dir.screenshot);
   if (*g_defaults.dir.core)
      path_mkdir(g_defaults.dir.core);
   if (*g_defaults.dir.autoconfig)
      path_mkdir(g_defaults.dir.autoconfig);
   if (*g_defaults.dir.audio_filter)
      path_mkdir(g_defaults.dir.audio_filter);
   if (*g_defaults.dir.video_filter)
      path_mkdir(g_defaults.dir.video_filter);
   if (*g_defaults.dir.assets)
      path_mkdir(g_defaults.dir.assets);
   if (*g_defaults.dir.playlist)
      path_mkdir(g_defaults.dir.playlist);
   if (*g_defaults.dir.core)
      path_mkdir(g_defaults.dir.core);
   if (*g_defaults.dir.core_info)
      path_mkdir(g_defaults.dir.core_info);
   if (*g_defaults.dir.overlay)
      path_mkdir(g_defaults.dir.overlay);
   if (*g_defaults.dir.port)
      path_mkdir(g_defaults.dir.port);
   if (*g_defaults.dir.shader)
      path_mkdir(g_defaults.dir.shader);
   if (*g_defaults.dir.savestate)
      path_mkdir(g_defaults.dir.savestate);
   if (*g_defaults.dir.sram)
      path_mkdir(g_defaults.dir.sram);
   if (*g_defaults.dir.system)
      path_mkdir(g_defaults.dir.system);
   if (*g_defaults.dir.resampler)
      path_mkdir(g_defaults.dir.resampler);
   if (*g_defaults.dir.menu_config)
      path_mkdir(g_defaults.dir.menu_config);
   if (*g_defaults.dir.content_history)
      path_mkdir(g_defaults.dir.content_history);
   if (*g_defaults.dir.cache)
      path_mkdir(g_defaults.dir.cache);
   if (*g_defaults.dir.database)
      path_mkdir(g_defaults.dir.database);
   if (*g_defaults.dir.cursor)
      path_mkdir(g_defaults.dir.cursor);
   if (*g_defaults.dir.cheats)
      path_mkdir(g_defaults.dir.cheats);
}

static void history_playlist_push(content_playlist_t *playlist,
      const char *path, const char *core_path,
      struct retro_system_info *info)
{
   char tmp[PATH_MAX_LENGTH]             = {0};
   global_t                    *global   = global_get_ptr();
   rarch_system_info_t *system           = rarch_system_info_get_ptr();

   if (!playlist || (global->inited.core.type == CORE_TYPE_DUMMY) || !info)
      return;

   /* Path can be relative here.
    * Ensure we're pushing absolute path. */

   strlcpy(tmp, path, sizeof(tmp));

   if (*tmp)
      path_resolve_realpath(tmp, sizeof(tmp));

   if (system->no_content || *tmp)
      content_playlist_push(playlist,
            *tmp ? tmp : NULL,
            NULL,
            core_path,
            info->library_name,
            NULL,
            NULL);
}

/**
 * main_load_content:
 * @argc             : Argument count.
 * @argv             : Argument variable list.
 * @args             : Arguments passed from callee.
 * @environ_get      : Function passed for environment_get function.
 *
 * Loads content file and starts up RetroArch.
 * If no content file can be loaded, will start up RetroArch
 * as-is.
 *
 * Returns: false (0) if rarch_main_init failed, otherwise true (1).
 **/
bool main_load_content(int argc, char **argv, void *args,
      environment_get_t environ_get)
{
   unsigned i;
   bool retval                       = true;
   int ret = 0, rarch_argc           = 0;
   char *rarch_argv[MAX_ARGS]        = {NULL};
   char *argv_copy [MAX_ARGS]        = {NULL};
   char **rarch_argv_ptr             = (char**)argv;
   int *rarch_argc_ptr               = (int*)&argc;
   global_t *global                  = global_get_ptr();
   struct rarch_main_wrap *wrap_args = (struct rarch_main_wrap*)
      calloc(1, sizeof(*wrap_args));

   if (!wrap_args)
      return false;

   (void)rarch_argc_ptr;
   (void)rarch_argv_ptr;
   (void)ret;

   retro_assert(wrap_args);

   if (environ_get)
      environ_get(rarch_argc_ptr, rarch_argv_ptr, args, wrap_args);

   if (wrap_args->touched)
   {
      rarch_main_init_wrap(wrap_args, &rarch_argc, rarch_argv);
      memcpy(argv_copy, rarch_argv, sizeof(rarch_argv));
      rarch_argv_ptr = (char**)rarch_argv;
      rarch_argc_ptr = (int*)&rarch_argc;
   }

   if (global->inited.main)
      rarch_main_deinit();

   if ((ret = rarch_main_init(*rarch_argc_ptr, rarch_argv_ptr)))
   {
      retval = false;
      goto error;
   }

   event_command(EVENT_CMD_RESUME);

   check_defaults_dirs();

   frontend_driver_process_args(rarch_argc_ptr, rarch_argv_ptr);

error:
   for (i = 0; i < ARRAY_SIZE(argv_copy); i++)
      free(argv_copy[i]);
   free(wrap_args);
   return retval;
}

/**
 * main_entry:
 *
 * Main function of RetroArch.
 *
 * If HAVE_MAIN is not defined, will contain main loop and will not
 * be exited from until we exit the program. Otherwise, will
 * just do initialization.
 *
 * Returns: varies per platform.
 **/
int rarch_main(int argc, char *argv[], void *data)
{
   void *args                      = (void*)data;
   int ret                         = 0;
   settings_t *settings            = NULL;
   driver_t *driver                = NULL;
#ifdef HAVE_THREADS
   global_t *global                = NULL;
#endif

   rarch_main_alloc();

   driver = driver_get_ptr();

   frontend_driver_init_first(args);
   rarch_main_new();

#ifdef HAVE_THREADS
   global = global_get_ptr();
   global->async_jobs = async_job_new();
#endif
   
   if (frontend_driver_is_inited())
   {
      if (!(ret = (main_load_content(argc, argv, args,
                     frontend_driver_environment_get_ptr()))))
         return ret;
   }

   event_command(EVENT_CMD_HISTORY_INIT);

   settings = config_get_ptr();

   if (settings->history_list_enable)
   {
      char *fullpath              = NULL;
      global_t *global            = global_get_ptr();
      rarch_system_info_t *system = rarch_system_info_get_ptr();

      runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);

      if (global->inited.content || system->no_content)
         history_playlist_push(
               g_defaults.history,
               fullpath,
               settings->libretro,
               system ? &system->info : NULL);
   }

   if (driver)
      driver->ui_companion = (ui_companion_driver_t*)ui_companion_init_first();

   if (driver->ui_companion && driver->ui_companion->toggle)
   {
      if (settings->ui.companion_start_on_boot)
         driver->ui_companion->toggle(driver->ui_companion_data);
   }

#ifndef HAVE_MAIN
   do
   {
      unsigned sleep_ms = 0;
      ret = rarch_main_iterate(&sleep_ms);

      if (ret == 1 && sleep_ms > 0)
         retro_sleep(sleep_ms);
      rarch_main_data_iterate();
   }while(ret != -1);

   main_exit(args);
#endif

   return 0;
}

#ifndef HAVE_MAIN
int main(int argc, char *argv[])
{
   return rarch_main(argc, argv, NULL);
}
#endif
